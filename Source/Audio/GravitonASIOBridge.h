#pragma once
// ==============================================================================
// Multichannel USB Audio Class 2.0 ASIO Streaming Backend
// Reference: MIDAS Heritage-D HD96-24 Chapter 26 (Hardware Bridge)
//
// Provides a lock-free stereo ring-buffer bridge between the Graviton DSP
// engine and an external ASIO-compatible audio interface.  The stub
// implementation is compiled by default; when the Steinberg ASIO SDK is
// available define USE_ASIO_SDK in CMake to activate ASIOBridgeReal.
//
// Key Features:
// - 64 k sample lock-free ring buffer per channel (~0.68 s @ 96 kHz)
// - Start / Stop control interface for the UI panel
// - Status & latency readouts for the ASIO Bridge Panel
// - processStereo() pass-through wired into FxSlot (FxType::ASIO_Bridge)
// - Zero heap allocation on the audio thread
// ==============================================================================
//
// **Important**: Defining USE_ASIO_SDK only enables Steinberg ASIO SDK header
// inclusion. It does NOT provide a real ASIO device backend. On Windows, the
// application falls back to JUCE's WASAPI device type. A real ASIO device
// backend must be implemented separately.
//

#include "DSP/GravitonDSP.h"
#include <array>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace nova::graviton {

class ASIOBridge final {
public:
    static constexpr size_t kBufferSize = 65536; // 64 k samples per channel

    ASIOBridge() {
        bufferL.resize(kBufferSize, 0.0);
        bufferR.resize(kBufferSize, 0.0);
        reset();
    }

    void reset() noexcept {
        if (bufferL.size() == kBufferSize)
            std::fill(bufferL.begin(), bufferL.end(), 0.0);
        if (bufferR.size() == kBufferSize)
            std::fill(bufferR.begin(), bufferR.end(), 0.0);
        writePos.store(0, std::memory_order_relaxed);
        readPos.store(0, std::memory_order_relaxed);
        running.store(false, std::memory_order_relaxed);
    }

    void setSampleRate(double sr) noexcept {
        cachedSampleRate = std::max(1000.0, sr);
    }

    // ---- Audio Processing (called from FxSlot on audio thread) ----
    void processStereo(double& inOutL, double& inOutR) noexcept {
        const size_t wp = writePos.load(std::memory_order_relaxed);
        bufferL[wp] = inOutL;
        bufferR[wp] = inOutR;
        writePos.store((wp + 1) % kBufferSize, std::memory_order_release);

        const size_t rp = readPos.load(std::memory_order_acquire);
        inOutL = bufferL[rp];
        inOutR = bufferR[rp];
        readPos.store((rp + 1) % kBufferSize, std::memory_order_release);
    }

    // ---- Control interface for UI ----
    void start() noexcept { running.store(true, std::memory_order_relaxed); }
    void stop()  noexcept { running.store(false, std::memory_order_relaxed); }
    bool isRunning() const noexcept {
        return running.load(std::memory_order_relaxed);
    }
    const char* status() const noexcept {
        return isRunning() ? "Running" : "Stopped";
    }

    // ---- Latency readout ----
    double getLatencyMs() const noexcept {
        const size_t wp = writePos.load(std::memory_order_relaxed);
        const size_t rp = readPos.load(std::memory_order_relaxed);
        const size_t dist = (wp >= rp) ? (wp - rp) : (kBufferSize - rp + wp);
        return static_cast<double>(dist) * 1000.0 / cachedSampleRate;
    }

    double getSampleRate() const noexcept { return cachedSampleRate; }

    // ---- Test helpers ----
    size_t getWritePos() const noexcept {
        return writePos.load(std::memory_order_relaxed);
    }
    size_t getReadPos() const noexcept {
        return readPos.load(std::memory_order_relaxed);
    }

private:
    double cachedSampleRate = 48000.0;
    std::vector<double> bufferL;
    std::vector<double> bufferR;
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> readPos{0};
    std::atomic<bool>   running{false};
};

} // namespace nova::graviton

