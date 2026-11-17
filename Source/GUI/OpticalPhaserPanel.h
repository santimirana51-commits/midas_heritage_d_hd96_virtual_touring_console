#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Effects/GravitonOpticalPhaser.h"
#include <array>
#include <cmath>
#include <functional>

namespace nova {

// ==============================================================================
// 1. Hardware Fluted Black/Silver Knob for Benidub PHASE Optical Phaser
// ==============================================================================
class OpticalPhaserKnob final : public juce::Component {
public:
  enum class ScaleStyle { Dots, Ticks };

  OpticalPhaserKnob(const juce::String &titleText,
                    const juce::String &leftLbl,
                    const juce::String &rightLbl,
                    ScaleStyle scale,
                    double minV, double maxV, double defaultV,
                    bool isLarge = false)
      : title(titleText), minLabel(leftLbl), maxLabel(rightLbl),
        scaleStyle(scale), minVal(minV), maxVal(maxV), value(defaultV),
        defaultVal(defaultV), largeKnob(isLarge) {}

  std::function<void(double)> onValueChanged;
  std::function<juce::String(double)> valueFormatter;

  void setValue(double v, bool notify = false) {
    const double clamped = std::clamp(v, minVal, maxVal);
    if (std::abs(clamped - value) > 1e-6) {
      value = clamped;
      repaint();
      if (notify && onValueChanged)
        onValueChanged(value);
    }
  }
  double getValue() const noexcept { return value; }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth();
    const float h = b.getHeight();

    const float botReserve = 26.0f;
    const float topReserve = 10.0f;
    const float availH = std::max(36.0f, h - topReserve - botReserve);
    const float radius = std::min(w, availH) * (largeKnob ? 0.38f : 0.34f);
    const float cx = w * 0.5f;
    const float cy = topReserve + availH * 0.50f;

    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.78f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 0.78f;

    // Hardware Scale Ring (Dots for LFO DEPTH, Radial Ticks for others)
    if (scaleStyle == ScaleStyle::Dots) {
      const int numDots = 17;
      for (int i = 0; i < numDots; ++i) {
        const float norm = static_cast<float>(i) / static_cast<float>(numDots - 1);
        const float ang = startAngle + norm * (endAngle - startAngle);
        const float r = radius * 1.24f;
        const float dotR = 1.4f + norm * 2.2f;
        g.setColour(juce::Colour(0xffe2e8f0));
        g.fillEllipse(cx + std::sin(ang) * r - dotR * 0.5f,
                      cy - std::cos(ang) * r - dotR * 0.5f, dotR, dotR);
      }
    } else {
      const int numTicks = largeKnob ? 33 : 27;
      for (int i = 0; i < numTicks; ++i) {
        const float norm = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float ang = startAngle + norm * (endAngle - startAngle);
        const float sinA = std::sin(ang);
        const float cosA = -std::cos(ang);
        const bool isMajor = (i == 0 || i == numTicks / 2 || i == numTicks - 1 || i % 4 == 0);
        const float r1 = radius * 1.07f;
        const float r2 = radius * (isMajor ? 1.33f : 1.21f);
        g.setColour(isMajor ? juce::Colours::white
                            : juce::Colours::white.withAlpha(0.75f));
        g.drawLine(cx + sinA * r1, cy + cosA * r1, cx + sinA * r2, cy + cosA * r2,
                   isMajor ? 1.7f : 1.0f);
      }
      if (largeKnob) {
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawEllipse(cx - radius * 1.26f, cy - radius * 1.26f,
                      radius * 2.52f, radius * 2.52f, 1.0f);
      }
    }

    // Drop Shadow
    g.setColour(juce::Colours::black.withAlpha(0.60f));
    g.fillEllipse(cx - radius + 2.0f, cy - radius + 3.0f, radius * 2.0f,
                  radius * 2.0f);

    // Fluted Black Hardware Knob Body
    g.setColour(juce::Colour(0xff111317));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff3f444e));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.4f);

    // 7 scalloped flutes around knob perimeter
    for (int k = 0; k < 7; ++k) {
      const float a = static_cast<float>(k) * juce::MathConstants<float>::twoPi / 7.0f;
      const float rx = cx + std::sin(a) * (radius * 0.88f);
      const float ry = cy - std::cos(a) * (radius * 0.88f);
      g.setColour(juce::Colour(0xff23272f));
      g.fillEllipse(rx - radius * 0.16f, ry - radius * 0.16f,
                    radius * 0.32f, radius * 0.32f);
    }

    // Brushed Silver Center Cap
    const float capR = radius * 0.62f;
    juce::ColourGradient capGrad(juce::Colour(0xfff1f5f9), cx - capR * 0.4f,
                                 cy - capR * 0.6f, juce::Colour(0xff94a3b8),
                                 cx + capR * 0.5f, cy + capR * 0.7f, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // White Indicator Line
    const float normVal = static_cast<float>((value - minVal) / (maxVal - minVal));
    const float valAng = startAngle + normVal * (endAngle - startAngle);
    const float pSin = std::sin(valAng);
    const float pCos = -std::cos(valAng);

    g.setColour(juce::Colours::white);
    g.drawLine(cx + pSin * (capR * 0.65f), cy + pCos * (capR * 0.65f),
               cx + pSin * (radius * 0.96f), cy + pCos * (radius * 0.96f), 2.3f);

    // Min / Max Labels
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffe2e8f0));
    if (minLabel.isNotEmpty() || maxLabel.isNotEmpty()) {
      g.drawText(minLabel, 4, static_cast<int>(h - 27.0f),
                 static_cast<int>(w * 0.42f), 12,
                 juce::Justification::centredLeft);
      g.drawText(maxLabel, static_cast<int>(w * 0.58f),
                 static_cast<int>(h - 27.0f), static_cast<int>(w * 0.42f) - 4, 12,
                 juce::Justification::centredRight);
    }

    // Bottom Title or Live Readout on Drag
    if (isDragging && valueFormatter) {
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.setColour(juce::Colour(0xff38bdf8));
      g.drawText(valueFormatter(value), 0, static_cast<int>(h - 15.0f),
                 static_cast<int>(w), 14, juce::Justification::centred);
    } else {
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.setColour(juce::Colours::white);
      g.drawText(title, 0, static_cast<int>(h - 15.0f), static_cast<int>(w), 14,
                 juce::Justification::centred);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    isDragging = true;
    dragStartY = e.position.y;
    dragStartVal = value;
    repaint();
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const double dy = static_cast<double>(dragStartY - e.position.y);
    const double sens = e.mods.isShiftDown() ? 420.0 : 140.0;
    setValue(dragStartVal + (dy / sens) * (maxVal - minVal), true);
  }

  void mouseUp(const juce::MouseEvent &) override {
    isDragging = false;
    repaint();
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {
    setValue(defaultVal, true);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    const double step = (maxVal - minVal) * 0.03;
    setValue(value + (wheel.deltaY > 0.0f ? step : -step), true);
  }

private:
  juce::String title, minLabel, maxLabel;
  ScaleStyle scaleStyle;
  double minVal, maxVal, value, defaultVal;
  bool largeKnob = false;
  float dragStartY = 0.0f;
  double dragStartVal = 0.0;
  bool isDragging = false;
};

// ==============================================================================
// 2. Main Hardware Faceplate Panel: OpticalPhaserPanel (Benidub PHASE Style)
// ==============================================================================
class OpticalPhaserPanel final : public juce::Component, private juce::Timer {
public:
  explicit OpticalPhaserPanel(nova::graviton::OpticalPhaserEngine &eng)
      : engine(&eng),
        knobDepth("LFO DEPTH", "", "", OpticalPhaserKnob::ScaleStyle::Dots,
                  0.0, 100.0, 45.0, false),
        knobCenter("CENTRE FREQUENCY", "LOW", "HIGH",
                   OpticalPhaserKnob::ScaleStyle::Ticks, 0.0, 100.0, 50.0, true),
        knobRate("LFO RATE", "SLOW", "FAST",
                 OpticalPhaserKnob::ScaleStyle::Ticks, 0.0, 100.0, 18.0, false),
        knobFeedback("FEEDBACK", "MIN", "MAX",
                     OpticalPhaserKnob::ScaleStyle::Ticks, 0.0, 100.0, 20.0, false) {

    const auto &presets =
        nova::graviton::OpticalPhaserEngine::getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
      const juce::String prefix = presets[i].isFlagship ? "* " : "";
      presetCombo.addItem(prefix + juce::String(presets[i].name) + " (" +
                              juce::String(presets[i].instrumentTarget) + ")",
                          static_cast<int>(i + 1));
    }
    presetCombo.onChange = [this] {
      if (engine == nullptr)
        return;
      const int idx = presetCombo.getSelectedId() - 1;
      if (idx >= 0) {
        engine->loadPreset(idx);
        syncFromEngine();
      }
    };
    addAndMakeVisible(presetCombo);

    btnPrevPreset.setButtonText("<");
    btnPrevPreset.onClick = [this] {
      if (engine == nullptr)
        return;
      const int n = static_cast<int>(
          nova::graviton::OpticalPhaserEngine::getFactoryPresets().size());
      engine->loadPreset((engine->getCurrentPresetIndex() - 1 + n) % n);
      syncFromEngine();
    };
    addAndMakeVisible(btnPrevPreset);

    btnNextPreset.setButtonText(">");
    btnNextPreset.onClick = [this] {
      if (engine == nullptr)
        return;
      const int n = static_cast<int>(
          nova::graviton::OpticalPhaserEngine::getFactoryPresets().size());
      engine->loadPreset((engine->getCurrentPresetIndex() + 1) % n);
      syncFromEngine();
    };
    addAndMakeVisible(btnNextPreset);

    knobDepth.valueFormatter = [](double v) {
      return juce::String(v, 0) + " %";
    };
    knobDepth.onValueChanged = [this](double v) {
      if (engine)
        engine->setDepthPct(v);
    };
    addAndMakeVisible(knobDepth);

    knobCenter.valueFormatter = [this](double) {
      return engine ? (juce::String(engine->getCenterHz(), 0) + " Hz")
                    : juce::String("710 Hz");
    };
    knobCenter.onValueChanged = [this](double v) {
      if (engine)
        engine->setCenterPct(v);
    };
    addAndMakeVisible(knobCenter);

    knobRate.valueFormatter = [this](double) {
      return engine ? (juce::String(engine->getRateHz(), 2) + " Hz")
                    : juce::String("0.20 Hz");
    };
    knobRate.onValueChanged = [this](double v) {
      if (engine)
        engine->setRatePct(v);
    };
    addAndMakeVisible(knobRate);

    knobFeedback.valueFormatter = [](double v) {
      return juce::String(v, 0) + " %";
    };
    knobFeedback.onValueChanged = [this](double v) {
      if (engine)
        engine->setFeedbackPct(v);
    };
    addAndMakeVisible(knobFeedback);

    syncFromEngine();
    startTimerHz(30);
  }

  ~OpticalPhaserPanel() override { stopTimer(); }

  void setEngine(nova::graviton::OpticalPhaserEngine &eng) {
    engine = &eng;
    syncFromEngine();
  }

  void syncFromEngine() {
    if (engine == nullptr)
      return;
    presetCombo.setSelectedId(engine->getCurrentPresetIndex() + 1,
                              juce::dontSendNotification);
    knobDepth.setValue(engine->getDepthPct(), false);
    knobCenter.setValue(engine->getCenterPct(), false);
    knobRate.setValue(engine->getRatePct(), false);
    knobFeedback.setValue(engine->getFeedbackPct(), false);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0b0f16));

    if (engine == nullptr)
      return;

    const float headerH = 28.0f;
    // Top Header Bar
    g.setColour(juce::Colour(0xff111827));
    g.fillRect(0.0f, 0.0f, b.getWidth(), headerH);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawHorizontalLine(static_cast<int>(headerH), 0.0f, b.getWidth());

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff818cf8));
    g.drawText("NOVA HD // PHASE 12-STAGE OPTICAL", 8, 0, 215,
               static_cast<int>(headerH), juce::Justification::centredLeft);

    // 8-Instrument Quick-Select Bar
    static const char *kInstLabels[8] = {
        "E-Piano", "Organ", "Synth Pad", "Synth Lead",
        "Drum Loop", "Bass Synth", "Guitar", "Dub Send"};
    static constexpr int kInstPresetMap[8] = {0, 5, 10, 2, 9, 1, 7, 4};
    const int curPreset = engine->getCurrentPresetIndex();
    for (int i = 0; i < 8; ++i) {
      const auto &rBtn = rectInstBtns[static_cast<size_t>(i)];
      if (rBtn.getWidth() > 4.0f) {
        const bool isSel = (curPreset == kInstPresetMap[i]);
        g.setColour(isSel ? juce::Colour(0xff4f46e5) : juce::Colour(0xff1e293b));
        g.fillRoundedRectangle(rBtn, 3.0f);
        g.setColour(isSel ? juce::Colour(0xff818cf8) : juce::Colour(0xff334155));
        g.drawRoundedRectangle(rBtn, 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        g.setColour(isSel ? juce::Colours::white : juce::Colour(0xffcbd5e1));
        g.drawText(kInstLabels[i], rBtn, juce::Justification::centred);
      }
    }

    // Live Optical Telemetry in Header
    const float fcA = engine->getTelemetryFreqA();
    const float fcB = engine->getTelemetryFreqB();
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff4ade80));
    g.drawText("Fc L:" + juce::String(fcA, 0) + "Hz R:" +
                   juce::String(fcB, 0) + "Hz",
               static_cast<int>(b.getWidth() - 165.0f), 0, 158,
               static_cast<int>(headerH), juce::Justification::centredRight);

    // =========================================================================
    // 3 FLAGSHIP SIGNATURE PRESET BAR (Classic Optical | Dub Space | Stereo Motion)
    // =========================================================================
    static const char *kFlagTitles[3] = {
        "FLAGSHIP: CLASSIC OPTICAL", "FLAGSHIP: DUB SPACE",
        "FLAGSHIP: STEREO MOTION"};
    static constexpr int kFlagIdx[3] = {0, 4, 3};
    for (int f = 0; f < 3; ++f) {
      const auto &rF = rectFlagBtns[static_cast<size_t>(f)];
      const bool activeFlag = (curPreset == kFlagIdx[f]);
      g.setColour(activeFlag ? juce::Colour(0xff92400e) : juce::Colour(0xff141923));
      g.fillRoundedRectangle(rF, 4.0f);
      g.setColour(activeFlag ? juce::Colour(0xfff59e0b) : juce::Colour(0xff334155));
      g.drawRoundedRectangle(rF, 4.0f, 1.1f);
      g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::bold)));
      g.setColour(activeFlag ? juce::Colour(0xfffef3c7) : juce::Colour(0xffcbd5e1));
      g.drawText(kFlagTitles[f], rF, juce::Justification::centred);
    }

    // =========================================================================
    // HARDWARE PEDAL / DESKTOP UNIT CHASSIS (Benidub PHASE Cream & Black)
    // =========================================================================
    const float subBarY = headerH + 26.0f;
    const float chassisW = std::min(b.getWidth() - 24.0f, 680.0f);
    const float chassisH = std::min(b.getHeight() - subBarY - 12.0f, 440.0f);
    const auto unit = juce::Rectangle<float>(
        (b.getWidth() - chassisW) * 0.5f,
        subBarY + (b.getHeight() - subBarY - chassisH) * 0.5f,
        chassisW, chassisH);

    // Outer metallic enclosure shadow & bezel
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(unit.translated(3.0f, 4.0f), 10.0f);

    g.setColour(juce::Colour(0xff14171d));
    g.fillRoundedRectangle(unit, 10.0f);

    // Cream / Off-White Upper Faceplate
    auto face = unit.reduced(6.0f);
    g.setColour(juce::Colour(0xffe9e7e0));
    g.fillRoundedRectangle(face, 6.0f);

    // Matte Black Lower Contoured Silkscreen Section (Benidub Signature Wave)
    juce::Path blackArea;
    const float splitYLeft = face.getY() + face.getHeight() * 0.25f;
    const float splitYMid = face.getY() + face.getHeight() * 0.36f;
    const float splitYRight = face.getY() + face.getHeight() * 0.27f;

    blackArea.startNewSubPath(face.getX(), splitYLeft);
    blackArea.cubicTo(face.getX() + face.getWidth() * 0.22f, face.getY() + face.getHeight() * 0.12f,
                      face.getX() + face.getWidth() * 0.34f, splitYMid + 15.0f,
                      face.getCentreX(), splitYMid - 10.0f);
    blackArea.cubicTo(face.getX() + face.getWidth() * 0.66f, splitYMid - 28.0f,
                      face.getX() + face.getWidth() * 0.80f, face.getY() + face.getHeight() * 0.14f,
                      face.getRight(), splitYRight);
    blackArea.lineTo(face.getRight(), face.getBottom());
    blackArea.lineTo(face.getX(), face.getBottom());
    blackArea.closeSubPath();

    g.saveState();
    juce::Path clipFace;
    clipFace.addRoundedRectangle(face, 6.0f);
    g.reduceClipRegion(clipFace);
    g.setColour(juce::Colour(0xff1b1d21));
    g.fillPath(blackArea);

    // White Wedge behind MODE SELECT
    juce::Path modeWedge;
    const float mwCx = rectModeSelect.getCentreX();
    modeWedge.startNewSubPath(mwCx - 42.0f, face.getBottom());
    modeWedge.lineTo(mwCx - 10.0f, rectModeSelect.getY() - 10.0f);
    modeWedge.lineTo(mwCx + 10.0f, rectModeSelect.getY() - 10.0f);
    modeWedge.lineTo(mwCx + 42.0f, face.getBottom());
    modeWedge.closeSubPath();
    g.setColour(juce::Colour(0xffe9e7e0));
    g.fillPath(modeWedge);
    g.restoreState();

    // Top Jack Labels Strip
    const float jackY = face.getY() + 4.0f;
    g.setColour(juce::Colour(0xff1b1d21));
    g.drawLine(face.getX() + 10.0f, jackY + 15.0f, face.getRight() - 10.0f,
               jackY + 15.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    const char *jackNames[5] = {"INPUT L/MONO", "INPUT R", "12VDC IN",
                                "OUTPUT L", "OUTPUT R"};
    const float jStep = face.getWidth() / 5.0f;
    for (int j = 0; j < 5; ++j) {
      g.drawText(jackNames[j], static_cast<int>(face.getX() + j * jStep),
                 static_cast<int>(jackY), static_cast<int>(jStep), 14,
                 juce::Justification::centred);
    }

    // BENIDUB Logo Badge (Top-Right)
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff1b1d21));
    g.drawText("NOVA / BENI DUB", static_cast<int>(face.getRight() - 118.0f),
               static_cast<int>(face.getY() + 26.0f), 110, 14,
               juce::Justification::centredRight);

    // Top-Left BYPASS Push Button + Status LED
    const bool active = !engine->isBypassed();
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff1b1d21));
    g.drawText("BYPASS", static_cast<int>(rectBtnBypass.getX() - 52.0f),
               static_cast<int>(rectBtnBypass.getY()), 48,
               static_cast<int>(rectBtnBypass.getHeight()),
               juce::Justification::centredRight);
    drawRoundPushButton(g, rectBtnBypass, !active);

    // Power/Bypass LED next to BYPASS button
    const float pwrLedX = rectBtnBypass.getRight() + 24.0f;
    const float pwrLedY = rectBtnBypass.getCentreY();
    g.setColour(juce::Colour(0xff1b1d21));
    g.drawLine(rectBtnBypass.getRight() + 2.0f, pwrLedY, pwrLedX - 6.0f,
               pwrLedY, 1.4f);
    drawBezelLed(g, {pwrLedX, pwrLedY}, active, juce::Colour(0xff22c55e));

    // Top-Right LFO MODE (LINEAR / MIRRORED) Push Button + Optical LFO LED
    const bool mirrored = engine->isMirroredLfo();
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(mirrored ? juce::Colour(0xff64748b) : juce::Colour(0xff111827));
    g.drawText("LINEAR", static_cast<int>(rectBtnLfoMode.getX() - 54.0f),
               static_cast<int>(rectBtnLfoMode.getY() - 2.0f), 50, 12,
               juce::Justification::centredRight);
    g.setColour(mirrored ? juce::Colour(0xff111827) : juce::Colour(0xff64748b));
    g.drawText("MIRRORED", static_cast<int>(rectBtnLfoMode.getX() - 56.0f),
               static_cast<int>(rectBtnLfoMode.getY() + 12.0f), 52, 12,
               juce::Justification::centredRight);
    g.setColour(juce::Colour(0xff1b1d21));
    g.drawText("LFO MODE", static_cast<int>(rectBtnLfoMode.getX() - 10.0f),
               static_cast<int>(rectBtnLfoMode.getBottom() + 4.0f),
               static_cast<int>(rectBtnLfoMode.getWidth() + 20.0f), 12,
               juce::Justification::centred);
    drawRoundPushButton(g, rectBtnLfoMode, mirrored);

    // Live Optical Photoresistor Pulse LED (pulses smoothly with LFO)
    const float lfoLedX = rectBtnLfoMode.getRight() + 26.0f;
    const float lfoLedY = rectBtnLfoMode.getCentreY();
    const float lfoNorm = std::clamp(0.5f + 0.5f * engine->getTelemetryLfoB(), 0.0f, 1.0f);
    drawBezelLed(g, {lfoLedX, lfoLedY}, active,
                 juce::Colour(0xfff59e0b).withAlpha(active ? (0.35f + 0.65f * lfoNorm) : 0.15f));

    // Bottom-Left INPUT (INSERT / AUX) Push Button
    const bool isAux = engine->isAuxRouting();
    g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
    g.setColour(!isAux ? juce::Colours::white : juce::Colour(0xff64748b));
    g.drawText("INSERT", static_cast<int>(rectBtnInputMode.getX() - 46.0f),
               static_cast<int>(rectBtnInputMode.getY() - 1.0f), 42, 12,
               juce::Justification::centredRight);
    g.setColour(isAux ? juce::Colour(0xff38bdf8) : juce::Colour(0xff64748b));
    g.drawText("AUX", static_cast<int>(rectBtnInputMode.getX() - 46.0f),
               static_cast<int>(rectBtnInputMode.getY() + 12.0f), 42, 12,
               juce::Justification::centredRight);
    g.setColour(juce::Colours::white);
    g.drawText("INPUT", static_cast<int>(rectBtnInputMode.getX() - 8.0f),
               static_cast<int>(rectBtnInputMode.getBottom() + 5.0f),
               static_cast<int>(rectBtnInputMode.getWidth() + 16.0f), 12,
               juce::Justification::centred);
    drawRoundPushButton(g, rectBtnInputMode, isAux);

    // Bottom-Left-Center MODE SELECT (MONO 12 STAGE / STEREO 6-6 STAGE)
    const bool isStereo = engine->isStereo6Plus6();
    auto rMonoBox = juce::Rectangle<float>(rectModeSelect.getCentreX() - 50.0f,
                                           rectModeSelect.getY() - 26.0f, 48.0f, 20.0f);
    auto rStereoBox = juce::Rectangle<float>(rectModeSelect.getCentreX() + 2.0f,
                                             rectModeSelect.getY() - 26.0f, 48.0f, 20.0f);
    auto drawModeTag = [&](juce::Rectangle<float> r, const char *l1, const char *l2, bool sel) {
      g.setColour(sel ? juce::Colour(0xff4f46e5) : juce::Colour(0xff0f172a));
      g.fillRoundedRectangle(r, 2.0f);
      g.setColour(sel ? juce::Colour(0xff818cf8) : juce::Colours::white.withAlpha(0.7f));
      g.drawRoundedRectangle(r, 2.0f, 1.0f);
      g.setFont(juce::Font(juce::FontOptions(6.8f, juce::Font::bold)));
      g.setColour(juce::Colours::white);
      g.drawFittedText(juce::String(l1) + "\n" + l2, r.toNearestInt(),
                       juce::Justification::centred, 2);
    };
    drawModeTag(rMonoBox, "MONO", "12 STAGE", !isStereo);
    drawModeTag(rStereoBox, "STEREO", "6-6 STAGE", isStereo);

    // White Pointer Rotary Selector for MODE SELECT
    const float msCx = rectModeSelect.getCentreX();
    const float msCy = rectModeSelect.getCentreY();
    const float msR = std::min(rectModeSelect.getWidth(), rectModeSelect.getHeight()) * 0.44f;
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(msCx - msR + 1.5f, msCy - msR + 2.5f, msR * 2.0f, msR * 2.0f);
    g.setColour(juce::Colour(0xfff1f5f9));
    g.fillEllipse(msCx - msR, msCy - msR, msR * 2.0f, msR * 2.0f);
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawEllipse(msCx - msR, msCy - msR, msR * 2.0f, msR * 2.0f, 1.2f);
    const float ptrAng = isStereo ? 0.55f : -0.55f;
    g.setColour(juce::Colour(0xff111827));
    g.drawLine(msCx, msCy, msCx + std::sin(ptrAng) * msR * 0.85f,
               msCy - std::cos(ptrAng) * msR * 0.85f, 3.0f);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff1b1d21));
    g.drawFittedText("MODE\nSELECT",
                     juce::Rectangle<int>(static_cast<int>(msCx - 35.0f),
                                          static_cast<int>(rectModeSelect.getBottom() + 2.0f),
                                          70, 22),
                     juce::Justification::centred, 2);

    // Signature Geometric PHASE Center Emblem
    const float embCx = face.getCentreX();
    const float embCy = rectModeSelect.getCentreY() - 6.0f;
    g.setColour(juce::Colours::white);
    g.drawRect(embCx - 16.0f, embCy - 20.0f, 32.0f, 36.0f, 1.5f);
    g.drawEllipse(embCx - 10.0f, embCy - 14.0f, 20.0f, 24.0f, 1.8f);
    g.fillRect(embCx - 2.0f, embCy - 12.0f, 4.0f, 20.0f);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("PHASE", static_cast<int>(embCx - 36.0f),
               static_cast<int>(embCy + 20.0f), 72, 16,
               juce::Justification::centred);
  }

  void resized() override {
    const auto b = getLocalBounds().toFloat();
    const float headerH = 28.0f;

    btnPrevPreset.setBounds(226, 4, 22, 20);
    presetCombo.setBounds(250, 4, 200, 20);
    btnNextPreset.setBounds(452, 4, 22, 20);

    const float instStartX = 480.0f;
    const float instEndX = std::max(instStartX + 140.0f, b.getWidth() - 170.0f);
    const float instBtnW = (instEndX - instStartX) / 8.0f;
    for (int i = 0; i < 8; ++i) {
      rectInstBtns[static_cast<size_t>(i)] = {
          instStartX + i * instBtnW + 1.0f, 5.0f, instBtnW - 2.0f, 18.0f};
    }

    // 3 Flagship Signature Preset Bar
    const float flagBarW = std::min(b.getWidth() - 24.0f, 680.0f);
    const float flagBarX = (b.getWidth() - flagBarW) * 0.5f;
    const float flagW = flagBarW / 3.0f;
    for (int f = 0; f < 3; ++f) {
      rectFlagBtns[static_cast<size_t>(f)] = {
          flagBarX + f * flagW + 2.0f, headerH + 4.0f, flagW - 4.0f, 18.0f};
    }

    const float subBarY = headerH + 26.0f;
    const float chassisW = std::min(b.getWidth() - 24.0f, 680.0f);
    const float chassisH = std::min(b.getHeight() - subBarY - 12.0f, 440.0f);
    const auto unit = juce::Rectangle<float>(
        (b.getWidth() - chassisW) * 0.5f,
        subBarY + (b.getHeight() - subBarY - chassisH) * 0.5f,
        chassisW, chassisH);
    const auto face = unit.reduced(6.0f);

    // Top Buttons
    rectBtnBypass = {face.getX() + 80.0f, face.getY() + 28.0f, 26.0f, 26.0f};
    rectBtnLfoMode = {face.getRight() - 124.0f, face.getY() + 28.0f, 26.0f, 26.0f};

    // Main 3 Top Knobs (LFO DEPTH | CENTRE FREQUENCY | LFO RATE)
    const float row1Y = face.getY() + face.getHeight() * 0.21f;
    const float row1H = face.getHeight() * 0.36f;
    const float colW = face.getWidth() / 3.0f;

    knobDepth.setBounds(static_cast<int>(face.getX() + 12.0f),
                        static_cast<int>(row1Y + 8.0f),
                        static_cast<int>(colW - 24.0f),
                        static_cast<int>(row1H - 8.0f));
    knobCenter.setBounds(static_cast<int>(face.getX() + colW - 8.0f),
                         static_cast<int>(row1Y - 6.0f),
                         static_cast<int>(colW + 16.0f),
                         static_cast<int>(row1H + 14.0f));
    knobRate.setBounds(static_cast<int>(face.getX() + colW * 2.0f + 12.0f),
                       static_cast<int>(row1Y + 8.0f),
                       static_cast<int>(colW - 24.0f),
                       static_cast<int>(row1H - 8.0f));

    // Bottom Row: INPUT button | MODE SELECT | FEEDBACK Knob
    const float row2Y = face.getY() + face.getHeight() * 0.59f;
    const float row2H = face.getHeight() * 0.37f;

    rectBtnInputMode = {face.getX() + 62.0f, row2Y + row2H * 0.35f, 26.0f, 26.0f};
    rectModeSelect = {face.getX() + colW * 0.56f, row2Y + row2H * 0.28f,
                      48.0f, 48.0f};
    knobFeedback.setBounds(static_cast<int>(face.getX() + colW * 2.0f + 12.0f),
                           static_cast<int>(row2Y),
                           static_cast<int>(colW - 24.0f),
                           static_cast<int>(row2H));
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (engine == nullptr)
      return;
    const auto p = e.position;

    static constexpr int kFlagIdx[3] = {0, 4, 3};
    for (int f = 0; f < 3; ++f) {
      if (rectFlagBtns[static_cast<size_t>(f)].contains(p)) {
        engine->loadPreset(kFlagIdx[f]);
        syncFromEngine();
        return;
      }
    }

    static constexpr int kInstPresetMap[8] = {0, 5, 10, 2, 9, 1, 7, 4};
    for (int i = 0; i < 8; ++i) {
      if (rectInstBtns[static_cast<size_t>(i)].contains(p)) {
        engine->loadPreset(kInstPresetMap[i]);
        syncFromEngine();
        return;
      }
    }

    // Direct Mono / Stereo Tag Clicks above Mode Select
    auto rMonoBox = juce::Rectangle<float>(rectModeSelect.getCentreX() - 50.0f,
                                           rectModeSelect.getY() - 26.0f, 48.0f, 20.0f);
    auto rStereoBox = juce::Rectangle<float>(rectModeSelect.getCentreX() + 2.0f,
                                             rectModeSelect.getY() - 26.0f, 48.0f, 20.0f);
    if (rMonoBox.contains(p)) {
      engine->setStageMode(
          nova::graviton::OpticalPhaserEngine::StageMode::Mono12Stage);
      repaint();
      return;
    }
    if (rStereoBox.contains(p)) {
      engine->setStageMode(
          nova::graviton::OpticalPhaserEngine::StageMode::Stereo6Plus6);
      repaint();
      return;
    }

    if (rectBtnBypass.expanded(18.0f, 10.0f).contains(p)) {
      engine->setBypassed(!engine->isBypassed());
      repaint();
    } else if (rectBtnLfoMode.withLeft(rectBtnLfoMode.getX() - 58.0f).expanded(6.0f, 10.0f).contains(p)) {
      engine->setLfoMode(
          engine->isMirroredLfo()
              ? nova::graviton::OpticalPhaserEngine::LfoMode::Linear
              : nova::graviton::OpticalPhaserEngine::LfoMode::Mirrored);
      repaint();
    } else if (rectBtnInputMode.withLeft(rectBtnInputMode.getX() - 50.0f).expanded(6.0f, 10.0f).contains(p)) {
      engine->setRoutingMode(
          engine->isAuxRouting()
              ? nova::graviton::OpticalPhaserEngine::RoutingMode::Insert50_50
              : nova::graviton::OpticalPhaserEngine::RoutingMode::AuxFullWet);
      repaint();
    } else if (rectModeSelect.expanded(18.0f).contains(p)) {
      engine->setStageMode(
          engine->isStereo6Plus6()
              ? nova::graviton::OpticalPhaserEngine::StageMode::Mono12Stage
              : nova::graviton::OpticalPhaserEngine::StageMode::Stereo6Plus6);
      repaint();
    }
  }

private:
  void timerCallback() override { repaint(); }

  static void drawRoundPushButton(juce::Graphics &g, juce::Rectangle<float> r,
                                  bool depressed) {
    g.setColour(juce::Colour(0xff111317));
    g.fillEllipse(r.expanded(2.5f));
    g.setColour(depressed ? juce::Colour(0xff374151) : juce::Colour(0xff6b7280));
    g.fillEllipse(r);
    g.setColour(juce::Colours::white.withAlpha(depressed ? 0.18f : 0.40f));
    g.drawEllipse(r, 1.2f);
  }

  static void drawBezelLed(juce::Graphics &g, juce::Point<float> c, bool on,
                           juce::Colour col) {
    g.setColour(juce::Colour(0xff111317));
    g.fillEllipse(c.x - 7.5f, c.y - 7.5f, 15.0f, 15.0f);
    if (on) {
      g.setColour(col.withAlpha(0.35f));
      g.fillEllipse(c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f);
      g.setColour(col);
      g.fillEllipse(c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f);
    } else {
      g.setColour(juce::Colour(0xff334155));
      g.fillEllipse(c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f);
    }
  }

  nova::graviton::OpticalPhaserEngine *engine = nullptr;

  juce::ComboBox presetCombo;
  juce::TextButton btnPrevPreset, btnNextPreset;

  OpticalPhaserKnob knobDepth, knobCenter, knobRate, knobFeedback;

  juce::Rectangle<float> rectBtnBypass, rectBtnLfoMode, rectBtnInputMode,
      rectModeSelect;
  std::array<juce::Rectangle<float>, 8> rectInstBtns{};
  std::array<juce::Rectangle<float>, 3> rectFlagBtns{};
};

} // namespace nova
