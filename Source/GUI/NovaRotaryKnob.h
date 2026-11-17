#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <algorithm>
#include <functional>

namespace nova {

// ==============================================================================
// HIGH-PRECISION ROTARY KNOB COMPONENT
// ==============================================================================
class NovaRotaryKnob : public juce::Component {
public:
  NovaRotaryKnob(const juce::String &paramLabel, const juce::String &unitStr,
                 float minVal, float maxVal, float defaultVal,
                 bool isLarge = false,
                 juce::Colour accent = juce::Colour(0xff0ea5e9))
      : label(paramLabel), unit(unitStr), minV(minVal), maxV(maxVal),
        currentV(defaultVal), largeKnob(isLarge), accentCol(accent) {
    setRepaintsOnMouseActivity(true);
  }

  void setRange(float minVal, float maxVal) {
    minV = minVal;
    maxV = maxVal;
    currentV = std::clamp(currentV, minV, maxV);
    repaint();
  }

  void setLogarithmic(bool log) noexcept {
    isLog = log;
    repaint();
  }

  void setValue(float v,
                juce::NotificationType notify = juce::sendNotification) {
    const float clamped = std::clamp(v, minV, maxV);
    if (std::abs(clamped - currentV) > 1.0e-5f) {
      currentV = clamped;
      repaint();
      if (notify != juce::dontSendNotification && onValueChanged)
        onValueChanged(currentV);
    }
  }

  float getValue() const noexcept { return currentV; }

  void setAccentColour(juce::Colour c) {
    accentCol = c;
    repaint();
  }

  void setDisplayDecimals(int d) { decimals = d; }
  void setCustomText(const juce::String &t) {
    customText = t;
    repaint();
  }

  float valueToNorm(float v) const noexcept {
    if (maxV <= minV)
      return 0.0f;
    if (isLog && minV > 0.0f) {
      const float logMin = std::log10(minV);
      const float logMax = std::log10(maxV);
      const float logVal = std::log10(std::clamp(v, minV, maxV));
      return std::clamp((logVal - logMin) / (logMax - logMin), 0.0f, 1.0f);
    }
    return std::clamp((v - minV) / (maxV - minV), 0.0f, 1.0f);
  }

  float normToValue(float n) const noexcept {
    const float clampedN = std::clamp(n, 0.0f, 1.0f);
    if (isLog && minV > 0.0f) {
      return minV * std::pow(maxV / minV, clampedN);
    }
    return minV + clampedN * (maxV - minV);
  }

  std::function<void(float)> onValueChanged;

  void mouseDown(const juce::MouseEvent &e) override {
    dragStartY = e.position.y;
    dragStartNorm = valueToNorm(currentV);
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float sensitivity = e.mods.isShiftDown() ? 0.001f : 0.0045f;
    const float deltaNorm = (dragStartY - e.position.y) * sensitivity;
    setValue(normToValue(dragStartNorm + deltaNorm));
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &w) override {
    const float step = w.isReversed ? -0.02f : 0.02f;
    const float curNorm = valueToNorm(currentV);
    setValue(normToValue(curNorm + step * w.deltaY));
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    const float topLabelH = 14.0f;
    const float bottomValH = 15.0f;

    // Top Label
    if (!label.isEmpty()) {
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.setColour(juce::Colour(0xff94a3b8));
      g.drawText(label, 0, 0, static_cast<int>(b.getWidth()),
                 static_cast<int>(topLabelH), juce::Justification::centred);
    }

    // Dial Area
    const float dialY = topLabelH + 2.0f;
    const float dialH = b.getHeight() - topLabelH - bottomValH - 4.0f;
    const float dialSize = std::min(b.getWidth() - 8.0f, dialH);
    const float dialX = (b.getWidth() - dialSize) * 0.5f;
    auto dialRect = juce::Rectangle<float>(
        dialX, dialY + (dialH - dialSize) * 0.5f, dialSize, dialSize);

    // Outer Track Ring
    const float norm = valueToNorm(currentV);
    const float startAngle = -2.35619f; // -135 deg
    const float endAngle = 2.35619f;    // +135 deg
    const float currentAngle = startAngle + norm * (endAngle - startAngle);

    // Track Background
    g.setColour(juce::Colour(0xff1e293b));
    juce::Path trackPath;
    trackPath.addCentredArc(dialRect.getCentreX(), dialRect.getCentreY(),
                            dialRect.getWidth() * 0.44f,
                            dialRect.getHeight() * 0.44f, 0.0f, startAngle,
                            endAngle, true);
    g.strokePath(trackPath,
                 juce::PathStrokeType(largeKnob ? 3.5f : 2.5f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Active Glow Arc
    if (norm > 0.001f) {
      g.setColour(accentCol);
      juce::Path valPath;
      valPath.addCentredArc(dialRect.getCentreX(), dialRect.getCentreY(),
                            dialRect.getWidth() * 0.44f,
                            dialRect.getHeight() * 0.44f, 0.0f, startAngle,
                            currentAngle, true);
      g.strokePath(valPath,
                   juce::PathStrokeType(largeKnob ? 3.5f : 2.5f,
                                        juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
    }

    // Knob Body Cap (Metallic Gradient)
    const float capSize = dialSize * 0.72f;
    auto capRect = juce::Rectangle<float>(
        dialRect.getCentreX() - capSize * 0.5f,
        dialRect.getCentreY() - capSize * 0.5f, capSize, capSize);
    juce::ColourGradient capGrad(juce::Colour(0xff334155), capRect.getX(),
                                 capRect.getY(), juce::Colour(0xff0f172a),
                                 capRect.getRight(), capRect.getBottom(), true);
    g.setGradientFill(capGrad);
    g.fillEllipse(capRect);
    g.setColour(juce::Colour(0xff475569));
    g.drawEllipse(capRect, 1.0f);

    // Indicator Pointer Line
    const float ptrR = capSize * 0.46f;
    const float px = dialRect.getCentreX() + std::sin(currentAngle) * ptrR;
    const float py = dialRect.getCentreY() - std::cos(currentAngle) * ptrR;
    g.setColour(juce::Colours::white);
    g.drawLine(dialRect.getCentreX(), dialRect.getCentreY(), px, py,
               largeKnob ? 2.5f : 1.8f);

    // Bottom Value Text with precision formatting
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfff1f5f9));

    juce::String valStr = customText;
    if (valStr.isEmpty()) {
      if (unit == "Hz") {
        if (currentV >= 1000.0f)
          valStr = juce::String(currentV * 0.001f, 1) + " kHz";
        else
          valStr = juce::String(static_cast<int>(std::round(currentV))) + " Hz";
      } else if (unit == ":1") {
        if (currentV >= 19.5f)
          valStr = "inf";
        else
          valStr = juce::String(currentV, 1) + ":1";
      } else if (unit == "dB") {
        if (currentV > 0.0f && label != "RANGE" && label != "THRESHOLD" && label != "RMS")
          valStr = "+" + juce::String(currentV, decimals) + " dB";
        else
          valStr = juce::String(currentV, decimals) + " dB";
      } else if (unit == "%") {
        valStr = juce::String(currentV, decimals) + " %";
      } else if (unit == "ms") {
        valStr = (currentV < 10.0f
                      ? juce::String(currentV, 1)
                      : juce::String(static_cast<int>(std::round(currentV)))) +
                 " ms";
      } else {
        valStr = juce::String(currentV, decimals) +
                 (unit.isNotEmpty() ? (" " + unit) : "");
      }
    }
    g.drawText(valStr, 0, static_cast<int>(b.getHeight() - bottomValH),
               static_cast<int>(b.getWidth()), static_cast<int>(bottomValH),
               juce::Justification::centred);
  }

private:
  juce::String label;
  juce::String unit;
  float minV, maxV, currentV;
  bool isLog = false;
  bool largeKnob = false;
  juce::Colour accentCol;
  int decimals = 1;
  juce::String customText;
  float dragStartY = 0.0f;
  float dragStartNorm = 0.0f;
};

// ==============================================================================
// TACTILE DRAGGABLE FREQUENCY BADGE LABEL
// ==============================================================================
class FreqDraggableLabel : public juce::Label {
public:
  FreqDraggableLabel(float minF = 10.0f, float maxF = 40000.0f)
      : minFreq(minF), maxFreq(maxF) {
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setJustificationType(juce::Justification::centred);
    setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
  }

  std::function<void(float)> onFreqChanged;

  void setRange(float minF, float maxF) {
    minFreq = minF;
    maxFreq = maxF;
  }

  void mouseDown(const juce::MouseEvent &e) override {
    dragStartY = e.position.y;
    dragStartFreq = currentFreq;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float deltaY = dragStartY - e.position.y;
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);
    const float curNorm =
        (std::log10(std::clamp(dragStartFreq, minFreq, maxFreq)) - logMin) /
        (logMax - logMin);
    const float newNorm = std::clamp(curNorm + deltaY * 0.005f, 0.0f, 1.0f);
    const float newFreq = minFreq * std::pow(maxFreq / minFreq, newNorm);
    setFreq(newFreq, true);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &w) override {
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);
    const float curNorm =
        (std::log10(std::clamp(currentFreq, minFreq, maxFreq)) - logMin) /
        (logMax - logMin);
    const float newNorm = std::clamp(curNorm + w.deltaY * 0.03f, 0.0f, 1.0f);
    const float newFreq = minFreq * std::pow(maxFreq / minFreq, newNorm);
    setFreq(newFreq, true);
  }

  void setFreq(float f, bool notify) {
    currentFreq = std::clamp(f, minFreq, maxFreq);
    if (currentFreq >= 1000.0f)
      setText(juce::String(currentFreq * 0.001f, 1) + " kHz",
              juce::dontSendNotification);
    else
      setText(juce::String(static_cast<int>(std::round(currentFreq))) + " Hz",
              juce::dontSendNotification);
    if (notify && onFreqChanged)
      onFreqChanged(currentFreq);
  }

  float getFreq() const noexcept { return currentFreq; }

private:
  float minFreq = 10.0f;
  float maxFreq = 40000.0f;
  float currentFreq = 100.0f;
  float dragStartY = 0.0f;
  float dragStartFreq = 100.0f;
};

} // namespace nova
