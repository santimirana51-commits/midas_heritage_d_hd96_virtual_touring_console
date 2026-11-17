#pragma once

#include "DSP/GravitonDSP.h"
#include "Effects/GravitonMasterX.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// 1. Vintage EMT 140 Plate Reverb & Klark Teknik DN780 Digital Plate
// Reference: MIDAS Heritage-D HD96-24 Chapters 21 & 22
//
// Key Features:
// - Physical Electromechanical Steel Plate Damping (0.5 s to 12.0 s decay)
// - Three plate models: Plate A (1957 tube), Plate B (1965 solid-state), Plate
// C (1970 modern)
// - Dattorro high-density cross-coupled tank with 4 input diffusers
// - Input Tube/Transistor Drive Saturation (0 dB to +24 dB)
// - Input filter stage (OFF, 90, 125, 180, 250, 270, 360 Hz @ 12 dB/oct)
// - 2-Band Tone Shaping Shelving EQ (Low & High)
// - LFO Modulation (Rate 0.01-1 Hz, Depth 0-10)
// - M/S Stereo Width (0-200%) & Balance control
// - Dual Mode: EMT 140 Vintage Warmth vs Klark Teknik DN780 Crisp Digital Plate
// ==============================================================================

inline BiquadCoefficients makePlateLowShelf(Sample sampleRate, Sample cutoffHz,
                                            Sample gainDb) noexcept {
  const Sample A = std::pow(10.0, gainDb / 40.0);
  const Sample w0 = 2.0 * 3.14159265358979323846 *
                    std::clamp(cutoffHz, 20.0, sampleRate * 0.45) / sampleRate;
  const Sample cos_w0 = std::cos(w0);
  const Sample sin_w0 = std::sin(w0);
  const Sample alpha = sin_w0 * 0.5 * std::sqrt((A + 1.0 / A) * 0.5 + 2.0);
  const Sample two_sqrt_A_alpha = 2.0 * std::sqrt(A) * alpha;

  const Sample b0 = A * ((A + 1.0) - (A - 1.0) * cos_w0 + two_sqrt_A_alpha);
  const Sample b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0);
  const Sample b2 = A * ((A + 1.0) - (A - 1.0) * cos_w0 - two_sqrt_A_alpha);
  const Sample a0 = (A + 1.0) + (A - 1.0) * cos_w0 + two_sqrt_A_alpha;
  const Sample a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cos_w0);
  const Sample a2 = (A + 1.0) + (A - 1.0) * cos_w0 - two_sqrt_A_alpha;

  BiquadCoefficients c;
  c.b0 = b0 / a0;
  c.b1 = b1 / a0;
  c.b2 = b2 / a0;
  c.a1 = a1 / a0;
  c.a2 = a2 / a0;
  return c;
}

inline BiquadCoefficients makePlateHighShelf(Sample sampleRate, Sample cutoffHz,
                                             Sample gainDb) noexcept {
  const Sample A = std::pow(10.0, gainDb / 40.0);
  const Sample w0 = 2.0 * 3.14159265358979323846 *
                    std::clamp(cutoffHz, 20.0, sampleRate * 0.45) / sampleRate;
  const Sample cos_w0 = std::cos(w0);
  const Sample sin_w0 = std::sin(w0);
  const Sample alpha = sin_w0 * 0.5 * std::sqrt((A + 1.0 / A) * 0.5 + 2.0);
  const Sample two_sqrt_A_alpha = 2.0 * std::sqrt(A) * alpha;

  const Sample b0 = A * ((A + 1.0) + (A - 1.0) * cos_w0 + two_sqrt_A_alpha);
  const Sample b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0);
  const Sample b2 = A * ((A + 1.0) + (A - 1.0) * cos_w0 - two_sqrt_A_alpha);
  const Sample a0 = (A + 1.0) - (A - 1.0) * cos_w0 + two_sqrt_A_alpha;
  const Sample a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cos_w0);
  const Sample a2 = (A + 1.0) - (A - 1.0) * cos_w0 - two_sqrt_A_alpha;

  BiquadCoefficients c;
  c.b0 = b0 / a0;
  c.b1 = b1 / a0;
  c.b2 = b2 / a0;
  c.a1 = a1 / a0;
  c.a2 = a2 / a0;
  return c;
}

class EMT140PlateReverb final {
public:
  enum class Mode : int { EMT140_Classic = 0, KlarkTeknik_DN780 = 1 };

  static constexpr int kMaxPreDelaySamples = 48000; // 500 ms @ 96 kHz
  static constexpr size_t kMaxTankBuffer = 16384;

  struct SimpleAllPass {
    std::vector<double> buffer;
    int writePos = 0;
    int delaySamples = 100;
    double feedback = 0.5;

    void prepare(int delayLen, double fb) {
      delaySamples = std::max(1, delayLen);
      buffer.assign(static_cast<size_t>(delaySamples + 16), 0.0);
      writePos = 0;
      feedback = fb;
    }

    void reset() {
      std::fill(buffer.begin(), buffer.end(), 0.0);
      writePos = 0;
    }

    double process(double input) noexcept {
      const size_t bufSize = buffer.size();
      const int readPos =
          (writePos - delaySamples + static_cast<int>(bufSize)) %
          static_cast<int>(bufSize);
      const double bufOut = buffer[static_cast<size_t>(readPos)];
      const double w = input + feedback * bufOut;
      buffer[static_cast<size_t>(writePos)] = w;
      writePos = (writePos + 1) % static_cast<int>(bufSize);
      return bufOut - feedback * w;
    }
  };

  EMT140PlateReverb() noexcept {
    preDelayBufferL.resize(kMaxPreDelaySamples, 0.0);
    preDelayBufferR.resize(kMaxPreDelaySamples, 0.0);
    tankL1.resize(kMaxTankBuffer, 0.0);
    tankL2.resize(kMaxTankBuffer, 0.0);
    tankR1.resize(kMaxTankBuffer, 0.0);
    tankR2.resize(kMaxTankBuffer, 0.0);
    reset();
  }

  void reset() noexcept {
    std::fill(preDelayBufferL.begin(), preDelayBufferL.end(), 0.0);
    std::fill(preDelayBufferR.begin(), preDelayBufferR.end(), 0.0);
    preDelayWritePos = 0;

    for (auto &ap : inputDiffusersL)
      ap.reset();
    for (auto &ap : inputDiffusersR)
      ap.reset();
    for (auto &ap : tankDiffusersL)
      ap.reset();
    for (auto &ap : tankDiffusersR)
      ap.reset();

    std::fill(tankL1.begin(), tankL1.end(), 0.0);
    std::fill(tankL2.begin(), tankL2.end(), 0.0);
    std::fill(tankR1.begin(), tankR1.end(), 0.0);
    std::fill(tankR2.begin(), tankR2.end(), 0.0);

    tankWriteL1 = tankWriteL2 = tankWriteR1 = tankWriteR2 = 0;
    dampStateL = dampStateR = 0.0;
    lfoPhase = 0.0;

    lowCutFilterL.reset();
    lowCutFilterR.reset();
    inputHpfL.reset();
    inputHpfR.reset();
    eqLowL.reset();
    eqLowR.reset();
    eqHighL.reset();
    eqHighR.reset();
    meterVuOut.store(0.0f, std::memory_order_relaxed);
    meterVuL.store(0.0f, std::memory_order_relaxed);
    meterVuR.store(0.0f, std::memory_order_relaxed);

    updateDelays();
    updateFilters();
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
      cachedSampleRate = sr;
      updateDelays();
      updateFilters();
    }
  }

  void setDecay(double seconds) noexcept {
    decaySec = std::clamp(seconds, 0.5, 12.0);
    plateDecaySec[static_cast<size_t>(selectedPlate)] = decaySec;
    updateFeedback();
  }
  double getDecay() const noexcept { return decaySec; }

  void setPlate(int p) noexcept {
    selectedPlate = std::clamp(p, 0, 2);
    decaySec = plateDecaySec[static_cast<size_t>(selectedPlate)];
    updatePlateTuning();
    updateFeedback();
    updateFilters();
  }
  int getPlate() const noexcept { return selectedPlate; }

  void setPlateDecay(int plateIdx, double sec) noexcept {
    if (plateIdx >= 0 && plateIdx < 3) {
      plateDecaySec[static_cast<size_t>(plateIdx)] = std::clamp(sec, 0.5, 5.5);
      if (plateIdx == selectedPlate) {
        decaySec = plateDecaySec[static_cast<size_t>(plateIdx)];
        updateFeedback();
      }
    }
  }
  double getPlateDecay(int plateIdx) const noexcept {
    return (plateIdx >= 0 && plateIdx < 3)
               ? plateDecaySec[static_cast<size_t>(plateIdx)]
               : 3.0;
  }

  void setInputFilterIndex(int idx) noexcept {
    inputFilterIdx = std::clamp(idx, 0, 6);
    updateInputFilter();
  }
  int getInputFilterIndex() const noexcept { return inputFilterIdx; }

  void setStereoWidth(double w) noexcept {
    stereoWidth = std::clamp(w, 0.0, 2.0);
  }
  double getStereoWidth() const noexcept { return stereoWidth; }

  void setStereoBalance(double b) noexcept {
    stereoBalance = std::clamp(b, -1.0, 1.0);
  }
  double getStereoBalance() const noexcept { return stereoBalance; }

  void setEqEnabled(bool en) noexcept { eqEnabled = en; }
  bool isEqEnabled() const noexcept { return eqEnabled; }

  void setEqLowFreq(double f) noexcept {
    eqLowFreq = std::clamp(f, 20.0, 2000.0);
    updateEq();
  }
  double getEqLowFreq() const noexcept { return eqLowFreq; }

  void setEqLowGainDb(double g) noexcept {
    eqLowGainDb = std::clamp(g, -12.0, 12.0);
    updateEq();
  }
  double getEqLowGainDb() const noexcept { return eqLowGainDb; }

  void setEqHighFreq(double f) noexcept {
    eqHighFreq = std::clamp(f, 200.0, 20000.0);
    updateEq();
  }
  double getEqHighFreq() const noexcept { return eqHighFreq; }

  void setEqHighGainDb(double g) noexcept {
    eqHighGainDb = std::clamp(g, -12.0, 12.0);
    updateEq();
  }
  double getEqHighGainDb() const noexcept { return eqHighGainDb; }

  void setModRateHz(double r) noexcept { modRateHz = std::clamp(r, 0.01, 1.0); }
  double getModRateHz() const noexcept { return modRateHz; }

  void setModDepth(double d) noexcept { modDepth = std::clamp(d, 0.0, 10.0); }
  double getModDepth() const noexcept { return modDepth; }

  void setWetOnly(bool w) noexcept { wetOnly = w; }
  bool getWetOnly() const noexcept { return wetOnly; }

  void setPower(bool p) noexcept { powerOn = p; }
  bool getPower() const noexcept { return powerOn; }

  float getMeterVuOut() const noexcept {
    return meterVuOut.load(std::memory_order_relaxed);
  }
  float getMeterVuL() const noexcept {
    return meterVuL.load(std::memory_order_relaxed);
  }
  float getMeterVuR() const noexcept {
    return meterVuR.load(std::memory_order_relaxed);
  }

  struct Preset {
    const char *name = "Default";
    const char *category = "Classic";
    int plate = 0; // 0=A, 1=B, 2=C
    double decaySec = 3.5;
    int inputFilterIdx = 1; // 0=OFF, 1=90, 2=125, 3=180, 4=250, 5=270, 6=360 Hz
    double stereoWidth = 1.0;
    double stereoBalance = 0.0;
    bool eqEnabled = true;
    double eqLowFreq = 100.0;
    double eqLowGainDb = 0.0;
    double eqHighFreq = 8000.0;
    double eqHighGainDb = 0.0;
    double modRateHz = 0.10;
    double modDepth = 5.0;
    double preDelayMs = 20.0;
    double mix = 0.35;
    bool wetOnly = false;
  };

  static const std::vector<Preset> &getFactoryPresets() noexcept {
    static const std::vector<Preset> kPresets = {
        {"01 - Default Classic Plate A", "Classic", 0, 3.5, 1, 1.0, 0.0, true,
         100.0, 0.0, 8000.0, 0.0, 0.10, 5.0, 20.0, 0.35, false},
        {"02 - Vocal Warm Plate A", "Vocals", 0, 2.8, 2, 1.20, 0.0, true, 120.0,
         -2.0, 6500.0, 1.5, 0.15, 6.0, 35.0, 0.25, false},
        {"03 - Vocal Air Shimmer Plate C", "Vocals", 2, 3.2, 3, 1.40, 0.0, true,
         150.0, -3.0, 10000.0, 3.5, 0.25, 7.0, 45.0, 0.22, false},
        {"04 - Vocal Slap & Plate B", "Vocals", 1, 1.8, 4, 1.15, 0.0, true,
         100.0, -2.0, 8000.0, 2.0, 0.15, 5.5, 85.0, 0.28, false},
        {"05 - Snare Drum Tight Plate B", "Drums", 1, 1.4, 1, 1.0, 0.0, true,
         80.0, 1.0, 5000.0, 2.0, 0.05, 2.0, 0.0, 0.40, false},
        {"06 - Drum Room Bloom Plate A", "Drums", 0, 2.2, 0, 1.30, 0.0, true,
         100.0, 0.0, 7000.0, 0.0, 0.10, 4.0, 15.0, 0.30, false},
        {"07 - 80s Gated Ambience Plate B", "Drums", 1, 1.2, 3, 1.0, 0.0, true,
         100.0, 2.0, 9000.0, 3.0, 0.02, 2.0, 10.0, 0.50, false},
        {"08 - Acoustic Guitar Shimmer C", "Instruments", 2, 2.6, 3, 1.50, 0.0,
         true, 150.0, -1.0, 9000.0, 2.5, 0.30, 6.5, 25.0, 0.28, false},
        {"09 - Electric Guitar Solo Plate B", "Instruments", 1, 3.8, 2, 1.10,
         0.0, true, 120.0, 0.0, 6000.0, 1.0, 0.20, 5.0, 20.0, 0.32, false},
        {"10 - Piano Concert Ambience A", "Instruments", 0, 4.2, 1, 1.60, 0.0,
         true, 80.0, 0.0, 8000.0, 0.5, 0.08, 4.5, 50.0, 0.26, false},
        {"11 - Strings Lush Plate C", "Instruments", 2, 5.2, 2, 1.80, 0.0, true,
         100.0, 1.0, 10000.0, 2.0, 0.18, 8.0, 60.0, 0.35, false},
        {"12 - Abbey Road High-Pass Plate B", "Studios", 1, 2.5, 6, 1.20, 0.0,
         true, 200.0, -4.0, 7500.0, 2.0, 0.12, 5.0, 30.0, 0.30, false},
        {"13 - Capitol Smooth Chamber A", "Studios", 0, 4.8, 2, 1.40, 0.0, true,
         90.0, 0.5, 6000.0, -1.0, 0.05, 3.0, 40.0, 0.24, false},
        {"14 - Dark Vintage Echo Plate A", "Studios", 0, 5.0, 1, 0.90, 0.0,
         true, 80.0, 2.0, 4500.0, -4.0, 0.08, 4.0, 30.0, 0.35, false},
        {"15 - Deep Spatial Cavern C", "Ambient", 2, 5.5, 1, 2.0, 0.0, true,
         70.0, 1.5, 8000.0, 1.5, 0.22, 8.5, 75.0, 0.40, false},
        {"16 - Aux Send 100% Wet Vocal A", "Special", 0, 3.2, 2, 1.25, 0.0,
         true, 120.0, -1.0, 7500.0, 1.5, 0.12, 5.0, 25.0, 1.00, true}};
    return kPresets;
  }

  void loadPreset(const Preset &p) noexcept {
    setPlate(p.plate);
    setPlateDecay(p.plate, p.decaySec);
    setDecay(p.decaySec);
    setInputFilterIndex(p.inputFilterIdx);
    setStereoWidth(p.stereoWidth);
    setStereoBalance(p.stereoBalance);
    setEqEnabled(p.eqEnabled);
    setEqLowFreq(p.eqLowFreq);
    setEqLowGainDb(p.eqLowGainDb);
    setEqHighFreq(p.eqHighFreq);
    setEqHighGainDb(p.eqHighGainDb);
    setModRateHz(p.modRateHz);
    setModDepth(p.modDepth);
    setPreDelay(p.preDelayMs);
    setMix(p.mix);
    setWetOnly(p.wetOnly);
  }

  Preset getCurrentPresetState(
      const char *nameStr = "Current Settings") const noexcept {
    Preset p;
    p.name = nameStr;
    p.category = "User";
    p.plate = selectedPlate;
    p.decaySec = decaySec;
    p.inputFilterIdx = inputFilterIdx;
    p.stereoWidth = stereoWidth;
    p.stereoBalance = stereoBalance;
    p.eqEnabled = eqEnabled;
    p.eqLowFreq = eqLowFreq;
    p.eqLowGainDb = eqLowGainDb;
    p.eqHighFreq = eqHighFreq;
    p.eqHighGainDb = eqHighGainDb;
    p.modRateHz = modRateHz;
    p.modDepth = modDepth;
    p.preDelayMs = preDelayMs;
    p.mix = mix;
    p.wetOnly = wetOnly;
    return p;
  }

  void setPreDelay(double ms) noexcept {
    preDelayMs = std::clamp(ms, 0.0, 250.0);
  }
  double getPreDelay() const noexcept { return preDelayMs; }

  void setDriveDb(double db) noexcept { driveDb = std::clamp(db, 0.0, 24.0); }
  double getDriveDb() const noexcept { return driveDb; }

  void setDampingHz(double freqHz) noexcept {
    dampingHz = std::clamp(freqHz, 1000.0, 16000.0);
    updateFilters();
  }
  double getDampingHz() const noexcept { return dampingHz; }

  void setLowCutHz(double freqHz) noexcept {
    lowCutHz = std::clamp(freqHz, 20.0, 500.0);
    updateFilters();
  }
  double getLowCutHz() const noexcept { return lowCutHz; }

  void setMix(double wetRatio) noexcept {
    mix = std::clamp(wetRatio, 0.0, 1.0);
  }
  double getMix() const noexcept { return mix; }

  void setMode(int modeIdx) noexcept {
    mode = (modeIdx == 1) ? Mode::KlarkTeknik_DN780 : Mode::EMT140_Classic;
    updateDelays();
    updateFeedback();
  }
  int getMode() const noexcept { return static_cast<int>(mode); }

  void processStereo(double &inOutL, double &inOutR) noexcept {
    const double dryL = inOutL;
    const double dryR = inOutR;

    if (!powerOn) {
      inOutL = dryL;
      inOutR = dryR;
      meterVuOut.store(0.0f, std::memory_order_relaxed);
      meterVuL.store(0.0f, std::memory_order_relaxed);
      meterVuR.store(0.0f, std::memory_order_relaxed);
      return;
    }

    // Input Filter (OFF, 90, 125, 180, 250, 270, 360 Hz)
    double filteredL = dryL;
    double filteredR = dryR;
    if (inputFilterIdx > 0) {
      filteredL = inputHpfL.process(filteredL);
      filteredR = inputHpfR.process(filteredR);
    }

    // 1. Input Drive Saturation (Tube/Transistor warmth of EMT 140 / DN780
    // preamps)
    const double driveGain = dbToGain(driveDb);
    double satL = filteredL * driveGain;
    double satR = filteredR * driveGain;
    if (driveDb > 0.05) {
      satL = std::tanh(satL) / std::max(1.0, driveGain * 0.5);
      satR = std::tanh(satR) / std::max(1.0, driveGain * 0.5);
    }

    // 2. Low-Cut Rumble Filter (Plate suspension isolation)
    satL = lowCutFilterL.process(satL);
    satR = lowCutFilterR.process(satR);

    // 3. Pre-Delay
    const int preDelaySamples =
        std::clamp(static_cast<int>(preDelayMs * 0.001 * cachedSampleRate), 0,
                   kMaxPreDelaySamples - 1);
    preDelayBufferL[static_cast<size_t>(preDelayWritePos)] = satL;
    preDelayBufferR[static_cast<size_t>(preDelayWritePos)] = satR;
    const int readPos =
        (preDelayWritePos - preDelaySamples + kMaxPreDelaySamples) %
        kMaxPreDelaySamples;
    double sL = preDelayBufferL[static_cast<size_t>(readPos)];
    double sR = preDelayBufferR[static_cast<size_t>(readPos)];
    preDelayWritePos = (preDelayWritePos + 1) % kMaxPreDelaySamples;

    // 4. Input Diffuser Cascade (4 All-Pass stages creating instantaneous plate
    // echo density)
    for (size_t i = 0; i < 4; ++i) {
      sL = inputDiffusersL[i].process(sL);
      sR = inputDiffusersR[i].process(sR);
    }

    // 5. Modulated Plate Tank Loop (Dattorro figure-eight architecture)
    lfoPhase += 2.0 * 3.14159265358979323846 * modRateHz / cachedSampleRate;
    if (lfoPhase >= 2.0 * 3.14159265358979323846)
      lfoPhase -= 2.0 * 3.14159265358979323846;
    const double modAmp = modDepth * 1.2;
    const double modL = std::sin(lfoPhase) * modAmp;
    const double modR = std::cos(lfoPhase) * modAmp;

    // Left Tank Branch: In + Feedback from Right Tank
    const int lenL1 = std::clamp(static_cast<int>(tankLenL1 + modL), 16,
                                 static_cast<int>(kMaxTankBuffer - 1));
    const int readL1 =
        (tankWriteL1 - lenL1 + static_cast<int>(kMaxTankBuffer)) %
        static_cast<int>(kMaxTankBuffer);
    const double outL1 = tankL1[static_cast<size_t>(readL1)];

    const double tankInL = sL + feedbackGain * tankOutR;
    const double diffL1 = tankDiffusersL[0].process(tankInL);
    tankL1[static_cast<size_t>(tankWriteL1)] = sanitize(diffL1);
    tankWriteL1 = (tankWriteL1 + 1) % static_cast<int>(kMaxTankBuffer);

    // High-damping pad (1-pole lowpass)
    dampStateL = dampCoeff * dampStateL + (1.0 - dampCoeff) * outL1;
    const double diffL2 = tankDiffusersL[1].process(dampStateL);

    const int readL2 =
        (tankWriteL2 - tankLenL2 + static_cast<int>(kMaxTankBuffer)) %
        static_cast<int>(kMaxTankBuffer);
    tankOutL = tankL2[static_cast<size_t>(readL2)];
    tankL2[static_cast<size_t>(tankWriteL2)] = sanitize(diffL2);
    tankWriteL2 = (tankWriteL2 + 1) % static_cast<int>(kMaxTankBuffer);

    // Right Tank Branch: In + Feedback from Left Tank
    const int lenR1 = std::clamp(static_cast<int>(tankLenR1 + modR), 16,
                                 static_cast<int>(kMaxTankBuffer - 1));
    const int readR1 =
        (tankWriteR1 - lenR1 + static_cast<int>(kMaxTankBuffer)) %
        static_cast<int>(kMaxTankBuffer);
    const double outR1 = tankR1[static_cast<size_t>(readR1)];

    const double tankInR = sR + feedbackGain * tankOutL;
    const double diffR1 = tankDiffusersR[0].process(tankInR);
    tankR1[static_cast<size_t>(tankWriteR1)] = sanitize(diffR1);
    tankWriteR1 = (tankWriteR1 + 1) % static_cast<int>(kMaxTankBuffer);

    // High-damping pad (1-pole lowpass)
    dampStateR = dampCoeff * dampStateR + (1.0 - dampCoeff) * outR1;
    const double diffR2 = tankDiffusersR[1].process(dampStateR);

    const int readR2 =
        (tankWriteR2 - tankLenR2 + static_cast<int>(kMaxTankBuffer)) %
        static_cast<int>(kMaxTankBuffer);
    tankOutR = tankR2[static_cast<size_t>(readR2)];
    tankR2[static_cast<size_t>(tankWriteR2)] = sanitize(diffR2);
    tankWriteR2 = (tankWriteR2 + 1) % static_cast<int>(kMaxTankBuffer);

    // 6. Cross-Tapped Plate Output Pickups
    double wetL = 0.5 * (sL * 0.35 + outL1 - diffL2 + tankOutR * 0.6);
    double wetR = 0.5 * (sR * 0.35 + outR1 - diffR2 + tankOutL * 0.6);

    // 7. Tone Shaping EQ
    if (eqEnabled) {
      wetL = eqLowL.process(wetL);
      wetR = eqLowR.process(wetR);
      wetL = eqHighL.process(wetL);
      wetR = eqHighR.process(wetR);
    }

    // 8. Stereo Width & Balance
    const double midSignal = 0.5 * (wetL + wetR);
    const double sideSignal = 0.5 * (wetL - wetR) * stereoWidth;
    wetL = midSignal + sideSignal;
    wetR = midSignal - sideSignal;

    if (stereoBalance < 0.0) {
      wetR *= (1.0 + stereoBalance);
    } else if (stereoBalance > 0.0) {
      wetL *= (1.0 - stereoBalance);
    }

    // 9. Blend (Dry / Wet or Wet Only)
    double outL = 0.0;
    double outR = 0.0;
    if (wetOnly) {
      outL = wetL;
      outR = wetR;
    } else {
      outL = dryL * (1.0 - mix) + wetL * mix;
      outR = dryR * (1.0 - mix) + wetR * mix;
    }

    inOutL = sanitize(outL);
    inOutR = sanitize(outR);

    // 10. Analog VU Meter Ballistics (~300 ms integration)
    const float outAbsL = static_cast<float>(std::abs(inOutL));
    const float outAbsR = static_cast<float>(std::abs(inOutR));
    const float outAbs = std::max(outAbsL, outAbsR);

    const float vuCoeff = 0.9992f;
    float curVu = meterVuOut.load(std::memory_order_relaxed);
    curVu = (outAbs > curVu) ? (curVu * 0.985f + outAbs * 0.015f)
                             : (curVu * vuCoeff);
    meterVuOut.store(curVu, std::memory_order_relaxed);

    float curVuL = meterVuL.load(std::memory_order_relaxed);
    curVuL = (outAbsL > curVuL) ? (curVuL * 0.985f + outAbsL * 0.015f)
                                : (curVuL * vuCoeff);
    meterVuL.store(curVuL, std::memory_order_relaxed);

    float curVuR = meterVuR.load(std::memory_order_relaxed);
    curVuR = (outAbsR > curVuR) ? (curVuR * 0.985f + outAbsR * 0.015f)
                                : (curVuR * vuCoeff);
    meterVuR.store(curVuR, std::memory_order_relaxed);
  }

private:
  void updateInputFilter() noexcept {
    static constexpr int kInputFilterFreqs[7] = {0,   90,  125, 180,
                                                 250, 270, 360};
    if (inputFilterIdx > 0 && inputFilterIdx <= 6) {
      const double freq =
          static_cast<double>(kInputFilterFreqs[inputFilterIdx]);
      inputHpfL.setCoefficients(
          makeButterworthHPF(cachedSampleRate, freq, 0.7071));
      inputHpfR.setCoefficients(
          makeButterworthHPF(cachedSampleRate, freq, 0.7071));
    }
  }

  void updateEq() noexcept {
    if (std::abs(eqLowGainDb) > 0.01) {
      auto lowCoeff =
          makePlateLowShelf(cachedSampleRate, eqLowFreq, eqLowGainDb);
      eqLowL.setCoefficients(lowCoeff);
      eqLowR.setCoefficients(lowCoeff);
    } else {
      eqLowL.reset();
      eqLowR.reset();
      eqLowL.setCoefficients({1.0, 0.0, 0.0, 0.0, 0.0});
      eqLowR.setCoefficients({1.0, 0.0, 0.0, 0.0, 0.0});
    }

    if (std::abs(eqHighGainDb) > 0.01) {
      auto highCoeff =
          makePlateHighShelf(cachedSampleRate, eqHighFreq, eqHighGainDb);
      eqHighL.setCoefficients(highCoeff);
      eqHighR.setCoefficients(highCoeff);
    } else {
      eqHighL.reset();
      eqHighR.reset();
      eqHighL.setCoefficients({1.0, 0.0, 0.0, 0.0, 0.0});
      eqHighR.setCoefficients({1.0, 0.0, 0.0, 0.0, 0.0});
    }
  }

  void updatePlateTuning() noexcept {
    if (selectedPlate == 0) { // Plate A: warm tube 1957
      dampingHz = 4800.0;
    } else if (selectedPlate == 1) { // Plate B: solid-state 1965
      dampingHz = 6800.0;
    } else { // Plate C: modern 1970
      dampingHz = 9800.0;
    }
  }

  void updateDelays() noexcept {
    const double scale = cachedSampleRate / 44100.0;
    const double diffScale =
        (mode == Mode::KlarkTeknik_DN780)
            ? 0.65
            : (selectedPlate == 0 ? 0.78 : (selectedPlate == 1 ? 0.72 : 0.68));

    // Input diffusers (Dattorro prime delay times)
    const std::array<int, 4> diffDelaysL = {142, 107, 379, 277};
    const std::array<int, 4> diffDelaysR = {149, 113, 367, 283};
    for (size_t i = 0; i < 4; ++i) {
      inputDiffusersL[i].prepare(static_cast<int>(diffDelaysL[i] * scale),
                                 diffScale);
      inputDiffusersR[i].prepare(static_cast<int>(diffDelaysR[i] * scale),
                                 diffScale);
    }

    // Tank diffusers
    tankDiffusersL[0].prepare(static_cast<int>(672 * scale),
                              (mode == Mode::KlarkTeknik_DN780) ? 0.55 : 0.625);
    tankDiffusersL[1].prepare(static_cast<int>(1800 * scale),
                              (mode == Mode::KlarkTeknik_DN780) ? 0.50 : 0.55);
    tankDiffusersR[0].prepare(static_cast<int>(908 * scale),
                              (mode == Mode::KlarkTeknik_DN780) ? 0.55 : 0.625);
    tankDiffusersR[1].prepare(static_cast<int>(2656 * scale),
                              (mode == Mode::KlarkTeknik_DN780) ? 0.50 : 0.55);

    // Delay line lengths
    tankLenL1 = std::clamp(static_cast<int>(4453 * scale), 32,
                           static_cast<int>(kMaxTankBuffer - 1));
    tankLenL2 = std::clamp(static_cast<int>(3720 * scale), 32,
                           static_cast<int>(kMaxTankBuffer - 1));
    tankLenR1 = std::clamp(static_cast<int>(4217 * scale), 32,
                           static_cast<int>(kMaxTankBuffer - 1));
    tankLenR2 = std::clamp(static_cast<int>(3163 * scale), 32,
                           static_cast<int>(kMaxTankBuffer - 1));

    updateFeedback();
  }

  void updateFeedback() noexcept {
    // T60 decay formula: loop length ~ 150 ms
    const double avgDelaySec = 0.15;
    const double loopFeedback = std::pow(10.0, -3.0 * avgDelaySec / decaySec);
    feedbackGain = std::clamp(loopFeedback, 0.1, 0.98);
  }

  void updateFilters() noexcept {
    // High damping one-pole filter (pad)
    const double dampingOmega =
        2.0 * 3.14159265358979323846 * dampingHz / cachedSampleRate;
    dampCoeff = std::exp(-dampingOmega);

    // Low cut rumble filter
    lowCutFilterL.prepare(cachedSampleRate, lowCutHz);
    lowCutFilterR.prepare(cachedSampleRate, lowCutHz);

    updateInputFilter();
    updateEq();
  }

  double cachedSampleRate = 48000.0;
  double decaySec = 3.5;
  double preDelayMs = 20.0;
  double driveDb = 3.0;
  double dampingHz = 6500.0;
  double lowCutHz = 90.0;
  double mix = 0.35;
  Mode mode = Mode::EMT140_Classic;

  int selectedPlate = 0; // 0=A, 1=B, 2=C
  std::array<double, 3> plateDecaySec = {3.5, 2.5, 1.8};
  int inputFilterIdx = 0; // 0=OFF, 1=90, 2=125, 3=180, 4=250, 5=270, 6=360 Hz
  double stereoWidth = 1.0;
  double stereoBalance = 0.0;
  bool eqEnabled = true;
  double eqLowFreq = 100.0;
  double eqLowGainDb = 0.0;
  double eqHighFreq = 8000.0;
  double eqHighGainDb = 0.0;
  double modRateHz = 0.10;
  double modDepth = 5.0;
  bool wetOnly = false;
  bool powerOn = true;
  std::atomic<float> meterVuOut{0.0f};
  std::atomic<float> meterVuL{0.0f};
  std::atomic<float> meterVuR{0.0f};

  Biquad inputHpfL, inputHpfR;
  Biquad eqLowL, eqLowR;
  Biquad eqHighL, eqHighR;

  std::vector<double> preDelayBufferL, preDelayBufferR;
  int preDelayWritePos = 0;

  std::array<SimpleAllPass, 4> inputDiffusersL, inputDiffusersR;
  std::array<SimpleAllPass, 2> tankDiffusersL, tankDiffusersR;

  std::vector<double> tankL1, tankL2, tankR1, tankR2;
  int tankWriteL1 = 0, tankWriteL2 = 0, tankWriteR1 = 0, tankWriteR2 = 0;
  int tankLenL1 = 4453, tankLenL2 = 3720, tankLenR1 = 4217, tankLenR2 = 3163;
  double tankOutL = 0.0, tankOutR = 0.0;

  double feedbackGain = 0.75;
  double dampCoeff = 0.4;
  double dampStateL = 0.0, dampStateR = 0.0;
  double lfoPhase = 0.0;

  OnePoleHighPass lowCutFilterL, lowCutFilterR;
};

// ==============================================================================
// 2. Vintage AKG BX 20 Dual-Tank Spring Reverb (Torsion Coil Emulation)
// Reference: MIDAS Heritage-D HD96-24 Chapter 22 & AKG BX 20 Mk II Hardware
//
// Key Features:
// - Authentic Dual-Tank Helical Torsion Coil physical model (Tank A & Tank B)
// - Transient Dispersion Network with cascaded all-pass chirps ("spring boing /
// drip")
// - Independent Channel 1 & Channel 2 Decay (0.5 s to 6.0 s), Volume (-60 to
// +12 dB), and Pan (L-C-R)
// - Input Section: Input Gain (-12 to +12 dB), Mono/Stereo mode, Direct signal
// bypass
// - Tank Select: Tank A, Tank B, Tank A/B (stereo dual-tank excitation)
// - Tonal Section: Bass (-12 to +12 dB), Treble (-12 to +12 dB), Low Cut (80 Hz
// HPF)
// - Blend Section: Dry/Wet mix (0 to 100%), Wet Solo, Pre-Delay (0 to 250 ms)
// - Master Link switch and Amber Incandescent Jewel Lamp
// - 10 Studio Factory Presets with A/B Compare and Undo/Redo support
// ==============================================================================

class VintageSpringReverb final {
public:
  static constexpr size_t kMaxDelaySamples = 16384;
  static constexpr int kMaxPreDelaySamples =
      48000; // 250 ms @ 192 kHz or 500 ms @ 96 kHz
  static constexpr size_t kNumChirpStages = 6;

  struct Preset {
    const char *name = "Default BX20";
    const char *category = "Classic";
    double inputGainDb = 0.0;
    bool inputStereo = true;
    bool directEnabled = true;
    int tankSelect = 2; // 0=A, 1=B, 2=A/B
    double bassDb = 0.0;
    bool lowCut = false;
    double trebleDb = 0.0;
    double mix = 0.35;
    bool wetSolo = false;
    double preDelayMs = 0.0;
    double ch1DecaySec = 3.0;
    double ch2DecaySec = 3.5;
    double ch1VolumeDb = 0.0;
    double ch2VolumeDb = 0.0;
    double ch1Pan = -0.8;
    double ch2Pan = 0.8;
    bool link = false;
  };

  static std::vector<Preset> getFactoryPresets() {
    return {{"01 - BX20 Default", "Classic", 0.0, true, true, 2, 0.0, false,
             0.0, 0.15, false, 0.0, 3.0, 3.0, 0.0, 0.0, -0.8, 0.8, true},
            {"02 - Vintage Vocal", "Vocal", 0.0, true, true, 0, 0.0, true, 1.0,
             0.20, false, 45.0, 3.0, 3.0, 0.0, 0.0, -0.7, 0.7, true},
            {"03 - Warm Vocal", "Vocal", 0.0, true, true, 0, 1.5, true, -0.5,
             0.18, false, 55.0, 3.5, 3.5, 0.0, 0.0, -0.7, 0.7, true},
            {"04 - Bright Vocal", "Vocal", 0.0, true, true, 1, -1.0, true, 2.5,
             0.20, false, 35.0, 2.5, 2.5, 0.0, 0.0, -0.7, 0.7, true},
            {"05 - Vintage Snare", "Drums", 1.0, true, true, 1, -2.0, true, 2.0,
             0.25, false, 10.0, 2.0, 2.0, 0.0, 0.0, -0.6, 0.6, true},
            {"06 - Piano Space", "Keys", 0.0, true, true, 2, 0.5, false, 1.0,
             0.18, false, 50.0, 3.5, 3.5, 0.0, 0.0, -0.85, 0.85, true},
            {"07 - Guitar Vintage", "Guitar", 2.0, false, true, 0, 2.0, false,
             1.5, 0.20, false, 25.0, 3.0, 3.0, 0.0, 0.0, -0.9, 0.9, true},
            {"08 - Wet Solo FX Send", "Special", 0.0, true, true, 2, 0.0, true,
             1.0, 1.0, true, 0.0, 3.5, 3.5, 0.0, 0.0, -0.8, 0.8, true}};
  }

  VintageSpringReverb() noexcept {
    tankBufferA.assign(kMaxDelaySamples, 0.0);
    tankBufferB.assign(kMaxDelaySamples, 0.0);
    preDelayBufL.assign(kMaxPreDelaySamples, 0.0);
    preDelayBufR.assign(kMaxPreDelaySamples, 0.0);
    reset();
  }

  void reset() noexcept {
    std::fill(tankBufferA.begin(), tankBufferA.end(), 0.0);
    std::fill(tankBufferB.begin(), tankBufferB.end(), 0.0);
    std::fill(preDelayBufL.begin(), preDelayBufL.end(), 0.0);
    std::fill(preDelayBufR.begin(), preDelayBufR.end(), 0.0);
    writePosA = writePosB = 0;
    preDelayWritePos = 0;
    dampStateA = dampStateB = 0.0;

    for (auto &stage : chirpStagesA)
      stage.reset();
    for (auto &stage : chirpStagesB)
      stage.reset();

    lowCutFilterL.reset();
    lowCutFilterR.reset();
    bassFilterL.reset();
    bassFilterR.reset();
    trebleFilterL.reset();
    trebleFilterR.reset();

    updateDelays();
    updateFilters();
  }

  void setSampleRate(double sr) noexcept {
    if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
      cachedSampleRate = sr;
      updateDelays();
      updateFilters();
    }
  }

  // Input Section
  void setInputGainDb(double db) noexcept {
    inputGainDb = std::clamp(db, -12.0, 12.0);
  }
  double getInputGainDb() const noexcept { return inputGainDb; }

  void setInputStereo(bool stereo) noexcept { inputStereo = stereo; }
  bool isInputStereo() const noexcept { return inputStereo; }

  void setDirectEnabled(bool en) noexcept { directEnabled = en; }
  bool isDirectEnabled() const noexcept { return directEnabled; }

  void setTankSelect(int sel) noexcept { tankSelect = std::clamp(sel, 0, 2); }
  int getTankSelect() const noexcept { return tankSelect; }

  // Tonal Section (Baxandall @ 150 Hz & 5 kHz)
  void setBassDb(double db) noexcept {
    bassDb = std::clamp(db, -8.0, 8.0);
    updateEq();
  }
  double getBassDb() const noexcept { return bassDb; }

  void setLowCut(bool en) noexcept { lowCut = en; }
  bool isLowCut() const noexcept { return lowCut; }

  void setTrebleDb(double db) noexcept {
    trebleDb = std::clamp(db, -4.0, 4.0);
    updateEq();
  }
  double getTrebleDb() const noexcept { return trebleDb; }

  // Blend Section
  void setMix(double wetRatio) noexcept {
    mix = std::clamp(wetRatio, 0.0, 1.0);
  }
  double getMix() const noexcept { return mix; }

  void setWetSolo(bool solo) noexcept { wetSolo = solo; }
  bool isWetSolo() const noexcept { return wetSolo; }

  void setPreDelay(double ms) noexcept {
    preDelayMs = std::clamp(ms, 0.0, 250.0);
  }
  double getPreDelay() const noexcept { return preDelayMs; }

  // Channel 1 & Channel 2 (Decay 2.0s - 4.5s)
  void setCh1Decay(double sec) noexcept {
    ch1DecaySec = std::clamp(sec, 2.0, 4.5);
    if (link)
      ch2DecaySec = ch1DecaySec;
    updateFeedback();
  }
  double getCh1Decay() const noexcept { return ch1DecaySec; }

  void setCh2Decay(double sec) noexcept {
    ch2DecaySec = std::clamp(sec, 2.0, 4.5);
    if (link)
      ch1DecaySec = ch2DecaySec;
    updateFeedback();
  }
  double getCh2Decay() const noexcept { return ch2DecaySec; }

  void setCh1VolumeDb(double db) noexcept {
    ch1VolumeDb = std::clamp(db, -60.0, 12.0);
    if (link)
      ch2VolumeDb = ch1VolumeDb;
  }
  double getCh1VolumeDb() const noexcept { return ch1VolumeDb; }

  void setCh2VolumeDb(double db) noexcept {
    ch2VolumeDb = std::clamp(db, -60.0, 12.0);
    if (link)
      ch1VolumeDb = ch2VolumeDb;
  }
  double getCh2VolumeDb() const noexcept { return ch2VolumeDb; }

  void setCh1Pan(double pan) noexcept {
    ch1Pan = std::clamp(pan, -1.0, 1.0);
    if (link)
      ch2Pan = -ch1Pan;
  }
  double getCh1Pan() const noexcept { return ch1Pan; }

  void setCh2Pan(double pan) noexcept {
    ch2Pan = std::clamp(pan, -1.0, 1.0);
    if (link)
      ch1Pan = -ch2Pan;
  }
  double getCh2Pan() const noexcept { return ch2Pan; }

  void setLink(bool lnk) noexcept { link = lnk; }
  bool isLink() const noexcept { return link; }

  void setPower(bool pwr) noexcept { power = pwr; }
  bool getPower() const noexcept { return power; }

  // Legacy compatibility methods (Mid-engine & test backwards compatibility)
  void setDecay(double seconds) noexcept {
    setCh1Decay(seconds);
    setCh2Decay(seconds);
  }
  double getDecay() const noexcept { return ch1DecaySec; }

  void setTensionPct(double pct) noexcept {
    tensionPct = std::clamp(pct, 0.0, 100.0);
    updateFilters();
  }
  double getTensionPct() const noexcept { return tensionPct; }

  void setToneHz(double freqHz) noexcept {
    toneHz = std::clamp(freqHz, 1000.0, 12000.0);
    updateFilters();
  }
  double getToneHz() const noexcept { return toneHz; }

  void setDriveDb(double db) noexcept { driveDb = std::clamp(db, 0.0, 24.0); }
  double getDriveDb() const noexcept { return driveDb; }

  void applyPreset(const Preset &p) noexcept {
    inputGainDb = p.inputGainDb;
    inputStereo = p.inputStereo;
    directEnabled = p.directEnabled;
    tankSelect = p.tankSelect;
    bassDb = p.bassDb;
    lowCut = p.lowCut;
    trebleDb = p.trebleDb;
    mix = p.mix;
    wetSolo = p.wetSolo;
    preDelayMs = p.preDelayMs;
    ch1DecaySec = p.ch1DecaySec;
    ch2DecaySec = p.ch2DecaySec;
    ch1VolumeDb = p.ch1VolumeDb;
    ch2VolumeDb = p.ch2VolumeDb;
    ch1Pan = p.ch1Pan;
    ch2Pan = p.ch2Pan;
    link = p.link;
    updateFeedback();
    updateEq();
  }

  void processStereo(double &inOutL, double &inOutR) noexcept {
    if (!power)
      return;

    const double dryL = inOutL;
    const double dryR = inOutR;

    // 1. Pre-Delay buffer handling
    preDelayBufL[static_cast<size_t>(preDelayWritePos)] = dryL;
    preDelayBufR[static_cast<size_t>(preDelayWritePos)] = dryR;

    const int preDelaySamples =
        std::clamp(static_cast<int>(preDelayMs * 0.001 * cachedSampleRate), 0,
                   kMaxPreDelaySamples - 1);
    const int preDelayReadPos =
        (preDelayWritePos - preDelaySamples + kMaxPreDelaySamples) %
        kMaxPreDelaySamples;
    preDelayWritePos = (preDelayWritePos + 1) % kMaxPreDelaySamples;

    double inL = preDelayBufL[static_cast<size_t>(preDelayReadPos)];
    double inR = preDelayBufR[static_cast<size_t>(preDelayReadPos)];

    // 2. Mono / Stereo Input Mode
    if (!inputStereo) {
      const double monoSum = (inL + inR) * 0.5;
      inL = monoSum;
      inR = monoSum;
    }

    // 3. Input Gain & Tonal Pre-shaping (Low-Cut filter)
    const double inGain = dbToGain(inputGainDb);
    inL *= inGain;
    inR *= inGain;

    if (lowCut) {
      inL = lowCutFilterL.process(inL);
      inR = lowCutFilterR.process(inR);
    }

    // Overdrive / Saturation (Transformer & coil transducer drive)
    const double driveGain = dbToGain(driveDb);
    double satL = inL * driveGain;
    double satR = inR * driveGain;
    if (driveDb > 0.05 || std::abs(inputGainDb) > 0.05) {
      satL = std::tanh(satL) / std::max(1.0, driveGain * 0.6);
      satR = std::tanh(satR) / std::max(1.0, driveGain * 0.6);
    }

    // 4. Tank Select Routing (0 = A only, 1 = B only, 2 = A/B)
    double feedInA = 0.0;
    double feedInB = 0.0;
    if (tankSelect == 0) {
      feedInA = satL;
      feedInB = 0.0;
    } else if (tankSelect == 1) {
      feedInA = 0.0;
      feedInB = satR;
    } else {
      feedInA = satL;
      feedInB = satR;
    }

    // 5. Dispersion All-Pass Cascade ("Spring Chirp / Boing" on transients)
    double chirpedA = feedInA;
    double chirpedB = feedInB;
    for (size_t i = 0; i < kNumChirpStages; ++i) {
      chirpedA = chirpStagesA[i].process(chirpedA);
      chirpedB = chirpStagesB[i].process(chirpedB);
    }

    // 6. Dual Torsion Delay Line Loop (Tank A: ~33.7 ms, Tank B: ~41.3 ms)
    const int readPosA =
        (writePosA - delaySamplesA + static_cast<int>(kMaxDelaySamples)) %
        static_cast<int>(kMaxDelaySamples);
    const double delayedA = tankBufferA[static_cast<size_t>(readPosA)];

    const int readPosB =
        (writePosB - delaySamplesB + static_cast<int>(kMaxDelaySamples)) %
        static_cast<int>(kMaxDelaySamples);
    const double delayedB = tankBufferB[static_cast<size_t>(readPosB)];

    // Tone Damping (High-frequency steel coil absorption)
    dampStateA = dampCoeff * dampStateA + (1.0 - dampCoeff) * delayedA;
    dampStateB = dampCoeff * dampStateB + (1.0 - dampCoeff) * delayedB;

    // Passive orthogonal unitary cross-coupling between springs in the tray
    // (diag^2 + k^2 = 1.0 ensures matrix eigenvalues are strictly <=
    // feedbackGain)
    constexpr double kCross = 0.08;
    constexpr double kDiag = 0.99679486355; // sqrt(1.0 - 0.08^2)
    const double tankA_rot = kDiag * dampStateA - kCross * dampStateB;
    const double tankB_rot = kCross * dampStateA + kDiag * dampStateB;

    // Feedback with spring mechanical soft-saturation limits
    const double feedA = std::tanh(chirpedA + feedbackGainA * tankA_rot);
    const double feedB = std::tanh(chirpedB + feedbackGainB * tankB_rot);

    tankBufferA[static_cast<size_t>(writePosA)] = sanitize(feedA);
    tankBufferB[static_cast<size_t>(writePosB)] = sanitize(feedB);

    writePosA = (writePosA + 1) % static_cast<int>(kMaxDelaySamples);
    writePosB = (writePosB + 1) % static_cast<int>(kMaxDelaySamples);

    // 7. Multi-tap Output Pickups (simulates multi-coil pickups in AKG BX20
    // chamber)
    const int tapPosA2 = (writePosA - (delaySamplesA * 6) / 10 +
                          static_cast<int>(kMaxDelaySamples)) %
                         static_cast<int>(kMaxDelaySamples);
    const int tapPosB2 = (writePosB - (delaySamplesB * 6) / 10 +
                          static_cast<int>(kMaxDelaySamples)) %
                         static_cast<int>(kMaxDelaySamples);
    const double outA = delayedA * 0.75 +
                        tankBufferA[static_cast<size_t>(tapPosA2)] * 0.40 -
                        0.20 * delayedB;
    const double outB = delayedB * 0.75 +
                        tankBufferB[static_cast<size_t>(tapPosB2)] * 0.40 -
                        0.20 * delayedA;

    // 8. Individual Channel Volume and Pan (Equal-power panning)
    const double gainA = (ch1VolumeDb <= -50.0) ? 0.0 : dbToGain(ch1VolumeDb);
    const double gainB = (ch2VolumeDb <= -50.0) ? 0.0 : dbToGain(ch2VolumeDb);

    const double p1 =
        (std::clamp(ch1Pan, -1.0, 1.0) + 1.0) * 0.25 * 3.14159265358979323846;
    const double p2 =
        (std::clamp(ch2Pan, -1.0, 1.0) + 1.0) * 0.25 * 3.14159265358979323846;
    const double panL1 = std::cos(p1), panR1 = std::sin(p1);
    const double panL2 = std::cos(p2), panR2 = std::sin(p2);

    double wetL = (outA * gainA) * panL1 + (outB * gainB) * panL2;
    double wetR = (outA * gainA) * panR1 + (outB * gainB) * panR2;

    // 9. Output Tone Shelving Filters (Bass & Treble)
    wetL = trebleFilterL.process(bassFilterL.process(wetL));
    wetR = trebleFilterR.process(bassFilterR.process(wetR));
    wetL = std::clamp(wetL, -4.0, 4.0);
    wetR = std::clamp(wetR, -4.0, 4.0);

    // 10. Blend / Wet Solo / Direct Signal Path
    if (wetSolo) {
      inOutL = std::clamp(sanitize(wetL), -2.0, 2.0);
      inOutR = std::clamp(sanitize(wetR), -2.0, 2.0);
    } else {
      const double dryGain = directEnabled ? (1.0 - mix) : 0.0;
      inOutL = std::clamp(sanitize(dryL * dryGain + wetL * mix), -2.0, 2.0);
      inOutR = std::clamp(sanitize(dryR * dryGain + wetR * mix), -2.0, 2.0);
    }
  }

private:
  void updateDelays() noexcept {
  delaySamplesA = std::clamp(static_cast<int>(0.0337 * cachedSampleRate), 32,
                             static_cast<int>(kMaxDelaySamples - 1));
  delaySamplesB = std::clamp(static_cast<int>(0.0413 * cachedSampleRate), 32,
                             static_cast<int>(kMaxDelaySamples - 1));
  updateFeedback();
}

void updateFeedback() noexcept {
  const double loopSecA = static_cast<double>(delaySamplesA) / cachedSampleRate;
  const double loopSecB = static_cast<double>(delaySamplesB) / cachedSampleRate;

  const double fbA =
      std::pow(10.0, -3.0 * loopSecA / std::max(0.2, ch1DecaySec));
  const double fbB =
      std::pow(10.0, -3.0 * loopSecB / std::max(0.2, ch2DecaySec));

  feedbackGainA = std::clamp(fbA, 0.05, 0.93);
  feedbackGainB = std::clamp(fbB, 0.05, 0.93);
}

void updateFilters() noexcept {
  const double baseChirpFreq = 2200.0 + (tensionPct / 100.0) * 2600.0;
  const double qChirp = 2.2;

  for (size_t i = 0; i < kNumChirpStages; ++i) {
    const double freqOffset = (static_cast<double>(i) - 2.5) * 180.0;
    const double fA =
        std::clamp(baseChirpFreq + freqOffset, 500.0, cachedSampleRate * 0.45);
    const double fB = std::clamp(baseChirpFreq * 1.1 + freqOffset, 500.0,
                                 cachedSampleRate * 0.45);
    chirpStagesA[i].setCoefficients(
        makeAllPass2ndOrder(cachedSampleRate, fA, qChirp));
    chirpStagesB[i].setCoefficients(
        makeAllPass2ndOrder(cachedSampleRate, fB, qChirp));
  }

  const double toneOmega =
      2.0 * 3.14159265358979323846 * toneHz / cachedSampleRate;
  dampCoeff = std::exp(-toneOmega);

  lowCutFilterL.prepare(cachedSampleRate, 80.0);
  lowCutFilterR.prepare(cachedSampleRate, 80.0);

  updateEq();
}

void updateEq() noexcept {
  bassFilterL.setCoefficients(
      makePlateLowShelf(cachedSampleRate, 150.0, bassDb));
  bassFilterR.setCoefficients(
      makePlateLowShelf(cachedSampleRate, 150.0, bassDb));
  trebleFilterL.setCoefficients(
      makePlateHighShelf(cachedSampleRate, 5000.0, trebleDb));
  trebleFilterR.setCoefficients(
      makePlateHighShelf(cachedSampleRate, 5000.0, trebleDb));
}

double cachedSampleRate = 48000.0;

// Controls
double inputGainDb = 0.0;
bool inputStereo = true;
bool directEnabled = true;
int tankSelect = 2; // 0=A, 1=B, 2=A/B
double bassDb = 0.0;
bool lowCut = false;
double trebleDb = 0.0;
double mix = 0.35;
bool wetSolo = false;
double preDelayMs = 0.0;

double ch1DecaySec = 3.0;
double ch2DecaySec = 3.5;
double ch1VolumeDb = 0.0;
double ch2VolumeDb = 0.0;
double ch1Pan = -0.8;
double ch2Pan = 0.8;
bool link = false;
bool power = true;

// Legacy parameters
double tensionPct = 50.0;
double toneHz = 4500.0;
double driveDb = 6.0;

// Buffers and filters
int delaySamplesA = 1600;
int delaySamplesB = 1980;
int writePosA = 0, writePosB = 0;
int preDelayWritePos = 0;

std::vector<double> tankBufferA, tankBufferB;
std::vector<double> preDelayBufL, preDelayBufR;

double feedbackGainA = 0.70;
double feedbackGainB = 0.70;
double dampCoeff = 0.55;
double dampStateA = 0.0, dampStateB = 0.0;

std::array<Biquad, kNumChirpStages> chirpStagesA;
std::array<Biquad, kNumChirpStages> chirpStagesB;

OnePoleHighPass lowCutFilterL, lowCutFilterR;
Biquad bassFilterL, bassFilterR;
Biquad trebleFilterL, trebleFilterR;
};

} // namespace nova::graviton
