#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Effects/GravitonSCFChorus.h"
#include <array>
#include <cmath>
#include <functional>

namespace nova {

// ==============================================================================
// 1. Gold-Capped Hardware Rotary Knob for TC SCF Gold
// ==============================================================================
class SCFRotaryKnob final : public juce::Component {
public:
  SCFRotaryKnob(const juce::String &titleText, const juce::String &subText,
                double minV, double maxV, double defV)
      : title(titleText), subtitle(subText), minVal(minV), maxVal(maxV),
        value(defV), defaultVal(defV) {}

  std::function<void(double)> onValueChanged;
  std::function<juce::String(double)> valueFormatter;

  void setValue(double v, bool notify = false) {
    const double clamped = std::clamp(v, minVal, maxVal);
    if (std::abs(clamped - value) > 1e-6) {
      value = clamped;
      repaint();
      if (notify && onValueChanged)
        onValueChanged(value);
    }
  }
  double getValue() const noexcept { return value; }

  void setSubtitle(const juce::String &s) {
    if (subtitle != s) {
      subtitle = s;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth();
    const float h = b.getHeight();

    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfffacc15));
    g.drawText(title, 0, 0, static_cast<int>(w), 13,
               juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(7.8f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText(subtitle, 0, 12, static_cast<int>(w), 11,
               juce::Justification::centred);

    const float topReserve = 24.0f;
    const float botReserve = 16.0f;
    const float availH = std::max(26.0f, h - topReserve - botReserve);
    const float radius = std::min(w, availH) * 0.39f;
    const float cx = w * 0.5f;
    const float cy = topReserve + availH * 0.50f;

    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 0.75f;

    // Gold scale ticks
    for (int i = 0; i <= 12; ++i) {
      const float norm = static_cast<float>(i) / 12.0f;
      const float ang = startAngle + norm * (endAngle - startAngle);
      const float sinA = std::sin(ang);
      const float cosA = -std::cos(ang);
      const bool isMajor = (i % 2 == 0);
      const float r1 = radius * 1.08f;
      const float r2 = radius * (isMajor ? 1.24f : 1.16f);
      g.setColour(isMajor ? juce::Colour(0xfffacc15)
                          : juce::Colour(0xfffacc15).withAlpha(0.55f));
      g.drawLine(cx + sinA * r1, cy + cosA * r1, cx + sinA * r2, cy + cosA * r2,
                 isMajor ? 1.5f : 1.0f);
    }

    // Outer black knurled skirt
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(cx - radius + 1.5f, cy - radius + 2.5f, radius * 2.0f,
                  radius * 2.0f);
    g.setColour(juce::Colour(0xff14171c));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xffca8a04));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.3f);

    // Brushed Gold Cap
    const float capR = radius * 0.68f;
    juce::ColourGradient goldGrad(juce::Colour(0xfffef08a), cx - capR * 0.4f,
                                  cy - capR * 0.5f, juce::Colour(0xffb45309),
                                  cx + capR * 0.5f, cy + capR * 0.6f, false);
    g.setGradientFill(goldGrad);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    // Pointer notch
    const float normVal =
        static_cast<float>((value - minVal) / (maxVal - minVal));
    const float valAng = startAngle + normVal * (endAngle - startAngle);
    const float pSin = std::sin(valAng);
    const float pCos = -std::cos(valAng);
    g.setColour(juce::Colour(0xff090c10));
    g.drawLine(cx + pSin * (capR * 0.2f), cy + pCos * (capR * 0.2f),
               cx + pSin * (radius * 0.94f), cy + pCos * (radius * 0.94f), 2.5f);

    // Value readout
    g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfffef08a));
    const juce::String txt =
        valueFormatter ? valueFormatter(value) : juce::String(value, 2);
    g.drawText(txt, 0, static_cast<int>(h - 15.0f), static_cast<int>(w), 14,
               juce::Justification::centred);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    dragStartY = e.position.y;
    dragStartVal = value;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const double dy = static_cast<double>(dragStartY - e.position.y);
    const double sens = e.mods.isShiftDown() ? 400.0 : 130.0;
    setValue(dragStartVal + (dy / sens) * (maxVal - minVal), true);
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {
    setValue(defaultVal, true);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    const double step = (maxVal - minVal) * 0.03;
    setValue(value + (wheel.deltaY > 0.0f ? step : -step), true);
  }

private:
  juce::String title, subtitle;
  double minVal, maxVal, value, defaultVal;
  float dragStartY = 0.0f;
  double dragStartVal = 0.0;
};

// ==============================================================================
// 2. TCSCFPanel — TC Electronic SCF Gold Stereo Chorus / P.M. / Flanger UI
// ==============================================================================
class TCSCFPanel final : public juce::Component, private juce::Timer {
public:
  explicit TCSCFPanel(nova::graviton::TCSCFProcessor &proc)
      : engine(&proc),
        knobInputGain("INPUT GAIN", "0 .. +15 dB", 0.0, 15.0, 2.0),
        knobSpeed("SPEED", "0.1 .. 10 Hz", 0.1, 10.0, 0.85),
        knobWidth("WIDTH", "SWEEP 0 .. 6", 0.0, 6.0, 3.8),
        knobIntensity("INTENSITY", "CHORUS NOTCH DEPTH", 0.0, 6.0, 3.4) {

    const auto &presets = nova::graviton::TCSCFProcessor::getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
      presetCombo.addItem(juce::String(presets[i].category) + ": " +
                              juce::String(presets[i].name),
                          static_cast<int>(i + 1));
    }
    presetCombo.onChange = [this] {
      if (engine == nullptr)
        return;
      const int idx = presetCombo.getSelectedId() - 1;
      if (idx >= 0) {
        engine->loadPresetByIndex(idx);
        syncFromEngine();
      }
    };
    addAndMakeVisible(presetCombo);

    knobInputGain.valueFormatter = [](double v) {
      return "+" + juce::String(v, 1) + " dB";
    };
    knobInputGain.onValueChanged = [this](double v) {
      if (engine)
        engine->setInputGainDb(v);
    };
    addAndMakeVisible(knobInputGain);

    knobSpeed.valueFormatter = [](double v) {
      return juce::String(v, 2) + " Hz";
    };
    knobSpeed.onValueChanged = [this](double v) {
      if (engine)
        engine->setSpeed(v);
    };
    addAndMakeVisible(knobSpeed);

    knobWidth.valueFormatter = [](double v) { return juce::String(v, 1); };
    knobWidth.onValueChanged = [this](double v) {
      if (engine)
        engine->setWidthDial(v);
    };
    addAndMakeVisible(knobWidth);

    knobIntensity.valueFormatter = [](double v) { return juce::String(v, 1); };
    knobIntensity.onValueChanged = [this](double v) {
      if (engine)
        engine->setIntensityDial(v);
    };
    addAndMakeVisible(knobIntensity);

    btnChorus.setButtonText("CHORUS");
    btnChorus.onClick = [this] {
      if (engine) {
        engine->setMode(nova::graviton::SCFMode::Chorus);
        syncFromEngine();
      }
    };
    addAndMakeVisible(btnChorus);

    btnPitchMod.setButtonText("PITCH MOD (P.M.)");
    btnPitchMod.onClick = [this] {
      if (engine) {
        engine->setMode(nova::graviton::SCFMode::PitchMod);
        syncFromEngine();
      }
    };
    addAndMakeVisible(btnPitchMod);

    btnFlanger.setButtonText("FLANGER");
    btnFlanger.onClick = [this] {
      if (engine) {
        engine->setMode(nova::graviton::SCFMode::Flanger);
        syncFromEngine();
      }
    };
    addAndMakeVisible(btnFlanger);

    btnFbPolarity.setClickingTogglesState(true);
    btnFbPolarity.onClick = [this] {
      if (engine) {
        engine->setFeedbackInvertPolarity(btnFbPolarity.getToggleState());
        syncFromEngine();
      }
    };
    addAndMakeVisible(btnFbPolarity);

    btnExPhase.setButtonText("EX-PHASE");
    btnExPhase.setClickingTogglesState(true);
    btnExPhase.onClick = [this] {
      if (engine)
        engine->setOutputStereoInvert(btnExPhase.getToggleState());
    };
    addAndMakeVisible(btnExPhase);

    btnPower.setButtonText("POWER ON");
    btnPower.setClickingTogglesState(true);
    btnPower.onClick = [this] {
      if (engine) {
        engine->setPower(btnPower.getToggleState());
        syncFromEngine();
      }
    };
    addAndMakeVisible(btnPower);

    syncFromEngine();
    startTimerHz(25);
  }

  ~TCSCFPanel() override { stopTimer(); }

  void setEngine(nova::graviton::TCSCFProcessor &proc) {
    engine = &proc;
    syncFromEngine();
  }

  void syncFromEngine() {
    if (engine == nullptr)
      return;
    presetCombo.setSelectedId(engine->getCurrentPresetIndex() + 1,
                              juce::dontSendNotification);
    knobInputGain.setValue(engine->getInputGainDb(), false);
    knobSpeed.setValue(engine->getSpeed(), false);
    knobWidth.setValue(engine->getWidthDial(), false);
    knobIntensity.setValue(engine->getIntensityDial(), false);

    const auto m = engine->getMode();
    if (m == nova::graviton::SCFMode::Chorus)
      knobIntensity.setSubtitle("CHORUS NOTCH DEPTH");
    else if (m == nova::graviton::SCFMode::PitchMod)
      knobIntensity.setSubtitle("CHORUS <-> VIBRATO");
    else
      knobIntensity.setSubtitle("FLANGER REGEN / FB");

    const auto goldCol = juce::Colour(0xffca8a04);
    const auto idleCol = juce::Colour(0xff1e293b);
    btnChorus.setColour(juce::TextButton::buttonColourId,
                        m == nova::graviton::SCFMode::Chorus ? goldCol : idleCol);
    btnPitchMod.setColour(juce::TextButton::buttonColourId,
                          m == nova::graviton::SCFMode::PitchMod ? goldCol : idleCol);
    btnFlanger.setColour(juce::TextButton::buttonColourId,
                         m == nova::graviton::SCFMode::Flanger ? goldCol : idleCol);

    const bool inv = engine->getFeedbackInvertPolarity();
    btnFbPolarity.setToggleState(inv, juce::dontSendNotification);
    btnFbPolarity.setButtonText(inv ? "FB: - HOLLOW" : "FB: + JET");
    btnExPhase.setToggleState(engine->getOutputStereoInvert(),
                              juce::dontSendNotification);
    btnPower.setToggleState(engine->getPower(), juce::dontSendNotification);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0d0f12));
    g.setColour(juce::Colour(0xffca8a04));
    g.drawRoundedRectangle(b.reduced(3.0f), 6.0f, 1.8f);

    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfffacc15));
    g.drawText("TC ELECTRONIC // SCF GOLD STEREO CHORUS + PITCH MODULATOR & FLANGER",
               14, 6, 480, 22, juce::Justification::centredLeft);

    if (engine != nullptr) {
      const bool ovl = engine->isOverloadActive();
      g.setColour(ovl ? juce::Colour(0xffef4444) : juce::Colour(0xff3f1d1d));
      g.fillEllipse(18.0f, 36.0f, 10.0f, 10.0f);
      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
      g.setColour(juce::Colours::white);
      g.drawText("OVERLOAD", 32, 34, 65, 14, juce::Justification::centredLeft);

      const float dL = engine->getTelemetryDelayMsL();
      const float dR = engine->getTelemetryDelayMsR();
      const float cL = engine->getTelemetryPitchCentsL();
      g.setColour(juce::Colour(0xfffef08a));
      g.drawText("DLY L: " + juce::String(dL, 2) + " ms | R: " +
                     juce::String(dR, 2) + " ms | " +
                     juce::String("dPitch: ") + (cL >= 0.0f ? "+" : "") +
                     juce::String(cL, 1) + " cents",
                 110, 34, 340, 14, juce::Justification::centredLeft);
    }
  }

  void resized() override {
    auto area = getLocalBounds().reduced(10);
    auto topBar = area.removeFromTop(26);
    presetCombo.setBounds(topBar.removeFromRight(260));

    area.removeFromTop(22);
    auto botBar = area.removeFromBottom(30);
    const int btnW = botBar.getWidth() / 6;
    btnChorus.setBounds(botBar.removeFromLeft(btnW).reduced(2));
    btnPitchMod.setBounds(botBar.removeFromLeft(btnW).reduced(2));
    btnFlanger.setBounds(botBar.removeFromLeft(btnW).reduced(2));
    btnFbPolarity.setBounds(botBar.removeFromLeft(btnW).reduced(2));
    btnExPhase.setBounds(botBar.removeFromLeft(btnW).reduced(2));
    btnPower.setBounds(botBar.reduced(2));

    area.removeFromBottom(6);
    const int knobW = area.getWidth() / 4;
    knobInputGain.setBounds(area.removeFromLeft(knobW).reduced(4));
    knobSpeed.setBounds(area.removeFromLeft(knobW).reduced(4));
    knobWidth.setBounds(area.removeFromLeft(knobW).reduced(4));
    knobIntensity.setBounds(area.reduced(4));
  }

private:
  void timerCallback() override { repaint(); }

  nova::graviton::TCSCFProcessor *engine = nullptr;
  juce::ComboBox presetCombo;
  SCFRotaryKnob knobInputGain, knobSpeed, knobWidth, knobIntensity;
  juce::TextButton btnChorus, btnPitchMod, btnFlanger, btnFbPolarity,
      btnExPhase, btnPower;
};

} // namespace nova
