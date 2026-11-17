#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <limits>

namespace nova::graviton {

// ==============================================================================
// Klark Teknik DN360 30-Band Inductor Graphic Equaliser
// Virtual FX Rack Slot 18 - MIDAS Heritage-D HD96-24 Chapter 14/15
//
// 1/3-octave ISO centre frequencies from 25 Hz to 20 kHz, constant-Q peaking
// sections with a +/-15 dB range per band. Every band is a direct-form-I
// biquad per channel; 0 dB yields an exact identity section.
//
// Extended with Graph-Equalizer concepts inspired by the LSP Graph Equalizer
// (lsp-plugins-graph-equalizer, written independently here):
//   - Channel modes: Stereo (linked), Left/Right (independent) and Mid/Side.
//   - Filter transform: Bilinear (RBJ) or Matched-Z (less cramping near Nyquist).
//   - Band slope (bandwidth) x1..x4, Shift Gain (scales every band), In/Out
//     gain, stereo Balance and Fader Range (+/-6, +/-12, +/-15 dB).
//   - Lock-free telemetry: peak meters, 30-band input/output spectrum analyser
//     (Goertzel bank) and an analytic frequency-response curve.
//
// Coefficient design happens in setters; processStereo() performs bounded
// arithmetic only (no allocation, locks or logging).
// ==============================================================================
class DN360InductorGEQ final {
public:
    static constexpr size_t kNumBands = 30;
    static constexpr double kMinGainDb = -15.0;
    static constexpr double kMaxGainDb = 15.0;
    static constexpr double kBandQ = 1.5;   // default constant-Q inductor sections

    enum class Mode : int { Stereo = 0, LeftRight = 1, MidSide = 2 };
    enum class Transform : int { Bilinear = 0, Matched = 1 };

    static constexpr std::array<double, kNumBands> kBandFrequencies = {
        25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0,
        250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0,
        2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0,
        16000.0, 20000.0
    };

    // Band slope presets (x1..x4) -> Q
    static constexpr std::array<double, 4> kSlopeQ = { 1.0, 1.5, 2.5, 4.0 };

    DN360InductorGEQ() noexcept { setSampleRate(48000.0); }

    void setSampleRate(double sampleRate) noexcept
    {
        cachedSampleRate = clampFinite(sampleRate, 1000.0, 768000.0, 48000.0);
        rebuildAll();
        rebuildAnalyzer();
        for (auto& sub : subEnhancer) sub.prepare(cachedSampleRate);
        reset();
    }

    void reset() noexcept
    {
        for (auto& b : leftBands) b.reset();
        for (auto& b : rightBands) b.reset();
        for (auto& sub : subEnhancer) sub.reset();
        resetAnalyzer();
        for (auto& p : peakIn) p.store(0.0f, std::memory_order_relaxed);
        for (auto& p : peakOut) p.store(0.0f, std::memory_order_relaxed);
    }

    // ------------------------------------------------------------ band gains
    // Legacy API: edits the primary (Stereo / Left / Mid) curve.
    void setBandGain(size_t band, double gainDb) noexcept { setBandGainCh(0, band, gainDb); }
    double getBandGain(size_t band) const noexcept { return getBandGainCh(0, band); }

    // ch 0 = Stereo/Left/Mid, ch 1 = Right/Side (used in LeftRight / MidSide).
    void setBandGainCh(size_t ch, size_t band, double gainDb) noexcept
    {
        if (band >= kNumBands || ch > 1) return;
        const double lim = std::min(rangeDb, kMaxGainDb);
        gains[ch][band] = clampFinite(gainDb, -lim, lim, 0.0);
        updateBand(band);
    }

    double getBandGainCh(size_t ch, size_t band) const noexcept
    {
        return (band < kNumBands && ch <= 1) ? gains[ch][band] : 0.0;
    }

    void flatten() noexcept
    {
        for (size_t c = 0; c < 2; ++c)
            for (size_t i = 0; i < kNumBands; ++i)
                gains[c][i] = 0.0;
        rebuildAll();
    }

    void flattenChannel(size_t ch) noexcept
    {
        if (ch > 1) return;
        for (size_t i = 0; i < kNumBands; ++i)
            gains[ch][i] = 0.0;
        rebuildAll();
    }

    // ------------------------------------------------------------- settings
    void setBypassed(bool shouldBypass) noexcept { bypassed = shouldBypass; }
    bool isBypassed() const noexcept { return bypassed; }
    double getSampleRate() const noexcept { return cachedSampleRate; }

    void setMode(int m) noexcept
    {
        mode = std::clamp(m, 0, 2);
        rebuildAll();
    }
    int getMode() const noexcept { return mode; }

    void setTransform(int t) noexcept
    {
        transform = std::clamp(t, 0, 1);
        rebuildAll();
    }
    int getTransform() const noexcept { return transform; }

    void setSlopeIndex(int s) noexcept
    {
        slopeIndex = std::clamp(s, 0, 3);
        rebuildAll();
    }
    int getSlopeIndex() const noexcept { return slopeIndex; }
    double getBandQ() const noexcept { return kSlopeQ[static_cast<size_t>(slopeIndex)]; }

    // Fader range: 6, 12 or 15 dB.
    void setRangeDb(double r) noexcept
    {
        rangeDb = (r <= 9.0) ? 6.0 : (r <= 13.5 ? 12.0 : 15.0);
        for (size_t c = 0; c < 2; ++c)
            for (size_t i = 0; i < kNumBands; ++i)
                gains[c][i] = std::clamp(gains[c][i], -rangeDb, rangeDb);
        rebuildAll();
    }
    double getRangeDb() const noexcept { return rangeDb; }

    // Scales every band gain (0 = flat .. 1 = as drawn .. 2 = doubled).
    void setShiftGain(double scale) noexcept
    {
        shiftGain = clampFinite(scale, 0.0, 2.0, 1.0);
        rebuildAll();
    }
    double getShiftGain() const noexcept { return shiftGain; }

    void setInGainDb(double db) noexcept { inGainDb = clampFinite(db, -24.0, 24.0, 0.0); inGainLin = std::pow(10.0, inGainDb / 20.0); }
    double getInGainDb() const noexcept { return inGainDb; }
    void setOutGainDb(double db) noexcept { outGainDb = clampFinite(db, -24.0, 24.0, 0.0); outGainLin = std::pow(10.0, outGainDb / 20.0); }
    double getOutGainDb() const noexcept { return outGainDb; }

    // -100 (left) .. +100 (right) percent; applied at the output.
    void setBalancePct(double pct) noexcept { balancePct = clampFinite(pct, -100.0, 100.0, 0.0); }
    double getBalancePct() const noexcept { return balancePct; }

    // SubBassEnhancer stage (cascaded ÷2..÷16 divider + warm saturation + envelope compression)
    void setSubEnhancePct(double pct) noexcept { subEnhancePct = clampFinite(pct, 0.0, 100.0, 0.0); }
    double getSubEnhancePct() const noexcept { return subEnhancePct; }
    void setWarmDrivePct(double pct) noexcept { warmDrivePct = clampFinite(pct, 0.0, 100.0, 0.0); }
    double getWarmDrivePct() const noexcept { return warmDrivePct; }
    void setSubCompPct(double pct) noexcept { subCompPct = clampFinite(pct, 0.0, 100.0, 100.0); }
    double getSubCompPct() const noexcept { return subCompPct; }

    // --------------------------------------------------------- factory presets
    static constexpr int kNumPresets = 10;

    static const char* getPresetName(int idx) noexcept
    {
        static constexpr const char* kNames[kNumPresets] = {
            "01 - Flat (Bypassed Curves)",
            "02 - FOH PA Contour (Warm Punch)",
            "03 - Floor Monitor Anti-Feedback",
            "04 - Vocal Presence & Air",
            "05 - Kick & Sub Tightness",
            "06 - Snare Crack & Body",
            "07 - Acoustic Guitar Sparkle",
            "08 - Drum Bus Inductor Glue",
            "09 - Master Bus M/S Clarity",
            "10 - Broadcast / Speech Clarity"
        };
        if (idx < 0 || idx >= kNumPresets) return kNames[0];
        return kNames[idx];
    }

    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    void applyPreset(int idx) noexcept
    {
        currentPresetIndex = std::clamp(idx, 0, kNumPresets - 1);
        flatten();
        setMode(static_cast<int>(Mode::Stereo));
        setTransform(static_cast<int>(Transform::Bilinear));
        setSlopeIndex(1); // x2 (Q = 1.5)
        setRangeDb(15.0);
        setShiftGain(1.0);
        setInGainDb(0.0);
        setOutGainDb(0.0);
        setBalancePct(0.0);
        setSubEnhancePct(0.0);
        setWarmDrivePct(0.0);
        setSubCompPct(100.0);
        setBypassed(false);

        auto set30 = [this](size_t ch, const std::array<double, kNumBands>& curve) {
            for (size_t i = 0; i < kNumBands; ++i)
                gains[ch][i] = std::clamp(curve[i], -rangeDb, rangeDb);
        };

        switch (currentPresetIndex)
        {
        case 0: // Flat
            break;
        case 1: { // FOH PA Contour (Warm Punch)
            setTransform(static_cast<int>(Transform::Matched));
            setWarmDrivePct(20.0);
            static constexpr std::array<double, kNumBands> c = {
                -6.0, -2.0,  0.5,  1.5,  2.5,  2.0,  0.5,  0.0, -0.5, -1.0,
                -2.0, -2.0, -1.0,  0.0,  0.0,  0.5,  0.5,  1.0,  1.0,  1.0,
                 1.5,  1.5,  1.0,  0.5,  1.0,  1.5,  2.0,  2.0,  1.0,  0.0
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 2: { // Floor Monitor Anti-Feedback (surgical Q=2.5)
            setSlopeIndex(2); // x3 (Q = 2.5)
            static constexpr std::array<double, kNumBands> c = {
               -12.0, -9.0, -6.0, -3.0,  0.0,  0.0,  0.0, -1.0, -1.5, -3.5,
                -1.5,  0.0, -2.5, -1.0,  0.0, -1.5,  0.0, -3.0, -1.0,  0.0,
                -4.0, -1.5, -3.5, -1.0,  0.0, -2.5,  0.0, -1.5,  0.0, -2.0
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 3: { // Vocal Presence & Air
            setTransform(static_cast<int>(Transform::Matched));
            setWarmDrivePct(15.0);
            static constexpr std::array<double, kNumBands> c = {
               -12.0,-10.0, -8.0, -5.0, -2.0,  0.0, -1.0, -2.5, -3.0, -3.0,
                -2.0, -2.0, -1.0,  0.0,  0.0,  0.5,  1.0,  1.5,  1.5,  2.0,
                 2.5,  2.5,  2.0,  2.0,  1.0,  0.0,  1.5,  2.5,  2.0,  1.0
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 4: { // Kick & Sub Tightness
            setSubEnhancePct(35.0);
            setWarmDrivePct(25.0);
            setSubCompPct(80.0);
            static constexpr std::array<double, kNumBands> c = {
                -6.0, -1.5,  2.0,  3.5,  3.0,  1.5,  0.0, -1.5, -2.5, -3.0,
                -4.5, -4.5, -4.0, -2.5, -1.0,  0.0,  0.0,  0.5,  1.0,  1.5,
                 2.5,  3.0,  2.5,  2.5,  1.5,  0.5,  0.0, -1.0, -2.0, -3.0
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 5: { // Snare Crack & Body
            setWarmDrivePct(20.0);
            static constexpr std::array<double, kNumBands> c = {
               -10.0, -8.0, -6.0, -3.0, -1.0,  0.0,  0.5,  1.5,  2.5,  3.0,
                 1.0, -1.5, -3.0, -3.0, -1.5,  0.0,  0.5,  1.0,  1.5,  2.0,
                 2.5,  2.5,  3.0,  2.5,  2.0,  2.0,  1.5,  1.0,  0.0, -1.0
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 6: { // Acoustic Guitar Sparkle
            setTransform(static_cast<int>(Transform::Matched));
            static constexpr std::array<double, kNumBands> c = {
               -12.0,-10.0, -8.0, -6.0, -4.5, -4.0, -2.0, -1.0, -2.5, -1.5,
                 1.5,  1.0,  0.0, -0.5,  0.0,  0.0,  0.5,  1.0,  1.0,  1.5,
                 1.5,  2.0,  2.0,  2.5,  2.5,  3.0,  3.0,  2.5,  2.0,  1.0
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 7: { // Drum Bus Inductor Glue
            setSubEnhancePct(20.0);
            setWarmDrivePct(30.0);
            setSubCompPct(85.0);
            static constexpr std::array<double, kNumBands> c = {
                -3.0,  0.0,  1.0,  2.0,  2.5,  1.5,  0.0,  0.0, -1.0, -1.5,
                -2.0, -2.0, -2.0, -1.0,  0.0,  0.5,  0.5,  1.0,  1.0,  1.0,
                 1.5,  1.5,  1.5,  1.5,  1.5,  2.0,  2.5,  2.0,  1.5,  0.5
            };
            set30(0, c); set30(1, c);
            break;
        }
        case 8: { // Master Bus M/S Clarity
            setMode(static_cast<int>(Mode::MidSide));
            setTransform(static_cast<int>(Transform::Matched));
            setWarmDrivePct(10.0);
            static constexpr std::array<double, kNumBands> midC = {
                -4.0, -2.0,  0.0,  1.0,  1.5,  1.0,  0.0,  0.0, -0.5, -1.0,
                -1.0, -1.5, -1.0,  0.0,  0.0,  0.0,  0.5,  0.5,  1.0,  1.0,
                 1.0,  1.0,  0.5,  0.5,  0.5,  1.0,  1.0,  1.0,  0.5,  0.0
            };
            static constexpr std::array<double, kNumBands> sideC = {
                -8.0, -8.0, -6.0, -5.0, -4.0, -3.5, -3.0, -1.5, -1.0, -0.5,
                 0.0,  0.0,  0.0,  0.0,  0.5,  0.5,  1.0,  1.0,  1.5,  1.5,
                 2.0,  2.0,  2.0,  2.0,  2.5,  2.5,  2.5,  2.5,  2.0,  1.0
            };
            set30(0, midC); set30(1, sideC);
            break;
        }
        case 9: { // Broadcast / Speech Clarity (+/-6 dB range)
            setRangeDb(6.0);
            setSlopeIndex(2); // x3
            static constexpr std::array<double, kNumBands> c = {
                -6.0, -6.0, -6.0, -6.0, -5.0, -3.5, -2.0, -1.5, -2.5, -2.5,
                -2.0, -1.5, -1.0,  0.0,  0.5,  1.0,  1.5,  2.0,  2.0,  2.5,
                 3.0,  3.0,  2.5,  1.5,  0.0, -2.0, -1.0,  0.5,  0.0, -1.5
            };
            set30(0, c); set30(1, c);
            break;
        }
        default:
            break;
        }
        rebuildAll();
    }

    /** Index of the ISO band closest to freqHz (logarithmic distance). */
    static size_t nearestBand(double freqHz) noexcept
    {
        if (!std::isfinite(freqHz) || freqHz <= 0.0)
            return 0;
        const double target = std::log(freqHz);
        size_t best = 0;
        double bestDistance = std::numeric_limits<double>::max();
        for (size_t i = 0; i < kNumBands; ++i)
        {
            const double d = std::abs(std::log(kBandFrequencies[i]) - target);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
            }
        }
        return best;
    }

    // ------------------------------------------------------------ telemetry
    float getPeakInDb(size_t ch) const noexcept { return toDb(peakIn[ch & 1].load(std::memory_order_relaxed)); }
    float getPeakOutDb(size_t ch) const noexcept { return toDb(peakOut[ch & 1].load(std::memory_order_relaxed)); }
    float getInputSpectrumDb(size_t band) const noexcept
    {
        return band < kNumBands ? inSpectrum[band].load(std::memory_order_relaxed) : -120.0f;
    }
    float getOutputSpectrumDb(size_t band) const noexcept
    {
        return band < kNumBands ? outSpectrum[band].load(std::memory_order_relaxed) : -120.0f;
    }

    /** Analytic magnitude response (dB) of the full EQ for curve ch (0/1) at freqHz. */
    double getMagnitudeDb(size_t ch, double freqHz) const noexcept
    {
        if (ch > 1) ch = 0;
        const double w = 2.0 * 3.14159265358979323846 * std::clamp(freqHz, 1.0, cachedSampleRate * 0.499)
                         / cachedSampleRate;
        const std::complex<double> z1 = std::polar(1.0, -w);
        const std::complex<double> z2 = std::polar(1.0, -2.0 * w);
        double totalDb = 0.0;
        for (size_t i = 0; i < kNumBands; ++i)
        {
            const auto& c = coeffs[ch][i];
            const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
            const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
            const double m = std::abs(num) / std::max(1.0e-12, std::abs(den));
            totalDb += 20.0 * std::log10(std::max(1.0e-9, m));
        }
        return totalDb;
    }

    // ------------------------------------------------------------ processing
    void processStereo(double& left, double& right) noexcept
    {
        if (bypassed) return;

        double l = sanitize(left) * inGainLin;
        double r = sanitize(right) * inGainLin;

        trackPeak(peakIn, l, r);
        analyze(inGoertzel, 0.5 * (l + r), inCount, true);

        if (mode == static_cast<int>(Mode::MidSide))
        {
            const double m = 0.5 * (l + r);
            const double s = 0.5 * (l - r);
            double mm = m, ss = s;
            for (size_t i = 0; i < kNumBands; ++i)
            {
                mm = sanitize(leftBands[i].process(mm));
                ss = sanitize(rightBands[i].process(ss));
            }
            l = mm + ss;
            r = mm - ss;
        }
        else
        {
            for (size_t i = 0; i < kNumBands; ++i)
            {
                l = sanitize(leftBands[i].process(l));
                r = sanitize(rightBands[i].process(r));
            }
        }

        l *= outGainLin;
        r *= outGainLin;

        // SubBassEnhancer (÷2..÷16 divider + warm saturation + envelope compression) on core path
        if (subEnhancePct > 0.0 || warmDrivePct > 0.0)
        {
            const double subAmt = subEnhancePct * 0.01;
            const double drvAmt = warmDrivePct * 0.01;
            const double cmpAmt = subCompPct * 0.01;
            if (subAmt > 0.0)
            {
                l += subEnhancer[0].process(l, cmpAmt) * subAmt;
                r += subEnhancer[1].process(r, cmpAmt) * subAmt;
            }
            if (drvAmt > 0.0)
            {
                const double wl = SubBassEnhancerChannel::warmSaturate(l * (1.0 + drvAmt * 1.5));
                const double wr = SubBassEnhancerChannel::warmSaturate(r * (1.0 + drvAmt * 1.5));
                l = l * (1.0 - drvAmt) + wl * drvAmt;
                r = r * (1.0 - drvAmt) + wr * drvAmt;
            }
        }

        if (balancePct != 0.0)
        {
            const double b = balancePct * 0.01;
            if (b > 0.0) l *= (1.0 - b); else r *= (1.0 + b);
        }

        l = sanitize(l);
        r = sanitize(r);

        trackPeak(peakOut, l, r);
        analyze(outGoertzel, 0.5 * (l + r), outCount, false);

        left = l;
        right = r;
    }

private:
    static float toDb(float lin) noexcept
    {
        return lin <= 1.0e-6f ? -120.0f : 20.0f * std::log10(lin);
    }

    static void trackPeak(std::array<std::atomic<float>, 2>& p, double l, double r) noexcept
    {
        const float al = static_cast<float>(std::abs(l));
        const float ar = static_cast<float>(std::abs(r));
        const float pl = p[0].load(std::memory_order_relaxed);
        const float pr = p[1].load(std::memory_order_relaxed);
        p[0].store(al > pl ? al : pl * 0.9995f, std::memory_order_relaxed);
        p[1].store(ar > pr ? ar : pr * 0.9995f, std::memory_order_relaxed);
    }

    // ---- Matched-Z peaking section -------------------------------------
    BiquadCoefficients designMatchedPeaking(double freq, double gainDb, double q) const noexcept
    {
        BiquadCoefficients id{};
        if (std::abs(gainDb) < 1.0e-9) return id;
        using cd = std::complex<double>;
        const double sr = cachedSampleRate;
        const double f0 = std::clamp(freq, 1.0, sr * 0.499);
        const double w0 = 2.0 * 3.14159265358979323846 * f0;
        const double A = std::pow(10.0, gainDb / 40.0);
        const double T = 1.0 / sr;

        // Analog prototype H(s) = (s^2 + s*w0*A/Q + w0^2) / (s^2 + s*w0/(A*Q) + w0^2)
        auto roots = [&](double k) {
            const double re = -0.5 * k;
            const double disc = re * re - 1.0;
            cd r1, r2;
            if (disc < 0.0)
            {
                const double im = std::sqrt(-disc);
                r1 = cd(re, im) * w0;
                r2 = cd(re, -im) * w0;
            }
            else
            {
                const double sq = std::sqrt(disc);
                r1 = cd(re + sq, 0.0) * w0;
                r2 = cd(re - sq, 0.0) * w0;
            }
            return std::array<cd, 2>{ r1, r2 };
        };
        const auto nz = roots(A / q);
        const auto dz = roots(1.0 / (A * q));

        auto toPoly = [&](const std::array<cd, 2>& s, double& c1, double& c2) {
            const cd z1 = std::exp(s[0] * T);
            const cd z2 = std::exp(s[1] * T);
            c1 = -(z1 + z2).real();
            c2 = (z1 * z2).real();
        };
        double nb1, nb2, da1, da2;
        toPoly(nz, nb1, nb2);
        toPoly(dz, da1, da2);

        // Normalise so the digital peak gain matches the analog gain at f0.
        const double w = 2.0 * 3.14159265358979323846 * f0 / sr;
        const cd e1 = std::polar(1.0, -w), e2 = std::polar(1.0, -2.0 * w);
        const double digital = std::abs((1.0 + nb1 * e1 + nb2 * e2) / (1.0 + da1 * e1 + da2 * e2));
        const double target = A * A;
        const double g = (digital > 1.0e-12) ? target / digital : 1.0;

        BiquadCoefficients c;
        c.b0 = g;
        c.b1 = g * nb1;
        c.b2 = g * nb2;
        c.a1 = da1;
        c.a2 = da2;
        if (!std::isfinite(c.b0) || !std::isfinite(c.b1) || !std::isfinite(c.b2)
            || !std::isfinite(c.a1) || !std::isfinite(c.a2))
            return id;
        return c;
    }

    double effectiveGain(size_t ch, size_t band) const noexcept
    {
        return std::clamp(gains[ch][band] * shiftGain, -kMaxGainDb * 2.0, kMaxGainDb * 2.0);
    }

    void updateBand(size_t band) noexcept
    {
        const double q = getBandQ();
        for (size_t ch = 0; ch < 2; ++ch)
        {
            // Stereo mode links both channels to curve 0.
            const size_t src = (mode == static_cast<int>(Mode::Stereo)) ? 0 : ch;
            const double g = effectiveGain(src, band);
            BiquadCoefficients c;
            if (transform == static_cast<int>(Transform::Matched))
                c = designMatchedPeaking(kBandFrequencies[band], g, q);
            else
                c = makePeakingEq(static_cast<Sample>(cachedSampleRate),
                                  static_cast<Sample>(kBandFrequencies[band]),
                                  static_cast<Sample>(g), static_cast<Sample>(q));
            coeffs[ch][band] = c;
            (ch == 0 ? leftBands : rightBands)[band].setCoefficients(c);
        }
    }

    void rebuildAll() noexcept
    {
        for (size_t i = 0; i < kNumBands; ++i) updateBand(i);
    }

    // ---- 30-band Goertzel spectrum analyser -----------------------------
    struct GoertzelBank
    {
        std::array<double, kNumBands> s1{}, s2{}, coef{};
    };

    void rebuildAnalyzer() noexcept
    {
        blockLength = std::max<long long>(2048, static_cast<long long>(cachedSampleRate * 0.17));
        for (size_t i = 0; i < kNumBands; ++i)
        {
            const double f = std::min(kBandFrequencies[i], cachedSampleRate * 0.45);
            const double c = 2.0 * std::cos(2.0 * 3.14159265358979323846 * f / cachedSampleRate);
            inGoertzel.coef[i] = c;
            outGoertzel.coef[i] = c;
        }
    }

    void resetAnalyzer() noexcept
    {
        inGoertzel.s1.fill(0.0); inGoertzel.s2.fill(0.0);
        outGoertzel.s1.fill(0.0); outGoertzel.s2.fill(0.0);
        inCount = outCount = 0;
        for (auto& v : inSpectrum) v.store(-120.0f, std::memory_order_relaxed);
        for (auto& v : outSpectrum) v.store(-120.0f, std::memory_order_relaxed);
    }

    void analyze(GoertzelBank& bank, double x, long long& count, bool isInput) noexcept
    {
        for (size_t i = 0; i < kNumBands; ++i)
        {
            const double s0 = x + bank.coef[i] * bank.s1[i] - bank.s2[i];
            bank.s2[i] = bank.s1[i];
            bank.s1[i] = s0;
        }
        if (++count < blockLength) return;

        auto& dst = isInput ? inSpectrum : outSpectrum;
        const double n = static_cast<double>(count);
        for (size_t i = 0; i < kNumBands; ++i)
        {
            const double p = bank.s1[i] * bank.s1[i] + bank.s2[i] * bank.s2[i]
                             - bank.coef[i] * bank.s1[i] * bank.s2[i];
            const double mag = std::sqrt(std::max(0.0, p)) * 2.0 / n;
            const float db = static_cast<float>(
                std::clamp(20.0 * std::log10(std::max(1.0e-7, mag)), -120.0, 6.0));
            const float prev = dst[i].load(std::memory_order_relaxed);
            dst[i].store(db > prev ? db : prev * 0.6f + db * 0.4f, std::memory_order_relaxed);
            bank.s1[i] = 0.0;
            bank.s2[i] = 0.0;
        }
        count = 0;
    }

    // ---- SubBassEnhancer (÷2..÷16 divider + warm saturation + compression) ----
    struct SubBassEnhancerChannel
    {
        Biquad hpf30, lpf300, smooth50;
        double sr = 48000.0;
        double env = 0.0;
        double attackC = 0.0, releaseC = 0.0;
        double prevFiltered = 0.0;
        double estimatedFreq = 80.0;
        long long samplesSinceCross = 0;
        double compGain = 1.0;
        std::array<int, 4> divCount{};
        std::array<bool, 4> divState{};

        void prepare(double sampleRate) noexcept
        {
            sr = std::max(8000.0, sampleRate);
            hpf30.setCoefficients(makeButterworthHPF(sr, 30.0));
            lpf300.setCoefficients(makeButterworthLPF(sr, 300.0));
            smooth50.setCoefficients(makeButterworthLPF(sr, 50.0));
            attackC = std::exp(-1.0 / (sr * 0.008));
            releaseC = std::exp(-1.0 / (sr * 0.030));
            reset();
        }

        void reset() noexcept
        {
            hpf30.reset();
            lpf300.reset();
            smooth50.reset();
            env = 0.0;
            prevFiltered = 0.0;
            estimatedFreq = 80.0;
            samplesSinceCross = 0;
            compGain = 1.0;
            divCount.fill(0);
            divState.fill(false);
        }

        static double warmSaturate(double x) noexcept
        {
            return (x >= 0.0) ? (std::tanh(x * 1.5) * 0.9) : std::tanh(x * 1.2);
        }

        double process(double input, double compMix) noexcept
        {
            const double filtered = sanitize(lpf300.process(hpf30.process(input)));
            const double absIn = std::abs(filtered);
            const double coeff = (absIn > env) ? attackC : releaseC;
            env = coeff * env + (1.0 - coeff) * absIn;
            if (!(env > 0.0) || !std::isfinite(env)) env = 0.0;

            if (samplesSinceCross < (1 << 24)) ++samplesSinceCross;

            if (env < 0.001)
            {
                prevFiltered = filtered;
                return sanitize(smooth50.process(0.0));
            }

            const bool zc = (prevFiltered <= 0.0 && filtered > 0.0);
            prevFiltered = filtered;

            if (zc)
            {
                if (samplesSinceCross > 1)
                {
                    const double rawFreq = sr / static_cast<double>(samplesSinceCross);
                    if (std::isfinite(rawFreq) && rawFreq >= 20.0 && rawFreq <= 600.0)
                        estimatedFreq = 0.85 * estimatedFreq + 0.15 * rawFreq;
                }
                samplesSinceCross = 0;

                bool trig = true;
                for (int s = 0; s < 4 && trig; ++s)
                {
                    if (++divCount[static_cast<size_t>(s)] >= 2)
                    {
                        divState[static_cast<size_t>(s)] = !divState[static_cast<size_t>(s)];
                        divCount[static_cast<size_t>(s)] = 0;
                    }
                    else
                    {
                        trig = false;
                    }
                }
            }

            int octaves = 0;
            double f = estimatedFreq;
            while (f > 50.0 && octaves < 4)
            {
                f *= 0.5;
                ++octaves;
            }
            const int stageIdx = std::clamp(std::max(1, octaves) - 1, 0, 3);
            const double rawSub = divState[static_cast<size_t>(stageIdx)] ? 1.0 : -1.0;
            const double smoothedSub = sanitize(smooth50.process(rawSub));
            const double dynamicSub = smoothedSub * env * 3.5;
            const double saturatedSub = warmSaturate(dynamicSub);

            // Envelope compression (threshold 0.1, ratio 3:1)
            const double subEnv = std::abs(saturatedSub);
            double targetGain = 1.0;
            if (subEnv > 0.1)
            {
                const double overDb = 20.0 * std::log10(subEnv / 0.1);
                const double compDb = overDb / 3.0;
                const double targetLvl = 0.1 * std::pow(10.0, compDb / 20.0);
                targetGain = targetLvl / subEnv;
            }
            compGain = 0.995 * compGain + 0.005 * targetGain;
            const double effGain = 1.0 - compMix + compMix * compGain;
            return sanitize(saturatedSub * effGain);
        }
    };
    std::array<SubBassEnhancerChannel, 2> subEnhancer{};

    std::array<Biquad, kNumBands> leftBands;
    std::array<Biquad, kNumBands> rightBands;
    std::array<std::array<BiquadCoefficients, kNumBands>, 2> coeffs{};
    std::array<std::array<double, kNumBands>, 2> gains{};
    double cachedSampleRate = 48000.0;
    bool bypassed = false;
    int mode = 0;
    int transform = 0;
    int slopeIndex = 1; // x2 -> Q 1.5 (original DN360 default)
    double rangeDb = 15.0;
    double shiftGain = 1.0;
    double inGainDb = 0.0, inGainLin = 1.0;
    double outGainDb = 0.0, outGainLin = 1.0;
    double balancePct = 0.0;
    double subEnhancePct = 0.0;
    double warmDrivePct = 0.0;
    double subCompPct = 100.0;
    int currentPresetIndex = 0;

    GoertzelBank inGoertzel, outGoertzel;
    long long blockLength = 8192;
    long long inCount = 0, outCount = 0;

    std::array<std::atomic<float>, 2> peakIn{}, peakOut{};
    std::array<std::atomic<float>, kNumBands> inSpectrum{}, outSpectrum{};
};

} // namespace nova::graviton
