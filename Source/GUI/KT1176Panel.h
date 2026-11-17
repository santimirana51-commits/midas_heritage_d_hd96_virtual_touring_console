#pragma once

// ==============================================================================
// KT1176Panel.h
// NOVA HD — Authentic 1176LN Blackface FET Limiting Amplifier Hardware UI
//
// Matches the physical layout and behavior of the Universal Audio / UREI
// 1176LN:
//   • Left: Large Fluted Spun-Aluminum INPUT & OUTPUT Knobs
//   (∞..48..36..30..24..18..12..6..0) • Center: Stacked Silver-Skirted ATTACK
//   (1–7 + OFF switch) & RELEASE (1–7) Knobs • Center-Right: Vertical 4-Button
//   RATIO Bank (20, 12, 8, 4) + ALL BUTTONS IN (Nuke) • Right: Warm Backlit
//   Analog VU / Gain Reduction Meter (-20 to +3 VU) • Far-Right: Vertical
//   4-Button METER Bank (GR, +8, +4, OFF) • Top Bar: 12 Presets, FET Revision
//   (Rev A Bluestripe / Rev D LN / Rev E Blackface),
//     Transformer Coloration, Sidechain HPF (Off / 60 / 90 / 150 Hz), Parallel
//     Mix
// ==============================================================================

#include "Effects/GravitonKT1176.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace nova {

// ==============================================================================
// 1. Custom 1176LN Rotary Knob (Large Fluted Input/Output or Small Skirted
// Att/Rel)
// ==============================================================================
class Knob1176 final : public juce::Component {
public:
  enum class Style {
    LargeFluted, // INPUT / OUTPUT (0..48 dial scale, 24 at 12 o'clock)
    SmallSkirted // ATTACK / RELEASE (1..7 inverted scale, 4 at 12 o'clock)
  };

  Knob1176(const juce::String &labelText, Style knobStyle, double minVal,
           double maxVal, double defaultVal)
      : title(labelText), style(knobStyle), minV(minVal), maxV(maxVal),
        defV(defaultVal), curV(defaultVal) {
    setRepaintsOnMouseActivity(true);
  }

  void setValue(double v,
                juce::NotificationType notify = juce::dontSendNotification) {
    const double clamped = std::clamp(v, minV, maxV);
    if (std::abs(clamped - curV) > 1e-5) {
      curV = clamped;
      repaint();
      if (notify != juce::dontSendNotification && onValueChanged) {
        onValueChanged(curV);
      }
    }
  }

  double getValue() const noexcept { return curV; }

  void setSubReadout(const juce::String &txt) {
    if (subReadout != txt) {
      subReadout = txt;
      repaint();
    }
  }

  void setOffState(bool off) {
    if (isOff != off) {
      isOff = off;
      repaint();
    }
  }

  std::function<void(double)> onValueChanged;

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    const float cy = (style == Style::LargeFluted) ? (b.getCentreY() - 6.0f)
                                                   : (b.getCentreY() - 2.0f);
    const float radius = (style == Style::LargeFluted)
                             ? (std::min(b.getWidth(), b.getHeight()) * 0.34f)
                             : (std::min(b.getWidth(), b.getHeight()) * 0.29f);

    // Angle sweep: -135 deg (7:30) to +135 deg (4:30) in radians (0 = 12
    // o'clock)
    constexpr float startAng = -2.35619449f; // -135 deg
    constexpr float endAng = 2.35619449f;    // +135 deg
    const float norm =
        isOff ? 0.0f
              : static_cast<float>((curV - minV) / std::max(1e-5, maxV - minV));
    const float angle = startAng + norm * (endAng - startAng);

    // 1. Silk-Screened Scale Markings Around Knob
    g.setColour(juce::Colour(0xffe5e7eb));
    if (style == Style::LargeFluted) {
      // 1176 Input/Output attenuator markings: ∞, 48, 36, 30, 24 (top), 18, 12,
      // 6, 0
      static const char *kLabels[9] = {"\xe2\x88\x9e", "48", "36", "30", "24",
                                       "18",           "12", "6",  "0"};
      const float labelDist = radius + 17.0f;
      const float dotDist = radius + 8.5f;

      for (int i = 0; i <= 24; ++i) {
        const float t = static_cast<float>(i) / 24.0f;
        const float a = startAng + t * (endAng - startAng);
        const float dx = cx + std::sin(a) * dotDist;
        const float dy = cy - std::cos(a) * dotDist;
        const bool isMajor = (i % 3 == 0);
        g.setColour(isMajor ? juce::Colour(0xfff3f4f6)
                            : juce::Colour(0xff9ca3af));
        g.fillEllipse(dx - (isMajor ? 1.7f : 1.1f),
                      dy - (isMajor ? 1.7f : 1.1f), isMajor ? 3.4f : 2.2f,
                      isMajor ? 3.4f : 2.2f);
      }

      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(juce::Colour(0xfff3f4f6));
      for (int i = 0; i < 9; ++i) {
        const float t = static_cast<float>(i) / 8.0f;
        const float a = startAng + t * (endAng - startAng);
        const float lx = cx + std::sin(a) * labelDist;
        const float ly = cy - std::cos(a) * labelDist;
        g.drawText(kLabels[i], static_cast<int>(lx - 12.0f),
                   static_cast<int>(ly - 7.0f), 24, 14,
                   juce::Justification::centred, false);
      }
    } else {
      // 1176 Attack/Release markings: 1, (2), 3, (4), 5, (6), 7
      const float labelDist = radius + 14.0f;
      const float dotDist = radius + 7.5f;
      for (int step = 1; step <= 7; ++step) {
        const float t = static_cast<float>(step - 1) / 6.0f;
        const float a = startAng + t * (endAng - startAng);
        const float dx = cx + std::sin(a) * dotDist;
        const float dy = cy - std::cos(a) * dotDist;
        g.setColour(juce::Colour(0xfff3f4f6));
        g.fillEllipse(dx - 1.5f, dy - 1.5f, 3.0f, 3.0f);

        if (step == 1 || step == 3 || step == 5 || step == 7) {
          const float lx = cx + std::sin(a) * labelDist;
          const float ly = cy - std::cos(a) * labelDist;
          g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
          g.drawText(juce::String(step), static_cast<int>(lx - 9.0f),
                     static_cast<int>(ly - 7.0f), 18, 14,
                     juce::Justification::centred, false);
        }
      }
    }

    // 2. Drop Shadow Under Knob
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillEllipse(cx - radius - 2.0f, cy - radius + 3.5f,
                  (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f);

    // 3. Outer Fluted Black Bakelite Skirt
    g.setColour(juce::Colour(0xff141619));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff374151));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.3f);

    // Knurled/fluted ridges around the black skirt
    const int numRidges = (style == Style::LargeFluted) ? 24 : 16;
    g.setColour(juce::Colour(0xff272b33));
    for (int i = 0; i < numRidges; ++i) {
      const float ra =
          angle + static_cast<float>(i) *
                      (2.0f * 3.14159265f / static_cast<float>(numRidges));
      const float x1 = cx + std::sin(ra) * (radius * 0.72f);
      const float y1 = cy - std::cos(ra) * (radius * 0.72f);
      const float x2 = cx + std::sin(ra) * (radius * 0.97f);
      const float y2 = cy - std::cos(ra) * (radius * 0.97f);
      g.drawLine(x1, y1, x2, y2, 1.4f);
    }

    // 4. Spun Brushed Silver Aluminum Center Cap
    const float capR = radius * 0.66f;
    juce::ColourGradient capGrad(juce::Colour(0xfff3f4f6), cx - capR * 0.5f,
                                 cy - capR * 0.5f, juce::Colour(0xff9ca3af),
                                 cx + capR * 0.5f, cy + capR * 0.5f, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colour(0xff4b5563));
    g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // 5. Pointer Indicator Line
    const float pStart =
        (style == Style::LargeFluted) ? (capR * 0.25f) : (capR * 0.15f);
    const float pEnd = radius * 0.95f;
    const float px1 = cx + std::sin(angle) * pStart;
    const float py1 = cy - std::cos(angle) * pStart;
    const float px2 = cx + std::sin(angle) * pEnd;
    const float py2 = cy - std::cos(angle) * pEnd;
    g.setColour(isOff ? juce::Colour(0xffef4444) : juce::Colour(0xff111827));
    g.drawLine(px1, py1, px2, py2, 2.4f);
    g.setColour(juce::Colours::white);
    g.drawLine(cx + std::sin(angle) * (capR * 0.72f),
               cy - std::cos(angle) * (capR * 0.72f), px2, py2, 1.8f);

    // 6. Title & Sub-Readout
    g.setColour(juce::Colour(0xfff3f4f6));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    if (style == Style::LargeFluted) {
      g.drawText(title, 0, static_cast<int>(b.getHeight() - 28.0f),
                 static_cast<int>(b.getWidth()), 14,
                 juce::Justification::centred, false);
      g.setColour(juce::Colour(0xff93c5fd));
      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      g.drawText(subReadout, 0, static_cast<int>(b.getHeight() - 14.0f),
                 static_cast<int>(b.getWidth()), 13,
                 juce::Justification::centred, false);
    } else {
      // For Attack (top) and Release (bottom), draw title and readout cleanly
      const bool isAttack = title.containsIgnoreCase("ATTACK");
      if (isAttack) {
        g.drawText(title, 0, 0, static_cast<int>(b.getWidth()), 13,
                   juce::Justification::centred, false);
      } else {
        g.drawText(title, 0, static_cast<int>(b.getHeight() - 14.0f),
                   static_cast<int>(b.getWidth()), 13,
                   juce::Justification::centred, false);
      }
      if (subReadout.isNotEmpty()) {
        g.setColour(isOff ? juce::Colour(0xfff87171)
                          : juce::Colour(0xff93c5fd));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        const int ry = isAttack ? static_cast<int>(b.getHeight() - 13.0f) : 0;
        g.drawText(subReadout, 0, ry, static_cast<int>(b.getWidth()), 12,
                   juce::Justification::centred, false);
      }
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    dragStartVal = curV;
    dragStartY = e.position.y;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float dy = dragStartY - e.position.y;
    const double delta = (static_cast<double>(dy) / 130.0) * (maxV - minV);
    setValue(dragStartVal + delta, juce::sendNotificationSync);
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {
    setValue(defV, juce::sendNotificationSync);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    const double step = (maxV - minV) * 0.025 * wheel.deltaY;
    setValue(curV + step, juce::sendNotificationSync);
  }

private:
  juce::String title;
  juce::String subReadout;
  Style style;
  double minV = 0.0, maxV = 1.0, defV = 0.5, curV = 0.5;
  double dragStartVal = 0.5;
  float dragStartY = 0.0f;
  bool isOff = false;
};

// ==============================================================================
// 2. Authentic Backlit 1176LN Analog VU & Gain Reduction Meter
// ==============================================================================
class VuMeter1176 final : public juce::Component {
public:
  void setTelemetry(float vuDbValue, float rawGrDb, float rawOutDb,
                    nova::graviton::KT1176MeterMode mode, bool isAllButtons) {
    // Smooth needle ballistics
    needleDb = needleDb * 0.72f + std::clamp(vuDbValue, -20.0f, 3.0f) * 0.28f;
    grDb = rawGrDb;
    outDb = rawOutDb;
    meterMode = mode;
    allButtonsActive = isAllButtons;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();

    // 1. Recessed Black Bakelite Meter Bezel
    g.setColour(juce::Colour(0xff0b0c0e));
    g.fillRoundedRectangle(b, 5.0f);
    g.setColour(juce::Colour(0xff374151));
    g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 1.5f);

    const auto face = b.reduced(8.0f, 8.0f);
    const bool isOff = (meterMode == nova::graviton::KT1176MeterMode::Off);

    // 2. Warm Incandescent Backlit Cream-Amber Meter Face
    if (isOff) {
      juce::ColourGradient offGrad(
          juce::Colour(0xff3f3cbb).withMultipliedBrightness(0.25f),
          face.getCentreX(), face.getY(), juce::Colour(0xff1f1d18),
          face.getCentreX(), face.getBottom(), false);
      g.setGradientFill(offGrad);
    } else {
      juce::ColourGradient lampGrad(
          juce::Colour(0xfffde68a), face.getCentreX(), face.getY() + 10.0f,
          juce::Colour(0xffd97706)
              .interpolatedWith(juce::Colour(0xfffef3c7), 0.55f),
          face.getCentreX(), face.getBottom(), true);
      g.setGradientFill(lampGrad);
    }
    g.fillRoundedRectangle(face, 3.5f);

    // Inner bezel shadow
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawRoundedRectangle(face, 3.5f, 1.8f);

    // 3. VU Scale Arc Geometry
    const float pivotX = face.getCentreX();
    const float pivotY = face.getBottom() + face.getHeight() * 0.32f;
    const float arcR = face.getHeight() * 1.02f;

    // Map dB (-20 to +3) to angle (-48 deg to +48 deg)
    auto dbToAngle = [](float db) noexcept -> float {
      // Non-linear authentic VU spacing:
      // -20 -> 0.0, -10 -> 0.22, -7 -> 0.34, -5 -> 0.44, -3 -> 0.56, -1 ->
      // 0.68, 0 -> 0.75, +1 -> 0.83, +2 -> 0.91, +3 -> 1.0
      struct Pt {
        float d;
        float n;
      };
      static constexpr Pt kMap[] = {
          {-20.0f, 0.00f}, {-10.0f, 0.22f}, {-7.0f, 0.34f}, {-5.0f, 0.44f},
          {-3.0f, 0.56f},  {-1.0f, 0.68f},  {0.0f, 0.75f},  {1.0f, 0.83f},
          {2.0f, 0.91f},   {3.0f, 1.00f}};
      const float c = std::clamp(db, -20.0f, 3.0f);
      float norm = 0.75f;
      for (int i = 0; i < 9; ++i) {
        if (c >= kMap[i].d && c <= kMap[i + 1].d) {
          const float t = (c - kMap[i].d) / (kMap[i + 1].d - kMap[i].d);
          norm = kMap[i].n + t * (kMap[i + 1].n - kMap[i].n);
          break;
        }
      }
      constexpr float minA = -0.78f; // -45 deg
      constexpr float maxA = 0.78f;  // +45 deg
      return minA + norm * (maxA - minA);
    };

    const juce::Colour inkDark =
        isOff ? juce::Colour(0xff57534e) : juce::Colour(0xff1c1917);
    const juce::Colour inkRed =
        isOff ? juce::Colour(0xff7f1d1d) : juce::Colour(0xffdc2626);

    // Draw red overload wedge from 0 VU to +3 VU
    const float ang0 = dbToAngle(0.0f);
    const float ang3 = dbToAngle(3.0f);
    juce::Path redArc;
    redArc.addCentredArc(pivotX, pivotY, arcR - 3.0f, arcR - 3.0f, 0.0f, ang0,
                         ang3, true);
    g.setColour(inkRed);
    g.strokePath(redArc, juce::PathStrokeType(4.5f));

    // Draw black main arc from -20 to 0 VU
    const float angM20 = dbToAngle(-20.0f);
    juce::Path blackArc;
    blackArc.addCentredArc(pivotX, pivotY, arcR, arcR, 0.0f, angM20, ang0,
                           true);
    g.setColour(inkDark);
    g.strokePath(blackArc, juce::PathStrokeType(1.4f));

    // Scale ticks & numbers
    static constexpr float kTicks[] = {-20.0f, -10.0f, -7.0f, -5.0f, -3.0f,
                                       -1.0f,  0.0f,   1.0f,  2.0f,  3.0f};
    static const char *kTickStr[] = {"-20", "-10", "-7", "-5", "-3",
                                     "-1",  "0",   "+1", "+2", "+3"};
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));

    for (int i = 0; i < 10; ++i) {
      const float a = dbToAngle(kTicks[i]);
      const bool isRed = (kTicks[i] > 0.0f);
      g.setColour(isRed ? inkRed : inkDark);

      const float x1 = pivotX + std::sin(a) * (arcR - 5.0f);
      const float y1 = pivotY - std::cos(a) * (arcR - 5.0f);
      const float x2 = pivotX + std::sin(a) * (arcR + 5.0f);
      const float y2 = pivotY - std::cos(a) * (arcR + 5.0f);
      g.drawLine(x1, y1, x2, y2, 1.4f);

      const float lx = pivotX + std::sin(a) * (arcR + 13.5f);
      const float ly = pivotY - std::cos(a) * (arcR + 13.5f);
      g.drawText(kTickStr[i], static_cast<int>(lx - 11.0f),
                 static_cast<int>(ly - 6.0f), 22, 12,
                 juce::Justification::centred, false);
    }

    // Sub-scale modulation percentage: 0, 20, 40, 60, 80, 100%
    g.setColour(inkDark.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::plain)));
    static const char *kPctStr[] = {"0", "20", "40", "60", "80", "100"};
    static constexpr float kPctDb[] = {-20.0f, -10.0f, -6.0f,
                                       -3.5f,  -1.5f,  0.0f};
    for (int i = 0; i < 6; ++i) {
      const float a = dbToAngle(kPctDb[i]);
      const float px = pivotX + std::sin(a) * (arcR - 14.0f);
      const float py = pivotY - std::cos(a) * (arcR - 14.0f);
      g.drawText(kPctStr[i], static_cast<int>(px - 10.0f),
                 static_cast<int>(py - 5.0f), 20, 10,
                 juce::Justification::centred, false);
    }

    // "VU" corner labels & center UA diamond emblem
    g.setColour(inkDark);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText("VU", static_cast<int>(face.getX() + 6.0f),
               static_cast<int>(face.getY() + 4.0f), 24, 12,
               juce::Justification::centredLeft);
    g.setColour(inkRed);
    g.drawText("VU", static_cast<int>(face.getRight() - 30.0f),
               static_cast<int>(face.getY() + 4.0f), 24, 12,
               juce::Justification::centredRight);

    // 4. Ballistic Needle with Drop Shadow
    const float needleAngle = dbToAngle(needleDb);
    const float tipLen = arcR + 7.0f;
    const float baseLen = arcR * 0.22f;
    const float nx1 = pivotX + std::sin(needleAngle) * baseLen;
    const float ny1 = pivotY - std::cos(needleAngle) * baseLen;
    const float nx2 = pivotX + std::sin(needleAngle) * tipLen;
    const float ny2 = pivotY - std::cos(needleAngle) * tipLen;

    // Needle shadow
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.drawLine(nx1 + 2.0f, ny1 + 2.0f, nx2 + 2.0f, ny2 + 2.0f, 2.0f);

    // Needle body
    g.setColour(inkDark);
    g.drawLine(nx1, ny1, nx2, ny2, 1.8f);

    // 5. Bottom Digital Telemetry Badge Inside Meter Window
    const auto badge = juce::Rectangle<float>(
        face.getCentreX() - 56.0f, face.getBottom() - 18.0f, 112.0f, 15.0f);
    g.setColour(juce::Colour(0xff1c1917).withAlpha(0.85f));
    g.fillRoundedRectangle(badge, 2.5f);
    g.setColour(allButtonsActive ? juce::Colour(0xfff59e0b)
                                 : juce::Colour(0xfffef08a));
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));

    juce::String teleText;
    if (isOff) {
      teleText = "METER: OFF";
    } else if (meterMode == nova::graviton::KT1176MeterMode::GR) {
      teleText = "GR: -" + juce::String(grDb, 1) + " dB";
      if (allButtonsActive)
        teleText += " [NUKE]";
    } else if (meterMode == nova::graviton::KT1176MeterMode::Plus8) {
      teleText = "OUT (+8): " + juce::String(outDb, 1) + " dB";
    } else {
      teleText = "OUT (+4): " + juce::String(outDb, 1) + " dB";
    }
    g.drawText(teleText, badge, juce::Justification::centred, false);
  }

private:
  float needleDb = 0.0f;
  float grDb = 0.0f;
  float outDb = -60.0f;
  nova::graviton::KT1176MeterMode meterMode =
      nova::graviton::KT1176MeterMode::GR;
  bool allButtonsActive = false;
};

// ==============================================================================
// 3. Main KT1176Panel Component (Authentic 2U 1176LN Blackface Hardware Panel)
// ==============================================================================
class KT1176Panel final : public juce::Component, private juce::Timer {
public:
  explicit KT1176Panel(nova::graviton::KT1176Limiter &limiterEngine)
      : engine(&limiterEngine),
        inputKnob("INPUT", Knob1176::Style::LargeFluted, 0.0, 48.0, 24.0),
        outputKnob("OUTPUT", Knob1176::Style::LargeFluted, 0.0, 48.0, 24.0),
        attackKnob("ATTACK", Knob1176::Style::SmallSkirted, 1.0, 7.0, 4.0),
        releaseKnob("RELEASE", Knob1176::Style::SmallSkirted, 1.0, 7.0, 4.0) {
    // ── Top Studio Toolbar: Presets & Advanced Circuit Controls ──
    presetBtn.setButtonText("PRESET");
    presetBtn.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff21262d));
    presetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetBtn);

    const auto &presets = nova::graviton::KT1176Limiter::getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
      presetCombo.addItem(presets[i].name, static_cast<int>(i + 1));
    }
    presetCombo.onChange = [this] {
      const int id = presetCombo.getSelectedId();
      if (id >= 1 && engine) {
        engine->loadPresetByIndex(id - 1);
        updateAllUiFromEngine();
      }
    };
    addAndMakeVisible(presetCombo);

    prevPresetBtn.setButtonText("<");
    prevPresetBtn.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff21262d));
    prevPresetBtn.onClick = [this] { stepPreset(-1); };
    addAndMakeVisible(prevPresetBtn);

    nextPresetBtn.setButtonText(">");
    nextPresetBtn.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff21262d));
    nextPresetBtn.onClick = [this] { stepPreset(1); };
    addAndMakeVisible(nextPresetBtn);

    // Circuit Revision Selector (Rev A Bluestripe / Rev D LN / Rev E Blackface)
    btnRevA.setButtonText("REV A (Blue)");
    btnRevD.setButtonText("REV D (LN)");
    btnRevE.setButtonText("REV E (Black)");
    int revIdx = 0;
    for (auto *b : {&btnRevA, &btnRevD, &btnRevE}) {
      b->setClickingTogglesState(true);
      b->setRadioGroupId(11761, juce::dontSendNotification);
      b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f242c));
      b->setColour(juce::TextButton::buttonOnColourId,
                   revIdx == 0   ? juce::Colour(0xff0284c7)
                   : revIdx == 1 ? juce::Colour(0xffd97706)
                                 : juce::Colour(0xff374151));
      const int capRev = revIdx++;
      b->onClick = [this, capRev] {
        if (engine) {
          engine->setRevision(
              static_cast<nova::graviton::KT1176Revision>(capRev));
          updateAllUiFromEngine();
        }
      };
      addAndMakeVisible(*b);
    }

    // Transformer Coloration Toggle
    btnTransformer.setButtonText("XFMR: ON");
    btnTransformer.setClickingTogglesState(true);
    btnTransformer.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff21262d));
    btnTransformer.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xffb45309));
    btnTransformer.onClick = [this] {
      if (engine) {
        engine->setTransformerColor(btnTransformer.getToggleState());
        updateAllUiFromEngine();
      }
    };
    addAndMakeVisible(btnTransformer);

    // FET Character Selector (0 - 100%)
    fetCharCombo.addItem("FET: 0% (Clean)", 1);
    fetCharCombo.addItem("FET: 50% (Warm)", 2);
    fetCharCombo.addItem("FET: 100% (Vintage)", 3);
    fetCharCombo.onChange = [this] {
      if (!engine)
        return;
      const int id = fetCharCombo.getSelectedId();
      const double pct = (id == 1) ? 0.0 : (id == 2) ? 50.0 : 100.0;
      engine->setFetCharacter(pct);
    };
    addAndMakeVisible(fetCharCombo);

    // Analog Noise Toggle
    btnAnalogNoise.setButtonText("NOISE: OFF");
    btnAnalogNoise.setClickingTogglesState(true);
    btnAnalogNoise.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff21262d));
    btnAnalogNoise.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff0f766e));
    btnAnalogNoise.onClick = [this] {
      if (engine) {
        engine->setAnalogNoise(btnAnalogNoise.getToggleState());
        updateAllUiFromEngine();
      }
    };
    addAndMakeVisible(btnAnalogNoise);

    // Sidechain HPF Selector (Off / 60 Hz / 90 Hz / 150 Hz)
    scHpfCombo.addItem("SC HPF: OFF", 1);
    scHpfCombo.addItem("SC HPF: 60 Hz", 2);
    scHpfCombo.addItem("SC HPF: 90 Hz", 3);
    scHpfCombo.addItem("SC HPF: 150 Hz", 4);
    scHpfCombo.onChange = [this] {
      if (!engine)
        return;
      const int id = scHpfCombo.getSelectedId();
      const double hz = (id == 2)   ? 60.0
                        : (id == 3) ? 90.0
                        : (id == 4) ? 150.0
                                    : 0.0;
      engine->setSidechainHpfHz(hz);
    };
    addAndMakeVisible(scHpfCombo);

    // Parallel Mix Slider (0 - 100%)
    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setRange(0.0, 100.0, 1.0);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
    mixSlider.setTextValueSuffix("%");
    mixSlider.onValueChange = [this] {
      if (engine)
        engine->setMix(mixSlider.getValue());
    };
    addAndMakeVisible(mixSlider);

    // Power / Bypass Button
    powerBtn.setButtonText("POWER");
    powerBtn.setClickingTogglesState(true);
    powerBtn.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xffda3633));
    powerBtn.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff2ea043));
    powerBtn.onClick = [this] {
      if (!engine)
        return;
      const bool active = powerBtn.getToggleState();
      engine->setBypass(!active);
      if (active &&
          engine->getMeterMode() == nova::graviton::KT1176MeterMode::Off) {
        engine->setMeterMode(nova::graviton::KT1176MeterMode::GR);
      }
      updateAllUiFromEngine();
    };
    addAndMakeVisible(powerBtn);

    // ── Hardware Faceplate Controls ──
    // 1. INPUT & OUTPUT Knobs
    inputKnob.onValueChanged = [this](double dialVal) {
      if (!engine)
        return;
      engine->setInputDial(dialVal);
      updateAllUiFromEngine();
    };
    addAndMakeVisible(inputKnob);

    outputKnob.onValueChanged = [this](double dialVal) {
      if (!engine)
        return;
      engine->setOutputDial(dialVal);
      updateAllUiFromEngine();
    };
    addAndMakeVisible(outputKnob);

    // 2. ATTACK & RELEASE Knobs + Attack OFF Switch
    attackKnob.onValueChanged = [this](double dialVal) {
      if (!engine)
        return;
      if (engine->getAttackOff())
        engine->setAttackOff(false);
      engine->setAttackDial(dialVal);
      updateAllUiFromEngine();
    };
    addAndMakeVisible(attackKnob);

    btnAttackOff.setButtonText("OFF");
    btnAttackOff.setClickingTogglesState(true);
    btnAttackOff.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xff1f242c));
    btnAttackOff.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xffdc2626));
    btnAttackOff.onClick = [this] {
      if (!engine)
        return;
      engine->setAttackOff(btnAttackOff.getToggleState());
      updateAllUiFromEngine();
    };
    addAndMakeVisible(btnAttackOff);

    releaseKnob.onValueChanged = [this](double dialVal) {
      if (!engine)
        return;
      engine->setReleaseDial(dialVal);
      updateAllUiFromEngine();
    };
    addAndMakeVisible(releaseKnob);

    // 3. RATIO Pushbutton Bank (Top to Bottom: 20, 12, 8, 4 + ALL IN)
    static const char *kRatioLabels[4] = {"20", "12", "8", "4"};
    static constexpr nova::graviton::KT1176Ratio kRatioEnums[4] = {
        nova::graviton::KT1176Ratio::Ratio_20_1,
        nova::graviton::KT1176Ratio::Ratio_12_1,
        nova::graviton::KT1176Ratio::Ratio_8_1,
        nova::graviton::KT1176Ratio::Ratio_4_1};
    for (int i = 0; i < 4; ++i) {
      auto &b = ratioButtons[static_cast<size_t>(i)];
      b.setButtonText(kRatioLabels[i]);
      b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff181a1f));
      b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
      const auto targetRatio = kRatioEnums[i];
      b.onClick = [this, targetRatio] {
        if (!engine)
          return;
        if (juce::ModifierKeys::currentModifiers.isShiftDown()) {
          engine->setRatio(nova::graviton::KT1176Ratio::AllButtons);
        } else {
          engine->setRatio(targetRatio);
        }
        updateAllUiFromEngine();
      };
      addAndMakeVisible(b);
    }

    btnAllButtons.setButtonText("ALL IN (NUKE)");
    btnAllButtons.setClickingTogglesState(true);
    btnAllButtons.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff1f242c));
    btnAllButtons.setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(0xffdc2626));
    btnAllButtons.onClick = [this] {
      if (!engine)
        return;
      engine->setRatio(btnAllButtons.getToggleState()
                           ? nova::graviton::KT1176Ratio::AllButtons
                           : nova::graviton::KT1176Ratio::Ratio_4_1);
      updateAllUiFromEngine();
    };
    addAndMakeVisible(btnAllButtons);

    // 4. VU Meter
    addAndMakeVisible(vuMeter);

    // 5. METER Pushbutton Bank (Top to Bottom: GR, +8, +4, OFF)
    static const char *kMeterLabels[4] = {"GR", "+8", "+4", "OFF"};
    static constexpr nova::graviton::KT1176MeterMode kMeterEnums[4] = {
        nova::graviton::KT1176MeterMode::GR,
        nova::graviton::KT1176MeterMode::Plus8,
        nova::graviton::KT1176MeterMode::Plus4,
        nova::graviton::KT1176MeterMode::Off};
    for (int i = 0; i < 4; ++i) {
      auto &b = meterButtons[static_cast<size_t>(i)];
      b.setButtonText(kMeterLabels[i]);
      b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff181a1f));
      b.setColour(juce::TextButton::buttonOnColourId,
                  i == 3 ? juce::Colour(0xffdc2626) : juce::Colour(0xffd97706));
      const auto targetMode = kMeterEnums[i];
      b.onClick = [this, targetMode] {
        if (!engine)
          return;
        engine->setMeterMode(targetMode);
        engine->setBypass(targetMode == nova::graviton::KT1176MeterMode::Off);
        updateAllUiFromEngine();
      };
      addAndMakeVisible(b);
    }

    updateAllUiFromEngine();
    startTimerHz(30);
    setSize(1120, 310);
  }

  ~KT1176Panel() override { stopTimer(); }

  void setEngine(nova::graviton::KT1176Limiter &newEngine) {
    engine = &newEngine;
    updateAllUiFromEngine();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds();

    // 1. Top Toolbar Bar
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(0, 0, b.getWidth(), 30);
    g.setColour(juce::Colour(0xff30363d));
    g.drawHorizontalLine(29, 0.0f, static_cast<float>(b.getWidth()));

    g.setColour(juce::Colour(0xff8b949e));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("MIX:", mixLabelBounds, juce::Justification::centredRight,
               false);

    // 2. 1176LN 2U Brushed Black Anodized Aluminum Faceplate
    const auto chassis = juce::Rectangle<float>(
        4.0f, 32.0f, static_cast<float>(b.getWidth() - 8),
        static_cast<float>(b.getHeight() - 36));
    juce::ColourGradient metalGrad(juce::Colour(0xff1c1e22), chassis.getX(),
                                   chassis.getY(), juce::Colour(0xff111316),
                                   chassis.getX(), chassis.getBottom(), false);
    g.setGradientFill(metalGrad);
    g.fillRoundedRectangle(chassis, 4.0f);

    // Rev A Iconic Blue Stripe across the meter & ratio section when Rev A is
    // active!
    if (engine && engine->getRevision() ==
                      nova::graviton::KT1176Revision::RevA_Bluestripe) {
      const auto stripeRect = juce::Rectangle<float>(
          static_cast<float>(vuMeter.getX() - 60), chassis.getY() + 2.0f,
          static_cast<float>(vuMeter.getWidth() + 145),
          chassis.getHeight() - 4.0f);
      g.setColour(juce::Colour(0xff0369a1).withAlpha(0.38f));
      g.fillRect(stripeRect);
    }

    // Subtle horizontal brushed-metal hairline texture
    g.setColour(juce::Colours::white.withAlpha(0.018f));
    for (int y = static_cast<int>(chassis.getY()) + 3;
         y < static_cast<int>(chassis.getBottom()); y += 3) {
      g.drawHorizontalLine(y, chassis.getX() + 2.0f, chassis.getRight() - 2.0f);
    }

    // Outer metallic bevel
    g.setColour(juce::Colour(0xff374151));
    g.drawRoundedRectangle(chassis, 4.0f, 1.4f);

    // 3. Rack Ears & 4 Mounting Screws + 4 Faceplate Screws
    auto drawRackHole = [&](float x, float y) {
      g.setColour(juce::Colour(0xff050608));
      g.fillRoundedRectangle(x, y, 22.0f, 11.0f, 5.5f);
      g.setColour(juce::Colour(0xff374151));
      g.drawRoundedRectangle(x, y, 22.0f, 11.0f, 5.5f, 1.0f);
    };
    drawRackHole(chassis.getX() + 10.0f, chassis.getY() + 14.0f);
    drawRackHole(chassis.getX() + 10.0f, chassis.getBottom() - 25.0f);
    drawRackHole(chassis.getRight() - 32.0f, chassis.getY() + 14.0f);
    drawRackHole(chassis.getRight() - 32.0f, chassis.getBottom() - 25.0f);

    auto drawPhillipsScrew = [&](float cx, float cy) {
      g.setColour(juce::Colour(0xff0b0d10));
      g.fillEllipse(cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
      g.setColour(juce::Colour(0xff374151));
      g.drawEllipse(cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, 1.0f);
      g.setColour(juce::Colour(0xff1f242c));
      g.drawLine(cx - 3.5f, cy, cx + 3.5f, cy, 1.4f);
      g.drawLine(cx, cy - 3.5f, cx, cy + 3.5f, 1.4f);
    };
    drawPhillipsScrew(chassis.getX() + 52.0f, chassis.getY() + 28.0f);
    drawPhillipsScrew(chassis.getX() + 52.0f, chassis.getBottom() - 28.0f);
    drawPhillipsScrew(chassis.getRight() - 52.0f, chassis.getY() + 28.0f);
    drawPhillipsScrew(chassis.getRight() - 52.0f, chassis.getBottom() - 28.0f);

    // 4. Pushbutton Bank Bezels & Silk-Screen Labels (RATIO & METER)
    auto drawButtonBankBezel = [&](juce::Rectangle<int> bankRect,
                                   const juce::String &title) {
      g.setColour(juce::Colour(0xfff3f4f6));
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.drawText(title, bankRect.getX() - 10, bankRect.getY() - 18,
                 bankRect.getWidth() + 20, 15, juce::Justification::centred,
                 false);

      g.setColour(juce::Colour(0xff090a0d));
      g.fillRoundedRectangle(bankRect.expanded(3, 3).toFloat(), 3.0f);
      g.setColour(juce::Colour(0xff374151));
      g.drawRoundedRectangle(bankRect.expanded(3, 3).toFloat(), 3.0f, 1.2f);
    };
    drawButtonBankBezel(ratioBankBounds, "RATIO");
    drawButtonBankBezel(meterBankBounds, "METER");

    // 5. UA Classics Diamond Emblem (Above VU Meter) & 1176LN Branding (Below
    // VU Meter)
    const auto mBounds = vuMeter.getBounds().toFloat();
    const float emblemCx = mBounds.getCentreX() - 16.0f;
    const float emblemCy = mBounds.getY() - 16.0f;

    juce::Path diamond;
    diamond.startNewSubPath(emblemCx, emblemCy - 9.0f);
    diamond.lineTo(emblemCx + 20.0f, emblemCy);
    diamond.lineTo(emblemCx, emblemCy + 9.0f);
    diamond.lineTo(emblemCx - 20.0f, emblemCy);
    diamond.closeSubPath();
    g.setColour(juce::Colour(0xfff3f4f6));
    g.strokePath(diamond, juce::PathStrokeType(1.5f));
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText("UA", static_cast<int>(emblemCx - 14.0f),
               static_cast<int>(emblemCy - 6.0f), 28, 12,
               juce::Justification::centred, false);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.drawText("CLASSICS", static_cast<int>(emblemCx + 23.0f),
               static_cast<int>(emblemCy - 5.0f), 65, 12,
               juce::Justification::centredLeft, false);

    // Bottom silk-screened model typography below VU meter
    g.setColour(juce::Colour(0xfff3f4f6));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("1176LN  LIMITING AMPLIFIER",
               static_cast<int>(mBounds.getX() - 10.0f),
               static_cast<int>(mBounds.getBottom() + 6.0f),
               static_cast<int>(mBounds.getWidth() + 20.0f), 15,
               juce::Justification::centred, false);

    g.setColour(juce::Colour(0xffd1d5db));
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.drawText("UNIVERSAL AUDIO", static_cast<int>(mBounds.getX() - 10.0f),
               static_cast<int>(mBounds.getBottom() + 21.0f),
               static_cast<int>(mBounds.getWidth() + 20.0f), 17,
               juce::Justification::centred, false);
  }

  void resized() override {
    const int w = getWidth();
    const int h = getHeight();

    // ── Top Toolbar ──
    const int tbY = 4;
    int tx = 6;
    presetBtn.setBounds(tx, tbY, 52, 22);
    tx += 55;
    presetCombo.setBounds(tx, tbY, 180, 22);
    tx += 183;
    prevPresetBtn.setBounds(tx, tbY, 20, 22);
    tx += 22;
    nextPresetBtn.setBounds(tx, tbY, 20, 22);
    tx += 24;

    btnRevA.setBounds(tx, tbY, 74, 22);
    tx += 76;
    btnRevD.setBounds(tx, tbY, 70, 22);
    tx += 72;
    btnRevE.setBounds(tx, tbY, 76, 22);
    tx += 79;

    fetCharCombo.setBounds(tx, tbY, 108, 22);
    tx += 111;
    btnTransformer.setBounds(tx, tbY, 74, 22);
    tx += 77;
    btnAnalogNoise.setBounds(tx, tbY, 76, 22);
    tx += 79;
    scHpfCombo.setBounds(tx, tbY, 106, 22);
    tx += 109;

    mixLabelBounds = juce::Rectangle<int>(tx, tbY, 30, 22);
    tx += 32;
    mixSlider.setBounds(tx, tbY, std::max(70, w - tx - 72), 22);
    powerBtn.setBounds(w - 66, tbY, 60, 22);

    // ── Main 2U 1176LN Faceplate Layout ──
    const int faceY = 34;
    const int faceH = h - faceY - 6;
    const int cy = faceY + faceH / 2;

    // 1. Large INPUT & OUTPUT Knobs (Left Side)
    const int largeKnobSz = std::min(190, faceH - 12);
    const int inX = static_cast<int>(w * 0.08f);
    const int outX = static_cast<int>(w * 0.25f);
    inputKnob.setBounds(inX, cy - largeKnobSz / 2, largeKnobSz, largeKnobSz);
    outputKnob.setBounds(outX, cy - largeKnobSz / 2, largeKnobSz, largeKnobSz);

    // 2. Stacked ATTACK & RELEASE Knobs (Center)
    const int smallKnobSz = std::min(112, (faceH - 22) / 2);
    const int attRelX = static_cast<int>(w * 0.44f);
    attackKnob.setBounds(attRelX, faceY + 8, smallKnobSz, smallKnobSz);
    btnAttackOff.setBounds(attRelX + smallKnobSz / 2 - 22,
                           faceY + 8 + smallKnobSz - 2, 44, 16);
    releaseKnob.setBounds(attRelX, faceY + faceH - smallKnobSz - 6, smallKnobSz,
                          smallKnobSz);

    // 3. RATIO Pushbutton Bank (Center-Right)
    const int ratioX = static_cast<int>(w * 0.59f);
    const int bankH = std::min(148, faceH - 64);
    const int bankY = cy - bankH / 2 - 6;
    const int btnW = 38;
    const int btnH = (bankH - 6) / 4;
    ratioBankBounds = juce::Rectangle<int>(ratioX, bankY, btnW, bankH);
    for (int i = 0; i < 4; ++i) {
      ratioButtons[static_cast<size_t>(i)].setBounds(
          ratioX, bankY + i * (btnH + 2), btnW, btnH);
    }
    btnAllButtons.setBounds(ratioX - 24, bankY + bankH + 8, btnW + 48, 20);

    // 4. Backlit VU Meter (Right)
    const int meterX = static_cast<int>(w * 0.665f);
    const int meterW = static_cast<int>(w * 0.195f);
    const int meterH = std::min(142, faceH - 72);
    const int meterY = cy - meterH / 2 - 6;
    vuMeter.setBounds(meterX, meterY, meterW, meterH);

    // 5. METER Pushbutton Bank (Far-Right)
    const int mBankX = static_cast<int>(w * 0.885f);
    meterBankBounds = juce::Rectangle<int>(mBankX, bankY, btnW, bankH);
    for (int i = 0; i < 4; ++i) {
      meterButtons[static_cast<size_t>(i)].setBounds(
          mBankX, bankY + i * (btnH + 2), btnW, btnH);
    }
  }

private:
  void stepPreset(int delta) {
    if (!engine)
      return;
    const auto &all = nova::graviton::KT1176Limiter::getFactoryPresets();
    if (all.empty())
      return;
    const int n = static_cast<int>(all.size());
    const int nextIdx = (engine->getCurrentPresetIndex() + delta + n) % n;
    engine->loadPresetByIndex(nextIdx);
    updateAllUiFromEngine();
  }

  void showPresetMenu() {
    juce::PopupMenu m;
    const auto &all = nova::graviton::KT1176Limiter::getFactoryPresets();
    const int cur = engine ? engine->getCurrentPresetIndex() : 0;
    for (size_t i = 0; i < all.size(); ++i) {
      m.addItem(static_cast<int>(i + 1), all[i].name, true,
                static_cast<int>(i) == cur);
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
                    [this](int res) {
                      if (res >= 1 && engine) {
                        engine->loadPresetByIndex(res - 1);
                        updateAllUiFromEngine();
                      }
                    });
  }

  void updateAllUiFromEngine() {
    if (!engine)
      return;

    presetCombo.setSelectedId(engine->getCurrentPresetIndex() + 1,
                              juce::dontSendNotification);

    // Input & Output Knobs
    inputKnob.setValue(engine->getInputDial(), juce::dontSendNotification);
    const double inDb = engine->getInputDb();
    inputKnob.setSubReadout((inDb >= 0.0 ? "+" : "") + juce::String(inDb, 1) +
                            " dB");

    outputKnob.setValue(engine->getOutputDial(), juce::dontSendNotification);
    const double outDb = engine->getOutputDb();
    outputKnob.setSubReadout((outDb >= 0.0 ? "+" : "") +
                             juce::String(outDb, 1) + " dB");

    // Attack & Release Knobs
    const bool attOff = engine->getAttackOff();
    attackKnob.setValue(engine->getAttackDial(), juce::dontSendNotification);
    attackKnob.setOffState(attOff);
    attackKnob.setSubReadout(attOff ? "COMP OFF"
                                    : (juce::String(static_cast<int>(
                                           std::round(engine->getAttackUs()))) +
                                       " \xc2\xb5s"));
    btnAttackOff.setToggleState(attOff, juce::dontSendNotification);

    releaseKnob.setValue(engine->getReleaseDial(), juce::dontSendNotification);
    releaseKnob.setSubReadout(
        juce::String(static_cast<int>(std::round(engine->getReleaseMs()))) +
        " ms");

    // Ratio Buttons (20, 12, 8, 4 + All Buttons In)
    const auto r = engine->getRatio();
    const bool isAll = (r == nova::graviton::KT1176Ratio::AllButtons);
    ratioButtons[0].setToggleState(
        isAll || r == nova::graviton::KT1176Ratio::Ratio_20_1,
        juce::dontSendNotification);
    ratioButtons[1].setToggleState(
        isAll || r == nova::graviton::KT1176Ratio::Ratio_12_1,
        juce::dontSendNotification);
    ratioButtons[2].setToggleState(
        isAll || r == nova::graviton::KT1176Ratio::Ratio_8_1,
        juce::dontSendNotification);
    ratioButtons[3].setToggleState(
        isAll || r == nova::graviton::KT1176Ratio::Ratio_4_1,
        juce::dontSendNotification);
    btnAllButtons.setToggleState(isAll, juce::dontSendNotification);

    // Meter Mode Buttons (GR, +8, +4, OFF)
    const auto mMode = engine->getMeterMode();
    meterButtons[0].setToggleState(mMode == nova::graviton::KT1176MeterMode::GR,
                                   juce::dontSendNotification);
    meterButtons[1].setToggleState(mMode ==
                                       nova::graviton::KT1176MeterMode::Plus8,
                                   juce::dontSendNotification);
    meterButtons[2].setToggleState(mMode ==
                                       nova::graviton::KT1176MeterMode::Plus4,
                                   juce::dontSendNotification);
    meterButtons[3].setToggleState(mMode ==
                                       nova::graviton::KT1176MeterMode::Off,
                                   juce::dontSendNotification);

    // Revision Buttons
    const auto rev = engine->getRevision();
    btnRevA.setToggleState(rev ==
                               nova::graviton::KT1176Revision::RevA_Bluestripe,
                           juce::dontSendNotification);
    btnRevD.setToggleState(rev ==
                               nova::graviton::KT1176Revision::RevD_LN_Classic,
                           juce::dontSendNotification);
    btnRevE.setToggleState(rev ==
                               nova::graviton::KT1176Revision::RevE_Blackface,
                           juce::dontSendNotification);

    // Transformer, FET Character, Analog Noise, SC HPF, Mix, Power
    btnTransformer.setToggleState(engine->getTransformerColor(),
                                  juce::dontSendNotification);
    btnTransformer.setButtonText(engine->getTransformerColor() ? "XFMR: ON"
                                                               : "XFMR: OFF");

    const double fetC = engine->getFetCharacter();
    const int fetId = (fetC < 25.0) ? 1 : (fetC < 75.0) ? 2 : 3;
    fetCharCombo.setSelectedId(fetId, juce::dontSendNotification);

    btnAnalogNoise.setToggleState(engine->getAnalogNoise(),
                                  juce::dontSendNotification);
    btnAnalogNoise.setButtonText(engine->getAnalogNoise() ? "NOISE: ON"
                                                          : "NOISE: OFF");

    const double hpf = engine->getSidechainHpfHz();
    const int hpfId = (hpf >= 130.0)  ? 4
                      : (hpf >= 80.0) ? 3
                      : (hpf >= 40.0) ? 2
                                      : 1;
    scHpfCombo.setSelectedId(hpfId, juce::dontSendNotification);

    mixSlider.setValue(engine->getMix(), juce::dontSendNotification);

    const bool isPowered =
        !engine->getBypass() && (mMode != nova::graviton::KT1176MeterMode::Off);
    powerBtn.setToggleState(isPowered, juce::dontSendNotification);
    powerBtn.setButtonText(isPowered ? "POWER" : "BYPASS");

    repaint();
  }

  void timerCallback() override {
    if (!engine)
      return;
    vuMeter.setTelemetry(engine->getVuNeedleDb(), engine->getMeterGrDb(),
                         engine->getMeterOutDb(), engine->getMeterMode(),
                         engine->getRatio() ==
                             nova::graviton::KT1176Ratio::AllButtons);
  }

  nova::graviton::KT1176Limiter *engine = nullptr;

  // Top Bar Controls
  juce::TextButton presetBtn, prevPresetBtn, nextPresetBtn;
  juce::ComboBox presetCombo;
  juce::TextButton btnRevA, btnRevD, btnRevE;
  juce::ComboBox fetCharCombo;
  juce::TextButton btnTransformer;
  juce::TextButton btnAnalogNoise;
  juce::ComboBox scHpfCombo;
  juce::Rectangle<int> mixLabelBounds;
  juce::Slider mixSlider;
  juce::TextButton powerBtn;

  // 1176LN Faceplate Controls
  Knob1176 inputKnob;
  Knob1176 outputKnob;
  Knob1176 attackKnob;
  Knob1176 releaseKnob;
  juce::TextButton btnAttackOff;

  juce::Rectangle<int> ratioBankBounds;
  std::array<juce::TextButton, 4> ratioButtons;
  juce::TextButton btnAllButtons;

  VuMeter1176 vuMeter;

  juce::Rectangle<int> meterBankBounds;
  std::array<juce::TextButton, 4> meterButtons;
};

} // namespace nova
