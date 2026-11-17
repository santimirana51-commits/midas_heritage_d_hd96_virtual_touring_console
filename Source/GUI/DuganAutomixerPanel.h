#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class DuganAutomixerPanel final : public juce::Component, private juce::Timer {
public:
  explicit DuganAutomixerPanel(MixerEngine &e) : engine(e) {
    title.setText("DAN DUGAN SPEECH AUTOMIXER ENGINE (CHAPTER 28)",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    for (size_t g = 0; g < 3; ++g) {
      grpEnable[g].setButtonText("GROUP " + juce::String(g + 1) + ": ENABLED");
      grpEnable[g].setClickingTogglesState(true);
      grpEnable[g].setToggleState(
          engine.duganAutomixer.getGroup(g).enabled.load(),
          juce::dontSendNotification);
      grpEnable[g].setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff2ea44f));
      grpEnable[g].onClick = [this, g] {
        const bool on = grpEnable[g].getToggleState();
        engine.duganAutomixer.getGroup(g).enabled.store(
            on, std::memory_order_relaxed);
        grpEnable[g].setButtonText("GROUP " + juce::String(g + 1) +
                                   (on ? ": ENABLED" : ": BYPASS"));
      };
      addAndMakeVisible(grpEnable[g]);

      grpMusicOverride[g].setButtonText("MUSIC OVERRIDE DUCKER");
      grpMusicOverride[g].setClickingTogglesState(true);
      grpMusicOverride[g].setToggleState(
          engine.duganAutomixer.getGroup(g).musicOverride.load(),
          juce::dontSendNotification);
      grpMusicOverride[g].setColour(juce::TextButton::buttonOnColourId,
                                    juce::Colour(0xffe53935));
      grpMusicOverride[g].onClick = [this, g] {
        const bool mo = grpMusicOverride[g].getToggleState();
        engine.duganAutomixer.getGroup(g).musicOverride.store(
            mo, std::memory_order_relaxed);
      };
      addAndMakeVisible(grpMusicOverride[g]);

      grpDuckerSlider[g].setSliderStyle(juce::Slider::LinearHorizontal);
      grpDuckerSlider[g].setRange(-40.0, 0.0, 1.0);
      grpDuckerSlider[g].setValue(
          engine.duganAutomixer.getGroup(g).musicOverrideDuckerDb.load(),
          juce::dontSendNotification);
      grpDuckerSlider[g].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65,
                                         20);
      grpDuckerSlider[g].setTextValueSuffix(" dB DUCK");
      grpDuckerSlider[g].onValueChange = [this, g] {
        engine.duganAutomixer.getGroup(g).musicOverrideDuckerDb.store(
            static_cast<float>(grpDuckerSlider[g].getValue()),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(grpDuckerSlider[g]);

      grpFloorSlider[g].setSliderStyle(juce::Slider::LinearHorizontal);
      grpFloorSlider[g].setRange(-80.0, -30.0, 1.0);
      grpFloorSlider[g].setValue(
          engine.duganAutomixer.getGroup(g).ambientFloorDb.load(),
          juce::dontSendNotification);
      grpFloorSlider[g].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65,
                                        20);
      grpFloorSlider[g].setTextValueSuffix(" dB FLOOR");
      grpFloorSlider[g].onValueChange = [this, g] {
        engine.duganAutomixer.getGroup(g).ambientFloorDb.store(
            static_cast<float>(grpFloorSlider[g].getValue()),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(grpFloorSlider[g]);

      grpStatus[g].setFont(juce::Font(juce::FontOptions(12.0f)));
      grpStatus[g].setColour(juce::Label::textColourId,
                             juce::Colour(0xff58a6ff));
      addAndMakeVisible(grpStatus[g]);
    }

    meterBox.setMultiLine(true);
    meterBox.setReadOnly(true);
    meterBox.setFont(juce::Font(juce::FontOptions(12.0f)));
    meterBox.setColour(juce::TextEditor::backgroundColourId,
                       juce::Colour(0xff161b22));
    meterBox.setColour(juce::TextEditor::textColourId,
                       juce::Colour(0xfff0f3f6));
    addAndMakeVisible(meterBox);

    startTimerHz(10);
  }

  void timerCallback() override {
    juce::String text = "ACTIVE DUGAN CHANNELS & REAL-TIME GAIN SHARING:\n\n";
    std::array<int, 3> count{};
    for (int c = 0; c < kLogicalChannels; ++c) {
      const auto &ch = engine.channels[static_cast<size_t>(c)];
      const int g = ch.duganGroup.load(std::memory_order_relaxed);
      if (g >= 1 && g <= 3) {
        count[static_cast<size_t>(g - 1)]++;
        const float gainDb = ch.duganGainDb.load(std::memory_order_relaxed);
        const float wt = ch.duganWeightDb.load(std::memory_order_relaxed);
        text += "  CH " + juce::String(c + 1) + " [Group " + juce::String(g) +
                "]: " + "Weight " + (wt >= 0 ? "+" : "") + juce::String(wt, 1) +
                " dB | " + "Gain: " + (gainDb >= 0 ? "+" : "") +
                juce::String(gainDb, 1) + " dB";
        if (gainDb > -1.0f)
          text += "  <<< ACTIVE SPEAKER >>>";
        text += "\n";
      }
    }
    if (count[0] == 0 && count[1] == 0 && count[2] == 0) {
      text += "  (No channels currently assigned to Dugan Groups 1-3.\n"
              "   Assign speech microphones to Dugan in the Selected Channel "
              "Panel.)\n";
    }

    meterBox.setText(text);
    for (size_t g = 0; g < 3; ++g) {
      grpStatus[g].setText("Group " + juce::String(g + 1) + ": " +
                               juce::String(count[g]) + " mics assigned",
                           juce::dontSendNotification);
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff161b22));
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    const bool isCompact = getHeight() < 340;
    auto area = getLocalBounds().reduced(isCompact ? 6 : 14);
    title.setBounds(area.removeFromTop(isCompact ? 18 : 26));
    area.removeFromTop(isCompact ? 4 : 8);

    const int rowH = isCompact ? 20 : 26;
    auto topGroupArea = area.removeFromTop(rowH * 4 + 4);
    const int grpW = topGroupArea.getWidth() / 3;
    for (size_t g = 0; g < 3; ++g) {
      auto col = topGroupArea.removeFromLeft(grpW).reduced(isCompact ? 2 : 4);
      grpEnable[g].setBounds(col.removeFromTop(rowH));
      grpMusicOverride[g].setBounds(col.removeFromTop(rowH).reduced(1));
      grpDuckerSlider[g].setBounds(col.removeFromTop(rowH));
      grpFloorSlider[g].setBounds(col.removeFromTop(rowH));
      grpStatus[g].setBounds(col.reduced(1));
    }

    area.removeFromTop(isCompact ? 4 : 8);
    meterBox.setBounds(area.reduced(2));
  }

private:
  MixerEngine &engine;
  juce::Label title;
  std::array<juce::TextButton, 3> grpEnable, grpMusicOverride;
  std::array<juce::Slider, 3> grpDuckerSlider, grpFloorSlider;
  std::array<juce::Label, 3> grpStatus;
  juce::TextEditor meterBox;
};

// ==============================================================================
// Phase 84: AES50 SuperMAC & Dante Stage Box 64x64 Patch Matrix (Chapters 27 &
// 31)
// ==============================================================================

} // namespace nova
