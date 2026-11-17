#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace nova::graviton {

// ==============================================================================
// NOVA HD — Optical Stereo Phaser (12-Stage Analogue Optical Emulation)
// Inspired by Benidub PHASE 12-Stage Optical Stereo Phaser Architecture
// - Per-sample smoothing for Rate, Depth, Centre Frequency & Feedback
// - Per-stage all-pass filter coefficient interpolation (zero zipper noise)
// - Photoresistor (LDR/Vactrol) nonlinear response curve & asymmetric lag
// - Soft-limited + DC-blocked feedback network (no runaway self-oscillation)
// - Click-free crossfade transitions for Mono<->Stereo, Insert<->AUX & Bypass
// ==============================================================================
class OpticalPhaserEngine final {
public:
  static constexpr size_t kNumStages = 12;
  static constexpr size_t kHalfStages = 6;
  static constexpr size_t kNumPresets = 12;

  enum class StageMode { Mono12Stage = 0, Stereo6Plus6 = 1 };
  enum class LfoMode { Linear = 0, Mirrored = 1 };
  enum class RoutingMode { Insert50_50 = 0, AuxFullWet = 1 };

  struct Preset {
    const char *name;
    const char *category;
    const char *instrumentTarget;
    bool isFlagship;
    double depthPct;        // 0 .. 100
    double centerPct;       // 0 .. 100
    double ratePct;         // 0 .. 100
    double feedbackPct;     // 0 .. 100
    StageMode stageMode;
    LfoMode lfoMode;
    RoutingMode routingMode;
  };

  OpticalPhaserEngine() noexcept { reset(); }

  void reset() noexcept {
    monoStageState.fill(0.0);
    monoStagePrevIn.fill(0.0);
    stereoStageState.fill(0.0);
    stereoStagePrevIn.fill(0.0);
    smoothedCoeffMono.fill(0.85);
    smoothedCoeffStereo.fill(0.85);

    fbMono = 0.0;
    fbStereoL = 0.0;
    fbStereoR = 0.0;
    fbDcStateMono = 0.0;
    fbDcStateL = 0.0;
    fbDcStateR = 0.0;

    lfoPhase = 0.0;
    optEnvA = 0.5;
    optEnvB = 0.5;

    // Snap smoothers to current target values on hard reset
    smoothDepth = depthPct;
    smoothCenter = centerPct;
    smoothRate = ratePct;
    smoothFeedback = feedbackPct;
    smoothStereoMix = (stageMode == StageMode::Stereo6Plus6) ? 1.0 : 0.0;
    smoothAuxMix = (routingMode == RoutingMode::AuxFullWet) ? 1.0 : 0.0;
    smoothActiveGain = bypassed ? 0.0 : 1.0;

    telemetryLfoA.store(0.0f, std::memory_order_relaxed);
    telemetryLfoB.store(0.0f, std::memory_order_relaxed);
    telemetryFreqA.store(static_cast<float>(getCenterHz()),
                         std::memory_order_relaxed);
    telemetryFreqB.store(static_cast<float>(getCenterHz()),
                         std::memory_order_relaxed);
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0)
      cachedSampleRate = sr;
  }
  double getSampleRate() const noexcept { return cachedSampleRate; }

  // --- Primary Normalized Controls (0 .. 100 %) ---
  void setDepthPct(double pct) noexcept {
    depthPct = std::clamp(pct, 0.0, 100.0);
  }
  double getDepthPct() const noexcept { return depthPct; }

  void setCenterPct(double pct) noexcept {
    centerPct = std::clamp(pct, 0.0, 100.0);
  }
  double getCenterPct() const noexcept { return centerPct; }

  void setRatePct(double pct) noexcept {
    ratePct = std::clamp(pct, 0.0, 100.0);
  }
  double getRatePct() const noexcept { return ratePct; }

  void setFeedbackPct(double pct) noexcept {
    feedbackPct = std::clamp(pct, 0.0, 100.0);
  }
  double getFeedbackPct() const noexcept { return feedbackPct; }

  // --- Physical Unit Helpers & Backward-Compatible Wrappers ---
  static double centerPctToHz(double pct) noexcept {
    return 120.0 * std::pow(4200.0 / 120.0, std::clamp(pct, 0.0, 100.0) * 0.01);
  }

  static double ratePctToHz(double pct) noexcept {
    return 0.04 * std::pow(12.0 / 0.04, std::clamp(pct, 0.0, 100.0) * 0.01);
  }

  double getCenterHz() const noexcept { return centerPctToHz(centerPct); }
  double getRateHz() const noexcept { return ratePctToHz(ratePct); }

  void setRateHz(double hz) noexcept {
    const double clamped = std::clamp(hz, 0.04, 12.0);
    ratePct = std::clamp(
        100.0 * std::log(clamped / 0.04) / std::log(12.0 / 0.04), 0.0, 100.0);
  }

  // Backward-compatible mix wrapper (Insert=0.50, AUX=1.00)
  void setMix(double newMix) noexcept {
    const double m = std::clamp(newMix, 0.0, 1.0);
    routingMode = (m >= 0.75) ? RoutingMode::AuxFullWet
                              : RoutingMode::Insert50_50;
  }
  double getMix() const noexcept {
    return (routingMode == RoutingMode::AuxFullWet) ? 1.0 : 0.5;
  }

  // --- Mode Switches ---
  void setStageMode(StageMode m) noexcept { stageMode = m; }
  StageMode getStageMode() const noexcept { return stageMode; }
  bool isStereo6Plus6() const noexcept {
    return stageMode == StageMode::Stereo6Plus6;
  }

  void setLfoMode(LfoMode m) noexcept { lfoMode = m; }
  LfoMode getLfoMode() const noexcept { return lfoMode; }
  bool isMirroredLfo() const noexcept {
    return lfoMode == LfoMode::Mirrored;
  }

  void setRoutingMode(RoutingMode m) noexcept { routingMode = m; }
  RoutingMode getRoutingMode() const noexcept { return routingMode; }
  bool isAuxRouting() const noexcept {
    return routingMode == RoutingMode::AuxFullWet;
  }

  void setBypassed(bool b) noexcept { bypassed = b; }
  bool isBypassed() const noexcept { return bypassed; }

  // --- Telemetry for UI ---
  float getTelemetryLfoA() const noexcept {
    return telemetryLfoA.load(std::memory_order_relaxed);
  }
  float getTelemetryLfoB() const noexcept {
    return telemetryLfoB.load(std::memory_order_relaxed);
  }
  float getTelemetryFreqA() const noexcept {
    return telemetryFreqA.load(std::memory_order_relaxed);
  }
  float getTelemetryFreqB() const noexcept {
    return telemetryFreqB.load(std::memory_order_relaxed);
  }
  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  // --- 12 Factory Presets (3 Flagship Signature Presets + 9 Studio Presets) ---
  static const std::array<Preset, kNumPresets> &getFactoryPresets() noexcept {
    static const std::array<Preset, kNumPresets> kPresets = {{
        // Flagship 1: Classic Optical (Identity: Phasing Klasik)
        {"01 - Classic Optical", "Flagship", "Electric Piano", true,
         45.0, 50.0, 18.0, 20.0,
         StageMode::Mono12Stage, LfoMode::Linear, RoutingMode::Insert50_50},

        {"02 - Slow Vintage", "Optical", "Bass Synth", false,
         65.0, 45.0, 8.0, 25.0,
         StageMode::Mono12Stage, LfoMode::Linear, RoutingMode::Insert50_50},

        {"03 - Deep Optical", "Optical", "Synth Lead", false,
         90.0, 55.0, 20.0, 45.0,
         StageMode::Mono12Stage, LfoMode::Linear, RoutingMode::Insert50_50},

        // Flagship 2: Stereo Motion (Identity: Pergerakan Stereo)
        {"04 - Stereo Motion", "Flagship", "Wide Stereo", true,
         70.0, 55.0, 25.0, 25.0,
         StageMode::Stereo6Plus6, LfoMode::Mirrored, RoutingMode::Insert50_50},

        // Flagship 3: Dub Space (Identity: Karakter Dub)
        {"05 - Dub Space", "Flagship", "Dub Send", true,
         75.0, 40.0, 12.0, 65.0,
         StageMode::Stereo6Plus6, LfoMode::Mirrored, RoutingMode::AuxFullWet},

        {"06 - Reggae Organ", "Optical", "Organ", false,
         55.0, 42.0, 35.0, 38.0,
         StageMode::Mono12Stage, LfoMode::Linear, RoutingMode::Insert50_50},

        {"07 - Wide Synth", "Stereo", "Synthesizer", false,
         85.0, 65.0, 22.0, 40.0,
         StageMode::Stereo6Plus6, LfoMode::Mirrored, RoutingMode::Insert50_50},

        {"08 - Liquid Sweep", "Stereo", "Guitar", false,
         60.0, 60.0, 10.0, 18.0,
         StageMode::Stereo6Plus6, LfoMode::Linear, RoutingMode::Insert50_50},

        {"09 - Resonant Filter", "Resonant", "Resonant FX", false,
         75.0, 70.0, 15.0, 80.0,
         StageMode::Mono12Stage, LfoMode::Linear, RoutingMode::Insert50_50},

        {"10 - Drum Movement", "Dub / AUX", "Drum Loop", false,
         45.0, 65.0, 45.0, 30.0,
         StageMode::Stereo6Plus6, LfoMode::Mirrored, RoutingMode::AuxFullWet},

        {"11 - Atmosphere", "Dub / AUX", "Synth Pad", false,
         95.0, 35.0, 5.0, 32.0,
         StageMode::Stereo6Plus6, LfoMode::Mirrored, RoutingMode::AuxFullWet},

        {"12 - Extreme Optical", "Resonant", "Experimental", false,
         100.0, 80.0, 70.0, 85.0,
         StageMode::Mono12Stage, LfoMode::Linear, RoutingMode::Insert50_50},
    }};
    return kPresets;
  }

  void loadPreset(int index) noexcept {
    const auto &presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
      return;
    currentPresetIndex = index;
    const auto &p = presets[static_cast<size_t>(index)];
    depthPct = p.depthPct;
    centerPct = p.centerPct;
    ratePct = p.ratePct;
    feedbackPct = p.feedbackPct;
    stageMode = p.stageMode;
    lfoMode = p.lfoMode;
    routingMode = p.routingMode;
    bypassed = false;
  }

  // --- 64-Bit Real-Time Audio Processing ---
  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double dryL = sanitize(inOutL);
    const double dryR = sanitize(inOutR);

    const double sr = std::max(8000.0, cachedSampleRate);
    constexpr double twoPi = 6.28318530717958647692;
    constexpr double pi = 3.14159265358979323846;

    // 1. Per-sample parameter & mode smoothers (zero clicks / zero zipper)
    const double paramSmoothAlpha = 1.0 - std::exp(-1.0 / (0.015 * sr));
    const double coeffSmoothAlpha = 1.0 - std::exp(-1.0 / (0.004 * sr));
    const double modeCrossfadeAlpha = 1.0 - std::exp(-1.0 / (0.020 * sr));

    smoothDepth += paramSmoothAlpha * (depthPct - smoothDepth);
    smoothCenter += paramSmoothAlpha * (centerPct - smoothCenter);
    smoothRate += paramSmoothAlpha * (ratePct - smoothRate);
    smoothFeedback += paramSmoothAlpha * (feedbackPct - smoothFeedback);

    const double targetStereo = (stageMode == StageMode::Stereo6Plus6) ? 1.0 : 0.0;
    const double targetAux = (routingMode == RoutingMode::AuxFullWet) ? 1.0 : 0.0;
    const double targetActive = bypassed ? 0.0 : 1.0;

    smoothStereoMix += modeCrossfadeAlpha * (targetStereo - smoothStereoMix);
    smoothAuxMix += modeCrossfadeAlpha * (targetAux - smoothAuxMix);
    smoothActiveGain += paramSmoothAlpha * (targetActive - smoothActiveGain);

    // 2. Optical LFO Generator
    const double rateHz = ratePctToHz(smoothRate);
    lfoPhase += rateHz / sr;
    if (lfoPhase >= 1.0)
      lfoPhase -= 1.0;

    const double tri = 1.0 - 4.0 * std::abs(lfoPhase - 0.5);
    const double sineLfo = std::sin(lfoPhase * twoPi);
    const double rawLfo = 0.62 * tri + 0.38 * sineLfo;

    const double lfoTargetA = (lfoMode == LfoMode::Mirrored) ? -rawLfo : rawLfo;
    const double lfoTargetB = rawLfo;

    // 3. Photoresistor (LDR / Vactrol) Nonlinear Optical Response Model
    // - LED brightness [0..1] passes through optical gamma compression
    // - Asymmetric LDR rise (~8 ms) vs decay/fall (~32 ms) lag
    const double riseCoeff = 1.0 - std::exp(-1.0 / (0.008 * sr));
    const double fallCoeff = 1.0 - std::exp(-1.0 / (0.032 * sr));

    auto opticalResponse = [](double bipolar) noexcept -> double {
      const double u = std::clamp(0.5 + 0.5 * bipolar, 0.0, 1.0);
      // Photoresistor illumination saturation & threshold curve
      return std::pow(u, 0.78) * (0.85 + 0.15 * u);
    };

    const double ldrTargetA = opticalResponse(lfoTargetA);
    const double ldrTargetB = opticalResponse(lfoTargetB);

    optEnvA += (ldrTargetA > optEnvA ? riseCoeff : fallCoeff) *
               (ldrTargetA - optEnvA);
    optEnvB += (ldrTargetB > optEnvB ? riseCoeff : fallCoeff) *
               (ldrTargetB - optEnvB);

    const double modA = (optEnvA * 2.0 - 1.0);
    const double modB = (optEnvB * 2.0 - 1.0);

    // 4. Frequency Sweep Calculation (in octaves around Centre Frequency)
    const double baseFc = centerPctToHz(smoothCenter);
    const double maxOctaveSpan = 2.6 * (smoothDepth * 0.01);
    const double fcA = std::clamp(baseFc * std::pow(2.0, modA * maxOctaveSpan),
                                  28.0, sr * 0.42);
    const double fcB = std::clamp(baseFc * std::pow(2.0, modB * maxOctaveSpan),
                                  28.0, sr * 0.42);

    auto calcAllPassCoeff = [&](double fc) noexcept -> double {
      const double t = std::tan(pi * fc / sr);
      return (1.0 - t) / (1.0 + t);
    };

    static constexpr std::array<double, kHalfStages> kStageSpread = {
        0.72, 0.88, 1.00, 1.14, 1.32, 1.54};

    // Controlled Feedback Gain (strictly bounded <= 0.84 to prevent runaway oscillation)
    const double normFb = std::clamp(smoothFeedback * 0.01, 0.0, 1.0);
    const double fbGain = 0.84 * std::pow(normFb, 0.88);
    // Gentle resonance headroom compensation at high feedback
    const double inputTrim = 1.0 - 0.18 * normFb;
    const double dcAlpha = 1.0 - std::exp(-twoPi * 12.0 / sr);

    auto processStage = [](double x, double a, double &prevIn,
                           double &prevOut) noexcept -> double {
      const double y = a * x + prevIn - a * prevOut;
      prevIn = x;
      prevOut = sanitize(y);
      return prevOut;
    };

    double wetMonoL = 0.0, wetMonoR = 0.0;
    double wetStereoL = 0.0, wetStereoR = 0.0;

    // Evaluate Mono 12-Stage path whenever active or crossfading
    if (smoothStereoMix < 0.9999) {
      const double monoIn = 0.5 * (dryL + dryR) * inputTrim;
      fbDcStateMono += dcAlpha * (fbMono - fbDcStateMono);
      const double cleanFb = std::tanh(fbMono - fbDcStateMono);
      double sig = monoIn + fbGain * cleanFb;

      for (size_t i = 0; i < kHalfStages; ++i) {
        const double targetA =
            calcAllPassCoeff(std::clamp(fcA * kStageSpread[i], 28.0, sr * 0.42));
        smoothedCoeffMono[i] += coeffSmoothAlpha * (targetA - smoothedCoeffMono[i]);
        sig = processStage(sig, smoothedCoeffMono[i], monoStagePrevIn[i],
                           monoStageState[i]);
      }
      for (size_t i = 0; i < kHalfStages; ++i) {
        const size_t idx = kHalfStages + i;
        const double targetA =
            calcAllPassCoeff(std::clamp(fcB * kStageSpread[i], 28.0, sr * 0.42));
        smoothedCoeffMono[idx] +=
            coeffSmoothAlpha * (targetA - smoothedCoeffMono[idx]);
        sig = processStage(sig, smoothedCoeffMono[idx], monoStagePrevIn[idx],
                           monoStageState[idx]);
      }
      fbMono = sanitize(sig);
      wetMonoL = sig;
      wetMonoR = sig;
    }

    // Evaluate Stereo 6+6 path whenever active or crossfading
    if (smoothStereoMix > 0.0001) {
      fbDcStateL += dcAlpha * (fbStereoL - fbDcStateL);
      fbDcStateR += dcAlpha * (fbStereoR - fbDcStateR);
      double sigL = dryL * inputTrim + fbGain * std::tanh(fbStereoL - fbDcStateL);
      double sigR = dryR * inputTrim + fbGain * std::tanh(fbStereoR - fbDcStateR);

      for (size_t i = 0; i < kHalfStages; ++i) {
        const double targetL =
            calcAllPassCoeff(std::clamp(fcA * kStageSpread[i], 28.0, sr * 0.42));
        const double targetR =
            calcAllPassCoeff(std::clamp(fcB * kStageSpread[i], 28.0, sr * 0.42));

        smoothedCoeffStereo[i] +=
            coeffSmoothAlpha * (targetL - smoothedCoeffStereo[i]);
        smoothedCoeffStereo[kHalfStages + i] +=
            coeffSmoothAlpha * (targetR - smoothedCoeffStereo[kHalfStages + i]);

        sigL = processStage(sigL, smoothedCoeffStereo[i], stereoStagePrevIn[i],
                            stereoStageState[i]);
        sigR = processStage(sigR, smoothedCoeffStereo[kHalfStages + i],
                            stereoStagePrevIn[kHalfStages + i],
                            stereoStageState[kHalfStages + i]);
      }
      fbStereoL = sanitize(sigL);
      fbStereoR = sanitize(sigR);
      wetStereoL = sigL;
      wetStereoR = sigR;
    }

    // Click-free Mono 12-Stage <-> Stereo 6+6 crossfade
    const double wetL = wetMonoL * (1.0 - smoothStereoMix) + wetStereoL * smoothStereoMix;
    const double wetR = wetMonoR * (1.0 - smoothStereoMix) + wetStereoR * smoothStereoMix;

    // 5. Routing Gain Structure:
    //    Insert (smoothAuxMix == 0.0): exact 50% Dry + 50% Wet
    //    AUX    (smoothAuxMix == 1.0): exact 100% Wet (0% Dry)
    const double dryRatio = 0.5 * (1.0 - smoothAuxMix);
    const double wetRatio = 0.5 + 0.5 * smoothAuxMix;
    const double processedL = dryRatio * dryL + wetRatio * wetL;
    const double processedR = dryRatio * dryR + wetRatio * wetR;

    // 6. Click-free Bypass Crossfade
    inOutL = sanitize(dryL * (1.0 - smoothActiveGain) + processedL * smoothActiveGain);
    inOutR = sanitize(dryR * (1.0 - smoothActiveGain) + processedR * smoothActiveGain);

    telemetryLfoA.store(static_cast<float>(modA), std::memory_order_relaxed);
    telemetryLfoB.store(static_cast<float>(modB), std::memory_order_relaxed);
    telemetryFreqA.store(static_cast<float>(fcA), std::memory_order_relaxed);
    telemetryFreqB.store(static_cast<float>(fcB), std::memory_order_relaxed);
  }

private:
  double cachedSampleRate = 48000.0;
  double depthPct = 45.0;
  double centerPct = 50.0;
  double ratePct = 18.0;
  double feedbackPct = 20.0;

  StageMode stageMode = StageMode::Mono12Stage;
  LfoMode lfoMode = LfoMode::Linear;
  RoutingMode routingMode = RoutingMode::Insert50_50;
  bool bypassed = false;
  int currentPresetIndex = 0;

  // Per-sample smoothers
  double smoothDepth = 45.0;
  double smoothCenter = 50.0;
  double smoothRate = 18.0;
  double smoothFeedback = 20.0;
  double smoothStereoMix = 0.0;
  double smoothAuxMix = 0.0;
  double smoothActiveGain = 1.0;

  double lfoPhase = 0.0;
  double optEnvA = 0.5;
  double optEnvB = 0.5;

  double fbMono = 0.0;
  double fbStereoL = 0.0;
  double fbStereoR = 0.0;
  double fbDcStateMono = 0.0;
  double fbDcStateL = 0.0;
  double fbDcStateR = 0.0;

  std::array<double, kNumStages> monoStageState{};
  std::array<double, kNumStages> monoStagePrevIn{};
  std::array<double, kNumStages> stereoStageState{};
  std::array<double, kNumStages> stereoStagePrevIn{};
  std::array<double, kNumStages> smoothedCoeffMono{};
  std::array<double, kNumStages> smoothedCoeffStereo{};

  std::atomic<float> telemetryLfoA{0.0f};
  std::atomic<float> telemetryLfoB{0.0f};
  std::atomic<float> telemetryFreqA{700.0f};
  std::atomic<float> telemetryFreqB{700.0f};
};

} // namespace nova::graviton
