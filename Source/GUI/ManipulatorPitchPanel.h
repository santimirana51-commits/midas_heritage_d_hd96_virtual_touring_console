#pragma once

#include "Effects/GravitonManipulatorPitch.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>
#include <iomanip>
#include <sstream>

namespace nova {

// ==============================================================================
// 1. Futuristic Glowing Manipulator Circular Knob
// With digital centered text, glowing radial arc, and multi-colored orbiting dots
// ==============================================================================
class ManipulatorKnob final : public juce::Component {
public:
    enum class Size { Large, Medium, Small };

    ManipulatorKnob(const juce::String& labelText,
                    double minV, double maxV, double defaultV,
                    juce::Colour arcColor,
                    Size knobSize = Size::Medium,
                    int decimalPlaces = 2)
        : title(labelText),
          minValue(minV),
          maxValue(maxV),
          defaultValue(defaultV),
          currentValue(defaultV),
          primaryColor(arcColor),
          sizeCategory(knobSize),
          decimals(decimalPlaces) {
        setRepaintsOnMouseActivity(true);
    }

    void setValue(double val, juce::NotificationType notify = juce::dontSendNotification) {
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

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY() + 6.0f;

        const float diameter = std::min(bounds.getWidth(), bounds.getHeight() - 20.0f);
        const float radius = diameter * 0.44f;

        // 1. Title on top or bottom
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(title, 0, 0, static_cast<int>(bounds.getWidth()), 16, juce::Justification::centred, false);

        // 2. Multi-colored outer orbiting dots
        constexpr int numDots = 14;
        const juce::Colour dotColours[4] = {
            juce::Colour(0xff00e5ff), // Cyan
            juce::Colour(0xffd946ef), // Magenta
            juce::Colour(0xffeab308), // Yellow
            juce::Colour(0xff818cf8)  // Indigo
        };
        for (int i = 0; i < numDots; ++i) {
            const float angle = -2.35619449f + (static_cast<float>(i) / (numDots - 1)) * 4.71238898f;
            const float dotR = radius + 4.5f;
            const float dx = cx + std::sin(angle) * dotR;
            const float dy = cy - std::cos(angle) * dotR;

            g.setColour(dotColours[i % 4].withAlpha(0.65f));
            g.fillEllipse(dx - 1.5f, dy - 1.5f, 3.0f, 3.0f);
        }

        // 3. Dark Outer Ring
        const auto dialBounds = juce::Rectangle<float>(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff141c2b));
        g.drawEllipse(dialBounds, 3.5f);

        // 4. Glowing Arc
        const float norm = static_cast<float>((currentValue - minValue) / std::max(1e-4, maxValue - minValue));
        constexpr float startAngle = -2.35619449f; // -135 deg
        constexpr float endAngle   =  2.35619449f; // +135 deg
        const float curAngle = startAngle + norm * (endAngle - startAngle);

        if (norm > 0.005f) {
            juce::Path arc;
            arc.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, curAngle, true);
            g.setColour(primaryColor.withAlpha(0.3f));
            g.strokePath(arc, juce::PathStrokeType(7.0f)); // Glow
            g.setColour(primaryColor);
            g.strokePath(arc, juce::PathStrokeType(3.5f)); // Core line
        }

        // 5. Dark Inner Disc (Metallic Blue-Navy)
        const auto inner = dialBounds.reduced(radius * 0.22f);
        juce::ColourGradient grad(juce::Colour(0xff182234), cx, inner.getY(),
                                  juce::Colour(0xff0d131f), cx, inner.getBottom(), false);
        g.setGradientFill(grad);
        g.fillEllipse(inner);
        g.setColour(juce::Colour(0xff223147));
        g.drawEllipse(inner, 1.2f);

        // 6. Centered Digital Value Readout
        g.setColour(juce::Colour(0xfff1f5f9));
        float fontSize = 14.0f;
        if (sizeCategory == Size::Large) fontSize = 17.0f;
        else if (sizeCategory == Size::Small) fontSize = 11.5f;

        g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));
        juce::String valStr = (decimals == 0) ? juce::String(static_cast<int>(std::round(currentValue)))
                                              : juce::String(currentValue, decimals);
        g.drawText(valStr, inner, juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragStartValue = currentValue;
        dragStartPos = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float deltaY = dragStartPos.y - e.position.y;
        const double range = maxValue - minValue;
        const double step = range / 140.0;
        setValue(dragStartValue + deltaY * step, juce::sendNotificationSync);
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
    juce::Colour primaryColor;
    Size sizeCategory;
    int decimals;

    double dragStartValue = 0.0;
    juce::Point<float> dragStartPos;
};

// ==============================================================================
// 2. Stylized Diagonal Slash Bar Control (Smear, Stereo, Detune)
// ==============================================================================
class ManipulatorSlashControl final : public juce::Component {
public:
    ManipulatorSlashControl(const juce::String& name, double minV, double maxV, double defV)
        : label(name), minValue(minV), maxValue(maxV), defaultValue(defV), currentValue(defV) {
        setRepaintsOnMouseActivity(true);
    }

    void setValue(double v, juce::NotificationType notify = juce::dontSendNotification) {
        currentValue = std::clamp(v, minValue, maxValue);
        repaint();
        if (notify != juce::dontSendNotification && onValueChanged) onValueChanged(currentValue);
    }
    double getValue() const noexcept { return currentValue; }

    std::function<void(double)> onValueChanged;

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        const float norm = static_cast<float>((currentValue - minValue) / std::max(1e-4, maxValue - minValue));

        // Diagonal slash bars (////)
        constexpr int numSlashes = 14;
        const float slashW = bounds.getWidth() / static_cast<float>(numSlashes);

        for (int i = 0; i < numSlashes; ++i) {
            const float x = bounds.getX() + i * slashW;
            const float barNorm = static_cast<float>(i) / static_cast<float>(numSlashes - 1);
            const bool active = (barNorm <= norm);

            g.setColour(active ? juce::Colour(0xff00e5ff) : juce::Colour(0xff1a2638));
            g.drawLine(x, bounds.getBottom() - 14.0f, x + 8.0f, bounds.getY() + 4.0f, active ? 2.5f : 1.5f);
        }

        // Label on bottom
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(label, bounds.removeFromBottom(16.0f), juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const float norm = std::clamp(e.position.x / getWidth(), 0.0f, 1.0f);
        setValue(minValue + norm * (maxValue - minValue), juce::sendNotificationSync);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const float norm = std::clamp(e.position.x / getWidth(), 0.0f, 1.0f);
        setValue(minValue + norm * (maxValue - minValue), juce::sendNotificationSync);
    }

private:
    juce::String label;
    double minValue;
    double maxValue;
    double defaultValue;
    double currentValue;
};

// ==============================================================================
// 3. Main Polyverse Manipulator Plugin Surface Component
// ==============================================================================
class ManipulatorPitchPanel final : public juce::Component, private juce::Timer {
public:
    explicit ManipulatorPitchPanel(nova::graviton::ManipulatorPitchEngine& engineRef)
        : engine(engineRef) {
        setOpaque(true);

        // ── 1. Preset Selector ────────────────────────────────────────────
        presetBtn.setButtonText("Mushroomize");
        presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff101724));
        presetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible(presetBtn);

        prevPresetBtn.setButtonText("<");
        prevPresetBtn.onClick = [this] { cyclePreset(-1); };
        addAndMakeVisible(prevPresetBtn);

        nextPresetBtn.setButtonText(">");
        nextPresetBtn.onClick = [this] { cyclePreset(1); };
        addAndMakeVisible(nextPresetBtn);

        // ── 2. Mode Switches (Poly, Low Detect) ────────────────────────────
        polyBtn.setButtonText("POLY");
        polyBtn.setClickingTogglesState(true);
        polyBtn.setToggleState(engine.getPolyMode(), juce::dontSendNotification);
        polyBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00e5ff));
        polyBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090d14));
        polyBtn.onClick = [this] { engine.setPolyMode(polyBtn.getToggleState()); };
        addAndMakeVisible(polyBtn);

        lowBtn.setButtonText("LOW");
        lowBtn.setClickingTogglesState(true);
        lowBtn.setToggleState(engine.getLowDetect(), juce::dontSendNotification);
        lowBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00e5ff));
        lowBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090d14));
        lowBtn.onClick = [this] { engine.setLowDetect(lowBtn.getToggleState()); };
        addAndMakeVisible(lowBtn);

        // ── 3. Primary Knobs (Pitch, Harmonics, Alternator) ───────────────
        knobPitch = std::make_unique<ManipulatorKnob>("PITCH", -24.0, 24.0, engine.getPitch(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Large, 2);
        knobPitch->onValueChanged = [this](double v) { engine.setPitch(v); };
        addAndMakeVisible(*knobPitch);

        knobHarmonics = std::make_unique<ManipulatorKnob>("HARMONICS", -2.0, 2.0, engine.getHarmonics(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Large, 3);
        knobHarmonics->onValueChanged = [this](double v) { engine.setHarmonics(v); };
        addAndMakeVisible(*knobHarmonics);

        knobAlternator = std::make_unique<ManipulatorKnob>("ALTERNATOR", 0.0, 12.0, engine.getAlternator(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Large, 2);
        knobAlternator->onValueChanged = [this](double v) { engine.setAlternator(v); };
        addAndMakeVisible(*knobAlternator);

        // ── 4. Formant, Ratio, FM, Octave & Snap ──────────────────────────
        knobFormant = std::make_unique<ManipulatorKnob>("FORMANT", -12.0, 12.0, engine.getFormant(), juce::Colour(0xff10b981), ManipulatorKnob::Size::Medium, 2);
        knobFormant->onValueChanged = [this](double v) { engine.setFormant(v); };
        addAndMakeVisible(*knobFormant);

        knobRatio = std::make_unique<ManipulatorKnob>("RATIO", 0.0, 2.0, engine.getRatio(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Medium, 3);
        knobRatio->onValueChanged = [this](double v) { engine.setRatio(v); };
        addAndMakeVisible(*knobRatio);

        snapBtn.setButtonText("SNAP");
        snapBtn.setClickingTogglesState(true);
        snapBtn.setToggleState(engine.getSnap(), juce::dontSendNotification);
        snapBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00e5ff));
        snapBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090d14));
        snapBtn.onClick = [this] { engine.setSnap(snapBtn.getToggleState()); };
        addAndMakeVisible(snapBtn);

        knobFm = std::make_unique<ManipulatorKnob>("FM", 0.0, 24.0, engine.getFm(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Medium, 2);
        knobFm->onValueChanged = [this](double v) { engine.setFm(v); };
        addAndMakeVisible(*knobFm);

        knobOctave = std::make_unique<ManipulatorKnob>("OCTAVE", -3.0, 3.0, engine.getOctave(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Medium, 3);
        knobOctave->onValueChanged = [this](double v) { engine.setOctave(v); };
        addAndMakeVisible(*knobOctave);

        // ── 5. Glide, Smear, Stereo & Detune ──────────────────────────────
        knobGlide = std::make_unique<ManipulatorKnob>("GLIDE", 0.0, 500.0, engine.getGlide(), juce::Colour(0xff00e5ff), ManipulatorKnob::Size::Small, 0);
        knobGlide->onValueChanged = [this](double v) { engine.setGlide(v); };
        addAndMakeVisible(*knobGlide);

        slashSmear = std::make_unique<ManipulatorSlashControl>("Smear", 0.0, 100.0, engine.getSmear());
        slashSmear->onValueChanged = [this](double v) { engine.setSmear(v); };
        addAndMakeVisible(*slashSmear);

        slashStereo = std::make_unique<ManipulatorSlashControl>("Stereo", 0.0, 100.0, engine.getStereo());
        slashStereo->onValueChanged = [this](double v) { engine.setStereo(v); };
        addAndMakeVisible(*slashStereo);

        slashDetune = std::make_unique<ManipulatorSlashControl>("Detune", 0.0, 50.0, engine.getDetune());
        slashDetune->onValueChanged = [this](double v) { engine.setDetune(v); };
        addAndMakeVisible(*slashDetune);

        // ── 6. Advanced Modulation Panels ─────────────────────────────────
        // Panel 1: Meta Knob
        metaKnobDial = std::make_unique<ManipulatorKnob>("Mushroomize", 0.0, 100.0, engine.getMetaKnob(), juce::Colour(0xff10b981), ManipulatorKnob::Size::Large, 1);
        metaKnobDial->onValueChanged = [this](double v) { engine.setMetaKnob(v); };
        addAndMakeVisible(*metaKnobDial);

        // Panel 3: Sequencer Knobs & Button
        seqRndmBtn.setButtonText("rndm");
        seqRndmBtn.onClick = [this] { engine.randomizeSeq(); repaint(); };
        addAndMakeVisible(seqRndmBtn);

        knobSeqRate = std::make_unique<ManipulatorKnob>("Rate", 0.1, 10.0, engine.getSeqRate(), juce::Colour(0xfff59e0b), ManipulatorKnob::Size::Small, 1);
        knobSeqRate->onValueChanged = [this](double v) { engine.setSeqRate(v); };
        addAndMakeVisible(*knobSeqRate);

        knobSeqShape = std::make_unique<ManipulatorKnob>("Shape", 0.0, 5.0, engine.getSeqShape(), juce::Colour(0xfff59e0b), ManipulatorKnob::Size::Small, 2);
        knobSeqShape->onValueChanged = [this](double v) { engine.setSeqShape(v); };
        addAndMakeVisible(*knobSeqShape);

        setSize(880, 880);
        startTimerHz(30);
    }

    ~ManipulatorPitchPanel() override {
        stopTimer();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // 1. Dark Charcoal Background
        g.fillAll(juce::Colour(0xff0e131d));

        // 2. Outer Border
        g.setColour(juce::Colour(0xff1c2637));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 10.0f, 1.5f);

        // 3. Top Logos & Header
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("INFECTED\nMUSHROOM", 28, 16, 90, 28, juce::Justification::topLeft);
        g.drawText("POLYVERSE", static_cast<int>(bounds.getWidth() - 110.0f), 18, 85, 20, juce::Justification::centredRight);

        // Stylized MANIPULATOR Brush Logo
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(32.0f, juce::Font::bold)));
        g.drawText("MANIPULATOR", 0, 14, static_cast<int>(bounds.getWidth()), 36, juce::Justification::centred);

        // 4. Side Indicators (MIDI & DETECT)
        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("MIDI", 30, 142, 40, 14, juce::Justification::centred);
        g.drawText("DETECT", 30, 222, 44, 14, juce::Justification::centred);

        // 5. Right Side DRY/WET Meter
        const auto dwBounds = juce::Rectangle<float>(bounds.getWidth() - 65.0f, 110.0f, 32.0f, 96.0f);
        g.setColour(juce::Colour(0xff141c2a));
        g.fillRoundedRectangle(dwBounds, 4.0f);
        g.setColour(juce::Colour(0xff25334a));
        g.drawRoundedRectangle(dwBounds, 4.0f, 1.0f);

        // Segmented LED Bars
        const float wetRatio = static_cast<float>(engine.getDryWet() * 0.01);
        constexpr int numLeds = 14;
        for (int i = 0; i < numLeds; ++i) {
            const float ledY = dwBounds.getBottom() - 6.0f - (i * 6.0f);
            const bool lit = (static_cast<float>(i) / numLeds) <= wetRatio;
            g.setColour(lit ? juce::Colour(0xff00e5ff) : juce::Colour(0xff182233));
            g.fillRect(dwBounds.getX() + 4.0f, ledY, dwBounds.getWidth() - 8.0f, 3.5f);
        }

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(juce::String(static_cast<int>(engine.getDryWet())) + "%",
                   static_cast<int>(dwBounds.getX()), static_cast<int>(dwBounds.getBottom() + 4.0f),
                   static_cast<int>(dwBounds.getWidth()), 16, juce::Justification::centred);
        g.drawText("DRY/WET", static_cast<int>(dwBounds.getX() - 10.0f), static_cast<int>(dwBounds.getBottom() + 20.0f),
                   static_cast<int>(dwBounds.getWidth() + 20.0f), 16, juce::Justification::centred);

        // 6. Draw 4 Bottom Modulation Cards
        auto drawModCard = [&](juce::Rectangle<int> r, const juce::String& cardTitle, juce::Colour cCol) {
            g.setColour(juce::Colour(0xff101622));
            g.fillRoundedRectangle(r.toFloat(), 6.0f);
            g.setColour(cCol.withAlpha(0.6f));
            g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);

            g.setColour(cCol);
            g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
            g.drawText(cardTitle, r.getX() + 10, r.getY() + 8, r.getWidth() - 20, 16, juce::Justification::topLeft);
        };

        drawModCard(cardMetaBounds, "META KNOB", juce::Colour(0xff10b981));
        drawModCard(cardMidiBounds, "MIDI", juce::Colour(0xffd946ef));
        drawModCard(cardSeqBounds, "SEQUENCER", juce::Colour(0xfff59e0b));
        drawModCard(cardAdsrBounds, "ADSR", juce::Colour(0xffc084fc));

        // Draw MIDI sources inside MIDI card
        const char* midiList[5] = { "Note", "Velocity", "Pitch Wheel", "Mod Wheel", "After Touch" };
        for (int i = 0; i < 5; ++i) {
            const int my = cardMidiBounds.getY() + 32 + (i * 20);
            const bool selected = (i == 1); // Velocity highlighted
            if (selected) {
                g.setColour(juce::Colour(0x30d946ef));
                g.fillRoundedRectangle(static_cast<float>(cardMidiBounds.getX() + 12), static_cast<float>(my - 2), static_cast<float>(cardMidiBounds.getWidth() - 24), 18.0f, 4.0f);
                g.setColour(juce::Colour(0xffd946ef));
                g.drawRoundedRectangle(static_cast<float>(cardMidiBounds.getX() + 12), static_cast<float>(my - 2), static_cast<float>(cardMidiBounds.getWidth() - 24), 18.0f, 4.0f, 1.0f);
            }
            g.setColour(selected ? juce::Colour(0xfff472b6) : juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(midiList[i], cardMidiBounds.getX() + 20, my, cardMidiBounds.getWidth() - 40, 16, juce::Justification::centred);
        }

        // Draw Sequencer Step Ladder Pattern
        juce::Path seqPath;
        const float sqX = static_cast<float>(cardSeqBounds.getX() + 14);
        const float sqY = static_cast<float>(cardSeqBounds.getY() + 38);
        const float sqW = static_cast<float>(cardSeqBounds.getWidth() - 28);
        const float sqH = 46.0f;

        seqPath.startNewSubPath(sqX, sqY + sqH * 0.5f);
        constexpr int steps = 8;
        const float stepW = sqW / steps;
        for (int s = 0; s < steps; ++s) {
            const float h = sqY + (0.2f + 0.6f * ((s % 3) * 0.4f)) * sqH;
            seqPath.lineTo(sqX + s * stepW, h);
            seqPath.lineTo(sqX + (s + 1) * stepW, h);
        }
        g.setColour(juce::Colour(0xfff59e0b));
        g.strokePath(seqPath, juce::PathStrokeType(2.0f));

        // Draw ADSR Envelope Curve
        juce::Path adsrPath;
        const float ax = static_cast<float>(cardAdsrBounds.getX() + 16);
        const float ay = static_cast<float>(cardAdsrBounds.getY() + 36);
        const float aw = static_cast<float>(cardAdsrBounds.getWidth() - 32);
        const float ah = 54.0f;

        adsrPath.startNewSubPath(ax, ay + ah);
        adsrPath.lineTo(ax + aw * 0.25f, ay + 6.0f); // Attack
        adsrPath.lineTo(ax + aw * 0.50f, ay + ah * 0.45f); // Decay
        adsrPath.lineTo(ax + aw * 0.75f, ay + ah * 0.45f); // Sustain
        adsrPath.lineTo(ax + aw, ay + ah); // Release
        g.setColour(juce::Colour(0xffc084fc));
        g.strokePath(adsrPath, juce::PathStrokeType(2.5f));

        // ADSR Numeric labels (A 86, D 20, S 50, R 64)
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        const char* adsrLabels[4] = { "A 86", "D 20", "S 50", "R 64" };
        const float colW = aw / 4.0f;
        for (int i = 0; i < 4; ++i) {
            g.drawText(adsrLabels[i], static_cast<int>(ax + i * colW), static_cast<int>(ay + ah + 4.0f), static_cast<int>(colW), 14, juce::Justification::centred);
        }
    }

    void resized() override {
        const int w = getWidth();
        const int h = getHeight();

        if (h < 380) {
            // Widescreen Embedded Console Mode (3 Horizontal Studio Zones)
            const int pW = 180;
            const int pX = (w - pW) / 2;
            prevPresetBtn.setBounds(pX - 26, 4, 22, 20);
            presetBtn.setBounds(pX, 4, pW, 20);
            nextPresetBtn.setBounds(pX + pW + 4, 4, 22, 20);

            const int mainY = 28;
            const int zoneW = (w - 32) / 3;

            // Zone 1 (Left): Pitch & Formants
            polyBtn.setBounds(12, mainY + 6, 40, 22);
            lowBtn.setBounds(12, mainY + 34, 40, 22);

            const int bigK = juce::jlimit(64, 82, h - mainY - 60);
            knobPitch->setBounds(60, mainY + 4, bigK, bigK + 16);
            knobAlternator->setBounds(60 + bigK + 10, mainY + 4, bigK, bigK + 16);

            const int smK = 48;
            const int smY = mainY + bigK + 22;
            knobFormant->setBounds(60, smY, smK, smK + 14);
            knobRatio->setBounds(60 + bigK + 10, smY, smK, smK + 14);

            // Zone 2 (Center): Harmonics & Modulation
            const int cx = 12 + zoneW;
            knobHarmonics->setBounds(cx + (zoneW - bigK) / 2, mainY + 4, bigK, bigK + 16);
            snapBtn.setBounds(cx + (zoneW - 44) / 2, mainY + bigK + 22, 44, 20);

            knobFm->setBounds(cx + 20, smY, smK, smK + 14);
            knobOctave->setBounds(cx + zoneW - smK - 20, smY, smK, smK + 14);

            // Zone 3 (Right): Glide, Sliders & Sequencer
            const int rx = 12 + zoneW * 2;
            knobGlide->setBounds(rx + 10, mainY + 6, 48, 56);

            const int slW = (zoneW - 80) / 3;
            slashSmear->setBounds(rx + 65, mainY + 6, slW, 52);
            slashStereo->setBounds(rx + 65 + slW + 6, mainY + 6, slW, 52);
            slashDetune->setBounds(rx + 65 + (slW + 6) * 2, mainY + 6, slW, 52);

            const int cardY = mainY + 66;
            const int cardH = h - cardY - 6;
            const int cardW = (zoneW - 16) / 2;

            cardMetaBounds = juce::Rectangle<int>(rx + 8, cardY, cardW, cardH);
            metaKnobDial->setBounds(cardMetaBounds.reduced(4));

            cardSeqBounds = juce::Rectangle<int>(rx + 8 + cardW + 8, cardY, cardW, cardH);
            seqRndmBtn.setBounds(cardSeqBounds.getRight() - 40, cardSeqBounds.getY() + 4, 36, 18);
            const int seqKw = (cardW - 12) / 2;
            knobSeqRate->setBounds(cardSeqBounds.getX() + 4, cardSeqBounds.getY() + 24, seqKw, 48);
            knobSeqShape->setBounds(cardSeqBounds.getX() + 4 + seqKw, cardSeqBounds.getY() + 24, seqKw, 48);
            return;
        }

        // Standalone Preset Header
        const int pW = 210;
        const int pX = (w - pW) / 2;
        prevPresetBtn.setBounds(pX - 30, 56, 26, 24);
        presetBtn.setBounds(pX, 56, pW, 24);
        nextPresetBtn.setBounds(pX + pW + 4, 56, 26, 24);

        // 2. Mode Switches (Poly, Low)
        polyBtn.setBounds(26, 110, 46, 26);
        lowBtn.setBounds(26, 190, 46, 26);

        // 3. Primary Knobs (Pitch, Harmonics, Alternator)
        const int bigKnobSize = 138;
        const int centerKnobY = 96;
        const int cx = w / 2;

        knobHarmonics->setBounds(cx - bigKnobSize / 2, centerKnobY, bigKnobSize, bigKnobSize + 20);
        knobPitch->setBounds(cx - bigKnobSize / 2 - 170, centerKnobY + 16, bigKnobSize, bigKnobSize + 20);
        knobAlternator->setBounds(cx + bigKnobSize / 2 + 32, centerKnobY + 16, bigKnobSize, bigKnobSize + 20);

        // 4. Secondary Knobs (Formant, Ratio, Snap, FM, Octave)
        const int medKnobSize = 104;
        const int medY = centerKnobY + bigKnobSize + 14;

        knobFormant->setBounds(cx - medKnobSize / 2 - 250, medY - 14, medKnobSize, medKnobSize + 20);
        knobRatio->setBounds(cx - medKnobSize / 2 - 100, medY + 6, medKnobSize, medKnobSize + 20);

        snapBtn.setBounds(cx - 24, medY + 40, 48, 24);

        knobFm->setBounds(cx + medKnobSize / 2 - 4, medY + 6, medKnobSize, medKnobSize + 20);
        knobOctave->setBounds(cx + medKnobSize / 2 + 146, medY - 14, medKnobSize, medKnobSize + 20);

        // 5. Glide, Smear, Stereo & Detune Strip
        const int stripY = medY + medKnobSize + 36;
        knobGlide->setBounds(32, stripY, 68, 80);

        const int slashW = 180;
        slashSmear->setBounds(120, stripY + 6, slashW, 64);
        slashStereo->setBounds(120 + slashW + 16, stripY + 6, slashW, 64);
        slashDetune->setBounds(120 + (slashW + 16) * 2, stripY + 6, slashW, 64);

        // 6. Advanced Modulation Bottom Panels (4 cards)
        const int cardY = stripY + 92;
        const int cardH = 176;
        const int cardW = (w - 56 - 24) / 4;

        cardMetaBounds = juce::Rectangle<int>(28, cardY, cardW, cardH);
        cardMidiBounds = juce::Rectangle<int>(28 + cardW + 8, cardY, cardW, cardH);
        cardSeqBounds  = juce::Rectangle<int>(28 + (cardW + 8) * 2, cardY, cardW, cardH);
        cardAdsrBounds = juce::Rectangle<int>(28 + (cardW + 8) * 3, cardY, cardW, cardH);

        // Position components inside cards
        metaKnobDial->setBounds(cardMetaBounds.getX() + (cardW - 110) / 2, cardMetaBounds.getY() + 30, 110, 130);

        seqRndmBtn.setBounds(cardSeqBounds.getRight() - 52, cardSeqBounds.getY() + 8, 44, 20);
        const int seqKnobW = (cardW - 24) / 2;
        knobSeqRate->setBounds(cardSeqBounds.getX() + 8, cardSeqBounds.getY() + 94, seqKnobW, 74);
        knobSeqShape->setBounds(cardSeqBounds.getX() + 8 + seqKnobW, cardSeqBounds.getY() + 94, seqKnobW, 74);
    }

private:
    void showPresetMenu() {
        juce::PopupMenu m;
        m.addSectionHeader("MANIPULATOR FACTORY PRESETS");
        m.addItem(1, "01 \xc2\xb7 Mushroomize");
        m.addItem(2, "02 \xc2\xb7 Alien Robot Vox");
        m.addItem(3, "03 \xc2\xb7 Harmonized Octaves");
        m.addItem(4, "04 \xc2\xb7 ADT Vocal Widener");
        m.addItem(5, "05 \xc2\xb7 Formant Monster");
        m.addItem(6, "06 \xc2\xb7 Sub-Bass Growl");
        m.addItem(7, "07 \xc2\xb7 Polyrhythmic Chopper");

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
            [this](int r) {
                if (r > 0) applyPreset(r);
            });
    }

    void cyclePreset(int delta) {
        const int num = 7;
        currentPreset = (currentPreset + delta + num) % num;
        applyPreset(currentPreset + 1);
    }

    void applyPreset(int index) {
        currentPreset = index - 1;
        switch (index) {
            case 1: // Mushroomize
                presetBtn.setButtonText("Mushroomize");
                if (knobPitch) knobPitch->setValue(0.0, juce::sendNotificationSync);
                if (knobHarmonics) knobHarmonics->setValue(-1.0, juce::sendNotificationSync);
                if (knobAlternator) knobAlternator->setValue(6.0, juce::sendNotificationSync);
                if (knobFormant) knobFormant->setValue(7.0, juce::sendNotificationSync);
                if (knobRatio) knobRatio->setValue(0.5, juce::sendNotificationSync);
                if (knobFm) knobFm->setValue(13.0, juce::sendNotificationSync);
                if (knobOctave) knobOctave->setValue(-1.0, juce::sendNotificationSync);
                break;
            case 2: // Alien Robot Vox
                presetBtn.setButtonText("Alien Robot Vox");
                if (knobPitch) knobPitch->setValue(-5.0, juce::sendNotificationSync);
                if (knobFormant) knobFormant->setValue(9.5, juce::sendNotificationSync);
                if (knobFm) knobFm->setValue(20.0, juce::sendNotificationSync);
                break;
            case 3: // Harmonized Octaves
                presetBtn.setButtonText("Harmonized Octaves");
                if (knobPitch) knobPitch->setValue(12.0, juce::sendNotificationSync);
                if (knobOctave) knobOctave->setValue(1.0, juce::sendNotificationSync);
                if (knobHarmonics) knobHarmonics->setValue(1.5, juce::sendNotificationSync);
                break;
            case 4: // ADT Vocal Widener
                presetBtn.setButtonText("ADT Vocal Widener");
                if (knobPitch) knobPitch->setValue(0.0, juce::sendNotificationSync);
                if (slashStereo) slashStereo->setValue(85.0, juce::sendNotificationSync);
                if (slashDetune) slashDetune->setValue(24.0, juce::sendNotificationSync);
                break;
            default:
                break;
        }
    }

    void timerCallback() override {
        // Meter readouts or live animation
        repaint(getLocalBounds().removeFromRight(70));
    }

    nova::graviton::ManipulatorPitchEngine& engine;
    int currentPreset = 0;

    juce::Rectangle<int> cardMetaBounds;
    juce::Rectangle<int> cardMidiBounds;
    juce::Rectangle<int> cardSeqBounds;
    juce::Rectangle<int> cardAdsrBounds;

    // Header & Presets
    juce::TextButton presetBtn;
    juce::TextButton prevPresetBtn;
    juce::TextButton nextPresetBtn;

    // Mode Buttons
    juce::TextButton polyBtn;
    juce::TextButton lowBtn;

    // Primary Large Knobs
    std::unique_ptr<ManipulatorKnob> knobPitch;
    std::unique_ptr<ManipulatorKnob> knobHarmonics;
    std::unique_ptr<ManipulatorKnob> knobAlternator;

    // Secondary Knobs
    std::unique_ptr<ManipulatorKnob> knobFormant;
    std::unique_ptr<ManipulatorKnob> knobRatio;
    juce::TextButton snapBtn;
    std::unique_ptr<ManipulatorKnob> knobFm;
    std::unique_ptr<ManipulatorKnob> knobOctave;

    // Glide, Smear, Stereo, Detune
    std::unique_ptr<ManipulatorKnob> knobGlide;
    std::unique_ptr<ManipulatorSlashControl> slashSmear;
    std::unique_ptr<ManipulatorSlashControl> slashStereo;
    std::unique_ptr<ManipulatorSlashControl> slashDetune;

    // Modulation Section
    std::unique_ptr<ManipulatorKnob> metaKnobDial;
    juce::TextButton seqRndmBtn;
    std::unique_ptr<ManipulatorKnob> knobSeqRate;
    std::unique_ptr<ManipulatorKnob> knobSeqShape;
};

} // namespace nova
