#pragma once

#include "Audio/HeritageVirtualShmBridge.h"
#include "DSP/GravitonDSP.h"
#include "Routing/GravitonPatchMatrix.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <memory>

namespace nova::graviton {

// ==============================================================================
// VBAN (VB-Audio Network) Protocol Helpers — 28-byte Header over UDP (Port 6980)
// ==============================================================================
#pragma pack(push, 1)
struct VbanHeader {
  char vban[4];             // "VBAN"
  uint8_t srAndProto;       // Bits 0-4: SR index, Bits 5-7: Sub-protocol (0=Audio)
  uint8_t nSamplesMinus1;   // 0..255 -> 1..256 samples per frame
  uint8_t nChannelsMinus1;  // 0..7   -> 1..8 channels
  uint8_t dataFormat;       // 0x01=INT16, 0x02=INT24, 0x04=FLOAT32
  char streamName[16];      // ASCII stream name
  uint32_t frameCounter;    // Sequential packet counter
};
#pragma pack(pop)
static_assert(sizeof(VbanHeader) == 28, "VBAN header must be exactly 28 bytes");

namespace vban_detail {
inline constexpr uint8_t kProtoAudio = 0x00;
inline constexpr uint8_t kFormatInt16 = 0x01;
inline constexpr uint8_t kFormatInt24 = 0x02;
inline constexpr uint8_t kFormatFloat32 = 0x04;
inline constexpr size_t kMaxUdpPayload = 1472;

inline constexpr std::array<double, 21> kSampleRates = {
    6000.0,  12000.0, 24000.0, 48000.0, 96000.0, 192000.0, 384000.0,
    8000.0,  11025.0, 22050.0, 44100.0, 88200.0, 176400.0, 352800.0,
    16000.0, 32000.0, 64000.0, 128000.0, 256000.0, 512000.0, 48000.0};

inline int sampleRateToIndex(double sr) noexcept {
  int bestIdx = 3; // 48000 Hz default
  double bestDiff = 1.0e12;
  for (int i = 0; i < 20; ++i) {
    const double diff = std::abs(kSampleRates[static_cast<size_t>(i)] - sr);
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIdx = i;
    }
  }
  return bestIdx;
}

inline double indexToSampleRate(uint8_t srIndex) noexcept {
  const uint8_t idx = srIndex & 0x1F;
  return (idx < kSampleRates.size()) ? kSampleRates[idx] : 48000.0;
}

inline size_t bytesPerSample(uint8_t fmt) noexcept {
  switch (fmt & 0x07) {
  case kFormatInt16:
    return 2;
  case kFormatInt24:
    return 3;
  case kFormatFloat32:
    return 4;
  default:
    return 0;
  }
}

inline size_t encodeAudioPacket(uint8_t *outBytes, size_t outCapacity,
                                  const float *inL, const float *inR,
                                  int numSamples, double sampleRate,
                                  uint8_t dataFormat, const char *streamName,
                                  uint32_t frameCounter) noexcept {
  if (outBytes == nullptr || inL == nullptr || numSamples <= 0 ||
      numSamples > 256)
    return 0;
  const int channels = (inR != nullptr) ? 2 : 1;
  const size_t bps = bytesPerSample(dataFormat);
  if (bps == 0)
    return 0;
  const size_t totalBytes =
      sizeof(VbanHeader) + static_cast<size_t>(numSamples * channels) * bps;
  if (totalBytes > outCapacity || totalBytes > kMaxUdpPayload)
    return 0;

  VbanHeader hdr{};
  hdr.vban[0] = 'V';
  hdr.vban[1] = 'B';
  hdr.vban[2] = 'A';
  hdr.vban[3] = 'N';
  hdr.srAndProto = static_cast<uint8_t>(sampleRateToIndex(sampleRate) & 0x1F);
  hdr.nSamplesMinus1 = static_cast<uint8_t>(numSamples - 1);
  hdr.nChannelsMinus1 = static_cast<uint8_t>(channels - 1);
  hdr.dataFormat = dataFormat;
  if (streamName != nullptr) {
    for (size_t i = 0; i < 16 && streamName[i] != '\0'; ++i)
      hdr.streamName[i] = streamName[i];
  }
  hdr.frameCounter = frameCounter;
  std::memcpy(outBytes, &hdr, sizeof(VbanHeader));

  uint8_t *payload = outBytes + sizeof(VbanHeader);
  for (int i = 0; i < numSamples; ++i) {
    for (int ch = 0; ch < channels; ++ch) {
      const float s = std::clamp(
          static_cast<float>(sanitize(ch == 0 ? inL[i] : inR[i])), -1.0f,
          1.0f);
      if ((dataFormat & 0x07) == kFormatFloat32) {
        std::memcpy(payload, &s, 4);
        payload += 4;
      } else if ((dataFormat & 0x07) == kFormatInt16) {
        const int16_t v = static_cast<int16_t>(
            std::clamp(std::lrintf(s * 32767.0f), -32768L, 32767L));
        std::memcpy(payload, &v, 2);
        payload += 2;
      } else if ((dataFormat & 0x07) == kFormatInt24) {
        const int32_t v = static_cast<int32_t>(
            std::clamp(std::lrintf(s * 8388607.0f), -8388608L, 8388607L));
        payload[0] = static_cast<uint8_t>(v & 0xFF);
        payload[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        payload[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        payload += 3;
      }
    }
  }
  return totalBytes;
}

struct DecodedVbanInfo {
  bool valid = false;
  double sampleRate = 48000.0;
  int numSamples = 0;
  int numChannels = 0;
  uint32_t frameCounter = 0;
  char streamName[17]{};
};

inline DecodedVbanInfo decodeAudioPacket(const uint8_t *inBytes,
                                         size_t inBytesLen, float *outL,
                                         float *outR,
                                         size_t maxOutSamples) noexcept {
  DecodedVbanInfo info;
  if (inBytes == nullptr || inBytesLen < sizeof(VbanHeader) || outL == nullptr ||
      outR == nullptr)
    return info;

  VbanHeader hdr{};
  std::memcpy(&hdr, inBytes, sizeof(VbanHeader));
  if (hdr.vban[0] != 'V' || hdr.vban[1] != 'B' || hdr.vban[2] != 'A' ||
      hdr.vban[3] != 'N')
    return info;
  if ((hdr.srAndProto & 0xE0) != kProtoAudio)
    return info;

  const int nSamples = static_cast<int>(hdr.nSamplesMinus1) + 1;
  const int nChannels = static_cast<int>(hdr.nChannelsMinus1) + 1;
  const size_t bps = bytesPerSample(hdr.dataFormat);
  if (bps == 0 || nChannels < 1 || nChannels > 8 ||
      static_cast<size_t>(nSamples) > maxOutSamples)
    return info;

  const size_t expectedBytes =
      sizeof(VbanHeader) + static_cast<size_t>(nSamples * nChannels) * bps;
  if (inBytesLen < expectedBytes)
    return info;

  const uint8_t *payload = inBytes + sizeof(VbanHeader);
  for (int i = 0; i < nSamples; ++i) {
    float sL = 0.0f;
    float sR = 0.0f;
    for (int ch = 0; ch < nChannels; ++ch) {
      float sample = 0.0f;
      const uint8_t *p =
          payload + static_cast<size_t>(i * nChannels + ch) * bps;
      if ((hdr.dataFormat & 0x07) == kFormatFloat32) {
        std::memcpy(&sample, p, 4);
      } else if ((hdr.dataFormat & 0x07) == kFormatInt16) {
        int16_t v = 0;
        std::memcpy(&v, p, 2);
        sample = static_cast<float>(v) / 32768.0f;
      } else if ((hdr.dataFormat & 0x07) == kFormatInt24) {
        int32_t v = static_cast<int32_t>(p[0]) |
                    (static_cast<int32_t>(p[1]) << 8) |
                    (static_cast<int32_t>(static_cast<int8_t>(p[2])) << 16);
        sample = static_cast<float>(v) / 8388608.0f;
      }
      sample = sanitize(sample);
      if (ch == 0)
        sL = sample;
      else if (ch == 1)
        sR = sample;
    }
    if (nChannels == 1)
      sR = sL;
    outL[i] = sL;
    outR[i] = sR;
  }

  info.valid = true;
  info.sampleRate = indexToSampleRate(hdr.srAndProto & 0x1F);
  info.numSamples = nSamples;
  info.numChannels = nChannels;
  info.frameCounter = hdr.frameCounter;
  std::memcpy(info.streamName, hdr.streamName, 16);
  info.streamName[16] = '\0';
  return info;
}
} // namespace vban_detail

// ==============================================================================
// Lock-Free SPSC Stereo Ring Buffer (Zero Allocation)
// ==============================================================================
template <size_t CapacityPow2 = 16384> class SpscStereoRingBuffer final {
public:
  static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0,
                "Ring capacity must be power of two");
  static constexpr size_t kMask = CapacityPow2 - 1;

  void reset() noexcept {
    writePos.store(0, std::memory_order_relaxed);
    readPos.store(0, std::memory_order_relaxed);
    underrunCount.store(0, std::memory_order_relaxed);
    overrunCount.store(0, std::memory_order_relaxed);
  }

  size_t availableRead() const noexcept {
    const size_t w = writePos.load(std::memory_order_acquire);
    const size_t r = readPos.load(std::memory_order_relaxed);
    return w - r;
  }

  size_t availableWrite() const noexcept {
    const size_t w = writePos.load(std::memory_order_relaxed);
    const size_t r = readPos.load(std::memory_order_acquire);
    return CapacityPow2 - (w - r);
  }

  size_t pushFrames(const float *inL, const float *inR,
                    size_t count) noexcept {
    if (inL == nullptr || count == 0)
      return 0;
    const size_t w = writePos.load(std::memory_order_relaxed);
    const size_t r = readPos.load(std::memory_order_acquire);
    const size_t freeSpace = CapacityPow2 - (w - r);
    const size_t toWrite = std::min(count, freeSpace);
    if (toWrite < count)
      overrunCount.fetch_add(1, std::memory_order_relaxed);

    for (size_t i = 0; i < toWrite; ++i) {
      const size_t idx = (w + i) & kMask;
      bufL[idx] = sanitize(inL[i]);
      bufR[idx] = sanitize(inR != nullptr ? inR[i] : inL[i]);
    }
    writePos.store(w + toWrite, std::memory_order_release);
    return toWrite;
  }

  size_t peekFrames(float *outL, float *outR, size_t maxCount) const noexcept {
    if (outL == nullptr || outR == nullptr || maxCount == 0)
      return 0;
    const size_t w = writePos.load(std::memory_order_acquire);
    const size_t r = readPos.load(std::memory_order_relaxed);
    const size_t avail = w - r;
    const size_t toPeek = std::min(maxCount, avail);
    for (size_t i = 0; i < toPeek; ++i) {
      const size_t idx = (r + i) & kMask;
      outL[i] = bufL[idx];
      outR[i] = bufR[idx];
    }
    return toPeek;
  }

  void advanceRead(size_t count) noexcept {
    if (count == 0)
      return;
    const size_t w = writePos.load(std::memory_order_acquire);
    const size_t r = readPos.load(std::memory_order_relaxed);
    const size_t avail = w - r;
    readPos.store(r + std::min(count, avail), std::memory_order_release);
  }

  size_t popFrames(float *outL, float *outR, size_t count) noexcept {
    if (outL == nullptr || outR == nullptr || count == 0)
      return 0;
    const size_t w = writePos.load(std::memory_order_acquire);
    const size_t r = readPos.load(std::memory_order_relaxed);
    const size_t avail = w - r;
    const size_t toRead = std::min(count, avail);
    for (size_t i = 0; i < toRead; ++i) {
      const size_t idx = (r + i) & kMask;
      outL[i] = bufL[idx];
      outR[i] = bufR[idx];
    }
    if (toRead < count) {
      std::fill(outL + toRead, outL + count, 0.0f);
      std::fill(outR + toRead, outR + count, 0.0f);
      if (avail == 0)
        underrunCount.fetch_add(1, std::memory_order_relaxed);
    }
    readPos.store(r + toRead, std::memory_order_release);
    return toRead;
  }

  std::atomic<uint64_t> underrunCount{0};
  std::atomic<uint64_t> overrunCount{0};

private:
  std::array<float, CapacityPow2> bufL{};
  std::array<float, CapacityPow2> bufR{};
  std::atomic<size_t> writePos{0};
  std::atomic<size_t> readPos{0};
};

// ==============================================================================
// Virtual Cable Slot (1 Stereo Send + 1 Stereo Return Pair)
// ==============================================================================
struct VirtualCableSlot final {
  static constexpr size_t kMaxBlockScratch = 4096;

  // Return Path (External / VBAN -> Heritage Mixer Channels)
  SpscStereoRingBuffer<16384> returnRing;
  std::atomic<bool> returnEnabled{false};
  std::atomic<float> returnGainDb{0.0f};
  std::atomic<bool> returnMuted{false};
  std::atomic<float> returnPeakL{0.0f};
  std::atomic<float> returnPeakR{0.0f};
  std::atomic<int> returnTargetChannelL{-1}; // 0..143, -1 = unassigned
  std::atomic<int> returnTargetChannelR{-1};
  std::atomic<double> returnSampleRate{48000.0};
  std::atomic<uint64_t> returnPacketsReceived{0};

  // Send Path (Heritage Mixer -> External / VBAN)
  SpscStereoRingBuffer<16384> sendRing;
  std::atomic<bool> sendEnabled{false};
  std::atomic<float> sendGainDb{0.0f};
  std::atomic<bool> sendMuted{false};
  std::atomic<float> sendPeakL{0.0f};
  std::atomic<float> sendPeakR{0.0f};
  std::atomic<int> sendSourceType{
      static_cast<int>(OutputBusSourceType::Unassigned)};
  std::atomic<int> sendSourceIndex{0};
  std::atomic<uint8_t> sendVbanFormat{vban_detail::kFormatInt16};
  std::atomic<int> sendTargetPort{6980};
  std::atomic<uint32_t> sendFrameCounter{0};
  std::atomic<uint64_t> sendPacketsSent{0};

  // Loop / Feedback Guard
  std::atomic<bool> loopGuardEnabled{true};
  std::atomic<bool> loopBlocked{false};

  // Pre-allocated ASRC interpolators & de-click state (zero audio-thread alloc)
  juce::LagrangeInterpolator interpL;
  juce::LagrangeInterpolator interpR;
  double cachedRatio = 1.0;
  double smoothedRatio = 1.0;
  float lastSampleL = 0.0f;
  float lastSampleR = 0.0f;
  float fadeEnv = 1.0f;
  float smoothedReturnGain = 1.0f;
  float smoothedSendGain = 1.0f;
  bool returnPrimed = false;
  bool inUnderrunRecovery = false;
  std::array<float, kMaxBlockScratch> asrcSrcL{};
  std::array<float, kMaxBlockScratch> asrcSrcR{};

  // Control-thread configured names/IP + lock-free mirrors for UDP thread
  juce::CriticalSection configLock;
  juce::String returnStreamName{"HeritageIn1"};
  juce::String sendStreamName{"HeritageOut1"};
  juce::String sendTargetIp{"127.0.0.1"};
  std::array<char, 17> fastReturnStreamName{"HeritageIn1"};
  std::array<char, 17> fastSendStreamName{"HeritageOut1"};
  std::array<char, 64> fastSendTargetIp{"127.0.0.1"};

  static void copyTruncated(char *dst, size_t dstCap,
                            const juce::String &src) noexcept {
    if (dst == nullptr || dstCap == 0)
      return;
    const auto utf8 = src.toRawUTF8();
    const size_t len = std::min(dstCap - 1, std::strlen(utf8));
    std::memcpy(dst, utf8, len);
    dst[len] = '\0';
  }

  void reset() noexcept {
    returnRing.reset();
    sendRing.reset();
    returnPeakL.store(0.0f, std::memory_order_relaxed);
    returnPeakR.store(0.0f, std::memory_order_relaxed);
    sendPeakL.store(0.0f, std::memory_order_relaxed);
    sendPeakR.store(0.0f, std::memory_order_relaxed);
    loopBlocked.store(false, std::memory_order_relaxed);
    interpL.reset();
    interpR.reset();
    cachedRatio = 1.0;
    smoothedRatio = 1.0;
    lastSampleL = 0.0f;
    lastSampleR = 0.0f;
    fadeEnv = 1.0f;
    smoothedReturnGain = 1.0f;
    smoothedSendGain = 1.0f;
    returnPrimed = false;
    inUnderrunRecovery = false;
  }

  void setReturnStreamName(const juce::String &name) {
    const juce::ScopedLock sl(configLock);
    returnStreamName = name.substring(0, 16);
    copyTruncated(fastReturnStreamName.data(), fastReturnStreamName.size(),
                  returnStreamName);
  }
  juce::String getReturnStreamName() const {
    const juce::ScopedLock sl(configLock);
    return returnStreamName;
  }

  bool matchesReturnStreamName(const char *rxName) const noexcept {
    if (rxName == nullptr)
      return false;
    return std::strncmp(fastReturnStreamName.data(), rxName, 16) == 0;
  }

  void setSendTarget(const juce::String &ip, int port,
                     const juce::String &streamName) {
    const juce::ScopedLock sl(configLock);
    sendTargetIp = ip;
    sendTargetPort.store(std::clamp(port, 1, 65535), std::memory_order_relaxed);
    sendStreamName = streamName.substring(0, 16);
    copyTruncated(fastSendTargetIp.data(), fastSendTargetIp.size(),
                  sendTargetIp);
    copyTruncated(fastSendStreamName.data(), fastSendStreamName.size(),
                  sendStreamName);
  }

  void getSendTarget(juce::String &ip, int &port, juce::String &streamName) const {
    const juce::ScopedLock sl(configLock);
    ip = sendTargetIp;
    port = sendTargetPort.load(std::memory_order_relaxed);
    streamName = sendStreamName;
  }

  bool updateLoopGuard() noexcept {
    if (!loopGuardEnabled.load(std::memory_order_relaxed)) {
      loopBlocked.store(false, std::memory_order_relaxed);
      return false;
    }
    const auto srcType = static_cast<OutputBusSourceType>(
        sendSourceType.load(std::memory_order_relaxed));
    const int srcIdx = sendSourceIndex.load(std::memory_order_relaxed);
    const int retL = returnTargetChannelL.load(std::memory_order_relaxed);
    const int retR = returnTargetChannelR.load(std::memory_order_relaxed);

    const bool directLoop =
        (srcType == OutputBusSourceType::DirectOut) &&
        ((retL >= 0 && srcIdx == retL) || (retR >= 0 && srcIdx == retR));
    loopBlocked.store(directLoop, std::memory_order_relaxed);
    return directLoop;
  }

  // Called on audio thread (lock-free, allocation-free)
  void pullReturnBlock(float *outL, float *outR, int numSamples,
                       double mixerSampleRate) noexcept {
    if (outL == nullptr || outR == nullptr || numSamples <= 0)
      return;
    const int n = std::min(numSamples, static_cast<int>(kMaxBlockScratch));
    if (!returnEnabled.load(std::memory_order_relaxed)) {
      std::fill(outL, outL + numSamples, 0.0f);
      std::fill(outR, outR + numSamples, 0.0f);
      returnPeakL.store(0.0f, std::memory_order_relaxed);
      returnPeakR.store(0.0f, std::memory_order_relaxed);
      lastSampleL = 0.0f;
      lastSampleR = 0.0f;
      fadeEnv = 1.0f;
      returnPrimed = false;
      inUnderrunRecovery = false;
      return;
    }

    const double srcSr =
        std::max(8000.0, returnSampleRate.load(std::memory_order_relaxed));
    const double dstSr = std::max(8000.0, mixerSampleRate);
    const double nominalRatio = srcSr / dstSr;
    if (std::abs(nominalRatio - cachedRatio) > 0.01) {
      interpL.reset();
      interpR.reset();
      cachedRatio = nominalRatio;
      smoothedRatio = nominalRatio;
    }

    size_t avail = returnRing.availableRead();

    // High-water overflow guard: trim excessive jitter buildup cleanly
    if (avail > 6144) {
      returnRing.advanceRead(avail - 1536);
      avail = returnRing.availableRead();
      fadeEnv = 0.0f;
    }

    // Adaptive clock-drift servo when actively receiving remote packets
    const uint64_t pktsRx =
        returnPacketsReceived.load(std::memory_order_relaxed);
    if (pktsRx > 4) {
      const double driftAdjust = std::clamp(
          (static_cast<double>(avail) - 1536.0) / 1638400.0, -0.0015, 0.0015);
      smoothedRatio =
          0.98 * smoothedRatio + 0.02 * (nominalRatio + driftAdjust);
    } else {
      smoothedRatio = nominalRatio;
    }

    const bool useDirectPop = (std::abs(smoothedRatio - 1.0) < 1.0e-5);
    const size_t minNeeded =
        static_cast<size_t>(std::ceil(static_cast<double>(n) * smoothedRatio)) +
        (useDirectPop ? 0u : 5u);

    if (pktsRx > 0) {
      if (!returnPrimed &&
          avail >= std::clamp<size_t>(minNeeded * 2, 256, 1536)) {
        returnPrimed = true;
      }
      if (inUnderrunRecovery &&
          avail >= std::clamp<size_t>(minNeeded * 3, 512, 2048)) {
        inUnderrunRecovery = false;
      }
    }

    const bool bufferingHold =
        (pktsRx > 0 && (!returnPrimed || inUnderrunRecovery));
    if (bufferingHold || avail < minNeeded) {
      if (avail < minNeeded && returnPrimed) {
        inUnderrunRecovery = true;
        returnRing.underrunCount.fetch_add(1, std::memory_order_relaxed);
      }
      float pkL = 0.0f;
      float pkR = 0.0f;
      for (int i = 0; i < n; ++i) {
        lastSampleL *= 0.90f;
        lastSampleR *= 0.90f;
        outL[i] = sanitize(lastSampleL);
        outR[i] = sanitize(lastSampleR);
        pkL = std::max(pkL, std::abs(outL[i]));
        pkR = std::max(pkR, std::abs(outR[i]));
      }
      if (n < numSamples) {
        std::fill(outL + n, outL + numSamples, 0.0f);
        std::fill(outR + n, outR + numSamples, 0.0f);
      }
      fadeEnv = 0.0f;
      returnPeakL.store(pkL, std::memory_order_relaxed);
      returnPeakR.store(pkR, std::memory_order_relaxed);
      return;
    }

    if (useDirectPop) {
      returnRing.popFrames(outL, outR, static_cast<size_t>(n));
    } else {
      const size_t toPeek = std::min<size_t>(kMaxBlockScratch, minNeeded + 4);
      const size_t peeked =
          returnRing.peekFrames(asrcSrcL.data(), asrcSrcR.data(), toPeek);
      const int consumedL = interpL.process(smoothedRatio, asrcSrcL.data(), outL,
                                            n, static_cast<int>(peeked), 0);
      const int consumedR = interpR.process(smoothedRatio, asrcSrcR.data(), outR,
                                            n, static_cast<int>(peeked), 0);
      const int consumed = std::clamp(std::min(consumedL, consumedR), 0,
                                      static_cast<int>(peeked));
      returnRing.advanceRead(static_cast<size_t>(consumed));
    }

    const bool muted = returnMuted.load(std::memory_order_relaxed);
    const float gainDb = std::clamp(
        returnGainDb.load(std::memory_order_relaxed), -90.0f, 12.0f);
    const float targetLinGain =
        (muted || gainDb <= -89.5f)
            ? 0.0f
            : static_cast<float>(dbToGain(static_cast<double>(gainDb)));

    float pkL = 0.0f;
    float pkR = 0.0f;
    for (int i = 0; i < n; ++i) {
      smoothedReturnGain += (targetLinGain - smoothedReturnGain) * 0.04f;
      fadeEnv += (1.0f - fadeEnv) * 0.08f;
      const float env = smoothedReturnGain * fadeEnv;
      outL[i] = sanitize(outL[i] * env);
      outR[i] = sanitize(outR[i] * env);
      lastSampleL = outL[i];
      lastSampleR = outR[i];
      pkL = std::max(pkL, std::abs(outL[i]));
      pkR = std::max(pkR, std::abs(outR[i]));
    }
    if (n < numSamples) {
      std::fill(outL + n, outL + numSamples, 0.0f);
      std::fill(outR + n, outR + numSamples, 0.0f);
    }
    returnPeakL.store(pkL, std::memory_order_relaxed);
    returnPeakR.store(pkR, std::memory_order_relaxed);
  }

  // Called on audio thread (lock-free, allocation-free)
  void pushSendBlock(const float *inL, const float *inR,
                     int numSamples) noexcept {
    if (inL == nullptr || inR == nullptr || numSamples <= 0)
      return;
    if (!sendEnabled.load(std::memory_order_relaxed)) {
      sendPeakL.store(0.0f, std::memory_order_relaxed);
      sendPeakR.store(0.0f, std::memory_order_relaxed);
      smoothedSendGain = 0.0f;
      return;
    }

    const bool blocked =
        loopGuardEnabled.load(std::memory_order_relaxed) &&
        loopBlocked.load(std::memory_order_relaxed);
    const bool muted = sendMuted.load(std::memory_order_relaxed);
    const float gainDb =
        std::clamp(sendGainDb.load(std::memory_order_relaxed), -90.0f, 12.0f);
    const float targetLinGain =
        (blocked || muted || gainDb <= -89.5f)
            ? 0.0f
            : static_cast<float>(dbToGain(static_cast<double>(gainDb)));
    if (blocked)
      smoothedSendGain = 0.0f;

    const int n = std::min(numSamples, static_cast<int>(kMaxBlockScratch));
    float pkL = 0.0f;
    float pkR = 0.0f;
    for (int i = 0; i < n; ++i) {
      if (!blocked)
        smoothedSendGain += (targetLinGain - smoothedSendGain) * 0.04f;
      const float sL = std::clamp(
          static_cast<float>(sanitize(inL[i] * smoothedSendGain)), -1.0f, 1.0f);
      const float sR = std::clamp(
          static_cast<float>(sanitize(inR[i] * smoothedSendGain)), -1.0f, 1.0f);
      asrcSrcL[static_cast<size_t>(i)] = sL;
      asrcSrcR[static_cast<size_t>(i)] = sR;
      pkL = std::max(pkL, std::abs(sL));
      pkR = std::max(pkR, std::abs(sR));
    }
    sendPeakL.store(pkL, std::memory_order_relaxed);
    sendPeakR.store(pkR, std::memory_order_relaxed);
    sendRing.pushFrames(asrcSrcL.data(), asrcSrcR.data(),
                        static_cast<size_t>(n));
  }
};

// ==============================================================================
// VirtualCableTransport: 4 Stereo Pairs + VBAN UDP & Windows SYSVAD Shared Memory
// ==============================================================================
enum class TransportBackend : int {
  VbanUdp = 0,
  SharedMemory = 1,
  Both = 2
};

class VirtualCableTransport final : private juce::Thread {
public:
  static constexpr size_t kNumVirtualCables = 4;

  VirtualCableTransport() : juce::Thread("HeritageVirtualTransportWorker") {
    for (size_t i = 0; i < kNumVirtualCables; ++i) {
      cables[i].setReturnStreamName("HeritageIn" + juce::String(i + 1));
      cables[i].setSendTarget("127.0.0.1", 6980,
                              "HeritageOut" + juce::String(i + 1));
    }
  }

  ~VirtualCableTransport() override {
    stopNetwork();
    stopSharedMemory();
  }

  void setTransportBackend(TransportBackend b) noexcept {
    backend.store(static_cast<int>(b), std::memory_order_relaxed);
  }
  TransportBackend getTransportBackend() const noexcept {
    return static_cast<TransportBackend>(
        backend.load(std::memory_order_relaxed));
  }

  bool startNetwork(int listenPort = 6980) {
    stopNetwork();
    auto sock = std::make_unique<juce::DatagramSocket>(false);
    if (!sock->bindToPort(listenPort))
      return false;
    boundPort.store(listenPort, std::memory_order_relaxed);
    socket = std::move(sock);
    if (!isThreadRunning())
      startThread(juce::Thread::Priority::high);
    return true;
  }

  void stopNetwork() {
    if (socket == nullptr)
      return;
    if (!shmMapping.isOpen()) {
      signalThreadShouldExit();
      socket->shutdown();
      stopThread(1000);
    } else {
      socket->shutdown();
    }
    socket.reset();
    boundPort.store(0, std::memory_order_relaxed);
  }

  bool startSharedMemory(
      const wchar_t *mappingName = L"Local\\HeritageVirtualAudioShmV1") {
    if (!shmMapping.openOrCreate(mappingName))
      return false;
    if (!isThreadRunning())
      startThread(juce::Thread::Priority::high);
    return true;
  }

  void stopSharedMemory() {
    if (!shmMapping.isOpen())
      return;
    if (socket == nullptr) {
      signalThreadShouldExit();
      stopThread(1000);
    }
    shmMapping.close();
  }

  bool isSharedMemoryOpen() const noexcept { return shmMapping.isOpen(); }
  HeritageShmBlock *getSharedMemoryBlock() noexcept { return shmMapping.get(); }

  void pumpSharedMemoryFromAudioThread() noexcept {
    audioThreadPumpsShm.store(true, std::memory_order_relaxed);
    pumpSharedMemoryOnce();
  }

  void pumpSharedMemoryOnce() noexcept {
    if (shmPumpBusy.test_and_set(std::memory_order_acquire))
      return;
    struct BusyGuard final {
      std::atomic_flag &flag;
      ~BusyGuard() noexcept { flag.clear(std::memory_order_release); }
    } busyGuard{shmPumpBusy};

    auto *shm = shmMapping.get();
    if (shm == nullptr || !shm->isValid())
      return;
    shm->mixerHeartbeat.fetch_add(1, std::memory_order_relaxed);
    const double sr = engineSampleRate.load(std::memory_order_relaxed);

    std::array<float, 512> tmpL{};
    std::array<float, 512> tmpR{};

    for (size_t k = 0; k < kNumVirtualCables && k < kHeritageShmNumPairs; ++k) {
      auto &vc = cables[k];
      auto &pair = shm->pairs[k];

      // 1. Driver WaveRT Render -> Mixer Return
      if (vc.returnEnabled.load(std::memory_order_relaxed)) {
        const uint32_t drvSr =
            pair.renderToMixer.sampleRate.load(std::memory_order_relaxed);
        if (drvSr >= 8000)
          vc.returnSampleRate.store(static_cast<double>(drvSr),
                                    std::memory_order_relaxed);
        size_t avail = pair.renderToMixer.availableRead();
        while (avail > 0) {
          const size_t chunk = std::min(avail, tmpL.size());
          pair.renderToMixer.pop(tmpL.data(), tmpR.data(), chunk);
          vc.returnRing.pushFrames(tmpL.data(), tmpR.data(), chunk);
          vc.returnPacketsReceived.fetch_add(1, std::memory_order_relaxed);
          avail = pair.renderToMixer.availableRead();
        }
      }

      // 2. Mixer Send -> Driver WaveRT Capture
      if (vc.sendEnabled.load(std::memory_order_relaxed)) {
        pair.mixerToCapture.sampleRate.store(static_cast<uint32_t>(sr),
                                             std::memory_order_relaxed);
        pair.mixerToCapture.streamActive.store(1, std::memory_order_relaxed);
        size_t avail = vc.sendRing.availableRead();
        while (avail > 0) {
          const size_t chunk = std::min(avail, tmpL.size());
          vc.sendRing.popFrames(tmpL.data(), tmpR.data(), chunk);
          pair.mixerToCapture.push(tmpL.data(), tmpR.data(), chunk);
          vc.sendPacketsSent.fetch_add(1, std::memory_order_relaxed);
          avail = vc.sendRing.availableRead();
        }
      } else {
        pair.mixerToCapture.streamActive.store(0, std::memory_order_relaxed);
      }
    }
  }

  bool isNetworkRunning() const noexcept {
    return socket != nullptr && isThreadRunning();
  }
  int getBoundPort() const noexcept {
    return boundPort.load(std::memory_order_relaxed);
  }

  void setEngineSampleRate(double sr) noexcept {
    engineSampleRate.store(std::max(8000.0, sr), std::memory_order_relaxed);
  }

  void updateAllLoopGuards() noexcept {
    for (auto &c : cables)
      c.updateLoopGuard();
  }

  std::array<VirtualCableSlot, kNumVirtualCables> cables;

private:
  void run() override {
    std::array<uint8_t, 2048> rxBuf{};
    std::array<uint8_t, 1500> txBuf{};
    std::array<float, 256> pktL{};
    std::array<float, 256> pktR{};

    while (!threadShouldExit()) {
      const auto mode = getTransportBackend();
      const bool useVban =
          (mode == TransportBackend::VbanUdp || mode == TransportBackend::Both) &&
          socket != nullptr;
      const bool useShm = (mode == TransportBackend::SharedMemory ||
                           mode == TransportBackend::Both) &&
                          shmMapping.isOpen();
      bool didWork = false;

      if (useShm && !audioThreadPumpsShm.load(std::memory_order_relaxed))
        pumpSharedMemoryOnce();

      if (useVban) {
        // 1. Drain all available incoming VBAN UDP packets non-blocking
        while (!threadShouldExit() && socket != nullptr &&
               socket->waitUntilReady(true, 0) > 0) {
          juce::String senderIp;
          int senderPort = 0;
          const int bytesRead =
              socket->read(rxBuf.data(), static_cast<int>(rxBuf.size()), false,
                           senderIp, senderPort);
          if (bytesRead <= 0)
            break;
          didWork = true;
          const auto info = vban_detail::decodeAudioPacket(
              rxBuf.data(), static_cast<size_t>(bytesRead), pktL.data(),
              pktR.data(), pktL.size());
          if (!info.valid)
            continue;

          for (auto &c : cables) {
            if (c.returnEnabled.load(std::memory_order_relaxed) &&
                c.matchesReturnStreamName(info.streamName)) {
              c.returnSampleRate.store(info.sampleRate,
                                       std::memory_order_relaxed);
              c.returnRing.pushFrames(pktL.data(), pktR.data(),
                                      static_cast<size_t>(info.numSamples));
              c.returnPacketsReceived.fetch_add(1, std::memory_order_relaxed);
            }
          }
        }

        // 2. Immediately drain outgoing Send ring buffers into VBAN UDP packets
        const double sr = engineSampleRate.load(std::memory_order_relaxed);
        for (auto &c : cables) {
          if (!c.sendEnabled.load(std::memory_order_relaxed))
            continue;
          const uint8_t fmt =
              c.sendVbanFormat.load(std::memory_order_relaxed);
          const size_t maxChunk =
              ((fmt & 0x07) == vban_detail::kFormatFloat32) ? 128 : 256;
          while (c.sendRing.availableRead() >= maxChunk) {
            didWork = true;
            c.sendRing.popFrames(pktL.data(), pktR.data(), maxChunk);
            const int port = c.sendTargetPort.load(std::memory_order_relaxed);
            const uint32_t fc =
                c.sendFrameCounter.fetch_add(1, std::memory_order_relaxed);
            const size_t pktLen = vban_detail::encodeAudioPacket(
                txBuf.data(), txBuf.size(), pktL.data(), pktR.data(),
                static_cast<int>(maxChunk), sr, fmt, c.fastSendStreamName.data(),
                fc);
            if (pktLen > 0 && socket != nullptr) {
              socket->write(juce::String::fromUTF8(c.fastSendTargetIp.data()),
                            port, txBuf.data(), static_cast<int>(pktLen));
              c.sendPacketsSent.fetch_add(1, std::memory_order_relaxed);
            }
          }
        }
      }

      if (!didWork) {
        if (useVban && socket != nullptr)
          socket->waitUntilReady(true, 1);
        else
          juce::Thread::sleep(1);
      }
    }
  }

  std::unique_ptr<juce::DatagramSocket> socket;
  HeritageShmMapping shmMapping;
  std::atomic<int> backend{static_cast<int>(TransportBackend::Both)};
  std::atomic<int> boundPort{0};
  std::atomic<double> engineSampleRate{48000.0};
  std::atomic<bool> audioThreadPumpsShm{false};
  std::atomic_flag shmPumpBusy = ATOMIC_FLAG_INIT;
};

} // namespace nova::graviton
