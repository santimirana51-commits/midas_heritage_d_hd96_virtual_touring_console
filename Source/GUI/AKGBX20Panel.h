#pragma once

#include "Effects/GravitonPlateReverb.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace nova {

// ==============================================================================
// 1. AKG BX 20 Fluted Vintage Knob with Domed Pearl/Cream Insert
// ==============================================================================
class BX20Knob final : public juce::Component {
public:
  enum class ScaleType {
    Input,      // -12 to +12 dB
    TankSelect, // 0 = A, 1 = B, 2 = A/B
    Bass,       // -8 to +8 dB
    Treble,     // -4 to +4 dB
    DryWet,     // 0 to 100%
    PreDelay,   // 0 to 250 ms
    Decay,      // 2.0 to 4.5 s
    Volume,     // -60 to +12 dB
    Pan         // -1 to +1
  };

  BX20Knob(const juce::String &paramTitle, double minV, double maxV,
           double defaultV, const juce::String &unitSuffix,
           ScaleType scale = ScaleType::Input, int numDecimals = 1,
           bool isLog = false)
      : title(paramTitle), minValue(minV), maxValue(maxV),
        defaultValue(defaultV), currentValue(defaultV), unit(unitSuffix),
        scaleType(scale), decimals(numDecimals), logarithmic(isLog) {
    setRepaintsOnMouseActivity(true);
  }

  void setValue(double val,
                juce::NotificationType notify = juce::dontSendNotification) {
    const double clamped = std::clamp(val, minValue, maxValue);
    if (std::abs(clamped - currentValue) > 1e-4) {
      currentValue = clamped;
      repaint();
      if (notify != juce::dontSendNotification && onValueChanged) {
        onValueChanged(currentValue);
      }
    }
  }

  double getValue() const noexcept { return currentValue; }
  double getDefaultValue() const noexcept { return defaultValue; }

  std::function<void(double)> onValueChanged;
  std::function<void()> onDragEnded;

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    const float titleH = 13.0f;
    auto titleArea = bounds.removeFromTop(titleH);
    auto valArea = bounds.removeFromBottom(12.0f);

    // Title Label
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff212529));
    g.drawText(title, titleArea, juce::Justification::centred, false);

    // Knob area
    const float knobSize =
        std::clamp(std::min(bounds.getWidth(), bounds.getHeight()) - 2.0f,
                   18.0f, 44.0f);
    const auto knobArea = bounds.withSizeKeepingCentre(knobSize, knobSize);
    const float cx = knobArea.getCentreX();
    const float cy = knobArea.getCentreY();
    const float radius = knobSize * 0.5f;

    // Drop shadow under knob
    g.setColour(juce::Colour(0x30000000));
    g.fillEllipse(knobArea.translated(1.0f, 1.5f));

    // Outer fluted skirt (Matte black bakelite)
    juce::ColourGradient skirtGrad(juce::Colour(0xff2c3035), cx - radius * 0.5f,
                                   cy - radius * 0.5f, juce::Colour(0xff121416),
                                   cx + radius * 0.5f, cy + radius * 0.5f, true);
    g.setGradientFill(skirtGrad);
    g.fillEllipse(knobArea);
    g.setColour(juce::Colour(0xff0d0e10));
    g.drawEllipse(knobArea, 1.0f);

    // Radial fluted ribs on skirt
    const int numFlutes = 16;
    g.setColour(juce::Colour(0x35ffffff));
    for (int i = 0; i < numFlutes; ++i) {
      const float angle = i * (juce::MathConstants<float>::twoPi / numFlutes);
      const float r1 = radius - 1.2f;
      const float r2 = radius - 3.8f;
      g.drawLine(cx + r1 * std::cos(angle), cy + r1 * std::sin(angle),
                 cx + r2 * std::cos(angle), cy + r2 * std::sin(angle), 0.9f);
    }

    // Inner Cream / Pearl Domed Cap
    const float capRadius = radius * 0.62f;
    const auto capArea = juce::Rectangle<float>(
        cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    juce::ColourGradient pearlGrad(
        juce::Colour(0xffffffff), cx - capRadius * 0.35f, cy - capRadius * 0.35f,
        juce::Colour(0xffdcd6c8), cx + capRadius * 0.5f, cy + capRadius * 0.5f,
        true);
    g.setGradientFill(pearlGrad);
    g.fillEllipse(capArea);
    g.setColour(juce::Colour(0xff8c867a));
    g.drawEllipse(capArea, 0.8f);

    // Pointer Dot on fluted skirt
    float norm = 0.0f;
    if (logarithmic && minValue > 0.0) {
      norm = static_cast<float>(std::log(currentValue / minValue) /
                                std::log(maxValue / minValue));
    } else {
      norm = static_cast<float>((currentValue - minValue) / (maxValue - minValue));
    }
    norm = std::clamp(norm, 0.0f, 1.0f);

    const float startAngle = juce::MathConstants<float>::pi * 0.75f;
    const float endAngle = juce::MathConstants<float>::pi * 2.25f;
    const float curAngle = startAngle + norm * (endAngle - startAngle);

    const float dotDist = radius - 2.8f;
    const float dotX = cx + dotDist * std::cos(curAngle);
    const float dotY = cy + dotDist * std::sin(curAngle);

    g.setColour(juce::Colour(0xffffffff));
    g.fillEllipse(dotX - 1.6f, dotY - 1.6f, 3.2f, 3.2f);

    // Value Text below knob
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff212529));

    juce::String valStr;
    if (scaleType == ScaleType::TankSelect) {
      const int sel = static_cast<int>(std::round(currentValue));
      valStr = (sel == 0) ? "TANK A" : (sel == 1) ? "TANK B" : "TANK A/B";
    } else if (scaleType == ScaleType::Pan) {
      if (std::abs(currentValue) < 0.05)
        valStr = "C";
      else if (currentValue < 0.0)
        valStr = "L " + juce::String(static_cast<int>(std::round(-currentValue * 100)));
      else
        valStr = "R " + juce::String(static_cast<int>(std::round(currentValue * 100)));
    } else {
      if (decimals == 0)
        valStr = juce::String(static_cast<int>(std::round(currentValue)));
      else
        valStr = juce::String(currentValue, decimals);
      if (unit.isNotEmpty())
        valStr << " " << unit;
    }

    g.drawText(valStr, valArea, juce::Justification::centred, false);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      setValue(defaultValue, juce::sendNotificationSync);
      return;
    }
    dragStartVal = currentValue;
    dragStartY = e.position.y;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float deltaY = dragStartY - e.position.y;
    const double range = maxValue - minValue;
    const double step = range * (e.mods.isShiftDown() ? 0.001 : 0.005);
    setValue(dragStartVal + deltaY * step, juce::sendNotification);
  }

  void mouseUp(const juce::MouseEvent &) override {
    if (onDragEnded)
      onDragEnded();
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {
    setValue(defaultValue, juce::sendNotification);
    if (onDragEnded)
      onDragEnded();
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    const double range = maxValue - minValue;
    const double step = range * 0.02 * wheel.deltaY;
    setValue(currentValue + step, juce::sendNotification);
    if (onDragEnded)
      onDragEnded();
  }

private:
  juce::String title;
  double minValue, maxValue, defaultValue, currentValue;
  juce::String unit;
  ScaleType scaleType;
  int decimals;
  bool logarithmic;
  double dragStartVal = 0.0;
  float dragStartY = 0.0f;
};

// ==============================================================================
// 2. Vintage AKG Jewel Pilot Lamp (Glowing Amber/Orange Incandescent Indicator)
// ==============================================================================
class BX20JewelLamp final : public juce::Component {
public:
  BX20JewelLamp() { setOpaque(false); }

  void setLit(bool lit) {
    if (isLit != lit) {
      isLit = lit;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    const float size = std::min(bounds.getWidth(), bounds.getHeight()) - 4.0f;
    const auto area = bounds.withSizeKeepingCentre(size, size);
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float radius = size * 0.5f;

    // Chrome outer threaded retaining bezel ring
    juce::ColourGradient chromeGrad(juce::Colour(0xffffffff), cx - radius, cy - radius,
                                    juce::Colour(0xff6c757d), cx + radius, cy + radius,
                                    false);
    g.setGradientFill(chromeGrad);
    g.fillEllipse(area);
    g.setColour(juce::Colour(0xff343a40));
    g.drawEllipse(area, 1.2f);

    // Inner faceted jewel glass
    const float jewelRadius = radius * 0.72f;
    const auto jewelArea =
        juce::Rectangle<float>(cx - jewelRadius, cy - jewelRadius,
                               jewelRadius * 2.0f, jewelRadius * 2.0f);

    if (isLit) {
      // Radiant amber jewel lens
      juce::ColourGradient glowGrad(juce::Colour(0xffffe082), cx - jewelRadius * 0.3f,
                                    cy - jewelRadius * 0.3f, juce::Colour(0xffe65100),
                                    cx + jewelRadius, cy + jewelRadius, false);
      g.setGradientFill(glowGrad);
      g.fillEllipse(jewelArea);

      // Faceted specular highlights
      g.setColour(juce::Colour(0x80ffffff));
      g.drawEllipse(jewelArea.reduced(2.0f), 1.0f);
      g.fillEllipse(cx - jewelRadius * 0.45f, cy - jewelRadius * 0.45f, 4.0f, 4.0f);
    } else {
      // Dark extinguished amber glass
      juce::ColourGradient darkGrad(juce::Colour(0xff4a2c00), cx - jewelRadius * 0.3f,
                                    cy - jewelRadius * 0.3f, juce::Colour(0xff1f1200),
                                    cx + jewelRadius, cy + jewelRadius, false);
      g.setGradientFill(darkGrad);
      g.fillEllipse(jewelArea);
    }

    g.setColour(juce::Colour(0xff212529));
    g.drawEllipse(jewelArea, 1.0f);
  }

private:
  bool isLit = true;
};

// ==============================================================================
// 3. Main AKG BX 20 Hardware GUI Panel
// ==============================================================================
class AKGBX20Panel final : public juce::Component {
public:
  explicit AKGBX20Panel(nova::graviton::VintageSpringReverb &springEngine)
      : engine(springEngine) {
    setOpaque(true);

    // 1. Toolbar Preset Setup
    setupPresetToolbar();

    // 2. Section 1: Input & Tank Architecture
    inputKnob = std::make_unique<BX20Knob>("INPUT GAIN", -12.0, 12.0, 0.0, "dB",
                                           BX20Knob::ScaleType::Input, 1);
    tankSelectKnob = std::make_unique<BX20Knob>("TANK SELECT", 0.0, 2.0, 2.0, "",
                                                BX20Knob::ScaleType::TankSelect, 0);

    // 3. Section 2: Channel 1 TTL Spring Core
    ch1DecayKnob = std::make_unique<BX20Knob>("CH 1 DECAY", 2.0, 4.5, 3.0, "s",
                                              BX20Knob::ScaleType::Decay, 1);
    ch1VolKnob = std::make_unique<BX20Knob>("CH 1 VOL", -60.0, 12.0, 0.0, "dB",
                                            BX20Knob::ScaleType::Volume, 1);
    ch1PanKnob = std::make_unique<BX20Knob>("CH 1 PAN", -1.0, 1.0, -0.8, "",
                                            BX20Knob::ScaleType::Pan, 2);

    // 4. Section 3: Baxandall Tone Shaping
    bassKnob = std::make_unique<BX20Knob>("BASS (150Hz)", -8.0, 8.0, 0.0, "dB",
                                          BX20Knob::ScaleType::Bass, 1);
    trebleKnob = std::make_unique<BX20Knob>("TREBLE (5kHz)", -4.0, 4.0, 0.0, "dB",
                                            BX20Knob::ScaleType::Treble, 1);

    // 5. Section 4: Channel 2 TTL Spring Core
    ch2DecayKnob = std::make_unique<BX20Knob>("CH 2 DECAY", 2.0, 4.5, 3.0, "s",
                                              BX20Knob::ScaleType::Decay, 1);
    ch2VolKnob = std::make_unique<BX20Knob>("CH 2 VOL", -60.0, 12.0, 0.0, "dB",
                                            BX20Knob::ScaleType::Volume, 1);
    ch2PanKnob = std::make_unique<BX20Knob>("CH 2 PAN", -1.0, 1.0, 0.8, "",
                                            BX20Knob::ScaleType::Pan, 2);

    // 6. Section 5: Master Blend & Monitor
    preDelayKnob = std::make_unique<BX20Knob>("PREDELAY", 0.0, 250.0, 0.0, "ms",
                                              BX20Knob::ScaleType::PreDelay, 0, true);
    dryWetKnob = std::make_unique<BX20Knob>("DRY / WET", 0.0, 100.0, 15.0, "%",
                                            BX20Knob::ScaleType::DryWet, 0);
    jewelLamp = std::make_unique<BX20JewelLamp>();

    // Interactive Toggle Buttons on Toolbar/Chassis
    setupToggle(btnInputStereo, "STEREO IN", [this] {
      const bool n = !engine.isInputStereo();
      engine.setInputStereo(n);
      updateToggleButtons();
      pushUndo();
    });

    setupToggle(btnDirect, "DIRECT", [this] {
      const bool n = !engine.isDirectEnabled();
      engine.setDirectEnabled(n);
      updateToggleButtons();
      pushUndo();
    });

    setupToggle(btnLowCut, "LOW CUT 80Hz", [this] {
      const bool n = !engine.isLowCut();
      engine.setLowCut(n);
      updateToggleButtons();
      pushUndo();
    });

    setupToggle(btnLink, "LINK CH1/2", [this] {
      const bool n = !engine.isLink();
      engine.setLink(n);
      updateToggleButtons();
      pushUndo();
    });

    setupToggle(btnWetSolo, "WET SOLO", [this] {
      const bool n = !engine.isWetSolo();
      engine.setWetSolo(n);
      updateToggleButtons();
      pushUndo();
    });

    setupToggle(btnPower, "\xe2\x8f\xbb", [this] {
      const bool n = !engine.getPower();
      engine.setPower(n);
      jewelLamp->setLit(n);
      updateToggleButtons();
      repaint();
    });

    // Add visual components
    addAndMakeVisible(*inputKnob);
    addAndMakeVisible(*tankSelectKnob);

    addAndMakeVisible(*ch1DecayKnob);
    addAndMakeVisible(*ch1VolKnob);
    addAndMakeVisible(*ch1PanKnob);

    addAndMakeVisible(*bassKnob);
    addAndMakeVisible(*trebleKnob);

    addAndMakeVisible(*ch2DecayKnob);
    addAndMakeVisible(*ch2VolKnob);
    addAndMakeVisible(*ch2PanKnob);

    addAndMakeVisible(*preDelayKnob);
    addAndMakeVisible(*dryWetKnob);
    addAndMakeVisible(*jewelLamp);

    bindCallbacks();
    syncFromEngine();

    setSize(1020, 520);
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // 1. Rich Austrian Walnut Wood Cabinet Frame
    juce::ColourGradient woodGrad(
        juce::Colour(0xff4a2717), bounds.getX(), bounds.getY(),
        juce::Colour(0xff29140b), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(woodGrad);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Fine wood grain texture lines
    g.setColour(juce::Colour(0x15000000));
    for (float y = bounds.getY() + 3.0f; y < bounds.getBottom(); y += 5.0f) {
      g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Top Toolbar Area
    const float toolbarH = (getHeight() < 380) ? 30.0f : 40.0f;
    auto toolbar = bounds.removeFromTop(toolbarH).reduced(4.0f, 2.0f);
    g.setColour(juce::Colour(0xff181a1d));
    g.fillRoundedRectangle(toolbar, 3.0f);

    // Subtitle on right of toolbar
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffc9d1d9));
    g.drawText("AKG BX 20  \xc2\xb7  TORSIONAL TRANSMISSION LINE",
               static_cast<int>(toolbar.getRight() - 320.0f), 0, 310,
               static_cast<int>(toolbarH), juce::Justification::centredRight, false);

    // 2. Inner Recessed Brushed Warm-Silver Aluminum Faceplate
    auto plateArea = bounds.reduced(10.0f, 6.0f);
    juce::ColourGradient plateGrad(
        juce::Colour(0xffe8ebef), plateArea.getX(), plateArea.getY(),
        juce::Colour(0xffcbd0d6), plateArea.getRight(), plateArea.getBottom(), false);
    g.setGradientFill(plateGrad);
    g.fillRoundedRectangle(plateArea, 3.5f);

    // Outer metal bevel border
    g.setColour(juce::Colour(0xff9aa0a8));
    g.drawRoundedRectangle(plateArea, 3.5f, 1.2f);

    // Corner mounting screws (+)
    auto drawScrew = [&g](float x, float y) {
      g.setColour(juce::Colour(0xff868e96));
      g.fillEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f);
      g.setColour(juce::Colour(0xff212529));
      g.drawLine(x - 2.5f, y, x + 2.5f, y, 0.8f);
      g.drawLine(x, y - 2.5f, x, y + 2.5f, 0.8f);
    };
    drawScrew(plateArea.getX() + 6.0f, plateArea.getY() + 6.0f);
    drawScrew(plateArea.getRight() - 6.0f, plateArea.getY() + 6.0f);
    drawScrew(plateArea.getX() + 6.0f, plateArea.getBottom() - 6.0f);
    drawScrew(plateArea.getRight() - 6.0f, plateArea.getBottom() - 6.0f);

    // Vertical Divider Lines between Modules
    const float secY = plateArea.getY() + 6.0f;
    const float secH = plateArea.getHeight() - 12.0f;
    g.setColour(juce::Colour(0xffa0a6ae));

    const float divXs[] = {
        ch1DecayKnob->getX() - 6.0f,
        bassKnob->getX() - 6.0f,
        ch2DecayKnob->getX() - 6.0f,
        preDelayKnob->getX() - 6.0f
    };
    for (float dx : divXs) {
      g.drawVerticalLine(static_cast<int>(dx), secY + 4.0f, secY + secH - 4.0f);
    }

    // Module Header Titles
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff212529));

    // Section 1: Input & Tank
    g.drawText("INPUT & TANK", plateArea.getX() + 6.0f, plateArea.getY() + 3.0f,
               divXs[0] - plateArea.getX() - 12.0f, 12.0f, juce::Justification::centred, false);

    // Section 2: Channel 1
    g.drawText("CHANNEL 1 (TANK A)", divXs[0] + 6.0f, plateArea.getY() + 3.0f,
               divXs[1] - divXs[0] - 12.0f, 12.0f, juce::Justification::centred, false);

    // Section 3: Baxandall Tone
    g.drawText("BAXANDALL TONE", divXs[1] + 6.0f, plateArea.getY() + 3.0f,
               divXs[2] - divXs[1] - 12.0f, 12.0f, juce::Justification::centred, false);

    // Section 4: Channel 2
    g.drawText("CHANNEL 2 (TANK B)", divXs[2] + 6.0f, plateArea.getY() + 3.0f,
               divXs[3] - divXs[2] - 12.0f, 12.0f, juce::Justification::centred, false);

    // Section 5: Master Output & Blend
    g.drawText("MASTER OUTPUT & BLEND", divXs[3] + 6.0f, plateArea.getY() + 3.0f,
               plateArea.getRight() - divXs[3] - 12.0f, 12.0f, juce::Justification::centred, false);

    // Vintage Badge on bottom right: "AKG WIEN BX 20"
    const float badgeW = 100.0f;
    const float badgeH = 22.0f;
    const float badgeX = plateArea.getRight() - badgeW - 10.0f;
    const float badgeY = plateArea.getBottom() - badgeH - 6.0f;
    auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeW, badgeH);

    g.setColour(juce::Colour(0xff181a1d));
    g.drawRoundedRectangle(badgeRect, 2.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.drawText("AKG WIEN  \xc2\xb7  BX 20", badgeRect, juce::Justification::centred, false);
  }

  void resized() override {
    const int w = getWidth();
    const int h = getHeight();

    const int toolbarH = (h < 380) ? 30 : 40;

    // Toolbar Controls
    presetCombo->setBounds(8, 4, 185, 22);
    prevPresetBtn->setBounds(197, 4, 22, 22);
    nextPresetBtn->setBounds(221, 4, 22, 22);
    compareBtn->setBounds(247, 4, 76, 22);
    copyBtn->setBounds(327, 4, 40, 22);
    pasteBtn->setBounds(371, 4, 40, 22);
    undoBtn->setBounds(415, 4, 40, 22);
    redoBtn->setBounds(459, 4, 40, 22);
    btnPower.setBounds(503, 4, 24, 22);

    // Switches on Toolbar: Input Mode, Direct, Low Cut, Link, Wet Solo
    btnInputStereo.setBounds(535, 4, 82, 22);
    btnDirect.setBounds(621, 4, 58, 22);
    btnLowCut.setBounds(683, 4, 96, 22);
    btnLink.setBounds(783, 4, 82, 22);
    btnWetSolo.setBounds(869, 4, 74, 22);

    // Main Chassis Layout: 5 Modular Sections across width
    auto plateArea = getLocalBounds().reduced(14, 8);
    plateArea.removeFromTop(toolbarH + 4);

    const int startX = plateArea.getX() + 6;
    const int startY = plateArea.getY() + 18;
    const int faceH = plateArea.getHeight() - 22;

    const int knobW = 54;
    const int knobH = 68;
    const int row1Y = startY + 6;
    const int row2Y = startY + (faceH - knobH - 6);

    // Section 1: Input & Tank (StartX, width ~150px)
    inputKnob->setBounds(startX + 14, row1Y, knobW, knobH);
    tankSelectKnob->setBounds(startX + 14, row2Y, knobW + 16, knobH);

    // Section 2: Channel 1 (width ~200px)
    const int sec2X = startX + 160;
    ch1DecayKnob->setBounds(sec2X + 8, row1Y, knobW, knobH);
    ch1VolKnob->setBounds(sec2X + 74, row1Y, knobW, knobH);
    ch1PanKnob->setBounds(sec2X + 41, row2Y, knobW, knobH);

    // Section 3: Baxandall Tone (width ~150px)
    const int sec3X = sec2X + 205;
    bassKnob->setBounds(sec3X + 14, row1Y, knobW, knobH);
    trebleKnob->setBounds(sec3X + 14, row2Y, knobW, knobH);

    // Section 4: Channel 2 (width ~200px)
    const int sec4X = sec3X + 165;
    ch2DecayKnob->setBounds(sec4X + 8, row1Y, knobW, knobH);
    ch2VolKnob->setBounds(sec4X + 74, row1Y, knobW, knobH);
    ch2PanKnob->setBounds(sec4X + 41, row2Y, knobW, knobH);

    // Section 5: Master Blend & Monitor (width ~260px)
    const int sec5X = sec4X + 205;
    preDelayKnob->setBounds(sec5X + 10, row1Y, knobW, knobH);
    dryWetKnob->setBounds(sec5X + 80, row1Y, knobW, knobH);

    jewelLamp->setBounds(sec5X + 160, row1Y + 12, 36, 36);
  }

  void syncFromEngine() {
    inputKnob->setValue(engine.getInputGainDb(), juce::dontSendNotification);
    tankSelectKnob->setValue(engine.getTankSelect(), juce::dontSendNotification);

    ch1DecayKnob->setValue(engine.getCh1Decay(), juce::dontSendNotification);
    ch1VolKnob->setValue(engine.getCh1VolumeDb(), juce::dontSendNotification);
    ch1PanKnob->setValue(engine.getCh1Pan(), juce::dontSendNotification);

    bassKnob->setValue(engine.getBassDb(), juce::dontSendNotification);
    trebleKnob->setValue(engine.getTrebleDb(), juce::dontSendNotification);

    ch2DecayKnob->setValue(engine.getCh2Decay(), juce::dontSendNotification);
    ch2VolKnob->setValue(engine.getCh2VolumeDb(), juce::dontSendNotification);
    ch2PanKnob->setValue(engine.getCh2Pan(), juce::dontSendNotification);

    preDelayKnob->setValue(engine.getPreDelay(), juce::dontSendNotification);
    dryWetKnob->setValue(engine.getMix() * 100.0, juce::dontSendNotification);

    jewelLamp->setLit(engine.getPower());
    updateToggleButtons();
    repaint();
  }

private:
  void setupPresetToolbar() {
    presetCombo = std::make_unique<juce::ComboBox>();
    presetCombo->setColour(juce::ComboBox::backgroundColourId,
                           juce::Colour(0xff16191d));
    presetCombo->setColour(juce::ComboBox::textColourId,
                           juce::Colour(0xfff0f6fc));
    presetCombo->setColour(juce::ComboBox::outlineColourId,
                           juce::Colour(0xff30363d));

    factoryPresets = nova::graviton::VintageSpringReverb::getFactoryPresets();

    juce::PopupMenu menu;
    int itemId = 1;
    juce::String lastCat = "";
    for (const auto &p : factoryPresets) {
      if (juce::String(p.category) != lastCat) {
        lastCat = p.category;
        menu.addSectionHeader(p.category);
      }
      menu.addItem(itemId++, p.name);
    }
    presetCombo->getRootMenu()->clear();
    *presetCombo->getRootMenu() = menu;
    presetCombo->setSelectedId(1, juce::dontSendNotification);

    presetCombo->onChange = [this]() {
      int id = presetCombo->getSelectedId();
      if (id >= 1 && id <= static_cast<int>(factoryPresets.size())) {
        pushUndo();
        engine.applyPreset(factoryPresets[static_cast<size_t>(id - 1)]);
        syncFromEngine();
      }
    };

    prevPresetBtn = std::make_unique<juce::TextButton>("<");
    prevPresetBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    prevPresetBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    prevPresetBtn->onClick = [this]() {
      int cur = presetCombo->getSelectedId();
      if (cur > 1)
        presetCombo->setSelectedId(cur - 1);
    };

    nextPresetBtn = std::make_unique<juce::TextButton>(">");
    nextPresetBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    nextPresetBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    nextPresetBtn->onClick = [this]() {
      int cur = presetCombo->getSelectedId();
      if (cur < static_cast<int>(factoryPresets.size()))
        presetCombo->setSelectedId(cur + 1);
    };

    compareBtn = std::make_unique<juce::TextButton>("Compare");
    compareBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    compareBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    compareBtn->setClickingTogglesState(true);
    compareBtn->onClick = [this]() {
      if (compareBtn->getToggleState()) {
        compareSnapshot = currentSnapshot();
        compareBtn->setButtonText("Comparing (Def)");
        compareBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f6feb));
        engine.applyPreset(factoryPresets[0]);
      } else {
        compareBtn->setButtonText("Compare");
        compareBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        restoreSnapshot(compareSnapshot);
      }
      syncFromEngine();
    };

    copyBtn = std::make_unique<juce::TextButton>("Copy");
    copyBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    copyBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    copyBtn->onClick = [this]() {
      clipboard = currentSnapshot();
      hasClipboard = true;
      copyBtn->setButtonText("Copied!");
      juce::Timer::callAfterDelay(1200, [this] { copyBtn->setButtonText("Copy"); });
    };

    pasteBtn = std::make_unique<juce::TextButton>("Paste");
    pasteBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    pasteBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    pasteBtn->onClick = [this]() {
      if (hasClipboard) {
        pushUndo();
        restoreSnapshot(clipboard);
        syncFromEngine();
        pasteBtn->setButtonText("Pasted!");
        juce::Timer::callAfterDelay(1200, [this] { pasteBtn->setButtonText("Paste"); });
      }
    };

    undoBtn = std::make_unique<juce::TextButton>("Undo");
    undoBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    undoBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    undoBtn->onClick = [this]() {
      if (undoPos > 0) {
        --undoPos;
        restoreSnapshot(undoHistory[static_cast<size_t>(undoPos)]);
        syncFromEngine();
      }
    };

    redoBtn = std::make_unique<juce::TextButton>("Redo");
    redoBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    redoBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    redoBtn->onClick = [this]() {
      if (undoPos + 1 < static_cast<int>(undoHistory.size())) {
        ++undoPos;
        restoreSnapshot(undoHistory[static_cast<size_t>(undoPos)]);
        syncFromEngine();
      }
    };

    addAndMakeVisible(*presetCombo);
    addAndMakeVisible(*prevPresetBtn);
    addAndMakeVisible(*nextPresetBtn);
    addAndMakeVisible(*compareBtn);
    addAndMakeVisible(*copyBtn);
    addAndMakeVisible(*pasteBtn);
    addAndMakeVisible(*undoBtn);
    addAndMakeVisible(*redoBtn);

    pushUndo();
  }

  void setupToggle(juce::TextButton &btn, const juce::String &txt,
                   std::function<void()> onClick) {
    btn.setButtonText(txt);
    btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    btn.onClick = onClick;
    addAndMakeVisible(btn);
  }

  void updateToggleButtons() {
    auto setBtn = [](juce::TextButton &btn, bool active, juce::Colour col,
                     const juce::String &onTxt, const juce::String &offTxt) {
      btn.setColour(juce::TextButton::buttonColourId,
                    active ? col : juce::Colour(0xff21262d));
      btn.setColour(juce::TextButton::textColourOffId,
                    active ? juce::Colour(0xffffffff) : juce::Colour(0xff8b949e));
      btn.setButtonText(active ? onTxt : offTxt);
    };

    setBtn(btnInputStereo, engine.isInputStereo(), juce::Colour(0xff1f6feb),
           "STEREO IN", "MONO IN");
    setBtn(btnDirect, engine.isDirectEnabled(), juce::Colour(0xff238636),
           "DIRECT ON", "DIRECT OFF");
    setBtn(btnLowCut, engine.isLowCut(), juce::Colour(0xff8957e5),
           "LOW CUT ON", "LOW CUT OFF");
    setBtn(btnLink, engine.isLink(), juce::Colour(0xff1f6feb),
           "LINK ON", "LINK OFF");
    setBtn(btnWetSolo, engine.isWetSolo(), juce::Colour(0xffd73a49),
           "WET SOLO", "WET SOLO");

    btnPower.setColour(juce::TextButton::buttonColourId,
                       engine.getPower() ? juce::Colour(0xff1f6feb)
                                        : juce::Colour(0xff21262d));
    btnPower.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
  }

  void bindCallbacks() {
    inputKnob->onValueChanged = [this](double v) { engine.setInputGainDb(v); };
    inputKnob->onDragEnded = [this] { pushUndo(); };

    tankSelectKnob->onValueChanged = [this](double v) {
      engine.setTankSelect(static_cast<int>(std::round(v)));
    };
    tankSelectKnob->onDragEnded = [this] { pushUndo(); };

    ch1DecayKnob->onValueChanged = [this](double v) {
      engine.setCh1Decay(v);
      if (engine.isLink())
        ch2DecayKnob->setValue(v, juce::dontSendNotification);
    };
    ch1DecayKnob->onDragEnded = [this] { pushUndo(); };

    ch1VolKnob->onValueChanged = [this](double v) {
      engine.setCh1VolumeDb(v);
      if (engine.isLink())
        ch2VolKnob->setValue(v, juce::dontSendNotification);
    };
    ch1VolKnob->onDragEnded = [this] { pushUndo(); };

    ch1PanKnob->onValueChanged = [this](double v) {
      engine.setCh1Pan(v);
      if (engine.isLink())
        ch2PanKnob->setValue(-v, juce::dontSendNotification);
    };
    ch1PanKnob->onDragEnded = [this] { pushUndo(); };

    bassKnob->onValueChanged = [this](double v) { engine.setBassDb(v); };
    bassKnob->onDragEnded = [this] { pushUndo(); };

    trebleKnob->onValueChanged = [this](double v) { engine.setTrebleDb(v); };
    trebleKnob->onDragEnded = [this] { pushUndo(); };

    ch2DecayKnob->onValueChanged = [this](double v) {
      engine.setCh2Decay(v);
      if (engine.isLink())
        ch1DecayKnob->setValue(v, juce::dontSendNotification);
    };
    ch2DecayKnob->onDragEnded = [this] { pushUndo(); };

    ch2VolKnob->onValueChanged = [this](double v) {
      engine.setCh2VolumeDb(v);
      if (engine.isLink())
        ch1VolKnob->setValue(v, juce::dontSendNotification);
    };
    ch2VolKnob->onDragEnded = [this] { pushUndo(); };

    ch2PanKnob->onValueChanged = [this](double v) {
      engine.setCh2Pan(v);
      if (engine.isLink())
        ch1PanKnob->setValue(-v, juce::dontSendNotification);
    };
    ch2PanKnob->onDragEnded = [this] { pushUndo(); };

    preDelayKnob->onValueChanged = [this](double v) { engine.setPreDelay(v); };
    preDelayKnob->onDragEnded = [this] { pushUndo(); };

    dryWetKnob->onValueChanged = [this](double v) { engine.setMix(v * 0.01); };
    dryWetKnob->onDragEnded = [this] { pushUndo(); };
  }

  nova::graviton::VintageSpringReverb::Preset currentSnapshot() const {
    nova::graviton::VintageSpringReverb::Preset p;
    p.inputGainDb = engine.getInputGainDb();
    p.inputStereo = engine.isInputStereo();
    p.directEnabled = engine.isDirectEnabled();
    p.tankSelect = engine.getTankSelect();
    p.bassDb = engine.getBassDb();
    p.lowCut = engine.isLowCut();
    p.trebleDb = engine.getTrebleDb();
    p.mix = engine.getMix();
    p.wetSolo = engine.isWetSolo();
    p.preDelayMs = engine.getPreDelay();
    p.ch1DecaySec = engine.getCh1Decay();
    p.ch2DecaySec = engine.getCh2Decay();
    p.ch1VolumeDb = engine.getCh1VolumeDb();
    p.ch2VolumeDb = engine.getCh2VolumeDb();
    p.ch1Pan = engine.getCh1Pan();
    p.ch2Pan = engine.getCh2Pan();
    p.link = engine.isLink();
    return p;
  }

  void restoreSnapshot(const nova::graviton::VintageSpringReverb::Preset &p) {
    engine.applyPreset(p);
  }

  void pushUndo() {
    auto cur = currentSnapshot();
    if (undoPos >= 0 && undoPos < static_cast<int>(undoHistory.size())) {
      undoHistory.erase(undoHistory.begin() + undoPos + 1, undoHistory.end());
    }
    undoHistory.push_back(cur);
    undoPos = static_cast<int>(undoHistory.size()) - 1;
    if (undoHistory.size() > 50) {
      undoHistory.erase(undoHistory.begin());
      --undoPos;
    }
  }

  nova::graviton::VintageSpringReverb &engine;

  // Header Toolbar
  std::unique_ptr<juce::ComboBox> presetCombo;
  std::unique_ptr<juce::TextButton> prevPresetBtn;
  std::unique_ptr<juce::TextButton> nextPresetBtn;
  std::unique_ptr<juce::TextButton> compareBtn;
  std::unique_ptr<juce::TextButton> copyBtn;
  std::unique_ptr<juce::TextButton> pasteBtn;
  std::unique_ptr<juce::TextButton> undoBtn;
  std::unique_ptr<juce::TextButton> redoBtn;

  juce::TextButton btnInputStereo;
  juce::TextButton btnDirect;
  juce::TextButton btnLowCut;
  juce::TextButton btnLink;
  juce::TextButton btnWetSolo;
  juce::TextButton btnPower;

  std::vector<nova::graviton::VintageSpringReverb::Preset> factoryPresets;
  nova::graviton::VintageSpringReverb::Preset compareSnapshot;
  nova::graviton::VintageSpringReverb::Preset clipboard;
  bool hasClipboard = false;
  std::vector<nova::graviton::VintageSpringReverb::Preset> undoHistory;
  int undoPos = -1;

  // Visual Controls
  std::unique_ptr<BX20Knob> inputKnob;
  std::unique_ptr<BX20Knob> tankSelectKnob;

  std::unique_ptr<BX20Knob> ch1DecayKnob;
  std::unique_ptr<BX20Knob> ch1VolKnob;
  std::unique_ptr<BX20Knob> ch1PanKnob;

  std::unique_ptr<BX20Knob> bassKnob;
  std::unique_ptr<BX20Knob> trebleKnob;

  std::unique_ptr<BX20Knob> ch2DecayKnob;
  std::unique_ptr<BX20Knob> ch2VolKnob;
  std::unique_ptr<BX20Knob> ch2PanKnob;

  std::unique_ptr<BX20Knob> preDelayKnob;
  std::unique_ptr<BX20Knob> dryWetKnob;
  std::unique_ptr<BX20JewelLamp> jewelLamp;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AKGBX20Panel)
};

} // namespace nova
