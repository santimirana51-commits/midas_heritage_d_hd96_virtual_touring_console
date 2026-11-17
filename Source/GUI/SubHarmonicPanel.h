#pragma once

#include "Effects/GravitonSubHarmonic.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace nova {

/**
 * Black fluted hardware knob LookAndFeel modeled after Brainworx bx_subsynth.
 */
class SubSynthKnobLNF : public juce::LookAndFeel_V4 {
public:
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override {
    const float radius = juce::jmin(width, height) * 0.37f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle =
        rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // White dot arc around knob
    for (int i = 0; i <= 10; ++i) {
      const float t = static_cast<float>(i) / 10.0f;
      const float a =
          rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
      const float rDot = radius + 6.0f;
      const float dx = cx + std::sin(a) * rDot;
      const float dy = cy - std::cos(a) * rDot;
      g.setColour(t <= sliderPos ? juce::Colours::white
                                 : juce::Colour(0x88ffffff));
      g.fillEllipse(dx - 1.3f, dy - 1.3f, 2.6f, 2.6f);
    }

    // Drop shadow
    g.setColour(juce::Colour(0x77000000));
    g.fillEllipse(cx - radius + 1.5f, cy - radius + 2.5f, radius * 2.0f,
                  radius * 2.0f);

    // Dark glossy bakelite body
    juce::ColourGradient capGrad(juce::Colour(0xff34363b), cx - radius * 0.5f,
                                 cy - radius * 0.6f, juce::Colour(0xff0b0c0e),
                                 cx + radius * 0.6f, cy + radius * 0.7f, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Outer rim highlight
    g.setColour(juce::Colour(0x44ffffff));
    g.drawEllipse(cx - radius + 0.6f, cy - radius + 0.6f,
                  (radius - 0.6f) * 2.0f, (radius - 0.6f) * 2.0f, 1.0f);

    // Inner cap circle
    const float innerR = radius * 0.68f;
    g.setColour(juce::Colour(0xff17191c));
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    // White pointer line
    const float pr1 = radius * 0.20f;
    const float pr2 = radius * 0.90f;
    g.setColour(slider.isEnabled() ? juce::Colours::white
                                   : juce::Colour(0xff888888));
    g.drawLine(cx + std::sin(angle) * pr1, cy - std::cos(angle) * pr1,
               cx + std::sin(angle) * pr2, cy - std::cos(angle) * pr2, 2.2f);
  }
};

/**
 * SubHarmonicPanel
 *
 * 3-Module Crimson Faceplate for Slot 17 (SubHarmonicSynthesizer):
 *   - GLOBAL (left): Mid/Stereo Synthesis Input, Gain In, Gain Out, Mix,
 *                    Mono Maker, Stereo Width, Input L/R LED Meters.
 *   - SUB (center):  Master Subharmonics, 56-80 Hz / 24-36 Hz / 36-56 Hz
 *                    bands with Trim, Solo, and LED Meters + Mixing Start.
 *   - EDGE (right):  Smooth/Harsh Mode, Squeeze, Drive, Low Cut, High Cut,
 *                    Tight Punch, Low End, Output L/R LED Meters.
 */
class SubHarmonicPanel : public juce::Component, private juce::Timer {
public:
  explicit SubHarmonicPanel(nova::graviton::SubHarmonicSynthesizer &eng)
      : engine(&eng) {
    setupControls();
    syncFromEngine();
    startTimerHz(30);
  }

  ~SubHarmonicPanel() override {
    stopTimer();
    for (auto *k : allKnobs)
      k->setLookAndFeel(nullptr);
  }

  void setEngine(nova::graviton::SubHarmonicSynthesizer &eng) {
    engine = &eng;
    syncFromEngine();
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff1a0707));

    auto body = b.reduced(6.0f);
    const float gap = 5.0f;
    const float sideW = (body.getWidth() - gap * 2.0f) * 0.285f;
    auto globalRect = body.removeFromLeft(sideW);
    body.removeFromLeft(gap);
    auto edgeRect = body.removeFromRight(sideW);
    body.removeFromRight(gap);
    auto subRect = body;

    drawCrimsonModule(g, globalRect, "Global");
    drawCrimsonModule(g, subRect, "Sub");
    drawCrimsonModule(g, edgeRect, "Edge");

    // Section dividers inside GLOBAL (M/S) and EDGE (Filter)
    g.setColour(juce::Colour(0x55ffffff));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));

    // M/S divider in Global
    const float msY = globalRect.getY() + globalRect.getHeight() * 0.56f;
    g.drawHorizontalLine(static_cast<int>(msY), globalRect.getX() + 14.0f,
                         globalRect.getCentreX() - 22.0f);
    g.drawHorizontalLine(static_cast<int>(msY), globalRect.getCentreX() + 22.0f,
                         globalRect.getRight() - 14.0f);
    g.drawText("M/S", static_cast<int>(globalRect.getCentreX() - 20.0f),
               static_cast<int>(msY - 8.0f), 40, 16,
               juce::Justification::centred);

    // Filter divider in Edge
    const float fltY = edgeRect.getY() + edgeRect.getHeight() * 0.56f;
    g.drawHorizontalLine(static_cast<int>(fltY), edgeRect.getX() + 14.0f,
                         edgeRect.getCentreX() - 28.0f);
    g.drawHorizontalLine(static_cast<int>(fltY), edgeRect.getCentreX() + 28.0f,
                         edgeRect.getRight() - 14.0f);
    g.drawText("Filter", static_cast<int>(edgeRect.getCentreX() - 26.0f),
               static_cast<int>(fltY - 8.0f), 52, 16,
               juce::Justification::centred);

    // Synthesized Frequency box inside SUB
    if (!synthBoxRect.isEmpty()) {
      g.setColour(juce::Colour(0x44ffffff));
      g.drawRoundedRectangle(synthBoxRect, 5.0f, 1.2f);
    }

    // Bottom Brainworx-style vent & badge in SUB
    auto ventRect = subRect.withTop(subRect.getBottom() - 48.0f)
                        .withHeight(14.0f)
                        .reduced(32.0f, 0.0f);
    g.setColour(juce::Colour(0xff140909));
    g.fillRoundedRectangle(ventRect, 6.0f);
    g.setColour(juce::Colour(0xff2c1414));
    for (float vx = ventRect.getX() + 8.0f; vx < ventRect.getRight() - 6.0f;
         vx += 7.0f) {
      g.fillEllipse(vx, ventRect.getCentreY() - 2.0f, 4.0f, 4.0f);
    }
    g.setColour(juce::Colour(0xffe6dcdc));
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.drawText("SUBHARMONIC SYNTH  //  bx_subsynth",
               subRect.withTop(subRect.getBottom() - 28.0f)
                   .withHeight(20.0f)
                   .toNearestInt(),
               juce::Justification::centred);

    // Input & Output Stereo LED Meters
    drawStereoLedMeter(g, inputMeterRect, "Input", inDbL, inDbR);
    drawStereoLedMeter(g, outputMeterRect, "Output", outDbL, outDbR);

    // 3 Sub-Band LED Meters
    for (int i = 0; i < 3; ++i)
      drawBandLedMeter(g, bandMeterRect[i], bandDb[i]);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(6);
    const int gap = 5;
    const int sideW = static_cast<int>((b.getWidth() - gap * 2) * 0.285f);
    auto globalArea = b.removeFromLeft(sideW);
    b.removeFromLeft(gap);
    auto edgeArea = b.removeFromRight(sideW);
    b.removeFromRight(gap);
    auto subArea = b;

    // --- GLOBAL (Left) ---
    auto gIn = globalArea.reduced(12, 10);
    gIn.removeFromTop(24); // Title
    lblSynthIn.setBounds(gIn.removeFromTop(14));
    auto synthToggleRow = gIn.removeFromTop(24);
    btnSynthMid.setBounds(
        synthToggleRow.withSizeKeepingCentre(126, 22).removeFromLeft(62));
    btnSynthStereo.setBounds(
        synthToggleRow.withSizeKeepingCentre(126, 22).removeFromRight(62));

    gIn.removeFromTop(6);
    auto rowGain = gIn.removeFromTop(82);
    layoutKnob(rowGain.removeFromLeft(rowGain.getWidth() / 2), lblGainIn,
               knobGainIn);
    layoutKnob(rowGain, lblGainOut, knobGainOut);

    auto rowMix = gIn.removeFromTop(80);
    layoutKnob(rowMix.withSizeKeepingCentre(96, 80), lblMix, knobMix);

    gIn.removeFromTop(18); // M/S divider space
    auto rowMs = gIn.removeFromTop(82);
    layoutKnob(rowMs.removeFromLeft(rowMs.getWidth() / 2), lblMonoMaker,
               knobMonoMaker);
    layoutKnob(rowMs, lblStereoWidth, knobStereoWidth);

    inputMeterRect = gIn.removeFromBottom(44).toFloat();

    // --- EDGE (Right) ---
    auto eIn = edgeArea.reduced(12, 10);
    eIn.removeFromTop(24); // Title
    lblEdgeMode.setBounds(eIn.removeFromTop(14));
    auto modeRow = eIn.removeFromTop(24);
    btnModeSmooth.setBounds(
        modeRow.withSizeKeepingCentre(134, 22).removeFromLeft(66));
    btnModeHarsh.setBounds(
        modeRow.withSizeKeepingCentre(134, 22).removeFromRight(66));

    eIn.removeFromTop(6);
    auto rowSqzDrv = eIn.removeFromTop(82);
    layoutKnob(rowSqzDrv.removeFromLeft(rowSqzDrv.getWidth() / 2), lblSqueeze,
               knobSqueeze);
    layoutKnob(rowSqzDrv, lblDrive, knobDrive);

    auto rowCuts = eIn.removeFromTop(80);
    layoutKnob(rowCuts.removeFromLeft(rowCuts.getWidth() / 2), lblLowCut,
               knobLowCut);
    layoutKnob(rowCuts, lblHighCut, knobHighCut);

    eIn.removeFromTop(18); // Filter divider space
    auto rowFlt = eIn.removeFromTop(82);
    layoutKnob(rowFlt.removeFromLeft(rowFlt.getWidth() / 2), lblTightPunch,
               knobTightPunch);
    layoutKnob(rowFlt, lblLowEnd, knobLowEnd);

    outputMeterRect = eIn.removeFromBottom(44).toFloat();

    // --- SUB (Center) ---
    auto sIn = subArea.reduced(14, 10);
    auto subTop = sIn.removeFromTop(24);
    btnStartPreset.setBounds(subTop.removeFromRight(92).reduced(2));

    lblSubharmonics.setBounds(sIn.removeFromTop(16));
    knobSubharmonics.setBounds(sIn.removeFromTop(108).withSizeKeepingCentre(118, 106));

    sIn.removeFromBottom(52); // bottom vent & logo
    lblSynthFreqTitle.setBounds(sIn.removeFromTop(16));
    synthBoxRect = sIn.toFloat();

    auto boxIn = sIn.reduced(10, 6);
    auto topBandArea = boxIn.removeFromTop(boxIn.getHeight() / 2);
    layoutSubBand(topBandArea.withSizeKeepingCentre(156, topBandArea.getHeight()),
                  0);

    auto botRow = boxIn;
    layoutSubBand(botRow.removeFromLeft(botRow.getWidth() / 2), 1);
    layoutSubBand(botRow, 2);
  }

private:
  void layoutKnob(juce::Rectangle<int> area, juce::Label &lbl,
                  juce::Slider &knob) {
    lbl.setBounds(area.removeFromTop(14));
    knob.setBounds(area.reduced(2, 0));
  }

  void layoutSubBand(juce::Rectangle<int> area, int idx) {
    lblBand[idx].setBounds(area.removeFromTop(15));
    bandMeterRect[idx] = area.removeFromBottom(14).reduced(18, 2).toFloat();
    auto mid = area;
    const int trimW = 36;
    const int soloW = 40;
    auto trimCol = mid.removeFromLeft(trimW);
    auto soloCol = mid.removeFromRight(soloW);
    knobBand[idx].setBounds(mid);
    knobTrim[idx].setBounds(trimCol.withSizeKeepingCentre(32, 32));
    btnSolo[idx].setBounds(soloCol.withSizeKeepingCentre(36, 20));
  }

  void timerCallback() override {
    if (!engine)
      return;
    inDbL += (engine->getInputLevelDbL() - inDbL) * 0.45f;
    inDbR += (engine->getInputLevelDbR() - inDbR) * 0.45f;
    outDbL += (engine->getOutputLevelDbL() - outDbL) * 0.45f;
    outDbR += (engine->getOutputLevelDbR() - outDbR) * 0.45f;
    for (int i = 0; i < 3; ++i)
      bandDb[i] += (engine->getSubBandLevelDb(i) - bandDb[i]) * 0.45f;
    repaint();
  }

  void drawCrimsonModule(juce::Graphics &g, juce::Rectangle<float> r,
                         const juce::String &title) {
    juce::ColourGradient grad(juce::Colour(0xffb51b1b), r.getX(), r.getY(),
                            juce::Colour(0xff7d0e0e), r.getX(), r.getBottom(),
                            false);
    grad.addColour(0.5, juce::Colour(0xff9c1414));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 6.0f);

    // Subtle distressed/brushed metal texture lines
    g.setColour(juce::Colour(0x0b000000));
    for (int x = static_cast<int>(r.getX()); x < static_cast<int>(r.getRight());
         x += 4)
      g.drawVerticalLine(x, r.getY() + 2.0f, r.getBottom() - 2.0f);

    g.setColour(juce::Colour(0xff380606));
    g.drawRoundedRectangle(r, 6.0f, 1.5f);

    // 4 Corner rack screws
    const float sc = 4.5f;
    auto drawScrew = [&](float sx, float sy) {
      g.setColour(juce::Colour(0xff141414));
      g.fillEllipse(sx - sc, sy - sc, sc * 2.0f, sc * 2.0f);
      g.setColour(juce::Colour(0xff525252));
      g.drawLine(sx - 2.5f, sy - 2.5f, sx + 2.5f, sy + 2.5f, 1.2f);
    };
    drawScrew(r.getX() + 9.0f, r.getY() + 9.0f);
    drawScrew(r.getRight() - 9.0f, r.getY() + 9.0f);
    drawScrew(r.getX() + 9.0f, r.getBottom() - 9.0f);
    drawScrew(r.getRight() - 9.0f, r.getBottom() - 9.0f);

    g.setColour(juce::Colour(0xfff1e8e8));
    g.setFont(juce::Font(juce::FontOptions(14.5f, juce::Font::bold)));
    g.drawText(title, r.withHeight(28.0f).toNearestInt(),
               juce::Justification::centred);
  }

  void drawStereoLedMeter(juce::Graphics &g, juce::Rectangle<float> r,
                          const juce::String &label, float dbL, float dbR) {
    if (r.isEmpty())
      return;
    g.setColour(juce::Colour(0xffe6dada));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText(label, r.withHeight(13.0f).toNearestInt(),
               juce::Justification::centred);

    auto bars = r.withTrimmedTop(14.0f).withTrimmedBottom(10.0f);
    const int nLeds = 14;
    const float ledW = (bars.getWidth() - 16.0f) / static_cast<float>(nLeds);
    const float rowH = (bars.getHeight() - 3.0f) * 0.5f;

    auto drawRow = [&](float y, const char *chName, float db) {
      g.setColour(juce::Colours::white);
      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      g.drawText(chName, static_cast<int>(bars.getX()), static_cast<int>(y),
                 12, static_cast<int>(rowH), juce::Justification::centredLeft);
      const float norm = juce::jlimit(0.0f, 1.0f, (db + 45.0f) / 45.0f);
      const int lit = static_cast<int>(std::round(norm * nLeds));
      for (int i = 0; i < nLeds; ++i) {
        const float lx = bars.getX() + 14.0f + i * ledW;
        juce::Colour onCol = (i >= 11) ? juce::Colour(0xffef4444)
                             : (i >= 8) ? juce::Colour(0xfffacc15)
                                        : juce::Colour(0xff22c55e);
        g.setColour(i < lit ? onCol : onCol.withAlpha(0.18f));
        g.fillRoundedRectangle(lx, y + 1.0f, ledW - 2.0f, rowH - 2.0f, 1.5f);
      }
    };
    drawRow(bars.getY(), "L", dbL);
    drawRow(bars.getY() + rowH + 3.0f, "R", dbR);

    g.setColour(juce::Colour(0xaaeedddd));
    g.setFont(juce::Font(juce::FontOptions(8.5f)));
    g.drawText("40   30   20   15   10   5   0",
               r.withTop(r.getBottom() - 10.0f).withTrimmedLeft(14.0f).toNearestInt(),
               juce::Justification::centred);
  }

  void drawBandLedMeter(juce::Graphics &g, juce::Rectangle<float> r, float db) {
    if (r.isEmpty())
      return;
    const int nLeds = 7;
    const float ledW = r.getWidth() / static_cast<float>(nLeds);
    const float norm = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
    const int lit = static_cast<int>(std::round(norm * nLeds));
    for (int i = 0; i < nLeds; ++i) {
      juce::Colour onCol = (i >= 5) ? juce::Colour(0xffef4444)
                           : (i >= 4) ? juce::Colour(0xfffacc15)
                                      : juce::Colour(0xff22c55e);
      g.setColour(i < lit ? onCol : onCol.withAlpha(0.20f));
      g.fillEllipse(r.getX() + i * ledW + 1.5f, r.getY() + 1.5f,
                    juce::jmin(ledW - 3.0f, r.getHeight() - 3.0f),
                    juce::jmin(ledW - 3.0f, r.getHeight() - 3.0f));
    }
  }

  void setupControls() {
    auto initLbl = [this](juce::Label &l, const juce::String &t) {
      l.setText(t, juce::dontSendNotification);
      l.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
      l.setJustificationType(juce::Justification::centred);
      l.setColour(juce::Label::textColourId, juce::Colour(0xfff4ebeb));
      addAndMakeVisible(l);
    };

    auto initKnob = [this](juce::Slider &s, double mn, double mx, double step,
                           double val, const juce::String &sfx) {
      s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
      s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 15);
      s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
      s.setColour(juce::Slider::textBoxOutlineColourId,
                  juce::Colours::transparentBlack);
      s.setRange(mn, mx, step);
      s.setValue(val, juce::dontSendNotification);
      s.setTextValueSuffix(sfx);
      s.setLookAndFeel(&knobLnf);
      addAndMakeVisible(s);
      allKnobs.push_back(&s);
    };

    // GLOBAL
    initLbl(lblSynthIn, "Synthesis Input");
    btnSynthMid.setButtonText("Mid");
    btnSynthStereo.setButtonText("Stereo");
    btnSynthMid.onClick = [this] {
      if (engine)
        engine->setSynthesisInputStereo(false);
      updateButtonStates();
    };
    btnSynthStereo.onClick = [this] {
      if (engine)
        engine->setSynthesisInputStereo(true);
      updateButtonStates();
    };
    addAndMakeVisible(btnSynthMid);
    addAndMakeVisible(btnSynthStereo);

    initLbl(lblGainIn, "Gain In");
    initKnob(knobGainIn, -12.0, 12.0, 0.1, 0.0, " dB");
    knobGainIn.onValueChange = [this] {
      if (engine)
        engine->setGainInDb(knobGainIn.getValue());
    };

    initLbl(lblGainOut, "Gain Out");
    initKnob(knobGainOut, -12.0, 12.0, 0.1, 0.0, " dB");
    knobGainOut.onValueChange = [this] {
      if (engine)
        engine->setGainOutDb(knobGainOut.getValue());
    };

    initLbl(lblMix, "Mix");
    initKnob(knobMix, 0.0, 100.0, 1.0, 100.0, " %");
    knobMix.onValueChange = [this] {
      if (engine)
        engine->setMixPct(knobMix.getValue());
    };

    initLbl(lblMonoMaker, "Mono Maker");
    initKnob(knobMonoMaker, 0.0, 200.0, 1.0, 0.0, " Hz");
    knobMonoMaker.onValueChange = [this] {
      if (engine)
        engine->setMonoMakerHz(knobMonoMaker.getValue());
    };

    initLbl(lblStereoWidth, "Stereo Width");
    initKnob(knobStereoWidth, 0.0, 200.0, 1.0, 100.0, " %");
    knobStereoWidth.onValueChange = [this] {
      if (engine)
        engine->setStereoWidthPct(knobStereoWidth.getValue());
    };

    // SUB
    btnStartPreset.setButtonText("MIX START");
    btnStartPreset.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff231010));
    btnStartPreset.setColour(juce::TextButton::textColourOffId,
                             juce::Colour(0xfff59e0b));
    btnStartPreset.onClick = [this] {
      if (engine) {
        engine->applyRecommendedStartPreset();
        syncFromEngine();
      }
    };
    addAndMakeVisible(btnStartPreset);

    initLbl(lblSubharmonics, "Subharmonics");
    initKnob(knobSubharmonics, 0.0, 100.0, 1.0, 50.0, " %");
    knobSubharmonics.onValueChange = [this] {
      if (engine)
        engine->setSubharmonicsPct(knobSubharmonics.getValue());
    };

    initLbl(lblSynthFreqTitle, "Synthesized Frequency");
    static constexpr const char *kBandNames[3] = {"56-80 Hz", "24-36 Hz",
                                                  "36-56 Hz"};
    for (int i = 0; i < 3; ++i) {
      initLbl(lblBand[i], kBandNames[i]);
      initKnob(knobBand[i], 0.0, 100.0, 1.0, (i == 0 ? 20.0 : (i == 1 ? 30.0 : 40.0)), " %");
      knobBand[i].onValueChange = [this, i] {
        if (engine)
          engine->setSubBandPct(i, knobBand[i].getValue());
      };

      knobTrim[i].setSliderStyle(juce::Slider::RotaryVerticalDrag);
      knobTrim[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      knobTrim[i].setRange(-12.0, 12.0, 0.5);
      knobTrim[i].setValue(0.0, juce::dontSendNotification);
      knobTrim[i].setTooltip("Trim (-12..+12 dB)");
      knobTrim[i].setLookAndFeel(&knobLnf);
      knobTrim[i].onValueChange = [this, i] {
        if (engine)
          engine->setSubBandTrimDb(i, knobTrim[i].getValue());
      };
      addAndMakeVisible(knobTrim[i]);
      allKnobs.push_back(&knobTrim[i]);

      btnSolo[i].setButtonText("Solo");
      btnSolo[i].onClick = [this, i] {
        if (engine)
          engine->setSubBandSolo(i, !engine->getSubBandSolo(i));
        updateButtonStates();
      };
      addAndMakeVisible(btnSolo[i]);
    }

    // EDGE
    initLbl(lblEdgeMode, "Mode");
    btnModeSmooth.setButtonText("Smooth");
    btnModeHarsh.setButtonText("Harsh");
    btnModeSmooth.onClick = [this] {
      if (engine)
        engine->setEdgeMode(0);
      updateButtonStates();
    };
    btnModeHarsh.onClick = [this] {
      if (engine)
        engine->setEdgeMode(1);
      updateButtonStates();
    };
    addAndMakeVisible(btnModeSmooth);
    addAndMakeVisible(btnModeHarsh);

    initLbl(lblSqueeze, "Squeeze");
    initKnob(knobSqueeze, 0.0, 100.0, 1.0, 0.0, " %");
    knobSqueeze.onValueChange = [this] {
      if (engine)
        engine->setSqueezePct(knobSqueeze.getValue());
    };

    initLbl(lblDrive, "Drive");
    initKnob(knobDrive, 0.0, 100.0, 1.0, 0.0, " %");
    knobDrive.onValueChange = [this] {
      if (engine)
        engine->setDrivePct(knobDrive.getValue());
    };

    initLbl(lblLowCut, "Low Cut");
    initKnob(knobLowCut, 20.0, 120.0, 1.0, 20.0, " Hz");
    knobLowCut.onValueChange = [this] {
      if (engine)
        engine->setLowCutHz(knobLowCut.getValue());
    };

    initLbl(lblHighCut, "High Cut");
    initKnob(knobHighCut, 60.0, 12000.0, 10.0, 12000.0, " Hz");
    knobHighCut.setSkewFactorFromMidPoint(400.0);
    knobHighCut.onValueChange = [this] {
      if (engine)
        engine->setHighCutHz(knobHighCut.getValue());
    };

    initLbl(lblTightPunch, "Tight Punch");
    initKnob(knobTightPunch, 0.0, 100.0, 1.0, 0.0, " %");
    knobTightPunch.onValueChange = [this] {
      if (engine)
        engine->setTightPunchPct(knobTightPunch.getValue());
    };

    initLbl(lblLowEnd, "Low End");
    initKnob(knobLowEnd, 0.0, 100.0, 1.0, 0.0, " %");
    knobLowEnd.onValueChange = [this] {
      if (engine)
        engine->setLowEndPct(knobLowEnd.getValue());
    };
  }

  void syncFromEngine() {
    if (!engine)
      return;
    knobGainIn.setValue(engine->getGainInDb(), juce::dontSendNotification);
    knobGainOut.setValue(engine->getGainOutDb(), juce::dontSendNotification);
    knobMix.setValue(engine->getMixPct(), juce::dontSendNotification);
    knobMonoMaker.setValue(engine->getMonoMakerHz(), juce::dontSendNotification);
    knobStereoWidth.setValue(engine->getStereoWidthPct(),
                             juce::dontSendNotification);

    knobSubharmonics.setValue(engine->getSubharmonicsPct(),
                              juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) {
      knobBand[i].setValue(engine->getSubBandPct(i),
                           juce::dontSendNotification);
      knobTrim[i].setValue(engine->getSubBandTrimDb(i),
                           juce::dontSendNotification);
    }

    knobSqueeze.setValue(engine->getSqueezePct(), juce::dontSendNotification);
    knobDrive.setValue(engine->getDrivePct(), juce::dontSendNotification);
    knobLowCut.setValue(engine->getLowCutHz(), juce::dontSendNotification);
    knobHighCut.setValue(engine->getHighCutHz(), juce::dontSendNotification);
    knobTightPunch.setValue(engine->getTightPunchPct(),
                            juce::dontSendNotification);
    knobLowEnd.setValue(engine->getLowEndPct(), juce::dontSendNotification);
    updateButtonStates();
  }

  void updateButtonStates() {
    if (!engine)
      return;
    const bool stereo = engine->getSynthesisInputStereo();
    btnSynthMid.setColour(juce::TextButton::buttonColourId,
                          !stereo ? juce::Colour(0xff141619)
                                  : juce::Colour(0xff521212));
    btnSynthMid.setColour(juce::TextButton::textColourOffId,
                          !stereo ? juce::Colour(0xfff59e0b)
                                  : juce::Colour(0xffd0b0b0));
    btnSynthStereo.setColour(juce::TextButton::buttonColourId,
                             stereo ? juce::Colour(0xff141619)
                                    : juce::Colour(0xff521212));
    btnSynthStereo.setColour(juce::TextButton::textColourOffId,
                             stereo ? juce::Colour(0xfff59e0b)
                                    : juce::Colour(0xffd0b0b0));

    const bool harsh = (engine->getEdgeMode() == 1);
    btnModeSmooth.setColour(juce::TextButton::buttonColourId,
                            !harsh ? juce::Colour(0xff141619)
                                   : juce::Colour(0xff521212));
    btnModeSmooth.setColour(juce::TextButton::textColourOffId,
                            !harsh ? juce::Colour(0xfff59e0b)
                                   : juce::Colour(0xffd0b0b0));
    btnModeHarsh.setColour(juce::TextButton::buttonColourId,
                           harsh ? juce::Colour(0xff141619)
                                 : juce::Colour(0xff521212));
    btnModeHarsh.setColour(juce::TextButton::textColourOffId,
                           harsh ? juce::Colour(0xfff59e0b)
                                 : juce::Colour(0xffd0b0b0));

    for (int i = 0; i < 3; ++i) {
      const bool s = engine->getSubBandSolo(i);
      btnSolo[i].setColour(juce::TextButton::buttonColourId,
                           s ? juce::Colour(0xfff59e0b)
                             : juce::Colour(0xff2c1010));
      btnSolo[i].setColour(juce::TextButton::textColourOffId,
                           s ? juce::Colours::black : juce::Colour(0xffe0c8c8));
    }
  }

  nova::graviton::SubHarmonicSynthesizer *engine = nullptr;
  SubSynthKnobLNF knobLnf;
  std::vector<juce::Slider *> allKnobs;

  float inDbL = -60.0f, inDbR = -60.0f;
  float outDbL = -60.0f, outDbR = -60.0f;
  std::array<float, 3> bandDb{{-60.0f, -60.0f, -60.0f}};

  juce::Rectangle<float> synthBoxRect;
  juce::Rectangle<float> inputMeterRect;
  juce::Rectangle<float> outputMeterRect;
  std::array<juce::Rectangle<float>, 3> bandMeterRect;

  // GLOBAL
  juce::Label lblSynthIn, lblGainIn, lblGainOut, lblMix, lblMonoMaker,
      lblStereoWidth;
  juce::TextButton btnSynthMid, btnSynthStereo;
  juce::Slider knobGainIn, knobGainOut, knobMix, knobMonoMaker, knobStereoWidth;

  // SUB
  juce::TextButton btnStartPreset;
  juce::Label lblSubharmonics, lblSynthFreqTitle;
  juce::Slider knobSubharmonics;
  std::array<juce::Label, 3> lblBand;
  std::array<juce::Slider, 3> knobBand, knobTrim;
  std::array<juce::TextButton, 3> btnSolo;

  // EDGE
  juce::Label lblEdgeMode, lblSqueeze, lblDrive, lblLowCut, lblHighCut,
      lblTightPunch, lblLowEnd;
  juce::TextButton btnModeSmooth, btnModeHarsh;
  juce::Slider knobSqueeze, knobDrive, knobLowCut, knobHighCut, knobTightPunch,
      knobLowEnd;
};

} // namespace nova
