#pragma once

#include "DSP/GravitonDSP.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nova::graviton {

// ==============================================================================
// Shared Memory ABI Contract between Heritage Virtual Audio Driver (SYSVAD/WaveRT)
// and OPENLIVE_REFACTORED User-Mode Mixer.
// 4 Separate Stereo Pairs for Maximum Windows App Compatibility:
//   Pair 0: Heritage Cable 1-2 (Render -> Mixer / Mixer -> Capture)
//   Pair 1: Heritage Cable 3-4
//   Pair 2: Heritage Cable 5-6
//   Pair 3: Heritage Cable 7-8
// ==============================================================================
inline constexpr uint32_t kHeritageShmMagic = 0x48564143u; // "HVAC"
inline constexpr uint32_t kHeritageShmVersion = 1u;
inline constexpr size_t kHeritageShmNumPairs = 4;
inline constexpr size_t kHeritageShmRingFrames = 16384; // Power of 2
inline constexpr size_t kHeritageShmRingMask = kHeritageShmRingFrames - 1;

struct HeritageShmStereoRing {
  std::atomic<uint64_t> writeFrame{0};
  std::atomic<uint64_t> readFrame{0};
  std::atomic<uint32_t> sampleRate{48000};
  std::atomic<uint32_t> streamActive{0};
  std::atomic<uint64_t> underruns{0};
  std::atomic<uint64_t> overruns{0};
  float left[kHeritageShmRingFrames]{};
  float right[kHeritageShmRingFrames]{};

  void reset() noexcept {
    writeFrame.store(0, std::memory_order_relaxed);
    readFrame.store(0, std::memory_order_relaxed);
    streamActive.store(0, std::memory_order_relaxed);
    underruns.store(0, std::memory_order_relaxed);
    overruns.store(0, std::memory_order_relaxed);
  }

  size_t availableRead() const noexcept {
    const uint64_t w = writeFrame.load(std::memory_order_acquire);
    const uint64_t r = readFrame.load(std::memory_order_relaxed);
    return (w >= r) ? static_cast<size_t>(w - r) : 0;
  }

  size_t push(const float *inL, const float *inR, size_t count) noexcept {
    if (inL == nullptr || count == 0)
      return 0;
    const uint64_t w = writeFrame.load(std::memory_order_relaxed);
    const uint64_t r = readFrame.load(std::memory_order_acquire);
    const size_t used = (w >= r) ? static_cast<size_t>(w - r) : kHeritageShmRingFrames;
    const size_t freeFrames =
        (used < kHeritageShmRingFrames) ? (kHeritageShmRingFrames - used) : 0;
    const size_t toWrite = std::min(count, freeFrames);
    if (toWrite < count)
      overruns.fetch_add(1, std::memory_order_relaxed);

    for (size_t i = 0; i < toWrite; ++i) {
      const size_t idx = static_cast<size_t>((w + i) & kHeritageShmRingMask);
      left[idx] = static_cast<float>(sanitize(inL[i]));
      right[idx] = static_cast<float>(sanitize(inR != nullptr ? inR[i] : inL[i]));
    }
    writeFrame.store(w + toWrite, std::memory_order_release);
    return toWrite;
  }

  size_t pop(float *outL, float *outR, size_t count) noexcept {
    if (outL == nullptr || outR == nullptr || count == 0)
      return 0;
    const uint64_t w = writeFrame.load(std::memory_order_acquire);
    const uint64_t r = readFrame.load(std::memory_order_relaxed);
    const size_t avail = (w >= r) ? static_cast<size_t>(w - r) : 0;
    const size_t toRead = std::min(count, avail);
    for (size_t i = 0; i < toRead; ++i) {
      const size_t idx = static_cast<size_t>((r + i) & kHeritageShmRingMask);
      outL[i] = static_cast<float>(sanitize(left[idx]));
      outR[i] = static_cast<float>(sanitize(right[idx]));
    }
    if (toRead < count) {
      std::fill(outL + toRead, outL + count, 0.0f);
      std::fill(outR + toRead, outR + count, 0.0f);
      if (avail == 0 && streamActive.load(std::memory_order_relaxed) != 0)
        underruns.fetch_add(1, std::memory_order_relaxed);
    }
    readFrame.store(r + toRead, std::memory_order_release);
    return toRead;
  }
};

struct HeritageShmCablePair {
  // Render: Windows App -> Driver WaveRT Render -> Mixer Return
  HeritageShmStereoRing renderToMixer;
  // Capture: Mixer Send -> Driver WaveRT Capture -> Windows App
  HeritageShmStereoRing mixerToCapture;
};

struct HeritageShmBlock {
  uint32_t magic{0};
  uint32_t version{0};
  std::atomic<uint64_t> driverHeartbeat{0};
  std::atomic<uint64_t> mixerHeartbeat{0};
  HeritageShmCablePair pairs[kHeritageShmNumPairs];

  void initialiseHeader() noexcept {
    magic = kHeritageShmMagic;
    version = kHeritageShmVersion;
    driverHeartbeat.store(0, std::memory_order_relaxed);
    mixerHeartbeat.store(0, std::memory_order_relaxed);
    for (auto &p : pairs) {
      p.renderToMixer.reset();
      p.mixerToCapture.reset();
    }
  }

  bool isValid() const noexcept {
    return magic == kHeritageShmMagic && version == kHeritageShmVersion;
  }
};

// ==============================================================================
// Cross-Process Windows Named Shared Memory Mapping (RAII)
// ==============================================================================
class HeritageShmMapping final {
public:
  HeritageShmMapping() = default;
  ~HeritageShmMapping() { close(); }

  HeritageShmMapping(const HeritageShmMapping &) = delete;
  HeritageShmMapping &operator=(const HeritageShmMapping &) = delete;

  bool openOrCreate(
      const wchar_t *mappingName = L"Local\\HeritageVirtualAudioShmV1") noexcept {
    close();
#if defined(_WIN32)
    const DWORD sizeLow = static_cast<DWORD>(sizeof(HeritageShmBlock) & 0xFFFFFFFFu);
    const DWORD sizeHigh =
        static_cast<DWORD>((static_cast<uint64_t>(sizeof(HeritageShmBlock)) >> 32) &
                           0xFFFFFFFFu);
    HANDLE h = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                    sizeHigh, sizeLow, mappingName);
    if (h == nullptr)
      return false;

    const bool alreadyExisted = (::GetLastError() == ERROR_ALREADY_EXISTS);
    void *ptr = ::MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HeritageShmBlock));
    if (ptr == nullptr) {
      ::CloseHandle(h);
      return false;
    }

    hMapping = h;
    mappedBlock = static_cast<HeritageShmBlock *>(ptr);
    if (!alreadyExisted || !mappedBlock->isValid()) {
      std::memset(mappedBlock, 0, sizeof(HeritageShmBlock));
      mappedBlock->initialiseHeader();
    }
    return true;
#else
    (void)mappingName;
    return false;
#endif
  }

  void close() noexcept {
#if defined(_WIN32)
    if (mappedBlock != nullptr) {
      ::UnmapViewOfFile(mappedBlock);
      mappedBlock = nullptr;
    }
    if (hMapping != nullptr) {
      ::CloseHandle(hMapping);
      hMapping = nullptr;
    }
#endif
  }

  bool isOpen() const noexcept {
    return mappedBlock != nullptr && mappedBlock->isValid();
  }
  HeritageShmBlock *get() noexcept { return mappedBlock; }
  const HeritageShmBlock *get() const noexcept { return mappedBlock; }

private:
#if defined(_WIN32)
  HANDLE hMapping = nullptr;
#endif
  HeritageShmBlock *mappedBlock = nullptr;
};

} // namespace nova::graviton
