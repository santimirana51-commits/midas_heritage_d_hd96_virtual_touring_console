#pragma once
#include "Audio/AudioBufferOps.h"
#include "Audio/GainSmoothing.h"
#include "Audio/GravitonASIOStreamer.h"
#include "Audio/GravitonOfflineBounce.h"
#include "Audio/GravitonOscServer.h"
#include "Audio/HeritageVirtualTransport.h"
#include "DSP/DspCoefficients.h"
#include "DSP/GravitonChannelAI.h"
#include "DSP/GravitonDN360.h"
#include "DSP/GravitonDSP.h"
#include "DSP/GravitonDeEsser.h"
#include "DSP/GravitonDynamicEQ.h"
#include "DSP/GravitonDynamics.h"
#include "DSP/GravitonGraphicEQ.h"
#include "DSP/GravitonRTA.h"
#include "DSP/GravitonSignalGenerator.h"
#include "DSP/GravitonSpectrogram.h"
#include "DSP/PeakingEq.h"
#include "Effects/GravitonConvolutionReverb.h"
#include "Effects/GravitonDVR2Reverb.h"
#include "Effects/GravitonEffectsRack.h"
#include "Effects/GravitonHybridReverb.h"
#include "Effects/GravitonSCFChorus.h"
#include "GUI/FaderLayout.h"
#include "Mixer/ChannelState.h"
#include "Mixer/GravitonBusIntegration.h"
#include "Mixer/GravitonDuganAutomixer.h"
#include "Mixer/GravitonMasterBus.h"
#include "Mixer/GravitonMonitor.h"
#include "Mixer/GravitonSidechain.h"
#include "Mixer/GravitonTalkback.h"
#include "Mixer/MixMath.h"
#include "Recording/RecordingWorker.h"
#include "Routing/AudioBusOutputRouter.h"
#include "Routing/GravitonAuxRoutingSnapshot.h"
#include "Routing/GravitonMatrixPlus.h"
#include "Routing/GravitonMatrixRouting.h"
#include "Routing/GravitonPatchMatrix.h"
#include "Routing/GravitonSurround.h"
#include "Routing/ProductionBusMath.h"
#include "Routing/SafeBusGraphActivation.h"
#include "Routing/ScalableBusGraph.h"
#include "Utilities/GravitonAutomation.h"
#include "Utilities/GravitonCueList.h"
#include "Utilities/GravitonManchino.h"
#include "Utilities/GravitonShowArchive.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <utility>
#include <vector>

namespace nova {

class MixerEngine final : public juce::AudioIODeviceCallback {
public:
  struct EqBandState {
    // Graviton Mk 2 owns the filter coefficients and delay state in double
    // precision.
    nova::graviton::Biquad filter;
    double cachedFrequency = -1.0, cachedGainDb = 999.0, cachedQ = -1.0,
           cachedSampleRate = -1.0;
    int cachedEqModel = -1;
  };

  struct ChannelFilterState {
    nova::graviton::Biquad hpf1;
    nova::graviton::Biquad hpf2;
    nova::graviton::Biquad lpf1;
    nova::graviton::Biquad lpf2;
    double cachedHpfFreq = -1.0;
    int cachedHpfSlope = -1;
    double cachedLpfFreq = -1.0;
    int cachedLpfSlope = -1;
    double cachedSampleRate = -1.0;
  };

  MixerEngine() {
    formatManager.registerBasicFormats();
    monitorFifoL.assign(static_cast<size_t>(kMonitorFifoCapacity), 0.0f);
    monitorFifoR.assign(static_cast<size_t>(kMonitorFifoCapacity), 0.0f);
    monitorEq.prepare(48000.0);
    inputPhysicalToCallback.fill(-1);
    outputPhysicalToCallback.fill(-1);
    channelGainSmooth.fill(1.0);
    auxMasterGainSmooth.fill(1.0);
    matrixMasterGainSmooth.fill(1.0);
    for (auto &lim : auxIemLimiterEnabled)
      lim.store(false, std::memory_order_relaxed);
    for (auto &level : matrixMainLeftDb)
      level.store(-60.0f, std::memory_order_relaxed);
    for (auto &level : matrixMainRightDb)
      level.store(-60.0f, std::memory_order_relaxed);
    matrixMainLeftDb[0].store(-6.0206f, std::memory_order_relaxed);
    matrixMainRightDb[0].store(-6.0206f, std::memory_order_relaxed);
    for (auto &matrixSends : matrixAuxSendDb)
      for (auto &send : matrixSends)
        send.store(-60.0f, std::memory_order_relaxed);
    for (int a = 0; a < kVisibleAuxSends; ++a)
      matrixAuxSendDb[1][static_cast<size_t>(a)].store(
          0.0f, std::memory_order_relaxed);
    channelDelayBuffers.resize(kLogicalChannels);
    for (int i = 0; i < kLogicalChannels; ++i) {
      channelDelayBuffers[static_cast<size_t>(i)].assign(kMaxDelaySamples, 0.0);
    }
    channelDelayWritePos.fill(0);
    masterDelayBufL.assign(kMaxDelaySamples, 0.0);
    masterDelayBufR.assign(kMaxDelaySamples, 0.0);
    masterDelayWritePos = 0;
    monitorDelayBufL.assign(kMaxDelaySamples, 0.0);
    monitorDelayBufR.assign(kMaxDelaySamples, 0.0);
    monitorDelayWritePos = 0;
    for (int i = 0; i < kLogicalChannels; ++i)
      channels[static_cast<size_t>(i)].sourceInput.store(
          i < kInitialPhysicalInputs ? i : -1);
    syncAuxRoutingSnapshot();
    auxRoutingQueue.tryConsumeLatest(activeAuxRoutingSnapshot);
    syncMatrixRoutingSnapshot();
    matrixRoutingQueue.tryConsumeLatest(activeMatrixRoutingSnapshot);
    oscServer.onMessageReceived = [this](const nova::graviton::OscMessage &msg,
                                         const juce::String &clientIp,
                                         int clientPort) {
      handleOscMessage(msg, clientIp, clientPort);
    };
  }

  bool loadPlaybackFile(const juce::File &file) {
    stopPlayback();
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 ||
        reader->numChannels == 0)
      return false;
    auto newSource =
        std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    playbackTransport.setSource(newSource.get(), 0, nullptr,
                                newSource->getAudioFormatReader()->sampleRate,
                                2);
    playbackSource = std::move(newSource);
    playbackLoaded.store(true, std::memory_order_relaxed);
    return true;
  }

  void playPlayback() {
    if (playbackLoaded.load(std::memory_order_relaxed))
      playbackTransport.start();
  }

  void stopPlayback() {
    playbackTransport.stop();
    playbackTransport.setPosition(0.0);
  }

  std::array<ChannelState, kLogicalChannels> channels;
  std::array<std::array<EqBandState, 4>, kLogicalChannels> eqState{};
  std::array<ChannelFilterState, kLogicalChannels> filterState{};
  std::array<nova::graviton::VariableAllPass, kLogicalChannels>
      allPassFilters{};
  nova::graviton::GraphicEQ mainGraphicEQ;
  std::atomic<bool> graphicEqEnabled{false};
  nova::graviton::EffectsRack effectsRack;
  nova::graviton::ChannelAIAnalyzer channelAI;
  nova::graviton::ManchinoSelection manchinoSelection;
  nova::graviton::DuganAutomixer duganAutomixer;
  std::array<nova::graviton::DynamicEqEngine, kLogicalChannels> dynamicEqEngine;
  std::array<nova::graviton::SidechainKeyFilter, kLogicalChannels>
      sidechainKeyFilter;
  // Phase 84: AES50 SuperMAC & Dante Stage Box 64x64 Patch Matrix (Chapters 27
  // & 31)
  nova::graviton::StageBoxPatchMatrix stageBoxPatchMatrix;
  std::array<juce::String, kLogicalChannels> channelNames{};
  mutable juce::CriticalSection channelNameLock;

  juce::String getChannelName(int channelIndex) const {
    if (channelIndex < 0 || channelIndex >= kLogicalChannels)
      return {};
    const juce::ScopedLock lock(channelNameLock);
    return channelNames[static_cast<size_t>(channelIndex)];
  }

  void setChannelName(int channelIndex, const juce::String &value) {
    if (channelIndex < 0 || channelIndex >= kLogicalChannels)
      return;
    const juce::ScopedLock lock(channelNameLock);
    channelNames[static_cast<size_t>(channelIndex)] = value;
  }
  std::array<double, nova::graviton::StageBoxPatchMatrix::kTotalOutputPorts>
      stageBoxOutputSamples{};
  // Phase 85: Multichannel Surround & Immersive Panning 5.1 / 7.1 / LCR &
  // Spatial Object Renderer (MIDAS Chapter 30)
  std::array<std::atomic<float>, 12> surroundMasterPeaks{};
  std::atomic<bool> surroundBinauralMonitorEnabled{false};
  nova::graviton::BinauralSpatializer binauralPreviewSpatializer;
  // Phase 86: Comprehensive Talkback & Comm Bus Intercom Matrix (MIDAS Chapter
  // 32)
  nova::graviton::TalkbackEngine talkbackEngine;
  // Phase 87: OSC & Wireless iPad Remote Protocol Server (MIDAS Chapter 27)
  nova::graviton::OscServer oscServer;
  // Phase 88: Matrix Plus & Speaker Delay Alignment Network (MIDAS Chapter 29)
  nova::graviton::MatrixPlusEngine matrixPlusEngine;
  // Phase 96: Real-Time Dual-FFT RTA / Transfer Function Audio Analyzer (MIDAS
  // Chapter 28)
  nova::graviton::DualFFTAnalyzer rtaAnalyzer;
  // Phase 97: Multichannel DAW USB 64x64 & ASIO Direct Recording Streamer
  // (MIDAS Chapter 26)
  nova::graviton::DAWStreamingInterface dawStreamer;
  nova::graviton::ASIOHardwareStreamBridge hardwareStreamBridge{dawStreamer};
  std::vector<std::string> oscLogMessages;
  juce::CriticalSection oscLogLock;

  void logOsc(const std::string &line) {
    const juce::ScopedLock sl(oscLogLock);
    if (oscLogMessages.size() > 50)
      oscLogMessages.erase(oscLogMessages.begin());
    oscLogMessages.push_back(line);
  }

  void handleOscMessage(const nova::graviton::OscMessage &msg,
                        const juce::String &clientIp, int clientPort) {
    logOsc("RX: " + msg.address + " (" + msg.typeTags + ")");

    // Reject non-finite numeric input before clamping or publishing DSP controls.
    // std::clamp does not sanitize NaN because all comparisons with NaN are false.
    for (const float value : msg.floatArgs) {
      if (!std::isfinite(value)) {
        logOsc("DROP: non-finite OSC float");
        return;
      }
    }

    // /ch/{id}/mix/fader [float] or /ch/{id}/fader [float]
    if (msg.address.rfind("/ch/", 0) == 0) {
      int chNum = 0;
      char subAddr[64] = {0};
      if (sscanf_s(msg.address.c_str(), "/ch/%d/%63s", &chNum, subAddr,
                   static_cast<unsigned int>(sizeof(subAddr))) == 2 &&
          chNum >= 1 && chNum <= kLogicalChannels) {
        const size_t cIdx = static_cast<size_t>(chNum - 1);
        auto &ch = channels[cIdx];
        std::string sub(subAddr);

        if ((sub == "mix/fader" || sub == "fader") && !msg.floatArgs.empty()) {
          float fVal = msg.floatArgs[0];
          float dbVal = (fVal >= 0.0f && fVal <= 1.0f)
                            ? (-60.0f + fVal * 70.0f)
                            : std::clamp(fVal, -60.0f, 10.0f);
          ch.gainDb.store(dbVal, std::memory_order_relaxed);
        } else if ((sub == "mix/pan" || sub == "pan") &&
                   !msg.floatArgs.empty()) {
          ch.pan.store(std::clamp(msg.floatArgs[0], -1.0f, 1.0f),
                       std::memory_order_relaxed);
        } else if (sub == "mix/on") {
          bool mVal = false;
          if (!msg.intArgs.empty())
            mVal = (msg.intArgs[0] == 0);
          else if (!msg.floatArgs.empty())
            mVal = (msg.floatArgs[0] < 0.5f);
          ch.muted.store(mVal, std::memory_order_relaxed);
        } else if (sub == "mix/mute" || sub == "mute") {
          bool mVal = true;
          if (!msg.intArgs.empty())
            mVal = (msg.intArgs[0] != 0);
          else if (!msg.floatArgs.empty())
            mVal = (msg.floatArgs[0] >= 0.5f);
          ch.muted.store(mVal, std::memory_order_relaxed);
        } else if (sub == "mix/solo" || sub == "solo") {
          bool sVal = false;
          if (!msg.intArgs.empty())
            sVal = (msg.intArgs[0] != 0);
          else if (!msg.floatArgs.empty())
            sVal = (msg.floatArgs[0] >= 0.5f);
          ch.solo.store(sVal, std::memory_order_relaxed);
        } else if ((sub == "spatial/xyz" || sub == "xyz") &&
                   msg.floatArgs.size() >= 3) {
          ch.surroundX.store(std::clamp(msg.floatArgs[0], -1.0f, 1.0f),
                             std::memory_order_relaxed);
          ch.surroundY.store(std::clamp(msg.floatArgs[1], -1.0f, 1.0f),
                             std::memory_order_relaxed);
          ch.surroundZ.store(std::clamp(msg.floatArgs[2], 0.0f, 1.0f),
                             std::memory_order_relaxed);
        } else if ((sub == "config/name" || sub == "name") &&
                   !msg.stringArgs.empty()) {
          setChannelName(static_cast<int>(cIdx), juce::String(msg.stringArgs[0]));
        }
      }
    } else if (msg.address == "/main/fader" && !msg.floatArgs.empty()) {
      float fVal = msg.floatArgs[0];
      float dbVal = (fVal >= 0.0f && fVal <= 1.0f)
                        ? (-60.0f + fVal * 70.0f)
                        : std::clamp(fVal, -60.0f, 10.0f);
      masterDb.store(dbVal, std::memory_order_relaxed);
    } else if (msg.address == "/talkback/on") {
      bool tOn = true;
      if (!msg.intArgs.empty())
        tOn = (msg.intArgs[0] != 0);
      else if (!msg.floatArgs.empty())
        tOn = (msg.floatArgs[0] >= 0.5f);
      talkbackEngine.isTalking.store(tOn, std::memory_order_relaxed);
    } else if (msg.address == "/xinfo" || msg.address == "/ping") {
      nova::graviton::OscMessage reply;
      reply.address = "/info";
      reply.typeTags = "sssii";
      reply.stringArgs.push_back("NOVA HD DIGITAL MIXING SYSTEM");
      reply.stringArgs.push_back("Graviton Mk 2 DSP Audio Engine");
      reply.stringArgs.push_back("v1.7.0");
      reply.intArgs.push_back(kLogicalChannels);
      reply.intArgs.push_back(96000);
      oscServer.sendMessage(reply, clientIp, clientPort);
      logOsc("TX: /info to " + clientIp.toStdString());
    }
  }
  std::atomic<bool> sipEnabled{false};
  RecordingWorker recorder;
  juce::AudioFormatManager formatManager;
  juce::AudioTransportSource playbackTransport;
  std::unique_ptr<juce::AudioFormatReaderSource> playbackSource;
  juce::AudioBuffer<float> playbackBlock{2, 8192};
  juce::AudioBuffer<float> mainRecordBuffer{2, 8192};
  juce::AudioBuffer<float> rawMultitrackRecordBuffer{64, 8192};
  std::array<int, 64> inputPhysicalToCallback{};
  std::array<int, 64> outputPhysicalToCallback{};
  std::atomic<bool> playbackLoaded{false};
  std::atomic<bool> midiUiDirty{false};
  std::atomic<bool> midiLearnedDirty{false};
  std::atomic<float> masterDb{0.0f};
  // Audio-thread-only gain smoothing state (linear gain); initialized to unity.
  std::array<double, kLogicalChannels> channelGainSmooth{};
  double masterGainSmooth = 1.0;
  double monitorGainSmoothA = 1.0;
  double monitorGainSmoothB = 1.0;
  std::array<double, kAuxBusCapacity> auxMasterGainSmooth{};
  std::array<double, kMatrixBusCapacity> matrixMasterGainSmooth{};
  std::array<std::atomic<float>, 24> vcaMasterDb{};
  std::array<std::atomic<bool>, 24> vcaMuted{};
  // Names are UI/session state only; the audio callback never reads these
  // strings.
  std::array<juce::String, 24> vcaNames{};
  std::atomic<int> activeSpillVca{
      0}; // 0 = normal banking, 1..24 = active VCA spill

  // Phase 98: POPulation Groups Navigation & Multi-Channel Spill (Chapter 11)
  std::array<juce::String, 24> popNames{
      "POP 1: Drums",           "POP 2: Bass/Gtr",     "POP 3: Keys/Synth",
      "POP 4: Horns/Brass",     "POP 5: Lead Vox",     "POP 6: Backing Vox",
      "POP 7: Strings",         "POP 8: Percussion",   "POP 9: FX Returns",
      "POP 10: Playback/DAW",   "POP 11: Guitars Ac",  "POP 12: Guitars El",
      "POP 13: Synths",         "POP 14: Choir",       "POP 15: Guests",
      "POP 16: Talkback/Comms", "POP 17: Band Wedges", "POP 18: In-Ear IEM",
      "POP 19: Frontfill",      "POP 20: Outfill",     "POP 21: Delay Towers",
      "POP 22: Broadcast L/R",  "POP 23: VIP Feed",    "POP 24: Spares"};
  std::atomic<int> activeSpillPop{
      0}; // 0 = normal banking, 1..24 = active POP spill

  // Phase 99: Scene Automation Cue List & MSC Manager (Chapter 34)
  nova::graviton::CueListManager cueManager;
  std::array<std::atomic<bool>, 8>
      muteGroupMasters{}; // 8 global mute group masters
  // Independent masters for the four exposed AUX buses and two monitor matrix
  // buses.
  std::array<std::atomic<float>, kAuxBusCapacity> auxMasterDb{};
  std::array<std::atomic<bool>, kAuxBusCapacity> auxMuted{};
  std::array<std::atomic<bool>, kAuxBusCapacity> auxIemLimiterEnabled{};
  std::atomic<float> auxIemCeilingDb{-6.0f};
  std::array<std::atomic<float>, kMatrixBusCapacity> matrixMasterDb{};
  std::array<std::atomic<bool>, kMatrixBusCapacity> matrixMuted{};
  std::array<std::atomic<float>, kMatrixBusCapacity> matrixMainLeftDb{};
  std::array<std::atomic<float>, kMatrixBusCapacity> matrixMainRightDb{};
  std::array<std::atomic<bool>, kMatrixBusCapacity> matrixMainMuted{};
  std::array<std::array<std::atomic<float>, kAuxBusCapacity>,
             kMatrixBusCapacity>
      matrixAuxSendDb{};
  std::array<std::array<std::atomic<bool>, kAuxBusCapacity>, kMatrixBusCapacity>
      matrixAuxSendMuted{};
  // Phase 71: Channel Stereo Linking & Pairing (72 odd/even pairs across 144
  // channels) stereoLink[i] links channel (2*i) and (2*i + 1), e.g., pair 0
  // links CH 1 & CH 2.
  std::array<std::atomic<bool>, kLogicalChannels / 2> stereoLink{};
  // Phase 70: Integrated Signal Generator / Alignment Oscillator
  std::atomic<bool> oscEnabled{false};
  std::atomic<int> oscType{0};              // 0 = Sine, 1 = Pink, 2 = White
  std::atomic<float> oscFrequency{1000.0f}; // 20 - 20000 Hz
  std::atomic<float> oscLevelDb{-18.0f};    // dBFS (-60.0 to 0.0)
  std::atomic<bool> oscRouteMain{false};
  std::atomic<bool> oscRouteMonitor{false};
  std::array<std::atomic<bool>, kAuxBusCapacity> oscRouteAux{};
  std::array<std::atomic<bool>, kMatrixBusCapacity> oscRouteMatrix{};
  nova::graviton::SignalGenerator signalGenerator;
  double oscGainSmooth = 0.0;
  // Phase 72: Input Delay Line (max 500 ms @ 96 kHz = 48000 samples)
  static constexpr int kMaxDelaySamples = 48000;
  std::vector<std::vector<double>> channelDelayBuffers;
  std::array<int, kLogicalChannels> channelDelayWritePos{};
  // Phase 75: Dual Solo Bus Architecture (Solo A & Solo B) & Monitor Section
  std::atomic<float> monitorLevelA{0.0f}; // dB (-60 to +10 dB)
  std::atomic<float> monitorLevelB{0.0f}; // dB (-60 to +10 dB)
  std::atomic<bool> monitorMuteA{false};
  std::atomic<bool> monitorMuteB{false};
  std::atomic<bool> monitorDimA{false}; // -20 dB dim
  std::atomic<bool> monitorDimB{false}; // -20 dB dim
  std::atomic<bool> talkbackActive{false};
  std::atomic<float> talkbackDuckerDb{-20.0f}; // Attenuation in dB
  std::atomic<float> monitorPeakL_A{0.0f}, monitorPeakR_A{0.0f};
  std::atomic<float> monitorPeakL_B{0.0f}, monitorPeakR_B{0.0f};

  // Master & Monitor Time Alignment Delay Lines (0 - 500 ms @ 96 kHz)
  std::atomic<float> masterDelayMs{0.0f};  // Master output delay in ms
  std::atomic<float> monitorDelayMs{0.0f}; // Monitor output delay in ms
  std::vector<double> masterDelayBufL, masterDelayBufR;
  int masterDelayWritePos = 0;
  std::vector<double> monitorDelayBufL, monitorDelayBufR;
  int monitorDelayWritePos = 0;

  // Phase 90: Master & Monitor Dual Output System (WASAPI 1 / Master & WASAPI 2
  // / Monitor)
  nova::graviton::MonitorEq monitorEq;
  std::atomic<bool> monitorEqBypass{false};
  std::atomic<float> fxToMonitorDb{
      -90.0f}; // Independent FX send/return to Monitor bus (-90 to +10 dB)
  std::atomic<bool> dualOutput4ChMode{
      false}; // When true, physical outputs 2 & 3 carry Monitor L/R

  // Lock-free FIFO for Secondary Audio Streamer (WASAPI 2 / Bluetooth)
  // Sized to 2 seconds @ 96 kHz (4 seconds @ 48 kHz) per AudioHQ EngineTunables::BufferSeconds = 2
  static constexpr int kMonitorFifoCapacity = 192000;
  juce::AbstractFifo monitorFifo{kMonitorFifoCapacity};
  std::vector<float> monitorFifoL;
  std::vector<float> monitorFifoR;
  std::atomic<bool> monitorConsumerActive{false};
  std::atomic<bool> monitorFifoResetRequested{false};

  // Monitor FIFO Real-Time Queue Depth & Overrun/Underrun Telemetry
  std::atomic<int> monitorFifoQueueDepth{0};
  std::atomic<float> monitorFifoLatencyMs{0.0f};
  std::atomic<uint32_t> monitorFifoOverruns{0};
  std::atomic<uint32_t> monitorFifoUnderruns{0};

  void setMonitorConsumerActive(bool active) noexcept {
    monitorConsumerActive.store(active, std::memory_order_release);
  }
  bool isMonitorConsumerActive() const noexcept {
    return monitorConsumerActive.load(std::memory_order_acquire);
  }

  int getMonitorFifoQueueDepth() const noexcept {
    return monitorFifoQueueDepth.load(std::memory_order_relaxed);
  }
  int getMonitorFifoReady() const noexcept {
    return monitorFifo.getNumReady();
  }
  float getMonitorFifoLatencyMs() const noexcept {
    return monitorFifoLatencyMs.load(std::memory_order_relaxed);
  }
  uint32_t getMonitorFifoOverruns() const noexcept {
    return monitorFifoOverruns.load(std::memory_order_relaxed);
  }
  uint32_t getMonitorFifoUnderruns() const noexcept {
    return monitorFifoUnderruns.load(std::memory_order_relaxed);
  }
  void resetMonitorFifoStats() noexcept {
    if (!running.load(std::memory_order_relaxed) &&
        !monitorConsumerActive.load(std::memory_order_relaxed)) {
      monitorFifo.reset();
    } else {
      monitorFifoResetRequested.store(true, std::memory_order_release);
    }
    monitorFifoOverruns.store(0, std::memory_order_relaxed);
    monitorFifoUnderruns.store(0, std::memory_order_relaxed);
    monitorFifoQueueDepth.store(0, std::memory_order_relaxed);
    monitorFifoLatencyMs.store(0.0f, std::memory_order_relaxed);
  }
  bool consumeMonitorFifoResetRequest() noexcept {
    return monitorFifoResetRequested.exchange(false, std::memory_order_acq_rel);
  }

  double getSampleRate() const noexcept {
    return sampleRate.load(std::memory_order_relaxed);
  }

  int readMonitorSamples(float *destL, float *destR,
                         int numSamplesRequested) noexcept {
    if (destL == nullptr || destR == nullptr || numSamplesRequested <= 0)
      return 0;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    monitorFifo.prepareToRead(numSamplesRequested, start1, size1, start2,
                              size2);
    const int totalRead = size1 + size2;
    if (totalRead < numSamplesRequested) {
      monitorFifoUnderruns.fetch_add(1, std::memory_order_relaxed);
    }
    if (size1 > 0) {
      for (int i = 0; i < size1; ++i) {
        destL[i] = monitorFifoL[static_cast<size_t>(start1 + i)];
        destR[i] = monitorFifoR[static_cast<size_t>(start1 + i)];
      }
    }
    if (size2 > 0) {
      for (int i = 0; i < size2; ++i) {
        destL[size1 + i] = monitorFifoL[static_cast<size_t>(start2 + i)];
        destR[size1 + i] = monitorFifoR[static_cast<size_t>(start2 + i)];
      }
    }
    monitorFifo.finishedRead(totalRead);
    const int ready = monitorFifo.getNumReady();
    monitorFifoQueueDepth.store(ready, std::memory_order_relaxed);
    const double curSr = sampleRate.load(std::memory_order_relaxed);
    if (curSr > 1000.0) {
      monitorFifoLatencyMs.store(static_cast<float>((ready / curSr) * 1000.0),
                                 std::memory_order_relaxed);
    }
    return totalRead;
  }

  int peekMonitorSamples(float *destL, float *destR,
                         int numSamplesRequested) noexcept {
    if (destL == nullptr || destR == nullptr || numSamplesRequested <= 0)
      return 0;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    monitorFifo.prepareToRead(numSamplesRequested, start1, size1, start2,
                              size2);
    const int totalAvailable = size1 + size2;
    if (size1 > 0) {
      for (int i = 0; i < size1; ++i) {
        destL[i] = monitorFifoL[static_cast<size_t>(start1 + i)];
        destR[i] = monitorFifoR[static_cast<size_t>(start1 + i)];
      }
    }
    if (size2 > 0) {
      for (int i = 0; i < size2; ++i) {
        destL[size1 + i] = monitorFifoL[static_cast<size_t>(start2 + i)];
        destR[size1 + i] = monitorFifoR[static_cast<size_t>(start2 + i)];
      }
    }
    return totalAvailable;
  }

  void advanceMonitorRead(int numSamplesConsumed) noexcept {
    if (numSamplesConsumed <= 0)
      return;
    monitorFifo.finishedRead(numSamplesConsumed);
    const int ready = monitorFifo.getNumReady();
    monitorFifoQueueDepth.store(ready, std::memory_order_relaxed);
    const double curSr = sampleRate.load(std::memory_order_relaxed);
    if (curSr > 1000.0) {
      monitorFifoLatencyMs.store(static_cast<float>((ready / curSr) * 1000.0),
                                 std::memory_order_relaxed);
    }
  }

  void recordMonitorUnderrun() noexcept {
    monitorFifoUnderruns.fetch_add(1, std::memory_order_relaxed);
  }

  int discardMonitorSamples(int numSamplesToDiscard) noexcept {
    if (numSamplesToDiscard <= 0)
      return 0;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    monitorFifo.prepareToRead(numSamplesToDiscard, start1, size1, start2,
                              size2);
    const int totalRead = size1 + size2;
    monitorFifo.finishedRead(totalRead);
    const int ready = monitorFifo.getNumReady();
    monitorFifoQueueDepth.store(ready, std::memory_order_relaxed);
    const double curSr = sampleRate.load(std::memory_order_relaxed);
    if (curSr > 1000.0) {
      monitorFifoLatencyMs.store(static_cast<float>((ready / curSr) * 1000.0),
                                 std::memory_order_relaxed);
    }
    return totalRead;
  }
  std::atomic<float> masterPeakL{0.0f}, masterPeakR{0.0f};
  std::atomic<bool> mainClipL{false}, mainClipR{false};
  // Phase 66: Final Master Bus Chain state (true-peak limiter, master EQ,
  // dither).
  std::atomic<bool> masterBusLimiterEnabled{false};
  std::atomic<double> masterBusLimiterCeilingDb{-0.1};
  std::atomic<bool> masterBusEqEnabled{false};
  std::atomic<double> masterBusEqFrequencyHz{100.0};
  std::atomic<double> masterBusEqGainDb{0.0};
  std::atomic<double> masterBusEqQ{1.0};
  std::atomic<double> masterBusEqLowGainDb{0.0};
  std::atomic<double> masterBusEqMidGainDb{0.0};
  std::atomic<double> masterBusEqHighGainDb{0.0};
  std::atomic<bool> masterBusDitherEnabled{false};
  nova::graviton::MasterBusChain masterBusChain;
  std::array<std::atomic<bool>, kAuxBusCapacity> auxClip{};
  std::array<std::atomic<bool>, kMatrixBusCapacity> matrixClip{};
  std::atomic<bool> running{false};
  std::atomic<bool> deviceFailure{false};
  std::atomic<uint64_t> deviceStopCount{0};
  std::atomic<uint64_t> deviceStartCount{0};
  std::atomic<uint64_t> deviceErrorCount{0};
  std::atomic<uint64_t> callbackCount{0};
  std::atomic<uint64_t> callbackFrames{0};
  std::atomic<int> largestCallbackBlock{0};
  // Updated by the device error callback and displayed in diagnostics.
  juce::String lastDeviceError;
  std::atomic<double> sampleRate{0.0};
  std::atomic<int> blockSize{0};
  std::atomic<int> inputCount{0}, outputCount{0};
  std::array<std::atomic<float>, kAuxBusCapacity> auxPeak{};
  std::array<std::atomic<float>, kMatrixBusCapacity> matrixPeak{};

  // Heritage Virtual Cable (Fase 1: Lock-Free VirtualTransport + VBAN UDP)
  nova::graviton::VirtualCableTransport virtualTransport;
  std::array<std::array<float, 4096>,
             nova::graviton::VirtualCableTransport::kNumVirtualCables>
      virtualReturnBufL{};
  std::array<std::array<float, 4096>,
             nova::graviton::VirtualCableTransport::kNumVirtualCables>
      virtualReturnBufR{};
  std::array<std::array<float, 4096>,
             nova::graviton::VirtualCableTransport::kNumVirtualCables>
      virtualSendBufL{};
  std::array<std::array<float, 4096>,
             nova::graviton::VirtualCableTransport::kNumVirtualCables>
      virtualSendBufR{};
  std::array<int, kLogicalChannels> channelVirtualCableSource{};

  // Graviton Mk 2: Real-time safe AUX routing snapshot publishing and
  // consumption.
  using AuxSnapshot =
      nova::graviton::MixerAuxRoutingSnapshot<kLogicalChannels, kAuxBusCapacity,
                                              kAuxBusCapacity>;
  using AuxSnapshotQueue =
      nova::graviton::AuxRoutingSnapshotQueue<AuxSnapshot, 4>;
  using AuxSnapshotPublisher =
      nova::graviton::AuxRoutingSnapshotPublisher<AuxSnapshot, 4>;

  AuxSnapshotQueue auxRoutingQueue;
  AuxSnapshot activeAuxRoutingSnapshot{};
  AuxSnapshotPublisher auxRoutingPublisher{auxRoutingQueue};
  std::atomic<bool> auxRoutingDirty{false};

  // Phase 65: Safe AUX bus graph runtime (MIDAS HD96-24 Chapter 15).
  // The control thread validates and publishes a fixed-capacity bus graph plan;
  // the audio thread activates the newest ready plan at the block boundary and
  // executes AUX sends from the immutable active plan (lock-free,
  // allocation-free).
  nova::SafeBusGraphActivation safeBusGraphActivation;

  void syncAuxRoutingSnapshot() noexcept {
    auto &staged = auxRoutingPublisher.staged();

    for (int c = 0; c < kLogicalChannels; ++c) {
      auto &chSnap = staged.channels[static_cast<size_t>(c)];
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        const auto index = static_cast<size_t>(a);
        const float sendDb =
            channels[static_cast<size_t>(c)].auxSendDb[index].load(
                std::memory_order_relaxed);
        const double linearGain =
            (!std::isfinite(sendDb) || sendDb <= -59.9f)
                ? 0.0
                : nova::graviton::dbToGain(
                      std::clamp(static_cast<double>(sendDb), -60.0, 10.0));
        chSnap.sendGains[index] = linearGain;
        chSnap.sendPreFader[index] =
            channels[static_cast<size_t>(c)].auxPreFader[index].load(
                std::memory_order_relaxed);
      }
      chSnap.rebuildRoutes();
    }
    auxRoutingPublisher.publishStaged();

    // Phase 65: validate the staged AUX routing state into the safe bus graph
    // plan that the realtime audio thread will execute from.
    syncSafeBusGraphAux();
  }

  void syncSafeBusGraphAux() noexcept {
    const auto &staged = auxRoutingPublisher.staged();
    nova::ScalableBusGraphConfig config;
    const int graphChannels =
        std::min(kLogicalChannels, nova::kBusGraphChannels);
    const int graphAuxes = std::min(kAuxBusCapacity, nova::kBusGraphAuxCount);
    for (int c = 0; c < graphChannels; ++c) {
      const auto &chSnap = staged.channels[static_cast<std::size_t>(c)];
      for (int a = 0; a < graphAuxes; ++a) {
        const auto index = static_cast<std::size_t>(a);
        config.sendGain[static_cast<std::size_t>(c)][index] =
            static_cast<float>(chSnap.sendGains[index]);
        config.sendPreFader[static_cast<std::size_t>(c)][index] =
            chSnap.sendPreFader[index];
      }
    }
    for (int a = 0; a < graphAuxes; ++a) {
      const auto index = static_cast<std::size_t>(a);
      config.auxMasterGain[index] = static_cast<float>(nova::graviton::dbToGain(
          std::clamp(static_cast<double>(
                         auxMasterDb[index].load(std::memory_order_relaxed)),
                     -60.0, 0.0)));
      config.auxMuted[index] = auxMuted[index].load(std::memory_order_relaxed);
    }
    (void)safeBusGraphActivation.publish(config);
  }

  void retryPendingAuxRouting() noexcept { auxRoutingPublisher.retryPending(); }

  // Graviton Mk 2: Real-time safe Matrix routing snapshot publishing and
  // consumption.
  using MatrixSnapshot =
      nova::graviton::MatrixRoutingSnapshot<kAuxBusCapacity, kMatrixBusCapacity,
                                            kAuxBusCapacity>;
  using MatrixSnapshotQueue =
      nova::graviton::MatrixRoutingSnapshotQueue<MatrixSnapshot, 4>;
  using MatrixSnapshotPublisher =
      nova::graviton::MatrixRoutingSnapshotPublisher<MatrixSnapshot, 4>;

  MatrixSnapshotQueue matrixRoutingQueue;
  MatrixSnapshot activeMatrixRoutingSnapshot{};
  MatrixSnapshotPublisher matrixRoutingPublisher{matrixRoutingQueue};
  std::atomic<bool> matrixRoutingDirty{false};

  void syncMatrixRoutingSnapshot() noexcept {
    auto &staged = matrixRoutingPublisher.staged();
    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      auto &mat = staged.matrices[static_cast<size_t>(m)];
      mat.masterGain = 1.0;
      mat.masterMuted =
          matrixMuted[static_cast<size_t>(m)].load(std::memory_order_relaxed);

      const float mlDb = matrixMainLeftDb[static_cast<size_t>(m)].load(
          std::memory_order_relaxed);
      const float mrDb = matrixMainRightDb[static_cast<size_t>(m)].load(
          std::memory_order_relaxed);
      mat.mainLeftGain = (!std::isfinite(mlDb) || mlDb <= -59.9f)
                             ? 0.0
                             : nova::graviton::dbToGain(std::clamp(
                                   static_cast<double>(mlDb), -60.0, 10.0));
      mat.mainRightGain = (!std::isfinite(mrDb) || mrDb <= -59.9f)
                              ? 0.0
                              : nova::graviton::dbToGain(std::clamp(
                                    static_cast<double>(mrDb), -60.0, 10.0));
      mat.mainMuted = matrixMainMuted[static_cast<size_t>(m)].load(
          std::memory_order_relaxed);

      for (int a = 0; a < kAuxBusCapacity; ++a) {
        const float aDb =
            matrixAuxSendDb[static_cast<size_t>(m)][static_cast<size_t>(a)]
                .load(std::memory_order_relaxed);
        mat.auxSendGains[static_cast<size_t>(a)] =
            (!std::isfinite(aDb) || aDb <= -59.9f)
                ? 0.0
                : nova::graviton::dbToGain(
                      std::clamp(static_cast<double>(aDb), -60.0, 10.0));
        mat.auxSendMuted[static_cast<size_t>(a)] =
            matrixAuxSendMuted[static_cast<size_t>(m)][static_cast<size_t>(a)]
                .load(std::memory_order_relaxed);
      }
      mat.rebuildActiveTaps();
    }
    matrixRoutingPublisher.publishStaged();
  }

  void setStereoLink(int pairIndex, bool linked) noexcept {
    if (pairIndex < 0 || pairIndex >= (kLogicalChannels / 2))
      return;
    stereoLink[static_cast<size_t>(pairIndex)].store(linked,
                                                     std::memory_order_relaxed);
    const int ch1 = pairIndex * 2;
    const int ch2 = ch1 + 1;
    channels[static_cast<size_t>(ch1)].stereoLinked.store(
        linked, std::memory_order_relaxed);
    channels[static_cast<size_t>(ch2)].stereoLinked.store(
        linked, std::memory_order_relaxed);

    if (linked) {
      // When linking, copy processing & fader from odd channel to even channel,
      // and auto-set pans to hard Left (-1.0) and hard Right (+1.0).
      auto &src = channels[static_cast<size_t>(ch1)];
      auto &dst = channels[static_cast<size_t>(ch2)];

      dst.gainDb.store(src.gainDb.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
      dst.muted.store(src.muted.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      dst.solo.store(src.solo.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
      dst.highPassEnabled.store(
          src.highPassEnabled.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.highPassFreqHz.store(
          src.highPassFreqHz.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.highPassSlope.store(src.highPassSlope.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
      dst.lowPassEnabled.store(
          src.lowPassEnabled.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.lowPassFreqHz.store(src.lowPassFreqHz.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
      dst.lowPassSlope.store(src.lowPassSlope.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      dst.compressorEnabled.store(
          src.compressorEnabled.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.compressorMode.store(
          src.compressorMode.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.gateMode.store(src.gateMode.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
      dst.gateEnabled.store(src.gateEnabled.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
      dst.vcaGroup.store(src.vcaGroup.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
      dst.muteGroupMask.store(src.muteGroupMask.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
      dst.eqEnabled.store(src.eqEnabled.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
      dst.eqModel.store(src.eqModel.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);

      for (int b = 0; b < 4; ++b) {
        const auto bIdx = static_cast<size_t>(b);
        dst.eqFrequency[bIdx].store(
            src.eqFrequency[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.eqGainDb[bIdx].store(
            src.eqGainDb[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.eqQ[bIdx].store(src.eqQ[bIdx].load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
        dst.dynEqEnabled[bIdx].store(
            src.dynEqEnabled[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqMode[bIdx].store(
            src.dynEqMode[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqThresholdDb[bIdx].store(
            src.dynEqThresholdDb[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqRangeDb[bIdx].store(
            src.dynEqRangeDb[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqRatio[bIdx].store(
            src.dynEqRatio[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqKneeDb[bIdx].store(
            src.dynEqKneeDb[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqAttackMs[bIdx].store(
            src.dynEqAttackMs[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.dynEqReleaseMs[bIdx].store(
            src.dynEqReleaseMs[bIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
      }
      dst.gateThresholdDb.store(
          src.gateThresholdDb.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.gateRatio.store(src.gateRatio.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
      dst.gateAttackMs.store(src.gateAttackMs.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      dst.gateReleaseMs.store(src.gateReleaseMs.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
      dst.compressorThresholdDb.store(
          src.compressorThresholdDb.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.compressorRatio.store(
          src.compressorRatio.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.compressorAttackMs.store(
          src.compressorAttackMs.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.compressorReleaseMs.store(
          src.compressorReleaseMs.load(std::memory_order_relaxed),
          std::memory_order_relaxed);

      for (int a = 0; a < kVisibleAuxSends; ++a) {
        const auto aIdx = static_cast<size_t>(a);
        dst.auxSendDb[aIdx].store(
            src.auxSendDb[aIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        dst.auxPreFader[aIdx].store(
            src.auxPreFader[aIdx].load(std::memory_order_relaxed),
            std::memory_order_relaxed);
      }

      src.pan.store(-1.0f, std::memory_order_relaxed);
      dst.pan.store(1.0f, std::memory_order_relaxed);

      dst.polarityInverted.store(
          src.polarityInverted.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.inputDelayMs.store(src.inputDelayMs.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      dst.postEqDynamics.store(
          src.postEqDynamics.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.muteSafe.store(src.muteSafe.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
      dst.soloSafe.store(src.soloSafe.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
      dst.soloBus.store(src.soloBus.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
      dst.soloPfl.store(src.soloPfl.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
      dst.allPassEnabled.store(
          src.allPassEnabled.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.allPassDegrees.store(
          src.allPassDegrees.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      dst.allPassRange.store(src.allPassRange.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      dst.allPassFreqMode.store(
          src.allPassFreqMode.load(std::memory_order_relaxed),
          std::memory_order_relaxed);

      auxRoutingDirty.store(true, std::memory_order_relaxed);
      syncAuxRoutingSnapshot();
    }
  }

  struct ChannelAISnapshot {
    bool valid = false;
    float inputGainDb = 0.0f;
    float gainDb = 0.0f;
    bool highPassEnabled = false;
    float highPassFreqHz = 80.0f;
    int highPassSlope = 0;
    bool eqEnabled = true;
    std::array<float, 4> eqFrequency{100.0f, 500.0f, 2000.0f, 8000.0f};
    std::array<float, 4> eqGainDb{0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 4> eqQ{1.0f, 1.0f, 1.0f, 1.0f};
    bool gateEnabled = false;
    float gateThresholdDb = -45.0f;
    float gateRatio = 4.0f;
    float gateAttackMs = 2.0f;
    float gateReleaseMs = 120.0f;
    bool compressorEnabled = false;
    int compressorMode = 1;
    float compressorThresholdDb = -18.0f;
    float compressorRatio = 4.0f;
    float compressorAttackMs = 10.0f;
    float compressorReleaseMs = 120.0f;
    std::array<float, 6> auxSendDb{-60.0f, -60.0f, -60.0f, -60.0f, -60.0f, -60.0f};
  };

  ChannelAISnapshot captureChannelAISnapshot(int chIdx) const noexcept {
    ChannelAISnapshot snap;
    if (chIdx < 0 || chIdx >= kLogicalChannels)
      return snap;
    const auto &ch = channels[static_cast<size_t>(chIdx)];
    snap.valid = true;
    snap.inputGainDb = ch.inputGainDb.load(std::memory_order_relaxed);
    snap.gainDb = ch.gainDb.load(std::memory_order_relaxed);
    snap.highPassEnabled = ch.highPassEnabled.load(std::memory_order_relaxed);
    snap.highPassFreqHz = ch.highPassFreqHz.load(std::memory_order_relaxed);
    snap.highPassSlope = ch.highPassSlope.load(std::memory_order_relaxed);
    snap.eqEnabled = ch.eqEnabled.load(std::memory_order_relaxed);
    for (size_t b = 0; b < 4; ++b) {
      snap.eqFrequency[b] = ch.eqFrequency[b].load(std::memory_order_relaxed);
      snap.eqGainDb[b] = ch.eqGainDb[b].load(std::memory_order_relaxed);
      snap.eqQ[b] = ch.eqQ[b].load(std::memory_order_relaxed);
    }
    snap.gateEnabled = ch.gateEnabled.load(std::memory_order_relaxed);
    snap.gateThresholdDb = ch.gateThresholdDb.load(std::memory_order_relaxed);
    snap.gateRatio = ch.gateRatio.load(std::memory_order_relaxed);
    snap.gateAttackMs = ch.gateAttackMs.load(std::memory_order_relaxed);
    snap.gateReleaseMs = ch.gateReleaseMs.load(std::memory_order_relaxed);
    snap.compressorEnabled =
        ch.compressorEnabled.load(std::memory_order_relaxed);
    snap.compressorMode = ch.compressorMode.load(std::memory_order_relaxed);
    snap.compressorThresholdDb =
        ch.compressorThresholdDb.load(std::memory_order_relaxed);
    snap.compressorRatio = ch.compressorRatio.load(std::memory_order_relaxed);
    snap.compressorAttackMs =
        ch.compressorAttackMs.load(std::memory_order_relaxed);
    snap.compressorReleaseMs =
        ch.compressorReleaseMs.load(std::memory_order_relaxed);
    for (size_t a = 0; a < 6 && a < kAuxBusCapacity; ++a)
      snap.auxSendDb[a] = ch.auxSendDb[a].load(std::memory_order_relaxed);
    return snap;
  }

  bool restoreChannelAISnapshot(int chIdx,
                                const ChannelAISnapshot &snap) noexcept {
    if (!snap.valid || chIdx < 0 || chIdx >= kLogicalChannels)
      return false;
    auto &ch = channels[static_cast<size_t>(chIdx)];
    ch.inputGainDb.store(snap.inputGainDb, std::memory_order_relaxed);
    ch.gainDb.store(snap.gainDb, std::memory_order_relaxed);
    ch.highPassEnabled.store(snap.highPassEnabled, std::memory_order_relaxed);
    ch.highPassFreqHz.store(snap.highPassFreqHz, std::memory_order_relaxed);
    ch.highPassSlope.store(snap.highPassSlope, std::memory_order_relaxed);
    ch.eqEnabled.store(snap.eqEnabled, std::memory_order_relaxed);
    for (size_t b = 0; b < 4; ++b) {
      ch.eqFrequency[b].store(snap.eqFrequency[b], std::memory_order_relaxed);
      ch.eqGainDb[b].store(snap.eqGainDb[b], std::memory_order_relaxed);
      ch.eqQ[b].store(snap.eqQ[b], std::memory_order_relaxed);
    }
    ch.gateEnabled.store(snap.gateEnabled, std::memory_order_relaxed);
    ch.gateThresholdDb.store(snap.gateThresholdDb, std::memory_order_relaxed);
    ch.gateRatio.store(snap.gateRatio, std::memory_order_relaxed);
    ch.gateAttackMs.store(snap.gateAttackMs, std::memory_order_relaxed);
    ch.gateReleaseMs.store(snap.gateReleaseMs, std::memory_order_relaxed);
    ch.compressorEnabled.store(snap.compressorEnabled,
                               std::memory_order_relaxed);
    ch.compressorMode.store(snap.compressorMode, std::memory_order_relaxed);
    ch.compressorThresholdDb.store(snap.compressorThresholdDb,
                                   std::memory_order_relaxed);
    ch.compressorRatio.store(snap.compressorRatio, std::memory_order_relaxed);
    ch.compressorAttackMs.store(snap.compressorAttackMs,
                                std::memory_order_relaxed);
    ch.compressorReleaseMs.store(snap.compressorReleaseMs,
                                 std::memory_order_relaxed);
    for (size_t a = 0; a < 6 && a < kAuxBusCapacity; ++a)
      ch.auxSendDb[a].store(snap.auxSendDb[a], std::memory_order_relaxed);
    syncAuxRoutingSnapshot();
    return true;
  }

  // Centralized AI Recommendation application enforcing AISafetyLimits.
  // scope: 0 = ALL, 1 = EQ, 2 = DYNAMICS, 3 = TRIM, 4 = FX, 5 = MONITOR, 6 = FEEDBACK
  bool applyAIRecommendation(int chIdx,
                             const nova::graviton::AIRecommendation &rec,
                             int scope = 0) noexcept {
    if (chIdx < 0 || chIdx >= kLogicalChannels || !rec.valid ||
        rec.insufficientData)
      return false;

    auto &ch = channels[static_cast<size_t>(chIdx)];
    const auto &limits = channelAI.getSafetyLimits();

    if (scope == 0 || scope == 3) {
      ch.inputGainDb.store(
          std::clamp(rec.targetTrimDb, limits.minInputTrimDb,
                     limits.maxInputTrimDb),
          std::memory_order_relaxed);
    }
    if (scope == 0 || scope == 1) {
      ch.highPassEnabled.store(rec.hpfEnabled, std::memory_order_relaxed);
      ch.highPassFreqHz.store(rec.hpfFreqHz, std::memory_order_relaxed);
      ch.highPassSlope.store(rec.hpfSlope, std::memory_order_relaxed);
      ch.eqEnabled.store(true, std::memory_order_relaxed);
      for (size_t b = 0; b < 4; ++b) {
        ch.eqFrequency[b].store(rec.eqFreqHz[b], std::memory_order_relaxed);
        ch.eqGainDb[b].store(
            std::clamp(rec.eqGainDb[b], limits.minEqGainDb, limits.maxEqGainDb),
            std::memory_order_relaxed);
        ch.eqQ[b].store(rec.eqQ[b], std::memory_order_relaxed);
      }
    }
    if (scope == 0 || scope == 2) {
      ch.gateEnabled.store(rec.gateEnabled, std::memory_order_relaxed);
      ch.gateThresholdDb.store(
          std::clamp(rec.gateThresholdDb, limits.minGateThresholdDb,
                     limits.maxGateThresholdDb),
          std::memory_order_relaxed);
      ch.gateRatio.store(rec.gateRatio, std::memory_order_relaxed);
      ch.gateAttackMs.store(rec.gateAttackMs, std::memory_order_relaxed);
      ch.gateReleaseMs.store(rec.gateReleaseMs, std::memory_order_relaxed);

      ch.compressorEnabled.store(rec.compEnabled, std::memory_order_relaxed);
      ch.compressorMode.store(std::clamp(rec.compMode, 0, 4),
                              std::memory_order_relaxed);
      ch.compressorThresholdDb.store(
          std::clamp(rec.compThresholdDb, limits.minCompThresholdDb,
                     limits.maxCompThresholdDb),
          std::memory_order_relaxed);
      ch.compressorRatio.store(rec.compRatio, std::memory_order_relaxed);
      ch.compressorAttackMs.store(rec.compAttackMs, std::memory_order_relaxed);
      ch.compressorReleaseMs.store(rec.compReleaseMs,
                                   std::memory_order_relaxed);
    }
    if (scope == 0 || scope == 4) {
      if (kAuxBusCapacity >= 6) {
        ch.auxSendDb[4].store(rec.fxSendDb, std::memory_order_relaxed);
        ch.auxSendDb[5].store(rec.fxSendDb, std::memory_order_relaxed);
      }
      syncAuxRoutingSnapshot();
    }
    if (scope == 0 || scope == 5) {
      for (size_t a = 0; a < 4 && a < kAuxBusCapacity; ++a) {
        ch.auxSendDb[a].store(rec.auxSendDb[a], std::memory_order_relaxed);
      }
      syncAuxRoutingSnapshot();
    }
    if (scope == 6 && rec.afsActive && rec.afsFreqHz > 20.0f) {
      ch.eqEnabled.store(true, std::memory_order_relaxed);
      ch.eqFrequency[2].store(rec.afsFreqHz, std::memory_order_relaxed);
      ch.eqGainDb[2].store(
          std::clamp(rec.afsNotchGainDb, limits.minEqGainDb,
                     limits.maxEqGainDb),
          std::memory_order_relaxed);
      ch.eqQ[2].store(rec.afsQ, std::memory_order_relaxed);
    }
    return true;
  }

  nova::graviton::AutoSoundcheckResult
  runAutoSoundcheckAudit(int maxChannels = 16) noexcept {
    const auto &limits = channelAI.getSafetyLimits();
    auto &res = channelAI.getSoundcheckResult();
    const int limitCount =
        std::clamp(maxChannels, 1, static_cast<int>(channels.size()));
    int activeCount = 0;
    float totalHeadroom = 0.0f;
    bool anyClip = false;
    bool anyPhaseInverted = false;
    for (int i = 0; i < limitCount; ++i) {
      auto &ch = channels[static_cast<size_t>(i)];
      const float peakLin = ch.trimmedPeak.load(std::memory_order_relaxed);
      const float peakDb = (peakLin > 1.0e-6f)
                               ? (20.0f * std::log10(peakLin))
                               : -120.0f;
      if (ch.polarityInverted.load(std::memory_order_relaxed))
        anyPhaseInverted = true;
      if (peakDb > -65.0f) {
        ++activeCount;
        const float deltaDb = -18.0f - peakDb;
        const float curTrim = ch.inputGainDb.load(std::memory_order_relaxed);
        const float safeTrim = std::clamp(
            curTrim + deltaDb, limits.minInputTrimDb, limits.maxInputTrimDb);
        ch.inputGainDb.store(safeTrim, std::memory_order_relaxed);
        const float headroom = std::max(0.0f, -peakDb);
        totalHeadroom += headroom;
        if (peakDb > -0.5f)
          anyClip = true;
      }
    }
    res.auditedChannels = activeCount;
    res.targetNominalDb = -18.0f;
    res.averageHeadroomDb =
        (activeCount > 0) ? (totalHeadroom / static_cast<float>(activeCount))
                          : 0.0f;
    res.phaseAligned = anyPhaseInverted;
    res.clippingDetected = anyClip;
    return res;
  }

  nova::graviton::AutoSoundcheckResult runHeadroomAudit() noexcept {
    auto &res = channelAI.getSoundcheckResult();
    int activeCount = 0;
    float totalHeadroom = 0.0f;
    bool anyClip = false;
    for (const auto &ch : channels) {
      const float peakLin = ch.trimmedPeak.load(std::memory_order_relaxed);
      const float peakDb = (peakLin > 1.0e-6f)
                               ? (20.0f * std::log10(peakLin))
                               : -120.0f;
      if (peakDb > -65.0f) {
        ++activeCount;
        totalHeadroom += std::max(0.0f, -peakDb);
        if (peakDb > -0.5f)
          anyClip = true;
      }
    }
    res.auditedChannels = activeCount;
    res.averageHeadroomDb =
        (activeCount > 0) ? (totalHeadroom / static_cast<float>(activeCount))
                          : 0.0f;
    res.clippingDetected = anyClip;
    return res;
  }

  bool isChannelStereoLinked(int channelIndex,
                             int *partnerIndex = nullptr) const noexcept {
    if (channelIndex < 0 || channelIndex >= kLogicalChannels)
      return false;
    const int pair = channelIndex / 2;
    const bool linked =
        stereoLink[static_cast<size_t>(pair)].load(std::memory_order_relaxed);
    if (partnerIndex != nullptr) {
      *partnerIndex =
          (channelIndex % 2 == 0) ? (channelIndex + 1) : (channelIndex - 1);
    }
    return linked;
  }

  void retryPendingMatrixRouting() noexcept {
    matrixRoutingPublisher.retryPending();
  }

  void audioDeviceAboutToStart(juce::AudioIODevice *device) override {
    deviceFailure.store(false, std::memory_order_relaxed);
    lastDeviceError.clear();
    deviceStartCount.fetch_add(1, std::memory_order_relaxed);
    sampleRate.store(device->getCurrentSampleRate());
    if (monitorFifoL.size() < static_cast<size_t>(kMonitorFifoCapacity)) {
      monitorFifoL.assign(static_cast<size_t>(kMonitorFifoCapacity), 0.0f);
      monitorFifoR.assign(static_cast<size_t>(kMonitorFifoCapacity), 0.0f);
    }
    if (!monitorConsumerActive.load(std::memory_order_acquire)) {
      monitorFifo.reset();
    } else {
      monitorFifoResetRequested.store(true, std::memory_order_release);
    }
    monitorEq.prepare(device->getCurrentSampleRate());
    effectsRack.setSampleRate(device->getCurrentSampleRate());
    duganAutomixer.setSampleRate(device->getCurrentSampleRate());
    matrixPlusEngine.setSampleRate(device->getCurrentSampleRate());
    rtaAnalyzer.setSampleRate(device->getCurrentSampleRate());
    dawStreamer.setSampleRate(device->getCurrentSampleRate());
    hardwareStreamBridge.setSampleRate(device->getCurrentSampleRate());
    hardwareStreamBridge.setHardwareBufferSize(
        device->getCurrentBufferSizeSamples());
    for (int c = 0; c < kLogicalChannels; ++c) {
      dynamicEqEngine[static_cast<size_t>(c)].setSampleRate(
          device->getCurrentSampleRate());
      sidechainKeyFilter[static_cast<size_t>(c)].setSampleRate(
          device->getCurrentSampleRate());
    }
    blockSize.store(device->getCurrentBufferSizeSamples());
    const auto activeInputs = device->getActiveInputChannels();
    const auto activeOutputs = device->getActiveOutputChannels();
    inputCount.store(activeInputs.countNumberOfSetBits());
    outputCount.store(activeOutputs.countNumberOfSetBits());

    inputPhysicalToCallback.fill(-1);
    outputPhysicalToCallback.fill(-1);
    int compactInput = 0, compactOutput = 0;
    for (int physical = 0;
         physical < static_cast<int>(inputPhysicalToCallback.size());
         ++physical)
      if (activeInputs[physical])
        inputPhysicalToCallback[static_cast<size_t>(physical)] = compactInput++;
    for (int physical = 0;
         physical < static_cast<int>(outputPhysicalToCallback.size());
         ++physical)
      if (activeOutputs[physical])
        outputPhysicalToCallback[static_cast<size_t>(physical)] =
            compactOutput++;

    const int preparedSamples =
        juce::jmax(8192, device->getCurrentBufferSizeSamples());
    playbackBlock.setSize(2, preparedSamples, false, false, true);
    mainRecordBuffer.setSize(2, preparedSamples, false, false, true);
    rawMultitrackRecordBuffer.setSize(64, preparedSamples, false, false, true);

    const double sr = juce::jmax(1.0, device->getCurrentSampleRate());
    // Graviton Mk 2: first-order HPF coefficient and per-channel delay states
    // are double.
    highPassAlpha =
        std::exp(-2.0 * juce::MathConstants<double>::pi * 100.0 / sr);
    compressorAttack = static_cast<float>(std::exp(-1.0 / (0.010 * sr)));
    compressorRelease = static_cast<float>(std::exp(-1.0 / (0.120 * sr)));
    signalGenerator.reset(sr);
    prevSurroundActive = false;
    binauralPreviewSpatializer.reset();
    running.store(true);
  }

  void audioDeviceStopped() override {
    running.store(false, std::memory_order_relaxed);
    sampleRate.store(0.0, std::memory_order_relaxed);
    blockSize.store(0, std::memory_order_relaxed);
    inputCount.store(0, std::memory_order_relaxed);
    outputCount.store(0, std::memory_order_relaxed);
    deviceStopCount.fetch_add(1, std::memory_order_relaxed);
    // Finalize the WAV writer when the device stops so the file is closed
    // cleanly.
    if (recorder.isRecording())
      recorder.stopRecording();
  }

  void audioDeviceError(const juce::String &errorMessage) override {
    deviceFailure.store(true, std::memory_order_relaxed);
    deviceErrorCount.fetch_add(1, std::memory_order_relaxed);
    lastDeviceError = errorMessage.substring(0, 512);
    if (recorder.isRecording())
      recorder.stopRecording();
  }

  // Deterministic test seam: configures compact identity maps and calls the
  // exact production callback path. It is intentionally not used by the
  // shipping UI.
  void processTestAudioBlock(const float *const *input, int numInputs,
                             float *const *output, int numOutputs,
                             int numSamples, double testSampleRate = 48000.0) {
    syncAuxRoutingSnapshot();
    syncMatrixRoutingSnapshot();
    inputPhysicalToCallback.fill(-1);
    outputPhysicalToCallback.fill(-1);
    const int safeInputs = juce::jlimit(
        0, static_cast<int>(inputPhysicalToCallback.size()), numInputs);
    const int safeOutputs = juce::jlimit(
        0, static_cast<int>(outputPhysicalToCallback.size()), numOutputs);
    for (int i = 0; i < safeInputs; ++i)
      inputPhysicalToCallback[static_cast<size_t>(i)] = i;
    for (int i = 0; i < safeOutputs; ++i)
      outputPhysicalToCallback[static_cast<size_t>(i)] = i;
    inputCount.store(safeInputs, std::memory_order_relaxed);
    outputCount.store(safeOutputs, std::memory_order_relaxed);
    sampleRate.store(juce::jmax(8000.0, testSampleRate),
                     std::memory_order_relaxed);
    signalGenerator.setSampleRate(juce::jmax(8000.0, testSampleRate));
    juce::AudioIODeviceCallbackContext context{};
    audioDeviceIOCallbackWithContext(input, numInputs, output, numOutputs,
                                     numSamples, context);
  }

  // Phase 67: Offline render of the master bus (stereo) into caller buffers.
  // Reuses the deterministic test seam to run the exact production callback
  // path off the realtime thread; master L/R are written into the caller
  // buffers.
  void renderOfflineBlocks(const float *const *input, int numInputs,
                           int numSamples, double testSampleRate,
                           float *outMainL, float *outMainR) {
    float *outputs[2] = {outMainL, outMainR};
    processTestAudioBlock(input, numInputs, outputs, 2, numSamples,
                          testSampleRate);
  }

  // Phase 67: Offline export of the master bus (stereo) to a 24-bit WAV file
  // through the JUCE WAV writer on the offline thread (never realtime path).
  bool exportOfflineToWav(const juce::File &target, const float *const *input,
                          int numInputs, int numSamples,
                          double testSampleRate) {
    if (numSamples <= 0 || target.getFileName().isEmpty())
      return false;
    auto stream = target.createOutputStream();
    if (stream == nullptr)
      return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writerPtr(
        wav.createWriterFor(stream.release(), testSampleRate, 2, 24, {}, 0));
    if (writerPtr == nullptr)
      return false;
    int framesDone = 0;
    while (framesDone < numSamples) {
      const int chunkFrames = std::min(4096, numSamples - framesDone);
      std::vector<float> lbuf(chunkFrames, 0.0f);
      std::vector<float> rbuf(chunkFrames, 0.0f);
      float *outputs[2] = {lbuf.data(), rbuf.data()};
      processTestAudioBlock(input, numInputs, outputs, 2, chunkFrames,
                            testSampleRate);
      juce::AudioBuffer<float> chunk(2, chunkFrames);
      for (int i = 0; i < chunkFrames; ++i) {
        chunk.setSample(0, i, lbuf[static_cast<size_t>(i)]);
        chunk.setSample(1, i, rbuf[static_cast<size_t>(i)]);
      }
      if (!writerPtr->writeFromAudioSampleBuffer(chunk, 0, chunkFrames))
        return false;
      framesDone += chunkFrames;
    }
    return true;
  }

  // Phase 89: Offline Multi-Track Stems Bounce (Chapter 36).
  // While armed, the Main L/R bus and every bus stem of each rendered sample is
  // copied into a caller-owned plan. Arming only ever happens on the
  // single-threaded offline bounce path (renderOfflineBlocks), never from the
  // realtime callback, so the realtime path pays exactly one relaxed atomic
  // load per audio block.
  static constexpr int kOfflineStemCount =
      kVisibleAuxSends + kVisibleMatrixBuses;

  struct OfflineStemCapture {
    float *masterL = nullptr;
    float *masterR = nullptr;
    std::array<float *, kOfflineStemCount> stems{};
    int numSamples = 0;
  };

  void armOfflineCapture(const OfflineStemCapture &plan) noexcept {
    offlineCapture = plan;
    offlineCaptureArmed.store(true, std::memory_order_release);
  }

  void disarmOfflineCapture() noexcept {
    offlineCaptureArmed.store(false, std::memory_order_release);
  }

  bool isOfflineCaptureArmed() const noexcept {
    return offlineCaptureArmed.load(std::memory_order_acquire);
  }

  std::atomic<bool> offlineCaptureArmed{false};
  OfflineStemCapture offlineCapture{};

  void audioDeviceIOCallbackWithContext(
      const float *const *input, int numInputs, float *const *output,
      int numOutputs, int numSamples,
      const juce::AudioIODeviceCallbackContext &) override {
    callbackCount.fetch_add(1, std::memory_order_relaxed);
    if (numSamples > 0) {
      callbackFrames.fetch_add(static_cast<uint64_t>(numSamples),
                               std::memory_order_relaxed);
      int observed = largestCallbackBlock.load(std::memory_order_relaxed);
      while (numSamples > observed &&
             !largestCallbackBlock.compare_exchange_weak(
                 observed, numSamples, std::memory_order_relaxed,
                 std::memory_order_relaxed)) {
      }
    }
    if (numOutputs <= 0 || output == nullptr)
      return;
    // Graviton Mk 2: lock-free consumption of newest published AUX and Matrix
    // routing snapshots
    auxRoutingQueue.tryConsumeLatest(activeAuxRoutingSnapshot);
    matrixRoutingQueue.tryConsumeLatest(activeMatrixRoutingSnapshot);

    // Clear all valid outputs first. Shared, allocation-free helper is
    // exercised by tests.
    nova::clearOutputBuffers(output, numOutputs, numSamples);

    // Phase 89: offline stem bounce capture flag (set only by the offline
    // render path; hoisted out of the per-sample loop).
    const bool offlineCapturing =
        offlineCaptureArmed.load(std::memory_order_acquire);

    bool anySoloA = false;
    bool anySoloB = false;
    for (const auto &ch : channels) {
      if (ch.solo.load(std::memory_order_relaxed) ||
          ch.sidechainKeyListen.load(std::memory_order_relaxed)) {
        if (ch.soloBus.load(std::memory_order_relaxed) == 1)
          anySoloB = true;
        else
          anySoloA = true;
      }
    }
    const bool anySolo = anySoloA || anySoloB;

    const double monTargetA =
        monitorMuteA.load(std::memory_order_relaxed)
            ? 0.0
            : nova::graviton::dbToGain(
                  std::clamp(static_cast<double>(
                                 monitorLevelA.load(std::memory_order_relaxed)),
                             -60.0, 10.0));
    const double monTargetB =
        monitorMuteB.load(std::memory_order_relaxed)
            ? 0.0
            : nova::graviton::dbToGain(
                  std::clamp(static_cast<double>(
                                 monitorLevelB.load(std::memory_order_relaxed)),
                             -60.0, 10.0));
    const bool tbActive = talkbackActive.load(std::memory_order_relaxed);
    const double duckerGain =
        tbActive ? nova::graviton::dbToGain(
                       std::clamp(static_cast<double>(talkbackDuckerDb.load(
                                      std::memory_order_relaxed)),
                                  -60.0, 0.0))
                 : 1.0;
    const double dimScale = nova::graviton::dbToGain(-20.0);
    const double effectiveMonGainA =
        monTargetA *
        (monitorDimA.load(std::memory_order_relaxed) ? dimScale : 1.0) *
        duckerGain;
    const double effectiveMonGainB =
        monTargetB *
        (monitorDimB.load(std::memory_order_relaxed) ? dimScale : 1.0) *
        duckerGain;

    float blockMonPeakL_A = 0.0f, blockMonPeakR_A = 0.0f;
    float blockMonPeakL_B = 0.0f, blockMonPeakR_B = 0.0f;
    std::array<float, 4096> blockMonBufL{};
    std::array<float, 4096> blockMonBufR{};

    uint8_t activeMuteGroupBits = 0;
    for (size_t g = 0; g < 8; ++g) {
      if (muteGroupMasters[g].load(std::memory_order_relaxed))
        activeMuteGroupBits |= static_cast<uint8_t>(1u << g);
    }

    std::array<bool, kLogicalChannels> channelGatedBlock{};

    for (int c = 0; c < kLogicalChannels; ++c) {
      const auto &state = channels[static_cast<size_t>(c)];
      const bool isMuteSafe = state.muteSafe.load(std::memory_order_relaxed);
      const bool isSoloSafe = state.soloSafe.load(std::memory_order_relaxed);
      const bool channelMuted = state.muted.load(std::memory_order_relaxed);
      const uint8_t channelMgMask =
          state.muteGroupMask.load(std::memory_order_relaxed);
      const bool mgGated =
          !isMuteSafe && ((channelMgMask & activeMuteGroupBits) != 0);
      const bool effectiveMute = channelMuted || mgGated;
      const bool soloSelected =
          state.solo.load(std::memory_order_relaxed) ||
          state.sidechainKeyListen.load(std::memory_order_relaxed);
      const bool soloGated = sipEnabled.load(std::memory_order_relaxed) &&
                             anySolo && !soloSelected && !isSoloSafe;
      channelGatedBlock[static_cast<size_t>(c)] = effectiveMute || soloGated;
    }

    const int playbackSamples =
        juce::jmin(numSamples, playbackBlock.getNumSamples());
    playbackBlock.clear();
    if (playbackLoaded.load(std::memory_order_relaxed) && playbackSamples > 0)
      playbackTransport.getNextAudioBlock(
          juce::AudioSourceChannelInfo(&playbackBlock, 0, playbackSamples));

    float peakL = 0.0f, peakR = 0.0f;
    std::array<float, kLogicalChannels> channelBlockPeaks{};
    std::array<float, kLogicalChannels> blockInputPeaks{};
    std::array<float, kLogicalChannels> blockTrimmedPeaks{};
    std::array<float, kLogicalChannels> blockCompressorGR{};
    std::array<float, kLogicalChannels> blockGateReduction{};
    std::array<double, kLogicalChannels> dynamicsAttackCoeff{};
    std::array<double, kLogicalChannels> dynamicsReleaseCoeff{};
    std::array<nova::graviton::DynamicsSettings, kLogicalChannels>
        dynamicsSettingsBlock{};
    const double masterTarget = nova::graviton::dbToGain(std::clamp(
        static_cast<double>(masterDb.load(std::memory_order_relaxed)), -60.0,
        10.0));
    std::array<double, kAuxBusCapacity> auxMasterTarget{};
    for (int a = 0; a < kAuxBusCapacity; ++a)
      auxMasterTarget[static_cast<size_t>(a)] =
          nova::graviton::dbToGain(std::clamp(
              static_cast<double>(auxMasterDb[static_cast<size_t>(a)].load(
                  std::memory_order_relaxed)),
              -60.0, 10.0));
    std::array<double, kMatrixBusCapacity> matrixMasterTarget{};
    for (int m = 0; m < kMatrixBusCapacity; ++m)
      matrixMasterTarget[static_cast<size_t>(m)] =
          nova::graviton::dbToGain(std::clamp(
              static_cast<double>(matrixMasterDb[static_cast<size_t>(m)].load(
                  std::memory_order_relaxed)),
              -60.0, 10.0));
    std::array<float, kAuxBusCapacity> blockAuxPeaks{};
    std::array<float, kMatrixBusCapacity> blockMatrixPeaks{};
    // Snapshot stable DSP controls once per block. This reduces atomic loads
    // and dB conversions in the innermost per-sample loop.
    std::array<float, kLogicalChannels> inputTrimLinearGain{};
    std::array<bool, kLogicalChannels> gateEnabledBlock{};
    std::array<bool, kLogicalChannels> compressorEnabledBlock{};
    std::array<float, kLogicalChannels> gateThresholdBlock{};
    std::array<float, kLogicalChannels> gateRatioBlock{};
    std::array<float, kLogicalChannels> compressorThresholdBlock{};
    std::array<float, kLogicalChannels> compressorRatioBlock{};
    // Graviton snapshots channel fader/VCA targets and pan gains once per
    // block. This keeps dB conversion and trigonometry out of the sample loop.
    std::array<double, kLogicalChannels> channelTargetGainBlock{};
    std::array<double, kLogicalChannels> panGainLeftBlock{};
    std::array<double, kLogicalChannels> panGainRightBlock{};
    std::array<bool, kLogicalChannels> monitorAssignedBlock{};
    std::array<bool, kLogicalChannels> monitorPreFaderBlock{};
    std::array<double, kLogicalChannels> monitorSendGainBlock{};
    std::array<nova::graviton::SurroundSpeakerGains, kLogicalChannels>
        channelSurroundGains{};
    std::array<int, kLogicalChannels> channelSurroundModes{};
    std::array<float, 12> blockSurroundPeaks{};
    std::array<int, kLogicalChannels> channelCallbackSource{};
    std::array<bool, kLogicalChannels> channelInputAvailable{};
    std::array<bool, kLogicalChannels> isExternalSidechainSource{};
    bool anyExternalSidechainTarget = false;
    bool anyDuganAssigned = false;
    bool anySurroundActive = false;

    const bool vscBlockActive = dawStreamer.isVirtualSoundcheckActive();
    bool anyStageBoxOutputPatched = false;
    for (size_t p = 0;
         p < nova::graviton::StageBoxPatchMatrix::kTotalOutputPorts; ++p) {
      nova::graviton::OutputBusSourceType outType;
      int outIdx = 0;
      stageBoxPatchMatrix.getOutputPatch(static_cast<int>(p), outType, outIdx);
      if (outType != nova::graviton::OutputBusSourceType::Unassigned) {
        anyStageBoxOutputPatched = true;
        break;
      }
    }

    for (int c = 0; c < kLogicalChannels; ++c) {
      const auto channelIndex = static_cast<size_t>(c);
      const auto &channel = channels[channelIndex];
      if (channel.sidechainSourceType.load(std::memory_order_relaxed) ==
          static_cast<int>(nova::graviton::KeySourceType::External)) {
        anyExternalSidechainTarget = true;
        const int extSrcCh = std::clamp(
            channel.sidechainExternalChannel.load(std::memory_order_relaxed),
            0, kLogicalChannels - 1);
        isExternalSidechainSource[static_cast<size_t>(extSrcCh)] = true;
      }
      if (channel.duganGroup.load(std::memory_order_relaxed) > 0)
        anyDuganAssigned = true;

      int physicalSource = channel.sourceInput.load(std::memory_order_relaxed);
      const int sbPatch = stageBoxPatchMatrix.getInputPatch(c);
      if (sbPatch >= 0 &&
          sbPatch < static_cast<int>(inputPhysicalToCallback.size())) {
        physicalSource = sbPatch;
      }
      int source = -1;
      if (physicalSource >= 0 &&
          physicalSource < static_cast<int>(inputPhysicalToCallback.size()) &&
          input != nullptr) {
        source = inputPhysicalToCallback[static_cast<size_t>(physicalSource)];
      }
      channelCallbackSource[channelIndex] = source;
      channelInputAvailable[channelIndex] =
          (vscBlockActive && c < 64) ||
          (source >= 0 && source < numInputs && input[source] != nullptr);
    }

    // Heritage Virtual Cable Return (External / VBAN -> Logical Channels)
    const double rawEngineSr = sampleRate.load(std::memory_order_relaxed);
    const double vcSr = (rawEngineSr > 1000.0) ? rawEngineSr : 48000.0;
    virtualTransport.setEngineSampleRate(vcSr);
    const auto pumpVirtualCableShmIfActive = [this]() noexcept {
      const auto bk = virtualTransport.getTransportBackend();
      if (virtualTransport.isSharedMemoryOpen() &&
          (bk == nova::graviton::TransportBackend::SharedMemory ||
           bk == nova::graviton::TransportBackend::Both)) {
        virtualTransport.pumpSharedMemoryFromAudioThread();
      }
    };
    pumpVirtualCableShmIfActive();
    channelVirtualCableSource.fill(-1);
    const int vcBlockSamples = std::min(numSamples, 4096);
    for (size_t k = 0; k < nova::graviton::VirtualCableTransport::kNumVirtualCables;
         ++k) {
      auto &vc = virtualTransport.cables[k];
      if (!vc.returnEnabled.load(std::memory_order_relaxed))
        continue;
      vc.pullReturnBlock(virtualReturnBufL[k].data(),
                         virtualReturnBufR[k].data(), vcBlockSamples, vcSr);
      const int chL = vc.returnTargetChannelL.load(std::memory_order_relaxed);
      const int chR = vc.returnTargetChannelR.load(std::memory_order_relaxed);
      if (chL >= 0 && chL < kLogicalChannels) {
        channelVirtualCableSource[static_cast<size_t>(chL)] =
            static_cast<int>(k * 2);
        channelInputAvailable[static_cast<size_t>(chL)] = true;
      }
      if (chR >= 0 && chR < kLogicalChannels && chR != chL) {
        channelVirtualCableSource[static_cast<size_t>(chR)] =
            static_cast<int>(k * 2 + 1);
        channelInputAvailable[static_cast<size_t>(chR)] = true;
      }
    }

    std::array<bool, kLogicalChannels> vcaMutedChannelBlock{};
    for (int c = 0; c < kLogicalChannels; ++c) {
      const auto channelIndex = static_cast<size_t>(c);
      const auto &channel = channels[channelIndex];
      if (!channelInputAvailable[channelIndex] &&
          !channel.sidechainKeyListen.load(std::memory_order_relaxed) &&
          !isExternalSidechainSource[channelIndex])
        continue;
      if (channelGatedBlock[channelIndex] &&
          channelGainSmooth[channelIndex] < 1.0e-6 &&
          !isExternalSidechainSource[channelIndex])
        continue;
      inputTrimLinearGain[channelIndex] = juce::Decibels::decibelsToGain(
          juce::jlimit(-20.0f, 60.0f,
                       channel.inputGainDb.load(std::memory_order_relaxed)));
      gateEnabledBlock[channelIndex] =
          channel.gateEnabled.load(std::memory_order_relaxed);
      compressorEnabledBlock[channelIndex] =
          channel.compressorEnabled.load(std::memory_order_relaxed);
      gateThresholdBlock[channelIndex] =
          juce::jlimit(-80.0f, 0.0f,
                       channel.gateThresholdDb.load(std::memory_order_relaxed));
      gateRatioBlock[channelIndex] = juce::jlimit(
          1.0f, 10.0f, channel.gateRatio.load(std::memory_order_relaxed));
      compressorThresholdBlock[channelIndex] = juce::jlimit(
          -60.0f, 0.0f,
          channel.compressorThresholdDb.load(std::memory_order_relaxed));
      compressorRatioBlock[channelIndex] = juce::jlimit(
          1.0f, 20.0f, channel.compressorRatio.load(std::memory_order_relaxed));
      const int vcaGroupBlock =
          channel.vcaGroup.load(std::memory_order_relaxed);
      const bool assignedToVcaBlock = vcaGroupBlock >= 1 && vcaGroupBlock <= 24;
      const auto vcaIndexBlock =
          static_cast<size_t>(juce::jlimit(1, 24, vcaGroupBlock) - 1);
      const double vcaDbBlock =
          assignedToVcaBlock
              ? static_cast<double>(
                    vcaMasterDb[vcaIndexBlock].load(std::memory_order_relaxed))
              : 0.0;
      const bool isMuteSafe = channel.muteSafe.load(std::memory_order_relaxed);
      const bool vcaMutedBlock =
          assignedToVcaBlock && !isMuteSafe &&
          vcaMuted[vcaIndexBlock].load(std::memory_order_relaxed);
      vcaMutedChannelBlock[channelIndex] = vcaMutedBlock;
      const double effectiveDbBlock = std::clamp(
          static_cast<double>(channel.gainDb.load(std::memory_order_relaxed)) +
              vcaDbBlock,
          -120.0, 20.0);
      channelTargetGainBlock[channelIndex] =
          (vcaMutedBlock || channelGatedBlock[channelIndex])
              ? 0.0
              : nova::graviton::dbToGain(effectiveDbBlock);
      const double panBlock = nova::graviton::clampFinite(
          static_cast<double>(channel.pan.load(std::memory_order_relaxed)),
          -1.0, 1.0, 0.0);
      const auto panGainsBlock = nova::graviton::equalPowerPanGains(panBlock);
      panGainLeftBlock[channelIndex] = panGainsBlock.left;
      panGainRightBlock[channelIndex] = panGainsBlock.right;

      const bool monAssigned =
          channel.monitorAssigned.load(std::memory_order_relaxed);
      monitorAssignedBlock[channelIndex] = monAssigned;
      if (monAssigned) {
        monitorPreFaderBlock[channelIndex] =
            channel.monitorPreFader.load(std::memory_order_relaxed);
        const float monSendDb = nova::graviton::clampFinite(
            channel.monitorSendDb.load(std::memory_order_relaxed), -90.0f,
            10.0f, 0.0f);
        monitorSendGainBlock[channelIndex] =
            static_cast<double>(nova::graviton::dbToGain(monSendDb));
      }

      // Phase 85: 7.1.4 Surround / Immersive 3D Spatial Vector Panning (MIDAS
      // Chapter 30)
      const int sMode = channel.surroundMode.load(std::memory_order_relaxed);
      channelSurroundModes[channelIndex] = sMode;
      if (sMode > 0) {
        anySurroundActive = true;
        channelSurroundGains[channelIndex] =
            nova::graviton::SurroundPanner::calculateGains(
                static_cast<nova::graviton::SurroundMode>(sMode),
                channel.surroundX.load(std::memory_order_relaxed),
                channel.surroundY.load(std::memory_order_relaxed),
                channel.surroundZ.load(std::memory_order_relaxed),
                channel.surroundDivergence.load(std::memory_order_relaxed),
                channel.surroundCenterPct.load(std::memory_order_relaxed),
                channel.surroundLfeGainDb.load(std::memory_order_relaxed));
        double downL = 0.0, downR = 0.0;
        nova::graviton::SurroundPanner::downmixToStereo(
            channelSurroundGains[channelIndex], downL, downR);
        panGainLeftBlock[channelIndex] = downL;
        panGainRightBlock[channelIndex] = downR;
      }
    }
    if (prevSurroundActive && !anySurroundActive) {
      binauralPreviewSpatializer.reset();
    }
    prevSurroundActive = anySurroundActive;
    const double sr =
        std::max(8000.0, sampleRate.load(std::memory_order_relaxed));
    // Graviton computes the 10 ms smoothing coefficient in double once per
    // block.
    const double gainSmoothingCoeff =
        nova::graviton::gainSmoothingCoefficient(sr, 0.010);

    for (int c = 0; c < kLogicalChannels; ++c) {
      const auto ci = static_cast<size_t>(c);
      auto &channel = channels[ci];
      if ((channelGatedBlock[ci] && channelGainSmooth[ci] < 1.0e-6) ||
          (!channelInputAvailable[ci] &&
           !channel.sidechainKeyListen.load(std::memory_order_relaxed))) {
        continue;
      }
      const float compAttackMs = juce::jlimit(
          0.1f, 500.0f,
          channel.compressorAttackMs.load(std::memory_order_relaxed));
      const float compReleaseMs = juce::jlimit(
          5.0f, 2000.0f,
          channel.compressorReleaseMs.load(std::memory_order_relaxed));
      const float gateAttackMs = juce::jlimit(
          0.1f, 100.0f, channel.gateAttackMs.load(std::memory_order_relaxed));
      const float gateReleaseMs = juce::jlimit(
          10.0f, 1000.0f, channel.gateReleaseMs.load(std::memory_order_relaxed));
      const double srSafe = std::max(1.0, static_cast<double>(sr));
      dynamicsAttackCoeff[static_cast<size_t>(c)] =
          std::exp(-1.0 / (srSafe * static_cast<double>(compAttackMs) * 0.001));
      dynamicsReleaseCoeff[static_cast<size_t>(c)] =
          std::exp(-1.0 / (srSafe * static_cast<double>(compReleaseMs) * 0.001));
      const double gateAttCoeff =
          std::exp(-1.0 / (srSafe * static_cast<double>(gateAttackMs) * 0.001));
      const double gateRelCoeff =
          std::exp(-1.0 / (srSafe * static_cast<double>(gateReleaseMs) * 0.001));
      auto &dynamics = dynamicsSettingsBlock[static_cast<size_t>(c)];
      dynamics.gateEnabled = gateEnabledBlock[static_cast<size_t>(c)];
      dynamics.compressorEnabled =
          compressorEnabledBlock[static_cast<size_t>(c)];
      dynamics.compressorMode = static_cast<nova::graviton::CompressorMode>(
          channel.compressorMode.load(std::memory_order_relaxed));
      dynamics.gateMode = static_cast<nova::graviton::GateMode>(
          channel.gateMode.load(std::memory_order_relaxed));
      dynamics.gateThresholdDb =
          static_cast<double>(gateThresholdBlock[static_cast<size_t>(c)]);
      dynamics.gateRatio =
          static_cast<double>(gateRatioBlock[static_cast<size_t>(c)]);
      dynamics.compressorThresholdDb =
          static_cast<double>(compressorThresholdBlock[static_cast<size_t>(c)]);
      dynamics.compressorRatio =
          static_cast<double>(compressorRatioBlock[static_cast<size_t>(c)]);
      dynamics.compressorKneeDb = static_cast<double>(
          channel.compressorKneeDb.load(std::memory_order_relaxed));
      dynamics.compressorKneeMode =
          channel.compressorKneeMode.load(std::memory_order_relaxed);
      dynamics.compressorInputGainDb = static_cast<double>(
          channel.compressorInputGainDb.load(std::memory_order_relaxed));
      float makeup =
          channel.compressorMakeupGainDb.load(std::memory_order_relaxed);
      if (channel.compressorMaximize.load(std::memory_order_relaxed)) {
        const float th =
            channel.compressorThresholdDb.load(std::memory_order_relaxed);
        const float r = std::max(
            1.0f, channel.compressorRatio.load(std::memory_order_relaxed));
        makeup += std::max(0.0f, (-th) * (1.0f - 1.0f / r));
      }
      dynamics.compressorMakeupGainDb = static_cast<double>(makeup);
      dynamics.compressorRmsMs = static_cast<double>(
          channel.compressorRmsMs.load(std::memory_order_relaxed));
      dynamics.gateHoldMs = static_cast<double>(
          channel.gateHoldMs.load(std::memory_order_relaxed));
      dynamics.expanderRangeDb = 80.0;
      dynamics.sampleRate = static_cast<double>(sr);
      dynamics.attackCoefficient = dynamicsAttackCoeff[static_cast<size_t>(c)];
      dynamics.releaseCoefficient =
          dynamicsReleaseCoeff[static_cast<size_t>(c)];
      dynamics.gateAttackCoefficient = gateAttCoeff;
      dynamics.gateReleaseCoefficient = gateRelCoeff;
      const int eqModel =
          std::clamp(channel.eqModel.load(std::memory_order_relaxed), 0, 3);
      for (int b = 0; b < 4; ++b) {
        auto &filter = eqState[static_cast<size_t>(c)][static_cast<size_t>(b)];
        const double sampleRate64 = static_cast<double>(sr);
        const double frequency =
            std::clamp(static_cast<double>(
                           channel.eqFrequency[static_cast<size_t>(b)].load(
                               std::memory_order_relaxed)),
                       20.0, std::min(20000.0, sampleRate64 * 0.45));
        const double gainDb = std::clamp(
            static_cast<double>(channel.eqGainDb[static_cast<size_t>(b)].load(
                std::memory_order_relaxed)),
            -18.0, 18.0);
        const double q = std::clamp(
            static_cast<double>(channel.eqQ[static_cast<size_t>(b)].load(
                std::memory_order_relaxed)),
            0.1, 10.0);
        if (frequency != filter.cachedFrequency ||
            gainDb != filter.cachedGainDb || q != filter.cachedQ ||
            sampleRate64 != filter.cachedSampleRate ||
            eqModel != filter.cachedEqModel) {
          double effectiveGainDb = gainDb;
          double effectiveQ = q;
          if (eqModel == 1) {
            // Neve 1073: warm Class-A inductor musical broad-Q & low-mid body
            effectiveQ = std::clamp(q * 0.78, 0.1, 10.0);
            if (b <= 1 && gainDb > 0.0)
              effectiveGainDb = std::clamp(gainDb * 1.06, -18.0, 18.0);
          } else if (eqModel == 2) {
            // SSL 4000 G-EQ: punchy tighter surgical Q & console presence
            effectiveQ = std::clamp(q * 1.18, 0.1, 10.0);
            if (b >= 2 && gainDb > 0.0)
              effectiveGainDb = std::clamp(gainDb * 1.08, -18.0, 18.0);
          } else if (eqModel == 3) {
            // API 550A: proportional-Q (narrows dynamically with |gainDb|)
            effectiveQ =
                std::clamp(q * (0.75 + 0.055 * std::abs(gainDb)), 0.1, 10.0);
          }
          filter.filter.setCoefficients(nova::graviton::makePeakingEq(
              sampleRate64, frequency, effectiveGainDb, effectiveQ));
          filter.cachedFrequency = frequency;
          filter.cachedGainDb = gainDb;
          filter.cachedQ = q;
          filter.cachedSampleRate = sampleRate64;
          filter.cachedEqModel = eqModel;
        }
      }

      auto &flt = filterState[static_cast<size_t>(c)];
      const double sampleRate64 = static_cast<double>(sr);
      const double hpfFreq =
          std::clamp(static_cast<double>(channel.highPassFreqHz.load(
                         std::memory_order_relaxed)),
                     20.0, std::min(2000.0, sampleRate64 * 0.45));
      const int hpfSlope =
          channel.highPassSlope.load(std::memory_order_relaxed);
      if (hpfFreq != flt.cachedHpfFreq || hpfSlope != flt.cachedHpfSlope ||
          sampleRate64 != flt.cachedSampleRate) {
        if (hpfSlope == 0) {
          flt.hpf1.setCoefficients(nova::graviton::makeButterworthHPF(
              sampleRate64, hpfFreq, 0.70710678118654752440));
        } else {
          flt.hpf1.setCoefficients(nova::graviton::makeButterworthHPF(
              sampleRate64, hpfFreq, 0.541196100146197));
          flt.hpf2.setCoefficients(nova::graviton::makeButterworthHPF(
              sampleRate64, hpfFreq, 1.3065629648763765));
        }
        flt.cachedHpfFreq = hpfFreq;
        flt.cachedHpfSlope = hpfSlope;
      }
      const double lpfFreq =
          std::clamp(static_cast<double>(
                         channel.lowPassFreqHz.load(std::memory_order_relaxed)),
                     1000.0, std::min(20000.0, sampleRate64 * 0.45));
      const int lpfSlope = channel.lowPassSlope.load(std::memory_order_relaxed);
      if (lpfFreq != flt.cachedLpfFreq || lpfSlope != flt.cachedLpfSlope ||
          sampleRate64 != flt.cachedSampleRate) {
        if (lpfSlope == 0) {
          flt.lpf1.setCoefficients(nova::graviton::makeButterworthLPF(
              sampleRate64, lpfFreq, 0.70710678118654752440));
        } else {
          flt.lpf1.setCoefficients(nova::graviton::makeButterworthLPF(
              sampleRate64, lpfFreq, 0.541196100146197));
          flt.lpf2.setCoefficients(nova::graviton::makeButterworthLPF(
              sampleRate64, lpfFreq, 1.3065629648763765));
        }
        flt.cachedLpfFreq = lpfFreq;
        flt.cachedLpfSlope = lpfSlope;
      }
      flt.cachedSampleRate = sampleRate64;
      if (channel.allPassEnabled.load(std::memory_order_relaxed)) {
        const int apMode =
            channel.allPassFreqMode.load(std::memory_order_relaxed);
        const double apCenterFreq = (apMode == 1) ? 1500.0 : 100.0;
        const double apDeg = static_cast<double>(
            channel.allPassDegrees.load(std::memory_order_relaxed));
        const double apMax =
            (channel.allPassRange.load(std::memory_order_relaxed) == 0) ? 90.0
                                                                        : 180.0;
        allPassFilters[static_cast<size_t>(c)].update(
            sampleRate64, apCenterFreq, apDeg, apMax);
      }
    }
    if (std::abs(sr - lastCallbackSampleRate) > 1.0) {
      lastCallbackSampleRate = sr;
      mainGraphicEQ.setSampleRate(static_cast<double>(sr));
      effectsRack.setSampleRate(static_cast<double>(sr));
      duganAutomixer.setSampleRate(static_cast<double>(sr));
      matrixPlusEngine.setSampleRate(static_cast<double>(sr));
      masterBusChain.setSampleRate(static_cast<double>(sr));
      rtaAnalyzer.setSampleRate(static_cast<double>(sr));
      for (int c = 0; c < kLogicalChannels; ++c) {
        dynamicEqEngine[static_cast<size_t>(c)].setSampleRate(
            static_cast<double>(sr));
        sidechainKeyFilter[static_cast<size_t>(c)].setSampleRate(
            static_cast<double>(sr));
      }
    }

    // Phase 81: Dan Dugan Speech Automixer Gain-Sharing Matrix Update (MIDAS
    // Chapter 28)
    if (anyDuganAssigned) {
      for (int c = 0; c < kLogicalChannels; ++c) {
        const auto &state = channels[static_cast<size_t>(c)];
        auto &dCh = duganAutomixer.getChannel(static_cast<size_t>(c));
        dCh.group.store(state.duganGroup.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
        dCh.weightDb.store(state.duganWeightDb.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
        dCh.mute.store(channelGatedBlock[static_cast<size_t>(c)],
                       std::memory_order_relaxed);
      }
      duganAutomixer.updateGainSharing();
      for (int c = 0; c < kLogicalChannels; ++c) {
        channels[static_cast<size_t>(c)].duganGainDb.store(
            duganAutomixer.getChannel(static_cast<size_t>(c))
                .currentGainDb.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
      }
    }
    // Phase 65: activate the newest validated safe AUX bus graph plan at the
    // audio block boundary; realtime path stays lock-free and allocation-free.
    safeBusGraphActivation.activatePendingAtBlockBoundary();
    const auto &busPlan = safeBusGraphActivation.activePlan();
    std::array<std::size_t, kLogicalChannels> chBusRouteStart{};
    std::array<std::size_t, kLogicalChannels> chBusRouteCount{};
    bool busRoutesContiguous = true;
    {
      std::array<bool, kLogicalChannels> chBusSeen{};
      int prevRouteCh = -1;
      for (std::size_t r = 0; r < busPlan.routeCount; ++r) {
        const int rc = static_cast<int>(busPlan.routes[r].channel);
        if (rc < 0 || rc >= kLogicalChannels) {
          busRoutesContiguous = false;
          break;
        }
        const auto rci = static_cast<size_t>(rc);
        if (rc != prevRouteCh) {
          if (chBusSeen[rci]) {
            busRoutesContiguous = false;
            break;
          }
          chBusSeen[rci] = true;
          chBusRouteStart[rci] = r;
          prevRouteCh = rc;
        }
        ++chBusRouteCount[rci];
      }
    }

    const double auxIemCeilingLinBlock =
        nova::graviton::dbToGain(static_cast<double>(std::clamp(
            auxIemCeilingDb.load(std::memory_order_relaxed), -24.0f, 0.0f)));
    std::array<bool, kAuxBusCapacity> auxIemLimiterEnabledBlock{};
    std::array<bool, kAuxBusCapacity> auxMutedBlock{};
    for (int a = 0; a < kAuxBusCapacity; ++a) {
      const auto index = static_cast<size_t>(a);
      auxIemLimiterEnabledBlock[index] =
          auxIemLimiterEnabled[index].load(std::memory_order_relaxed);
      auxMutedBlock[index] = auxMuted[index].load(std::memory_order_relaxed);
    }
    std::array<bool, kMatrixBusCapacity> matrixMutedBlock{};
    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      matrixMutedBlock[static_cast<size_t>(m)] =
          matrixMuted[static_cast<size_t>(m)].load(std::memory_order_relaxed);
    }

    // Phase 66: snapshot final master bus chain controls (true-peak limiter,
    // parametric master EQ, dither) once per audio block.
    masterBusChain.setLimiterEnabled(
        masterBusLimiterEnabled.load(std::memory_order_relaxed));
    masterBusChain.setLimiterCeilingDb(static_cast<double>(
        masterBusLimiterCeilingDb.load(std::memory_order_relaxed)));
    masterBusChain.setEqEnabled(
        masterBusEqEnabled.load(std::memory_order_relaxed));
    masterBusChain.setEqFrequencyHz(static_cast<double>(
        masterBusEqFrequencyHz.load(std::memory_order_relaxed)));
    masterBusChain.setEqGainDb(
        static_cast<double>(masterBusEqGainDb.load(std::memory_order_relaxed)));
    masterBusChain.setEqQ(
        static_cast<double>(masterBusEqQ.load(std::memory_order_relaxed)));
    masterBusChain.setThreeBandGainsDb(
        static_cast<double>(
            masterBusEqLowGainDb.load(std::memory_order_relaxed)),
        static_cast<double>(
            masterBusEqMidGainDb.load(std::memory_order_relaxed)),
        static_cast<double>(
            masterBusEqHighGainDb.load(std::memory_order_relaxed)));
    masterBusChain.setDitherEnabled(
        masterBusDitherEnabled.load(std::memory_order_relaxed));
    monitorEq.prepare(sr);
    const bool monEqBypassBlock =
        monitorEqBypass.load(std::memory_order_relaxed);
    const float fxMonGainBlock = nova::graviton::dbToGain(
        fxToMonitorDb.load(std::memory_order_relaxed));
    const float monDelMsBlock = monitorDelayMs.load(std::memory_order_relaxed);

    std::array<double, 12> sampleSurroundOutputs{};
    std::array<double, kAuxBusCapacity> auxSums{};
    std::array<double, kLogicalChannels> channelDirectOutSamples{};
    std::array<double, kLogicalChannels> externalSidechainSamples{};
    std::array<bool, kLogicalChannels> externalSidechainAvailable{};
    std::array<double, kAuxBusCapacity> masteredAuxDoubleSamples{};
    std::array<float, kAuxBusCapacity> masteredAuxSamples{};
    std::array<double, kMatrixBusCapacity> matrixRawOutputs{};
    std::array<float, kMatrixBusCapacity> matrixSamples{};

    for (int s = 0; s < numSamples; ++s) {
      // Graviton Mk 2: accumulate the stereo Main bus in double precision.
      // Channel DSP and device buffers remain float for this migration step.
      double left = 0.0, right = 0.0;
      double monMixL = 0.0, monMixR = 0.0;
      double soloSumL_A = 0.0, soloSumR_A = 0.0;
      double soloSumL_B = 0.0, soloSumR_B = 0.0;
      auxSums.fill(0.0);
      if (anySurroundActive)
        sampleSurroundOutputs.fill(0.0);
      if (anyExternalSidechainTarget)
        externalSidechainAvailable.fill(false);

      // External keys tap the source channel before its channel processing.
      // This keeps key routing independent of channel order and avoids any
      // allocation or synchronization on the audio thread.
      if (anyExternalSidechainTarget) {
        for (int target = 0; target < kLogicalChannels; ++target) {
          const auto &targetState = channels[static_cast<size_t>(target)];
          if (targetState.sidechainSourceType.load(std::memory_order_relaxed) !=
              static_cast<int>(nova::graviton::KeySourceType::External)) {
            continue;
          }
          const int sourceChannel =
              std::clamp(targetState.sidechainExternalChannel.load(
                             std::memory_order_relaxed),
                         0, kLogicalChannels - 1);
          const auto sourceIndex = static_cast<size_t>(sourceChannel);
          if (externalSidechainAvailable[sourceIndex])
            continue;

          const auto &sourceState = channels[sourceIndex];
          const int callbackInput = channelCallbackSource[sourceIndex];
          if (!channelInputAvailable[sourceIndex])
            continue;

          const double rawSample =
              (vscBlockActive && sourceChannel < 64)
                  ? dawStreamer.pullSample(sourceChannel, s)
                  : ((callbackInput >= 0 && callbackInput < numInputs &&
                      input[callbackInput] != nullptr)
                         ? static_cast<double>(input[callbackInput][s])
                         : 0.0);
          double keySample = nova::graviton::sanitize(rawSample) *
                             inputTrimLinearGain[sourceIndex];
          if (sourceState.polarityInverted.load(std::memory_order_relaxed))
            keySample = -keySample;
          externalSidechainSamples[sourceIndex] =
              nova::graviton::sanitize(keySample);
          externalSidechainAvailable[sourceIndex] = true;
        }
      }

      for (int c = 0; c < kLogicalChannels; ++c) {
        const auto &state = channels[static_cast<size_t>(c)];
        const auto channelIndex = static_cast<size_t>(c);
        if (channelGatedBlock[channelIndex])
          continue;

        const int source = channelCallbackSource[channelIndex];
        const bool inputAvailable = channelInputAvailable[channelIndex];
        if (!inputAvailable) {
          if (state.sidechainKeyListen.load(std::memory_order_relaxed)) {
            double externalKeySample = 0.0;
            const int keyChannel = std::clamp(
                state.sidechainExternalChannel.load(std::memory_order_relaxed),
                0, kLogicalChannels - 1);
            const auto keyIndex = static_cast<size_t>(keyChannel);
            if (state.sidechainSourceType.load(std::memory_order_relaxed) ==
                    static_cast<int>(nova::graviton::KeySourceType::External) &&
                externalSidechainAvailable[keyIndex]) {
              externalKeySample = externalSidechainSamples[keyIndex];
            }
            const auto keySourceType =
                state.sidechainSourceType.load(std::memory_order_relaxed) ==
                        static_cast<int>(
                            nova::graviton::KeySourceType::External)
                    ? nova::graviton::KeySourceType::External
                    : nova::graviton::KeySourceType::Internal;
            const double keySample = nova::graviton::processSidechainKeySample(
                sidechainKeyFilter[channelIndex], keySourceType, 0.0,
                externalKeySample,
                static_cast<nova::graviton::KeyFilterType>(std::clamp(
                    state.sidechainFilterType.load(std::memory_order_relaxed),
                    0, 4)),
                static_cast<double>(state.sidechainFilterFreqHz.load(
                    std::memory_order_relaxed)),
                static_cast<double>(
                    state.sidechainFilterQ.load(std::memory_order_relaxed)));
            const double listenSample = keySample * 0.7071067811865475;
            if (state.soloBus.load(std::memory_order_relaxed) == 1) {
              soloSumL_B += listenSample;
              soloSumR_B += listenSample;
            } else {
              soloSumL_A += listenSample;
              soloSumR_A += listenSample;
            }
          }
          continue;
        }

        const int vcSrc = channelVirtualCableSource[channelIndex];
        float inputSample = 0.0f;
        if (vcSrc >= 0 && s < vcBlockSamples) {
          const size_t vcIdx = static_cast<size_t>(vcSrc / 2);
          inputSample = (vcSrc % 2 == 0) ? virtualReturnBufL[vcIdx][s]
                                         : virtualReturnBufR[vcIdx][s];
        } else {
          inputSample =
              (vscBlockActive && c < 64)
                  ? static_cast<float>(dawStreamer.pullSample(c, s))
                  : ((source >= 0 && source < numInputs &&
                      input[source] != nullptr)
                         ? input[source][s]
                         : 0.0f);
        }
        if (dawStreamer.isStreaming() && c < 64 && source >= 0 &&
            source < numInputs && input[source] != nullptr) {
          dawStreamer.pushSample(c, s, static_cast<double>(input[source][s]));
        }
        if (c < 64 && s < rawMultitrackRecordBuffer.getNumSamples()) {
          rawMultitrackRecordBuffer.setSample(c, s, inputSample);
        }
        // Channel AI: feed sample to profiler if actively capturing this
        // channel
        channelAI.pushSample(c, inputSample);
        // Device input is float, but all internal preamp/HPF/EQ arithmetic is
        // double.
        inputSample = nova::sanitizeSample(inputSample);
        blockInputPeaks[channelIndex] =
            std::max(blockInputPeaks[channelIndex], std::abs(inputSample));
        double preEqSignal =
            static_cast<double>(inputSample) *
            static_cast<double>(inputTrimLinearGain[channelIndex]);
        if (state.polarityInverted.load(std::memory_order_relaxed))
          preEqSignal = -preEqSignal;
        preEqSignal = nova::graviton::sanitize(preEqSignal);

        const float delayMs =
            state.inputDelayMs.load(std::memory_order_relaxed);
        if (delayMs > 0.001f) {
          const int delaySamples =
              std::clamp(static_cast<int>(std::round(
                             static_cast<double>(delayMs) * 0.001 * sr)),
                         0, kMaxDelaySamples - 1);
          auto &delayBuf = channelDelayBuffers[channelIndex];
          int &wPos = channelDelayWritePos[channelIndex];
          delayBuf[static_cast<size_t>(wPos)] = preEqSignal;
          const int rPos =
              (wPos - delaySamples + kMaxDelaySamples) % kMaxDelaySamples;
          preEqSignal = delayBuf[static_cast<size_t>(rPos)];
          wPos = (wPos + 1) % kMaxDelaySamples;
        }

        blockTrimmedPeaks[channelIndex] =
            std::max(blockTrimmedPeaks[channelIndex],
                     std::abs(nova::graviton::toFloatBoundary(preEqSignal)));
        auto &flt = filterState[channelIndex];
        if (state.highPassEnabled.load(std::memory_order_relaxed)) {
          preEqSignal = flt.hpf1.process(preEqSignal);
          if (flt.cachedHpfSlope == 1)
            preEqSignal = flt.hpf2.process(preEqSignal);
          preEqSignal = nova::graviton::sanitize(preEqSignal);
        }
        if (state.lowPassEnabled.load(std::memory_order_relaxed)) {
          preEqSignal = flt.lpf1.process(preEqSignal);
          if (flt.cachedLpfSlope == 1)
            preEqSignal = flt.lpf2.process(preEqSignal);
          preEqSignal = nova::graviton::sanitize(preEqSignal);
        }
        if (state.allPassEnabled.load(std::memory_order_relaxed)) {
          const double deg = static_cast<double>(
              state.allPassDegrees.load(std::memory_order_relaxed));
          preEqSignal = allPassFilters[channelIndex].process(preEqSignal, deg);
          preEqSignal = nova::graviton::sanitize(preEqSignal);
        }

        // Virtual FX Rack Channel Insert point (MIDAS HD96-24 Chapter 15)
        double insertDummyR = preEqSignal;
        effectsRack.processChannelInsert(c, preEqSignal, insertDummyR);

        double channelSignal = preEqSignal;
        const int sidechainSourceType =
            state.sidechainSourceType.load(std::memory_order_relaxed);
        const int sidechainFilterType = std::clamp(
            state.sidechainFilterType.load(std::memory_order_relaxed), 0, 4);
        double externalKeySample = 0.0;
        if (sidechainSourceType ==
            static_cast<int>(nova::graviton::KeySourceType::External)) {
          const int keyChannel = std::clamp(
              state.sidechainExternalChannel.load(std::memory_order_relaxed), 0,
              kLogicalChannels - 1);
          const auto keyIndex = static_cast<size_t>(keyChannel);
          if (externalSidechainAvailable[keyIndex])
            externalKeySample = externalSidechainSamples[keyIndex];
        }
        const auto keySourceType =
            sidechainSourceType ==
                    static_cast<int>(nova::graviton::KeySourceType::External)
                ? nova::graviton::KeySourceType::External
                : nova::graviton::KeySourceType::Internal;
        const auto filterSidechainKey = [&](double internalKeySignal) noexcept {
          return nova::graviton::processSidechainKeySample(
              sidechainKeyFilter[channelIndex], keySourceType,
              internalKeySignal, externalKeySample,
              static_cast<nova::graviton::KeyFilterType>(sidechainFilterType),
              static_cast<double>(
                  state.sidechainFilterFreqHz.load(std::memory_order_relaxed)),
              static_cast<double>(
                  state.sidechainFilterQ.load(std::memory_order_relaxed)));
        };
        double filteredSidechainKey = 0.0;
        const auto processEqStage = [&](double sig) noexcept -> double {
          const bool eqOn = state.eqEnabled.load(std::memory_order_relaxed);
          const bool elBypassOn =
              state.dynEqElBypass.load(std::memory_order_relaxed);
          if (!eqOn && !elBypassOn)
            return nova::graviton::sanitize(sig);
          bool anyDyn = false;
          for (int b = 0; b < 4; ++b) {
            if (state.dynEqEnabled[static_cast<size_t>(b)].load(
                    std::memory_order_relaxed)) {
              anyDyn = true;
              break;
            }
          }
          const bool wBandOn =
              state.dynEqWBandEnabled.load(std::memory_order_relaxed);
          const bool grDeltaOn =
              state.dynEqGrDelta.load(std::memory_order_relaxed);
          const int soloBand =
              state.dynEqSoloBand.load(std::memory_order_relaxed);
          const bool novaFeaturesActive =
              anyDyn || wBandOn || elBypassOn || grDeltaOn || soloBand >= 0 ||
              state.dynEqHpfEnabled.load(std::memory_order_relaxed) ||
              state.dynEqLpfEnabled.load(std::memory_order_relaxed) ||
              state.dynEqDryMixPct.load(std::memory_order_relaxed) > 0.01f ||
              std::abs(state.dynEqOutGainDb.load(std::memory_order_relaxed)) >
                  0.01f ||
              std::abs(state.dynEqEqGainDb.load(std::memory_order_relaxed)) >
                  0.01f;

          if (novaFeaturesActive &&
              state.dynEqParallelMode.load(std::memory_order_relaxed)) {
            nova::graviton::NovaParallelParams np{};
            for (size_t b = 0; b < 4; ++b) {
              np.freqs[b] = static_cast<double>(
                  state.eqFrequency[b].load(std::memory_order_relaxed));
              np.qs[b] = static_cast<double>(
                  state.eqQ[b].load(std::memory_order_relaxed));
              np.staticGainsDb[b] = static_cast<double>(
                  state.eqGainDb[b].load(std::memory_order_relaxed));
              np.shapes[b] =
                  static_cast<nova::graviton::DynamicEqBandShape>(std::clamp(
                      state.dynEqShape[b].load(std::memory_order_relaxed), 0,
                      2));
              np.bandActive[b] = true;
              np.dynEnables[b] =
                  state.dynEqEnabled[b].load(std::memory_order_relaxed);
              np.splitIndependent[b] =
                  state.dynEqSplit[b].load(std::memory_order_relaxed);
              np.modes[b] = static_cast<nova::graviton::DynamicEqMode>(
                  state.dynEqMode[b].load(std::memory_order_relaxed));
              np.thresholdsDb[b] = static_cast<double>(
                  state.dynEqThresholdDb[b].load(std::memory_order_relaxed));
              np.rangesDb[b] = static_cast<double>(
                  state.dynEqRangeDb[b].load(std::memory_order_relaxed));
              np.ratios[b] = static_cast<double>(
                  state.dynEqRatio[b].load(std::memory_order_relaxed));
              np.kneesDb[b] = static_cast<double>(
                  state.dynEqKneeDb[b].load(std::memory_order_relaxed));
              np.attacksMs[b] = static_cast<double>(
                  state.dynEqAttackMs[b].load(std::memory_order_relaxed));
              np.releasesMs[b] = static_cast<double>(
                  state.dynEqReleaseMs[b].load(std::memory_order_relaxed));
              np.stereoModes[b] =
                  static_cast<nova::graviton::DynamicEqStereoMode>(std::clamp(
                      state.dynEqStereoMode[b].load(std::memory_order_relaxed),
                      0, 5));
            }
            np.hpfEnabled =
                state.dynEqHpfEnabled.load(std::memory_order_relaxed);
            np.hpfFreqHz = static_cast<double>(
                state.dynEqHpfFreqHz.load(std::memory_order_relaxed));
            np.hpfSlope = state.dynEqHpfSlope.load(std::memory_order_relaxed);
            np.lpfEnabled =
                state.dynEqLpfEnabled.load(std::memory_order_relaxed);
            np.lpfFreqHz = static_cast<double>(
                state.dynEqLpfFreqHz.load(std::memory_order_relaxed));
            np.lpfSlope = state.dynEqLpfSlope.load(std::memory_order_relaxed);

            np.wBandEnabled = wBandOn;
            np.wBandMode = static_cast<nova::graviton::DynamicEqMode>(
                state.dynEqWBandMode.load(std::memory_order_relaxed));
            np.wBandThresholdDb = static_cast<double>(
                state.dynEqWBandThresholdDb.load(std::memory_order_relaxed));
            np.wBandRatio = static_cast<double>(
                state.dynEqWBandRatio.load(std::memory_order_relaxed));
            np.wBandRangeDb = static_cast<double>(
                state.dynEqWBandRangeDb.load(std::memory_order_relaxed));
            np.wBandKneeDb = static_cast<double>(
                state.dynEqWBandKneeDb.load(std::memory_order_relaxed));
            np.wBandAttackMs = static_cast<double>(
                state.dynEqWBandAttackMs.load(std::memory_order_relaxed));
            np.wBandReleaseMs = static_cast<double>(
                state.dynEqWBandReleaseMs.load(std::memory_order_relaxed));
            np.wBandGainDb = static_cast<double>(
                state.dynEqWBandGainDb.load(std::memory_order_relaxed));

            np.bypassed = !eqOn;
            np.elBypass = elBypassOn;
            np.extSidechain =
                state.dynEqExtSc.load(std::memory_order_relaxed) ||
                (sidechainSourceType ==
                 static_cast<int>(nova::graviton::KeySourceType::External));
            np.grDelta = grDeltaOn;
            np.soloBandIndex = soloBand;
            np.eqGainDb = static_cast<double>(
                state.dynEqEqGainDb.load(std::memory_order_relaxed));
            np.dryMix = static_cast<double>(state.dynEqDryMixPct.load(
                            std::memory_order_relaxed)) *
                        0.01;
            np.outGainDb = static_cast<double>(
                state.dynEqOutGainDb.load(std::memory_order_relaxed));

            sig = dynamicEqEngine[channelIndex].processParallelSample(
                sig, externalKeySample, np);
          } else if (anyDyn) {
            std::array<double, 4> freqs{}, qs{}, staticGains{}, threshs{},
                ranges{}, ratios{}, knees{}, atts{}, rels{};
            std::array<bool, 4> dynEnables{};
            std::array<nova::graviton::DynamicEqMode, 4> modes{};
            for (size_t b = 0; b < 4; ++b) {
              freqs[b] = static_cast<double>(
                  state.eqFrequency[b].load(std::memory_order_relaxed));
              qs[b] = static_cast<double>(
                  state.eqQ[b].load(std::memory_order_relaxed));
              staticGains[b] = static_cast<double>(
                  state.eqGainDb[b].load(std::memory_order_relaxed));
              dynEnables[b] =
                  state.dynEqEnabled[b].load(std::memory_order_relaxed);
              modes[b] = static_cast<nova::graviton::DynamicEqMode>(
                  state.dynEqMode[b].load(std::memory_order_relaxed));
              threshs[b] = static_cast<double>(
                  state.dynEqThresholdDb[b].load(std::memory_order_relaxed));
              ranges[b] = static_cast<double>(
                  state.dynEqRangeDb[b].load(std::memory_order_relaxed));
              ratios[b] = static_cast<double>(
                  state.dynEqRatio[b].load(std::memory_order_relaxed));
              knees[b] = static_cast<double>(
                  state.dynEqKneeDb[b].load(std::memory_order_relaxed));
              atts[b] = static_cast<double>(
                  state.dynEqAttackMs[b].load(std::memory_order_relaxed));
              rels[b] = static_cast<double>(
                  state.dynEqReleaseMs[b].load(std::memory_order_relaxed));
            }
            sig = dynamicEqEngine[channelIndex].processSample(
                sig, freqs, qs, staticGains, dynEnables, modes, threshs, ranges,
                ratios, knees, atts, rels);
          } else {
            for (int band = 0; band < 4; ++band) {
              auto &filter = eqState[channelIndex][static_cast<size_t>(band)];
              sig = filter.filter.process(sig);
            }
          }
          return nova::graviton::sanitize(sig);
        };

        if (state.postEqDynamics.load(std::memory_order_relaxed)) {
          // Standard topology: HPF -> 4-band PEQ -> Dynamics (Gate & Comp)
          channelSignal = processEqStage(channelSignal);
          filteredSidechainKey = filterSidechainKey(channelSignal);

          auto dynamicsResult =
              dynamicsProcessor[channelIndex].processWithSidechain(
                  channelSignal, filteredSidechainKey,
                  dynamicsSettingsBlock[channelIndex]);
          blockGateReduction[channelIndex] =
              std::max(blockGateReduction[channelIndex],
                       static_cast<float>(dynamicsResult.gateReductionDb));
          blockCompressorGR[channelIndex] = std::max(
              blockCompressorGR[channelIndex],
              static_cast<float>(dynamicsResult.compressorReductionDb));
          channels[channelIndex].dynamicsInDb.store(
              static_cast<float>(dynamicsResult.inLevelDb),
              std::memory_order_relaxed);
          channels[channelIndex].dynamicsSideDb.store(
              static_cast<float>(dynamicsResult.sideLevelDb),
              std::memory_order_relaxed);
          channels[channelIndex].dynamicsOutDb.store(
              static_cast<float>(dynamicsResult.outLevelDb),
              std::memory_order_relaxed);
          channelSignal = nova::graviton::sanitize(dynamicsResult.sample);
        } else {
          // Dynamics Pre-EQ topology: HPF -> Dynamics (Gate & Comp) -> 4-band
          // PEQ
          filteredSidechainKey = filterSidechainKey(channelSignal);
          auto dynamicsResult =
              dynamicsProcessor[channelIndex].processWithSidechain(
                  channelSignal, filteredSidechainKey,
                  dynamicsSettingsBlock[channelIndex]);
          blockGateReduction[channelIndex] =
              std::max(blockGateReduction[channelIndex],
                       static_cast<float>(dynamicsResult.gateReductionDb));
          blockCompressorGR[channelIndex] = std::max(
              blockCompressorGR[channelIndex],
              static_cast<float>(dynamicsResult.compressorReductionDb));
          channels[channelIndex].dynamicsInDb.store(
              static_cast<float>(dynamicsResult.inLevelDb),
              std::memory_order_relaxed);
          channels[channelIndex].dynamicsSideDb.store(
              static_cast<float>(dynamicsResult.sideLevelDb),
              std::memory_order_relaxed);
          channels[channelIndex].dynamicsOutDb.store(
              static_cast<float>(dynamicsResult.outLevelDb),
              std::memory_order_relaxed);
          channelSignal = nova::graviton::sanitize(dynamicsResult.sample);

          channelSignal = processEqStage(channelSignal);
        }

        const int dGroup = state.duganGroup.load(std::memory_order_relaxed);
        if (dGroup > 0) {
          channelSignal =
              duganAutomixer.processSample(channelIndex, channelSignal, dGroup);
          channelSignal = nova::graviton::sanitize(channelSignal);
        }

        const double targetGain = channelTargetGainBlock[channelIndex];
        auto &smoothedGain = channelGainSmooth[channelIndex];
        smoothedGain = nova::graviton::smoothGainStep(
            smoothedGain, targetGain, static_cast<double>(gainSmoothingCoeff));
        const double g = std::isfinite(smoothedGain) ? smoothedGain : 0.0;
        const double sample64 = channelSignal * g;
        // AUX send source, gain multiplication, and bus accumulation remain
        // double precision; narrowing is deferred until the router boundary.
        const double dry = channelSignal;
        const double drySan = nova::graviton::sanitize(dry);
        const double drySendSan = (vcaMutedChannelBlock[channelIndex] ||
                                   channelGatedBlock[channelIndex])
                                      ? 0.0
                                      : drySan;
        const double postSan = nova::graviton::sanitize(sample64);
        channelDirectOutSamples[channelIndex] = postSan;
        // Phase 65: AUX send accumulation executes through the immutable safe
        // bus graph plan activated at this audio block boundary.
        // Phase 90: Independent Master and Monitor channel routing
        if (state.masterAssigned.load(std::memory_order_relaxed)) {
          left = nova::graviton::sanitize(nova::graviton::sanitize(left) +
                                          postSan *
                                              panGainLeftBlock[channelIndex]);
          right = nova::graviton::sanitize(nova::graviton::sanitize(right) +
                                           postSan *
                                               panGainRightBlock[channelIndex]);
        }

        if (monitorAssignedBlock[channelIndex]) {
          const double monSendGain = monitorSendGainBlock[channelIndex];
          if (monSendGain > 0.0) {
            const double monSource =
                monitorPreFaderBlock[channelIndex] ? drySendSan : postSan;
            monMixL = nova::graviton::sanitize(
                monMixL +
                monSource * monSendGain * panGainLeftBlock[channelIndex]);
            monMixR = nova::graviton::sanitize(
                monMixR +
                monSource * monSendGain * panGainRightBlock[channelIndex]);
          }
        }
        if (busPlan.routeCount > 0) {
          if (busRoutesContiguous) {
            const std::size_t rStart = chBusRouteStart[channelIndex];
            const std::size_t rEnd = rStart + chBusRouteCount[channelIndex];
            for (std::size_t r = rStart; r < rEnd; ++r) {
              const auto &route = busPlan.routes[r];
              const std::size_t sendAuxIndex =
                  static_cast<std::size_t>(route.aux);
              if (sendAuxIndex >= auxSums.size() ||
                  static_cast<std::size_t>(route.channel) >=
                      static_cast<std::size_t>(nova::kBusGraphChannels)) {
                continue;
              }
              const double sendSource = route.preFader ? drySendSan : postSan;
              nova::graviton::accumulateBus(auxSums[sendAuxIndex], sendSource,
                                            route.gain);
            }
          } else {
            for (std::size_t r = 0; r < busPlan.routeCount; ++r) {
              const auto &route = busPlan.routes[r];
              if (static_cast<int>(route.channel) != c)
                continue;
              const std::size_t sendAuxIndex =
                  static_cast<std::size_t>(route.aux);
              if (sendAuxIndex >= auxSums.size() ||
                  static_cast<std::size_t>(route.channel) >=
                      static_cast<std::size_t>(nova::kBusGraphChannels)) {
                continue;
              }
              const double sendSource = route.preFader ? drySendSan : postSan;
              nova::graviton::accumulateBus(auxSums[sendAuxIndex], sendSource,
                                            route.gain);
            }
          }
        }
        // ScalableBusGraph plan covers up to kBusGraphChannels and
        // kBusGraphAuxCount. For remaining channels or auxes, accumulate from
        // activeAuxRoutingSnapshot sparse route list:
        const auto &chRouting = activeAuxRoutingSnapshot.channels[channelIndex];
        const std::size_t startAux =
            (c < nova::kBusGraphChannels)
                ? static_cast<std::size_t>(nova::kBusGraphAuxCount)
                : 0;
        const std::size_t activeRouteCount =
            std::min(chRouting.routeCount, chRouting.routes.size());
        for (std::size_t r = 0; r < activeRouteCount; ++r) {
          const auto &ar = chRouting.routes[r];
          if (ar.auxIndex < startAux || ar.auxIndex >= kAuxBusCapacity)
            continue;
          if (ar.gain > 1.0e-5) {
            const double sendSource = ar.preFader ? drySendSan : postSan;
            nova::graviton::accumulateBus(auxSums[ar.auxIndex], sendSource,
                                          ar.gain);
          }
        }
        channelBlockPeaks[channelIndex] =
            std::max(channelBlockPeaks[channelIndex],
                     std::abs(nova::graviton::toFloatBoundary(dry)));

        // Phase 85: Multichannel Surround & Immersive Object Accumulation
        // (Chapter 30)
        if (channelSurroundModes[channelIndex] > 0) {
          const auto &sGains = channelSurroundGains[channelIndex];
          for (size_t spk = 0; spk < 12; ++spk) {
            const double spkSig = sample64 * sGains[spk];
            sampleSurroundOutputs[spk] += spkSig;
            blockSurroundPeaks[spk] = std::max(
                blockSurroundPeaks[spk], static_cast<float>(std::abs(spkSig)));
          }
        }

        const bool keyListen =
            state.sidechainKeyListen.load(std::memory_order_relaxed);
        if (state.solo.load(std::memory_order_relaxed) || keyListen) {
          const int targetBus = state.soloBus.load(std::memory_order_relaxed);
          const bool isPfl = state.soloPfl.load(std::memory_order_relaxed);
          double sL = 0.0, sR = 0.0;
          if (keyListen) {
            sL = filteredSidechainKey * 0.7071067811865475;
            sR = sL;
          } else if (isPfl) {
            sL = channelSignal * 0.7071067811865475;
            sR = sL;
          } else {
            sL = sample64 * panGainLeftBlock[channelIndex];
            sR = sample64 * panGainRightBlock[channelIndex];
          }
          if (targetBus == 1) {
            soloSumL_B += sL;
            soloSumR_B += sR;
          } else {
            soloSumL_A += sL;
            soloSumR_A += sR;
          }
        }
      }
      if (s < playbackSamples) {
        left += static_cast<double>(playbackBlock.getSample(0, s));
        right += static_cast<double>(playbackBlock.getSample(1, s));
      }

      // Graviton Mk 2 Phase 70: Real-time Signal Generator injection
      const double oscTargetGain =
          oscEnabled.load(std::memory_order_relaxed)
              ? nova::graviton::dbToGain(
                    std::clamp(static_cast<double>(
                                   oscLevelDb.load(std::memory_order_relaxed)),
                               -60.0, 0.0))
              : 0.0;
      oscGainSmooth = nova::graviton::smoothGainStep(
          oscGainSmooth, oscTargetGain, gainSmoothingCoeff);
      const double oscSample =
          (oscGainSmooth > 1.0e-5)
              ? signalGenerator.renderSample(
                    static_cast<nova::graviton::GeneratorType>(
                        oscType.load(std::memory_order_relaxed)),
                    static_cast<double>(
                        oscFrequency.load(std::memory_order_relaxed)),
                    oscGainSmooth)
              : 0.0;

      if (oscSample != 0.0) {
        if (oscRouteMain.load(std::memory_order_relaxed)) {
          left += oscSample;
          right += oscSample;
        }
        for (int a = 0; a < kAuxBusCapacity; ++a) {
          if (oscRouteAux[static_cast<size_t>(a)].load(
                  std::memory_order_relaxed)) {
            auxSums[static_cast<size_t>(a)] += oscSample;
          }
        }
      }

      // AUX Bus Masters & Virtual FX Rack Aux Send / Return loop (MIDAS HD96-24
      // Chapter 15) run BEFORE the Main L/R Master Fader, 31-Band GEQ, Master
      // Insert, RTA, and Monitor tap so FX returns are properly controlled by
      // the Main L/R bus chain.
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        const auto index = static_cast<size_t>(a);
        auxMasterGainSmooth[index] = nova::graviton::smoothGainStep(
            auxMasterGainSmooth[index], auxMasterTarget[index],
            gainSmoothingCoeff);
        const double auxMaster = std::isfinite(auxMasterGainSmooth[index])
                                     ? auxMasterGainSmooth[index]
                                     : 0.0;
        masteredAuxDoubleSamples[index] = nova::graviton::applyBusMaster(
            auxSums[index], auxMaster, auxMutedBlock[index]);
        if (auxIemLimiterEnabledBlock[index]) {
          masteredAuxDoubleSamples[index] =
              std::clamp(masteredAuxDoubleSamples[index],
                         -auxIemCeilingLinBlock, auxIemCeilingLinBlock);
        }
        auxSums[index] = masteredAuxDoubleSamples[index];
        masteredAuxSamples[index] =
            nova::graviton::toFloatBoundary(masteredAuxDoubleSamples[index]);
        blockAuxPeaks[index] =
            std::max(blockAuxPeaks[index], std::abs(masteredAuxSamples[index]));
      }

      double totalFxReturnL = 0.0, totalFxReturnR = 0.0;
      for (size_t fxIdx = 0; fxIdx < nova::graviton::EffectsRack::kNumSlots;
           ++fxIdx) {
        auto &slot = effectsRack.getSlot(fxIdx);
        const int auxSrc = slot.auxSendSource.load(std::memory_order_relaxed);
        if (auxSrc >= 0 && auxSrc < kAuxBusCapacity &&
            !slot.bypassed.load(std::memory_order_relaxed) &&
            slot.type.load(std::memory_order_relaxed) !=
                static_cast<int>(nova::graviton::FxType::None)) {
          double fxL = auxSums[static_cast<size_t>(auxSrc)];
          if (std::abs(fxL) < 1.0e-9 &&
              slot.outputPeak.load(std::memory_order_relaxed) < 1.0e-6f) {
            continue;
          }
          double fxR = fxL;
          slot.processStereo(fxL, fxR);
          if (!slot.returnMute.load(std::memory_order_relaxed)) {
            const double retGain = nova::graviton::dbToGain(
                std::clamp(static_cast<double>(slot.returnLevelDb.load(
                               std::memory_order_relaxed)),
                           -60.0, 10.0));
            left += fxL * retGain;
            right += fxR * retGain;
            totalFxReturnL += fxL;
            totalFxReturnR += fxR;
          }
        }
      }

      masterGainSmooth = nova::graviton::smoothGainStep(
          masterGainSmooth, masterTarget, gainSmoothingCoeff);
      const double master =
          std::isfinite(masterGainSmooth) ? masterGainSmooth : 0.0;
      left *= master;
      right *= master;
      left = nova::graviton::sanitize(left);
      right = nova::graviton::sanitize(right);
      if (graphicEqEnabled.load(std::memory_order_relaxed)) {
        mainGraphicEQ.processStereo(left, right);
      }
      // Master Bus Insert Point (MIDAS HD96-24 Chapters 15 & 18):
      effectsRack.processMasterInsert(left, right);
      rtaAnalyzer.pushSamplePair(left, right);

      // Dual Monitor Calculation (MIDAS HD96-24 Chapters 11 & 13)
      double monSigL_A = anySoloA ? soloSumL_A : monMixL;
      double monSigR_A = anySoloA ? soloSumR_A : monMixR;
      const double monSigL_B = anySoloB ? soloSumL_B : monMixL;
      const double monSigR_B = anySoloB ? soloSumR_B : monMixR;

      // Independent FX Send / Return to Monitor Bus:
      if (fxMonGainBlock > 0.0001f) {
        monSigL_A += totalFxReturnL * fxMonGainBlock;
        monSigR_A += totalFxReturnR * fxMonGainBlock;
      }
      if (oscSample != 0.0 && oscRouteMonitor.load(std::memory_order_relaxed)) {
        monSigL_A += oscSample;
        monSigR_A += oscSample;
      }

      double finalMonL_A = monSigL_A * effectiveMonGainA;
      double finalMonR_A = monSigR_A * effectiveMonGainA;
      double finalMonL_B = monSigL_B * effectiveMonGainB;
      double finalMonR_B = monSigR_B * effectiveMonGainB;

      // Independent Monitor 4-Band Parametric EQ:
      if (!monEqBypassBlock) {
        monitorEq.process(finalMonL_A, finalMonR_A);
      }
      finalMonL_A = nova::graviton::sanitize(finalMonL_A);
      finalMonR_A = nova::graviton::sanitize(finalMonR_A);

      // Monitor Output Delay Line (Time Alignment)
      if (monDelMsBlock > 0.001f && !monitorDelayBufL.empty()) {
        const int delSamp =
            juce::jlimit(0, kMaxDelaySamples - 1,
                         static_cast<int>(std::round(
                             static_cast<double>(monDelMsBlock) * 0.001 * sr)));
        monitorDelayBufL[static_cast<size_t>(monitorDelayWritePos)] =
            finalMonL_A;
        monitorDelayBufR[static_cast<size_t>(monitorDelayWritePos)] =
            finalMonR_A;
        const int readPos =
            (monitorDelayWritePos - delSamp + kMaxDelaySamples) %
            kMaxDelaySamples;
        finalMonL_A = monitorDelayBufL[static_cast<size_t>(readPos)];
        finalMonR_A = monitorDelayBufR[static_cast<size_t>(readPos)];
        monitorDelayWritePos = (monitorDelayWritePos + 1) % kMaxDelaySamples;
      }
      // Matrix masters are smoothed once per sample. The Matrix calculation
      // itself consumes double Main/AUX samples and only narrows at the
      // existing output boundary.
      for (int m = 0; m < kMatrixBusCapacity; ++m)
        matrixMasterGainSmooth[static_cast<size_t>(m)] =
            nova::graviton::smoothGainStep(
                matrixMasterGainSmooth[static_cast<size_t>(m)],
                matrixMasterTarget[static_cast<size_t>(m)], gainSmoothingCoeff);
      // Graviton Mk 2: Generalized adaptive matrix routing in 64-bit double
      // precision
      nova::graviton::processMatrixBusesAdaptive(matrixRawOutputs, left, right,
                                                 masteredAuxDoubleSamples,
                                                 activeMatrixRoutingSnapshot);
      if (oscSample != 0.0) {
        for (int m = 0; m < kMatrixBusCapacity; ++m) {
          if (oscRouteMatrix[static_cast<size_t>(m)].load(
                  std::memory_order_relaxed)) {
            matrixRawOutputs[static_cast<size_t>(m)] += oscSample;
          }
        }
      }

      // Phase 86: Talkback & Intercom Bus Matrix Injection (MIDAS HD96 Chapter
      // 32)
      double tbDuckerGain = 1.0;
      const double tbMicInput =
          (input != nullptr && numInputs > 0 && input[0] != nullptr)
              ? static_cast<double>(input[0][s])
              : 0.0;
      const double tbSig =
          talkbackEngine.processSample(tbMicInput, sr, tbDuckerGain);

      if (talkbackEngine.isActive() || tbDuckerGain < 0.999) {
        if (talkbackEngine.isMainLRTarget()) {
          left = (left * tbDuckerGain) + tbSig;
          right = (right * tbDuckerGain) + tbSig;
        }
        for (int a = 0; a < kAuxBusCapacity; ++a) {
          if (talkbackEngine.isAuxTarget(static_cast<size_t>(a))) {
            auxSums[static_cast<size_t>(a)] =
                (auxSums[static_cast<size_t>(a)] * tbDuckerGain) + tbSig;
            masteredAuxDoubleSamples[static_cast<size_t>(a)] =
                (masteredAuxDoubleSamples[static_cast<size_t>(a)] *
                 tbDuckerGain) +
                tbSig;
            masteredAuxSamples[static_cast<size_t>(a)] =
                nova::graviton::toFloatBoundary(
                    masteredAuxDoubleSamples[static_cast<size_t>(a)]);
          }
        }
        for (int m = 0; m < kMatrixBusCapacity; ++m) {
          if (talkbackEngine.isMatrixTarget(static_cast<size_t>(m))) {
            matrixRawOutputs[static_cast<size_t>(m)] =
                (matrixRawOutputs[static_cast<size_t>(m)] * tbDuckerGain) +
                tbSig;
          }
        }
        if (talkbackEngine.isSoloATarget()) {
          finalMonL_A = (finalMonL_A * tbDuckerGain) + tbSig;
          finalMonR_A = (finalMonR_A * tbDuckerGain) + tbSig;
        }
        if (talkbackEngine.isSoloBTarget()) {
          finalMonL_B = (finalMonL_B * tbDuckerGain) + tbSig;
          finalMonR_B = (finalMonR_B * tbDuckerGain) + tbSig;
        }
      }

      // Phase 66: final master bus chain (true-peak limiter, master EQ,
      // dither).
      masterBusChain.processStereo(left, right);

      // Master Output Delay Line (Time Alignment / Bluetooth Latency
      // Compensation)
      const float mstDelMs = masterDelayMs.load(std::memory_order_relaxed);
      if (mstDelMs > 0.001f && !masterDelayBufL.empty()) {
        const int delSamp =
            juce::jlimit(0, kMaxDelaySamples - 1,
                         static_cast<int>(std::round(
                             static_cast<double>(mstDelMs) * 0.001 * sr)));
        masterDelayBufL[static_cast<size_t>(masterDelayWritePos)] = left;
        masterDelayBufR[static_cast<size_t>(masterDelayWritePos)] = right;
        const int readPos = (masterDelayWritePos - delSamp + kMaxDelaySamples) %
                            kMaxDelaySamples;
        left = masterDelayBufL[static_cast<size_t>(readPos)];
        right = masterDelayBufR[static_cast<size_t>(readPos)];
        masterDelayWritePos = (masterDelayWritePos + 1) % kMaxDelaySamples;
      }

      // Phase 88: Matrix Plus & Speaker Delay Alignment Network (MIDAS Chapter
      // 29)
      matrixPlusEngine.processMatrixFrame(matrixRawOutputs);

      for (int m = 0; m < kMatrixBusCapacity; ++m) {
        const auto index = static_cast<size_t>(m);
        const double matrixGain = std::isfinite(matrixMasterGainSmooth[index])
                                      ? matrixMasterGainSmooth[index]
                                      : 0.0;
        matrixSamples[index] =
            matrixMutedBlock[index]
                ? 0.0f
                : nova::graviton::toFloatBoundary(matrixRawOutputs[index] *
                                                  matrixGain);
        blockMatrixPeaks[index] =
            std::max(blockMatrixPeaks[index], std::abs(matrixSamples[index]));
      }
      const float matrix1 = matrixSamples[0];
      const float matrix2 = matrixSamples[1];

      // Realistic Virtual 3D Surround Stereo Downmix to L/R (Woodworth ITD +
      // HRTF Head-Shadow) — skipped completely when no active channel uses 3D
      // surround.
      if (anySurroundActive) {
        double binSurroundL = 0.0, binSurroundR = 0.0;
        binauralPreviewSpatializer.processFrame(sampleSurroundOutputs, sr,
                                                binSurroundL, binSurroundR);
        left = nova::graviton::sanitize(left + binSurroundL);
        right = nova::graviton::sanitize(right + binSurroundR);
        finalMonL_A = nova::graviton::sanitize(finalMonL_A + binSurroundL);
        finalMonR_A = nova::graviton::sanitize(finalMonR_A + binSurroundR);
      }

      finalMonL_A = std::clamp(nova::graviton::sanitize(finalMonL_A), -1.0, 1.0);
      finalMonR_A = std::clamp(nova::graviton::sanitize(finalMonR_A), -1.0, 1.0);
      finalMonL_B = std::clamp(nova::graviton::sanitize(finalMonL_B), -1.0, 1.0);
      finalMonR_B = std::clamp(nova::graviton::sanitize(finalMonR_B), -1.0, 1.0);

      if (static_cast<size_t>(s) < blockMonBufL.size()) {
        blockMonBufL[static_cast<size_t>(s)] =
            nova::graviton::toFloatBoundary(finalMonL_A);
        blockMonBufR[static_cast<size_t>(s)] =
            nova::graviton::toFloatBoundary(finalMonR_A);
      }

      blockMonPeakL_A =
          std::max(blockMonPeakL_A, static_cast<float>(std::abs(finalMonL_A)));
      blockMonPeakR_A =
          std::max(blockMonPeakR_A, static_cast<float>(std::abs(finalMonR_A)));
      blockMonPeakL_B =
          std::max(blockMonPeakL_B, static_cast<float>(std::abs(finalMonL_B)));
      blockMonPeakR_B =
          std::max(blockMonPeakR_B, static_cast<float>(std::abs(finalMonR_B)));

      // Device callback, router, recorder and meters remain float-based.
      const float mainOutL = nova::graviton::toFloatBoundary(left);
      const float mainOutR = nova::graviton::toFloatBoundary(right);
      // Phase 89: copy the completed Master and bus stem samples for this frame
      // while an offline bounce owns the engine (Chapter 36).
      if (offlineCapturing) {
        auto &capture = offlineCapture;
        if (s < capture.numSamples) {
          if (capture.masterL != nullptr)
            capture.masterL[s] = mainOutL;
          if (capture.masterR != nullptr)
            capture.masterR[s] = mainOutR;
          for (int a = 0; a < kVisibleAuxSends; ++a) {
            auto *stem = capture.stems[static_cast<size_t>(a)];
            if (stem != nullptr)
              stem[s] = masteredAuxSamples[static_cast<size_t>(a)];
          }
          for (int m = 0; m < kVisibleMatrixBuses; ++m) {
            auto *stem =
                capture.stems[static_cast<size_t>(kVisibleAuxSends + m)];
            if (stem != nullptr)
              stem[s] = (m == 0) ? matrix1 : matrix2;
          }
        }
      }
      // Shared with tests: route the actual engine's completed bus samples to
      // physical outputs.
      nova::routeBusOutputSample(
          output, numOutputs, outputPhysicalToCallback,
          outputCount.load(std::memory_order_relaxed), s, numSamples, mainOutL,
          mainOutR, masteredAuxSamples, matrixSamples,
          static_cast<float>(finalMonL_A), static_cast<float>(finalMonR_A),
          dualOutput4ChMode.load(std::memory_order_relaxed));
      // Output Delivery to Physical/Virtual Endpoints
      if (anyStageBoxOutputPatched || s == numSamples - 1) {
        stageBoxPatchMatrix.routeOutputs(
            stageBoxOutputSamples, left, right, 0.5 * (left + right),
            masteredAuxDoubleSamples, matrixRawOutputs, finalMonL_A,
            finalMonR_A, &sampleSurroundOutputs, &channelDirectOutSamples);
        if (anyStageBoxOutputPatched) {
          const size_t maxHardwareOuts =
              std::min(nova::graviton::StageBoxPatchMatrix::kTotalOutputPorts,
                       outputPhysicalToCallback.size());
          for (size_t p = 0; p < maxHardwareOuts; ++p) {
            nova::graviton::OutputBusSourceType outType;
            int outIdx = 0;
            stageBoxPatchMatrix.getOutputPatch(static_cast<int>(p), outType,
                                               outIdx);
            if (outType != nova::graviton::OutputBusSourceType::Unassigned) {
              (void)nova::writeMappedOutputSample(
                  output, numOutputs, outputPhysicalToCallback,
                  static_cast<int>(p), s, numSamples,
                  nova::graviton::toFloatBoundary(stageBoxOutputSamples[p]));
            }
          }
        }
      }
      if (s < mainRecordBuffer.getNumSamples()) {
        mainRecordBuffer.setSample(0, s, mainOutL);
        mainRecordBuffer.setSample(1, s, mainOutR);
      }
      peakL = std::max(peakL, std::abs(mainOutL));
      peakR = std::max(peakR, std::abs(mainOutR));

      if (s < vcBlockSamples) {
        for (size_t k = 0;
             k < nova::graviton::VirtualCableTransport::kNumVirtualCables;
             ++k) {
          const auto &vc = virtualTransport.cables[k];
          if (!vc.sendEnabled.load(std::memory_order_relaxed))
            continue;
          const auto srcType = static_cast<nova::graviton::OutputBusSourceType>(
              vc.sendSourceType.load(std::memory_order_relaxed));
          const int srcIdx = vc.sendSourceIndex.load(std::memory_order_relaxed);
          float sL = 0.0f;
          float sR = 0.0f;
          if (srcType == nova::graviton::OutputBusSourceType::MainLeft ||
              srcType == nova::graviton::OutputBusSourceType::MainRight) {
            sL = mainOutL;
            sR = mainOutR;
          } else if (srcType == nova::graviton::OutputBusSourceType::Mono) {
            sL = sR = 0.5f * (mainOutL + mainOutR);
          } else if (srcType == nova::graviton::OutputBusSourceType::AuxBus) {
            if (srcIdx >= 0 && srcIdx < kAuxBusCapacity) {
              sL = masteredAuxSamples[static_cast<size_t>(srcIdx)];
              sR = (srcIdx + 1 < kAuxBusCapacity)
                       ? masteredAuxSamples[static_cast<size_t>(srcIdx + 1)]
                       : sL;
            }
          } else if (srcType ==
                     nova::graviton::OutputBusSourceType::MatrixBus) {
            if (srcIdx >= 0 && srcIdx < kMatrixBusCapacity) {
              sL = matrixSamples[static_cast<size_t>(srcIdx)];
              sR = (srcIdx + 1 < kMatrixBusCapacity)
                       ? matrixSamples[static_cast<size_t>(srcIdx + 1)]
                       : sL;
            }
          } else if (srcType ==
                     nova::graviton::OutputBusSourceType::DirectOut) {
            if (srcIdx >= 0 && srcIdx < kLogicalChannels) {
              sL = sR = nova::graviton::toFloatBoundary(
                  channelDirectOutSamples[static_cast<size_t>(srcIdx)]);
            }
          } else if (srcType ==
                         nova::graviton::OutputBusSourceType::MonitorLeft ||
                     srcType ==
                         nova::graviton::OutputBusSourceType::MonitorRight) {
            sL = static_cast<float>(finalMonL_A);
            sR = static_cast<float>(finalMonR_A);
          }
          virtualSendBufL[k][static_cast<size_t>(s)] = sL;
          virtualSendBufR[k][static_cast<size_t>(s)] = sR;
        }
      }
    }
    for (size_t k = 0;
         k < nova::graviton::VirtualCableTransport::kNumVirtualCables; ++k) {
      if (virtualTransport.cables[k].sendEnabled.load(
              std::memory_order_relaxed)) {
        virtualTransport.cables[k].pushSendBlock(
            virtualSendBufL[k].data(), virtualSendBufR[k].data(),
            vcBlockSamples);
      }
    }
    pumpVirtualCableShmIfActive();
    if (recorder.isMultitrackActive()) {
      if (numSamples <= rawMultitrackRecordBuffer.getNumSamples())
        recorder.pushMultitrack(
            rawMultitrackRecordBuffer.getArrayOfReadPointers(), 64, numSamples);
    } else {
      if (numSamples <= mainRecordBuffer.getNumSamples())
        recorder.push(mainRecordBuffer.getReadPointer(0),
                      mainRecordBuffer.getReadPointer(1), numSamples);
    }

    if (dawStreamer.isVirtualSoundcheckActive())
      dawStreamer.advanceReadPos(numSamples);
    if (dawStreamer.isStreaming())
      dawStreamer.advanceWritePos(numSamples);

    // Multichannel USB Audio Class 2.0 ASIO Streaming Backend Bridge (MIDAS
    // Chapter 26)
    hardwareStreamBridge.bridgeHardware(input, numInputs, output, numOutputs,
                                        numSamples);

    // Peak-hold with per-block decay: meter values remain readable between
    // callbacks, but old peaks do not stay pinned forever.

    for (int c = 0; c < kLogicalChannels; ++c) {
      auto &peak = channels[static_cast<size_t>(c)].peak;
      const float decayed = peak.load(std::memory_order_relaxed) * 0.82f;
      peak.store(std::max(channelBlockPeaks[static_cast<size_t>(c)], decayed),
                 std::memory_order_relaxed);

      auto &channel = channels[static_cast<size_t>(c)];
      const auto ci = static_cast<size_t>(c);
      channel.inputPeak.store(
          std::max(blockInputPeaks[ci],
                   channel.inputPeak.load(std::memory_order_relaxed) * 0.82f),
          std::memory_order_relaxed);
      channel.trimmedPeak.store(
          std::max(blockTrimmedPeaks[ci],
                   channel.trimmedPeak.load(std::memory_order_relaxed) * 0.82f),
          std::memory_order_relaxed);
      auto &compGR = channel.compressorGainReductionDb;
      compGR.store(std::max(blockCompressorGR[static_cast<size_t>(c)],
                            compGR.load(std::memory_order_relaxed) * 0.78f),
                   std::memory_order_relaxed);
      auto &gateGR = channel.gateReductionDb;
      gateGR.store(std::max(blockGateReduction[static_cast<size_t>(c)],
                            gateGR.load(std::memory_order_relaxed) * 0.78f),
                   std::memory_order_relaxed);
      for (size_t b = 0; b < 4; ++b) {
        channel.dynEqGainDb[b].store(
            dynamicEqEngine[ci].getBand(b).getDynamicGainDb(),
            std::memory_order_relaxed);
      }
      channel.dynEqWBandDynGainDb.store(
          dynamicEqEngine[ci].getWBandDynamicGainDb(),
          std::memory_order_relaxed);
    }
    masterPeakL.store(
        std::max(peakL, masterPeakL.load(std::memory_order_relaxed) * 0.82f),
        std::memory_order_relaxed);
    masterPeakR.store(
        std::max(peakR, masterPeakR.load(std::memory_order_relaxed) * 0.82f),
        std::memory_order_relaxed);
    for (size_t spk = 0; spk < 12; ++spk) {
      const float decayed =
          surroundMasterPeaks[spk].load(std::memory_order_relaxed) * 0.82f;
      surroundMasterPeaks[spk].store(std::max(blockSurroundPeaks[spk], decayed),
                                     std::memory_order_relaxed);
    }
    if (peakL >= 1.0f)
      mainClipL.store(true, std::memory_order_relaxed);
    if (peakR >= 1.0f)
      mainClipR.store(true, std::memory_order_relaxed);
    for (int a = 0; a < kAuxBusCapacity; ++a) {
      if (blockAuxPeaks[static_cast<size_t>(a)] >= 1.0f)
        auxClip[static_cast<size_t>(a)].store(true, std::memory_order_relaxed);
      auto &peak = auxPeak[static_cast<size_t>(a)];
      peak.store(std::max(blockAuxPeaks[static_cast<size_t>(a)],
                          peak.load(std::memory_order_relaxed) * 0.82f),
                 std::memory_order_relaxed);
    }
    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      if (blockMatrixPeaks[static_cast<size_t>(m)] >= 1.0f)
        matrixClip[static_cast<size_t>(m)].store(true,
                                                 std::memory_order_relaxed);
      auto &peak = matrixPeak[static_cast<size_t>(m)];
      peak.store(std::max(blockMatrixPeaks[static_cast<size_t>(m)],
                          peak.load(std::memory_order_relaxed) * 0.82f),
                 std::memory_order_relaxed);
    }
    monitorPeakL_A.store(
        std::max(blockMonPeakL_A,
                 monitorPeakL_A.load(std::memory_order_relaxed) * 0.82f),
        std::memory_order_relaxed);
    monitorPeakR_A.store(
        std::max(blockMonPeakR_A,
                 monitorPeakR_A.load(std::memory_order_relaxed) * 0.82f),
        std::memory_order_relaxed);
    monitorPeakL_B.store(
        std::max(blockMonPeakL_B,
                 monitorPeakL_B.load(std::memory_order_relaxed) * 0.82f),
        std::memory_order_relaxed);
    monitorPeakR_B.store(
        std::max(blockMonPeakR_B,
                 monitorPeakR_B.load(std::memory_order_relaxed) * 0.82f),
        std::memory_order_relaxed);

    // Phase 90: Push Monitor A samples to lock-free FIFO for Secondary Audio
    // Streamer (WASAPI 2 / Bluetooth)
    const int monSamples =
        std::min(numSamples, static_cast<int>(blockMonBufL.size()));
    const bool monConsumerLive =
        monitorConsumerActive.load(std::memory_order_acquire);
    if (!monConsumerLive && monitorFifo.getFreeSpace() < monSamples) {
      // No active WASAPI 2 consumer: keep only recent samples in FIFO without
      // counting false overruns or stalling when WASAPI 2 later connects.
      const int readyNow = monitorFifo.getNumReady();
      const int keepMax = 4096;
      if (readyNow > keepMax) {
        int rStart1 = 0, rSize1 = 0, rStart2 = 0, rSize2 = 0;
        monitorFifo.prepareToRead(readyNow - keepMax, rStart1, rSize1, rStart2,
                                  rSize2);
        monitorFifo.finishedRead(rSize1 + rSize2);
      }
    }
    int monStart1 = 0, monSize1 = 0, monStart2 = 0, monSize2 = 0;
    monitorFifo.prepareToWrite(monSamples, monStart1, monSize1, monStart2,
                               monSize2);
    if (monConsumerLive && (monSize1 + monSize2 < monSamples)) {
      monitorFifoOverruns.fetch_add(1, std::memory_order_relaxed);
    }
    if (monSize1 > 0) {
      for (int i = 0; i < monSize1; ++i) {
        monitorFifoL[static_cast<size_t>(monStart1 + i)] =
            blockMonBufL[static_cast<size_t>(i)];
        monitorFifoR[static_cast<size_t>(monStart1 + i)] =
            blockMonBufR[static_cast<size_t>(i)];
      }
    }
    if (monSize2 > 0) {
      for (int i = 0; i < monSize2; ++i) {
        monitorFifoL[static_cast<size_t>(monStart2 + i)] =
            blockMonBufL[static_cast<size_t>(monSize1 + i)];
        monitorFifoR[static_cast<size_t>(monStart2 + i)] =
            blockMonBufR[static_cast<size_t>(monSize1 + i)];
      }
    }
    monitorFifo.finishedWrite(monSize1 + monSize2);
    const int readySamples = monitorFifo.getNumReady();
    monitorFifoQueueDepth.store(readySamples, std::memory_order_relaxed);
    const double curSr = sampleRate.load(std::memory_order_relaxed);
    if (curSr > 1000.0) {
      monitorFifoLatencyMs.store(
          static_cast<float>((readySamples / curSr) * 1000.0),
          std::memory_order_relaxed);
    }
    for (size_t fxIdx = 0; fxIdx < nova::graviton::EffectsRack::kNumSlots;
         ++fxIdx) {
      auto &slot = effectsRack.getSlot(fxIdx);
      slot.inputPeak.store(slot.inputPeak.load(std::memory_order_relaxed) *
                               0.85f,
                           std::memory_order_relaxed);
      slot.outputPeak.store(slot.outputPeak.load(std::memory_order_relaxed) *
                                0.85f,
                            std::memory_order_relaxed);
    }
  }

private:
  double lastCallbackSampleRate = -1.0;
  bool prevSurroundActive = false;
  double highPassAlpha = 0.0;
  float compressorAttack = 0.0f, compressorRelease = 0.0f;
  std::array<double, kLogicalChannels> highPassInput{};
  std::array<double, kLogicalChannels> highPassOutput{};
  std::array<nova::graviton::DynamicsProcessor, kLogicalChannels>
      dynamicsProcessor{};
};

// ==============================================================================
// Phase 89: Offline Mixdown & Multi-Track Stems Bounce target (Chapter 36)
// Bridges the OfflineBouncer onto MixerEngine's deterministic test seam so the
// bounce renders through the exact production callback path, capturing Master
// L/R plus every bus stem in one pass.
// ==============================================================================
class MixerEngineBounceTarget final
    : public nova::graviton::OfflineRenderTarget {
public:
  explicit MixerEngineBounceTarget(MixerEngine &e) : engine(e) {}

  int getNumStems() const override { return MixerEngine::kOfflineStemCount; }

  juce::String getStemName(int stemIndex) const override {
    if (stemIndex >= 0 && stemIndex < kVisibleAuxSends)
      return "AUX " + juce::String(stemIndex + 1);
    const int matrixIndex = stemIndex - kVisibleAuxSends;
    if (matrixIndex >= 0 && matrixIndex < kVisibleMatrixBuses)
      return "MATRIX " + juce::String(matrixIndex + 1);
    return "STEM " + juce::String(stemIndex + 1);
  }

  void renderBlock(const float *const *inputs, int numInputs, int numSamples,
                   double sampleRate, float *masterOutL, float *masterOutR,
                   float *const *stems) override {
    MixerEngine::OfflineStemCapture plan;
    plan.masterL = masterOutL;
    plan.masterR = masterOutR;
    plan.numSamples = numSamples;
    for (int s = 0; s < getNumStems(); ++s)
      plan.stems[static_cast<size_t>(s)] = stems[s];
    engine.armOfflineCapture(plan);
    engine.renderOfflineBlocks(inputs, numInputs, numSamples, sampleRate,
                               masterOutL, masterOutR);
    engine.disarmOfflineCapture();
  }

  // 1:1 track patch: file track N drives console channel N for the duration of
  // the bounce; the operator's patch is restored afterwards.
  void applyTrackPatch(int numTracks) {
    if (patchApplied)
      return;
    const int count = juce::jlimit(0, kLogicalChannels, numTracks);
    savedPatch.reserve(static_cast<size_t>(count));
    for (int c = 0; c < count; ++c) {
      const int previous =
          engine.channels[static_cast<size_t>(c)].sourceInput.load(
              std::memory_order_relaxed);
      savedPatch.emplace_back(c, previous);
      engine.channels[static_cast<size_t>(c)].sourceInput.store(
          c, std::memory_order_relaxed);
    }
    patchApplied = true;
  }

  void restoreTrackPatch() {
    for (const auto &entry : savedPatch)
      engine.channels[static_cast<size_t>(entry.first)].sourceInput.store(
          entry.second, std::memory_order_relaxed);
    savedPatch.clear();
    patchApplied = false;
  }

  bool isTrackPatchApplied() const { return patchApplied; }

private:
  MixerEngine &engine;
  std::vector<std::pair<int, int>>
      savedPatch; // channel -> previous source input
  bool patchApplied = false;
};

// Basic fixed MIDI CC map. MIDI callbacks only write atomic mixer parameters.

} // namespace nova
