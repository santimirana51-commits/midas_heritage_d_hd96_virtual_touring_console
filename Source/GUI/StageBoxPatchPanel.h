#pragma once

#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <string>

namespace nova {

// ==============================================================================
// Modern Touring Console Interactive Crosspoint Patch Matrix
// Inspired by MIDAS HD96 / Behringer WING / Yamaha Rivage PM Grid Routing
// - 32 Console Input Channels
// - 32 Local Hardware Soundcard Ports + 16 Virtual PC Audio Ports (48 Sources)
// - Output Routing Grid (Master L/R, Mono, Aux 1-2 Monitor, Matrix 1-8, Direct Out)
// - Touch-Friendly Crosspoint Grid with Live Crosshair & LED Glow
// ==============================================================================
class StageBoxPatchPanel final : public juce::Component, private juce::Timer {
public:
  explicit StageBoxPatchPanel(MixerEngine &mixerEngine) : engine(mixerEngine) {
    setOpaque(true);

    // Main Mode Switchers
    tabInputPatch.setButtonText("1. INPUT PATCH MATRIX");
    tabInputPatch.setClickingTogglesState(true);
    tabInputPatch.setRadioGroupId(1001);
    tabInputPatch.setToggleState(true, juce::dontSendNotification);
    tabInputPatch.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00b4d8));
    tabInputPatch.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    tabInputPatch.onClick = [this] {
      isOutputMode = false;
      repaint();
    };
    addAndMakeVisible(tabInputPatch);

    tabOutputPatch.setButtonText("2. OUTPUT BUS ROUTING");
    tabOutputPatch.setClickingTogglesState(true);
    tabOutputPatch.setRadioGroupId(1001);
    tabOutputPatch.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffa000));
    tabOutputPatch.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    tabOutputPatch.onClick = [this] {
      isOutputMode = true;
      repaint();
    };
    addAndMakeVisible(tabOutputPatch);

    // Bank Navigation for Input Mode
    bankAllCh.setButtonText("ALL CH (1-32)");
    bankAllCh.setClickingTogglesState(true);
    bankAllCh.setRadioGroupId(1002);
    bankAllCh.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a4758));
    bankAllCh.onClick = [this] { chBank = 0; repaint(); };
    addAndMakeVisible(bankAllCh);

    bank1_16.setButtonText("CH 01-16");
    bank1_16.setClickingTogglesState(true);
    bank1_16.setRadioGroupId(1002);
    bank1_16.setToggleState(true, juce::dontSendNotification);
    bank1_16.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a4758));
    bank1_16.onClick = [this] { chBank = 1; repaint(); };
    addAndMakeVisible(bank1_16);

    bank17_32.setButtonText("CH 17-32");
    bank17_32.setClickingTogglesState(true);
    bank17_32.setRadioGroupId(1002);
    bank17_32.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a4758));
    bank17_32.onClick = [this] { chBank = 2; repaint(); };
    addAndMakeVisible(bank17_32);

    // Source Filter Buttons
    srcFilterAll.setButtonText("ALL (48)");
    srcFilterAll.setClickingTogglesState(true);
    srcFilterAll.setRadioGroupId(1003);
    srcFilterAll.setToggleState(true, juce::dontSendNotification);
    srcFilterAll.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2d3748));
    srcFilterAll.onClick = [this] { srcFilter = 0; repaint(); };
    addAndMakeVisible(srcFilterAll);

    srcFilterHw.setButtonText("HW (1-32)");
    srcFilterHw.setClickingTogglesState(true);
    srcFilterHw.setRadioGroupId(1003);
    srcFilterHw.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0077b6));
    srcFilterHw.onClick = [this] { srcFilter = 1; repaint(); };
    addAndMakeVisible(srcFilterHw);

    srcFilterVirt.setButtonText("VIRT (1-16)");
    srcFilterVirt.setClickingTogglesState(true);
    srcFilterVirt.setRadioGroupId(1003);
    srcFilterVirt.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2d6a4f));
    srcFilterVirt.onClick = [this] { srcFilter = 2; repaint(); };
    addAndMakeVisible(srcFilterVirt);

    // Global One-Touch Automation Tools
    btnAutoPatchHw.setButtonText("1:1 AUTO-PATCH HW");
    btnAutoPatchHw.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff023e8a));
    btnAutoPatchHw.onClick = [this] {
      engine.stageBoxPatchMatrix.autoPatchLocalInputs(0, 32);
      for (int c = 0; c < kLogicalChannels; ++c)
        engine.channelNames[static_cast<size_t>(c)] = engine.stageBoxPatchMatrix.getChannelAutoLabel(c);
      repaint();
    };
    addAndMakeVisible(btnAutoPatchHw);

    btnAutoPatchVirt.setButtonText("AUTO-PATCH VIRTUAL");
    btnAutoPatchVirt.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b4332));
    btnAutoPatchVirt.onClick = [this] {
      engine.stageBoxPatchMatrix.autoPatchVirtualAudio(16, 16);
      for (int c = 16; c < kLogicalChannels; ++c)
        engine.channelNames[static_cast<size_t>(c)] = engine.stageBoxPatchMatrix.getChannelAutoLabel(c);
      repaint();
    };
    addAndMakeVisible(btnAutoPatchVirt);

    btnAutoPatchOut.setButtonText("AUTO-PATCH MASTER/MON");
    btnAutoPatchOut.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd48b00));
    btnAutoPatchOut.onClick = [this] {
      engine.stageBoxPatchMatrix.autoPatchMasterAndMonitor();
      repaint();
    };
    addAndMakeVisible(btnAutoPatchOut);

    btnSyncLabels.setButtonText("AUTO-LABEL CHANNELS");
    btnSyncLabels.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2b2d42));
    btnSyncLabels.onClick = [this] {
      for (int c = 0; c < kLogicalChannels; ++c) {
        if (engine.stageBoxPatchMatrix.getInputPatch(c) >= 0)
          engine.channelNames[static_cast<size_t>(c)] = engine.stageBoxPatchMatrix.getChannelAutoLabel(c);
      }
      repaint();
    };
    addAndMakeVisible(btnSyncLabels);

    btnClearAll.setButtonText("DISCONNECT ALL");
    btnClearAll.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff780000));
    btnClearAll.onClick = [this] {
      engine.stageBoxPatchMatrix.reset();
      repaint();
    };
    addAndMakeVisible(btnClearAll);

    startTimerHz(15); // Smooth 15 Hz telemetry & hover repaint
  }

  ~StageBoxPatchPanel() override {
    stopTimer();
  }

  void timerCallback() override {
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    const bool isCompact = (bounds.getHeight() < 420.0f);
    // Console touring dark carbon background
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff0d1117), 0.0f, 0.0f,
        juce::Colour(0xff161b22), 0.0f, bounds.getHeight(), false));
    g.fillRect(bounds);

    // Top Header Banner
    const float headerH = isCompact ? 28.0f : 42.0f;
    auto headerArea = bounds.removeFromTop(headerH);
    g.setColour(juce::Colour(0xff1c2128));
    g.fillRect(headerArea);
    g.setColour(juce::Colour(0xff30363d));
    g.drawHorizontalLine(static_cast<int>(headerArea.getBottom()), 0.0f, bounds.getWidth());

    // Logo & Title
    g.setFont(juce::Font(juce::FontOptions(isCompact ? 12.5f : 15.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff58a6ff));
    g.drawText("OPENLIVE HD", 14, 0, isCompact ? 90 : 105, static_cast<int>(headerH), juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(isCompact ? 10.5f : 13.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfff0f6fc));
    g.drawText("/  ROUTING MATRIX & I/O PATCHBAY", isCompact ? 108 : 124, 0, 260, static_cast<int>(headerH), juce::Justification::centredLeft);

    // Clock Status Pill (actual engine sample rate)
    const double curSr = engine.sampleRate.load(std::memory_order_relaxed);
    const int hwInCount = engine.inputCount.load(std::memory_order_relaxed);
    const int hwOutCount = engine.outputCount.load(std::memory_order_relaxed);
    const bool srValid = (curSr > 1000.0);

    const auto clockArea = juce::Rectangle<float>(bounds.getWidth() - (isCompact ? 305.0f : 380.0f), isCompact ? 4.0f : 9.0f, isCompact ? 145.0f : 175.0f, isCompact ? 20.0f : 24.0f);
    g.setColour(srValid ? juce::Colour(0xff13231b) : juce::Colour(0xff231b13));
    g.fillRoundedRectangle(clockArea, 4.0f);
    g.setColour(srValid ? juce::Colour(0xff2ea44f) : juce::Colour(0xffeab308));
    g.drawRoundedRectangle(clockArea, 4.0f, 1.0f);
    g.fillEllipse(clockArea.getX() + 6.0f, clockArea.getY() + (isCompact ? 6.0f : 8.0f), isCompact ? 6.0f : 8.0f, isCompact ? 6.0f : 8.0f);
    g.setFont(juce::Font(juce::FontOptions(isCompact ? 9.0f : 10.5f, juce::Font::bold)));
    g.drawText(srValid ? (juce::String(curSr / 1000.0, 1) + " kHz  |  ACTIVE")
                       : "ENGINE IDLE",
               clockArea.getX() + (isCompact ? 16.0f : 22.0f), clockArea.getY(), clockArea.getWidth() - 18.0f, clockArea.getHeight(), juce::Justification::centredLeft);

    // Engine Load & Channel Capacity Pill (actual hardware I/O count)
    const auto capArea = juce::Rectangle<float>(bounds.getWidth() - (isCompact ? 154.0f : 198.0f), isCompact ? 4.0f : 9.0f, isCompact ? 146.0f : 186.0f, isCompact ? 20.0f : 24.0f);
    g.setColour(juce::Colour(0xff1f242c));
    g.fillRoundedRectangle(capArea, 4.0f);
    g.setColour(juce::Colour(0xff388bfd));
    g.drawRoundedRectangle(capArea, 4.0f, 1.0f);
    g.setColour(juce::Colour(0xff79c0ff));
    g.drawText(juce::String(kLogicalChannels) + " CH  |  " +
                   juce::String(hwInCount) + " IN  |  " +
                   juce::String(hwOutCount) + " OUT",
               capArea, juce::Justification::centred);

    // Draw Active Crosspoint Grid
    if (!isOutputMode)
      drawInputGrid(g);
    else
      drawOutputGrid(g);

    // Bottom Status & Telemetry Inspector
    drawInspectorBar(g);
  }

  void mouseMove(const juce::MouseEvent &e) override {
    hoverPos = e.position;
    repaint();
  }

  void mouseExit(const juce::MouseEvent &) override {
    hoverPos = {-1.0f, -1.0f};
    repaint();
  }

  void mouseDown(const juce::MouseEvent &e) override {
    dragStartPos = e.position;
    dragStartScrollX = scrollX;
    isHorizontalSliding = false;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (!gridBounds.contains(dragStartPos))
      return;
    const float dx = e.position.x - dragStartPos.x;
    if (!isHorizontalSliding && std::abs(dx) >= 6.0f)
      isHorizontalSliding = true;
    if (isHorizontalSliding && maxScrollX > 0.0f) {
      scrollX = std::clamp(dragStartScrollX - dx, 0.0f, maxScrollX);
      hoverPos = e.position;
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (isHorizontalSliding) {
      isHorizontalSliding = false;
      return;
    }
    if (!isOutputMode)
      handleInputGridClick(e.position);
    else
      handleOutputGridClick(e.position);
  }

  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &wheel) override {
    if (maxScrollX <= 0.0f)
      return;
    const float delta =
        (std::abs(wheel.deltaX) > std::abs(wheel.deltaY)) ? wheel.deltaX
                                                          : wheel.deltaY;
    scrollX = std::clamp(scrollX - delta * 140.0f, 0.0f, maxScrollX);
    repaint();
  }

  void resized() override {
    const bool isCompact = (getHeight() < 420);
    auto area = getLocalBounds().reduced(isCompact ? 4 : 8);
    const int headerH = isCompact ? 28 : 44;
    area.removeFromTop(headerH); // Header

    // Row 1: Mode Switchers & Bank Navigators
    const int rowH = isCompact ? 24 : 32;
    auto modeRow = area.removeFromTop(rowH);
    const int tabW = isCompact ? 220 : 330;
    tabInputPatch.setBounds(modeRow.removeFromLeft(tabW).reduced(1));
    tabOutputPatch.setBounds(modeRow.removeFromLeft(tabW).reduced(1));

    modeRow.removeFromLeft(isCompact ? 6 : 12);
    if (!isOutputMode) {
      bankAllCh.setBounds(modeRow.removeFromLeft(isCompact ? 68 : 95).reduced(1));
      bank1_16.setBounds(modeRow.removeFromLeft(isCompact ? 60 : 80).reduced(1));
      bank17_32.setBounds(modeRow.removeFromLeft(isCompact ? 60 : 80).reduced(1));

      modeRow.removeFromLeft(isCompact ? 6 : 12);
      srcFilterAll.setBounds(modeRow.removeFromLeft(isCompact ? 75 : 110).reduced(1));
      srcFilterHw.setBounds(modeRow.removeFromLeft(isCompact ? 75 : 105).reduced(1));
      srcFilterVirt.setBounds(modeRow.removeFromLeft(isCompact ? 75 : 110).reduced(1));
    }

    // Row 2: One-Touch Action Buttons
    area.removeFromTop(isCompact ? 2 : 4);
    auto actionRow = area.removeFromTop(isCompact ? 22 : 28);
    btnAutoPatchHw.setBounds(actionRow.removeFromLeft(isCompact ? 120 : 150).reduced(1));
    btnAutoPatchVirt.setBounds(actionRow.removeFromLeft(isCompact ? 130 : 160).reduced(1));
    btnAutoPatchOut.setBounds(actionRow.removeFromLeft(isCompact ? 140 : 180).reduced(1));
    btnSyncLabels.setBounds(actionRow.removeFromLeft(isCompact ? 120 : 160).reduced(1));
    btnClearAll.setBounds(actionRow.removeFromLeft(isCompact ? 110 : 140).reduced(1));

    // Remaining area reserved for grid and bottom inspector bar
    const int bottomH = isCompact ? 24 : 44;
    gridBounds = area.removeFromTop(area.getHeight() - bottomH).toFloat();
  }

private:
  // --- Input Crosspoint Grid Rendering ---
  void drawInputGrid(juce::Graphics &g) {
    if (gridBounds.getWidth() <= 100.0f || gridBounds.getHeight() <= 100.0f)
      return;

    // Filter displayed channels
    const int startCh = (chBank == 2) ? 16 : 0;
    const int numCh = (chBank == 0) ? 32 : 16;

    // Filter displayed sources
    std::vector<int> displayedSources;
    if (srcFilter == 0) { // All 48
      for (int i = 0; i < 48; ++i) displayedSources.push_back(i);
    } else if (srcFilter == 1) { // HW 1-32
      for (int i = 0; i < 32; ++i) displayedSources.push_back(i);
    } else { // Virtual 1-16
      for (int i = 32; i < 48; ++i) displayedSources.push_back(i);
    }

    const float labelColW = 120.0f;
    const float headerRowH = 34.0f;
    const float availableW = gridBounds.getWidth() - labelColW;
    const float minColW = 28.0f;
    const float totalColsW = minColW * static_cast<float>(displayedSources.size());
    const bool needsHScroll = (totalColsW > availableW + 1.0f);
    const float scrollBarH = needsHScroll ? 6.0f : 0.0f;
    const float availableH = gridBounds.getHeight() - headerRowH - scrollBarH;

    const float colW = needsHScroll ? minColW
                                    : (availableW / static_cast<float>(displayedSources.size()));
    const float rowH = std::min(26.0f, availableH / static_cast<float>(numCh));
    maxScrollX = needsHScroll ? std::max(0.0f, totalColsW - availableW) : 0.0f;
    scrollX = std::clamp(scrollX, 0.0f, maxScrollX);

    // Grid container frame
    g.setColour(juce::Colour(0xff12171f));
    g.fillRoundedRectangle(gridBounds, 10.0f);
    g.setColour(juce::Colour(0xff233146));
    g.drawRoundedRectangle(gridBounds, 10.0f, 1.0f);

    // Compute hovered cell (accounting for horizontal scroll)
    int hoveredColIdx = -1;
    int hoveredRowIdx = -1;
    if (gridBounds.contains(hoverPos)) {
      const float relX = hoverPos.x - (gridBounds.getX() + labelColW);
      const float relY = hoverPos.y - (gridBounds.getY() + headerRowH);
      if (relX >= 0.0f)
        hoveredColIdx = static_cast<int>((relX + scrollX) / colW);
      if (relY >= 0.0f && relY < availableH)
        hoveredRowIdx = static_cast<int>(relY / rowH);
      if (hoveredColIdx >= static_cast<int>(displayedSources.size())) hoveredColIdx = -1;
      if (hoveredRowIdx >= numCh) hoveredRowIdx = -1;
    }

    // 1. Scrollable Right Pane (Clipped to gridBounds minus frozen Channel column)
    const auto scrollClip = juce::Rectangle<float>(
        gridBounds.getX() + labelColW, gridBounds.getY(),
        availableW, gridBounds.getHeight() - scrollBarH);
    g.saveState();
    g.reduceClipRegion(scrollClip.toNearestInt());

    // Crosshair Guide Highlight
    if (hoveredColIdx >= 0 && hoveredRowIdx >= 0) {
      const float crossX = gridBounds.getX() + labelColW + hoveredColIdx * colW - scrollX;
      const float crossY = gridBounds.getY() + headerRowH + hoveredRowIdx * rowH;
      g.setColour(juce::Colour(0x1800e5ff));
      g.fillRect(crossX, gridBounds.getY(), colW, gridBounds.getHeight());
      g.fillRect(scrollClip.getX(), crossY, scrollClip.getWidth(), rowH);
    }

    // Draw Column Headers (Sources)
    for (size_t c = 0; c < displayedSources.size(); ++c) {
      const int globalPort = displayedSources[c];
      const bool isHw = (globalPort < 32);
      const int portNum = isHw ? (globalPort + 1) : (globalPort - 31);
      const float x = gridBounds.getX() + labelColW + static_cast<float>(c) * colW - scrollX;
      if (x + colW < scrollClip.getX() || x > scrollClip.getRight())
        continue;
      auto colHeader = juce::Rectangle<float>(x, gridBounds.getY(), colW, headerRowH);

      const bool isColHovered = (hoveredColIdx == static_cast<int>(c));
      g.setColour(isColHovered ? (isHw ? juce::Colour(0xff005f73) : juce::Colour(0xff1b4332))
                               : (isHw ? juce::Colour(0xff18222d) : juce::Colour(0xff16281e)));
      g.fillRect(colHeader.reduced(1.0f, 1.0f));

      g.setFont(juce::Font(juce::FontOptions(colW > 20.0f ? 9.5f : 8.0f, juce::Font::bold)));
      g.setColour(isHw ? juce::Colour(0xff00b4d8) : juce::Colour(0xff52b788));
      auto colTop = colHeader.removeFromTop(14.0f);
      g.drawText(isHw ? "HW" : "VR", colTop, juce::Justification::centred);
      g.setColour(isColHovered ? juce::Colours::white : juce::Colour(0xffc9d1d9));
      g.drawText(juce::String(portNum), colHeader, juce::Justification::centred);
    }

    // Draw Scrollable Crosspoint Cells
    for (int r = 0; r < numCh; ++r) {
      const int channelIndex = startCh + r;
      const float y = gridBounds.getY() + headerRowH + static_cast<float>(r) * rowH;
      const int patchedGlobalPort = engine.stageBoxPatchMatrix.getInputPatch(channelIndex);

      for (size_t c = 0; c < displayedSources.size(); ++c) {
        const float x = gridBounds.getX() + labelColW + static_cast<float>(c) * colW - scrollX;
        if (x + colW < scrollClip.getX() || x > scrollClip.getRight())
          continue;
        const int globalPort = displayedSources[c];
        const auto cellRect = juce::Rectangle<float>(x, y, colW, rowH).reduced(1.5f, 1.5f);

        const bool isPatched = (patchedGlobalPort == globalPort);
        const bool isCellHovered = (hoveredColIdx == static_cast<int>(c) && hoveredRowIdx == r);

        if (isPatched) {
          const bool isHw = (globalPort < 32);
          const juce::Colour ledColor = isHw ? juce::Colour(0xff00e5ff) : juce::Colour(0xff00e676);
          g.setColour(ledColor.withAlpha(0.25f));
          g.fillRoundedRectangle(cellRect.expanded(1.0f), 3.0f);
          g.setColour(ledColor);
          g.fillRoundedRectangle(cellRect, 2.5f);
          g.setColour(juce::Colours::white);
          g.fillEllipse(cellRect.getCentreX() - 2.5f, cellRect.getCentreY() - 2.5f, 5.0f, 5.0f);
        } else if (isCellHovered) {
          g.setColour(juce::Colour(0xff2d3748));
          g.fillRoundedRectangle(cellRect, 2.0f);
          g.setColour(juce::Colour(0xff58a6ff));
          g.drawRoundedRectangle(cellRect, 2.0f, 1.0f);
          g.drawText("+", cellRect, juce::Justification::centred);
        } else {
          g.setColour(juce::Colour(0xff161b22));
          g.fillRoundedRectangle(cellRect, 1.5f);
          g.setColour(juce::Colour(0xff21262d));
          g.drawRoundedRectangle(cellRect, 1.5f, 0.5f);
        }
      }
    }
    g.restoreState();

    // 2. Frozen Left Channel Column (Excel-style Freeze Pane)
    auto originRect = juce::Rectangle<float>(gridBounds.getX(), gridBounds.getY(), labelColW, headerRowH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(originRect);
    g.setColour(juce::Colour(0xff8b949e));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("DEST \\ SOURCE", originRect, juce::Justification::centred);

    for (int r = 0; r < numCh; ++r) {
      const int channelIndex = startCh + r;
      const float y = gridBounds.getY() + headerRowH + static_cast<float>(r) * rowH;
      const auto rowHeader = juce::Rectangle<float>(gridBounds.getX(), y, labelColW, rowH);

      const bool isRowHovered = (hoveredRowIdx == r);
      g.setColour(isRowHovered ? juce::Colour(0xff2d3748) : juce::Colour(0xff161b22));
      g.fillRect(rowHeader.reduced(1.0f, 0.5f));

      const float peak = (channelIndex < static_cast<int>(engine.channels.size()))
                             ? engine.channels[static_cast<size_t>(channelIndex)].peak.load(std::memory_order_relaxed)
                             : 0.0f;
      const bool hasSignal = (peak > 0.005f);
      g.setColour(hasSignal ? juce::Colour(0xff00ff66) : juce::Colour(0xff30363d));
      g.fillEllipse(rowHeader.getX() + 6.0f, y + rowH * 0.5f - 3.0f, 6.0f, 6.0f);

      g.setFont(juce::Font(juce::FontOptions(std::clamp(rowH * 0.65f, 7.0f, 10.0f), juce::Font::bold)));
      g.setColour(isRowHovered ? juce::Colour(0xff58a6ff) : juce::Colour(0xffc9d1d9));
      juce::String chText = "CH " + juce::String(channelIndex + 1);
      const auto customName = engine.channelNames[static_cast<size_t>(channelIndex)];
      if (customName.isNotEmpty())
        chText += " " + customName.substring(0, 7);
      g.drawText(chText, rowHeader.getX() + 16.0f, y, labelColW - 18.0f, rowH, juce::Justification::centredLeft);
    }

    // Freeze divider line
    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.55f));
    g.drawVerticalLine(static_cast<int>(gridBounds.getX() + labelColW),
                       gridBounds.getY(), gridBounds.getBottom());

    // 3. Horizontal Slide Scrollbar Indicator
    if (needsHScroll) {
      auto track = juce::Rectangle<float>(
          gridBounds.getX() + labelColW + 4.0f,
          gridBounds.getBottom() - 5.0f, availableW - 8.0f, 3.5f);
      g.setColour(juce::Colour(0xff1e293b));
      g.fillRoundedRectangle(track, 1.5f);
      const float thumbRatio = std::clamp(availableW / totalColsW, 0.1f, 1.0f);
      const float thumbW = track.getWidth() * thumbRatio;
      const float scrollFrac = (maxScrollX > 0.0f) ? (scrollX / maxScrollX) : 0.0f;
      const float thumbX = track.getX() + scrollFrac * (track.getWidth() - thumbW);
      g.setColour(juce::Colour(0xff38bdf8));
      g.fillRoundedRectangle(thumbX, track.getY(), thumbW, track.getHeight(), 1.5f);
    }
  }

  // --- Output Crosspoint Grid Rendering ---
  void drawOutputGrid(juce::Graphics &g) {
    if (gridBounds.getWidth() <= 100.0f || gridBounds.getHeight() <= 100.0f)
      return;

    // 16 Output Destination Ports: 1-8 Physical Hardware, 9-16 Virtual PC Audio
    static const char *destNames[16] = {
        "HW OUT 01 (MAIN PA L)", "HW OUT 02 (MAIN PA R)", "HW OUT 03 (IEM MON L)",
        "HW OUT 04 (IEM MON R)", "HW OUT 05 (AUX 5)",    "HW OUT 06 (AUX 6)",
        "HW OUT 07 (AUX 7)",     "HW OUT 08 (AUX 8)",    "VIRT OUT 01 (STREAM L)",
        "VIRT OUT 02 (STREAM R)","VIRT OUT 03 (BROADCAST)","VIRT OUT 04 (DAW REC)",
        "VIRT OUT 05 (MATRIX 1)","VIRT OUT 06 (MATRIX 2)","VIRT OUT 07 (SUB 3D)",
        "VIRT OUT 08 (SURROUND)"};

    // 16 Available Source Buses
    static const char *srcBusLabels[16] = {
        "MASTER BUS (L)", "MASTER BUS (R)", "MASTER MONO", "AUX 1 (MON L)",
        "AUX 2 (MON R)",  "AUX 3 (MIX)",    "AUX 4 (MIX)",  "AUX 5 (FX)",
        "MATRIX 1 (PA)",  "MATRIX 2 (PA)",  "MATRIX 3 (FILL)", "MATRIX 4 (SUB)",
        "DIRECT CH 01",   "DIRECT CH 02",   "DIRECT CH 03",    "DIRECT CH 04"};

    const float labelColW = 160.0f;
    const float headerRowH = 34.0f;
    const float availableW = gridBounds.getWidth() - labelColW;
    const float availableH = gridBounds.getHeight() - headerRowH;

    const float colW = availableW / 16.0f;
    const float rowH = availableH / 16.0f;

    // Grid container frame
    g.setColour(juce::Colour(0xff12171f));
    g.fillRoundedRectangle(gridBounds, 10.0f);
    g.setColour(juce::Colour(0xff233146));
    g.drawRoundedRectangle(gridBounds, 10.0f, 1.0f);

    // Origin Label
    auto originRect = juce::Rectangle<float>(gridBounds.getX(), gridBounds.getY(), labelColW, headerRowH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(originRect);
    g.setColour(juce::Colour(0xffffa000));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("SOURCE BUS \\ OUT PORT", originRect, juce::Justification::centred);

    // Compute hovered cell
    int hoveredColIdx = -1;
    int hoveredRowIdx = -1;
    if (gridBounds.contains(hoverPos)) {
      const float relX = hoverPos.x - (gridBounds.getX() + labelColW);
      const float relY = hoverPos.y - (gridBounds.getY() + headerRowH);
      if (relX >= 0.0f) hoveredColIdx = static_cast<int>(relX / colW);
      if (relY >= 0.0f) hoveredRowIdx = static_cast<int>(relY / rowH);
      if (hoveredColIdx >= 16) hoveredColIdx = -1;
      if (hoveredRowIdx >= 16) hoveredRowIdx = -1;
    }

    // Crosshair Guide
    if (hoveredColIdx >= 0 && hoveredRowIdx >= 0) {
      const float crossX = gridBounds.getX() + labelColW + hoveredColIdx * colW;
      const float crossY = gridBounds.getY() + headerRowH + hoveredRowIdx * rowH;
      g.setColour(juce::Colour(0x18ffa000));
      g.fillRect(crossX, gridBounds.getY(), colW, gridBounds.getHeight());
      g.fillRect(gridBounds.getX(), crossY, gridBounds.getWidth(), rowH);
    }

    // Draw Column Headers (16 Output Ports)
    for (int p = 0; p < 16; ++p) {
      const float x = gridBounds.getX() + labelColW + p * colW;
      auto colHeader = juce::Rectangle<float>(x, gridBounds.getY(), colW, headerRowH);
      const bool isHw = (p < 8);
      const bool isColHovered = (hoveredColIdx == p);

      g.setColour(isColHovered ? (isHw ? juce::Colour(0xff7a4f01) : juce::Colour(0xff4a154b))
                               : (isHw ? juce::Colour(0xff2d2005) : juce::Colour(0xff250b29)));
      g.fillRect(colHeader.reduced(1.0f, 1.0f));

      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      g.setColour(isHw ? juce::Colour(0xffffb703) : juce::Colour(0xffe0aaff));
      auto colTop = colHeader.removeFromTop(14.0f);
      g.drawText(isHw ? "HW" : "VR", colTop, juce::Justification::centred);
      g.setColour(isColHovered ? juce::Colours::white : juce::Colour(0xffc9d1d9));
      g.drawText(juce::String(isHw ? (p + 1) : (p - 7)), colHeader, juce::Justification::centred);
    }

    // Draw Rows (16 Source Buses)
    for (int r = 0; r < 16; ++r) {
      const float y = gridBounds.getY() + headerRowH + r * rowH;
      const auto rowHeader = juce::Rectangle<float>(gridBounds.getX(), y, labelColW, rowH);
      const bool isRowHovered = (hoveredRowIdx == r);

      g.setColour(isRowHovered ? juce::Colour(0xff2d3748) : juce::Colour(0xff161b22));
      g.fillRect(rowHeader.reduced(1.0f, 0.5f));

      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.setColour(isRowHovered ? juce::Colour(0xffffa000) : juce::Colour(0xffc9d1d9));
      g.drawText(srcBusLabels[r], rowHeader.getX() + 10.0f, y, labelColW - 12.0f, rowH, juce::Justification::centredLeft);

      // Check current routing for output port
      for (int p = 0; p < 16; ++p) {
        nova::graviton::OutputBusSourceType bType;
        int bIdx = 0;
        engine.stageBoxPatchMatrix.getOutputPatch(p, bType, bIdx);

        // Map busType to row index
        int activeRowForPort = -1;
        if (bType == nova::graviton::OutputBusSourceType::MainLeft) activeRowForPort = 0;
        else if (bType == nova::graviton::OutputBusSourceType::MainRight) activeRowForPort = 1;
        else if (bType == nova::graviton::OutputBusSourceType::Mono) activeRowForPort = 2;
        else if (bType == nova::graviton::OutputBusSourceType::AuxBus) activeRowForPort = 3 + bIdx;
        else if (bType == nova::graviton::OutputBusSourceType::MatrixBus) activeRowForPort = 8 + bIdx;
        else if (bType == nova::graviton::OutputBusSourceType::DirectOut) activeRowForPort = 12 + bIdx;

        const float x = gridBounds.getX() + labelColW + p * colW;
        const auto cellRect = juce::Rectangle<float>(x, y, colW, rowH).reduced(1.5f, 1.5f);
        const bool isPatched = (activeRowForPort == r);
        const bool isCellHovered = (hoveredColIdx == p && hoveredRowIdx == r);

        if (isPatched) {
          const juce::Colour ledColor = (r <= 2) ? juce::Colour(0xffffab00)
                                                 : (r <= 7) ? juce::Colour(0xffd500f9)
                                                            : juce::Colour(0xff00e5ff);
          g.setColour(ledColor.withAlpha(0.25f));
          g.fillRoundedRectangle(cellRect.expanded(1.0f), 3.0f);
          g.setColour(ledColor);
          g.fillRoundedRectangle(cellRect, 2.5f);
          g.setColour(juce::Colours::white);
          g.fillEllipse(cellRect.getCentreX() - 2.5f, cellRect.getCentreY() - 2.5f, 5.0f, 5.0f);
        } else {
          if (isCellHovered) {
            g.setColour(juce::Colour(0xff2d3748));
            g.fillRoundedRectangle(cellRect, 2.0f);
            g.setColour(juce::Colour(0xffffa000));
            g.drawRoundedRectangle(cellRect, 2.0f, 1.0f);
          } else {
            g.setColour(juce::Colour(0xff161b22));
            g.fillRoundedRectangle(cellRect, 1.5f);
            g.setColour(juce::Colour(0xff21262d));
            g.drawRoundedRectangle(cellRect, 1.5f, 0.5f);
          }
        }
      }
    }
  }

  // --- Click Crosspoint Handlers ---
  void handleInputGridClick(juce::Point<float> pos) {
    if (!gridBounds.contains(pos)) return;

    const int startCh = (chBank == 2) ? 16 : 0;
    const int numCh = (chBank == 0) ? 32 : 16;
    std::vector<int> displayedSources;
    if (srcFilter == 0) for (int i = 0; i < 48; ++i) displayedSources.push_back(i);
    else if (srcFilter == 1) for (int i = 0; i < 32; ++i) displayedSources.push_back(i);
    else for (int i = 32; i < 48; ++i) displayedSources.push_back(i);

    const float labelColW = 120.0f;
    const float headerRowH = 34.0f;
    const float availableW = gridBounds.getWidth() - labelColW;
    const float minColW = 28.0f;
    const float totalColsW = minColW * static_cast<float>(displayedSources.size());
    const bool needsHScroll = (totalColsW > availableW + 1.0f);
    const float scrollBarH = needsHScroll ? 6.0f : 0.0f;
    const float availableH = gridBounds.getHeight() - headerRowH - scrollBarH;
    const float colW = needsHScroll ? minColW
                                    : (availableW / static_cast<float>(displayedSources.size()));
    const float rowH = std::min(26.0f, availableH / static_cast<float>(numCh));

    const float relX = pos.x - (gridBounds.getX() + labelColW);
    const float relY = pos.y - (gridBounds.getY() + headerRowH);
    if (relX < 0.0f || relY < 0.0f || relY >= availableH) return;

    const int col = static_cast<int>((relX + scrollX) / colW);
    const int row = static_cast<int>(relY / rowH);
    if (col >= 0 && col < static_cast<int>(displayedSources.size()) && row >= 0 && row < numCh) {
      const int ch = startCh + row;
      const int globalPort = displayedSources[static_cast<size_t>(col)];
      const int currentPatch = engine.stageBoxPatchMatrix.getInputPatch(ch);

      // Toggle connection (click active = disconnect, click empty = connect)
      if (currentPatch == globalPort) {
        engine.stageBoxPatchMatrix.patchInput(ch, -1);
      } else {
        engine.stageBoxPatchMatrix.patchInput(ch, globalPort);
        engine.channelNames[static_cast<size_t>(ch)] = engine.stageBoxPatchMatrix.getChannelAutoLabel(ch);
      }
      repaint();
    }
  }

  void handleOutputGridClick(juce::Point<float> pos) {
    if (!gridBounds.contains(pos)) return;

    const float labelColW = 160.0f;
    const float headerRowH = 34.0f;
    const float colW = (gridBounds.getWidth() - labelColW) / 16.0f;
    const float rowH = (gridBounds.getHeight() - headerRowH) / 16.0f;

    const float relX = pos.x - (gridBounds.getX() + labelColW);
    const float relY = pos.y - (gridBounds.getY() + headerRowH);
    if (relX < 0.0f || relY < 0.0f) return;

    const int port = static_cast<int>(relX / colW);
    const int row = static_cast<int>(relY / rowH);

    if (port >= 0 && port < 16 && row >= 0 && row < 16) {
      nova::graviton::OutputBusSourceType bType = nova::graviton::OutputBusSourceType::Unassigned;
      int bIdx = 0;

      if (row == 0) { bType = nova::graviton::OutputBusSourceType::MainLeft; bIdx = 0; }
      else if (row == 1) { bType = nova::graviton::OutputBusSourceType::MainRight; bIdx = 0; }
      else if (row == 2) { bType = nova::graviton::OutputBusSourceType::Mono; bIdx = 0; }
      else if (row >= 3 && row <= 7) { bType = nova::graviton::OutputBusSourceType::AuxBus; bIdx = row - 3; }
      else if (row >= 8 && row <= 11) { bType = nova::graviton::OutputBusSourceType::MatrixBus; bIdx = row - 8; }
      else if (row >= 12 && row <= 15) { bType = nova::graviton::OutputBusSourceType::DirectOut; bIdx = row - 12; }

      nova::graviton::OutputBusSourceType curType;
      int curIdx = 0;
      engine.stageBoxPatchMatrix.getOutputPatch(port, curType, curIdx);

      // Toggle patch
      if (curType == bType && curIdx == bIdx) {
        engine.stageBoxPatchMatrix.patchOutput(port, nova::graviton::OutputBusSourceType::Unassigned, 0);
      } else {
        engine.stageBoxPatchMatrix.patchOutput(port, bType, bIdx);
      }
      repaint();
    }
  }

  // --- Bottom Detail & Telemetry Inspector Bar ---
  void drawInspectorBar(juce::Graphics &g) {
    const bool isCompact = (getLocalBounds().getHeight() < 420);
    const float barH = isCompact ? 20.0f : 28.0f;
    const float barY = getLocalBounds().getHeight() - (isCompact ? 22.0f : 36.0f);
    auto barArea = juce::Rectangle<float>(isCompact ? 4.0f : 8.0f, barY, getLocalBounds().getWidth() - (isCompact ? 8.0f : 16.0f), barH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(barArea, 4.0f);
    g.setColour(juce::Colour(0xff30363d));
    g.drawRoundedRectangle(barArea, 4.0f, 1.0f);

    juce::String detailText;
    if (!isOutputMode) {
      int patchedCount = 0;
      for (int c = 0; c < kLogicalChannels; ++c)
        if (engine.stageBoxPatchMatrix.getInputPatch(c) >= 0) patchedCount++;
      detailText = "INPUT STATUS: " + juce::String(patchedCount) + " / " +
                   juce::String(kLogicalChannels) + " CHANNELS PATCHED" +
                   (patchedCount == 0 ? " (DIRECT INPUT FALLBACK ACTIVE)" : "") +
                   "  |  CLICK CELL TO TOGGLE  |  ZERO LATENCY LOCK-FREE";
    } else {
      int outCount = 0;
      for (size_t p = 0; p < 16; ++p) {
        nova::graviton::OutputBusSourceType bType;
        int bIdx = 0;
        engine.stageBoxPatchMatrix.getOutputPatch(static_cast<int>(p), bType, bIdx);
        if (bType != nova::graviton::OutputBusSourceType::Unassigned) outCount++;
      }
      detailText = "OUTPUT STATUS: " + juce::String(outCount) + " / 16 PORTS ROUTED  |  HW OUT 1-2 (MASTER PA), HW OUT 3-4 (AUX 1-2 IEM MONITOR)";
    }

    g.setFont(juce::Font(juce::FontOptions(isCompact ? 9.5f : 11.0f, juce::Font::bold)));
    g.setColour(isOutputMode ? juce::Colour(0xffffb703) : juce::Colour(0xff58a6ff));
    g.drawText(detailText, barArea.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
  }

  MixerEngine &engine;
  bool isOutputMode = false;
  int chBank = 1;      // 0 = All 32, 1 = 1-16, 2 = 17-32
  int srcFilter = 0;   // 0 = All 48, 1 = HW 1-32, 2 = Virtual 1-16
  juce::Point<float> hoverPos{-1.0f, -1.0f};
  juce::Rectangle<float> gridBounds;
  float scrollX = 0.0f;
  float maxScrollX = 0.0f;
  juce::Point<float> dragStartPos{-1.0f, -1.0f};
  float dragStartScrollX = 0.0f;
  bool isHorizontalSliding = false;

  juce::TextButton tabInputPatch, tabOutputPatch;
  juce::TextButton bankAllCh, bank1_16, bank17_32;
  juce::TextButton srcFilterAll, srcFilterHw, srcFilterVirt;
  juce::TextButton btnAutoPatchHw, btnAutoPatchVirt, btnAutoPatchOut, btnSyncLabels, btnClearAll;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StageBoxPatchPanel)
};

} // namespace nova
