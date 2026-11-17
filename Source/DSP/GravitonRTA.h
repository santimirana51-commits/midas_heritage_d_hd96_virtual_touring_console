#pragma once
//==============================================================================
// GravitonRTA.h
// NOVA HD Digital Mixing System — Graviton Mk2 DSP Audio Engine
//
// Real-Time Dual-FFT RTA / Transfer Function Audio Analyzer
//
// Constraints:
//   - Zero heap allocation (fixed-size arrays only)
//   - Lock-free producer/consumer via std::atomic
//   - All DSP in double precision
//   - Cooley-Tukey radix-2 in-place FFT (Danielson-Lanczos)
//   - Hann window, 50% overlap, exponential averaging
//   - MSVC /W4 clean
//==============================================================================

#include <atomic>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace nova::graviton
{

//==============================================================================
/**
 * DualFFTAnalyzer
 *
 * Dual-channel Real-Time FFT spectrum / transfer-function analyser.
 *
 * Audio thread:  call pushSamplePair() per sample pair.
 * GUI thread:    poll hasNewData(), then call getSpectrumData() to consume.
 *
 * All memory is statically allocated — zero heap usage, real-time safe.
 */
class DualFFTAnalyzer final
{
public:
    //==========================================================================
    // Compile-time constants
    //==========================================================================
    static constexpr int kFFTSize  = 2048;
    static constexpr int kFFTHalf  = kFFTSize / 2;
    static constexpr int kRingSize = kFFTSize * 4;   // 8 192-sample ring buffer
    static constexpr int kRingMask = kRingSize - 1;  // power-of-2 wrap mask

    //==========================================================================
    // Construction / initialisation
    //==========================================================================
    DualFFTAnalyzer() noexcept
    {
        buildHannWindow();
        reset();
    }

    ~DualFFTAnalyzer() = default;

    // Non-copyable, non-movable (atomics + large fixed arrays)
    DualFFTAnalyzer(const DualFFTAnalyzer&)            = delete;
    DualFFTAnalyzer& operator=(const DualFFTAnalyzer&) = delete;
    DualFFTAnalyzer(DualFFTAnalyzer&&)                 = delete;
    DualFFTAnalyzer& operator=(DualFFTAnalyzer&&)      = delete;

    //==========================================================================
    // Configuration (call before audio starts, or from a non-audio thread
    // while the audio thread is idle)
    //==========================================================================

    /** Set the sample rate so getBinFrequency() returns correct values. */
    void setSampleRate(double sr) noexcept
    {
        sampleRate = (sr > 0.0) ? sr : 44100.0;
    }

    /** Set exponential averaging window (1 = no averaging, 16 = maximum). */
    void setAveraging(int frames) noexcept
    {
        averagingFrames = std::clamp(frames, 1, 16);
    }

    /** Reset all ring buffers, running averages, and state. */
    void reset() noexcept
    {
        writePos.store(0, std::memory_order_relaxed);
        sampleCount = 0;
        frameCount  = 0;
        newDataReady.store(false, std::memory_order_relaxed);

        for (int i = 0; i < kRingSize; ++i)
        {
            ringA[i] = 0.0;
            ringB[i] = 0.0;
        }
        for (int i = 0; i < kFFTSize; ++i)
        {
            windowedA[i] = 0.0;
            windowedB[i] = 0.0;
            fftReal[i]   = 0.0;
            fftImag[i]   = 0.0;
        }
        for (int i = 0; i < kFFTHalf; ++i)
        {
            spectrumA[i]   = -144.0;
            spectrumB[i]   = -144.0;
            coherence[i]   = 0.0;
            transferFunc[i] = 0.0;
            avgSpecA[i]    = -144.0;
            avgSpecB[i]    = -144.0;
        }
    }

    //==========================================================================
    // Audio thread API
    //==========================================================================

    /**
     * Push one stereo sample pair.
     * Call from audio thread only.  Lock-free, zero allocation.
     *
     * When kFFTSize/2 new samples have accumulated (50% overlap), an FFT
     * frame is computed and newDataReady is set.
     */
    void pushSamplePair(double chA, double chB) noexcept
    {
        const double safeA = std::isfinite(chA) ? chA : 0.0;
        const double safeB = std::isfinite(chB) ? chB : 0.0;

        // --- Write to ring buffers -------------------------------------------
        const int wp = writePos.load(std::memory_order_relaxed) & kRingMask;
        ringA[wp] = safeA;
        ringB[wp] = safeB;
        writePos.store(wp + 1, std::memory_order_relaxed);

        ++sampleCount;

        // Trigger an FFT frame every kFFTSize/2 samples (50% overlap)
        if (sampleCount < (kFFTSize / 2))
            return;

        sampleCount = 0;

        // --- Build windowed frames from ring buffer --------------------------
        // Current write position after the last write
        const int currentWp = writePos.load(std::memory_order_relaxed);

        for (int i = 0; i < kFFTSize; ++i)
        {
            // Walk backwards: most recent sample is at index kFFTSize-1
            const int ringIdx = (currentWp - kFFTSize + i) & kRingMask;
            windowedA[i] = ringA[ringIdx] * hannWindow[i];
            windowedB[i] = ringB[ringIdx] * hannWindow[i];
        }

        // --- FFT channel A --------------------------------------------------
        for (int i = 0; i < kFFTSize; ++i)
        {
            fftReal[i] = windowedA[i];
            fftImag[i] = 0.0;
        }
        performFFT(fftReal, fftImag, kFFTSize);

        const double normA = 2.0 / static_cast<double>(kFFTSize); // two-sided → one-sided
        for (int i = 0; i < kFFTHalf; ++i)
        {
            const double mag = std::sqrt(fftReal[i] * fftReal[i] +
                                         fftImag[i] * fftImag[i]) * normA;
            spectrumA[i] = 20.0 * std::log10(mag + 1e-12);
        }

        // --- FFT channel B --------------------------------------------------
        for (int i = 0; i < kFFTSize; ++i)
        {
            fftReal[i] = windowedB[i];
            fftImag[i] = 0.0;
        }
        performFFT(fftReal, fftImag, kFFTSize);

        for (int i = 0; i < kFFTHalf; ++i)
        {
            const double mag = std::sqrt(fftReal[i] * fftReal[i] +
                                         fftImag[i] * fftImag[i]) * normA;
            spectrumB[i] = 20.0 * std::log10(mag + 1e-12);
        }

        // --- Exponential averaging ------------------------------------------
        const double alpha = 1.0 / static_cast<double>(averagingFrames);

        for (int i = 0; i < kFFTHalf; ++i)
        {
            avgSpecA[i] = avgSpecA[i] * (1.0 - alpha) + spectrumA[i] * alpha;
            avgSpecB[i] = avgSpecB[i] * (1.0 - alpha) + spectrumB[i] * alpha;
        }

        // --- Coherence (simplified magnitude-based proxy) -------------------
        // Real coherence requires complex cross-spectrum accumulation; this
        // proxy gives an intuitive "how similar are the two spectra" metric.
        for (int i = 0; i < kFFTHalf; ++i)
        {
            coherence[i] = (avgSpecA[i] > -80.0 && avgSpecB[i] > -80.0)
                ? std::max(0.0, 1.0 - std::abs(avgSpecB[i] - avgSpecA[i]) / 20.0)
                : 0.0;
        }

        // --- Transfer function (B/A in dB) ----------------------------------
        for (int i = 0; i < kFFTHalf; ++i)
            transferFunc[i] = avgSpecB[i] - avgSpecA[i];

        ++frameCount;

        // Signal the GUI thread
        newDataReady.store(true, std::memory_order_release);
    }

    //==========================================================================
    // GUI thread API
    //==========================================================================

    /** Returns true if a new FFT frame is ready to be consumed. */
    bool hasNewData() const noexcept
    {
        return newDataReady.load(std::memory_order_acquire);
    }

    /**
     * Copies the latest averaged spectra to caller-supplied arrays and clears
     * the newDataReady flag.
     *
     * @param outA         Destination for channel-A magnitude (dBFS), kFFTHalf doubles.
     * @param outB         Destination for channel-B magnitude (dBFS), kFFTHalf doubles.
     * @param outCoherence Destination for coherence values [0..1],         kFFTHalf doubles.
     * @param outTF        Destination for transfer function (dB),           kFFTHalf doubles.
     * @param outBins      Set to kFFTHalf on return.
     *
     * Safe to call even when hasNewData() is false; outBins will still be
     * set and the last valid data will be copied.
     */
    void getSpectrumData(double* outA,
                         double* outB,
                         double* outCoherence,
                         double* outTF,
                         int&    outBins) noexcept
    {
        outBins = kFFTHalf;

        for (int i = 0; i < kFFTHalf; ++i)
        {
            outA[i]         = avgSpecA[i];
            outB[i]         = avgSpecB[i];
            outCoherence[i] = coherence[i];
            outTF[i]        = transferFunc[i];
        }

        newDataReady.store(false, std::memory_order_release);
    }

    /**
     * Returns the centre frequency of FFT bin @p bin in Hz.
     * Bin 0 = DC, bin kFFTHalf-1 = Nyquist-1.
     */
    double getBinFrequency(int bin) const noexcept
    {
        return static_cast<double>(bin) * sampleRate / static_cast<double>(kFFTSize);
    }

    /** Returns the number of complete FFT frames processed so far. */
    int getFrameCount() const noexcept { return frameCount; }

private:
    //==========================================================================
    // Ring buffers  (producer: audio thread)
    //==========================================================================
    double ringA[kRingSize]{};
    double ringB[kRingSize]{};

    std::atomic<int> writePos{0};

    //==========================================================================
    // Windowed input staging
    //==========================================================================
    double windowedA[kFFTSize]{};
    double windowedB[kFFTSize]{};

    //==========================================================================
    // FFT work arrays (reused each frame, members → no stack allocation)
    //==========================================================================
    double fftReal[kFFTSize]{};
    double fftImag[kFFTSize]{};

    //==========================================================================
    // Per-frame magnitude spectra (dBFS)
    //==========================================================================
    double spectrumA[kFFTHalf]{};
    double spectrumB[kFFTHalf]{};

    //==========================================================================
    // Running averages, coherence, transfer function  (consumed by GUI)
    //==========================================================================
    double avgSpecA[kFFTHalf]{};
    double avgSpecB[kFFTHalf]{};
    double coherence[kFFTHalf]{};
    double transferFunc[kFFTHalf]{};

    //==========================================================================
    // Hann window (precomputed at construction)
    //==========================================================================
    double hannWindow[kFFTSize]{};

    //==========================================================================
    // State
    //==========================================================================
    double           sampleRate     { 44100.0 };
    int              averagingFrames{ 4       };
    int              sampleCount    { 0       };
    int              frameCount     { 0       };
    std::atomic<bool> newDataReady  { false   };

    //==========================================================================
    // Private helpers
    //==========================================================================

    /** Pre-compute the Hann analysis window. */
    void buildHannWindow() noexcept
    {
        constexpr double twoPi = 2.0 * std::numbers::pi;
        const double     N1    = static_cast<double>(kFFTSize - 1);

        for (int i = 0; i < kFFTSize; ++i)
            hannWindow[i] = 0.5 * (1.0 - std::cos(twoPi * static_cast<double>(i) / N1));
    }

    /**
     * In-place Cooley-Tukey radix-2 DIT FFT (Danielson-Lanczos).
     *
     * @param real  Real part array, length N (must be power of 2).
     * @param imag  Imaginary part array, length N.
     * @param N     Transform size (must equal kFFTSize for member arrays).
     *
     * Result is in natural (not bit-reversed) order.
     * Uses member arrays fftReal / fftImag; not re-entrant.
     */
    void performFFT(double* real, double* imag, int N) noexcept
    {
        // --- Bit-reversal permutation ---------------------------------------
        {
            int j = 0;
            for (int i = 1; i < N; ++i)
            {
                int bit = N >> 1;
                for (; j & bit; bit >>= 1)
                    j ^= bit;
                j ^= bit;

                if (i < j)
                {
                    // swap real
                    double tmp = real[i]; real[i] = real[j]; real[j] = tmp;
                    // swap imag
                            tmp = imag[i]; imag[i] = imag[j]; imag[j] = tmp;
                }
            }
        }

        // --- Danielson-Lanczos butterfly ------------------------------------
        for (int len = 2; len <= N; len <<= 1)
        {
            const double ang  = -2.0 * std::numbers::pi / static_cast<double>(len);
            const double wRe  =  std::cos(ang);
            const double wIm  =  std::sin(ang);

            for (int i = 0; i < N; i += len)
            {
                double uRe = 1.0;
                double uIm = 0.0;

                const int half = len >> 1;
                for (int k = 0; k < half; ++k)
                {
                    const int p = i + k;
                    const int q = p + half;

                    // twiddle × X[q]
                    const double tRe = uRe * real[q] - uIm * imag[q];
                    const double tIm = uRe * imag[q] + uIm * real[q];

                    real[q] = real[p] - tRe;
                    imag[q] = imag[p] - tIm;
                    real[p] = real[p] + tRe;
                    imag[p] = imag[p] + tIm;

                    // advance twiddle factor
                    const double newUre = uRe * wRe - uIm * wIm;
                    uIm                 = uRe * wIm + uIm * wRe;
                    uRe                 = newUre;
                }
            }
        }
    }
};

} // namespace nova::graviton
