#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// MIDAS Heritage-D HD96-24 Chapter 35: Channel AI & Automatic Acoustic
// Profiling
// ==============================================================================
// ==============================================================================
// 1. DUA JENIS KECERDASAN (TWO AI INTELLIGENCE MODES)
// ==============================================================================
enum class AIMode {
  SoundEngineer = 0,  // Mode Penasihat: Analisis & rekomendasi untuk operator
                      // (Aman untuk Live Show)
  AutonomousMixer = 1 // Mode Otonom: Otomatis mengatur Gain, EQ, Comp, Gate,
                      // FX, Monitor secara berkala
};

inline const char *aiModeToString(AIMode mode) noexcept {
  switch (mode) {
  case AIMode::SoundEngineer:
    return "AI Sound Engineer (Advisory Mode)";
  case AIMode::AutonomousMixer:
    return "AI Autonomous Mixer (Auto-Pilot Mode)";
  default:
    return "AI Sound Engineer";
  }
}

// ==============================================================================
// 2. DELAPAN MODUL AI OTOMATIS (8 CORE AUTONOMOUS MODULES)
// ==============================================================================
enum class AIModuleId {
  Gain = 0,       // Menjaga level input optimal (-18 dBFS nominal)
  EQ = 1,         // Mengatur frekuensi sesuai karakter instrumen
  Compressor = 2, // Mengontrol dinamika suara
  Gate = 3,       // Mengurangi kebocoran suara panggung
  FX = 4,         // Memilih & mengatur Reverb, Delay, Chorus
  Monitor = 5,    // Membuat mix monitor tiap musisi (Aux 1-4)
  Feedback = 6,   // Mendeteksi & mengurangi feedback (AFS Notch)
  Master = 7,     // Mengoptimalkan keseluruhan mix (True-Peak, LUFS)
  TotalModules = 8
};

inline const char *aiModuleToString(AIModuleId mod) noexcept {
  switch (mod) {
  case AIModuleId::Gain:
    return "AI Gain (Auto-Trim)";
  case AIModuleId::EQ:
    return "AI EQ (Spectral Balance)";
  case AIModuleId::Compressor:
    return "AI Compressor (Dynamics)";
  case AIModuleId::Gate:
    return "AI Gate (Anti-Bleed)";
  case AIModuleId::FX:
    return "AI FX (Spatial & Time)";
  case AIModuleId::Monitor:
    return "AI Monitor (Stage Aux)";
  case AIModuleId::Feedback:
    return "AI Feedback (AFS Killer)";
  case AIModuleId::Master:
    return "AI Master (Loudness)";
  default:
    return "AI Module";
  }
}

// ==============================================================================
// 3. SAFETY LIMITS & SLEW RATE GUARD RAILS (PROTEKSI SUARA MENDADAK)
// ==============================================================================
struct AISafetyLimits {
  // Batas minimum & maksimum (Clamping Protection)
  float minInputTrimDb = -18.0f;
  float maxInputTrimDb = 18.0f;
  float minEqGainDb = -12.0f;
  float maxEqGainDb = 6.0f;
  float minCompThresholdDb = -36.0f;
  float maxCompThresholdDb = -6.0f;
  float minGateThresholdDb = -60.0f;
  float maxGateThresholdDb = -20.0f;
  float minFaderDb = -60.0f;
  float maxFaderDb = 6.0f;

  // Batas kecepatan perubahan per detik (Slewing Rate Limit)
  // Mencegah perubahan suara mendadak, zipper noise, dan audio clicks
  float maxTrimRateDbPerSec = 1.0f;       // Maks. 1.0 dB/detik
  float maxEqRateDbPerSec = 1.5f;         // Maks. 1.5 dB/detik
  float maxCompThreshRateDbPerSec = 2.0f; // Maks. 2.0 dB/detik
  float maxGateThreshRateDbPerSec = 3.0f; // Maks. 3.0 dB/detik
  float maxFaderRateDbPerSec = 1.5f;      // Maks. 1.5 dB/detik
};

inline float smoothSlew(float current, float target, float maxDelta) noexcept {
  const float diff = target - current;
  if (std::abs(diff) <= maxDelta)
    return target;
  return (diff > 0.0f) ? (current + maxDelta) : (current - maxDelta);
}

enum class InstrumentType {
  Unknown = 0,
  KickDrum,
  SnareDrum,
  BassGuitar,
  Vocal,
  ElectricGuitar,
  AcousticPiano
};

inline const char *instrumentToString(InstrumentType type) noexcept {
  switch (type) {
  case InstrumentType::KickDrum:
    return "Kick Drum";
  case InstrumentType::SnareDrum:
    return "Snare Drum";
  case InstrumentType::BassGuitar:
    return "Bass Guitar";
  case InstrumentType::Vocal:
    return "Vocal (Lead/Backing)";
  case InstrumentType::ElectricGuitar:
    return "Electric/Acoustic Guitar";
  case InstrumentType::AcousticPiano:
    return "Acoustic Piano";
  case InstrumentType::Unknown:
  default:
    return "Generic Instrument";
  }
}

struct AcousticFeatures {
  double peak = 0.0;
  double rms = 0.0;
  double peakDb = -90.0;
  double rmsDb = -90.0;
  double crestFactor = 1.0;
  double zcr = 0.0;
  double energyLow = 0.0;
  double energyLoMid = 0.0;
  double energyHiMid = 0.0;
  double energyHigh = 0.0;
  double spectralCentroid = 1000.0;
  double lowEnergyRatio = 0.0;
  double highEnergyRatio = 0.0;
  size_t sampleCount = 0;
  double durationSec = 0.0;
  bool clipped = false;
  bool valid = false;
};

struct AIRecommendation {
  bool valid = false;
  bool insufficientData = false;
  bool clipped = false;
  int sourceChannel = -1;

  InstrumentType detectedInstrument = InstrumentType::Unknown;
  float confidence = 0.0f; // 0.0 to 1.0

  // Auto-Trim (targets 0 dBu studio nominal = -18 dBFS RMS)
  float targetTrimDb = 0.0f;

  // HPF recommendations
  bool hpfEnabled = true;
  float hpfFreqHz = 80.0f;
  int hpfSlope = 0; // 0 = 12 dB/oct, 1 = 18 dB/oct

  // 4-band Parametric EQ recommendations
  std::array<float, 4> eqFreqHz = {100.0f, 400.0f, 2500.0f, 10000.0f};
  std::array<float, 4> eqGainDb = {0.0f, 0.0f, 0.0f, 0.0f};
  std::array<float, 4> eqQ = {1.0f, 1.4f, 1.4f, 1.0f};

  // Gate recommendations
  bool gateEnabled = false;
  float gateThresholdDb = -45.0f;
  float gateRatio = 3.0f;
  float gateAttackMs = 1.0f;
  float gateReleaseMs = 100.0f;

  // Compressor recommendations (Modul 3: AI Compressor)
  // Maps directly to nova::graviton::CompressorMode (0=Off, 1=Corrective, 2=Adaptive, 3=Creative, 4=Vintage)
  bool compEnabled = true;
  float compThresholdDb = -18.0f;
  float compRatio = 3.0f;
  float compAttackMs = 15.0f;
  float compReleaseMs = 150.0f;
  int compMode = 2; // Adaptive (Optical Tube)

  // Modul 5: AI FX (Spatial Reverb, Delay & Modulation)
  int fxSlot = 0;          // 0: Plate Reverb, 1: Tape Delay, 2: Stereo Chorus
  float fxSendDb = -12.0f; // Send level
  float fxDecaySec = 2.2f; // Decay time

  // Modul 6: AI Monitor (Stage Aux Sends 1-4)
  std::array<float, 4> auxSendDb = {
      0.0f, -3.0f, -6.0f,
      -6.0f}; // Aux 1 Lead Wedge, Aux 2 Band, Aux 3 Drum IEM, Aux 4 Keys IEM

  // Modul 7: AI Feedback Suppression (AFS Notch)
  bool afsActive = false;
  float afsFreqHz = 0.0f; // Detected ringing frequency
  float afsNotchGainDb = -12.0f;
  float afsQ = 9.0f;

  // Modul 8: AI Master & Loudness Control
  bool masterLimiterEnabled = true;
  float masterCeilingDb = -0.1f;
  float targetLufs = -14.0f;
};

// ==============================================================================
// 4. FITUR LANJUTAN AI (SIX ADVANCED AI INTELLIGENCE ENGINES)
// ==============================================================================
// 1. Self Learning: AI mempelajari karakter ruangan dan instrumen
struct SelfLearningMemory {
  int totalLearnedProfiles = 0;
  float learnedRoomResonanceHz[3] = {63.0f, 250.0f, 4000.0f};
  float learnedRoomCutDb[3] = {0.0f, 0.0f, 0.0f};
  float vocalFormantPeakHz = 3200.0f;
  float vocalWarmthHz = 320.0f;
  float confidence = 0.0f;
  bool memoryActive = false;
};

// 2. Auto Soundcheck: AI melakukan konfigurasi awal seluruh channel secara
// simultan
struct AutoSoundcheckResult {
  int auditedChannels = 0;
  float targetNominalDb = -18.0f;
  float averageHeadroomDb = 0.0f;
  bool phaseAligned = false;
  bool clippingDetected = false;
};

// 3. Auto Scene: AI mengenali perubahan lagu dan kebutuhan mixing
enum class AutoSceneStyle {
  BalladAcoustic = 0,
  UpbeatPop = 1,
  HighEnergyRock = 2,
  SpeechBreak = 3
};

inline const char *autoSceneStyleToString(AutoSceneStyle s) noexcept {
  switch (s) {
  case AutoSceneStyle::BalladAcoustic:
    return "Ballad / Acoustic (Lush Reverb, Opto 2:1)";
  case AutoSceneStyle::UpbeatPop:
    return "Upbeat Pop (Tight VCA, Rhythmic Delay)";
  case AutoSceneStyle::HighEnergyRock:
    return "High-Energy Rock (Punchy Transients, Tight Gate)";
  case AutoSceneStyle::SpeechBreak:
    return "Speech / MC Break (Dugan Automixer, Ducked FX)";
  default:
    return "Live Scene";
  }
}

// 4. Multi-Mic Intelligence: AI mengurangi benturan frekuensi antarmikrofon
// (Spectral Unmasking)
struct MultiMicUnmaskingData {
  bool leadVoxUnmasked = false;  // Carved guitars/keys @ 2.8 kHz (-3 dB)
  bool kickBassUnmasked = false; // Carved bass @ 80 Hz (-4 dB) for kick punch
  int detectedCollisions = 0;
  float dynamicSidechainDepthDb = 0.0f;
};

// 5. Stage Intelligence: AI mengenali posisi dan kebutuhan monitor musisi
struct StageIntelligenceData {
  int mappedPositions = 0;
  bool hearingProtectionLimiter = false;
  float iemCeilingDb = -6.0f; // Ear-safety ceiling
  std::array<float, 4> auxFeedSplit = {0.0f, -3.0f, 0.0f, -4.0f};
};

// 6. Acoustic Intelligence: AI menganalisis respons ruangan melalui mikrofon
// pengukuran
struct AcousticIntelligenceData {
  bool measurementMicActive = false; // Input 32 RTA reference mic
  float roomModesHz[3] = {63.0f, 250.0f, 4000.0f};
  float roomModeDeltasDb[3] = {0.0f, 0.0f, 0.0f};
  float subwooferPhaseDelayMs = 0.0f; // Sub alignment delay
  bool geqFlattened = false;
};

class ChannelAIAnalyzer final {
public:
  static constexpr size_t kMaxCaptureSamples =
      384000; // Up to 4 seconds @ 96 kHz (or 8s @ 48 kHz)

  ChannelAIAnalyzer() noexcept {
    captureBuffer.resize(kMaxCaptureSamples, 0.0f);
    for (auto &en : moduleEnabled)
      en.store(true, std::memory_order_relaxed);
    reset();
  }

  AIMode getMode() const noexcept {
    return static_cast<AIMode>(aiMode.load(std::memory_order_relaxed));
  }
  void setMode(AIMode m) noexcept {
    aiMode.store(static_cast<int>(m), std::memory_order_relaxed);
  }

  bool isModuleEnabled(AIModuleId mod) const noexcept {
    const size_t idx = static_cast<size_t>(mod);
    return (idx < 8) ? moduleEnabled[idx].load(std::memory_order_relaxed)
                     : false;
  }
  void setModuleEnabled(AIModuleId mod, bool en) noexcept {
    const size_t idx = static_cast<size_t>(mod);
    if (idx < 8)
      moduleEnabled[idx].store(en, std::memory_order_relaxed);
  }

  bool isEmergencyFrozen() const noexcept {
    return emergencyFreeze.load(std::memory_order_relaxed);
  }
  void setEmergencyFreeze(bool freeze) noexcept {
    emergencyFreeze.store(freeze, std::memory_order_relaxed);
  }

  const AISafetyLimits &getSafetyLimits() const noexcept {
    return safetyLimits;
  }

  // Advanced 6-Engine Accessors
  SelfLearningMemory &getSelfLearningMemory() noexcept { return selfLearning; }
  const SelfLearningMemory &getSelfLearningMemory() const noexcept {
    return selfLearning;
  }

  AutoSoundcheckResult &getSoundcheckResult() noexcept {
    return soundcheckResult;
  }
  const AutoSoundcheckResult &getSoundcheckResult() const noexcept {
    return soundcheckResult;
  }

  AutoSceneStyle getAutoSceneStyle() const noexcept {
    return static_cast<AutoSceneStyle>(
        currentAutoScene.load(std::memory_order_relaxed));
  }
  void setAutoSceneStyle(AutoSceneStyle s) noexcept {
    currentAutoScene.store(static_cast<int>(s), std::memory_order_relaxed);
  }

  MultiMicUnmaskingData &getMultiMicData() noexcept { return multiMicData; }
  const MultiMicUnmaskingData &getMultiMicData() const noexcept {
    return multiMicData;
  }

  StageIntelligenceData &getStageData() noexcept { return stageData; }
  const StageIntelligenceData &getStageData() const noexcept {
    return stageData;
  }

  AcousticIntelligenceData &getAcousticData() noexcept { return acousticData; }
  const AcousticIntelligenceData &getAcousticData() const noexcept {
    return acousticData;
  }

  bool learnFromRecommendation(const AIRecommendation &rec) noexcept {
    if (!rec.valid || rec.insufficientData)
      return false;
    selfLearning.learnedRoomResonanceHz[0] = rec.eqFreqHz[0];
    selfLearning.learnedRoomResonanceHz[1] = rec.eqFreqHz[1];
    selfLearning.learnedRoomResonanceHz[2] = rec.eqFreqHz[2];
    selfLearning.learnedRoomCutDb[0] = rec.eqGainDb[0];
    selfLearning.learnedRoomCutDb[1] = rec.eqGainDb[1];
    selfLearning.learnedRoomCutDb[2] = rec.eqGainDb[2];
    selfLearning.vocalWarmthHz = rec.eqFreqHz[1];
    selfLearning.vocalFormantPeakHz = rec.eqFreqHz[2];
    selfLearning.totalLearnedProfiles += 1;
    selfLearning.confidence =
        std::clamp(rec.confidence, 0.0f, 1.0f);
    selfLearning.memoryActive = true;
    return true;
  }

  void clearSelfLearningMemory() noexcept {
    selfLearning = SelfLearningMemory{};
  }

  void reset() noexcept {
    capturing.store(false, std::memory_order_release);
    completed.store(false, std::memory_order_relaxed);
    writeIndex.store(0, std::memory_order_relaxed);
    capturedCount.store(0, std::memory_order_relaxed);
    channelToProfile.store(-1, std::memory_order_relaxed);
  }

  void startProfiling(int channelIndex, double sampleRate = 48000.0,
                      double durationSec = 4.0) noexcept {
    capturing.store(false, std::memory_order_release);
    const double safeSr =
        (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
    const double safeDur = std::clamp(durationSec, 0.5, 4.0);
    const size_t target = std::clamp(
        static_cast<size_t>(std::round(safeSr * safeDur)), size_t(1),
        kMaxCaptureSamples);
    targetCaptureSamples.store(target, std::memory_order_relaxed);
    captureSampleRate.store(safeSr, std::memory_order_relaxed);
    channelToProfile.store(channelIndex, std::memory_order_relaxed);
    writeIndex.store(0, std::memory_order_relaxed);
    capturedCount.store(0, std::memory_order_relaxed);
    completed.store(false, std::memory_order_relaxed);
    capturing.store(true, std::memory_order_release);
  }

  bool isCapturing() const noexcept {
    return capturing.load(std::memory_order_relaxed);
  }

  bool isCompleted() const noexcept {
    return completed.load(std::memory_order_acquire);
  }

  float getProgress() const noexcept {
    const size_t count = capturedCount.load(std::memory_order_relaxed);
    const size_t target = std::max<size_t>(
        1, targetCaptureSamples.load(std::memory_order_relaxed));
    return std::clamp(static_cast<float>(count) / static_cast<float>(target),
                      0.0f, 1.0f);
  }

  int getTargetChannel() const noexcept {
    return channelToProfile.load(std::memory_order_relaxed);
  }

  size_t getCapturedCount() const noexcept {
    return capturedCount.load(std::memory_order_relaxed);
  }

  // Called in audio callback per-sample (real-time safe, lock-free)
  void pushSample(int channelIndex, float sample) noexcept {
    if (!capturing.load(std::memory_order_acquire))
      return;
    if (channelIndex != channelToProfile.load(std::memory_order_relaxed))
      return;

    const size_t target = targetCaptureSamples.load(std::memory_order_relaxed);
    const size_t idx = writeIndex.load(std::memory_order_relaxed);
    if (idx < target && idx < kMaxCaptureSamples) {
      captureBuffer[idx] = std::isfinite(sample) ? sample : 0.0f;
      writeIndex.store(idx + 1, std::memory_order_relaxed);
      capturedCount.store(idx + 1, std::memory_order_relaxed);
      if (idx + 1 >= target) {
        capturing.store(false, std::memory_order_relaxed);
        completed.store(true, std::memory_order_release);
      }
    }
  }

  // Analyze the captured audio buffer or a provided buffer
  AcousticFeatures extractFeatures(const float *buffer, size_t length,
                                   double sampleRate = 48000.0) const noexcept {
    AcousticFeatures feat;
    if (buffer == nullptr || length == 0)
      return feat;

    double sumSq = 0.0;
    double maxAbs = 0.0;
    size_t zeroCrossings = 0;

    // Bandpass filter simulations for energy breakdown:
    // 1. Low: Butterworth LPF @ 150 Hz
    // 2. LoMid: Peaking/Bandpass 150-800 Hz
    // 3. HiMid: Peaking/Bandpass 800-4000 Hz
    // 4. High: Butterworth HPF @ 4000 Hz
    Biquad fltLow, fltLoMidHpf, fltLoMidLpf, fltHiMidHpf, fltHiMidLpf, fltHigh;
    fltLow.setCoefficients(makeButterworthLPF(sampleRate, 150.0, 0.7071));
    fltLoMidHpf.setCoefficients(makeButterworthHPF(sampleRate, 150.0, 0.7071));
    fltLoMidLpf.setCoefficients(makeButterworthLPF(sampleRate, 800.0, 0.7071));
    fltHiMidHpf.setCoefficients(makeButterworthHPF(sampleRate, 800.0, 0.7071));
    fltHiMidLpf.setCoefficients(makeButterworthLPF(sampleRate, 4000.0, 0.7071));
    fltHigh.setCoefficients(makeButterworthHPF(sampleRate, 4000.0, 0.7071));

    double sumLow = 0.0, sumLoMid = 0.0, sumHiMid = 0.0, sumHigh = 0.0;

    for (size_t i = 0; i < length; ++i) {
      const double s = static_cast<double>(buffer[i]);
      const double absS = std::abs(s);
      if (absS > maxAbs)
        maxAbs = absS;
      sumSq += s * s;

      if (i > 0) {
        if ((buffer[i] >= 0.0f && buffer[i - 1] < 0.0f) ||
            (buffer[i] < 0.0f && buffer[i - 1] >= 0.0f)) {
          zeroCrossings++;
        }
      }

      const double yLow = fltLow.process(s);
      const double yLoMid = fltLoMidLpf.process(fltLoMidHpf.process(s));
      const double yHiMid = fltHiMidLpf.process(fltHiMidHpf.process(s));
      const double yHigh = fltHigh.process(s);

      sumLow += yLow * yLow;
      sumLoMid += yLoMid * yLoMid;
      sumHiMid += yHiMid * yHiMid;
      sumHigh += yHigh * yHigh;
    }

    const double safeSr =
        (std::isfinite(sampleRate) && sampleRate >= 8000.0) ? sampleRate : 48000.0;
    feat.sampleCount = length;
    feat.durationSec = static_cast<double>(length) / safeSr;
    feat.peak = maxAbs;
    feat.clipped = (maxAbs >= 0.99);
    feat.rms = std::sqrt(sumSq / static_cast<double>(length));
    feat.peakDb = (feat.peak > 1.0e-7) ? 20.0 * std::log10(feat.peak) : -90.0;
    feat.rmsDb = (feat.rms > 1.0e-7) ? 20.0 * std::log10(feat.rms) : -90.0;
    feat.valid = (feat.durationSec >= 0.45) && (feat.rmsDb > -65.0);
    feat.crestFactor = feat.peak / (feat.rms + 1.0e-9);
    feat.zcr = static_cast<double>(zeroCrossings) /
               static_cast<double>(length > 1 ? length - 1 : 1);

    feat.energyLow = sumLow;
    feat.energyLoMid = sumLoMid;
    feat.energyHiMid = sumHiMid;
    feat.energyHigh = sumHigh;

    const double totalEnergy = sumLow + sumLoMid + sumHiMid + sumHigh + 1.0e-12;
    feat.lowEnergyRatio = sumLow / totalEnergy;
    feat.highEnergyRatio = sumHigh / totalEnergy;

    feat.spectralCentroid = (60.0 * sumLow + 400.0 * sumLoMid +
                             2000.0 * sumHiMid + 8000.0 * sumHigh) /
                            totalEnergy;
    return feat;
  }

  AIRecommendation
  classifyAndRecommend(const AcousticFeatures &feat) const noexcept {
    AIRecommendation rec;
    rec.clipped = feat.clipped;

    // Reject silence, noise-floor-only, or short captures so silent channels
    // never receive +36 dB preamp boost or fake instrument classification.
    if (!std::isfinite(feat.rmsDb) || feat.rmsDb <= -65.0 ||
        (feat.sampleCount > 0 && feat.durationSec < 0.45)) {
      rec.valid = false;
      rec.insufficientData = true;
      rec.detectedInstrument = InstrumentType::Unknown;
      rec.confidence = 0.0f;
      rec.targetTrimDb = 0.0f;
      return rec;
    }

    rec.valid = true;
    rec.insufficientData = false;

    // Auto-Trim calculation: targets 0 dBu studio nominal = -18.0 dBFS RMS
    // Trim Delta = -18.0 - RMS_dB
    const float calculatedTrim = static_cast<float>(-18.0 - feat.rmsDb);
    rec.targetTrimDb = std::clamp(calculatedTrim, -24.0f, 36.0f);

    // Classification heuristics based on physical acoustics
    if (feat.lowEnergyRatio > 0.40 && feat.crestFactor > 3.0 &&
        feat.zcr < 0.06) {
      // Kick Drum: High transient crest factor + dominant sub energy (< 150 Hz)
      rec.detectedInstrument = InstrumentType::KickDrum;
      rec.confidence = 0.94f;

      rec.hpfEnabled = true;
      rec.hpfFreqHz = 30.0f;
      rec.hpfSlope = 1; // 18 dB/oct

      rec.eqFreqHz = {60.0f, 350.0f, 3500.0f, 10000.0f};
      rec.eqGainDb = {4.5f, -4.5f, 3.5f,
                      0.0f}; // Boost punch, cut boxiness, boost click
      rec.eqQ = {1.5f, 2.0f, 1.8f, 1.0f};

      rec.gateEnabled = true;
      rec.gateThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb - 12.0, -45.0, -20.0));
      rec.gateRatio = 6.0f;
      rec.gateAttackMs = 0.2f;
      rec.gateReleaseMs = 120.0f;

      rec.compEnabled = true;
      rec.compThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb + 2.0, -24.0, -12.0));
      rec.compRatio = 4.0f;
      rec.compAttackMs = 20.0f;
      rec.compReleaseMs = 100.0f;
      rec.compMode = 1; // Corrective (VCA Punch)
      rec.fxSlot = -1;  // Dry
      rec.fxSendDb = -90.0f;
      rec.auxSendDb = {-12.0f, -6.0f, 0.0f, -12.0f};

    } else if (feat.lowEnergyRatio > 0.35 && feat.crestFactor <= 3.2 &&
               feat.zcr < 0.07) {
      // Bass Guitar: Dominant low energy, sustained lower crest factor
      rec.detectedInstrument = InstrumentType::BassGuitar;
      rec.confidence = 0.91f;

      rec.hpfEnabled = true;
      rec.hpfFreqHz = 35.0f;
      rec.hpfSlope = 1; // 18 dB/oct

      rec.eqFreqHz = {80.0f, 450.0f, 2200.0f, 8000.0f};
      rec.eqGainDb = {3.5f, -3.0f, 2.5f,
                      -2.0f}; // Warm sub, clean low-mid mud, string bite
      rec.eqQ = {1.5f, 2.0f, 1.8f, 1.0f};

      rec.gateEnabled = true;
      rec.gateThresholdDb = -50.0f;
      rec.gateRatio = 2.5f;
      rec.gateAttackMs = 5.0f;
      rec.gateReleaseMs = 200.0f;

      rec.compEnabled = true;
      rec.compThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb - 2.0, -26.0, -14.0));
      rec.compRatio = 4.0f;
      rec.compAttackMs = 10.0f;
      rec.compReleaseMs = 220.0f;
      rec.compMode = 4; // Vintage (Vari-Mu Bass Glue)
      rec.fxSlot = -1;  // Dry
      rec.fxSendDb = -90.0f;
      rec.auxSendDb = {-8.0f, -3.0f, 0.0f, 0.0f};

    } else if (feat.crestFactor > 3.0 &&
               (feat.energyHigh + feat.energyHiMid) >
                   (feat.energyLow + feat.energyLoMid)) {
      // Snare Drum: High crest factor + strong high-mid / high sizzle and snap
      rec.detectedInstrument = InstrumentType::SnareDrum;
      rec.confidence = 0.92f;

      rec.hpfEnabled = true;
      rec.hpfFreqHz = 80.0f;
      rec.hpfSlope = 0; // 12 dB/oct

      rec.eqFreqHz = {100.0f, 220.0f, 800.0f, 5000.0f};
      rec.eqGainDb = {0.0f, 3.0f, -2.5f,
                      4.0f}; // Thump at 220Hz, ring cut at 800Hz, crack at 5kHz
      rec.eqQ = {1.0f, 1.6f, 2.2f, 1.5f};

      rec.gateEnabled = true;
      rec.gateThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb - 14.0, -45.0, -22.0));
      rec.gateRatio = 5.0f;
      rec.gateAttackMs = 0.2f;
      rec.gateReleaseMs = 150.0f;

      rec.compEnabled = true;
      rec.compThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb + 2.0, -22.0, -12.0));
      rec.compRatio = 3.5f;
      rec.compAttackMs = 15.0f;
      rec.compReleaseMs = 120.0f;
      rec.compMode = 3; // Creative (FET 76 Snap)
      rec.fxSlot = 0;   // Plate Reverb
      rec.fxSendDb = -16.0f;
      rec.auxSendDb = {-10.0f, -4.0f, 0.0f, -8.0f};

    } else if (feat.spectralCentroid >= 600.0 &&
               feat.spectralCentroid <= 2200.0 && feat.zcr >= 0.04 &&
               feat.zcr <= 0.16) {
      // Vocal: Centered around human voice formants (600 - 2200 Hz), moderate
      // ZCR
      rec.detectedInstrument = InstrumentType::Vocal;
      rec.confidence = 0.95f;

      rec.hpfEnabled = true;
      rec.hpfFreqHz = 85.0f;
      rec.hpfSlope = 0; // 12 dB/oct

      rec.eqFreqHz = {160.0f, 400.0f, 3200.0f, 12000.0f};
      rec.eqGainDb = {-2.0f, -2.5f, 3.0f,
                      3.5f}; // Cut boominess/mud, boost articulation and air
      rec.eqQ = {1.4f, 1.8f, 1.5f, 1.0f};

      rec.gateEnabled = false; // Never gate vocal by default
      rec.gateThresholdDb = -55.0f;
      rec.gateRatio = 2.0f;

      rec.compEnabled = true;
      rec.compThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb - 4.0, -26.0, -16.0));
      rec.compRatio = 2.8f;
      rec.compAttackMs = 15.0f;
      rec.compReleaseMs = 180.0f;
      rec.compMode = 2; // Adaptive (Optical Tube Stability)
      rec.fxSlot = 0;   // Vocal Plate Reverb
      rec.fxSendDb = -12.0f;
      rec.fxDecaySec = 2.4f;
      rec.auxSendDb = {0.0f, -3.0f, -8.0f,
                       -4.0f}; // Aux 1 Lead Wedge prioritized
      rec.afsActive = true;
      rec.afsFreqHz = 3150.0f;

    } else if (feat.lowEnergyRatio < 0.25 &&
               (feat.energyLoMid + feat.energyHiMid) >
                   0.65 * (feat.energyLow + feat.energyLoMid +
                           feat.energyHiMid + feat.energyHigh)) {
      // Electric / Acoustic Guitar: Midrange focused, low sub
      rec.detectedInstrument = InstrumentType::ElectricGuitar;
      rec.confidence = 0.88f;

      rec.hpfEnabled = true;
      rec.hpfFreqHz = 100.0f;
      rec.hpfSlope = 0;

      rec.eqFreqHz = {120.0f, 800.0f, 2500.0f, 7000.0f};
      rec.eqGainDb = {-1.5f, 2.0f, 2.5f, -1.0f};
      rec.eqQ = {1.2f, 1.8f, 1.6f, 1.0f};

      rec.gateEnabled = false;
      rec.compEnabled = true;
      rec.compThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb - 2.0, -24.0, -14.0));
      rec.compRatio = 2.5f;
      rec.compAttackMs = 25.0f;
      rec.compReleaseMs = 150.0f;
      rec.compMode = 1; // Corrective (VCA)
      rec.fxSlot = 2;   // Stereo Chorus / Delay
      rec.fxSendDb = -14.0f;
      rec.auxSendDb = {-6.0f, -2.0f, -6.0f, -4.0f};

    } else {
      // Acoustic Piano / Wideband: balanced across all octaves
      rec.detectedInstrument = InstrumentType::AcousticPiano;
      rec.confidence = 0.85f;

      rec.hpfEnabled = true;
      rec.hpfFreqHz = 40.0f;
      rec.hpfSlope = 0;

      rec.eqFreqHz = {100.0f, 300.0f, 2500.0f, 10000.0f};
      rec.eqGainDb = {1.0f, -1.5f, 1.5f, 2.0f};
      rec.eqQ = {1.0f, 1.5f, 1.2f, 1.0f};

      rec.gateEnabled = false;
      rec.compEnabled = true;
      rec.compThresholdDb =
          static_cast<float>(std::clamp(feat.rmsDb - 1.0, -22.0, -14.0));
      rec.compRatio = 2.0f;
      rec.compAttackMs = 30.0f;
      rec.compReleaseMs = 250.0f;
      rec.compMode = 2; // Adaptive (Optical Tube)
      rec.fxSlot = 0;   // Warm Reverb
      rec.fxSendDb = -10.0f;
      rec.auxSendDb = {-6.0f, -3.0f, -8.0f, 0.0f};
    }

    return rec;
  }

  AIRecommendation
  runProfileOnBuffer(const float *buffer, size_t length,
                     double sampleRate = 48000.0) const noexcept {
    const auto feat = extractFeatures(buffer, length, sampleRate);
    return classifyAndRecommend(feat);
  }

  AcousticFeatures
  extractCapturedFeatures(double sampleRate = 48000.0) const noexcept {
    const size_t count = std::min(
        capturedCount.load(std::memory_order_relaxed), kMaxCaptureSamples);
    if (count == 0)
      return AcousticFeatures{};
    const double sr =
        (std::isfinite(sampleRate) && sampleRate >= 8000.0)
            ? sampleRate
            : captureSampleRate.load(std::memory_order_relaxed);
    return extractFeatures(captureBuffer.data(), count, sr);
  }

  AIRecommendation
  runProfileOnCaptured(double sampleRate = 48000.0) const noexcept {
    const size_t count = std::min(
        capturedCount.load(std::memory_order_relaxed), kMaxCaptureSamples);
    if (count == 0) {
      AIRecommendation rec;
      rec.valid = false;
      rec.insufficientData = true;
      rec.sourceChannel = channelToProfile.load(std::memory_order_relaxed);
      return rec;
    }
    const double sr =
        (std::isfinite(sampleRate) && sampleRate >= 8000.0)
            ? sampleRate
            : captureSampleRate.load(std::memory_order_relaxed);
    AIRecommendation rec = runProfileOnBuffer(captureBuffer.data(), count, sr);
    rec.sourceChannel = channelToProfile.load(std::memory_order_relaxed);
    return rec;
  }

private:
  std::vector<float> captureBuffer;
  std::atomic<size_t> writeIndex{0};
  std::atomic<size_t> capturedCount{0};
  std::atomic<size_t> targetCaptureSamples{192000};
  std::atomic<double> captureSampleRate{48000.0};
  std::atomic<bool> capturing{false};
  std::atomic<bool> completed{false};
  std::atomic<int> channelToProfile{-1};

  // AI Dual-Intelligence Mode & Module Activation
  std::atomic<int> aiMode{static_cast<int>(AIMode::SoundEngineer)};
  std::array<std::atomic<bool>, 8> moduleEnabled;
  std::atomic<bool> emergencyFreeze{false};
  AISafetyLimits safetyLimits;

  // Advanced 6 Features State
  SelfLearningMemory selfLearning;
  AutoSoundcheckResult soundcheckResult;
  std::atomic<int> currentAutoScene{
      static_cast<int>(AutoSceneStyle::HighEnergyRock)};
  MultiMicUnmaskingData multiMicData;
  StageIntelligenceData stageData;
  AcousticIntelligenceData acousticData;
};

} // namespace nova::graviton
