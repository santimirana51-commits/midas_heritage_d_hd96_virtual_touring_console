#pragma once

// =============================================================================
// GravitonDVR2Reverb.h
// NOVA HD — Graviton Mk2 DSP Audio Engine
// TC Electronic System 6000 DVR2 Non-Linear Reverb & EMT 250 Vintage Emulation
//
// Authentic Architecture:
//   • Input Level stage + Asymmetric Input Transformer saturation & warmth
//   • 75 Hz 2-pole Butterworth Low-Frequency Trim filter (Direct Form II
//   Transposed) • Pre-Delay line (0–500 ms) • 4-Stage Series Modulated Allpass
//   Diffuser network + Early Reflection taps • 8-line Unitary Hadamard Feedback
//   Delay Network (FDN) with coprime lengths • Irrational prime-ratio
//   multi-phase LFO chorus modulation (Hermite cubic) • True 3-Band
//   Phase-Coherent Crossover Decay (Lo Decay, Mid Decay, Hi Decay) • EMT 250
//   12-bit Pre-Emphasis / Symmetric Mid-Tread Quantizer / De-Emphasis •
//   Resonant Hi Cut + Hi QScale contour filter & HF loop damping • Orthogonal
//   Stereo Output Matrix + Kill Dry (Aux/Insert mode) + Peak/OVR Meters • 28
//   Factory Presets across 5 System 6000 Banks
//
// Constraints:
//   • Zero heap allocation inside processStereo()
//   • 64-bit double-precision DSP throughout
//   • Real-time safe: processStereo() is noexcept and allocation-free
// =============================================================================

#include "DSP/GravitonDSP.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <numbers>
#include <vector>

namespace nova::graviton {

class DVR2NonLinearReverb final {
public:
  static constexpr int kFdnOrder = 8;
  static constexpr int kNumDiffusers = 4;
  static constexpr int kMaxPreDelaySmp = 48000; // 500 ms @ 96 kHz
  static constexpr int kFdnBufLen = 8192;  // Power of 2, >= max scaled length
  static constexpr int kDiffBufLen = 2048; // Power of 2 for allpass diffusers

  // Coprime base FDN delay lengths (samples @ 48 kHz) — 30.8 ms to 73.9 ms
  static constexpr int kBaseLengths[kFdnOrder] = {1481, 1867, 2053, 2399,
                                                  2707, 3001, 3313, 3547};

  // Coprime base input allpass diffuser lengths (samples @ 48 kHz) — 3.1 ms
  // to 10.2 ms
  static constexpr int kDiffBaseLengths[kNumDiffusers] = {149, 211, 337, 491};

  // Irrational-spaced LFO frequencies (Hz) to prevent periodic modulation
  // beating
  static constexpr double kLfoFreqs[kFdnOrder] = {0.53, 0.67, 0.79, 0.97,
                                                  1.13, 1.31, 1.51, 1.73};

  // ── Factory Preset Definition ─────────────────────────────────────────
  struct Preset {
    const char *name;
    const char *bank;
    double preDelayMs;
    double decaySec;
    double loDecayMult;
    double hiDecayMult;
    double hiCutHz;
    double hiQScale;
    double modulationPct;
    bool inputTrans;
    bool trimLoFreq;
    bool loRes;
    double inLevelDb;
    double wetLevelDb;
    double dryLevelDb;
  };

  DVR2NonLinearReverb() noexcept {
    buildHadamardMatrix();
    setSampleRate(48000.0);
    reset();
    loadPresetByIndex(0);
  }

  void setSampleRate(double sr) noexcept {
    sampleRate_ = (sr > 1000.0) ? sr : 48000.0;
    const double ratio = sampleRate_ / 48000.0;

    for (int i = 0; i < kFdnOrder; ++i) {
      int scaled = static_cast<int>(
          std::round(static_cast<double>(kBaseLengths[i]) * ratio));
      scaled = std::clamp(scaled, 32, kFdnBufLen - 32);
      fdnLen_[i] = scaled;
    }

    for (int i = 0; i < kNumDiffusers; ++i) {
      int scaled = static_cast<int>(
          std::round(static_cast<double>(kDiffBaseLengths[i]) * ratio));
      scaled = std::clamp(scaled, 16, kDiffBufLen - 16);
      diffLen_[i] = scaled;
    }

    maxPreDelaySamples_ = std::clamp(static_cast<int>(0.500 * sampleRate_), 10,
                                     kMaxPreDelaySmp - 1);
    recalcCoefficients();
  }

  void reset() noexcept {
    if (buffers_) {
      std::memset(buffers_->preDelayBuf, 0, sizeof(buffers_->preDelayBuf));
      for (int i = 0; i < kNumDiffusers; ++i) {
        std::memset(buffers_->diffBufL[i], 0, sizeof(buffers_->diffBufL[i]));
        std::memset(buffers_->diffBufR[i], 0, sizeof(buffers_->diffBufR[i]));
      }
      for (int i = 0; i < kFdnOrder; ++i) {
        std::memset(buffers_->fdnBuf[i], 0, sizeof(buffers_->fdnBuf[i]));
      }
    }
    preDelayWr_ = 0;

    for (int i = 0; i < kNumDiffusers; ++i) {
      diffWr_[i] = 0;
    }

    for (int i = 0; i < kFdnOrder; ++i) {
      fdnWr_[i] = 0;
      loState_[i] = 0.0;
      hiState_[i] = 0.0;
      dampState_[i] = 0.0;
      dcX1_[i] = 0.0;
      dcY1_[i] = 0.0;
      lfoPhase_[i] = static_cast<double>(i) *
                     (2.0 * std::numbers::pi / static_cast<double>(kFdnOrder));
    }

    trimZ1_L_ = trimZ1_R_ = 0.0;
    trimZ2_L_ = trimZ2_R_ = 0.0;
    hiCutZ1_L_ = hiCutZ1_R_ = 0.0;
    hiCutZ2_L_ = hiCutZ2_R_ = 0.0;
    transLpL_ = transLpR_ = 0.0;
    preEmpL_ = preEmpR_ = 0.0;
    deEmpL_ = deEmpR_ = 0.0;

    meterInL.store(0.0f, std::memory_order_relaxed);
    meterInR.store(0.0f, std::memory_order_relaxed);
    meterOutL.store(0.0f, std::memory_order_relaxed);
    meterOutR.store(0.0f, std::memory_order_relaxed);
    overloadIn.store(false, std::memory_order_relaxed);
    overloadOut.store(false, std::memory_order_relaxed);
  }

  // ── 1. Main 4 Parameters ──────────────────────────────────────────────
  void setPreDelay(double ms) noexcept {
    preDelayMs_ = std::clamp(ms, 0.0, 500.0);
  }
  double getPreDelay() const noexcept { return preDelayMs_; }

  void setDecay(double sec) noexcept {
    decaySec_ = std::clamp(sec, 0.2, 20.0);
    recalcCoefficients();
  }
  double getDecay() const noexcept { return decaySec_; }

  void setLoDecay(double mult) noexcept {
    loDecayMult_ = std::clamp(mult, 0.1, 2.5);
    recalcCoefficients();
  }
  double getLoDecay() const noexcept { return loDecayMult_; }

  void setHiDecay(double mult) noexcept {
    hiDecayMult_ = std::clamp(mult, 0.1, 2.0);
    recalcCoefficients();
  }
  double getHiDecay() const noexcept { return hiDecayMult_; }

  // ── 2. Vintage Section ────────────────────────────────────────────────
  void setHiCut(double hz) noexcept {
    hiCutHz_ = std::clamp(hz, 1000.0, 18000.0);
    recalcCoefficients();
  }
  double getHiCut() const noexcept { return hiCutHz_; }

  void setHiQScale(double q) noexcept {
    hiQScale_ = std::clamp(q, 0.5, 2.5);
    recalcCoefficients();
  }
  double getHiQScale() const noexcept { return hiQScale_; }

  void setModulation(double pct) noexcept {
    modulationPct_ = std::clamp(pct, 0.0, 200.0);
  }
  double getModulation() const noexcept { return modulationPct_; }

  void setInputTrans(bool on) noexcept { inputTrans_ = on; }
  bool getInputTrans() const noexcept { return inputTrans_; }

  void setTrimLoFreq(bool on) noexcept { trimLoFreq_ = on; }
  bool getTrimLoFreq() const noexcept { return trimLoFreq_; }

  void setLoRes(bool on) noexcept { loRes_ = on; }
  bool getLoRes() const noexcept { return loRes_; }

  void vintageReset() noexcept {
    hiCutHz_ = 8000.0;
    hiQScale_ = 1.4;
    modulationPct_ = 100.0;
    inputTrans_ = true;
    trimLoFreq_ = true;
    loRes_ = true;
    recalcCoefficients();
  }

  // ── 3. Levels Section ─────────────────────────────────────────────────
  void setInLevel(double db) noexcept {
    inLevelDb_ = std::clamp(db, -100.0, 12.0);
  }
  double getInLevel() const noexcept { return inLevelDb_; }

  void setWetLevel(double db) noexcept {
    wetLevelDb_ = std::clamp(db, -100.0, 6.0);
  }
  double getWetLevel() const noexcept { return wetLevelDb_; }

  void setDryLevel(double db) noexcept {
    dryLevelDb_ = std::clamp(db, -100.0, 0.0);
  }
  double getDryLevel() const noexcept { return dryLevelDb_; }

  void setMix(double mix01) noexcept {
    const double m = std::clamp(mix01, 0.0, 1.0);
    wetLevelDb_ = (m <= 1.0e-5) ? -100.0 : 20.0 * std::log10(m);
    dryLevelDb_ = ((1.0 - m) <= 1.0e-5) ? -100.0 : 20.0 * std::log10(1.0 - m);
  }

  void setKillDry(bool kill) noexcept { killDry_ = kill; }
  bool getKillDry() const noexcept { return killDry_; }

  void setDensity(double d) noexcept { densityPct_ = std::clamp(d, 0.0, 1.0); }
  double getDensity() const noexcept { return densityPct_; }

  void setNonLinearDrive(double db) noexcept {
    nonLinearDriveDb_ = std::clamp(db, 0.0, 18.0);
    recalcCoefficients();
  }
  double getNonLinearDrive() const noexcept { return nonLinearDriveDb_; }

  void setStereoWidth(double w) noexcept {
    stereoWidth_ = std::clamp(w, 0.0, 1.0);
  }
  double getStereoWidth() const noexcept { return stereoWidth_; }

  void setBypass(bool bp) noexcept { bypassed_ = bp; }
  bool getBypass() const noexcept { return bypassed_; }

  // ── 4. Meters & Overload Telemetry ────────────────────────────────────
  float getMeterInL() const noexcept {
    return meterInL.load(std::memory_order_relaxed);
  }
  float getMeterInR() const noexcept {
    return meterInR.load(std::memory_order_relaxed);
  }
  float getMeterOutL() const noexcept {
    return meterOutL.load(std::memory_order_relaxed);
  }
  float getMeterOutR() const noexcept {
    return meterOutR.load(std::memory_order_relaxed);
  }
  bool hasOverloadIn() const noexcept {
    return overloadIn.load(std::memory_order_relaxed);
  }
  bool hasOverloadOut() const noexcept {
    return overloadOut.load(std::memory_order_relaxed);
  }

  void resetOverload() noexcept {
    overloadIn.store(false, std::memory_order_relaxed);
    overloadOut.store(false, std::memory_order_relaxed);
  }

  // ── 5. Presets Management (28 System 6000 & NOVA HD Presets) ───────────
  static const std::vector<Preset> &getFactoryPresets() noexcept {
    static const std::vector<Preset> presets = {
        // ── Bank 01 · Vocal (8 presets) ──
        {"EMT 250 Nice Vocal", "01 · Vocal", 35.0, 2.2, 0.85, 0.80, 8000.0, 1.4,
         100.0, true, true, true, 0.0, -3.5, 0.0},
        {"EMT 250 Ballad Vocal", "01 · Vocal", 55.0, 3.2, 0.90, 0.70, 7000.0,
         1.3, 115.0, true, true, true, 0.0, -3.0, 0.0},
        {"Long Vocal", "01 · Vocal", 60.0, 4.5, 0.75, 0.65, 6500.0, 1.2, 125.0,
         true, true, true, 0.0, -4.0, 0.0},
        {"Warm Vocal", "01 · Vocal", 25.0, 2.0, 1.20, 0.60, 5500.0, 1.5, 90.0,
         true, false, true, 0.0, -3.5, 0.0},
        {"Bright Vocal", "01 · Vocal", 20.0, 1.8, 0.70, 1.15, 10500.0, 1.6,
         110.0, true, true, false, 0.0, -4.0, 0.0},
        {"Intimate Vocal", "01 · Vocal", 12.0, 1.2, 0.80, 0.85, 7500.0, 1.4,
         80.0, true, true, true, 0.0, -5.0, 0.0},
        {"Wide Vocal", "01 · Vocal", 45.0, 2.6, 0.90, 0.75, 8500.0, 1.5, 140.0,
         true, true, true, 0.0, -3.0, 0.0},
        {"Vocal Ambience", "01 · Vocal", 8.0, 0.9, 0.65, 0.90, 9000.0, 1.3,
         95.0, true, true, true, 0.0, -5.5, 0.0},

        // ── Bank 02 · Room & Hall (6 presets) ──
        {"Dark Room", "02 · Room & Hall", 15.0, 1.4, 1.25, 0.45, 4500.0, 1.1,
         85.0, false, false, true, 0.0, -4.0, 0.0},
        {"EMT 250 Church", "02 · Room & Hall", 75.0, 5.0, 1.15, 0.70, 6800.0,
         1.3, 110.0, true, false, true, 0.0, -2.5, 0.0},
        {"EMT 250 Hall", "02 · Room & Hall", 40.0, 2.8, 1.00, 0.80, 8000.0, 1.4,
         100.0, true, true, true, 0.0, -3.0, 0.0},
        {"Scoring Stage", "02 · Room & Hall", 50.0, 3.4, 0.95, 0.85, 8800.0,
         1.5, 105.0, true, true, false, 0.0, -3.0, 0.0},
        {"Large Vintage Hall", "02 · Room & Hall", 65.0, 4.2, 1.05, 0.75,
         7200.0, 1.4, 120.0, true, true, true, 0.0, -2.5, 0.0},
        {"Small Vintage Room", "02 · Room & Hall", 10.0, 0.8, 0.90, 0.70,
         6000.0, 1.2, 75.0, false, true, true, 0.0, -4.5, 0.0},

        // ── Bank 03 · Strings & Piano (5 presets) ──
        {"EMT 250 String Hall", "03 · Strings & Piano", 45.0, 3.0, 1.10, 0.90,
         8200.0, 1.4, 130.0, true, true, true, 0.0, -3.0, 0.0},
        {"Super Strings", "03 · Strings & Piano", 55.0, 3.8, 1.15, 0.85, 9000.0,
         1.5, 145.0, true, false, true, 0.0, -2.5, 0.0},
        {"Warm Strings", "03 · Strings & Piano", 30.0, 2.5, 1.25, 0.65, 6200.0,
         1.3, 110.0, true, false, true, 0.0, -3.5, 0.0},
        {"Vintage Piano", "03 · Strings & Piano", 25.0, 2.1, 0.95, 0.80, 7800.0,
         1.4, 90.0, true, true, true, 0.0, -4.0, 0.0},
        {"Piano Space", "03 · Strings & Piano", 40.0, 2.7, 0.85, 0.90, 8500.0,
         1.5, 105.0, true, true, false, 0.0, -3.5, 0.0},

        // ── Bank 04 · Drums & Percussion (5 presets) ──
        {"EMT 250 Nice 4 Perc", "04 · Drums & Perc", 18.0, 1.5, 0.40, 1.10,
         9500.0, 1.6, 85.0, true, true, true, 0.0, -3.5, 0.0},
        {"EMT 250 Long Perc", "04 · Drums & Perc", 28.0, 2.4, 0.50, 0.95,
         8000.0, 1.4, 100.0, true, true, true, 0.0, -3.0, 0.0},
        {"Vintage Snare Room", "04 · Drums & Perc", 8.0, 1.1, 0.35, 1.20,
         10000.0, 1.7, 70.0, true, true, true, 0.0, -3.5, 0.0},
        {"Drum Ambience", "04 · Drums & Perc", 4.0, 0.7, 0.30, 0.85, 6500.0,
         1.3, 60.0, true, true, true, 0.0, -4.5, 0.0},
        {"Percussion Space", "04 · Drums & Perc", 22.0, 1.7, 0.45, 1.05, 8800.0,
         1.5, 95.0, true, true, false, 0.0, -3.5, 0.0},

        // ── Bank 05 · Special (4 presets) ──
        {"Vintage Plate Space", "05 · Special", 15.0, 2.5, 0.80, 1.15, 11000.0,
         1.8, 120.0, true, true, true, 0.0, -3.0, 0.0},
        {"Long Vintage Tail", "05 · Special", 80.0, 8.5, 1.10, 0.60, 6000.0,
         1.3, 140.0, true, false, true, 0.0, -2.0, 0.0},
        {"Modulated Space", "05 · Special", 50.0, 3.6, 0.90, 0.90, 8500.0, 1.4,
         190.0, true, true, false, 0.0, -2.5, 0.0},
        {"Deep Atmosphere", "05 · Special", 95.0, 6.2, 1.40, 0.50, 4800.0, 1.2,
         160.0, true, false, true, 0.0, -2.0, 0.0}};
    return presets;
  }

  void loadPreset(const Preset &p) noexcept {
    setPreDelay(p.preDelayMs);
    setDecay(p.decaySec);
    setLoDecay(p.loDecayMult);
    setHiDecay(p.hiDecayMult);
    setHiCut(p.hiCutHz);
    setHiQScale(p.hiQScale);
    setModulation(p.modulationPct);
    setInputTrans(p.inputTrans);
    setTrimLoFreq(p.trimLoFreq);
    setLoRes(p.loRes);
    setInLevel(p.inLevelDb);
    setWetLevel(p.wetLevelDb);
    if (!killDry_) {
      setDryLevel(p.dryLevelDb);
    }

    const auto &all = getFactoryPresets();
    for (size_t i = 0; i < all.size(); ++i) {
      if (std::strcmp(all[i].name, p.name) == 0) {
        currentPresetIndex_ = static_cast<int>(i);
        break;
      }
    }
  }

  void loadPresetByIndex(int idx) noexcept {
    const auto &all = getFactoryPresets();
    if (all.empty())
      return;
    const int clamped = std::clamp(idx, 0, static_cast<int>(all.size()) - 1);
    currentPresetIndex_ = clamped;
    loadPreset(all[static_cast<size_t>(clamped)]);
  }

  int getCurrentPresetIndex() const noexcept { return currentPresetIndex_; }

  Preset getCurrentPreset(const char *name = "Custom",
                          const char *bank = "User") const noexcept {
    return Preset{name,           bank,         preDelayMs_, decaySec_,
                  loDecayMult_,   hiDecayMult_, hiCutHz_,    hiQScale_,
                  modulationPct_, inputTrans_,  trimLoFreq_, loRes_,
                  inLevelDb_,     wetLevelDb_,  dryLevelDb_};
  }

  // ── 6. DSP Audio Processing ───────────────────────────────────────────
  void processStereo(double &inOutL, double &inOutR) noexcept {
    if (bypassed_ || !buffers_)
      return;

    auto &preDelayBuf = buffers_->preDelayBuf;
    auto &diffBufL = buffers_->diffBufL;
    auto &diffBufR = buffers_->diffBufR;
    auto &fdnBuf = buffers_->fdnBuf;

    const double dryL = sanitize(inOutL);
    const double dryR = sanitize(inOutR);

    // 1. Input Level Stage (-100 dB to +12 dB)
    const double inGain = (inLevelDb_ <= -90.0) ? 0.0 : dbToGain(inLevelDb_);
    double inL = dryL * inGain;
    double inR = dryR * inGain;

    const float inPeakL = static_cast<float>(std::abs(inL));
    const float inPeakR = static_cast<float>(std::abs(inR));
    if (inPeakL >= 1.0f || inPeakR >= 1.0f) {
      overloadIn.store(true, std::memory_order_relaxed);
    }

    float pInL = meterInL.load(std::memory_order_relaxed);
    meterInL.store(std::max(inPeakL, pInL * 0.9992f),
                   std::memory_order_relaxed);
    float pInR = meterInR.load(std::memory_order_relaxed);
    meterInR.store(std::max(inPeakR, pInR * 0.9992f),
                   std::memory_order_relaxed);

    // 2. Vintage EMT 250 Input Transformer Emulation
    if (inputTrans_) {
      // Subtle asymmetric 2nd-harmonic iron core warmth + HF inductance
      // smoothing
      const double asymL = inL + 0.065 * inL * inL - 0.065 * 0.15;
      const double asymR = inR + 0.065 * inR * inR - 0.065 * 0.15;
      inL = std::tanh(asymL * 1.18) * 0.90;
      inR = std::tanh(asymR * 1.18) * 0.90;
      transLpL_ += transLpCoeff_ * (inL - transLpL_);
      transLpR_ += transLpCoeff_ * (inR - transLpR_);
      inL = sanitize(transLpL_);
      inR = sanitize(transLpR_);
    }

    // 3. Vintage 75 Hz Low-Frequency Trim (2-Pole Butterworth HPF, DF-II
    // Transposed)
    if (trimLoFreq_) {
      inL = processTrimFilter(inL, trimZ1_L_, trimZ2_L_);
      inR = processTrimFilter(inR, trimZ1_R_, trimZ2_R_);
    }

    // 4. Pre-Delay Line (up to 500 ms)
    const double monoIn = 0.5 * (inL + inR);
    const double sideIn = 0.25 * (inL - inR);
    preDelayBuf[preDelayWr_] = sanitize(monoIn);

    const int preDelaySmp = std::clamp(
        static_cast<int>(std::round(preDelayMs_ * sampleRate_ * 0.001)), 0,
        maxPreDelaySamples_);
    const int preDelayRd =
        (preDelayWr_ - preDelaySmp + kMaxPreDelaySmp) % kMaxPreDelaySmp;
    double preSig = preDelayBuf[preDelayRd];
    preDelayWr_ = (preDelayWr_ + 1) % kMaxPreDelaySmp;

    // 5. EMT 250 Pre-Emphasis before ADC (when Lo Res 12-bit converter path is
    // active)
    if (loRes_) {
      const double hpIn = preSig - preEmpL_;
      preEmpL_ += preEmpCoeff_ * (preSig - preEmpL_);
      preSig = sanitize(preSig + 0.45 * hpIn);
      // 12-bit symmetric mid-tread ADC quantization
      preSig =
          std::round(std::clamp(preSig, -1.5, 1.5) * 2048.0) * (1.0 / 2048.0);
    }

    // 6. 4-Stage Series Schroeder/Blesser Input Allpass Diffusers + Early
    // Reflections
    double diffSigL = preSig + sideIn;
    double diffSigR = preSig - sideIn;
    const double apGain = 0.52 * std::clamp(densityPct_, 0.15, 1.0);
    double erL = 0.0;
    double erR = 0.0;

    for (int s = 0; s < kNumDiffusers; ++s) {
      const int wr = diffWr_[s];
      const int rd = (wr - diffLen_[s] + kDiffBufLen) & (kDiffBufLen - 1);

      const double delL = diffBufL[s][rd];
      const double vL = diffSigL - apGain * delL;
      diffBufL[s][wr] = sanitize(vL);
      diffSigL = sanitize(delL + apGain * vL);

      const double delR = diffBufR[s][rd];
      const double vR = diffSigR - (-apGain) * delR;
      diffBufR[s][wr] = sanitize(vR);
      diffSigR = sanitize(delR + (-apGain) * vR);

      diffWr_[s] = (wr + 1) & (kDiffBufLen - 1);

      // Accumulate early reflection taps from diffuser stages
      if (s == 1) {
        erL += 0.35 * delL;
        erR += 0.35 * delR;
      } else if (s == 3) {
        erL += 0.25 * delR;
        erR += 0.25 * delL;
      }
    }

    // 7. Read 8 FDN Tank Delay Lines with Irrational Prime LFO Chorus
    // Modulation
    double nodeOut[kFdnOrder];
    const double srScale = sampleRate_ / 48000.0;
    const double modDepthSmp = (modulationPct_ * 0.01) * 6.5 * srScale;

    for (int i = 0; i < kFdnOrder; ++i) {
      const double lfoFreq = kLfoFreqs[i];
      lfoPhase_[i] += (2.0 * std::numbers::pi * lfoFreq) / sampleRate_;
      if (lfoPhase_[i] >= 2.0 * std::numbers::pi) {
        lfoPhase_[i] -= 2.0 * std::numbers::pi;
      }

      const double modOffset = std::sin(lfoPhase_[i]) * modDepthSmp;
      const double exactRd =
          static_cast<double>(fdnWr_[i] - fdnLen_[i]) + modOffset;

      nodeOut[i] = sanitize(readHermite(fdnBuf[i], exactRd));

      // Vintage Lo-Res: symmetric mid-tread 12-bit quantization of circulating
      // tail
      if (loRes_) {
        nodeOut[i] = std::round(nodeOut[i] * 2048.0) * (1.0 / 2048.0);
      }
    }

    // 8. 8x8 Unitary Hadamard Feedback Mixing Matrix
    double mixed[kFdnOrder] = {};
    for (int i = 0; i < kFdnOrder; ++i) {
      double acc = 0.0;
      for (int j = 0; j < kFdnOrder; ++j) {
        acc += feedbackMatrix_[i][j] * nodeOut[j];
      }
      mixed[i] = acc;
    }

    // 9. True 3-Band Phase-Coherent Crossover Decay (Lo Decay, Mid Decay, Hi
    // Decay)
    for (int i = 0; i < kFdnOrder; ++i) {
      double v = mixed[i];

      // 3-band Linkwitz-Riley-style 1-pole phase-coherent split:
      // lowBand (< 300 Hz), midBand (300 Hz - 2800 Hz), highBand (> 2800 Hz)
      loState_[i] = sanitize(loState_[i] + loCoeff_ * (v - loState_[i]));
      hiState_[i] = sanitize(hiState_[i] + hiCoeff_ * (v - hiState_[i]));

      const double lowBand = loState_[i];
      const double midBand = hiState_[i] - loState_[i];
      const double highBand = v - hiState_[i];

      // Independent per-band RT60 loop attenuation
      v = (lowBand * loGain_[i]) + (midBand * fdnGain_[i]) +
          (highBand * hiGain_[i]);

      // Progressive HF loop damping at Hi Cut frequency
      dampState_[i] =
          sanitize(dampState_[i] + loopHiCutCoeff_ * (v - dampState_[i]));
      v = dampState_[i];

      // Gentle non-linear saturation inside tank to prevent overload & enrich
      // density
      if (drive_ > 1.001) {
        v = std::tanh(v * drive_) * invDrive_;
      }

      // First-order DC blocker (12 Hz) inside tank loop
      const double dcOut = v - dcX1_[i] + 0.9985 * dcY1_[i];
      dcX1_[i] = v;
      dcY1_[i] = sanitize(dcOut);
      v = dcY1_[i];

      // Inject stereo-diffused input into the 8 FDN nodes with alternating
      // polarity
      const double inj = ((i & 1) == 0 ? diffSigL : diffSigR) * inputGain_[i];
      fdnBuf[i][fdnWr_[i]] = sanitize(v + inj);
      fdnWr_[i] = (fdnWr_[i] + 1) & (kFdnBufLen - 1);
    }

    // 10. Orthogonal Stereo Tank Tap Summation + Early Reflections
    const double tankL =
        (nodeOut[0] - nodeOut[2] + nodeOut[4] - nodeOut[6]) * 0.42;
    const double tankR =
        (nodeOut[1] - nodeOut[3] + nodeOut[5] - nodeOut[7]) * 0.42;

    double wetL = tankL + erL;
    double wetR = tankR + erR;

    // 11. Resonant Hi Cut + Hi QScale Output Contour Filter (DF-II Transposed)
    wetL = processHiCut(wetL, hiCutZ1_L_, hiCutZ2_L_);
    wetR = processHiCut(wetR, hiCutZ1_R_, hiCutZ2_R_);

    // 12. EMT 250 De-Emphasis Filter when Lo-Res 12-bit mode is active
    if (loRes_) {
      deEmpL_ += preEmpCoeff_ * (wetL - deEmpL_);
      deEmpR_ += preEmpCoeff_ * (wetR - deEmpR_);
      wetL = sanitize(0.68 * wetL + 0.32 * deEmpL_);
      wetR = sanitize(0.68 * wetR + 0.32 * deEmpR_);
    }

    // 13. Mid/Side Stereo Width Control
    const double mid = 0.5 * (wetL + wetR);
    const double side = 0.5 * (wetL - wetR) * stereoWidth_;
    const double finalWetL = mid + side;
    const double finalWetR = mid - side;

    // 14. Output Level & Dry/Wet Mix (with Kill Dry support for Aux Sends)
    const double wetGain = (wetLevelDb_ <= -90.0) ? 0.0 : dbToGain(wetLevelDb_);
    const double dryGain =
        (killDry_ || dryLevelDb_ <= -90.0) ? 0.0 : dbToGain(dryLevelDb_);

    inOutL = sanitize(dryL * dryGain + finalWetL * wetGain);
    inOutR = sanitize(dryR * dryGain + finalWetR * wetGain);

    // 15. Output Telemetry & Overload Detection
    const float outPeakL = static_cast<float>(std::abs(inOutL));
    const float outPeakR = static_cast<float>(std::abs(inOutR));
    if (outPeakL >= 1.0f || outPeakR >= 1.0f) {
      overloadOut.store(true, std::memory_order_relaxed);
    }

    float pOutL = meterOutL.load(std::memory_order_relaxed);
    meterOutL.store(std::max(outPeakL, pOutL * 0.9992f),
                    std::memory_order_relaxed);
    float pOutR = meterOutR.load(std::memory_order_relaxed);
    meterOutR.store(std::max(outPeakR, pOutR * 0.9992f),
                    std::memory_order_relaxed);
  }

private:
  void buildHadamardMatrix() noexcept {
    const double norm = 1.0 / std::sqrt(static_cast<double>(kFdnOrder));
    for (int i = 0; i < kFdnOrder; ++i) {
      for (int j = 0; j < kFdnOrder; ++j) {
        unsigned int bits = static_cast<unsigned int>(i & j);
        int parity = 0;
        while (bits) {
          parity ^= 1;
          bits &= bits - 1u;
        }
        feedbackMatrix_[i][j] = (parity == 0) ? norm : -norm;
      }
    }
  }

  void recalcCoefficients() noexcept {
    const double sr = (sampleRate_ > 1000.0) ? sampleRate_ : 48000.0;

    // FDN loop gains per line for exact specified RT60 (-60 dB decay time)
    for (int i = 0; i < kFdnOrder; ++i) {
      const double lenSmp = static_cast<double>(fdnLen_[i]);
      // Mid-band reference decay gain
      const double decayClamp = std::clamp(decaySec_, 0.15, 20.0);
      fdnGain_[i] = std::clamp(
          std::pow(10.0, (-3.0 / decayClamp) * (lenSmp / sr)), 0.0, 0.9995);

      // Low-band decay gain (scaled by Lo Decay multiplier)
      const double loDecayTime =
          std::clamp(decaySec_ * loDecayMult_, 0.05, 40.0);
      loGain_[i] = std::clamp(
          std::pow(10.0, (-3.0 / loDecayTime) * (lenSmp / sr)), 0.0, 0.9996);

      // High-band decay gain (scaled by Hi Decay multiplier)
      const double hiDecayTime =
          std::clamp(decaySec_ * hiDecayMult_, 0.05, 35.0);
      hiGain_[i] = std::clamp(
          std::pow(10.0, (-3.0 / hiDecayTime) * (lenSmp / sr)), 0.0, 0.9995);

      inputGain_[i] = 1.0 / std::sqrt(static_cast<double>(kFdnOrder));
    }

    // 3-band crossover frequencies: Low = 300 Hz, High = 2800 Hz
    loCoeff_ = 1.0 - std::exp(-2.0 * std::numbers::pi * 300.0 / sr);
    hiCoeff_ = 1.0 - std::exp(-2.0 * std::numbers::pi * 2800.0 / sr);

    // Progressive 1-pole HF loop damping coefficient tracking Hi Cut
    const double loopCutHz = std::clamp(hiCutHz_ * 1.15, 1200.0, sr * 0.46);
    loopHiCutCoeff_ = 1.0 - std::exp(-2.0 * std::numbers::pi * loopCutHz / sr);

    // Transformer HF inductance smoothing (15.5 kHz) and converter
    // pre/de-emphasis (3.2 kHz)
    transLpCoeff_ = 1.0 - std::exp(-2.0 * std::numbers::pi *
                                   std::min(15500.0, sr * 0.45) / sr);
    preEmpCoeff_ = 1.0 - std::exp(-2.0 * std::numbers::pi * 3200.0 / sr);

    // Resonant Hi Cut Filter with Hi QScale (2nd-order lowpass, DF-II
    // Transposed)
    const double w0 =
        2.0 * std::numbers::pi * std::clamp(hiCutHz_, 1000.0, sr * 0.45) / sr;
    const double q = std::clamp(hiQScale_, 0.5, 2.5) * 0.70710678;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double cosw0 = std::cos(w0);

    const double a0 = 1.0 + alpha;
    b0_ = ((1.0 - cosw0) * 0.5) / a0;
    b1_ = (1.0 - cosw0) / a0;
    b2_ = ((1.0 - cosw0) * 0.5) / a0;
    a1_ = (-2.0 * cosw0) / a0;
    a2_ = (1.0 - alpha) / a0;

    // 75 Hz 2-pole Butterworth High-pass for Trim Lo Freq (DF-II Transposed)
    const double wTrim = 2.0 * std::numbers::pi * 75.0 / sr;
    const double alphaTrim = std::sin(wTrim) / (2.0 * 0.70710678);
    const double cosTrim = std::cos(wTrim);
    const double a0Trim = 1.0 + alphaTrim;
    trimB0_ = ((1.0 + cosTrim) * 0.5) / a0Trim;
    trimB1_ = (-(1.0 + cosTrim)) / a0Trim;
    trimB2_ = ((1.0 + cosTrim) * 0.5) / a0Trim;
    trimA1_ = (-2.0 * cosTrim) / a0Trim;
    trimA2_ = (1.0 - alphaTrim) / a0Trim;

    drive_ = dbToGain(nonLinearDriveDb_ * 0.35);
    invDrive_ =
        (drive_ > 1e-6) ? (1.0 / std::tanh(drive_)) * std::tanh(1.0) : 1.0;
  }

  double readHermite(const double *buf, double rdPos) noexcept {
    const int iPos = static_cast<int>(std::floor(rdPos));
    const double frac = rdPos - static_cast<double>(iPos);

    const int p0 = (iPos - 1 + kFdnBufLen * 4) & (kFdnBufLen - 1);
    const int p1 = (iPos + kFdnBufLen * 4) & (kFdnBufLen - 1);
    const int p2 = (iPos + 1 + kFdnBufLen * 4) & (kFdnBufLen - 1);
    const int p3 = (iPos + 2 + kFdnBufLen * 4) & (kFdnBufLen - 1);

    const double y0 = buf[p0];
    const double y1 = buf[p1];
    const double y2 = buf[p2];
    const double y3 = buf[p3];

    const double c0 = y1;
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
  }

  // Proper Direct Form II Transposed 2nd-order Biquad for Hi Cut + Hi QScale
  double processHiCut(double x, double &z1, double &z2) noexcept {
    const double y = b0_ * x + z1;
    z1 = sanitize(b1_ * x - a1_ * y + z2);
    z2 = sanitize(b2_ * x - a2_ * y);
    return sanitize(y);
  }

  // Proper Direct Form II Transposed 2nd-order Biquad for 75 Hz Trim Lo Freq
  // HPF
  double processTrimFilter(double x, double &z1, double &z2) noexcept {
    const double y = trimB0_ * x + z1;
    z1 = sanitize(trimB1_ * x - trimA1_ * y + z2);
    z2 = sanitize(trimB2_ * x - trimA2_ * y);
    return sanitize(y);
  }

  // ── Parameters ────────────────────────────────────────────────────────
  double sampleRate_ = 48000.0;
  double preDelayMs_ = 35.0;
  double decaySec_ = 2.2;
  double loDecayMult_ = 0.85;
  double hiDecayMult_ = 0.80;

  double hiCutHz_ = 8000.0;
  double hiQScale_ = 1.4;
  double modulationPct_ = 100.0;
  bool inputTrans_ = true;
  bool trimLoFreq_ = true;
  bool loRes_ = true;

  double inLevelDb_ = 0.0;
  double wetLevelDb_ = -3.5;
  double dryLevelDb_ = 0.0;
  bool killDry_ = false;
  double densityPct_ = 0.82;
  double nonLinearDriveDb_ = 3.0;
  double stereoWidth_ = 1.0;
  bool bypassed_ = false;
  int currentPresetIndex_ = 0;

  // ── Derived Coefficients ──────────────────────────────────────────────
  double fdnGain_[kFdnOrder] = {};
  double loGain_[kFdnOrder] = {};
  double hiGain_[kFdnOrder] = {};
  double inputGain_[kFdnOrder] = {};
  double loCoeff_ = 0.0;
  double hiCoeff_ = 0.0;
  double loopHiCutCoeff_ = 0.5;
  double transLpCoeff_ = 0.8;
  double preEmpCoeff_ = 0.3;

  double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
  double trimB0_ = 1.0, trimB1_ = 0.0, trimB2_ = 0.0, trimA1_ = 0.0,
         trimA2_ = 0.0;
  double drive_ = 1.0, invDrive_ = 1.0;

  double feedbackMatrix_[kFdnOrder][kFdnOrder] = {};

  // ── State Buffers (Pre-allocated once on construction to keep sizeof tiny)
  // ──
  struct DelayBuffers {
    double preDelayBuf[kMaxPreDelaySmp]{};
    double diffBufL[kNumDiffusers][kDiffBufLen]{};
    double diffBufR[kNumDiffusers][kDiffBufLen]{};
    double fdnBuf[kFdnOrder][kFdnBufLen]{};
  };
  std::unique_ptr<DelayBuffers> buffers_ = std::make_unique<DelayBuffers>();

  int preDelayWr_ = 0;
  int maxPreDelaySamples_ = kMaxPreDelaySmp - 1;

  int diffWr_[kNumDiffusers] = {};
  int diffLen_[kNumDiffusers] = {149, 211, 337, 491};

  int fdnWr_[kFdnOrder] = {};
  int fdnLen_[kFdnOrder] = {1481, 1867, 2053, 2399, 2707, 3001, 3313, 3547};

  double loState_[kFdnOrder] = {};
  double hiState_[kFdnOrder] = {};
  double dampState_[kFdnOrder] = {};
  double dcX1_[kFdnOrder] = {};
  double dcY1_[kFdnOrder] = {};
  double lfoPhase_[kFdnOrder] = {};

  double trimZ1_L_ = 0.0, trimZ2_L_ = 0.0;
  double trimZ1_R_ = 0.0, trimZ2_R_ = 0.0;
  double hiCutZ1_L_ = 0.0, hiCutZ2_L_ = 0.0;
  double hiCutZ1_R_ = 0.0, hiCutZ2_R_ = 0.0;
  double transLpL_ = 0.0, transLpR_ = 0.0;
  double preEmpL_ = 0.0, preEmpR_ = 0.0;
  double deEmpL_ = 0.0, deEmpR_ = 0.0;

  // ── Atomic Telemetry ──────────────────────────────────────────────────
  std::atomic<float> meterInL{0.0f};
  std::atomic<float> meterInR{0.0f};
  std::atomic<float> meterOutL{0.0f};
  std::atomic<float> meterOutR{0.0f};
  std::atomic<bool> overloadIn{false};
  std::atomic<bool> overloadOut{false};
};

} // namespace nova::graviton
