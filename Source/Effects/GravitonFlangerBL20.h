#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace nova::graviton {

// ==============================================================================
// NOVA HD — Stereo Flanger BL-20 (64-Bit BBD Stereo Flanger & Function Engine)
// Inspired by Bel BF-20 Hardware & Arturia Flanger BL-20 Architecture
//
// Complete 12 Core Features:
//  1. BBD Emulation: Clock-dependent dynamic anti-aliasing filter, syllabic
//     compander, and asymmetric analog stage saturation.
//  2. Zero Crossing (ZERO CR): Compensated reference dry delay line allowing
//     the modulated BBD delay to sweep directly through 0.0 ms relative delay.
//  3. Manual Control: Static BBD delay sweep position + Function Generator bus.
//  4. Envelope Follower (ENV): Threshold, Decay (Slow/Fast), INT ENV & EXT ENV.
//  5. Auto LFO: Rate (Slow/Fast), Depth, Tempo Sync, Reverse Sweep O/P 2.
//  6. Function Generator: 16-node custom breakpoint modulator with Drawing Tools,
//     8 curve presets, Bipolar/Unipolar mode, and Dice Randomizer.
//  7. Auto Shape: 5 LFO waveforms (ORIG, Sine, Square, Rising Saw, Falling Saw).
//  8. Stereo Offset: Bipolar L/R control voltage differential (-100% to +100%).
//  9. Reverse Sweep O/P 2: Opposite-direction stereo sweep on Channel 2.
// 10. Wet-Path Hi-Pass Filter (OFF to 3 kHz): Keeps low-end solid in dry path.
// 11. External Envelope (EXT ENV): Transient-accentuated sidechain detector.
// 12. Tempo Sync: Musical subdivision lock (2 Bars, 1 Bar, 1/2, 1/4, 1/8, 1/16).
// ==============================================================================
class FlangerBL20Engine final {
public:
  static constexpr int kMaxDelaySamples = 8192; // > 40 ms @ 192 kHz
  static constexpr size_t kMaxFuncNodes = 16;
  static constexpr size_t kNumPresets = 28;

  enum class AutoShape : int {
    Orig = 0,       // Classic RC-curved hardware triangle
    Sine = 1,       // Smooth sinusoidal sweep
    Square = 2,     // Stepped ping-pong jump
    RisingSaw = 3,  // Upward ramp
    FallingSaw = 4  // Downward ramp
  };

  struct FuncNode {
    float x = 0.0f;     // Normalized time [0.0 .. 1.0]
    float y = 0.5f;     // Normalized delay modulation [0.0 .. 1.0]
    float curve = 0.0f; // -1.0 (exp) .. 0.0 (linear) .. +1.0 (log), >1.5 = step
  };

  struct Preset {
    const char *name;
    const char *category;
    const char *instrumentTarget;
    bool stereoMode;
    bool zeroCrossing;
    double mix;            // 0.0 .. 1.0 (Doppler Dry/Wet)
    double regen;          // 0.0 .. 0.98
    bool manualEnabled;
    bool envEnabled;
    bool autoEnabled;
    double manualVal;      // 0.0 .. 1.0
    double envThresholdDb; // -48.0 .. 0.0 dB
    double envDecayMs;     // 15.0 .. 1500.0 ms
    bool extEnv;
    double autoRateHz;     // 0.02 .. 12.0 Hz
    bool tempoSync;
    double autoDepth;      // 0.0 .. 1.0
    bool reverseSweepOp2;
    double hiPassHz;       // 15.0 (OFF) .. 3000.0 Hz
    bool addToManual;
    double funcAmount;     // -1.0 .. +1.0
    double funcRateHz;     // 0.05 .. 16.0 Hz
    int funcPresetIdx;     // 0 .. 7
    AutoShape autoShape;
    double stereoOffset;   // -1.0 .. +1.0
  };

  FlangerBL20Engine() noexcept {
    loadFuncPreset(1);
    reset();
    loadPreset(0);
  }

  void reset() noexcept {
    wetBufL.fill(0.0);
    wetBufR.fill(0.0);
    dryZeroBufL.fill(0.0);
    dryZeroBufR.fill(0.0);
    writePos = 0;

    fbStateL = 0.0;
    fbStateR = 0.0;
    dcInL = dcOutL = 0.0;
    dcInR = dcOutR = 0.0;
    bbdLpL = bbdLpR = 0.0;
    compEnvInL = compEnvInR = 0.1;
    compEnvOutL = compEnvOutR = 0.1;

    hpfWetL.reset();
    hpfWetR.reset();
    extSidechainBp.reset();

    lfoPhase = 0.0;
    funcPhase = 0.0;
    envFollower = 0.0;
    smoothDelayMsL = 1.8;
    smoothDelayMsR = 1.8;

    clipFlag.store(false, std::memory_order_relaxed);
    clipMeter.store(0.0f, std::memory_order_relaxed);
    controlModMeterL.store(0.0f, std::memory_order_relaxed);
    controlModMeterR.store(0.0f, std::memory_order_relaxed);
    envTelemetry.store(0.0f, std::memory_order_relaxed);
    autoTelemetry.store(0.0f, std::memory_order_relaxed);
    funcPlayhead.store(0.0f, std::memory_order_relaxed);
    telemetryDelayMsL.store(1.8f, std::memory_order_relaxed);
    telemetryDelayMsR.store(1.8f, std::memory_order_relaxed);

    updateFilters();
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
      sampleRate = sr;
      updateFilters();
    }
  }
  double getSampleRate() const noexcept { return sampleRate; }

  // --- 1. Top-Left Section: Power, Mono/Stereo, Zero CR, Mix, Regen ---
  void setEnabled(bool on) noexcept { enabled = on; }
  bool isEnabled() const noexcept { return enabled; }

  void setStereoMode(bool st) noexcept { stereoMode = st; }
  bool isStereoMode() const noexcept { return stereoMode; }

  void setZeroCrossing(bool zc) noexcept { zeroCrossing = zc; }
  bool isZeroCrossing() const noexcept { return zeroCrossing; }

  void setMix(double newMix) noexcept { mix = std::clamp(newMix, 0.0, 1.0); }
  double getMix() const noexcept { return mix; }

  void setRegen(double r) noexcept { regen = std::clamp(r, 0.0, 0.98); }
  double getRegen() const noexcept { return regen; }

  void setFeedbackPolarity(bool negative) noexcept { negativeRegen = negative; }
  bool isFeedbackNegative() const noexcept { return negativeRegen; }

  void clearClipFlag() noexcept {
    clipFlag.store(false, std::memory_order_relaxed);
    clipMeter.store(0.0f, std::memory_order_relaxed);
  }

  // Backward-compatible aliases for EffectsRack / FlangerEngine
  void setDelayMs(double ms) noexcept {
    const double norm = std::clamp((ms - 0.15) / 6.85, 0.0, 1.0);
    setManualValue(norm);
  }
  double getDelayMs() const noexcept { return 0.15 + manualValue * 6.85; }
  void setFeedback(double fb) noexcept { setRegen(fb); }
  double getFeedback() const noexcept { return getRegen(); }

  // --- 2. Top-Right Section: Control Mode (MANUAL, ENV, AUTO) ---
  void setManualModeEnabled(bool on) noexcept { manualModeEnabled = on; }
  bool isManualModeEnabled() const noexcept { return manualModeEnabled; }

  void setEnvModeEnabled(bool on) noexcept { envModeEnabled = on; }
  bool isEnvModeEnabled() const noexcept { return envModeEnabled; }

  void setAutoModeEnabled(bool on) noexcept { autoModeEnabled = on; }
  bool isAutoModeEnabled() const noexcept { return autoModeEnabled; }

  // Manual Knob
  void setManualValue(double v) noexcept {
    manualValue = std::clamp(v, 0.0, 1.0);
  }
  double getManualValue() const noexcept { return manualValue; }

  // Envelope Follower
  void setEnvThresholdDb(double db) noexcept {
    envThresholdDb = std::clamp(db, -48.0, 0.0);
  }
  double getEnvThresholdDb() const noexcept { return envThresholdDb; }

  void setEnvDecayMs(double ms) noexcept {
    envDecayMs = std::clamp(ms, 15.0, 1500.0);
  }
  double getEnvDecayMs() const noexcept { return envDecayMs; }

  // Set Decay by percentage (0% = SLOW 1500ms, 100% = FAST 15ms; 40% ≈ 340ms)
  void setEnvDecayPercent(double pct) noexcept {
    const double clamped = std::clamp(pct, 0.0, 100.0);
    envDecayMs = 1500.0 * std::pow(15.0 / 1500.0, clamped * 0.01);
  }
  double getEnvDecayPercent() const noexcept {
    const double curMs = std::clamp(envDecayMs, 15.0, 1500.0);
    return std::clamp(
        100.0 * std::log(curMs / 1500.0) / std::log(15.0 / 1500.0), 0.0, 100.0);
  }

  void setExtEnvSource(bool ext) noexcept { extEnvSource = ext; }
  bool isExtEnvSource() const noexcept { return extEnvSource; }

  // Auto LFO
  void setAutoRateHz(double hz) noexcept {
    autoRateHz = std::clamp(hz, 0.02, 12.0);
  }
  double getAutoRateHz() const noexcept { return autoRateHz; }

  void setTempoSync(bool sync) noexcept { tempoSync = sync; }
  bool isTempoSync() const noexcept { return tempoSync; }

  void setHostBpm(double bpm) noexcept {
    hostBpm = std::clamp(bpm, 40.0, 260.0);
  }
  double getHostBpm() const noexcept { return hostBpm; }

  const char *getTempoSyncDivisionName() const noexcept {
    if (autoRateHz < 0.15)      return "2 BARS";
    else if (autoRateHz < 0.35) return "1 BAR";
    else if (autoRateHz < 0.75) return "1/2";
    else if (autoRateHz < 1.50) return "1/4";
    else if (autoRateHz < 3.50) return "1/8";
    else                        return "1/16";
  }

  void setAutoDepth(double d) noexcept { autoDepth = std::clamp(d, 0.0, 1.0); }
  double getAutoDepth() const noexcept { return autoDepth; }

  void setReverseSweepOp2(bool rev) noexcept { reverseSweepOp2 = rev; }
  bool isReverseSweepOp2() const noexcept { return reverseSweepOp2; }

  // --- 3. Bottom-Left Section: Wet-Path Hi-Pass Filter ---
  void setHiPassHz(double hz) noexcept {
    hiPassHz = std::clamp(hz, 15.0, 3000.0);
    updateFilters();
  }
  double getHiPassHz() const noexcept { return hiPassHz; }
  bool isHiPassActive() const noexcept { return hiPassHz > 20.0; }

  // --- 4. Bottom-Center Section: Function Generator ---
  void setAddToManual(bool add) noexcept { addToManual = add; }
  bool isAddToManual() const noexcept { return addToManual; }

  void setFuncAmount(double amt) noexcept {
    funcAmount = std::clamp(amt, -1.0, 1.0);
  }
  double getFuncAmount() const noexcept { return funcAmount; }

  void setFuncRateHz(double hz) noexcept {
    funcRateHz = std::clamp(hz, 0.05, 16.0);
  }
  double getFuncRateHz() const noexcept { return funcRateHz; }

  void setFuncBipolar(bool bi) noexcept { funcBipolar = bi; }
  bool isFuncBipolar() const noexcept { return funcBipolar; }

  int getNumFuncNodes() const noexcept { return numFuncNodes; }
  const std::array<FuncNode, kMaxFuncNodes> &getFuncNodes() const noexcept {
    return funcNodes;
  }

  void setFuncNode(int idx, float x, float y, float curve = 0.0f) noexcept {
    if (idx < 0 || idx >= numFuncNodes)
      return;
    if (idx == 0)
      x = 0.0f;
    else if (idx == numFuncNodes - 1)
      x = 1.0f;
    else {
      const float prevX = funcNodes[static_cast<size_t>(idx - 1)].x + 0.01f;
      const float nextX = funcNodes[static_cast<size_t>(idx + 1)].x - 0.01f;
      x = std::clamp(x, prevX, nextX);
    }
    funcNodes[static_cast<size_t>(idx)].x = std::clamp(x, 0.0f, 1.0f);
    funcNodes[static_cast<size_t>(idx)].y = std::clamp(y, 0.0f, 1.0f);
    funcNodes[static_cast<size_t>(idx)].curve = curve;
  }

  void addFuncNode(float x, float y, float curve = 0.0f) noexcept {
    if (numFuncNodes >= static_cast<int>(kMaxFuncNodes))
      return;
    x = std::clamp(x, 0.02f, 0.98f);
    y = std::clamp(y, 0.0f, 1.0f);
    int insertIdx = numFuncNodes - 1;
    for (int i = 1; i < numFuncNodes; ++i) {
      if (x < funcNodes[static_cast<size_t>(i)].x) {
        insertIdx = i;
        break;
      }
    }
    for (int i = numFuncNodes; i > insertIdx; --i) {
      funcNodes[static_cast<size_t>(i)] = funcNodes[static_cast<size_t>(i - 1)];
    }
    funcNodes[static_cast<size_t>(insertIdx)] = {x, y, curve};
    numFuncNodes++;
  }

  void removeFuncNode(int idx) noexcept {
    if (numFuncNodes <= 2 || idx <= 0 || idx >= numFuncNodes - 1)
      return;
    for (int i = idx; i < numFuncNodes - 1; ++i) {
      funcNodes[static_cast<size_t>(i)] = funcNodes[static_cast<size_t>(i + 1)];
    }
    numFuncNodes--;
  }

  int getFuncPresetIndex() const noexcept { return currentFuncPreset; }
  int getLastDrawingTool() const noexcept { return activeDrawingTool; }

  void loadFuncPreset(int presetIdx) noexcept {
    currentFuncPreset = std::clamp(presetIdx, 0, 7);
    switch (currentFuncPreset) {
    case 0: // INIT (Flat / Neutral)
      numFuncNodes = 3;
      funcNodes[0] = {0.0f, 0.5f, 0.0f};
      funcNodes[1] = {0.5f, 0.5f, 0.0f};
      funcNodes[2] = {1.0f, 0.5f, 0.0f};
      break;
    case 1: // BL-20 Signature Multi-Peak (Reference Image Curve)
      numFuncNodes = 12;
      funcNodes[0]  = {0.00f, 0.22f, 0.0f};
      funcNodes[1]  = {0.10f, 0.65f, 0.0f};
      funcNodes[2]  = {0.18f, 0.30f, 0.0f};
      funcNodes[3]  = {0.28f, 0.58f, 0.0f};
      funcNodes[4]  = {0.38f, 0.36f, 0.0f};
      funcNodes[5]  = {0.48f, 0.76f, 0.0f};
      funcNodes[6]  = {0.56f, 0.14f, 0.0f};
      funcNodes[7]  = {0.66f, 0.85f, 0.0f};
      funcNodes[8]  = {0.74f, 0.45f, 0.0f};
      funcNodes[9]  = {0.82f, 0.68f, 0.0f};
      funcNodes[10] = {0.91f, 0.28f, 0.0f};
      funcNodes[11] = {1.00f, 0.22f, 0.0f};
      break;
    case 2: // 8-Step Sequencer
      numFuncNodes = 9;
      funcNodes[0] = {0.000f, 0.15f, 2.0f};
      funcNodes[1] = {0.125f, 0.75f, 2.0f};
      funcNodes[2] = {0.250f, 0.35f, 2.0f};
      funcNodes[3] = {0.375f, 0.90f, 2.0f};
      funcNodes[4] = {0.500f, 0.20f, 2.0f};
      funcNodes[5] = {0.625f, 0.65f, 2.0f};
      funcNodes[6] = {0.750f, 0.45f, 2.0f};
      funcNodes[7] = {0.875f, 0.82f, 2.0f};
      funcNodes[8] = {1.000f, 0.15f, 2.0f};
      break;
    case 3: // Triangle Sweep
      numFuncNodes = 3;
      funcNodes[0] = {0.0f, 0.08f, 0.0f};
      funcNodes[1] = {0.5f, 0.92f, 0.0f};
      funcNodes[2] = {1.0f, 0.08f, 0.0f};
      break;
    case 4: // Rising Staircase
      numFuncNodes = 7;
      funcNodes[0] = {0.00f, 0.10f, 2.0f};
      funcNodes[1] = {0.18f, 0.26f, 2.0f};
      funcNodes[2] = {0.36f, 0.42f, 2.0f};
      funcNodes[3] = {0.54f, 0.58f, 2.0f};
      funcNodes[4] = {0.72f, 0.74f, 2.0f};
      funcNodes[5] = {0.88f, 0.90f, 2.0f};
      funcNodes[6] = {1.00f, 0.10f, 0.0f};
      break;
    case 5: // Heartbeat Pulse
      numFuncNodes = 8;
      funcNodes[0] = {0.00f, 0.25f, 0.0f};
      funcNodes[1] = {0.18f, 0.25f, 0.0f};
      funcNodes[2] = {0.24f, 0.92f, 0.0f};
      funcNodes[3] = {0.31f, 0.10f, 0.0f};
      funcNodes[4] = {0.40f, 0.72f, 0.0f};
      funcNodes[5] = {0.48f, 0.25f, 0.0f};
      funcNodes[6] = {0.85f, 0.25f, 0.0f};
      funcNodes[7] = {1.00f, 0.25f, 0.0f};
      break;
    case 6: // Twin Peaks
      numFuncNodes = 6;
      funcNodes[0] = {0.00f, 0.15f, 0.4f};
      funcNodes[1] = {0.25f, 0.88f, -0.4f};
      funcNodes[2] = {0.50f, 0.28f, 0.4f};
      funcNodes[3] = {0.75f, 0.92f, -0.4f};
      funcNodes[4] = {0.90f, 0.35f, 0.0f};
      funcNodes[5] = {1.00f, 0.15f, 0.0f};
      break;
    case 7: // Chaotic Glitch
    default:
      numFuncNodes = 10;
      funcNodes[0] = {0.00f, 0.50f, 2.0f};
      funcNodes[1] = {0.11f, 0.92f, 0.0f};
      funcNodes[2] = {0.21f, 0.12f, 2.0f};
      funcNodes[3] = {0.34f, 0.78f, 0.0f};
      funcNodes[4] = {0.46f, 0.18f, 0.0f};
      funcNodes[5] = {0.59f, 0.88f, 2.0f};
      funcNodes[6] = {0.71f, 0.32f, 0.0f};
      funcNodes[7] = {0.83f, 0.95f, 0.0f};
      funcNodes[8] = {0.92f, 0.14f, 2.0f};
      funcNodes[9] = {1.00f, 0.50f, 0.0f};
      break;
    }
  }

  // Apply drawing tool macro to current function curve
  void applyDrawingTool(int toolId) noexcept {
    activeDrawingTool = std::clamp(toolId, 0, 5);
    if (toolId == 0) {
      loadFuncPreset(3); // Triangle
    } else if (toolId == 1) {
      numFuncNodes = 6;
      for (int i = 0; i < 6; ++i) {
        const float t = static_cast<float>(i) / 5.0f;
        funcNodes[static_cast<size_t>(i)] = {t, 0.1f + 0.8f * t, 0.0f};
      }
    } else if (toolId == 2) {
      numFuncNodes = 6;
      for (int i = 0; i < 6; ++i) {
        const float t = static_cast<float>(i) / 5.0f;
        funcNodes[static_cast<size_t>(i)] = {t, 0.9f - 0.8f * t, 0.0f};
      }
    } else if (toolId == 3) {
      for (int i = 0; i < numFuncNodes; ++i)
        funcNodes[static_cast<size_t>(i)].curve = 2.0f; // Step hold
    } else if (toolId == 4) {
      for (int i = 0; i < numFuncNodes; ++i)
        funcNodes[static_cast<size_t>(i)].curve = 0.0f; // Smooth linear
    } else if (toolId == 5) {
      loadFuncPreset(0);
    }
  }

  // Dice / Randomize Function Generator pattern
  void randomizeFuncPattern() noexcept {
    numFuncNodes = 10;
    funcNodes[0] = {0.0f, 0.35f, 0.0f};
    for (int i = 1; i < numFuncNodes - 1; ++i) {
      rngState = rngState * 1664525u + 1013904223u;
      const float rY = static_cast<float>((rngState >> 8) & 0xFFFF) / 65535.0f;
      rngState = rngState * 1664525u + 1013904223u;
      const bool step = ((rngState >> 16) & 0x3) == 0;
      const float x =
          static_cast<float>(i) / static_cast<float>(numFuncNodes - 1);
      funcNodes[static_cast<size_t>(i)] = {x, 0.08f + 0.84f * rY,
                                           step ? 2.0f : 0.0f};
    }
    funcNodes[static_cast<size_t>(numFuncNodes - 1)] = {1.0f, funcNodes[0].y,
                                                        0.0f};
  }

  // Evaluate Function Generator at normalized phase [0.0 .. 1.0]
  double evaluateFunctionAt(double phase01) const noexcept {
    const float p = static_cast<float>(std::clamp(phase01, 0.0, 1.0));
    if (numFuncNodes < 2)
      return 0.5;
    for (int i = 0; i < numFuncNodes - 1; ++i) {
      const auto &n0 = funcNodes[static_cast<size_t>(i)];
      const auto &n1 = funcNodes[static_cast<size_t>(i + 1)];
      if (p >= n0.x && p <= n1.x) {
        if (n0.curve > 1.5f)
          return static_cast<double>(n0.y); // Step hold
        const float span = std::max(1e-5f, n1.x - n0.x);
        float t = (p - n0.x) / span;
        if (std::abs(n0.curve) > 0.05f) {
          const float expFactor = std::pow(2.0f, n0.curve * 2.0f);
          t = std::pow(t, expFactor);
        }
        return static_cast<double>(n0.y + t * (n1.y - n0.y));
      }
    }
    return static_cast<double>(
        funcNodes[static_cast<size_t>(numFuncNodes - 1)].y);
  }

  // --- 5. Bottom-Right-Center Section: Auto Shape (5 Waveforms) ---
  void setAutoShape(AutoShape s) noexcept { autoShape = s; }
  AutoShape getAutoShape() const noexcept { return autoShape; }

  void cycleAutoShape() noexcept {
    const int next = (static_cast<int>(autoShape) + 1) % 5;
    autoShape = static_cast<AutoShape>(next);
  }

  static const char *getAutoShapeName(AutoShape s) noexcept {
    switch (s) {
    case AutoShape::Orig:       return "ORIG";
    case AutoShape::Sine:       return "SINE";
    case AutoShape::Square:     return "SQUARE";
    case AutoShape::RisingSaw:  return "RISING SAW";
    case AutoShape::FallingSaw: return "FALLING SAW";
    default:                    return "ORIG";
    }
  }

  // --- 6. Bottom-Far-Right Section: Stereo Offset ---
  void setStereoOffset(double offset) noexcept {
    stereoOffset = std::clamp(offset, -1.0, 1.0);
  }
  double getStereoOffset() const noexcept { return stereoOffset; }

  // --- Telemetry Getters for UI ---
  bool isClipping() const noexcept {
    return clipFlag.load(std::memory_order_relaxed);
  }
  float getClipMeter() const noexcept {
    return clipMeter.load(std::memory_order_relaxed);
  }
  float getControlModMeterL() const noexcept {
    return controlModMeterL.load(std::memory_order_relaxed);
  }
  float getControlModMeterR() const noexcept {
    return controlModMeterR.load(std::memory_order_relaxed);
  }
  float getEnvTelemetry() const noexcept {
    return envTelemetry.load(std::memory_order_relaxed);
  }
  float getAutoTelemetry() const noexcept {
    return autoTelemetry.load(std::memory_order_relaxed);
  }
  float getFuncPlayhead() const noexcept {
    return funcPlayhead.load(std::memory_order_relaxed);
  }
  float getDelayMsL() const noexcept {
    return telemetryDelayMsL.load(std::memory_order_relaxed);
  }
  float getDelayMsR() const noexcept {
    return telemetryDelayMsR.load(std::memory_order_relaxed);
  }

  // ===========================================================================
  // Complete 28-Preset Bank:
  //   01..08: Custom Mixing & Instrument Presets (Exact User Spec)
  //   09..19: Official Arturia BL-20 Reference Presets
  //   20..28: Studio & Vintage BBD Production Presets
  // ===========================================================================
  static const std::array<Preset, kNumPresets> &getFactoryPresets() noexcept {
    static const std::array<Preset, kNumPresets> kPresets = {{
        // --- BANK A: 8 CUSTOM MIXING PRESETS (INSTRUMENT TARGETED) ---
        // 01 — Vocal Air (Lead Vocal Enhancement)
        {"01 - Vocal Air", "Mixing Custom", "Lead Vocal",
         true, false, 0.10, 0.08,
         false, false, true, 0.25, -24.0, 300.0, false,
         0.15, false, 0.15, false, 180.0, false, 0.0, 0.50, 0,
         AutoShape::Sine, 0.12},

        // 02 — Wide Backing Vocal (Backing Vocal Stereo)
        {"02 - Wide Backing Vocal", "Mixing Custom", "Backing Vocal",
         true, false, 0.25, 0.15,
         false, false, true, 0.30, -24.0, 300.0, false,
         0.25, false, 0.35, false, 120.0, false, 0.0, 0.50, 0,
         AutoShape::Sine, 0.20},

        // 03 — Vintage Guitar (Classic Analog Flanger - Electric Guitar)
        {"03 - Vintage Guitar", "Mixing Custom", "Electric Guitar",
         true, true, 0.30, 0.30,
         false, false, true, 0.32, -22.0, 260.0, false,
         0.30, false, 0.50, false, 85.0, false, 0.0, 0.50, 1,
         AutoShape::Orig, 0.15},

        // 04 — Jet Plane (Extreme Flanging - Guitar Solo)
        {"04 - Jet Plane", "Mixing Custom", "Guitar Solo",
         true, true, 0.55, 0.65,
         false, false, true, 0.35, -20.0, 240.0, false,
         0.65, false, 0.85, false, 60.0, false, 0.0, 0.60, 3,
         AutoShape::Sine, 0.18},

        // 05 — Synth Dimension (Wide Synth & Pad)
        {"05 - Synth Dimension", "Mixing Custom", "Synth / Pad",
         true, false, 0.35, 0.25,
         false, false, true, 0.38, -22.0, 320.0, false,
         0.12, false, 0.65, true, 70.0, false, 0.0, 0.40, 1,
         AutoShape::Sine, 0.30},

        // 06 — Drum Metallic (Snare & Percussion)
        {"06 - Drum Metallic", "Mixing Custom", "Snare",
         true, false, 0.25, 0.45,
         false, false, true, 0.28, -18.0, 180.0, false,
         1.20, false, 0.55, false, 150.0, false, 0.0, 1.00, 2,
         AutoShape::Square, 0.15},

        // 07 — Envelope Motion (Dynamic Vocal / Guitar FX)
        {"07 - Envelope Motion", "Mixing Custom", "Vocal FX",
         true, false, 0.25, 0.20,
         false, true, false, 0.22, -22.0, 340.0, false,
         0.30, false, 0.30, false, 140.0, false, 0.0, 0.60, 1,
         AutoShape::Orig, 0.18},

        // 08 — Rhythmic Modulation (Electronic Music / Experimental Stereo)
        {"08 - Rhythmic Modulation", "Mixing Custom", "Electronic Music",
         true, false, 0.35, 0.35,
         true, false, true, 0.30, -20.0, 220.0, false,
         2.00, true, 0.70, true, 110.0, true, 0.65, 2.00, 2,
         AutoShape::Square, 0.35},

        // --- BANK B: 11 OFFICIAL ARTURIA BL-20 PRESETS ---
        // 09 — Default
        {"09 - Default", "Arturia Official", "All-Round",
         true, false, 0.45, 0.35,
         true, false, true, 0.32, -24.0, 250.0, false,
         0.35, false, 0.45, false, 15.0, false, 0.0, 0.80, 1,
         AutoShape::Orig, 0.00},

        // 10 — FG Gone Crazy
        {"10 - FG Gone Crazy", "Arturia Official", "Experimental FX",
         true, true, 0.55, 0.78,
         true, false, true, 0.40, -20.0, 180.0, false,
         0.85, false, 0.45, true, 95.0, true, 0.92, 2.40, 7,
         AutoShape::FallingSaw, -0.45},

        // 11 — Flanger Mono
        {"11 - Flanger Mono", "Arturia Official", "Mono Guitar / Keys",
         false, false, 0.48, 0.52,
         false, false, true, 0.30, -24.0, 260.0, false,
         0.28, false, 0.58, false, 55.0, false, 0.0, 0.50, 3,
         AutoShape::Orig, 0.00},

        // 12 — Side-Chain Experiment
        {"12 - Side-Chain Experiment", "Arturia Official", "Sidechain Dynamic",
         true, true, 0.46, 0.62,
         true, true, false, 0.24, -26.0, 140.0, true,
         0.40, false, 0.35, false, 130.0, false, 0.0, 1.00, 5,
         AutoShape::Orig, 0.32},

        // 13 — Three of a Kind (Manual + ENV + AUTO Layered)
        {"13 - Three of a Kind", "Arturia Official", "Multi-Modulation",
         true, true, 0.44, 0.56,
         true, true, true, 0.26, -23.0, 220.0, false,
         0.32, false, 0.42, true, 90.0, true, 0.48, 0.75, 6,
         AutoShape::Orig, 0.28},

        // 14 — Flunky
        {"14 - Flunky", "Arturia Official", "Funk Clav / Bass",
         true, false, 0.50, 0.66,
         true, true, true, 0.20, -25.0, 120.0, true,
         0.55, true, 0.28, false, 185.0, false, 0.0, 1.20, 2,
         AutoShape::RisingSaw, -0.25},

        // 15 — Drumwarp
        {"15 - Drumwarp", "Arturia Official", "Drum Bus / Loops",
         true, true, 0.42, 0.70,
         false, true, true, 0.28, -19.0, 110.0, false,
         0.48, true, 0.64, true, 240.0, false, 0.0, 1.50, 4,
         AutoShape::RisingSaw, 0.38},

        // 16 — Morning after chords_Wet
        {"16 - Morning after chords_Wet", "Arturia Official", "Chords / Keys",
         true, false, 0.78, 0.32,
         true, false, true, 0.42, -24.0, 350.0, false,
         0.16, false, 0.52, true, 110.0, true, 0.35, 0.35, 6,
         AutoShape::Sine, 0.42},

        // 17 — Sweet soul
        {"17 - Sweet soul", "Arturia Official", "Soul Guitar / Rhodes",
         true, false, 0.32, 0.28,
         true, false, true, 0.34, -24.0, 300.0, false,
         0.20, false, 0.40, false, 95.0, false, 0.0, 0.45, 1,
         AutoShape::Orig, 0.22},

        // 18 — Snare vibrations
        {"18 - Snare vibrations", "Arturia Official", "Snare / Clap",
         true, true, 0.36, 0.74,
         false, true, true, 0.22, -17.0, 85.0, true,
         1.60, false, 0.48, true, 280.0, false, 0.0, 1.80, 5,
         AutoShape::Square, -0.35},

        // 19 — Hearing double
        {"19 - Hearing double", "Arturia Official", "Stereo Doubler",
         true, false, 0.45, 0.12,
         true, false, true, 0.48, -24.0, 280.0, false,
         0.28, false, 0.25, true, 140.0, false, 0.0, 0.60, 1,
         AutoShape::Sine, 0.65},

        // --- BANK C: 9 STUDIO & HARDWARE BBD PRESETS ---
        // 20 — Subtle Vocal Flanger
        {"20 - Subtle Vocal Flanger", "Studio BBD", "Vocal",
         true, false, 0.15, 0.18,
         false, false, true, 0.25, -24.0, 220.0, false,
         0.22, false, 0.24, false, 220.0, false, 0.0, 0.50, 1,
         AutoShape::Sine, 0.18},

        // 21 — Wide Stereo Synth
        {"21 - Wide Stereo Synth", "Studio BBD", "Synth",
         true, false, 0.38, 0.52,
         false, false, true, 0.35, -20.0, 300.0, false,
         0.45, false, 0.62, true, 90.0, false, 0.0, 0.80, 1,
         AutoShape::Sine, 0.45},

        // 22 — Jet Flanger Guitar
        {"22 - Jet Flanger Guitar", "Studio BBD", "Guitar",
         true, true, 0.50, 0.84,
         false, false, true, 0.30, -18.0, 250.0, false,
         0.18, false, 0.82, false, 65.0, false, 0.0, 0.40, 3,
         AutoShape::Orig, 0.12},

        // 23 — Through-Zero Tape Whoosh
        {"23 - Through-Zero Tape Whoosh", "Studio BBD", "Master / FX",
         true, true, 0.50, 0.72,
         true, false, true, 0.34, -20.0, 350.0, false,
         0.12, false, 0.88, false, 35.0, false, 0.0, 0.50, 3,
         AutoShape::Orig, 0.22},

        // 24 — Dynamic Envelope Funk
        {"24 - Dynamic Envelope Funk", "Studio BBD", "Bass / Guitar",
         true, false, 0.48, 0.68,
         true, true, false, 0.20, -26.0, 180.0, false,
         0.50, false, 0.35, false, 160.0, false, 0.0, 1.00, 1,
         AutoShape::Orig, 0.25},

        // 25 — Rhythmic Step Flanger
        {"25 - Rhythmic Step Flanger", "Studio BBD", "Arp / Synth",
         true, false, 0.46, 0.76,
         true, false, false, 0.22, -22.0, 200.0, false,
         0.80, true, 0.40, false, 110.0, true, 0.78, 1.50, 2,
         AutoShape::Square, -0.30},

        // 26 — Metallic Square Ping-Pong
        {"26 - Metallic Square Ping-Pong", "Studio BBD", "FX / Keys",
         true, false, 0.44, 0.80,
         false, false, true, 0.30, -20.0, 200.0, false,
         1.80, true, 0.58, true, 180.0, false, 0.0, 1.00, 2,
         AutoShape::Square, 0.50},

        // 27 — Bel BF-20 Vintage Studio
        {"27 - Bel BF-20 Vintage Studio", "Studio BBD", "Vintage Rack",
         true, false, 0.45, 0.45,
         true, false, true, 0.32, -24.0, 260.0, false,
         0.32, false, 0.48, false, 50.0, true, 0.32, 0.40, 1,
         AutoShape::Orig, -0.28},

        // 28 — Drum Bus Comb Sweeper
        {"28 - Drum Bus Comb Sweeper", "Studio BBD", "Overheads / Bus",
         true, true, 0.32, 0.64,
         false, true, true, 0.28, -18.0, 140.0, false,
         0.26, false, 0.52, true, 380.0, false, 0.0, 0.80, 5,
         AutoShape::RisingSaw, 0.35}
    }};
    return kPresets;
  }

  void loadPreset(int index) noexcept {
    const auto &presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
      return;
    currentPresetIndex = index;
    const auto &p = presets[static_cast<size_t>(index)];
    stereoMode = p.stereoMode;
    zeroCrossing = p.zeroCrossing;
    mix = p.mix;
    regen = p.regen;
    manualModeEnabled = p.manualEnabled;
    envModeEnabled = p.envEnabled;
    autoModeEnabled = p.autoEnabled;
    manualValue = p.manualVal;
    envThresholdDb = p.envThresholdDb;
    envDecayMs = p.envDecayMs;
    extEnvSource = p.extEnv;
    autoRateHz = p.autoRateHz;
    tempoSync = p.tempoSync;
    autoDepth = p.autoDepth;
    reverseSweepOp2 = p.reverseSweepOp2;
    setHiPassHz(p.hiPassHz);
    addToManual = p.addToManual;
    funcAmount = p.funcAmount;
    funcRateHz = p.funcRateHz;
    loadFuncPreset(p.funcPresetIdx);
    autoShape = p.autoShape;
    stereoOffset = p.stereoOffset;
  }

  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  // ===========================================================================
  // Core 64-Bit Real-Time Stereo BBD Flanger Processing
  // ===========================================================================
  void processStereo(double &inOutL, double &inOutR) noexcept {
    if (!enabled)
      return;

    const double rawL = sanitize(inOutL);
    const double rawR = sanitize(inOutR);

    const double inL = stereoMode ? rawL : 0.5 * (rawL + rawR);
    const double inR = stereoMode ? rawR : inL;

    // -------------------------------------------------------------------------
    // 1. Envelope Follower (ENV Mode: INT ENV or EXT ENV)
    // -------------------------------------------------------------------------
    double envInput = 0.5 * (std::abs(inL) + std::abs(inR));
    if (extEnvSource) {
      envInput = std::abs(extSidechainBp.process(0.5 * (rawL + rawR))) * 1.65;
    }

    const double threshLin = dbToGain(envThresholdDb);
    double trigLevel = 0.0;
    if (envInput > threshLin) {
      const double overDb =
          20.0 * std::log10(std::max(1e-6, envInput / threshLin));
      trigLevel = std::clamp(overDb / 24.0, 0.0, 1.0);
    }

    const double atkCoeff = std::exp(-1.0 / (0.0025 * sampleRate));
    const double relCoeff =
        std::exp(-1.0 / (std::max(0.010, envDecayMs * 0.001) * sampleRate));
    if (trigLevel > envFollower)
      envFollower = atkCoeff * envFollower + (1.0 - atkCoeff) * trigLevel;
    else
      envFollower = relCoeff * envFollower + (1.0 - relCoeff) * trigLevel;

    envTelemetry.store(static_cast<float>(envFollower),
                       std::memory_order_relaxed);

    // -------------------------------------------------------------------------
    // 2. Auto LFO Generator (5 Shapes + Tempo Sync + Reverse Sweep O/P 2)
    // -------------------------------------------------------------------------
    double effectiveAutoRate = autoRateHz;
    if (tempoSync) {
      const double beatHz = hostBpm / 60.0;
      if (autoRateHz < 0.15)      effectiveAutoRate = beatHz * 0.125; // 2 Bars
      else if (autoRateHz < 0.35) effectiveAutoRate = beatHz * 0.25;  // 1 Bar
      else if (autoRateHz < 0.75) effectiveAutoRate = beatHz * 0.5;   // 1/2 Note
      else if (autoRateHz < 1.50) effectiveAutoRate = beatHz * 1.0;   // 1/4 Note
      else if (autoRateHz < 3.50) effectiveAutoRate = beatHz * 2.0;   // 1/8 Note
      else                        effectiveAutoRate = beatHz * 4.0;   // 1/16 Note
    }

    const double lfoVal01 = evaluateAutoShape(lfoPhase, autoShape);
    lfoPhase += effectiveAutoRate / sampleRate;
    if (lfoPhase >= 1.0)
      lfoPhase -= std::floor(lfoPhase);

    autoTelemetry.store(static_cast<float>(lfoVal01),
                        std::memory_order_relaxed);

    // -------------------------------------------------------------------------
    // 3. Function Generator (Multi-Node Curve + Add to Manual)
    // -------------------------------------------------------------------------
    const double funcRaw01 = evaluateFunctionAt(funcPhase);
    funcPlayhead.store(static_cast<float>(funcPhase),
                       std::memory_order_relaxed);
    funcPhase += funcRateHz / sampleRate;
    if (funcPhase >= 1.0)
      funcPhase -= std::floor(funcPhase);

    const double funcMod = funcBipolar ? ((funcRaw01 - 0.5) * 2.0 * funcAmount)
                                       : (funcRaw01 * funcAmount);

    // -------------------------------------------------------------------------
    // 4. Combine the 3 Modulation Sources (MANUAL + ENV + AUTO) + Stereo Offset
    // -------------------------------------------------------------------------
    double cvL = 0.0;
    double cvR = 0.0;

    if (manualModeEnabled) {
      double manSig = manualValue;
      if (addToManual)
        manSig = std::clamp(manSig + funcMod, 0.0, 1.0);
      cvL += manSig;
      cvR += manSig;
    } else if (addToManual) {
      const double fctOnly = std::clamp(0.5 + 0.5 * funcMod, 0.0, 1.0);
      cvL += fctOnly;
      cvR += fctOnly;
    }

    if (envModeEnabled) {
      cvL += envFollower;
      cvR += envFollower;
    }

    if (autoModeEnabled) {
      const double autoL = lfoVal01 * autoDepth;
      const double autoR =
          (reverseSweepOp2 ? (1.0 - lfoVal01) : lfoVal01) * autoDepth;
      cvL += autoL;
      cvR += autoR;
    }

    if (stereoMode) {
      cvR += stereoOffset * 0.45;
    } else {
      cvR = cvL;
    }

    cvL = std::clamp(cvL, 0.0, 1.0);
    cvR = std::clamp(cvR, 0.0, 1.0);

    controlModMeterL.store(static_cast<float>(cvL), std::memory_order_relaxed);
    controlModMeterR.store(static_cast<float>(cvR), std::memory_order_relaxed);

    // -------------------------------------------------------------------------
    // 5. Map Control Voltage [0..1] to BBD Delay Time (Exponential BBD Law)
    // -------------------------------------------------------------------------
    constexpr double kMinDelayMs = 0.15;
    constexpr double kMaxDelayMs = 6.85;
    constexpr double kZeroRefDelayMs = 2.10;

    const double targetDelayMsL =
        kMinDelayMs * std::pow(kMaxDelayMs / kMinDelayMs, cvL);
    const double targetDelayMsR =
        kMinDelayMs * std::pow(kMaxDelayMs / kMinDelayMs, cvR);

    const double dSmooth = std::exp(-1.0 / (0.0012 * sampleRate));
    smoothDelayMsL = dSmooth * smoothDelayMsL + (1.0 - dSmooth) * targetDelayMsL;
    smoothDelayMsR = dSmooth * smoothDelayMsR + (1.0 - dSmooth) * targetDelayMsR;

    telemetryDelayMsL.store(static_cast<float>(smoothDelayMsL),
                            std::memory_order_relaxed);
    telemetryDelayMsR.store(static_cast<float>(smoothDelayMsR),
                            std::memory_order_relaxed);

    const double delaySamplesL = std::clamp(
        smoothDelayMsL * 0.001 * sampleRate, 1.0,
        static_cast<double>(kMaxDelaySamples - 4));
    const double delaySamplesR = std::clamp(
        smoothDelayMsR * 0.001 * sampleRate, 1.0,
        static_cast<double>(kMaxDelaySamples - 4));

    double wetL = readDelayHermite(wetBufL, writePos, delaySamplesL);
    double wetR = readDelayHermite(wetBufR, writePos, delaySamplesR);

    // -------------------------------------------------------------------------
    // 6. BBD Syllabic Compander & Regeneration Feedback + Clip Detection
    // -------------------------------------------------------------------------
    const double compFollow = std::exp(-1.0 / (0.008 * sampleRate));
    compEnvInL = compFollow * compEnvInL +
                 (1.0 - compFollow) * std::max(0.01, std::abs(inL));
    compEnvInR = compFollow * compEnvInR +
                 (1.0 - compFollow) * std::max(0.01, std::abs(inR));

    const double compInGainL = std::pow(compEnvInL, -0.18);
    const double compInGainR = std::pow(compEnvInR, -0.18);

    const double signedRegen = negativeRegen ? -regen : regen;
    double fbInL = (inL * compInGainL) + signedRegen * fbStateL;
    double fbInR = (inR * compInGainR) + signedRegen * fbStateR;

    // 20 Hz DC Blocker in feedback loop
    const double dcOutNextL = fbInL - dcInL + dcR * dcOutL;
    dcInL = fbInL;
    dcOutL = dcOutNextL;

    const double dcOutNextR = fbInR - dcInR + dcR * dcOutR;
    dcInR = fbInR;
    dcOutR = dcOutNextR;

    // Clock-dependent BBD anti-aliasing filter (warmer at longer BBD delays)
    const double clockFactor =
        1.0 - 0.28 * std::clamp((smoothDelayMsL - 0.15) / 6.70, 0.0, 1.0);
    const double dynAlpha = std::clamp(bbdLpAlpha * clockFactor, 0.15, 0.95);
    bbdLpL += dynAlpha * (dcOutL - bbdLpL);
    bbdLpR += dynAlpha * (dcOutR - bbdLpR);

    // Check Clip threshold before soft BBD limiting
    const double fbPeak = std::max(std::abs(bbdLpL), std::abs(bbdLpR));
    if (fbPeak > 0.85) {
      clipFlag.store(true, std::memory_order_relaxed);
      clipMeter.store(static_cast<float>(std::min(1.0, fbPeak)),
                      std::memory_order_relaxed);
    } else {
      const float prevClip = clipMeter.load(std::memory_order_relaxed);
      const float nextClip = prevClip * 0.9992f;
      clipMeter.store(nextClip, std::memory_order_relaxed);
      if (nextClip < 0.15f)
        clipFlag.store(false, std::memory_order_relaxed);
    }

    // Asymmetric analog BBD cell saturation
    const double satL = 0.96 * std::tanh((bbdLpL + 0.015 * bbdLpL * bbdLpL) / 0.96);
    const double satR = 0.96 * std::tanh((bbdLpR + 0.015 * bbdLpR * bbdLpR) / 0.96);

    wetBufL[static_cast<size_t>(writePos)] = sanitize(satL);
    wetBufR[static_cast<size_t>(writePos)] = sanitize(satR);

    fbStateL = wetL;
    fbStateR = wetR;

    // Compander output expander
    compEnvOutL = compFollow * compEnvOutL +
                  (1.0 - compFollow) * std::max(0.01, std::abs(wetL));
    compEnvOutR = compFollow * compEnvOutR +
                  (1.0 - compFollow) * std::max(0.01, std::abs(wetR));
    wetL *= std::pow(compEnvOutL, 0.18);
    wetR *= std::pow(compEnvOutR, 0.18);

    // -------------------------------------------------------------------------
    // 7. Zero-Crossing (ZERO CR) Reference Dry Delay Line
    // -------------------------------------------------------------------------
    dryZeroBufL[static_cast<size_t>(writePos)] = inL;
    dryZeroBufR[static_cast<size_t>(writePos)] = inR;

    double dryRefL = inL;
    double dryRefR = inR;
    if (zeroCrossing) {
      const double zeroDelaySamples = std::clamp(
          kZeroRefDelayMs * 0.001 * sampleRate, 1.0,
          static_cast<double>(kMaxDelaySamples - 4));
      dryRefL = readDelayHermite(dryZeroBufL, writePos, zeroDelaySamples);
      dryRefR = readDelayHermite(dryZeroBufR, writePos, zeroDelaySamples);
    }

    writePos = (writePos + 1) & (kMaxDelaySamples - 1);

    // -------------------------------------------------------------------------
    // 8. Wet-Path High-Pass Filter (OFF to 3 kHz)
    // -------------------------------------------------------------------------
    if (hiPassHz > 20.0) {
      wetL = hpfWetL.process(wetL);
      wetR = hpfWetR.process(wetR);
    }

    // -------------------------------------------------------------------------
    // 9. Doppler Dry/Wet Mixer
    // -------------------------------------------------------------------------
    const double wetPol = zeroCrossing ? -1.0 : 1.0;
    const double outL = dryRefL * (1.0 - mix) + (wetL * wetPol) * mix;
    const double outR = dryRefR * (1.0 - mix) + (wetR * wetPol) * mix;

    inOutL = sanitize(outL);
    inOutR = sanitize(stereoMode ? outR : outL);
  }

private:
  static double evaluateAutoShape(double phase01, AutoShape shape) noexcept {
    const double p = phase01 - std::floor(phase01);
    switch (shape) {
    case AutoShape::Orig: {
      const double tri = (p < 0.5) ? (p * 2.0) : (2.0 - p * 2.0);
      return std::clamp(
          0.5 + 0.5 * std::tanh((tri - 0.5) * 2.4) / std::tanh(1.2), 0.0, 1.0);
    }
    case AutoShape::Sine:
      return 0.5 + 0.5 * std::sin(2.0 * 3.14159265358979323846 * p);
    case AutoShape::Square:
      return (p < 0.5) ? 0.92 : 0.08;
    case AutoShape::RisingSaw:
      return p;
    case AutoShape::FallingSaw:
      return 1.0 - p;
    default:
      return 0.5;
    }
  }

  static double readDelayHermite(const std::array<double, kMaxDelaySamples> &buf,
                                 int wPos, double delaySamples) noexcept {
    const int dInt = static_cast<int>(delaySamples);
    const double frac = delaySamples - static_cast<double>(dInt);
    constexpr int mask = kMaxDelaySamples - 1;

    const int idx0 = (wPos - dInt + 1) & mask;
    const int idx1 = (wPos - dInt) & mask;
    const int idx2 = (wPos - dInt - 1) & mask;
    const int idx3 = (wPos - dInt - 2) & mask;

    const double y0 = buf[static_cast<size_t>(idx0)];
    const double y1 = buf[static_cast<size_t>(idx1)];
    const double y2 = buf[static_cast<size_t>(idx2)];
    const double y3 = buf[static_cast<size_t>(idx3)];

    const double c0 = y1;
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
  }

  void updateFilters() noexcept {
    dcR = std::clamp(1.0 - (2.0 * 3.14159265358979323846 * 20.0 / sampleRate),
                     0.90, 0.9999);

    const double fc = std::min(13500.0, sampleRate * 0.44);
    const double dt = 1.0 / sampleRate;
    const double rc = 1.0 / (2.0 * 3.14159265358979323846 * fc);
    bbdLpAlpha = dt / (rc + dt);

    const double hpfCutoff = std::clamp(hiPassHz, 15.0, sampleRate * 0.42);
    const double w0 = 2.0 * 3.14159265358979323846 * hpfCutoff / sampleRate;
    const double cosW = std::cos(w0);
    const double sinW = std::sin(w0);
    const double alpha = sinW / (2.0 * 0.7071067811865475);
    const double a0 = 1.0 + alpha;

    BiquadCoefficients hpfCoeffs{};
    hpfCoeffs.b0 = ((1.0 + cosW) * 0.5) / a0;
    hpfCoeffs.b1 = -(1.0 + cosW) / a0;
    hpfCoeffs.b2 = ((1.0 + cosW) * 0.5) / a0;
    hpfCoeffs.a1 = (-2.0 * cosW) / a0;
    hpfCoeffs.a2 = (1.0 - alpha) / a0;
    hpfWetL.setCoefficients(hpfCoeffs);
    hpfWetR.setCoefficients(hpfCoeffs);

    const double scW0 = 2.0 * 3.14159265358979323846 * 1200.0 / sampleRate;
    const double scSin = std::sin(scW0);
    const double scCos = std::cos(scW0);
    const double scAlpha = scSin / (2.0 * 1.2);
    const double scA0 = 1.0 + scAlpha;
    BiquadCoefficients bpCoeffs{};
    bpCoeffs.b0 = scAlpha / scA0;
    bpCoeffs.b1 = 0.0;
    bpCoeffs.b2 = -scAlpha / scA0;
    bpCoeffs.a1 = (-2.0 * scCos) / scA0;
    bpCoeffs.a2 = (1.0 - scAlpha) / scA0;
    extSidechainBp.setCoefficients(bpCoeffs);
  }

  double sampleRate = 48000.0;
  std::array<double, kMaxDelaySamples> wetBufL{};
  std::array<double, kMaxDelaySamples> wetBufR{};
  std::array<double, kMaxDelaySamples> dryZeroBufL{};
  std::array<double, kMaxDelaySamples> dryZeroBufR{};
  int writePos = 0;

  double fbStateL = 0.0, fbStateR = 0.0;
  double dcInL = 0.0, dcOutL = 0.0;
  double dcInR = 0.0, dcOutR = 0.0;
  double dcR = 0.997;
  double bbdLpL = 0.0, bbdLpR = 0.0;
  double bbdLpAlpha = 0.75;
  double compEnvInL = 0.1, compEnvInR = 0.1;
  double compEnvOutL = 0.1, compEnvOutR = 0.1;

  Biquad hpfWetL, hpfWetR;
  Biquad extSidechainBp;

  bool enabled = true;
  bool stereoMode = true;
  bool zeroCrossing = false;
  double mix = 0.45;
  double regen = 0.52;
  bool negativeRegen = false;

  bool manualModeEnabled = false;
  bool envModeEnabled = false;
  bool autoModeEnabled = true;

  double manualValue = 0.32;
  double envThresholdDb = -24.0;
  double envDecayMs = 240.0;
  bool extEnvSource = false;

  double autoRateHz = 0.35;
  bool tempoSync = false;
  double hostBpm = 120.0;
  double autoDepth = 0.55;
  bool reverseSweepOp2 = false;

  double hiPassHz = 15.0; // 15 Hz = OFF

  bool addToManual = false;
  double funcAmount = 0.65;
  double funcRateHz = 0.80;
  bool funcBipolar = false;
  int numFuncNodes = 12;
  std::array<FuncNode, kMaxFuncNodes> funcNodes{};
  int currentFuncPreset = 1;
  int activeDrawingTool = -1;
  uint32_t rngState = 0xB120F1A9u;

  AutoShape autoShape = AutoShape::Orig;
  double stereoOffset = 0.0;
  int currentPresetIndex = 0;

  double lfoPhase = 0.0;
  double funcPhase = 0.0;
  double envFollower = 0.0;
  double smoothDelayMsL = 1.8;
  double smoothDelayMsR = 1.8;

  std::atomic<bool> clipFlag{false};
  std::atomic<float> clipMeter{0.0f};
  std::atomic<float> controlModMeterL{0.0f};
  std::atomic<float> controlModMeterR{0.0f};
  std::atomic<float> envTelemetry{0.0f};
  std::atomic<float> autoTelemetry{0.0f};
  std::atomic<float> funcPlayhead{0.0f};
  std::atomic<float> telemetryDelayMsL{1.8f};
  std::atomic<float> telemetryDelayMsR{1.8f};
};

} // namespace nova::graviton
