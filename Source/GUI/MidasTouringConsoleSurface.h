#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

#include "Audio/SecondaryAudioStreamer.h"
#include "DSP/GravitonDSP.h"
#include "GUI/ASIOBridgePanel.h"
#include "GUI/CueListPanel.h"
#include "GUI/DuganAutomixerPanel.h"
#include "GUI/DynamicEqPanel.h"
#include "GUI/DynamicsPanel.h"
#include "GUI/EMT140Panel.h"
#include "GUI/EffectsRackPanel.h"
#include "GUI/EmbeddedFxHostPanel.h"
#include "GUI/EqPanel.h"
#include "GUI/MasterXPanel.h"
#include "GUI/MonitorPanel.h"
#include "GUI/NebulaDelayPanel.h"
#include "GUI/OfflineBouncePanel.h"
#include "GUI/OscServerPanel.h"
#include "GUI/StageBoxPatchPanel.h"
#include "GUI/SurroundPanPanel.h"
#include "GUI/TalkbackPanel.h"
#include "GUI/VSS4Panel.h"
#include "GUI/VcaPanel.h"
#include "MIDI/MixerMidiCallback.h"
#include "Mixer/ChannelState.h"
#include "Mixer/MixerEngine.h"

using namespace nova;

namespace midas_colors {
static const juce::Colour kChassisBg{0xff0c192c};
static const juce::Colour kDeckBg{0xff10223b};
static const juce::Colour kSurfaceBg{0xff091322};
static const juce::Colour kStripBg{0xff0e1d32};
static const juce::Colour kBorderCol{0xff1e3a5f};
static const juce::Colour kBorderSubtle{0xff152b47};
static const juce::Colour kAccentCyan{0xff00d4ff};
static const juce::Colour kAccentGold{0xfff59e0b};
static const juce::Colour kAccentRed{0xffef4444};
static const juce::Colour kAccentGreen{0xff10b981};
static const juce::Colour kAccentPurple{0xffa855f7};
static const juce::Colour kAccentBlue{0xff3b82f6};
static const juce::Colour kTextWhite{0xfff8fafc};
static const juce::Colour kTextMuted{0xff94a3b8};
} // namespace midas_colors

namespace midas_ui {
// Global font scale factor: default 1.30f ensures all console text is large,
// crisp, and readable on high-res displays
inline float fontScale = 1.30f;

inline juce::Font getFont(float baseSize, bool bold = false,
                          bool italic = false) {
  const float scaled = std::max(10.0f, baseSize * fontScale);
  auto f = juce::Font(
      juce::FontOptions(scaled, bold ? juce::Font::bold : juce::Font::plain));
  if (italic)
    f = f.italicised();
  return f;
}
} // namespace midas_ui

// ==============================================================================
// CUSTOM HARDWARE ROTARY KNOB
// ==============================================================================
class MidasKnob : public juce::Component {
public:
  enum class NotchColor { Cyan, Gold, Red, Blue, Green, Purple };

  MidasKnob(const juce::String &name, float minVal, float maxVal,
            float defaultVal, const juce::String &unit = "",
            NotchColor color = NotchColor::Cyan)
      : label(name), minValue(minVal), maxValue(maxVal),
        currentValue(defaultVal), unitSuffix(unit), notchCol(color) {
    setRepaintsOnMouseActivity(true);
  }

  std::function<void(float)> onValueChanged;

  void setValue(float v,
                juce::NotificationType notification = juce::sendNotification) {
    const float clamped = juce::jlimit(minValue, maxValue, v);
    if (std::abs(currentValue - clamped) > 0.0001f) {
      currentValue = clamped;
      repaint();
      if (notification != juce::dontSendNotification && onValueChanged)
        onValueChanged(currentValue);
    }
  }

  float getValue() const { return currentValue; }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float pad = 2.0f;
    const float knobH = area.getHeight() - 13.0f;
    const float knobW = area.getWidth();
    const float diameter =
        juce::jmax(8.0f, juce::jmin(knobW, knobH) - pad * 2.0f);
    const auto knobRect = juce::Rectangle<float>((knobW - diameter) * 0.5f, pad,
                                                 diameter, diameter);

    // Bevel Outer Ring
    g.setColour(juce::Colour(0xff05090f));
    g.fillEllipse(knobRect);
    g.setColour(midas_colors::kBorderCol);
    g.drawEllipse(knobRect, 1.0f);

    // Dial background
    auto dialRect = knobRect.reduced(2.0f);
    juce::ColourGradient grad(juce::Colour(0xff182436), dialRect.getCentreX(),
                              dialRect.getY(), juce::Colour(0xff090f18),
                              dialRect.getCentreX(), dialRect.getBottom(),
                              false);
    g.setGradientFill(grad);
    g.fillEllipse(dialRect);

    // Arc Track
    const float norm = (currentValue - minValue) / (maxValue - minValue);
    const float startAngle = juce::MathConstants<float>::pi * 1.25f;
    const float endAngle = juce::MathConstants<float>::pi * 2.75f;
    const float currentAngle = startAngle + norm * (endAngle - startAngle);

    juce::Path trackPath;
    trackPath.addCentredArc(dialRect.getCentreX(), dialRect.getCentreY(),
                            diameter * 0.42f, diameter * 0.42f, 0.0f,
                            startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff1e2e42));
    g.strokePath(trackPath,
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Active Arc
    juce::Colour activeCol = midas_colors::kAccentCyan;
    switch (notchCol) {
    case NotchColor::Gold:
      activeCol = midas_colors::kAccentGold;
      break;
    case NotchColor::Red:
      activeCol = midas_colors::kAccentRed;
      break;
    case NotchColor::Blue:
      activeCol = midas_colors::kAccentBlue;
      break;
    case NotchColor::Green:
      activeCol = midas_colors::kAccentGreen;
      break;
    case NotchColor::Purple:
      activeCol = midas_colors::kAccentPurple;
      break;
    default:
      break;
    }

    juce::Path activePath;
    activePath.addCentredArc(dialRect.getCentreX(), dialRect.getCentreY(),
                             diameter * 0.42f, diameter * 0.42f, 0.0f,
                             startAngle, currentAngle, true);
    g.setColour(activeCol);
    g.strokePath(activePath,
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Indicator Dot / Notch
    const float radius = diameter * 0.32f;
    const float dotX = dialRect.getCentreX() + std::sin(currentAngle) * radius;
    const float dotY = dialRect.getCentreY() - std::cos(currentAngle) * radius;
    g.setColour(activeCol);
    g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);

    // Label & Readout
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(midas_colors::kTextWhite);
    juce::String valStr;
    if (std::abs(currentValue) < 10.0f && (maxValue - minValue) <= 30.0f)
      valStr = juce::String(currentValue, 1);
    else
      valStr = juce::String(std::round(currentValue));

    if (unitSuffix.isNotEmpty())
      valStr << unitSuffix;

    g.drawText(valStr, 0, static_cast<int>(area.getHeight() - 13.0f),
               static_cast<int>(area.getWidth()), 12,
               juce::Justification::centred);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    dragStartY = e.position.y;
    valAtDragStart = currentValue;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float dragDist = dragStartY - e.position.y;
    const float range = maxValue - minValue;
    const float step = (e.mods.isShiftDown() ? 0.001f : 0.006f) * range;
    setValue(valAtDragStart + dragDist * step);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &w) override {
    const float range = maxValue - minValue;
    const float step = range * 0.05f;
    setValue(currentValue + (w.deltaY > 0 ? step : -step));
  }

private:
  juce::String label;
  float minValue, maxValue, currentValue;
  juce::String unitSuffix;
  NotchColor notchCol;
  float dragStartY = 0.0f;
  float valAtDragStart = 0.0f;
};

// ==============================================================================
// TACTILE ILLUMINATED BUTTON
// ==============================================================================
namespace midas_ui {
inline void drawConsoleScrew(juce::Graphics &g, float cx, float cy,
                             float radius = 3.5f) {
  g.setColour(juce::Colour(0xff121822));
  g.fillEllipse(cx - radius - 0.5f, cy - radius - 0.5f, (radius + 0.5f) * 2.0f,
                (radius + 0.5f) * 2.0f);
  juce::ColourGradient grad(juce::Colour(0xff6e7e90), cx - radius * 0.5f,
                            cy - radius * 0.5f, juce::Colour(0xff364454),
                            cx + radius * 0.5f, cy + radius * 0.5f, false);
  g.setGradientFill(grad);
  g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
  g.setColour(juce::Colour(0x60ffffff));
  g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 0.6f);
  g.setColour(juce::Colour(0xff161e28));
  g.drawLine(cx - radius * 0.6f, cy, cx + radius * 0.6f, cy, 1.0f);
  g.drawLine(cx, cy - radius * 0.6f, cx, cy + radius * 0.6f, 1.0f);
}
} // namespace midas_ui

class MidasTactileButton : public juce::Button {
public:
  enum class GlowMode { Cyan, Gold, Red, Purple, Green, Gray };

  MidasTactileButton(const juce::String &text, GlowMode glow = GlowMode::Cyan)
      : juce::Button(text), glowCol(glow) {
    setClickingTogglesState(true);
  }

  void setGlowMode(GlowMode g) {
    glowCol = g;
    repaint();
  }

  void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted,
                   bool shouldDrawButtonAsDown) override {
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    const bool on = getToggleState();

    juce::Colour activeGlow = midas_colors::kAccentCyan;
    switch (glowCol) {
    case GlowMode::Gold:
      activeGlow = midas_colors::kAccentGold;
      break;
    case GlowMode::Red:
      activeGlow = midas_colors::kAccentRed;
      break;
    case GlowMode::Purple:
      activeGlow = midas_colors::kAccentPurple;
      break;
    case GlowMode::Green:
      activeGlow = midas_colors::kAccentGreen;
      break;
    case GlowMode::Gray:
      activeGlow = juce::Colour(0xff64748b);
      break;
    default:
      break;
    }

    const float radius = std::min(area.getHeight() * 0.44f, 10.0f);

    if (on) {
      g.setColour(activeGlow.withAlpha(0.28f));
      g.fillRoundedRectangle(area.expanded(1.2f), radius + 1.0f);
      g.setColour(activeGlow);
      g.fillRoundedRectangle(area, radius);
      g.setColour(juce::Colour(0xff040810));
    } else {
      juce::Colour bg =
          shouldDrawButtonAsDown
              ? juce::Colour(0xff080e18)
              : (glowCol == GlowMode::Red    ? juce::Colour(0xff220a0f)
                 : glowCol == GlowMode::Gold ? juce::Colour(0xff221808)
                 : glowCol == GlowMode::Cyan
                     ? juce::Colour(0xff0f1a28)
                     : (shouldDrawButtonAsHighlighted
                            ? juce::Colour(0xff1e2e42)
                            : juce::Colour(0xff121d2b)));
      if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown)
        bg = bg.brighter(0.16f);
      g.setColour(bg);
      g.fillRoundedRectangle(area, radius);

      juce::Colour borderCol =
          (glowCol == GlowMode::Red)    ? juce::Colour(0xff551620)
          : (glowCol == GlowMode::Gold) ? juce::Colour(0xff593e0e)
          : (glowCol == GlowMode::Cyan) ? juce::Colour(0xff1c334d)
                                        : midas_colors::kBorderCol;
      g.setColour(borderCol);
      g.drawRoundedRectangle(area, radius, 1.0f);

      juce::Colour textCol =
          (glowCol == GlowMode::Red)    ? midas_colors::kAccentRed
          : (glowCol == GlowMode::Gold) ? midas_colors::kAccentGold
          : (glowCol == GlowMode::Cyan)
              ? midas_colors::kAccentCyan
              : (shouldDrawButtonAsHighlighted ? juce::Colours::white
                                               : midas_colors::kTextWhite);
      g.setColour(textCol);
    }

    g.setFont(midas_ui::getFont(9.0f, true));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
  }

private:
  GlowMode glowCol;
};

// ==============================================================================
// MIDAS ROTARY PAN / BALANCE KNOB
// ==============================================================================
class MidasPanKnob : public juce::Component {
public:
  MidasPanKnob(bool isMasterBalance = false) : isMaster(isMasterBalance) {
    setRepaintsOnMouseActivity(true);
  }

  std::function<void(float pan)> onValueChanged;

  void setValue(float v,
                juce::NotificationType notif = juce::sendNotification) {
    const float clamped = juce::jlimit(-1.0f, 1.0f, v);
    if (std::abs(panValue - clamped) > 0.001f) {
      panValue = clamped;
      repaint();
      if (notif != juce::dontSendNotification && onValueChanged)
        onValueChanged(panValue);
    }
  }

  float getValue() const { return panValue; }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float d = juce::jmin(area.getWidth(), area.getHeight()) - 2.0f;
    const auto knobRect =
        juce::Rectangle<float>((area.getWidth() - d) * 0.5f, 1.0f, d, d);

    // Bevel outer ring
    g.setColour(juce::Colour(0xff060a12));
    g.fillEllipse(knobRect);
    g.setColour(juce::Colour(0xff1e2e42));
    g.drawEllipse(knobRect, 1.2f);

    // Dial face gradient
    auto dialRect = knobRect.reduced(2.0f);
    juce::ColourGradient grad(juce::Colour(0xff1f2d3e), dialRect.getCentreX(),
                              dialRect.getY(), juce::Colour(0xff0a111a),
                              dialRect.getCentreX(), dialRect.getBottom(),
                              false);
    g.setGradientFill(grad);
    g.fillEllipse(dialRect);

    // Center tick / Indicator line
    const float angle = panValue * (juce::MathConstants<float>::pi * 0.75f);
    const float cx = dialRect.getCentreX();
    const float cy = dialRect.getCentreY();
    const float rInner = d * 0.18f;
    const float rOuter = d * 0.44f;

    const float sinA = std::sin(angle);
    const float cosA = std::cos(angle);

    const juce::Colour notchCol =
        isMaster ? midas_colors::kAccentGold : midas_colors::kAccentCyan;

    // Glowing notch tick
    g.setColour(notchCol.withAlpha(0.4f));
    g.drawLine(cx + sinA * rInner, cy - cosA * rInner,
               cx + sinA * (rOuter + 1.0f), cy - cosA * (rOuter + 1.0f), 3.5f);
    g.setColour(notchCol);
    g.drawLine(cx + sinA * rInner, cy - cosA * rInner, cx + sinA * rOuter,
               cy - cosA * rOuter, 2.0f);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
      setValue(0.0f);
      return;
    }
    dragStartY = e.position.y;
    valAtStart = panValue;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float delta = (dragStartY - e.position.y) / 80.0f;
    setValue(valAtStart + delta);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &w) override {
    setValue(panValue + w.deltaY * 0.1f);
  }

private:
  float panValue = 0.0f;
  float dragStartY = 0.0f;
  float valAtStart = 0.0f;
  bool isMaster = false;
};

// ==============================================================================
// 24-SEGMENT PRECISION LED METER LADDER
// ==============================================================================
class MidasMeterLadder : public juce::Component {
public:
  MidasMeterLadder() { setOpaque(false); }

  void setLevels(float dbL, float dbR = -999.0f) {
    levelL = dbL;
    levelR = (dbR > -900.0f) ? dbR : dbL;
    isStereo = (dbR > -900.0f);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const int segCount = 24;
    const float gap = 1.5f;
    const float segH = (area.getHeight() - (segCount + 1) * gap) /
                       static_cast<float>(segCount);

    static const float thresholds[24] = {
        -60.0f, -54.0f, -48.0f, -42.0f, -36.0f, -30.0f, -26.0f, -22.0f,
        -18.0f, -15.0f, -12.0f, -10.0f, -8.0f,  -6.0f,  -4.0f,  -2.0f,
        -1.0f,  0.0f,   1.5f,   3.0f,   4.5f,   6.0f,   8.0f,   10.0f};

    auto drawColumn = [&](float lvl, float colX, float colW) {
      for (int i = 0; i < segCount; ++i) {
        const float segY = area.getBottom() - (i + 1) * (segH + gap);
        const bool lit = lvl >= thresholds[i];

        juce::Colour segCol;
        if (i >= 19)
          segCol = midas_colors::kAccentRed;
        else if (i >= 14)
          segCol = midas_colors::kAccentGold;
        else
          segCol = midas_colors::kAccentGreen;

        if (lit) {
          g.setColour(segCol);
          g.fillRoundedRectangle(colX, segY, colW, segH, 1.0f);
        } else {
          g.setColour(segCol.withAlpha(0.12f));
          g.fillRoundedRectangle(colX, segY, colW, segH, 1.0f);
        }
      }
    };

    if (isStereo) {
      const float colW = (area.getWidth() - 3.0f) * 0.5f;
      drawColumn(levelL, 0.0f, colW);
      drawColumn(levelR, colW + 3.0f, colW);
    } else {
      drawColumn(levelL, 0.0f, area.getWidth());
    }
  }

private:
  float levelL = -60.0f;
  float levelR = -60.0f;
  bool isStereo = false;
};

// ==============================================================================
// 100mm MOTORIZED STUDIO FADER & RECESSED WELL
// ==============================================================================
class MidasFader : public juce::Component {
public:
  MidasFader() { setRepaintsOnMouseActivity(true); }

  std::function<void(float db)> onFaderMoved;
  bool isDraggingFader() const noexcept { return isDragging; }

  void setGainDb(float db,
                 juce::NotificationType notif = juce::sendNotification) {
    const float clamped = juce::jlimit(-90.0f, 10.0f, db);
    if (std::abs(currentDb - clamped) > 0.01f) {
      currentDb = clamped;
      repaint();
      if (notif != juce::dontSendNotification && onFaderMoved)
        onFaderMoved(currentDb);
    }
  }

  float getGainDb() const { return currentDb; }

  void setIsMaster(bool m) {
    isMaster = m;
    repaint();
  }

  void setFaderCapColor(juce::Colour col) {
    capAccent = col;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float wellW = juce::jlimit(28.0f, 38.0f, area.getWidth() - 2.0f);
    const float wellX = (area.getWidth() - wellW) * 0.5f;
    const float trackTop = 8.0f;
    const float trackBottom = area.getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;

    // 1. Recessed Fader Well Trench
    auto wellRect = juce::Rectangle<float>(wellX, trackTop, wellW, trackH);
    g.setColour(juce::Colour(0xff060a12));
    g.fillRoundedRectangle(wellRect, 6.0f);
    g.setColour(juce::Colour(0xff020406));
    g.drawRoundedRectangle(wellRect, 6.0f, 1.2f);
    g.setColour(juce::Colour(0x50000000));
    g.drawRoundedRectangle(wellRect.reduced(1.0f), 5.0f, 1.0f);

    // Mechanical Track Slit
    const float slitW = 2.5f;
    const float slitX = (area.getWidth() - slitW) * 0.5f;
    g.setColour(juce::Colour(0xff010204));
    g.fillRoundedRectangle(slitX, trackTop + 8.0f, slitW, trackH - 16.0f, 1.0f);

    // 2. Fader Cap
    const float normPos = dbToNormalized(currentDb);
    const float capH = 28.0f;
    const float capW = juce::jmin(area.getWidth(), wellW + 4.0f);
    const float capY = trackBottom - normPos * trackH - capH * 0.5f;
    const float capX = (area.getWidth() - capW) * 0.5f;

    auto capRect = juce::Rectangle<float>(capX, capY, capW, capH);

    // Deep Drop Shadow behind metallic cap
    g.setColour(juce::Colour(0xb8000000));
    g.fillRoundedRectangle(capRect.translated(0.0f, 3.5f), 4.0f);

    // Multi-stage Brushed Aluminum Metallic Gradient (Touring Spec)
    juce::ColourGradient capGrad(juce::Colour(0xffadbccb), capX, capY,
                                 juce::Colour(0xff4b5866), capX, capY + capH,
                                 false);
    capGrad.addColour(0.08, juce::Colour(0xffc2cedc)); // top chamfer highlight
    capGrad.addColour(0.35, juce::Colour(0xff8696a6)); // upper body
    capGrad.addColour(0.48, juce::Colour(0xff576675)); // groove upper bevel
    capGrad.addColour(0.52, juce::Colour(0xff3f4c59)); // groove center well
    capGrad.addColour(0.65, juce::Colour(0xff687888)); // groove lower bevel
    capGrad.addColour(0.85, juce::Colour(0xff525f6e)); // lower body
    capGrad.addColour(0.98, juce::Colour(0xff343e49)); // bottom chamfer shadow
    g.setGradientFill(capGrad);
    g.fillRoundedRectangle(capRect, 3.5f);

    // Tactile Ergonomic Finger Indentation Bevels
    const float grooveY = capY + capH * 0.5f;
    g.setColour(juce::Colour(0x60000000));
    g.drawHorizontalLine(static_cast<int>(grooveY - 3.0f), capX + 2.0f,
                         capX + capW - 2.0f);
    g.setColour(juce::Colour(0x35ffffff));
    g.drawHorizontalLine(static_cast<int>(grooveY + 4.0f), capX + 2.0f,
                         capX + capW - 2.0f);

    // Top Bevel Specular Line
    g.setColour(juce::Colour(0xd0ffffff));
    g.drawHorizontalLine(static_cast<int>(capY + 1.0f), capX + 3.0f,
                         capX + capW - 3.0f);

    // Lateral Chamfers / Rim Highlight
    g.setColour(juce::Colour(0x60ffffff));
    g.drawVerticalLine(static_cast<int>(capX + 1.0f), capY + 3.0f,
                       capY + capH - 3.0f);
    g.setColour(juce::Colour(0x35000000));
    g.drawVerticalLine(static_cast<int>(capX + capW - 1.0f), capY + 3.0f,
                       capY + capH - 3.0f);

    // Cap Outline
    g.setColour(juce::Colour(0xff222a33));
    g.drawRoundedRectangle(capRect, 3.5f, 1.0f);

    // Signature Glowing Center Stripe (Crisp White for Channel Faders, Amber
    // for Master)
    const float lineY = capY + capH * 0.5f;
    const float lineW = capW - 8.0f;
    const float lineX = capX + 4.0f;

    if (isMaster) {
      g.setColour(juce::Colour(0x70ffb300));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(juce::Colour(0xffffb300));
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    } else {
      g.setColour(juce::Colour(0x70ffffff));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(juce::Colours::white);
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
      setGainDb(0.0f);
      return;
    }
    isDragging = true;
    const float trackTop = 8.0f;
    const float trackBottom = getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;
    const float norm =
        juce::jlimit(0.0f, 1.0f, (trackBottom - e.position.y) / trackH);
    setGainDb(normalizedToDb(norm));
    dragStartY = e.position.y;
    dbAtStart = currentDb;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    isDragging = true;
    const float trackTop = 8.0f;
    const float trackBottom = getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;
    const float deltaNorm = (dragStartY - e.position.y) / trackH;
    const float startNorm = dbToNormalized(dbAtStart);
    setGainDb(normalizedToDb(startNorm + deltaNorm));
  }

  void mouseUp(const juce::MouseEvent &) override { isDragging = false; }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &w) override {
    setGainDb(currentDb + w.deltaY * 0.5f);
  }

private:
  float currentDb = 0.0f;
  float dragStartY = 0.0f;
  float dbAtStart = 0.0f;
  bool isDragging = false;
  bool isMaster = false;
  juce::Colour capAccent = midas_colors::kAccentCyan;

  static float dbToNormalized(float db) {
    if (db <= -90.0f)
      return 0.0f;
    if (db >= 10.0f)
      return 1.0f;
    if (db >= 0.0f)
      return 0.75f + (db / 10.0f) * 0.25f;
    return std::pow((db + 90.0f) / 90.0f, 2.2f) * 0.75f;
  }

  static float normalizedToDb(float norm) {
    const float c = juce::jlimit(0.0f, 1.0f, norm);
    if (c <= 0.0001f)
      return -90.0f;
    if (c >= 0.75f)
      return ((c - 0.75f) / 0.25f) * 10.0f;
    return std::pow(c / 0.75f, 1.0f / 2.2f) * 90.0f - 90.0f;
  }
};
// ==============================================================================
// 1. OVERVIEW VIEW (144-CHANNEL PEAK METER BRIDGE & CONNECTIVITY DASHBOARD)
// ==============================================================================
struct OverviewTelemetrySnapshot {
  bool audioDeviceValid = false;
  juce::String audioDeviceType;
  juce::String audioDeviceName;
  double sampleRate = 0.0;
  int bufferSize = 0;
  int inputChannelsActive = 0;
  int outputChannelsActive = 0;
  uint64_t callbackCount = 0;

  int totalChannels = nova::kLogicalChannels;
  int mutedCount = 0;
  int soloCount = 0;
  int auxBusCount = nova::kAuxBusCapacity;
  int matrixBusCount = nova::kMatrixBusCapacity;
  int fxActiveCount = 0;
  int fxTotalSlots = 24;
  int vcaAssignedCount = 0;
  int linkGroupsActive = 0;

  std::array<float, nova::kLogicalChannels> chPeaksDb{};
  float masterPeakL = -120.0f;
  float masterPeakR = -120.0f;
  float auxMaxPeakDb = -120.0f;
  float matrixMaxPeakDb = -120.0f;
  float fxReturnMaxPeakDb = -120.0f;

  bool monitorWasapi2Open = false;
  juce::String monitorDeviceName;
  float monitorPeakL = -120.0f;
  float monitorPeakR = -120.0f;
  float monitorFifoLatencyMs = 0.0f;

  bool recorderActive = false;
  bool recorderMultitrack = false;
  bool oscRunning = false;
};

class MidasOverviewView : public juce::Component {
public:
  MidasOverviewView() {
    setOpaque(false);
    snapshot.chPeaksDb.fill(-120.0f);
  }

  void updateSnapshot(const OverviewTelemetrySnapshot &next) {
    snapshot = next;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff09121f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    auto inner = area.reduced(10.0f, 6.0f);

    // Header Row
    auto hdr = inner.removeFromTop(20.0f);
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("OPENLIVE HD - CONSOLE OVERVIEW", hdr.toNearestInt(),
               juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(9.5f, true));
    if (snapshot.audioDeviceValid) {
      const double latMs = (snapshot.sampleRate > 1000.0)
                               ? (static_cast<double>(snapshot.bufferSize) /
                                  snapshot.sampleRate) *
                                     1000.0
                               : 0.0;
      g.setColour(midas_colors::kAccentGreen);
      g.drawText("AUDIO ENGINE ACTIVE  |  " +
                     juce::String(snapshot.sampleRate / 1000.0, 1) +
                     " kHz  |  " + juce::String(snapshot.bufferSize) +
                     " SMP (" + juce::String(latMs, 1) + " ms)",
                 hdr.toNearestInt(), juce::Justification::centredRight);
    } else {
      g.setColour(midas_colors::kAccentGold);
      g.drawText("AUDIO ENGINE IDLE", hdr.toNearestInt(),
                 juce::Justification::centredRight);
    }

    inner.removeFromTop(4.0f);

    // Row 1: Dynamic Console State Counters (only real, verified counts)
    struct StatCard {
      juce::String label;
      juce::String value;
      juce::Colour col;
    };
    std::vector<StatCard> statCards;
    statCards.push_back({"INPUTS", juce::String(snapshot.totalChannels),
                         midas_colors::kAccentCyan});
    statCards.push_back({"MUTED", juce::String(snapshot.mutedCount),
                         snapshot.mutedCount > 0 ? midas_colors::kAccentRed
                                                 : midas_colors::kAccentGold});
    statCards.push_back({"SOLO", juce::String(snapshot.soloCount),
                         snapshot.soloCount > 0 ? midas_colors::kAccentGold
                                                : midas_colors::kAccentGreen});
    statCards.push_back({"AUX / MTX",
                         juce::String(snapshot.auxBusCount) + " / " +
                             juce::String(snapshot.matrixBusCount),
                         midas_colors::kAccentCyan});
    statCards.push_back({"FX ACTIVE",
                         juce::String(snapshot.fxActiveCount) + "/" +
                             juce::String(snapshot.fxTotalSlots),
                         midas_colors::kAccentPurple});
    if (snapshot.vcaAssignedCount > 0 || snapshot.linkGroupsActive > 0) {
      statCards.push_back({"VCA / LINK",
                           juce::String(snapshot.vcaAssignedCount) + " VCA | " +
                               juce::String(snapshot.linkGroupsActive) + " GRP",
                           midas_colors::kAccentGold});
    }

    const float statH = juce::jlimit(36.0f, 44.0f, inner.getHeight() * 0.16f);
    auto statRow = inner.removeFromTop(statH);
    const float statGap = 6.0f;
    const float statW = (statRow.getWidth() -
                         statGap * static_cast<float>(statCards.size() - 1)) /
                        static_cast<float>(statCards.size());
    for (size_t i = 0; i < statCards.size(); ++i) {
      auto card = juce::Rectangle<float>(
          statRow.getX() + static_cast<float>(i) * (statW + statGap),
          statRow.getY(), statW, statRow.getHeight());
      g.setColour(juce::Colour(0xff060d16));
      g.fillRoundedRectangle(card, 4.0f);
      g.setColour(midas_colors::kBorderSubtle);
      g.drawRoundedRectangle(card, 4.0f, 1.0f);

      auto cIn = card.reduced(8.0f, 4.0f);
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(midas_colors::kTextMuted);
      g.drawText(statCards[i].label, cIn.removeFromTop(12.0f).toNearestInt(),
                 juce::Justification::centredLeft);
      g.setFont(midas_ui::getFont(14.0f, true));
      g.setColour(statCards[i].col);
      g.drawText(statCards[i].value, cIn.toNearestInt(),
                 juce::Justification::centredLeft);
    }

    inner.removeFromTop(6.0f);

    // Bottom Dynamic Subsystem Cards (only active/connected services!)
    struct SysCard {
      juce::String title;
      juce::String line1;
      juce::String line2;
      juce::Colour accent;
    };
    std::vector<SysCard> sysCards;
    if (snapshot.audioDeviceValid) {
      sysCards.push_back(
          {"AUDIO HARDWARE",
           snapshot.audioDeviceType + "  (" +
               juce::String(snapshot.inputChannelsActive) + " IN / " +
               juce::String(snapshot.outputChannelsActive) + " OUT)",
           snapshot.audioDeviceName, midas_colors::kAccentCyan});
    }
    if (snapshot.monitorWasapi2Open) {
      sysCards.push_back(
          {"MONITOR WASAPI 2", snapshot.monitorDeviceName,
           "FIFO JITTER: " + juce::String(snapshot.monitorFifoLatencyMs, 1) +
               " ms",
           midas_colors::kAccentGreen});
    }
    if (snapshot.recorderActive) {
      sysCards.push_back(
          {"WAV RECORDER",
           snapshot.recorderMultitrack ? "64-CH MULTITRACK RECORDING"
                                       : "STEREO MAIN RECORDING",
           "NON-BLOCKING FIFO ACTIVE", midas_colors::kAccentRed});
    }
    if (snapshot.oscRunning) {
      sysCards.push_back({"OSC SERVER", "UDP REMOTE LISTENING",
                          "IPAD / CONSOLE SYNC", midas_colors::kAccentPurple});
    }

    juce::Rectangle<float> sysRow;
    if (!sysCards.empty()) {
      const float sysH = juce::jlimit(36.0f, 46.0f, inner.getHeight() * 0.22f);
      sysRow = inner.removeFromBottom(sysH);
      inner.removeFromBottom(6.0f);
    }

    // Live Bus Output Meters (only when audio device is active!)
    struct MeterCard {
      juce::String label;
      float dbL = -120.0f;
      float dbR = -120.0f;
      bool stereo = false;
      juce::Colour col;
    };
    std::vector<MeterCard> meterCards;
    if (snapshot.audioDeviceValid) {
      meterCards.push_back({"MASTER L/R", snapshot.masterPeakL,
                            snapshot.masterPeakR, true,
                            midas_colors::kAccentCyan});
      meterCards.push_back({"MIX BUS (AUX)", snapshot.auxMaxPeakDb,
                            snapshot.auxMaxPeakDb, false,
                            midas_colors::kAccentGreen});
      meterCards.push_back({"MATRIX PEAK", snapshot.matrixMaxPeakDb,
                            snapshot.matrixMaxPeakDb, false,
                            midas_colors::kAccentGold});
      if (snapshot.fxActiveCount > 0) {
        meterCards.push_back({"FX RETURN", snapshot.fxReturnMaxPeakDb,
                              snapshot.fxReturnMaxPeakDb, false,
                              midas_colors::kAccentPurple});
      }
      if (snapshot.monitorWasapi2Open) {
        meterCards.push_back({"MONITOR A", snapshot.monitorPeakL,
                              snapshot.monitorPeakR, true,
                              midas_colors::kAccentCyan});
      }
    }

    juce::Rectangle<float> meterRow;
    if (!meterCards.empty()) {
      const float mH = juce::jlimit(34.0f, 42.0f, inner.getHeight() * 0.22f);
      meterRow = inner.removeFromBottom(mH);
      inner.removeFromBottom(6.0f);
    }

    // Center Area: 32-Channel Peak Meter Bridge (only when Audio Engine Active)
    if (snapshot.audioDeviceValid && inner.getHeight() >= 40.0f) {
      g.setColour(juce::Colour(0xff060d16));
      g.fillRoundedRectangle(inner, 4.0f);
      g.setColour(midas_colors::kBorderSubtle);
      g.drawRoundedRectangle(inner, 4.0f, 1.0f);

      auto bIn = inner.reduced(8.0f, 4.0f);
      auto bHdr = bIn.removeFromTop(14.0f);
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(midas_colors::kTextMuted);
      g.drawText(juce::String(nova::kLogicalChannels) +
                     " INPUT CHANNEL PEAK METERS  |  4 GROUPS x 8 CHANNELS",
                 bHdr.toNearestInt(), juce::Justification::centredLeft);
      g.setColour(midas_colors::kAccentGreen);
      g.drawText("LIVE TELEMETRY (" + juce::String(snapshot.callbackCount) +
                     " CB)",
                 bHdr.toNearestInt(), juce::Justification::centredRight);

      bIn.removeFromTop(3.0f);
      const float grpGap = 6.0f;
      const float grpW = (bIn.getWidth() - grpGap * 3.0f) / 4.0f;
      for (int grp = 0; grp < 4; ++grp) {
        auto gBox = juce::Rectangle<float>(
            bIn.getX() + static_cast<float>(grp) * (grpW + grpGap), bIn.getY(),
            grpW, bIn.getHeight());
        g.setColour(juce::Colour(0xff0a1320));
        g.fillRoundedRectangle(gBox, 3.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(gBox, 3.0f, 0.8f);

        auto lblRect = gBox.removeFromBottom(12.0f);
        g.setFont(midas_ui::getFont(7.5f, true));
        g.setColour(midas_colors::kTextMuted);
        g.drawText("CH " + juce::String(grp * 8 + 1) + "-" +
                       juce::String((grp + 1) * 8),
                   lblRect.toNearestInt(), juce::Justification::centred);

        auto mArea = gBox.reduced(4.0f, 3.0f);
        const float barW =
            std::max(2.0f, (mArea.getWidth() - 7.0f * 2.0f) / 8.0f);
        for (int c = 0; c < 8; ++c) {
          const int chIdx = grp * 8 + c;
          const float mx = mArea.getX() + static_cast<float>(c) * (barW + 2.0f);
          g.setColour(juce::Colour(0xff0f1b2d));
          g.fillRect(mx, mArea.getY(), barW, mArea.getHeight());

          const float db = (chIdx < nova::kLogicalChannels)
                               ? snapshot.chPeaksDb[static_cast<size_t>(chIdx)]
                               : -120.0f;
          const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
          if (norm > 0.015f) {
            const float bh = norm * mArea.getHeight();
            g.setColour(norm > 0.88f
                            ? midas_colors::kAccentRed
                            : (norm > 0.65f ? midas_colors::kAccentGold
                                            : midas_colors::kAccentGreen));
            g.fillRect(mx, mArea.getBottom() - bh, barW, bh);
          }
        }
      }
    } else if (!snapshot.audioDeviceValid && inner.getHeight() >= 30.0f) {
      g.setColour(juce::Colour(0xff060d16));
      g.fillRoundedRectangle(inner, 4.0f);
      g.setColour(midas_colors::kAccentGold.withAlpha(0.45f));
      g.drawRoundedRectangle(inner, 4.0f, 1.0f);
      g.setFont(midas_ui::getFont(10.5f, true));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("Audio Engine sedang idle - buka 5. SYSTEM > AUDIO HARDWARE "
                 "untuk mengaktifkan perangkat audio.",
                 inner.reduced(12.0f).toNearestInt(),
                 juce::Justification::centred, true);
    }

    // Draw Live Bus Meter Cards
    if (!meterCards.empty()) {
      const float mGap = 6.0f;
      const float mW = (meterRow.getWidth() -
                        mGap * static_cast<float>(meterCards.size() - 1)) /
                       static_cast<float>(meterCards.size());
      for (size_t i = 0; i < meterCards.size(); ++i) {
        auto mBox = juce::Rectangle<float>(
            meterRow.getX() + static_cast<float>(i) * (mW + mGap),
            meterRow.getY(), mW, meterRow.getHeight());
        g.setColour(juce::Colour(0xff060d16));
        g.fillRoundedRectangle(mBox, 4.0f);
        g.setColour(meterCards[i].col.withAlpha(0.45f));
        g.drawRoundedRectangle(mBox, 4.0f, 1.0f);

        auto mIn = mBox.reduced(8.0f, 4.0f);
        auto topR = mIn.removeFromTop(13.0f);
        g.setFont(midas_ui::getFont(8.5f, true));
        g.setColour(meterCards[i].col);
        g.drawText(meterCards[i].label, topR.toNearestInt(),
                   juce::Justification::centredLeft);

        const float maxDb = std::max(meterCards[i].dbL, meterCards[i].dbR);
        g.setColour(midas_colors::kTextWhite);
        g.drawText(maxDb <= -90.0f ? "-INF" : juce::String(maxDb, 1) + " dB",
                   topR.toNearestInt(), juce::Justification::centredRight);

        mIn.removeFromTop(3.0f);
        auto drawBar = [&](juce::Rectangle<float> r, float dbVal) {
          g.setColour(juce::Colour(0xff101c2e));
          g.fillRoundedRectangle(r, 2.0f);
          const float norm = juce::jlimit(0.0f, 1.0f, (dbVal + 60.0f) / 66.0f);
          if (norm > 0.01f) {
            g.setColour(norm > 0.88f ? midas_colors::kAccentRed
                                     : (norm > 0.65f ? midas_colors::kAccentGold
                                                     : meterCards[i].col));
            g.fillRoundedRectangle(r.withWidth(r.getWidth() * norm), 2.0f);
          }
        };
        if (meterCards[i].stereo) {
          const float barH = std::max(3.0f, (mIn.getHeight() - 2.0f) * 0.5f);
          drawBar(mIn.removeFromTop(barH), meterCards[i].dbL);
          mIn.removeFromTop(2.0f);
          drawBar(mIn.removeFromTop(barH), meterCards[i].dbR);
        } else {
          drawBar(mIn.withHeight(std::min(7.0f, mIn.getHeight())),
                  meterCards[i].dbL);
        }
      }
    }

    // Draw Active Subsystem Cards
    if (!sysCards.empty()) {
      const float sGap = 6.0f;
      const float sW =
          (sysRow.getWidth() - sGap * static_cast<float>(sysCards.size() - 1)) /
          static_cast<float>(sysCards.size());
      for (size_t i = 0; i < sysCards.size(); ++i) {
        auto sBox = juce::Rectangle<float>(
            sysRow.getX() + static_cast<float>(i) * (sW + sGap), sysRow.getY(),
            sW, sysRow.getHeight());
        g.setColour(juce::Colour(0xff060d16));
        g.fillRoundedRectangle(sBox, 4.0f);
        g.setColour(sysCards[i].accent.withAlpha(0.45f));
        g.drawRoundedRectangle(sBox, 4.0f, 1.0f);

        auto sIn = sBox.reduced(8.0f, 3.0f);
        g.setFont(midas_ui::getFont(8.0f, true));
        g.setColour(sysCards[i].accent);
        g.drawText(sysCards[i].title, sIn.removeFromTop(11.0f).toNearestInt(),
                   juce::Justification::centredLeft, true);
        g.setFont(midas_ui::getFont(8.5f, true));
        g.setColour(midas_colors::kTextWhite);
        g.drawText(sysCards[i].line1, sIn.removeFromTop(12.0f).toNearestInt(),
                   juce::Justification::centredLeft, true);
        g.setFont(midas_ui::getFont(7.8f, false));
        g.setColour(midas_colors::kTextMuted);
        g.drawText(sysCards[i].line2, sIn.toNearestInt(),
                   juce::Justification::centredLeft, true);
      }
    }
  }

private:
  OverviewTelemetrySnapshot snapshot;
};

// ==============================================================================
// CONSOLE HOME DIRECTORY, GLOBAL SEARCH, QUICK ACCESS, FAVORITES & RECENT
// ==============================================================================
class AndroidHomeLauncherView final : public juce::Component {
public:
  struct PageDef {
    int groupIdx;
    int subPage;
    const char *groupName;
    const char *badge;
    const char *title;
    const char *subtitle;
    const char *keywords;
    juce::uint32 colorHex;
  };

  static const std::vector<PageDef> &getCatalog() {
    static const std::vector<PageDef> kCatalog = {
        // 0: MIX
        {0, 0, "MIX", "OV", "Overview", "Meter & Status Konsol",
         "overview meter status console mix home", 0xff38bdf8},
        {0, 1, "MIX", "EQ", "Equaliser (PEQ & DYN)",
         "4-Band PEQ, Dynamic EQ & RTA",
         "eq equaliser peq dynamic rta filter hpf lpf frequency", 0xfff59e0b},
        {0, 2, "MIX", "DY", "Comp & Gate", "Compressor & Noise Gate",
         "dynamics comp compressor gate threshold ratio attack release",
         0xffa855f7},
        {0, 3, "MIX", "AI", "Midas AI", "Adaptive Channel AI Assist",
         "midas ai channel auto eq assist learn smart", 0xff10b981},
        {0, 4, "MIX", "DG", "Dugan Automixer", "16-Ch Speech Automixer",
         "dugan automixer speech mic gain sharing conference", 0xfffacc15},
        {0, 5, "MIX", "MN", "Monitor & Talkback",
         "Control Room, Solo, IEM & Talkback",
         "monitor talkback solo cue phones intercom cr iem comms", 0xff38bdf8},
        // 1: ROUTING
        {1, 0, "ROUTING", "PM", "Patch Matrix",
         "Stagebox & Crosspoint Routing",
         "patch matrix stagebox input output crosspoint routing", 0xff10b981},
        {1, 1, "ROUTING", "3D", "3D Surround", "Immersive 5.1 / LFE Panning",
         "3d surround panning lfe immersive spatial xyz", 0xff38bdf8},
        {1, 2, "ROUTING", "VC", "VCA Groups",
         "24 VCA & POPulation Groups",
         "vca pop population groups spill fader master", 0xfff59e0b},
        {1, 3, "ROUTING", "BS", "Bus & Direct",
         "96 AUX / IEM Mix & Matrix Sends",
         "bus direct aux iem monitor wedge mix send matrix", 0xff0ea5e9},
        {1, 4, "ROUTING", "GF", "Gang Fader",
         "16-Group Channel Link & Ganging",
         "gang fader link group channel relative absolute", 0xff38bdf8},
        // 2: FX
        {2, 0, "FX", "FX", "24-Slot Rack", "Virtual FX Rack & Reverbs",
         "fx rack reverb delay chorus pitch plate dvr2 nebula effects",
         0xfff59e0b},
        {2, 1, "FX", "MD", "DSP Models", "Vintage Comp & EQ Models",
         "dsp models vintage emt140 kt1176 tc2290 hardware", 0xff38bdf8},
        // 3: SHOW
        {3, 0, "SHOW", "CU", "Cue List", "Scene Automation & Recall",
         "cue list scene snapshot recall automation show", 0xff10b981},
        {3, 1, "SHOW", "RC", "64-Ch Recorder",
         "Multitrack WAV & Virtual Soundcheck",
         "recorder multitrack wav 64ch usb daw virtual soundcheck record",
         0xff38bdf8},
        {3, 2, "SHOW", "BN", "Offline Bounce", "High-Speed Offline Render",
         "offline bounce export render stem wav mixdown", 0xffa855f7},
        {3, 3, "SHOW", "SC", "Scene Overview", "Quick Scene Tiles",
         "scene overview quick recall tiles snapshot", 0xfff59e0b},
        {3, 4, "SHOW", "BC", "Broadcast Stems", "EBU R128 & Broadcast Bus",
         "broadcast stems lufs ebu r128 streaming feed", 0xfff59e0b},
        {3, 5, "SHOW", "MC", "mCloud Sync",
         "Cloud Showfile Sync & Backup",
         "mcloud sync cloud showfile backup archive export", 0xff38bdf8},
        // 4: SYSTEM
        {4, 0, "SYSTEM", "AS", "ASIO Bridge", "Low-Latency ASIO Streamer",
         "asio bridge driver low latency daw streamer", 0xff38bdf8},
        {4, 1, "SYSTEM", "HW", "Audio Hardware",
         "Device, Sample Rate & Buffer",
         "audio hardware device soundcard sample rate buffer wasapi asio",
         0xffa855f7},
        {4, 2, "SYSTEM", "CB", "Virtual Cable",
         "VBAN UDP (6980) & 4x Stereo IPC",
         "virtual cable heritage vban udp shm voicemeeter loopback windows",
         0xff38bdf8},
        {4, 3, "SYSTEM", "OS", "OSC Server",
         "Android Remote & UDP 53000",
         "osc server remote android wifi tablet udp 53000 mirror", 0xff10b981},
        {4, 4, "SYSTEM", "ST", "Console Setup",
         "Preferences & Signal Generator",
         "console setup settings preferences oscillator signal generator",
         0xff0ea5e9},
    };
    return kCatalog;
  }

  static std::vector<const PageDef *>
  findPagesMatching(const juce::String &rawQuery) {
    const auto q = rawQuery.trim().toLowerCase();
    std::vector<std::pair<int, const PageDef *>> scored;
    if (q.isEmpty()) {
      for (const auto &p : getCatalog())
        scored.push_back({0, &p});
    } else {
      juce::StringArray tokens;
      tokens.addTokens(q, " ", "\"");
      tokens.removeEmptyStrings();
      for (const auto &p : getCatalog()) {
        const juce::String titleL = juce::String(p.title).toLowerCase();
        const juce::String grpL = juce::String(p.groupName).toLowerCase();
        const juce::String subL = juce::String(p.subtitle).toLowerCase();
        const juce::String kwL = juce::String(p.keywords).toLowerCase();
        const juce::String all =
            grpL + " " + titleL + " " + subL + " " + kwL;

        bool allMatch = true;
        for (const auto &tok : tokens) {
          if (!all.contains(tok)) {
            allMatch = false;
            break;
          }
        }
        if (!allMatch)
          continue;

        int score = 10;
        if (titleL == q)
          score = 100;
        else if (titleL.startsWith(q))
          score = 80;
        else if (titleL.contains(q))
          score = 60;
        else if (grpL.contains(q))
          score = 40;
        else if (subL.contains(q))
          score = 30;
        scored.push_back({score, &p});
      }
      std::stable_sort(scored.begin(), scored.end(),
                       [](const auto &a, const auto &b) {
                         return a.first > b.first;
                       });
    }
    std::vector<const PageDef *> out;
    out.reserve(scored.size());
    for (const auto &item : scored)
      out.push_back(item.second);
    return out;
  }

  static int findCatalogIndex(int origGroupIdx, int subPage) {
    const auto &cat = getCatalog();
    for (size_t i = 0; i < cat.size(); ++i) {
      if (cat[i].groupIdx == origGroupIdx && cat[i].subPage == subPage)
        return static_cast<int>(i);
    }
    return -1;
  }

  AndroidHomeLauncherView() {
    setOpaque(false);

    searchBox.setTextToShowWhenEmpty(
        "Cari fitur (mis: virtual cable, IEM, EQ, recorder, osc)...",
        juce::Colour(0xff64748b));
    searchBox.setColour(juce::TextEditor::backgroundColourId,
                        juce::Colour(0xff111927));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId,
                        juce::Colour(0xff2b3c56));
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId,
                        juce::Colour(0xff38bdf8));
    searchBox.setFont(midas_ui::getFont(11.0f, false));
    searchBox.onTextChange = [this] {
      hoveredKind = -1;
      hoveredIndex = -1;
      clearDragState();
      clearSearchBtn.setVisible(searchBox.getText().isNotEmpty());
      repaint();
    };
    searchBox.onReturnKey = [this] {
      const auto matches = findPagesMatching(searchBox.getText());
      if (!matches.empty() && onPageSelected) {
        onPageSelected(matches[0]->groupIdx, matches[0]->subPage);
      }
    };
    addAndMakeVisible(searchBox);

    clearSearchBtn.setButtonText("X");
    clearSearchBtn.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1e293b));
    clearSearchBtn.setColour(juce::TextButton::textColourOffId,
                             juce::Colours::white);
    clearSearchBtn.onClick = [this] { clearSearch(); };
    addChildComponent(clearSearchBtn);

    resetLayoutBtn.setButtonText("RESET POSISI");
    resetLayoutBtn.setTooltip(
        "Kembalikan posisi semua modul dan urutan kolom ke standar");
    resetLayoutBtn.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1e293b));
    resetLayoutBtn.setColour(juce::TextButton::textColourOffId,
                             juce::Colour(0xff94a3b8));
    resetLayoutBtn.onClick = [this] { resetLayoutToDefault(true); };
    addAndMakeVisible(resetLayoutBtn);

    loadLayoutFromDisk();
  }

  std::function<void(int groupIdx, int subPage)> onPageSelected;
  std::function<void(int appIdx)> onAppSelected;
  std::function<void(int groupIdx, int subPage)> onToggleFavorite;
  std::function<void()> onLayoutChanged;

  void setActivePage(int groupIdx, int subPage) {
    activeGroupIdx = groupIdx;
    activeSubPageIdx = subPage;
    repaint();
  }

  void setFavorites(const std::vector<std::pair<int, int>> &favs) {
    favoriteKeys = favs;
    repaint();
  }

  void clearSearch() {
    searchBox.setText("", juce::dontSendNotification);
    clearSearchBtn.setVisible(false);
    hoveredKind = -1;
    hoveredIndex = -1;
    repaint();
  }

  void focusSearch() {
    if (isShowing())
      searchBox.grabKeyboardFocus();
  }

  void resetLayoutToDefault(bool save = true) {
    columnOrder = {0, 1, 2, 3, 4};
    for (auto &vec : columnItems)
      vec.clear();
    const auto &cat = getCatalog();
    for (size_t i = 0; i < cat.size(); ++i) {
      const int g = juce::jlimit(0, 4, cat[i].groupIdx);
      columnItems[static_cast<size_t>(g)].push_back(static_cast<int>(i));
    }
    clearDragState();
    if (save)
      saveLayoutToDisk();
    updateResetButtonAppearance();
    if (onLayoutChanged)
      onLayoutChanged();
    repaint();
  }

  bool moveCatalogItem(int catalogIdx, int targetGroupIdx, int targetRowIdx,
                       bool save = true) {
    const auto &cat = getCatalog();
    if (catalogIdx < 0 || catalogIdx >= static_cast<int>(cat.size()) ||
        targetGroupIdx < 0 || targetGroupIdx >= 5)
      return false;

    int srcGrp = -1;
    int srcRow = -1;
    for (int g = 0; g < 5; ++g) {
      const auto &vec = columnItems[static_cast<size_t>(g)];
      for (size_t r = 0; r < vec.size(); ++r) {
        if (vec[r] == catalogIdx) {
          srcGrp = g;
          srcRow = static_cast<int>(r);
          break;
        }
      }
      if (srcGrp >= 0)
        break;
    }

    if (srcGrp >= 0 && srcRow >= 0) {
      auto &srcVec = columnItems[static_cast<size_t>(srcGrp)];
      srcVec.erase(srcVec.begin() + srcRow);
      if (srcGrp == targetGroupIdx && srcRow < targetRowIdx)
        --targetRowIdx;
    }

    auto &dstVec = columnItems[static_cast<size_t>(targetGroupIdx)];
    const int clampedRow =
        juce::jlimit(0, static_cast<int>(dstVec.size()), targetRowIdx);
    dstVec.insert(dstVec.begin() + clampedRow, catalogIdx);

    if (save)
      saveLayoutToDisk();
    updateResetButtonAppearance();
    if (onLayoutChanged)
      onLayoutChanged();
    repaint();
    return true;
  }

  bool swapVisualColumns(int visColA, int visColB, bool save = true) {
    if (visColA < 0 || visColA >= 5 || visColB < 0 || visColB >= 5)
      return false;
    std::swap(columnOrder[static_cast<size_t>(visColA)],
              columnOrder[static_cast<size_t>(visColB)]);
    if (save)
      saveLayoutToDisk();
    updateResetButtonAppearance();
    if (onLayoutChanged)
      onLayoutChanged();
    repaint();
    return true;
  }

  int getAssignedGroupForCatalogIndex(int catalogIdx) const {
    for (int g = 0; g < 5; ++g) {
      for (int idx : columnItems[static_cast<size_t>(g)]) {
        if (idx == catalogIdx)
          return g;
      }
    }
    const auto &cat = getCatalog();
    if (catalogIdx >= 0 && catalogIdx < static_cast<int>(cat.size()))
      return cat[static_cast<size_t>(catalogIdx)].groupIdx;
    return -1;
  }

  int getAssignedGroupForPage(int origGroupIdx, int subPage) const {
    const int idx = findCatalogIndex(origGroupIdx, subPage);
    if (idx < 0)
      return -1;
    return getAssignedGroupForCatalogIndex(idx);
  }

  const std::vector<int> &getItemsInGroup(int groupIdx) const {
    return columnItems[static_cast<size_t>(juce::jlimit(0, 4, groupIdx))];
  }

  const std::array<int, 5> &getColumnOrder() const { return columnOrder; }

  bool isDefaultLayout() const {
    if (columnOrder != std::array<int, 5>{0, 1, 2, 3, 4})
      return false;
    const auto &cat = getCatalog();
    std::array<std::vector<int>, 5> expected;
    for (size_t i = 0; i < cat.size(); ++i) {
      expected[static_cast<size_t>(juce::jlimit(0, 4, cat[i].groupIdx))]
          .push_back(static_cast<int>(i));
    }
    return columnItems == expected;
  }

  void resized() override {
    auto inner = getLocalBounds().reduced(12, 8);
    auto hdr = inner.removeFromTop(26);
    hdr.removeFromLeft(195); // Room for CONSOLE DIRECTORY title
    resetLayoutBtn.setBounds(hdr.removeFromRight(92).reduced(2, 1));
    hdr.removeFromRight(4);
    if (clearSearchBtn.isVisible())
      clearSearchBtn.setBounds(hdr.removeFromRight(26).reduced(1));
    else
      clearSearchBtn.setBounds(hdr.getRight() - 26, hdr.getY(), 24, 24);
    searchBox.setBounds(hdr.reduced(2, 1));
  }

  void mouseMove(const juce::MouseEvent &e) override {
    if (isDraggingItem || isDraggingColumn)
      return;
    const auto hit = hitTest(e.position);
    if (hit.first != hoveredKind || hit.second != hoveredIndex) {
      hoveredKind = hit.first;
      hoveredIndex = hit.second;
      repaint();
    }
  }

  void mouseExit(const juce::MouseEvent &) override {
    if (isDraggingItem || isDraggingColumn)
      return;
    if (hoveredKind != -1 || hoveredIndex != -1) {
      hoveredKind = -1;
      hoveredIndex = -1;
      repaint();
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    mouseDownPos = e.position;
    currentDragPos = e.position;
    clearDragState();

    const auto hit = hitTest(e.position);
    if (searchBox.getText().trim().isNotEmpty()) {
      pressedKind = hit.first;
      pressedIndex = hit.second;
      return;
    }

    if (e.mods.isPopupMenu()) {
      showContextMenu(hit, e);
      return;
    }

    pressedKind = hit.first;
    pressedIndex = hit.second;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (searchBox.getText().trim().isNotEmpty())
      return;
    if (pressedKind != 3 && pressedKind != 4)
      return;

    currentDragPos = e.position;
    if (!isDraggingItem && !isDraggingColumn) {
      if (mouseDownPos.getDistanceFrom(e.position) <= 6.0f)
        return;

      if (pressedKind == 3) {
        isDraggingItem = true;
        dragCatalogIdx = pressedIndex;
      } else if (pressedKind == 4) {
        for (int vc = 0; vc < 5; ++vc) {
          if (columnOrder[static_cast<size_t>(vc)] == pressedIndex) {
            isDraggingColumn = true;
            dragSourceVisCol = vc;
            dragTargetVisCol = vc;
            break;
          }
        }
      }
    }

    if (isDraggingItem) {
      const auto drop = computeItemDropTarget(e.position);
      dragTargetGroup = drop.first;
      dragTargetRow = drop.second;
      repaint();
    } else if (isDraggingColumn) {
      dragTargetVisCol = computeClosestVisCol(e.position.x);
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (isDraggingItem) {
      const int itemIdx = dragCatalogIdx;
      const int dstGrp = dragTargetGroup;
      const int dstRow = dragTargetRow;
      clearDragState();
      if (itemIdx >= 0 && dstGrp >= 0)
        moveCatalogItem(itemIdx, dstGrp, dstRow, true);
      else
        repaint();
      return;
    }

    if (isDraggingColumn) {
      const int srcVc = dragSourceVisCol;
      const int dstVc = dragTargetVisCol;
      clearDragState();
      if (srcVc >= 0 && dstVc >= 0 && srcVc != dstVc)
        swapVisualColumns(srcVc, dstVc, true);
      else
        repaint();
      return;
    }

    clearDragState();

    if (e.mods.isPopupMenu())
      return;

    const auto hit = hitTest(e.position);
    if (hit.first == 1) { // Search result
      const auto matches = findPagesMatching(searchBox.getText());
      if (hit.second >= 0 && hit.second < static_cast<int>(matches.size()) &&
          onPageSelected) {
        onPageSelected(matches[static_cast<size_t>(hit.second)]->groupIdx,
                       matches[static_cast<size_t>(hit.second)]->subPage);
      }
    } else if (hit.first == 3) { // Directory item
      const auto &cat = getCatalog();
      if (hit.second >= 0 && hit.second < static_cast<int>(cat.size()) &&
          onPageSelected) {
        onPageSelected(cat[static_cast<size_t>(hit.second)].groupIdx,
                       cat[static_cast<size_t>(hit.second)].subPage);
      }
    } else if (hit.first == 4) { // Category header (opens first item in column)
      const int grp = juce::jlimit(0, 4, hit.second);
      const auto &items = columnItems[static_cast<size_t>(grp)];
      const auto &cat = getCatalog();
      if (!items.empty() && onPageSelected) {
        const int idx = items.front();
        if (idx >= 0 && idx < static_cast<int>(cat.size())) {
          onPageSelected(cat[static_cast<size_t>(idx)].groupIdx,
                         cat[static_cast<size_t>(idx)].subPage);
        }
      } else if (onPageSelected) {
        onPageSelected(grp, 0);
      }
    }
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0b111c));
    g.fillRoundedRectangle(area, 12.0f);
    g.setColour(juce::Colour(0xff233146));
    g.drawRoundedRectangle(area, 12.0f, 1.0f);

    auto inner = area.reduced(12.0f, 8.0f);
    auto hdr = inner.removeFromTop(26.0f);
    g.setFont(midas_ui::getFont(12.0f, true));
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawText("CONSOLE DIRECTORY", hdr.removeFromLeft(190.0f).toNearestInt(),
               juce::Justification::centredLeft);

    inner.removeFromTop(8.0f);

    const auto q = searchBox.getText().trim();
    if (q.isNotEmpty()) {
      paintSearchResults(g, inner, q);
      return;
    }

    static const char *catBaseNames[5] = {"MIX", "ROUTING", "FX", "SHOW",
                                          "SYSTEM"};
    static const juce::uint32 catCols[5] = {0xff38bdf8, 0xff10b981, 0xfff59e0b,
                                            0xffa855f7, 0xff0ea5e9};
    const auto &cat = getCatalog();

    for (int visCol = 0; visCol < 5; ++visCol) {
      const auto m = getColumnMetrics(visCol);
      const int grp = m.grp;
      const juce::Colour accent(catCols[grp]);
      const bool isDropCol =
          (isDraggingItem && dragTargetGroup == grp) ||
          (isDraggingColumn && dragTargetVisCol == visCol);
      const bool isDraggedCol =
          (isDraggingColumn && dragSourceVisCol == visCol);

      g.setColour(isDropCol ? juce::Colour(0xff132036)
                            : juce::Colour(0xff0f1726));
      g.fillRoundedRectangle(m.colRect, 8.0f);
      g.setColour(isDropCol ? midas_colors::kAccentCyan
                            : (isDraggedCol ? accent.withAlpha(0.18f)
                                            : accent.withAlpha(0.30f)));
      g.drawRoundedRectangle(m.colRect, 8.0f, isDropCol ? 2.0f : 1.0f);

      const bool hdrHov =
          (!isDraggingItem && !isDraggingColumn && hoveredKind == 4 &&
           hoveredIndex == grp);
      if (hdrHov || isDropCol) {
        g.setColour(accent.withAlpha(isDropCol ? 0.24f : 0.18f));
        g.fillRoundedRectangle(m.colHdr, 4.0f);
      }
      g.setFont(midas_ui::getFont(10.5f, true));
      g.setColour(isDraggedCol ? accent.withAlpha(0.45f) : accent);
      g.drawText(juce::String(visCol + 1) + ". " + catBaseNames[grp],
                 m.colHdr.reduced(4.0f, 0.0f).withTrimmedRight(14.0f)
                     .toNearestInt(),
                 juce::Justification::centredLeft);

      // Subtle column move grip "::"
      drawDragGrip(g,
                   juce::Rectangle<float>(m.colHdr.getRight() - 13.0f,
                                          m.colHdr.getY() + 4.0f, 10.0f,
                                          m.colHdr.getHeight() - 8.0f),
                   accent.withAlpha(0.45f));

      const auto &indices = columnItems[static_cast<size_t>(grp)];
      const int itemCount = static_cast<int>(indices.size());

      if (itemCount == 0) {
        auto emptyBox = m.itemsArea.withHeight(std::min(58.0f, m.itemsArea.getHeight()));
        g.setColour(juce::Colour(0xff141e2f).withAlpha(0.6f));
        g.fillRoundedRectangle(emptyBox, 6.0f);
        const juce::Colour borderCol =
            isDropCol ? midas_colors::kAccentCyan : juce::Colour(0xff2b3c56);
        g.setColour(borderCol);
        float dashes[2] = {4.0f, 3.0f};
        juce::Path p;
        p.addRoundedRectangle(emptyBox.reduced(1.0f), 6.0f);
        juce::PathStrokeType(1.0f).createDashedStroke(p, p, dashes, 2);
        g.fillPath(p);
        g.setFont(midas_ui::getFont(9.0f, false));
        g.setColour(isDropCol ? midas_colors::kAccentCyan
                              : midas_colors::kTextMuted);
        g.drawText("Kosong - Tarik ke sini", emptyBox.toNearestInt(),
                   juce::Justification::centred, true);
      }

      for (int r = 0; r < itemCount; ++r) {
        const int catIdx = indices[static_cast<size_t>(r)];
        const auto &item = cat[static_cast<size_t>(catIdx)];
        auto rowRect = getItemRect(m, r);
        const bool isDragSrc = (isDraggingItem && dragCatalogIdx == catIdx);
        const bool isHov = (!isDraggingItem && !isDraggingColumn &&
                            hoveredKind == 3 && hoveredIndex == catIdx);
        const bool isAct = (item.groupIdx == activeGroupIdx &&
                            item.subPage == activeSubPageIdx);
        const bool isFav = isFavorite(item.groupIdx, item.subPage);
        const juce::Colour itemAcc(item.colorHex);
        const float cardAlpha = isDragSrc ? 0.35f : 1.0f;

        g.setColour((isHov ? juce::Colour(0xff1e2d45)
                           : (isAct ? itemAcc.withAlpha(0.20f)
                                    : juce::Colour(0xff141e2f)))
                        .withMultipliedAlpha(cardAlpha));
        g.fillRoundedRectangle(rowRect, 6.0f);
        g.setColour((isAct ? itemAcc
                           : (isHov ? itemAcc.withAlpha(0.75f)
                                    : juce::Colour(0xff223047)))
                        .withMultipliedAlpha(cardAlpha));
        g.drawRoundedRectangle(rowRect, 6.0f, isAct ? 1.4f : 0.8f);

        auto rIn = rowRect.reduced(6.0f, 2.0f);
        const float badgeH = std::min(18.0f, std::max(12.0f, rIn.getHeight()));
        auto badgeRect =
            rIn.removeFromLeft(24.0f).withSizeKeepingCentre(22.0f, badgeH);
        g.setColour(itemAcc.withAlpha(0.16f * cardAlpha));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(midas_ui::getFont(7.6f, true));
        g.setColour(itemAcc.withMultipliedAlpha(cardAlpha));
        g.drawText(item.badge, badgeRect.toNearestInt(),
                   juce::Justification::centred);
        rIn.removeFromLeft(5.0f);

        drawDragGrip(g,
                     juce::Rectangle<float>(rIn.getRight() - 9.0f, rIn.getY(),
                                            8.0f, rIn.getHeight()),
                     juce::Colour(0xff64748b).withMultipliedAlpha(cardAlpha));
        rIn.removeFromRight(10.0f);

        if (isFav) {
          g.setFont(midas_ui::getFont(10.0f, true));
          g.setColour(midas_colors::kAccentGold.withMultipliedAlpha(cardAlpha));
          g.drawText("*", rIn.removeFromRight(12.0f).toNearestInt(),
                     juce::Justification::centred);
        }
        g.setFont(midas_ui::getFont(rowRect.getHeight() < 22.0f ? 9.0f : 10.0f,
                                    isAct || isHov));
        g.setColour((isAct ? juce::Colours::white : juce::Colour(0xffe2e8f0))
                        .withMultipliedAlpha(cardAlpha));
        g.drawText(item.title, rIn.toNearestInt(),
                   juce::Justification::centredLeft, true);
      }

      if (isDraggingItem && dragTargetGroup == grp) {
        float lineY = m.itemsArea.getY() + 12.0f;
        if (itemCount > 0) {
          if (dragTargetRow <= 0)
            lineY = m.itemsArea.getY() - 1.5f;
          else if (dragTargetRow >= itemCount)
            lineY = getItemRect(m, itemCount - 1).getBottom() + 1.5f;
          else
            lineY = getItemRect(m, dragTargetRow).getY() - m.itemGap * 0.5f;
        }
        g.setColour(midas_colors::kAccentGold);
        g.fillRoundedRectangle(m.itemsArea.getX(), lineY - 1.5f,
                               m.itemsArea.getWidth(), 3.0f, 1.5f);
        g.fillEllipse(m.itemsArea.getX() - 2.0f, lineY - 3.5f, 7.0f, 7.0f);
        g.fillEllipse(m.itemsArea.getRight() - 5.0f, lineY - 3.5f, 7.0f, 7.0f);
      }
    }

    if (isDraggingItem && dragCatalogIdx >= 0 &&
        dragCatalogIdx < static_cast<int>(cat.size())) {
      const auto &dragItem = cat[static_cast<size_t>(dragCatalogIdx)];
      const juce::Colour acc(dragItem.colorHex);
      auto ghost = juce::Rectangle<float>(170.0f, 28.0f)
                       .withCentre(currentDragPos);
      g.setColour(juce::Colour(0xff0b1322).withAlpha(0.92f));
      g.fillRoundedRectangle(ghost, 7.0f);
      g.setColour(acc);
      g.drawRoundedRectangle(ghost, 7.0f, 1.6f);

      auto gIn = ghost.reduced(6.0f, 4.0f);
      auto gBadge =
          gIn.removeFromLeft(24.0f).withSizeKeepingCentre(22.0f, 18.0f);
      g.setColour(acc.withAlpha(0.22f));
      g.fillRoundedRectangle(gBadge, 4.0f);
      g.setFont(midas_ui::getFont(7.8f, true));
      g.setColour(acc);
      g.drawText(dragItem.badge, gBadge.toNearestInt(),
                 juce::Justification::centred);
      gIn.removeFromLeft(6.0f);
      g.setFont(midas_ui::getFont(10.0f, true));
      g.setColour(juce::Colours::white);
      g.drawText(dragItem.title, gIn.toNearestInt(),
                 juce::Justification::centredLeft, true);
    } else if (isDraggingColumn && dragSourceVisCol >= 0 &&
               dragSourceVisCol < 5) {
      const int grp = columnOrder[static_cast<size_t>(dragSourceVisCol)];
      const juce::Colour acc(catCols[grp]);
      auto pill = juce::Rectangle<float>(150.0f, 28.0f)
                      .withCentre(currentDragPos);
      g.setColour(juce::Colour(0xff0b1322).withAlpha(0.94f));
      g.fillRoundedRectangle(pill, 7.0f);
      g.setColour(acc);
      g.drawRoundedRectangle(pill, 7.0f, 1.6f);
      g.setFont(midas_ui::getFont(10.5f, true));
      g.drawText(juce::String("PINDAH KOLOM: ") + catBaseNames[grp],
                 pill.toNearestInt(), juce::Justification::centred, true);
    }
  }

private:
  struct ColumnMetrics {
    int visCol = 0;
    int grp = 0;
    juce::Rectangle<float> colRect;
    juce::Rectangle<float> colHdr;
    juce::Rectangle<float> itemsArea;
    float itemGap = 4.0f;
    float rowH = 30.0f;
  };

  static juce::File getLayoutFile() {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
        .getChildFile("OpenLiveHD")
        .getChildFile("console_directory_layout_v1.txt");
  }

  void saveLayoutToDisk() const {
    const auto file = getLayoutFile();
    if (!file.getParentDirectory().exists())
      file.getParentDirectory().createDirectory();

    juce::StringArray lines;
    juce::StringArray colTokens;
    for (int c : columnOrder)
      colTokens.add(juce::String(c));
    lines.add("COLS:" + colTokens.joinIntoString(","));

    for (int g = 0; g < 5; ++g) {
      juce::StringArray itemTokens;
      for (int idx : columnItems[static_cast<size_t>(g)])
        itemTokens.add(juce::String(idx));
      lines.add("GRP" + juce::String(g) + ":" +
                itemTokens.joinIntoString(","));
    }
    file.replaceWithText(lines.joinIntoString("\n") + "\n");
  }

  void loadLayoutFromDisk() {
    resetLayoutToDefault(false);
    const auto file = getLayoutFile();
    if (!file.existsAsFile())
      return;

    const auto lines = juce::StringArray::fromLines(file.loadFileAsString());
    std::array<int, 5> parsedOrder = {0, 1, 2, 3, 4};
    bool hasOrder = false;
    std::array<std::vector<int>, 5> parsedItems;
    const auto &cat = getCatalog();
    const int nCat = static_cast<int>(cat.size());
    std::vector<bool> seen(static_cast<size_t>(nCat), false);
    bool hasAnyGroupLine = false;

    for (const auto &rawLine : lines) {
      const auto line = rawLine.trim();
      if (line.startsWith("COLS:")) {
        auto toks = juce::StringArray::fromTokens(line.substring(5), ",", "");
        if (toks.size() == 5) {
          std::array<bool, 5> used{};
          bool valid = true;
          for (int i = 0; i < 5; ++i) {
            const int v = toks[i].trim().getIntValue();
            if (v < 0 || v >= 5 || used[static_cast<size_t>(v)]) {
              valid = false;
              break;
            }
            used[static_cast<size_t>(v)] = true;
            parsedOrder[static_cast<size_t>(i)] = v;
          }
          if (valid)
            hasOrder = true;
        }
      } else if (line.startsWith("GRP") && line.length() >= 5 &&
                 line[4] == ':') {
        const int grp = line.substring(3, 4).getIntValue();
        if (grp >= 0 && grp < 5) {
          hasAnyGroupLine = true;
          auto toks = juce::StringArray::fromTokens(line.substring(5), ",", "");
          for (const auto &tok : toks) {
            if (tok.trim().isEmpty())
              continue;
            const int idx = tok.trim().getIntValue();
            if (idx >= 0 && idx < nCat && !seen[static_cast<size_t>(idx)]) {
              seen[static_cast<size_t>(idx)] = true;
              parsedItems[static_cast<size_t>(grp)].push_back(idx);
            }
          }
        }
      }
    }

    if (hasOrder)
      columnOrder = parsedOrder;

    if (hasAnyGroupLine) {
      for (int idx = 0; idx < nCat; ++idx) {
        if (!seen[static_cast<size_t>(idx)]) {
          const int defGrp =
              juce::jlimit(0, 4, cat[static_cast<size_t>(idx)].groupIdx);
          parsedItems[static_cast<size_t>(defGrp)].push_back(idx);
        }
      }
      columnItems = std::move(parsedItems);
    }
    updateResetButtonAppearance();
  }

  void updateResetButtonAppearance() {
    const bool def = isDefaultLayout();
    resetLayoutBtn.setColour(
        juce::TextButton::textColourOffId,
        def ? juce::Colour(0xff64748b) : juce::Colour(0xff38bdf8));
  }

  void clearDragState() {
    pressedKind = -1;
    pressedIndex = -1;
    isDraggingItem = false;
    dragCatalogIdx = -1;
    dragTargetGroup = -1;
    dragTargetRow = -1;
    isDraggingColumn = false;
    dragSourceVisCol = -1;
    dragTargetVisCol = -1;
  }

  juce::Rectangle<float> getDirectoryArea() const {
    auto inner = getLocalBounds().toFloat().reduced(12.0f, 8.0f);
    inner.removeFromTop(34.0f);
    return inner;
  }

  ColumnMetrics getColumnMetrics(int visCol) const {
    ColumnMetrics m;
    m.visCol = juce::jlimit(0, 4, visCol);
    m.grp = columnOrder[static_cast<size_t>(m.visCol)];
    const auto dirArea = getDirectoryArea();
    const float colGap = 7.0f;
    const float colW = (dirArea.getWidth() - colGap * 4.0f) / 5.0f;
    m.colRect = juce::Rectangle<float>(
        dirArea.getX() + static_cast<float>(m.visCol) * (colW + colGap),
        dirArea.getY(), colW, dirArea.getHeight());
    auto colIn = m.colRect.reduced(6.0f, 6.0f);
    m.colHdr = colIn.removeFromTop(22.0f);
    colIn.removeFromTop(4.0f);
    m.itemsArea = colIn;

    const int itemCount =
        static_cast<int>(columnItems[static_cast<size_t>(m.grp)].size());
    m.itemGap = (itemCount > 7) ? 2.5f : 4.0f;
    const float maxRowH =
        (m.itemsArea.getHeight() -
         m.itemGap * static_cast<float>(std::max(0, itemCount - 1))) /
        static_cast<float>(std::max(1, itemCount));
    m.rowH = juce::jlimit(16.0f, 44.0f, maxRowH);
    return m;
  }

  juce::Rectangle<float> getItemRect(const ColumnMetrics &m, int rowIdx) const {
    return juce::Rectangle<float>(
        m.itemsArea.getX(),
        m.itemsArea.getY() + static_cast<float>(rowIdx) * (m.rowH + m.itemGap),
        m.itemsArea.getWidth(), m.rowH);
  }

  int computeClosestVisCol(float x) const {
    int bestCol = 0;
    float bestDist = 1.0e9f;
    for (int vc = 0; vc < 5; ++vc) {
      const auto m = getColumnMetrics(vc);
      if (x >= m.colRect.getX() && x <= m.colRect.getRight())
        return vc;
      const float d = std::abs(x - m.colRect.getCentreX());
      if (d < bestDist) {
        bestDist = d;
        bestCol = vc;
      }
    }
    return bestCol;
  }

  std::pair<int, int> computeItemDropTarget(juce::Point<float> p) const {
    const int visCol = computeClosestVisCol(p.x);
    const auto m = getColumnMetrics(visCol);
    const int itemCount =
        static_cast<int>(columnItems[static_cast<size_t>(m.grp)].size());
    for (int r = 0; r < itemCount; ++r) {
      if (p.y < getItemRect(m, r).getCentreY())
        return {m.grp, r};
    }
    return {m.grp, itemCount};
  }

  static void drawDragGrip(juce::Graphics &g, juce::Rectangle<float> box,
                           juce::Colour col) {
    g.setColour(col);
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    for (int dy = -1; dy <= 1; ++dy) {
      const float y = cy + static_cast<float>(dy) * 3.5f - 1.0f;
      g.fillEllipse(cx - 3.0f, y, 2.0f, 2.0f);
      g.fillEllipse(cx + 1.0f, y, 2.0f, 2.0f);
    }
  }

  void showContextMenu(std::pair<int, int> hit, const juce::MouseEvent &e) {
    static const char *catBaseNames[5] = {"MIX", "ROUTING", "FX", "SHOW",
                                          "SYSTEM"};
    juce::Component::SafePointer<AndroidHomeLauncherView> safeThis(this);

    if (hit.first == 3) {
      const int catIdx = hit.second;
      const auto &cat = getCatalog();
      if (catIdx < 0 || catIdx >= static_cast<int>(cat.size()))
        return;
      const auto &item = cat[static_cast<size_t>(catIdx)];

      int srcGrp = 0;
      int srcRow = 0;
      for (int g = 0; g < 5; ++g) {
        const auto &vec = columnItems[static_cast<size_t>(g)];
        for (size_t r = 0; r < vec.size(); ++r) {
          if (vec[r] == catIdx) {
            srcGrp = g;
            srcRow = static_cast<int>(r);
          }
        }
      }
      const int colCount =
          static_cast<int>(columnItems[static_cast<size_t>(srcGrp)].size());
      const bool isFav = isFavorite(item.groupIdx, item.subPage);

      juce::PopupMenu menu;
      menu.addItem(1, juce::String("Buka ") + item.title);
      menu.addSeparator();
      menu.addItem(2, isFav ? "Hapus dari Favorit (*)"
                            : "Jadikan Favorit (*)");
      menu.addSeparator();
      menu.addItem(3, "Geser Naik", srcRow > 0);
      menu.addItem(4, "Geser Turun", srcRow + 1 < colCount);

      juce::PopupMenu colSub;
      for (int vc = 0; vc < 5; ++vc) {
        const int g = columnOrder[static_cast<size_t>(vc)];
        colSub.addItem(100 + g,
                       juce::String(vc + 1) + ". " + catBaseNames[g], true,
                       g == srcGrp);
      }
      menu.addSubMenu("Pindah ke Kolom", colSub);
      menu.addSeparator();
      menu.addItem(99, "Reset Semua Posisi ke Default");

      menu.showMenuAsync(
          juce::PopupMenu::Options().withTargetScreenArea(
              juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
          [safeThis, catIdx, srcGrp, srcRow, item](int res) {
            if (safeThis == nullptr || res == 0)
              return;
            if (res == 1 && safeThis->onPageSelected) {
              safeThis->onPageSelected(item.groupIdx, item.subPage);
            } else if (res == 2 && safeThis->onToggleFavorite) {
              safeThis->onToggleFavorite(item.groupIdx, item.subPage);
            } else if (res == 3) {
              safeThis->moveCatalogItem(catIdx, srcGrp, srcRow - 1, true);
            } else if (res == 4) {
              safeThis->moveCatalogItem(catIdx, srcGrp, srcRow + 2, true);
            } else if (res >= 100 && res < 105) {
              const int dstGrp = res - 100;
              safeThis->moveCatalogItem(
                  catIdx, dstGrp,
                  static_cast<int>(
                      safeThis->columnItems[static_cast<size_t>(dstGrp)]
                          .size()),
                  true);
            } else if (res == 99) {
              safeThis->resetLayoutToDefault(true);
            }
          });
    } else if (hit.first == 4) {
      const int grp = hit.second;
      int visCol = -1;
      for (int vc = 0; vc < 5; ++vc) {
        if (columnOrder[static_cast<size_t>(vc)] == grp) {
          visCol = vc;
          break;
        }
      }
      if (visCol < 0)
        return;

      juce::PopupMenu menu;
      menu.addItem(1, "Geser Kolom ke Kiri", visCol > 0);
      menu.addItem(2, "Geser Kolom ke Kanan", visCol < 4);
      menu.addSeparator();
      menu.addItem(99, "Reset Semua Posisi ke Default");

      menu.showMenuAsync(
          juce::PopupMenu::Options().withTargetScreenArea(
              juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
          [safeThis, visCol](int res) {
            if (safeThis == nullptr || res == 0)
              return;
            if (res == 1 && visCol > 0)
              safeThis->swapVisualColumns(visCol, visCol - 1, true);
            else if (res == 2 && visCol < 4)
              safeThis->swapVisualColumns(visCol, visCol + 1, true);
            else if (res == 99)
              safeThis->resetLayoutToDefault(true);
          });
    }
  }

  bool isFavorite(int grp, int sub) const {
    for (const auto &f : favoriteKeys) {
      if (f.first == grp && f.second == sub)
        return true;
    }
    return false;
  }

  void paintSearchResults(juce::Graphics &g, juce::Rectangle<float> area,
                          const juce::String &query) {
    const auto matches = findPagesMatching(query);
    auto hdr = area.removeFromTop(18.0f);
    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kTextMuted);
    g.drawText("HASIL PENCARIAN \"" + query.toUpperCase() + "\" (" +
                   juce::String(matches.size()) +
                   " modul ditemukan  |  Tekan ENTER untuk buka teratas)",
               hdr.toNearestInt(), juce::Justification::centredLeft);
    area.removeFromTop(6.0f);

    if (matches.empty()) {
      g.setFont(midas_ui::getFont(12.0f, false));
      g.setColour(midas_colors::kTextMuted);
      g.drawText(
          "Tidak ada modul yang cocok. Coba: virtual cable, IEM, EQ, recorder, osc",
          area.toNearestInt(), juce::Justification::centred);
      return;
    }

    const int count = std::min(8, static_cast<int>(matches.size()));
    const float gap = 8.0f;
    const float cardW = (area.getWidth() - gap) * 0.5f;
    const float cardH = std::min(62.0f, (area.getHeight() - gap * 3.0f) * 0.25f);

    for (int i = 0; i < count; ++i) {
      const auto *p = matches[static_cast<size_t>(i)];
      const int col = i % 2;
      const int row = i / 2;
      auto card = juce::Rectangle<float>(
          area.getX() + static_cast<float>(col) * (cardW + gap),
          area.getY() + static_cast<float>(row) * (cardH + gap), cardW, cardH);
      const juce::Colour accent(p->colorHex);
      const bool hov = (hoveredKind == 1 && hoveredIndex == i);

      g.setColour(hov ? juce::Colour(0xff1c2b42) : juce::Colour(0xff121b2b));
      g.fillRoundedRectangle(card, 9.0f);
      g.setColour(hov ? accent : accent.withAlpha(0.45f));
      g.drawRoundedRectangle(card, 9.0f, hov ? 1.5f : 1.0f);

      auto cIn = card.reduced(10.0f, 6.0f);
      auto badge = cIn.removeFromLeft(34.0f).withSizeKeepingCentre(30.0f, 30.0f);
      g.setColour(accent.withAlpha(0.18f));
      g.fillRoundedRectangle(badge, 7.0f);
      g.setColour(accent);
      g.drawRoundedRectangle(badge, 7.0f, 1.0f);
      g.setFont(midas_ui::getFont(10.0f, true));
      g.drawText(p->badge, badge.toNearestInt(), juce::Justification::centred);

      cIn.removeFromLeft(8.0f);
      auto openPill = cIn.removeFromRight(56.0f).withSizeKeepingCentre(52.0f, 20.0f);
      g.setColour(accent.withAlpha(0.22f));
      g.fillRoundedRectangle(openPill, 5.0f);
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(accent);
      g.drawText("OPEN >", openPill.toNearestInt(), juce::Justification::centred);

      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(accent);
      g.drawText(juce::String(p->groupName) + "  >  " +
                     juce::String(p->title).toUpperCase(),
                 cIn.removeFromTop(14.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);
      g.setFont(midas_ui::getFont(10.0f, false));
      g.setColour(juce::Colour(0xffcbd5e1));
      g.drawText(p->subtitle, cIn.toNearestInt(),
                 juce::Justification::centredLeft, true);
    }
  }

  // Returns {kind, index}:
  //   kind 1 = search match index
  //   kind 3 = catalog index
  //   kind 4 = category header (groupIdx 0..4)
  std::pair<int, int> hitTest(juce::Point<float> p) const {
    auto inner = getDirectoryArea();

    const auto q = searchBox.getText().trim();
    if (q.isNotEmpty()) {
      inner.removeFromTop(24.0f);
      const auto matches = findPagesMatching(q);
      const int count = std::min(8, static_cast<int>(matches.size()));
      const float gap = 8.0f;
      const float cardW = (inner.getWidth() - gap) * 0.5f;
      const float cardH =
          std::min(62.0f, (inner.getHeight() - gap * 3.0f) * 0.25f);
      for (int i = 0; i < count; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        auto card = juce::Rectangle<float>(
            inner.getX() + static_cast<float>(col) * (cardW + gap),
            inner.getY() + static_cast<float>(row) * (cardH + gap), cardW,
            cardH);
        if (card.contains(p))
          return {1, i};
      }
      return {-1, -1};
    }

    for (int visCol = 0; visCol < 5; ++visCol) {
      const auto m = getColumnMetrics(visCol);
      if (m.colHdr.contains(p))
        return {4, m.grp};

      const auto &indices = columnItems[static_cast<size_t>(m.grp)];
      const int itemCount = static_cast<int>(indices.size());
      for (int r = 0; r < itemCount; ++r) {
        if (getItemRect(m, r).contains(p))
          return {3, indices[static_cast<size_t>(r)]};
      }
    }
    return {-1, -1};
  }

  juce::TextEditor searchBox;
  juce::TextButton clearSearchBtn;
  juce::TextButton resetLayoutBtn{"RESET POSISI"};
  std::array<std::vector<int>, 5> columnItems;
  std::array<int, 5> columnOrder = {0, 1, 2, 3, 4};
  std::vector<std::pair<int, int>> favoriteKeys;
  int activeGroupIdx = 0;
  int activeSubPageIdx = 0;
  int hoveredKind = -1;
  int hoveredIndex = -1;

  int pressedKind = -1;
  int pressedIndex = -1;
  juce::Point<float> mouseDownPos{};
  juce::Point<float> currentDragPos{};
  bool isDraggingItem = false;
  int dragCatalogIdx = -1;
  int dragTargetGroup = -1;
  int dragTargetRow = -1;
  bool isDraggingColumn = false;
  int dragSourceVisCol = -1;
  int dragTargetVisCol = -1;
};

class AndroidRecentWorkspaceView final : public juce::Component {
public:
  struct Entry {
    juce::String title;
    juce::String groupLabel;
    int groupIdx = 0;
    int subPage = 0;
    juce::Colour accent{0xff38bdf8};
  };

  AndroidRecentWorkspaceView() { setOpaque(false); }

  std::function<void(int groupIdx, int subPage)> onEntrySelected;
  std::function<void()> onDismiss;

  void setEntries(const std::vector<Entry> &items) {
    entries = items;
    repaint();
  }

  void setFavorites(const std::vector<Entry> &favs) {
    favorites = favs;
    repaint();
  }

  void mouseMove(const juce::MouseEvent &e) override {
    const auto hit = hitTestEntry(e.position);
    if (hit.first != hoveredSection || hit.second != hoveredIdx) {
      hoveredSection = hit.first;
      hoveredIdx = hit.second;
      repaint();
    }
  }

  void mouseExit(const juce::MouseEvent &) override {
    if (hoveredSection != -1 || hoveredIdx != -1) {
      hoveredSection = -1;
      hoveredIdx = -1;
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &e) override {
    const auto hit = hitTestEntry(e.position);
    if (hit.first == 0 && hit.second >= 0 &&
        hit.second < static_cast<int>(favorites.size())) {
      const auto item = favorites[static_cast<size_t>(hit.second)];
      if (onEntrySelected)
        onEntrySelected(item.groupIdx, item.subPage);
    } else if (hit.first == 1 && hit.second >= 0 &&
               hit.second < static_cast<int>(entries.size())) {
      const auto item = entries[static_cast<size_t>(hit.second)];
      if (onEntrySelected)
        onEntrySelected(item.groupIdx, item.subPage);
    } else if (onDismiss) {
      onDismiss();
    }
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xe6090f19));
    g.fillRoundedRectangle(area, 12.0f);
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(area, 12.0f, 1.0f);

    auto inner = area.reduced(14.0f, 10.0f);
    auto hdr = inner.removeFromTop(22.0f);
    g.setFont(midas_ui::getFont(12.0f, true));
    g.setColour(juce::Colours::white);
    g.drawText("FAVORITES & RECENT WORKSPACE", hdr.toNearestInt(),
               juce::Justification::centredLeft);
    g.setFont(midas_ui::getFont(9.5f, false));
    g.setColour(midas_colors::kTextMuted);
    g.drawText("Gunakan tombol * FAV di bilah atas untuk menyematkan fitur",
               hdr.toNearestInt(), juce::Justification::centredRight);

    inner.removeFromTop(6.0f);

    // Top section: Favorites
    auto favHdr = inner.removeFromTop(14.0f);
    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("* PINNED FAVORITES", favHdr.toNearestInt(),
               juce::Justification::centredLeft);
    inner.removeFromTop(3.0f);
    auto favRow = inner.removeFromTop(inner.getHeight() * 0.44f);
    paintRow(g, favRow, favorites, 0,
             "Belum ada favorit. Tap * FAV pada modul mana pun.");

    inner.removeFromTop(8.0f);

    // Bottom section: Recent
    auto recHdr = inner.removeFromTop(14.0f);
    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("RECENTLY VISITED", recHdr.toNearestInt(),
               juce::Justification::centredLeft);
    inner.removeFromTop(3.0f);
    paintRow(g, inner, entries, 1, "Belum ada riwayat modul.");
  }

private:
  void paintRow(juce::Graphics &g, juce::Rectangle<float> rowArea,
                const std::vector<Entry> &list, int sectionId,
                const char *emptyText) {
    if (list.empty()) {
      g.setFont(midas_ui::getFont(10.5f, false));
      g.setColour(midas_colors::kTextMuted);
      g.drawText(emptyText, rowArea.toNearestInt(),
                 juce::Justification::centred);
      return;
    }

    const int count = std::min(6, static_cast<int>(list.size()));
    const float gap = 8.0f;
    const float cardW =
        (rowArea.getWidth() - gap * static_cast<float>(std::max(1, count) - 1)) /
        static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
      const auto &e = list[static_cast<size_t>(i)];
      auto card = juce::Rectangle<float>(
          rowArea.getX() + static_cast<float>(i) * (cardW + gap),
          rowArea.getY(), cardW, rowArea.getHeight());
      const bool hov = (hoveredSection == sectionId && hoveredIdx == i);
      g.setColour(hov ? juce::Colour(0xff1a283d) : juce::Colour(0xff121c2c));
      g.fillRoundedRectangle(card, 10.0f);
      g.setColour(hov ? e.accent : e.accent.withAlpha(0.45f));
      g.drawRoundedRectangle(card, 10.0f, hov ? 1.5f : 1.0f);

      auto cIn = card.reduced(8.0f, 6.0f);
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(e.accent);
      g.drawText((sectionId == 0 ? "* " : "") + e.groupLabel,
                 cIn.removeFromTop(14.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);
      g.setFont(midas_ui::getFont(11.5f, true));
      g.setColour(juce::Colours::white);
      g.drawText(e.title, cIn.toNearestInt(), juce::Justification::centred,
                 true);
    }
  }

  std::pair<int, int> hitTestEntry(juce::Point<float> p) const {
    auto inner = getLocalBounds().toFloat().reduced(14.0f, 10.0f);
    inner.removeFromTop(28.0f);
    inner.removeFromTop(17.0f);
    auto favRow = inner.removeFromTop(inner.getHeight() * 0.44f);
    const int favHit = hitTestRow(favRow, favorites, p);
    if (favHit >= 0)
      return {0, favHit};

    inner.removeFromTop(25.0f);
    const int recHit = hitTestRow(inner, entries, p);
    if (recHit >= 0)
      return {1, recHit};
    return {-1, -1};
  }

  static int hitTestRow(juce::Rectangle<float> rowArea,
                        const std::vector<Entry> &list, juce::Point<float> p) {
    if (list.empty() || !rowArea.contains(p))
      return -1;
    const int count = std::min(6, static_cast<int>(list.size()));
    const float gap = 8.0f;
    const float cardW =
        (rowArea.getWidth() - gap * static_cast<float>(std::max(1, count) - 1)) /
        static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
      auto card = juce::Rectangle<float>(
          rowArea.getX() + static_cast<float>(i) * (cardW + gap),
          rowArea.getY(), cardW, rowArea.getHeight());
      if (card.contains(p))
        return i;
    }
    return -1;
  }

  std::vector<Entry> favorites;
  std::vector<Entry> entries;
  int hoveredSection = -1;
  int hoveredIdx = -1;
};

class AndroidQuickSettingsPanel final : public juce::Component {
public:
  AndroidQuickSettingsPanel()
      : btnDisplayScale("TEXT: 130%", MidasTactileButton::GlowMode::Cyan),
        btnOpenAudio("AUDIO HARDWARE", MidasTactileButton::GlowMode::Green),
        btnOpenHome("HOME LAUNCHER", MidasTactileButton::GlowMode::Gold),
        btnClose("CLOSE ^", MidasTactileButton::GlowMode::Red) {
    setAlwaysOnTop(true);

    brightnessSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brightnessSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 22);
    brightnessSlider.setRange(35.0, 100.0, 1.0);
    brightnessSlider.setValue(100.0, juce::dontSendNotification);
    brightnessSlider.setTextValueSuffix("%");
    brightnessSlider.setColour(juce::Slider::thumbColourId,
                               juce::Colour(0xff38bdf8));
    brightnessSlider.setColour(juce::Slider::trackColourId,
                               juce::Colour(0xff38bdf8));
    brightnessSlider.setColour(juce::Slider::backgroundColourId,
                               juce::Colour(0xff1e293b));
    brightnessSlider.onValueChange = [this] {
      if (onBrightnessChanged)
        onBrightnessChanged(
            static_cast<float>(brightnessSlider.getValue() / 100.0));
    };
    addAndMakeVisible(brightnessSlider);

    btnDisplayScale.setClickingTogglesState(false);
    btnOpenAudio.setClickingTogglesState(false);
    btnOpenHome.setClickingTogglesState(false);
    btnClose.setClickingTogglesState(false);

    btnDisplayScale.onClick = [this] {
      if (onCycleTextScale)
        onCycleTextScale();
    };
    btnOpenAudio.onClick = [this] {
      setVisible(false);
      if (onOpenAudioSettings)
        onOpenAudioSettings();
    };
    btnOpenHome.onClick = [this] {
      setVisible(false);
      if (onOpenHomeLauncher)
        onOpenHomeLauncher();
    };
    btnClose.onClick = [this] { setVisible(false); };

    addAndMakeVisible(btnDisplayScale);
    addAndMakeVisible(btnOpenAudio);
    addAndMakeVisible(btnOpenHome);
    addAndMakeVisible(btnClose);
  }

  std::function<void(float brightness01)> onBrightnessChanged;
  std::function<void()> onCycleTextScale;
  std::function<void()> onOpenAudioSettings;
  std::function<void()> onOpenHomeLauncher;

  void updateStatus(const juce::String &audioStatus,
                    const juce::String &audioDetail,
                    const juce::String &netStatus,
                    const juce::String &netDetail, const juce::String &sysInfo,
                    const juce::String &scaleText) {
    audioLine1 = audioStatus;
    audioLine2 = audioDetail;
    netLine1 = netStatus;
    netLine2 = netDetail;
    sysLine = sysInfo;
    btnDisplayScale.setButtonText(scaleText);
    repaint();
  }

  void mouseUp(const juce::MouseEvent &e) override {
    const auto bounds = getLocalBounds().toFloat();
    const float panelW = juce::jmin(720.0f, bounds.getWidth() - 24.0f);
    auto box = juce::Rectangle<float>((bounds.getWidth() - panelW) * 0.5f, 8.0f,
                                      panelW, 248.0f);
    if (!box.contains(e.position))
      setVisible(false);
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::black.withAlpha(0.55f));

    const float panelW = juce::jmin(720.0f, bounds.getWidth() - 24.0f);
    const float panelH = 248.0f;
    auto box = juce::Rectangle<float>((bounds.getWidth() - panelW) * 0.5f, 8.0f,
                                      panelW, panelH);

    g.setColour(juce::Colour(0xff0f1724));
    g.fillRoundedRectangle(box, 16.0f);
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(box, 16.0f, 1.2f);

    auto inner = box.reduced(16.0f, 12.0f);
    auto hdr = inner.removeFromTop(24.0f);
    g.setFont(midas_ui::getFont(12.5f, true));
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawText("QUICK SETTINGS", hdr.toNearestInt(),
               juce::Justification::centredLeft);

    inner.removeFromTop(6.0f);
    // Brightness row label
    auto bRow = inner.removeFromTop(28.0f);
    g.setFont(midas_ui::getFont(10.5f, true));
    g.setColour(juce::Colours::white);
    g.drawText("BRIGHTNESS LAYAR", bRow.removeFromLeft(140.0f).toNearestInt(),
               juce::Justification::centredLeft);

    inner.removeFromTop(10.0f);
    inner.removeFromTop(34.0f); // Quick action buttons row
    inner.removeFromTop(10.0f);

    // 3 Info Cards: Network Status, Audio Device Status, System Info
    const float gap = 8.0f;
    const float cardW = (inner.getWidth() - gap * 2.0f) / 3.0f;
    auto drawInfoCard = [&](int idx, const juce::String &title,
                            const juce::String &l1, const juce::String &l2,
                            juce::Colour col) {
      auto c = juce::Rectangle<float>(inner.getX() + static_cast<float>(idx) *
                                                         (cardW + gap),
                                      inner.getY(), cardW, inner.getHeight());
      g.setColour(juce::Colour(0xff09101a));
      g.fillRoundedRectangle(c, 10.0f);
      g.setColour(col.withAlpha(0.45f));
      g.drawRoundedRectangle(c, 10.0f, 1.0f);
      auto cIn = c.reduced(10.0f, 6.0f);
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(col);
      g.drawText(title, cIn.removeFromTop(13.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);
      g.setFont(midas_ui::getFont(10.0f, true));
      g.setColour(juce::Colours::white);
      g.drawText(l1, cIn.removeFromTop(16.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);
      g.setFont(midas_ui::getFont(8.5f, false));
      g.setColour(juce::Colour(0xff94a3b8));
      g.drawText(l2, cIn.toNearestInt(), juce::Justification::centredLeft,
                 true);
    };

    drawInfoCard(0, "NETWORK & OSC", netLine1, netLine2,
                 juce::Colour(0xffa855f7));
    drawInfoCard(1, "AUDIO DEVICE", audioLine1, audioLine2,
                 juce::Colour(0xff10b981));
    drawInfoCard(2, "SYSTEM INFO", "OPENLIVE HD v1.7.0", sysLine,
                 juce::Colour(0xff38bdf8));
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    const float panelW = juce::jmin(720.0f, bounds.getWidth() - 24.0f);
    const float dx = (bounds.getWidth() - panelW) * 0.5f;
    auto inner =
        juce::Rectangle<float>(dx, 8.0f, panelW, 248.0f).reduced(16.0f, 12.0f);

    auto hdr = inner.removeFromTop(24.0f);
    btnClose.setBounds(static_cast<int>(hdr.getRight() - 82.0f),
                       static_cast<int>(hdr.getY()), 82, 24);

    inner.removeFromTop(6.0f);
    auto bRow = inner.removeFromTop(28.0f);
    bRow.removeFromLeft(144.0f);
    brightnessSlider.setBounds(bRow.toNearestInt());

    inner.removeFromTop(10.0f);
    auto btnRow = inner.removeFromTop(34.0f);
    const float bw = (btnRow.getWidth() - 16.0f) / 3.0f;
    btnDisplayScale.setBounds(static_cast<int>(btnRow.getX()),
                              static_cast<int>(btnRow.getY()),
                              static_cast<int>(bw), 32);
    btnOpenAudio.setBounds(static_cast<int>(btnRow.getX() + bw + 8.0f),
                           static_cast<int>(btnRow.getY()),
                           static_cast<int>(bw), 32);
    btnOpenHome.setBounds(static_cast<int>(btnRow.getX() + (bw + 8.0f) * 2.0f),
                          static_cast<int>(btnRow.getY()), static_cast<int>(bw),
                          32);
  }

private:
  juce::Slider brightnessSlider;
  MidasTactileButton btnDisplayScale, btnOpenAudio, btnOpenHome, btnClose;
  juce::String audioLine1{"IDLE"}, audioLine2{"No active device"};
  juce::String netLine1{"OSC STOPPED"}, netLine2{"Localhost"};
  juce::String sysLine{"Windows"};
};

// ==============================================================================
// 2. EQUALISER VIEW (4-BAND PEQ + HPF + 64-BAND RTA SPECTRUM)
// ==============================================================================
class MidasEqCurveView : public juce::Component {
public:
  MidasEqCurveView()
      : qKnob("Q WIDTH", 0.1f, 10.0f, 1.0f, "", MidasKnob::NotchColor::Cyan) {
    setRepaintsOnMouseActivity(true);
    addAndMakeVisible(btnRtaPost);
    addAndMakeVisible(btnFlat);
    addAndMakeVisible(btnEqIn);
    addAndMakeVisible(btnDynEq);
    addAndMakeVisible(btnChAi);
    addAndMakeVisible(qKnob);
    btnEqIn.setToggleState(true, juce::dontSendNotification);
    btnFlat.setClickingTogglesState(false);
    btnDynEq.setClickingTogglesState(false);
    btnChAi.setClickingTogglesState(false);

    btnFlat.onClick = [this] {
      for (int i = 0; i < 4; ++i)
        gains[i] = 0.0f;
      repaint();
      if (onFlatClicked)
        onFlatClicked();
    };
    btnRtaPost.onClick = [this] {
      rtaEnabled = btnRtaPost.getToggleState();
      repaint();
    };
    btnEqIn.onClick = [this] {
      eqActive = btnEqIn.getToggleState();
      repaint();
      if (onEqInToggled)
        onEqInToggled(eqActive);
    };
    btnDynEq.onClick = [this] {
      if (onOpenDynEq)
        onOpenDynEq();
    };
    btnChAi.onClick = [this] {
      if (onOpenChannelAi)
        onOpenChannelAi();
    };
    qKnob.onValueChanged = [this](float qVal) {
      if (selectedNode >= 0 && selectedNode < 4) {
        qs[selectedNode] = qVal;
        repaint();
        if (onQChanged)
          onQChanged(selectedNode, qVal);
      }
    };
  }

  void setBands(float lfG, float lfF, float lmfG, float lmfF, float hmfG,
                float hmfF, float hfG, float hfF, float lfQ = 1.0f,
                float lmfQ = 1.0f, float hmfQ = 1.0f, float hfQ = 1.0f) {
    gains[0] = lfG;
    freqs[0] = lfF;
    qs[0] = lfQ;
    gains[1] = lmfG;
    freqs[1] = lmfF;
    qs[1] = lmfQ;
    gains[2] = hmfG;
    freqs[2] = hmfF;
    qs[2] = hmfQ;
    gains[3] = hfG;
    freqs[3] = hfF;
    qs[3] = hfQ;
    if (selectedNode >= 0 && selectedNode < 4)
      qKnob.setValue(qs[selectedNode], juce::dontSendNotification);
    repaint();
  }

  void setHpf(bool enabled, float freq, int slope = 0) {
    hpfActive = enabled;
    hpfFreq = freq;
    hpfSlope = (slope == 0) ? 2 : 4;
    repaint();
  }

  void setLpf(bool enabled, float freq, int slope = 0) {
    lpfActive = enabled;
    lpfFreq = freq;
    lpfSlope = (slope == 0) ? 2 : 4;
    repaint();
  }

  void setEqEnabled(bool enabled) {
    eqActive = enabled;
    btnEqIn.setToggleState(enabled, juce::dontSendNotification);
    repaint();
  }

  void setDynEqGains(float g0, float g1, float g2, float g3) {
    dynGains[0] = g0;
    dynGains[1] = g1;
    dynGains[2] = g2;
    dynGains[3] = g3;
    repaint();
  }

  void setRtaSpectrum(const float *mags, int count) {
    for (int i = 0; i < juce::jmin(64, count); ++i)
      rtaData[i] = mags[i];
    if (rtaEnabled)
      repaint();
  }

  void setRtaFromLevel(float levelDb) {
    if (!rtaEnabled)
      return;
    const float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    for (int i = 0; i < 64; ++i) {
      const float centerWeight = 1.0f - std::abs(i - 28.0f) / 38.0f;
      const float wave = 0.7f + 0.3f * std::sin((i + simPhase) * 0.45f);
      float target = norm * std::max(0.08f, centerWeight) * wave;
      rtaData[static_cast<size_t>(i)] =
          rtaData[static_cast<size_t>(i)] * 0.65f + target * 0.35f;
    }
    simPhase += 0.3f;
    repaint();
  }

  std::function<void(int band, float gain, float freq)> onNodeDragged;
  std::function<void(int band, float q)> onQChanged;
  std::function<void()> onFlatClicked;
  std::function<void(bool)> onEqInToggled;
  std::function<void()> onOpenDynEq;
  std::function<void()> onOpenChannelAi;

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(b, 6.0f, 1.0f);

    const float curveH = b.getHeight() - 40.0f;
    const float zeroY = curveH * 0.5f;

    // Header badge
    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TOUCHSCREEN 4-BAND PEQ + HPF/LPF CURVE & 64-BAND RTA (LINKED "
               "TO PHYSICAL HW KNOBS)",
               12, 6, 520, 14, juce::Justification::centredLeft);

    // Grid lines: 50, 100, 250, 500, 1k, 2.5k, 5k, 10k, 20k
    g.setColour(juce::Colour(0xff182436));
    const float freqsGrid[] = {50.0f,   100.0f,  250.0f,   500.0f,  1000.0f,
                               2500.0f, 5000.0f, 10000.0f, 20000.0f};
    for (float f : freqsGrid) {
      const float x = freqToX(f, b.getWidth());
      g.drawVerticalLine(static_cast<int>(x), 22.0f, curveH - 4.0f);
      g.setFont(midas_ui::getFont(7.5f, true));
      g.setColour(midas_colors::kTextMuted.withAlpha(0.6f));
      juce::String fStr =
          (f >= 1000.0f)
              ? (juce::String(f / 1000.0f, (f == 2500.0f ? 1 : 0)) + "k")
              : juce::String(f, 0);
      g.drawText(fStr, static_cast<int>(x - 14.0f),
                 static_cast<int>(curveH - 14.0f), 28, 10,
                 juce::Justification::centred);
      g.setColour(juce::Colour(0xff182436));
    }

    // Gain grid lines: +12dB, +6dB, 0dB, -6dB, -12dB
    const float gainTicks[] = {12.0f, 6.0f, 0.0f, -6.0f, -12.0f};
    for (float gd : gainTicks) {
      const float gy = zeroY - (gd / 18.0f) * (curveH * 0.44f);
      g.setColour((gd == 0.0f) ? midas_colors::kBorderCol
                               : juce::Colour(0xff131f2e));
      g.drawHorizontalLine(static_cast<int>(gy), 10.0f, b.getWidth() - 10.0f);
      g.setFont(midas_ui::getFont(7.0f, false));
      g.setColour(midas_colors::kTextMuted.withAlpha(0.5f));
      g.drawText((gd > 0 ? "+" : "") + juce::String(static_cast<int>(gd)) +
                     "dB",
                 12, static_cast<int>(gy - 5.0f), 30, 10,
                 juce::Justification::centredLeft);
    }

    // 64-band RTA Spectrum Bars
    if (rtaEnabled) {
      const float rtaW = (b.getWidth() - 24.0f) / 64.0f;
      for (int i = 0; i < 64; ++i) {
        const float val = rtaData[static_cast<size_t>(i)];
        const float barH =
            juce::jlimit(0.0f, curveH - 26.0f, val * (curveH - 26.0f));
        if (barH > 1.0f) {
          const float rx = 12.0f + i * rtaW;
          const float ry = curveH - 4.0f - barH;
          g.setColour(midas_colors::kAccentCyan.withAlpha(0.16f));
          g.fillRect(rx, ry, rtaW - 1.0f, barH);
        }
      }
    }

    // HPF Cutoff line
    if (hpfActive) {
      const float hx = freqToX(hpfFreq, b.getWidth());
      g.setColour(midas_colors::kAccentGold.withAlpha(0.5f));
      g.drawVerticalLine(static_cast<int>(hx), 22.0f, curveH - 4.0f);
      g.setFont(midas_ui::getFont(7.5f, true));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("HPF " + juce::String(static_cast<int>(hpfFreq)) + "Hz",
                 static_cast<int>(hx + 3.0f), 24, 70, 10,
                 juce::Justification::centredLeft);
    }

    // LPF Cutoff line
    if (lpfActive) {
      const float lx = freqToX(lpfFreq, b.getWidth());
      g.setColour(midas_colors::kAccentPurple.withAlpha(0.5f));
      g.drawVerticalLine(static_cast<int>(lx), 22.0f, curveH - 4.0f);
      g.setFont(midas_ui::getFont(7.5f, true));
      g.setColour(midas_colors::kAccentPurple);
      g.drawText("LPF " + juce::String(lpfFreq / 1000.0f, 1) + "kHz",
                 static_cast<int>(lx - 73.0f), 24, 70, 10,
                 juce::Justification::centredRight);
    }

    // EQ Transfer Curve Path
    juce::Path p;
    bool started = false;
    for (float x = 10.0f; x < b.getWidth() - 10.0f; x += 2.5f) {
      const float f = xToFreq(x, b.getWidth());
      float totalGain = 0.0f;

      // High-pass & Low-pass filters operate independently of 4-band PEQ IN
      // switch
      if (hpfActive && f < hpfFreq * 4.0f) {
        const float ratio = hpfFreq / std::max(1.0f, f);
        const float hpfLoss =
            -10.0f *
            std::log10(1.0f + std::pow(ratio, static_cast<float>(hpfSlope)));
        totalGain += hpfLoss;
      }
      if (lpfActive && f > lpfFreq * 0.25f) {
        const float ratio = f / std::max(1.0f, lpfFreq);
        const float lpfLoss =
            -10.0f *
            std::log10(1.0f + std::pow(ratio, static_cast<float>(lpfSlope)));
        totalGain += lpfLoss;
      }

      if (eqActive) {
        // 4 Peaking bands with Q and dynamic EQ deltas
        for (int i = 0; i < 4; ++i) {
          const float fRatio = f / std::max(1.0f, freqs[i]);
          const float qTerm = qs[i] * (fRatio - 1.0f / fRatio);
          const float bell = 1.0f / (1.0f + qTerm * qTerm);
          totalGain += (gains[i] + dynGains[i]) * bell;
        }
      }

      const float y = juce::jlimit(
          20.0f, curveH - 2.0f, zeroY - (totalGain / 18.0f) * (curveH * 0.44f));
      if (!started) {
        p.startNewSubPath(x, y);
        started = true;
      } else {
        p.lineTo(x, y);
      }
    }

    // Shaded fill under curve
    juce::Path fillP = p;
    fillP.lineTo(b.getWidth() - 10.0f, zeroY);
    fillP.lineTo(10.0f, zeroY);
    fillP.closeSubPath();
    g.setColour(midas_colors::kAccentCyan.withAlpha(0.08f));
    g.fillPath(fillP);

    // Thick Curve Line
    g.setColour(eqActive ? midas_colors::kAccentCyan
                         : juce::Colour(0xff64748b));
    g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));

    // 4 Draggable Nodes with Dynamic EQ Rings
    static const juce::Colour nodeCols[] = {
        midas_colors::kAccentRed, midas_colors::kAccentGold,
        midas_colors::kAccentGreen, midas_colors::kAccentCyan};
    static const char *nodeLabels[] = {"LF", "LMF", "HMF", "HF"};

    for (int i = 0; i < 4; ++i) {
      const float nx = freqToX(freqs[i], b.getWidth());
      const float ny =
          zeroY - ((gains[i] + dynGains[i]) / 18.0f) * (curveH * 0.44f);
      auto nr = juce::Rectangle<float>(nx - 8.0f, ny - 8.0f, 16.0f, 16.0f);

      // Dynamic EQ halo
      if (std::abs(dynGains[i]) > 0.1f) {
        const float haloR = 12.0f + std::abs(dynGains[i]) * 1.5f;
        auto hr = juce::Rectangle<float>(nx - haloR, ny - haloR, haloR * 2.0f,
                                         haloR * 2.0f);
        g.setColour(nodeCols[i].withAlpha(0.25f));
        g.drawEllipse(hr, 1.5f);
      }

      // Outer glow if selected
      if (i == selectedNode) {
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.drawEllipse(nr.expanded(3.0f), 2.0f);
      }

      g.setColour(nodeCols[i]);
      g.fillEllipse(nr);
      g.setColour(juce::Colours::black);
      g.drawEllipse(nr, 1.2f);
      g.setFont(midas_ui::getFont(7.5f, true));
      g.setColour(juce::Colours::black);
      g.drawText(nodeLabels[i], nr.toNearestInt(),
                 juce::Justification::centred);
    }

    // Active Dragging Tooltip Tag
    if (selectedNode >= 0 && selectedNode < 4) {
      const int sn = selectedNode;
      const float nx = freqToX(freqs[sn], b.getWidth());
      const float ny = zeroY - (gains[sn] / 18.0f) * (curveH * 0.44f);
      const juce::String tag =
          juce::String(nodeLabels[sn]) + ": " +
          (freqs[sn] >= 1000.0f
               ? (juce::String(freqs[sn] / 1000.0f, 2) + " kHz")
               : (juce::String(static_cast<int>(freqs[sn])) + " Hz")) +
          " | " + (gains[sn] >= 0.0f ? "+" : "") + juce::String(gains[sn], 1) +
          " dB | Q " + juce::String(qs[sn], 2);
      const float tagW = 160.0f;
      const float tagX =
          juce::jlimit(12.0f, b.getWidth() - tagW - 12.0f, nx - tagW * 0.5f);
      const float tagY = (ny > 50.0f) ? (ny - 26.0f) : (ny + 14.0f);

      g.setColour(juce::Colour(0xff060c14).withAlpha(0.92f));
      g.fillRoundedRectangle(tagX, tagY, tagW, 18.0f, 3.0f);
      g.setColour(nodeCols[sn]);
      g.drawRoundedRectangle(tagX, tagY, tagW, 18.0f, 3.0f, 1.0f);
      g.setFont(midas_ui::getFont(8.0f, true));
      g.drawText(tag, static_cast<int>(tagX), static_cast<int>(tagY),
                 static_cast<int>(tagW), 18, juce::Justification::centred);
    }
  }

  void resized() override {
    const auto b = getLocalBounds();
    const int btmY = b.getHeight() - 32;
    btnEqIn.setBounds(10, btmY, 62, 22);
    btnFlat.setBounds(76, btmY, 62, 22);
    btnRtaPost.setBounds(142, btmY, 74, 22);
    btnDynEq.setBounds(220, btmY, 92, 22);
    btnChAi.setBounds(316, btmY, 88, 22);
    qKnob.setBounds(b.getWidth() - 62, btmY - 14, 52, 42);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const float curveH = getHeight() - 40.0f;
    const float zeroY = curveH * 0.5f;
    selectedNode = -1;
    for (int i = 0; i < 4; ++i) {
      const float nx = freqToX(freqs[i], getWidth());
      const float ny =
          zeroY - ((gains[i] + dynGains[i]) / 18.0f) * (curveH * 0.44f);
      if (std::hypot(e.position.x - nx, e.position.y - ny) < 20.0f) {
        selectedNode = i;
        qKnob.setValue(qs[selectedNode], juce::dontSendNotification);
        repaint();
        break;
      }
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (selectedNode >= 0 && selectedNode < 4) {
      const float curveH = getHeight() - 40.0f;
      const float zeroY = curveH * 0.5f;
      const float newFreq = xToFreq(e.position.x, getWidth());
      const float newGain = juce::jlimit(
          -18.0f, 18.0f, -(e.position.y - zeroY) / (curveH * 0.44f) * 18.0f);
      freqs[selectedNode] = juce::jlimit(20.0f, 20000.0f, newFreq);
      gains[selectedNode] = newGain;
      repaint();
      if (onNodeDragged)
        onNodeDragged(selectedNode, newGain, freqs[selectedNode]);
    }
  }

private:
  float freqs[4] = {100.0f, 400.0f, 2500.0f, 10000.0f};
  float gains[4] = {2.0f, -3.5f, 4.0f, 1.5f};
  float qs[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float dynGains[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool hpfActive = true;
  float hpfFreq = 80.0f;
  int hpfSlope = 2; // 2 = 12dB/oct, 4 = 24dB/oct
  bool lpfActive = false;
  float lpfFreq = 18000.0f;
  int lpfSlope = 2;

  std::array<float, 64> rtaData{};
  bool rtaEnabled = true;
  bool eqActive = true;
  int selectedNode = 1;
  float simPhase = 0.0f;

  MidasTactileButton btnEqIn{"EQ IN", MidasTactileButton::GlowMode::Cyan};
  MidasTactileButton btnFlat{"FLAT 0dB", MidasTactileButton::GlowMode::Gold};
  MidasTactileButton btnRtaPost{"RTA POST",
                                MidasTactileButton::GlowMode::Green};
  MidasTactileButton btnDynEq{"DYN EQ 4-BAND",
                              MidasTactileButton::GlowMode::Purple};
  MidasTactileButton btnChAi{"CHANNEL AI", MidasTactileButton::GlowMode::Cyan};
  MidasKnob qKnob;

  static float freqToX(float f, float w) {
    const float minF = 20.0f, maxF = 20000.0f;
    const float norm = std::log10(juce::jlimit(minF, maxF, f) / minF) /
                       std::log10(maxF / minF);
    return 10.0f + norm * (w - 20.0f);
  }

  static float xToFreq(float x, float w) {
    const float norm = juce::jlimit(0.0f, 1.0f, (x - 10.0f) / (w - 20.0f));
    return 20.0f * std::pow(1000.0f, norm);
  }
};

// ==============================================================================
// 3. COMPRESSOR VIEW (TRANSFER CURVE, LIVE BOUNCING BALL, GR LADDER & SLAM)
// ==============================================================================
class MidasCompView : public juce::Component {
public:
  MidasCompView()
      : thrKnob("THRESH", -48.0f, 0.0f, -18.0f, "dB",
                MidasKnob::NotchColor::Gold),
        ratioKnob("RATIO", 1.0f, 20.0f, 4.0f, ":1",
                  MidasKnob::NotchColor::Gold),
        attKnob("ATTACK", 0.1f, 100.0f, 10.0f, "ms",
                MidasKnob::NotchColor::Cyan),
        relKnob("RELEASE", 10.0f, 1000.0f, 120.0f, "ms",
                MidasKnob::NotchColor::Cyan),
        gainKnob("MAKEUP", 0.0f, 24.0f, 0.0f, "dB",
                 MidasKnob::NotchColor::Green),
        btnSlam("ALL-BUTTONS SLAM", MidasTactileButton::GlowMode::Red),
        btnOpenDyn("DYN & SIDECHAIN", MidasTactileButton::GlowMode::Cyan) {

    addAndMakeVisible(thrKnob);
    addAndMakeVisible(ratioKnob);
    addAndMakeVisible(attKnob);
    addAndMakeVisible(relKnob);
    addAndMakeVisible(gainKnob);
    addAndMakeVisible(btnSlam);
    addAndMakeVisible(btnOpenDyn);
    btnOpenDyn.setClickingTogglesState(false);

    auto notifyParams = [this] {
      if (onParamsChanged)
        onParamsChanged(thrKnob.getValue(), ratioKnob.getValue(),
                        attKnob.getValue(), relKnob.getValue(),
                        gainKnob.getValue());
    };
    thrKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    ratioKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    attKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    relKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    gainKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    btnSlam.onClick = [this, notifyParams] {
      if (btnSlam.getToggleState()) {
        ratioKnob.setValue(20.0f, juce::sendNotificationSync);
        attKnob.setValue(0.1f, juce::sendNotificationSync);
      } else {
        ratioKnob.setValue(4.0f, juce::sendNotificationSync);
        attKnob.setValue(10.0f, juce::sendNotificationSync);
      }
      notifyParams();
    };
    btnOpenDyn.onClick = [this] {
      if (onOpenDynamics)
        onOpenDynamics();
    };
  }

  void syncState(float thr, float rat, float att, float rel,
                 float grDb = 0.0f) {
    thrKnob.setValue(thr, juce::dontSendNotification);
    ratioKnob.setValue(rat, juce::dontSendNotification);
    attKnob.setValue(att, juce::dontSendNotification);
    relKnob.setValue(rel, juce::dontSendNotification);
    reductionDb = grDb;
    repaint();
  }

  void setTelemetry(float grDb, float inDb) {
    reductionDb = grDb;
    inputDb = inDb;
    repaint();
  }

  std::function<void(float thr, float rat, float att, float rel, float gain)>
      onParamsChanged;
  std::function<void()> onOpenDynamics;

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Left Transfer Curve Box
    const float curveW = area.getWidth() * 0.45f;
    const float curveH = area.getHeight() - 20.0f;
    auto cBox = juce::Rectangle<float>(12.0f, 10.0f, curveW, curveH);

    g.setColour(juce::Colour(0xff05090f));
    g.fillRoundedRectangle(cBox, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(cBox, 4.0f, 1.0f);

    // Grid dB lines: -48, -36, -24, -12, 0 dB
    g.setColour(juce::Colour(0xff142030));
    const float dbSteps[] = {-48.0f, -36.0f, -24.0f, -12.0f};
    for (float db : dbSteps) {
      const float norm = (db + 60.0f) / 60.0f;
      const float gx = cBox.getX() + norm * cBox.getWidth();
      const float gy = cBox.getBottom() - norm * cBox.getHeight();
      g.drawVerticalLine(static_cast<int>(gx), cBox.getY(), cBox.getBottom());
      g.drawHorizontalLine(static_cast<int>(gy), cBox.getX(), cBox.getRight());

      g.setFont(midas_ui::getFont(7.0f, false));
      g.setColour(midas_colors::kTextMuted.withAlpha(0.5f));
      g.drawText(juce::String(static_cast<int>(db)),
                 static_cast<int>(gx - 12.0f),
                 static_cast<int>(cBox.getBottom() - 12.0f), 24, 10,
                 juce::Justification::centred);
      g.setColour(juce::Colour(0xff142030));
    }

    // 1:1 diagonal unity guide
    g.setColour(juce::Colour(0xff1d2f44));
    g.drawLine(cBox.getX(), cBox.getBottom(), cBox.getRight(), cBox.getY(),
               1.0f);

    // Compression Transfer Curve with Soft Knee
    const float thr = thrKnob.getValue();
    const float ratio = ratioKnob.getValue();
    const float kneeWidth = 6.0f; // 6dB soft knee

    juce::Path p;
    bool started = false;
    for (float inDb = -60.0f; inDb <= 0.0f; inDb += 1.0f) {
      float outDb = inDb;
      if (inDb > thr + kneeWidth * 0.5f) {
        outDb = thr + (inDb - thr) / ratio;
      } else if (inDb > thr - kneeWidth * 0.5f) {
        const float xKnee = inDb - thr + kneeWidth * 0.5f;
        outDb =
            inDb + ((1.0f / ratio - 1.0f) * xKnee * xKnee) / (2.0f * kneeWidth);
      }
      const float px = cBox.getX() + ((inDb + 60.0f) / 60.0f) * cBox.getWidth();
      const float py =
          cBox.getBottom() - ((outDb + 60.0f) / 60.0f) * cBox.getHeight();

      if (!started) {
        p.startNewSubPath(px, py);
        started = true;
      } else {
        p.lineTo(px, py);
      }
    }

    g.setColour(midas_colors::kAccentGold);
    g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));

    // Live Bouncing Dot on Curve
    const float clampedIn = juce::jlimit(-60.0f, 0.0f, inputDb);
    float dotOut = clampedIn;
    if (clampedIn > thr)
      dotOut = thr + (clampedIn - thr) / ratio;
    const float dotX =
        cBox.getX() + ((clampedIn + 60.0f) / 60.0f) * cBox.getWidth();
    const float dotY =
        cBox.getBottom() - ((dotOut + 60.0f) / 60.0f) * cBox.getHeight();

    g.setColour(midas_colors::kAccentCyan.withAlpha(0.35f));
    g.fillEllipse(dotX - 7.0f, dotY - 7.0f, 14.0f, 14.0f);
    g.setColour(midas_colors::kAccentCyan);
    g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);

    // Multi-LED Gain Reduction Ladder Meter
    const float barX = cBox.getRight() + 12.0f;
    const float barW = 20.0f;
    auto grRect = juce::Rectangle<float>(barX, 10.0f, barW, curveH);
    g.setColour(juce::Colour(0xff05090f));
    g.fillRoundedRectangle(grRect, 3.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(grRect, 3.0f, 0.8f);

    // 12 LED segments from 0dB down to 24dB
    const int numLeds = 12;
    const float ledGap = 2.0f;
    const float ledH = (curveH - 6.0f - (numLeds - 1) * ledGap) / numLeds;
    const float curGr = juce::jlimit(0.0f, 24.0f, reductionDb);

    for (int l = 0; l < numLeds; ++l) {
      const float ledVal = (l + 1) * (24.0f / numLeds);
      const float ly = 13.0f + l * (ledH + ledGap);
      const bool lit = (curGr >= ledVal - 1.0f);

      juce::Colour lCol;
      if (l >= 8)
        lCol = midas_colors::kAccentRed;
      else if (l >= 4)
        lCol = midas_colors::kAccentGold;
      else
        lCol = midas_colors::kAccentGreen;

      g.setColour(lit ? lCol : lCol.withAlpha(0.12f));
      g.fillRoundedRectangle(barX + 2.0f, ly, barW - 4.0f, ledH, 1.5f);
    }

    // Telemetry text below
    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("GR: -" + juce::String(reductionDb, 1) + " dB",
               static_cast<int>(barX - 8.0f), static_cast<int>(curveH - 12.0f),
               70, 14, juce::Justification::centredLeft);
  }

  void resized() override {
    const auto area = getLocalBounds();
    const int curveW = static_cast<int>(area.getWidth() * 0.45f);
    const int ctrlX = curveW + 48;
    const int knobW = 48;
    const int knobH = 50;

    thrKnob.setBounds(ctrlX, 10, knobW, knobH);
    ratioKnob.setBounds(ctrlX + knobW + 10, 10, knobW, knobH);
    gainKnob.setBounds(ctrlX + (knobW + 10) * 2, 10, knobW, knobH);

    attKnob.setBounds(ctrlX, 64, knobW, knobH);
    relKnob.setBounds(ctrlX + knobW + 10, 64, knobW, knobH);
    btnSlam.setBounds(ctrlX + (knobW + 10) * 2, 62, 116, 22);
    btnOpenDyn.setBounds(ctrlX + (knobW + 10) * 2, 88, 116, 22);
  }

private:
  MidasKnob thrKnob, ratioKnob, attKnob, relKnob, gainKnob;
  MidasTactileButton btnSlam, btnOpenDyn;
  float reductionDb = 0.0f;
  float inputDb = -60.0f;
};

// ==============================================================================
// 4. NOISE GATE VIEW (TRANSFER CURVE, LIVE BOUNCING BALL, FAST ATTACK)
// ==============================================================================
class MidasGateView : public juce::Component {
public:
  MidasGateView()
      : thrKnob("THRESH", -60.0f, 0.0f, -42.0f, "dB",
                MidasKnob::NotchColor::Cyan),
        rangeKnob("RANGE", -60.0f, 0.0f, -24.0f, "dB",
                  MidasKnob::NotchColor::Cyan),
        attKnob("ATTACK", 0.01f, 20.0f, 1.5f, "ms",
                MidasKnob::NotchColor::Cyan),
        relKnob("RELEASE", 10.0f, 1000.0f, 120.0f, "ms",
                MidasKnob::NotchColor::Cyan),
        btnGateOpen("GATE: OPEN", MidasTactileButton::GlowMode::Green) {

    addAndMakeVisible(thrKnob);
    addAndMakeVisible(rangeKnob);
    addAndMakeVisible(attKnob);
    addAndMakeVisible(relKnob);
    addAndMakeVisible(btnGateOpen);
    btnGateOpen.setToggleState(true, juce::dontSendNotification);

    auto notifyParams = [this] {
      if (onParamsChanged)
        onParamsChanged(thrKnob.getValue(), rangeKnob.getValue(),
                        attKnob.getValue(), relKnob.getValue());
    };
    thrKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    rangeKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    attKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
    relKnob.onValueChanged = [notifyParams](float) { notifyParams(); };
  }

  void syncState(float thr, float range, float att, float rel,
                 float grDb = 0.0f) {
    thrKnob.setValue(thr, juce::dontSendNotification);
    rangeKnob.setValue(range, juce::dontSendNotification);
    attKnob.setValue(att, juce::dontSendNotification);
    relKnob.setValue(rel, juce::dontSendNotification);
    reductionDb = grDb;
    repaint();
  }

  void setTelemetry(float grDb, float inDb) {
    reductionDb = grDb;
    inputDb = inDb;
    const bool isOpen = (inDb >= thrKnob.getValue());
    btnGateOpen.setButtonText(
        isOpen ? "GATE: OPEN"
               : (reductionDb > 2.0f ? "GATE: CLOSED" : "GATE: HOLD"));
    btnGateOpen.setGlowMode(
        isOpen ? MidasTactileButton::GlowMode::Green
               : (reductionDb > 2.0f ? MidasTactileButton::GlowMode::Red
                                     : MidasTactileButton::GlowMode::Gold));
    repaint();
  }

  std::function<void(float thr, float range, float att, float rel)>
      onParamsChanged;

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Left Transfer curve box
    const float curveW = area.getWidth() * 0.45f;
    const float curveH = area.getHeight() - 20.0f;
    auto cBox = juce::Rectangle<float>(12.0f, 10.0f, curveW, curveH);

    g.setColour(juce::Colour(0xff05090f));
    g.fillRoundedRectangle(cBox, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(cBox, 4.0f, 1.0f);

    // 1:1 diagonal guide
    g.setColour(juce::Colour(0xff142030));
    g.drawLine(cBox.getX(), cBox.getBottom(), cBox.getRight(), cBox.getY(),
               1.0f);

    // Gate Curve with Floor
    const float thr = thrKnob.getValue();
    const float range = rangeKnob.getValue(); // e.g. -24dB floor attenuation
    const float thrNorm = (thr + 60.0f) / 60.0f;
    const float gateX = cBox.getX() + thrNorm * cBox.getWidth();
    const float floorY =
        cBox.getBottom() - ((thr + range + 60.0f) / 60.0f) * cBox.getHeight();

    juce::Path p;
    p.startNewSubPath(cBox.getX(), floorY);
    p.lineTo(gateX, floorY);
    p.lineTo(gateX, cBox.getBottom() - thrNorm * cBox.getHeight());
    p.lineTo(cBox.getRight(), cBox.getY());

    g.setColour(midas_colors::kAccentGreen);
    g.strokePath(p, juce::PathStrokeType(2.5f));

    // Live Bouncing Dot on Gate Curve
    const float clampedIn = juce::jlimit(-60.0f, 0.0f, inputDb);
    float dotOut = clampedIn;
    if (clampedIn < thr)
      dotOut = std::max(-60.0f, clampedIn + range);
    const float dotX =
        cBox.getX() + ((clampedIn + 60.0f) / 60.0f) * cBox.getWidth();
    const float dotY =
        cBox.getBottom() - ((dotOut + 60.0f) / 60.0f) * cBox.getHeight();

    g.setColour(midas_colors::kAccentGreen.withAlpha(0.35f));
    g.fillEllipse(dotX - 7.0f, dotY - 7.0f, 14.0f, 14.0f);
    g.setColour(midas_colors::kAccentGreen);
    g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);

    // Gate Reduction Bar
    const float barX = cBox.getRight() + 12.0f;
    const float barW = 18.0f;
    auto grRect = juce::Rectangle<float>(barX, 10.0f, barW, curveH);
    g.setColour(juce::Colour(0xff05090f));
    g.fillRoundedRectangle(grRect, 3.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(grRect, 3.0f, 0.8f);

    const float grNorm = juce::jlimit(0.0f, 1.0f, reductionDb / 40.0f);
    if (grNorm > 0.01f) {
      g.setColour(midas_colors::kAccentRed);
      g.fillRoundedRectangle(barX + 2.0f, 12.0f, barW - 4.0f,
                             grNorm * (curveH - 4.0f), 2.0f);
    }
  }

  void resized() override {
    const auto area = getLocalBounds();
    const int curveW = static_cast<int>(area.getWidth() * 0.45f);
    const int ctrlX = curveW + 48;
    const int knobW = 48;
    const int knobH = 50;

    thrKnob.setBounds(ctrlX, 10, knobW, knobH);
    rangeKnob.setBounds(ctrlX + knobW + 12, 10, knobW, knobH);
    attKnob.setBounds(ctrlX, 64, knobW, knobH);
    relKnob.setBounds(ctrlX + knobW + 12, 64, knobW, knobH);
    btnGateOpen.setBounds(ctrlX + (knobW + 12) * 2, 40, 110, 28);
  }

private:
  MidasKnob thrKnob, rangeKnob, attKnob, relKnob;
  MidasTactileButton btnGateOpen;
  float reductionDb = 0.0f;
  float inputDb = -60.0f;
};

// ==============================================================================
// 5. DIRECT ROUTING & PATCHBAY MATRIX VIEW
// ==============================================================================
class MidasRoutingMatrixView : public juce::Component {
public:
  MidasRoutingMatrixView() {
    static const char *labels[8] = {"STEREO\nMAIN L/R", "MONITOR\nWASAPI 2",
                                    "MON PRE\nFADER",   "MIX 1-2\nIEM VOX",
                                    "MIX 3-4\nWEDGE",   "FX 1-2\nRACK",
                                    "VCA 1\nDRUMS",     "MUTE\nSAFE"};

    for (int i = 0; i < 8; ++i) {
      const auto glow = (i == 0) ? MidasTactileButton::GlowMode::Gold
                        : (i == 1 || i == 2)
                            ? MidasTactileButton::GlowMode::Cyan
                        : (i == 7) ? MidasTactileButton::GlowMode::Red
                                   : MidasTactileButton::GlowMode::Cyan;
      tiles[i] = std::make_unique<MidasTactileButton>(labels[i], glow);
      if (i == 0 || i == 1)
        tiles[i]->setToggleState(true, juce::dontSendNotification);
      tiles[i]->onClick = [this, i] {
        if (onTileToggled)
          onTileToggled(i, tiles[i]->getToggleState());
      };
      addAndMakeVisible(*tiles[i]);
    }
    addAndMakeVisible(btnSurround3D);
    addAndMakeVisible(btnDugan);
    addAndMakeVisible(btnManchino);
    btnSurround3D.setClickingTogglesState(false);
    btnDugan.setClickingTogglesState(false);
    btnManchino.setClickingTogglesState(false);
    btnSurround3D.onClick = [this] {
      if (onOpenSurroundPan)
        onOpenSurroundPan();
    };
    btnDugan.onClick = [this] {
      if (onOpenDugan)
        onOpenDugan();
    };
    btnManchino.onClick = [this] {
      if (onOpenManchino)
        onOpenManchino();
    };
  }

  std::function<void(int tileIdx, bool state)> onTileToggled;
  std::function<void()> onOpenSurroundPan;
  std::function<void()> onOpenDugan;
  std::function<void()> onOpenManchino;

  void setTileStates(const std::array<bool, 8> &states) {
    for (int i = 0; i < 8; ++i) {
      tiles[static_cast<size_t>(i)]->setToggleState(
          states[static_cast<size_t>(i)], juce::dontSendNotification);
    }
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kTextWhite);
    g.drawText("DIRECT MATRIX & BUS ROUTING MATRIX (TOUCH TILES TO TOGGLE)", 14,
               8, 360, 14, juce::Justification::centredLeft);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 28.0f - 7.0f * 6.0f) / 8.0f;
    const float tileH = area.getHeight() - 40.0f;
    const float startY = 28.0f;

    for (int i = 0; i < 8; ++i) {
      const float tx = 14.0f + i * (tileW + 6.0f);
      tiles[i]->setBounds(static_cast<int>(tx), static_cast<int>(startY),
                          static_cast<int>(tileW), static_cast<int>(tileH));
    }
    const int rx = static_cast<int>(area.getWidth());
    btnSurround3D.setBounds(rx - 340, 4, 140, 21);
    btnDugan.setBounds(rx - 192, 4, 90, 21);
    btnManchino.setBounds(rx - 98, 4, 86, 21);
  }

private:
  std::array<std::unique_ptr<MidasTactileButton>, 8> tiles;
  MidasTactileButton btnSurround3D{"3D SURROUND PAN",
                                   MidasTactileButton::GlowMode::Purple};
  MidasTactileButton btnDugan{"DUGAN AUTO",
                              MidasTactileButton::GlowMode::Green};
  MidasTactileButton btnManchino{"MANCHINO",
                                 MidasTactileButton::GlowMode::Cyan};
};

// ==============================================================================
// 6. MANIC VCA MEMBERSHIP VIEW
// ==============================================================================
class MidasVcaView : public juce::Component {
public:
  MidasVcaView() {
    setRepaintsOnMouseActivity(true);
    for (int i = 0; i < 8; ++i) {
      btnMutes[i] = std::make_unique<MidasTactileButton>(
          "MUTE", MidasTactileButton::GlowMode::Red);
      btnSolos[i] = std::make_unique<MidasTactileButton>(
          "SOLO", MidasTactileButton::GlowMode::Gold);
      addAndMakeVisible(*btnMutes[i]);
      addAndMakeVisible(*btnSolos[i]);

      const int vcaIdx = i;
      btnMutes[i]->onClick = [this, vcaIdx] {
        vcaMutes[static_cast<size_t>(vcaIdx)] =
            btnMutes[static_cast<size_t>(vcaIdx)]->getToggleState();
        repaint();
        if (onVcaMuteToggled)
          onVcaMuteToggled(vcaIdx, vcaMutes[static_cast<size_t>(vcaIdx)]);
      };
      btnSolos[i]->onClick = [this, vcaIdx] {
        vcaSolos[static_cast<size_t>(vcaIdx)] =
            btnSolos[static_cast<size_t>(vcaIdx)]->getToggleState();
        repaint();
        if (onVcaSoloToggled)
          onVcaSoloToggled(vcaIdx, vcaSolos[static_cast<size_t>(vcaIdx)]);
      };
    }
    addAndMakeVisible(btnVcaMgr);
    addAndMakeVisible(btnPopMgr);
    btnVcaMgr.setClickingTogglesState(false);
    btnPopMgr.setClickingTogglesState(false);
    btnVcaMgr.onClick = [this] {
      if (onOpenVcaPanel)
        onOpenVcaPanel();
    };
    btnPopMgr.onClick = [this] {
      if (onOpenPopGroups)
        onOpenPopGroups();
    };
  }

  std::function<void(int vcaIdx, bool muted)> onVcaMuteToggled;
  std::function<void(int vcaIdx, bool solo)> onVcaSoloToggled;
  std::function<void(int vcaIdx)> onVcaTileClicked;
  std::function<void(int vcaIdx, float levelDb)> onVcaLevelChanged;
  std::function<void()> onOpenVcaPanel;
  std::function<void()> onOpenPopGroups;

  void syncVcaState(const std::array<int, 8> &counts,
                    const std::array<float, 8> &levelsDb,
                    const std::array<bool, 8> &mutes, int selectedChVca) {
    memberCounts = counts;
    vcaLevelsDb = levelsDb;
    vcaMutes = mutes;
    activeSelectedVca = selectedChVca;
    for (int i = 0; i < 8; ++i) {
      btnMutes[static_cast<size_t>(i)]->setToggleState(
          mutes[static_cast<size_t>(i)], juce::dontSendNotification);
      btnSolos[static_cast<size_t>(i)]->setToggleState(
          vcaSolos[static_cast<size_t>(i)], juce::dontSendNotification);
    }
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MANIC VCA ASSIGNMENT & MOTORIZED DCA TRIM ENGINE (CLICK TILE "
               "TO ASSIGN CH)",
               12, 6, 460, 14, juce::Justification::centredLeft);

    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const float tileH = area.getHeight() - 30.0f;
    static const char *vcaTitles[8] = {
        "VCA 1: DRUMS", "VCA 2: BASS", "VCA 3: GUITARS", "VCA 4: KEYS",
        "VCA 5: LEAD",  "VCA 6: BGV",  "VCA 7: HORNS",   "VCA 8: FX"};

    for (int i = 0; i < 8; ++i) {
      const float tx = 12.0f + i * (tileW + 5.0f);
      auto r = juce::Rectangle<float>(tx, 24.0f, tileW, tileH);
      const bool isAssigned = (activeSelectedVca == (i + 1));
      g.setColour(isAssigned ? juce::Colour(0xff13263a)
                             : juce::Colour(0xff0e1826));
      g.fillRoundedRectangle(r, 4.0f);
      g.setColour(isAssigned ? midas_colors::kAccentCyan
                             : midas_colors::kBorderSubtle);
      g.drawRoundedRectangle(r, 4.0f, isAssigned ? 1.5f : 1.0f);

      // Title
      g.setFont(midas_ui::getFont(8.0f, true));
      g.setColour(isAssigned ? midas_colors::kAccentGold
                             : midas_colors::kAccentCyan);
      g.drawText(vcaTitles[i], tx, 26.0f, tileW, 13.0f,
                 juce::Justification::centred);

      // Member pill & dB readout
      g.setFont(midas_ui::getFont(7.5f, false));
      g.setColour(isAssigned ? midas_colors::kTextWhite
                             : midas_colors::kTextMuted);
      const int cnt = memberCounts[static_cast<size_t>(i)];
      juce::String cntStr = juce::String(cnt) + (cnt == 1 ? " CH" : " CHS") +
                            (isAssigned ? " [SEL]" : "");
      g.drawText(cntStr, tx, 39.0f, tileW, 11.0f, juce::Justification::centred);

      const float lvlDb = vcaLevelsDb[static_cast<size_t>(i)];
      g.setFont(midas_ui::getFont(7.5f, true));
      g.setColour(vcaMutes[static_cast<size_t>(i)] ? midas_colors::kAccentRed
                                                   : midas_colors::kAccentGold);
      g.drawText(juce::String(lvlDb >= 0.0f ? "+" : "") +
                     juce::String(lvlDb, 1) + " dB",
                 tx, 50.0f, tileW, 11.0f, juce::Justification::centred);

      // Level bar
      const float barTop = 64.0f;
      const float barH = juce::jmax(12.0f, tileH - 106.0f);
      g.setColour(juce::Colour(0xff182436));
      g.fillRoundedRectangle(tx + (tileW - 8.0f) * 0.5f, barTop, 8.0f, barH,
                             2.0f);

      const float norm = juce::jlimit(0.0f, 1.0f, (lvlDb + 60.0f) / 70.0f);
      const float fillH = norm * barH;
      g.setColour(vcaMutes[static_cast<size_t>(i)]
                      ? midas_colors::kAccentRed.withAlpha(0.5f)
                      : midas_colors::kAccentGold);
      g.fillRoundedRectangle(tx + (tileW - 8.0f) * 0.5f,
                             barTop + (barH - fillH), 8.0f, fillH, 2.0f);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const int idx = static_cast<int>((e.position.x - 12.0f) / (tileW + 5.0f));
    if (idx >= 0 && idx < 8) {
      if (e.position.y < 60.0f) {
        if (onVcaTileClicked)
          onVcaTileClicked(idx);
      } else if (e.position.y < area.getHeight() - 52.0f) {
        updateVcaLevelFromMouse(idx, e.position.y);
      }
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const int idx = static_cast<int>((e.position.x - 12.0f) / (tileW + 5.0f));
    if (idx >= 0 && idx < 8 && e.position.y >= 55.0f &&
        e.position.y < area.getHeight() - 52.0f) {
      updateVcaLevelFromMouse(idx, e.position.y);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const float btmY = area.getHeight() - 32.0f;

    for (int i = 0; i < 8; ++i) {
      const float tx = 12.0f + i * (tileW + 5.0f);
      btnMutes[i]->setBounds(static_cast<int>(tx + 2.0f),
                             static_cast<int>(btmY - 20.0f),
                             static_cast<int>(tileW - 4.0f), 18);
      btnSolos[i]->setBounds(static_cast<int>(tx + 2.0f),
                             static_cast<int>(btmY),
                             static_cast<int>(tileW - 4.0f), 18);
    }
    const int rx = static_cast<int>(area.getWidth());
    btnVcaMgr.setBounds(rx - 196, 4, 90, 18);
    btnPopMgr.setBounds(rx - 102, 4, 92, 18);
  }

private:
  void updateVcaLevelFromMouse(int idx, float mouseY) {
    const float tileH = getHeight() - 30.0f;
    const float barTop = 64.0f;
    const float barH = juce::jmax(12.0f, tileH - 106.0f);
    const float norm =
        juce::jlimit(0.0f, 1.0f, 1.0f - (mouseY - barTop) / barH);
    const float db = -60.0f + norm * 70.0f;
    vcaLevelsDb[static_cast<size_t>(idx)] = db;
    repaint();
    if (onVcaLevelChanged)
      onVcaLevelChanged(idx, db);
  }

  std::array<std::unique_ptr<MidasTactileButton>, 8> btnMutes;
  std::array<std::unique_ptr<MidasTactileButton>, 8> btnSolos;
  MidasTactileButton btnVcaMgr{"24 VCA MGR",
                               MidasTactileButton::GlowMode::Gold};
  MidasTactileButton btnPopMgr{"24 POP GROUPS",
                               MidasTactileButton::GlowMode::Purple};
  std::array<int, 8> memberCounts = {2, 1, 2, 2, 1, 2, 0, 2};
  std::array<float, 8> vcaLevelsDb = {0.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 0.0f};
  std::array<bool, 8> vcaMutes{};
  std::array<bool, 8> vcaSolos{};
  int activeSelectedVca = 1;
};

// ==============================================================================
// 7. 8x8 MATRIX ZONE ROUTING VIEW
// ==============================================================================
class MidasMatrixZonesView : public juce::Component {
public:
  MidasMatrixZonesView() {
    setRepaintsOnMouseActivity(true);
    for (int i = 0; i < 8; ++i) {
      btnEqs[i] = std::make_unique<MidasTactileButton>(
          "EQ IN", MidasTactileButton::GlowMode::Cyan);
      btnComps[i] = std::make_unique<MidasTactileButton>(
          "MUTE", MidasTactileButton::GlowMode::Red);
      btnEqs[i]->setToggleState(true, juce::dontSendNotification);
      addAndMakeVisible(*btnEqs[i]);
      addAndMakeVisible(*btnComps[i]);

      const int mtxIdx = i;
      btnEqs[i]->onClick = [this, mtxIdx] {
        repaint();
        if (onMatrixEqToggled)
          onMatrixEqToggled(
              mtxIdx, btnEqs[static_cast<size_t>(mtxIdx)]->getToggleState());
      };
      btnComps[i]->onClick = [this, mtxIdx] {
        zoneMuted[static_cast<size_t>(mtxIdx)] =
            btnComps[static_cast<size_t>(mtxIdx)]->getToggleState();
        repaint();
        if (onMatrixMuteToggled)
          onMatrixMuteToggled(mtxIdx, zoneMuted[static_cast<size_t>(mtxIdx)]);
      };
    }
    addAndMakeVisible(btnMatrixGrid);
    btnMatrixGrid.setClickingTogglesState(false);
    btnMatrixGrid.onClick = [this] {
      if (onOpenMatrixGrid)
        onOpenMatrixGrid();
    };
  }

  std::function<void(int mtxIdx, bool eqOn)> onMatrixEqToggled;
  std::function<void(int mtxIdx, bool muted)> onMatrixMuteToggled;
  std::function<void(int mtxIdx, float delayMs, float levelDb)>
      onMatrixZoneChanged;
  std::function<void()> onOpenMatrixGrid;

  void syncMatrixState(const std::array<float, 8> &levelsDb,
                       const std::array<bool, 8> &mutes) {
    zoneLevelDb = levelsDb;
    zoneMuted = mutes;
    for (int i = 0; i < 8; ++i) {
      btnComps[static_cast<size_t>(i)]->setToggleState(
          mutes[static_cast<size_t>(i)], juce::dontSendNotification);
    }
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("8x8 MATRIX OUTPUT ZONES & SPEAKER MANAGEMENT DELAYS (DRAG TILE "
               "FOR LEVEL / DELAY)",
               12, 6, 460, 14, juce::Justification::centredLeft);

    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const float tileH = area.getHeight() - 30.0f;
    static const char *mtxNames[8] = {
        "MTX 1: FOH L",   "MTX 2: FOH R",   "MTX 3: FILL", "MTX 4: SUB",
        "MTX 5: DELAY L", "MTX 6: DELAY R", "MTX 7: VIP",  "MTX 8: BDCST"};

    for (int i = 0; i < 8; ++i) {
      const float tx = 12.0f + i * (tileW + 5.0f);
      auto r = juce::Rectangle<float>(tx, 24.0f, tileW, tileH);
      const bool isMuted = zoneMuted[static_cast<size_t>(i)];
      g.setColour(isMuted ? juce::Colour(0xff1c0d12)
                          : juce::Colour(0xff0e1826));
      g.fillRoundedRectangle(r, 4.0f);
      g.setColour(isMuted ? midas_colors::kAccentRed.withAlpha(0.6f)
                          : midas_colors::kBorderSubtle);
      g.drawRoundedRectangle(r, 4.0f, 1.0f);

      g.setFont(midas_ui::getFont(8.0f, true));
      g.setColour(isMuted ? midas_colors::kAccentRed
                          : midas_colors::kAccentGold);
      g.drawText(mtxNames[i], tx, 26.0f, tileW, 13.0f,
                 juce::Justification::centred);

      const float dlyMs = zoneDelayMs[static_cast<size_t>(i)];
      const float distM = dlyMs * 0.343f;
      const float lvlDb = zoneLevelDb[static_cast<size_t>(i)];

      g.setFont(midas_ui::getFont(7.2f, false));
      g.setColour(midas_colors::kAccentCyan);
      g.drawText("DLY: " + juce::String(dlyMs, 1) + "ms (" +
                     juce::String(distM, 1) + "m)",
                 tx, 40.0f, tileW, 11.0f, juce::Justification::centred);

      g.setColour(midas_colors::kTextWhite);
      g.drawText("LVL: " + juce::String(lvlDb >= 0.0f ? "+" : "") +
                     juce::String(lvlDb, 1) + "dB",
                 tx, 52.0f, tileW, 11.0f, juce::Justification::centred);

      // Mini zone level bar
      const float barTop = 66.0f;
      const float barH = juce::jmax(10.0f, tileH - 108.0f);
      g.setColour(juce::Colour(0xff182436));
      g.fillRoundedRectangle(tx + (tileW - 8.0f) * 0.5f, barTop, 8.0f, barH,
                             2.0f);
      const float norm = juce::jlimit(0.0f, 1.0f, (lvlDb + 60.0f) / 70.0f);
      const float fillH = norm * barH;
      g.setColour(isMuted ? midas_colors::kAccentRed.withAlpha(0.5f)
                          : midas_colors::kAccentCyan);
      g.fillRoundedRectangle(tx + (tileW - 8.0f) * 0.5f,
                             barTop + (barH - fillH), 8.0f, fillH, 2.0f);
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const int idx = static_cast<int>((e.position.x - 12.0f) / (tileW + 5.0f));
    if (idx >= 0 && idx < 8 && e.position.y < area.getHeight() - 52.0f) {
      const float tileH = area.getHeight() - 30.0f;
      const float barTop = 66.0f;
      const float barH = juce::jmax(10.0f, tileH - 108.0f);
      const float normY =
          juce::jlimit(0.0f, 1.0f, 1.0f - (e.position.y - barTop) / barH);
      zoneLevelDb[static_cast<size_t>(idx)] = -60.0f + normY * 70.0f;

      const float tx = 12.0f + idx * (tileW + 5.0f);
      const float normX = juce::jlimit(0.0f, 1.0f, (e.position.x - tx) / tileW);
      zoneDelayMs[static_cast<size_t>(idx)] = normX * 85.0f;
      repaint();
      if (onMatrixZoneChanged)
        onMatrixZoneChanged(idx, zoneDelayMs[static_cast<size_t>(idx)],
                            zoneLevelDb[static_cast<size_t>(idx)]);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 7.0f * 5.0f) / 8.0f;
    const float btmY = area.getHeight() - 32.0f;

    for (int i = 0; i < 8; ++i) {
      const float tx = 12.0f + i * (tileW + 5.0f);
      btnEqs[i]->setBounds(static_cast<int>(tx + 2.0f),
                           static_cast<int>(btmY - 20.0f),
                           static_cast<int>(tileW - 4.0f), 18);
      btnComps[i]->setBounds(static_cast<int>(tx + 2.0f),
                             static_cast<int>(btmY),
                             static_cast<int>(tileW - 4.0f), 18);
    }
    const int rx = static_cast<int>(area.getWidth());
    btnMatrixGrid.setBounds(rx - 108, 4, 100, 18);
  }

private:
  std::array<std::unique_ptr<MidasTactileButton>, 8> btnEqs;
  std::array<std::unique_ptr<MidasTactileButton>, 8> btnComps;
  MidasTactileButton btnMatrixGrid{"MATRIX GRID",
                                   MidasTactileButton::GlowMode::Cyan};
  std::array<float, 8> zoneDelayMs = {0.0f,  0.0f,  4.2f, 2.8f,
                                      18.5f, 18.5f, 6.0f, 0.0f};
  std::array<float, 8> zoneLevelDb = {0.0f,  0.0f,  -3.0f, 1.5f,
                                      -2.0f, -2.0f, -4.0f, 0.0f};
  std::array<bool, 8> zoneMuted{};
};

// ==============================================================================
// 8. QUAD FX RACK ENGINES VIEW
// ==============================================================================
class MidasFxRackView : public juce::Component {
public:
  MidasFxRackView()
      : btnOpenRack("24-SLOT FX RACK", MidasTactileButton::GlowMode::Cyan),
        btnTapTempo("TAP TEMPO", MidasTactileButton::GlowMode::Gold),
        btnBypassAll("BYPASS ALL", MidasTactileButton::GlowMode::Red),
        btnVss4("VSS4 HD GUI", MidasTactileButton::GlowMode::Cyan),
        btnNebula("NEBULA DLY GUI", MidasTactileButton::GlowMode::Gold),
        btnEmt140("EMT 140 GUI", MidasTactileButton::GlowMode::Green),
        btnBx20("AKG BX20", MidasTactileButton::GlowMode::Gold),
        btnMasterX("MASTER X", MidasTactileButton::GlowMode::Purple),
        btnPitch("PITCH ADT", MidasTactileButton::GlowMode::Cyan) {
    setRepaintsOnMouseActivity(true);
    addAndMakeVisible(btnOpenRack);
    addAndMakeVisible(btnTapTempo);
    addAndMakeVisible(btnBypassAll);
    addAndMakeVisible(btnVss4);
    addAndMakeVisible(btnNebula);
    addAndMakeVisible(btnEmt140);
    addAndMakeVisible(btnBx20);
    addAndMakeVisible(btnMasterX);
    addAndMakeVisible(btnPitch);
    btnTapTempo.setClickingTogglesState(false);
    btnOpenRack.setClickingTogglesState(false);
    btnVss4.setClickingTogglesState(false);
    btnNebula.setClickingTogglesState(false);
    btnEmt140.setClickingTogglesState(false);
    btnBx20.setClickingTogglesState(false);
    btnMasterX.setClickingTogglesState(false);
    btnPitch.setClickingTogglesState(false);

    btnOpenRack.onClick = [this] {
      if (onOpenFullFxRack)
        onOpenFullFxRack();
    };
    btnVss4.onClick = [this] {
      if (onOpenHardwareFxEditor)
        onOpenHardwareFxEditor(0);
    };
    btnNebula.onClick = [this] {
      if (onOpenHardwareFxEditor)
        onOpenHardwareFxEditor(1);
    };
    btnEmt140.onClick = [this] {
      if (onOpenHardwareFxEditor)
        onOpenHardwareFxEditor(2);
    };
    btnBx20.onClick = [this] {
      if (onOpenHardwareFxEditor)
        onOpenHardwareFxEditor(3);
    };
    btnMasterX.onClick = [this] {
      if (onOpenHardwareFxEditor)
        onOpenHardwareFxEditor(4);
    };
    btnPitch.onClick = [this] {
      if (onOpenHardwareFxEditor)
        onOpenHardwareFxEditor(5);
    };
    btnTapTempo.onClick = [this] {
      const double nowMs = juce::Time::getMillisecondCounterHiRes();
      const double delta = nowMs - lastTapTimeMs;
      lastTapTimeMs = nowMs;
      if (delta > 180.0 && delta < 1800.0) {
        tapDelayMs = static_cast<float>(delta);
        tapBpm = static_cast<float>(60000.0 / delta);
      } else {
        tapDelayMs = (tapDelayMs > 450.0f) ? 250.0f : (tapDelayMs + 45.0f);
        tapBpm = 60000.0f / tapDelayMs;
      }
      repaint();
      if (onTapTempoChanged)
        onTapTempoChanged(tapDelayMs);
    };
    btnBypassAll.onClick = [this] {
      fxBypassed = btnBypassAll.getToggleState();
      repaint();
      if (onBypassAllToggled)
        onBypassAllToggled(fxBypassed);
    };
  }

  std::function<void()> onOpenFullFxRack;
  std::function<void(int editorType)>
      onOpenHardwareFxEditor; // 0=VSS4, 1=Nebula, 2=EMT140, 3=AKGBX20,
                              // 4=MasterX, 5=Manipulator
  std::function<void(float delayMs)> onTapTempoChanged;
  std::function<void(bool bypassed)> onBypassAllToggled;

  void syncFromRack(float vss4DecaySec, float vss4PreDlyMs, float nebDelayLMs,
                    float plateDecaySec, float masterXCeilDb) {
    vss4Decay = vss4DecaySec;
    vss4PreDelay = vss4PreDlyMs;
    if (nebDelayLMs > 1.0f) {
      tapDelayMs = nebDelayLMs;
      tapBpm = 60000.0f / std::max(50.0f, tapDelayMs);
    }
    plateDecay = plateDecaySec;
    masterXCeiling = masterXCeilDb;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("QUAD STEREO FX ENGINES (GRAVITON DSP RACK SYNCED)", 12, 6, 360,
               14, juce::Justification::centredLeft);

    const float tileW = (area.getWidth() - 24.0f - 3.0f * 8.0f) / 4.0f;
    const float tileH = area.getHeight() - 34.0f;
    static const char *fxNames[4] = {
        "FX 1: TC VSS4 HD REVERB", "FX 2: NEBULA TAPE DELAY",
        "FX 3: EMT 140 / AKG BX20", "FX 4: MASTER X / PITCH ADT"};

    for (int i = 0; i < 4; ++i) {
      const float tx = 12.0f + i * (tileW + 8.0f);
      auto r = juce::Rectangle<float>(tx, 26.0f, tileW, tileH);
      const bool isSel = (selectedFxSlot == i);
      g.setColour(fxBypassed ? juce::Colour(0xff1a0d12)
                             : (isSel ? juce::Colour(0xff102236)
                                      : juce::Colour(0xff060d16)));
      g.fillRoundedRectangle(r, 4.0f);
      g.setColour(fxBypassed ? midas_colors::kAccentRed.withAlpha(0.6f)
                             : (isSel ? midas_colors::kAccentCyan
                                      : midas_colors::kBorderSubtle));
      g.drawRoundedRectangle(r, 4.0f, isSel ? 1.5f : 1.0f);

      g.setFont(midas_ui::getFont(8.2f, true));
      g.setColour(fxBypassed ? midas_colors::kAccentRed
                             : midas_colors::kAccentCyan);
      g.drawText(fxNames[i], tx + 6.0f, 29.0f, tileW - 12.0f, 13.0f,
                 juce::Justification::centredLeft);

      // Parameters
      g.setFont(midas_ui::getFont(7.3f, false));
      g.setColour(midas_colors::kTextWhite);
      if (i == 0) {
        g.drawText("DECAY: " + juce::String(vss4Decay, 1) + "s | PRE: " +
                       juce::String(static_cast<int>(vss4PreDelay)) + "ms",
                   tx + 6.0f, 45.0f, tileW - 12.0f, 11.0f,
                   juce::Justification::centredLeft);
        g.drawText("SYSTEM 6000 ALGO | AUX 5", tx + 6.0f, 57.0f, tileW - 12.0f,
                   11.0f, juce::Justification::centredLeft);
      } else if (i == 1) {
        g.setColour(midas_colors::kAccentGold);
        g.drawText("TIME: " + juce::String(static_cast<int>(tapDelayMs)) +
                       "ms (" + juce::String(static_cast<int>(tapBpm)) +
                       " BPM)",
                   tx + 6.0f, 45.0f, tileW - 12.0f, 11.0f,
                   juce::Justification::centredLeft);
        g.setColour(midas_colors::kTextWhite);
        g.drawText("DUAL TAPE ECHO | AUX 6", tx + 6.0f, 57.0f, tileW - 12.0f,
                   11.0f, juce::Justification::centredLeft);
      } else if (i == 2) {
        g.drawText("PLATE DECAY: " + juce::String(plateDecay, 1) + "s | SPRING",
                   tx + 6.0f, 45.0f, tileW - 12.0f, 11.0f,
                   juce::Justification::centredLeft);
        g.drawText("WILHELM FRANZ / BX20 | AUX 7", tx + 6.0f, 57.0f,
                   tileW - 12.0f, 11.0f, juce::Justification::centredLeft);
      } else {
        g.drawText("CEIL: " + juce::String(masterXCeiling, 1) + "dB | 3-BAND",
                   tx + 6.0f, 45.0f, tileW - 12.0f, 11.0f,
                   juce::Justification::centredLeft);
        g.drawText("FINALIZER & HARMONIZER | AUX 8", tx + 6.0f, 57.0f,
                   tileW - 12.0f, 11.0f, juce::Justification::centredLeft);
      }
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.position.y < 26.0f)
      return;
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 3.0f * 8.0f) / 4.0f;
    const int idx = static_cast<int>((e.position.x - 12.0f) / (tileW + 8.0f));
    if (idx >= 0 && idx < 4) {
      selectedFxSlot = idx;
      repaint();
      if (e.getNumberOfClicks() >= 2 && onOpenHardwareFxEditor) {
        onOpenHardwareFxEditor(idx == 3 ? 4 : idx);
      }
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    btnOpenRack.setBounds(static_cast<int>(area.getWidth() - 305.0f), 4, 110,
                          19);
    btnTapTempo.setBounds(static_cast<int>(area.getWidth() - 190.0f), 4, 85,
                          19);
    btnBypassAll.setBounds(static_cast<int>(area.getWidth() - 100.0f), 4, 88,
                           19);

    const float tileW = (area.getWidth() - 24.0f - 3.0f * 8.0f) / 4.0f;
    const int btnY = static_cast<int>(area.getHeight() - 30.0f);
    const int t0x = static_cast<int>(12.0f + 0.0f * (tileW + 8.0f));
    const int t1x = static_cast<int>(12.0f + 1.0f * (tileW + 8.0f));
    const int t2x = static_cast<int>(12.0f + 2.0f * (tileW + 8.0f));
    const int t3x = static_cast<int>(12.0f + 3.0f * (tileW + 8.0f));
    const int tw = static_cast<int>(tileW) - 10;
    const int halfW = (tw - 4) / 2;

    btnVss4.setBounds(t0x + 5, btnY, tw, 18);
    btnNebula.setBounds(t1x + 5, btnY, tw, 18);
    btnEmt140.setBounds(t2x + 5, btnY, halfW, 18);
    btnBx20.setBounds(t2x + 5 + halfW + 4, btnY, halfW, 18);
    btnMasterX.setBounds(t3x + 5, btnY, halfW, 18);
    btnPitch.setBounds(t3x + 5 + halfW + 4, btnY, halfW, 18);
  }

private:
  MidasTactileButton btnOpenRack, btnTapTempo, btnBypassAll;
  MidasTactileButton btnVss4, btnNebula, btnEmt140, btnBx20, btnMasterX,
      btnPitch;
  double lastTapTimeMs = 0.0;
  float tapDelayMs = 320.0f;
  float tapBpm = 187.5f;
  float vss4Decay = 2.8f;
  float vss4PreDelay = 25.0f;
  float plateDecay = 2.4f;
  float masterXCeiling = -0.3f;
  bool fxBypassed = false;
  int selectedFxSlot = 0;
};

#include "GUI/EmbeddedFxHostPanel.h"

// ==============================================================================
// 9. BROADCAST VIEW (EBU R128 LOUDNESS METER & N-1 IFB MATRIX)
// ==============================================================================
class MidasBroadcastView : public juce::Component {
public:
  MidasBroadcastView() {
    addAndMakeVisible(btnTalkback);
    addAndMakeVisible(btnMonitorCr);
    btnTalkback.setClickingTogglesState(false);
    btnMonitorCr.setClickingTogglesState(false);
    btnTalkback.onClick = [this] {
      if (onOpenTalkback)
        onOpenTalkback();
    };
    btnMonitorCr.onClick = [this] {
      if (onOpenMonitorCr)
        onOpenMonitorCr();
    };
  }

  std::function<void()> onOpenTalkback;
  std::function<void()> onOpenMonitorCr;

  void updateLoudness(float integratedLufs, float shortTermLufs,
                      float momentaryLufs, float truePeakDb,
                      float lraValue = 5.6f) {
    intLufs = integratedLufs;
    shortLufs = shortTermLufs;
    momLufs = momentaryLufs;
    truePeak = truePeakDb;
    lra = lraValue;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(
        "BROADCAST STREAM & EBU R128 LOUDNESS RADAR (TARGET: -23.0 LUFS)", 12,
        6, 420, 14, juce::Justification::centredLeft);

    // Left: Loudness meters
    const float lufsBoxW = area.getWidth() * 0.48f;
    auto lBox = juce::Rectangle<float>(12.0f, 26.0f, lufsBoxW,
                                       area.getHeight() - 34.0f);
    g.setColour(juce::Colour(0xff060d16));
    g.fillRoundedRectangle(lBox, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(lBox, 4.0f, 1.0f);

    const bool passed = (std::abs(intLufs - (-23.0f)) <= 1.0f);
    const juce::String status =
        passed ? "[PASSED - EBU R128]"
               : (intLufs > -22.0f ? "[TOO HOT]" : "[TOO LOW]");
    const auto statusCol = passed
                               ? midas_colors::kAccentGreen
                               : (intLufs > -22.0f ? midas_colors::kAccentRed
                                                   : midas_colors::kAccentCyan);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(statusCol);
    g.drawText("INTEGRATED: " + juce::String(intLufs, 1) + " LUFS " + status,
               lBox.getX() + 10.0f, 32.0f, lufsBoxW - 20.0f, 14.0f,
               juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(9.0f, false));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("SHORT TERM: " + juce::String(shortLufs, 1) +
                   " LUFS | MOMENTARY: " + juce::String(momLufs, 1) + " LUFS",
               lBox.getX() + 10.0f, 50.0f, lufsBoxW - 20.0f, 14.0f,
               juce::Justification::centredLeft);

    g.setColour(midas_colors::kAccentGold);
    g.drawText("TRUE PEAK: " + juce::String(truePeak, 1) +
                   " dBFS | LRA: " + juce::String(lra, 1) + " LU",
               lBox.getX() + 10.0f, 68.0f, lufsBoxW - 20.0f, 14.0f,
               juce::Justification::centredLeft);

    // Dynamic Loudness Bargraph
    const float barX = lBox.getX() + 10.0f;
    const float barY = 88.0f;
    const float barW = lufsBoxW - 20.0f;
    const float barH = 14.0f;
    g.setColour(juce::Colour(0xff0a1420));
    g.fillRect(barX, barY, barW, barH);
    const float normMom = juce::jlimit(0.0f, 1.0f, (momLufs + 60.0f) / 60.0f);
    g.setColour(normMom > 0.85f
                    ? midas_colors::kAccentRed
                    : (normMom > 0.65f ? midas_colors::kAccentGold
                                       : midas_colors::kAccentGreen));
    g.fillRect(barX, barY, barW * normMom, barH);
    // Draw target marker at -23 LUFS (which is (-23 + 60)/60 = 37/60 = 0.6167)
    g.setColour(midas_colors::kAccentCyan);
    g.drawVerticalLine(static_cast<int>(barX + barW * 0.6167f), barY - 2.0f,
                       barY + barH + 2.0f);

    // Right: Mix-Minus N-1 IFB Matrix
    const float ifbX = lufsBoxW + 20.0f;
    const float ifbW = area.getWidth() - ifbX - 12.0f;
    auto ifbBox =
        juce::Rectangle<float>(ifbX, 26.0f, ifbW, area.getHeight() - 34.0f);
    g.setColour(juce::Colour(0xff060d16));
    g.fillRoundedRectangle(ifbBox, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(ifbBox, 4.0f, 1.0f);

    g.setFont(midas_ui::getFont(9.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("N-1 MIX-MINUS IFB MATRIX & LIP-SYNC DELAY",
               ifbBox.getX() + 10.0f, 32.0f, ifbW - 20.0f, 14.0f,
               juce::Justification::centredLeft);
    g.setFont(midas_ui::getFont(7.5f, false));
    g.setColour(midas_colors::kTextWhite);
    g.drawText("IFB 1: ANCHOR 1 (DUCK: -12dB) • IFB 2: ANCHOR 2 (DUCK: -12dB)",
               ifbBox.getX() + 10.0f, 50.0f, ifbW - 20.0f, 14.0f,
               juce::Justification::centredLeft);
    g.drawText("IFB 3: FIELD REPORTER (AUTO DUCK ON TB) • LIP-SYNC: 80.0ms",
               ifbBox.getX() + 10.0f, 68.0f, ifbW - 20.0f, 14.0f,
               juce::Justification::centredLeft);
  }

  void resized() override {
    const int rx = getWidth();
    btnTalkback.setBounds(rx - 208, 4, 98, 18);
    btnMonitorCr.setBounds(rx - 104, 4, 94, 18);
  }

private:
  MidasTactileButton btnTalkback{"TALKBACK MTX",
                                 MidasTactileButton::GlowMode::Red};
  MidasTactileButton btnMonitorCr{"MONITOR CR",
                                  MidasTactileButton::GlowMode::Cyan};
  float intLufs = -23.1f;
  float shortLufs = -22.4f;
  float momLufs = -21.8f;
  float truePeak = -1.2f;
  float lra = 5.6f;
};

// ==============================================================================
// 10. MULTITRACK RECORDER VIEW (64-TRACK TRANSPORT & TIMECODE)
// ==============================================================================
class MidasRecorderView : public juce::Component {
public:
  MidasRecorderView()
      : btnRec("● REC", MidasTactileButton::GlowMode::Red),
        btnPlay("▶ PLAY", MidasTactileButton::GlowMode::Green),
        btnStop("■ STOP", MidasTactileButton::GlowMode::Gray),
        btnRtz("|<< RTZ", MidasTactileButton::GlowMode::Cyan),
        btnMode64Ch("64-CH RAW", MidasTactileButton::GlowMode::Red),
        btnSelectStorage("CHANGE DRIVE", MidasTactileButton::GlowMode::Cyan),
        btnScanUsb("CYCLE USB", MidasTactileButton::GlowMode::Purple),
        btnOpenFolder("OPEN FOLDER", MidasTactileButton::GlowMode::Gold),
        btnLoadWav("LOAD WAV", MidasTactileButton::GlowMode::Gold),
        btnDawStream("DAW / V-CHECK", MidasTactileButton::GlowMode::Purple),
        btnBounce("STEMS BOUNCE", MidasTactileButton::GlowMode::Green) {
    btnMode64Ch.setClickingTogglesState(true);
    btnMode64Ch.setToggleState(
        true, juce::dontSendNotification); // Default: 64-CH RAW
    btnStop.setClickingTogglesState(false);
    btnRtz.setClickingTogglesState(false);
    btnSelectStorage.setClickingTogglesState(false);
    btnScanUsb.setClickingTogglesState(false);
    btnOpenFolder.setClickingTogglesState(false);

    targetStorageDir =
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("OpenLive_Recordings");
    if (!targetStorageDir.exists())
      targetStorageDir.createDirectory();
    updateArmGrid();
    updateStorageInfo();

    addAndMakeVisible(btnRec);
    addAndMakeVisible(btnPlay);
    addAndMakeVisible(btnStop);
    addAndMakeVisible(btnRtz);
    addAndMakeVisible(btnMode64Ch);
    addAndMakeVisible(btnSelectStorage);
    addAndMakeVisible(btnScanUsb);
    addAndMakeVisible(btnOpenFolder);
    addAndMakeVisible(btnLoadWav);
    addAndMakeVisible(btnDawStream);
    addAndMakeVisible(btnBounce);
    btnLoadWav.setClickingTogglesState(false);
    btnDawStream.setClickingTogglesState(false);
    btnBounce.setClickingTogglesState(false);

    btnMode64Ch.onClick = [this] {
      const bool is64 = btnMode64Ch.getToggleState();
      btnMode64Ch.setButtonText(is64 ? "64-CH RAW" : "2-CH MASTER");
      updateArmGrid();
      statusStr =
          is64 ? "RECORDER IDLE — 64-CH DIRECT RAW WAV (STANDALONE USB/SSD)"
               : "RECORDER IDLE — 2-CH STEREO MASTER WAV";
      updateStorageInfo();
      repaint();
    };

    btnSelectStorage.onClick = [this] {
      auto chooser = std::make_shared<juce::FileChooser>(
          "Select Storage Drive / Folder for 64-CH Multitrack & Showfiles",
          targetStorageDir, "*");
      const int flags = juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectDirectories;
      chooser->launchAsync(
          flags, [this, chooser,
                  safe = juce::Component::SafePointer<MidasRecorderView>(this)](
                     const juce::FileChooser &fc) {
            if (safe == nullptr)
              return;
            const auto res = fc.getResult();
            if (res.exists() || res.isDirectory()) {
              safe->setTargetDirectory(
                  res.isDirectory() ? res : res.getParentDirectory());
            }
          });
    };

    btnScanUsb.onClick = [this] { scanAndSelectUsbDrive(); };

    btnOpenFolder.onClick = [this] {
      if (!targetStorageDir.exists())
        targetStorageDir.createDirectory();
      targetStorageDir.startAsProcess();
    };

    btnRec.onClick = [this] {
      if (onRecordToggled)
        onRecordToggled(btnRec.getToggleState());
    };
    btnPlay.onClick = [this] {
      if (onPlayToggled)
        onPlayToggled(btnPlay.getToggleState());
    };
    btnStop.onClick = [this] {
      btnRec.setToggleState(false, juce::dontSendNotification);
      btnPlay.setToggleState(false, juce::dontSendNotification);
      if (onStopClicked)
        onStopClicked();
    };
    btnRtz.onClick = [this] {
      if (onRtzClicked)
        onRtzClicked();
    };
    btnLoadWav.onClick = [this] {
      if (onLoadWavClicked)
        onLoadWavClicked();
    };
    btnDawStream.onClick = [this] {
      if (onOpenDawStream)
        onOpenDawStream();
    };
    btnBounce.onClick = [this] {
      if (onOpenBounce)
        onOpenBounce();
    };
  }

  std::function<void(bool recActive)> onRecordToggled;
  std::function<void(bool playActive)> onPlayToggled;
  std::function<void()> onStopClicked;
  std::function<void()> onRtzClicked;
  std::function<void()> onLoadWavClicked;
  std::function<void()> onOpenDawStream;
  std::function<void()> onOpenBounce;

  void setTimecode(const juce::String &tc) {
    timecodeStr = tc;
    repaint();
  }
  void setProgress(float prog) {
    progress = juce::jlimit(0.0f, 1.0f, prog);
    repaint();
  }
  void setRecordingState(bool isRec, const juce::String &status = "") {
    btnRec.setToggleState(isRec, juce::dontSendNotification);
    if (status.isNotEmpty())
      statusStr = status;
    repaint();
  }
  void setPlaybackState(bool isPlay) {
    btnPlay.setToggleState(isPlay, juce::dontSendNotification);
    repaint();
  }
  bool is64ChannelMode() const noexcept { return btnMode64Ch.getToggleState(); }

  void updateArmGrid() {
    const bool is64 = btnMode64Ch.getToggleState();
    for (int i = 0; i < 64; ++i)
      armedTracks[static_cast<size_t>(i)] = is64 ? true : (i < 2);
  }

  void setArmState(int trackIdx, bool armed) {
    if (trackIdx >= 0 && trackIdx < 64) {
      armedTracks[static_cast<size_t>(trackIdx)] = armed;
      repaint();
    }
  }

  void setTargetDirectory(const juce::File &dir) {
    targetStorageDir = dir;
    if (!targetStorageDir.exists())
      targetStorageDir.createDirectory();
    updateStorageInfo();
    repaint();
  }

  juce::File getTargetDirectory() const {
    if (!targetStorageDir.exists())
      targetStorageDir.createDirectory();
    return targetStorageDir;
  }

  void scanAndSelectUsbDrive() {
    juce::Array<juce::File> roots;
    juce::File::findFileSystemRoots(roots);
    juce::Array<juce::File> candidateDrives;
    for (const auto &r : roots) {
      if (r.isDirectory() && r.exists()) {
        candidateDrives.add(r);
      }
    }
    if (!candidateDrives.isEmpty()) {
      currentDriveIdx = (currentDriveIdx + 1) % candidateDrives.size();
      auto target =
          candidateDrives[currentDriveIdx].getChildFile("OpenLive_Recordings");
      setTargetDirectory(target);
    }
  }

  void updateStorageInfo() {
    if (!targetStorageDir.exists())
      targetStorageDir.createDirectory();

    const juce::int64 freeBytes = targetStorageDir.getBytesFreeOnVolume();
    const juce::int64 totalBytes = targetStorageDir.getVolumeTotalSize();

    const double freeGb =
        static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);
    const double totalGb =
        static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);
    const double freePercent = (totalBytes > 0)
                                   ? (static_cast<double>(freeBytes) /
                                      static_cast<double>(totalBytes) * 100.0)
                                   : 0.0;

    const bool is64 = is64ChannelMode();
    const double bytesPerSec =
        (is64 ? 64.0 : 2.0) * 96000.0 * 3.0; // 24-bit 96kHz PCM
    const double remSec = (bytesPerSec > 0.0)
                              ? (static_cast<double>(freeBytes) / bytesPerSec)
                              : 0.0;
    const int remHours = static_cast<int>(remSec / 3600.0);
    const int remMins = static_cast<int>(std::fmod(remSec, 3600.0) / 60.0);

    storageInfoStr =
        juce::String::formatted("FREE: %.1f GB / %.1f GB (%.1f%%)  |  EST. "
                                "TIME: ~%dh %02dm (%s 96kHz 24-bit)",
                                freeGb, totalGb, freePercent, remHours, remMins,
                                is64 ? "64-CH" : "2-CH");
    storageUsageRatio =
        (totalBytes > 0)
            ? static_cast<float>(1.0 - (static_cast<double>(freeBytes) /
                                        static_cast<double>(totalBytes)))
            : 0.0f;
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // SMPTE Timecode Box
    auto tcRect = juce::Rectangle<float>(12.0f, 12.0f, 210.0f, 36.0f);
    g.setColour(juce::Colour(0xff03070d));
    g.fillRoundedRectangle(tcRect, 4.0f);
    g.setColour(midas_colors::kAccentGreen.withAlpha(0.6f));
    g.drawRoundedRectangle(tcRect, 4.0f, 1.0f);

    g.setFont(midas_ui::getFont(18.0f, true));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(timecodeStr, tcRect.toNearestInt(),
               juce::Justification::centred);

    // Timeline Scrub Bar
    const float tlX = 232.0f;
    const float tlW = area.getWidth() - tlX - 12.0f;
    auto tlRect = juce::Rectangle<float>(tlX, 16.0f, tlW, 28.0f);
    g.setColour(juce::Colour(0xff060d16));
    g.fillRoundedRectangle(tlRect, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(tlRect, 4.0f, 1.0f);

    // Progress
    g.setColour(midas_colors::kAccentCyan.withAlpha(0.35f));
    g.fillRoundedRectangle(tlX, 16.0f, tlW * progress, 28.0f, 4.0f);
    g.setColour(midas_colors::kAccentGold);
    g.drawVerticalLine(static_cast<int>(tlX + tlW * progress), 16.0f, 44.0f);

    // Status String
    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(btnRec.getToggleState() ? midas_colors::kAccentRed
                                        : midas_colors::kAccentGreen);
    g.drawText(statusStr, static_cast<int>(tlX + 8.0f), 20,
               static_cast<int>(tlW - 16.0f), 20,
               juce::Justification::centredLeft);

    // 64-track arm grid preview
    const float gridY = 56.0f;
    const float gridW = area.getWidth() - 24.0f;
    const float blockW = gridW / 32.0f;
    for (int r = 0; r < 2; ++r) {
      for (int c = 0; c < 32; ++c) {
        const int trk = r * 32 + c;
        const float bx = 12.0f + c * blockW;
        const float by = gridY + r * 14.0f;
        const bool armed =
            (trk < 64) ? armedTracks[static_cast<size_t>(trk)] : false;
        g.setColour(armed ? midas_colors::kAccentRed.withAlpha(0.7f)
                          : juce::Colour(0xff182436));
        g.fillRoundedRectangle(bx + 1.0f, by + 1.0f, blockW - 2.0f, 11.0f,
                               2.0f);
      }
    }

    // Media Storage Box (Drive, Free Space, Remaining Time Estimation)
    const float btmY = area.getHeight() - 30.0f;
    const float storageY = 88.0f;
    const float storageH = std::max(48.0f, btmY - storageY - 6.0f);
    const float storageW = area.getWidth() - 24.0f;
    auto storageBox =
        juce::Rectangle<float>(12.0f, storageY, storageW, storageH);

    g.setColour(juce::Colour(0xff060c14));
    g.fillRoundedRectangle(storageBox, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(storageBox, 4.0f, 1.0f);

    // Title
    g.setFont(midas_ui::getFont(9.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("MEDIA STORAGE / TARGET DRIVE (RAW MULTITRACK & SHOWFILES)", 20,
               static_cast<int>(storageY + 5.0f),
               static_cast<int>(storageW - 300.0f), 13,
               juce::Justification::centredLeft);

    // Current Storage Path
    g.setFont(midas_ui::getFont(10.5f, true));
    g.setColour(juce::Colours::white);
    const juce::String displayPath =
        "PATH: " + targetStorageDir.getFullPathName();
    g.drawText(displayPath, 20, static_cast<int>(storageY + 19.0f),
               static_cast<int>(storageW - 300.0f), 15,
               juce::Justification::centredLeft, true);

    // Storage capacity & estimate stats
    g.setFont(midas_ui::getFont(9.0f, true));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(storageInfoStr, 20, static_cast<int>(storageY + 34.0f),
               static_cast<int>(storageW - 300.0f), 13,
               juce::Justification::centredLeft);

    // Storage Usage Bar
    const float barX = 20.0f;
    const float barY = storageY + storageH - 8.0f;
    const float barW = storageW - 300.0f;
    g.setColour(juce::Colour(0xff121d2b));
    g.fillRoundedRectangle(barX, barY, barW, 4.0f, 2.0f);
    juce::Colour barCol = midas_colors::kAccentGreen;
    if (storageUsageRatio > 0.90f)
      barCol = midas_colors::kAccentRed;
    else if (storageUsageRatio > 0.75f)
      barCol = midas_colors::kAccentGold;
    g.setColour(barCol);
    g.fillRoundedRectangle(barX, barY,
                           barW * juce::jlimit(0.0f, 1.0f, storageUsageRatio),
                           4.0f, 2.0f);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const int btmY = static_cast<int>(area.getHeight() - 30.0f);
    btnRtz.setBounds(12, btmY, 56, 22);
    btnPlay.setBounds(72, btmY, 56, 22);
    btnStop.setBounds(132, btmY, 56, 22);
    btnRec.setBounds(192, btmY, 56, 22);
    btnMode64Ch.setBounds(252, btmY, 92, 22);
    btnLoadWav.setBounds(348, btmY, 72, 22);
    btnDawStream.setBounds(424, btmY, 92, 22);
    btnBounce.setBounds(520, btmY, 92, 22);

    const int rightEdge = static_cast<int>(area.getWidth() - 16.0f);
    const int stBtnY = 92;
    btnOpenFolder.setBounds(rightEdge - 86, stBtnY, 82, 22);
    btnScanUsb.setBounds(rightEdge - 174, stBtnY, 84, 22);
    btnSelectStorage.setBounds(rightEdge - 280, stBtnY, 102, 22);
  }

  MidasTactileButton btnRec, btnPlay, btnStop, btnRtz;
  MidasTactileButton btnMode64Ch;
  MidasTactileButton btnSelectStorage, btnScanUsb, btnOpenFolder;
  MidasTactileButton btnLoadWav, btnDawStream, btnBounce;

private:
  juce::String timecodeStr = "00:00:00:00";
  float progress = 0.0f;
  juce::String statusStr =
      "RECORDER IDLE — 64-CH DIRECT RAW WAV (NO DAW NEEDED)";
  std::array<bool, 64> armedTracks{};
  juce::File targetStorageDir;
  juce::String storageInfoStr = "FREE: SCANNING...";
  float storageUsageRatio = 0.0f;
  int currentDriveIdx = 0;
};

// ==============================================================================
// 11. HARDWARE PATCHBAY & STAGEBOX VIEW
// ==============================================================================
class MidasPatchbayView : public juce::Component {
public:
  MidasPatchbayView()
      : btnOpenMatrix("PRO CROSSPOINT MATRIX",
                      MidasTactileButton::GlowMode::Cyan),
        btnStageLock("48kHz SYNC", MidasTactileButton::GlowMode::Green) {
    setRepaintsOnMouseActivity(true);
    addAndMakeVisible(btnOpenMatrix);
    addAndMakeVisible(btnStageLock);
    btnOpenMatrix.setClickingTogglesState(false);
    btnStageLock.setToggleState(true, juce::dontSendNotification);

    for (auto &st : patchActive)
      st = true;

    btnOpenMatrix.onClick = [this] {
      if (onOpenPatchMatrix)
        onOpenPatchMatrix();
    };
    btnStageLock.onClick = [this] { repaint(); };
  }

  std::function<void()> onOpenPatchMatrix;
  std::function<void(int patchIdx, bool active)> onPatchTileToggled;

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("PC AUDIO PATCH MATRIX & 8 DIRECT ROUTING TILES (CLICK TO "
               "PATCH/UNPATCH)",
               12, 6, 450, 14, juce::Justification::centredLeft);

    const float tileW = (area.getWidth() - 24.0f - 3.0f * 6.0f) / 4.0f;
    const float tileH = (area.getHeight() - 34.0f - 6.0f) * 0.5f;
    static const char *patches[8] = {
        "LOCAL HW 01 -> CH 01 (KICK)",    "LOCAL HW 02 -> CH 02 (SNARE)",
        "LOCAL HW 03 -> CH 03 (HIHAT)",   "LOCAL HW 04 -> CH 04 (BASS)",
        "MASTER L/R -> HW OUT 1-2 (PA)",  "AUX 1-2 -> HW OUT 3-4 (IEM)",
        "VIRT IN 01 -> CH 17 (PC MEDIA)", "VIRT OUT 01 -> STREAM (DAW)"};

    for (int i = 0; i < 8; ++i) {
      const int row = i / 4;
      const int col = i % 4;
      const float tx = 12.0f + col * (tileW + 6.0f);
      const float ty = 26.0f + row * (tileH + 6.0f);

      auto r = juce::Rectangle<float>(tx, ty, tileW, tileH);
      const bool active = patchActive[static_cast<size_t>(i)];
      g.setColour(active ? juce::Colour(0xff0b1c2c) : juce::Colour(0xff060d16));
      g.fillRoundedRectangle(r, 4.0f);
      g.setColour(active ? midas_colors::kAccentCyan.withAlpha(0.75f)
                         : midas_colors::kBorderSubtle);
      g.drawRoundedRectangle(r, 4.0f, active ? 1.3f : 1.0f);

      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(active ? midas_colors::kAccentGold
                         : midas_colors::kTextMuted);
      g.drawText(juce::String("PATCH 0") + juce::String(i + 1) +
                     (active ? "  [● 48kHz LOCK]" : "  [○ UNPATCHED]"),
                 tx + 6.0f, ty + 5.0f, tileW - 12.0f, 12.0f,
                 juce::Justification::centredLeft);

      g.setFont(midas_ui::getFont(7.5f, false));
      g.setColour(active ? midas_colors::kTextWhite : midas_colors::kTextMuted);
      g.drawText(patches[i], tx + 6.0f, ty + 20.0f, tileW - 12.0f, 14.0f,
                 juce::Justification::centredLeft);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.position.y < 26.0f)
      return;
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 24.0f - 3.0f * 6.0f) / 4.0f;
    const float tileH = (area.getHeight() - 34.0f - 6.0f) * 0.5f;
    const int col = static_cast<int>((e.position.x - 12.0f) / (tileW + 6.0f));
    const int row = static_cast<int>((e.position.y - 26.0f) / (tileH + 6.0f));
    if (col >= 0 && col < 4 && row >= 0 && row < 2) {
      const int idx = row * 4 + col;
      patchActive[static_cast<size_t>(idx)] =
          !patchActive[static_cast<size_t>(idx)];
      repaint();
      if (onPatchTileToggled)
        onPatchTileToggled(idx, patchActive[static_cast<size_t>(idx)]);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    btnStageLock.setBounds(static_cast<int>(area.getWidth() - 280.0f), 4, 96,
                           21);
    btnOpenMatrix.setBounds(static_cast<int>(area.getWidth() - 176.0f), 4, 170,
                            21);
  }

private:
  MidasTactileButton btnOpenMatrix, btnStageLock;
  std::array<bool, 8> patchActive{};
};

// ==============================================================================
// 12. CUE LIST THEATRE AUTOMATION VIEW
// ==============================================================================
class MidasCueListView : public juce::Component {
public:
  MidasCueListView()
      : btnNext("GO NEXT CUE ▶", MidasTactileButton::GlowMode::Green),
        btnPrev("◀ PREV", MidasTactileButton::GlowMode::Cyan),
        btnStore("+ STORE CUE", MidasTactileButton::GlowMode::Gold),
        btnOpenCueMgr("CUE MANAGER", MidasTactileButton::GlowMode::Cyan) {
    setRepaintsOnMouseActivity(true);
    addAndMakeVisible(btnNext);
    addAndMakeVisible(btnPrev);
    addAndMakeVisible(btnStore);
    addAndMakeVisible(btnOpenCueMgr);
    btnNext.setClickingTogglesState(false);
    btnPrev.setClickingTogglesState(false);
    btnStore.setClickingTogglesState(false);
    btnOpenCueMgr.setClickingTogglesState(false);

    cueItems = {
        "CUE 1.0 — SHOW OPENER (SCENE 01: FOH ARENA LIVE) • GLIDE: 2.0s",
        "CUE 2.0 — VERSE ACOUSTIC (SCENE 02: ACOUSTIC UNPLUGGED) • GLIDE: 3.5s",
        "CUE 3.0 — DRUM SOLO PUNCH (SCENE 03: DRUM SOLO) • GLIDE: 1.0s",
        "CUE 4.0 — BROADCAST 4K STEM (SCENE 04: BROADCAST STEM) • GLIDE: 2.5s",
        "CUE 5.0 — ALL FLAT UNITY (SCENE 05: UNITY 0dB RESET) • GLIDE: 0.5s"};

    btnPrev.onClick = [this] {
      if (!cueItems.empty()) {
        activeCueIndex =
            (activeCueIndex - 1 + static_cast<int>(cueItems.size())) %
            static_cast<int>(cueItems.size());
        repaint();
        if (onCueRecalled)
          onCueRecalled(activeCueIndex + 1);
      }
    };
    btnNext.onClick = [this] {
      if (!cueItems.empty()) {
        activeCueIndex =
            (activeCueIndex + 1) % static_cast<int>(cueItems.size());
        repaint();
        if (onCueRecalled)
          onCueRecalled(activeCueIndex + 1);
      }
    };
    btnStore.onClick = [this] {
      const int nextNum = static_cast<int>(cueItems.size()) + 1;
      const auto nowStr = juce::Time::getCurrentTime().formatted("%H:%M:%S");
      cueItems.push_back("CUE " + juce::String(nextNum) +
                         ".0 — USER SNAPSHOT @ " + nowStr + " • GLIDE: 2.0s");
      activeCueIndex = static_cast<int>(cueItems.size()) - 1;
      repaint();
      if (onCueStored)
        onCueStored(activeCueIndex + 1);
    };
    btnOpenCueMgr.onClick = [this] {
      if (onOpenCueManager)
        onOpenCueManager();
    };
  }

  std::function<void(int sceneId)> onCueRecalled;
  std::function<void(int cueCount)> onCueStored;
  std::function<void()> onOpenCueManager;

  void setActiveSceneId(int sceneId) {
    if (sceneId >= 1 && sceneId <= static_cast<int>(cueItems.size())) {
      activeCueIndex = sceneId - 1;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText(
        "THEATRE CUE LIST & CROSSFADE GLIDE AUTOMATION (CLICK ROW TO RECALL)",
        12, 6, 480, 14, juce::Justification::centredLeft);

    const int visibleCount = juce::jmin(4, static_cast<int>(cueItems.size()));
    const int startIdx = juce::jmax(0, activeCueIndex - 2);
    for (int row = 0; row < visibleCount; ++row) {
      const int idx = startIdx + row;
      if (idx >= static_cast<int>(cueItems.size()))
        break;
      const float cy = 24.0f + row * 18.0f;
      auto r =
          juce::Rectangle<float>(12.0f, cy, area.getWidth() - 24.0f, 16.0f);
      const bool isActive = (idx == activeCueIndex);
      g.setColour(isActive ? midas_colors::kAccentCyan.withAlpha(0.22f)
                           : juce::Colour(0xff060d16));
      g.fillRoundedRectangle(r, 3.0f);
      if (isActive) {
        g.setColour(midas_colors::kAccentCyan.withAlpha(0.7f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
      }
      g.setFont(midas_ui::getFont(8.0f, isActive));
      g.setColour(isActive ? midas_colors::kAccentCyan
                           : midas_colors::kTextWhite);
      g.drawText((isActive ? "▶ " : "   ") + cueItems[static_cast<size_t>(idx)],
                 static_cast<int>(r.getX() + 8.0f), static_cast<int>(r.getY()),
                 static_cast<int>(r.getWidth() - 16.0f), 16,
                 juce::Justification::centredLeft);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.position.y >= 24.0f && e.position.y < 96.0f) {
      const int row = static_cast<int>((e.position.y - 24.0f) / 18.0f);
      const int startIdx = juce::jmax(0, activeCueIndex - 2);
      const int idx = startIdx + row;
      if (idx >= 0 && idx < static_cast<int>(cueItems.size())) {
        activeCueIndex = idx;
        repaint();
        if (onCueRecalled)
          onCueRecalled(activeCueIndex + 1);
      }
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float btmY = area.getHeight() - 28.0f;
    btnPrev.setBounds(12, static_cast<int>(btmY), 76, 21);
    btnNext.setBounds(94, static_cast<int>(btmY), 116, 21);
    btnStore.setBounds(216, static_cast<int>(btmY), 96, 21);
    btnOpenCueMgr.setBounds(318, static_cast<int>(btmY), 106, 21);
  }

private:
  MidasTactileButton btnNext, btnPrev, btnStore, btnOpenCueMgr;
  std::vector<juce::String> cueItems;
  int activeCueIndex = 0;
};

// ==============================================================================
// 13. CONSOLE SETUP & TEST OSCILLATOR VIEW
// ==============================================================================
class MidasSetupView : public juce::Component {
public:
  MidasSetupView()
      : btnOscSine("SINE", MidasTactileButton::GlowMode::Cyan),
        btnOscPink("PINK NOISE", MidasTactileButton::GlowMode::Gold),
        btnOscWhite("WHITE NOISE", MidasTactileButton::GlowMode::Gray),
        btnOscInject("INJECT MAIN", MidasTactileButton::GlowMode::Red),
        btnOscInjectAux("INJECT AUX 1-2", MidasTactileButton::GlowMode::Purple),
        oscFreqKnob("FREQ", 20.0f, 20000.0f, 1000.0f, "Hz",
                    MidasKnob::NotchColor::Cyan),
        oscLevelKnob("LEVEL", -60.0f, 0.0f, -18.0f, "dB",
                     MidasKnob::NotchColor::Gold) {
    addAndMakeVisible(btnOscSine);
    addAndMakeVisible(btnOscPink);
    addAndMakeVisible(btnOscWhite);
    addAndMakeVisible(btnOscInject);
    addAndMakeVisible(btnOscInjectAux);
    addAndMakeVisible(oscFreqKnob);
    addAndMakeVisible(oscLevelKnob);
    btnOscSine.setToggleState(true, juce::dontSendNotification);

    btnOscSine.onClick = [this] {
      btnOscSine.setToggleState(true, juce::dontSendNotification);
      btnOscPink.setToggleState(false, juce::dontSendNotification);
      btnOscWhite.setToggleState(false, juce::dontSendNotification);
      if (onOscTypeChanged)
        onOscTypeChanged(0);
    };
    btnOscPink.onClick = [this] {
      btnOscSine.setToggleState(false, juce::dontSendNotification);
      btnOscPink.setToggleState(true, juce::dontSendNotification);
      btnOscWhite.setToggleState(false, juce::dontSendNotification);
      if (onOscTypeChanged)
        onOscTypeChanged(1);
    };
    btnOscWhite.onClick = [this] {
      btnOscSine.setToggleState(false, juce::dontSendNotification);
      btnOscPink.setToggleState(false, juce::dontSendNotification);
      btnOscWhite.setToggleState(true, juce::dontSendNotification);
      if (onOscTypeChanged)
        onOscTypeChanged(2);
    };

    oscFreqKnob.onValueChanged = [this](float f) {
      if (onOscFreqChanged)
        onOscFreqChanged(f);
    };
    oscLevelKnob.onValueChanged = [this](float l) {
      if (onOscLevelChanged)
        onOscLevelChanged(l);
    };
    btnOscInject.onClick = [this] {
      if (onOscInjectMainToggled)
        onOscInjectMainToggled(btnOscInject.getToggleState());
    };
    btnOscInjectAux.onClick = [this] {
      if (onOscInjectAuxToggled)
        onOscInjectAuxToggled(btnOscInjectAux.getToggleState());
    };
  }

  std::function<void(int type)> onOscTypeChanged;
  std::function<void(float freq)> onOscFreqChanged;
  std::function<void(float lvlDb)> onOscLevelChanged;
  std::function<void(bool injectMain)> onOscInjectMainToggled;
  std::function<void(bool injectAux)> onOscInjectAuxToggled;
  std::function<void()> onOpenAudioSettings;
  std::function<void()> onOpenAsioBridge;
  std::function<void()> onOpenOscServer;

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090f18));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TEST SIGNAL GENERATOR & DUAL REDUNDANT PSU TELEMETRY", 12, 6,
               static_cast<int>(area.getWidth()) - 24, 14,
               juce::Justification::centredLeft);

    // PSU Telemetry Box
    const float psuW = area.getWidth() * 0.40f;
    auto pBox =
        juce::Rectangle<float>(12.0f, 26.0f, psuW, area.getHeight() - 34.0f);
    g.setColour(juce::Colour(0xff060d16));
    g.fillRoundedRectangle(pBox, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(pBox, 4.0f, 1.0f);

    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText("PRIMARY PSU 1: 52.4V DC | 12.8A [HEALTHY]", pBox.getX() + 8.0f,
               32.0f, psuW - 16.0f, 14.0f, juce::Justification::centredLeft);
    g.drawText("BACKUP PSU 2:  52.3V DC | 12.6A [HEALTHY]", pBox.getX() + 8.0f,
               48.0f, psuW - 16.0f, 14.0f, juce::Justification::centredLeft);
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("CHASSIS TEMP: 41.5 C | FAN RPM: 1850 (SILENT)",
               pBox.getX() + 8.0f, 66.0f, psuW - 16.0f, 14.0f,
               juce::Justification::centredLeft);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const bool isCompact = (area.getHeight() < 220.0f);
    const float oscX = area.getWidth() * 0.40f + 16.0f;
    const float btmY = area.getHeight() - (isCompact ? 26.0f : 32.0f);

    const int btnH = isCompact ? 18 : 22;
    const int knobSz = isCompact ? 46 : 52;
    const int knobY = isCompact ? 16 : 22;

    btnOscSine.setBounds(static_cast<int>(oscX), 24, 65, btnH);
    btnOscPink.setBounds(static_cast<int>(oscX + 68.0f), 24, 75, btnH);
    btnOscWhite.setBounds(static_cast<int>(oscX + 146.0f), 24, 80, btnH);

    oscFreqKnob.setBounds(static_cast<int>(oscX + 232.0f), knobY, knobSz,
                          knobSz);
    oscLevelKnob.setBounds(static_cast<int>(oscX + 232.0f + knobSz + 4.0f),
                           knobY, knobSz, knobSz);

    const int injH = isCompact ? 20 : 24;
    btnOscInject.setBounds(static_cast<int>(oscX), static_cast<int>(btmY), 110,
                           injH);
    btnOscInjectAux.setBounds(static_cast<int>(oscX + 116.0f),
                              static_cast<int>(btmY), 120, injH);
  }

private:
  MidasTactileButton btnOscSine, btnOscPink, btnOscWhite, btnOscInject,
      btnOscInjectAux;
  MidasKnob oscFreqKnob, oscLevelKnob;
};

// ==============================================================================
// QUICK CHANNEL OVERVIEW STRIP (BOTTOM OF TOUCHSCREEN HUB)
// ==============================================================================
class MidasQuickStrip : public juce::Component {
public:
  MidasQuickStrip()
      : preampKnob("PREAMP", -12.0f, 60.0f, 24.0f, "dB",
                   MidasKnob::NotchColor::Cyan),
        driveKnob("XL4 DRIVE", 0.0f, 100.0f, 15.0f, "%",
                  MidasKnob::NotchColor::Gold),
        hpfKnob("HPF", 20.0f, 400.0f, 80.0f, "Hz", MidasKnob::NotchColor::Cyan),
        delayKnob("DELAY", 0.0f, 500.0f, 0.0f, "ms",
                  MidasKnob::NotchColor::Cyan),
        panKnob("PAN", -50.0f, 50.0f, 0.0f, "", MidasKnob::NotchColor::Cyan),
        btn48V("48V", MidasTactileButton::GlowMode::Red),
        btnPhase("Ø REV", MidasTactileButton::GlowMode::Gray),
        btnHpf("HPF ON", MidasTactileButton::GlowMode::Cyan),
        btnDelay("DLY ON", MidasTactileButton::GlowMode::Cyan),
        btnPanLaw("-3.0dB", MidasTactileButton::GlowMode::Gold),
        btnSurround3D("3D PAN", MidasTactileButton::GlowMode::Purple) {

    addAndMakeVisible(preampKnob);
    addAndMakeVisible(driveKnob);
    addAndMakeVisible(hpfKnob);
    addAndMakeVisible(delayKnob);
    addAndMakeVisible(panKnob);

    addAndMakeVisible(btn48V);
    addAndMakeVisible(btnPhase);
    addAndMakeVisible(btnHpf);
    addAndMakeVisible(btnDelay);
    addAndMakeVisible(btnPanLaw);
    addAndMakeVisible(btnSurround3D);
    btnSurround3D.setClickingTogglesState(false);
    btnSurround3D.onClick = [this] {
      if (onOpenSurroundPan)
        onOpenSurroundPan();
    };
  }

  std::function<void()> onOpenSurroundPan;
  std::function<void(float gainDb)> onPreampChanged;
  std::function<void(float drive)> onDriveChanged;
  std::function<void(bool en, float freq)> onHpfChanged;
  std::function<void(bool en, float dlyMs)> onDelayChanged;
  std::function<void(float panVal)> onPanChanged;
  std::function<void(bool p48)> on48vToggled;
  std::function<void(bool phase)> onPhaseToggled;
  std::function<void(int tabIndex)>
      onQuickTileClicked; // 0=EQ, 1=Comp, 2=Gate, 3=Routing

  void syncChannel(const ChannelState &state) {
    curMasterAssigned = state.masterAssigned.load(std::memory_order_relaxed);
    curMonAssigned = state.monitorAssigned.load(std::memory_order_relaxed);
    curMonPre = state.monitorPreFader.load(std::memory_order_relaxed);
    curMonSendDb = state.monitorSendDb.load(std::memory_order_relaxed);
    curCompEnabled = state.compressorEnabled.load(std::memory_order_relaxed);
    curGateEnabled = state.gateEnabled.load(std::memory_order_relaxed);
    curCompRatio = state.compressorRatio.load(std::memory_order_relaxed);
    curCompGrDb =
        state.compressorGainReductionDb.load(std::memory_order_relaxed);
    preampKnob.setValue(state.inputGainDb.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    hpfKnob.setValue(state.highPassFreqHz.load(std::memory_order_relaxed),
                     juce::dontSendNotification);
    panKnob.setValue(state.pan.load(std::memory_order_relaxed) * 50.0f,
                     juce::dontSendNotification);
    delayKnob.setValue(state.inputDelayMs.load(std::memory_order_relaxed),
                       juce::dontSendNotification);
    btnHpf.setToggleState(state.highPassEnabled.load(std::memory_order_relaxed),
                          juce::dontSendNotification);
    btnPhase.setToggleState(
        state.polarityInverted.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 28.0f) / 8.0f;
    const float tileH = area.getHeight();

    // 8 Sub-panels / summary tiles
    for (int i = 0; i < 8; ++i) {
      const float x = i * (tileW + 4.0f);
      auto r = juce::Rectangle<float>(x, 0.0f, tileW, tileH);
      g.setColour(midas_colors::kSurfaceBg);
      g.fillRoundedRectangle(r, 5.0f);
      g.setColour(midas_colors::kBorderCol);
      g.drawRoundedRectangle(r, 5.0f, 1.0f);

      g.setFont(midas_ui::getFont(8.0f, true));
      g.setColour(midas_colors::kTextMuted);

      if (i == 5) {
        g.drawText("EQUALISER", x + 4.0f, 3.0f, tileW - 8.0f, 11.0f,
                   juce::Justification::centred);
        g.setColour(midas_colors::kAccentCyan);
        g.drawText("4-BAND PEQ\nIN: 96kHz", x + 2.0f, 16.0f, tileW - 4.0f,
                   22.0f, juce::Justification::centred);
      } else if (i == 6) {
        g.drawText("DYNAMICS", x + 4.0f, 3.0f, tileW - 8.0f, 11.0f,
                   juce::Justification::centred);
        g.setColour(curCompEnabled ? midas_colors::kAccentGold
                                   : midas_colors::kTextMuted);
        const juce::String line1 =
            (curCompEnabled ? "CMP " : "BYP ") + juce::String(curCompRatio, 1) +
            ":1";
        const juce::String grTxt =
            curCompGrDb > 0.05f ? ("-" + juce::String(curCompGrDb, 1)) : "0.0";
        const juce::String line2 =
            "GR:" + grTxt + "dB | " + (curGateEnabled ? "GT:ON" : "GT:OFF");
        g.drawText(line1 + "\n" + line2, x + 2.0f, 15.0f, tileW - 4.0f, 24.0f,
                   juce::Justification::centred);
      } else if (i == 7) {
        g.drawText("BUS ROUTING", x + 4.0f, 3.0f, tileW - 8.0f, 11.0f,
                   juce::Justification::centred);
        g.setColour(curMasterAssigned ? midas_colors::kAccentGold
                                      : midas_colors::kTextMuted);
        const juce::String mstStr = curMasterAssigned ? "MST: ON" : "MST: OFF";
        const juce::String monStr = curMonAssigned
                                        ? (curMonPre ? "MON: PRE" : "MON: POST")
                                        : "MON: OFF";
        g.drawText(mstStr + " | " + monStr, x + 2.0f, 15.0f, tileW - 4.0f,
                   12.0f, juce::Justification::centred);
        g.setColour(midas_colors::kAccentCyan);
        const juce::String sndStr =
            "SND: " + juce::String(curMonSendDb, 1) + " dB";
        g.drawText(sndStr, x + 2.0f, 27.0f, tileW - 4.0f, 12.0f,
                   juce::Justification::centred);
      }
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float tileW = (area.getWidth() - 28.0f) / 8.0f;
    const float tileH = area.getHeight();

    // Tile 0: PREAMP + 48V + Phase
    preampKnob.setBounds(static_cast<int>(0.0f * (tileW + 4.0f) + 2.0f), 2,
                         static_cast<int>(tileW * 0.55f),
                         static_cast<int>(tileH - 4.0f));
    const float btnX0 = 0.0f * (tileW + 4.0f) + tileW * 0.57f;
    btn48V.setBounds(static_cast<int>(btnX0), 6, static_cast<int>(tileW * 0.4f),
                     static_cast<int>((tileH - 14.0f) * 0.5f));
    btnPhase.setBounds(static_cast<int>(btnX0),
                       static_cast<int>(tileH * 0.5f + 1.0f),
                       static_cast<int>(tileW * 0.4f),
                       static_cast<int>((tileH - 14.0f) * 0.5f));

    // Tile 1: XL4 DRIVE
    driveKnob.setBounds(
        static_cast<int>(1.0f * (tileW + 4.0f) + (tileW - 42.0f) * 0.5f), 2, 42,
        static_cast<int>(tileH - 4.0f));

    // Tile 2: HPF + ON
    hpfKnob.setBounds(static_cast<int>(2.0f * (tileW + 4.0f) + 2.0f), 2,
                      static_cast<int>(tileW * 0.55f),
                      static_cast<int>(tileH - 4.0f));
    btnHpf.setBounds(static_cast<int>(2.0f * (tileW + 4.0f) + tileW * 0.57f),
                     14, static_cast<int>(tileW * 0.4f), 22);

    // Tile 3: DELAY + ON
    delayKnob.setBounds(static_cast<int>(3.0f * (tileW + 4.0f) + 2.0f), 2,
                        static_cast<int>(tileW * 0.55f),
                        static_cast<int>(tileH - 4.0f));
    btnDelay.setBounds(static_cast<int>(3.0f * (tileW + 4.0f) + tileW * 0.57f),
                       14, static_cast<int>(tileW * 0.4f), 22);

    // Tile 4: PAN + PAN LAW + 3D PAN
    panKnob.setBounds(static_cast<int>(4.0f * (tileW + 4.0f) + 2.0f), 2,
                      static_cast<int>(tileW * 0.50f),
                      static_cast<int>(tileH - 4.0f));
    const float panBtnX = 4.0f * (tileW + 4.0f) + tileW * 0.52f;
    const float panBtnW = tileW * 0.45f;
    btnPanLaw.setBounds(static_cast<int>(panBtnX), 4, static_cast<int>(panBtnW),
                        21);
    btnSurround3D.setBounds(static_cast<int>(panBtnX), 28,
                            static_cast<int>(panBtnW), 24);

    preampKnob.onValueChanged = [this](float val) {
      if (onPreampChanged)
        onPreampChanged(val);
    };
    driveKnob.onValueChanged = [this](float val) {
      if (onDriveChanged)
        onDriveChanged(val);
    };
    hpfKnob.onValueChanged = [this](float val) {
      if (onHpfChanged)
        onHpfChanged(btnHpf.getToggleState(), val);
    };
    delayKnob.onValueChanged = [this](float val) {
      if (onDelayChanged)
        onDelayChanged(btnDelay.getToggleState(), val);
    };
    panKnob.onValueChanged = [this](float val) {
      if (onPanChanged)
        onPanChanged(val / 50.0f);
    };

    btn48V.onClick = [this] {
      if (on48vToggled)
        on48vToggled(btn48V.getToggleState());
    };
    btnPhase.onClick = [this] {
      if (onPhaseToggled)
        onPhaseToggled(btnPhase.getToggleState());
    };
    btnHpf.onClick = [this] {
      if (onHpfChanged)
        onHpfChanged(btnHpf.getToggleState(), hpfKnob.getValue());
    };
    btnDelay.onClick = [this] {
      if (onDelayChanged)
        onDelayChanged(btnDelay.getToggleState(), delayKnob.getValue());
    };
  }

  void mouseUp(const juce::MouseEvent &e) override {
    const float tileW = (getWidth() - 28.0f) / 8.0f;
    const int clickedIdx = static_cast<int>(e.position.x / (tileW + 4.0f));
    if (clickedIdx >= 5 && clickedIdx <= 7) {
      if (onQuickTileClicked) {
        if (clickedIdx == 5)
          onQuickTileClicked(0); // EQ
        else if (clickedIdx == 6)
          onQuickTileClicked(1); // Comp
        else if (clickedIdx == 7)
          onQuickTileClicked(3); // Routing
      }
    }
  }

private:
  bool curMasterAssigned{true};
  bool curMonAssigned{true};
  bool curMonPre{false};
  bool curCompEnabled{false};
  bool curGateEnabled{false};
  float curMonSendDb{0.0f};
  float curCompRatio{4.0f};
  float curCompGrDb{0.0f};
  MidasKnob preampKnob, driveKnob, hpfKnob, delayKnob, panKnob;
  MidasTactileButton btn48V, btnPhase, btnHpf, btnDelay, btnPanLaw,
      btnSurround3D;
};

// ==============================================================================
// DEDICATED HARDWARE DSP PHYSICAL PANEL (UPPER RIGHT SECTION)
// ==============================================================================
class MidasDedicatedDspPanel : public juce::Component {
public:
  enum class FocusMode { Eq, Comp, Gate, All };
  MidasDedicatedDspPanel()
      : btnUsbRec("USB READY", MidasTactileButton::GlowMode::Green),
        btnMacroA("A", MidasTactileButton::GlowMode::Cyan),
        btnMacroB("B", MidasTactileButton::GlowMode::Cyan),
        btnMacroC("C", MidasTactileButton::GlowMode::Cyan),
        btnMacroD("D", MidasTactileButton::GlowMode::Cyan),
        btnMacroE("E", MidasTactileButton::GlowMode::Cyan),
        btnMacroF("F", MidasTactileButton::GlowMode::Cyan),
        btnSipSafe("SIP SAFE", MidasTactileButton::GlowMode::Red),
        btnSoloMode("MODE: AFL", MidasTactileButton::GlowMode::Gold),
        btnHwEq("HARDWARE 4-PEQ", MidasTactileButton::GlowMode::Cyan),
        btnHwComp("HARDWARE DYN", MidasTactileButton::GlowMode::Gold),
        btnHwGate("HARDWARE GATE", MidasTactileButton::GlowMode::Green),
        lfKnob("LF", -18.0f, 18.0f, 2.5f, "dB", MidasKnob::NotchColor::Red),
        lmfKnob("LMF", -18.0f, 18.0f, -4.0f, "dB", MidasKnob::NotchColor::Gold),
        hmfKnob("HMF", -18.0f, 18.0f, 3.0f, "dB", MidasKnob::NotchColor::Green),
        hfKnob("HF", -18.0f, 18.0f, 1.0f, "dB", MidasKnob::NotchColor::Cyan),
        dynThrKnob("THRESH", -48.0f, 0.0f, -18.0f, "dB",
                   MidasKnob::NotchColor::Gold),
        dynRatioKnob("RATIO", 1.0f, 20.0f, 4.0f, ":1",
                     MidasKnob::NotchColor::Gold),
        macroFx1("FX1", 0.0f, 100.0f, 65.0f, "%",
                 MidasKnob::NotchColor::Purple),
        macroSub("SUB", 0.0f, 100.0f, 50.0f, "%",
                 MidasKnob::NotchColor::Purple),
        macroAir("AIR", 0.0f, 100.0f, 30.0f, "%",
                 MidasKnob::NotchColor::Purple),
        macroTrim("TRIM", -12.0f, 12.0f, 0.0f, "dB",
                  MidasKnob::NotchColor::Purple),
        monStudioKnob("MON A", -60.0f, 10.0f, -6.0f, "dB",
                      MidasKnob::NotchColor::Cyan),
        monPhonesKnob("PHONES", -60.0f, 10.0f, -3.0f, "dB",
                      MidasKnob::NotchColor::Cyan),
        tbLvlKnob("TB LVL", -60.0f, 10.0f, 0.0f, "dB",
                  MidasKnob::NotchColor::Gold),
        mainPanKnob("MAIN PAN", -50.0f, 50.0f, 0.0f, "",
                    MidasKnob::NotchColor::Gold),
        mainDelayKnob("BUS DLY", 0.0f, 200.0f, 0.0f, "ms",
                      MidasKnob::NotchColor::Cyan),
        mainWidthKnob("ST WIDTH", 0.0f, 200.0f, 100.0f, "%",
                      MidasKnob::NotchColor::Cyan),
        btnTalkback("TALKBACK", MidasTactileButton::GlowMode::Gold),
        btnShoutIem("SHOUT IEM", MidasTactileButton::GlowMode::Purple) {

    addAndMakeVisible(btnUsbRec);
    addAndMakeVisible(btnMacroA);
    addAndMakeVisible(btnMacroB);
    addAndMakeVisible(btnMacroC);
    addAndMakeVisible(btnMacroD);
    addAndMakeVisible(btnMacroE);
    addAndMakeVisible(btnMacroF);
    addAndMakeVisible(btnSipSafe);
    addAndMakeVisible(btnSoloMode);

    addAndMakeVisible(btnHwEq);
    addAndMakeVisible(btnHwComp);
    addAndMakeVisible(btnHwGate);
    btnHwEq.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(lfKnob);
    addAndMakeVisible(lmfKnob);
    addAndMakeVisible(hmfKnob);
    addAndMakeVisible(hfKnob);

    addAndMakeVisible(dynThrKnob);
    addAndMakeVisible(dynRatioKnob);

    addAndMakeVisible(macroFx1);
    addAndMakeVisible(macroSub);
    addAndMakeVisible(macroAir);
    addAndMakeVisible(macroTrim);

    addAndMakeVisible(monStudioKnob);
    addAndMakeVisible(monPhonesKnob);
    addAndMakeVisible(tbLvlKnob);

    addAndMakeVisible(mainPanKnob);
    addAndMakeVisible(mainDelayKnob);
    addAndMakeVisible(mainWidthKnob);

    addAndMakeVisible(btnTalkback);
    addAndMakeVisible(btnShoutIem);
  }

  std::function<void(int band, float gain)> onEqGainChanged;
  std::function<void(float thr, float ratio)> onDynChanged;
  std::function<void(float studio, float phones)> onMonitorChanged;
  std::function<void(bool tbActive)> onTalkbackToggled;
  std::function<void(bool shoutActive)> onShoutIemToggled;
  std::function<void(bool sipSafe)> onSipSafeToggled;
  std::function<void(bool isPfl)> onSoloModeToggled;
  std::function<void(int macroIdx, float val)> onMacroKnobChanged;
  std::function<void(int tabIndex)> onHwModuleSelected;
  std::function<void(bool rec)> onUsbRecToggled;

  void setBusMeters(float lDb, float rDb, float cDb, float monDb) {
    busMeterDb[0] = lDb;
    busMeterDb[1] = rDb;
    busMeterDb[2] = cDb;
    busMeterDb[3] = monDb;
    repaint();
  }

  void setUsbRecState(bool recording, const juce::String &text) {
    btnUsbRec.setToggleState(recording, juce::dontSendNotification);
    btnUsbRec.setGlowMode(recording ? MidasTactileButton::GlowMode::Red
                                    : MidasTactileButton::GlowMode::Green);
    btnUsbRec.setButtonText(text);
    repaint();
  }

  void setEqInState(bool enabled) {
    btnHwEq.setToggleState(enabled, juce::dontSendNotification);
    repaint();
  }

  void setFocusMode(FocusMode mode) {
    currentFocus = mode;
    btnHwEq.setToggleState(mode == FocusMode::Eq, juce::dontSendNotification);
    btnHwComp.setToggleState(mode == FocusMode::Comp,
                             juce::dontSendNotification);
    btnHwGate.setToggleState(mode == FocusMode::Gate,
                             juce::dontSendNotification);
    repaint();
  }

  FocusMode getFocusMode() const noexcept { return currentFocus; }

  void selectHardwareModule(int modIdx) {
    if (modIdx == 0)
      setFocusMode(FocusMode::Eq);
    else if (modIdx == 1)
      setFocusMode(FocusMode::Comp);
    else if (modIdx == 2)
      setFocusMode(FocusMode::Gate);
    if (onHwModuleSelected)
      onHwModuleSelected(modIdx);
  }

  void syncChannel(const ChannelState &state) {
    lfKnob.setValue(state.eqGainDb[0].load(std::memory_order_relaxed),
                    juce::dontSendNotification);
    lmfKnob.setValue(state.eqGainDb[1].load(std::memory_order_relaxed),
                     juce::dontSendNotification);
    hmfKnob.setValue(state.eqGainDb[2].load(std::memory_order_relaxed),
                     juce::dontSendNotification);
    hfKnob.setValue(state.eqGainDb[3].load(std::memory_order_relaxed),
                    juce::dontSendNotification);
    if (currentFocus == FocusMode::Gate) {
      dynThrKnob.setValue(state.gateThresholdDb.load(std::memory_order_relaxed),
                          juce::dontSendNotification);
      dynRatioKnob.setValue(state.gateRatio.load(std::memory_order_relaxed),
                            juce::dontSendNotification);
    } else {
      dynThrKnob.setValue(
          state.compressorThresholdDb.load(std::memory_order_relaxed),
          juce::dontSendNotification);
      dynRatioKnob.setValue(
          state.compressorRatio.load(std::memory_order_relaxed),
          juce::dontSendNotification);
    }
    btnSipSafe.setToggleState(state.soloSafe.load(std::memory_order_relaxed),
                              juce::dontSendNotification);
    const bool isPfl = state.soloPfl.load(std::memory_order_relaxed);
    btnSoloMode.setToggleState(isPfl, juce::dontSendNotification);
    btnSoloMode.setButtonText(isPfl ? "MODE: PFL" : "MODE: AFL");
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Active Focus Highlight Border around Dedicated Hardware Controls
    if (currentFocus == FocusMode::Eq) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.12f));
      g.fillRoundedRectangle(6.0f, 50.0f, (area.getWidth() - 22.0f) * 0.5f,
                             56.0f, 4.0f);
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.6f));
      g.drawRoundedRectangle(6.0f, 50.0f, (area.getWidth() - 22.0f) * 0.5f,
                             56.0f, 4.0f, 1.0f);
    } else if (currentFocus == FocusMode::Comp ||
               currentFocus == FocusMode::Gate) {
      const float dynX = 6.0f + (area.getWidth() - 22.0f) * 0.5f + 4.0f;
      const float dynW = (area.getWidth() - 22.0f) * 0.5f;
      g.setColour(midas_colors::kAccentGold.withAlpha(0.12f));
      g.fillRoundedRectangle(dynX, 50.0f, dynW, 56.0f, 4.0f);
      g.setColour(midas_colors::kAccentGold.withAlpha(0.6f));
      g.drawRoundedRectangle(dynX, 50.0f, dynW, 56.0f, 4.0f, 1.0f);
    }

    // Bottom Master 4-Bus Meters (L, R, C, MON) Box
    const float btmMtrY = area.getHeight() - 28.0f;
    auto btmBox =
        juce::Rectangle<float>(8.0f, btmMtrY, area.getWidth() - 16.0f, 22.0f);
    g.setColour(juce::Colour(0xff060c14));
    g.fillRoundedRectangle(btmBox, 3.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(btmBox, 3.0f, 0.8f);

    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MASTER 4-BUS METERS", 14, static_cast<int>(btmMtrY + 4.0f), 120,
               14, juce::Justification::centredLeft);

    // 4 mini meter bars: L, R, C, MON
    const float startMtrX = area.getWidth() - 170.0f;
    static const char *labels[4] = {"L", "R", "C", "M"};
    for (int i = 0; i < 4; ++i) {
      const float mx = startMtrX + i * 40.0f;
      g.setFont(midas_ui::getFont(7.5f, true));
      g.setColour(midas_colors::kTextMuted);
      g.drawText(labels[i], static_cast<int>(mx),
                 static_cast<int>(btmMtrY + 4.0f), 12, 14,
                 juce::Justification::centred);

      g.setColour(juce::Colour(0xff182436));
      g.fillRect(mx + 12.0f, btmMtrY + 7.0f, 22.0f, 8.0f);
      const float norm = juce::jlimit(
          0.08f, 1.0f, (busMeterDb[static_cast<size_t>(i)] + 60.0f) / 66.0f);
      g.setColour((i < 2) ? midas_colors::kAccentGreen
                          : ((i == 2) ? midas_colors::kAccentGold
                                      : midas_colors::kAccentCyan));
      g.fillRect(mx + 12.0f, btmMtrY + 7.0f, 22.0f * norm, 8.0f);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();

    // Row 1: USB READY + MACROS A-F + SIP SAFE + MODE: AFL
    btnUsbRec.setBounds(8, 6, 75, 20);
    btnUsbRec.onClick = [this] {
      if (onUsbRecToggled)
        onUsbRecToggled(btnUsbRec.getToggleState());
    };
    const float macroW = 18.0f;
    float mx = 88.0f;
    btnMacroA.setBounds(static_cast<int>(mx), 6, static_cast<int>(macroW), 20);
    mx += macroW + 2.0f;
    btnMacroB.setBounds(static_cast<int>(mx), 6, static_cast<int>(macroW), 20);
    mx += macroW + 2.0f;
    btnMacroC.setBounds(static_cast<int>(mx), 6, static_cast<int>(macroW), 20);
    mx += macroW + 2.0f;
    btnMacroD.setBounds(static_cast<int>(mx), 6, static_cast<int>(macroW), 20);
    mx += macroW + 2.0f;
    btnMacroE.setBounds(static_cast<int>(mx), 6, static_cast<int>(macroW), 20);
    mx += macroW + 2.0f;
    btnMacroF.setBounds(static_cast<int>(mx), 6, static_cast<int>(macroW), 20);
    mx += macroW + 4.0f;

    btnSipSafe.setBounds(static_cast<int>(w - 146.0f), 6, 68, 20);
    btnSoloMode.setBounds(static_cast<int>(w - 74.0f), 6, 66, 20);

    // Row 2: HW Module Selector Buttons
    const float hwModW = (w - 20.0f) / 3.0f;
    btnHwEq.setBounds(8, 30, static_cast<int>(hwModW), 20);
    btnHwComp.setBounds(static_cast<int>(8.0f + hwModW + 2.0f), 30,
                        static_cast<int>(hwModW), 20);
    btnHwGate.setBounds(static_cast<int>(8.0f + (hwModW + 2.0f) * 2.0f), 30,
                        static_cast<int>(hwModW), 20);

    btnHwEq.onClick = [this] { selectHardwareModule(0); };
    btnHwComp.onClick = [this] { selectHardwareModule(1); };
    btnHwGate.onClick = [this] { selectHardwareModule(2); };

    // Row 3: 4-Band EQ Gain & Dynamics Knobs
    const float halfW = (w - 22.0f) * 0.5f;
    const float eqKnobW = (halfW - 6.0f) * 0.25f;
    lfKnob.setBounds(8, 54, static_cast<int>(eqKnobW), 48);
    lmfKnob.setBounds(static_cast<int>(8.0f + eqKnobW + 2.0f), 54,
                      static_cast<int>(eqKnobW), 48);
    hmfKnob.setBounds(static_cast<int>(8.0f + (eqKnobW + 2.0f) * 2.0f), 54,
                      static_cast<int>(eqKnobW), 48);
    hfKnob.setBounds(static_cast<int>(8.0f + (eqKnobW + 2.0f) * 3.0f), 54,
                     static_cast<int>(eqKnobW), 48);

    const float dynX = 8.0f + halfW + 6.0f;
    const float dynKnobW = (halfW - 6.0f) * 0.5f;
    dynThrKnob.setBounds(static_cast<int>(dynX), 54, static_cast<int>(dynKnobW),
                         48);
    dynRatioKnob.setBounds(static_cast<int>(dynX + dynKnobW + 4.0f), 54,
                           static_cast<int>(dynKnobW), 48);

    // Row 4: Assignable Macros (FX1, SUB, AIR, TRIM) & Studio Monitor / Phones
    macroFx1.setBounds(8, 106, static_cast<int>(eqKnobW), 48);
    macroSub.setBounds(static_cast<int>(8.0f + eqKnobW + 2.0f), 106,
                       static_cast<int>(eqKnobW), 48);
    macroAir.setBounds(static_cast<int>(8.0f + (eqKnobW + 2.0f) * 2.0f), 106,
                       static_cast<int>(eqKnobW), 48);
    macroTrim.setBounds(static_cast<int>(8.0f + (eqKnobW + 2.0f) * 3.0f), 106,
                        static_cast<int>(eqKnobW), 48);

    const float monKnobW = (halfW - 6.0f) / 3.0f;
    monStudioKnob.setBounds(static_cast<int>(dynX), 106,
                            static_cast<int>(monKnobW), 48);
    monPhonesKnob.setBounds(static_cast<int>(dynX + monKnobW + 2.0f), 106,
                            static_cast<int>(monKnobW), 48);
    tbLvlKnob.setBounds(static_cast<int>(dynX + (monKnobW + 2.0f) * 2.0f), 106,
                        static_cast<int>(monKnobW), 48);

    // Row 5: Main Stereo Bus Hardware (PAN, DLY, WIDTH) & Talkback / Shout
    const float panW = (halfW - 6.0f) / 3.0f;
    mainPanKnob.setBounds(8, 158, static_cast<int>(panW), 48);
    mainDelayKnob.setBounds(static_cast<int>(8.0f + panW + 2.0f), 158,
                            static_cast<int>(panW), 48);
    mainWidthKnob.setBounds(static_cast<int>(8.0f + (panW + 2.0f) * 2.0f), 158,
                            static_cast<int>(panW), 48);

    btnTalkback.setBounds(static_cast<int>(dynX), 164,
                          static_cast<int>((halfW - 6.0f) * 0.5f), 24);
    btnShoutIem.setBounds(static_cast<int>(dynX + (halfW - 6.0f) * 0.5f + 4.0f),
                          164, static_cast<int>((halfW - 6.0f) * 0.5f), 24);

    lfKnob.onValueChanged = [this](float v) {
      if (onEqGainChanged)
        onEqGainChanged(0, v);
    };
    lmfKnob.onValueChanged = [this](float v) {
      if (onEqGainChanged)
        onEqGainChanged(1, v);
    };
    hmfKnob.onValueChanged = [this](float v) {
      if (onEqGainChanged)
        onEqGainChanged(2, v);
    };
    hfKnob.onValueChanged = [this](float v) {
      if (onEqGainChanged)
        onEqGainChanged(3, v);
    };

    dynThrKnob.onValueChanged = [this](float) {
      if (onDynChanged)
        onDynChanged(dynThrKnob.getValue(), dynRatioKnob.getValue());
    };
    dynRatioKnob.onValueChanged = [this](float) {
      if (onDynChanged)
        onDynChanged(dynThrKnob.getValue(), dynRatioKnob.getValue());
    };
    monStudioKnob.onValueChanged = [this](float) {
      if (onMonitorChanged)
        onMonitorChanged(monStudioKnob.getValue(), monPhonesKnob.getValue());
    };
    monPhonesKnob.onValueChanged = [this](float) {
      if (onMonitorChanged)
        onMonitorChanged(monStudioKnob.getValue(), monPhonesKnob.getValue());
    };
    btnTalkback.onClick = [this] {
      if (onTalkbackToggled)
        onTalkbackToggled(btnTalkback.getToggleState());
    };
    btnShoutIem.onClick = [this] {
      if (onShoutIemToggled)
        onShoutIemToggled(btnShoutIem.getToggleState());
    };
    btnSipSafe.onClick = [this] {
      if (onSipSafeToggled)
        onSipSafeToggled(btnSipSafe.getToggleState());
    };
    btnSoloMode.onClick = [this] {
      const bool isPfl = btnSoloMode.getToggleState();
      btnSoloMode.setButtonText(isPfl ? "MODE: PFL" : "MODE: AFL");
      if (onSoloModeToggled)
        onSoloModeToggled(isPfl);
    };
    macroFx1.onValueChanged = [this](float v) {
      if (onMacroKnobChanged)
        onMacroKnobChanged(0, v);
    };
    macroSub.onValueChanged = [this](float v) {
      if (onMacroKnobChanged)
        onMacroKnobChanged(1, v);
    };
    macroAir.onValueChanged = [this](float v) {
      if (onMacroKnobChanged)
        onMacroKnobChanged(2, v);
    };
    macroTrim.onValueChanged = [this](float v) {
      if (onMacroKnobChanged)
        onMacroKnobChanged(3, v);
    };
  }

private:
  std::array<float, 4> busMeterDb = {-18.0f, -18.0f, -24.0f, -20.0f};
  FocusMode currentFocus = FocusMode::Eq;
  MidasTactileButton btnUsbRec;
  MidasTactileButton btnMacroA, btnMacroB, btnMacroC, btnMacroD, btnMacroE,
      btnMacroF;
  MidasTactileButton btnSipSafe, btnSoloMode;
  MidasTactileButton btnHwEq, btnHwComp, btnHwGate;
  MidasKnob lfKnob, lmfKnob, hmfKnob, hfKnob;
  MidasKnob dynThrKnob, dynRatioKnob;
  MidasKnob macroFx1, macroSub, macroAir, macroTrim;
  MidasKnob monStudioKnob, monPhonesKnob, tbLvlKnob;
  MidasKnob mainPanKnob, mainDelayKnob, mainWidthKnob;
  MidasTactileButton btnTalkback, btnShoutIem;
};

// ==============================================================================
// ==============================================================================
// MOTORIZED CHANNEL FADER STRIP
// ==============================================================================
class MidasConsoleStrip : public juce::Component {
public:
  MidasConsoleStrip(int slotIndex)
      : slot(slotIndex), panKnob(false),
        btnSelect("SEL", MidasTactileButton::GlowMode::Cyan),
        btnMute("MUTE", MidasTactileButton::GlowMode::Red),
        btnSolo("SOLO", MidasTactileButton::GlowMode::Gold) {

    addAndMakeVisible(panKnob);
    addAndMakeVisible(btnSelect);
    addAndMakeVisible(btnMute);
    addAndMakeVisible(btnSolo);
    addAndMakeVisible(fader);
    addAndMakeVisible(meter);
  }

  std::function<void(int slot)> onSelected;
  std::function<void(int slot, float db)> onFaderChanged;
  std::function<void(int slot, bool muted)> onMuteChanged;
  std::function<void(int slot, bool solo)> onSoloChanged;
  std::function<void(int slot, float panVal)> onPanChanged;
  std::function<void(int slot, float trimDb)> onTrimChanged;

  void setChannelInfo(const juce::String &tag, const juce::String &name,
                      const juce::String &badge, juce::Colour color) {
    chTag = tag;
    chName = name;
    chBadge = badge;
    accentColor = color;
    fader.setFaderCapColor(color);
    setName(name);
    repaint();
  }

  bool isFaderDragging() const noexcept { return fader.isDraggingFader(); }

  void syncState(const ChannelState &state) {
    if (!fader.isDraggingFader()) {
      if (isSofMonitorMode) {
        fader.setGainDb(state.monitorSendDb.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
        fader.setFaderCapColor(midas_colors::kAccentCyan);
      } else {
        const float fDb = state.gainDb.load(std::memory_order_relaxed);
        fader.setGainDb(fDb, juce::dontSendNotification);
        fader.setFaderCapColor(accentColor);
      }
    }
    btnMute.setToggleState(state.muted.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    btnSolo.setToggleState(state.solo.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    panKnob.setValue(state.pan.load(std::memory_order_relaxed),
                     juce::dontSendNotification);

    isPreFader = state.monitorPreFader.load(std::memory_order_relaxed);

    const float pk = state.peak.load(std::memory_order_relaxed);
    const float pkDb =
        (pk > 1.0e-5f) ? juce::Decibels::gainToDecibels(pk, -60.0f) : -60.0f;
    meter.setLevels(pkDb);
  }

  void syncBusOrVcaState(float faderDb, bool muted, float linearPeak) {
    if (!fader.isDraggingFader()) {
      fader.setGainDb(faderDb, juce::dontSendNotification);
    }
    btnMute.setToggleState(muted, juce::dontSendNotification);
    const float pkDb = (linearPeak > 1.0e-5f)
                           ? juce::Decibels::gainToDecibels(linearPeak, -60.0f)
                           : -60.0f;
    meter.setLevels(pkDb);
    repaint();
  }

  float getCurrentFaderDb() const { return fader.getGainDb(); }

  void setSelected(bool sel) {
    isSelected = sel;
    btnSelect.setToggleState(sel, juce::dontSendNotification);
    repaint();
  }

  void setSofMonitorMode(bool sofActive, bool preFader = false) {
    isSofMonitorMode = sofActive;
    isPreFader = preFader;
    fader.setFaderCapColor(sofActive ? midas_colors::kAccentCyan : accentColor);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Strip Card Background & Outer Selection Border
    auto cardRect = area.reduced(2.0f);
    g.setColour(midas_colors::kStripBg);
    g.fillRoundedRectangle(cardRect, 8.0f);

    if (isSelected) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.35f));
      g.drawRoundedRectangle(cardRect.expanded(1.0f), 8.5f, 2.5f);
      g.setColour(midas_colors::kAccentCyan);
      g.drawRoundedRectangle(cardRect, 8.0f, 2.0f);
    } else if (isSofMonitorMode) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.6f));
      g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);
    } else {
      g.setColour(midas_colors::kBorderCol.withAlpha(0.7f));
      g.drawRoundedRectangle(cardRect, 8.0f, 1.2f);
    }

    // 2. Pan Readout Text (directly beneath Pan Knob)
    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(juce::Colour(0xff7fa0c0));
    juce::String panStr = "C";
    const float p = panKnob.getValue();
    if (p < -0.01f) {
      panStr = "L" +
               juce::String(static_cast<int>(std::round(std::abs(p) * 100.0f)));
    } else if (p > 0.01f) {
      panStr = "R" + juce::String(static_cast<int>(std::round(p * 100.0f)));
    }
    g.drawText(panStr, 0, 36, static_cast<int>(w), 14,
               juce::Justification::centred);

    // 3. Digital LCD Scribble Strip Screen
    const float oledY = 82.0f;
    const float oledH = 58.0f;
    auto oledRect = juce::Rectangle<float>(6.0f, oledY, w - 12.0f, oledH);

    g.setColour(juce::Colour(0xff03060a));
    g.fillRoundedRectangle(oledRect, 4.0f);
    g.setColour(accentColor);
    g.drawRoundedRectangle(oledRect, 4.0f, 1.2f);

    // Top Category Color Accent Bar
    g.fillRect(oledRect.getX() + 2.0f, oledRect.getY() + 2.0f,
               oledRect.getWidth() - 4.0f, 2.5f);

    const auto stripAccent =
        isSofMonitorMode ? midas_colors::kAccentCyan : accentColor;
    // Line 1: Category Badge (e.g. "DRUMS 1", or "MON PRE" / "MON POST" during
    // SOF)
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(stripAccent);
    const juce::String displayTag =
        isSofMonitorMode ? (isPreFader ? "MON PRE" : "MON POST") : chTag;
    g.drawText(displayTag, static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 6.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);

    // Line 2: Channel Name (e.g. "KICK IN", "SNARE TOP", "BASS DI")
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(juce::Colours::white);
    g.drawText(chName, static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 18.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 14,
               juce::Justification::centred);

    // Line 3: DSP Subtitle (e.g. "VCA COMP | MIDAS ...", or "AUX MON SEND"
    // during SOF)
    g.setFont(midas_ui::getFont(7.5f, false));
    g.setColour(isSofMonitorMode ? midas_colors::kAccentCyan
                                 : midas_colors::kAccentGold);
    const juce::String displayBadge =
        isSofMonitorMode ? "AUX MON SEND" : chBadge;
    g.drawText(displayBadge, static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 32.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);

    // Line 4: Fader dB Readout (e.g. "+1.2 dB", "+0.0 dB", "-4.2 dB")
    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    const float curDb = fader.getGainDb();
    juce::String dbStr;
    if (curDb <= -89.0f)
      dbStr = "-inf dB";
    else if (curDb >= 0.05f)
      dbStr = "+" + juce::String(curDb, 1) + " dB";
    else
      dbStr = juce::String(curDb, 1) + " dB";

    g.drawText(dbStr, static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 43.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 13,
               juce::Justification::centred);

    // 4. Bottom Channel Identifier (e.g. "CH 01" .. "CH 08")
    const float badgeY = h - 25.0f;
    const float badgeW = juce::jmin(w - 20.0f, 54.0f);
    const float badgeX = (w - badgeW) * 0.5f;
    auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeW, 18.0f);

    g.setColour(juce::Colour(0xff060a12));
    g.fillRoundedRectangle(badgeRect, 4.0f);
    g.setColour(juce::Colour(0xff182436));
    g.drawRoundedRectangle(badgeRect, 4.0f, 1.0f);

    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(juce::Colour(0xff7a8fa6));
    juce::String bottomText = "CH " + (slot < 9 ? ("0" + juce::String(slot + 1))
                                                : juce::String(slot + 1));
    g.drawText(bottomText, badgeRect, juce::Justification::centred);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Pan Knob
    const float knobSize = 28.0f;
    panKnob.setBounds(static_cast<int>((w - knobSize) * 0.5f), 6,
                      static_cast<int>(knobSize), static_cast<int>(knobSize));

    // 2. SEL Button
    const float selW = w - 16.0f;
    btnSelect.setBounds(static_cast<int>((w - selW) * 0.5f), 54,
                        static_cast<int>(selW), 24);

    // 3. MUTE & SOLO Buttons (Side-by-side)
    const float btnGap = 4.0f;
    const float btnW = (w - 16.0f - btnGap) * 0.5f;
    const float btnY = 144.0f;
    btnMute.setBounds(8, static_cast<int>(btnY), static_cast<int>(btnW), 24);
    btnSolo.setBounds(static_cast<int>(8.0f + btnW + btnGap),
                      static_cast<int>(btnY), static_cast<int>(btnW), 24);

    // 4. Fader & Meter (Meter on the LEFT, Fader on the RIGHT)
    const float faderTop = 174.0f;
    const float faderBottom = h - 30.0f;
    const float faderH = faderBottom - faderTop;

    const float meterW = 7.0f;
    const float wellW = juce::jlimit(30.0f, 40.0f, w - meterW - 26.0f);
    const float totalFaderAreaW = meterW + 8.0f + wellW;
    const float startX = (w - totalFaderAreaW) * 0.5f;

    meter.setBounds(static_cast<int>(startX), static_cast<int>(faderTop + 4.0f),
                    static_cast<int>(meterW), static_cast<int>(faderH - 8.0f));

    fader.setBounds(static_cast<int>(startX + meterW + 8.0f),
                    static_cast<int>(faderTop), static_cast<int>(wellW),
                    static_cast<int>(faderH));

    btnSelect.onClick = [this] {
      if (onSelected)
        onSelected(slot);
    };
    btnMute.onClick = [this] {
      if (onMuteChanged)
        onMuteChanged(slot, btnMute.getToggleState());
    };
    btnSolo.onClick = [this] {
      if (onSoloChanged)
        onSoloChanged(slot, btnSolo.getToggleState());
    };
    fader.onFaderMoved = [this](float db) {
      repaint();
      if (onFaderChanged)
        onFaderChanged(slot, db);
    };
    panKnob.onValueChanged = [this](float val) {
      repaint();
      if (onPanChanged)
        onPanChanged(slot, val);
    };
  }

private:
  int slot = 0;
  bool isSelected = false;
  bool isSofMonitorMode = false;
  bool isPreFader = false;
  juce::String chTag = "IN 01";
  juce::String chName = "KICK IN";
  juce::String chBadge = "CH 01";
  juce::Colour accentColor = midas_colors::kAccentRed;

  MidasPanKnob panKnob;
  MidasTactileButton btnSelect, btnMute, btnSolo;
  MidasFader fader;
  MidasMeterLadder meter;
};

// ==============================================================================
// MASTER STEREO BUS STRIP (MASTER L/R SECTION)
// ==============================================================================
class MidasMasterStrip : public juce::Component {
public:
  MidasMasterStrip()
      : balanceKnob(true), btnDim("DIM", MidasTactileButton::GlowMode::Gold),
        btnMono("MONO", MidasTactileButton::GlowMode::Gray),
        btnClearSolo("CLR SOLO", MidasTactileButton::GlowMode::Gold),
        btnMuteMain("MUTE", MidasTactileButton::GlowMode::Red),
        btnMainLr("MAIN L/R", MidasTactileButton::GlowMode::Gold) {

    addAndMakeVisible(balanceKnob);
    addAndMakeVisible(btnClearSolo);
    addAndMakeVisible(btnDim);
    addAndMakeVisible(btnMuteMain);
    addAndMakeVisible(masterFader);
    addAndMakeVisible(masterMeter);
    addAndMakeVisible(btnMainLr);

    masterFader.setIsMaster(true);
    masterFader.setFaderCapColor(midas_colors::kAccentGold);
    btnClearSolo.setClickingTogglesState(false);
  }

  std::function<void(float db)> onMasterFaderMoved;
  std::function<void(bool muted)> onMasterMuteChanged;
  std::function<void(bool dim)> onDimToggled;
  std::function<void(bool mono)> onMonoToggled;
  std::function<void()> onClearSoloClicked;
  std::function<void(float bal)> onBalanceChanged;
  std::function<void(bool monitorMode)> onModeToggled;

  void setLevels(float dbL, float dbR) { masterMeter.setLevels(dbL, dbR); }

  void setFaderDb(float db) {
    if (!masterFader.isDraggingFader()) {
      masterFader.setGainDb(db, juce::dontSendNotification);
      repaint();
    }
  }

  float getFaderDb() const { return masterFader.getGainDb(); }

  void setMuted(bool m) {
    btnMuteMain.setToggleState(m, juce::dontSendNotification);
  }

  void setDim(bool d) { btnDim.setToggleState(d, juce::dontSendNotification); }

  void setMonitorMode(bool monMode) {
    isMonitorMode = monMode;
    masterFader.setFaderCapColor(monMode ? midas_colors::kAccentCyan
                                         : midas_colors::kAccentGold);
    btnMainLr.setButtonText(monMode ? "MONITOR A" : "MAIN L/R");
    btnMainLr.setGlowMode(monMode ? MidasTactileButton::GlowMode::Cyan
                                  : MidasTactileButton::GlowMode::Gold);
    btnDim.setGlowMode(monMode ? MidasTactileButton::GlowMode::Cyan
                               : MidasTactileButton::GlowMode::Gold);
    repaint();
  }

  bool getIsMonitorMode() const noexcept { return isMonitorMode; }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Card Background & Accent Border (Gold for Master, Cyan for Monitor)
    auto cardRect = area.reduced(2.0f);
    g.setColour(midas_colors::kStripBg);
    g.fillRoundedRectangle(cardRect, 8.0f);

    const auto accentCol =
        isMonitorMode ? midas_colors::kAccentCyan : midas_colors::kAccentGold;
    g.setColour(accentCol);
    g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);

    // Realistic Touring Chassis Screws
    midas_ui::drawConsoleScrew(g, w - 7.0f, 8.0f, 3.2f);
    midas_ui::drawConsoleScrew(g, w - 7.0f, h - 8.0f, 3.2f);

    // 2. Header Icon + Title
    if (isMonitorMode) {
      const float hpX = (w - 92.0f) * 0.5f;
      const float hpY = 5.0f;
      g.setColour(midas_colors::kAccentCyan);
      juce::Path hp;
      hp.addCentredArc(hpX + 7.0f, hpY + 7.0f, 6.0f, 6.0f, 0.0f,
                       -juce::MathConstants<float>::halfPi - 0.6f,
                       juce::MathConstants<float>::halfPi + 0.6f, true);
      g.strokePath(hp, juce::PathStrokeType(1.8f));
      g.fillRoundedRectangle(hpX + 1.0f, hpY + 5.5f, 3.0f, 6.5f, 1.2f);
      g.fillRoundedRectangle(hpX + 10.0f, hpY + 5.5f, 3.0f, 6.5f, 1.2f);

      g.setFont(midas_ui::getFont(9.5f, true));
      g.setColour(midas_colors::kAccentCyan);
      g.drawText("MONITOR L/R", static_cast<int>(hpX + 18.0f), 3, 76, 14,
                 juce::Justification::centredLeft);
    } else {
      const float crownX = (w - 86.0f) * 0.5f;
      const float crownY = 6.0f;
      juce::Path crown;
      crown.startNewSubPath(crownX, crownY + 8.0f);
      crown.lineTo(crownX + 2.0f, crownY + 2.0f);
      crown.lineTo(crownX + 5.0f, crownY + 6.0f);
      crown.lineTo(crownX + 8.0f, crownY);
      crown.lineTo(crownX + 11.0f, crownY + 6.0f);
      crown.lineTo(crownX + 14.0f, crownY + 2.0f);
      crown.lineTo(crownX + 16.0f, crownY + 8.0f);
      crown.closeSubPath();
      g.setColour(midas_colors::kAccentGold);
      g.fillPath(crown);

      g.setFont(midas_ui::getFont(9.5f, true));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("MASTER L/R", static_cast<int>(crownX + 18.0f), 3, 72, 14,
                 juce::Justification::centredLeft);
    }

    // 3. Balance Readout Text
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(juce::Colour(0xff7fa0c0));
    g.drawText(isMonitorMode ? "MON BAL" : "BALANCE", 0, 48,
               static_cast<int>(w), 12, juce::Justification::centred);

    // 4. Master Scribble Strip Screen
    const float oledY = 64.0f;
    const float oledH = 54.0f;
    auto oledRect = juce::Rectangle<float>(6.0f, oledY, w - 12.0f, oledH);

    g.setColour(juce::Colour(0xff03060a));
    g.fillRoundedRectangle(oledRect, 4.0f);
    g.setColour(accentCol);
    g.drawRoundedRectangle(oledRect, 4.0f, 1.2f);
    g.fillRect(oledRect.getX() + 2.0f, oledRect.getY() + 2.0f,
               oledRect.getWidth() - 4.0f, 2.5f);

    // Line 1:
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(accentCol.withAlpha(0.85f));
    g.drawText(isMonitorMode ? "AUX MONITOR" : "STEREO BUS",
               static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 6.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);

    // Line 2:
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(accentCol);
    g.drawText(isMonitorMode ? "MONITOR OUT" : "MAIN OUT",
               static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 19.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 14,
               juce::Justification::centred);

    // Line 3: Fader dB Readout
    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    const float curDb = masterFader.getGainDb();
    juce::String dbStr;
    if (curDb <= -89.0f)
      dbStr = "-inf dB";
    else if (curDb >= 0.05f)
      dbStr = "+" + juce::String(curDb, 1) + " dB";
    else
      dbStr = juce::String(curDb, 1) + " dB";
    g.drawText(dbStr, static_cast<int>(oledRect.getX() + 4.0f),
               static_cast<int>(oledY + 35.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 13,
               juce::Justification::centred);

    // 5. Calibrated dB Scale Labels (Right of Master Fader)
    const float faderTop = 152.0f;
    const float faderBottom = h - 34.0f;
    const float trackH = (faderBottom - faderTop) - 16.0f;
    const float faderRight = masterFader.getBounds().toFloat().getRight();

    struct ScaleMark {
      float db;
      const char *label;
      juce::Colour col;
    };
    static const ScaleMark marks[] = {
        {10.0f, "+10", midas_colors::kAccentRed},
        {0.0f, "0", midas_colors::kAccentGold},
        {-5.0f, "-5", juce::Colour(0xff7a8fa6)},
        {-10.0f, "-10", juce::Colour(0xff7a8fa6)},
        {-20.0f, "-20", juce::Colour(0xff7a8fa6)},
        {-30.0f, "-30", juce::Colour(0xff7a8fa6)},
        {-90.0f, "-â", juce::Colour(0xff7a8fa6)}};

    g.setFont(midas_ui::getFont(7.5f, true));
    for (const auto &m : marks) {
      const float norm =
          (m.db <= -90.0f)
              ? 0.0f
              : (m.db >= 10.0f
                     ? 1.0f
                     : (m.db >= 0.0f
                            ? 0.75f + (m.db / 10.0f) * 0.25f
                            : std::pow((m.db + 90.0f) / 90.0f, 2.2f) * 0.75f));
      const float y = (faderBottom - 8.0f) - norm * trackH;
      g.setColour(m.col);
      g.drawText(m.label, static_cast<int>(faderRight + 3.0f),
                 static_cast<int>(y - 6.0f), 24, 12,
                 juce::Justification::centredLeft);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Balance Knob
    const float knobSize = 28.0f;
    balanceKnob.setBounds(static_cast<int>((w - knobSize) * 0.5f), 18,
                          static_cast<int>(knobSize),
                          static_cast<int>(knobSize));

    // 2. CLR SOLO + MUTE & DIM Buttons
    const float btnGap = 4.0f;
    const float btnW = (w - 16.0f - btnGap) * 0.5f;
    btnClearSolo.setBounds(8, 92, static_cast<int>(w - 16.0f), 22);
    const float btnY = 120.0f;
    btnMuteMain.setBounds(8, static_cast<int>(btnY), static_cast<int>(btnW),
                          24);
    btnDim.setBounds(static_cast<int>(8.0f + btnW + btnGap),
                     static_cast<int>(btnY), static_cast<int>(btnW), 24);

    // 3. Master Dual Stereo Meter + Fader + Scale
    const float faderTop = 152.0f;
    const float faderBottom = h - 34.0f;
    const float faderH = faderBottom - faderTop;

    const float dualMeterW = 14.0f;
    const float wellW = 38.0f;
    const float meterX = 8.0f;
    const float faderX = meterX + dualMeterW + 6.0f;

    masterMeter.setBounds(
        static_cast<int>(meterX), static_cast<int>(faderTop + 4.0f),
        static_cast<int>(dualMeterW), static_cast<int>(faderH - 8.0f));

    masterFader.setBounds(static_cast<int>(faderX), static_cast<int>(faderTop),
                          static_cast<int>(wellW), static_cast<int>(faderH));

    // 4. MAIN L/R Bottom Button
    btnMainLr.setBounds(8, static_cast<int>(h - 26.0f),
                        static_cast<int>(w - 16.0f), 22);

    balanceKnob.onValueChanged = [this](float bal) {
      if (onBalanceChanged)
        onBalanceChanged(bal);
    };
    masterFader.onFaderMoved = [this](float db) {
      repaint();
      if (onMasterFaderMoved)
        onMasterFaderMoved(db);
    };
    btnMuteMain.onClick = [this] {
      if (onMasterMuteChanged)
        onMasterMuteChanged(btnMuteMain.getToggleState());
    };
    btnDim.onClick = [this] {
      if (onDimToggled)
        onDimToggled(btnDim.getToggleState());
    };
    btnMono.onClick = [this] {
      if (onMonoToggled)
        onMonoToggled(btnMono.getToggleState());
    };
    btnClearSolo.onClick = [this] {
      if (onClearSoloClicked)
        onClearSoloClicked();
    };
    btnMainLr.onClick = [this] {
      if (onModeToggled)
        onModeToggled(isMonitorMode);
    };
  }

private:
  bool isMonitorMode{false};
  MidasPanKnob balanceKnob;
  MidasTactileButton btnDim, btnMono, btnClearSolo, btnMuteMain, btnMainLr;
  MidasFader masterFader;
  MidasMeterLadder masterMeter;
};
// ==============================================================================
// MASTER STEREO PROCESSING SUB-PANEL
// ==============================================================================
class MidasMasterProcessingPanel : public juce::Component {
public:
  MidasMasterProcessingPanel()
      : lowEq("LOW EQ", -12.0f, 12.0f, 0.0f, "dB", MidasKnob::NotchColor::Red),
        midEq("MID EQ", -12.0f, 12.0f, 0.0f, "dB", MidasKnob::NotchColor::Gold),
        highEq("HIGH EQ", -12.0f, 12.0f, 0.0f, "dB",
               MidasKnob::NotchColor::Cyan),
        compThr("COMP THR", -30.0f, 0.0f, -12.0f, "dB",
                MidasKnob::NotchColor::Gold),
        compRatio("RATIO", 1.5f, 10.0f, 2.5f, ":1",
                  MidasKnob::NotchColor::Gold),
        stWidth("WIDTH", 0.0f, 200.0f, 100.0f, "%",
                MidasKnob::NotchColor::Cyan),
        outDelay("MST DLY", 0.0f, 500.0f, 0.0f, "ms",
                 MidasKnob::NotchColor::Gold),
        monDelay("MON DLY", 0.0f, 500.0f, 0.0f, "ms",
                 MidasKnob::NotchColor::Cyan),
        btnBtSync("SYNC BT", MidasTactileButton::GlowMode::Cyan),
        btnAudioOut("OUTPUT DEVICE", MidasTactileButton::GlowMode::Gold),
        btnBypass("DSP BYPASS", MidasTactileButton::GlowMode::Red) {

    addAndMakeVisible(lowEq);
    addAndMakeVisible(midEq);
    addAndMakeVisible(highEq);
    addAndMakeVisible(compThr);
    addAndMakeVisible(compRatio);
    addAndMakeVisible(stWidth);
    addAndMakeVisible(outDelay);
    addAndMakeVisible(monDelay);
    addAndMakeVisible(btnBtSync);
    btnBtSync.setClickingTogglesState(false);
    addAndMakeVisible(btnAudioOut);
    btnAudioOut.setClickingTogglesState(false);
    addAndMakeVisible(btnBypass);
  }

  std::function<void(float lowDb, float midDb, float highDb)> onMasterEqChanged;
  std::function<void(float thrDb, float ratio)> onMasterCompChanged;
  std::function<void(bool bypassed)> onMasterBypassToggled;
  std::function<void(float mstDelayMs)> onMasterDelayChanged;
  std::function<void(float monDelayMs)> onMonitorDelayChanged;
  std::function<void()> onOpenAudioSettings;

  void setFifoTelemetry(int depthSamples, float latencyMs, uint32_t overruns,
                        uint32_t underruns) {
    juce::String str = "MON FIFO: " + juce::String(depthSamples) + " smp (" +
                       juce::String(latencyMs, 1) +
                       " ms) | Ovr: " + juce::String(overruns) +
                       " Und: " + juce::String(underruns);
    if (fifoTelemetryStr != str) {
      fifoTelemetryStr = str;
      repaint();
    }
  }

  void updateBtSyncVisual(float mstMs) {
    if (mstMs < 1.0f) {
      btnBtSync.setButtonText("SYNC BT: OFF");
      btnBtSync.setToggleState(false, juce::dontSendNotification);
      btnBtSync.setGlowMode(MidasTactileButton::GlowMode::Gray);
    } else if (std::abs(mstMs - 120.0f) < 2.0f) {
      btnBtSync.setButtonText("SYNC BT: 120ms");
      btnBtSync.setToggleState(true, juce::dontSendNotification);
      btnBtSync.setGlowMode(MidasTactileButton::GlowMode::Cyan);
    } else {
      btnBtSync.setButtonText(
          "SYNC BT: " + juce::String(static_cast<int>(mstMs)) + "ms");
      btnBtSync.setToggleState(true, juce::dontSendNotification);
      btnBtSync.setGlowMode(MidasTactileButton::GlowMode::Gold);
    }
  }

  void setDelays(float mstMs, float monMs) {
    if (std::abs(outDelay.getValue() - mstMs) > 0.05f) {
      outDelay.setValue(mstMs, juce::dontSendNotification);
      updateBtSyncVisual(mstMs);
    }
    if (std::abs(monDelay.getValue() - monMs) > 0.05f) {
      monDelay.setValue(monMs, juce::dontSendNotification);
    }
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kAccentGold.withAlpha(0.6f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MAIN STEREO PROCESSING (3-BAND EQ • MASTER BUS COMP • STEREO "
               "ENHANCER • LIMITER)",
               12, 4, 460, 14, juce::Justification::centredLeft);

    // Live Secondary Monitor FIFO & Drift Telemetry (Priority 3)
    if (fifoTelemetryStr.isNotEmpty()) {
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(juce::Colour(0xff4dd0e1));
      g.drawText(fifoTelemetryStr, static_cast<int>(area.getWidth() - 480.0f),
                 4, 255, 14, juce::Justification::centredRight);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float knobW = 44.0f;
    float kx = 12.0f;

    lowEq.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 6.0f;
    midEq.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 6.0f;
    highEq.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 12.0f;

    compThr.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 6.0f;
    compRatio.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 12.0f;

    stWidth.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 6.0f;
    outDelay.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 6.0f;
    monDelay.setBounds(static_cast<int>(kx), 18, static_cast<int>(knobW), 46);
    kx += knobW + 10.0f;

    btnBtSync.setBounds(static_cast<int>(kx), 24, 76, 24);
    btnAudioOut.setBounds(static_cast<int>(area.getWidth() - 215.0f), 24, 112,
                          24);
    btnBypass.setBounds(static_cast<int>(area.getWidth() - 95.0f), 24, 85, 24);

    btnAudioOut.onClick = [this] {
      if (onOpenAudioSettings)
        onOpenAudioSettings();
    };

    auto notifyEq = [this](float) {
      if (onMasterEqChanged)
        onMasterEqChanged(lowEq.getValue(), midEq.getValue(),
                          highEq.getValue());
    };
    lowEq.onValueChanged = notifyEq;
    midEq.onValueChanged = notifyEq;
    highEq.onValueChanged = notifyEq;

    auto notifyComp = [this](float) {
      if (onMasterCompChanged)
        onMasterCompChanged(compThr.getValue(), compRatio.getValue());
    };
    compThr.onValueChanged = notifyComp;
    compRatio.onValueChanged = notifyComp;

    btnBypass.onClick = [this] {
      if (onMasterBypassToggled)
        onMasterBypassToggled(btnBypass.getToggleState());
    };

    outDelay.onValueChanged = [this](float ms) {
      updateBtSyncVisual(ms);
      if (onMasterDelayChanged)
        onMasterDelayChanged(ms);
    };
    monDelay.onValueChanged = [this](float ms) {
      if (onMonitorDelayChanged)
        onMonitorDelayChanged(ms);
    };
    btnBtSync.onClick = [this] {
      // Priority 1: Strictly toggle between 0ms (default OFF) and 120ms
      // (Bluetooth Latency Alignment)
      const float cur = outDelay.getValue();
      const float nextVal = (cur < 10.0f) ? 120.0f : 0.0f;
      outDelay.setValue(nextVal);
    };
  }

private:
  MidasKnob lowEq, midEq, highEq, compThr, compRatio, stWidth, outDelay,
      monDelay;
  MidasTactileButton btnBtSync, btnAudioOut, btnBypass;
  juce::String fifoTelemetryStr;
};

// ==============================================================================
// TOP STATUS / MIDAS CLOUD TELEMETRY HEADER
// ==============================================================================
class MidasTopStatusBar : public juce::Component {
public:
  MidasTopStatusBar()
      : btnAiModal("MIDAS AI", MidasTactileButton::GlowMode::Green),
        btnSurroundModal("3D SURROUND", MidasTactileButton::GlowMode::Purple),
        btnPatchModal("PATCH MATRIX", MidasTactileButton::GlowMode::Cyan),
        btnMCloudModal("mCLOUD SYNC", MidasTactileButton::GlowMode::Cyan),
        btnDspRack("DSP: MIDAS PRO", MidasTactileButton::GlowMode::Gold),
        btnGangModal("GANG FADER", MidasTactileButton::GlowMode::Cyan),
        btnTextScale("TEXT: 130%", MidasTactileButton::GlowMode::Cyan),
        btnQuickSettings("v QS", MidasTactileButton::GlowMode::Cyan),
        btnSof("SOF: OFF", MidasTactileButton::GlowMode::Purple),
        btnEngine("AUDIO ENGINE: LIVE", MidasTactileButton::GlowMode::Green) {

    // Midas AI, 3D Surround, Patch Matrix, DSP Rack, Gang Fader & mCloud Sync
    // live inside the embedded touchscreen layer; hide from top status bar so
    // there are no duplicate links.
    addChildComponent(btnAiModal);
    addChildComponent(btnSurroundModal);
    addChildComponent(btnPatchModal);
    addChildComponent(btnDspRack);
    btnSurroundModal.setClickingTogglesState(false);
    btnPatchModal.setClickingTogglesState(false);
    btnSurroundModal.onClick = [this] {
      if (onOpenSurroundPan)
        onOpenSurroundPan();
    };
    btnPatchModal.onClick = [this] {
      if (onOpenPatchMatrix)
        onOpenPatchMatrix();
    };
    addChildComponent(btnMCloudModal);
    addChildComponent(btnGangModal);
    addAndMakeVisible(btnTextScale);
    btnTextScale.setClickingTogglesState(false);
    addAndMakeVisible(btnQuickSettings);
    btnQuickSettings.setClickingTogglesState(false);
    btnQuickSettings.onClick = [this] {
      if (onOpenQuickSettings)
        onOpenQuickSettings();
    };
    addAndMakeVisible(btnSof);
    addAndMakeVisible(btnEngine);
    btnEngine.setClickingTogglesState(false);
    btnEngine.onClick = [this] {
      if (onOpenAudioSettings)
        onOpenAudioSettings();
    };

    sceneSelect.addItem("01: FOH Arena Live", 1);
    sceneSelect.addItem("02: Acoustic Unplugged", 2);
    sceneSelect.addItem("03: Drum Solo Punch", 3);
    sceneSelect.addItem("04: Broadcast 4K Stem", 4);
    sceneSelect.addItem("05: All Flat Unity (0dB)", 5);
    sceneSelect.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(sceneSelect);
  }

  std::function<void(bool sofActive)> onSofToggled;
  std::function<void(int mode)> onSofModeChanged;
  int sofMode = 0;
  std::function<void(int sceneId)> onSceneChanged;
  std::function<void()> onOpenAiModal;
  std::function<void()> onOpenSurroundPan;
  std::function<void()> onOpenPatchMatrix;
  std::function<void()> onOpenMCloudModal;
  std::function<void()> onOpenDspRackModal;
  std::function<void()> onOpenGangModal;
  std::function<void()> onOpenAudioSettings;
  std::function<void()> onOpenQuickSettings;

  juce::String getTextScaleLabel() const {
    return btnTextScale.getButtonText();
  }

  void cycleTextScale() {
    if (midas_ui::fontScale < 1.15f) {
      midas_ui::fontScale = 1.30f;
      btnTextScale.setButtonText("TEXT: 130%");
    } else if (midas_ui::fontScale < 1.40f) {
      midas_ui::fontScale = 1.50f;
      btnTextScale.setButtonText("TEXT: 150%");
    } else if (midas_ui::fontScale < 1.60f) {
      midas_ui::fontScale = 1.00f;
      btnTextScale.setButtonText("TEXT: 100%");
    } else {
      midas_ui::fontScale = 1.15f;
      btnTextScale.setButtonText("TEXT: 115%");
    }
    if (auto *top = getTopLevelComponent())
      top->repaint();
    else if (auto *parent = getParentComponent())
      parent->repaint();
    else
      repaint();
  }

  void mouseDown(const juce::MouseEvent &) override {
    pullDownTriggered = false;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (!pullDownTriggered && e.getOffsetFromDragStart().y > 16) {
      pullDownTriggered = true;
      if (onOpenQuickSettings)
        onOpenQuickSettings();
    }
  }

  void setSofActive(bool active, const juce::String &busTag = "") {
    btnSof.setToggleState(active, juce::dontSendNotification);
    btnSof.setButtonText(
        active ? ("SOF: " + (busTag.isEmpty() ? "MIX 1-2" : busTag))
               : "SOF: OFF");
  }

  void setDspRackBadge(const juce::String &badge) {
    btnDspRack.setButtonText(badge);
  }

  void setTelemetry(float sr, float load, float latMs) {
    sampleRate = sr;
    dspLoad = load;
    latencyMs = latMs;
    repaint();
  }

  void updateClock(const juce::String &timeStr) {
    clockTime = timeStr;
    repaint();
  }

  int getSelectedSceneId() const { return sceneSelect.getSelectedId(); }

  void setSelectedSceneId(int id) {
    sceneSelect.setSelectedId(id, juce::dontSendNotification);
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(midas_colors::kChassisBg);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Brand: MIDAS in italic amber
    g.setFont(midas_ui::getFont(15.0f, true, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MIDAS", 8, 4, 60, 22, juce::Justification::centredLeft);

    // Model: HERITAGE-D HD96-24 in cyan
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("HERITAGE-D HD96-24", 72, 4, 150, 22,
               juce::Justification::centredLeft);

    // Telemetry Capsule: 96 kHz FPGA | 144 IN / 123 BUS
    auto teleRect = juce::Rectangle<float>(228.0f, 5.0f, 250.0f, 20.0f);
    g.setColour(juce::Colour(0xff060c14));
    g.fillRoundedRectangle(teleRect, 4.0f);
    g.setColour(midas_colors::kBorderSubtle);
    g.drawRoundedRectangle(teleRect, 4.0f, 0.8f);

    g.setColour(midas_colors::kAccentGreen);
    g.fillEllipse(teleRect.getX() + 6.0f, teleRect.getY() + 7.0f, 6.0f, 6.0f);

    g.setFont(midas_ui::getFont(8.5f, true));
    g.drawText(juce::String(sampleRate * 0.001f, 0) +
                   " kHz | 32 IN / DUAL WASAPI",
               static_cast<int>(teleRect.getX() + 16.0f),
               static_cast<int>(teleRect.getY()), 170, 20,
               juce::Justification::centredLeft);

    // Digital Wall Clock (Rightmost)
    const float clockX = area.getRight() - 68.0f;
    auto clockRect = juce::Rectangle<float>(clockX, 5.0f, 60.0f, 20.0f);
    g.setColour(juce::Colour(0xff03070d));
    g.fillRoundedRectangle(clockRect, 4.0f);
    g.setColour(midas_colors::kAccentCyan.withAlpha(0.6f));
    g.drawRoundedRectangle(clockRect, 4.0f, 0.8f);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText(clockTime.isEmpty() ? "08:22:55" : clockTime,
               clockRect.toNearestInt(), juce::Justification::centred);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float clockX = area.getRight() - 68.0f;

    btnEngine.setBounds(static_cast<int>(clockX - 146.0f), 5, 140, 20);
    sceneSelect.setBounds(static_cast<int>(clockX - 322.0f), 5, 172, 20);
    btnSof.setBounds(static_cast<int>(clockX - 424.0f), 5, 98, 20);

    // Embedded in Touchscreen Layer - hidden from top status bar to prevent
    // duplication
    btnAiModal.setBounds(0, 0, 0, 0);
    btnAiModal.setVisible(false);
    btnSurroundModal.setBounds(0, 0, 0, 0);
    btnSurroundModal.setVisible(false);
    btnPatchModal.setBounds(0, 0, 0, 0);
    btnPatchModal.setVisible(false);
    btnDspRack.setBounds(0, 0, 0, 0);
    btnDspRack.setVisible(false);
    btnGangModal.setBounds(0, 0, 0, 0);
    btnGangModal.setVisible(false);
    btnMCloudModal.setBounds(0, 0, 0, 0);
    btnMCloudModal.setVisible(false);

    const float midLeft = 484.0f;
    const float midRight = std::max(midLeft + 140.0f, clockX - 430.0f);
    const float midAvail = midRight - midLeft;
    const int wText = 90;
    const int wQs = 56;
    const int gap = 8;
    const int bx = static_cast<int>(
        midLeft + std::max(0.0f, (midAvail - static_cast<float>(wText + wQs + gap)) * 0.5f));
    btnTextScale.setBounds(bx, 5, wText, 20);
    btnQuickSettings.setBounds(bx + wText + gap, 5, wQs, 20);

    btnTextScale.onClick = [this] { cycleTextScale(); };

    btnAiModal.onClick = [this] {
      if (onOpenAiModal)
        onOpenAiModal();
    };
    btnMCloudModal.onClick = [this] {
      if (onOpenMCloudModal)
        onOpenMCloudModal();
    };
    btnDspRack.onClick = [this] {
      if (onOpenDspRackModal)
        onOpenDspRackModal();
    };
    btnGangModal.onClick = [this] {
      if (onOpenGangModal)
        onOpenGangModal();
    };

    btnSof.onClick = [this] {
      sofMode = (sofMode + 1) % 3;
      if (sofMode == 1) {
        btnSof.setToggleState(true, juce::dontSendNotification);
        btnSof.setButtonText("SOF: MONITOR");
        btnSof.setGlowMode(MidasTactileButton::GlowMode::Cyan);
      } else if (sofMode == 2) {
        btnSof.setToggleState(true, juce::dontSendNotification);
        btnSof.setButtonText("SOF: MIX 1-2");
        btnSof.setGlowMode(MidasTactileButton::GlowMode::Purple);
      } else {
        btnSof.setToggleState(false, juce::dontSendNotification);
        btnSof.setButtonText("SOF: OFF");
      }
      if (onSofModeChanged)
        onSofModeChanged(sofMode);
      else if (onSofToggled)
        onSofToggled(sofMode != 0);
    };
    sceneSelect.onChange = [this] {
      if (onSceneChanged)
        onSceneChanged(sceneSelect.getSelectedId());
    };
  }

private:
  float sampleRate = 96000.0f;
  float dspLoad = 28.4f;
  float latencyMs = 0.68f;
  juce::String clockTime = "08:22:55";

  MidasTactileButton btnAiModal, btnSurroundModal, btnPatchModal,
      btnMCloudModal, btnDspRack, btnGangModal, btnTextScale, btnQuickSettings;
  MidasTactileButton btnSof, btnEngine;
  juce::ComboBox sceneSelect;
  bool pullDownTriggered = false;
};

// ==============================================================================
// MIDDLE NAVIGATION BAR (FADER BANKS, POP GROUPS & MOTORIZED AUTOMATION)
// ==============================================================================
class MidasLayerNavBar : public juce::Component {
public:
  MidasLayerNavBar()
      : btnBank1("INPUTS 1-8", MidasTactileButton::GlowMode::Cyan),
        btnBank2("INPUTS 9-16", MidasTactileButton::GlowMode::Cyan),
        btnBankAux("MIX BUS / AUX", MidasTactileButton::GlowMode::Cyan),
        btnBankVca("VCA GROUPS", MidasTactileButton::GlowMode::Cyan),
        btnGeqOnFaders("GEQ ON FADERS", MidasTactileButton::GlowMode::Purple),
        btnPopAll("ALL", MidasTactileButton::GlowMode::Cyan),
        btnPop1("POP 1: DRUMS", MidasTactileButton::GlowMode::Gold),
        btnPop2("POP 2: BASS", MidasTactileButton::GlowMode::Gold),
        btnPop3("POP 3: GTR", MidasTactileButton::GlowMode::Gold),
        btnPop4("POP 4: VOX", MidasTactileButton::GlowMode::Gold),
        btnFadeIn("FADE IN", MidasTactileButton::GlowMode::Cyan),
        btnFadeOut("FADE OUT", MidasTactileButton::GlowMode::Cyan),
        btnAutoWrite("● WRITE", MidasTactileButton::GlowMode::Red),
        btnAutoRead("▶ READ", MidasTactileButton::GlowMode::Green),
        btnLinkPair("LINK 5-6", MidasTactileButton::GlowMode::Gold) {

    addAndMakeVisible(btnBank1);
    addAndMakeVisible(btnBank2);
    addAndMakeVisible(btnBankAux);
    addAndMakeVisible(btnBankVca);
    addAndMakeVisible(btnGeqOnFaders);
    btnBank1.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(btnPopAll);
    addAndMakeVisible(btnPop1);
    addAndMakeVisible(btnPop2);
    addAndMakeVisible(btnPop3);
    addAndMakeVisible(btnPop4);
    btnPopAll.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(btnFadeIn);
    addAndMakeVisible(btnFadeOut);
    addAndMakeVisible(btnAutoWrite);
    addAndMakeVisible(btnAutoRead);
    addAndMakeVisible(btnLinkPair);
    btnAutoRead.setToggleState(true, juce::dontSendNotification);

    allInputsSelect.addItem("CHANNELS: 1-16", 1);
    allInputsSelect.addItem("CHANNELS: 17-32", 2);
    allInputsSelect.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(allInputsSelect);
  }

  std::function<void(int bankIndex)> onBankSelected;
  std::function<void(int popGroup)> onPopGroupSelected;
  std::function<void(int rangeOffset)> onInputRangeChanged;
  std::function<void(bool active)> onGeqOnFadersToggled;
  std::function<void()> onFadeInTriggered;
  std::function<void()> onFadeOutTriggered;
  std::function<void(bool linked)> onLinkPairToggled;

  void selectBank(int index) {
    btnBank1.setToggleState(index == 0, juce::dontSendNotification);
    btnBank2.setToggleState(index == 1, juce::dontSendNotification);
    btnBankAux.setToggleState(index == 2, juce::dontSendNotification);
    btnBankVca.setToggleState(index == 3, juce::dontSendNotification);
    if (onBankSelected)
      onBankSelected(index);
  }

  void selectPop(int popIdx) {
    btnPopAll.setToggleState(popIdx == 0, juce::dontSendNotification);
    btnPop1.setToggleState(popIdx == 1, juce::dontSendNotification);
    btnPop2.setToggleState(popIdx == 2, juce::dontSendNotification);
    btnPop3.setToggleState(popIdx == 3, juce::dontSendNotification);
    btnPop4.setToggleState(popIdx == 4, juce::dontSendNotification);
    if (onPopGroupSelected)
      onPopGroupSelected(popIdx);
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    // Labels
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(midas_colors::kTextMuted);
    g.drawText("LAYERS", 8, 2, 60, 10, juce::Justification::centredLeft);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float btnH = 20.0f;
    const float y = (area.getHeight() - btnH) * 0.5f;

    // 1. Layer Banks
    float x = 8.0f;
    btnBank1.setBounds(static_cast<int>(x), static_cast<int>(y), 80,
                       static_cast<int>(btnH));
    x += 84.0f;
    btnBank2.setBounds(static_cast<int>(x), static_cast<int>(y), 85,
                       static_cast<int>(btnH));
    x += 89.0f;
    btnBankAux.setBounds(static_cast<int>(x), static_cast<int>(y), 96,
                         static_cast<int>(btnH));
    x += 100.0f;
    btnBankVca.setBounds(static_cast<int>(x), static_cast<int>(y), 88,
                         static_cast<int>(btnH));
    x += 92.0f;
    allInputsSelect.setBounds(static_cast<int>(x), static_cast<int>(y), 130,
                              static_cast<int>(btnH));
    x += 134.0f;
    btnGeqOnFaders.setBounds(static_cast<int>(x), static_cast<int>(y), 105,
                             static_cast<int>(btnH));
    x += 115.0f;

    // 2. POP Groups
    btnPopAll.setBounds(static_cast<int>(x), static_cast<int>(y), 36,
                        static_cast<int>(btnH));
    x += 40.0f;
    btnPop1.setBounds(static_cast<int>(x), static_cast<int>(y), 92,
                      static_cast<int>(btnH));
    x += 96.0f;
    btnPop2.setBounds(static_cast<int>(x), static_cast<int>(y), 86,
                      static_cast<int>(btnH));
    x += 90.0f;
    btnPop3.setBounds(static_cast<int>(x), static_cast<int>(y), 86,
                      static_cast<int>(btnH));
    x += 90.0f;
    btnPop4.setBounds(static_cast<int>(x), static_cast<int>(y), 86,
                      static_cast<int>(btnH));
    x += 96.0f;

    // 3. Fader Automation Transport (Rightmost)
    float rx = area.getWidth() - 8.0f;
    rx -= 68.0f;
    btnLinkPair.setBounds(static_cast<int>(rx), static_cast<int>(y), 68,
                          static_cast<int>(btnH));
    rx -= 66.0f;
    btnAutoRead.setBounds(static_cast<int>(rx), static_cast<int>(y), 62,
                          static_cast<int>(btnH));
    rx -= 68.0f;
    btnAutoWrite.setBounds(static_cast<int>(rx), static_cast<int>(y), 64,
                           static_cast<int>(btnH));
    rx -= 70.0f;
    btnFadeOut.setBounds(static_cast<int>(rx), static_cast<int>(y), 66,
                         static_cast<int>(btnH));
    rx -= 66.0f;
    btnFadeIn.setBounds(static_cast<int>(rx), static_cast<int>(y), 62,
                        static_cast<int>(btnH));

    btnFadeIn.setClickingTogglesState(false);
    btnFadeOut.setClickingTogglesState(false);

    btnBank1.onClick = [this] { selectBank(0); };
    btnBank2.onClick = [this] { selectBank(1); };
    btnBankAux.onClick = [this] { selectBank(2); };
    btnBankVca.onClick = [this] { selectBank(3); };

    btnPopAll.onClick = [this] { selectPop(0); };
    btnPop1.onClick = [this] { selectPop(1); };
    btnPop2.onClick = [this] { selectPop(2); };
    btnPop3.onClick = [this] { selectPop(3); };
    btnPop4.onClick = [this] { selectPop(4); };

    allInputsSelect.onChange = [this] {
      const int id = allInputsSelect.getSelectedId();
      if (onInputRangeChanged && id >= 1)
        onInputRangeChanged((id - 1) * 16);
    };

    btnGeqOnFaders.onClick = [this] {
      if (onGeqOnFadersToggled)
        onGeqOnFadersToggled(btnGeqOnFaders.getToggleState());
    };
    btnFadeIn.onClick = [this] {
      if (onFadeInTriggered)
        onFadeInTriggered();
    };
    btnFadeOut.onClick = [this] {
      if (onFadeOutTriggered)
        onFadeOutTriggered();
    };
    btnLinkPair.onClick = [this] {
      if (onLinkPairToggled)
        onLinkPairToggled(btnLinkPair.getToggleState());
    };
  }

private:
  MidasTactileButton btnBank1, btnBank2, btnBankAux, btnBankVca, btnGeqOnFaders;
  MidasTactileButton btnPopAll, btnPop1, btnPop2, btnPop3, btnPop4;
  MidasTactileButton btnFadeIn, btnFadeOut, btnAutoWrite, btnAutoRead,
      btnLinkPair;
  juce::ComboBox allInputsSelect;
};

// ==============================================================================
// LUXURY MAHOGANY & LEATHER ARMREST
// ==============================================================================
class MidasArmrest : public juce::Component {
public:
  MidasArmrest() { setOpaque(true); }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();

    // Mahogany Woodgrain Gradient
    juce::ColourGradient woodGrad(juce::Colour(0xff2b1106), area.getX(),
                                  area.getY(), juce::Colour(0xff120602),
                                  area.getX(), area.getBottom(), false);
    woodGrad.addColour(0.35, juce::Colour(0xff3d1a0a));
    woodGrad.addColour(0.7, juce::Colour(0xff1f0c04));
    g.setGradientFill(woodGrad);
    g.fillRoundedRectangle(area, 4.0f);

    // Stitched Leather Bevel Edge
    g.setColour(juce::Colour(0xff4a2210));
    g.drawHorizontalLine(1, 0.0f, area.getWidth());
    g.setColour(juce::Colour(0xff090402));
    g.drawHorizontalLine(2, 0.0f, area.getWidth());

    // Center Gold Inscription
    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MIDAS CONSOLES UK — HERITAGE-D SERIES", 24, 2, 320,
               static_cast<int>(area.getHeight() - 4.0f),
               juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(8.5f, false));
    g.setColour(midas_colors::kTextMuted);
    g.drawText("40-BIT FLOATING POINT DSP ENGINE • TRUE WEB AUDIO GRAPH",
               static_cast<int>(area.getWidth() - 380.0f), 2, 350,
               static_cast<int>(area.getHeight() - 4.0f),
               juce::Justification::centredRight);
  }
};

// ==============================================================================
// MODAL: GANG FADER CONTROL
// ==============================================================================
struct ChannelLinkGroup {
  int groupId = 1; // 1..16
  bool active = false;
  bool isRelative = false; // false = ABSOLUTE, true = RELATIVE
  bool linkFader = true;
  bool linkPreamp = false;
  bool linkPan = false;
  bool linkMute = false;
  bool linkSolo = false;
  std::array<bool, nova::kLogicalChannels> members{};
};

class LinkCheckboxLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  void drawTickBox(juce::Graphics &g, juce::Component &c, float x, float y,
                   float w, float h, bool ticked, bool /*isEnabled*/,
                   bool /*shouldDrawButtonAsHighlighted*/,
                   bool /*shouldDrawButtonAsDown*/) override {
    juce::ignoreUnused(c);
    auto box = juce::Rectangle<float>(x, y, w, h).reduced(1.5f);
    if (ticked) {
      g.setColour(juce::Colour(0xff0ea5e9));
      g.fillRoundedRectangle(box, 3.5f);
      g.setColour(juce::Colours::white);
      juce::Path tick;
      tick.startNewSubPath(box.getX() + box.getWidth() * 0.22f,
                           box.getY() + box.getHeight() * 0.52f);
      tick.lineTo(box.getX() + box.getWidth() * 0.44f,
                  box.getY() + box.getHeight() * 0.74f);
      tick.lineTo(box.getX() + box.getWidth() * 0.78f,
                  box.getY() + box.getHeight() * 0.28f);
      g.strokePath(tick, juce::PathStrokeType(2.0f));
    } else {
      g.setColour(juce::Colours::white.withAlpha(0.92f));
      g.fillRoundedRectangle(box, 3.5f);
    }
  }
};

class MidasGangFaderModal : public juce::Component {
public:
  static constexpr int kMaxGroups = 16;

  void setEmbedded(bool embedded) {
    isEmbedded = embedded;
    btnClose.setVisible(!embedded);
    setAlwaysOnTop(!embedded);
    resized();
    repaint();
  }
  bool isEmbedded = false;

  MidasGangFaderModal()
      : btnClose("X", MidasTactileButton::GlowMode::Red),
        btnLink("LINK SELECTED", MidasTactileButton::GlowMode::Cyan),
        btnUnlink("UNLINK GROUP", MidasTactileButton::GlowMode::Red) {
    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);
    addAndMakeVisible(btnLink);
    addAndMakeVisible(btnUnlink);
    btnClose.setClickingTogglesState(false);
    btnLink.setClickingTogglesState(false);
    btnUnlink.setClickingTogglesState(false);

    for (int g = 0; g < kMaxGroups; ++g) {
      groups[static_cast<size_t>(g)].groupId = g + 1;
      groupCombo.addItem(juce::String(g + 1), g + 1);
    }
    groupCombo.setSelectedId(1, juce::dontSendNotification);
    groupCombo.setColour(juce::ComboBox::backgroundColourId,
                         juce::Colour(0xff0b121c));
    groupCombo.setColour(juce::ComboBox::outlineColourId,
                         juce::Colour(0xff334155));
    groupCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    groupCombo.onChange = [this] {
      loadGroupIntoUi(groupCombo.getSelectedId() - 1);
    };
    addAndMakeVisible(groupCombo);

    modeCombo.addItem("ABSOLUTE", 1);
    modeCombo.addItem("RELATIVE", 2);
    modeCombo.setSelectedId(1, juce::dontSendNotification);
    modeCombo.setColour(juce::ComboBox::backgroundColourId,
                        juce::Colour(0xff0b121c));
    modeCombo.setColour(juce::ComboBox::outlineColourId,
                        juce::Colour(0xff334155));
    modeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(modeCombo);

    auto initToggle = [&](juce::ToggleButton &tb, const juce::String &txt,
                          bool def = false) {
      tb.setButtonText(txt);
      tb.setLookAndFeel(&checkboxLnf);
      tb.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe2e8f0));
      tb.setToggleState(def, juce::dontSendNotification);
      addAndMakeVisible(tb);
    };
    initToggle(tglFader, "Fader", true);
    initToggle(tglPreamp, "Preamp", false);
    initToggle(tglPan, "Pan", false);
    initToggle(tglMute, "Mute", false);
    initToggle(tglSolo, "Solo", false);

    btnSelectVisible.setButtonText("Select visible");
    btnSelectVisible.setColour(juce::TextButton::buttonColourId,
                               juce::Colours::transparentBlack);
    btnSelectVisible.setColour(juce::TextButton::textColourOffId,
                               juce::Colour(0xff38bdf8));
    btnSelectVisible.onClick = [this] {
      for (int ch : visibleChannels) {
        if (ch >= 0 && ch < nova::kLogicalChannels)
          chToggles[static_cast<size_t>(ch)]->setToggleState(
              true, juce::dontSendNotification);
      }
      updateHintText();
    };
    addAndMakeVisible(btnSelectVisible);

    for (int i = 0; i < nova::kLogicalChannels; ++i) {
      auto tb =
          std::make_unique<juce::ToggleButton>("CH" + juce::String(i + 1));
      tb->setLookAndFeel(&checkboxLnf);
      tb->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe2e8f0));
      tb->onClick = [this] { updateHintText(); };
      chGridContainer.addAndMakeVisible(*tb);
      chToggles[static_cast<size_t>(i)] = std::move(tb);
    }
    chViewport.setViewedComponent(&chGridContainer, false);
    chViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(chViewport);

    lblHint.setFont(midas_ui::getFont(10.0f, false));
    lblHint.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    lblHint.setText("Select minimal dua channel.", juce::dontSendNotification);
    addAndMakeVisible(lblHint);

    btnClose.onClick = [this] { setVisible(false); };

    btnLink.onClick = [this] {
      int selectedCount = 0;
      for (const auto &tb : chToggles) {
        if (tb->getToggleState())
          ++selectedCount;
      }
      if (selectedCount < 2) {
        lblHint.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
        lblHint.setText("Select minimal dua channel.",
                        juce::dontSendNotification);
        return;
      }

      const int gIdx =
          juce::jlimit(0, kMaxGroups - 1, groupCombo.getSelectedId() - 1);
      auto &grp = groups[static_cast<size_t>(gIdx)];
      grp.groupId = gIdx + 1;
      grp.active = true;
      grp.isRelative = (modeCombo.getSelectedId() == 2);
      grp.linkFader = tglFader.getToggleState();
      grp.linkPreamp = tglPreamp.getToggleState();
      grp.linkPan = tglPan.getToggleState();
      grp.linkMute = tglMute.getToggleState();
      grp.linkSolo = tglSolo.getToggleState();

      for (int i = 0; i < nova::kLogicalChannels; ++i) {
        const bool sel = chToggles[static_cast<size_t>(i)]->getToggleState();
        grp.members[static_cast<size_t>(i)] = sel;
        if (sel) {
          // Ensure channel belongs to at most one link group
          for (int og = 0; og < kMaxGroups; ++og) {
            if (og != gIdx)
              groups[static_cast<size_t>(og)].members[static_cast<size_t>(i)] =
                  false;
          }
        }
      }

      if (onGroupsUpdated)
        onGroupsUpdated(groups, gIdx);
      if (isEmbedded) {
        lblHint.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        lblHint.setText("Link Group " + juce::String(gIdx + 1) +
                            " aktif (" + juce::String(selectedCount) +
                            " channel tersimpan).",
                        juce::dontSendNotification);
      } else {
        setVisible(false);
      }
    };

    btnUnlink.onClick = [this] {
      const int gIdx =
          juce::jlimit(0, kMaxGroups - 1, groupCombo.getSelectedId() - 1);
      auto &grp = groups[static_cast<size_t>(gIdx)];
      grp.active = false;
      grp.members.fill(false);
      for (auto &tb : chToggles)
        tb->setToggleState(false, juce::dontSendNotification);
      updateHintText();
      if (onGroupsUpdated)
        onGroupsUpdated(groups, gIdx);
    };
  }

  ~MidasGangFaderModal() override {
    tglFader.setLookAndFeel(nullptr);
    tglPreamp.setLookAndFeel(nullptr);
    tglPan.setLookAndFeel(nullptr);
    tglMute.setLookAndFeel(nullptr);
    tglSolo.setLookAndFeel(nullptr);
    for (auto &tb : chToggles) {
      if (tb != nullptr)
        tb->setLookAndFeel(nullptr);
    }
  }

  std::function<void(const std::array<ChannelLinkGroup, kMaxGroups> &,
                     int updatedGroupIdx)>
      onGroupsUpdated;

  void setVisibleChannels(const std::array<int, 8> &chs) {
    visibleChannels = chs;
  }

  const std::array<ChannelLinkGroup, kMaxGroups> &getGroups() const noexcept {
    return groups;
  }

  void loadGroupIntoUi(int gIdx) {
    gIdx = juce::jlimit(0, kMaxGroups - 1, gIdx);
    const auto &grp = groups[static_cast<size_t>(gIdx)];
    modeCombo.setSelectedId(grp.isRelative ? 2 : 1, juce::dontSendNotification);
    tglFader.setToggleState(grp.linkFader, juce::dontSendNotification);
    tglPreamp.setToggleState(grp.linkPreamp, juce::dontSendNotification);
    tglPan.setToggleState(grp.linkPan, juce::dontSendNotification);
    tglMute.setToggleState(grp.linkMute, juce::dontSendNotification);
    tglSolo.setToggleState(grp.linkSolo, juce::dontSendNotification);
    for (int i = 0; i < nova::kLogicalChannels; ++i) {
      chToggles[static_cast<size_t>(i)]->setToggleState(
          grp.members[static_cast<size_t>(i)], juce::dontSendNotification);
    }
    updateHintText();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (!isEmbedded) {
      g.fillAll(juce::Colours::black.withAlpha(0.75f));
    } else {
      g.fillAll(juce::Colour(0xff0b111a));
    }

    const float dialogW =
        isEmbedded ? (bounds.getWidth() - 8.0f)
                   : juce::jmin(500.0f, bounds.getWidth() - 24.0f);
    const float dialogH =
        isEmbedded ? (bounds.getHeight() - 6.0f)
                   : juce::jmin(540.0f, bounds.getHeight() - 24.0f);
    auto dBox =
        isEmbedded
            ? juce::Rectangle<float>(4.0f, 3.0f, dialogW, dialogH)
            : juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                     (bounds.getHeight() - dialogH) * 0.5f,
                                     dialogW, dialogH);

    g.setColour(juce::Colour(0xff0d1420));
    g.fillRoundedRectangle(dBox, 10.0f);
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(dBox, 10.0f, 1.2f);

    if (isEmbedded) {
      const float x = dBox.getX() + 14.0f;
      const float y = dBox.getY() + 8.0f;
      const float innerW = dialogW - 28.0f;

      g.setFont(midas_ui::getFont(12.0f, true));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("GANG FADER — 16-GROUP CHANNEL LINK", static_cast<int>(x),
                 static_cast<int>(y), 320, 18, juce::Justification::centredLeft);

      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(juce::Colour(0xff94a3b8));
      g.drawText("GROUP ID", static_cast<int>(x), static_cast<int>(y + 22.0f),
                 80, 18, juce::Justification::centredLeft);
      g.drawText("MODE", static_cast<int>(x + 170.0f),
                 static_cast<int>(y + 22.0f), 60, 18,
                 juce::Justification::centredLeft);
      g.drawText("PILIH CHANNEL (CH 1-" + juce::String(nova::kLogicalChannels) +
                     ")",
                 static_cast<int>(x), static_cast<int>(y + 72.0f), 260, 16,
                 juce::Justification::centredLeft);

      auto gridBox =
          juce::Rectangle<float>(x, y + 90.0f, innerW, dialogH - 142.0f);
      g.setColour(juce::Colour(0xff060b12));
      g.fillRoundedRectangle(gridBox, 6.0f);
      g.setColour(juce::Colour(0xff1e293b));
      g.drawRoundedRectangle(gridBox, 6.0f, 1.0f);
      return;
    }

    const float x = dBox.getX() + 18.0f;
    const float y = dBox.getY() + 14.0f;
    const float innerW = dialogW - 36.0f;
    const float halfW = (innerW - 14.0f) * 0.5f;

    g.setFont(midas_ui::getFont(13.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("BUAT / PERBARUI LINK GROUP", static_cast<int>(x),
               static_cast<int>(y), 320, 20, juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText("GROUP ID", static_cast<int>(x), static_cast<int>(y + 30.0f),
               static_cast<int>(halfW), 14, juce::Justification::centredLeft);
    g.drawText("LINK MODE", static_cast<int>(x + halfW + 14.0f),
               static_cast<int>(y + 30.0f), static_cast<int>(halfW), 14,
               juce::Justification::centredLeft);

    g.drawText("PARAMETER YANG DI-LINK", static_cast<int>(x),
               static_cast<int>(y + 84.0f), 260, 14,
               juce::Justification::centredLeft);

    g.drawText("PILIH CHANNEL (CH 1-" + juce::String(nova::kLogicalChannels) +
                   ")",
               static_cast<int>(x), static_cast<int>(y + 158.0f), 260, 14,
               juce::Justification::centredLeft);

    // Dark inset container behind channel grid
    auto gridBox =
        juce::Rectangle<float>(x, y + 178.0f, innerW, dialogH - 270.0f);
    g.setColour(juce::Colour(0xff060b12));
    g.fillRoundedRectangle(gridBox, 6.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(gridBox, 6.0f, 1.0f);
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    if (isEmbedded) {
      btnClose.setVisible(false);
      const float dialogW = bounds.getWidth() - 8.0f;
      const float dialogH = bounds.getHeight() - 6.0f;
      const float x = 4.0f + 14.0f;
      const float y = 3.0f + 8.0f;
      const float innerW = dialogW - 28.0f;

      groupCombo.setBounds(static_cast<int>(x + 70.0f),
                           static_cast<int>(y + 21.0f), 90, 22);
      modeCombo.setBounds(static_cast<int>(x + 218.0f),
                          static_cast<int>(y + 21.0f), 110, 22);

      const float pColW = innerW / 5.0f;
      tglFader.setBounds(static_cast<int>(x), static_cast<int>(y + 47.0f),
                         static_cast<int>(pColW), 22);
      tglPreamp.setBounds(static_cast<int>(x + pColW),
                          static_cast<int>(y + 47.0f), static_cast<int>(pColW),
                          22);
      tglPan.setBounds(static_cast<int>(x + pColW * 2.0f),
                       static_cast<int>(y + 47.0f), static_cast<int>(pColW),
                       22);
      tglMute.setBounds(static_cast<int>(x + pColW * 3.0f),
                        static_cast<int>(y + 47.0f), static_cast<int>(pColW),
                        22);
      tglSolo.setBounds(static_cast<int>(x + pColW * 4.0f),
                        static_cast<int>(y + 47.0f), static_cast<int>(pColW),
                        22);

      btnSelectVisible.setBounds(static_cast<int>(x + innerW - 104.0f),
                                 static_cast<int>(y + 70.0f), 104, 18);

      const int gridH = static_cast<int>(dialogH - 142.0f);
      chViewport.setBounds(static_cast<int>(x + 4.0f),
                           static_cast<int>(y + 92.0f),
                           static_cast<int>(innerW - 8.0f),
                           std::max(40, gridH - 4));

      const int cols = 8;
      const int rows = (nova::kLogicalChannels + cols - 1) / cols;
      const int viewW = static_cast<int>(innerW - 22.0f);
      const int cellW = std::max(68, viewW / cols);
      const int cellH = 24;
      chGridContainer.setSize(cols * cellW, rows * cellH);
      for (int i = 0; i < nova::kLogicalChannels; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        chToggles[static_cast<size_t>(i)]->setBounds(c * cellW + 4, r * cellH,
                                                     cellW - 4, cellH);
      }

      const float btnY = 3.0f + dialogH - 36.0f;
      btnLink.setBounds(static_cast<int>(x), static_cast<int>(btnY), 130, 26);
      btnUnlink.setBounds(static_cast<int>(x + 138.0f), static_cast<int>(btnY),
                          130, 26);
      lblHint.setBounds(static_cast<int>(x + 278.0f), static_cast<int>(btnY),
                        static_cast<int>(std::max(100.0f, innerW - 278.0f)),
                        26);
      return;
    }

    btnClose.setVisible(true);
    const float dialogW = juce::jmin(500.0f, bounds.getWidth() - 24.0f);
    const float dialogH = juce::jmin(540.0f, bounds.getHeight() - 24.0f);
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;
    const float x = dx + 18.0f;
    const float y = dy + 14.0f;
    const float innerW = dialogW - 36.0f;
    const float halfW = (innerW - 14.0f) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 34.0f),
                       static_cast<int>(dy + 10.0f), 24, 24);

    groupCombo.setBounds(static_cast<int>(x), static_cast<int>(y + 46.0f),
                         static_cast<int>(halfW), 28);
    modeCombo.setBounds(static_cast<int>(x + halfW + 14.0f),
                        static_cast<int>(y + 46.0f), static_cast<int>(halfW),
                        28);

    const float pColW = innerW / 3.0f;
    tglFader.setBounds(static_cast<int>(x), static_cast<int>(y + 102.0f),
                       static_cast<int>(pColW), 24);
    tglPreamp.setBounds(static_cast<int>(x + pColW),
                        static_cast<int>(y + 102.0f), static_cast<int>(pColW),
                        24);
    tglPan.setBounds(static_cast<int>(x + pColW * 2.0f),
                     static_cast<int>(y + 102.0f), static_cast<int>(pColW), 24);
    tglMute.setBounds(static_cast<int>(x), static_cast<int>(y + 128.0f),
                      static_cast<int>(pColW), 24);
    tglSolo.setBounds(static_cast<int>(x + pColW), static_cast<int>(y + 128.0f),
                      static_cast<int>(pColW), 24);

    btnSelectVisible.setBounds(static_cast<int>(x + innerW - 104.0f),
                               static_cast<int>(y + 154.0f), 104, 20);

    const int gridH = static_cast<int>(dialogH - 270.0f);
    chViewport.setBounds(static_cast<int>(x + 4.0f),
                         static_cast<int>(y + 182.0f),
                         static_cast<int>(innerW - 8.0f), gridH - 8);

    const int cols = 4;
    const int rows = (nova::kLogicalChannels + cols - 1) / cols;
    const int viewW = static_cast<int>(innerW - 22.0f);
    const int cellW = std::max(72, viewW / cols);
    const int cellH = 26;
    chGridContainer.setSize(cols * cellW, rows * cellH);
    for (int i = 0; i < nova::kLogicalChannels; ++i) {
      const int r = i / cols;
      const int c = i % cols;
      chToggles[static_cast<size_t>(i)]->setBounds(c * cellW + 4, r * cellH,
                                                   cellW - 4, cellH);
    }

    const float btnY = dy + dialogH - 72.0f;
    btnLink.setBounds(static_cast<int>(x), static_cast<int>(btnY), 150, 34);
    btnUnlink.setBounds(static_cast<int>(x + 162.0f), static_cast<int>(btnY),
                        150, 34);
    lblHint.setBounds(static_cast<int>(x), static_cast<int>(btnY + 40.0f),
                      static_cast<int>(innerW), 18);
  }

private:
  void updateHintText() {
    int count = 0;
    for (const auto &tb : chToggles) {
      if (tb->getToggleState())
        ++count;
    }
    const int gIdx =
        juce::jlimit(0, kMaxGroups - 1, groupCombo.getSelectedId() - 1);
    const auto &grp = groups[static_cast<size_t>(gIdx)];
    if (count < 2) {
      lblHint.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
      lblHint.setText("Select minimal dua channel.",
                      juce::dontSendNotification);
    } else {
      lblHint.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
      lblHint.setText(
          "Group " + juce::String(gIdx + 1) + ": " + juce::String(count) +
              " channel siap di-link (" +
              (modeCombo.getSelectedId() == 2 ? "RELATIVE" : "ABSOLUTE") +
              (grp.active ? " - AKTIF" : "") + ").",
          juce::dontSendNotification);
    }
  }

  LinkCheckboxLookAndFeel checkboxLnf;
  MidasTactileButton btnClose, btnLink, btnUnlink;
  juce::ComboBox groupCombo, modeCombo;
  juce::ToggleButton tglFader, tglPreamp, tglPan, tglMute, tglSolo;
  juce::TextButton btnSelectVisible;
  juce::Viewport chViewport;
  juce::Component chGridContainer;
  std::array<std::unique_ptr<juce::ToggleButton>, nova::kLogicalChannels>
      chToggles;
  juce::Label lblHint;
  std::array<ChannelLinkGroup, kMaxGroups> groups{};
  std::array<int, 8> visibleChannels{0, 1, 2, 3, 4, 5, 6, 7};
};

// ==============================================================================
// MODAL: DSP RACK HARDWARE EMULATION MODELS
// ==============================================================================
class MidasDspModelModal : public juce::Component {
public:
  MidasDspModelModal()
      : btnClose("✕", MidasTactileButton::GlowMode::Red),
        btnBackToRack("< BACK TO 24-SLOT RACK",
                      MidasTactileButton::GlowMode::Gold) {
    setAlwaysOnTop(true);
    btnClose.setClickingTogglesState(false);
    btnBackToRack.setClickingTogglesState(false);
    addAndMakeVisible(btnClose);
    addAndMakeVisible(btnBackToRack);

    static const char *eqNames[4] = {"MIDAS PRO 4-PEQ", "NEVE 1073 EQ",
                                     "SSL 4000 G-EQ", "API 550A PROPORTIONAL"};
    static const char *compNames[4] = {"MIDAS VCA CORRECTIVE",
                                       "FET 76 ALL-BUTTONS", "OPTICAL 2A TUBE",
                                       "VARI-MU MASTER GLUE"};
    static const char *gateNames[3] = {
        "MIDAS PRO DUAL-GATE", "TRANSIENT DESIGNER", "EXPANDER / DUCKER"};

    for (int i = 0; i < 4; ++i) {
      eqButtons[i] = std::make_unique<MidasTactileButton>(
          eqNames[i], MidasTactileButton::GlowMode::Cyan);
      compButtons[i] = std::make_unique<MidasTactileButton>(
          compNames[i], MidasTactileButton::GlowMode::Gold);
      eqButtons[i]->setClickingTogglesState(false);
      compButtons[i]->setClickingTogglesState(false);
      addAndMakeVisible(*eqButtons[i]);
      addAndMakeVisible(*compButtons[i]);
      const int idx = i;
      eqButtons[i]->onClick = [this, idx] { selectEq(idx); };
      compButtons[i]->onClick = [this, idx] { selectComp(idx); };
    }
    for (int i = 0; i < 3; ++i) {
      gateButtons[i] = std::make_unique<MidasTactileButton>(
          gateNames[i], MidasTactileButton::GlowMode::Green);
      gateButtons[i]->setClickingTogglesState(false);
      addAndMakeVisible(*gateButtons[i]);
      const int idx = i;
      gateButtons[i]->onClick = [this, idx] { selectGate(idx); };
    }

    eqButtons[0]->setToggleState(true, juce::dontSendNotification);
    // Comp & Gate start off (bypassed) matching ChannelState defaults; click
    // once to enable, click again to bypass.
    btnClose.onClick = [this] { setVisible(false); };
    btnBackToRack.onClick = [this] {
      if (onBackToRack)
        onBackToRack();
    };
  }

  std::function<void(const juce::String &eqName, const juce::String &compName)>
      onModelChanged;
  std::function<void()> onBackToRack;
  int lastChangedSection = 0; // 0=EQ, 1=Comp, 2=Gate

  void selectEq(int idx) {
    lastChangedSection = 0;
    const bool wasActive = eqButtons[idx]->getToggleState();
    for (int i = 0; i < 4; ++i)
      eqButtons[i]->setToggleState(!wasActive && (i == idx),
                                   juce::dontSendNotification);
    if (onModelChanged)
      onModelChanged(getActiveEqName(), getActiveCompName());
    repaint();
  }

  void selectComp(int idx) {
    lastChangedSection = 1;
    const bool wasActive = compButtons[idx]->getToggleState();
    for (int i = 0; i < 4; ++i)
      compButtons[i]->setToggleState(!wasActive && (i == idx),
                                     juce::dontSendNotification);
    if (onModelChanged)
      onModelChanged(getActiveEqName(), getActiveCompName());
    repaint();
  }

  void selectGate(int idx) {
    lastChangedSection = 2;
    const bool wasActive = gateButtons[idx]->getToggleState();
    for (int i = 0; i < 3; ++i)
      gateButtons[i]->setToggleState(!wasActive && (i == idx),
                                     juce::dontSendNotification);
    if (onModelChanged)
      onModelChanged(getActiveEqName(), getActiveCompName());
    repaint();
  }

  bool isEqActive() const {
    for (int i = 0; i < 4; ++i) {
      if (eqButtons[i]->getToggleState())
        return true;
    }
    return false;
  }

  bool isCompActive() const {
    for (int i = 0; i < 4; ++i) {
      if (compButtons[i]->getToggleState())
        return true;
    }
    return false;
  }

  bool isGateActive() const {
    for (int i = 0; i < 3; ++i) {
      if (gateButtons[i]->getToggleState())
        return true;
    }
    return false;
  }

  juce::String getActiveEqName() const {
    for (int i = 0; i < 4; ++i) {
      if (eqButtons[i]->getToggleState())
        return eqButtons[i]->getButtonText();
    }
    return "BYPASSED";
  }

  juce::String getActiveCompName() const {
    for (int i = 0; i < 4; ++i) {
      if (compButtons[i]->getToggleState())
        return compButtons[i]->getButtonText();
    }
    return "BYPASSED";
  }

  juce::String getActiveGateName() const {
    for (int i = 0; i < 3; ++i) {
      if (gateButtons[i]->getToggleState())
        return gateButtons[i]->getButtonText();
    }
    return "BYPASSED";
  }

  int getActiveEqIndex() const {
    for (int i = 0; i < 4; ++i) {
      if (eqButtons[i]->getToggleState())
        return i;
    }
    return 0;
  }

  int getActiveGateIndex() const {
    for (int i = 0; i < 3; ++i) {
      if (gateButtons[i]->getToggleState())
        return i;
    }
    return 0;
  }

  void setEmbedded(bool embedded) {
    isEmbedded = embedded;
    btnClose.setVisible(!embedded);
    btnBackToRack.setVisible(embedded);
    setAlwaysOnTop(!embedded);
  }
  bool isEmbedded = false;

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (!isEmbedded) {
      g.fillAll(juce::Colours::black.withAlpha(0.75f));
    } else {
      g.fillAll(juce::Colour(0xff0b111a));
    }

    const float dialogW = isEmbedded ? (bounds.getWidth() - 8.0f) : 560.0f;
    const float dialogH = isEmbedded ? (bounds.getHeight() - 6.0f) : 340.0f;
    auto dBox =
        isEmbedded
            ? juce::Rectangle<float>(4.0f, 3.0f, dialogW, dialogH)
            : juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                     (bounds.getHeight() - dialogH) * 0.5f,
                                     dialogW, dialogH);

    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(dBox, 6.0f);
    g.setColour(midas_colors::kAccentGold.withAlpha(0.75f));
    g.drawRoundedRectangle(dBox, 6.0f, 1.2f);

    g.setFont(midas_ui::getFont(13.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("DSP RACK HARDWARE EMULATION MODELS",
               static_cast<int>(dBox.getX() + 14.0f),
               static_cast<int>(dBox.getY() + 8.0f), 360, 20,
               juce::Justification::centredLeft);

    if (isEmbedded) {
      auto inner = dBox.reduced(12.0f, 8.0f);
      inner.removeFromTop(26.0f);
      auto footer = inner.removeFromBottom(24.0f);
      const float gap = 6.0f;
      const float secH = (inner.getHeight() - gap * 2.0f) / 3.0f;

      static const char *kEqDesc[4] = {
          "Clean Surgical Live EQ", "Warm Class-A British Inductor",
          "Punchy Console Bus EQ", "Musical Proportional-Q Bite"};
      static const char *kCompDesc[4] = {
          "Transparent Live VCA Control", "Aggressive Fast Drum / Vocal",
          "Smooth Tube Levelling", "Warm Bus & Mix Glue"};
      static const char *kGateDesc[3] = {"Precision Live Dual-Gate",
                                         "Punchy Attack & Sustain Shape",
                                         "Gentle Downward Expander"};

      auto drawSection = [&](juce::Rectangle<float> sec, const char *hdr,
                             juce::Colour col, int count,
                             const char *const *descs) {
        g.setColour(juce::Colour(0xff0f1722));
        g.fillRoundedRectangle(sec, 5.0f);
        g.setColour(col.withAlpha(0.45f));
        g.drawRoundedRectangle(sec, 5.0f, 1.0f);
        g.fillRoundedRectangle(sec.getX() + 1.0f, sec.getY() + 4.0f, 3.0f,
                               sec.getHeight() - 8.0f, 1.5f);

        g.setFont(midas_ui::getFont(10.0f, true));
        g.setColour(col);
        g.drawText(hdr, static_cast<int>(sec.getX() + 12.0f),
                   static_cast<int>(sec.getY() + 5.0f),
                   static_cast<int>(sec.getWidth() - 24.0f), 14,
                   juce::Justification::centredLeft);

        const float descY = sec.getBottom() - 16.0f;
        const float colW = (sec.getWidth() - 24.0f - (count - 1) * 6.0f) /
                           static_cast<float>(count);
        g.setFont(midas_ui::getFont(9.0f, false));
        g.setColour(juce::Colour(0xff94a3b8));
        for (int i = 0; i < count; ++i) {
          g.drawText(descs[i],
                     static_cast<int>(sec.getX() + 12.0f + i * (colW + 6.0f)),
                     static_cast<int>(descY), static_cast<int>(colW), 14,
                     juce::Justification::centred, true);
        }
      };

      auto sec1 = inner.removeFromTop(secH);
      inner.removeFromTop(gap);
      auto sec2 = inner.removeFromTop(secH);
      inner.removeFromTop(gap);
      auto sec3 = inner;

      drawSection(sec1, "1. PARAMETRIC EQUALISER HARDWARE EMULATION",
                  midas_colors::kAccentCyan, 4, kEqDesc);
      drawSection(sec2, "2. DYNAMICS / COMPRESSOR HARDWARE EMULATION",
                  midas_colors::kAccentGold, 4, kCompDesc);
      drawSection(sec3, "3. NOISE GATE & TRANSIENT EMULATION",
                  midas_colors::kAccentGreen, 3, kGateDesc);

      g.setColour(juce::Colour(0xff0f1722));
      g.fillRoundedRectangle(footer, 4.0f);
      g.setColour(juce::Colour(0xff1e293b));
      g.drawRoundedRectangle(footer, 4.0f, 1.0f);
      g.setFont(midas_ui::getFont(10.0f, true));
      g.setColour(juce::Colour(0xffcbd5e1));
      g.drawText("ACTIVE CHAIN:  EQ: " + getActiveEqName() +
                     "   |   COMP: " + getActiveCompName() +
                     "   |   GATE: " + getActiveGateName(),
                 footer.reduced(10.0f, 0.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);
      return;
    }

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("1. PARAMETRIC EQUALISER HARDWARE EMULATION",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 42.0f), 400, 14,
               juce::Justification::centredLeft);

    g.setColour(midas_colors::kAccentGold);
    g.drawText("2. DYNAMICS / COMPRESSOR HARDWARE EMULATION",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 132.0f), 400, 14,
               juce::Justification::centredLeft);

    g.setColour(midas_colors::kAccentGreen);
    g.drawText("3. NOISE GATE & TRANSIENT EMULATION",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 222.0f), 400, 14,
               juce::Justification::centredLeft);
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    if (isEmbedded) {
      btnClose.setVisible(false);
      btnBackToRack.setVisible(true);
      auto dBox = juce::Rectangle<float>(4.0f, 3.0f, bounds.getWidth() - 8.0f,
                                         bounds.getHeight() - 6.0f);
      btnBackToRack.setBounds(static_cast<int>(dBox.getRight() - 186.0f),
                              static_cast<int>(dBox.getY() + 6.0f), 174, 22);

      auto inner = dBox.reduced(12.0f, 8.0f);
      inner.removeFromTop(26.0f);
      inner.removeFromBottom(24.0f);
      const float gap = 6.0f;
      const float secH = (inner.getHeight() - gap * 2.0f) / 3.0f;
      auto sec1 = inner.removeFromTop(secH);
      inner.removeFromTop(gap);
      auto sec2 = inner.removeFromTop(secH);
      inner.removeFromTop(gap);
      auto sec3 = inner;

      auto layoutRow = [](juce::Rectangle<float> sec, int count,
                          auto &buttons) {
        const float colW = (sec.getWidth() - 24.0f - (count - 1) * 6.0f) /
                           static_cast<float>(count);
        const float btnH = juce::jlimit(26.0f, 44.0f, sec.getHeight() - 40.0f);
        const float btnY = sec.getY() + 21.0f;
        for (int i = 0; i < count; ++i) {
          buttons[i]->setBounds(
              static_cast<int>(sec.getX() + 12.0f + i * (colW + 6.0f)),
              static_cast<int>(btnY), static_cast<int>(colW),
              static_cast<int>(btnH));
        }
      };

      layoutRow(sec1, 4, eqButtons);
      layoutRow(sec2, 4, compButtons);
      layoutRow(sec3, 3, gateButtons);
      return;
    }

    btnBackToRack.setVisible(false);
    const float dialogW = 560.0f;
    const float dialogH = 340.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f),
                       static_cast<int>(dy + 8.0f), 24, 24);

    const float btnW = (dialogW - 48.0f - 3.0f * 6.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
      eqButtons[i]->setBounds(static_cast<int>(dx + 20.0f + i * (btnW + 6.0f)),
                              static_cast<int>(dy + 62.0f),
                              static_cast<int>(btnW), 32);
      compButtons[i]->setBounds(
          static_cast<int>(dx + 20.0f + i * (btnW + 6.0f)),
          static_cast<int>(dy + 152.0f), static_cast<int>(btnW), 32);
    }
    const float gBtnW = (dialogW - 48.0f - 2.0f * 6.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
      gateButtons[i]->setBounds(
          static_cast<int>(dx + 20.0f + i * (gBtnW + 6.0f)),
          static_cast<int>(dy + 242.0f), static_cast<int>(gBtnW), 32);
    }
  }

private:
  MidasTactileButton btnClose;
  MidasTactileButton btnBackToRack;
  std::array<std::unique_ptr<MidasTactileButton>, 4> eqButtons;
  std::array<std::unique_ptr<MidasTactileButton>, 4> compButtons;
  std::array<std::unique_ptr<MidasTactileButton>, 3> gateButtons;
};

// ==============================================================================
// MODAL: ADVANCED MIDAS AI INTELLIGENCE SYSTEM (9 NEXT-GEN AI MODULES)
// ==============================================================================
class MidasAiModal : public juce::Component, private juce::Timer {
public:
  void setEmbedded(bool embedded) {
    isEmbedded = embedded;
    btnClose.setVisible(!embedded);
    setAlwaysOnTop(!embedded);
  }
  bool isEmbedded = false;

  explicit MidasAiModal(MixerEngine &eng)
      : engine(eng), btnClose("X", MidasTactileButton::GlowMode::Red),
        // 1. Mode Switch
        btnModeSoundEngineer("ASSIST (MANUAL)",
                             MidasTactileButton::GlowMode::Cyan),
        btnModeAutonomous("AUTO-PILOT", MidasTactileButton::GlowMode::Gold),
        // 2. Simple View Switcher: Quick AI vs Live Tools
        btnSuiteCore("QUICK AI", MidasTactileButton::GlowMode::Cyan),
        btnSuiteAdvanced("LIVE TOOLS", MidasTactileButton::GlowMode::Gold),
        // Legacy Core Module Tabs (kept hidden in simplified UI)
        btnTabGain("1. AI GAIN", MidasTactileButton::GlowMode::Cyan),
        btnTabEq("2. AI EQ", MidasTactileButton::GlowMode::Gold),
        btnTabComp("3. AI COMPRESSOR", MidasTactileButton::GlowMode::Green),
        btnTabGate("4. AI GATE", MidasTactileButton::GlowMode::Purple),
        btnTabFx("5. AI FX", MidasTactileButton::GlowMode::Cyan),
        btnTabMonitor("6. AI MONITOR", MidasTactileButton::GlowMode::Gold),
        btnTabFeedback("7. AI FEEDBACK", MidasTactileButton::GlowMode::Red),
        btnTabMaster("8. AI MASTER", MidasTactileButton::GlowMode::Purple),
        // Live Tools Row 2 (3 direct tool buttons)
        btnTabSelfLearning("UNMASK VOX & BASS",
                           MidasTactileButton::GlowMode::Purple),
        btnTabAutoSoundcheck("CYCLE AUTO SCENE",
                             MidasTactileButton::GlowMode::Cyan),
        btnTabAutoScene("LEARN / CLEAR PROFILE",
                        MidasTactileButton::GlowMode::Gold),
        btnTabMultiMic("4. MULTI-MIC INTEL",
                       MidasTactileButton::GlowMode::Purple),
        btnTabStageIntel("5. STAGE INTEL", MidasTactileButton::GlowMode::Cyan),
        btnTabAcousticIntel("6. ACOUSTIC INTEL",
                            MidasTactileButton::GlowMode::Gold),
        // Primary Action Controls
        btnApplyModule("APPLY THIS MODULE", MidasTactileButton::GlowMode::Cyan),
        btnApplyAllModules("2. APPLY AI", MidasTactileButton::GlowMode::Green),
        btnStartCapture("1. CAPTURE (4S)", MidasTactileButton::GlowMode::Gold),
        btnToggleModuleAutonomous("MODULE AUTO-PILOT: ACTIVE",
                                  MidasTactileButton::GlowMode::Green),
        btnEmergencyFreeze("EMERGENCY FREEZE AI",
                           MidasTactileButton::GlowMode::Red),
        btnResetNominal("3. UNDO / RESTORE",
                        MidasTactileButton::GlowMode::Cyan),
        // Live Tools Row 1 (3 direct tool buttons)
        btnAdvAction1("AUTO SOUNDCHECK (1-16)",
                      MidasTactileButton::GlowMode::Cyan),
        btnAdvAction2("IEM LIMITER -6dB (AUX 3-4)",
                      MidasTactileButton::GlowMode::Gold),
        btnAdvAction3("CALIBRATE MASTER GEQ",
                      MidasTactileButton::GlowMode::Green) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);

    // Dua Mode Kecerdasan
    addAndMakeVisible(btnModeSoundEngineer);
    addAndMakeVisible(btnModeAutonomous);

    // Suite Switcher
    addAndMakeVisible(btnSuiteCore);
    addAndMakeVisible(btnSuiteAdvanced);

    // Delapan Modul AI Otomatis
    addChildComponent(btnTabGain);
    addChildComponent(btnTabEq);
    addChildComponent(btnTabComp);
    addChildComponent(btnTabGate);
    addChildComponent(btnTabFx);
    addChildComponent(btnTabMonitor);
    addChildComponent(btnTabFeedback);
    addChildComponent(btnTabMaster);

    // Enam Fitur Lanjutan AI
    addChildComponent(btnTabSelfLearning);
    addChildComponent(btnTabAutoSoundcheck);
    addChildComponent(btnTabAutoScene);
    addChildComponent(btnTabMultiMic);
    addChildComponent(btnTabStageIntel);
    addChildComponent(btnTabAcousticIntel);

    // Action Controls
    addAndMakeVisible(btnApplyModule);
    addAndMakeVisible(btnApplyAllModules);
    addAndMakeVisible(btnStartCapture);
    addAndMakeVisible(btnToggleModuleAutonomous);
    addAndMakeVisible(btnEmergencyFreeze);
    addAndMakeVisible(btnResetNominal);

    // Advanced Action Buttons
    addChildComponent(btnAdvAction1);
    addChildComponent(btnAdvAction2);
    addChildComponent(btnAdvAction3);

    btnClose.setClickingTogglesState(false);
    btnApplyModule.setClickingTogglesState(false);
    btnApplyAllModules.setClickingTogglesState(false);
    btnStartCapture.setClickingTogglesState(false);
    btnResetNominal.setClickingTogglesState(false);
    btnAdvAction1.setClickingTogglesState(false);
    btnAdvAction2.setClickingTogglesState(false);
    btnAdvAction3.setClickingTogglesState(false);
    btnTabSelfLearning.setClickingTogglesState(false);
    btnTabAutoSoundcheck.setClickingTogglesState(false);
    btnTabAutoScene.setClickingTogglesState(false);

    btnClose.onClick = [this] {
      stopTimer();
      setVisible(false);
    };

    // Mode Selection Handlers
    btnModeSoundEngineer.onClick = [this] {
      engine.channelAI.setMode(nova::graviton::AIMode::SoundEngineer);
      updateModeVisuals();
      aiLog = "Mode: Assist (Manual) - Capture sinyal lalu klik Apply AI.";
      repaint();
    };

    btnModeAutonomous.onClick = [this] {
      engine.channelAI.setMode(nova::graviton::AIMode::AutonomousMixer);
      updateModeVisuals();
      aiLog =
          "Mode: Auto-Pilot - Penyetelan otomatis bertahap (Slew <= 1.0 dB/s).";
      repaint();
    };

    // Simple View Switcher Handlers
    btnSuiteCore.onClick = [this] {
      isAdvancedSuite = false;
      btnSuiteCore.setToggleState(true, juce::dontSendNotification);
      btnSuiteAdvanced.setToggleState(false, juce::dontSendNotification);
      updateSuiteVisibility();
    };

    btnSuiteAdvanced.onClick = [this] {
      isAdvancedSuite = true;
      btnSuiteCore.setToggleState(false, juce::dontSendNotification);
      btnSuiteAdvanced.setToggleState(true, juce::dontSendNotification);
      updateSuiteVisibility();
    };

    // Live Tools Row 2 Handlers (direct actions)
    btnTabSelfLearning.onClick = [this] { handleAdvancedAction(4); };
    btnTabAutoSoundcheck.onClick = [this] { handleAdvancedAction(5); };
    btnTabAutoScene.onClick = [this] { handleAdvancedAction(6); };

    // Advisory Mode Actions
    btnApplyModule.onClick = [this] { applyCurrentModuleSetting(); };

    btnApplyAllModules.onClick = [this] { applyAllModulesSetting(); };

    btnStartCapture.onClick = [this] {
      if (activeChannelIndex >= 0 &&
          activeChannelIndex < static_cast<int>(engine.channels.size())) {
        const double sr = engine.sampleRate.load(std::memory_order_relaxed);
        engine.channelAI.startProfiling(activeChannelIndex, sr, 4.0);
        analyzed = false;
        captureProgress = 0.0f;
        aiLog = "Listening to audio signal on " + channelName +
                "... (Capture 4.0s @ " + juce::String(static_cast<int>(sr)) +
                " Hz)";
        repaint();
      }
    };

    // Autonomous Mode Actions
    btnToggleModuleAutonomous.onClick = [this] {
      const auto modId =
          static_cast<nova::graviton::AIModuleId>(currentModuleTab);
      const bool curEn = engine.channelAI.isModuleEnabled(modId);
      engine.channelAI.setModuleEnabled(modId, !curEn);
      btnToggleModuleAutonomous.setButtonText(
          !curEn ? "MODULE AUTO-PILOT: ACTIVE" : "MODULE AUTO-PILOT: BYPASSED");
      aiLog = juce::String(nova::graviton::aiModuleToString(modId)) +
              (!curEn ? " auto-pilot ENGAGED" : " auto-pilot BYPASSED");
      repaint();
    };

    btnEmergencyFreeze.onClick = [this] {
      const bool frozen = !engine.channelAI.isEmergencyFrozen();
      engine.channelAI.setEmergencyFreeze(frozen);
      btnEmergencyFreeze.setToggleState(frozen, juce::dontSendNotification);
      btnEmergencyFreeze.setButtonText(frozen ? "AI FROZEN (LOCKED)"
                                              : "EMERGENCY FREEZE AI");
      aiLog =
          frozen ? "EMERGENCY FREEZE ACTIVATED: All parameters locked in place!"
                 : "Emergency freeze lifted: AI operational.";
      repaint();
    };

    btnResetNominal.onClick = [this] { restoreNominalMix(); };

    // Live Tools Row 1 Handlers (direct actions)
    btnAdvAction1.onClick = [this] { handleAdvancedAction(1); };
    btnAdvAction2.onClick = [this] { handleAdvancedAction(2); };
    btnAdvAction3.onClick = [this] { handleAdvancedAction(3); };

    btnSuiteCore.setToggleState(true, juce::dontSendNotification);
    updateSuiteVisibility();
    updateModeVisuals();
  }

  ~MidasAiModal() override { stopTimer(); }

  std::function<void(int channelIdx,
                     const nova::graviton::AIRecommendation &rec,
                     int applyScope)>
      onApplyAiProfile;
  std::function<void()> onChannelUpdated;

  void updateSuiteVisibility() {
    // Legacy sub-tabs stay hidden in simplified UI
    btnTabGain.setVisible(false);
    btnTabEq.setVisible(false);
    btnTabComp.setVisible(false);
    btnTabGate.setVisible(false);
    btnTabFx.setVisible(false);
    btnTabMonitor.setVisible(false);
    btnTabFeedback.setVisible(false);
    btnTabMaster.setVisible(false);
    btnTabMultiMic.setVisible(false);
    btnTabStageIntel.setVisible(false);
    btnTabAcousticIntel.setVisible(false);

    // 6 Direct Live Tools buttons (2 rows of 3)
    btnAdvAction1.setVisible(isAdvancedSuite);
    btnAdvAction2.setVisible(isAdvancedSuite);
    btnAdvAction3.setVisible(isAdvancedSuite);
    btnTabSelfLearning.setVisible(isAdvancedSuite);
    btnTabAutoSoundcheck.setVisible(isAdvancedSuite);
    btnTabAutoScene.setVisible(isAdvancedSuite);

    updateModeVisuals();
    resized();
    repaint();
  }

  void updateModeVisuals() {
    const bool isAutonomous =
        (engine.channelAI.getMode() == nova::graviton::AIMode::AutonomousMixer);
    btnModeSoundEngineer.setToggleState(!isAutonomous,
                                        juce::dontSendNotification);
    btnModeAutonomous.setToggleState(isAutonomous, juce::dontSendNotification);

    btnApplyModule.setVisible(false);
    btnToggleModuleAutonomous.setVisible(false);

    if (!isAdvancedSuite) {
      btnStartCapture.setVisible(true);
      btnApplyAllModules.setVisible(!isAutonomous);
      btnEmergencyFreeze.setVisible(isAutonomous);
      btnResetNominal.setVisible(true);
    } else {
      btnApplyAllModules.setVisible(false);
      btnStartCapture.setVisible(false);
      btnEmergencyFreeze.setVisible(false);
      btnResetNominal.setVisible(false);
    }
  }

  void switchModuleTab(int tabIdx) {
    currentModuleTab = tabIdx;
    btnTabGain.setToggleState(tabIdx == 0, juce::dontSendNotification);
    btnTabEq.setToggleState(tabIdx == 1, juce::dontSendNotification);
    btnTabComp.setToggleState(tabIdx == 2, juce::dontSendNotification);
    btnTabGate.setToggleState(tabIdx == 3, juce::dontSendNotification);
    btnTabFx.setToggleState(tabIdx == 4, juce::dontSendNotification);
    btnTabMonitor.setToggleState(tabIdx == 5, juce::dontSendNotification);
    btnTabFeedback.setToggleState(tabIdx == 6, juce::dontSendNotification);
    btnTabMaster.setToggleState(tabIdx == 7, juce::dontSendNotification);

    updateModeVisuals();
    repaint();
  }

  void switchAdvancedTab(int tabIdx) {
    currentAdvancedTab = tabIdx;
    btnTabSelfLearning.setToggleState(tabIdx == 0, juce::dontSendNotification);
    btnTabAutoSoundcheck.setToggleState(tabIdx == 1,
                                        juce::dontSendNotification);
    btnTabAutoScene.setToggleState(tabIdx == 2, juce::dontSendNotification);
    btnTabMultiMic.setToggleState(tabIdx == 3, juce::dontSendNotification);
    btnTabStageIntel.setToggleState(tabIdx == 4, juce::dontSendNotification);
    btnTabAcousticIntel.setToggleState(tabIdx == 5, juce::dontSendNotification);

    // Update Action Button Labels based on active advanced tab
    switch (tabIdx) {
    case 0: // Self Learning
      btnAdvAction1.setButtonText("LEARN ROOM & VOCAL PROFILE");
      btnAdvAction2.setButtonText("RECALL LEARNED VENUE MODEL");
      btnAdvAction3.setButtonText("CLEAR AI MEMORY");
      break;
    case 1: // Auto Soundcheck
      btnAdvAction1.setButtonText("RUN 16-CH VIRTUAL SOUNDCHECK");
      btnAdvAction2.setButtonText("ALIGN MULTI-MIC POLARITY");
      btnAdvAction3.setButtonText("HEADROOM AUDIT (0 CLIPS)");
      break;
    case 2: // Auto Scene
      btnAdvAction1.setButtonText("BALLAD / ACOUSTIC SCENE");
      btnAdvAction2.setButtonText("HIGH-ENERGY ROCK SCENE");
      btnAdvAction3.setButtonText("SPEECH / MC BREAK SCENE");
      break;
    case 3: // Multi-Mic Intel
      btnAdvAction1.setButtonText("UNMASK LEAD VOCAL (CARVE 2.8kHz)");
      btnAdvAction2.setButtonText("UNMASK KICK & BASS (CARVE 80Hz)");
      btnAdvAction3.setButtonText("ENABLE DYNAMIC SPECTRAL UNMASK");
      break;
    case 4: // Stage Intel
      btnAdvAction1.setButtonText("AUTO-MAP STAGE POSITIONS");
      btnAdvAction2.setButtonText("ENGAGE IEM SAFETY LIMITER (-6dBFS)");
      btnAdvAction3.setButtonText("OPTIMIZE WEDGE & IEM SPLIT");
      break;
    case 5: // Acoustic Intel
      btnAdvAction1.setButtonText("RUN DUAL-FFT TRANSFER FUNCTION");
      btnAdvAction2.setButtonText("AUTO-FLATTEN 31-BAND GEQ");
      btnAdvAction3.setButtonText("PHASE-ALIGN SUBWOOFER (+4.2ms)");
      break;
    }

    repaint();
  }

  void handleAdvancedAction(int actionNum) {
    auto &ai = engine.channelAI;
    const auto &limits = ai.getSafetyLimits();
    if (actionNum == 1) { // Auto Soundcheck (Ch 1-16)
      const auto res = engine.runAutoSoundcheckAudit(16);
      if (res.auditedChannels > 0) {
        aiLog = "Auto Soundcheck: " + juce::String(res.auditedChannels) +
                " active ch calibrated to -18 dBFS | Headroom: +" +
                juce::String(res.averageHeadroomDb, 1) + " dB";
      } else {
        aiLog = "Auto Soundcheck: 0 active channels (> -65 dBFS) detected "
                "on Ch 1-16. Feed audio first.";
      }
      if (onChannelUpdated)
        onChannelUpdated();
    } else if (actionNum == 2) { // IEM Ear-Safety Limiter -6 dBFS (Aux 3-4)
      auto &st = ai.getStageData();
      const bool nextLim = !st.hearingProtectionLimiter;
      st.hearingProtectionLimiter = nextLim;
      engine.auxIemCeilingDb.store(st.iemCeilingDb, std::memory_order_relaxed);
      if (engine.auxIemLimiterEnabled.size() >= 4) {
        engine.auxIemLimiterEnabled[2].store(nextLim,
                                             std::memory_order_relaxed);
        engine.auxIemLimiterEnabled[3].store(nextLim,
                                             std::memory_order_relaxed);
      }
      aiLog = juce::String(nextLim ? "IEM Limiter ON: " : "IEM Limiter OFF: ") +
              juce::String(st.iemCeilingDb, 1) +
              " dBFS Brickwall on Aux 3 & 4.";
    } else if (actionNum == 3) { // Calibrate Master 31-Band GEQ
      auto &ac = ai.getAcousticData();
      engine.mainGraphicEQ.setBandGain(5, -4.5f);  // 63 Hz
      engine.mainGraphicEQ.setBandGain(11, 3.0f);  // 250 Hz
      engine.mainGraphicEQ.setBandGain(23, -2.5f); // 4000 Hz
      engine.graphicEqEnabled.store(true, std::memory_order_relaxed);
      ac.measurementMicActive = true;
      ac.geqFlattened = true;
      aiLog = "Master 31-Band DN360 GEQ calibrated & enabled!";
      if (onChannelUpdated)
        onChannelUpdated();
    } else if (actionNum == 4) { // Unmask Vox & Bass
      auto &mm = ai.getMultiMicData();
      for (size_t c = 4; c < 8 && c < engine.channels.size(); ++c) {
        engine.channels[c].eqEnabled.store(true, std::memory_order_relaxed);
        engine.channels[c].eqFrequency[2].store(2800.0f,
                                                std::memory_order_relaxed);
        engine.channels[c].eqGainDb[2].store(
            std::clamp(-3.0f, limits.minEqGainDb, limits.maxEqGainDb),
            std::memory_order_relaxed);
        engine.channels[c].eqQ[2].store(2.0f, std::memory_order_relaxed);
      }
      if (engine.channels.size() > 3) {
        auto &bass = engine.channels[3];
        bass.eqEnabled.store(true, std::memory_order_relaxed);
        bass.eqFrequency[1].store(80.0f, std::memory_order_relaxed);
        bass.eqGainDb[1].store(
            std::clamp(-4.0f, limits.minEqGainDb, limits.maxEqGainDb),
            std::memory_order_relaxed);
        bass.eqQ[1].store(2.5f, std::memory_order_relaxed);
      }
      mm.leadVoxUnmasked = true;
      mm.kickBassUnmasked = true;
      mm.detectedCollisions = 0;
      aiLog = "Multi-Mic Unmasking: Carved -3dB @ 2.8kHz (Ch 5-8) & -4dB @ "
              "80Hz (Ch 4 Bass).";
      if (onChannelUpdated)
        onChannelUpdated();
    } else if (actionNum == 5) { // Cycle Auto Scene
      const auto cur = ai.getAutoSceneStyle();
      const auto next =
          (cur == nova::graviton::AutoSceneStyle::BalladAcoustic)
              ? nova::graviton::AutoSceneStyle::HighEnergyRock
              : ((cur == nova::graviton::AutoSceneStyle::HighEnergyRock)
                     ? nova::graviton::AutoSceneStyle::SpeechBreak
                     : nova::graviton::AutoSceneStyle::BalladAcoustic);
      ai.setAutoSceneStyle(next);
      if (activeChannelIndex >= 0 &&
          activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        if (next == nova::graviton::AutoSceneStyle::BalladAcoustic) {
          ch.compressorEnabled.store(true, std::memory_order_relaxed);
          ch.compressorMode.store(
              static_cast<int>(nova::graviton::CompressorMode::Vintage),
              std::memory_order_relaxed);
          ch.compressorRatio.store(2.2f, std::memory_order_relaxed);
        } else if (next == nova::graviton::AutoSceneStyle::HighEnergyRock) {
          ch.compressorEnabled.store(true, std::memory_order_relaxed);
          ch.compressorMode.store(
              static_cast<int>(nova::graviton::CompressorMode::Creative),
              std::memory_order_relaxed);
          ch.compressorRatio.store(4.0f, std::memory_order_relaxed);
          ch.gateEnabled.store(true, std::memory_order_relaxed);
        } else {
          ch.highPassEnabled.store(true, std::memory_order_relaxed);
          ch.highPassFreqHz.store(120.0f, std::memory_order_relaxed);
          ch.duganGroup.store(1, std::memory_order_relaxed);
        }
      }
      aiLog = "Auto-Scene: " +
              juce::String(nova::graviton::autoSceneStyleToString(next));
      if (onChannelUpdated)
        onChannelUpdated();
    } else if (actionNum == 6) { // Learn / Clear Profile
      if (ai.learnFromRecommendation(currentRec)) {
        aiLog = "Self-Learning: Profil #" +
                juce::String(ai.getSelfLearningMemory().totalLearnedProfiles) +
                " disimpan dari " + channelName + ".";
      } else if (ai.getSelfLearningMemory().totalLearnedProfiles > 0) {
        ai.clearSelfLearningMemory();
        aiLog = "AI Memory Cleared.";
      } else {
        aiLog = "Self-Learning: Capture & Analyze sinyal aktif dulu di tab "
                "QUICK AI.";
      }
    }
    repaint();
  }

  void setTargetChannel(int chIdx, const juce::String &chName) {
    activeChannelIndex = chIdx;
    channelName = chName;
    analyzed = false;
    captureProgress = 0.0f;
    currentRec = {};
    preAiSnapshot = {};
    aiLog = "Target: " + channelName + " (" +
            juce::String(
                nova::graviton::aiModeToString(engine.channelAI.getMode())) +
            ")";

    startTimerHz(20);
    repaint();
  }

  void applyCurrentModuleSetting() {
    if (activeChannelIndex < 0 ||
        activeChannelIndex >= static_cast<int>(engine.channels.size()))
      return;

    if (currentModuleTab != 7 &&
        (!currentRec.valid || currentRec.insufficientData)) {
      aiLog = "Cannot apply: profil belum valid atau sinyal terlalu pelan. "
              "Jalankan Capture terlebih dahulu.";
      repaint();
      return;
    }

    if (!preAiSnapshot.valid)
      preAiSnapshot = engine.captureChannelAISnapshot(activeChannelIndex);

    switch (currentModuleTab) {
    case 0: // AI Gain
      engine.applyAIRecommendation(activeChannelIndex, currentRec, 3);
      aiLog =
          "Applied AI Gain: Preamp trim clamped to " +
          juce::String(engine.channels[static_cast<size_t>(activeChannelIndex)]
                           .inputGainDb.load(std::memory_order_relaxed),
                       1) +
          " dB";
      break;
    case 1: // AI EQ
      engine.applyAIRecommendation(activeChannelIndex, currentRec, 1);
      aiLog = "Applied AI EQ: Optimized parametric response for " +
              juce::String(nova::graviton::instrumentToString(
                  currentRec.detectedInstrument));
      break;
    case 2: // AI Compressor
      engine.applyAIRecommendation(activeChannelIndex, currentRec, 2);
      aiLog = "Applied AI Compressor: Threshold " +
              juce::String(currentRec.compThresholdDb, 1) + " dB, Ratio " +
              juce::String(currentRec.compRatio, 1) + ":1";
      break;
    case 3: // AI Gate
      engine.applyAIRecommendation(activeChannelIndex, currentRec, 2);
      aiLog = "Applied AI Gate: Threshold " +
              juce::String(currentRec.gateThresholdDb, 1) + " dB (Anti-Bleed)";
      break;
    case 4: // AI FX
      engine.applyAIRecommendation(activeChannelIndex, currentRec, 4);
      aiLog = "Applied AI FX: Aux 5-6 Reverb/Delay send set to " +
              juce::String(currentRec.fxSendDb, 1) + " dB";
      break;
    case 5: // AI Monitor
      engine.applyAIRecommendation(activeChannelIndex, currentRec, 5);
      aiLog =
          "Applied AI Monitor: Balanced Aux 1-4 sends for stage wedge and IEMs";
      break;
    case 6: // AI Feedback
      if (engine.applyAIRecommendation(activeChannelIndex, currentRec, 6)) {
        aiLog = "Applied AI Feedback Notch @ " +
                juce::String(currentRec.afsFreqHz, 0) + " Hz (-12 dB, Q 9.0)";
      } else {
        aiLog = "AI Feedback: Tidak ada notch aktif pada profil instrumen ini.";
      }
      break;
    case 7: // AI Master
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(currentRec.masterCeilingDb,
                                             std::memory_order_relaxed);
      aiLog = "Applied AI Master: True-Peak limiter ceiling set to " +
              juce::String(currentRec.masterCeilingDb, 2) + " dBFS";
      break;
    }

    if (onChannelUpdated)
      onChannelUpdated();
    repaint();
  }

  void applyAllModulesSetting() {
    if (activeChannelIndex < 0 ||
        activeChannelIndex >= static_cast<int>(engine.channels.size()))
      return;
    if (!currentRec.valid || currentRec.insufficientData) {
      aiLog =
          "Cannot apply ALL: profil belum valid atau data audio tidak cukup.";
      repaint();
      return;
    }

    if (!preAiSnapshot.valid)
      preAiSnapshot = engine.captureChannelAISnapshot(activeChannelIndex);

    engine.applyAIRecommendation(activeChannelIndex, currentRec, 0);
    if (onApplyAiProfile) {
      onApplyAiProfile(activeChannelIndex, currentRec, 0);
    }
    engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
    engine.masterBusLimiterCeilingDb.store(-0.1, std::memory_order_relaxed);

    if (onChannelUpdated)
      onChannelUpdated();
    aiLog = "Applied ALL AI MODULES to " + channelName + " (" +
            juce::String(nova::graviton::instrumentToString(
                currentRec.detectedInstrument)) +
            ")";
    repaint();
  }

  void restoreNominalMix() {
    if (activeChannelIndex >= 0 &&
        activeChannelIndex < static_cast<int>(engine.channels.size())) {
      if (preAiSnapshot.valid &&
          engine.restoreChannelAISnapshot(activeChannelIndex, preAiSnapshot)) {
        preAiSnapshot.valid = false;
        aiLog = "Restored pre-AI mix state on " + channelName;
      } else {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.inputGainDb.store(0.0f, std::memory_order_relaxed);
        ch.gainDb.store(0.0f, std::memory_order_relaxed);
        ch.highPassEnabled.store(false, std::memory_order_relaxed);
        for (size_t b = 0; b < 4; ++b) {
          ch.eqGainDb[b].store(0.0f, std::memory_order_relaxed);
        }
        ch.compressorEnabled.store(false, std::memory_order_relaxed);
        ch.compressorThresholdDb.store(-18.0f, std::memory_order_relaxed);
        ch.compressorRatio.store(3.0f, std::memory_order_relaxed);
        ch.gateEnabled.store(false, std::memory_order_relaxed);
        aiLog = "Restored safe nominal mix on " + channelName;
      }
      if (onChannelUpdated)
        onChannelUpdated();
      repaint();
    }
  }

  void timerCallback() override {
    const float dt = 0.05f;
    const bool matchingTarget =
        (engine.channelAI.getTargetChannel() == activeChannelIndex);

    // 1. Acoustic Profiling Capture Progress (Strictly real captured samples)
    if (matchingTarget && engine.channelAI.isCapturing()) {
      captureProgress = engine.channelAI.getProgress();
      aiLog = "Capturing audio signal: " +
              juce::String(static_cast<int>(captureProgress * 100.0f)) + "%";
      repaint();
    } else if (matchingTarget && engine.channelAI.isCompleted() && !analyzed) {
      captureProgress = 1.0f;
      currentRec = engine.channelAI.runProfileOnCaptured(
          engine.sampleRate.load(std::memory_order_relaxed));
      analyzed = currentRec.valid && !currentRec.insufficientData;
      if (!analyzed) {
        aiLog = "Data tidak cukup / sinyal terlalu pelan (<= -65 dBFS). "
                "Ulangi capture saat sinyal aktif.";
      } else {
        aiLog = "Acoustic profiling complete: " +
                juce::String(nova::graviton::instrumentToString(
                    currentRec.detectedInstrument)) +
                " (" +
                juce::String(static_cast<int>(currentRec.confidence * 100.0f)) +
                "%)" + (currentRec.clipped ? " [CLIPPING]" : "");
      }
      repaint();
    }

    // 2. AI Autonomous Mixer Auto-Pilot Loop (Requires valid analyzed profile
    // for the active channel + respects Emergency Freeze & AISafetyLimits)
    if (engine.channelAI.getMode() == nova::graviton::AIMode::AutonomousMixer &&
        !engine.channelAI.isEmergencyFrozen() && analyzed && currentRec.valid &&
        !currentRec.insufficientData &&
        currentRec.sourceChannel == activeChannelIndex) {
      if (activeChannelIndex >= 0 &&
          activeChannelIndex < static_cast<int>(engine.channels.size())) {
        if (!preAiSnapshot.valid)
          preAiSnapshot = engine.captureChannelAISnapshot(activeChannelIndex);
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        const auto &limits = engine.channelAI.getSafetyLimits();
        bool changed = false;

        // Modul 1: AI Gain
        if (engine.channelAI.isModuleEnabled(
                nova::graviton::AIModuleId::Gain)) {
          const float curTrim = ch.inputGainDb.load(std::memory_order_relaxed);
          const float targetTrim =
              std::clamp(currentRec.targetTrimDb, limits.minInputTrimDb,
                         limits.maxInputTrimDb);
          const float slewedTrim = nova::graviton::smoothSlew(
              curTrim, targetTrim, limits.maxTrimRateDbPerSec * dt);
          if (std::abs(curTrim - slewedTrim) > 0.01f) {
            ch.inputGainDb.store(slewedTrim, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 2: AI EQ
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::EQ)) {
          if (!ch.eqEnabled.load(std::memory_order_relaxed))
            ch.eqEnabled.store(true, std::memory_order_relaxed);
          for (size_t b = 0; b < 4; ++b) {
            const float curG = ch.eqGainDb[b].load(std::memory_order_relaxed);
            const float targetG = std::clamp(
                currentRec.eqGainDb[b], limits.minEqGainDb, limits.maxEqGainDb);
            const float slewedG = nova::graviton::smoothSlew(
                curG, targetG, limits.maxEqRateDbPerSec * dt);
            if (std::abs(curG - slewedG) > 0.01f) {
              ch.eqGainDb[b].store(slewedG, std::memory_order_relaxed);
              changed = true;
            }
          }
        }

        // Modul 3: AI Compressor
        if (engine.channelAI.isModuleEnabled(
                nova::graviton::AIModuleId::Compressor)) {
          if (!ch.compressorEnabled.load(std::memory_order_relaxed))
            ch.compressorEnabled.store(true, std::memory_order_relaxed);
          ch.compressorMode.store(std::clamp(currentRec.compMode, 0, 4),
                                  std::memory_order_relaxed);
          const float curThresh =
              ch.compressorThresholdDb.load(std::memory_order_relaxed);
          const float targetThresh =
              std::clamp(currentRec.compThresholdDb, limits.minCompThresholdDb,
                         limits.maxCompThresholdDb);
          const float slewedThresh = nova::graviton::smoothSlew(
              curThresh, targetThresh, limits.maxCompThreshRateDbPerSec * dt);
          if (std::abs(curThresh - slewedThresh) > 0.01f) {
            ch.compressorThresholdDb.store(slewedThresh,
                                           std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 4: AI Gate
        if (engine.channelAI.isModuleEnabled(
                nova::graviton::AIModuleId::Gate)) {
          if (currentRec.gateEnabled &&
              !ch.gateEnabled.load(std::memory_order_relaxed)) {
            ch.gateEnabled.store(true, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 8: AI Master
        if (engine.channelAI.isModuleEnabled(
                nova::graviton::AIModuleId::Master)) {
          if (!engine.masterBusLimiterEnabled.load(std::memory_order_relaxed)) {
            engine.masterBusLimiterEnabled.store(true,
                                                 std::memory_order_relaxed);
            engine.masterBusLimiterCeilingDb.store(-0.1,
                                                   std::memory_order_relaxed);
          }
        }

        if (changed && onChannelUpdated) {
          onChannelUpdated();
          repaint();
        }
      }
    }
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (!isEmbedded) {
      g.fillAll(juce::Colours::black.withAlpha(0.85f));
    }

    const float dialogW = isEmbedded ? (bounds.getWidth() - 8.0f) : 680.0f;
    const float dialogH = isEmbedded ? (bounds.getHeight() - 6.0f) : 500.0f;
    auto dBox =
        isEmbedded
            ? juce::Rectangle<float>(4.0f, 3.0f, dialogW, dialogH)
            : juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                     (bounds.getHeight() - dialogH) * 0.5f,
                                     dialogW, dialogH);

    // Chassis
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(dBox, 6.0f);
    g.setColour(midas_colors::kAccentGreen);
    g.drawRoundedRectangle(dBox, 6.0f, 1.2f);

    // Header
    g.setFont(midas_ui::getFont(12.0f, true));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(
        "MIDAS HERITAGE-D AI INTELLIGENCE SYSTEM (DUAL-MODE & ADVANCED SUITE)",
        static_cast<int>(dBox.getX() + 16.0f),
        static_cast<int>(dBox.getY() + 8.0f), 550, 18,
        juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TARGET: " + channelName + " (CH " +
                   juce::String(activeChannelIndex + 1) + ")",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + (isEmbedded ? 24.0f : 28.0f)),
               260, 15, juce::Justification::centredLeft);

    g.setColour(midas_colors::kAccentGold);
    g.drawText("PROTECTION: Slew Limit <= 1.0 dB/s | Clamping Active | "
               "Glitch-Free Safe",
               static_cast<int>(dBox.getX() + 260.0f),
               static_cast<int>(dBox.getY() + (isEmbedded ? 24.0f : 28.0f)),
               380, 15, juce::Justification::centredRight);

    const float barX = dBox.getX() + (isEmbedded ? 12.0f : 20.0f);
    const float barW = dialogW - (isEmbedded ? 24.0f : 40.0f);

    // Card Area
    const float cardY =
        isEmbedded ? (dBox.getY() + 68.0f) : (dBox.getY() + 106.0f);
    const float cardH = isEmbedded ? (dialogH - 134.0f) : 138.0f;
    auto cardBox = juce::Rectangle<float>(barX, cardY, barW, cardH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(cardBox, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(cardBox, 6.0f, 1.0f);

    if (!isAdvancedSuite) {
      g.setFont(midas_ui::getFont(10.5f, true));
      g.setColour(midas_colors::kAccentGreen);
      juce::String headerLine = "QUICK AI: ";
      if (!analyzed) {
        headerLine +=
            "Klik [1. CAPTURE (4S)] saat audio aktif pada " + channelName;
      } else {
        headerLine +=
            juce::String(nova::graviton::instrumentToString(
                currentRec.detectedInstrument)) +
            " (" +
            juce::String(static_cast<int>(currentRec.confidence * 100.0f)) +
            "%)" + (currentRec.clipped ? " [CLIPPING!]" : "");
      }
      g.drawText(headerLine, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 8.0f),
                 static_cast<int>(barW - 24.0f), 16,
                 juce::Justification::centredLeft);

      g.setFont(midas_ui::getFont(9.5f, false));
      g.setColour(midas_colors::kTextWhite);
      juce::String line2 =
          analyzed
              ? ("Gain Trim: " + juce::String(currentRec.targetTrimDb, 1) +
                 " dB | HPF: " +
                 juce::String(currentRec.hpfEnabled
                                  ? juce::String(currentRec.hpfFreqHz, 0) +
                                        " Hz"
                                  : "OFF") +
                 " | EQ Bands: " + juce::String(currentRec.eqGainDb[0], 1) +
                 " / " + juce::String(currentRec.eqGainDb[1], 1) + " / " +
                 juce::String(currentRec.eqGainDb[2], 1) + " / " +
                 juce::String(currentRec.eqGainDb[3], 1) + " dB")
              : "Langkah: 1) Capture 4 detik  ->  2) Apply AI  ->  3) Undo / "
                "Restore jika perlu.";
      g.drawText(line2, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 30.0f),
                 static_cast<int>(barW - 24.0f), 18,
                 juce::Justification::centredLeft);

      g.setFont(midas_ui::getFont(9.0f, true));
      g.setColour(midas_colors::kAccentGold);
      juce::String line3 =
          analyzed
              ? ("Comp Thresh: " + juce::String(currentRec.compThresholdDb, 1) +
                 " dB (" + juce::String(currentRec.compRatio, 1) +
                 ":1) | Gate: " + juce::String(currentRec.gateThresholdDb, 1) +
                 " dB | FX Send: " + juce::String(currentRec.fxSendDb, 1) +
                 " dB")
              : "Mode Assist: Operator memegang kendali penuh | Mode "
                "Auto-Pilot: "
                "Penyesuaian bertahap (Slew <= 1.0 dB/s).";
      g.drawText(line3, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 52.0f),
                 static_cast<int>(barW - 24.0f), 18,
                 juce::Justification::centredLeft);
    } else {
      const auto &sc = engine.channelAI.getSoundcheckResult();
      const auto &st = engine.channelAI.getStageData();
      const auto &ac = engine.channelAI.getAcousticData();
      const auto &mm = engine.channelAI.getMultiMicData();
      const auto &mem = engine.channelAI.getSelfLearningMemory();

      g.setFont(midas_ui::getFont(10.5f, true));
      g.setColour(midas_colors::kAccentGreen);
      g.drawText("LIVE CONSOLE TOOLS (1-CLICK ACTIONS)",
                 static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 8.0f),
                 static_cast<int>(barW - 24.0f), 16,
                 juce::Justification::centredLeft);

      g.setFont(midas_ui::getFont(9.2f, false));
      g.setColour(midas_colors::kTextWhite);
      g.drawText("Soundcheck: " + juce::String(sc.auditedChannels) +
                     " Ch | IEM Aux 3/4 Limiter: " +
                     juce::String(st.hearingProtectionLimiter ? "ON (-6 dBFS)"
                                                              : "OFF") +
                     " | Master GEQ: " +
                     juce::String(ac.geqFlattened ? "Calibrated" : "Idle"),
                 static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 30.0f),
                 static_cast<int>(barW - 24.0f), 18,
                 juce::Justification::centredLeft);

      g.setFont(midas_ui::getFont(9.0f, true));
      g.setColour(midas_colors::kAccentGold);
      g.drawText(
          "Unmask: " +
              juce::String(
                  (mm.leadVoxUnmasked || mm.kickBassUnmasked) ? "ON" : "OFF") +
              " | Scene: " +
              juce::String(nova::graviton::autoSceneStyleToString(
                  engine.channelAI.getAutoSceneStyle())) +
              " | Learned: " + juce::String(mem.totalLearnedProfiles),
          static_cast<int>(cardBox.getX() + 12.0f),
          static_cast<int>(cardBox.getY() + 52.0f),
          static_cast<int>(barW - 24.0f), 18, juce::Justification::centredLeft);
    }

    // Capture Progress Bar
    if (captureProgress > 0.001f && captureProgress < 1.0f) {
      const float pW = barW - 24.0f;
      const float pY = cardBox.getY() + cardH - 12.0f;
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(cardBox.getX() + 12.0f, pY, pW, 8.0f, 2.0f);
      g.setColour(midas_colors::kAccentCyan);
      g.fillRoundedRectangle(cardBox.getX() + 13.0f, pY + 1.0f,
                             (pW - 2.0f) * captureProgress, 6.0f, 2.0f);
    }

    // Telemetry Footer
    auto statBox =
        isEmbedded ? juce::Rectangle<float>(barX, dBox.getY() + dialogH - 28.0f,
                                            barW, 24.0f)
                   : juce::Rectangle<float>(barX, dBox.getY() + dialogH - 46.0f,
                                            barW, 34.0f);
    g.setColour(juce::Colour(0xff12171d));
    g.fillRoundedRectangle(statBox, 4.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(statBox, 4.0f, 1.0f);

    g.setFont(midas_ui::getFont(9.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("AI TELEMETRY: " + aiLog, statBox.toNearestInt(),
               juce::Justification::centred);
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    if (isEmbedded) {
      btnClose.setVisible(false);
      const float dx = 4.0f;
      const float dy = 3.0f;
      const float dialogW = bounds.getWidth() - 8.0f;
      const float dialogH = bounds.getHeight() - 6.0f;

      // 1. Two Intelligence Mode Buttons & Suite Switcher (One Row)
      const float topBtnW = (dialogW - 24.0f - 3.0f * 6.0f) / 4.0f;
      btnModeSoundEngineer.setBounds(
          static_cast<int>(dx + 12.0f + 0 * (topBtnW + 6.0f)),
          static_cast<int>(dy + 40.0f), static_cast<int>(topBtnW), 22);
      btnModeAutonomous.setBounds(
          static_cast<int>(dx + 12.0f + 1 * (topBtnW + 6.0f)),
          static_cast<int>(dy + 40.0f), static_cast<int>(topBtnW), 22);
      btnSuiteCore.setBounds(
          static_cast<int>(dx + 12.0f + 2 * (topBtnW + 6.0f)),
          static_cast<int>(dy + 40.0f), static_cast<int>(topBtnW), 22);
      btnSuiteAdvanced.setBounds(
          static_cast<int>(dx + 12.0f + 3 * (topBtnW + 6.0f)),
          static_cast<int>(dy + 40.0f), static_cast<int>(topBtnW), 22);

      if (!isAdvancedSuite) {
        // Quick AI: 3 clear action buttons at the bottom
        const float actW = (dialogW - 24.0f - 2.0f * 8.0f) / 3.0f;
        const float actY = dy + dialogH - 60.0f;
        btnStartCapture.setBounds(
            static_cast<int>(dx + 12.0f + 0 * (actW + 8.0f)),
            static_cast<int>(actY), static_cast<int>(actW), 28);
        btnApplyAllModules.setBounds(
            static_cast<int>(dx + 12.0f + 1 * (actW + 8.0f)),
            static_cast<int>(actY), static_cast<int>(actW), 28);
        btnEmergencyFreeze.setBounds(
            static_cast<int>(dx + 12.0f + 1 * (actW + 8.0f)),
            static_cast<int>(actY), static_cast<int>(actW), 28);
        btnResetNominal.setBounds(
            static_cast<int>(dx + 12.0f + 2 * (actW + 8.0f)),
            static_cast<int>(actY), static_cast<int>(actW), 28);
      } else {
        // Live Tools: 6 direct 1-click buttons (2 rows of 3)
        const float actW = (dialogW - 24.0f - 2.0f * 8.0f) / 3.0f;
        const float row1Y = dy + dialogH - 88.0f;
        const float row2Y = dy + dialogH - 58.0f;
        btnAdvAction1.setBounds(
            static_cast<int>(dx + 12.0f + 0 * (actW + 8.0f)),
            static_cast<int>(row1Y), static_cast<int>(actW), 25);
        btnAdvAction2.setBounds(
            static_cast<int>(dx + 12.0f + 1 * (actW + 8.0f)),
            static_cast<int>(row1Y), static_cast<int>(actW), 25);
        btnAdvAction3.setBounds(
            static_cast<int>(dx + 12.0f + 2 * (actW + 8.0f)),
            static_cast<int>(row1Y), static_cast<int>(actW), 25);

        btnTabSelfLearning.setBounds(
            static_cast<int>(dx + 12.0f + 0 * (actW + 8.0f)),
            static_cast<int>(row2Y), static_cast<int>(actW), 25);
        btnTabAutoSoundcheck.setBounds(
            static_cast<int>(dx + 12.0f + 1 * (actW + 8.0f)),
            static_cast<int>(row2Y), static_cast<int>(actW), 25);
        btnTabAutoScene.setBounds(
            static_cast<int>(dx + 12.0f + 2 * (actW + 8.0f)),
            static_cast<int>(row2Y), static_cast<int>(actW), 25);
      }
      return;
    }

    const float dialogW = 680.0f;
    const float dialogH = 500.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f),
                       static_cast<int>(dy + 8.0f), 24, 24);

    // Top Bar: Mode & View Switcher (1 clean row of 4)
    const float topBtnW = (dialogW - 40.0f - 3.0f * 8.0f) / 4.0f;
    btnModeSoundEngineer.setBounds(
        static_cast<int>(dx + 20.0f + 0 * (topBtnW + 8.0f)),
        static_cast<int>(dy + 52.0f), static_cast<int>(topBtnW), 28);
    btnModeAutonomous.setBounds(
        static_cast<int>(dx + 20.0f + 1 * (topBtnW + 8.0f)),
        static_cast<int>(dy + 52.0f), static_cast<int>(topBtnW), 28);
    btnSuiteCore.setBounds(static_cast<int>(dx + 20.0f + 2 * (topBtnW + 8.0f)),
                           static_cast<int>(dy + 52.0f),
                           static_cast<int>(topBtnW), 28);
    btnSuiteAdvanced.setBounds(
        static_cast<int>(dx + 20.0f + 3 * (topBtnW + 8.0f)),
        static_cast<int>(dy + 52.0f), static_cast<int>(topBtnW), 28);

    if (!isAdvancedSuite) {
      const float actW = (dialogW - 40.0f - 16.0f) / 3.0f;
      btnStartCapture.setBounds(
          static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)),
          static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnApplyAllModules.setBounds(
          static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)),
          static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnEmergencyFreeze.setBounds(
          static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)),
          static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnResetNominal.setBounds(
          static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)),
          static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
    } else {
      const float actW = (dialogW - 40.0f - 16.0f) / 3.0f;
      btnAdvAction1.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)),
                              static_cast<int>(dy + 260.0f),
                              static_cast<int>(actW), 34);
      btnAdvAction2.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)),
                              static_cast<int>(dy + 260.0f),
                              static_cast<int>(actW), 34);
      btnAdvAction3.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)),
                              static_cast<int>(dy + 260.0f),
                              static_cast<int>(actW), 34);

      btnTabSelfLearning.setBounds(
          static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)),
          static_cast<int>(dy + 302.0f), static_cast<int>(actW), 34);
      btnTabAutoSoundcheck.setBounds(
          static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)),
          static_cast<int>(dy + 302.0f), static_cast<int>(actW), 34);
      btnTabAutoScene.setBounds(
          static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)),
          static_cast<int>(dy + 302.0f), static_cast<int>(actW), 34);
    }
  }

private:
  MixerEngine &engine;
  int activeChannelIndex = 0;
  juce::String channelName = "IN 01 (KICK IN)";
  float captureProgress = 0.0f;
  bool analyzed = false;
  MixerEngine::ChannelAISnapshot preAiSnapshot;

  bool isAdvancedSuite = false;
  int currentModuleTab = 0;
  int currentAdvancedTab = 0;

  nova::graviton::AIRecommendation currentRec;
  juce::String aiLog = "MIDAS AI Dual-Mode System Ready";

  MidasTactileButton btnClose;

  // 1. Two Intelligence Mode Buttons
  MidasTactileButton btnModeSoundEngineer, btnModeAutonomous;

  // Suite Switcher Buttons
  MidasTactileButton btnSuiteCore, btnSuiteAdvanced;

  // 2. Eight Core Module Tabs
  MidasTactileButton btnTabGain, btnTabEq, btnTabComp, btnTabGate, btnTabFx,
      btnTabMonitor, btnTabFeedback, btnTabMaster;

  // 3. Six Advanced Feature Tabs
  MidasTactileButton btnTabSelfLearning, btnTabAutoSoundcheck, btnTabAutoScene,
      btnTabMultiMic, btnTabStageIntel, btnTabAcousticIntel;

  // Core Advisory Mode Buttons
  MidasTactileButton btnApplyModule, btnApplyAllModules, btnStartCapture;

  // Core Autonomous Mode Buttons
  MidasTactileButton btnToggleModuleAutonomous, btnEmergencyFreeze,
      btnResetNominal;

  // Advanced Feature Action Buttons
  MidasTactileButton btnAdvAction1, btnAdvAction2, btnAdvAction3;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasAiModal)
};

// ==============================================================================
// MODAL: mCLOUD SHOWFILE SYNC & CLOUD PRESETS
// ==============================================================================
class MidasMCloudModal : public juce::Component {
public:
  void setEmbedded(bool embedded) {
    isEmbedded = embedded;
    btnClose.setVisible(!embedded);
    setAlwaysOnTop(!embedded);
    resized();
    repaint();
  }
  bool isEmbedded = false;

  MidasMCloudModal()
      : btnClose("X", MidasTactileButton::GlowMode::Red),
        btnSyncNow("SYNC SHOWFILE NOW", MidasTactileButton::GlowMode::Cyan),
        btnExportBackup("EXPORT OFFLINE BACKUP",
                        MidasTactileButton::GlowMode::Gold) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);
    addAndMakeVisible(btnSyncNow);
    addAndMakeVisible(btnExportBackup);

    btnClose.onClick = [this] { setVisible(false); };
    btnSyncNow.onClick = [this] {
      if (onShowfileExport)
        onShowfileExport(false);
      if (isEmbedded) {
        statusBanner = "SYNC COMPLETE: Arena_Tour_2026_Main.m96";
        repaint();
      } else {
        setVisible(false);
      }
    };
    btnExportBackup.onClick = [this] {
      if (onShowfileExport)
        onShowfileExport(true);
      if (isEmbedded) {
        statusBanner = "OFFLINE BACKUP SAVED TO SHOW DIRECTORY";
        repaint();
      } else {
        setVisible(false);
      }
    };
  }

  std::function<void(bool isBackup)> onShowfileExport;

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (!isEmbedded) {
      g.fillAll(juce::Colours::black.withAlpha(0.75f));
    } else {
      g.fillAll(juce::Colour(0xff0b111a));
    }

    const float dialogW =
        isEmbedded ? (bounds.getWidth() - 8.0f) : 460.0f;
    const float dialogH =
        isEmbedded ? (bounds.getHeight() - 6.0f) : 240.0f;
    auto dBox =
        isEmbedded
            ? juce::Rectangle<float>(4.0f, 3.0f, dialogW, dialogH)
            : juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                     (bounds.getHeight() - dialogH) * 0.5f,
                                     dialogW, dialogH);

    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(dBox, 8.0f);
    g.setColour(midas_colors::kAccentCyan);
    g.drawRoundedRectangle(dBox, 8.0f, 1.2f);

    g.setFont(midas_ui::getFont(13.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("mCLOUD CONSOLE SYNC & SHOW ARCHIVE",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 14.0f), 420, 20,
               juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(10.0f, false));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(statusBanner.isNotEmpty()
                   ? statusBanner
                   : "CONNECTED: London Fast CDN Node - Latency: 14ms",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 44.0f),
               static_cast<int>(dialogW - 32.0f), 18,
               juce::Justification::centredLeft);

    g.setColour(midas_colors::kTextWhite);
    g.drawText("Active Showfile: Arena_Tour_2026_Main.m96",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 68.0f),
               static_cast<int>(dialogW - 32.0f), 18,
               juce::Justification::centredLeft);
    g.drawText("Multi-Console Mirroring: ENABLED (Mirror FOH to MON Console)",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 92.0f),
               static_cast<int>(dialogW - 32.0f), 18,
               juce::Justification::centredLeft);
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    if (isEmbedded) {
      btnClose.setVisible(false);
      const float dialogW = bounds.getWidth() - 8.0f;
      const float dialogH = bounds.getHeight() - 6.0f;
      const float dx = 4.0f;
      const float dy = 3.0f;
      btnSyncNow.setBounds(static_cast<int>(dx + 16.0f),
                           static_cast<int>(dy + dialogH - 44.0f), 190, 30);
      btnExportBackup.setBounds(static_cast<int>(dx + 218.0f),
                                static_cast<int>(dy + dialogH - 44.0f), 210, 30);
      return;
    }

    btnClose.setVisible(true);
    const float dialogW = 460.0f;
    const float dialogH = 240.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f),
                       static_cast<int>(dy + 8.0f), 24, 24);
    btnSyncNow.setBounds(static_cast<int>(dx + 20.0f),
                         static_cast<int>(dy + dialogH - 46.0f), 190, 28);
    btnExportBackup.setBounds(static_cast<int>(dx + 220.0f),
                              static_cast<int>(dy + dialogH - 46.0f), 210, 28);
  }

private:
  juce::String statusBanner;
  MidasTactileButton btnClose, btnSyncNow, btnExportBackup;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasMCloudModal)
};

// ==============================================================================
// HERITAGE VIRTUAL CABLE & VBAN EMBEDDED VIEW
// ==============================================================================
class MidasVirtualCableView : public juce::Component, private juce::Timer {
public:
  explicit MidasVirtualCableView(MixerEngine &eng)
      : engine(eng),
        btnToggleVban("START VBAN (6980)", MidasTactileButton::GlowMode::Cyan),
        btnToggleShm("START WIN SHM", MidasTactileButton::GlowMode::Gold),
        btnBackend("MODE: BOTH", MidasTactileButton::GlowMode::Purple),
        btnApplyTarget("APPLY VBAN TARGET", MidasTactileButton::GlowMode::Green) {
    addAndMakeVisible(btnToggleVban);
    addAndMakeVisible(btnToggleShm);
    addAndMakeVisible(btnBackend);
    addAndMakeVisible(btnApplyTarget);

    btnToggleVban.setClickingTogglesState(false);
    btnToggleShm.setClickingTogglesState(false);
    btnBackend.setClickingTogglesState(false);
    btnApplyTarget.setClickingTogglesState(false);

    for (size_t k = 0; k < kNumCables; ++k) {
      auto &row = rows[k];
      row.btnSelect = std::make_unique<MidasTactileButton>(
          "CABLE " + juce::String(k * 2 + 1) + "-" + juce::String(k * 2 + 2),
          MidasTactileButton::GlowMode::Gold);
      row.btnRetEn = std::make_unique<MidasTactileButton>(
          "RET: OFF", MidasTactileButton::GlowMode::Cyan);
      row.btnRetCh = std::make_unique<MidasTactileButton>(
          "CH " + juce::String(k * 2 + 1) + "-" + juce::String(k * 2 + 2),
          MidasTactileButton::GlowMode::Cyan);
      row.btnRetMute = std::make_unique<MidasTactileButton>(
          "M", MidasTactileButton::GlowMode::Red);
      row.btnSndEn = std::make_unique<MidasTactileButton>(
          "SND: OFF", MidasTactileButton::GlowMode::Green);
      row.btnSndSrc = std::make_unique<MidasTactileButton>(
          "MAIN L/R", MidasTactileButton::GlowMode::Green);
      row.btnSndMute = std::make_unique<MidasTactileButton>(
          "M", MidasTactileButton::GlowMode::Red);

      for (auto *b : {row.btnSelect.get(), row.btnRetEn.get(),
                      row.btnRetCh.get(), row.btnRetMute.get(),
                      row.btnSndEn.get(), row.btnSndSrc.get(),
                      row.btnSndMute.get()}) {
        b->setClickingTogglesState(false);
        addAndMakeVisible(b);
      }

      row.btnSelect->onClick = [this, k] {
        selectedCable = k;
        loadSelectedTargetFields();
        repaint();
      };
      row.btnRetEn->onClick = [this, k] {
        auto &vc = engine.virtualTransport.cables[k];
        const bool next = !vc.returnEnabled.load(std::memory_order_relaxed);
        vc.returnEnabled.store(next, std::memory_order_relaxed);
        if (next) {
          int chL = vc.returnTargetChannelL.load(std::memory_order_relaxed);
          if (chL < 0) {
            chL = static_cast<int>(k * 2);
            vc.returnTargetChannelL.store(chL, std::memory_order_relaxed);
          }
          if (vc.returnTargetChannelR.load(std::memory_order_relaxed) < 0 &&
              chL + 1 < nova::kLogicalChannels) {
            vc.returnTargetChannelR.store(chL + 1, std::memory_order_relaxed);
          }
        }
        vc.updateLoopGuard();
        refreshButtonLabels();
      };
      row.btnRetCh->onClick = [this, k] {
        auto &vc = engine.virtualTransport.cables[k];
        const int curL = vc.returnTargetChannelL.load(std::memory_order_relaxed);
        const int curPair = (curL < 0) ? 0 : (curL / 2);
        const int nextPair = (curL < 0 || curPair >= 7) ? 0 : (curPair + 1);
        vc.returnTargetChannelL.store(nextPair * 2, std::memory_order_relaxed);
        vc.returnTargetChannelR.store(nextPair * 2 + 1,
                                      std::memory_order_relaxed);
        vc.updateLoopGuard();
        refreshButtonLabels();
      };
      row.btnRetMute->onClick = [this, k] {
        auto &vc = engine.virtualTransport.cables[k];
        vc.returnMuted.store(!vc.returnMuted.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
        refreshButtonLabels();
      };
      row.btnSndEn->onClick = [this, k] {
        auto &vc = engine.virtualTransport.cables[k];
        const bool next = !vc.sendEnabled.load(std::memory_order_relaxed);
        vc.sendEnabled.store(next, std::memory_order_relaxed);
        if (next &&
            vc.sendSourceType.load(std::memory_order_relaxed) ==
                static_cast<int>(nova::graviton::OutputBusSourceType::Unassigned)) {
          vc.sendSourceType.store(
              static_cast<int>(nova::graviton::OutputBusSourceType::MainLeft),
              std::memory_order_relaxed);
        }
        vc.updateLoopGuard();
        refreshButtonLabels();
      };
      row.btnSndSrc->onClick = [this, k] {
        auto &vc = engine.virtualTransport.cables[k];
        const auto curType = static_cast<nova::graviton::OutputBusSourceType>(
            vc.sendSourceType.load(std::memory_order_relaxed));
        if (curType == nova::graviton::OutputBusSourceType::MainLeft ||
            curType == nova::graviton::OutputBusSourceType::Unassigned) {
          vc.sendSourceType.store(
              static_cast<int>(nova::graviton::OutputBusSourceType::AuxBus),
              std::memory_order_relaxed);
          vc.sendSourceIndex.store(0, std::memory_order_relaxed);
        } else if (curType == nova::graviton::OutputBusSourceType::AuxBus) {
          vc.sendSourceType.store(
              static_cast<int>(nova::graviton::OutputBusSourceType::MatrixBus),
              std::memory_order_relaxed);
          vc.sendSourceIndex.store(0, std::memory_order_relaxed);
        } else if (curType == nova::graviton::OutputBusSourceType::MatrixBus) {
          vc.sendSourceType.store(
              static_cast<int>(nova::graviton::OutputBusSourceType::DirectOut),
              std::memory_order_relaxed);
          vc.sendSourceIndex.store(static_cast<int>(k * 2),
                                   std::memory_order_relaxed);
        } else {
          vc.sendSourceType.store(
              static_cast<int>(nova::graviton::OutputBusSourceType::MainLeft),
              std::memory_order_relaxed);
          vc.sendSourceIndex.store(0, std::memory_order_relaxed);
        }
        vc.updateLoopGuard();
        refreshButtonLabels();
      };
      row.btnSndMute->onClick = [this, k] {
        auto &vc = engine.virtualTransport.cables[k];
        vc.sendMuted.store(!vc.sendMuted.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
        refreshButtonLabels();
      };
    }

    auto styleEditor = [this](juce::TextEditor &ed) {
      ed.setColour(juce::TextEditor::backgroundColourId,
                   juce::Colour(0xff12171d));
      ed.setColour(juce::TextEditor::textColourId, midas_colors::kTextWhite);
      ed.setColour(juce::TextEditor::outlineColourId, midas_colors::kBorderCol);
      ed.setFont(midas_ui::getFont(10.0f, false));
      addAndMakeVisible(ed);
    };
    styleEditor(ipEditor);
    styleEditor(portEditor);
    styleEditor(retStreamEditor);
    styleEditor(sndStreamEditor);

    btnToggleVban.onClick = [this] {
      if (engine.virtualTransport.isNetworkRunning())
        engine.virtualTransport.stopNetwork();
      else
        engine.virtualTransport.startNetwork(6980);
      refreshButtonLabels();
    };
    btnToggleShm.onClick = [this] {
      if (engine.virtualTransport.isSharedMemoryOpen())
        engine.virtualTransport.stopSharedMemory();
      else
        engine.virtualTransport.startSharedMemory();
      refreshButtonLabels();
    };
    btnBackend.onClick = [this] {
      const auto cur = engine.virtualTransport.getTransportBackend();
      const auto next =
          (cur == nova::graviton::TransportBackend::Both)
              ? nova::graviton::TransportBackend::VbanUdp
              : ((cur == nova::graviton::TransportBackend::VbanUdp)
                     ? nova::graviton::TransportBackend::SharedMemory
                     : nova::graviton::TransportBackend::Both);
      engine.virtualTransport.setTransportBackend(next);
      refreshButtonLabels();
    };
    btnApplyTarget.onClick = [this] {
      auto &vc = engine.virtualTransport.cables[selectedCable];
      const int port = std::clamp(portEditor.getText().getIntValue(), 1, 65535);
      vc.setReturnStreamName(retStreamEditor.getText().trim());
      vc.setSendTarget(ipEditor.getText().trim(), port,
                       sndStreamEditor.getText().trim());
      statusNote = "Saved Cable " + juce::String(selectedCable * 2 + 1) + "-" +
                   juce::String(selectedCable * 2 + 2) + " VBAN Target";
      repaint();
    };

    loadSelectedTargetFields();
    refreshButtonLabels();
    startTimerHz(15);
  }

  ~MidasVirtualCableView() override { stopTimer(); }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat().reduced(4.0f, 3.0f);
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(midas_colors::kAccentCyan);
    g.drawRoundedRectangle(b, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(11.5f, true));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("HERITAGE VIRTUAL CABLE (VBAN UDP + WIN SHM 4x STEREO)",
               static_cast<int>(b.getX() + 12.0f),
               static_cast<int>(b.getY() + 6.0f), 460, 18,
               juce::Justification::centredLeft);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentGold);
    const juce::String netStat =
        "VBAN: " +
        juce::String(engine.virtualTransport.isNetworkRunning() ? "ON:6980"
                                                                : "OFF") +
        " | SHM: " +
        juce::String(engine.virtualTransport.isSharedMemoryOpen() ? "OPEN"
                                                                  : "CLOSED");
    g.drawText(netStat, static_cast<int>(b.getRight() - 230.0f),
               static_cast<int>(b.getY() + 6.0f), 218, 18,
               juce::Justification::centredRight);

    // Draw 4 cable row backgrounds, meters & telemetry
    const float rowStartY = b.getY() + 58.0f;
    const float rowH = 34.0f;
    for (size_t k = 0; k < kNumCables; ++k) {
      const float ry = rowStartY + static_cast<float>(k) * (rowH + 4.0f);
      auto rBox =
          juce::Rectangle<float>(b.getX() + 10.0f, ry, b.getWidth() - 20.0f, rowH);
      g.setColour(k == selectedCable ? juce::Colour(0xff1b2633)
                                     : juce::Colour(0xff141920));
      g.fillRoundedRectangle(rBox, 4.0f);
      g.setColour(k == selectedCable ? midas_colors::kAccentGold
                                     : midas_colors::kBorderCol);
      g.drawRoundedRectangle(rBox, 4.0f, 1.0f);

      const auto &vc = engine.virtualTransport.cables[k];
      // Return & Send Peak Bars at right side of row
      const float meterW = 54.0f;
      const float retPk =
          std::clamp(vc.returnPeakL.load(std::memory_order_relaxed), 0.0f, 1.0f);
      const float sndPk =
          std::clamp(vc.sendPeakL.load(std::memory_order_relaxed), 0.0f, 1.0f);

      const float mx = rBox.getRight() - 122.0f;
      g.setColour(juce::Colour(0xff0d1117));
      g.fillRect(mx, ry + 7.0f, meterW, 8.0f);
      g.fillRect(mx, ry + 19.0f, meterW, 8.0f);
      g.setColour(midas_colors::kAccentCyan);
      g.fillRect(mx, ry + 7.0f, meterW * retPk, 8.0f);
      g.setColour(midas_colors::kAccentGreen);
      g.fillRect(mx, ry + 19.0f, meterW * sndPk, 8.0f);

      // Loop Guard / RX-TX badge
      const bool blocked = vc.loopBlocked.load(std::memory_order_relaxed);
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(blocked ? midas_colors::kAccentRed : midas_colors::kTextMuted);
      g.drawText(
          blocked ? "LOOP BLOCK"
                  : ("RX:" +
                     juce::String(vc.returnPacketsReceived.load(
                         std::memory_order_relaxed)) +
                     " TX:" +
                     juce::String(
                         vc.sendPacketsSent.load(std::memory_order_relaxed))),
          static_cast<int>(mx + meterW + 4.0f), static_cast<int>(ry + 8.0f),
          60, 18, juce::Justification::centredLeft);
    }

    // Bottom status line
    g.setFont(midas_ui::getFont(9.0f, false));
    g.setColour(midas_colors::kTextMuted);
    g.drawText(statusNote, static_cast<int>(b.getX() + 12.0f),
               static_cast<int>(b.getBottom() - 20.0f),
               static_cast<int>(b.getWidth() - 24.0f), 16,
               juce::Justification::centredLeft);
  }

  void resized() override {
    const auto b = getLocalBounds().toFloat().reduced(4.0f, 3.0f);
    const float topW = (b.getWidth() - 20.0f - 12.0f) / 3.0f;
    btnToggleVban.setBounds(static_cast<int>(b.getX() + 10.0f),
                            static_cast<int>(b.getY() + 28.0f),
                            static_cast<int>(topW), 24);
    btnToggleShm.setBounds(
        static_cast<int>(b.getX() + 10.0f + topW + 6.0f),
        static_cast<int>(b.getY() + 28.0f), static_cast<int>(topW), 24);
    btnBackend.setBounds(
        static_cast<int>(b.getX() + 10.0f + 2.0f * (topW + 6.0f)),
        static_cast<int>(b.getY() + 28.0f), static_cast<int>(topW), 24);

    const float rowStartY = b.getY() + 58.0f;
    const float rowH = 34.0f;
    for (size_t k = 0; k < kNumCables; ++k) {
      const float ry = rowStartY + static_cast<float>(k) * (rowH + 4.0f) + 5.0f;
      float x = b.getX() + 16.0f;
      rows[k].btnSelect->setBounds(static_cast<int>(x), static_cast<int>(ry), 74, 24);
      x += 78.0f;
      rows[k].btnRetEn->setBounds(static_cast<int>(x), static_cast<int>(ry), 66, 24);
      x += 70.0f;
      rows[k].btnRetCh->setBounds(static_cast<int>(x), static_cast<int>(ry), 60, 24);
      x += 64.0f;
      rows[k].btnRetMute->setBounds(static_cast<int>(x), static_cast<int>(ry), 26, 24);
      x += 34.0f;
      rows[k].btnSndEn->setBounds(static_cast<int>(x), static_cast<int>(ry), 66, 24);
      x += 70.0f;
      rows[k].btnSndSrc->setBounds(static_cast<int>(x), static_cast<int>(ry), 78, 24);
      x += 82.0f;
      rows[k].btnSndMute->setBounds(static_cast<int>(x), static_cast<int>(ry), 26, 24);
    }

    const float edY = b.getBottom() - 48.0f;
    const float availW = b.getWidth() - 20.0f;
    const float ipW = availW * 0.22f;
    const float portW = availW * 0.11f;
    const float stW = availW * 0.21f;
    const float btnW = availW - ipW - portW - 2.0f * stW - 20.0f;
    float ex = b.getX() + 10.0f;
    ipEditor.setBounds(static_cast<int>(ex), static_cast<int>(edY),
                       static_cast<int>(ipW), 24);
    ex += ipW + 5.0f;
    portEditor.setBounds(static_cast<int>(ex), static_cast<int>(edY),
                         static_cast<int>(portW), 24);
    ex += portW + 5.0f;
    retStreamEditor.setBounds(static_cast<int>(ex), static_cast<int>(edY),
                              static_cast<int>(stW), 24);
    ex += stW + 5.0f;
    sndStreamEditor.setBounds(static_cast<int>(ex), static_cast<int>(edY),
                              static_cast<int>(stW), 24);
    ex += stW + 5.0f;
    btnApplyTarget.setBounds(static_cast<int>(ex), static_cast<int>(edY),
                             static_cast<int>(std::max(80.0f, btnW)), 24);
  }

private:
  static constexpr size_t kNumCables =
      nova::graviton::VirtualCableTransport::kNumVirtualCables;

  struct CableRowUi {
    std::unique_ptr<MidasTactileButton> btnSelect, btnRetEn, btnRetCh,
        btnRetMute, btnSndEn, btnSndSrc, btnSndMute;
  };

  void timerCallback() override {
    if (isVisible())
      repaint();
  }

  void loadSelectedTargetFields() {
    const auto &vc = engine.virtualTransport.cables[selectedCable];
    juce::String ip, sndStream;
    int port = 6980;
    vc.getSendTarget(ip, port, sndStream);
    ipEditor.setText(ip, juce::dontSendNotification);
    portEditor.setText(juce::String(port), juce::dontSendNotification);
    retStreamEditor.setText(vc.getReturnStreamName(), juce::dontSendNotification);
    sndStreamEditor.setText(sndStream, juce::dontSendNotification);
  }

  void refreshButtonLabels() {
    btnToggleVban.setButtonText(engine.virtualTransport.isNetworkRunning()
                                    ? "STOP VBAN (6980)"
                                    : "START VBAN (6980)");
    btnToggleShm.setButtonText(engine.virtualTransport.isSharedMemoryOpen()
                                   ? "STOP WIN SHM"
                                   : "START WIN SHM");
    const auto bk = engine.virtualTransport.getTransportBackend();
    btnBackend.setButtonText(
        bk == nova::graviton::TransportBackend::Both
            ? "MODE: BOTH"
            : (bk == nova::graviton::TransportBackend::VbanUdp
                   ? "MODE: VBAN ONLY"
                   : "MODE: SHM ONLY"));

    for (size_t k = 0; k < kNumCables; ++k) {
      const auto &vc = engine.virtualTransport.cables[k];
      auto &r = rows[k];
      const bool retOn = vc.returnEnabled.load(std::memory_order_relaxed);
      const bool retM = vc.returnMuted.load(std::memory_order_relaxed);
      const int chL = vc.returnTargetChannelL.load(std::memory_order_relaxed);
      const int chR = vc.returnTargetChannelR.load(std::memory_order_relaxed);
      r.btnRetEn->setButtonText(retOn ? "RET: ON" : "RET: OFF");
      juce::String chTxt = "NONE";
      if (chL >= 0) {
        chTxt = (chR == chL + 1)
                    ? ("CH " + juce::String(chL + 1) + "-" + juce::String(chR + 1))
                    : ("CH " + juce::String(chL + 1));
      }
      r.btnRetCh->setButtonText(chTxt);
      r.btnRetMute->setToggleState(retM, juce::dontSendNotification);

      const bool sndOn = vc.sendEnabled.load(std::memory_order_relaxed);
      const bool sndM = vc.sendMuted.load(std::memory_order_relaxed);
      const auto srcType = static_cast<nova::graviton::OutputBusSourceType>(
          vc.sendSourceType.load(std::memory_order_relaxed));
      r.btnSndEn->setButtonText(sndOn ? "SND: ON" : "SND: OFF");
      r.btnSndSrc->setButtonText(
          srcType == nova::graviton::OutputBusSourceType::AuxBus
              ? "AUX 1-2"
              : (srcType == nova::graviton::OutputBusSourceType::MatrixBus
                     ? "MTX 1-2"
                     : (srcType ==
                                nova::graviton::OutputBusSourceType::DirectOut
                            ? "DIR OUT"
                            : "MAIN L/R")));
      r.btnSndMute->setToggleState(sndM, juce::dontSendNotification);
    }
    repaint();
  }

  MixerEngine &engine;
  size_t selectedCable = 0;
  juce::String statusNote =
      "Pilih CABLE 1-2..7-8, atur Return/Send & Target IP/Stream VBAN.";
  MidasTactileButton btnToggleVban, btnToggleShm, btnBackend, btnApplyTarget;
  std::array<CableRowUi, kNumCables> rows;
  juce::TextEditor ipEditor, portEditor, retStreamEditor, sndStreamEditor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasVirtualCableView)
};

// ==============================================================================
// COMPLETE MIDAS TOURING CONSOLE SURFACE COMPONENT
// ==============================================================================
class MidasTouringConsoleSurface : public juce::Component, private juce::Timer {
public:
  enum class MainNavGroup {
    Mix = 0,
    Routing = 1,
    Fx = 2,
    Show = 3,
    System = 4
  };

  MidasTouringConsoleSurface(
      MixerEngine &eng, juce::AudioDeviceManager &dm, MixerMidiCallback &midi,
      nova::SecondaryAudioStreamer *secStreamer = nullptr)
      : engine(eng), deviceManager(dm), midiCallback(midi),
        secondaryStreamer(secStreamer), aiModal(eng), monitorView(eng),
        talkbackView(eng), patchMatrixView(eng), surroundPanView(eng),
        offlineBounceView(
            eng,
            [this](bool suspend) {
              if (suspend) {
                if (!audioSuspendedForBounce &&
                    engine.running.load(std::memory_order_relaxed)) {
                  deviceManager.closeAudioDevice();
                  audioSuspendedForBounce = true;
                }
              } else if (audioSuspendedForBounce) {
                audioSuspendedForBounce = false;
                deviceManager.restartLastAudioDevice();
              }
            }),
        asioBridgeView(eng), oscServerView(eng), virtualCableView(eng),
        fxHostView(eng),
        dynamicEqView(eng), dynamicsPanelView(eng),
        vcaPanelView(eng), duganView(eng), fullCueListView(eng),
        btnNavMenu("MIX v", MidasTactileButton::GlowMode::Cyan),
        btnNavBack("< BACK", MidasTactileButton::GlowMode::Cyan),
        btnNavHome("HOME", MidasTactileButton::GlowMode::Gold),
        btnNavSearch("FIND", MidasTactileButton::GlowMode::Cyan),
        btnNavFav("* FAV", MidasTactileButton::GlowMode::Gold),
        btnNavRecent("RECENT", MidasTactileButton::GlowMode::Purple) {

    // Top status bar
    addAndMakeVisible(statusBar);

    // Dynamic Swappable Sub-Views (All Embedded directly in Touchscreen)
    addAndMakeVisible(overviewView);
    addChildComponent(dynamicEqView);
    addChildComponent(dynamicsPanelView);
    addChildComponent(duganView);
    addChildComponent(monitorView);
    addChildComponent(talkbackView);

    addChildComponent(patchMatrixView);
    addChildComponent(surroundPanView);
    addChildComponent(vcaPanelView);
    addChildComponent(routingView);

    addChildComponent(fxHostView);
    fxHostView.onSubPageChanged = [this](int /*pageIdx*/) {
      if (activeGroup == MainNavGroup::Fx) {
        activeSubPage = 0;
        for (size_t i = 0; i < subTabButtons.size(); ++i)
          subTabButtons[i]->setToggleState(i == 0, juce::dontSendNotification);
      }
    };

    addChildComponent(fullCueListView);
    addChildComponent(cuelistView);
    addChildComponent(recorderView);
    addChildComponent(offlineBounceView);

    addChildComponent(asioBridgeView);
    addChildComponent(oscServerView);
    addChildComponent(virtualCableView);
    addChildComponent(setupView);
    audioDeviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager, 1, 64, 1, 64, true, true, true, false);
    addChildComponent(*audioDeviceSelector);
    addChildComponent(broadcastView);
    addChildComponent(homeLauncherView);
    addChildComponent(recentView);
    addChildComponent(quickSettingsPanel);

    addChildComponent(btnNavMenu);
    addAndMakeVisible(btnNavBack);
    addAndMakeVisible(btnNavHome);
    addAndMakeVisible(btnNavSearch);
    addAndMakeVisible(btnNavFav);
    addAndMakeVisible(btnNavRecent);
    btnNavMenu.setClickingTogglesState(false);
    btnNavBack.setClickingTogglesState(false);
    btnNavHome.setClickingTogglesState(false);
    btnNavSearch.setClickingTogglesState(false);
    btnNavFav.setClickingTogglesState(true);
    btnNavRecent.setClickingTogglesState(false);

    // Pre-pin essential shortcuts to Favorites
    favoriteEntries = {
        {"Virtual Cable", "SYSTEM", 4, 2, midas_colors::kAccentCyan},
        {"Patch Matrix", "ROUTING", 1, 0, midas_colors::kAccentGreen},
        {"Audio Hardware", "SYSTEM", 4, 1, midas_colors::kAccentPurple},
    };

    btnNavMenu.onClick = [this] {
      juce::PopupMenu menu;
      menu.addItem(1, "1. MIX");
      menu.addItem(2, "2. ROUTING");
      menu.addItem(3, "3. FX RACK");
      menu.addItem(4, "4. SHOW");
      menu.addItem(5, "5. SYSTEM");
      menu.showMenuAsync(
          juce::PopupMenu::Options().withTargetComponent(&btnNavMenu),
          [this](int res) {
            if (res >= 1 && res <= 5)
              selectNav(static_cast<MainNavGroup>(res - 1), 0);
          });
    };

    btnNavBack.onClick = [this] {
      if (homeLauncherView.isVisible() || recentView.isVisible()) {
        homeLauncherView.setVisible(false);
        recentView.setVisible(false);
        repaint();
        return;
      }
      if (!navHistory.empty()) {
        auto prev = navHistory.back();
        navHistory.pop_back();
        selectNav(prev.first, prev.second, false);
      } else {
        openHomeLauncher(false);
      }
    };
    btnNavHome.onClick = [this] {
      if (homeLauncherView.isVisible()) {
        homeLauncherView.setVisible(false);
        repaint();
      } else {
        openHomeLauncher(false);
      }
    };
    btnNavSearch.onClick = [this] { openHomeLauncher(true); };
    btnNavFav.onClick = [this] { toggleCurrentPageFavorite(); };
    btnNavRecent.onClick = [this] {
      if (recentView.isVisible()) {
        recentView.setVisible(false);
        repaint();
      } else {
        homeLauncherView.setVisible(false);
        recentView.setFavorites(favoriteEntries);
        recentView.setEntries(recentEntries);
        recentView.setVisible(true);
        recentView.toFront(true);
        repaint();
      }
    };

    homeLauncherView.onPageSelected = [this](int grpIdx, int subPage) {
      homeLauncherView.setVisible(false);
      selectNav(static_cast<MainNavGroup>(juce::jlimit(0, 4, grpIdx)), subPage);
    };
    homeLauncherView.onToggleFavorite = [this](int grpIdx, int subPage) {
      toggleFavoriteForPage(grpIdx, subPage);
    };
    homeLauncherView.onAppSelected = [this](int appIdx) {
      homeLauncherView.setVisible(false);
      if (appIdx == 0)
        selectNav(MainNavGroup::Mix, 0);
      else if (appIdx == 1) {
        selectNav(MainNavGroup::Mix, 0);
        openChannelAiModal();
      } else if (appIdx == 2)
        selectNav(MainNavGroup::Routing, 0);
      else if (appIdx == 3)
        selectNav(MainNavGroup::Mix, 1);
      else if (appIdx == 4)
        selectNav(MainNavGroup::Mix, 2);
      else if (appIdx == 5)
        selectNav(MainNavGroup::Fx, 0);
      else if (appIdx == 6)
        selectNav(MainNavGroup::Show, 1);
      else if (appIdx == 7)
        selectNav(MainNavGroup::System, 4);
    };

    recentView.onEntrySelected = [this](int grpIdx, int subPage) {
      recentView.setVisible(false);
      selectNav(static_cast<MainNavGroup>(grpIdx), subPage);
    };
    recentView.onDismiss = [this] {
      recentView.setVisible(false);
      repaint();
    };

    quickSettingsPanel.onBrightnessChanged = [this](float b) {
      screenBrightness = juce::jlimit(0.35f, 1.0f, b);
      repaint();
    };
    quickSettingsPanel.onCycleTextScale = [this] {
      statusBar.cycleTextScale();
      refreshQuickSettings();
    };
    quickSettingsPanel.onOpenAudioSettings = [this] {
      selectNav(MainNavGroup::System, 1);
    };
    quickSettingsPanel.onOpenHomeLauncher = [this] { openHomeLauncher(false); };

    statusBar.onOpenQuickSettings = [this] {
      if (quickSettingsPanel.isVisible()) {
        quickSettingsPanel.setVisible(false);
      } else {
        refreshQuickSettings();
        quickSettingsPanel.setBounds(getLocalBounds());
        quickSettingsPanel.setVisible(true);
        quickSettingsPanel.toFront(true);
      }
    };

    // 5 Main Group Buttons (kept hidden - groups are accessed via Home
    // Directory & Global Search to avoid top-bar clutter)
    static const char *grpNames[5] = {"MIX", "ROUTING", "FX", "SHOW",
                                      "SYSTEM"};
    static const MidasTactileButton::GlowMode grpGlows[5] = {
        MidasTactileButton::GlowMode::Cyan, MidasTactileButton::GlowMode::Green,
        MidasTactileButton::GlowMode::Gold,
        MidasTactileButton::GlowMode::Purple,
        MidasTactileButton::GlowMode::Cyan};
    for (int i = 0; i < 5; ++i) {
      groupButtons[i] =
          std::make_unique<MidasTactileButton>(grpNames[i], grpGlows[i]);
      addChildComponent(*groupButtons[i]);
      const int gIdx = i;
      groupButtons[i]->onClick = [this, gIdx] {
        selectNav(static_cast<MainNavGroup>(gIdx), 0);
      };
    }

    // Dynamic Context Sub-Tab Buttons (hidden from top bar; navigation via Home
    // Directory & Search)
    for (size_t i = 0; i < subTabButtons.size(); ++i) {
      subTabButtons[i] = std::make_unique<MidasTactileButton>(
          "", MidasTactileButton::GlowMode::Cyan);
      addChildComponent(*subTabButtons[i]);
      const int sIdx = static_cast<int>(i);
      subTabButtons[i]->onClick = [this, sIdx] {
        selectNav(activeGroup, sIdx);
      };
    }

    // Quick channel strip
    addAndMakeVisible(quickStrip);

    // Upper Right Dedicated Hardware DSP
    addAndMakeVisible(dedicatedDsp);

    // Layer Navigation Bar
    addAndMakeVisible(layerBar);

    // Lower 8 Strips Bay + Master Bay
    for (int i = 0; i < 8; ++i) {
      strips[static_cast<size_t>(i)] = std::make_unique<MidasConsoleStrip>(i);
      addAndMakeVisible(*strips[static_cast<size_t>(i)]);
    }
    addAndMakeVisible(masterStrip);
    addAndMakeVisible(monitorStrip);
    addAndMakeVisible(masterProcessing);

    // Luxury Mahogany Armrest
    addAndMakeVisible(armrest);

    // Modals & Embedded Modules (all embedded inside touchscreen layer)
    gangModal.setEmbedded(true);
    addChildComponent(gangModal);
    dspModal.setEmbedded(true);
    addChildComponent(dspModal);
    aiModal.setEmbedded(true);
    addChildComponent(aiModal);
    mcloudModal.setEmbedded(true);
    addChildComponent(mcloudModal);

    setupInteractions();

    // Default touring fader and pan values matching FOH reference image
    static const float initialPans[8] = {0.0f,   0.05f, -0.25f, 0.0f,
                                         -0.40f, 0.40f, -0.15f, 0.0f};
    static const float initialFaders[8] = {1.2f,  0.0f,  -4.2f, 2.0f,
                                           -1.8f, -1.8f, -3.0f, 3.6f};
    for (int i = 0; i < 8; ++i) {
      if (i < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(i)].pan.store(
            initialPans[i], std::memory_order_relaxed);
        engine.channels[static_cast<size_t>(i)].gainDb.store(
            initialFaders[i], std::memory_order_relaxed);
        // Default safe mute on boot to prevent live PC microphone feedback/hiss
        // loop
        engine.channels[static_cast<size_t>(i)].muted.store(
            true, std::memory_order_relaxed);
      }
    }

    // Priority 1: Ensure MST DLY and MON DLY strictly initialized to 0.0 ms
    engine.masterDelayMs.store(0.0f, std::memory_order_relaxed);
    engine.monitorDelayMs.store(0.0f, std::memory_order_relaxed);
    masterProcessing.setDelays(0.0f, 0.0f);

    rebankStrips(0);
    selectChannel(0);
    selectNav(MainNavGroup::Mix, 0);

    startTimerHz(25); // 25 fps UI telemetry update
  }

  ~MidasTouringConsoleSurface() override { stopTimer(); }

  std::function<void()> onOpenFxRack;
  std::function<void()> onOpenAudioSettings;
  std::function<void()> onOpenOfflineBounce;
  std::function<void()> onOpenDugan;
  std::function<void()> onOpenDN360;
  std::function<void()> onOpenPatchMatrix;
  std::function<void()> onOpenRecording;
  std::function<void()> onOpenCueList;
  std::function<void()> onOpenDynEq;
  std::function<void()> onOpenChannelAi;
  std::function<void()> onOpenDynamics;
  std::function<void()> onOpenSurroundPan;
  std::function<void()> onOpenManchino;
  std::function<void()> onOpenVcaPanel;
  std::function<void()> onOpenPopGroups;
  std::function<void()> onOpenMatrixGrid;
  std::function<void()> onOpenTalkback;
  std::function<void()> onOpenMonitorCr;
  std::function<void()> onLoadWav;
  std::function<void()> onOpenAsioBridge;
  std::function<void()> onOpenOscServer;
  std::function<void(int)> onOpenHardwareFxEditor;
  std::function<void(int)> onChannelSelected;

  void rebankStrips(int bank) {
    currentBank = juce::jlimit(0, 4, bank);

    struct BankPreset {
      const char *tag;
      const char *name;
      const char *badge;
      juce::Colour col;
    };

    static const BankPreset bankPresets[5][8] = {
        // 0: INPUTS 1-8 (Matched to Reference Touring Surface Layout)
        {{"DRUMS 1", "KICK IN", "VCA COMP | MIDAS ...",
          midas_colors::kAccentRed},
         {"DRUMS 2", "SNARE TOP", "FET 76 | BRITISH ...",
          midas_colors::kAccentRed},
         {"DRUMS 3", "HI-HAT", "VCA COMP | MIDAS ...",
          midas_colors::kAccentGold},
         {"BASS 4", "BASS DI", "OPTO T4 | TUBE-PUL ...",
          midas_colors::kAccentGold},
         {"GUITARS 5", "GTR ELEC L", "VCA COMP | BRITIS ...",
          midas_colors::kAccentGreen},
         {"GUITARS 6", "GTR ELEC R", "VCA COMP | BRITIS ...",
          midas_colors::kAccentGreen},
         {"KEYS 7", "KEYBOARD", "BUS GLUE | DYN-PEQ",
          midas_colors::kAccentPurple},
         {"VOX 8", "LEAD VOX", "OPTO T4 | MIDAS P ...",
          juce::Colour(0xffff9500)}},
        // 1: INPUTS 9-16
        {{"IN 09", "TOM 1", "CH 09", midas_colors::kAccentRed},
         {"IN 10", "TOM 2", "CH 10", midas_colors::kAccentRed},
         {"IN 11", "OH LEFT", "CH 11", midas_colors::kAccentGold},
         {"IN 12", "OH RIGHT", "CH 12", midas_colors::kAccentGold},
         {"IN 13", "SYNTH PAD", "CH 13", midas_colors::kAccentPurple},
         {"IN 14", "ACOUSTIC", "CH 14", midas_colors::kAccentGreen},
         {"IN 15", "BACKING 1", "CH 15", midas_colors::kAccentGold},
         {"IN 16", "BACKING 2", "CH 16", midas_colors::kAccentGold}},
        // 2: MIX BUS / AUX
        {{"SEND 1-2 R1", "IEM VOX", "AUX 01", midas_colors::kAccentCyan},
         {"SEND 1-2 R2", "IEM DRUMS", "AUX 02", midas_colors::kAccentCyan},
         {"SEND 3-4 R1", "IEM GUITAR", "AUX 03", midas_colors::kAccentCyan},
         {"SEND 3-4 R2", "IEM BASS", "AUX 04", midas_colors::kAccentCyan},
         {"SEND 5-6 R1", "WEDGE 1", "AUX 05", midas_colors::kAccentGold},
         {"SEND 5-6 R2", "WEDGE 2", "AUX 06", midas_colors::kAccentGold},
         {"SEND 7-8 R1", "SIDEFILL L", "AUX 07", midas_colors::kAccentPurple},
         {"SEND 7-8 R2", "SIDEFILL R", "AUX 08", midas_colors::kAccentPurple}},
        // 3: VCA GROUPS
        {{"VCA 01", "DRUMS VCA", "VCA 01", midas_colors::kAccentRed},
         {"VCA 02", "BASS VCA", "VCA 02", midas_colors::kAccentCyan},
         {"VCA 03", "GUITAR VCA", "VCA 03", midas_colors::kAccentGreen},
         {"VCA 04", "KEYS VCA", "VCA 04", midas_colors::kAccentPurple},
         {"VCA 05", "HORNS VCA", "VCA 05", midas_colors::kAccentGold},
         {"VCA 06", "VOX VCA", "VCA 06", midas_colors::kAccentGold},
         {"VCA 07", "CHOIR VCA", "VCA 07", midas_colors::kAccentCyan},
         {"VCA 08", "FX RET VCA", "VCA 08", midas_colors::kAccentBlue}},
        // 4: MATRIX / MASTERS
        {{"MTX 01", "MAIN PA L", "MTX 01", midas_colors::kAccentGold},
         {"MTX 02", "MAIN PA R", "MTX 02", midas_colors::kAccentGold},
         {"MTX 03", "FRONT FILL", "MTX 03", midas_colors::kAccentCyan},
         {"MTX 04", "SUB MONO", "MTX 04", midas_colors::kAccentRed},
         {"MTX 05", "OUTFILL L", "MTX 05", midas_colors::kAccentGreen},
         {"MTX 06", "OUTFILL R", "MTX 06", midas_colors::kAccentGreen},
         {"MTX 07", "RECORDING L", "MTX 07", midas_colors::kAccentPurple},
         {"MTX 08", "RECORDING R", "MTX 08", midas_colors::kAccentPurple}}};

    const int spillVca = engine.activeSpillVca.load(std::memory_order_relaxed);
    const auto &p = bankPresets[currentBank];
    for (int i = 0; i < 8; ++i) {
      if (currentBank == 2) {
        strips[static_cast<size_t>(i)]->setChannelInfo(p[i].tag, p[i].name,
                                                       p[i].badge, p[i].col);
        const float auxDb =
            (static_cast<size_t>(i) < engine.auxMasterDb.size())
                ? engine.auxMasterDb[static_cast<size_t>(i)].load(
                      std::memory_order_relaxed)
                : -60.0f;
        const bool auxMt = (static_cast<size_t>(i) < engine.auxMuted.size())
                               ? engine.auxMuted[static_cast<size_t>(i)].load(
                                     std::memory_order_relaxed)
                               : true;
        const float auxPk = (static_cast<size_t>(i) < engine.auxPeak.size())
                                ? engine.auxPeak[static_cast<size_t>(i)].load(
                                      std::memory_order_relaxed)
                                : 0.0f;
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(auxDb, auxMt, auxPk);
      } else if (currentBank == 3) {
        strips[static_cast<size_t>(i)]->setChannelInfo(p[i].tag, p[i].name,
                                                       p[i].badge, p[i].col);
        const float vcaDb =
            (static_cast<size_t>(i) < engine.vcaMasterDb.size())
                ? engine.vcaMasterDb[static_cast<size_t>(i)].load(
                      std::memory_order_relaxed)
                : 0.0f;
        const bool vcaMt = (static_cast<size_t>(i) < engine.vcaMuted.size())
                               ? engine.vcaMuted[static_cast<size_t>(i)].load(
                                     std::memory_order_relaxed)
                               : false;
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(vcaDb, vcaMt, 0.0f);
      } else {
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          if (spillVca >= 1 || activePopGroup >= 1 || inputRangeOffset > 0) {
            const juce::String numStr =
                (chIdx + 1 < 10 ? "0" : "") + juce::String(chIdx + 1);
            const juce::String tagStr =
                (spillVca >= 1) ? ("VCA " + juce::String(spillVca))
                                : (activePopGroup >= 1
                                       ? ("POP " + juce::String(activePopGroup))
                                       : ("IN " + numStr));
            const juce::String chName =
                engine.channelNames[static_cast<size_t>(chIdx)].isNotEmpty()
                    ? engine.channelNames[static_cast<size_t>(chIdx)]
                    : ("CH " + numStr);
            const juce::Colour col =
                (spillVca >= 1)
                    ? midas_colors::kAccentPurple
                    : (activePopGroup >= 1 ? midas_colors::kAccentGold
                                           : midas_colors::kAccentCyan);
            strips[static_cast<size_t>(i)]->setChannelInfo(tagStr, chName,
                                                           "CH " + numStr, col);
          } else {
            strips[static_cast<size_t>(i)]->setChannelInfo(
                p[i].tag, p[i].name, p[i].badge, p[i].col);
          }
          strips[static_cast<size_t>(i)]->syncState(
              engine.channels[static_cast<size_t>(chIdx)]);
        } else {
          strips[static_cast<size_t>(i)]->setChannelInfo(
              "--", "(EMPTY)", "UNASSIGNED", midas_colors::kTextMuted);
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(-90.0f, true, 0.0f);
        }
      }
    }
  }

  void selectChannel(int slot) {
    selectedSlot = juce::jlimit(0, 7, slot);
    for (int i = 0; i < 8; ++i) {
      strips[static_cast<size_t>(i)]->setSelected(i == selectedSlot);
    }
    const int chIdx = getEngineChannelForSlot(selectedSlot);
    if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
      const auto &state = engine.channels[static_cast<size_t>(chIdx)];
      quickStrip.syncChannel(state);
      dedicatedDsp.syncChannel(state);



      std::array<bool, 8> tileStates{};
      tileStates[0] = state.masterAssigned.load(std::memory_order_relaxed);
      tileStates[1] = state.monitorAssigned.load(std::memory_order_relaxed);
      tileStates[2] = state.monitorPreFader.load(std::memory_order_relaxed);
      tileStates[3] =
          (state.auxSendDb[0].load(std::memory_order_relaxed) > -50.0f);
      tileStates[4] =
          (state.auxSendDb[2].load(std::memory_order_relaxed) > -50.0f);
      tileStates[5] =
          (state.auxSendDb[4].load(std::memory_order_relaxed) > -50.0f);
      tileStates[6] = (state.vcaGroup.load(std::memory_order_relaxed) == 1);
      tileStates[7] = state.muteSafe.load(std::memory_order_relaxed);
      routingView.setTileStates(tileStates);
      dynamicEqView.setSelectedChannel(chIdx);
      dynamicsPanelView.setSelectedChannel(chIdx);
      surroundPanView.setSelectedChannel(chIdx);
      fxHostView.setSelectedChannel(chIdx);
      juce::String chName = "IN " + juce::String(chIdx + 1);
      if (selectedSlot >= 0 && selectedSlot < 8)
        chName = strips[static_cast<size_t>(selectedSlot)]->getName();
      aiModal.setTargetChannel(chIdx, chName);
      syncVcaAndMatrixViews();
      if (onChannelSelected)
        onChannelSelected(chIdx);
    }
    repaint();
  }

  void syncVcaAndMatrixViews() {
    std::array<int, 8> vcaCounts{};
    for (const auto &ch : engine.channels) {
      const int g = ch.vcaGroup.load(std::memory_order_relaxed);
      if (g >= 1 && g <= 8)
        vcaCounts[static_cast<size_t>(g - 1)]++;
    }
    std::array<float, 8> vcaLevels{};
    std::array<bool, 8> vcaMutes{};
    std::array<float, 8> mtxLevels{};
    std::array<bool, 8> mtxMutes{};
    for (size_t i = 0; i < 8; ++i) {
      vcaLevels[i] = engine.vcaMasterDb[i].load(std::memory_order_relaxed);
      vcaMutes[i] = engine.vcaMuted[i].load(std::memory_order_relaxed);
      mtxLevels[i] = engine.matrixMasterDb[i].load(std::memory_order_relaxed);
      mtxMutes[i] = engine.matrixMuted[i].load(std::memory_order_relaxed);
    }
    int selVca = 0;
    const int chIdx = getEngineChannelForSlot(selectedSlot);
    if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size()))
      selVca = engine.channels[static_cast<size_t>(chIdx)].vcaGroup.load(
          std::memory_order_relaxed);

  }

  void applyConsoleScene(int sceneId) {
    statusBar.setSelectedSceneId(sceneId);
    cuelistView.setActiveSceneId(sceneId);

    for (size_t i = 0; i < 16 && i < engine.channels.size(); ++i) {
      auto &ch = engine.channels[i];
      if (sceneId == 1) { // FOH Arena Live
        ch.gainDb.store((i == 0 || i == 7) ? 2.0f : -2.5f,
                        std::memory_order_relaxed);
        ch.eqGainDb[0].store((i == 0 || i == 3) ? 3.5f : 0.0f,
                             std::memory_order_relaxed);
        ch.eqGainDb[3].store((i == 7) ? 2.5f : 1.0f, std::memory_order_relaxed);
      } else if (sceneId == 2) { // Acoustic Unplugged
        ch.gainDb.store((i >= 4 && i <= 7) ? 0.0f : -12.0f,
                        std::memory_order_relaxed);
        ch.eqGainDb[0].store(-2.0f, std::memory_order_relaxed);
        ch.eqGainDb[3].store(3.0f, std::memory_order_relaxed);
      } else if (sceneId == 3) { // Drum Solo Punch
        ch.gainDb.store((i <= 3) ? 4.0f : -18.0f, std::memory_order_relaxed);
        ch.compressorThresholdDb.store(-24.0f, std::memory_order_relaxed);
        ch.compressorRatio.store(8.0f, std::memory_order_relaxed);
      } else if (sceneId == 4) { // Broadcast 4K Stem
        ch.gainDb.store(-3.0f, std::memory_order_relaxed);
        ch.compressorThresholdDb.store(-16.0f, std::memory_order_relaxed);
        ch.compressorRatio.store(4.0f, std::memory_order_relaxed);
      } else { // All Flat Unity (0dB)
        ch.gainDb.store(0.0f, std::memory_order_relaxed);
        for (int b = 0; b < 4; ++b)
          ch.eqGainDb[static_cast<size_t>(b)].store(0.0f,
                                                    std::memory_order_relaxed);
      }
    }
    selectChannel(selectedSlot);
  }

  MainNavGroup getActiveGroup() const { return activeGroup; }
  int getActiveSubPage() const { return activeSubPage; }

  void selectNav(MainNavGroup group, int subPage = 0,
                 bool recordHistory = true) {
    if (recordHistory && (group != activeGroup || subPage != activeSubPage)) {
      navHistory.push_back({activeGroup, activeSubPage});
      if (navHistory.size() > 16)
        navHistory.erase(navHistory.begin());
    }
    activeGroup = group;
    activeSubPage = subPage;
    homeLauncherView.setVisible(false);
    recentView.setVisible(false);

    // 1. Group Buttons toggle states
    for (int i = 0; i < 5; ++i)
      groupButtons[i]->setToggleState(i == static_cast<int>(activeGroup),
                                      juce::dontSendNotification);

    // 2. Hide all dynamic components
    overviewView.setVisible(false);
    dynamicEqView.setVisible(false);
    dynamicsPanelView.setVisible(false);
    aiModal.setVisible(false);
    dspModal.setVisible(false);
    duganView.setVisible(false);
    monitorView.setVisible(false);
    talkbackView.setVisible(false);

    patchMatrixView.setVisible(false);
    surroundPanView.setVisible(false);
    vcaPanelView.setVisible(false);
    routingView.setVisible(false);
    gangModal.setVisible(false);

    fxHostView.setVisible(false);

    fullCueListView.setVisible(false);
    cuelistView.setVisible(false);
    recorderView.setVisible(false);
    offlineBounceView.setVisible(false);
    broadcastView.setVisible(false);
    mcloudModal.setVisible(false);

    asioBridgeView.setVisible(false);
    oscServerView.setVisible(false);
    virtualCableView.setVisible(false);
    setupView.setVisible(false);
    if (audioDeviceSelector)
      audioDeviceSelector->setVisible(false);

    // 3. Define and show active subpage
    updateSubTabs();
    resized();
  }

  void updateSubTabs() {
    struct SubTabDef {
      const char *label;
      MidasTactileButton::GlowMode glow;
    };
    std::vector<SubTabDef> defs;

    if (activeGroup == MainNavGroup::Mix) {
      defs = {{"OVERVIEW", MidasTactileButton::GlowMode::Cyan},
              {"EQUALISER (PEQ & DYN)", MidasTactileButton::GlowMode::Gold},
              {"COMP & GATE", MidasTactileButton::GlowMode::Purple},
              {"MIDAS AI", MidasTactileButton::GlowMode::Green},
              {"DUGAN AUTOMIXER", MidasTactileButton::GlowMode::Gold},
              {"MONITOR & TALKBACK", MidasTactileButton::GlowMode::Cyan}};
      activeSubPage = juce::jlimit(0, (int)defs.size() - 1, activeSubPage);
      if (activeSubPage == 0)
        overviewView.setVisible(true);
      else if (activeSubPage == 1) {
        dynamicEqView.setVisible(true);
        dedicatedDsp.setFocusMode(MidasDedicatedDspPanel::FocusMode::Eq);
      } else if (activeSubPage == 2) {
        dynamicsPanelView.setVisible(true);
        dedicatedDsp.setFocusMode(
            dynamicsPanelView.isGateViewActive()
                ? MidasDedicatedDspPanel::FocusMode::Gate
                : MidasDedicatedDspPanel::FocusMode::Comp);
      } else if (activeSubPage == 3) {
        const int chIdx = getEngineChannelForSlot(selectedSlot);
        juce::String chName = "IN " + juce::String(chIdx + 1);
        if (selectedSlot >= 0 && selectedSlot < 8)
          chName = strips[static_cast<size_t>(selectedSlot)]->getName();
        aiModal.setTargetChannel(chIdx, chName);
        aiModal.setVisible(true);
      } else if (activeSubPage == 4) {
        duganView.setVisible(true);
      } else if (activeSubPage == 5) {
        monitorView.setVisible(true);
        talkbackView.setVisible(true);
      }
    } else if (activeGroup == MainNavGroup::Routing) {
      defs = {{"PATCH MATRIX", MidasTactileButton::GlowMode::Green},
              {"3D SURROUND", MidasTactileButton::GlowMode::Cyan},
              {"VCA GROUPS", MidasTactileButton::GlowMode::Gold},
              {"BUS & DIRECT", MidasTactileButton::GlowMode::Cyan},
              {"GANG FADER", MidasTactileButton::GlowMode::Cyan}};
      activeSubPage = juce::jlimit(0, (int)defs.size() - 1, activeSubPage);
      if (activeSubPage == 0)
        patchMatrixView.setVisible(true);
      else if (activeSubPage == 1)
        surroundPanView.setVisible(true);
      else if (activeSubPage == 2)
        vcaPanelView.setVisible(true);
      else if (activeSubPage == 3)
        routingView.setVisible(true);
      else if (activeSubPage == 4) {
        std::array<int, 8> vis{};
        for (int s = 0; s < 8; ++s)
          vis[static_cast<size_t>(s)] = getEngineChannelForSlot(s);
        gangModal.setVisibleChannels(vis);
        gangModal.setVisible(true);
      }
    } else if (activeGroup == MainNavGroup::Fx) {
      defs = {{"24-SLOT RACK", MidasTactileButton::GlowMode::Gold},
              {"DSP MODELS", MidasTactileButton::GlowMode::Cyan}};
      activeSubPage = juce::jlimit(0, (int)defs.size() - 1, activeSubPage);
      if (activeSubPage == 1) {
        dspModal.setVisible(true);
      } else {
        fxHostView.setVisible(true);
        fxHostView.toFront(false);
        fxHostView.showSubPage(0);
      }
    } else if (activeGroup == MainNavGroup::Show) {
      defs = {{"CUE LIST", MidasTactileButton::GlowMode::Green},
              {"64-CH RECORDER", MidasTactileButton::GlowMode::Cyan},
              {"OFFLINE BOUNCE", MidasTactileButton::GlowMode::Purple},
              {"SCENE OVERVIEW", MidasTactileButton::GlowMode::Gold},
              {"BROADCAST STEMS", MidasTactileButton::GlowMode::Gold},
              {"mCLOUD SYNC", MidasTactileButton::GlowMode::Cyan}};
      activeSubPage = juce::jlimit(0, (int)defs.size() - 1, activeSubPage);
      if (activeSubPage == 0)
        fullCueListView.setVisible(true);
      else if (activeSubPage == 1)
        recorderView.setVisible(true);
      else if (activeSubPage == 2)
        offlineBounceView.setVisible(true);
      else if (activeSubPage == 3)
        cuelistView.setVisible(true);
      else if (activeSubPage == 4)
        broadcastView.setVisible(true);
      else if (activeSubPage == 5)
        mcloudModal.setVisible(true);
    } else if (activeGroup == MainNavGroup::System) {
      // Grouped: Audio I/O (0-2) | Remote (3) | Setup (4)
      defs = {{"ASIO BRIDGE", MidasTactileButton::GlowMode::Cyan},
              {"AUDIO HARDWARE", MidasTactileButton::GlowMode::Purple},
              {"VIRTUAL CABLE", MidasTactileButton::GlowMode::Cyan},
              {"OSC SERVER", MidasTactileButton::GlowMode::Green},
              {"CONSOLE SETUP", MidasTactileButton::GlowMode::Cyan}};
      activeSubPage = juce::jlimit(0, (int)defs.size() - 1, activeSubPage);
      if (activeSubPage == 0)
        asioBridgeView.setVisible(true);
      else if (activeSubPage == 1) {
        if (audioDeviceSelector)
          audioDeviceSelector->setVisible(true);
      } else if (activeSubPage == 2)
        virtualCableView.setVisible(true);
      else if (activeSubPage == 3)
        oscServerView.setVisible(true);
      else if (activeSubPage == 4)
        setupView.setVisible(true);
    }

    // Circled group/sub-tab bars removed from touchscreen header
    for (size_t i = 0; i < subTabButtons.size(); ++i)
      subTabButtons[i]->setVisible(false);

    if (activeSubPage >= 0 && activeSubPage < static_cast<int>(defs.size())) {
      static const char *grpTags[5] = {"MIX", "ROUTING", "FX", "SHOW",
                                       "SYSTEM"};
      const int gInt = juce::jlimit(0, 4, static_cast<int>(activeGroup));
      activeGroupTag = grpTags[gInt];
      activePageTitle = defs[static_cast<size_t>(activeSubPage)].label;
      activeBreadcrumbText = activeGroupTag + "  >  " + activePageTitle;
      btnNavMenu.setButtonText(activeGroupTag + " v");
      homeLauncherView.setActivePage(gInt, activeSubPage);

      AndroidRecentWorkspaceView::Entry entry;
      entry.title = activePageTitle;
      entry.groupLabel = activeGroupTag;
      entry.groupIdx = gInt;
      entry.subPage = activeSubPage;
      entry.accent =
          defs[static_cast<size_t>(activeSubPage)].glow ==
                  MidasTactileButton::GlowMode::Gold
              ? midas_colors::kAccentGold
              : (defs[static_cast<size_t>(activeSubPage)].glow ==
                         MidasTactileButton::GlowMode::Green
                     ? midas_colors::kAccentGreen
                     : (defs[static_cast<size_t>(activeSubPage)].glow ==
                                MidasTactileButton::GlowMode::Purple
                            ? midas_colors::kAccentPurple
                            : midas_colors::kAccentCyan));
      activePageAccent = entry.accent;
      recentEntries.erase(
          std::remove_if(recentEntries.begin(), recentEntries.end(),
                         [&](const auto &e) {
                           return e.groupIdx == entry.groupIdx &&
                                  e.subPage == entry.subPage;
                         }),
          recentEntries.end());
      recentEntries.insert(recentEntries.begin(), entry);
      if (recentEntries.size() > 6)
        recentEntries.resize(6);
    }

    btnNavFav.setToggleState(isCurrentPageFavorite(),
                             juce::dontSendNotification);
    resized();
    repaint();
  }

  juce::String getActiveBreadcrumb() const { return activeBreadcrumbText; }

  bool isGroupButtonBarVisible() const {
    return groupButtons[0] != nullptr && groupButtons[0]->isVisible();
  }

  bool isCurrentPageFavorite() const {
    const int g = static_cast<int>(activeGroup);
    for (const auto &f : favoriteEntries) {
      if (f.groupIdx == g && f.subPage == activeSubPage)
        return true;
    }
    return false;
  }

  void toggleFavoriteForPage(int g, int sub) {
    auto it = std::find_if(favoriteEntries.begin(), favoriteEntries.end(),
                           [&](const auto &f) {
                             return f.groupIdx == g && f.subPage == sub;
                           });
    if (it != favoriteEntries.end()) {
      favoriteEntries.erase(it);
    } else {
      AndroidRecentWorkspaceView::Entry entry;
      const int catIdx = AndroidHomeLauncherView::findCatalogIndex(g, sub);
      if (catIdx >= 0) {
        const auto &def = AndroidHomeLauncherView::getCatalog()[static_cast<size_t>(catIdx)];
        entry.title = def.title;
        entry.groupLabel = def.groupName;
        entry.accent = juce::Colour(def.colorHex);
      } else {
        entry.title = activePageTitle;
        entry.groupLabel = activeGroupTag;
        entry.accent = activePageAccent;
      }
      entry.groupIdx = g;
      entry.subPage = sub;
      favoriteEntries.insert(favoriteEntries.begin(), entry);
      if (favoriteEntries.size() > 6)
        favoriteEntries.resize(6);
    }
    btnNavFav.setToggleState(isCurrentPageFavorite(),
                             juce::dontSendNotification);
    syncLauncherFavorites();
    if (recentView.isVisible())
      recentView.setFavorites(favoriteEntries);
  }

  void toggleCurrentPageFavorite() {
    toggleFavoriteForPage(static_cast<int>(activeGroup), activeSubPage);
  }

  AndroidHomeLauncherView &getHomeLauncherView() { return homeLauncherView; }
  DynamicsPanel &getDynamicsPanel() { return dynamicsPanelView; }
  MidasDedicatedDspPanel &getDedicatedDspPanel() { return dedicatedDsp; }

  void switchTouchTab(int index) {
    if (index == 0)
      selectNav(MainNavGroup::Mix, 0); // Overview
    else if (index == 1)
      selectNav(MainNavGroup::Mix, 1); // Equaliser (PEQ & DYN)
    else if (index == 2)
      selectNav(MainNavGroup::Mix, 2); // Comp & Gate
    else if (index == 3)
      selectNav(MainNavGroup::Routing, 2); // VCA Groups
    else if (index == 4)
      selectNav(MainNavGroup::Routing, 3); // Bus & Direct
    else if (index == 5)
      selectNav(MainNavGroup::Routing, 0); // Patch Matrix
    else if (index == 6)
      selectNav(MainNavGroup::Fx, 0); // 24-Slot FX Rack
    else if (index == 7)
      selectNav(MainNavGroup::System, 1); // Audio Hardware
    else if (index == 8)
      selectNav(MainNavGroup::Show, 1); // 64-Ch Recorder
    else if (index == 9)
      selectNav(MainNavGroup::Routing, 0); // Patchbay Matrix
    else if (index == 10)
      selectNav(MainNavGroup::Show, 0); // Cue List
    else if (index == 11)
      selectNav(MainNavGroup::System, 4); // Console Setup
  }

  void openChannelAiModal() {
    const int chIdx = getEngineChannelForSlot(selectedSlot);
    juce::String chName = "IN " + juce::String(chIdx + 1);
    if (selectedSlot >= 0 && selectedSlot < 8)
      chName = strips[static_cast<size_t>(selectedSlot)]->getName();
    aiModal.setTargetChannel(chIdx, chName);
    selectNav(MainNavGroup::Mix, 3);
  }

  void setCueListCallbacks(CueListPanel::CueRecallCallback recallCue,
                           CueListPanel::StopRecallCallback stopRecall = {}) {
    fullCueListView.setCallbacks(std::move(recallCue), std::move(stopRecall));
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    // Console Chassis Background
    g.setColour(midas_colors::kChassisBg);
    g.fillAll();

    // Upper Section: Touchscreen Deck (Left) & Dedicated DSP Deck (Right)
    const float topY = 38.0f;
    const float upperH = area.getHeight() * 0.42f;
    const bool isWideTouchGroup = false;
    const float normalTouchW = (area.getWidth() - 24.0f) * 0.63f;
    const float touchW =
        isWideTouchGroup ? (area.getWidth() - 16.0f) : normalTouchW;

    // Touchscreen Glass Bezel
    auto touchRect = juce::Rectangle<float>(8.0f, topY, touchW, upperH);
    g.setColour(juce::Colour(0xff05090f));
    g.fillRoundedRectangle(touchRect, 10.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(touchRect, 10.0f, 1.2f);

    // Persistent Location Breadcrumb Pill (Single Clean Header Row)
    if (breadcrumbBounds.getWidth() > 54) {
      const bool inHome = homeLauncherView.isVisible();
      const bool inRecent = recentView.isVisible();
      const juce::Colour pillAcc =
          inHome ? midas_colors::kAccentGold
                 : (inRecent ? midas_colors::kAccentPurple : activePageAccent);
      const juce::String pillText =
          inHome ? "HOME  >  DIRECTORY & SEARCH"
                 : (inRecent ? "FAVORITES & RECENT WORKSPACE"
                             : activeBreadcrumbText);
      auto bRect = breadcrumbBounds.toFloat();
      g.setColour(juce::Colour(0xff0c1524));
      g.fillRoundedRectangle(bRect, 5.0f);
      g.setColour(pillAcc.withAlpha(0.45f));
      g.drawRoundedRectangle(bRect, 5.0f, 1.0f);
      g.setFont(midas_ui::getFont(9.5f, true));
      g.setColour(pillAcc);
      g.drawText(pillText, breadcrumbBounds.reduced(8, 0),
                 juce::Justification::centredLeft, true);
    }

    // Chassis Corner Screws (Reference Hardware Touring Deck Inset)
    const float layerY = topY + upperH + 6.0f;
    const float lowerY = layerY + 34.0f;
    const float masterW = 120.0f;
    const float screwX = area.getWidth() - 8.0f - masterW - 6.0f;
    const float lowerH = area.getHeight() - lowerY - 22.0f - 68.0f - 8.0f;

    auto drawChassisScrew = [&](float cx, float cy) {
      g.setColour(juce::Colour(0xff060a12));
      g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
      g.setColour(juce::Colour(0xff1e2e42));
      g.drawEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.0f);
      g.setColour(juce::Colour(0xff33455c));
      g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
      g.setColour(juce::Colour(0xff090f18));
      g.drawLine(cx - 2.5f, cy, cx + 2.5f, cy, 1.2f);
      g.drawLine(cx, cy - 2.5f, cx, cy + 2.5f, 1.2f);
    };

    drawChassisScrew(screwX, lowerY + 12.0f);
    drawChassisScrew(screwX, lowerY + lowerH - 12.0f);
  }

  void resized() override {
    if (strips[7] == nullptr || groupButtons[4] == nullptr)
      return;

    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Top status bar
    statusBar.setBounds(8, 4, static_cast<int>(w - 16.0f), 30);

    // 2. Upper section
    const float topY = 38.0f;
    const float upperH = h * 0.42f;

    const bool isWideTouchGroup = false;

    const float normalTouchW = (w - 24.0f) * 0.63f;
    const float dspW = (w - 24.0f) - normalTouchW;
    const float touchW = isWideTouchGroup ? (w - 32.0f) : normalTouchW;

    dedicatedDsp.setVisible(!isWideTouchGroup);

    // Single Clean Touchscreen Header: Location Breadcrumb (Left) +
    // Back/Home/Find/Fav/Recent (Right); group & sub-tab bars hidden
    const float row1Y = topY + 4.0f;
    const float rowH = 22.0f;
    const float navAreaW = 262.0f;
    const float navX = touchW - navAreaW + 2.0f;

    btnNavMenu.setVisible(false);
    btnNavMenu.setBounds(0, 0, 0, 0);
    for (int i = 0; i < 5; ++i) {
      if (groupButtons[i] != nullptr) {
        groupButtons[i]->setVisible(false);
        groupButtons[i]->setBounds(0, 0, 0, 0);
      }
    }
    for (size_t i = 0; i < subTabButtons.size(); ++i) {
      if (subTabButtons[i] != nullptr) {
        subTabButtons[i]->setVisible(false);
        subTabButtons[i]->setBounds(0, 0, 0, 0);
      }
    }

    btnNavBack.setBounds(static_cast<int>(navX), static_cast<int>(row1Y), 52,
                         static_cast<int>(rowH));
    btnNavHome.setBounds(static_cast<int>(navX + 55.0f),
                         static_cast<int>(row1Y), 46, static_cast<int>(rowH));
    btnNavSearch.setBounds(static_cast<int>(navX + 104.0f),
                           static_cast<int>(row1Y), 46,
                           static_cast<int>(rowH));
    btnNavFav.setBounds(static_cast<int>(navX + 153.0f),
                        static_cast<int>(row1Y), 48, static_cast<int>(rowH));
    btnNavRecent.setBounds(static_cast<int>(navX + 204.0f),
                           static_cast<int>(row1Y), 56,
                           static_cast<int>(rowH));

    const int bcX = 14;
    const int bcRight = static_cast<int>(navX - 8.0f);
    breadcrumbBounds =
        juce::Rectangle<int>(bcX, static_cast<int>(row1Y),
                             std::max(0, bcRight - bcX), static_cast<int>(rowH));

    // Touchscreen Swappable Views (All Embedded - Full Height Below Header)
    const float viewY = topY + 30.0f;
    const bool isFullHeightView = true;
    const float quickH = 0.0f;
    const float viewH = upperH - 34.0f;
    const float viewW = touchW - 16.0f;

    auto viewBounds =
        juce::Rectangle<int>(16, static_cast<int>(viewY),
                             static_cast<int>(viewW), static_cast<int>(viewH));
    overviewView.setBounds(viewBounds);
    dynamicEqView.setBounds(viewBounds);
    dynamicsPanelView.setBounds(viewBounds);
    aiModal.setBounds(viewBounds);
    dspModal.setBounds(viewBounds);
    duganView.setBounds(viewBounds);
    routingView.setBounds(viewBounds);
    vcaPanelView.setBounds(viewBounds);
    patchMatrixView.setBounds(viewBounds);
    surroundPanView.setBounds(viewBounds);
    gangModal.setBounds(viewBounds);
    fxHostView.setBounds(viewBounds);
    broadcastView.setBounds(viewBounds);
    mcloudModal.setBounds(viewBounds);
    homeLauncherView.setBounds(viewBounds);
    recentView.setBounds(viewBounds);
    quickSettingsPanel.setBounds(getLocalBounds());
    recorderView.setBounds(viewBounds);
    cuelistView.setBounds(viewBounds);
    fullCueListView.setBounds(viewBounds);
    offlineBounceView.setBounds(viewBounds);
    asioBridgeView.setBounds(viewBounds);
    oscServerView.setBounds(viewBounds);
    virtualCableView.setBounds(viewBounds);
    setupView.setBounds(viewBounds);
    if (audioDeviceSelector)
      audioDeviceSelector->setBounds(viewBounds);

    // Dual Split for Monitor & Talkback
    auto monBounds = viewBounds;
    auto talkBounds = monBounds.removeFromRight(
        static_cast<int>(viewBounds.getWidth() * 0.44f));
    monitorView.setBounds(monBounds.reduced(2));
    talkbackView.setBounds(talkBounds.reduced(2));

    // Quick channel strip
    quickStrip.setVisible(!isFullHeightView);
    if (!isFullHeightView) {
      quickStrip.setBounds(16, static_cast<int>(viewY + viewH + 4.0f),
                           static_cast<int>(viewW), static_cast<int>(quickH));
    }

    // Upper Right Dedicated Hardware DSP Panel
    if (!isWideTouchGroup) {
      dedicatedDsp.setBounds(static_cast<int>(8.0f + normalTouchW + 8.0f),
                             static_cast<int>(topY), static_cast<int>(dspW),
                             static_cast<int>(upperH));
    }

    // 3. Fader Layer Navigation Bar
    const float layerY = topY + upperH + 6.0f;
    layerBar.setBounds(8, static_cast<int>(layerY), static_cast<int>(w - 16.0f),
                       30);

    // 4. Lower Deck: 8 Strips + Master Strip & Master Processing Panel
    const float lowerY = layerY + 34.0f;
    const float armrestH = 22.0f;
    const float masterProcH = 68.0f;
    const float lowerH = h - lowerY - armrestH - masterProcH - 8.0f;

    const float masterW = 108.0f;
    const float masterBayGap = 6.0f;
    const float totalMasterBayW = masterW * 2.0f + masterBayGap;
    const float stripsAreaW = w - 16.0f - totalMasterBayW - 10.0f;
    const float stripW = (stripsAreaW - 7.0f * 4.0f) / 8.0f;

    for (int i = 0; i < 8; ++i) {
      const float sx = 8.0f + i * (stripW + 4.0f);
      if (strips[static_cast<size_t>(i)] != nullptr) {
        strips[static_cast<size_t>(i)]->setBounds(
            static_cast<int>(sx), static_cast<int>(lowerY),
            static_cast<int>(stripW), static_cast<int>(lowerH));
      }
    }

    const float masterX = w - 8.0f - totalMasterBayW;
    const float monitorX = masterX + masterW + masterBayGap;

    masterStrip.setBounds(static_cast<int>(masterX), static_cast<int>(lowerY),
                          static_cast<int>(masterW), static_cast<int>(lowerH));

    monitorStrip.setBounds(static_cast<int>(monitorX), static_cast<int>(lowerY),
                           static_cast<int>(masterW), static_cast<int>(lowerH));

    // Master Stereo Processing Panel (below strips)
    masterProcessing.setBounds(8, static_cast<int>(lowerY + lowerH + 4.0f),
                               static_cast<int>(w - 16.0f),
                               static_cast<int>(masterProcH));

    // 5. Luxury Mahogany Armrest
    armrest.setBounds(0, static_cast<int>(h - armrestH), static_cast<int>(w),
                      static_cast<int>(armrestH));
  }

private:
  int getEngineChannelForSlot(int slot) const {
    const int spillVca = engine.activeSpillVca.load(std::memory_order_relaxed);
    if (spillVca >= 1 && spillVca <= 24) {
      int matchIdx = 0;
      for (int c = 0; c < static_cast<int>(engine.channels.size()); ++c) {
        if (engine.channels[static_cast<size_t>(c)].vcaGroup.load(
                std::memory_order_relaxed) == spillVca) {
          if (matchIdx == slot)
            return c;
          ++matchIdx;
        }
      }
      return -1;
    }
    if (activePopGroup >= 1 && activePopGroup <= 4) {
      int matchIdx = 0;
      int assignedCount = 0;
      for (int c = 0; c < static_cast<int>(engine.channels.size()); ++c) {
        if (engine.channels[static_cast<size_t>(c)].popGroup.load(
                std::memory_order_relaxed) == activePopGroup) {
          if (matchIdx == slot)
            return c;
          ++matchIdx;
          ++assignedCount;
        }
      }
      if (assignedCount > 0)
        return -1;

      if (activePopGroup == 1) { // POP 1: DRUMS fallback
        static const int drumChs[8] = {0, 1, 2, 8, 9, 10, 11, 3};
        return drumChs[juce::jlimit(0, 7, slot)];
      }
      if (activePopGroup == 2) { // POP 2: BASS & RHYTHM fallback
        static const int bassChs[8] = {3, 0, 1, 12, 4, 5, 6, 7};
        return bassChs[juce::jlimit(0, 7, slot)];
      }
      if (activePopGroup == 3) { // POP 3: GUITARS & KEYS fallback
        static const int gtrChs[8] = {4, 5, 6, 12, 13, 3, 7, 14};
        return gtrChs[juce::jlimit(0, 7, slot)];
      }
      static const int voxChs[8] = {7, 14, 15, 6, 13, 4, 5, 0};
      return voxChs[juce::jlimit(0, 7, slot)];
    }
    if (currentBank == 0)
      return juce::jlimit(0, nova::kLogicalChannels - 1,
                          inputRangeOffset + slot);
    if (currentBank == 1)
      return juce::jlimit(0, nova::kLogicalChannels - 1,
                          inputRangeOffset + slot + 8);
    return juce::jlimit(0, nova::kLogicalChannels - 1, slot);
  }

  void setupInteractions() {
    auto openDynamicsSubView = [this](bool showGate) {
      dynamicsPanelView.setGateViewActive(showGate);
      selectNav(MainNavGroup::Mix, 2);
      dedicatedDsp.setFocusMode(
          showGate ? MidasDedicatedDspPanel::FocusMode::Gate
                   : MidasDedicatedDspPanel::FocusMode::Comp);
      selectChannel(selectedSlot);
    };

    // Quick Tile interactions
    quickStrip.onQuickTileClicked = [this, openDynamicsSubView](int tabIdx) {
      if (tabIdx == 0)
        selectNav(MainNavGroup::Mix, 1); // Equaliser (PEQ & DYN)
      else if (tabIdx == 1)
        openDynamicsSubView(false); // Dynamics (Comp)
      else if (tabIdx == 2)
        openDynamicsSubView(true); // Dynamics (Gate)
      else if (tabIdx == 3)
        selectNav(MainNavGroup::Routing, 3); // Bus & Direct Routing
      else
        switchTouchTab(tabIdx);
    };
    quickStrip.onPreampChanged = [this](float val) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size())) {
        const float prevGain =
            engine.channels[static_cast<size_t>(ch)].inputGainDb.load(
                std::memory_order_relaxed);
        const float deltaGain = val - prevGain;
        engine.channels[static_cast<size_t>(ch)].inputGainDb.store(
            val, std::memory_order_relaxed);
        if (const auto *grp = findLinkGroupForChannel(ch)) {
          if (grp->linkPreamp) {
            for (int c = 0; c < static_cast<int>(engine.channels.size()); ++c) {
              if (c != ch && grp->members[static_cast<size_t>(c)]) {
                auto &dst = engine.channels[static_cast<size_t>(c)];
                const float nextGain =
                    grp->isRelative
                        ? juce::jlimit(
                              -24.0f, 60.0f,
                              dst.inputGainDb.load(std::memory_order_relaxed) +
                                  deltaGain)
                        : val;
                dst.inputGainDb.store(nextGain, std::memory_order_relaxed);
              }
            }
          }
        }
      }
    };
    quickStrip.onHpfChanged = [this](bool en, float freq) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(ch)].highPassEnabled.store(
            en, std::memory_order_relaxed);
        engine.channels[static_cast<size_t>(ch)].highPassFreqHz.store(
            freq, std::memory_order_relaxed);
      }
    };
    quickStrip.onDelayChanged = [this](bool en, float ms) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(ch)].inputDelayMs.store(
            en ? ms : 0.0f, std::memory_order_relaxed);
      }
    };
    quickStrip.onPanChanged = [this](float pan) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size()))
        engine.channels[static_cast<size_t>(ch)].pan.store(
            pan, std::memory_order_relaxed);
    };
    quickStrip.on48vToggled = [this](bool p48) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size()))
        engine.channels[static_cast<size_t>(ch)].phantom48v.store(
            p48, std::memory_order_relaxed);
    };
    quickStrip.onPhaseToggled = [this](bool phase) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size()))
        engine.channels[static_cast<size_t>(ch)].polarityInverted.store(
            phase, std::memory_order_relaxed);
    };

    // Dedicated DSP HW Module Buttons & Monitor/Talkback/Macros (Fase 4)
    dedicatedDsp.onHwModuleSelected = [this, openDynamicsSubView](int modIdx) {
      if (modIdx == 0)
        selectNav(MainNavGroup::Mix, 1); // Equaliser (PEQ & DYN)
      else if (modIdx == 1)
        openDynamicsSubView(false); // Dynamics (Compressor)
      else if (modIdx == 2)
        openDynamicsSubView(true); // Dynamics (Gate)
    };
    dedicatedDsp.onEqGainChanged = [this](int band, float gain) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size()) &&
          band >= 0 && band < 4) {
        engine.channels[static_cast<size_t>(ch)]
            .eqGainDb[static_cast<size_t>(band)]
            .store(gain, std::memory_order_relaxed);
        int partnerCh = -1;
        if (engine.isChannelStereoLinked(ch, &partnerCh) && partnerCh >= 0 &&
            partnerCh < static_cast<int>(engine.channels.size())) {
          engine.channels[static_cast<size_t>(partnerCh)]
              .eqGainDb[static_cast<size_t>(band)]
              .store(gain, std::memory_order_relaxed);
        }
        selectChannel(selectedSlot);
      }
    };
    dedicatedDsp.onDynChanged = [this](float thr, float ratio) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size())) {
        auto &st = engine.channels[static_cast<size_t>(ch)];
        if (dedicatedDsp.getFocusMode() ==
            MidasDedicatedDspPanel::FocusMode::Gate) {
          st.gateThresholdDb.store(juce::jlimit(-80.0f, 0.0f, thr),
                                   std::memory_order_relaxed);
          st.gateRatio.store(juce::jlimit(1.0f, 10.0f, ratio),
                             std::memory_order_relaxed);
        } else {
          st.compressorThresholdDb.store(juce::jlimit(-60.0f, 0.0f, thr),
                                         std::memory_order_relaxed);
          st.compressorRatio.store(juce::jlimit(1.0f, 20.0f, ratio),
                                   std::memory_order_relaxed);
        }
        selectChannel(selectedSlot);
      }
    };
    dedicatedDsp.onMonitorChanged = [this](float studioDb, float phonesDb) {
      engine.monitorLevelA.store(studioDb, std::memory_order_relaxed);
      engine.monitorLevelB.store(phonesDb, std::memory_order_relaxed);
    };
    dedicatedDsp.onTalkbackToggled = [this](bool tbActive) {
      engine.talkbackActive.store(tbActive, std::memory_order_relaxed);
    };
    dedicatedDsp.onShoutIemToggled = [this](bool shoutActive) {
      engine.talkbackActive.store(shoutActive, std::memory_order_relaxed);
      engine.talkbackDuckerDb.store(shoutActive ? -12.0f : -20.0f,
                                    std::memory_order_relaxed);
    };
    dedicatedDsp.onSipSafeToggled = [this](bool safe) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(ch)].soloSafe.store(
            safe, std::memory_order_relaxed);
        engine.channels[static_cast<size_t>(ch)].muteSafe.store(
            safe, std::memory_order_relaxed);
      }
    };
    dedicatedDsp.onSoloModeToggled = [this](bool isPfl) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch >= 0 && ch < static_cast<int>(engine.channels.size()))
        engine.channels[static_cast<size_t>(ch)].soloPfl.store(
            isPfl, std::memory_order_relaxed);
    };
    dedicatedDsp.onMacroKnobChanged = [this](int macroIdx, float val) {
      const int ch = getEngineChannelForSlot(selectedSlot);
      if (ch < 0 || ch >= static_cast<int>(engine.channels.size()))
        return;
      auto &st = engine.channels[static_cast<size_t>(ch)];
      if (macroIdx == 0) { // FX1 Send %
        const float sendDb = (val <= 1.0f) ? -60.0f : (-40.0f + val * 0.45f);
        st.auxSendDb[4].store(sendDb, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      } else if (macroIdx == 1) { // SUB Warmth
        st.eqGainDb[0].store((val - 50.0f) * 0.18f, std::memory_order_relaxed);
      } else if (macroIdx == 2) { // AIR Brilliance
        st.eqGainDb[3].store((val - 30.0f) * 0.15f, std::memory_order_relaxed);
      } else if (macroIdx == 3) { // TRIM dB
        st.inputGainDb.store(val, std::memory_order_relaxed);
      }
      selectChannel(selectedSlot);
    };




    // Multitrack Recorder bindings (Transport & Dedicated USB Button)
    auto handleRecToggle = [this](bool startRec) {
      if (startRec) {
        const auto storageDir = recorderView.getTargetDirectory();
        if (!storageDir.exists())
          storageDir.createDirectory();
        const auto now = juce::Time::getCurrentTime();
        const bool is64Ch = recorderView.is64ChannelMode();
        const auto prefix =
            is64Ch ? "OpenLive_64Track_RAW_" : "OpenLive_Stereo_Master_";
        const auto filename = prefix + now.formatted("%Y%m%d_%H%M%S") + ".wav";
        const auto recFile = storageDir.getChildFile(filename);
        double sr = 96000.0;
        if (auto *dev = deviceManager.getCurrentAudioDevice()) {
          sr = dev->getCurrentSampleRate();
          if (sr < 8000.0 || sr > 384000.0)
            sr = 96000.0;
        }
        const int chCount = is64Ch ? 64 : 2;
        if (engine.recorder.startRecording(recFile, sr, chCount)) {
          const auto status =
              (is64Ch ? "RECORDING 64-CH RAW: " : "RECORDING MASTER: ") +
              filename;
          recorderView.setRecordingState(true, status);
          dedicatedDsp.setUsbRecState(true,
                                      is64Ch ? "● 64-CH REC" : "● REC ACTIVE");
        }
      } else {
        engine.recorder.stopRecording();
        recorderView.setRecordingState(false, "RECORDER STOPPED");
        dedicatedDsp.setUsbRecState(false, "USB READY");
      }
    };
    recorderView.onRecordToggled = handleRecToggle;
    dedicatedDsp.onUsbRecToggled = handleRecToggle;
    recorderView.onStopClicked = [this] {
      if (engine.recorder.isRecording()) {
        engine.recorder.stopRecording();
        recorderView.setRecordingState(false, "RECORDER STOPPED");
        dedicatedDsp.setUsbRecState(false, "USB READY");
      }
      engine.stopPlayback();
      recorderView.setPlaybackState(false);
    };
    recorderView.onPlayToggled = [this](bool playActive) {
      if (playActive)
        engine.playPlayback();
      else
        engine.stopPlayback();
    };
    recorderView.onLoadWavClicked = [this] {
      if (onLoadWav)
        onLoadWav();
    };
    recorderView.onOpenDawStream = [this] { selectNav(MainNavGroup::Show, 1); };
    recorderView.onOpenBounce = [this] { selectNav(MainNavGroup::Show, 2); };
    recorderView.onRtzClicked = [this] {
      recorderView.setTimecode("00:00:00:00");
      recorderView.setProgress(0.0f);
    };

    // Signal Generator & Setup bindings (Tab 12: Setup)
    setupView.onOscTypeChanged = [this](int type) {
      engine.oscType.store(type, std::memory_order_relaxed);
    };
    setupView.onOscFreqChanged = [this](float freq) {
      engine.oscFrequency.store(freq, std::memory_order_relaxed);
    };
    setupView.onOscLevelChanged = [this](float lvl) {
      engine.oscLevelDb.store(lvl, std::memory_order_relaxed);
    };
    setupView.onOscInjectMainToggled = [this](bool inject) {
      engine.oscRouteMain.store(inject, std::memory_order_relaxed);
      engine.oscEnabled.store(
          inject || engine.oscRouteAux[0].load(std::memory_order_relaxed),
          std::memory_order_relaxed);
    };
    setupView.onOscInjectAuxToggled = [this](bool inject) {
      for (int a = 0; a < 2; ++a)
        engine.oscRouteAux[static_cast<size_t>(a)].store(
            inject, std::memory_order_relaxed);
      engine.oscEnabled.store(
          inject || engine.oscRouteMain.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
    };

    // Direct Routing Matrix bindings (Tab 5: Routing)
    routingView.onTileToggled = [this](int tileIdx, bool state) {
      const int chIdx = getEngineChannelForSlot(selectedSlot);
      if (chIdx < 0 || chIdx >= static_cast<int>(engine.channels.size()))
        return;
      auto &ch = engine.channels[static_cast<size_t>(chIdx)];
      if (tileIdx == 0) { // STEREO MAIN L/R
        ch.masterAssigned.store(state, std::memory_order_relaxed);
      } else if (tileIdx == 1) { // MONITOR WASAPI 2
        ch.monitorAssigned.store(state, std::memory_order_relaxed);
      } else if (tileIdx == 2) { // MON PRE FADER
        ch.monitorPreFader.store(state, std::memory_order_relaxed);
      } else if (tileIdx == 3) { // MIX 1-2 IEM VOX
        ch.auxSendDb[0].store(state ? 0.0f : -100.0f,
                              std::memory_order_relaxed);
        ch.auxSendDb[1].store(state ? 0.0f : -100.0f,
                              std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      } else if (tileIdx == 4) { // MIX 3-4 WEDGE
        ch.auxSendDb[2].store(state ? 0.0f : -100.0f,
                              std::memory_order_relaxed);
        ch.auxSendDb[3].store(state ? 0.0f : -100.0f,
                              std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      } else if (tileIdx == 5) { // FX 1-2 RACK
        ch.auxSendDb[4].store(state ? 0.0f : -100.0f,
                              std::memory_order_relaxed);
        ch.auxSendDb[5].store(state ? 0.0f : -100.0f,
                              std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      } else if (tileIdx == 6) { // VCA 1 DRUMS
        ch.vcaGroup.store(state ? 1 : 0, std::memory_order_relaxed);
      } else if (tileIdx == 7) { // MUTE SAFE
        ch.muteSafe.store(state, std::memory_order_relaxed);
      }
      syncVcaAndMatrixViews();
    };
    routingView.onOpenSurroundPan = [this] {
      selectNav(MainNavGroup::Routing, 1);
    };
    routingView.onOpenDugan = [this] { selectNav(MainNavGroup::Mix, 4); };
    routingView.onOpenManchino = [this] {
      selectNav(MainNavGroup::Routing, 0);
    };

    // Fase 3 & 5: Manic VCA Membership & Motorized DCA Engine (Tab 5)

    // Fase 3 & 5: Quad Stereo FX Rack Engines & Dedicated Hardware FX Editors
    // (Tab 7)
    {
      using nova::graviton::FxType;
      struct DefaultSlotCfg {
        size_t idx;
        FxType type;
        int auxSrc;
      };
      static constexpr DefaultSlotCfg kDefaults[24] = {
          {0, FxType::Reverb_VSS4, 0},
          {1, FxType::StereoDelay, 1},
          {2, FxType::EMT140_Plate, 2},
          {3, FxType::MasterX_Multiband, 3},
          {4, FxType::SpringReverb, 4},
          {5, FxType::PitchShifter, 5},
          {6, FxType::TC_2290_Delay, 6},
          {7, FxType::TC_SCF, 7},
          {8, FxType::DVR2_NonLinear, 8},
          {9, FxType::KT_DN780, 9},
          {10, FxType::DualTank_Spring, 10},
          {11, FxType::Chorus, 11},
          {12, FxType::Flanger, 12},
          {13, FxType::Phaser, 13},
          {14, FxType::SubHarmonic_Synth, 14},
          {15, FxType::Precision_DeEsser, 15},
          {16, FxType::Convolution_Reverb, 16},
          {17, FxType::Hybrid_Reverb, 17},
          {18, FxType::DN360_InductorGEQ, 18},
          {19, FxType::TC_NonLin2, 19},
          {20, FxType::Distortion, 20},
          {21, FxType::NoiseGate, 21},
          {22, FxType::Compressor, 22},
          {23, FxType::ASIO_Bridge, 23}};
      for (const auto &cfg : kDefaults) {
        auto &sl = engine.effectsRack.getSlot(cfg.idx);
        if (sl.type.load(std::memory_order_relaxed) ==
            static_cast<int>(FxType::None)) {
          sl.type.store(static_cast<int>(cfg.type), std::memory_order_relaxed);
          sl.auxSendSource.store(cfg.auxSrc, std::memory_order_relaxed);
        }
      }
    }
    onOpenFxRack = [this] { selectNav(MainNavGroup::Fx, 0); };

    // Fase 5: Broadcast Talkback & Control Room Monitor Panels (Tab 8)
    broadcastView.onOpenTalkback = [this] {
      selectNav(MainNavGroup::Mix, 5);
    };
    broadcastView.onOpenMonitorCr = [this] { selectNav(MainNavGroup::Mix, 5); };


    // Fase 3: Theatre Cue List & Scene Automation (Tab 11 & Top Status Bar)
    cuelistView.onCueRecalled = [this](int sceneId) {
      applyConsoleScene(sceneId);
    };
    cuelistView.onOpenCueManager = [this] { selectNav(MainNavGroup::Show, 0); };
    statusBar.onSceneChanged = [this](int sceneId) {
      applyConsoleScene(sceneId);
    };

    // Fase 4: Sends on Fader (SOF) & Layer Navigation / POP Groups / Automation
    // Glide
    statusBar.onSofModeChanged = [this](int mode) {
      currentSofMode = mode;
      sofActive = (mode != 0);
      if (mode == 1) { // SOF: MONITOR
        for (int i = 0; i < 8; ++i) {
          const int chIdx = getEngineChannelForSlot(i);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
            strips[static_cast<size_t>(i)]->setSofMonitorMode(
                true, ch.monitorPreFader.load(std::memory_order_relaxed));
            strips[static_cast<size_t>(i)]->syncBusOrVcaState(
                ch.monitorSendDb.load(std::memory_order_relaxed),
                !ch.monitorAssigned.load(std::memory_order_relaxed),
                ch.peak.load(std::memory_order_relaxed));
          }
        }
      } else if (mode == 2) { // SOF: AUX 1-2
        for (int i = 0; i < 8; ++i) {
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
          const int chIdx = getEngineChannelForSlot(i);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
            const float sendDb =
                ch.auxSendDb[0].load(std::memory_order_relaxed);
            const float pk = ch.peak.load(std::memory_order_relaxed);
            strips[static_cast<size_t>(i)]->syncBusOrVcaState(sendDb, false,
                                                              pk);
          }
        }
      } else { // SOF: OFF
        for (int i = 0; i < 8; ++i) {
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        }
        rebankStrips(currentBank);
      }
      repaint();
    };
    statusBar.onSofToggled = [this](bool active) {
      sofActive = active;
      if (!sofActive) {
        currentSofMode = 0;
        for (int i = 0; i < 8; ++i) {
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        }
        rebankStrips(currentBank);
      }
      repaint();
    };

    layerBar.onBankSelected = [this](int bank) {
      activePopGroup = 0;
      sofActive = false;
      statusBar.setSofActive(false);
      rebankStrips(bank);
      selectChannel(selectedSlot);
      repaint();
    };
    layerBar.onPopGroupSelected = [this](int popIdx) {
      activePopGroup = popIdx;
      engine.activeSpillPop.store(popIdx, std::memory_order_relaxed);
      rebankStrips(currentBank);
      selectChannel(selectedSlot);
      repaint();
    };
    layerBar.onInputRangeChanged = [this](int offset) {
      inputRangeOffset =
          juce::jlimit(0, std::max(0, nova::kLogicalChannels - 16), offset);
      rebankStrips(currentBank);
      selectChannel(selectedSlot);
      repaint();
    };
    layerBar.onFadeInTriggered = [this] { faderGlideMode = 1; };
    layerBar.onFadeOutTriggered = [this] { faderGlideMode = -1; };
    layerBar.onLinkPairToggled = [this](bool linked) {
      link56Active = linked;
      engine.setStereoLink(2, linked);
      selectChannel(selectedSlot);
    };

    layerBar.onGeqOnFadersToggled = [this](bool active) {
      engine.graphicEqEnabled.store(active, std::memory_order_relaxed);
      if (active && onOpenDN360)
        onOpenDN360();
    };

    // Fader strip selections & parameter bindings (Fase 4: SOF / Bus / VCA /
    // Gang / Link 5-6)
    for (int i = 0; i < 8; ++i) {
      strips[static_cast<size_t>(i)]->onSelected = [this](int slot) {
        selectChannel(slot);
      };
      strips[static_cast<size_t>(i)]->onFaderChanged = [this](int slot,
                                                              float db) {
        faderGlideMode = 0; // Manual touch overrides automation glide
        if (currentSofMode ==
            1) { // SOF: MONITOR (Controls channel monitor send level)
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)].monitorSendDb.store(
                db, std::memory_order_relaxed);
          }
          return;
        }
        if (currentSofMode ==
            2) { // SOF: MIX 1-2 (Controls each channel send to Mix 1-2)
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)].auxSendDb[0].store(
                db, std::memory_order_relaxed);
            engine.channels[static_cast<size_t>(chIdx)].auxSendDb[1].store(
                db, std::memory_order_relaxed);
            engine.syncAuxRoutingSnapshot();
          }
          return;
        }
        if (currentBank ==
            2) { // Bank 2: MIX BUS / AUX Masters (Aux 1-8 Masters)
          engine.auxMasterDb[static_cast<size_t>(slot)].store(
              db, std::memory_order_relaxed);
          engine.syncAuxRoutingSnapshot();
          return;
        }
        if (currentBank == 3) {
          engine.vcaMasterDb[static_cast<size_t>(slot)].store(
              db, std::memory_order_relaxed);
          syncVcaAndMatrixViews();
          return;
        }

        const int chIdx = getEngineChannelForSlot(slot);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const float prevDb =
              engine.channels[static_cast<size_t>(chIdx)].gainDb.load(
                  std::memory_order_relaxed);
          const float deltaDb = db - prevDb;
          engine.channels[static_cast<size_t>(chIdx)].gainDb.store(
              db, std::memory_order_relaxed);

          // Stereo Link (Pair 5-6 or any stereo-linked pair in MixerEngine)
          int partnerCh = -1;
          if (engine.isChannelStereoLinked(chIdx, &partnerCh) &&
              partnerCh >= 0 &&
              partnerCh < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(partnerCh)].gainDb.store(
                db, std::memory_order_relaxed);
          } else if (link56Active && (slot == 4 || slot == 5)) {
            const int partnerSlot = (slot == 4) ? 5 : 4;
            const int pCh = getEngineChannelForSlot(partnerSlot);
            if (pCh >= 0 && pCh < static_cast<int>(engine.channels.size())) {
              engine.channels[static_cast<size_t>(pCh)].gainDb.store(
                  db, std::memory_order_relaxed);
            }
          }

          // Link Group Fader (ABSOLUTE / RELATIVE across CH 1-144)
          if (const auto *grp = findLinkGroupForChannel(chIdx)) {
            if (grp->linkFader) {
              for (int c = 0; c < static_cast<int>(engine.channels.size());
                   ++c) {
                if (c != chIdx && grp->members[static_cast<size_t>(c)]) {
                  auto &dst = engine.channels[static_cast<size_t>(c)];
                  const float nextDb =
                      grp->isRelative
                          ? juce::jlimit(
                                -90.0f, 10.0f,
                                dst.gainDb.load(std::memory_order_relaxed) +
                                    deltaDb)
                          : db;
                  dst.gainDb.store(nextDb, std::memory_order_relaxed);
                }
              }
              for (int s = 0; s < 8; ++s) {
                if (s != slot) {
                  const int visCh = getEngineChannelForSlot(s);
                  if (visCh >= 0 && visCh < nova::kLogicalChannels &&
                      grp->members[static_cast<size_t>(visCh)]) {
                    strips[static_cast<size_t>(s)]->syncState(
                        engine.channels[static_cast<size_t>(visCh)]);
                  }
                }
              }
            }
          }
        }
      };
      strips[static_cast<size_t>(i)]->onMuteChanged = [this](int slot,
                                                             bool muted) {
        if (sofActive &&
            currentSofMode ==
                1) { // SOF: MONITOR (Mute button acts as Monitor Send Gate)
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)].monitorAssigned.store(
                !muted, std::memory_order_relaxed);
          }
          return;
        }
        if (currentBank == 2 && !sofActive) {
          engine.auxMuted[static_cast<size_t>(slot)].store(
              muted, std::memory_order_relaxed);
          return;
        }
        if (currentBank == 3 && !sofActive) {
          engine.vcaMuted[static_cast<size_t>(slot)].store(
              muted, std::memory_order_relaxed);
          syncVcaAndMatrixViews();
          return;
        }
        const int chIdx = getEngineChannelForSlot(slot);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          engine.channels[static_cast<size_t>(chIdx)].muted.store(
              muted, std::memory_order_relaxed);
          int partnerCh = -1;
          if (engine.isChannelStereoLinked(chIdx, &partnerCh) &&
              partnerCh >= 0 &&
              partnerCh < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(partnerCh)].muted.store(
                muted, std::memory_order_relaxed);
          }
          if (const auto *grp = findLinkGroupForChannel(chIdx)) {
            if (grp->linkMute) {
              for (int c = 0; c < static_cast<int>(engine.channels.size());
                   ++c) {
                if (c != chIdx && grp->members[static_cast<size_t>(c)]) {
                  engine.channels[static_cast<size_t>(c)].muted.store(
                      muted, std::memory_order_relaxed);
                }
              }
              for (int s = 0; s < 8; ++s) {
                if (s != slot) {
                  const int visCh = getEngineChannelForSlot(s);
                  if (visCh >= 0 && visCh < nova::kLogicalChannels &&
                      grp->members[static_cast<size_t>(visCh)]) {
                    strips[static_cast<size_t>(s)]->syncState(
                        engine.channels[static_cast<size_t>(visCh)]);
                  }
                }
              }
            }
          }
        }
      };
      strips[static_cast<size_t>(i)]->onSoloChanged = [this](int slot,
                                                             bool solo) {
        const int chIdx = getEngineChannelForSlot(slot);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          engine.channels[static_cast<size_t>(chIdx)].solo.store(
              solo, std::memory_order_relaxed);
          int partnerCh = -1;
          if (engine.isChannelStereoLinked(chIdx, &partnerCh) &&
              partnerCh >= 0 &&
              partnerCh < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(partnerCh)].solo.store(
                solo, std::memory_order_relaxed);
          }
          if (const auto *grp = findLinkGroupForChannel(chIdx)) {
            if (grp->linkSolo) {
              for (int c = 0; c < static_cast<int>(engine.channels.size());
                   ++c) {
                if (c != chIdx && grp->members[static_cast<size_t>(c)]) {
                  engine.channels[static_cast<size_t>(c)].solo.store(
                      solo, std::memory_order_relaxed);
                }
              }
              for (int s = 0; s < 8; ++s) {
                if (s != slot) {
                  const int visCh = getEngineChannelForSlot(s);
                  if (visCh >= 0 && visCh < nova::kLogicalChannels &&
                      grp->members[static_cast<size_t>(visCh)]) {
                    strips[static_cast<size_t>(s)]->syncState(
                        engine.channels[static_cast<size_t>(visCh)]);
                  }
                }
              }
            }
          }
        }
      };
      strips[static_cast<size_t>(i)]->onPanChanged = [this](int slot,
                                                            float val) {
        const int chIdx = getEngineChannelForSlot(slot);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const float prevPan =
              engine.channels[static_cast<size_t>(chIdx)].pan.load(
                  std::memory_order_relaxed);
          const float deltaPan = val - prevPan;
          engine.channels[static_cast<size_t>(chIdx)].pan.store(
              val, std::memory_order_relaxed);
          int partnerCh = -1;
          if (engine.isChannelStereoLinked(chIdx, &partnerCh) &&
              partnerCh >= 0 &&
              partnerCh < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(partnerCh)].pan.store(
                -val, std::memory_order_relaxed);
          }
          if (const auto *grp = findLinkGroupForChannel(chIdx)) {
            if (grp->linkPan) {
              for (int c = 0; c < static_cast<int>(engine.channels.size());
                   ++c) {
                if (c != chIdx && grp->members[static_cast<size_t>(c)]) {
                  auto &dst = engine.channels[static_cast<size_t>(c)];
                  const float nextPan =
                      grp->isRelative
                          ? juce::jlimit(
                                -1.0f, 1.0f,
                                dst.pan.load(std::memory_order_relaxed) +
                                    deltaPan)
                          : val;
                  dst.pan.store(nextPan, std::memory_order_relaxed);
                }
              }
              for (int s = 0; s < 8; ++s) {
                if (s != slot) {
                  const int visCh = getEngineChannelForSlot(s);
                  if (visCh >= 0 && visCh < nova::kLogicalChannels &&
                      grp->members[static_cast<size_t>(visCh)]) {
                    strips[static_cast<size_t>(s)]->syncState(
                        engine.channels[static_cast<size_t>(visCh)]);
                  }
                }
              }
            }
          }
        }
      };
      strips[static_cast<size_t>(i)]->onTrimChanged = [this](int slot,
                                                             float val) {
        const int chIdx = getEngineChannelForSlot(slot);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const float prevGain =
              engine.channels[static_cast<size_t>(chIdx)].inputGainDb.load(
                  std::memory_order_relaxed);
          const float deltaGain = val - prevGain;
          engine.channels[static_cast<size_t>(chIdx)].inputGainDb.store(
              val, std::memory_order_relaxed);
          int partnerCh = -1;
          if (engine.isChannelStereoLinked(chIdx, &partnerCh) &&
              partnerCh >= 0 &&
              partnerCh < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(partnerCh)].inputGainDb.store(
                val, std::memory_order_relaxed);
          }
          if (const auto *grp = findLinkGroupForChannel(chIdx)) {
            if (grp->linkPreamp) {
              for (int c = 0; c < static_cast<int>(engine.channels.size());
                   ++c) {
                if (c != chIdx && grp->members[static_cast<size_t>(c)]) {
                  auto &dst = engine.channels[static_cast<size_t>(c)];
                  const float nextGain =
                      grp->isRelative
                          ? juce::jlimit(-24.0f, 60.0f,
                                         dst.inputGainDb.load(
                                             std::memory_order_relaxed) +
                                             deltaGain)
                          : val;
                  dst.inputGainDb.store(nextGain, std::memory_order_relaxed);
                }
              }
              for (int s = 0; s < 8; ++s) {
                if (s != slot) {
                  const int visCh = getEngineChannelForSlot(s);
                  if (visCh >= 0 && visCh < nova::kLogicalChannels &&
                      grp->members[static_cast<size_t>(visCh)]) {
                    strips[static_cast<size_t>(s)]->syncState(
                        engine.channels[static_cast<size_t>(visCh)]);
                  }
                }
              }
            }
          }
        }
      };
    }

    // Master Strip & Monitor Strip bindings (Fase 4 - Dual Physical & Surface
    // Output)
    masterStrip.setMonitorMode(false);
    masterStrip.onMasterFaderMoved = [this](float db) {
      savedMasterDb = db;
      if (!masterMutedState)
        engine.masterDb.store(masterDimState ? juce::jmax(-90.0f, db - 20.0f)
                                             : db,
                              std::memory_order_relaxed);
    };

    monitorStrip.setMonitorMode(true);
    monitorStrip.setFaderDb(
        engine.monitorLevelA.load(std::memory_order_relaxed));
    monitorStrip.setMuted(engine.monitorMuteA.load(std::memory_order_relaxed));
    monitorStrip.setDim(engine.monitorDimA.load(std::memory_order_relaxed));

    monitorStrip.onMasterFaderMoved = [this](float db) {
      engine.monitorLevelA.store(db, std::memory_order_relaxed);
    };
    monitorStrip.onMasterMuteChanged = [this](bool muted) {
      engine.monitorMuteA.store(muted, std::memory_order_relaxed);
    };
    monitorStrip.onDimToggled = [this](bool dim) {
      engine.monitorDimA.store(dim, std::memory_order_relaxed);
    };
    masterStrip.onMasterMuteChanged = [this](bool muted) {
      masterMutedState = muted;
      engine.masterDb.store(
          muted ? -90.0f
                : (masterDimState ? juce::jmax(-90.0f, savedMasterDb - 20.0f)
                                  : savedMasterDb),
          std::memory_order_relaxed);
    };
    masterStrip.onDimToggled = [this](bool dim) {
      masterDimState = dim;
      if (!masterMutedState)
        engine.masterDb.store(dim ? juce::jmax(-90.0f, savedMasterDb - 20.0f)
                                  : savedMasterDb,
                              std::memory_order_relaxed);
    };
    masterStrip.onClearSoloClicked = [this] {
      for (auto &ch : engine.channels) {
        ch.solo.store(false, std::memory_order_relaxed);
        ch.sidechainKeyListen.store(false, std::memory_order_relaxed);
      }
      selectChannel(selectedSlot);
    };

    masterProcessing.onMasterEqChanged = [this](float lowDb, float midDb,
                                                float highDb) {
      engine.masterBusEqEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusEqLowGainDb.store(static_cast<double>(lowDb),
                                        std::memory_order_relaxed);
      engine.masterBusEqMidGainDb.store(static_cast<double>(midDb),
                                        std::memory_order_relaxed);
      engine.masterBusEqHighGainDb.store(static_cast<double>(highDb),
                                         std::memory_order_relaxed);
    };
    masterProcessing.onMasterCompChanged = [this](float thrDb,
                                                  float /*ratio*/) {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(
          static_cast<double>(juce::jlimit(-12.0f, -0.1f, thrDb * 0.25f)),
          std::memory_order_relaxed);
    };
    masterProcessing.onMasterBypassToggled = [this](bool bypassed) {
      engine.masterBusEqEnabled.store(!bypassed, std::memory_order_relaxed);
      engine.masterBusLimiterEnabled.store(!bypassed,
                                           std::memory_order_relaxed);
    };
    masterProcessing.onMasterDelayChanged = [this](float ms) {
      engine.masterDelayMs.store(ms, std::memory_order_relaxed);
    };
    masterProcessing.onMonitorDelayChanged = [this](float ms) {
      engine.monitorDelayMs.store(ms, std::memory_order_relaxed);
    };

    auto showAudioOutMenu = [this] {
      juce::PopupMenu m;
      m.addItem(1, "1. Master Output Setup (WASAPI 1 / ASIO / Soundcard)...");
      m.addSeparator();

      // Priority 2: Primary Device Buffer Size / Latency Selector
      juce::PopupMenu bufMenu;
      auto setup = deviceManager.getAudioDeviceSetup();
      const int currentBuf = setup.bufferSize;
      const double sr =
          (setup.sampleRate > 1000.0) ? setup.sampleRate : 48000.0;

      auto addBuf = [&](int id, int samples) {
        const float ms =
            (static_cast<float>(samples) / static_cast<float>(sr)) * 1000.0f;
        juce::String desc = (samples <= 64    ? " - Ultra Low"
                             : samples <= 128 ? " - Pro Live Low"
                             : samples <= 256 ? " - Balanced Touring"
                             : samples <= 512 ? " - Safe High"
                                              : " - Heavy Load");
        juce::String label = juce::String(samples) + " samples (" +
                             juce::String(ms, 1) + " ms per buffer" + desc +
                             ")";
        bufMenu.addItem(id, label, true, currentBuf == samples);
      };
      addBuf(10, 64);
      addBuf(11, 128);
      addBuf(12, 256);
      addBuf(13, 512);
      addBuf(14, 1024);
      m.addSubMenu("Primary Device Buffer Size / Latency", bufMenu);

      m.addSeparator();
      m.addSectionHeader("Monitor Output (WASAPI 2 / Bluetooth Device):");
      juce::StringArray btDevices;
      if (secondaryStreamer != nullptr) {
        btDevices = secondaryStreamer->getAvailableOutputDeviceNames();
        const auto activeName = secondaryStreamer->getCurrentDeviceName();
        for (int i = 0; i < btDevices.size(); ++i) {
          const bool isCurrent =
              (btDevices[i] == activeName && secondaryStreamer->isDeviceOpen());
          m.addItem(100 + i, btDevices[i], true, isCurrent);
        }
        if (secondaryStreamer->isDeviceOpen())
          m.addItem(99, "Disconnect Monitor (WASAPI 2)");
      } else {
        m.addItem(98, "Secondary Streamer Initializing...", false);
      }

      // Priority 3: Monitor FIFO Real-Time Queue Depth & Overrun/Underrun
      // Telemetry
      m.addSeparator();
      const int qDepth = engine.getMonitorFifoQueueDepth();
      const float qMs = engine.getMonitorFifoLatencyMs();
      const uint32_t ovr = engine.getMonitorFifoOverruns();
      const uint32_t und = engine.getMonitorFifoUnderruns();
      juce::String fifoStatus = "Monitor FIFO Queue: " + juce::String(qDepth) +
                                " smp (" + juce::String(qMs, 1) +
                                " ms) | Overrun: " + juce::String(ovr) +
                                " | Underrun: " + juce::String(und);
      m.addItem(300, fifoStatus, false);
      m.addItem(301, "Reset Monitor FIFO & Re-align Queue (Zero Delay)");

      m.addSeparator();
      const bool quadOn =
          engine.dualOutput4ChMode.load(std::memory_order_relaxed);
      m.addItem(200,
                "Dual Output 4-Channel Mode (Ch 1-2 Master, Ch 3-4 Monitor)",
                true, quadOn);

      m.showMenuAsync(
          juce::PopupMenu::Options(), [this, btDevices](int result) {
            if (result == 1) {
              if (onOpenAudioSettings)
                onOpenAudioSettings();
            } else if (result >= 10 && result <= 14) {
              int newBuf = 256;
              if (result == 10)
                newBuf = 64;
              else if (result == 11)
                newBuf = 128;
              else if (result == 12)
                newBuf = 256;
              else if (result == 13)
                newBuf = 512;
              else if (result == 14)
                newBuf = 1024;
              auto setup = deviceManager.getAudioDeviceSetup();
              setup.bufferSize = newBuf;
              deviceManager.setAudioDeviceSetup(setup, true);
            } else if (result == 301) {
              engine.resetMonitorFifoStats();
            } else if (result == 99) {
              if (secondaryStreamer != nullptr)
                secondaryStreamer->closeDevice();
            } else if (result >= 100 && result < 100 + btDevices.size()) {
              const int idx = result - 100;
              if (secondaryStreamer != nullptr && idx < btDevices.size()) {
                secondaryStreamer->openDevice(btDevices[idx]);
              }
            } else if (result == 200) {
              const bool cur =
                  engine.dualOutput4ChMode.load(std::memory_order_relaxed);
              engine.dualOutput4ChMode.store(!cur, std::memory_order_relaxed);
            }
          });
    };

    statusBar.onOpenAudioSettings = showAudioOutMenu;
    masterProcessing.onOpenAudioSettings = showAudioOutMenu;

    statusBar.onOpenSurroundPan = [this] {
      selectNav(MainNavGroup::Routing, 1);
    };
    statusBar.onOpenPatchMatrix = [this] {
      selectNav(MainNavGroup::Routing, 0);
    };
    quickStrip.onOpenSurroundPan = [this] {
      selectNav(MainNavGroup::Routing, 1);
    };

    // Embedded modules routed via Touchscreen Layers (no modal overlays)
    statusBar.onOpenAiModal = [this] { openChannelAiModal(); };
    statusBar.onOpenDspRackModal = [this] { selectNav(MainNavGroup::Fx, 1); };
    statusBar.onOpenMCloudModal = [this] { selectNav(MainNavGroup::Show, 5); };
    statusBar.onOpenGangModal = [this] { selectNav(MainNavGroup::Routing, 4); };

    gangModal.onGroupsUpdated =
        [this](const std::array<ChannelLinkGroup,
                                MidasGangFaderModal::kMaxGroups> &newGroups,
               int updatedGroupIdx) {
          linkGroups = newGroups;
          gangedSlots.fill(false);
          for (int s = 0; s < 8; ++s) {
            const int chIdx = getEngineChannelForSlot(s);
            if (findLinkGroupForChannel(chIdx) != nullptr)
              gangedSlots[static_cast<size_t>(s)] = true;
          }
          if (updatedGroupIdx >= 0 &&
              updatedGroupIdx < MidasGangFaderModal::kMaxGroups) {
            const auto &grp = linkGroups[static_cast<size_t>(updatedGroupIdx)];
            if (grp.active && !grp.isRelative) {
              // ABSOLUTE mode: immediately align all members to the first
              // active member
              int masterCh = -1;
              for (int c = 0; c < nova::kLogicalChannels; ++c) {
                if (grp.members[static_cast<size_t>(c)]) {
                  masterCh = c;
                  break;
                }
              }
              if (masterCh >= 0) {
                const auto &src =
                    engine.channels[static_cast<size_t>(masterCh)];
                for (int c = 0; c < nova::kLogicalChannels; ++c) {
                  if (c != masterCh && grp.members[static_cast<size_t>(c)]) {
                    auto &dst = engine.channels[static_cast<size_t>(c)];
                    if (grp.linkFader)
                      dst.gainDb.store(
                          src.gainDb.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
                    if (grp.linkPreamp)
                      dst.inputGainDb.store(
                          src.inputGainDb.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
                    if (grp.linkPan)
                      dst.pan.store(src.pan.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
                    if (grp.linkMute)
                      dst.muted.store(src.muted.load(std::memory_order_relaxed),
                                      std::memory_order_relaxed);
                    if (grp.linkSolo)
                      dst.solo.store(src.solo.load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);
                  }
                }
              }
            }
          }
          for (int s = 0; s < 8; ++s) {
            const int chIdx = getEngineChannelForSlot(s);
            if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size()))
              strips[static_cast<size_t>(s)]->syncState(
                  engine.channels[static_cast<size_t>(chIdx)]);
          }
          selectChannel(selectedSlot);
        };

    dspModal.onBackToRack = [this] { selectNav(MainNavGroup::Fx, 0); };

    dspModal.onModelChanged = [this](const juce::String &eqName,
                                     const juce::String &compName) {
      statusBar.setDspRackBadge("DSP: " + eqName);
      const int chIdx = getEngineChannelForSlot(selectedSlot);
      if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(chIdx)];
        ch.eqModel.store(dspModal.getActiveEqIndex(),
                         std::memory_order_relaxed);
        int compMode =
            static_cast<int>(nova::graviton::CompressorMode::Corrective);
        if (compName.containsIgnoreCase("FET"))
          compMode = static_cast<int>(nova::graviton::CompressorMode::Creative);
        else if (compName.containsIgnoreCase("OPTICAL"))
          compMode = static_cast<int>(nova::graviton::CompressorMode::Adaptive);
        else if (compName.containsIgnoreCase("VARI-MU"))
          compMode = static_cast<int>(nova::graviton::CompressorMode::Vintage);
        ch.compressorMode.store(compMode, std::memory_order_relaxed);
        ch.gateMode.store(dspModal.getActiveGateIndex(),
                          std::memory_order_relaxed);
        if (dspModal.lastChangedSection == 0)
          ch.eqEnabled.store(dspModal.isEqActive(), std::memory_order_relaxed);
        else if (dspModal.lastChangedSection == 1)
          ch.compressorEnabled.store(dspModal.isCompActive(),
                                     std::memory_order_relaxed);
        else if (dspModal.lastChangedSection == 2)
          ch.gateEnabled.store(dspModal.isGateActive(),
                               std::memory_order_relaxed);
        if (selectedSlot >= 0 && selectedSlot < 8 &&
            strips[static_cast<size_t>(selectedSlot)] != nullptr) {
          strips[static_cast<size_t>(selectedSlot)]->syncState(ch);
        }
      }
      repaint();
    };

    aiModal.onChannelUpdated = [this] {
      selectChannel(selectedSlot);
      for (size_t i = 0; i < 8; ++i) {
        const int c = getEngineChannelForSlot(static_cast<int>(i));
        if (c >= 0 && c < static_cast<int>(engine.channels.size())) {
          strips[i]->syncState(engine.channels[static_cast<size_t>(c)]);
        }
      }
      repaint();
    };

    aiModal.onApplyAiProfile =
        [this](int chIdx, const nova::graviton::AIRecommendation &rec,
               int scope) {
          if (!engine.applyAIRecommendation(chIdx, rec, scope))
            return;

          int partnerCh = -1;
          if (engine.isChannelStereoLinked(chIdx, &partnerCh) &&
              partnerCh >= 0 &&
              partnerCh < static_cast<int>(engine.channels.size())) {
            engine.applyAIRecommendation(partnerCh, rec, scope);
          }

          selectChannel(selectedSlot);
        };

    mcloudModal.onShowfileExport = [this](bool /*isBackup*/) {
      const auto storageDir = recorderView.getTargetDirectory();
      if (!storageDir.exists())
        storageDir.createDirectory();
      const auto showFile = storageDir.getChildFile("Arena_Tour_2026_Main.m96");
      juce::String json = "{\n  \"console\": \"MIDAS HERITAGE-D HD96-24\",\n";
      json += "  \"timestamp\": \"" +
              juce::Time::getCurrentTime().toISO8601(true) + "\",\n";
      json += "  \"masterDb\": " +
              juce::String(engine.masterDb.load(std::memory_order_relaxed), 2) +
              ",\n  \"channels\": [\n";
      for (int c = 0; c < 16; ++c) {
        const auto &ch = engine.channels[static_cast<size_t>(c)];
        json += "    {\"ch\": " + juce::String(c + 1) + ", \"gainDb\": " +
                juce::String(ch.gainDb.load(std::memory_order_relaxed), 2) +
                ", \"vca\": " +
                juce::String(ch.vcaGroup.load(std::memory_order_relaxed)) +
                "}" + (c < 15 ? ",\n" : "\n");
      }
      json += "  ]\n}\n";
      showFile.replaceWithText(json);
    };
  }

  void timerCallback() override {
    // 25 fps UI Telemetry update
    const auto now = juce::Time::getCurrentTime();
    statusBar.updateClock(now.formatted("%H:%M:%S"));

    // Motorized Fader Automation Glide (FADE IN / FADE OUT)
    if (faderGlideMode != 0) {
      bool stillGliding = false;
      const float targetDb = (faderGlideMode > 0) ? 0.0f : -90.0f;
      const float stepDb = (faderGlideMode > 0) ? 1.8f : -2.2f;
      for (int i = 0; i < 8; ++i) {
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const float cur =
              engine.channels[static_cast<size_t>(chIdx)].gainDb.load(
                  std::memory_order_relaxed);
          if (std::abs(cur - targetDb) > 0.5f) {
            const float next = (faderGlideMode > 0)
                                   ? juce::jmin(targetDb, cur + stepDb)
                                   : juce::jmax(targetDb, cur + stepDb);
            engine.channels[static_cast<size_t>(chIdx)].gainDb.store(
                next, std::memory_order_relaxed);
            stillGliding = true;
          }
        }
      }
      if (!stillGliding)
        faderGlideMode = 0;
    }

    std::vector<float> peaks;
    peaks.reserve(32);
    for (size_t i = 0; i < 32; ++i) {
      if (i < engine.channels.size()) {
        const float lin =
            engine.channels[i].peak.load(std::memory_order_relaxed);
        const float db = (lin > 1.0e-5f)
                             ? juce::Decibels::gainToDecibels(lin, -60.0f)
                             : -60.0f;
        peaks.push_back(db);
      } else {
        peaks.push_back(-60.0f);
      }
    }

    const float mstLinL = engine.masterPeakL.load(std::memory_order_relaxed);
    const float mstLinR = engine.masterPeakR.load(std::memory_order_relaxed);
    const float mstDbL = (mstLinL > 1.0e-5f)
                             ? juce::Decibels::gainToDecibels(mstLinL, -60.0f)
                             : -60.0f;
    const float mstDbR = (mstLinR > 1.0e-5f)
                             ? juce::Decibels::gainToDecibels(mstLinR, -60.0f)
                             : -60.0f;
    const float monLinL = engine.monitorPeakL_A.load(std::memory_order_relaxed);
    const float monDb = (monLinL > 1.0e-5f)
                            ? juce::Decibels::gainToDecibels(monLinL, -60.0f)
                            : -60.0f;

    const float monLinR = engine.monitorPeakR_A.load(std::memory_order_relaxed);
    const float monDbR = (monLinR > 1.0e-5f)
                             ? juce::Decibels::gainToDecibels(monLinR, -60.0f)
                             : -60.0f;

    if (overviewView.isVisible()) {
      OverviewTelemetrySnapshot ov;
      auto *curDev = deviceManager.getCurrentAudioDevice();
      const double sr = engine.sampleRate.load(std::memory_order_relaxed);
      const int bs = engine.blockSize.load(std::memory_order_relaxed);
      const uint64_t cbs = engine.callbackCount.load(std::memory_order_relaxed);
      ov.audioDeviceValid =
          (curDev != nullptr && sr > 1000.0 && bs > 0 && cbs > 0);
      if (curDev != nullptr) {
        ov.audioDeviceType = deviceManager.getCurrentAudioDeviceType();
        ov.audioDeviceName = curDev->getName();
      }
      ov.sampleRate = sr;
      ov.bufferSize = bs;
      ov.inputChannelsActive =
          engine.inputCount.load(std::memory_order_relaxed);
      ov.outputChannelsActive =
          engine.outputCount.load(std::memory_order_relaxed);
      ov.callbackCount = cbs;

      std::array<bool, 24> vcaHasMember{};
      for (size_t c = 0; c < engine.channels.size(); ++c) {
        const auto &ch = engine.channels[c];
        if (ch.muted.load(std::memory_order_relaxed))
          ++ov.mutedCount;
        if (ch.solo.load(std::memory_order_relaxed))
          ++ov.soloCount;
        const int vg = ch.vcaGroup.load(std::memory_order_relaxed);
        if (vg >= 1 && vg <= 24)
          vcaHasMember[static_cast<size_t>(vg - 1)] = true;
        if (c < nova::kLogicalChannels) {
          const float lin = ch.peak.load(std::memory_order_relaxed);
          ov.chPeaksDb[c] = (lin > 1.0e-5f)
                                ? juce::Decibels::gainToDecibels(lin, -120.0f)
                                : -120.0f;
        }
      }
      for (bool hasM : vcaHasMember) {
        if (hasM)
          ++ov.vcaAssignedCount;
      }
      for (const auto &lg : linkGroups) {
        if (lg.active)
          ++ov.linkGroupsActive;
      }

      float maxFxOut = 0.0f;
      for (size_t s = 0; s < nova::graviton::EffectsRack::kNumSlots; ++s) {
        const auto &slot = engine.effectsRack.getSlot(s);
        const int tp = slot.type.load(std::memory_order_relaxed);
        const bool byp = slot.bypassed.load(std::memory_order_relaxed);
        if (tp != static_cast<int>(nova::graviton::FxType::None) && !byp) {
          ++ov.fxActiveCount;
          maxFxOut = std::max(maxFxOut,
                              slot.outputPeak.load(std::memory_order_relaxed));
        }
      }

      ov.masterPeakL = (mstLinL > 1.0e-5f)
                           ? juce::Decibels::gainToDecibels(mstLinL, -120.0f)
                           : -120.0f;
      ov.masterPeakR = (mstLinR > 1.0e-5f)
                           ? juce::Decibels::gainToDecibels(mstLinR, -120.0f)
                           : -120.0f;

      float maxAuxLin = 0.0f;
      for (const auto &ap : engine.auxPeak)
        maxAuxLin = std::max(maxAuxLin, ap.load(std::memory_order_relaxed));
      ov.auxMaxPeakDb = (maxAuxLin > 1.0e-5f)
                            ? juce::Decibels::gainToDecibels(maxAuxLin, -120.0f)
                            : -120.0f;

      float maxMtxLin = 0.0f;
      for (const auto &mp : engine.matrixPeak)
        maxMtxLin = std::max(maxMtxLin, mp.load(std::memory_order_relaxed));
      ov.matrixMaxPeakDb =
          (maxMtxLin > 1.0e-5f)
              ? juce::Decibels::gainToDecibels(maxMtxLin, -120.0f)
              : -120.0f;

      ov.fxReturnMaxPeakDb =
          (maxFxOut > 1.0e-5f)
              ? juce::Decibels::gainToDecibels(maxFxOut, -120.0f)
              : -120.0f;

      ov.monitorWasapi2Open =
          (secondaryStreamer != nullptr && secondaryStreamer->isDeviceOpen());
      if (ov.monitorWasapi2Open) {
        ov.monitorDeviceName = secondaryStreamer->getCurrentDeviceName();
        ov.monitorPeakL = (monLinL > 1.0e-5f)
                              ? juce::Decibels::gainToDecibels(monLinL, -120.0f)
                              : -120.0f;
        ov.monitorPeakR = (monLinR > 1.0e-5f)
                              ? juce::Decibels::gainToDecibels(monLinR, -120.0f)
                              : -120.0f;
        ov.monitorFifoLatencyMs = engine.getMonitorFifoLatencyMs();
      }

      ov.recorderActive = engine.recorder.isRecording();
      ov.recorderMultitrack = engine.recorder.isMultitrackActive();
      ov.oscRunning = engine.oscServer.isServerRunning();
      overviewView.updateSnapshot(ov);
    }
    masterStrip.setLevels(mstDbL, mstDbR);
    masterStrip.setFaderDb(engine.masterDb.load(std::memory_order_relaxed));
    monitorStrip.setLevels(monDb, monDbR);
    dedicatedDsp.setBusMeters(mstDbL, mstDbR, (mstDbL + mstDbR) * 0.5f,
                              (monDb + monDbR) * 0.5f);

    const int curSpillVca =
        engine.activeSpillVca.load(std::memory_order_relaxed);
    if (curSpillVca != lastSpillVca) {
      lastSpillVca = curSpillVca;
      rebankStrips(currentBank);
      selectChannel(selectedSlot);
    }

    // Consolidated, non-conflicting strip telemetry update loop
    for (int i = 0; i < 8; ++i) {
      if (currentSofMode == 1) { // 1. SOF: MONITOR (Controls and reflects
                                 // channel Monitor Send Level)
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
          strips[static_cast<size_t>(i)]->setSofMonitorMode(
              true, ch.monitorPreFader.load(std::memory_order_relaxed));
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(
              ch.monitorSendDb.load(std::memory_order_relaxed),
              !ch.monitorAssigned.load(std::memory_order_relaxed),
              ch.peak.load(std::memory_order_relaxed));
        }
      } else if (currentSofMode == 2) { // 2. SOF: MIX 1-2 (Reflects channel
                                        // send level to Mix 1-2)
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
          const float sendDb = ch.auxSendDb[0].load(std::memory_order_relaxed);
          const float pk = ch.peak.load(std::memory_order_relaxed);
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(sendDb, false, pk);
        }
      } else if (currentBank ==
                 2) { // 3. Bank 2: MIX BUS / AUX (Aux 1-8 Masters)
        strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        const float auxDb =
            (static_cast<size_t>(i) < engine.auxMasterDb.size())
                ? engine.auxMasterDb[static_cast<size_t>(i)].load(
                      std::memory_order_relaxed)
                : -60.0f;
        const bool auxMt = (static_cast<size_t>(i) < engine.auxMuted.size())
                               ? engine.auxMuted[static_cast<size_t>(i)].load(
                                     std::memory_order_relaxed)
                               : true;
        const float auxPk = (static_cast<size_t>(i) < engine.auxPeak.size())
                                ? engine.auxPeak[static_cast<size_t>(i)].load(
                                      std::memory_order_relaxed)
                                : 0.0f;
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(auxDb, auxMt, auxPk);
      } else if (currentBank == 3) { // 4. Bank 3: VCA GROUPS
        strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        const float vcaDb =
            (static_cast<size_t>(i) < engine.vcaMasterDb.size())
                ? engine.vcaMasterDb[static_cast<size_t>(i)].load(
                      std::memory_order_relaxed)
                : 0.0f;
        const bool vcaMt = (static_cast<size_t>(i) < engine.vcaMuted.size())
                               ? engine.vcaMuted[static_cast<size_t>(i)].load(
                                     std::memory_order_relaxed)
                               : false;
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(vcaDb, vcaMt, 0.0f);
      } else { // 5. Normal input channels
        strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          strips[static_cast<size_t>(i)]->syncState(
              engine.channels[static_cast<size_t>(chIdx)]);
        }
      }
    }

    // EBU R128 Loudness telemetry calculations
    const float mstMaxPeak = std::max(mstDbL, mstDbR);
    const float momLufs = std::clamp(mstMaxPeak - 3.0f, -70.0f, 0.0f);
    shortTermLufsSmooth = shortTermLufsSmooth * 0.85f + momLufs * 0.15f;
    integratedLufsSmooth = integratedLufsSmooth * 0.98f + momLufs * 0.02f;
    broadcastView.updateLoudness(integratedLufsSmooth, shortTermLufsSmooth,
                                 momLufs, mstMaxPeak);

    // Multitrack recorder SMPTE timecode progression
    if (engine.recorder.isRecording()) {
      double sr = 96000.0;
      if (auto *dev = deviceManager.getCurrentAudioDevice()) {
        sr = dev->getCurrentSampleRate();
        if (sr < 8000.0)
          sr = 96000.0;
      }
      const uint64_t frames = engine.recorder.getRecordedFrames();
      const double totalSec = static_cast<double>(frames) / sr;
      const int hours = static_cast<int>(totalSec / 3600.0);
      const int mins = static_cast<int>(std::fmod(totalSec, 3600.0) / 60.0);
      const int secs = static_cast<int>(std::fmod(totalSec, 60.0));
      const int framesRemainder =
          static_cast<int>(std::fmod(totalSec, 1.0) * 25.0);
      const auto tc = juce::String::formatted("%02d:%02d:%02d:%02d", hours,
                                              mins, secs, framesRemainder);
      recorderView.setTimecode(tc);
      recorderView.setProgress(
          static_cast<float>(std::fmod(totalSec, 300.0) / 300.0));
    }

    if ((++storageTelemetryTick % 50) == 0) {
      recorderView.updateStorageInfo();
    }

    // Real-time telemetry feed for active touchscreen DSP curve visualizers
    // (Fase 2)
    const int selCh = getEngineChannelForSlot(selectedSlot);
    if (selCh >= 0 && selCh < static_cast<int>(engine.channels.size())) {
      const auto &curCh = engine.channels[static_cast<size_t>(selCh)];
      const float linPk = curCh.peak.load(std::memory_order_relaxed);
      const float curPeakDb =
          (linPk > 1.0e-5f) ? juce::Decibels::gainToDecibels(linPk, -60.0f)
                            : -60.0f;
      const float compGr =
          curCh.compressorGainReductionDb.load(std::memory_order_relaxed);
      const float gateGr =
          curCh.gateReductionDb.load(std::memory_order_relaxed);

      if (engine.rtaAnalyzer.hasNewData()) {
        std::array<double, nova::graviton::DualFFTAnalyzer::kFFTHalf> specA{},
            specB{}, coh{}, tf{};
        int bins = 0;
        engine.rtaAnalyzer.getSpectrumData(specA.data(), specB.data(),
                                           coh.data(), tf.data(), bins);
        std::array<float, 64> bands64{};
        for (int i = 0; i < 64; ++i) {
          const int startBin = std::clamp(
              static_cast<int>(std::pow(2.0, (i / 64.0) * 10.0)), 0, bins - 1);
          const int endBin = std::clamp(
              static_cast<int>(std::pow(2.0, ((i + 1) / 64.0) * 10.0)),
              startBin + 1, bins);
          double maxDb = -120.0;
          for (int b = startBin; b < endBin; ++b) {
            maxDb = std::max(maxDb, std::max(specA[static_cast<size_t>(b)],
                                             specB[static_cast<size_t>(b)]));
          }
          bands64[static_cast<size_t>(i)] = juce::jlimit(
              0.0f, 1.0f, static_cast<float>((maxDb + 60.0) / 60.0));
        }
      } else {
      }
    }

    // Priority 3: Real-time Monitor FIFO Depth & Overrun/Underrun Telemetry
    // Update (25 Hz)
    masterProcessing.setFifoTelemetry(
        engine.getMonitorFifoQueueDepth(), engine.getMonitorFifoLatencyMs(),
        engine.getMonitorFifoOverruns(), engine.getMonitorFifoUnderruns());
    monitorStrip.setDim(engine.monitorDimA.load(std::memory_order_relaxed));
    monitorStrip.setMuted(engine.monitorMuteA.load(std::memory_order_relaxed));

    // Sync Quad FX Rack telemetry from GravitonEffectsRack (Fase 5)
    if (activeTab == 7) {
      const auto &s0 = engine.effectsRack.getSlot(0);
      const auto &s1 = engine.effectsRack.getSlot(1);
      const auto &s2 = engine.effectsRack.getSlot(2);
      const auto &s4 = engine.effectsRack.getSlot(4);
      fxHostView.syncSummaryFromRack(
          static_cast<float>(s0.vss4Reverb.getMasterDecay()),
          static_cast<float>(s0.vss4Reverb.getPreDelayMs()),
          static_cast<float>(s1.nebulaDelay.getDelayTimeMsL()),
          static_cast<float>(s2.plate.getDecay()),
          static_cast<float>(s4.masterX.getLimiterCeilingDb()));
    }
  }

  void paintOverChildren(juce::Graphics &g) override {
    if (screenBrightness < 0.99f) {
      g.fillAll(juce::Colours::black.withAlpha(
          juce::jlimit(0.0f, 0.55f, (1.0f - screenBrightness) * 0.72f)));
    }
  }

  void syncLauncherFavorites() {
    std::vector<std::pair<int, int>> keys;
    keys.reserve(favoriteEntries.size());
    for (const auto &f : favoriteEntries)
      keys.push_back({f.groupIdx, f.subPage});
    homeLauncherView.setFavorites(keys);
  }

  void openHomeLauncher(bool focusSearchField = false) {
    recentView.setVisible(false);
    homeLauncherView.setActivePage(static_cast<int>(activeGroup), activeSubPage);
    syncLauncherFavorites();
    if (focusSearchField)
      homeLauncherView.clearSearch();
    homeLauncherView.setVisible(true);
    homeLauncherView.toFront(true);
    if (focusSearchField)
      homeLauncherView.focusSearch();
    repaint();
  }

  void refreshQuickSettings() {
    auto *dev = deviceManager.getCurrentAudioDevice();
    const double sr = engine.sampleRate.load(std::memory_order_relaxed);
    const int bs = engine.blockSize.load(std::memory_order_relaxed);
    juce::String audioL1 = "IDLE / NO DEVICE";
    juce::String audioL2 = "Open Audio Hardware to select";
    if (dev != nullptr && sr > 1000.0) {
      const double latMs = (static_cast<double>(bs) / sr) * 1000.0;
      audioL1 = deviceManager.getCurrentAudioDeviceType();
      audioL2 = dev->getName() + " | " + juce::String(sr / 1000.0, 1) +
                " kHz / " + juce::String(bs) + " (" + juce::String(latMs, 1) +
                " ms)";
    }

    const bool oscUp = engine.oscServer.isServerRunning();
    juce::String ipStr = "127.0.0.1";
    for (const auto &ip : juce::IPAddress::getAllAddresses(false)) {
      const auto s = ip.toString();
      if (!s.startsWith("127.") && s != "::1") {
        ipStr = s;
        break;
      }
    }
    const juce::String netL1 = oscUp ? "OSC LISTENING" : "OSC STOPPED";
    const juce::String netL2 = "IP: " + ipStr;

    const juce::String sysInfo =
        juce::SystemStats::getOperatingSystemName() + " | " +
        juce::String(juce::SystemStats::getNumCpus()) + " CPU";

    quickSettingsPanel.updateStatus(audioL1, audioL2, netL1, netL2, sysInfo,
                                    statusBar.getTextScaleLabel());
  }

  float integratedLufsSmooth = -23.0f;
  float shortTermLufsSmooth = -23.0f;

  MixerEngine &engine;
  juce::AudioDeviceManager &deviceManager;
  MixerMidiCallback &midiCallback;

  int currentBank = 0;
  int selectedSlot = 0;
  int activeTab = 0;
  int activePopGroup = 0;
  int inputRangeOffset = 0;
  int faderGlideMode = 0;
  bool sofActive = false;
  bool link56Active = false;
  bool masterMutedState = false;
  bool masterDimState = false;
  int lastSpillVca = 0;
  float savedMasterDb = 0.0f;
  std::array<float, 4> savedEqGains{};
  std::array<bool, 8> gangedSlots{};
  std::array<ChannelLinkGroup, MidasGangFaderModal::kMaxGroups> linkGroups{};
  const ChannelLinkGroup *findLinkGroupForChannel(int chIdx) const {
    if (chIdx < 0 || chIdx >= nova::kLogicalChannels)
      return nullptr;
    for (const auto &grp : linkGroups) {
      if (grp.active && grp.members[static_cast<size_t>(chIdx)])
        return &grp;
    }
    return nullptr;
  }
  int storageTelemetryTick = 0;

  // Touchscreen Unified Navigation State
  MainNavGroup activeGroup = MainNavGroup::Mix;
  int activeSubPage = 0;
  bool audioSuspendedForBounce = false;

  MidasTopStatusBar statusBar;
  std::array<std::unique_ptr<MidasTactileButton>, 5> groupButtons;
  std::array<std::unique_ptr<MidasTactileButton>, 26> subTabButtons;

  // MIX GROUP (Embedded: Overview, PEQ, Dynamic EQ, Dynamics, Dugan, Monitor &
  // Talkback)
  MidasOverviewView overviewView;
  DynamicEqPanel dynamicEqView;
  DynamicsPanel dynamicsPanelView;
  DuganAutomixerPanel duganView;
  MonitorPanel monitorView;
  TalkbackPanel talkbackView;

  // ROUTING GROUP (Embedded: Patch Matrix, 3D Surround, VCA Groups, Bus &
  // Direct, Talkback Router)
  StageBoxPatchPanel patchMatrixView;
  SurroundPanPanel surroundPanView;
  VcaPanel vcaPanelView;
  MidasRoutingMatrixView routingView;

  // FX GROUP (Embedded Host: 24-Slot Rack + Quad Summary + Dedicated DSP
  // Editors)
  EmbeddedFxHostPanel fxHostView;

  // SHOW GROUP
  CueListPanel fullCueListView;
  MidasCueListView cuelistView;
  MidasRecorderView recorderView;
  OfflineBouncePanel offlineBounceView;

  // SYSTEM GROUP
  ASIOBridgePanel asioBridgeView;
  OscServerPanel oscServerView;
  MidasVirtualCableView virtualCableView;
  MidasSetupView setupView;
  std::unique_ptr<juce::AudioDeviceSelectorComponent> audioDeviceSelector;
  MidasBroadcastView broadcastView;

  MidasQuickStrip quickStrip;
  MidasDedicatedDspPanel dedicatedDsp;
  MidasLayerNavBar layerBar;

  std::array<std::unique_ptr<MidasConsoleStrip>, 8> strips;
  MidasMasterStrip masterStrip;
  MidasMasterStrip monitorStrip;
  MidasMasterProcessingPanel masterProcessing;
  int currentSofMode = 0;
  MidasArmrest armrest;

  MidasGangFaderModal gangModal;
  MidasDspModelModal dspModal;
  MidasAiModal aiModal;
  MidasMCloudModal mcloudModal;

  MidasTactileButton btnNavMenu, btnNavBack, btnNavHome, btnNavSearch,
      btnNavFav, btnNavRecent;
  AndroidHomeLauncherView homeLauncherView;
  AndroidRecentWorkspaceView recentView;
  AndroidQuickSettingsPanel quickSettingsPanel;
  std::vector<std::pair<MainNavGroup, int>> navHistory;
  std::vector<AndroidRecentWorkspaceView::Entry> recentEntries;
  std::vector<AndroidRecentWorkspaceView::Entry> favoriteEntries;
  juce::Rectangle<int> breadcrumbBounds;
  juce::String activeGroupTag{"MIX"};
  juce::String activePageTitle{"OVERVIEW"};
  juce::String activeBreadcrumbText{"MIX  >  OVERVIEW"};
  juce::Colour activePageAccent{midas_colors::kAccentCyan};
  float screenBrightness = 1.0f;

  nova::SecondaryAudioStreamer *secondaryStreamer = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasTouringConsoleSurface)
};
