#pragma once

#include "Effects/GravitonTC2290.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

/**
 * @brief Hardware-authentic UI panel for NOVA HD 2290 Dynamic Digital Delay
 *        (inspired by TC Electronic TC 2290 / TC 2290-DT).
 *        Designed with zero-overlap responsive geometry for wide rack-strip
 * hosts.
 */
class TC2290Panel : public juce::Component, private juce::Timer {
public:
  explicit TC2290Panel(nova::graviton::TC2290Delay *enginePtr = nullptr)
      : engine(enginePtr) {
    setOpaque(true);
    setupControls();
    populatePresetCombo();
    syncFromEngine();
    startTimerHz(30);
  }

  ~TC2290Panel() override { stopTimer(); }

  void setEngine(nova::graviton::TC2290Delay *enginePtr) {
    engine = enginePtr;
    compareActive = false;
    hasCompareSnapshot = false;
    populatePresetCombo();
    syncFromEngine();
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Dark brushed-anodized chassis background
    juce::ColourGradient bgGrad(juce::Colour(0xFF14161A), 0.0f, 0.0f,
                                juce::Colour(0xFF0B0C0F), 0.0f,
                                bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Subtle horizontal brushed-metal lines
    g.setColour(juce::Colour(0x08FFFFFF));
    for (int y = 32; y < getHeight(); y += 4)
      g.drawHorizontalLine(y, 6.0f, bounds.getWidth() - 6.0f);

    // Outer rack border & corner hex screws
    g.setColour(juce::Colour(0xFF2B2F38));
    g.drawRoundedRectangle(bounds.reduced(2.0f), 5.0f, 1.5f);
    drawRackScrew(g, 11.0f, 12.0f);
    drawRackScrew(g, bounds.getWidth() - 11.0f, 12.0f);
    drawRackScrew(g, 11.0f, bounds.getHeight() - 12.0f);
    drawRackScrew(g, bounds.getWidth() - 11.0f, bounds.getHeight() - 12.0f);

    // Header strip
    g.setColour(juce::Colour(0xFF1A1D24));
    g.fillRect(headerRect);
    g.setColour(juce::Colour(0xFF343946));
    g.drawHorizontalLine(headerRect.getBottom(), 4.0f,
                         bounds.getWidth() - 4.0f);

    g.setColour(juce::Colour(0xFFE8ECF2));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("NOVA HD 2290  //  DYNAMIC DIGITAL DELAY",
               headerRect.getX() + 74, headerRect.getY() + 2, 320,
               headerRect.getHeight() - 4, juce::Justification::centredLeft);

    // Section frames
    drawSectionFrame(g, meterRect, "INPUT / OUTPUT LEVEL (dB)");
    drawSectionFrame(g, modRect, "MODULATION");
    drawSectionFrame(g, panDynRect, "PAN - DYN");
    drawSectionFrame(g, delayRect, "DELAY");
    drawSectionFrame(g, feedbackRect, "FEEDBACK");
    drawSectionFrame(g, presetSpecRect, "PRESET / SPEC.");
    drawSectionFrame(g, keypadRect, "KEYBOARD");

    if (showSpecialPanel) {
      drawSectionFrame(g, spInputRect, "INPUT");
      drawSectionFrame(g, spDelayOnRect, "DELAY ON");
      drawSectionFrame(g, spOutputRect, "OUTPUT");
      drawSectionFrame(g, spSyncRect, "DAW SYNC");
      drawSectionFrame(g, spDelayModRect, "DELAY MOD");
      drawSectionFrame(g, spThreshRect, "THRESHOLDS");
      drawSectionFrame(g, spPresetInfoRect, "PRESET");
    }

    // Meter Bridge LED bars
    drawMeterBar(g, meterInLRect, "IN L", meterInL);
    drawMeterBar(g, meterInRRect, "IN R", meterInR);
    drawMeterBar(g, meterOutLRect, "OUT L", meterOutL);
    drawMeterBar(g, meterOutRRect, "OUT R", meterOutR);

    if (engine == nullptr)
      return;

    const bool isTypingMod = (activeNumericTarget == NumericTarget::ModSpeed ||
                              activeNumericTarget == NumericTarget::ModDepth);
    const bool isTypingDelay =
        (activeNumericTarget == NumericTarget::DelayTime);
    const bool isTypingFb = (activeNumericTarget == NumericTarget::FbLevel ||
                             activeNumericTarget == NumericTarget::FbHigh ||
                             activeNumericTarget == NumericTarget::FbLow);
    const bool isTypingPreset =
        (activeNumericTarget == NumericTarget::PresetNum);

    // Modulation display
    auto modSel = engine->getModTargetSelect();
    juce::String modDispStr;
    if (isTypingMod && keypadBuffer.isNotEmpty()) {
      modDispStr = keypadBuffer;
    } else if (modShowDepth) {
      modDispStr =
          juce::String(juce::roundToInt(engine->getTargetModDepthPct(modSel))) +
          "%";
    } else {
      modDispStr = juce::String(engine->getTargetModSpeedHz(modSel), 1);
    }
    drawSevenSegBox(g, modDisplayRect, modDispStr, isTypingMod,
                    modShowDepth ? "DPT%" : "SPD");

    // Mod Waveform & Select compact LEDs
    drawCompactLedList(g, modWaveLedRect, {"SIN", "RND", "ENV", "TRG"},
                       static_cast<int>(engine->getTargetModWaveform(modSel)));
    drawCompactLedList(g, modSelLedRect, {"DLY", "PAN", "DYN"},
                       static_cast<int>(modSel));

    // OSC / TRIG status LEDs
    drawStatusDot(g, modRect.getX() + 10, modRect.getY() + 12,
                  engine->isDelayModEnabled() || engine->isPanModEnabled() ||
                      engine->isDynModEnabled(),
                  juce::Colour(0xFF42E66B));
    drawStatusDot(g, modRect.getRight() - 14, modRect.getY() + 12,
                  engine->isTrigActive(), juce::Colour(0xFFFF3A3A));

    // Pan - Dyn LEDs (beside buttons, zero overlap)
    drawCompactLedList(g, panTargetLedRect, {"DELAY", "DIRECT", "BOTH"},
                       static_cast<int>(engine->getPanTarget()));

    // Delay 7-segment display
    juce::String delayStr;
    if (isTypingDelay && keypadBuffer.isNotEmpty())
      delayStr = keypadBuffer;
    else
      delayStr = juce::String(juce::roundToInt(engine->getDelayTimeMs()));
    drawSevenSegBox(g, delayDisplayRect, delayStr, isTypingDelay,
                    engine->isTempoSync() ? "SYNC ms" : "TIME ms");

    // Feedback 7-segment display
    auto fbSel = engine->getFeedbackSelect();
    juce::String fbStr;
    if (isTypingFb && keypadBuffer.isNotEmpty()) {
      fbStr = keypadBuffer;
    } else if (fbSel == nova::graviton::TC2290Delay::FeedbackSelect::Level) {
      fbStr = juce::String(juce::roundToInt(engine->getFeedback() * 100.0f));
    } else if (fbSel == nova::graviton::TC2290Delay::FeedbackSelect::High) {
      fbStr = juce::String(juce::roundToInt(engine->getHighCutHz()));
    } else {
      fbStr = juce::String(juce::roundToInt(engine->getLowCutHz()));
    }
    drawSevenSegBox(
        g, fbDisplayRect, fbStr, isTypingFb,
        fbSel == nova::graviton::TC2290Delay::FeedbackSelect::Level
            ? "FB%"
            : (fbSel == nova::graviton::TC2290Delay::FeedbackSelect::High
                   ? "HI"
                   : "LO"));
    drawCompactLedList(g, fbSelLedRect, {"LVL", "HI", "LO"},
                       static_cast<int>(fbSel));

    // Preset / Spec. 7-segment display (80..99 for Bank A, 01..12 for Banks
    // B/C)
    juce::String presetStr;
    if (isTypingPreset && keypadBuffer.isNotEmpty()) {
      presetStr = keypadBuffer;
    } else {
      int pIdx = engine->getCurrentPresetIndex();
      int dispNum = (pIdx < 20) ? (80 + pIdx) : (pIdx - 19);
      presetStr = juce::String(dispNum).paddedLeft('0', 2);
    }
    drawSevenSegBox(g, presetDisplayRect, presetStr, isTypingPreset, "PROG");

    // Button state LEDs (tiny corner indicator inside button bounds)
    drawButtonStateHighlight(g, btnModPan, engine->isPanModEnabled(),
                             juce::Colour(0xFFFF8822));
    drawButtonStateHighlight(g, btnModDyn, engine->isDynModEnabled(),
                             juce::Colour(0xFFFF8822));
    drawButtonStateHighlight(g, btnReverse, engine->isDynReverse(),
                             juce::Colour(0xFFFF3A3A));
    drawButtonStateHighlight(g, btnDirect, engine->isDirectEnabled(),
                             juce::Colour(0xFF42E66B));
    drawButtonStateHighlight(g, btnDelayMod, engine->isDelayModEnabled(),
                             juce::Colour(0xFFFF8822));
    drawButtonStateHighlight(g, btnSync, engine->isTempoSync(),
                             juce::Colour(0xFF39C5FF));
    drawButtonStateHighlight(g, btnLearn, tapFlashFrames > 0,
                             juce::Colour(0xFFFF3A3A));
    drawButtonStateHighlight(g, btnFbInvert, engine->getFeedbackInvert(),
                             juce::Colour(0xFFFF3A3A));
    drawButtonStateHighlight(g, btnOnDelay, engine->isDelayEnabled(),
                             juce::Colour(0xFF42E66B));
    drawButtonStateHighlight(g, btnSpec, showSpecialPanel,
                             juce::Colour(0xFFFF8822));

    // Special Parameters Right Panel Readouts
    if (showSpecialPanel) {
      drawSevenSegBox(g, spInLevelDisplay,
                      (activeNumericTarget == NumericTarget::InputLevel &&
                       keypadBuffer.isNotEmpty())
                          ? keypadBuffer
                          : juce::String(engine->getInputLevelDb(), 1) + "dB",
                      activeNumericTarget == NumericTarget::InputLevel, "IN");

      drawSevenSegBox(g, spMixDisplay,
                      (activeNumericTarget == NumericTarget::MixPct &&
                       keypadBuffer.isNotEmpty())
                          ? keypadBuffer
                          : juce::String(juce::roundToInt(engine->getMix())) +
                                "%",
                      activeNumericTarget == NumericTarget::MixPct, "MIX");

      static const char *kMuteNames[] = {"NONE", "DELAY", "DIRECT", "BOTH"};
      int mIdx = juce::jlimit(0, 3, static_cast<int>(engine->getMuteMode()));
      drawSevenSegBox(g, spMuteDisplay, kMuteNames[mIdx], false, "MUTE");

      drawSevenSegBox(g, spOutLevelDisplay,
                      (activeNumericTarget == NumericTarget::OutputLevel &&
                       keypadBuffer.isNotEmpty())
                          ? keypadBuffer
                          : juce::String(engine->getOutputLevelDb(), 1) + "dB",
                      activeNumericTarget == NumericTarget::OutputLevel, "OUT");

      drawSevenSegBox(
          g, spOutPanDisplay,
          (activeNumericTarget == NumericTarget::OutputPan &&
           keypadBuffer.isNotEmpty())
              ? keypadBuffer
              : juce::String(juce::roundToInt(engine->getOutputPan())),
          activeNumericTarget == NumericTarget::OutputPan, "PAN");

      drawButtonStateHighlight(g, btnInvDelayOut, engine->isInvertDelayOutput(),
                               juce::Colour(0xFFFF3A3A));
      drawButtonStateHighlight(g, btnInvDlyMod, engine->isInvertDelayMod(),
                               juce::Colour(0xFFFF8822));
      drawButtonStateHighlight(g, btnDeepMod, engine->isDeepMod(),
                               juce::Colour(0xFFFF3A3A));

      static const char *kSubdivNames[] = {"1/1",  "1/2",  "1/4", "1/8",
                                           "1/16", "1/32", "1/64"};
      static const char *kModeNames[] = {"STRAIGHT", "DOTTED", "TRIPLET"};
      drawSevenSegBox(g, spSubdivDisplay,
                      kSubdivNames[juce::jlimit(
                          0, 6, static_cast<int>(engine->getSyncSubdiv()))],
                      false, "DIV");
      drawSevenSegBox(g, spSyncModeDisplay,
                      kModeNames[juce::jlimit(
                          0, 2, static_cast<int>(engine->getSyncMode()))],
                      false, "MODE");

      static const char *kThLabels[4] = {"DLY", "PAN", "VOL", "FB"};
      for (int i = 0; i < 4; ++i) {
        NumericTarget nt = static_cast<NumericTarget>(
            static_cast<int>(NumericTarget::ThreshDlyMod) + i);
        drawSevenSegBox(g, spThreshDisplays[i],
                        (activeNumericTarget == nt && keypadBuffer.isNotEmpty())
                            ? keypadBuffer
                            : juce::String(engine->getThresholdStep(i)),
                        activeNumericTarget == nt, kThLabels[i]);
      }

      // Preset Banner at bottom-right
      g.setColour(juce::Colour(0xFF0A0B0E));
      g.fillRoundedRectangle(spPresetBanner.toFloat(), 3.0f);
      g.setColour(juce::Colour(0xFF2B303A));
      g.drawRoundedRectangle(spPresetBanner.toFloat(), 3.0f, 1.0f);

      const auto &presets = engine->getFactoryPresets();
      int pIdx = juce::jlimit(0, static_cast<int>(presets.size()) - 1,
                              engine->getCurrentPresetIndex());
      int dispNum = (pIdx < 20) ? (80 + pIdx) : (pIdx - 19);
      g.setColour(juce::Colour(0xFF8C96A8));
      g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
      g.drawText(
          juce::String("Bank: ") + presets[static_cast<size_t>(pIdx)].category +
              "  |  Target: " + getNumericTargetName(),
          spPresetBanner.getX() + 6, spPresetBanner.getY() + 2,
          spPresetBanner.getWidth() - 12, 12, juce::Justification::centredLeft);
      g.setColour(juce::Colour(0xFFE8ECF2));
      g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
      g.drawText(juce::String(dispNum).paddedLeft('0', 2) + "  " +
                     presets[static_cast<size_t>(pIdx)].name,
                 spPresetBanner.getX() + 6, spPresetBanner.getY() + 14,
                 spPresetBanner.getWidth() - 12, 18,
                 juce::Justification::centredLeft);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    auto pos = e.getPosition();
    if (modDisplayRect.contains(pos)) {
      selectNumericTarget(modShowDepth ? NumericTarget::ModDepth
                                       : NumericTarget::ModSpeed);
    } else if (delayDisplayRect.contains(pos)) {
      selectNumericTarget(NumericTarget::DelayTime);
    } else if (fbDisplayRect.contains(pos) && engine != nullptr) {
      auto sel = engine->getFeedbackSelect();
      if (sel == nova::graviton::TC2290Delay::FeedbackSelect::Level)
        selectNumericTarget(NumericTarget::FbLevel);
      else if (sel == nova::graviton::TC2290Delay::FeedbackSelect::High)
        selectNumericTarget(NumericTarget::FbHigh);
      else
        selectNumericTarget(NumericTarget::FbLow);
    } else if (presetDisplayRect.contains(pos)) {
      selectNumericTarget(NumericTarget::PresetNum);
    } else if (showSpecialPanel) {
      if (spInLevelDisplay.contains(pos))
        selectNumericTarget(NumericTarget::InputLevel);
      else if (spMixDisplay.contains(pos))
        selectNumericTarget(NumericTarget::MixPct);
      else if (spOutLevelDisplay.contains(pos))
        selectNumericTarget(NumericTarget::OutputLevel);
      else if (spOutPanDisplay.contains(pos))
        selectNumericTarget(NumericTarget::OutputPan);
      else if (spThreshDisplays[0].contains(pos))
        selectNumericTarget(NumericTarget::ThreshDlyMod);
      else if (spThreshDisplays[1].contains(pos))
        selectNumericTarget(NumericTarget::ThreshPanMod);
      else if (spThreshDisplays[2].contains(pos))
        selectNumericTarget(NumericTarget::ThreshDynVol);
      else if (spThreshDisplays[3].contains(pos))
        selectNumericTarget(NumericTarget::ThreshDynFb);
    }
  }

  void resized() override {
    auto area = getLocalBounds().reduced(6);
    headerRect = area.removeFromTop(26);
    area.removeFromTop(4);

    // Header buttons
    btnPower.setBounds(headerRect.getX() + 4, headerRect.getY() + 2, 58, 22);
    btnEditorToggle.setBounds(headerRect.getRight() - 62, headerRect.getY() + 2,
                              58, 22);
    btnCompare.setBounds(headerRect.getRight() - 134, headerRect.getY() + 2, 68,
                         22);
    btnNextPreset.setBounds(headerRect.getRight() - 162, headerRect.getY() + 2,
                            24, 22);
    cmbPreset.setBounds(headerRect.getRight() - 346, headerRect.getY() + 2, 180,
                        22);
    btnPrevPreset.setBounds(headerRect.getRight() - 374, headerRect.getY() + 2,
                            24, 22);

    // Split Left (Classic Hardware) and Right (Special Parameters)
    juce::Rectangle<int> leftArea;
    juce::Rectangle<int> rightArea;
    if (showSpecialPanel) {
      int leftW = static_cast<int>(area.getWidth() * 0.52f);
      leftArea = area.removeFromLeft(leftW);
      area.removeFromLeft(6);
      rightArea = area;
    } else {
      leftArea = area;
    }

    // Meter Bridge (compact 34px)
    meterRect = leftArea.removeFromTop(34);
    leftArea.removeFromTop(4);

    auto meterInner = meterRect.reduced(8, 12);
    int halfMeter = (meterInner.getWidth() - 10) / 2;
    auto inArea = meterInner.removeFromLeft(halfMeter);
    meterInner.removeFromLeft(10);
    auto outArea = meterInner;
    meterInLRect = inArea.removeFromTop(8).withTrimmedLeft(28);
    inArea.removeFromTop(2);
    meterInRRect = inArea.removeFromTop(8).withTrimmedLeft(28);
    meterOutLRect = outArea.removeFromTop(8).withTrimmedLeft(32);
    outArea.removeFromTop(2);
    meterOutRRect = outArea.removeFromTop(8).withTrimmedLeft(32);

    // Two equal rows for the 6 hardware modules
    int rowH = (leftArea.getHeight() - 4) / 2;
    auto topRow = leftArea.removeFromTop(rowH);
    leftArea.removeFromTop(4);
    auto botRow = leftArea;

    // Row 1: MODULATION (39%) | PAN-DYN (24%) | DELAY (37%)
    int modW = static_cast<int>(topRow.getWidth() * 0.39f);
    int panW = static_cast<int>(topRow.getWidth() * 0.24f);
    modRect = topRow.removeFromLeft(modW);
    topRow.removeFromLeft(4);
    panDynRect = topRow.removeFromLeft(panW);
    topRow.removeFromLeft(4);
    delayRect = topRow;

    layoutModSection();
    layoutPanDynSection();
    layoutDelaySection();

    // Row 2: FEEDBACK (34%) | PRESET/SPEC (34%) | KEYBOARD (32%)
    int fbW = static_cast<int>(botRow.getWidth() * 0.34f);
    int psW = static_cast<int>(botRow.getWidth() * 0.34f);
    feedbackRect = botRow.removeFromLeft(fbW);
    botRow.removeFromLeft(4);
    presetSpecRect = botRow.removeFromLeft(psW);
    botRow.removeFromLeft(4);
    keypadRect = botRow;

    layoutFeedbackSection();
    layoutPresetSpecSection();
    layoutKeypadSection();

    if (showSpecialPanel)
      layoutSpecialPanel(rightArea);
    setSpecialComponentsVisible(showSpecialPanel);
  }

private:
  enum class NumericTarget {
    DelayTime = 0,
    ModSpeed,
    ModDepth,
    FbLevel,
    FbHigh,
    FbLow,
    PresetNum,
    InputLevel,
    MixPct,
    OutputLevel,
    OutputPan,
    ThreshDlyMod,
    ThreshPanMod,
    ThreshDynVol,
    ThreshDynFb
  };

  struct CompareSnapshot {
    bool enabled = true;
    float delayMs = 250.0f;
    bool directOn = true;
    bool delayOn = true;
    bool delayModOn = false;
    bool tempoSync = false;
    int syncSubdiv = 2;
    int syncMode = 0;
    float feedback = 0.35f;
    bool fbInvert = false;
    float highCut = 12000.0f;
    float lowCut = 60.0f;
    int modSelect = 0;
    float modSpeed[3] = {1.0f, 0.5f, 1.0f};
    float modDepth[3] = {20.0f, 30.0f, 25.0f};
    int modWave[3] = {0, 0, 2};
    bool panModOn = false;
    bool dynModOn = false;
    int panTarget = 0;
    bool dynReverse = false;
    float inLevelDb = 0.0f;
    float mix = 50.0f;
    int muteMode = 0;
    float outLevelDb = 0.0f;
    float outPan = 0.0f;
    bool invDelayOut = false;
    bool invDlyMod = false;
    bool deepMod = false;
    float thresh[4] = {-24.0f, -24.0f, -18.0f, -18.0f};
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

    if (tapFlashFrames > 0)
      --tapFlashFrames;

    repaint();
  }

  void setupControls() {
    auto styleHwBtn = [](juce::TextButton &b, const juce::String &txt,
                         juce::Colour bg = juce::Colour(0xFF232730)) {
      b.setButtonText(txt);
      b.setColour(juce::TextButton::buttonColourId, bg);
      b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF3A4150));
      b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFE5E9F0));
      b.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFFFFFFFF));
    };

    styleHwBtn(btnPower, "POWER", juce::Colour(0xFF2D1E20));
    btnPower.setClickingTogglesState(true);
    btnPower.onClick = [this] {
      if (engine)
        engine->setEnabled(btnPower.getToggleState());
      updatePowerColors();
    };
    addAndMakeVisible(btnPower);

    styleHwBtn(btnPrevPreset, "<");
    btnPrevPreset.onClick = [this] { stepPreset(-1); };
    addAndMakeVisible(btnPrevPreset);

    styleHwBtn(btnNextPreset, ">");
    btnNextPreset.onClick = [this] { stepPreset(1); };
    addAndMakeVisible(btnNextPreset);

    cmbPreset.setColour(juce::ComboBox::backgroundColourId,
                        juce::Colour(0xFF15181E));
    cmbPreset.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFE8ECF2));
    cmbPreset.setColour(juce::ComboBox::outlineColourId,
                        juce::Colour(0xFF383E4C));
    cmbPreset.onChange = [this] {
      if (suppressCallbacks || engine == nullptr)
        return;
      int id = cmbPreset.getSelectedId();
      if (id > 0) {
        engine->loadPreset(id - 1);
        compareActive = false;
        syncFromEngine();
      }
    };
    addAndMakeVisible(cmbPreset);

    styleHwBtn(btnCompare, "COMPARE");
    btnCompare.onClick = [this] { toggleCompare(); };
    addAndMakeVisible(btnCompare);

    styleHwBtn(btnEditorToggle, "SPEC.");
    btnEditorToggle.setClickingTogglesState(true);
    btnEditorToggle.setToggleState(true, juce::dontSendNotification);
    btnEditorToggle.onClick = [this] {
      showSpecialPanel = btnEditorToggle.getToggleState();
      resized();
      repaint();
    };
    addAndMakeVisible(btnEditorToggle);

    // Modulation buttons
    styleHwBtn(btnSpeedUp, "S+");
    styleHwBtn(btnSpeedDown, "S-");
    styleHwBtn(btnDepthUp, "D+");
    styleHwBtn(btnDepthDown, "D-");
    styleHwBtn(btnWaveform, "WAVE");
    styleHwBtn(btnModSelect, "SEL");
    btnSpeedUp.onClick = [this] { nudgeModSpeed(0.2f); };
    btnSpeedDown.onClick = [this] { nudgeModSpeed(-0.2f); };
    btnDepthUp.onClick = [this] { nudgeModDepth(5.0f); };
    btnDepthDown.onClick = [this] { nudgeModDepth(-5.0f); };
    btnWaveform.onClick = [this] {
      if (!engine)
        return;
      auto sel = engine->getModTargetSelect();
      int next = (static_cast<int>(engine->getTargetModWaveform(sel)) + 1) % 4;
      engine->setTargetModWaveform(
          sel, static_cast<nova::graviton::TC2290Delay::ModWaveform>(next));
      repaint();
    };
    btnModSelect.onClick = [this] {
      if (!engine)
        return;
      int next = (static_cast<int>(engine->getModTargetSelect()) + 1) % 3;
      engine->setModTargetSelect(
          static_cast<nova::graviton::TC2290Delay::ModTarget>(next));
      repaint();
    };
    for (auto *b : {&btnSpeedUp, &btnSpeedDown, &btnDepthUp, &btnDepthDown,
                    &btnWaveform, &btnModSelect})
      addAndMakeVisible(*b);

    // Pan - Dyn buttons
    styleHwBtn(btnModPan, "PAN");
    styleHwBtn(btnModDyn, "DYN");
    styleHwBtn(btnPanTarget, "TGT");
    styleHwBtn(btnReverse, "REV");
    btnModPan.onClick = [this] {
      if (engine)
        engine->setPanModEnabled(!engine->isPanModEnabled());
      repaint();
    };
    btnModDyn.onClick = [this] {
      if (engine)
        engine->setDynModEnabled(!engine->isDynModEnabled());
      repaint();
    };
    btnPanTarget.onClick = [this] {
      if (!engine)
        return;
      int next = (static_cast<int>(engine->getPanTarget()) + 1) % 4;
      engine->setPanTarget(
          static_cast<nova::graviton::TC2290Delay::PanTarget>(next));
      repaint();
    };
    btnReverse.onClick = [this] {
      if (engine)
        engine->setDynReverse(!engine->isDynReverse());
      repaint();
    };
    for (auto *b : {&btnModPan, &btnModDyn, &btnPanTarget, &btnReverse})
      addAndMakeVisible(*b);

    // Delay buttons
    styleHwBtn(btnDelayUp, "+");
    styleHwBtn(btnDelayDown, "-");
    styleHwBtn(btnDirect, "DIR");
    styleHwBtn(btnDelayMod, "MOD");
    styleHwBtn(btnSync, "SYNC");
    styleHwBtn(btnLearn, "TAP");
    btnDelayUp.onClick = [this] { nudgeDelay(10.0f); };
    btnDelayDown.onClick = [this] { nudgeDelay(-10.0f); };
    btnDirect.onClick = [this] {
      if (engine)
        engine->setDirectEnabled(!engine->isDirectEnabled());
      repaint();
    };
    btnDelayMod.onClick = [this] {
      if (engine)
        engine->setDelayModEnabled(!engine->isDelayModEnabled());
      repaint();
    };
    btnSync.onClick = [this] {
      if (engine)
        engine->setTempoSync(!engine->isTempoSync());
      repaint();
    };
    btnLearn.onClick = [this] { handleTapLearn(); };
    for (auto *b : {&btnDelayUp, &btnDelayDown, &btnDirect, &btnDelayMod,
                    &btnSync, &btnLearn})
      addAndMakeVisible(*b);

    // Feedback buttons
    styleHwBtn(btnFbUp, "+");
    styleHwBtn(btnFbDown, "-");
    styleHwBtn(btnFbInvert, "INV");
    styleHwBtn(btnFbSelect, "SEL");
    btnFbUp.onClick = [this] { nudgeFeedback(1.0f); };
    btnFbDown.onClick = [this] { nudgeFeedback(-1.0f); };
    btnFbInvert.onClick = [this] {
      if (engine)
        engine->setFeedbackInvert(!engine->getFeedbackInvert());
      repaint();
    };
    btnFbSelect.onClick = [this] {
      if (!engine)
        return;
      int next = (static_cast<int>(engine->getFeedbackSelect()) + 1) % 3;
      engine->setFeedbackSelect(
          static_cast<nova::graviton::TC2290Delay::FeedbackSelect>(next));
      repaint();
    };
    for (auto *b : {&btnFbUp, &btnFbDown, &btnFbInvert, &btnFbSelect})
      addAndMakeVisible(*b);

    // Preset / Spec. buttons
    styleHwBtn(btnPresetUp, "+");
    styleHwBtn(btnPresetDown, "-");
    styleHwBtn(btnOnDelay, "DLY");
    styleHwBtn(btnSpec, "SPEC");
    btnPresetUp.onClick = [this] { stepPreset(1); };
    btnPresetDown.onClick = [this] { stepPreset(-1); };
    btnOnDelay.onClick = [this] {
      if (engine)
        engine->setDelayEnabled(!engine->isDelayEnabled());
      repaint();
    };
    btnSpec.onClick = [this] {
      showSpecialPanel = !showSpecialPanel;
      btnEditorToggle.setToggleState(showSpecialPanel,
                                     juce::dontSendNotification);
      resized();
      repaint();
    };
    for (auto *b : {&btnPresetUp, &btnPresetDown, &btnOnDelay, &btnSpec})
      addAndMakeVisible(*b);

    // Numeric Keyboard (7-8-9, 4-5-6, 1-2-3, 0-.-ENT)
    const char *keyLabels[12] = {"7", "8", "9", "4", "5", "6",
                                 "1", "2", "3", "0", ".", "ENT"};
    for (int i = 0; i < 12; ++i) {
      styleHwBtn(btnKeypad[i], keyLabels[i],
                 i == 11 ? juce::Colour(0xFF2D3A2E) : juce::Colour(0xFF1E222A));
      btnKeypad[i].onClick = [this, i] { handleKeypadPress(i); };
      addAndMakeVisible(btnKeypad[i]);
    }

    // Special Panel Controls
    styleHwBtn(btnInLevelDown, "-");
    styleHwBtn(btnInLevelUp, "+");
    btnInLevelDown.onClick = [this] {
      if (engine)
        engine->setInputLevelDb(engine->getInputLevelDb() - 1.0);
      repaint();
    };
    btnInLevelUp.onClick = [this] {
      if (engine)
        engine->setInputLevelDb(engine->getInputLevelDb() + 1.0);
      repaint();
    };

    styleHwBtn(btnMixDown, "-");
    styleHwBtn(btnMixUp, "+");
    btnMixDown.onClick = [this] {
      if (engine)
        engine->setMix(engine->getMix() - 5.0);
      repaint();
    };
    btnMixUp.onClick = [this] {
      if (engine)
        engine->setMix(engine->getMix() + 5.0);
      repaint();
    };

    styleHwBtn(btnMuteMode, "MUTE");
    btnMuteMode.onClick = [this] {
      if (!engine)
        return;
      int next = (static_cast<int>(engine->getMuteMode()) + 1) % 4;
      engine->setMuteMode(
          static_cast<nova::graviton::TC2290Delay::MuteMode>(next));
      repaint();
    };

    styleHwBtn(btnOutLevelDown, "-");
    styleHwBtn(btnOutLevelUp, "+");
    btnOutLevelDown.onClick = [this] {
      if (engine)
        engine->setOutputLevelDb(engine->getOutputLevelDb() - 1.0);
      repaint();
    };
    btnOutLevelUp.onClick = [this] {
      if (engine)
        engine->setOutputLevelDb(engine->getOutputLevelDb() + 1.0);
      repaint();
    };

    styleHwBtn(btnOutPanDown, "L");
    styleHwBtn(btnOutPanUp, "R");
    btnOutPanDown.onClick = [this] {
      if (engine)
        engine->setOutputPan(engine->getOutputPan() - 5.0);
      repaint();
    };
    btnOutPanUp.onClick = [this] {
      if (engine)
        engine->setOutputPan(engine->getOutputPan() + 5.0);
      repaint();
    };

    styleHwBtn(btnInvDelayOut, "INV.DLY");
    btnInvDelayOut.onClick = [this] {
      if (engine)
        engine->setInvertDelayOutput(!engine->isInvertDelayOutput());
      repaint();
    };

    styleHwBtn(btnSubdivCycle, "DIV");
    btnSubdivCycle.onClick = [this] {
      if (!engine)
        return;
      int next = (static_cast<int>(engine->getSyncSubdiv()) + 1) % 7;
      engine->setSyncSubdiv(
          static_cast<nova::graviton::TC2290Delay::SyncSubdiv>(next));
      repaint();
    };

    styleHwBtn(btnSyncModeCycle, "MODE");
    btnSyncModeCycle.onClick = [this] {
      if (!engine)
        return;
      int next = (static_cast<int>(engine->getSyncMode()) + 1) % 3;
      engine->setSyncMode(
          static_cast<nova::graviton::TC2290Delay::SyncMode>(next));
      repaint();
    };

    styleHwBtn(btnInvDlyMod, "INV. DLY MOD");
    btnInvDlyMod.onClick = [this] {
      if (engine)
        engine->setInvertDelayMod(!engine->isInvertDelayMod());
      repaint();
    };

    styleHwBtn(btnDeepMod, "DEEP MOD");
    btnDeepMod.onClick = [this] {
      if (engine)
        engine->setDeepMod(!engine->isDeepMod());
      repaint();
    };

    for (int i = 0; i < 4; ++i) {
      styleHwBtn(btnThreshDown[i], "-");
      styleHwBtn(btnThreshUp[i], "+");
      btnThreshDown[i].onClick = [this, i] { nudgeThreshold(i, -1); };
      btnThreshUp[i].onClick = [this, i] { nudgeThreshold(i, 1); };
    }

    styleHwBtn(btnAssignFav, "ASSIGN");
    btnAssignFav.onClick = [this] {
      if (engine) {
        int idx = engine->getCurrentPresetIndex();
        engine->setFavorite(idx, !engine->isFavorite(idx));
      }
      updateFavoriteButton();
      repaint();
    };

    for (auto *b :
         {&btnInLevelDown, &btnInLevelUp, &btnMixDown, &btnMixUp, &btnMuteMode,
          &btnOutLevelDown, &btnOutLevelUp, &btnOutPanDown, &btnOutPanUp,
          &btnInvDelayOut, &btnSubdivCycle, &btnSyncModeCycle, &btnInvDlyMod,
          &btnDeepMod, &btnAssignFav}) {
      addAndMakeVisible(*b);
    }
    for (int i = 0; i < 4; ++i) {
      addAndMakeVisible(btnThreshDown[i]);
      addAndMakeVisible(btnThreshUp[i]);
    }
  }

  void setSpecialComponentsVisible(bool vis) {
    for (auto *b :
         {&btnInLevelDown, &btnInLevelUp, &btnMixDown, &btnMixUp, &btnMuteMode,
          &btnOutLevelDown, &btnOutLevelUp, &btnOutPanDown, &btnOutPanUp,
          &btnInvDelayOut, &btnSubdivCycle, &btnSyncModeCycle, &btnInvDlyMod,
          &btnDeepMod, &btnAssignFav}) {
      b->setVisible(vis);
    }
    for (int i = 0; i < 4; ++i) {
      btnThreshDown[i].setVisible(vis);
      btnThreshUp[i].setVisible(vis);
    }
  }

  // --- Zero-Overlap Proportional Layouts ---
  void layoutModSection() {
    auto r = modRect.reduced(6, 12);
    auto btnRow = r.removeFromBottom(20);
    r.removeFromBottom(3);

    int bw = (btnRow.getWidth() - 10) / 6;
    btnSpeedDown.setBounds(btnRow.getX(), btnRow.getY(), bw, 20);
    btnSpeedUp.setBounds(btnRow.getX() + (bw + 2), btnRow.getY(), bw, 20);
    btnDepthDown.setBounds(btnRow.getX() + (bw + 2) * 2, btnRow.getY(), bw, 20);
    btnDepthUp.setBounds(btnRow.getX() + (bw + 2) * 3, btnRow.getY(), bw, 20);
    btnWaveform.setBounds(btnRow.getX() + (bw + 2) * 4, btnRow.getY(), bw + 2,
                          20);
    btnModSelect.setBounds(
        btnRow.getX() + (bw + 2) * 5 + 2, btnRow.getY(),
        btnRow.getRight() - (btnRow.getX() + (bw + 2) * 5 + 2), 20);

    modDisplayRect = r.removeFromLeft(static_cast<int>(r.getWidth() * 0.44f));
    r.removeFromLeft(4);
    modWaveLedRect = r.removeFromLeft(r.getWidth() / 2);
    modSelLedRect = r;
  }

  void layoutPanDynSection() {
    auto r = panDynRect.reduced(6, 12);
    auto botRow = r.removeFromBottom(20);
    r.removeFromBottom(3);

    int bw = (botRow.getWidth() - 4) / 2;
    btnPanTarget.setBounds(botRow.getX(), botRow.getY(), bw, 20);
    btnReverse.setBounds(botRow.getX() + bw + 4, botRow.getY(), bw, 20);

    auto topArea = r;
    auto topBtns = topArea.removeFromTop(20);
    btnModPan.setBounds(topBtns.getX(), topBtns.getY(), bw, 20);
    btnModDyn.setBounds(topBtns.getX() + bw + 4, topBtns.getY(), bw, 20);
    topArea.removeFromTop(2);
    panTargetLedRect = topArea;
  }

  void layoutDelaySection() {
    auto r = delayRect.reduced(6, 12);
    auto btnRow = r.removeFromBottom(20);
    r.removeFromBottom(3);
    delayDisplayRect = r;

    int bw = (btnRow.getWidth() - 10) / 6;
    btnDelayDown.setBounds(btnRow.getX(), btnRow.getY(), bw, 20);
    btnDelayUp.setBounds(btnRow.getX() + (bw + 2), btnRow.getY(), bw, 20);
    btnDirect.setBounds(btnRow.getX() + (bw + 2) * 2, btnRow.getY(), bw, 20);
    btnDelayMod.setBounds(btnRow.getX() + (bw + 2) * 3, btnRow.getY(), bw, 20);
    btnSync.setBounds(btnRow.getX() + (bw + 2) * 4, btnRow.getY(), bw, 20);
    btnLearn.setBounds(btnRow.getX() + (bw + 2) * 5, btnRow.getY(),
                       btnRow.getRight() - (btnRow.getX() + (bw + 2) * 5), 20);
  }

  void layoutFeedbackSection() {
    auto r = feedbackRect.reduced(6, 12);
    auto btnRow = r.removeFromBottom(20);
    r.removeFromBottom(3);

    int bw = (btnRow.getWidth() - 6) / 4;
    btnFbDown.setBounds(btnRow.getX(), btnRow.getY(), bw, 20);
    btnFbUp.setBounds(btnRow.getX() + bw + 2, btnRow.getY(), bw, 20);
    btnFbInvert.setBounds(btnRow.getX() + (bw + 2) * 2, btnRow.getY(), bw, 20);
    btnFbSelect.setBounds(btnRow.getX() + (bw + 2) * 3, btnRow.getY(),
                          btnRow.getRight() - (btnRow.getX() + (bw + 2) * 3),
                          20);

    fbDisplayRect = r.removeFromLeft(static_cast<int>(r.getWidth() * 0.58f));
    r.removeFromLeft(4);
    fbSelLedRect = r;
  }

  void layoutPresetSpecSection() {
    auto r = presetSpecRect.reduced(6, 12);
    auto btnRow = r.removeFromBottom(20);
    r.removeFromBottom(3);
    presetDisplayRect = r;

    int bw = (btnRow.getWidth() - 6) / 4;
    btnPresetDown.setBounds(btnRow.getX(), btnRow.getY(), bw, 20);
    btnPresetUp.setBounds(btnRow.getX() + bw + 2, btnRow.getY(), bw, 20);
    btnOnDelay.setBounds(btnRow.getX() + (bw + 2) * 2, btnRow.getY(), bw, 20);
    btnSpec.setBounds(btnRow.getX() + (bw + 2) * 3, btnRow.getY(),
                      btnRow.getRight() - (btnRow.getX() + (bw + 2) * 3), 20);
  }

  void layoutKeypadSection() {
    auto r = keypadRect.reduced(6, 12);
    int cols = 3, rows = 4, gap = 2;
    int kw = (r.getWidth() - gap * (cols - 1)) / cols;
    int kh = juce::jmax(14, (r.getHeight() - gap * (rows - 1)) / rows);
    for (int idx = 0; idx < 12; ++idx) {
      int row = idx / 3;
      int col = idx % 3;
      btnKeypad[idx].setBounds(r.getX() + col * (kw + gap),
                               r.getY() + row * (kh + gap), kw, kh);
    }
  }

  void layoutSpecialPanel(juce::Rectangle<int> rightArea) {
    int footerH =
        juce::jmax(38, static_cast<int>(rightArea.getHeight() * 0.21f));
    spPresetInfoRect = rightArea.removeFromBottom(footerH);
    rightArea.removeFromBottom(4);

    int row1H = (rightArea.getHeight() - 4) / 2;
    auto row1 = rightArea.removeFromTop(row1H);
    rightArea.removeFromTop(4);
    auto row2 = rightArea;

    int w3 = (row1.getWidth() - 8) / 3;
    spInputRect = row1.removeFromLeft(w3);
    row1.removeFromLeft(4);
    spDelayOnRect = row1.removeFromLeft(w3);
    row1.removeFromLeft(4);
    spOutputRect = row1;

    // INPUT
    {
      auto r = spInputRect.reduced(6, 12);
      auto bRow = r.removeFromBottom(20);
      r.removeFromBottom(3);
      spInLevelDisplay = r;
      int bw = (bRow.getWidth() - 4) / 2;
      btnInLevelDown.setBounds(bRow.getX(), bRow.getY(), bw, 20);
      btnInLevelUp.setBounds(bRow.getX() + bw + 4, bRow.getY(), bw, 20);
    }

    // DELAY ON (MIX + MUTE)
    {
      auto r = spDelayOnRect.reduced(6, 12);
      int halfH = (r.getHeight() - 3) / 2;
      auto topR = r.removeFromTop(halfH);
      r.removeFromTop(3);
      auto botR = r;

      spMixDisplay = topR.withTrimmedRight(46);
      btnMixDown.setBounds(spMixDisplay.getRight() + 2, topR.getY(), 20,
                           topR.getHeight());
      btnMixUp.setBounds(spMixDisplay.getRight() + 24, topR.getY(), 20,
                         topR.getHeight());

      spMuteDisplay = botR.withTrimmedRight(46);
      btnMuteMode.setBounds(spMuteDisplay.getRight() + 2, botR.getY(), 44,
                            botR.getHeight());
    }

    // OUTPUT (LEVEL + PAN + INV.DLY)
    {
      auto r = spOutputRect.reduced(6, 12);
      int halfH = (r.getHeight() - 3) / 2;
      auto topR = r.removeFromTop(halfH);
      r.removeFromTop(3);
      auto botR = r;

      spOutLevelDisplay = topR.withTrimmedRight(46);
      btnOutLevelDown.setBounds(spOutLevelDisplay.getRight() + 2, topR.getY(),
                                20, topR.getHeight());
      btnOutLevelUp.setBounds(spOutLevelDisplay.getRight() + 24, topR.getY(),
                              20, topR.getHeight());

      spOutPanDisplay = botR.withTrimmedRight(92);
      btnOutPanDown.setBounds(spOutPanDisplay.getRight() + 2, botR.getY(), 20,
                              botR.getHeight());
      btnOutPanUp.setBounds(spOutPanDisplay.getRight() + 24, botR.getY(), 20,
                            botR.getHeight());
      btnInvDelayOut.setBounds(spOutPanDisplay.getRight() + 46, botR.getY(), 46,
                               botR.getHeight());
    }

    // Row 2: DAW SYNC | DELAY MOD | THRESHOLDS
    spSyncRect = row2.removeFromLeft(w3);
    row2.removeFromLeft(4);
    spDelayModRect = row2.removeFromLeft(w3);
    row2.removeFromLeft(4);
    spThreshRect = row2;

    {
      auto r = spSyncRect.reduced(6, 12);
      int halfH = (r.getHeight() - 3) / 2;
      auto topR = r.removeFromTop(halfH);
      r.removeFromTop(3);
      auto botR = r;

      spSubdivDisplay = topR.withTrimmedRight(44);
      btnSubdivCycle.setBounds(spSubdivDisplay.getRight() + 2, topR.getY(), 42,
                               topR.getHeight());

      spSyncModeDisplay = botR.withTrimmedRight(44);
      btnSyncModeCycle.setBounds(spSyncModeDisplay.getRight() + 2, botR.getY(),
                                 42, botR.getHeight());
    }

    {
      auto r = spDelayModRect.reduced(6, 12);
      int halfH = (r.getHeight() - 4) / 2;
      btnInvDlyMod.setBounds(r.getX(), r.getY(), r.getWidth(), halfH);
      btnDeepMod.setBounds(r.getX(), r.getY() + halfH + 4, r.getWidth(), halfH);
    }

    {
      auto r = spThreshRect.reduced(6, 11);
      int rowH = (r.getHeight() - 6) / 4;
      for (int i = 0; i < 4; ++i) {
        int y = r.getY() + i * (rowH + 2);
        spThreshDisplays[i] =
            juce::Rectangle<int>(r.getX(), y, r.getWidth() - 42, rowH);
        btnThreshDown[i].setBounds(spThreshDisplays[i].getRight() + 2, y, 19,
                                   rowH);
        btnThreshUp[i].setBounds(spThreshDisplays[i].getRight() + 23, y, 19,
                                 rowH);
      }
    }

    // Footer Preset Banner + ASSIGN
    {
      auto r = spPresetInfoRect.reduced(6, 10);
      btnAssignFav.setBounds(r.getRight() - 68, r.getY() + 2, 68,
                             r.getHeight() - 4);
      spPresetBanner = r.withTrimmedRight(74);
    }
  }

  void populatePresetCombo() {
    suppressCallbacks = true;
    cmbPreset.clear(juce::dontSendNotification);
    if (engine != nullptr) {
      const auto &presets = engine->getFactoryPresets();
      for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        const auto &p = presets[static_cast<size_t>(i)];
        cmbPreset.addItem(juce::String("[") + p.category + "] " + p.name,
                          i + 1);
      }
    } else {
      cmbPreset.addItem("80 Long Delay", 1);
    }
    suppressCallbacks = false;
  }

  void syncFromEngine() {
    if (engine == nullptr)
      return;
    suppressCallbacks = true;
    btnPower.setToggleState(engine->isEnabled(), juce::dontSendNotification);
    updatePowerColors();
    cmbPreset.setSelectedId(engine->getCurrentPresetIndex() + 1,
                            juce::dontSendNotification);
    updateFavoriteButton();
    suppressCallbacks = false;
  }

  void updatePowerColors() {
    bool on = btnPower.getToggleState();
    btnPower.setColour(juce::TextButton::buttonColourId,
                       on ? juce::Colour(0xFF9E2626)
                          : juce::Colour(0xFF232730));
  }

  void updateFavoriteButton() {
    if (engine == nullptr)
      return;
    bool fav = engine->isFavorite(engine->getCurrentPresetIndex());
    btnAssignFav.setButtonText(fav ? "FAV *" : "ASSIGN");
    btnAssignFav.setColour(juce::TextButton::buttonColourId,
                           fav ? juce::Colour(0xFF8C2D46)
                               : juce::Colour(0xFF232730));
  }

  void stepPreset(int delta) {
    if (engine == nullptr)
      return;
    int n = static_cast<int>(engine->getFactoryPresets().size());
    if (n <= 0)
      return;
    int next = (engine->getCurrentPresetIndex() + delta + n) % n;
    engine->loadPreset(next);
    compareActive = false;
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
      btnCompare.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xFF2E4057));
      btnCompare.setButtonText("STORED A");
      return;
    }
    if (!compareActive) {
      captureSnapshot(snapshotB);
      applySnapshot(snapshotA);
      compareActive = true;
      btnCompare.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xFFB56A1E));
      btnCompare.setButtonText("CMP [A]");
    } else {
      applySnapshot(snapshotB);
      compareActive = false;
      btnCompare.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xFF2E4057));
      btnCompare.setButtonText("CMP [B]");
    }
    syncFromEngine();
    repaint();
  }

  void captureSnapshot(CompareSnapshot &s) {
    s.enabled = engine->isEnabled();
    s.delayMs = static_cast<float>(engine->getRawDelayTimeMs());
    s.directOn = engine->isDirectEnabled();
    s.delayOn = engine->isDelayEnabled();
    s.delayModOn = engine->isDelayModEnabled();
    s.tempoSync = engine->isTempoSync();
    s.syncSubdiv = static_cast<int>(engine->getSyncSubdiv());
    s.syncMode = static_cast<int>(engine->getSyncMode());
    s.feedback = static_cast<float>(engine->getFeedback());
    s.fbInvert = engine->getFeedbackInvert();
    s.highCut = static_cast<float>(engine->getHighCutHz());
    s.lowCut = static_cast<float>(engine->getLowCutHz());
    s.modSelect = static_cast<int>(engine->getModTargetSelect());
    for (int i = 0; i < 3; ++i) {
      auto t = static_cast<nova::graviton::TC2290Delay::ModTarget>(i);
      s.modSpeed[i] = static_cast<float>(engine->getTargetModSpeedHz(t));
      s.modDepth[i] = static_cast<float>(engine->getTargetModDepthPct(t));
      s.modWave[i] = static_cast<int>(engine->getTargetModWaveform(t));
    }
    s.panModOn = engine->isPanModEnabled();
    s.dynModOn = engine->isDynModEnabled();
    s.panTarget = static_cast<int>(engine->getPanTarget());
    s.dynReverse = engine->isDynReverse();
    s.inLevelDb = static_cast<float>(engine->getInputLevelDb());
    s.mix = static_cast<float>(engine->getMix());
    s.muteMode = static_cast<int>(engine->getMuteMode());
    s.outLevelDb = static_cast<float>(engine->getOutputLevelDb());
    s.outPan = static_cast<float>(engine->getOutputPan());
    s.invDelayOut = engine->isInvertDelayOutput();
    s.invDlyMod = engine->isInvertDelayMod();
    s.deepMod = engine->isDeepMod();
    s.thresh[0] = static_cast<float>(engine->getThresholdDlyModDb());
    s.thresh[1] = static_cast<float>(engine->getThresholdPanModDb());
    s.thresh[2] = static_cast<float>(engine->getThresholdDynDlyVolDb());
    s.thresh[3] = static_cast<float>(engine->getThresholdDynFbDb());
  }

  void applySnapshot(const CompareSnapshot &s) {
    engine->setEnabled(s.enabled);
    engine->setDelayTimeMs(s.delayMs);
    engine->setDirectEnabled(s.directOn);
    engine->setDelayEnabled(s.delayOn);
    engine->setDelayModEnabled(s.delayModOn);
    engine->setTempoSync(s.tempoSync);
    engine->setSyncSubdiv(
        static_cast<nova::graviton::TC2290Delay::SyncSubdiv>(s.syncSubdiv));
    engine->setSyncMode(
        static_cast<nova::graviton::TC2290Delay::SyncMode>(s.syncMode));
    engine->setFeedback(s.feedback);
    engine->setFeedbackInvert(s.fbInvert);
    engine->setHighCutHz(s.highCut);
    engine->setLowCutHz(s.lowCut);
    engine->setModTargetSelect(
        static_cast<nova::graviton::TC2290Delay::ModTarget>(s.modSelect));
    for (int i = 0; i < 3; ++i) {
      auto t = static_cast<nova::graviton::TC2290Delay::ModTarget>(i);
      engine->setTargetModSpeedHz(t, s.modSpeed[i]);
      engine->setTargetModDepthPct(t, s.modDepth[i]);
      engine->setTargetModWaveform(
          t,
          static_cast<nova::graviton::TC2290Delay::ModWaveform>(s.modWave[i]));
    }
    engine->setPanModEnabled(s.panModOn);
    engine->setDynModEnabled(s.dynModOn);
    engine->setPanTarget(
        static_cast<nova::graviton::TC2290Delay::PanTarget>(s.panTarget));
    engine->setDynReverse(s.dynReverse);
    engine->setInputLevelDb(s.inLevelDb);
    engine->setMix(s.mix);
    engine->setMuteMode(
        static_cast<nova::graviton::TC2290Delay::MuteMode>(s.muteMode));
    engine->setOutputLevelDb(s.outLevelDb);
    engine->setOutputPan(s.outPan);
    engine->setInvertDelayOutput(s.invDelayOut);
    engine->setInvertDelayMod(s.invDlyMod);
    engine->setDeepMod(s.deepMod);
    engine->setThresholdDlyModDb(s.thresh[0]);
    engine->setThresholdPanModDb(s.thresh[1]);
    engine->setThresholdDynDlyVolDb(s.thresh[2]);
    engine->setThresholdDynFbDb(s.thresh[3]);
  }

  void nudgeModSpeed(float delta) {
    if (!engine)
      return;
    modShowDepth = false;
    auto sel = engine->getModTargetSelect();
    engine->setTargetModSpeedHz(sel, engine->getTargetModSpeedHz(sel) + delta);
    selectNumericTarget(NumericTarget::ModSpeed);
  }

  void nudgeModDepth(float delta) {
    if (!engine)
      return;
    modShowDepth = true;
    auto sel = engine->getModTargetSelect();
    engine->setTargetModDepthPct(sel,
                                 engine->getTargetModDepthPct(sel) + delta);
    selectNumericTarget(NumericTarget::ModDepth);
  }

  void nudgeDelay(float delta) {
    if (!engine)
      return;
    engine->setTempoSync(false);
    engine->setDelayTimeMs(engine->getRawDelayTimeMs() + delta);
    selectNumericTarget(NumericTarget::DelayTime);
  }

  void nudgeFeedback(float delta) {
    if (!engine)
      return;
    auto sel = engine->getFeedbackSelect();
    if (sel == nova::graviton::TC2290Delay::FeedbackSelect::Level) {
      engine->setFeedback(engine->getFeedback() + delta * 0.02);
      selectNumericTarget(NumericTarget::FbLevel);
    } else if (sel == nova::graviton::TC2290Delay::FeedbackSelect::High) {
      engine->setHighCutHz(engine->getHighCutHz() + delta * 250.0);
      selectNumericTarget(NumericTarget::FbHigh);
    } else {
      engine->setLowCutHz(engine->getLowCutHz() + delta * 15.0);
      selectNumericTarget(NumericTarget::FbLow);
    }
  }

  void nudgeThreshold(int idx, int stepDelta) {
    if (!engine)
      return;
    engine->setThresholdStep(idx, engine->getThresholdStep(idx) + stepDelta);
    if (idx == 0)
      selectNumericTarget(NumericTarget::ThreshDlyMod);
    else if (idx == 1)
      selectNumericTarget(NumericTarget::ThreshPanMod);
    else if (idx == 2)
      selectNumericTarget(NumericTarget::ThreshDynVol);
    else
      selectNumericTarget(NumericTarget::ThreshDynFb);
  }

  void handleTapLearn() {
    if (!engine)
      return;
    tapFlashFrames = 6;
    double nowMs = juce::Time::getMillisecondCounterHiRes();
    if (lastTapMs > 0.0 && (nowMs - lastTapMs) < 2500.0 &&
        (nowMs - lastTapMs) > 40.0) {
      double intervalMs = nowMs - lastTapMs;
      engine->setTempoSync(false);
      engine->setDelayTimeMs(juce::jlimit(0.0, 9999.0, intervalMs));
    }
    lastTapMs = nowMs;
    repaint();
  }

  void selectNumericTarget(NumericTarget target) {
    activeNumericTarget = target;
    keypadBuffer.clear();
    repaint();
  }

  void handleKeypadPress(int keyIdx) {
    if (engine == nullptr)
      return;
    if (keyIdx == 11) {
      if (keypadBuffer.isNotEmpty()) {
        float val = keypadBuffer.getFloatValue();
        applyNumericEntry(val);
        keypadBuffer.clear();
      }
      repaint();
      return;
    }
    static const char *kChars[11] = {"7", "8", "9", "4", "5", "6",
                                     "1", "2", "3", "0", "."};
    if (keypadBuffer.length() < 6) {
      if (keyIdx == 10 && keypadBuffer.containsChar('.'))
        return;
      keypadBuffer += kChars[keyIdx];
    } else {
      keypadBuffer = kChars[keyIdx];
    }
    repaint();
  }

  void applyNumericEntry(float val) {
    switch (activeNumericTarget) {
    case NumericTarget::DelayTime:
      engine->setTempoSync(false);
      engine->setDelayTimeMs(val);
      break;
    case NumericTarget::ModSpeed:
      engine->setTargetModSpeedHz(engine->getModTargetSelect(), val);
      break;
    case NumericTarget::ModDepth:
      engine->setTargetModDepthPct(engine->getModTargetSelect(), val);
      break;
    case NumericTarget::FbLevel:
      engine->setFeedback(val * 0.01f);
      break;
    case NumericTarget::FbHigh:
      engine->setHighCutHz(val);
      break;
    case NumericTarget::FbLow:
      engine->setLowCutHz(val);
      break;
    case NumericTarget::PresetNum: {
      int pNum = static_cast<int>(val);
      int idx = (pNum >= 80 && pNum <= 99) ? (pNum - 80) : (pNum - 1);
      engine->loadPreset(idx);
      syncFromEngine();
      break;
    }
    case NumericTarget::InputLevel:
      engine->setInputLevelDb(val);
      break;
    case NumericTarget::MixPct:
      engine->setMix(val);
      break;
    case NumericTarget::OutputLevel:
      engine->setOutputLevelDb(val);
      break;
    case NumericTarget::OutputPan:
      engine->setOutputPan(val);
      break;
    case NumericTarget::ThreshDlyMod:
      if (val >= 1.0f && val <= 9.0f)
        engine->setThresholdStep(0, static_cast<int>(val));
      else
        engine->setThresholdDlyModDb(val > 0.0f ? -val : val);
      break;
    case NumericTarget::ThreshPanMod:
      if (val >= 1.0f && val <= 9.0f)
        engine->setThresholdStep(1, static_cast<int>(val));
      else
        engine->setThresholdPanModDb(val > 0.0f ? -val : val);
      break;
    case NumericTarget::ThreshDynVol:
      if (val >= 1.0f && val <= 9.0f)
        engine->setThresholdStep(2, static_cast<int>(val));
      else
        engine->setThresholdDynDlyVolDb(val > 0.0f ? -val : val);
      break;
    case NumericTarget::ThreshDynFb:
      if (val >= 1.0f && val <= 9.0f)
        engine->setThresholdStep(3, static_cast<int>(val));
      else
        engine->setThresholdDynFbDb(val > 0.0f ? -val : val);
      break;
    }
  }

  juce::String getNumericTargetName() const {
    switch (activeNumericTarget) {
    case NumericTarget::DelayTime:
      return "DELAY ms";
    case NumericTarget::ModSpeed:
      return "MOD SPEED";
    case NumericTarget::ModDepth:
      return "MOD DEPTH";
    case NumericTarget::FbLevel:
      return "FB LEVEL";
    case NumericTarget::FbHigh:
      return "FB HIGH Hz";
    case NumericTarget::FbLow:
      return "FB LOW Hz";
    case NumericTarget::PresetNum:
      return "PRESET #";
    case NumericTarget::InputLevel:
      return "IN LEVEL";
    case NumericTarget::MixPct:
      return "MIX %";
    case NumericTarget::OutputLevel:
      return "OUT LEVEL";
    case NumericTarget::OutputPan:
      return "OUT PAN";
    case NumericTarget::ThreshDlyMod:
      return "TH DLY MOD";
    case NumericTarget::ThreshPanMod:
      return "TH PAN MOD";
    case NumericTarget::ThreshDynVol:
      return "TH DYN VOL";
    case NumericTarget::ThreshDynFb:
      return "TH DYN FB";
    }
    return "DELAY ms";
  }

  // --- Drawing Helpers ---
  void drawRackScrew(juce::Graphics &g, float cx, float cy) {
    g.setColour(juce::Colour(0xFF3A404D));
    g.fillEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
    g.setColour(juce::Colour(0xFF15181F));
    g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
  }

  void drawSectionFrame(juce::Graphics &g, juce::Rectangle<int> r,
                        const juce::String &title) {
    if (r.isEmpty())
      return;
    g.setColour(juce::Colour(0xFF353B47));
    g.drawRoundedRectangle(r.toFloat(), 3.0f, 1.0f);
    g.setColour(juce::Colour(0xFF14161A));
    int tw = juce::jmin(r.getWidth() - 12,
                        static_cast<int>(title.length() * 5.8f + 10));
    int tx = r.getCentreX() - tw / 2;
    g.fillRect(tx, r.getY() - 4, tw, 9);
    g.setColour(juce::Colour(0xFFD0D6E0));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(title, tx, r.getY() - 5, tw, 11, juce::Justification::centred);
  }

  void drawSevenSegBox(juce::Graphics &g, juce::Rectangle<int> r,
                       const juce::String &text, bool selected,
                       const juce::String &subLabel) {
    if (r.isEmpty())
      return;
    g.setColour(juce::Colour(0xFF07080A));
    g.fillRoundedRectangle(r.toFloat(), 2.5f);
    g.setColour(selected ? juce::Colour(0xFFFF6633) : juce::Colour(0xFF2C313B));
    g.drawRoundedRectangle(r.toFloat(), 2.5f, selected ? 1.5f : 1.0f);

    if (subLabel.isNotEmpty() && r.getWidth() > 42) {
      g.setColour(juce::Colour(0xFF707A8A));
      g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
      g.drawText(subLabel, r.getX() + 3, r.getY() + 1, r.getWidth() - 6,
                 r.getHeight() - 2, juce::Justification::centredLeft);
    }

    g.setColour(juce::Colour(0xFFFF2E2E));
    float fontSize =
        juce::jlimit(10.5f, 19.0f, static_cast<float>(r.getHeight()) * 0.72f);
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                fontSize, juce::Font::bold));
    g.drawText(text, r.reduced(3, 0), juce::Justification::centredRight);
  }

  void drawStatusDot(juce::Graphics &g, int cx, int cy, bool on,
                     juce::Colour col) {
    g.setColour(on ? col : col.withAlpha(0.18f));
    g.fillEllipse(static_cast<float>(cx - 3), static_cast<float>(cy - 3), 6.0f,
                  6.0f);
  }

  void drawCompactLedList(juce::Graphics &g, juce::Rectangle<int> r,
                          std::initializer_list<const char *> items,
                          int activeIdx) {
    int count = static_cast<int>(items.size());
    if (count <= 0 || r.isEmpty())
      return;
    int rowH = juce::jmax(8, r.getHeight() / count);
    int idx = 0;
    for (const char *txt : items) {
      bool on = (idx == activeIdx);
      int ry = r.getY() + idx * rowH;
      g.setColour(on ? juce::Colour(0xFFE8ECF2) : juce::Colour(0xFF7E8796));
      g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
      g.drawText(txt, r.getX(), ry, r.getWidth() - 10, rowH,
                 juce::Justification::centredRight);
      drawStatusDot(g, r.getRight() - 5, ry + rowH / 2, on,
                    juce::Colour(0xFFFF3A3A));
      ++idx;
    }
  }

  void drawButtonStateHighlight(juce::Graphics &g, const juce::TextButton &btn,
                                bool on, juce::Colour col) {
    if (!btn.isVisible())
      return;
    auto b = btn.getBounds();
    g.setColour(on ? col : col.withAlpha(0.15f));
    g.fillRect(b.getX() + 3, b.getY() + 2, b.getWidth() - 6, 2);
  }

  void drawMeterBar(juce::Graphics &g, juce::Rectangle<int> r, const char *ch,
                    float linLevel) {
    g.setColour(juce::Colour(0xFF9EA6B5));
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText(ch, r.getX() - 32, r.getY() - 1, 30, r.getHeight() + 2,
               juce::Justification::centredRight);

    constexpr int kSegs = 12;
    float db = (linLevel > 1e-6f) ? 20.0f * std::log10(linLevel) : -60.0f;
    float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    int litSegs = static_cast<int>(std::round(norm * kSegs));
    int gap = 2;
    float segW = static_cast<float>(r.getWidth() - (kSegs - 1) * gap) / kSegs;

    for (int i = 0; i < kSegs; ++i) {
      juce::Colour c = (i >= 10) ? juce::Colour(0xFFFF3333)
                                 : (i >= 8 ? juce::Colour(0xFFFFB822)
                                           : juce::Colour(0xFF42E65B));
      g.setColour(i < litSegs ? c : juce::Colour(0xFF1D222B));
      g.fillRect(r.getX() + static_cast<float>(i) * (segW + gap),
                 static_cast<float>(r.getY()), segW,
                 static_cast<float>(r.getHeight()));
    }
  }

  nova::graviton::TC2290Delay *engine = nullptr;
  bool showSpecialPanel = true;
  bool suppressCallbacks = false;
  bool modShowDepth = false;

  NumericTarget activeNumericTarget = NumericTarget::DelayTime;
  juce::String keypadBuffer;

  bool hasCompareSnapshot = false;
  bool compareActive = false;
  CompareSnapshot snapshotA, snapshotB;

  float meterInL = 0.0f, meterInR = 0.0f;
  float meterOutL = 0.0f, meterOutR = 0.0f;
  double lastTapMs = 0.0;
  int tapFlashFrames = 0;

  // Layout Rects
  juce::Rectangle<int> headerRect, meterRect, modRect, panDynRect, delayRect,
      feedbackRect, presetSpecRect, keypadRect;
  juce::Rectangle<int> meterInLRect, meterInRRect, meterOutLRect, meterOutRRect;
  juce::Rectangle<int> modDisplayRect, modWaveLedRect, modSelLedRect;
  juce::Rectangle<int> panTargetLedRect, delayDisplayRect, fbDisplayRect,
      fbSelLedRect, presetDisplayRect;

  juce::Rectangle<int> spInputRect, spDelayOnRect, spOutputRect, spSyncRect,
      spDelayModRect, spThreshRect, spPresetInfoRect, spPresetBanner;
  juce::Rectangle<int> spInLevelDisplay, spMixDisplay, spMuteDisplay,
      spOutLevelDisplay, spOutPanDisplay, spSubdivDisplay, spSyncModeDisplay;
  juce::Rectangle<int> spThreshDisplays[4];

  // Controls
  juce::TextButton btnPower, btnPrevPreset, btnNextPreset, btnCompare,
      btnEditorToggle;
  juce::ComboBox cmbPreset;

  juce::TextButton btnSpeedUp, btnSpeedDown, btnDepthUp, btnDepthDown,
      btnWaveform, btnModSelect;
  juce::TextButton btnModPan, btnModDyn, btnPanTarget, btnReverse;
  juce::TextButton btnDelayUp, btnDelayDown, btnDirect, btnDelayMod, btnSync,
      btnLearn;
  juce::TextButton btnFbUp, btnFbDown, btnFbInvert, btnFbSelect;
  juce::TextButton btnPresetUp, btnPresetDown, btnOnDelay, btnSpec;
  juce::TextButton btnKeypad[12];

  juce::TextButton btnInLevelDown, btnInLevelUp, btnMixDown, btnMixUp,
      btnMuteMode;
  juce::TextButton btnOutLevelDown, btnOutLevelUp, btnOutPanDown, btnOutPanUp,
      btnInvDelayOut;
  juce::TextButton btnSubdivCycle, btnSyncModeCycle, btnInvDlyMod, btnDeepMod;
  juce::TextButton btnThreshDown[4], btnThreshUp[4];
  juce::TextButton btnAssignFav;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TC2290Panel)
};

} // namespace nova
