#pragma once
#include "GUI/EqPanel.h"
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class GraphicEqPanel final : public juce::Component {
public:
  explicit GraphicEqPanel(MixerEngine &e) : engine(e) {
    title.setText("KLARK TEKNIK DN370 \xe2\x80\x94 31-BAND GRAPHIC EQUALISER",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    bypassButton.setButtonText(engine.graphicEqEnabled.load() ? "EQ ON (ACTIVE)"
                                                              : "EQ BYPASS");
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(engine.graphicEqEnabled.load(),
                                juce::dontSendNotification);
    bypassButton.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xff2ea44f));
    bypassButton.onClick = [this] {
      const bool on = bypassButton.getToggleState();
      engine.graphicEqEnabled.store(on, std::memory_order_relaxed);
      bypassButton.setButtonText(on ? "EQ ON (ACTIVE)" : "EQ BYPASS");
    };
    addAndMakeVisible(bypassButton);

    propQButton.setButtonText(
        engine.mainGraphicEQ.getProportionalQ() ? "PROP-Q" : "CONST-Q");
    propQButton.setClickingTogglesState(true);
    propQButton.setToggleState(engine.mainGraphicEQ.getProportionalQ(),
                               juce::dontSendNotification);
    propQButton.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xff00b4d8));
    propQButton.onClick = [this] {
      const bool prop = propQButton.getToggleState();
      engine.mainGraphicEQ.setProportionalQ(prop);
      propQButton.setButtonText(prop ? "PROP-Q" : "CONST-Q");
    };
    addAndMakeVisible(propQButton);

    flattenButton.setButtonText("FLAT");
    flattenButton.onClick = [this] {
      engine.mainGraphicEQ.flatten();
      for (size_t i = 0; i < sliders.size(); ++i)
        sliders[i].setValue(0.0, juce::dontSendNotification);
    };
    addAndMakeVisible(flattenButton);

    for (size_t i = 0; i < nova::graviton::GraphicEQ::kNumBands; ++i) {
      auto &s = sliders[i];
      s.setSliderStyle(juce::Slider::LinearVertical);
      s.setRange(-12.0, 12.0, 0.1);
      s.setValue(engine.mainGraphicEQ.getBandGain(i),
                 juce::dontSendNotification);
      s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      s.setTooltip(
          juce::String(nova::graviton::GraphicEQ::kIsoFrequencies[i], 1) +
          " Hz");
      s.onValueChange = [this, i] {
        engine.mainGraphicEQ.setBandGain(
            i, static_cast<float>(sliders[i].getValue()));
      };
      addAndMakeVisible(s);

      const double freq = nova::graviton::GraphicEQ::kIsoFrequencies[i];
      juce::String labelText =
          (freq >= 1000.0)
              ? juce::String(freq * 0.001, (freq == 1000.0 || freq == 2000.0 ||
                                            freq == 4000.0 || freq == 5000.0 ||
                                            freq == 8000.0 || freq == 10000.0 ||
                                            freq == 16000.0 || freq == 20000.0)
                                               ? 0
                                               : 1) +
                    "k"
              : juce::String(static_cast<int>(freq));
      labels[i].setText(labelText, juce::dontSendNotification);
      labels[i].setFont(juce::Font(juce::FontOptions(9.0f)));
      labels[i].setJustificationType(juce::Justification::centred);
      labels[i].setColour(juce::Label::textColourId, juce::Colour(0xffa0b0c0));
      addAndMakeVisible(labels[i]);
    }

    setSize(940, 300);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff181e24));
    g.setColour(juce::Colour(0xff2d3642));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(juce::Colour(0x30ffffff));
    g.drawHorizontalLine(175, 16.0f, static_cast<float>(getWidth() - 16));
  }

  void resized() override {
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(32);
    title.setBounds(header.removeFromLeft(480).reduced(2));
    flattenButton.setBounds(header.removeFromRight(70).reduced(2));
    propQButton.setBounds(header.removeFromRight(85).reduced(2));
    bypassButton.setBounds(header.removeFromRight(120).reduced(2));

    area.removeFromTop(8);
    auto sliderArea = area.removeFromTop(190);
    auto labelArea = area.removeFromTop(20);

    const int count = static_cast<int>(sliders.size());
    const int stepW = sliderArea.getWidth() / count;
    for (int i = 0; i < count; ++i) {
      sliders[static_cast<size_t>(i)].setBounds(
          sliderArea.removeFromLeft(stepW).reduced(1, 0));
      labels[static_cast<size_t>(i)].setBounds(
          labelArea.removeFromLeft(stepW).reduced(1, 0));
    }
  }

private:
  MixerEngine &engine;
  juce::Label title;
  juce::TextButton bypassButton, propQButton, flattenButton;
  std::array<juce::Slider, nova::graviton::GraphicEQ::kNumBands> sliders;
  std::array<juce::Label, nova::graviton::GraphicEQ::kNumBands> labels;
};

} // namespace nova
