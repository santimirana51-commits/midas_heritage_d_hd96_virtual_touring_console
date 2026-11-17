#pragma once

// ==============================================================================
// PrecisionDeEsserPanel.h
// NOVA HD — Authentic Precision Mix Series De-Esser Hardware UI
//
// Matches the Universal Audio Precision De-Esser faceplate & NOVA HD spec:
//   • Section 1 (Left): THRESHOLD Knob (-40..0 dB) + Backlit Amber LCD Readout
//     + [SPEED] Button with FAST (0.5/30ms) & SLOW (2/120ms) Cyan LED Indicators
//   • Section 2 (Center-Left): FREQUENCY Knob (2..16 kHz) + Backlit Amber LCD Readout
//     + Square Illuminated Red-Orange [S] (Solo Sidechain) Button
//   • Section 3 (Center-Right): WIDTH Knob (0.15..1.61 oct Bandpass + Full-CW High-Pass)
//     with Bandpass & High-Pass curve icons + Backlit Amber LCD Readout
//     + [SPLIT] Button with OFF (Broadband) & ON (Selective) Cyan LED Indicators
//   • Section 4 (Right): Horizontal Right-to-Left Cyan LED GAIN REDUCTION Meter
//     (-25, -15, -10, -7, -5, -3, -1.5, 0 dB) with 3-Second Peak Hold
//   • Section 5 (Far-Right): Glowing Cyan Diamond Emblem + OFF [ ] ON Power Switch
// ==============================================================================

#include "DSP/GravitonDeEsser.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace nova {

// ==============================================================================
// 1. Precision Mix Series Rotary Knob (Threshold / Frequency / Width)
// ==============================================================================
class PrecisionDeEsserKnob final : public juce::Component {
public:
    enum class Kind {
        Threshold,  // -40 dB to 0 dB
        Frequency,  // 2 kHz to 16 kHz
        Width       // 0.15 to 1.61 oct Bandpass + High-Pass at full clockwise (1.70)
    };

    PrecisionDeEsserKnob(Kind k, double minVal, double maxVal, double defaultVal)
        : kind(k), minV(minVal), maxV(maxVal), defV(defaultVal), curV(defaultVal) {
        setRepaintsOnMouseActivity(true);
    }

    void setValue(double v, juce::NotificationType notify = juce::dontSendNotification) {
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

    std::function<void(double)> onValueChanged;

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX();
        const float cy = b.getCentreY() - 8.0f;
        const float radius = std::min(b.getWidth(), b.getHeight()) * 0.32f;

        constexpr float startAng = -2.35619449f; // -135 deg
        constexpr float endAng   =  2.35619449f; // +135 deg
        const float norm = static_cast<float>((curV - minV) / std::max(1e-5, maxV - minV));
        const float angle = startAng + norm * (endAng - startAng);

        // 1. White Silkscreen Dot Ring Around Knob (11 dots matching photo)
        const float dotDist = radius + 7.0f;
        for (int i = 0; i <= 10; ++i) {
            const float t = static_cast<float>(i) / 10.0f;
            const float a = startAng + t * (endAng - startAng);
            const float dx = cx + std::sin(a) * dotDist;
            const float dy = cy - std::cos(a) * dotDist;
            const bool isEnd = (i == 0 || i == 5 || i == 10);
            const float rDot = isEnd ? 2.1f : 1.5f;
            const bool isHpZone = (kind == Kind::Width && i == 10);
            g.setColour(isHpZone ? juce::Colour(0xff38bdf8) : juce::Colour(0xffe5e7eb));
            g.fillEllipse(dx - rDot, dy - rDot, rDot * 2.0f, rDot * 2.0f);
        }

        // 2. Drop Shadow
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillEllipse(cx - radius + 1.5f, cy - radius + 3.5f, radius * 2.0f, radius * 2.0f);

        // 3. Dark Charcoal Precision Fluted Skirt
        g.setColour(juce::Colour(0xff181b20));
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff4b5563));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.3f);

        // Subtle knurled grip flutes
        g.setColour(juce::Colour(0xff2d333b));
        for (int i = 0; i < 16; ++i) {
            const float ra = angle + static_cast<float>(i) * (2.0f * 3.14159265f / 16.0f);
            g.drawLine(cx + std::sin(ra) * (radius * 0.76f), cy - std::cos(ra) * (radius * 0.76f),
                       cx + std::sin(ra) * (radius * 0.96f), cy - std::cos(ra) * (radius * 0.96f), 1.3f);
        }

        // 4. Inner Gunmetal Cap with Specular Highlight
        const float capR = radius * 0.72f;
        juce::ColourGradient capGrad(juce::Colour(0xff4b5563), cx - capR * 0.5f, cy - capR * 0.5f,
                                     juce::Colour(0xff1f242c), cx + capR * 0.5f, cy + capR * 0.5f, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
        g.setColour(juce::Colour(0xff6b7280));
        g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        // 5. Crisp White Pointer Line
        const float px1 = cx + std::sin(angle) * (capR * 0.25f);
        const float py1 = cy - std::cos(angle) * (capR * 0.25f);
        const float px2 = cx + std::sin(angle) * (radius * 0.94f);
        const float py2 = cy - std::cos(angle) * (radius * 0.94f);
        g.setColour(juce::Colours::white);
        g.drawLine(px1, py1, px2, py2, 2.2f);

        // 6. Bottom Legend or Filter Icons (Exact match to photo)
        const float botY = b.getHeight() - 18.0f;
        g.setColour(juce::Colour(0xffe5e7eb));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));

        if (kind == Kind::Threshold) {
            g.drawText("-40  dB  0", 0, static_cast<int>(botY), static_cast<int>(b.getWidth()), 15,
                       juce::Justification::centred, false);
        } else if (kind == Kind::Frequency) {
            g.drawText("2  kHz  16", 0, static_cast<int>(botY), static_cast<int>(b.getWidth()), 15,
                       juce::Justification::centred, false);
        } else {
            // Draw Bandpass Bell icon on left & High-Pass Box/Shelf icon on right
            const bool isHp = (curV > 1.63);
            g.setColour(!isHp ? juce::Colour(0xff38bdf8) : juce::Colour(0xff9ca3af));
            juce::Path bpIcon;
            const float lx = cx - 26.0f;
            const float ly = botY + 12.0f;
            bpIcon.startNewSubPath(lx - 8.0f, ly);
            bpIcon.lineTo(lx - 4.0f, ly);
            bpIcon.cubicTo(lx - 2.0f, ly - 10.0f, lx + 2.0f, ly - 10.0f, lx + 4.0f, ly);
            bpIcon.lineTo(lx + 8.0f, ly);
            g.strokePath(bpIcon, juce::PathStrokeType(1.6f));

            // High-Pass icon on right
            g.setColour(isHp ? juce::Colour(0xff38bdf8) : juce::Colour(0xff9ca3af));
            const float rx = cx + 22.0f;
            const float ry = botY + 3.0f;
            g.drawRect(rx - 9.0f, ry, 18.0f, 11.0f, 1.2f);
            juce::Path hpIcon;
            hpIcon.startNewSubPath(rx - 6.0f, ry + 9.0f);
            hpIcon.lineTo(rx - 1.0f, ry + 3.0f);
            hpIcon.lineTo(rx + 6.0f, ry + 3.0f);
            g.strokePath(hpIcon, juce::PathStrokeType(1.5f));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        // Allow clicking the Bandpass / High-Pass icons below the Width knob directly
        if (kind == Kind::Width && e.position.y >= getHeight() - 22.0f) {
            if (e.position.x > getWidth() * 0.5f) {
                setValue(1.70, juce::sendNotificationSync); // High-Pass
            } else {
                setValue(0.89, juce::sendNotificationSync); // Bandpass
            }
            return;
        }
        dragStartVal = curV;
        dragStartY = e.position.y;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float dy = dragStartY - e.position.y;
        const double delta = (static_cast<double>(dy) / 120.0) * (maxV - minV);
        setValue(dragStartVal + delta, juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        setValue(defV, juce::sendNotificationSync);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        const double step = (maxV - minV) * 0.025 * wheel.deltaY;
        setValue(curV + step, juce::sendNotificationSync);
    }

private:
    Kind kind;
    double minV = 0.0, maxV = 1.0, defV = 0.5, curV = 0.5;
    double dragStartVal = 0.5;
    float dragStartY = 0.0f;
};

// ==============================================================================
// 2. Horizontal Right-to-Left Cyan LED Gain Reduction Meter (-25..0 dB + 3s Peak Hold)
// ==============================================================================
class PrecisionDeEsserGrMeter final : public juce::Component {
public:
    std::function<void()> onResetPeakHold;

    void setTelemetry(float grDbValue, float peakHoldGrDb, bool isSplitOn, bool isSoloOn, bool isPowerOn) {
        grDb = isPowerOn ? std::clamp(grDbValue, 0.0f, 25.0f) : 0.0f;
        holdDb = isPowerOn ? std::clamp(peakHoldGrDb, 0.0f, 25.0f) : 0.0f;
        splitOn = isSplitOn;
        soloOn = isSoloOn;
        powerOn = isPowerOn;
        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override {
        if (onResetPeakHold) onResetPeakHold();
    }

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat();

        // Title above meter box: "GAIN REDUCTION" + active mode badge
        g.setColour(juce::Colour(0xffe5e7eb));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText("GAIN REDUCTION", 2, 0, 130, 15, juce::Justification::centredLeft, false);

        g.setColour(soloOn ? juce::Colour(0xfff97316) : juce::Colour(0xff38bdf8));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        const juce::String badge = !powerOn ? "BYPASSED"
                                 : soloOn   ? "SIDECHAIN SOLO ACTIVE"
                                 : splitOn  ? ("SPLIT BAND GR: -" + juce::String(grDb, 1) + " dB (PK -" + juce::String(holdDb, 1) + " dB)")
                                            : ("BROADBAND GR: -" + juce::String(grDb, 1) + " dB (PK -" + juce::String(holdDb, 1) + " dB)");
        g.drawText(badge, 125, 0, static_cast<int>(b.getWidth() - 127.0f), 15, juce::Justification::centredRight, false);

        // Recessed Dark Meter Bezel
        const auto box = b.withTrimmedTop(18.0f);
        g.setColour(juce::Colour(0xff0b0d10));
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(juce::Colour(0xff4b5563));
        g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 1.4f);

        const auto inner = box.reduced(10.0f, 8.0f);
        const auto barArea = inner.withTrimmedBottom(18.0f);

        // Non-linear scale mapping matching UA Precision De-Esser meter markings:
        // Left (0.0) = -25 dB, Right (1.0) = 0 dB
        // Markings: -25, -15, -10, -7, -5, -3, -1.5, 0
        auto grToNormX = [](float positiveGrDb) noexcept -> float {
            struct Pt { float gr; float x; };
            static constexpr Pt kPts[] = {
                {  0.0f, 1.00f },
                {  1.5f, 0.87f },
                {  3.0f, 0.74f },
                {  5.0f, 0.60f },
                {  7.0f, 0.46f },
                { 10.0f, 0.32f },
                { 15.0f, 0.16f },
                { 25.0f, 0.00f }
            };
            const float c = std::clamp(positiveGrDb, 0.0f, 25.0f);
            for (int i = 0; i < 7; ++i) {
                if (c >= kPts[i].gr && c <= kPts[i + 1].gr) {
                    const float t = (c - kPts[i].gr) / (kPts[i + 1].gr - kPts[i].gr);
                    return kPts[i].x + t * (kPts[i + 1].x - kPts[i].x);
                }
            }
            return 0.0f;
        };

        // Draw 36 Segmented Cyan LED Bars (Right-to-Left from 0 dB down to -25 dB)
        constexpr int kNumSegs = 36;
        const float segStep = barArea.getWidth() / static_cast<float>(kNumSegs);
        const float segW = std::max(2.0f, segStep - 1.6f);

        const float activeLeftNorm = grToNormX(grDb);
        const float holdLeftNorm   = grToNormX(holdDb);
        const int holdSegIdx = std::clamp(static_cast<int>(std::floor(holdLeftNorm * kNumSegs)), 0, kNumSegs - 1);

        for (int i = 0; i < kNumSegs; ++i) {
            const float segNormCenter = (static_cast<float>(i) + 0.5f) / static_cast<float>(kNumSegs);
            const float sx = barArea.getX() + static_cast<float>(i) * segStep;
            const auto segRect = juce::Rectangle<float>(sx, barArea.getY(), segW, barArea.getHeight());

            const bool isLit = powerOn && (grDb > 0.05f) && (segNormCenter >= activeLeftNorm);
            const bool isPeakHoldSeg = powerOn && (holdDb > 0.25f) && (i == holdSegIdx);

            if (isLit) {
                // Iconic glowing cyan-blue LED segment
                juce::ColourGradient ledGrad(juce::Colour(0xff67e8f9), sx, barArea.getY(),
                                             juce::Colour(0xff0284c7), sx, barArea.getBottom(), false);
                g.setGradientFill(ledGrad);
                g.fillRoundedRectangle(segRect, 1.0f);
            } else if (isPeakHoldSeg) {
                // 3-second Peak Hold segment
                g.setColour(juce::Colour(0xfffde047));
                g.fillRoundedRectangle(segRect, 1.0f);
            } else {
                // Unlit dark segment cell
                g.setColour(juce::Colour(0xff1f242c));
                g.fillRoundedRectangle(segRect, 1.0f);
            }
        }

        // Draw Scale Ticks & Numbers along bottom (-25, -15, -10, -7, -5, -3, -1.5, 0)
        static constexpr float kTickGr[8]  = { 25.0f, 15.0f, 10.0f, 7.0f, 5.0f, 3.0f, 1.5f, 0.0f };
        static const char* kTickLabel[8]   = { "-25", "-15", "-10", "-7", "-5", "-3", "-1.5", "0" };
        g.setColour(juce::Colour(0xffe5e7eb));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));

        for (int i = 0; i < 8; ++i) {
            const float nx = grToNormX(kTickGr[i]);
            const float tx = barArea.getX() + nx * barArea.getWidth();
            g.setColour(juce::Colour(0xff9ca3af));
            g.drawVerticalLine(static_cast<int>(tx), barArea.getBottom() + 1.0f, barArea.getBottom() + 5.0f);

            g.setColour(juce::Colour(0xfff3f4f6));
            g.drawText(kTickLabel[i], static_cast<int>(tx - 16.0f), static_cast<int>(barArea.getBottom() + 5.0f),
                       32, 13, juce::Justification::centred, false);
        }
    }

private:
    float grDb = 0.0f;
    float holdDb = 0.0f;
    bool splitOn = true;
    bool soloOn = false;
    bool powerOn = true;
};

// ==============================================================================
// 3. Main NOVA HD Precision De-Esser Component
// ==============================================================================
class PrecisionDeEsserPanel final : public juce::Component, private juce::Timer {
public:
    explicit PrecisionDeEsserPanel(nova::graviton::PrecisionDeEsser& deEsserEngine)
        : engine(&deEsserEngine),
          thresholdKnob(PrecisionDeEsserKnob::Kind::Threshold, -40.0, 0.0, -24.0),
          frequencyKnob(PrecisionDeEsserKnob::Kind::Frequency, 2000.0, 16000.0, 6500.0),
          widthKnob(PrecisionDeEsserKnob::Kind::Width, 0.15, 1.70, 0.89)
    {
        // ── Top Studio Toolbar: 5 Preset Banks (16 Presets) & Stereo Link ──
        bankCombo.addItem("BANK 1: Vocal Female", 1);
        bankCombo.addItem("BANK 2: Vocal Male", 2);
        bankCombo.addItem("BANK 3: Vocal Problem", 3);
        bankCombo.addItem("BANK 4: Instruments", 4);
        bankCombo.addItem("BANK 5: Master", 5);
        bankCombo.onChange = [this] {
            const int id = bankCombo.getSelectedId();
            if (id >= 1 && id <= 5 && engine) {
                engine->selectBank(static_cast<nova::graviton::PrecisionDeEsser::PresetBank>(id - 1));
                updateAllUiFromEngine();
            }
        };
        addAndMakeVisible(bankCombo);

        presetBtn.setButtonText("PRESET");
        presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible(presetBtn);

        const auto& presets = nova::graviton::PrecisionDeEsser::getFactoryPresets();
        for (size_t i = 0; i < presets.size(); ++i) {
            presetCombo.addItem(juce::String("[") + presets[i].bankName + "] " + presets[i].name,
                                static_cast<int>(i + 1));
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

        // Filter Mode Quick Toggle Button (Bandpass 0.15-1.61 oct vs High-Pass)
        btnFilterMode.setButtonText("FILTER: BANDPASS");
        btnFilterMode.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f242c));
        btnFilterMode.onClick = [this] {
            if (!engine) return;
            const bool toHp = (engine->getFilterMode() == nova::graviton::PrecisionDeEsser::FilterMode::Bandpass);
            engine->setFilterMode(toHp ? nova::graviton::PrecisionDeEsser::FilterMode::HighPass
                                       : nova::graviton::PrecisionDeEsser::FilterMode::Bandpass);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnFilterMode);

        // Stereo Link Selector (NOVA HD feature)
        stereoLinkCombo.addItem("STEREO LINK: 100%", 1);
        stereoLinkCombo.addItem("STEREO LINK: 75%", 2);
        stereoLinkCombo.addItem("STEREO LINK: 50%", 3);
        stereoLinkCombo.addItem("STEREO LINK: 0% (Dual Mono)", 4);
        stereoLinkCombo.onChange = [this] {
            if (!engine) return;
            const int id = stereoLinkCombo.getSelectedId();
            const double pct = (id == 2) ? 75.0 : (id == 3) ? 50.0 : (id == 4) ? 0.0 : 100.0;
            engine->setStereoLink(pct);
        };
        addAndMakeVisible(stereoLinkCombo);

        // Reset 3-Second Peak Hold Button
        btnResetPeak.setButtonText("CLEAR PEAK HOLD");
        btnResetPeak.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        btnResetPeak.onClick = [this] {
            if (engine) engine->resetPeakHold();
        };
        addAndMakeVisible(btnResetPeak);

        // ── Main Precision De-Esser Hardware Controls ──
        // 1. THRESHOLD Knob & SPEED Button
        thresholdKnob.onValueChanged = [this](double valDb) {
            if (!engine) return;
            engine->setThresholdDialDb(valDb);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(thresholdKnob);

        btnSpeed.setButtonText("SPEED");
        btnSpeed.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        btnSpeed.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff3f4f6));
        btnSpeed.onClick = [this] {
            if (!engine) return;
            const auto nextSpd = (engine->getSpeedMode() == nova::graviton::PrecisionDeEsser::SpeedMode::Fast)
                                     ? nova::graviton::PrecisionDeEsser::SpeedMode::Slow
                                     : nova::graviton::PrecisionDeEsser::SpeedMode::Fast;
            engine->setSpeedMode(nextSpd);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnSpeed);

        // 2. FREQUENCY Knob & [S] (Solo) Button
        frequencyKnob.onValueChanged = [this](double valHz) {
            if (!engine) return;
            engine->setFrequencyHz(valHz);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(frequencyKnob);

        btnSolo.setButtonText("S");
        btnSolo.setClickingTogglesState(true);
        btnSolo.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        btnSolo.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffea580c)); // Glowing red-orange Solo button
        btnSolo.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btnSolo.onClick = [this] {
            if (!engine) return;
            engine->setSolo(btnSolo.getToggleState());
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnSolo);

        // 3. WIDTH Knob & SPLIT Button
        widthKnob.onValueChanged = [this](double dialVal) {
            if (!engine) return;
            engine->setWidthDialValue(dialVal);
            updateAllUiFromEngine();
        };
        addAndMakeVisible(widthKnob);

        btnSplit.setButtonText("SPLIT");
        btnSplit.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        btnSplit.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff3f4f6));
        btnSplit.onClick = [this] {
            if (!engine) return;
            engine->setSplitEnabled(!engine->getSplitEnabled());
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnSplit);

        // 4. Gain Reduction Meter
        grMeter.onResetPeakHold = [this] {
            if (engine) engine->resetPeakHold();
        };
        addAndMakeVisible(grMeter);

        // 5. Power Rocker Switch (OFF / ON)
        btnPower.setButtonText("ON");
        btnPower.setClickingTogglesState(true);
        btnPower.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff272b33));
        btnPower.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0284c7));
        btnPower.onClick = [this] {
            if (!engine) return;
            engine->setPower(btnPower.getToggleState());
            updateAllUiFromEngine();
        };
        addAndMakeVisible(btnPower);

        updateAllUiFromEngine();
        startTimerHz(30);
        setSize(1120, 310);
    }

    ~PrecisionDeEsserPanel() override {
        stopTimer();
    }

    void setEngine(nova::graviton::PrecisionDeEsser& newEngine) {
        engine = &newEngine;
        updateAllUiFromEngine();
    }

    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds();

        // 1. Top Studio Toolbar
        g.setColour(juce::Colour(0xff161b22));
        g.fillRect(0, 0, b.getWidth(), 30);
        g.setColour(juce::Colour(0xff30363d));
        g.drawHorizontalLine(29, 0.0f, static_cast<float>(b.getWidth()));

        // Purpose description badge in top bar
        if (engine) {
            const auto& presets = nova::graviton::PrecisionDeEsser::getFactoryPresets();
            const int idx = std::clamp(engine->getCurrentPresetIndex(), 0, static_cast<int>(presets.size()) - 1);
            g.setColour(juce::Colour(0xff93c5fd));
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText("PURPOSE: " + juce::String(presets[static_cast<size_t>(idx)].purpose) + "  |  RATIO: 7:1",
                       purposeBounds, juce::Justification::centredLeft, false);
        }

        // 2. Precision Mix Series Brushed Slate-Charcoal Hardware Unit
        const auto chassis = juce::Rectangle<float>(4.0f, 34.0f, static_cast<float>(b.getWidth() - 8), static_cast<float>(b.getHeight() - 38));
        juce::ColourGradient metalGrad(juce::Colour(0xff383c42), chassis.getX(), chassis.getY(),
                                       juce::Colour(0xff22252a), chassis.getX(), chassis.getBottom(), false);
        g.setGradientFill(metalGrad);
        g.fillRoundedRectangle(chassis, 4.0f);

        // Inner recessed faceplate frame (Matches photo's silver-trimmed inner panel)
        const auto innerPanel = juce::Rectangle<float>(chassis.getX() + 34.0f, chassis.getY() + 10.0f,
                                                       brandSectionX - chassis.getX() - 42.0f, chassis.getHeight() - 20.0f);
        juce::ColourGradient panelGrad(juce::Colour(0xff3f434a), innerPanel.getX(), innerPanel.getY(),
                                       juce::Colour(0xff2b2e33), innerPanel.getX(), innerPanel.getBottom(), false);
        g.setGradientFill(panelGrad);
        g.fillRoundedRectangle(innerPanel, 3.0f);
        g.setColour(juce::Colour(0xff9ca3af));
        g.drawRoundedRectangle(innerPanel, 3.0f, 1.5f);

        // Diagonal/Vertical White Section Lines inside innerPanel (Matches photo)
        g.setColour(juce::Colour(0xff9ca3af).withAlpha(0.75f));
        g.drawLine(sec1RightX, innerPanel.getY() + 18.0f, sec1RightX - 18.0f, innerPanel.getBottom() - 6.0f, 1.5f);
        g.drawLine(sec2RightX, innerPanel.getY() + 18.0f, sec2RightX - 18.0f, innerPanel.getBottom() - 6.0f, 1.5f);
        g.drawLine(sec3RightX, innerPanel.getY() + 2.0f,  sec3RightX,         innerPanel.getBottom() - 2.0f, 1.5f);

        // 4 Corner Allen Bolts on Outer Chassis
        auto drawAllenBolt = [&](float cx, float cy) {
            g.setColour(juce::Colour(0xff111317));
            g.fillEllipse(cx - 7.5f, cy - 7.5f, 15.0f, 15.0f);
            g.setColour(juce::Colour(0xff6b7280));
            g.drawEllipse(cx - 7.5f, cy - 7.5f, 15.0f, 15.0f, 1.2f);
            g.setColour(juce::Colour(0xff9ca3af));
            g.drawEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f, 1.2f);
        };
        drawAllenBolt(chassis.getX() + 16.0f, chassis.getY() + 18.0f);
        drawAllenBolt(chassis.getX() + 16.0f, chassis.getBottom() - 18.0f);
        drawAllenBolt(chassis.getRight() - 16.0f, chassis.getY() + 18.0f);
        drawAllenBolt(chassis.getRight() - 16.0f, chassis.getBottom() - 18.0f);

        // 3. Draw Backlit Yellow-Amber Digital LCD Readout Boxes (THRESHOLD, FREQUENCY, WIDTH)
        auto drawLcdReadout = [&](juce::Rectangle<int> lcdRect, const juce::String& title, const juce::String& valText) {
            g.setColour(juce::Colour(0xffe5e7eb));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(title, lcdRect.getX() - 4, lcdRect.getY() - 17, lcdRect.getWidth() + 8, 14,
                       juce::Justification::centredLeft, false);

            // Outer black recessed bezel
            g.setColour(juce::Colour(0xff0b0d10));
            g.fillRoundedRectangle(lcdRect.expanded(2, 2).toFloat(), 2.5f);
            g.setColour(juce::Colour(0xff4b5563));
            g.drawRoundedRectangle(lcdRect.expanded(2, 2).toFloat(), 2.5f, 1.0f);

            // Warm backlit yellow-cream LCD screen (Matches uploaded photo)
            const bool pwr = engine ? engine->getPower() : true;
            juce::ColourGradient lcdGrad(pwr ? juce::Colour(0xfffef08a) : juce::Colour(0xff374151),
                                         static_cast<float>(lcdRect.getX()), static_cast<float>(lcdRect.getY()),
                                         pwr ? juce::Colour(0xffeab308) : juce::Colour(0xff1f242c),
                                         static_cast<float>(lcdRect.getX()), static_cast<float>(lcdRect.getBottom()), false);
            g.setGradientFill(lcdGrad);
            g.fillRoundedRectangle(lcdRect.toFloat(), 1.5f);

            // Inner top shadow inside LCD
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.fillRect(lcdRect.getX(), lcdRect.getY(), lcdRect.getWidth(), 3);

            // Dark LCD digits
            g.setColour(pwr ? juce::Colour(0xff1c1917) : juce::Colour(0xff9ca3af));
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText(valText, lcdRect, juce::Justification::centred, false);
        };

        const double thrDb = engine ? engine->getThresholdDialDb() : -24.0;
        const double freqKhz = (engine ? engine->getFrequencyHz() : 6500.0) * 0.001;
        const bool isHp = engine && (engine->getFilterMode() == nova::graviton::PrecisionDeEsser::FilterMode::HighPass);
        const juce::String widthStr = isHp ? "HIGH PASS"
                                           : (juce::String(engine ? engine->getWidthOctaves() : 0.89, 2) + " oct");

        drawLcdReadout(threshLcdBounds, "THRESHOLD", juce::String(thrDb, 2) + " dB");
        drawLcdReadout(freqLcdBounds,   "FREQUENCY", juce::String(freqKhz, 2) + " k");
        drawLcdReadout(widthLcdBounds,  "WIDTH",     widthStr);

        // 4. Draw Dual Cyan LED Indicators Next to [SPEED] (FAST / SLOW) and [SPLIT] (OFF / ON)
        auto drawDualLedPair = [&](int x, int y, const char* topLbl, const char* botLbl, bool topActive, bool botActive) {
            const bool pwr = engine ? engine->getPower() : true;
            // Top LED pill
            g.setColour(juce::Colour(0xff111317));
            g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(y + 2), 14.0f, 6.0f, 2.0f);
            if (pwr && topActive) {
                g.setColour(juce::Colour(0xff38bdf8));
                g.fillRoundedRectangle(static_cast<float>(x + 1), static_cast<float>(y + 3), 12.0f, 4.0f, 1.5f);
            }
            g.setColour(juce::Colour(0xffe5e7eb));
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.drawText(topLbl, x + 17, y - 1, 36, 11, juce::Justification::centredLeft, false);

            // Bottom LED pill
            g.setColour(juce::Colour(0xff111317));
            g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(y + 14), 14.0f, 6.0f, 2.0f);
            if (pwr && botActive) {
                g.setColour(juce::Colour(0xff38bdf8));
                g.fillRoundedRectangle(static_cast<float>(x + 1), static_cast<float>(y + 15), 12.0f, 4.0f, 1.5f);
            }
            g.setColour(juce::Colour(0xffe5e7eb));
            g.drawText(botLbl, x + 17, y + 11, 36, 11, juce::Justification::centredLeft, false);
        };

        const bool isFast = engine && (engine->getSpeedMode() == nova::graviton::PrecisionDeEsser::SpeedMode::Fast);
        drawDualLedPair(btnSpeed.getRight() + 6, btnSpeed.getY(), "FAST", "SLOW", isFast, !isFast);

        const bool isSplit = engine ? engine->getSplitEnabled() : true;
        drawDualLedPair(btnSplit.getRight() + 6, btnSplit.getY(), "OFF", "ON", !isSplit, isSplit);

        // 5. Right-Side Branding & Glowing Blue UA Diamond Emblem (Exact match to photo)
        const float brX = brandSectionX;
        const float brW = chassis.getRight() - brX - 28.0f;
        const float brCy = chassis.getCentreY();

        // Glowing Cyan UA Diamond Badge on right
        const float diaCx = brX + brW * 0.74f;
        const float diaCy = brCy - 24.0f;
        juce::Path diamond;
        diamond.startNewSubPath(diaCx, diaCy - 18.0f);
        diamond.lineTo(diaCx + 32.0f, diaCy);
        diamond.lineTo(diaCx, diaCy + 18.0f);
        diamond.lineTo(diaCx - 32.0f, diaCy);
        diamond.closeSubPath();

        const bool pwr = engine ? engine->getPower() : true;
        g.setColour(pwr ? juce::Colour(0xff0284c7).withAlpha(0.45f) : juce::Colour(0xff1f242c));
        g.fillPath(diamond);
        g.setColour(pwr ? juce::Colour(0xff38bdf8) : juce::Colour(0xff4b5563));
        g.strokePath(diamond, juce::PathStrokeType(2.2f));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("UA", static_cast<int>(diaCx - 18.0f), static_cast<int>(diaCy - 8.0f), 36, 16, juce::Justification::centred, false);

        // Left-side text of brand block: UNIVERSAL AUDIO / PRECISION MIX SERIES / DE-ESSER
        const int txtX = static_cast<int>(brX);
        const int txtW = static_cast<int>(brW * 0.52f);
        g.setColour(juce::Colour(0xfff3f4f6));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText("UNIVERSAL AUDIO", txtX, static_cast<int>(diaCy - 16.0f), txtW, 13, juce::Justification::centredRight, false);

        g.setColour(juce::Colour(0xff38bdf8));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText("PRECISION MIX SERIES", txtX, static_cast<int>(diaCy - 3.0f), txtW, 12, juce::Justification::centredRight, false);

        g.setColour(juce::Colour(0xfff3f4f6));
        g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
        g.drawText("DE-ESSER", txtX, static_cast<int>(diaCy + 9.0f), txtW, 15, juce::Justification::centredRight, false);

        // OFF / ON silkscreen labels beside the Power Rocker Switch
        g.setColour(juce::Colour(0xffe5e7eb));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("OFF", btnPower.getX() - 28, btnPower.getY(), 24, btnPower.getHeight(), juce::Justification::centredRight, false);
        g.drawText("ON",  btnPower.getRight() + 4, btnPower.getY(), 24, btnPower.getHeight(), juce::Justification::centredLeft, false);
    }

    void resized() override {
        const int w = getWidth();
        const int h = getHeight();

        // ── Top Studio Toolbar ──
        const int tbY = 4;
        int tx = 6;
        bankCombo.setBounds(tx, tbY, 152, 22); tx += 155;
        presetBtn.setBounds(tx, tbY, 50, 22); tx += 53;
        presetCombo.setBounds(tx, tbY, 210, 22); tx += 213;
        prevPresetBtn.setBounds(tx, tbY, 22, 22); tx += 24;
        nextPresetBtn.setBounds(tx, tbY, 22, 22); tx += 26;

        btnFilterMode.setBounds(tx, tbY, 128, 22); tx += 131;
        stereoLinkCombo.setBounds(tx, tbY, 138, 22); tx += 141;
        btnResetPeak.setBounds(tx, tbY, 110, 22); tx += 115;
        purposeBounds = juce::Rectangle<int>(tx, tbY, std::max(80, w - tx - 8), 22);

        // ── Main Precision De-Esser Hardware Layout ──
        const int faceY = 44;
        const int faceH = h - faceY - 14;
        const int cy = faceY + faceH / 2;

        brandSectionX = w * 0.78f;
        const float innerLeft = 38.0f;
        const float innerW = brandSectionX - innerLeft - 8.0f;

        sec1RightX = innerLeft + innerW * 0.235f;
        sec2RightX = innerLeft + innerW * 0.470f;
        sec3RightX = innerLeft + innerW * 0.705f;

        const int knobSz = std::min(118, faceH - 18);

        // Section 1: THRESHOLD Knob + LCD + SPEED Button
        const int s1X = static_cast<int>(innerLeft) + 4;
        thresholdKnob.setBounds(s1X, cy - knobSz / 2 + 4, knobSz, knobSz);
        const int s1RightColX = s1X + knobSz - 2;
        threshLcdBounds = juce::Rectangle<int>(s1RightColX, cy - 28, 86, 26);
        btnSpeed.setBounds(s1RightColX, cy + 12, 46, 22);

        // Section 2: FREQUENCY Knob + LCD + [S] Solo Button
        const int s2X = static_cast<int>(sec1RightX) + 4;
        frequencyKnob.setBounds(s2X, cy - knobSz / 2 + 4, knobSz, knobSz);
        const int s2RightColX = s2X + knobSz - 2;
        freqLcdBounds = juce::Rectangle<int>(s2RightColX, cy - 28, 86, 26);
        btnSolo.setBounds(s2RightColX + 28, cy + 11, 28, 24);

        // Section 3: WIDTH Knob + LCD + SPLIT Button
        const int s3X = static_cast<int>(sec2RightX) + 4;
        widthKnob.setBounds(s3X, cy - knobSz / 2 + 4, knobSz, knobSz);
        const int s3RightColX = s3X + knobSz - 2;
        widthLcdBounds = juce::Rectangle<int>(s3RightColX, cy - 28, 86, 26);
        btnSplit.setBounds(s3RightColX, cy + 12, 46, 22);

        // Section 4: GAIN REDUCTION Horizontal Meter
        const int s4X = static_cast<int>(sec3RightX) + 12;
        const int s4W = static_cast<int>(brandSectionX) - s4X - 18;
        const int meterH = std::min(96, faceH - 36);
        grMeter.setBounds(s4X, cy - meterH / 2, s4W, meterH);

        // Section 5: Far-Right Power Switch
        const int pwrX = static_cast<int>(brandSectionX + (w - brandSectionX) * 0.52f);
        btnPower.setBounds(pwrX - 24, cy + 20, 52, 24);
    }

private:
    void stepPreset(int delta) {
        if (!engine) return;
        const auto& all = nova::graviton::PrecisionDeEsser::getFactoryPresets();
        if (all.empty()) return;
        const int n = static_cast<int>(all.size());
        const int nextIdx = (engine->getCurrentPresetIndex() + delta + n) % n;
        engine->loadPresetByIndex(nextIdx);
        updateAllUiFromEngine();
    }

    void showPresetMenu() {
        juce::PopupMenu m;
        const auto& all = nova::graviton::PrecisionDeEsser::getFactoryPresets();
        const int cur = engine ? engine->getCurrentPresetIndex() : 0;

        static constexpr const char* kBankHeaders[5] = {
            "1. Vocal Female",
            "2. Vocal Male",
            "3. Vocal Problem",
            "4. Instruments",
            "5. Master"
        };

        for (int b = 0; b < 5; ++b) {
            juce::PopupMenu sub;
            for (size_t i = 0; i < all.size(); ++i) {
                if (static_cast<int>(all[i].bank) == b) {
                    sub.addItem(static_cast<int>(i + 1),
                                juce::String(all[i].name) + " — " + all[i].purpose,
                                true, static_cast<int>(i) == cur);
                }
            }
            m.addSubMenu(kBankHeaders[b], sub);
        }

        m.addSeparator();
        for (size_t i = 0; i < all.size(); ++i) {
            m.addItem(static_cast<int>(i + 1),
                      juce::String("[") + all[i].bankName + "] " + all[i].name + " — " + all[i].purpose,
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

        bankCombo.setSelectedId(static_cast<int>(engine->getCurrentBank()) + 1, juce::dontSendNotification);
        presetCombo.setSelectedId(engine->getCurrentPresetIndex() + 1, juce::dontSendNotification);

        thresholdKnob.setValue(engine->getThresholdDialDb(), juce::dontSendNotification);
        frequencyKnob.setValue(engine->getFrequencyHz(), juce::dontSendNotification);
        widthKnob.setValue(engine->getWidthDialValue(), juce::dontSendNotification);

        btnSolo.setToggleState(engine->getSolo(), juce::dontSendNotification);

        const bool isHp = (engine->getFilterMode() == nova::graviton::PrecisionDeEsser::FilterMode::HighPass);
        btnFilterMode.setButtonText(isHp ? "FILTER: HIGH-PASS" : "FILTER: BANDPASS");
        btnFilterMode.setColour(juce::TextButton::buttonColourId,
                                isHp ? juce::Colour(0xff0284c7) : juce::Colour(0xff1f242c));

        const double link = engine->getStereoLink();
        const int linkId = (link < 25.0) ? 4 : (link < 62.0) ? 3 : (link < 88.0) ? 2 : 1;
        stereoLinkCombo.setSelectedId(linkId, juce::dontSendNotification);

        const bool pwr = engine->getPower();
        btnPower.setToggleState(pwr, juce::dontSendNotification);
        btnPower.setButtonText(pwr ? "ON" : "OFF");

        repaint();
    }

    void timerCallback() override {
        if (!engine) return;
        grMeter.setTelemetry(engine->getMeterGrDb(),
                             engine->getMeterPeakHoldDb(),
                             engine->getSplitEnabled(),
                             engine->getSolo(),
                             engine->getPower());
    }

    nova::graviton::PrecisionDeEsser* engine = nullptr;

    // Top Bar Controls
    juce::ComboBox bankCombo;
    juce::TextButton presetBtn, prevPresetBtn, nextPresetBtn;
    juce::ComboBox presetCombo;
    juce::TextButton btnFilterMode;
    juce::ComboBox stereoLinkCombo;
    juce::TextButton btnResetPeak;
    juce::Rectangle<int> purposeBounds;

    // Hardware Faceplate Controls
    PrecisionDeEsserKnob thresholdKnob;
    PrecisionDeEsserKnob frequencyKnob;
    PrecisionDeEsserKnob widthKnob;

    juce::Rectangle<int> threshLcdBounds;
    juce::Rectangle<int> freqLcdBounds;
    juce::Rectangle<int> widthLcdBounds;

    juce::TextButton btnSpeed;
    juce::TextButton btnSolo;
    juce::TextButton btnSplit;
    PrecisionDeEsserGrMeter grMeter;
    juce::TextButton btnPower;

    float sec1RightX = 240.0f;
    float sec2RightX = 460.0f;
    float sec3RightX = 680.0f;
    float brandSectionX = 860.0f;
};

} // namespace nova
