#pragma once

#include "DSP/GravitonDN360.h"
#include "GUI/NovaRotaryKnob.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace nova {

// ==============================================================================
// DN360 30-Band Graphic EQ panel (Slot 18).
//   - Log-frequency graph: EQ response curve + input/output spectrum analyser
//   - 30 vertical faders (edit curve = Stereo / L / R / Mid / Side)
//   - Mode (Stereo / L-R / M-S), Transform (Bilinear / Matched), Slope x1..x4,
//     Range (+/-6 / 12 / 15), Shift Gain, In/Out Gain, Balance, Flat, Bypass
//   - Peak meters In/Out (L/R)
// ==============================================================================
class DN360Panel final : public juce::Component, private juce::Timer {
public:
  using Eq = nova::graviton::DN360InductorGEQ;

  explicit DN360Panel(Eq &e) : eq(&e) {
    setOpaque(true);

    auto initBtn = [this](juce::TextButton &b, const juce::String &t,
                          juce::Colour on) {
      b.setButtonText(t);
      b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
      b.setColour(juce::TextButton::buttonOnColourId, on);
      b.setClickingTogglesState(false);
      addAndMakeVisible(b);
    };

    initBtn(btnStereo, "STEREO", juce::Colour(0xff0ea5e9));
    initBtn(btnLR, "L / R", juce::Colour(0xff0ea5e9));
    initBtn(btnMS, "M / S", juce::Colour(0xff0ea5e9));
    btnStereo.onClick = [this] { setMode(0); };
    btnLR.onClick = [this] { setMode(1); };
    btnMS.onClick = [this] { setMode(2); };

    initBtn(btnEditA, "EDIT: L", juce::Colour(0xfff59e0b));
    initBtn(btnEditB, "EDIT: R", juce::Colour(0xfff59e0b));
    btnEditA.onClick = [this] { editCh = 0; syncFaders(); updateButtons(); };
    btnEditB.onClick = [this] { editCh = 1; syncFaders(); updateButtons(); };

    initBtn(btnBilinear, "BILINEAR", juce::Colour(0xff10b981));
    initBtn(btnMatched, "MATCHED", juce::Colour(0xff10b981));
    btnBilinear.onClick = [this] { eq->setTransform(0); updateButtons(); };
    btnMatched.onClick = [this] { eq->setTransform(1); updateButtons(); };

    static const char *slopeNames[4] = {"x1", "x2", "x3", "x4"};
    for (int i = 0; i < 4; ++i) {
      initBtn(btnSlope[static_cast<size_t>(i)], slopeNames[i],
              juce::Colour(0xff8b5cf6));
      btnSlope[static_cast<size_t>(i)].onClick = [this, i] {
        eq->setSlopeIndex(i);
        updateButtons();
      };
    }

    static const double ranges[3] = {6.0, 12.0, 15.0};
    static const char *rangeNames[3] = {"+/-6", "+/-12", "+/-15"};
    for (int i = 0; i < 3; ++i) {
      initBtn(btnRange[static_cast<size_t>(i)], rangeNames[i],
              juce::Colour(0xfffb7185));
      btnRange[static_cast<size_t>(i)].onClick = [this, i] {
        eq->setRangeDb(ranges[i]);
        applyRangeToFaders();
        syncFaders();
        updateButtons();
      };
    }

    initBtn(btnFlat, "FLAT", juce::Colour(0xff64748b));
    btnFlat.onClick = [this] {
      eq->flattenChannel(static_cast<size_t>(
          eq->getMode() == 0 ? 0 : editCh));
      if (eq->getMode() == 0)
        eq->flattenChannel(1);
      syncFaders();
    };
    initBtn(btnBypass, "BYPASS", juce::Colour(0xffef4444));
    btnBypass.onClick = [this] {
      eq->setBypassed(!eq->isBypassed());
      updateButtons();
    };

    initBtn(btnPrevPreset, "<", juce::Colour(0xff334155));
    initBtn(btnNextPreset, ">", juce::Colour(0xff334155));
    for (int i = 0; i < Eq::kNumPresets; ++i)
      cbPreset.addItem(Eq::getPresetName(i), i + 1);
    cbPreset.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e293b));
    cbPreset.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff59e0b));
    cbPreset.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff334155));
    cbPreset.onChange = [this] {
      const int idx = cbPreset.getSelectedId() - 1;
      if (idx >= 0) {
        eq->applyPreset(idx);
        editCh = 0;
        syncFromEngine();
      }
    };
    addAndMakeVisible(cbPreset);
    btnPrevPreset.onClick = [this] {
      const int next = (eq->getCurrentPresetIndex() - 1 + Eq::kNumPresets) % Eq::kNumPresets;
      eq->applyPreset(next);
      editCh = 0;
      syncFromEngine();
    };
    btnNextPreset.onClick = [this] {
      const int next = (eq->getCurrentPresetIndex() + 1) % Eq::kNumPresets;
      eq->applyPreset(next);
      editCh = 0;
      syncFromEngine();
    };

    auto mkKnob = [this](std::unique_ptr<NovaRotaryKnob> &k, const char *name,
                         const char *unit, float lo, float hi, float def,
                         juce::Colour c, int dec) {
      k = std::make_unique<NovaRotaryKnob>(name, unit, lo, hi, def, true, c);
      k->setDisplayDecimals(dec);
      addAndMakeVisible(*k);
    };
    mkKnob(knobIn, "IN GAIN", "dB", -24.0f, 24.0f, 0.0f, juce::Colour(0xff22d3ee), 1);
    mkKnob(knobOut, "OUT GAIN", "dB", -24.0f, 24.0f, 0.0f, juce::Colour(0xff34d399), 1);
    mkKnob(knobShift, "SHIFT GAIN", "x", 0.0f, 2.0f, 1.0f, juce::Colour(0xfff59e0b), 2);
    mkKnob(knobBal, "BALANCE", "%", -100.0f, 100.0f, 0.0f, juce::Colour(0xff8b5cf6), 0);
    mkKnob(knobSubEnh, "SUB ENH", "%", 0.0f, 100.0f, 0.0f, juce::Colour(0xffef4444), 0);
    mkKnob(knobWarm, "WARMTH", "%", 0.0f, 100.0f, 0.0f, juce::Colour(0xfffb7185), 0);
    mkKnob(knobSubComp, "SUB COMP", "%", 0.0f, 100.0f, 100.0f, juce::Colour(0xfffacc15), 0);
    knobIn->onValueChanged = [this](float v) { eq->setInGainDb(v); };
    knobOut->onValueChanged = [this](float v) { eq->setOutGainDb(v); };
    knobShift->onValueChanged = [this](float v) { eq->setShiftGain(v); };
    knobBal->onValueChanged = [this](float v) { eq->setBalancePct(v); };
    knobSubEnh->onValueChanged = [this](float v) { eq->setSubEnhancePct(v); };
    knobWarm->onValueChanged = [this](float v) { eq->setWarmDrivePct(v); };
    knobSubComp->onValueChanged = [this](float v) { eq->setSubCompPct(v); };

    for (size_t i = 0; i < Eq::kNumBands; ++i) {
      auto &s = faders[i];
      s.setSliderStyle(juce::Slider::LinearBarVertical);
      s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      s.setRange(-15.0, 15.0, 0.1);
      s.setDoubleClickReturnValue(true, 0.0);
      s.setColour(juce::Slider::trackColourId, juce::Colour(0xff0ea5e9));
      s.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff0b1220));
      s.onValueChange = [this, i] {
        eq->setBandGainCh(static_cast<size_t>(eq->getMode() == 0 ? 0 : editCh),
                          i, faders[i].getValue());
        if (eq->getMode() == 0)
          eq->setBandGainCh(1, i, faders[i].getValue());
        selectedBand = static_cast<int>(i);
      };
      addAndMakeVisible(s);
    }

    syncFromEngine();
    startTimerHz(30);
  }

  ~DN360Panel() override { stopTimer(); }

  void setEngine(Eq &e) {
    eq = &e;
    syncFromEngine();
    repaint();
  }

  void syncFromEngine() {
    knobIn->setValue(static_cast<float>(eq->getInGainDb()), juce::dontSendNotification);
    knobOut->setValue(static_cast<float>(eq->getOutGainDb()), juce::dontSendNotification);
    knobShift->setValue(static_cast<float>(eq->getShiftGain()), juce::dontSendNotification);
    knobBal->setValue(static_cast<float>(eq->getBalancePct()), juce::dontSendNotification);
    knobSubEnh->setValue(static_cast<float>(eq->getSubEnhancePct()), juce::dontSendNotification);
    knobWarm->setValue(static_cast<float>(eq->getWarmDrivePct()), juce::dontSendNotification);
    knobSubComp->setValue(static_cast<float>(eq->getSubCompPct()), juce::dontSendNotification);
    cbPreset.setSelectedId(eq->getCurrentPresetIndex() + 1, juce::dontSendNotification);
    applyRangeToFaders();
    syncFaders();
    updateButtons();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff0b1220));
    g.setColour(juce::Colour(0xff111c30));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 8.0f);

    g.setColour(juce::Colour(0xfff0f3f6));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("KLARK TEKNIK DN360  //  30-BAND INDUCTOR GRAPHIC EQ",
               titleRect.toNearestInt(), juce::Justification::centredLeft);

    drawGraph(g);
    drawMeter(g, meterIn, "IN", eq->getPeakInDb(0), eq->getPeakInDb(1));
    drawMeter(g, meterOut, "OUT", eq->getPeakOutDb(0), eq->getPeakOutDb(1));

    // Fader frequency labels
    g.setFont(juce::Font(juce::FontOptions(8.5f)));
    for (size_t i = 0; i < Eq::kNumBands; ++i) {
      const auto b = faders[i].getBounds();
      const double f = Eq::kBandFrequencies[i];
      const juce::String t =
          f >= 1000.0 ? juce::String(f / 1000.0, f >= 10000.0 ? 0 : 1) + "k"
                      : juce::String(static_cast<int>(f));
      g.setColour(static_cast<int>(i) == selectedBand ? juce::Colour(0xfff59e0b)
                                                       : juce::Colour(0xff94a3b8));
      g.drawText(t, b.getX() - 4, b.getBottom() + 1, b.getWidth() + 8, 11,
                 juce::Justification::centred);
      const double gdb = eq->getBandGainCh(
          static_cast<size_t>(eq->getMode() == 0 ? 0 : editCh), i);
      g.setColour(juce::Colour(0xff64748b));
      g.drawText(juce::String(gdb, 1), b.getX() - 4, b.getY() - 11,
                 b.getWidth() + 8, 10, juce::Justification::centred);
    }
  }

  void resized() override {
    auto b = getLocalBounds().reduced(12);
    auto topBar = b.removeFromTop(24);
    auto presArea = topBar.removeFromRight(juce::jlimit(220, 320, topBar.getWidth() / 3));
    btnPrevPreset.setBounds(presArea.removeFromLeft(24).reduced(1));
    btnNextPreset.setBounds(presArea.removeFromRight(24).reduced(1));
    cbPreset.setBounds(presArea.reduced(2, 1));
    titleRect = topBar.toFloat();
    b.removeFromTop(4);

    // Top control row
    auto row = b.removeFromTop(26);
    auto place = [&row](juce::Component &c, int w) {
      c.setBounds(row.removeFromLeft(w).reduced(1, 0));
    };
    place(btnStereo, 62);
    place(btnLR, 50);
    place(btnMS, 50);
    row.removeFromLeft(8);
    place(btnEditA, 66);
    place(btnEditB, 66);
    row.removeFromLeft(8);
    place(btnBilinear, 70);
    place(btnMatched, 70);
    row.removeFromLeft(8);
    for (auto &s : btnSlope) place(s, 32);
    row.removeFromLeft(8);
    for (auto &r : btnRange) place(r, 46);
    row.removeFromLeft(8);
    place(btnFlat, 50);
    place(btnBypass, 64);

    b.removeFromTop(4);

    // Right: knobs + meters column
    auto right = b.removeFromRight(juce::jlimit(165, 250, b.getWidth() / 4));
    b.removeFromRight(6);
    auto meters = right.removeFromBottom(right.getHeight() * 34 / 100);
    meterIn = meters.removeFromLeft(meters.getWidth() / 2).reduced(6, 4).toFloat();
    meterOut = meters.reduced(6, 4).toFloat();
    const int rowH = right.getHeight() / 3;
    auto k1 = right.removeFromTop(rowH);
    knobIn->setBounds(k1.removeFromLeft(k1.getWidth() / 2).reduced(2));
    knobOut->setBounds(k1.reduced(2));
    auto k2 = right.removeFromTop(rowH);
    knobShift->setBounds(k2.removeFromLeft(k2.getWidth() / 2).reduced(2));
    knobBal->setBounds(k2.reduced(2));
    const int k3w = right.getWidth() / 3;
    knobSubEnh->setBounds(right.removeFromLeft(k3w).reduced(1));
    knobWarm->setBounds(right.removeFromLeft(k3w).reduced(1));
    knobSubComp->setBounds(right.reduced(1));

    // Faders strip at bottom
    auto faderArea = b.removeFromBottom(juce::jlimit(110, 190, b.getHeight() * 38 / 100));
    faderArea.removeFromTop(12);
    faderArea.removeFromBottom(12);
    const int fw = faderArea.getWidth() / static_cast<int>(Eq::kNumBands);
    for (size_t i = 0; i < Eq::kNumBands; ++i)
      faders[i].setBounds(faderArea.getX() + static_cast<int>(i) * fw + 3,
                          faderArea.getY(), fw - 6, faderArea.getHeight());

    b.removeFromBottom(14);
    graphRect = b.toFloat();
  }

private:
  void timerCallback() override { repaint(); }

  void setMode(int m) {
    eq->setMode(m);
    if (m == 0)
      editCh = 0;
    syncFaders();
    updateButtons();
  }

  void applyRangeToFaders() {
    const double r = eq->getRangeDb();
    for (auto &s : faders)
      s.setRange(-r, r, 0.1);
  }

  void syncFaders() {
    const size_t ch = static_cast<size_t>(eq->getMode() == 0 ? 0 : editCh);
    for (size_t i = 0; i < Eq::kNumBands; ++i)
      faders[i].setValue(eq->getBandGainCh(ch, i), juce::dontSendNotification);
    repaint();
  }

  void updateButtons() {
    const int m = eq->getMode();
    btnStereo.setToggleState(m == 0, juce::dontSendNotification);
    btnLR.setToggleState(m == 1, juce::dontSendNotification);
    btnMS.setToggleState(m == 2, juce::dontSendNotification);
    btnEditA.setButtonText(m == 2 ? "EDIT: MID" : "EDIT: L");
    btnEditB.setButtonText(m == 2 ? "EDIT: SIDE" : "EDIT: R");
    btnEditA.setEnabled(m != 0);
    btnEditB.setEnabled(m != 0);
    btnEditA.setToggleState(editCh == 0, juce::dontSendNotification);
    btnEditB.setToggleState(editCh == 1, juce::dontSendNotification);
    btnBilinear.setToggleState(eq->getTransform() == 0, juce::dontSendNotification);
    btnMatched.setToggleState(eq->getTransform() == 1, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
      btnSlope[static_cast<size_t>(i)].setToggleState(eq->getSlopeIndex() == i,
                                                      juce::dontSendNotification);
    const double r = eq->getRangeDb();
    btnRange[0].setToggleState(r < 9.0, juce::dontSendNotification);
    btnRange[1].setToggleState(r > 9.0 && r < 13.5, juce::dontSendNotification);
    btnRange[2].setToggleState(r > 13.5, juce::dontSendNotification);
    btnBypass.setToggleState(eq->isBypassed(), juce::dontSendNotification);
  }

  static float freqToX(double f, juce::Rectangle<float> r) {
    const double lo = std::log10(20.0), hi = std::log10(20000.0);
    return r.getX() + static_cast<float>((std::log10(std::max(20.0, f)) - lo) /
                                         (hi - lo)) * r.getWidth();
  }

  void drawGraph(juce::Graphics &g) {
    if (graphRect.isEmpty())
      return;
    const auto r = graphRect.reduced(26.0f, 8.0f);
    g.setColour(juce::Colour(0xff050a14));
    g.fillRoundedRectangle(graphRect, 5.0f);

    const double range = eq->getRangeDb();
    auto dbToY = [&](double db) {
      return r.getCentreY() - static_cast<float>(db / range) * (r.getHeight() * 0.5f);
    };

    // Grid: frequency decades
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    static const double gridF[] = {20,   30,   50,   100,  200,  300,   500,
                                   1000, 2000, 3000, 5000, 10000, 20000};
    for (double f : gridF) {
      const float x = freqToX(f, r);
      const bool major = (f == 100 || f == 1000 || f == 10000);
      g.setColour(juce::Colour(major ? 0xff1e3a5f : 0xff131f33));
      g.drawVerticalLine(static_cast<int>(x), r.getY(), r.getBottom());
      if (major || f == 20 || f == 20000) {
        g.setColour(juce::Colour(0xff64748b));
        const juce::String t = f >= 1000 ? juce::String(f / 1000.0, 0) + "k"
                                         : juce::String(static_cast<int>(f));
        g.drawText(t, static_cast<int>(x) - 14, static_cast<int>(r.getBottom()) + 1,
                   28, 10, juce::Justification::centred);
      }
    }
    // Grid: dB lines
    for (double db = -range; db <= range + 0.01; db += (range > 8.0 ? 5.0 : 3.0)) {
      const float y = dbToY(db);
      g.setColour(std::abs(db) < 0.01 ? juce::Colour(0xff2b4a73) : juce::Colour(0xff131f33));
      g.drawHorizontalLine(static_cast<int>(y), r.getX(), r.getRight());
      g.setColour(juce::Colour(0xff64748b));
      g.drawText(juce::String(static_cast<int>(db)), static_cast<int>(r.getX()) - 24,
                 static_cast<int>(y) - 5, 22, 10, juce::Justification::centredRight);
    }

    // Spectrum analyser: -90..0 dBFS mapped to bottom..top (input dim, output bright)
    auto specY = [&](float db) {
      const float n = juce::jlimit(0.0f, 1.0f, (db + 90.0f) / 90.0f);
      return r.getBottom() - n * r.getHeight();
    };
    auto drawSpectrum = [&](bool input, juce::Colour c) {
      juce::Path p;
      p.startNewSubPath(freqToX(Eq::kBandFrequencies[0], r), r.getBottom());
      for (size_t i = 0; i < Eq::kNumBands; ++i) {
        const float db = input ? eq->getInputSpectrumDb(i) : eq->getOutputSpectrumDb(i);
        p.lineTo(freqToX(Eq::kBandFrequencies[i], r), specY(db));
      }
      p.lineTo(freqToX(Eq::kBandFrequencies[Eq::kNumBands - 1], r), r.getBottom());
      p.closeSubPath();
      g.setColour(c.withAlpha(input ? 0.14f : 0.26f));
      g.fillPath(p);
      g.setColour(c.withAlpha(input ? 0.45f : 0.8f));
      g.strokePath(p, juce::PathStrokeType(1.0f));
    };
    drawSpectrum(true, juce::Colour(0xff94a3b8));
    drawSpectrum(false, juce::Colour(0xff22d3ee));

    // EQ response curve(s)
    auto drawCurve = [&](size_t ch, juce::Colour c, float thick) {
      juce::Path p;
      const int n = 220;
      for (int i = 0; i <= n; ++i) {
        const double f = 20.0 * std::pow(1000.0, static_cast<double>(i) / n);
        const double db = std::clamp(eq->getMagnitudeDb(ch, f), -range * 1.4, range * 1.4);
        const float x = freqToX(f, r);
        const float y = juce::jlimit(r.getY(), r.getBottom(), dbToY(db));
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
      }
      g.setColour(c);
      g.strokePath(p, juce::PathStrokeType(thick));
    };
    if (eq->getMode() == 0) {
      drawCurve(0, juce::Colour(0xfff59e0b), 2.2f);
    } else {
      drawCurve(0, juce::Colour(editCh == 0 ? 0xfff59e0b : 0x88f59e0b), editCh == 0 ? 2.4f : 1.4f);
      drawCurve(1, juce::Colour(editCh == 1 ? 0xff10b981 : 0x8810b981), editCh == 1 ? 2.4f : 1.4f);
    }

    // Band markers at the selected band
    if (selectedBand >= 0) {
      const auto bi = static_cast<size_t>(selectedBand);
      const float x = freqToX(Eq::kBandFrequencies[bi], r);
      g.setColour(juce::Colour(0x55f59e0b));
      g.drawVerticalLine(static_cast<int>(x), r.getY(), r.getBottom());
    }
  }

  void drawMeter(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &label,
                 float dbL, float dbR) {
    if (r.isEmpty())
      return;
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(label, r.withHeight(12.0f).toNearestInt(), juce::Justification::centred);
    auto bars = r.withTrimmedTop(14.0f).withTrimmedBottom(12.0f);
    const float bw = juce::jmin(14.0f, bars.getWidth() * 0.3f);
    auto drawBar = [&](float cx, float db) {
      auto bar = juce::Rectangle<float>(cx - bw * 0.5f, bars.getY(), bw, bars.getHeight());
      g.setColour(juce::Colour(0xff0b1220));
      g.fillRoundedRectangle(bar, 2.0f);
      const float n = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
      auto fill = bar.withTop(bar.getBottom() - bar.getHeight() * n);
      juce::ColourGradient grad(juce::Colour(0xff22c55e), 0, bar.getBottom(),
                                juce::Colour(0xffef4444), 0, bar.getY(), false);
      grad.addColour(0.75, juce::Colour(0xfff59e0b));
      g.setGradientFill(grad);
      g.fillRoundedRectangle(fill, 2.0f);
      g.setColour(juce::Colour(0xff334155));
      g.drawRoundedRectangle(bar, 2.0f, 1.0f);
    };
    drawBar(bars.getCentreX() - bw, dbL);
    drawBar(bars.getCentreX() + bw, dbR);
    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("L    R", r.withTop(r.getBottom() - 12.0f).toNearestInt(),
               juce::Justification::centred);
  }

  Eq *eq;
  int editCh = 0;
  int selectedBand = -1;

  juce::Rectangle<float> titleRect, graphRect, meterIn, meterOut;
  juce::TextButton btnStereo, btnLR, btnMS, btnEditA, btnEditB, btnBilinear,
      btnMatched, btnFlat, btnBypass, btnPrevPreset, btnNextPreset;
  juce::ComboBox cbPreset;
  std::array<juce::TextButton, 4> btnSlope;
  std::array<juce::TextButton, 3> btnRange;
  std::unique_ptr<NovaRotaryKnob> knobIn, knobOut, knobShift, knobBal,
      knobSubEnh, knobWarm, knobSubComp;
  std::array<juce::Slider, Eq::kNumBands> faders;
};

} // namespace nova
