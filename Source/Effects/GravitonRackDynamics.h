#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace nova {
namespace graviton {

/**
 * RackDynamicsEngine
 *
 * Unified studio dynamics processor for Effects Rack (Compressor / Expander-Gate)
 * combining:
 *  - DIE-Plugins (a-comp.c / a-exp.c) soft-knee quadratic transfer curve,
 *    coupled stereo peak/sidechain detection, asymmetric dB ballistics, and
 *    inline transfer-curve operating-point telemetry.
 *  - 7 Analog & Digital Circuit Models (Platinum Digital, Studio VCA, Studio FET,
 *    Classic VCA, Vintage VCA, Vintage FET, Vintage Opto).
 *  - Auto Gain (OFF / 0 dB / -12 dB), Auto Release, Output Limiter,
 *    Distortion stage (Off / Soft / Hard / Clip), Input/Output Gain, and Mix.
 *
 * Real-time audio-thread safe: zero allocations, zero locks, NaN/Inf sanitized.
 */
class RackDynamicsEngine {
public:
  enum class Mode : int { Compressor = 0, ExpanderGate = 1 };

  enum class CircuitModel : int {
    PlatinumDigital = 0,
    StudioVCA = 1,
    StudioFET = 2,
    ClassicVCA = 3,
    VintageVCA = 4,
    VintageFET = 5,
    VintageOpto = 6
  };

  enum class AutoGainMode : int { Off = 0, ZeroDb = 1, Minus12Db = 2 };

  enum class DistortionMode : int { Off = 0, Soft = 1, Hard = 2, Clip = 3 };

  RackDynamicsEngine() noexcept { reset(); }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0 && std::isfinite(sr)) {
      sampleRate = sr;
      updateCoefficients();
    }
  }

  void reset() noexcept {
    oldGainReductionDb = 0.0;
    rmsEnvelopeSq = 0.0;
    optoSlowReleaseDb = 0.0;
    limiterGain = 1.0;
    smoothedMakeupGain = 1.0;
    scHpfZ1L = scHpfZ2L = 0.0;
    scHpfZ1R = scHpfZ2R = 0.0;
    peakHoldSamples = 0;

    inLevelDb.store(-60.0f, std::memory_order_relaxed);
    outLevelDb.store(-60.0f, std::memory_order_relaxed);
    gainReductionDb.store(0.0f, std::memory_order_relaxed);
    peakHoldDb.store(-60.0f, std::memory_order_relaxed);
    operatingPointDb.store(-60.0f, std::memory_order_relaxed);
    limiterActive.store(false, std::memory_order_relaxed);
    updateCoefficients();
  }

  // --- Parameter Setters / Getters ---
  void setMode(int m) noexcept {
    mode.store(std::clamp(m, 0, 1), std::memory_order_relaxed);
  }
  int getMode() const noexcept { return mode.load(std::memory_order_relaxed); }

  void setCircuitModel(int c) noexcept {
    circuitModel.store(std::clamp(c, 0, 6), std::memory_order_relaxed);
    updateCoefficients();
  }
  int getCircuitModel() const noexcept {
    return circuitModel.load(std::memory_order_relaxed);
  }

  void setInputGainDb(double db) noexcept {
    inputGainDb.store(static_cast<float>(std::clamp(db, -30.0, 30.0)),
                      std::memory_order_relaxed);
  }
  double getInputGainDb() const noexcept {
    return static_cast<double>(inputGainDb.load(std::memory_order_relaxed));
  }

  void setOutputGainDb(double db) noexcept {
    outputGainDb.store(static_cast<float>(std::clamp(db, -30.0, 30.0)),
                       std::memory_order_relaxed);
  }
  double getOutputGainDb() const noexcept {
    return static_cast<double>(outputGainDb.load(std::memory_order_relaxed));
  }

  void setThresholdDb(double db) noexcept {
    thresholdDb.store(static_cast<float>(std::clamp(db, -60.0, 0.0)),
                      std::memory_order_relaxed);
  }
  double getThresholdDb() const noexcept {
    return static_cast<double>(thresholdDb.load(std::memory_order_relaxed));
  }

  void setRatio(double r) noexcept {
    ratio.store(static_cast<float>(std::clamp(r, 1.0, 30.0)),
                std::memory_order_relaxed);
  }
  double getRatio() const noexcept {
    return static_cast<double>(ratio.load(std::memory_order_relaxed));
  }

  void setMakeupDb(double db) noexcept {
    makeupDb.store(static_cast<float>(std::clamp(db, -20.0, 50.0)),
                   std::memory_order_relaxed);
  }
  double getMakeupDb() const noexcept {
    return static_cast<double>(makeupDb.load(std::memory_order_relaxed));
  }

  void setAutoGainMode(int m) noexcept {
    autoGainMode.store(std::clamp(m, 0, 2), std::memory_order_relaxed);
  }
  int getAutoGainMode() const noexcept {
    return autoGainMode.load(std::memory_order_relaxed);
  }

  void setKnee(double k) noexcept {
    knee.store(static_cast<float>(std::clamp(k, 0.0, 1.0)),
               std::memory_order_relaxed);
  }
  double getKnee() const noexcept {
    return static_cast<double>(knee.load(std::memory_order_relaxed));
  }

  void setAttackMs(double ms) noexcept {
    attackMs.store(static_cast<float>(std::clamp(ms, 0.05, 200.0)),
                   std::memory_order_relaxed);
    updateCoefficients();
  }
  double getAttackMs() const noexcept {
    return static_cast<double>(attackMs.load(std::memory_order_relaxed));
  }

  void setReleaseMs(double ms) noexcept {
    releaseMs.store(static_cast<float>(std::clamp(ms, 5.0, 5000.0)),
                    std::memory_order_relaxed);
    updateCoefficients();
  }
  double getReleaseMs() const noexcept {
    return static_cast<double>(releaseMs.load(std::memory_order_relaxed));
  }

  void setAutoRelease(bool on) noexcept {
    autoRelease.store(on, std::memory_order_relaxed);
  }
  bool getAutoRelease() const noexcept {
    return autoRelease.load(std::memory_order_relaxed);
  }

  void setLimiterEnabled(bool on) noexcept {
    limiterEnabled.store(on, std::memory_order_relaxed);
  }
  bool getLimiterEnabled() const noexcept {
    return limiterEnabled.load(std::memory_order_relaxed);
  }

  void setLimiterThresholdDb(double db) noexcept {
    limiterThresholdDb.store(static_cast<float>(std::clamp(db, -10.0, 0.0)),
                             std::memory_order_relaxed);
  }
  double getLimiterThresholdDb() const noexcept {
    return static_cast<double>(
        limiterThresholdDb.load(std::memory_order_relaxed));
  }

  void setDistortionMode(int d) noexcept {
    distortionMode.store(std::clamp(d, 0, 3), std::memory_order_relaxed);
  }
  int getDistortionMode() const noexcept {
    return distortionMode.load(std::memory_order_relaxed);
  }

  void setMixPct(double pct) noexcept {
    mixPct.store(static_cast<float>(std::clamp(pct, 0.0, 100.0)),
                 std::memory_order_relaxed);
  }
  double getMixPct() const noexcept {
    return static_cast<double>(mixPct.load(std::memory_order_relaxed));
  }

  void setSidechainFilterEnabled(bool on) noexcept {
    sidechainFilterEnabled.store(on, std::memory_order_relaxed);
  }
  bool getSidechainFilterEnabled() const noexcept {
    return sidechainFilterEnabled.load(std::memory_order_relaxed);
  }

  void setSidechainHpfHz(double hz) noexcept {
    sidechainHpfHz.store(static_cast<float>(std::clamp(hz, 20.0, 400.0)),
                         std::memory_order_relaxed);
    updateScHpf();
  }
  double getSidechainHpfHz() const noexcept {
    return static_cast<double>(sidechainHpfHz.load(std::memory_order_relaxed));
  }

  void setSidechainListen(bool on) noexcept {
    sidechainListen.store(on, std::memory_order_relaxed);
  }
  bool getSidechainListen() const noexcept {
    return sidechainListen.load(std::memory_order_relaxed);
  }

  // --- Telemetry Getters (for UI) ---
  float getInLevelDb() const noexcept {
    return inLevelDb.load(std::memory_order_relaxed);
  }
  float getOutLevelDb() const noexcept {
    return outLevelDb.load(std::memory_order_relaxed);
  }
  float getGainReductionDb() const noexcept {
    return gainReductionDb.load(std::memory_order_relaxed);
  }
  float getPeakHoldDb() const noexcept {
    return peakHoldDb.load(std::memory_order_relaxed);
  }
  float getOperatingPointDb() const noexcept {
    return operatingPointDb.load(std::memory_order_relaxed);
  }
  bool isLimiterActive() const noexcept {
    return limiterActive.load(std::memory_order_relaxed);
  }

  /**
   * Compute static transfer curve output (dB) for a given input level (dB)
   * using DIE-Plugins (a-comp / a-exp) soft-knee equations + Auto/Manual makeup.
   * Used by both DSP and Graph view in RackDynamicsPanel.
   */
  double computeTransferDb(double inDb) const noexcept {
    const int m = mode.load(std::memory_order_relaxed);
    const double thres = static_cast<double>(thresholdDb.load(std::memory_order_relaxed));
    const double rat = std::max(1.0, static_cast<double>(ratio.load(std::memory_order_relaxed)));
    const double k = static_cast<double>(knee.load(std::memory_order_relaxed));
    const double width = 6.0 * k + 0.01;

    double outDb = inDb;
    if (m == static_cast<int>(Mode::Compressor)) {
      if (2.0 * (inDb - thres) < -width) {
        outDb = inDb;
      } else if (2.0 * (inDb - thres) > width) {
        outDb = thres + (inDb - thres) / rat;
      } else {
        const double d = inDb - thres + width * 0.5;
        outDb = inDb + (1.0 / rat - 1.0) * (d * d) / (2.0 * width);
      }
    } else {
      if (2.0 * (inDb - thres) < -width) {
        outDb = thres + (inDb - thres) * rat;
      } else if (2.0 * (inDb - thres) > width) {
        outDb = inDb;
      } else {
        const double d = inDb - thres - width * 0.5;
        outDb = inDb + (1.0 - rat) * (d * d) / (2.0 * width);
      }
    }
    return outDb + computeTotalMakeupDb();
  }

  double computeTotalMakeupDb() const noexcept {
    const double manual = static_cast<double>(makeupDb.load(std::memory_order_relaxed));
    const int ag = autoGainMode.load(std::memory_order_relaxed);
    if (ag == static_cast<int>(AutoGainMode::Off))
      return manual;

    const int m = mode.load(std::memory_order_relaxed);
    if (m == static_cast<int>(Mode::ExpanderGate))
      return manual;

    const double thres = static_cast<double>(thresholdDb.load(std::memory_order_relaxed));
    const double rat = std::max(1.0, static_cast<double>(ratio.load(std::memory_order_relaxed)));
    // Auto-gain estimate: compensate expected compression at reference level
    const double refDb = (ag == static_cast<int>(AutoGainMode::ZeroDb)) ? 0.0 : -12.0;
    const double over = std::max(0.0, refDb - thres);
    const double compLoss = over * (1.0 - 1.0 / rat) * 0.55;
    return std::clamp(manual + compLoss, -24.0, 50.0);
  }

  void processStereo(double &inOutL, double &inOutR) noexcept {
    inOutL = sanitize(inOutL);
    inOutR = sanitize(inOutR);

    const double dryL = inOutL;
    const double dryR = inOutR;

    // 1. Input Gain Stage
    const double inGainLin = dbToGain(static_cast<double>(inputGainDb.load(std::memory_order_relaxed)));
    const double sigL = inOutL * inGainLin;
    const double sigR = inOutR * inGainLin;

    // 2. Sidechain Detection Path (optional HPF)
    double detL = sigL;
    double detR = sigR;
    if (sidechainFilterEnabled.load(std::memory_order_relaxed)) {
      detL = processScHpfL(detL);
      detR = processScHpfR(detR);
    }

    if (sidechainListen.load(std::memory_order_relaxed)) {
      inOutL = sanitize(detL);
      inOutR = sanitize(detR);
      return;
    }

    const double maxAbs = std::max(std::abs(detL), std::abs(detR));
    const double sqAvg = 0.5 * (detL * detL + detR * detR);
    rmsEnvelopeSq = rmsAlpha * rmsEnvelopeSq + (1.0 - rmsAlpha) * sqAvg;
    const double rmsAbs = std::sqrt(std::max(0.0, rmsEnvelopeSq));

    const int model = circuitModel.load(std::memory_order_relaxed);
    double detectorLin = maxAbs;
    if (model == static_cast<int>(CircuitModel::ClassicVCA) ||
        model == static_cast<int>(CircuitModel::VintageOpto)) {
      detectorLin = 0.65 * rmsAbs + 0.35 * maxAbs;
    } else if (model == static_cast<int>(CircuitModel::VintageVCA)) {
      detectorLin = 0.40 * rmsAbs + 0.60 * maxAbs;
    }

    const double lxg = (detectorLin <= 1.0e-8) ? -160.0 : gainToDb(detectorLin);
    const double rawInDb = (std::max(std::abs(sigL), std::abs(sigR)) <= 1.0e-8)
                               ? -60.0
                               : std::clamp(gainToDb(std::max(std::abs(sigL), std::abs(sigR))), -60.0, 6.0);

    // Smooth input meter + 3s peak hold
    const float prevIn = inLevelDb.load(std::memory_order_relaxed);
    const float nextIn = (rawInDb > prevIn)
                             ? static_cast<float>(rawInDb)
                             : static_cast<float>(prevIn * 0.9992 + rawInDb * 0.0008);
    inLevelDb.store(nextIn, std::memory_order_relaxed);

    const float prevHold = peakHoldDb.load(std::memory_order_relaxed);
    if (rawInDb >= prevHold || peakHoldSamples > static_cast<uint32_t>(sampleRate * 3.0)) {
      peakHoldDb.store(static_cast<float>(rawInDb), std::memory_order_relaxed);
      peakHoldSamples = 0;
    } else {
      ++peakHoldSamples;
    }

    // 3. DIE-Plugins Soft-Knee Transfer Curve (a-comp.c / a-exp.c)
    const int m = mode.load(std::memory_order_relaxed);
    const double thres = static_cast<double>(thresholdDb.load(std::memory_order_relaxed));
    const double rat = std::max(1.0, static_cast<double>(ratio.load(std::memory_order_relaxed)));
    const double k = static_cast<double>(knee.load(std::memory_order_relaxed));
    const double width = 6.0 * k + 0.01;

    double lyg = lxg;
    if (m == static_cast<int>(Mode::Compressor)) {
      if (2.0 * (lxg - thres) < -width) {
        lyg = lxg;
      } else if (2.0 * (lxg - thres) > width) {
        lyg = thres + (lxg - thres) / rat;
      } else {
        const double d = lxg - thres + width * 0.5;
        lyg = lxg + (1.0 / rat - 1.0) * (d * d) / (2.0 * width);
      }
    } else {
      if (2.0 * (lxg - thres) < -width) {
        lyg = thres + (lxg - thres) * rat;
      } else if (2.0 * (lxg - thres) > width) {
        lyg = lxg;
      } else {
        const double d = lxg - thres - width * 0.5;
        lyg = lxg + (1.0 - rat) * (d * d) / (2.0 * width);
      }
    }

    double targetGr = std::max(0.0, lxg - lyg);
    if (m == static_cast<int>(Mode::ExpanderGate) && targetGr > 80.0) {
      targetGr = 80.0;
    }

    // 4. Ballistics (Attack / Release + Program-Dependent Auto Release + Opto Tail)
    double effReleaseCoeff = releaseCoeff;
    if (autoRelease.load(std::memory_order_relaxed) ||
        model == static_cast<int>(CircuitModel::VintageFET) ||
        model == static_cast<int>(CircuitModel::VintageVCA)) {
      // Program-dependent release: deeper GR slows release slightly for smooth musicality
      const double autoScale = 1.0 + std::clamp(oldGainReductionDb / 18.0, 0.0, 1.5);
      effReleaseCoeff = std::pow(releaseCoeff, 1.0 / autoScale);
    }

    double currentGr = targetGr;
    if (m == static_cast<int>(Mode::Compressor)) {
      if (currentGr < oldGainReductionDb) {
        currentGr = effReleaseCoeff * oldGainReductionDb + (1.0 - effReleaseCoeff) * currentGr;
      } else if (currentGr > oldGainReductionDb) {
        currentGr = attackCoeff * oldGainReductionDb + (1.0 - attackCoeff) * currentGr;
      }
    } else {
      // Expander/Gate ballistics matching a-exp.c
      if (currentGr > oldGainReductionDb) {
        currentGr = effReleaseCoeff * oldGainReductionDb + (1.0 - effReleaseCoeff) * currentGr;
      } else if (currentGr < oldGainReductionDb) {
        currentGr = attackCoeff * oldGainReductionDb + (1.0 - attackCoeff) * currentGr;
      }
    }

    // Vintage Opto dual-stage optical cell memory tail
    if (model == static_cast<int>(CircuitModel::VintageOpto)) {
      optoSlowReleaseDb = optoSlowCoeff * optoSlowReleaseDb + (1.0 - optoSlowCoeff) * currentGr;
      if (currentGr < optoSlowReleaseDb) {
        currentGr = 0.55 * currentGr + 0.45 * optoSlowReleaseDb;
      }
    } else {
      optoSlowReleaseDb = currentGr;
    }

    currentGr = sanitize(std::clamp(currentGr, 0.0, 160.0));
    oldGainReductionDb = currentGr;

    gainReductionDb.store(static_cast<float>(std::clamp(currentGr, 0.0, 60.0)),
                          std::memory_order_relaxed);

    // Operating point X on transfer curve (a-comp.c lines 446-452)
    double stateX = lxg;
    if (m == static_cast<int>(Mode::Compressor) && rat > 1.001) {
      const double kneeLimGr = (1.0 - 1.0 / rat) * width * 0.5;
      if (currentGr > kneeLimGr) {
        stateX = currentGr / (1.0 - 1.0 / rat) + thres;
      } else {
        stateX = std::sqrt(std::max(0.0, (2.0 * width * currentGr) / (1.0 - 1.0 / rat))) +
                 thres - width * 0.5;
      }
    }
    operatingPointDb.store(static_cast<float>(std::clamp(stateX, -60.0, 10.0)),
                           std::memory_order_relaxed);

    // 5. Apply Gain Reduction + Smoothed Makeup Gain
    const double compGainLin = dbToGain(-currentGr);
    const double targetMakeupLin = dbToGain(computeTotalMakeupDb());
    smoothedMakeupGain += makeupTau * (targetMakeupLin - smoothedMakeupGain);

    double wetL = sigL * compGainLin * smoothedMakeupGain;
    double wetR = sigR * compGainLin * smoothedMakeupGain;

    // 6. Circuit Character Coloration (subtle analog harmonics)
    applyCircuitColor(model, wetL, wetR);

    // 7. Distortion / Saturation Stage (Off / Soft / Hard / Clip)
    const int distMode = distortionMode.load(std::memory_order_relaxed);
    if (distMode > 0) {
      wetL = applyDistortion(distMode, wetL);
      wetR = applyDistortion(distMode, wetR);
    }

    // 8. Output Gain Stage
    const double outGainLin = dbToGain(static_cast<double>(outputGainDb.load(std::memory_order_relaxed)));
    wetL *= outGainLin;
    wetR *= outGainLin;

    // 9. Parallel Mix (Input -> Output)
    const double mix = static_cast<double>(mixPct.load(std::memory_order_relaxed)) * 0.01;
    double outL = dryL * (1.0 - mix) + wetL * mix;
    double outR = dryR * (1.0 - mix) + wetR * mix;

    // 10. Output Limiter Stage
    if (limiterEnabled.load(std::memory_order_relaxed)) {
      const double limThresLin =
          dbToGain(static_cast<double>(limiterThresholdDb.load(std::memory_order_relaxed)));
      const double peak = std::max(std::abs(outL), std::abs(outR));
      const double desiredLimGain = (peak > limThresLin && peak > 1.0e-12)
                                        ? (limThresLin / peak)
                                        : 1.0;
      if (desiredLimGain < limiterGain) {
        limiterGain = limAttackCoeff * limiterGain + (1.0 - limAttackCoeff) * desiredLimGain;
      } else {
        limiterGain = limReleaseCoeff * limiterGain + (1.0 - limReleaseCoeff) * desiredLimGain;
      }
      outL *= limiterGain;
      outR *= limiterGain;
      limiterActive.store(limiterGain < 0.995, std::memory_order_relaxed);
    } else {
      limiterGain = 1.0;
      limiterActive.store(false, std::memory_order_relaxed);
    }

    outL = sanitize(outL);
    outR = sanitize(outR);

    const double rawOutDb = (std::max(std::abs(outL), std::abs(outR)) <= 1.0e-8)
                                ? -60.0
                                : std::clamp(gainToDb(std::max(std::abs(outL), std::abs(outR))), -60.0, 6.0);
    const float prevOut = outLevelDb.load(std::memory_order_relaxed);
    const float nextOut = (rawOutDb > prevOut)
                              ? static_cast<float>(rawOutDb)
                              : static_cast<float>(prevOut * 0.9992 + rawOutDb * 0.0008);
    outLevelDb.store(nextOut, std::memory_order_relaxed);

    inOutL = outL;
    inOutR = outR;
  }

private:
  void updateCoefficients() noexcept {
    const double sr = std::max(8000.0, sampleRate);
    double attMs = static_cast<double>(attackMs.load(std::memory_order_relaxed));
    double relMs = static_cast<double>(releaseMs.load(std::memory_order_relaxed));

    switch (static_cast<CircuitModel>(circuitModel.load(std::memory_order_relaxed))) {
    case CircuitModel::PlatinumDigital:
      break;
    case CircuitModel::StudioVCA:
      attMs *= 0.85;
      relMs *= 0.90;
      break;
    case CircuitModel::StudioFET:
      attMs *= 0.45;
      relMs *= 0.80;
      break;
    case CircuitModel::ClassicVCA:
      attMs *= 0.95;
      relMs *= 0.85;
      break;
    case CircuitModel::VintageVCA:
      attMs *= 1.20;
      relMs *= 1.15;
      break;
    case CircuitModel::VintageFET:
      attMs *= 0.50;
      relMs *= 1.05;
      break;
    case CircuitModel::VintageOpto:
      attMs *= 1.80;
      relMs *= 1.40;
      break;
    }

    attMs = std::max(0.05, attMs);
    relMs = std::max(5.0, relMs);

    attackCoeff = std::exp(-1000.0 / (attMs * sr));
    releaseCoeff = std::exp(-1000.0 / (relMs * sr));
    rmsAlpha = std::exp(-1000.0 / (12.0 * sr));
    optoSlowCoeff = std::exp(-1000.0 / (650.0 * sr));
    makeupTau = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 25.0 / sr);
    limAttackCoeff = std::exp(-1000.0 / (0.15 * sr));
    limReleaseCoeff = std::exp(-1000.0 / (45.0 * sr));
    updateScHpf();
  }

  void updateScHpf() noexcept {
    const double sr = std::max(8000.0, sampleRate);
    const double fc = std::clamp(
        static_cast<double>(sidechainHpfHz.load(std::memory_order_relaxed)),
        20.0, std::min(400.0, sr * 0.4));
    const double w0 = 2.0 * 3.14159265358979323846 * fc / sr;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / std::sqrt(2.0);
    const double a0 = 1.0 + alpha;
    scB0 = ((1.0 + cosW0) * 0.5) / a0;
    scB1 = (-(1.0 + cosW0)) / a0;
    scB2 = ((1.0 + cosW0) * 0.5) / a0;
    scA1 = (-2.0 * cosW0) / a0;
    scA2 = (1.0 - alpha) / a0;
  }

  double processScHpfL(double x) noexcept {
    const double y = scB0 * x + scHpfZ1L;
    scHpfZ1L = scB1 * x - scA1 * y + scHpfZ2L;
    scHpfZ2L = scB2 * x - scA2 * y;
    return sanitize(y);
  }

  double processScHpfR(double x) noexcept {
    const double y = scB0 * x + scHpfZ1R;
    scHpfZ1R = scB1 * x - scA1 * y + scHpfZ2R;
    scHpfZ2R = scB2 * x - scA2 * y;
    return sanitize(y);
  }

  static void applyCircuitColor(int model, double &l, double &r) noexcept {
    switch (static_cast<CircuitModel>(model)) {
    case CircuitModel::PlatinumDigital:
      return;
    case CircuitModel::StudioVCA:
      l = l - 0.008 * l * l * l;
      r = r - 0.008 * r * r * r;
      return;
    case CircuitModel::StudioFET:
      l = l + 0.012 * l * l - 0.010 * l * l * l;
      r = r + 0.012 * r * r - 0.010 * r * r * r;
      return;
    case CircuitModel::ClassicVCA:
      l = l - 0.005 * l * l * l;
      r = r - 0.005 * r * r * r;
      return;
    case CircuitModel::VintageVCA:
      l = std::tanh(l * 1.03) / 1.03;
      r = std::tanh(r * 1.03) / 1.03;
      return;
    case CircuitModel::VintageFET:
      l = std::tanh(l * 1.08 + 0.018 * l * l) / 1.08;
      r = std::tanh(r * 1.08 + 0.018 * r * r) / 1.08;
      return;
    case CircuitModel::VintageOpto:
      l = std::tanh(l * 1.05 + 0.022 * l * l) / 1.05;
      r = std::tanh(r * 1.05 + 0.022 * r * r) / 1.05;
      return;
    }
  }

  static double applyDistortion(int distMode, double x) noexcept {
    switch (static_cast<DistortionMode>(distMode)) {
    case DistortionMode::Off:
      return x;
    case DistortionMode::Soft:
      return std::tanh(x * 1.25) / 1.15;
    case DistortionMode::Hard:
      return std::tanh(x * 1.85) / 1.35;
    case DistortionMode::Clip:
      return std::clamp(x * 1.25, -0.98, 0.98);
    }
    return x;
  }

  static double dbToGain(double db) noexcept {
    return std::pow(10.0, 0.05 * db);
  }

  static double gainToDb(double g) noexcept {
    return 20.0 * std::log10(std::max(1.0e-8, g));
  }

  static double sanitize(double v) noexcept {
    return std::isfinite(v) ? std::clamp(v, -16.0, 16.0) : 0.0;
  }

  double sampleRate = 48000.0;
  double oldGainReductionDb = 0.0;
  double rmsEnvelopeSq = 0.0;
  double optoSlowReleaseDb = 0.0;
  double limiterGain = 1.0;
  double smoothedMakeupGain = 1.0;

  double attackCoeff = 0.99;
  double releaseCoeff = 0.999;
  double rmsAlpha = 0.998;
  double optoSlowCoeff = 0.9999;
  double makeupTau = 0.003;
  double limAttackCoeff = 0.85;
  double limReleaseCoeff = 0.9995;

  double scB0 = 1.0, scB1 = 0.0, scB2 = 0.0, scA1 = 0.0, scA2 = 0.0;
  double scHpfZ1L = 0.0, scHpfZ2L = 0.0;
  double scHpfZ1R = 0.0, scHpfZ2R = 0.0;
  uint32_t peakHoldSamples = 0;

  std::atomic<int> mode{0};
  std::atomic<int> circuitModel{5}; // Vintage FET default
  std::atomic<float> inputGainDb{0.0f};
  std::atomic<float> outputGainDb{0.0f};
  std::atomic<float> thresholdDb{-18.0f};
  std::atomic<float> ratio{4.0f};
  std::atomic<float> makeupDb{0.0f};
  std::atomic<int> autoGainMode{0};
  std::atomic<float> knee{0.35f};
  std::atomic<float> attackMs{10.0f};
  std::atomic<float> releaseMs{120.0f};
  std::atomic<bool> autoRelease{false};
  std::atomic<bool> limiterEnabled{false};
  std::atomic<float> limiterThresholdDb{-1.0f};
  std::atomic<int> distortionMode{0};
  std::atomic<float> mixPct{100.0f};
  std::atomic<bool> sidechainFilterEnabled{false};
  std::atomic<float> sidechainHpfHz{80.0f};
  std::atomic<bool> sidechainListen{false};

  std::atomic<float> inLevelDb{-60.0f};
  std::atomic<float> outLevelDb{-60.0f};
  std::atomic<float> gainReductionDb{0.0f};
  std::atomic<float> peakHoldDb{-60.0f};
  std::atomic<float> operatingPointDb{-60.0f};
  std::atomic<bool> limiterActive{false};
};

} // namespace graviton
} // namespace nova
