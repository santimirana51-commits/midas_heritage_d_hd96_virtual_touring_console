import sys
import re

path = 'Source/GUI/MidasTouringConsoleSurface.h'
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# 1. Update MidasConsoleStrip private members
target_cs_priv = """private:
  int slot = 0;
  bool isSelected = false;"""
replace_cs_priv = """private:
  int slot = 0;
  bool isSelected = false;
  bool isSofMonitorMode = false;
  bool isPreFader = false;"""
if target_cs_priv in content:
    content = content.replace(target_cs_priv, replace_cs_priv, 1)
    print("Updated MidasConsoleStrip private members")
else:
    print("WARN: target_cs_priv not found")

# 2. Update MidasConsoleStrip paint method
target_cs_paint = """    if (isSelected) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.35f));
      g.drawRoundedRectangle(cardRect.expanded(1.0f), 8.5f, 2.5f);
      g.setColour(midas_colors::kAccentCyan);
      g.drawRoundedRectangle(cardRect, 8.0f, 2.0f);
    } else {
      g.setColour(midas_colors::kBorderCol.withAlpha(0.7f));
      g.drawRoundedRectangle(cardRect, 8.0f, 1.2f);
    }"""
replace_cs_paint = """    if (isSelected) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.35f));
      g.drawRoundedRectangle(cardRect.expanded(1.0f), 8.5f, 2.5f);
      g.setColour(midas_colors::kAccentCyan);
      g.drawRoundedRectangle(cardRect, 8.0f, 2.0f);
    } else if (isSofMonitorMode) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.6f));
      g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);
    } else {
      g.setColour(midas_colors::kBorderCol.withAlpha(0.7f));
      g.drawRoundedRectangle(cardRect, 8.0f, 1.2f);
    }"""
if target_cs_paint in content:
    content = content.replace(target_cs_paint, replace_cs_paint, 1)
    print("Updated MidasConsoleStrip paint card border")
else:
    print("WARN: target_cs_paint not found")

# Update MidasConsoleStrip OLED rendering
target_cs_oled = """    // Line 1: Category Badge (e.g. "DRUMS 1", "BASS 4", "GUITARS 5")
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(accentColor);
    g.drawText(chTag, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 6.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);

    // Line 2: Channel Name (e.g. "KICK IN", "SNARE TOP", "BASS DI")
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(juce::Colours::white);
    g.drawText(chName, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 18.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 14,
               juce::Justification::centred);

    // Line 3: DSP Subtitle (e.g. "VCA COMP | MIDAS ...", "FET 76 | BRITISH ...")
    g.setFont(midas_ui::getFont(7.5f, false));
    g.setColour(midas_colors::kAccentGold);
    g.drawText(chBadge, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 32.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);"""

replace_cs_oled = """    const auto stripAccent = isSofMonitorMode ? midas_colors::kAccentCyan : accentColor;
    // Line 1: Category Badge (e.g. "DRUMS 1", or "MON PRE" / "MON POST" during SOF)
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(stripAccent);
    const juce::String displayTag = isSofMonitorMode ? (isPreFader ? "MON PRE" : "MON POST") : chTag;
    g.drawText(displayTag, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 6.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);

    // Line 2: Channel Name (e.g. "KICK IN", "SNARE TOP", "BASS DI")
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(juce::Colours::white);
    g.drawText(chName, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 18.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 14,
               juce::Justification::centred);

    // Line 3: DSP Subtitle (e.g. "VCA COMP | MIDAS ...", or "AUX MON SEND" during SOF)
    g.setFont(midas_ui::getFont(7.5f, false));
    g.setColour(isSofMonitorMode ? midas_colors::kAccentCyan : midas_colors::kAccentGold);
    const juce::String displayBadge = isSofMonitorMode ? "AUX MON SEND" : chBadge;
    g.drawText(displayBadge, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 32.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11,
               juce::Justification::centred);"""
if target_cs_oled in content:
    content = content.replace(target_cs_oled, replace_cs_oled, 1)
    print("Updated MidasConsoleStrip OLED rendering")
else:
    print("WARN: target_cs_oled not found")

# Fix dbStr infinity encoding in MidasConsoleStrip
content = re.sub(
    r'if \(curDb <= -89\.0f\)\s+dbStr = "[^"]+";',
    'if (curDb <= -89.0f)\n      dbStr = "-inf dB";',
    content
)
print("Sanitized dbStr in content")

# 3. Update MidasMasterStrip paint method (dual mode: Master vs Monitor)
target_mst_paint = """    // 1. Card Background & Golden Border
    auto cardRect = area.reduced(2.0f);
    g.setColour(midas_colors::kStripBg);
    g.fillRoundedRectangle(cardRect, 8.0f);

    g.setColour(midas_colors::kAccentGold);
    g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);

    // 2. Crown Icon + MASTER L/R Header
    const float crownX = (w - 86.0f) * 0.5f;
    const float crownY = 6.0f;
    juce::Path crown;
    crown.startNewSubPath(crownX, crownY + 8.0f);
    crown.lineTo(crownX + 2.0f, crownY + 2.0f);
    crown.lineTo(crownX + 5.0f, crownY + 6.0f);
    crown.lineTo(crownX + 8.0f, crownY);
    crown.lineTo(crownX + 11.0f, crownY + 6.0f);
    crown.lineTo(crownX + 14.0f, crownY + 2.0f);
    crown.lineTo(crownX + 16.0f, crownY + 8.0f);
    crown.closeSubPath();
    g.setColour(midas_colors::kAccentGold);
    g.fillPath(crown);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MASTER L/R", static_cast<int>(crownX + 18.0f), 3, 72, 14, juce::Justification::centredLeft);

    // 3. Balance Readout Text
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(juce::Colour(0xff7fa0c0));
    g.drawText("BALANCE", 0, 48, static_cast<int>(w), 12, juce::Justification::centred);

    // 4. Master Scribble Strip Screen
    const float oledY = 64.0f;
    const float oledH = 54.0f;
    auto oledRect = juce::Rectangle<float>(6.0f, oledY, w - 12.0f, oledH);

    g.setColour(juce::Colour(0xff03060a));
    g.fillRoundedRectangle(oledRect, 4.0f);
    g.setColour(midas_colors::kAccentGold);
    g.drawRoundedRectangle(oledRect, 4.0f, 1.2f);
    g.fillRect(oledRect.getX() + 2.0f, oledRect.getY() + 2.0f,
               oledRect.getWidth() - 4.0f, 2.5f);

    // Line 1: "STEREO BUS"
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(midas_colors::kAccentGold.withAlpha(0.85f));
    g.drawText("STEREO BUS", static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 6.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11, juce::Justification::centred);

    // Line 2: "MAIN OUT"
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MAIN OUT", static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 19.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 14, juce::Justification::centred);"""

replace_mst_paint = """    // 1. Card Background & Accent Border (Gold for Master, Cyan for Monitor)
    auto cardRect = area.reduced(2.0f);
    g.setColour(midas_colors::kStripBg);
    g.fillRoundedRectangle(cardRect, 8.0f);

    const auto accentCol = isMonitorMode ? midas_colors::kAccentCyan : midas_colors::kAccentGold;
    g.setColour(accentCol);
    g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);

    // 2. Header Icon + Title
    if (isMonitorMode) {
      const float hpX = (w - 92.0f) * 0.5f;
      const float hpY = 5.0f;
      g.setColour(midas_colors::kAccentCyan);
      juce::Path hp;
      hp.addCentredArc(hpX + 7.0f, hpY + 7.0f, 6.0f, 6.0f, 0.0f,
                       -juce::MathConstants<float>::halfPi - 0.6f,
                       juce::MathConstants<float>::halfPi + 0.6f, true);
      g.strokePath(hp, juce::PathStrokeType(1.8f));
      g.fillRoundedRectangle(hpX + 1.0f, hpY + 5.5f, 3.0f, 6.5f, 1.2f);
      g.fillRoundedRectangle(hpX + 10.0f, hpY + 5.5f, 3.0f, 6.5f, 1.2f);

      g.setFont(midas_ui::getFont(9.5f, true));
      g.setColour(midas_colors::kAccentCyan);
      g.drawText("MONITOR L/R", static_cast<int>(hpX + 18.0f), 3, 76, 14, juce::Justification::centredLeft);
    } else {
      const float crownX = (w - 86.0f) * 0.5f;
      const float crownY = 6.0f;
      juce::Path crown;
      crown.startNewSubPath(crownX, crownY + 8.0f);
      crown.lineTo(crownX + 2.0f, crownY + 2.0f);
      crown.lineTo(crownX + 5.0f, crownY + 6.0f);
      crown.lineTo(crownX + 8.0f, crownY);
      crown.lineTo(crownX + 11.0f, crownY + 6.0f);
      crown.lineTo(crownX + 14.0f, crownY + 2.0f);
      crown.lineTo(crownX + 16.0f, crownY + 8.0f);
      crown.closeSubPath();
      g.setColour(midas_colors::kAccentGold);
      g.fillPath(crown);

      g.setFont(midas_ui::getFont(9.5f, true));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("MASTER L/R", static_cast<int>(crownX + 18.0f), 3, 72, 14, juce::Justification::centredLeft);
    }

    // 3. Balance Readout Text
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(juce::Colour(0xff7fa0c0));
    g.drawText(isMonitorMode ? "MON BAL" : "BALANCE", 0, 48, static_cast<int>(w), 12, juce::Justification::centred);

    // 4. Master Scribble Strip Screen
    const float oledY = 64.0f;
    const float oledH = 54.0f;
    auto oledRect = juce::Rectangle<float>(6.0f, oledY, w - 12.0f, oledH);

    g.setColour(juce::Colour(0xff03060a));
    g.fillRoundedRectangle(oledRect, 4.0f);
    g.setColour(accentCol);
    g.drawRoundedRectangle(oledRect, 4.0f, 1.2f);
    g.fillRect(oledRect.getX() + 2.0f, oledRect.getY() + 2.0f,
               oledRect.getWidth() - 4.0f, 2.5f);

    // Line 1:
    g.setFont(midas_ui::getFont(8.0f, true));
    g.setColour(accentCol.withAlpha(0.85f));
    g.drawText(isMonitorMode ? "AUX MONITOR" : "STEREO BUS", static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 6.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 11, juce::Justification::centred);

    // Line 2:
    g.setFont(midas_ui::getFont(11.0f, true));
    g.setColour(accentCol);
    g.drawText(isMonitorMode ? "MONITOR OUT" : "MAIN OUT", static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 19.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 14, juce::Justification::centred);"""
if target_mst_paint in content:
    content = content.replace(target_mst_paint, replace_mst_paint, 1)
    print("Updated MidasMasterStrip paint")
else:
    print("WARN: target_mst_paint not found")

# Update MidasMasterStrip btnMainLr.onClick so it doesn't self-toggle mode
target_mst_btn = """    btnMainLr.onClick = [this] {
      setMonitorMode(!isMonitorMode);
      if (onModeToggled)
        onModeToggled(isMonitorMode);
    };"""
replace_mst_btn = """    btnMainLr.onClick = [this] {
      if (onModeToggled)
        onModeToggled(isMonitorMode);
    };"""
if target_mst_btn in content:
    content = content.replace(target_mst_btn, replace_mst_btn, 1)
    print("Updated MidasMasterStrip btnMainLr click")
else:
    print("WARN: target_mst_btn not found")

# 4. Update MidasTopStatusBar SOF button and 3-way mode
target_sof_btn = """    btnSof.onClick = [this] {
      const bool on = btnSof.getToggleState();
      btnSof.setButtonText(on ? "SOF: MIX 1-2" : "SOF: OFF");
      if (onSofToggled)
        onSofToggled(on);
    };"""
replace_sof_btn = """    btnSof.onClick = [this] {
      sofMode = (sofMode + 1) % 3;
      if (sofMode == 1) {
        btnSof.setToggleState(true, juce::dontSendNotification);
        btnSof.setButtonText("SOF: MONITOR");
        btnSof.setGlowMode(MidasTactileButton::GlowMode::Cyan);
      } else if (sofMode == 2) {
        btnSof.setToggleState(true, juce::dontSendNotification);
        btnSof.setButtonText("SOF: MIX 1-2");
        btnSof.setGlowMode(MidasTactileButton::GlowMode::Purple);
      } else {
        btnSof.setToggleState(false, juce::dontSendNotification);
        btnSof.setButtonText("SOF: OFF");
      }
      if (onSofModeChanged)
        onSofModeChanged(sofMode);
      if (onSofToggled)
        onSofToggled(sofMode != 0);
    };"""
if target_sof_btn in content:
    content = content.replace(target_sof_btn, replace_sof_btn, 1)
    print("Updated MidasTopStatusBar SOF button")
else:
    print("WARN: target_sof_btn not found")

# Add onSofModeChanged and sofMode in MidasTopStatusBar
target_stat_cb = """  std::function<void(bool sofActive)> onSofToggled;"""
replace_stat_cb = """  std::function<void(bool sofActive)> onSofToggled;
  std::function<void(int mode)> onSofModeChanged;
  int sofMode = 0;"""
if target_stat_cb in content:
    content = content.replace(target_stat_cb, replace_stat_cb, 1)
    print("Updated MidasTopStatusBar callbacks")
else:
    print("WARN: target_stat_cb not found")

# 5. Add monitorStrip to MidasTouringConsoleSurface members
target_members = """  std::array<std::unique_ptr<MidasConsoleStrip>, 8> strips;
  MidasMasterStrip masterStrip;
  MidasMasterProcessingPanel masterProcessing;"""
replace_members = """  std::array<std::unique_ptr<MidasConsoleStrip>, 8> strips;
  MidasMasterStrip masterStrip;
  MidasMasterStrip monitorStrip;
  MidasMasterProcessingPanel masterProcessing;
  int currentSofMode = 0;"""
if target_members in content:
    content = content.replace(target_members, replace_members, 1)
    print("Added monitorStrip member to MidasTouringConsoleSurface")
else:
    print("WARN: target_members not found")

# 6. Add monitorStrip to constructor
target_ctor_add = """    addAndMakeVisible(masterStrip);
    addAndMakeVisible(masterProcessing);"""
replace_ctor_add = """    addAndMakeVisible(masterStrip);
    addAndMakeVisible(monitorStrip);
    addAndMakeVisible(masterProcessing);"""
if target_ctor_add in content:
    content = content.replace(target_ctor_add, replace_ctor_add, 1)
    print("Added monitorStrip to constructor addAndMakeVisible")
else:
    print("WARN: target_ctor_add not found")

# 7. Update resized() for dual side-by-side strips (Master and Monitor)
target_resized = """    const float masterW = 120.0f;
    const float stripsAreaW = w - 16.0f - masterW - 10.0f;
    const float stripW = (stripsAreaW - 7.0f * 4.0f) / 8.0f;

    for (int i = 0; i < 8; ++i) {
      const float sx = 8.0f + i * (stripW + 4.0f);
      strips[static_cast<size_t>(i)]->setBounds(
          static_cast<int>(sx), static_cast<int>(lowerY),
          static_cast<int>(stripW), static_cast<int>(lowerH));
    }

    masterStrip.setBounds(static_cast<int>(w - 8.0f - masterW),
                          static_cast<int>(lowerY), static_cast<int>(masterW),
                          static_cast<int>(lowerH));"""

replace_resized = """    const float masterW = 108.0f;
    const float masterBayGap = 6.0f;
    const float totalMasterBayW = masterW * 2.0f + masterBayGap;
    const float stripsAreaW = w - 16.0f - totalMasterBayW - 10.0f;
    const float stripW = (stripsAreaW - 7.0f * 4.0f) / 8.0f;

    for (int i = 0; i < 8; ++i) {
      const float sx = 8.0f + i * (stripW + 4.0f);
      strips[static_cast<size_t>(i)]->setBounds(
          static_cast<int>(sx), static_cast<int>(lowerY),
          static_cast<int>(stripW), static_cast<int>(lowerH));
    }

    const float masterX = w - 8.0f - totalMasterBayW;
    const float monitorX = masterX + masterW + masterBayGap;

    masterStrip.setBounds(static_cast<int>(masterX),
                          static_cast<int>(lowerY), static_cast<int>(masterW),
                          static_cast<int>(lowerH));

    monitorStrip.setBounds(static_cast<int>(monitorX),
                           static_cast<int>(lowerY), static_cast<int>(masterW),
                           static_cast<int>(lowerH));"""
if target_resized in content:
    content = content.replace(target_resized, replace_resized, 1)
    print("Updated resized() for dual side-by-side strips")
else:
    print("WARN: target_resized not found")

# 8. Configure monitorStrip event bindings and SOF mode handling
target_bindings = """    // Master Strip & Master Processing Panel bindings (Fase 4)
    masterStrip.onMasterFaderMoved = [this](float db) {
      savedMasterDb = db;
      if (!masterMutedState)
        engine.masterDb.store(masterDimState ? (db - 20.0f) : db,
                              std::memory_order_relaxed);
    };"""

replace_bindings = """    // Master Strip & Monitor Strip bindings (Fase 4 - Dual Physical & Surface Output)
    masterStrip.setMonitorMode(false);
    masterStrip.onMasterFaderMoved = [this](float db) {
      savedMasterDb = db;
      if (!masterMutedState)
        engine.masterDb.store(masterDimState ? (db - 20.0f) : db,
                              std::memory_order_relaxed);
    };

    monitorStrip.setMonitorMode(true);
    monitorStrip.setFaderDb(engine.monitorLevelA.load(std::memory_order_relaxed));
    monitorStrip.setMuted(engine.monitorMuteA.load(std::memory_order_relaxed));
    monitorStrip.setDim(engine.monitorDimA.load(std::memory_order_relaxed));

    monitorStrip.onMasterFaderMoved = [this](float db) {
      engine.monitorLevelA.store(db, std::memory_order_relaxed);
    };
    monitorStrip.onMasterMuteChanged = [this](bool muted) {
      engine.monitorMuteA.store(muted, std::memory_order_relaxed);
    };
    monitorStrip.onDimToggled = [this](bool dim) {
      engine.monitorDimA.store(dim, std::memory_order_relaxed);
      engine.monitorDimB.store(dim, std::memory_order_relaxed);
    };"""
if target_bindings in content:
    content = content.replace(target_bindings, replace_bindings, 1)
    print("Configured monitorStrip bindings in constructor")
else:
    print("WARN: target_bindings not found")

# 9. Update statusBar.onSofModeChanged in MidasTouringConsoleSurface
target_sof_hook = """    statusBar.onSofToggled = [this](bool active) {
      sofActive = active;
      if (sofActive) {
        rebankStrips(2);
      } else {
        rebankStrips(currentBank);
      }
      repaint();
    };"""

replace_sof_hook = """    statusBar.onSofModeChanged = [this](int mode) {
      currentSofMode = mode;
      sofActive = (mode != 0);
      if (mode == 1) { // SOF: MONITOR
        for (int i = 0; i < 8; ++i) {
          const int chIdx = getEngineChannelForSlot(i);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
            strips[static_cast<size_t>(i)]->setSofMonitorMode(true, ch.monitorPreFader.load(std::memory_order_relaxed));
            strips[static_cast<size_t>(i)]->syncBusOrVcaState(
                ch.monitorSendDb.load(std::memory_order_relaxed),
                !ch.monitorAssigned.load(std::memory_order_relaxed),
                ch.peak.load(std::memory_order_relaxed));
          }
        }
      } else if (mode == 2) { // SOF: AUX 1-2
        for (int i = 0; i < 8; ++i) {
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        }
        rebankStrips(2);
      } else { // SOF: OFF
        for (int i = 0; i < 8; ++i) {
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        }
        rebankStrips(currentBank);
      }
      repaint();
    };
    statusBar.onSofToggled = [this](bool active) {
      sofActive = active;
      if (!sofActive) {
        currentSofMode = 0;
        for (int i = 0; i < 8; ++i) {
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        }
        rebankStrips(currentBank);
      }
      repaint();
    };"""
if target_sof_hook in content:
    content = content.replace(target_sof_hook, replace_sof_hook, 1)
    print("Updated statusBar.onSofModeChanged handling")
else:
    print("WARN: target_sof_hook not found")

# 10. Update onFaderChanged for SOF: MONITOR
target_fader_hook = """      strips[static_cast<size_t>(i)]->onFaderChanged = [this](int slot,
                                                              float db) {
        faderGlideMode = 0; // Manual touch overrides automation glide
        if (sofActive) {"""

replace_fader_hook = """      strips[static_cast<size_t>(i)]->onFaderChanged = [this](int slot,
                                                              float db) {
        faderGlideMode = 0; // Manual touch overrides automation glide
        if (currentSofMode == 1) { // SOF: MONITOR
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)]
                .monitorSendDb.store(db, std::memory_order_relaxed);
          }
          return;
        }
        if (sofActive) {"""
if target_fader_hook in content:
    content = content.replace(target_fader_hook, replace_fader_hook, 1)
    print("Updated onFaderChanged for SOF: MONITOR")
else:
    print("WARN: target_fader_hook not found")

# 11. Update timerCallback() for monitorStrip metering and sync
target_timer = """    overviewView.updateMeters(peaks, mstDbL, mstDbR);
    masterStrip.setLevels(mstDbL, mstDbR);
    dedicatedDsp.setBusMeters(mstDbL, mstDbR, (mstDbL + mstDbR) * 0.5f, monDb);"""

replace_timer = """    const float monLinR = engine.monitorPeakR_A.load(std::memory_order_relaxed);
    const float monDbR = (monLinR > 1.0e-5f)
                            ? juce::Decibels::gainToDecibels(monLinR, -60.0f)
                            : -60.0f;

    overviewView.updateMeters(peaks, mstDbL, mstDbR);
    masterStrip.setLevels(mstDbL, mstDbR);
    monitorStrip.setLevels(monDb, monDbR);
    dedicatedDsp.setBusMeters(mstDbL, mstDbR, (mstDbL + mstDbR) * 0.5f, (monDb + monDbR) * 0.5f);

    if (currentSofMode == 1) {
      for (int i = 0; i < 8; ++i) {
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
          strips[static_cast<size_t>(i)]->setSofMonitorMode(true, ch.monitorPreFader.load(std::memory_order_relaxed));
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(
              ch.monitorSendDb.load(std::memory_order_relaxed),
              !ch.monitorAssigned.load(std::memory_order_relaxed),
              ch.peak.load(std::memory_order_relaxed));
        }
      }
    }"""
if target_timer in content:
    content = content.replace(target_timer, replace_timer, 1)
    print("Updated timerCallback() for monitorStrip and SOF: MONITOR sync")
else:
    print("WARN: target_timer not found")

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Finished writing Source/GUI/MidasTouringConsoleSurface.h")

