#pragma once

#include "DSP/GravitonDSP.h"

namespace nova::graviton {
// MIDAS Heritage-D HD96-24 Dynamic Control: Compressor Operating Modes.
enum class CompressorMode : int {
  Off = 0,        // Compressor disabled (gain reduction passthrough)
  Corrective = 1, // Clean corrective gain reduction (low ratio, transparent)
  Adaptive = 2,   // Level-tracking adaptive threshold (auto gain staging)
  Creative = 3,   // Colored / high-ratio creative compression
  Vintage =
      4 // Vintage opto/FET style darker preset (lower ratio, higher threshold)
};

enum class GateMode : int {
  DualGate = 0,          // Midas Pro precision noise gate
  TransientDesigner = 1, // Differential fast/slow envelope transient shaper
  ExpanderDucker = 2     // Gentle downward expander / ducker
};

struct DynamicsSettings {
  bool gateEnabled = false;
  bool compressorEnabled = false;
  CompressorMode compressorMode = CompressorMode::Corrective;
  GateMode gateMode = GateMode::DualGate;
  Sample gateThresholdDb = -45.0;
  Sample gateRatio = 4.0;
  Sample gateHoldMs = 50.0;
  Sample expanderRangeDb = 6.0;
  Sample compressorThresholdDb = -18.0;
  Sample compressorRatio = 4.0;
  Sample compressorKneeDb = 3.0;
  int compressorKneeMode = 2; // 0=Hard, 1=Linear, 2=Soft
  Sample compressorInputGainDb = 0.0;
  Sample compressorMakeupGainDb = 0.0;
  Sample compressorRmsMs = 1.0;
  Sample sampleRate = 48000.0;
  // One-pole envelope coefficients, calculated once per block from
  // attack/release.
  Sample attackCoefficient = 0.0;
  Sample releaseCoefficient = 0.0;
};

struct DynamicsResult {
  Sample sample = 0.0;
  Sample inLevelDb = -120.0;
  Sample sideLevelDb = -120.0;
  Sample outLevelDb = -120.0;
  Sample gateReductionDb = 0.0;
  Sample compressorReductionDb = 0.0;
};

// Allocation-free gate/expander and compressor detector with double state/math.
// Call from the audio thread only; settings are expected to be snapshotted per
// block.
class DynamicsProcessor {
public:
  DynamicsResult process(Sample input,
                         const DynamicsSettings &settings) noexcept {
    return processWithSidechain(input, input, settings);
  }

  DynamicsResult
  processWithSidechain(Sample input, Sample keySignal,
                       const DynamicsSettings &settings) noexcept {
    Sample sample = sanitize(input);
    if (std::abs(settings.compressorInputGainDb) > 1.0e-5)
      sample *= dbToGain(
          clampFinite(settings.compressorInputGainDb, -30.0, 30.0, 0.0));

    const Sample inDb = gainToDb(std::max(std::abs(sample), 1.0e-6), -120.0);
    const Sample keySan = sanitize(keySignal);
    const Sample sideDb = gainToDb(std::max(std::abs(keySan), 1.0e-6), -120.0);

    Sample keyMag = std::abs(keySan);
    if (settings.compressorRmsMs > 0.05 && settings.sampleRate > 1000.0) {
      const Sample rmsCoeff = clampFinite(
          std::exp(-1.0 / (settings.sampleRate *
                           std::max(0.0001, settings.compressorRmsMs * 0.001))),
          0.0, 1.0, 0.0);
      rmsState_ =
          sanitize(rmsCoeff * rmsState_ + (1.0 - rmsCoeff) * (keyMag * keyMag));
      keyMag = std::sqrt(std::max(1.0e-12, rmsState_));
    }

    const Sample target = std::max(keyMag, dbToGain(-120.0));
    const Sample coefficient =
        target > envelope_
            ? clampFinite(settings.attackCoefficient, 0.0, 1.0, 0.0)
            : clampFinite(settings.releaseCoefficient, 0.0, 1.0, 0.0);
    envelope_ =
        sanitize(coefficient * envelope_ + (1.0 - coefficient) * target);
    const Sample envelopeDb = gainToDb(std::max(envelope_, 1.0e-6), -120.0);

    const Sample srSafe = std::max(8000.0, settings.sampleRate);
    const Sample fastCoeff =
        clampFinite(std::exp(-1.0 / (srSafe * 0.0015)), 0.0, 1.0, 0.9);
    const Sample slowCoeff =
        clampFinite(std::exp(-1.0 / (srSafe * 0.035)), 0.0, 1.0, 0.99);
    fastEnv_ = sanitize(fastCoeff * fastEnv_ + (1.0 - fastCoeff) * keyMag);
    slowEnv_ = sanitize(slowCoeff * slowEnv_ + (1.0 - slowCoeff) * keyMag);

    DynamicsResult result;
    result.inLevelDb = inDb;
    result.sideLevelDb = sideDb;

    if (settings.gateEnabled) {
      const Sample threshold =
          clampFinite(settings.gateThresholdDb, -80.0, 0.0, -45.0);
      const Sample ratio = clampFinite(settings.gateRatio, 1.0, 10.0, 4.0);
      const Sample rangeDb =
          clampFinite(settings.expanderRangeDb, 0.0, 80.0, 6.0);
      const Sample holdCoefficient = clampFinite(
          std::exp(-1.0 /
                   (srSafe * std::max(settings.gateHoldMs * 0.001, 0.01))),
          0.0, 1.0, 0.0);

      if (settings.gateMode == GateMode::TransientDesigner) {
        // Differential transient shaper: fast attack punch & controlled sustain
        // tail
        const Sample diffDb = gainToDb(std::max(fastEnv_, 1.0e-6), -120.0) -
                              gainToDb(std::max(slowEnv_, 1.0e-6), -120.0);
        if (diffDb > 0.5) {
          // Attack transient boost (up to +6 dB)
          const Sample attackBoostDb =
              clampFinite(diffDb * 0.45, 0.0, 6.0, 0.0);
          gateReductionDb_ *= holdCoefficient;
          result.gateReductionDb = 0.0;
          sample *= dbToGain(attackBoostDb);
        } else if (envelopeDb < threshold) {
          // Gentle sustain/tail attenuation
          const Sample tailAttenDb =
              clampFinite((threshold - envelopeDb) * 0.65, 0.0, rangeDb, 0.0);
          gateReductionDb_ = tailAttenDb > gateReductionDb_
                                 ? tailAttenDb
                                 : gateReductionDb_ * holdCoefficient +
                                       tailAttenDb * (1.0 - holdCoefficient);
          result.gateReductionDb =
              clampFinite(gateReductionDb_, 0.0, 80.0, 0.0);
          sample *=
              dbToGain(clampFinite(-result.gateReductionDb, -80.0, 0.0, 0.0));
        } else {
          gateReductionDb_ *= holdCoefficient;
          result.gateReductionDb = 0.0;
        }
      } else if (settings.gateMode == GateMode::ExpanderDucker) {
        // Smooth downward expander: 1.5:1 - 2.5:1 gentle slope over wider range
        const Sample expanderKneeStart = threshold + 4.0;
        if (envelopeDb < expanderKneeStart) {
          const Sample belowDb =
              clampFinite(expanderKneeStart - envelopeDb, 0.0, 80.0, 0.0);
          const Sample gentleSlope =
              clampFinite(1.0 + (ratio - 1.0) * 0.25, 1.1, 3.0, 1.5);
          const Sample expRangeDb = std::max(rangeDb, 18.0);
          const Sample attenuationDb =
              clampFinite(belowDb * (gentleSlope - 1.0), 0.0, expRangeDb, 0.0);
          gateReductionDb_ = gateReductionDb_ * 0.92 + attenuationDb * 0.08;
          result.gateReductionDb =
              clampFinite(gateReductionDb_, 0.0, 80.0, 0.0);
          sample *=
              dbToGain(clampFinite(-result.gateReductionDb, -80.0, 0.0, 0.0));
        } else {
          gateReductionDb_ *= holdCoefficient;
          if (gateReductionDb_ < 1.0e-4)
            gateReductionDb_ = 0.0;
          result.gateReductionDb = 0.0;
        }
      } else if (envelopeDb < threshold) {
        const Sample belowThresholdDb =
            clampFinite(threshold - envelopeDb, 0.0, 80.0, 0.0);
        const Sample attenuationDb =
            clampFinite(belowThresholdDb * ratio, 0.0, rangeDb, 0.0);
        gateReductionDb_ = attenuationDb > gateReductionDb_
                               ? attenuationDb
                               : gateReductionDb_ * holdCoefficient +
                                     attenuationDb * (1.0 - holdCoefficient);
        result.gateReductionDb = clampFinite(gateReductionDb_, 0.0, 80.0, 0.0);
        sample *=
            dbToGain(clampFinite(-result.gateReductionDb, -80.0, 0.0, 0.0));
      } else {
        gateReductionDb_ *= holdCoefficient;
        if (gateReductionDb_ < 1.0e-4)
          gateReductionDb_ = 0.0;
        result.gateReductionDb = 0.0;
      }
    } else {
      const Sample holdCoefficient = clampFinite(
          std::exp(-1.0 /
                   (srSafe * std::max(settings.gateHoldMs * 0.001, 0.01))),
          0.0, 1.0, 0.0);
      gateReductionDb_ *= holdCoefficient;
      if (gateReductionDb_ < 1.0e-4)
        gateReductionDb_ = 0.0;
      result.gateReductionDb = 0.0;
    }

    if (settings.compressorEnabled &&
        settings.compressorMode != CompressorMode::Off) {
      Sample threshold =
          clampFinite(settings.compressorThresholdDb, -60.0, 0.0, -18.0);
      Sample ratio = clampFinite(settings.compressorRatio, 1.0, 20.0, 4.0);
      Sample effectiveThresholdDb = threshold;
      Sample effectiveRatio = ratio;
      switch (settings.compressorMode) {
      case CompressorMode::Adaptive:
        // Optical 2A Tube: smooth program-dependent leveling with lower
        // threshold & gentle ratio
        effectiveThresholdDb =
            clampFinite(threshold - 2.0, -60.0, 0.0, threshold);
        effectiveRatio = clampFinite(ratio * 0.85, 1.5, 10.0, 3.0);
        break;
      case CompressorMode::Creative:
        // FET 76 All-Buttons: aggressive high-ratio slam
        effectiveThresholdDb =
            clampFinite(threshold - 3.0, -60.0, 0.0, threshold);
        effectiveRatio = clampFinite(ratio * 2.0, 1.0, 40.0, ratio);
        break;
      case CompressorMode::Vintage:
        // Vari-Mu Master Glue: smooth low-threshold bus glue
        effectiveThresholdDb = std::min(threshold, -20.0);
        effectiveRatio = std::min(ratio, 6.0);
        break;
      case CompressorMode::Corrective:
      case CompressorMode::Off:
      default:
        break;
      }

      Sample kneeDb = clampFinite(settings.compressorKneeDb, 0.0, 24.0, 3.0);
      if (settings.compressorKneeMode == 0) // Hard
        kneeDb = 0.0;

      Sample reductionDb = 0.0;
      if (kneeDb <= 0.001) {
        if (envelopeDb > threshold) {
          const Sample compressedDb =
              effectiveThresholdDb +
              (envelopeDb - effectiveThresholdDb) / effectiveRatio;
          reductionDb = clampFinite(envelopeDb - compressedDb, 0.0, 60.0, 0.0);
        }
      } else {
        const Sample halfKnee = kneeDb * 0.5;
        const Sample kneeStartDb = threshold - halfKnee;
        const Sample kneeEndDb = threshold + halfKnee;
        if (envelopeDb > kneeStartDb) {
          if (envelopeDb >= kneeEndDb) {
            const Sample compressedDb =
                effectiveThresholdDb +
                (envelopeDb - effectiveThresholdDb) / effectiveRatio;
            reductionDb =
                clampFinite(envelopeDb - compressedDb, 0.0, 60.0, 0.0);
          } else if (settings.compressorKneeMode == 1) // Linear knee
          {
            const Sample t = (envelopeDb - kneeStartDb) / kneeDb;
            const Sample slope = 1.0 - (1.0 / effectiveRatio);
            reductionDb = clampFinite(
                t * (envelopeDb - kneeStartDb) * slope * 0.5, 0.0, 60.0, 0.0);
          } else // Soft knee
          {
            const Sample normalized =
                clampFinite((envelopeDb - kneeStartDb) / kneeDb, 0.0, 1.0, 0.0);
            const Sample softKneeDelta = (normalized * normalized) * halfKnee;
            const Sample compressedDb =
                effectiveThresholdDb +
                (envelopeDb - effectiveThresholdDb) / effectiveRatio +
                softKneeDelta;
            reductionDb =
                clampFinite(envelopeDb - compressedDb, 0.0, 60.0, 0.0);
          }
        }
      }

      if (reductionDb > 0.0) {
        result.compressorReductionDb = clampFinite(reductionDb, 0.0, 60.0, 0.0);
        sample *= dbToGain(
            clampFinite(-result.compressorReductionDb, -60.0, 0.0, 0.0));
      }
      const Sample makeupGainDb =
          clampFinite(settings.compressorMakeupGainDb, -24.0, 24.0, 0.0);
      if (std::abs(makeupGainDb) > 1.0e-6)
        sample *= dbToGain(makeupGainDb);
    }
    result.sample = sanitize(sample);
    result.outLevelDb =
        gainToDb(std::max(std::abs(result.sample), 1.0e-6), -120.0);
    return result;
  }

  void reset() noexcept {
    envelope_ = 0.0;
    gateReductionDb_ = 0.0;
    rmsState_ = 0.0;
    fastEnv_ = 0.0;
    slowEnv_ = 0.0;
  }
  Sample envelope() const noexcept { return envelope_; }

private:
  Sample envelope_ = 0.0;
  Sample gateReductionDb_ = 0.0;
  Sample rmsState_ = 0.0;
  Sample fastEnv_ = 0.0;
  Sample slowEnv_ = 0.0;
};
} // namespace nova::graviton
