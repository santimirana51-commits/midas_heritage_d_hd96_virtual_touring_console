#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>

namespace nova {

class MixerMidiCallback final : public juce::MidiInputCallback {
public:
  explicit MixerMidiCallback(MixerEngine &e) : engine(e) {
    for (auto &target : ccTargets)
      target.store(-1, std::memory_order_relaxed);
  }

  void armLearn(int target) noexcept {
    learnTarget.store(target, std::memory_order_relaxed);
    learnArmed.store(target > 0, std::memory_order_relaxed);
  }

  bool isLearnArmed() const noexcept {
    return learnArmed.load(std::memory_order_relaxed);
  }

  void loadMappings(const juce::var &value) {
    auto *object = value.getDynamicObject();
    if (object == nullptr)
      return;
    for (int cc = 0; cc < 128; ++cc) {
      const auto key = "cc" + juce::String(cc);
      if (object->hasProperty(key)) {
        const int target = static_cast<int>(object->getProperty(key));
        if (isValidTarget(target))
          ccTargets[static_cast<size_t>(cc)].store(target,
                                                   std::memory_order_relaxed);
      }
    }
  }

  juce::var exportMappings() const {
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    for (int cc = 0; cc < 128; ++cc) {
      const int target =
          ccTargets[static_cast<size_t>(cc)].load(std::memory_order_relaxed);
      if (target > 0)
        object->setProperty("cc" + juce::String(cc), target);
    }
    return juce::var(object.get());
  }

  int getLastLearnedCC() const noexcept {
    return lastLearnedCC.load(std::memory_order_relaxed);
  }

  void handleIncomingMidiMessage(juce::MidiInput *,
                                 const juce::MidiMessage &message) override {
    if (!message.isController())
      return;

    const int cc = message.getControllerNumber();
    const int value = message.getControllerValue();
    const float normalized = static_cast<float>(value) / 127.0f;

    if (learnArmed.exchange(false, std::memory_order_relaxed)) {
      const int target = learnTarget.load(std::memory_order_relaxed);
      if (isValidTarget(target)) {
        ccTargets[static_cast<size_t>(cc)].store(target,
                                                 std::memory_order_relaxed);
        lastLearnedCC.store(cc, std::memory_order_relaxed);
        engine.midiLearnedDirty.store(true, std::memory_order_relaxed);
        engine.midiUiDirty.store(true, std::memory_order_relaxed);
      }
      return;
    }

    const int learnedTarget =
        ccTargets[static_cast<size_t>(cc)].load(std::memory_order_relaxed);
    if (learnedTarget > 0) {
      applyTarget(learnedTarget, value, normalized);
      return;
    }

    // CC 0-7: channel 1-8 fader gain, mapped to -60..+10 dB.
    if (cc >= 0 && cc <= 7) {
      engine.channels[static_cast<size_t>(cc)].gainDb.store(
          -60.0f + normalized * 70.0f, std::memory_order_relaxed);
      engine.midiUiDirty.store(true, std::memory_order_relaxed);
      return;
    }

    // CC 16-23: channel 1-8 mute. Toggle only when crossing the midpoint,
    // avoiding repeated toggles while a controller sends continuous values.
    if (cc >= 16 && cc <= 23) {
      const auto index = static_cast<size_t>(cc - 16);
      const bool high = value >= 64;
      if (high && !muteCcHigh[index].exchange(true, std::memory_order_relaxed))
        engine.channels[index].muted.store(
            !engine.channels[index].muted.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
      if (!high)
        muteCcHigh[index].store(false, std::memory_order_relaxed);
      engine.midiUiDirty.store(true, std::memory_order_relaxed);
      return;
    }

    // CC 32: stereo Main master, mapped to -60..+10 dB.
    if (cc == 32) {
      engine.masterDb.store(-60.0f + normalized * 70.0f,
                            std::memory_order_relaxed);
      engine.midiUiDirty.store(true, std::memory_order_relaxed);
      return;
    }

    // CC 40-43: AUX 1-4 master level, mapped to -60..+10 dB.
    if (cc >= 40 && cc <= 43) {
      engine.auxMasterDb[static_cast<size_t>(cc - 40)].store(
          -60.0f + normalized * 70.0f, std::memory_order_relaxed);
      engine.midiUiDirty.store(true, std::memory_order_relaxed);
    }
  }

private:
  static bool isValidTarget(int target) noexcept {
    return (target >= 1 && target <= 8) || (target >= 11 && target <= 18) ||
           target == 30 || (target >= 41 && target <= 44) ||
           (target >= 51 && target <= 74) ||
           (target >= 1000 && target < 1000 + kAuxBusCapacity);
  }

  void applyTarget(int target, int value, float normalized) noexcept {
    if (target >= 1 && target <= 8)
      engine.channels[static_cast<size_t>(target - 1)].gainDb.store(
          -60.0f + normalized * 70.0f, std::memory_order_relaxed);
    else if (target >= 11 && target <= 18) {
      const auto index = static_cast<size_t>(target - 11);
      const bool high = value >= 64;
      if (high && !muteCcHigh[index].exchange(true, std::memory_order_relaxed))
        engine.channels[index].muted.store(
            !engine.channels[index].muted.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
      if (!high)
        muteCcHigh[index].store(false, std::memory_order_relaxed);
    } else if (target == 30)
      engine.masterDb.store(-60.0f + normalized * 70.0f,
                            std::memory_order_relaxed);
    else if (target >= 41 && target <= 44)
      engine.auxMasterDb[static_cast<size_t>(target - 41)].store(
          -60.0f + normalized * 70.0f, std::memory_order_relaxed);
    else if (target >= 1000 && target < 1000 + kAuxBusCapacity)
      engine.auxMasterDb[static_cast<size_t>(target - 1000)].store(
          -60.0f + normalized * 70.0f, std::memory_order_relaxed);
    else if (target >= 51 && target <= 74)
      engine.vcaMasterDb[static_cast<size_t>(target - 51)].store(
          -60.0f + normalized * 70.0f, std::memory_order_relaxed);
    engine.midiUiDirty.store(true, std::memory_order_relaxed);
  }

  MixerEngine &engine;
  std::array<std::atomic<int>, 128> ccTargets{};
  std::array<std::atomic<bool>, 8> muteCcHigh{};
  std::atomic<int> learnTarget{0};
  std::atomic<int> lastLearnedCC{-1};
  std::atomic<bool> learnArmed{false};
};

} // namespace nova
