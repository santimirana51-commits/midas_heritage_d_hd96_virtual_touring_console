#pragma once

//==============================================================================
// GravitonASIOStreamer.h
// NOVA HD Digital Mixing System — Graviton Mk2 DSP Audio Engine
//
// Multichannel DAW USB 64×64 ASIO Direct Recording Streamer
//
// Provides a virtual 64-in × 64-out streaming bus between NOVA HD and an
// external DAW (Reaper, Pro Tools, Nuendo).  All ring-buffer I/O is performed
// on the real-time audio thread without heap allocation.  GUI-thread helpers
// allow routing configuration, virtual-soundcheck toggling and status queries.
//
// namespace nova::graviton
//==============================================================================

#include <atomic>
#include <algorithm>
#include <cstring>

namespace nova::graviton {

//==============================================================================
/**
 *  DAWStreamingInterface
 *
 *  Lock-free, zero-heap-allocation 64×64 channel streaming bus.
 *
 *  Ring topology
 *  ─────────────
 *  outputRing  : NOVA HD → DAW  (recording path)
 *  inputRing   : DAW → NOVA HD  (virtual soundcheck / playback path)
 *
 *  Routing
 *  ───────
 *  outputRouting[novaCh] = dawCh   (which DAW channel receives NOVA channel)
 *  inputRouting [novaCh] = dawCh   (which DAW channel feeds NOVA channel)
 *  Default: identity mapping  (channel N ↔ channel N).
 *
 *  Thread safety
 *  ─────────────
 *  pushToDAW / pullFromDAW  → audio thread only
 *  set*  / start* / stop*  → GUI / control thread only
 *  Atomic read/write positions ensure visibility across threads.
 */
class DAWStreamingInterface final
{
public:
    //==========================================================================
    // Constants
    //==========================================================================

    /** Number of channels on each direction of the streaming bus. */
    static constexpr int kDAWChannels = 64;

    /** Ring buffer depth in samples — must be a power of 2. */
    static constexpr int kRingSize    = 4096;

    /** Bitmask for wrapping ring indices without division. */
    static constexpr int kRingMask    = kRingSize - 1;

    //==========================================================================
    // Construction
    //==========================================================================

    /**
     *  Initialises all ring buffers to zero and establishes an identity
     *  routing (NOVA channel N → DAW channel N) for both directions.
     *  No heap allocation occurs here or anywhere in this class.
     */
    DAWStreamingInterface() noexcept
    {
        for (int i = 0; i < kDAWChannels; ++i)
        {
            inputRouting[i]  = i;
            outputRouting[i] = i;
        }

        // Zero the ring buffers (compile-time aggregate init already does this
        // for the inline initialisers, but be explicit for clarity).
        std::memset(outputRing, 0, sizeof(outputRing));
        std::memset(inputRing,  0, sizeof(inputRing));
    }

    // Non-copyable, non-movable — single owner, fixed memory layout.
    DAWStreamingInterface(const DAWStreamingInterface&)            = delete;
    DAWStreamingInterface& operator=(const DAWStreamingInterface&) = delete;
    DAWStreamingInterface(DAWStreamingInterface&&)                 = delete;
    DAWStreamingInterface& operator=(DAWStreamingInterface&&)      = delete;

    ~DAWStreamingInterface() noexcept = default;

    //==========================================================================
    // Audio-thread API
    //==========================================================================

    /**
     *  pushToDAW  — NOVA HD → DAW (recording)
     *
     *  Called from the real-time audio callback.  Writes `numSamples` frames
     *  from `channelData` into outputRing, applying outputRouting so each
     *  NOVA channel lands on the correct DAW channel slot.
     *
     *  An xRun is counted if the write pointer overtakes the read pointer
     *  (ring overflow / DAW not consuming fast enough).
     *
     *  @param channelData  Non-null pointer to array of channel pointers.
     *                      Individual channel pointers may be null (skipped).
     *  @param numChannels  Number of channels supplied by the caller.
     *  @param numSamples   Frames per block (must be ≤ kRingSize).
     */
    void pushToDAW(const double* const* channelData,
                   int numChannels,
                   int numSamples) noexcept
    {
        if (!streamingActive.load(std::memory_order_relaxed))
            return;

        if (channelData == nullptr || numSamples <= 0)
            return;

        const int chLimit   = std::min(numChannels, kDAWChannels);
        const int writeBase = outWritePos.load(std::memory_order_relaxed);

        for (int s = 0; s < numSamples; ++s)
        {
            const int writeSlot = (writeBase + s) & kRingMask;

            for (int ch = 0; ch < chLimit; ++ch)
            {
                const int dawCh = outputRouting[ch];

                if (dawCh < 0 || dawCh >= kDAWChannels)
                    continue;

                if (channelData[ch] == nullptr)
                    continue;

                outputRing[dawCh][writeSlot] = channelData[ch][s];
            }
        }

        // Advance write position.
        const int newWrite = (writeBase + numSamples) & kRingMask;
        outWritePos.store(newWrite, std::memory_order_release);

        // Overflow check: if the new write position has overtaken the read
        // position within the same ring lap, an xRun has occurred.
        const int readPos = outReadPos.load(std::memory_order_acquire);
        const int used    = (newWrite - readPos) & kRingMask;
        if (used == 0 || used > (kRingSize - numSamples))
            xRunCount.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     *  pullFromDAW  — DAW → NOVA HD (virtual soundcheck / playback)
     *
     *  Called from the real-time audio callback.  Reads `numSamples` frames
     *  from inputRing into `channelData`, applying inputRouting so each NOVA
     *  channel reads from the correct DAW slot.  Channels with an invalid
     *  routing index are filled with silence.
     *
     *  @param channelData  Non-null pointer to array of channel pointers.
     *  @param numChannels  Number of channels the caller expects.
     *  @param numSamples   Frames per block.
     */
    void pullFromDAW(double* const* channelData,
                     int numChannels,
                     int numSamples) noexcept
    {
        if (!virtualSoundcheckActive.load(std::memory_order_relaxed))
            return;

        if (channelData == nullptr || numSamples <= 0)
            return;

        const int chLimit  = std::min(numChannels, kDAWChannels);
        const int readBase = inReadPos.load(std::memory_order_relaxed);

        for (int s = 0; s < numSamples; ++s)
        {
            const int readSlot = (readBase + s) & kRingMask;

            for (int ch = 0; ch < chLimit; ++ch)
            {
                if (channelData[ch] == nullptr)
                    continue;

                const int dawCh = inputRouting[ch];

                if (dawCh >= 0 && dawCh < kDAWChannels)
                    channelData[ch][s] = inputRing[dawCh][readSlot];
                else
                    channelData[ch][s] = 0.0;
            }
        }

        // Advance read position.
        inReadPos.store((readBase + numSamples) & kRingMask,
                        std::memory_order_release);
    }

    /** Real-time per-sample pull for Virtual Soundcheck without multi-channel arrays. */
    double pullSample(int novaCh, int sampleOffset) const noexcept
    {
        if (!virtualSoundcheckActive.load(std::memory_order_relaxed) ||
            novaCh < 0 || novaCh >= kDAWChannels)
            return 0.0;
        const int dawCh = inputRouting[novaCh];
        if (dawCh < 0 || dawCh >= kDAWChannels)
            return 0.0;
        const int readBase = inReadPos.load(std::memory_order_relaxed);
        const int readSlot = (readBase + sampleOffset) & kRingMask;
        return inputRing[dawCh][readSlot];
    }

    void advanceReadPos(int numSamples) noexcept
    {
        const int readBase = inReadPos.load(std::memory_order_relaxed);
        inReadPos.store((readBase + numSamples) & kRingMask, std::memory_order_release);
    }

    /** Real-time per-sample push to DAW stream without multi-channel arrays. */
    void pushSample(int novaCh, int sampleOffset, double sample) noexcept
    {
        if (!streamingActive.load(std::memory_order_relaxed) ||
            novaCh < 0 || novaCh >= kDAWChannels)
            return;
        const int dawCh = outputRouting[novaCh];
        if (dawCh < 0 || dawCh >= kDAWChannels)
            return;
        const int writeBase = outWritePos.load(std::memory_order_relaxed);
        const int writeSlot = (writeBase + sampleOffset) & kRingMask;
        outputRing[dawCh][writeSlot] = sample;
    }

    void advanceWritePos(int numSamples) noexcept
    {
        const int writeBase = outWritePos.load(std::memory_order_relaxed);
        outWritePos.store((writeBase + numSamples) & kRingMask, std::memory_order_release);
    }

    //==========================================================================
    // GUI / control-thread API — configuration
    //==========================================================================

    /**
     *  Map NOVA input channel `novaCh` to DAW channel `dawCh`.
     *  Safe to call from the GUI thread; reads on the audio thread are
     *  naturally benign (worst case: one stale value during transition).
     */
    void setInputRouting(int novaCh, int dawCh) noexcept
    {
        if (novaCh >= 0 && novaCh < kDAWChannels)
            inputRouting[novaCh] = dawCh;
    }

    int getInputRouting(int novaCh) const noexcept
    {
        if (novaCh >= 0 && novaCh < kDAWChannels)
            return inputRouting[novaCh];
        return -1;
    }

    /**
     *  Map NOVA output channel `novaCh` to DAW channel `dawCh`.
     */
    void setOutputRouting(int novaCh, int dawCh) noexcept
    {
        if (novaCh >= 0 && novaCh < kDAWChannels)
            outputRouting[novaCh] = dawCh;
    }

    int getOutputRouting(int novaCh) const noexcept
    {
        if (novaCh >= 0 && novaCh < kDAWChannels)
            return outputRouting[novaCh];
        return -1;
    }

    /** Enable / disable virtual soundcheck (DAW→NOVA playback path). */
    void setVirtualSoundcheck(bool active) noexcept
    {
        virtualSoundcheckActive.store(active, std::memory_order_relaxed);
    }

    /** Begin streaming (allows pushToDAW to write). */
    void startStreaming() noexcept
    {
        streamingActive.store(true, std::memory_order_relaxed);
    }

    /** Halt streaming.  Does NOT clear ring buffers. */
    void stopStreaming() noexcept
    {
        streamingActive.store(false, std::memory_order_relaxed);
    }

    /**
     *  reset()
     *
     *  Zeroes both ring buffers, resets all read/write positions, clears the
     *  xRun counter and resets the streaming/virtual-soundcheck flags.
     *  Call from the GUI thread while the audio thread is not active.
     */
    void reset() noexcept
    {
        streamingActive.store(false,        std::memory_order_relaxed);
        virtualSoundcheckActive.store(false, std::memory_order_relaxed);

        std::memset(outputRing, 0, sizeof(outputRing));
        std::memset(inputRing,  0, sizeof(inputRing));

        outWritePos.store(0, std::memory_order_relaxed);
        outReadPos.store(0,  std::memory_order_relaxed);
        inWritePos.store(0,  std::memory_order_relaxed);
        inReadPos.store(0,   std::memory_order_relaxed);

        xRunCount.store(0,   std::memory_order_relaxed);
    }

    /** Update the nominal sample rate (informational; used by callers). */
    void setSampleRate(double sr) noexcept { sampleRate = sr; }

    //==========================================================================
    // GUI / control-thread API — status queries
    //==========================================================================

    /** Returns true while the NOVA→DAW recording path is active. */
    bool isStreaming() const noexcept
    {
        return streamingActive.load(std::memory_order_relaxed);
    }

    /** Returns true while the DAW→NOVA virtual-soundcheck path is active. */
    bool isVirtualSoundcheckActive() const noexcept
    {
        return virtualSoundcheckActive.load(std::memory_order_relaxed);
    }

    /** Cumulative count of ring-buffer overflow events since last clear. */
    int getXRunCount() const noexcept
    {
        return xRunCount.load(std::memory_order_relaxed);
    }

    /** Configured latency compensation in samples. */
    int getLatencySamples() const noexcept
    {
        return latencySamples.load(std::memory_order_relaxed);
    }

    /** Reset the xRun counter to zero. */
    void clearXRunCount() noexcept
    {
        xRunCount.store(0, std::memory_order_relaxed);
    }

    /** Set the latency compensation value (samples). */
    void setLatencySamples(int samples) noexcept
    {
        latencySamples.store(samples, std::memory_order_relaxed);
    }

private:
    //==========================================================================
    // Ring buffers
    //==========================================================================

    /** NOVA HD → DAW recording ring  [dawChannel][sampleIndex] */
    double outputRing[kDAWChannels][kRingSize]{};

    /** DAW → NOVA HD playback ring   [dawChannel][sampleIndex] */
    double inputRing[kDAWChannels][kRingSize]{};

    //==========================================================================
    // Ring-buffer positions
    //==========================================================================

    std::atomic<int> outWritePos{0};  ///< Audio thread writes here (NOVA→DAW)
    std::atomic<int> outReadPos{0};   ///< DAW consumer reads from here
    std::atomic<int> inWritePos{0};   ///< DAW producer writes here (DAW→NOVA)
    std::atomic<int> inReadPos{0};    ///< Audio thread reads from here

    //==========================================================================
    // State flags
    //==========================================================================

    std::atomic<bool> virtualSoundcheckActive{false};
    std::atomic<bool> streamingActive{false};

    //==========================================================================
    // Diagnostics
    //==========================================================================

    std::atomic<int> xRunCount{0};       ///< Overflow event counter
    std::atomic<int> latencySamples{256};///< Latency compensation (samples)

    //==========================================================================
    // Routing tables  (GUI thread writes, audio thread reads — benign race)
    //==========================================================================

    /** inputRouting[novaCh]  = dawCh  — which DAW channel feeds NOVA ch */
    int inputRouting[kDAWChannels]{};

    /** outputRouting[novaCh] = dawCh  — which DAW channel receives NOVA ch */
    int outputRouting[kDAWChannels]{};

    //==========================================================================
    // Nominal sample rate (informational)
    //==========================================================================

    double sampleRate{48000.0};
};

// ==============================================================================
// ASIOHardwareStreamBridge: Multichannel USB Audio Class 2.0 / ASIO Hardware Bridge
// Reference: MIDAS Heritage-D HD96-24 Chapter 26 (pp. 215-228)
//
// Key Features:
// - Direct low-latency hardware stream bridge between 64x64 DAW ring buffer
//   and physical audio interface hardware buffers (float, PCM24, PCM32).
// - Bi-directional streaming: Stage inputs -> DAW recording ring buffer,
//   DAW playback ring buffer -> physical hardware line outputs.
// - Interleaved USB Audio Class 2.0 frame converters for low-overhead bulk transfer.
// - Round-trip latency (RTT) calculator based on buffer size and sample rate.
// - 100% lock-free, zero heap allocation in real-time callbacks.
// ==============================================================================
class ASIOHardwareStreamBridge final {
public:
    explicit ASIOHardwareStreamBridge(DAWStreamingInterface& streamerRef) noexcept
        : streamer(streamerRef) {}

    void setEnabled(bool active) noexcept {
        enabled.store(active, std::memory_order_relaxed);
    }
    bool isEnabled() const noexcept {
        return enabled.load(std::memory_order_relaxed);
    }

    void setSampleRate(double sr) noexcept {
        if (sr > 1000.0) sampleRate.store(sr, std::memory_order_relaxed);
    }
    double getSampleRate() const noexcept {
        return sampleRate.load(std::memory_order_relaxed);
    }

    void setHardwareBufferSize(int size) noexcept {
        if (size > 0 && size <= 8192) hwBufferSize.store(size, std::memory_order_relaxed);
    }
    int getHardwareBufferSize() const noexcept {
        return hwBufferSize.load(std::memory_order_relaxed);
    }

    // Direct bridge for JUCE / ASIO physical float buffers
    void bridgeHardware(const float* const* inputs, int numInputs,
                        float* const* outputs, int numOutputs,
                        int numSamples) noexcept {
        if (!enabled.load(std::memory_order_relaxed) || numSamples <= 0) return;

        // Bridge Inputs: Physical Hardware Mic/Line -> DAW Recording Ring Buffer
        if (inputs != nullptr && streamer.isStreaming()) {
            const int inChans = std::min(numInputs, DAWStreamingInterface::kDAWChannels);
            for (int ch = 0; ch < inChans; ++ch) {
                if (inputs[ch] != nullptr) {
                    for (int s = 0; s < numSamples; ++s) {
                        streamer.pushSample(ch, s, static_cast<double>(inputs[ch][s]));
                    }
                }
            }
            streamer.advanceWritePos(numSamples);
        }

        // Bridge Outputs: DAW Playback Ring Buffer -> Physical Hardware Line Outputs
        if (outputs != nullptr && streamer.isVirtualSoundcheckActive()) {
            const int outChans = std::min(numOutputs, DAWStreamingInterface::kDAWChannels);
            for (int ch = 0; ch < outChans; ++ch) {
                if (outputs[ch] != nullptr) {
                    for (int s = 0; s < numSamples; ++s) {
                        outputs[ch][s] = static_cast<float>(streamer.pullSample(ch, s));
                    }
                }
            }
            streamer.advanceReadPos(numSamples);
        }
    }

    // Interleaved 32-bit Float UAC 2.0 Packet Bridging
    void bridgeInterleavedFloatInputs(const float* interleaved, int numChannels, int numSamples) noexcept {
        if (!enabled.load(std::memory_order_relaxed) || interleaved == nullptr || numSamples <= 0) return;
        const int chLimit = std::min(numChannels, DAWStreamingInterface::kDAWChannels);
        for (int s = 0; s < numSamples; ++s) {
            for (int ch = 0; ch < chLimit; ++ch) {
                const float smp = interleaved[s * numChannels + ch];
                streamer.pushSample(ch, s, static_cast<double>(smp));
            }
        }
        streamer.advanceWritePos(numSamples);
    }

    void bridgeInterleavedFloatOutputs(float* interleaved, int numChannels, int numSamples) noexcept {
        if (!enabled.load(std::memory_order_relaxed) || interleaved == nullptr || numSamples <= 0) return;
        const int chLimit = std::min(numChannels, DAWStreamingInterface::kDAWChannels);
        for (int s = 0; s < numSamples; ++s) {
            for (int ch = 0; ch < chLimit; ++ch) {
                interleaved[s * numChannels + ch] = static_cast<float>(streamer.pullSample(ch, s));
            }
        }
        streamer.advanceReadPos(numSamples);
    }

    // Interleaved 24-bit PCM UAC 2.0 Packet Bridging (3 bytes per sample)
    void bridgeInterleavedPCM24Inputs(const uint8_t* pcmBytes, int numChannels, int numSamples) noexcept {
        if (!enabled.load(std::memory_order_relaxed) || pcmBytes == nullptr || numSamples <= 0) return;
        const int chLimit = std::min(numChannels, DAWStreamingInterface::kDAWChannels);
        constexpr double kNorm24 = 1.0 / 8388608.0; // 2^23
        for (int s = 0; s < numSamples; ++s) {
            for (int ch = 0; ch < chLimit; ++ch) {
                const size_t byteIdx = (static_cast<size_t>(s) * static_cast<size_t>(numChannels) + static_cast<size_t>(ch)) * 3;
                int32_t val = (static_cast<int32_t>(pcmBytes[byteIdx]) << 8) |
                              (static_cast<int32_t>(pcmBytes[byteIdx + 1]) << 16) |
                              (static_cast<int32_t>(pcmBytes[byteIdx + 2]) << 24);
                val >>= 8; // sign extend 24-bit
                const double smp = static_cast<double>(val) * kNorm24;
                streamer.pushSample(ch, s, smp);
            }
        }
        streamer.advanceWritePos(numSamples);
    }

    void bridgeInterleavedPCM24Outputs(uint8_t* pcmBytes, int numChannels, int numSamples) noexcept {
        if (!enabled.load(std::memory_order_relaxed) || pcmBytes == nullptr || numSamples <= 0) return;
        const int chLimit = std::min(numChannels, DAWStreamingInterface::kDAWChannels);
        constexpr double kScale24 = 8388607.0; // 2^23 - 1
        for (int s = 0; s < numSamples; ++s) {
            for (int ch = 0; ch < chLimit; ++ch) {
                const double smp = streamer.pullSample(ch, s);
                const int32_t val = std::clamp(static_cast<int32_t>(smp * kScale24), -8388648, 8388607);
                const size_t byteIdx = (static_cast<size_t>(s) * static_cast<size_t>(numChannels) + static_cast<size_t>(ch)) * 3;
                pcmBytes[byteIdx]     = static_cast<uint8_t>(val & 0xFF);
                pcmBytes[byteIdx + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
                pcmBytes[byteIdx + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
            }
        }
        streamer.advanceReadPos(numSamples);
    }

    // Round-trip latency calculation in milliseconds
    double getRoundTripLatencyMs() const noexcept {
        const double sr = sampleRate.load(std::memory_order_relaxed);
        const int buf = hwBufferSize.load(std::memory_order_relaxed);
        const int dawLat = streamer.getLatencySamples();
        if (sr < 1000.0) return 0.0;
        return (2.0 * static_cast<double>(buf) + static_cast<double>(dawLat)) / sr * 1000.0;
    }

private:
    DAWStreamingInterface& streamer;
    std::atomic<bool> enabled{false};
    std::atomic<double> sampleRate{48000.0};
    std::atomic<int> hwBufferSize{256};
};

} // namespace nova::graviton
