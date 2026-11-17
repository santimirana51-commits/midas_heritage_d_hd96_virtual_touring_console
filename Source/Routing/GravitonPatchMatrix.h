#pragma once

#include "DSP/GravitonDSP.h"
#include <array>
#include <atomic>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace nova::graviton {

// ==============================================================================
// Streamlined PC Digital Mixer Patch Matrix Engine
// Focused on:
// - 32 Logical Input Channels
// - Local Hardware Soundcard/USB Inputs (1-32)
// - Virtual PC / Windows / DAW / Bluetooth Audio Inputs (1-16)
// - Dedicated Stereo Master & Stereo Aux Monitor Output Routing
// ==============================================================================

enum class PatchDeviceType : int {
    LocalHardware = 0, // Local Soundcard / USB Audio Interface In 1-32
    VirtualAudio = 1   // Windows Virtual / Media / Bluetooth Audio In 1-16
};

enum class ClockSyncSource : int {
    InternalClock = 0, // Clean PC Master Clock (48 / 96 kHz)
    AudioDevice = 1    // External Hardware Device Clock
};

enum class OutputBusSourceType : int {
    Unassigned = 0,
    MainLeft = 1,
    MainRight = 2,
    Mono = 3,
    AuxBus = 4,         // Aux 1 = Monitor L, Aux 2 = Monitor R
    MatrixBus = 5,
    DirectOut = 6,      // Channel 1-32 Direct Output
    MonitorLeft = 7,    // Monitor A L (WASAPI 2)
    MonitorRight = 8,   // Monitor A R (WASAPI 2)
    SurroundSpeaker = 9
};

class StageBoxPatchMatrix final {
public:
    static constexpr size_t kNumLogicalChannels = 32;
    static constexpr size_t kNumAuxBuses = 8;
    static constexpr size_t kNumMatrixBuses = 8;

    static constexpr size_t kLocalPorts = 32;   // Local Hardware soundcard ports 1-32
    static constexpr size_t kVirtualPorts = 16; // Virtual PC / Bluetooth ports 1-16

    // Total Input Ports: 32 (Hardware) + 16 (Virtual) = 48
    static constexpr size_t kTotalInputPorts = kLocalPorts + kVirtualPorts;
    // Total Output Ports: 8 (Hardware Out) + 8 (Virtual Out) = 16
    static constexpr size_t kTotalOutputPorts = 16;

    struct OutputRoutingAssignment {
        std::atomic<int> busType{static_cast<int>(OutputBusSourceType::Unassigned)};
        std::atomic<int> busIndex{0}; // 0-1 for Aux, 0-3 for Matrix, 0-31 for DirectOut
    };

    StageBoxPatchMatrix() noexcept {
        reset();
    }

    void reset() noexcept {
        clockSource.store(static_cast<int>(ClockSyncSource::InternalClock), std::memory_order_relaxed);
        clockLocked.store(true, std::memory_order_relaxed);
        sampleRate.store(48000.0, std::memory_order_relaxed);
        jitterPs.store(4.2f, std::memory_order_relaxed);
        syncSlipCount.store(0, std::memory_order_relaxed);

        for (size_t c = 0; c < kNumLogicalChannels; ++c) {
            channelInputSource[c].store(-1, std::memory_order_relaxed); // -1 = unassigned
        }

        for (size_t p = 0; p < kTotalOutputPorts; ++p) {
            outputRouting[p].busType.store(static_cast<int>(OutputBusSourceType::Unassigned), std::memory_order_relaxed);
            outputRouting[p].busIndex.store(0, std::memory_order_relaxed);
        }
    }

    // --- Clock & Sync Status ---
    void setClockSource(ClockSyncSource src) noexcept {
        clockSource.store(static_cast<int>(src), std::memory_order_relaxed);
        clockLocked.store(true, std::memory_order_relaxed);
        jitterPs.store(src == ClockSyncSource::InternalClock ? 3.5f : 6.5f, std::memory_order_relaxed);
    }

    ClockSyncSource getClockSource() const noexcept {
        return static_cast<ClockSyncSource>(clockSource.load(std::memory_order_relaxed));
    }

    bool isClockLocked() const noexcept {
        return clockLocked.load(std::memory_order_relaxed);
    }

    double getSampleRate() const noexcept {
        return sampleRate.load(std::memory_order_relaxed);
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0)
            sampleRate.store(sr, std::memory_order_relaxed);
    }

    float getJitterPicoseconds() const noexcept {
        return jitterPs.load(std::memory_order_relaxed);
    }

    uint32_t getSyncSlipCount() const noexcept {
        return syncSlipCount.load(std::memory_order_relaxed);
    }

    // --- Input Patch Matrix ---
    // Connects an input port (0..47, or -1 for unassigned) to console logical channel (0..31)
    bool patchInput(int logicalChannel, int globalPortIndex) noexcept {
        if (logicalChannel < 0 || logicalChannel >= static_cast<int>(kNumLogicalChannels))
            return false;
        if (globalPortIndex < -1 || globalPortIndex >= static_cast<int>(kTotalInputPorts))
            return false;

        channelInputSource[static_cast<size_t>(logicalChannel)].store(
            globalPortIndex,
            std::memory_order_relaxed);
        return true;
    }

    int getInputPatch(int logicalChannel) const noexcept {
        if (logicalChannel >= 0 && logicalChannel < static_cast<int>(kNumLogicalChannels)) {
            return channelInputSource[static_cast<size_t>(logicalChannel)].load(std::memory_order_relaxed);
        }
        return -1;
    }

    // Helper: decode global port index into device type and device port index
    static void decodeGlobalPort(int globalPort, PatchDeviceType& outDev, int& outPortIndex) noexcept {
        if (globalPort < 0 || globalPort >= static_cast<int>(kTotalInputPorts)) {
            outDev = PatchDeviceType::LocalHardware;
            outPortIndex = -1;
            return;
        }

        if (globalPort < static_cast<int>(kLocalPorts)) {
            outDev = PatchDeviceType::LocalHardware;
            outPortIndex = globalPort;
        } else {
            outDev = PatchDeviceType::VirtualAudio;
            outPortIndex = globalPort - static_cast<int>(kLocalPorts);
        }
    }

    // Helper: encode device type and device port index into global port index
    static int encodeGlobalPort(PatchDeviceType dev, int portIndex) noexcept {
        if (portIndex < 0) return -1;
        switch (dev) {
            case PatchDeviceType::LocalHardware:
                return (portIndex < static_cast<int>(kLocalPorts)) ? portIndex : -1;
            case PatchDeviceType::VirtualAudio:
                return (portIndex < static_cast<int>(kVirtualPorts)) ? static_cast<int>(kLocalPorts) + portIndex : -1;
            default:
                return -1;
        }
    }

    // --- Output Patch Matrix ---
    bool patchOutput(int globalPortIndex, OutputBusSourceType busType, int busIndex) noexcept {
        if (globalPortIndex < 0 || globalPortIndex >= static_cast<int>(kTotalOutputPorts))
            return false;

        switch (busType) {
            case OutputBusSourceType::Unassigned:
            case OutputBusSourceType::MainLeft:
            case OutputBusSourceType::MainRight:
            case OutputBusSourceType::Mono:
            case OutputBusSourceType::MonitorLeft:
            case OutputBusSourceType::MonitorRight:
                busIndex = 0;
                break;
            case OutputBusSourceType::AuxBus:
                if (busIndex < 0 || busIndex >= static_cast<int>(kNumAuxBuses))
                    return false;
                break;
            case OutputBusSourceType::MatrixBus:
                if (busIndex < 0 || busIndex >= static_cast<int>(kNumMatrixBuses))
                    return false;
                break;
            case OutputBusSourceType::DirectOut:
                if (busIndex < 0 || busIndex >= static_cast<int>(kNumLogicalChannels))
                    return false;
                break;
            case OutputBusSourceType::SurroundSpeaker:
                if (busIndex < 0 || busIndex >= 12)
                    return false;
                break;
            default:
                return false;
        }

        auto& entry = outputRouting[static_cast<size_t>(globalPortIndex)];
        entry.busType.store(static_cast<int>(busType), std::memory_order_relaxed);
        entry.busIndex.store(busIndex, std::memory_order_relaxed);
        return true;
    }

    void getOutputPatch(int globalPortIndex, OutputBusSourceType& outType, int& outBusIndex) const noexcept {
        if (globalPortIndex >= 0 && globalPortIndex < static_cast<int>(kTotalOutputPorts)) {
            const auto& entry = outputRouting[static_cast<size_t>(globalPortIndex)];
            outType = static_cast<OutputBusSourceType>(entry.busType.load(std::memory_order_relaxed));
            outBusIndex = entry.busIndex.load(std::memory_order_relaxed);
        } else {
            outType = OutputBusSourceType::Unassigned;
            outBusIndex = 0;
        }
    }

    // --- Streamlined PC Auto-Patching Workflows ---
    // 1. Auto-Patch Hardware Inputs (1..32) 1:1 to console channels
    void autoPatchLocalInputs(int startChannel = 0, int count = 32) noexcept {
        const int maxCh = std::min(static_cast<int>(kNumLogicalChannels), startChannel + count);
        for (int ch = startChannel; ch < maxCh; ++ch) {
            const int offset = ch - startChannel;
            patchInput(ch, encodeGlobalPort(PatchDeviceType::LocalHardware, offset));
        }
    }

    // 2. Auto-Patch Virtual PC Audio (1..16) to console channels
    void autoPatchVirtualAudio(int startChannel = 16, int count = 16) noexcept {
        const int maxCh = std::min(static_cast<int>(kNumLogicalChannels), startChannel + count);
        for (int ch = startChannel; ch < maxCh; ++ch) {
            const int offset = ch - startChannel;
            patchInput(ch, encodeGlobalPort(PatchDeviceType::VirtualAudio, offset));
        }
    }

    // 3. Auto-Patch Master and Monitor to Physical Outputs
    void autoPatchMasterAndMonitor() noexcept {
        // Port 0 & 1 = Master L/R
        patchOutput(0, OutputBusSourceType::MainLeft, 0);
        patchOutput(1, OutputBusSourceType::MainRight, 0);
        // Port 2 & 3 = Monitor L/R (Aux 1 & Aux 2)
        patchOutput(2, OutputBusSourceType::AuxBus, 0);
        patchOutput(3, OutputBusSourceType::AuxBus, 1);
    }

    // --- Auto-Label Generation ---
    std::string getChannelAutoLabel(int logicalChannel) const {
        const int port = getInputPatch(logicalChannel);
        if (port < 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "CH %02d", logicalChannel + 1);
            return std::string(buf);
        }
        PatchDeviceType dev;
        int portIdx = 0;
        decodeGlobalPort(port, dev, portIdx);
        char buf[32];
        if (dev == PatchDeviceType::LocalHardware) {
            snprintf(buf, sizeof(buf), "HW IN %02d", portIdx + 1);
        } else {
            snprintf(buf, sizeof(buf), "VIRT %02d", portIdx + 1);
        }
        return std::string(buf);
    }

    // Real-Time Output Delivery in Audio Thread (Zero Allocation, Lock-Free)
    template <size_t VisibleAuxCount, size_t VisibleMatrixCount, size_t DirectOutCount = kNumLogicalChannels>
    void routeOutputs(
        std::array<double, kTotalOutputPorts>& outBuffer,
        double mainL, double mainR, double mono,
        const std::array<double, VisibleAuxCount>& auxBuses,
        const std::array<double, VisibleMatrixCount>& matrixBuses,
        double monL = 0.0, double monR = 0.0,
        const std::array<double, 12>* surroundSpeakers = nullptr,
        const std::array<double, DirectOutCount>* directOuts = nullptr) const noexcept
    {
        for (size_t p = 0; p < kTotalOutputPorts; ++p) {
            const auto type = static_cast<OutputBusSourceType>(outputRouting[p].busType.load(std::memory_order_relaxed));
            const int idx = outputRouting[p].busIndex.load(std::memory_order_relaxed);
            double sample = 0.0;
            switch (type) {
                case OutputBusSourceType::MainLeft:
                    sample = mainL;
                    break;
                case OutputBusSourceType::MainRight:
                    sample = mainR;
                    break;
                case OutputBusSourceType::Mono:
                    sample = mono;
                    break;
                case OutputBusSourceType::AuxBus:
                    if (idx >= 0 && idx < static_cast<int>(VisibleAuxCount))
                        sample = auxBuses[static_cast<size_t>(idx)];
                    break;
                case OutputBusSourceType::MatrixBus:
                    if (idx >= 0 && idx < static_cast<int>(VisibleMatrixCount))
                        sample = matrixBuses[static_cast<size_t>(idx)];
                    break;
                case OutputBusSourceType::DirectOut:
                    if (directOuts != nullptr && idx >= 0 && idx < static_cast<int>(DirectOutCount))
                        sample = (*directOuts)[static_cast<size_t>(idx)];
                    break;
                case OutputBusSourceType::MonitorLeft:
                    sample = monL;
                    break;
                case OutputBusSourceType::MonitorRight:
                    sample = monR;
                    break;
                case OutputBusSourceType::SurroundSpeaker:
                    if (surroundSpeakers != nullptr && idx >= 0 && idx < 12)
                        sample = (*surroundSpeakers)[static_cast<size_t>(idx)];
                    break;
                case OutputBusSourceType::Unassigned:
                default:
                    sample = 0.0;
                    break;
            }
            outBuffer[p] = sanitize(sample);
        }
    }

private:
    std::atomic<int> clockSource{static_cast<int>(ClockSyncSource::InternalClock)};
    std::atomic<bool> clockLocked{true};
    std::atomic<double> sampleRate{48000.0};
    std::atomic<float> jitterPs{4.2f};
    std::atomic<uint32_t> syncSlipCount{0};

    std::array<std::atomic<int>, kNumLogicalChannels> channelInputSource;
    std::array<OutputRoutingAssignment, kTotalOutputPorts> outputRouting;
};

} // namespace nova::graviton
