#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// NOVA HD 2290 — Dynamic Digital Delay Processor (TC Electronic TC 2290 / DT)
// - Up to 9999 ms delay time (pre-allocated 960,000 samples @ 96 kHz)
// - 3 Independent Modulation Engines (Delay Mod, Pan Mod, Dyn Mod)
// - 4 Modulation Waveforms: SINE, RAND, ENV, TRIG
// - DAW Tempo Sync (Straight / Dotted / Triplet, 1/1 .. 1/32) + Tap LEARN
// - Feedback Network with High-Cut, Low-Cut & Polarity Invert (INV.)
// - Special Parameters: Input/Output Level, Mix, Mute, Pan, INV.DLY,
//   INV.DLY MOD, DEEP MOD (Golden Ratio bypass), 4 Thresholds
// - 12 Factory Presets + Compare Snapshot + Favorite Assign
// ==============================================================================
class TC2290Delay final {
public:
  static constexpr int kMaxDelaySamples = 960000; // 10.0 s @ 96 kHz
  static constexpr size_t kNumPresets = 32;

  enum class ModWaveform { Sine = 0, Rand = 1, Env = 2, Trig = 3 };
  enum class ModTarget { Delay = 0, Pan = 1, Dyn = 2 };
  enum class PanTarget { Delay = 0, Direct = 1, Both = 2, Off = 3 };
  enum class MuteMode { None = 0, Delay = 1, Direct = 2, Both = 3 };
  enum class SyncSubdiv {
    Div1_1 = 0,
    Div1_2 = 1,
    Div1_4 = 2,
    Div1_8 = 3,
    Div1_16 = 4,
    Div1_32 = 5,
    Div1_64 = 6
  };
  enum class SyncMode { Straight = 0, Dotted = 1, Triplet = 2 };
  enum class FeedbackSelect { Level = 0, High = 1, Low = 2 };

  struct Preset {
    const char *name;
    const char *category;
    double delayTimeMs;
    double feedbackPct;
    bool feedbackInvert;
    double highCutHz;
    double lowCutHz;
    double mixPct;
    bool delayModOn;
    double dlyModSpeedHz;
    double dlyModDepthPct;
    ModWaveform dlyModWave;
    bool panModOn;
    double panModSpeedHz;
    double panModDepthPct;
    ModWaveform panModWave;
    PanTarget panTarget;
    bool dynModOn;
    double dynModSpeedHz;
    double dynModDepthPct;
    ModWaveform dynModWave;
    bool dynReverse;
    double dynThreshDb;
    bool invDelayOut;
    bool deepMod;
    bool tempoSync;
    SyncSubdiv syncSubdiv;
    SyncMode syncMode;
  };

  TC2290Delay() {
    bufferL.resize(kMaxDelaySamples, 0.0);
    bufferR.resize(kMaxDelaySamples, 0.0);
    reset();
    loadPreset(0);
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
      sampleRate = sr;
      updateFilters();
    }
  }
  double getSampleRate() const noexcept { return sampleRate; }

  void reset() noexcept {
    std::fill(bufferL.begin(), bufferL.end(), 0.0);
    std::fill(bufferR.begin(), bufferR.end(), 0.0);
    writePos = 0;

    dlyLfoPhase = 0.0;
    panLfoPhase = 0.0;
    dynLfoPhase = 0.0;

    randHoldDly = 0.0;
    randTargetDly = 0.0;
    randHoldPan = 0.0;
    randTargetPan = 0.0;
    randHoldDyn = 0.0;
    randTargetDyn = 0.0;

    envDetector = 0.0;
    trigEnvDly = 0.0;
    trigEnvPan = 0.0;
    trigEnvDyn = 0.0;

    smoothDelayMs = delayTimeMs;
    smoothMix = mix;
    smoothFeedback = feedback;
    smoothInGain = dbToGain(inputLevelDb);
    smoothOutGain = dbToGain(outputLevelDb);

    lowCutFilterL = {};
    lowCutFilterR = {};
    highCutFilterL = {};
    highCutFilterR = {};
    updateFilters();

    meterInL.store(0.0f, std::memory_order_relaxed);
    meterInR.store(0.0f, std::memory_order_relaxed);
    meterOutL.store(0.0f, std::memory_order_relaxed);
    meterOutR.store(0.0f, std::memory_order_relaxed);
    trigActiveFlag.store(false, std::memory_order_relaxed);
  }

  // --- Header Controls ---
  void setEnabled(bool on) noexcept { enabled = on; }
  bool isEnabled() const noexcept { return enabled; }

  void setFavorite(int idx, bool fav) noexcept {
    if (idx >= 0 && idx < static_cast<int>(kNumPresets))
      favorites[static_cast<size_t>(idx)] = fav;
  }
  bool isFavorite(int idx) const noexcept {
    return (idx >= 0 && idx < static_cast<int>(kNumPresets))
               ? favorites[static_cast<size_t>(idx)]
               : false;
  }

  // --- Main Delay Section ---
  void setDelayTimeMs(double ms) noexcept {
    delayTimeMs = std::clamp(ms, 0.0, 9999.0);
  }
  double getDelayTimeMs() const noexcept {
    if (!tempoSync)
      return delayTimeMs;
    return computeSyncedDelayMs();
  }
  double getRawDelayTimeMs() const noexcept { return delayTimeMs; }

  void setDirectEnabled(bool on) noexcept { directEnabled = on; }
  bool isDirectEnabled() const noexcept { return directEnabled; }

  void setDelayEnabled(bool on) noexcept { delayEnabled = on; }
  bool isDelayEnabled() const noexcept { return delayEnabled; }

  void setDelayModEnabled(bool on) noexcept { delayModOn = on; }
  bool isDelayModEnabled() const noexcept { return delayModOn; }

  void setTempoSync(bool on) noexcept { tempoSync = on; }
  bool isTempoSync() const noexcept { return tempoSync; }

  void setHostBpm(double bpm) noexcept {
    hostBpm = std::clamp(bpm, 40.0, 260.0);
  }
  double getHostBpm() const noexcept { return hostBpm; }

  void setSyncSubdiv(SyncSubdiv s) noexcept { syncSubdiv = s; }
  SyncSubdiv getSyncSubdiv() const noexcept { return syncSubdiv; }

  void setSyncMode(SyncMode m) noexcept { syncMode = m; }
  SyncMode getSyncMode() const noexcept { return syncMode; }

  // --- Feedback Section ---
  void setFeedback(double pct) noexcept {
    feedback = std::clamp(pct, -99.0, 99.0) * 0.01;
  }
  double getFeedback() const noexcept { return feedback * 100.0; }

  void setFeedbackInvert(bool inv) noexcept { feedbackInvert = inv; }
  bool getFeedbackInvert() const noexcept { return feedbackInvert; }

  void setHighCutHz(double hz) noexcept {
    highCutHz = std::clamp(hz, 1000.0, 20000.0);
    updateFilters();
  }
  double getHighCutHz() const noexcept { return highCutHz; }

  void setLowCutHz(double hz) noexcept {
    lowCutHz = std::clamp(hz, 20.0, 1000.0);
    updateFilters();
  }
  double getLowCutHz() const noexcept { return lowCutHz; }

  void setFeedbackSelect(FeedbackSelect sel) noexcept { fbSelect = sel; }
  FeedbackSelect getFeedbackSelect() const noexcept { return fbSelect; }

  // --- Modulation Engine (3 Targets: Delay / Pan / Dyn) ---
  void setModTargetSelect(ModTarget t) noexcept { modSelect = t; }
  ModTarget getModTargetSelect() const noexcept { return modSelect; }

  void setTargetModSpeedHz(ModTarget t, double hz) noexcept {
    const double v = std::clamp(hz, 0.05, 10.0);
    if (t == ModTarget::Delay)
      dlyModSpeedHz = v;
    else if (t == ModTarget::Pan)
      panModSpeedHz = v;
    else
      dynModSpeedHz = v;
  }
  double getTargetModSpeedHz(ModTarget t) const noexcept {
    if (t == ModTarget::Delay)
      return dlyModSpeedHz;
    if (t == ModTarget::Pan)
      return panModSpeedHz;
    return dynModSpeedHz;
  }

  void setTargetModDepthPct(ModTarget t, double pct) noexcept {
    const double v = std::clamp(pct, 0.0, 99.0);
    if (t == ModTarget::Delay)
      dlyModDepthPct = v;
    else if (t == ModTarget::Pan)
      panModDepthPct = v;
    else
      dynModDepthPct = v;
  }
  double getTargetModDepthPct(ModTarget t) const noexcept {
    if (t == ModTarget::Delay)
      return dlyModDepthPct;
    if (t == ModTarget::Pan)
      return panModDepthPct;
    return dynModDepthPct;
  }

  void setTargetModWaveform(ModTarget t, ModWaveform w) noexcept {
    if (t == ModTarget::Delay)
      dlyModWave = w;
    else if (t == ModTarget::Pan)
      panModWave = w;
    else
      dynModWave = w;
  }
  ModWaveform getTargetModWaveform(ModTarget t) const noexcept {
    if (t == ModTarget::Delay)
      return dlyModWave;
    if (t == ModTarget::Pan)
      return panModWave;
    return dynModWave;
  }

  // Backward-compatible Mod wrappers
  void setModRateHz(double hz) noexcept {
    setTargetModSpeedHz(ModTarget::Delay, hz);
  }
  double getModRateHz() const noexcept {
    return getTargetModSpeedHz(ModTarget::Delay);
  }
  void setModDepth(double pct) noexcept {
    setTargetModDepthPct(ModTarget::Delay, pct);
  }
  double getModDepth() const noexcept {
    return getTargetModDepthPct(ModTarget::Delay);
  }

  // --- Pan - Dyn Section ---
  void setPanModEnabled(bool on) noexcept { panModOn = on; }
  bool isPanModEnabled() const noexcept { return panModOn; }

  void setDynModEnabled(bool on) noexcept { dynModOn = on; }
  bool isDynModEnabled() const noexcept { return dynModOn; }

  void setPanTarget(PanTarget pt) noexcept { panTarget = pt; }
  PanTarget getPanTarget() const noexcept { return panTarget; }

  void setDynReverse(bool rev) noexcept { dynReverse = rev; }
  bool isDynReverse() const noexcept { return dynReverse; }

  // Backward-compatible Ducking wrappers
  void setDuckingEnabled(bool enable) noexcept { dynModOn = enable; }
  bool getDuckingEnabled() const noexcept { return dynModOn; }
  void setDuckingThresholdDb(double db) noexcept {
    threshDynDlyVolDb = std::clamp(db, -60.0, 0.0);
  }
  double getDuckingThresholdDb() const noexcept { return threshDynDlyVolDb; }
  void setDuckingAmountDb(double db) noexcept {
    const double norm = std::clamp(-db / 40.0, 0.0, 1.0);
    dynModDepthPct = norm * 99.0;
  }
  double getDuckingAmountDb() const noexcept {
    return -(dynModDepthPct / 99.0) * 40.0;
  }

  // --- Special Parameters (Right Panel) ---
  void setInputLevelDb(double db) noexcept {
    inputLevelDb = std::clamp(db, -24.0, 12.0);
  }
  double getInputLevelDb() const noexcept { return inputLevelDb; }

  void setMix(double pct) noexcept { mix = std::clamp(pct, 0.0, 100.0) * 0.01; }
  double getMix() const noexcept { return mix * 100.0; }

  void setMuteMode(MuteMode m) noexcept { muteMode = m; }
  MuteMode getMuteMode() const noexcept { return muteMode; }

  void setOutputLevelDb(double db) noexcept {
    outputLevelDb = std::clamp(db, -24.0, 12.0);
  }
  double getOutputLevelDb() const noexcept { return outputLevelDb; }

  void setOutputPan(double pan) noexcept {
    outputPan = std::clamp(pan, -50.0, 50.0);
  }
  double getOutputPan() const noexcept { return outputPan; }

  void setInvertDelayOutput(bool inv) noexcept { invDelayOut = inv; }
  bool isInvertDelayOutput() const noexcept { return invDelayOut; }

  void setInvertDelayMod(bool inv) noexcept { invDelayMod = inv; }
  bool isInvertDelayMod() const noexcept { return invDelayMod; }

  void setDeepMod(bool deep) noexcept { deepMod = deep; }
  bool isDeepMod() const noexcept { return deepMod; }

  void setThresholdDlyModDb(double db) noexcept {
    threshDlyModDb = std::clamp(db, -60.0, 0.0);
  }
  double getThresholdDlyModDb() const noexcept { return threshDlyModDb; }

  void setThresholdPanModDb(double db) noexcept {
    threshPanModDb = std::clamp(db, -60.0, 0.0);
  }
  double getThresholdPanModDb() const noexcept { return threshPanModDb; }

  void setThresholdDynDlyVolDb(double db) noexcept {
    threshDynDlyVolDb = std::clamp(db, -60.0, 0.0);
  }
  double getThresholdDynDlyVolDb() const noexcept { return threshDynDlyVolDb; }

  void setThresholdDynFbDb(double db) noexcept {
    threshDynFbDb = std::clamp(db, -60.0, 0.0);
  }
  double getThresholdDynFbDb() const noexcept { return threshDynFbDb; }

  // TC 2290-DT 9-step threshold mapping (Step 1 = -27 dB .. Step 9 = -3 dB, 3
  // dB/step)
  void setThresholdStep(int idx, int step1to9) noexcept {
    const int s = std::clamp(step1to9, 1, 9);
    const double db = -30.0 + static_cast<double>(s) * 3.0;
    if (idx == 0)
      setThresholdDlyModDb(db);
    else if (idx == 1)
      setThresholdPanModDb(db);
    else if (idx == 2)
      setThresholdDynDlyVolDb(db);
    else if (idx == 3)
      setThresholdDynFbDb(db);
  }
  int getThresholdStep(int idx) const noexcept {
    double db = -18.0;
    if (idx == 0)
      db = threshDlyModDb;
    else if (idx == 1)
      db = threshPanModDb;
    else if (idx == 2)
      db = threshDynDlyVolDb;
    else if (idx == 3)
      db = threshDynFbDb;
    return std::clamp(static_cast<int>(std::round((db + 30.0) / 3.0)), 1, 9);
  }

  // --- Meter Telemetry ---
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
  bool isTrigActive() const noexcept {
    return trigActiveFlag.load(std::memory_order_relaxed);
  }
  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  // --- 32 Official TC 2290 Presets (Bank A: 80-99 Original, Bank B: Studio
  // Essentials, Bank C: Creative) ---
  static const std::array<Preset, kNumPresets> &getFactoryPresets() noexcept {
    static const std::array<Preset, kNumPresets> kPresets = {
        {// Bank A — 2290 Original (80–99)
         {"80 Long Delay",
          "2290 Original",
          1023.0,
          0.0,
          false,
          12000.0,
          60.0,
          45.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_4,
          SyncMode::Straight},

         {"81 Chorus + Pan",
          "2290 Original",
          50.0,
          15.0,
          false,
          10000.0,
          80.0,
          50.0,
          true,
          1.1,
          35.0,
          ModWaveform::Sine,
          true,
          0.7,
          70.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"82 Dyn Delay Trig",
          "2290 Original",
          850.0,
          25.0,
          false,
          7800.0,
          100.0,
          50.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.2,
          65.0,
          ModWaveform::Trig,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_4,
          SyncMode::Straight},

         {"83 Preset 82 + Pan",
          "2290 Original",
          850.0,
          25.0,
          false,
          7800.0,
          100.0,
          50.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          true,
          0.8,
          75.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.2,
          65.0,
          ModWaveform::Trig,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_4,
          SyncMode::Straight},

         {"84 Delayed Dyn Chorus",
          "2290 Original",
          300.0,
          20.0,
          false,
          9000.0,
          90.0,
          48.0,
          true,
          1.3,
          40.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.0,
          55.0,
          ModWaveform::Trig,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"85 Chorus",
          "2290 Original",
          20.0,
          0.0,
          false,
          11000.0,
          70.0,
          50.0,
          true,
          1.4,
          48.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"86 Flanger I",
          "2290 Original",
          10.0,
          50.0,
          false,
          12000.0,
          60.0,
          50.0,
          true,
          0.45,
          70.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"87 Delay Dyn Reversed",
          "2290 Original",
          200.0,
          50.0,
          false,
          8500.0,
          120.0,
          52.0,
          true,
          0.9,
          30.0,
          ModWaveform::Sine,
          true,
          1.1,
          60.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.4,
          75.0,
          ModWaveform::Trig,
          true,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"88 Dyn Delay + Pan Env",
          "2290 Original",
          500.0,
          25.0,
          false,
          8200.0,
          100.0,
          50.0,
          true,
          0.8,
          25.0,
          ModWaveform::Sine,
          true,
          1.0,
          75.0,
          ModWaveform::Env,
          PanTarget::Delay,
          true,
          1.1,
          55.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_4,
          SyncMode::Straight},

         {"89 Dyn Delay + Pan Env II",
          "2290 Original",
          300.0,
          25.0,
          false,
          8200.0,
          100.0,
          50.0,
          true,
          0.9,
          28.0,
          ModWaveform::Sine,
          true,
          1.2,
          75.0,
          ModWaveform::Env,
          PanTarget::Delay,
          true,
          1.2,
          55.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"90 Dyn Delay + Pan Env III",
          "2290 Original",
          250.0,
          25.0,
          false,
          8200.0,
          100.0,
          50.0,
          true,
          1.0,
          30.0,
          ModWaveform::Sine,
          true,
          1.3,
          75.0,
          ModWaveform::Env,
          PanTarget::Delay,
          true,
          1.3,
          55.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"91 Flanger II",
          "2290 Original",
          20.0,
          55.0,
          true,
          12000.0,
          60.0,
          50.0,
          true,
          0.35,
          78.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          true,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"92 AutoPan Direct",
          "2290 Original",
          0.0,
          0.0,
          false,
          16000.0,
          30.0,
          0.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          true,
          1.6,
          95.0,
          ModWaveform::Sine,
          PanTarget::Direct,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"93 Tremolo Direct",
          "2290 Original",
          0.0,
          0.0,
          false,
          16000.0,
          30.0,
          0.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Direct,
          true,
          3.8,
          70.0,
          ModWaveform::Sine,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"94 AutoPan + Tremolo Dir",
          "2290 Original",
          0.0,
          0.0,
          false,
          16000.0,
          30.0,
          0.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          true,
          1.8,
          90.0,
          ModWaveform::Sine,
          PanTarget::Direct,
          true,
          3.6,
          65.0,
          ModWaveform::Sine,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"95 Doubler I",
          "2290 Original",
          80.0,
          25.0,
          false,
          9500.0,
          100.0,
          48.0,
          true,
          1.2,
          32.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.0,
          40.0,
          ModWaveform::Trig,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"96 Delay Short + Dir Pan",
          "2290 Original",
          100.0,
          35.0,
          false,
          9200.0,
          110.0,
          45.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          true,
          1.4,
          80.0,
          ModWaveform::Sine,
          PanTarget::Direct,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"97 Doubler II",
          "2290 Original",
          30.0,
          15.0,
          false,
          10000.0,
          90.0,
          50.0,
          true,
          1.5,
          36.0,
          ModWaveform::Sine,
          true,
          0.9,
          65.0,
          ModWaveform::Sine,
          PanTarget::Both,
          true,
          1.1,
          35.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"98 Chorus + Pan + Tremolo",
          "2290 Original",
          10.0,
          0.0,
          false,
          11000.0,
          80.0,
          50.0,
          true,
          1.3,
          45.0,
          ModWaveform::Sine,
          true,
          1.1,
          75.0,
          ModWaveform::Sine,
          PanTarget::Both,
          true,
          3.2,
          55.0,
          ModWaveform::Sine,
          false,
          -21.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"99 Flanger III",
          "2290 Original",
          1.8,
          78.0,
          true,
          12500.0,
          50.0,
          50.0,
          true,
          0.28,
          85.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -21.0,
          false,
          true,
          false,
          SyncSubdiv::Div1_32,
          SyncMode::Straight},

         // Bank B — Studio Essentials
         {"Vocal Slap",
          "Studio Essentials",
          112.0,
          10.0,
          false,
          7800.0,
          180.0,
          38.0,
          true,
          1.1,
          18.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Trig,
          false,
          -24.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"Vocal Ducking",
          "Studio Essentials",
          480.0,
          42.0,
          false,
          6800.0,
          160.0,
          52.0,
          true,
          0.8,
          16.0,
          ModWaveform::Sine,
          true,
          0.6,
          40.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.4,
          72.0,
          ModWaveform::Trig,
          false,
          -24.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_4,
          SyncMode::Straight},

         {"Wide Vocal Doubler",
          "Studio Essentials",
          24.0,
          8.0,
          false,
          10500.0,
          120.0,
          46.0,
          true,
          1.3,
          32.0,
          ModWaveform::Sine,
          true,
          0.5,
          55.0,
          ModWaveform::Sine,
          PanTarget::Both,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -24.0,
          true,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"Snare Ambience",
          "Studio Essentials",
          220.0,
          26.0,
          false,
          11000.0,
          240.0,
          48.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          2.2,
          78.0,
          ModWaveform::Trig,
          true,
          -18.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"Stereo Guitar Delay",
          "Studio Essentials",
          375.0,
          44.0,
          false,
          6500.0,
          140.0,
          44.0,
          true,
          0.7,
          20.0,
          ModWaveform::Sine,
          true,
          0.9,
          60.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          1.0,
          35.0,
          ModWaveform::Trig,
          false,
          -24.0,
          false,
          false,
          true,
          SyncSubdiv::Div1_8,
          SyncMode::Dotted},

         {"Piano Chorus",
          "Studio Essentials",
          22.0,
          12.0,
          false,
          12000.0,
          80.0,
          42.0,
          true,
          0.85,
          38.0,
          ModWaveform::Sine,
          true,
          0.4,
          45.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Env,
          false,
          -24.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         // Bank C — Creative Effects
         {"Reverse Mod Delay",
          "Creative Effects",
          340.0,
          48.0,
          true,
          8200.0,
          130.0,
          54.0,
          true,
          1.5,
          55.0,
          ModWaveform::Env,
          true,
          1.1,
          65.0,
          ModWaveform::Sine,
          PanTarget::Both,
          true,
          1.6,
          65.0,
          ModWaveform::Trig,
          true,
          -21.0,
          true,
          true,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"Infinite Feedback",
          "Creative Effects",
          500.0,
          88.0,
          false,
          5400.0,
          180.0,
          52.0,
          true,
          0.35,
          25.0,
          ModWaveform::Sine,
          true,
          0.45,
          70.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          0.8,
          50.0,
          ModWaveform::Env,
          false,
          -24.0,
          false,
          false,
          true,
          SyncSubdiv::Div1_4,
          SyncMode::Straight},

         {"Rhythmic Pan Delay",
          "Creative Effects",
          375.0,
          48.0,
          false,
          8000.0,
          120.0,
          50.0,
          true,
          0.6,
          15.0,
          ModWaveform::Sine,
          true,
          2.0,
          95.0,
          ModWaveform::Sine,
          PanTarget::Both,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          -24.0,
          false,
          false,
          true,
          SyncSubdiv::Div1_8,
          SyncMode::Dotted},

         {"Gated Echo",
          "Creative Effects",
          280.0,
          36.0,
          false,
          9500.0,
          160.0,
          55.0,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          false,
          1.0,
          0.0,
          ModWaveform::Sine,
          PanTarget::Delay,
          true,
          2.5,
          85.0,
          ModWaveform::Trig,
          true,
          -18.0,
          false,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight},

         {"Random Pitch Delay",
          "Creative Effects",
          160.0,
          38.0,
          false,
          11000.0,
          90.0,
          52.0,
          true,
          2.8,
          72.0,
          ModWaveform::Rand,
          true,
          1.7,
          70.0,
          ModWaveform::Rand,
          PanTarget::Delay,
          false,
          1.0,
          0.0,
          ModWaveform::Rand,
          false,
          -21.0,
          false,
          true,
          false,
          SyncSubdiv::Div1_16,
          SyncMode::Straight},

         {"Stereo Motion",
          "Creative Effects",
          420.0,
          42.0,
          false,
          8800.0,
          100.0,
          50.0,
          true,
          0.9,
          30.0,
          ModWaveform::Sine,
          true,
          1.4,
          92.0,
          ModWaveform::Sine,
          PanTarget::Both,
          true,
          2.8,
          55.0,
          ModWaveform::Sine,
          true,
          -21.0,
          true,
          false,
          false,
          SyncSubdiv::Div1_8,
          SyncMode::Straight}}};
    return kPresets;
  }

  void loadPreset(int index) noexcept {
    const auto &presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
      return;
    currentPresetIndex = index;
    const auto &p = presets[static_cast<size_t>(index)];
    delayTimeMs = p.delayTimeMs;
    feedback = p.feedbackPct * 0.01;
    feedbackInvert = p.feedbackInvert;
    highCutHz = p.highCutHz;
    lowCutHz = p.lowCutHz;
    mix = p.mixPct * 0.01;
    delayModOn = p.delayModOn;
    dlyModSpeedHz = p.dlyModSpeedHz;
    dlyModDepthPct = p.dlyModDepthPct;
    dlyModWave = p.dlyModWave;
    panModOn = p.panModOn;
    panModSpeedHz = p.panModSpeedHz;
    panModDepthPct = p.panModDepthPct;
    panModWave = p.panModWave;
    panTarget = p.panTarget;
    dynModOn = p.dynModOn;
    dynModSpeedHz = p.dynModSpeedHz;
    dynModDepthPct = p.dynModDepthPct;
    dynModWave = p.dynModWave;
    dynReverse = p.dynReverse;
    threshDynDlyVolDb = p.dynThreshDb;
    invDelayOut = p.invDelayOut;
    deepMod = p.deepMod;
    tempoSync = p.tempoSync;
    syncSubdiv = p.syncSubdiv;
    syncMode = p.syncMode;
    delayEnabled = true;
    directEnabled = true;
    muteMode = MuteMode::None;
    updateFilters();
  }

  // --- 64-Bit Real-Time Audio Processing ---
  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double rawInL = sanitize(inOutL);
    const double rawInR = sanitize(inOutR);

    // Input Meter L/R (pre-Input Level per TC 2290 spec)
    const double meterDecay = std::exp(-1.0 / (sampleRate * 0.08));
    float curInL = meterInL.load(std::memory_order_relaxed);
    float curInR = meterInR.load(std::memory_order_relaxed);
    meterInL.store(std::max(static_cast<float>(std::abs(rawInL)),
                            curInL * static_cast<float>(meterDecay)),
                   std::memory_order_relaxed);
    meterInR.store(std::max(static_cast<float>(std::abs(rawInR)),
                            curInR * static_cast<float>(meterDecay)),
                   std::memory_order_relaxed);

    if (!enabled) {
      inOutL = rawInL;
      inOutR = rawInR;
      meterOutL.store(meterInL.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      meterOutR.store(meterInR.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      return;
    }

    const double sr = std::max(8000.0, sampleRate);
    constexpr double twoPi = 2.0 * std::numbers::pi;

    // 1. Parameter Smoothers
    const double smoothAlpha = 1.0 - std::exp(-1.0 / (sr * 0.015));
    const double targetDelayMs = getDelayTimeMs();
    smoothDelayMs += smoothAlpha * (targetDelayMs - smoothDelayMs);
    smoothMix += smoothAlpha * (mix - smoothMix);
    smoothFeedback += smoothAlpha * (feedback - smoothFeedback);
    smoothInGain += smoothAlpha * (dbToGain(inputLevelDb) - smoothInGain);
    smoothOutGain += smoothAlpha * (dbToGain(outputLevelDb) - smoothOutGain);

    const double dryL = rawInL * smoothInGain;
    const double dryR = rawInR * smoothInGain;

    // 2. Envelope & Trigger Detector
    const double inLevel = std::max(std::abs(dryL), std::abs(dryR));
    const double envCoeff = (inLevel > envDetector)
                                ? std::exp(-1.0 / (sr * 0.005))
                                : std::exp(-1.0 / (sr * 0.140));
    envDetector = envCoeff * envDetector + (1.0 - envCoeff) * inLevel;
    const double envDb = gainToDb(envDetector, -80.0);

    const bool trigDly = envDb > threshDlyModDb;
    const bool trigPan = envDb > threshPanModDb;
    trigActiveFlag.store(trigDly || trigPan, std::memory_order_relaxed);

    const double trigAtk = 1.0 - std::exp(-1.0 / (sr * 0.012));
    const double dynRecSec =
        std::clamp(0.40 / std::max(0.1, dynModSpeedHz), 0.02, 1.5);
    const double trigRel = 1.0 - std::exp(-1.0 / (sr * 0.250));
    const double dynRel = 1.0 - std::exp(-1.0 / (sr * dynRecSec));
    trigEnvDly +=
        (trigDly ? trigAtk : trigRel) * ((trigDly ? 1.0 : 0.0) - trigEnvDly);
    trigEnvPan +=
        (trigPan ? trigAtk : trigRel) * ((trigPan ? 1.0 : 0.0) - trigEnvPan);
    const bool trigDyn = envDb > threshDynDlyVolDb;
    trigEnvDyn +=
        (trigDyn ? trigAtk : dynRel) * ((trigDyn ? 1.0 : 0.0) - trigEnvDyn);

    // 3. Modulation Waveform Evaluators
    auto evalWave = [&](ModWaveform w, double &phase, double speedHz,
                        double &randHold, double &randTarget,
                        double trigVal) noexcept -> double {
      phase += speedHz / sr;
      if (phase >= 1.0) {
        phase -= 1.0;
        rngState = rngState * 1664525u + 1013904223u;
        randTarget =
            (static_cast<double>((rngState >> 8) & 0xFFFF) / 32767.5) - 1.0;
      }
      randHold +=
          (1.0 - std::exp(-1.0 / (sr * 0.025))) * (randTarget - randHold);

      if (w == ModWaveform::Sine)
        return std::sin(phase * twoPi);
      if (w == ModWaveform::Rand)
        return randHold;
      if (w == ModWaveform::Env)
        return std::clamp(envDetector * 2.5, 0.0, 1.0) * 2.0 - 1.0;
      return trigVal * 2.0 - 1.0; // Trig
    };

    double dlyLfo = delayModOn
                        ? evalWave(dlyModWave, dlyLfoPhase, dlyModSpeedHz,
                                   randHoldDly, randTargetDly, trigEnvDly)
                        : 0.0;
    if (invDelayMod &&
        (dlyModWave == ModWaveform::Env || dlyModWave == ModWaveform::Trig))
      dlyLfo = -dlyLfo;

    const double panLfo = panModOn
                              ? evalWave(panModWave, panLfoPhase, panModSpeedHz,
                                         randHoldPan, randTargetPan, trigEnvPan)
                              : 0.0;

    const double dynLfo = dynModOn
                              ? evalWave(dynModWave, dynLfoPhase, dynModSpeedHz,
                                         randHoldDyn, randTargetDyn, trigEnvDyn)
                              : 0.0;

    // 4. Delay Modulation Excursion (Golden Ratio or Deep Mod)
    const double maxExcursionMs =
        deepMod ? 16.0 : 6.18; // 0.618 Golden Ratio scaling
    const double dlyExcursion =
        (dlyModDepthPct * 0.01) * maxExcursionMs * dlyLfo;
    const double dlySignR = invDelayMod ? -1.0 : 1.0;

    const double effDelayMsL =
        std::clamp(smoothDelayMs + dlyExcursion, 0.0, 9999.0);
    const double effDelayMsR =
        std::clamp(smoothDelayMs + dlySignR * dlyExcursion, 0.0, 9999.0);

    const double delayedL =
        (effDelayMsL < 0.02)
            ? dryL
            : readInterpolated(bufferL, effDelayMsL * 0.001 * sr);
    const double delayedR =
        (effDelayMsR < 0.02)
            ? dryR
            : readInterpolated(bufferR, effDelayMsR * 0.001 * sr);

    // 5. Feedback Filters (High Cut & Low Cut) + Dynamic Volume / Feedback
    // Modulation
    double filteredL = highCutFilterL.process(lowCutFilterL.process(delayedL));
    double filteredR = highCutFilterR.process(lowCutFilterR.process(delayedR));

    double dynVolGain = 1.0;
    double dynDirGain = 1.0;
    double dynFbGain = 1.0;
    if (dynModOn) {
      const double depthNorm = dynModDepthPct * 0.01;
      if (dynModWave == ModWaveform::Sine || dynModWave == ModWaveform::Rand) {
        // SINE / RAND: Tremolo on Direct and Delay (REVERSE flips phase between
        // Direct & Delay)
        const double modAmt = 0.5 * (1.0 - dynLfo) * depthNorm;
        dynVolGain = std::clamp(1.0 - modAmt, 0.05, 1.0);
        if (dynReverse) {
          const double revAmt = 0.5 * (1.0 + dynLfo) * depthNorm;
          dynDirGain = std::clamp(1.0 - revAmt, 0.05, 1.0);
        } else {
          dynDirGain = dynVolGain;
        }
      } else if (dynModWave == ModWaveform::Env) {
        // ENV: Compression (normal) vs Expansion (REVERSE) with Speed recovery
        const double envNorm =
            std::clamp((envDb - threshDynDlyVolDb) / 24.0, 0.0, 1.0);
        if (!dynReverse) {
          dynVolGain = std::clamp(1.0 - envNorm * depthNorm, 0.05, 1.0);
        } else {
          dynVolGain = std::clamp(
              (1.0 - depthNorm * 0.85) + envNorm * depthNorm * 0.85, 0.05, 1.0);
        }
      } else {
        // TRIG: Ducking (normal) vs Gating (REVERSE) + Feedback Modulation
        if (!dynReverse) {
          dynVolGain = std::clamp(1.0 - trigEnvDyn * depthNorm, 0.02, 1.0);
        } else {
          dynVolGain = std::clamp((1.0 - depthNorm * 0.90) +
                                      trigEnvDyn * depthNorm * 0.90,
                                  0.05, 1.0);
        }
      }

      if (envDb > threshDynFbDb) {
        const double overFb =
            std::clamp((envDb - threshDynFbDb) / 24.0, 0.0, 1.0);
        dynFbGain = dynReverse ? (1.0 + 0.25 * overFb * depthNorm)
                               : (1.0 - 0.50 * overFb * depthNorm);
      }
    }

    double fbMult = smoothFeedback * dynFbGain;
    if (feedbackInvert)
      fbMult = -fbMult;
    fbMult = std::clamp(fbMult, -0.96, 0.96);

    bufferL[static_cast<size_t>(writePos)] =
        sanitize(std::tanh(dryL + filteredL * fbMult));
    bufferR[static_cast<size_t>(writePos)] =
        sanitize(std::tanh(dryR + filteredR * fbMult));
    writePos = (writePos + 1) % kMaxDelaySamples;

    // 6. Wet & Direct Mutes + Polarity Invert (INV. DLY)
    const bool muteDly = !delayEnabled || (muteMode == MuteMode::Delay) ||
                         (muteMode == MuteMode::Both);
    const bool muteDir = !directEnabled || (muteMode == MuteMode::Direct) ||
                         (muteMode == MuteMode::Both);

    double wetL =
        muteDly ? 0.0 : (filteredL * dynVolGain * (invDelayOut ? -1.0 : 1.0));
    double wetR =
        muteDly ? 0.0 : (filteredR * dynVolGain * (invDelayOut ? -1.0 : 1.0));
    double dirL = muteDir ? 0.0 : (dryL * dynDirGain);
    double dirR = muteDir ? 0.0 : (dryR * dynDirGain);

    // 7. Pan Modulation (PAN MOD: Off=R-invert, Delay, Direct, Both=opposite
    // directions)
    if (panModOn) {
      if (panTarget == PanTarget::Off) {
        wetR = -wetR;
      } else if (panModDepthPct > 0.1) {
        const double panPos =
            std::clamp(panLfo * (panModDepthPct * 0.01), -1.0, 1.0);
        constexpr double norm = 1.414213562373095;
        const double gL =
            std::cos((panPos + 1.0) * 0.25 * std::numbers::pi) * norm;
        const double gR =
            std::sin((panPos + 1.0) * 0.25 * std::numbers::pi) * norm;
        if (panTarget == PanTarget::Delay) {
          wetL *= gL;
          wetR *= gR;
        } else if (panTarget == PanTarget::Direct) {
          dirL *= gL;
          dirR *= gR;
        } else if (panTarget == PanTarget::Both) {
          // Original TC 2290: Direct and Delay pan in opposite directions
          wetL *= gL;
          wetR *= gR;
          dirL *= gR;
          dirR *= gL;
        }
      }
    }

    // 8. Mix & Output Level / Output Pan
    double outL = dirL * (1.0 - smoothMix) + wetL * smoothMix;
    double outR = dirR * (1.0 - smoothMix) + wetR * smoothMix;

    const double outPanNorm = outputPan / 50.0; // -1 .. +1
    const double panGainL =
        std::clamp(1.0 - std::max(0.0, outPanNorm), 0.0, 1.0);
    const double panGainR =
        std::clamp(1.0 + std::min(0.0, outPanNorm), 0.0, 1.0);

    inOutL = sanitize(outL * smoothOutGain * panGainL);
    inOutR = sanitize(outR * smoothOutGain * panGainR);

    float curOutL = meterOutL.load(std::memory_order_relaxed);
    float curOutR = meterOutR.load(std::memory_order_relaxed);
    meterOutL.store(std::max(static_cast<float>(std::abs(inOutL)),
                             curOutL * static_cast<float>(meterDecay)),
                    std::memory_order_relaxed);
    meterOutR.store(std::max(static_cast<float>(std::abs(inOutR)),
                             curOutR * static_cast<float>(meterDecay)),
                    std::memory_order_relaxed);
  }

private:
  double computeSyncedDelayMs() const noexcept {
    const double quarterMs = 60000.0 / std::max(40.0, hostBpm);
    static constexpr double kSubdivMult[7] = {4.0,  2.0,   1.0,   0.5,
                                              0.25, 0.125, 0.0625};
    const int idx = std::clamp(static_cast<int>(syncSubdiv), 0, 6);
    double ms = quarterMs * kSubdivMult[idx];
    if (syncMode == SyncMode::Dotted)
      ms *= 1.5;
    else if (syncMode == SyncMode::Triplet)
      ms *= (2.0 / 3.0);
    return std::clamp(ms, 0.0, 9999.0);
  }

  void updateFilters() noexcept {
    const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    highCutFilterL.setCoefficients(makeButterworthLPF(sr, highCutHz, 0.7071));
    highCutFilterR.setCoefficients(makeButterworthLPF(sr, highCutHz, 0.7071));
    lowCutFilterL.setCoefficients(makeButterworthHPF(sr, lowCutHz, 0.7071));
    lowCutFilterR.setCoefficients(makeButterworthHPF(sr, lowCutHz, 0.7071));
  }

  double readInterpolated(const std::vector<double> &buf,
                          double delaySamples) const noexcept {
    const double d = std::clamp(delaySamples, 0.0,
                                static_cast<double>(kMaxDelaySamples - 2));
    const int iPart = static_cast<int>(std::floor(d));
    const double frac = d - static_cast<double>(iPart);

    int r0 = writePos - 1 - iPart;
    while (r0 < 0)
      r0 += kMaxDelaySamples;
    int r1 = r0 - 1;
    while (r1 < 0)
      r1 += kMaxDelaySamples;

    const double s0 = buf[static_cast<size_t>(r0 % kMaxDelaySamples)];
    const double s1 = buf[static_cast<size_t>(r1 % kMaxDelaySamples)];
    return s0 + frac * (s1 - s0);
  }

  std::vector<double> bufferL;
  std::vector<double> bufferR;
  int writePos = 0;
  double sampleRate = 48000.0;
  bool enabled = true;

  // Delay & Sync
  double delayTimeMs = 420.0;
  bool directEnabled = true;
  bool delayEnabled = true;
  bool tempoSync = false;
  double hostBpm = 120.0;
  SyncSubdiv syncSubdiv = SyncSubdiv::Div1_8;
  SyncMode syncMode = SyncMode::Straight;

  // Feedback
  double feedback = 0.35;
  bool feedbackInvert = false;
  double highCutHz = 8000.0;
  double lowCutHz = 80.0;
  FeedbackSelect fbSelect = FeedbackSelect::Level;

  // Modulation
  ModTarget modSelect = ModTarget::Delay;
  bool delayModOn = true;
  double dlyModSpeedHz = 1.0;
  double dlyModDepthPct = 20.0;
  ModWaveform dlyModWave = ModWaveform::Sine;

  bool panModOn = false;
  double panModSpeedHz = 1.2;
  double panModDepthPct = 50.0;
  ModWaveform panModWave = ModWaveform::Sine;
  PanTarget panTarget = PanTarget::Delay;

  bool dynModOn = false;
  double dynModSpeedHz = 1.0;
  double dynModDepthPct = 50.0;
  ModWaveform dynModWave = ModWaveform::Env;
  bool dynReverse = false;

  // Special Parameters
  double inputLevelDb = 0.0;
  double mix = 0.50;
  MuteMode muteMode = MuteMode::None;
  double outputLevelDb = 0.0;
  double outputPan = 0.0;
  bool invDelayOut = false;
  bool invDelayMod = false;
  bool deepMod = false;
  double threshDlyModDb = -24.0;
  double threshPanModDb = -24.0;
  double threshDynDlyVolDb = -22.0;
  double threshDynFbDb = -18.0;

  // Smoothers & LFO State
  double smoothDelayMs = 420.0;
  double smoothMix = 0.50;
  double smoothFeedback = 0.35;
  double smoothInGain = 1.0;
  double smoothOutGain = 1.0;

  double dlyLfoPhase = 0.0;
  double panLfoPhase = 0.0;
  double dynLfoPhase = 0.0;
  double randHoldDly = 0.0, randTargetDly = 0.0;
  double randHoldPan = 0.0, randTargetPan = 0.0;
  double randHoldDyn = 0.0, randTargetDyn = 0.0;
  uint32_t rngState = 0x22901985u;

  double envDetector = 0.0;
  double trigEnvDly = 0.0;
  double trigEnvPan = 0.0;
  double trigEnvDyn = 0.0;

  Biquad highCutFilterL, highCutFilterR;
  Biquad lowCutFilterL, lowCutFilterR;

  int currentPresetIndex = 0;
  std::array<bool, kNumPresets> favorites{};

  std::atomic<float> meterInL{0.0f};
  std::atomic<float> meterInR{0.0f};
  std::atomic<float> meterOutL{0.0f};
  std::atomic<float> meterOutR{0.0f};
  std::atomic<bool> trigActiveFlag{false};
};

} // namespace nova::graviton
