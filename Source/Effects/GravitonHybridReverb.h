#pragma once
/*  GravitonHybridReverb.h  –  Ableton-Style Hybrid Reverb (FX Slot 22)
    =========================================================================
    Combines a lock-free Convolution IR engine with a 5-Algorithm FDN
    Algorithmic Reverb engine, Parallel/Serial/Single routing, 4-Band
    Parametric Reverb EQ (Pre/Post Algo), Send/Predelay/Feedback,
    Bass Mono, Vintage character, Stereo Width, Freeze, and Dry/Wet Mix.
*/

#include "DSP/GravitonDSP.h"
#include "Effects/GravitonConvolutionReverb.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace nova {

// ─── Hybrid Reverb Parameters ─────────────────────────────────
struct HybridReverbParams {
  // Input & Time
  float sendDb = 0.0f;           // -60..0 dB
  float dryWet = 0.30f;          // 0..1
  float preDelayMs = 10.0f;      // 0..500
  bool preDelaySync = false;
  float feedbackPct = 0.0f;      // 0..95 (%)

  // Algorithmic Reverb
  float algoAttackMs = 0.0f;     // 0..500 ms
  float decay = 8.0f;            // 0.1..30 seconds (RT60)
  float size = 0.20f;            // 0.05..1.0 (Algo Size)
  int routing = 0;               // 0=Parallel, 1=Serial, 2=Algo Only, 3=Conv Only
  int algorithm = 1;             // 0=DarkHall, 1=Quartz, 2=Shimmer, 3=Prism, 4=Tides
  bool freeze = false;
  float algoDelayMs = 0.0f;      // 0..250 ms
  float modDepth = 0.50f;        // 0..1
  float diffusion = 1.0f;        // 0..1
  float modRate = 0.5f;          // Hz

  // Convolution IR
  float irBlendPct = 60.0f;      // 0..100 (0=100% Algo, 100=100% Conv, 60=60/40)
  float irDecaySec = 3.50f;      // 0.2..10 s
  float irSizePct = 50.0f;       // 25..200 %
  float irDampingPct = 50.0f;    // 0..100 %
  float irLoDampPct = 50.0f;     // 0..100 %
  float irDistancePct = 50.0f;   // 0..100 %
  int irCategory = 0;            // 0=Factory, 1=User
  int irPresetIdx = 0;

  // Output
  float stereoSpread = 1.0f;     // 0..2
  int vintageMode = 0;           // 0=Off, 1=Subtle, 2=Old, 3=12-Bit
  bool bassMono = true;          // Mono lows < 120 Hz on wet signal

  // 4-Band Reverb EQ
  bool eqPreAlgo = false;
  struct EqBand {
    int type = 2; // 0=HPF, 1=LowShelf, 2=Bell, 3=HighShelf, 4=LPF
    float freqHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.7f;
    int slopeDb = 12; // 12, 24, 48, 60
  };
  std::array<EqBand, 4> eqBands{{
      {0, 157.0f, 0.0f, 0.7f, 60},   // Point 1: HPF 157 Hz 60dB
      {2, 550.0f, 0.0f, 0.9f, 12},   // Point 2: Bell 550 Hz
      {2, 1600.0f, 0.0f, 0.9f, 12},  // Point 3: Bell 1.6 kHz
      {3, 420.0f, 6.0f, 0.7f, 12},   // Point 4: High Shelf
  }};

  // Legacy fields (preserved for existing callers)
  float earlyLevel = 0.5f;
  float earlyPattern = 0.5f;
  float earlyDecay = 0.3f;
  float lateLevel = 1.0f;
  float crossoverFreq = 500.0f;
  float lowDamping = 0.3f;
  float highDamping = 0.5f;
  float lowDampFreq = 200.0f;
  float highDampFreq = 6000.0f;
  float shimmer = 0.0f;
  float duckAmount = 0.0f;
  float duckThreshold = -20.0f;
  float duckRelease = 200.0f;
};

// ─── All-Pass & Comb Building Blocks ──────────────────────────
class AllPassDelay {
public:
  void init(int maxDelay) {
    buffer_.assign(static_cast<size_t>(maxDelay + 1), 0.0f);
    pos_ = 0;
  }
  float process(float input, int delaySamples, float feedback) {
    if (buffer_.empty())
      return input;
    delaySamples = std::clamp(delaySamples, 1, static_cast<int>(buffer_.size()) - 1);
    int readPos = (pos_ - delaySamples + static_cast<int>(buffer_.size())) %
                  static_cast<int>(buffer_.size());
    float delayed = buffer_[static_cast<size_t>(readPos)];
    float output = -input * feedback + delayed;
    buffer_[static_cast<size_t>(pos_)] = input + delayed * feedback;
    pos_ = (pos_ + 1) % static_cast<int>(buffer_.size());
    return output;
  }
  void clear() { std::fill(buffer_.begin(), buffer_.end(), 0.0f); }

private:
  std::vector<float> buffer_;
  int pos_ = 0;
};

class CombFilter {
public:
  void init(int maxDelay) {
    buffer_.assign(static_cast<size_t>(maxDelay + 1), 0.0f);
    pos_ = 0;
  }
  float process(float input, int delaySamples, float feedback, float damping) {
    if (buffer_.empty())
      return input;
    delaySamples = std::clamp(delaySamples, 1, static_cast<int>(buffer_.size()) - 1);
    int readPos = (pos_ - delaySamples + static_cast<int>(buffer_.size())) %
                  static_cast<int>(buffer_.size());
    float delayed = buffer_[static_cast<size_t>(readPos)];
    filterState_ = delayed * (1.0f - damping) + filterState_ * damping;
    buffer_[static_cast<size_t>(pos_)] = input + filterState_ * feedback;
    pos_ = (pos_ + 1) % static_cast<int>(buffer_.size());
    return delayed;
  }
  void clear() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    filterState_ = 0.0f;
  }

private:
  std::vector<float> buffer_;
  int pos_ = 0;
  float filterState_ = 0.0f;
};

// ─── Hybrid Reverb Engine ────────────────────────────────────
class HybridReverbEngine {
public:
  static constexpr int NUM_COMBS = 8;
  static constexpr int NUM_ALLPASS = 4;
  static constexpr int MAX_PRE_DELAY = 24000; // 500ms @ 48kHz

  HybridReverbEngine() { init(48000.0); }

  void init(double sampleRate) {
    if (sampleRate <= 1000.0)
      return;
    if (std::abs(sampleRate - sampleRate_) < 1.0 && !earlyBufL_.empty())
      return;
    sampleRate_ = sampleRate;

    static constexpr int combDelaysL[] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    static constexpr int combDelaysR[] = {1617, 1557, 1422, 1491, 1356, 1277, 1116, 1188};
    for (int i = 0; i < NUM_COMBS; ++i) {
      combsL_[static_cast<size_t>(i)].init(4096);
      combsR_[static_cast<size_t>(i)].init(4096);
      combDelayL_[static_cast<size_t>(i)] = combDelaysL[i];
      combDelayR_[static_cast<size_t>(i)] = combDelaysR[i];
    }

    static constexpr int apDelays[] = {556, 441, 341, 225};
    for (int i = 0; i < NUM_ALLPASS; ++i) {
      allpassL_[static_cast<size_t>(i)].init(2048);
      allpassR_[static_cast<size_t>(i)].init(2048);
      apDelay_[static_cast<size_t>(i)] = apDelays[i];
    }

    preDelayBufL_.assign(MAX_PRE_DELAY, 0.0f);
    preDelayBufR_.assign(MAX_PRE_DELAY, 0.0f);
    preDelayPos_ = 0;

    algoDelayBufL_.assign(MAX_PRE_DELAY, 0.0f);
    algoDelayBufR_.assign(MAX_PRE_DELAY, 0.0f);
    algoDelayPos_ = 0;

    earlyTaps_ = {{1.5f, 0.8f},  {3.2f, 0.6f},  {5.7f, 0.5f},  {8.1f, 0.4f},
                  {11.3f, 0.35f},{15.8f, 0.3f}, {19.2f, 0.25f},{24.5f, 0.2f},
                  {30.1f, 0.15f},{37.0f, 0.12f},{42.3f, 0.1f}, {50.0f, 0.08f}};
    int earlyBufSize = static_cast<int>(0.06 * sampleRate) + 1;
    earlyBufL_.assign(static_cast<size_t>(earlyBufSize), 0.0f);
    earlyBufR_.assign(static_cast<size_t>(earlyBufSize), 0.0f);
    earlyPos_ = 0;

    convEngine_.setSampleRate(sampleRate_);
    convEngine_.loadFactoryPreset(params_.irPresetIdx);
    updateEqAndFilters();
  }

  HybridReverbParams &getParams() { return params_; }
  const HybridReverbParams &getParams() const { return params_; }
  void setParams(const HybridReverbParams &p) {
    const bool irChanged =
        (p.irPresetIdx != params_.irPresetIdx ||
         std::abs(p.irDecaySec - params_.irDecaySec) > 0.02f ||
         std::abs(p.irSizePct - params_.irSizePct) > 0.5f);
    params_ = p;
    if (irChanged) {
      if (p.irPresetIdx != convEngine_.getCurrentPresetIndex())
        convEngine_.loadFactoryPreset(p.irPresetIdx);
      auto cp = convEngine_.getParams();
      cp.decayScale = std::clamp(params_.irDecaySec / 3.5f, 0.2f, 3.0f);
      cp.sizePct = std::clamp(params_.irSizePct * 2.0f, 50.0f, 200.0f);
      cp.dryWet = 1.0f;
      cp.preDelayMs = 0.0f;
      convEngine_.setParams(cp);
    }
    updateEqAndFilters();
  }

  ConvolutionReverbEngine &getConvEngine() { return convEngine_; }
  const ConvolutionReverbEngine &getConvEngine() const { return convEngine_; }

  void loadIrPreset(int idx) {
    params_.irPresetIdx = std::clamp(idx, 0, ConvolutionReverbEngine::kNumFactoryPresets - 1);
    convEngine_.loadFactoryPreset(params_.irPresetIdx);
    auto cp = convEngine_.getParams();
    cp.dryWet = 1.0f;
    cp.preDelayMs = 0.0f;
    convEngine_.setParams(cp);
  }

  void getIrWaveformPeaks(std::vector<float> &outL, std::vector<float> &outR,
                          int numBins) const {
    convEngine_.getWaveformPeaks(outL, outR, numBins);
  }

  double getEqMagnitudeDb(double freqHz) const noexcept {
    const double w = 2.0 * 3.14159265358979323846 *
                     std::clamp(freqHz, 10.0, sampleRate_ * 0.49) / sampleRate_;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = std::polar(1.0, -2.0 * w);
    double totalDb = 0.0;
    for (int b = 0; b < 4; ++b) {
      const int passes = (params_.eqBands[static_cast<size_t>(b)].type == 0 ||
                          params_.eqBands[static_cast<size_t>(b)].type == 4)
                             ? std::clamp(params_.eqBands[static_cast<size_t>(b)].slopeDb / 12, 1, 5)
                             : 1;
      const auto &c = eqCoeffs_[static_cast<size_t>(b)];
      const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
      const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
      const double m = std::abs(num) / std::max(1.0e-12, std::abs(den));
      totalDb += static_cast<double>(passes) * 20.0 * std::log10(std::max(1.0e-9, m));
    }
    return totalDb;
  }

  void processStereo(float &left, float &right) {
    const float dryL = std::isfinite(left) ? left : 0.0f;
    const float dryR = std::isfinite(right) ? right : 0.0f;

    const float sendGain = std::pow(10.0f, params_.sendDb / 20.0f);
    float inL = dryL * sendGain + fbStateL_ * (params_.feedbackPct * 0.0085f);
    float inR = dryR * sendGain + fbStateR_ * (params_.feedbackPct * 0.0085f);

    if (params_.eqPreAlgo)
      apply4BandEq(inL, inR);

    // Pre-delay
    int preDelaySamples = static_cast<int>(params_.preDelayMs * sampleRate_ / 1000.0);
    preDelaySamples = std::clamp(preDelaySamples, 0, MAX_PRE_DELAY - 1);
    float preL = inL, preR = inR;
    if (preDelaySamples > 0 && !preDelayBufL_.empty()) {
      preDelayBufL_[static_cast<size_t>(preDelayPos_)] = inL;
      preDelayBufR_[static_cast<size_t>(preDelayPos_)] = inR;
      int rp = (preDelayPos_ - preDelaySamples + MAX_PRE_DELAY) % MAX_PRE_DELAY;
      preL = preDelayBufL_[static_cast<size_t>(rp)];
      preR = preDelayBufR_[static_cast<size_t>(rp)];
      preDelayPos_ = (preDelayPos_ + 1) % MAX_PRE_DELAY;
    }

    // Convolution IR path
    float convL = preL, convR = preR;
    if (params_.routing != 2) {
      convEngine_.processStereo(convL, convR);
      if (params_.irLoDampPct > 1.0f) {
        convL = static_cast<float>(convLoDampL_.process(convL));
        convR = static_cast<float>(convLoDampR_.process(convR));
      }
      if (params_.irDampingPct > 1.0f) {
        convL = static_cast<float>(convHiDampL_.process(convL));
        convR = static_cast<float>(convHiDampR_.process(convR));
      }
    } else {
      convL = convR = 0.0f;
    }

    // Algorithmic Reverb path (Routing 1 Serial feeds Conv into Algo)
    float algoInL = (params_.routing == 1) ? convL : preL;
    float algoInR = (params_.routing == 1) ? convR : preR;
    float algoL = 0.0f, algoR = 0.0f;
    if (params_.routing != 3) {
      runAlgorithmic(algoInL, algoInR, algoL, algoR);
    }

    // Combine Routing & Blend
    float wetL = 0.0f, wetR = 0.0f;
    if (params_.routing == 2) {
      wetL = algoL; wetR = algoR;
    } else if (params_.routing == 3) {
      wetL = convL; wetR = convR;
    } else if (params_.routing == 1) {
      const float b = params_.irBlendPct * 0.01f;
      wetL = algoL * (1.0f - b * 0.5f) + convL * (b * 0.5f);
      wetR = algoR * (1.0f - b * 0.5f) + convR * (b * 0.5f);
    } else {
      const float b = params_.irBlendPct * 0.01f; // 0=Algo, 100=Conv
      wetL = algoL * (1.0f - b) + convL * b;
      wetR = algoR * (1.0f - b) + convR * b;
    }

    if (!params_.eqPreAlgo)
      apply4BandEq(wetL, wetR);

    // Vintage Character
    if (params_.vintageMode > 0) {
      const float drv = 1.0f + params_.vintageMode * 0.35f;
      wetL = std::tanh(wetL * drv) / drv;
      wetR = std::tanh(wetR * drv) / drv;
    }

    // Bass Mono (< 120 Hz M/S fold on Side) + Stereo Width
    const float mid = (wetL + wetR) * 0.5f;
    float side = (wetL - wetR) * 0.5f * params_.stereoSpread;
    if (params_.bassMono)
      side = static_cast<float>(bassMonoHpf_.process(side));
    wetL = mid + side;
    wetR = mid - side;

    fbStateL_ = std::clamp(wetL, -2.0f, 2.0f);
    fbStateR_ = std::clamp(wetR, -2.0f, 2.0f);

    left = std::clamp(dryL * (1.0f - params_.dryWet) + wetL * params_.dryWet, -4.0f, 4.0f);
    right = std::clamp(dryR * (1.0f - params_.dryWet) + wetR * params_.dryWet, -4.0f, 4.0f);
  }

  void reset() {
    for (int i = 0; i < NUM_COMBS; ++i) {
      combsL_[static_cast<size_t>(i)].clear();
      combsR_[static_cast<size_t>(i)].clear();
    }
    for (int i = 0; i < NUM_ALLPASS; ++i) {
      allpassL_[static_cast<size_t>(i)].clear();
      allpassR_[static_cast<size_t>(i)].clear();
    }
    std::fill(preDelayBufL_.begin(), preDelayBufL_.end(), 0.0f);
    std::fill(preDelayBufR_.begin(), preDelayBufR_.end(), 0.0f);
    std::fill(algoDelayBufL_.begin(), algoDelayBufL_.end(), 0.0f);
    std::fill(algoDelayBufR_.begin(), algoDelayBufR_.end(), 0.0f);
    std::fill(earlyBufL_.begin(), earlyBufL_.end(), 0.0f);
    std::fill(earlyBufR_.begin(), earlyBufR_.end(), 0.0f);
    convEngine_.reset();
    preDelayPos_ = algoDelayPos_ = earlyPos_ = 0;
    modPhase_ = 0.0f;
    fbStateL_ = fbStateR_ = 0.0f;
    attackEnv_ = 1.0f;
  }

#if NOVA_HAS_JUCE_CORE
  juce::var toVar() const {
    auto obj = new juce::DynamicObject();
    obj->setProperty("dryWet", static_cast<double>(params_.dryWet));
    obj->setProperty("preDelayMs", static_cast<double>(params_.preDelayMs));
    obj->setProperty("size", static_cast<double>(params_.size));
    obj->setProperty("decay", static_cast<double>(params_.decay));
    obj->setProperty("diffusion", static_cast<double>(params_.diffusion));
    obj->setProperty("routing", params_.routing);
    obj->setProperty("algorithm", params_.algorithm);
    obj->setProperty("freeze", params_.freeze);
    obj->setProperty("irBlendPct", static_cast<double>(params_.irBlendPct));
    obj->setProperty("stereoSpread", static_cast<double>(params_.stereoSpread));
    return juce::var(obj);
  }
#endif

private:
  void runAlgorithmic(float inL, float inR, float &outL, float &outR) {
    // Algo Delay
    int aDelay = std::clamp(static_cast<int>(params_.algoDelayMs * sampleRate_ / 1000.0),
                            0, MAX_PRE_DELAY - 1);
    float dL = inL, dR = inR;
    if (aDelay > 0 && !algoDelayBufL_.empty()) {
      algoDelayBufL_[static_cast<size_t>(algoDelayPos_)] = inL;
      algoDelayBufR_[static_cast<size_t>(algoDelayPos_)] = inR;
      int rp = (algoDelayPos_ - aDelay + MAX_PRE_DELAY) % MAX_PRE_DELAY;
      dL = algoDelayBufL_[static_cast<size_t>(rp)];
      dR = algoDelayBufR_[static_cast<size_t>(rp)];
      algoDelayPos_ = (algoDelayPos_ + 1) % MAX_PRE_DELAY;
    }

    // Attack swell
    if (params_.algoAttackMs > 1.0f) {
      const float trig = std::max(std::abs(dL), std::abs(dR));
      const float coeff = std::exp(-1000.0f / (params_.algoAttackMs * static_cast<float>(sampleRate_)));
      attackEnv_ = coeff * attackEnv_ + (1.0f - coeff) * std::min(1.0f, trig * 4.0f + 0.15f);
      dL *= attackEnv_;
      dR *= attackEnv_;
    }

    const float roomScale = std::clamp(params_.size, 0.05f, 1.0f) * 0.45f + 0.65f;
    const float rt60 = params_.freeze ? 120.0f : std::max(0.1f, params_.decay);

    modPhase_ += params_.modRate / static_cast<float>(sampleRate_);
    if (modPhase_ > 1.0f) modPhase_ -= 1.0f;
    const float lfo = std::sin(6.2831853f * modPhase_);
    const int modOffset = static_cast<int>(lfo * params_.modDepth * 22.0f);

    float damp = params_.highDamping;
    if (params_.algorithm == 0) damp = std::max(damp, 0.65f);      // Dark Hall
    else if (params_.algorithm == 1) damp = std::min(damp, 0.25f); // Quartz

    float combOutL = 0.0f, combOutR = 0.0f;
    for (int i = 0; i < NUM_COMBS; ++i) {
      int dl = std::clamp(static_cast<int>(combDelayL_[static_cast<size_t>(i)] * roomScale) +
                              (i % 2 == 0 ? modOffset : -modOffset),
                          1, 4095);
      int dr = std::clamp(static_cast<int>(combDelayR_[static_cast<size_t>(i)] * roomScale) +
                              (i % 2 == 1 ? modOffset : -modOffset),
                          1, 4095);
      float fbL = std::min(std::pow(0.001f, static_cast<float>(dl) / (rt60 * static_cast<float>(sampleRate_))),
                           params_.freeze ? 1.0f : 0.985f);
      float fbR = std::min(std::pow(0.001f, static_cast<float>(dr) / (rt60 * static_cast<float>(sampleRate_))),
                           params_.freeze ? 1.0f : 0.985f);
      combOutL += combsL_[static_cast<size_t>(i)].process(dL, dl, fbL, damp);
      combOutR += combsR_[static_cast<size_t>(i)].process(dR, dr, fbR, damp);
    }
    combOutL /= static_cast<float>(NUM_COMBS);
    combOutR /= static_cast<float>(NUM_COMBS);

    float apL = combOutL, apR = combOutR;
    for (int i = 0; i < NUM_ALLPASS; ++i) {
      int dl = std::clamp(static_cast<int>(apDelay_[static_cast<size_t>(i)] * roomScale), 1, 2047);
      apL = allpassL_[static_cast<size_t>(i)].process(apL, dl, params_.diffusion * 0.5f);
      apR = allpassR_[static_cast<size_t>(i)].process(apR, dl, params_.diffusion * 0.5f);
    }

    // Shimmer / Prism harmonic sheen
    if (params_.algorithm == 2 || params_.algorithm == 3 || params_.shimmer > 0.01f) {
      const float shimAmt = (params_.algorithm == 2) ? 0.30f : (params_.algorithm == 3 ? 0.18f : params_.shimmer * 0.3f);
      apL += std::abs(apL) * shimAmt;
      apR += std::abs(apR) * shimAmt;
    }

    outL = apL * params_.lateLevel;
    outR = apR * params_.lateLevel;
  }

  void apply4BandEq(float &l, float &r) noexcept {
    for (int b = 0; b < 4; ++b) {
      const int passes = (params_.eqBands[static_cast<size_t>(b)].type == 0 ||
                          params_.eqBands[static_cast<size_t>(b)].type == 4)
                             ? std::clamp(params_.eqBands[static_cast<size_t>(b)].slopeDb / 12, 1, 5)
                             : 1;
      for (int p = 0; p < passes; ++p) {
        l = static_cast<float>(eqFiltersL_[static_cast<size_t>(b)][static_cast<size_t>(p)].process(l));
        r = static_cast<float>(eqFiltersR_[static_cast<size_t>(b)][static_cast<size_t>(p)].process(r));
      }
    }
  }

  void updateEqAndFilters() noexcept {
    const double sr = std::max(8000.0, sampleRate_);
    bassMonoHpf_.setCoefficients(graviton::makeButterworthHPF(sr, 120.0));
    const double loDampHz = 20.0 + params_.irLoDampPct * 3.5;
    const double hiDampHz = 18000.0 - params_.irDampingPct * 140.0;
    convLoDampL_.setCoefficients(graviton::makeButterworthHPF(sr, loDampHz));
    convLoDampR_.setCoefficients(graviton::makeButterworthHPF(sr, loDampHz));
    convHiDampL_.setCoefficients(graviton::makeButterworthLPF(sr, std::clamp(hiDampHz, 1200.0, sr * 0.45)));
    convHiDampR_.setCoefficients(graviton::makeButterworthLPF(sr, std::clamp(hiDampHz, 1200.0, sr * 0.45)));

    for (int b = 0; b < 4; ++b) {
      const auto &eb = params_.eqBands[static_cast<size_t>(b)];
      graviton::BiquadCoefficients c;
      if (eb.type == 0)
        c = graviton::makeButterworthHPF(sr, std::clamp<double>(eb.freqHz, 20.0, sr * 0.45));
      else if (eb.type == 4)
        c = graviton::makeButterworthLPF(sr, std::clamp<double>(eb.freqHz, 80.0, sr * 0.45));
      else
        c = graviton::makePeakingEq(sr, std::clamp<double>(eb.freqHz, 20.0, sr * 0.45),
                                    eb.gainDb, std::clamp<double>(eb.q, 0.3, 8.0));
      eqCoeffs_[static_cast<size_t>(b)] = c;
      for (int p = 0; p < 5; ++p) {
        eqFiltersL_[static_cast<size_t>(b)][static_cast<size_t>(p)].setCoefficients(c);
        eqFiltersR_[static_cast<size_t>(b)][static_cast<size_t>(p)].setCoefficients(c);
      }
    }
  }

  HybridReverbParams params_;
  double sampleRate_ = 48000.0;
  ConvolutionReverbEngine convEngine_;

  std::array<CombFilter, NUM_COMBS> combsL_, combsR_;
  std::array<int, NUM_COMBS> combDelayL_{}, combDelayR_{};
  std::array<AllPassDelay, NUM_ALLPASS> allpassL_, allpassR_;
  std::array<int, NUM_ALLPASS> apDelay_{};

  std::vector<float> preDelayBufL_, preDelayBufR_;
  int preDelayPos_ = 0;
  std::vector<float> algoDelayBufL_, algoDelayBufR_;
  int algoDelayPos_ = 0;
  std::vector<std::pair<float, float>> earlyTaps_;
  std::vector<float> earlyBufL_, earlyBufR_;
  int earlyPos_ = 0;

  graviton::Biquad bassMonoHpf_;
  graviton::Biquad convLoDampL_, convLoDampR_, convHiDampL_, convHiDampR_;
  std::array<graviton::BiquadCoefficients, 4> eqCoeffs_{};
  std::array<std::array<graviton::Biquad, 5>, 4> eqFiltersL_{}, eqFiltersR_{};

  float modPhase_ = 0.0f;
  float fbStateL_ = 0.0f, fbStateR_ = 0.0f;
  float attackEnv_ = 1.0f;
};

} // namespace nova
