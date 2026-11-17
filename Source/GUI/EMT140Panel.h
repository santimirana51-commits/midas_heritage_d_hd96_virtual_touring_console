#pragma once

#include "Effects/GravitonPlateReverb.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <juce_gui_basics/juce_gui_basics.h>
#include <sstream>
#include <string>
#include <vector>

namespace nova {

// ==============================================================================
// 1. Vintage Fluted Bakelite Knob with Spun Aluminum Cap
// ==============================================================================
class VintageKnob final : public juce::Component {
public:
  VintageKnob(const juce::String &paramTitle, double minV, double maxV,
              double defaultV, const juce::String &unitSuffix,
              int numDecimals = 1, bool isLog = false)
      : title(paramTitle), minValue(minV), maxValue(maxV),
        defaultValue(defaultV), currentValue(defaultV), unit(unitSuffix),
        decimals(numDecimals), logarithmic(isLog) {
    setRepaintsOnMouseActivity(true);
  }

  void setValue(double val,
                juce::NotificationType notify = juce::dontSendNotification) {
    const double clamped = std::clamp(val, minValue, maxValue);
    if (std::abs(clamped - currentValue) > 1e-5) {
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
        std::clamp(std::min(bounds.getWidth(), bounds.getHeight()) - 2.0f, 18.0f, 44.0f);
    const auto knobArea = bounds.withSizeKeepingCentre(knobSize, knobSize);
    const float cx = knobArea.getCentreX();
    const float cy = knobArea.getCentreY();
    const float radius = knobSize * 0.5f;

    // Drop shadow behind knob
    g.setColour(juce::Colour(0x30000000));
    g.fillEllipse(knobArea.translated(1.0f, 1.5f));

    // Outer fluted skirt (Dark charcoal bakelite)
    juce::ColourGradient skirtGrad(juce::Colour(0xff2b2e33), cx - radius, cy - radius,
                                   juce::Colour(0xff121417), cx + radius, cy + radius, false);
    g.setGradientFill(skirtGrad);
    g.fillEllipse(knobArea);
    g.setColour(juce::Colour(0xff0f1113));
    g.drawEllipse(knobArea, 1.0f);

    // Fluted ridges on outer ring
    const int numRidges = 16;
    g.setColour(juce::Colour(0x35ffffff));
    for (int i = 0; i < numRidges; ++i) {
      const float angle = i * (juce::MathConstants<float>::twoPi / numRidges);
      const float rx1 = cx + (radius - 1.0f) * std::cos(angle);
      const float ry1 = cy + (radius - 1.0f) * std::sin(angle);
      const float rx2 = cx + (radius - 3.5f) * std::cos(angle);
      const float ry2 = cy + (radius - 3.5f) * std::sin(angle);
      g.drawLine(rx1, ry1, rx2, ry2, 0.9f);
    }

    // Inner spun aluminum cap
    const float capRadius = radius * 0.62f;
    auto capArea = juce::Rectangle<float>(cx - capRadius, cy - capRadius,
                                          capRadius * 2.0f, capRadius * 2.0f);
    juce::ColourGradient aluGrad(juce::Colour(0xfff0f2f5), cx - capRadius * 0.7f,
                                 cy - capRadius * 0.7f, juce::Colour(0xff8c929a),
                                 cx + capRadius, cy + capRadius, false);
    g.setGradientFill(aluGrad);
    g.fillEllipse(capArea);
    g.setColour(juce::Colour(0xff495057));
    g.drawEllipse(capArea, 0.8f);

    // Normalized value and rotation angle
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
    const float currentAngle = startAngle + norm * (endAngle - startAngle);

    // Indicator line across the knob
    const float px = cx + (radius - 1.5f) * std::cos(currentAngle);
    const float py = cy + (radius - 1.5f) * std::sin(currentAngle);
    const float pxInner = cx + (capRadius * 0.25f) * std::cos(currentAngle);
    const float pyInner = cy + (capRadius * 0.25f) * std::sin(currentAngle);

    g.setColour(juce::Colour(0xffffffff));
    g.drawLine(pxInner, pyInner, px, py, 1.8f);

    // Value text
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff2d3136));
    juce::String valStr;
    if (decimals == 0)
      valStr = juce::String(static_cast<int>(std::round(currentValue)));
    else
      valStr = juce::String(currentValue, decimals);
    if (unit.isNotEmpty())
      valStr << " " << unit;
    g.drawText(valStr, valArea, juce::Justification::centred, false);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    mouseStartVal = currentValue;
    dragStartY = e.position.y;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float deltaY = dragStartY - e.position.y;
    const double range = maxValue - minValue;
    const double step = range * (e.mods.isShiftDown() ? 0.001 : 0.006);
    setValue(mouseStartVal + deltaY * step, juce::sendNotification);
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
  int decimals;
  bool logarithmic;
  double mouseStartVal = 0.0;
  float dragStartY = 0.0f;
};

// ==============================================================================
// 2. Mechanical Vertical Slotted Slide Filter (Input Filter)
// Detent positions: OFF, 90, 125, 180, 250, 270, 360 Hz
// ==============================================================================
class InputFilterSlider final : public juce::Component {
public:
  InputFilterSlider() { setRepaintsOnMouseActivity(true); }

  void setFilterIndex(int idx,
                      juce::NotificationType notify = juce::dontSendNotification) {
    const int clamped = std::clamp(idx, 0, 6);
    if (clamped != currentIdx) {
      currentIdx = clamped;
      repaint();
      if (notify != juce::dontSendNotification && onIndexChanged) {
        onIndexChanged(currentIdx);
      }
    }
  }

  int getFilterIndex() const noexcept { return currentIdx; }

  std::function<void(int)> onIndexChanged;
  std::function<void()> onDragEnded;

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Section Header
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff181a1d));
    g.drawText("INPUT FILTER", bounds.removeFromTop(14.0f),
               juce::Justification::centred, false);

    // Lower fuse label
    auto fuseArea = bounds.removeFromBottom(22.0f);
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff495057));
    g.drawText("SLOW BLOW", fuseArea.removeFromTop(10.0f),
               juce::Justification::centred, false);
    g.setFont(juce::Font(juce::FontOptions(6.8f)));
    g.setColour(juce::Colour(0xff6c757d));
    g.drawText("100-260V ~ 0.5A", fuseArea, juce::Justification::centred, false);

    const float trackLeft = bounds.getX() + 32.0f;
    const float trackWidth = 7.0f;
    const float trackTop = bounds.getY() + 6.0f;
    const float trackHeight = bounds.getHeight() - 12.0f;

    // Slotted dark metal groove
    auto grooveRect =
        juce::Rectangle<float>(trackLeft, trackTop, trackWidth, trackHeight);
    g.setColour(juce::Colour(0xff16181b));
    g.fillRoundedRectangle(grooveRect, 2.5f);
    g.setColour(juce::Colour(0xff343a40));
    g.drawRoundedRectangle(grooveRect, 2.5f, 1.0f);

    // Tick marks and frequency labels
    static const char *labels[] = {"OFF",    "90 Hz",  "125 Hz", "180 Hz",
                                   "250 Hz", "270 Hz", "360 Hz"};
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));

    for (int i = 0; i <= 6; ++i) {
      const float norm = i / 6.0f;
      const float y = trackTop + (1.0f - norm) * trackHeight;

      // Tick line
      g.setColour(juce::Colour(0xff555d67));
      g.drawHorizontalLine(static_cast<int>(y), trackLeft - 4.0f, trackLeft);

      // Label on left
      g.setColour(currentIdx == i ? juce::Colour(0xff0d1117)
                                  : juce::Colour(0xff6c757d));
      g.drawText(
          labels[i],
          juce::Rectangle<float>(1.0f, y - 5.0f, trackLeft - 6.0f, 10.0f),
          juce::Justification::centredRight, false);
    }

    // Slope label on right
    g.setFont(juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff8c2025));
    g.drawText("12 dB/oct",
               juce::Rectangle<float>(trackLeft + trackWidth + 4.0f,
                                      trackTop + trackHeight * 0.45f, 38.0f, 10.0f),
               juce::Justification::centredLeft, false);

    // Metal slider thumb handle
    const float thumbNorm = currentIdx / 6.0f;
    const float thumbY = trackTop + (1.0f - thumbNorm) * trackHeight;
    auto thumbRect = juce::Rectangle<float>(trackLeft - 8.0f, thumbY - 6.0f,
                                            trackWidth + 16.0f, 12.0f);

    // Metallic handle with bevel and white center line
    g.setColour(juce::Colour(0x35000000));
    g.fillRoundedRectangle(thumbRect.translated(1.0f, 1.5f), 2.0f);
    juce::ColourGradient handleGrad(juce::Colour(0xffe9ecef), thumbRect.getX(),
                                    thumbRect.getY(), juce::Colour(0xffadb5bd),
                                    thumbRect.getX(), thumbRect.getBottom(), false);
    g.setGradientFill(handleGrad);
    g.fillRoundedRectangle(thumbRect, 2.0f);
    g.setColour(juce::Colour(0xff343a40));
    g.drawRoundedRectangle(thumbRect, 2.0f, 0.8f);

    // Center white indicator line
    g.setColour(juce::Colour(0xffffffff));
    g.drawHorizontalLine(static_cast<int>(thumbY), thumbRect.getX() + 2.0f,
                          thumbRect.getRight() - 2.0f);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    updateFromMouse(e.position.y);
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    updateFromMouse(e.position.y);
  }

  void mouseUp(const juce::MouseEvent &) override {
    if (onDragEnded)
      onDragEnded();
  }

private:
  void updateFromMouse(float mouseY) {
    const float trackTop = 20.0f;
    const float trackHeight = getHeight() - 48.0f;
    if (trackHeight <= 0.0f)
      return;
    const float norm =
        1.0f - std::clamp((mouseY - trackTop) / trackHeight, 0.0f, 1.0f);
    const int idx = static_cast<int>(std::round(norm * 6.0f));
    setFilterIndex(idx, juce::sendNotification);
  }

  int currentIdx = 0;
};

// ==============================================================================
// 3. Vintage Plate Selector Rotary Switch (Chicken-Head Pointer)
// Switches between Plate A, Plate B, and Plate C
// ==============================================================================
class PlateSelectorSwitch final : public juce::Component {
public:
  PlateSelectorSwitch() { setRepaintsOnMouseActivity(true); }

  void setSelectedPlate(int plate,
                        juce::NotificationType notify = juce::dontSendNotification) {
    const int clamped = std::clamp(plate, 0, 2);
    if (clamped != currentPlate) {
      currentPlate = clamped;
      repaint();
      if (notify != juce::dontSendNotification && onPlateSelected) {
        onPlateSelected(currentPlate);
      }
    }
  }

  int getSelectedPlate() const noexcept { return currentPlate; }

  std::function<void(int)> onPlateSelected;

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Section Header
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff181a1d));
    g.drawText("PLATE", bounds.removeFromTop(14.0f), juce::Justification::centred,
               false);

    // Subtitle indicating active model
    auto subArea = bounds.removeFromBottom(22.0f);
    static const char *subLabels[] = {"1957 TUBE", "1965 S-STATE", "1970 MODERN"};
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff0d1117));
    g.drawText(subLabels[currentPlate], subArea.removeFromTop(10.0f),
               juce::Justification::centred, false);
    g.setFont(juce::Font(juce::FontOptions(6.8f)));
    g.setColour(juce::Colour(0xff6c757d));
    g.drawText("STEEL TANK", subArea, juce::Justification::centred, false);

    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY() + 2.0f;
    const float radius = 20.0f;

    // Position angles for A, B, C: -35 deg, 0 deg, +35 deg from vertical top
    static const float anglesDeg[3] = {-35.0f, 0.0f, 35.0f};
    static const char *labels[3] = {"A", "B", "C"};

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    for (int i = 0; i < 3; ++i) {
      const float rad =
          (anglesDeg[i] - 90.0f) * (juce::MathConstants<float>::pi / 180.0f);
      const float lx = cx + 30.0f * std::cos(rad);
      const float ly = cy + 30.0f * std::sin(rad);

      g.setColour(currentPlate == i ? juce::Colour(0xff0d1117)
                                    : juce::Colour(0xff6c757d));
      g.drawText(labels[i],
                 juce::Rectangle<float>(lx - 9.0f, ly - 7.0f, 18.0f, 14.0f),
                 juce::Justification::centred, false);

      // Detent dot
      const float dx = cx + 22.0f * std::cos(rad);
      const float dy = cy + 22.0f * std::sin(rad);
      g.setColour(juce::Colour(0xff343a40));
      g.fillEllipse(dx - 1.2f, dy - 1.2f, 2.4f, 2.4f);
    }

    // Circular knob base shadow & body
    g.setColour(juce::Colour(0x30000000));
    g.fillEllipse(cx - radius + 1.0f, cy - radius + 1.5f, radius * 2.0f,
                  radius * 2.0f);

    g.setColour(juce::Colour(0xff181a1d));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff2d3238));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // Chicken-head elongated pointer
    const float pointerAngle = (anglesDeg[currentPlate] - 90.0f) *
                               (juce::MathConstants<float>::pi / 180.0f);
    const float pointerLen = 22.0f;

    juce::Path pointer;
    pointer.startNewSubPath(cx + pointerLen * std::cos(pointerAngle),
                            cy + pointerLen * std::sin(pointerAngle));
    const float sideAngle1 = pointerAngle + juce::MathConstants<float>::halfPi;
    const float sideAngle2 = pointerAngle - juce::MathConstants<float>::halfPi;
    pointer.lineTo(cx + 5.0f * std::cos(sideAngle1),
                   cy + 5.0f * std::sin(sideAngle1));
    pointer.lineTo(cx - 6.0f * std::cos(pointerAngle),
                   cy - 6.0f * std::sin(pointerAngle));
    pointer.lineTo(cx + 5.0f * std::cos(sideAngle2),
                   cy + 5.0f * std::sin(sideAngle2));
    pointer.closeSubPath();

    g.setColour(juce::Colour(0xff141618));
    g.fillPath(pointer);
    g.setColour(juce::Colour(0xff3c424a));
    g.strokePath(pointer, juce::PathStrokeType(0.8f));

    // Center ridge white stripe
    const float tipX = cx + (pointerLen - 1.5f) * std::cos(pointerAngle);
    const float tipY = cy + (pointerLen - 1.5f) * std::sin(pointerAngle);
    g.setColour(juce::Colour(0xffffffff));
    g.drawLine(cx, cy, tipX, tipY, 1.6f);

    // Center chrome screw
    g.setColour(juce::Colour(0xffadb5bd));
    g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff343a40));
    g.drawEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f, 0.6f);
  }

  void mouseDown(const juce::MouseEvent &) override {
    currentPlate = (currentPlate + 1) % 3;
    setSelectedPlate(currentPlate, juce::sendNotification);
  }

private:
  int currentPlate = 0;
};

// ==============================================================================
// 4. Mechanical Damper Gauge Display Window (Plate A, B, C)
// Features glass meter scale (max, 5..1, min Sek) and ergonomic +/- buttons
// ==============================================================================
class PlateDamperModule final : public juce::Component {
public:
  PlateDamperModule(const juce::String &plateLabel, double initialDecay)
      : label(plateLabel), decaySec(initialDecay) {
    setRepaintsOnMouseActivity(true);
  }

  void setDecay(double sec,
                juce::NotificationType notify = juce::dontSendNotification) {
    const double clamped = std::clamp(sec, 0.5, 5.5);
    if (std::abs(clamped - decaySec) > 1e-4) {
      decaySec = clamped;
      repaint();
      if (notify != juce::dontSendNotification && onDecayChanged) {
        onDecayChanged(decaySec);
      }
    }
  }

  double getDecay() const noexcept { return decaySec; }

  void setActive(bool active) {
    if (isActive != active) {
      isActive = active;
      repaint();
    }
  }

  std::function<void(double)> onDecayChanged;
  std::function<void()> onDragEnded;

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Plate letter header (A, B, or C)
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(isActive ? juce::Colour(0xff0d1117) : juce::Colour(0xff6c757d));
    g.drawText(label, bounds.removeFromTop(14.0f), juce::Justification::centred,
               false);

    // Bottom Controls: [-] [3.5s] [+]
    auto buttonArea = bounds.removeFromBottom(28.0f);

    // Glass Meter Scale Display Window
    auto meterRect = bounds.reduced(4.0f, 2.0f);

    // Outer recessed metal bezel
    g.setColour(juce::Colour(0xff212529));
    g.fillRoundedRectangle(meterRect, 3.0f);
    g.setColour(juce::Colour(0xff343a40));
    g.drawRoundedRectangle(meterRect, 3.0f, 1.0f);

    // Inner glowing glass background
    auto innerGlass = meterRect.reduced(2.5f);
    juce::Colour glassBg =
        isActive ? juce::Colour(0xfffff3cd) : juce::Colour(0xffe9ecef);
    g.setColour(glassBg);
    g.fillRoundedRectangle(innerGlass, 2.0f);

    // Subtle vintage glass reflection gradient
    juce::ColourGradient glassGrad(juce::Colour(0x25ffffff), innerGlass.getX(),
                                   innerGlass.getY(), juce::Colour(0x10000000),
                                   innerGlass.getX(), innerGlass.getBottom(),
                                   false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(innerGlass, 2.0f);

    // Scale markings: max, 5, 4, 3, 2, 1, min Sek
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff343a40));

    const float scaleTop = innerGlass.getY() + 10.0f;
    const float scaleHeight = innerGlass.getHeight() - 20.0f;

    g.drawText("max",
               juce::Rectangle<float>(innerGlass.getX() + 3.0f,
                                      innerGlass.getY() + 1.0f, 18.0f, 9.0f),
               juce::Justification::centredLeft, false);
    g.drawText("min",
               juce::Rectangle<float>(innerGlass.getX() + 3.0f,
                                      innerGlass.getBottom() - 10.0f, 18.0f, 9.0f),
               juce::Justification::centredLeft, false);
    g.drawText("Sek",
               juce::Rectangle<float>(innerGlass.getX() + 3.0f,
                                      innerGlass.getCentreY() - 5.0f, 18.0f, 10.0f),
               juce::Justification::centredLeft, false);

    for (int s = 5; s >= 1; --s) {
      const float norm = (s - 1.0f) / 4.0f; // 1 to 5
      const float y = scaleTop + (1.0f - norm) * scaleHeight;

      // Tick lines
      g.setColour(juce::Colour(0xff495057));
      g.drawHorizontalLine(static_cast<int>(y), innerGlass.getRight() - 10.0f,
                           innerGlass.getRight() - 3.0f);
      g.drawText(juce::String(s),
                 juce::Rectangle<float>(innerGlass.getRight() - 20.0f, y - 5.0f,
                                        9.0f, 10.0f),
                 juce::Justification::centredRight, false);
    }

    // Mechanical Damper Ribbon / Needle
    const float decayNorm =
        std::clamp(static_cast<float>((decaySec - 0.5) / 5.0), 0.0f, 1.0f);
    const float needleY = scaleTop + (1.0f - decayNorm) * scaleHeight;

    // Red / Amber Damper Needle Bar
    g.setColour(juce::Colour(0xffd73a49));
    g.fillRect(innerGlass.getX() + 1.5f, needleY - 1.2f,
               innerGlass.getWidth() - 3.0f, 2.5f);
    g.setColour(juce::Colour(0xffffffff));
    g.drawHorizontalLine(static_cast<int>(needleY), innerGlass.getX() + 3.0f,
                         innerGlass.getRight() - 3.0f);

    // Bottom row: [-] button, value text, [+] button
    const float btnSize = 18.0f;
    const float btnY = buttonArea.getY() + 5.0f;
    const float btnLeftX = buttonArea.getX() + 4.0f;
    const float btnRightX = buttonArea.getRight() - 4.0f - btnSize;

    auto drawTealBtn = [&g](float x, float y, float sz, const juce::String &sym,
                            bool pressed) {
      auto bRect = juce::Rectangle<float>(x, y, sz, sz);
      g.setColour(juce::Colour(0x30000000));
      g.fillRoundedRectangle(bRect.translated(0.5f, 1.0f), 3.0f);

      juce::Colour tealCol =
          pressed ? juce::Colour(0xff00897b) : juce::Colour(0xff00b4d8);
      g.setColour(tealCol);
      g.fillRoundedRectangle(bRect, 3.0f);
      g.setColour(juce::Colour(0xff0077b6));
      g.drawRoundedRectangle(bRect, 3.0f, 0.8f);

      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(juce::Colour(0xffffffff));
      g.drawText(sym, bRect, juce::Justification::centred, false);
    };

    drawTealBtn(btnLeftX, btnY, btnSize, "-", minusPressed);
    drawTealBtn(btnRightX, btnY, btnSize, "+", plusPressed);

    // Value text in center
    auto valBox = juce::Rectangle<float>(btnLeftX + btnSize, btnY - 2.0f,
                                         btnRightX - (btnLeftX + btnSize), btnSize + 4.0f);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff212529));
    g.drawText(juce::String(decaySec, 1) + " s", valBox,
               juce::Justification::centred, false);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const float btnSize = 18.0f;
    const float btnY = getHeight() - 28.0f + 5.0f;
    const float btnLeftX = 4.0f;
    const float btnRightX = getWidth() - 4.0f - btnSize;

    auto btnMinus = juce::Rectangle<float>(btnLeftX, btnY, btnSize, btnSize);
    auto btnPlus = juce::Rectangle<float>(btnRightX, btnY, btnSize, btnSize);

    if (btnMinus.contains(e.position)) {
      minusPressed = true;
      setDecay(decaySec - 0.2, juce::sendNotification);
      repaint();
      return;
    }
    if (btnPlus.contains(e.position)) {
      plusPressed = true;
      setDecay(decaySec + 0.2, juce::sendNotification);
      repaint();
      return;
    }

    // Direct clicking/dragging on meter scale
    updateFromMeter(e.position.y);
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (!minusPressed && !plusPressed) {
      updateFromMeter(e.position.y);
    }
  }

  void mouseUp(const juce::MouseEvent &) override {
    minusPressed = false;
    plusPressed = false;
    repaint();
    if (onDragEnded)
      onDragEnded();
  }

private:
  void updateFromMeter(float mouseY) {
    const float scaleTop = 18.0f;
    const float scaleHeight = getHeight() - 56.0f;
    if (scaleHeight <= 0.0f)
      return;
    const float norm =
        1.0f - std::clamp((mouseY - scaleTop) / scaleHeight, 0.0f, 1.0f);
    setDecay(0.5 + norm * 5.0, juce::sendNotification);
  }

  juce::String label;
  double decaySec = 3.5;
  bool isActive = false;
  bool minusPressed = false;
  bool plusPressed = false;
};

// ==============================================================================
// 5. Authentic Vintage Analog Stereo VU Meter with Dual Needles (L & R)
// ==============================================================================
class AnalogVUMeter final : public juce::Component {
public:
  AnalogVUMeter() { setOpaque(false); }

  void setLevels(float targetL, float targetR) {
    needleValueL = needleValueL * 0.85f + targetL * 0.15f;
    needleValueR = needleValueR * 0.85f + targetR * 0.15f;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Outer molded black phenolic bezel frame
    g.setColour(juce::Colour(0xff16181b));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour(0xff2d3238));
    g.drawRoundedRectangle(bounds, 5.0f, 1.2f);

    // Corner mounting screws
    auto drawScrew = [&g](float x, float y) {
      g.setColour(juce::Colour(0xff6c757d));
      g.fillEllipse(x - 2.5f, y - 2.5f, 5.0f, 5.0f);
      g.setColour(juce::Colour(0xff1a1d20));
      g.drawLine(x - 1.5f, y, x + 1.5f, y, 0.7f);
    };
    drawScrew(bounds.getX() + 6.0f, bounds.getY() + 6.0f);
    drawScrew(bounds.getRight() - 6.0f, bounds.getY() + 6.0f);
    drawScrew(bounds.getX() + 6.0f, bounds.getBottom() - 6.0f);
    drawScrew(bounds.getRight() - 6.0f, bounds.getBottom() - 6.0f);

    // Inner warm incandescent backlight cream meter dial face
    auto meterFace = bounds.reduced(7.0f);
    juce::ColourGradient bgGrad(
        juce::Colour(0xfffffdf0), meterFace.getCentreX(), meterFace.getY(),
        juce::Colour(0xfffaecc5), meterFace.getCentreX(), meterFace.getBottom(),
        false);
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(meterFace, 3.5f);
    g.setColour(juce::Colour(0xff495057));
    g.drawRoundedRectangle(meterFace, 3.5f, 0.9f);

    // Meter geometry
    const float pivotX = meterFace.getCentreX();
    const float pivotY = meterFace.getBottom() + meterFace.getHeight() * 0.12f;
    const float arcRadius = meterFace.getHeight() * 0.90f;

    // Angle range: -45 deg to +30 deg
    const float startAngle = -45.0f * (juce::MathConstants<float>::pi / 180.0f);
    const float zeroAngle = 10.0f * (juce::MathConstants<float>::pi / 180.0f);
    const float endAngle = 30.0f * (juce::MathConstants<float>::pi / 180.0f);

    // Black Normal Zone Arc (-20 to 0 VU)
    juce::Path normalArc;
    normalArc.addCentredArc(pivotX, pivotY, arcRadius, arcRadius, 0.0f,
                            startAngle - juce::MathConstants<float>::halfPi,
                            zeroAngle - juce::MathConstants<float>::halfPi, true);
    g.setColour(juce::Colour(0xff212529));
    g.strokePath(normalArc, juce::PathStrokeType(1.5f));

    // Red Overload Zone Arc (0 to +3 VU)
    juce::Path overloadArc;
    overloadArc.addCentredArc(pivotX, pivotY, arcRadius, arcRadius, 0.0f,
                              zeroAngle - juce::MathConstants<float>::halfPi,
                              endAngle - juce::MathConstants<float>::halfPi, true);
    g.setColour(juce::Colour(0xffd73a49));
    g.strokePath(overloadArc, juce::PathStrokeType(2.4f));

    // Scale tick markings & labels
    struct TickMark {
      float vu;
      const char *text;
    };
    static const TickMark ticks[] = {
        {-20.0f, "-20"}, {-10.0f, "-10"}, {-7.0f, "-7"}, {-5.0f, "-5"},
        {-3.0f, "-3"},   {-2.0f, "-2"},   {-1.0f, "-1"}, {0.0f, "0"},
        {1.0f, "+1"},    {2.0f, "+2"},    {3.0f, "+3"}};

    g.setFont(juce::Font(juce::FontOptions(7.2f, juce::Font::bold)));

    for (const auto &tm : ticks) {
      float norm = 0.0f;
      if (tm.vu <= 0.0f) {
        norm = (tm.vu + 20.0f) / 20.0f * 0.72f;
      } else {
        norm = 0.72f + (tm.vu / 3.0f) * 0.28f;
      }
      const float a = startAngle + norm * (endAngle - startAngle) -
                      juce::MathConstants<float>::halfPi;

      const float r1 = arcRadius - 2.0f;
      const float r2 = arcRadius + 4.0f;
      const float rText = arcRadius + 10.0f;

      const bool isRed = (tm.vu >= 0.0f);
      g.setColour(isRed ? juce::Colour(0xffd73a49) : juce::Colour(0xff212529));
      g.drawLine(pivotX + r1 * std::cos(a), pivotY + r1 * std::sin(a),
                 pivotX + r2 * std::cos(a), pivotY + r2 * std::sin(a), 1.0f);

      g.drawText(tm.text,
                 juce::Rectangle<float>(pivotX + rText * std::cos(a) - 8.0f,
                                        pivotY + rText * std::sin(a) - 5.0f,
                                        16.0f, 9.0f),
                 juce::Justification::centred, false);
    }

    // Label "VU"
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff343a40));
    g.drawText("VU",
               juce::Rectangle<float>(pivotX - 18.0f, meterFace.getY() + 28.0f,
                                      36.0f, 13.0f),
               juce::Justification::centred, false);

    // Label "EMT" badge in dial center
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff6c757d));
    g.drawText("ELEKTROMESSTECHNIK",
               juce::Rectangle<float>(pivotX - 60.0f, meterFace.getY() + 41.0f,
                                      120.0f, 10.0f),
               juce::Justification::centred, false);

    // Legend L / R
    g.setFont(juce::Font(juce::FontOptions(7.2f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffd73a49));
    g.drawText("L", juce::Rectangle<float>(meterFace.getX() + 8.0f, meterFace.getBottom() - 20.0f, 12.0f, 12.0f), juce::Justification::centred, false);
    g.setColour(juce::Colour(0xffe65100));
    g.drawText("R", juce::Rectangle<float>(meterFace.getRight() - 20.0f, meterFace.getBottom() - 20.0f, 12.0f, 12.0f), juce::Justification::centred, false);

    // Moving Needles: Needle L (Red) & Needle R (Amber/Orange)
    auto drawNeedle = [&](float needleVal, juce::Colour col, float strokeW) {
      const float targetNorm = std::clamp(needleVal * 1.25f, 0.0f, 1.0f);
      const float needleAngle = startAngle + targetNorm * (endAngle - startAngle) -
                                juce::MathConstants<float>::halfPi;

      // Shadow
      g.setColour(juce::Colour(0x25000000));
      g.drawLine(pivotX + 1.0f, pivotY + 1.5f,
                 pivotX + (arcRadius + 3.0f) * std::cos(needleAngle) + 1.0f,
                 pivotY + (arcRadius + 3.0f) * std::sin(needleAngle) + 1.5f,
                 strokeW);

      // Needle line
      g.setColour(col);
      g.drawLine(pivotX, pivotY,
                 pivotX + (arcRadius + 3.0f) * std::cos(needleAngle),
                 pivotY + (arcRadius + 3.0f) * std::sin(needleAngle), strokeW);
    };

    drawNeedle(needleValueR, juce::Colour(0xffe65100), 1.2f); // Right channel (amber)
    drawNeedle(needleValueL, juce::Colour(0xffd73a49), 1.4f); // Left channel (red)

    // Bottom Pivot Housing Bezel
    g.setColour(juce::Colour(0xff181a1d));
    g.fillEllipse(pivotX - 11.0f, pivotY - 11.0f, 22.0f, 22.0f);
    g.setColour(juce::Colour(0xffadb5bd));
    g.fillEllipse(pivotX - 4.5f, pivotY - 4.5f, 9.0f, 9.0f);
  }

private:
  float needleValueL = 0.0f;
  float needleValueR = 0.0f;
};

// ==============================================================================
// 6. Vintage Metal Bat Toggle Switch (OUT / IN, OFF / ON)
// ==============================================================================
class VintageToggleSwitch final : public juce::Component {
public:
  VintageToggleSwitch(const juce::String &switchLabel,
                      const juce::String &offTxt, const juce::String &onTxt)
      : label(switchLabel), offText(offTxt), onText(onTxt) {
    setRepaintsOnMouseActivity(true);
  }

  void setToggleState(bool st,
                      juce::NotificationType notify = juce::dontSendNotification) {
    if (state != st) {
      state = st;
      repaint();
      if (notify != juce::dontSendNotification && onStateChanged) {
        onStateChanged(state);
      }
    }
  }

  bool getToggleState() const noexcept { return state; }

  std::function<void(bool)> onStateChanged;

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    if (label.isNotEmpty()) {
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(juce::Colour(0xff212529));
      g.drawText(label, bounds.removeFromTop(12.0f),
                 juce::Justification::centred, false);
    }

    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();

    // Labels on left and right
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(!state ? juce::Colour(0xff0d1117) : juce::Colour(0xff6c757d));
    g.drawText(offText,
               juce::Rectangle<float>(1.0f, cy - 7.0f, cx - 11.0f, 14.0f),
               juce::Justification::centredRight, false);

    g.setColour(state ? juce::Colour(0xff0d1117) : juce::Colour(0xff6c757d));
    g.drawText(onText,
               juce::Rectangle<float>(cx + 11.0f, cy - 7.0f,
                                      bounds.getRight() - cx - 12.0f, 14.0f),
               juce::Justification::centredLeft, false);

    // Circular threaded metal bushing
    g.setColour(juce::Colour(0xff343a40));
    g.fillEllipse(cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour(juce::Colour(0xff8c959f));
    g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);

    // Chrome toggle bat lever
    const float leverLength = 10.0f;
    const float angle = state ? 0.42f : -0.42f;
    const float tipX = cx + leverLength * std::sin(angle);
    const float tipY = cy - leverLength * std::cos(angle);

    g.setColour(juce::Colour(0x35000000));
    g.drawLine(cx + 0.8f, cy + 0.8f, tipX + 0.8f, tipY + 0.8f, 2.8f);
    g.setColour(juce::Colour(0xffd0d7de));
    g.drawLine(cx, cy, tipX, tipY, 2.8f);
    g.setColour(juce::Colour(0xffffffff));
    g.fillEllipse(tipX - 2.0f, tipY - 2.0f, 4.0f, 4.0f);
  }

  void mouseDown(const juce::MouseEvent &) override {
    setToggleState(!state, juce::sendNotification);
  }

private:
  juce::String label, offText, onText;
  bool state = false;
};

// ==============================================================================
// 7. Master EMT 140 Hardware Reverb Dialog Panel (UAD 1:1 Architecture)
// ==============================================================================
class EMT140Panel final : public juce::Component, private juce::Timer {
public:
  explicit EMT140Panel(nova::graviton::EMT140PlateReverb &dspRef)
      : plate(dspRef),
        knobLoFreq("LO FREQ", 20.0, 2000.0, 100.0, "Hz", 0, true),
        knobLoGain("GAIN", -12.0, 12.0, 0.0, "dB", 1),
        knobHiFreq("HI FREQ", 200.0, 20000.0, 8000.0, "Hz", 0, true),
        knobHiGain("GAIN", -12.0, 12.0, 0.0, "dB", 1),
        knobModRate("RATE", 0.01, 1.0, 0.10, "Hz", 2),
        knobModDepth("DEPTH", 0.0, 10.0, 5.0, "", 1),
        knobWidth("WIDTH", 0.0, 200.0, 100.0, "%", 0),
        knobBalance("BALANCE", -1.0, 1.0, 0.0, "", 2),
        knobPreDelay("PRE DELAY", 0.0, 250.0, 20.0, "ms", 0),
        knobMix("MIX", 0.0, 100.0, 35.0, "%", 0),
        eqSwitch("EQ", "OUT", "IN"),
        powerSwitch("", "OFF", "ON"),
        damperA("A", dspRef.getPlateDecay(0)),
        damperB("B", dspRef.getPlateDecay(1)),
        damperC("C", dspRef.getPlateDecay(2)) {

    setSize(1020, 520);

    // Toolbar Preset Management Buttons
    presetBtn.setButtonText(plate.getFactoryPresets()[0].name + juce::String(" \xe2\x96\xbe"));
    presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff16191d));
    presetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    presetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetBtn);

    prevPresetBtn.setButtonText("<");
    prevPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    prevPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    prevPresetBtn.onClick = [this] {
      const auto &factory = plate.getFactoryPresets();
      const int numPresets = static_cast<int>(factory.size());
      currentPresetIdx = (currentPresetIdx - 1 + numPresets) % numPresets;
      applyPreset(currentPresetIdx);
    };
    addAndMakeVisible(prevPresetBtn);

    nextPresetBtn.setButtonText(">");
    nextPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    nextPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    nextPresetBtn.onClick = [this] {
      const auto &factory = plate.getFactoryPresets();
      const int numPresets = static_cast<int>(factory.size());
      currentPresetIdx = (currentPresetIdx + 1) % numPresets;
      applyPreset(currentPresetIdx);
    };
    addAndMakeVisible(nextPresetBtn);

    compareBtn.setButtonText("Compare");
    compareBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    compareBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    compareBtn.onClick = [this] {
      if (!compareActive) {
        stateA = plate.getCurrentPresetState("State A (User)");
        compareActive = true;
        compareBtn.setButtonText("Comparing (B)");
        compareBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f6feb));
        plate.loadPreset(stateB);
      } else {
        compareActive = false;
        compareBtn.setButtonText("Compare (A)");
        compareBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        plate.loadPreset(stateA);
      }
      syncControlsFromDSP();
    };
    addAndMakeVisible(compareBtn);

    copyBtn.setButtonText("Copy");
    copyBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    copyBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    copyBtn.onClick = [this] {
      clipboardPreset = plate.getCurrentPresetState("Clipboard Preset");
      hasClipboard = true;
      copyBtn.setButtonText("Copied!");
      juce::Timer::callAfterDelay(1200, [this] { copyBtn.setButtonText("Copy"); });
    };
    addAndMakeVisible(copyBtn);

    pasteBtn.setButtonText("Paste");
    pasteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    pasteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    pasteBtn.onClick = [this] {
      if (hasClipboard) {
        pushUndo();
        plate.loadPreset(clipboardPreset);
        syncControlsFromDSP();
        pasteBtn.setButtonText("Pasted!");
        juce::Timer::callAfterDelay(1200, [this] { pasteBtn.setButtonText("Paste"); });
      }
    };
    addAndMakeVisible(pasteBtn);

    undoBtn.setButtonText("Undo");
    undoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    undoBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    undoBtn.onClick = [this] {
      if (undoPos > 0) {
        --undoPos;
        plate.loadPreset(undoHistory[static_cast<size_t>(undoPos)]);
        syncControlsFromDSP();
      }
    };
    addAndMakeVisible(undoBtn);

    redoBtn.setButtonText("Redo");
    redoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    redoBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    redoBtn.onClick = [this] {
      if (undoPos + 1 < static_cast<int>(undoHistory.size())) {
        ++undoPos;
        plate.loadPreset(undoHistory[static_cast<size_t>(undoPos)]);
        syncControlsFromDSP();
      }
    };
    addAndMakeVisible(redoBtn);

    // Wet Solo Button
    wetSoloBtn.setButtonText("WET SOLO");
    wetSoloBtn.setColour(juce::TextButton::buttonColourId,
                         plate.getWetOnly() ? juce::Colour(0xffd73a49) : juce::Colour(0xff21262d));
    wetSoloBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    wetSoloBtn.onClick = [this] {
      const bool n = !plate.getWetOnly();
      plate.setWetOnly(n);
      wetSoloBtn.setColour(juce::TextButton::buttonColourId,
                           n ? juce::Colour(0xffd73a49) : juce::Colour(0xff21262d));
      repaint();
    };
    addAndMakeVisible(wetSoloBtn);

    // Initial state setup for Compare and Undo
    stateB = plate.getFactoryPresets()[0];
    stateA = stateB;
    pushUndo();

    // Child components
    addAndMakeVisible(filterSlider);
    addAndMakeVisible(plateSwitch);
    addAndMakeVisible(damperA);
    addAndMakeVisible(damperB);
    addAndMakeVisible(damperC);
    addAndMakeVisible(vuMeter);

    addAndMakeVisible(knobWidth);
    addAndMakeVisible(knobBalance);
    addAndMakeVisible(eqSwitch);
    addAndMakeVisible(knobLoFreq);
    addAndMakeVisible(knobLoGain);
    addAndMakeVisible(knobHiFreq);
    addAndMakeVisible(knobHiGain);
    addAndMakeVisible(knobModRate);
    addAndMakeVisible(knobModDepth);
    addAndMakeVisible(knobPreDelay);
    addAndMakeVisible(knobMix);
    addAndMakeVisible(powerSwitch);

    // Wire parameter bindings
    filterSlider.setFilterIndex(plate.getInputFilterIndex());
    filterSlider.onIndexChanged = [this](int idx) {
      plate.setInputFilterIndex(idx);
    };
    filterSlider.onDragEnded = [this] { pushUndo(); };

    plateSwitch.setSelectedPlate(plate.getPlate());
    plateSwitch.onPlateSelected = [this](int p) {
      plate.setPlate(p);
      updateDamperHighlights();
      pushUndo();
    };

    damperA.onDecayChanged = [this](double sec) {
      plate.setPlateDecay(0, sec);
    };
    damperA.onDragEnded = [this] { pushUndo(); };

    damperB.onDecayChanged = [this](double sec) {
      plate.setPlateDecay(1, sec);
    };
    damperB.onDragEnded = [this] { pushUndo(); };

    damperC.onDecayChanged = [this](double sec) {
      plate.setPlateDecay(2, sec);
    };
    damperC.onDragEnded = [this] { pushUndo(); };

    updateDamperHighlights();

    knobWidth.setValue(plate.getStereoWidth() * 100.0);
    knobWidth.onValueChanged = [this](double val) {
      plate.setStereoWidth(val * 0.01);
    };
    knobWidth.onDragEnded = [this] { pushUndo(); };

    knobBalance.setValue(plate.getStereoBalance());
    knobBalance.onValueChanged = [this](double val) {
      plate.setStereoBalance(val);
    };
    knobBalance.onDragEnded = [this] { pushUndo(); };

    eqSwitch.setToggleState(plate.isEqEnabled());
    eqSwitch.onStateChanged = [this](bool en) {
      plate.setEqEnabled(en);
      pushUndo();
    };

    knobLoFreq.setValue(plate.getEqLowFreq());
    knobLoFreq.onValueChanged = [this](double val) { plate.setEqLowFreq(val); };
    knobLoFreq.onDragEnded = [this] { pushUndo(); };

    knobLoGain.setValue(plate.getEqLowGainDb());
    knobLoGain.onValueChanged = [this](double val) {
      plate.setEqLowGainDb(val);
    };
    knobLoGain.onDragEnded = [this] { pushUndo(); };

    knobHiFreq.setValue(plate.getEqHighFreq());
    knobHiFreq.onValueChanged = [this](double val) {
      plate.setEqHighFreq(val);
    };
    knobHiFreq.onDragEnded = [this] { pushUndo(); };

    knobHiGain.setValue(plate.getEqHighGainDb());
    knobHiGain.onValueChanged = [this](double val) {
      plate.setEqHighGainDb(val);
    };
    knobHiGain.onDragEnded = [this] { pushUndo(); };

    knobModRate.setValue(plate.getModRateHz());
    knobModRate.onValueChanged = [this](double val) {
      plate.setModRateHz(val);
    };
    knobModRate.onDragEnded = [this] { pushUndo(); };

    knobModDepth.setValue(plate.getModDepth());
    knobModDepth.onValueChanged = [this](double val) {
      plate.setModDepth(val);
    };
    knobModDepth.onDragEnded = [this] { pushUndo(); };

    knobPreDelay.setValue(plate.getPreDelay());
    knobPreDelay.onValueChanged = [this](double val) {
      plate.setPreDelay(val);
    };
    knobPreDelay.onDragEnded = [this] { pushUndo(); };

    knobMix.setValue(plate.getMix() * 100.0);
    knobMix.onValueChanged = [this](double val) { plate.setMix(val * 0.01); };
    knobMix.onDragEnded = [this] { pushUndo(); };

    powerSwitch.setToggleState(plate.getPower());
    powerSwitch.onStateChanged = [this](bool pwr) {
      plate.setPower(pwr);
      repaint();
    };

    // Start 30 fps UI timer for VU meter
    startTimerHz(30);
  }

  ~EMT140Panel() override { stopTimer(); }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // 1. Dark outer rack chassis background
    g.setColour(juce::Colour(0xff16181b));
    g.fillAll();

    // Top toolbar
    const float toolbarH = (getHeight() < 380) ? 32.0f : 42.0f;
    auto toolbar = bounds.removeFromTop(toolbarH);
    g.setColour(juce::Colour(0xff1f2328));
    g.fillRect(toolbar);
    g.setColour(juce::Colour(0xff30363d));
    g.drawHorizontalLine(static_cast<int>(toolbar.getBottom()), toolbar.getX(),
                         toolbar.getRight());

    // Power LED Indicator & Title
    auto powerLedRect = juce::Rectangle<float>(12.0f, (toolbarH - 18.0f) * 0.5f, 18.0f, 18.0f);
    const bool pwr = plate.getPower();
    g.setColour(pwr ? juce::Colour(0xff2ea043) : juce::Colour(0xff30363d));
    g.fillEllipse(powerLedRect);
    g.setColour(pwr ? juce::Colour(0xff56d364) : juce::Colour(0xff484f58));
    g.drawEllipse(powerLedRect, 1.0f);

    // Title centered or right
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfff0f6fc));
    g.drawText("EMT 140 NACHHALLPLATTE", static_cast<int>(toolbar.getRight() - 220.0f), 0,
               210, static_cast<int>(toolbarH), juce::Justification::centredRight, false);

    // 2. Vintage Brushed Warm Silver/Aluminum Faceplate
    auto faceplate = bounds.reduced(8.0f, 6.0f);
    juce::ColourGradient aluGrad(juce::Colour(0xffe9ecef), faceplate.getX(),
                                 faceplate.getY(), juce::Colour(0xffced4da),
                                 faceplate.getX(), faceplate.getBottom(), false);
    g.setGradientFill(aluGrad);
    g.fillRoundedRectangle(faceplate, 4.0f);

    // Engraved border bevel
    g.setColour(juce::Colour(0xffadb5bd));
    g.drawRoundedRectangle(faceplate, 4.0f, 1.2f);

    // Corner mounting screws (+)
    auto drawPlateScrew = [&g](float x, float y) {
      g.setColour(juce::Colour(0xff868e96));
      g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
      g.setColour(juce::Colour(0xff212529));
      g.drawLine(x - 2.5f, y, x + 2.5f, y, 0.8f);
      g.drawLine(x, y - 2.5f, x, y + 2.5f, 0.8f);
    };
    drawPlateScrew(faceplate.getX() + 6.0f, faceplate.getY() + 6.0f);
    drawPlateScrew(faceplate.getRight() - 6.0f, faceplate.getY() + 6.0f);
    drawPlateScrew(faceplate.getX() + 6.0f, faceplate.getBottom() - 6.0f);
    drawPlateScrew(faceplate.getRight() - 6.0f, faceplate.getBottom() - 6.0f);

    if (getHeight() < 380) {
      // Widescreen Embedded Section Dividers and Titles
      // Module 1: INPUT & SELECTION (StartX to Filter/Plate)
      // Module 2: REVERBERATION TIME (Dampers A, B, C)
      // Module 3: TONE SHAPING EQUALISER
      // Module 4: MODULATION & STEREO
      // Module 5: BLEND & OUTPUT
      // Module 6: ANALOG VU METER
      const float secY = faceplate.getY() + 4.0f;
      const float secH = faceplate.getHeight() - 8.0f;

      // Vertical engraved dividers between sections
      g.setColour(juce::Colour(0xffadb5bd));
      const float divXs[] = {
          damperA.getX() - 6.0f,
          knobLoFreq.getX() - 8.0f,
          knobModRate.getX() - 8.0f,
          knobPreDelay.getX() - 8.0f,
          vuMeter.getX() - 8.0f
      };
      for (float dx : divXs) {
        g.drawVerticalLine(static_cast<int>(dx), secY + 4.0f, secY + secH - 4.0f);
      }

      // Section Header Titles
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(juce::Colour(0xff212529));

      // Reverberation Time Header
      g.drawText("REVERBERATION TIME", damperA.getX(), faceplate.getY() + 2.0f,
                 damperC.getRight() - damperA.getX(), 12.0f,
                 juce::Justification::centred, false);

      // Tone Shaping EQ Header
      g.drawText("TONE SHAPING EQ", knobLoFreq.getX(), faceplate.getY() + 2.0f,
                 knobHiFreq.getRight() - knobLoFreq.getX(), 12.0f,
                 juce::Justification::centred, false);

      // Modulation & Stereo Header
      g.drawText("MOD & STEREO", knobModRate.getX(), faceplate.getY() + 2.0f,
                 knobBalance.getRight() - knobModRate.getX(), 12.0f,
                 juce::Justification::centred, false);

      // Blend & Output Header
      g.drawText("TIME & BLEND", knobPreDelay.getX(), faceplate.getY() + 2.0f,
                 knobMix.getRight() - knobPreDelay.getX(), 12.0f,
                 juce::Justification::centred, false);

      return;
    }

    // Full Height Standalone Mode
    g.setColour(juce::Colour(0xffadb5bd));
    const float divXs[] = {110.0f, 175.0f, 440.0f, 555.0f, 615.0f, 715.0f, 765.0f};
    for (float dx : divXs) {
      g.drawVerticalLine(static_cast<int>(faceplate.getX() + dx),
                         faceplate.getY() + 6.0f, faceplate.getBottom() - 6.0f);
    }
  }

  void resized() override {
    const int w = getWidth();
    const int h = getHeight();

    if (h < 380) {
      // ==============================================================================
      // Widescreen Embedded Console Mode (Tinggi ~280px, Lebar ~1300px)
      // ==============================================================================
      const int toolbarH = 30;
      presetBtn.setBounds(10, 4, 180, 22);
      prevPresetBtn.setBounds(194, 4, 22, 22);
      nextPresetBtn.setBounds(218, 4, 22, 22);
      compareBtn.setBounds(244, 4, 82, 22);
      copyBtn.setBounds(330, 4, 44, 22);
      pasteBtn.setBounds(378, 4, 44, 22);
      undoBtn.setBounds(426, 4, 44, 22);
      redoBtn.setBounds(474, 4, 44, 22);
      wetSoloBtn.setBounds(522, 4, 76, 22);

      auto face = getLocalBounds().toFloat().reduced(8.0f, 4.0f);
      face.removeFromTop(static_cast<float>(toolbarH + 2));

      const float startX = face.getX();
      const float startY = face.getY();
      const float faceH = face.getHeight();

      // Seksi 1: Input Filter & Plate Selector (~155px)
      filterSlider.setBounds(static_cast<int>(startX + 4.0f),
                             static_cast<int>(startY + 4.0f), 78,
                             static_cast<int>(faceH - 8.0f));

      plateSwitch.setBounds(static_cast<int>(startX + 86.0f),
                            static_cast<int>(startY + 4.0f), 68,
                            static_cast<int>(faceH - 8.0f));

      // Seksi 2: Reverberation Time Gauges (A, B, C) (~260px)
      const float gaugeW = 78.0f;
      const float gaugeH = faceH - 22.0f;
      const float damperStartY = startY + 16.0f;
      damperA.setBounds(static_cast<int>(startX + 162.0f),
                        static_cast<int>(damperStartY),
                        static_cast<int>(gaugeW), static_cast<int>(gaugeH));
      damperB.setBounds(static_cast<int>(startX + 244.0f),
                        static_cast<int>(damperStartY),
                        static_cast<int>(gaugeW), static_cast<int>(gaugeH));
      damperC.setBounds(static_cast<int>(startX + 326.0f),
                        static_cast<int>(damperStartY),
                        static_cast<int>(gaugeW), static_cast<int>(gaugeH));

      // Seksi 3: Tone Shaping EQ (2x2 Knobs + EQ Bat Switch) (~185px)
      const float eqX = startX + 418.0f;
      eqSwitch.setBounds(static_cast<int>(eqX + 22.0f),
                         static_cast<int>(startY + 16.0f), 60, 24);

      const float knobW = 48.0f;
      const float knobH = 68.0f;
      const float row1Y = startY + 44.0f;
      const float row2Y = startY + 118.0f;

      knobLoFreq.setBounds(static_cast<int>(eqX + 4.0f), static_cast<int>(row1Y),
                           static_cast<int>(knobW), static_cast<int>(knobH));
      knobHiFreq.setBounds(static_cast<int>(eqX + 56.0f), static_cast<int>(row1Y),
                           static_cast<int>(knobW), static_cast<int>(knobH));

      knobLoGain.setBounds(static_cast<int>(eqX + 4.0f), static_cast<int>(row2Y),
                           static_cast<int>(knobW), static_cast<int>(knobH));
      knobHiGain.setBounds(static_cast<int>(eqX + 56.0f), static_cast<int>(row2Y),
                           static_cast<int>(knobW), static_cast<int>(knobH));

      // Seksi 4: Modulation & Stereo (4 Knobs) (~185px)
      const float modX = startX + 538.0f;
      knobModRate.setBounds(static_cast<int>(modX + 4.0f), static_cast<int>(row1Y),
                            static_cast<int>(knobW), static_cast<int>(knobH));
      knobModDepth.setBounds(static_cast<int>(modX + 56.0f), static_cast<int>(row1Y),
                             static_cast<int>(knobW), static_cast<int>(knobH));

      knobWidth.setBounds(static_cast<int>(modX + 4.0f), static_cast<int>(row2Y),
                          static_cast<int>(knobW), static_cast<int>(knobH));
      knobBalance.setBounds(static_cast<int>(modX + 56.0f), static_cast<int>(row2Y),
                            static_cast<int>(knobW), static_cast<int>(knobH));

      // Seksi 5: Time & Blend (Pre Delay & Mix) (~140px)
      const float blendX = startX + 658.0f;
      knobPreDelay.setBounds(static_cast<int>(blendX + 6.0f), static_cast<int>(row1Y),
                             52, static_cast<int>(knobH));
      knobMix.setBounds(static_cast<int>(blendX + 6.0f), static_cast<int>(row2Y),
                        52, static_cast<int>(knobH));

      // Power Switch in faceplate
      powerSwitch.setBounds(static_cast<int>(blendX + 64.0f),
                            static_cast<int>(startY + (faceH - 32.0f) * 0.5f), 52, 32);

      // Seksi 6: Authentic Vintage Analog Dual VU Meter (~240px wide, ~150px high)
      const float vuX = startX + 786.0f;
      const float vuW = std::clamp(face.getRight() - vuX - 8.0f, 220.0f, 260.0f);
      const float vuH = std::min(faceH - 24.0f, 156.0f);
      const float vuY = startY + (faceH - vuH) * 0.5f;

      vuMeter.setBounds(static_cast<int>(vuX), static_cast<int>(vuY),
                        static_cast<int>(vuW), static_cast<int>(vuH));
      return;
    }

    // ==============================================================================
    // Standalone Window Mode (Full Height, h >= 380)
    // ==============================================================================
    presetBtn.setBounds(50, 8, 180, 26);
    prevPresetBtn.setBounds(234, 8, 24, 26);
    nextPresetBtn.setBounds(260, 8, 24, 26);
    compareBtn.setBounds(288, 8, 80, 26);
    copyBtn.setBounds(372, 8, 44, 26);
    pasteBtn.setBounds(420, 8, 44, 26);
    undoBtn.setBounds(468, 8, 44, 26);
    redoBtn.setBounds(516, 8, 44, 26);
    wetSoloBtn.setBounds(564, 8, 80, 26);

    auto faceplate = getLocalBounds().toFloat().reduced(10.0f, 8.0f);
    faceplate.removeFromTop(42.0f);

    filterSlider.setBounds(static_cast<int>(faceplate.getX() + 6.0f),
                           static_cast<int>(faceplate.getY() + 6.0f), 95,
                           static_cast<int>(faceplate.getHeight() - 12.0f));

    plateSwitch.setBounds(static_cast<int>(faceplate.getX() + 112.0f),
                          static_cast<int>(faceplate.getY() + 6.0f), 60,
                          static_cast<int>(faceplate.getHeight() - 12.0f));

    const float gaugeW = 82.0f;
    const float gaugeH = faceplate.getHeight() - 36.0f;
    damperA.setBounds(static_cast<int>(faceplate.getX() + 182.0f),
                      static_cast<int>(faceplate.getY() + 28.0f),
                      static_cast<int>(gaugeW), static_cast<int>(gaugeH));
    damperB.setBounds(static_cast<int>(faceplate.getX() + 268.0f),
                      static_cast<int>(faceplate.getY() + 28.0f),
                      static_cast<int>(gaugeW), static_cast<int>(gaugeH));
    damperC.setBounds(static_cast<int>(faceplate.getX() + 354.0f),
                      static_cast<int>(faceplate.getY() + 28.0f),
                      static_cast<int>(gaugeW), static_cast<int>(gaugeH));

    const float emtCx = faceplate.getX() + 498.0f;
    powerSwitch.setBounds(static_cast<int>(emtCx - 30.0f),
                          static_cast<int>(faceplate.getY() + 120.0f), 60, 36);

    knobWidth.setBounds(static_cast<int>(faceplate.getX() + 558.0f),
                        static_cast<int>(faceplate.getY() + 32.0f), 54, 76);
    knobBalance.setBounds(static_cast<int>(faceplate.getX() + 558.0f),
                          static_cast<int>(faceplate.getY() + 130.0f), 54, 76);

    eqSwitch.setBounds(static_cast<int>(faceplate.getX() + 636.0f),
                       static_cast<int>(faceplate.getY() + 10.0f), 56, 32);
    knobLoFreq.setBounds(static_cast<int>(faceplate.getX() + 616.0f),
                         static_cast<int>(faceplate.getY() + 46.0f), 48, 68);
    knobHiFreq.setBounds(static_cast<int>(faceplate.getX() + 664.0f),
                         static_cast<int>(faceplate.getY() + 46.0f), 48, 68);
    knobLoGain.setBounds(static_cast<int>(faceplate.getX() + 616.0f),
                         static_cast<int>(faceplate.getY() + 124.0f), 48, 68);
    knobHiGain.setBounds(static_cast<int>(faceplate.getX() + 664.0f),
                         static_cast<int>(faceplate.getY() + 124.0f), 48, 68);

    knobModRate.setBounds(static_cast<int>(faceplate.getX() + 716.0f),
                          static_cast<int>(faceplate.getY() + 32.0f), 48, 76);
    knobModDepth.setBounds(static_cast<int>(faceplate.getX() + 716.0f),
                           static_cast<int>(faceplate.getY() + 130.0f), 48, 76);

    vuMeter.setBounds(static_cast<int>(faceplate.getX() + 772.0f),
                      static_cast<int>(faceplate.getY() + 10.0f), 220, 138);

    knobPreDelay.setBounds(static_cast<int>(faceplate.getX() + 788.0f),
                           static_cast<int>(faceplate.getY() + 168.0f), 54, 76);
    knobMix.setBounds(static_cast<int>(faceplate.getX() + 862.0f),
                      static_cast<int>(faceplate.getY() + 168.0f), 54, 76);
  }

  void showPresetMenu() {
    juce::PopupMenu m;
    const auto &presets = plate.getFactoryPresets();

    juce::PopupMenu vocalsMenu;
    juce::PopupMenu drumsMenu;
    juce::PopupMenu instrumentsMenu;
    juce::PopupMenu studiosMenu;
    juce::PopupMenu ambientMenu;
    juce::PopupMenu specialMenu;

    for (size_t i = 0; i < presets.size(); ++i) {
      const auto &p = presets[i];
      const int id = static_cast<int>(i + 1);
      const bool isCurrent = (static_cast<int>(i) == currentPresetIdx);
      const juce::String cat(p.category);

      if (cat == "Vocals") {
        vocalsMenu.addItem(id, p.name, true, isCurrent);
      } else if (cat == "Drums") {
        drumsMenu.addItem(id, p.name, true, isCurrent);
      } else if (cat == "Instruments") {
        instrumentsMenu.addItem(id, p.name, true, isCurrent);
      } else if (cat == "Studios") {
        studiosMenu.addItem(id, p.name, true, isCurrent);
      } else if (cat == "Ambient") {
        ambientMenu.addItem(id, p.name, true, isCurrent);
      } else {
        specialMenu.addItem(id, p.name, true, isCurrent);
      }
    }

    m.addSubMenu("Vocals", vocalsMenu);
    m.addSubMenu("Drums & Percussion", drumsMenu);
    m.addSubMenu("Instruments", instrumentsMenu);
    m.addSubMenu("Vintage Studios (Abbey Rd / Capitol)", studiosMenu);
    m.addSubMenu("Ambient & Caverns", ambientMenu);
    m.addSubMenu("Special (Wet Solo / Send)", specialMenu);
    m.addSeparator();

    juce::PopupMenu allMenu;
    for (size_t i = 0; i < presets.size(); ++i) {
      allMenu.addItem(static_cast<int>(i + 1), presets[i].name, true,
                      static_cast<int>(i) == currentPresetIdx);
    }
    m.addSubMenu("All Presets (1-16)", allMenu);
    m.addSeparator();
    m.addItem(100, "Reset to Default", true, false);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
                    [this, presets](int result) {
                      if (result >= 1 && result <= static_cast<int>(presets.size())) {
                        applyPreset(result - 1);
                      } else if (result == 100) {
                        applyPreset(0);
                      }
                    });
  }

  void applyPreset(int index) {
    const auto &presets = plate.getFactoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
      pushUndo();
      currentPresetIdx = index;
      stateB = presets[static_cast<size_t>(index)];
      plate.loadPreset(stateB);
      presetBtn.setButtonText(juce::String(stateB.name) + " \xe2\x96\xbe");
      syncControlsFromDSP();
    }
  }

  void pushUndo() {
    auto cur = plate.getCurrentPresetState();
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

  void syncControlsFromDSP() {
    filterSlider.setFilterIndex(plate.getInputFilterIndex(),
                                juce::dontSendNotification);
    plateSwitch.setSelectedPlate(plate.getPlate(), juce::dontSendNotification);
    damperA.setDecay(plate.getPlateDecay(0), juce::dontSendNotification);
    damperB.setDecay(plate.getPlateDecay(1), juce::dontSendNotification);
    damperC.setDecay(plate.getPlateDecay(2), juce::dontSendNotification);
    updateDamperHighlights();

    knobWidth.setValue(plate.getStereoWidth() * 100.0, juce::dontSendNotification);
    knobBalance.setValue(plate.getStereoBalance(), juce::dontSendNotification);
    eqSwitch.setToggleState(plate.isEqEnabled(), juce::dontSendNotification);
    knobLoFreq.setValue(plate.getEqLowFreq(), juce::dontSendNotification);
    knobLoGain.setValue(plate.getEqLowGainDb(), juce::dontSendNotification);
    knobHiFreq.setValue(plate.getEqHighFreq(), juce::dontSendNotification);
    knobHiGain.setValue(plate.getEqHighGainDb(), juce::dontSendNotification);
    knobModRate.setValue(plate.getModRateHz(), juce::dontSendNotification);
    knobModDepth.setValue(plate.getModDepth(), juce::dontSendNotification);
    knobPreDelay.setValue(plate.getPreDelay(), juce::dontSendNotification);
    knobMix.setValue(plate.getMix() * 100.0, juce::dontSendNotification);
    powerSwitch.setToggleState(plate.getPower(), juce::dontSendNotification);
    wetSoloBtn.setColour(juce::TextButton::buttonColourId,
                         plate.getWetOnly() ? juce::Colour(0xffd73a49)
                                            : juce::Colour(0xff21262d));

    repaint();
  }

private:
  void timerCallback() override {
    // Poll real-time VU meter level from DSP engine for both channels
    if (plate.getPower()) {
      vuMeter.setLevels(plate.getMeterVuL(), plate.getMeterVuR());
    } else {
      vuMeter.setLevels(0.0f, 0.0f);
    }
  }

  void updateDamperHighlights() {
    const int p = plate.getPlate();
    damperA.setActive(p == 0);
    damperB.setActive(p == 1);
    damperC.setActive(p == 2);
  }

  nova::graviton::EMT140PlateReverb &plate;

  juce::TextButton presetBtn;
  juce::TextButton prevPresetBtn, nextPresetBtn;
  juce::TextButton compareBtn, copyBtn, pasteBtn, undoBtn, redoBtn;
  juce::TextButton wetSoloBtn;

  int currentPresetIdx = 0;
  nova::graviton::EMT140PlateReverb::Preset stateA, stateB, clipboardPreset;
  bool hasClipboard = false;
  std::vector<nova::graviton::EMT140PlateReverb::Preset> undoHistory;
  int undoPos = -1;

  InputFilterSlider filterSlider;
  PlateSelectorSwitch plateSwitch;
  PlateDamperModule damperA, damperB, damperC;
  AnalogVUMeter vuMeter;

  VintageKnob knobLoFreq, knobLoGain, knobHiFreq, knobHiGain;
  VintageKnob knobModRate, knobModDepth;
  VintageKnob knobWidth, knobBalance;
  VintageKnob knobPreDelay, knobMix;
  VintageToggleSwitch eqSwitch;
  VintageToggleSwitch powerSwitch;

  bool compareActive = false;
};

} // namespace nova
