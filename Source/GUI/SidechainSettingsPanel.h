#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class SidechainSettingsPanel final : public juce::Component {
public:
  SidechainSettingsPanel(MixerEngine &mixer, int channelIndex)
      : engine(mixer), channel(channelIndex) {
    source.addItem("Internal key", 1);
    source.addItem("External channel", 2);
    filter.addItem("Bypass", 1);
    filter.addItem("HPF", 2);
    filter.addItem("LPF", 3);
    filter.addItem("Bandpass", 4);
    filter.addItem("Notch", 5);
    for (int i = 1; i <= kLogicalChannels; ++i)
      externalChannel.addItem("CH " + juce::String(i), i);

    for (auto *component : {static_cast<juce::Component *>(&source),
                            static_cast<juce::Component *>(&externalChannel),
                            static_cast<juce::Component *>(&filter),
                            static_cast<juce::Component *>(&frequency),
                            static_cast<juce::Component *>(&q),
                            static_cast<juce::Component *>(&keyListen)}) {
      addAndMakeVisible(*component);
    }

    frequency.setSliderStyle(juce::Slider::LinearHorizontal);
    frequency.setRange(20.0, 20000.0, 1.0);
    frequency.setSkewFactorFromMidPoint(1000.0);
    frequency.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 20);
    frequency.setTextValueSuffix(" Hz");
    q.setSliderStyle(juce::Slider::LinearHorizontal);
    q.setRange(0.1, 10.0, 0.1);
    q.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    keyListen.setButtonText("KEY LISTEN");
    keyListen.setClickingTogglesState(true);

    source.onChange = [this] {
      current().sidechainSourceType.store(
          source.getSelectedId() == 2
              ? static_cast<int>(nova::graviton::KeySourceType::External)
              : static_cast<int>(nova::graviton::KeySourceType::Internal),
          std::memory_order_relaxed);
      updateEnabledState();
    };
    externalChannel.onChange = [this] {
      current().sidechainExternalChannel.store(
          externalChannel.getSelectedId() - 1, std::memory_order_relaxed);
    };
    filter.onChange = [this] {
      current().sidechainFilterType.store(filter.getSelectedId() - 1,
                                          std::memory_order_relaxed);
    };
    frequency.onValueChange = [this] {
      current().sidechainFilterFreqHz.store(
          static_cast<float>(frequency.getValue()), std::memory_order_relaxed);
    };
    q.onValueChange = [this] {
      current().sidechainFilterQ.store(static_cast<float>(q.getValue()),
                                       std::memory_order_relaxed);
    };
    keyListen.onClick = [this] {
      current().sidechainKeyListen.store(keyListen.getToggleState(),
                                         std::memory_order_relaxed);
    };

    const auto &state = current();
    source.setSelectedId(
        state.sidechainSourceType.load(std::memory_order_relaxed) ==
                static_cast<int>(nova::graviton::KeySourceType::External)
            ? 2
            : 1,
        juce::dontSendNotification);
    externalChannel.setSelectedId(
        state.sidechainExternalChannel.load(std::memory_order_relaxed) + 1,
        juce::dontSendNotification);
    filter.setSelectedId(
        state.sidechainFilterType.load(std::memory_order_relaxed) + 1,
        juce::dontSendNotification);
    frequency.setValue(
        state.sidechainFilterFreqHz.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    q.setValue(state.sidechainFilterQ.load(std::memory_order_relaxed),
               juce::dontSendNotification);
    keyListen.setToggleState(
        state.sidechainKeyListen.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    updateEnabledState();
    setSize(600, 170);
  }

  void paint(juce::Graphics &graphics) override {
    graphics.fillAll(juce::Colour(0xff20262d));
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    auto row = area.removeFromTop(28);
    source.setBounds(row.removeFromLeft(150).reduced(2));
    externalChannel.setBounds(row.removeFromLeft(140).reduced(2));
    filter.setBounds(row.removeFromLeft(120).reduced(2));
    keyListen.setBounds(row.reduced(2));
    area.removeFromTop(8);
    frequency.setBounds(area.removeFromTop(34));
    area.removeFromTop(4);
    q.setBounds(area.removeFromTop(34));
  }

private:
  ChannelState &current() {
    return engine.channels[static_cast<size_t>(channel)];
  }

  void updateEnabledState() {
    externalChannel.setEnabled(source.getSelectedId() == 2);
  }

  MixerEngine &engine;
  int channel;
  juce::ComboBox source, externalChannel, filter;
  juce::Slider frequency, q;
  juce::TextButton keyListen;
};

} // namespace nova
