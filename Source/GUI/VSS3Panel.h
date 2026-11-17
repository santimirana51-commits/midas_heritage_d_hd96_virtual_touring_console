#pragma once

#include "Effects/GravitonEffectsRack.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace nova {

// ==============================================================================
// VSS3Panel — Dedicated TC Electronic VSS3 Stereo Source Reverb (Slot 23)
//   - Dark-slate & silver/cyan TC System 6000 faceplate
//   - 8 Factory Presets (<, ComboBox, >) + BYPASS
//   - 5 Space Types: HALL, ROOM, PLATE, AMBIENCE, CHAPEL
//   - Real-time Early Reflections + Reverb Tail Decay Visualizer + IN/OUT Meters
//   - 10 Studio Knobs: DECAY, ROOM SIZE, PRE-DELAY, HI-CUT, LOW DAMP,
//     EARLY LVL, TAIL LVL, MOD DEPTH, WIDTH, WET MIX
// ==============================================================================
class VSS3Panel final : public juce::Component, private juce::Timer {
public:
  explicit VSS3Panel(nova::graviton::VSSReverb &eng) : engine(&eng) {
    setOpaque(true);

    btnPrevPreset.setButtonText("<");
    btnNextPreset.setButtonText(">");
    btnBypass.setButtonText("BYPASS");
    for (int i = 0; i < nova::graviton::VSSReverb::kNumPresets; ++i)
      cbPreset.addItem(nova::graviton::VSSReverb::getPresetName(i), i + 1);
    cbPreset.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f172a));
    cbPreset.setColour(juce::ComboBox::textColourId, juce::Colour(0xff38bdf8));
    cbPreset.onChange = [this] {
      const int idx = cbPreset.getSelectedId() - 1;
      if (engine && idx >= 0) {
        engine->applyPreset(idx);
        syncFromEngine();
      }
    };
    btnPrevPreset.onClick = [this] {
      if (!engine) return;
      const int n = nova::graviton::VSSReverb::kNumPresets;
      engine->applyPreset((engine->getCurrentPresetIndex() - 1 + n) % n);
      syncFromEngine();
    };
    btnNextPreset.onClick = [this] {
      if (!engine) return;
      const int n = nova::graviton::VSSReverb::kNumPresets;
      engine->applyPreset((engine->getCurrentPresetIndex() + 1) % n);
      syncFromEngine();
    };
    btnBypass.onClick = [this] {
      if (!engine) return;
      engine->setBypassed(!engine->isBypassed());
      updateButtons();
    };
    addAndMakeVisible(btnPrevPreset);
    addAndMakeVisible(cbPreset);
    addAndMakeVisible(btnNextPreset);
    addAndMakeVisible(btnBypass);

    static constexpr const char *kSpaces[5] = {"HALL", "ROOM", "PLATE", "AMBIENCE", "CHAPEL"};
    for (int i = 0; i < 5; ++i) {
      btnSpace[static_cast<size_t>(i)].setButtonText(kSpaces[i]);
      btnSpace[static_cast<size_t>(i)].onClick = [this, i] {
        if (engine) engine->setSpaceType(i);
        updateButtons();
        repaint();
      };
      addAndMakeVisible(btnSpace[static_cast<size_t>(i)]);
    }

    auto mkKnob = [this](std::unique_ptr<NovaRotaryKnob> &k, const char *name,
                         const char *unit, float lo, float hi, float def,
                         juce::Colour c, int dec) {
      k = std::make_unique<NovaRotaryKnob>(name, unit, lo, hi, def, false, c);
      k->setDisplayDecimals(dec);
      addAndMakeVisible(*k);
    };

    const juce::Colour cyan(0xff38bdf8);
    const juce::Colour silver(0xffcbd5e1);
    mkKnob(knobDecay, "DECAY", "s", 0.1f, 20.0f, 2.0f, cyan, 2);
    mkKnob(knobSize, "ROOM SIZE", "", 0.1f, 2.0f, 0.85f, cyan, 2);
    mkKnob(knobPreDelay, "PRE-DELAY", "ms", 0.0f, 300.0f, 20.0f, cyan, 1);
    mkKnob(knobHiCut, "HI-CUT", "Hz", 500.0f, 20000.0f, 9500.0f, silver, 0);
    mkKnob(knobLowDamp, "LOW DAMP", "x", 0.2f, 2.0f, 1.0f, silver, 2);

    mkKnob(knobEarly, "EARLY LVL", "%", 0.0f, 100.0f, 65.0f, silver, 0);
    mkKnob(knobTail, "TAIL LVL", "%", 0.0f, 100.0f, 90.0f, silver, 0);
    mkKnob(knobMod, "MOD DEPTH", "%", 0.0f, 100.0f, 35.0f, silver, 0);
    mkKnob(knobWidth, "WIDTH", "%", 0.0f, 200.0f, 115.0f, cyan, 0);
    mkKnob(knobMix, "WET MIX", "%", 0.0f, 100.0f, 35.0f, cyan, 0);

    knobHiCut->setLogarithmic(true);
    knobDecay->onValueChanged = [this](float v) { if (engine) engine->setDecay(v); repaint(); };
    knobSize->onValueChanged = [this](float v) { if (engine) engine->setRoomSize(v); repaint(); };
    knobPreDelay->onValueChanged = [this](float v) { if (engine) engine->setPreDelay(v); repaint(); };
    knobHiCut->onValueChanged = [this](float v) { if (engine) engine->setHiCut(v); };
    knobLowDamp->onValueChanged = [this](float v) { if (engine) engine->setLowDamp(v); };
    knobEarly->onValueChanged = [this](float v) { if (engine) engine->setEarlyLevel(v * 0.01); repaint(); };
    knobTail->onValueChanged = [this](float v) { if (engine) engine->setTailLevel(v * 0.01); repaint(); };
    knobMod->onValueChanged = [this](float v) { if (engine) engine->setModDepth(v * 0.01); };
    knobWidth->onValueChanged = [this](float v) { if (engine) engine->setStereoWidth(v * 0.01); };
    knobMix->onValueChanged = [this](float v) { if (engine) engine->setMix(v * 0.01); };

    syncFromEngine();
    startTimerHz(25);
  }

  ~VSS3Panel() override { stopTimer(); }

  void setEngine(nova::graviton::VSSReverb &eng) {
    engine = &eng;
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    if (!engine) return;
    cbPreset.setSelectedId(engine->getCurrentPresetIndex() + 1, juce::dontSendNotification);
    knobDecay->setValue(static_cast<float>(engine->getDecay()), juce::dontSendNotification);
    knobSize->setValue(static_cast<float>(engine->getRoomSize()), juce::dontSendNotification);
    knobPreDelay->setValue(static_cast<float>(engine->getPreDelay()), juce::dontSendNotification);
    knobHiCut->setValue(static_cast<float>(engine->getHiCut()), juce::dontSendNotification);
    knobLowDamp->setValue(static_cast<float>(engine->getLowDamp()), juce::dontSendNotification);
    knobEarly->setValue(static_cast<float>(engine->getEarlyLevel() * 100.0), juce::dontSendNotification);
    knobTail->setValue(static_cast<float>(engine->getTailLevel() * 100.0), juce::dontSendNotification);
    knobMod->setValue(static_cast<float>(engine->getModDepth() * 100.0), juce::dontSendNotification);
    knobWidth->setValue(static_cast<float>(engine->getStereoWidth() * 100.0), juce::dontSendNotification);
    knobMix->setValue(static_cast<float>(engine->getMix() * 100.0), juce::dontSendNotification);
    updateButtons();
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0b1018));

    juce::ColourGradient chassis(juce::Colour(0xff1e293b), 0.0f, 0.0f,
                                 juce::Colour(0xff0f172a), 0.0f, b.getBottom(), false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(b.reduced(6.0f), 7.0f);
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(b.reduced(6.0f), 7.0f, 1.3f);

    g.setColour(juce::Colour(0xffe2e8f0));
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.drawText("TC ELECTRONIC VSS3  //  STEREO SOURCE REVERB",
               titleRect.toNearestInt(), juce::Justification::centredLeft);

    drawEnvelope(g, displayRect);
    drawMeter(g, meterInRect, "IN", inDbSmooth);
    drawMeter(g, meterOutRect, "OUT", outDbSmooth);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(14);
    auto topBar = b.removeFromTop(26);
    btnBypass.setBounds(topBar.removeFromRight(68).reduced(2));
    auto presArea = topBar.removeFromRight(juce::jlimit(190, 260, topBar.getWidth() / 3));
    btnPrevPreset.setBounds(presArea.removeFromLeft(24).reduced(1));
    btnNextPreset.setBounds(presArea.removeFromRight(24).reduced(1));
    cbPreset.setBounds(presArea.reduced(2, 1));
    titleRect = topBar.toFloat();

    b.removeFromTop(6);
    auto spaceRow = b.removeFromTop(26);
    const int sw = spaceRow.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
      btnSpace[static_cast<size_t>(i)].setBounds(spaceRow.removeFromLeft(sw).reduced(2));

    b.removeFromTop(6);
    auto dispRow = b.removeFromTop(juce::jlimit(85, 145, b.getHeight() * 30 / 100));
    auto meters = dispRow.removeFromRight(76);
    meterInRect = meters.removeFromLeft(38).reduced(4, 2).toFloat();
    meterOutRect = meters.reduced(4, 2).toFloat();
    dispRow.removeFromRight(6);
    displayRect = dispRow.toFloat();

    b.removeFromTop(8);
    auto row1 = b.removeFromTop(b.getHeight() / 2);
    auto row2 = b;
    const int kw = row1.getWidth() / 5;
    knobDecay->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobSize->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobPreDelay->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobHiCut->setBounds(row1.removeFromLeft(kw).reduced(2));
    knobLowDamp->setBounds(row1.reduced(2));

    knobEarly->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobTail->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobMod->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobWidth->setBounds(row2.removeFromLeft(kw).reduced(2));
    knobMix->setBounds(row2.reduced(2));
  }

private:
  void timerCallback() override {
    if (!engine) return;
    inDbSmooth += (engine->getInLevelDb() - inDbSmooth) * 0.45f;
    outDbSmooth += (engine->getOutLevelDb() - outDbSmooth) * 0.45f;
    repaint();
  }

  void drawEnvelope(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff050a14));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(r, 5.0f, 1.2f);
    if (!engine) return;

    auto plot = r.reduced(14.0f, 10.0f);
    const float pdMs = static_cast<float>(engine->getPreDelay());
    const float decSec = static_cast<float>(engine->getDecay());
    const float totalMs = std::max(1500.0f, pdMs + decSec * 1000.0f);
    const float pdX = plot.getX() + (pdMs / totalMs) * plot.getWidth();

    // Early reflections spikes
    g.setColour(juce::Colour(0xff7dd3fc));
    const float erAmp = static_cast<float>(engine->getEarlyLevel()) * plot.getHeight() * 0.8f;
    for (int i = 0; i < 6; ++i) {
      const float x = plot.getX() + (i + 1) * ((pdX - plot.getX() + 24.0f) / 7.0f);
      const float h = erAmp * (1.0f - i * 0.12f);
      g.drawVerticalLine(static_cast<int>(x), plot.getBottom() - h, plot.getBottom());
    }

    // Late tail decay curve
    juce::Path p;
    p.startNewSubPath(pdX, plot.getBottom());
    const float tailH = static_cast<float>(engine->getTailLevel()) * (plot.getHeight() - 6.0f);
    for (int i = 0; i <= 70; ++i) {
      const float t = static_cast<float>(i) / 70.0f;
      const float x = pdX + t * (plot.getRight() - pdX);
      const float y = plot.getBottom() - std::exp(-3.2f * t) * tailH;
      p.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xff38bdf8));
    g.strokePath(p, juce::PathStrokeType(2.2f));
  }

  void drawMeter(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &lbl, float db) {
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText(lbl, r.withHeight(12.0f).toNearestInt(), juce::Justification::centred);
    auto bar = r.withTrimmedTop(14.0f).withSizeKeepingCentre(12.0f, r.getHeight() - 14.0f);
    g.setColour(juce::Colour(0xff050a14));
    g.fillRoundedRectangle(bar, 2.0f);
    const float n = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
    if (n > 0.005f) {
      g.setColour(juce::Colour(0xff38bdf8));
      g.fillRoundedRectangle(bar.withTop(bar.getBottom() - bar.getHeight() * n), 2.0f);
    }
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(bar, 2.0f, 1.0f);
  }

  void updateButtons() {
    if (!engine) return;
    const int sp = engine->getSpaceType();
    for (int i = 0; i < 5; ++i) {
      const bool on = (i == sp);
      btnSpace[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId,
                                                  on ? juce::Colour(0xff0284c7) : juce::Colour(0xff1e293b));
      btnSpace[static_cast<size_t>(i)].setColour(juce::TextButton::textColourOffId,
                                                  on ? juce::Colours::white : juce::Colour(0xff94a3b8));
    }
    const bool bp = engine->isBypassed();
    btnBypass.setColour(juce::TextButton::buttonColourId, bp ? juce::Colour(0xffef4444) : juce::Colour(0xff1e293b));
    btnBypass.setColour(juce::TextButton::textColourOffId, bp ? juce::Colours::white : juce::Colour(0xff94a3b8));
  }

  nova::graviton::VSSReverb *engine = nullptr;
  float inDbSmooth = -60.0f, outDbSmooth = -60.0f;
  juce::Rectangle<float> titleRect, displayRect, meterInRect, meterOutRect;

  juce::TextButton btnPrevPreset, btnNextPreset, btnBypass;
  juce::ComboBox cbPreset;
  std::array<juce::TextButton, 5> btnSpace;
  std::unique_ptr<NovaRotaryKnob> knobDecay, knobSize, knobPreDelay, knobHiCut, knobLowDamp,
      knobEarly, knobTail, knobMod, knobWidth, knobMix;
};

} // namespace nova
