#pragma once

#include "Effects/GravitonConvolutionReverb.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace nova {

// ==============================================================================
// ConvolutionReverbPanel — Dedicated Hardware-Style True IR Editor (Slot 21)
//   - Walnut rack sides + dark brushed metal + silver knobs + blue LEDs
//   - Header: Preset < / >, Folder (Load WAV), Save, A/B, Gear, Stereo 64-bit, Power
//   - Left Preset Browser: Factory / User / Favorites tabs, 8 Categories, Search
//   - Center: Dual L/R IR Waveform Display + Info Strip + 5 Main Knobs
//   - Bottom: IR Lo/Hi Cut (with Power), 3-Band IR EQ, Utilities (Reverse/Norm/Trim)
//   - Right: Stereo IN / OUT LED Meters (Green / Yellow / Red)
// ==============================================================================
class ConvolutionReverbPanel final : public juce::Component,
                                     private juce::Timer {
public:
  explicit ConvolutionReverbPanel(ConvolutionReverbEngine &eng)
      : engine(&eng) {
    setOpaque(true);

    // --- 1. Header ---
    btnPrevPreset.setButtonText("<");
    btnNextPreset.setButtonText(">");
    btnFolder.setButtonText("LOAD IR");
    btnSave.setButtonText("SAVE");
    btnA.setButtonText("A");
    btnB.setButtonText("B");
    btnGear.setButtonText("64B");
    btnPower.setButtonText("PWR");

    lblPresetTitle.setJustificationType(juce::Justification::centred);
    lblPresetTitle.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
    lblPresetTitle.setColour(juce::Label::backgroundColourId,
                             juce::Colour(0xff0b121c));
    lblPresetTitle.setColour(juce::Label::textColourId,
                             juce::Colour(0xff7dd3fc));
    addAndMakeVisible(lblPresetTitle);

    btnPrevPreset.onClick = [this] {
      if (!engine)
        return;
      const int n = ConvolutionReverbEngine::kNumFactoryPresets;
      engine->loadFactoryPreset((engine->getCurrentPresetIndex() - 1 + n) % n);
      syncFromEngine();
    };
    btnNextPreset.onClick = [this] {
      if (!engine)
        return;
      const int n = ConvolutionReverbEngine::kNumFactoryPresets;
      engine->loadFactoryPreset((engine->getCurrentPresetIndex() + 1) % n);
      syncFromEngine();
    };
    btnFolder.onClick = [this] { openIrFileChooser(); };
    btnSave.onClick = [this] {
      if (!engine)
        return;
      engine->saveUserPreset();
      btnSave.setButtonText("SAVED");
    };
    btnA.onClick = [this] {
      if (!engine)
        return;
      engine->selectSlotB(false);
      syncFromEngine();
    };
    btnB.onClick = [this] {
      if (!engine)
        return;
      engine->selectSlotB(true);
      syncFromEngine();
    };
    btnPower.onClick = [this] {
      if (!engine)
        return;
      auto p = engine->getParams();
      p.bypassed = !p.bypassed;
      engine->setParams(p);
      updateButtons();
    };

    for (auto *b : {&btnPrevPreset, &btnNextPreset, &btnFolder, &btnSave, &btnA,
                    &btnB, &btnGear, &btnPower})
      addAndMakeVisible(b);

    // --- 2. Preset Browser ---
    btnTabFactory.setButtonText("Factory");
    btnTabUser.setButtonText("User");
    btnTabFav.setButtonText("Favorites *");
    btnTabFactory.onClick = [this] {
      browserTab = 0;
      rebuildPresetList();
      updateButtons();
    };
    btnTabUser.onClick = [this] {
      browserTab = 1;
      if (engine && engine->hasUserPreset()) {
        engine->loadUserPreset();
        syncFromEngine();
      }
      rebuildPresetList();
      updateButtons();
    };
    btnTabFav.onClick = [this] {
      browserTab = 2;
      rebuildPresetList();
      updateButtons();
    };
    addAndMakeVisible(btnTabFactory);
    addAndMakeVisible(btnTabUser);
    addAndMakeVisible(btnTabFav);

    static constexpr const char *kCats[8] = {
        "Halls",    "Rooms",    "Plates", "Chambers",
        "Churches", "Ambience", "Spaces", "FX"};
    for (int i = 0; i < 8; ++i) {
      btnCats[static_cast<size_t>(i)].setButtonText(kCats[i]);
      btnCats[static_cast<size_t>(i)].onClick = [this, i] {
        selectedCategory = i;
        browserTab = 0;
        rebuildPresetList();
        updateButtons();
      };
      addAndMakeVisible(btnCats[static_cast<size_t>(i)]);
    }

    txtSearch.setTextToShowWhenEmpty("Search Preset...",
                                     juce::Colour(0xff64748b));
    txtSearch.setColour(juce::TextEditor::backgroundColourId,
                        juce::Colour(0xff0b121c));
    txtSearch.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    txtSearch.setColour(juce::TextEditor::outlineColourId,
                        juce::Colour(0xff1e293b));
    txtSearch.onTextChange = [this] { rebuildPresetList(); };
    addAndMakeVisible(txtSearch);

    for (int i = 0; i < 6; ++i) {
      auto &pb = btnPresetItems[static_cast<size_t>(i)];
      pb.onClick = [this, i] {
        if (!engine)
          return;
        if (i < static_cast<int>(filteredPresetIndices.size())) {
          engine->loadFactoryPreset(
              filteredPresetIndices[static_cast<size_t>(i)]);
          syncFromEngine();
        }
      };
      addAndMakeVisible(pb);
    }

    btnFavToggle.setButtonText("*");
    btnFavToggle.onClick = [this] {
      if (!engine)
        return;
      engine->toggleFavorite(engine->getCurrentPresetIndex());
      rebuildPresetList();
      updateButtons();
    };
    addAndMakeVisible(btnFavToggle);

    // --- 4. Main Parameter Controls ---
    auto mkKnob = [this](std::unique_ptr<NovaRotaryKnob> &k, const char *name,
                         const char *unit, float lo, float hi, float def,
                         bool bipolar, juce::Colour c, int dec) {
      k = std::make_unique<NovaRotaryKnob>(name, unit, lo, hi, def, bipolar, c);
      k->setDisplayDecimals(dec);
      addAndMakeVisible(*k);
    };

    const juce::Colour blueLed(0xff38bdf8);
    mkKnob(knobPreDelay, "PREDELAY", "ms", 0.0f, 500.0f, 24.0f, false, blueLed, 1);
    mkKnob(knobDecay, "DECAY", "x", 0.2f, 3.0f, 1.0f, false, blueLed, 2);
    mkKnob(knobDryWet, "DRY / WET", "%", 0.0f, 100.0f, 40.0f, false, blueLed, 0);
    mkKnob(knobWidth, "STEREO WIDTH", "%", 0.0f, 200.0f, 130.0f, false, blueLed, 0);
    mkKnob(knobSize, "SIZE", "%", 50.0f, 200.0f, 100.0f, false, blueLed, 0);

    knobPreDelay->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.preDelayMs = v;
      engine->setParams(p);
    };
    knobDecay->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.decayScale = v;
      engine->setParams(p);
      refreshWaveform();
    };
    knobDryWet->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.dryWet = v * 0.01f;
      engine->setParams(p);
    };
    knobWidth->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.stereoWidth = v * 0.01f;
      engine->setParams(p);
    };
    knobSize->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.sizePct = v;
      engine->setParams(p);
      refreshWaveform();
    };

    // --- 5. IR Filtering ---
    btnLoCutPwr.setButtonText("PWR");
    btnHiCutPwr.setButtonText("PWR");
    btnLoCutPwr.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.loCutEnabled = !p.loCutEnabled;
      engine->setParams(p);
      updateButtons();
    };
    btnHiCutPwr.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.hiCutEnabled = !p.hiCutEnabled;
      engine->setParams(p);
      updateButtons();
    };
    addAndMakeVisible(btnLoCutPwr);
    addAndMakeVisible(btnHiCutPwr);

    mkKnob(knobLoCut, "IR LO CUT", "Hz", 20.0f, 500.0f, 80.0f, false, blueLed, 0);
    mkKnob(knobHiCut, "IR HI CUT", "Hz", 1000.0f, 20000.0f, 16000.0f, false, blueLed, 0);
    knobLoCut->setLogarithmic(true);
    knobHiCut->setLogarithmic(true);
    knobLoCut->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.loCutHz = v;
      engine->setParams(p);
    };
    knobHiCut->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.hiCutHz = v;
      engine->setParams(p);
    };

    // --- 6. IR EQ ---
    btnEqPwr.setButtonText("PWR");
    btnEqPwr.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.eqEnabled = !p.eqEnabled;
      engine->setParams(p);
      updateButtons();
    };
    addAndMakeVisible(btnEqPwr);

    mkKnob(knobEqLow, "LOW", "dB", -12.0f, 12.0f, 0.0f, true, blueLed, 1);
    mkKnob(knobEqMid, "MID", "dB", -12.0f, 12.0f, 1.5f, true, blueLed, 1);
    mkKnob(knobEqHigh, "HIGH", "dB", -12.0f, 12.0f, -0.5f, true, blueLed, 1);
    knobEqLow->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.lowShelfGain = v;
      engine->setParams(p);
    };
    knobEqMid->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.midEqGain = v;
      engine->setParams(p);
    };
    knobEqHigh->onValueChanged = [this](float v) {
      if (!engine) return;
      auto p = engine->getParams();
      p.highShelfGain = v;
      engine->setParams(p);
    };

    // --- 7. IR Utilities ---
    btnReverse.setButtonText("Reverse");
    btnNormalize.setButtonText("Normalize");
    btnTrimIr.setButtonText("Trim IR");
    btnReverse.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.reverseIR = !p.reverseIR;
      engine->setParams(p);
      refreshWaveform();
      updateButtons();
    };
    btnNormalize.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      p.normalizeIR = !p.normalizeIR;
      engine->setParams(p);
      refreshWaveform();
      updateButtons();
    };
    btnTrimIr.onClick = [this] {
      if (!engine) return;
      auto p = engine->getParams();
      const bool trimmed = (p.irStartTrim > 0.01f || p.irEndTrim < 0.99f);
      p.irStartTrim = trimmed ? 0.0f : 0.03f;
      p.irEndTrim = trimmed ? 1.0f : 0.90f;
      engine->setParams(p);
      refreshWaveform();
      updateButtons();
    };
    addAndMakeVisible(btnReverse);
    addAndMakeVisible(btnNormalize);
    addAndMakeVisible(btnTrimIr);

    syncFromEngine();
    startTimerHz(30);
  }

  ~ConvolutionReverbPanel() override { stopTimer(); }

  void setEngine(ConvolutionReverbEngine &eng) {
    engine = &eng;
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    if (!engine)
      return;
    const auto &p = engine->getParams();
    knobPreDelay->setValue(p.preDelayMs, juce::dontSendNotification);
    knobDecay->setValue(p.decayScale, juce::dontSendNotification);
    knobDryWet->setValue(p.dryWet * 100.0f, juce::dontSendNotification);
    knobWidth->setValue(p.stereoWidth * 100.0f, juce::dontSendNotification);
    knobSize->setValue(p.sizePct, juce::dontSendNotification);
    knobLoCut->setValue(p.loCutHz, juce::dontSendNotification);
    knobHiCut->setValue(p.hiCutHz, juce::dontSendNotification);
    knobEqLow->setValue(p.lowShelfGain, juce::dontSendNotification);
    knobEqMid->setValue(p.midEqGain, juce::dontSendNotification);
    knobEqHigh->setValue(p.highShelfGain, juce::dontSendNotification);

    const auto &lib = ConvolutionReverbEngine::getFactoryLibrary();
    lblPresetTitle.setText(
        lib[static_cast<size_t>(engine->getCurrentPresetIndex())].name,
        juce::dontSendNotification);
    refreshWaveform();
    rebuildPresetList();
    updateButtons();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0b0d11));

    // Wooden rack side cheeks
    const float woodW = 10.0f;
    juce::ColourGradient wood(juce::Colour(0xff5a2e16), 0.0f, 0.0f,
                              juce::Colour(0xff381b0c), woodW, 0.0f, false);
    g.setGradientFill(wood);
    g.fillRect(b.withWidth(woodW));
    g.fillRect(b.withLeft(b.getRight() - woodW));

    // Dark brushed metal chassis
    auto plate = b.reduced(woodW, 4.0f);
    juce::ColourGradient metal(juce::Colour(0xff1e242c), 0.0f, 0.0f,
                               juce::Colour(0xff12161c), 0.0f, b.getBottom(),
                               false);
    g.setGradientFill(metal);
    g.fillRoundedRectangle(plate, 5.0f);
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(plate, 5.0f, 1.2f);

    // Header bar
    g.setColour(juce::Colour(0xff11161d));
    g.fillRoundedRectangle(headerRect, 4.0f);
    g.setColour(juce::Colour(0xfff1f5f9));
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("Convolution_Reverb",
               headerRect.reduced(10.0f, 2.0f).withHeight(18.0f),
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("True IR Convolution Reverb",
               headerRect.reduced(10.0f, 2.0f).withTop(headerRect.getY() + 18.0f),
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("STEREO  *  TRUE IR  *  64-BIT",
               infoBadgeRect.toNearestInt(), juce::Justification::centredRight);

    // Panels
    drawBox(g, browserBox, "PRESET BROWSER");
    drawBox(g, filterBox, "");
    drawBox(g, eqBox, "IR EQ");
    drawBox(g, utilBox, "UTILITIES");
    drawBox(g, meterBox, "");

    // Waveform Display
    drawWaveformDisplay(g, waveRect);

    // Stereo Input/Output LED Meters
    drawLedMeter(g, meterInRect, "IN", inDbL, inDbR);
    drawLedMeter(g, meterOutRect, "OUT", outDbL, outDbR);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(14, 8);
    headerRect = b.removeFromTop(38).toFloat();
    auto hIn = headerRect.reduced(8.0f, 5.0f).toNearestInt();
    hIn.removeFromLeft(185);
    btnPower.setBounds(hIn.removeFromRight(42).reduced(2));
    infoBadgeRect = hIn.removeFromRight(150).toFloat();
    btnGear.setBounds(hIn.removeFromRight(40).reduced(2));
    btnB.setBounds(hIn.removeFromRight(26).reduced(1));
    btnA.setBounds(hIn.removeFromRight(26).reduced(1));
    btnSave.setBounds(hIn.removeFromRight(52).reduced(2));
    btnFolder.setBounds(hIn.removeFromRight(66).reduced(2));
    auto presRow = hIn.reduced(4, 1);
    btnPrevPreset.setBounds(presRow.removeFromLeft(24));
    btnNextPreset.setBounds(presRow.removeFromRight(24));
    lblPresetTitle.setBounds(presRow.reduced(2, 0));

    b.removeFromTop(6);

    // Left Preset Browser
    const int browserW = juce::jlimit(175, 225, b.getWidth() * 21 / 100);
    browserBox = b.removeFromLeft(browserW).reduced(2).toFloat();
    auto brIn = browserBox.reduced(8.0f, 22.0f).toNearestInt();
    auto tabRow = brIn.removeFromTop(22);
    const int tw = tabRow.getWidth() / 3;
    btnTabFactory.setBounds(tabRow.removeFromLeft(tw).reduced(1));
    btnTabUser.setBounds(tabRow.removeFromLeft(tw).reduced(1));
    btnTabFav.setBounds(tabRow.reduced(1));

    brIn.removeFromTop(4);
    txtSearch.setBounds(brIn.removeFromBottom(24));
    brIn.removeFromBottom(4);

    auto catArea = brIn.removeFromTop(144);
    for (int i = 0; i < 8; ++i)
      btnCats[static_cast<size_t>(i)].setBounds(catArea.removeFromTop(18).reduced(1));

    brIn.removeFromTop(4);
    const int ph = juce::jmax(18, brIn.getHeight() / 6);
    for (int i = 0; i < 6; ++i)
      btnPresetItems[static_cast<size_t>(i)].setBounds(
          brIn.removeFromTop(ph).reduced(1));

    b.removeFromLeft(6);

    // Top Right: Waveform + Meters
    auto topMain = b.removeFromTop(b.getHeight() * 43 / 100);
    const int meterW = 92;
    meterBox = topMain.removeFromRight(meterW).reduced(2).toFloat();
    auto mIn = meterBox.reduced(8.0f, 8.0f).toNearestInt();
    meterInRect = mIn.removeFromLeft(mIn.getWidth() / 2).toFloat();
    meterOutRect = mIn.toFloat();

    topMain.removeFromRight(6);
    waveRect = topMain.reduced(2).toFloat();
    btnFavToggle.setBounds(
        waveRect.reduced(6.0f).toNearestInt().removeFromTop(20).removeFromRight(24));

    b.removeFromTop(6);

    // Bottom Strip: IR Filtering | IR EQ | Utilities
    auto botStrip = b.removeFromBottom(juce::jlimit(92, 122, b.getHeight() * 44 / 100));
    const int utilW = juce::jlimit(185, 250, botStrip.getWidth() * 27 / 100);
    const int fltW = juce::jlimit(195, 255, botStrip.getWidth() * 32 / 100);
    filterBox = botStrip.removeFromLeft(fltW).reduced(2).toFloat();
    utilBox = botStrip.removeFromRight(utilW).reduced(2).toFloat();
    eqBox = botStrip.reduced(2).toFloat();

    // Filter knobs
    auto flIn = filterBox.reduced(8.0f, 8.0f).toNearestInt();
    auto flLeft = flIn.removeFromLeft(flIn.getWidth() / 2);
    btnLoCutPwr.setBounds(flLeft.removeFromBottom(20).withSizeKeepingCentre(40, 18));
    knobLoCut->setBounds(flLeft.reduced(2));
    btnHiCutPwr.setBounds(flIn.removeFromBottom(20).withSizeKeepingCentre(40, 18));
    knobHiCut->setBounds(flIn.reduced(2));

    // EQ knobs
    btnEqPwr.setBounds(
        eqBox.reduced(8.0f, 4.0f).toNearestInt().removeFromTop(18).removeFromRight(38));
    auto eqIn = eqBox.reduced(8.0f, 18.0f).toNearestInt();
    const int eqw = eqIn.getWidth() / 3;
    knobEqLow->setBounds(eqIn.removeFromLeft(eqw).reduced(2));
    knobEqMid->setBounds(eqIn.removeFromLeft(eqw).reduced(2));
    knobEqHigh->setBounds(eqIn.reduced(2));

    // Utilities buttons
    auto utIn = utilBox.reduced(8.0f, 26.0f).toNearestInt();
    const int uw = utIn.getWidth() / 3;
    btnReverse.setBounds(utIn.removeFromLeft(uw).reduced(2, 6));
    btnNormalize.setBounds(utIn.removeFromLeft(uw).reduced(2, 6));
    btnTrimIr.setBounds(utIn.reduced(2, 6));

    // Main 5 Knobs Row
    b.removeFromBottom(6);
    const int kw = b.getWidth() / 5;
    knobPreDelay->setBounds(b.removeFromLeft(kw).reduced(2));
    knobDecay->setBounds(b.removeFromLeft(kw).reduced(2));
    knobDryWet->setBounds(b.removeFromLeft(kw).reduced(2));
    knobWidth->setBounds(b.removeFromLeft(kw).reduced(2));
    knobSize->setBounds(b.reduced(2));
  }

private:
  void timerCallback() override {
    if (!engine)
      return;
    inDbL += (engine->getInputLevelDbL() - inDbL) * 0.45f;
    inDbR += (engine->getInputLevelDbR() - inDbR) * 0.45f;
    outDbL += (engine->getOutputLevelDbL() - outDbL) * 0.45f;
    outDbR += (engine->getOutputLevelDbR() - outDbR) * 0.45f;
    repaint();
  }

  void openIrFileChooser() {
    chooser = std::make_unique<juce::FileChooser>(
        "Load Impulse Response (WAV / AIFF / FLAC)", juce::File{},
        "*.wav;*.aiff;*.aif;*.flac");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser &fc) {
          const auto f = fc.getResult();
          if (f.existsAsFile() && engine && engine->loadWavFile(f)) {
            lblPresetTitle.setText(f.getFileNameWithoutExtension(),
                                   juce::dontSendNotification);
            refreshWaveform();
            repaint();
          }
        });
  }

  void refreshWaveform() {
    if (engine)
      engine->getWaveformPeaks(waveL, waveR, 220);
  }

  void rebuildPresetList() {
    filteredPresetIndices.clear();
    const auto &lib = ConvolutionReverbEngine::getFactoryLibrary();
    const auto q = txtSearch.getText().trim().toLowerCase();
    static constexpr const char *kCats[8] = {
        "Halls",    "Rooms",    "Plates", "Chambers",
        "Churches", "Ambience", "Spaces", "FX"};
    const juce::String catName = kCats[selectedCategory];

    for (int i = 0; i < ConvolutionReverbEngine::kNumFactoryPresets; ++i) {
      const auto &e = lib[static_cast<size_t>(i)];
      if (browserTab == 2 && (!engine || !engine->isFavorite(i)))
        continue;
      if (q.isNotEmpty()) {
        if (!juce::String(e.name).toLowerCase().contains(q))
          continue;
      } else if (browserTab == 0) {
        if (juce::String(e.category) != catName)
          continue;
      }
      filteredPresetIndices.push_back(i);
    }

    for (int i = 0; i < 6; ++i) {
      auto &pb = btnPresetItems[static_cast<size_t>(i)];
      if (i < static_cast<int>(filteredPresetIndices.size())) {
        const int idx = filteredPresetIndices[static_cast<size_t>(i)];
        pb.setButtonText(lib[static_cast<size_t>(idx)].name);
        pb.setVisible(true);
      } else {
        pb.setVisible(false);
      }
    }
  }

  void drawBox(juce::Graphics &g, juce::Rectangle<float> r,
               const juce::String &title) {
    g.setColour(juce::Colour(0xff10151c));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(juce::Colour(0xff283241));
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
    if (title.isNotEmpty()) {
      g.setColour(juce::Colour(0xffcbd5e1));
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.drawText(title, r.reduced(8.0f, 4.0f).withHeight(14.0f).toNearestInt(),
                 juce::Justification::centredLeft);
    }
  }

  void drawWaveformDisplay(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff070c14));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(r, 5.0f, 1.2f);

    if (!engine)
      return;

    // Top IR metadata bar
    auto infoBar = r.reduced(8.0f, 4.0f).withHeight(18.0f);
    const juce::String info =
        "IR: " + juce::String(engine->getIrFileName()) + "   |   " +
        juce::String(engine->getIRDurationSec(), 2) + " s   |   " +
        juce::String(engine->getSampleRate() / 1000.0, 1) + " kHz   |   " +
        (engine->isIrStereo() ? "Stereo" : "Mono");
    g.setColour(juce::Colour(0xff7dd3fc));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText(info, infoBar.withTrimmedRight(28.0f).toNearestInt(),
               juce::Justification::centredLeft);

    auto plot = r.reduced(10.0f, 6.0f).withTrimmedTop(20.0f);
    auto topHalf = plot.removeFromTop(plot.getHeight() * 0.5f).reduced(0.0f, 2.0f);
    auto botHalf = plot.reduced(0.0f, 2.0f);

    auto drawWave = [&](juce::Rectangle<float> area, const std::vector<float> &wf,
                        const char *chLabel) {
      g.setColour(juce::Colour(0xff131f30));
      g.drawHorizontalLine(static_cast<int>(area.getCentreY()), area.getX(),
                           area.getRight());
      g.setColour(juce::Colour(0xff64748b));
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.drawText(chLabel, static_cast<int>(area.getX()),
                 static_cast<int>(area.getY()), 14, 14,
                 juce::Justification::centredLeft);

      if (wf.empty())
        return;
      const float bw = area.getWidth() / static_cast<float>(wf.size());
      g.setColour(juce::Colour(0xff38bdf8));
      for (size_t i = 0; i < wf.size(); ++i) {
        const float amp = std::clamp(wf[i], 0.0f, 1.0f) * (area.getHeight() * 0.48f);
        const float x = area.getX() + static_cast<float>(i) * bw;
        g.drawVerticalLine(static_cast<int>(x), area.getCentreY() - amp,
                           area.getCentreY() + amp);
      }
    };

    drawWave(topHalf, waveL, "L");
    drawWave(botHalf, waveR, "R");
  }

  void drawLedMeter(juce::Graphics &g, juce::Rectangle<float> r,
                    const juce::String &label, float dbL, float dbR) {
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText(label, r.withHeight(12.0f).toNearestInt(),
               juce::Justification::centred);
    auto bars = r.withTrimmedTop(14.0f);
    const int nLeds = 16;
    const float ledH = bars.getHeight() / static_cast<float>(nLeds);
    auto drawCol = [&](float x, float w, float db) {
      const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 63.0f);
      const int lit = static_cast<int>(std::round(norm * nLeds));
      for (int i = 0; i < nLeds; ++i) {
        const float y = bars.getBottom() - (i + 1) * ledH;
        juce::Colour c = (i >= 14) ? juce::Colour(0xffef4444)
                         : (i >= 11) ? juce::Colour(0xfffacc15)
                                     : juce::Colour(0xff22c55e);
        g.setColour(i < lit ? c : c.withAlpha(0.18f));
        g.fillRoundedRectangle(x + 1.0f, y + 1.0f, w - 2.0f, ledH - 2.0f, 1.0f);
      }
    };
    const float cw = bars.getWidth() * 0.5f;
    drawCol(bars.getX(), cw, dbL);
    drawCol(bars.getX() + cw, cw, dbR);
  }

  void updateButtons() {
    if (!engine)
      return;
    const auto &p = engine->getParams();
    auto styleBtn = [](juce::TextButton &b, bool on,
                       juce::Colour onCol = juce::Colour(0xff0284c7)) {
      b.setColour(juce::TextButton::buttonColourId,
                  on ? onCol : juce::Colour(0xff1e293b));
      b.setColour(juce::TextButton::textColourOffId,
                  on ? juce::Colours::white : juce::Colour(0xff94a3b8));
    };

    styleBtn(btnPower, !p.bypassed, juce::Colour(0xff0ea5e9));
    styleBtn(btnA, !engine->isSlotB(), juce::Colour(0xff0284c7));
    styleBtn(btnB, engine->isSlotB(), juce::Colour(0xff0284c7));
    styleBtn(btnTabFactory, browserTab == 0);
    styleBtn(btnTabUser, browserTab == 1);
    styleBtn(btnTabFav, browserTab == 2);

    for (int i = 0; i < 8; ++i)
      styleBtn(btnCats[static_cast<size_t>(i)],
               browserTab == 0 && i == selectedCategory,
               juce::Colour(0xff1d4ed8));

    const int curIdx = engine->getCurrentPresetIndex();
    for (int i = 0; i < 6; ++i) {
      if (i < static_cast<int>(filteredPresetIndices.size()))
        styleBtn(btnPresetItems[static_cast<size_t>(i)],
                 filteredPresetIndices[static_cast<size_t>(i)] == curIdx,
                 juce::Colour(0xff0369a1));
    }

    styleBtn(btnFavToggle, engine->isFavorite(curIdx), juce::Colour(0xfff59e0b));
    styleBtn(btnLoCutPwr, p.loCutEnabled, juce::Colour(0xff0ea5e9));
    styleBtn(btnHiCutPwr, p.hiCutEnabled, juce::Colour(0xff0ea5e9));
    styleBtn(btnEqPwr, p.eqEnabled, juce::Colour(0xff0ea5e9));
    styleBtn(btnReverse, p.reverseIR, juce::Colour(0xff0284c7));
    styleBtn(btnNormalize, p.normalizeIR, juce::Colour(0xff0284c7));
    styleBtn(btnTrimIr, p.irStartTrim > 0.01f, juce::Colour(0xff0284c7));
    btnSave.setButtonText("SAVE");
  }

  ConvolutionReverbEngine *engine = nullptr;
  std::unique_ptr<juce::FileChooser> chooser;
  std::vector<float> waveL, waveR;
  std::vector<int> filteredPresetIndices;

  int browserTab = 0;
  int selectedCategory = 0;
  float inDbL = -60.0f, inDbR = -60.0f, outDbL = -60.0f, outDbR = -60.0f;

  juce::Rectangle<float> headerRect, infoBadgeRect, browserBox, waveRect,
      meterBox, meterInRect, meterOutRect, filterBox, eqBox, utilBox;

  juce::TextButton btnPrevPreset, btnNextPreset, btnFolder, btnSave, btnA, btnB,
      btnGear, btnPower, btnFavToggle;
  juce::Label lblPresetTitle;

  juce::TextButton btnTabFactory, btnTabUser, btnTabFav;
  std::array<juce::TextButton, 8> btnCats;
  std::array<juce::TextButton, 6> btnPresetItems;
  juce::TextEditor txtSearch;

  std::unique_ptr<NovaRotaryKnob> knobPreDelay, knobDecay, knobDryWet,
      knobWidth, knobSize;
  juce::TextButton btnLoCutPwr, btnHiCutPwr, btnEqPwr;
  std::unique_ptr<NovaRotaryKnob> knobLoCut, knobHiCut, knobEqLow, knobEqMid,
      knobEqHigh;
  juce::TextButton btnReverse, btnNormalize, btnTrimIr;
};

} // namespace nova
