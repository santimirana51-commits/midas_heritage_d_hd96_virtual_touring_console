#pragma once

// ==============================================================================
// GBusCompressorPanel.h
// NOVA HD — Authentic SSL 4000 G-Series Stereo VCA Bus Compressor UI
//
// Matches the SSL 4000 G Center Section Bus Compressor hardware layout & NOVA HD spec:
//   • Top-Center: Recessed Black Bezel "dB COMPRESSION" Analog Meter (0, 4, 8, 12, 16, 20 dB)
//     with white needle deflecting clockwise from 0 (left) to 20 (right) & calibration screw
//   • Classic SSL Grey Fluted Knobs with White Dot Silkscreen:
//       - THRESHOLD (-15 to +15 dB, 0 at 12 o'clock)
//       - MAKE-UP   (-5 to +15 dB, 7.5 at upper center)
//       - ATTACK -mS (Stepped: .1, .3, 1, 3, 10, 30 ms)
//       - RELEASE -S (Stepped: .1, .3, .6, 1.2 s, Auto)
//       - RATIO      (Stepped: 2:1, 4:1, 10:1)
//       - IN         (Illuminated SSL Console Pushbutton)
//   • NOVA HD G-Bus Advanced Section:
//       - Sidechain HPF (Off / 60 / 90 / 120 / 185 Hz)
//       - Stereo Link (0–100%, default 100% Stereo-Linked)
//       - Parallel Mix (0–100%)
//       - VCA Character (0–100% + DBX 202C Gold Can / 4000G Twin / THAT 2180)
//       - Analog Coloration (On/Off)
// ==============================================================================

#include "Effects/GravitonKTBusComp.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace nova {

// ==============================================================================
// 1. Authentic SSL 4000 G Grey Fluted Knob (Continuous or Stepped Switch)
// ==============================================================================
class GBusKnob final : public juce::Component {
public:
    enum class Mode {
        ContinuousThreshold, // -15 to +15 dB (0 at 12 o'clock)
        ContinuousMakeup,    // -5 to +15 dB (7.5 at 12 o'clock)
        ContinuousPercent,   // 0 to 100% (50% at 12 o'clock)
        SteppedAttack,       // .1, .3, 1, 3, 10, 30
        SteppedRelease,      // .1, .3, .6, 1.2, Auto
        SteppedRatio         // 2, 4, 10
    };

    GBusKnob(const juce::String& titleText, Mode knobMode,
             double minVal, double maxVal, double defaultVal)
        : title(titleText),
          mode(knobMode),
          minV(minVal),
          maxV(maxVal),
          defV(defaultVal),
          curV(defaultVal) {
        setRepaintsOnMouseActivity(true);
    }

    void setValue(double v, juce::NotificationType notify = juce::dontSendNotification) {
        double clamped = std::clamp(v, minV, maxV);
        if (mode == Mode::SteppedAttack || mode == Mode::SteppedRelease || mode == Mode::SteppedRatio) {
            clamped = std::round(clamped);
        }
        if (std::abs(clamped - curV) > 1e-5) {
            curV = clamped;
            repaint();
            if (notify != juce::dontSendNotification && onValueChanged) {
                onValueChanged(curV);
            }
        }
    }

    double getValue() const noexcept { return curV; }

    void setSubReadout(const juce::String& text) {
        if (subReadout != text) {
            subReadout = text;
            repaint();
        }
    }

    std::function<void(double)> onValueChanged;

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX();
        const float cy = b.getCentreY() - 6.0f;
        const float radius = std::min(b.getWidth(), b.getHeight()) * 0.29f;

        // Angle sweep matching SSL 4000 G hardware photo
        float startAng = -2.35619449f; // -135 deg
        float endAng   =  2.35619449f; // +135 deg
        if (mode == Mode::SteppedAttack) {
            startAng = -1.75f; // ~ -100 deg (.1)
            endAng   =  1.65f; // ~ +95 deg (30)
        } else if (mode == Mode::SteppedRelease) {
            startAng = -1.55f; // ~ -88 deg (.1)
            endAng   =  1.55f; // ~ +88 deg (Auto)
        } else if (mode == Mode::SteppedRatio) {
            startAng = -1.05f; // ~ -60 deg (2:1)
            endAng   =  1.05f; // ~ +60 deg (10:1)
        }

        const float norm = static_cast<float>((curV - minV) / std::max(1e-5, maxV - minV));
        const float angle = startAng + norm * (endAng - startAng);

        // 1. White Silkscreen Dots & Markings Around Knob (Exact SSL 4000 G Style)
        g.setColour(juce::Colour(0xfff3f4f6));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));

        if (mode == Mode::ContinuousThreshold || mode == Mode::ContinuousMakeup || mode == Mode::ContinuousPercent) {
            const float dotDist = radius + 8.0f;
            for (int i = 0; i <= 10; ++i) {
                const float t = static_cast<float>(i) / 10.0f;
                const float a = startAng + t * (endAng - startAng);
                const float dx = cx + std::sin(a) * dotDist;
                const float dy = cy - std::cos(a) * dotDist;
                const float rDot = (i == 0 || i == 5 || i == 10) ? 2.2f : 1.7f;
                g.fillEllipse(dx - rDot, dy - rDot, rDot * 2.0f, rDot * 2.0f);
            }
            if (mode == Mode::ContinuousThreshold) {
                g.drawText("0", static_cast<int>(cx - 12.0f), static_cast<int>(cy - radius - 22.0f), 24, 13, juce::Justification::centred);
                g.drawText("-15", static_cast<int>(cx - radius - 14.0f), static_cast<int>(cy + radius - 2.0f), 28, 13, juce::Justification::centred);
                g.drawText("+15", static_cast<int>(cx + radius - 14.0f), static_cast<int>(cy + radius - 2.0f), 28, 13, juce::Justification::centred);
            } else if (mode == Mode::ContinuousMakeup) {
                g.drawText("7.5", static_cast<int>(cx - 15.0f), static_cast<int>(cy - radius - 22.0f), 30, 13, juce::Justification::centred);
                g.drawText("-5", static_cast<int>(cx - radius - 12.0f), static_cast<int>(cy + radius - 2.0f), 24, 13, juce::Justification::centred);
                g.drawText("+15", static_cast<int>(cx + radius - 14.0f), static_cast<int>(cy + radius - 2.0f), 28, 13, juce::Justification::centred);
            } else {
                g.drawText("50%", static_cast<int>(cx - 16.0f), static_cast<int>(cy - radius - 22.0f), 32, 13, juce::Justification::centred);
                g.drawText("0", static_cast<int>(cx - radius - 10.0f), static_cast<int>(cy + radius - 2.0f), 20, 13, juce::Justification::centred);
                g.drawText("100", static_cast<int>(cx + radius - 14.0f), static_cast<int>(cy + radius - 2.0f), 28, 13, juce::Justification::centred);
            }
        } else if (mode == Mode::SteppedAttack) {
            static const char* kAttLabels[6] = { ".1", ".3", "1", "3", "10", "30" };
            const float dotDist = radius + 7.5f;
            const float lblDist = radius + 17.5f;
            for (int i = 0; i < 6; ++i) {
                const float t = static_cast<float>(i) / 5.0f;
                const float a = startAng + t * (endAng - startAng);
                const float dx = cx + std::sin(a) * dotDist;
                const float dy = cy - std::cos(a) * dotDist;
                g.fillEllipse(dx - 2.1f, dy - 2.1f, 4.2f, 4.2f);

                const float lx = cx + std::sin(a) * lblDist;
                const float ly = cy - std::cos(a) * lblDist;
                g.drawText(kAttLabels[i], static_cast<int>(lx - 13.0f), static_cast<int>(ly - 7.0f), 26, 14, juce::Justification::centred);
            }
        } else if (mode == Mode::SteppedRelease) {
            static const char* kRelLabels[5] = { ".1", ".3", ".6", "1.2", "Auto" };
            const float dotDist = radius + 7.5f;
            const float lblDist = radius + 18.5f;
            for (int i = 0; i < 5; ++i) {
                const float t = static_cast<float>(i) / 4.0f;
                const float a = startAng + t * (endAng - startAng);
                const float dx = cx + std::sin(a) * dotDist;
                const float dy = cy - std::cos(a) * dotDist;
                g.fillEllipse(dx - 2.1f, dy - 2.1f, 4.2f, 4.2f);

                const float lx = cx + std::sin(a) * lblDist + (i == 4 ? 6.0f : 0.0f);
                const float ly = cy - std::cos(a) * lblDist;
                g.drawText(kRelLabels[i], static_cast<int>(lx - 16.0f), static_cast<int>(ly - 7.0f), 32, 14, juce::Justification::centred);
            }
        } else if (mode == Mode::SteppedRatio) {
            static const char* kRatLabels[3] = { "2", "4", "10" };
            const float dotDist = radius + 7.5f;
            const float lblDist = radius + 17.5f;
            for (int i = 0; i < 3; ++i) {
                const float t = static_cast<float>(i) / 2.0f;
                const float a = startAng + t * (endAng - startAng);
                const float dx = cx + std::sin(a) * dotDist;
                const float dy = cy - std::cos(a) * dotDist;
                g.fillEllipse(dx - 2.1f, dy - 2.1f, 4.2f, 4.2f);

                const float lx = cx + std::sin(a) * lblDist;
                const float ly = cy - std::cos(a) * lblDist;
                g.drawText(kRatLabels[i], static_cast<int>(lx - 12.0f), static_cast<int>(ly - 7.0f), 24, 14, juce::Justification::centred);
            }
        }

        // 2. Cast Drop Shadow Under SSL Knob
        g.setColour(juce::Colours::black.withAlpha(0.58f));
        g.fillEllipse(cx - radius + 2.0f, cy - radius + 4.5f, radius * 2.0f, radius * 2.0f);

        // 3. Molded SSL Light-Grey Fluted Body with 6 Scalloped Lobes
        juce::ColourGradient bodyGrad(juce::Colour(0xffb8bec6), cx - radius * 0.6f, cy - radius * 0.6f,
                                      juce::Colour(0xff6c727a), cx + radius * 0.6f, cy + radius * 0.6f, false);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff3f444a));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.3f);

        // 6 scalloped grip indents around perimeter (authentic SSL 4000 knob silhouette)
        for (int i = 0; i < 6; ++i) {
            const float la = angle + static_cast<float>(i) * (3.14159265f / 3.0f);
            const float lx = cx + std::sin(la) * (radius * 0.90f);
            const float ly = cy - std::cos(la) * (radius * 0.90f);
            g.setColour(juce::Colour(0xffd1d5db).withAlpha(0.55f));
            g.drawEllipse(lx - 3.5f, ly - 3.5f, 7.0f, 7.0f, 1.1f);
        }

        // 4. Inner Grey Cap
        const float capR = radius * 0.72f;
        juce::ColourGradient capGrad(juce::Colour(0xffc9cfd6), cx - capR * 0.4f, cy - capR * 0.4f,
                                     juce::Colour(0xff888f98), cx + capR * 0.5f, cy + capR * 0.5f, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
        g.setColour(juce::Colour(0xff525860));
        g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        // 5. Bold Black Pointer Line (Matches uploaded SSL photo)
        const float px1 = cx + std::sin(angle) * (capR * 0.12f);
        const float py1 = cy - std::cos(angle) * (capR * 0.12f);
        const float px2 = cx + std::sin(angle) * (radius * 0.92f);
        const float py2 = cy - std::cos(angle) * (radius * 0.92f);
        g.setColour(juce::Colour(0xff111315));
        g.drawLine(px1, py1, px2, py2, 2.8f);

        // 6. Silkscreen Label & Digital Readout Below Knob
        g.setColour(juce::Colour(0xfff3f4f6));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText(title, 0, static_cast<int>(b.getHeight() - 24.0f), static_cast<int>(b.getWidth()), 13,
                   juce::Justification::centred, false);

        if (subReadout.isNotEmpty()) {
            g.setColour(juce::Colour(0xff93c5fd));
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText(subReadout, 0, static_cast<int>(b.getHeight() - 12.0f), static_cast<int>(b.getWidth()), 12,
                       juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragStartVal = curV;
        dragStartY = e.position.y;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float dy = dragStartY - e.position.y;
        const double delta = (static_cast<double>(dy) / 110.0) * (maxV - minV);
        setValue(dragStartVal + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        setValue(defV, juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        const double step = (mode == Mode::SteppedAttack || mode == Mode::SteppedRelease || mode == Mode::SteppedRatio)
                                ? (wheel.deltaY > 0.0f ? 1.0 : -1.0)
                                : ((maxV - minV) * 0.03 * wheel.deltaY);
        setValue(curV + step, juce::sendNotificationSync);
    }

private:
    juce::String title;
    juce::String subReadout;
    Mode mode;
    double minV = 0.0, maxV = 1.0, defV = 0.0, curV = 0.0;
    double dragStartVal = 0.0;
    float dragStartY = 0.0f;
};

// ==============================================================================
// 2. Authentic SSL 4000 G "dB COMPRESSION" Analog Gain Reduction Meter (0..20 dB)
//    Matches uploaded photo: Black rectangular bezel, 0..4..8..12..16..20 arc,
//    white needle deflecting left-to-right, and calibration screw at bottom center
// ==============================================================================
class GBusGrMeter final : public juce::Component {
public:
    void setTelemetry(float grDbValue, float inDbValue, float outDbValue, bool isInActive) {
        const float target = isInActive ? std::clamp(grDbValue, 0.0f, 20.0f) : 0.0f;
        needleGrDb = needleGrDb * 0.68f + target * 0.32f;
        inDb = inDbValue;
        outDb = outDbValue;
        active = isInActive;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat();

        // 1. Outer Molded Black Meter Housing (Matches SSL 4000 G Bezel in photo)
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(b.translated(1.5f, 3.0f), 3.0f);

        juce::ColourGradient bezelGrad(juce::Colour(0xff26292e), b.getX(), b.getY(),
                                       juce::Colour(0xff141619), b.getX(), b.getBottom(), false);
        g.setGradientFill(bezelGrad);
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(0xff3f444c));
        g.drawRoundedRectangle(b.reduced(0.6f), 3.0f, 1.3f);

        // Recessed inner rim
        const float bottomRimH = std::max(22.0f, b.getHeight() * 0.22f);
        const auto win = b.reduced(12.0f, 10.0f).withTrimmedBottom(bottomRimH - 6.0f);

        g.setColour(juce::Colour(0xff0d0f12));
        g.fillRoundedRectangle(win.expanded(2.0f), 2.0f);

        // 2. Dark Charcoal Analog Meter Faceplate
        juce::ColourGradient faceGrad(juce::Colour(0xff2c3036), win.getCentreX(), win.getY(),
                                      juce::Colour(0xff1b1e22), win.getCentreX(), win.getBottom(), false);
        g.setGradientFill(faceGrad);
        g.fillRoundedRectangle(win, 2.0f);

        // Top inner shadow inside meter glass
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(win.getX(), win.getY(), win.getWidth(), 5.0f);

        // 3. Scale Markings: 0, 4, 8, 12, 16, 20 dB (Left to Right clockwise)
        const float pivotX = win.getCentreX();
        const float pivotY = win.getBottom() + win.getHeight() * 0.36f;
        const float arcR   = win.getHeight() * 1.02f;

        auto grToAngle = [](float gr) noexcept -> float {
            const float norm = std::clamp(gr / 20.0f, 0.0f, 1.0f);
            constexpr float minA = -0.68f; // ~ -39 deg (0 dB at left)
            constexpr float maxA =  0.68f; // ~ +39 deg (20 dB at right)
            return minA + norm * (maxA - minA);
        };

        g.setColour(juce::Colour(0xffe5e7eb));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));

        static constexpr float kMarks[6] = { 0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 20.0f };
        static const char* kMarkText[6]  = { "0",  "4",  "8",  "12",  "16",  "20" };

        for (int i = 0; i < 6; ++i) {
            const float a = grToAngle(kMarks[i]);
            const float x1 = pivotX + std::sin(a) * (arcR - 9.0f);
            const float y1 = pivotY - std::cos(a) * (arcR - 9.0f);
            const float x2 = pivotX + std::sin(a) * (arcR + 1.0f);
            const float y2 = pivotY - std::cos(a) * (arcR + 1.0f);
            g.drawLine(x1, y1, x2, y2, 1.5f);

            const float lx = pivotX + std::sin(a) * (arcR + 11.5f);
            const float ly = pivotY - std::cos(a) * (arcR + 11.5f);
            g.drawText(kMarkText[i], static_cast<int>(lx - 12.0f), static_cast<int>(ly - 7.0f),
                       24, 14, juce::Justification::centred, false);
        }

        // Center legends: "dB" and "COMPRESSION" (exact match to photo)
        g.setColour(juce::Colour(0xffd1d5db));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("dB", static_cast<int>(win.getCentreX() - 20.0f),
                   static_cast<int>(win.getBottom() - 32.0f), 40, 12, juce::Justification::centred, false);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("COMPRESSION", static_cast<int>(win.getCentreX() - 55.0f),
                   static_cast<int>(win.getBottom() - 19.0f), 110, 12, juce::Justification::centred, false);

        // 4. White Ballistic Pointer Needle
        const float needleAngle = grToAngle(needleGrDb);
        const float nx1 = pivotX + std::sin(needleAngle) * (arcR * 0.25f);
        const float ny1 = pivotY - std::cos(needleAngle) * (arcR * 0.25f);
        const float nx2 = pivotX + std::sin(needleAngle) * (arcR + 3.0f);
        const float ny2 = pivotY - std::cos(needleAngle) * (arcR + 3.0f);

        // Needle shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawLine(nx1 + 2.0f, ny1 + 2.0f, nx2 + 2.0f, ny2 + 2.0f, 2.2f);

        // White needle
        g.setColour(juce::Colour(0xfff9fafb));
        g.drawLine(nx1, ny1, nx2, ny2, 2.0f);

        // 5. Mechanical Zero-Calibration Screw on Lower Bezel (Exact match to photo)
        const float screwCx = b.getCentreX();
        const float screwCy = b.getBottom() - bottomRimH * 0.48f;
        const float screwR  = 6.5f;
        g.setColour(juce::Colour(0xff141619));
        g.fillEllipse(screwCx - screwR, screwCy - screwR, screwR * 2.0f, screwR * 2.0f);
        g.setColour(juce::Colour(0xff374151));
        g.drawEllipse(screwCx - screwR, screwCy - screwR, screwR * 2.0f, screwR * 2.0f, 1.2f);
        g.setColour(juce::Colour(0xff090a0c));
        g.drawLine(screwCx - 4.2f, screwCy + 3.2f, screwCx + 4.2f, screwCy - 3.2f, 1.8f);

        // Digital GR Readout Pill on Left/Right of Lower Bezel
        g.setColour(active ? juce::Colour(0xfffef08a) : juce::Colour(0xff6b7280));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText(active ? ("GR: -" + juce::String(needleGrDb, 1) + " dB") : "BYPASSED",
                   static_cast<int>(win.getX()), static_cast<int>(screwCy - 6.0f), 85, 12,
                   juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xff9ca3af));
        g.drawText("OUT: " + juce::String(outDb, 1) + " dB",
                   static_cast<int>(win.getRight() - 85.0f), static_cast<int>(screwCy - 6.0f), 85, 12,
                   juce::Justification::centredRight, false);
    }

private:
    float needleGrDb = 0.0f;
    float inDb = -60.0f;
    float outDb = -60.0f;
    bool active = true;
};

// ==============================================================================
// 3. Main NOVA HD G-Bus Stereo VCA Bus Compressor Panel
// ==============================================================================
class GBusCompressorPanel final : public juce::Component, private juce::Timer {
public:
    explicit GBusCompressorPanel(nova::graviton::KTBusCompressor& busCompEngine)
        : engine(&busCompEngine),
          thresholdKnob("THRESHOLD", GBusKnob::Mode::ContinuousThreshold, -15.0, 15.0, -6.0),
          makeupKnob("MAKE-UP", GBusKnob::Mode::ContinuousMakeup, -5.0, 15.0, 2.0),
          attackKnob("ATTACK -mS", GBusKnob::Mode::SteppedAttack, 0.0, 5.0, 5.0),
          releaseKnob("RELEASE -S", GBusKnob::Mode::SteppedRelease, 0.0, 4.0, 4.0),
          ratioKnob("RATIO", GBusKnob::Mode::SteppedRatio, 0.0, 2.0, 0.0),
          stereoLinkKnob("STEREO LINK", GBusKnob::Mode::ContinuousPercent, 0.0, 100.0, 100.0),
          mixKnob("PARALLEL MIX", GBusKnob::Mode::ContinuousPercent, 0.0, 100.0, 100.0),
          vcaCharKnob("VCA CHARACTER", GBusKnob::Mode::ContinuousPercent, 0.0, 100.0, 55.0)
    {
        // ── Top Studio Toolbar ──
        presetBtn.setButtonText("PRESET");
        presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible(presetBtn);

        const auto& presets = nova::graviton::KTBusCompressor::getFactoryPresets();
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
        prevPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        prevPresetBtn.onClick = [this] { stepPreset(-1); };
        addAndMakeVisible(prevPresetBtn);

        nextPresetBtn.setButtonText(">");
        nextPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        nextPresetBtn.onClick = [this] { stepPreset(1); };
        addAndMakeVisible(nextPresetBtn);

        // VCA Circuit Topology Model Buttons
        btnVca202C.setButtonText("DBX 202C (Gold)");
        btnVca4000G.setButtonText("4000G TWIN VCA");
        btnVca2180.setButtonText("THAT 2180 (Clean)");
        int mIdx = 0;
        for (auto* b : { &btnVca202C, &btnVca4000G, &btnVca2180 }) {
            b->setClickingTogglesState(true);
            b->setRadioGroupId(40001, juce::dontSendNotification);
            b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f242c));
            b->setColour(juce::TextButton::buttonOnColourId,
                         mIdx == 0 ? juce::Colour(0xffd97706)
                       : mIdx == 1 ? juce::Colour(0xff0284c7)
                                   : juce::Colour(0xff059669));
            const int capIdx = mIdx++;
            b->onClick = [this, capIdx] {
                if (engine) {
                    engine->setVcaModel(static_cast<nova::graviton::GBusVcaModel>(capIdx));
                    updateAllUiFromEngine();
                }
            };
            addAndMakeVisible(*b);
        }

        // Sidechain HPF Selector (Off / 60 / 90 / 120 / 185 Hz)
        scHpfCombo.addItem("SC HPF: OFF", 1);
        scHpfCombo.addItem("SC HPF: 60 Hz", 2);
        scHpfCombo.addItem("SC HPF: 90 Hz", 3);
        scHpfCombo.addItem("SC HPF: 120 Hz", 4);
        scHpfCombo.addItem("SC HPF: 185 Hz", 5);
        scHpfCombo.onChange = [this] {
            if (!engine) return;
            const int id = scHpfCombo.getSelectedId();
            const double hz = (id == 2) ? 60.0 : (id == 3) ? 90.0 : (id == 4) ? 120.0 : (id == 5) ? 185.0 : 0.0;
            engine->setSidechainHpfHz(hz);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(scHpfCombo);

        // Analog Coloration Toggle
        btnAnalogColor.setButtonText("ANALOG GLUE: ON");
        btnAnalogColor.setClickingTogglesState(true);
        btnAnalogColor.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        btnAnalogColor.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb45309));
        btnAnalogColor.onClick = [this] {
            if (engine) {
                engine->setAnalogColoration(btnAnalogColor.getToggleState());
                updateAllUiFromEngine();
            }
        };
        addAndMakeVisible(btnAnalogColor);

        // ── SSL 4000 G Hardware Center Module Controls ──
        addAndMakeVisible(grMeter);

        thresholdKnob.onValueChanged = [this](double v) {
            if (!engine) return;
            engine->setThresholdDialDb(v);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(thresholdKnob);

        makeupKnob.onValueChanged = [this](double v) {
            if (!engine) return;
            engine->setMakeupGainDb(v);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(makeupKnob);

        attackKnob.onValueChanged = [this](double stepVal) {
            if (!engine) return;
            const int s = std::clamp(static_cast<int>(std::round(stepVal)), 0, 5);
            engine->setAttackStep(static_cast<nova::graviton::GBusAttackStep>(s));
            updateAllUiFromEngine();
        };
        addAndMakeVisible(attackKnob);

        releaseKnob.onValueChanged = [this](double stepVal) {
            if (!engine) return;
            const int s = std::clamp(static_cast<int>(std::round(stepVal)), 0, 4);
            engine->setReleaseStep(static_cast<nova::graviton::GBusReleaseStep>(s));
            updateAllUiFromEngine();
        };
        addAndMakeVisible(releaseKnob);

        ratioKnob.onValueChanged = [this](double stepVal) {
            if (!engine) return;
            const int s = std::clamp(static_cast<int>(std::round(stepVal)), 0, 2);
            engine->setRatioStep(static_cast<nova::graviton::GBusRatio>(s));
            updateAllUiFromEngine();
        };
        addAndMakeVisible(ratioKnob);

        // Quick-Select Classic Ratio Buttons (2:1, 4:1, 10:1)
        static const char* kRatioText[3] = { "2:1", "4:1", "10:1" };
        for (int i = 0; i < 3; ++i) {
            auto& b = ratioQuickBtns[static_cast<size_t>(i)];
            b.setButtonText(kRatioText[i]);
            b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2228));
            b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
            const int capR = i;
            b.onClick = [this, capR] {
                if (!engine) return;
                engine->setRatioStep(static_cast<nova::graviton::GBusRatio>(capR));
                updateAllUiFromEngine();
            };
            addAndMakeVisible(b);
        }

        // Illuminated SSL "IN" Pushbutton
        btnCompIn.setButtonText("COMP IN");
        btnCompIn.setClickingTogglesState(true);
        btnCompIn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        btnCompIn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffeab308));
        btnCompIn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff111827));
        btnCompIn.onClick = [this] {
            if (!engine) return;
            engine->setInEnabled(btnCompIn.getToggleState());
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnCompIn);

        // ── NOVA HD Advanced Bus Controls ──
        stereoLinkKnob.onValueChanged = [this](double v) {
            if (!engine) return;
            engine->setStereoLink(v);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(stereoLinkKnob);

        mixKnob.onValueChanged = [this](double v) {
            if (!engine) return;
            engine->setMix(v);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(mixKnob);

        vcaCharKnob.onValueChanged = [this](double v) {
            if (!engine) return;
            engine->setVcaCharacter(v);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(vcaCharKnob);

        updateAllUiFromEngine();
        startTimerHz(30);
        setSize(1120, 310);
    }

    ~GBusCompressorPanel() override {
        stopTimer();
    }

    void setEngine(nova::graviton::KTBusCompressor& newEngine) {
        engine = &newEngine;
        updateAllUiFromEngine();
    }

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds();

        // 1. Top Studio Toolbar Background
        g.setColour(juce::Colour(0xff161b22));
        g.fillRect(0, 0, b.getWidth(), 30);
        g.setColour(juce::Colour(0xff30363d));
        g.drawHorizontalLine(29, 0.0f, static_cast<float>(b.getWidth()));

        // 2. SSL 4000 G Console Charcoal-Grey Textured Module Chassis
        const auto chassis = juce::Rectangle<float>(4.0f, 32.0f, static_cast<float>(b.getWidth() - 8), static_cast<float>(b.getHeight() - 36));
        juce::ColourGradient sslGrad(juce::Colour(0xff464a50), chassis.getX(), chassis.getY(),
                                     juce::Colour(0xff32363b), chassis.getX(), chassis.getBottom(), false);
        g.setGradientFill(sslGrad);
        g.fillRoundedRectangle(chassis, 4.0f);

        // Subtle console powder-coat micro-grain lines
        g.setColour(juce::Colours::black.withAlpha(0.08f));
        for (int y = static_cast<int>(chassis.getY()) + 2; y < static_cast<int>(chassis.getBottom()); y += 2) {
            g.drawHorizontalLine(y, chassis.getX() + 2.0f, chassis.getRight() - 2.0f);
        }

        // Outer metallic border
        g.setColour(juce::Colour(0xff525862));
        g.drawRoundedRectangle(chassis, 4.0f, 1.4f);

        // 3. White Vertical SSL Console Section Divider Lines (Exact match to uploaded photo)
        g.setColour(juce::Colour(0xffe5e7eb).withAlpha(0.85f));
        g.fillRect(divLine1X, chassis.getY() + 6.0f, 2.0f, chassis.getHeight() - 12.0f);
        g.fillRect(divLine2X, chassis.getY() + 6.0f, 2.0f, chassis.getHeight() - 12.0f);
        g.fillRect(divLine3X, chassis.getY() + 6.0f, 2.0f, chassis.getHeight() - 12.0f);

        // 4. Module Branding Silkscreen (Left Section)
        g.setColour(juce::Colour(0xfff3f4f6));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("SL 4000 G-SERIES", static_cast<int>(chassis.getX() + 14.0f), static_cast<int>(chassis.getY() + 10.0f),
                   static_cast<int>(divLine1X - chassis.getX() - 24.0f), 18, juce::Justification::centred, false);

        g.setColour(juce::Colour(0xffd1d5db));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText("QUAD VCA MASTER BUS COMPRESSOR", static_cast<int>(chassis.getX() + 10.0f), static_cast<int>(chassis.getY() + 28.0f),
                   static_cast<int>(divLine1X - chassis.getX() - 16.0f), 14, juce::Justification::centred, false);

        // Right Section Header ("NOVA HD G-BUS ADVANCED")
        g.setColour(juce::Colour(0xfff3f4f6));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("NOVA HD ADVANCED BUS CONTROL",
                   static_cast<int>(divLine2X + 10.0f), static_cast<int>(chassis.getY() + 8.0f),
                   static_cast<int>(divLine3X - divLine2X - 20.0f), 15, juce::Justification::centred, false);

        // Sidechain & Signal Flow Summary Card in Advanced Section
        if (!flowBadgeBounds.isEmpty()) {
            g.setColour(juce::Colour(0xff1e2228).withAlpha(0.88f));
            g.fillRoundedRectangle(flowBadgeBounds.toFloat(), 4.0f);
            g.setColour(juce::Colour(0xff4b5563));
            g.drawRoundedRectangle(flowBadgeBounds.toFloat(), 4.0f, 1.0f);

            g.setColour(juce::Colour(0xfffef08a));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            const double hpf = engine ? engine->getSidechainHpfHz() : 0.0;
            const juce::String hpfTxt = (hpf > 10.0) ? (juce::String(static_cast<int>(hpf)) + " Hz") : "OFF";
            const juce::String relTxt = (engine && engine->getAutoRelease()) ? "AUTO (Dual-RC)" : (juce::String(engine ? engine->getReleaseMs() * 0.001 : 0.3, 1) + " s");
            g.drawText("STEREO-LINKED VCA  |  SC HPF: " + hpfTxt + "  |  RELEASE: " + relTxt,
                       flowBadgeBounds.reduced(6, 2), juce::Justification::centred, false);
        }
    }

    void resized() override {
        const int w = getWidth();
        const int h = getHeight();

        // ── Top Studio Toolbar ──
        const int tbY = 4;
        int tx = 6;
        presetBtn.setBounds(tx, tbY, 54, 22); tx += 57;
        presetCombo.setBounds(tx, tbY, 215, 22); tx += 218;
        prevPresetBtn.setBounds(tx, tbY, 22, 22); tx += 24;
        nextPresetBtn.setBounds(tx, tbY, 22, 22); tx += 28;

        btnVca202C.setBounds(tx, tbY, 102, 22); tx += 105;
        btnVca4000G.setBounds(tx, tbY, 104, 22); tx += 107;
        btnVca2180.setBounds(tx, tbY, 108, 22); tx += 112;

        scHpfCombo.setBounds(tx, tbY, 120, 22); tx += 124;
        btnAnalogColor.setBounds(tx, tbY, std::min(140, w - tx - 8), 22);

        // ── Main SSL 4000 G Faceplate Sections ──
        const int faceY = 34;
        const int faceH = h - faceY - 6;

        // 3 Columns matching the SSL console module proportions:
        //   Column 1 (Left, 24%):dB COMPRESSION Meter + Ratio Switch + IN Switch
        //   Column 2 (Center, 42%): Authentic 2x2 Knob Grid (THRESHOLD, MAKE-UP / ATTACK -mS, RELEASE -S)
        //   Column 3 (Right, 32%): NOVA HD Advanced (STEREO LINK, PARALLEL MIX, VCA CHARACTER)
        divLine1X = w * 0.28f;
        divLine2X = w * 0.68f;
        divLine3X = w - 12.0f;

        // Column 1: Analog GR Meter + Ratio + IN button
        const int col1X = 14;
        const int col1W = static_cast<int>(divLine1X) - col1X - 10;
        const int meterH = std::min(126, static_cast<int>(faceH * 0.50f));
        grMeter.setBounds(col1X + 6, faceY + 46, col1W - 12, meterH);

        const int botY = faceY + 50 + meterH;
        const int botH = faceH - (52 + meterH);
        const int ratioKnobSz = std::min(104, botH);
        ratioKnob.setBounds(col1X + 4, botY, ratioKnobSz, ratioKnobSz);

        const int rBtnX = col1X + ratioKnobSz + 10;
        const int rBtnW = std::max(54, col1W - ratioKnobSz - 16);
        for (int i = 0; i < 3; ++i) {
            ratioQuickBtns[static_cast<size_t>(i)].setBounds(rBtnX, botY + 2 + i * 21, rBtnW, 19);
        }
        btnCompIn.setBounds(rBtnX, botY + 68, rBtnW, 26);

        // Column 2: Authentic SSL 4000 G 2x2 Knob Layout (THRESHOLD & MAKE-UP on top, ATTACK & RELEASE on bottom)
        const int col2X = static_cast<int>(divLine1X) + 8;
        const int col2W = static_cast<int>(divLine2X - divLine1X) - 16;
        const int knobCellW = col2W / 2;
        const int knobCellH = (faceH - 8) / 2;
        const int knobSz = std::min(knobCellW - 12, knobCellH - 2);

        thresholdKnob.setBounds(col2X + (knobCellW - knobSz) / 2, faceY + 4, knobSz, knobSz);
        makeupKnob.setBounds(col2X + knobCellW + (knobCellW - knobSz) / 2, faceY + 4, knobSz, knobSz);

        attackKnob.setBounds(col2X + (knobCellW - knobSz) / 2, faceY + 4 + knobCellH, knobSz, knobSz);
        releaseKnob.setBounds(col2X + knobCellW + (knobCellW - knobSz) / 2, faceY + 4 + knobCellH, knobSz, knobSz);

        // Column 3: NOVA HD G-Bus Advanced Section (Stereo Link, Parallel Mix, VCA Character)
        const int col3X = static_cast<int>(divLine2X) + 8;
        const int col3W = static_cast<int>(divLine3X - divLine2X) - 16;
        const int advCellW = col3W / 3;
        const int advKnobSz = std::min(advCellW - 4, faceH - 68);
        const int advY = faceY + 28;

        stereoLinkKnob.setBounds(col3X + (advCellW - advKnobSz) / 2, advY, advKnobSz, advKnobSz);
        mixKnob.setBounds(col3X + advCellW + (advCellW - advKnobSz) / 2, advY, advKnobSz, advKnobSz);
        vcaCharKnob.setBounds(col3X + advCellW * 2 + (advCellW - advKnobSz) / 2, advY, advKnobSz, advKnobSz);

        flowBadgeBounds = juce::Rectangle<int>(col3X + 6, faceY + faceH - 32, col3W - 12, 24);
    }

private:
    void stepPreset(int delta) {
        if (!engine) return;
        const auto& all = nova::graviton::KTBusCompressor::getFactoryPresets();
        if (all.empty()) return;
        const int n = static_cast<int>(all.size());
        const int nextIdx = (engine->getCurrentPresetIndex() + delta + n) % n;
        engine->loadPresetByIndex(nextIdx);
        updateAllUiFromEngine();
    }

    void showPresetMenu() {
        juce::PopupMenu m;
        const auto& all = nova::graviton::KTBusCompressor::getFactoryPresets();
        const int cur = engine ? engine->getCurrentPresetIndex() : 0;
        for (size_t i = 0; i < all.size(); ++i) {
            m.addItem(static_cast<int>(i + 1),
                      juce::String(all[i].category) + " — " + all[i].name,
                      true, static_cast<int>(i) == cur);
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
        if (!engine) return;

        presetCombo.setSelectedId(engine->getCurrentPresetIndex() + 1, juce::dontSendNotification);

        // Threshold (-15 to +15 dB)
        const double thr = engine->getThresholdDialDb();
        thresholdKnob.setValue(thr, juce::dontSendNotification);
        thresholdKnob.setSubReadout((thr >= 0.0 ? "+" : "") + juce::String(thr, 1) + " dB");

        // Makeup (-5 to +15 dB)
        const double mk = engine->getMakeupGainDb();
        makeupKnob.setValue(mk, juce::dontSendNotification);
        makeupKnob.setSubReadout((mk >= 0.0 ? "+" : "") + juce::String(mk, 1) + " dB");

        // Attack (0.1, 0.3, 1, 3, 10, 30 ms)
        attackKnob.setValue(static_cast<double>(engine->getAttackStep()), juce::dontSendNotification);
        const double attMs = engine->getAttackMs();
        attackKnob.setSubReadout(juce::String(attMs, attMs < 1.0 ? 1 : 0) + " ms");

        // Release (0.1, 0.3, 0.6, 1.2 s, Auto)
        releaseKnob.setValue(static_cast<double>(engine->getReleaseStep()), juce::dontSendNotification);
        releaseKnob.setSubReadout(engine->getAutoRelease()
                                      ? "AUTO (Dual-RC)"
                                      : (juce::String(engine->getReleaseMs() * 0.001, 1) + " s"));

        // Ratio (2:1, 4:1, 10:1)
        const auto rStep = engine->getRatioStep();
        ratioKnob.setValue(static_cast<double>(rStep), juce::dontSendNotification);
        const char* rStr = (rStep == nova::graviton::GBusRatio::Ratio_2_1) ? "2 : 1"
                         : (rStep == nova::graviton::GBusRatio::Ratio_4_1) ? "4 : 1"
                                                                           : "10 : 1";
        ratioKnob.setSubReadout(rStr);
        for (int i = 0; i < 3; ++i) {
            ratioQuickBtns[static_cast<size_t>(i)].setToggleState(static_cast<int>(rStep) == i, juce::dontSendNotification);
        }

        // IN Button
        const bool inOn = engine->getInEnabled();
        btnCompIn.setToggleState(inOn, juce::dontSendNotification);
        btnCompIn.setButtonText(inOn ? "COMP IN" : "BYPASS");

        // Advanced Knobs
        stereoLinkKnob.setValue(engine->getStereoLink(), juce::dontSendNotification);
        stereoLinkKnob.setSubReadout(juce::String(static_cast<int>(std::round(engine->getStereoLink()))) + "% LINK");

        mixKnob.setValue(engine->getMix(), juce::dontSendNotification);
        mixKnob.setSubReadout(juce::String(static_cast<int>(std::round(engine->getMix()))) + "% WET");

        vcaCharKnob.setValue(engine->getVcaCharacter(), juce::dontSendNotification);
        vcaCharKnob.setSubReadout(juce::String(static_cast<int>(std::round(engine->getVcaCharacter()))) + "% DRIVE");

        // VCA Model Buttons
        const auto vm = engine->getVcaModel();
        btnVca202C.setToggleState(vm == nova::graviton::GBusVcaModel::DBX202C_GoldCan, juce::dontSendNotification);
        btnVca4000G.setToggleState(vm == nova::graviton::GBusVcaModel::Quad4000G_Twin, juce::dontSendNotification);
        btnVca2180.setToggleState(vm == nova::graviton::GBusVcaModel::THAT2180_Clean,  juce::dontSendNotification);

        // SC HPF & Analog Coloration
        const double hpf = engine->getSidechainHpfHz();
        const int hpfId = (hpf >= 160.0) ? 5 : (hpf >= 110.0) ? 4 : (hpf >= 80.0) ? 3 : (hpf >= 40.0) ? 2 : 1;
        scHpfCombo.setSelectedId(hpfId, juce::dontSendNotification);

        btnAnalogColor.setToggleState(engine->getAnalogColoration(), juce::dontSendNotification);
        btnAnalogColor.setButtonText(engine->getAnalogColoration() ? "ANALOG GLUE: ON" : "ANALOG GLUE: OFF");

        repaint();
    }

    void timerCallback() override {
        if (!engine) return;
        grMeter.setTelemetry(engine->getMeterGrDb(),
                             engine->getMeterInDb(),
                             engine->getMeterOutDb(),
                             engine->getInEnabled());
    }

    nova::graviton::KTBusCompressor* engine = nullptr;

    // Top Bar Controls
    juce::TextButton presetBtn, prevPresetBtn, nextPresetBtn;
    juce::ComboBox presetCombo;
    juce::TextButton btnVca202C, btnVca4000G, btnVca2180;
    juce::ComboBox scHpfCombo;
    juce::TextButton btnAnalogColor;

    // SSL 4000 G Module Controls
    GBusGrMeter grMeter;
    GBusKnob thresholdKnob;
    GBusKnob makeupKnob;
    GBusKnob attackKnob;
    GBusKnob releaseKnob;
    GBusKnob ratioKnob;
    std::array<juce::TextButton, 3> ratioQuickBtns;
    juce::TextButton btnCompIn;

    // NOVA HD Advanced Controls
    GBusKnob stereoLinkKnob;
    GBusKnob mixKnob;
    GBusKnob vcaCharKnob;

    float divLine1X = 280.0f;
    float divLine2X = 740.0f;
    float divLine3X = 1100.0f;
    juce::Rectangle<int> flowBadgeBounds;
};

} // namespace nova
