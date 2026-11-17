#pragma once
#include "GUI/Meter.h"
#include "Mixer/MixerEngine.h"
#include "GUI/SpatialRadarDisplay.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class SurroundPanPanel final : public juce::Component, private juce::Timer {
public:
  explicit SurroundPanPanel(MixerEngine &e) : engine(e) {
    title.setText(
        "7.1.4 IMMERSIVE & MULTICHANNEL SURROUND SPATIAL PANNER (CHAPTER 30)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText("Graviton Mk 2 64-bit 3D Vector Panner \xe2\x80\xa2 Dolby "
                     "Atmos Bed \xe2\x80\xa2 Binaural Preview",
                     juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(12.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    // Channel Selector
    for (int i = 1; i <= kLogicalChannels; ++i)
      channelSelector.addItem("CH " + juce::String(i), i);
    channelSelector.setSelectedId(1, juce::dontSendNotification);
    channelSelector.onChange = [this] { syncFromChannel(); };
    addAndMakeVisible(channelSelector);

    channelNameLabel.setFont(
        juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    channelNameLabel.setColour(juce::Label::textColourId,
                               juce::Colour(0xff58a6ff));
    addAndMakeVisible(channelNameLabel);

    // Surround Mode ComboBox
    modeSelector.addItem("STEREO (2.0 TRADITIONAL)", 1);
    modeSelector.addItem("LCR (3.0 LEFT-CENTER-RIGHT)", 2);
    modeSelector.addItem("QUADRAPHONIC (4.0 SURROUND)", 3);
    modeSelector.addItem("SURROUND 5.1 (ITU-R BS.775)", 4);
    modeSelector.addItem("SURROUND 7.1 (BROADCAST BED)", 5);
    modeSelector.addItem("IMMERSIVE 7.1.4 (3D SPATIAL DOME)", 6);
    modeSelector.setSelectedId(4, juce::dontSendNotification);
    modeSelector.onChange = [this] {
      const int sel = modeSelector.getSelectedId() - 1;
      auto &ch = engine.channels[static_cast<size_t>(getSelectedChannel())];
      ch.surroundMode.store(sel, std::memory_order_relaxed);
      radarDisplay.setMode(static_cast<nova::graviton::SurroundMode>(sel));
      updateSliderEnables();
      recalculateGains();
    };
    addAndMakeVisible(modeSelector);

    // Radar Display
    radarDisplay.onPositionChanged = [this](float nx, float ny) {
      auto &ch = engine.channels[static_cast<size_t>(getSelectedChannel())];
      ch.surroundX.store(nx, std::memory_order_relaxed);
      ch.surroundY.store(ny, std::memory_order_relaxed);
      sliderX.setValue(nx, juce::dontSendNotification);
      sliderY.setValue(ny, juce::dontSendNotification);
      recalculateGains();
    };
    addAndMakeVisible(radarDisplay);

    // Parameter Sliders
    setupSlider(sliderX, "PAN X (L/R)", -1.0, 1.0, 0.01, [this] {
      const float v = static_cast<float>(sliderX.getValue());
      engine.channels[static_cast<size_t>(getSelectedChannel())]
          .surroundX.store(v, std::memory_order_relaxed);
      radarDisplay.setPosition(v, static_cast<float>(sliderY.getValue()),
                               static_cast<float>(sliderZ.getValue()));
      recalculateGains();
    });
    setupSlider(sliderY, "PAN Y (FRONT/REAR)", -1.0, 1.0, 0.01, [this] {
      const float v = static_cast<float>(sliderY.getValue());
      engine.channels[static_cast<size_t>(getSelectedChannel())]
          .surroundY.store(v, std::memory_order_relaxed);
      radarDisplay.setPosition(static_cast<float>(sliderX.getValue()), v,
                               static_cast<float>(sliderZ.getValue()));
      recalculateGains();
    });
    setupSlider(sliderZ, "HEIGHT Z (3D ELEVATION)", 0.0, 1.0, 0.01, [this] {
      const float v = static_cast<float>(sliderZ.getValue());
      engine.channels[static_cast<size_t>(getSelectedChannel())]
          .surroundZ.store(v, std::memory_order_relaxed);
      radarDisplay.setPosition(static_cast<float>(sliderX.getValue()),
                               static_cast<float>(sliderY.getValue()), v);
      recalculateGains();
    });
    setupSlider(sliderDivergence, "DIVERGENCE / SPREAD (%)", 0.0, 100.0, 1.0,
                [this] {
                  const float v =
                      static_cast<float>(sliderDivergence.getValue()) * 0.01f;
                  engine.channels[static_cast<size_t>(getSelectedChannel())]
                      .surroundDivergence.store(v, std::memory_order_relaxed);
                  recalculateGains();
                });
    setupSlider(
        sliderCenterPct, "CENTER % DIVERGENCE", 0.0, 100.0, 1.0, [this] {
          const float v = static_cast<float>(sliderCenterPct.getValue());
          engine.channels[static_cast<size_t>(getSelectedChannel())]
              .surroundCenterPct.store(v, std::memory_order_relaxed);
          recalculateGains();
        });
    setupSlider(sliderLfeDb, "SUB / LFE SEND (dB)", -60.0, 10.0, 0.5, [this] {
      const float v = static_cast<float>(sliderLfeDb.getValue());
      engine.channels[static_cast<size_t>(getSelectedChannel())]
          .surroundLfeGainDb.store(v, std::memory_order_relaxed);
      recalculateGains();
    });

    // Preset Buttons
    presetFrontLcr.setButtonText("PRESET: FRONT LCR");
    presetFrontLcr.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff21262d));
    presetFrontLcr.onClick = [this] {
      applyPreset(0.0f, 1.0f, 0.0f, 0.0f, 100.0f, -60.0f);
    };
    addAndMakeVisible(presetFrontLcr);

    presetLeadCenter.setButtonText("PRESET: LEAD VOCAL C");
    presetLeadCenter.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff21262d));
    presetLeadCenter.onClick = [this] {
      applyPreset(0.0f, 1.0f, 0.0f, 0.0f, 100.0f, -60.0f);
    };
    addAndMakeVisible(presetLeadCenter);

    presetWideSurround.setButtonText("PRESET: WIDE SURROUND");
    presetWideSurround.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff21262d));
    presetWideSurround.onClick = [this] {
      applyPreset(0.0f, -0.2f, 0.0f, 65.0f, 50.0f, -12.0f);
    };
    addAndMakeVisible(presetWideSurround);

    presetOverheadFlyby.setButtonText("PRESET: OVERHEAD 3D");
    presetOverheadFlyby.setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(0xff21262d));
    presetOverheadFlyby.onClick = [this] {
      modeSelector.setSelectedId(6, juce::sendNotification);
      applyPreset(0.0f, 0.0f, 0.85f, 30.0f, 50.0f, -6.0f);
    };
    addAndMakeVisible(presetOverheadFlyby);

    // Binaural Preview Button
    binauralPreviewBtn.setButtonText("3D BINAURAL PREVIEW (HEADPHONES / IEM)");
    binauralPreviewBtn.setClickingTogglesState(true);
    binauralPreviewBtn.setToggleState(
        engine.surroundBinauralMonitorEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    binauralPreviewBtn.setColour(juce::TextButton::buttonOnColourId,
                                 juce::Colour(0xff238636));
    binauralPreviewBtn.onClick = [this] {
      engine.surroundBinauralMonitorEnabled.store(
          binauralPreviewBtn.getToggleState(), std::memory_order_relaxed);
    };
    addAndMakeVisible(binauralPreviewBtn);

    syncFromChannel();
    startTimerHz(20);
  }

  ~SurroundPanPanel() override { stopTimer(); }

  void timerCallback() override {
    recalculateGains();
    repaint();
  }

  void setSelectedChannel(int chIdx) {
    channelSelector.setSelectedId(juce::jlimit(1, kLogicalChannels, chIdx + 1), juce::dontSendNotification);
    syncFromChannel();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff0d1117));

    // Meter Bridge Frame at bottom
    auto area = getLocalBounds().reduced(16);
    auto meterArea = area.removeFromBottom(85);

    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(meterArea.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff30363d));
    g.drawRoundedRectangle(meterArea.toFloat(), 6.0f, 1.0f);

    auto mHeader = meterArea.removeFromTop(18);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff8b949e));
    g.drawText("SURROUND OUTPUT BUS METER BRIDGE (12 CHANNELS: 7.1.4 SPATIAL "
               "BED + HEIGHT)",
               mHeader.reduced(8, 0), juce::Justification::left);

    // Draw vertical meter bars for 12 surround channels
    static const char *meterLabels[12] = {"L",   "R",   "C",   "LFE",
                                          "Lss", "Rss", "Lsr", "Rsr",
                                          "TFL", "TFR", "TRL", "TRR"};

    const int meterW = meterArea.getWidth() / 12;
    for (int i = 0; i < 12; ++i) {
      auto col = meterArea.removeFromLeft(meterW).reduced(4, 2);
      auto labelBox = col.removeFromBottom(16);

      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.setColour(juce::Colour(0xffc9d1d9));
      g.drawText(meterLabels[i], labelBox, juce::Justification::centred);

      // Meter bar background
      g.setColour(juce::Colour(0xff21262d));
      g.fillRect(col);

      // Meter fill
      const float peak =
          engine.surroundMasterPeaks[static_cast<size_t>(i)].load(
              std::memory_order_relaxed);
      const float norm = std::clamp(peak, 0.0f, 1.0f);
      const int barH = static_cast<int>(col.getHeight() * norm);

      if (barH > 0) {
        auto barRect = col.removeFromBottom(barH);
        g.setColour(norm > 0.95f ? juce::Colour(0xfff85149)
                                 : (i >= 8 ? juce::Colour(0xffbc8cff)
                                           : juce::Colour(0xff238636)));
        g.fillRect(barRect);
      }
    }
  }

  void resized() override {
    const bool isCompact = (getHeight() < 420);
    auto area = getLocalBounds().reduced(isCompact ? 6 : 16);
    const int meterH = isCompact ? (getHeight() < 320 ? 32 : 48) : 95;
    area.removeFromBottom(meterH); // Reserve for meter bridge

    auto header = area.removeFromTop(isCompact ? 22 : 36);
    channelSelector.setVisible(false);
    channelNameLabel.setBounds(header.removeFromRight(isCompact ? 120 : 180));
    title.setBounds(header);

    if (!isCompact) {
      subTitle.setBounds(area.removeFromTop(20));
      area.removeFromTop(6);
    } else {
      subTitle.setBounds(0, 0, 0, 0);
      area.removeFromTop(2);
    }

    // Left: Radar Display (Square)
    const int radarSize = std::min(area.getWidth() * 5 / 9, area.getHeight());
    radarDisplay.setBounds(area.removeFromLeft(radarSize));
    area.removeFromLeft(isCompact ? 8 : 16);

    // Right: Controls
    auto rightArea = area;
    modeSelector.setBounds(rightArea.removeFromTop(isCompact ? 22 : 28).reduced(1));
    rightArea.removeFromTop(isCompact ? 3 : 6);

    const int rowH = isCompact ? 16 : 30;
    auto grid = rightArea.removeFromTop(rowH * 6);
    sliderX.setBounds(grid.removeFromTop(rowH).reduced(1));
    sliderY.setBounds(grid.removeFromTop(rowH).reduced(1));
    sliderZ.setBounds(grid.removeFromTop(rowH).reduced(1));
    sliderDivergence.setBounds(grid.removeFromTop(rowH).reduced(1));
    sliderCenterPct.setBounds(grid.removeFromTop(rowH).reduced(1));
    sliderLfeDb.setBounds(grid.removeFromTop(rowH).reduced(1));

    rightArea.removeFromTop(isCompact ? 4 : 8);
    const int pRowH = isCompact ? 18 : 26;
    auto pRow1 = rightArea.removeFromTop(pRowH);
    presetFrontLcr.setBounds(
        pRow1.removeFromLeft(pRow1.getWidth() / 2).reduced(1));
    presetLeadCenter.setBounds(pRow1.reduced(1));

    auto pRow2 = rightArea.removeFromTop(pRowH);
    presetWideSurround.setBounds(
        pRow2.removeFromLeft(pRow2.getWidth() / 2).reduced(1));
    presetOverheadFlyby.setBounds(pRow2.reduced(1));

    rightArea.removeFromTop(isCompact ? 4 : 10);
    binauralPreviewBtn.setBounds(rightArea.removeFromTop(isCompact ? 20 : 32).reduced(1));
  }

private:
  MixerEngine &engine;

  juce::Label title, subTitle, channelNameLabel;
  juce::ComboBox channelSelector, modeSelector;
  SpatialRadarDisplay radarDisplay;

  juce::Slider sliderX, sliderY, sliderZ;
  juce::Slider sliderDivergence, sliderCenterPct, sliderLfeDb;
  juce::Label labelX, labelY, labelZ, labelDiv, labelCenter, labelLfe;

  juce::TextButton presetFrontLcr, presetLeadCenter, presetWideSurround,
      presetOverheadFlyby;
  juce::TextButton binauralPreviewBtn;

  int getSelectedChannel() const {
    const int id = channelSelector.getSelectedId();
    if (id < 1 || id > kLogicalChannels)
      return 0;
    return id - 1;
  }

  void setupSlider(juce::Slider &s, const juce::String &suffix, double minVal,
                   double maxVal, double step, std::function<void()> onChange) {
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setRange(minVal, maxVal, step);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    s.setTextValueSuffix(" " + suffix);
    s.onValueChange = std::move(onChange);
    addAndMakeVisible(s);
  }

  void syncFromChannel() {
    const int ch = getSelectedChannel();
    const auto &cState = engine.channels[static_cast<size_t>(ch)];

    const auto cName = engine.channelNames[static_cast<size_t>(ch)];
    channelNameLabel.setText(cName.isEmpty() ? ("CH " + juce::String(ch + 1))
                                             : cName,
                             juce::dontSendNotification);

    const int mode = cState.surroundMode.load(std::memory_order_relaxed);
    modeSelector.setSelectedId(mode + 1, juce::dontSendNotification);
    radarDisplay.setMode(static_cast<nova::graviton::SurroundMode>(mode));

    const float x = cState.surroundX.load(std::memory_order_relaxed);
    const float y = cState.surroundY.load(std::memory_order_relaxed);
    const float z = cState.surroundZ.load(std::memory_order_relaxed);
    const float div =
        cState.surroundDivergence.load(std::memory_order_relaxed) * 100.0f;
    const float cPct = cState.surroundCenterPct.load(std::memory_order_relaxed);
    const float lfe = cState.surroundLfeGainDb.load(std::memory_order_relaxed);

    sliderX.setValue(x, juce::dontSendNotification);
    sliderY.setValue(y, juce::dontSendNotification);
    sliderZ.setValue(z, juce::dontSendNotification);
    sliderDivergence.setValue(div, juce::dontSendNotification);
    sliderCenterPct.setValue(cPct, juce::dontSendNotification);
    sliderLfeDb.setValue(lfe, juce::dontSendNotification);

    radarDisplay.setPosition(x, y, z);
    updateSliderEnables();
    recalculateGains();
  }

  void updateSliderEnables() {
    const int mode = modeSelector.getSelectedId() - 1;
    sliderZ.setEnabled(mode == 5);                      // Height only in 7.1.4
    sliderCenterPct.setEnabled(mode == 1 || mode >= 3); // LCR, 5.1, 7.1, 7.1.4
    sliderLfeDb.setEnabled(mode >= 3);                  // 5.1, 7.1, 7.1.4
  }

  void recalculateGains() {
    const int ch = getSelectedChannel();
    const auto &cState = engine.channels[static_cast<size_t>(ch)];

    const int mode = cState.surroundMode.load(std::memory_order_relaxed);
    const auto gains = nova::graviton::SurroundPanner::calculateGains(
        static_cast<nova::graviton::SurroundMode>(mode),
        cState.surroundX.load(std::memory_order_relaxed),
        cState.surroundY.load(std::memory_order_relaxed),
        cState.surroundZ.load(std::memory_order_relaxed),
        cState.surroundDivergence.load(std::memory_order_relaxed),
        cState.surroundCenterPct.load(std::memory_order_relaxed),
        cState.surroundLfeGainDb.load(std::memory_order_relaxed));

    radarDisplay.setSpeakerGains(gains);
  }

  void applyPreset(float x, float y, float z, float div, float cPct,
                   float lfe) {
    sliderX.setValue(x, juce::sendNotification);
    sliderY.setValue(y, juce::sendNotification);
    sliderZ.setValue(z, juce::sendNotification);
    sliderDivergence.setValue(div, juce::sendNotification);
    sliderCenterPct.setValue(cPct, juce::sendNotification);
    sliderLfeDb.setValue(lfe, juce::sendNotification);
  }
};

// ==============================================================================
// Phase 86: Comprehensive Talkback & Comm Bus Intercom Matrix (MIDAS Chapter
// 32)
// ==============================================================================

} // namespace nova
