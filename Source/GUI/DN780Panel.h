#pragma once

#include "Effects/GravitonPlateReverb.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace nova {

// ==============================================================================
// DN780Panel — Dedicated Klark Teknik DN780 Digital Reverb Processor (Slot 16)
// Separated from EMT140Panel (Page 3) -> Dedicated Page 22.
//   - Classic Klark Teknik DN780 charcoal-slate rack faceplate + amber/cyan LEDs
//   - 5 Factory Programs: Crisp Plate, Bright Hall, Studio Room, Tight Chamber,
//     Vocal Air
//   - Real-time Reverb Decay Envelope & Predelay Visualizer
//   - 10 Studio Knobs: DECAY, PRE-DELAY, DAMPING, LOW CUT, EQ LOW, EQ HIGH,
//     MOD RATE, MOD DEPTH, WIDTH, MIX
//   - Toggles: POWER, EQ ON/OFF, WET ONLY
// ==============================================================================
class DN780Panel final : public juce::Component, private juce::Timer {
public:
  explicit DN780Panel(nova::graviton::EMT140PlateReverb &eng)
      : engine(&eng) {
    setOpaque(true);

    static constexpr const char *kProgNames[5] = {
        "01 CRISP PLATE", "02 BRIGHT HALL", "03 STUDIO ROOM",
        "04 TIGHT CHAMBER", "05 VOCAL AIR"};
    for (int i = 0; i < 5; ++i) {
      btnProg[static_cast<size_t>(i)].setButtonText(kProgNames[i]);
      btnProg[static_cast<size_t>(i)].onClick = [this, i] {
        applyProgram(i);
      };
      addAndMakeVisible(btnProg[static_cast<size_t>(i)]);
    }

    btnPower.setButtonText("POWER");
    btnPower.onClick = [this] {
      if (!engine) return;
      engine->setPower(!engine->getPower());
      updateButtons();
    };
    btnEq.setButtonText("EQ");
    btnEq.onClick = [this] {
      eqEnabledLocal = !eqEnabledLocal;
      if (engine) engine->setEqEnabled(eqEnabledLocal);
      updateButtons();
    };
    btnWetOnly.setButtonText("WET ONLY");
    btnWetOnly.onClick = [this] {
      if (!engine) return;
      engine->setWetOnly(!engine->getWetOnly());
      updateButtons();
    };
    addAndMakeVisible(btnPower);
    addAndMakeVisible(btnEq);
    addAndMakeVisible(btnWetOnly);

    auto mkKnob = [this](std::unique_ptr<NovaRotaryKnob> &k, const char *name,
                         const char *unit, float lo, float hi, float def,
                         bool bipolar, juce::Colour c, int dec) {
      k = std::make_unique<NovaRotaryKnob>(name, unit, lo, hi, def, bipolar, c);
      k->setDisplayDecimals(dec);
      addAndMakeVisible(*k);
    };

    const juce::Colour cyan(0xff22d3ee);
    const juce::Colour amber(0xfff59e0b);
    mkKnob(knobDecay, "DECAY", "s", 0.5f, 12.0f, 2.2f, false, amber, 2);
    mkKnob(knobPreDelay, "PRE-DELAY", "ms", 0.0f, 500.0f, 18.0f, false, cyan, 1);
    mkKnob(knobDamping, "DAMPING", "Hz", 1000.0f, 16000.0f, 9500.0f, false, cyan, 0);
    mkKnob(knobLowCut, "LOW CUT", "Hz", 20.0f, 500.0f, 90.0f, false, cyan, 0);
    mkKnob(knobWidth, "WIDTH", "%", 0.0f, 200.0f, 115.0f, false, amber, 0);

    mkKnob(knobEqLow, "EQ LOW", "dB", -12.0f, 12.0f, -1.0f, true, cyan, 1);
    mkKnob(knobEqHigh, "EQ HIGH", "dB", -12.0f, 12.0f, 1.5f, true, cyan, 1);
    mkKnob(knobModRate, "MOD RATE", "Hz", 0.01f, 1.0f, 0.25f, false, cyan, 2);
    mkKnob(knobModDepth, "MOD DEPTH", "", 0.0f, 10.0f, 2.5f, false, cyan, 1);
    mkKnob(knobMix, "MIX", "%", 0.0f, 100.0f, 35.0f, false, amber, 0);

    knobDamping->setLogarithmic(true);
    knobLowCut->setLogarithmic(true);

    knobDecay->onValueChanged = [this](float v) {
      if (engine) engine->setDecay(v);
      repaint();
    };
    knobPreDelay->onValueChanged = [this](float v) {
      if (engine) engine->setPreDelay(v);
      repaint();
    };
    knobDamping->onValueChanged = [this](float v) {
      if (engine) engine->setDampingHz(v);
    };
    knobLowCut->onValueChanged = [this](float v) {
      if (engine) engine->setLowCutHz(v);
    };
    knobWidth->onValueChanged = [this](float v) {
      if (engine) engine->setStereoWidth(v * 0.01);
    };
    knobEqLow->onValueChanged = [this](float v) {
      if (engine) engine->setEqLowGainDb(v);
    };
    knobEqHigh->onValueChanged = [this](float v) {
      if (engine) engine->setEqHighGainDb(v);
    };
    knobModRate->onValueChanged = [this](float v) {
      if (engine) engine->setModRateHz(v);
    };
    knobModDepth->onValueChanged = [this](float v) {
      if (engine) engine->setModDepth(v);
    };
    knobMix->onValueChanged = [this](float v) {
      if (engine) engine->setMix(v * 0.01);
    };

    syncFromEngine();
    startTimerHz(25);
  }

  ~DN780Panel() override { stopTimer(); }

  void setEngine(nova::graviton::EMT140PlateReverb &eng) {
    engine = &eng;
    engine->setMode(1); // Ensure KlarkTeknik_DN780 mode
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    if (!engine) return;
    knobDecay->setValue(static_cast<float>(engine->getDecay()), juce::dontSendNotification);
    knobPreDelay->setValue(static_cast<float>(engine->getPreDelay()), juce::dontSendNotification);
    knobDamping->setValue(static_cast<float>(engine->getDampingHz()), juce::dontSendNotification);
    knobLowCut->setValue(static_cast<float>(engine->getLowCutHz()), juce::dontSendNotification);
    knobWidth->setValue(static_cast<float>(engine->getStereoWidth() * 100.0), juce::dontSendNotification);
    knobEqLow->setValue(static_cast<float>(engine->getEqLowGainDb()), juce::dontSendNotification);
    knobEqHigh->setValue(static_cast<float>(engine->getEqHighGainDb()), juce::dontSendNotification);
    knobModRate->setValue(static_cast<float>(engine->getModRateHz()), juce::dontSendNotification);
    knobModDepth->setValue(static_cast<float>(engine->getModDepth()), juce::dontSendNotification);
    knobMix->setValue(static_cast<float>(engine->getMix() * 100.0), juce::dontSendNotification);
    updateButtons();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0b0f17));

    // Klark Teknik charcoal-slate rack faceplate
    juce::ColourGradient bg(juce::Colour(0xff1a2232), 0.0f, 0.0f,
                            juce::Colour(0xff101622), 0.0f, b.getBottom(),
                            false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(b.reduced(6.0f), 7.0f);
    g.setColour(juce::Colour(0xff2c3a52));
    g.drawRoundedRectangle(b.reduced(6.0f), 7.0f, 1.4f);

    // Header
    g.setColour(juce::Colour(0xfff59e0b));
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("KLARK TEKNIK DN780  //  DIGITAL REVERB PROCESSOR",
               titleRect.toNearestInt(), juce::Justification::centredLeft);

    // Decay & Predelay LED Display
    drawEnvelopeDisplay(g, displayRect);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(14);
    auto topBar = b.removeFromTop(28);
    btnPower.setBounds(topBar.removeFromRight(64).reduced(2));
    btnWetOnly.setBounds(topBar.removeFromRight(78).reduced(2));
    btnEq.setBounds(topBar.removeFromRight(48).reduced(2));
    titleRect = topBar.toFloat();

    b.removeFromTop(6);
    auto progRow = b.removeFromTop(26);
    const int pw = progRow.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
      btnProg[static_cast<size_t>(i)].setBounds(progRow.removeFromLeft(pw).reduced(2));

    b.removeFromTop(6);
    displayRect = b.removeFromTop(juce::jlimit(85, 145, b.getHeight() * 30 / 100)).toFloat();
    b.removeFromTop(8);

    auto row1 = b.removeFromTop(b.getHeight() / 2);
    auto row2 = b;
    const int kw = row1.getWidth() / 5;
    knobDecay->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobPreDelay->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobDamping->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobLowCut->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobWidth->setBounds(row1.reduced(2));

    knobEqLow->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobEqHigh->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobModRate->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobModDepth->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobMix->setBounds(row2.reduced(2));
  }

private:
  void timerCallback() override { repaint(); }

  void applyProgram(int idx) {
    if (!engine) return;
    currentProg = idx;
    engine->setMode(1); // KlarkTeknik_DN780
    switch (idx) {
    case 0: // Crisp Plate
      engine->setPlate(2); engine->setDecay(2.1); engine->setPreDelay(14.0);
      engine->setDampingHz(11000.0); engine->setLowCutHz(110.0); engine->setMix(0.32);
      break;
    case 1: // Bright Hall
      engine->setPlate(1); engine->setDecay(3.4); engine->setPreDelay(28.0);
      engine->setDampingHz(9500.0); engine->setLowCutHz(85.0); engine->setMix(0.38);
      break;
    case 2: // Studio Room
      engine->setPlate(0); engine->setDecay(0.95); engine->setPreDelay(8.0);
      engine->setDampingHz(7800.0); engine->setLowCutHz(100.0); engine->setMix(0.28);
      break;
    case 3: // Tight Chamber
      engine->setPlate(1); engine->setDecay(1.45); engine->setPreDelay(16.0);
      engine->setDampingHz(6800.0); engine->setLowCutHz(120.0); engine->setMix(0.30);
      break;
    case 4: // Vocal Air
      engine->setPlate(2); engine->setDecay(2.6); engine->setPreDelay(32.0);
      engine->setDampingHz(13500.0); engine->setLowCutHz(150.0); engine->setMix(0.34);
      break;
    }
    syncFromEngine();
  }

  void drawEnvelopeDisplay(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff060a10));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(juce::Colour(0xff233147));
    g.drawRoundedRectangle(r, 5.0f, 1.2f);
    if (!engine) return;

    const float decaySec = static_cast<float>(engine->getDecay());
    const float pdMs = static_cast<float>(engine->getPreDelay());
    auto plot = r.reduced(14.0f, 10.0f);

    const float totalMs = std::max(1500.0f, pdMs + decaySec * 1000.0f);
    const float pdX = plot.getX() + (pdMs / totalMs) * plot.getWidth();

    // Predelay gap marker
    g.setColour(juce::Colour(0x4422d3ee));
    g.fillRect(plot.getX(), plot.getY(), pdX - plot.getX(), plot.getHeight());

    // Exponential decay curve
    juce::Path env;
    env.startNewSubPath(pdX, plot.getBottom());
    env.lineTo(pdX, plot.getY() + 4.0f);
    const int steps = 80;
    for (int i = 1; i <= steps; ++i) {
      const float t = static_cast<float>(i) / steps;
      const float x = pdX + t * (plot.getRight() - pdX);
      const float y = plot.getBottom() - std::exp(-3.5f * t) * (plot.getHeight() - 4.0f);
      env.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xfff59e0b));
    g.strokePath(env, juce::PathStrokeType(2.2f));

    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("MODE: DN780 DIGITAL   |   DECAY: " + juce::String(decaySec, 2) +
                   " s   |   PRE-DELAY: " + juce::String(pdMs, 1) + " ms",
               r.reduced(10.0f, 4.0f).withHeight(16.0f).toNearestInt(),
               juce::Justification::centredRight);
  }

  void updateButtons() {
    if (!engine) return;
    auto style = [](juce::TextButton &b, bool on, juce::Colour onCol) {
      b.setColour(juce::TextButton::buttonColourId,
                  on ? onCol : juce::Colour(0xff1e293b));
      b.setColour(juce::TextButton::textColourOffId,
                  on ? juce::Colours::black : juce::Colour(0xff94a3b8));
    };
    style(btnPower, engine->getPower(), juce::Colour(0xff22d3ee));
    style(btnEq, eqEnabledLocal, juce::Colour(0xfff59e0b));
    style(btnWetOnly, engine->getWetOnly(), juce::Colour(0xff38bdf8));
    for (int i = 0; i < 5; ++i)
      style(btnProg[static_cast<size_t>(i)], i == currentProg,
            juce::Colour(0xfff59e0b));
  }

  nova::graviton::EMT140PlateReverb *engine = nullptr;
  int currentProg = 0;
  bool eqEnabledLocal = true;
  juce::Rectangle<float> titleRect, displayRect;

  std::array<juce::TextButton, 5> btnProg;
  juce::TextButton btnPower, btnEq, btnWetOnly;
  std::unique_ptr<NovaRotaryKnob> knobDecay, knobPreDelay, knobDamping,
      knobLowCut, knobWidth, knobEqLow, knobEqHigh, knobModRate, knobModDepth,
      knobMix;
};

} // namespace nova
