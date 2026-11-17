#pragma once

#include "Effects/GravitonHybridReverb.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace nova {

// ==============================================================================
// HybridReverbPanel — Ableton Hybrid Reverb Style 2-Tab Editor (Slot 22)
//   - Left Strip: Send, Predelay (+ ms/Sync), Feedback
//   - Right Strip: Stereo, Vintage, Bass Mono, Dry/Wet
//   - Tab 1 (Reverb): Orange IR Waveform + Algorithmic Section (Attack, Decay,
//     Size, Routing, Algorithm, Freeze, Delay, Mod, Diffusion) + Convolution IR
//     Section (Source, IR Preset, Blend 60/40, Decay, Size, Damping, Lo Damp, Distance)
//   - Tab 2 (EQ): Interactive Blue EQ Curve + 4 Orange Numbered Nodes (1..4) +
//     Pre Algo, Type, Freq, Slope, Gain
// ==============================================================================
class HybridReverbPanel final : public juce::Component, private juce::Timer {
public:
  explicit HybridReverbPanel(HybridReverbEngine &eng) : engine(&eng) {
    setOpaque(true);

    // Header tabs
    btnTabReverb.setButtonText("Reverb");
    btnTabEq.setButtonText("EQ");
    btnTabReverb.onClick = [this] { activeTab = 0; updateTabVisibility(); };
    btnTabEq.onClick = [this] { activeTab = 1; updateTabVisibility(); };
    addAndMakeVisible(btnTabReverb);
    addAndMakeVisible(btnTabEq);

    auto mkKnob = [this](std::unique_ptr<NovaRotaryKnob> &k, const char *name,
                         const char *unit, float lo, float hi, float def,
                         bool bipolar, juce::Colour c, int dec) {
      k = std::make_unique<NovaRotaryKnob>(name, unit, lo, hi, def, bipolar, c);
      k->setDisplayDecimals(dec);
      addAndMakeVisible(*k);
    };

    const juce::Colour cyan(0xff38bdf8);
    const juce::Colour orange(0xfff59e0b);

    // --- Left Strip ---
    mkKnob(knobSend, "Send", "dB", -60.0f, 0.0f, 0.0f, false, cyan, 1);
    mkKnob(knobPredelay, "Predelay", "ms", 0.0f, 500.0f, 10.0f, false, cyan, 1);
    btnMsSync.setButtonText("ms");
    btnMsSync.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.preDelaySync = !p.preDelaySync;
      engine->setParams(p);
      updateButtons();
    };
    addAndMakeVisible(btnMsSync);
    mkKnob(knobFeedback, "Feedback", "%", 0.0f, 95.0f, 0.0f, false, cyan, 1);

    knobSend->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams(); p.sendDb = v; engine->setParams(p);
    };
    knobPredelay->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams(); p.preDelayMs = v; engine->setParams(p);
    };
    knobFeedback->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams(); p.feedbackPct = v; engine->setParams(p);
    };

    // --- Right Strip ---
    mkKnob(knobStereo, "Stereo", "%", 0.0f, 200.0f, 100.0f, false, cyan, 0);
    btnVintage.setButtonText("Vintage: Off");
    btnVintage.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.vintageMode = (p.vintageMode + 1) % 4;
      engine->setParams(p);
      updateButtons();
    };
    btnBassMono.setButtonText("Bass: Mono");
    btnBassMono.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.bassMono = !p.bassMono;
      engine->setParams(p);
      updateButtons();
    };
    mkKnob(knobDryWet, "Dry/Wet", "%", 0.0f, 100.0f, 30.0f, false, cyan, 0);
    addAndMakeVisible(btnVintage);
    addAndMakeVisible(btnBassMono);

    knobStereo->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams(); p.stereoSpread = v * 0.01f; engine->setParams(p);
    };
    knobDryWet->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams(); p.dryWet = v * 0.01f; engine->setParams(p);
    };

    // --- Tab 1: Algorithmic + Convolution ---
    mkKnob(knobAlgoAttack, "Attack", "ms", 0.0f, 500.0f, 0.0f, false, orange, 1);
    mkKnob(knobAlgoDecay, "Decay", "s", 0.1f, 30.0f, 8.0f, false, orange, 1);
    mkKnob(knobAlgoSize, "Size", "%", 5.0f, 100.0f, 20.0f, false, orange, 0);
    mkKnob(knobAlgoDelay, "Delay", "ms", 0.0f, 250.0f, 0.0f, false, cyan, 1);
    mkKnob(knobAlgoMod, "Mod", "%", 0.0f, 100.0f, 50.0f, false, cyan, 0);
    mkKnob(knobAlgoDiff, "Diffusion", "%", 0.0f, 100.0f, 100.0f, false, cyan, 0);

    cbRouting.addItem("Parallel", 1);
    cbRouting.addItem("Serial", 2);
    cbRouting.addItem("Algo Only", 3);
    cbRouting.addItem("Conv Only", 4);
    cbRouting.onChange = [this] {
      if (!engine) return;
      auto p = engine->getParams(); p.routing = cbRouting.getSelectedId() - 1; engine->setParams(p);
    };
    addAndMakeVisible(cbRouting);

    cbAlgo.addItem("Dark Hall", 1);
    cbAlgo.addItem("Quartz", 2);
    cbAlgo.addItem("Shimmer", 3);
    cbAlgo.addItem("Prism", 4);
    cbAlgo.addItem("Tides", 5);
    cbAlgo.onChange = [this] {
      if (!engine) return;
      auto p = engine->getParams(); p.algorithm = cbAlgo.getSelectedId() - 1; engine->setParams(p);
    };
    addAndMakeVisible(cbAlgo);

    btnFreeze.setButtonText("FREEZE");
    btnFreeze.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams(); p.freeze = !p.freeze; engine->setParams(p);
      updateButtons();
    };
    addAndMakeVisible(btnFreeze);

    knobAlgoAttack->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.algoAttackMs = v; engine->setParams(p);
    };
    knobAlgoDecay->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.decay = v; engine->setParams(p); repaint();
    };
    knobAlgoSize->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.size = v * 0.01f; engine->setParams(p);
    };
    knobAlgoDelay->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.algoDelayMs = v; engine->setParams(p);
    };
    knobAlgoMod->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.modDepth = v * 0.01f; engine->setParams(p);
    };
    knobAlgoDiff->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.diffusion = v * 0.01f; engine->setParams(p);
    };

    // Convolution IR controls
    btnPrevIr.setButtonText("<");
    btnNextIr.setButtonText(">");
    btnLoadWav.setButtonText("LOAD IR");
    const auto &lib = ConvolutionReverbEngine::getFactoryLibrary();
    for (int i = 0; i < ConvolutionReverbEngine::kNumFactoryPresets; ++i)
      cbIrPreset.addItem(lib[static_cast<size_t>(i)].name, i + 1);
    cbIrPreset.onChange = [this] {
      if (!engine) return;
      engine->loadIrPreset(cbIrPreset.getSelectedId() - 1);
      refreshWaveform();
      repaint();
    };
    btnPrevIr.onClick = [this] {
      if (!engine) return;
      const int n = ConvolutionReverbEngine::kNumFactoryPresets;
      engine->loadIrPreset((engine->getParams().irPresetIdx - 1 + n) % n);
      syncFromEngine();
    };
    btnNextIr.onClick = [this] {
      if (!engine) return;
      const int n = ConvolutionReverbEngine::kNumFactoryPresets;
      engine->loadIrPreset((engine->getParams().irPresetIdx + 1) % n);
      syncFromEngine();
    };
    btnLoadWav.onClick = [this] { openIrChooser(); };
    addAndMakeVisible(btnPrevIr);
    addAndMakeVisible(btnNextIr);
    addAndMakeVisible(btnLoadWav);
    addAndMakeVisible(cbIrPreset);

    mkKnob(knobBlend, "Blend", "%", 0.0f, 100.0f, 60.0f, false, cyan, 0);
    mkKnob(knobIrDecay, "Decay", "s", 0.2f, 10.0f, 3.5f, false, cyan, 2);
    mkKnob(knobIrSize, "Size", "%", 25.0f, 200.0f, 50.0f, false, cyan, 0);
    mkKnob(knobIrDamp, "Damping", "%", 0.0f, 100.0f, 50.0f, false, cyan, 0);
    mkKnob(knobIrLoDamp, "Lo Damp", "%", 0.0f, 100.0f, 50.0f, false, cyan, 0);
    mkKnob(knobIrDist, "Distance", "%", 0.0f, 100.0f, 50.0f, false, cyan, 0);

    knobBlend->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.irBlendPct = v; engine->setParams(p);
    };
    knobIrDecay->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.irDecaySec = v; engine->setParams(p); refreshWaveform();
    };
    knobIrSize->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.irSizePct = v; engine->setParams(p); refreshWaveform();
    };
    knobIrDamp->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.irDampingPct = v; engine->setParams(p);
    };
    knobIrLoDamp->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.irLoDampPct = v; engine->setParams(p);
    };
    knobIrDist->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams(); p.irDistancePct = v; engine->setParams(p);
    };

    // --- Tab 2: EQ Controls ---
    btnPreAlgo.setButtonText("Pre Algo");
    btnPreAlgo.onClick = [this] {
      if (!engine) return; auto p = engine->getParams(); p.eqPreAlgo = !p.eqPreAlgo; engine->setParams(p);
      updateButtons();
    };
    addAndMakeVisible(btnPreAlgo);

    cbEqType.addItem("HPF", 1);
    cbEqType.addItem("Low Shelf", 2);
    cbEqType.addItem("Bell", 3);
    cbEqType.addItem("High Shelf", 4);
    cbEqType.addItem("LPF", 5);
    cbEqType.onChange = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.eqBands[static_cast<size_t>(selectedEqPt)].type = cbEqType.getSelectedId() - 1;
      engine->setParams(p);
      repaint();
    };
    addAndMakeVisible(cbEqType);

    mkKnob(knobEqFreq, "Freq", "Hz", 20.0f, 18000.0f, 157.0f, false, orange, 0);
    knobEqFreq->setLogarithmic(true);
    mkKnob(knobEqGain, "Gain", "dB", -15.0f, 15.0f, 0.0f, true, orange, 1);
    mkKnob(knobEqSlope, "Slope", "dB", 12.0f, 60.0f, 60.0f, false, orange, 0);
    knobEqFreq->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams();
      p.eqBands[static_cast<size_t>(selectedEqPt)].freqHz = v;
      engine->setParams(p); repaint();
    };
    knobEqGain->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams();
      p.eqBands[static_cast<size_t>(selectedEqPt)].gainDb = v;
      engine->setParams(p); repaint();
    };
    knobEqSlope->onValueChanged = [this](float v) {
      if (!engine) return; auto p = engine->getParams();
      p.eqBands[static_cast<size_t>(selectedEqPt)].slopeDb = static_cast<int>(std::round(v / 12.0f)) * 12;
      engine->setParams(p); repaint();
    };

    syncFromEngine();
    updateTabVisibility();
    startTimerHz(25);
  }

  ~HybridReverbPanel() override { stopTimer(); }

  void setEngine(HybridReverbEngine &eng) {
    engine = &eng;
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    if (!engine) return;
    const auto &p = engine->getParams();
    knobSend->setValue(p.sendDb, juce::dontSendNotification);
    knobPredelay->setValue(p.preDelayMs, juce::dontSendNotification);
    knobFeedback->setValue(p.feedbackPct, juce::dontSendNotification);
    knobStereo->setValue(p.stereoSpread * 100.0f, juce::dontSendNotification);
    knobDryWet->setValue(p.dryWet * 100.0f, juce::dontSendNotification);

    knobAlgoAttack->setValue(p.algoAttackMs, juce::dontSendNotification);
    knobAlgoDecay->setValue(p.decay, juce::dontSendNotification);
    knobAlgoSize->setValue(p.size * 100.0f, juce::dontSendNotification);
    knobAlgoDelay->setValue(p.algoDelayMs, juce::dontSendNotification);
    knobAlgoMod->setValue(p.modDepth * 100.0f, juce::dontSendNotification);
    knobAlgoDiff->setValue(p.diffusion * 100.0f, juce::dontSendNotification);
    cbRouting.setSelectedId(p.routing + 1, juce::dontSendNotification);
    cbAlgo.setSelectedId(p.algorithm + 1, juce::dontSendNotification);

    cbIrPreset.setSelectedId(p.irPresetIdx + 1, juce::dontSendNotification);
    knobBlend->setValue(p.irBlendPct, juce::dontSendNotification);
    knobIrDecay->setValue(p.irDecaySec, juce::dontSendNotification);
    knobIrSize->setValue(p.irSizePct, juce::dontSendNotification);
    knobIrDamp->setValue(p.irDampingPct, juce::dontSendNotification);
    knobIrLoDamp->setValue(p.irLoDampPct, juce::dontSendNotification);
    knobIrDist->setValue(p.irDistancePct, juce::dontSendNotification);

    syncEqControls();
    refreshWaveform();
    updateButtons();
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (activeTab == 1 && engine && eqGraphRect.contains(e.position)) {
      const auto &bands = engine->getParams().eqBands;
      int best = selectedEqPt;
      float bestDist = 1.0e9f;
      for (int i = 0; i < 4; ++i) {
        const auto &eb = bands[static_cast<size_t>(i)];
        const float nx = freqToX(eb.freqHz, eqGraphRect);
        const float ny = dbToY(eb.type == 0 || eb.type == 4 ? 0.0f : eb.gainDb, eqGraphRect);
        const float d = e.position.getDistanceFrom({nx, ny});
        if (d < bestDist) { bestDist = d; best = i; }
      }
      selectedEqPt = best;
      syncEqControls();
      dragEq(e.position);
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (activeTab == 1 && engine && eqGraphRect.contains(e.position))
      dragEq(e.position);
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff2b2b2e));

    // Header
    g.setColour(juce::Colour(0xff1e1e21));
    g.fillRoundedRectangle(headerRect, 4.0f);
    g.setColour(juce::Colour(0xfff59e0b));
    g.fillEllipse(headerRect.getX() + 10.0f, headerRect.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("Hybrid Reverb", headerRect.reduced(28.0f, 0.0f).toNearestInt(),
               juce::Justification::centredLeft);

    // Side strips
    g.setColour(juce::Colour(0xff232326));
    g.fillRoundedRectangle(leftStripRect, 4.0f);
    g.fillRoundedRectangle(rightStripRect, 4.0f);

    if (activeTab == 0) {
      drawOrangeWaveform(g, waveRect);
    } else {
      drawBlueEqGraph(g, eqGraphRect);
    }
  }

  void resized() override {
    auto b = getLocalBounds().reduced(8);
    headerRect = b.removeFromTop(30).toFloat();
    auto tabs = headerRect.withSizeKeepingCentre(160.0f, 22.0f).toNearestInt();
    btnTabReverb.setBounds(tabs.removeFromLeft(80).reduced(1));
    btnTabEq.setBounds(tabs.reduced(1));

    b.removeFromTop(6);
    const int sideW = juce::jlimit(92, 118, b.getWidth() * 11 / 100);
    auto leftArea = b.removeFromLeft(sideW);
    leftStripRect = leftArea.toFloat();
    b.removeFromLeft(6);
    auto rightArea = b.removeFromRight(sideW);
    rightStripRect = rightArea.toFloat();
    b.removeFromRight(6);

    // Left Strip
    auto lIn = leftArea.reduced(4);
    knobSend->setBounds(lIn.removeFromTop(lIn.getHeight() * 34 / 100));
    knobPredelay->setBounds(lIn.removeFromTop(lIn.getHeight() * 44 / 100));
    btnMsSync.setBounds(lIn.removeFromTop(22).reduced(14, 1));
    knobFeedback->setBounds(lIn);

    // Right Strip
    auto rIn = rightArea.reduced(4);
    knobStereo->setBounds(rIn.removeFromTop(rIn.getHeight() * 32 / 100));
    btnVintage.setBounds(rIn.removeFromTop(24).reduced(4, 2));
    btnBassMono.setBounds(rIn.removeFromTop(24).reduced(4, 2));
    knobDryWet->setBounds(rIn);

    // Center Tab 1 (Reverb)
    auto c1 = b;
    waveRect = c1.removeFromTop(c1.getHeight() * 36 / 100).toFloat();
    c1.removeFromTop(4);
    auto algoRow = c1.removeFromTop(c1.getHeight() * 46 / 100);
    const int aw = algoRow.getWidth() / 9;
    knobAlgoAttack->setBounds(algoRow.removeFromLeft(aw).reduced(1));
    knobAlgoDecay->setBounds(algoRow.removeFromLeft(aw).reduced(1));
    knobAlgoSize->setBounds(algoRow.removeFromLeft(aw).reduced(1));
    cbRouting.setBounds(algoRow.removeFromLeft(aw).reduced(2, 18));
    cbAlgo.setBounds(algoRow.removeFromLeft(aw).reduced(2, 18));
    btnFreeze.setBounds(algoRow.removeFromLeft(aw).reduced(3, 18));
    knobAlgoDelay->setBounds(algoRow.removeFromLeft(aw).reduced(1));
    knobAlgoMod->setBounds(algoRow.removeFromLeft(aw).reduced(1));
    knobAlgoDiff->setBounds(algoRow.reduced(1));

    c1.removeFromTop(4);
    auto convRow = c1;
    auto irSelectCol = convRow.removeFromLeft(juce::jlimit(150, 210, convRow.getWidth() * 24 / 100));
    auto irBtns = irSelectCol.removeFromTop(24);
    btnPrevIr.setBounds(irBtns.removeFromLeft(28).reduced(1));
    btnNextIr.setBounds(irBtns.removeFromLeft(28).reduced(1));
    btnLoadWav.setBounds(irBtns.reduced(1));
    cbIrPreset.setBounds(irSelectCol.removeFromTop(26).reduced(1));

    const int cw = convRow.getWidth() / 6;
    knobBlend->setBounds(convRow.removeFromLeft(cw).reduced(1));
    knobIrDecay->setBounds(convRow.removeFromLeft(cw).reduced(1));
    knobIrSize->setBounds(convRow.removeFromLeft(cw).reduced(1));
    knobIrDamp->setBounds(convRow.removeFromLeft(cw).reduced(1));
    knobIrLoDamp->setBounds(convRow.removeFromLeft(cw).reduced(1));
    knobIrDist->setBounds(convRow.reduced(1));

    // Center Tab 2 (EQ)
    auto c2 = b;
    auto eqBot = c2.removeFromBottom(82);
    eqGraphRect = c2.reduced(2).toFloat();
    btnPreAlgo.setBounds(eqBot.removeFromLeft(88).reduced(4, 24));
    cbEqType.setBounds(eqBot.removeFromLeft(110).reduced(4, 24));
    const int eqw = eqBot.getWidth() / 3;
    knobEqFreq->setBounds(eqBot.removeFromLeft(eqw).reduced(2));
    knobEqSlope->setBounds(eqBot.removeFromLeft(eqw).reduced(2));
    knobEqGain->setBounds(eqBot.reduced(2));
  }

private:
  void timerCallback() override { repaint(); }

  void dragEq(juce::Point<float> pos) {
    if (!engine) return;
    auto p = engine->getParams();
    auto &eb = p.eqBands[static_cast<size_t>(selectedEqPt)];
    const double lo = std::log10(20.0), hi = std::log10(20000.0);
    const float nx = std::clamp((pos.x - eqGraphRect.getX()) / eqGraphRect.getWidth(), 0.0f, 1.0f);
    eb.freqHz = static_cast<float>(std::pow(10.0, lo + nx * (hi - lo)));
    const float ny = std::clamp((eqGraphRect.getCentreY() - pos.y) / (eqGraphRect.getHeight() * 0.45f), -1.0f, 1.0f);
    eb.gainDb = ny * 15.0f;
    engine->setParams(p);
    syncEqControls();
    repaint();
  }

  void syncEqControls() {
    if (!engine) return;
    const auto &eb = engine->getParams().eqBands[static_cast<size_t>(selectedEqPt)];
    cbEqType.setSelectedId(eb.type + 1, juce::dontSendNotification);
    knobEqFreq->setValue(eb.freqHz, juce::dontSendNotification);
    knobEqGain->setValue(eb.gainDb, juce::dontSendNotification);
    knobEqSlope->setValue(static_cast<float>(eb.slopeDb), juce::dontSendNotification);
  }

  void updateTabVisibility() {
    const bool r = (activeTab == 0);
    const std::array<juce::Component *, 19> revComps = {
        knobAlgoAttack.get(), knobAlgoDecay.get(), knobAlgoSize.get(),
        &cbRouting, &cbAlgo, &btnFreeze, knobAlgoDelay.get(),
        knobAlgoMod.get(), knobAlgoDiff.get(), &btnPrevIr, &btnNextIr,
        &btnLoadWav, &cbIrPreset, knobBlend.get(), knobIrDecay.get(),
        knobIrSize.get(), knobIrDamp.get(), knobIrLoDamp.get(), knobIrDist.get()};
    for (auto *c : revComps)
      c->setVisible(r);

    const std::array<juce::Component *, 5> eqComps = {
        &btnPreAlgo, &cbEqType, knobEqFreq.get(), knobEqSlope.get(), knobEqGain.get()};
    for (auto *c : eqComps)
      c->setVisible(!r);

    updateButtons();
    repaint();
  }

  void updateButtons() {
    if (!engine) return;
    const auto &p = engine->getParams();
    auto style = [](juce::TextButton &b, bool on, juce::Colour onCol) {
      b.setColour(juce::TextButton::buttonColourId, on ? onCol : juce::Colour(0xff333338));
      b.setColour(juce::TextButton::textColourOffId, on ? juce::Colours::black : juce::Colour(0xffd4d4d8));
    };
    style(btnTabReverb, activeTab == 0, juce::Colour(0xfff59e0b));
    style(btnTabEq, activeTab == 1, juce::Colour(0xfff59e0b));
    style(btnMsSync, !p.preDelaySync, juce::Colour(0xfff59e0b));
    btnMsSync.setButtonText(p.preDelaySync ? "Sync" : "ms");
    style(btnFreeze, p.freeze, juce::Colour(0xff38bdf8));
    style(btnBassMono, p.bassMono, juce::Colour(0xff38bdf8));
    btnBassMono.setButtonText(p.bassMono ? "Bass: Mono" : "Bass: Stereo");
    style(btnPreAlgo, p.eqPreAlgo, juce::Colour(0xfff59e0b));
    static constexpr const char *kVint[4] = {"Vintage: Off", "Vintage: Subtle", "Vintage: Old", "Vintage: 12-Bit"};
    btnVintage.setButtonText(kVint[std::clamp(p.vintageMode, 0, 3)]);
    style(btnVintage, p.vintageMode > 0, juce::Colour(0xfff59e0b));
  }

  void refreshWaveform() {
    if (engine)
      engine->getIrWaveformPeaks(waveL, waveR, 180);
  }

  void openIrChooser() {
    chooser = std::make_unique<juce::FileChooser>(
        "Load Hybrid Reverb Impulse Response", juce::File{}, "*.wav;*.aiff;*.aif;*.flac");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser &fc) {
          const auto f = fc.getResult();
          if (f.existsAsFile() && engine && engine->getConvEngine().loadWavFile(f)) {
            refreshWaveform();
            repaint();
          }
        });
  }

  void drawOrangeWaveform(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff141518));
    g.fillRoundedRectangle(r, 4.0f);
    if (!waveL.empty()) {
      const float bw = r.getWidth() / static_cast<float>(waveL.size());
      g.setColour(juce::Colour(0xfff59e0b));
      for (size_t i = 0; i < waveL.size(); ++i) {
        const float amp = std::clamp(waveL[i], 0.0f, 1.0f) * (r.getHeight() * 0.44f);
        const float x = r.getX() + static_cast<float>(i) * bw;
        g.drawVerticalLine(static_cast<int>(x), r.getCentreY() - amp, r.getCentreY() + amp);
      }
    }
    if (engine) {
      g.setColour(juce::Colours::white);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.drawText(juce::String(static_cast<int>(engine->getParams().size * 100.0f)) + "% / " +
                     juce::String(engine->getParams().decay, 2) + " s",
                 r.reduced(8.0f, 4.0f).withHeight(16.0f).toNearestInt(),
                 juce::Justification::centredRight);
    }
  }

  static float freqToX(float f, juce::Rectangle<float> r) {
    const double lo = std::log10(20.0), hi = std::log10(20000.0);
    return r.getX() + static_cast<float>((std::log10(std::max(20.0f, f)) - lo) / (hi - lo)) * r.getWidth();
  }
  static float dbToY(float db, juce::Rectangle<float> r) {
    return r.getCentreY() - (db / 15.0f) * (r.getHeight() * 0.45f);
  }

  void drawBlueEqGraph(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff141518));
    g.fillRoundedRectangle(r, 4.0f);

    static constexpr float kGridF[] = {100, 1000, 10000};
    for (float f : kGridF) {
      const float x = freqToX(f, r);
      g.setColour(juce::Colour(0xff27272a));
      g.drawVerticalLine(static_cast<int>(x), r.getY(), r.getBottom());
    }
    g.setColour(juce::Colour(0xff27272a));
    g.drawHorizontalLine(static_cast<int>(r.getCentreY()), r.getX(), r.getRight());

    if (!engine) return;
    juce::Path curve;
    const int n = 160;
    for (int i = 0; i <= n; ++i) {
      const double f = 20.0 * std::pow(1000.0, static_cast<double>(i) / n);
      const float db = static_cast<float>(std::clamp(engine->getEqMagnitudeDb(f), -16.0, 16.0));
      const float x = freqToX(static_cast<float>(f), r);
      const float y = std::clamp(dbToY(db, r), r.getY() + 2.0f, r.getBottom() - 2.0f);
      if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xff38bdf8));
    g.strokePath(curve, juce::PathStrokeType(2.2f));

    // 4 orange numbered points
    const auto &bands = engine->getParams().eqBands;
    for (int i = 0; i < 4; ++i) {
      const auto &eb = bands[static_cast<size_t>(i)];
      const float px = freqToX(eb.freqHz, r);
      const float py = dbToY((eb.type == 0 || eb.type == 4) ? 0.0f : eb.gainDb, r);
      const bool sel = (i == selectedEqPt);
      g.setColour(sel ? juce::Colour(0xfff59e0b) : juce::Colour(0xffea580c));
      g.fillEllipse(px - 9.0f, py - 9.0f, 18.0f, 18.0f);
      g.setColour(juce::Colours::black);
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.drawText(juce::String(i + 1), static_cast<int>(px - 9), static_cast<int>(py - 9),
                 18, 18, juce::Justification::centred);
    }
  }

  HybridReverbEngine *engine = nullptr;
  std::unique_ptr<juce::FileChooser> chooser;
  std::vector<float> waveL, waveR;
  int activeTab = 0;
  int selectedEqPt = 0;

  juce::Rectangle<float> headerRect, leftStripRect, rightStripRect, waveRect, eqGraphRect;
  juce::TextButton btnTabReverb, btnTabEq, btnMsSync, btnVintage, btnBassMono, btnFreeze;
  juce::TextButton btnPrevIr, btnNextIr, btnLoadWav, btnPreAlgo;
  juce::ComboBox cbRouting, cbAlgo, cbIrPreset, cbEqType;

  std::unique_ptr<NovaRotaryKnob> knobSend, knobPredelay, knobFeedback, knobStereo, knobDryWet;
  std::unique_ptr<NovaRotaryKnob> knobAlgoAttack, knobAlgoDecay, knobAlgoSize, knobAlgoDelay, knobAlgoMod, knobAlgoDiff;
  std::unique_ptr<NovaRotaryKnob> knobBlend, knobIrDecay, knobIrSize, knobIrDamp, knobIrLoDamp, knobIrDist;
  std::unique_ptr<NovaRotaryKnob> knobEqFreq, knobEqSlope, knobEqGain;
};

} // namespace nova
