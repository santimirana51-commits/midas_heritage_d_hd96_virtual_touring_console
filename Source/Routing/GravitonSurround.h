#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <atomic>
#include <cmath>
#include <string>
#include <algorithm>

namespace nova::graviton {

/**
 * @brief Surround format modes supported by the Graviton Mk 2 DSP Audio Engine
 * Conforms to MIDAS Heritage-D HD96-24 Chapter 30 (Surround Panning & Immersive 3D Space).
 */
enum class SurroundMode : int {
    Stereo = 0,         // Traditional 2.0 Stereo (L/R)
    LCR = 1,            // 3.0 Left-Center-Right with Center Divergence
    Quad_4_0 = 2,       // 4.0 Quadraphonic (L, R, Lsr, Rsr)
    Surround_5_1 = 3,   // 5.1 Surround (L, R, C, LFE, Lss, Rss)
    Surround_7_1 = 4,   // 7.1 Surround (L, R, C, LFE, Lss, Rss, Lsr, Rsr)
    Immersive_7_1_4 = 5 // 7.1.4 Immersive 3D Spatial (7.1 bed + 4 Height speakers)
};

/**
 * @brief Discrete speaker channel assignments in 7.1.4 spatial domain
 */
enum class SurroundSpeaker : int {
    L = 0,      // Left Front
    R = 1,      // Right Front
    C = 2,      // Center
    LFE = 3,    // Low-Frequency Effects / Subwoofer
    Lss = 4,    // Left Side Surround (5.1/7.1 Ls)
    Rss = 5,    // Right Side Surround (5.1/7.1 Rs)
    Lsr = 6,    // Left Rear Surround
    Rsr = 7,    // Right Rear Surround
    TFL = 8,    // Top Front Left (Height)
    TFR = 9,    // Top Front Right (Height)
    TRL = 10,   // Top Rear Left (Height)
    TRR = 11,   // Top Rear Right (Height)
    Count = 12
};

using SurroundSpeakerGains = std::array<double, static_cast<size_t>(SurroundSpeaker::Count)>;

class SurroundLfeLowPass final {
public:
    void prepare(double sampleRate, double cutoffHz) noexcept
    {
        const double sr = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0,
                                     8000.0, 192000.0);
        const double cutoff = std::clamp(std::isfinite(cutoffHz) ? cutoffHz : 120.0,
                                         40.0, std::min(120.0, sr * 0.45));
        if (std::abs(sr - sampleRate_) > 0.5 ||
            std::abs(cutoff - cutoffHz_) > 0.01) {
            filter_.setCoefficients(makeButterworthLPF(sr, cutoff));
            sampleRate_ = sr;
            cutoffHz_ = cutoff;
        }
    }

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    double process(double input) noexcept
    {
        const double safeInput = sanitize(input);
        return enabled_ ? filter_.process(safeInput) : safeInput;
    }

    void reset() noexcept { filter_.reset(); }

private:
    Biquad filter_;
    double sampleRate_ = -1.0;
    double cutoffHz_ = -1.0;
    bool enabled_ = false;
};

class SurroundPanner final {
public:
    static constexpr size_t kNumSpeakers = static_cast<size_t>(SurroundSpeaker::Count);
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kHalfPi = 1.57079632679489661923;
    static constexpr double kQuarterPi = 0.78539816339744830962;

    static double distanceGain(double distanceM) noexcept
    {
        const double safeDistance = std::clamp(
            std::isfinite(distanceM) ? distanceM : 1.0, 0.1, 100.0);
        return std::min(1.0, 1.0 / safeDistance);
    }

    static const char* getSpeakerName(SurroundSpeaker spk) noexcept {
        switch (spk) {
            case SurroundSpeaker::L:   return "Left (L)";
            case SurroundSpeaker::R:   return "Right (R)";
            case SurroundSpeaker::C:   return "Center (C)";
            case SurroundSpeaker::LFE: return "Sub (LFE)";
            case SurroundSpeaker::Lss: return "Left Side (Lss)";
            case SurroundSpeaker::Rss: return "Right Side (Rss)";
            case SurroundSpeaker::Lsr: return "Left Rear (Lsr)";
            case SurroundSpeaker::Rsr: return "Right Rear (Rsr)";
            case SurroundSpeaker::TFL: return "Top Front L (TFL)";
            case SurroundSpeaker::TFR: return "Top Front R (TFR)";
            case SurroundSpeaker::TRL: return "Top Rear L (TRL)";
            case SurroundSpeaker::TRR: return "Top Rear R (TRR)";
            default:                   return "Unknown";
        }
    }

    static const char* getSpeakerCode(SurroundSpeaker spk) noexcept {
        switch (spk) {
            case SurroundSpeaker::L:   return "L";
            case SurroundSpeaker::R:   return "R";
            case SurroundSpeaker::C:   return "C";
            case SurroundSpeaker::LFE: return "LFE";
            case SurroundSpeaker::Lss: return "Lss";
            case SurroundSpeaker::Rss: return "Rss";
            case SurroundSpeaker::Lsr: return "Lsr";
            case SurroundSpeaker::Rsr: return "Rsr";
            case SurroundSpeaker::TFL: return "TFL";
            case SurroundSpeaker::TFR: return "TFR";
            case SurroundSpeaker::TRL: return "TRL";
            case SurroundSpeaker::TRR: return "TRR";
            default:                   return "?";
        }
    }

    /**
     * @brief Calculate equal-power speaker gains for any 3D coordinate and format.
     * @param mode Target surround/spatial format
     * @param x Normalized X axis: -1.0 (Left) to +1.0 (Right)
     * @param y Normalized Y axis: -1.0 (Rear) to +1.0 (Front)
     * @param z Normalized Z axis: 0.0 (Floor/Ear level) to 1.0 (Ceiling/Height)
     * @param divergence Divergence/Spread: 0.0 (Point Source) to 1.0 (Diffuse Omnidirectional)
     * @param centerPct Center Divergence: 0.0% (Phantom Center) to 100.0% (Discrete Center)
     * @param lfeGainDb Subwoofer send level (-60.0 dB to +10.0 dB)
     */
    static SurroundSpeakerGains calculateGains(SurroundMode mode,
                                              double x, double y, double z,
                                              double divergence,
                                              double centerPct,
                                              double lfeGainDb) noexcept {
        SurroundSpeakerGains gains{};
        gains.fill(0.0);

        const double clampedX = std::clamp(x, -1.0, 1.0);
        const double clampedY = std::clamp(y, -1.0, 1.0);
        const double clampedZ = std::clamp(z, 0.0, 1.0);
        const double clampedDiv = std::clamp(divergence, 0.0, 1.0);
        const double clampedCenter = std::clamp(centerPct * 0.01, 0.0, 1.0);

        // LFE is independent sub-bass send
        if (lfeGainDb > -59.0) {
            gains[static_cast<size_t>(SurroundSpeaker::LFE)] = dbToGain(std::clamp(lfeGainDb, -60.0, 10.0));
        }

        switch (mode) {
            case SurroundMode::Stereo: {
                // Classic equal-power stereo pan
                const double angle = (clampedX + 1.0) * kQuarterPi;
                gains[static_cast<size_t>(SurroundSpeaker::L)] = std::cos(angle);
                gains[static_cast<size_t>(SurroundSpeaker::R)] = std::sin(angle);
                break;
            }

            case SurroundMode::LCR: {
                // 3.0 Left-Center-Right with Center Divergence
                double rawL = 0.0, rawR = 0.0, rawC = 0.0;
                if (clampedX <= 0.0) {
                    const double angle = (clampedX + 1.0) * kHalfPi;
                    rawL = std::cos(angle);
                    rawC = std::sin(angle);
                } else {
                    const double angle = clampedX * kHalfPi;
                    rawC = std::cos(angle);
                    rawR = std::sin(angle);
                }

                // Apply Center Divergence: Center % directs power to C vs phantom L/R
                const double cGain = rawC * std::sqrt(clampedCenter);
                const double phantomBleedPower = (rawC * rawC) * (1.0 - clampedCenter) * 0.5;
                const double lGain = std::sqrt(rawL * rawL + phantomBleedPower);
                const double rGain = std::sqrt(rawR * rawR + phantomBleedPower);

                gains[static_cast<size_t>(SurroundSpeaker::L)] = lGain;
                gains[static_cast<size_t>(SurroundSpeaker::R)] = rGain;
                gains[static_cast<size_t>(SurroundSpeaker::C)] = cGain;
                break;
            }

            case SurroundMode::Quad_4_0: {
                // 4.0 Quadraphonic (FL, FR, RL, RR)
                const double frontWeight = std::sin((clampedY + 1.0) * kQuarterPi);
                const double rearWeight  = std::cos((clampedY + 1.0) * kQuarterPi);
                const double leftWeight  = std::cos((clampedX + 1.0) * kQuarterPi);
                const double rightWeight = std::sin((clampedX + 1.0) * kQuarterPi);

                gains[static_cast<size_t>(SurroundSpeaker::L)]   = frontWeight * leftWeight;
                gains[static_cast<size_t>(SurroundSpeaker::R)]   = frontWeight * rightWeight;
                gains[static_cast<size_t>(SurroundSpeaker::Lsr)] = rearWeight * leftWeight;
                gains[static_cast<size_t>(SurroundSpeaker::Rsr)] = rearWeight * rightWeight;
                break;
            }

            case SurroundMode::Surround_5_1: {
                // 5.1 Surround (L, R, C, LFE, Lss, Rss)
                const double frontWeight = std::sin((clampedY + 1.0) * kQuarterPi);
                const double surrWeight  = std::cos((clampedY + 1.0) * kQuarterPi);

                // Front stage LCR
                double rawL = 0.0, rawR = 0.0, rawC = 0.0;
                if (clampedX <= 0.0) {
                    const double angle = (clampedX + 1.0) * kHalfPi;
                    rawL = std::cos(angle);
                    rawC = std::sin(angle);
                } else {
                    const double angle = clampedX * kHalfPi;
                    rawC = std::cos(angle);
                    rawR = std::sin(angle);
                }
                const double cGain = rawC * std::sqrt(clampedCenter);
                const double phantomBleed = (rawC * rawC) * (1.0 - clampedCenter) * 0.5;
                const double lGain = std::sqrt(rawL * rawL + phantomBleed);
                const double rGain = std::sqrt(rawR * rawR + phantomBleed);

                gains[static_cast<size_t>(SurroundSpeaker::L)]   = frontWeight * lGain;
                gains[static_cast<size_t>(SurroundSpeaker::R)]   = frontWeight * rGain;
                gains[static_cast<size_t>(SurroundSpeaker::C)]   = frontWeight * cGain;

                // Surround stage
                const double leftSurr = std::cos((clampedX + 1.0) * kQuarterPi);
                const double rightSurr = std::sin((clampedX + 1.0) * kQuarterPi);
                gains[static_cast<size_t>(SurroundSpeaker::Lss)] = surrWeight * leftSurr;
                gains[static_cast<size_t>(SurroundSpeaker::Rss)] = surrWeight * rightSurr;
                break;
            }

            case SurroundMode::Surround_7_1: {
                // 7.1 Surround (L, R, C, LFE, Lss, Rss, Lsr, Rsr)
                double wFront = 0.0, wSide = 0.0, wRear = 0.0;
                if (clampedY >= 0.0) {
                    wFront = std::sin(clampedY * kHalfPi);
                    wSide  = std::cos(clampedY * kHalfPi);
                    wRear  = 0.0;
                } else {
                    const double u = -clampedY;
                    wFront = 0.0;
                    wSide  = std::cos(u * kHalfPi);
                    wRear  = std::sin(u * kHalfPi);
                }

                // Front LCR
                double rawL = 0.0, rawR = 0.0, rawC = 0.0;
                if (clampedX <= 0.0) {
                    const double angle = (clampedX + 1.0) * kHalfPi;
                    rawL = std::cos(angle);
                    rawC = std::sin(angle);
                } else {
                    const double angle = clampedX * kHalfPi;
                    rawC = std::cos(angle);
                    rawR = std::sin(angle);
                }
                const double cGain = rawC * std::sqrt(clampedCenter);
                const double phantomBleed = (rawC * rawC) * (1.0 - clampedCenter) * 0.5;
                const double lGain = std::sqrt(rawL * rawL + phantomBleed);
                const double rGain = std::sqrt(rawR * rawR + phantomBleed);

                gains[static_cast<size_t>(SurroundSpeaker::L)] = wFront * lGain;
                gains[static_cast<size_t>(SurroundSpeaker::R)] = wFront * rGain;
                gains[static_cast<size_t>(SurroundSpeaker::C)] = wFront * cGain;

                // Side and Rear Surrounds
                const double leftPan  = std::cos((clampedX + 1.0) * kQuarterPi);
                const double rightPan = std::sin((clampedX + 1.0) * kQuarterPi);

                gains[static_cast<size_t>(SurroundSpeaker::Lss)] = wSide * leftPan;
                gains[static_cast<size_t>(SurroundSpeaker::Rss)] = wSide * rightPan;
                gains[static_cast<size_t>(SurroundSpeaker::Lsr)] = wRear * leftPan;
                gains[static_cast<size_t>(SurroundSpeaker::Rsr)] = wRear * rightPan;
                break;
            }

            case SurroundMode::Immersive_7_1_4: {
                // 7.1.4 Immersive 3D Spatial (7.1 bed layer + 4 height speakers)
                const double bedWeight    = std::cos(clampedZ * kHalfPi);
                const double heightWeight = std::sin(clampedZ * kHalfPi);

                // Calculate 7.1 bed layer
                auto bedGains = calculateGains(SurroundMode::Surround_7_1, clampedX, clampedY, 0.0, 0.0, centerPct, -120.0);
                for (size_t i = 0; i < kNumSpeakers; ++i) {
                    if (i != static_cast<size_t>(SurroundSpeaker::LFE)) {
                        gains[i] = bedGains[i] * bedWeight;
                    }
                }

                // 4 Height speakers (Top Front Left/Right, Top Rear Left/Right)
                const double frontH = std::sin((clampedY + 1.0) * kQuarterPi);
                const double rearH  = std::cos((clampedY + 1.0) * kQuarterPi);
                const double leftH  = std::cos((clampedX + 1.0) * kQuarterPi);
                const double rightH = std::sin((clampedX + 1.0) * kQuarterPi);

                gains[static_cast<size_t>(SurroundSpeaker::TFL)] = heightWeight * frontH * leftH;
                gains[static_cast<size_t>(SurroundSpeaker::TFR)] = heightWeight * frontH * rightH;
                gains[static_cast<size_t>(SurroundSpeaker::TRL)] = heightWeight * rearH * leftH;
                gains[static_cast<size_t>(SurroundSpeaker::TRR)] = heightWeight * rearH * rightH;
                break;
            }
        }

        // Apply Divergence / Object Spread across active speakers
        if (clampedDiv > 1.0e-5) {
            std::array<bool, kNumSpeakers> isActive{};
            isActive.fill(false);
            size_t activeCount = 0;

            switch (mode) {
                case SurroundMode::Stereo:
                    isActive[0] = isActive[1] = true; activeCount = 2; break;
                case SurroundMode::LCR:
                    isActive[0] = isActive[1] = isActive[2] = true; activeCount = 3; break;
                case SurroundMode::Quad_4_0:
                    isActive[0] = isActive[1] = isActive[6] = isActive[7] = true; activeCount = 4; break;
                case SurroundMode::Surround_5_1:
                    isActive[0] = isActive[1] = isActive[2] = isActive[4] = isActive[5] = true; activeCount = 5; break;
                case SurroundMode::Surround_7_1:
                    isActive[0] = isActive[1] = isActive[2] = isActive[4] = isActive[5] = isActive[6] = isActive[7] = true; activeCount = 7; break;
                case SurroundMode::Immersive_7_1_4:
                    for (size_t s = 0; s < kNumSpeakers; ++s) {
                        if (s != static_cast<size_t>(SurroundSpeaker::LFE)) {
                            isActive[s] = true;
                        }
                    }
                    activeCount = 11;
                    break;
            }

            if (activeCount > 0) {
                const double diffuseGain = 1.0 / std::sqrt(static_cast<double>(activeCount));
                double sumSq = 0.0;
                for (size_t s = 0; s < kNumSpeakers; ++s) {
                    if (isActive[s]) {
                        gains[s] = (1.0 - clampedDiv) * gains[s] + clampedDiv * diffuseGain;
                        sumSq += gains[s] * gains[s];
                    }
                }
                if (sumSq > 1.0e-9) {
                    const double norm = 1.0 / std::sqrt(sumSq);
                    for (size_t s = 0; s < kNumSpeakers; ++s) {
                        if (isActive[s]) {
                            gains[s] *= norm;
                        }
                    }
                }
            }
        }

        return gains;
    }

    /**
     * @brief Downmix surround 7.1.4 array into standard stereo (ITU-R BS.775 fold-down).
     */
    static void downmixToStereo(const SurroundSpeakerGains& in, double& outL, double& outR) noexcept {
        constexpr double invSqrt2 = 0.70710678118654752440;
        constexpr double surrAtt  = 0.70710678118654752440; // -3 dB fold-down for surrounds
        constexpr double heightAtt = 0.50000000000000000000; // -6 dB fold-down for heights

        const double centerFold = in[static_cast<size_t>(SurroundSpeaker::C)] * invSqrt2;

        outL = in[static_cast<size_t>(SurroundSpeaker::L)] + centerFold
             + in[static_cast<size_t>(SurroundSpeaker::Lss)] * surrAtt
             + in[static_cast<size_t>(SurroundSpeaker::Lsr)] * surrAtt
             + in[static_cast<size_t>(SurroundSpeaker::TFL)] * heightAtt
             + in[static_cast<size_t>(SurroundSpeaker::TRL)] * heightAtt;

        outR = in[static_cast<size_t>(SurroundSpeaker::R)] + centerFold
             + in[static_cast<size_t>(SurroundSpeaker::Rss)] * surrAtt
             + in[static_cast<size_t>(SurroundSpeaker::Rsr)] * surrAtt
             + in[static_cast<size_t>(SurroundSpeaker::TFR)] * heightAtt
             + in[static_cast<size_t>(SurroundSpeaker::TRR)] * heightAtt;
    }
};

/**
 * @brief Binaural 3D Headphone Spatializer
 * Models Interaural Time Difference (ITD) and Interaural Level Difference (ILD)
 * for real-time headphone preview of the full 7.1.4 surround soundfield.
 */
class BinauralSpatializer final {
public:
    static constexpr size_t kDelayRingSize = 256;
    static constexpr size_t kNumSpeakers = static_cast<size_t>(SurroundSpeaker::Count);

    BinauralSpatializer() noexcept {
        reset();
    }

    void reset() noexcept {
        writePos = 0;
        for (size_t s = 0; s < kNumSpeakers; ++s) {
            delayBuffer[s].fill(0.0);
            shadowFilterL[s] = 0.0;
            shadowFilterR[s] = 0.0;
        }
    }

    /**
     * @brief Process one multi-channel surround sample frame into 3D binaural stereo.
     */
    void processFrame(const SurroundSpeakerGains& frame, double sampleRate, double& binauralL, double& binauralR) noexcept {
        const double sr = (sampleRate > 8000.0) ? sampleRate : 96000.0;
        // Azimuth in degrees for each speaker: negative = Left, positive = Right
        static constexpr double speakerAzimuthDeg[kNumSpeakers] = {
            -30.0,  // L
            +30.0,  // R
              0.0,  // C
              0.0,  // LFE
            -90.0,  // Lss
            +90.0,  // Rss
           -140.0,  // Lsr
           +140.0,  // Rsr
            -45.0,  // TFL
            +45.0,  // TFR
           -135.0,  // TRL
           +135.0   // TRR
        };

        binauralL = 0.0;
        binauralR = 0.0;

        const size_t currentW = writePos;
        writePos = (writePos + 1) & (kDelayRingSize - 1);

        for (size_t s = 0; s < kNumSpeakers; ++s) {
            const double sig = frame[s];
            delayBuffer[s][currentW] = sig;

            if (std::abs(sig) < 1.0e-8 && std::abs(shadowFilterL[s]) < 1.0e-8 && std::abs(shadowFilterR[s]) < 1.0e-8) {
                continue;
            }

            const double azRad = speakerAzimuthDeg[s] * (SurroundPanner::kPi / 180.0);
            const double sinAz = std::sin(azRad);

            // Woodworth ITD model (max ~0.65 ms delay across ears)
            // Delay for Left and Right ear
            double delaySamplesL = 0.0;
            double delaySamplesR = 0.0;
            if (sinAz >= 0.0) { // Source on the Right: arrives first at Right ear, delayed to Left ear
                delaySamplesL = (sr * 0.00065) * (sinAz + azRad * 0.2);
            } else {            // Source on the Left: arrives first at Left ear, delayed to Right ear
                delaySamplesR = (sr * 0.00065) * (-sinAz - azRad * 0.2);
            }

            const size_t delayIntL = std::min(kDelayRingSize - 1, static_cast<size_t>(std::max(0.0, delaySamplesL)));
            const size_t delayIntR = std::min(kDelayRingSize - 1, static_cast<size_t>(std::max(0.0, delaySamplesR)));

            const double delayedSigL = delayBuffer[s][(currentW + kDelayRingSize - delayIntL) & (kDelayRingSize - 1)];
            const double delayedSigR = delayBuffer[s][(currentW + kDelayRingSize - delayIntR) & (kDelayRingSize - 1)];

            // Head Shadow ILD (low-pass filter on contralateral ear)
            double ildL = 1.0;
            double ildR = 1.0;
            if (sinAz > 0.0) { // Right side: attenuate Left ear
                ildL = std::clamp(1.0 - 0.45 * sinAz, 0.4, 1.0);
            } else if (sinAz < 0.0) { // Left side: attenuate Right ear
                ildR = std::clamp(1.0 + 0.45 * sinAz, 0.4, 1.0);
            }

            // Simple 1-pole head shadow smoothing
            constexpr double alpha = 0.35;
            shadowFilterL[s] += alpha * (delayedSigL * ildL - shadowFilterL[s]);
            shadowFilterR[s] += alpha * (delayedSigR * ildR - shadowFilterR[s]);

            binauralL += shadowFilterL[s];
            binauralR += shadowFilterR[s];
        }
    }

private:
    size_t writePos{0};
    std::array<std::array<double, kDelayRingSize>, kNumSpeakers> delayBuffer{};
    std::array<double, kNumSpeakers> shadowFilterL{};
    std::array<double, kNumSpeakers> shadowFilterR{};
};

} // namespace nova::graviton
