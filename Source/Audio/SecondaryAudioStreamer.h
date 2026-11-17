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
        monitorPrimed = false;
        lastOutSampleL = 0.0f;
        lastOutSampleR = 0.0f;
        smoothedSpeedRatio = 1.0;
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
        wasUnderflowing = false;
        monitorPrimed = false;
        lastOutSampleL = 0.0f;
        lastOutSampleR = 0.0f;
        smoothedSpeedRatio = 1.0;
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
        const int minBlock = std::max(engBuf, numSamples);
        const int defaultTarget = (validEngineSr >= 88200.0) ? 2048 : 1024;
        const int targetQueue = std::clamp(std::max(minBlock * 3, defaultTarget), 512, 8192);

        int queuedSamples = engine.getMonitorFifoReady();
        bool trimmedOverflow = false;

        // Emergency drain when backlog exceeds 3x targetQueue: discard excess without resetting Lagrange history
        if (queuedSamples > targetQueue * 3) {
            const int discarded = engine.discardMonitorSamples(queuedSamples - targetQueue);
            queuedSamples = engine.getMonitorFifoReady();
            trimmedOverflow = (discarded > 0);
        }

        // Pre-fill watermark: wait until FIFO reaches targetQueue before pulling audio
        if (!monitorPrimed) {
            smoothedSpeedRatio = nominalSpeedRatio;
            if (queuedSamples < targetQueue) {
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
        }

        // Elastic drift compensation + one-pole smoothing to prevent ASRC pitch wobble across async WASAPI threads
        double driftAdjust = 0.0;
        const int diff = queuedSamples - targetQueue;
        if (diff > 24) {
            driftAdjust = std::min(0.004, (diff - 24) * 0.000015);
        } else if (diff < -24) {
            driftAdjust = -std::min(0.004, (-diff - 24) * 0.000015);
        }
        const double rawRatio = nominalSpeedRatio * (1.0 + driftAdjust);
        if (std::abs(nominalSpeedRatio - smoothedSpeedRatio) > 0.05) {
            smoothedSpeedRatio = rawRatio;
        } else {
            smoothedSpeedRatio += 0.02 * (rawRatio - smoothedSpeedRatio);
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
            // Partial underrun: clamp interpolated output count strictly inside `available`
            // so we never read stale history beyond readBuffer[available - 1], then decay exponentially.
            engine.recordMonitorUnderrun();
            monitorPrimed = false;
            wasUnderflowing = true;

            const int validOutSamples = std::clamp(
                static_cast<int>(std::floor(static_cast<double>(available - 1) / speedRatio)),
                0, numSamples);
            if (validOutSamples > 0) {
                const int partL = interpolatorL.process(speedRatio, readBufferL.data(), dstL, validOutSamples, available, 0);
                const int partR = interpolatorR.process(speedRatio, readBufferR.data(), dstR, validOutSamples, available, 0);
                engine.advanceMonitorRead(std::min(available, std::min(partL, partR)));
                if (trimmedOverflow) {
                    const int xfadeLen = std::min(32, validOutSamples);
                    for (int i = 0; i < xfadeLen; ++i) {
                        const float alpha = static_cast<float>(i + 1) / static_cast<float>(xfadeLen);
                        if (outL != nullptr) outL[i] = lastOutSampleL * (1.0f - alpha) + outL[i] * alpha;
                        if (outR != nullptr) outR[i] = lastOutSampleR * (1.0f - alpha) + outR[i] * alpha;
                    }
                }
                lastOutSampleL = dstL[validOutSamples - 1];
                lastOutSampleR = dstR[validOutSamples - 1];
            }
            for (int i = validOutSamples; i < numSamples; ++i) {
                lastOutSampleL *= 0.92f;
                lastOutSampleR *= 0.92f;
                if (outL != nullptr) outL[i] = lastOutSampleL;
                if (outR != nullptr) outR[i] = lastOutSampleR;
            }
            if (std::abs(lastOutSampleL) < 1.0e-7f) lastOutSampleL = 0.0f;
            if (std::abs(lastOutSampleR) < 1.0e-7f) lastOutSampleR = 0.0f;
        } else {
            // Full underrun: trigger smooth exponential sample-hold decay and wait for watermark refill
            engine.recordMonitorUnderrun();
            monitorPrimed = false;
            wasUnderflowing = true;
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
    int currentBufferSize{256};
    bool wasUnderflowing{false};
    bool monitorPrimed{false};
    double smoothedSpeedRatio{1.0};
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
