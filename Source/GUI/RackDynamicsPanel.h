#pragma once

#include "Effects/GravitonRackDynamics.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

/**
 * Brushed-metal rotary knob LookAndFeel modeled after classic studio
 * hardware / Logic Pro Compressor faceplates.
 */
class MetallicCompressorKnobLNF : public juce::LookAndFeel_V4 {
public:
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override {
    const float radius = juce::jmin(width, height) * 0.36f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle =
        rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Outer tick ring (11 ticks around arc)
    g.setColour(juce::Colour(0xff2b2e33));
    for (int i = 0; i <= 10; ++i) {
      const float t = static_cast<float>(i) / 10.0f;
      const float a =
          rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
      const float r1 = radius + 3.5f;
      const float r2 = radius + ((i % 2 == 0) ? 7.0f : 5.0f);
      g.drawLine(cx + std::sin(a) * r1, cy - std::cos(a) * r1,
                 cx + std::sin(a) * r2, cy - std::cos(a) * r2,
                 (i % 2 == 0) ? 1.4f : 1.0f);
    }

    // Dark recessed knurled collar
    const float collarR = radius + 1.5f;
    g.setColour(juce::Colour(0xff1b1d21));
    g.fillEllipse(cx - collarR, cy - collarR, collarR * 2.0f, collarR * 2.0f);

    // Brushed aluminum cap gradient
    juce::ColourGradient capGrad(juce::Colour(0xffe6e8ec), cx - radius * 0.5f,
                                 cy - radius * 0.6f, juce::Colour(0xff8e949d),
                                 cx + radius * 0.6f, cy + radius * 0.7f, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Subtle specular rim
    g.setColour(juce::Colour(0x88ffffff));
    g.drawEllipse(cx - radius + 0.8f, cy - radius + 0.8f,
                  (radius - 0.8f) * 2.0f, (radius - 0.8f) * 2.0f, 1.0f);

    // Pointer notch
    const float pr1 = radius * 0.28f;
    const float pr2 = radius * 0.86f;
    g.setColour(slider.isEnabled() ? juce::Colour(0xff141619)
                                   : juce::Colour(0xff555960));
    g.drawLine(cx + std::sin(angle) * pr1, cy - std::cos(angle) * pr1,
               cx + std::sin(angle) * pr2, cy - std::cos(angle) * pr2, 2.4f);
  }
};

/**
 * RackDynamicsPanel
 *
 * Dedicated UI for Rack Dynamics (Slot 24 / Compressor / Expander-Gate)
 * combining the brushed-metal 7-circuit Logic-style faceplate with
 * DIE-Plugins' (a-comp / a-exp / dynamic_display.c) soft-knee transfer graph
 * and warm analog VU gain-reduction meter.
 */
class RackDynamicsPanel : public juce::Component, private juce::Timer {
public:
  explicit RackDynamicsPanel(nova::graviton::RackDynamicsEngine &eng)
      : engine(&eng) {
    setupControls();
    syncFromEngine();
    startTimerHz(30);
  }

  ~RackDynamicsPanel() override {
    stopTimer();
    for (auto *s : allKnobs)
      s->setLookAndFeel(nullptr);
  }

  void setEngine(nova::graviton::RackDynamicsEngine &eng) {
    engine = &eng;
    syncFromEngine();
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();

    // Brushed aluminum faceplate background
    juce::ColourGradient bg(juce::Colour(0xffcfd2d7), 0.0f, 0.0f,
                            juce::Colour(0xffadb2b9), 0.0f, bounds.getBottom(),
                            false);
    bg.addColour(0.5, juce::Colour(0xffc0c4ca));
    g.setGradientFill(bg);
    g.fillRect(bounds);

    // Subtle horizontal brushed-metal grain lines
    g.setColour(juce::Colour(0x09000000));
    for (int y = 0; y < getHeight(); y += 3)
      g.drawHorizontalLine(y, 0.0f, bounds.getWidth());

    // Top dark header strip
    auto topBar = bounds.withHeight(44.0f);
    g.setColour(juce::Colour(0xff212429));
    g.fillRect(topBar);
    g.setColour(juce::Colour(0xff0f1114));
    g.drawHorizontalLine(44, 0.0f, bounds.getWidth());

    // Bottom footer bar
    auto footer = bounds.withTop(bounds.getBottom() - 24.0f);
    g.setColour(juce::Colour(0xff141619));
    g.fillRect(footer);
    g.setColour(juce::Colour(0xffaeb3bb));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    const bool isExp =
        engine && engine->getMode() ==
                      static_cast<int>(
                          nova::graviton::RackDynamicsEngine::Mode::ExpanderGate);
    g.drawText(isExp ? "Rack Dynamics  |  DIE-Plugins Expander / Gate"
                     : "Rack Dynamics  |  7-Circuit Compressor (DIE-Plugins DSP)",
               footer.toNearestInt(), juce::Justification::centred);

    // Recessed panel borders (Left Input, Center Main, Right Output/Limiter)
    auto body = bounds.withTrimmedTop(50.0f).withTrimmedBottom(28.0f).reduced(8.0f);
    const float sideW = juce::jlimit(92.0f, 128.0f, body.getWidth() * 0.12f);
    const float rightSecW = juce::jlimit(205.0f, 260.0f, body.getWidth() * 0.25f);

    auto leftBox = body.removeFromLeft(sideW).reduced(3.0f);
    auto rightBox = body.removeFromRight(rightSecW).reduced(3.0f);
    auto centerBox = body.reduced(3.0f);

    drawRecessedPanel(g, leftBox);
    drawRecessedPanel(g, centerBox);
    drawRecessedPanel(g, rightBox);

    // Draw Input Meter inside leftBox
    drawVerticalMeter(g, inputMeterRect, inDbSmooth, inPeakSmooth, true);

    // Draw Output Meter inside rightBox
    drawVerticalMeter(g, outputMeterRect, outDbSmooth, outDbSmooth, false);

    // Draw Limiter activity LED
    if (!limiterLedRect.isEmpty()) {
      const bool limOn = engine && engine->getLimiterEnabled();
      const bool limAct = limOn && engine && engine->isLimiterActive();
      g.setColour(juce::Colour(0xff1a1c20));
      g.fillEllipse(limiterLedRect.expanded(1.5f));
      g.setColour(limAct ? juce::Colour(0xffff3b30)
                         : (limOn ? juce::Colour(0xff7f1d1d)
                                  : juce::Colour(0xff3a2020)));
      g.fillEllipse(limiterLedRect);
    }

    // Center Display Window: VU Meter OR DIE-Plugins Transfer Curve Graph
    if (!displayRect.isEmpty()) {
      g.setColour(juce::Colour(0xff0b0c0e));
      g.fillRoundedRectangle(displayRect.expanded(3.0f), 4.0f);

      if (showGraphView)
        drawTransferGraph(g, displayRect);
      else
        drawAnalogVuMeter(g, displayRect);
    }
  }

  void resized() override {
    auto b = getLocalBounds();

    // Top bar: 7 circuit model buttons + COMP/EXP + Side Chain / Output
    auto topBar = b.removeFromTop(44).reduced(8, 6);
    auto rightToggles = topBar.removeFromRight(235);
    btnModeCompExp.setBounds(rightToggles.removeFromLeft(78).reduced(2));
    btnSidechainMode.setBounds(rightToggles.removeFromLeft(88).reduced(2));
    btnScHpf.setBounds(rightToggles.reduced(2));

    const int modelCount = 7;
    const int btnW = juce::jmax(60, (topBar.getWidth() - 12) / modelCount);
    for (int i = 0; i < modelCount; ++i) {
      modelButtons[i].setBounds(topBar.removeFromLeft(btnW).reduced(1));
    }

    b.removeFromBottom(24); // Footer
    auto body = b.reduced(11, 8);
    const int sideW = juce::jlimit(92, 128, static_cast<int>(body.getWidth() * 0.12f));
    const int rightSecW =
        juce::jlimit(205, 260, static_cast<int>(body.getWidth() * 0.25f));

    // Left Input Strip
    auto leftArea = body.removeFromLeft(sideW).reduced(8, 8);
    lblInReadout.setBounds(leftArea.removeFromTop(22));
    auto inKnobArea = leftArea.removeFromBottom(86);
    lblInputGain.setBounds(inKnobArea.removeFromBottom(16));
    knobInputGain.setBounds(inKnobArea);
    inputMeterRect = leftArea.reduced(14, 6).toFloat();

    // Right Section (Limiter + Distortion + Mix + Output Meter/Gain)
    auto rightArea = body.removeFromRight(rightSecW).reduced(8, 8);
    auto outStrip = rightArea.removeFromRight(juce::jmin(92, rightArea.getWidth() / 2));
    lblOutReadout.setBounds(outStrip.removeFromTop(22));
    auto outKnobArea = outStrip.removeFromBottom(86);
    lblOutputGain.setBounds(outKnobArea.removeFromBottom(16));
    knobOutputGain.setBounds(outKnobArea);
    outputMeterRect = outStrip.reduced(14, 6).toFloat();

    // Limiter / Distortion / Mix column
    auto limArea = rightArea.removeFromTop(rightArea.getHeight() * 44 / 100);
    auto limHeader = limArea.removeFromTop(18);
    lblLimiter.setBounds(limHeader.removeFromLeft(limHeader.getWidth() - 16));
    limiterLedRect = limHeader.withSizeKeepingCentre(9, 9).toFloat();
    btnLimiterOn.setBounds(limArea.removeFromTop(24).reduced(18, 1));
    lblLimThresh.setBounds(limArea.removeFromTop(14));
    knobLimThresh.setBounds(limArea);

    auto distArea = rightArea.removeFromTop(rightArea.getHeight() * 48 / 100);
    lblDistortion.setBounds(distArea.removeFromTop(16));
    auto distRow1 = distArea.removeFromTop(24);
    auto distRow2 = distArea.removeFromTop(24);
    distButtons[0].setBounds(distRow1.removeFromLeft(distRow1.getWidth() / 2).reduced(2));
    distButtons[1].setBounds(distRow1.reduced(2));
    distButtons[2].setBounds(distRow2.removeFromLeft(distRow2.getWidth() / 2).reduced(2));
    distButtons[3].setBounds(distRow2.reduced(2));

    lblMix.setBounds(rightArea.removeFromBottom(16));
    knobMix.setBounds(rightArea);

    // Center Main Area
    auto centerArea = body.reduced(10, 8);
    const int dispH = juce::jlimit(125, 195, centerArea.getHeight() * 46 / 100);
    auto dispContainer = centerArea.removeFromTop(dispH);
    displayRect = dispContainer.reduced(4, 2).toFloat();

    // Meter / Graph pill buttons centered near top of display
    auto pillArea = dispContainer.withSizeKeepingCentre(148, 22);
    pillArea.setY(dispContainer.getY() + 8);
    btnViewMeter.setBounds(pillArea.removeFromLeft(74));
    btnViewGraph.setBounds(pillArea);

    centerArea.removeFromTop(6);
    auto row1 = centerArea.removeFromTop(centerArea.getHeight() / 2);
    auto row2 = centerArea;

    const int colW = row1.getWidth() / 4;
    // Row 1: Threshold, Ratio, Make Up, Auto Gain (OFF / 0 dB / -12 dB)
    layoutKnobCell(row1.removeFromLeft(colW), knobThreshold, lblThreshold);
    layoutKnobCell(row1.removeFromLeft(colW), knobRatio, lblRatio);
    layoutKnobCell(row1.removeFromLeft(colW), knobMakeup, lblMakeup);

    auto agCell = row1.reduced(6, 2);
    lblAutoGain.setBounds(agCell.removeFromTop(16));
    const int agBtnH = juce::jmax(20, (agCell.getHeight() - 6) / 3);
    autoGainButtons[0].setBounds(agCell.removeFromTop(agBtnH).reduced(6, 1));
    autoGainButtons[1].setBounds(agCell.removeFromTop(agBtnH).reduced(6, 1));
    autoGainButtons[2].setBounds(agCell.removeFromTop(agBtnH).reduced(6, 1));

    // Row 2: Knee, Attack, Release, AUTO Release
    layoutKnobCell(row2.removeFromLeft(colW), knobKnee, lblKnee);
    layoutKnobCell(row2.removeFromLeft(colW), knobAttack, lblAttack);
    layoutKnobCell(row2.removeFromLeft(colW), knobRelease, lblRelease);

    auto autoRelCell = row2.reduced(10, 14);
    btnAutoRelease.setBounds(autoRelCell.withSizeKeepingCentre(64, 28));
  }

private:
  void timerCallback() override {
    if (!engine)
      return;

    const float targetIn = engine->getInLevelDb();
    const float targetOut = engine->getOutLevelDb();
    const float targetGr = engine->getGainReductionDb();
    const float targetPeak = engine->getPeakHoldDb();

    inDbSmooth += (targetIn - inDbSmooth) * 0.45f;
    outDbSmooth += (targetOut - outDbSmooth) * 0.45f;
    grDbSmooth += (targetGr - grDbSmooth) * 0.35f;
    inPeakSmooth = targetPeak;

    auto fmtDb = [](float v) -> juce::String {
      if (v <= -59.5f)
        return "-inf dB";
      return juce::String(v, 1) + " dB";
    };
    lblInReadout.setText(fmtDb(inDbSmooth), juce::dontSendNotification);
    lblOutReadout.setText(fmtDb(outDbSmooth), juce::dontSendNotification);
    repaint();
  }

  void layoutKnobCell(juce::Rectangle<int> cell, juce::Slider &knob,
                      juce::Label &lbl) {
    lbl.setBounds(cell.removeFromTop(16));
    knob.setBounds(cell.reduced(2));
  }

  void drawRecessedPanel(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0x38ffffff));
    g.drawRoundedRectangle(r.translated(0.0f, 1.0f), 5.0f, 1.0f);
    g.setColour(juce::Colour(0xff787d85));
    g.drawRoundedRectangle(r, 5.0f, 1.2f);
  }

  void drawVerticalMeter(juce::Graphics &g, juce::Rectangle<float> r,
                         float levelDb, float peakDb, bool isInput) {
    if (r.isEmpty())
      return;
    const float barW = 7.0f;
    auto barRect = r.withWidth(barW).withX(r.getCentreX() - barW * 0.5f);

    // Track
    g.setColour(juce::Colour(0xff1b1e23));
    g.fillRoundedRectangle(barRect, 2.0f);

    const float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 63.0f);
    if (norm > 0.005f) {
      auto fillRect = barRect.withTop(barRect.getBottom() - barRect.getHeight() * norm);
      juce::ColourGradient grad(juce::Colour(0xff22c55e), fillRect.getX(),
                                fillRect.getBottom(), juce::Colour(0xffef4444),
                                fillRect.getX(), barRect.getY(), false);
      grad.addColour(0.75, juce::Colour(0xfff59e0b));
      g.setGradientFill(grad);
      g.fillRoundedRectangle(fillRect, 2.0f);
    }

    if (isInput && peakDb > -59.0f) {
      const float pNorm = juce::jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 63.0f);
      const float py = barRect.getBottom() - barRect.getHeight() * pNorm;
      g.setColour(juce::Colours::white);
      g.drawHorizontalLine(static_cast<int>(py), barRect.getX(), barRect.getRight());
    }

    // Scale markings (+3 .. -60)
    static constexpr float kMarks[] = {3, 0, -3, -6, -9, -12, -18, -24, -30, -40, -60};
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    for (float db : kMarks) {
      const float mNorm = (db + 60.0f) / 63.0f;
      const float my = barRect.getBottom() - barRect.getHeight() * mNorm;
      g.setColour(juce::Colour(0xff2c3036));
      g.drawHorizontalLine(static_cast<int>(my), barRect.getX() - 4.0f, barRect.getX() - 1.0f);
      g.drawText(db > 0 ? "+3" : juce::String(static_cast<int>(db)),
                 static_cast<int>(barRect.getX() - 26.0f),
                 static_cast<int>(my - 5.0f), 20, 10,
                 juce::Justification::centredRight);
    }
  }

  void drawAnalogVuMeter(juce::Graphics &g, juce::Rectangle<float> r) {
    // Warm amber backlit VU face
    juce::ColourGradient vuBg(juce::Colour(0xffd79e5e), r.getCentreX(),
                              r.getCentreY(), juce::Colour(0xff7e4c21),
                              r.getX(), r.getY(), true);
    g.setGradientFill(vuBg);
    g.fillRoundedRectangle(r, 3.0f);

    const float cx = r.getCentreX();
    const float cy = r.getBottom() + r.getHeight() * 0.42f;
    const float arcR = r.getHeight() * 0.92f;
    const float startAng = -0.82f; // -50 dB (left)
    const float endAng = 0.82f;    // 0 dB (right)

    // Arc scale
    juce::Path arcPath;
    arcPath.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAng, endAng, true);
    g.setColour(juce::Colour(0xff23170d));
    g.strokePath(arcPath, juce::PathStrokeType(1.6f));

    // Tick marks & dB labels (-50, -30, -20, -10, -5, 0)
    struct VuMark {
      float db;
      const char *label;
      float pos;
    };
    static constexpr VuMark kMarks[] = {
        {50.0f, "-50", 0.0f}, {30.0f, "-30", 0.22f}, {20.0f, "-20", 0.40f},
        {10.0f, "-10", 0.58f}, {5.0f, "-5", 0.78f}, {0.0f, "0", 1.0f}};

    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    for (int i = 0; i <= 20; ++i) {
      const float t = static_cast<float>(i) / 20.0f;
      const float a = startAng + t * (endAng - startAng);
      const float r1 = arcR;
      const float r2 = arcR + ((i % 2 == 0) ? 9.0f : 5.0f);
      g.setColour(juce::Colour(0xff23170d));
      g.drawLine(cx + std::sin(a) * r1, cy - std::cos(a) * r1,
                 cx + std::sin(a) * r2, cy - std::cos(a) * r2, 1.2f);
    }

    for (const auto &m : kMarks) {
      const float a = startAng + m.pos * (endAng - startAng);
      const float lr = arcR + 22.0f;
      const float lx = cx + std::sin(a) * lr;
      const float ly = cy - std::cos(a) * lr;
      g.setColour(juce::Colour(0xff1e130a));
      g.drawText(m.label, static_cast<int>(lx - 18), static_cast<int>(ly - 8),
                 36, 16, juce::Justification::centred);
    }

    // Map grDbSmooth (0..50 dB) to VU needle angle (0 dB on right, -50 dB on left)
    const float gr = juce::jlimit(0.0f, 50.0f, grDbSmooth);
    float pos = 1.0f;
    if (gr <= 5.0f)
      pos = 1.0f - (gr / 5.0f) * 0.22f;
    else if (gr <= 10.0f)
      pos = 0.78f - ((gr - 5.0f) / 5.0f) * 0.20f;
    else if (gr <= 20.0f)
      pos = 0.58f - ((gr - 10.0f) / 10.0f) * 0.18f;
    else if (gr <= 30.0f)
      pos = 0.40f - ((gr - 20.0f) / 10.0f) * 0.18f;
    else
      pos = 0.22f - ((gr - 30.0f) / 20.0f) * 0.22f;

    const float needleAng = startAng + pos * (endAng - startAng);
    const float tipR = arcR + 12.0f;
    const float baseR = arcR * 0.48f;
    g.setColour(juce::Colour(0xff14100d));
    g.drawLine(cx + std::sin(needleAng) * baseR,
               cy - std::cos(needleAng) * baseR,
               cx + std::sin(needleAng) * tipR, cy - std::cos(needleAng) * tipR,
               2.4f);

    // GR numeric badge bottom-right
    g.setColour(juce::Colour(0xaa1a120b));
    g.fillRoundedRectangle(r.getRight() - 78.0f, r.getBottom() - 22.0f, 70.0f,
                           16.0f, 3.0f);
    g.setColour(juce::Colour(0xfff5c07a));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("GR -" + juce::String(grDbSmooth, 1) + " dB",
               static_cast<int>(r.getRight() - 78),
               static_cast<int>(r.getBottom() - 22), 70, 16,
               juce::Justification::centred);
  }

  /**
   * DIE-Plugins (dynamic_display.c + a-comp.c / a-exp.c) Transfer Curve Graph
   * Range: -60 dB .. +10 dB on X and Y, plus live GR bar on right.
   */
  void drawTransferGraph(juce::Graphics &g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff23272e));
    g.fillRoundedRectangle(r, 3.0f);

    auto plot = r.reduced(24.0f, 10.0f);
    const float grBarW = 14.0f;
    auto grRect = plot.removeFromRight(grBarW + 8.0f).withWidth(grBarW);
    plot.removeFromRight(6.0f);

    auto mapX = [&](float db) {
      return plot.getX() + ((db + 60.0f) / 70.0f) * plot.getWidth();
    };
    auto mapY = [&](float db) {
      return plot.getBottom() - ((db + 60.0f) / 70.0f) * plot.getHeight();
    };

    // 10 dB grid lines
    g.setColour(juce::Colour(0x448b949e));
    for (int d = -50; d <= 0; d += 10) {
      const float gx = mapX(static_cast<float>(d));
      const float gy = mapY(static_cast<float>(d));
      g.drawVerticalLine(static_cast<int>(gx), plot.getY(), plot.getBottom());
      g.drawHorizontalLine(static_cast<int>(gy), plot.getX(), plot.getRight());
    }

    // Diagonal 1:1 unity reference
    g.setColour(juce::Colour(0x668b949e));
    g.drawLine(mapX(-60.0f), mapY(-60.0f), mapX(10.0f), mapY(10.0f), 1.0f);

    if (engine) {
      // Threshold line
      const float th = static_cast<float>(engine->getThresholdDb());
      g.setColour(juce::Colour(0x66f59e0b));
      g.drawVerticalLine(static_cast<int>(mapX(th)), plot.getY(), plot.getBottom());

      // Transfer curve
      juce::Path curve;
      for (int i = 0; i <= 70; ++i) {
        const float inDb = -60.0f + static_cast<float>(i);
        const float outDb = juce::jlimit(
            -60.0f, 10.0f, static_cast<float>(engine->computeTransferDb(inDb)));
        const float px = mapX(inDb);
        const float py = mapY(outDb);
        if (i == 0)
          curve.startNewSubPath(px, py);
        else
          curve.lineTo(px, py);
      }
      g.setColour(juce::Colour(0xfff59e0b));
      g.strokePath(curve, juce::PathStrokeType(2.2f));

      // Live operating point dot
      const float opX = juce::jlimit(-60.0f, 10.0f, engine->getOperatingPointDb());
      const float opY = juce::jlimit(
          -60.0f, 10.0f, static_cast<float>(engine->computeTransferDb(opX)));
      const float dotX = mapX(opX);
      const float dotY = mapY(opY);
      g.setColour(juce::Colour(0xffffd166));
      g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
    }

    // Gain Reduction bar (DIE-Plugins draw_GR_bar)
    g.setColour(juce::Colour(0xff14171c));
    g.fillRect(grRect);
    const float grNorm = juce::jlimit(0.0f, 1.0f, grDbSmooth / 45.0f);
    if (grNorm > 0.005f) {
      g.setColour(juce::Colour(0xffef4444));
      g.fillRect(grRect.withHeight(grRect.getHeight() * grNorm));
    }
    g.setColour(juce::Colour(0xff6e7681));
    g.drawRect(grRect, 1.0f);
  }

  void setupControls() {
    static constexpr const char *kModelNames[7] = {
        "Platinum\nDigital", "Studio\nVCA",  "Studio\nFET", "Classic\nVCA",
        "Vintage\nVCA",      "Vintage\nFET", "Vintage\nOpto"};

    for (int i = 0; i < 7; ++i) {
      modelButtons[i].setButtonText(kModelNames[i]);
      modelButtons[i].onClick = [this, i] {
        if (engine)
          engine->setCircuitModel(i);
        updateButtonStates();
      };
      addAndMakeVisible(modelButtons[i]);
    }

    btnModeCompExp.setButtonText("COMP");
    btnModeCompExp.onClick = [this] {
      if (!engine)
        return;
      const int next = (engine->getMode() == 0) ? 1 : 0;
      engine->setMode(next);
      updateButtonStates();
      repaint();
    };
    addAndMakeVisible(btnModeCompExp);

    btnSidechainMode.setButtonText("Output");
    btnSidechainMode.onClick = [this] {
      if (!engine)
        return;
      engine->setSidechainListen(!engine->getSidechainListen());
      updateButtonStates();
    };
    addAndMakeVisible(btnSidechainMode);

    btnScHpf.setButtonText("SC HPF");
    btnScHpf.onClick = [this] {
      if (!engine)
        return;
      engine->setSidechainFilterEnabled(!engine->getSidechainFilterEnabled());
      updateButtonStates();
    };
    addAndMakeVisible(btnScHpf);

    btnViewMeter.setButtonText("Meter");
    btnViewMeter.onClick = [this] {
      showGraphView = false;
      updateButtonStates();
      repaint();
    };
    addAndMakeVisible(btnViewMeter);

    btnViewGraph.setButtonText("Graph");
    btnViewGraph.onClick = [this] {
      showGraphView = true;
      updateButtonStates();
      repaint();
    };
    addAndMakeVisible(btnViewGraph);

    auto initReadout = [this](juce::Label &l) {
      l.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      l.setJustificationType(juce::Justification::centred);
      l.setColour(juce::Label::backgroundColourId, juce::Colour(0xff23262b));
      l.setColour(juce::Label::textColourId, juce::Colour(0xffe6edf3));
      l.setText("-inf dB", juce::dontSendNotification);
      addAndMakeVisible(l);
    };
    initReadout(lblInReadout);
    initReadout(lblOutReadout);

    auto initKnob = [this](juce::Slider &s, juce::Label &l,
                           const juce::String &name, double mn, double mx,
                           double step, double val, const juce::String &sfx) {
      s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
      s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
      s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff14161a));
      s.setColour(juce::Slider::textBoxOutlineColourId,
                  juce::Colours::transparentBlack);
      s.setRange(mn, mx, step);
      s.setValue(val, juce::dontSendNotification);
      s.setTextValueSuffix(sfx);
      s.setLookAndFeel(&knobLnf);
      addAndMakeVisible(s);
      allKnobs.push_back(&s);

      l.setText(name, juce::dontSendNotification);
      l.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
      l.setJustificationType(juce::Justification::centred);
      l.setColour(juce::Label::textColourId, juce::Colour(0xff1e2228));
      addAndMakeVisible(l);
    };

    initKnob(knobInputGain, lblInputGain, "INPUT GAIN", -30.0, 30.0, 0.1, 0.0, " dB");
    initKnob(knobOutputGain, lblOutputGain, "OUTPUT GAIN", -30.0, 30.0, 0.1, 0.0, " dB");
    initKnob(knobThreshold, lblThreshold, "THRESHOLD", -60.0, 0.0, 0.5, -18.0, " dB");
    initKnob(knobRatio, lblRatio, "RATIO", 1.0, 30.0, 0.1, 4.0, ":1");
    initKnob(knobMakeup, lblMakeup, "MAKE UP", -20.0, 50.0, 0.1, 0.0, " dB");
    initKnob(knobKnee, lblKnee, "KNEE", 0.0, 1.0, 0.01, 0.35, "");
    initKnob(knobAttack, lblAttack, "ATTACK", 0.1, 200.0, 0.1, 10.0, " ms");
    initKnob(knobRelease, lblRelease, "RELEASE", 5.0, 5000.0, 1.0, 120.0, " ms");
    initKnob(knobLimThresh, lblLimThresh, "THRESHOLD", -10.0, 0.0, 0.1, -1.0, " dB");
    initKnob(knobMix, lblMix, "MIX", 0.0, 100.0, 1.0, 100.0, " %");

    knobRatio.setSkewFactorFromMidPoint(4.0);
    knobRelease.setSkewFactorFromMidPoint(200.0);

    knobInputGain.onValueChange = [this] {
      if (engine)
        engine->setInputGainDb(knobInputGain.getValue());
    };
    knobOutputGain.onValueChange = [this] {
      if (engine)
        engine->setOutputGainDb(knobOutputGain.getValue());
    };
    knobThreshold.onValueChange = [this] {
      if (engine)
        engine->setThresholdDb(knobThreshold.getValue());
    };
    knobRatio.onValueChange = [this] {
      if (engine)
        engine->setRatio(knobRatio.getValue());
    };
    knobMakeup.onValueChange = [this] {
      if (engine)
        engine->setMakeupDb(knobMakeup.getValue());
    };
    knobKnee.onValueChange = [this] {
      if (engine)
        engine->setKnee(knobKnee.getValue());
    };
    knobAttack.onValueChange = [this] {
      if (engine)
        engine->setAttackMs(knobAttack.getValue());
    };
    knobRelease.onValueChange = [this] {
      if (engine)
        engine->setReleaseMs(knobRelease.getValue());
    };
    knobLimThresh.onValueChange = [this] {
      if (engine)
        engine->setLimiterThresholdDb(knobLimThresh.getValue());
    };
    knobMix.onValueChange = [this] {
      if (engine)
        engine->setMixPct(knobMix.getValue());
    };

    lblAutoGain.setText("AUTO GAIN", juce::dontSendNotification);
    lblAutoGain.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    lblAutoGain.setJustificationType(juce::Justification::centred);
    lblAutoGain.setColour(juce::Label::textColourId, juce::Colour(0xff1e2228));
    addAndMakeVisible(lblAutoGain);

    static constexpr const char *kAgNames[3] = {"OFF", "0 dB", "-12 dB"};
    for (int i = 0; i < 3; ++i) {
      autoGainButtons[i].setButtonText(kAgNames[i]);
      autoGainButtons[i].onClick = [this, i] {
        if (engine)
          engine->setAutoGainMode(i);
        updateButtonStates();
      };
      addAndMakeVisible(autoGainButtons[i]);
    }

    btnAutoRelease.setButtonText("AUTO");
    btnAutoRelease.onClick = [this] {
      if (engine)
        engine->setAutoRelease(!engine->getAutoRelease());
      updateButtonStates();
    };
    addAndMakeVisible(btnAutoRelease);

    lblLimiter.setText("LIMITER", juce::dontSendNotification);
    lblLimiter.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    lblLimiter.setJustificationType(juce::Justification::centred);
    lblLimiter.setColour(juce::Label::textColourId, juce::Colour(0xff1e2228));
    addAndMakeVisible(lblLimiter);

    btnLimiterOn.setButtonText("ON");
    btnLimiterOn.onClick = [this] {
      if (engine)
        engine->setLimiterEnabled(!engine->getLimiterEnabled());
      updateButtonStates();
    };
    addAndMakeVisible(btnLimiterOn);

    lblDistortion.setText("DISTORTION", juce::dontSendNotification);
    lblDistortion.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    lblDistortion.setJustificationType(juce::Justification::centred);
    lblDistortion.setColour(juce::Label::textColourId, juce::Colour(0xff1e2228));
    addAndMakeVisible(lblDistortion);

    static constexpr const char *kDistNames[4] = {"Off", "Soft", "Hard", "Clip"};
    for (int i = 0; i < 4; ++i) {
      distButtons[i].setButtonText(kDistNames[i]);
      distButtons[i].onClick = [this, i] {
        if (engine)
          engine->setDistortionMode(i);
        updateButtonStates();
      };
      addAndMakeVisible(distButtons[i]);
    }
  }

  void syncFromEngine() {
    if (!engine)
      return;
    knobInputGain.setValue(engine->getInputGainDb(), juce::dontSendNotification);
    knobOutputGain.setValue(engine->getOutputGainDb(), juce::dontSendNotification);
    knobThreshold.setValue(engine->getThresholdDb(), juce::dontSendNotification);
    knobRatio.setValue(engine->getRatio(), juce::dontSendNotification);
    knobMakeup.setValue(engine->getMakeupDb(), juce::dontSendNotification);
    knobKnee.setValue(engine->getKnee(), juce::dontSendNotification);
    knobAttack.setValue(engine->getAttackMs(), juce::dontSendNotification);
    knobRelease.setValue(engine->getReleaseMs(), juce::dontSendNotification);
    knobLimThresh.setValue(engine->getLimiterThresholdDb(), juce::dontSendNotification);
    knobMix.setValue(engine->getMixPct(), juce::dontSendNotification);
    updateButtonStates();
  }

  void updateButtonStates() {
    if (!engine)
      return;
    const int curModel = engine->getCircuitModel();
    for (int i = 0; i < 7; ++i) {
      const bool active = (i == curModel);
      modelButtons[i].setColour(juce::TextButton::buttonColourId,
                                active ? juce::Colour(0xff1b1e23)
                                       : juce::Colour(0xff2c3037));
      modelButtons[i].setColour(juce::TextButton::textColourOffId,
                                active ? juce::Colour(0xfff59e0b)
                                       : juce::Colour(0xff9aa1ab));
    }

    const bool isExp = (engine->getMode() == 1);
    btnModeCompExp.setButtonText(isExp ? "EXP/GATE" : "COMP");
    btnModeCompExp.setColour(juce::TextButton::buttonColourId,
                             isExp ? juce::Colour(0xff1f6feb)
                                   : juce::Colour(0xff2c3037));
    btnModeCompExp.setColour(juce::TextButton::textColourOffId,
                             juce::Colours::white);

    const bool scListen = engine->getSidechainListen();
    btnSidechainMode.setButtonText(scListen ? "Side Chain" : "Output");
    btnSidechainMode.setColour(juce::TextButton::buttonColourId,
                               scListen ? juce::Colour(0xffd97706)
                                        : juce::Colour(0xff14161a));
    btnSidechainMode.setColour(juce::TextButton::textColourOffId,
                               juce::Colours::white);

    const bool scHpf = engine->getSidechainFilterEnabled();
    btnScHpf.setColour(juce::TextButton::buttonColourId,
                       scHpf ? juce::Colour(0xff238636)
                             : juce::Colour(0xff2c3037));
    btnScHpf.setColour(juce::TextButton::textColourOffId,
                       scHpf ? juce::Colours::white : juce::Colour(0xff9aa1ab));

    btnViewMeter.setColour(juce::TextButton::buttonColourId,
                           !showGraphView ? juce::Colour(0xffe0b27d)
                                          : juce::Colour(0xff3a2a1b));
    btnViewMeter.setColour(juce::TextButton::textColourOffId,
                           !showGraphView ? juce::Colour(0xff1a120b)
                                          : juce::Colour(0xffd2b89a));

    btnViewGraph.setColour(juce::TextButton::buttonColourId,
                           showGraphView ? juce::Colour(0xffe0b27d)
                                         : juce::Colour(0xff3a2a1b));
    btnViewGraph.setColour(juce::TextButton::textColourOffId,
                           showGraphView ? juce::Colour(0xff1a120b)
                                         : juce::Colour(0xffd2b89a));

    const int ag = engine->getAutoGainMode();
    for (int i = 0; i < 3; ++i) {
      const bool on = (i == ag);
      autoGainButtons[i].setColour(juce::TextButton::buttonColourId,
                                   on ? juce::Colour(0xff1e2228)
                                      : juce::Colour(0xff343942));
      autoGainButtons[i].setColour(juce::TextButton::textColourOffId,
                                   on ? juce::Colour(0xfff59e0b)
                                      : juce::Colour(0xffadb5bd));
    }

    const bool ar = engine->getAutoRelease();
    btnAutoRelease.setColour(juce::TextButton::buttonColourId,
                             ar ? juce::Colour(0xff1e2228)
                                : juce::Colour(0xff343942));
    btnAutoRelease.setColour(juce::TextButton::textColourOffId,
                             ar ? juce::Colour(0xfff59e0b)
                                : juce::Colour(0xffadb5bd));

    const bool lim = engine->getLimiterEnabled();
    btnLimiterOn.setColour(juce::TextButton::buttonColourId,
                           lim ? juce::Colour(0xff1e2228)
                               : juce::Colour(0xff343942));
    btnLimiterOn.setColour(juce::TextButton::textColourOffId,
                           lim ? juce::Colour(0xfff59e0b)
                               : juce::Colour(0xffadb5bd));

    const int dm = engine->getDistortionMode();
    for (int i = 0; i < 4; ++i) {
      const bool on = (i == dm);
      distButtons[i].setColour(juce::TextButton::buttonColourId,
                               on ? juce::Colour(0xff1e2228)
                                  : juce::Colour(0xff343942));
      distButtons[i].setColour(juce::TextButton::textColourOffId,
                               on ? juce::Colour(0xfff59e0b)
                                  : juce::Colour(0xffadb5bd));
    }
  }

  nova::graviton::RackDynamicsEngine *engine = nullptr;
  MetallicCompressorKnobLNF knobLnf;
  std::vector<juce::Slider *> allKnobs;

  bool showGraphView = false;
  float inDbSmooth = -60.0f;
  float outDbSmooth = -60.0f;
  float grDbSmooth = 0.0f;
  float inPeakSmooth = -60.0f;

  juce::Rectangle<float> displayRect;
  juce::Rectangle<float> inputMeterRect;
  juce::Rectangle<float> outputMeterRect;
  juce::Rectangle<float> limiterLedRect;

  std::array<juce::TextButton, 7> modelButtons;
  juce::TextButton btnModeCompExp;
  juce::TextButton btnSidechainMode;
  juce::TextButton btnScHpf;
  juce::TextButton btnViewMeter;
  juce::TextButton btnViewGraph;

  juce::Label lblInReadout, lblOutReadout;
  juce::Label lblInputGain, lblOutputGain;
  juce::Label lblThreshold, lblRatio, lblMakeup, lblAutoGain;
  juce::Label lblKnee, lblAttack, lblRelease;
  juce::Label lblLimiter, lblLimThresh, lblDistortion, lblMix;

  juce::Slider knobInputGain, knobOutputGain;
  juce::Slider knobThreshold, knobRatio, knobMakeup;
  juce::Slider knobKnee, knobAttack, knobRelease;
  juce::Slider knobLimThresh, knobMix;

  std::array<juce::TextButton, 3> autoGainButtons;
  juce::TextButton btnAutoRelease;
  juce::TextButton btnLimiterOn;
  std::array<juce::TextButton, 4> distButtons;
};

} // namespace nova
