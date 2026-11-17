#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace nova {

class RecordingWorker final : public juce::Thread {
public:
  static constexpr int kMaxRecordChannels = 64;
  static constexpr int fifoCapacity = 1 << 17; // 131,072 frames per channel (~1.36s @ 96kHz, ~2.73s @ 48kHz)
  static constexpr int writerChunk = 4096;

  enum class RecordMode {
    StereoMaster = 2,
    Multitrack64 = 64
  };

  RecordingWorker()
      : juce::Thread("NOVA WAV Recorder"), fifo(fifoCapacity),
        planarRings(static_cast<size_t>(kMaxRecordChannels * fifoCapacity), 0.0f) {}

  ~RecordingWorker() override { stopRecording(); }

  bool startRecording(const juce::File &file, double sampleRate, int numChannelsToRecord = 2) {
    stopRecording();
    if (sampleRate < 8000.0 || sampleRate > 384000.0)
      return false;

    const int actualChannels = (numChannelsToRecord >= 64) ? 64 : 2;
    activeChannels.store(actualChannels, std::memory_order_relaxed);

    auto stream = file.createOutputStream();
    if (stream == nullptr)
      return false;

    juce::WavAudioFormat wav;
    // Standard Broadcast Polyphonic WAV format (2 to 64 channels seamlessly)
    writer.reset(
        wav.createWriterFor(stream.release(), sampleRate,
                            static_cast<unsigned int>(actualChannels), 24, {}, 0));
    if (writer == nullptr)
      return false;

    fifo.reset();
    droppedFrames.store(0, std::memory_order_relaxed);
    recordedFrames.store(0, std::memory_order_relaxed);
    recording.store(true, std::memory_order_relaxed);
    startThread();
    return isThreadRunning();
  }

  // Push Stereo Master L/R
  void push(const float *left, const float *right, int numSamples) noexcept {
    pushStereo(left, right, numSamples);
  }

  void pushStereo(const float *left, const float *right, int numSamples) noexcept {
    if (!recording.load(std::memory_order_relaxed) || left == nullptr ||
        right == nullptr || numSamples <= 0 ||
        activeChannels.load(std::memory_order_relaxed) != 2)
      return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    const int accepted = size1 + size2;

    float *ch0 = planarRings.data();
    float *ch1 = planarRings.data() + fifoCapacity;

    for (int i = 0; i < size1; ++i) {
      ch0[start1 + i] = left[i];
      ch1[start1 + i] = right[i];
    }
    for (int i = 0; i < size2; ++i) {
      ch0[start2 + i] = left[size1 + i];
      ch1[start2 + i] = right[size1 + i];
    }
    fifo.finishedWrite(accepted);
    if (accepted < numSamples)
      droppedFrames.fetch_add(static_cast<uint64_t>(numSamples - accepted),
                              std::memory_order_relaxed);
  }

  // Push 64-Channel Multitrack RAW
  void pushMultitrack(const float *const *channels, int numChannels, int numSamples) noexcept {
    if (!recording.load(std::memory_order_relaxed) || channels == nullptr ||
        numChannels <= 0 || numSamples <= 0)
      return;

    const int active = activeChannels.load(std::memory_order_relaxed);
    if (active != 64)
      return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    const int accepted = size1 + size2;
    const int chToCopy = std::min(numChannels, kMaxRecordChannels);

    for (int ch = 0; ch < kMaxRecordChannels; ++ch) {
      const float *src = (ch < chToCopy) ? channels[ch] : nullptr;
      float *dst = planarRings.data() + (static_cast<size_t>(ch) * fifoCapacity);
      if (src != nullptr) {
        for (int i = 0; i < size1; ++i)
          dst[start1 + i] = src[i];
        for (int i = 0; i < size2; ++i)
          dst[start2 + i] = src[size1 + i];
      } else {
        for (int i = 0; i < size1; ++i)
          dst[start1 + i] = 0.0f;
        for (int i = 0; i < size2; ++i)
          dst[start2 + i] = 0.0f;
      }
    }

    fifo.finishedWrite(accepted);
    if (accepted < numSamples)
      droppedFrames.fetch_add(static_cast<uint64_t>(numSamples - accepted),
                              std::memory_order_relaxed);
  }

  void stopRecording() {
    recording.store(false, std::memory_order_relaxed);
    if (isThreadRunning()) {
      signalThreadShouldExit();
      notify();
      waitForThreadToExit(-1);
    }
    writer.reset();
  }

  bool isRecording() const noexcept {
    return recording.load(std::memory_order_relaxed);
  }
  bool isMultitrackActive() const noexcept {
    return recording.load(std::memory_order_relaxed) &&
           activeChannels.load(std::memory_order_relaxed) == 64;
  }
  int getActiveChannelCount() const noexcept {
    return activeChannels.load(std::memory_order_relaxed);
  }
  uint64_t getRecordedFrames() const noexcept {
    return recordedFrames.load(std::memory_order_relaxed);
  }
  uint64_t getDroppedFrames() const noexcept {
    return droppedFrames.load(std::memory_order_relaxed);
  }

  void run() override {
    const int channelsToRead = activeChannels.load(std::memory_order_relaxed);
    juce::AudioBuffer<float> chunk(channelsToRead, writerChunk);

    while (!threadShouldExit() || fifo.getNumReady() > 0) {
      int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
      fifo.prepareToRead(writerChunk, start1, size1, start2, size2);
      const int count = size1 + size2;
      if (count == 0) {
        wait(10);
        continue;
      }

      for (int ch = 0; ch < channelsToRead; ++ch) {
        const float *ring =
            planarRings.data() + (static_cast<size_t>(ch) * fifoCapacity);
        float *dst = chunk.getWritePointer(ch);
        for (int i = 0; i < size1; ++i)
          dst[i] = ring[start1 + i];
        for (int i = 0; i < size2; ++i)
          dst[size1 + i] = ring[start2 + i];
      }

      fifo.finishedRead(count);
      if (writer != nullptr) {
        if (!writer->writeFromAudioSampleBuffer(chunk, 0, count))
          droppedFrames.fetch_add(static_cast<uint64_t>(count),
                                  std::memory_order_relaxed);
        else
          recordedFrames.fetch_add(static_cast<uint64_t>(count),
                                   std::memory_order_relaxed);
      }
    }
  }

private:
  juce::AbstractFifo fifo;
  std::vector<float> planarRings;
  std::unique_ptr<juce::AudioFormatWriter> writer;
  std::atomic<bool> recording{false};
  std::atomic<int> activeChannels{2};
  std::atomic<uint64_t> recordedFrames{0}, droppedFrames{0};
};

} // namespace nova
