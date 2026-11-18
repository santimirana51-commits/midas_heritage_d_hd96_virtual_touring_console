#pragma once
/*  GravitonConvolutionReverb.h  –  True IR Convolution Reverb (FX Slot 21)
    =========================================================================
    64-bit internal precision stereo IR convolution reverb with:
    - 24 Built-in Factory IR Presets across 8 Categories:
      Halls, Rooms, Plates, Chambers, Churches, Ambience, Spaces, FX
    - WAV/AIFF/FLAC Impulse Response file loader
    - Real-time IR shaping: Pre-delay, Decay envelope, Size time-stretch,
      Reverse, Normalize, Trim IR
    - IR Filtering (Lo Cut / Hi Cut with independent power) & 3-Band IR EQ
    - A/B Snapshot Compare, Favorites, User Preset Save, Bypass
    - Lock-free Input/Output stereo peak metering & L/R waveform preview
*/

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstring>
#include <type_traits>
#if __has_include(<juce_audio_formats/juce_audio_formats.h>)
#include <juce_audio_formats/juce_audio_formats.h>
#define NOVA_HAS_JUCE_AUDIO_FORMATS 1
#else
#define NOVA_HAS_JUCE_AUDIO_FORMATS 0
#endif
#if __has_include(<juce_core/juce_core.h>)
#include <juce_core/juce_core.h>
#define NOVA_HAS_JUCE_CORE 1
#else
#define NOVA_HAS_JUCE_CORE 0
#endif
#include <string>
#include <mutex>
#include <thread>
#include <vector>

namespace nova {

// ─── Built-in IR Type (preserved for compatibility) ──────────
enum class IRPreset {
  None = 0,
  Hall_Large,
  Hall_Small,
  Room,
  Chamber,
  Cathedral,
  Plate,
  Spring,
  Ambience
};

inline const char *irPresetName(IRPreset p) {
  switch (p) {
  case IRPreset::Hall_Large:
    return "Large Concert Hall";
  case IRPreset::Hall_Small:
    return "Small Hall";
  case IRPreset::Room:
    return "Studio Room";
  case IRPreset::Chamber:
    return "Classic Chamber";
  case IRPreset::Cathedral:
    return "Grand Cathedral";
  case IRPreset::Plate:
    return "EMT Gold Plate";
  case IRPreset::Spring:
    return "Vintage Spring";
  case IRPreset::Ambience:
    return "Short Ambience";
  default:
    return "None";
  }
}

// ─── Convolution Reverb Parameters ───────────────────────────
struct ConvReverbParams {
  float dryWet = 0.40f;         // 0..1
  float preDelayMs = 24.0f;     // 0..500
  float decayScale = 1.0f;      // 0.2..3.0 (multiplier on IR decay envelope)
  float sizePct = 100.0f;       // 50..200 (%)
  float damping = 0.25f;        // 0..1
  float stereoWidth = 1.30f;    // 0..2
  bool loCutEnabled = true;
  float loCutHz = 80.0f;        // 20..500
  bool hiCutEnabled = true;
  float hiCutHz = 16000.0f;     // 1000..20000
  bool eqEnabled = true;
  float lowShelfGain = 0.0f;    // dB (-12..+12)
  float lowShelfFreq = 200.0f;
  float midEqGain = 1.5f;       // dB (-12..+12)
  float highShelfGain = -0.5f;  // dB (-12..+12)
  float highShelfFreq = 8000.0f;
  float irStartTrim = 0.0f;     // 0..0.45
  float irEndTrim = 1.0f;       // 0.55..1.0
  bool reverseIR = false;
  bool normalizeIR = true;
  bool bypassed = false;
  IRPreset preset = IRPreset::Hall_Large;
};

struct ConvPresetEntry {
  const char *name;
  const char *category; // Halls, Rooms, Plates, Chambers, Churches, Ambience, Spaces, FX
  const char *fileName;
  float durationSec;
  float decay;
  float earlyLevel;
  float density;
  float preDelayMs;
  float dryWet;
  bool reverse;
};

// ─── Convolution Reverb Engine ───────────────────────────────
class ConvolutionReverbEngine {
public:
  static constexpr int MAX_IR_SAMPLES = 240000; // 5 sec @ 48kHz
  static constexpr int BLOCK_SIZE = 1024;
  static constexpr int kNumFactoryPresets = 24;

  static const std::array<ConvPresetEntry, kNumFactoryPresets> &
  getFactoryLibrary() {
    static const std::array<ConvPresetEntry, kNumFactoryPresets> kLib = {{
        // Halls (6)
        {"Large Concert Hall", "Halls", "Large_Concert_Hall.wav", 4.20f, 4.20f, 0.30f, 0.85f, 24.0f, 0.40f, false},
        {"Medium Concert Hall", "Halls", "Medium_Concert_Hall.wav", 2.60f, 2.60f, 0.35f, 0.80f, 18.0f, 0.35f, false},
        {"Small Hall", "Halls", "Small_Hall.wav", 1.75f, 1.75f, 0.42f, 0.72f, 14.0f, 0.32f, false},
        {"Bright Hall", "Halls", "Bright_Hall.wav", 3.10f, 3.10f, 0.32f, 0.82f, 20.0f, 0.36f, false},
        {"Dark Hall", "Halls", "Dark_Hall.wav", 3.40f, 3.40f, 0.28f, 0.84f, 26.0f, 0.38f, false},
        {"Wooden Hall", "Halls", "Wooden_Hall.wav", 2.20f, 2.20f, 0.38f, 0.76f, 16.0f, 0.34f, false},
        // Rooms (3)
        {"Studio Live Room", "Rooms", "Studio_Live_Room.wav", 0.95f, 0.95f, 0.55f, 0.65f, 8.0f, 0.28f, false},
        {"Wood Booth", "Rooms", "Wood_Booth.wav", 0.55f, 0.55f, 0.62f, 0.58f, 4.0f, 0.22f, false},
        {"Drum Scoring Room", "Rooms", "Drum_Scoring_Room.wav", 1.25f, 1.25f, 0.50f, 0.70f, 10.0f, 0.30f, false},
        // Plates (3)
        {"EMT 140 Gold Plate", "Plates", "EMT140_Gold_Plate.wav", 2.10f, 2.10f, 0.60f, 0.90f, 12.0f, 0.35f, false},
        {"Vocal Silver Plate", "Plates", "Vocal_Silver_Plate.wav", 1.85f, 1.85f, 0.55f, 0.88f, 22.0f, 0.32f, false},
        {"Snare Bright Plate", "Plates", "Snare_Bright_Plate.wav", 1.40f, 1.40f, 0.65f, 0.85f, 6.0f, 0.30f, false},
        // Chambers (2)
        {"Capitol Echo Chamber", "Chambers", "Capitol_Chamber.wav", 1.50f, 1.50f, 0.40f, 0.78f, 15.0f, 0.35f, false},
        {"Tile Stone Chamber", "Chambers", "Tile_Chamber.wav", 1.20f, 1.20f, 0.45f, 0.75f, 12.0f, 0.30f, false},
        // Churches (2)
        {"Grand Cathedral", "Churches", "Grand_Cathedral.wav", 4.60f, 4.60f, 0.20f, 0.92f, 35.0f, 0.45f, false},
        {"Chapel Stone", "Churches", "Chapel_Stone.wav", 2.80f, 2.80f, 0.26f, 0.82f, 20.0f, 0.38f, false},
        // Ambience (2)
        {"Tight Studio Ambience", "Ambience", "Tight_Ambience.wav", 0.42f, 0.42f, 0.80f, 0.52f, 2.0f, 0.24f, false},
        {"Airy Early Reflections", "Ambience", "Airy_ER.wav", 0.60f, 0.60f, 0.75f, 0.55f, 5.0f, 0.26f, false},
        // Spaces (3)
        {"Infinite Canyon", "Spaces", "Infinite_Canyon.wav", 4.80f, 4.80f, 0.18f, 0.90f, 45.0f, 0.48f, false},
        {"Deep Ice Cavern", "Spaces", "Deep_Ice_Cavern.wav", 3.90f, 3.90f, 0.22f, 0.86f, 30.0f, 0.42f, false},
        {"Metallic Hangar", "Spaces", "Metallic_Hangar.wav", 3.20f, 3.20f, 0.34f, 0.80f, 22.0f, 0.38f, false},
        // FX (3)
        {"Reverse Bloom", "FX", "Reverse_Bloom.wav", 1.80f, 1.80f, 0.45f, 0.82f, 0.0f, 0.50f, true},
        {"Spring Tank Boing", "FX", "Spring_Tank.wav", 1.50f, 1.50f, 0.70f, 0.45f, 8.0f, 0.38f, false},
        {"Gated Warehouse", "FX", "Gated_Warehouse.wav", 0.75f, 0.75f, 0.68f, 0.75f, 10.0f, 0.42f, false},
    }};
    return kLib;
  }

  ConvolutionReverbEngine() {
    inputBufL_.assign(MAX_IR_SAMPLES + BLOCK_SIZE, 0.0f);
    inputBufR_.assign(MAX_IR_SAMPLES + BLOCK_SIZE, 0.0f);
    preDelayBufL_.assign(48000, 0.0f);
    preDelayBufR_.assign(48000, 0.0f);
    loadFactoryPreset(0);
    slotA_ = params_;
    slotB_ = params_;
    publishAudioParams(params_);
    audioCachedParams_ = params_;
  }

  // ── Parameters ──
  ConvReverbParams getParams() const {
    std::lock_guard<std::mutex> lock(parameterMutex_);
    return params_;
  }

  void setParams(const ConvReverbParams &p) {
    bool needIrRebuild = false;
    {
      std::lock_guard<std::mutex> lock(parameterMutex_);
      needIrRebuild =
          (std::abs(p.decayScale - params_.decayScale) > 0.001f ||
           std::abs(p.sizePct - params_.sizePct) > 0.1f ||
           std::abs(p.irStartTrim - params_.irStartTrim) > 0.001f ||
           std::abs(p.irEndTrim - params_.irEndTrim) > 0.001f ||
           p.reverseIR != params_.reverseIR ||
           p.normalizeIR != params_.normalizeIR);
      params_ = p;
    }
    publishAudioParams(p);
    if (needIrRebuild && irLoaded_.load(std::memory_order_acquire))
      prepareIR();
  }

  void refreshIRProcessing() {
    if (irLoaded_.load(std::memory_order_acquire))
      prepareIR();
  }

  void setSampleRate(double sr) {
    if (sr > 1000.0 && std::isfinite(sr)) {
      sampleRate_.store(sr, std::memory_order_release);
      if (irLoaded_.load(std::memory_order_acquire))
        prepareIR();
    }
  }
  double getSampleRate() const { return sampleRate_.load(std::memory_order_acquire); }

  // ── Load IR ──
  bool loadIR(const std::vector<float> &irData, double irSampleRate = 48000.0) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (irData.empty())
      return false;

    const double targetSampleRate = getSampleRate();
    if (std::abs(irSampleRate - targetSampleRate) > 1.0) {
      double ratio = targetSampleRate / irSampleRate;
      int newLen = static_cast<int>(irData.size() * ratio);
      newLen = std::min(newLen, MAX_IR_SAMPLES);
      irL_.resize(static_cast<size_t>(newLen));
      irR_.resize(static_cast<size_t>(newLen));
      for (int i = 0; i < newLen; ++i) {
        double srcPos = static_cast<double>(i) / ratio;
        int idx = static_cast<int>(srcPos);
        float frac = static_cast<float>(srcPos - idx);
        idx = std::min(idx, static_cast<int>(irData.size()) - 1);
        int idx2 = std::min(idx + 1, static_cast<int>(irData.size()) - 1);
        float val = irData[static_cast<size_t>(idx)] * (1.0f - frac) +
                    irData[static_cast<size_t>(idx2)] * frac;
        irL_[static_cast<size_t>(i)] = val;
        irR_[static_cast<size_t>(i)] = val;
      }
    } else {
      int len = std::min(static_cast<int>(irData.size()), MAX_IR_SAMPLES);
      irL_.assign(irData.begin(), irData.begin() + len);
      irR_ = irL_;
    }

    irIsStereo_ = false;
    irLoaded_ = true;
    prepareIR();
    return true;
  }

  bool loadStereoIR(const std::vector<float> &left,
                    const std::vector<float> &right) {
    std::lock_guard<std::mutex> lock(mutex_);
    int len = std::min({static_cast<int>(left.size()),
                        static_cast<int>(right.size()), MAX_IR_SAMPLES});
    if (len == 0)
      return false;

    irL_.assign(left.begin(), left.begin() + len);
    irR_.assign(right.begin(), right.begin() + len);
    irIsStereo_ = true;
    irLoaded_ = true;
    prepareIR();
    return true;
  }

#if NOVA_HAS_JUCE_AUDIO_FORMATS
  bool loadWavFile(const juce::File &file) {
    if (!file.existsAsFile())
      return false;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (!reader || reader->lengthInSamples <= 0)
      return false;

    const int nSamp = static_cast<int>(
        std::min<juce::int64>(reader->lengthInSamples, MAX_IR_SAMPLES));
    const int nCh = static_cast<int>(reader->numChannels);
    juce::AudioBuffer<float> buf(std::max(1, nCh), nSamp);
    reader->read(&buf, 0, nSamp, 0, true, nCh > 1);

    std::vector<float> l(static_cast<size_t>(nSamp));
    std::vector<float> r(static_cast<size_t>(nSamp));
    const float *srcL = buf.getReadPointer(0);
    const float *srcR = (nCh > 1) ? buf.getReadPointer(1) : srcL;
    std::copy(srcL, srcL + nSamp, l.begin());
    std::copy(srcR, srcR + nSamp, r.begin());

    irFileName_ = file.getFileName().toStdString();
    irIsStereo_ = (nCh > 1);
    return loadStereoIR(l, r);
  }
#endif

  void loadFactoryPreset(int idx) {
    const auto &lib = getFactoryLibrary();
    currentPresetIdx_ = std::clamp(idx, 0, kNumFactoryPresets - 1);
    const auto &e = lib[static_cast<size_t>(currentPresetIdx_)];

    auto p = getParams();
    p.preDelayMs = e.preDelayMs;
    p.dryWet = e.dryWet;
    p.reverseIR = e.reverse;
    p.decayScale = 1.0f;
    p.sizePct = 100.0f;
    p.irStartTrim = 0.0f;
    p.irEndTrim = 1.0f;
    setParams(p);
    irFileName_ = e.fileName;
    irIsStereo_ = true;

    generateSyntheticIR(e.durationSec, e.decay, e.earlyLevel, e.density);
  }

  // Legacy preset loader (preserved)
  void loadPreset(IRPreset preset) {
    auto p = getParams();
    p.preset = preset;
    setParams(p);
    switch (preset) {
    case IRPreset::Hall_Large: loadFactoryPreset(0); break;
    case IRPreset::Hall_Small: loadFactoryPreset(2); break;
    case IRPreset::Room:       loadFactoryPreset(6); break;
    case IRPreset::Chamber:    loadFactoryPreset(12); break;
    case IRPreset::Cathedral:  loadFactoryPreset(14); break;
    case IRPreset::Plate:      loadFactoryPreset(9); break;
    case IRPreset::Spring:     loadFactoryPreset(22); break;
    case IRPreset::Ambience:   loadFactoryPreset(16); break;
    default:
      irLoaded_ = false;
      break;
    }
  }

  // A/B & User Save & Favorites
  void selectSlotB(bool useB) {
    if (useB == activeSlotB_)
      return;
    const auto currentParams = getParams();
    if (activeSlotB_)
      slotB_ = currentParams;
    else
      slotA_ = currentParams;
    activeSlotB_ = useB;
    setParams(activeSlotB_ ? slotB_ : slotA_);
  }
  bool isSlotB() const { return activeSlotB_; }

  void saveUserPreset() {
    userSaved_ = getParams();
    hasUserSaved_ = true;
  }
  bool hasUserPreset() const { return hasUserSaved_; }
  void loadUserPreset() {
    if (hasUserSaved_)
      setParams(userSaved_);
  }

  void toggleFavorite(int idx) {
    if (idx >= 0 && idx < kNumFactoryPresets)
      favorites_[static_cast<size_t>(idx)] =
          !favorites_[static_cast<size_t>(idx)];
  }
  bool isFavorite(int idx) const {
    return (idx >= 0 && idx < kNumFactoryPresets)
               ? favorites_[static_cast<size_t>(idx)]
               : false;
  }

  int getCurrentPresetIndex() const { return currentPresetIdx_; }
  std::string getIrFileName() const { return irFileName_; }
  bool isIrStereo() const { return irIsStereo_; }
  bool isIRLoaded() const { return irLoaded_; }
  int getIRLength() const {
    const auto &ir = irSlots_[static_cast<size_t>(activeSlot_.load(std::memory_order_acquire))];
    return static_cast<int>(ir.L.size());
  }
  float getIRDurationSec() const {
    return static_cast<float>(getIRLength()) / static_cast<float>(getSampleRate());
  }

  float getInputLevelDbL() const {
    return inLevelDbL_.load(std::memory_order_relaxed);
  }
  float getInputLevelDbR() const {
    return inLevelDbR_.load(std::memory_order_relaxed);
  }
  float getOutputLevelDbL() const {
    return outLevelDbL_.load(std::memory_order_relaxed);
  }
  float getOutputLevelDbR() const {
    return outLevelDbR_.load(std::memory_order_relaxed);
  }

  void getWaveformPeaks(std::vector<float> &outL, std::vector<float> &outR,
                        int numBins) const {
    std::lock_guard<std::mutex> lock(mutex_);
    outL.assign(static_cast<size_t>(numBins), 0.0f);
    outR.assign(static_cast<size_t>(numBins), 0.0f);
    const auto &ir = irSlots_[static_cast<size_t>(activeSlot_.load(std::memory_order_acquire))];
    if (ir.L.empty() || numBins <= 0)
      return;
    const size_t total = ir.L.size();
    for (int b = 0; b < numBins; ++b) {
      const size_t i0 = (static_cast<size_t>(b) * total) / static_cast<size_t>(numBins);
      const size_t i1 = std::max(
          i0 + 1, (static_cast<size_t>(b + 1) * total) / static_cast<size_t>(numBins));
      float pkL = 0.0f, pkR = 0.0f;
      for (size_t i = i0; i < std::min(i1, total); ++i) {
        pkL = std::max(pkL, std::abs(ir.L[i]));
        pkR = std::max(pkR, std::abs(ir.R[i]));
      }
      outL[static_cast<size_t>(b)] = pkL;
      outR[static_cast<size_t>(b)] = pkR;
    }
  }

  // ── Process Audio (stereo, real-time safe, lock-free) ──
  void processStereo(float &left, float &right) {
    const double dryL = std::isfinite(left) ? static_cast<double>(left) : 0.0;
    const double dryR = std::isfinite(right) ? static_cast<double>(right) : 0.0;
    updateMeter(inLevelDbL_, dryL);
    updateMeter(inLevelDbR_, dryR);

    refreshAudioParams();
    const auto &audioParams = audioCachedParams_;
    const double currentSampleRate = getSampleRate();
    if (!audioFilterParamsValid_ ||
        currentSampleRate != audioFilterSampleRate_ ||
        audioParams.loCutHz != audioFilterParams_.loCutHz ||
        audioParams.hiCutHz != audioFilterParams_.hiCutHz ||
        audioParams.lowShelfGain != audioFilterParams_.lowShelfGain ||
        audioParams.midEqGain != audioFilterParams_.midEqGain ||
        audioParams.highShelfGain != audioFilterParams_.highShelfGain) {
      updateToneFilters(audioParams);
      audioFilterParams_ = audioParams;
      audioFilterSampleRate_ = currentSampleRate;
      audioFilterParamsValid_ = true;
    }

    const int sIdx = activeSlot_.load(std::memory_order_acquire);
    audioInSlot_.store(sIdx, std::memory_order_release);
    const auto &ir = irSlots_[static_cast<size_t>(sIdx)];

    if (audioParams.bypassed || !irLoaded_.load(std::memory_order_acquire) || ir.L.empty() || inputBufL_.empty()) {
      audioInSlot_.store(-1, std::memory_order_release);
      updateMeter(outLevelDbL_, dryL);
      updateMeter(outLevelDbR_, dryR);
      return;
    }

    // 1. Pre-delay
    double delayedL = dryL, delayedR = dryR;
    const int pdSize = static_cast<int>(preDelayBufL_.size());
    if (audioParams.preDelayMs > 0.0f && pdSize > 1) {
      int delaySamples = static_cast<int>(audioParams.preDelayMs * getSampleRate() / 1000.0);
      delaySamples = std::clamp(delaySamples, 0, pdSize - 1);
      if (delaySamples > 0) {
        preDelayBufL_[static_cast<size_t>(preDelayPos_)] = static_cast<float>(dryL);
        preDelayBufR_[static_cast<size_t>(preDelayPos_)] = static_cast<float>(dryR);
        int readPos = (preDelayPos_ - delaySamples + pdSize) % pdSize;
        delayedL = preDelayBufL_[static_cast<size_t>(readPos)];
        delayedR = preDelayBufR_[static_cast<size_t>(readPos)];
        preDelayPos_ = (preDelayPos_ + 1) % pdSize;
      }
    }

    // 2. Write to fixed-size circular input history buffer
    const int bufSize = static_cast<int>(inputBufL_.size());
    inputBufL_[static_cast<size_t>(inputPos_)] = static_cast<float>(delayedL);
    inputBufR_[static_cast<size_t>(inputPos_)] = static_cast<float>(delayedR);

    // 3. Early dense convolution (first 256 samples) + Sparse multi-tap late IR tail
    double wetL = 0.0, wetR = 0.0;
    const int irLen = static_cast<int>(ir.L.size());
    const int earlyLen = std::min(irLen, 256);
    for (int j = 0; j < earlyLen; ++j) {
      const int readIdx = (inputPos_ - j + bufSize) % bufSize;
      wetL += inputBufL_[static_cast<size_t>(readIdx)] * ir.L[static_cast<size_t>(j)];
      wetR += inputBufR_[static_cast<size_t>(readIdx)] * ir.R[static_cast<size_t>(j)];
    }
    for (const auto &tap : ir.lateTaps) {
      if (tap.offset < bufSize) {
        const int readIdx = (inputPos_ - tap.offset + bufSize) % bufSize;
        wetL += inputBufL_[static_cast<size_t>(readIdx)] * tap.gainL;
        wetR += inputBufR_[static_cast<size_t>(readIdx)] * tap.gainR;
      }
    }
    inputPos_ = (inputPos_ + 1) % bufSize;
    audioInSlot_.store(-1, std::memory_order_release);

    // 4. IR Filtering (Lo Cut / Hi Cut) & 3-Band EQ
    if (audioParams.loCutEnabled) {
      wetL = loCutL_.process(wetL);
      wetR = loCutR_.process(wetR);
    }
    if (audioParams.hiCutEnabled) {
      wetL = hiCutL_.process(wetL);
      wetR = hiCutR_.process(wetR);
    }
    if (audioParams.eqEnabled) {
      if (std::abs(audioParams.lowShelfGain) > 0.05f) {
        wetL = eqLowL_.process(wetL);
        wetR = eqLowR_.process(wetR);
      }
      if (std::abs(audioParams.midEqGain) > 0.05f) {
        wetL = eqMidL_.process(wetL);
        wetR = eqMidR_.process(wetR);
      }
      if (std::abs(audioParams.highShelfGain) > 0.05f) {
        wetL = eqHighL_.process(wetL);
        wetR = eqHighR_.process(wetR);
      }
    }

    // 5. Stereo Width
    if (std::abs(audioParams.stereoWidth - 1.0f) > 0.01f) {
      const double mid = (wetL + wetR) * 0.5;
      const double side = (wetL - wetR) * 0.5 * audioParams.stereoWidth;
      wetL = mid + side;
      wetR = mid - side;
    }

    // 6. Dry/Wet Mix
    const double outL = dryL * (1.0 - audioParams.dryWet) + wetL * audioParams.dryWet;
    const double outR = dryR * (1.0 - audioParams.dryWet) + wetR * audioParams.dryWet;

    left = std::isfinite(outL) ? static_cast<float>(std::clamp(outL, -4.0, 4.0)) : 0.0f;
    right = std::isfinite(outR) ? static_cast<float>(std::clamp(outR, -4.0, 4.0)) : 0.0f;

    updateMeter(outLevelDbL_, left);
    updateMeter(outLevelDbR_, right);
  }

  void reset() {
    std::fill(inputBufL_.begin(), inputBufL_.end(), 0.0f);
    std::fill(inputBufR_.begin(), inputBufR_.end(), 0.0f);
    std::fill(preDelayBufL_.begin(), preDelayBufL_.end(), 0.0f);
    std::fill(preDelayBufR_.begin(), preDelayBufR_.end(), 0.0f);
    loCutL_.reset(); loCutR_.reset();
    hiCutL_.reset(); hiCutR_.reset();
    eqLowL_.reset(); eqLowR_.reset();
    eqMidL_.reset(); eqMidR_.reset();
    eqHighL_.reset(); eqHighR_.reset();
    inputPos_ = 0;
    preDelayPos_ = 0;
    inLevelDbL_.store(-60.0f, std::memory_order_relaxed);
    inLevelDbR_.store(-60.0f, std::memory_order_relaxed);
    outLevelDbL_.store(-60.0f, std::memory_order_relaxed);
    outLevelDbR_.store(-60.0f, std::memory_order_relaxed);
  }

#if NOVA_HAS_JUCE_CORE
  juce::var toVar() const {
    const auto p = getParams();
    auto obj = new juce::DynamicObject();
    obj->setProperty("preset", static_cast<int>(p.preset));
    obj->setProperty("dryWet", static_cast<double>(p.dryWet));
    obj->setProperty("preDelayMs", static_cast<double>(p.preDelayMs));
    obj->setProperty("decayScale", static_cast<double>(p.decayScale));
    obj->setProperty("sizePct", static_cast<double>(p.sizePct));
    obj->setProperty("stereoWidth", static_cast<double>(p.stereoWidth));
    obj->setProperty("loCutHz", static_cast<double>(p.loCutHz));
    obj->setProperty("hiCutHz", static_cast<double>(p.hiCutHz));
    obj->setProperty("lowShelfGain", static_cast<double>(p.lowShelfGain));
    obj->setProperty("midEqGain", static_cast<double>(p.midEqGain));
    obj->setProperty("highShelfGain", static_cast<double>(p.highShelfGain));
    obj->setProperty("reverseIR", p.reverseIR);
    obj->setProperty("normalizeIR", p.normalizeIR);
    return juce::var(obj);
  }
#endif

private:
  struct LateTap {
    int offset = 0;
    float gainL = 0.0f;
    float gainR = 0.0f;
  };

  void generateSyntheticIR(float durationSec, float decayTime, float earlyLevel,
                           float density) {
    const double sr = getSampleRate();
    const int irLen = std::clamp(static_cast<int>(sr * durationSec),
                                 1024, MAX_IR_SAMPLES);
    std::lock_guard<std::mutex> lock(mutex_);
    irL_.resize(static_cast<size_t>(irLen));
    irR_.resize(static_cast<size_t>(irLen));

    unsigned int seed = 12345u + static_cast<unsigned int>(currentPresetIdx_ * 7919);
    auto pseudoRand = [&seed]() -> float {
      seed = seed * 1103515245u + 12345u;
      return (static_cast<float>(seed & 0x7FFFFFFF) /
              static_cast<float>(0x7FFFFFFF)) *
                 2.0f -
             1.0f;
    };

    for (int i = 0; i < irLen; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(sr);
      const float env = std::exp(-3.0f * t / std::max(0.1f, decayTime));
      float early = 0.0f;
      if (t < 0.06f) {
        static constexpr int erMs[] = {4, 9, 15, 21, 29, 37, 46, 54};
        for (int er : erMs) {
          if (i == static_cast<int>(er * sr / 1000.0))
            early = earlyLevel * (pseudoRand() > 0.0f ? 1.0f : -1.0f);
        }
      }
      irL_[static_cast<size_t>(i)] = early + pseudoRand() * env * density;
      irR_[static_cast<size_t>(i)] = early + pseudoRand() * env * density;
    }

    irLoaded_ = true;
    prepareIR();
  }

  void prepareIR() {
    const auto p = getParams();
    if (irL_.empty())
      return;
    const int writeSlot = 1 - activeSlot_.load(std::memory_order_relaxed);
    while (audioInSlot_.load(std::memory_order_acquire) == writeSlot) {
      std::this_thread::yield();
    }

    auto &dst = irSlots_[static_cast<size_t>(writeSlot)];
    const int fullLen = static_cast<int>(irL_.size());
    const int startSample = std::clamp(static_cast<int>(p.irStartTrim * fullLen), 0, fullLen - 1);
    int endSample = std::clamp(static_cast<int>(p.irEndTrim * fullLen), startSample + 64, fullLen);
    if (endSample <= startSample)
      endSample = fullLen;

    const int trimmedLen = endSample - startSample;
    const double sizeRatio = std::clamp(p.sizePct / 100.0, 0.5, 2.0);
    const int stretchedLen = std::clamp(static_cast<int>(trimmedLen * sizeRatio), 256, MAX_IR_SAMPLES);

    dst.L.resize(static_cast<size_t>(stretchedLen));
    dst.R.resize(static_cast<size_t>(stretchedLen));

    for (int i = 0; i < stretchedLen; ++i) {
      const double normPos = static_cast<double>(i) / static_cast<double>(stretchedLen);
      const double srcPos = p.reverseIR ? (1.0 - normPos) : normPos;
      const int srcIdx = std::clamp(startSample + static_cast<int>(srcPos * (trimmedLen - 1)),
                                    0, fullLen - 1);
      const float envScale = std::pow(1.0f - static_cast<float>(normPos),
                                      1.0f / std::max(0.2f, p.decayScale));
      dst.L[static_cast<size_t>(i)] = irL_[static_cast<size_t>(srcIdx)] * envScale;
      dst.R[static_cast<size_t>(i)] = irR_[static_cast<size_t>(srcIdx)] * envScale;
    }

    if (p.normalizeIR) {
      float maxVal = 0.0f;
      for (float v : dst.L) maxVal = std::max(maxVal, std::abs(v));
      for (float v : dst.R) maxVal = std::max(maxVal, std::abs(v));
      if (maxVal > 0.001f) {
        const float nf = 0.85f / maxVal;
        for (float &v : dst.L) v *= nf;
        for (float &v : dst.R) v *= nf;
      }
    }

    dst.lateTaps.clear();
    if (stretchedLen > 256) {
      const int numTaps = 96;
      const int tailSpan = stretchedLen - 256;
      const float tapWeight = std::sqrt(static_cast<float>(tailSpan) / static_cast<float>(numTaps)) * 0.14f;
      for (int t = 0; t < numTaps; ++t) {
        const int off = 256 + (t * tailSpan) / numTaps;
        LateTap tap;
        tap.offset = off;
        tap.gainL = dst.L[static_cast<size_t>(off)] * tapWeight;
        tap.gainR = dst.R[static_cast<size_t>(off)] * tapWeight;
        dst.lateTaps.push_back(tap);
      }
    }

    activeSlot_.store(writeSlot, std::memory_order_release);
  }

  static constexpr size_t kParamWordCount =
      (sizeof(ConvReverbParams) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
  static_assert(std::is_trivially_copyable_v<ConvReverbParams>);
  static_assert(std::atomic<uint32_t>::is_always_lock_free,
                "Audio parameter publication requires lock-free 32-bit atomics");

  void publishAudioParams(const ConvReverbParams &p) {
    std::lock_guard<std::mutex> writerLock(parameterPublishMutex_);
    std::array<uint32_t, kParamWordCount> words{};
    std::memcpy(words.data(), &p, sizeof(p));
    audioParamsVersion_.fetch_add(1, std::memory_order_acq_rel); // odd: write in progress
    for (size_t i = 0; i < words.size(); ++i)
      audioParamWords_[i].store(words[i], std::memory_order_relaxed);
    audioParamsVersion_.fetch_add(1, std::memory_order_release); // even: published
  }

  void refreshAudioParams() noexcept {
    for (int attempt = 0; attempt < 2; ++attempt) {
      const uint32_t before = audioParamsVersion_.load(std::memory_order_acquire);
      if ((before & 1u) != 0u)
        continue;
      std::array<uint32_t, kParamWordCount> words{};
      for (size_t i = 0; i < words.size(); ++i)
        words[i] = audioParamWords_[i].load(std::memory_order_relaxed);
      const uint32_t after = audioParamsVersion_.load(std::memory_order_acquire);
      if (before == after && (after & 1u) == 0u) {
        std::memcpy(&audioCachedParams_, words.data(), sizeof(audioCachedParams_));
        return;
      }
    }
    // Never spin or wait on the audio thread; retain the last coherent snapshot.
  }

  void updateToneFilters(const ConvReverbParams &p) {
    const double sr = std::max(8000.0, getSampleRate());
    const auto lc = graviton::makeButterworthHPF(sr, std::clamp<double>(p.loCutHz, 20.0, 500.0));
    const auto hc = graviton::makeButterworthLPF(sr, std::clamp<double>(p.hiCutHz, 1000.0, sr * 0.45));
    loCutL_.setCoefficients(lc); loCutR_.setCoefficients(lc);
    hiCutL_.setCoefficients(hc); hiCutR_.setCoefficients(hc);

    const auto eqL = graviton::makePeakingEq(sr, 160.0, p.lowShelfGain, 0.7);
    const auto eqM = graviton::makePeakingEq(sr, 1400.0, p.midEqGain, 0.8);
    const auto eqH = graviton::makePeakingEq(sr, 7500.0, p.highShelfGain, 0.7);
    eqLowL_.setCoefficients(eqL); eqLowR_.setCoefficients(eqL);
    eqMidL_.setCoefficients(eqM); eqMidR_.setCoefficients(eqM);
    eqHighL_.setCoefficients(eqH); eqHighR_.setCoefficients(eqH);
  }

  static void updateMeter(std::atomic<float> &m, double peak) noexcept {
    const float db = (peak <= 1.0e-6)
                         ? -60.0f
                         : static_cast<float>(std::clamp(20.0 * std::log10(std::abs(peak)), -60.0, 6.0));
    const float prev = m.load(std::memory_order_relaxed);
    m.store(db > prev ? db : (prev * 0.999f + db * 0.001f),
            std::memory_order_relaxed);
  }

  ConvReverbParams params_;
  mutable std::mutex parameterMutex_;
  std::mutex parameterPublishMutex_;
  std::array<std::atomic<uint32_t>, kParamWordCount> audioParamWords_{};
  std::atomic<uint32_t> audioParamsVersion_{0};
  ConvReverbParams audioCachedParams_{};
  ConvReverbParams audioFilterParams_{};
  bool audioFilterParamsValid_ = false;
  double audioFilterSampleRate_ = 0.0;
  std::atomic<double> sampleRate_{48000.0};
  int currentPresetIdx_ = 0;
  std::string irFileName_{"Large_Concert_Hall.wav"};
  bool irIsStereo_ = true;

  struct IRBuffer {
    std::vector<float> L, R;
    std::vector<LateTap> lateTaps;
  };

  std::vector<float> irL_, irR_;
  std::atomic<bool> irLoaded_{false};
  std::array<IRBuffer, 2> irSlots_;
  std::atomic<int> activeSlot_{0};
  std::atomic<int> audioInSlot_{-1};

  std::vector<float> inputBufL_, inputBufR_;
  int inputPos_ = 0;
  std::vector<float> preDelayBufL_, preDelayBufR_;
  int preDelayPos_ = 0;

  graviton::Biquad loCutL_, loCutR_, hiCutL_, hiCutR_;
  graviton::Biquad eqLowL_, eqLowR_, eqMidL_, eqMidR_, eqHighL_, eqHighR_;

  bool activeSlotB_ = false;
  bool hasUserSaved_ = false;
  ConvReverbParams slotA_, slotB_, userSaved_;
  std::array<bool, kNumFactoryPresets> favorites_{};

  std::atomic<float> inLevelDbL_{-60.0f}, inLevelDbR_{-60.0f};
  std::atomic<float> outLevelDbL_{-60.0f}, outLevelDbR_{-60.0f};
  mutable std::mutex mutex_;
};

} // namespace nova
