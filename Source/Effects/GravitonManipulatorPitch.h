#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>
#include <numbers>

namespace nova::graviton {

// ==============================================================================
// Polyverse Manipulator Pitch Shifter & Voice Doubler (ADT) Engine
// Reference: Infected Mushroom & Polyverse Manipulator Vocal Processor
// ==============================================================================
class ManipulatorPitchEngine final {
public:
    static constexpr int kMaxBufferSamples = 65536; // ~1.36s @ 48 kHz
    static constexpr size_t kSeqSteps = 8;

    ManipulatorPitchEngine() noexcept {
        bufferL.resize(kMaxBufferSamples, 0.0);
        bufferR.resize(kMaxBufferSamples, 0.0);
        reset();
    }

    void reset() noexcept {
        std::fill(bufferL.begin(), bufferL.end(), 0.0);
        std::fill(bufferR.begin(), bufferR.end(), 0.0);
        writePos = 0;
        headPos1 = 0.0;
        headPos2 = 0.0;
        headPosOct = 0.0;
        lastShiftedL = 0.0;
        lastShiftedR = 0.0;
        fmPhase = 0.0;
        seqPhase = 0.0;
        currentPitchGlided = pitchSemi;

        meterInL.store(0.0f, std::memory_order_relaxed);
        meterInR.store(0.0f, std::memory_order_relaxed);
        meterOutL.store(0.0f, std::memory_order_relaxed);
        meterOutR.store(0.0f, std::memory_order_relaxed);

        formantFilterL.reset();
        formantFilterR.reset();
        updateFormantFilters();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
            sampleRate = sr;
            updateFormantFilters();
        }
    }

    // ── Main Pitch & Harmonics Controls ────────────────────────────────────
    void setPitch(double semi) noexcept { pitchSemi = std::clamp(semi, -24.0, 24.0); }
    double getPitch() const noexcept { return pitchSemi; }

    void setHarmonics(double val) noexcept { harmonicsVal = std::clamp(val, -2.0, 2.0); }
    double getHarmonics() const noexcept { return harmonicsVal; }

    void setAlternator(double val) noexcept { alternatorVal = std::clamp(val, 0.0, 12.0); }
    double getAlternator() const noexcept { return alternatorVal; }

    void setDryWet(double pct) noexcept { dryWetPct = std::clamp(pct, 0.0, 100.0); }
    double getDryWet() const noexcept { return dryWetPct; }

    void setPolyMode(bool poly) noexcept { polyMode = poly; }
    bool getPolyMode() const noexcept { return polyMode; }

    void setLowDetect(bool low) noexcept { lowDetect = low; }
    bool getLowDetect() const noexcept { return lowDetect; }

    // ── Formant, Ratio, FM, and Octave Controls ────────────────────────────
    void setFormant(double semi) noexcept {
        formantSemi = std::clamp(semi, -12.0, 12.0);
        updateFormantFilters();
    }
    double getFormant() const noexcept { return formantSemi; }

    void setRatio(double val) noexcept { ratioVal = std::clamp(val, 0.0, 2.0); }
    double getRatio() const noexcept { return ratioVal; }

    void setSnap(bool sn) noexcept { snapPitch = sn; }
    bool getSnap() const noexcept { return snapPitch; }

    void setFm(double val) noexcept { fmAmount = std::clamp(val, 0.0, 24.0); }
    double getFm() const noexcept { return fmAmount; }

    void setOctave(double val) noexcept { octaveVal = std::clamp(val, -3.0, 3.0); }
    double getOctave() const noexcept { return octaveVal; }

    // ── Glide, Smear, Stereo & Detune (ADT Section) ────────────────────────
    void setGlide(double ms) noexcept { glideMs = std::clamp(ms, 0.0, 1000.0); }
    double getGlide() const noexcept { return glideMs; }

    void setSmear(double pct) noexcept { smearPct = std::clamp(pct, 0.0, 100.0); }
    double getSmear() const noexcept { return smearPct; }

    void setStereo(double pct) noexcept { stereoPct = std::clamp(pct, 0.0, 100.0); }
    double getStereo() const noexcept { return stereoPct; }

    void setDetune(double cents) noexcept { detuneCents = std::clamp(cents, 0.0, 50.0); }
    double getDetune() const noexcept { return detuneCents; }

    // ── Modulation Architecture Controls ───────────────────────────────────
    void setMetaKnob(double pct) noexcept { metaKnobPct = std::clamp(pct, 0.0, 100.0); }
    double getMetaKnob() const noexcept { return metaKnobPct; }

    void setSeqRate(double hz) noexcept { seqRateHz = std::clamp(hz, 0.1, 20.0); }
    double getSeqRate() const noexcept { return seqRateHz; }

    void setSeqShape(double val) noexcept { seqShape = std::clamp(val, 0.0, 5.0); }
    double getSeqShape() const noexcept { return seqShape; }

    void randomizeSeq() noexcept {
        for (auto& step : seqPattern) {
            step = (static_cast<double>(std::rand()) / RAND_MAX) * 2.0 - 1.0;
        }
    }

    void setAdsr(double a, double d, double s, double r) noexcept {
        adsrA = std::clamp(a, 0.0, 100.0);
        adsrD = std::clamp(d, 0.0, 100.0);
        adsrS = std::clamp(s, 0.0, 100.0);
        adsrR = std::clamp(r, 0.0, 100.0);
    }

    // ── Real-Time Meter Readouts ───────────────────────────────────────────
    float getMeterInL() const noexcept { return meterInL.load(std::memory_order_relaxed); }
    float getMeterInR() const noexcept { return meterInR.load(std::memory_order_relaxed); }
    float getMeterOutL() const noexcept { return meterOutL.load(std::memory_order_relaxed); }
    float getMeterOutR() const noexcept { return meterOutR.load(std::memory_order_relaxed); }

    // ── Audio Processing ───────────────────────────────────────────────────
    void processStereo(double& inOutL, double& inOutR) noexcept {
        const double dryInL = inOutL;
        const double dryInR = inOutR;

        // 1. Step Sequencer & Modulation Clock
        seqPhase += (seqRateHz * static_cast<double>(kSeqSteps)) / sampleRate;
        if (seqPhase >= static_cast<double>(kSeqSteps)) seqPhase -= static_cast<double>(kSeqSteps);
        const size_t currentStep = static_cast<size_t>(seqPhase) % kSeqSteps;
        const double seqMod = seqPattern[currentStep] * (seqShape * 0.2);

        // 2. Meta Knob Macro Modulation Amount
        const double macroNorm = metaKnobPct * 0.01;
        const double macroPitchOffset = macroNorm * 2.0;
        const double macroFmOffset = macroNorm * 4.0;

        // 3. Glide Interpolation for Pitch
        double targetPitch = pitchSemi + macroPitchOffset + seqMod;
        if (snapPitch) {
            targetPitch = std::round(targetPitch);
        }

        const double glideCoeff = (glideMs > 1.0)
            ? (1.0 - std::exp(-1000.0 / (glideMs * sampleRate)))
            : 1.0;
        currentPitchGlided += (targetPitch - currentPitchGlided) * glideCoeff;

        // Effective pitch ratios
        const double baseRatio = std::pow(2.0, currentPitchGlided / 12.0);
        const double detuneRatioL = std::pow(2.0, (detuneCents * 0.5) / 1200.0);
        const double detuneRatioR = std::pow(2.0, (-detuneCents * 0.5) / 1200.0);

        const double effRatioL = baseRatio * detuneRatioL;
        const double effRatioR = baseRatio * detuneRatioR;

        // 4. Frequency Modulation (FM Engine)
        double fmOffset = 0.0;
        const double totalFm = fmAmount + macroFmOffset;
        if (totalFm > 0.05) {
            constexpr double twoPi = 2.0 * std::numbers::pi;
            fmPhase += (twoPi * (140.0 * (1.0 + harmonicsVal))) / sampleRate;
            if (fmPhase >= twoPi) fmPhase -= twoPi;
            fmOffset = std::sin(fmPhase) * (totalFm * 0.001 * sampleRate);
        }

        // 5. Circular Buffer Recording
        bufferL[static_cast<size_t>(writePos)] = sanitize(dryInL);
        bufferR[static_cast<size_t>(writePos)] = sanitize(dryInR);

        // Window size for pitch granular crossfade
        const double windowSize = 2048.0 + (smearPct * 20.48); // ~42 to ~84 ms
        const double halfWin = windowSize * 0.5;

        // Triangular Hanning-like crossfade envelopes
        const double env1 = (headPos1 < halfWin) ? (headPos1 / halfWin) : ((windowSize - headPos1) / halfWin);
        const double env2 = (headPos2 < halfWin) ? (headPos2 / halfWin) : ((windowSize - headPos2) / halfWin);

        auto readInterp = [](const std::vector<double>& buf, int wPos, double headOffset) noexcept -> double {
            const int offset = static_cast<int>(headOffset);
            const double frac = headOffset - static_cast<double>(offset);
            const int sz = static_cast<int>(buf.size());
            const int p1 = (wPos - offset + sz) % sz;
            const int p2 = (p1 - 1 + sz) % sz;
            return buf[static_cast<size_t>(p1)] * (1.0 - frac) + buf[static_cast<size_t>(p2)] * frac;
        };

        double shiftedL1 = readInterp(bufferL, writePos, headPos1 + fmOffset) * env1;
        double shiftedL2 = readInterp(bufferL, writePos, headPos2 + fmOffset) * env2;
        double shiftedR1 = readInterp(bufferR, writePos, headPos1 - fmOffset) * env1;
        double shiftedR2 = readInterp(bufferR, writePos, headPos2 - fmOffset) * env2;

        double shiftedL = shiftedL1 + shiftedL2;
        double shiftedR = shiftedR1 + shiftedR2;

        // 6. Octave Layer Processing
        if (std::abs(octaveVal) > 0.05) {
            const double octRatio = std::pow(2.0, octaveVal);
            const double envOct = (headPosOct < halfWin) ? (headPosOct / halfWin) : ((windowSize - headPosOct) / halfWin);
            const double octShiftL = readInterp(bufferL, writePos, headPosOct) * envOct;
            const double octShiftR = readInterp(bufferR, writePos, headPosOct) * envOct;
            const double octGain = std::abs(octaveVal) * 0.45;
            shiftedL += octShiftL * octGain;
            shiftedR += octShiftR * octGain;

            headPosOct += (1.0 - octRatio);
            if (headPosOct >= windowSize) headPosOct -= windowSize;
            if (headPosOct < 0.0) headPosOct += windowSize;
        }

        // 7. Alternator Harmonic Modulation (Voice Doubling & Width)
        if (alternatorVal > 0.1) {
            const double altRatio = std::pow(2.0, (alternatorVal * 0.5) / 12.0);
            const double altL = readInterp(bufferL, writePos, (headPos1 * altRatio)) * env1 * 0.4;
            const double altR = readInterp(bufferR, writePos, (headPos2 * altRatio)) * env2 * 0.4;
            shiftedL += altR;
            shiftedR += altL;
        }

        // Advance rotating heads
        headPos1 += (1.0 - (effRatioL + effRatioR) * 0.5);
        if (headPos1 >= windowSize) headPos1 -= windowSize;
        if (headPos1 < 0.0) headPos1 += windowSize;

        headPos2 = headPos1 + halfWin;
        if (headPos2 >= windowSize) headPos2 -= windowSize;

        writePos = (writePos + 1) % kMaxBufferSamples;

        // 8. Formant Filtering (Resonant Peak Shift)
        shiftedL = formantFilterL.process(shiftedL);
        shiftedR = formantFilterR.process(shiftedR);

        // 9. Stereo Widening & Haas ADT Delay
        const double stereoNorm = stereoPct * 0.01;
        if (stereoNorm > 0.01) {
            const int adtSamples = static_cast<int>(0.012 * sampleRate * stereoNorm); // up to 12ms
            const double adtTapR = readInterp(bufferR, writePos, headPos1 + static_cast<double>(adtSamples)) * env1;
            const double mid = (shiftedL + shiftedR) * 0.5;
            const double side = (shiftedR - shiftedL) * 0.5 * (1.0 + stereoNorm * 1.5);
            shiftedL = mid - side;
            shiftedR = (mid + side) * 0.7 + adtTapR * 0.3;
        }

        // 10. Dry/Wet Mix Staging
        const double wetGain = dryWetPct * 0.01;
        const double dryGain = 1.0 - (wetGain * 0.5);

        inOutL = sanitize(dryInL * dryGain + shiftedL * wetGain);
        inOutR = sanitize(dryInR * dryGain + shiftedR * wetGain);

        // 11. Live Peak Metering
        const float inLVal = static_cast<float>(std::abs(dryInL));
        const float inRVal = static_cast<float>(std::abs(dryInR));
        const float outLVal = static_cast<float>(std::abs(inOutL));
        const float outRVal = static_cast<float>(std::abs(inOutR));

        float pInL = meterInL.load(std::memory_order_relaxed);
        meterInL.store(std::max(inLVal, pInL * 0.999f), std::memory_order_relaxed);
        float pInR = meterInR.load(std::memory_order_relaxed);
        meterInR.store(std::max(inRVal, pInR * 0.999f), std::memory_order_relaxed);
        float pOutL = meterOutL.load(std::memory_order_relaxed);
        meterOutL.store(std::max(outLVal, pOutL * 0.999f), std::memory_order_relaxed);
        float pOutR = meterOutR.load(std::memory_order_relaxed);
        meterOutR.store(std::max(outRVal, pOutR * 0.999f), std::memory_order_relaxed);
    }

private:
    struct FormantFilter {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }

        double process(double in) noexcept {
            const double out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = in;
            y2 = y1; y1 = sanitize(out);
            return y1;
        }

        void setBandpass(double freqHz, double q, double sr) noexcept {
            const double w0 = 2.0 * std::numbers::pi * std::clamp(freqHz, 50.0, sr * 0.45) / sr;
            const double alpha = std::sin(w0) / (2.0 * q);
            const double cosw0 = std::cos(w0);
            const double a0 = 1.0 + alpha;
            b0 = (alpha * 1.8) / a0;
            b1 = 0.0;
            b2 = (-alpha * 1.8) / a0;
            a1 = (-2.0 * cosw0) / a0;
            a2 = (1.0 - alpha) / a0;
        }
    };

    void updateFormantFilters() noexcept {
        const double formantFactor = std::pow(2.0, formantSemi / 12.0);
        const double centerFreq = std::clamp(1200.0 * formantFactor, 250.0, 6500.0);
        formantFilterL.setBandpass(centerFreq, 2.5, sampleRate);
        formantFilterR.setBandpass(centerFreq, 2.5, sampleRate);
    }

    double sampleRate = 48000.0;

    // Pitch & Harmonics
    double pitchSemi = 0.0;
    double harmonicsVal = -1.0;
    double alternatorVal = 6.0;
    double dryWetPct = 100.0;
    bool polyMode = true;
    bool lowDetect = true;

    // Formant, Ratio, FM, Octave
    double formantSemi = 7.0;
    double ratioVal = 0.500;
    bool snapPitch = false;
    double fmAmount = 13.0;
    double octaveVal = -1.0;

    // Glide, Smear, Stereo, Detune
    double glideMs = 35.0;
    double smearPct = 20.0;
    double stereoPct = 50.0;
    double detuneCents = 15.0;

    // Modulation
    double metaKnobPct = 100.0;
    double seqRateHz = 1.0;
    double seqShape = 2.0;
    std::array<double, kSeqSteps> seqPattern = { 0.0, 0.4, 0.8, 0.2, -0.5, 0.3, 0.7, -0.2 };
    double adsrA = 86.0;
    double adsrD = 20.0;
    double adsrS = 50.0;
    double adsrR = 64.0;

    // Internal State
    double currentPitchGlided = 0.0;
    std::vector<double> bufferL;
    std::vector<double> bufferR;
    int writePos = 0;
    double headPos1 = 0.0;
    double headPos2 = 0.0;
    double headPosOct = 0.0;
    double lastShiftedL = 0.0;
    double lastShiftedR = 0.0;
    double fmPhase = 0.0;
    double seqPhase = 0.0;

    FormantFilter formantFilterL;
    FormantFilter formantFilterR;

    // Real-Time Atomic Meters
    std::atomic<float> meterInL{ 0.0f };
    std::atomic<float> meterInR{ 0.0f };
    std::atomic<float> meterOutL{ 0.0f };
    std::atomic<float> meterOutR{ 0.0f };
};

} // namespace nova::graviton
