#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace nova::graviton {

// ==============================================================================
// TC Electronic Master X HD / System 6000 Multiband Dynamics Processor
// Reference: MIDAS Heritage-D HD96-24 Chapter 18
//
// Key Features:
// - 3-Band Phase-Coherent Linkwitz-Riley 24 dB/octave (LR4) Crossover
// - Low band phase-compensated with Mid/High All-Pass 2nd-order stage
// - Independent 3-Band Soft-Knee Compressor with Makeup Gain & Solo/Mute
// - True-Peak Lookahead Brickwall Limiter (-0.1 dBFS ceiling)
// ==============================================================================

inline BiquadCoefficients
makeAllPass2ndOrder(Sample sampleRate, Sample cutoffHz,
                    Sample q = 0.70710678118654752440) noexcept {
  BiquadCoefficients unity{};
  if (!std::isfinite(sampleRate) || !std::isfinite(cutoffHz) ||
      !std::isfinite(q) || sampleRate < 1000.0 || cutoffHz <= 0.0 || q <= 0.0)
    return unity;

  const Sample sr = std::clamp(sampleRate, 1000.0, 768000.0);
  const Sample f = std::clamp(cutoffHz, 5.0, sr * 0.499);
  const Sample quality = std::clamp(q, 0.05, 50.0);
  const Sample omega = 2.0 * 3.14159265358979323846 * f / sr;
  const Sample sn = std::sin(omega);
  const Sample cs = std::cos(omega);
  const Sample alpha = sn / (2.0 * quality);
  const Sample a0 = 1.0 + alpha;
  if (!std::isfinite(a0) || std::abs(a0) < 1.0e-20)
    return unity;

  BiquadCoefficients c;
  c.b0 = (1.0 - alpha) / a0;
  c.b1 = (-2.0 * cs) / a0;
  c.b2 = (1.0 + alpha) / a0;
  c.a1 = (-2.0 * cs) / a0;
  c.a2 = (1.0 - alpha) / a0;
  if (!std::isfinite(c.b0) || !std::isfinite(c.b1) || !std::isfinite(c.b2) ||
      !std::isfinite(c.a1) || !std::isfinite(c.a2))
    return unity;
  return c;
}

enum class TargetCurve : int {
  Flat = 0,
  Bass = 1,
  Air = 2,
  Smiley = 3
};

enum class DynamicsMode : int {
  Expander = 0,
  Compressor = 1,
  Limiter = 2
};

class MasterXMultiband final {
public:
  static constexpr size_t kNumBands = 3;
  static constexpr size_t kMaxLookaheadSamples = 512; // ~5.3 ms @ 96 kHz

  struct BandConfig {
    double thresholdDb = -20.0;
    double ratio = 1.80;
    double attackMs = 35.0;
    double releaseMs = 150.0;
    double kneeDb = 3.0;
    double makeupGainDb = 0.0;
    bool bypassed = false;
    bool muted = false;
    bool solo = false;
    std::atomic<float> gainReductionDb{0.0f};
    double envelopeGrDb = 0.0;
  };

  MasterXMultiband() noexcept { reset(); }

  void reset() noexcept {
    // Reset crossover biquads
    lpf1_L1.reset();
    lpf1_L2.reset();
    lpf1_R1.reset();
    lpf1_R2.reset();
    hpf1_L1.reset();
    hpf1_L2.reset();
    hpf1_R1.reset();
    hpf1_R2.reset();

    lpf2_L1.reset();
    lpf2_L2.reset();
    lpf2_R1.reset();
    lpf2_R2.reset();
    hpf2_L1.reset();
    hpf2_L2.reset();
    hpf2_R1.reset();
    hpf2_R2.reset();

    ap2_L.reset();
    ap2_R.reset();

    for (auto &b : bands) {
      b.envelopeGrDb = 0.0;
      b.gainReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    // Reset limiter ring buffer
    std::fill(lookaheadBufL.begin(), lookaheadBufL.end(), 0.0);
    std::fill(lookaheadBufR.begin(), lookaheadBufR.end(), 0.0);
    lookaheadWritePos = 0;
    limiterGain = 1.0;
    limiterGrDb.store(0.0f, std::memory_order_relaxed);

    updateCoefficients();
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
      cachedSampleRate = sr;
      updateCoefficients();
    }
  }

  // Crossover frequency setters
  void setCrossoverLowMidHz(double freqHz) noexcept {
    crossoverLowMidHz = std::clamp(freqHz, 40.0, 1000.0);
    updateCoefficients();
  }
  double getCrossoverLowMidHz() const noexcept { return crossoverLowMidHz; }

  void setCrossoverMidHighHz(double freqHz) noexcept {
    crossoverMidHighHz = std::clamp(freqHz, 1000.0, 16000.0);
    updateCoefficients();
  }
  double getCrossoverMidHighHz() const noexcept { return crossoverMidHighHz; }

  // Band configuration accessors
  void setBandThresholdDb(size_t band, double db) noexcept {
    if (band < kNumBands)
      bands[band].thresholdDb = std::clamp(db, -40.0, 0.0);
  }
  double getBandThresholdDb(size_t band) const noexcept {
    return band < kNumBands ? bands[band].thresholdDb : 0.0;
  }

  void setBandRatio(size_t band, double r) noexcept {
    if (band < kNumBands)
      bands[band].ratio = std::clamp(r, 1.0, 20.0);
  }
  double getBandRatio(size_t band) const noexcept {
    return band < kNumBands ? bands[band].ratio : 1.0;
  }

  void setBandAttackMs(size_t band, double ms) noexcept {
    if (band < kNumBands)
      bands[band].attackMs = std::clamp(ms, 0.1, 100.0);
  }
  double getBandAttackMs(size_t band) const noexcept {
    return band < kNumBands ? bands[band].attackMs : 10.0;
  }

  void setBandReleaseMs(size_t band, double ms) noexcept {
    if (band < kNumBands)
      bands[band].releaseMs = std::clamp(ms, 10.0, 2000.0);
  }
  double getBandReleaseMs(size_t band) const noexcept {
    return band < kNumBands ? bands[band].releaseMs : 100.0;
  }

  void setBandKneeDb(size_t band, double db) noexcept {
    if (band < kNumBands)
      bands[band].kneeDb = std::clamp(db, 0.0, 12.0);
  }
  double getBandKneeDb(size_t band) const noexcept {
    return band < kNumBands ? bands[band].kneeDb : 0.0;
  }

  void setBandMakeupGainDb(size_t band, double db) noexcept {
    if (band < kNumBands)
      bands[band].makeupGainDb = std::clamp(db, -12.0, 18.0);
  }
  double getBandMakeupGainDb(size_t band) const noexcept {
    return band < kNumBands ? bands[band].makeupGainDb : 0.0;
  }

  void setBandSolo(size_t band, bool s) noexcept {
    if (band < kNumBands)
      bands[band].solo = s;
  }
  bool getBandSolo(size_t band) const noexcept {
    return band < kNumBands ? bands[band].solo : false;
  }

  void setBandMute(size_t band, bool m) noexcept {
    if (band < kNumBands)
      bands[band].muted = m;
  }
  bool getBandMute(size_t band) const noexcept {
    return band < kNumBands ? bands[band].muted : false;
  }

  void setBandBypass(size_t band, bool bp) noexcept {
    if (band < kNumBands)
      bands[band].bypassed = bp;
  }
  bool getBandBypass(size_t band) const noexcept {
    return band < kNumBands ? bands[band].bypassed : false;
  }

  float getBandGainReductionDb(size_t band) const noexcept {
    return band < kNumBands
               ? bands[band].gainReductionDb.load(std::memory_order_relaxed)
               : 0.0f;
  }

  // Brickwall Limiter controls
  void setLimiterCeilingDb(double db) noexcept {
    limiterCeilingDb = std::clamp(db, -12.0, 0.0);
  }
  double getLimiterCeilingDb() const noexcept { return limiterCeilingDb; }

  void setLimiterReleaseMs(double ms) noexcept {
    limiterReleaseMs = std::clamp(ms, 10.0, 500.0);
  }
  double getLimiterReleaseMs() const noexcept { return limiterReleaseMs; }

  void setLimiterEnabled(bool enabled) noexcept { limiterEnabled = enabled; }
  bool getLimiterEnabled() const noexcept { return limiterEnabled; }

  float getLimiterGainReductionDb() const noexcept {
    return limiterGrDb.load(std::memory_order_relaxed);
  }

  void setTargetCurve(TargetCurve c) noexcept {
    targetCurve = c;
    applyTargetCurve();
  }
  TargetCurve getTargetCurve() const noexcept { return targetCurve; }

  void setDynamicsMode(DynamicsMode m) noexcept { dynMode = m; }
  DynamicsMode getDynamicsMode() const noexcept { return dynMode; }

  void setFactorPct(double pct) noexcept { factorPct = std::clamp(pct, 0.0, 100.0); }
  double getFactorPct() const noexcept { return factorPct; }

  void setMix(double m) noexcept { mix = std::clamp(m, 0.0, 1.0); }
  double getMix() const noexcept { return mix; }

  void setSoftClipDb(double db) noexcept { softClipDb = std::clamp(db, -18.0, 0.0); }
  double getSoftClipDb() const noexcept { return softClipDb; }

  void setInGainDb(double db) noexcept {
    inGainDb = std::clamp(db, -18.0, 18.0);
    if (gainLinked) outGainDb = -inGainDb;
  }
  double getInGainDb() const noexcept { return inGainDb; }

  void setOutGainDb(double db) noexcept {
    outGainDb = std::clamp(db, -18.0, 18.0);
    if (gainLinked) inGainDb = -outGainDb;
  }
  double getOutGainDb() const noexcept { return outGainDb; }

  void setGainLinked(bool lk) noexcept { gainLinked = lk; }
  bool getGainLinked() const noexcept { return gainLinked; }

  float getMeterInL() const noexcept { return meterInL.load(std::memory_order_relaxed); }
  float getMeterInR() const noexcept { return meterInR.load(std::memory_order_relaxed); }
  float getMeterOutL() const noexcept { return meterOutL.load(std::memory_order_relaxed); }
  float getMeterOutR() const noexcept { return meterOutR.load(std::memory_order_relaxed); }

  struct Preset {
    const char* name = "Default";
    const char* category = "Master Bus";
    TargetCurve curve = TargetCurve::Smiley;
    DynamicsMode mode = DynamicsMode::Compressor;
    double factorPct = 20.0;
    double mix = 1.0;
    double xoverLowMid = 120.0;
    double xoverMidHigh = 3500.0;
    double softClipDb = -18.0;
    double inGainDb = 0.0;
    double outGainDb = 0.0;
    bool limiterEnabled = true;
    double limiterCeiling = -0.1;
    double lowThresh = -20.0, lowRatio = 1.80, lowAtt = 35.0, lowRel = 150.0, lowGain = 0.0;
    double midThresh = -18.0, midRatio = 1.60, midAtt = 25.0, midRel = 120.0, midGain = 0.0;
    double highThresh = -16.0, highRatio = 1.50, highAtt = 15.0, highRel = 80.0, highGain = 0.0;
  };

  static const std::vector<Preset>& getFactoryPresets() noexcept {
    static const std::vector<Preset> kPresets = {
        { "01 - Master Bus 101 Tight Master", "Master Bus", TargetCurve::Smiley, DynamicsMode::Compressor, 20.0, 1.0, 120.0, 3500.0, -9.0, 0.0, 0.0, true, -0.1, -20.0, 1.8, 35.0, 150.0, 0.0, -18.0, 1.6, 25.0, 120.0, 0.0, -16.0, 1.5, 15.0, 80.0, 0.0 },
        { "02 - Punchy Master Bus", "Master Bus", TargetCurve::Flat, DynamicsMode::Compressor, 15.0, 1.0, 100.0, 4000.0, -12.0, 0.0, 0.5, true, -0.2, -18.0, 2.2, 50.0, 120.0, 0.0, -16.0, 1.8, 40.0, 100.0, 0.0, -14.0, 1.5, 25.0, 70.0, 0.0 },
        { "03 - Transparent Mix Finisher", "Master Bus", TargetCurve::Flat, DynamicsMode::Compressor, 10.0, 1.0, 130.0, 3200.0, -18.0, 0.0, 0.0, true, -0.3, -22.0, 1.4, 45.0, 180.0, 0.0, -20.0, 1.3, 35.0, 150.0, 0.0, -18.0, 1.25, 20.0, 100.0, 0.0 },
        { "04 - EDM Dancefloor Maximizer", "Electronic", TargetCurve::Smiley, DynamicsMode::Compressor, 45.0, 1.0, 90.0, 4500.0, -6.0, 1.0, 0.0, true, -0.05, -24.0, 2.8, 20.0, 100.0, 1.0, -20.0, 2.2, 15.0, 80.0, 0.0, -18.0, 2.0, 10.0, 60.0, 0.5 },
        { "05 - Rock & Metal Wall of Sound", "Rock", TargetCurve::Bass, DynamicsMode::Compressor, 30.0, 1.0, 110.0, 3000.0, -6.0, 0.5, 0.0, true, -0.1, -22.0, 2.5, 30.0, 120.0, 0.5, -18.0, 2.0, 20.0, 90.0, -0.5, -15.0, 1.8, 12.0, 70.0, 0.5 },
        { "06 - Acoustic & Orchestral Glue", "Acoustic", TargetCurve::Air, DynamicsMode::Compressor, 8.0, 1.0, 150.0, 3500.0, -18.0, 0.0, 0.0, true, -0.5, -26.0, 1.3, 60.0, 250.0, 0.0, -24.0, 1.2, 50.0, 200.0, 0.0, -22.0, 1.2, 30.0, 140.0, 0.0 },
        { "07 - Vocal Air & Smooth Presence", "Vocal Bus", TargetCurve::Air, DynamicsMode::Compressor, 22.0, 1.0, 180.0, 4000.0, -12.0, 0.0, 0.0, true, -0.2, -18.0, 1.8, 30.0, 120.0, -1.0, -16.0, 1.6, 20.0, 100.0, 0.5, -14.0, 1.5, 10.0, 60.0, 1.5 },
        { "08 - Broadcast Density Leveler", "Broadcast", TargetCurve::Smiley, DynamicsMode::Compressor, 35.0, 1.0, 120.0, 3200.0, -6.0, 0.0, 0.0, true, -0.1, -25.0, 3.2, 15.0, 140.0, 0.0, -22.0, 2.8, 12.0, 110.0, 0.0, -19.0, 2.4, 8.0, 75.0, 0.0 },
        { "09 - Hip-Hop Sub Bass Heavy", "Urban", TargetCurve::Bass, DynamicsMode::Compressor, 35.0, 1.0, 85.0, 3000.0, -7.0, 1.0, 0.0, true, -0.1, -22.0, 3.0, 35.0, 110.0, 2.0, -18.0, 1.8, 25.0, 90.0, 0.0, -16.0, 1.6, 15.0, 60.0, 0.0 },
        { "10 - Multiband Expander Clean", "Restoration", TargetCurve::Flat, DynamicsMode::Expander, 25.0, 1.0, 120.0, 3500.0, -18.0, 0.0, 0.0, true, -0.1, -30.0, 1.5, 10.0, 100.0, 0.0, -28.0, 1.4, 10.0, 80.0, 0.0, -26.0, 1.3, 8.0, 60.0, 0.0 },
        { "11 - Vinyl Pre-Master Safety", "Master Bus", TargetCurve::Flat, DynamicsMode::Compressor, 18.0, 1.0, 140.0, 5000.0, -14.0, 0.0, -0.5, true, -0.8, -19.0, 1.6, 40.0, 160.0, -0.5, -17.0, 1.4, 30.0, 130.0, 0.0, -15.0, 1.4, 20.0, 90.0, -0.5 },
        { "12 - Brickwall Peak Defense", "Limiter", TargetCurve::Flat, DynamicsMode::Limiter, 50.0, 1.0, 120.0, 3500.0, -3.0, 0.0, 0.0, true, -0.05, -12.0, 8.0, 1.0, 60.0, 0.0, -10.0, 8.0, 1.0, 50.0, 0.0, -8.0, 8.0, 0.5, 40.0, 0.0 }
    };
    return kPresets;
  }

  void loadPreset(const Preset& p) noexcept {
    setTargetCurve(p.curve);
    setDynamicsMode(p.mode);
    setFactorPct(p.factorPct);
    setMix(p.mix);
    setCrossoverLowMidHz(p.xoverLowMid);
    setCrossoverMidHighHz(p.xoverMidHigh);
    setSoftClipDb(p.softClipDb);
    setInGainDb(p.inGainDb);
    setOutGainDb(p.outGainDb);
    setLimiterEnabled(p.limiterEnabled);
    setLimiterCeilingDb(p.limiterCeiling);

    // Low band
    setBandThresholdDb(0, p.lowThresh);
    setBandRatio(0, p.lowRatio);
    setBandAttackMs(0, p.lowAtt);
    setBandReleaseMs(0, p.lowRel);
    setBandMakeupGainDb(0, p.lowGain);

    // Mid band
    setBandThresholdDb(1, p.midThresh);
    setBandRatio(1, p.midRatio);
    setBandAttackMs(1, p.midAtt);
    setBandReleaseMs(1, p.midRel);
    setBandMakeupGainDb(1, p.midGain);

    // High band
    setBandThresholdDb(2, p.highThresh);
    setBandRatio(2, p.highRatio);
    setBandAttackMs(2, p.highAtt);
    setBandReleaseMs(2, p.highRel);
    setBandMakeupGainDb(2, p.highGain);
  }

  Preset getCurrentPresetState(const char* nameStr = "Current Settings") const noexcept {
    Preset p;
    p.name = nameStr;
    p.category = "User";
    p.curve = targetCurve;
    p.mode = dynMode;
    p.factorPct = factorPct;
    p.mix = mix;
    p.xoverLowMid = crossoverLowMidHz;
    p.xoverMidHigh = crossoverMidHighHz;
    p.softClipDb = softClipDb;
    p.inGainDb = inGainDb;
    p.outGainDb = outGainDb;
    p.limiterEnabled = limiterEnabled;
    p.limiterCeiling = limiterCeilingDb;

    p.lowThresh = bands[0].thresholdDb;
    p.lowRatio = bands[0].ratio;
    p.lowAtt = bands[0].attackMs;
    p.lowRel = bands[0].releaseMs;
    p.lowGain = bands[0].makeupGainDb;

    p.midThresh = bands[1].thresholdDb;
    p.midRatio = bands[1].ratio;
    p.midAtt = bands[1].attackMs;
    p.midRel = bands[1].releaseMs;
    p.midGain = bands[1].makeupGainDb;

    p.highThresh = bands[2].thresholdDb;
    p.highRatio = bands[2].ratio;
    p.highAtt = bands[2].attackMs;
    p.highRel = bands[2].releaseMs;
    p.highGain = bands[2].makeupGainDb;
    return p;
  }


  // Core Stereo DSP Process
  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double rawInL = inOutL;
    const double rawInR = inOutR;

    const double inGain = dbToGain(inGainDb);
    const double outGain = dbToGain(outGainDb);

    const double xL = sanitize(rawInL * inGain);
    const double xR = sanitize(rawInR * inGain);

    // ------------------------------------------------------------------
    // 1. Linkwitz-Riley 4th Order 3-Way Crossover Split
    // ------------------------------------------------------------------
    // Low Band: LP1 cascaded 2 stages, then phase-compensated by AP2
    const double lowL1 = lpf1_L1.process(xL);
    const double lowR1 = lpf1_R1.process(xR);
    const double lowL2 = lpf1_L2.process(lowL1);
    const double lowR2 = lpf1_R2.process(lowR1);
    double lowL = ap2_L.process(lowL2);
    double lowR = ap2_R.process(lowR2);

    // High-pass from Filter 1 feeds Mid and High splits
    const double hpL1 = hpf1_L1.process(xL);
    const double hpR1 = hpf1_R1.process(xR);
    const double hpL2 = hpf1_L2.process(hpL1);
    const double hpR2 = hpf1_R2.process(hpR1);

    // Mid Band: Filter 1 HP2 -> Filter 2 LP2
    const double midL1 = lpf2_L1.process(hpL2);
    const double midR1 = lpf2_R1.process(hpR2);
    double midL = lpf2_L2.process(midL1);
    double midR = lpf2_R2.process(midR1);

    // High Band: Filter 1 HP2 -> Filter 2 HP2
    const double hiL1 = hpf2_L1.process(hpL2);
    const double hiR1 = hpf2_R1.process(hpR2);
    double hiL = hpf2_L2.process(hiL1);
    double hiR = hpf2_R2.process(hiR1);

    // ------------------------------------------------------------------
    // 2. Solo / Mute Evaluation
    // ------------------------------------------------------------------
    const bool anySolo = bands[0].solo || bands[1].solo || bands[2].solo;

    // ------------------------------------------------------------------
    // 3. Process Dynamics for each of the 3 Bands
    // ------------------------------------------------------------------
    processBandDynamics(0, lowL, lowR, anySolo);
    processBandDynamics(1, midL, midR, anySolo);
    processBandDynamics(2, hiL, hiR, anySolo);

    // Sum the 3 coherent bands
    double sumL = lowL + midL + hiL;
    double sumR = lowR + midR + hiR;

    // Parallel Dry / Wet Mix
    double blendedL = sumL;
    double blendedR = sumR;
    if (mix < 0.999) {
      blendedL = rawInL * (1.0 - mix) + sumL * mix;
      blendedR = rawInR * (1.0 - mix) + sumR * mix;
    }

    // Soft-clip saturation stage
    if (softClipDb > -17.5) {
      const double drive = dbToGain(-softClipDb);
      blendedL = std::tanh(blendedL * drive) / drive;
      blendedR = std::tanh(blendedR * drive) / drive;
    }

    // Apply output make-up gain prior to final brickwall limiter stage
    blendedL *= outGain;
    blendedR *= outGain;

    // ------------------------------------------------------------------
    // 4. True-Peak Lookahead Brickwall Limiter (Final Output Defense)
    // ------------------------------------------------------------------
    if (limiterEnabled) {
      processLimiter(blendedL, blendedR);
    } else {
      limiterGrDb.store(0.0f, std::memory_order_relaxed);
    }

    inOutL = sanitize(blendedL);
    inOutR = sanitize(blendedR);

    // Update real-time peak meters
    const float inPeakLVal = static_cast<float>(std::abs(rawInL));
    const float inPeakRVal = static_cast<float>(std::abs(rawInR));
    const float outPeakLVal = static_cast<float>(std::abs(inOutL));
    const float outPeakRVal = static_cast<float>(std::abs(inOutR));

    float pInL = meterInL.load(std::memory_order_relaxed);
    meterInL.store(std::max(inPeakLVal, pInL * 0.999f), std::memory_order_relaxed);
    float pInR = meterInR.load(std::memory_order_relaxed);
    meterInR.store(std::max(inPeakRVal, pInR * 0.999f), std::memory_order_relaxed);
    float pOutL = meterOutL.load(std::memory_order_relaxed);
    meterOutL.store(std::max(outPeakLVal, pOutL * 0.999f), std::memory_order_relaxed);
    float pOutR = meterOutR.load(std::memory_order_relaxed);
    meterOutR.store(std::max(outPeakRVal, pOutR * 0.999f), std::memory_order_relaxed);
  }

private:
  void updateCoefficients() noexcept {
    const double sr = cachedSampleRate;
    const double qButterworth = 0.70710678118654752440;

    // Low/Mid Split (Filter 1)
    const auto lpf1 = makeButterworthLPF(sr, crossoverLowMidHz, qButterworth);
    lpf1_L1.setCoefficients(lpf1);
    lpf1_L2.setCoefficients(lpf1);
    lpf1_R1.setCoefficients(lpf1);
    lpf1_R2.setCoefficients(lpf1);

    const auto hpf1 = makeButterworthHPF(sr, crossoverLowMidHz, qButterworth);
    hpf1_L1.setCoefficients(hpf1);
    hpf1_L2.setCoefficients(hpf1);
    hpf1_R1.setCoefficients(hpf1);
    hpf1_R2.setCoefficients(hpf1);

    // Mid/High Split (Filter 2)
    const auto lpf2 = makeButterworthLPF(sr, crossoverMidHighHz, qButterworth);
    lpf2_L1.setCoefficients(lpf2);
    lpf2_L2.setCoefficients(lpf2);
    lpf2_R1.setCoefficients(lpf2);
    lpf2_R2.setCoefficients(lpf2);

    const auto hpf2 = makeButterworthHPF(sr, crossoverMidHighHz, qButterworth);
    hpf2_L1.setCoefficients(hpf2);
    hpf2_L2.setCoefficients(hpf2);
    hpf2_R1.setCoefficients(hpf2);
    hpf2_R2.setCoefficients(hpf2);

    // Phase Compensation All-Pass for Low Band (Cutoff matches Filter 2)
    const auto ap2 = makeAllPass2ndOrder(sr, crossoverMidHighHz, qButterworth);
    ap2_L.setCoefficients(ap2);
    ap2_R.setCoefficients(ap2);
  }

  void processBandDynamics(size_t bandIdx, double &sL, double &sR,
                           bool anySolo) noexcept {
    auto &b = bands[bandIdx];

    // Mute / Solo check
    if ((anySolo && !b.solo) || b.muted) {
      sL = 0.0;
      sR = 0.0;
      b.gainReductionDb.store(0.0f, std::memory_order_relaxed);
      return;
    }

    if (b.bypassed || (dynMode == DynamicsMode::Compressor && b.ratio <= 1.001)) {
      b.gainReductionDb.store(0.0f, std::memory_order_relaxed);
      return;
    }

    // Stereo-linked peak detector
    const double peak = std::max(std::abs(sL), std::abs(sR));
    const double peakDb = gainToDb(std::max(peak, 1.0e-7), -120.0);

    // Factor coupling: factorPct (0..100) deepens compression & increases ratio
    const double effectiveThresh = b.thresholdDb - (factorPct * 0.15);
    const double effectiveRatio = b.ratio * (1.0 + factorPct * 0.01);

    double targetGrDb = 0.0;
    const double halfKnee = b.kneeDb * 0.5;

    if (dynMode == DynamicsMode::Expander) {
      // Downwards expander below threshold
      if (peakDb < (effectiveThresh - halfKnee)) {
        targetGrDb = (1.0 - effectiveRatio) * (effectiveThresh - peakDb);
      } else if (b.kneeDb > 0.001 && peakDb < (effectiveThresh + halfKnee)) {
        const double delta = (effectiveThresh + halfKnee) - peakDb;
        targetGrDb = (1.0 - effectiveRatio) * (delta * delta) / (2.0 * b.kneeDb);
      }
      targetGrDb = std::clamp(targetGrDb, -36.0, 0.0);
    } else if (dynMode == DynamicsMode::Limiter) {
      // Brickwall band limiter
      const double limRatio = std::max(12.0, effectiveRatio * 4.0);
      if (peakDb > effectiveThresh) {
        targetGrDb = (effectiveThresh + (peakDb - effectiveThresh) / limRatio) - peakDb;
      }
      targetGrDb = std::clamp(targetGrDb, -40.0, 0.0);
    } else {
      // Standard Soft-knee compressor
      if (b.kneeDb > 0.001 && peakDb > (effectiveThresh - halfKnee) &&
          peakDb < (effectiveThresh + halfKnee)) {
        const double delta = peakDb - effectiveThresh + halfKnee;
        targetGrDb = (1.0 / effectiveRatio - 1.0) * (delta * delta) / (2.0 * b.kneeDb);
      } else if (peakDb >= (effectiveThresh + halfKnee)) {
        targetGrDb =
            (effectiveThresh + (peakDb - effectiveThresh) / effectiveRatio) - peakDb;
      }
      targetGrDb = std::clamp(targetGrDb, -40.0, 0.0);
    }

    // Envelope ballistics (attack/release smoothing in dB domain)
    const double attMs = (dynMode == DynamicsMode::Limiter) ? std::min(1.0, b.attackMs) : b.attackMs;
    const double attCoeff =
        std::exp(-1.0 / (attMs * 0.001 * cachedSampleRate));
    const double relCoeff =
        std::exp(-1.0 / (b.releaseMs * 0.001 * cachedSampleRate));
    const double coeff = (targetGrDb < b.envelopeGrDb) ? attCoeff : relCoeff;

    b.envelopeGrDb = coeff * b.envelopeGrDb + (1.0 - coeff) * targetGrDb;
    b.gainReductionDb.store(static_cast<float>(-b.envelopeGrDb),
                            std::memory_order_relaxed);

    const double totalGain = dbToGain(b.envelopeGrDb + b.makeupGainDb);
    sL *= totalGain;
    sR *= totalGain;
  }

  void processLimiter(double &inOutL, double &inOutR) noexcept {
    const double ceilingAmp = dbToGain(limiterCeilingDb);

    // Peak on incoming sample
    const double inPeak = std::max(std::abs(inOutL), std::abs(inOutR));

    // Required limiter gain for incoming sample to stay below ceiling
    const double targetGain =
        (inPeak > ceilingAmp) ? (ceilingAmp / inPeak) : 1.0;

    // Limiter ballistics: fast attack (~1 ms) lookahead smoothing, controlled
    // release
    const double attCoeff = std::exp(-1.0 / (0.001 * cachedSampleRate));
    const double relCoeff =
        std::exp(-1.0 / (limiterReleaseMs * 0.001 * cachedSampleRate));

    if (targetGain < limiterGain) {
      limiterGain = attCoeff * limiterGain + (1.0 - attCoeff) * targetGain;
    } else {
      limiterGain = relCoeff * limiterGain + (1.0 - relCoeff) * targetGain;
    }

    // Store into lookahead circular buffer
    lookaheadBufL[static_cast<size_t>(lookaheadWritePos)] = inOutL;
    lookaheadBufR[static_cast<size_t>(lookaheadWritePos)] = inOutR;

    // Read out delayed sample (~1.5 ms lookahead delay)
    const int lookaheadSamples =
        std::clamp(static_cast<int>(0.0015 * cachedSampleRate), 16,
                   static_cast<int>(kMaxLookaheadSamples - 1));
    const int readPos = (lookaheadWritePos - lookaheadSamples +
                         static_cast<int>(kMaxLookaheadSamples)) %
                        static_cast<int>(kMaxLookaheadSamples);

    lookaheadWritePos =
        (lookaheadWritePos + 1) % static_cast<int>(kMaxLookaheadSamples);

    // Apply smoothed limiter gain to delayed audio
    double delayedL = lookaheadBufL[static_cast<size_t>(readPos)] * limiterGain;
    double delayedR = lookaheadBufR[static_cast<size_t>(readPos)] * limiterGain;

    // Brickwall true-peak safety ceiling clamp
    delayedL = std::clamp(delayedL, -ceilingAmp, ceilingAmp);
    delayedR = std::clamp(delayedR, -ceilingAmp, ceilingAmp);

    const double curGrDb = -gainToDb(std::max(limiterGain, 1.0e-5), -100.0);
    limiterGrDb.store(static_cast<float>(std::max(0.0, curGrDb)),
                      std::memory_order_relaxed);

    inOutL = delayedL;
    inOutR = delayedR;
  }

  double cachedSampleRate = 48000.0;
  double crossoverLowMidHz = 120.0;
  double crossoverMidHighHz = 3500.0;

  // Crossover LR4 cascaded biquads
  Biquad lpf1_L1, lpf1_L2, lpf1_R1, lpf1_R2;
  Biquad hpf1_L1, hpf1_L2, hpf1_R1, hpf1_R2;
  Biquad lpf2_L1, lpf2_L2, lpf2_R1, lpf2_R2;
  Biquad hpf2_L1, hpf2_L2, hpf2_R1, hpf2_R2;
  Biquad ap2_L, ap2_R;

  // 3 Bands: 0=Low, 1=Mid, 2=High
  std::array<BandConfig, kNumBands> bands;

  // True-Peak Lookahead Limiter
  bool limiterEnabled = true;
  double limiterCeilingDb = -0.1; // Default -0.1 dBFS true-peak ceiling
  double limiterReleaseMs = 50.0;
  double limiterGain = 1.0;
  std::atomic<float> limiterGrDb{0.0f};

  std::array<double, kMaxLookaheadSamples> lookaheadBufL{};
  std::array<double, kMaxLookaheadSamples> lookaheadBufR{};
  int lookaheadWritePos = 0;

  TargetCurve targetCurve = TargetCurve::Smiley;
  DynamicsMode dynMode = DynamicsMode::Compressor;
  double factorPct = 20.0;
  double mix = 1.0;
  double softClipDb = -18.0;
  double inGainDb = 0.0;
  double outGainDb = 0.0;
  bool gainLinked = false;

  std::atomic<float> meterInL{0.0f};
  std::atomic<float> meterInR{0.0f};
  std::atomic<float> meterOutL{0.0f};
  std::atomic<float> meterOutR{0.0f};

  void applyTargetCurve() noexcept {
    switch (targetCurve) {
    case TargetCurve::Flat:
      bands[0].makeupGainDb = 0.0;
      bands[1].makeupGainDb = 0.0;
      bands[2].makeupGainDb = 0.0;
      break;
    case TargetCurve::Bass:
      bands[0].makeupGainDb = 4.0;
      bands[1].makeupGainDb = 0.0;
      bands[2].makeupGainDb = -1.0;
      break;
    case TargetCurve::Air:
      bands[0].makeupGainDb = 0.0;
      bands[1].makeupGainDb = 0.0;
      bands[2].makeupGainDb = 4.5;
      break;
    case TargetCurve::Smiley:
      bands[0].makeupGainDb = 3.0;
      bands[1].makeupGainDb = -2.0;
      bands[2].makeupGainDb = 3.0;
      break;
    }
  }
};

} // namespace nova::graviton
