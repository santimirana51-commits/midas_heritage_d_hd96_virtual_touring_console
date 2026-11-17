#pragma once

#include "DSP/GravitonDSP.h"
#include "DSP/GravitonDynamics.h"
#include "Effects/GravitonMasterX.h"
#include "Effects/GravitonPlateReverb.h"
#include "Effects/GravitonDVR2Reverb.h"
#include "Effects/GravitonSCFChorus.h"
#include "Effects/GravitonNonLin2.h"
#include "Audio/GravitonASIOBridge.h"
#include "Effects/GravitonSubHarmonic.h"
#include "DSP/GravitonDN360.h"
#include "DSP/GravitonDeEsser.h"
#include "Effects/GravitonConvolutionReverb.h"
#include "Effects/GravitonHybridReverb.h"
#include "Effects/GravitonTC2290.h"
#include "Effects/GravitonKT1176.h"
#include "Effects/GravitonKTBusComp.h"
#include "Effects/GravitonVSS4.h"
#include "Effects/GravitonNebulaDelay.h"
#include "Effects/GravitonManipulatorPitch.h"
#include "Effects/GravitonVocalPitchCorrection.h"
#include "Effects/GravitonFlangerBL20.h"
#include "Effects/GravitonOpticalPhaser.h"
#include "Effects/GravitonRackDynamics.h"
#include "Effects/GravitonDistortion.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>

namespace nova::graviton {

// ==============================================================================
// 1. VSS Algorithmic Stereo Reverb (TC Electronic VSS4 / VSS3 Emulation)
// Reference: MIDAS Heritage-D HD96-24 Chapter 15 (pp. 133-143)
// ==============================================================================
class VSSReverb final {
public:
    static constexpr int kMaxPreDelaySamples = 48000; // 500 ms @ 96 kHz
    static constexpr size_t kNumFDNLst = 4;

    VSSReverb() noexcept {
        preDelayBufferL.resize(kMaxPreDelaySamples, 0.0);
        preDelayBufferR.resize(kMaxPreDelaySamples, 0.0);
        reset();
    }

    void reset() noexcept {
        std::fill(preDelayBufferL.begin(), preDelayBufferL.end(), 0.0);
        std::fill(preDelayBufferR.begin(), preDelayBufferR.end(), 0.0);
        preDelayWritePos = 0;

        for (auto& ap : diffusersL) ap.reset();
        for (auto& ap : diffusersR) ap.reset();
        for (auto& buf : fdnBuffersL) std::fill(buf.begin(), buf.end(), 0.0);
        for (auto& buf : fdnBuffersR) std::fill(buf.begin(), buf.end(), 0.0);
        fdnWritePosL.fill(0);
        fdnWritePosR.fill(0);
        fdnFiltersL.fill({});
        fdnFiltersR.fill({});

        cachedSampleRate = 48000.0;
        updateDelays();
        updateFilters();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
            cachedSampleRate = sr;
            updateDelays();
            updateFilters();
        }
    }

    void setDecay(double seconds) noexcept {
        decaySec = std::clamp(seconds, 0.1, 20.0);
        updateFeedback();
    }
    double getDecay() const noexcept { return decaySec; }

    void setPreDelay(double ms) noexcept { preDelayMs = std::clamp(ms, 0.0, 300.0); }
    double getPreDelay() const noexcept { return preDelayMs; }

    void setRoomSize(double size) noexcept {
        roomSize = std::clamp(size, 0.1, 2.0);
        updateDelays();
    }
    double getRoomSize() const noexcept { return roomSize; }

    void setHiCut(double freqHz) noexcept {
        hiCutHz = std::clamp(freqHz, 500.0, 20000.0);
        updateFilters();
    }
    double getHiCut() const noexcept { return hiCutHz; }

    void setMix(double wetRatio) noexcept { mix = std::clamp(wetRatio, 0.0, 1.0); }
    double getMix() const noexcept { return mix; }

    void setModel(int model) noexcept {
        vssModel = model; // 0 = VSS4, 1 = VSS3
        if (vssModel == 1) {
            decaySec = 2.0;
            roomSize = 0.85;
            preDelayMs = 20.0;
            hiCutHz = 9500.0;
        }
        updateFeedback();
        updateDelays();
        updateFilters();
    }
    int getModel() const noexcept { return vssModel; }

    // --- Extended VSS3 Studio Controls ---
    void setSpaceType(int sp) noexcept { spaceType = std::clamp(sp, 0, 4); }
    int getSpaceType() const noexcept { return spaceType; }
    void setEarlyLevel(double v) noexcept { earlyLevel = std::clamp(v, 0.0, 1.0); }
    double getEarlyLevel() const noexcept { return earlyLevel; }
    void setTailLevel(double v) noexcept { tailLevel = std::clamp(v, 0.0, 1.0); }
    double getTailLevel() const noexcept { return tailLevel; }
    void setLowDamp(double v) noexcept { lowDamp = std::clamp(v, 0.2, 2.0); updateFilters(); }
    double getLowDamp() const noexcept { return lowDamp; }
    void setModDepth(double v) noexcept { modDepth = std::clamp(v, 0.0, 1.0); }
    double getModDepth() const noexcept { return modDepth; }
    void setStereoWidth(double v) noexcept { stereoWidth = std::clamp(v, 0.0, 2.0); }
    double getStereoWidth() const noexcept { return stereoWidth; }
    void setBypassed(bool b) noexcept { vssBypassed = b; }
    bool isBypassed() const noexcept { return vssBypassed; }

    float getInLevelDb() const noexcept { return inLevelDb.load(std::memory_order_relaxed); }
    float getOutLevelDb() const noexcept { return outLevelDb.load(std::memory_order_relaxed); }

    static constexpr int kNumPresets = 8;
    static const char* getPresetName(int idx) noexcept {
        static constexpr const char* kNames[kNumPresets] = {
            "01 - Concert Hall", "02 - Vocal Studio", "03 - Warm Plate",
            "04 - Acoustic Room", "05 - Drum Ambience", "06 - Grand Chapel",
            "07 - Jazz Club", "08 - Lush Ballad"
        };
        return (idx >= 0 && idx < kNumPresets) ? kNames[idx] : kNames[0];
    }
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    void applyPreset(int idx) noexcept {
        currentPresetIndex = std::clamp(idx, 0, kNumPresets - 1);
        switch (currentPresetIndex) {
            case 0: setSpaceType(0); setDecay(2.8); setRoomSize(1.15); setPreDelay(24.0); setHiCut(8500.0); setEarlyLevel(0.65); setTailLevel(0.90); setModDepth(0.35); setStereoWidth(1.15); setMix(0.32); break;
            case 1: setSpaceType(1); setDecay(1.6); setRoomSize(0.80); setPreDelay(22.0); setHiCut(10500.0); setEarlyLevel(0.75); setTailLevel(0.80); setModDepth(0.30); setStereoWidth(1.05); setMix(0.28); break;
            case 2: setSpaceType(2); setDecay(2.2); setRoomSize(0.95); setPreDelay(12.0); setHiCut(12000.0); setEarlyLevel(0.50); setTailLevel(1.00); setModDepth(0.45); setStereoWidth(1.20); setMix(0.30); break;
            case 3: setSpaceType(1); setDecay(0.95); setRoomSize(0.65); setPreDelay(8.0); setHiCut(9000.0); setEarlyLevel(0.85); setTailLevel(0.70); setModDepth(0.15); setStereoWidth(1.00); setMix(0.24); break;
            case 4: setSpaceType(3); setDecay(0.55); setRoomSize(0.55); setPreDelay(4.0); setHiCut(11000.0); setEarlyLevel(0.95); setTailLevel(0.55); setModDepth(0.10); setStereoWidth(1.10); setMix(0.22); break;
            case 5: setSpaceType(4); setDecay(4.5); setRoomSize(1.45); setPreDelay(35.0); setHiCut(7200.0); setEarlyLevel(0.55); setTailLevel(0.95); setModDepth(0.40); setStereoWidth(1.30); setMix(0.38); break;
            case 6: setSpaceType(1); setDecay(1.25); setRoomSize(0.75); setPreDelay(14.0); setHiCut(7800.0); setEarlyLevel(0.80); setTailLevel(0.75); setModDepth(0.20); setStereoWidth(0.95); setMix(0.26); break;
            case 7: setSpaceType(0); setDecay(3.6); setRoomSize(1.25); setPreDelay(30.0); setHiCut(9800.0); setEarlyLevel(0.60); setTailLevel(1.00); setModDepth(0.55); setStereoWidth(1.35); setMix(0.35); break;
        }
    }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        const double dryL = sanitize(inOutL);
        const double dryR = sanitize(inOutR);
        const double inPk = std::max(std::abs(dryL), std::abs(dryR));
        const float inDb = (inPk <= 1.0e-6) ? -60.0f : static_cast<float>(std::clamp(20.0 * std::log10(inPk), -60.0, 6.0));
        inLevelDb.store(inDb, std::memory_order_relaxed);
        if (vssBypassed) {
            outLevelDb.store(inDb, std::memory_order_relaxed);
            return;
        }

        // 1. Pre-Delay
        const int preDelaySamples = std::clamp(static_cast<int>(preDelayMs * 0.001 * cachedSampleRate), 0, kMaxPreDelaySamples - 1);
        preDelayBufferL[static_cast<size_t>(preDelayWritePos)] = dryL;
        preDelayBufferR[static_cast<size_t>(preDelayWritePos)] = dryR;
        const int readPos = (preDelayWritePos - preDelaySamples + kMaxPreDelaySamples) % kMaxPreDelaySamples;
        double sL = preDelayBufferL[static_cast<size_t>(readPos)];
        double sR = preDelayBufferR[static_cast<size_t>(readPos)];
        preDelayWritePos = (preDelayWritePos + 1) % kMaxPreDelaySamples;

        // 2. Input Diffusion (All-Pass cascade)
        if (vssModel == 1) {
            // VSS3: single-stage clean diffusion to preserve early reflection articulation
            sL = diffusersL[0].process(sL);
            sR = diffusersR[0].process(sR);
        } else {
            // VSS4: dual-stage complex spatial diffusion
            sL = diffusersL[0].process(sL);
            sL = diffusersL[1].process(sL);
            sR = diffusersR[0].process(sR);
            sR = diffusersR[1].process(sR);
        }

        // 3. 4-Channel Feedback Delay Network (FDN)
        std::array<double, kNumFDNLst> fdnOutL{}, fdnOutR{};
        for (size_t i = 0; i < kNumFDNLst; ++i) {
            const auto lenL = fdnLengthsL[i];
            const auto lenR = fdnLengthsR[i];
            const int rPosL = (fdnWritePosL[i] - lenL + static_cast<int>(fdnBuffersL[i].size())) % static_cast<int>(fdnBuffersL[i].size());
            const int rPosR = (fdnWritePosR[i] - lenR + static_cast<int>(fdnBuffersR[i].size())) % static_cast<int>(fdnBuffersR[i].size());
            fdnOutL[i] = fdnBuffersL[i][static_cast<size_t>(rPosL)];
            fdnOutR[i] = fdnBuffersR[i][static_cast<size_t>(rPosR)];
        }

        // Householder mixing matrix: y_i = s_in - 0.5 * sum(fdnOut) + fdnOut_i
        const double sumL = fdnOutL[0] + fdnOutL[1] + fdnOutL[2] + fdnOutL[3];
        const double sumR = fdnOutR[0] + fdnOutR[1] + fdnOutR[2] + fdnOutR[3];

        for (size_t i = 0; i < kNumFDNLst; ++i) {
            const double mixedL = (sL * 0.5) + fdnFeedback * (fdnOutL[i] - 0.5 * sumL);
            const double mixedR = (sR * 0.5) + fdnFeedback * (fdnOutR[i] - 0.5 * sumR);

            // Damping filters
            const double dampedL = fdnFiltersL[i].process(mixedL);
            const double dampedR = fdnFiltersR[i].process(mixedR);

            fdnBuffersL[i][static_cast<size_t>(fdnWritePosL[i])] = sanitize(dampedL);
            fdnBuffersR[i][static_cast<size_t>(fdnWritePosR[i])] = sanitize(dampedR);

            fdnWritePosL[i] = (fdnWritePosL[i] + 1) % static_cast<int>(fdnBuffersL[i].size());
            fdnWritePosR[i] = (fdnWritePosR[i] + 1) % static_cast<int>(fdnBuffersR[i].size());
        }

        // Combine FDN lines with decorrelated polarities + Early/Tail & Stereo Width
        const double earlyMixL = sL * earlyLevel * 0.35;
        const double earlyMixR = sR * earlyLevel * 0.35;
        double wetL = earlyMixL + (fdnOutL[0] - fdnOutL[1] + fdnOutL[2] - fdnOutL[3]) * 0.5 * tailLevel;
        double wetR = earlyMixR + (fdnOutR[0] + fdnOutR[1] - fdnOutR[2] - fdnOutR[3]) * 0.5 * tailLevel;

        if (std::abs(stereoWidth - 1.0) > 0.01) {
            const double mid = (wetL + wetR) * 0.5;
            const double side = (wetL - wetR) * 0.5 * stereoWidth;
            wetL = mid + side;
            wetR = mid - side;
        }

        inOutL = sanitize(dryL * (1.0 - mix) + wetL * mix);
        inOutR = sanitize(dryR * (1.0 - mix) + wetR * mix);
        const double outPk = std::max(std::abs(inOutL), std::abs(inOutR));
        const float outDb = (outPk <= 1.0e-6) ? -60.0f : static_cast<float>(std::clamp(20.0 * std::log10(outPk), -60.0, 6.0));
        outLevelDb.store(outDb, std::memory_order_relaxed);
    }

private:
    void updateDelays() noexcept {
        // Mutually prime delay line lengths scaled by roomSize
        const std::array<double, kNumFDNLst> baseMsL = { 29.3, 37.1, 43.7, 49.9 };
        const std::array<double, kNumFDNLst> baseMsR = { 31.7, 39.3, 41.9, 53.1 };

        for (size_t i = 0; i < kNumFDNLst; ++i) {
            fdnLengthsL[i] = std::clamp(static_cast<int>(baseMsL[i] * 0.001 * cachedSampleRate * roomSize), 16, 12000);
            fdnLengthsR[i] = std::clamp(static_cast<int>(baseMsR[i] * 0.001 * cachedSampleRate * roomSize), 16, 12000);
            fdnBuffersL[i].resize(16384, 0.0);
            fdnBuffersR[i].resize(16384, 0.0);
        }

        // All-pass diffusers
        diffusersL[0].prepare(cachedSampleRate, 140.0 * roomSize);
        diffusersL[1].prepare(cachedSampleRate, 340.0 * roomSize);
        diffusersR[0].prepare(cachedSampleRate, 160.0 * roomSize);
        diffusersR[1].prepare(cachedSampleRate, 320.0 * roomSize);

        updateFeedback();
    }

    void updateFeedback() noexcept {
        // T60 decay formula: gain = 10^(-3 * avgDelay / decaySec)
        const double avgDelaySec = 0.040 * roomSize;
        fdnFeedback = std::clamp(std::pow(10.0, -3.0 * avgDelaySec / decaySec), 0.0, 0.98);
    }

    void updateFilters() noexcept {
        const auto coeffs = makeButterworthLPF(cachedSampleRate, hiCutHz, 0.7071);
        for (size_t i = 0; i < kNumFDNLst; ++i) {
            fdnFiltersL[i].setCoefficients(coeffs);
            fdnFiltersR[i].setCoefficients(coeffs);
        }
    }

    struct SimpleAllPass {
        std::vector<double> buf;
        int wPos = 0;
        int delay = 200;
        double gain = 0.5;

        void prepare(double sr, double delaySamples) noexcept {
            (void)sr;
            delay = std::clamp(static_cast<int>(delaySamples), 4, 4000);
            buf.resize(static_cast<size_t>(delay + 1), 0.0);
            wPos = 0;
        }

        void reset() noexcept {
            std::fill(buf.begin(), buf.end(), 0.0);
            wPos = 0;
        }

        double process(double in) noexcept {
            if (buf.empty()) return in;
            const int rPos = (wPos - delay + static_cast<int>(buf.size())) % static_cast<int>(buf.size());
            const double bufOut = buf[static_cast<size_t>(rPos)];
            const double out = -gain * in + bufOut;
            buf[static_cast<size_t>(wPos)] = in + gain * out;
            wPos = (wPos + 1) % static_cast<int>(buf.size());
            return out;
        }
    };

    std::vector<double> preDelayBufferL;
    std::vector<double> preDelayBufferR;
    int preDelayWritePos = 0;

    std::array<SimpleAllPass, 2> diffusersL;
    std::array<SimpleAllPass, 2> diffusersR;

    std::array<std::vector<double>, kNumFDNLst> fdnBuffersL;
    std::array<std::vector<double>, kNumFDNLst> fdnBuffersR;
    std::array<int, kNumFDNLst> fdnLengthsL{};
    std::array<int, kNumFDNLst> fdnLengthsR{};
    std::array<int, kNumFDNLst> fdnWritePosL{};
    std::array<int, kNumFDNLst> fdnWritePosR{};
    std::array<Biquad, kNumFDNLst> fdnFiltersL;
    std::array<Biquad, kNumFDNLst> fdnFiltersR;

    double cachedSampleRate = 48000.0;
    double decaySec = 2.0;
    double preDelayMs = 20.0;
    double roomSize = 1.0;
    double hiCutHz = 8000.0;
    double mix = 0.35;
    double fdnFeedback = 0.75;
    int vssModel = 0; // 0 = VSS4, 1 = VSS3
    int spaceType = 0;
    double earlyLevel = 0.65;
    double tailLevel = 0.90;
    double lowDamp = 1.0;
    double modDepth = 0.35;
    double stereoWidth = 1.15;
    bool vssBypassed = false;
    int currentPresetIndex = 0;
    std::atomic<float> inLevelDb{-60.0f}, outLevelDb{-60.0f};
};

// ==============================================================================
// 2. Stereo Tape & Digital Delay with LFO Modulation (Chapter 15, pp. 149-153)
// ==============================================================================
class StereoDelay final {
public:
    static constexpr int kMaxDelaySamples = 192000; // 2 seconds @ 96 kHz

    StereoDelay() noexcept {
        delayBufferL.resize(kMaxDelaySamples, 0.0);
        delayBufferR.resize(kMaxDelaySamples, 0.0);
        reset();
    }

    void reset() noexcept {
        std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0);
        std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0);
        writePos = 0;
        lfoPhase = 0.0;
        cachedSampleRate = 48000.0;
        filterL.reset();
        filterR.reset();
        updateFilter();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
            cachedSampleRate = sr;
            updateFilter();
        }
    }

    void setDelayTimeL(double ms) noexcept { delayTimeMsL = std::clamp(ms, 1.0, 2000.0); }
    void setDelayTimeR(double ms) noexcept { delayTimeMsR = std::clamp(ms, 1.0, 2000.0); }
    double getDelayTimeL() const noexcept { return delayTimeMsL; }
    double getDelayTimeR() const noexcept { return delayTimeMsR; }

    void setFeedback(double fb) noexcept { feedback = std::clamp(fb, 0.0, 0.95); }
    double getFeedback() const noexcept { return feedback; }

    void setModRate(double hz) noexcept { modRateHz = std::clamp(hz, 0.01, 10.0); }
    double getModRate() const noexcept { return modRateHz; }

    void setModDepth(double depth) noexcept { modDepthMs = std::clamp(depth, 0.0, 10.0); }
    double getModDepth() const noexcept { return modDepthMs; }

    void setHiDamp(double freqHz) noexcept {
        hiDampHz = std::clamp(freqHz, 1000.0, 20000.0);
        updateFilter();
    }
    double getHiDamp() const noexcept { return hiDampHz; }

    void setPingPong(bool enabled) noexcept { pingPong = enabled; }
    bool getPingPong() const noexcept { return pingPong; }

    void setMix(double wetRatio) noexcept { mix = std::clamp(wetRatio, 0.0, 1.0); }
    double getMix() const noexcept { return mix; }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        const double dryL = inOutL;
        const double dryR = inOutR;

        // LFO for modulation (tape wow/flutter / chorus)
        const double lfo = std::sin(lfoPhase);
        lfoPhase += 2.0 * 3.14159265358979323846 * modRateHz / cachedSampleRate;
        if (lfoPhase >= 2.0 * 3.14159265358979323846) lfoPhase -= 2.0 * 3.14159265358979323846;

        const double modOffset = lfo * (modDepthMs * 0.001 * cachedSampleRate);

        // Compute delay in samples
        const double delaySampL = std::clamp((delayTimeMsL * 0.001 * cachedSampleRate) + modOffset, 1.0, static_cast<double>(kMaxDelaySamples - 2));
        const double delaySampR = std::clamp((delayTimeMsR * 0.001 * cachedSampleRate) - modOffset, 1.0, static_cast<double>(kMaxDelaySamples - 2));

        // Linear interpolation read
        auto readDelay = [](const std::vector<double>& buf, int wPos, double delaySamples) noexcept -> double {
            const int dInt = static_cast<int>(delaySamples);
            const double frac = delaySamples - static_cast<double>(dInt);
            const int sz = static_cast<int>(buf.size());
            const int pos1 = (wPos - dInt + sz) % sz;
            const int pos2 = (pos1 - 1 + sz) % sz;
            return buf[static_cast<size_t>(pos1)] * (1.0 - frac) + buf[static_cast<size_t>(pos2)] * frac;
        };

        const double delayedL = readDelay(delayBufferL, writePos, delaySampL);
        const double delayedR = readDelay(delayBufferR, writePos, delaySampR);

        // Feedback through damping filters
        const double fbInL = pingPong ? delayedR : delayedL;
        const double fbInR = pingPong ? delayedL : delayedR;

        const double filteredL = filterL.process(fbInL);
        const double filteredR = filterR.process(fbInR);

        delayBufferL[static_cast<size_t>(writePos)] = sanitize(dryL + filteredL * feedback);
        delayBufferR[static_cast<size_t>(writePos)] = sanitize(dryR + filteredR * feedback);

        writePos = (writePos + 1) % kMaxDelaySamples;

        inOutL = sanitize(dryL * (1.0 - mix) + delayedL * mix);
        inOutR = sanitize(dryR * (1.0 - mix) + delayedR * mix);
    }

private:
    void updateFilter() noexcept {
        const auto coeffs = makeButterworthLPF(cachedSampleRate, hiDampHz, 0.7071);
        filterL.setCoefficients(coeffs);
        filterR.setCoefficients(coeffs);
    }

    std::vector<double> delayBufferL;
    std::vector<double> delayBufferR;
    int writePos = 0;
    double lfoPhase = 0.0;
    double cachedSampleRate = 48000.0;

    Biquad filterL;
    Biquad filterR;

    double delayTimeMsL = 375.0;
    double delayTimeMsR = 500.0;
    double feedback = 0.40;
    double modRateHz = 0.50;
    double modDepthMs = 1.0;
    double hiDampHz = 6000.0;
    bool pingPong = false;
    double mix = 0.30;
};

// ==============================================================================
// 3. Studio Pitch Shifter & Voice Doubler (ADT) (Chapter 15, pp. 177, 180)
// ==============================================================================
class StudioPitchShifter final {
public:
    static constexpr int kMaxBufferSamples = 16384;

    StudioPitchShifter() noexcept {
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
        cachedSampleRate = 48000.0;
        updateRatio();
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - cachedSampleRate) > 1.0) {
            cachedSampleRate = sr;
            updateRatio();
        }
    }

    void setSemitones(int semi) noexcept {
        semitones = std::clamp(semi, -12, 12);
        updateRatio();
    }
    int getSemitones() const noexcept { return semitones; }

    void setCents(int c) noexcept {
        cents = std::clamp(c, -100, 100);
        updateRatio();
    }
    int getCents() const noexcept { return cents; }

    void setVoiceDoubler(bool enabled) noexcept { voiceDoubler = enabled; }
    bool getVoiceDoubler() const noexcept { return voiceDoubler; }

    void setFeedback(double fb) noexcept { feedback = std::clamp(fb, 0.0, 0.90); }
    double getFeedback() const noexcept { return feedback; }

    void setMix(double wetRatio) noexcept { mix = std::clamp(wetRatio, 0.0, 1.0); }
    double getMix() const noexcept { return mix; }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        const double dryL = inOutL;
        const double dryR = inOutR;

        // Circular buffer recording with feedback
        bufferL[static_cast<size_t>(writePos)] = sanitize(dryL + lastShiftedL * feedback);
        bufferR[static_cast<size_t>(writePos)] = sanitize(dryR + lastShiftedR * feedback);

        // Window size for pitch head crossfade
        const double windowSize = 2048.0; // ~42 ms @ 48 kHz
        const double halfWin = windowSize * 0.5;

        // Dual rotating read heads with triangular crossfade envelope
        const double env1 = (headPos1 < halfWin) ? (headPos1 / halfWin) : ((windowSize - headPos1) / halfWin);
        const double env2 = (headPos2 < halfWin) ? (headPos2 / halfWin) : ((windowSize - headPos2) / halfWin);

        auto readInterp = [](const std::vector<double>& buf, int wPos, double headOffset) noexcept -> double {
            const int offset = static_cast<int>(headOffset);
            const double frac = headOffset - static_cast<double>(offset);
            const int sz = static_cast<int>(buf.size());
            const int pos1 = (wPos - offset + sz) % sz;
            const int pos2 = (pos1 - 1 + sz) % sz;
            return buf[static_cast<size_t>(pos1)] * (1.0 - frac) + buf[static_cast<size_t>(pos2)] * frac;
        };

        double shiftedL = readInterp(bufferL, writePos, headPos1) * env1 + readInterp(bufferL, writePos, headPos2) * env2;
        double shiftedR = readInterp(bufferR, writePos, headPos1) * env1 + readInterp(bufferR, writePos, headPos2) * env2;

        if (voiceDoubler) {
            // ADT spatial doubling: cross-delay right channel by ~15 ms
            const int adtOffset = static_cast<int>(0.015 * cachedSampleRate);
            shiftedR = readInterp(bufferR, writePos, headPos1 + static_cast<double>(adtOffset)) * env1;
        }

        lastShiftedL = shiftedL;
        lastShiftedR = shiftedR;

        // Advance rotating heads according to pitch ratio: speed = 1.0 - ratio
        const double headSpeed = 1.0 - pitchRatio;
        headPos1 += headSpeed;
        headPos2 += headSpeed;

        if (headPos1 >= windowSize) headPos1 -= windowSize;
        if (headPos1 < 0.0) headPos1 += windowSize;

        if (headPos2 >= windowSize) headPos2 -= windowSize;
        if (headPos2 < 0.0) headPos2 += windowSize;

        writePos = (writePos + 1) % kMaxBufferSamples;

        inOutL = sanitize(dryL * (1.0 - mix) + shiftedL * mix);
        inOutR = sanitize(dryR * (1.0 - mix) + shiftedR * mix);
    }

private:
    void updateRatio() noexcept {
        const double totalCents = static_cast<double>(semitones * 100 + cents);
        pitchRatio = std::pow(2.0, totalCents / 1200.0);
        headPos1 = 0.0;
        headPos2 = 1024.0; // 180 degrees out-of-phase for smooth constant-power crossfading
    }

    std::vector<double> bufferL;
    std::vector<double> bufferR;
    int writePos = 0;
    double headPos1 = 0.0;
    double headPos2 = 1024.0;
    double pitchRatio = 1.0;
    double cachedSampleRate = 48000.0;

    int semitones = 0;
    int cents = 0;
    bool voiceDoubler = false;
    double feedback = 0.0;
    double mix = 0.50;

    double lastShiftedL = 0.0;
    double lastShiftedR = 0.0;
};

// ==============================================================================
// 4. Stereo Chorus Engine (Modulated Short Delay)
// ==============================================================================
class ChorusEngine final {
public:
    static constexpr int kMaxDelaySamples = 4800; // 50 ms @ 96 kHz
    ChorusEngine() noexcept { chorusBufferL.resize(kMaxDelaySamples, 0.0); chorusBufferR.resize(kMaxDelaySamples, 0.0); reset(); }
    void reset() noexcept {
        std::fill(chorusBufferL.begin(), chorusBufferL.end(), 0.0);
        std::fill(chorusBufferR.begin(), chorusBufferR.end(), 0.0);
        writePosL = writePosR = 0;
        lfoPhase = 0.0;
        cachedSampleRate = 48000.0;
    }
    void setSampleRate(double sr) noexcept { cachedSampleRate = sr; }
    void setDelayMs(double ms) noexcept { delayMs = std::clamp(ms, 5.0, 50.0); }
    void setModRateHz(double hz) noexcept { modRateHz = std::clamp(hz, 0.05, 5.0); }
    void setModDepthMs(double ms) noexcept { modDepthMs = std::clamp(ms, 0.0, 20.0); }
    void setMix(double newMix) noexcept { mix = std::clamp(newMix, 0.0, 1.0); }
    double getDelayMs() const noexcept { return delayMs; }
    double getModRateHz() const noexcept { return modRateHz; }
    double getModDepthMs() const noexcept { return modDepthMs; }
    double getMix() const noexcept { return mix; }
    void processStereo(double& inOutL, double& inOutR) noexcept {
        const double dryL = inOutL;
        const double dryR = inOutR;
        const double lfo = std::sin(lfoPhase);
        lfoPhase += 2.0 * 3.14159265358979323846 * modRateHz / cachedSampleRate;
        if (lfoPhase >= 2.0 * 3.14159265358979323846) lfoPhase -= 2.0 * 3.14159265358979323846;
        const double modSamples = (lfo * 2.0 - 1.0) * (modDepthMs * 0.001 * cachedSampleRate);
        const double delaySamplesL = std::clamp(delayMs * 0.001 * cachedSampleRate + modSamples, 1.0, static_cast<double>(kMaxDelaySamples - 2));
        const double delaySamplesR = std::clamp(delayMs * 0.001 * cachedSampleRate - modSamples, 1.0, static_cast<double>(kMaxDelaySamples - 2));
        auto readDelay = [](const std::vector<double>& buf, int wPos, double delaySamples) noexcept -> double {
            const int dInt = static_cast<int>(delaySamples);
            const double frac = delaySamples - static_cast<double>(dInt);
            const int sz = static_cast<int>(buf.size());
            const int pos1 = (wPos - dInt + sz) % sz;
            const int pos2 = (pos1 - 1 + sz) % sz;
            return buf[static_cast<size_t>(pos1)] * (1.0 - frac) + buf[static_cast<size_t>(pos2)] * frac;
        };
        const double delayedL = readDelay(chorusBufferL, writePosL, delaySamplesL);
        const double delayedR = readDelay(chorusBufferR, writePosR, delaySamplesR);
        chorusBufferL[static_cast<size_t>(writePosL)] = dryL;
        chorusBufferR[static_cast<size_t>(writePosR)] = dryR;
        writePosL = (writePosL + 1) % kMaxDelaySamples;
        writePosR = (writePosR + 1) % kMaxDelaySamples;
        inOutL = sanitize(dryL * (1.0 - mix) + delayedL * mix);
        inOutR = sanitize(dryR * (1.0 - mix) + delayedR * mix);
    }
private:
    std::vector<double> chorusBufferL;
    std::vector<double> chorusBufferR;
    int writePosL = 0;
    int writePosR = 0;
    double lfoPhase = 0.0;
    double cachedSampleRate = 48000.0;
    double delayMs = 20.0;
    double modRateHz = 0.5;
    double modDepthMs = 2.0;
    double mix = 0.30;
};

// ==============================================================================
// 5. Stereo Flanger Engine (NOVA HD Flanger BL-20 BBD & Function Generator)
// ==============================================================================
using FlangerEngine = FlangerBL20Engine;

inline BiquadCoefficients makeAllPassCoefficients(Sample frequency, Sample sampleRate) noexcept
{
    BiquadCoefficients coefficients{};
    const Sample f = std::clamp(frequency, 20.0, sampleRate * 0.45);
    const Sample omega = 2.0 * 3.14159265358979323846 * f / sampleRate;
    const Sample cosOmega = std::cos(omega);
    coefficients.b0 = 0.5 * (1.0 + cosOmega);
    coefficients.b1 = 0.5 * (1.0 - cosOmega);
    coefficients.b2 = 0.0;
    coefficients.a1 = -cosOmega;
    coefficients.a2 = 0.0;
    return coefficients;
}

// ==============================================================================
// 6. Stereo Phaser Engine (NOVA HD 12-Stage Optical Stereo Phaser)
// ==============================================================================
using PhaserEngine = OpticalPhaserEngine;

// ==============================================================================
// 7. Distortion / Saturation Engine
// ==============================================================================
// DistortionEngine moved to Effects/GravitonDistortion.h

// ==============================================================================
// 4. Virtual FX Rack: 24 Independent DSP Effect Slots (MIDAS Chapter 15)
// ==============================================================================
enum class FxType {
    None = 0,
    Reverb_VSS4 = 1,
    StereoDelay = 2,
    PitchShifter = 3,
    Chorus = 4,
    Flanger = 5,
    Phaser = 6,
    Compressor = 7,
    NoiseGate = 8,
    Distortion = 9,
    MasterX_Multiband = 10,
    EMT140_Plate = 11,
    SpringReverb = 12,
    DVR2_NonLinear = 13,
    TC_SCF = 14,
    TC_NonLin2 = 15,
    KT_DN780 = 16,
    DualTank_Spring = 17,
    ASIO_Bridge = 18,
    SubHarmonic_Synth = 19,
    DN360_InductorGEQ = 20,
    Precision_DeEsser = 21,
    Convolution_Reverb = 22,
    Hybrid_Reverb = 23,
    TC_2290_Delay = 24,
    KT_1176_Limiter = 25,
    KT_BusCompressor = 26,
    Reverb_VSS3 = 27,
    Vocal_Pitch_Corrector = 28
};

struct FxSlot {
    std::atomic<int> type{ static_cast<int>(FxType::None) };
    std::atomic<bool> bypassed{ false };
    std::atomic<int> insertChannel{ -1 }; // -1 = unassigned, 0-143 = channel insert
    std::atomic<int> auxSendSource{ -1 }; // -1 = unassigned, 0-95 = Aux bus send
    std::atomic<int> auxReturnChannel{ -1 }; // 0-143 = return channel
    std::atomic<float> returnLevelDb{ 0.0f }; // -60 dB to +10 dB
    std::atomic<bool> returnMute{ false };
    std::atomic<float> inputPeak{ 0.0f };
    std::atomic<float> outputPeak{ 0.0f };

    VSSReverb reverb;
    StereoDelay delay;
    StudioPitchShifter shifter;
    ChorusEngine chorus;
    FlangerEngine flanger;
    PhaserEngine phaser;
    DistortionEngine distortion;
    MasterXMultiband masterX;
    EMT140PlateReverb plate;
    VintageSpringReverb spring;
    DVR2NonLinearReverb dvr2;
    TCSCFProcessor scf;
    TCNonLin2Processor nonLin2;
    ASIOBridge asioBridge;
    SubHarmonicSynthesizer subHarmonic;
    DN360InductorGEQ dn360;
    PrecisionDeEsser deEsser;
    nova::ConvolutionReverbEngine convReverb;
    nova::HybridReverbEngine hybridReverb;
    TC2290Delay tc2290;
    KT1176Limiter kt1176;
    KTBusCompressor busComp;
    VSSReverb vss3Reverb;
    VSS4ReverbEngine vss4Reverb;
    NebulaStereoDelay nebulaDelay;
    ManipulatorPitchEngine manipulator;
    VocalPitchCorrector vocalPitch;
    RackDynamicsEngine rackDynamics;

    nova::graviton::DynamicsProcessor compressorProcessor;
    nova::graviton::DynamicsProcessor compressorProcessorR;
    nova::graviton::DynamicsProcessor gateProcessor;
    nova::graviton::DynamicsProcessor gateProcessorR;
    double cachedSampleRate = 48000.0;

    double compressorThresholdDb = -18.0;
    double compressorRatio = 4.0;
    double compressorAttackMs = 10.0;
    double compressorReleaseMs = 120.0;
    int compressorMode = static_cast<int>(nova::graviton::CompressorMode::Corrective);

    double gateThresholdDb = -45.0;
    double gateRatio = 4.0;
    double gateAttackMs = 10.0;
    double gateReleaseMs = 120.0;

    double distortionDriveDb = 6.0;
    double distortionMix = 0.25;

    FxSlot() noexcept {
        vss3Reverb.setModel(1);
    }
    FxSlot(const FxSlot&) = delete;
    FxSlot& operator=(const FxSlot&) = delete;

    void setSampleRate(double sr) noexcept {
        if (sr <= 1000.0 || std::abs(sr - cachedSampleRate) < 1.0)
            return;
        reverb.setSampleRate(sr);
        delay.setSampleRate(sr);
        shifter.setSampleRate(sr);
        chorus.setSampleRate(sr);
        flanger.setSampleRate(sr);
        phaser.setSampleRate(sr);
        distortion.setSampleRate(sr);
        masterX.setSampleRate(sr);
        plate.setSampleRate(sr);
        spring.setSampleRate(sr);
        dvr2.setSampleRate(sr);
        scf.setSampleRate(sr);
        nonLin2.setSampleRate(sr);
        subHarmonic.setSampleRate(sr);
        dn360.setSampleRate(sr);
        deEsser.setSampleRate(sr);
        convReverb.setSampleRate(sr);
        hybridReverb.init(sr);
        tc2290.setSampleRate(sr);
        kt1176.setSampleRate(sr);
        busComp.setSampleRate(sr);
        vss3Reverb.setSampleRate(sr);
        vss4Reverb.setSampleRate(sr);
        nebulaDelay.setSampleRate(sr);
        manipulator.setSampleRate(sr);
        vocalPitch.setSampleRate(sr);
        rackDynamics.setSampleRate(sr);
        cachedSampleRate = sr;
    }

    void reset() noexcept {
        reverb.reset();
        delay.reset();
        shifter.reset();
        chorus.reset();
        flanger.reset();
        phaser.reset();
        distortion.reset();
        masterX.reset();
        plate.reset();
        spring.reset();
        dvr2.reset();
        scf.reset();
        nonLin2.reset();
        subHarmonic.reset();
        dn360.reset();
        deEsser.reset();
        convReverb.reset();
        hybridReverb.reset();
        tc2290.reset();
        kt1176.reset();
        busComp.reset();
        vss3Reverb.reset();
        vss3Reverb.setModel(1);
        vss4Reverb.reset();
        nebulaDelay.reset();
        manipulator.reset();
        vocalPitch.reset();
        rackDynamics.reset();
        compressorProcessor.reset();
        compressorProcessorR.reset();
        gateProcessor.reset();
        gateProcessorR.reset();
        inputPeak.store(0.0f, std::memory_order_relaxed);
        outputPeak.store(0.0f, std::memory_order_relaxed);
    }

    void setCompressorThresholdDb(double valueDb) noexcept {
        compressorThresholdDb = valueDb;
        rackDynamics.setThresholdDb(valueDb);
    }
    double getCompressorThresholdDb() const noexcept { return rackDynamics.getThresholdDb(); }
    void setCompressorRatio(double valueRatio) noexcept {
        compressorRatio = valueRatio;
        rackDynamics.setRatio(valueRatio);
    }
    double getCompressorRatio() const noexcept { return rackDynamics.getRatio(); }
    void setCompressorAttackMs(double valueMs) noexcept {
        compressorAttackMs = valueMs;
        rackDynamics.setAttackMs(valueMs);
    }
    double getCompressorAttackMs() const noexcept { return rackDynamics.getAttackMs(); }
    void setCompressorReleaseMs(double valueMs) noexcept {
        compressorReleaseMs = valueMs;
        rackDynamics.setReleaseMs(valueMs);
    }
    double getCompressorReleaseMs() const noexcept { return rackDynamics.getReleaseMs(); }

    void setCompressorMode(int value) noexcept {
        compressorMode = value;
        // Map 1..4 legacy rack mode to circuit models (VintageFET, PlatinumDigital, StudioVCA, VintageOpto)
        static constexpr int kLegacyToCircuit[4] = { 0, 1, 2, 6 };
        const int idx = std::clamp(value - 1, 0, 3);
        rackDynamics.setCircuitModel(kLegacyToCircuit[idx]);
    }
    int getCompressorMode() const noexcept { return compressorMode; }

    void setGateThresholdDb(double valueDb) noexcept {
        gateThresholdDb = valueDb;
        rackDynamics.setMode(static_cast<int>(RackDynamicsEngine::Mode::ExpanderGate));
        rackDynamics.setThresholdDb(valueDb);
    }
    double getGateThresholdDb() const noexcept { return rackDynamics.getThresholdDb(); }
    void setGateRatio(double valueRatio) noexcept {
        gateRatio = valueRatio;
        rackDynamics.setMode(static_cast<int>(RackDynamicsEngine::Mode::ExpanderGate));
        rackDynamics.setRatio(valueRatio);
    }
    double getGateRatio() const noexcept { return rackDynamics.getRatio(); }
    void setGateAttackMs(double valueMs) noexcept {
        gateAttackMs = valueMs;
        rackDynamics.setAttackMs(valueMs);
    }
    double getGateAttackMs() const noexcept { return rackDynamics.getAttackMs(); }
    void setGateReleaseMs(double valueMs) noexcept {
        gateReleaseMs = valueMs;
        rackDynamics.setReleaseMs(valueMs);
    }
    double getGateReleaseMs() const noexcept { return rackDynamics.getReleaseMs(); }

    void setDistortionDriveDb(double valueDb) noexcept {
        distortionDriveDb = valueDb;
        distortion.setDriveDb(valueDb);
        distortion.setTapeDriveDb(valueDb * 0.5);
    }
    double getDistortionDriveDb() const noexcept { return distortion.getDriveDb(); }
    void setDistortionMix(double valueMix) noexcept {
        distortionMix = valueMix;
        distortion.setMix(valueMix);
    }
    double getDistortionMix() const noexcept { return distortion.getMix(); }

    void processStereo(double& inOutL, double& inOutR) noexcept {
        if (bypassed.load(std::memory_order_relaxed)) return;

        const auto curType = static_cast<FxType>(type.load(std::memory_order_relaxed));
        if (curType == FxType::None) return;
        const float inputLevel = static_cast<float>(
            std::max(std::abs(inOutL), std::abs(inOutR)));
        switch (curType) {
            case FxType::Reverb_VSS4:
                vss4Reverb.processStereo(inOutL, inOutR);
                break;
            case FxType::StereoDelay:
                nebulaDelay.processStereo(inOutL, inOutR);
                break;
            case FxType::PitchShifter:
                manipulator.processStereo(inOutL, inOutR);
                break;
            case FxType::Vocal_Pitch_Corrector:
                vocalPitch.processStereo(inOutL, inOutR);
                break;
            case FxType::Chorus:
                chorus.processStereo(inOutL, inOutR);
                break;
            case FxType::Flanger:
                flanger.processStereo(inOutL, inOutR);
                break;
            case FxType::Phaser:
                phaser.processStereo(inOutL, inOutR);
                break;
            case FxType::Compressor:
                processCompressor(inOutL, inOutR);
                break;
            case FxType::NoiseGate:
                processNoiseGate(inOutL, inOutR);
                break;
            case FxType::Distortion:
                distortion.processStereo(inOutL, inOutR);
                break;
            case FxType::MasterX_Multiband:
                masterX.processStereo(inOutL, inOutR);
                break;
            case FxType::EMT140_Plate:
                plate.processStereo(inOutL, inOutR);
                break;
            case FxType::SpringReverb:
                spring.processStereo(inOutL, inOutR);
                break;
            case FxType::DVR2_NonLinear:
                dvr2.processStereo(inOutL, inOutR);
                break;
            case FxType::TC_SCF:
                scf.processStereo(inOutL, inOutR);
                break;
            case FxType::TC_NonLin2:
                nonLin2.processStereo(inOutL, inOutR);
                break;
            case FxType::KT_DN780:
                plate.setMode(1);
                plate.processStereo(inOutL, inOutR);
                break;
            case FxType::DualTank_Spring:
                spring.processStereo(inOutL, inOutR);
                break;
            case FxType::ASIO_Bridge:
                asioBridge.processStereo(inOutL, inOutR);
                break;
            case FxType::SubHarmonic_Synth:
                subHarmonic.processStereo(inOutL, inOutR);
                break;
            case FxType::DN360_InductorGEQ:
                dn360.processStereo(inOutL, inOutR);
                break;
            case FxType::Precision_DeEsser:
                deEsser.processStereo(inOutL, inOutR);
                break;
            case FxType::Convolution_Reverb: {
                float fL = static_cast<float>(inOutL);
                float fR = static_cast<float>(inOutR);
                convReverb.processStereo(fL, fR);
                inOutL = static_cast<double>(fL);
                inOutR = static_cast<double>(fR);
                break;
            }
            case FxType::Hybrid_Reverb: {
                float fL = static_cast<float>(inOutL);
                float fR = static_cast<float>(inOutR);
                hybridReverb.processStereo(fL, fR);
                inOutL = static_cast<double>(fL);
                inOutR = static_cast<double>(fR);
                break;
            }
            case FxType::TC_2290_Delay:
                tc2290.processStereo(inOutL, inOutR);
                break;
            case FxType::KT_1176_Limiter:
                kt1176.processStereo(inOutL, inOutR);
                break;
            case FxType::KT_BusCompressor:
                busComp.processStereo(inOutL, inOutR);
                break;
            case FxType::Reverb_VSS3:
                vss3Reverb.processStereo(inOutL, inOutR);
                break;
            case FxType::None:
            default:
                break;
        }
        updatePeak(inputPeak, inputLevel);
        updatePeak(outputPeak, static_cast<float>(
            std::max(std::abs(inOutL), std::abs(inOutR))));
    }

private:
    static void updatePeak(std::atomic<float>& peak, float value) noexcept
    {
        if (!std::isfinite(value))
            return;
        float current = peak.load(std::memory_order_relaxed);
        while (current < value &&
               !peak.compare_exchange_weak(current, value,
                                           std::memory_order_relaxed,
                                           std::memory_order_relaxed)) {
        }
    }

    void processCompressor(double& inOutL, double& inOutR) noexcept
    {
        if (compressorMode == 0)
            return;
        rackDynamics.processStereo(inOutL, inOutR);
    }

    void processNoiseGate(double& inOutL, double& inOutR) noexcept
    {
        rackDynamics.setMode(static_cast<int>(RackDynamicsEngine::Mode::ExpanderGate));
        rackDynamics.processStereo(inOutL, inOutR);
    }
};

class EffectsRack final {
public:
    static constexpr size_t kNumSlots = 24;

    EffectsRack() noexcept {
        reset();
    }

    void reset() noexcept {
        cachedRackSampleRate = -1.0;
        for (auto& s : slots) s.reset();
    }

    void setSampleRate(double sr) noexcept {
        if (sr <= 1000.0 || std::abs(sr - cachedRackSampleRate) < 1.0)
            return;
        cachedRackSampleRate = sr;
        for (auto& s : slots) s.setSampleRate(sr);
    }

    FxSlot& getSlot(size_t index) noexcept {
        return slots[index < kNumSlots ? index : 0];
    }
    const FxSlot& getSlot(size_t index) const noexcept {
        return slots[index < kNumSlots ? index : 0];
    }

    static constexpr int kMasterInsertChannel = 1000;

    // Process channel insert if an FX slot is assigned to this channel
    void processChannelInsert(int channelIndex, double& inOutL, double& inOutR) noexcept {
        if (channelIndex < 0) return;
        for (auto& slot : slots) {
            if (slot.insertChannel.load(std::memory_order_relaxed) == channelIndex &&
                !slot.bypassed.load(std::memory_order_relaxed) &&
                slot.type.load(std::memory_order_relaxed) != static_cast<int>(FxType::None)) {
                slot.processStereo(inOutL, inOutR);
            }
        }
    }

    // Process Master Bus insert (MIDAS Chapters 15 & 18)
    void processMasterInsert(double& inOutL, double& inOutR) noexcept {
        for (auto& slot : slots) {
            if (slot.insertChannel.load(std::memory_order_relaxed) == kMasterInsertChannel &&
                !slot.bypassed.load(std::memory_order_relaxed) &&
                slot.type.load(std::memory_order_relaxed) != static_cast<int>(FxType::None)) {
                slot.processStereo(inOutL, inOutR);
            }
        }
    }

    template <size_t NumAux, size_t NumChannels>
    void processAuxSends(const std::array<double, NumAux>& auxSums,
                         double& masterAccumL, double& masterAccumR,
                         std::array<double, NumChannels>& channelInjection) noexcept {
        for (auto& slot : slots) {
            const int auxSrc = slot.auxSendSource.load(std::memory_order_relaxed);
            if (auxSrc >= 0 && static_cast<size_t>(auxSrc) < NumAux &&
                !slot.bypassed.load(std::memory_order_relaxed) &&
                slot.type.load(std::memory_order_relaxed) != static_cast<int>(FxType::None)) {
                
                double fxInL = auxSums[static_cast<size_t>(auxSrc)];
                double fxInR = fxInL;
                slot.processStereo(fxInL, fxInR);

                if (!slot.returnMute.load(std::memory_order_relaxed)) {
                    const double retGain = dbToGain(
                        std::clamp(static_cast<double>(slot.returnLevelDb.load(std::memory_order_relaxed)), -60.0, 10.0));
                    fxInL *= retGain;
                    fxInR *= retGain;

                    const int retCh = slot.auxReturnChannel.load(std::memory_order_relaxed);
                    if (retCh >= 0 && static_cast<size_t>(retCh) < NumChannels) {
                        channelInjection[static_cast<size_t>(retCh)] += (fxInL + fxInR) * 0.5;
                    } else {
                        // Default return to Master L/R
                        masterAccumL += fxInL;
                        masterAccumR += fxInR;
                    }
                }
            }
        }
    }

private:
    double cachedRackSampleRate = -1.0;
    std::array<FxSlot, kNumSlots> slots;
};

} // namespace nova::graviton
