#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// G-Bus Classic Ratio Choices (SSL 4000 G Reference: 2:1, 4:1, 10:1)
// ==============================================================================
enum class GBusRatio : int {
  Ratio_2_1 = 0, // Gentle mix bus glue
  Ratio_4_1 = 1, // Classic SSL 4000 G bus cohesion & punch
  Ratio_10_1 = 2 // Firm peak control / bus limiting
};

// ==============================================================================
// G-Bus Classic Attack Steps (0.1, 0.3, 1, 3, 10, 30 ms)
// ==============================================================================
enum class GBusAttackStep : int {
  Att_0_1_ms = 0,
  Att_0_3_ms = 1,
  Att_1_0_ms = 2,
  Att_3_0_ms = 3,
  Att_10_ms = 4,
  Att_30_ms = 5
};

// ==============================================================================
// G-Bus Classic Release Steps (0.1, 0.3, 0.6, 1.2 s, Auto)
// ==============================================================================
enum class GBusReleaseStep : int {
  Rel_0_1_s = 0,
  Rel_0_3_s = 1,
  Rel_0_6_s = 2,
  Rel_1_2_s = 3,
  Rel_Auto = 4
};

// ==============================================================================
// G-Bus VCA Circuit Topology Model
// ==============================================================================
enum class GBusVcaModel : int {
  DBX202C_GoldCan =
      0, // Classic warm 4000 G console VCA with rich 2nd/3rd harmonics
  Quad4000G_Twin = 1, // Punchy quad-VCA console center section glue
  THAT2180_Clean = 2  // Ultra-linear modern trimmed VCA
};

// ==============================================================================
// G-Bus Factory Preset Structure
// ==============================================================================
struct GBusPreset {
  const char *name;
  const char *category;
  double thresholdDb;      // -15.0 to +15.0 dB
  double makeupDb;         // -5.0 to +15.0 dB
  GBusAttackStep attack;   // 0.1, 0.3, 1, 3, 10, 30 ms
  GBusReleaseStep release; // 0.1, 0.3, 0.6, 1.2 s, Auto
  GBusRatio ratio;         // 2:1, 4:1, 10:1
  double sidechainHpfHz;   // 0 (Off), 60, 90, 120, 185 Hz
  double stereoLinkPct;    // 0 to 100% (100% = true SSL stereo link)
  double mixPct;           // 0 to 100%
  double vcaCharacterPct;  // 0 to 100%
  bool analogColoration;   // On/Off console transformer & VCA glue
  GBusVcaModel vcaModel;
};

// ==============================================================================
// NOVA HD G-BUS — Stereo VCA Master Bus Compressor (SSL 4000 G Architecture)
//
// Signal & Control Flow:
//   STEREO INPUT L/R
//     -> STEREO INPUT STAGE (Input level & stereo channel linking)
//     -> VCA GAIN CONTROL (Stereo-linked VCA gain cell: Y = T + (X - T) / R)
//     -> MAKEUP GAIN (-5 to +15 dB + VCA/Transformer Analog Coloration)
//     -> PARALLEL MIX -> STEREO OUTPUT
//
//   SIDECHAIN CONTROL PATH:
//     Stereo Detector -> Sidechain HPF (optional) -> Threshold / Ratio
//     -> Attack (0.1..30 ms) / Dual-Stage Auto Release (0.1..1.2 s / Auto)
//     -> VCA Control Voltage (controls both L & R VCAs simultaneously)
// ==============================================================================
class KTBusCompressor final {
public:
  static constexpr std::array<double, 6> kClassicAttackMs = {0.1, 0.3,  1.0,
                                                             3.0, 10.0, 30.0};
  static constexpr std::array<double, 4> kClassicReleaseMs = {100.0, 300.0,
                                                              600.0, 1200.0};
  static constexpr std::array<double, 3> kClassicRatios = {2.0, 4.0, 10.0};

  KTBusCompressor() noexcept {
    reset();
    loadPresetByIndex(0);
  }

  void setSampleRate(double sr) noexcept {
    sampleRate = (sr > 1000.0) ? sr : 48000.0;
    updateFilters();
    updateTimeConstants();
  }

  void reset() noexcept {
    envFastL = 0.0;
    envSlowL = 0.0;
    envFastR = 0.0;
    envSlowR = 0.0;
    vcaControlDbL = 0.0;
    vcaControlDbR = 0.0;
    gainReductionDb = 0.0;
    xfmrStateL = 0.0;
    xfmrStateR = 0.0;
    scFilterL = {};
    scFilterR = {};
    meterGrDb.store(0.0f, std::memory_order_relaxed);
    meterInDb.store(-60.0f, std::memory_order_relaxed);
    meterOutDb.store(-60.0f, std::memory_order_relaxed);
    updateFilters();
    updateTimeConstants();
  }

  // --------------------------------------------------------------------------
  // 1. Threshold (-15 to +15 dB on classic G-Bus faceplate; extended -30..+15
  // dB)
  // --------------------------------------------------------------------------
  void setThresholdDb(double db) noexcept {
    thresholdDb = std::clamp(db, -30.0, 15.0);
  }
  double getThresholdDb() const noexcept { return thresholdDb; }

  // Classic faceplate dial range (-15.0 to +15.0 dB)
  void setThresholdDialDb(double db) noexcept {
    thresholdDb = std::clamp(db, -15.0, 15.0);
  }
  double getThresholdDialDb() const noexcept {
    return std::clamp(thresholdDb, -15.0, 15.0);
  }

  // --------------------------------------------------------------------------
  // 2. Makeup Gain (-5 to +15 dB)
  // --------------------------------------------------------------------------
  void setMakeupGainDb(double db) noexcept {
    makeupGainDb = std::clamp(db, -5.0, 15.0);
  }
  double getMakeupGainDb() const noexcept { return makeupGainDb; }

  // --------------------------------------------------------------------------
  // 3. Attack (0.1, 0.3, 1, 3, 10, 30 ms)
  // --------------------------------------------------------------------------
  void setAttackStep(GBusAttackStep step) noexcept {
    attackStep = step;
    const size_t idx = std::clamp(static_cast<size_t>(step), size_t(0),
                                  kClassicAttackMs.size() - 1);
    attackMs = kClassicAttackMs[idx];
    updateTimeConstants();
  }
  GBusAttackStep getAttackStep() const noexcept { return attackStep; }

  void setAttackMs(double ms) noexcept {
    attackMs = std::clamp(ms, 0.1, 30.0);
    // Snap attackStep to nearest classic SSL step for UI indicator
    size_t bestIdx = 0;
    double minDiff = 1e9;
    for (size_t i = 0; i < kClassicAttackMs.size(); ++i) {
      const double d = std::abs(attackMs - kClassicAttackMs[i]);
      if (d < minDiff) {
        minDiff = d;
        bestIdx = i;
      }
    }
    attackStep = static_cast<GBusAttackStep>(bestIdx);
    updateTimeConstants();
  }
  double getAttackMs() const noexcept { return attackMs; }

  // --------------------------------------------------------------------------
  // 4. Release (0.1, 0.3, 0.6, 1.2 s, Auto)
  // --------------------------------------------------------------------------
  void setReleaseStep(GBusReleaseStep step) noexcept {
    releaseStep = step;
    if (step == GBusReleaseStep::Rel_Auto) {
      autoRelease = true;
    } else {
      autoRelease = false;
      const size_t idx = std::clamp(static_cast<size_t>(step), size_t(0),
                                    kClassicReleaseMs.size() - 1);
      releaseMs = kClassicReleaseMs[idx];
    }
    updateTimeConstants();
  }
  GBusReleaseStep getReleaseStep() const noexcept { return releaseStep; }

  void setReleaseMs(double ms) noexcept {
    releaseMs = std::clamp(ms, 100.0, 1200.0);
    autoRelease = false;
    size_t bestIdx = 0;
    double minDiff = 1e9;
    for (size_t i = 0; i < kClassicReleaseMs.size(); ++i) {
      const double d = std::abs(releaseMs - kClassicReleaseMs[i]);
      if (d < minDiff) {
        minDiff = d;
        bestIdx = i;
      }
    }
    releaseStep = static_cast<GBusReleaseStep>(bestIdx);
    updateTimeConstants();
  }
  double getReleaseMs() const noexcept { return releaseMs; }

  void setAutoRelease(bool autoRel) noexcept {
    autoRelease = autoRel;
    if (autoRelease) {
      releaseStep = GBusReleaseStep::Rel_Auto;
    } else if (releaseStep == GBusReleaseStep::Rel_Auto) {
      releaseStep = GBusReleaseStep::Rel_0_3_s;
      releaseMs = 300.0;
    }
    updateTimeConstants();
  }
  bool getAutoRelease() const noexcept { return autoRelease; }

  // --------------------------------------------------------------------------
  // 5. Ratio (2:1, 4:1, 10:1)
  // --------------------------------------------------------------------------
  void setRatioStep(GBusRatio rStep) noexcept {
    ratioStep = rStep;
    const size_t idx = std::clamp(static_cast<size_t>(rStep), size_t(0),
                                  kClassicRatios.size() - 1);
    ratio = kClassicRatios[idx];
  }
  GBusRatio getRatioStep() const noexcept { return ratioStep; }

  void setRatio(double r) noexcept {
    ratio = std::clamp(r, 1.5, 20.0);
    if (ratio < 3.0) {
      ratioStep = GBusRatio::Ratio_2_1;
    } else if (ratio < 7.0) {
      ratioStep = GBusRatio::Ratio_4_1;
    } else {
      ratioStep = GBusRatio::Ratio_10_1;
    }
  }
  double getRatio() const noexcept { return ratio; }

  // --------------------------------------------------------------------------
  // 6. Advanced NOVA HD G-Bus Parameters
  //    (IN Button, Sidechain HPF, Stereo Link, Parallel Mix, VCA Character,
  //    Analog Coloration)
  // --------------------------------------------------------------------------
  void setInEnabled(bool active) noexcept { inEnabled = active; }
  bool getInEnabled() const noexcept { return inEnabled; }

  void setSidechainHpfHz(double hz) noexcept {
    sidechainHpfHz = (hz < 20.0) ? 0.0 : std::clamp(hz, 20.0, 300.0);
    updateFilters();
  }
  double getSidechainHpfHz() const noexcept { return sidechainHpfHz; }

  void setStereoLink(double pct) noexcept {
    stereoLink = std::clamp(pct, 0.0, 100.0) * 0.01;
  }
  double getStereoLink() const noexcept { return stereoLink * 100.0; }

  void setMix(double pct) noexcept { mix = std::clamp(pct, 0.0, 100.0) * 0.01; }
  double getMix() const noexcept { return mix * 100.0; }

  void setVcaCharacter(double pct) noexcept {
    vcaCharacter = std::clamp(pct, 0.0, 100.0) * 0.01;
  }
  double getVcaCharacter() const noexcept { return vcaCharacter * 100.0; }

  void setAnalogColoration(bool enabled) noexcept {
    analogColoration = enabled;
  }
  bool getAnalogColoration() const noexcept { return analogColoration; }

  void setVcaModel(GBusVcaModel model) noexcept { vcaModel = model; }
  GBusVcaModel getVcaModel() const noexcept { return vcaModel; }

  // --------------------------------------------------------------------------
  // 7. Metering & Telemetry (0..20 dB Compression Meter)
  // --------------------------------------------------------------------------
  double getGainReductionDb() const noexcept { return gainReductionDb; }
  float getMeterGrDb() const noexcept {
    return meterGrDb.load(std::memory_order_relaxed);
  }
  float getMeterInDb() const noexcept {
    return meterInDb.load(std::memory_order_relaxed);
  }
  float getMeterOutDb() const noexcept {
    return meterOutDb.load(std::memory_order_relaxed);
  }

  // --------------------------------------------------------------------------
  // 8. Factory Presets (Master Bus, Drum Bus, Bass Bus, Instrument Bus, Full
  // Mix)
  // --------------------------------------------------------------------------
  static const std::vector<GBusPreset> &getFactoryPresets() {
    static const std::vector<GBusPreset> kPresets = {
        {"01. Master Bus Glue (2:1 Starting Point)", "Master Bus", -6.0, 2.0,
         GBusAttackStep::Att_30_ms, GBusReleaseStep::Rel_Auto,
         GBusRatio::Ratio_2_1, 60.0, 100.0, 100.0, 55.0, true,
         GBusVcaModel::DBX202C_GoldCan},
        {"02. Master Bus Fast Cohesion (2:1 / 10ms)", "Master Bus", -8.0, 2.5,
         GBusAttackStep::Att_10_ms, GBusReleaseStep::Rel_Auto,
         GBusRatio::Ratio_2_1, 60.0, 100.0, 100.0, 60.0, true,
         GBusVcaModel::Quad4000G_Twin},
        {"03. SSL 4000G Classic Radio Bus (4:1)", "Master Bus", -10.0, 3.5,
         GBusAttackStep::Att_10_ms, GBusReleaseStep::Rel_0_1_s,
         GBusRatio::Ratio_4_1, 60.0, 100.0, 100.0, 75.0, true,
         GBusVcaModel::DBX202C_GoldCan},
        {"04. Drum Bus Punch & Snap (4:1 / 30ms)", "Drum Bus", -11.0, 4.0,
         GBusAttackStep::Att_30_ms, GBusReleaseStep::Rel_0_1_s,
         GBusRatio::Ratio_4_1, 90.0, 100.0, 100.0, 80.0, true,
         GBusVcaModel::DBX202C_GoldCan},
        {"05. Drum Bus Parallel Crush (10:1)", "Drum Bus", -15.0, 6.5,
         GBusAttackStep::Att_0_3_ms, GBusReleaseStep::Rel_0_1_s,
         GBusRatio::Ratio_10_1, 120.0, 100.0, 50.0, 95.0, true,
         GBusVcaModel::DBX202C_GoldCan},
        {"06. Bass Bus Tight Foundation (4:1)", "Bass Bus", -12.0, 3.5,
         GBusAttackStep::Att_3_0_ms, GBusReleaseStep::Rel_0_3_s,
         GBusRatio::Ratio_4_1, 90.0, 100.0, 100.0, 70.0, true,
         GBusVcaModel::Quad4000G_Twin},
        {"07. Instrument Bus Stereo Glue (2:1)", "Instrument Bus", -8.0, 2.0,
         GBusAttackStep::Att_10_ms, GBusReleaseStep::Rel_0_3_s,
         GBusRatio::Ratio_2_1, 60.0, 100.0, 100.0, 50.0, true,
         GBusVcaModel::Quad4000G_Twin},
        {"08. Acoustic & Keys Sub-Group (2:1)", "Instrument Bus", -7.5, 2.0,
         GBusAttackStep::Att_30_ms, GBusReleaseStep::Rel_0_6_s,
         GBusRatio::Ratio_2_1, 0.0, 100.0, 100.0, 35.0, true,
         GBusVcaModel::THAT2180_Clean},
        {"09. Vocal Group Bus Control (4:1 Auto)", "Vocal Bus", -10.5, 3.0,
         GBusAttackStep::Att_1_0_ms, GBusReleaseStep::Rel_Auto,
         GBusRatio::Ratio_4_1, 90.0, 100.0, 100.0, 60.0, true,
         GBusVcaModel::DBX202C_GoldCan},
        {"10. Full Mix Transparent Mastering", "Full Mix", -5.0, 1.5,
         GBusAttackStep::Att_30_ms, GBusReleaseStep::Rel_Auto,
         GBusRatio::Ratio_2_1, 60.0, 100.0, 100.0, 30.0, false,
         GBusVcaModel::THAT2180_Clean}};
    return kPresets;
  }

  void loadPresetByIndex(int idx) noexcept {
    const auto &all = getFactoryPresets();
    if (all.empty())
      return;
    const int clamped = std::clamp(idx, 0, static_cast<int>(all.size()) - 1);
    currentPresetIndex = clamped;
    const auto &p = all[static_cast<size_t>(clamped)];
    setThresholdDb(p.thresholdDb);
    setMakeupGainDb(p.makeupDb);
    setAttackStep(p.attack);
    setReleaseStep(p.release);
    setRatioStep(p.ratio);
    setSidechainHpfHz(p.sidechainHpfHz);
    setStereoLink(p.stereoLinkPct);
    setMix(p.mixPct);
    setVcaCharacter(p.vcaCharacterPct);
    setAnalogColoration(p.analogColoration);
    setVcaModel(p.vcaModel);
    setInEnabled(true);
  }

  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  // --------------------------------------------------------------------------
  // 9. Real-Time Stereo VCA Bus Processing
  // --------------------------------------------------------------------------
  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double dryL = sanitize(inOutL);
    const double dryR = sanitize(inOutR);

    const double inPeak = std::max(std::abs(dryL), std::abs(dryR));
    meterInDb.store(static_cast<float>(gainToDb(inPeak, -60.0)),
                    std::memory_order_relaxed);

    // IN button bypasses the compressor cleanly
    if (!inEnabled) {
      gainReductionDb = 0.0;
      meterGrDb.store(0.0f, std::memory_order_relaxed);
      meterOutDb.store(static_cast<float>(gainToDb(inPeak, -60.0)),
                       std::memory_order_relaxed);
      inOutL = dryL;
      inOutR = dryR;
      return;
    }

    // 1. SIDECHAIN CONTROL PATH: Optional Sidechain High-Pass Filter
    //    Prevents low-frequency kick/sub energy from overly triggering VCA gain
    //    reduction.
    double scL = dryL;
    double scR = dryR;
    if (sidechainHpfHz > 10.0) {
      scL = scFilterL.process(scL);
      scR = scFilterR.process(scR);
    }

    // 2. STEREO DETECTOR & CHANNEL LINKING
    //    In SSL G-Bus, both channels feed a summed/linked sidechain detector so
    //    a peak in Left produces identical gain reduction in Left and Right,
    //    preserving stereo image.
    const double detIndividualL = std::abs(scL);
    const double detIndividualR = std::abs(scR);
    // True SSL G-Bus stereo sidechain summing: combines L/R energy and peak
    // crest
    const double detLinked =
        std::max(std::sqrt(0.5 * (scL * scL + scR * scR)),
                 0.85 * std::max(detIndividualL, detIndividualR));

    const double detL =
        detIndividualL * (1.0 - stereoLink) + detLinked * stereoLink;
    const double detR =
        detIndividualR * (1.0 - stereoLink) + detLinked * stereoLink;

    // 3. THRESHOLD & RATIO SIDECHAIN AMP (Before Attack/Release Timing
    // Capacitor!)
    //    In the authentic SSL 4000 G circuit, the sidechain VCA/log detector
    //    computes the raw gain-reduction voltage above Threshold first, and
    //    that voltage charges the Attack / Release RC timing network. This
    //    preserves initial transient punch at 10 ms / 30 ms Attack while
    //    smoothly gluing the body of the mix together.
    const double rawGrDbL = computeVcaGainReductionDb(gainToDb(detL, -100.0));
    const double rawGrDbR =
        (stereoLink >= 0.999)
            ? rawGrDbL
            : computeVcaGainReductionDb(gainToDb(detR, -100.0));

    // 4. ATTACK / PROGRAM-DEPENDENT RELEASE RC TIMING NETWORK (Operating on GR
    // Control Voltage)
    vcaControlDbL =
        sanitize(processControlVoltageEnvelope(rawGrDbL, envFastL, envSlowL));
    vcaControlDbR = (stereoLink >= 0.999)
                        ? vcaControlDbL
                        : sanitize(processControlVoltageEnvelope(
                              rawGrDbR, envFastR, envSlowR));

    gainReductionDb = std::max(vcaControlDbL, vcaControlDbR);

    const double vcaGainL = dbToGain(-vcaControlDbL);
    const double vcaGainR = dbToGain(-vcaControlDbR);

    // 5. VCA GAIN CELL & MAKEUP GAIN STAGE
    const double makeupLin = dbToGain(makeupGainDb);
    double wetL =
        applyVcaCellAndColoration(dryL, vcaGainL, vcaControlDbL, xfmrStateL) *
        makeupLin;
    double wetR =
        applyVcaCellAndColoration(dryR, vcaGainR, vcaControlDbR, xfmrStateR) *
        makeupLin;

    // 6. PARALLEL COMPRESSION MIX
    inOutL = sanitize(dryL * (1.0 - mix) + wetL * mix);
    inOutR = sanitize(dryR * (1.0 - mix) + wetR * mix);

    // 7. UPDATE METERS
    const float prevGr = meterGrDb.load(std::memory_order_relaxed);
    const float curGr = static_cast<float>(gainReductionDb);
    const float smoothGr = (curGr > prevGr) ? (prevGr * 0.55f + curGr * 0.45f)
                                            : (prevGr * 0.86f + curGr * 0.14f);
    meterGrDb.store(smoothGr, std::memory_order_relaxed);

    const double outPeak = std::max(std::abs(inOutL), std::abs(inOutR));
    meterOutDb.store(static_cast<float>(gainToDb(outPeak, -60.0)),
                     std::memory_order_relaxed);
  }

private:
  double processControlVoltageEnvelope(double rawGrDb, double &fastVcDb,
                                       double &slowVcDb) const noexcept {
    if (autoRelease) {
      // SSL G-Bus Dual-RC Program-Dependent Auto Release Network:
      // Fast capacitor (75 ms) tracks transient peaks and recovers rapidly so
      // drum hits never cause mix dropouts; Slow capacitor (920 ms) charges on
      // sustained program density for effortless, transparent mix bus cohesion
      // ("glue").
      if (rawGrDb > fastVcDb) {
        fastVcDb = attackCoeff * fastVcDb + (1.0 - attackCoeff) * rawGrDb;
      } else {
        fastVcDb =
            autoFastRelCoeff * fastVcDb + (1.0 - autoFastRelCoeff) * rawGrDb;
      }

      if (rawGrDb > slowVcDb) {
        slowVcDb = autoSlowChargeCoeff * slowVcDb +
                   (1.0 - autoSlowChargeCoeff) * rawGrDb;
      } else {
        slowVcDb =
            autoSlowRelCoeff * slowVcDb + (1.0 - autoSlowRelCoeff) * rawGrDb;
      }

      fastVcDb = sanitize(fastVcDb);
      slowVcDb = sanitize(slowVcDb);
      return std::max(fastVcDb, slowVcDb * 0.85);
    } else {
      // Even in manual release modes, slight dual-slope program softening
      // prevents abrupt VCA chatter
      if (rawGrDb > fastVcDb) {
        fastVcDb = attackCoeff * fastVcDb + (1.0 - attackCoeff) * rawGrDb;
      } else {
        fastVcDb = releaseCoeff * fastVcDb + (1.0 - releaseCoeff) * rawGrDb;
      }
      if (rawGrDb > slowVcDb) {
        slowVcDb = autoSlowChargeCoeff * slowVcDb +
                   (1.0 - autoSlowChargeCoeff) * rawGrDb;
      } else {
        slowVcDb = releaseCoeff * slowVcDb + (1.0 - releaseCoeff) * rawGrDb;
      }
      fastVcDb = sanitize(fastVcDb);
      slowVcDb = sanitize(slowVcDb);
      return 0.82 * fastVcDb + 0.18 * slowVcDb;
    }
  }

  double computeVcaGainReductionDb(double inputLevelDb) const noexcept {
    // VCA Compression Law:
    // Above threshold T with ratio R: Y = T + (X - T) / R  =>  GR = (1 - 1/R) *
    // (X - T) Ratio-dependent soft knee: wider knee at 2:1 (5.0 dB) for
    // effortless mix glue, medium knee at 4:1 (3.5 dB), tight knee at 10:1 (2.0
    // dB).
    const double kneeWidthDb = (ratio <= 2.2)   ? 5.0
                               : (ratio <= 5.0) ? 3.5
                                                : 2.0;
    const double slope = 1.0 - (1.0 / std::max(1.0, ratio));
    const double overDb = inputLevelDb - thresholdDb;

    if (overDb <= -0.5 * kneeWidthDb) {
      return 0.0;
    }
    if (overDb < 0.5 * kneeWidthDb) {
      const double x = overDb + 0.5 * kneeWidthDb;
      return std::clamp(slope * (x * x) / (2.0 * kneeWidthDb), 0.0, 30.0);
    }
    return std::clamp(slope * overDb, 0.0, 30.0);
  }

  double applyVcaCellAndColoration(double x, double vcaGain, double grDb,
                                   double &lpfState) const noexcept {
    double y = x * vcaGain;
    if (!analogColoration && vcaCharacter <= 0.001) {
      return y;
    }

    // Model DBX 202C Gold Can / 4000G Twin VCA current-modulation harmonics
    const double modelScale = (vcaModel == GBusVcaModel::DBX202C_GoldCan) ? 1.25
                              : (vcaModel == GBusVcaModel::Quad4000G_Twin)
                                  ? 0.90
                                  : 0.35;
    const double drive =
        vcaCharacter * modelScale * (1.0 + 0.06 * std::min(grDb, 16.0));

    // Subtle even-order VCA symmetry offset + odd-order console bus saturation
    const double h2 = 0.012 * drive * y * std::abs(y);
    const double h3 = -0.008 * drive * (y * y * y);
    y += h2 + h3;

    if (analogColoration) {
      // SSL 4000 G output bus low-frequency transformer weight
      lpfState = sanitize(0.992 * lpfState + 0.008 * y);
      const double lfSat = std::tanh(lpfState * 1.4) - lpfState * 1.4;
      y += lfSat * 0.035 * modelScale;
    }
    return y;
  }

  void updateFilters() noexcept {
    const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    if (sidechainHpfHz > 10.0) {
      scFilterL.setCoefficients(makeButterworthHPF(sr, sidechainHpfHz, 0.7071));
      scFilterR.setCoefficients(makeButterworthHPF(sr, sidechainHpfHz, 0.7071));
    } else {
      scFilterL.setCoefficients({});
      scFilterR.setCoefficients({});
    }
  }

  void updateTimeConstants() noexcept {
    const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    const double attSec = std::max(1e-4, attackMs * 0.001);
    attackCoeff = std::exp(-1.0 / (sr * attSec));

    const double relSec = std::max(0.05, releaseMs * 0.001);
    releaseCoeff = std::exp(-1.0 / (sr * relSec));

    // Dual-RC Auto Release time constants
    autoFastRelCoeff =
        std::exp(-1.0 / (sr * 0.085)); // 85 ms fast peak recovery
    autoSlowChargeCoeff = std::exp(-1.0 / (sr * (attSec * 3.5 + 0.025)));
    autoSlowRelCoeff =
        std::exp(-1.0 / (sr * 0.920)); // 920 ms program sustain recovery
  }

  double sampleRate = 48000.0;
  double thresholdDb = -6.0;
  double makeupGainDb = 2.0;
  double ratio = 2.0;
  double attackMs = 30.0;
  double releaseMs = 300.0;
  bool autoRelease = true;
  bool inEnabled = true;

  GBusAttackStep attackStep = GBusAttackStep::Att_30_ms;
  GBusReleaseStep releaseStep = GBusReleaseStep::Rel_Auto;
  GBusRatio ratioStep = GBusRatio::Ratio_2_1;
  GBusVcaModel vcaModel = GBusVcaModel::DBX202C_GoldCan;

  double sidechainHpfHz = 60.0;
  double stereoLink = 1.0;    // 1.0 = 100% stereo-linked
  double mix = 1.0;           // 1.0 = 100% wet
  double vcaCharacter = 0.55; // 55% default VCA harmonic glue
  bool analogColoration = true;
  int currentPresetIndex = 0;

  double attackCoeff = 0.0;
  double releaseCoeff = 0.0;
  double autoFastRelCoeff = 0.0;
  double autoSlowChargeCoeff = 0.0;
  double autoSlowRelCoeff = 0.0;

  double envFastL = 0.0;
  double envSlowL = 0.0;
  double envFastR = 0.0;
  double envSlowR = 0.0;
  double vcaControlDbL = 0.0;
  double vcaControlDbR = 0.0;
  double gainReductionDb = 0.0;
  double xfmrStateL = 0.0;
  double xfmrStateR = 0.0;

  Biquad scFilterL;
  Biquad scFilterR;

  std::atomic<float> meterGrDb{0.0f};
  std::atomic<float> meterInDb{-60.0f};
  std::atomic<float> meterOutDb{-60.0f};
};

} // namespace nova::graviton
