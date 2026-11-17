#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace nova::graviton {

// ==============================================================================
// DistortionEngine — Dual Warm Tube & Tape Saturation (Kiive Tape Face style)
// Virtual FX Rack Slot 19 - MIDAS Heritage-D HD96-24 Chapter 15
//
// Signal Flow:
//   AUDIO INPUT
//     -> TUBE SATURATION (Power, Type: 12AX7/12AU7/12AT7/6L6/EL34, Drive, Bias, Mix)
//     -> TAPE SATURATION (Power, Type: 1/4" 15 IPS, 1/2" 30 IPS, 2" 30 IPS, Vintage,
//                         Drive, Flutter, Wow, Mix)
//     -> TUBE / TAPE BLEND (0% Tube .. 50% Balanced .. 100% Tape)
//     -> TONE CONTROL (Power, Low dB, High dB)
//     -> CHARACTER PROCESSING (Warm / Vintage / Aggressive)
//     -> OUTPUT TRIM + SOFT CLIP (Power, Trim dB, Soft Clip)
//   AUDIO OUTPUT
//
// Real-time audio-thread safe: zero heap allocations, zero locks, NaN/Inf sanitized.
// ==============================================================================
class DistortionEngine final {
public:
  enum class TubeType : int {
    T_12AX7 = 0,
    T_12AU7 = 1,
    T_12AT7 = 2,
    T_6L6 = 3,
    T_EL34 = 4
  };

  enum class TapeType : int {
    IPS15_Quarter = 0, // 1/4" 15 IPS
    IPS30_Half = 1,    // 1/2" 30 IPS
    IPS30_Two = 2,     // 2" 30 IPS
    VintageTape = 3    // Vintage Tape
  };

  enum class CharacterMode : int { Warm = 0, Vintage = 1, Aggressive = 2 };

  static constexpr int kNumPresets = 12;
  static constexpr int kDelayBufSize = 2048;

  struct Snapshot {
    bool tubeEnabled = true;
    int tubeType = 0;
    double tubeDriveDb = 6.0;
    double tubeBiasPct = 15.0;
    double tubeMixPct = 100.0;

    double blendPct = 50.0; // 0 = Tube, 100 = Tape

    bool tapeEnabled = true;
    int tapeType = 2;
    double tapeDriveDb = 6.0;
    double flutterPct = 10.0;
    double wowPct = 10.0;
    double tapeMixPct = 100.0;

    bool toneEnabled = true;
    double toneLowDb = 1.5;
    double toneHighDb = -0.5;

    int characterMode = 0; // Warm / Vintage / Aggressive

    bool outputEnabled = true;
    double outputTrimDb = 0.0;
    bool softClip = true;
    double masterMix = 1.0;
  };

  DistortionEngine() noexcept {
    setSampleRate(48000.0);
    applyPreset(0);
    slotA = currentSnapshot();
    slotB = currentSnapshot();
  }

  void setSampleRate(double sr) noexcept {
    cachedSampleRate = clampFinite(sr, 1000.0, 768000.0, 48000.0);
    updateFilters();
    reset();
  }

  void reset() noexcept {
    tapeHeadLpfL.reset();
    tapeHeadLpfR.reset();
    tapeBumpL.reset();
    tapeBumpR.reset();
    toneLowL.reset();
    toneLowR.reset();
    toneHighL.reset();
    toneHighR.reset();
    charMidL.reset();
    charMidR.reset();
    dcX1L = dcY1L = dcX1R = dcY1R = 0.0;
    prevTubeL = prevTubeR = prevTapeL = prevTapeR = 0.0;
    wowPhase = 0.0;
    flutterPhase = 0.0;
    delayBufL.fill(0.0);
    delayBufR.fill(0.0);
    delayWriteIdx = 0;
    inLevelDb.store(-60.0f, std::memory_order_relaxed);
    outLevelDb.store(-60.0f, std::memory_order_relaxed);
    vuLevelDb.store(-20.0f, std::memory_order_relaxed);
  }

  // --- Legacy / Rack Summary Compatibility ---
  void setDriveDb(double driveDb) noexcept {
    tubeDriveDb = clampFinite(driveDb, 0.0, 24.0, 6.0);
  }
  double getDriveDb() const noexcept { return tubeDriveDb; }

  void setMix(double newMix) noexcept {
    masterMix = clampFinite(newMix, 0.0, 1.0, 1.0);
  }
  double getMix() const noexcept { return masterMix; }

  // --- Tube Saturation Controls ---
  void setTubeEnabled(bool on) noexcept { tubeEnabled = on; }
  bool getTubeEnabled() const noexcept { return tubeEnabled; }
  void setTubeType(int t) noexcept { tubeType = std::clamp(t, 0, 4); }
  int getTubeType() const noexcept { return tubeType; }
  void setTubeDriveDb(double db) noexcept {
    tubeDriveDb = clampFinite(db, 0.0, 24.0, 6.0);
  }
  double getTubeDriveDb() const noexcept { return tubeDriveDb; }
  void setTubeBiasPct(double pct) noexcept {
    tubeBiasPct = clampFinite(pct, -100.0, 100.0, 0.0);
  }
  double getTubeBiasPct() const noexcept { return tubeBiasPct; }
  void setTubeMixPct(double pct) noexcept {
    tubeMixPct = clampFinite(pct, 0.0, 100.0, 100.0);
  }
  double getTubeMixPct() const noexcept { return tubeMixPct; }

  // --- Blend (0% Tube .. 100% Tape) ---
  void setBlendPct(double pct) noexcept {
    blendPct = clampFinite(pct, 0.0, 100.0, 50.0);
  }
  double getBlendPct() const noexcept { return blendPct; }

  // --- Tape Saturation Controls ---
  void setTapeEnabled(bool on) noexcept { tapeEnabled = on; }
  bool getTapeEnabled() const noexcept { return tapeEnabled; }
  void setTapeType(int t) noexcept {
    tapeType = std::clamp(t, 0, 3);
    updateFilters();
  }
  int getTapeType() const noexcept { return tapeType; }
  void setTapeDriveDb(double db) noexcept {
    tapeDriveDb = clampFinite(db, 0.0, 24.0, 6.0);
  }
  double getTapeDriveDb() const noexcept { return tapeDriveDb; }
  void setFlutterPct(double pct) noexcept {
    flutterPct = clampFinite(pct, 0.0, 100.0, 10.0);
  }
  double getFlutterPct() const noexcept { return flutterPct; }
  void setWowPct(double pct) noexcept {
    wowPct = clampFinite(pct, 0.0, 100.0, 10.0);
  }
  double getWowPct() const noexcept { return wowPct; }
  void setTapeMixPct(double pct) noexcept {
    tapeMixPct = clampFinite(pct, 0.0, 100.0, 100.0);
  }
  double getTapeMixPct() const noexcept { return tapeMixPct; }

  // --- Tone Controls ---
  void setToneEnabled(bool on) noexcept { toneEnabled = on; }
  bool getToneEnabled() const noexcept { return toneEnabled; }
  void setToneLowDb(double db) noexcept {
    toneLowDb = clampFinite(db, -12.0, 12.0, 1.5);
    updateFilters();
  }
  double getToneLowDb() const noexcept { return toneLowDb; }
  void setToneHighDb(double db) noexcept {
    toneHighDb = clampFinite(db, -12.0, 12.0, -0.5);
    updateFilters();
  }
  double getToneHighDb() const noexcept { return toneHighDb; }

  // --- Character Mode ---
  void setCharacterMode(int m) noexcept {
    characterMode = std::clamp(m, 0, 2);
    updateFilters();
  }
  int getCharacterMode() const noexcept { return characterMode; }

  // --- Output Section ---
  void setOutputEnabled(bool on) noexcept { outputEnabled = on; }
  bool getOutputEnabled() const noexcept { return outputEnabled; }
  void setOutputTrimDb(double db) noexcept {
    outputTrimDb = clampFinite(db, -24.0, 12.0, 0.0);
  }
  double getOutputTrimDb() const noexcept { return outputTrimDb; }
  void setSoftClip(bool on) noexcept { softClip = on; }
  bool getSoftClip() const noexcept { return softClip; }

  // --- VU Meter Source (false = Output, true = Input) ---
  void setVuSourceInput(bool inSrc) noexcept { vuSourceInput = inSrc; }
  bool getVuSourceInput() const noexcept { return vuSourceInput; }

  // --- A/B Compare & User Save ---
  void selectSlotB(bool useB) noexcept {
    if (useB == activeSlotB)
      return;
    if (activeSlotB)
      slotB = currentSnapshot();
    else
      slotA = currentSnapshot();
    activeSlotB = useB;
    applySnapshot(activeSlotB ? slotB : slotA);
  }
  bool isSlotB() const noexcept { return activeSlotB; }

  void saveUserPreset() noexcept {
    userSaved = currentSnapshot();
    hasUserSaved = true;
  }
  bool loadUserPreset() noexcept {
    if (!hasUserSaved)
      return false;
    applySnapshot(userSaved);
    return true;
  }

  // --- Presets ---
  static const char *getPresetName(int idx) noexcept {
    static constexpr const char *kNames[kNumPresets] = {
        "01 - Warm Vocal",     "02 - Vintage Vocal",   "03 - Analog Glue",
        "04 - Warm Tape Drive","05 - Tube Warmth",     "06 - Aggressive Drums",
        "07 - Tape Drums",     "08 - Bass Thickener",  "09 - Guitar Crunch",
        "10 - Synth Color",    "11 - Master Warmth",   "12 - Heavy Analog"};
    if (idx < 0 || idx >= kNumPresets)
      return kNames[0];
    return kNames[idx];
  }

  int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

  void applyPreset(int idx) noexcept {
    currentPresetIndex = std::clamp(idx, 0, kNumPresets - 1);
    Snapshot s;
    switch (currentPresetIndex) {
    case 0: // 01 - Warm Vocal (Warm | Tube 2.5, Tape 2.0, Blend 35%)
      s.tubeEnabled = true; s.tubeType = 0; s.tubeDriveDb = 2.5; s.tubeBiasPct = 15.0; s.tubeMixPct = 100.0;
      s.blendPct = 35.0;
      s.tapeEnabled = true; s.tapeType = 1; s.tapeDriveDb = 2.0; s.flutterPct = 5.0; s.wowPct = 5.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 1.0; s.toneHighDb = 0.5;
      s.characterMode = 0; s.outputTrimDb = 0.0; s.softClip = true;
      break;
    case 1: // 02 - Vintage Vocal (Vintage | Tube 3.5, Tape 3.8, Blend 55%)
      s.tubeEnabled = true; s.tubeType = 0; s.tubeDriveDb = 3.5; s.tubeBiasPct = 22.0; s.tubeMixPct = 100.0;
      s.blendPct = 55.0;
      s.tapeEnabled = true; s.tapeType = 3; s.tapeDriveDb = 3.8; s.flutterPct = 12.0; s.wowPct = 10.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 0.5; s.toneHighDb = -0.5;
      s.characterMode = 1; s.outputTrimDb = 0.0; s.softClip = true;
      break;
    case 2: // 03 - Analog Glue (Warm | Tube 2.0, Tape 3.0, Blend 60%)
      s.tubeEnabled = true; s.tubeType = 1; s.tubeDriveDb = 2.0; s.tubeBiasPct = 10.0; s.tubeMixPct = 100.0;
      s.blendPct = 60.0;
      s.tapeEnabled = true; s.tapeType = 2; s.tapeDriveDb = 3.0; s.flutterPct = 4.0; s.wowPct = 4.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 1.0; s.toneHighDb = 0.0;
      s.characterMode = 0; s.outputTrimDb = 0.0; s.softClip = true;
      break;
    case 3: // 04 - Warm Tape Drive (Vintage | Tube 2.0, Tape 5.2, Blend 85%)
      s.tubeEnabled = true; s.tubeType = 0; s.tubeDriveDb = 2.0; s.tubeBiasPct = 12.0; s.tubeMixPct = 100.0;
      s.blendPct = 85.0;
      s.tapeEnabled = true; s.tapeType = 1; s.tapeDriveDb = 5.2; s.flutterPct = 10.0; s.wowPct = 8.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 1.5; s.toneHighDb = -0.5;
      s.characterMode = 1; s.outputTrimDb = 0.0; s.softClip = true;
      break;
    case 4: // 05 - Tube Warmth (Warm | Tube 5.0, Tape 1.5, Blend 15%)
      s.tubeEnabled = true; s.tubeType = 3; s.tubeDriveDb = 5.0; s.tubeBiasPct = 20.0; s.tubeMixPct = 100.0;
      s.blendPct = 15.0;
      s.tapeEnabled = true; s.tapeType = 1; s.tapeDriveDb = 1.5; s.flutterPct = 3.0; s.wowPct = 3.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 1.5; s.toneHighDb = -0.5;
      s.characterMode = 0; s.outputTrimDb = 0.0; s.softClip = true;
      break;
    case 5: // 06 - Aggressive Drums (Aggressive | Tube 6.5, Tape 6.0, Blend 50%)
      s.tubeEnabled = true; s.tubeType = 4; s.tubeDriveDb = 6.5; s.tubeBiasPct = 18.0; s.tubeMixPct = 100.0;
      s.blendPct = 50.0;
      s.tapeEnabled = true; s.tapeType = 2; s.tapeDriveDb = 6.0; s.flutterPct = 8.0; s.wowPct = 6.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 2.0; s.toneHighDb = 1.0;
      s.characterMode = 2; s.outputTrimDb = -1.0; s.softClip = true;
      break;
    case 6: // 07 - Tape Drums (Vintage | Tube 2.0, Tape 5.5, Blend 90%)
      s.tubeEnabled = true; s.tubeType = 1; s.tubeDriveDb = 2.0; s.tubeBiasPct = 10.0; s.tubeMixPct = 100.0;
      s.blendPct = 90.0;
      s.tapeEnabled = true; s.tapeType = 0; s.tapeDriveDb = 5.5; s.flutterPct = 12.0; s.wowPct = 8.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 2.0; s.toneHighDb = -0.5;
      s.characterMode = 1; s.outputTrimDb = -0.5; s.softClip = true;
      break;
    case 7: // 08 - Bass Thickener (Warm | Tube 4.5, Tape 3.5, Blend 35%)
      s.tubeEnabled = true; s.tubeType = 3; s.tubeDriveDb = 4.5; s.tubeBiasPct = 25.0; s.tubeMixPct = 100.0;
      s.blendPct = 35.0;
      s.tapeEnabled = true; s.tapeType = 0; s.tapeDriveDb = 3.5; s.flutterPct = 4.0; s.wowPct = 4.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 2.5; s.toneHighDb = -1.0;
      s.characterMode = 0; s.outputTrimDb = -0.5; s.softClip = true;
      break;
    case 8: // 09 - Guitar Crunch (Aggressive | Tube 7.0, Tape 4.0, Blend 30%)
      s.tubeEnabled = true; s.tubeType = 4; s.tubeDriveDb = 7.0; s.tubeBiasPct = 25.0; s.tubeMixPct = 100.0;
      s.blendPct = 30.0;
      s.tapeEnabled = true; s.tapeType = 2; s.tapeDriveDb = 4.0; s.flutterPct = 6.0; s.wowPct = 5.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 1.0; s.toneHighDb = 1.5;
      s.characterMode = 2; s.outputTrimDb = -1.0; s.softClip = true;
      break;
    case 9: // 10 - Synth Color (Vintage | Tube 3.5, Tape 4.5, Blend 65%)
      s.tubeEnabled = true; s.tubeType = 2; s.tubeDriveDb = 3.5; s.tubeBiasPct = 18.0; s.tubeMixPct = 100.0;
      s.blendPct = 65.0;
      s.tapeEnabled = true; s.tapeType = 3; s.tapeDriveDb = 4.5; s.flutterPct = 14.0; s.wowPct = 14.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 1.0; s.toneHighDb = 1.0;
      s.characterMode = 1; s.outputTrimDb = -0.5; s.softClip = true;
      break;
    case 10: // 11 - Master Warmth (Warm | Tube 1.5, Tape 1.8, Blend 50%)
      s.tubeEnabled = true; s.tubeType = 1; s.tubeDriveDb = 1.5; s.tubeBiasPct = 8.0; s.tubeMixPct = 100.0;
      s.blendPct = 50.0;
      s.tapeEnabled = true; s.tapeType = 1; s.tapeDriveDb = 1.8; s.flutterPct = 2.0; s.wowPct = 2.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 0.5; s.toneHighDb = -0.5;
      s.characterMode = 0; s.outputTrimDb = 0.0; s.softClip = true;
      break;
    case 11: // 12 - Heavy Analog (Aggressive | Tube 8.0, Tape 7.5, Blend 50%)
      s.tubeEnabled = true; s.tubeType = 4; s.tubeDriveDb = 8.0; s.tubeBiasPct = 35.0; s.tubeMixPct = 100.0;
      s.blendPct = 50.0;
      s.tapeEnabled = true; s.tapeType = 3; s.tapeDriveDb = 7.5; s.flutterPct = 18.0; s.wowPct = 18.0; s.tapeMixPct = 100.0;
      s.toneEnabled = true; s.toneLowDb = 2.5; s.toneHighDb = -1.0;
      s.characterMode = 2; s.outputTrimDb = -2.0; s.softClip = true;
      break;
    }
    applySnapshot(s);
  }

  // --- Telemetry ---
  float getInputLevelDb() const noexcept {
    return inLevelDb.load(std::memory_order_relaxed);
  }
  float getOutputLevelDb() const noexcept {
    return outLevelDb.load(std::memory_order_relaxed);
  }
  float getVuLevelDb() const noexcept {
    return vuLevelDb.load(std::memory_order_relaxed);
  }

  double computeTransferSample(double x) const noexcept {
    double y = x;
    if (tubeEnabled) {
      const double g = std::pow(10.0, tubeDriveDb / 20.0);
      const double b = tubeBiasPct * 0.0035;
      const double shaped = shapeTube(tubeType, g * x + b) - shapeTube(tubeType, b);
      const double m = tubeMixPct * 0.01;
      y = x * (1.0 - m) + shaped * m;
    }
    if (tapeEnabled) {
      const double g = std::pow(10.0, tapeDriveDb / 20.0);
      const double shaped = shapeTape(tapeType, g * y);
      const double m = tapeMixPct * 0.01;
      y = y * (1.0 - m) + shaped * m;
    }
    return std::clamp(y, -1.5, 1.5);
  }

  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double dryL = sanitize(inOutL);
    const double dryR = sanitize(inOutR);
    const double inPeak = std::max(std::abs(dryL), std::abs(dryR));
    updateMeter(inLevelDb, inPeak);

    // 1. TUBE SATURATION (2x oversampled)
    double tubeL = dryL, tubeR = dryR;
    if (tubeEnabled) {
      const double g = std::pow(10.0, tubeDriveDb / 20.0);
      const double b = tubeBiasPct * 0.0035;
      const double midL = 0.5 * (prevTubeL + dryL);
      const double midR = 0.5 * (prevTubeR + dryR);
      prevTubeL = dryL;
      prevTubeR = dryR;

      const double yL = 0.5 * (shapeTube(tubeType, g * midL + b) +
                               shapeTube(tubeType, g * dryL + b));
      const double yR = 0.5 * (shapeTube(tubeType, g * midR + b) +
                               shapeTube(tubeType, g * dryR + b));

      // DC block asymmetric tube bias
      const double dcL = yL - dcX1L + 0.995 * dcY1L;
      dcX1L = yL; dcY1L = dcL;
      const double dcR = yR - dcX1R + 0.995 * dcY1R;
      dcX1R = yR; dcY1R = dcR;

      const double tm = tubeMixPct * 0.01;
      tubeL = dryL * (1.0 - tm) + dcL * tm;
      tubeR = dryR * (1.0 - tm) + dcR * tm;
    }

    // 2. TAPE SATURATION + WOW & FLUTTER (2x oversampled)
    double tapeL = tubeL, tapeR = tubeR;
    if (tapeEnabled) {
      // Write into circular delay line for Wow & Flutter pitch modulation
      delayBufL[static_cast<size_t>(delayWriteIdx)] = tubeL;
      delayBufR[static_cast<size_t>(delayWriteIdx)] = tubeR;

      const double wowInc = 2.0 * 3.14159265358979323846 * 0.65 / cachedSampleRate;
      const double fltInc = 2.0 * 3.14159265358979323846 * 6.80 / cachedSampleRate;
      wowPhase += wowInc;
      if (wowPhase > 6.28318530717958647692) wowPhase -= 6.28318530717958647692;
      flutterPhase += fltInc;
      if (flutterPhase > 6.28318530717958647692) flutterPhase -= 6.28318530717958647692;

      const double modSamples =
          12.0 + (wowPct * 0.01 * 18.0 * std::sin(wowPhase)) +
          (flutterPct * 0.01 * 4.5 * std::sin(flutterPhase));
      const double modL = (wowPct > 0.1 || flutterPct > 0.1)
                              ? readDelay(delayBufL, modSamples)
                              : tubeL;
      const double modR = (wowPct > 0.1 || flutterPct > 0.1)
                              ? readDelay(delayBufR, modSamples)
                              : tubeR;
      delayWriteIdx = (delayWriteIdx + 1) & (kDelayBufSize - 1);

      // Head bump & HF head rolloff
      double bumpedL = sanitize(tapeHeadLpfL.process(tapeBumpL.process(modL)));
      double bumpedR = sanitize(tapeHeadLpfR.process(tapeBumpR.process(modR)));

      const double tg = std::pow(10.0, tapeDriveDb / 20.0);
      const double midL = 0.5 * (prevTapeL + bumpedL);
      const double midR = 0.5 * (prevTapeR + bumpedR);
      prevTapeL = bumpedL;
      prevTapeR = bumpedR;

      const double yL = 0.5 * (shapeTape(tapeType, tg * midL) +
                               shapeTape(tapeType, tg * bumpedL));
      const double yR = 0.5 * (shapeTape(tapeType, tg * midR) +
                               shapeTape(tapeType, tg * bumpedR));

      const double tm = tapeMixPct * 0.01;
      tapeL = tubeL * (1.0 - tm) + yL * tm;
      tapeR = tubeR * (1.0 - tm) + yR * tm;
    }

    // 3. TUBE / TAPE BLEND (0% = Tube dominant, 100% = Tape dominant)
    const double b = blendPct * 0.01;
    double sigL = tubeL * (1.0 - b) + tapeL * b;
    double sigR = tubeR * (1.0 - b) + tapeR * b;

    // 4. TONE CONTROL (Low & High)
    if (toneEnabled) {
      if (std::abs(toneLowDb) > 0.05) {
        sigL = sanitize(toneLowL.process(sigL));
        sigR = sanitize(toneLowR.process(sigR));
      }
      if (std::abs(toneHighDb) > 0.05) {
        sigL = sanitize(toneHighL.process(sigL));
        sigR = sanitize(toneHighR.process(sigR));
      }
    }

    // 5. CHARACTER PROCESSING (Warm / Vintage / Aggressive)
    sigL = applyCharacter(sigL, charMidL);
    sigR = applyCharacter(sigR, charMidR);

    // 6. OUTPUT TRIM + SOFT CLIP
    if (outputEnabled) {
      const double trimGain = std::pow(10.0, outputTrimDb / 20.0);
      sigL *= trimGain;
      sigR *= trimGain;
      if (softClip) {
        sigL = std::tanh(sigL * 0.95) / 0.95;
        sigR = std::tanh(sigR * 0.95) / 0.95;
      }
    }

    const double outL = sanitize(dryL * (1.0 - masterMix) + sigL * masterMix);
    const double outR = sanitize(dryR * (1.0 - masterMix) + sigR * masterMix);

    const double outPeak = std::max(std::abs(outL), std::abs(outR));
    updateMeter(outLevelDb, outPeak);

    // Analog VU ballistic (-20 .. +3 dB around -18 dBFS = 0 VU)
    const double vuSrc = vuSourceInput ? inPeak : outPeak;
    const double vuRaw = (vuSrc <= 1.0e-6) ? -20.0
                                           : std::clamp(20.0 * std::log10(vuSrc) + 12.0, -20.0, 3.0);
    const float prevVu = vuLevelDb.load(std::memory_order_relaxed);
    vuLevelDb.store(static_cast<float>(prevVu * 0.996 + vuRaw * 0.004),
                    std::memory_order_relaxed);

    inOutL = outL;
    inOutR = outR;
  }

private:
  Snapshot currentSnapshot() const noexcept {
    Snapshot s;
    s.tubeEnabled = tubeEnabled; s.tubeType = tubeType;
    s.tubeDriveDb = tubeDriveDb; s.tubeBiasPct = tubeBiasPct; s.tubeMixPct = tubeMixPct;
    s.blendPct = blendPct;
    s.tapeEnabled = tapeEnabled; s.tapeType = tapeType;
    s.tapeDriveDb = tapeDriveDb; s.flutterPct = flutterPct; s.wowPct = wowPct; s.tapeMixPct = tapeMixPct;
    s.toneEnabled = toneEnabled; s.toneLowDb = toneLowDb; s.toneHighDb = toneHighDb;
    s.characterMode = characterMode;
    s.outputEnabled = outputEnabled; s.outputTrimDb = outputTrimDb;
    s.softClip = softClip; s.masterMix = masterMix;
    return s;
  }

  void applySnapshot(const Snapshot &s) noexcept {
    tubeEnabled = s.tubeEnabled; tubeType = s.tubeType;
    tubeDriveDb = s.tubeDriveDb; tubeBiasPct = s.tubeBiasPct; tubeMixPct = s.tubeMixPct;
    blendPct = s.blendPct;
    tapeEnabled = s.tapeEnabled; tapeType = s.tapeType;
    tapeDriveDb = s.tapeDriveDb; flutterPct = s.flutterPct; wowPct = s.wowPct; tapeMixPct = s.tapeMixPct;
    toneEnabled = s.toneEnabled; toneLowDb = s.toneLowDb; toneHighDb = s.toneHighDb;
    characterMode = s.characterMode;
    outputEnabled = s.outputEnabled; outputTrimDb = s.outputTrimDb;
    softClip = s.softClip; masterMix = s.masterMix;
    updateFilters();
  }

  double readDelay(const std::array<double, kDelayBufSize> &buf,
                   double delaySamples) const noexcept {
    const double d = std::clamp(delaySamples, 1.0, static_cast<double>(kDelayBufSize - 3));
    const int i0 = static_cast<int>(d);
    const double frac = d - static_cast<double>(i0);
    const int idx0 = (delayWriteIdx - i0) & (kDelayBufSize - 1);
    const int idx1 = (delayWriteIdx - i0 - 1) & (kDelayBufSize - 1);
    return buf[static_cast<size_t>(idx0)] * (1.0 - frac) +
           buf[static_cast<size_t>(idx1)] * frac;
  }

  static double shapeTube(int type, double x) noexcept {
    switch (static_cast<TubeType>(type)) {
    case TubeType::T_12AX7: // High-gain warm triode, strong 2nd harmonic
      return (x >= 0.0) ? std::tanh(x * 1.25) : (std::tanh(x * 0.88) * 1.04);
    case TubeType::T_12AU7: // Smooth clean headroom
      return std::tanh(x * 0.85) / 0.92;
    case TubeType::T_12AT7: // Balanced crisp driver
      return (2.0 / 3.14159265358979323846) * std::atan(x * 1.15);
    case TubeType::T_6L6: // Power tube warm punch
      return std::tanh(x * 1.10 + 0.08 * x * x);
    case TubeType::T_EL34: // British crunch odd+even
      return std::tanh(x * 1.45 - 0.10 * x * x * x);
    }
    return std::tanh(x);
  }

  static double shapeTape(int type, double x) noexcept {
    switch (static_cast<TapeType>(type)) {
    case TapeType::IPS15_Quarter: // Warm thick saturation
      return std::tanh(x * 1.15);
    case TapeType::IPS30_Half: // Clean wide mastering tape
      return std::tanh(x * 0.85) / 0.90;
    case TapeType::IPS30_Two: // Studio multitrack punch
      return std::tanh(x * 1.02);
    case TapeType::VintageTape: // Rich compression & grit
      return std::tanh(x * 1.35 + 0.06 * x * x);
    }
    return std::tanh(x);
  }

  double applyCharacter(double x, Biquad &midEq) noexcept {
    switch (static_cast<CharacterMode>(characterMode)) {
    case CharacterMode::Warm:
      return std::tanh(x * 0.95) / 0.95;
    case CharacterMode::Vintage: {
      const double y = sanitize(midEq.process(x));
      return std::tanh(y * 1.12 + 0.05 * y * y) / 1.08;
    }
    case CharacterMode::Aggressive: {
      const double y = sanitize(midEq.process(x));
      return std::tanh(y * 1.45 + 0.18 * y * y * y) / 1.20;
    }
    }
    return x;
  }

  void updateFilters() noexcept {
    // Tape head bump & HF rolloff per tape type
    double bumpHz = 65.0, bumpDb = 1.2, lpfHz = 18000.0;
    switch (static_cast<TapeType>(tapeType)) {
    case TapeType::IPS15_Quarter: bumpHz = 62.0; bumpDb = 1.8; lpfHz = 14500.0; break;
    case TapeType::IPS30_Half:    bumpHz = 85.0; bumpDb = 0.8; lpfHz = 19500.0; break;
    case TapeType::IPS30_Two:     bumpHz = 72.0; bumpDb = 1.4; lpfHz = 17500.0; break;
    case TapeType::VintageTape:   bumpHz = 58.0; bumpDb = 2.4; lpfHz = 11500.0; break;
    }
    const auto tb = makePeakingEq(cachedSampleRate, bumpHz, bumpDb, 0.9);
    tapeBumpL.setCoefficients(tb);
    tapeBumpR.setCoefficients(tb);
    const auto tlpf = makeButterworthLPF(cachedSampleRate, std::min(lpfHz, cachedSampleRate * 0.45));
    tapeHeadLpfL.setCoefficients(tlpf);
    tapeHeadLpfR.setCoefficients(tlpf);

    // Post Tone Low (100 Hz) & High (8 kHz)
    const auto lc = makePeakingEq(cachedSampleRate, 100.0, toneLowDb, 0.7);
    const auto hc = makePeakingEq(cachedSampleRate, 8000.0, toneHighDb, 0.7);
    toneLowL.setCoefficients(lc);
    toneLowR.setCoefficients(lc);
    toneHighL.setCoefficients(hc);
    toneHighR.setCoefficients(hc);

    // Character mid presence (Vintage: +1.5 dB @ 1.4 kHz; Aggressive: +2.5 dB @ 2.8 kHz)
    const double cFreq = (characterMode == 2) ? 2800.0 : 1400.0;
    const double cDb = (characterMode == 0) ? 0.0 : (characterMode == 1 ? 1.5 : 2.5);
    const auto cc = makePeakingEq(cachedSampleRate, cFreq, cDb, 0.8);
    charMidL.setCoefficients(cc);
    charMidR.setCoefficients(cc);
  }

  static void updateMeter(std::atomic<float> &m, double peak) noexcept {
    const float db =
        (peak <= 1.0e-6)
            ? -60.0f
            : static_cast<float>(std::clamp(20.0 * std::log10(peak), -60.0, 6.0));
    const float prev = m.load(std::memory_order_relaxed);
    m.store(db > prev ? db : (prev * 0.999f + db * 0.001f),
            std::memory_order_relaxed);
  }

  double cachedSampleRate = 48000.0;
  bool tubeEnabled = true;
  int tubeType = 0;
  double tubeDriveDb = 6.0;
  double tubeBiasPct = 15.0;
  double tubeMixPct = 100.0;

  double blendPct = 50.0;

  bool tapeEnabled = true;
  int tapeType = 2;
  double tapeDriveDb = 6.0;
  double flutterPct = 10.0;
  double wowPct = 10.0;
  double tapeMixPct = 100.0;

  bool toneEnabled = true;
  double toneLowDb = 1.5;
  double toneHighDb = -0.5;

  int characterMode = 0;

  bool outputEnabled = true;
  double outputTrimDb = 0.0;
  bool softClip = true;
  double masterMix = 1.0;
  bool vuSourceInput = false;

  int currentPresetIndex = 0;
  bool activeSlotB = false;
  bool hasUserSaved = false;
  Snapshot slotA{}, slotB{}, userSaved{};

  Biquad tapeHeadLpfL, tapeHeadLpfR, tapeBumpL, tapeBumpR;
  Biquad toneLowL, toneLowR, toneHighL, toneHighR, charMidL, charMidR;
  double dcX1L = 0.0, dcY1L = 0.0, dcX1R = 0.0, dcY1R = 0.0;
  double prevTubeL = 0.0, prevTubeR = 0.0, prevTapeL = 0.0, prevTapeR = 0.0;
  double wowPhase = 0.0, flutterPhase = 0.0;
  std::array<double, kDelayBufSize> delayBufL{}, delayBufR{};
  int delayWriteIdx = 0;

  std::atomic<float> inLevelDb{-60.0f};
  std::atomic<float> outLevelDb{-60.0f};
  std::atomic<float> vuLevelDb{-20.0f};
};

} // namespace nova::graviton
