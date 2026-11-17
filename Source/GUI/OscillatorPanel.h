#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class OscillatorPanel final : public juce::Component, private juce::Timer {
public:
  explicit OscillatorPanel(MixerEngine &e) : engine(e) {
    enableButton.setButtonText("GENERATOR: OFF");
    enableButton.setClickingTogglesState(true);
    enableButton.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xffe65100));
    enableButton.setTooltip("Master enable for internal test signal generator");
    enableButton.onClick = [this] {
      const bool on = enableButton.getToggleState();
      engine.oscEnabled.store(on, std::memory_order_relaxed);
      enableButton.setButtonText(on ? "GENERATOR: ON (ACTIVE)"
                                    : "GENERATOR: OFF");
    };
    addAndMakeVisible(enableButton);

    typeLabel.setText("SIGNAL TYPE", juce::dontSendNotification);
    typeLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    typeLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(typeLabel);

    typeSelector.addItem("SINE WAVE", 1);
    typeSelector.addItem("PINK NOISE (-3 dB/oct)", 2);
    typeSelector.addItem("WHITE NOISE (flat)", 3);
    typeSelector.setSelectedId(1, juce::dontSendNotification);
    typeSelector.onChange = [this] {
      engine.oscType.store(typeSelector.getSelectedId() - 1,
                           std::memory_order_relaxed);
      updateFrequencyVisibility();
    };
    addAndMakeVisible(typeSelector);

    freqLabel.setText("FREQUENCY", juce::dontSendNotification);
    freqLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    freqLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(freqLabel);

    freqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    freqSlider.setRange(20.0, 20000.0, 1.0);
    freqSlider.setSkewFactorFromMidPoint(1000.0);
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 20);
    freqSlider.setTextValueSuffix(" Hz");
    freqSlider.onValueChange = [this] {
      engine.oscFrequency.store(static_cast<float>(freqSlider.getValue()),
                                std::memory_order_relaxed);
    };
    addAndMakeVisible(freqSlider);

    const std::array<int, 4> quickFreqs{100, 440, 1000, 10000};
    for (size_t i = 0; i < 4; ++i) {
      quickFreqButtons[i].setButtonText(juce::String(quickFreqs[i]) + " Hz");
      const int f = quickFreqs[i];
      quickFreqButtons[i].onClick = [this, f] { freqSlider.setValue(f); };
      addAndMakeVisible(quickFreqButtons[i]);
    }

    levelLabel.setText("OUTPUT LEVEL", juce::dontSendNotification);
    levelLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    levelLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(levelLabel);

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    levelSlider.setRange(-60.0, 0.0, 0.5);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 20);
    levelSlider.setTextValueSuffix(" dBFS");
    levelSlider.onValueChange = [this] {
      engine.oscLevelDb.store(static_cast<float>(levelSlider.getValue()),
                              std::memory_order_relaxed);
    };
    addAndMakeVisible(levelSlider);

    routingGroupLabel.setText("ASSIGN DESTINATIONS",
                              juce::dontSendNotification);
    routingGroupLabel.setFont(
        juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    routingGroupLabel.setColour(juce::Label::textColourId,
                                juce::Colour(0xff56a8ff));
    addAndMakeVisible(routingGroupLabel);

    routeMainButton.setButtonText("MAIN L/R");
    routeMainButton.setClickingTogglesState(true);
    routeMainButton.setColour(juce::TextButton::buttonOnColourId,
                              juce::Colour(0xff2ea44f));
    routeMainButton.onClick = [this] {
      engine.oscRouteMain.store(routeMainButton.getToggleState(),
                                std::memory_order_relaxed);
    };
    addAndMakeVisible(routeMainButton);

    for (int a = 0; a < kVisibleAuxSends; ++a) {
      routeAuxButtons[static_cast<size_t>(a)].setButtonText(
          "AUX " + juce::String(a + 1));
      routeAuxButtons[static_cast<size_t>(a)].setClickingTogglesState(true);
      routeAuxButtons[static_cast<size_t>(a)].setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0xff2ea44f));
      routeAuxButtons[static_cast<size_t>(a)].onClick = [this, a] {
        engine.oscRouteAux[static_cast<size_t>(a)].store(
            routeAuxButtons[static_cast<size_t>(a)].getToggleState(),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(routeAuxButtons[static_cast<size_t>(a)]);
    }

    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      routeMatrixButtons[static_cast<size_t>(m)].setButtonText(
          "MATRIX " + juce::String(m + 1));
      routeMatrixButtons[static_cast<size_t>(m)].setClickingTogglesState(true);
      routeMatrixButtons[static_cast<size_t>(m)].setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0xff2ea44f));
      routeMatrixButtons[static_cast<size_t>(m)].onClick = [this, m] {
        engine.oscRouteMatrix[static_cast<size_t>(m)].store(
            routeMatrixButtons[static_cast<size_t>(m)].getToggleState(),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(routeMatrixButtons[static_cast<size_t>(m)]);
    }

    setSize(580, 400);
    syncFromState();
    startTimerHz(10);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff20262d));
    g.setColour(juce::Colour(0xfff0f3f6));
    g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    g.drawText("SIGNAL GENERATOR & SYSTEM ALIGNMENT", 16, 8, getWidth() - 32,
               28, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff30363d));
    g.drawHorizontalLine(40, 16.0f, static_cast<float>(getWidth() - 16));
    g.drawHorizontalLine(270, 16.0f, static_cast<float>(getWidth() - 16));
  }

  void resized() override {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto topRow = area.removeFromTop(36);
    enableButton.setBounds(topRow.removeFromLeft(200).reduced(2));
    typeLabel.setBounds(topRow.removeFromLeft(90).reduced(2));
    typeSelector.setBounds(topRow.reduced(2));

    area.removeFromTop(12);

    auto freqRow = area.removeFromTop(32);
    freqLabel.setBounds(freqRow.removeFromLeft(100).reduced(2));
    freqSlider.setBounds(freqRow.reduced(2));

    auto quickRow = area.removeFromTop(28);
    quickRow.removeFromLeft(100);
    const int qW = quickRow.getWidth() / 4;
    for (size_t i = 0; i < 4; ++i)
      quickFreqButtons[i].setBounds(quickRow.removeFromLeft(qW).reduced(2));

    area.removeFromTop(12);

    auto lvlRow = area.removeFromTop(32);
    levelLabel.setBounds(lvlRow.removeFromLeft(100).reduced(2));
    levelSlider.setBounds(lvlRow.reduced(2));

    area.removeFromTop(20);
    routingGroupLabel.setBounds(area.removeFromTop(24));

    auto destRow = area.removeFromTop(34);
    routeMainButton.setBounds(destRow.removeFromLeft(88).reduced(2));
    for (size_t a = 0; a < kVisibleAuxSends; ++a)
      routeAuxButtons[a].setBounds(destRow.removeFromLeft(72).reduced(2));
    for (size_t m = 0; m < kVisibleMatrixBuses; ++m)
      routeMatrixButtons[m].setBounds(destRow.removeFromLeft(88).reduced(2));
  }

private:
  void updateFrequencyVisibility() {
    const bool isSine = (typeSelector.getSelectedId() == 1);
    freqSlider.setEnabled(isSine);
    for (auto &btn : quickFreqButtons)
      btn.setEnabled(isSine);
  }

  void syncFromState() {
    const bool on = engine.oscEnabled.load(std::memory_order_relaxed);
    enableButton.setToggleState(on, juce::dontSendNotification);
    enableButton.setButtonText(on ? "GENERATOR: ON (ACTIVE)"
                                  : "GENERATOR: OFF");

    typeSelector.setSelectedId(engine.oscType.load(std::memory_order_relaxed) +
                                   1,
                               juce::dontSendNotification);
    freqSlider.setValue(engine.oscFrequency.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    levelSlider.setValue(engine.oscLevelDb.load(std::memory_order_relaxed),
                         juce::dontSendNotification);

    routeMainButton.setToggleState(
        engine.oscRouteMain.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    for (int a = 0; a < kVisibleAuxSends; ++a)
      routeAuxButtons[static_cast<size_t>(a)].setToggleState(
          engine.oscRouteAux[static_cast<size_t>(a)].load(
              std::memory_order_relaxed),
          juce::dontSendNotification);
    for (int m = 0; m < kVisibleMatrixBuses; ++m)
      routeMatrixButtons[static_cast<size_t>(m)].setToggleState(
          engine.oscRouteMatrix[static_cast<size_t>(m)].load(
              std::memory_order_relaxed),
          juce::dontSendNotification);

    updateFrequencyVisibility();
  }

  void timerCallback() override {
    const bool on = engine.oscEnabled.load(std::memory_order_relaxed);
    if (enableButton.getToggleState() != on) {
      enableButton.setToggleState(on, juce::dontSendNotification);
      enableButton.setButtonText(on ? "GENERATOR: ON (ACTIVE)"
                                    : "GENERATOR: OFF");
    }
  }

  MixerEngine &engine;
  juce::TextButton enableButton;
  juce::Label typeLabel;
  juce::ComboBox typeSelector;
  juce::Label freqLabel;
  juce::Slider freqSlider;
  std::array<juce::TextButton, 4> quickFreqButtons;
  juce::Label levelLabel;
  juce::Slider levelSlider;
  juce::Label routingGroupLabel;
  juce::TextButton routeMainButton;
  std::array<juce::TextButton, kVisibleAuxSends> routeAuxButtons;
  std::array<juce::TextButton, kVisibleMatrixBuses> routeMatrixButtons;
};

} // namespace nova
