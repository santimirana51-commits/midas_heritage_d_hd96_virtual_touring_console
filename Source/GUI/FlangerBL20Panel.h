#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Effects/GravitonFlangerBL20.h"
#include <array>
#include <cmath>
#include <functional>

namespace nova {

// ==============================================================================
// 1. Custom Fluted Hardware Rotary Knob (Silver / Red / Green Cap)
// ==============================================================================
class BL20RotaryKnob final : public juce::Component {
public:
  enum class CapStyle { Silver, Red, Green };

  BL20RotaryKnob(const juce::String &titleText, const juce::String &leftMinLabel,
                 const juce::String &rightMaxLabel, CapStyle capStyle,
                 double minV, double maxV, double defaultV)
      : title(titleText), minLabel(leftMinLabel), maxLabel(rightMaxLabel),
        style(capStyle), minVal(minV), maxVal(maxV), value(defaultV),
        defaultVal(defaultV) {}

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

  void setTitle(const juce::String &t) {
    title = t;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth();
    const float h = b.getHeight();

    // Top title label
    if (title.isNotEmpty()) {
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(juce::Colour(0xffe2e8f0));
      g.drawText(title, 0, 0, static_cast<int>(w), 13,
                 juce::Justification::centred);
    }

    const float topReserve = title.isNotEmpty() ? 13.0f : 2.0f;
    const float botReserve = 13.0f;
    const float availH = std::max(24.0f, h - topReserve - botReserve);
    const float radius = std::min(w, availH) * 0.39f;
    const float cx = w * 0.5f;
    const float cy = topReserve + availH * 0.52f;

    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.76f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 0.76f;

    // Outer white tick ring (matching BL-20 hardware dial scale)
    const int numTicks = 21;
    for (int i = 0; i < numTicks; ++i) {
      const float norm = static_cast<float>(i) / static_cast<float>(numTicks - 1);
      const float ang = startAngle + norm * (endAngle - startAngle);
      const float sinA = std::sin(ang);
      const float cosA = -std::cos(ang);
      const bool isMajor = (i == 0 || i == numTicks / 2 || i == numTicks - 1 || i % 5 == 0);
      const float r1 = radius * 1.06f;
      const float r2 = radius * (isMajor ? 1.24f : 1.16f);
      g.setColour(isMajor ? juce::Colours::white
                          : juce::Colours::white.withAlpha(0.65f));
      g.drawLine(cx + sinA * r1, cy + cosA * r1, cx + sinA * r2, cy + cosA * r2,
                 isMajor ? 1.5f : 1.0f);
    }

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - radius + 1.5f, cy - radius + 2.5f, radius * 2.0f,
                  radius * 2.0f);

    // Fluted dark Bakelite skirt
    g.setColour(juce::Colour(0xff141920));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff475569));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.3f);

    // Fluted ridges around perimeter
    for (int k = 0; k < 12; ++k) {
      const float a = static_cast<float>(k) * juce::MathConstants<float>::twoPi / 12.0f;
      const float rx = cx + std::sin(a) * (radius * 0.90f);
      const float ry = cy - std::cos(a) * (radius * 0.90f);
      g.setColour(juce::Colour(0xff2c3542));
      g.fillEllipse(rx - 1.6f, ry - 1.6f, 3.2f, 3.2f);
    }

    // Colored center cap (Silver, Red for REGEN, Green for MANUAL)
    const float capR = radius * 0.68f;
    juce::Colour topCol, botCol;
    if (style == CapStyle::Red) {
      topCol = juce::Colour(0xfff43f5e);
      botCol = juce::Colour(0xffbe123c);
    } else if (style == CapStyle::Green) {
      topCol = juce::Colour(0xff4ade80);
      botCol = juce::Colour(0xff15803d);
    } else {
      topCol = juce::Colour(0xffe2e8f0);
      botCol = juce::Colour(0xff94a3b8);
    }
    juce::ColourGradient capGrad(topCol, cx - capR * 0.4f, cy - capR * 0.6f,
                                 botCol, cx + capR * 0.5f, cy + capR * 0.7f,
                                 false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // Pointer line from center to outer edge
    const float normVal = static_cast<float>((value - minVal) / (maxVal - minVal));
    const float valAng = startAngle + normVal * (endAngle - startAngle);
    const float pSin = std::sin(valAng);
    const float pCos = -std::cos(valAng);

    g.setColour(juce::Colour(0xff0f172a));
    g.drawLine(cx + pSin * (capR * 0.15f), cy + pCos * (capR * 0.15f),
               cx + pSin * (radius * 0.95f), cy + pCos * (radius * 0.95f), 2.4f);
    g.setColour(juce::Colours::white);
    g.drawLine(cx + pSin * (capR * 0.25f), cy + pCos * (capR * 0.25f),
               cx + pSin * (radius * 0.92f), cy + pCos * (radius * 0.92f), 1.4f);

    // Bottom Min/Max labels & live readout on hover/drag
    g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffcbd5e1));
    if (isDragging && valueFormatter) {
      g.setColour(juce::Colour(0xff38bdf8));
      g.drawText(valueFormatter(value), 0, static_cast<int>(h - 13.0f),
                 static_cast<int>(w), 12, juce::Justification::centred);
    } else {
      g.drawText(minLabel, 2, static_cast<int>(h - 13.0f),
                 static_cast<int>(w * 0.48f), 12,
                 juce::Justification::centredLeft);
      g.drawText(maxLabel, static_cast<int>(w * 0.52f),
                 static_cast<int>(h - 13.0f), static_cast<int>(w * 0.46f), 12,
                 juce::Justification::centredRight);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    isDragging = true;
    dragStartY = e.position.y;
    dragStartVal = value;
    repaint();
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const double dy = static_cast<double>(dragStartY - e.position.y);
    const double sens = e.mods.isShiftDown() ? 450.0 : 140.0;
    setValue(dragStartVal + (dy / sens) * (maxVal - minVal), true);
  }

  void mouseUp(const juce::MouseEvent &) override {
    isDragging = false;
    repaint();
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
  juce::String title, minLabel, maxLabel;
  CapStyle style;
  double minVal, maxVal, value, defaultVal;
  float dragStartY = 0.0f;
  double dragStartVal = 0.0;
  bool isDragging = false;
};

// ==============================================================================
// 2. Interactive Multi-Node Function Generator Display (Green Phosphor CRT)
// ==============================================================================
class BL20FunctionDisplay final : public juce::Component {
public:
  explicit BL20FunctionDisplay(nova::graviton::FlangerBL20Engine &eng)
      : engine(&eng) {}

  void setEngine(nova::graviton::FlangerBL20Engine &eng) noexcept {
    engine = &eng;
    repaint();
  }

  std::function<void()> onStateModified;

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff070e0a));
    g.fillRoundedRectangle(b, 3.0f);
    g.setColour(juce::Colour(0xff1e3a2a));
    g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.2f);

    if (engine == nullptr)
      return;

    const float toolbarH = 34.0f;
    auto graphArea = b.withTrimmedBottom(toolbarH).reduced(6.0f, 5.0f);
    auto barArea = b.withTop(b.getBottom() - toolbarH).reduced(4.0f, 2.0f);

    // Subtle CRT grid lines
    g.setColour(juce::Colour(0xff132a1e));
    for (int gx = 1; gx < 8; ++gx) {
      const float x = graphArea.getX() + graphArea.getWidth() * (static_cast<float>(gx) / 8.0f);
      g.drawVerticalLine(static_cast<int>(x), graphArea.getY(), graphArea.getBottom());
    }
    for (int gy = 1; gy < 4; ++gy) {
      const float y = graphArea.getY() + graphArea.getHeight() * (static_cast<float>(gy) / 4.0f);
      g.drawHorizontalLine(static_cast<int>(y), graphArea.getX(), graphArea.getRight());
    }

    // MAX DELAY / MIN DELAY watermark labels
    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff234d36));
    g.drawText("MAX DELAY", graphArea.withHeight(11.0f), juce::Justification::centred);
    g.drawText("MIN DELAY", graphArea.withTop(graphArea.getBottom() - 11.0f),
               juce::Justification::centred);

    // Plot continuous curve from evaluateFunctionAt()
    juce::Path curvePath;
    juce::Path fillPath;
    const int numSteps = 120;
    for (int i = 0; i <= numSteps; ++i) {
      const double t = static_cast<double>(i) / static_cast<double>(numSteps);
      const double val = engine->evaluateFunctionAt(t);
      const float px = graphArea.getX() + static_cast<float>(t) * graphArea.getWidth();
      const float py = graphArea.getBottom() - static_cast<float>(val) * graphArea.getHeight();
      if (i == 0) {
        curvePath.startNewSubPath(px, py);
        fillPath.startNewSubPath(px, graphArea.getBottom());
        fillPath.lineTo(px, py);
      } else {
        curvePath.lineTo(px, py);
        fillPath.lineTo(px, py);
      }
    }
    fillPath.lineTo(graphArea.getRight(), graphArea.getBottom());
    fillPath.closeSubPath();

    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.10f));
    g.fillPath(fillPath);

    g.setColour(juce::Colour(0xff4ade80));
    g.strokePath(curvePath, juce::PathStrokeType(1.8f));

    // Live animated playhead vertical line
    const float playhead = engine->getFuncPlayhead();
    const float phX = graphArea.getX() + playhead * graphArea.getWidth();
    const float phY = graphArea.getBottom() -
                      static_cast<float>(engine->evaluateFunctionAt(playhead)) *
                          graphArea.getHeight();
    g.setColour(juce::Colour(0xff4ade80).withAlpha(0.45f));
    g.drawLine(phX, graphArea.getY(), phX, graphArea.getBottom(), 1.2f);
    g.setColour(juce::Colour(0xffbbf7d0));
    g.fillEllipse(phX - 3.5f, phY - 3.5f, 7.0f, 7.0f);

    // Draw editable breakpoint nodes
    const int nNodes = engine->getNumFuncNodes();
    const auto &nodes = engine->getFuncNodes();
    for (int i = 0; i < nNodes; ++i) {
      const float nx = graphArea.getX() + nodes[static_cast<size_t>(i)].x * graphArea.getWidth();
      const float ny = graphArea.getBottom() - nodes[static_cast<size_t>(i)].y * graphArea.getHeight();
      const bool isSel = (i == draggedNode);

      if (isSel) {
        g.setColour(juce::Colour(0xff4ade80).withAlpha(0.35f));
        g.fillEllipse(nx - 8.0f, ny - 8.0f, 16.0f, 16.0f);
      }
      g.setColour(juce::Colour(0xff070e0a));
      g.fillEllipse(nx - 4.5f, ny - 4.5f, 9.0f, 9.0f);
      g.setColour(isSel ? juce::Colours::white : juce::Colour(0xff86efac));
      g.drawEllipse(nx - 4.5f, ny - 4.5f, 9.0f, 9.0f, 1.5f);
      g.fillEllipse(nx - 2.0f, ny - 2.0f, 4.0f, 4.0f);
    }

    // Bottom CRT Toolbar (AMOUNT | RATE | DRAWING TOOLS | PRESET | DICE)
    g.setColour(juce::Colour(0xff142b1d));
    g.drawLine(barArea.getX(), barArea.getY(), barArea.getRight(), barArea.getY(), 1.0f);

    const float w = barArea.getWidth();
    auto rAmount = juce::Rectangle<float>(barArea.getX() + 2.0f, barArea.getY() + 3.0f, w * 0.14f, 16.0f);
    auto rRate   = juce::Rectangle<float>(rAmount.getRight() + 4.0f, barArea.getY() + 3.0f, w * 0.14f, 16.0f);
    auto rBi     = juce::Rectangle<float>(rRate.getRight() + 2.0f, barArea.getY() + 3.0f, 18.0f, 16.0f);
    auto rTools  = juce::Rectangle<float>(rBi.getRight() + 5.0f, barArea.getY() + 3.0f, w * 0.30f, 16.0f);
    auto rPreset = juce::Rectangle<float>(rTools.getRight() + 5.0f, barArea.getY() + 3.0f, w * 0.22f, 16.0f);
    auto rDice   = juce::Rectangle<float>(rPreset.getRight() + 4.0f, barArea.getY() + 3.0f,
                                          std::max(22.0f, barArea.getRight() - rPreset.getRight() - 6.0f), 16.0f);

    rectAmount = rAmount;
    rectRate = rRate;
    rectBipolar = rBi;
    rectTools = rTools;
    rectPreset = rPreset;
    rectDice = rDice;

    auto drawGreenBox = [&](juce::Rectangle<float> r, const juce::String &txt, bool active = false) {
      g.setColour(active ? juce::Colour(0xff164e33) : juce::Colour(0xff0b1911));
      g.fillRoundedRectangle(r, 2.0f);
      g.setColour(active ? juce::Colour(0xff4ade80) : juce::Colour(0xff27593c));
      g.drawRoundedRectangle(r, 2.0f, 1.0f);
      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
      g.setColour(active ? juce::Colours::white : juce::Colour(0xff4ade80));
      g.drawText(txt, r, juce::Justification::centred);
    };

    drawGreenBox(rAmount, juce::String(engine->getFuncAmount(), 2));
    drawGreenBox(rRate, juce::String(engine->getFuncRateHz(), 2) + " Hz");
    drawGreenBox(rBi, engine->isFuncBipolar() ? "+/-" : "+", engine->isFuncBipolar());

    // 6 mini drawing tools inside rTools
    const char *toolLabels[6] = {"/\\", "/", "\\", "STEP", "SMTH", "RST"};
    const float tw = rTools.getWidth() / 6.0f;
    const int activeTool = engine->getLastDrawingTool();
    for (int t = 0; t < 6; ++t) {
      auto tr = juce::Rectangle<float>(rTools.getX() + t * tw + 0.5f, rTools.getY(), tw - 1.0f, rTools.getHeight());
      drawGreenBox(tr, toolLabels[t], t == activeTool);
    }

    static const char *kFuncPresetNames[8] = {
        "INIT", "BL-20 WAVE", "8-STEP SEQ", "TRIANGLE",
        "STAIRCASE", "HEARTBEAT", "TWIN PEAKS", "GLITCH"};
    const int fpIdx = std::clamp(engine->getFuncPresetIndex(), 0, 7);
    drawGreenBox(rPreset, kFuncPresetNames[fpIdx]);

    // 5-pip Dice button above REGEN label
    drawGreenBox(rDice, "");
    g.setColour(juce::Colour(0xff4ade80));
    const float dcx = rDice.getCentreX();
    const float dcy = rDice.getCentreY();
    const float dx = std::min(4.2f, rDice.getWidth() * 0.24f);
    const float dy = 3.5f;
    for (auto pt : {juce::Point<float>(dcx, dcy),
                    juce::Point<float>(dcx - dx, dcy - dy),
                    juce::Point<float>(dcx + dx, dcy - dy),
                    juce::Point<float>(dcx - dx, dcy + dy),
                    juce::Point<float>(dcx + dx, dcy + dy)}) {
      g.fillEllipse(pt.x - 1.3f, pt.y - 1.3f, 2.6f, 2.6f);
    }

    // Bottom sub-captions
    g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff347850));
    const float lblY = barArea.getBottom() - 11.0f;
    g.drawText("AMOUNT", rAmount.withY(lblY).withHeight(10.0f), juce::Justification::centred);
    g.drawText("RATE", rRate.withY(lblY).withHeight(10.0f), juce::Justification::centred);
    g.drawText("DRAWING TOOLS", rTools.withY(lblY).withHeight(10.0f), juce::Justification::centred);
    g.drawText("PRESET", rPreset.withY(lblY).withHeight(10.0f), juce::Justification::centred);
    g.drawText("REGEN", rDice.withY(lblY).withHeight(10.0f), juce::Justification::centred);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (engine == nullptr)
      return;
    const auto pos = e.position;
    dragMode = DragMode::None;

    if (rectAmount.contains(pos)) {
      dragMode = DragMode::Amount;
      dragStartY = pos.y;
      dragStartVal = engine->getFuncAmount();
      return;
    }
    if (rectRate.contains(pos)) {
      dragMode = DragMode::Rate;
      dragStartY = pos.y;
      dragStartVal = engine->getFuncRateHz();
      return;
    }
    if (rectBipolar.contains(pos)) {
      engine->setFuncBipolar(!engine->isFuncBipolar());
      repaint();
      return;
    }
    if (rectTools.contains(pos)) {
      const int toolIdx = std::clamp(
          static_cast<int>((pos.x - rectTools.getX()) / (rectTools.getWidth() / 6.0f)),
          0, 5);
      engine->applyDrawingTool(toolIdx);
      if (onStateModified)
        onStateModified();
      repaint();
      return;
    }
    if (rectPreset.contains(pos)) {
      const int nextP = (engine->getFuncPresetIndex() + 1) % 8;
      engine->loadFuncPreset(nextP);
      if (onStateModified)
        onStateModified();
      repaint();
      return;
    }
    if (rectDice.contains(pos)) {
      engine->randomizeFuncPattern();
      if (onStateModified)
        onStateModified();
      repaint();
      return;
    }

    // Check breakpoint nodes in graph area
    const auto b = getLocalBounds().toFloat();
    const auto graphArea = b.withTrimmedBottom(34.0f).reduced(6.0f, 5.0f);
    if (!graphArea.contains(pos))
      return;

    const int nNodes = engine->getNumFuncNodes();
    const auto &nodes = engine->getFuncNodes();
    for (int i = 0; i < nNodes; ++i) {
      const float nx = graphArea.getX() + nodes[static_cast<size_t>(i)].x * graphArea.getWidth();
      const float ny = graphArea.getBottom() - nodes[static_cast<size_t>(i)].y * graphArea.getHeight();
      if (pos.getDistanceFrom({nx, ny}) < 10.0f) {
        if (e.mods.isRightButtonDown() && i > 0 && i < nNodes - 1) {
          engine->removeFuncNode(i);
          draggedNode = -1;
          repaint();
          return;
        }
        dragMode = DragMode::Node;
        draggedNode = i;
        repaint();
        return;
      }
    }
  }

  void mouseDoubleClick(const juce::MouseEvent &e) override {
    if (engine == nullptr)
      return;
    const auto b = getLocalBounds().toFloat();
    const auto graphArea = b.withTrimmedBottom(34.0f).reduced(6.0f, 5.0f);
    if (!graphArea.contains(e.position))
      return;

    const int nNodes = engine->getNumFuncNodes();
    const auto &nodes = engine->getFuncNodes();
    for (int i = 1; i < nNodes - 1; ++i) {
      const float nx = graphArea.getX() + nodes[static_cast<size_t>(i)].x * graphArea.getWidth();
      const float ny = graphArea.getBottom() - nodes[static_cast<size_t>(i)].y * graphArea.getHeight();
      if (e.position.getDistanceFrom({nx, ny}) < 10.0f) {
        engine->removeFuncNode(i);
        draggedNode = -1;
        repaint();
        return;
      }
    }

    const float normX = (e.position.x - graphArea.getX()) / graphArea.getWidth();
    const float normY = (graphArea.getBottom() - e.position.y) / graphArea.getHeight();
    engine->addFuncNode(normX, normY, 0.0f);
    repaint();
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (engine == nullptr)
      return;
    if (dragMode == DragMode::Amount) {
      const double dy = static_cast<double>(dragStartY - e.position.y);
      engine->setFuncAmount(dragStartVal + dy / 90.0);
      repaint();
    } else if (dragMode == DragMode::Rate) {
      const double dy = static_cast<double>(dragStartY - e.position.y);
      engine->setFuncRateHz(dragStartVal + dy / 25.0);
      repaint();
    } else if (dragMode == DragMode::Node && draggedNode >= 0) {
      const auto b = getLocalBounds().toFloat();
      const auto graphArea = b.withTrimmedBottom(34.0f).reduced(6.0f, 5.0f);
      const float normX = (e.position.x - graphArea.getX()) / graphArea.getWidth();
      const float normY = (graphArea.getBottom() - e.position.y) / graphArea.getHeight();
      const float curCurve =
          engine->getFuncNodes()[static_cast<size_t>(draggedNode)].curve;
      engine->setFuncNode(draggedNode, normX, normY, curCurve);
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &) override {
    dragMode = DragMode::None;
    draggedNode = -1;
    repaint();
  }

private:
  enum class DragMode { None, Node, Amount, Rate };
  nova::graviton::FlangerBL20Engine *engine = nullptr;
  DragMode dragMode = DragMode::None;
  int draggedNode = -1;
  float dragStartY = 0.0f;
  double dragStartVal = 0.0;

  juce::Rectangle<float> rectAmount, rectRate, rectBipolar, rectTools,
      rectPreset, rectDice;
};

// ==============================================================================
// 3. Main Two-Tier Rack Panel: FlangerBL20Panel
// ==============================================================================
class FlangerBL20Panel final : public juce::Component, private juce::Timer {
public:
  explicit FlangerBL20Panel(nova::graviton::FlangerBL20Engine &eng)
      : engine(&eng),
        knobMix("DOPPLER", "DRY", "WET", BL20RotaryKnob::CapStyle::Silver, 0.0,
                100.0, 45.0),
        knobRegen("REGEN", "MIN", "MAX", BL20RotaryKnob::CapStyle::Red, 0.0,
                  98.0, 52.0),
        knobManual("", "MIN", "MAX", BL20RotaryKnob::CapStyle::Green, 0.0,
                   100.0, 32.0),
        knobEnvThresh("THRESHOLD", "MIN", "MAX",
                      BL20RotaryKnob::CapStyle::Silver, -48.0, 0.0, -24.0),
        knobEnvDecay("DECAY", "SLOW", "FAST", BL20RotaryKnob::CapStyle::Silver,
                     0.0, 100.0, 50.0),
        knobAutoRate("RATE", "SLOW", "FAST", BL20RotaryKnob::CapStyle::Silver,
                     0.02, 12.0, 0.35),
        knobAutoDepth("DEPTH", "MIN", "MAX", BL20RotaryKnob::CapStyle::Silver,
                      0.0, 100.0, 55.0),
        knobHiPass("HI-PASS", "OFF", "3KHZ", BL20RotaryKnob::CapStyle::Silver,
                   15.0, 3000.0, 15.0),
        knobStereoOffset("0", "-100", "+100", BL20RotaryKnob::CapStyle::Silver,
                         -100.0, 100.0, 0.0),
        funcDisplay(eng) {

    // Preset Header Bar
    const auto &presets = nova::graviton::FlangerBL20Engine::getFactoryPresets();
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
        engine->loadPreset(idx);
        syncFromEngine();
      }
    };
    addAndMakeVisible(presetCombo);

    btnPrevPreset.setButtonText("<");
    btnPrevPreset.onClick = [this] {
      if (engine == nullptr)
        return;
      const int n = static_cast<int>(
          nova::graviton::FlangerBL20Engine::getFactoryPresets().size());
      const int prev = (engine->getCurrentPresetIndex() - 1 + n) % n;
      engine->loadPreset(prev);
      syncFromEngine();
    };
    addAndMakeVisible(btnPrevPreset);

    btnNextPreset.setButtonText(">");
    btnNextPreset.onClick = [this] {
      if (engine == nullptr)
        return;
      const int n = static_cast<int>(
          nova::graviton::FlangerBL20Engine::getFactoryPresets().size());
      const int next = (engine->getCurrentPresetIndex() + 1) % n;
      engine->loadPreset(next);
      syncFromEngine();
    };
    addAndMakeVisible(btnNextPreset);

    // Formatters & Callbacks for Knobs
    knobMix.valueFormatter = [](double v) {
      return juce::String(v, 0) + " %";
    };
    knobMix.onValueChanged = [this](double v) {
      if (engine)
        engine->setMix(v * 0.01);
    };
    addAndMakeVisible(knobMix);

    knobRegen.valueFormatter = [](double v) {
      return juce::String(v, 0) + " %";
    };
    knobRegen.onValueChanged = [this](double v) {
      if (engine)
        engine->setRegen(v * 0.01);
    };
    addAndMakeVisible(knobRegen);

    knobManual.valueFormatter = [](double v) {
      return juce::String(v, 0) + " %";
    };
    knobManual.onValueChanged = [this](double v) {
      if (engine)
        engine->setManualValue(v * 0.01);
    };
    addAndMakeVisible(knobManual);

    knobEnvThresh.valueFormatter = [](double v) {
      return juce::String(v, 1) + " dB";
    };
    knobEnvThresh.onValueChanged = [this](double v) {
      if (engine)
        engine->setEnvThresholdDb(v);
    };
    addAndMakeVisible(knobEnvThresh);

    // Note: on BL-20, DECAY knob goes from SLOW (left, ~1500 ms) to FAST (right, ~15 ms)
    knobEnvDecay.valueFormatter = [this](double) {
      return engine ? (juce::String(engine->getEnvDecayMs(), 0) + " ms")
                    : juce::String("200 ms");
    };
    knobEnvDecay.onValueChanged = [this](double v) {
      if (engine) {
        // 0 = SLOW (1500 ms), 100 = FAST (15 ms)
        const double ms = 1500.0 * std::pow(15.0 / 1500.0, v * 0.01);
        engine->setEnvDecayMs(ms);
      }
    };
    addAndMakeVisible(knobEnvDecay);

    knobAutoRate.valueFormatter = [this](double v) {
      if (engine && engine->isTempoSync())
        return juce::String(engine->getTempoSyncDivisionName()) + " Sync";
      return juce::String(v, 2) + " Hz";
    };
    knobAutoRate.onValueChanged = [this](double v) {
      if (engine)
        engine->setAutoRateHz(v);
    };
    addAndMakeVisible(knobAutoRate);

    knobAutoDepth.valueFormatter = [](double v) {
      return juce::String(v, 0) + " %";
    };
    knobAutoDepth.onValueChanged = [this](double v) {
      if (engine)
        engine->setAutoDepth(v * 0.01);
    };
    addAndMakeVisible(knobAutoDepth);

    knobHiPass.valueFormatter = [](double v) {
      return (v <= 20.0) ? juce::String("OFF")
                         : (juce::String(v, 0) + " Hz");
    };
    knobHiPass.onValueChanged = [this](double v) {
      if (engine)
        engine->setHiPassHz(v);
    };
    addAndMakeVisible(knobHiPass);

    knobStereoOffset.valueFormatter = [](double v) {
      return (v >= 0.0 ? "+" : "") + juce::String(v, 0) + " %";
    };
    knobStereoOffset.onValueChanged = [this](double v) {
      if (engine)
        engine->setStereoOffset(v * 0.01);
    };
    addAndMakeVisible(knobStereoOffset);

    addAndMakeVisible(funcDisplay);

    syncFromEngine();
    startTimerHz(30);
  }

  ~FlangerBL20Panel() override { stopTimer(); }

  void setEngine(nova::graviton::FlangerBL20Engine &eng) {
    engine = &eng;
    funcDisplay.setEngine(eng);
    syncFromEngine();
  }

  void syncFromEngine() {
    if (engine == nullptr)
      return;
    presetCombo.setSelectedId(engine->getCurrentPresetIndex() + 1,
                              juce::dontSendNotification);
    knobMix.setValue(engine->getMix() * 100.0, false);
    knobRegen.setValue(engine->getRegen() * 100.0, false);
    knobManual.setValue(engine->getManualValue() * 100.0, false);
    knobEnvThresh.setValue(engine->getEnvThresholdDb(), false);

    // Inverse map decay ms to 0..100 (SLOW..FAST)
    const double curMs = std::clamp(engine->getEnvDecayMs(), 15.0, 1500.0);
    const double decayDial =
        100.0 * std::log(curMs / 1500.0) / std::log(15.0 / 1500.0);
    knobEnvDecay.setValue(std::clamp(decayDial, 0.0, 100.0), false);

    knobAutoRate.setValue(engine->getAutoRateHz(), false);
    knobAutoDepth.setValue(engine->getAutoDepth() * 100.0, false);
    knobHiPass.setValue(engine->getHiPassHz(), false);
    knobStereoOffset.setValue(engine->getStereoOffset() * 100.0, false);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0b0f16));

    if (engine == nullptr)
      return;

    const float headerH = 26.0f;
    // Top header strip
    g.setColour(juce::Colour(0xff111827));
    g.fillRect(0.0f, 0.0f, b.getWidth(), headerH);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawHorizontalLine(static_cast<int>(headerH), 0.0f, b.getWidth());

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawText("NOVA HD // FLANGER BL-20", 8, 0,
               190, static_cast<int>(headerH),
               juce::Justification::centredLeft);

    // 8-Instrument Quick-Select Bar (01..08 Custom Mixing Presets)
    static const char *kInstLabels[8] = {
        "Lead Vox", "BG Vox", "Elec Gtr", "Gtr Solo",
        "Synth/Pad", "Snare", "Vocal FX", "Electronic"};
    const int curPreset = engine->getCurrentPresetIndex();
    for (int i = 0; i < 8; ++i) {
      const auto &rBtn = rectInstBtns[static_cast<size_t>(i)];
      if (rBtn.getWidth() > 4.0f) {
        const bool isSel = (curPreset == i);
        g.setColour(isSel ? juce::Colour(0xff0284c7) : juce::Colour(0xff1e293b));
        g.fillRoundedRectangle(rBtn, 3.0f);
        g.setColour(isSel ? juce::Colour(0xff38bdf8) : juce::Colour(0xff334155));
        g.drawRoundedRectangle(rBtn, 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        g.setColour(isSel ? juce::Colours::white : juce::Colour(0xffcbd5e1));
        g.drawText(kInstLabels[i], rBtn, juce::Justification::centred);
      }
    }

    // Live BBD Delay Telemetry in Header
    const float dL = engine->getDelayMsL();
    const float dR = engine->getDelayMsR();
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff4ade80));
    g.drawText("BBD L:" + juce::String(dL, 2) +
                   "ms R:" + juce::String(dR, 2) + "ms",
               static_cast<int>(b.getWidth() - 172.0f), 0, 166,
               static_cast<int>(headerH), juce::Justification::centredRight);

    // Two-tier 19-inch Rackmount Layout
    auto body = b.withTrimmedTop(headerH + 2.0f).reduced(4.0f, 2.0f);
    const float tierGap = 4.0f;
    const float tierH = (body.getHeight() - tierGap) * 0.5f;
    auto topUnit = body.removeFromTop(tierH);
    body.removeFromTop(tierGap);
    auto botUnit = body;

    drawRackChassis(g, topUnit, true);
    drawRackChassis(g, botUnit, false);

    // Draw Blue Faceplate Bays inside Top Unit
    drawBlueBay(g, rectTopLeftBay);
    drawBlueBay(g, rectTopRightBay);

    // Draw Blue Faceplate Bays inside Bottom Unit
    drawBlueBay(g, rectBotHiPassBay);
    drawBlueBay(g, rectBotFuncBay);
    drawBlueBay(g, rectBotShapeBay);
    drawBlueBay(g, rectBotOffsetBay);

    // =========================================================================
    // TOP-LEFT BAY: POWER / MONO-STEREO / ZERO CR / MIX / REGEN
    // =========================================================================
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("POWER", static_cast<int>(rectTopLeftBay.getX() + 10.0f),
               static_cast<int>(rectTopLeftBay.getY() + 6.0f), 60, 13,
               juce::Justification::centredLeft);
    g.drawText("MIX", static_cast<int>(knobMix.getX()),
               static_cast<int>(rectTopLeftBay.getY() + 6.0f),
               knobMix.getWidth(), 13, juce::Justification::centred);
    g.drawText("REGEN", static_cast<int>(knobRegen.getX()),
               static_cast<int>(rectTopLeftBay.getY() + 6.0f),
               knobRegen.getWidth(), 13, juce::Justification::centred);

    // 3 Left Switches + Status LEDs + Vertical Guide Lines
    drawLedWithSwitch(g, rectSwPower, engine->isEnabled(),
                      juce::Colour(0xffef4444), "ON", "OFF");
    drawLedWithSwitch(g, rectSwStereo, !engine->isStereoMode(),
                      juce::Colour(0xfff59e0b), "MONO", "STEREO");
    drawLedWithSwitch(g, rectSwZeroCr, engine->isZeroCrossing(),
                      juce::Colour(0xffef4444), "ZERO CR", "DEFAULT");

    // Red CLIP LED above REGEN (click to clear clip indicator)
    const float clipLedX = static_cast<float>(knobRegen.getX() + knobRegen.getWidth() * 0.5f - 18.0f);
    const float clipLedY = rectTopLeftBay.getY() + 21.0f;
    rectClipLed = {clipLedX, clipLedY - 2.0f, 44.0f, 12.0f};
    drawRectangularLed(g, {clipLedX, clipLedY, 12.0f, 7.0f},
                       engine->isClipping(), juce::Colour(0xffef4444));
    g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("CLIP", static_cast<int>(clipLedX + 14.0f),
               static_cast<int>(clipLedY - 2.0f), 28, 11,
               juce::Justification::centredLeft);

    // =========================================================================
    // TOP-RIGHT BAY: CONTROL MODE / MANUAL / ENV / AUTO
    // =========================================================================
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("CONTROL MODE", static_cast<int>(rectModMeter.getX() - 6.0f),
               static_cast<int>(rectTopRightBay.getY() + 6.0f),
               static_cast<int>(rectModMeter.getWidth() + 12.0f), 13,
               juce::Justification::centred);

    // 12-Segment Modulation LED Bar Meter
    drawModulationBarMeter(g, rectModMeter,
                           0.5f * (engine->getControlModMeterL() +
                                   engine->getControlModMeterR()));

    // 3 Combinable Control Mode Push-Buttons (MANUAL | ENV | AUTO)
    drawModePushButton(g, rectBtnManualMode, "MANUAL",
                       engine->isManualModeEnabled());
    drawModePushButton(g, rectBtnEnvMode, "ENV", engine->isEnvModeEnabled());
    drawModePushButton(g, rectBtnAutoMode, "AUTO", engine->isAutoModeEnabled());

    // Lower bracket connecting the 3 mode buttons
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    const float brkY = rectBtnManualMode.getBottom() + 5.0f;
    g.drawLine(rectBtnManualMode.getCentreX(), rectBtnManualMode.getBottom() + 1.0f,
               rectBtnManualMode.getCentreX(), brkY, 1.2f);
    g.drawLine(rectBtnAutoMode.getCentreX(), rectBtnAutoMode.getBottom() + 1.0f,
               rectBtnAutoMode.getCentreX(), brkY, 1.2f);
    g.drawLine(rectBtnManualMode.getCentreX(), brkY,
               rectBtnAutoMode.getCentreX(), brkY, 1.2f);

    // MANUAL header + FCT green LED
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("MANUAL", knobManual.getX(),
               static_cast<int>(rectTopRightBay.getY() + 6.0f),
               knobManual.getWidth(), 13, juce::Justification::centred);
    const float fctLedX = static_cast<float>(knobManual.getX() + knobManual.getWidth() * 0.5f - 16.0f);
    const float fctLedY = rectTopRightBay.getY() + 21.0f;
    drawRectangularLed(g, {fctLedX, fctLedY, 12.0f, 7.0f},
                       engine->isAddToManual(), juce::Colour(0xff4ade80));
    g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("FCT", static_cast<int>(fctLedX + 14.0f),
               static_cast<int>(fctLedY - 2.0f), 26, 11,
               juce::Justification::centredLeft);

    // ENV Bracket Header & EXT/INT Switch
    drawBracketHeader(g, static_cast<float>(knobEnvThresh.getX() + 8),
                      static_cast<float>(knobEnvDecay.getRight() - 8),
                      rectTopRightBay.getY() + 11.0f, "ENV");
    drawRectangularLed(
        g, {rectSwExtEnv.getCentreX() - 6.0f, rectTopRightBay.getY() + 21.0f, 12.0f, 7.0f},
        engine->getEnvTelemetry() > 0.05f, juce::Colour(0xfff59e0b));
    drawSimpleRockerSwitch(g, rectSwExtEnv, engine->isExtEnvSource(),
                           "EXT ENV", "INT ENV");

    // AUTO Bracket Header, Tempo Sync Note Button & Reverse Sweep O/P 2 Switch
    drawBracketHeader(g, static_cast<float>(knobAutoRate.getX() + 8),
                      static_cast<float>(knobAutoDepth.getRight() - 8),
                      rectTopRightBay.getY() + 11.0f, "AUTO");
    // Tempo Sync button next to RATE
    const bool syncOn = engine->isTempoSync();
    g.setColour(syncOn ? juce::Colour(0xfff59e0b) : juce::Colour(0xff0f172a));
    g.fillEllipse(rectBtnTempoSync);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawEllipse(rectBtnTempoSync, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(syncOn ? juce::Colours::black : juce::Colours::white);
    g.drawText("S", rectBtnTempoSync, juce::Justification::centred);

    drawRectangularLed(
        g, {rectSwRevSweep.getCentreX() - 6.0f, rectTopRightBay.getY() + 21.0f, 12.0f, 7.0f},
        engine->getAutoTelemetry() > 0.5f, juce::Colour(0xffa3e635));
    drawSimpleRockerSwitch(g, rectSwRevSweep, engine->isReverseSweepOp2(),
                           "REVERSE\nSWEEP O/P 2", "NORMAL");

    // =========================================================================
    // BOTTOM BAYS: HI-PASS | FUNCTION | AUTO SHAPE | STEREO OFFSET
    // =========================================================================
    // Bay 2: FUNCTION Header + ADD TO MANUAL Button
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("FUNCTION", static_cast<int>(rectBotFuncBay.getX() + 4.0f),
               static_cast<int>(rectBotFuncBay.getY() + 8.0f), 68, 14,
               juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.drawFittedText("ADD TO\nMANUAL",
                     juce::Rectangle<int>(static_cast<int>(rectBtnAddToManual.getX() - 6.0f),
                                          static_cast<int>(rectBtnAddToManual.getY() - 24.0f),
                                          static_cast<int>(rectBtnAddToManual.getWidth() + 12.0f), 22),
                     juce::Justification::centred, 2);
    drawHardwareToggleButton(g, rectBtnAddToManual, engine->isAddToManual());

    // Bay 3: AUTO SHAPE (5 LEDs + Waveform Symbols + Cycle Button)
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("AUTO SHAPE", static_cast<int>(rectBotShapeBay.getX()),
               static_cast<int>(rectBotShapeBay.getY() + 8.0f),
               static_cast<int>(rectBotShapeBay.getWidth()), 13,
               juce::Justification::centred);

    const int curShape = static_cast<int>(engine->getAutoShape());
    for (int s = 0; s < 5; ++s) {
      const auto &rRow = rectShapeRows[static_cast<size_t>(s)];
      const bool isActive = (s == curShape);
      auto ledR = juce::Rectangle<float>(rRow.getX() + 6.0f,
                                         rRow.getCentreY() - 3.5f, 11.0f, 7.0f);
      drawRectangularLed(g, ledR, isActive, juce::Colours::white);

      auto symR = juce::Rectangle<float>(ledR.getRight() + 6.0f, rRow.getY(),
                                         rRow.getWidth() - 26.0f, rRow.getHeight());
      g.setColour(isActive ? juce::Colours::white
                           : juce::Colours::white.withAlpha(0.65f));
      drawAutoShapeIcon(g, symR, s);
    }
    drawHardwareToggleButton(g, rectBtnAutoShape, false);

    // Bay 4: STEREO OFFSET Header & Center Line
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("STEREO OFFSET", static_cast<int>(rectBotOffsetBay.getX()),
               static_cast<int>(rectBotOffsetBay.getY() + 8.0f),
               static_cast<int>(rectBotOffsetBay.getWidth()), 13,
               juce::Justification::centred);
    const float offCx = static_cast<float>(knobStereoOffset.getX() +
                                           knobStereoOffset.getWidth() * 0.5f);
    g.drawLine(offCx, rectBotOffsetBay.getY() + 23.0f, offCx,
               static_cast<float>(knobStereoOffset.getY() - 2.0f), 1.2f);
  }

  void resized() override {
    const auto b = getLocalBounds().toFloat();
    const float headerH = 26.0f;

    btnPrevPreset.setBounds(178, 3, 22, 20);
    presetCombo.setBounds(202, 3, 210, 20);
    btnNextPreset.setBounds(414, 3, 22, 20);

    // 8-Instrument Quick-Select Bar (01..08 Custom Mixing Presets)
    const float instStartX = 442.0f;
    const float instEndX = std::max(instStartX + 160.0f, b.getWidth() - 176.0f);
    const float instBtnW = (instEndX - instStartX) / 8.0f;
    for (int i = 0; i < 8; ++i) {
      rectInstBtns[static_cast<size_t>(i)] = {
          instStartX + i * instBtnW + 1.0f, 4.0f, instBtnW - 2.0f, 18.0f};
    }

    auto body = b.withTrimmedTop(headerH + 2.0f).reduced(4.0f, 2.0f);
    const float tierGap = 4.0f;
    const float tierH = (body.getHeight() - tierGap) * 0.5f;
    auto topUnit = body.removeFromTop(tierH);
    body.removeFromTop(tierGap);
    auto botUnit = body;

    const float earLeft = 42.0f;
    const float earRight = 64.0f;

    // --- TOP RACK BAYS ---
    auto topInner = topUnit.withTrimmedLeft(earLeft).withTrimmedRight(earRight).reduced(4.0f, 5.0f);
    const float topLeftW = topInner.getWidth() * 0.31f;
    rectTopLeftBay = topInner.removeFromLeft(topLeftW);
    topInner.removeFromLeft(5.0f);
    rectTopRightBay = topInner;

    // Layout inside Top-Left Bay (3 switches on left + DOPPLER Mix knob + REGEN knob)
    const float swAreaW = rectTopLeftBay.getWidth() * 0.44f;
    const float swColW = swAreaW / 3.0f;
    const float swY = rectTopLeftBay.getBottom() - 36.0f;
    rectSwPower = {rectTopLeftBay.getX() + 6.0f, swY, swColW - 4.0f, 16.0f};
    rectSwStereo = {rectSwPower.getRight() + 3.0f, swY, swColW - 4.0f, 16.0f};
    rectSwZeroCr = {rectSwStereo.getRight() + 3.0f, swY, swColW - 4.0f, 16.0f};

    const float mixRegenW = (rectTopLeftBay.getWidth() - swAreaW - 8.0f) * 0.5f;
    const float knobTopY = rectTopLeftBay.getY() + 26.0f;
    const float knobH = rectTopLeftBay.getHeight() - 30.0f;
    knobMix.setBounds(static_cast<int>(rectTopLeftBay.getX() + swAreaW + 2.0f),
                      static_cast<int>(knobTopY), static_cast<int>(mixRegenW),
                      static_cast<int>(knobH));
    knobRegen.setBounds(static_cast<int>(knobMix.getRight() + 2),
                        static_cast<int>(knobTopY), static_cast<int>(mixRegenW),
                        static_cast<int>(knobH));

    // Layout inside Top-Right Bay (CONTROL MODE + MANUAL + ENV + AUTO)
    const float trW = rectTopRightBay.getWidth();
    const float modeSecW = trW * 0.21f;
    rectModMeter = {rectTopRightBay.getX() + 14.0f,
                    rectTopRightBay.getY() + 22.0f, modeSecW - 24.0f, 9.0f};
    const float modeBtnW = (modeSecW - 16.0f) / 3.0f;
    const float modeBtnY = rectTopRightBay.getBottom() - 36.0f;
    rectBtnManualMode = {rectTopRightBay.getX() + 8.0f, modeBtnY, modeBtnW - 2.0f, 16.0f};
    rectBtnEnvMode = {rectBtnManualMode.getRight() + 2.0f, modeBtnY, modeBtnW - 2.0f, 16.0f};
    rectBtnAutoMode = {rectBtnEnvMode.getRight() + 2.0f, modeBtnY, modeBtnW - 2.0f, 16.0f};

    const float remW = trW - modeSecW - 8.0f;
    const float unitW = remW / 6.3f;
    float curX = rectTopRightBay.getX() + modeSecW + 2.0f;

    knobManual.setBounds(static_cast<int>(curX), static_cast<int>(knobTopY),
                         static_cast<int>(unitW * 1.05f), static_cast<int>(knobH));
    curX += unitW * 1.08f;

    knobEnvThresh.setBounds(static_cast<int>(curX), static_cast<int>(knobTopY),
                            static_cast<int>(unitW), static_cast<int>(knobH));
    curX += unitW;

    knobEnvDecay.setBounds(static_cast<int>(curX), static_cast<int>(knobTopY),
                           static_cast<int>(unitW), static_cast<int>(knobH));
    curX += unitW;

    rectSwExtEnv = {curX + 4.0f, swY, unitW * 0.62f - 8.0f, 16.0f};
    curX += unitW * 0.64f;

    knobAutoRate.setBounds(static_cast<int>(curX), static_cast<int>(knobTopY),
                           static_cast<int>(unitW), static_cast<int>(knobH));
    rectBtnTempoSync = {static_cast<float>(knobAutoRate.getRight() - 16),
                        knobTopY - 1.0f, 14.0f, 14.0f};
    curX += unitW;

    knobAutoDepth.setBounds(static_cast<int>(curX), static_cast<int>(knobTopY),
                            static_cast<int>(unitW), static_cast<int>(knobH));
    curX += unitW;

    rectSwRevSweep = {curX + 2.0f, swY,
                      std::max(32.0f, rectTopRightBay.getRight() - curX - 8.0f),
                      16.0f};

    // --- BOTTOM RACK BAYS ---
    auto botInner = botUnit.withTrimmedLeft(earLeft).withTrimmedRight(earRight).reduced(4.0f, 5.0f);
    const float biW = botInner.getWidth();
    const float hpW = biW * 0.135f;
    const float offsetW = biW * 0.135f;
    const float shapeW = biW * 0.130f;

    rectBotHiPassBay = botInner.removeFromLeft(hpW);
    botInner.removeFromLeft(5.0f);
    rectBotOffsetBay = botInner.removeFromRight(offsetW);
    botInner.removeFromRight(5.0f);
    rectBotShapeBay = botInner.removeFromRight(shapeW);
    botInner.removeFromRight(5.0f);
    rectBotFuncBay = botInner;

    knobHiPass.setBounds(rectBotHiPassBay.reduced(6.0f, 8.0f).toNearestInt());

    // Function Generator bay: left strip for ADD TO MANUAL, rest for CRT display
    const float funcLeftW = 74.0f;
    rectBtnAddToManual = {rectBotFuncBay.getX() + 18.0f,
                          rectBotFuncBay.getBottom() - 34.0f, 38.0f, 18.0f};
    funcDisplay.setBounds(
        rectBotFuncBay.withTrimmedLeft(funcLeftW).reduced(5.0f, 6.0f).toNearestInt());

    // Auto Shape bay: 5 rows + bottom button
    const float shTop = rectBotShapeBay.getY() + 24.0f;
    const float shBot = rectBotShapeBay.getBottom() - 28.0f;
    const float rowH = (shBot - shTop) / 5.0f;
    for (int s = 0; s < 5; ++s) {
      rectShapeRows[static_cast<size_t>(s)] = {
          rectBotShapeBay.getX() + 8.0f, shTop + s * rowH,
          rectBotShapeBay.getWidth() - 16.0f, rowH};
    }
    rectBtnAutoShape = {rectBotShapeBay.getCentreX() - 18.0f,
                        rectBotShapeBay.getBottom() - 23.0f, 36.0f, 16.0f};

    // Stereo Offset bay
    knobStereoOffset.setBounds(
        rectBotOffsetBay.withTrimmedTop(18.0f).reduced(6.0f, 4.0f).toNearestInt());
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (engine == nullptr)
      return;
    const auto p = e.position;

    // 8-Instrument Quick-Select Bar (01..08 Custom Mixing Presets)
    for (int i = 0; i < 8; ++i) {
      if (rectInstBtns[static_cast<size_t>(i)].contains(p)) {
        engine->loadPreset(i);
        syncFromEngine();
        return;
      }
    }

    if (rectClipLed.contains(p)) {
      engine->clearClipFlag();
      repaint();
      return;
    }

    if (rectSwPower.expanded(4.0f, 12.0f).contains(p)) {
      engine->setEnabled(!engine->isEnabled());
      repaint();
    } else if (rectSwStereo.expanded(4.0f, 12.0f).contains(p)) {
      engine->setStereoMode(!engine->isStereoMode());
      repaint();
    } else if (rectSwZeroCr.expanded(4.0f, 12.0f).contains(p)) {
      engine->setZeroCrossing(!engine->isZeroCrossing());
      repaint();
    } else if (rectBtnManualMode.expanded(2.0f, 6.0f).contains(p)) {
      engine->setManualModeEnabled(!engine->isManualModeEnabled());
      repaint();
    } else if (rectBtnEnvMode.expanded(2.0f, 6.0f).contains(p)) {
      engine->setEnvModeEnabled(!engine->isEnvModeEnabled());
      repaint();
    } else if (rectBtnAutoMode.expanded(2.0f, 6.0f).contains(p)) {
      engine->setAutoModeEnabled(!engine->isAutoModeEnabled());
      repaint();
    } else if (rectSwExtEnv.expanded(4.0f, 12.0f).contains(p)) {
      engine->setExtEnvSource(!engine->isExtEnvSource());
      repaint();
    } else if (rectBtnTempoSync.expanded(3.0f).contains(p)) {
      engine->setTempoSync(!engine->isTempoSync());
      repaint();
    } else if (rectSwRevSweep.expanded(4.0f, 12.0f).contains(p)) {
      engine->setReverseSweepOp2(!engine->isReverseSweepOp2());
      repaint();
    } else if (rectBtnAddToManual.expanded(4.0f, 8.0f).contains(p)) {
      engine->setAddToManual(!engine->isAddToManual());
      repaint();
    } else if (rectBtnAutoShape.expanded(4.0f, 6.0f).contains(p)) {
      engine->cycleAutoShape();
      repaint();
    } else {
      for (int s = 0; s < 5; ++s) {
        if (rectShapeRows[static_cast<size_t>(s)].contains(p)) {
          engine->setAutoShape(
              static_cast<nova::graviton::FlangerBL20Engine::AutoShape>(s));
          repaint();
          break;
        }
      }
    }
  }

private:
  void timerCallback() override {
    funcDisplay.repaint();
    repaint();
  }

  // Draw 19-inch black anodized rack unit with mounting ears, screws & logos
  static void drawRackChassis(juce::Graphics &g, juce::Rectangle<float> r,
                              bool isTopUnit) {
    juce::ColourGradient chGrad(juce::Colour(0xff22262c), r.getX(), r.getY(),
                                juce::Colour(0xff14171b), r.getX(),
                                r.getBottom(), false);
    g.setGradientFill(chGrad);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0xff333942));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.2f);

    // Rack cutouts & hex screws on left and right ears
    auto drawRackEar = [&](float x, bool leftSide) {
      g.setColour(juce::Colour(0xff090b0e));
      const float holeW = 14.0f, holeH = 10.0f;
      g.fillRoundedRectangle(x, r.getY() + 8.0f, holeW, holeH, 4.5f);
      g.fillRoundedRectangle(x, r.getBottom() - 18.0f, holeW, holeH, 4.5f);

      const float sx = leftSide ? (r.getX() + 31.0f) : (r.getRight() - 52.0f);
      for (float sy : {r.getY() + 18.0f, r.getBottom() - 18.0f}) {
        g.setColour(juce::Colour(0xff0d1014));
        g.fillEllipse(sx - 4.5f, sy - 4.5f, 9.0f, 9.0f);
        g.setColour(juce::Colour(0xff3f4752));
        g.drawEllipse(sx - 4.5f, sy - 4.5f, 9.0f, 9.0f, 1.0f);
      }
    };
    drawRackEar(r.getX() + 6.0f, true);
    drawRackEar(r.getRight() - 20.0f, false);

    if (isTopUnit) {
      // Left emblem circle
      auto logoR = juce::Rectangle<float>(r.getX() + 8.0f,
                                          r.getCentreY() - 12.0f, 24.0f, 24.0f);
      g.setColour(juce::Colour(0xffcbd5e1));
      g.drawEllipse(logoR, 2.0f);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.drawText("A", logoR, juce::Justification::centred);

      // Right "flanger STEREO BL-20" silkscreen
      auto rightTxt = juce::Rectangle<float>(r.getRight() - 62.0f,
                                             r.getCentreY() - 26.0f, 58.0f, 52.0f);
      g.setColour(juce::Colours::white);
      g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
      g.drawText("flanger", rightTxt.removeFromTop(16.0f),
                 juce::Justification::centred);
      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
      g.setColour(juce::Colour(0xffcbd5e1));
      g.drawText("STEREO", rightTxt.removeFromTop(13.0f),
                 juce::Justification::centred);
      g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
      g.drawText("BL-20", rightTxt.removeFromTop(12.0f),
                 juce::Justification::centred);
    } else {
      auto rightTxt = juce::Rectangle<float>(r.getRight() - 62.0f,
                                             r.getBottom() - 42.0f, 58.0f, 28.0f);
      g.setFont(juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));
      g.setColour(juce::Colour(0xff94a3b8));
      g.drawFittedText("NOVA HD\nBBD ENGINE", rightTxt.toNearestInt(),
                       juce::Justification::centred, 2);
    }
  }

  // Draw Royal-Blue Anodized Inset Bay with Silver Trim Border
  static void drawBlueBay(juce::Graphics &g, juce::Rectangle<float> r) {
    juce::ColourGradient blueGrad(juce::Colour(0xff235587), r.getX(), r.getY(),
                                  juce::Colour(0xff183d63), r.getX(),
                                  r.getBottom(), false);
    g.setGradientFill(blueGrad);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(juce::Colour(0xffdce6f2));
    g.drawRoundedRectangle(r.reduced(0.8f), 6.0f, 1.5f);
  }

  static void drawRectangularLed(juce::Graphics &g, juce::Rectangle<float> r,
                                 bool on, juce::Colour col) {
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(r.expanded(1.0f), 1.5f);
    if (on) {
      g.setColour(col.withAlpha(0.38f));
      g.fillRoundedRectangle(r.expanded(3.0f), 3.0f);
      g.setColour(col);
      g.fillRoundedRectangle(r, 1.2f);
    } else {
      g.setColour(col.darker(2.2f).withAlpha(0.45f));
      g.fillRoundedRectangle(r, 1.2f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawRoundedRectangle(r, 1.2f, 0.7f);
  }

  void drawLedWithSwitch(juce::Graphics &g, juce::Rectangle<float> swRect,
                         bool topStateOn, juce::Colour ledCol,
                         const char *topLabel, const char *botLabel) const {
    const float cx = swRect.getCentreX();
    const float ledY = rectTopLeftBay.getY() + 21.0f;
    drawRectangularLed(g, {cx - 6.0f, ledY, 12.0f, 7.0f}, topStateOn, ledCol);

    // Vertical guide line from LED down toward switch label
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawLine(cx, ledY + 10.0f, cx, swRect.getY() - 14.0f, 1.2f);

    drawSimpleRockerSwitch(g, swRect, topStateOn, topLabel, botLabel);
  }

  static void drawSimpleRockerSwitch(juce::Graphics &g,
                                     juce::Rectangle<float> swRect,
                                     bool topStateActive, const char *topLabel,
                                     const char *botLabel) {
    g.setFont(juce::Font(juce::FontOptions(7.6f, juce::Font::bold)));
    g.setColour(topStateActive ? juce::Colours::white
                               : juce::Colour(0xffcbd5e1));
    g.drawFittedText(
        topLabel,
        juce::Rectangle<int>(static_cast<int>(swRect.getX() - 8.0f),
                             static_cast<int>(swRect.getY() - 19.0f),
                             static_cast<int>(swRect.getWidth() + 16.0f), 17),
        juce::Justification::centredBottom, 2);

    // Black recessed switch housing
    g.setColour(juce::Colour(0xff0b0f17));
    g.fillRoundedRectangle(swRect, 2.0f);
    g.setColour(juce::Colour(0xff475569));
    g.drawRoundedRectangle(swRect, 2.0f, 1.0f);

    // Rocker actuator
    auto cap = topStateActive
                   ? swRect.reduced(2.0f).withWidth(swRect.getWidth() * 0.54f)
                   : swRect.reduced(2.0f).withLeft(swRect.getCentreX() - 2.0f);
    g.setColour(topStateActive ? juce::Colour(0xff475569)
                               : juce::Colour(0xff283240));
    g.fillRoundedRectangle(cap, 1.5f);

    g.setColour(!topStateActive ? juce::Colours::white
                                : juce::Colour(0xffcbd5e1));
    g.drawText(botLabel, static_cast<int>(swRect.getX() - 6.0f),
               static_cast<int>(swRect.getBottom() + 2.0f),
               static_cast<int>(swRect.getWidth() + 12.0f), 11,
               juce::Justification::centred);
  }

  static void drawModePushButton(juce::Graphics &g, juce::Rectangle<float> r,
                                 const char *label, bool active) {
    g.setFont(juce::Font(juce::FontOptions(7.8f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText(label, static_cast<int>(r.getX() - 4.0f),
               static_cast<int>(r.getY() - 13.0f),
               static_cast<int>(r.getWidth() + 8.0f), 12,
               juce::Justification::centred);

    g.setColour(juce::Colour(0xff0b0f17));
    g.fillRoundedRectangle(r, 2.0f);
    if (active) {
      g.setColour(juce::Colour(0xfff59e0b));
      g.fillRoundedRectangle(r.reduced(2.2f), 1.5f);
    } else {
      g.setColour(juce::Colour(0xff26303d));
      g.fillRoundedRectangle(r.reduced(2.2f), 1.5f);
    }
    g.setColour(juce::Colour(0xff475569));
    g.drawRoundedRectangle(r, 2.0f, 1.0f);
  }

  static void drawHardwareToggleButton(juce::Graphics &g,
                                       juce::Rectangle<float> r, bool active) {
    g.setColour(juce::Colour(0xff0b0f17));
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(active ? juce::Colour(0xff38bdf8) : juce::Colour(0xff2c3644));
    g.fillRoundedRectangle(r.reduced(2.2f), 1.5f);
    g.setColour(juce::Colour(0xff64748b));
    g.drawRoundedRectangle(r, 2.0f, 1.0f);
  }

  static void drawModulationBarMeter(juce::Graphics &g,
                                     juce::Rectangle<float> r, float normVal) {
    g.setColour(juce::Colour(0xff0b0f17));
    g.fillRoundedRectangle(r.expanded(2.0f, 1.5f), 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(r.expanded(2.0f, 1.5f), 2.0f, 0.8f);

    const int numSegs = 12;
    const float segW = r.getWidth() / static_cast<float>(numSegs);
    const int litCount =
         std::clamp(static_cast<int>( std::round(normVal * numSegs)), 0, numSegs);
    for (int i = 0; i < numSegs; ++i) {
      auto sr = juce::Rectangle<float>(r.getX() + i * segW + 0.8f, r.getY(),
                                       segW - 1.6f, r.getHeight());
      if (i < litCount) {
        juce::Colour c = (i >= 9)   ? juce::Colour(0xffef4444)
                         : (i >= 6) ? juce::Colour(0xfff59e0b)
                                    : juce::Colour(0xff4ade80);
        g.setColour(c);
      } else {
        g.setColour(juce::Colour(0xff1e293b));
      }
      g.fillRect(sr);
    }
  }

  static void drawBracketHeader(juce::Graphics &g, float x1, float x2, float y,
                                const char *title) {
    const float mid = 0.5f * (x1 + x2);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(title, static_cast<int>(mid - 24.0f), static_cast<int>(y - 6.0f),
               48, 12, juce::Justification::centred);
    g.drawLine(x1, y, mid - 24.0f, y, 1.2f);
    g.drawLine(x1, y, x1, y + 12.0f, 1.2f);
    g.drawLine(mid + 24.0f, y, x2, y, 1.2f);
    g.drawLine(x2, y, x2, y + 12.0f, 1.2f);
  }

  static void drawAutoShapeIcon(juce::Graphics &g, juce::Rectangle<float> r,
                                int shapeIdx) {
    if (shapeIdx == 0) {
      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
      g.drawText("ORIG", r, juce::Justification::centredLeft);
      return;
    }
    juce::Path p;
    const float x0 = r.getX() + 2.0f;
    const float w = std::min(22.0f, r.getWidth() - 4.0f);
    const float yTop = r.getY() + 3.0f;
    const float yBot = r.getBottom() - 3.0f;
    const float yMid = 0.5f * (yTop + yBot);

    if (shapeIdx == 1) { // Sine
      p.startNewSubPath(x0, yMid);
      for (int i = 1; i <= 16; ++i) {
        const float t = static_cast<float>(i) / 16.0f;
        p.lineTo(x0 + t * w,
                 yMid - std::sin(t * juce::MathConstants<float>::twoPi) *
                            (yMid - yTop));
      }
    } else if (shapeIdx == 2) { // Square
      p.startNewSubPath(x0, yBot);
      p.lineTo(x0, yTop);
      p.lineTo(x0 + w * 0.5f, yTop);
      p.lineTo(x0 + w * 0.5f, yBot);
      p.lineTo(x0 + w, yBot);
      p.lineTo(x0 + w, yTop);
    } else if (shapeIdx == 3) { // Rising Saw
      p.startNewSubPath(x0, yBot);
      p.lineTo(x0 + w, yTop);
      p.lineTo(x0 + w, yBot);
    } else if (shapeIdx == 4) { // Falling Saw
      p.startNewSubPath(x0, yBot);
      p.lineTo(x0, yTop);
      p.lineTo(x0 + w, yBot);
    }
    g.strokePath(p, juce::PathStrokeType(1.5f));
  }

  nova::graviton::FlangerBL20Engine *engine = nullptr;

  juce::ComboBox presetCombo;
  juce::TextButton btnPrevPreset, btnNextPreset;

  BL20RotaryKnob knobMix, knobRegen, knobManual, knobEnvThresh, knobEnvDecay,
      knobAutoRate, knobAutoDepth, knobHiPass, knobStereoOffset;
  BL20FunctionDisplay funcDisplay;

  juce::Rectangle<float> rectTopLeftBay, rectTopRightBay;
  juce::Rectangle<float> rectBotHiPassBay, rectBotFuncBay, rectBotShapeBay,
      rectBotOffsetBay;

  juce::Rectangle<float> rectSwPower, rectSwStereo, rectSwZeroCr, rectClipLed;
  juce::Rectangle<float> rectModMeter;
  juce::Rectangle<float> rectBtnManualMode, rectBtnEnvMode, rectBtnAutoMode;
  juce::Rectangle<float> rectSwExtEnv, rectBtnTempoSync, rectSwRevSweep;
  juce::Rectangle<float> rectBtnAddToManual, rectBtnAutoShape;
  std::array<juce::Rectangle<float>, 5> rectShapeRows{};
  std::array<juce::Rectangle<float>, 8> rectInstBtns{};
};

} // namespace nova
