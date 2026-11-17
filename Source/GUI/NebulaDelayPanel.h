#pragma once

#include "Effects/GravitonNebulaDelay.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <algorithm>

namespace nova {

// ==============================================================================
// 1. Custom Glowing Rotary Knob (Nebula Hardware Dial)
// ==============================================================================
class NebulaKnob final : public juce::Component {
public:
    NebulaKnob(const juce::String& labelText,
               double minV, double maxV, double defaultV,
               const juce::String& unitSuffix,
               juce::Colour glowColour,
               int decimalPlaces = 1)
        : title(labelText),
          minValue(minV),
          maxValue(maxV),
          defaultValue(defaultV),
          currentValue(defaultV),
          unit(unitSuffix),
          accentColor(glowColour),
          decimals(decimalPlaces) {
        setRepaintsOnMouseActivity(true);
    }

    void setValue(double val, juce::NotificationType notify = juce::dontSendNotification) {
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

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const bool isCompact = (bounds.getHeight() < 58.0f);

        // 1. Label on top
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(isCompact ? 9.0f : 10.5f, juce::Font::bold)));
        const float labelH = isCompact ? 13.0f : 15.0f;
        g.drawText(title, 0, 0, static_cast<int>(bounds.getWidth()), static_cast<int>(labelH),
                   juce::Justification::centred, true);

        // 2. Rotary Dial Area
        const float valBoxH = isCompact ? 13.0f : 15.0f;
        const float dialY = labelH + 1.0f;
        const float availH = bounds.getHeight() - dialY - valBoxH - 2.0f;
        const float dialSize = std::clamp(std::min(bounds.getWidth() - 6.0f, availH), 22.0f, 60.0f);
        const float dialX = (bounds.getWidth() - dialSize) * 0.5f;
        const auto dialBounds = juce::Rectangle<float>(dialX, dialY, dialSize, dialSize);

        // Background track ring
        g.setColour(juce::Colour(0xff18162e));
        g.drawEllipse(dialBounds, isCompact ? 2.5f : 3.5f);

        // Normalized value [0.0, 1.0]
        const float norm = static_cast<float>((currentValue - minValue) / std::max(1e-4, maxValue - minValue));
        constexpr float startAngle = -2.35619449f; // -135 deg in rad
        constexpr float endAngle   =  2.35619449f; // +135 deg in rad
        const float curAngle = startAngle + norm * (endAngle - startAngle);

        // Glowing Active Arc
        if (norm > 0.001f) {
            juce::Path arc;
            arc.addCentredArc(dialBounds.getCentreX(), dialBounds.getCentreY(),
                              dialSize * 0.5f, dialSize * 0.5f,
                              0.0f, startAngle, curAngle, true);
            g.setColour(accentColor.withAlpha(0.28f));
            g.strokePath(arc, juce::PathStrokeType(isCompact ? 4.5f : 6.0f));
            g.setColour(accentColor);
            g.strokePath(arc, juce::PathStrokeType(isCompact ? 2.2f : 3.2f));
        }

        // Inner knob body
        const auto innerBounds = dialBounds.reduced(dialSize * 0.20f);
        juce::ColourGradient knobGrad(juce::Colour(0xff1d1b38), innerBounds.getCentreX(), innerBounds.getY(),
                                      juce::Colour(0xff0d0c1e), innerBounds.getCentreX(), innerBounds.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(innerBounds);
        g.setColour(juce::Colour(0xff2d2b52));
        g.drawEllipse(innerBounds, 1.0f);

        // Pointer indicator
        const float radius = innerBounds.getWidth() * 0.5f;
        const float cx = innerBounds.getCentreX();
        const float cy = innerBounds.getCentreY();
        const float px = cx + std::sin(curAngle) * (radius - 1.5f);
        const float py = cy - std::cos(curAngle) * (radius - 1.5f);

        g.setColour(accentColor);
        g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);
        g.drawLine(cx, cy, px, py, isCompact ? 1.2f : 1.5f);

        // 3. Numeric Readout Box below knob
        const float boxY = bounds.getBottom() - valBoxH;
        const auto valBox = juce::Rectangle<float>(bounds.getX() + 2.0f, boxY, bounds.getWidth() - 4.0f, valBoxH);

        g.setColour(juce::Colour(0xff0e0d1f));
        g.fillRoundedRectangle(valBox, 2.5f);
        g.setColour(juce::Colour(0xff242145));
        g.drawRoundedRectangle(valBox, 2.5f, 1.0f);

        juce::String valStr;
        if (decimals == 0) {
            valStr = juce::String(static_cast<int>(std::round(currentValue)));
        } else {
            valStr = juce::String(currentValue, decimals);
        }
        if (unit.isNotEmpty()) {
            valStr += " " + unit;
        }

        g.setColour(accentColor.brighter(0.4f));
        g.setFont(juce::Font(juce::FontOptions(isCompact ? 8.5f : 9.5f, juce::Font::bold)));
        g.drawText(valStr, valBox, juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragStartValue = currentValue;
        dragStartPos = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float deltaY = dragStartPos.y - e.position.y;
        const double range = maxValue - minValue;
        const double deltaVal = (static_cast<double>(deltaY) / 140.0) * range;
        setValue(dragStartValue + deltaVal, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        setValue(defaultValue, juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        const double range = maxValue - minValue;
        const double step = range * 0.02 * wheel.deltaY;
        setValue(currentValue + step, juce::sendNotificationSync);
    }

private:
    juce::String title;
    double minValue = 0.0;
    double maxValue = 1.0;
    double defaultValue = 0.5;
    double currentValue = 0.5;
    double dragStartValue = 0.5;
    juce::Point<float> dragStartPos;
    juce::String unit;
    juce::Colour accentColor;
    int decimals = 1;
};

// ==============================================================================
// 2. Large Circular Delay Time Gauge
// ==============================================================================
class NebulaBigGauge final : public juce::Component {
public:
    NebulaBigGauge() {
        setRepaintsOnMouseActivity(true);
    }

    void setTimeMs(double ms, juce::NotificationType notify = juce::dontSendNotification) {
        const double clamped = std::clamp(ms, 1.0, 2500.0);
        if (std::abs(clamped - timeMs) > 0.05) {
            timeMs = clamped;
            repaint();
            if (notify != juce::dontSendNotification && onTimeChanged) {
                onTimeChanged(timeMs);
            }
        }
    }

    double getTimeMs() const noexcept { return timeMs; }

    std::function<void(double)> onTimeChanged;

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const float size = std::min(bounds.getWidth(), bounds.getHeight());
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float radius = size * 0.46f;

        // 1. Outer Tick Marks (Cyan & Magenta ticks around ring)
        constexpr int numTicks = 24;
        for (int i = 0; i < numTicks; ++i) {
            const float angle = -2.35619449f + (static_cast<float>(i) / (numTicks - 1)) * 4.71238898f;
            const float r1 = radius + 2.0f;
            const float r2 = radius + 5.5f;
            const float x1 = cx + std::sin(angle) * r1;
            const float y1 = cy - std::cos(angle) * r1;
            const float x2 = cx + std::sin(angle) * r2;
            const float y2 = cy - std::cos(angle) * r2;

            const juce::Colour tickCol = (i % 3 == 0) ? juce::Colour(0xff00e5ff) : juce::Colour(0xffd946ef);
            g.setColour(tickCol.withAlpha((i % 2 == 0) ? 0.85f : 0.35f));
            g.drawLine(x1, y1, x2, y2, (i % 3 == 0) ? 1.8f : 1.0f);
        }

        // 2. Main Track Circle
        const auto gaugeBounds = juce::Rectangle<float>(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff18162e));
        g.drawEllipse(gaugeBounds, 4.0f);

        // 3. Active Glowing Arc (Cyan)
        const float norm = static_cast<float>((timeMs - 1.0) / 2499.0);
        constexpr float startAngle = -2.35619449f;
        constexpr float endAngle   =  2.35619449f;
        const float curAngle = startAngle + norm * (endAngle - startAngle);

        if (norm > 0.005f) {
            juce::Path arc;
            arc.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, curAngle, true);
            g.setColour(juce::Colour(0x5000e5ff));
            g.strokePath(arc, juce::PathStrokeType(7.0f));
            g.setColour(juce::Colour(0xff00e5ff));
            g.strokePath(arc, juce::PathStrokeType(4.0f));
        }

        // 4. Center Disc
        const auto inner = gaugeBounds.reduced(radius * 0.26f);
        juce::ColourGradient grad(juce::Colour(0xff181530), cx, inner.getY(),
                                  juce::Colour(0xff0c0b1a), cx, inner.getBottom(), false);
        g.setGradientFill(grad);
        g.fillEllipse(inner);
        g.setColour(juce::Colour(0xff292550));
        g.drawEllipse(inner, 1.2f);

        // 5. Center Time Readout
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(size > 85.0f ? 15.0f : 13.0f, juce::Font::bold)));
        g.drawText(juce::String(static_cast<int>(std::round(timeMs))) + " ms",
                   inner, juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragStartMs = timeMs;
        dragStartPos = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float deltaY = dragStartPos.y - e.position.y;
        setTimeMs(dragStartMs + deltaY * 4.0, juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        setTimeMs(timeMs + wheel.deltaY * 50.0, juce::sendNotificationSync);
    }

private:
    double timeMs = 131.0;
    double dragStartMs = 131.0;
    juce::Point<float> dragStartPos;
};

// ==============================================================================
// 3. Stereo Peak Meter with dB readout, Peak Hold & Clip Hold
// ==============================================================================
class NebulaStereoPeakMeter final : public juce::Component {
public:
    NebulaStereoPeakMeter(const juce::String& meterTitle) : title(meterTitle) {}

    void updateLevels(float peakL, float peakR, float peakHold, bool clip) {
        levelL = std::clamp(peakL, 0.0f, 2.0f);
        levelR = std::clamp(peakR, 0.0f, 2.0f);
        peakHoldVal = std::clamp(peakHold, 0.0f, 2.0f);
        clipOccurred = clip;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Background container
        g.setColour(juce::Colour(0xff100e21));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff221e42));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Title header
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(title, bounds.removeFromTop(15.0f), juce::Justification::centred, false);

        // Clip / Peak dB badge box
        const float maxLvl = std::max(levelL, levelR);
        const float maxDb = (maxLvl > 1e-4f) ? 20.0f * std::log10(maxLvl) : -60.0f;
        auto clipBox = bounds.removeFromTop(15.0f).reduced(3.0f, 1.0f);

        g.setColour(clipOccurred ? juce::Colour(0xffef4444) : juce::Colour(0xff171530));
        g.fillRoundedRectangle(clipBox, 2.5f);
        g.setColour(clipOccurred ? juce::Colours::white : (maxDb > -3.0f ? juce::Colour(0xfff59e0b) : juce::Colour(0xff00e5ff)));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(clipOccurred ? "CLIP" : (juce::String(maxDb, 1) + " dB"), clipBox, juce::Justification::centred, false);

        // Dual Bars (L and R)
        const float barAreaW = bounds.getWidth() - 6.0f;
        const float barW = (barAreaW - 2.0f) * 0.5f;
        const auto barL = juce::Rectangle<float>(bounds.getX() + 3.0f, bounds.getY() + 3.0f, barW, bounds.getHeight() - 6.0f);
        const auto barR = juce::Rectangle<float>(barL.getRight() + 2.0f, barL.getY(), barW, barL.getHeight());

        auto renderBar = [&](juce::Rectangle<float> r, float peakVal) {
            g.setColour(juce::Colour(0xff080712));
            g.fillRect(r);

            const float db = (peakVal > 1e-4f) ? 20.0f * std::log10(peakVal) : -60.0f;
            const float norm = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
            const float fillH = r.getHeight() * norm;
            const auto filled = r.removeFromBottom(fillH);

            if (fillH > 1.0f) {
                juce::ColourGradient grad(juce::Colour(0xff00e5ff), filled.getCentreX(), filled.getBottom(),
                                          juce::Colour(0xff10b981), filled.getCentreX(), filled.getY() + filled.getHeight() * 0.4f, false);
                grad.addColour(0.85, juce::Colour(0xfff59e0b));
                grad.addColour(0.98, juce::Colour(0xffef4444));
                g.setGradientFill(grad);
                g.fillRect(filled);
            }

            // Peak Hold line
            const float phDb = (peakHoldVal > 1e-4f) ? 20.0f * std::log10(peakHoldVal) : -60.0f;
            const float phNorm = std::clamp((phDb + 60.0f) / 60.0f, 0.0f, 1.0f);
            const float phY = r.getBottom() - (r.getHeight() * phNorm);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawHorizontalLine(static_cast<int>(phY), r.getX(), r.getRight());
        };

        renderBar(barL, levelL);
        renderBar(barR, levelR);
    }

private:
    juce::String title;
    float levelL = 0.0f;
    float levelR = 0.0f;
    float peakHoldVal = 0.0f;
    bool clipOccurred = false;
};

// ==============================================================================
// 4. Stereo Correlation & L/R Balance Monitor Widget
// ==============================================================================
class NebulaStereoMonitor final : public juce::Component {
public:
    NebulaStereoMonitor() {}

    void updateTelemetry(float correlation, float balance) {
        corrVal = std::clamp(correlation, -1.0f, 1.0f);
        balVal = std::clamp(balance, -1.0f, 1.0f);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // ── 1. L/R Balance Section ──
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("L/R BALANCE", bounds.getX(), bounds.getY() + 2.0f, bounds.getWidth(), 14.0f, juce::Justification::centred, false);

        const auto balTrack = juce::Rectangle<float>(bounds.getX() + 6.0f, bounds.getY() + 18.0f, bounds.getWidth() - 12.0f, 9.0f);
        g.setColour(juce::Colour(0xff121026));
        g.fillRoundedRectangle(balTrack, 2.0f);
        g.setColour(juce::Colour(0xff252148));
        g.drawRoundedRectangle(balTrack, 2.0f, 1.0f);

        // Center line
        const float midX = balTrack.getCentreX();
        g.setColour(juce::Colour(0xff475569));
        g.drawVerticalLine(static_cast<int>(midX), balTrack.getY(), balTrack.getBottom());

        // Fill balance deviation
        const float halfW = balTrack.getWidth() * 0.5f;
        const float barFillW = halfW * std::abs(balVal);
        juce::Rectangle<float> balFill;
        if (balVal < 0.0f) {
            balFill = juce::Rectangle<float>(midX - barFillW, balTrack.getY() + 1.0f, barFillW, balTrack.getHeight() - 2.0f);
            g.setColour(juce::Colour(0xff00e5ff));
        } else {
            balFill = juce::Rectangle<float>(midX, balTrack.getY() + 1.0f, barFillW, balTrack.getHeight() - 2.0f);
            g.setColour(juce::Colour(0xffd946ef));
        }
        g.fillRoundedRectangle(balFill, 1.5f);

        // Readout below balance
        juce::String balStr;
        if (std::abs(balVal) < 0.02f) balStr = "CENTER";
        else if (balVal < 0.0f) balStr = "L " + juce::String(static_cast<int>(std::round(-balVal * 100.0f))) + "%";
        else balStr = "R " + juce::String(static_cast<int>(std::round(balVal * 100.0f))) + "%";
        g.setColour(juce::Colour(0xffcbd5e1));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText(balStr, bounds.getX(), bounds.getY() + 29.0f, bounds.getWidth(), 12.0f, juce::Justification::centred, false);

        // ── 2. Stereo Correlation Section ──
        const float corrY = bounds.getY() + 44.0f;
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("CORRELATION", bounds.getX(), corrY, bounds.getWidth(), 14.0f, juce::Justification::centred, false);

        const auto corrTrack = juce::Rectangle<float>(bounds.getX() + 6.0f, corrY + 16.0f, bounds.getWidth() - 12.0f, 9.0f);
        g.setColour(juce::Colour(0xff121026));
        g.fillRoundedRectangle(corrTrack, 2.0f);
        g.setColour(juce::Colour(0xff252148));
        g.drawRoundedRectangle(corrTrack, 2.0f, 1.0f);

        // Correlation zero tick
        const float corrZeroX = corrTrack.getX() + corrTrack.getWidth() * 0.5f;
        g.setColour(juce::Colour(0xff475569));
        g.drawVerticalLine(static_cast<int>(corrZeroX), corrTrack.getY(), corrTrack.getBottom());

        // Normalized pointer (-1.0 -> 0.0, 0.0 -> 0.5, +1.0 -> 1.0)
        const float normCorr = (corrVal + 1.0f) * 0.5f;
        const float curX = corrTrack.getX() + corrTrack.getWidth() * normCorr;

        // Pointer indicator
        juce::Colour corrCol = (corrVal < -0.1f) ? juce::Colour(0xffef4444) : (corrVal < 0.4f ? juce::Colour(0xff00e5ff) : juce::Colour(0xff10b981));
        g.setColour(corrCol);
        g.fillRect(curX - 2.0f, corrTrack.getY() - 1.0f, 4.0f, corrTrack.getHeight() + 2.0f);

        // Scale ticks: -1, 0, +1
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::plain)));
        g.setColour(juce::Colour(0xffef4444));
        g.drawText("-1", static_cast<int>(corrTrack.getX()), static_cast<int>(corrTrack.getBottom() + 1.0f), 16, 11, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("0", static_cast<int>(corrZeroX - 8.0f), static_cast<int>(corrTrack.getBottom() + 1.0f), 16, 11, juce::Justification::centred);
        g.setColour(juce::Colour(0xff10b981));
        g.drawText("+1", static_cast<int>(corrTrack.getRight() - 16.0f), static_cast<int>(corrTrack.getBottom() + 1.0f), 16, 11, juce::Justification::centredRight);

        // Numeric Readout
        juce::String cStr = (corrVal >= 0.0f ? "+" : "") + juce::String(corrVal, 2);
        g.setColour(corrCol);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(cStr, bounds.getX(), corrTrack.getBottom() + 12.0f, bounds.getWidth(), 13.0f, juce::Justification::centred, false);
    }

private:
    float corrVal = 1.0f;
    float balVal = 0.0f;
};

// ==============================================================================
// 5. Main Nebula Stereo Delay Plugin Component
// ==============================================================================
class NebulaDelayPanel final : public juce::Component, private juce::Timer {
public:
    explicit NebulaDelayPanel(nova::graviton::NebulaStereoDelay& delayEngine)
        : engine(delayEngine),
          inMeter("INPUT"),
          outMeter("OUTPUT") {
        setOpaque(true);

        // ── 1. Top Header & Toolbar ───────────────────────────────────────
        presetBtn.setButtonText("PRESET");
        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible(presetBtn);

        presetCombo.addItemList({
            "1. Vocal Slap",
            "2. Vocal Stereo",
            "3. Vocal Wide",
            "4. Ping Pong",
            "5. Guitar Space",
            "6. Piano Echo",
            "7. Synth Movement",
            "8. Ambient Wash",
            "9. Chorus Width",
            "10. Flanger Motion"
        }, 1);
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        presetCombo.onChange = [this] {
            const int id = presetCombo.getSelectedId();
            if (id >= 1 && id <= 10) {
                applyFactoryPreset(id - 1);
            }
        };
        addAndMakeVisible(presetCombo);

        defaultBtn.setButtonText("DEFAULT");
        defaultBtn.onClick = [this] {
            engine.reset();
            updateAllUiFromEngine();
            presetCombo.setText("Default", juce::dontSendNotification);
        };
        addAndMakeVisible(defaultBtn);

        saveBtn.setButtonText("SAVE");
        saveBtn.onClick = [this] {
            // Flash save indicator
            saveBtn.setButtonText("SAVED!");
            juce::Timer::callAfterDelay(1200, [this] { saveBtn.setButtonText("SAVE"); });
        };
        addAndMakeVisible(saveBtn);

        infoBtn.setButtonText("INFO");
        infoBtn.onClick = [this] { showInfoDialog(); };
        addAndMakeVisible(infoBtn);

        syncBtn.setButtonText("Sync On");
        syncBtn.setClickingTogglesState(true);
        syncBtn.setToggleState(engine.getSyncEnabled(), juce::dontSendNotification);
        syncBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00e5ff));
        syncBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090814));
        syncBtn.onClick = [this] {
            engine.setSyncEnabled(syncBtn.getToggleState());
            syncBtn.setButtonText(syncBtn.getToggleState() ? "Sync On" : "Sync Off");
        };
        addAndMakeVisible(syncBtn);

        linkBtn.setButtonText("Unlinked");
        linkBtn.setClickingTogglesState(true);
        linkBtn.setToggleState(engine.getLinked(), juce::dontSendNotification);
        linkBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd946ef));
        linkBtn.onClick = [this] {
            engine.setLinked(linkBtn.getToggleState());
            linkBtn.setButtonText(linkBtn.getToggleState() ? "Linked" : "Unlinked");
        };
        addAndMakeVisible(linkBtn);

        analogModeCombo.addItem("Off", 1);
        analogModeCombo.addItem("Clean Digital", 2);
        analogModeCombo.addItem("Warm Tape", 3);
        analogModeCombo.addItem("Vintage Tube", 4);
        analogModeCombo.addItem("Lo-Fi Cassette", 5);
        analogModeCombo.setSelectedId(static_cast<int>(engine.getAnalogMode()) + 1, juce::dontSendNotification);
        analogModeCombo.onChange = [this] {
            engine.setAnalogMode(static_cast<nova::graviton::DelayAnalogMode>(analogModeCombo.getSelectedId() - 1));
        };
        addAndMakeVisible(analogModeCombo);

        fxOnBtn.setButtonText("FX On");
        fxOnBtn.setClickingTogglesState(true);
        fxOnBtn.setToggleState(engine.getFxEnabled(), juce::dontSendNotification);
        fxOnBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00e5ff));
        fxOnBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090814));
        fxOnBtn.onClick = [this] {
            engine.setFxEnabled(fxOnBtn.getToggleState());
            fxOnBtn.setButtonText(fxOnBtn.getToggleState() ? "FX On" : "FX Off");
        };
        addAndMakeVisible(fxOnBtn);

        resetClipBtn.setButtonText("RESET CLIP");
        resetClipBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a1b32));
        resetClipBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff87171));
        resetClipBtn.onClick = [this] {
            engine.resetClip();
        };
        addAndMakeVisible(resetClipBtn);

        // ── 2. Input Monitoring & Gain ─────────────────────────────────────
        inGain = std::make_unique<NebulaKnob>("IN GAIN", -24.0, 24.0, engine.getInputGainDb(), "dB", juce::Colour(0xff00e5ff), 1);
        inGain->onValueChanged = [this](double v) { engine.setInputGainDb(v); };
        addAndMakeVisible(*inGain);
        addAndMakeVisible(inMeter);

        // ── 3. Left Delay Engine ──────────────────────────────────────────
        inputSourceComboL.addItem("Left", 1);
        inputSourceComboL.addItem("Right", 2);
        inputSourceComboL.addItem("Sum (L+R)", 3);
        inputSourceComboL.addItem("Mute", 4);
        inputSourceComboL.setSelectedId(static_cast<int>(engine.getInputSourceL()) + 1, juce::dontSendNotification);
        inputSourceComboL.onChange = [this] {
            engine.setInputSourceL(static_cast<nova::graviton::DelayInputSource>(inputSourceComboL.getSelectedId() - 1));
        };
        addAndMakeVisible(inputSourceComboL);

        gaugeTimeL.setTimeMs(engine.getDelayTimeMsL());
        gaugeTimeL.onTimeChanged = [this](double ms) {
            engine.setDelayTimeMsL(ms);
            if (engine.getLinked()) {
                gaugeTimeR.setTimeMs(ms, juce::dontSendNotification);
                engine.setDelayTimeMsR(ms);
            }
        };
        addAndMakeVisible(gaugeTimeL);

        for (int i = 0; i < static_cast<int>(nova::graviton::NoteDivision::NumDivisions); ++i) {
            noteComboL.addItem(nova::graviton::getNoteDivisionName(static_cast<nova::graviton::NoteDivision>(i)), i + 1);
        }
        noteComboL.setSelectedId(static_cast<int>(engine.getNoteDivisionL()) + 1, juce::dontSendNotification);
        noteComboL.onChange = [this] {
            engine.setNoteDivisionL(static_cast<nova::graviton::NoteDivision>(noteComboL.getSelectedId() - 1));
            if (engine.getLinked()) {
                noteComboR.setSelectedId(noteComboL.getSelectedId(), juce::dontSendNotification);
                engine.setNoteDivisionR(engine.getNoteDivisionL());
            }
        };
        addAndMakeVisible(noteComboL);

        divTimeBtnL.setButtonText(":2");
        divTimeBtnL.onClick = [this] {
            gaugeTimeL.setTimeMs(gaugeTimeL.getTimeMs() * 0.5, juce::sendNotificationSync);
        };
        addAndMakeVisible(divTimeBtnL);

        mulTimeBtnL.setButtonText("x2");
        mulTimeBtnL.onClick = [this] {
            gaugeTimeL.setTimeMs(gaugeTimeL.getTimeMs() * 2.0, juce::sendNotificationSync);
        };
        addAndMakeVisible(mulTimeBtnL);

        deviationL = std::make_unique<NebulaKnob>("Deviation", -100.0, 100.0, engine.getDeviationCentsL(), "ct", juce::Colour(0xff00e5ff), 1);
        deviationL->onValueChanged = [this](double v) { engine.setDeviationCentsL(v); };
        addAndMakeVisible(*deviationL);

        hpfL = std::make_unique<NebulaKnob>("HPF", 10.0, 1000.0, engine.getHpfHzL(), "Hz", juce::Colour(0xff00e5ff), 0);
        hpfL->onValueChanged = [this](double v) { engine.setHpfHzL(v); };
        addAndMakeVisible(*hpfL);

        hpfsL = std::make_unique<NebulaKnob>("HPFS", 6.0, 48.0, engine.getHpfsDbL(), "dB/o", juce::Colour(0xffa855f7), 0);
        hpfsL->onValueChanged = [this](double v) { engine.setHpfsDbL(v); };
        addAndMakeVisible(*hpfsL);

        lpfL = std::make_unique<NebulaKnob>("LPF", 500.0, 20000.0, engine.getLpfHzL(), "Hz", juce::Colour(0xfff97316), 0);
        lpfL->onValueChanged = [this](double v) { engine.setLpfHzL(v); };
        addAndMakeVisible(*lpfL);

        lpfsL = std::make_unique<NebulaKnob>("LPFS", 6.0, 48.0, engine.getLpfsDbL(), "dB/o", juce::Colour(0xffec4899), 0);
        lpfsL->onValueChanged = [this](double v) { engine.setLpfsDbL(v); };
        addAndMakeVisible(*lpfsL);

        feedbackL = std::make_unique<NebulaKnob>("Feedback", 0.0, 100.0, engine.getFeedbackL(), "%", juce::Colour(0xff00e5ff), 1);
        feedbackL->onValueChanged = [this](double v) { engine.setFeedbackL(v); };
        addAndMakeVisible(*feedbackL);

        crossfeedL = std::make_unique<NebulaKnob>("Crossfeed L-R", 0.0, 100.0, engine.getCrossfeedLR(), "%", juce::Colour(0xffd946ef), 1);
        crossfeedL->onValueChanged = [this](double v) { engine.setCrossfeedLR(v); };
        addAndMakeVisible(*crossfeedL);

        phaseFbBtnL.setButtonText("NORM");
        phaseFbBtnL.setClickingTogglesState(true);
        phaseFbBtnL.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b));
        phaseFbBtnL.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090814));
        phaseFbBtnL.onClick = [this] {
            engine.setPhaseInvertFeedbackL(phaseFbBtnL.getToggleState());
            phaseFbBtnL.setButtonText(phaseFbBtnL.getToggleState() ? "INV" : "NORM");
        };
        addAndMakeVisible(phaseFbBtnL);

        phaseCrossBtnL.setButtonText("NORM");
        phaseCrossBtnL.setClickingTogglesState(true);
        phaseCrossBtnL.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b));
        phaseCrossBtnL.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090814));
        phaseCrossBtnL.onClick = [this] {
            engine.setPhaseInvertCrossfeedLR(phaseCrossBtnL.getToggleState());
            phaseCrossBtnL.setButtonText(phaseCrossBtnL.getToggleState() ? "INV" : "NORM");
        };
        addAndMakeVisible(phaseCrossBtnL);

        // ── 4. Right Delay Engine ─────────────────────────────────────────
        inputSourceComboR.addItem("Left", 1);
        inputSourceComboR.addItem("Right", 2);
        inputSourceComboR.addItem("Sum (L+R)", 3);
        inputSourceComboR.addItem("Mute", 4);
        inputSourceComboR.setSelectedId(static_cast<int>(engine.getInputSourceR()) + 1, juce::dontSendNotification);
        inputSourceComboR.onChange = [this] {
            engine.setInputSourceR(static_cast<nova::graviton::DelayInputSource>(inputSourceComboR.getSelectedId() - 1));
        };
        addAndMakeVisible(inputSourceComboR);

        gaugeTimeR.setTimeMs(engine.getDelayTimeMsR());
        gaugeTimeR.onTimeChanged = [this](double ms) {
            engine.setDelayTimeMsR(ms);
            if (engine.getLinked()) {
                gaugeTimeL.setTimeMs(ms, juce::dontSendNotification);
                engine.setDelayTimeMsL(ms);
            }
        };
        addAndMakeVisible(gaugeTimeR);

        for (int i = 0; i < static_cast<int>(nova::graviton::NoteDivision::NumDivisions); ++i) {
            noteComboR.addItem(nova::graviton::getNoteDivisionName(static_cast<nova::graviton::NoteDivision>(i)), i + 1);
        }
        noteComboR.setSelectedId(static_cast<int>(engine.getNoteDivisionR()) + 1, juce::dontSendNotification);
        noteComboR.onChange = [this] {
            engine.setNoteDivisionR(static_cast<nova::graviton::NoteDivision>(noteComboR.getSelectedId() - 1));
            if (engine.getLinked()) {
                noteComboL.setSelectedId(noteComboR.getSelectedId(), juce::dontSendNotification);
                engine.setNoteDivisionL(engine.getNoteDivisionR());
            }
        };
        addAndMakeVisible(noteComboR);

        divTimeBtnR.setButtonText(":2");
        divTimeBtnR.onClick = [this] {
            gaugeTimeR.setTimeMs(gaugeTimeR.getTimeMs() * 0.5, juce::sendNotificationSync);
        };
        addAndMakeVisible(divTimeBtnR);

        mulTimeBtnR.setButtonText("x2");
        mulTimeBtnR.onClick = [this] {
            gaugeTimeR.setTimeMs(gaugeTimeR.getTimeMs() * 2.0, juce::sendNotificationSync);
        };
        addAndMakeVisible(mulTimeBtnR);

        deviationR = std::make_unique<NebulaKnob>("Deviation", -100.0, 100.0, engine.getDeviationCentsR(), "ct", juce::Colour(0xff00e5ff), 1);
        deviationR->onValueChanged = [this](double v) { engine.setDeviationCentsR(v); };
        addAndMakeVisible(*deviationR);

        hpfR = std::make_unique<NebulaKnob>("HPF", 10.0, 1000.0, engine.getHpfHzR(), "Hz", juce::Colour(0xff00e5ff), 0);
        hpfR->onValueChanged = [this](double v) { engine.setHpfHzR(v); };
        addAndMakeVisible(*hpfR);

        hpfsR = std::make_unique<NebulaKnob>("HPFS", 6.0, 48.0, engine.getHpfsDbR(), "dB/o", juce::Colour(0xffa855f7), 0);
        hpfsR->onValueChanged = [this](double v) { engine.setHpfsDbR(v); };
        addAndMakeVisible(*hpfsR);

        lpfR = std::make_unique<NebulaKnob>("LPF", 500.0, 20000.0, engine.getLpfHzR(), "Hz", juce::Colour(0xfff97316), 0);
        lpfR->onValueChanged = [this](double v) { engine.setLpfHzR(v); };
        addAndMakeVisible(*lpfR);

        lpfsR = std::make_unique<NebulaKnob>("LPFS", 6.0, 48.0, engine.getLpfsDbR(), "dB/o", juce::Colour(0xffec4899), 0);
        lpfsR->onValueChanged = [this](double v) { engine.setLpfsDbR(v); };
        addAndMakeVisible(*lpfsR);

        feedbackR = std::make_unique<NebulaKnob>("Feedback", 0.0, 100.0, engine.getFeedbackR(), "%", juce::Colour(0xff00e5ff), 1);
        feedbackR->onValueChanged = [this](double v) { engine.setFeedbackR(v); };
        addAndMakeVisible(*feedbackR);

        crossfeedR = std::make_unique<NebulaKnob>("Crossfeed R-L", 0.0, 100.0, engine.getCrossfeedRL(), "%", juce::Colour(0xffd946ef), 1);
        crossfeedR->onValueChanged = [this](double v) { engine.setCrossfeedRL(v); };
        addAndMakeVisible(*crossfeedR);

        phaseFbBtnR.setButtonText("NORM");
        phaseFbBtnR.setClickingTogglesState(true);
        phaseFbBtnR.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b));
        phaseFbBtnR.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090814));
        phaseFbBtnR.onClick = [this] {
            engine.setPhaseInvertFeedbackR(phaseFbBtnR.getToggleState());
            phaseFbBtnR.setButtonText(phaseFbBtnR.getToggleState() ? "INV" : "NORM");
        };
        addAndMakeVisible(phaseFbBtnR);

        phaseCrossBtnR.setButtonText("NORM");
        phaseCrossBtnR.setClickingTogglesState(true);
        phaseCrossBtnR.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b));
        phaseCrossBtnR.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090814));
        phaseCrossBtnR.onClick = [this] {
            engine.setPhaseInvertCrossfeedRL(phaseCrossBtnR.getToggleState());
            phaseCrossBtnR.setButtonText(phaseCrossBtnR.getToggleState() ? "INV" : "NORM");
        };
        addAndMakeVisible(phaseCrossBtnR);

        // ── 5. Stereo Output Mixer ────────────────────────────────────────
        wetL = std::make_unique<NebulaKnob>("Wet", 0.0, 100.0, engine.getWetLevelL(), "%", juce::Colour(0xff00e5ff), 0);
        wetL->onValueChanged = [this](double v) { engine.setWetLevelL(v); };
        addAndMakeVisible(*wetL);

        dryL = std::make_unique<NebulaKnob>("Dry", 0.0, 100.0, engine.getDryLevelL(), "%", juce::Colour(0xff10b981), 0);
        dryL->onValueChanged = [this](double v) { engine.setDryLevelL(v); };
        addAndMakeVisible(*dryL);

        wetPanL = std::make_unique<NebulaKnob>("Wet Pan", -100.0, 100.0, engine.getWetPanL(), "", juce::Colour(0xfff97316), 0);
        wetPanL->onValueChanged = [this](double v) { engine.setWetPanL(v); };
        addAndMakeVisible(*wetPanL);

        dryPanL = std::make_unique<NebulaKnob>("Dry Pan", -100.0, 100.0, engine.getDryPanL(), "", juce::Colour(0xffa855f7), 0);
        dryPanL->onValueChanged = [this](double v) { engine.setDryPanL(v); };
        addAndMakeVisible(*dryPanL);

        wetR = std::make_unique<NebulaKnob>("Wet", 0.0, 100.0, engine.getWetLevelR(), "%", juce::Colour(0xff00e5ff), 0);
        wetR->onValueChanged = [this](double v) { engine.setWetLevelR(v); };
        addAndMakeVisible(*wetR);

        dryR = std::make_unique<NebulaKnob>("Dry", 0.0, 100.0, engine.getDryLevelR(), "%", juce::Colour(0xff10b981), 0);
        dryR->onValueChanged = [this](double v) { engine.setDryLevelR(v); };
        addAndMakeVisible(*dryR);

        wetPanR = std::make_unique<NebulaKnob>("Wet Pan", -100.0, 100.0, engine.getWetPanR(), "", juce::Colour(0xfff97316), 0);
        wetPanR->onValueChanged = [this](double v) { engine.setWetPanR(v); };
        addAndMakeVisible(*wetPanR);

        dryPanR = std::make_unique<NebulaKnob>("Dry Pan", -100.0, 100.0, engine.getDryPanR(), "", juce::Colour(0xffa855f7), 0);
        dryPanR->onValueChanged = [this](double v) { engine.setDryPanR(v); };
        addAndMakeVisible(*dryPanR);

        // ── 6. Master Monitoring & Telemetry ──────────────────────────────
        outGain = std::make_unique<NebulaKnob>("OUT GAIN", -24.0, 24.0, engine.getOutputGainDb(), "dB", juce::Colour(0xff00e5ff), 1);
        outGain->onValueChanged = [this](double v) { engine.setOutputGainDb(v); };
        addAndMakeVisible(*outGain);

        addAndMakeVisible(outMeter);
        addAndMakeVisible(stereoMonitor);

        setSize(1320, 260);
        startTimerHz(30);
    }

    ~NebulaDelayPanel() override {
        stopTimer();
    }

    void paint(juce::Graphics& g) override {
        // Deep Obsidian Navy Background
        g.fillAll(juce::Colour(0xff0b0a17));

        auto drawSectionBox = [&](juce::Rectangle<int> r, const juce::String& title, juce::Colour titleCol) {
            auto rf = r.toFloat();
            g.setColour(juce::Colour(0xff121024));
            g.fillRoundedRectangle(rf, 4.0f);
            g.setColour(juce::Colour(0xff231e42));
            g.drawRoundedRectangle(rf, 4.0f, 1.0f);

            // Top Header Bar
            auto headerR = rf.removeFromTop(20.0f);
            g.setColour(juce::Colour(0xff191632));
            g.fillRoundedRectangle(headerR, 3.5f);
            g.setColour(titleCol);
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.drawText(title, headerR.reduced(6.0f, 0.0f), juce::Justification::centredLeft, false);
        };

        drawSectionBox(inPanelBounds, "INPUT", juce::Colour(0xff00e5ff));
        drawSectionBox(leftEngineBounds, "LEFT DELAY ENGINE", juce::Colour(0xff00e5ff));
        drawSectionBox(rightEngineBounds, "RIGHT DELAY ENGINE", juce::Colour(0xff00e5ff));
        drawSectionBox(mixerBounds, "STEREO MIXER", juce::Colour(0xffd946ef));
        drawSectionBox(outPanelBounds, "OUTPUT & MONITOR", juce::Colour(0xff10b981));

        // Subheaders inside Mixer
        g.setColour(juce::Colour(0xff00e5ff));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("LEFT OUT", mixerBounds.getX() + 6, mixerBounds.getY() + 22, mixerBounds.getWidth() - 12, 14, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffd946ef));
        g.drawText("RIGHT OUT", mixerBounds.getX() + 6, mixerBounds.getY() + 120, mixerBounds.getWidth() - 12, 14, juce::Justification::centredLeft);
    }

    void resized() override {
        const int w = getWidth();
        const int h = getHeight();

        // 1. Top Toolbar (h: 24px)
        const int tbY = 3;
        int tx = 8;
        presetBtn.setBounds(tx, tbY, 60, 22); tx += 64;
        presetCombo.setBounds(tx, tbY, 130, 22); tx += 134;
        defaultBtn.setBounds(tx, tbY, 65, 22); tx += 69;
        saveBtn.setBounds(tx, tbY, 52, 22); tx += 56;
        infoBtn.setBounds(tx, tbY, 48, 22); tx += 54;

        syncBtn.setBounds(tx, tbY, 68, 22); tx += 72;
        linkBtn.setBounds(tx, tbY, 70, 22); tx += 74;
        analogModeCombo.setBounds(tx, tbY, 110, 22); tx += 114;
        resetClipBtn.setBounds(tx, tbY, 82, 22);

        fxOnBtn.setBounds(w - 74, tbY, 66, 22);

        // 2. Main Compartments Layout
        const int mainY = 29;
        const int mainH = h - mainY - 4;

        const int inW = 92;
        const int outW = 180;
        const int mixerW = 196;
        const int engineW = (w - inW - outW - mixerW - 28) / 2;

        int curX = 6;
        inPanelBounds = juce::Rectangle<int>(curX, mainY, inW, mainH); curX += inW + 5;
        leftEngineBounds = juce::Rectangle<int>(curX, mainY, engineW, mainH); curX += engineW + 5;
        rightEngineBounds = juce::Rectangle<int>(curX, mainY, engineW, mainH); curX += engineW + 5;
        mixerBounds = juce::Rectangle<int>(curX, mainY, mixerW, mainH); curX += mixerW + 5;
        outPanelBounds = juce::Rectangle<int>(curX, mainY, outW, mainH);

        // ── Input Panel Layout ──
        if (inGain) inGain->setBounds(inPanelBounds.getX() + 6, inPanelBounds.getY() + 24, inPanelBounds.getWidth() - 12, 52);
        inMeter.setBounds(inPanelBounds.getX() + 6, inPanelBounds.getY() + 80, inPanelBounds.getWidth() - 12, inPanelBounds.getHeight() - 86);

        // ── Delay Engines Layout (Symmetrical) ──
        auto layoutEngine = [&](juce::Rectangle<int> eb,
                                juce::ComboBox& inCombo,
                                NebulaBigGauge& gauge,
                                juce::ComboBox& nCombo,
                                juce::TextButton& dBtn,
                                juce::TextButton& mBtn,
                                std::unique_ptr<NebulaKnob>& devK,
                                std::unique_ptr<NebulaKnob>& hpfK,
                                std::unique_ptr<NebulaKnob>& hpfsK,
                                std::unique_ptr<NebulaKnob>& lpfK,
                                std::unique_ptr<NebulaKnob>& lpfsK,
                                std::unique_ptr<NebulaKnob>& fbK,
                                std::unique_ptr<NebulaKnob>& crK,
                                juce::TextButton& pFbBtn,
                                juce::TextButton& pCrBtn) {
            inCombo.setBounds(eb.getRight() - 86, eb.getY() + 2, 80, 16);

            // Left sub-part: Delay Gauge & Timing
            const int leftPartW = 126;
            const int gaugeSize = 74;
            const int gx = eb.getX() + 6;
            const int gy = eb.getY() + 24;
            gauge.setBounds(gx, gy, gaugeSize, gaugeSize);

            // Note Div & Multiply buttons
            const int nX = gx + gaugeSize + 4;
            nCombo.setBounds(nX, gy, 40, 18);
            dBtn.setBounds(nX, gy + 22, 19, 16);
            mBtn.setBounds(nX + 21, gy + 22, 19, 16);
            if (devK) devK->setBounds(gx, gy + gaugeSize + 2, leftPartW - 6, 48);

            // Right sub-part: Filters (Top) & Feedback (Bottom)
            const int filterStartX = eb.getX() + leftPartW + 6;
            const int filterW = eb.getRight() - filterStartX - 6;
            const int kw = filterW / 4;
            const int kh = 48;

            const int row1Y = eb.getY() + 24;
            if (hpfK)  hpfK->setBounds(filterStartX, row1Y, kw, kh);
            if (hpfsK) hpfsK->setBounds(filterStartX + kw, row1Y, kw, kh);
            if (lpfK)  lpfK->setBounds(filterStartX + kw * 2, row1Y, kw, kh);
            if (lpfsK) lpfsK->setBounds(filterStartX + kw * 3, row1Y, kw, kh);

            const int row2Y = row1Y + kh + 6;
            const int fbColW = filterW / 2;
            if (fbK) fbK->setBounds(filterStartX, row2Y, fbColW - 32, kh);
            pFbBtn.setBounds(filterStartX + fbColW - 30, row2Y + 20, 26, 18);

            if (crK) crK->setBounds(filterStartX + fbColW + 4, row2Y, fbColW - 32, kh);
            pCrBtn.setBounds(filterStartX + fbColW + fbColW - 26, row2Y + 20, 26, 18);
        };

        layoutEngine(leftEngineBounds, inputSourceComboL, gaugeTimeL, noteComboL, divTimeBtnL, mulTimeBtnL,
                     deviationL, hpfL, hpfsL, lpfL, lpfsL, feedbackL, crossfeedL, phaseFbBtnL, phaseCrossBtnL);

        layoutEngine(rightEngineBounds, inputSourceComboR, gaugeTimeR, noteComboR, divTimeBtnR, mulTimeBtnR,
                     deviationR, hpfR, hpfsR, lpfR, lpfsR, feedbackR, crossfeedR, phaseFbBtnR, phaseCrossBtnR);

        // ── Stereo Mixer Layout ──
        const int mx = mixerBounds.getX() + 6;
        const int mw = (mixerBounds.getWidth() - 16) / 2;
        const int mh = 38;

        // Left Output
        int my = mixerBounds.getY() + 38;
        if (wetL)    wetL->setBounds(mx, my, mw, mh);
        if (dryL)    dryL->setBounds(mx + mw + 4, my, mw, mh);
        my += mh + 2;
        if (wetPanL) wetPanL->setBounds(mx, my, mw, mh);
        if (dryPanL) dryPanL->setBounds(mx + mw + 4, my, mw, mh);

        // Right Output
        my = mixerBounds.getY() + 136;
        if (wetR)    wetR->setBounds(mx, my, mw, mh);
        if (dryR)    dryR->setBounds(mx + mw + 4, my, mw, mh);
        my += mh + 2;
        if (wetPanR) wetPanR->setBounds(mx, my, mw, mh);
        if (dryPanR) dryPanR->setBounds(mx + mw + 4, my, mw, mh);

        // ── Output Panel Layout ──
        const int outPartW = 68;
        if (outGain) outGain->setBounds(outPanelBounds.getX() + 4, outPanelBounds.getY() + 24, outPartW, 52);
        outMeter.setBounds(outPanelBounds.getX() + 4, outPanelBounds.getY() + 80, outPartW, outPanelBounds.getHeight() - 86);

        const int monX = outPanelBounds.getX() + outPartW + 6;
        const int monW = outPanelBounds.getRight() - monX - 4;
        stereoMonitor.setBounds(monX, outPanelBounds.getY() + 24, monW, outPanelBounds.getHeight() - 28);
    }

private:
    void showPresetMenu() {
        juce::PopupMenu m;
        m.addSectionHeader("SOUNDSPOT NEBULA PRESETS");
        const auto& presets = nova::graviton::NebulaStereoDelay::getFactoryPresets();
        for (size_t i = 0; i < presets.size(); ++i) {
            m.addItem(static_cast<int>(i + 1), juce::String(i + 1) + ". " + presets[i].name + " (" + presets[i].description + ")");
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
            [this](int r) {
                if (r >= 1 && r <= 10) {
                    presetCombo.setSelectedId(r, juce::dontSendNotification);
                    applyFactoryPreset(r - 1);
                }
            });
    }

    void applyFactoryPreset(int index) {
        const auto& presets = nova::graviton::NebulaStereoDelay::getFactoryPresets();
        if (index >= 0 && index < static_cast<int>(presets.size())) {
            engine.loadPreset(presets[static_cast<size_t>(index)]);
            updateAllUiFromEngine();
            presetCombo.setText(presets[static_cast<size_t>(index)].name, juce::dontSendNotification);
        }
    }

    void updateAllUiFromEngine() {
        syncBtn.setToggleState(engine.getSyncEnabled(), juce::dontSendNotification);
        syncBtn.setButtonText(engine.getSyncEnabled() ? "Sync On" : "Sync Off");

        analogModeCombo.setSelectedId(static_cast<int>(engine.getAnalogMode()) + 1, juce::dontSendNotification);

        if (inGain) inGain->setValue(engine.getInputGainDb(), juce::dontSendNotification);
        if (outGain) outGain->setValue(engine.getOutputGainDb(), juce::dontSendNotification);

        // Left
        inputSourceComboL.setSelectedId(static_cast<int>(engine.getInputSourceL()) + 1, juce::dontSendNotification);
        gaugeTimeL.setTimeMs(engine.getDelayTimeMsL(), juce::dontSendNotification);
        noteComboL.setSelectedId(static_cast<int>(engine.getNoteDivisionL()) + 1, juce::dontSendNotification);
        if (deviationL) deviationL->setValue(engine.getDeviationCentsL(), juce::dontSendNotification);
        if (hpfL) hpfL->setValue(engine.getHpfHzL(), juce::dontSendNotification);
        if (hpfsL) hpfsL->setValue(engine.getHpfsDbL(), juce::dontSendNotification);
        if (lpfL) lpfL->setValue(engine.getLpfHzL(), juce::dontSendNotification);
        if (lpfsL) lpfsL->setValue(engine.getLpfsDbL(), juce::dontSendNotification);
        if (feedbackL) feedbackL->setValue(engine.getFeedbackL(), juce::dontSendNotification);
        if (crossfeedL) crossfeedL->setValue(engine.getCrossfeedLR(), juce::dontSendNotification);
        phaseFbBtnL.setToggleState(engine.getPhaseInvertFeedbackL(), juce::dontSendNotification);
        phaseFbBtnL.setButtonText(engine.getPhaseInvertFeedbackL() ? "INV" : "NORM");
        phaseCrossBtnL.setToggleState(engine.getPhaseInvertCrossfeedLR(), juce::dontSendNotification);
        phaseCrossBtnL.setButtonText(engine.getPhaseInvertCrossfeedLR() ? "INV" : "NORM");

        // Right
        inputSourceComboR.setSelectedId(static_cast<int>(engine.getInputSourceR()) + 1, juce::dontSendNotification);
        gaugeTimeR.setTimeMs(engine.getDelayTimeMsR(), juce::dontSendNotification);
        noteComboR.setSelectedId(static_cast<int>(engine.getNoteDivisionR()) + 1, juce::dontSendNotification);
        if (deviationR) deviationR->setValue(engine.getDeviationCentsR(), juce::dontSendNotification);
        if (hpfR) hpfR->setValue(engine.getHpfHzR(), juce::dontSendNotification);
        if (hpfsR) hpfsR->setValue(engine.getHpfsDbR(), juce::dontSendNotification);
        if (lpfR) lpfR->setValue(engine.getLpfHzR(), juce::dontSendNotification);
        if (lpfsR) lpfsR->setValue(engine.getLpfsDbR(), juce::dontSendNotification);
        if (feedbackR) feedbackR->setValue(engine.getFeedbackR(), juce::dontSendNotification);
        if (crossfeedR) crossfeedR->setValue(engine.getCrossfeedRL(), juce::dontSendNotification);
        phaseFbBtnR.setToggleState(engine.getPhaseInvertFeedbackR(), juce::dontSendNotification);
        phaseFbBtnR.setButtonText(engine.getPhaseInvertFeedbackR() ? "INV" : "NORM");
        phaseCrossBtnR.setToggleState(engine.getPhaseInvertCrossfeedRL(), juce::dontSendNotification);
        phaseCrossBtnR.setButtonText(engine.getPhaseInvertCrossfeedRL() ? "INV" : "NORM");

        // Output Mixer
        if (wetL) wetL->setValue(engine.getWetLevelL(), juce::dontSendNotification);
        if (dryL) dryL->setValue(engine.getDryLevelL(), juce::dontSendNotification);
        if (wetPanL) wetPanL->setValue(engine.getWetPanL(), juce::dontSendNotification);
        if (dryPanL) dryPanL->setValue(engine.getDryPanL(), juce::dontSendNotification);
        if (wetR) wetR->setValue(engine.getWetLevelR(), juce::dontSendNotification);
        if (dryR) dryR->setValue(engine.getDryLevelR(), juce::dontSendNotification);
        if (wetPanR) wetPanR->setValue(engine.getWetPanR(), juce::dontSendNotification);
        if (dryPanR) dryPanR->setValue(engine.getDryPanR(), juce::dontSendNotification);

        repaint();
    }

    void showInfoDialog() {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "SOUNDSPOT NEBULA DELAY ENGINE",
            "Nebula Stereo Delay Processor\n\n"
            "\xe2\x80\xa2 Engine: Dual-Tap Hermite Interpolated Delay with up to 3000ms delay time.\n"
            "\xe2\x80\xa2 Multi-Slope Filters: Cascaded 4-stage IIR Butterworth HPF & LPF (6 - 48 dB/oct).\n"
            "\xe2\x80\xa2 Analog Emulation: Warm Tape (tanh saturation), Vintage Tube, Lo-Fi, Clean Digital.\n"
            "\xe2\x80\xa2 Phase Matrix: Independent phase inversion for feedback & crossfeed paths.\n"
            "\xe2\x80\xa2 Monitoring: Input/Output Peak meters with Peak Hold, persistent Clip Hold, Stereo Correlation (-1 to +1), and L/R Balance.",
            "OK");
    }

    void timerCallback() override {
        inMeter.updateLevels(engine.getMeterInL(), engine.getMeterInR(), engine.getPeakHoldIn(), engine.hasClipIn());
        outMeter.updateLevels(engine.getMeterOutL(), engine.getMeterOutR(), engine.getPeakHoldOut(), engine.hasClipOut());
        stereoMonitor.updateTelemetry(engine.getStereoCorrelation(), engine.getLrBalance());
    }

    nova::graviton::NebulaStereoDelay& engine;

    juce::Rectangle<int> inPanelBounds;
    juce::Rectangle<int> leftEngineBounds;
    juce::Rectangle<int> rightEngineBounds;
    juce::Rectangle<int> mixerBounds;
    juce::Rectangle<int> outPanelBounds;

    // Header & Toolbar
    juce::TextButton presetBtn;
    juce::ComboBox presetCombo;
    juce::TextButton defaultBtn;
    juce::TextButton saveBtn;
    juce::TextButton infoBtn;
    juce::TextButton syncBtn;
    juce::TextButton linkBtn;
    juce::ComboBox analogModeCombo;
    juce::TextButton fxOnBtn;
    juce::TextButton resetClipBtn;

    // Input Monitoring
    std::unique_ptr<NebulaKnob> inGain;
    NebulaStereoPeakMeter inMeter;

    // Left Delay Components
    juce::ComboBox inputSourceComboL;
    NebulaBigGauge gaugeTimeL;
    juce::ComboBox noteComboL;
    juce::TextButton divTimeBtnL;
    juce::TextButton mulTimeBtnL;
    std::unique_ptr<NebulaKnob> deviationL;
    std::unique_ptr<NebulaKnob> hpfL;
    std::unique_ptr<NebulaKnob> hpfsL;
    std::unique_ptr<NebulaKnob> lpfL;
    std::unique_ptr<NebulaKnob> lpfsL;
    std::unique_ptr<NebulaKnob> feedbackL;
    std::unique_ptr<NebulaKnob> crossfeedL;
    juce::TextButton phaseFbBtnL;
    juce::TextButton phaseCrossBtnL;

    // Right Delay Components
    juce::ComboBox inputSourceComboR;
    NebulaBigGauge gaugeTimeR;
    juce::ComboBox noteComboR;
    juce::TextButton divTimeBtnR;
    juce::TextButton mulTimeBtnR;
    std::unique_ptr<NebulaKnob> deviationR;
    std::unique_ptr<NebulaKnob> hpfR;
    std::unique_ptr<NebulaKnob> hpfsR;
    std::unique_ptr<NebulaKnob> lpfR;
    std::unique_ptr<NebulaKnob> lpfsR;
    std::unique_ptr<NebulaKnob> feedbackR;
    std::unique_ptr<NebulaKnob> crossfeedR;
    juce::TextButton phaseFbBtnR;
    juce::TextButton phaseCrossBtnR;

    // Stereo Output Mixer Components
    std::unique_ptr<NebulaKnob> wetL;
    std::unique_ptr<NebulaKnob> dryL;
    std::unique_ptr<NebulaKnob> wetPanL;
    std::unique_ptr<NebulaKnob> dryPanL;

    std::unique_ptr<NebulaKnob> wetR;
    std::unique_ptr<NebulaKnob> dryR;
    std::unique_ptr<NebulaKnob> wetPanR;
    std::unique_ptr<NebulaKnob> dryPanR;

    // Output Monitoring & Telemetry
    std::unique_ptr<NebulaKnob> outGain;
    NebulaStereoPeakMeter outMeter;
    NebulaStereoMonitor stereoMonitor;
};

} // namespace nova
