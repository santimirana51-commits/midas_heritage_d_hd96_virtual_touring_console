#pragma once
#include "DSP/GravitonDSP.h"
#include "DSP/GravitonDynamics.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <juce_core/juce_core.h>

namespace nova {

constexpr int kLogicalChannels = 144;
constexpr int kInitialPhysicalInputs = 8;
constexpr int kAuxBusCapacity = 96; // AUX 1-2 for Stereo Monitor, AUX 3-96 available
constexpr int kMatrixBusCapacity = 24;
constexpr int kVisibleAuxSends = 4;
constexpr int kVisibleMatrixBuses = 2;

struct ChannelState {
  std::atomic<float> inputGainDb{
      0.0f};                          // Preamp/trim before HPF, EQ and dynamics
  std::atomic<bool> phantom48v{false}; // +48V phantom power state
  std::atomic<float> inputPeak{0.0f}; // Raw physical input peak before trim
  std::atomic<float> trimmedPeak{
      0.0f}; // Peak after input trim, before channel processing
  std::atomic<float> gainDb{0.0f}; // Channel fader, separate from input trim
  std::atomic<float> pan{0.0f};    // -1 left, +1 right
  std::atomic<bool> muted{false};
  std::atomic<bool> solo{false};
  // Phase 90: Master & Monitor Independent Routing (FOH Master vs Musician
  // Monitor IEM/Wedge)
  std::atomic<bool> masterAssigned{true};
  std::atomic<bool> monitorAssigned{true};
  std::atomic<float> monitorSendDb{
      0.0f}; // Independent send level to Monitor bus (-90 dB to +10 dB)
  std::atomic<bool> monitorPreFader{
      false}; // false = post-fader, true = pre-fader
  std::atomic<bool> highPassEnabled{false};
  std::atomic<float> highPassFreqHz{80.0f};
  std::atomic<int> highPassSlope{0}; // 0 = 12 dB/oct, 1 = 24 dB/oct
  std::atomic<bool> lowPassEnabled{false};
  std::atomic<float> lowPassFreqHz{12000.0f};
  std::atomic<int> lowPassSlope{0}; // 0 = 12 dB/oct, 1 = 24 dB/oct
  std::atomic<bool> compressorEnabled{false};
  std::atomic<bool> gateEnabled{false};
  std::atomic<float> gateThresholdDb{-45.0f};
  std::atomic<float> gateRatio{4.0f};
  std::atomic<float> gateAttackMs{2.0f};
  std::atomic<float> gateReleaseMs{120.0f};
  std::atomic<float> compressorThresholdDb{-18.0f};
  std::atomic<float> compressorRatio{4.0f};
  std::atomic<float> compressorAttackMs{10.0f};
  std::atomic<float> compressorReleaseMs{120.0f};
  std::atomic<float> compressorInputGainDb{0.0f};
  std::atomic<float> compressorMakeupGainDb{0.0f};
  std::atomic<float> compressorKneeDb{3.0f};
  std::atomic<int> compressorKneeMode{2}; // 0 = Hard (0 dB), 1 = Linear, 2 = Soft
  std::atomic<float> compressorRmsMs{1.0f};
  std::atomic<bool> compressorMaximize{false};
  std::atomic<bool> compressorCustomShape{false};
  std::atomic<float> gateHoldMs{50.0f};
  std::atomic<int> compressorMode{
      static_cast<int>(nova::graviton::CompressorMode::Corrective)};
  std::atomic<int> gateMode{
      static_cast<int>(nova::graviton::GateMode::DualGate)}; // 0=DualGate, 1=TransientDesigner, 2=ExpanderDucker
  std::atomic<float> dynamicsInDb{-120.0f};
  std::atomic<float> dynamicsSideDb{-120.0f};
  std::atomic<float> dynamicsOutDb{-120.0f};
  // Four fully parametric peaking EQ bands; zero gain is flat/bypassed.
  std::atomic<bool> eqEnabled{true};
  std::atomic<int> eqModel{0}; // 0=MidasPro, 1=Neve1073, 2=SSL4000G, 3=API550A
  std::array<std::atomic<float>, 4> eqFrequency{
      std::atomic<float>{100.0f}, std::atomic<float>{500.0f},
      std::atomic<float>{2000.0f}, std::atomic<float>{8000.0f}};
  std::array<std::atomic<float>, 4> eqGainDb{
      std::atomic<float>{0.0f}, std::atomic<float>{0.0f},
      std::atomic<float>{0.0f}, std::atomic<float>{0.0f}};
  std::array<std::atomic<float>, 4> eqQ{
      std::atomic<float>{1.0f}, std::atomic<float>{1.0f},
      std::atomic<float>{1.0f}, std::atomic<float>{1.0f}};
  std::atomic<float> peak{0.0f};
  // Runtime-only meters: positive dB attenuation reported for dynamics
  // processors.
  std::atomic<float> compressorGainReductionDb{0.0f};
  std::atomic<float> gateReductionDb{0.0f};
  // Physical input index; -1 means disconnected. Channels 1-8 default to inputs
  // 1-8.
  std::atomic<int> sourceInput{-1};
  std::atomic<int> vcaGroup{0}; // 0 = unassigned, 1-24 = VCA group
  std::atomic<uint8_t> muteGroupMask{
      0}; // Bitmask: bits 0-7 correspond to Mute Groups 1-8
  std::array<std::atomic<float>, kAuxBusCapacity> auxSendDb{};
  std::array<std::atomic<bool>, kAuxBusCapacity> auxPreFader{};
  std::atomic<bool> stereoLinked{false};
  std::atomic<bool> polarityInverted{false};
  std::atomic<float> inputDelayMs{0.0f};
  std::atomic<bool> postEqDynamics{true};
  std::atomic<bool> muteSafe{false};
  std::atomic<bool> soloSafe{false};
  std::atomic<int> soloBus{0};     // 0 = Solo A, 1 = Solo B
  std::atomic<bool> soloPfl{true}; // true = PFL, false = AFL
  std::atomic<bool> allPassEnabled{false};
  std::atomic<float> allPassDegrees{0.0f}; // 0.0 to 180.0 deg
  std::atomic<int> allPassRange{1};        // 0 = 90 deg max, 1 = 180 deg max
  std::atomic<int> allPassFreqMode{0}; // 0 = Low (100 Hz), 1 = High (1500 Hz)
  // Phase 81: Dan Dugan Speech Automixer Parameters (MIDAS HD96-24 Chapter 28)
  std::atomic<int> duganGroup{
      0}; // 0 = Off, 1 = Group 1, 2 = Group 2, 3 = Group 3
  std::atomic<float> duganWeightDb{0.0f}; // -12.0 to +12.0 dB
  std::atomic<float> duganGainDb{0.0f};   // Runtime automixer attenuation (dB)
  // Phase 82: 4-Band Dynamic Parametric EQ Parameters (MIDAS HD96-24 Chapter
  // 16)
  std::array<std::atomic<bool>, 4> dynEqEnabled{
      std::atomic<bool>{false}, std::atomic<bool>{false},
      std::atomic<bool>{false}, std::atomic<bool>{false}};
  std::array<std::atomic<int>, 4> dynEqMode{
      std::atomic<int>{0}, std::atomic<int>{0}, std::atomic<int>{0},
      std::atomic<int>{0}}; // 0 = Above, 1 = Below
  std::array<std::atomic<float>, 4> dynEqThresholdDb{
      std::atomic<float>{-20.0f}, std::atomic<float>{-20.0f},
      std::atomic<float>{-20.0f}, std::atomic<float>{-20.0f}};
  std::array<std::atomic<float>, 4> dynEqRangeDb{
      std::atomic<float>{-6.0f}, std::atomic<float>{-6.0f},
      std::atomic<float>{-6.0f}, std::atomic<float>{-6.0f}}; // -24 to +24 dB
  std::array<std::atomic<float>, 4> dynEqRatio{
      std::atomic<float>{4.0f}, std::atomic<float>{4.0f},
      std::atomic<float>{4.0f}, std::atomic<float>{4.0f}};
  std::array<std::atomic<float>, 4> dynEqKneeDb{
      std::atomic<float>{3.0f}, std::atomic<float>{3.0f},
      std::atomic<float>{3.0f}, std::atomic<float>{3.0f}};
  std::array<std::atomic<float>, 4> dynEqAttackMs{
      std::atomic<float>{10.0f}, std::atomic<float>{10.0f},
      std::atomic<float>{10.0f}, std::atomic<float>{10.0f}};
  std::array<std::atomic<float>, 4> dynEqReleaseMs{
      std::atomic<float>{100.0f}, std::atomic<float>{100.0f},
      std::atomic<float>{100.0f}, std::atomic<float>{100.0f}};
  std::array<std::atomic<float>, 4> dynEqGainDb{
      std::atomic<float>{0.0f}, std::atomic<float>{0.0f},
      std::atomic<float>{0.0f}, std::atomic<float>{0.0f}};
  // TDR Nova Parallel Dynamic EQ Parameters
  std::atomic<bool> dynEqParallelMode{true};
  std::array<std::atomic<bool>, 4> dynEqSplit{
      std::atomic<bool>{true}, std::atomic<bool>{true},
      std::atomic<bool>{true}, std::atomic<bool>{true}}; // false = slaved to W-Band
  std::array<std::atomic<int>, 4> dynEqShape{
      std::atomic<int>{1}, std::atomic<int>{1},
      std::atomic<int>{1}, std::atomic<int>{1}}; // 0=LowShelf, 1=Bell, 2=HighShelf
  std::array<std::atomic<int>, 4> dynEqStereoMode{
      std::atomic<int>{0}, std::atomic<int>{0},
      std::atomic<int>{0}, std::atomic<int>{0}}; // 0=Stereo, 1=Sum, 2=Diff, 3=Left, 4=Right, 5=DualMono
  std::atomic<bool> dynEqWBandEnabled{false};
  std::atomic<int> dynEqWBandMode{0}; // 0 = Above (Compress), 1 = Below (Expand)
  std::atomic<float> dynEqWBandThresholdDb{-20.0f};
  std::atomic<float> dynEqWBandRatio{2.0f};
  std::atomic<float> dynEqWBandRangeDb{-6.0f};
  std::atomic<float> dynEqWBandKneeDb{3.0f};
  std::atomic<float> dynEqWBandAttackMs{10.0f};
  std::atomic<float> dynEqWBandReleaseMs{100.0f};
  std::atomic<float> dynEqWBandGainDb{0.0f};
  std::atomic<float> dynEqWBandDynGainDb{0.0f};
  std::atomic<bool> dynEqElBypass{false};     // Equal Loudness compensation
  std::atomic<bool> dynEqGrDelta{false};      // Audition gain reduction delta
  std::atomic<int> dynEqSoloBand{-1};         // -1 = off, 0..3 = band I..IV
  std::atomic<bool> dynEqExtSc{false};        // INT SC (false) vs EXT SC (true)
  std::atomic<float> dynEqDryMixPct{0.0f};    // 0.0 to 100.0%
  std::atomic<float> dynEqEqGainDb{0.0f};     // -24.0 to +24.0 dB
  std::atomic<float> dynEqOutGainDb{0.0f};    // -24.0 to +24.0 dB
  std::atomic<bool> dynEqHpfEnabled{false};
  std::atomic<float> dynEqHpfFreqHz{30.0f};
  std::atomic<int> dynEqHpfSlope{1};          // 0=6dB, 1=12dB, 2=18dB, 3=24dB
  std::atomic<bool> dynEqLpfEnabled{false};
  std::atomic<float> dynEqLpfFreqHz{18000.0f};
  std::atomic<int> dynEqLpfSlope{1};          // 0=6dB, 1=12dB, 2=18dB, 3=24dB
  // Phase 83: Frequency-Conscious Key Sidechain & External Key Routing (MIDAS
  // Chapter 17)
  std::atomic<int> sidechainSourceType{0}; // 0 = Internal, 1 = External Channel
  std::atomic<int> sidechainExternalChannel{0}; // 0 to 31 (CH 1 to 32)
  std::atomic<int> sidechainFilterType{
      0}; // 0 = Bypass, 1 = HPF, 2 = LPF, 3 = Bandpass, 4 = Notch
  std::atomic<float> sidechainFilterFreqHz{1000.0f}; // 20.0 to 20000.0 Hz
  std::atomic<float> sidechainFilterQ{1.0f};         // 0.1 to 10.0
  std::atomic<bool> sidechainKeyListen{
      false}; // Audition filtered detector signal on solo/monitor bus

  // Phase 85: Multichannel Surround & Immersive Panning 5.1 / 7.1 / LCR &
  // Spatial Object Renderer (MIDAS Chapter 30)
  std::atomic<int> surroundMode{
      0}; // 0 = Stereo (Default), 1 = LCR, 2 = Quad 4.0, 3 = Surround 5.1, 4 =
          // Surround 7.1, 5 = Immersive 7.1.4
  std::atomic<float> surroundX{0.0f}; // -1.0 (Left) to +1.0 (Right)
  std::atomic<float> surroundY{1.0f}; // -1.0 (Rear) to +1.0 (Front)
  std::atomic<float> surroundZ{0.0f}; // 0.0 (Floor) to 1.0 (Height/Ceiling)
  std::atomic<float> surroundDivergence{
      0.0f}; // 0.0 (Point Source) to 1.0 (Full Diffuse)
  std::atomic<float> surroundCenterPct{100.0f}; // 0.0% to 100.0%
  std::atomic<float> surroundLfeGainDb{-60.0f}; // -60.0 dB (Off) to +10.0 dB

  // Phase 95: Channel Color Coding & Custom Instrument Icons (MIDAS HD96-24
  // Chapter 12)
  std::atomic<uint32_t> colorRgb{0xff2d3540}; // Default Slate
  std::atomic<int> instrumentIcon{0}; // 0 = GEN, 1 = KICK, ..., 15 = PERC

  // Phase 98: POPulation Groups Navigation & Multi-Channel Spill (MIDAS HD96-24
  // Chapter 11)
  std::atomic<int> popGroup{0}; // 0 = None, 1..24 = POP Group 1..24

  ChannelState() noexcept {
    for (auto &send : auxSendDb)
      send.store(-60.0f, std::memory_order_relaxed);
  }
};

static constexpr uint32_t kMidasColors[16] = {
    0xff2d3540, // 0: Slate (Default)
    0xffe53935, // 1: Red
    0xffd81b60, // 2: Pink / Magenta
    0xff8e24aa, // 3: Purple
    0xff3949ab, // 4: Indigo
    0xff1e88e5, // 5: Blue
    0xff00acc1, // 6: Cyan (MIDAS Cyan)
    0xff00897b, // 7: Teal
    0xff43a047, // 8: Green
    0xff7cb342, // 9: Light Green
    0xffc0ca33, // 10: Lime
    0xfffdd835, // 11: Yellow
    0xffffb300, // 12: Amber
    0xfffb8c00, // 13: Orange
    0xff6d4c41, // 14: Brown
    0xff757575  // 15: Grey
};

static const char *const kInstrumentTags[16] = {
    "GEN",  "KICK", "SNR",  "HH",  "TOM",  "OH",   "BASS", "GTR",
    "AC.G", "KEYS", "SYNT", "VOX", "BVOX", "BRSS", "STR",  "PERC"};

// Background WAV writer. The audio callback only copies into a preallocated
// FIFO; file I/O is performed on this worker thread to avoid disk writes in
// real-time audio.

} // namespace nova
