#pragma once

#include "Effects/GravitonVSS4.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>
#include <iomanip>
#include <sstream>

namespace nova {

// ==============================================================================
// 1. High-Precision Hardware-Inspired Parameter Card with Rotary Knob
// Replicates the flat rectangular blue and red parameter blocks from System 6000
// ==============================================================================
class VSS4KnobCard final : public juce::Component {
public:
    enum class Theme { Blue, Red };

    VSS4KnobCard(const juce::String& titleText,
                 double minV, double maxV, double defaultV,
                 const juce::String& unitSuffix,
                 Theme cardTheme,
                 int decimalPlaces = 1)
        : title(titleText),
          minValue(minV),
          maxValue(maxV),
          defaultValue(defaultV),
          currentValue(defaultV),
          unit(unitSuffix),
          theme(cardTheme),
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
        auto bounds = getLocalBounds().toFloat();
        const float corner = 7.0f;

        // Card Background Color
        juce::Colour bgCol = (theme == Theme::Blue)
                                ? juce::Colour(0xff6da2dc)   // Blue section
                                : juce::Colour(0xffd57885);  // Red section

        if (isMouseOverOrDragging()) {
            bgCol = bgCol.brighter(0.04f);
        }

        g.setColour(bgCol);
        g.fillRoundedRectangle(bounds, corner);

        // Subtle inner border
        g.setColour(juce::Colour(0x20000000));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

        // Typography Colors (dark charcoal navy / deep wine)
        const juce::Colour textCol = (theme == Theme::Blue)
                                         ? juce::Colour(0xff182333)
                                         : juce::Colour(0xff2a151b);

        // 1. Title Label (top-left)
        g.setColour(textCol);
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawText(title, static_cast<int>(bounds.getX() + 12.0f), static_cast<int>(bounds.getY() + 8.0f),
                   static_cast<int>(bounds.getWidth() - 65.0f), 18,
                   juce::Justification::topLeft, true);

        // 2. Large Value Text (bottom-left)
        g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
        juce::String valStr;
        if (decimals == 0) {
            valStr = juce::String(static_cast<int>(std::round(currentValue)));
        } else {
            valStr = juce::String(currentValue, decimals);
        }
        if (unit.isNotEmpty()) {
            valStr << " " << unit;
        }

        g.drawText(valStr, static_cast<int>(bounds.getX() + 12.0f),
                   static_cast<int>(bounds.getBottom() - 36.0f),
                   static_cast<int>(bounds.getWidth() - 65.0f), 30,
                   juce::Justification::bottomLeft, true);

        // 3. Rotary Knob with Position Indicator (bottom-right)
        const float knobSize = std::min(bounds.getHeight() * 0.44f, 34.0f);
        const float kx = bounds.getRight() - knobSize - 12.0f;
        const float ky = bounds.getBottom() - knobSize - 8.0f;
        const auto knobBounds = juce::Rectangle<float>(kx, ky, knobSize, knobSize);

        // Outer Bezel Ring (dark metal)
        g.setColour(juce::Colour(0x5010151c));
        g.drawEllipse(knobBounds.expanded(1.5f), 1.5f);

        // Knob Face Disc
        g.setColour(juce::Colour(0xff1b222c));
        g.fillEllipse(knobBounds);
        g.setColour(juce::Colour(0xff2d3744));
        g.drawEllipse(knobBounds, 1.2f);

        // Angle indicator (-135 to +135 deg)
        const float norm = static_cast<float>((currentValue - minValue) / std::max(1e-4, maxValue - minValue));
        constexpr float startAngle = -2.35619449f; // -135 deg in rad
        constexpr float endAngle   =  2.35619449f; // +135 deg in rad
        const float angle = startAngle + norm * (endAngle - startAngle);

        const float centerKx = knobBounds.getCentreX();
        const float centerKy = knobBounds.getCentreY();
        const float pointerLen = knobSize * 0.36f;
        const float px = centerKx + std::sin(angle) * pointerLen;
        const float py = centerKy - std::cos(angle) * pointerLen;

        // Pointer Line / Dot
        g.setColour(juce::Colours::white);
        g.drawLine(centerKx, centerKy, px, py, 2.0f);
        g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);

        // Center Cap
        g.setColour(juce::Colour(0xff0d1117));
        g.fillEllipse(centerKx - 3.0f, centerKy - 3.0f, 6.0f, 6.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragStartValue = currentValue;
        dragStartPos = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float deltaY = dragStartPos.y - e.position.y;
        const float deltaX = e.position.x - dragStartPos.x;
        const float delta = (std::abs(deltaY) > std::abs(deltaX)) ? deltaY : deltaX;

        const double range = maxValue - minValue;
        const double step = (range / 150.0);
        const double newVal = dragStartValue + delta * step;
        setValue(newVal, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        setValue(defaultValue, juce::sendNotificationSync);
    }

private:
    juce::String title;
    double minValue;
    double maxValue;
    double defaultValue;
    double currentValue;
    juce::String unit;
    Theme theme;
    int decimals;

    double dragStartValue = 0.0;
    juce::Point<float> dragStartPos;
};

// ==============================================================================
// 2. Assignable Parameter Pill Strip (1–6)
// ==============================================================================
class VSS4AssignablePill final : public juce::Component {
public:
    VSS4AssignablePill(int index, const juce::String& paramName)
        : slotIndex(index), labelText(juce::String(index) + ". " + paramName) {
        setRepaintsOnMouseActivity(true);
    }

    void setParameterName(const juce::String& name) {
        labelText = juce::String(slotIndex) + ". " + name;
        repaint();
    }

    std::function<void(int)> onClicked;

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        const float corner = 5.0f;

        juce::Colour bgCol = isMouseOverOrDragging() ? juce::Colour(0xfff0f4f8)
                                                     : juce::Colour(0xffd8dee9);

        g.setColour(bgCol);
        g.fillRoundedRectangle(bounds, corner);

        g.setColour(juce::Colour(0x30000000));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

        g.setColour(juce::Colour(0xff161f2c));
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawText(labelText, static_cast<int>(bounds.getX() + 10.0f), static_cast<int>(bounds.getY()),
                   static_cast<int>(bounds.getWidth() - 20.0f), static_cast<int>(bounds.getHeight()),
                   juce::Justification::centredLeft, true);
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (onClicked) onClicked(slotIndex);
    }

private:
    int slotIndex;
    juce::String labelText;
};

// ==============================================================================
// 3. Stereo Precision Level Meter with dB Scale
// ==============================================================================
class VSS4StereoMeter final : public juce::Component {
public:
    VSS4StereoMeter(const juce::String& meterTitle) : title(meterTitle) {}

    void setLevels(float leftPeak, float rightPeak) {
        levelL = std::clamp(leftPeak, 0.0f, 1.5f);
        levelR = std::clamp(rightPeak, 0.0f, 1.5f);
        if (levelL >= 1.0f || levelR >= 1.0f) clip = true;
        repaint();
    }

    void resetClip() { clip = false; repaint(); }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xff0e131b));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff212a38));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Header Title
        g.setColour(juce::Colour(0xff8b949e));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(title, bounds.removeFromTop(14.0f), juce::Justification::centred, false);

        // Clip indicator
        auto clipArea = bounds.removeFromTop(7.0f).reduced(2.0f, 0.5f);
        g.setColour(clip ? juce::Colour(0xffff3b30) : juce::Colour(0xff331517));
        g.fillRoundedRectangle(clipArea, 2.0f);

        const float barW = (bounds.getWidth() - 6.0f) * 0.5f;
        const auto barAreaL = juce::Rectangle<float>(bounds.getX() + 2.0f, bounds.getY() + 2.0f, barW, bounds.getHeight() - 4.0f);
        const auto barAreaR = juce::Rectangle<float>(barAreaL.getRight() + 2.0f, barAreaL.getY(), barW, barAreaL.getHeight());

        auto drawBar = [&](juce::Rectangle<float> r, float peakVal) {
            g.setColour(juce::Colour(0xff161e29));
            g.fillRect(r);

            const float db = (peakVal > 1e-4f) ? 20.0f * std::log10(peakVal) : -60.0f;
            // Map -60 dB to 0 dB
            const float norm = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
            const float filledH = r.getHeight() * norm;
            const auto filledArea = r.removeFromBottom(filledH);

            juce::ColourGradient grad(juce::Colour(0xff2ea043), filledArea.getCentreX(), filledArea.getBottom(),
                                      juce::Colour(0xffe3b341), filledArea.getCentreX(), filledArea.getY() + filledArea.getHeight() * 0.25f, false);
            grad.addColour(0.9, juce::Colour(0xfff85149));
            g.setGradientFill(grad);
            g.fillRect(filledArea);
        };

        drawBar(barAreaL, levelL);
        drawBar(barAreaR, levelR);
    }

private:
    juce::String title;
    float levelL = 0.0f;
    float levelR = 0.0f;
    bool clip = false;
};

// ==============================================================================
// 4. Main VSS4 HD Desktop Plugin Interface Component (System 6000 Replica)
// ==============================================================================
class VSS4Panel final : public juce::Component, private juce::Timer {
public:
    enum class PageTab { Main, Setup, Colour, Gloss };

    explicit VSS4Panel(nova::graviton::VSS4ReverbEngine& engineRef)
        : engine(engineRef),
          inMeter("IN"),
          outMeter("OUT") {
        setOpaque(true);

        // ── 1. Header & Controls ──────────────────────────────────────────
        bypassButton.setButtonText("BYPASS");
        bypassButton.setClickingTogglesState(true);
        bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff152238));
        bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb62324));
        bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff58a6ff));
        bypassButton.onClick = [this] {
            isBypassed = bypassButton.getToggleState();
            repaint();
        };
        addAndMakeVisible(bypassButton);

        // Tab Navigation Buttons (MAIN, SETUP, COLOUR, GLOSS)
        const char* tabNames[4] = { "MAIN", "SETUP", "COLOUR", "GLOSS" };
        for (int i = 0; i < 4; ++i) {
            tabButtons[static_cast<size_t>(i)].setButtonText(tabNames[i]);
            tabButtons[static_cast<size_t>(i)].setClickingTogglesState(false);
            tabButtons[static_cast<size_t>(i)].onClick = [this, i] {
                activeTab = static_cast<PageTab>(i);
                updateTabDisplay();
                repaint();
            };
            addAndMakeVisible(tabButtons[static_cast<size_t>(i)]);
        }

        // ── 2. Blue Control Section (Main Reverb Parameters) ──────────────
        cardDecay = std::make_unique<VSS4KnobCard>("MASTER DECAY", 0.1, 20.0, 2.8, "s", VSS4KnobCard::Theme::Blue, 1);
        cardDecay->setValue(engine.getMasterDecay());
        cardDecay->onValueChanged = [this](double v) {
            engine.setMasterDecay(v);
            if (pillAssign[0]) pillAssign[0]->setParameterName("MASTER DECAY: " + juce::String(v, 1) + " s");
        };
        addAndMakeVisible(*cardDecay);

        cardHiCut = std::make_unique<VSS4KnobCard>("HI CUT", 0.5, 20.0, 2.17, "kHz", VSS4KnobCard::Theme::Blue, 2);
        cardHiCut->setValue(engine.getHiCut() * 0.001);
        cardHiCut->onValueChanged = [this](double v) {
            engine.setHiCut(v * 1000.0);
            if (pillAssign[2]) pillAssign[2]->setParameterName("HI CUT: " + juce::String(v, 2) + " kHz");
        };
        addAndMakeVisible(*cardHiCut);

        cardReverbSize = std::make_unique<VSS4KnobCard>("REVERB SIZE", 1.0, 25.0, 14.0, "", VSS4KnobCard::Theme::Blue, 0);
        cardReverbSize->setValue(engine.getReverbSize());
        cardReverbSize->onValueChanged = [this](double v) {
            engine.setReverbSize(v);
            if (pillAssign[1]) pillAssign[1]->setParameterName("REVERB SIZE: " + juce::String(static_cast<int>(v)));
        };
        addAndMakeVisible(*cardReverbSize);

        cardPreDelay = std::make_unique<VSS4KnobCard>("PRE DELAY", 0.0, 300.0, 22.0, "ms", VSS4KnobCard::Theme::Blue, 0);
        cardPreDelay->setValue(engine.getPreDelay());
        cardPreDelay->onValueChanged = [this](double v) {
            engine.setPreDelay(v);
            if (pillAssign[3]) pillAssign[3]->setParameterName("PRE DELAY: " + juce::String(static_cast<int>(v)) + " ms");
        };
        addAndMakeVisible(*cardPreDelay);

        cardReverbDelay = std::make_unique<VSS4KnobCard>("REVERB DELAY", 0.0, 100.0, 0.0, "ms", VSS4KnobCard::Theme::Blue, 0);
        cardReverbDelay->setValue(engine.getReverbDelay());
        cardReverbDelay->onValueChanged = [this](double v) { engine.setReverbDelay(v); };
        addAndMakeVisible(*cardReverbDelay);

        cardReverbWidth = std::make_unique<VSS4KnobCard>("REVERB WIDTH", 0.0, 10.0, 5.0, "", VSS4KnobCard::Theme::Blue, 0);
        cardReverbWidth->setValue(5.0);
        cardReverbWidth->onValueChanged = [this](double v) { engine.setReverbWidth(v * 20.0); };
        addAndMakeVisible(*cardReverbWidth);

        // ── 3. Red Control Section (Early Reflections Parameters) ─────────
        cardEarlyDecrease = std::make_unique<VSS4KnobCard>("EARLY DECREASE", 0.0, 100.0, 3.0, "%", VSS4KnobCard::Theme::Red, 0);
        cardEarlyDecrease->setValue(engine.getEarlyDecrease());
        cardEarlyDecrease->onValueChanged = [this](double v) {
            engine.setEarlyDecrease(v);
            if (pillAssign[4]) pillAssign[4]->setParameterName("EARLY DECREASE: " + juce::String(static_cast<int>(v)) + "%");
        };
        addAndMakeVisible(*cardEarlyDecrease);

        cardHiColor = std::make_unique<VSS4KnobCard>("HI COLOR", -50.0, 50.0, 20.0, "", VSS4KnobCard::Theme::Red, 0);
        cardHiColor->setValue(engine.getHiColor());
        cardHiColor->onValueChanged = [this](double v) { engine.setHiColor(v); };
        addAndMakeVisible(*cardHiColor);

        cardEarlyStart = std::make_unique<VSS4KnobCard>("EARLY START", 0.0, 100.0, 20.0, "%", VSS4KnobCard::Theme::Red, 0);
        cardEarlyStart->setValue(engine.getEarlyStart());
        cardEarlyStart->onValueChanged = [this](double v) {
            engine.setEarlyStart(v);
            if (pillAssign[5]) pillAssign[5]->setParameterName("EARLY START: " + juce::String(static_cast<int>(v)) + "%");
        };
        addAndMakeVisible(*cardEarlyStart);

        cardEarlyStop = std::make_unique<VSS4KnobCard>("EARLY STOP", 10.0, 100.0, 100.0, "%", VSS4KnobCard::Theme::Red, 0);
        cardEarlyStop->setValue(engine.getEarlyStop());
        cardEarlyStop->onValueChanged = [this](double v) { engine.setEarlyStop(v); };
        addAndMakeVisible(*cardEarlyStop);

        cardLoColor = std::make_unique<VSS4KnobCard>("LO COLOR", -50.0, 50.0, 12.0, "", VSS4KnobCard::Theme::Red, 0);
        cardLoColor->setValue(engine.getLoColor());
        cardLoColor->onValueChanged = [this](double v) { engine.setLoColor(v); };
        addAndMakeVisible(*cardLoColor);

        cardEarlyGain = std::make_unique<VSS4KnobCard>("EARLY GAIN", -60.0, 10.0, -1.0, "dB", VSS4KnobCard::Theme::Red, 1);
        cardEarlyGain->setValue(engine.getEarlyLevelDb());
        cardEarlyGain->onValueChanged = [this](double v) { engine.setEarlyLevelDb(v); };
        addAndMakeVisible(*cardEarlyGain);

        // ── 4. Assignable Controls 1–6 ────────────────────────────────────
        const char* assignDefaults[6] = {
            "MASTER DECAY", "REVERB SIZE", "HI CUT", "PRE DELAY", "EARLY DECREASE", "EARLY START"
        };
        for (int i = 0; i < 6; ++i) {
            pillAssign[static_cast<size_t>(i)] = std::make_unique<VSS4AssignablePill>(i + 1, assignDefaults[i]);
            pillAssign[static_cast<size_t>(i)]->onClicked = [this, i](int) {
                // Focus corresponding parameter card
                repaint();
            };
            addAndMakeVisible(*pillAssign[static_cast<size_t>(i)]);
        }

        // ── 5. Metering & Right Side Output Strip ─────────────────────────
        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);

        // ── 6. Preset Management Footer ───────────────────────────────────
        presetButton.setButtonText("023 \xc2\xb7 Halls \xc2\xb7 VSS4HD Large Hall");
        presetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
        presetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
        presetButton.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible(presetButton);

        prevPresetBtn.setButtonText("<");
        prevPresetBtn.onClick = [this] { cyclePreset(-1); };
        addAndMakeVisible(prevPresetBtn);

        nextPresetBtn.setButtonText(">");
        nextPresetBtn.onClick = [this] { cyclePreset(1); };
        addAndMakeVisible(nextPresetBtn);

        favoriteBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x99\xa5")); // Heart
        favoriteBtn.setClickingTogglesState(true);
        favoriteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
        favoriteBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xfff85149));
        addAndMakeVisible(favoriteBtn);

        infoBtn.setButtonText("(i)");
        infoBtn.setTooltip("VSS4 HD Parameter & Algorithm Specification");
        infoBtn.onClick = [this] {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "TC Electronic VSS4 HD",
                "System 6000 Dual-Source True Stereo Algorithmic Reverb\n"
                "Sample Rate: " + juce::String(48000) + " Hz\n"
                "Architecture: 8-Channel FDN + 12-Ray Early Reflection Matrix\n"
                "Reference: MIDAS Heritage-D HD96-24 Chapter 15");
        };
        addAndMakeVisible(infoBtn);

        settingsBtn.setButtonText("\xe2\x9a\x99"); // Gear icon
        settingsBtn.onClick = [this] { showSettingsMenu(); };
        addAndMakeVisible(settingsBtn);

        setSize(1000, 650);
        updateTabDisplay();
        startTimerHz(30);
    }

    ~VSS4Panel() override {
        stopTimer();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Dark Navy Blue Background (#0c1017)
        g.fillAll(juce::Colour(0xff0b0e14));

        // Outer Industrial Boundary Border
        g.setColour(juce::Colour(0xff212836));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 10.0f, 1.5f);

        // Header Title: VSS4 HD – SYSTEM 6000
        g.setColour(juce::Colour(0xfff0f6fc));
        g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
        g.drawText("VSS4 HD \xe2\x80\x93 SYSTEM 6000", 24, 12, 340, 24, juce::Justification::centredLeft);

        // Sub-Header Banner (MASTER REVERB)
        const auto masterReverbBounds = juce::Rectangle<float>(24.0f, 44.0f, bounds.getWidth() - 360.0f, 32.0f);
        g.setColour(juce::Colour(0xff4a5360));
        g.fillRoundedRectangle(masterReverbBounds, 6.0f);
        g.setColour(juce::Colour(0xfff0f6fc));
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("MASTER REVERB", masterReverbBounds.translated(14.0f, 0.0f), juce::Justification::centredLeft);

        // Blue Control Section Title
        g.setColour(juce::Colour(0xfff0f6fc));
        g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
        g.drawText("BLUE CONTROL SECTION", 24, static_cast<int>(blueSectionTop - 20.0f), 300, 18, juce::Justification::centredLeft);

        // Red Control Section Title
        g.drawText("RED CONTROL SECTION \xe2\x80\x93 EARLY REFLECTIONS", 24, static_cast<int>(redSectionTop - 20.0f), 450, 18, juce::Justification::centredLeft);

        // Assignable Controls Title
        g.drawText("ASSIGNABLE CONTROLS 1\xe2\x80\x93 6", 24, static_cast<int>(assignSectionTop - 18.0f), 300, 16, juce::Justification::centredLeft);

        // Footer separator line
        g.setColour(juce::Colour(0xff1f2735));
        g.drawLine(20.0f, bounds.getBottom() - 44.0f, bounds.getRight() - 20.0f, bounds.getBottom() - 44.0f, 1.0f);
    }

    void resized() override {
        const auto bounds = getLocalBounds();
        const int w = bounds.getWidth();
        const int h = bounds.getHeight();

        if (h < 380) {
            // Widescreen Embedded Console Mode (Fit directly in 1340 x 255px touch area)
            bypassButton.setBounds(w - 90, 4, 80, 22);
            const int tabGroupW = 280;
            const int tabGroupX = w - tabGroupW - 100;
            const int tabItemW = tabGroupW / 4;
            for (int i = 0; i < 4; ++i) {
                tabButtons[static_cast<size_t>(i)].setBounds(tabGroupX + i * tabItemW, 4, tabItemW - 2, 22);
            }

            const int cardAreaY = 28;
            const int cardH = juce::jlimit(52, 70, (h - cardAreaY - 32) / 2);
            const int meterStripW = 76;
            const int rightMeterX = w - meterStripW - 10;
            const int availCardW = rightMeterX - 24;

            const int blueW = static_cast<int>(availCardW * 0.38f);
            const int redW  = static_cast<int>(availCardW * 0.38f);
            const int assnW = availCardW - blueW - redW - 16;

            const int cardWBlue = (blueW - 12) / 3;
            const int cardWRed  = (redW - 12) / 3;

            blueSectionTop = static_cast<float>(cardAreaY);
            redSectionTop  = static_cast<float>(cardAreaY);
            assignSectionTop = static_cast<float>(cardAreaY);

            // Blue Section (Reverb Tail)
            int bx = 12;
            int by = cardAreaY + 14;
            cardDecay->setBounds(bx, by, cardWBlue, cardH);
            cardHiCut->setBounds(bx + cardWBlue + 6, by, cardWBlue, cardH);
            cardReverbSize->setBounds(bx + (cardWBlue + 6) * 2, by, cardWBlue, cardH);

            by += cardH + 4;
            cardPreDelay->setBounds(bx, by, cardWBlue, cardH);
            cardReverbDelay->setBounds(bx + cardWBlue + 6, by, cardWBlue, cardH);
            cardReverbWidth->setBounds(bx + (cardWBlue + 6) * 2, by, cardWBlue, cardH);

            // Red Section (Early Reflections)
            int rx = bx + blueW + 8;
            int ry = cardAreaY + 14;
            cardEarlyDecrease->setBounds(rx, ry, cardWRed, cardH);
            cardHiColor->setBounds(rx + cardWRed + 6, ry, cardWRed, cardH);
            cardEarlyStart->setBounds(rx + (cardWRed + 6) * 2, ry, cardWRed, cardH);

            ry += cardH + 4;
            cardEarlyStop->setBounds(rx, ry, cardWRed, cardH);
            cardLoColor->setBounds(rx + cardWRed + 6, ry, cardWRed, cardH);
            cardEarlyGain->setBounds(rx + (cardWRed + 6) * 2, ry, cardWRed, cardH);

            // Assignable pills (2 cols x 3 rows)
            int ax = rx + redW + 8;
            int ay = cardAreaY + 14;
            const int pillW = (assnW - 6) / 2;
            const int pillH = (cardH * 2 + 4 - 8) / 3;
            pillAssign[0]->setBounds(ax, ay, pillW, pillH);
            pillAssign[1]->setBounds(ax + pillW + 6, ay, pillW, pillH);
            ay += pillH + 4;
            pillAssign[2]->setBounds(ax, ay, pillW, pillH);
            pillAssign[3]->setBounds(ax + pillW + 6, ay, pillW, pillH);
            ay += pillH + 4;
            pillAssign[4]->setBounds(ax, ay, pillW, pillH);
            pillAssign[5]->setBounds(ax + pillW + 6, ay, pillW, pillH);

            // Meters
            inMeter.setBounds(rightMeterX, cardAreaY + 14, 32, cardH * 2 + 4);
            outMeter.setBounds(rightMeterX + 36, cardAreaY + 14, 32, cardH * 2 + 4);

            // Footer Preset bar
            const int footerY = h - 26;
            prevPresetBtn.setBounds(12, footerY, 24, 22);
            presetButton.setBounds(38, footerY, 260, 22);
            nextPresetBtn.setBounds(300, footerY, 24, 22);
            settingsBtn.setBounds(w - 32, footerY, 24, 22);
            infoBtn.setBounds(w - 58, footerY, 24, 22);
            favoriteBtn.setBounds(w - 84, footerY, 24, 22);
            return;
        }

        // 1. Top Bar Elements
        bypassButton.setBounds(w - 110, 10, 86, 26);

        // 2. Tab Navigation Container (MAIN / SETUP / COLOUR / GLOSS)
        const int tabGroupW = 310;
        const int tabGroupX = w - tabGroupW - 24;
        const int tabItemW = tabGroupW / 4;
        for (int i = 0; i < 4; ++i) {
            tabButtons[static_cast<size_t>(i)].setBounds(tabGroupX + i * tabItemW, 44, tabItemW - 2, 32);
        }

        // Available area for cards (accounting for right meter strip)
        const int meterStripW = 86;
        const int cardsAreaW = w - meterStripW - 48;
        const int cardW = (cardsAreaW - 16) / 3;
        const int cardH = static_cast<int>(std::clamp(h * 0.125f, 72.0f, 86.0f));

        // 3. Blue Control Section
        blueSectionTop = 104.0f;
        int bx = 24;
        int by = static_cast<int>(blueSectionTop);

        cardDecay->setBounds(bx, by, cardW, cardH);
        cardHiCut->setBounds(bx + cardW + 8, by, cardW, cardH);
        cardReverbSize->setBounds(bx + (cardW + 8) * 2, by, cardW, cardH);

        by += cardH + 8;
        cardPreDelay->setBounds(bx, by, cardW, cardH);
        cardReverbDelay->setBounds(bx + cardW + 8, by, cardW, cardH);
        cardReverbWidth->setBounds(bx + (cardW + 8) * 2, by, cardW, cardH);

        // 4. Red Control Section
        redSectionTop = static_cast<float>(by + cardH + 28);
        int rx = 24;
        int ry = static_cast<int>(redSectionTop);

        cardEarlyDecrease->setBounds(rx, ry, cardW, cardH);
        cardHiColor->setBounds(rx + cardW + 8, ry, cardW, cardH);
        cardEarlyStart->setBounds(rx + (cardW + 8) * 2, ry, cardW, cardH);

        ry += cardH + 8;
        cardEarlyStop->setBounds(rx, ry, cardW, cardH);
        cardLoColor->setBounds(rx + cardW + 8, ry, cardW, cardH);
        cardEarlyGain->setBounds(rx + (cardW + 8) * 2, ry, cardW, cardH);

        // 5. Assignable Controls 1–6 (Pill strips)
        assignSectionTop = static_cast<float>(ry + cardH + 24);
        int ax = 24;
        int ay = static_cast<int>(assignSectionTop);
        const int pillW = cardW;
        const int pillH = 25;

        pillAssign[0]->setBounds(ax, ay, pillW, pillH);
        pillAssign[1]->setBounds(ax + pillW + 8, ay, pillW, pillH);
        pillAssign[2]->setBounds(ax + (pillW + 8) * 2, ay, pillW, pillH);

        ay += pillH + 6;
        pillAssign[3]->setBounds(ax, ay, pillW, pillH);
        pillAssign[4]->setBounds(ax + pillW + 8, ay, pillW, pillH);
        pillAssign[5]->setBounds(ax + (pillW + 8) * 2, ay, pillW, pillH);

        // 6. Right Side Meters
        const int meterX = w - meterStripW - 14;
        const int meterY = static_cast<int>(blueSectionTop);
        const int meterH = (ry + cardH) - meterY;
        inMeter.setBounds(meterX, meterY, 36, meterH);
        outMeter.setBounds(meterX + 40, meterY, 36, meterH);

        // 7. Footer Strip
        const int footerY = h - 38;
        prevPresetBtn.setBounds(20, footerY, 26, 26);
        presetButton.setBounds(50, footerY, 320, 26);
        nextPresetBtn.setBounds(374, footerY, 26, 26);

        settingsBtn.setBounds(w - 44, footerY, 28, 26);
        infoBtn.setBounds(w - 76, footerY, 28, 26);
        favoriteBtn.setBounds(w - 108, footerY, 28, 26);
    }

private:
    void updateTabDisplay() {
        for (int i = 0; i < 4; ++i) {
            const bool isCur = (static_cast<int>(activeTab) == i);
            tabButtons[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId,
                isCur ? juce::Colour(0xff2d3748) : juce::Colour(0xff4a5360));
            tabButtons[static_cast<size_t>(i)].setColour(juce::TextButton::textColourOffId,
                isCur ? juce::Colour(0xff58a6ff) : juce::Colour(0xfff0f6fc));
        }
    }

    void showPresetMenu() {
        juce::PopupMenu m;
        m.addSectionHeader("FACTORY PRESETS (VSS4 HD)");

        const juce::StringArray presets = {
            "001 \xc2\xb7 Halls \xc2\xb7 Vienna Hall",
            "002 \xc2\xb7 Halls \xc2\xb7 Concert Hall",
            "023 \xc2\xb7 Halls \xc2\xb7 VSS4HD Large Hall",
            "024 \xc2\xb7 Halls \xc2\xb7 Deep Symphony Hall",
            "030 \xc2\xb7 Rooms \xc2\xb7 Studio Wood Room",
            "031 \xc2\xb7 Rooms \xc2\xb7 Drum Room Studio",
            "045 \xc2\xb7 Plates \xc2\xb7 EMT 140 Vintage Plate",
            "050 \xc2\xb7 Churches \xc2\xb7 Sacred Cathedral",
            "060 \xc2\xb7 Ambience \xc2\xb7 Wide AmbiScene"
        };

        for (int i = 0; i < presets.size(); ++i) {
            m.addItem(i + 1, presets[i], true, i == currentPresetIndex);
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
            [this, presets](int result) {
                if (result > 0 && result <= presets.size()) {
                    currentPresetIndex = result - 1;
                    applyPreset(currentPresetIndex, presets[currentPresetIndex]);
                }
            });
    }

    void cyclePreset(int delta) {
        const int numPresets = 9;
        currentPresetIndex = (currentPresetIndex + delta + numPresets) % numPresets;
        const juce::StringArray presets = {
            "001 \xc2\xb7 Halls \xc2\xb7 Vienna Hall",
            "002 \xc2\xb7 Halls \xc2\xb7 Concert Hall",
            "023 \xc2\xb7 Halls \xc2\xb7 VSS4HD Large Hall",
            "024 \xc2\xb7 Halls \xc2\xb7 Deep Symphony Hall",
            "030 \xc2\xb7 Rooms \xc2\xb7 Studio Wood Room",
            "031 \xc2\xb7 Rooms \xc2\xb7 Drum Room Studio",
            "045 \xc2\xb7 Plates \xc2\xb7 EMT 140 Vintage Plate",
            "050 \xc2\xb7 Churches \xc2\xb7 Sacred Cathedral",
            "060 \xc2\xb7 Ambience \xc2\xb7 Wide AmbiScene"
        };
        applyPreset(currentPresetIndex, presets[currentPresetIndex]);
    }

    void applyPreset(int index, const juce::String& name) {
        presetButton.setButtonText(name);

        switch (index) {
            case 0: // Vienna Hall
                cardDecay->setValue(2.7, juce::sendNotificationSync);
                cardHiCut->setValue(2.17, juce::sendNotificationSync);
                cardReverbSize->setValue(14.0, juce::sendNotificationSync);
                cardPreDelay->setValue(0.0, juce::sendNotificationSync);
                cardEarlyDecrease->setValue(3.0, juce::sendNotificationSync);
                break;
            case 1: // Concert Hall
                cardDecay->setValue(2.2, juce::sendNotificationSync);
                cardHiCut->setValue(4.50, juce::sendNotificationSync);
                cardReverbSize->setValue(18.0, juce::sendNotificationSync);
                cardPreDelay->setValue(25.0, juce::sendNotificationSync);
                cardEarlyDecrease->setValue(10.0, juce::sendNotificationSync);
                break;
            case 2: // VSS4HD Large Hall (default on screenshot)
                cardDecay->setValue(2.8, juce::sendNotificationSync);
                cardHiCut->setValue(2.17, juce::sendNotificationSync);
                cardReverbSize->setValue(14.0, juce::sendNotificationSync);
                cardPreDelay->setValue(22.0, juce::sendNotificationSync);
                cardReverbDelay->setValue(0.0, juce::sendNotificationSync);
                cardReverbWidth->setValue(5.0, juce::sendNotificationSync);
                cardEarlyDecrease->setValue(3.0, juce::sendNotificationSync);
                cardHiColor->setValue(20.0, juce::sendNotificationSync);
                cardEarlyStart->setValue(20.0, juce::sendNotificationSync);
                cardEarlyStop->setValue(100.0, juce::sendNotificationSync);
                cardLoColor->setValue(12.0, juce::sendNotificationSync);
                cardEarlyGain->setValue(-1.0, juce::sendNotificationSync);
                break;
            case 4: // Studio Wood Room
                cardDecay->setValue(0.85, juce::sendNotificationSync);
                cardHiCut->setValue(6.20, juce::sendNotificationSync);
                cardReverbSize->setValue(6.0, juce::sendNotificationSync);
                cardPreDelay->setValue(12.0, juce::sendNotificationSync);
                cardEarlyDecrease->setValue(25.0, juce::sendNotificationSync);
                break;
            case 6: // EMT 140 Plate
                cardDecay->setValue(1.9, juce::sendNotificationSync);
                cardHiCut->setValue(8.50, juce::sendNotificationSync);
                cardReverbSize->setValue(10.0, juce::sendNotificationSync);
                cardPreDelay->setValue(10.0, juce::sendNotificationSync);
                cardEarlyDecrease->setValue(15.0, juce::sendNotificationSync);
                break;
            case 7: // Sacred Cathedral
                cardDecay->setValue(4.8, juce::sendNotificationSync);
                cardHiCut->setValue(3.20, juce::sendNotificationSync);
                cardReverbSize->setValue(24.0, juce::sendNotificationSync);
                cardPreDelay->setValue(45.0, juce::sendNotificationSync);
                cardEarlyDecrease->setValue(2.0, juce::sendNotificationSync);
                break;
            default:
                break;
        }
    }

    void showSettingsMenu() {
        juce::PopupMenu m;
        m.addItem(1, "A/B Compare Snapshot (Store A)");
        m.addItem(2, "Recall Snapshot A");
        m.addItem(3, "Reset All Parameters to Defaults");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsBtn),
            [this](int r) {
                if (r == 3) {
                    applyPreset(2, "023 \xc2\xb7 Halls \xc2\xb7 VSS4HD Large Hall");
                }
            });
    }

    void timerCallback() override {
        // Update Live Stereo Peak Meters
        inMeter.setLevels(engine.getMeterInL(), engine.getMeterInR());
        outMeter.setLevels(engine.getMeterOutL(), engine.getMeterOutR());
    }

    nova::graviton::VSS4ReverbEngine& engine;
    PageTab activeTab = PageTab::Main;
    bool isBypassed = false;
    int currentPresetIndex = 2; // 023 VSS4HD Large Hall

    float blueSectionTop = 104.0f;
    float redSectionTop = 270.0f;
    float assignSectionTop = 460.0f;

    // Header buttons
    juce::TextButton bypassButton;
    juce::TextButton tabButtons[4];

    // Blue Control Section Cards
    std::unique_ptr<VSS4KnobCard> cardDecay;
    std::unique_ptr<VSS4KnobCard> cardHiCut;
    std::unique_ptr<VSS4KnobCard> cardReverbSize;
    std::unique_ptr<VSS4KnobCard> cardPreDelay;
    std::unique_ptr<VSS4KnobCard> cardReverbDelay;
    std::unique_ptr<VSS4KnobCard> cardReverbWidth;

    // Red Control Section Cards
    std::unique_ptr<VSS4KnobCard> cardEarlyDecrease;
    std::unique_ptr<VSS4KnobCard> cardHiColor;
    std::unique_ptr<VSS4KnobCard> cardEarlyStart;
    std::unique_ptr<VSS4KnobCard> cardEarlyStop;
    std::unique_ptr<VSS4KnobCard> cardLoColor;
    std::unique_ptr<VSS4KnobCard> cardEarlyGain;

    // Assignable Controls (1–6)
    std::unique_ptr<VSS4AssignablePill> pillAssign[6];

    // Meters
    VSS4StereoMeter inMeter;
    VSS4StereoMeter outMeter;

    // Footer Preset Navigation
    juce::TextButton presetButton;
    juce::TextButton prevPresetBtn;
    juce::TextButton nextPresetBtn;
    juce::TextButton favoriteBtn;
    juce::TextButton infoBtn;
    juce::TextButton settingsBtn;
};

} // namespace nova
