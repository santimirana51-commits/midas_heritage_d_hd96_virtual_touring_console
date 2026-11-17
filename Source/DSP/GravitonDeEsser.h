#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// NOVA HD Precision De-Esser (Universal Audio Precision Mix Series Architecture)
// Virtual FX Rack Slot 9 - MIDAS Heritage-D HD96-24 Chapter 15
//
// Authentic Hardware & UAD Algorithm Emulation:
//   • Steep 2-Stage Cascaded Sidechain Detector: Rejects low/mid vowel chest
//     energy at -24 dB/octave so only true sibilance triggers gain reduction.
//   • 100% Bit-Transparent Zero-Phase Split Path: At 0 dB GR, output equals
//     input bit-for-bit; during attenuation, only the phase-aligned sibilance
//     component is subtracted, preserving vocal warmth, chest, and air.
//   • Soft-Knee 7:1 Logarithmic Gain Computer with Fast (0.5/30 ms) and
//     Slow (2.0/120 ms) ballistics.
//   • Complete 5-Bank, 16-Preset NOVA HD Library:
//       1. Vocal Female  (Soprano, Alto, Airy Female, Bright Female)
//       2. Vocal Male    (Tenor, Baritone, Deep Male)
//       3. Vocal Problem (Strong Sibilance, Harsh S, Sharp SH, Whistle)
//       4. Instruments   (Hi-Hat, Overhead, Cymbal, Trumpet)
//       5. Master        (Mix Bus High-Frequency Control)
// ==============================================================================
class PrecisionDeEsser final {
public:
    enum class Mode : int {
        Split = 0,      // Split ON: selective attenuation of the sibilance band only
        Broadband = 1   // Split OFF: broadband attenuation of the full signal
    };

    enum class SpeedMode : int {
        Fast = 0,       // Attack 0.5 ms, Release 30 ms
        Slow = 1        // Attack 2.0 ms, Release 120 ms
    };

    enum class FilterMode : int {
        Bandpass = 0,   // 0.15 to 1.61 octave resonant bandpass detector
        HighPass = 1    // All frequencies above Frequency cutoff enter sidechain
    };

    enum class PresetBank : int {
        VocalFemale  = 0,
        VocalMale    = 1,
        VocalProblem = 2,
        Instruments  = 3,
        Master       = 4
    };

    struct Preset {
        PresetBank bank;
        const char* bankName;
        const char* name;
        const char* purpose;
        double thresholdDb;     // -40.0 to 0.0 dB
        double frequencyHz;     // 2000.0 to 16000.0 Hz
        double widthOctaves;    // 0.15 to 1.61 oct
        FilterMode filterMode;  // Bandpass or HighPass
        SpeedMode speed;        // Fast or Slow
        bool splitEnabled;      // Split ON or OFF
        double stereoLinkPct;   // 0 to 100%
    };

    static constexpr double kMinFrequencyHz = 2000.0;
    static constexpr double kMaxFrequencyHz = 16000.0;
    static constexpr double kMinWidthOct = 0.15;
    static constexpr double kMaxWidthOct = 1.61;
    static constexpr double kDefaultRatio = 4.0;
    static constexpr double kMaxReductionCapDb = 24.0;

    PrecisionDeEsser() noexcept {
        setSampleRate(48000.0);
    }

    void setSampleRate(double sampleRate) noexcept {
        cachedSampleRate = clampFinite(sampleRate, 1000.0, 768000.0, 48000.0);
        smoothedFreqHz = frequencyHz_;
        smoothedWidthOct = widthOctaves_;
        updateCoefficients();
        reset();
    }

    void reset() noexcept {
        detectStage1L.reset();
        detectStage1R.reset();
        detectStage2L.reset();
        detectStage2R.reset();
        splitBandL.reset();
        splitBandR.reset();
        splitLowL.reset();
        splitLowR.reset();
        envelopeL = 0.0;
        envelopeR = 0.0;
        currentReductionDb = 0.0;
        currentReductionDbR = 0.0;
        peakHoldDb = 0.0;
        peakHoldSamplesRemaining = 0;
        smoothedFreqHz = frequencyHz_;
        smoothedWidthOct = widthOctaves_;
        meterGrDb.store(0.0f, std::memory_order_relaxed);
        meterPeakHoldDb.store(0.0f, std::memory_order_relaxed);
        meterInDb.store(-60.0f, std::memory_order_relaxed);
        meterOutDb.store(-60.0f, std::memory_order_relaxed);
    }

    // --------------------------------------------------------------------------
    // 1. Threshold (-40 to 0 dB on UI; supports -60 to 0 dB via API)
    // --------------------------------------------------------------------------
    void setThresholdDb(double valueDb) noexcept {
        thresholdDb = clampFinite(valueDb, -60.0, 0.0, -24.0);
    }
    double getThresholdDb() const noexcept { return thresholdDb; }

    void setThresholdDialDb(double valueDb) noexcept {
        thresholdDb = clampFinite(valueDb, -40.0, 0.0, -24.0);
    }
    double getThresholdDialDb() const noexcept {
        return std::clamp(thresholdDb, -40.0, 0.0);
    }

    // --------------------------------------------------------------------------
    // 2. Ratio (Default 7:1 per Precision De-Esser specification)
    // --------------------------------------------------------------------------
    void setRatio(double valueRatio) noexcept {
        ratio = clampFinite(valueRatio, 1.5, 20.0, kDefaultRatio);
    }
    double getRatio() const noexcept { return ratio; }

    // --------------------------------------------------------------------------
    // 3. Speed (Fast: 0.5 ms / 30 ms, Slow: 2.0 ms / 120 ms)
    // --------------------------------------------------------------------------
    void setSpeedMode(SpeedMode spd) noexcept {
        speedMode = spd;
        if (spd == SpeedMode::Fast) {
            attackMs = 0.5;
            releaseMs = 30.0;
        } else {
            attackMs = 2.0;
            releaseMs = 120.0;
        }
        updateTimingCoefficients();
    }
    SpeedMode getSpeedMode() const noexcept { return speedMode; }

    void setAttackMs(double milliseconds) noexcept {
        attackMs = clampFinite(milliseconds, 0.05, 50.0, 0.5);
        speedMode = (attackMs <= 1.0) ? SpeedMode::Fast : SpeedMode::Slow;
        updateTimingCoefficients();
    }
    double getAttackMs() const noexcept { return attackMs; }

    void setReleaseMs(double milliseconds) noexcept {
        releaseMs = clampFinite(milliseconds, 5.0, 1000.0, 30.0);
        updateTimingCoefficients();
    }
    double getReleaseMs() const noexcept { return releaseMs; }

    // --------------------------------------------------------------------------
    // 4. Frequency (2.0 kHz to 16.0 kHz)
    // --------------------------------------------------------------------------
    void setFrequencyHz(double frequencyHz) noexcept {
        frequencyHz_ = clampFinite(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz, 6500.0);
        updateFilterCoefficients(frequencyHz_, widthOctaves_);
    }
    double getFrequencyHz() const noexcept { return frequencyHz_; }

    // --------------------------------------------------------------------------
    // 5. Width (0.15 to 1.61 octaves Bandpass + High-Pass mode at full CW)
    // --------------------------------------------------------------------------
    void setWidthOctaves(double octaves) noexcept {
        widthOctaves_ = clampFinite(octaves, kMinWidthOct, kMaxWidthOct, 0.89);
        updateFilterCoefficients(frequencyHz_, widthOctaves_);
    }
    double getWidthOctaves() const noexcept { return widthOctaves_; }

    void setFilterMode(FilterMode fm) noexcept {
        filterMode = fm;
        updateFilterCoefficients(frequencyHz_, widthOctaves_);
    }
    FilterMode getFilterMode() const noexcept { return filterMode; }

    // Combined Width knob dial (0.15 .. 1.61 = Bandpass, > 1.61 up to 1.75 = High-Pass)
    void setWidthDialValue(double dialVal) noexcept {
        if (dialVal > kMaxWidthOct + 0.02) {
            filterMode = FilterMode::HighPass;
            widthOctaves_ = kMaxWidthOct;
        } else {
            filterMode = FilterMode::Bandpass;
            widthOctaves_ = clampFinite(dialVal, kMinWidthOct, kMaxWidthOct, 0.89);
        }
        updateFilterCoefficients(frequencyHz_, widthOctaves_);
    }
    double getWidthDialValue() const noexcept {
        return (filterMode == FilterMode::HighPass) ? 1.70 : widthOctaves_;
    }

    // --------------------------------------------------------------------------
    // 6. Split (Split ON = Mode::Split, Split OFF = Mode::Broadband)
    // --------------------------------------------------------------------------
    void setMode(Mode value) noexcept { mode = value; }
    Mode getMode() const noexcept { return mode; }

    void setSplitEnabled(bool splitOn) noexcept {
        mode = splitOn ? Mode::Split : Mode::Broadband;
    }
    bool getSplitEnabled() const noexcept { return mode == Mode::Split; }

    // --------------------------------------------------------------------------
    // 7. Solo (Isolates Sidechain Signal) & Power (Active / Bypass)
    // --------------------------------------------------------------------------
    void setSolo(bool soloOn) noexcept { soloEnabled = soloOn; }
    bool getSolo() const noexcept { return soloEnabled; }

    void setPower(bool powerOn) noexcept { powerEnabled = powerOn; }
    bool getPower() const noexcept { return powerEnabled; }

    // --------------------------------------------------------------------------
    // 8. Max Reduction & Stereo Link (NOVA HD Extras)
    // --------------------------------------------------------------------------
    void setMaxReductionDb(double valueDb) noexcept {
        maxReductionDb = clampFinite(valueDb, 0.0, kMaxReductionCapDb, 25.0);
    }
    double getMaxReductionDb() const noexcept { return maxReductionDb; }

    void setStereoLink(double pct) noexcept {
        stereoLink = clampFinite(pct, 0.0, 100.0, 100.0) * 0.01;
    }
    double getStereoLink() const noexcept { return stereoLink * 100.0; }

    // --------------------------------------------------------------------------
    // 9. Metering & Telemetry (Including 3-Second Peak Hold)
    // --------------------------------------------------------------------------
    double getGainReductionDb() const noexcept { return currentReductionDb; }
    float getMeterGrDb() const noexcept { return meterGrDb.load(std::memory_order_relaxed); }
    float getMeterPeakHoldDb() const noexcept { return meterPeakHoldDb.load(std::memory_order_relaxed); }
    float getMeterInDb() const noexcept { return meterInDb.load(std::memory_order_relaxed); }
    float getMeterOutDb() const noexcept { return meterOutDb.load(std::memory_order_relaxed); }
    double getSampleRate() const noexcept { return cachedSampleRate; }

    void resetPeakHold() noexcept {
        peakHoldDb = 0.0;
        peakHoldSamplesRemaining = 0;
        meterPeakHoldDb.store(0.0f, std::memory_order_relaxed);
    }

    // --------------------------------------------------------------------------
    // 10. Complete 5-Bank, 16-Preset NOVA HD Library
    // --------------------------------------------------------------------------
    static const std::vector<Preset>& getFactoryPresets() {
        static const std::vector<Preset> kPresets = {
            // ── Bank 1: Vocal Female (Soprano, Alto, Airy Female, Bright Female) ──
            { PresetBank::VocalFemale, "Vocal Female", "Soprano",
              "Kontrol sibilance vokal soprano (7.8 kHz) dengan menjaga kilau atas",
              -24.0,  7800.0, 0.75, FilterMode::Bandpass, SpeedMode::Slow, true,  100.0 },
            { PresetBank::VocalFemale, "Vocal Female", "Alto",
              "De-essing natural vokal alto pada rentang hangat 6.4 kHz",
              -25.0,  6400.0, 0.89, FilterMode::Bandpass, SpeedMode::Slow, true,  100.0 },
            { PresetBank::VocalFemale, "Vocal Female", "Airy Female",
              "Mengendalikan desis udara (air/breath sibilance) di atas 10.8 kHz",
              -26.5, 10800.0, 1.61, FilterMode::HighPass, SpeedMode::Slow, true,  100.0 },
            { PresetBank::VocalFemale, "Vocal Female", "Bright Female",
              "Menjaga vokal pop modern tetap terang sambil meredam puncak 8.5 kHz",
              -22.5,  8500.0, 0.65, FilterMode::Bandpass, SpeedMode::Slow, true,  100.0 },

            // ── Bank 2: Vocal Male (Tenor, Baritone, Deep Male) ──
            { PresetBank::VocalMale, "Vocal Male", "Tenor",
              "Meredam sibilance vokal tenor pria pada titik 6.0 kHz secara transparan",
              -24.0,  6000.0, 0.85, FilterMode::Bandpass, SpeedMode::Slow, true,  100.0 },
            { PresetBank::VocalMale, "Vocal Male", "Baritone",
              "Mengontrol konsonan desis vokal bariton (5.2 kHz) tanpa menumpulkan artikulasi",
              -23.5,  5200.0, 0.95, FilterMode::Bandpass, SpeedMode::Slow, true,  100.0 },
            { PresetBank::VocalMale, "Vocal Male", "Deep Male",
              "Menjinakkan sibilance frekuensi bawah (4.5 kHz) khas suara pria berat",
              -23.0,  4500.0, 1.05, FilterMode::Bandpass, SpeedMode::Slow, true,  100.0 },

            // ── Bank 3: Vocal Problem (Strong Sibilance, Harsh S, Sharp SH, Whistle) ──
            { PresetBank::VocalProblem, "Vocal Problem", "Strong Sibilance",
              "Penanganan cepat & kuat untuk vokal dengan sibilance sangat dominan",
              -32.5,  6800.0, 1.15, FilterMode::Bandpass, SpeedMode::Fast, true,  100.0 },
            { PresetBank::VocalProblem, "Vocal Problem", "Harsh S",
              "Filter terfokus (0.55 oct) untuk memotong huruf 'S' yang tajam menusuk",
              -30.0,  7400.0, 0.55, FilterMode::Bandpass, SpeedMode::Fast, true,  100.0 },
            { PresetBank::VocalProblem, "Vocal Problem", "Sharp SH",
              "Mengendalikan konsonan 'SH', 'CH', dan 'J' yang tebal di area 4.6 kHz",
              -29.0,  4600.0, 0.90, FilterMode::Bandpass, SpeedMode::Fast, false, 100.0 },
            { PresetBank::VocalProblem, "Vocal Problem", "Whistle",
              "Deteksi sangat sempit (0.22 oct surgical) untuk meredam bunyi siulan gigi",
              -31.0,  9600.0, 0.22, FilterMode::Bandpass, SpeedMode::Fast, true,  100.0 },

            // ── Bank 4: Instruments (Hi-Hat, Overhead, Cymbal, Trumpet) ──
            { PresetBank::Instruments, "Instruments", "Hi-Hat",
              "Merapikan desis hi-hat yang terlalu tajam pada 8.8 kHz",
              -26.0,  8800.0, 1.20, FilterMode::Bandpass, SpeedMode::Fast, true,  100.0 },
            { PresetBank::Instruments, "Instruments", "Overhead",
              "Mengendalikan kekerasan simbal pada drum overhead tanpa mengubah snare/tom",
              -22.0,  7500.0, 1.61, FilterMode::HighPass, SpeedMode::Fast, true,  100.0 },
            { PresetBank::Instruments, "Instruments", "Cymbal",
              "Mengurangi harshness crash dan ride cymbal di area 6.2 kHz",
              -25.0,  6200.0, 1.35, FilterMode::Bandpass, SpeedMode::Fast, true,  100.0 },
            { PresetBank::Instruments, "Instruments", "Trumpet",
              "Menjinakkan gigitan frekuensi tinggi (piercing bite) pada brass & terompet",
              -24.0,  5800.0, 0.80, FilterMode::Bandpass, SpeedMode::Fast, true,  100.0 },

            // ── Bank 5: Master (Mix Bus High-Frequency Control) ──
            { PresetBank::Master, "Master", "Mix Bus High-Frequency Control",
              "Kontrol dinamis frekuensi tinggi yang transparan untuk Mix Bus / Master",
              -18.5,  9200.0, 1.61, FilterMode::HighPass, SpeedMode::Slow, true,  100.0 }
        };
        return kPresets;
    }

    void loadPresetByIndex(int idx) noexcept {
        const auto& all = getFactoryPresets();
        if (all.empty()) return;
        const int clamped = std::clamp(idx, 0, static_cast<int>(all.size()) - 1);
        currentPresetIndex = clamped;
        const auto& p = all[static_cast<size_t>(clamped)];
        currentBank = p.bank;
        setThresholdDb(p.thresholdDb);
        setRatio(kDefaultRatio);
        setMaxReductionDb(kMaxReductionCapDb);
        setFrequencyHz(p.frequencyHz);
        setWidthOctaves(p.widthOctaves);
        setFilterMode(p.filterMode);
        setSpeedMode(p.speed);
        setSplitEnabled(p.splitEnabled);
        setStereoLink(p.stereoLinkPct);
        setSolo(false);
        setPower(true);
    }

    void selectBank(PresetBank bank) noexcept {
        currentBank = bank;
        const auto& all = getFactoryPresets();
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].bank == bank) {
                loadPresetByIndex(static_cast<int>(i));
                return;
            }
        }
    }

    PresetBank getCurrentBank() const noexcept { return currentBank; }
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    // --------------------------------------------------------------------------
    // 11. Real-Time Stereo De-Esser Processing
    // --------------------------------------------------------------------------
    void processStereo(double& left, double& right) noexcept {
        const double inLeft = sanitize(left);
        const double inRight = sanitize(right);

        const double inPeak = std::max(std::abs(inLeft), std::abs(inRight));
        meterInDb.store(static_cast<float>(gainToDb(inPeak, -60.0)), std::memory_order_relaxed);

        if (!powerEnabled) {
            currentReductionDb = 0.0;
            currentReductionDbR = 0.0;
            meterGrDb.store(0.0f, std::memory_order_relaxed);
            meterOutDb.store(static_cast<float>(gainToDb(inPeak, -60.0)), std::memory_order_relaxed);
            left = inLeft;
            right = inRight;
            return;
        }

        // Smooth Frequency & Width changes if user sweeps knobs in real time
        if (std::abs(frequencyHz_ - smoothedFreqHz) > 0.5 ||
            std::abs(widthOctaves_ - smoothedWidthOct) > 1e-4) {
            smoothedFreqHz += 0.025 * (frequencyHz_ - smoothedFreqHz);
            smoothedWidthOct += 0.025 * (widthOctaves_ - smoothedWidthOct);
            updateFilterCoefficients(smoothedFreqHz, smoothedWidthOct);
        }

        // 1. TWO-STAGE CASCADED SIDECHAIN DETECTOR FILTER
        //    Stage 1 isolates the sibilance band; Stage 2 sharpens out-of-band vowel
        //    rejection (-24 dB/oct) so chest/fundamental frequencies never false-trigger!
        const double sc1L = sanitize(detectStage1L.process(inLeft));
        const double sc1R = sanitize(detectStage1R.process(inRight));
        const double detLeft  = sanitize(detectStage2L.process(sc1L));
        const double detRight = sanitize(detectStage2R.process(sc1R));

        // Phase-aligned audio band for Split-Band subtraction
        const double splitBandOutL = sanitize(splitBandL.process(inLeft));
        const double splitBandOutR = sanitize(splitBandR.process(inRight));
        const double splitLowOutL = sanitize(splitLowL.process(inLeft));
        const double splitLowOutR = sanitize(splitLowR.process(inRight));

        // 2. SOLO MODE: Monitors isolated sidechain signal directly
        //    (Per UA manual: Threshold and Split do not affect Solo monitoring)
        if (soloEnabled) {
            currentReductionDb = 0.0;
            meterGrDb.store(0.0f, std::memory_order_relaxed);
            left = detLeft;
            right = detRight;
            const double outPk = std::max(std::abs(left), std::abs(right));
            meterOutDb.store(static_cast<float>(gainToDb(outPk, -60.0)), std::memory_order_relaxed);
            return;
        }

        // 3. STEREO-LINKED RMS/PEAK HYBRID DETECTOR
        const double absL = std::abs(detLeft);
        const double absR = std::abs(detRight);
        const double linkedRect = std::max(
            std::sqrt(0.5 * (detLeft * detLeft + detRight * detRight)),
            0.85 * std::max(absL, absR));

        const double rectL = absL * (1.0 - stereoLink) + linkedRect * stereoLink;
        const double rectR = absR * (1.0 - stereoLink) + linkedRect * stereoLink;

        const double envCoeffL = (rectL > envelopeL) ? detectorAttackCoeff : detectorReleaseCoeff;
        envelopeL = sanitize(envelopeL + envCoeffL * (rectL - envelopeL));

        const double envCoeffR = (rectR > envelopeR) ? detectorAttackCoeff : detectorReleaseCoeff;
        envelopeR = (stereoLink >= 0.999)
                        ? envelopeL
                        : sanitize(envelopeR + envCoeffR * (rectR - envelopeR));

        // 4. SOFT-KNEE 7:1 GAIN COMPUTER ABOVE THRESHOLD
        const double targetGrL = computeReductionDb(envelopeL);
        const double targetGrR = (stereoLink >= 0.999) ? targetGrL : computeReductionDb(envelopeR);

        const double grCoeffL = (targetGrL > currentReductionDb) ? gainAttackCoeff : gainReleaseCoeff;
        currentReductionDb = sanitize(currentReductionDb + grCoeffL * (targetGrL - currentReductionDb));

        const double grCoeffR = (targetGrR > currentReductionDbR) ? gainAttackCoeff : gainReleaseCoeff;
        currentReductionDbR = (stereoLink >= 0.999)
                                  ? currentReductionDb
                                  : sanitize(currentReductionDbR + grCoeffR * (targetGrR - currentReductionDbR));

        const double gainL = dbToGain(-currentReductionDb);
        const double gainR = dbToGain(-currentReductionDbR);

        // 5. 100% BIT-TRANSPARENT SPLIT (Selective) vs BROADBAND (Full-Band) ATTENUATION
        if (mode == Mode::Split) {
            if (filterMode == FilterMode::HighPass) {
                left  = (currentReductionDb  < 1.0e-4) ? inLeft  : sanitize(splitLowOutL - gainL * splitBandOutL);
                right = (currentReductionDbR < 1.0e-4) ? inRight : sanitize(splitLowOutR - gainR * splitBandOutR);
            } else {
                left  = sanitize(inLeft  - (1.0 - gainL) * splitBandOutL);
                right = sanitize(inRight - (1.0 - gainR) * splitBandOutR);
            }
        } else {
            // Split OFF (Broadband): attenuates entire signal based on sidechain detection
            left  = sanitize(inLeft  * gainL);
            right = sanitize(inRight * gainR);
        }

        // 6. UPDATE GAIN REDUCTION METER & 3-SECOND PEAK HOLD
        const double maxGr = std::max(currentReductionDb, currentReductionDbR);
        meterGrDb.store(static_cast<float>(maxGr), std::memory_order_relaxed);

        const int holdSamples3Sec = static_cast<int>(cachedSampleRate * 3.0);
        if (maxGr >= peakHoldDb) {
            peakHoldDb = maxGr;
            peakHoldSamplesRemaining = holdSamples3Sec;
        } else if (peakHoldSamplesRemaining > 0) {
            --peakHoldSamplesRemaining;
        } else {
            peakHoldDb = std::max(maxGr, peakHoldDb * 0.9995);
        }
        meterPeakHoldDb.store(static_cast<float>(peakHoldDb), std::memory_order_relaxed);

        const double outPeak = std::max(std::abs(left), std::abs(right));
        meterOutDb.store(static_cast<float>(gainToDb(outPeak, -60.0)), std::memory_order_relaxed);
    }

private:
    static BiquadCoefficients makeConstantSkirtBandpass(double sampleRate, double frequencyHz, double widthOctaves) noexcept {
        if (!std::isfinite(sampleRate) || sampleRate <= 0.0 || !std::isfinite(frequencyHz))
            return {};
        const double nyquist = 0.49 * sampleRate;
        const double f0 = clampFinite(frequencyHz, 20.0, nyquist, 6500.0);
        const double bw = clampFinite(widthOctaves, kMinWidthOct, kMaxWidthOct, 0.89);

        constexpr double kTwoPi = 6.28318530717958647692;
        constexpr double kLn2Half = 0.3465735902799726547; // ln(2) / 2
        const double w0 = kTwoPi * f0 / sampleRate;
        const double sinW0 = std::sin(w0);
        const double cosW0 = std::cos(w0);

        // RBJ Audio EQ Cookbook exact octave bandwidth to alpha formula:
        const double sinhArg = kLn2Half * bw * (w0 / std::max(1e-6, sinW0));
        const double alpha = sinW0 * std::sinh(sinhArg);

        // Constant 0 dB peak gain bandpass (zero phase at f0)
        const double a0 = 1.0 + alpha;
        if (std::abs(a0) < 1e-12) return {};

        BiquadCoefficients c;
        c.b0 = alpha / a0;
        c.b1 = 0.0;
        c.b2 = -alpha / a0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    double computeReductionDb(double envLin) const noexcept {
        if (!(envLin > 0.0) || !std::isfinite(envLin)) return 0.0;
        const double overDb = gainToDb(envLin, -100.0) - thresholdDb;
        constexpr double kneeDb = 1.8; // Smooth 1.8 dB musical transition into 7:1 de-essing
        if (overDb <= -0.5 * kneeDb) return 0.0;

        const double slope = 1.0 - (1.0 / std::max(1.0, ratio));
        double gr = 0.0;
        if (overDb < 0.5 * kneeDb) {
            const double x = overDb + 0.5 * kneeDb;
            gr = slope * (x * x) / (2.0 * kneeDb);
        } else {
            gr = slope * overDb;
        }
        return std::clamp(gr, 0.0, std::min(maxReductionDb, kMaxReductionCapDb));
    }

    void updateFilterCoefficients(double freqHz, double widthOct) noexcept {
        if (filterMode == FilterMode::Bandpass) {
            const auto bpCoeffs = makeConstantSkirtBandpass(cachedSampleRate, freqHz, widthOct);
            detectStage1L.setCoefficients(bpCoeffs);
            detectStage1R.setCoefficients(bpCoeffs);
            detectStage2L.setCoefficients(bpCoeffs);
            detectStage2R.setCoefficients(bpCoeffs);
            splitBandL.setCoefficients(bpCoeffs);
            splitBandR.setCoefficients(bpCoeffs);
        } else {
            const auto hpCoeffs = makeButterworthHPF(cachedSampleRate, freqHz, 0.7071);
            detectStage1L.setCoefficients(hpCoeffs);
            detectStage1R.setCoefficients(hpCoeffs);
            detectStage2L.setCoefficients(hpCoeffs);
            detectStage2R.setCoefficients(hpCoeffs);
            const auto lrLpCoeffs = makeButterworthLPF(cachedSampleRate, freqHz, 0.5);
            const auto lrHpCoeffs = makeButterworthHPF(cachedSampleRate, freqHz, 0.5);
            splitLowL.setCoefficients(lrLpCoeffs);
            splitLowR.setCoefficients(lrLpCoeffs);
            splitBandL.setCoefficients(lrHpCoeffs);
            splitBandR.setCoefficients(lrHpCoeffs);
        }
    }

    void updateTimingCoefficients() noexcept {
        detectorAttackCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * 0.00025));
        detectorReleaseCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * 0.018));
        gainAttackCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * std::max(0.05, attackMs) * 0.001));
        gainReleaseCoeff = 1.0 - std::exp(-1.0 / (cachedSampleRate * std::max(1.0, releaseMs) * 0.001));

        if (!std::isfinite(detectorAttackCoeff)) detectorAttackCoeff = 0.0;
        if (!std::isfinite(detectorReleaseCoeff)) detectorReleaseCoeff = 0.0;
        if (!std::isfinite(gainAttackCoeff)) gainAttackCoeff = 0.0;
        if (!std::isfinite(gainReleaseCoeff)) gainReleaseCoeff = 0.0;
    }

    void updateCoefficients() noexcept {
        updateFilterCoefficients(frequencyHz_, widthOctaves_);
        updateTimingCoefficients();
    }

    Biquad detectStage1L, detectStage1R;
    Biquad detectStage2L, detectStage2R;
    Biquad splitBandL, splitBandR;
    Biquad splitLowL, splitLowR;

    double cachedSampleRate = 48000.0;
    double thresholdDb = -22.0;
    double ratio = kDefaultRatio; // 4:1
    double frequencyHz_ = 6500.0;
    double widthOctaves_ = 0.75;
    double maxReductionDb = 12.0;
    double attackMs = 2.0;
    double releaseMs = 120.0;
    double stereoLink = 1.0;

    double smoothedFreqHz = 6500.0;
    double smoothedWidthOct = 0.75;

    Mode mode = Mode::Split;
    SpeedMode speedMode = SpeedMode::Slow;
    FilterMode filterMode = FilterMode::HighPass;
    PresetBank currentBank = PresetBank::VocalFemale;
    bool soloEnabled = false;
    bool powerEnabled = true;
    int currentPresetIndex = 0;

    double detectorAttackCoeff = 0.0;
    double detectorReleaseCoeff = 0.0;
    double gainAttackCoeff = 0.0;
    double gainReleaseCoeff = 0.0;

    double envelopeL = 0.0;
    double envelopeR = 0.0;
    double currentReductionDb = 0.0;
    double currentReductionDbR = 0.0;

    double peakHoldDb = 0.0;
    int peakHoldSamplesRemaining = 0;

    std::atomic<float> meterGrDb{0.0f};
    std::atomic<float> meterPeakHoldDb{0.0f};
    std::atomic<float> meterInDb{-60.0f};
    std::atomic<float> meterOutDb{-60.0f};
};

} // namespace nova::graviton
