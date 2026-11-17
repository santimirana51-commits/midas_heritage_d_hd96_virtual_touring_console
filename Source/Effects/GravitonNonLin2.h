#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <numbers>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// TC Electronic System 6000 — NonLin2 Envelope & Gated / Reverse Reverb
// - Envelope Shaping: Pre Delay (P), Attack (A), Hold (H), Release (R)
// - Levels: In Gain, Out Gain, Dry Gain (with OFF mute), Wet Gain
// - Modify: Reverb Style (6 styles), Diffuse, Twist Type (6 types), Twist Ratio,
//           Lo Cut, Hi Cut, Stereo Width
// - Safe Lock, Bypass, Stereo IN/OUT Metering, 12 Factory Presets
// ==============================================================================
class TCNonLin2Processor final {
public:
  static constexpr int kMaxPreDelaySamples = 96000; // up to 1000 ms @ 96 kHz
  static constexpr int kTankSize = 192000;          // 2.0 s @ 96 kHz
  static constexpr int kNumDiffusers = 6;
  static constexpr int kNumTaps = 16;
  static constexpr size_t kNumPresets = 33;

  enum class ReverbStyle {
    NonLinA = 0,
    NonLinB = 1,
    NonLinC = 2,
    NonLinD = 3,
    Gated80s = 4,
    Reverse = 5
  };

  enum class TwistType {
    Off = 0,
    Detune = 1,
    Modulate = 2,
    Crunch = 3,
    Comb = 4,
    Phase = 5
  };

  struct Preset {
    const char *name;
    const char *category;
    double inGainDb;
    double outGainDb;
    double dryGainDb; // <= -60.0 means OFF
    double wetGainDb;
    double preDelayMs;
    double attackMs;
    double holdMs;
    double releaseMs;
    double slope;
    ReverbStyle style;
    double diffusePct;
    TwistType twistType;
    double twistRatioPct;
    double lowCutHz;
    double highCutHz;
    double widthPct;
  };

  TCNonLin2Processor() {
    preDelayBufL.assign(kMaxPreDelaySamples, 0.0);
    preDelayBufR.assign(kMaxPreDelaySamples, 0.0);
    tankBufL.assign(kTankSize, 0.0);
    tankBufR.assign(kTankSize, 0.0);
    reset();
    loadPreset(0);
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0) {
      sampleRate = sr;
      updateFilters();
    }
  }
  double getSampleRate() const noexcept { return sampleRate; }

  void reset() noexcept {
    std::fill(preDelayBufL.begin(), preDelayBufL.end(), 0.0);
    std::fill(preDelayBufR.begin(), preDelayBufR.end(), 0.0);
    preDelayWrite = 0;

    for (int i = 0; i < kNumDiffusers; ++i) {
      std::memset(diffBufL[i].data(), 0, diffBufL[i].size() * sizeof(double));
      std::memset(diffBufR[i].data(), 0, diffBufR[i].size() * sizeof(double));
      diffIdxL[i] = 0;
      diffIdxR[i] = 0;
    }

    std::fill(tankBufL.begin(), tankBufL.end(), 0.0);
    std::fill(tankBufR.begin(), tankBufR.end(), 0.0);
    tankWrite = 0;

    lowCutFilterL = {};
    lowCutFilterR = {};
    highCutFilterL = {};
    highCutFilterR = {};
    updateFilters();

    twistPhase = 0.0;
    meterInL.store(0.0f, std::memory_order_relaxed);
    meterInR.store(0.0f, std::memory_order_relaxed);
    meterOutL.store(0.0f, std::memory_order_relaxed);
    meterOutR.store(0.0f, std::memory_order_relaxed);
  }

  // --- Global Controls ---
  void setBypassed(bool b) noexcept { bypassed = b; }
  bool isBypassed() const noexcept { return bypassed; }

  void setSafeMode(bool s) noexcept { safeMode = s; }
  bool isSafeMode() const noexcept { return safeMode; }

  // --- Levels ---
  void setInGainDb(double db) noexcept {
    inGainDb = std::clamp(db, -48.0, 12.0);
    inGainLin = dbToGain(inGainDb);
  }
  double getInGainDb() const noexcept { return inGainDb; }

  void setOutGainDb(double db) noexcept {
    outGainDb = std::clamp(db, -48.0, 12.0);
    outGainLin = dbToGain(outGainDb);
  }
  double getOutGainDb() const noexcept { return outGainDb; }
  // Backward-compatible alias
  void setOutputGainDb(double db) noexcept { setOutGainDb(db); }

  void setDryGainDb(double db) noexcept {
    dryGainDb = std::clamp(db, -96.0, 6.0);
    dryGainLin = (dryGainDb <= -60.0) ? 0.0 : dbToGain(dryGainDb);
  }
  double getDryGainDb() const noexcept { return dryGainDb; }
  bool isDryOff() const noexcept { return dryGainDb <= -60.0; }

  void setWetGainDb(double db) noexcept {
    wetGainDb = std::clamp(db, -96.0, 6.0);
    wetGainLin = (wetGainDb <= -60.0) ? 0.0 : dbToGain(wetGainDb);
  }
  double getWetGainDb() const noexcept { return wetGainDb; }

  // Backward-compatible Mix (0..1)
  void setMix(double m) noexcept {
    mix = std::clamp(m, 0.0, 1.0);
    useMixMode = true;
  }
  double getMix() const noexcept { return mix; }
  void setLevelMode(bool useGainKnobs) noexcept { useMixMode = !useGainKnobs; }
  bool isUsingGainKnobs() const noexcept { return !useMixMode; }

  // --- Envelope (P - A - H - R) ---
  void setPreDelayMs(double ms) noexcept { preDelayMs = std::clamp(ms, 0.0, 500.0); }
  double getPreDelayMs() const noexcept { return preDelayMs; }

  void setAttackMs(double ms) noexcept { attackMs = std::clamp(ms, 0.0, 500.0); }
  double getAttackMs() const noexcept { return attackMs; }

  void setHoldMs(double ms) noexcept { holdMs = std::clamp(ms, 0.0, 1000.0); }
  double getHoldMs() const noexcept { return holdMs; }

  void setReleaseMs(double ms) noexcept { releaseMs = std::clamp(ms, 0.0, 1000.0); }
  double getReleaseMs() const noexcept { return releaseMs; }

  void setSlope(double s) noexcept { slope = std::clamp(s, -1.0, 1.0); }
  double getSlope() const noexcept { return slope; }

  // --- Modify Section ---
  void setReverbStyle(ReverbStyle s) noexcept { reverbStyle = s; }
  ReverbStyle getReverbStyle() const noexcept { return reverbStyle; }

  void setDiffusePct(double pct) noexcept { diffusePct = std::clamp(pct, 0.0, 100.0); }
  double getDiffusePct() const noexcept { return diffusePct; }

  void setTwistType(TwistType t) noexcept { twistType = t; }
  TwistType getTwistType() const noexcept { return twistType; }

  void setTwistRatioPct(double pct) noexcept { twistRatioPct = std::clamp(pct, 0.0, 100.0); }
  double getTwistRatioPct() const noexcept { return twistRatioPct; }

  void setLowCutHz(double hz) noexcept {
    lowCutHz = std::clamp(hz, 20.0, 1000.0);
    updateFilters();
  }
  double getLowCutHz() const noexcept { return lowCutHz; }

  void setHighCutHz(double hz) noexcept {
    highCutHz = std::clamp(hz, 1000.0, 20000.0);
    updateFilters();
  }
  double getHighCutHz() const noexcept { return highCutHz; }

  void setWidthPct(double pct) noexcept { widthPct = std::clamp(pct, 0.0, 100.0); }
  double getWidthPct() const noexcept { return widthPct; }

  // --- Meters ---
  float getMeterInL() const noexcept { return meterInL.load(std::memory_order_relaxed); }
  float getMeterInR() const noexcept { return meterInR.load(std::memory_order_relaxed); }
  float getMeterOutL() const noexcept { return meterOutL.load(std::memory_order_relaxed); }
  float getMeterOutR() const noexcept { return meterOutR.load(std::memory_order_relaxed); }
  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  // --- 33 Official TC System 6000 NonLin2 Factory Presets (301–333) ---
  static const std::array<Preset, kNumPresets> &getFactoryPresets() noexcept {
    static const std::array<Preset, kNumPresets> kPresets = {{
        {"301 Tight NonLin", "NonLin",
         0.0, 0.0, -96.0, -5.0, 10.0, 8.0, 65.0, 45.0, 0.0,
         ReverbStyle::NonLinA, 82.0, TwistType::Off, 0.0, 120.0, 6800.0, 100.0},

        {"302 AMS NonLin A", "Classic 80s",
         0.0, 0.0, -96.0, -6.0, 0.0, 36.0, 84.0, 204.0, 0.0,
         ReverbStyle::NonLinB, 78.0, TwistType::Off, 0.0, 90.0, 2360.0, 100.0},

        {"303 Garage Gate", "Drums",
         0.0, 0.0, -96.0, -4.0, 14.0, 4.0, 90.0, 38.0, 0.0,
         ReverbStyle::Gated80s, 72.0, TwistType::Crunch, 20.0, 140.0, 4800.0, 100.0},

        {"304 Cheap Spring", "Vintage",
         0.0, 0.0, -96.0, -6.0, 8.0, 12.0, 55.0, 140.0, 0.35,
         ReverbStyle::NonLinC, 42.0, TwistType::Comb, 38.0, 220.0, 3900.0, 65.0},

        {"305 Light Starburst", "FX",
         0.0, 0.0, -96.0, -5.5, 22.0, 18.0, 110.0, 180.0, 0.25,
         ReverbStyle::NonLinD, 88.0, TwistType::Modulate, 35.0, 160.0, 9500.0, 100.0},

        {"306 TajMahal", "Large",
         0.0, 0.0, -96.0, -4.5, 45.0, 40.0, 220.0, 320.0, 0.45,
         ReverbStyle::NonLinD, 92.0, TwistType::Detune, 22.0, 80.0, 5800.0, 100.0},

        {"307 AutoPan Gate", "FX / Mod",
         0.0, 0.0, -96.0, -4.0, 12.0, 6.0, 95.0, 48.0, 0.0,
         ReverbStyle::Gated80s, 80.0, TwistType::Modulate, 65.0, 130.0, 7200.0, 100.0},

        {"308 Xplode Gate", "Drums",
         0.0, 0.0, -96.0, -2.5, 16.0, 2.0, 145.0, 30.0, 0.0,
         ReverbStyle::Gated80s, 90.0, TwistType::Crunch, 32.0, 110.0, 7800.0, 100.0},

        {"309 Reverse Ramp", "Reverse",
         0.0, 0.0, -96.0, -4.0, 0.0, 260.0, 60.0, 22.0, -1.0,
         ReverbStyle::Reverse, 85.0, TwistType::Off, 0.0, 100.0, 8200.0, 100.0},

        {"310 Dixie Chicken", "Ambience",
         0.0, 0.0, -96.0, -6.5, 18.0, 10.0, 68.0, 95.0, 0.20,
         ReverbStyle::NonLinB, 70.0, TwistType::Detune, 14.0, 150.0, 5400.0, 85.0},

        {"311 AMS NonLin B", "Classic 80s",
         0.0, 0.0, -96.0, -5.0, 12.0, 16.0, 105.0, 120.0, 0.0,
         ReverbStyle::NonLinB, 84.0, TwistType::Off, 0.0, 100.0, 4200.0, 100.0},

        {"312 Whisper", "Vocal",
         0.0, 0.0, -96.0, -8.5, 24.0, 32.0, 75.0, 150.0, 0.30,
         ReverbStyle::NonLinC, 86.0, TwistType::Modulate, 18.0, 280.0, 9200.0, 90.0},

        {"313 KitPig1", "Drums",
         0.0, 0.0, -96.0, -3.5, 10.0, 4.0, 88.0, 42.0, 0.0,
         ReverbStyle::NonLinA, 78.0, TwistType::Crunch, 24.0, 130.0, 6200.0, 95.0},

        {"314 KitPig2", "Drums",
         0.0, 0.0, -96.0, -3.0, 18.0, 6.0, 118.0, 58.0, 0.0,
         ReverbStyle::NonLinD, 85.0, TwistType::Detune, 20.0, 115.0, 5600.0, 100.0},

        {"315 Drummy", "Drums",
         0.0, 0.0, -96.0, -4.5, 15.0, 5.0, 92.0, 65.0, 0.10,
         ReverbStyle::NonLinB, 80.0, TwistType::Off, 0.0, 125.0, 6400.0, 95.0},

        {"316 Electronic Megaphone", "FX / Lo-Fi",
         0.0, 0.0, -96.0, -4.0, 6.0, 4.0, 60.0, 45.0, 0.0,
         ReverbStyle::NonLinC, 48.0, TwistType::Crunch, 75.0, 480.0, 2400.0, 45.0},

        {"317 Spacedome PA", "Large",
         0.0, 0.0, -96.0, -5.0, 55.0, 28.0, 190.0, 280.0, 0.30,
         ReverbStyle::NonLinD, 88.0, TwistType::Phase, 30.0, 140.0, 4400.0, 100.0},

        {"318 NonLin Bright", "Bright",
         0.0, 0.0, -96.0, -5.0, 14.0, 12.0, 85.0, 95.0, 0.15,
         ReverbStyle::NonLinA, 82.0, TwistType::Off, 0.0, 180.0, 14500.0, 100.0},

        {"319 Muffled Pipe Vocal", "FX / Vocal",
         0.0, 0.0, -96.0, -6.0, 12.0, 18.0, 90.0, 110.0, 0.20,
         ReverbStyle::NonLinC, 55.0, TwistType::Comb, 58.0, 260.0, 1850.0, 60.0},

        {"320 Breathing 130BPM", "Rhythmic",
         0.0, 0.0, -96.0, -4.5, 0.0, 115.0, 115.0, 230.0, -0.35,
         ReverbStyle::NonLinB, 84.0, TwistType::Modulate, 40.0, 110.0, 7000.0, 100.0},

        {"321 Kick Thundergate", "Drums",
         0.0, 0.0, -96.0, -3.0, 8.0, 2.0, 110.0, 32.0, 0.0,
         ReverbStyle::Gated80s, 76.0, TwistType::Off, 0.0, 45.0, 2200.0, 75.0},

        {"322 Snare Boinkygate", "Drums",
         0.0, 0.0, -96.0, -3.5, 16.0, 3.0, 95.0, 40.0, 0.0,
         ReverbStyle::NonLinC, 65.0, TwistType::Comb, 42.0, 150.0, 6600.0, 100.0},

        {"323 Thick Slapback", "Slap",
         0.0, 0.0, -96.0, -5.5, 95.0, 6.0, 48.0, 55.0, 0.10,
         ReverbStyle::NonLinA, 68.0, TwistType::Detune, 16.0, 140.0, 5500.0, 80.0},

        {"324 Tight Stereo Slap", "Slap",
         0.0, 0.0, -96.0, -6.0, 72.0, 4.0, 36.0, 42.0, 0.0,
         ReverbStyle::NonLinB, 72.0, TwistType::Off, 0.0, 160.0, 7200.0, 100.0},

        {"325 Dense Dark Room", "Room",
         0.0, 0.0, -96.0, -6.5, 18.0, 22.0, 115.0, 160.0, 0.35,
         ReverbStyle::NonLinD, 94.0, TwistType::Off, 0.0, 90.0, 2600.0, 90.0},

        {"326 Dense Arena", "Large",
         0.0, 0.0, -96.0, -4.0, 38.0, 24.0, 165.0, 240.0, 0.25,
         ReverbStyle::NonLinD, 95.0, TwistType::Detune, 20.0, 100.0, 5200.0, 100.0},

        {"327 NonLin Slap", "Slap",
         0.0, 0.0, -96.0, -5.5, 84.0, 8.0, 52.0, 60.0, 0.0,
         ReverbStyle::NonLinA, 74.0, TwistType::Off, 0.0, 150.0, 6400.0, 95.0},

        {"328 Ricochet Verb", "FX",
         0.0, 0.0, -96.0, -4.5, 42.0, 10.0, 125.0, 140.0, 0.0,
         ReverbStyle::NonLinC, 45.0, TwistType::Phase, 52.0, 180.0, 8800.0, 100.0},

        {"329 Tight & Smooth", "Studio",
         0.0, 0.0, -96.0, -6.5, 16.0, 14.0, 78.0, 105.0, 0.25,
         ReverbStyle::NonLinB, 90.0, TwistType::Off, 0.0, 130.0, 6200.0, 95.0},

        {"330 80's 808", "Drums",
         0.0, 0.0, -96.0, -3.5, 12.0, 2.0, 108.0, 36.0, 0.0,
         ReverbStyle::Gated80s, 84.0, TwistType::Detune, 24.0, 95.0, 5800.0, 100.0},

        {"331 Studio Spring", "Vintage",
         0.0, 0.0, -96.0, -6.0, 14.0, 16.0, 70.0, 165.0, 0.40,
         ReverbStyle::NonLinC, 54.0, TwistType::Comb, 28.0, 190.0, 4600.0, 75.0},

        {"332 Nonlin Vocal", "Vocal",
         0.0, 0.0, -96.0, -7.0, 26.0, 20.0, 82.0, 135.0, 0.20,
         ReverbStyle::NonLinB, 85.0, TwistType::Modulate, 14.0, 210.0, 6900.0, 90.0},

        {"333 RichVerb", "Studio",
         0.0, 0.0, -96.0, -5.0, 28.0, 30.0, 140.0, 260.0, 0.40,
         ReverbStyle::NonLinD, 96.0, TwistType::Detune, 18.0, 100.0, 7800.0, 100.0}
    }};
    return kPresets;
  }

  void loadPreset(int index) noexcept {
    const auto &presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
      return;
    currentPresetIndex = index;
    const auto &p = presets[static_cast<size_t>(index)];
    setInGainDb(p.inGainDb);
    setOutGainDb(p.outGainDb);
    setDryGainDb(p.dryGainDb);
    setWetGainDb(p.wetGainDb);
    setPreDelayMs(p.preDelayMs);
    setAttackMs(p.attackMs);
    setHoldMs(p.holdMs);
    setReleaseMs(p.releaseMs);
    setSlope(p.slope);
    setReverbStyle(p.style);
    setDiffusePct(p.diffusePct);
    setTwistType(p.twistType);
    setTwistRatioPct(p.twistRatioPct);
    setLowCutHz(p.lowCutHz);
    setHighCutHz(p.highCutHz);
    setWidthPct(p.widthPct);
    useMixMode = false;
  }

  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double rawInL = sanitize(inOutL);
    const double rawInR = sanitize(inOutR);

    const double sr = std::max(8000.0, sampleRate);
    const double meterDecay = std::exp(-1.0 / (sr * 0.08));

    float curInL = meterInL.load(std::memory_order_relaxed);
    float curInR = meterInR.load(std::memory_order_relaxed);
    meterInL.store(std::max(static_cast<float>(std::abs(rawInL)),
                            curInL * static_cast<float>(meterDecay)),
                   std::memory_order_relaxed);
    meterInR.store(std::max(static_cast<float>(std::abs(rawInR)),
                            curInR * static_cast<float>(meterDecay)),
                   std::memory_order_relaxed);

    if (bypassed) {
      inOutL = rawInL;
      inOutR = rawInR;
      meterOutL.store(meterInL.load(std::memory_order_relaxed), std::memory_order_relaxed);
      meterOutR.store(meterInR.load(std::memory_order_relaxed), std::memory_order_relaxed);
      return;
    }

    const double dryL = rawInL * inGainLin;
    const double dryR = rawInR * inGainLin;

    // 1. Pre-delay
    const int preDelaySamples = std::clamp(
        static_cast<int>(preDelayMs * 0.001 * sr), 0, kMaxPreDelaySamples - 1);
    preDelayBufL[preDelayWrite] = dryL;
    preDelayBufR[preDelayWrite] = dryR;

    int preRead = preDelayWrite - preDelaySamples;
    if (preRead < 0)
      preRead += kMaxPreDelaySamples;
    const double preL = preDelayBufL[preRead];
    const double preR = preDelayBufR[preRead];
    preDelayWrite = (preDelayWrite + 1) % kMaxPreDelaySamples;

    // 2. All-Pass Diffusion Network (scaled by Diffuse %)
    double diffInL = preL;
    double diffInR = preR;

    static constexpr int kDiffLengths[kNumDiffusers] = {149, 227, 337, 431, 557, 661};
    const double diffCoeff = 0.15 + (diffusePct * 0.01) * 0.57;

    for (int i = 0; i < kNumDiffusers; ++i) {
      const int len = kDiffLengths[i];
      auto &bufL = diffBufL[static_cast<size_t>(i)];
      auto &bufR = diffBufR[static_cast<size_t>(i)];
      int &idxL = diffIdxL[static_cast<size_t>(i)];
      int &idxR = diffIdxR[static_cast<size_t>(i)];

      const double delayedL = bufL[static_cast<size_t>(idxL)];
      const double delayedR = bufR[static_cast<size_t>(idxR)];

      const double outL = -diffCoeff * diffInL + delayedL;
      const double outR = -diffCoeff * diffInR + delayedR;

      bufL[static_cast<size_t>(idxL)] = diffInL + diffCoeff * delayedL;
      bufR[static_cast<size_t>(idxR)] = diffInR + diffCoeff * delayedR;

      idxL = (idxL + 1) % len;
      idxR = (idxR + 1) % len;

      diffInL = outL;
      diffInR = outR;
    }

    // 3. Twist pre-tank coloration
    twistPhase += 2.4 / sr;
    if (twistPhase >= 1.0)
      twistPhase -= 1.0;
    const double twistLfo = std::sin(twistPhase * 2.0 * std::numbers::pi);
    const double twistAmt = twistRatioPct * 0.01;

    if (twistType == TwistType::Crunch && twistAmt > 0.01) {
      const double drive = 1.0 + twistAmt * 6.0;
      diffInL = std::tanh(diffInL * drive) / drive;
      diffInR = std::tanh(diffInR * drive) / drive;
    } else if (twistType == TwistType::Modulate && twistAmt > 0.01) {
      const double mod = 1.0 + 0.35 * twistAmt * twistLfo;
      diffInL *= mod;
      diffInR *= (2.0 - mod);
    }

    // 4. Tank Buffer Insertion
    tankBufL[tankWrite] = diffInL;
    tankBufR[tankWrite] = diffInR;

    // 5. Non-linear Multi-Tap Envelope Sampling (P - A - H - R + Style)
    const double totalTimeMs = std::max(5.0, attackMs + holdMs + releaseMs);
    const double totalSamples = std::max(10.0, totalTimeMs * 0.001 * sr);
    const double aFrac = attackMs / totalTimeMs;
    const double hFrac = (attackMs + holdMs) / totalTimeMs;

    static constexpr double kTapOffsets[kNumTaps] = {
        0.04, 0.09, 0.15, 0.21, 0.28, 0.35, 0.42, 0.49,
        0.56, 0.63, 0.70, 0.77, 0.84, 0.90, 0.95, 1.00};

    // Style-dependent tap spacing skew
    double styleSkew = 1.0;
    if (reverbStyle == ReverbStyle::NonLinA) styleSkew = 0.92;
    else if (reverbStyle == ReverbStyle::NonLinB) styleSkew = 1.0;
    else if (reverbStyle == ReverbStyle::NonLinC) styleSkew = 1.12;
    else if (reverbStyle == ReverbStyle::NonLinD) styleSkew = 1.24;
    else if (reverbStyle == ReverbStyle::Gated80s) styleSkew = 0.85;
    else if (reverbStyle == ReverbStyle::Reverse) styleSkew = 1.18;

    double accumL = 0.0;
    double accumR = 0.0;

    for (int t = 0; t < kNumTaps; ++t) {
      const double frac = std::pow(kTapOffsets[t], styleSkew);
      double tapMod = 0.0;
      if (twistType == TwistType::Detune && twistAmt > 0.01)
        tapMod = twistAmt * 0.015 * twistLfo * ((t % 2 == 0) ? 1.0 : -1.0);
      else if (twistType == TwistType::Comb && twistAmt > 0.01)
        tapMod = -0.3 * twistAmt * (t % 3 == 0 ? 1.0 : 0.0);

      const int tapDelay = std::clamp(
          static_cast<int>((frac + tapMod) * totalSamples), 1, kTankSize - 1);
      int readPos = tankWrite - tapDelay;
      if (readPos < 0)
        readPos += kTankSize;

      // Authentic Attack -> Hold -> Release envelope weight
      double envWeight = 1.0;
      if (frac < aFrac && aFrac > 1e-4) {
        envWeight = frac / aFrac;
      } else if (frac <= hFrac) {
        envWeight = 1.0;
      } else {
        const double rSpan = std::max(1e-4, 1.0 - hFrac);
        envWeight = std::clamp(1.0 - (frac - hFrac) / rSpan, 0.0, 1.0);
      }

      // Slope & Style shaping
      if (slope < -0.05 || reverbStyle == ReverbStyle::Reverse) {
        const double revFactor = (reverbStyle == ReverbStyle::Reverse) ? 0.85 : -slope;
        envWeight = envWeight * (1.0 - revFactor) + revFactor * std::pow(frac, 2.0);
      } else if (slope > 0.05) {
        envWeight *= std::exp(-slope * 2.5 * frac);
      }

      const double pol = (twistType == TwistType::Phase && (t % 2 == 1) && twistAmt > 0.2)
                             ? -1.0
                             : 1.0;

      if ((t % 2) == 0) {
        accumL += tankBufL[readPos] * envWeight;
        accumR += (tankBufR[readPos] * 0.7 + tankBufL[readPos] * 0.3) * envWeight * pol;
      } else {
        accumL += (tankBufL[readPos] * 0.3 + tankBufR[readPos] * 0.7) * envWeight * pol;
        accumR += tankBufR[readPos] * envWeight;
      }
    }

    tankWrite = (tankWrite + 1) % kTankSize;

    const double norm = 1.8 / std::sqrt(static_cast<double>(kNumTaps));
    accumL *= norm;
    accumR *= norm;

    // 6. Tone Filtering (2nd-order Lo Cut HPF & Hi Cut LPF)
    double wetL = highCutFilterL.process(lowCutFilterL.process(accumL));
    double wetR = highCutFilterR.process(lowCutFilterR.process(accumR));

    // 7. Stereo Width (Mid-Side)
    const double mid = 0.5 * (wetL + wetR);
    const double side = 0.5 * (wetL - wetR) * (widthPct * 0.01);
    wetL = mid + side;
    wetR = mid - side;

    // 8. Levels (Dry Gain + Wet Gain + Out Gain OR legacy Mix)
    double outL = 0.0;
    double outR = 0.0;
    if (useMixMode) {
      outL = (dryL * (1.0 - mix) + wetL * mix) * outGainLin;
      outR = (dryR * (1.0 - mix) + wetR * mix) * outGainLin;
    } else {
      outL = (dryL * dryGainLin + wetL * wetGainLin) * outGainLin;
      outR = (dryR * dryGainLin + wetR * wetGainLin) * outGainLin;
    }

    inOutL = sanitize(outL);
    inOutR = sanitize(outR);

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
  void updateFilters() noexcept {
    const double sr = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    lowCutFilterL.setCoefficients(makeButterworthHPF(sr, lowCutHz, 0.7071));
    lowCutFilterR.setCoefficients(makeButterworthHPF(sr, lowCutHz, 0.7071));
    highCutFilterL.setCoefficients(makeButterworthLPF(sr, highCutHz, 0.7071));
    highCutFilterR.setCoefficients(makeButterworthLPF(sr, highCutHz, 0.7071));
  }

  double sampleRate = 48000.0;
  bool bypassed = false;
  bool safeMode = false;

  double inGainDb = 0.0, inGainLin = 1.0;
  double outGainDb = 0.0, outGainLin = 1.0;
  double dryGainDb = -96.0, dryGainLin = 0.0;
  double wetGainDb = -6.0, wetGainLin = 0.501187;
  double mix = 0.35;
  bool useMixMode = false;

  double preDelayMs = 0.0;
  double attackMs = 36.0;
  double holdMs = 84.0;
  double releaseMs = 204.0;
  double slope = 0.0;

  ReverbStyle reverbStyle = ReverbStyle::NonLinB;
  double diffusePct = 78.0;
  TwistType twistType = TwistType::Off;
  double twistRatioPct = 0.0;
  double lowCutHz = 90.0;
  double highCutHz = 2360.0;
  double widthPct = 100.0;

  Biquad lowCutFilterL, lowCutFilterR;
  Biquad highCutFilterL, highCutFilterR;

  std::vector<double> preDelayBufL;
  std::vector<double> preDelayBufR;
  int preDelayWrite = 0;

  std::array<std::array<double, 1024>, kNumDiffusers> diffBufL{};
  std::array<std::array<double, 1024>, kNumDiffusers> diffBufR{};
  std::array<int, kNumDiffusers> diffIdxL{};
  std::array<int, kNumDiffusers> diffIdxR{};

  std::vector<double> tankBufL;
  std::vector<double> tankBufR;
  int tankWrite = 0;
  double twistPhase = 0.0;

  int currentPresetIndex = 0;
  std::atomic<float> meterInL{0.0f};
  std::atomic<float> meterInR{0.0f};
  std::atomic<float> meterOutL{0.0f};
  std::atomic<float> meterOutR{0.0f};
};

} // namespace nova::graviton
