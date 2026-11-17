#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace nova::graviton {

// ==============================================================================
// Sub-Harmonic Synthesizer (Brainworx bx_subsynth + dbx 120XSP style)
// Virtual FX Rack Slot 17 - MIDAS Heritage-D HD96-24 Chapter 15 (Virtual FX Rack)
//
// 3-Module Architecture:
//   1. GLOBAL: Mid/Stereo Synthesis Input, Gain In, Gain Out, Mix,
//              M/S Mono Maker (low-end mono fold), Stereo Width, Input L/R Meters.
//   2. SUB:    Master Subharmonics + 3-band subharmonic generators:
//              - Band 0: 56-80 Hz (Synthesized Frequency)
//              - Band 1: 24-36 Hz (Ultra-low Sub)
//              - Band 2: 36-56 Hz (Punch Sub)
//              Each band has Level %, Trim (-12..+12 dB), Solo, and LED meter.
//              Also preserves the fundamental zero-crossing sub-octave tracker.
//   3. EDGE:   Mode (Smooth / Harsh), Squeeze dynamics, Drive saturation,
//              Low Cut (20-120 Hz), High Cut (60-12000 Hz), Tight Punch,
//              Low End contour, and Output L/R Meters.
//
// Real-time audio-thread safe: zero allocations, zero locks, NaN/Inf sanitized.
// ==============================================================================
class SubHarmonicSynthesizer final {
public:
  static constexpr double kMinSubHz = 20.0;
  static constexpr double kMaxSubHz = 160.0;
  static constexpr double kMinSplitHz = 40.0;
  static constexpr double kMaxSplitHz = 250.0;
  static constexpr double kMinResponseMs = 10.0;
  static constexpr double kMaxResponseMs = 500.0;

  enum class EdgeMode : int { Smooth = 0, Harsh = 1 };

  SubHarmonicSynthesizer() noexcept { setSampleRate(48000.0); }

  void setSampleRate(double sampleRate) noexcept {
    cachedSampleRate = clampFinite(sampleRate, 1000.0, 768000.0, 48000.0);
    updateCoefficients();
    reset();
  }

  void reset() noexcept {
    bandFilter.reset();
    bandFilterR.reset();
    for (auto &perBand : bandCh)
      for (auto &c : perBand)
        c.reset();
    monoMakerHpf.reset();
    lowCutL.reset();
    lowCutR.reset();
    highCutL.reset();
    highCutR.reset();
    tightPunchFilterL.reset();
    tightPunchFilterR.reset();
    lowEndFilterL.reset();
    lowEndFilterR.reset();

    lastBandSample = 0.0;
    samplesSinceCrossing = 0;
    lastPeriodSamples = 0;
    measuredFrequencyHz = 0.0;
    targetFrequencyHz = 0.0;
    envelope = 0.0;
    subFrequencyHz = 0.0;
    phase = 0.0;
    frequencyLocked = false;

    for (int i = 0; i < 3; ++i) {
      subBandEnv[i] = 0.0;
      subBandPhase[i] = 0.0;
      subBandLevelDb[i].store(-60.0f, std::memory_order_relaxed);
    }
    squeezeEnv = 0.0;
    inLevelDbL.store(-60.0f, std::memory_order_relaxed);
    inLevelDbR.store(-60.0f, std::memory_order_relaxed);
    outLevelDbL.store(-60.0f, std::memory_order_relaxed);
    outLevelDbR.store(-60.0f, std::memory_order_relaxed);
  }

  // --- Legacy / Core API (preserved for compatibility & existing tests) ---
  void setSplitFrequencyHz(double frequencyHz) noexcept {
    splitFrequencyHz = clampFinite(frequencyHz, kMinSplitHz, kMaxSplitHz, 120.0);
    updateCoefficients();
  }
  double getSplitFrequencyHz() const noexcept { return splitFrequencyHz; }

  void setSubLevel(double level) noexcept {
    subLevel = clampFinite(level, 0.0, 1.0, 0.5);
    subharmonicsPct.store(static_cast<float>(subLevel * 100.0),
                          std::memory_order_relaxed);
  }
  double getSubLevel() const noexcept { return subLevel; }

  void setDirectLevel(double level) noexcept {
    directLevel = clampFinite(level, 0.0, 1.0, 1.0);
  }
  double getDirectLevel() const noexcept { return directLevel; }

  void setResponseMs(double milliseconds) noexcept {
    responseMs = clampFinite(milliseconds, kMinResponseMs, kMaxResponseMs, 120.0);
    updateCoefficients();
  }
  double getResponseMs() const noexcept { return responseMs; }

  double getSubFrequencyHz() const noexcept { return subFrequencyHz; }
  bool isLocked() const noexcept { return frequencyLocked; }
  long long getLastPeriodSamples() const noexcept { return lastPeriodSamples; }
  double getMeasuredFrequencyHz() const noexcept { return measuredFrequencyHz; }
  double getBandEnvelope() const noexcept { return envelope; }
  double getSampleRate() const noexcept { return cachedSampleRate; }

  // --- GLOBAL Module Controls ---
  void setSynthesisInputStereo(bool stereo) noexcept {
    synthesisInputStereo.store(stereo, std::memory_order_relaxed);
  }
  bool getSynthesisInputStereo() const noexcept {
    return synthesisInputStereo.load(std::memory_order_relaxed);
  }

  void setGainInDb(double db) noexcept {
    gainInDb.store(static_cast<float>(clampFinite(db, -12.0, 12.0, 0.0)),
                   std::memory_order_relaxed);
  }
  double getGainInDb() const noexcept {
    return static_cast<double>(gainInDb.load(std::memory_order_relaxed));
  }

  void setGainOutDb(double db) noexcept {
    gainOutDb.store(static_cast<float>(clampFinite(db, -12.0, 12.0, 0.0)),
                    std::memory_order_relaxed);
  }
  double getGainOutDb() const noexcept {
    return static_cast<double>(gainOutDb.load(std::memory_order_relaxed));
  }

  void setMixPct(double pct) noexcept {
    mixPct.store(static_cast<float>(clampFinite(pct, 0.0, 100.0, 100.0)),
                 std::memory_order_relaxed);
  }
  double getMixPct() const noexcept {
    return static_cast<double>(mixPct.load(std::memory_order_relaxed));
  }

  void setMonoMakerHz(double hz) noexcept {
    monoMakerHz.store(static_cast<float>(clampFinite(hz, 0.0, 200.0, 0.0)),
                      std::memory_order_relaxed);
    updateMonoMaker();
  }
  double getMonoMakerHz() const noexcept {
    return static_cast<double>(monoMakerHz.load(std::memory_order_relaxed));
  }

  void setStereoWidthPct(double pct) noexcept {
    stereoWidthPct.store(
        static_cast<float>(clampFinite(pct, 0.0, 200.0, 100.0)),
        std::memory_order_relaxed);
  }
  double getStereoWidthPct() const noexcept {
    return static_cast<double>(stereoWidthPct.load(std::memory_order_relaxed));
  }

  // --- SUB Module Controls ---
  void setSubharmonicsPct(double pct) noexcept {
    const double clamped = clampFinite(pct, 0.0, 100.0, 50.0);
    subharmonicsPct.store(static_cast<float>(clamped),
                          std::memory_order_relaxed);
    subLevel = clamped * 0.01;
  }
  double getSubharmonicsPct() const noexcept {
    return static_cast<double>(subharmonicsPct.load(std::memory_order_relaxed));
  }

  // bandIdx: 0 = 56-80 Hz (Synth Freq), 1 = 24-36 Hz, 2 = 36-56 Hz
  void setSubBandPct(int bandIdx, double pct) noexcept {
    if (bandIdx >= 0 && bandIdx < 3) {
      subBandPct[bandIdx].store(
          static_cast<float>(clampFinite(pct, 0.0, 100.0, 30.0)),
          std::memory_order_relaxed);
    }
  }
  double getSubBandPct(int bandIdx) const noexcept {
    if (bandIdx >= 0 && bandIdx < 3)
      return static_cast<double>(
          subBandPct[bandIdx].load(std::memory_order_relaxed));
    return 0.0;
  }

  void setSubBandTrimDb(int bandIdx, double db) noexcept {
    if (bandIdx >= 0 && bandIdx < 3) {
      subBandTrimDb[bandIdx].store(
          static_cast<float>(clampFinite(db, -12.0, 12.0, 0.0)),
          std::memory_order_relaxed);
    }
  }
  double getSubBandTrimDb(int bandIdx) const noexcept {
    if (bandIdx >= 0 && bandIdx < 3)
      return static_cast<double>(
          subBandTrimDb[bandIdx].load(std::memory_order_relaxed));
    return 0.0;
  }

  void setSubBandSolo(int bandIdx, bool solo) noexcept {
    if (bandIdx >= 0 && bandIdx < 3)
      subBandSolo[bandIdx].store(solo, std::memory_order_relaxed);
  }
  bool getSubBandSolo(int bandIdx) const noexcept {
    if (bandIdx >= 0 && bandIdx < 3)
      return subBandSolo[bandIdx].load(std::memory_order_relaxed);
    return false;
  }

  // --- EDGE Module Controls ---
  void setEdgeMode(int m) noexcept {
    edgeMode.store(std::clamp(m, 0, 1), std::memory_order_relaxed);
  }
  int getEdgeMode() const noexcept {
    return edgeMode.load(std::memory_order_relaxed);
  }

  void setSqueezePct(double pct) noexcept {
    squeezePct.store(static_cast<float>(clampFinite(pct, 0.0, 100.0, 0.0)),
                     std::memory_order_relaxed);
  }
  double getSqueezePct() const noexcept {
    return static_cast<double>(squeezePct.load(std::memory_order_relaxed));
  }

  void setDrivePct(double pct) noexcept {
    drivePct.store(static_cast<float>(clampFinite(pct, 0.0, 100.0, 0.0)),
                   std::memory_order_relaxed);
  }
  double getDrivePct() const noexcept {
    return static_cast<double>(drivePct.load(std::memory_order_relaxed));
  }

  void setLowCutHz(double hz) noexcept {
    lowCutHz.store(static_cast<float>(clampFinite(hz, 20.0, 120.0, 20.0)),
                   std::memory_order_relaxed);
    updateEdgeFilters();
  }
  double getLowCutHz() const noexcept {
    return static_cast<double>(lowCutHz.load(std::memory_order_relaxed));
  }

  void setHighCutHz(double hz) noexcept {
    highCutHz.store(
        static_cast<float>(clampFinite(hz, 60.0, 12000.0, 12000.0)),
        std::memory_order_relaxed);
    updateEdgeFilters();
  }
  double getHighCutHz() const noexcept {
    return static_cast<double>(highCutHz.load(std::memory_order_relaxed));
  }

  void setTightPunchPct(double pct) noexcept {
    tightPunchPct.store(static_cast<float>(clampFinite(pct, 0.0, 100.0, 0.0)),
                        std::memory_order_relaxed);
    updateEdgeFilters();
  }
  double getTightPunchPct() const noexcept {
    return static_cast<double>(tightPunchPct.load(std::memory_order_relaxed));
  }

  void setLowEndPct(double pct) noexcept {
    lowEndPct.store(static_cast<float>(clampFinite(pct, 0.0, 100.0, 0.0)),
                    std::memory_order_relaxed);
    updateEdgeFilters();
  }
  double getLowEndPct() const noexcept {
    return static_cast<double>(lowEndPct.load(std::memory_order_relaxed));
  }

  // Apply recommended mixing starting preset
  void applyRecommendedStartPreset() noexcept {
    setGainInDb(0.0);
    setGainOutDb(0.0);
    setMixPct(50.0);
    setSubharmonicsPct(30.0);
    setSubBandPct(0, 20.0); // 56-80 Hz (Synthesized Frequency)
    setSubBandPct(1, 30.0); // 24-36 Hz
    setSubBandPct(2, 40.0); // 36-56 Hz
    setEdgeMode(static_cast<int>(EdgeMode::Smooth));
    setSqueezePct(30.0);
    setDrivePct(15.0);
    setLowCutHz(30.0);
    setHighCutHz(120.0);
    setTightPunchPct(40.0);
    setLowEndPct(40.0);
  }

  // --- Telemetry Getters (for UI LED Meters) ---
  float getInputLevelDbL() const noexcept {
    return inLevelDbL.load(std::memory_order_relaxed);
  }
  float getInputLevelDbR() const noexcept {
    return inLevelDbR.load(std::memory_order_relaxed);
  }
  float getOutputLevelDbL() const noexcept {
    return outLevelDbL.load(std::memory_order_relaxed);
  }
  float getOutputLevelDbR() const noexcept {
    return outLevelDbR.load(std::memory_order_relaxed);
  }
  float getSubBandLevelDb(int bandIdx) const noexcept {
    if (bandIdx >= 0 && bandIdx < 3)
      return subBandLevelDb[bandIdx].load(std::memory_order_relaxed);
    return -60.0f;
  }

  void processStereo(double &left, double &right) noexcept {
    const double dryLeft = sanitize(left);
    const double dryRight = sanitize(right);

    // 1. GLOBAL Input Gain Stage
    const double inGain =
        dbToGain(static_cast<double>(gainInDb.load(std::memory_order_relaxed)));
    const double inLeft = dryLeft * inGain;
    const double inRight = dryRight * inGain;

    updateMeter(inLevelDbL, inLeft);
    updateMeter(inLevelDbR, inRight);

    // 2. Fundamental Zero-Crossing Sub-Octave Tracker (original core)
    const bool stereoDet =
        synthesisInputStereo.load(std::memory_order_relaxed);
    const double detMono =
        stereoDet ? (0.5 * (inLeft + inRight)) : (0.5 * (inLeft + inRight));
    const double band = sanitize(bandFilter.process(detMono));
    const double bandR =
        stereoDet ? sanitize(bandFilterR.process(inRight)) : band;

    const double rectified =
        stereoDet ? std::max(std::abs(band), std::abs(bandR)) : std::abs(band);
    const double envCoeff = rectified > envelope ? attackCoeff : releaseCoeff;
    envelope += envCoeff * (rectified - envelope);
    if (!(envelope > 0.0) || !std::isfinite(envelope))
      envelope = 0.0;

    double measuredHz = 0.0;
    if (samplesSinceCrossing < (1 << 24))
      ++samplesSinceCrossing;

    if (lastBandSample < 0.0 && band >= 0.0 && samplesSinceCrossing > 1) {
      lastPeriodSamples = samplesSinceCrossing;
      const double f0 =
          cachedSampleRate / static_cast<double>(samplesSinceCrossing);
      if (std::isfinite(f0) && f0 >= 25.0 && f0 <= 800.0) {
        measuredHz = f0;
        measuredFrequencyHz = f0;
      }
      samplesSinceCrossing = 0;
    }
    lastBandSample = band;

    if (measuredHz > 0.0 && envelope > 1.0e-4) {
      const double target =
          std::clamp(measuredHz * 0.5, kMinSubHz, kMaxSubHz);
      if (!frequencyLocked) {
        subFrequencyHz = targetFrequencyHz = target;
        frequencyLocked = true;
      } else {
        targetFrequencyHz = target;
      }
    }

    if (frequencyLocked &&
        std::abs(targetFrequencyHz - subFrequencyHz) > 1.0e-9) {
      subFrequencyHz += glideCoeff * (targetFrequencyHz - subFrequencyHz);
      if (!std::isfinite(subFrequencyHz))
        subFrequencyHz = targetFrequencyHz;
    }

    double coreSub = 0.0;
    if (frequencyLocked && envelope > 0.0) {
      phase += kTwoPi * subFrequencyHz / cachedSampleRate;
      if (!(phase < kTwoPi))
        phase = 0.0;
      coreSub = sanitize(std::sin(phase)) * envelope;
    }

    // 3. SUB 3-Band Subharmonic Synthesis (56-80 Hz, 24-36 Hz, 36-56 Hz)
    // Pitch-tracked flip-flop octave divider (analog divide-by-2 principle,
    // as in the HK Enhancer SubharmonicGenerator reference): each band isolates
    // the octave above its target range, a flip-flop toggles on every second
    // rising zero crossing (-> half frequency square wave), a smoothing LPF
    // rounds it into a sine-like wave, and the band envelope shapes it.
    const bool anySolo = subBandSolo[0].load(std::memory_order_relaxed) ||
                         subBandSolo[1].load(std::memory_order_relaxed) ||
                         subBandSolo[2].load(std::memory_order_relaxed);

    double subSum = 0.0;
    for (int i = 0; i < 3; ++i) {
      double oscSum;
      if (stereoDet) {
        oscSum = 0.5 * (bandCh[i][0].process(inLeft, attackCoeff, releaseCoeff) +
                        bandCh[i][1].process(inRight, attackCoeff, releaseCoeff));
      } else {
        oscSum = bandCh[i][0].process(detMono, attackCoeff, releaseCoeff);
      }
      subBandEnv[i] = bandCh[i][0].env;

      const double pct =
          static_cast<double>(subBandPct[i].load(std::memory_order_relaxed)) *
          0.01;
      const double trim = dbToGain(
          static_cast<double>(subBandTrimDb[i].load(std::memory_order_relaxed)));
      const double bandSignal = oscSum * pct * trim;

      updateMeter(subBandLevelDb[i], bandSignal * subLevel * 2.5);

      if (!anySolo || subBandSolo[i].load(std::memory_order_relaxed)) {
        subSum += bandSignal;
      }
    }

    // Blend core zero-crossing sub-octave with 3-band synthesized sub
    double subSignal = (coreSub * 0.75 + subSum * 0.65) * subLevel;

    // 4. EDGE Processing (Squeeze, Drive, Mode Smooth/Harsh, Filters)
    const double sqz =
        static_cast<double>(squeezePct.load(std::memory_order_relaxed)) * 0.01;
    if (sqz > 0.001) {
      const double absSub = std::abs(subSignal);
      const double sc = absSub > squeezeEnv ? attackCoeff : releaseCoeff;
      squeezeEnv += sc * (absSub - squeezeEnv);
      const double compGain = 1.0 / (1.0 + squeezeEnv * sqz * 8.0);
      subSignal *= compGain * (1.0 + sqz * 0.45);
    }

    const double drv =
        static_cast<double>(drivePct.load(std::memory_order_relaxed)) * 0.01;
    if (drv > 0.001) {
      const int m = edgeMode.load(std::memory_order_relaxed);
      if (m == static_cast<int>(EdgeMode::Smooth)) {
        const double g = 1.0 + drv * 3.0;
        subSignal = std::tanh(subSignal * g) / std::tanh(g);
      } else {
        const double g = 1.0 + drv * 6.0;
        subSignal =
            std::tanh(subSignal * g + 0.25 * subSignal * subSignal * g) /
            std::tanh(g);
      }
    }

    // Low Cut & High Cut on synthesized sub path (and Tight Punch / Low End)
    double wetL = anySolo ? subSignal : (inLeft * directLevel + subSignal);
    double wetR = anySolo ? subSignal : (inRight * directLevel + subSignal);

    if (lowCutHz.load(std::memory_order_relaxed) > 20.5f) {
      wetL = sanitize(lowCutL.process(wetL));
      wetR = sanitize(lowCutR.process(wetR));
    }

    if (anySolo && highCutHz.load(std::memory_order_relaxed) < 11900.0f) {
      wetL = sanitize(highCutL.process(wetL));
      wetR = sanitize(highCutR.process(wetR));
    }

    if (tightPunchPct.load(std::memory_order_relaxed) > 0.5f) {
      wetL = sanitize(tightPunchFilterL.process(wetL));
      wetR = sanitize(tightPunchFilterR.process(wetR));
    }

    if (lowEndPct.load(std::memory_order_relaxed) > 0.5f) {
      wetL = sanitize(lowEndFilterL.process(wetL));
      wetR = sanitize(lowEndFilterR.process(wetR));
    }

    // 5. GLOBAL M/S Stereo Width & Mono Maker
    if (!anySolo) {
      const double width =
          static_cast<double>(stereoWidthPct.load(std::memory_order_relaxed)) *
          0.01;
      const double mmHz =
          static_cast<double>(monoMakerHz.load(std::memory_order_relaxed));
      if (std::abs(width - 1.0) > 0.005 || mmHz > 1.0) {
        const double mid = 0.5 * (wetL + wetR);
        double side = 0.5 * (wetL - wetR) * width;
        if (mmHz > 1.0)
          side = sanitize(monoMakerHpf.process(side));
        wetL = mid + side;
        wetR = mid - side;
      }
    }

    // 6. GLOBAL Mix (Dry / Wet) & Output Gain
    const double mix =
        static_cast<double>(mixPct.load(std::memory_order_relaxed)) * 0.01;
    const double outGain = dbToGain(
        static_cast<double>(gainOutDb.load(std::memory_order_relaxed)));

    double outL = (dryLeft * (1.0 - mix) + wetL * mix) * outGain;
    double outR = (dryRight * (1.0 - mix) + wetR * mix) * outGain;

    outL = sanitize(outL);
    outR = sanitize(outR);

    updateMeter(outLevelDbL, outL);
    updateMeter(outLevelDbR, outR);

    left = outL;
    right = outR;
  }

private:
  void updateCoefficients() noexcept {
    bandFilter.setCoefficients(
        makeButterworthLPF(cachedSampleRate, splitFrequencyHz));
    bandFilterR.setCoefficients(
        makeButterworthLPF(cachedSampleRate, splitFrequencyHz));

    // 3-band octave-above detection bandpass filters:
    // Band 0 (56-80 Hz) tracks 112-160 Hz; Band 1 (24-36 Hz) tracks 48-72 Hz; Band 2 (36-56 Hz) tracks 72-112 Hz
    static constexpr double kDetLowHz[3] = {112.0, 48.0, 72.0};
    static constexpr double kDetHighHz[3] = {160.0, 72.0, 112.0};
    // Smoothing LPF cutoff per band (upper edge of the target sub range)
    static constexpr double kSmoothHz[3] = {80.0, 36.0, 56.0};
    for (int i = 0; i < 3; ++i) {
      for (int ch = 0; ch < 2; ++ch) {
        bandCh[i][ch].hpf.setCoefficients(
            makeButterworthHPF(cachedSampleRate, kDetLowHz[i]));
        bandCh[i][ch].lpf.setCoefficients(
            makeButterworthLPF(cachedSampleRate, kDetHighHz[i]));
        bandCh[i][ch].smooth.setCoefficients(
            makeButterworthLPF(cachedSampleRate, kSmoothHz[i]));
      }
    }

    attackCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * 0.001));
    releaseCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * responseMs / 1000.0));
    glideCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * 0.040));
    if (!std::isfinite(attackCoeff))
      attackCoeff = 0.0;
    if (!std::isfinite(releaseCoeff))
      releaseCoeff = 0.0;
    if (!std::isfinite(glideCoeff))
      glideCoeff = 0.0;

    updateMonoMaker();
    updateEdgeFilters();
  }

  void updateMonoMaker() noexcept {
    const double mm = std::clamp(
        static_cast<double>(monoMakerHz.load(std::memory_order_relaxed)), 20.0,
        200.0);
    monoMakerHpf.setCoefficients(makeButterworthHPF(cachedSampleRate, mm));
  }

  void updateEdgeFilters() noexcept {
    const double lc = std::clamp(
        static_cast<double>(lowCutHz.load(std::memory_order_relaxed)), 20.0,
        120.0);
    const double hc = std::clamp(
        static_cast<double>(highCutHz.load(std::memory_order_relaxed)), 60.0,
        std::min(12000.0, cachedSampleRate * 0.45));
    lowCutL.setCoefficients(makeButterworthHPF(cachedSampleRate, lc));
    lowCutR.setCoefficients(makeButterworthHPF(cachedSampleRate, lc));
    highCutL.setCoefficients(makeButterworthLPF(cachedSampleRate, hc));
    highCutR.setCoefficients(makeButterworthLPF(cachedSampleRate, hc));

    const double tpGainDb =
        static_cast<double>(tightPunchPct.load(std::memory_order_relaxed)) *
        0.045; // 0..+4.5 dB punch @ 68 Hz
    const auto tpCoeffs =
        makePeakingEq(cachedSampleRate, 68.0, 1.4, tpGainDb);
    tightPunchFilterL.setCoefficients(tpCoeffs);
    tightPunchFilterR.setCoefficients(tpCoeffs);

    const double leGainDb =
        static_cast<double>(lowEndPct.load(std::memory_order_relaxed)) *
        0.05; // 0..+5.0 dB low contour @ 42 Hz
    const auto leCoeffs = makePeakingEq(cachedSampleRate, 42.0, 0.7, leGainDb);
    lowEndFilterL.setCoefficients(leCoeffs);
    lowEndFilterR.setCoefficients(leCoeffs);
  }

  static void updateMeter(std::atomic<float> &meterDb, double sample) noexcept {
    const double absS = std::abs(sample);
    const float rawDb =
        (absS <= 1.0e-6)
            ? -60.0f
            : static_cast<float>(
                  std::clamp(20.0 * std::log10(absS), -60.0, 6.0));
    const float prev = meterDb.load(std::memory_order_relaxed);
    const float next = (rawDb > prev) ? rawDb : (prev * 0.9990f + rawDb * 0.0010f);
    meterDb.store(next, std::memory_order_relaxed);
  }

  static double dbToGain(double db) noexcept {
    return std::pow(10.0, 0.05 * db);
  }

  static constexpr double kTwoPi = 6.283185307179586476925286766559;

  Biquad bandFilter, bandFilterR;
  // Per-band, per-channel flip-flop octave divider (HK Enhancer principle).
  struct BandChannel {
    Biquad hpf, lpf, smooth;
    double env = 0.0;
    double prev = 0.0;
    bool armed = false;  // Schmitt arm: signal went negative since last crossing
    bool phaseFlag = false;
    bool flipFlop = false;
    static constexpr double kNoiseGate = 0.001; // ~ -60 dB

    void reset() noexcept {
      hpf.reset();
      lpf.reset();
      smooth.reset();
      env = prev = 0.0;
      armed = phaseFlag = flipFlop = false;
    }

    double process(double x, double attackC, double releaseC) noexcept {
      const double bp = sanitize(hpf.process(lpf.process(x)));
      const double rect = std::abs(bp);
      env += (rect > env ? attackC : releaseC) * (rect - env);
      if (!(env > 0.0) || !std::isfinite(env))
        env = 0.0;

      if (env < kNoiseGate) {
        prev = bp;
        armed = false;
        return sanitize(smooth.process(0.0));
      }

      const double hyst = 0.05 * env; // Schmitt hysteresis against chatter
      if (bp < -hyst)
        armed = true;
      if (armed && prev <= 0.0 && bp > 0.0) {
        armed = false;
        phaseFlag = !phaseFlag;       // every 2nd rising crossing -> f/2
        if (!phaseFlag)
          flipFlop = !flipFlop;
      }
      prev = bp;

      const double square = flipFlop ? 1.0 : -1.0;
      return sanitize(smooth.process(square)) * env * 2.0;
    }
  };
  std::array<std::array<BandChannel, 2>, 3> bandCh;
  Biquad monoMakerHpf;
  Biquad lowCutL, lowCutR, highCutL, highCutR;
  Biquad tightPunchFilterL, tightPunchFilterR;
  Biquad lowEndFilterL, lowEndFilterR;

  double cachedSampleRate = 48000.0;
  double splitFrequencyHz = 120.0;
  double subLevel = 0.5;
  double directLevel = 1.0;
  double responseMs = 120.0;

  double attackCoeff = 0.0;
  double releaseCoeff = 0.0;
  double glideCoeff = 0.0;

  double lastBandSample = 0.0;
  long long samplesSinceCrossing = 0;
  long long lastPeriodSamples = 0;
  double measuredFrequencyHz = 0.0;
  double targetFrequencyHz = 0.0;
  double envelope = 0.0;
  double subFrequencyHz = 0.0;
  double phase = 0.0;
  bool frequencyLocked = false;

  std::array<double, 3> subBandEnv{{0.0, 0.0, 0.0}};
  std::array<double, 3> subBandPhase{{0.0, 0.0, 0.0}};
  double squeezeEnv = 0.0;

  // GLOBAL parameters
  std::atomic<bool> synthesisInputStereo{false}; // false=Mid, true=Stereo
  std::atomic<float> gainInDb{0.0f};
  std::atomic<float> gainOutDb{0.0f};
  std::atomic<float> mixPct{100.0f};
  std::atomic<float> monoMakerHz{0.0f};
  std::atomic<float> stereoWidthPct{100.0f};

  // SUB parameters
  std::atomic<float> subharmonicsPct{50.0f};
  std::array<std::atomic<float>, 3> subBandPct{{20.0f, 30.0f, 40.0f}};
  std::array<std::atomic<float>, 3> subBandTrimDb{{0.0f, 0.0f, 0.0f}};
  std::array<std::atomic<bool>, 3> subBandSolo{{false, false, false}};

  // EDGE parameters
  std::atomic<int> edgeMode{0}; // 0=Smooth, 1=Harsh
  std::atomic<float> squeezePct{0.0f};
  std::atomic<float> drivePct{0.0f};
  std::atomic<float> lowCutHz{20.0f};
  std::atomic<float> highCutHz{12000.0f};
  std::atomic<float> tightPunchPct{0.0f};
  std::atomic<float> lowEndPct{0.0f};

  // Telemetry
  std::atomic<float> inLevelDbL{-60.0f};
  std::atomic<float> inLevelDbR{-60.0f};
  std::atomic<float> outLevelDbL{-60.0f};
  std::atomic<float> outLevelDbR{-60.0f};
  std::array<std::atomic<float>, 3> subBandLevelDb{{-60.0f, -60.0f, -60.0f}};
};

} // namespace nova::graviton
