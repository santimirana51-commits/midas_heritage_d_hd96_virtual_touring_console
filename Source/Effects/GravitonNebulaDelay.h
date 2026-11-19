#pragma once

#include "DSP/GravitonDSP.h"
#include "DSP/GravitonAtomicParameterSnapshot.h"
#include <mutex>
#include <type_traits>
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>
#include <numbers>

namespace nova::graviton {

// ==============================================================================
// Note division table for tempo-synchronized delay
// ==============================================================================
enum class NoteDivision : int {
    Div_1_64 = 0,
    Div_1_32 = 1,
    Div_1_16T = 2,
    Div_1_16 = 3,
    Div_1_16D = 4,
    Div_1_8T = 5,
    Div_1_8 = 6,
    Div_1_8D = 7,
    Div_1_4T = 8,
    Div_1_4 = 9,
    Div_1_4D = 10,
    Div_1_2T = 11,
    Div_1_2 = 12,
    Div_1_1 = 13,
    NumDivisions = 14
};

inline const char* getNoteDivisionName(NoteDivision div) noexcept {
    switch (div) {
        case NoteDivision::Div_1_64:  return "1/64";
        case NoteDivision::Div_1_32:  return "1/32";
        case NoteDivision::Div_1_16T: return "1/16T";
        case NoteDivision::Div_1_16:  return "1/16";
        case NoteDivision::Div_1_16D: return "1/16D";
        case NoteDivision::Div_1_8T:  return "1/8T";
        case NoteDivision::Div_1_8:   return "1/8";
        case NoteDivision::Div_1_8D:  return "1/8D";
        case NoteDivision::Div_1_4T:  return "1/4T";
        case NoteDivision::Div_1_4:   return "1/4";
        case NoteDivision::Div_1_4D:  return "1/4D";
        case NoteDivision::Div_1_2T:  return "1/2T";
        case NoteDivision::Div_1_2:   return "1/2";
        case NoteDivision::Div_1_1:   return "1/1";
        default: return "1/4";
    }
}

// Beats per division (quarter note = 1.0 beat)
inline double getNoteBeats(NoteDivision div) noexcept {
    switch (div) {
        case NoteDivision::Div_1_64:  return 0.0625;
        case NoteDivision::Div_1_32:  return 0.125;
        case NoteDivision::Div_1_16T: return 0.25 * (2.0 / 3.0);
        case NoteDivision::Div_1_16:  return 0.25;
        case NoteDivision::Div_1_16D: return 0.25 * 1.5;
        case NoteDivision::Div_1_8T:  return 0.5 * (2.0 / 3.0);
        case NoteDivision::Div_1_8:   return 0.5;
        case NoteDivision::Div_1_8D:  return 0.5 * 1.5;
        case NoteDivision::Div_1_4T:  return 1.0 * (2.0 / 3.0);
        case NoteDivision::Div_1_4:   return 1.0;
        case NoteDivision::Div_1_4D:  return 1.0 * 1.5;
        case NoteDivision::Div_1_2T:  return 2.0 * (2.0 / 3.0);
        case NoteDivision::Div_1_2:   return 2.0;
        case NoteDivision::Div_1_1:   return 4.0;
        default: return 1.0;
    }
}

enum class DelayInputSource : int {
    Left = 0,
    Right = 1,
    Sum = 2,
    Mute = 3
};

enum class DelayAnalogMode : int {
    Off = 0,
    CleanDigital = 1,
    WarmTape = 2,
    VintageTube = 3,
    LoFi = 4
};

// ==============================================================================
// Nebula Stereo Tape & Digital Delay Engine
// Reference: Nebula Audio Stereo Delay Processor & Midas HD96 Delay Section
// ==============================================================================
class NebulaStereoDelay final {
public:
    static constexpr int kMaxDelaySamples = 288000; // 3.0 seconds @ 96 kHz
    static constexpr size_t kMaxFilterStages = 4;   // Up to 48 dB/oct (4 cascaded 2nd-order stages)

    NebulaStereoDelay() noexcept {
        delayBufferL.resize(kMaxDelaySamples, 0.0);
        delayBufferR.resize(kMaxDelaySamples, 0.0);
        reset();
    }

    // Call only while the audio callback is stopped: this clears DSP state.
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0);
        std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0);
        writePos = 0;
        prevFbSampleL = 0.0;
        prevFbSampleR = 0.0;
        wowFlutterPhase = 0.0;

        for (auto& s : hpfStagesL) s.reset();
        for (auto& s : hpfStagesR) s.reset();
        for (auto& s : lpfStagesL) s.reset();
        for (auto& s : lpfStagesR) s.reset();

        meterInL.store(0.0f, std::memory_order_relaxed);
        meterInR.store(0.0f, std::memory_order_relaxed);
        meterOutL.store(0.0f, std::memory_order_relaxed);
        meterOutR.store(0.0f, std::memory_order_relaxed);
        meterCorrelation.store(1.0f, std::memory_order_relaxed);
        meterBalance.store(0.0f, std::memory_order_relaxed);
        clipOccurredIn.store(false, std::memory_order_relaxed);
        clipOccurredOut.store(false, std::memory_order_relaxed);
        peakHoldInVal.store(0.0f, std::memory_order_relaxed);
        peakHoldOutVal.store(0.0f, std::memory_order_relaxed);
        corrDotSum = corrEnergyL = corrEnergyR = 0.0;

        publishParamsLocked();
        audioParams_ = makeCurrentPreset();
        updateFilters(audioParams_);
        audioRevision_ = audioParams_.revision;
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0 && std::abs(sr - sampleRate) > 1.0) {
            sampleRate = sr; // coefficients are rebuilt by processStereo on the audio thread
        }
    }

    // ── Global & Sync Controls ─────────────────────────────────────────────
    void setBpm(double newBpm) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); bpm = std::clamp(newBpm, 20.0, 300.0); 
        publishParamsLocked();
    }
    double getBpm() const noexcept { return getCurrentPreset().bpm; }

    void setSyncEnabled(bool sync) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); syncEnabled = sync; 
        publishParamsLocked();
    }
    bool getSyncEnabled() const noexcept { return getCurrentPreset().syncEnabled; }

    void setLinked(bool lk) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); linked = lk; 
        publishParamsLocked();
    }
    bool getLinked() const noexcept { return getCurrentPreset().linked; }

    void setAnalogMode(DelayAnalogMode mode) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); analogMode = mode; 
        publishParamsLocked();
    }
    DelayAnalogMode getAnalogMode() const noexcept { return getCurrentPreset().analogMode; }

    void setFxEnabled(bool on) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); fxEnabled = on; 
        publishParamsLocked();
    }
    bool getFxEnabled() const noexcept { return getCurrentPreset().fxEnabled; }

    void setBypass(bool bp) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); bypassed = bp; 
        publishParamsLocked();
    }
    bool getBypass() const noexcept { return getCurrentPreset().bypassed; }

    void setInputGainDb(double db) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); inGainDb = std::clamp(db, -24.0, 24.0); 
        publishParamsLocked();
    }
    double getInputGainDb() const noexcept { return getCurrentPreset().inGainDb; }

    void setOutputGainDb(double db) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); outGainDb = std::clamp(db, -24.0, 24.0); 
        publishParamsLocked();
    }
    double getOutputGainDb() const noexcept { return getCurrentPreset().outGainDb; }

    void resetClip() noexcept {
        clipOccurredIn.store(false, std::memory_order_relaxed);
        clipOccurredOut.store(false, std::memory_order_relaxed);
        peakHoldInVal.store(0.0f, std::memory_order_relaxed);
        peakHoldOutVal.store(0.0f, std::memory_order_relaxed);
    }
    bool hasClipIn() const noexcept { return clipOccurredIn.load(std::memory_order_relaxed); }
    bool hasClipOut() const noexcept { return clipOccurredOut.load(std::memory_order_relaxed); }
    float getPeakHoldIn() const noexcept { return peakHoldInVal.load(std::memory_order_relaxed); }
    float getPeakHoldOut() const noexcept { return peakHoldOutVal.load(std::memory_order_relaxed); }
    float getStereoCorrelation() const noexcept { return meterCorrelation.load(std::memory_order_relaxed); }
    float getLrBalance() const noexcept { return meterBalance.load(std::memory_order_relaxed); }

    // ── Preset Definition & Factory Presets ────────────────────────────────
    struct Preset {
        const char* name = "Default";
        const char* description = "Default Clean Delay";
        bool syncEnabled = true;
        DelayAnalogMode analogMode = DelayAnalogMode::CleanDigital;
        // Left
        DelayInputSource inputSourceL = DelayInputSource::Left;
        double delayTimeMsL = 131.0;
        NoteDivision noteDivL = NoteDivision::Div_1_16;
        double deviationCentsL = 0.0;
        double hpfHzL = 72.7;
        double hpfsDbL = 21.3;
        double lpfHzL = 5800.0;
        double lpfsDbL = 47.0;
        double feedbackPctL = 45.0;
        double crossfeedPctLR = 25.0;
        bool phaseInvFbL = false;
        bool phaseInvCrossLR = false;
        // Right
        DelayInputSource inputSourceR = DelayInputSource::Right;
        double delayTimeMsR = 349.0;
        NoteDivision noteDivR = NoteDivision::Div_1_4;
        double deviationCentsR = 0.0;
        double hpfHzR = 72.7;
        double hpfsDbR = 21.3;
        double lpfHzR = 5800.0;
        double lpfsDbR = 47.0;
        double feedbackPctR = 45.0;
        double crossfeedPctRL = 25.0;
        bool phaseInvFbR = false;
        bool phaseInvCrossRL = false;
        // Output & Gain
        double wetLevelPctL = 100.0;
        double dryLevelPctL = 50.0;
        double wetPanL = -100.0;
        double dryPanL = -40.0;
        double wetLevelPctR = 100.0;
        double dryLevelPctR = 50.0;
        double wetPanR = 100.0;
        double dryPanR = 40.0;
        double inGainDb = 0.0;
        double outGainDb = 0.0;
        double bpm = 120.0;
        bool fxEnabled = true;
        bool bypassed = false;
        bool linked = false;
        uint64_t revision = 1;
    };
    static_assert(std::is_trivially_copyable<Preset>::value, "Delay preset snapshot must be trivially copyable");

    static const std::vector<Preset>& getFactoryPresets() noexcept {
        static const std::vector<Preset> presets = {
            // 1. Vocal Slap
            {
                "Vocal Slap", "Slapback vocal",
                false, DelayAnalogMode::WarmTape,
                DelayInputSource::Left, 85.0, NoteDivision::Div_1_32, 0.0,
                120.0, 18.0, 6000.0, 24.0, 0.0, 0.0, false, false,
                DelayInputSource::Right, 110.0, NoteDivision::Div_1_16T, 12.0,
                140.0, 18.0, 5500.0, 24.0, 5.0, 0.0, false, false,
                60.0, 100.0, -75.0, -30.0, 60.0, 100.0, 75.0, 30.0, 0.0, 0.0
            },
            // 2. Vocal Stereo
            {
                "Vocal Stereo", "Stereo vocal delay",
                true, DelayAnalogMode::WarmTape,
                DelayInputSource::Left, 250.0, NoteDivision::Div_1_8, -10.0,
                100.0, 18.0, 7000.0, 24.0, 30.0, 15.0, false, false,
                DelayInputSource::Right, 500.0, NoteDivision::Div_1_4, 12.0,
                100.0, 18.0, 6500.0, 24.0, 35.0, 15.0, false, false,
                55.0, 100.0, -100.0, 0.0, 55.0, 100.0, 100.0, 0.0, 0.0, 0.0
            },
            // 3. Vocal Wide
            {
                "Vocal Wide", "Memperlebar vocal",
                false, DelayAnalogMode::CleanDigital,
                DelayInputSource::Left, 23.0, NoteDivision::Div_1_32, -25.0,
                150.0, 12.0, 8500.0, 18.0, 0.0, 0.0, false, false,
                DelayInputSource::Right, 37.0, NoteDivision::Div_1_32, 25.0,
                150.0, 12.0, 8500.0, 18.0, 0.0, 0.0, false, false,
                75.0, 100.0, -100.0, -20.0, 75.0, 100.0, 100.0, 20.0, 0.0, 0.0
            },
            // 4. Ping Pong
            {
                "Ping Pong", "Delay bergantian kiri-kanan",
                true, DelayAnalogMode::CleanDigital,
                DelayInputSource::Left, 250.0, NoteDivision::Div_1_8, 0.0,
                90.0, 18.0, 5200.0, 30.0, 10.0, 68.0, false, false,
                DelayInputSource::Right, 500.0, NoteDivision::Div_1_4, 0.0,
                90.0, 18.0, 5200.0, 30.0, 10.0, 68.0, false, false,
                80.0, 80.0, -100.0, 0.0, 80.0, 80.0, 100.0, 0.0, 0.0, 0.0
            },
            // 5. Guitar Space
            {
                "Guitar Space", "Ruang untuk gitar",
                true, DelayAnalogMode::VintageTube,
                DelayInputSource::Left, 375.0, NoteDivision::Div_1_8D, -8.0,
                150.0, 24.0, 4500.0, 36.0, 42.0, 22.0, false, false,
                DelayInputSource::Right, 500.0, NoteDivision::Div_1_4, 10.0,
                180.0, 24.0, 4000.0, 36.0, 45.0, 22.0, false, false,
                65.0, 90.0, -90.0, -25.0, 65.0, 90.0, 90.0, 25.0, 0.0, 0.0
            },
            // 6. Piano Echo
            {
                "Piano Echo", "Echo piano",
                true, DelayAnalogMode::CleanDigital,
                DelayInputSource::Left, 500.0, NoteDivision::Div_1_4, 0.0,
                80.0, 12.0, 5000.0, 24.0, 35.0, 18.0, false, false,
                DelayInputSource::Right, 1000.0, NoteDivision::Div_1_2, 5.0,
                80.0, 12.0, 4500.0, 24.0, 28.0, 18.0, false, false,
                45.0, 100.0, -80.0, 0.0, 45.0, 100.0, 80.0, 0.0, 0.0, 0.0
            },
            // 7. Synth Movement
            {
                "Synth Movement", "Modulasi synthesizer",
                true, DelayAnalogMode::LoFi,
                DelayInputSource::Left, 375.0, NoteDivision::Div_1_16D, -55.0,
                180.0, 24.0, 8000.0, 30.0, 55.0, 35.0, false, false,
                DelayInputSource::Right, 166.0, NoteDivision::Div_1_8T, 60.0,
                180.0, 24.0, 7500.0, 30.0, 50.0, 40.0, false, false,
                80.0, 85.0, -100.0, -15.0, 80.0, 85.0, 100.0, 15.0, 0.0, 0.0
            },
            // 8. Ambient Wash
            {
                "Ambient Wash", "Delay panjang dan atmosferik",
                true, DelayAnalogMode::WarmTape,
                DelayInputSource::Left, 1000.0, NoteDivision::Div_1_2, -15.0,
                200.0, 24.0, 3200.0, 42.0, 72.0, 45.0, false, false,
                DelayInputSource::Right, 2000.0, NoteDivision::Div_1_1, 18.0,
                220.0, 24.0, 2800.0, 42.0, 68.0, 45.0, false, false,
                90.0, 70.0, -100.0, 0.0, 90.0, 70.0, 100.0, 0.0, 0.0, 0.0
            },
            // 9. Chorus Width
            {
                "Chorus Width", "Stereo chorus",
                false, DelayAnalogMode::CleanDigital,
                DelayInputSource::Left, 18.0, NoteDivision::Div_1_64, -35.0,
                150.0, 12.0, 9000.0, 18.0, 15.0, 25.0, false, true,
                DelayInputSource::Right, 24.0, NoteDivision::Div_1_64, 35.0,
                150.0, 12.0, 9000.0, 18.0, 15.0, 25.0, false, false,
                70.0, 90.0, -100.0, 0.0, 70.0, 90.0, 100.0, 0.0, 0.0, 0.0
            },
            // 10. Flanger Motion
            {
                "Flanger Motion", "Efek flanger bergerak",
                false, DelayAnalogMode::LoFi,
                DelayInputSource::Left, 4.5, NoteDivision::Div_1_64, -75.0,
                300.0, 18.0, 10000.0, 24.0, 75.0, 30.0, true, false,
                DelayInputSource::Right, 6.2, NoteDivision::Div_1_64, 75.0,
                300.0, 18.0, 10000.0, 24.0, 70.0, 30.0, false, false,
                75.0, 85.0, -100.0, 0.0, 75.0, 85.0, 100.0, 0.0, 0.0, 0.0
            }
        };
        return presets;
    }

    void loadPreset(const Preset& p) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        syncEnabled = p.syncEnabled;
        analogMode = p.analogMode;
        // Left
        inputSourceL = p.inputSourceL;
        delayTimeMsL = p.delayTimeMsL;
        noteDivL = p.noteDivL;
        deviationCentsL = p.deviationCentsL;
        hpfHzL = p.hpfHzL;
        hpfsDbL = p.hpfsDbL;
        lpfHzL = p.lpfHzL;
        lpfsDbL = p.lpfsDbL;
        feedbackPctL = p.feedbackPctL;
        crossfeedPctLR = p.crossfeedPctLR;
        phaseInvFbL = p.phaseInvFbL;
        phaseInvCrossLR = p.phaseInvCrossLR;
        // Right
        inputSourceR = p.inputSourceR;
        delayTimeMsR = p.delayTimeMsR;
        noteDivR = p.noteDivR;
        deviationCentsR = p.deviationCentsR;
        hpfHzR = p.hpfHzR;
        hpfsDbR = p.hpfsDbR;
        lpfHzR = p.lpfHzR;
        lpfsDbR = p.lpfsDbR;
        feedbackPctR = p.feedbackPctR;
        crossfeedPctRL = p.crossfeedPctRL;
        phaseInvFbR = p.phaseInvFbR;
        phaseInvCrossRL = p.phaseInvCrossRL;
        // Output & Gain
        wetLevelPctL = p.wetLevelPctL;
        dryLevelPctL = p.dryLevelPctL;
        wetPanL = p.wetPanL;
        dryPanL = p.dryPanL;
        wetLevelPctR = p.wetLevelPctR;
        dryLevelPctR = p.dryLevelPctR;
        wetPanR = p.wetPanR;
        dryPanR = p.dryPanR;
        inGainDb = p.inGainDb;
        outGainDb = p.outGainDb;
        bpm = p.bpm;
        fxEnabled = p.fxEnabled;
        bypassed = p.bypassed;
        linked = p.linked;
        publishParamsLocked();
    }

    Preset getCurrentPreset(const char* name = "Custom") const noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        return makeCurrentPreset(name);
    }

private:
    Preset makeCurrentPreset(const char* name = "Custom") const noexcept {
        Preset p;
        p.name = name;
        p.description = "Current Settings";
        p.syncEnabled = syncEnabled;
        p.analogMode = analogMode;
        p.inputSourceL = inputSourceL;
        p.delayTimeMsL = delayTimeMsL;
        p.noteDivL = noteDivL;
        p.deviationCentsL = deviationCentsL;
        p.hpfHzL = hpfHzL;
        p.hpfsDbL = hpfsDbL;
        p.lpfHzL = lpfHzL;
        p.lpfsDbL = lpfsDbL;
        p.feedbackPctL = feedbackPctL;
        p.crossfeedPctLR = crossfeedPctLR;
        p.phaseInvFbL = phaseInvFbL;
        p.phaseInvCrossLR = phaseInvCrossLR;

        p.inputSourceR = inputSourceR;
        p.delayTimeMsR = delayTimeMsR;
        p.noteDivR = noteDivR;
        p.deviationCentsR = deviationCentsR;
        p.hpfHzR = hpfHzR;
        p.hpfsDbR = hpfsDbR;
        p.lpfHzR = lpfHzR;
        p.lpfsDbR = lpfsDbR;
        p.feedbackPctR = feedbackPctR;
        p.crossfeedPctRL = crossfeedPctRL;
        p.phaseInvFbR = phaseInvFbR;
        p.phaseInvCrossRL = phaseInvCrossRL;

        p.wetLevelPctL = wetLevelPctL;
        p.dryLevelPctL = dryLevelPctL;
        p.wetPanL = wetPanL;
        p.dryPanL = dryPanL;
        p.wetLevelPctR = wetLevelPctR;
        p.dryLevelPctR = dryLevelPctR;
        p.wetPanR = wetPanR;
        p.dryPanR = dryPanR;
        p.inGainDb = inGainDb;
        p.outGainDb = outGainDb;
        p.bpm = bpm;
        p.fxEnabled = fxEnabled;
        p.bypassed = bypassed;
        p.linked = linked;
        p.revision = controlRevision_;
        return p;
    }

public:
    // ── Left Channel Parameters ────────────────────────────────────────────
    void setInputSourceL(DelayInputSource src) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); inputSourceL = src; 
        publishParamsLocked();
    }
    DelayInputSource getInputSourceL() const noexcept { return getCurrentPreset().inputSourceL; }

    void setDelayTimeMsL(double ms) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        delayTimeMsL = std::clamp(ms, 1.0, 2500.0);
        if (linked) delayTimeMsR = delayTimeMsL;
    
        publishParamsLocked();
    }
    double getDelayTimeMsL() const noexcept { return getCurrentPreset().delayTimeMsL; }

    void setNoteDivisionL(NoteDivision div) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        noteDivL = div;
        if (linked) noteDivR = div;
    
        publishParamsLocked();
    }
    NoteDivision getNoteDivisionL() const noexcept { return getCurrentPreset().noteDivL; }

    void setDeviationCentsL(double cents) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        deviationCentsL = std::clamp(cents, -100.0, 100.0);
        if (linked) deviationCentsR = deviationCentsL;
    
        publishParamsLocked();
    }
    double getDeviationCentsL() const noexcept { return getCurrentPreset().deviationCentsL; }

    void divideTimeL() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        delayTimeMsL = std::max(1.0, delayTimeMsL * 0.5);
        if (linked) delayTimeMsR = delayTimeMsL;
    
        publishParamsLocked();
    }
    void multiplyTimeL() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        delayTimeMsL = std::min(2500.0, delayTimeMsL * 2.0);
        if (linked) delayTimeMsR = delayTimeMsL;
    
        publishParamsLocked();
    }

    void setHpfHzL(double hz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        hpfHzL = std::clamp(hz, 10.0, 2000.0);
        if (linked) hpfHzR = hpfHzL;

        publishParamsLocked();
    }
    double getHpfHzL() const noexcept { return getCurrentPreset().hpfHzL; }

    void setHpfsDbL(double slope) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        hpfsDbL = std::clamp(slope, 6.0, 48.0);
        if (linked) hpfsDbR = hpfsDbL;

        publishParamsLocked();
    }
    double getHpfsDbL() const noexcept { return getCurrentPreset().hpfsDbL; }

    void setLpfHzL(double hz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        lpfHzL = std::clamp(hz, 500.0, 22000.0);
        if (linked) lpfHzR = lpfHzL;

        publishParamsLocked();
    }
    double getLpfHzL() const noexcept { return getCurrentPreset().lpfHzL; }

    void setLpfsDbL(double slope) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        lpfsDbL = std::clamp(slope, 6.0, 48.0);
        if (linked) lpfsDbR = lpfsDbL;

        publishParamsLocked();
    }
    double getLpfsDbL() const noexcept { return getCurrentPreset().lpfsDbL; }

    void setFeedbackL(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        feedbackPctL = std::clamp(pct, 0.0, 110.0);
        if (linked) feedbackPctR = feedbackPctL;
    
        publishParamsLocked();
    }
    double getFeedbackL() const noexcept { return getCurrentPreset().feedbackPctL; }

    void setCrossfeedLR(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        crossfeedPctLR = std::clamp(pct, 0.0, 100.0);
        if (linked) crossfeedPctRL = crossfeedPctLR;
    
        publishParamsLocked();
    }
    double getCrossfeedLR() const noexcept { return getCurrentPreset().crossfeedPctLR; }

    void setPhaseInvertFeedbackL(bool inv) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); phaseInvFbL = inv; 
        publishParamsLocked();
    }
    bool getPhaseInvertFeedbackL() const noexcept { return getCurrentPreset().phaseInvFbL; }

    void setPhaseInvertCrossfeedLR(bool inv) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); phaseInvCrossLR = inv; 
        publishParamsLocked();
    }
    bool getPhaseInvertCrossfeedLR() const noexcept { return getCurrentPreset().phaseInvCrossLR; }

    // ── Right Channel Parameters ───────────────────────────────────────────
    void setInputSourceR(DelayInputSource src) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); inputSourceR = src; 
        publishParamsLocked();
    }
    DelayInputSource getInputSourceR() const noexcept { return getCurrentPreset().inputSourceR; }

    void setDelayTimeMsR(double ms) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        delayTimeMsR = std::clamp(ms, 1.0, 2500.0);
        if (linked) delayTimeMsL = delayTimeMsR;
    
        publishParamsLocked();
    }
    double getDelayTimeMsR() const noexcept { return getCurrentPreset().delayTimeMsR; }

    void setNoteDivisionR(NoteDivision div) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        noteDivR = div;
        if (linked) noteDivL = div;
    
        publishParamsLocked();
    }
    NoteDivision getNoteDivisionR() const noexcept { return getCurrentPreset().noteDivR; }

    void setDeviationCentsR(double cents) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        deviationCentsR = std::clamp(cents, -100.0, 100.0);
        if (linked) deviationCentsL = deviationCentsR;
    
        publishParamsLocked();
    }
    double getDeviationCentsR() const noexcept { return getCurrentPreset().deviationCentsR; }

    void divideTimeR() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        delayTimeMsR = std::max(1.0, delayTimeMsR * 0.5);
        if (linked) delayTimeMsL = delayTimeMsR;
    
        publishParamsLocked();
    }
    void multiplyTimeR() noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        delayTimeMsR = std::min(2500.0, delayTimeMsR * 2.0);
        if (linked) delayTimeMsL = delayTimeMsR;
    
        publishParamsLocked();
    }

    void setHpfHzR(double hz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        hpfHzR = std::clamp(hz, 10.0, 2000.0);
        if (linked) hpfHzL = hpfHzR;

        publishParamsLocked();
    }
    double getHpfHzR() const noexcept { return getCurrentPreset().hpfHzR; }

    void setHpfsDbR(double slope) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        hpfsDbR = std::clamp(slope, 6.0, 48.0);
        if (linked) hpfsDbL = hpfsDbR;

        publishParamsLocked();
    }
    double getHpfsDbR() const noexcept { return getCurrentPreset().hpfsDbR; }

    void setLpfHzR(double hz) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        lpfHzR = std::clamp(hz, 500.0, 22000.0);
        if (linked) lpfHzL = lpfHzR;

        publishParamsLocked();
    }
    double getLpfHzR() const noexcept { return getCurrentPreset().lpfHzR; }

    void setLpfsDbR(double slope) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        lpfsDbR = std::clamp(slope, 6.0, 48.0);
        if (linked) lpfsDbL = lpfsDbR;

        publishParamsLocked();
    }
    double getLpfsDbR() const noexcept { return getCurrentPreset().lpfsDbR; }

    void setFeedbackR(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        feedbackPctR = std::clamp(pct, 0.0, 110.0);
        if (linked) feedbackPctL = feedbackPctR;
    
        publishParamsLocked();
    }
    double getFeedbackR() const noexcept { return getCurrentPreset().feedbackPctR; }

    void setCrossfeedRL(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_);
        crossfeedPctRL = std::clamp(pct, 0.0, 100.0);
        if (linked) crossfeedPctLR = crossfeedPctRL;
    
        publishParamsLocked();
    }
    double getCrossfeedRL() const noexcept { return getCurrentPreset().crossfeedPctRL; }

    void setPhaseInvertFeedbackR(bool inv) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); phaseInvFbR = inv; 
        publishParamsLocked();
    }
    bool getPhaseInvertFeedbackR() const noexcept { return getCurrentPreset().phaseInvFbR; }

    void setPhaseInvertCrossfeedRL(bool inv) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); phaseInvCrossRL = inv; 
        publishParamsLocked();
    }
    bool getPhaseInvertCrossfeedRL() const noexcept { return getCurrentPreset().phaseInvCrossRL; }

    // ── Output Mixer Controls ──────────────────────────────────────────────
    void setWetLevelL(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); wetLevelPctL = std::clamp(pct, 0.0, 100.0); 
        publishParamsLocked();
    }
    double getWetLevelL() const noexcept { return getCurrentPreset().wetLevelPctL; }

    void setDryLevelL(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); dryLevelPctL = std::clamp(pct, 0.0, 100.0); 
        publishParamsLocked();
    }
    double getDryLevelL() const noexcept { return getCurrentPreset().dryLevelPctL; }

    void setWetPanL(double pan) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); wetPanL = std::clamp(pan, -100.0, 100.0); 
        publishParamsLocked();
    }
    double getWetPanL() const noexcept { return getCurrentPreset().wetPanL; }

    void setDryPanL(double pan) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); dryPanL = std::clamp(pan, -100.0, 100.0); 
        publishParamsLocked();
    }
    double getDryPanL() const noexcept { return getCurrentPreset().dryPanL; }

    void setWetLevelR(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); wetLevelPctR = std::clamp(pct, 0.0, 100.0); 
        publishParamsLocked();
    }
    double getWetLevelR() const noexcept { return getCurrentPreset().wetLevelPctR; }

    void setDryLevelR(double pct) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); dryLevelPctR = std::clamp(pct, 0.0, 100.0); 
        publishParamsLocked();
    }
    double getDryLevelR() const noexcept { return getCurrentPreset().dryLevelPctR; }

    void setWetPanR(double pan) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); wetPanR = std::clamp(pan, -100.0, 100.0); 
        publishParamsLocked();
    }
    double getWetPanR() const noexcept { return getCurrentPreset().wetPanR; }

    void setDryPanR(double pan) noexcept {
        std::lock_guard<std::mutex> lock(controlMutex_); dryPanR = std::clamp(pan, -100.0, 100.0); 
        publishParamsLocked();
    }
    double getDryPanR() const noexcept { return getCurrentPreset().dryPanR; }

    // ── Real-Time Meter Readouts ───────────────────────────────────────────
    float getMeterInL() const noexcept { return meterInL.load(std::memory_order_relaxed); }
    float getMeterInR() const noexcept { return meterInR.load(std::memory_order_relaxed); }
    float getMeterOutL() const noexcept { return meterOutL.load(std::memory_order_relaxed); }
    float getMeterOutR() const noexcept { return meterOutR.load(std::memory_order_relaxed); }

    // ── Audio Processing ───────────────────────────────────────────────────
    // ── Audio Processing ───────────────────────────────────────────────────
    void processStereo(double& inOutL, double& inOutR) noexcept {
        Preset latest{};
        if (publishedParams_.load(latest) && latest.revision != audioParams_.revision)
            audioParams_ = latest;
        if (audioParams_.revision != audioRevision_ ||
            std::abs(sampleRate - audioFilterSampleRate_) > 1.0) {
            updateFilters(audioParams_);
            audioRevision_ = audioParams_.revision;
        }
        const auto& p = audioParams_;
        if (p.bypassed || !p.fxEnabled) return;

        // Apply Input Gain (-24 dB to +24 dB)
        const double inGainLin = std::pow(10.0, p.inGainDb * 0.05);
        const double dryInL = inOutL * inGainLin;
        const double dryInR = inOutR * inGainLin;

        // Input Clip Hold Detection (> 0 dBFS)
        if (std::abs(dryInL) >= 1.0 || std::abs(dryInR) >= 1.0) {
            clipOccurredIn.store(true, std::memory_order_relaxed);
        }

        // 1. Input Routing
        double srcInL = 0.0;
        switch (p.inputSourceL) {
            case DelayInputSource::Left:  srcInL = dryInL; break;
            case DelayInputSource::Right: srcInL = dryInR; break;
            case DelayInputSource::Sum:   srcInL = (dryInL + dryInR) * 0.7071; break;
            case DelayInputSource::Mute:  srcInL = 0.0; break;
        }

        double srcInR = 0.0;
        switch (p.inputSourceR) {
            case DelayInputSource::Left:  srcInR = dryInL; break;
            case DelayInputSource::Right: srcInR = dryInR; break;
            case DelayInputSource::Sum:   srcInR = (dryInL + dryInR) * 0.7071; break;
            case DelayInputSource::Mute:  srcInR = 0.0; break;
        }

        // 2. Compute Effective Delay Times (Free ms vs BPM Sync + Deviation Pitch Shift)
        double effTimeMsL = p.delayTimeMsL;
        double effTimeMsR = p.delayTimeMsR;

        if (p.syncEnabled) {
            const double msPerBeat = 60000.0 / std::max(20.0, p.bpm);
            effTimeMsL = msPerBeat * getNoteBeats(p.noteDivL);
            effTimeMsR = msPerBeat * getNoteBeats(p.noteDivR);
        }

        // Micro-timing Deviation in cents: timeFactor = 2^(-cents / 1200)
        if (std::abs(p.deviationCentsL) > 0.01) {
            effTimeMsL *= std::pow(2.0, -p.deviationCentsL / 1200.0);
        }
        if (std::abs(p.deviationCentsR) > 0.01) {
            effTimeMsR *= std::pow(2.0, -p.deviationCentsR / 1200.0);
        }

        effTimeMsL = std::clamp(effTimeMsL, 1.0, 2500.0);
        effTimeMsR = std::clamp(effTimeMsR, 1.0, 2500.0);

        // Wow & Flutter modulation for Tape / Tube modes
        double modOffsetL = 0.0;
        double modOffsetR = 0.0;
        if (p.analogMode == DelayAnalogMode::WarmTape || p.analogMode == DelayAnalogMode::LoFi) {
            wowFlutterPhase += 2.0 * std::numbers::pi * 1.8 / sampleRate;
            if (wowFlutterPhase >= 2.0 * std::numbers::pi) wowFlutterPhase -= 2.0 * std::numbers::pi;
            const double modAmt = (p.analogMode == DelayAnalogMode::LoFi) ? 1.5 : 0.6; // ms
            modOffsetL = std::sin(wowFlutterPhase) * modAmt;
            modOffsetR = std::cos(wowFlutterPhase * 0.73) * modAmt;
        }

        const double delaySampL = std::clamp((effTimeMsL + modOffsetL) * 0.001 * sampleRate, 2.0, static_cast<double>(kMaxDelaySamples - 4));
        const double delaySampR = std::clamp((effTimeMsR + modOffsetR) * 0.001 * sampleRate, 2.0, static_cast<double>(kMaxDelaySamples - 4));

        // 3. Hermite 4-point Interpolated Delay Read
        auto readCubic = [](const std::vector<double>& buf, int wPos, double dSamp) noexcept -> double {
            const int dInt = static_cast<int>(dSamp);
            const double frac = dSamp - static_cast<double>(dInt);
            const int sz = static_cast<int>(buf.size());

            const int p0 = (wPos - dInt + 1 + sz) % sz;
            const int p1 = (wPos - dInt + sz) % sz;
            const int p2 = (wPos - dInt - 1 + sz) % sz;
            const int p3 = (wPos - dInt - 2 + sz) % sz;

            const double y0 = buf[static_cast<size_t>(p0)];
            const double y1 = buf[static_cast<size_t>(p1)];
            const double y2 = buf[static_cast<size_t>(p2)];
            const double y3 = buf[static_cast<size_t>(p3)];

            // 4-point, 3rd-order Hermite interpolation
            const double c0 = y1;
            const double c1 = 0.5 * (y2 - y0);
            const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
            const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

            return ((c3 * frac + c2) * frac + c1) * frac + c0;
        };

        double delayedL = readCubic(delayBufferL, writePos, delaySampL);
        double delayedR = readCubic(delayBufferR, writePos, delaySampR);

        // 4. Analog Saturation on Delayed Tap
        if (p.analogMode == DelayAnalogMode::WarmTape) {
            delayedL = std::tanh(delayedL * 1.15) * 0.95;
            delayedR = std::tanh(delayedR * 1.15) * 0.95;
        } else if (p.analogMode == DelayAnalogMode::VintageTube) {
            auto tubeSat = [](double x) noexcept -> double {
                return (x > 0.0) ? (1.0 - std::exp(-x * 1.2)) : (-1.0 + std::exp(x * 0.9));
            };
            delayedL = tubeSat(delayedL);
            delayedR = tubeSat(delayedR);
        } else if (p.analogMode == DelayAnalogMode::LoFi) {
            delayedL = std::clamp(std::round(delayedL * 128.0) / 128.0, -1.0, 1.0);
            delayedR = std::clamp(std::round(delayedR * 128.0) / 128.0, -1.0, 1.0);
        }

        // 5. Multi-Slope Filter Cascade (HPF + LPF)
        for (size_t i = 0; i < numHpfStagesL; ++i) delayedL = hpfStagesL[i].process(delayedL);
        for (size_t i = 0; i < numLpfStagesL; ++i) delayedL = lpfStagesL[i].process(delayedL);

        for (size_t i = 0; i < numHpfStagesR; ++i) delayedR = hpfStagesR[i].process(delayedR);
        for (size_t i = 0; i < numLpfStagesR; ++i) delayedR = lpfStagesR[i].process(delayedR);

        // 6. Feedback & Crossfeed Accumulation
        const double fbSignL = p.phaseInvFbL ? -1.0 : 1.0;
        const double fbSignR = p.phaseInvFbR ? -1.0 : 1.0;
        const double crossSignLR = p.phaseInvCrossLR ? -1.0 : 1.0;
        const double crossSignRL = p.phaseInvCrossRL ? -1.0 : 1.0;

        const double fbGainL = (p.feedbackPctL * 0.01) * fbSignL;
        const double fbGainR = (p.feedbackPctR * 0.01) * fbSignR;
        const double crossGainLR = (p.crossfeedPctLR * 0.01) * crossSignLR;
        const double crossGainRL = (p.crossfeedPctRL * 0.01) * crossSignRL;

        const double nextBufInL = srcInL + (delayedL * fbGainL) + (delayedR * crossGainRL);
        const double nextBufInR = srcInR + (delayedR * fbGainR) + (delayedL * crossGainLR);

        delayBufferL[static_cast<size_t>(writePos)] = sanitize(nextBufInL);
        delayBufferR[static_cast<size_t>(writePos)] = sanitize(nextBufInR);

        writePos = (writePos + 1) % kMaxDelaySamples;

        // 7. Stereo Output Mixer Matrix (Wet/Dry and Wet Pan / Dry Pan)
        auto panGains = [](double panVal, double& gL, double& gR) noexcept {
            const double norm = std::clamp((panVal + 100.0) * 0.005, 0.0, 1.0);
            constexpr double halfPi = std::numbers::pi * 0.5;
            gL = std::cos(norm * halfPi);
            gR = std::sin(norm * halfPi);
        };

        double dryPanLL = 1.0, dryPanLR = 0.0;
        double dryPanRL = 0.0, dryPanRR = 1.0;
        double wetPanLL = 1.0, wetPanLR = 0.0;
        double wetPanRL = 0.0, wetPanRR = 1.0;

        panGains(p.dryPanL, dryPanLL, dryPanLR);
        panGains(p.dryPanR, dryPanRL, dryPanRR);
        panGains(p.wetPanL, wetPanLL, wetPanLR);
        panGains(p.wetPanR, wetPanRL, wetPanRR);

        const double dryL = dryInL * (p.dryLevelPctL * 0.01);
        const double dryR = dryInR * (p.dryLevelPctR * 0.01);
        const double wetL = delayedL * (p.wetLevelPctL * 0.01);
        const double wetR = delayedR * (p.wetLevelPctR * 0.01);

        const double mixOutL = (dryL * dryPanLL + dryR * dryPanRL) + (wetL * wetPanLL + wetR * wetPanRL);
        const double mixOutR = (dryL * dryPanLR + dryR * dryPanRR) + (wetL * wetPanLR + wetR * wetPanRR);

        // Apply Output Gain (-24 dB to +24 dB)
        const double outGainLin = std::pow(10.0, p.outGainDb * 0.05);
        inOutL = sanitize(mixOutL * outGainLin);
        inOutR = sanitize(mixOutR * outGainLin);

        // Output Clip Hold Detection (> 0 dBFS)
        if (std::abs(inOutL) >= 1.0 || std::abs(inOutR) >= 1.0) {
            clipOccurredOut.store(true, std::memory_order_relaxed);
        }

        // 8. Update Atomic Peak Meters & Peak Hold
        const float inPeakLVal = static_cast<float>(std::abs(dryInL));
        const float inPeakRVal = static_cast<float>(std::abs(dryInR));
        const float outPeakLVal = static_cast<float>(std::abs(inOutL));
        const float outPeakRVal = static_cast<float>(std::abs(inOutR));

        float pInL = meterInL.load(std::memory_order_relaxed);
        meterInL.store(std::max(inPeakLVal, pInL * 0.999f), std::memory_order_relaxed);
        float pInR = meterInR.load(std::memory_order_relaxed);
        meterInR.store(std::max(inPeakRVal, pInR * 0.999f), std::memory_order_relaxed);
        float pOutL = meterOutL.load(std::memory_order_relaxed);
        meterOutL.store(std::max(outPeakLVal, pOutL * 0.999f), std::memory_order_relaxed);
        float pOutR = meterOutR.load(std::memory_order_relaxed);
        meterOutR.store(std::max(outPeakRVal, pOutR * 0.999f), std::memory_order_relaxed);

        const float maxInNow = std::max(inPeakLVal, inPeakRVal);
        float phIn = peakHoldInVal.load(std::memory_order_relaxed);
        if (maxInNow > phIn) peakHoldInVal.store(maxInNow, std::memory_order_relaxed);
        else peakHoldInVal.store(phIn * 0.9999f, std::memory_order_relaxed);

        const float maxOutNow = std::max(outPeakLVal, outPeakRVal);
        float phOut = peakHoldOutVal.load(std::memory_order_relaxed);
        if (maxOutNow > phOut) peakHoldOutVal.store(maxOutNow, std::memory_order_relaxed);
        else peakHoldOutVal.store(phOut * 0.9999f, std::memory_order_relaxed);

        // 9. Stereo Correlation (-1.0 to +1.0) & L/R Balance
        corrDotSum = corrDotSum * 0.998 + (inOutL * inOutR) * 0.002;
        corrEnergyL = corrEnergyL * 0.998 + (inOutL * inOutL) * 0.002;
        corrEnergyR = corrEnergyR * 0.998 + (inOutR * inOutR) * 0.002;
        const double denom = std::sqrt(corrEnergyL * corrEnergyR) + 1e-9;
        const float corr = static_cast<float>(std::clamp(corrDotSum / denom, -1.0, 1.0));
        meterCorrelation.store(corr, std::memory_order_relaxed);

        const double absL = std::abs(inOutL);
        const double absR = std::abs(inOutR);
        const float bal = static_cast<float>(std::clamp((absR - absL) / (absR + absL + 1e-6), -1.0, 1.0));
        float prevBal = meterBalance.load(std::memory_order_relaxed);
        meterBalance.store(prevBal * 0.95f + bal * 0.05f, std::memory_order_relaxed);
    }

private:
    struct BiquadSection {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;

        void reset() noexcept {
            x1 = x2 = y1 = y2 = 0.0;
        }

        double process(double in) noexcept {
            const double out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = in;
            y2 = y1; y1 = sanitize(out);
            return y1;
        }

        void setCoeffs(double b0_, double b1_, double b2_, double a1_, double a2_) noexcept {
            b0 = b0_; b1 = b1_; b2 = b2_; a1 = a1_; a2 = a2_;
        }
    };

    void updateFilters(const Preset& p) noexcept {
        auto designHPF = [this](double freqHz, double q, BiquadSection& s) {
            const double w0 = 2.0 * std::numbers::pi * std::clamp(freqHz, 10.0, sampleRate * 0.45) / sampleRate;
            const double alpha = std::sin(w0) / (2.0 * q);
            const double cosw0 = std::cos(w0);
            const double a0 = 1.0 + alpha;
            s.setCoeffs(((1.0 + cosw0) * 0.5) / a0,
                        (-(1.0 + cosw0)) / a0,
                        ((1.0 + cosw0) * 0.5) / a0,
                        (-2.0 * cosw0) / a0,
                        (1.0 - alpha) / a0);
        };

        auto designLPF = [this](double freqHz, double q, BiquadSection& s) {
            const double w0 = 2.0 * std::numbers::pi * std::clamp(freqHz, 100.0, sampleRate * 0.45) / sampleRate;
            const double alpha = std::sin(w0) / (2.0 * q);
            const double cosw0 = std::cos(w0);
            const double a0 = 1.0 + alpha;
            s.setCoeffs(((1.0 - cosw0) * 0.5) / a0,
                        (1.0 - cosw0) / a0,
                        ((1.0 - cosw0) * 0.5) / a0,
                        (-2.0 * cosw0) / a0,
                        (1.0 - alpha) / a0);
        };

        // Determine number of cascaded stages from dB/oct slope (1 stage = 12 dB/oct, 2 = 24 dB/oct, 3 = 36, 4 = 48)
        numHpfStagesL = std::clamp(static_cast<size_t>(std::round(p.hpfsDbL / 12.0)), size_t{1}, kMaxFilterStages);
        numHpfStagesR = std::clamp(static_cast<size_t>(std::round(p.hpfsDbR / 12.0)), size_t{1}, kMaxFilterStages);
        numLpfStagesL = std::clamp(static_cast<size_t>(std::round(p.lpfsDbL / 12.0)), size_t{1}, kMaxFilterStages);
        numLpfStagesR = std::clamp(static_cast<size_t>(std::round(p.lpfsDbR / 12.0)), size_t{1}, kMaxFilterStages);

        for (size_t i = 0; i < kMaxFilterStages; ++i) {
            designHPF(p.hpfHzL, 0.7071, hpfStagesL[i]);
            designHPF(p.hpfHzR, 0.7071, hpfStagesR[i]);
            designLPF(p.lpfHzL, 0.7071, lpfStagesL[i]);
            designLPF(p.lpfHzR, 0.7071, lpfStagesR[i]);
        }
        audioFilterSampleRate_ = sampleRate;
    }
    void publishParamsLocked() noexcept {
        ++controlRevision_;
        auto snapshot = makeCurrentPreset();
        snapshot.revision = controlRevision_;
        publishedParams_.publish(snapshot);
    }

    mutable std::mutex controlMutex_;
    uint64_t controlRevision_ = 1;
    AtomicParameterSnapshot<Preset> publishedParams_;
    Preset audioParams_{};
    uint64_t audioRevision_ = 0;
    double audioFilterSampleRate_ = 0.0;

    double sampleRate = 48000.0;
    double bpm = 120.0;
    bool syncEnabled = true;
    bool linked = false;
    DelayAnalogMode analogMode = DelayAnalogMode::CleanDigital;
    bool fxEnabled = true;
    bool bypassed = false;

    // Left Engine Parameters
    DelayInputSource inputSourceL = DelayInputSource::Left;
    double delayTimeMsL = 131.0;
    NoteDivision noteDivL = NoteDivision::Div_1_16;
    double deviationCentsL = 79.3;
    double hpfHzL = 72.7;
    double hpfsDbL = 21.3;
    double lpfHzL = 5800.0;
    double lpfsDbL = 47.0;
    double feedbackPctL = 55.0;
    double crossfeedPctLR = 29.0;
    bool phaseInvFbL = false;
    bool phaseInvCrossLR = false;

    // Right Engine Parameters
    DelayInputSource inputSourceR = DelayInputSource::Right;
    double delayTimeMsR = 349.0;
    NoteDivision noteDivR = NoteDivision::Div_1_4T;
    double deviationCentsR = 81.5;
    double hpfHzR = 33.1;
    double hpfsDbR = 23.2;
    double lpfHzR = 4460.0;
    double lpfsDbR = 37.1;
    double feedbackPctR = 40.0;
    double crossfeedPctRL = 19.0;
    bool phaseInvFbR = false;
    bool phaseInvCrossRL = false;

    // Output Mixer Parameters
    double wetLevelPctL = 100.0;
    double dryLevelPctL = 51.0;
    double wetPanL = -100.0; // L100
    double dryPanL = -42.0;  // L42

    double wetLevelPctR = 100.0;
    double dryLevelPctR = 49.0;
    double wetPanR = 100.0;  // R100
    double dryPanR = 42.0;   // R42

    // Buffers & States
    std::vector<double> delayBufferL;
    std::vector<double> delayBufferR;
    int writePos = 0;
    double prevFbSampleL = 0.0;
    double prevFbSampleR = 0.0;
    double wowFlutterPhase = 0.0;

    size_t numHpfStagesL = 2;
    size_t numHpfStagesR = 2;
    size_t numLpfStagesL = 4;
    size_t numLpfStagesR = 3;
    std::array<BiquadSection, kMaxFilterStages> hpfStagesL;
    std::array<BiquadSection, kMaxFilterStages> hpfStagesR;
    std::array<BiquadSection, kMaxFilterStages> lpfStagesL;
    std::array<BiquadSection, kMaxFilterStages> lpfStagesR;

    // Real-Time Atomic Meters & Telemetry
    std::atomic<float> meterInL{ 0.0f };
    std::atomic<float> meterInR{ 0.0f };
    std::atomic<float> meterOutL{ 0.0f };
    std::atomic<float> meterOutR{ 0.0f };
    std::atomic<float> meterCorrelation{ 1.0f };
    std::atomic<float> meterBalance{ 0.0f };
    std::atomic<bool>  clipOccurredIn{ false };
    std::atomic<bool>  clipOccurredOut{ false };
    std::atomic<float> peakHoldInVal{ 0.0f };
    std::atomic<float> peakHoldOutVal{ 0.0f };

    double inGainDb = 0.0;
    double outGainDb = 0.0;
    double corrDotSum = 0.0;
    double corrEnergyL = 0.0;
    double corrEnergyR = 0.0;
};

} // namespace nova::graviton

