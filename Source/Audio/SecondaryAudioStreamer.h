#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Mixer/MixerEngine.h"
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

namespace nova {

/**
 * @brief SecondaryAudioStreamer manages a dedicated secondary audio device (e.g. WASAPI 2 / Bluetooth)
 *        for independent Monitor / IEM / Wedge output streaming.
 *
 * Runs lock-free in Windows WASAPI Shared Mode concurrently with the primary audio device (WASAPI 1 / Master).
 * Pulls from MixerEngine's lock-free monitor FIFO and handles sample-rate conversion (via Lagrange Interpolator)
 * so that devices running at different rates (e.g., 48 kHz engine vs 44.1 kHz Bluetooth headphones) stream glitch-free.
 */
class SecondaryAudioStreamer final : public juce::AudioIODeviceCallback {
public:
    explicit SecondaryAudioStreamer(MixerEngine& eng) : engine(eng) {}

    ~SecondaryAudioStreamer() override {
        closeDevice();
    }

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    /**
     * @brief Returns list of available output device names for WASAPI / Windows Audio.
     */
    juce::StringArray getAvailableOutputDeviceNames() {
        juce::StringArray names;
        if (deviceManager.getAvailableDeviceTypes().isEmpty()) {
            deviceManager.initialise(0, 2, nullptr, true);
        }

        for (auto* type : deviceManager.getAvailableDeviceTypes()) {
            if (type != nullptr) {
                type->scanForDevices();
                const auto outNames = type->getDeviceNames(false); // false = output devices
                for (const auto& n : outNames) {
                    if (!names.contains(n))
                        names.add(n);
                }
            }
        }
        return names;
    }

    /**
     * @brief Open a secondary WASAPI device specifically for Monitor output.
     * Follows UnderFusion/AudioHQ architecture: prefers standard Windows Audio (WASAPI Shared Mode
     * with native ~10 ms period and AUTOCONVERTPCM) so audiodg.exe does not suffer low-latency
     * period contention across Master (WASAPI 1) and Monitor (WASAPI 2 / Bluetooth).
     */
    bool openDevice(const juce::String& outputDeviceName, double targetSampleRate = 0.0, int bufferSize = 480) {
        closeDevice();

        if (deviceManager.getAvailableDeviceTypes().isEmpty()) {
            deviceManager.initialise(0, 2, nullptr, true);
        }

        auto tryOpenWithType = [&](const juce::String& typeName) -> bool {
            bool hasType = false;
            for (auto* t : deviceManager.getAvailableDeviceTypes()) {
                if (t != nullptr && t->getTypeName() == typeName) {
                    hasType = true;
                    break;
                }
            }
            if (!hasType)
                return false;

            deviceManager.setCurrentAudioDeviceType(typeName, true);

            juce::AudioDeviceManager::AudioDeviceSetup setup;
            deviceManager.getAudioDeviceSetup(setup);
            setup.outputDeviceName = outputDeviceName;
            setup.inputDeviceName = ""; // Output only for monitor
            setup.useDefaultInputChannels = false;
            setup.useDefaultOutputChannels = true;
            if (targetSampleRate > 1000.0)
                setup.sampleRate = targetSampleRate;
            if (bufferSize >= 128)
                setup.bufferSize = bufferSize;

            const auto err = deviceManager.setAudioDeviceSetup(setup, true);
            if (err.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
                return false;

            if (auto* dev = deviceManager.getCurrentAudioDevice()) {
                const auto sizes = dev->getAvailableBufferSizes();
                const double sr = (dev->getCurrentSampleRate() > 1000.0) ? dev->getCurrentSampleRate() : 48000.0;
                const int desiredBuf = (sr >= 88200.0) ? std::max(bufferSize, 960) : std::max(bufferSize, 480);
                const int minSafe = (sr >= 88200.0) ? 480 : 384;
                const int maxSafe = (sr >= 88200.0) ? 2048 : 1024;
                int bestBuf = 0;
                if (sizes.contains(desiredBuf)) {
                    bestBuf = desiredBuf;
                } else {
                    int bestDist = 999999;
                    int largestAny = 0;
                    for (int s : sizes) {
                        if (s >= minSafe && s <= maxSafe) {
                            const int dist = std::abs(s - desiredBuf);
                            if (bestBuf == 0 || dist < bestDist || (dist == bestDist && s > bestBuf)) {
                                bestBuf = s;
                                bestDist = dist;
                            }
                        }
                        if (s > largestAny)
                            largestAny = s;
                    }
                    if (bestBuf == 0)
                        bestBuf = largestAny;
                }
                auto curSetup = deviceManager.getAudioDeviceSetup();
                if (bestBuf > 0 && curSetup.bufferSize != bestBuf) {
                    curSetup.bufferSize = bestBuf;
                    deviceManager.setAudioDeviceSetup(curSetup, true);
                }
            }
            return true;
        };

        if (tryOpenWithType("Windows Audio") ||
            tryOpenWithType("Windows Audio (Low Latency Mode)")) {
            engine.setMonitorConsumerActive(true);
            deviceManager.addAudioCallback(this);
            deviceActive.store(true, std::memory_order_relaxed);
            activeDeviceName = outputDeviceName;
            return true;
        }

        engine.setMonitorConsumerActive(false);
        activeDeviceName.clear();
        deviceActive.store(false, std::memory_order_relaxed);
        return false;
    }

    /**
     * @brief Close the secondary audio device stream.
     */
    void closeDevice() {
        deviceActive.store(false, std::memory_order_relaxed);
        deviceManager.removeAudioCallback(this);
        deviceManager.closeAudioDevice();
        engine.setMonitorConsumerActive(false);
        activeDeviceName.clear();
    }

    bool isDeviceOpen() const noexcept {
        return deviceActive.load(std::memory_order_relaxed);
    }

    juce::String getCurrentDeviceName() const {
        return activeDeviceName;
    }

    double getCurrentSampleRate() const noexcept {
        return currentSampleRate;
    }

    int getCurrentBufferSize() const noexcept {
        return currentBufferSize;
    }

    void setEnabled(bool enable) noexcept {
        enabled.store(enable, std::memory_order_relaxed);
    }

    bool isEnabled() const noexcept {
        return enabled.load(std::memory_order_relaxed);
    }

    int getFifoQueueDepth() const noexcept { return engine.getMonitorFifoQueueDepth(); }
    float getFifoLatencyMs() const noexcept { return engine.getMonitorFifoLatencyMs(); }
    uint32_t getFifoOverruns() const noexcept { return engine.getMonitorFifoOverruns(); }
    uint32_t getFifoUnderruns() const noexcept { return engine.getMonitorFifoUnderruns(); }
    void resetFifoStats() noexcept { engine.resetMonitorFifoStats(); }

    // =========================================================================
    // AudioHQ EngineTunables Constants (UnderFusion/AudioHQ reference)
    // =========================================================================
    static constexpr double kTargetBacklogMarginMs = 15.0;     // Trough margin above 1 pull so bursty endpoints never starve
    static constexpr double kResyncMarginMs = 120.0;           // Hard resync ceiling above target trough for stalls
    static constexpr double kResamplerMaxCorrection = 0.005;   // +/- 0.5% (~8 cents, inaudible)
    static constexpr double kResamplerGain = 0.5;              // Proportional gain per second of trough error
    static constexpr double kResamplerTroughSmoothing = 0.3;   // EMA factor applied to trough each 200 ms control tick

    // =========================================================================
    // juce::AudioIODeviceCallback Implementation
    // =========================================================================

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        engine.setMonitorConsumerActive(true);
        // Drain any old queued samples from consumer side (thread-safe SPSC without cross-thread reset())
        const int stale = engine.getMonitorFifoReady();
        if (stale > 0)
            engine.discardMonitorSamples(stale);
        (void)engine.consumeMonitorFifoResetRequest();

        wasUnderflowing = false;
        monitorPrimed = false;
        consecutiveEmptyCallbacks = 0;
        lastOutSampleL = 0.0f;
        lastOutSampleR = 0.0f;
        smoothedSpeedRatio = 1.0;
        currentCorrection = 0.0;
        windowMinFillSeconds = 1.0e12;
        windowFrames = 0;
        smoothedTroughSeconds = 0.030;
        if (device != nullptr) {
            currentSampleRate = device->getCurrentSampleRate();
            currentBufferSize = device->getCurrentBufferSizeSamples();
            const double engSrInit = engine.getSampleRate();
            if (engSrInit > 1000.0 && currentSampleRate > 1000.0)
                smoothedSpeedRatio = engSrInit / currentSampleRate;
            const int maxNeeded = static_cast<int>(std::ceil(std::max(4096, currentBufferSize) * 4.0)) + 512;
            readBufferL.assign(static_cast<size_t>(maxNeeded), 0.0f);
            readBufferR.assign(static_cast<size_t>(maxNeeded), 0.0f);
            resampleBufferL.assign(static_cast<size_t>(maxNeeded), 0.0f);
            resampleBufferR.assign(static_cast<size_t>(maxNeeded), 0.0f);
            interpolatorL.reset();
            interpolatorR.reset();
        }
    }

    void audioDeviceStopped() override {
        engine.setMonitorConsumerActive(false);
        wasUnderflowing = false;
        monitorPrimed = false;
        consecutiveEmptyCallbacks = 0;
        lastOutSampleL = 0.0f;
        lastOutSampleR = 0.0f;
        smoothedSpeedRatio = 1.0;
        currentCorrection = 0.0;
        windowMinFillSeconds = 1.0e12;
        windowFrames = 0;
        interpolatorL.reset();
        interpolatorR.reset();
    }

    void audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                          int /*numInputChannels*/,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& /*context*/) override {
        if (outputChannelData == nullptr || numOutputChannels <= 0 || numSamples <= 0)
            return;

        float* outL = (numOutputChannels > 0) ? outputChannelData[0] : nullptr;
        float* outR = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

        if (!enabled.load(std::memory_order_relaxed)) {
            monitorPrimed = false;
            lastOutSampleL = 0.0f;
            lastOutSampleR = 0.0f;
            if (outL != nullptr) std::fill_n(outL, numSamples, 0.0f);
            if (outR != nullptr) std::fill_n(outR, numSamples, 0.0f);
            return;
        }

        const double engineSr = engine.getSampleRate();
        const double secSr = (currentSampleRate > 1000.0) ? currentSampleRate : (engineSr > 1000.0 ? engineSr : 48000.0);
        const double validEngineSr = (engineSr > 1000.0) ? engineSr : secSr;
        const double nominalSpeedRatio = validEngineSr / secSr;

        const int engBuf = std::max(64, engine.blockSize.load(std::memory_order_relaxed));
        const double engPeriodSec = static_cast<double>(engBuf) / validEngineSr;
        const double secPeriodSec = static_cast<double>(numSamples) / secSr;
        const double maxPullPeriodSec = std::max(engPeriodSec, secPeriodSec);

        // AudioHQ Trough Target: keep the MINIMUM backlog (trough) across each 200ms window
        // safely above 1 full pull + TargetBacklogMarginMs (15ms) so bursty Bluetooth/WASAPI
        // endpoints never starve at the bottom of the sawtooth.
        const double targetTroughSeconds = std::clamp(maxPullPeriodSec + (kTargetBacklogMarginMs * 0.001), 0.025, 0.065);
        const double prefillTargetSeconds = targetTroughSeconds + maxPullPeriodSec;
        const double maxBacklogSeconds = prefillTargetSeconds + (kResyncMarginMs * 0.001);

        int queuedSamples = engine.getMonitorFifoReady();
        bool trimmedOverflow = false;

        // Handle explicit UI / producer reset requests on the consumer thread safely
        if (engine.consumeMonitorFifoResetRequest()) {
            const int targetKeep = static_cast<int>(std::ceil(prefillTargetSeconds * validEngineSr));
            if (queuedSamples > targetKeep) {
                engine.discardMonitorSamples(queuedSamples - targetKeep);
                queuedSamples = engine.getMonitorFifoReady();
                trimmedOverflow = true;
            }
            smoothedTroughSeconds = targetTroughSeconds;
            windowMinFillSeconds = 1.0e12;
            windowFrames = 0;
            currentCorrection = 0.0;
        }

        const double fillSeconds = static_cast<double>(queuedSamples) / validEngineSr;

        // Safety-net backlog resync (AudioHQ OutputChannel.Write _maxBacklog check):
        // Only fires if a true device stall pushed backlog past maxBacklogSeconds (~165 ms).
        if (fillSeconds > maxBacklogSeconds) {
            const int targetKeep = static_cast<int>(std::ceil(prefillTargetSeconds * validEngineSr));
            if (queuedSamples > targetKeep) {
                const int discarded = engine.discardMonitorSamples(queuedSamples - targetKeep);
                queuedSamples = engine.getMonitorFifoReady();
                trimmedOverflow = (discarded > 0);
            }
            smoothedTroughSeconds = targetTroughSeconds;
            windowMinFillSeconds = static_cast<double>(queuedSamples) / validEngineSr;
            windowFrames = 0;
        }

        // Pre-fill watermark on initial start or after complete stream stop
        if (!monitorPrimed) {
            smoothedSpeedRatio = nominalSpeedRatio;
            currentCorrection = 0.0;
            smoothedTroughSeconds = targetTroughSeconds;
            windowMinFillSeconds = 1.0e12;
            windowFrames = 0;
            const int prefillSamples = static_cast<int>(std::ceil(prefillTargetSeconds * validEngineSr));
            if (queuedSamples < prefillSamples) {
                for (int i = 0; i < numSamples; ++i) {
                    lastOutSampleL *= 0.92f;
                    lastOutSampleR *= 0.92f;
                    if (outL != nullptr) outL[i] = lastOutSampleL;
                    if (outR != nullptr) outR[i] = lastOutSampleR;
                }
                if (std::abs(lastOutSampleL) < 1.0e-7f) lastOutSampleL = 0.0f;
                if (std::abs(lastOutSampleR) < 1.0e-7f) lastOutSampleR = 0.0f;
                return;
            }
            monitorPrimed = true;
            wasUnderflowing = true;
            consecutiveEmptyCallbacks = 0;
        }

        // Track minimum backlog ("trough") across the 200 ms control window (AudioHQ AdaptiveResampler)
        const double currentFillSec = static_cast<double>(queuedSamples) / validEngineSr;
        if (currentFillSec < windowMinFillSeconds)
            windowMinFillSeconds = currentFillSec;

        windowFrames += numSamples;
        const long controlFrames = std::max<long>(1, static_cast<long>(secSr / 5.0)); // ~5 ticks/sec (200 ms)
        if (windowFrames >= controlFrames) {
            if (windowMinFillSeconds < 1.0e9) {
                smoothedTroughSeconds += kResamplerTroughSmoothing * (windowMinFillSeconds - smoothedTroughSeconds);
                const double errorSeconds = smoothedTroughSeconds - targetTroughSeconds;
                currentCorrection = std::clamp(errorSeconds * kResamplerGain,
                                               -kResamplerMaxCorrection,
                                               kResamplerMaxCorrection);
            }
            windowMinFillSeconds = 1.0e12;
            windowFrames = 0;
        }

        const double targetRatio = nominalSpeedRatio * (1.0 + currentCorrection);
        if (std::abs(nominalSpeedRatio - smoothedSpeedRatio) > 0.05) {
            smoothedSpeedRatio = targetRatio;
        } else {
            smoothedSpeedRatio += 0.05 * (targetRatio - smoothedSpeedRatio);
        }
        const double speedRatio = std::clamp(smoothedSpeedRatio, 0.25, 4.0);

        // Required input samples for Lagrange 4-point cubic polynomial interpolator
        const int neededEngineSamples = static_cast<int>(std::ceil(numSamples * speedRatio)) + 8;
        const int safeNeeded = std::min(neededEngineSamples, static_cast<int>(readBufferL.size()));
        const int available = engine.peekMonitorSamples(readBufferL.data(), readBufferR.data(), safeNeeded);

        float* dstL = (outL != nullptr) ? outL : resampleBufferL.data();
        float* dstR = (outR != nullptr) ? outR : resampleBufferR.data();
        const bool dstEnough = (static_cast<int>(resampleBufferL.size()) >= numSamples);
        if ((outL == nullptr || outR == nullptr) && !dstEnough) {
            if (outL != nullptr) std::fill_n(outL, numSamples, 0.0f);
            if (outR != nullptr) std::fill_n(outR, numSamples, 0.0f);
            return;
        }

        if (available >= neededEngineSamples && safeNeeded >= neededEngineSamples) {
            consecutiveEmptyCallbacks = 0;
            const int usedL = interpolatorL.process(speedRatio, readBufferL.data(), dstL, numSamples);
            const int usedR = interpolatorR.process(speedRatio, readBufferR.data(), dstR, numSamples);
            const int consumed = std::min(usedL, usedR);
            engine.advanceMonitorRead(consumed);

            if (wasUnderflowing) {
                wasUnderflowing = false;
                const int rampLen = std::min(64, numSamples);
                for (int i = 0; i < rampLen; ++i) {
                    const float alpha = static_cast<float>(i + 1) / static_cast<float>(rampLen);
                    if (outL != nullptr) outL[i] = lastOutSampleL * (1.0f - alpha) + outL[i] * alpha;
                    if (outR != nullptr) outR[i] = lastOutSampleR * (1.0f - alpha) + outR[i] * alpha;
                }
            } else if (trimmedOverflow) {
                const int xfadeLen = std::min(32, numSamples);
                for (int i = 0; i < xfadeLen; ++i) {
                    const float alpha = static_cast<float>(i + 1) / static_cast<float>(xfadeLen);
                    if (outL != nullptr) outL[i] = lastOutSampleL * (1.0f - alpha) + outL[i] * alpha;
                    if (outR != nullptr) outR[i] = lastOutSampleR * (1.0f - alpha) + outR[i] * alpha;
                }
            }
            lastOutSampleL = dstL[numSamples - 1];
            lastOutSampleR = dstR[numSamples - 1];
        } else if (available > 0) {
            // Transient low trough: pad readBuffer[available..safeNeeded-1] with smooth decay
            // from the last available sample and continue continuous resampling without unpriming!
            consecutiveEmptyCallbacks = 0;
            engine.recordMonitorUnderrun();
            windowMinFillSeconds = 0.0;
            float holdL = readBufferL[static_cast<size_t>(available - 1)];
            float holdR = readBufferR[static_cast<size_t>(available - 1)];
            for (int i = available; i < safeNeeded; ++i) {
                holdL *= 0.96f;
                holdR *= 0.96f;
                readBufferL[static_cast<size_t>(i)] = holdL;
                readBufferR[static_cast<size_t>(i)] = holdR;
            }
            const int usedL = interpolatorL.process(speedRatio, readBufferL.data(), dstL, numSamples, safeNeeded, 0);
            const int usedR = interpolatorR.process(speedRatio, readBufferR.data(), dstR, numSamples, safeNeeded, 0);
            engine.advanceMonitorRead(std::min(available, std::min(usedL, usedR)));
            lastOutSampleL = dstL[numSamples - 1];
            lastOutSampleR = dstR[numSamples - 1];
        } else {
            // FIFO completely empty: decay smoothly; only re-enter prefill if empty for 3+ callbacks
            engine.recordMonitorUnderrun();
            windowMinFillSeconds = 0.0;
            ++consecutiveEmptyCallbacks;
            if (consecutiveEmptyCallbacks >= 3) {
                monitorPrimed = false;
                wasUnderflowing = true;
            }
            for (int i = 0; i < numSamples; ++i) {
                lastOutSampleL *= 0.92f;
                lastOutSampleR *= 0.92f;
                if (outL != nullptr) outL[i] = lastOutSampleL;
                if (outR != nullptr) outR[i] = lastOutSampleR;
            }
            if (std::abs(lastOutSampleL) < 1.0e-7f) lastOutSampleL = 0.0f;
            if (std::abs(lastOutSampleR) < 1.0e-7f) lastOutSampleR = 0.0f;
        }
    }

private:
    MixerEngine& engine;
    juce::AudioDeviceManager deviceManager;
    std::atomic<bool> enabled{true};
    std::atomic<bool> deviceActive{false};
    juce::String activeDeviceName;

    double currentSampleRate{48000.0};
    int currentBufferSize{480};
    bool wasUnderflowing{false};
    bool monitorPrimed{false};
    int consecutiveEmptyCallbacks{0};
    double smoothedSpeedRatio{1.0};
    double currentCorrection{0.0};
    double windowMinFillSeconds{1.0e12};
    long windowFrames{0};
    double smoothedTroughSeconds{0.030};
    float lastOutSampleL{0.0f};
    float lastOutSampleR{0.0f};

    // Pre-allocated real-time scratch buffers (zero heap allocation in callback)
    std::vector<float> readBufferL;
    std::vector<float> readBufferR;
    std::vector<float> resampleBufferL;
    std::vector<float> resampleBufferR;

    juce::LagrangeInterpolator interpolatorL;
    juce::LagrangeInterpolator interpolatorR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecondaryAudioStreamer)
};

} // namespace nova
