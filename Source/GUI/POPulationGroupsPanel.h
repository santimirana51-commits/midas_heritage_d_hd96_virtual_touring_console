#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class POPulationGroupsPanel final : public juce::Component,
                                    private juce::Timer {
public:
  explicit POPulationGroupsPanel(MixerEngine &e) : engine(e) {
    title.setText(
        "POPULATION GROUPS NAVIGATION & MULTI-CHANNEL SPILL (MIDAS CHAPTER 11)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText(
        "24 Concert POP Groups \xe2\x80\x94 1-Click Instrument Multi-Channel "
        "Fader Spill for Instant Touring Stage Navigation",
        juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    clearSpillBtn.setButtonText("EXIT / CLEAR ALL SPILL");
    clearSpillBtn.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xffd73a49));
    clearSpillBtn.onClick = [this] {
      engine.activeSpillPop.store(0, std::memory_order_relaxed);
      engine.activeSpillVca.store(0, std::memory_order_relaxed);
      updateButtons();
    };
    addAndMakeVisible(clearSpillBtn);

    for (int i = 0; i < 24; ++i) {
      const auto idx = static_cast<size_t>(i);
      names[idx].setText(engine.popNames[idx], juce::dontSendNotification);
      names[idx].setJustificationType(juce::Justification::centredLeft);
      names[idx].setEditable(true);
      names[idx].onTextChange = [this, idx] {
        auto name = names[idx].getText().trim();
        engine.popNames[idx] = name.isEmpty() ? ("POP " + juce::String(idx + 1))
                                              : name.substring(0, 28);
      };
      addAndMakeVisible(names[idx]);

      memberCountLabels[idx].setFont(
          juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      memberCountLabels[idx].setColour(juce::Label::textColourId,
                                       juce::Colour(0xff58a6ff));
      memberCountLabels[idx].setJustificationType(
          juce::Justification::centredRight);
      addAndMakeVisible(memberCountLabels[idx]);

      spillButtons[idx].setButtonText("SPILL");
      spillButtons[idx].onClick = [this, i] {
        const int cur = engine.activeSpillPop.load(std::memory_order_relaxed);
        const int next = (cur == i + 1) ? 0 : (i + 1);
        engine.activeSpillPop.store(next, std::memory_order_relaxed);
        if (next > 0)
          engine.activeSpillVca.store(0, std::memory_order_relaxed);
        updateButtons();
      };
      addAndMakeVisible(spillButtons[idx]);
    }

    setSize(920, 600);
    startTimerHz(10);
    updateButtons();
  }

  void updateButtons() {
    const int activePop = engine.activeSpillPop.load(std::memory_order_relaxed);
    for (int i = 0; i < 24; ++i) {
      const bool isSpilling = (activePop == i + 1);
      spillButtons[static_cast<size_t>(i)].setButtonText(isSpilling ? "SPILLING"
                                                                    : "SPILL");
      spillButtons[static_cast<size_t>(i)].setColour(
          juce::TextButton::buttonColourId,
          isSpilling ? juce::Colour(0xffd946ef) : juce::Colour(0xff2d3540));
    }
  }

  void timerCallback() override {
    updateButtons();
    std::array<int, 24> counts{};
    for (int c = 0; c < kLogicalChannels; ++c) {
      const int pop = engine.channels[static_cast<size_t>(c)].popGroup.load(
          std::memory_order_relaxed);
      if (pop >= 1 && pop <= 24)
        counts[static_cast<size_t>(pop - 1)]++;
    }
    for (int i = 0; i < 24; ++i) {
      memberCountLabels[static_cast<size_t>(i)].setText(
          juce::String(counts[static_cast<size_t>(i)]) + " Ch",
          juce::dontSendNotification);
    }
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(24);
    title.setBounds(top.removeFromLeft(680));
    clearSpillBtn.setBounds(top.removeFromRight(150).reduced(2));
    subTitle.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);

    const int cols = 4;
    const int rows = 6;
    const int colW = area.getWidth() / cols;
    const int rowH = area.getHeight() / rows;

    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        const int idx = r * cols + c;
        if (idx >= 24)
          break;
        auto cell = juce::Rectangle<int>(area.getX() + c * colW,
                                         area.getY() + r * rowH, colW, rowH)
                        .reduced(3);
        spillButtons[static_cast<size_t>(idx)].setBounds(
            cell.removeFromRight(64).reduced(2));
        memberCountLabels[static_cast<size_t>(idx)].setBounds(
            cell.removeFromRight(42).reduced(2));
        names[static_cast<size_t>(idx)].setBounds(cell.reduced(2));
      }
    }
  }

private:
  MixerEngine &engine;
  juce::Label title, subTitle;
  juce::TextButton clearSpillBtn;
  std::array<juce::Label, 24> names;
  std::array<juce::Label, 24> memberCountLabels;
  std::array<juce::TextButton, 24> spillButtons;
};

// ==============================================================================
// Phase 99: Scene Automation Cue List & MIDI Show Control (MSC) (Chapter 34)
// ==============================================================================

} // namespace nova
