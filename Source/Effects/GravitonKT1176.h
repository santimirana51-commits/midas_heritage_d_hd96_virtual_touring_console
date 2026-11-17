#pragma once

// ==============================================================================
// GravitonKT1176.h
// NOVA HD — Graviton Mk2 DSP Audio Engine
// Universal Audio / UREI 1176LN Limiting Amplifier — Authentic Feedback FET
// Model
//
// Signal & Control Flow (True 1176LN Feedback Topology):
//   AUDIO INPUT
//     ↓
//   INPUT DRIVE & INPUT TRANSFORMER (UTC O-12 coloration, -24 to +24 dB)
//     ↓
//   FET GAIN CELL: G(n) = f(Vc(n-1), Vbias, X(n)) -> Signal Preamp Output S(n)
//     ├──► [OUTPUT SENSING POINT] ──► FEEDBACK DETECTOR:
//     │      Sidechain HPF -> Bipolar Rectifier -> Ratio Bias Threshold Network
//     │      -> Attack/Release Dual-RC Envelope -> FET Gate Control Voltage
//     Vc(n) │      (Feeds back to control FET Gain Cell on next sample) ↓
//   CLASS-A OUTPUT AMPLIFIER (1108/1176 Line Amp + 5002 Output Transformer)
//     ↓
//   OPTIONAL ANALOG NOISE FLOOR & PARALLEL MIX -> AUDIO OUTPUT
// ==============================================================================

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <vector>

namespace nova::graviton {

enum class KT1176Ratio {
  Ratio_4_1 = 0,
  Ratio_8_1 = 1,
  Ratio_12_1 = 2,
  Ratio_20_1 = 3,
  AllButtons = 4 // Signature "All Buttons In" (British Nuke) mode
};

enum class KT1176MeterMode {
  GR = 0,    // Gain Reduction in dB (0 dB resting at 0 VU, deflects left)
  Plus8 = 1, // Output level referenced to +8 dBu (0 VU = -10 dBFS)
  Plus4 = 2, // Output level referenced to +4 dBu (0 VU = -14 dBFS)
  Off = 3    // Meter / unit off
};

enum class KT1176Revision {
  RevA_Bluestripe = 0, // Higher FET asymmetry, faster transient bite
  RevD_LN_Classic = 1, // Bill Putnam Low-Noise Class-A circuit (Reference)
  RevE_Blackface = 2   // High-headroom modern Blackface response
};

class KT1176Limiter final {
public:
  struct Preset {
    const char *name;
    double inputDb;     // -24 to +24 dB
    double outputDb;    // -24 to +24 dB
    double attackDial;  // 1.0 (800 us slowest) to 7.0 (20 us fastest)
    double releaseDial; // 1.0 (1100 ms slowest) to 7.0 (50 ms fastest)
    KT1176Ratio ratio;
    KT1176Revision revision;
    double fetCharacterPct; // 0 to 100%
    double xfmrColorPct;    // 0 to 100%
    double sidechainHpfHz;  // 0, 60, 90, 150 Hz
    double mixPct;          // 0 to 100%
    bool attackOff;
    bool analogNoise;
  };

  KT1176Limiter() noexcept {
    setSampleRate(48000.0);
    reset();
  }

  void setSampleRate(double sr) noexcept {
    sampleRate = (sr > 1000.0) ? sr : 48000.0;
    updateTimeConstants();
    updateFilters();
  }

  void reset() noexcept {
    vcFastL = 0.0;
    vcFastR = 0.0;
    vcSlowL = 0.0;
    vcSlowR = 0.0;
    feedbackGrDbL = 0.0;
    feedbackGrDbR = 0.0;
    prevGainL = 1.0;
    prevGainR = 1.0;
    scHpfZ1_L = scHpfZ1_R = 0.0;
    xfmrInLowL = xfmrInLowR = 0.0;
    xfmrOutLowL = xfmrOutLowR = 0.0;
    dcX1_L = dcX1_R = 0.0;
    dcY1_L = dcY1_R = 0.0;
    meterGrDb.store(0.0f, std::memory_order_relaxed);
    meterOutDb.store(-60.0f, std::memory_order_relaxed);
    meterInDb.store(-60.0f, std::memory_order_relaxed);
    updateTimeConstants();
    updateFilters();
  }

  // ── 1. Input & Output (-24 to +24 dBNOVA HD Standard, compatible up to ±30
  // dB) ─
  void setInputDb(double db) noexcept {
    inputGainDb = std::clamp(db, -30.0, 30.0);
  }
  double getInputDb() const noexcept { return inputGainDb; }

  // Hardware dial 0..48 where 24 (12 o'clock) = 0.0 dB, 0 = -24 dB, 48 = +24 dB
  void setInputDial(double dial0to48) noexcept {
    const double clamped = std::clamp(dial0to48, 0.0, 48.0);
    setInputDb(clamped - 24.0);
  }
  double getInputDial() const noexcept {
    return std::clamp(24.0 + inputGainDb, 0.0, 48.0);
  }

  void setOutputDb(double db) noexcept {
    outputGainDb = std::clamp(db, -30.0, 30.0);
  }
  double getOutputDb() const noexcept { return outputGainDb; }

  void setOutputDial(double dial0to48) noexcept {
    const double clamped = std::clamp(dial0to48, 0.0, 48.0);
    setOutputDb(clamped - 24.0);
  }
  double getOutputDial() const noexcept {
    return std::clamp(24.0 + outputGainDb, 0.0, 48.0);
  }

  // ── 2. Attack (20–800 µs; Knob 1 = 800 µs Lambat, Knob 7 = 20 µs Cepat) ──
  void setAttackUs(double us) noexcept {
    attackUs = std::clamp(us, 20.0, 800.0);
    const double ratioLog = std::log(attackUs / 800.0) / std::log(20.0 / 800.0);
    attackDial = std::clamp(1.0 + 6.0 * ratioLog, 1.0, 7.0);
    updateTimeConstants();
  }
  double getAttackUs() const noexcept { return attackUs; }

  void setAttackDial(double dial1to7) noexcept {
    attackDial = std::clamp(dial1to7, 1.0, 7.0);
    const double norm = (attackDial - 1.0) / 6.0;
    attackUs = std::clamp(800.0 * std::pow(20.0 / 800.0, norm), 20.0, 800.0);
    updateTimeConstants();
  }
  double getAttackDial() const noexcept { return attackDial; }

  void setAttackOff(bool off) noexcept { attackOff = off; }
  bool getAttackOff() const noexcept { return attackOff; }

  // ── 3. Release (50–1100 ms; Knob 1 = 1100 ms Lambat, Knob 7 = 50 ms Cepat) ─
  void setReleaseMs(double ms) noexcept {
    releaseMs = std::clamp(ms, 50.0, 1100.0);
    const double ratioLog =
        std::log(releaseMs / 1100.0) / std::log(50.0 / 1100.0);
    releaseDial = std::clamp(1.0 + 6.0 * ratioLog, 1.0, 7.0);
    updateTimeConstants();
  }
  double getReleaseMs() const noexcept { return releaseMs; }

  void setReleaseDial(double dial1to7) noexcept {
    releaseDial = std::clamp(dial1to7, 1.0, 7.0);
    const double norm = (releaseDial - 1.0) / 6.0;
    releaseMs =
        std::clamp(1100.0 * std::pow(50.0 / 1100.0, norm), 50.0, 1100.0);
    updateTimeConstants();
  }
  double getReleaseDial() const noexcept { return releaseDial; }

  // ── 4. Ratio & All Buttons In ──────────────────────────────────────────
  void setRatio(KT1176Ratio r) noexcept {
    ratio = r;
    if (r != KT1176Ratio::AllButtons) {
      lastStandardRatio = r;
    }
  }
  KT1176Ratio getRatio() const noexcept { return ratio; }

  void setAllButtonsIn(bool allIn) noexcept {
    ratio = allIn ? KT1176Ratio::AllButtons : lastStandardRatio;
  }
  bool getAllButtonsIn() const noexcept {
    return ratio == KT1176Ratio::AllButtons;
  }

  // ── 5. Meter Selector (GR, +8, +4, OFF) ────────────────────────────────
  void setMeterMode(KT1176MeterMode m) noexcept { meterMode = m; }
  KT1176MeterMode getMeterMode() const noexcept { return meterMode; }

  // ── 6. FET Character, Transformer Color, Analog Noise, SC HPF, Mix ─────
  void setRevision(KT1176Revision rev) noexcept { revision = rev; }
  KT1176Revision getRevision() const noexcept { return revision; }

  void setFetCharacter(double pct) noexcept {
    fetCharacter = std::clamp(pct, 0.0, 100.0) * 0.01;
  }
  double getFetCharacter() const noexcept { return fetCharacter * 100.0; }

  void setTransformerColor(bool enabled) noexcept {
    transformerColor = enabled;
    if (enabled && transformerColorAmount < 0.05)
      transformerColorAmount = 0.65;
  }
  bool getTransformerColor() const noexcept { return transformerColor; }

  void setTransformerColorAmount(double pct) noexcept {
    transformerColorAmount = std::clamp(pct, 0.0, 100.0) * 0.01;
    transformerColor = (transformerColorAmount > 0.01);
  }
  double getTransformerColorAmount() const noexcept {
    return transformerColor ? (transformerColorAmount * 100.0) : 0.0;
  }

  void setAnalogNoise(bool enabled) noexcept { analogNoise = enabled; }
  bool getAnalogNoise() const noexcept { return analogNoise; }

  void setSidechainHpfHz(double hz) noexcept {
    sidechainHpfHz = std::clamp(hz, 0.0, 400.0);
    updateFilters();
  }
  double getSidechainHpfHz() const noexcept { return sidechainHpfHz; }

  void setMix(double pct) noexcept { mix = std::clamp(pct, 0.0, 100.0) * 0.01; }
  double getMix() const noexcept { return mix * 100.0; }

  void setBypass(bool bp) noexcept { bypassed = bp; }
  bool getBypass() const noexcept { return bypassed; }

  // ── 7. Telemetry Getters ───────────────────────────────────────────────
  double getGainReductionDb() const noexcept {
    return std::max(feedbackGrDbL, feedbackGrDbR);
  }
  float getMeterGrDb() const noexcept {
    return meterGrDb.load(std::memory_order_relaxed);
  }
  float getMeterOutDb() const noexcept {
    return meterOutDb.load(std::memory_order_relaxed);
  }
  float getMeterInDb() const noexcept {
    return meterInDb.load(std::memory_order_relaxed);
  }

  float getVuNeedleDb() const noexcept {
    if (bypassed || meterMode == KT1176MeterMode::Off) {
      return -20.0f;
    }
    if (meterMode == KT1176MeterMode::GR) {
      const float gr = meterGrDb.load(std::memory_order_relaxed);
      return std::clamp(-gr, -20.0f, 3.0f);
    }
    const float outDb = meterOutDb.load(std::memory_order_relaxed);
    if (meterMode == KT1176MeterMode::Plus8) {
      return std::clamp(outDb + 10.0f, -20.0f, 3.0f);
    }
    return std::clamp(outDb + 14.0f, -20.0f, 3.0f);
  }

  // ── 8. Factory Presets (12 Curated Studio 1176LN Presets) ──────────────
  static const std::vector<Preset> &getFactoryPresets() noexcept {
    static const std::vector<Preset> presets = {
        {"01. Lead Vocal Classic (4:1)", 6.0, 3.5, 3.5, 5.5,
         KT1176Ratio::Ratio_4_1, KT1176Revision::RevD_LN_Classic, 55.0, 65.0,
         80.0, 100.0, false, false},
        {"02. Rock Vocal In-Your-Face (8:1)", 10.5, 5.0, 4.8, 6.2,
         KT1176Ratio::Ratio_8_1, KT1176Revision::RevA_Bluestripe, 75.0, 80.0,
         90.0, 100.0, false, false},
        {"03. Snare Crack & Body (4:1)", 7.5, 4.0, 1.6, 6.5,
         KT1176Ratio::Ratio_4_1, KT1176Revision::RevD_LN_Classic, 60.0, 70.0,
         60.0, 100.0, false, false},
        {"04. Kick Drum Tight Punch (8:1)", 8.0, 4.5, 1.4, 5.8,
         KT1176Ratio::Ratio_8_1, KT1176Revision::RevE_Blackface, 45.0, 75.0,
         60.0, 100.0, false, false},
        {"05. Room Mic British Nuke (ALL)", 14.0, 6.0, 4.2, 6.8,
         KT1176Ratio::AllButtons, KT1176Revision::RevA_Bluestripe, 90.0, 85.0,
         90.0, 75.0, false, true},
        {"06. Bass Guitar Anchor (12:1)", 9.0, 5.0, 2.8, 4.5,
         KT1176Ratio::Ratio_12_1, KT1176Revision::RevD_LN_Classic, 65.0, 80.0,
         0.0, 100.0, false, false},
        {"07. Acoustic Guitar Strum (4:1)", 4.5, 2.5, 2.5, 5.2,
         KT1176Ratio::Ratio_4_1, KT1176Revision::RevE_Blackface, 35.0, 40.0,
         80.0, 100.0, false, false},
        {"08. Electric Guitar Sustain (8:1)", 8.5, 4.0, 4.2, 4.0,
         KT1176Ratio::Ratio_8_1, KT1176Revision::RevD_LN_Classic, 60.0, 70.0,
         60.0, 100.0, false, false},
        {"09. Drum Bus NY Parallel (12:1)", 12.0, 5.5, 2.2, 6.4,
         KT1176Ratio::Ratio_12_1, KT1176Revision::RevD_LN_Classic, 70.0, 75.0,
         90.0, 45.0, false, false},
        {"10. Fast Peak Limiter (20:1)", 5.0, 2.0, 6.8, 6.5,
         KT1176Ratio::Ratio_20_1, KT1176Revision::RevE_Blackface, 40.0, 35.0,
         0.0, 100.0, false, false},
        {"11. Pure Class-A & Iron (ATT OFF)", 4.0, 0.0, 4.0, 4.0,
         KT1176Ratio::Ratio_4_1, KT1176Revision::RevD_LN_Classic, 65.0, 85.0,
         0.0, 100.0, true, false},
        {"12. Default 1176LN Reference", 0.0, 0.0, 4.0, 4.0,
         KT1176Ratio::Ratio_4_1, KT1176Revision::RevD_LN_Classic, 50.0, 65.0,
         0.0, 100.0, false, false}};
    return presets;
  }

  void loadPreset(const Preset &p) noexcept {
    setInputDb(p.inputDb);
    setOutputDb(p.outputDb);
    setAttackDial(p.attackDial);
    setReleaseDial(p.releaseDial);
    setRatio(p.ratio);
    setRevision(p.revision);
    setFetCharacter(p.fetCharacterPct);
    setTransformerColorAmount(p.xfmrColorPct);
    setSidechainHpfHz(p.sidechainHpfHz);
    setMix(p.mixPct);
    setAttackOff(p.attackOff);
    setAnalogNoise(p.analogNoise);

    const auto &all = getFactoryPresets();
    for (size_t i = 0; i < all.size(); ++i) {
      if (std::strcmp(all[i].name, p.name) == 0) {
        currentPresetIndex = static_cast<int>(i);
        break;
      }
    }
  }

  void loadPresetByIndex(int idx) noexcept {
    const auto &all = getFactoryPresets();
    if (all.empty())
      return;
    const int clamped = std::clamp(idx, 0, static_cast<int>(all.size()) - 1);
    currentPresetIndex = clamped;
    loadPreset(all[static_cast<size_t>(clamped)]);
  }

  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  // ── 9. Real-Time True-Feedback FET Audio Processing ────────────────────
  void processStereo(double &inOutL, double &inOutR) noexcept {
    if (bypassed || meterMode == KT1176MeterMode::Off) {
      feedbackGrDbL = 0.0;
      feedbackGrDbR = 0.0;
      meterGrDb.store(0.0f, std::memory_order_relaxed);
      return;
    }

    const double dryL = sanitize(inOutL);
    const double dryR = sanitize(inOutR);

    // ---------------------------------------------------------------------
    // STAGE 1: INPUT ATTENUATOR & UTC O-12 INPUT TRANSFORMER COLORATION
    // ---------------------------------------------------------------------
    const double inGain = dbToGain(inputGainDb);
    const double outGain = dbToGain(outputGainDb);

    double xL = dryL * inGain;
    double xR = dryR * inGain;

    const double inPeak = std::max(std::abs(xL), std::abs(xR));
    const float inDbNow = static_cast<float>(gainToDb(inPeak, -60.0));
    const float prevInDb = meterInDb.load(std::memory_order_relaxed);
    meterInDb.store(
        inDbNow > prevInDb ? inDbNow : (prevInDb * 0.9992f + inDbNow * 0.0008f),
        std::memory_order_relaxed);

    const double xfmrAmt = transformerColor ? transformerColorAmount : 0.0;
    if (xfmrAmt > 0.005) {
      // Input transformer low-frequency magnetic core weight (~95 Hz) +
      // asymmetric hysteresis
      xfmrInLowL += xfmrLowCoeff * (xL - xfmrInLowL);
      xfmrInLowR += xfmrLowCoeff * (xR - xfmrInLowR);
      const double revIron =
          (revision == KT1176Revision::RevA_Bluestripe)   ? 1.30
          : (revision == KT1176Revision::RevD_LN_Classic) ? 1.00
                                                          : 0.65;
      const double kIron = 0.055 * xfmrAmt * revIron;
      xL = sanitize(xL + (0.075 * xfmrAmt) * xfmrInLowL + kIron * (xL * xL));
      xR = sanitize(xR + (0.075 * xfmrAmt) * xfmrInLowR + kIron * (xR * xR));
    }

    // ---------------------------------------------------------------------
    // STAGE 2: RATIO BIAS & DETECTOR THRESHOLD NETWORK
    // ---------------------------------------------------------------------
    // In 1176LN, the Ratio buttons simultaneously switch:
    //   1. The DC bias threshold V_bias on the detector rectifier diodes
    //   2. The feedback loop gain K_R = (R - 1) driving the FET gate
    double vBiasThreshDb = -20.0;
    double targetRatio = 4.0;
    double kneeWidthDb = 5.5;

    switch (ratio) {
    case KT1176Ratio::Ratio_4_1:
      vBiasThreshDb = -21.0;
      targetRatio = 4.0;
      kneeWidthDb = 6.0;
      break;
    case KT1176Ratio::Ratio_8_1:
      vBiasThreshDb = -18.5;
      targetRatio = 8.0;
      kneeWidthDb = 4.5;
      break;
    case KT1176Ratio::Ratio_12_1:
      vBiasThreshDb = -16.5;
      targetRatio = 12.0;
      kneeWidthDb = 3.2;
      break;
    case KT1176Ratio::Ratio_20_1:
      vBiasThreshDb = -14.5;
      targetRatio = 20.0;
      kneeWidthDb = 1.8;
      break;
    case KT1176Ratio::AllButtons:
      // All-Buttons-In ("British Nuke"): bias shifts dramatically lower,
      // ratio sits between 12:1 and 20:1 with an aggressive non-linear knee
      vBiasThreshDb = -26.5;
      targetRatio = 16.5;
      kneeWidthDb = 8.5;
      break;
    }

    // ---------------------------------------------------------------------
    // STAGE 3: NONLINEAR JFET GAIN CELL: G(n) = f(Vc(n-1), Vbias, X(n))
    // ---------------------------------------------------------------------
    // Combine fast transient capacitor and slow program-dependent sustain
    // capacitor
    const double vcL = std::max(vcFastL, 0.70 * vcFastL + 0.30 * vcSlowL);
    const double vcR = std::max(vcFastR, 0.70 * vcFastR + 0.30 * vcSlowR);

    // Stereo-linked FET gate control voltage (80% stereo link, 20% dual-mono)
    const double vcLinked = std::max(vcL, vcR);
    const double effVcL = attackOff ? 0.0 : (0.80 * vcLinked + 0.20 * vcL);
    const double effVcR = attackOff ? 0.0 : (0.80 * vcLinked + 0.20 * vcR);

    feedbackGrDbL = effVcL;
    feedbackGrDbR = effVcR;

    const double baseGainL = dbToGain(-effVcL);
    const double baseGainR = dbToGain(-effVcR);

    // Smooth single-sample feedback loop transition
    const double gLinearL = 0.5 * (baseGainL + prevGainL);
    const double gLinearR = 0.5 * (baseGainR + prevGainR);
    prevGainL = baseGainL;
    prevGainR = baseGainR;

    // Nonlinear JFET drain-source conductance modulation f(Vc, Vbias, X):
    // When the JFET attenuates the signal, its channel resistance R_ds varies
    // slightly with instantaneous signal voltage V_ds = X(n), producing
    // signature FET 2nd/3rd harmonics
    const double revFetScale =
        (revision == KT1176Revision::RevA_Bluestripe)   ? 1.45
        : (revision == KT1176Revision::RevD_LN_Classic) ? 1.00
                                                        : 0.65;
    const double fetModDepth =
        fetCharacter * revFetScale * std::clamp(vcLinked / 18.0, 0.08, 1.2);

    // Signal Preamp Output S(n) = G(Vc, Vbias, X) * X(n)  <-- [OUTPUT SENSING
    // POINT]
    auto applyFetGainCell = [&](double x, double gLin) noexcept -> double {
      const double vds = x * gLin;
      // JFET triode-region conductance curvature (even + odd harmonic
      // enrichment)
      const double gMod = gLin * (1.0 + 0.042 * fetModDepth * vds -
                                  0.024 * fetModDepth * (vds * vds));
      return sanitize(x * gMod);
    };

    double sensedL = applyFetGainCell(xL, gLinearL);
    double sensedR = applyFetGainCell(xR, gLinearR);

    // In All-Buttons-In mode, the over-biased JFET & preamp stage exhibit
    // characteristic asymmetrical transient squashing & harmonic grit
    if (ratio == KT1176Ratio::AllButtons && !attackOff) {
      const double nukeDrive = 1.22 + 0.25 * fetCharacter;
      sensedL = std::tanh(sensedL * nukeDrive) * (1.0 / nukeDrive) +
                0.055 * fetCharacter * (sensedL * std::abs(sensedL));
      sensedR = std::tanh(sensedR * nukeDrive) * (1.0 / nukeDrive) +
                0.055 * fetCharacter * (sensedR * std::abs(sensedR));
    }

    // ---------------------------------------------------------------------
    // STAGE 4: FEEDBACK DETECTOR (Senses Signal Preamp Output S(n) AFTER FET!)
    // ---------------------------------------------------------------------
    double detInL = sensedL;
    double detInR = sensedR;
    if (sidechainHpfHz > 20.0) {
      scHpfZ1_L += scHpfCoeff * (detInL - scHpfZ1_L);
      scHpfZ1_R += scHpfCoeff * (detInR - scHpfZ1_R);
      detInL = sanitize(detInL - scHpfZ1_L);
      detInR = sanitize(detInR - scHpfZ1_R);
    }

    // Full-wave bipolar rectifier on feedback-sensed signal S(n)
    const double rectL = std::abs(detInL);
    const double rectR = std::abs(detInR);

    // Feedback Detector Transfer Function:
    // Because S(n) is already attenuated by effVc (S_dB = X_dB - effVc),
    // the closed-loop equilibrium satisfies:
    //   effVc_target = (X_dB - V_bias) * (1 - 1/R)
    // which in feedback coordinates (measuring S_dB = X_dB - effVc) corresponds
    // to open-loop detector gain K_R = (R - 1), solved implicitly for
    // unconditional stability:
    auto computeFeedbackTargetVcDb =
        [&](double rectLinear, double currentVcDb) noexcept -> double {
      // Reconstruct effective input excitation from feedback-sensed output S(n)
      // + current FET gate state
      const double sensedDb = gainToDb(rectLinear, -100.0);
      const double effectiveExcitationDb = sensedDb + currentVcDb * 0.92;
      const double over = effectiveExcitationDb - vBiasThreshDb;
      const double slope = 1.0 - (1.0 / targetRatio);
      const double halfKnee = kneeWidthDb * 0.5;

      if (over <= -halfKnee) {
        return 0.0;
      }
      if (over < halfKnee && kneeWidthDb > 0.1) {
        const double k = over + halfKnee;
        return slope * (k * k) / (2.0 * kneeWidthDb);
      }
      return over * slope;
    };

    const double targetVcL = computeFeedbackTargetVcDb(rectL, vcFastL);
    const double targetVcR = computeFeedbackTargetVcDb(rectR, vcFastR);

    // In All-Buttons mode, initial attack has the classic 1176 plateau lag
    const double effAttCoeff = (ratio == KT1176Ratio::AllButtons)
                                   ? std::min(0.9996, attackCoeff * 1.0003)
                                   : attackCoeff;

    // Charge / discharge fast RC detector capacitor
    vcFastL =
        (targetVcL > vcFastL)
            ? effAttCoeff * vcFastL + (1.0 - effAttCoeff) * targetVcL
            : releaseFastCoeff * vcFastL + (1.0 - releaseFastCoeff) * targetVcL;

    vcFastR =
        (targetVcR > vcFastR)
            ? effAttCoeff * vcFastR + (1.0 - effAttCoeff) * targetVcR
            : releaseFastCoeff * vcFastR + (1.0 - releaseFastCoeff) * targetVcR;

    // Charge / discharge slow program-dependent sustain memory capacitor
    vcSlowL =
        (targetVcL > vcSlowL)
            ? slowAttackCoeff * vcSlowL + (1.0 - slowAttackCoeff) * targetVcL
            : releaseSlowCoeff * vcSlowL + (1.0 - releaseSlowCoeff) * targetVcL;

    vcSlowR =
        (targetVcR > vcSlowR)
            ? slowAttackCoeff * vcSlowR + (1.0 - slowAttackCoeff) * targetVcR
            : releaseSlowCoeff * vcSlowR + (1.0 - releaseSlowCoeff) * targetVcR;

    // ---------------------------------------------------------------------
    // STAGE 5: CLASS-A OUTPUT AMPLIFIER & BILL PUTNAM 5002 OUTPUT TRANSFORMER
    // ---------------------------------------------------------------------
    double wetL = sensedL * outGain;
    double wetR = sensedR * outGain;

    if (xfmrAmt > 0.005) {
      xfmrOutLowL += xfmrLowCoeff * (wetL - xfmrOutLowL);
      xfmrOutLowR += xfmrLowCoeff * (wetR - xfmrOutLowR);
      const double outDrive = 0.85 + 0.35 * xfmrAmt;
      const double invOutDrive = 1.0 / std::tanh(outDrive);
      wetL = std::tanh((wetL + 0.04 * xfmrAmt * xfmrOutLowL) * outDrive) *
             invOutDrive;
      wetR = std::tanh((wetR + 0.04 * xfmrAmt * xfmrOutLowR) * outDrive) *
             invOutDrive;
    }

    // Optional authentic Class-A analog thermal floor (-84 dBFS)
    if (analogNoise) {
      noiseSeed = noiseSeed * 1664525u + 1013904223u;
      const double white =
          (static_cast<double>(static_cast<int32_t>(noiseSeed)) /
           2147483648.0) *
          0.000063;
      wetL += white;
      wetR -= white;
    }

    // First-order DC blocker (15 Hz)
    const double dcOutL = wetL - dcX1_L + 0.9982 * dcY1_L;
    dcX1_L = wetL;
    dcY1_L = sanitize(dcOutL);
    wetL = dcY1_L;

    const double dcOutR = wetR - dcX1_R + 0.9982 * dcY1_R;
    dcX1_R = wetR;
    dcY1_R = sanitize(dcOutR);
    wetR = dcY1_R;

    // ---------------------------------------------------------------------
    // STAGE 6: PARALLEL MIX & VU METER BALLISTICS
    // ---------------------------------------------------------------------
    inOutL = sanitize(dryL * (1.0 - mix) + wetL * mix);
    inOutR = sanitize(dryR * (1.0 - mix) + wetR * mix);

    const float curGr =
        static_cast<float>(std::max(feedbackGrDbL, feedbackGrDbR));
    const float prevGr = meterGrDb.load(std::memory_order_relaxed);
    meterGrDb.store(curGr > prevGr ? (prevGr * 0.84f + curGr * 0.16f)
                                   : (prevGr * 0.9988f + curGr * 0.0012f),
                    std::memory_order_relaxed);

    const double outPeak = std::max(std::abs(inOutL), std::abs(inOutR));
    const float outDbNow = static_cast<float>(gainToDb(outPeak, -60.0));
    const float prevOutDb = meterOutDb.load(std::memory_order_relaxed);
    meterOutDb.store(outDbNow > prevOutDb
                         ? (prevOutDb * 0.90f + outDbNow * 0.10f)
                         : (prevOutDb * 0.9990f + outDbNow * 0.0010f),
                     std::memory_order_relaxed);
  }

private:
  void updateTimeConstants() noexcept {
    const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    // Attack time constant tau_A (20 us to 800 us): alpha_A = exp(-1 / (Fs *
    // tau_A))
    const double attSec = std::max(2.0e-5, attackUs * 1e-6);
    attackCoeff = std::exp(-1.0 / (sr * attSec));
    slowAttackCoeff = std::exp(-1.0 / (sr * std::max(0.004, attSec * 14.0)));

    // Release time constant tau_R (50 ms to 1100 ms): alpha_R = exp(-1 / (Fs *
    // tau_R))
    const double relSec = std::max(0.050, releaseMs * 1e-3);
    releaseFastCoeff = std::exp(-1.0 / (sr * relSec));
    releaseSlowCoeff = std::exp(-1.0 / (sr * (relSec * 2.6)));
  }

  void updateFilters() noexcept {
    const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    xfmrLowCoeff = 1.0 - std::exp(-2.0 * std::numbers::pi * 95.0 / sr);
    if (sidechainHpfHz > 20.0) {
      scHpfCoeff = 1.0 - std::exp(-2.0 * std::numbers::pi *
                                  std::clamp(sidechainHpfHz, 20.0, 400.0) / sr);
    } else {
      scHpfCoeff = 0.0;
    }
  }

  double sampleRate = 48000.0;
  double inputGainDb = 0.0;
  double outputGainDb = 0.0;
  double attackUs = 180.0;
  double releaseMs = 235.0;
  double attackDial = 4.0;
  double releaseDial = 4.0;
  bool attackOff = false;
  KT1176Ratio ratio = KT1176Ratio::Ratio_4_1;
  KT1176Ratio lastStandardRatio = KT1176Ratio::Ratio_4_1;
  KT1176MeterMode meterMode = KT1176MeterMode::GR;
  KT1176Revision revision = KT1176Revision::RevD_LN_Classic;

  double fetCharacter = 0.50;
  bool transformerColor = true;
  double transformerColorAmount = 0.65;
  bool analogNoise = false;
  double sidechainHpfHz = 0.0;
  double mix = 1.0;
  bool bypassed = false;
  int currentPresetIndex = 11;
  uint32_t noiseSeed = 22222u;

  double attackCoeff = 0.0;
  double slowAttackCoeff = 0.0;
  double releaseFastCoeff = 0.0;
  double releaseSlowCoeff = 0.0;
  double xfmrLowCoeff = 0.01;
  double scHpfCoeff = 0.0;

  // Feedback detector & FET state
  double vcFastL = 0.0;
  double vcFastR = 0.0;
  double vcSlowL = 0.0;
  double vcSlowR = 0.0;
  double feedbackGrDbL = 0.0;
  double feedbackGrDbR = 0.0;
  double prevGainL = 1.0;
  double prevGainR = 1.0;

  double scHpfZ1_L = 0.0, scHpfZ1_R = 0.0;
  double xfmrInLowL = 0.0, xfmrInLowR = 0.0;
  double xfmrOutLowL = 0.0, xfmrOutLowR = 0.0;
  double dcX1_L = 0.0, dcX1_R = 0.0;
  double dcY1_L = 0.0, dcY1_R = 0.0;

  std::atomic<float> meterGrDb{0.0f};
  std::atomic<float> meterOutDb{-60.0f};
  std::atomic<float> meterInDb{-60.0f};
};

} // namespace nova::graviton
