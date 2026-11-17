#include "../Source/DSP/GravitonDSP.h"
#include "../Source/DSP/GravitonDN360.h"
#include "../Source/DSP/GravitonDeEsser.h"
#include "../Source/DSP/GravitonDynamicEQ.h"
#include "../Source/DSP/GravitonDynamics.h"
#include "../Source/Effects/GravitonDVR2Reverb.h"
#include "../Source/Effects/GravitonKT1176.h"
#include "../Source/Effects/GravitonKTBusComp.h"
#include "../Source/Effects/GravitonManipulatorPitch.h"
#include "../Source/Effects/GravitonMasterX.h"
#include "../Source/Effects/GravitonNebulaDelay.h"
#include "../Source/Effects/GravitonPlateReverb.h"
#include "../Source/Effects/GravitonSCFChorus.h"
#include "../Source/Effects/GravitonFlangerBL20.h"
#include "../Source/Effects/GravitonOpticalPhaser.h"
#include "../Source/Effects/GravitonTC2290.h"
#include "../Source/Effects/GravitonNonLin2.h"
#include "../Source/Effects/GravitonVocalPitchCorrection.h"
#include "../Source/Effects/GravitonRackDynamics.h"
#include "../Source/Effects/GravitonSubHarmonic.h"
#include "../Source/Effects/GravitonDistortion.h"
#include "../Source/Effects/GravitonConvolutionReverb.h"
#include "../Source/Effects/GravitonHybridReverb.h"
#include "../Source/Effects/GravitonEffectsRack.h"
#include "../Source/Mixer/GravitonBusIntegration.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>

using namespace nova::graviton;

int main() {
  static_assert(sizeof(Sample) == sizeof(double));
  static_assert(kSamplePrecisionBits == 64);

  assert(sanitize(std::numeric_limits<Sample>::quiet_NaN()) == 0.0);
  assert(sanitize(std::numeric_limits<Sample>::infinity()) == 0.0);
  assert(std::abs(dbToGain(0.0) - 1.0) < 1e-14);
  assert(std::abs(gainToDb(dbToGain(-18.0)) + 18.0) < 1e-12);
  assert(dbToGain(-1000.0) == 0.0);

  // The Main bus now sums in double and narrows safely only at the float device
  // boundary.
  assert(toFloatBoundary(0.125) == 0.125f);
  assert(toFloatBoundary(std::numeric_limits<Sample>::quiet_NaN()) == 0.0f);
  assert(toFloatBoundary(std::numeric_limits<Sample>::infinity()) == 0.0f);
  assert(std::isfinite(toFloatBoundary(std::numeric_limits<Sample>::max())));
  StereoFrame precisionFrame{};
  accumulate(precisionFrame, 100000000.0, 0.0);
  accumulate(precisionFrame, 0.25, 0.0);
  accumulate(precisionFrame, -100000000.0, 0.0);
  assert(std::abs(precisionFrame.left - 0.25) < 1e-12);

  GainSmoother smoother;
  smoother.prepare(48000.0, 1.0, 1.0);
  smoother.setTargetDb(-6.0);
  const Sample first = smoother.next();
  assert(first < 1.0 && first > dbToGain(-6.0));
  for (int i = 0; i < 1000; ++i)
    smoother.next();
  assert(std::abs(smoother.current() - dbToGain(-6.0)) < 1e-12);

  const auto unity = makePeakingEq(48000.0, 1000.0, 0.0, 1.0);
  Biquad filter;
  filter.setCoefficients(unity);
  for (int i = 0; i < 100; ++i)
    assert(std::abs(filter.process(0.25) - 0.25) < 1e-12);

  const auto boost = makePeakingEq(48000.0, 1000.0, 6.0, 1.0);
  filter.setCoefficients(boost);
  filter.reset();
  Sample maxAbs = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const Sample input =
        std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 48000.0);
    const Sample output = filter.process(input);
    assert(std::isfinite(output));
    maxAbs = std::max(maxAbs, std::abs(output));
  }
  assert(maxAbs > 1.0);

  // Double-precision first-order HPF: DC decays toward zero and all states stay
  // finite.
  OnePoleHighPass hpf;
  assert(hpf.prepare(48000.0, 100.0));
  Sample dcOutput = 0.0;
  for (int i = 0; i < 48000; ++i)
    dcOutput = hpf.process(1.0);
  assert(std::isfinite(dcOutput));
  assert(std::abs(dcOutput) < 1.0e-8);
  hpf.reset();
  Sample hpfPeak = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const Sample input =
        std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 48000.0);
    hpfPeak = std::max(hpfPeak, std::abs(hpf.process(input)));
    assert(std::isfinite(hpfPeak));
  }
  assert(hpfPeak > 0.9 && hpfPeak < 1.1);
  assert(!hpf.prepare(std::numeric_limits<Sample>::quiet_NaN(), 100.0));

  const auto invalid =
      makePeakingEq(std::numeric_limits<Sample>::quiet_NaN(), 1000.0, 6.0, 1.0);
  assert(invalid.b0 == 1.0 && invalid.b1 == 0.0);

  // Eight serial double-precision EQ bands, representative of the production
  // channel path.
  std::array<Biquad, 8> cascade;
  cascade[0].setCoefficients(makePeakingEq(48000.0, 100.0, 6.0, 0.8));
  cascade[1].setCoefficients(makePeakingEq(48000.0, 700.0, -4.0, 1.2));
  cascade[2].setCoefficients(makePeakingEq(48000.0, 3200.0, 3.0, 1.4));
  cascade[3].setCoefficients(makePeakingEq(48000.0, 10000.0, -2.0, 0.9));
  cascade[4].setCoefficients(makePeakingEq(48000.0, 60.0, 1.0, 1.0));
  cascade[5].setCoefficients(makePeakingEq(48000.0, 250.0, -1.0, 1.0));
  cascade[6].setCoefficients(makePeakingEq(48000.0, 5000.0, 2.0, 1.1));
  cascade[7].setCoefficients(makePeakingEq(48000.0, 15000.0, -1.0, 0.9));
  for (int i = 0; i < 48000 * 3; ++i) {
    Sample sample =
        0.5 * std::sin(2.0 * 3.14159265358979323846 * 997.0 * i / 48000.0);
    for (auto &band : cascade)
      sample = band.process(sample);
    assert(std::isfinite(sample));
    assert(std::abs(sample) < 10.0);
  }
  for (auto &band : cascade)
    band.reset();
  for (auto &band : cascade)
    assert(std::abs(band.process(0.0)) < 1e-15);

  // Double fader smoothing and equal-power pan behavior used by the channel
  // path.
  assert(std::abs(smoothGainStep(1.0, 0.0, 0.5) - 0.5) < 1e-15);
  const Sample smoothingCoeff = gainSmoothingCoefficient(48000.0, 0.010);
  assert(smoothingCoeff > 0.0 && smoothingCoeff < 1.0);
  assert(gainSmoothingCoefficient(0.0, 0.010) == 1.0);
  Sample smoothedMaster = 1.0;
  const Sample masterTarget = dbToGain(-12.0);
  for (int i = 0; i < 48000; ++i)
    smoothedMaster =
        smoothGainStep(smoothedMaster, masterTarget, smoothingCoeff);
  assert(std::abs(smoothedMaster - masterTarget) < 1e-12);
  const auto centerPan = equalPowerPanGains(0.0);
  assert(std::abs(centerPan.left - std::sqrt(0.5)) < 1e-14);
  assert(std::abs(centerPan.right - std::sqrt(0.5)) < 1e-14);
  const auto leftPan = equalPowerPanGains(-1.0);
  assert(std::abs(leftPan.left - 1.0) < 1e-14);
  assert(std::abs(leftPan.right) < 1e-14);
  const auto rightPan = equalPowerPanGains(1.0);
  assert(std::abs(rightPan.left) < 1e-14);
  assert(std::abs(rightPan.right - 1.0) < 1e-14);

  StereoFrame frame{};
  accumulate(frame, 0.1, -0.2);
  accumulate(frame, 0.3, 0.4);
  assert(std::abs(frame.left - 0.4) < 1e-15);
  assert(std::abs(frame.right - 0.2) < 1e-15);
  accumulate(frame, std::numeric_limits<Sample>::infinity(), 0.0);
  assert(std::isfinite(frame.left));

  // AUX send accumulation and master gain remain double until the output
  // boundary.
  Sample auxBus = 0.0;
  accumulateBus(auxBus, 100000000.0, 1.0);
  accumulateBus(auxBus, 0.25, 1.0);
  accumulateBus(auxBus, -100000000.0, 1.0);
  assert(std::abs(auxBus - 0.25) < 1e-12);
  assert(std::abs(applyBusMaster(auxBus, 0.5, false) - 0.125) < 1e-12);
  assert(applyBusMaster(auxBus, 0.5, true) == 0.0);
  Sample invalidAux = 0.0;
  accumulateBus(invalidAux, std::numeric_limits<Sample>::infinity(), 1.0);
  assert(invalidAux == 0.0);

  // Matrix 1/2 preserve double precision through source summing and master
  // gain.
  const std::array<Sample, 4> matrixAux{100000000.0, 0.25, -100000000.0, 0.0};
  const auto matrix =
      mixMatrixOutputs(0.5, 0.5, matrixAux, 2.0, false, 0.5, false);
  assert(std::abs(matrix.matrix1 - 1.0) < 1e-12);
  assert(std::abs(matrix.matrix2 - 0.125) < 1e-12);
  const auto mutedMatrix =
      mixMatrixOutputs(0.5, -0.5, matrixAux, 1.0, true, 1.0, true);
  assert(mutedMatrix.matrix1 == 0.0 && mutedMatrix.matrix2 == 0.0);
  const std::array<Sample, 4> invalidMatrixAux{
      std::numeric_limits<Sample>::infinity(), 0.0, 0.0, 0.0};
  const auto finiteMatrix =
      mixMatrixOutputs(std::numeric_limits<Sample>::quiet_NaN(), 0.5,
                       invalidMatrixAux, 1.0, false, 1.0, false);
  assert(std::isfinite(finiteMatrix.matrix1) &&
         std::isfinite(finiteMatrix.matrix2));

  // Double-precision gate/compressor envelope and gain-reduction path.
  DynamicsProcessor dynamics;
  DynamicsSettings settings;
  settings.attackCoefficient = std::exp(-1.0 / (48000.0 * 0.010));
  settings.releaseCoefficient = std::exp(-1.0 / (48000.0 * 0.120));
  settings.compressorEnabled = true;
  settings.compressorThresholdDb = -18.0;
  settings.compressorRatio = 4.0;
  Sample compressed = 0.0;
  Sample maxReduction = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const Sample input =
        0.8 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0);
    const auto result = dynamics.process(input, settings);
    compressed = result.sample;
    maxReduction = std::max(maxReduction, result.compressorReductionDb);
    assert(std::isfinite(result.sample));
    assert(std::isfinite(dynamics.envelope()));
  }
  assert(maxReduction > 0.0);
  assert(std::abs(compressed) < 0.8);

  DynamicsProcessor gate;
  settings.compressorEnabled = false;
  settings.gateEnabled = true;
  settings.gateThresholdDb = -30.0;
  settings.gateRatio = 4.0;
  const auto gated = gate.process(0.001, settings);
  assert(std::isfinite(gated.sample));
  assert(gated.gateReductionDb > 0.0);
  gate.reset();
  assert(gate.envelope() == 0.0);

  // Phase 3 dynamics: gate hold and compressor knee/makeup gain are preserved
  // in the shared channel dynamics engine and remain safe under limiting.
  DynamicsSettings phase3;
  phase3.sampleRate = 48000.0;
  phase3.gateEnabled = true;
  phase3.gateThresholdDb = -24.0;
  phase3.gateRatio = 2.0;
  phase3.gateHoldMs = 100.0;
  phase3.expanderRangeDb = 12.0;
  phase3.compressorEnabled = true;
  phase3.compressorThresholdDb = -12.0;
  phase3.compressorRatio = 3.0;
  phase3.compressorKneeDb = 6.0;
  phase3.compressorMakeupGainDb = 3.0;
  phase3.attackCoefficient = std::exp(-1.0 / (48000.0 * 0.010));
  phase3.releaseCoefficient = std::exp(-1.0 / (48000.0 * 0.120));
  const auto phase3Gate = gate.process(0.001, phase3);
  const auto phase3Comp = dynamics.process(0.9, phase3);
  assert(std::isfinite(phase3Gate.sample));
  assert(phase3Gate.gateReductionDb >= 0.0);
  assert(phase3Comp.compressorReductionDb >= 0.0);
  assert(std::abs(phase3Comp.sample) <= 2.0);

  // Cross-bus integration regression: two channels -> Main/AUX -> Matrix.
  // Channel 1 sends PRE (0.5), channel 2 sends POST (-0.25); both also feed
  // Main.
  const std::array<Sample, 2> flowPre{0.5, -0.5};
  const std::array<Sample, 2> flowPost{0.25, -0.25};
  const std::array<StereoFrame, 2> flowPan{StereoFrame{1.0, 0.0},
                                           StereoFrame{0.0, 1.0}};
  const std::array<Sample, 2> flowSendGain{0.5, 0.25};
  const std::array<bool, 2> flowSendPre{true, false};
  const auto integrated =
      integrateTwoChannelBusFrame(flowPre, flowPost, flowPan, flowSendGain,
                                  flowSendPre, 2.0, 2.0, 0.5, 0.25);
  assert(std::abs(integrated.main.left - 0.5) < 1e-12);
  assert(std::abs(integrated.main.right + 0.5) < 1e-12);
  assert(std::abs(integrated.aux - 0.375) < 1e-12);
  assert(std::abs(integrated.matrixFromMain) <
         1e-12); // mono Main L/R cancellation
  assert(std::abs(integrated.matrixFromAux - 0.09375) < 1e-12);

  const auto mutedIntegrated = integrateTwoChannelBusFrame(
      flowPre, flowPost, flowPan, flowSendGain, flowSendPre, 2.0, 2.0, 0.5,
      0.25, true, false, false, true);
  assert(mutedIntegrated.main.left == 0.0 && mutedIntegrated.main.right == 0.0);
  assert(mutedIntegrated.matrixFromMain == 0.0);
  assert(mutedIntegrated.matrixFromAux == 0.0);

  // High dynamic range and non-finite inputs must not leak NaN/Inf across
  // buses.
  const std::array<Sample, 2> stressPre{
      100000000.0, std::numeric_limits<Sample>::quiet_NaN()};
  const std::array<Sample, 2> stressPost{100000000.0, -100000000.0};
  const std::array<StereoFrame, 2> stressPan{
      StereoFrame{0.7071067811865476, 0.7071067811865476},
      StereoFrame{0.7071067811865476, 0.7071067811865476}};
  const std::array<Sample, 2> stressGain{1.0, 1.0};
  const std::array<bool, 2> stressSendPre{true, false};
  const auto stressed =
      integrateTwoChannelBusFrame(stressPre, stressPost, stressPan, stressGain,
                                  stressSendPre, 1.0, 1.0, 1.0, 1.0);
  assert(std::isfinite(stressed.main.left) &&
         std::isfinite(stressed.main.right));
  assert(std::isfinite(stressed.aux));
  assert(std::isfinite(stressed.matrixFromMain) &&
         std::isfinite(stressed.matrixFromAux));

  // Sparse AUX route helper must match dense routing for configured sends.
  {
    std::array<Sample, 8> denseAux{};
    std::array<Sample, 8> sparseAux{};
    std::array<Sample, 8> denseGains{};
    std::array<bool, 8> densePre{};
    std::array<ActiveAuxRoute, 4> routes{};
    std::size_t routeCount = 0;
    for (std::size_t i = 0; i < 8; ++i) {
      if (i == 1 || i == 4 || i == 7) {
        denseGains[i] = 0.25 * static_cast<Sample>(i + 1);
        densePre[i] = (i == 4);
        routes[routeCount++] = ActiveAuxRoute{i, denseGains[i], densePre[i]};
      }
    }
    Sample denseL = 0.0, denseR = 0.0, sparseL = 0.0, sparseR = 0.0;
    accumulateChannelToBuses(denseL, denseR, denseAux, 0.3, -0.2, 0.6, 0.8,
                             denseGains, densePre);
    accumulateChannelToBusesSparse(sparseL, sparseR, sparseAux, 0.3, -0.2, 0.6,
                                   0.8, routes, routeCount);
    assert(std::abs(denseL - sparseL) < 1e-12);
    assert(std::abs(denseR - sparseR) < 1e-12);
    for (std::size_t i = 0; i < denseAux.size(); ++i)
      assert(std::abs(denseAux[i] - sparseAux[i]) < 1e-12);

    // Adaptive policy uses sparse for low density and dense for high density,
    // while preserving the same accumulated output for valid route snapshots.
    assert(chooseAuxRoutingMode(3, 8) == AuxRoutingMode::Sparse);
    assert(chooseAuxRoutingMode(4, 8) == AuxRoutingMode::Dense);
    assert(chooseAuxRoutingMode(96, 96) == AuxRoutingMode::Dense);
    std::array<Sample, 8> adaptiveAux{};
    Sample adaptiveL = 0.0, adaptiveR = 0.0;
    const auto chosen = accumulateChannelToBusesAdaptive(
        adaptiveL, adaptiveR, adaptiveAux, 0.3, -0.2, 0.6, 0.8, denseGains,
        densePre, routes, routeCount);
    assert(chosen == AuxRoutingMode::Sparse);
    assert(std::abs(adaptiveL - denseL) < 1e-12);
    assert(std::abs(adaptiveR - denseR) < 1e-12);
    for (std::size_t i = 0; i < denseAux.size(); ++i)
      assert(std::abs(adaptiveAux[i] - denseAux[i]) < 1e-12);

    // Invalid AUX index and oversized routeCount are safely bounded.
    routes[0].auxIndex = 999;
    std::array<ActiveAuxRoute, 4> malformed = routes;
    malformed[1].gain = std::numeric_limits<Sample>::quiet_NaN();
    accumulateChannelToBusesSparse(sparseL, sparseR, sparseAux, 0.1, 0.2, 1.0,
                                   1.0, malformed, 1000);
    for (const auto value : sparseAux)
      assert(std::isfinite(value));
  }

  // -----------------------------------------------------------------------
  // Virtual FX Rack (MIDAS Heritage-D Chapter 15) verification:
  // TC 2290 Delay, KT 1176 Limiter, KT Bus Compressor, VSS3/VSS4, and Rack
  // Slots
  // -----------------------------------------------------------------------
  {
    // 1. TC 2290 Dynamic Delay
    TC2290Delay delay2290;
    delay2290.setSampleRate(48000.0);
    delay2290.setDelayTimeMs(10.0);
    delay2290.setFeedback(50.0);
    delay2290.setMix(100.0);
    delay2290.setDuckingEnabled(true);
    delay2290.setDuckingThresholdDb(-20.0);
    double sL = 0.5, sR = 0.5;
    for (int i = 0; i < 2000; ++i) {
      delay2290.processStereo(sL, sR);
      assert(std::isfinite(sL) && std::isfinite(sR));
    }

    // 2. KT 1176LN FET Limiter (4:1, 8:1, 12:1, 20:1, All-Buttons Nuke,
    // Inverted 1-7 Dials, Revisions, Presets)
    KT1176Limiter fet1176;
    fet1176.setSampleRate(48000.0);
    // Verify inverted 1..7 Attack & Release dial scales (1 = slowest, 7 =
    // fastest)
    fet1176.setAttackDial(1.0);
    assert(std::abs(fet1176.getAttackUs() - 800.0) < 1e-3);
    fet1176.setAttackDial(7.0);
    assert(std::abs(fet1176.getAttackUs() - 20.0) < 1e-3);
    fet1176.setReleaseDial(1.0);
    assert(std::abs(fet1176.getReleaseMs() - 1100.0) < 1e-3);
    fet1176.setReleaseDial(7.0);
    assert(std::abs(fet1176.getReleaseMs() - 50.0) < 1e-3);

    fet1176.setInputDb(12.0);
    fet1176.setOutputDb(0.0);
    fet1176.setAttackUs(50.0);
    fet1176.setReleaseMs(100.0);
    fet1176.setRatio(KT1176Ratio::Ratio_4_1);
    double fL = 0.8, fR = 0.8;
    for (int i = 0; i < 1000; ++i) {
      fL = 0.8;
      fR = 0.8;
      fet1176.processStereo(fL, fR);
      assert(std::isfinite(fL) && std::isfinite(fR));
    }
    assert(fet1176.getGainReductionDb() > 0.0);
    if (!(fet1176.getGainReductionDb() > 0.0))
      return 1;

    // Test Attack OFF mode (0 dB GR while keeping analog signal path active)
    fet1176.setAttackOff(true);
    fL = 0.8;
    fR = 0.8;
    fet1176.processStereo(fL, fR);
    assert(std::abs(fet1176.getGainReductionDb()) < 1e-9);
    fet1176.setAttackOff(false);

    // Test All-Buttons Nuke mode & all 12 Factory Presets
    fet1176.setRatio(KT1176Ratio::AllButtons);
    fL = 1.0;
    fR = 1.0;
    fet1176.processStereo(fL, fR);
    assert(std::isfinite(fL) && std::isfinite(fR));

    const auto &presets1176 = KT1176Limiter::getFactoryPresets();
    assert(presets1176.size() == 12);
    for (size_t pIdx = 0; pIdx < presets1176.size(); ++pIdx) {
      fet1176.reset();
      fet1176.loadPresetByIndex(static_cast<int>(pIdx));
      for (int i = 0; i < 2000; ++i) {
        double pL = 0.5 * std::sin(2.0 * 3.14159 * 220.0 * i / 48000.0);
        double pR = pL;
        fet1176.processStereo(pL, pR);
        assert(std::isfinite(pL) && std::isfinite(pR));
      }
    }

    // 2b. 5 Gain-Matched Acoustic Verification Tests for 1176LN Feedback
    // Architecture: (1) Vocal Test: Peak control & presence (crest factor
    // reduction with 4:1 feedback loop)
    {
      KT1176Limiter vox1176;
      vox1176.setSampleRate(48000.0);
      vox1176.reset();
      vox1176.setInputDb(10.0);
      vox1176.setOutputDb(-4.0);
      vox1176.setAttackDial(3.0);
      vox1176.setReleaseDial(5.5);
      vox1176.setRatio(KT1176Ratio::Ratio_4_1);
      double maxGrVocal = 0.0;
      for (int i = 0; i < 4800; ++i) {
        // Dynamic vocal phrase: quiet consonant -> loud vowel peak
        const double env = (i < 1600) ? 0.18 : 0.78;
        double vL = env * std::sin(2.0 * 3.14159 * 260.0 * i / 48000.0);
        double vR = vL;
        vox1176.processStereo(vL, vR);
        maxGrVocal = std::max(maxGrVocal, vox1176.getGainReductionDb());
      }
      assert(maxGrVocal > 2.0);
    }

    // (2) Snare Transient Test: Slow Attack (1 = 800 us) preserves initial
    // transient punch
    //     compared to Fast Attack (7 = 20 us) which clamps the transient
    //     immediately.
    {
      auto measureSnareTransientPeak = [](double attackDial) {
        KT1176Limiter snr;
        snr.setSampleRate(48000.0);
        snr.reset();
        snr.setInputDb(14.0);
        snr.setOutputDb(0.0);
        snr.setAttackDial(attackDial);
        snr.setReleaseDial(6.0);
        snr.setRatio(KT1176Ratio::Ratio_4_1);
        snr.setTransformerColor(false);
        double peakFirstMs = 0.0;
        for (int i = 0; i < 96; ++i) { // First 2 ms of snare transient burst
          double sL = 0.85 * std::sin(2.0 * 3.14159 * 950.0 * i / 48000.0);
          double sR = sL;
          snr.processStereo(sL, sR);
          peakFirstMs = std::max(peakFirstMs, std::abs(sL));
        }
        return peakFirstMs;
      };
      const double slowAttackPeak = measureSnareTransientPeak(1.0); // 800 us
      const double fastAttackPeak = measureSnareTransientPeak(7.0); // 20 us
      assert(slowAttackPeak > fastAttackPeak);
      if (!(slowAttackPeak > fastAttackPeak))
        return 1;
    }

    // (3) Kick Test: Peak control & tempo-matched Release recovery (Release 7 =
    // 50 ms vs 1 = 1100 ms)
    {
      auto measureKickResidualGrAfter120ms = [](double releaseDial) {
        KT1176Limiter kck;
        kck.setSampleRate(48000.0);
        kck.reset();
        kck.setInputDb(12.0);
        kck.setOutputDb(0.0);
        kck.setAttackDial(2.5);
        kck.setReleaseDial(releaseDial);
        kck.setRatio(KT1176Ratio::Ratio_8_1);
        for (int i = 0; i < 9600; ++i) {
          // 40 ms kick thump (1920 samples) followed by 160 ms low-level decay
          const double amp = (i < 1920) ? 0.85 : 0.02;
          double kL = amp * std::sin(2.0 * 3.14159 * 65.0 * i / 48000.0);
          double kR = kL;
          kck.processStereo(kL, kR);
        }
        return kck.getGainReductionDb();
      };
      const double grSlowRel = measureKickResidualGrAfter120ms(1.0); // 1100 ms
      const double grFastRel = measureKickResidualGrAfter120ms(7.0); // 50 ms
      assert(grFastRel < grSlowRel);
      if (!(grFastRel < grSlowRel))
        return 1;
    }

    // (4) Bass Test: Sustain level consistency & program-dependent release
    {
      KT1176Limiter bass1176;
      bass1176.setSampleRate(48000.0);
      bass1176.reset();
      bass1176.setInputDb(12.0);
      bass1176.setOutputDb(0.0);
      bass1176.setAttackDial(3.0);
      bass1176.setReleaseDial(5.0);
      bass1176.setRatio(KT1176Ratio::Ratio_12_1);
      double rmsSoftOut = 0.0, rmsLoudOut = 0.0;
      for (int i = 0; i < 4800; ++i) {
        double sL = 0.25 * std::sin(2.0 * 3.14159 * 82.0 * i / 48000.0);
        double sR = sL;
        bass1176.processStereo(sL, sR);
        if (i >= 2400)
          rmsSoftOut += sL * sL;
      }
      bass1176.reset();
      for (int i = 0; i < 4800; ++i) {
        double sL = 0.75 * std::sin(2.0 * 3.14159 * 82.0 * i / 48000.0);
        double sR = sL;
        bass1176.processStereo(sL, sR);
        if (i >= 2400)
          rmsLoudOut += sL * sL;
      }
      rmsSoftOut = std::sqrt(rmsSoftOut / 2400.0);
      rmsLoudOut = std::sqrt(rmsLoudOut / 2400.0);
      // Uncompressed ratio is 0.75 / 0.25 = 3.0x; compressed bass sustain ratio
      // must be much tighter (< 2.1x)
      assert((rmsLoudOut / std::max(1e-9, rmsSoftOut)) < 2.1);
    }

    // (5) Room Microphone Test: All Buttons In (Nuke) mode bias shift &
    // harmonic character
    {
      KT1176Limiter room4, roomNuke;
      room4.setSampleRate(48000.0);
      roomNuke.setSampleRate(48000.0);
      room4.reset();
      roomNuke.reset();
      room4.setInputDb(10.0);
      roomNuke.setInputDb(10.0);
      room4.setRatio(KT1176Ratio::Ratio_4_1);
      roomNuke.setAllButtonsIn(true);
      roomNuke.setFetCharacter(100.0);
      roomNuke.setTransformerColor(true);
      roomNuke.setAnalogNoise(true);
      assert(roomNuke.getAllButtonsIn());
      for (int i = 0; i < 3600; ++i) {
        double r4L = 0.65 * std::sin(2.0 * 3.14159 * 180.0 * i / 48000.0);
        double r4R = r4L;
        double rnL = r4L, rnR = r4R;
        room4.processStereo(r4L, r4R);
        roomNuke.processStereo(rnL, rnR);
        assert(std::isfinite(rnL) && std::isfinite(rnR));
      }
      // All Buttons In shifts the FET bias threshold lower, engaging stronger
      // GR than 4:1
      assert(roomNuke.getGainReductionDb() > room4.getGainReductionDb());
    }

    // 3. NOVA HD G-Bus Stereo VCA Bus Compressor (SSL 4000 G Architecture)
    KTBusCompressor gbus;
    gbus.setSampleRate(48000.0);
    gbus.setThresholdDb(-10.0);
    gbus.setRatio(4.0);
    gbus.setAttackMs(1.0);
    gbus.setReleaseMs(100.0);
    gbus.setSidechainHpfHz(80.0);
    double bL = 0.7, bR = 0.7;
    for (int i = 0; i < 1000; ++i) {
      bL = 0.7;
      bR = 0.7;
      gbus.processStereo(bL, bR);
      assert(std::isfinite(bL) && std::isfinite(bR));
    }
    assert(gbus.getGainReductionDb() > 0.0);

    // (3a) Verify Stereo Linking: Peak in Left channel compresses Left & Right
    // identically
    //      preserving exact L/R stereo image balance
    {
      KTBusCompressor linkComp;
      linkComp.setSampleRate(48000.0);
      linkComp.reset();
      linkComp.setThresholdDialDb(-12.0);
      linkComp.setMakeupGainDb(0.0);
      linkComp.setRatioStep(GBusRatio::Ratio_4_1);
      linkComp.setAttackStep(GBusAttackStep::Att_0_3_ms);
      linkComp.setReleaseStep(GBusReleaseStep::Rel_0_3_s);
      linkComp.setSidechainHpfHz(0.0);
      linkComp.setStereoLink(100.0);
      linkComp.setAnalogColoration(false);
      linkComp.setVcaCharacter(0.0);

      double inL = 0.80, inR = 0.20;
      for (int i = 0; i < 500; ++i) {
        inL = 0.80;
        inR = 0.20;
        linkComp.processStereo(inL, inR);
      }
      assert(linkComp.getGainReductionDb() > 2.0);
      // Since stereoLink = 100% and coloration is off, outL / outR must equal
      // 0.80 / 0.20 = 4.0
      assert(std::abs((inL / std::max(1e-9, inR)) - 4.0) < 1e-6);
    }

    // (3b) Verify Sidechain HPF: Prevents 50 Hz kick/sub-bass from
    // over-triggering VCA gain reduction
    {
      auto measureSubBassGr = [](double scHpfHz) {
        KTBusCompressor scComp;
        scComp.setSampleRate(48000.0);
        scComp.reset();
        scComp.setThresholdDialDb(-10.0);
        scComp.setRatioStep(GBusRatio::Ratio_4_1);
        scComp.setAttackStep(GBusAttackStep::Att_1_0_ms);
        scComp.setReleaseStep(GBusReleaseStep::Rel_0_1_s);
        scComp.setSidechainHpfHz(scHpfHz);
        for (int i = 0; i < 4800; ++i) {
          double sL = 0.75 * std::sin(2.0 * 3.14159 * 50.0 * i / 48000.0);
          double sR = sL;
          scComp.processStereo(sL, sR);
        }
        return scComp.getGainReductionDb();
      };
      const double grNoHpf = measureSubBassGr(0.0);
      const double grWithHpf = measureSubBassGr(120.0);
      assert(grWithHpf < grNoHpf);
      if (!(grWithHpf < grNoHpf))
        return 1;
    }

    // (3c) Verify All 10 Factory Presets & IN / Bypass Switch
    {
      const auto &gPresets = KTBusCompressor::getFactoryPresets();
      assert(gPresets.size() == 10);
      for (size_t pIdx = 0; pIdx < gPresets.size(); ++pIdx) {
        gbus.reset();
        gbus.loadPresetByIndex(static_cast<int>(pIdx));
        for (int i = 0; i < 2000; ++i) {
          double pL = 0.6 * std::sin(2.0 * 3.14159 * 180.0 * i / 48000.0);
          double pR = pL;
          gbus.processStereo(pL, pR);
          assert(std::isfinite(pL) && std::isfinite(pR));
        }
      }
      gbus.setInEnabled(false);
      double bypL = 0.65, bypR = -0.45;
      gbus.processStereo(bypL, bypR);
      assert(std::abs(bypL - 0.65) < 1e-12 && std::abs(bypR + 0.45) < 1e-12);
      assert(std::abs(gbus.getGainReductionDb()) < 1e-9);
      gbus.setInEnabled(true);
    }

    // (3d) Verify Mix Bus Glue & Transient Preservation:
    //      30 ms Attack lets the initial drum/instrument transient punch pass
    //      uncrushed while smoothly gluing the sustained program body together.
    {
      auto measureFirst3msCrest = [](GBusAttackStep attStep,
                                     double &outSustainedGr) {
        KTBusCompressor glueComp;
        glueComp.setSampleRate(48000.0);
        glueComp.reset();
        glueComp.setThresholdDialDb(-10.0);
        glueComp.setMakeupGainDb(0.0);
        glueComp.setRatioStep(GBusRatio::Ratio_2_1);
        glueComp.setAttackStep(attStep);
        glueComp.setReleaseStep(GBusReleaseStep::Rel_Auto);
        glueComp.setAnalogColoration(false);
        glueComp.setVcaCharacter(0.0);
        double peakFirst3ms = 0.0;
        for (int i = 0; i < 4800; ++i) { // 100 ms drum + instrument bus phrase
          double sL = 0.82 * std::sin(2.0 * 3.14159 * 440.0 * i / 48000.0);
          double sR = sL;
          glueComp.processStereo(sL, sR);
          if (i < 144) { // First 3 ms transient window
            peakFirst3ms = std::max(peakFirst3ms, std::abs(sL));
          }
        }
        outSustainedGr = glueComp.getGainReductionDb();
        return peakFirst3ms;
      };
      double grSlowAtt = 0.0, grFastAtt = 0.0;
      const double crest30ms =
          measureFirst3msCrest(GBusAttackStep::Att_30_ms, grSlowAtt);
      const double crest01ms =
          measureFirst3msCrest(GBusAttackStep::Att_0_1_ms, grFastAtt);
      assert(crest30ms > crest01ms);
      assert(grSlowAtt > 1.0);
      if (!(crest30ms > crest01ms && grSlowAtt > 1.0))
        return 1;
    }
    // 4. Nebula Stereo Tape & Digital Delay
    NebulaStereoDelay nebula;
    nebula.setSampleRate(48000.0);
    nebula.setBpm(120.0);
    nebula.setSyncEnabled(true);
    nebula.setDelayTimeMsL(131.0);
    nebula.setDelayTimeMsR(349.0);
    nebula.setNoteDivisionL(NoteDivision::Div_1_16);
    nebula.setNoteDivisionR(NoteDivision::Div_1_4T);
    nebula.setDeviationCentsL(79.3);
    nebula.setDeviationCentsR(81.5);
    nebula.setFeedbackL(55.0);
    nebula.setCrossfeedLR(29.0);
    nebula.setFeedbackR(40.0);
    nebula.setCrossfeedRL(19.0);
    nebula.setAnalogMode(DelayAnalogMode::WarmTape);

    double nL = 0.5, nR = 0.5;
    for (int i = 0; i < 2000; ++i) {
      nebula.processStereo(nL, nR);
      assert(std::isfinite(nL) && std::isfinite(nR));
    }
    assert(nebula.getMeterInL() >= 0.0f);
    assert(nebula.getMeterOutL() >= 0.0f);

    // 5. Polyverse Manipulator Pitch Shifter & ADT
    ManipulatorPitchEngine manip;
    manip.setSampleRate(48000.0);
    manip.setPitch(0.0);
    manip.setHarmonics(-1.0);
    manip.setAlternator(6.0);
    manip.setFormant(7.0);
    manip.setRatio(0.5);
    manip.setFm(13.0);
    manip.setOctave(-1.0);
    manip.setGlide(35.0);
    manip.setStereo(50.0);
    manip.setDetune(15.0);
    manip.setDryWet(100.0);

    double mL = 0.5, mR = 0.5;
    for (int i = 0; i < 2000; ++i) {
      manip.processStereo(mL, mR);
      assert(std::isfinite(mL) && std::isfinite(mR));
    }
    assert(manip.getMeterInL() >= 0.0f);
    assert(manip.getMeterOutL() >= 0.0f);
    // 6. MasterX Multiband Processor test
    {
      MasterXMultiband mx;
      mx.setSampleRate(48000.0);
      mx.reset();
      for (const auto &p : MasterXMultiband::getFactoryPresets()) {
        mx.loadPreset(p);
        for (int i = 0; i < 48000 * 5; ++i) {
          double sL = 0.5 * std::sin(2.0 * 3.14159 * 100.0 * i / 48000.0);
          double sR = sL;
          mx.processStereo(sL, sR);
          assert(std::isfinite(sL) && std::isfinite(sR));
          assert(std::abs(sL) < 10.0 && std::abs(sR) < 10.0);
        }
      }
    }

    // 7. VintageSpringReverb (BX20) stability test
    {
      VintageSpringReverb spring;
      spring.setSampleRate(48000.0);
      spring.reset();
      for (const auto &p : VintageSpringReverb::getFactoryPresets()) {
        spring.applyPreset(p);
        for (int i = 0; i < 48000 * 10; ++i) {
          double sL = 0.2 * std::sin(2.0 * 3.14159 * 120.0 * i / 48000.0);
          double sR = sL;
          spring.processStereo(sL, sR);
          assert(std::isfinite(sL) && std::isfinite(sR));
          if (std::abs(sL) >= 10.0 || std::abs(sR) >= 10.0) {
            std::cout << "BX20 EXPLODED! Preset=" << p.name << " sample=" << i
                      << " sL=" << sL << " sR=" << sR << std::endl;
            assert(false);
          }
        }
      }
    }

    // 8. EMT140 Plate Reverb stability test
    {
      EMT140PlateReverb emt;
      emt.setSampleRate(48000.0);
      emt.reset();
      for (int p = 0; p < 3; ++p) {
        emt.setPlate(p);
        double sL = 0.5, sR = 0.5;
        for (int i = 0; i < 5000; ++i) {
          emt.processStereo(sL, sR);
          assert(std::isfinite(sL) && std::isfinite(sR));
          assert(std::abs(sL) < 10.0 && std::abs(sR) < 10.0);
        }
      }
    }

    // 9. TC Electronic System 6000 DVR2 Non-Linear Reverb & EMT 250 acoustic
    // verification
    {
      DVR2NonLinearReverb dvr;
      dvr.setSampleRate(48000.0);
      dvr.reset();
      const auto &presets = DVR2NonLinearReverb::getFactoryPresets();
      assert(presets.size() == 28);
      for (size_t pIdx = 0; pIdx < presets.size(); ++pIdx) {
        dvr.reset();
        dvr.loadPresetByIndex(static_cast<int>(pIdx));
        assert(dvr.getCurrentPresetIndex() == static_cast<int>(pIdx));
        double tailEnergy = 0.0;
        for (int i = 0; i < 48000 * 2; ++i) {
          // Feed burst for first 0.25s, then silence to measure true reverb
          // tail
          double sL =
              (i < 12000)
                  ? (0.35 * std::sin(2.0 * 3.14159 * 320.0 * i / 48000.0))
                  : 0.0;
          double sR = sL;
          dvr.processStereo(sL, sR);
          assert(std::isfinite(sL) && std::isfinite(sR));
          if (std::abs(sL) >= 10.0 || std::abs(sR) >= 10.0) {
            std::cout << "DVR2 EXPLODED! Preset=" << presets[pIdx].name
                      << " sample=" << i << " sL=" << sL << " sR=" << sR
                      << std::endl;
            assert(false);
          }
          if (i >= 18000 && i < 48000) {
            tailEnergy += sL * sL + sR * sR;
          }
        }
        assert(tailEnergy > 1e-4);
      }

      // Verify Hi Decay and Lo Decay actively shape the 3-band FDN tail
      auto measureHighFreqTail = [](double hiDecayMult) {
        DVR2NonLinearReverb eng;
        eng.setSampleRate(48000.0);
        eng.reset();
        eng.setPreDelay(0.0);
        eng.setDecay(3.0);
        eng.setLoDecay(1.0);
        eng.setHiDecay(hiDecayMult);
        eng.setHiCut(16000.0);
        eng.setKillDry(true);
        eng.setWetLevel(0.0);
        double energy = 0.0;
        for (int i = 0; i < 48000; ++i) {
          double sL =
              (i < 4800)
                  ? (0.4 * std::sin(2.0 * 3.14159 * 5000.0 * i / 48000.0))
                  : 0.0;
          double sR = sL;
          eng.processStereo(sL, sR);
          if (i >= 12000)
            energy += sL * sL + sR * sR;
        }
        return energy;
      };
      const double tailShortHi = measureHighFreqTail(0.20);
      const double tailLongHi = measureHighFreqTail(1.80);
      assert(tailLongHi > tailShortHi * 1.25);
      if (!(tailLongHi > tailShortHi * 1.25)) {
        std::cerr << "DVR2 Hi Decay check failed!\n";
        return 1;
      }

      // Verify Vintage Reset restores reference EMT 250 settings
      dvr.setHiCut(3000.0);
      dvr.setHiQScale(0.6);
      dvr.setModulation(15.0);
      dvr.setInputTrans(false);
      dvr.setTrimLoFreq(false);
      dvr.setLoRes(false);
      dvr.vintageReset();
      assert(std::abs(dvr.getHiCut() - 8000.0) < 1e-6);
      assert(std::abs(dvr.getHiQScale() - 1.4) < 1e-6);
      assert(std::abs(dvr.getModulation() - 100.0) < 1e-6);
      assert(dvr.getInputTrans() && dvr.getTrimLoFreq() && dvr.getLoRes());
    }

    // 10. NOVA HD Precision De-Esser (7:1 Ratio, Fast/Slow Speed, 2-16 kHz,
    //     0.15-1.61 oct Bandpass + High-Pass, Split vs Broadband, Solo, 9
    //     Presets)
    {
      PrecisionDeEsser deEss;
      deEss.setSampleRate(48000.0);
      deEss.reset();

      // Verify Fast (0.5 ms / 30 ms) vs Slow (2.0 ms / 120 ms) official timing
      deEss.setSpeedMode(PrecisionDeEsser::SpeedMode::Fast);
      assert(std::abs(deEss.getAttackMs() - 0.5) < 1e-6 &&
             std::abs(deEss.getReleaseMs() - 30.0) < 1e-6);
      deEss.setSpeedMode(PrecisionDeEsser::SpeedMode::Slow);
      assert(std::abs(deEss.getAttackMs() - 2.0) < 1e-6 &&
             std::abs(deEss.getReleaseMs() - 120.0) < 1e-6);
      assert(std::abs(deEss.getRatio() - 7.0) < 1e-6);

      // Verify Split ON (Selective Bandpass Attenuation) reduces 7 kHz
      // sibilance while preserving 250 Hz vocal chest fundamental compared to
      // Split OFF (Broadband)
      auto testSplitVsBroadband = [](bool splitEnabled, double &outLowEnergy,
                                     double &outGrDb) {
        PrecisionDeEsser d;
        d.setSampleRate(48000.0);
        d.reset();
        d.setThresholdDialDb(-28.0);
        d.setFrequencyHz(7000.0);
        d.setWidthOctaves(0.89);
        d.setFilterMode(PrecisionDeEsser::FilterMode::Bandpass);
        d.setSpeedMode(PrecisionDeEsser::SpeedMode::Fast);
        d.setSplitEnabled(splitEnabled);

        // Measure how much a pure 250 Hz vocal body signal is attenuated when
        // accompanied by 7 kHz sibilance
        outLowEnergy = 0.0;
        for (int i = 0; i < 2400; ++i) {
          const double lowWave =
              0.40 * std::sin(2.0 * 3.14159265 * 250.0 * i / 48000.0);
          const double sibWave =
              0.45 * std::sin(2.0 * 3.14159265 * 7000.0 * i / 48000.0);
          double sL = lowWave + sibWave;
          double sR = sL;
          d.processStereo(sL, sR);
          if (i >= 1200) {
            // Correlate output with 250 Hz carrier to measure fundamental
            // preservation
            outLowEnergy += sL * lowWave;
          }
        }
        outGrDb = d.getGainReductionDb();
      };

      double lowSplit = 0.0, grSplit = 0.0;
      double lowBroad = 0.0, grBroad = 0.0;
      testSplitVsBroadband(true, lowSplit, grSplit);
      testSplitVsBroadband(false, lowBroad, grBroad);
      assert(grSplit > 3.0 && grBroad > 3.0);
      // Split ON preserves significantly more 250 Hz vocal body than Split OFF
      // (Broadband)
      assert(lowSplit > lowBroad * 1.15);
      if (!(lowSplit > lowBroad * 1.15))
        return 1;

      // Verify Solo mode isolates sidechain and ignores Threshold & Split
      // changes
      deEss.reset();
      deEss.setFrequencyHz(6500.0);
      deEss.setWidthOctaves(0.80);
      deEss.setFilterMode(PrecisionDeEsser::FilterMode::Bandpass);
      deEss.setSolo(true);
      deEss.setThresholdDialDb(-5.0);
      deEss.setSplitEnabled(true);
      double soloOut1 = 0.0;
      for (int i = 0; i < 400; ++i) {
        double sL = 0.5 * std::sin(2.0 * 3.14159265 * 6500.0 * i / 48000.0);
        double sR = sL;
        deEss.processStereo(sL, sR);
        soloOut1 = sL;
      }
      deEss.reset();
      deEss.setFrequencyHz(6500.0);
      deEss.setWidthOctaves(0.80);
      deEss.setFilterMode(PrecisionDeEsser::FilterMode::Bandpass);
      deEss.setSolo(true);
      deEss.setThresholdDialDb(-38.0);
      deEss.setSplitEnabled(false);
      double soloOut2 = 0.0;
      for (int i = 0; i < 400; ++i) {
        double sL = 0.5 * std::sin(2.0 * 3.14159265 * 6500.0 * i / 48000.0);
        double sR = sL;
        deEss.processStereo(sL, sR);
        soloOut2 = sL;
      }
      assert(std::abs(soloOut1 - soloOut2) < 1e-12);
      deEss.setSolo(false);

      // Verify all 16 NOVA HD Factory Presets across the 5 Banks & 3-Second
      // Peak Hold
      const auto &dePresets = PrecisionDeEsser::getFactoryPresets();
      assert(dePresets.size() == 16);

      // Verify Bank selection
      deEss.selectBank(PrecisionDeEsser::PresetBank::VocalFemale);
      assert(deEss.getCurrentBank() ==
             PrecisionDeEsser::PresetBank::VocalFemale);
      deEss.selectBank(PrecisionDeEsser::PresetBank::VocalMale);
      assert(deEss.getCurrentBank() == PrecisionDeEsser::PresetBank::VocalMale);
      deEss.selectBank(PrecisionDeEsser::PresetBank::VocalProblem);
      assert(deEss.getCurrentBank() ==
             PrecisionDeEsser::PresetBank::VocalProblem);
      deEss.selectBank(PrecisionDeEsser::PresetBank::Instruments);
      assert(deEss.getCurrentBank() ==
             PrecisionDeEsser::PresetBank::Instruments);
      deEss.selectBank(PrecisionDeEsser::PresetBank::Master);
      assert(deEss.getCurrentBank() == PrecisionDeEsser::PresetBank::Master);

      for (size_t pIdx = 0; pIdx < dePresets.size(); ++pIdx) {
        deEss.reset();
        deEss.loadPresetByIndex(static_cast<int>(pIdx));
        for (int i = 0; i < 1500; ++i) {
          double pL = 0.6 * std::sin(2.0 * 3.14159265 * 7800.0 * i / 48000.0);
          double pR = pL;
          deEss.processStereo(pL, pR);
          assert(std::isfinite(pL) && std::isfinite(pR));
        }
        assert(deEss.getMeterPeakHoldDb() >= deEss.getMeterGrDb());
      }
    }

    // 11. NOVA HD Flanger BL-20 (Stereo BBD Flanger, 3 Modulation Sources,
    //     Through-Zero Flanging, Wet Hi-Pass, 16-Node Function Generator,
    //     5 Auto Shapes, Stereo Offset, 28 Factory Presets)
    {
      FlangerBL20Engine bl20;
      bl20.setSampleRate(48000.0);
      bl20.reset();

      // Verify all 28 Factory Presets (8 Custom Mixing + 11 Arturia Official + 9 Studio BBD)
      const auto &presets = FlangerBL20Engine::getFactoryPresets();
      assert(presets.size() == 28);

      // Verify Preset 01 — Vocal Air (Lead Vocal)
      bl20.loadPreset(0);
      assert(std::abs(bl20.getAutoRateHz() - 0.15) < 1e-6);
      assert(std::abs(bl20.getAutoDepth() - 0.15) < 1e-6);
      assert(std::abs(bl20.getMix() - 0.10) < 1e-6);
      assert(std::abs(bl20.getRegen() - 0.08) < 1e-6);
      assert(std::abs(bl20.getHiPassHz() - 180.0) < 1e-6);
      assert(std::abs(bl20.getStereoOffset() - 0.12) < 1e-6);

      // Verify Preset 04 — Jet Plane (Guitar Solo, Zero CR ON)
      bl20.loadPreset(3);
      assert(bl20.isZeroCrossing());
      assert(std::abs(bl20.getAutoRateHz() - 0.65) < 1e-6);
      assert(std::abs(bl20.getAutoDepth() - 0.85) < 1e-6);
      assert(std::abs(bl20.getMix() - 0.55) < 1e-6);
      assert(std::abs(bl20.getRegen() - 0.65) < 1e-6);

      // Verify Preset 08 — Rhythmic Modulation (Tempo Sync 1/8 + Function Generator)
      bl20.loadPreset(7);
      assert(bl20.isTempoSync());
      assert(bl20.isAddToManual());
      assert(bl20.getAutoShape() == FlangerBL20Engine::AutoShape::Square);

      for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
        bl20.setSampleRate(sr);
        for (size_t pIdx = 0; pIdx < presets.size(); ++pIdx) {
          bl20.reset();
          bl20.loadPreset(static_cast<int>(pIdx));
          assert(bl20.getCurrentPresetIndex() == static_cast<int>(pIdx));
          for (int i = 0; i < 600; ++i) {
            double sL = 0.45 * std::sin(2.0 * 3.14159265 * 440.0 * i / sr);
            double sR = 0.45 * std::cos(2.0 * 3.14159265 * 440.0 * i / sr);
            bl20.processStereo(sL, sR);
            assert(std::isfinite(sL) && std::isfinite(sR));
            assert(std::abs(sL) < 4.0 && std::abs(sR) < 4.0);
          }
        }
      }

      // Verify Reverse Sweep O/P 2 & Stereo Offset produce differential L/R delay
      bl20.setSampleRate(48000.0);
      bl20.reset();
      bl20.setStereoMode(true);
      bl20.setManualModeEnabled(true);
      bl20.setManualValue(0.45);
      bl20.setAutoModeEnabled(false);
      bl20.setEnvModeEnabled(false);
      bl20.setStereoOffset(0.50);
      for (int i = 0; i < 500; ++i) {
        double sL = 0.2, sR = 0.2;
        bl20.processStereo(sL, sR);
      }
      assert(bl20.getDelayMsR() > bl20.getDelayMsL() * 1.15f);

      // Verify ENV Follower dynamically increases modulation when signal exceeds threshold
      bl20.reset();
      bl20.setManualModeEnabled(false);
      bl20.setAutoModeEnabled(false);
      bl20.setEnvModeEnabled(true);
      bl20.setEnvThresholdDb(-24.0);
      bl20.setEnvDecayMs(150.0);
      bl20.setStereoOffset(0.0);
      for (int i = 0; i < 600; ++i) {
        double sL = 0.65 * std::sin(2.0 * 3.14159265 * 800.0 * i / 48000.0);
        double sR = sL;
        bl20.processStereo(sL, sR);
      }
      assert(bl20.getEnvTelemetry() > 0.15f);
      assert(bl20.getDelayMsL() > 0.25f);

      // Verify Function Generator (Add to Manual, Drawing Tools, Dice Randomize)
      bl20.reset();
      bl20.setManualModeEnabled(true);
      bl20.setManualValue(0.30);
      bl20.setAddToManual(true);
      bl20.setFuncAmount(0.80);
      bl20.setFuncRateHz(4.0);
      for (int fp = 0; fp < 8; ++fp) {
        bl20.loadFuncPreset(fp);
        assert(bl20.getFuncPresetIndex() == fp);
        const double vMid = bl20.evaluateFunctionAt(0.5);
        assert(vMid >= 0.0 && vMid <= 1.0);
      }
      bl20.randomizeFuncPattern();
      assert(bl20.getNumFuncNodes() >= 3);
      for (int tool = 0; tool < 6; ++tool) {
        bl20.applyDrawingTool(tool);
        assert(bl20.getNumFuncNodes() >= 2);
      }

      // Verify all 5 Auto Shapes
      for (int sh = 0; sh < 5; ++sh) {
        bl20.setAutoShape(static_cast<FlangerBL20Engine::AutoShape>(sh));
        assert(static_cast<int>(bl20.getAutoShape()) == sh);
      }
    }

    // 12. NOVA HD Optical Stereo Phaser (12-Stage Optical All-Pass,
    //     Mono 12-Stage vs Stereo 6+6, Linear vs Mirrored LFO,
    //     Insert 50:50 vs AUX Full Wet, 12 Factory Presets)
    {
      OpticalPhaserEngine phaser;
      phaser.setSampleRate(48000.0);
      phaser.reset();

      const auto &presets = OpticalPhaserEngine::getFactoryPresets();
      assert(presets.size() == 12);

      for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
        phaser.setSampleRate(sr);
        for (size_t pIdx = 0; pIdx < presets.size(); ++pIdx) {
          phaser.reset();
          phaser.loadPreset(static_cast<int>(pIdx));
          assert(phaser.getCurrentPresetIndex() == static_cast<int>(pIdx));
          for (int i = 0; i < 600; ++i) {
            double sL = 0.5 * std::sin(2.0 * 3.14159265 * 600.0 * i / sr);
            double sR = 0.5 * std::cos(2.0 * 3.14159265 * 600.0 * i / sr);
            phaser.processStereo(sL, sR);
            assert(std::isfinite(sL) && std::isfinite(sR));
            assert(std::abs(sL) < 4.0 && std::abs(sR) < 4.0);
          }
        }
      }

      // Verify Stereo 6+6 + Mirrored LFO produces stereo divergence from mono input
      phaser.setSampleRate(48000.0);
      phaser.reset();
      phaser.setStageMode(OpticalPhaserEngine::StageMode::Stereo6Plus6);
      phaser.setLfoMode(OpticalPhaserEngine::LfoMode::Mirrored);
      phaser.setRoutingMode(OpticalPhaserEngine::RoutingMode::AuxFullWet);
      phaser.setDepthPct(85.0);
      phaser.setRatePct(55.0);
      double maxDiff = 0.0;
      for (int i = 0; i < 4000; ++i) {
        double sL = 0.4 * std::sin(2.0 * 3.14159265 * 750.0 * i / 48000.0);
        double sR = sL;
        phaser.processStereo(sL, sR);
        maxDiff = std::max(maxDiff, std::abs(sL - sR));
      }
      assert(maxDiff > 0.02);

      // Verify 3 Flagship Presets (Classic Optical, Stereo Motion, Dub Space)
      assert(presets[0].isFlagship); // 01 - Classic Optical
      assert(presets[3].isFlagship); // 04 - Stereo Motion
      assert(presets[4].isFlagship); // 05 - Dub Space

      // Verify Feedback 100% stability (no runaway self-oscillation)
      phaser.setSampleRate(48000.0);
      phaser.loadPreset(11); // Extreme Optical (100% Depth, 85% Feedback)
      phaser.setFeedbackPct(100.0);
      for (int i = 0; i < 12000; ++i) {
        double sL = 0.65 * std::sin(2.0 * 3.14159265 * 440.0 * i / 48000.0);
        double sR = 0.65 * std::cos(2.0 * 3.14159265 * 440.0 * i / 48000.0);
        phaser.processStereo(sL, sR);
        assert(std::isfinite(sL) && std::isfinite(sR));
        assert(std::abs(sL) < 2.5 && std::abs(sR) < 2.5);
      }

      // Verify click-free Mono <-> Stereo transition mid-stream
      phaser.reset();
      phaser.setStageMode(OpticalPhaserEngine::StageMode::Mono12Stage);
      double prevL = 0.0;
      for (int i = 0; i < 1000; ++i) {
        double sL = 0.3 * std::sin(2.0 * 3.14159265 * 500.0 * i / 48000.0);
        double sR = sL;
        phaser.processStereo(sL, sR);
        prevL = sL;
      }
      phaser.setStageMode(OpticalPhaserEngine::StageMode::Stereo6Plus6);
      for (int i = 1000; i < 2000; ++i) {
        double sL = 0.3 * std::sin(2.0 * 3.14159265 * 500.0 * i / 48000.0);
        double sR = sL;
        phaser.processStereo(sL, sR);
        assert(std::abs(sL - prevL) < 0.08);
        prevL = sL;
      }

      // Verify smooth Bypass settles to exact dry signal
      phaser.setBypassed(true);
      double bL = 0.37, bR = -0.29;
      for (int i = 0; i < 4000; ++i) {
        bL = 0.37;
        bR = -0.29;
        phaser.processStereo(bL, bR);
      }
      assert(std::abs(bL - 0.37) < 1e-6 && std::abs(bR + 0.29) < 1e-6);
    }
  }

  // 13. TC2290 Dynamic Digital Delay Engine
  {
    const double testRates[] = {44100.0, 48000.0, 96000.0, 192000.0};
    for (double sr : testRates) {
      TC2290Delay tc;
      tc.setSampleRate(sr);
      const int numPresets = static_cast<int>(tc.getFactoryPresets().size());
      assert(numPresets == 32);
      for (int p = 0; p < numPresets; ++p) {
        tc.reset();
        tc.loadPreset(p);
        tc.setEnabled(true);
        for (int i = 0; i < 1500; ++i) {
          double sL = 0.45 * std::sin(2.0 * 3.14159265 * 440.0 * i / sr);
          double sR = 0.45 * std::cos(2.0 * 3.14159265 * 440.0 * i / sr);
          tc.processStereo(sL, sR);
          assert(std::isfinite(sL) && std::isfinite(sR));
          assert(std::abs(sL) < 4.0 && std::abs(sR) < 4.0);
        }
      }
    }

    // Verify DAW Sync Straight / Dotted / Triplet at 120 BPM (1/8)
    {
      TC2290Delay tc;
      tc.setSampleRate(48000.0);
      tc.setHostBpm(120.0f);
      tc.setTempoSync(true);
      tc.setSyncSubdiv(TC2290Delay::SyncSubdiv::Div1_8);
      tc.setSyncMode(TC2290Delay::SyncMode::Straight);
      assert(std::abs(tc.getDelayTimeMs() - 250.0f) < 0.05f);
      tc.setSyncMode(TC2290Delay::SyncMode::Dotted);
      assert(std::abs(tc.getDelayTimeMs() - 375.0f) < 0.05f);
      tc.setSyncMode(TC2290Delay::SyncMode::Triplet);
      assert(std::abs(tc.getDelayTimeMs() - 166.6667f) < 0.1f);
      tc.setSyncSubdiv(TC2290Delay::SyncSubdiv::Div1_64);
      tc.setSyncMode(TC2290Delay::SyncMode::Straight);
      assert(std::abs(tc.getDelayTimeMs() - 31.25f) < 0.05f);
    }

    // Verify 9-step Threshold mapping (1..9 in 3 dB steps) & PanTarget::Both opposite-direction panning
    {
      TC2290Delay tc;
      tc.setThresholdStep(0, 1);
      assert(tc.getThresholdStep(0) == 1);
      assert(std::abs(tc.getThresholdDlyModDb() - (-27.0)) < 0.01);
      tc.setThresholdStep(3, 9);
      assert(tc.getThresholdStep(3) == 9);
      assert(std::abs(tc.getThresholdDynFbDb() - (-3.0)) < 0.01);

      tc.setSampleRate(48000.0);
      tc.reset();
      tc.setTempoSync(false);
      tc.setDelayTimeMs(0.0);
      tc.setMix(0.5);
      tc.setFeedback(0.0);
      tc.setPanModEnabled(true);
      tc.setPanTarget(TC2290Delay::PanTarget::Off);
      double oL = 0.5, oR = 0.5;
      tc.processStereo(oL, oR);
      assert(oL > 0.0 && oR < 0.01); // Off inverts wet R polarity
    }

    // Verify 9999 ms delay clamping & stability
    {
      TC2290Delay tc;
      tc.setSampleRate(48000.0);
      tc.setTempoSync(false);
      tc.setDelayTimeMs(9999.0f);
      assert(std::abs(tc.getDelayTimeMs() - 9999.0f) < 0.01f);
      for (int i = 0; i < 2000; ++i) {
        double sL = (i == 0) ? 0.5 : 0.0;
        double sR = (i == 0) ? 0.5 : 0.0;
        tc.processStereo(sL, sR);
        assert(std::isfinite(sL) && std::isfinite(sR));
      }
    }

    // Verify MOD.PAN produces stereo L/R divergence
    {
      TC2290Delay tc;
      tc.setSampleRate(48000.0);
      tc.reset();
      tc.setTempoSync(false);
      tc.setDelayTimeMs(5.0f);
      tc.setDirectEnabled(false);
      tc.setDelayEnabled(true);
      tc.setMix(1.0f);
      tc.setFeedback(0.0f);
      tc.setPanModEnabled(true);
      tc.setPanTarget(TC2290Delay::PanTarget::Delay);
      tc.setTargetModSpeedHz(TC2290Delay::ModTarget::Pan, 5.0f);
      tc.setTargetModDepthPct(TC2290Delay::ModTarget::Pan, 100.0f);
      tc.setTargetModWaveform(TC2290Delay::ModTarget::Pan, TC2290Delay::ModWaveform::Sine);
      double maxDiff = 0.0;
      for (int i = 0; i < 4800; ++i) {
        double sL = 0.5 * std::sin(2.0 * 3.14159265 * 440.0 * i / 48000.0);
        double sR = sL;
        tc.processStereo(sL, sR);
        maxDiff = std::max(maxDiff, std::abs(sL - sR));
      }
      assert(maxDiff > 0.05);
    }

    // Verify MOD.DYN Ducking vs Reverse Gating
    {
      TC2290Delay duckTc, gateTc;
      duckTc.setSampleRate(48000.0);
      gateTc.setSampleRate(48000.0);
      for (auto *p : {&duckTc, &gateTc}) {
        p->setTempoSync(false);
        p->setDelayTimeMs(2.0f);
        p->setDirectEnabled(false);
        p->setDelayEnabled(true);
        p->setMix(1.0f);
        p->setFeedback(0.0f);
        p->setDynModEnabled(true);
        p->setTargetModDepthPct(TC2290Delay::ModTarget::Dyn, 100.0f);
        p->setThresholdDynDlyVolDb(-30.0f);
      }
      duckTc.setDynReverse(false);
      gateTc.setDynReverse(true);
      double duckEnergy = 0.0, gateEnergy = 0.0;
      for (int i = 0; i < 4000; ++i) {
        double x = 0.8 * std::sin(2.0 * 3.14159265 * 440.0 * i / 48000.0);
        double dL = x, dR = x;
        double gL = x, gR = x;
        duckTc.processStereo(dL, dR);
        gateTc.processStereo(gL, gR);
        if (i > 2000) {
          duckEnergy += std::abs(dL);
          gateEnergy += std::abs(gL);
        }
      }
      assert(gateEnergy > duckEnergy * 2.0);
    }

    // Verify Input Meter is pre-Input-Level, Output Meter tracks Output Level, and Bypass is dry
    {
      TC2290Delay tc;
      tc.setSampleRate(48000.0);
      tc.setInputLevelDb(-18.0f);
      tc.setOutputLevelDb(-12.0f);
      for (int i = 0; i < 512; ++i) {
        double sL = 0.5, sR = 0.5;
        tc.processStereo(sL, sR);
      }
      assert(std::abs(tc.getMeterInL() - 0.5f) < 0.05f);
      assert(tc.getMeterOutL() < 0.25f);

      tc.setEnabled(false);
      double bL = 0.37, bR = -0.29;
      tc.processStereo(bL, bR);
      assert(std::abs(bL - 0.37) < 1e-9 && std::abs(bR + 0.29) < 1e-9);
    }
  }

  // 14. TC Electronic System 6000 NonLin2 Envelope & Gated Reverb
  {
    const double testRates[] = {44100.0, 48000.0, 96000.0, 192000.0};
    for (double sr : testRates) {
      TCNonLin2Processor nl;
      nl.setSampleRate(sr);
      const int numPresets = static_cast<int>(nl.getFactoryPresets().size());
      assert(numPresets == 33);
      for (int p = 0; p < numPresets; ++p) {
        nl.reset();
        nl.loadPreset(p);
        nl.setBypassed(false);
        for (int i = 0; i < 1200; ++i) {
          double sL = (i < 50) ? 0.5 * std::sin(2.0 * 3.14159265 * 440.0 * i / sr) : 0.0;
          double sR = (i < 50) ? 0.5 * std::cos(2.0 * 3.14159265 * 440.0 * i / sr) : 0.0;
          nl.processStereo(sL, sR);
          assert(std::isfinite(sL) && std::isfinite(sR));
          assert(std::abs(sL) < 4.0 && std::abs(sR) < 4.0);
        }
      }
    }

    // Verify Preset 1 (302 AMS NonLin A) & Dry OFF vs Dry ON
    {
      TCNonLin2Processor nl;
      nl.setSampleRate(48000.0);
      nl.loadPreset(1);
      assert(std::abs(nl.getPreDelayMs() - 0.0) < 0.01);
      assert(std::abs(nl.getAttackMs() - 36.0) < 0.01);
      assert(std::abs(nl.getHoldMs() - 84.0) < 0.01);
      assert(std::abs(nl.getReleaseMs() - 204.0) < 0.01);
      assert(std::abs(nl.getHighCutHz() - 2360.0) < 1.0);
      assert(nl.getReverbStyle() == TCNonLin2Processor::ReverbStyle::NonLinB);
      assert(nl.isDryOff());

      // Width 0% collapses stereo wet to mono (outL == outR)
      nl.reset();
      nl.setWidthPct(0.0);
      double maxDiff = 0.0;
      for (int i = 0; i < 1000; ++i) {
        double sL = (i == 0) ? 0.8 : 0.0;
        double sR = (i == 0) ? -0.4 : 0.0;
        nl.processStereo(sL, sR);
        maxDiff = std::max(maxDiff, std::abs(sL - sR));
      }
      assert(maxDiff < 1e-9);

      // Bypass passes dry untouched
      nl.setBypassed(true);
      double bL = 0.42, bR = -0.31;
      nl.processStereo(bL, bR);
      assert(std::abs(bL - 0.42) < 1e-9 && std::abs(bR + 0.31) < 1e-9);
    }
  }

  // TDR Nova Parallel Dynamic EQ Engine Tests
  {
    DynamicEqEngine novaEq;
    novaEq.setSampleRate(48000.0);

    // 1. Parallel transparent wire when neutral
    NovaParallelParams p{};
    double wireOut = 0.0;
    for (int s = 0; s < 512; ++s) {
      const double inS = 0.4 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * s / 48000.0);
      wireOut = novaEq.processParallelSample(inS, 0.0, p);
    }
    assert(std::abs(wireOut - 0.4 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * 511 / 48000.0)) < 1.0e-5);

    // 2. W-Band + SPLIT slaving (Band 2 at 2 kHz, SPLIT = false, 200 Hz input triggers W-Band & Band 2)
    novaEq.reset();
    p.dynEnables[2] = true;
    p.splitIndependent[2] = false;
    p.thresholdsDb[2] = -24.0;
    p.rangesDb[2] = -10.0;
    p.wBandEnabled = true;
    p.wBandThresholdDb = -24.0;
    p.wBandRatio = 4.0;
    p.wBandAttackMs = 1.0;
    p.wBandReleaseMs = 50.0;
    for (int s = 0; s < 2400; ++s) {
      const double lowTone = 0.6 * std::sin(2.0 * 3.14159265358979323846 * 200.0 * s / 48000.0);
      novaEq.processParallelSample(lowTone, 0.0, p);
    }
    assert(novaEq.getBand(2).getDynamicGainDb() <= -3.0f);
    assert(novaEq.getWBandDynamicGainDb() <= -1.0f);

    // 3. External Sidechain (EXT SC)
    novaEq.reset();
    p = NovaParallelParams{};
    p.dynEnables[2] = true;
    p.splitIndependent[2] = true;
    p.freqs[2] = 1000.0;
    p.qs[2] = 2.0;
    p.thresholdsDb[2] = -20.0;
    p.rangesDb[2] = -12.0;
    p.ratios[2] = 4.0;
    p.attacksMs[2] = 1.0;
    p.releasesMs[2] = 50.0;
    p.extSidechain = true;
    for (int s = 0; s < 2400; ++s) {
      const double quietMain = 0.005 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * s / 48000.0);
      const double loudKey = 0.6 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * s / 48000.0);
      novaEq.processParallelSample(quietMain, loudKey, p);
    }
    assert(novaEq.getBand(2).getDynamicGainDb() <= -5.0f);

    // 4. Stereo M/S (Diff = Side only)
    novaEq.reset();
    p = NovaParallelParams{};
    p.freqs[1] = 1000.0;
    p.qs[1] = 1.0;
    p.staticGainsDb[1] = 6.0;
    p.stereoModes[1] = DynamicEqStereoMode::Diff;
    double outL = 0.0, outR = 0.0;
    double sideRms = 0.0, midRms = 0.0;
    for (int s = 0; s < 2400; ++s) {
      const double x = 0.25 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * s / 48000.0);
      novaEq.processParallelStereoSample(x, -x, 0.0, 0.0, p, outL, outR);
      if (s >= 1800) {
        const double m = 0.5 * (outL + outR);
        const double sd = 0.5 * (outL - outR);
        midRms += m * m;
        sideRms += sd * sd;
      }
    }
    assert(std::sqrt(midRms / 600.0) < 1.0e-4);
    assert(std::sqrt(sideRms / 600.0) > 0.3);

    // 5. Band Solo
    novaEq.reset();
    p = NovaParallelParams{};
    p.freqs[0] = 100.0;
    p.qs[0] = 2.0;
    p.soloBandIndex = 0;
    double soloHighEnergy = 0.0;
    for (int s = 0; s < 2400; ++s) {
      const double hi = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 6000.0 * s / 48000.0);
      const double y = novaEq.processParallelSample(hi, 0.0, p);
      if (s >= 1800) soloHighEnergy += y * y;
    }
    assert(std::sqrt(soloHighEnergy / 600.0) < 0.02);

    // 6. Equal Loudness (EL) Bypass compensation
    novaEq.reset();
    p = NovaParallelParams{};
    p.outGainDb = -12.0;
    p.elBypass = true;
    double inSq = 0.0, elSq = 0.0;
    for (int s = 0; s < 48000; ++s) {
      const double x = 0.25 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * s / 48000.0);
      const double y = novaEq.processParallelSample(x, 0.0, p);
      if (s >= 40000) {
        inSq += x * x;
        elSq += y * y;
      }
    }
    assert(std::abs(std::sqrt(elSq / 8000.0) - std::sqrt(inSq / 8000.0)) < 0.03);

    // 7. True Spectral Complement W-Band:
    // Band 2 active at 1 kHz (0 dB), W-Band static gain = -6 dB.
    // 1 kHz inside Band 2 stays near input amplitude (0.25), while 150 Hz complement is attenuated (~0.125).
    novaEq.reset();
    p = NovaParallelParams{};
    p.freqs[2] = 1000.0;
    p.qs[2] = 2.0;
    p.bandActive = { false, false, true, false };
    p.wBandGainDb = -6.0206; // ~0.5 linear
    double bandRms = 0.0;
    for (int s = 0; s < 2400; ++s) {
      const double x = 0.25 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * s / 48000.0);
      const double y = novaEq.processParallelSample(x, 0.0, p);
      if (s >= 1800) bandRms += y * y;
    }
    const double bandOutRms = std::sqrt(bandRms / 600.0);
    novaEq.reset();
    double compRms = 0.0;
    for (int s = 0; s < 2400; ++s) {
      const double x = 0.25 * std::sin(2.0 * 3.14159265358979323846 * 150.0 * s / 48000.0);
      const double y = novaEq.processParallelSample(x, 0.0, p);
      if (s >= 1800) compRms += y * y;
    }
    const double compOutRms = std::sqrt(compRms / 600.0);
    assert(bandOutRms > compOutRms * 1.6);

    // 8. Wideband Expansion (wBandMode = Below, upward lift)
    novaEq.reset();
    p = NovaParallelParams{};
    p.wBandEnabled = true;
    p.wBandMode = DynamicEqMode::Below;
    p.wBandThresholdDb = -15.0;
    p.wBandRangeDb = 6.0;
    p.wBandRatio = 2.5;
    p.wBandAttackMs = 1.0;
    p.wBandReleaseMs = 50.0;
    for (int s = 0; s < 2400; ++s) {
      const double quiet = 0.02 * std::sin(2.0 * 3.14159265358979323846 * 400.0 * s / 48000.0);
      novaEq.processParallelSample(quiet, 0.0, p);
    }
    assert(novaEq.getWBandDynamicGainDb() >= 3.0f && novaEq.getWBandDynamicGainDb() <= 6.0f);
  }

  // Vocal Pitch Correction (Auto-Tune Evo Style) Tests
  {
    VocalPitchCorrector vpc;
    vpc.setSampleRate(48000.0);
    vpc.setKey(0);   // C
    vpc.setScale(1); // Major
    vpc.setRetuneSpeedMs(5.0);
    vpc.setHumanizePct(0.0);
    vpc.setFormantPreserve(true);

    // 1. Flat A4 (432 Hz) snaps up toward A4 (MIDI 69, 440 Hz)
    for (int i = 0; i < 4096; ++i) {
      double sL = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 432.0 * i / 48000.0);
      double sR = sL;
      vpc.processStereo(sL, sR);
      assert(std::isfinite(sL) && std::isfinite(sR));
      assert(std::abs(sL) < 2.0 && std::abs(sR) < 2.0);
    }
    assert(vpc.getVoicedConfidence() > 0.5f);
    assert(std::abs(vpc.getTargetMidi() - 69.0f) < 0.01f);
    assert(vpc.getCorrectionCents() > 20.0f);

    // 2. C Major scale skips C#4 (MIDI 61, 277.18 Hz) and snaps to C4 (60) or D4 (62)
    vpc.reset();
    for (int i = 0; i < 4096; ++i) {
      double sL = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 277.18 * i / 48000.0);
      double sR = sL;
      vpc.processStereo(sL, sR);
      assert(std::isfinite(sL) && std::isfinite(sR));
    }
    const int snappedMidi = static_cast<int>(std::round(vpc.getTargetMidi()));
    assert(snappedMidi == 60 || snappedMidi == 62);

    // 3. Flayr Tune extensions: DisplayPoint ring, removedMask, Live/PSOLA, Expression, NatVib, Transpose
    tune::DisplayPoint pt{};
    int poppedPoints = 0;
    while (vpc.popDisplayPoint(pt)) {
      ++poppedPoints;
      assert(std::isfinite(pt.inNote) && std::isfinite(pt.outNote) && std::isfinite(pt.targetNote));
    }
    assert(poppedPoints > 0);

    // Toggle removed pitch class C# (pc=1) and verify mask state
    assert(!vpc.isPitchClassRemoved(1));
    vpc.toggleRemovedPitchClass(1);
    assert(vpc.isPitchClassRemoved(1));
    vpc.toggleRemovedPitchClass(1);
    assert(!vpc.isPitchClassRemoved(1));

    // Switch to TD-PSOLA mode, enable Expression / Natural Vibrato / Transpose and process audio
    vpc.reset();
    vpc.setLiveMode(false);
    vpc.setExpressionPct(40.0);
    vpc.setNaturalVibratoDb(3.0);
    vpc.setTranspose(2);
    for (int i = 0; i < 4096; ++i) {
      double sL = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0);
      double sR = sL;
      vpc.processStereo(sL, sR);
      assert(std::isfinite(sL) && std::isfinite(sR));
      assert(std::abs(sL) < 2.0 && std::abs(sR) < 2.0);
    }
    assert(vpc.getLatencySamples() > 0);
  }

  // RackDynamicsEngine (DIE-Plugins + 7-Circuit Compressor/Expander) Tests
  {
    nova::graviton::RackDynamicsEngine dyn;
    dyn.setSampleRate(48000.0);
    dyn.setMode(static_cast<int>(nova::graviton::RackDynamicsEngine::Mode::Compressor));
    dyn.setThresholdDb(-20.0);
    dyn.setRatio(4.0);
    dyn.setKnee(0.5);
    dyn.setAttackMs(1.0);
    dyn.setReleaseMs(50.0);
    dyn.setMakeupDb(0.0);
    dyn.setMixPct(100.0);

    // 1. Hot signal (-6 dBFS -> 0.5 peak) triggers measurable gain reduction across all 7 circuit models
    for (int m = 0; m < 7; ++m) {
      dyn.reset();
      dyn.setCircuitModel(m);
      for (int i = 0; i < 2048; ++i) {
        double l = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 48000.0);
        double r = l;
        dyn.processStereo(l, r);
        assert(std::isfinite(l) && std::isfinite(r));
        assert(std::abs(l) < 2.0 && std::abs(r) < 2.0);
      }
      assert(dyn.getGainReductionDb() > 4.0f);
      assert(dyn.getInLevelDb() > -12.0f);
    }

    // 2. Output Limiter clamps hot signal below limiter threshold
    dyn.reset();
    dyn.setCircuitModel(0);
    dyn.setThresholdDb(-4.0);
    dyn.setRatio(1.0);
    dyn.setInputGainDb(12.0);
    dyn.setLimiterEnabled(true);
    dyn.setLimiterThresholdDb(-3.0);
    double maxLimPeak = 0.0;
    for (int i = 0; i < 2048; ++i) {
      double l = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0);
      double r = l;
      dyn.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
      if (i > 256)
        maxLimPeak = std::max(maxLimPeak, std::abs(l));
    }
    assert(maxLimPeak <= 0.78); // ~ -2.2 dBFS steady-state below -3 dB ceiling

    // 3. Expander/Gate mode attenuates quiet signal below threshold
    dyn.reset();
    dyn.setInputGainDb(0.0);
    dyn.setLimiterEnabled(false);
    dyn.setMode(static_cast<int>(nova::graviton::RackDynamicsEngine::Mode::ExpanderGate));
    dyn.setThresholdDb(-20.0);
    dyn.setRatio(4.0);
    dyn.setAttackMs(0.5);
    dyn.setReleaseMs(10.0);
    for (int i = 0; i < 2048; ++i) {
      double l = 0.01 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0); // -40 dBFS
      double r = l;
      dyn.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
    }
    assert(dyn.getGainReductionDb() > 10.0f);
    assert(dyn.computeTransferDb(-40.0) < -50.0);
  }

  // SubHarmonicSynthesizer (bx_subsynth 3-Module Global / Sub / Edge) Tests
  {
    SubHarmonicSynthesizer sub;
    sub.setSampleRate(48000.0);
    sub.applyRecommendedStartPreset();
    assert(std::abs(sub.getMixPct() - 50.0) < 0.01);
    assert(std::abs(sub.getSubharmonicsPct() - 30.0) < 0.01);
    assert(std::abs(sub.getSubBandPct(0) - 20.0) < 0.01);
    assert(std::abs(sub.getSubBandPct(1) - 30.0) < 0.01);
    assert(std::abs(sub.getSubBandPct(2) - 40.0) < 0.01);

    sub.setSynthesisInputStereo(true);
    sub.setMonoMakerHz(100.0);
    sub.setStereoWidthPct(120.0);
    sub.setEdgeMode(static_cast<int>(SubHarmonicSynthesizer::EdgeMode::Harsh));
    sub.setSubBandTrimDb(1, 3.0);

    // Feed 80 Hz stereo bass tone to excite 36-56 Hz / 56-80 Hz bands
    for (int i = 0; i < 4096; ++i) {
      double l = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 80.0 * i / 48000.0);
      double r = 0.4 * std::sin(2.0 * 3.14159265358979323846 * 80.0 * i / 48000.0);
      sub.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
      assert(std::abs(l) < 4.0 && std::abs(r) < 4.0);
    }
    assert(sub.getInputLevelDbL() > -20.0f);
    assert(sub.getOutputLevelDbL() > -20.0f);
    assert(sub.getSubBandLevelDb(2) > -55.0f);

    // Solo band 2 isolates synthesized sub output
    sub.setSubBandSolo(2, true);
    double sl = 0.5, sr = 0.5;
    sub.processStereo(sl, sr);
    assert(std::isfinite(sl) && std::isfinite(sr));
    sub.setSubBandSolo(2, false);

    // Flip-flop octave divider tracks input pitch: 120 Hz in -> ~60 Hz out (band 56-80 Hz solo)
    SubHarmonicSynthesizer trk;
    trk.setSampleRate(48000.0);
    trk.setMixPct(100.0);
    trk.setSubharmonicsPct(100.0);
    trk.setSubBandPct(0, 100.0);
    trk.setSubBandSolo(0, true);
    int risingCrossings = 0;
    double prevOut = 0.0;
    const int warm = 12000, total = 48000;
    for (int i = 0; i < total; ++i) {
      double l = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 120.0 * i / 48000.0);
      double r = l;
      trk.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
      if (i >= warm) {
        if (prevOut <= 0.0 && l > 0.0)
          ++risingCrossings;
        prevOut = l;
      }
    }
    // 0.75 s window: ~45 cycles at 60 Hz (120 Hz would give ~90)
    assert(risingCrossings >= 30 && risingCrossings <= 60);
  }

  // DN360InductorGEQ (LSP Graph-Equalizer Extensions) Tests
  {
    DN360InductorGEQ geq;
    geq.setSampleRate(48000.0);

    // 1. Flat response is bit-transparent (0 dB magnitude)
    assert(std::abs(geq.getMagnitudeDb(0, 1000.0)) < 1.0e-6);

    // 2. Band 16 (1000 Hz) boost +6 dB in Matched & Bilinear modes
    const size_t band1k = DN360InductorGEQ::nearestBand(1000.0);
    assert(band1k == 16);
    geq.setBandGain(band1k, 6.0);
    assert(std::abs(geq.getMagnitudeDb(0, 1000.0) - 6.0) < 0.25);
    geq.setTransform(static_cast<int>(DN360InductorGEQ::Transform::Matched));
    assert(std::abs(geq.getMagnitudeDb(0, 1000.0) - 6.0) < 0.25);

    // 3. Shift Gain scales band response (0.5x -> +3 dB)
    geq.setShiftGain(0.5);
    assert(std::abs(geq.getMagnitudeDb(0, 1000.0) - 3.0) < 0.25);
    geq.setShiftGain(1.0);

    // 4. Mid/Side & Left/Right independent curves + Range clamping
    geq.setMode(static_cast<int>(DN360InductorGEQ::Mode::MidSide));
    geq.setBandGainCh(1, band1k, -6.0);
    assert(std::abs(geq.getMagnitudeDb(0, 1000.0) - 6.0) < 0.25);
    assert(std::abs(geq.getMagnitudeDb(1, 1000.0) + 6.0) < 0.25);
    geq.setRangeDb(6.0);
    geq.setBandGainCh(0, band1k, 12.0);
    assert(std::abs(geq.getBandGainCh(0, band1k) - 6.0) < 1.0e-6);

    // 5. Audio processing updates peak meters & 30-band Goertzel analyzer
    geq.setMode(static_cast<int>(DN360InductorGEQ::Mode::Stereo));
    geq.setRangeDb(15.0);
    geq.flatten();
    geq.setBandGain(band1k, 6.0);
    for (int i = 0; i < 9000; ++i) {
      double l = 0.25 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / 48000.0);
      double r = l;
      geq.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
    }
    assert(geq.getPeakInDb(0) > -20.0f);
    assert(geq.getPeakOutDb(0) > geq.getPeakInDb(0) + 4.0f);
    assert(geq.getOutputSpectrumDb(band1k) > geq.getInputSpectrumDb(band1k) + 4.0f);

    // 6. SubBassEnhancer (÷2..÷16 divider + warm saturation + compression) on core path
    geq.reset();
    geq.flatten();
    geq.setSubEnhancePct(60.0);
    geq.setWarmDrivePct(40.0);
    geq.setSubCompPct(100.0);
    for (int i = 0; i < 4096; ++i) {
      double l = 0.4 * std::sin(2.0 * 3.14159265358979323846 * 90.0 * i / 48000.0);
      double r = l;
      geq.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
      assert(std::abs(l) < 2.0 && std::abs(r) < 2.0);
    }
    assert(geq.getPeakOutDb(0) > -18.0f);

    // 7. Authentic DN360 Factory Presets (0..9)
    for (int p = 0; p < DN360InductorGEQ::kNumPresets; ++p) {
      geq.applyPreset(p);
      assert(geq.getCurrentPresetIndex() == p);
      assert(DN360InductorGEQ::getPresetName(p) != nullptr);
      double l = 0.2, r = -0.15;
      geq.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
    }
    geq.applyPreset(2); // Floor Monitor Anti-Feedback uses Slope x3 (Q=2.5)
    assert(geq.getSlopeIndex() == 2);
    geq.applyPreset(8); // Master Bus M/S Clarity uses Mid/Side mode
    assert(geq.getMode() == static_cast<int>(DN360InductorGEQ::Mode::MidSide));
  }

  // DistortionEngine (Dual Tube + Tape Saturation, Blend, Tone, Character, Output, A/B) Tests
  {
    DistortionEngine dist;
    dist.setSampleRate(48000.0);

    // 1. All 5 Tube types + 4 Tape types + 3 Character modes produce finite bounded output
    for (int tt = 0; tt < 5; ++tt) {
      dist.reset();
      dist.setTubeEnabled(true);
      dist.setTubeType(tt);
      dist.setTubeDriveDb(12.0);
      dist.setTubeBiasPct(25.0);
      dist.setTapeEnabled(true);
      dist.setTapeType(tt % 4);
      dist.setFlutterPct(15.0);
      dist.setWowPct(15.0);
      dist.setBlendPct(50.0);
      dist.setCharacterMode(tt % 3);
      dist.setSoftClip(true);
      assert(std::isfinite(dist.computeTransferSample(0.5)));
      for (int i = 0; i < 2048; ++i) {
        double l = 0.4 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0);
        double r = l;
        dist.processStereo(l, r);
        assert(std::isfinite(l) && std::isfinite(r));
        assert(std::abs(l) < 3.0 && std::abs(r) < 3.0);
      }
      assert(dist.getInputLevelDb() > -15.0f);
      assert(dist.getVuLevelDb() > -20.0f);
    }

    // 2. A/B snapshot & Save/Load preserve distinct configurations
    dist.applyPreset(0); // 01 - Warm Vocal (Warm | 2.5 / 2.0 / 35%)
    assert(std::abs(dist.getTubeDriveDb() - 2.5) < 0.01);
    assert(std::abs(dist.getTapeDriveDb() - 2.0) < 0.01);
    assert(std::abs(dist.getBlendPct() - 35.0) < 0.01);
    dist.selectSlotB(true);
    dist.applyPreset(11); // 12 - Heavy Analog (Aggressive | 8.0 / 7.5 / 50%)
    assert(dist.getCharacterMode() == 2);
    assert(std::abs(dist.getTubeDriveDb() - 8.0) < 0.01);
    dist.selectSlotB(false);
    assert(dist.getCharacterMode() == 0);
    dist.saveUserPreset();
    assert(dist.loadUserPreset());

    // 3. All 12 factory presets apply cleanly
    assert(DistortionEngine::kNumPresets == 12);
    for (int p = 0; p < DistortionEngine::kNumPresets; ++p) {
      dist.applyPreset(p);
      assert(dist.getCurrentPresetIndex() == p);
      assert(DistortionEngine::getPresetName(p) != nullptr);
      double l = 0.25, r = -0.25;
      dist.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
    }
  }

  // ConvolutionReverbEngine (True IR + 24 Factory Presets + IR Shaping/EQ/Meters) Tests
  {
    nova::ConvolutionReverbEngine conv;
    conv.setSampleRate(48000.0);
    assert(conv.isIRLoaded());
    assert(conv.getIRLength() > 10000);

    // 1. Size scaling actually changes processed IR length
    const int baseLen = conv.getIRLength();
    auto p = conv.getParams();
    p.sizePct = 150.0f;
    p.decayScale = 1.5f;
    p.reverseIR = true;
    conv.setParams(p);
    assert(conv.getIRLength() > baseLen);

    // 2. Waveform peaks & audio processing + IN/OUT meters
    std::vector<float> wfL, wfR;
    conv.getWaveformPeaks(wfL, wfR, 64);
    assert(wfL.size() == 64 && wfR.size() == 64);
    for (int i = 0; i < 2048; ++i) {
      float l = (i < 64) ? 0.5f : 0.0f;
      float r = l;
      conv.processStereo(l, r);
      assert(std::isfinite(l) && std::isfinite(r));
    }
    assert(conv.getOutputLevelDbL() > -55.0f);

    // 3. All 24 factory presets across 8 categories load cleanly
    for (int idx = 0; idx < nova::ConvolutionReverbEngine::kNumFactoryPresets; ++idx) {
      conv.loadFactoryPreset(idx);
      assert(conv.isIRLoaded());
    }

    // 4. Concurrent audio thread + GUI preset switching stress test (lock-free)
    std::atomic<bool> stopStress{false};
    std::thread audioSim([&] {
      while (!stopStress.load(std::memory_order_relaxed)) {
        float l = 0.25f, r = -0.25f;
        conv.processStereo(l, r);
        assert(std::isfinite(l) && std::isfinite(r));
      }
    });
    for (int round = 0; round < 3; ++round) {
      for (int idx = 0; idx < nova::ConvolutionReverbEngine::kNumFactoryPresets; ++idx)
        conv.loadFactoryPreset(idx);
    }
    stopStress.store(true, std::memory_order_relaxed);
    audioSim.join();
  }

  // HybridReverbEngine (IR + Algorithmic, Routing, 5 Algos, 4-Band EQ) Tests
  {
    nova::HybridReverbEngine hyb;
    hyb.init(48000.0);

    // 1. Test all 4 routing modes & 5 algorithms produce finite output
    for (int r = 0; r < 4; ++r) {
      auto p = hyb.getParams();
      p.routing = r;
      p.algorithm = r % 5;
      p.dryWet = 0.5f;
      p.bassMono = true;
      p.vintageMode = 1;
      hyb.setParams(p);
      for (int i = 0; i < 1024; ++i) {
        float l = (i < 32) ? 0.5f : 0.0f;
        float rSig = l;
        hyb.processStereo(l, rSig);
        assert(std::isfinite(l) && std::isfinite(rSig));
      }
    }

    // 2. EQ magnitude & IR waveform peaks
    assert(std::isfinite(hyb.getEqMagnitudeDb(1000.0)));
    std::vector<float> wfL, wfR;
    hyb.getIrWaveformPeaks(wfL, wfR, 64);
    assert(wfL.size() == 64 && wfR.size() == 64);
  }

  // VSSReverb (TC Electronic VSS3 Stereo Source Reverb) Tests
  {
    VSSReverb vss3;
    vss3.setSampleRate(48000.0);
    vss3.setModel(1);
    for (int p = 0; p < VSSReverb::kNumPresets; ++p) {
      vss3.applyPreset(p);
      assert(vss3.getCurrentPresetIndex() == p);
      for (int i = 0; i < 512; ++i) {
        double l = (i < 32) ? 0.4 : 0.0;
        double r = l;
        vss3.processStereo(l, r);
        assert(std::isfinite(l) && std::isfinite(r));
      }
    }
    assert(vss3.getOutLevelDb() > -60.0f);
  }

  // DynamicsProcessor GateMode (DualGate, TransientDesigner, ExpanderDucker) Tests
  {
    for (int gm = 0; gm <= 2; ++gm) {
      DynamicsProcessor dp;
      DynamicsSettings s;
      s.gateEnabled = true;
      s.gateMode = static_cast<GateMode>(gm);
      s.gateThresholdDb = -30.0;
      s.gateRatio = 4.0;
      s.expanderRangeDb = 12.0;
      s.sampleRate = 48000.0;
      s.attackCoefficient = std::exp(-1.0 / (48000.0 * 0.002));
      s.releaseCoefficient = std::exp(-1.0 / (48000.0 * 0.05));
      for (int i = 0; i < 256; ++i) {
        const double x = (i < 64) ? 0.5 : 0.005;
        auto res = dp.process(x, s);
        assert(std::isfinite(res.sample));
        assert(std::isfinite(res.gateReductionDb));
      }
    }
  }

  std::cout << "Graviton Mk 2 DSP tests passed (64-bit double precision).\n";
  return 0;
}
