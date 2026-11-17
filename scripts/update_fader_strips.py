import os

header_path = r"Source/GUI/MidasTouringConsoleSurface.h"
with open(header_path, "r", encoding="utf-8") as f:
    lines = f.readlines()

# Part A: Replace MidasMeterLadder & MidasFader (from "// 12-SEGMENT LED METER LADDER" up to "// 1. OVERVIEW VIEW")
start_idx_a = -1
end_idx_a = -1
for i, line in enumerate(lines):
    if "// 12-SEGMENT LED METER LADDER" in line:
        start_idx_a = i - 1
        break

for i in range(start_idx_a, len(lines)):
    if "// 1. OVERVIEW VIEW" in lines[i]:
        end_idx_a = i - 1
        break

print(f"Block A: lines {start_idx_a+1} to {end_idx_a+1}")

new_block_a = """// ==============================================================================
// MIDAS ROTARY PAN / BALANCE KNOB
// ==============================================================================
class MidasPanKnob : public juce::Component {
public:
  MidasPanKnob(bool isMasterBalance = false) : isMaster(isMasterBalance) {
    setRepaintsOnMouseActivity(true);
  }

  std::function<void(float pan)> onValueChanged;

  void setValue(float v, juce::NotificationType notif = juce::sendNotification) {
    const float clamped = juce::jlimit(-1.0f, 1.0f, v);
    if (std::abs(panValue - clamped) > 0.001f) {
      panValue = clamped;
      repaint();
      if (notif != juce::dontSendNotification && onValueChanged)
        onValueChanged(panValue);
    }
  }

  float getValue() const { return panValue; }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float d = juce::jmin(area.getWidth(), area.getHeight()) - 2.0f;
    const auto knobRect = juce::Rectangle<float>((area.getWidth() - d) * 0.5f, 1.0f, d, d);

    // Bevel outer ring
    g.setColour(juce::Colour(0xff060a12));
    g.fillEllipse(knobRect);
    g.setColour(juce::Colour(0xff1e2e42));
    g.drawEllipse(knobRect, 1.2f);

    // Dial face gradient
    auto dialRect = knobRect.reduced(2.0f);
    juce::ColourGradient grad(juce::Colour(0xff1f2d3e), dialRect.getCentreX(), dialRect.getY(),
                               juce::Colour(0xff0a111a), dialRect.getCentreX(), dialRect.getBottom(), false);
    g.setGradientFill(grad);
    g.fillEllipse(dialRect);

    // Center tick / Indicator line
    const float angle = panValue * (juce::MathConstants<float>::pi * 0.75f);
    const float cx = dialRect.getCentreX();
    const float cy = dialRect.getCentreY();
    const float rInner = d * 0.18f;
    const float rOuter = d * 0.44f;

    const float sinA = std::sin(angle);
    const float cosA = std::cos(angle);

    const juce::Colour notchCol = isMaster ? midas_colors::kAccentGold : midas_colors::kAccentCyan;

    // Glowing notch tick
    g.setColour(notchCol.withAlpha(0.4f));
    g.drawLine(cx + sinA * rInner, cy - cosA * rInner,
               cx + sinA * (rOuter + 1.0f), cy - cosA * (rOuter + 1.0f), 3.5f);
    g.setColour(notchCol);
    g.drawLine(cx + sinA * rInner, cy - cosA * rInner,
               cx + sinA * rOuter, cy - cosA * rOuter, 2.0f);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
      setValue(0.0f);
      return;
    }
    dragStartY = e.position.y;
    valAtStart = panValue;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float delta = (dragStartY - e.position.y) / 80.0f;
    setValue(valAtStart + delta);
  }

  void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override {
    setValue(panValue + w.deltaY * 0.1f);
  }

private:
  float panValue = 0.0f;
  float dragStartY = 0.0f;
  float valAtStart = 0.0f;
  bool isMaster = false;
};

// ==============================================================================
// 24-SEGMENT PRECISION LED METER LADDER
// ==============================================================================
class MidasMeterLadder : public juce::Component {
public:
  MidasMeterLadder() { setOpaque(false); }

  void setLevels(float dbL, float dbR = -999.0f) {
    levelL = dbL;
    levelR = (dbR > -900.0f) ? dbR : dbL;
    isStereo = (dbR > -900.0f);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const int segCount = 24;
    const float gap = 1.5f;
    const float segH = (area.getHeight() - (segCount + 1) * gap) / static_cast<float>(segCount);

    static const float thresholds[24] = {
      -60.0f, -54.0f, -48.0f, -42.0f, -36.0f, -30.0f, -26.0f, -22.0f,
      -18.0f, -15.0f, -12.0f, -10.0f,  -8.0f,  -6.0f,  -4.0f,  -2.0f,
       -1.0f,   0.0f,   1.5f,   3.0f,   4.5f,   6.0f,   8.0f,  10.0f
    };

    auto drawColumn = [&](float lvl, float colX, float colW) {
      for (int i = 0; i < segCount; ++i) {
        const float segY = area.getBottom() - (i + 1) * (segH + gap);
        const bool lit = lvl >= thresholds[i];

        juce::Colour segCol;
        if (i >= 19)
          segCol = midas_colors::kAccentRed;
        else if (i >= 14)
          segCol = midas_colors::kAccentGold;
        else
          segCol = midas_colors::kAccentGreen;

        if (lit) {
          g.setColour(segCol);
          g.fillRoundedRectangle(colX, segY, colW, segH, 1.0f);
        } else {
          g.setColour(segCol.withAlpha(0.12f));
          g.fillRoundedRectangle(colX, segY, colW, segH, 1.0f);
        }
      }
    };

    if (isStereo) {
      const float colW = (area.getWidth() - 3.0f) * 0.5f;
      drawColumn(levelL, 0.0f, colW);
      drawColumn(levelR, colW + 3.0f, colW);
    } else {
      drawColumn(levelL, 0.0f, area.getWidth());
    }
  }

private:
  float levelL = -60.0f;
  float levelR = -60.0f;
  bool isStereo = false;
};

// ==============================================================================
// 100mm MOTORIZED STUDIO FADER & RECESSED WELL
// ==============================================================================
class MidasFader : public juce::Component {
public:
  MidasFader() { setRepaintsOnMouseActivity(true); }

  std::function<void(float db)> onFaderMoved;

  void setGainDb(float db,
                 juce::NotificationType notif = juce::sendNotification) {
    const float clamped = juce::jlimit(-90.0f, 10.0f, db);
    if (std::abs(currentDb - clamped) > 0.01f) {
      currentDb = clamped;
      repaint();
      if (notif != juce::dontSendNotification && onFaderMoved)
        onFaderMoved(currentDb);
    }
  }

  float getGainDb() const { return currentDb; }

  void setIsMaster(bool m) {
    isMaster = m;
    repaint();
  }

  void setFaderCapColor(juce::Colour col) {
    capAccent = col;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float wellW = juce::jlimit(28.0f, 38.0f, area.getWidth() - 2.0f);
    const float wellX = (area.getWidth() - wellW) * 0.5f;
    const float trackTop = 8.0f;
    const float trackBottom = area.getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;

    // 1. Recessed Fader Well Trench
    auto wellRect = juce::Rectangle<float>(wellX, trackTop, wellW, trackH);
    g.setColour(juce::Colour(0xff060a12));
    g.fillRoundedRectangle(wellRect, 6.0f);
    g.setColour(juce::Colour(0xff020406));
    g.drawRoundedRectangle(wellRect, 6.0f, 1.2f);
    g.setColour(juce::Colour(0x50000000));
    g.drawRoundedRectangle(wellRect.reduced(1.0f), 5.0f, 1.0f);

    // Mechanical Track Slit
    const float slitW = 2.5f;
    const float slitX = (area.getWidth() - slitW) * 0.5f;
    g.setColour(juce::Colour(0xff010204));
    g.fillRoundedRectangle(slitX, trackTop + 8.0f, slitW, trackH - 16.0f, 1.0f);

    // 2. Fader Cap
    const float normPos = dbToNormalized(currentDb);
    const float capH = 28.0f;
    const float capW = juce::jmin(area.getWidth(), wellW + 4.0f);
    const float capY = trackBottom - normPos * trackH - capH * 0.5f;
    const float capX = (area.getWidth() - capW) * 0.5f;

    auto capRect = juce::Rectangle<float>(capX, capY, capW, capH);

    // Drop Shadow
    g.setColour(juce::Colour(0x95000000));
    g.fillRoundedRectangle(capRect.translated(0.0f, 3.0f), 4.0f);

    // Metallic Touring Gradient
    juce::ColourGradient capGrad(juce::Colour(0xff2d3b4e), capX, capY,
                                 juce::Colour(0xff0e141d), capX, capY + capH, false);
    capGrad.addColour(0.35, juce::Colour(0xff222e3d));
    capGrad.addColour(0.65, juce::Colour(0xff161f2c));
    g.setGradientFill(capGrad);
    g.fillRoundedRectangle(capRect, 3.5f);

    // Top Bevel Highlight
    g.setColour(juce::Colour(0xff556b87));
    g.drawHorizontalLine(static_cast<int>(capY + 1.0f), capX + 2.0f, capX + capW - 2.0f);

    // Cap Outline
    g.setColour(juce::Colour(0xff1e2a3a));
    g.drawRoundedRectangle(capRect, 3.5f, 1.0f);

    // Signature Glowing Center Stripe
    const float lineY = capY + capH * 0.5f;
    const float lineW = capW - 8.0f;
    const float lineX = capX + 4.0f;

    if (isMaster) {
      g.setColour(juce::Colour(0x70ff9f0a));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(midas_colors::kAccentGold);
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    } else {
      g.setColour(juce::Colour(0x60ffffff));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(juce::Colours::white);
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
      setGainDb(0.0f);
      return;
    }
    dragStartY = e.position.y;
    dbAtStart = currentDb;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float trackTop = 8.0f;
    const float trackBottom = getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;
    const float deltaNorm = (dragStartY - e.position.y) / trackH;
    const float startNorm = dbToNormalized(dbAtStart);
    setGainDb(normalizedToDb(startNorm + deltaNorm));
  }

  void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override {
    setGainDb(currentDb + w.deltaY * 0.5f);
  }

private:
  float currentDb = 0.0f;
  float dragStartY = 0.0f;
  float dbAtStart = 0.0f;
  bool isMaster = false;
  juce::Colour capAccent = midas_colors::kAccentCyan;

  static float dbToNormalized(float db) {
    if (db <= -90.0f)
      return 0.0f;
    if (db >= 10.0f)
      return 1.0f;
    if (db >= 0.0f)
      return 0.75f + (db / 10.0f) * 0.25f;
    return std::pow((db + 90.0f) / 90.0f, 2.2f) * 0.75f;
  }

  static float normalizedToDb(float norm) {
    const float c = juce::jlimit(0.0f, 1.0f, norm);
    if (c <= 0.0001f)
      return -90.0f;
    if (c >= 0.75f)
      return ((c - 0.75f) / 0.25f) * 10.0f;
    return std::pow(c / 0.75f, 1.0f / 2.2f) * 90.0f - 90.0f;
  }
};
"""

# Part B: Replace MidasConsoleStrip and MidasMasterStrip
start_idx_b = -1
end_idx_b = -1
for i, line in enumerate(lines):
    if "class MidasConsoleStrip" in line:
        start_idx_b = i - 2
        break

for i in range(start_idx_b, len(lines)):
    if "class MidasMasterProcessingPanel" in line or "// MASTER STEREO PROCESSING SUB-PANEL" in lines[i]:
        end_idx_b = i - 1
        break

print(f"Block B: lines {start_idx_b+1} to {end_idx_b+1}")

new_block_b = """// ==============================================================================
// MOTORIZED CHANNEL FADER STRIP
// ==============================================================================
class MidasConsoleStrip : public juce::Component {
public:
  MidasConsoleStrip(int slotIndex)
      : slot(slotIndex),
        panKnob(false),
        btnSelect("SEL", MidasTactileButton::GlowMode::Cyan),
        btnMute("MUTE", MidasTactileButton::GlowMode::Red),
        btnSolo("SOLO", MidasTactileButton::GlowMode::Gold) {

    addAndMakeVisible(panKnob);
    addAndMakeVisible(btnSelect);
    addAndMakeVisible(btnMute);
    addAndMakeVisible(btnSolo);
    addAndMakeVisible(fader);
    addAndMakeVisible(meter);
  }

  std::function<void(int slot)> onSelected;
  std::function<void(int slot, float db)> onFaderChanged;
  std::function<void(int slot, bool muted)> onMuteChanged;
  std::function<void(int slot, bool solo)> onSoloChanged;
  std::function<void(int slot, float panVal)> onPanChanged;
  std::function<void(int slot, float trimDb)> onTrimChanged;

  void setChannelInfo(const juce::String &tag, const juce::String &name,
                      const juce::String &badge, juce::Colour color) {
    chTag = tag;
    chName = name;
    chBadge = badge;
    accentColor = color;
    fader.setFaderCapColor(color);
    setName(name);
    repaint();
  }

  void syncState(const ChannelState &state) {
    const float fDb = state.gainDb.load(std::memory_order_relaxed);
    fader.setGainDb(fDb, juce::dontSendNotification);
    btnMute.setToggleState(state.muted.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    btnSolo.setToggleState(state.solo.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    panKnob.setValue(state.pan.load(std::memory_order_relaxed),
                     juce::dontSendNotification);

    const float pk = state.peak.load(std::memory_order_relaxed);
    const float pkDb =
        (pk > 1.0e-5f) ? juce::Decibels::gainToDecibels(pk, -60.0f) : -60.0f;
    meter.setLevels(pkDb);
  }

  void syncBusOrVcaState(float faderDb, bool muted, float linearPeak) {
    fader.setGainDb(faderDb, juce::dontSendNotification);
    btnMute.setToggleState(muted, juce::dontSendNotification);
    const float pkDb = (linearPeak > 1.0e-5f)
                           ? juce::Decibels::gainToDecibels(linearPeak, -60.0f)
                           : -60.0f;
    meter.setLevels(pkDb);
    repaint();
  }

  float getCurrentFaderDb() const { return fader.getGainDb(); }

  void setSelected(bool sel) {
    isSelected = sel;
    btnSelect.setToggleState(sel, juce::dontSendNotification);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Strip Card Background & Outer Selection Border
    auto cardRect = area.reduced(2.0f);
    g.setColour(midas_colors::kStripBg);
    g.fillRoundedRectangle(cardRect, 8.0f);

    if (isSelected) {
      g.setColour(midas_colors::kAccentCyan.withAlpha(0.35f));
      g.drawRoundedRectangle(cardRect.expanded(1.0f), 8.5f, 2.5f);
      g.setColour(midas_colors::kAccentCyan);
      g.drawRoundedRectangle(cardRect, 8.0f, 2.0f);
    } else {
      g.setColour(midas_colors::kBorderCol.withAlpha(0.7f));
      g.drawRoundedRectangle(cardRect, 8.0f, 1.2f);
    }

    // 2. Pan Readout Text (directly beneath Pan Knob)
    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(juce::Colour(0xff7fa0c0));
    juce::String panStr = "C";
    const float p = panKnob.getValue();
    if (p < -0.01f) {
      panStr = "L" + juce::String(static_cast<int>(std::round(std::abs(p) * 100.0f)));
    } else if (p > 0.01f) {
      panStr = "R" + juce::String(static_cast<int>(std::round(p * 100.0f)));
    }
    g.drawText(panStr, 0, 36, static_cast<int>(w), 14, juce::Justification::centred);

    // 3. Digital LCD Scribble Strip Screen
    const float oledY = 82.0f;
    const float oledH = 58.0f;
    auto oledRect = juce::Rectangle<float>(6.0f, oledY, w - 12.0f, oledH);

    g.setColour(juce::Colour(0xff03060a));
    g.fillRoundedRectangle(oledRect, 4.0f);
    g.setColour(accentColor);
    g.drawRoundedRectangle(oledRect, 4.0f, 1.2f);

    // Top Category Color Accent Bar
    g.fillRect(oledRect.getX() + 2.0f, oledRect.getY() + 2.0f,
               oledRect.getWidth() - 4.0f, 2.5f);

    // Line 1: Category Badge (e.g. "DRUMS 1", "BASS 4", "GUITARS 5")
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
               juce::Justification::centred);

    // Line 4: Fader dB Readout (e.g. "+1.2 dB", "+0.0 dB", "-4.2 dB")
    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    const float curDb = fader.getGainDb();
    juce::String dbStr;
    if (curDb <= -89.0f)
      dbStr = "-\xe2\x88\x9e dB";
    else if (curDb >= 0.05f)
      dbStr = "+" + juce::String(curDb, 1) + " dB";
    else
      dbStr = juce::String(curDb, 1) + " dB";

    g.drawText(dbStr, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 43.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 13,
               juce::Justification::centred);

    // 4. Bottom Channel Identifier (e.g. "CH 01" .. "CH 08")
    const float badgeY = h - 25.0f;
    const float badgeW = juce::jmin(w - 20.0f, 54.0f);
    const float badgeX = (w - badgeW) * 0.5f;
    auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeW, 18.0f);

    g.setColour(juce::Colour(0xff060a12));
    g.fillRoundedRectangle(badgeRect, 4.0f);
    g.setColour(juce::Colour(0xff182436));
    g.drawRoundedRectangle(badgeRect, 4.0f, 1.0f);

    g.setFont(midas_ui::getFont(8.5f, true));
    g.setColour(juce::Colour(0xff7a8fa6));
    juce::String bottomText = "CH " + (slot < 9 ? ("0" + juce::String(slot + 1)) : juce::String(slot + 1));
    g.drawText(bottomText, badgeRect, juce::Justification::centred);
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Pan Knob
    const float knobSize = 28.0f;
    panKnob.setBounds(static_cast<int>((w - knobSize) * 0.5f), 6,
                      static_cast<int>(knobSize), static_cast<int>(knobSize));

    // 2. SEL Button
    const float selW = w - 16.0f;
    btnSelect.setBounds(static_cast<int>((w - selW) * 0.5f), 54,
                        static_cast<int>(selW), 24);

    // 3. MUTE & SOLO Buttons (Side-by-side)
    const float btnGap = 4.0f;
    const float btnW = (w - 16.0f - btnGap) * 0.5f;
    const float btnY = 144.0f;
    btnMute.setBounds(8, static_cast<int>(btnY), static_cast<int>(btnW), 24);
    btnSolo.setBounds(static_cast<int>(8.0f + btnW + btnGap), static_cast<int>(btnY),
                      static_cast<int>(btnW), 24);

    // 4. Fader & Meter (Meter on the LEFT, Fader on the RIGHT)
    const float faderTop = 174.0f;
    const float faderBottom = h - 30.0f;
    const float faderH = faderBottom - faderTop;

    const float meterW = 7.0f;
    const float wellW = juce::jlimit(30.0f, 40.0f, w - meterW - 26.0f);
    const float totalFaderAreaW = meterW + 8.0f + wellW;
    const float startX = (w - totalFaderAreaW) * 0.5f;

    meter.setBounds(static_cast<int>(startX), static_cast<int>(faderTop + 4.0f),
                    static_cast<int>(meterW), static_cast<int>(faderH - 8.0f));

    fader.setBounds(static_cast<int>(startX + meterW + 8.0f), static_cast<int>(faderTop),
                    static_cast<int>(wellW), static_cast<int>(faderH));

    btnSelect.onClick = [this] {
      if (onSelected)
        onSelected(slot);
    };
    btnMute.onClick = [this] {
      if (onMuteChanged)
        onMuteChanged(slot, btnMute.getToggleState());
    };
    btnSolo.onClick = [this] {
      if (onSoloChanged)
        onSoloChanged(slot, btnSolo.getToggleState());
    };
    fader.onFaderMoved = [this](float db) {
      repaint();
      if (onFaderChanged)
        onFaderChanged(slot, db);
    };
    panKnob.onValueChanged = [this](float val) {
      repaint();
      if (onPanChanged)
        onPanChanged(slot, val);
      if (onTrimChanged)
        onTrimChanged(slot, val * 18.0f);
    };
  }

private:
  int slot = 0;
  bool isSelected = false;
  juce::String chTag = "IN 01";
  juce::String chName = "KICK IN";
  juce::String chBadge = "CH 01";
  juce::Colour accentColor = midas_colors::kAccentRed;

  MidasPanKnob panKnob;
  MidasTactileButton btnSelect, btnMute, btnSolo;
  MidasFader fader;
  MidasMeterLadder meter;
};

// ==============================================================================
// MASTER STEREO BUS STRIP (MASTER L/R SECTION)
// ==============================================================================
class MidasMasterStrip : public juce::Component {
public:
  MidasMasterStrip()
      : balanceKnob(true),
        btnDim("DIM", MidasTactileButton::GlowMode::Gold),
        btnMono("MONO", MidasTactileButton::GlowMode::Gray),
        btnClearSolo("CLR SOLO", MidasTactileButton::GlowMode::Gold),
        btnMuteMain("MUTE", MidasTactileButton::GlowMode::Red),
        btnMainLr("MAIN L/R", MidasTactileButton::GlowMode::Gold) {

    addAndMakeVisible(balanceKnob);
    addAndMakeVisible(btnDim);
    addAndMakeVisible(btnMuteMain);
    addAndMakeVisible(masterFader);
    addAndMakeVisible(masterMeter);
    addAndMakeVisible(btnMainLr);

    masterFader.setIsMaster(true);
    masterFader.setFaderCapColor(midas_colors::kAccentGold);
    btnClearSolo.setClickingTogglesState(false);
  }

  std::function<void(float db)> onMasterFaderMoved;
  std::function<void(bool muted)> onMasterMuteChanged;
  std::function<void(bool dim)> onDimToggled;
  std::function<void(bool mono)> onMonoToggled;
  std::function<void()> onClearSoloClicked;
  std::function<void(float bal)> onBalanceChanged;

  void setLevels(float dbL, float dbR) { masterMeter.setLevels(dbL, dbR); }

  void setFaderDb(float db) {
    masterFader.setGainDb(db, juce::dontSendNotification);
    repaint();
  }

  float getFaderDb() const { return masterFader.getGainDb(); }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Card Background & Golden Border
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
               static_cast<int>(oledRect.getWidth() - 8.0f), 14, juce::Justification::centred);

    // Line 3: Fader dB Readout
    g.setFont(midas_ui::getFont(10.0f, true));
    g.setColour(midas_colors::kAccentCyan);
    const float curDb = masterFader.getGainDb();
    juce::String dbStr;
    if (curDb <= -89.0f)
      dbStr = "-\xe2\x88\x9e dB";
    else if (curDb >= 0.05f)
      dbStr = "+" + juce::String(curDb, 1) + " dB";
    else
      dbStr = juce::String(curDb, 1) + " dB";
    g.drawText(dbStr, static_cast<int>(oledRect.getX() + 4.0f), static_cast<int>(oledY + 35.0f),
               static_cast<int>(oledRect.getWidth() - 8.0f), 13, juce::Justification::centred);

    // 5. Calibrated dB Scale Labels (Right of Master Fader)
    const float faderTop = 152.0f;
    const float faderBottom = h - 34.0f;
    const float trackH = (faderBottom - faderTop) - 16.0f;
    const float faderRight = masterFader.getBounds().toFloat().getRight();

    struct ScaleMark {
      float db;
      const char *label;
      juce::Colour col;
    };
    static const ScaleMark marks[] = {
      { 10.0f, "+10", midas_colors::kAccentRed },
      {  0.0f,   "0", midas_colors::kAccentGold },
      { -5.0f,  "-5", juce::Colour(0xff7a8fa6) },
      {-10.0f, "-10", juce::Colour(0xff7a8fa6) },
      {-20.0f, "-20", juce::Colour(0xff7a8fa6) },
      {-30.0f, "-30", juce::Colour(0xff7a8fa6) },
      {-90.0f, "-\xe2\x88\x9e", juce::Colour(0xff7a8fa6) }
    };

    g.setFont(midas_ui::getFont(7.5f, true));
    for (const auto &m : marks) {
      const float norm = (m.db <= -90.0f) ? 0.0f : (m.db >= 10.0f ? 1.0f : (m.db >= 0.0f ? 0.75f + (m.db / 10.0f) * 0.25f : std::pow((m.db + 90.0f) / 90.0f, 2.2f) * 0.75f));
      const float y = (faderBottom - 8.0f) - norm * trackH;
      g.setColour(m.col);
      g.drawText(m.label, static_cast<int>(faderRight + 3.0f), static_cast<int>(y - 6.0f), 24, 12, juce::Justification::centredLeft);
    }
  }

  void resized() override {
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Balance Knob
    const float knobSize = 28.0f;
    balanceKnob.setBounds(static_cast<int>((w - knobSize) * 0.5f), 18,
                          static_cast<int>(knobSize), static_cast<int>(knobSize));

    // 2. MUTE & DIM Buttons (Side-by-side)
    const float btnGap = 4.0f;
    const float btnW = (w - 16.0f - btnGap) * 0.5f;
    const float btnY = 120.0f;
    btnMuteMain.setBounds(8, static_cast<int>(btnY), static_cast<int>(btnW), 24);
    btnDim.setBounds(static_cast<int>(8.0f + btnW + btnGap), static_cast<int>(btnY),
                     static_cast<int>(btnW), 24);

    // 3. Master Dual Stereo Meter + Fader + Scale
    const float faderTop = 152.0f;
    const float faderBottom = h - 34.0f;
    const float faderH = faderBottom - faderTop;

    const float dualMeterW = 14.0f;
    const float wellW = 38.0f;
    const float meterX = 8.0f;
    const float faderX = meterX + dualMeterW + 6.0f;

    masterMeter.setBounds(static_cast<int>(meterX), static_cast<int>(faderTop + 4.0f),
                          static_cast<int>(dualMeterW), static_cast<int>(faderH - 8.0f));

    masterFader.setBounds(static_cast<int>(faderX), static_cast<int>(faderTop),
                          static_cast<int>(wellW), static_cast<int>(faderH));

    // 4. MAIN L/R Bottom Button
    btnMainLr.setBounds(8, static_cast<int>(h - 26.0f), static_cast<int>(w - 16.0f), 22);

    balanceKnob.onValueChanged = [this](float bal) {
      if (onBalanceChanged)
        onBalanceChanged(bal);
    };
    masterFader.onFaderMoved = [this](float db) {
      repaint();
      if (onMasterFaderMoved)
        onMasterFaderMoved(db);
    };
    btnMuteMain.onClick = [this] {
      if (onMasterMuteChanged)
        onMasterMuteChanged(btnMuteMain.getToggleState());
    };
    btnDim.onClick = [this] {
      if (onDimToggled)
        onDimToggled(btnDim.getToggleState());
    };
    btnMono.onClick = [this] {
      if (onMonoToggled)
        onMonoToggled(btnMono.getToggleState());
    };
    btnClearSolo.onClick = [this] {
      if (onClearSoloClicked)
        onClearSoloClicked();
    };
  }

private:
  MidasPanKnob balanceKnob;
  MidasTactileButton btnDim, btnMono, btnClearSolo, btnMuteMain, btnMainLr;
  MidasFader masterFader;
  MidasMeterLadder masterMeter;
};
"""

# Apply Block B first (since it's lower down in the file, line indices don't shift block A)
lines[start_idx_b:end_idx_b] = [new_block_b]
lines[start_idx_a:end_idx_a] = [new_block_a]

with open(header_path, "w", encoding="utf-8") as f:
    f.writelines(lines)

print("Blocks A and B replaced successfully!")
