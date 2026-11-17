#pragma once

#include "Effects/GravitonNonLin2.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

/**
 * @brief Dedicated System 6000 NonLin2 Envelope & Gated Reverb UI Panel.
 */
class NonLin2Panel : public juce::Component, private juce::Timer {
public:
  explicit NonLin2Panel(nova::graviton::TCNonLin2Processor *enginePtr = nullptr)
      : engine(enginePtr) {
    setOpaque(true);
    setupControls();
    populatePresetCombo();
    syncFromEngine();
    startTimerHz(30);
  }

  ~NonLin2Panel() override { stopTimer(); }

  void setEngine(nova::graviton::TCNonLin2Processor *enginePtr) {
    engine = enginePtr;
    hasCompareSnapshot = false;
    compareActive = false;
    populatePresetCombo();
    syncFromEngine();
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // System 6000 brushed dark metallic slate background
    juce::ColourGradient bg(juce::Colour(0xFF1A1E26), 0.0f, 0.0f,
                            juce::Colour(0xFF101318), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xFF333B4A));
    g.drawRoundedRectangle(bounds.reduced(3.0f), 6.0f, 1.5f);

    // Top Global Plugin Control Bar
    g.setColour(juce::Colour(0xFF222731));
    g.fillRect(topBarRect);
    g.setColour(juce::Colour(0xFF3A4354));
    g.drawHorizontalLine(topBarRect.getBottom(), 4.0f, bounds.getWidth() - 4.0f);

    g.setColour(juce::Colour(0xFF9EB4CC));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("TRACK: SLOT 15  |  NONLIN2",
               topBarRect.getX() + 10, topBarRect.getY() + 6, 180, 22,
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFF42C8FF));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("NATIVE",
               topBarRect.getRight() - 62, topBarRect.getY() + 6, 54, 22,
               juce::Justification::centred);

    // Left panel frame (MAIN / MODIFY)
    g.setColour(juce::Colour(0xFF151820));
    g.fillRoundedRectangle(leftPanelRect.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xFF2D3544));
    g.drawRoundedRectangle(leftPanelRect.toFloat(), 5.0f, 1.0f);

    if (activeTab == 0) {
      g.setColour(juce::Colour(0xFF8FB0D0));
      g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
      g.drawText("LEVELS", leftPanelRect.getX() + 10, leftPanelRect.getY() + 36, 120, 16,
                 juce::Justification::centredLeft);
      g.drawText("ENVELOPE (P - A - H - R)", leftPanelRect.getX() + 10,
                 leftPanelRect.getY() + 196, 200, 16, juce::Justification::centredLeft);
    } else {
      g.setColour(juce::Colour(0xFF8FB0D0));
      g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
      g.drawText("MODIFY CHARACTER & TONE", leftPanelRect.getX() + 10,
                 leftPanelRect.getY() + 36, 220, 16, juce::Justification::centredLeft);
    }

    // Center Envelope Display
    drawEnvelopeDisplay(g, envDisplayRect);

    // Right Meter Bridge
    g.setColour(juce::Colour(0xFF13161D));
    g.fillRoundedRectangle(meterPanelRect.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xFF2D3544));
    g.drawRoundedRectangle(meterPanelRect.toFloat(), 5.0f, 1.0f);
    g.setColour(juce::Colour(0xFF9EB4CC));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("IN   OUT", meterPanelRect.getX() + 4, meterPanelRect.getY() + 6,
               meterPanelRect.getWidth() - 8, 16, juce::Justification::centred);

    drawVerticalMeter(g, meterInLRect, "L", meterInL);
    drawVerticalMeter(g, meterInRRect, "R", meterInR);
    drawVerticalMeter(g, meterOutLRect, "L", meterOutL);
    drawVerticalMeter(g, meterOutRRect, "R", meterOutR);

    // Bottom ASSIGN Strip (6 Favorite Slots)
    g.setColour(juce::Colour(0xFF151922));
    g.fillRoundedRectangle(assignPanelRect.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xFF2D3544));
    g.drawRoundedRectangle(assignPanelRect.toFloat(), 5.0f, 1.0f);

    drawAssignCards(g);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(8);
    topBarRect = area.removeFromTop(34);
    area.removeFromTop(6);

    // Top bar controls
    int rx = topBarRect.getRight() - 68;
    btnBypass.setBounds(rx - 74, topBarRect.getY() + 5, 68, 24);
    btnSafe.setBounds(rx - 134, topBarRect.getY() + 5, 56, 24);
    btnCompare.setBounds(rx - 210, topBarRect.getY() + 5, 72, 24);
    btnAuto.setBounds(rx - 264, topBarRect.getY() + 5, 50, 24);
    btnNextPreset.setBounds(rx - 296, topBarRect.getY() + 5, 26, 24);
    cmbPreset.setBounds(rx - 472, topBarRect.getY() + 5, 172, 24);
    btnPrevPreset.setBounds(rx - 502, topBarRect.getY() + 5, 26, 24);

    // Bottom ASSIGN strip
    assignPanelRect = area.removeFromBottom(82);
    area.removeFromBottom(6);

    // Right Meter Bridge
    meterPanelRect = area.removeFromRight(92);
    area.removeFromRight(6);

    auto mInner = meterPanelRect.reduced(10, 26);
    int mw = (mInner.getWidth() - 12) / 4;
    meterInLRect = juce::Rectangle<int>(mInner.getX(), mInner.getY(), mw, mInner.getHeight() - 14);
    meterInRRect = juce::Rectangle<int>(mInner.getX() + mw + 2, mInner.getY(), mw, mInner.getHeight() - 14);
    meterOutLRect = juce::Rectangle<int>(mInner.getX() + (mw + 2) * 2 + 6, mInner.getY(), mw, mInner.getHeight() - 14);
    meterOutRRect = juce::Rectangle<int>(mInner.getX() + (mw + 2) * 3 + 6, mInner.getY(), mw, mInner.getHeight() - 14);

    // Left MAIN / MODIFY panel
    int leftW = juce::jlimit(270, 340, static_cast<int>(area.getWidth() * 0.40f));
    leftPanelRect = area.removeFromLeft(leftW);
    area.removeFromLeft(6);
    envDisplayRect = area;

    // Tabs
    btnTabMain.setBounds(leftPanelRect.getX() + 8, leftPanelRect.getY() + 6, 90, 24);
    btnTabModify.setBounds(leftPanelRect.getX() + 102, leftPanelRect.getY() + 6, 90, 24);

    // MAIN tab sliders
    auto lInner = leftPanelRect.reduced(10, 0);
    int yLv = leftPanelRect.getY() + 54;
    for (int i = 0; i < 4; ++i) {
      lblMain[i].setBounds(lInner.getX(), yLv + i * 34, 72, 24);
      sldMain[i].setBounds(lInner.getX() + 74, yLv + i * 34, lInner.getWidth() - 74, 26);
    }
    int yEnv = leftPanelRect.getY() + 216;
    for (int i = 4; i < 8; ++i) {
      lblMain[i].setBounds(lInner.getX(), yEnv + (i - 4) * 34, 72, 24);
      sldMain[i].setBounds(lInner.getX() + 74, yEnv + (i - 4) * 34, lInner.getWidth() - 74, 26);
    }

    // MODIFY tab controls
    int yMod = leftPanelRect.getY() + 58;
    lblStyle.setBounds(lInner.getX(), yMod, 82, 24);
    cmbStyle.setBounds(lInner.getX() + 84, yMod, lInner.getWidth() - 84, 24);

    lblTwistType.setBounds(lInner.getX(), yMod + 34, 82, 24);
    cmbTwistType.setBounds(lInner.getX() + 84, yMod + 34, lInner.getWidth() - 84, 24);

    for (int i = 0; i < 5; ++i) {
      int ry = yMod + 70 + i * 36;
      lblModify[i].setBounds(lInner.getX(), ry, 82, 24);
      sldModify[i].setBounds(lInner.getX() + 84, ry, lInner.getWidth() - 84, 26);
    }

    // Bottom ASSIGN 6 slots
    auto aInner = assignPanelRect.reduced(8, 6);
    int cardGap = 6;
    int cw = (aInner.getWidth() - cardGap * 5) / 6;
    for (int i = 0; i < 6; ++i) {
      assignCardRects[i] = juce::Rectangle<int>(aInner.getX() + i * (cw + cardGap),
                                                aInner.getY(), cw, aInner.getHeight());
      int by = assignCardRects[i].getBottom() - 22;
      int bw = (cw - 12) / 2;
      btnAssignDown[i].setBounds(assignCardRects[i].getX() + 4, by, bw, 18);
      btnAssignUp[i].setBounds(assignCardRects[i].getX() + 8 + bw, by, bw, 18);
    }

    updateTabVisibility();
  }

private:
  struct CompareSnapshot {
    double inGainDb = 0.0, outGainDb = 0.0, dryGainDb = -96.0, wetGainDb = -6.0;
    double preDelayMs = 0.0, attackMs = 36.0, holdMs = 84.0, releaseMs = 204.0;
    int style = 1, twistType = 0;
    double diffusePct = 78.0, twistRatioPct = 0.0;
    double lowCutHz = 90.0, highCutHz = 2360.0, widthPct = 100.0;
  };

  void timerCallback() override {
    if (engine == nullptr)
      return;
    float inL = engine->getMeterInL();
    float inR = engine->getMeterInR();
    float outL = engine->getMeterOutL();
    float outR = engine->getMeterOutR();
    meterInL = (inL > meterInL) ? inL : meterInL * 0.85f;
    meterInR = (inR > meterInR) ? inR : meterInR * 0.85f;
    meterOutL = (outL > meterOutL) ? outL : meterOutL * 0.85f;
    meterOutR = (outR > meterOutR) ? outR : meterOutR * 0.85f;
    repaint();
  }

  void setupControls() {
    auto styleBtn = [](juce::TextButton &b, const juce::String &txt,
                       juce::Colour col = juce::Colour(0xFF28303D)) {
      b.setButtonText(txt);
      b.setColour(juce::TextButton::buttonColourId, col);
      b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF3E4C63));
      b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFE5EAF2));
      b.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFFFFFFFF));
    };

    styleBtn(btnPrevPreset, "<");
    btnPrevPreset.onClick = [this] { stepPreset(-1); };
    styleBtn(btnNextPreset, ">");
    btnNextPreset.onClick = [this] { stepPreset(1); };

    cmbPreset.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF151922));
    cmbPreset.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFE8EEF5));
    cmbPreset.onChange = [this] {
      if (suppressCallbacks || engine == nullptr || engine->isSafeMode())
        return;
      int id = cmbPreset.getSelectedId();
      if (id > 0) {
        engine->loadPreset(id - 1);
        compareActive = false;
        syncFromEngine();
      }
    };

    styleBtn(btnAuto, "AUTO");
    btnAuto.setClickingTogglesState(true);
    btnAuto.onClick = [this] {
      btnAuto.setColour(juce::TextButton::buttonColourId,
                        btnAuto.getToggleState() ? juce::Colour(0xFF2B6EA8)
                                                 : juce::Colour(0xFF28303D));
    };

    styleBtn(btnCompare, "COMPARE");
    btnCompare.onClick = [this] { toggleCompare(); };

    styleBtn(btnSafe, "SAFE");
    btnSafe.setClickingTogglesState(true);
    btnSafe.onClick = [this] {
      if (engine)
        engine->setSafeMode(btnSafe.getToggleState());
      updateSafeState();
    };

    styleBtn(btnBypass, "BYPASS");
    btnBypass.setClickingTogglesState(true);
    btnBypass.onClick = [this] {
      if (engine)
        engine->setBypassed(btnBypass.getToggleState());
      btnBypass.setColour(juce::TextButton::buttonColourId,
                          btnBypass.getToggleState() ? juce::Colour(0xFFA62B2B)
                                                     : juce::Colour(0xFF28303D));
    };

    for (juce::Component *c : {static_cast<juce::Component *>(&btnPrevPreset),
                               static_cast<juce::Component *>(&cmbPreset),
                               static_cast<juce::Component *>(&btnNextPreset),
                               static_cast<juce::Component *>(&btnAuto),
                               static_cast<juce::Component *>(&btnCompare),
                               static_cast<juce::Component *>(&btnSafe),
                               static_cast<juce::Component *>(&btnBypass)})
      addAndMakeVisible(*c);

    // Tabs
    styleBtn(btnTabMain, "MAIN");
    styleBtn(btnTabModify, "MODIFY");
    btnTabMain.onClick = [this] {
      activeTab = 0;
      updateTabVisibility();
      repaint();
    };
    btnTabModify.onClick = [this] {
      activeTab = 1;
      updateTabVisibility();
      repaint();
    };
    addAndMakeVisible(btnTabMain);
    addAndMakeVisible(btnTabModify);

    // MAIN Sliders (0..3 Levels, 4..7 Envelope)
    const char *mainNames[8] = {"In Gain", "Out Gain", "Dry Gain", "Wet Gain",
                                "Pre Delay", "Attack", "Hold", "Release"};
    const double mainMin[8] = {-48.0, -48.0, -64.0, -64.0, 0.0, 0.0, 0.0, 0.0};
    const double mainMax[8] = {12.0, 12.0, 0.0, 6.0, 500.0, 500.0, 1000.0, 1000.0};
    const double mainStep[8] = {0.5, 0.5, 0.5, 0.5, 1.0, 1.0, 1.0, 1.0};
    const char *mainSuf[8] = {" dB", " dB", " dB", " dB", " ms", " ms", " ms", " ms"};

    for (int i = 0; i < 8; ++i) {
      lblMain[i].setText(mainNames[i], juce::dontSendNotification);
      lblMain[i].setColour(juce::Label::textColourId, juce::Colour(0xFFC4CEDB));
      lblMain[i].setFont(juce::FontOptions(11.5f, juce::Font::plain));
      addAndMakeVisible(lblMain[i]);

      sldMain[i].setSliderStyle(juce::Slider::LinearHorizontal);
      sldMain[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
      sldMain[i].setRange(mainMin[i], mainMax[i], mainStep[i]);
      sldMain[i].setTextValueSuffix(mainSuf[i]);
      sldMain[i].setColour(juce::Slider::trackColourId, juce::Colour(0xFF39B8FF));
      sldMain[i].setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE8EEF5));
      sldMain[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFE8EEF5));
      sldMain[i].setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0E1117));
      sldMain[i].onValueChange = [this, i] { handleMainSliderChanged(i); };
      addAndMakeVisible(sldMain[i]);
    }
    sldMain[2].textFromValueFunction = [](double v) -> juce::String {
      return (v <= -60.0) ? "OFF" : juce::String(v, 1) + " dB";
    };
    sldMain[3].textFromValueFunction = [](double v) -> juce::String {
      return (v <= -60.0) ? "OFF" : juce::String(v, 1) + " dB";
    };

    // MODIFY Controls
    lblStyle.setText("Reverb Style", juce::dontSendNotification);
    lblStyle.setColour(juce::Label::textColourId, juce::Colour(0xFFC4CEDB));
    addAndMakeVisible(lblStyle);

    cmbStyle.addItem("NonLin A", 1);
    cmbStyle.addItem("NonLin B", 2);
    cmbStyle.addItem("NonLin C", 3);
    cmbStyle.addItem("NonLin D", 4);
    cmbStyle.addItem("Gated 80s", 5);
    cmbStyle.addItem("Reverse", 6);
    cmbStyle.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0E1117));
    cmbStyle.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFE8EEF5));
    cmbStyle.onChange = [this] {
      if (suppressCallbacks || engine == nullptr)
        return;
      int id = cmbStyle.getSelectedId();
      if (id >= 1 && id <= 6)
        engine->setReverbStyle(static_cast<nova::graviton::TCNonLin2Processor::ReverbStyle>(id - 1));
      repaint();
    };
    addAndMakeVisible(cmbStyle);

    lblTwistType.setText("Twist Type", juce::dontSendNotification);
    lblTwistType.setColour(juce::Label::textColourId, juce::Colour(0xFFC4CEDB));
    addAndMakeVisible(lblTwistType);

    cmbTwistType.addItem("Off", 1);
    cmbTwistType.addItem("Detune", 2);
    cmbTwistType.addItem("Modulate", 3);
    cmbTwistType.addItem("Crunch", 4);
    cmbTwistType.addItem("Comb", 5);
    cmbTwistType.addItem("Phase", 6);
    cmbTwistType.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0E1117));
    cmbTwistType.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFE8EEF5));
    cmbTwistType.onChange = [this] {
      if (suppressCallbacks || engine == nullptr)
        return;
      int id = cmbTwistType.getSelectedId();
      if (id >= 1 && id <= 6)
        engine->setTwistType(static_cast<nova::graviton::TCNonLin2Processor::TwistType>(id - 1));
      repaint();
    };
    addAndMakeVisible(cmbTwistType);

    const char *modNames[5] = {"Diffuse", "Twist Ratio", "Lo Cut", "Hi Cut", "Width"};
    const double modMin[5] = {0.0, 0.0, 20.0, 1000.0, 0.0};
    const double modMax[5] = {100.0, 100.0, 1000.0, 20000.0, 100.0};
    const double modStep[5] = {1.0, 1.0, 5.0, 20.0, 1.0};
    const char *modSuf[5] = {" %", " %", " Hz", " Hz", " %"};

    for (int i = 0; i < 5; ++i) {
      lblModify[i].setText(modNames[i], juce::dontSendNotification);
      lblModify[i].setColour(juce::Label::textColourId, juce::Colour(0xFFC4CEDB));
      addAndMakeVisible(lblModify[i]);

      sldModify[i].setSliderStyle(juce::Slider::LinearHorizontal);
      sldModify[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
      sldModify[i].setRange(modMin[i], modMax[i], modStep[i]);
      sldModify[i].setTextValueSuffix(modSuf[i]);
      sldModify[i].setColour(juce::Slider::trackColourId, juce::Colour(0xFFFFA834));
      sldModify[i].setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE8EEF5));
      sldModify[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFE8EEF5));
      sldModify[i].setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0E1117));
      sldModify[i].onValueChange = [this, i] { handleModifySliderChanged(i); };
      addAndMakeVisible(sldModify[i]);
    }

    // Bottom 6 ASSIGN quick-step buttons
    for (int i = 0; i < 6; ++i) {
      styleBtn(btnAssignDown[i], "-");
      styleBtn(btnAssignUp[i], "+");
      btnAssignDown[i].onClick = [this, i] { stepAssignSlot(i, -1); };
      btnAssignUp[i].onClick = [this, i] { stepAssignSlot(i, 1); };
      addAndMakeVisible(btnAssignDown[i]);
      addAndMakeVisible(btnAssignUp[i]);
    }
  }

  void updateTabVisibility() {
    const bool isMain = (activeTab == 0);
    btnTabMain.setColour(juce::TextButton::buttonColourId,
                         isMain ? juce::Colour(0xFF2B6EA8) : juce::Colour(0xFF232934));
    btnTabModify.setColour(juce::TextButton::buttonColourId,
                           !isMain ? juce::Colour(0xFFB56E1F) : juce::Colour(0xFF232934));

    for (int i = 0; i < 8; ++i) {
      lblMain[i].setVisible(isMain);
      sldMain[i].setVisible(isMain);
    }
    lblStyle.setVisible(!isMain);
    cmbStyle.setVisible(!isMain);
    lblTwistType.setVisible(!isMain);
    cmbTwistType.setVisible(!isMain);
    for (int i = 0; i < 5; ++i) {
      lblModify[i].setVisible(!isMain);
      sldModify[i].setVisible(!isMain);
    }
  }

  void updateSafeState() {
    const bool locked = (engine != nullptr && engine->isSafeMode());
    btnSafe.setColour(juce::TextButton::buttonColourId,
                      locked ? juce::Colour(0xFFB0781B) : juce::Colour(0xFF28303D));
    for (int i = 0; i < 8; ++i)
      sldMain[i].setEnabled(!locked);
    for (int i = 0; i < 5; ++i)
      sldModify[i].setEnabled(!locked);
    cmbStyle.setEnabled(!locked);
    cmbTwistType.setEnabled(!locked);
    cmbPreset.setEnabled(!locked);
    for (int i = 0; i < 6; ++i) {
      btnAssignDown[i].setEnabled(!locked);
      btnAssignUp[i].setEnabled(!locked);
    }
  }

  void populatePresetCombo() {
    suppressCallbacks = true;
    cmbPreset.clear(juce::dontSendNotification);
    if (engine != nullptr) {
      const auto &presets = engine->getFactoryPresets();
      for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        cmbPreset.addItem(presets[static_cast<size_t>(i)].name, i + 1);
      }
    } else {
      cmbPreset.addItem("301 Tight NonLin", 1);
    }
    suppressCallbacks = false;
  }

  void syncFromEngine() {
    if (engine == nullptr)
      return;
    suppressCallbacks = true;
    cmbPreset.setSelectedId(engine->getCurrentPresetIndex() + 1, juce::dontSendNotification);
    btnBypass.setToggleState(engine->isBypassed(), juce::dontSendNotification);
    btnBypass.setColour(juce::TextButton::buttonColourId,
                        engine->isBypassed() ? juce::Colour(0xFFA62B2B) : juce::Colour(0xFF28303D));
    btnSafe.setToggleState(engine->isSafeMode(), juce::dontSendNotification);

    sldMain[0].setValue(engine->getInGainDb(), juce::dontSendNotification);
    sldMain[1].setValue(engine->getOutGainDb(), juce::dontSendNotification);
    sldMain[2].setValue(engine->isDryOff() ? -64.0 : engine->getDryGainDb(), juce::dontSendNotification);
    sldMain[3].setValue(engine->getWetGainDb() <= -60.0 ? -64.0 : engine->getWetGainDb(), juce::dontSendNotification);
    sldMain[4].setValue(engine->getPreDelayMs(), juce::dontSendNotification);
    sldMain[5].setValue(engine->getAttackMs(), juce::dontSendNotification);
    sldMain[6].setValue(engine->getHoldMs(), juce::dontSendNotification);
    sldMain[7].setValue(engine->getReleaseMs(), juce::dontSendNotification);

    cmbStyle.setSelectedId(static_cast<int>(engine->getReverbStyle()) + 1, juce::dontSendNotification);
    cmbTwistType.setSelectedId(static_cast<int>(engine->getTwistType()) + 1, juce::dontSendNotification);
    sldModify[0].setValue(engine->getDiffusePct(), juce::dontSendNotification);
    sldModify[1].setValue(engine->getTwistRatioPct(), juce::dontSendNotification);
    sldModify[2].setValue(engine->getLowCutHz(), juce::dontSendNotification);
    sldModify[3].setValue(engine->getHighCutHz(), juce::dontSendNotification);
    sldModify[4].setValue(engine->getWidthPct(), juce::dontSendNotification);

    updateSafeState();
    suppressCallbacks = false;
  }

  void handleMainSliderChanged(int idx) {
    if (suppressCallbacks || engine == nullptr)
      return;
    double v = sldMain[idx].getValue();
    engine->setLevelMode(true);
    switch (idx) {
    case 0: engine->setInGainDb(v); break;
    case 1: engine->setOutGainDb(v); break;
    case 2: engine->setDryGainDb(v <= -60.0 ? -96.0 : v); break;
    case 3: engine->setWetGainDb(v <= -60.0 ? -96.0 : v); break;
    case 4: engine->setPreDelayMs(v); break;
    case 5: engine->setAttackMs(v); break;
    case 6: engine->setHoldMs(v); break;
    case 7: engine->setReleaseMs(v); break;
    }
    repaint();
  }

  void handleModifySliderChanged(int idx) {
    if (suppressCallbacks || engine == nullptr)
      return;
    double v = sldModify[idx].getValue();
    switch (idx) {
    case 0: engine->setDiffusePct(v); break;
    case 1: engine->setTwistRatioPct(v); break;
    case 2: engine->setLowCutHz(v); break;
    case 3: engine->setHighCutHz(v); break;
    case 4: engine->setWidthPct(v); break;
    }
    repaint();
  }

  void stepPreset(int delta) {
    if (engine == nullptr || engine->isSafeMode())
      return;
    int n = static_cast<int>(engine->getFactoryPresets().size());
    int next = (engine->getCurrentPresetIndex() + delta + n) % n;
    engine->loadPreset(next);
    compareActive = false;
    syncFromEngine();
    repaint();
  }

  void stepAssignSlot(int slot, int dir) {
    if (engine == nullptr || engine->isSafeMode())
      return;
    switch (slot) {
    case 0: engine->setPreDelayMs(engine->getPreDelayMs() + dir * 5.0); break;
    case 1: engine->setAttackMs(engine->getAttackMs() + dir * 5.0); break;
    case 2: engine->setHoldMs(engine->getHoldMs() + dir * 10.0); break;
    case 3: engine->setReleaseMs(engine->getReleaseMs() + dir * 10.0); break;
    case 4: engine->setHighCutHz(engine->getHighCutHz() + dir * 250.0); break;
    case 5: {
      int s = (static_cast<int>(engine->getReverbStyle()) + dir + 6) % 6;
      engine->setReverbStyle(static_cast<nova::graviton::TCNonLin2Processor::ReverbStyle>(s));
      break;
    }
    }
    syncFromEngine();
    repaint();
  }

  void toggleCompare() {
    if (engine == nullptr)
      return;
    if (!hasCompareSnapshot) {
      captureSnapshot(snapshotA);
      hasCompareSnapshot = true;
      compareActive = false;
      btnCompare.setButtonText("STORED A");
      btnCompare.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2B4D6F));
      return;
    }
    if (!compareActive) {
      captureSnapshot(snapshotB);
      applySnapshot(snapshotA);
      compareActive = true;
      btnCompare.setButtonText("CMP [A]");
      btnCompare.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFB56E1F));
    } else {
      applySnapshot(snapshotB);
      compareActive = false;
      btnCompare.setButtonText("CMP [B]");
      btnCompare.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2B4D6F));
    }
    syncFromEngine();
    repaint();
  }

  void captureSnapshot(CompareSnapshot &s) {
    s.inGainDb = engine->getInGainDb();
    s.outGainDb = engine->getOutGainDb();
    s.dryGainDb = engine->getDryGainDb();
    s.wetGainDb = engine->getWetGainDb();
    s.preDelayMs = engine->getPreDelayMs();
    s.attackMs = engine->getAttackMs();
    s.holdMs = engine->getHoldMs();
    s.releaseMs = engine->getReleaseMs();
    s.style = static_cast<int>(engine->getReverbStyle());
    s.twistType = static_cast<int>(engine->getTwistType());
    s.diffusePct = engine->getDiffusePct();
    s.twistRatioPct = engine->getTwistRatioPct();
    s.lowCutHz = engine->getLowCutHz();
    s.highCutHz = engine->getHighCutHz();
    s.widthPct = engine->getWidthPct();
  }

  void applySnapshot(const CompareSnapshot &s) {
    engine->setInGainDb(s.inGainDb);
    engine->setOutGainDb(s.outGainDb);
    engine->setDryGainDb(s.dryGainDb);
    engine->setWetGainDb(s.wetGainDb);
    engine->setPreDelayMs(s.preDelayMs);
    engine->setAttackMs(s.attackMs);
    engine->setHoldMs(s.holdMs);
    engine->setReleaseMs(s.releaseMs);
    engine->setReverbStyle(static_cast<nova::graviton::TCNonLin2Processor::ReverbStyle>(s.style));
    engine->setTwistType(static_cast<nova::graviton::TCNonLin2Processor::TwistType>(s.twistType));
    engine->setDiffusePct(s.diffusePct);
    engine->setTwistRatioPct(s.twistRatioPct);
    engine->setLowCutHz(s.lowCutHz);
    engine->setHighCutHz(s.highCutHz);
    engine->setWidthPct(s.widthPct);
  }

  void drawEnvelopeDisplay(juce::Graphics &g, juce::Rectangle<int> r) {
    g.setColour(juce::Colour(0xFF0A0D12));
    g.fillRoundedRectangle(r.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xFF2E3848));
    g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);

    auto plot = r.reduced(16, 26).toFloat();
    g.setColour(juce::Colour(0xFF9EB4CC));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("ENVELOPE DISPLAY", r.getX() + 12, r.getY() + 6, 180, 16,
               juce::Justification::centredLeft);

    // Grid lines
    g.setColour(juce::Colour(0x18FFFFFF));
    for (int i = 1; i < 4; ++i) {
      float gy = plot.getY() + plot.getHeight() * (static_cast<float>(i) / 4.0f);
      g.drawHorizontalLine(static_cast<int>(gy), plot.getX(), plot.getRight());
    }

    if (engine == nullptr)
      return;

    const double pMs = engine->getPreDelayMs();
    const double aMs = engine->getAttackMs();
    const double hMs = engine->getHoldMs();
    const double rMs = engine->getReleaseMs();
    const double totalMs = std::max(50.0, pMs + aMs + hMs + rMs);

    auto xAt = [&](double ms) -> float {
      return plot.getX() + static_cast<float>(ms / totalMs) * plot.getWidth();
    };
    const float yBase = plot.getBottom();
    const float yTop = plot.getY() + 10.0f;

    const float xP = xAt(pMs);
    const float xA = xAt(pMs + aMs);
    const float xH = xAt(pMs + aMs + hMs);
    const float xR = xAt(pMs + aMs + hMs + rMs);

    juce::Path fillPath;
    fillPath.startNewSubPath(plot.getX(), yBase);
    fillPath.lineTo(xP, yBase);
    fillPath.lineTo(xA, yTop);
    fillPath.lineTo(xH, yTop);
    fillPath.lineTo(xR, yBase);
    fillPath.closeSubPath();

    juce::ColourGradient fillGrad(juce::Colour(0x5539B8FF), 0.0f, yTop,
                                  juce::Colour(0x0839B8FF), 0.0f, yBase, false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    juce::Path strokePath;
    strokePath.startNewSubPath(plot.getX(), yBase);
    strokePath.lineTo(xP, yBase);
    strokePath.lineTo(xA, yTop);
    strokePath.lineTo(xH, yTop);
    strokePath.lineTo(xR, yBase);

    g.setColour(juce::Colour(0xFF42C8FF));
    g.strokePath(strokePath, juce::PathStrokeType(2.2f));

    // Nodes P, A, H, R
    auto drawNode = [&](float nx, float ny, const char *tag) {
      g.setColour(juce::Colour(0xFFFFB238));
      g.fillEllipse(nx - 4.0f, ny - 4.0f, 8.0f, 8.0f);
      g.setColour(juce::Colour(0xFFFFFFFF));
      g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
      g.drawText(tag, static_cast<int>(nx - 8), static_cast<int>(ny - 18), 16, 14,
                 juce::Justification::centred);
    };
    drawNode(xP, yBase, "P");
    drawNode(xA, yTop, "A");
    drawNode(xH, yTop, "H");
    drawNode(xR, yBase, "R");

    static const char *kStyleNames[] = {"NonLin A", "NonLin B", "NonLin C", "NonLin D", "Gated 80s", "Reverse"};
    static const char *kTwistNames[] = {"Off", "Detune", "Modulate", "Crunch", "Comb", "Phase"};
    int sIdx = juce::jlimit(0, 5, static_cast<int>(engine->getReverbStyle()));
    int tIdx = juce::jlimit(0, 5, static_cast<int>(engine->getTwistType()));
    g.setColour(juce::Colour(0xFF8FB0D0));
    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    g.drawText(juce::String("Style: ") + kStyleNames[sIdx] + "  |  Twist: " + kTwistNames[tIdx] +
                   "  |  Total: " + juce::String(juce::roundToInt(totalMs)) + " ms",
               r.getX() + 12, r.getBottom() - 20, r.getWidth() - 24, 16,
               juce::Justification::centredRight);
  }

  void drawVerticalMeter(juce::Graphics &g, juce::Rectangle<int> r, const char *lbl, float lin) {
    constexpr int kSegs = 16;
    float db = (lin > 1e-6f) ? 20.0f * std::log10(lin) : -60.0f;
    float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    int lit = static_cast<int>(std::round(norm * kSegs));
    int gap = 2;
    float segH = static_cast<float>(r.getHeight() - (kSegs - 1) * gap) / kSegs;

    for (int i = 0; i < kSegs; ++i) {
      juce::Colour c = (i >= 14) ? juce::Colour(0xFFFF3A3A)
                       : (i >= 11 ? juce::Colour(0xFFFFB224) : juce::Colour(0xFF42E65B));
      g.setColour(i < lit ? c : juce::Colour(0xFF1E242E));
      float sy = static_cast<float>(r.getBottom()) - static_cast<float>(i + 1) * (segH + gap);
      g.fillRect(static_cast<float>(r.getX()), sy, static_cast<float>(r.getWidth()), segH);
    }

    g.setColour(juce::Colour(0xFF8D98AA));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(lbl, r.getX(), r.getBottom() + 2, r.getWidth(), 12, juce::Justification::centred);
  }

  void drawAssignCards(juce::Graphics &g) {
    if (engine == nullptr)
      return;
    static const char *kSlotTitles[6] = {
        "1: PRE DELAY", "2: ATTACK", "3: HOLD", "4: RELEASE", "5: HI CUT", "6: STYLE"};
    static const char *kStyleNames[6] = {
        "NonLin A", "NonLin B", "NonLin C", "NonLin D", "Gated 80s", "Reverse"};

    juce::String vals[6] = {
        juce::String(juce::roundToInt(engine->getPreDelayMs())) + " ms",
        juce::String(juce::roundToInt(engine->getAttackMs())) + " ms",
        juce::String(juce::roundToInt(engine->getHoldMs())) + " ms",
        juce::String(juce::roundToInt(engine->getReleaseMs())) + " ms",
        engine->getHighCutHz() >= 1000.0
            ? juce::String(engine->getHighCutHz() * 0.001, 2) + " kHz"
            : juce::String(juce::roundToInt(engine->getHighCutHz())) + " Hz",
        kStyleNames[juce::jlimit(0, 5, static_cast<int>(engine->getReverbStyle()))]};

    for (int i = 0; i < 6; ++i) {
      auto cr = assignCardRects[i];
      g.setColour(juce::Colour(0xFF0E1117));
      g.fillRoundedRectangle(cr.toFloat(), 4.0f);
      g.setColour(juce::Colour(0xFF2C3442));
      g.drawRoundedRectangle(cr.toFloat(), 4.0f, 1.0f);

      g.setColour(juce::Colour(0xFF8C9BB0));
      g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
      g.drawText(kSlotTitles[i], cr.getX() + 4, cr.getY() + 4, cr.getWidth() - 8, 12,
                 juce::Justification::centred);

      g.setColour(juce::Colour(0xFF42C8FF));
      g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
      g.drawText(vals[i], cr.getX() + 4, cr.getY() + 18, cr.getWidth() - 8, 20,
                 juce::Justification::centred);
    }
  }

  nova::graviton::TCNonLin2Processor *engine = nullptr;
  int activeTab = 0; // 0 = MAIN, 1 = MODIFY
  bool suppressCallbacks = false;

  bool hasCompareSnapshot = false;
  bool compareActive = false;
  CompareSnapshot snapshotA, snapshotB;

  float meterInL = 0.0f, meterInR = 0.0f;
  float meterOutL = 0.0f, meterOutR = 0.0f;

  juce::Rectangle<int> topBarRect, leftPanelRect, envDisplayRect, meterPanelRect, assignPanelRect;
  juce::Rectangle<int> meterInLRect, meterInRRect, meterOutLRect, meterOutRRect;
  juce::Rectangle<int> assignCardRects[6];

  juce::TextButton btnPrevPreset, btnNextPreset, btnAuto, btnCompare, btnSafe, btnBypass;
  juce::ComboBox cmbPreset;
  juce::TextButton btnTabMain, btnTabModify;

  juce::Label lblMain[8];
  juce::Slider sldMain[8];

  juce::Label lblStyle, lblTwistType, lblModify[5];
  juce::ComboBox cmbStyle, cmbTwistType;
  juce::Slider sldModify[5];

  juce::TextButton btnAssignDown[6], btnAssignUp[6];

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NonLin2Panel)
};

} // namespace nova
