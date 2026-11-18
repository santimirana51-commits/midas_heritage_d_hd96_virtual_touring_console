#pragma once
/*  GravitonSpectrogram.h  –  Full-Screen Spectrogram & 3D Waterfall Analyzer
    ==========================================================================
    Professional-grade frequency analysis inspired by Smaart / MIDAS HD96 RTA.
    
    Features:
    - Real-time spectrogram (time × frequency × magnitude heatmap)
    - 3D waterfall display (cascading FFT frames)
    - Dual-channel comparison (A/B overlay)
    - Peak hold with adjustable decay
    - Freeze / snapshot capture
    - Adjustable FFT size (256 to 16384)
    - Windowing: Hann, Hamming, Blackman, Flat-Top, Kaiser
    - Logarithmic & linear frequency scale
    - Octave band smoothing (1/1, 1/3, 1/6, 1/12, 1/24)
    - Transfer function (magnitude + phase)
    - Coherence display
    - SPL weighting (A, C, Z)
    - CSV / PNG export
*/

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include <complex>
#include <algorithm>
#include <deque>
#include <mutex>
#include <numeric>

namespace nova {

// ─── Configuration ────────────────────────────────────────────
enum class WindowFunction {
    Hann = 0, Hamming, Blackman, FlatTop, Kaiser, Rectangular
};

enum class FreqScale {
    Logarithmic = 0, Linear
};

enum class OctaveSmoothing {
    None = 0, Oct_1, Oct_1_3, Oct_1_6, Oct_1_12, Oct_1_24
};

enum class WeightingCurve {
    Z = 0,  // Flat / unweighted
    A,
    C
};

enum class SpectrogramMode {
    RTA = 0,        // Standard RTA bars
    Spectrogram,    // Time-frequency heatmap
    Waterfall3D,    // Cascading 3D frames
    TransferFunction,
    Coherence
};

// ─── FFT Frame ────────────────────────────────────────────────
struct FFTFrame {
    std::vector<float> magnitudeDB;   // Per-bin magnitude in dB
    std::vector<float> phase;          // Per-bin phase in radians
    double timestamp = 0.0;            // Seconds since start
    float peakFreq = 0.0f;
    float peakDB = -120.0f;
    float rmsDB = -120.0f;
};

// ─── Spectrogram Engine ──────────────────────────────────────
class SpectrogramEngine {
public:
    static constexpr int MAX_FFT_SIZE = 16384;
    static constexpr int MIN_FFT_SIZE = 256;
    static constexpr int MAX_HISTORY = 512;   // Waterfall depth
    static constexpr int NUM_BINS_DISPLAY = 1024;
    
    SpectrogramEngine() {
        setFFTSize(4096);
        generateWindow();
    }
    
    // ── Configuration ──
    void setFFTSize(int size) {
        size = juce::jlimit(MIN_FFT_SIZE, MAX_FFT_SIZE, size);
        // Round to power of 2
        int p = 1;
        while (p < size) p <<= 1;
        fftSize_ = p;
        halfSize_ = p / 2;
        inputBuffer_.resize(fftSize_, 0.0f);
        fftBuffer_.resize(fftSize_ * 2, 0.0f);
        window_.resize(fftSize_, 0.0f);
        magnitudeDB_.resize(halfSize_, -120.0f);
        phase_.resize(halfSize_, 0.0f);
        peakHold_.resize(halfSize_, -120.0f);
        publishedMagnitudeDB_.assign(halfSize_, -120.0f);
        publishedPhase_.assign(halfSize_, 0.0f);
        publishedPeakHold_.assign(halfSize_, -120.0f);
        refBuffer_.assign(fftSize_, 0.0f);
        for (auto &slot : historyFrames_) {
            slot.frame.magnitudeDB.assign(halfSize_, -120.0f);
            slot.frame.phase.assign(halfSize_, 0.0f);
            slot.sequence.store(0, std::memory_order_relaxed);
            slot.state.store(0, std::memory_order_relaxed);
        }
        historyWriteSequence_ = 0;
        historyPublishedCount_.store(0, std::memory_order_relaxed);
        inputPos_ = 0;
        refPos_ = 0;
        generateWindow();
    }
    
    int getFFTSize() const { return fftSize_; }
    int getNumBins() const { return halfSize_; }
    
    void setSampleRate(double sr) {
        if (std::isfinite(sr) && sr > 1000.0)
            sampleRate_.store(sr, std::memory_order_relaxed);
    }
    double getSampleRate() const { return sampleRate_.load(std::memory_order_relaxed); }
    
    void setWindowFunction(WindowFunction wf) {
        windowFunc_ = wf;
        generateWindow();
    }
    
    void setFreqScale(FreqScale fs) { freqScale_ = fs; }
    void setSmoothing(OctaveSmoothing s) { smoothing_ = s; }
    void setWeighting(WeightingCurve w) { weighting_.store(w, std::memory_order_relaxed); }
    void setMode(SpectrogramMode m) { mode_ = m; }
    void setRangeDB(float minDB, float maxDB) { minDB_ = minDB; maxDB_ = maxDB; }
    
    SpectrogramMode getMode() const { return mode_; }
    FreqScale getFreqScale() const { return freqScale_; }
    OctaveSmoothing getSmoothing() const { return smoothing_; }
    WeightingCurve getWeighting() const { return weighting_; }
    WindowFunction getWindowFunction() const { return windowFunc_; }
    float getMinDB() const { return minDB_; }
    float getMaxDB() const { return maxDB_; }
    
    // ── Peak Hold ──
    void setPeakHoldEnabled(bool en) { peakHoldEnabled_.store(en, std::memory_order_relaxed); }
    void setPeakDecayRate(float dbPerSec) { peakDecayRate_.store(std::max(0.0f, dbPerSec), std::memory_order_relaxed); }
    void resetPeakHold() { resetPeakHoldRequested_.store(true, std::memory_order_release); }
    bool isPeakHoldEnabled() const { return peakHoldEnabled_.load(std::memory_order_relaxed); }
    
    // ── Freeze ──
    void setFreeze(bool f) { frozen_.store(f, std::memory_order_relaxed); }
    bool isFrozen() const { return frozen_.load(std::memory_order_relaxed); }
    
    // ── Push Audio Samples ──
    // Single audio-producer entry point. Configuration changes require the
    // producer to be stopped. This path performs no locking or allocation.
    void pushSamples(const float* data, int numSamples) noexcept {
        if (frozen_.load(std::memory_order_relaxed) || data == nullptr || numSamples <= 0) return;
        for (int i = 0; i < numSamples; ++i) {
            inputBuffer_[static_cast<size_t>(inputPos_)] = data[i];
            if (++inputPos_ >= fftSize_) { processFFT(); inputPos_ = 0; }
        }
    }
    
    // Push reference channel for transfer function
    void pushReferenceSamples(const float* data, int numSamples) noexcept {
        if (frozen_.load(std::memory_order_relaxed) || data == nullptr || numSamples <= 0) return;
        for (int i = 0; i < numSamples; ++i) {
            refBuffer_[static_cast<size_t>(refPos_)] = data[i];
            if (++refPos_ >= fftSize_) refPos_ = 0;
        }
    }
    
    // ── Get Current Magnitude (smoothed, weighted) ──
    // UI-facing getters return copies. The producer never waits for a reader.
    std::vector<float> getMagnitudeDB() const { return copyPublished(publishedMagnitudeDB_); }
    std::vector<float> getPhase() const { return copyPublished(publishedPhase_); }
    std::vector<float> getPeakHold() const { return copyPublished(publishedPeakHold_); }

    // ── Waterfall History ──
    int getHistoryDepth() const {
        return static_cast<int>(std::min<uint64_t>(
            historyPublishedCount_.load(std::memory_order_acquire), MAX_HISTORY));
    }

    FFTFrame getHistoryFrame(int idx) const {
        const uint64_t published = historyPublishedCount_.load(std::memory_order_acquire);
        const uint64_t depth = std::min<uint64_t>(published, MAX_HISTORY);
        if (idx < 0 || static_cast<uint64_t>(idx) >= depth)
            return {};
        const uint64_t sequence = published - depth + static_cast<uint64_t>(idx) + 1;
        auto &slot = historyFrames_[static_cast<size_t>((sequence - 1) % MAX_HISTORY)];
        int expected = 0;
        if (!slot.state.compare_exchange_strong(expected, 2, std::memory_order_acquire))
            return {};
        FFTFrame copy;
        if (slot.sequence.load(std::memory_order_acquire) == sequence)
            copy = slot.frame;
        slot.state.store(0, std::memory_order_release);
        return copy;
    }

    std::deque<FFTFrame> getHistory() const {
        std::deque<FFTFrame> snapshot;
        const int depth = getHistoryDepth();
        for (int i = 0; i < depth; ++i) {
            auto frame = getHistoryFrame(i);
            if (!frame.magnitudeDB.empty())
                snapshot.push_back(std::move(frame));
        }
        return snapshot;
    }
    
    // ── Frequency for bin ──
    float binToFreq(int bin) const {
        return static_cast<float>(bin) * static_cast<float>(sampleRate_.load(std::memory_order_relaxed)) / static_cast<float>(fftSize_);
    }
    
    int freqToBin(float freq) const {
        return static_cast<int>(freq * static_cast<float>(fftSize_) / static_cast<float>(sampleRate_.load(std::memory_order_relaxed)));
    }
    
    // ── Statistics ──
    float getCurrentPeakFreq() const { return currentPeakFreq_.load(std::memory_order_relaxed); }
    float getCurrentPeakDB() const { return currentPeakDB_.load(std::memory_order_relaxed); }
    float getCurrentRMSdB() const { return currentRMSdB_.load(std::memory_order_relaxed); }
    int getFrameCount() const { return frameCount_.load(std::memory_order_relaxed); }
    
    // ── Export ──
    juce::String exportCSV() const {
        const auto magnitude = getMagnitudeDB();
        const auto phase = getPhase();
        juce::String csv = "Frequency_Hz,Magnitude_dB,Phase_rad\n";
        const int bins = std::min(static_cast<int>(magnitude.size()),
                                  static_cast<int>(phase.size()));
        for (int i = 0; i < bins; ++i) {
            csv += juce::String(binToFreq(i), 2) + ","
                 + juce::String(magnitude[static_cast<size_t>(i)], 2) + ","
                 + juce::String(phase[static_cast<size_t>(i)], 4) + "\n";
        }
        return csv;
    }
    
    // ── A-Weighting curve (dB offset per frequency) ──
    static float aWeightDB(float freq) {
        if (freq < 10.0f) return -120.0f;
        double f2 = (double)freq * (double)freq;
        double num = 12194.0 * 12194.0 * f2 * f2;
        double den = (f2 + 20.6 * 20.6)
                   * std::sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9))
                   * (f2 + 12194.0 * 12194.0);
        double ra = num / den;
        return static_cast<float>(20.0 * std::log10(ra) + 2.0);
    }
    
    static float cWeightDB(float freq) {
        if (freq < 10.0f) return -120.0f;
        double f2 = (double)freq * (double)freq;
        double num = 12194.0 * 12194.0 * f2;
        double den = (f2 + 20.6 * 20.6) * (f2 + 12194.0 * 12194.0);
        double rc = num / den;
        return static_cast<float>(20.0 * std::log10(rc) + 0.06);
    }
    
    // ── Reset ──
    // Call only while the audio producer is stopped.
    void reset() {
        std::fill(inputBuffer_.begin(), inputBuffer_.end(), 0.0f);
        std::fill(magnitudeDB_.begin(), magnitudeDB_.end(), -120.0f);
        std::fill(phase_.begin(), phase_.end(), 0.0f);
        std::fill(peakHold_.begin(), peakHold_.end(), -120.0f);
        std::fill(publishedMagnitudeDB_.begin(), publishedMagnitudeDB_.end(), -120.0f);
        std::fill(publishedPhase_.begin(), publishedPhase_.end(), 0.0f);
        std::fill(publishedPeakHold_.begin(), publishedPeakHold_.end(), -120.0f);
        for (auto &slot : historyFrames_) {
            slot.sequence.store(0, std::memory_order_relaxed);
            slot.state.store(0, std::memory_order_relaxed);
        }
        inputPos_ = refPos_ = 0;
        frameCount_.store(0, std::memory_order_relaxed);
        historyWriteSequence_ = 0;
        historyPublishedCount_.store(0, std::memory_order_relaxed);
    }
    
private:
    void generateWindow() {
        window_.resize(fftSize_);
        for (int i = 0; i < fftSize_; ++i) {
            float n = static_cast<float>(i) / static_cast<float>(fftSize_ - 1);
            switch (windowFunc_) {
                case WindowFunction::Hann:
                    window_[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * n));
                    break;
                case WindowFunction::Hamming:
                    window_[i] = 0.54f - 0.46f * std::cos(2.0f * juce::MathConstants<float>::pi * n);
                    break;
                case WindowFunction::Blackman:
                    window_[i] = 0.42f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * n)
                                + 0.08f * std::cos(4.0f * juce::MathConstants<float>::pi * n);
                    break;
                case WindowFunction::FlatTop:
                    window_[i] = 0.21557895f
                                - 0.41663158f * std::cos(2.0f * juce::MathConstants<float>::pi * n)
                                + 0.277263158f * std::cos(4.0f * juce::MathConstants<float>::pi * n)
                                - 0.083578947f * std::cos(6.0f * juce::MathConstants<float>::pi * n)
                                + 0.006947368f * std::cos(8.0f * juce::MathConstants<float>::pi * n);
                    break;
                case WindowFunction::Kaiser: {
                    float alpha = 3.0f;
                    float x = 2.0f * n - 1.0f;
                    window_[i] = besselI0(alpha * std::sqrt(1.0f - x * x)) / besselI0(alpha);
                    break;
                }
                case WindowFunction::Rectangular:
                    window_[i] = 1.0f;
                    break;
            }
        }
    }
    
    static float besselI0(float x) {
        float sum = 1.0f;
        float term = 1.0f;
        for (int k = 1; k <= 20; ++k) {
            term *= (x * x) / (4.0f * static_cast<float>(k * k));
            sum += term;
        }
        return sum;
    }
    
    void processFFT() {
        // Apply window
        for (int i = 0; i < fftSize_; ++i) {
            fftBuffer_[i * 2] = inputBuffer_[i] * window_[i];
            fftBuffer_[i * 2 + 1] = 0.0f;
        }
        
        // In-place radix-2 DIT FFT
        performFFT(fftBuffer_.data(), fftSize_);
        
        // Calculate magnitude and phase
        float peakMag = 0.0f;
        int peakBin = 0;
        float sumSquares = 0.0f;
        
        for (int i = 0; i < halfSize_; ++i) {
            float re = fftBuffer_[i * 2];
            float im = fftBuffer_[i * 2 + 1];
            float mag = std::sqrt(re * re + im * im) / static_cast<float>(fftSize_);
            
            float db = (mag > 1e-10f) ? 20.0f * std::log10(mag) : -120.0f;
            
            // Apply weighting
            float freq = binToFreq(i);
            const auto weighting = weighting_.load(std::memory_order_relaxed);
            if (weighting == WeightingCurve::A) db += aWeightDB(freq);
            else if (weighting == WeightingCurve::C) db += cWeightDB(freq);
            
            magnitudeDB_[i] = db;
            phase_[i] = std::atan2(im, re);
            
            if (mag > peakMag) {
                peakMag = mag;
                peakBin = i;
            }
            sumSquares += mag * mag;
        }
        
        currentPeakFreq_ = binToFreq(peakBin);
        currentPeakDB_ = magnitudeDB_[peakBin];
        float rms = std::sqrt(sumSquares / static_cast<float>(halfSize_));
        currentRMSdB_ = (rms > 1e-10f) ? 20.0f * std::log10(rms) : -120.0f;
        
        // Peak hold is audio-thread-owned; UI requests are applied here.
        if (resetPeakHoldRequested_.exchange(false, std::memory_order_acq_rel))
            std::fill(peakHold_.begin(), peakHold_.end(), -120.0f);
        if (peakHoldEnabled_.load(std::memory_order_relaxed)) {
            const float decay = peakDecayRate_.load(std::memory_order_relaxed) / 30.0f;
            for (int i = 0; i < halfSize_; ++i) {
                if (magnitudeDB_[static_cast<size_t>(i)] > peakHold_[static_cast<size_t>(i)])
                    peakHold_[static_cast<size_t>(i)] = magnitudeDB_[static_cast<size_t>(i)];
                else
                    peakHold_[static_cast<size_t>(i)] -= decay;
            }
        }

        const int frameNumber = frameCount_.fetch_add(1, std::memory_order_relaxed);
        currentPeakFreq_.store(binToFreq(peakBin), std::memory_order_relaxed);
        currentPeakDB_.store(magnitudeDB_[static_cast<size_t>(peakBin)], std::memory_order_relaxed);
        const float rms = std::sqrt(sumSquares / static_cast<float>(halfSize_));
        currentRMSdB_.store((rms > 1e-10f) ? 20.0f * std::log10(rms) : -120.0f,
                             std::memory_order_relaxed);

        // Publish current spectrum only when the UI is not copying it.
        int expectedSpectrumState = 0;
        if (spectrumState_.compare_exchange_strong(expectedSpectrumState, 1,
                                                   std::memory_order_acquire)) {
            std::copy(magnitudeDB_.begin(), magnitudeDB_.end(), publishedMagnitudeDB_.begin());
            std::copy(phase_.begin(), phase_.end(), publishedPhase_.begin());
            std::copy(peakHold_.begin(), peakHold_.end(), publishedPeakHold_.begin());
            spectrumState_.store(0, std::memory_order_release);
        }

        // Fixed-capacity waterfall ring. Every frame's vectors are preallocated
        // by setFFTSize(); a busy UI reader causes this frame to be dropped.
        const uint64_t sequence = historyWriteSequence_ + 1;
        auto &slot = historyFrames_[static_cast<size_t>((sequence - 1) % MAX_HISTORY)];
        int expectedSlotState = 0;
        if (slot.state.compare_exchange_strong(expectedSlotState, 1,
                                               std::memory_order_acquire)) {
            std::copy(magnitudeDB_.begin(), magnitudeDB_.end(), slot.frame.magnitudeDB.begin());
            std::copy(phase_.begin(), phase_.end(), slot.frame.phase.begin());
            slot.frame.timestamp = static_cast<double>(frameNumber) *
                                   static_cast<double>(fftSize_) /
                                   sampleRate_.load(std::memory_order_relaxed);
            slot.frame.peakFreq = currentPeakFreq_.load(std::memory_order_relaxed);
            slot.frame.peakDB = currentPeakDB_.load(std::memory_order_relaxed);
            slot.frame.rmsDB = currentRMSdB_.load(std::memory_order_relaxed);
            slot.sequence.store(sequence, std::memory_order_release);
            slot.state.store(0, std::memory_order_release);
            historyWriteSequence_ = sequence;
            historyPublishedCount_.store(sequence, std::memory_order_release);
        }
    }
    
    // Simple radix-2 DIT FFT (interleaved real/imag)
    static void performFFT(float* data, int n) {
        // Bit reversal
        int j = 0;
        for (int i = 0; i < n; ++i) {
            if (j > i) {
                std::swap(data[i * 2], data[j * 2]);
                std::swap(data[i * 2 + 1], data[j * 2 + 1]);
            }
            int m = n >> 1;
            while (m >= 1 && j >= m) { j -= m; m >>= 1; }
            j += m;
        }
        
        // Butterfly
        for (int step = 1; step < n; step <<= 1) {
            float angle = -juce::MathConstants<float>::pi / static_cast<float>(step);
            float wRe = std::cos(angle);
            float wIm = std::sin(angle);
            
            for (int group = 0; group < n; group += step * 2) {
                float tRe = 1.0f, tIm = 0.0f;
                for (int pair = 0; pair < step; ++pair) {
                    int idx1 = (group + pair) * 2;
                    int idx2 = (group + pair + step) * 2;
                    
                    float uRe = data[idx2] * tRe - data[idx2 + 1] * tIm;
                    float uIm = data[idx2] * tIm + data[idx2 + 1] * tRe;
                    
                    data[idx2] = data[idx1] - uRe;
                    data[idx2 + 1] = data[idx1 + 1] - uIm;
                    data[idx1] += uRe;
                    data[idx1 + 1] += uIm;
                    
                    float newTRe = tRe * wRe - tIm * wIm;
                    tIm = tRe * wIm + tIm * wRe;
                    tRe = newTRe;
                }
            }
        }
    }
    
    int fftSize_ = 4096;
    int halfSize_ = 2048;
    double sampleRate_ = 48000.0;
    
    std::vector<float> inputBuffer_;
    std::vector<float> fftBuffer_;
    std::vector<float> window_;
    std::vector<float> magnitudeDB_;
    std::vector<float> phase_;
    std::vector<float> peakHold_;
    int inputPos_ = 0;
    
    // Reference channel
    std::vector<float> refBuffer_;
    int refPos_ = 0;
    
    // History for waterfall
    std::deque<FFTFrame> history_;
    int frameCount_ = 0;
    
    // Settings
    WindowFunction windowFunc_ = WindowFunction::Hann;
    FreqScale freqScale_ = FreqScale::Logarithmic;
    OctaveSmoothing smoothing_ = OctaveSmoothing::None;
    WeightingCurve weighting_ = WeightingCurve::Z;
    SpectrogramMode mode_ = SpectrogramMode::RTA;
    float minDB_ = -120.0f;
    float maxDB_ = 0.0f;
    
    // Peak hold
    bool peakHoldEnabled_ = true;
    float peakDecayRate_ = 6.0f;  // dB/sec
    
    bool frozen_ = false;
    
    float currentPeakFreq_ = 0.0f;
    float currentPeakDB_ = -120.0f;
    float currentRMSdB_ = -120.0f;
    
    mutable std::mutex mutex_;
};

} // namespace nova

