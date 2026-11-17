#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class MonitorPanel final : public juce::Component, private juce::Timer {
public:
  explicit MonitorPanel(MixerEngine &e) : engine(e) {
    title.setText("MONITOR & CONTROL ROOM (CR) SECTION",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    labelA.setText("MONITOR A (IEM / HEADPHONES)", juce::dontSendNotification);
    labelA.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    labelA.setColour(juce::Label::textColourId, juce::Colour(0xff29b6f6));
    addAndMakeVisible(labelA);

    levelA.setSliderStyle(juce::Slider::LinearVertical);
    levelA.setRange(-60.0, 10.0, 0.5);
    levelA.setValue(engine.monitorLevelA.load(), juce::dontSendNotification);
    levelA.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    levelA.setTextValueSuffix(" dB");
    levelA.onValueChange = [this] {
      engine.monitorLevelA.store(static_cast<float>(levelA.getValue()),
                                 std::memory_order_relaxed);
    };
    addAndMakeVisible(levelA);

    dimA.setButtonText("DIM (-20dB)");
    dimA.setClickingTogglesState(true);
    dimA.setToggleState(engine.monitorDimA.load(), juce::dontSendNotification);
    dimA.setColour(juce::TextButton::buttonOnColourId,
                   juce::Colour(0xffffb300));
    dimA.onClick = [this] {
      engine.monitorDimA.store(dimA.getToggleState(),
                               std::memory_order_relaxed);
    };
    addAndMakeVisible(dimA);

    muteA.setButtonText("MUTE");
    muteA.setClickingTogglesState(true);
    muteA.setToggleState(engine.monitorMuteA.load(),
                         juce::dontSendNotification);
    muteA.setColour(juce::TextButton::buttonOnColourId,
                    juce::Colour(0xffe53935));
    muteA.onClick = [this] {
      engine.monitorMuteA.store(muteA.getToggleState(),
                                std::memory_order_relaxed);
    };
    addAndMakeVisible(muteA);

    labelB.setText("MONITOR B (WEDGE / BOOTH)", juce::dontSendNotification);
    labelB.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    labelB.setColour(juce::Label::textColourId, juce::Colour(0xffffa000));
    addAndMakeVisible(labelB);

    levelB.setSliderStyle(juce::Slider::LinearVertical);
    levelB.setRange(-60.0, 10.0, 0.5);
    levelB.setValue(engine.monitorLevelB.load(), juce::dontSendNotification);
    levelB.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    levelB.setTextValueSuffix(" dB");
    levelB.onValueChange = [this] {
      engine.monitorLevelB.store(static_cast<float>(levelB.getValue()),
                                 std::memory_order_relaxed);
    };
    addAndMakeVisible(levelB);

    dimB.setButtonText("DIM (-20dB)");
    dimB.setClickingTogglesState(true);
    dimB.setToggleState(engine.monitorDimB.load(), juce::dontSendNotification);
    dimB.setColour(juce::TextButton::buttonOnColourId,
                   juce::Colour(0xffffb300));
    dimB.onClick = [this] {
      engine.monitorDimB.store(dimB.getToggleState(),
                               std::memory_order_relaxed);
    };
    addAndMakeVisible(dimB);

    muteB.setButtonText("MUTE");
    muteB.setClickingTogglesState(true);
    muteB.setToggleState(engine.monitorMuteB.load(),
                         juce::dontSendNotification);
    muteB.setColour(juce::TextButton::buttonOnColourId,
                    juce::Colour(0xffe53935));
    muteB.onClick = [this] {
      engine.monitorMuteB.store(muteB.getToggleState(),
                                std::memory_order_relaxed);
    };
    addAndMakeVisible(muteB);

    talkbackBtn.setButtonText(engine.talkbackActive.load() ? "TALKBACK: ACTIVE"
                                                           : "TALKBACK");
    talkbackBtn.setClickingTogglesState(true);
    talkbackBtn.setToggleState(engine.talkbackActive.load(),
                               juce::dontSendNotification);
    talkbackBtn.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xff43a047));
    talkbackBtn.onClick = [this] {
      const bool act = talkbackBtn.getToggleState();
      engine.talkbackActive.store(act, std::memory_order_relaxed);
      talkbackBtn.setButtonText(act ? "TALKBACK: ACTIVE" : "TALKBACK");
    };
    addAndMakeVisible(talkbackBtn);

    sipBtn.setButtonText(engine.sipEnabled.load() ? "SIP: ON (MUTING MAIN)"
                                                  : "SOLO IN PLACE");
    sipBtn.setClickingTogglesState(true);
    sipBtn.setToggleState(engine.sipEnabled.load(), juce::dontSendNotification);
    sipBtn.setColour(juce::TextButton::buttonOnColourId,
                     juce::Colour(0xffd32f2f));
    sipBtn.onClick = [this] {
      const bool sip = sipBtn.getToggleState();
      engine.sipEnabled.store(sip, std::memory_order_relaxed);
      sipBtn.setButtonText(sip ? "SIP: ON (MUTING MAIN)" : "SOLO IN PLACE");
    };
    addAndMakeVisible(sipBtn);

    meterReadout.setText("MON A: -inf dBFS  |  MON B: -inf dBFS",
                         juce::dontSendNotification);
    meterReadout.setFont(juce::Font(juce::FontOptions(12.0f)));
    meterReadout.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(meterReadout);

    // Phase 90: Dual Output & Monitor 4-Band Parametric EQ + FX Send
    eqTitle.setText("MONITOR 4-BAND PARAMETRIC EQ & DUAL OUTPUT", juce::dontSendNotification);
    eqTitle.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    eqTitle.setColour(juce::Label::textColourId, juce::Colour(0xff00d4ff));
    addAndMakeVisible(eqTitle);

    const char* bandNames[4] = {"LF (100Hz)", "LMF (500Hz)", "HMF (2kHz)", "HF (8kHz)"};
    for (size_t b = 0; b < 4; ++b) {
      eqSliders[b].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      eqSliders[b].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
      eqSliders[b].setRange(-15.0, 15.0, 0.5);
      eqSliders[b].setValue(engine.monitorEq.gainDb[b].load(), juce::dontSendNotification);
      eqSliders[b].setTextValueSuffix(" dB");
      eqSliders[b].onValueChange = [this, b] {
        engine.monitorEq.gainDb[b].store(static_cast<float>(eqSliders[b].getValue()), std::memory_order_relaxed);
        engine.monitorEq.enabled[b].store(std::abs(eqSliders[b].getValue()) > 0.05, std::memory_order_relaxed);
      };
      addAndMakeVisible(eqSliders[b]);

      eqLabels[b].setText(bandNames[b], juce::dontSendNotification);
      eqLabels[b].setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      eqLabels[b].setJustificationType(juce::Justification::centred);
      eqLabels[b].setColour(juce::Label::textColourId, juce::Colour(0xffa0b0c0));
      addAndMakeVisible(eqLabels[b]);
    }

    fxToMonSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fxToMonSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
    fxToMonSlider.setRange(-60.0, 10.0, 0.5);
    fxToMonSlider.setValue(engine.fxToMonitorDb.load(), juce::dontSendNotification);
    fxToMonSlider.setTextValueSuffix(" dB");
    fxToMonSlider.onValueChange = [this] {
      engine.fxToMonitorDb.store(static_cast<float>(fxToMonSlider.getValue()), std::memory_order_relaxed);
    };
    addAndMakeVisible(fxToMonSlider);

    fxToMonLabel.setText("FX SEND -> MON", juce::dontSendNotification);
    fxToMonLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    fxToMonLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
    addAndMakeVisible(fxToMonLabel);

    btnEqBypass.setButtonText(engine.monitorEqBypass.load() ? "MON EQ: BYPASS" : "MON EQ: ACTIVE");
    btnEqBypass.setClickingTogglesState(true);
    btnEqBypass.setToggleState(engine.monitorEqBypass.load(), juce::dontSendNotification);
    btnEqBypass.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe53935));
    btnEqBypass.onClick = [this] {
      const bool b = btnEqBypass.getToggleState();
      engine.monitorEqBypass.store(b, std::memory_order_relaxed);
      btnEqBypass.setButtonText(b ? "MON EQ: BYPASS" : "MON EQ: ACTIVE");
    };
    addAndMakeVisible(btnEqBypass);

    btn4ChMode.setButtonText(engine.dualOutput4ChMode.load() ? "4-CH MODE: ON (OUT 3-4 = MON)" : "4-CH HARDWARE MODE");
    btn4ChMode.setClickingTogglesState(true);
    btn4ChMode.setToggleState(engine.dualOutput4ChMode.load(), juce::dontSendNotification);
    btn4ChMode.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffa855f7));
    btn4ChMode.onClick = [this] {
      const bool m = btn4ChMode.getToggleState();
      engine.dualOutput4ChMode.store(m, std::memory_order_relaxed);
      btn4ChMode.setButtonText(m ? "4-CH MODE: ON (OUT 3-4 = MON)" : "4-CH HARDWARE MODE");
    };
    addAndMakeVisible(btn4ChMode);

    setSize(540, 520);
    startTimerHz(15);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff20262d));
    g.setColour(juce::Colour(0xff3a444e));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    const bool isCompact = (getHeight() < 340 || getWidth() < 640);
    auto area = getLocalBounds().reduced(isCompact ? 6 : 14);
    title.setBounds(area.removeFromTop(isCompact ? 18 : 28));
    area.removeFromTop(isCompact ? 4 : 8);

    auto commsRow = area.removeFromTop(isCompact ? 24 : 32);
    const int tbW = isCompact ? 100 : 140;
    const int sipW = isCompact ? 120 : 170;
    talkbackBtn.setBounds(commsRow.removeFromLeft(tbW).reduced(1));
    sipBtn.setBounds(commsRow.removeFromLeft(sipW).reduced(1));
    meterReadout.setBounds(commsRow.reduced(1));

    area.removeFromTop(isCompact ? 4 : 12);
    auto monArea = area;
    const int colW = monArea.getWidth() / 2;

    auto colA = monArea.removeFromLeft(colW).reduced(isCompact ? 3 : 6);
    labelA.setBounds(colA.removeFromTop(isCompact ? 16 : 22));
    auto btnRowA = colA.removeFromBottom(isCompact ? 22 : 28);
    dimA.setBounds(btnRowA.removeFromLeft(btnRowA.getWidth() / 2).reduced(1));
    muteA.setBounds(btnRowA.reduced(1));
    levelA.setBounds(colA.reduced(isCompact ? 2 : 8, 0));

    if (isCompact) {
      auto colB = monArea.reduced(3);
      labelB.setBounds(colB.removeFromTop(16));
      auto btnRowB = colB.removeFromBottom(22);
      dimB.setBounds(btnRowB.removeFromLeft(btnRowB.getWidth() / 2).reduced(1));
      muteB.setBounds(btnRowB.reduced(1));
      levelB.setBounds(colB.reduced(2, 0));

      eqTitle.setBounds(0, 0, 0, 0);
      for (size_t b = 0; b < 4; ++b) {
        eqLabels[b].setBounds(0, 0, 0, 0);
        eqSliders[b].setBounds(0, 0, 0, 0);
      }
      fxToMonLabel.setBounds(0, 0, 0, 0);
      fxToMonSlider.setBounds(0, 0, 0, 0);
      btnEqBypass.setBounds(0, 0, 0, 0);
      btn4ChMode.setBounds(0, 0, 0, 0);
    } else {
      const int bHeight = std::max(60, monArea.getHeight() - 150);
      auto colB = monArea.removeFromTop(bHeight).reduced(6);
      labelB.setBounds(colB.removeFromTop(22));
      auto btnRowB = colB.removeFromBottom(28);
      dimB.setBounds(btnRowB.removeFromLeft(btnRowB.getWidth() / 2).reduced(2));
      muteB.setBounds(btnRowB.reduced(2));
      levelB.setBounds(colB.reduced(8, 0));

      auto eqArea = monArea;
      eqTitle.setBounds(eqArea.removeFromTop(20));
      auto eqRow = eqArea.removeFromTop(68);
      const int eqW = eqRow.getWidth() / 4;
      for (size_t b = 0; b < 4; ++b) {
        auto bandCol = eqRow.removeFromLeft(eqW);
        eqLabels[b].setBounds(bandCol.removeFromTop(16));
        eqSliders[b].setBounds(bandCol.reduced(2));
      }

      auto btmRow = eqArea.removeFromTop(28);
      fxToMonLabel.setBounds(btmRow.removeFromLeft(110));
      fxToMonSlider.setBounds(btmRow.removeFromLeft(200).reduced(2));
      btnEqBypass.setBounds(btmRow.removeFromLeft(100).reduced(2));
      btn4ChMode.setBounds(btmRow.reduced(2));
    }
  }

  void timerCallback() override {
    const float pA =
        std::max(engine.monitorPeakL_A.load(), engine.monitorPeakR_A.load());
    const float pB =
        std::max(engine.monitorPeakL_B.load(), engine.monitorPeakR_B.load());
    const auto toDb = [](float p) {
      return (p <= 1.0e-5f)
                 ? juce::String("-inf")
                 : juce::String(juce::Decibels::gainToDecibels(p), 1);
    };
    meterReadout.setText("MON A: " + toDb(pA) + " dBFS  |  MON B: " + toDb(pB) +
                             " dBFS",
                         juce::dontSendNotification);
  }

private:
  MixerEngine &engine;
  juce::Label title, labelA, labelB, meterReadout, eqTitle, fxToMonLabel;
  juce::Label eqLabels[4];
  juce::Slider levelA, levelB, fxToMonSlider;
  juce::Slider eqSliders[4];
  juce::TextButton dimA, muteA, dimB, muteB, talkbackBtn, sipBtn, btnEqBypass, btn4ChMode;
};

} // namespace nova
