#pragma once

#include "Effects/GravitonDistortion.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace nova {

// ==============================================================================
// DistortionPanel — Kiive Audio Tape Face Inspired 4-Part Studio Chassis
//   1. Header / Preset Management (<, Preset, >, Save, A/B, Settings VU Source)
//   2. Tube Saturation (Power, 5 Tube Types, Drive, Bias, Mix + Glowing Tube)
//   3. Blend (0% Tube <-> 100% Tape)
//   4. Tape Saturation (Power, 4 Tape Types, Drive, Flutter, Wow, Mix + Reels)
//   5. Tone (Power, Low, High) | Character (Warm / Vintage / Aggressive) |
//      Output (Power, Trim, Soft Clip) | Analog VU Meter (-20..+3 dB)
// ==============================================================================
class DistortionPanel final : public juce::Component, private juce::Timer {
public:
  explicit DistortionPanel(nova::graviton::DistortionEngine &eng)
      : engine(&eng) {
    setOpaque(true);

    // --- 1. Header & Presets ---
    btnPrevPreset.setButtonText("<");
    btnNextPreset.setButtonText(">");
    btnSave.setButtonText("SAVE");
    btnAB.setButtonText("A / B: A");
    btnSettings.setButtonText("VU: OUT");

    for (int i = 0; i < nova::graviton::DistortionEngine::kNumPresets; ++i)
      cbPreset.addItem(nova::graviton::DistortionEngine::getPresetName(i),
                       i + 1);
    cbPreset.setColour(juce::ComboBox::backgroundColourId,
                       juce::Colour(0xff1c1917));
    cbPreset.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff59e0b));
    cbPreset.setColour(juce::ComboBox::outlineColourId,
                       juce::Colour(0xff44403c));
    cbPreset.onChange = [this] {
      const int idx = cbPreset.getSelectedId() - 1;
      if (engine && idx >= 0) {
        engine->applyPreset(idx);
        syncFromEngine();
      }
    };
    btnPrevPreset.onClick = [this] {
      if (!engine)
        return;
      const int n = nova::graviton::DistortionEngine::kNumPresets;
      engine->applyPreset((engine->getCurrentPresetIndex() - 1 + n) % n);
      syncFromEngine();
    };
    btnNextPreset.onClick = [this] {
      if (!engine)
        return;
      const int n = nova::graviton::DistortionEngine::kNumPresets;
      engine->applyPreset((engine->getCurrentPresetIndex() + 1) % n);
      syncFromEngine();
    };
    btnSave.onClick = [this] {
      if (!engine)
        return;
      engine->saveUserPreset();
      btnSave.setButtonText("SAVED");
    };
    btnAB.onClick = [this] {
      if (!engine)
        return;
      engine->selectSlotB(!engine->isSlotB());
      syncFromEngine();
    };
    btnSettings.onClick = [this] {
      if (!engine)
        return;
      engine->setVuSourceInput(!engine->getVuSourceInput());
      updateButtons();
    };

    addAndMakeVisible(btnPrevPreset);
    addAndMakeVisible(cbPreset);
    addAndMakeVisible(btnNextPreset);
    addAndMakeVisible(btnSave);
    addAndMakeVisible(btnAB);
    addAndMakeVisible(btnSettings);

    // --- 2. Tube Saturation ---
    btnTubePower.setButtonText("PWR");
    btnTubePower.onClick = [this] {
      if (engine)
        engine->setTubeEnabled(!engine->getTubeEnabled());
      updateButtons();
      repaint();
    };
    addAndMakeVisible(btnTubePower);

    cbTubeType.addItem("12AX7", 1);
    cbTubeType.addItem("12AU7", 2);
    cbTubeType.addItem("12AT7", 3);
    cbTubeType.addItem("6L6", 4);
    cbTubeType.addItem("EL34", 5);
    cbTubeType.setColour(juce::ComboBox::backgroundColourId,
                         juce::Colour(0xff1c1917));
    cbTubeType.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff59e0b));
    cbTubeType.onChange = [this] {
      if (engine)
        engine->setTubeType(cbTubeType.getSelectedId() - 1);
    };
    addAndMakeVisible(cbTubeType);

    auto mkKnob = [this](std::unique_ptr<NovaRotaryKnob> &k, const char *name,
                         const char *unit, float lo, float hi, float def,
                         bool bipolar, juce::Colour c, int dec) {
      k = std::make_unique<NovaRotaryKnob>(name, unit, lo, hi, def, bipolar, c);
      k->setDisplayDecimals(dec);
      addAndMakeVisible(*k);
    };

    mkKnob(knobTubeDrive, "DRIVE", "dB", 0.0f, 24.0f, 5.0f, false,
           juce::Colour(0xfff59e0b), 1);
    mkKnob(knobTubeBias, "BIAS", "%", -100.0f, 100.0f, 15.0f, true,
           juce::Colour(0xffea580c), 0);
    mkKnob(knobTubeMix, "MIX", "%", 0.0f, 100.0f, 80.0f, false,
           juce::Colour(0xfffbbf24), 0);
    knobTubeDrive->onValueChanged = [this](float v) {
      if (engine)
        engine->setTubeDriveDb(v);
    };
    knobTubeBias->onValueChanged = [this](float v) {
      if (engine)
        engine->setTubeBiasPct(v);
    };
    knobTubeMix->onValueChanged = [this](float v) {
      if (engine)
        engine->setTubeMixPct(v);
    };

    // --- 3. Blend ---
    mkKnob(knobBlend, "BLEND", "%", 0.0f, 100.0f, 50.0f, false,
           juce::Colour(0xff38bdf8), 0);
    knobBlend->onValueChanged = [this](float v) {
      if (engine)
        engine->setBlendPct(v);
    };

    // --- 4. Tape Saturation ---
    btnTapePower.setButtonText("PWR");
    btnTapePower.onClick = [this] {
      if (engine)
        engine->setTapeEnabled(!engine->getTapeEnabled());
      updateButtons();
      repaint();
    };
    addAndMakeVisible(btnTapePower);

    cbTapeType.addItem("1/4\" 15 IPS", 1);
    cbTapeType.addItem("1/2\" 30 IPS", 2);
    cbTapeType.addItem("2\" 30 IPS", 3);
    cbTapeType.addItem("Vintage Tape", 4);
    cbTapeType.setColour(juce::ComboBox::backgroundColourId,
                         juce::Colour(0xff1c1917));
    cbTapeType.setColour(juce::ComboBox::textColourId, juce::Colour(0xff38bdf8));
    cbTapeType.onChange = [this] {
      if (engine)
        engine->setTapeType(cbTapeType.getSelectedId() - 1);
    };
    addAndMakeVisible(cbTapeType);

    mkKnob(knobTapeDrive, "DRIVE", "dB", 0.0f, 24.0f, 8.0f, false,
           juce::Colour(0xff38bdf8), 1);
    mkKnob(knobTapeFlutter, "FLUTTER", "%", 0.0f, 100.0f, 10.0f, false,
           juce::Colour(0xff22d3ee), 0);
    mkKnob(knobTapeWow, "WOW", "%", 0.0f, 100.0f, 10.0f, false,
           juce::Colour(0xff0ea5e9), 0);
    mkKnob(knobTapeMix, "MIX", "%", 0.0f, 100.0f, 100.0f, false,
           juce::Colour(0xff818cf8), 0);
    knobTapeDrive->onValueChanged = [this](float v) {
      if (engine)
        engine->setTapeDriveDb(v);
    };
    knobTapeFlutter->onValueChanged = [this](float v) {
      if (engine)
        engine->setFlutterPct(v);
    };
    knobTapeWow->onValueChanged = [this](float v) {
      if (engine)
        engine->setWowPct(v);
    };
    knobTapeMix->onValueChanged = [this](float v) {
      if (engine)
        engine->setTapeMixPct(v);
    };

    // --- 5. Tone ---
    btnTonePower.setButtonText("PWR");
    btnTonePower.onClick = [this] {
      if (engine)
        engine->setToneEnabled(!engine->getToneEnabled());
      updateButtons();
      repaint();
    };
    addAndMakeVisible(btnTonePower);

    mkKnob(knobToneLow, "LOW", "dB", -12.0f, 12.0f, 1.5f, true,
           juce::Colour(0xff34d399), 1);
    mkKnob(knobToneHigh, "HIGH", "dB", -12.0f, 12.0f, -0.5f, true,
           juce::Colour(0xff34d399), 1);
    knobToneLow->onValueChanged = [this](float v) {
      if (engine)
        engine->setToneLowDb(v);
    };
    knobToneHigh->onValueChanged = [this](float v) {
      if (engine)
        engine->setToneHighDb(v);
    };

    // --- 6. Character ---
    static constexpr const char *kCharNames[3] = {"WARM", "VINTAGE",
                                                  "AGGRESSIVE"};
    for (int i = 0; i < 3; ++i) {
      btnChar[static_cast<size_t>(i)].setButtonText(kCharNames[i]);
      btnChar[static_cast<size_t>(i)].onClick = [this, i] {
        if (engine)
          engine->setCharacterMode(i);
        updateButtons();
      };
      addAndMakeVisible(btnChar[static_cast<size_t>(i)]);
    }

    // --- 7. Output ---
    btnOutPower.setButtonText("PWR");
    btnOutPower.onClick = [this] {
      if (engine)
        engine->setOutputEnabled(!engine->getOutputEnabled());
      updateButtons();
      repaint();
    };
    addAndMakeVisible(btnOutPower);

    mkKnob(knobOutTrim, "TRIM", "dB", -24.0f, 12.0f, 0.0f, true,
           juce::Colour(0xfffacc15), 1);
    knobOutTrim->onValueChanged = [this](float v) {
      if (engine)
        engine->setOutputTrimDb(v);
    };

    btnSoftClip.setButtonText("SOFT CLIP");
    btnSoftClip.onClick = [this] {
      if (engine)
        engine->setSoftClip(!engine->getSoftClip());
      updateButtons();
    };
    addAndMakeVisible(btnSoftClip);

    syncFromEngine();
    startTimerHz(30);
  }

  ~DistortionPanel() override { stopTimer(); }

  void setEngine(nova::graviton::DistortionEngine &eng) {
    engine = &eng;
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    if (!engine)
      return;
    cbPreset.setSelectedId(engine->getCurrentPresetIndex() + 1,
                           juce::dontSendNotification);
    cbTubeType.setSelectedId(engine->getTubeType() + 1,
                             juce::dontSendNotification);
    knobTubeDrive->setValue(static_cast<float>(engine->getTubeDriveDb()),
                            juce::dontSendNotification);
    knobTubeBias->setValue(static_cast<float>(engine->getTubeBiasPct()),
                           juce::dontSendNotification);
    knobTubeMix->setValue(static_cast<float>(engine->getTubeMixPct()),
                          juce::dontSendNotification);

    knobBlend->setValue(static_cast<float>(engine->getBlendPct()),
                        juce::dontSendNotification);

    cbTapeType.setSelectedId(engine->getTapeType() + 1,
                             juce::dontSendNotification);
    knobTapeDrive->setValue(static_cast<float>(engine->getTapeDriveDb()),
                            juce::dontSendNotification);
    knobTapeFlutter->setValue(static_cast<float>(engine->getFlutterPct()),
                              juce::dontSendNotification);
    knobTapeWow->setValue(static_cast<float>(engine->getWowPct()),
                          juce::dontSendNotification);
    knobTapeMix->setValue(static_cast<float>(engine->getTapeMixPct()),
                          juce::dontSendNotification);

    knobToneLow->setValue(static_cast<float>(engine->getToneLowDb()),
                          juce::dontSendNotification);
    knobToneHigh->setValue(static_cast<float>(engine->getToneHighDb()),
                           juce::dontSendNotification);
    knobOutTrim->setValue(static_cast<float>(engine->getOutputTrimDb()),
                          juce::dontSendNotification);

    updateButtons();
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff120c08));

    // Walnut wood side cheeks
    const float woodW = 12.0f;
    juce::ColourGradient woodL(juce::Colour(0xff5a2e16), 0.0f, 0.0f,
                               juce::Colour(0xff381b0c), woodW, 0.0f, false);
    g.setGradientFill(woodL);
    g.fillRect(b.withWidth(woodW));
    g.fillRect(b.withLeft(b.getRight() - woodW));

    // Brushed dark-bronze studio faceplate
    auto plate = b.reduced(woodW, 4.0f);
    juce::ColourGradient chassis(juce::Colour(0xff29231e), 0.0f, 0.0f,
                                 juce::Colour(0xff181512), 0.0f, b.getBottom(),
                                 false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(plate, 6.0f);
    g.setColour(juce::Colour(0xff4a3d33));
    g.drawRoundedRectangle(plate, 6.0f, 1.2f);

    // Header bar
    g.setColour(juce::Colour(0xff121316));
    g.fillRoundedRectangle(headerRect, 5.0f);
    g.setColour(juce::Colour(0xfff59e0b));
    g.setFont(juce::Font(juce::FontOptions(14.5f, juce::Font::bold)));
    g.drawText("Distortion", headerRect.reduced(10.0f, 2.0f).withHeight(18.0f),
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff9ca3af));
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("Warm Tube & Tape Saturation",
               headerRect.reduced(10.0f, 2.0f).withTop(headerRect.getY() + 18.0f),
               juce::Justification::centredLeft);

    // Draw section frames
    drawPanelBox(g, tubeBox, "TUBE SATURATION", juce::Colour(0xfff59e0b));
    drawPanelBox(g, blendBox, "TUBE / TAPE BLEND", juce::Colour(0xff38bdf8));
    drawPanelBox(g, tapeBox, "TAPE SATURATION", juce::Colour(0xff38bdf8));
    drawPanelBox(g, toneBox, "TONE", juce::Colour(0xff34d399));
    drawPanelBox(g, charBox, "CHARACTER", juce::Colour(0xfff59e0b));
    drawPanelBox(g, outBox, "OUTPUT", juce::Colour(0xfffacc15));
    drawPanelBox(g, vuBox, "ANALOG VU", juce::Colour(0xffe0b27d));

    // Glowing vacuum tube visual inside Tube box
    drawTubeGlow(g, tubeGlowRect);

    // Spinning tape reels visual inside Tape box
    drawTapeReels(g, tapeReelsRect);

    // Blend side labels
    g.setColour(juce::Colour(0xff9ca3af));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("TUBE 0%", blendBox.reduced(8.0f).withTop(blendBox.getBottom() - 22.0f).toNearestInt(),
               juce::Justification::centredLeft);
    g.drawText("100% TAPE", blendBox.reduced(8.0f).withTop(blendBox.getBottom() - 22.0f).toNearestInt(),
               juce::Justification::centredRight);

    // Analog VU Meter
    drawVuMeter(g, vuMeterRect);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(12);
    headerRect = b.removeFromTop(38).toFloat();
    auto hIn = headerRect.reduced(8.0f, 5.0f).toNearestInt();
    hIn.removeFromLeft(195); // Title space
    btnSettings.setBounds(hIn.removeFromRight(72).reduced(2));
    btnAB.setBounds(hIn.removeFromRight(74).reduced(2));
    btnSave.setBounds(hIn.removeFromRight(62).reduced(2));
    auto presRow = hIn.reduced(4, 0);
    btnPrevPreset.setBounds(presRow.removeFromLeft(26).reduced(1));
    btnNextPreset.setBounds(presRow.removeFromRight(26).reduced(1));
    cbPreset.setBounds(presRow.reduced(2, 1));

    b.removeFromTop(8);

    // Top row: Tube (38%) | Blend (18%) | Tape (44%)
    auto topRow = b.removeFromTop(b.getHeight() * 54 / 100);
    const int blendW = juce::jlimit(120, 175, topRow.getWidth() * 18 / 100);
    const int tubeW = (topRow.getWidth() - blendW) * 46 / 100;

    tubeBox = topRow.removeFromLeft(tubeW).reduced(3).toFloat();
    blendBox = topRow.removeFromLeft(blendW).reduced(3).toFloat();
    tapeBox = topRow.reduced(3).toFloat();

    // Tube internals
    auto tIn = tubeBox.reduced(10.0f, 24.0f).toNearestInt();
    auto tTop = tIn.removeFromTop(24);
    btnTubePower.setBounds(tTop.removeFromLeft(46).reduced(1));
    cbTubeType.setBounds(tTop.removeFromLeft(110).reduced(2, 0));
    tubeGlowRect = tTop.toFloat();
    tIn.removeFromTop(6);
    const int tkw = tIn.getWidth() / 3;
    knobTubeDrive->setBounds(tIn.removeFromLeft(tkw).reduced(2));
    knobTubeBias->setBounds(tIn.removeFromLeft(tkw).reduced(2));
    knobTubeMix->setBounds(tIn.reduced(2));

    // Blend internals
    auto bIn = blendBox.reduced(10.0f, 28.0f).toNearestInt();
    bIn.removeFromBottom(14);
    knobBlend->setBounds(bIn.withSizeKeepingCentre(
        juce::jmin(115, bIn.getWidth()), juce::jmin(115, bIn.getHeight())));

    // Tape internals
    auto tpIn = tapeBox.reduced(10.0f, 24.0f).toNearestInt();
    auto tpTop = tpIn.removeFromTop(24);
    btnTapePower.setBounds(tpTop.removeFromLeft(46).reduced(1));
    cbTapeType.setBounds(tpTop.removeFromLeft(126).reduced(2, 0));
    tapeReelsRect = tpTop.toFloat();
    tpIn.removeFromTop(6);
    const int tpkw = tpIn.getWidth() / 4;
    knobTapeDrive->setBounds(tpIn.removeFromLeft(tpkw).reduced(2));
    knobTapeFlutter->setBounds(tpIn.removeFromLeft(tpkw).reduced(2));
    knobTapeWow->setBounds(tpIn.removeFromLeft(tpkw).reduced(2));
    knobTapeMix->setBounds(tpIn.reduced(2));

    b.removeFromTop(6);

    // Bottom row: Tone | Character | Output | Analog VU Meter
    auto botRow = b;
    const int vuW = juce::jlimit(180, 260, botRow.getWidth() * 27 / 100);
    const int charW = juce::jlimit(125, 165, botRow.getWidth() * 17 / 100);
    const int outW = juce::jlimit(145, 200, botRow.getWidth() * 22 / 100);

    vuBox = botRow.removeFromRight(vuW).reduced(3).toFloat();
    outBox = botRow.removeFromRight(outW).reduced(3).toFloat();
    charBox = botRow.removeFromRight(charW).reduced(3).toFloat();
    toneBox = botRow.reduced(3).toFloat();

    // Tone internals
    auto tnIn = toneBox.reduced(10.0f, 24.0f).toNearestInt();
    btnTonePower.setBounds(
        toneBox.reduced(8.0f, 4.0f).toNearestInt().removeFromTop(20).removeFromRight(42));
    knobToneLow->setBounds(tnIn.removeFromLeft(tnIn.getWidth() / 2).reduced(2));
    knobToneHigh->setBounds(tnIn.reduced(2));

    // Character internals
    auto cIn = charBox.reduced(12.0f, 28.0f).toNearestInt();
    const int ch = juce::jmax(22, (cIn.getHeight() - 8) / 3);
    for (int i = 0; i < 3; ++i)
      btnChar[static_cast<size_t>(i)].setBounds(
          cIn.removeFromTop(ch).reduced(2, 2));

    // Output internals
    btnOutPower.setBounds(
        outBox.reduced(8.0f, 4.0f).toNearestInt().removeFromTop(20).removeFromRight(42));
    auto oIn = outBox.reduced(10.0f, 24.0f).toNearestInt();
    btnSoftClip.setBounds(oIn.removeFromBottom(26).reduced(8, 2));
    knobOutTrim->setBounds(oIn.reduced(2));

    // VU Meter internals
    vuMeterRect = vuBox.reduced(12.0f, 26.0f);
  }

private:
  void timerCallback() override {
    if (!engine)
      return;
    const float targetVu = engine->getVuLevelDb();
    vuSmooth += (targetVu - vuSmooth) * 0.32f;
    if (engine->getTapeEnabled())
      reelAngle += 0.08f;
    repaint();
  }

  void drawPanelBox(juce::Graphics &g, juce::Rectangle<float> r,
                    const juce::String &title, juce::Colour accent) {
    g.setColour(juce::Colour(0xff17181c));
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(juce::Colour(0xff2e3038));
    g.drawRoundedRectangle(r, 6.0f, 1.2f);
    g.setColour(accent);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText(title, r.reduced(10.0f, 5.0f).withHeight(16.0f).toNearestInt(),
               juce::Justification::centredLeft);
  }

  void drawTubeGlow(juce::Graphics &g, juce::Rectangle<float> r) {
    if (r.isEmpty() || !engine)
      return;
    const bool on = engine->getTubeEnabled();
    const float driveNorm =
        static_cast<float>(engine->getTubeDriveDb() / 24.0);
    auto bulb = r.withSizeKeepingCentre(20.0f, 20.0f);
    g.setColour(on ? juce::Colour(0xfff59e0b).withAlpha(0.25f + 0.55f * driveNorm)
                   : juce::Colour(0xff27272a));
    g.fillEllipse(bulb.expanded(3.0f));
    g.setColour(on ? juce::Colour(0xfffbbf24) : juce::Colour(0xff3f3f46));
    g.fillEllipse(bulb);
  }

  void drawTapeReels(juce::Graphics &g, juce::Rectangle<float> r) {
    if (r.isEmpty() || !engine)
      return;
    const bool on = engine->getTapeEnabled();
    auto drawReel = [&](float cx, float cy) {
      const float rad = 9.0f;
      g.setColour(on ? juce::Colour(0xff38bdf8) : juce::Colour(0xff52525b));
      g.drawEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.4f);
      for (int i = 0; i < 3; ++i) {
        const float a = reelAngle + i * 2.094395f;
        g.drawLine(cx, cy, cx + std::cos(a) * rad, cy + std::sin(a) * rad, 1.2f);
      }
    };
    drawReel(r.getCentreX() - 14.0f, r.getCentreY());
    drawReel(r.getCentreX() + 14.0f, r.getCentreY());
  }

  void drawVuMeter(juce::Graphics &g, juce::Rectangle<float> r) {
    if (r.isEmpty())
      return;
    g.setColour(juce::Colour(0xff0b0c0e));
    g.fillRoundedRectangle(r.expanded(2.0f), 4.0f);

    juce::ColourGradient vuBg(juce::Colour(0xffdfb074), r.getCentreX(),
                              r.getCentreY(), juce::Colour(0xff8c5a2b),
                              r.getX(), r.getY(), true);
    g.setGradientFill(vuBg);
    g.fillRoundedRectangle(r, 3.0f);

    const float cx = r.getCentreX();
    const float cy = r.getBottom() + r.getHeight() * 0.35f;
    const float arcR = r.getHeight() * 0.90f;
    const float startA = -0.78f;
    const float endA = 0.78f;

    juce::Path arc;
    arc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startA, endA, true);
    g.setColour(juce::Colour(0xff23170d));
    g.strokePath(arc, juce::PathStrokeType(1.5f));

    // Red zone arc (0 .. +3 dB)
    juce::Path redArc;
    const float zeroA = startA + (20.0f / 23.0f) * (endA - startA);
    redArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, zeroA, endA, true);
    g.setColour(juce::Colour(0xffdc2626));
    g.strokePath(redArc, juce::PathStrokeType(3.0f));

    // VU Scale labels (-20, -10, -7, -5, -3, -2, -1, 0, +1, +2, +3)
    struct VuTick { float db; const char *txt; };
    static constexpr VuTick kTicks[] = {
        {-20, "-20"}, {-10, "-10"}, {-7, "-7"}, {-5, "-5"}, {-3, "-3"},
        {-1, "-1"},   {0, "0"},     {1, "+1"},  {2, "+2"},  {3, "+3"}};
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    for (const auto &tk : kTicks) {
      const float tn = (tk.db + 20.0f) / 23.0f;
      const float ta = startA + tn * (endA - startA);
      const float lx = cx + std::sin(ta) * (arcR + 13.0f);
      const float ly = cy - std::cos(ta) * (arcR + 13.0f);
      g.setColour(tk.db >= 0.0f ? juce::Colour(0xffb91c1c)
                                : juce::Colour(0xff23170d));
      g.drawText(tk.txt, static_cast<int>(lx - 12), static_cast<int>(ly - 6),
                 24, 12, juce::Justification::centred);
    }

    // VU Needle
    const float norm = juce::jlimit(0.0f, 1.0f, (vuSmooth + 20.0f) / 23.0f);
    const float needleA = startA + norm * (endA - startA);
    g.setColour(juce::Colour(0xff14100d));
    g.drawLine(cx + std::sin(needleA) * (arcR * 0.45f),
               cy - std::cos(needleA) * (arcR * 0.45f),
               cx + std::sin(needleA) * (arcR + 8.0f),
               cy - std::cos(needleA) * (arcR + 8.0f), 2.2f);

    g.setColour(juce::Colour(0xff1e130a));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("VU  " + juce::String(vuSmooth, 1) + " dB",
               r.withTop(r.getBottom() - 18.0f).toNearestInt(),
               juce::Justification::centred);
  }

  void updateButtons() {
    if (!engine)
      return;
    auto stylePwr = [](juce::TextButton &b, bool on, juce::Colour onCol) {
      b.setColour(juce::TextButton::buttonColourId,
                  on ? onCol : juce::Colour(0xff27272a));
      b.setColour(juce::TextButton::textColourOffId,
                  on ? juce::Colours::black : juce::Colour(0xff9ca3af));
    };
    stylePwr(btnTubePower, engine->getTubeEnabled(), juce::Colour(0xfff59e0b));
    stylePwr(btnTapePower, engine->getTapeEnabled(), juce::Colour(0xff38bdf8));
    stylePwr(btnTonePower, engine->getToneEnabled(), juce::Colour(0xff34d399));
    stylePwr(btnOutPower, engine->getOutputEnabled(), juce::Colour(0xfffacc15));

    const int cm = engine->getCharacterMode();
    for (int i = 0; i < 3; ++i) {
      const bool on = (i == cm);
      btnChar[static_cast<size_t>(i)].setColour(
          juce::TextButton::buttonColourId,
          on ? juce::Colour(0xfff59e0b) : juce::Colour(0xff27272a));
      btnChar[static_cast<size_t>(i)].setColour(
          juce::TextButton::textColourOffId,
          on ? juce::Colours::black : juce::Colour(0xffd1d5db));
    }

    const bool sc = engine->getSoftClip();
    btnSoftClip.setColour(juce::TextButton::buttonColourId,
                          sc ? juce::Colour(0xffea580c) : juce::Colour(0xff27272a));
    btnSoftClip.setColour(juce::TextButton::textColourOffId,
                          sc ? juce::Colours::white : juce::Colour(0xff9ca3af));

    btnAB.setButtonText(engine->isSlotB() ? "A / B: B" : "A / B: A");
    btnAB.setColour(juce::TextButton::buttonColourId,
                    engine->isSlotB() ? juce::Colour(0xff0ea5e9)
                                      : juce::Colour(0xff27272a));
    btnSettings.setButtonText(engine->getVuSourceInput() ? "VU: IN" : "VU: OUT");
    btnSettings.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff27272a));
    btnSave.setButtonText("SAVE");
    btnSave.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    btnPrevPreset.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    btnNextPreset.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
  }

  nova::graviton::DistortionEngine *engine = nullptr;
  float vuSmooth = -20.0f;
  float reelAngle = 0.0f;

  juce::Rectangle<float> headerRect, tubeBox, blendBox, tapeBox, toneBox,
      charBox, outBox, vuBox, tubeGlowRect, tapeReelsRect, vuMeterRect;

  juce::TextButton btnPrevPreset, btnNextPreset, btnSave, btnAB, btnSettings;
  juce::ComboBox cbPreset, cbTubeType, cbTapeType;
  juce::TextButton btnTubePower, btnTapePower, btnTonePower, btnOutPower,
      btnSoftClip;
  std::array<juce::TextButton, 3> btnChar;

  std::unique_ptr<NovaRotaryKnob> knobTubeDrive, knobTubeBias, knobTubeMix;
  std::unique_ptr<NovaRotaryKnob> knobBlend;
  std::unique_ptr<NovaRotaryKnob> knobTapeDrive, knobTapeFlutter, knobTapeWow,
      knobTapeMix;
  std::unique_ptr<NovaRotaryKnob> knobToneLow, knobToneHigh;
  std::unique_ptr<NovaRotaryKnob> knobOutTrim;
};

} // namespace nova
