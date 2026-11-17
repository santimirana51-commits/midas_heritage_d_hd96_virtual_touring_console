#pragma once

#include "Effects/GravitonDVR2Reverb.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace nova {

// ==============================================================================
// 1. TC Electronic Hardware Vertical Fader & Value Display Box
// ==============================================================================
class TCSystem6000Fader final : public juce::Component {
public:
    TCSystem6000Fader(const juce::String& paramName,
                      double minV, double maxV, double defaultV,
                      const juce::String& unitSuffix,
                      int decimalPlaces = 2,
                      bool isTimeScale = false)
        : title(paramName),
          minValue(minV),
          maxValue(maxV),
          defaultValue(defaultV),
          currentValue(defaultV),
          unit(unitSuffix),
          decimals(decimalPlaces),
          timeScale(isTimeScale) {
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
    double getMinValue() const noexcept { return minValue; }
    double getMaxValue() const noexcept { return maxValue; }

    void setFocused(bool focused) noexcept {
        if (isFocused != focused) {
            isFocused = focused;
            repaint();
        }
    }

    void setOverrideText(const juce::String& txt) {
        if (overrideText != txt) {
            overrideText = txt;
            repaint();
        }
    }

    std::function<void(double)> onValueChanged;
    std::function<void()> onFaderFocused;

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // 1. Outer Box Container
        g.setColour(isFocused ? juce::Colour(0xff1c2331) : juce::Colour(0xff181a1f));
        g.fillRoundedRectangle(bounds, 3.5f);
        g.setColour(isFocused ? juce::Colour(0xff38bdf8) : juce::Colour(0xff2d3139));
        g.drawRoundedRectangle(bounds, 3.5f, isFocused ? 1.4f : 1.0f);

        // 2. Title Label (Top)
        g.setColour(isFocused ? juce::Colour(0xff7dd3fc) : juce::Colour(0xff8b949e));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(title, 2, 3, static_cast<int>(bounds.getWidth() - 4), 14, juce::Justification::centred, false);

        // 3. Numeric Readout Box
        const auto valBox = juce::Rectangle<float>(bounds.getX() + 4.0f, bounds.getY() + 18.0f, bounds.getWidth() - 8.0f, 18.0f);
        g.setColour(juce::Colour(0xff0d1117));
        g.fillRoundedRectangle(valBox, 2.5f);
        g.setColour(isFocused ? juce::Colour(0xff0284c7) : juce::Colour(0xff30363d));
        g.drawRoundedRectangle(valBox, 2.5f, 1.0f);

        juce::String valStr;
        if (overrideText.isNotEmpty()) {
            valStr = overrideText;
        } else if (unit == "Off" && currentValue <= -90.0) {
            valStr = "Off";
        } else if (unit == "dB" && currentValue <= -90.0) {
            valStr = "Off";
        } else if (timeScale && currentValue >= 1000.0) {
            valStr = juce::String(currentValue * 0.001, 2) + " s";
        } else if (decimals == 0) {
            valStr = juce::String(static_cast<int>(std::round(currentValue))) + (unit.isNotEmpty() ? (" " + unit) : "");
        } else {
            valStr = juce::String(currentValue, decimals) + (unit.isNotEmpty() ? (" " + unit) : "");
        }

        g.setColour(juce::Colour(0xfff0f6fc));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText(valStr, valBox, juce::Justification::centred, false);

        // 4. Vertical Fader Track
        const float trackW = 4.0f;
        const float trackTop = valBox.getBottom() + 6.0f;
        const float trackBottom = bounds.getBottom() - 8.0f;
        const float trackH = std::max(10.0f, trackBottom - trackTop);
        const float trackX = bounds.getCentreX() - trackW * 0.5f;

        g.setColour(juce::Colour(0xff0b0d10));
        g.fillRoundedRectangle(trackX, trackTop, trackW, trackH, 2.0f);
        g.setColour(juce::Colour(0xff21262d));
        g.drawRoundedRectangle(trackX, trackTop, trackW, trackH, 2.0f, 1.0f);

        // Normalized Position [0.0 = bottom, 1.0 = top]
        const float norm = static_cast<float>((currentValue - minValue) / std::max(1e-4, maxValue - minValue));
        const float thumbY = trackBottom - (norm * trackH);

        // Active fill bar inside track
        const float fillH = trackBottom - thumbY;
        if (fillH > 1.0f) {
            g.setColour(juce::Colour(0xff0284c7).withAlpha(0.55f));
            g.fillRoundedRectangle(trackX, thumbY, trackW, fillH, 1.5f);
        }

        // Metallic Fader Thumb
        const float thumbW = bounds.getWidth() - 16.0f;
        const float thumbH = 14.0f;
        const auto thumbRect = juce::Rectangle<float>(bounds.getCentreX() - thumbW * 0.5f, thumbY - thumbH * 0.5f, thumbW, thumbH);

        juce::ColourGradient thumbGrad(juce::Colour(0xff484f58), thumbRect.getCentreX(), thumbRect.getY(),
                                       juce::Colour(0xff21262d), thumbRect.getCentreX(), thumbRect.getBottom(), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumbRect, 2.0f);
        g.setColour(isFocused ? juce::Colour(0xff38bdf8) : juce::Colour(0xff6e7681));
        g.drawRoundedRectangle(thumbRect, 2.0f, 1.0f);

        // Center notch line on thumb
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawHorizontalLine(static_cast<int>(thumbRect.getCentreY()), thumbRect.getX() + 3.0f, thumbRect.getRight() - 3.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (onFaderFocused) onFaderFocused();
        dragStartValue = currentValue;
        dragStartPos = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float deltaY = dragStartPos.y - e.position.y;
        const double range = maxValue - minValue;
        const double deltaVal = (static_cast<double>(deltaY) / 120.0) * range;
        setValue(dragStartValue + deltaVal, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (onFaderFocused) onFaderFocused();
        setValue(defaultValue, juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        if (onFaderFocused) onFaderFocused();
        const double range = maxValue - minValue;
        setValue(currentValue + range * 0.02 * wheel.deltaY, juce::sendNotificationSync);
    }

private:
    juce::String title;
    juce::String overrideText;
    double minValue = 0.0;
    double maxValue = 1.0;
    double defaultValue = 0.5;
    double currentValue = 0.5;
    double dragStartValue = 0.5;
    juce::Point<float> dragStartPos;
    juce::String unit;
    int decimals = 2;
    bool timeScale = false;
    bool isFocused = false;
};

// ==============================================================================
// 2. TC Electronic System 6000 Multi-Segment LED Meter
// ==============================================================================
class TCSystem6000Meter final : public juce::Component {
public:
    TCSystem6000Meter(const juce::String& meterTitle, bool isOutput = false)
        : title(meterTitle), isOut(isOutput) {}

    void setLevels(float peakL, float peakR, bool overloadFlag) {
        levelL = std::clamp(peakL, 0.0f, 2.0f);
        levelR = std::clamp(peakR, 0.0f, 2.0f);
        overload = overloadFlag;
        repaint();
    }

    std::function<void()> onResetOverload;

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // Container
        g.setColour(juce::Colour(0xff121418));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(juce::Colour(0xff252a32));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Header Title
        g.setColour(juce::Colour(0xff8b949e));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(title, bounds.getX(), bounds.getY() + 2.0f, bounds.getWidth(), 14.0f, juce::Justification::centred, false);

        // Overload Badge (OVR)
        const auto ovrBox = juce::Rectangle<float>(bounds.getX() + 4.0f, bounds.getY() + 16.0f, bounds.getWidth() - 8.0f, 13.0f);
        g.setColour(overload ? juce::Colour(0xffef4444) : juce::Colour(0xff1f242c));
        g.fillRoundedRectangle(ovrBox, 2.0f);
        g.setColour(overload ? juce::Colours::white : juce::Colour(0xff484f58));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText("OVR", ovrBox, juce::Justification::centred, false);

        // Meter Bars Area
        const float barTop = ovrBox.getBottom() + 3.0f;
        const float barH = bounds.getBottom() - barTop - 4.0f;
        const float barW = 8.0f;
        const float gap = 4.0f;
        const float totalW = (barW * 2.0f) + gap;
        const float startX = bounds.getCentreX() - totalW * 0.5f;

        const auto barRectL = juce::Rectangle<float>(startX, barTop, barW, barH);
        const auto barRectR = juce::Rectangle<float>(startX + barW + gap, barTop, barW, barH);

        auto renderSegments = [&](juce::Rectangle<float> r, float peakVal) {
            constexpr int numSegments = 16;
            const float segH = (r.getHeight() - (numSegments - 1) * 1.5f) / numSegments;

            const float db = (peakVal > 1e-4f) ? 20.0f * std::log10(peakVal) : -60.0f;
            const float norm = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
            const int activeSegs = static_cast<int>(std::round(norm * numSegments));

            for (int i = 0; i < numSegments; ++i) {
                const int segIdx = (numSegments - 1 - i); // bottom to top
                const float y = r.getBottom() - (segIdx + 1) * (segH + 1.5f);
                const auto segRect = juce::Rectangle<float>(r.getX(), y, r.getWidth(), segH);

                const bool isActive = (segIdx < activeSegs);
                juce::Colour segCol;
                if (segIdx >= 14) segCol = juce::Colour(0xffef4444);      // Red (top)
                else if (segIdx >= 10) segCol = juce::Colour(0xfff59e0b); // Amber
                else segCol = juce::Colour(0xff10b981);                   // Green

                g.setColour(isActive ? segCol : segCol.withAlpha(0.15f));
                g.fillRect(segRect);
            }
        };

        renderSegments(barRectL, levelL);
        renderSegments(barRectR, levelR);
    }

    void mouseDown(const juce::MouseEvent&) override {
        if (onResetOverload) onResetOverload();
    }

private:
    juce::String title;
    bool isOut = false;
    float levelL = 0.0f;
    float levelR = 0.0f;
    bool overload = false;
};

// ==============================================================================
// 3. Main DVR2 Reverb Panel Component
// ==============================================================================
class DVR2Panel final : public juce::Component, private juce::Timer {
public:
    explicit DVR2Panel(nova::graviton::DVR2NonLinearReverb& dvrEngine)
        : engine(&dvrEngine),
          inMeter("Input", false),
          outMeter("Output", true)
    {
        // ── Top Bar: Preset Bank & Selector ──
        bankCombo.addItem("01 \xc2\xb7 Vocal", 1);
        bankCombo.addItem("02 \xc2\xb7 Room & Hall", 2);
        bankCombo.addItem("03 \xc2\xb7 Strings & Piano", 3);
        bankCombo.addItem("04 \xc2\xb7 Drums & Perc", 4);
        bankCombo.addItem("05 \xc2\xb7 Special", 5);
        bankCombo.setSelectedId(1, juce::dontSendNotification);
        bankCombo.onChange = [this] {
            const int bankIdx = bankCombo.getSelectedId() - 1;
            populatePresetsForBank(bankIdx, true);
        };
        addAndMakeVisible(bankCombo);

        presetCombo.onChange = [this] {
            const int selId = presetCombo.getSelectedId();
            const auto& allPresets = nova::graviton::DVR2NonLinearReverb::getFactoryPresets();
            if (selId >= 1 && selId <= static_cast<int>(allPresets.size())) {
                engine->loadPresetByIndex(selId - 1);
                updateAllUiFromEngine();
            }
        };
        addAndMakeVisible(presetCombo);

        // Prev / Next Preset Buttons
        prevPresetBtn.setButtonText("<");
        prevPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        prevPresetBtn.onClick = [this] { stepPreset(-1); };
        addAndMakeVisible(prevPresetBtn);

        nextPresetBtn.setButtonText(">");
        nextPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        nextPresetBtn.onClick = [this] { stepPreset(1); };
        addAndMakeVisible(nextPresetBtn);

        // System 6000 Top Action Buttons
        presetBtn.setButtonText("PRESET");
        presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible(presetBtn);

        tabE1.setButtonText("Main");
        tabE2.setButtonText("Vintage");
        tabE3.setButtonText("Levels");
        tabE4.setButtonText("Assign");
        int tabIdx = 0;
        for (auto* b : { &tabE1, &tabE2, &tabE3, &tabE4 }) {
            b->setClickingTogglesState(true);
            b->setRadioGroupId(6001, juce::dontSendNotification);
            b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f242c));
            b->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0284c7));
            const int capturedIdx = tabIdx++;
            b->onClick = [this, capturedIdx] {
                setActiveSection(capturedIdx, true);
            };
            addAndMakeVisible(*b);
        }
        tabE1.setToggleState(true, juce::dontSendNotification);

        // Kill Dry Toggle Button (for Aux Send vs Channel Insert routing)
        killDryBtn.setButtonText("KILL DRY: OFF");
        killDryBtn.setClickingTogglesState(true);
        killDryBtn.setToggleState(engine->getKillDry(), juce::dontSendNotification);
        killDryBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        killDryBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
        killDryBtn.onClick = [this] {
            engine->setKillDry(killDryBtn.getToggleState());
            updateAllUiFromEngine();
        };
        addAndMakeVisible(killDryBtn);

        powerBtn.setButtonText("POWER");
        powerBtn.setClickingTogglesState(true);
        powerBtn.setToggleState(!engine->getBypass(), juce::dontSendNotification);
        powerBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffda3633));
        powerBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2ea043));
        powerBtn.onClick = [this] {
            engine->setBypass(!powerBtn.getToggleState());
            powerBtn.setButtonText(powerBtn.getToggleState() ? "POWER" : "BYPASS");
            repaint();
        };
        addAndMakeVisible(powerBtn);

        // ── 1. Main Parameters (4 Faders) ──
        faderPreDelay = std::make_unique<TCSystem6000Fader>("Pre Delay", 0.0, 500.0, 35.0, "ms", 0, true);
        faderPreDelay->onFaderFocused = [this] { focusParameter(0, 0); };
        faderPreDelay->onValueChanged = [this](double v) {
            engine->setPreDelay(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderPreDelay);

        faderDecay = std::make_unique<TCSystem6000Fader>("Decay", 0.2, 20.0, 2.2, "s", 2);
        faderDecay->onFaderFocused = [this] { focusParameter(0, 1); };
        faderDecay->onValueChanged = [this](double v) {
            engine->setDecay(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderDecay);

        faderLoDecay = std::make_unique<TCSystem6000Fader>("Lo Decay", 0.1, 2.5, 0.85, "x", 2);
        faderLoDecay->onFaderFocused = [this] { focusParameter(0, 2); };
        faderLoDecay->onValueChanged = [this](double v) {
            engine->setLoDecay(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderLoDecay);

        faderHiDecay = std::make_unique<TCSystem6000Fader>("Hi Decay", 0.1, 2.0, 0.80, "x", 2);
        faderHiDecay->onFaderFocused = [this] { focusParameter(0, 3); };
        faderHiDecay->onValueChanged = [this](double v) {
            engine->setHiDecay(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderHiDecay);

        // ── 2. Vintage Section (EMT 250 Character) ──
        faderHiCut = std::make_unique<TCSystem6000Fader>("Hi Cut", 1000.0, 18000.0, 8000.0, "Hz", 0);
        faderHiCut->onFaderFocused = [this] { focusParameter(1, 4); };
        faderHiCut->onValueChanged = [this](double v) {
            engine->setHiCut(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderHiCut);

        faderHiQScale = std::make_unique<TCSystem6000Fader>("Hi QScale", 0.5, 2.5, 1.4, "", 2);
        faderHiQScale->onFaderFocused = [this] { focusParameter(1, 4); };
        faderHiQScale->onValueChanged = [this](double v) {
            engine->setHiQScale(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderHiQScale);

        faderModulation = std::make_unique<TCSystem6000Fader>("Modulation", 0.0, 200.0, 100.0, "%", 0);
        faderModulation->onFaderFocused = [this] { focusParameter(1, 5); };
        faderModulation->onValueChanged = [this](double v) {
            engine->setModulation(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderModulation);

        // Vintage Buttons
        btnInputTrans.setButtonText("Input Trans");
        btnInputTrans.setClickingTogglesState(true);
        btnInputTrans.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        btnInputTrans.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
        btnInputTrans.onClick = [this] {
            setActiveSection(1, false);
            engine->setInputTrans(btnInputTrans.getToggleState());
            syncAssignBar();
        };
        addAndMakeVisible(btnInputTrans);

        btnTrimLoFreq.setButtonText("Trim Lo Freq");
        btnTrimLoFreq.setClickingTogglesState(true);
        btnTrimLoFreq.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        btnTrimLoFreq.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
        btnTrimLoFreq.onClick = [this] {
            setActiveSection(1, false);
            engine->setTrimLoFreq(btnTrimLoFreq.getToggleState());
            syncAssignBar();
        };
        addAndMakeVisible(btnTrimLoFreq);

        btnLoRes.setButtonText("Lo Res (12-bit)");
        btnLoRes.setClickingTogglesState(true);
        btnLoRes.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        btnLoRes.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0284c7));
        btnLoRes.onClick = [this] {
            setActiveSection(1, false);
            engine->setLoRes(btnLoRes.getToggleState());
            syncAssignBar();
        };
        addAndMakeVisible(btnLoRes);

        btnVintageReset.setButtonText("Vintage Reset");
        btnVintageReset.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30363d));
        btnVintageReset.onClick = [this] {
            setActiveSection(1, false);
            engine->vintageReset();
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnVintageReset);

        // ── 3. Levels Section (In, Wet, Dry) ──
        faderInLevel = std::make_unique<TCSystem6000Fader>("In Level", -100.0, 12.0, 0.0, "dB", 1);
        faderInLevel->onFaderFocused = [this] { focusParameter(2, 6); };
        faderInLevel->onValueChanged = [this](double v) {
            engine->setInLevel(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderInLevel);

        faderWetLevel = std::make_unique<TCSystem6000Fader>("Wet Level", -100.0, 6.0, -3.5, "Off", 1);
        faderWetLevel->onFaderFocused = [this] { focusParameter(2, 7); };
        faderWetLevel->onValueChanged = [this](double v) {
            engine->setWetLevel(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderWetLevel);

        faderDryLevel = std::make_unique<TCSystem6000Fader>("Dry Level", -100.0, 0.0, 0.0, "Off", 1);
        faderDryLevel->onFaderFocused = [this] { focusParameter(2, 7); };
        faderDryLevel->onValueChanged = [this](double v) {
            if (engine->getKillDry() && v > -90.0) {
                engine->setKillDry(false);
                killDryBtn.setToggleState(false, juce::dontSendNotification);
                killDryBtn.setButtonText("KILL DRY: OFF");
            }
            engine->setDryLevel(v);
            syncAssignBar();
        };
        addAndMakeVisible(*faderDryLevel);

        // ── 4. Meters Section ──
        inMeter.onResetOverload = [this] { engine->resetOverload(); };
        outMeter.onResetOverload = [this] { engine->resetOverload(); };
        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);

        populatePresetsForBank(0, false);
        updateAllUiFromEngine();

        startTimerHz(30);
        setSize(1120, 310);
    }

    ~DVR2Panel() override {
        stopTimer();
    }

    void setEngine(nova::graviton::DVR2NonLinearReverb& newEngine) {
        engine = &newEngine;
        updateAllUiFromEngine();
    }

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds();

        // 1. Main Hardware Chassis Background
        juce::ColourGradient bgGrad(juce::Colour(0xff101318), 0.0f, 0.0f,
                                    juce::Colour(0xff090b0e), 0.0f, static_cast<float>(b.getHeight()), false);
        g.setGradientFill(bgGrad);
        g.fillAll();

        // Outer Bezel
        g.setColour(juce::Colour(0xff2d333b));
        g.drawRect(b, 1);

        // 2. Top Navigation Strip Background
        g.setColour(juce::Colour(0xff161b22));
        g.fillRect(0, 0, b.getWidth(), 30);
        g.setColour(juce::Colour(0xff30363d));
        g.drawHorizontalLine(29, 0.0f, static_cast<float>(b.getWidth()));

        // 3. Section Group Frames (with active section highlight)
        auto drawGroupFrame = [&](juce::Rectangle<int> rect, const juce::String& title, juce::Colour accent, bool isSectionActive) {
            const auto rf = rect.toFloat();
            g.setColour(isSectionActive ? juce::Colour(0xff131820) : juce::Colour(0xff0d1014));
            g.fillRoundedRectangle(rf, 4.0f);
            g.setColour(isSectionActive ? accent : juce::Colour(0xff252a32));
            g.drawRoundedRectangle(rf, 4.0f, isSectionActive ? 1.6f : 1.0f);

            // Title bar
            const auto headerRect = juce::Rectangle<float>(rf.getX(), rf.getY(), rf.getWidth(), 18.0f);
            g.setColour(accent.withAlpha(isSectionActive ? 0.28f : 0.14f));
            g.fillRoundedRectangle(headerRect, 4.0f);
            g.setColour(accent);
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText(title, headerRect.reduced(6.0f, 0.0f), juce::Justification::centredLeft, false);
        };

        drawGroupFrame(mainBoxBounds,    "MAIN PARAMETERS (EMT 250 CORE)", juce::Colour(0xff38bdf8), activeSection == 0);
        drawGroupFrame(vintageBoxBounds, "VINTAGE EMT 250 SECTION",        juce::Colour(0xfff59e0b), activeSection == 1);
        drawGroupFrame(levelsBoxBounds,  "LEVELS",                         juce::Colour(0xff10b981), activeSection == 2);
        drawGroupFrame(metersBoxBounds,  "METERS",                         juce::Colour(0xffa855f7), activeSection == 2);

        // 4. Bottom Assign Bar (Interactive 8 Shortcut Cells — Drag/Scroll/Double-Click)
        g.setColour(activeSection == 3 ? juce::Colour(0xff17202b) : juce::Colour(0xff14171d));
        g.fillRoundedRectangle(assignBarBounds.toFloat(), 3.0f);
        g.setColour(activeSection == 3 ? juce::Colour(0xff38bdf8) : juce::Colour(0xff282e37));
        g.drawRoundedRectangle(assignBarBounds.toFloat(), 3.0f, activeSection == 3 ? 1.5f : 1.0f);

        const int numCells = 8;
        const float cellW = static_cast<float>(assignBarBounds.getWidth()) / numCells;
        for (int i = 0; i < numCells; ++i) {
            const float cx = assignBarBounds.getX() + i * cellW;
            const auto cRect = juce::Rectangle<float>(cx, static_cast<float>(assignBarBounds.getY()), cellW, static_cast<float>(assignBarBounds.getHeight()));

            const bool isCellFocused = (i == focusedAssignIndex);
            if (isCellFocused) {
                g.setColour(juce::Colour(0xff0284c7).withAlpha(0.22f));
                g.fillRoundedRectangle(cRect.reduced(1.5f), 2.5f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(cRect.reduced(1.0f), 2.5f, 1.2f);
            }

            if (i > 0) {
                g.setColour(juce::Colour(0xff252a32));
                g.drawVerticalLine(static_cast<int>(cx), cRect.getY() + 2.0f, cRect.getBottom() - 2.0f);
            }

            // Normalized progress bar at bottom of cell
            const float normBar = std::clamp(assignNorms[i], 0.0f, 1.0f);
            const auto barStrip = juce::Rectangle<float>(cRect.getX() + 4.0f, cRect.getBottom() - 3.0f, (cRect.getWidth() - 8.0f) * normBar, 2.0f);
            g.setColour(isCellFocused ? juce::Colour(0xff38bdf8) : juce::Colour(0xff0284c7).withAlpha(0.65f));
            g.fillRect(barStrip);

            g.setColour(isCellFocused ? juce::Colour(0xff7dd3fc) : juce::Colour(0xff8b949e));
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.drawText(assignLabels[i], cRect.withHeight(12.0f).translated(0.0f, 1.0f), juce::Justification::centred, false);

            g.setColour(juce::Colour(0xfff0f6fc));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(assignValues[i], cRect.withTrimmedTop(10.0f).withTrimmedBottom(2.0f), juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();
        if (assignBarBounds.contains(pos)) {
            const int cellIdx = std::clamp(static_cast<int>((pos.x - assignBarBounds.getX()) * 8 / std::max(1, assignBarBounds.getWidth())), 0, 7);
            focusedAssignIndex = cellIdx;
            assignDragStartNorm = assignNorms[cellIdx];
            assignDragStartY = e.position.y;
            assignDragStartX = e.position.x;

            // Map cell to section
            if (cellIdx <= 3) setActiveSection(0, false);
            else if (cellIdx <= 5) setActiveSection(1, false);
            else setActiveSection(2, false);
            updateFaderFocusHighlights();
            repaint();
            return;
        }

        if (mainBoxBounds.contains(pos)) setActiveSection(0, false);
        else if (vintageBoxBounds.contains(pos)) setActiveSection(1, false);
        else if (levelsBoxBounds.contains(pos) || metersBoxBounds.contains(pos)) setActiveSection(2, false);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (focusedAssignIndex < 0 || focusedAssignIndex >= 8) return;
        if (!assignBarBounds.contains(e.getMouseDownPosition())) return;

        const float dy = assignDragStartY - e.position.y;
        const float dx = e.position.x - assignDragStartX;
        const float deltaNorm = (dy + dx * 0.5f) / 120.0f;
        applyAssignCellNormalized(focusedAssignIndex, std::clamp(assignDragStartNorm + deltaNorm, 0.0f, 1.0f));
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();
        if (assignBarBounds.contains(pos)) {
            const int cellIdx = std::clamp(static_cast<int>((pos.x - assignBarBounds.getX()) * 8 / std::max(1, assignBarBounds.getWidth())), 0, 7);
            resetAssignCellToDefault(cellIdx);
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        const auto pos = e.getPosition();
        if (assignBarBounds.contains(pos)) {
            const int cellIdx = std::clamp(static_cast<int>((pos.x - assignBarBounds.getX()) * 8 / std::max(1, assignBarBounds.getWidth())), 0, 7);
            focusedAssignIndex = cellIdx;
            const float nextNorm = std::clamp(assignNorms[cellIdx] + wheel.deltaY * 0.03f, 0.0f, 1.0f);
            applyAssignCellNormalized(cellIdx, nextNorm);
        }
    }

    void resized() override {
        const int w = getWidth();
        const int h = getHeight();

        // ── Top Toolbar ──
        const int tbY = 4;
        int tx = 6;
        presetBtn.setBounds(tx, tbY, 58, 22); tx += 62;
        bankCombo.setBounds(tx, tbY, 122, 22); tx += 126;
        presetCombo.setBounds(tx, tbY, 172, 22); tx += 175;
        prevPresetBtn.setBounds(tx, tbY, 24, 22); tx += 26;
        nextPresetBtn.setBounds(tx, tbY, 24, 22); tx += 30;

        tabE1.setBounds(tx, tbY, 46, 22); tx += 48;
        tabE2.setBounds(tx, tbY, 54, 22); tx += 56;
        tabE3.setBounds(tx, tbY, 48, 22); tx += 50;
        tabE4.setBounds(tx, tbY, 50, 22); tx += 56;

        killDryBtn.setBounds(w - 178, tbY, 98, 22);
        powerBtn.setBounds(w - 74, tbY, 66, 22);

        // ── Main Body Compartments ──
        const int mainY = 30;
        const int assignH = 26;
        const int mainH = h - mainY - assignH - 6;

        // Proportional 4 Columns so it scales cleanly in any embedded console resolution
        const int availW = w - 27;
        const int mainW    = static_cast<int>(availW * 0.35f);
        const int vintageW = static_cast<int>(availW * 0.33f);
        const int levelsW  = static_cast<int>(availW * 0.21f);
        const int metersW  = availW - (mainW + vintageW + levelsW);

        int curX = 6;
        mainBoxBounds    = juce::Rectangle<int>(curX, mainY, mainW, mainH);    curX += mainW + 5;
        vintageBoxBounds = juce::Rectangle<int>(curX, mainY, vintageW, mainH); curX += vintageW + 5;
        levelsBoxBounds  = juce::Rectangle<int>(curX, mainY, levelsW, mainH);  curX += levelsW + 5;
        metersBoxBounds  = juce::Rectangle<int>(curX, mainY, metersW, mainH);

        // Assign Bar at bottom
        assignBarBounds = juce::Rectangle<int>(6, h - assignH - 3, w - 12, assignH);

        // ── Layout Main Parameters (4 Faders) ──
        const int mfw = (mainBoxBounds.getWidth() - 20) / 4;
        const int mfh = mainBoxBounds.getHeight() - 26;
        const int mfy = mainBoxBounds.getY() + 22;
        if (faderPreDelay) faderPreDelay->setBounds(mainBoxBounds.getX() + 6 + 0 * (mfw + 3), mfy, mfw, mfh);
        if (faderDecay)    faderDecay->setBounds(mainBoxBounds.getX() + 6 + 1 * (mfw + 3), mfy, mfw, mfh);
        if (faderLoDecay)  faderLoDecay->setBounds(mainBoxBounds.getX() + 6 + 2 * (mfw + 3), mfy, mfw, mfh);
        if (faderHiDecay)  faderHiDecay->setBounds(mainBoxBounds.getX() + 6 + 3 * (mfw + 3), mfy, mfw, mfh);

        // ── Layout Vintage Section (3 Faders + 4 Buttons) ──
        const int vfw = std::max(68, (vintageBoxBounds.getWidth() - 140) / 3);
        const int vfy = vintageBoxBounds.getY() + 22;
        const int vfh = vintageBoxBounds.getHeight() - 26;
        if (faderHiCut)      faderHiCut->setBounds(vintageBoxBounds.getX() + 6, vfy, vfw, vfh);
        if (faderHiQScale)   faderHiQScale->setBounds(vintageBoxBounds.getX() + 6 + vfw + 3, vfy, vfw, vfh);
        if (faderModulation) faderModulation->setBounds(vintageBoxBounds.getX() + 6 + (vfw + 3) * 2, vfy, vfw, vfh);

        // 4 Vintage Buttons column
        const int vbx = vintageBoxBounds.getX() + 6 + (vfw + 3) * 3 + 4;
        const int vbw = std::max(80, vintageBoxBounds.getRight() - vbx - 6);
        const int vbh = (vfh - 9) / 4;
        btnInputTrans.setBounds(vbx, vfy, vbw, vbh);
        btnTrimLoFreq.setBounds(vbx, vfy + vbh + 3, vbw, vbh);
        btnLoRes.setBounds(vbx, vfy + (vbh + 3) * 2, vbw, vbh);
        btnVintageReset.setBounds(vbx, vfy + (vbh + 3) * 3, vbw, vbh);

        // ── Layout Levels Section (3 Faders) ──
        const int lfw = (levelsBoxBounds.getWidth() - 16) / 3;
        const int lfy = levelsBoxBounds.getY() + 22;
        const int lfh = levelsBoxBounds.getHeight() - 26;
        if (faderInLevel)  faderInLevel->setBounds(levelsBoxBounds.getX() + 6 + 0 * (lfw + 3), lfy, lfw, lfh);
        if (faderWetLevel) faderWetLevel->setBounds(levelsBoxBounds.getX() + 6 + 1 * (lfw + 3), lfy, lfw, lfh);
        if (faderDryLevel) faderDryLevel->setBounds(levelsBoxBounds.getX() + 6 + 2 * (lfw + 3), lfy, lfw, lfh);

        // ── Layout Meters ──
        const int mtrW = (metersBoxBounds.getWidth() - 14) / 2;
        const int mtrH = metersBoxBounds.getHeight() - 26;
        const int mtrY = metersBoxBounds.getY() + 22;
        inMeter.setBounds(metersBoxBounds.getX() + 5, mtrY, mtrW, mtrH);
        outMeter.setBounds(metersBoxBounds.getX() + 5 + mtrW + 4, mtrY, mtrW, mtrH);
    }

private:
    static int getBankIndexForName(const char* bankName) noexcept {
        if (!bankName) return 0;
        if (std::strstr(bankName, "01") != nullptr) return 0;
        if (std::strstr(bankName, "02") != nullptr) return 1;
        if (std::strstr(bankName, "03") != nullptr) return 2;
        if (std::strstr(bankName, "04") != nullptr) return 3;
        if (std::strstr(bankName, "05") != nullptr) return 4;
        return 0;
    }

    void setActiveSection(int secIdx, bool fromTabClick) {
        activeSection = std::clamp(secIdx, 0, 3);
        tabE1.setToggleState(activeSection == 0, juce::dontSendNotification);
        tabE2.setToggleState(activeSection == 1, juce::dontSendNotification);
        tabE3.setToggleState(activeSection == 2, juce::dontSendNotification);
        tabE4.setToggleState(activeSection == 3, juce::dontSendNotification);

        if (fromTabClick) {
            if (activeSection == 0) focusedAssignIndex = 1;      // Decay
            else if (activeSection == 1) focusedAssignIndex = 4; // Hi Cut
            else if (activeSection == 2) focusedAssignIndex = 7; // Wet Level
            else if (activeSection == 3) {
                assignPageAlt = !assignPageAlt;
                tabE4.setButtonText(assignPageAlt ? "Assign B" : "Assign");
                syncAssignBar();
            }
        }
        updateFaderFocusHighlights();
        repaint();
    }

    void focusParameter(int secIdx, int assignCellIdx) {
        activeSection = std::clamp(secIdx, 0, 3);
        focusedAssignIndex = std::clamp(assignCellIdx, 0, 7);
        tabE1.setToggleState(activeSection == 0, juce::dontSendNotification);
        tabE2.setToggleState(activeSection == 1, juce::dontSendNotification);
        tabE3.setToggleState(activeSection == 2, juce::dontSendNotification);
        tabE4.setToggleState(activeSection == 3, juce::dontSendNotification);
        updateFaderFocusHighlights();
        repaint();
    }

    void updateFaderFocusHighlights() {
        if (faderPreDelay)   faderPreDelay->setFocused(focusedAssignIndex == 0);
        if (faderDecay)      faderDecay->setFocused(focusedAssignIndex == 1);
        if (faderLoDecay)    faderLoDecay->setFocused(focusedAssignIndex == 2 && !assignPageAlt);
        if (faderHiDecay)    faderHiDecay->setFocused(focusedAssignIndex == 3);
        if (faderHiCut)      faderHiCut->setFocused(focusedAssignIndex == 4);
        if (faderHiQScale)   faderHiQScale->setFocused(focusedAssignIndex == 2 && assignPageAlt);
        if (faderModulation) faderModulation->setFocused(focusedAssignIndex == 5);
        if (faderInLevel)    faderInLevel->setFocused(focusedAssignIndex == 6 && !assignPageAlt);
        if (faderWetLevel)   faderWetLevel->setFocused((focusedAssignIndex == 7 && !assignPageAlt) || (focusedAssignIndex == 6 && assignPageAlt));
        if (faderDryLevel)   faderDryLevel->setFocused(focusedAssignIndex == 7 && assignPageAlt);
    }

    void stepPreset(int delta) {
        if (!engine) return;
        const auto& allPresets = nova::graviton::DVR2NonLinearReverb::getFactoryPresets();
        if (allPresets.empty()) return;
        const int count = static_cast<int>(allPresets.size());
        const int nextIdx = (engine->getCurrentPresetIndex() + delta + count) % count;
        engine->loadPresetByIndex(nextIdx);
        updateAllUiFromEngine();
    }

    void populatePresetsForBank(int bankIdx, bool loadFirstPresetOfBank) {
        presetCombo.clear(juce::dontSendNotification);
        const auto& allPresets = nova::graviton::DVR2NonLinearReverb::getFactoryPresets();

        const char* targetBank = "01 \xc2\xb7 Vocal";
        if (bankIdx == 1) targetBank = "02 \xc2\xb7 Room & Hall";
        else if (bankIdx == 2) targetBank = "03 \xc2\xb7 Strings & Piano";
        else if (bankIdx == 3) targetBank = "04 \xc2\xb7 Drums & Perc";
        else if (bankIdx == 4) targetBank = "05 \xc2\xb7 Special";

        int firstPresetGlobalId = -1;
        for (size_t i = 0; i < allPresets.size(); ++i) {
            if (std::strcmp(allPresets[i].bank, targetBank) == 0) {
                const int globalId = static_cast<int>(i + 1);
                if (firstPresetGlobalId < 0) firstPresetGlobalId = globalId;
                presetCombo.addItem(juce::String(i + 1) + ". " + allPresets[i].name, globalId);
            }
        }
        if (firstPresetGlobalId > 0) {
            presetCombo.setSelectedId(firstPresetGlobalId, juce::dontSendNotification);
            if (loadFirstPresetOfBank && engine) {
                engine->loadPresetByIndex(firstPresetGlobalId - 1);
                updateAllUiFromEngine();
            }
        }
    }

    void showPresetMenu() {
        juce::PopupMenu m;
        const auto& allPresets = nova::graviton::DVR2NonLinearReverb::getFactoryPresets();
        const int activeIdx = engine ? engine->getCurrentPresetIndex() : 0;

        juce::String currentBank = "";
        std::unique_ptr<juce::PopupMenu> subMenu;

        for (size_t i = 0; i < allPresets.size(); ++i) {
            const juce::String bName = allPresets[i].bank;
            if (bName != currentBank) {
                if (subMenu) {
                    m.addSubMenu(currentBank, *subMenu);
                }
                currentBank = bName;
                subMenu = std::make_unique<juce::PopupMenu>();
            }
            if (subMenu) {
                const bool isTicked = (static_cast<int>(i) == activeIdx);
                subMenu->addItem(static_cast<int>(i + 1),
                                 juce::String(i + 1) + ". " + allPresets[i].name,
                                 true, isTicked);
            }
        }
        if (subMenu) {
            m.addSubMenu(currentBank, *subMenu);
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
            [this](int r) {
                const auto& presets = nova::graviton::DVR2NonLinearReverb::getFactoryPresets();
                if (r >= 1 && r <= static_cast<int>(presets.size()) && engine) {
                    engine->loadPresetByIndex(r - 1);
                    updateAllUiFromEngine();
                }
            });
    }

    void updateAllUiFromEngine() {
        if (!engine) return;

        // Synchronize Bank & Preset Combo Boxes with active engine preset
        const auto& allPresets = nova::graviton::DVR2NonLinearReverb::getFactoryPresets();
        const int curIdx = std::clamp(engine->getCurrentPresetIndex(), 0, static_cast<int>(allPresets.size()) - 1);
        if (!allPresets.empty()) {
            const int bIdx = getBankIndexForName(allPresets[static_cast<size_t>(curIdx)].bank);
            if (bankCombo.getSelectedId() != bIdx + 1) {
                bankCombo.setSelectedId(bIdx + 1, juce::dontSendNotification);
            }
            populatePresetsForBank(bIdx, false);
            presetCombo.setSelectedId(curIdx + 1, juce::dontSendNotification);
        }

        if (faderPreDelay)   faderPreDelay->setValue(engine->getPreDelay(), juce::dontSendNotification);
        if (faderDecay)      faderDecay->setValue(engine->getDecay(), juce::dontSendNotification);
        if (faderLoDecay)    faderLoDecay->setValue(engine->getLoDecay(), juce::dontSendNotification);
        if (faderHiDecay)    faderHiDecay->setValue(engine->getHiDecay(), juce::dontSendNotification);

        if (faderHiCut)      faderHiCut->setValue(engine->getHiCut(), juce::dontSendNotification);
        if (faderHiQScale)   faderHiQScale->setValue(engine->getHiQScale(), juce::dontSendNotification);
        if (faderModulation) faderModulation->setValue(engine->getModulation(), juce::dontSendNotification);

        btnInputTrans.setToggleState(engine->getInputTrans(), juce::dontSendNotification);
        btnTrimLoFreq.setToggleState(engine->getTrimLoFreq(), juce::dontSendNotification);
        btnLoRes.setToggleState(engine->getLoRes(), juce::dontSendNotification);

        if (faderInLevel)    faderInLevel->setValue(engine->getInLevel(), juce::dontSendNotification);
        if (faderWetLevel)   faderWetLevel->setValue(engine->getWetLevel(), juce::dontSendNotification);
        if (faderDryLevel) {
            faderDryLevel->setValue(engine->getDryLevel(), juce::dontSendNotification);
            faderDryLevel->setOverrideText(engine->getKillDry() ? "Kill Dry" : "");
        }

        killDryBtn.setToggleState(engine->getKillDry(), juce::dontSendNotification);
        killDryBtn.setButtonText(engine->getKillDry() ? "KILL DRY: ON" : "KILL DRY: OFF");

        powerBtn.setToggleState(!engine->getBypass(), juce::dontSendNotification);
        powerBtn.setButtonText(powerBtn.getToggleState() ? "POWER" : "BYPASS");

        updateFaderFocusHighlights();
        syncAssignBar();
        repaint();
    }

    void applyAssignCellNormalized(int cellIdx, float norm) {
        if (!engine) return;
        norm = std::clamp(norm, 0.0f, 1.0f);

        if (!assignPageAlt) {
            switch (cellIdx) {
                case 0: engine->setPreDelay(norm * 500.0); break;
                case 1: engine->setDecay(0.2 + norm * (20.0 - 0.2)); break;
                case 2: engine->setLoDecay(0.1 + norm * (2.5 - 0.1)); break;
                case 3: engine->setHiDecay(0.1 + norm * (2.0 - 0.1)); break;
                case 4: engine->setHiCut(1000.0 + norm * (18000.0 - 1000.0)); break;
                case 5: engine->setModulation(norm * 200.0); break;
                case 6: engine->setInLevel(-100.0 + norm * (12.0 - (-100.0))); break;
                case 7: engine->setWetLevel(-100.0 + norm * (6.0 - (-100.0))); break;
                default: break;
            }
        } else {
            switch (cellIdx) {
                case 0: engine->setPreDelay(norm * 500.0); break;
                case 1: engine->setDecay(0.2 + norm * (20.0 - 0.2)); break;
                case 2: engine->setHiQScale(0.5 + norm * (2.5 - 0.5)); break;
                case 3: engine->setHiDecay(0.1 + norm * (2.0 - 0.1)); break;
                case 4: engine->setHiCut(1000.0 + norm * (18000.0 - 1000.0)); break;
                case 5: engine->setModulation(norm * 200.0); break;
                case 6: engine->setWetLevel(-100.0 + norm * (6.0 - (-100.0))); break;
                case 7:
                    if (engine->getKillDry() && norm > 0.08f) engine->setKillDry(false);
                    engine->setDryLevel(-100.0 + norm * (0.0 - (-100.0)));
                    break;
                default: break;
            }
        }
        updateAllUiFromEngine();
    }

    void resetAssignCellToDefault(int cellIdx) {
        if (!engine) return;
        if (!assignPageAlt) {
            switch (cellIdx) {
                case 0: engine->setPreDelay(35.0); break;
                case 1: engine->setDecay(2.2); break;
                case 2: engine->setLoDecay(0.85); break;
                case 3: engine->setHiDecay(0.80); break;
                case 4: engine->setHiCut(8000.0); break;
                case 5: engine->setModulation(100.0); break;
                case 6: engine->setInLevel(0.0); break;
                case 7: engine->setWetLevel(-3.5); break;
                default: break;
            }
        } else {
            switch (cellIdx) {
                case 0: engine->setPreDelay(35.0); break;
                case 1: engine->setDecay(2.2); break;
                case 2: engine->setHiQScale(1.4); break;
                case 3: engine->setHiDecay(0.80); break;
                case 4: engine->setHiCut(8000.0); break;
                case 5: engine->setModulation(100.0); break;
                case 6: engine->setWetLevel(-3.5); break;
                case 7: engine->setKillDry(false); engine->setDryLevel(0.0); break;
                default: break;
            }
        }
        updateAllUiFromEngine();
    }

    void syncAssignBar() {
        if (!engine) return;
        assignLabels[0] = "Pre Delay";
        assignValues[0] = (engine->getPreDelay() >= 1000.0) ? (juce::String(engine->getPreDelay() * 0.001, 2) + " s")
                                                           : (juce::String(static_cast<int>(engine->getPreDelay())) + " ms");
        assignNorms[0]  = static_cast<float>(engine->getPreDelay() / 500.0);

        assignLabels[1] = "Decay";
        assignValues[1] = juce::String(engine->getDecay(), 2) + " s";
        assignNorms[1]  = static_cast<float>((engine->getDecay() - 0.2) / (20.0 - 0.2));

        if (!assignPageAlt) {
            assignLabels[2] = "Lo Decay";
            assignValues[2] = juce::String(engine->getLoDecay(), 2) + " x";
            assignNorms[2]  = static_cast<float>((engine->getLoDecay() - 0.1) / (2.5 - 0.1));
        } else {
            assignLabels[2] = "Hi QScale";
            assignValues[2] = juce::String(engine->getHiQScale(), 2);
            assignNorms[2]  = static_cast<float>((engine->getHiQScale() - 0.5) / (2.5 - 0.5));
        }

        assignLabels[3] = "Hi Decay";
        assignValues[3] = juce::String(engine->getHiDecay(), 2) + " x";
        assignNorms[3]  = static_cast<float>((engine->getHiDecay() - 0.1) / (2.0 - 0.1));

        assignLabels[4] = "Hi Cut";
        assignValues[4] = juce::String(engine->getHiCut() * 0.001, 1) + " kHz";
        assignNorms[4]  = static_cast<float>((engine->getHiCut() - 1000.0) / (18000.0 - 1000.0));

        assignLabels[5] = "Modulation";
        assignValues[5] = juce::String(static_cast<int>(engine->getModulation())) + " %";
        assignNorms[5]  = static_cast<float>(engine->getModulation() / 200.0);

        if (!assignPageAlt) {
            assignLabels[6] = "In Level";
            assignValues[6] = (engine->getInLevel() <= -90.0) ? "Off" : (juce::String(engine->getInLevel(), 1) + " dB");
            assignNorms[6]  = static_cast<float>((engine->getInLevel() - (-100.0)) / (12.0 - (-100.0)));

            assignLabels[7] = "Wet Level";
            assignValues[7] = (engine->getWetLevel() <= -90.0) ? "Off" : (juce::String(engine->getWetLevel(), 1) + " dB");
            assignNorms[7]  = static_cast<float>((engine->getWetLevel() - (-100.0)) / (6.0 - (-100.0)));
        } else {
            assignLabels[6] = "Wet Level";
            assignValues[6] = (engine->getWetLevel() <= -90.0) ? "Off" : (juce::String(engine->getWetLevel(), 1) + " dB");
            assignNorms[6]  = static_cast<float>((engine->getWetLevel() - (-100.0)) / (6.0 - (-100.0)));

            assignLabels[7] = "Dry Level";
            assignValues[7] = (engine->getKillDry() || engine->getDryLevel() <= -90.0)
                                  ? "Off"
                                  : (juce::String(engine->getDryLevel(), 1) + " dB");
            assignNorms[7]  = engine->getKillDry()
                                  ? 0.0f
                                  : static_cast<float>((engine->getDryLevel() - (-100.0)) / (0.0 - (-100.0)));
        }

        repaint(assignBarBounds);
    }

    void timerCallback() override {
        if (!engine) return;
        inMeter.setLevels(engine->getMeterInL(), engine->getMeterInR(), engine->hasOverloadIn());
        outMeter.setLevels(engine->getMeterOutL(), engine->getMeterOutR(), engine->hasOverloadOut());
    }

    nova::graviton::DVR2NonLinearReverb* engine = nullptr;
    int activeSection = 0;
    int focusedAssignIndex = 1;
    bool assignPageAlt = false;
    float assignDragStartNorm = 0.0f;
    float assignDragStartY = 0.0f;
    float assignDragStartX = 0.0f;

    juce::Rectangle<int> mainBoxBounds;
    juce::Rectangle<int> vintageBoxBounds;
    juce::Rectangle<int> levelsBoxBounds;
    juce::Rectangle<int> metersBoxBounds;
    juce::Rectangle<int> assignBarBounds;

    // Top Controls
    juce::ComboBox bankCombo;
    juce::ComboBox presetCombo;
    juce::TextButton prevPresetBtn;
    juce::TextButton nextPresetBtn;
    juce::TextButton presetBtn;
    juce::TextButton tabE1, tabE2, tabE3, tabE4;
    juce::TextButton killDryBtn;
    juce::TextButton powerBtn;

    // 4 Main Parameter Faders
    std::unique_ptr<TCSystem6000Fader> faderPreDelay;
    std::unique_ptr<TCSystem6000Fader> faderDecay;
    std::unique_ptr<TCSystem6000Fader> faderLoDecay;
    std::unique_ptr<TCSystem6000Fader> faderHiDecay;

    // Vintage Section
    std::unique_ptr<TCSystem6000Fader> faderHiCut;
    std::unique_ptr<TCSystem6000Fader> faderHiQScale;
    std::unique_ptr<TCSystem6000Fader> faderModulation;
    juce::TextButton btnInputTrans;
    juce::TextButton btnTrimLoFreq;
    juce::TextButton btnLoRes;
    juce::TextButton btnVintageReset;

    // Levels Section
    std::unique_ptr<TCSystem6000Fader> faderInLevel;
    std::unique_ptr<TCSystem6000Fader> faderWetLevel;
    std::unique_ptr<TCSystem6000Fader> faderDryLevel;

    // Meters
    TCSystem6000Meter inMeter;
    TCSystem6000Meter outMeter;

    // Assign Bar Strings & Normalized Values
    std::array<juce::String, 8> assignLabels;
    std::array<juce::String, 8> assignValues;
    std::array<float, 8> assignNorms{};
};

} // namespace nova
