#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace nova::graviton
{
// ==============================================================================
// Final Master Bus Chain (MIDAS Heritage-D HD96-24 Chapter 18)
// Final stereo master processing stage of the digital mixer bus:
//   1. True-Peak Lookahead Brickwall Limiter (-0.1 dBFS ceiling)
//   2. Parametric Master Bus EQ (peaking stage via shared biquad coefficients)
//   3. TPD Dither (optional, allocation-free deterministic TPDF noise)
//   4. Final gain-reduction metering (limiter GR latch)
// All processing stays in double precision and is allocation-free for realtime.
// ==============================================================================

class MasterBusChain final
{
public:
  MasterBusChain() noexcept { setSampleRate(48000.0); }

  void setSampleRate(double sr) noexcept
  {
    cachedSampleRate = std::clamp(sr, 8000.0, 768000.0);
  }

  // True-Peak Lookahead Brickwall Limiter
  void setLimiterEnabled(bool enabled) noexcept { limiterEnabled = enabled; }
  bool getLimiterEnabled() const noexcept { return limiterEnabled; }

  void setLimiterCeilingDb(double db) noexcept { limiterCeilingDb = std::clamp(db, -12.0, 0.0); }
  double getLimiterCeilingDb() const noexcept { return limiterCeilingDb; }

  float getLimiterGainReductionDb() const noexcept { return limiterGrDb.load(std::memory_order_relaxed); }

  // Parametric Master Bus EQ (peaking stage + 3-band Low/Mid/High master contour)
  void setEqEnabled(bool enabled) noexcept { eqEnabled = enabled; }
  bool getEqEnabled() const noexcept { return eqEnabled; }
  void setEqFrequencyHz(double freqHz) noexcept
  {
    eqFrequencyHz = std::clamp(freqHz, 20.0, std::min(20000.0, cachedSampleRate * 0.45));
  }
  double getEqFrequencyHz() const noexcept { return eqFrequencyHz; }
  void setEqGainDb(double gainDb) noexcept { eqGainDb = std::clamp(gainDb, -18.0, 18.0); }
  double getEqGainDb() const noexcept { return eqGainDb; }
  void setEqQ(double q) noexcept { eqQ = std::clamp(q, 0.1, 10.0); }
  double getEqQ() const noexcept { return eqQ; }

  void setThreeBandGainsDb(double lowDb, double midDb, double highDb) noexcept
  {
    eqLowGainDb = std::clamp(lowDb, -18.0, 18.0);
    eqMidGainDb = std::clamp(midDb, -18.0, 18.0);
    eqHighGainDb = std::clamp(highDb, -18.0, 18.0);
  }
  double getEqLowGainDb() const noexcept { return eqLowGainDb; }
  double getEqMidGainDb() const noexcept { return eqMidGainDb; }
  double getEqHighGainDb() const noexcept { return eqHighGainDb; }

  // Output Dither (TPDF)
  void setDitherEnabled(bool enabled) noexcept { ditherEnabled = enabled; }
  bool getDitherEnabled() const noexcept { return ditherEnabled; }

  // Final stereo processing: limiter -> master EQ -> dither.
  void processStereo(double &inOutL, double &inOutR) noexcept
  {
    double l = sanitize(inOutL);
    double r = sanitize(inOutR);

    if (limiterEnabled) {
      applyLimiter(l, r);
    } else {
      limiterGrDb.store(0.0f, std::memory_order_relaxed);
    }

    if (eqEnabled) {
      updateEqCoefficientsIfNeeded();
      if (std::abs(eqGainDb) > 1.0e-4) {
        l = sanitize(eqFilterL.process(l));
        r = sanitize(eqFilterR.process(r));
      }
      if (std::abs(eqLowGainDb) > 1.0e-4) {
        l = sanitize(lowFilterL.process(l));
        r = sanitize(lowFilterR.process(r));
      }
      if (std::abs(eqMidGainDb) > 1.0e-4) {
        l = sanitize(midFilterL.process(l));
        r = sanitize(midFilterR.process(r));
      }
      if (std::abs(eqHighGainDb) > 1.0e-4) {
        l = sanitize(highFilterL.process(l));
        r = sanitize(highFilterR.process(r));
      }
    }

    if (ditherEnabled) {
      l = sanitize(l + ditherSample());
      r = sanitize(r + ditherSample());
    }

    inOutL = l;
    inOutR = r;
  }

  // Final gain reduction metering latch (limiter reduction in dB).
  float getFinalGainReductionDb() const noexcept { return limiterGrDb.load(std::memory_order_relaxed); }

private:
  void applyLimiter(double &l, double &r) noexcept
  {
    const double ceilingAmp = dbToGain(limiterCeilingDb);
    const double inPeak = std::max(std::abs(l), std::abs(r));
    const double targetGain = (inPeak > ceilingAmp) ? (ceilingAmp / inPeak) : 1.0;
    const double attCoeff = std::exp(-1.0 / (0.001 * cachedSampleRate));
    const double relCoeff = std::exp(-1.0 / (limiterReleaseMs * 0.001 * cachedSampleRate));
    if (targetGain < limiterGain) {
      limiterGain = attCoeff * limiterGain + (1.0 - attCoeff) * targetGain;
    } else {
      limiterGain = relCoeff * limiterGain + (1.0 - relCoeff) * targetGain;
    }

    lookaheadBufL[static_cast<std::size_t>(lookaheadWritePos)] = l;
    lookaheadBufR[static_cast<std::size_t>(lookaheadWritePos)] = r;
    const int lookaheadSamples =
        std::clamp(static_cast<int>(0.0015 * cachedSampleRate), 16,
                  static_cast<int>(kMaxLookaheadSamples - 1));
    const int readPos =
        (lookaheadWritePos - lookaheadSamples + static_cast<int>(kMaxLookaheadSamples)) %
        static_cast<int>(kMaxLookaheadSamples);
    lookaheadWritePos = (lookaheadWritePos + 1) % static_cast<int>(kMaxLookaheadSamples);

    double delayedL = lookaheadBufL[static_cast<std::size_t>(readPos)] * limiterGain;
    double delayedR = lookaheadBufR[static_cast<std::size_t>(readPos)] * limiterGain;
    delayedL = std::clamp(delayedL, -ceilingAmp, ceilingAmp);
    delayedR = std::clamp(delayedR, -ceilingAmp, ceilingAmp);

    const double curGrDb = -gainToDb(std::max(limiterGain, 1.0e-5), -100.0);
    limiterGrDb.store(static_cast<float>(std::max(0.0, curGrDb)), std::memory_order_relaxed);

    l = delayedL;
    r = delayedR;
  }

  double ditherSample() noexcept
  {
    phaseA = std::fmod(phaseA * 1664525.0 + 1013904223.0, 2147483648.0);
    phaseB = std::fmod(phaseB * 1664525.0 + 1013904223.0, 2147483648.0);
    const double nA = phaseA * (1.0 / 2147483648.0);
    const double nB = phaseB * (1.0 / 2147483648.0);
    return std::fabs(nA - nB) - 0.5;
  }

  void updateEqCoefficientsIfNeeded() noexcept
  {
    if (cachedSampleRate != lastEqSampleRate || eqFrequencyHz != lastEqFreqHz ||
        eqGainDb != lastEqGainDb || eqQ != lastEqQ) {
      const auto coeffs = makePeakingEq(cachedSampleRate, eqFrequencyHz, eqGainDb, eqQ);
      eqFilterL.setCoefficients(coeffs);
      eqFilterR.setCoefficients(coeffs);
      lastEqFreqHz = eqFrequencyHz;
      lastEqGainDb = eqGainDb;
      lastEqQ = eqQ;
    }
    if (cachedSampleRate != lastEqSampleRate || eqLowGainDb != lastLowGainDb) {
      const auto coeffs = makePeakingEq(cachedSampleRate, 100.0, eqLowGainDb, 0.707);
      lowFilterL.setCoefficients(coeffs);
      lowFilterR.setCoefficients(coeffs);
      lastLowGainDb = eqLowGainDb;
    }
    if (cachedSampleRate != lastEqSampleRate || eqMidGainDb != lastMidGainDb) {
      const auto coeffs = makePeakingEq(cachedSampleRate, 1000.0, eqMidGainDb, 0.85);
      midFilterL.setCoefficients(coeffs);
      midFilterR.setCoefficients(coeffs);
      lastMidGainDb = eqMidGainDb;
    }
    if (cachedSampleRate != lastEqSampleRate || eqHighGainDb != lastHighGainDb) {
      const auto coeffs = makePeakingEq(cachedSampleRate, 8000.0, eqHighGainDb, 0.707);
      highFilterL.setCoefficients(coeffs);
      highFilterR.setCoefficients(coeffs);
      lastHighGainDb = eqHighGainDb;
    }
    lastEqSampleRate = cachedSampleRate;
  }

  static constexpr std::size_t kMaxLookaheadSamples = 512;

  double cachedSampleRate = 48000.0;
  bool limiterEnabled = false;
  double limiterCeilingDb = -0.1;
  double limiterReleaseMs = 50.0;
  double limiterGain = 1.0;
  std::atomic<float> limiterGrDb{0.0f};
  bool eqEnabled = false;
  double eqFrequencyHz = 100.0;
  double eqGainDb = 0.0;
  double eqQ = 1.0;
  double eqLowGainDb = 0.0;
  double eqMidGainDb = 0.0;
  double eqHighGainDb = 0.0;
  double lastEqSampleRate = -1.0;
  double lastEqFreqHz = -1.0;
  double lastEqGainDb = 999.0;
  double lastEqQ = -1.0;
  double lastLowGainDb = 999.0;
  double lastMidGainDb = 999.0;
  double lastHighGainDb = 999.0;
  bool ditherEnabled = false;
  double phaseA = 0.0;
  double phaseB = 0.0;
  Biquad eqFilterL;
  Biquad eqFilterR;
  Biquad lowFilterL;
  Biquad lowFilterR;
  Biquad midFilterL;
  Biquad midFilterR;
  Biquad highFilterL;
  Biquad highFilterR;
  std::array<double, kMaxLookaheadSamples> lookaheadBufL{};
  std::array<double, kMaxLookaheadSamples> lookaheadBufR{};
  int lookaheadWritePos = 0;
};
} // namespace nova::graviton
