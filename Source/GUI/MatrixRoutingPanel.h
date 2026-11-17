#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class MatrixRoutingPanel final : public juce::Component, private juce::Timer {
public:
  explicit MatrixRoutingPanel(MixerEngine &e) : engine(e) {
    for (int m = 0; m < kMatrixBusCapacity; ++m)
      matrixSelector.addItem("MATRIX " + juce::String(m + 1), m + 1);
    matrixSelector.setSelectedId(1, juce::dontSendNotification);
    matrixSelector.onChange = [this] { syncFromState(); };
    addAndMakeVisible(matrixSelector);

    for (int bank = 0; bank < kAuxBusCapacity / kVisibleAuxSends; ++bank)
      auxBankSelector.addItem(
          "AUX " + juce::String(bank * kVisibleAuxSends + 1) + "-" +
              juce::String((bank + 1) * kVisibleAuxSends),
          bank + 1);
    auxBankSelector.setSelectedId(1, juce::dontSendNotification);
    auxBankSelector.onChange = [this] { syncFromState(); };
    addAndMakeVisible(auxBankSelector);

    masterLabel.setText("MATRIX MASTER", juce::dontSendNotification);
    masterLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    masterLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(masterLabel);

    masterSlider.setSliderStyle(juce::Slider::LinearVertical);
    masterSlider.setRange(-60.0, 10.0, 0.1);
    masterSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    masterSlider.setTextValueSuffix(" dB");
    masterSlider.onValueChange = [this] {
      const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
      engine.matrixMasterDb[mIdx].store(
          static_cast<float>(masterSlider.getValue()),
          std::memory_order_relaxed);
      engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncMatrixRoutingSnapshot();
    };
    addAndMakeVisible(masterSlider);

    masterMute.setButtonText("MUTE");
    masterMute.setClickingTogglesState(true);
    masterMute.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour(0xffd64d5b));
    masterMute.onClick = [this] {
      const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
      engine.matrixMuted[mIdx].store(masterMute.getToggleState(),
                                     std::memory_order_relaxed);
      engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncMatrixRoutingSnapshot();
    };
    addAndMakeVisible(masterMute);

    const std::array<juce::String, 6> srcNames{"MAIN L", "MAIN R", "AUX 1",
                                               "AUX 2",  "AUX 3",  "AUX 4"};
    for (size_t s = 0; s < 6; ++s) {
      sourceLabels[s].setText(srcNames[s], juce::dontSendNotification);
      sourceLabels[s].setFont(
          juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
      sourceLabels[s].setColour(juce::Label::textColourId,
                                juce::Colour(0xfff0f3f6));
      addAndMakeVisible(sourceLabels[s]);

      sourceSliders[s].setSliderStyle(juce::Slider::LinearHorizontal);
      sourceSliders[s].setRange(-60.0, 10.0, 0.5);
      sourceSliders[s].setTextBoxStyle(juce::Slider::TextBoxRight, false, 58,
                                       20);
      sourceSliders[s].setTextValueSuffix(" dB");
      sourceSliders[s].onValueChange = [this, s] {
        const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
        const float val = static_cast<float>(sourceSliders[s].getValue());
        if (s == 0)
          engine.matrixMainLeftDb[mIdx].store(val, std::memory_order_relaxed);
        else if (s == 1)
          engine.matrixMainRightDb[mIdx].store(val, std::memory_order_relaxed);
        else {
          const auto auxIndex = static_cast<size_t>(
              (auxBankSelector.getSelectedId() - 1) * kVisibleAuxSends +
              static_cast<int>(s - 2));
          engine.matrixAuxSendDb[mIdx][auxIndex].store(
              val, std::memory_order_relaxed);
        }
        engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
        engine.syncMatrixRoutingSnapshot();
      };
      addAndMakeVisible(sourceSliders[s]);

      sourceMutes[s].setButtonText("M");
      sourceMutes[s].setClickingTogglesState(true);
      sourceMutes[s].setColour(juce::TextButton::buttonOnColourId,
                               juce::Colour(0xffd64d5b));
      sourceMutes[s].onClick = [this, s] {
        const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
        const bool muted = sourceMutes[s].getToggleState();
        if (s == 0 || s == 1)
          engine.matrixMainMuted[mIdx].store(muted, std::memory_order_relaxed);
        else
          engine
              .matrixAuxSendMuted[mIdx][static_cast<size_t>(
                  (auxBankSelector.getSelectedId() - 1) * kVisibleAuxSends +
                  static_cast<int>(s - 2))]
              .store(muted, std::memory_order_relaxed);
        engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
        engine.syncMatrixRoutingSnapshot();
      };
      addAndMakeVisible(sourceMutes[s]);
    }

    presetMonoMain.setButtonText("PRESET: MONO MAIN SUM");
    presetMonoMain.setTooltip(
        "Route Main L/R at -6 dB (mono sum) and mute AUX sends");
    presetMonoMain.onClick = [this] {
      const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
      engine.matrixMainLeftDb[mIdx].store(-6.0206f, std::memory_order_relaxed);
      engine.matrixMainRightDb[mIdx].store(-6.0206f, std::memory_order_relaxed);
      engine.matrixMainMuted[mIdx].store(false, std::memory_order_relaxed);
      for (int a = 0; a < kAuxBusCapacity; ++a)
        engine.matrixAuxSendDb[mIdx][static_cast<size_t>(a)].store(
            -60.0f, std::memory_order_relaxed);
      engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncMatrixRoutingSnapshot();
      syncFromState();
    };
    addAndMakeVisible(presetMonoMain);

    presetAuxSum.setButtonText("PRESET: AUX 1-4 SUM");
    presetAuxSum.setTooltip("Route AUX 1..4 at unity (0 dB) and mute Main L/R");
    presetAuxSum.onClick = [this] {
      const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
      engine.matrixMainLeftDb[mIdx].store(-60.0f, std::memory_order_relaxed);
      engine.matrixMainRightDb[mIdx].store(-60.0f, std::memory_order_relaxed);
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        engine.matrixAuxSendDb[mIdx][static_cast<size_t>(a)].store(
            a < kVisibleAuxSends ? 0.0f : -60.0f, std::memory_order_relaxed);
        engine.matrixAuxSendMuted[mIdx][static_cast<size_t>(a)].store(
            false, std::memory_order_relaxed);
      }
      engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncMatrixRoutingSnapshot();
      syncFromState();
    };
    addAndMakeVisible(presetAuxSum);

    clearAllSends.setButtonText("MUTE ALL SENDS");
    clearAllSends.onClick = [this] {
      const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
      engine.matrixMainLeftDb[mIdx].store(-60.0f, std::memory_order_relaxed);
      engine.matrixMainRightDb[mIdx].store(-60.0f, std::memory_order_relaxed);
      for (int a = 0; a < kAuxBusCapacity; ++a)
        engine.matrixAuxSendDb[mIdx][static_cast<size_t>(a)].store(
            -60.0f, std::memory_order_relaxed);
      engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncMatrixRoutingSnapshot();
      syncFromState();
    };
    addAndMakeVisible(clearAllSends);

    setSize(680, 420);
    syncFromState();
    startTimerHz(10);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff20262d));
    g.setColour(juce::Colour(0xfff0f3f6));
    g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    g.drawText("MATRIX CROSSPOINT ROUTING GRID", 16, 8, getWidth() - 32, 28,
               juce::Justification::centredLeft);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(14);
    area.removeFromTop(32);
    auto topRow = area.removeFromTop(32);
    matrixSelector.setBounds(topRow.removeFromLeft(125).reduced(2));
    auxBankSelector.setBounds(topRow.removeFromLeft(105).reduced(2));
    presetMonoMain.setBounds(topRow.removeFromLeft(170).reduced(2));
    presetAuxSum.setBounds(topRow.removeFromLeft(150).reduced(2));
    clearAllSends.setBounds(topRow.reduced(2));
    area.removeFromTop(10);

    auto masterCol = area.removeFromRight(120).reduced(4);
    masterLabel.setBounds(masterCol.removeFromTop(24));
    masterMute.setBounds(masterCol.removeFromTop(28).reduced(2));
    masterSlider.setBounds(masterCol);

    area.removeFromRight(14);
    const int rowH = juce::jmax(26, area.getHeight() / 6);
    for (size_t s = 0; s < 6; ++s) {
      auto r = area.removeFromTop(rowH);
      sourceLabels[s].setBounds(r.removeFromLeft(70).reduced(2));
      sourceMutes[s].setBounds(r.removeFromRight(32).reduced(2));
      sourceSliders[s].setBounds(r.reduced(2));
    }
  }

private:
  int getSelectedMatrixIndex() const noexcept {
    return juce::jlimit(0, kMatrixBusCapacity - 1,
                        matrixSelector.getSelectedId() - 1);
  }

  void syncFromState() {
    const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
    masterSlider.setValue(
        engine.matrixMasterDb[mIdx].load(std::memory_order_relaxed),
        juce::dontSendNotification);
    masterMute.setToggleState(
        engine.matrixMuted[mIdx].load(std::memory_order_relaxed),
        juce::dontSendNotification);

    sourceSliders[0].setValue(
        engine.matrixMainLeftDb[mIdx].load(std::memory_order_relaxed),
        juce::dontSendNotification);
    sourceSliders[1].setValue(
        engine.matrixMainRightDb[mIdx].load(std::memory_order_relaxed),
        juce::dontSendNotification);
    const bool mainMuted =
        engine.matrixMainMuted[mIdx].load(std::memory_order_relaxed);
    sourceMutes[0].setToggleState(mainMuted, juce::dontSendNotification);
    sourceMutes[1].setToggleState(mainMuted, juce::dontSendNotification);

    for (int a = 0; a < kVisibleAuxSends; ++a) {
      const auto auxIndex = static_cast<size_t>(
          (auxBankSelector.getSelectedId() - 1) * kVisibleAuxSends + a);
      sourceLabels[2 + a].setText("AUX " + juce::String(auxIndex + 1),
                                  juce::dontSendNotification);
      sourceSliders[2 + a].setValue(engine.matrixAuxSendDb[mIdx][auxIndex].load(
                                        std::memory_order_relaxed),
                                    juce::dontSendNotification);
      sourceMutes[2 + a].setToggleState(
          engine.matrixAuxSendMuted[mIdx][auxIndex].load(
              std::memory_order_relaxed),
          juce::dontSendNotification);
    }
  }

  void timerCallback() override {
    const auto mIdx = static_cast<size_t>(getSelectedMatrixIndex());
    if (!masterSlider.isMouseButtonDown())
      masterSlider.setValue(
          engine.matrixMasterDb[mIdx].load(std::memory_order_relaxed),
          juce::dontSendNotification);
  }

  MixerEngine &engine;
  juce::ComboBox matrixSelector, auxBankSelector;
  juce::Label masterLabel;
  juce::Slider masterSlider;
  juce::TextButton masterMute;
  std::array<juce::Label, 6> sourceLabels;
  std::array<juce::Slider, 6> sourceSliders;
  std::array<juce::TextButton, 6> sourceMutes;
  juce::TextButton presetMonoMain, presetAuxSum, clearAllSends;
};

} // namespace nova
