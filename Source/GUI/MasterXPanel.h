#pragma once

#include "Effects/GravitonMasterX.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <juce_gui_basics/juce_gui_basics.h>
#include <sstream>
#include <string>
#include <vector>

namespace nova {

// ==============================================================================
// 1. Hardware-Inspired Precision Rotary Knob for Master X HD
// Displays circular track, neon arc indicator, digital value in center, title below.
// ==============================================================================
class MasterXDial final : public juce::Component {
public:
  enum class DialColour { Cyan, Yellow, Green, White, Orange };

  MasterXDial(const juce::String &paramTitle, double minV, double maxV,
              double defaultV, const juce::String &unitSuffix,
              DialColour col = DialColour::Cyan, int numDecimals = 1,
              bool isLog = false)
      : title(paramTitle), minValue(minV), maxValue(maxV),
        defaultValue(defaultV), currentValue(defaultV), unit(unitSuffix),
        colour(col), decimals(numDecimals), logarithmic(isLog) {
    setRepaintsOnMouseActivity(true);
  }

  void setValue(double val,
                juce::NotificationType notify = juce::dontSendNotification) {
    const double clamped = std::clamp(val, minValue, maxValue);
    if (std::abs(clamped - currentValue) > 1e-5) {
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
  std::function<void()> onDragEnded;

  void setColour(DialColour col) {
    if (colour != col) {
      colour = col;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    const float titleH = 13.0f;
    auto titleArea = bounds.removeFromBottom(titleH);

    // Knob dial area
    const float dialSize =
        std::clamp(std::min(bounds.getWidth(), bounds.getHeight()) - 2.0f,
                   20.0f, 48.0f);
    const auto dialArea = bounds.withSizeKeepingCentre(dialSize, dialSize);
    const float cx = dialArea.getCentreX();
    const float cy = dialArea.getCentreY();
    const float radius = dialSize * 0.5f;

    // Dark well background
    g.setColour(juce::Colour(0xff121418));
    g.fillEllipse(dialArea);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawEllipse(dialArea, 1.0f);

    // Normalized value and arc angle
    float norm = 0.0f;
    if (logarithmic && minValue > 0.0) {
      norm = static_cast<float>(std::log(currentValue / minValue) /
                                std::log(maxValue / minValue));
    } else {
      norm = static_cast<float>((currentValue - minValue) / (maxValue - minValue));
    }
    norm = std::clamp(norm, 0.0f, 1.0f);

    const float startAngle = juce::MathConstants<float>::pi * 1.25f;
    const float endAngle = juce::MathConstants<float>::pi * 2.75f;
    const float currentAngle = startAngle + norm * (endAngle - startAngle);

    // Background passive arc track
    juce::Path bgArc;
    bgArc.addCentredArc(cx, cy, radius - 2.5f, radius - 2.5f, 0.0f, startAngle,
                        endAngle, true);
    g.setColour(juce::Colour(0xff22272e));
    g.strokePath(bgArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Active arc color
    juce::Colour arcCol = juce::Colour(0xff29b6f6); // Cyan
    if (colour == DialColour::Yellow)
      arcCol = juce::Colour(0xffffca28);
    else if (colour == DialColour::Green)
      arcCol = juce::Colour(0xff00e676);
    else if (colour == DialColour::Orange)
      arcCol = juce::Colour(0xffff9100);
    else if (colour == DialColour::White)
      arcCol = juce::Colour(0xffe6edf3);

    if (isMouseOverOrDragging())
      arcCol = arcCol.brighter(0.18f);

    juce::Path activeArc;
    activeArc.addCentredArc(cx, cy, radius - 2.5f, radius - 2.5f, 0.0f,
                            startAngle, currentAngle, true);
    g.setColour(arcCol);
    g.strokePath(activeArc,
                 juce::PathStrokeType(2.8f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Value text in center
    g.setColour(juce::Colour(0xfff0f6fc));
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    juce::String valStr;
    if (decimals == 0)
      valStr = juce::String(static_cast<int>(std::round(currentValue)));
    else
      valStr = juce::String(currentValue, decimals);

    auto textBounds = dialArea.reduced(4.0f);
    if (unit.isNotEmpty()) {
      auto valArea = textBounds.removeFromTop(textBounds.getHeight() * 0.58f);
      g.drawText(valStr, valArea, juce::Justification::centred, false);
      g.setFont(juce::Font(juce::FontOptions(7.5f)));
      g.setColour(juce::Colour(0xff8b949e));
      g.drawText(unit, textBounds, juce::Justification::centred, false);
    } else {
      g.drawText(valStr, textBounds, juce::Justification::centred, false);
    }

    // Title label below dial
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.setColour(isMouseOverOrDragging() ? juce::Colour(0xff58a6ff)
                                        : juce::Colour(0xff8b949e));
    g.drawText(title, titleArea, juce::Justification::centred, false);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    mouseStartVal = currentValue;
    dragStartY = e.position.y;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float deltaY = dragStartY - e.position.y;
    const double range = maxValue - minValue;
    const double step = range * (e.mods.isShiftDown() ? 0.001 : 0.005);
    setValue(mouseStartVal + deltaY * step, juce::sendNotification);
  }

  void mouseUp(const juce::MouseEvent &) override {
    if (onDragEnded)
      onDragEnded();
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {
    setValue(defaultValue, juce::sendNotification);
    if (onDragEnded)
      onDragEnded();
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    const double range = maxValue - minValue;
    const double step = range * 0.02 * wheel.deltaY;
    setValue(currentValue + step, juce::sendNotification);
    if (onDragEnded)
      onDragEnded();
  }

private:
  juce::String title;
  double minValue, maxValue, defaultValue, currentValue;
  juce::String unit;
  DialColour colour;
  int decimals;
  bool logarithmic;
  double mouseStartVal = 0.0;
  float dragStartY = 0.0f;
};

// ==============================================================================
// 2. Three-Band Mini Dynamics Section
// Renders the 3 mini transfer function cards (LOW, MID, HIGH) with Solo & Mute
// ==============================================================================
class MasterXMiniTransferSection final : public juce::Component {
public:
  explicit MasterXMiniTransferSection(nova::graviton::MasterXMultiband &dspRef)
      : masterX(dspRef) {
    setRepaintsOnMouseActivity(true);
  }

  std::function<void(int band)> onBandSelected;

  void setSelectedBand(int band) {
    if (currentBand != band) {
      currentBand = band;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float bandW = (w - 8.0f) / 3.0f;

    static const char *bandNames[] = {"LOW (BASS)", "MID (BODY)", "HIGH (AIR)"};
    static const juce::Colour bandColors[] = {
        juce::Colour(0xff29b6f6), // Cyan for Low
        juce::Colour(0xffab47bc), // Purple for Mid
        juce::Colour(0xffffb300)  // Amber for High
    };

    for (int i = 0; i < 3; ++i) {
      auto bandRect = juce::Rectangle<float>(bounds.getX() + i * (bandW + 4.0f),
                                             bounds.getY(), bandW, h);

      const bool isSelected = (currentBand == i || currentBand == 3);

      // Card Background
      g.setColour(isSelected ? juce::Colour(0xff181c22) : juce::Colour(0xff121417));
      g.fillRoundedRectangle(bandRect, 4.0f);
      g.setColour(isSelected ? bandColors[i].withAlpha(0.8f) : juce::Colour(0xff262c36));
      g.drawRoundedRectangle(bandRect, 4.0f, isSelected ? 1.5f : 0.8f);

      // Header row: [BAND NAME] [SOLO] [MUTE]
      auto headerArea = bandRect.removeFromTop(18.0f).reduced(2.0f, 1.0f);

      const bool isSolo = masterX.getBandSolo(static_cast<size_t>(i));
      const bool isMuted = masterX.getBandMute(static_cast<size_t>(i));

      auto muteRect = headerArea.removeFromRight(26.0f);
      auto soloRect = headerArea.removeFromRight(26.0f);

      // Band Name Label
      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
      g.setColour(bandColors[i]);
      g.drawText(bandNames[i], headerArea, juce::Justification::centredLeft, false);

      // Solo button
      g.setColour(isSolo ? juce::Colour(0xffffca28) : juce::Colour(0xff21262d));
      g.fillRoundedRectangle(soloRect.reduced(1.0f), 2.0f);
      g.setColour(isSolo ? juce::Colour(0xff0d1117) : juce::Colour(0xff8b949e));
      g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
      g.drawText("SOLO", soloRect, juce::Justification::centred, false);

      // Mute button
      g.setColour(isMuted ? juce::Colour(0xffd73a49) : juce::Colour(0xff21262d));
      g.fillRoundedRectangle(muteRect.reduced(1.0f), 2.0f);
      g.setColour(isMuted ? juce::Colour(0xffffffff) : juce::Colour(0xff8b949e));
      g.drawText("MUTE", muteRect, juce::Justification::centred, false);

      // 2D Transfer Function Plot
      auto gridRect = bandRect.reduced(4.0f, 3.0f);
      g.setColour(juce::Colour(0xff0c0e12));
      g.fillRect(gridRect);

      // Grid lines (0, -12, -24, -36 dB)
      g.setColour(juce::Colour(0xff1a1e24));
      g.drawLine(gridRect.getX(), gridRect.getY() + gridRect.getHeight() * 0.33f,
                 gridRect.getRight(), gridRect.getY() + gridRect.getHeight() * 0.33f, 0.8f);
      g.drawLine(gridRect.getX(), gridRect.getY() + gridRect.getHeight() * 0.66f,
                 gridRect.getRight(), gridRect.getY() + gridRect.getHeight() * 0.66f, 0.8f);
      g.drawLine(gridRect.getX() + gridRect.getWidth() * 0.33f, gridRect.getY(),
                 gridRect.getX() + gridRect.getWidth() * 0.33f, gridRect.getBottom(), 0.8f);
      g.drawLine(gridRect.getX() + gridRect.getWidth() * 0.66f, gridRect.getY(),
                 gridRect.getX() + gridRect.getWidth() * 0.66f, gridRect.getBottom(), 0.8f);

      // Unity diagonal guide
      g.setColour(juce::Colour(0xff2d333b));
      g.drawLine(gridRect.getX(), gridRect.getBottom(), gridRect.getRight(),
                 gridRect.getY(), 0.8f);

      // Band Transfer Curve Calculation
      const double thresh = masterX.getBandThresholdDb(static_cast<size_t>(i));
      const double ratio = masterX.getBandRatio(static_cast<size_t>(i));
      const float threshNorm =
          std::clamp(static_cast<float>((thresh + 40.0) / 40.0), 0.0f, 1.0f);
      const float kneeX = gridRect.getX() + threshNorm * gridRect.getWidth();
      const float kneeY = gridRect.getBottom() - threshNorm * gridRect.getHeight();

      juce::Path transferCurve;
      auto dynMode = masterX.getDynamicsMode();

      if (dynMode == nova::graviton::DynamicsMode::Expander) {
        // Expander: downward expansion below knee
        const float slope = static_cast<float>(ratio);
        const float startY = std::clamp(kneeY + (kneeX - gridRect.getX()) * (slope - 1.0f),
                                        gridRect.getY(), gridRect.getBottom());
        transferCurve.startNewSubPath(gridRect.getX(), startY);
        transferCurve.lineTo(kneeX, kneeY);
        transferCurve.lineTo(gridRect.getRight(), gridRect.getY());
      } else {
        // Compressor or Limiter
        transferCurve.startNewSubPath(gridRect.getX(), gridRect.getBottom());
        transferCurve.lineTo(kneeX, kneeY);
        const float slope = (dynMode == nova::graviton::DynamicsMode::Limiter)
                                ? 0.05f
                                : (1.0f / static_cast<float>(ratio));
        const float dx = gridRect.getRight() - kneeX;
        const float endY = kneeY - dx * slope;
        transferCurve.lineTo(gridRect.getRight(), endY);
      }

      g.setColour(bandColors[i]);
      g.strokePath(transferCurve, juce::PathStrokeType(1.6f));

      // Dynamic Gain Reduction Dot
      const float grDb =
          masterX.getBandGainReductionDb(static_cast<size_t>(i));
      if (grDb > 0.2f) {
        const float grNorm = std::clamp(grDb / 20.0f, 0.0f, 1.0f);
        const float dotX = gridRect.getRight() - grNorm * gridRect.getWidth() * 0.4f;
        const float dotY = gridRect.getY() + grNorm * gridRect.getHeight() * 0.4f;
        g.setColour(juce::Colour(0xffff5252));
        g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
      }

      // GR dB Text indicator
      g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
      g.setColour(grDb > 0.5f ? juce::Colour(0xffff5252) : juce::Colour(0xff6e7681));
      juce::String grText = "-" + juce::String(grDb, 1) + " dB";
      g.drawText(grText,
                 juce::Rectangle<float>(gridRect.getX() + 2.0f,
                                        gridRect.getY() + 2.0f, 40.0f, 10.0f),
                 juce::Justification::topLeft, false);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const float w = static_cast<float>(getWidth());
    const float bandW = (w - 8.0f) / 3.0f;

    for (int i = 0; i < 3; ++i) {
      auto bandRect = juce::Rectangle<float>(i * (bandW + 4.0f), 0.0f, bandW,
                                             static_cast<float>(getHeight()));
      if (bandRect.contains(e.position)) {
        auto headerArea = bandRect.removeFromTop(18.0f).reduced(2.0f, 1.0f);
        auto muteRect = headerArea.removeFromRight(26.0f);
        auto soloRect = headerArea.removeFromRight(26.0f);

        if (soloRect.contains(e.position)) {
          masterX.setBandSolo(static_cast<size_t>(i),
                              !masterX.getBandSolo(static_cast<size_t>(i)));
          repaint();
          return;
        }
        if (muteRect.contains(e.position)) {
          masterX.setBandMute(static_cast<size_t>(i),
                              !masterX.getBandMute(static_cast<size_t>(i)));
          repaint();
          return;
        }

        // Click on the card selects the active band for editing
        currentBand = i;
        if (onBandSelected)
          onBandSelected(currentBand);
        repaint();
        return;
      }
    }
  }

private:
  nova::graviton::MasterXMultiband &masterX;
  int currentBand = 0;
};

// ==============================================================================
// 3. Interactive Master Frequency Response & Crossover Graph
// Log 20Hz-20kHz, -12 to +12dB, draggable crossover dividers and live spectrum
// ==============================================================================
class MasterXFrequencyGraph final : public juce::Component {
public:
  explicit MasterXFrequencyGraph(nova::graviton::MasterXMultiband &dspRef)
      : masterX(dspRef) {
    setRepaintsOnMouseActivity(true);
  }

  std::function<void()> onCrossoverChanged;

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    const float leftMargin = 32.0f;
    const float rightMargin = 32.0f;
    const float bottomMargin = 16.0f;

    auto plotArea = bounds;
    plotArea.removeFromLeft(leftMargin);
    plotArea.removeFromRight(rightMargin);
    plotArea.removeFromBottom(bottomMargin);

    // Dark slate graph background
    g.setColour(juce::Colour(0xff0d1014));
    g.fillRoundedRectangle(plotArea, 3.0f);
    g.setColour(juce::Colour(0xff21262d));
    g.drawRoundedRectangle(plotArea, 3.0f, 1.0f);

    auto freqToX = [plotArea](double freq) -> float {
      const double minF = 20.0;
      const double maxF = 20000.0;
      const double norm = std::log10(freq / minF) / std::log10(maxF / minF);
      return plotArea.getX() + static_cast<float>(norm) * plotArea.getWidth();
    };

    auto dbToY = [plotArea](double db) -> float {
      // Range: +12 dB to -12 dB
      const double norm = (12.0 - db) / 24.0;
      return plotArea.getY() + static_cast<float>(norm) * plotArea.getHeight();
    };

    // Frequency grid lines & labels
    static const double gridFreqs[] = {20.0,  50.0,  100.0,  200.0, 500.0,
                                       1000.0, 2000.0, 5000.0, 10000.0, 20000.0};
    static const char *freqLabels[] = {"20",  "50",  "100", "200", "500",
                                       "1k",  "2k",  "5k",  "10k", "20k"};

    g.setFont(juce::Font(juce::FontOptions(7.5f)));
    for (size_t i = 0; i < 10; ++i) {
      const float x = freqToX(gridFreqs[i]);
      g.setColour(juce::Colour(0xff181c22));
      g.drawVerticalLine(static_cast<int>(x), plotArea.getY(), plotArea.getBottom());

      g.setColour(juce::Colour(0xff6e7681));
      g.drawText(freqLabels[i],
                 juce::Rectangle<float>(x - 12.0f, bounds.getBottom() - 14.0f,
                                        24.0f, 12.0f),
                 juce::Justification::centred, false);
    }

    // Horizontal dB grid lines
    static const double gridDbs[] = {12.0, 6.0, 0.0, -6.0, -12.0};
    for (double db : gridDbs) {
      const float y = dbToY(db);
      g.setColour(db == 0.0 ? juce::Colour(0xff30363d) : juce::Colour(0xff181c22));
      g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());

      // Left axis label
      g.setColour(juce::Colour(0xff8b949e));
      juce::String dbLabel = (db > 0.0 ? "+" : "") + juce::String(static_cast<int>(db));
      g.drawText(dbLabel,
                 juce::Rectangle<float>(2.0f, y - 5.0f, leftMargin - 4.0f, 10.0f),
                 juce::Justification::centredRight, false);
    }

    // Draggable Crossover Frequency Vertical Divider Lines
    const float xover1X = freqToX(masterX.getCrossoverLowMidHz());
    const float xover2X = freqToX(masterX.getCrossoverMidHighHz());

    // Shaded frequency zones: Low, Mid, High
    g.setColour(juce::Colour(0x1229b6f6));
    g.fillRect(plotArea.getX(), plotArea.getY(), xover1X - plotArea.getX(),
               plotArea.getHeight());
    g.setColour(juce::Colour(0x12ab47bc));
    g.fillRect(xover1X, plotArea.getY(), xover2X - xover1X, plotArea.getHeight());
    g.setColour(juce::Colour(0x12ffb300));
    g.fillRect(xover2X, plotArea.getY(), plotArea.getRight() - xover2X,
               plotArea.getHeight());

    // Crossover 1 Line (Low/Mid)
    float dashPattern[] = {4.0f, 3.0f};
    g.setColour(draggingXover == 1 ? juce::Colour(0xff58a6ff) : juce::Colour(0xff29b6f6));
    g.drawDashedLine(juce::Line<float>(xover1X, plotArea.getY(), xover1X,
                                       plotArea.getBottom()),
                     dashPattern, 2, 1.4f);

    // Crossover 2 Line (Mid/High)
    g.setColour(draggingXover == 2 ? juce::Colour(0xff58a6ff) : juce::Colour(0xffffb300));
    g.drawDashedLine(juce::Line<float>(xover2X, plotArea.getY(), xover2X,
                                       plotArea.getBottom()),
                     dashPattern, 2, 1.4f);

    // Crossover Frequency Badges
    auto drawXoverBadge = [&](float x, double freq, juce::Colour col) {
      juce::String fStr;
      if (freq >= 1000.0)
        fStr = juce::String(freq * 0.001, 1) + "k";
      else
        fStr = juce::String(static_cast<int>(std::round(freq))) + "Hz";

      auto badgeRect = juce::Rectangle<float>(x - 18.0f, plotArea.getY() + 3.0f, 36.0f, 13.0f);
      g.setColour(juce::Colour(0xff161b22));
      g.fillRoundedRectangle(badgeRect, 2.0f);
      g.setColour(col);
      g.drawRoundedRectangle(badgeRect, 2.0f, 0.8f);
      g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
      g.drawText(fStr, badgeRect, juce::Justification::centred, false);
    };

    drawXoverBadge(xover1X, masterX.getCrossoverLowMidHz(), juce::Colour(0xff29b6f6));
    drawXoverBadge(xover2X, masterX.getCrossoverMidHighHz(), juce::Colour(0xffffb300));

    // Dynamic Multiband Compression Response Curve
    const double lowGain = masterX.getBandMakeupGainDb(0) - masterX.getBandGainReductionDb(0);
    const double midGain = masterX.getBandMakeupGainDb(1) - masterX.getBandGainReductionDb(1);
    const double highGain = masterX.getBandMakeupGainDb(2) - masterX.getBandGainReductionDb(2);

    juce::Path compPath;
    const float pt1X = plotArea.getX();
    const float pt1Y = dbToY(lowGain);
    const float pt2X = freqToX(masterX.getCrossoverLowMidHz() * 0.5);
    const float pt2Y = dbToY(lowGain);
    const float pt3X = freqToX(std::sqrt(masterX.getCrossoverLowMidHz() * masterX.getCrossoverMidHighHz()));
    const float pt3Y = dbToY(midGain);
    const float pt4X = freqToX(std::sqrt(masterX.getCrossoverMidHighHz() * 20000.0));
    const float pt4Y = dbToY(highGain);
    const float pt5X = plotArea.getRight();
    const float pt5Y = dbToY(highGain);

    compPath.startNewSubPath(pt1X, pt1Y);
    compPath.cubicTo(pt2X, pt2Y, (pt2X + pt3X) * 0.5f, pt3Y, pt3X, pt3Y);
    compPath.cubicTo((pt3X + pt4X) * 0.5f, pt3Y, pt4X, pt4Y, pt5X, pt5Y);

    // Gradient fill under curve
    juce::Path fillPath = compPath;
    fillPath.lineTo(plotArea.getRight(), dbToY(0.0));
    fillPath.lineTo(plotArea.getX(), dbToY(0.0));
    fillPath.closeSubPath();

    juce::ColourGradient grad(juce::Colour(0x3029b6f6), 0.0f, plotArea.getY(),
                              juce::Colour(0x0529b6f6), 0.0f, dbToY(0.0), false);
    g.setGradientFill(grad);
    g.fillPath(fillPath);

    g.setColour(juce::Colour(0xff29b6f6));
    g.strokePath(compPath, juce::PathStrokeType(1.8f));

    // Yellow Limiter Ceiling Line at top
    const double limCeiling = masterX.getLimiterCeilingDb();
    const float limY = dbToY(limCeiling);
    g.setColour(juce::Colour(0xffffca28));
    g.drawLine(plotArea.getX(), limY, plotArea.getRight(), limY, 1.2f);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    auto plotArea = getPlotArea();
    if (!plotArea.contains(e.position))
      return;

    auto freqToX = [plotArea](double freq) -> float {
      const double norm = std::log10(freq / 20.0) / std::log10(20000.0 / 20.0);
      return plotArea.getX() + static_cast<float>(norm) * plotArea.getWidth();
    };

    const float x1 = freqToX(masterX.getCrossoverLowMidHz());
    const float x2 = freqToX(masterX.getCrossoverMidHighHz());

    if (std::abs(e.position.x - x1) < 14.0f) {
      draggingXover = 1;
    } else if (std::abs(e.position.x - x2) < 14.0f) {
      draggingXover = 2;
    } else {
      draggingXover = 0;
    }
    repaint();
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (draggingXover == 0)
      return;

    auto plotArea = getPlotArea();
    const float norm = std::clamp(
        (e.position.x - plotArea.getX()) / plotArea.getWidth(), 0.01f, 0.99f);
    const double freq = 20.0 * std::pow(20000.0 / 20.0, norm);

    if (draggingXover == 1) {
      masterX.setCrossoverLowMidHz(std::clamp(
          freq, 40.0, std::min(1000.0, masterX.getCrossoverMidHighHz() - 200.0)));
    } else if (draggingXover == 2) {
      masterX.setCrossoverMidHighHz(std::clamp(
          freq, std::max(1000.0, masterX.getCrossoverLowMidHz() + 200.0), 16000.0));
    }

    repaint();
    if (onCrossoverChanged)
      onCrossoverChanged();
  }

  void mouseUp(const juce::MouseEvent &) override {
    draggingXover = 0;
    repaint();
  }

private:
  juce::Rectangle<float> getPlotArea() const {
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromLeft(32.0f);
    bounds.removeFromRight(32.0f);
    bounds.removeFromBottom(16.0f);
    return bounds;
  }

  nova::graviton::MasterXMultiband &masterX;
  int draggingXover = 0; // 0 = none, 1 = low/mid, 2 = mid/high
};

// ==============================================================================
// 4. Multiband Precision GR & Peak Metering Tower
// ==============================================================================
class MasterXMeterTower final : public juce::Component {
public:
  explicit MasterXMeterTower(nova::graviton::MasterXMultiband &dspRef)
      : masterX(dspRef) {}

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float barW = (bounds.getWidth() - 16.0f) / 5.0f;

    // Meters: [LOW GR] [MID GR] [HIGH GR] [LIM GR] [OUT PK]
    auto drawGrMeter = [&](float x, float grDb, const juce::String &lbl,
                           juce::Colour ledCol) {
      auto mRect = juce::Rectangle<float>(x, bounds.getY() + 14.0f, barW,
                                          bounds.getHeight() - 18.0f);
      g.setColour(juce::Colour(0xff121417));
      g.fillRect(mRect);
      g.setColour(juce::Colour(0xff22272e));
      g.drawRect(mRect, 0.8f);

      // Label at top
      g.setFont(juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));
      g.setColour(juce::Colour(0xff8b949e));
      g.drawText(lbl,
                 juce::Rectangle<float>(x - 4.0f, bounds.getY(), barW + 8.0f, 12.0f),
                 juce::Justification::centred, false);

      // Active GR Bar (moves from top down)
      const float norm = std::clamp(grDb / 20.0f, 0.0f, 1.0f);
      if (norm > 0.01f) {
        auto barRect = juce::Rectangle<float>(mRect.getX() + 1.0f, mRect.getY() + 1.0f,
                                              mRect.getWidth() - 2.0f,
                                              (mRect.getHeight() - 2.0f) * norm);
        g.setColour(ledCol);
        g.fillRect(barRect);
      }
    };

    const float startX = bounds.getX();
    drawGrMeter(startX, masterX.getBandGainReductionDb(0), "L-GR",
                juce::Colour(0xff29b6f6));
    drawGrMeter(startX + barW + 3.0f, masterX.getBandGainReductionDb(1), "M-GR",
                juce::Colour(0xffab47bc));
    drawGrMeter(startX + (barW + 3.0f) * 2.0f, masterX.getBandGainReductionDb(2),
                "H-GR", juce::Colour(0xffffb300));
    drawGrMeter(startX + (barW + 3.0f) * 3.0f, masterX.getLimiterGainReductionDb(),
                "LIM", juce::Colour(0xffff5252));

    // Stereo Out Peak Meter
    const float outL = masterX.getMeterOutL();
    const float outR = masterX.getMeterOutR();
    const float outPeak = std::max(outL, outR);
    const float outDb = (outPeak > 1e-5f)
                            ? (20.0f * std::log10(outPeak))
                            : -80.0f;
    const float outNorm = std::clamp((outDb + 60.0f) / 60.0f, 0.0f, 1.0f);

    auto outRect = juce::Rectangle<float>(startX + (barW + 3.0f) * 4.0f,
                                          bounds.getY() + 14.0f, barW,
                                          bounds.getHeight() - 18.0f);
    g.setColour(juce::Colour(0xff121417));
    g.fillRect(outRect);
    g.setColour(juce::Colour(0xff22272e));
    g.drawRect(outRect, 0.8f);

    g.setFont(juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff8b949e));
    g.drawText("OUT",
               juce::Rectangle<float>(outRect.getX() - 2.0f, bounds.getY(),
                                      barW + 4.0f, 12.0f),
               juce::Justification::centred, false);

    if (outNorm > 0.01f) {
      const float hFill = (outRect.getHeight() - 2.0f) * outNorm;
      auto fillRect = juce::Rectangle<float>(outRect.getX() + 1.0f,
                                             outRect.getBottom() - 1.0f - hFill,
                                             outRect.getWidth() - 2.0f, hFill);
      juce::Colour outCol = (outDb > -0.2f) ? juce::Colour(0xffff5252)
                            : (outDb > -6.0f) ? juce::Colour(0xffffca28)
                                             : juce::Colour(0xff00e676);
      g.setColour(outCol);
      g.fillRect(fillRect);
    }
  }

private:
  nova::graviton::MasterXMultiband &masterX;
};

// ==============================================================================
// 5. Main Master X HD Dialog Panel
// Complete 1:1 TC Electronic System 6000 Multiband Architecture
// ==============================================================================
class MasterXPanel final : public juce::Component, private juce::Timer {
public:
  explicit MasterXPanel(nova::graviton::MasterXMultiband &dspRef)
      : masterX(dspRef), miniTransfer(dspRef), mainGraph(dspRef),
        meterTower(dspRef),
        dialFactor("FACTOR", 0.0, 100.0, 20.0, "%", MasterXDial::DialColour::White, 0),
        dialThreshold("THRESH", -40.0, 0.0, -20.0, "dB", MasterXDial::DialColour::Cyan, 1),
        dialRatio("RATIO", 1.0, 20.0, 1.80, ":1", MasterXDial::DialColour::Cyan, 2),
        dialAttack("ATTACK", 0.1, 100.0, 35.0, "ms", MasterXDial::DialColour::Cyan, 1, true),
        dialRelease("RELEASE", 10.0, 2000.0, 150.0, "ms", MasterXDial::DialColour::Cyan, 0, true),
        dialBandGain("MAKEUP", -12.0, 18.0, 0.0, "dB", MasterXDial::DialColour::Cyan, 1),
        dialSoftClip("SOFT CLIP", -18.0, 0.0, -9.0, "dB", MasterXDial::DialColour::Orange, 1),
        dialMix("MIX", 0.0, 100.0, 100.0, "%", MasterXDial::DialColour::White, 0),
        dialInGain("IN GAIN", -18.0, 18.0, 0.0, "dB", MasterXDial::DialColour::White, 1),
        dialOutGain("OUT GAIN", -18.0, 18.0, 0.0, "dB", MasterXDial::DialColour::White, 1) {

    setSize(1020, 520);

    // Preset & Toolbar Buttons
    presetBtn.setButtonText(masterX.getFactoryPresets()[0].name + juce::String(" \xe2\x96\xbe"));
    presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff16191d));
    presetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    presetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetBtn);

    prevPresetBtn.setButtonText("<");
    prevPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    prevPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    prevPresetBtn.onClick = [this] {
      const auto &factory = masterX.getFactoryPresets();
      const int numPresets = static_cast<int>(factory.size());
      currentPresetIdx = (currentPresetIdx - 1 + numPresets) % numPresets;
      applyPreset(currentPresetIdx);
    };
    addAndMakeVisible(prevPresetBtn);

    nextPresetBtn.setButtonText(">");
    nextPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    nextPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    nextPresetBtn.onClick = [this] {
      const auto &factory = masterX.getFactoryPresets();
      const int numPresets = static_cast<int>(factory.size());
      currentPresetIdx = (currentPresetIdx + 1) % numPresets;
      applyPreset(currentPresetIdx);
    };
    addAndMakeVisible(nextPresetBtn);

    compareBtn.setButtonText("Compare");
    compareBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    compareBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    compareBtn.onClick = [this] {
      if (!compareActive) {
        stateA = masterX.getCurrentPresetState("State A (User)");
        compareActive = true;
        compareBtn.setButtonText("Comparing (B)");
        compareBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f6feb));
        masterX.loadPreset(stateB);
      } else {
        compareActive = false;
        compareBtn.setButtonText("Compare (A)");
        compareBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
        masterX.loadPreset(stateA);
      }
      syncControlsFromDSP();
    };
    addAndMakeVisible(compareBtn);

    copyBtn.setButtonText("Copy");
    copyBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    copyBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    copyBtn.onClick = [this] {
      clipboardPreset = masterX.getCurrentPresetState("Clipboard Preset");
      hasClipboard = true;
      copyBtn.setButtonText("Copied!");
      juce::Timer::callAfterDelay(1200, [this] { copyBtn.setButtonText("Copy"); });
    };
    addAndMakeVisible(copyBtn);

    pasteBtn.setButtonText("Paste");
    pasteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    pasteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    pasteBtn.onClick = [this] {
      if (hasClipboard) {
        pushUndo();
        masterX.loadPreset(clipboardPreset);
        syncControlsFromDSP();
        pasteBtn.setButtonText("Pasted!");
        juce::Timer::callAfterDelay(1200, [this] { pasteBtn.setButtonText("Paste"); });
      }
    };
    addAndMakeVisible(pasteBtn);

    undoBtn.setButtonText("Undo");
    undoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    undoBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    undoBtn.onClick = [this] {
      if (undoPos > 0) {
        --undoPos;
        masterX.loadPreset(undoHistory[static_cast<size_t>(undoPos)]);
        syncControlsFromDSP();
      }
    };
    addAndMakeVisible(undoBtn);

    redoBtn.setButtonText("Redo");
    redoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    redoBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8b949e));
    redoBtn.onClick = [this] {
      if (undoPos + 1 < static_cast<int>(undoHistory.size())) {
        ++undoPos;
        masterX.loadPreset(undoHistory[static_cast<size_t>(undoPos)]);
        syncControlsFromDSP();
      }
    };
    addAndMakeVisible(redoBtn);

    powerBtn.setButtonText("\xe2\x8f\xbb");
    powerBtn.setColour(juce::TextButton::buttonColourId,
                       masterX.getLimiterEnabled() ? juce::Colour(0xff1f6feb)
                                                  : juce::Colour(0xff21262d));
    powerBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    powerBtn.onClick = [this] {
      const bool pwr = !masterX.getLimiterEnabled();
      masterX.setLimiterEnabled(pwr);
      powerBtn.setColour(juce::TextButton::buttonColourId,
                         pwr ? juce::Colour(0xff1f6feb) : juce::Colour(0xff21262d));
      repaint();
    };
    addAndMakeVisible(powerBtn);

    // Initial state setup for Compare and Undo
    stateB = masterX.getFactoryPresets()[0];
    stateA = stateB;
    pushUndo();

    // Mode Buttons: EXP, COM, LIM
    setupButton(btnModeExp, "EXP", [this] {
      masterX.setDynamicsMode(nova::graviton::DynamicsMode::Expander);
      updateModeButtons();
      pushUndo();
    });
    setupButton(btnModeCom, "COM", [this] {
      masterX.setDynamicsMode(nova::graviton::DynamicsMode::Compressor);
      updateModeButtons();
      pushUndo();
    });
    setupButton(btnModeLim, "LIM", [this] {
      masterX.setDynamicsMode(nova::graviton::DynamicsMode::Limiter);
      updateModeButtons();
      pushUndo();
    });
    updateModeButtons();

    // Target Curve Buttons: FLAT, BASS, AIR, SMILEY
    setupButton(btnCurveFlat, "FLAT", [this] {
      masterX.setTargetCurve(nova::graviton::TargetCurve::Flat);
      updateCurveButtons();
      syncBandControls();
      pushUndo();
    });
    setupButton(btnCurveBass, "BASS", [this] {
      masterX.setTargetCurve(nova::graviton::TargetCurve::Bass);
      updateCurveButtons();
      syncBandControls();
      pushUndo();
    });
    setupButton(btnCurveAir, "AIR", [this] {
      masterX.setTargetCurve(nova::graviton::TargetCurve::Air);
      updateCurveButtons();
      syncBandControls();
      pushUndo();
    });
    setupButton(btnCurveSmiley, "SMILEY", [this] {
      masterX.setTargetCurve(nova::graviton::TargetCurve::Smiley);
      updateCurveButtons();
      syncBandControls();
      pushUndo();
    });
    updateCurveButtons();

    // Band Selector Tabs: LOW, MID, HIGH, ALL
    setupButton(btnBandLow, "LOW BAND", [this] { selectBand(0); });
    setupButton(btnBandMid, "MID BAND", [this] { selectBand(1); });
    setupButton(btnBandHigh, "HIGH BAND", [this] { selectBand(2); });
    setupButton(btnBandAll, "ALL (LINK)", [this] { selectBand(3); });
    updateBandButtons();

    // Gain Link Button
    setupButton(btnGainLink, "LINK", [this] {
      masterX.setGainLinked(!masterX.getGainLinked());
      updateLinkButton();
      pushUndo();
    });
    updateLinkButton();

    // Add and wire visual modules
    addAndMakeVisible(miniTransfer);
    miniTransfer.onBandSelected = [this](int b) { selectBand(b); };

    addAndMakeVisible(mainGraph);
    mainGraph.onCrossoverChanged = [this] {
      miniTransfer.repaint();
      pushUndo();
    };

    addAndMakeVisible(meterTower);

    // Setup knobs
    dialFactor.setValue(masterX.getFactorPct());
    dialFactor.onValueChanged = [this](double val) { masterX.setFactorPct(val); };
    dialFactor.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialFactor);

    dialThreshold.onValueChanged = [this](double val) {
      if (selectedBand == 3) {
        masterX.setBandThresholdDb(0, val);
        masterX.setBandThresholdDb(1, val);
        masterX.setBandThresholdDb(2, val);
      } else {
        masterX.setBandThresholdDb(static_cast<size_t>(selectedBand), val);
      }
      miniTransfer.repaint();
      mainGraph.repaint();
    };
    dialThreshold.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialThreshold);

    dialRatio.onValueChanged = [this](double val) {
      if (selectedBand == 3) {
        masterX.setBandRatio(0, val);
        masterX.setBandRatio(1, val);
        masterX.setBandRatio(2, val);
      } else {
        masterX.setBandRatio(static_cast<size_t>(selectedBand), val);
      }
      miniTransfer.repaint();
      mainGraph.repaint();
    };
    dialRatio.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialRatio);

    dialAttack.onValueChanged = [this](double val) {
      if (selectedBand == 3) {
        masterX.setBandAttackMs(0, val);
        masterX.setBandAttackMs(1, val);
        masterX.setBandAttackMs(2, val);
      } else {
        masterX.setBandAttackMs(static_cast<size_t>(selectedBand), val);
      }
    };
    dialAttack.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialAttack);

    dialRelease.onValueChanged = [this](double val) {
      if (selectedBand == 3) {
        masterX.setBandReleaseMs(0, val);
        masterX.setBandReleaseMs(1, val);
        masterX.setBandReleaseMs(2, val);
      } else {
        masterX.setBandReleaseMs(static_cast<size_t>(selectedBand), val);
      }
    };
    dialRelease.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialRelease);

    dialBandGain.onValueChanged = [this](double val) {
      if (selectedBand == 3) {
        masterX.setBandMakeupGainDb(0, val);
        masterX.setBandMakeupGainDb(1, val);
        masterX.setBandMakeupGainDb(2, val);
      } else {
        masterX.setBandMakeupGainDb(static_cast<size_t>(selectedBand), val);
      }
      mainGraph.repaint();
    };
    dialBandGain.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialBandGain);

    dialSoftClip.setValue(masterX.getSoftClipDb());
    dialSoftClip.onValueChanged = [this](double val) { masterX.setSoftClipDb(val); };
    dialSoftClip.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialSoftClip);

    dialMix.setValue(masterX.getMix() * 100.0);
    dialMix.onValueChanged = [this](double val) { masterX.setMix(val * 0.01); };
    dialMix.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialMix);

    dialInGain.setValue(masterX.getInGainDb());
    dialInGain.onValueChanged = [this](double val) {
      masterX.setInGainDb(val);
      if (masterX.getGainLinked()) {
        dialOutGain.setValue(masterX.getOutGainDb(), juce::dontSendNotification);
      }
    };
    dialInGain.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialInGain);

    dialOutGain.setValue(masterX.getOutGainDb());
    dialOutGain.onValueChanged = [this](double val) {
      masterX.setOutGainDb(val);
      if (masterX.getGainLinked()) {
        dialInGain.setValue(masterX.getInGainDb(), juce::dontSendNotification);
      }
    };
    dialOutGain.onDragEnded = [this] { pushUndo(); };
    addAndMakeVisible(dialOutGain);

    // Initial band sync
    syncBandControls();

    // Start 30 fps UI timer for peak meters and dynamic curves
    startTimerHz(30);
  }

  ~MasterXPanel() override { stopTimer(); }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // 1. Dark professional chassis background
    g.setColour(juce::Colour(0xff121417));
    g.fillAll();

    // Top toolbar background
    const float toolbarH = (getHeight() < 380) ? 30.0f : 40.0f;
    auto toolbar = bounds.removeFromTop(toolbarH);
    g.setColour(juce::Colour(0xff181c22));
    g.fillRect(toolbar);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawHorizontalLine(static_cast<int>(toolbar.getBottom()), toolbar.getX(),
                         toolbar.getRight());

    // Title on right
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffe6edf3));
    g.drawText("MASTER X HD  \xc2\xb7  tc electronic",
               static_cast<int>(toolbar.getRight() - 210.0f), 0, 200,
               static_cast<int>(toolbarH), juce::Justification::centredRight, false);

    // Lower Knobs Strip Area
    const float bottomY = getHeight() - 78.0f;
    auto knobStrip = juce::Rectangle<float>(6.0f, bottomY, getWidth() - 12.0f, 74.0f);
    g.setColour(juce::Colour(0xff16191f));
    g.fillRoundedRectangle(knobStrip, 4.0f);
    g.setColour(juce::Colour(0xff262c36));
    g.drawRoundedRectangle(knobStrip, 4.0f, 1.0f);
  }

  void resized() override {
    const int w = getWidth();
    const int h = getHeight();

    const int toolbarH = (h < 380) ? 30 : 40;

    // Row 1: Toolbar Buttons
    presetBtn.setBounds(8, 4, 175, 22);
    prevPresetBtn.setBounds(187, 4, 22, 22);
    nextPresetBtn.setBounds(211, 4, 22, 22);
    compareBtn.setBounds(237, 4, 80, 22);
    copyBtn.setBounds(321, 4, 40, 22);
    pasteBtn.setBounds(365, 4, 40, 22);
    undoBtn.setBounds(409, 4, 40, 22);
    redoBtn.setBounds(453, 4, 40, 22);
    powerBtn.setBounds(497, 4, 24, 22);

    // Mode Radio Buttons: EXP, COM, LIM
    const int modeStartX = 530;
    btnModeExp.setBounds(modeStartX, 4, 38, 22);
    btnModeCom.setBounds(modeStartX + 40, 4, 38, 22);
    btnModeLim.setBounds(modeStartX + 80, 4, 38, 22);

    // Target Curve Buttons: FLAT, BASS, AIR, SMILEY
    const int curveStartX = modeStartX + 126;
    btnCurveFlat.setBounds(curveStartX, 4, 42, 22);
    btnCurveBass.setBounds(curveStartX + 44, 4, 42, 22);
    btnCurveAir.setBounds(curveStartX + 88, 4, 38, 22);
    btnCurveSmiley.setBounds(curveStartX + 128, 4, 52, 22);

    // Band Tabs: LOW, MID, HIGH, ALL
    const int bandStartX = curveStartX + 188;
    btnBandLow.setBounds(bandStartX, 4, 68, 22);
    btnBandMid.setBounds(bandStartX + 70, 4, 68, 22);
    btnBandHigh.setBounds(bandStartX + 140, 4, 72, 22);
    btnBandAll.setBounds(bandStartX + 214, 4, 76, 22);

    btnGainLink.setBounds(bandStartX + 294, 4, 44, 22);

    // Middle Area: Monitoring Bay
    const int topY = toolbarH + 4;
    const int bottomY = h - 76;
    const int midH = bottomY - topY - 4;

    const int miniW = 270;
    const int meterW = 125;
    const int graphW = w - 16 - miniW - meterW - 8;

    miniTransfer.setBounds(8, topY, miniW, midH);
    mainGraph.setBounds(8 + miniW + 4, topY, graphW, midH);
    meterTower.setBounds(8 + miniW + 4 + graphW + 4, topY, meterW, midH);

    // Bottom Area: 10 Precision Dials
    const int knobY = h - 72;
    const int knobSize = 52;
    const int count = 10;
    const int totalKnobsW = count * knobSize;
    const int spacing = (w - 24 - totalKnobsW) / (count - 1);
    const int startX = 12;

    dialFactor.setBounds(startX, knobY, knobSize, knobSize);
    dialThreshold.setBounds(startX + (knobSize + spacing) * 1, knobY, knobSize, knobSize);
    dialRatio.setBounds(startX + (knobSize + spacing) * 2, knobY, knobSize, knobSize);
    dialAttack.setBounds(startX + (knobSize + spacing) * 3, knobY, knobSize, knobSize);
    dialRelease.setBounds(startX + (knobSize + spacing) * 4, knobY, knobSize, knobSize);
    dialBandGain.setBounds(startX + (knobSize + spacing) * 5, knobY, knobSize, knobSize);
    dialSoftClip.setBounds(startX + (knobSize + spacing) * 6, knobY, knobSize, knobSize);
    dialMix.setBounds(startX + (knobSize + spacing) * 7, knobY, knobSize, knobSize);
    dialInGain.setBounds(startX + (knobSize + spacing) * 8, knobY, knobSize, knobSize);
    dialOutGain.setBounds(startX + (knobSize + spacing) * 9, knobY, knobSize, knobSize);
  }

  void selectBand(int b) {
    selectedBand = std::clamp(b, 0, 3);
    miniTransfer.setSelectedBand(selectedBand);
    updateBandButtons();
    syncBandControls();
  }

  void showPresetMenu() {
    juce::PopupMenu m;
    const auto &presets = masterX.getFactoryPresets();

    for (size_t i = 0; i < presets.size(); ++i) {
      m.addItem(static_cast<int>(i + 1), presets[i].name, true,
                static_cast<int>(i) == currentPresetIdx);
    }
    m.addSeparator();
    m.addItem(100, "Reset to Default", true, false);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
                    [this, presets](int result) {
                      if (result >= 1 && result <= static_cast<int>(presets.size())) {
                        applyPreset(result - 1);
                      } else if (result == 100) {
                        applyPreset(0);
                      }
                    });
  }

  void applyPreset(int index) {
    const auto &presets = masterX.getFactoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
      pushUndo();
      currentPresetIdx = index;
      stateB = presets[static_cast<size_t>(index)];
      masterX.loadPreset(stateB);
      presetBtn.setButtonText(juce::String(stateB.name) + " \xe2\x96\xbe");
      syncControlsFromDSP();
    }
  }

  void pushUndo() {
    auto cur = masterX.getCurrentPresetState();
    if (undoPos >= 0 && undoPos < static_cast<int>(undoHistory.size())) {
      undoHistory.erase(undoHistory.begin() + undoPos + 1, undoHistory.end());
    }
    undoHistory.push_back(cur);
    undoPos = static_cast<int>(undoHistory.size()) - 1;
    if (undoHistory.size() > 50) {
      undoHistory.erase(undoHistory.begin());
      --undoPos;
    }
  }

  void syncControlsFromDSP() {
    dialFactor.setValue(masterX.getFactorPct(), juce::dontSendNotification);
    dialSoftClip.setValue(masterX.getSoftClipDb(), juce::dontSendNotification);
    dialMix.setValue(masterX.getMix() * 100.0, juce::dontSendNotification);
    dialInGain.setValue(masterX.getInGainDb(), juce::dontSendNotification);
    dialOutGain.setValue(masterX.getOutGainDb(), juce::dontSendNotification);
    powerBtn.setColour(juce::TextButton::buttonColourId,
                       masterX.getLimiterEnabled() ? juce::Colour(0xff1f6feb)
                                                  : juce::Colour(0xff21262d));

    updateModeButtons();
    updateCurveButtons();
    updateBandButtons();
    updateLinkButton();
    syncBandControls();

    miniTransfer.repaint();
    mainGraph.repaint();
    repaint();
  }

  void syncBandControls() {
    const size_t b = static_cast<size_t>(selectedBand == 3 ? 1 : selectedBand);
    dialThreshold.setValue(masterX.getBandThresholdDb(b), juce::dontSendNotification);
    dialRatio.setValue(masterX.getBandRatio(b), juce::dontSendNotification);
    dialAttack.setValue(masterX.getBandAttackMs(b), juce::dontSendNotification);
    dialRelease.setValue(masterX.getBandReleaseMs(b), juce::dontSendNotification);
    dialBandGain.setValue(masterX.getBandMakeupGainDb(b), juce::dontSendNotification);

    // Update dial colors matching active band
    MasterXDial::DialColour col = MasterXDial::DialColour::Cyan;
    if (selectedBand == 1)
      col = MasterXDial::DialColour::Yellow;
    else if (selectedBand == 2)
      col = MasterXDial::DialColour::Orange;
    else if (selectedBand == 3)
      col = MasterXDial::DialColour::White;

    dialThreshold.setColour(col);
    dialRatio.setColour(col);
    dialAttack.setColour(col);
    dialRelease.setColour(col);
    dialBandGain.setColour(col);
  }

private:
  void timerCallback() override {
    // Redraw dynamic graphs and meters at 30 fps
    miniTransfer.repaint();
    mainGraph.repaint();
    meterTower.repaint();
  }

  void setupButton(juce::TextButton &btn, const juce::String &txt,
                   std::function<void()> onClick) {
    btn.setButtonText(txt);
    btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
    btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d1d9));
    btn.onClick = onClick;
    addAndMakeVisible(btn);
  }

  void updateModeButtons() {
    const auto m = masterX.getDynamicsMode();
    btnModeExp.setColour(juce::TextButton::buttonColourId,
                         m == nova::graviton::DynamicsMode::Expander
                             ? juce::Colour(0xff00e676)
                             : juce::Colour(0xff21262d));
    btnModeExp.setColour(juce::TextButton::textColourOffId,
                         m == nova::graviton::DynamicsMode::Expander
                             ? juce::Colour(0xff0d1117)
                             : juce::Colour(0xffc9d1d9));

    btnModeCom.setColour(juce::TextButton::buttonColourId,
                         m == nova::graviton::DynamicsMode::Compressor
                             ? juce::Colour(0xff29b6f6)
                             : juce::Colour(0xff21262d));
    btnModeCom.setColour(juce::TextButton::textColourOffId,
                         m == nova::graviton::DynamicsMode::Compressor
                             ? juce::Colour(0xff0d1117)
                             : juce::Colour(0xffc9d1d9));

    btnModeLim.setColour(juce::TextButton::buttonColourId,
                         m == nova::graviton::DynamicsMode::Limiter
                             ? juce::Colour(0xffffca28)
                             : juce::Colour(0xff21262d));
    btnModeLim.setColour(juce::TextButton::textColourOffId,
                         m == nova::graviton::DynamicsMode::Limiter
                             ? juce::Colour(0xff0d1117)
                             : juce::Colour(0xffc9d1d9));
  }

  void updateCurveButtons() {
    const auto c = masterX.getTargetCurve();
    auto setBtnState = [](juce::TextButton &btn, bool active) {
      btn.setColour(juce::TextButton::buttonColourId,
                    active ? juce::Colour(0xff1f6feb) : juce::Colour(0xff21262d));
      btn.setColour(juce::TextButton::textColourOffId,
                    active ? juce::Colour(0xffffffff) : juce::Colour(0xff8b949e));
    };
    setBtnState(btnCurveFlat, c == nova::graviton::TargetCurve::Flat);
    setBtnState(btnCurveBass, c == nova::graviton::TargetCurve::Bass);
    setBtnState(btnCurveAir, c == nova::graviton::TargetCurve::Air);
    setBtnState(btnCurveSmiley, c == nova::graviton::TargetCurve::Smiley);
  }

  void updateBandButtons() {
    auto setBandState = [](juce::TextButton &btn, bool active, juce::Colour activeCol) {
      btn.setColour(juce::TextButton::buttonColourId,
                    active ? activeCol : juce::Colour(0xff21262d));
      btn.setColour(juce::TextButton::textColourOffId,
                    active ? juce::Colour(0xff0d1117) : juce::Colour(0xffc9d1d9));
    };
    setBandState(btnBandLow, selectedBand == 0, juce::Colour(0xff29b6f6));
    setBandState(btnBandMid, selectedBand == 1, juce::Colour(0xffab47bc));
    setBandState(btnBandHigh, selectedBand == 2, juce::Colour(0xffffb300));
    setBandState(btnBandAll, selectedBand == 3, juce::Colour(0xff58a6ff));
  }

  void updateLinkButton() {
    const bool lk = masterX.getGainLinked();
    btnGainLink.setColour(juce::TextButton::buttonColourId,
                          lk ? juce::Colour(0xff1f6feb) : juce::Colour(0xff21262d));
    btnGainLink.setColour(juce::TextButton::textColourOffId,
                          lk ? juce::Colour(0xffffffff) : juce::Colour(0xff8b949e));
  }

  nova::graviton::MasterXMultiband &masterX;

  juce::TextButton presetBtn;
  juce::TextButton prevPresetBtn, nextPresetBtn;
  juce::TextButton compareBtn, copyBtn, pasteBtn, undoBtn, redoBtn;
  juce::TextButton powerBtn;

  // Dynamics Mode Buttons
  juce::TextButton btnModeExp, btnModeCom, btnModeLim;

  // Target Curve Buttons
  juce::TextButton btnCurveFlat, btnCurveBass, btnCurveAir, btnCurveSmiley;

  // Band Tabs
  juce::TextButton btnBandLow, btnBandMid, btnBandHigh, btnBandAll;
  juce::TextButton btnGainLink;

  int currentPresetIdx = 0;
  nova::graviton::MasterXMultiband::Preset stateA, stateB, clipboardPreset;
  bool hasClipboard = false;
  std::vector<nova::graviton::MasterXMultiband::Preset> undoHistory;
  int undoPos = -1;

  int selectedBand = 0; // 0=Low, 1=Mid, 2=High, 3=All (Link)

  MasterXMiniTransferSection miniTransfer;
  MasterXFrequencyGraph mainGraph;
  MasterXMeterTower meterTower;

  MasterXDial dialFactor;
  MasterXDial dialThreshold;
  MasterXDial dialRatio;
  MasterXDial dialAttack;
  MasterXDial dialRelease;
  MasterXDial dialBandGain;
  MasterXDial dialSoftClip;
  MasterXDial dialMix;
  MasterXDial dialInGain;
  MasterXDial dialOutGain;

  bool compareActive = false;
};

} // namespace nova
