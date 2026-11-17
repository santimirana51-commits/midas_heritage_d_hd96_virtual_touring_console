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
     * Prefers Windows Audio (Low Latency Mode) with safe fallback to Shared Mode.
     */
    bool openDevice(const juce::String& outputDeviceName, double targetSampleRate = 0.0, int bufferSize = 256) {
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
            if (bufferSize >= 64)
                setup.bufferSize = bufferSize;

            const auto err = deviceManager.setAudioDeviceSetup(setup, true);
            if (err.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
                return false;

            if (auto* dev = deviceManager.getCurrentAudioDevice()) {
                const auto sizes = dev->getAvailableBufferSizes();
                int bestBuf = 0;
                if (sizes.contains(bufferSize)) {
                    bestBuf = bufferSize;
                } else {
                    int bestInRange = 999999;
                    int smallestAny = 999999;
                    for (int s : sizes) {
                        if (s >= 192 && s <= 512 && s < bestInRange)
                            bestInRange = s;
                        if (s >= 64 && s < smallestAny)
                            smallestAny = s;
                    }
                    if (bestInRange < 999999)
                        bestBuf = bestInRange;
                    else if (smallestAny < 999999)
                        bestBuf = smallestAny;
                }
                auto curSetup = deviceManager.getAudioDeviceSetup();
                if (bestBuf > 0 && curSetup.bufferSize != bestBuf) {
                    curSetup.bufferSize = bestBuf;
                    deviceManager.setAudioDeviceSetup(curSetup, true);
                }
            }
            return true;
        };

        if (tryOpenWithType("Windows Audio (Low Latency Mode)") ||
            tryOpenWithType("Windows Audio")) {
            deviceManager.addAudioCallback(this);
            deviceActive.store(true, std::memory_order_relaxed);
            activeDeviceName = outputDeviceName;
            return true;
        }

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
    // juce::AudioIODeviceCallback Implementation
    // =========================================================================

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        engine.resetMonitorFifoStats();
        wasUnderflowing = false;
        if (device != nullptr) {
            currentSampleRate = device->getCurrentSampleRate();
            currentBufferSize = device->getCurrentBufferSizeSamples();
            const int maxNeeded = static_cast<int>(std::ceil(std::max(1024, currentBufferSize) * 4.0)) + 512;
            readBufferL.assign(static_cast<size_t>(maxNeeded), 0.0f);
            readBufferR.assign(static_cast<size_t>(maxNeeded), 0.0f);
            resampleBufferL.assign(static_cast<size_t>(maxNeeded), 0.0f);
            resampleBufferR.assign(static_cast<size_t>(maxNeeded), 0.0f);
            interpolatorL.reset();
            interpolatorR.reset();
        }
    }

    void audioDeviceStopped() override {
        wasUnderflowing = false;
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
            if (outL != nullptr) std::fill_n(outL, numSamples, 0.0f);
            if (outR != nullptr) std::fill_n(outR, numSamples, 0.0f);
            return;
        }

        const double engineSr = engine.getSampleRate();
        const double secSr = (currentSampleRate > 1000.0) ? currentSampleRate : (engineSr > 1000.0 ? engineSr : 48000.0);
        const double validEngineSr = (engineSr > 1000.0) ? engineSr : secSr;
        const double nominalSpeedRatio = validEngineSr / secSr;

        const int queuedSamples = engine.getMonitorFifoQueueDepth();
        // Target elastic jitter buffer: ~8-16 ms (zero-crackle margin across 2 async WASAPI threads)
        const int targetQueue = std::clamp(static_cast<int>(std::ceil(numSamples * nominalSpeedRatio * 2.0)), 384, 1024);

        // Emergency drain only if huge queue backlog accumulated (e.g. after long device stall)
        if (queuedSamples > targetQueue + 1536) {
            engine.discardMonitorSamples(queuedSamples - (targetQueue + 256));
            interpolatorL.reset();
            interpolatorR.reset();
        }

        // Elastic drift compensation (micro-adjust speed by +/- 0.05% to 0.4% without any audible pitch shift)
        double driftAdjust = 0.0;
        const int diff = queuedSamples - targetQueue;
        if (diff > 24) {
            driftAdjust = std::min(0.004, (diff - 24) * 0.000015);
        } else if (diff < -24) {
            driftAdjust = -std::min(0.004, (-diff - 24) * 0.000015);
        }
        const double speedRatio = nominalSpeedRatio * (1.0 + driftAdjust);

        // Required input samples for Lagrange 4-point cubic polynomial interpolator
        const int neededEngineSamples = static_cast<int>(std::ceil(numSamples * speedRatio)) + 8;
        const int safeNeeded = std::min(neededEngineSamples, static_cast<int>(readBufferL.size()));

        // Hysteresis after underrun: wait until FIFO pre-fills to 75% of targetQueue so it doesn't oscillate/crackle
        const int minResumeSamples = wasUnderflowing ? std::max(neededEngineSamples, (targetQueue * 3) / 4) : neededEngineSamples;
        const int available = engine.peekMonitorSamples(readBufferL.data(), readBufferR.data(), safeNeeded);

        if (available >= neededEngineSamples && queuedSamples >= minResumeSamples) {
            const int usedL = interpolatorL.process(speedRatio, readBufferL.data(), (outL != nullptr ? outL : resampleBufferL.data()), numSamples);
            const int usedR = interpolatorR.process(speedRatio, readBufferR.data(), (outR != nullptr ? outR : resampleBufferR.data()), numSamples);
            const int consumed = std::max(usedL, usedR);
            engine.advanceMonitorRead(consumed);

            if (wasUnderflowing) {
                wasUnderflowing = false;
                const int rampLen = std::min(64, numSamples);
                for (int i = 0; i < rampLen; ++i) {
                    const float gain = static_cast<float>(i + 1) / static_cast<float>(rampLen);
                    if (outL != nullptr) outL[i] *= gain;
                    if (outR != nullptr) outR[i] *= gain;
                }
            }
        } else {
            // Buffer underrun handling: soft smooth ramp to zero to eliminate clicking
            engine.recordMonitorUnderrun();
            if (!wasUnderflowing) {
                wasUnderflowing = true;
                if (available > 6) {
                    const int partL = interpolatorL.process(speedRatio, readBufferL.data(), (outL != nullptr ? outL : resampleBufferL.data()), numSamples);
                    const int partR = interpolatorR.process(speedRatio, readBufferR.data(), (outR != nullptr ? outR : resampleBufferR.data()), numSamples);
                    engine.advanceMonitorRead(std::max(partL, partR));
                    const int rampLen = std::min(16, numSamples);
                    for (int i = 0; i < rampLen; ++i) {
                        const float gain = 1.0f - (static_cast<float>(i + 1) / static_cast<float>(rampLen));
                        if (outL != nullptr) outL[i] *= gain;
                        if (outR != nullptr) outR[i] *= gain;
                    }
                    if (numSamples > rampLen) {
                        if (outL != nullptr) std::fill_n(outL + rampLen, numSamples - rampLen, 0.0f);
                        if (outR != nullptr) std::fill_n(outR + rampLen, numSamples - rampLen, 0.0f);
                    }
                    return;
                }
            }
            if (outL != nullptr) std::fill_n(outL, numSamples, 0.0f);
            if (outR != nullptr) std::fill_n(outR, numSamples, 0.0f);
        }
    }

private:
    MixerEngine& engine;
    juce::AudioDeviceManager deviceManager;
    std::atomic<bool> enabled{true};
    std::atomic<bool> deviceActive{false};
    juce::String activeDeviceName;

    double currentSampleRate{48000.0};
    int currentBufferSize{256};
    bool wasUnderflowing{false};

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
