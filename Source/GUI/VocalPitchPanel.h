#pragma once

#include "Effects/GravitonVocalPitchCorrection.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <deque>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

// ==============================================================================
// OPENLIVE_REFACTORED — Flayr-Tune + Evo Style Vocal Pitch Correction Panel
// Dedicated studio UI for Slot 20 (VocalPitchCorrector):
//   - Input Type, Key (C..B), 13 Scales, Auto Key Learn (Krumhansl-Schmuckler)
//   - LIVE (low-latency) vs PSOLA mode, Formant Preserve, Quick Presets
//   - Real-time Scrolling Pitch Graph (Input vs Corrected vs Target) +
//     Center-Zero Correction Cents Meter + Interactive 12-Note Removed Keyboard
//   - 8 Studio Rotary Knobs: Retune, Humanize, Flex/Expr, Nat Vib, Throat,
//     Transpose, Detune, Mix
// ==============================================================================
class VocalPitchPanel final : public juce::Component, private juce::Timer {
public:
  explicit VocalPitchPanel(nova::graviton::VocalPitchCorrector &engineRef)
      : engine(&engineRef) {
    setOpaque(true);

    // Input Type selector
    lblInputType.setText("INPUT TYPE", juce::dontSendNotification);
    lblInputType.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    lblInputType.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(lblInputType);

    cbInputType.addItem("Soprano", 1);
    cbInputType.addItem("Alto / Tenor", 2);
    cbInputType.addItem("Low Male", 3);
    cbInputType.addItem("Instrument", 4);
    cbInputType.onChange = [this] {
      if (engine != nullptr)
        engine->setInputType(cbInputType.getSelectedId() - 1);
    };
    addAndMakeVisible(cbInputType);

    // Key selector
    lblKey.setText("KEY", juce::dontSendNotification);
    lblKey.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    lblKey.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(lblKey);

    for (int i = 0; i < 12; ++i)
      cbKey.addItem(tune::kNoteNames[static_cast<size_t>(i)], i + 1);
    cbKey.onChange = [this] {
      if (engine != nullptr)
        engine->setKey(cbKey.getSelectedId() - 1);
      repaint();
    };
    addAndMakeVisible(cbKey);

    // Scale selector (13 scales from Flayr TuneEngine)
    lblScale.setText("SCALE", juce::dontSendNotification);
    lblScale.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    lblScale.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(lblScale);

    for (int i = 0; i < static_cast<int>(tune::kScaleNames.size()); ++i)
      cbScale.addItem(tune::kScaleNames[static_cast<size_t>(i)], i + 1);
    cbScale.onChange = [this] {
      if (engine != nullptr)
        engine->setScale(cbScale.getSelectedId() - 1);
      repaint();
    };
    addAndMakeVisible(cbScale);

    // Learn Key button
    btnLearnKey.setButtonText("LEARN KEY");
    btnLearnKey.setClickingTogglesState(true);
    btnLearnKey.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff1e293b));
    btnLearnKey.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xfff59e0b));
    btnLearnKey.onClick = [this] {
      if (engine == nullptr)
        return;
      if (btnLearnKey.getToggleState()) {
        engine->beginKeyLearn();
      } else {
        engine->endKeyLearn();
        if (engine->applyLearnedKey())
          syncFromEngine();
      }
      repaint();
    };
    addAndMakeVisible(btnLearnKey);

    // Live Mode (low-latency vs TD-PSOLA)
    btnLiveMode.setButtonText("LIVE");
    btnLiveMode.setClickingTogglesState(true);
    btnLiveMode.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff1e293b));
    btnLiveMode.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xff10b981));
    btnLiveMode.onClick = [this] {
      if (engine != nullptr) {
        const bool live = btnLiveMode.getToggleState();
        engine->setLiveMode(live);
        btnLiveMode.setButtonText(live ? "LIVE" : "PSOLA");
      }
    };
    addAndMakeVisible(btnLiveMode);

    // Formant Preserve toggle
    btnFormant.setButtonText("FORMANT");
    btnFormant.setClickingTogglesState(true);
    btnFormant.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff1e293b));
    btnFormant.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour(0xff0ea5e9));
    btnFormant.onClick = [this] {
      if (engine != nullptr)
        engine->setFormantPreserve(btnFormant.getToggleState());
    };
    addAndMakeVisible(btnFormant);

    // Quick Preset buttons
    static const char *presetLabels[4] = {"HARD TUNE", "POP VOCAL", "NATURAL",
                                          "TRANSPARENT"};
    for (int i = 0; i < 4; ++i) {
      btnPresets[static_cast<size_t>(i)] =
          std::make_unique<juce::TextButton>(presetLabels[i]);
      auto *b = btnPresets[static_cast<size_t>(i)].get();
      b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162235));
      b->setColour(juce::TextButton::textColourOffId,
                   juce::Colour(0xffcbd5e1));
      const int idx = i;
      b->onClick = [this, idx] { applyQuickPreset(idx); };
      addAndMakeVisible(*b);
    }

    // 8 Studio Rotary Knobs
    knobRetune = std::make_unique<NovaRotaryKnob>(
        "RETUNE", "ms", 0.0f, 400.0f, 20.0f, false, juce::Colour(0xff0ea5e9));
    knobRetune->setDisplayDecimals(0);
    knobRetune->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setRetuneSpeedMs(v);
    };
    addAndMakeVisible(*knobRetune);

    knobHumanize = std::make_unique<NovaRotaryKnob>(
        "HUMANIZE", "%", 0.0f, 100.0f, 20.0f, false,
        juce::Colour(0xff38bdf8));
    knobHumanize->setDisplayDecimals(0);
    knobHumanize->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setHumanizePct(v);
    };
    addAndMakeVisible(*knobHumanize);

    knobExpression = std::make_unique<NovaRotaryKnob>(
        "FLEX-TUNE", "%", 0.0f, 100.0f, 0.0f, false,
        juce::Colour(0xff22d3ee));
    knobExpression->setDisplayDecimals(0);
    knobExpression->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setExpressionPct(v);
    };
    addAndMakeVisible(*knobExpression);

    knobNatVib = std::make_unique<NovaRotaryKnob>(
        "NAT VIB", "dB", -12.0f, 12.0f, 0.0f, true, juce::Colour(0xff34d399));
    knobNatVib->setDisplayDecimals(1);
    knobNatVib->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setNaturalVibratoDb(v);
    };
    addAndMakeVisible(*knobNatVib);

    knobThroat = std::make_unique<NovaRotaryKnob>(
        "THROAT", "%", -50.0f, 50.0f, 0.0f, true, juce::Colour(0xfff59e0b));
    knobThroat->setDisplayDecimals(0);
    knobThroat->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setThroatLengthPct(v);
    };
    addAndMakeVisible(*knobThroat);

    knobTranspose = std::make_unique<NovaRotaryKnob>(
        "TRANSPOSE", "st", -12.0f, 12.0f, 0.0f, true,
        juce::Colour(0xfffb7185));
    knobTranspose->setDisplayDecimals(1);
    knobTranspose->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setTranspose(v);
    };
    addAndMakeVisible(*knobTranspose);

    knobDetune = std::make_unique<NovaRotaryKnob>(
        "DETUNE", "ct", -50.0f, 50.0f, 0.0f, true, juce::Colour(0xff10b981));
    knobDetune->setDisplayDecimals(0);
    knobDetune->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setDetuneCents(v);
    };
    addAndMakeVisible(*knobDetune);

    knobMix = std::make_unique<NovaRotaryKnob>(
        "MIX", "%", 0.0f, 100.0f, 100.0f, false, juce::Colour(0xff8b5cf6));
    knobMix->setDisplayDecimals(0);
    knobMix->onValueChanged = [this](float v) {
      if (engine != nullptr)
        engine->setMix(v * 0.01);
    };
    addAndMakeVisible(*knobMix);

    syncFromEngine();
    startTimerHz(30);
  }

  ~VocalPitchPanel() override { stopTimer(); }

  void setEngine(nova::graviton::VocalPitchCorrector &engineRef) {
    engine = &engineRef;
    history.clear();
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    if (engine == nullptr)
      return;
    cbInputType.setSelectedId(engine->getInputType() + 1,
                              juce::dontSendNotification);
    cbKey.setSelectedId(engine->getKey() + 1, juce::dontSendNotification);
    cbScale.setSelectedId(engine->getScale() + 1, juce::dontSendNotification);
    btnLiveMode.setToggleState(engine->getLiveMode(),
                               juce::dontSendNotification);
    btnLiveMode.setButtonText(engine->getLiveMode() ? "LIVE" : "PSOLA");
    btnFormant.setToggleState(engine->getFormantPreserve(),
                              juce::dontSendNotification);
    btnLearnKey.setToggleState(engine->isLearningKey(),
                               juce::dontSendNotification);

    knobRetune->setValue(static_cast<float>(engine->getRetuneSpeedMs()),
                         juce::dontSendNotification);
    knobHumanize->setValue(static_cast<float>(engine->getHumanizePct()),
                           juce::dontSendNotification);
    knobExpression->setValue(static_cast<float>(engine->getExpressionPct()),
                             juce::dontSendNotification);
    knobNatVib->setValue(static_cast<float>(engine->getNaturalVibratoDb()),
                         juce::dontSendNotification);
    knobThroat->setValue(static_cast<float>(engine->getThroatLengthPct()),
                         juce::dontSendNotification);
    knobTranspose->setValue(static_cast<float>(engine->getTranspose()),
                            juce::dontSendNotification);
    knobDetune->setValue(static_cast<float>(engine->getDetuneCents()),
                         juce::dontSendNotification);
    knobMix->setValue(static_cast<float>(engine->getMix() * 100.0),
                      juce::dontSendNotification);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (engine == nullptr)
      return;
    if (keyboardRect.contains(e.position)) {
      const float relX = e.position.x - keyboardRect.getX();
      const int pc = std::clamp(
          static_cast<int>(relX / (keyboardRect.getWidth() / 12.0f)), 0, 11);
      engine->toggleRemovedPitchClass(pc);
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0b1220));

    // Metallic studio chassis
    g.setColour(juce::Colour(0xff111c30));
    g.fillRoundedRectangle(b.reduced(6.0f), 8.0f);
    g.setColour(juce::Colour(0xff1e3a5f));
    g.drawRoundedRectangle(b.reduced(6.0f), 8.0f, 1.2f);

    // Brand badge + mode info
    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::FontOptions(14.5f, juce::Font::bold));
    g.drawText("NOVA EVO // FLAYR-TUNE VOCAL PITCH ENGINE",
               juce::Rectangle<float>(18.0f, 10.0f, 380.0f, 20.0f),
               juce::Justification::centredLeft);

    if (engine != nullptr) {
      const float latMs = static_cast<float>(engine->getLatencySamples()) / 48.0f;
      g.setColour(juce::Colour(0xff64748b));
      g.setFont(juce::FontOptions(11.0f));
      g.drawText((engine->getLiveMode() ? "LOW-LATENCY LIVE (" : "TD-PSOLA (") +
                     juce::String(latMs, 1) + " ms)",
                 juce::Rectangle<float>(b.getRight() - 230.0f, 10.0f, 212.0f,
                                        20.0f),
                 juce::Justification::centredRight);
    }

    if (!meterRect.isEmpty()) {
      g.setColour(juce::Colour(0xff070d18));
      g.fillRoundedRectangle(meterRect, 6.0f);
      g.setColour(juce::Colour(0xff1e293b));
      g.drawRoundedRectangle(meterRect, 6.0f, 1.0f);

      const float detMidi =
          engine != nullptr ? engine->getDetectedMidi() : 69.0f;
      const float tgtMidi = engine != nullptr ? engine->getTargetMidi() : 69.0f;
      const float cents =
          engine != nullptr ? engine->getCorrectionCents() : 0.0f;
      const bool isVoiced =
          engine != nullptr && engine->getVoicedConfidence() > 0.35f;

      // Top telemetry strip inside meterRect
      auto topInfo = meterRect.withHeight(48.0f).reduced(10.0f, 4.0f);

      // Voiced LED + status
      g.setColour(isVoiced ? juce::Colour(0xff22c55e)
                           : juce::Colour(0xff334155));
      g.fillEllipse(topInfo.getX(), topInfo.getY() + 14.0f, 9.0f, 9.0f);

      const juce::String detStr =
          isVoiced ? midiToNoteName(static_cast<int>(std::round(detMidi)))
                   : "--";
      const juce::String tgtStr =
          isVoiced ? midiToNoteName(static_cast<int>(std::round(tgtMidi)))
                   : "--";

      g.setColour(juce::Colour(0xff94a3b8));
      g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
      g.drawText("IN",
                 juce::Rectangle<float>(topInfo.getX() + 16.0f, topInfo.getY(),
                                        50.0f, 12.0f),
                 juce::Justification::centredLeft);
      g.setColour(juce::Colours::white);
      g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
      g.drawText(detStr,
                 juce::Rectangle<float>(topInfo.getX() + 16.0f,
                                        topInfo.getY() + 12.0f, 64.0f, 24.0f),
                 juce::Justification::centredLeft);

      // Center-Zero Cents Meter
      const float cBarW = std::clamp(topInfo.getWidth() - 220.0f, 120.0f, 380.0f);
      const float cBarX = topInfo.getCentreX() - cBarW * 0.5f;
      const float cBarY = topInfo.getY() + 8.0f;
      const juce::Rectangle<float> cBar(cBarX, cBarY, cBarW, 14.0f);
      g.setColour(juce::Colour(0xff1e293b));
      g.fillRoundedRectangle(cBar, 3.0f);

      const float normCents = std::clamp(cents / 50.0f, -1.0f, 1.0f);
      const float cx = cBar.getCentreX();
      if (isVoiced && std::abs(normCents) > 0.01f) {
        const float fillW = std::abs(normCents) * (cBarW * 0.5f);
        const float fillX = (normCents >= 0.0f) ? cx : (cx - fillW);
        g.setColour(normCents >= 0.0f ? juce::Colour(0xff0ea5e9)
                                      : juce::Colour(0xfff59e0b));
        g.fillRoundedRectangle(fillX, cBarY + 2.0f, fillW, 10.0f, 2.0f);
      }
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.drawVerticalLine(static_cast<int>(cx), cBarY - 1.0f, cBarY + 15.0f);

      g.setColour(juce::Colour(0xff94a3b8));
      g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
      g.drawText(isVoiced ? (juce::String(cents >= 0.0f ? "+" : "") +
                             juce::String(cents, 1) + " ct")
                          : "0.0 ct",
                 juce::Rectangle<float>(cBarX, cBarY + 16.0f, cBarW, 14.0f),
                 juce::Justification::centred);

      // Target Note
      g.setColour(juce::Colour(0xff38bdf8));
      g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
      g.drawText("TARGET",
                 juce::Rectangle<float>(topInfo.getRight() - 80.0f,
                                        topInfo.getY(), 80.0f, 12.0f),
                 juce::Justification::centredRight);
      g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
      g.drawText(tgtStr,
                 juce::Rectangle<float>(topInfo.getRight() - 80.0f,
                                        topInfo.getY() + 12.0f, 80.0f, 24.0f),
                 juce::Justification::centredRight);

      // Scrolling Pitch Graph
      const juce::Rectangle<float> graphArea(
          meterRect.getX() + 10.0f, meterRect.getY() + 50.0f,
          meterRect.getWidth() - 20.0f,
          std::max(40.0f, meterRect.getHeight() - 96.0f));
      drawPitchGraph(g, graphArea);

      // Interactive 12-Semitone Keyboard (click to bypass/remove pitch class)
      keyboardRect = juce::Rectangle<float>(
          meterRect.getX() + 10.0f, meterRect.getBottom() - 38.0f,
          meterRect.getWidth() - 20.0f, 28.0f);
      drawScaleKeyboard(
          g, keyboardRect,
          isVoiced ? ((static_cast<int>(std::round(tgtMidi)) % 12 + 12) % 12)
                   : -1);
    }
  }

  void resized() override {
    auto b = getLocalBounds().reduced(14);
    b.removeFromTop(26); // brand header

    // Top control strip
    auto topRow = b.removeFromTop(40);
    const int colW = std::max(72, topRow.getWidth() / 8);

    auto rIn = topRow.removeFromLeft(colW + 12).reduced(2, 0);
    lblInputType.setBounds(rIn.removeFromTop(13));
    cbInputType.setBounds(rIn);

    auto rKey = topRow.removeFromLeft(std::max(54, colW * 2 / 3)).reduced(2, 0);
    lblKey.setBounds(rKey.removeFromTop(13));
    cbKey.setBounds(rKey);

    auto rScale = topRow.removeFromLeft(colW + 24).reduced(2, 0);
    lblScale.setBounds(rScale.removeFromTop(13));
    cbScale.setBounds(rScale);

    btnLearnKey.setBounds(topRow.removeFromLeft(78).reduced(2, 5));
    btnLiveMode.setBounds(topRow.removeFromLeft(58).reduced(2, 5));
    btnFormant.setBounds(topRow.removeFromLeft(72).reduced(2, 5));

    auto rPres = topRow.reduced(2, 5);
    const int pW = std::max(48, rPres.getWidth() / 4);
    for (int i = 0; i < 4; ++i) {
      btnPresets[static_cast<size_t>(i)]->setBounds(
          rPres.removeFromLeft(pW).reduced(2, 0));
    }

    b.removeFromTop(6);

    // Bottom 8 rotary knobs deck
    auto knobRow = b.removeFromBottom(std::clamp(b.getHeight() / 3, 96, 138));
    const int kW = knobRow.getWidth() / 8;
    knobRetune->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobHumanize->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobExpression->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobNatVib->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobThroat->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobTranspose->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobDetune->setBounds(knobRow.removeFromLeft(kW).reduced(2));
    knobMix->setBounds(knobRow.reduced(2));

    b.removeFromBottom(6);
    meterRect = b.toFloat();
  }

private:
  static constexpr size_t kMaxHistory = 180;

  void timerCallback() override {
    if (engine != nullptr) {
      tune::DisplayPoint pt{};
      while (engine->popDisplayPoint(pt)) {
        history.push_back(pt);
        while (history.size() > kMaxHistory)
          history.pop_front();
      }
    }
    repaint();
  }

  void applyQuickPreset(int idx) {
    if (engine == nullptr)
      return;
    if (idx == 0) { // HARD TUNE
      engine->setRetuneSpeedMs(0.0);
      engine->setHumanizePct(0.0);
      engine->setExpressionPct(0.0);
      engine->setNaturalVibratoDb(0.0);
      engine->setFormantPreserve(true);
    } else if (idx == 1) { // POP VOCAL
      engine->setRetuneSpeedMs(20.0);
      engine->setHumanizePct(25.0);
      engine->setExpressionPct(20.0);
      engine->setNaturalVibratoDb(0.0);
      engine->setFormantPreserve(true);
    } else if (idx == 2) { // NATURAL
      engine->setRetuneSpeedMs(65.0);
      engine->setHumanizePct(55.0);
      engine->setExpressionPct(40.0);
      engine->setNaturalVibratoDb(0.0);
      engine->setFormantPreserve(true);
    } else { // TRANSPARENT
      engine->setRetuneSpeedMs(150.0);
      engine->setHumanizePct(80.0);
      engine->setExpressionPct(65.0);
      engine->setNaturalVibratoDb(0.0);
      engine->setFormantPreserve(true);
    }
    syncFromEngine();
    repaint();
  }

  static juce::String midiToNoteName(int midi) {
    const int pc = ((midi % 12) + 12) % 12;
    const int oct = (midi / 12) - 1;
    return juce::String(tune::kNoteNames[static_cast<size_t>(pc)]) +
           juce::String(oct);
  }

  void drawPitchGraph(juce::Graphics &g, juce::Rectangle<float> area) const {
    g.setColour(juce::Colour(0xff050a14));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colour(0xff17253b));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    // MIDI range 48 (C3) .. 84 (C6)
    constexpr float kMinMidi = 48.0f;
    constexpr float kMaxMidi = 84.0f;
    auto midiToY = [&](float m) -> float {
      const float norm = (m - kMinMidi) / (kMaxMidi - kMinMidi);
      return area.getBottom() - std::clamp(norm, 0.0f, 1.0f) * area.getHeight();
    };

    // Subtle octave grid lines (C3, C4, C5, C6)
    g.setColour(juce::Colour(0xff1e293b).withAlpha(0.7f));
    for (int m = 48; m <= 84; m += 12) {
      const float y = midiToY(static_cast<float>(m));
      g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
    }

    if (history.size() < 2)
      return;

    juce::Path inPath, outPath;
    bool inStarted = false, outStarted = false;
    const float dx = area.getWidth() / static_cast<float>(kMaxHistory - 1);
    const float xOffset =
        area.getRight() - static_cast<float>(history.size() - 1) * dx;

    for (size_t i = 0; i < history.size(); ++i) {
      const auto &pt = history[i];
      const float x = xOffset + static_cast<float>(i) * dx;
      if (pt.voiced != 0 && pt.inNote > 20.0f) {
        const float yIn = midiToY(pt.inNote);
        const float yOut = midiToY(pt.outNote);
        if (!inStarted) {
          inPath.startNewSubPath(x, yIn);
          inStarted = true;
        } else {
          inPath.lineTo(x, yIn);
        }
        if (!outStarted) {
          outPath.startNewSubPath(x, yOut);
          outStarted = true;
        } else {
          outPath.lineTo(x, yOut);
        }
      } else {
        inStarted = false;
        outStarted = false;
      }
    }

    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.55f));
    g.strokePath(inPath, juce::PathStrokeType(1.6f));
    g.setColour(juce::Colour(0xff38bdf8));
    g.strokePath(outPath, juce::PathStrokeType(2.2f));
  }

  void drawScaleKeyboard(juce::Graphics &g, juce::Rectangle<float> area,
                         int activePitchClass) const {
    const int key = engine != nullptr ? engine->getKey() : 0;
    const int sc = engine != nullptr ? engine->getScale() : 1;
    const uint16_t scaleMask = tune::scalePitchClasses(key, sc);
    const uint16_t removedMask = engine != nullptr ? engine->getRemovedMask() : 0;
    const float noteW = area.getWidth() / 12.0f;

    for (int i = 0; i < 12; ++i) {
      const bool inScale = ((scaleMask >> i) & 1u) != 0;
      const bool isRemoved = ((removedMask >> i) & 1u) != 0;
      const bool isCurrent = (i == activePitchClass);
      const juce::Rectangle<float> r(area.getX() + i * noteW + 1.5f,
                                     area.getY(), noteW - 3.0f,
                                     area.getHeight());

      if (isRemoved) {
        g.setColour(juce::Colour(0xff4c1d24));
      } else if (isCurrent) {
        g.setColour(juce::Colour(0xff0ea5e9));
      } else if (inScale) {
        g.setColour(juce::Colour(0xff1e3a5f));
      } else {
        g.setColour(juce::Colour(0xff0f172a));
      }
      g.fillRoundedRectangle(r, 3.0f);

      g.setColour(isRemoved
                      ? juce::Colour(0xfff87171)
                      : (isCurrent
                             ? juce::Colours::white
                             : (inScale ? juce::Colour(0xffcbd5e1)
                                        : juce::Colour(0xff475569))));
      g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
      g.drawText(tune::kNoteNames[static_cast<size_t>(i)], r,
                 juce::Justification::centred);
    }
  }

  nova::graviton::VocalPitchCorrector *engine = nullptr;
  juce::Rectangle<float> meterRect;
  juce::Rectangle<float> keyboardRect;
  std::deque<tune::DisplayPoint> history;

  juce::Label lblInputType, lblKey, lblScale;
  juce::ComboBox cbInputType, cbKey, cbScale;
  juce::TextButton btnLearnKey, btnLiveMode, btnFormant;
  std::array<std::unique_ptr<juce::TextButton>, 4> btnPresets;

  std::unique_ptr<NovaRotaryKnob> knobRetune, knobHumanize, knobExpression,
      knobNatVib, knobThroat, knobTranspose, knobDetune, knobMix;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalPitchPanel)
};

} // namespace nova
