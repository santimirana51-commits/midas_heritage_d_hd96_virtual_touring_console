#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class VcaPanel final : public juce::Component, private juce::Timer {
public:
  explicit VcaPanel(MixerEngine &e) : engine(e) {
    for (int i = 0; i < 24; ++i) {
      const auto idx = static_cast<size_t>(i);
      const auto fallbackName = "VCA " + juce::String(i + 1);
      if (engine.vcaNames[idx].isEmpty())
        engine.vcaNames[idx] = fallbackName;
      names[idx].setText(engine.vcaNames[idx], juce::dontSendNotification);
      names[idx].setJustification(juce::Justification::centred);
      names[idx].setSelectAllWhenFocused(true);
      names[idx].onTextChange = [this, idx] {
        auto name = names[idx].getText().trim();
        engine.vcaNames[idx] = name.isEmpty() ? "VCA " + juce::String(idx + 1)
                                              : name.substring(0, 24);
      };
      addAndMakeVisible(names[idx]);

      muteButtons[idx].setClickingTogglesState(false);
      muteButtons[idx].onClick = [this, idx] {
        const bool next = !engine.vcaMuted[idx].load(std::memory_order_relaxed);
        engine.vcaMuted[idx].store(next, std::memory_order_relaxed);
        updateMuteButton(idx);
      };
      addAndMakeVisible(muteButtons[idx]);
      updateMuteButton(idx);

      spillButtons[idx].setButtonText("SPILL");
      spillButtons[idx].setTooltip(
          "Spill member channels onto the active fader bank");
      spillButtons[idx].onClick = [this, i] {
        const int current =
            engine.activeSpillVca.load(std::memory_order_relaxed);
        engine.activeSpillVca.store(current == (i + 1) ? 0 : (i + 1),
                                    std::memory_order_relaxed);
        updateSpillButton(static_cast<size_t>(i));
      };
      addAndMakeVisible(spillButtons[idx]);
      updateSpillButton(idx);

      levels[idx].setSliderStyle(juce::Slider::LinearVertical);
      levels[idx].setRange(-60.0, 10.0, 0.1);
      levels[idx].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 20);
      levels[idx].setTextValueSuffix(" dB");
      levels[idx].setValue(
          engine.vcaMasterDb[idx].load(std::memory_order_relaxed),
          juce::dontSendNotification);
      levels[idx].onValueChange = [this, idx] {
        engine.vcaMasterDb[idx].store(
            static_cast<float>(levels[idx].getValue()),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(levels[idx]);
    }
    setSize(840, 560);
    startTimerHz(10);
  }

  void resized() override {
    const bool isCompact = (getHeight() < 340);
    auto area = getLocalBounds().reduced(isCompact ? 4 : 8);
    const int cols = 6, rows = 4;
    const int cellW = area.getWidth() / cols, cellH = area.getHeight() / rows;
    const int nameH = isCompact ? 16 : 24;
    const int btnH = isCompact ? 18 : 24;
    for (int i = 0; i < 24; ++i) {
      const int col = i % cols, row = i / cols;
      auto cell = juce::Rectangle<int>(area.getX() + col * cellW,
                                       area.getY() + row * cellH, cellW, cellH)
                      .reduced(isCompact ? 1 : 3);
      names[static_cast<size_t>(i)].setBounds(cell.removeFromTop(nameH));
      auto btnRow = cell.removeFromTop(btnH);
      muteButtons[static_cast<size_t>(i)].setBounds(
          btnRow.removeFromLeft(btnRow.getWidth() / 2).reduced(1));
      spillButtons[static_cast<size_t>(i)].setBounds(btnRow.reduced(1));
      levels[static_cast<size_t>(i)].setBounds(cell);
    }
  }

private:
  void updateMuteButton(size_t idx) {
    const bool muted = engine.vcaMuted[idx].load(std::memory_order_relaxed);
    muteButtons[idx].setButtonText(muted ? "MUTED" : "MUTE");
    muteButtons[idx].setColour(juce::TextButton::buttonColourId,
                               muted ? juce::Colour(0xffb83b45)
                                     : juce::Colour(0xff343d47));
  }

  void updateSpillButton(size_t idx) {
    const bool spilling =
        engine.activeSpillVca.load(std::memory_order_relaxed) ==
        static_cast<int>(idx + 1);
    spillButtons[idx].setColour(juce::TextButton::buttonColourId,
                                spilling ? juce::Colour(0xff00b4d8)
                                         : juce::Colour(0xff2d3540));
    spillButtons[idx].setColour(juce::TextButton::textColourOffId,
                                spilling ? juce::Colour(0xff12161a)
                                         : juce::Colour(0xfff0f3f6));
  }

  void timerCallback() override {
    for (size_t i = 0; i < levels.size(); ++i) {
      levels[i].setValue(engine.vcaMasterDb[i].load(std::memory_order_relaxed),
                         juce::dontSendNotification);
      if (!names[i].hasKeyboardFocus(true)) {
        const auto desired = engine.vcaNames[i].isEmpty()
                                 ? "VCA " + juce::String(i + 1)
                                 : engine.vcaNames[i];
        if (names[i].getText() != desired)
          names[i].setText(desired, juce::dontSendNotification);
      }
      updateMuteButton(i);
      updateSpillButton(i);
    }
  }

  MixerEngine &engine;
  std::array<juce::TextEditor, 24> names;
  std::array<juce::TextButton, 24> muteButtons;
  std::array<juce::TextButton, 24> spillButtons;
  std::array<juce::Slider, 24> levels;
};

} // namespace nova
