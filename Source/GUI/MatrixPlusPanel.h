#pragma once
#include "GUI/Meter.h"
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class MatrixPlusPanel final : public juce::Component, private juce::Timer {
public:
  explicit MatrixPlusPanel(MixerEngine &e) : engine(e) {
    title.setText("MATRIX PLUS & SPEAKER DELAY ALIGNMENT (MIDAS CHAPTER 29)",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText("24-Matrix Phase-Coherent Acoustic Delay, Environmental "
                     "Speed-of-Sound Calculator & Sub Alignment",
                     juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    // Environmental Calculator Controls
    tempLabel.setText("Ambient Temp:", juce::dontSendNotification);
    tempLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d1d9));
    addAndMakeVisible(tempLabel);

    tempSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempSlider.setRange(-20.0, 50.0, 0.5);
    tempSlider.setValue(
        engine.matrixPlusEngine.getEnvironment().temperatureCelsius,
        juce::dontSendNotification);
    tempSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    tempSlider.setTextValueSuffix(" \xc2\xb0\x43");
    tempSlider.onValueChange = [this] {
      engine.matrixPlusEngine.setTemperatureCelsius(tempSlider.getValue());
      updateEnvReadout();
      updateSelectedChannelUi();
    };
    addAndMakeVisible(tempSlider);

    humidityLabel.setText("Humidity (%RH):", juce::dontSendNotification);
    humidityLabel.setColour(juce::Label::textColourId,
                            juce::Colour(0xffc9d1d9));
    addAndMakeVisible(humidityLabel);

    humiditySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    humiditySlider.setRange(0.0, 100.0, 1.0);
    humiditySlider.setValue(
        engine.matrixPlusEngine.getEnvironment().relativeHumidityPct,
        juce::dontSendNotification);
    humiditySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    humiditySlider.setTextValueSuffix(" %");
    humiditySlider.onValueChange = [this] {
      engine.matrixPlusEngine.setRelativeHumidity(humiditySlider.getValue());
      updateEnvReadout();
      updateSelectedChannelUi();
    };
    addAndMakeVisible(humiditySlider);

    speedOfSoundLabel.setFont(
        juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    speedOfSoundLabel.setColour(juce::Label::textColourId,
                                juce::Colour(0xff58a6ff));
    addAndMakeVisible(speedOfSoundLabel);

    // Matrix Selector Combo
    for (int m = 1; m <= 24; ++m) {
      juce::String name = "MTX " + juce::String(m);
      if (m == 1)
        name += " (Main Left)";
      else if (m == 2)
        name += " (Main Right)";
      else if (m == 3)
        name += " (Sub Array)";
      else if (m == 4)
        name += " (Frontfill)";
      else if (m == 5)
        name += " (Outfill L)";
      else if (m == 6)
        name += " (Outfill R)";
      else if (m == 7)
        name += " (Delay Tower 1)";
      else if (m == 8)
        name += " (Delay Tower 2)";
      matrixSelector.addItem(name, m);
    }
    matrixSelector.setSelectedId(1, juce::dontSendNotification);
    matrixSelector.onChange = [this] {
      selectedMatrix = matrixSelector.getSelectedId() - 1;
      updateSelectedChannelUi();
    };
    addAndMakeVisible(matrixSelector);

    // Preset Combo
    presetSelector.addItem("[Touring Presets...]", 1);
    presetSelector.addItem("FOH Main Line Array (0m)", 2);
    presetSelector.addItem("Subwoofer Array Alignment (1.8m + 80Hz AP)", 3);
    presetSelector.addItem("Stage Frontfills (4.5m, -3dB)", 4);
    presetSelector.addItem("Stage Outfills (8.0m, -2dB)", 5);
    presetSelector.addItem("Lawn / Delay Tower 1 (35.0m)", 6);
    presetSelector.addItem("Stadium Delay Tower 2 (70.0m)", 7);
    presetSelector.setSelectedId(1, juce::dontSendNotification);
    presetSelector.onChange = [this] {
      const int sel = presetSelector.getSelectedId();
      if (sel == 2)
        engine.matrixPlusEngine.applyTouringPreset(
            static_cast<size_t>(selectedMatrix), "FOH_MAIN_ARRAY");
      else if (sel == 3)
        engine.matrixPlusEngine.applyTouringPreset(
            static_cast<size_t>(selectedMatrix), "SUBWOOFER_ARRAY");
      else if (sel == 4)
        engine.matrixPlusEngine.applyTouringPreset(
            static_cast<size_t>(selectedMatrix), "FRONTFILL");
      else if (sel == 5)
        engine.matrixPlusEngine.applyTouringPreset(
            static_cast<size_t>(selectedMatrix), "OUTFILL");
      else if (sel == 6)
        engine.matrixPlusEngine.applyTouringPreset(
            static_cast<size_t>(selectedMatrix), "DELAY_TOWER_1");
      else if (sel == 7)
        engine.matrixPlusEngine.applyTouringPreset(
            static_cast<size_t>(selectedMatrix), "DELAY_TOWER_2");
      presetSelector.setSelectedId(1, juce::dontSendNotification);
      updateSelectedChannelUi();
    };
    addAndMakeVisible(presetSelector);

    // Delay Controls
    delayEnableBtn.setClickingTogglesState(true);
    delayEnableBtn.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff238636));
    delayEnableBtn.onClick = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setDelayEnabled(delayEnableBtn.getToggleState());
      delayEnableBtn.setButtonText(ch.isDelayEnabled() ? "DELAY: ON"
                                                       : "DELAY: OFF");
    };
    addAndMakeVisible(delayEnableBtn);

    delayMsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    delayMsSlider.setRange(0.0, 500.0, 0.01);
    delayMsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    delayMsSlider.setTextValueSuffix(" ms");
    delayMsSlider.onValueChange = [this] {
      if (!isUpdatingUi) {
        auto &ch = engine.matrixPlusEngine.getChannel(
            static_cast<size_t>(selectedMatrix));
        ch.setDelayMs(delayMsSlider.getValue());
        const double distM =
            engine.matrixPlusEngine.getEnvironment().delayMsToDistanceMeters(
                ch.getDelayMs());
        isUpdatingUi = true;
        distanceSlider.setValue(distM, juce::dontSendNotification);
        distFeetLabel.setText("= " + juce::String(distM * 3.28084, 1) + " ft",
                              juce::dontSendNotification);
        isUpdatingUi = false;
      }
    };
    addAndMakeVisible(delayMsSlider);

    distanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    distanceSlider.setRange(0.0, 171.0, 0.01);
    distanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    distanceSlider.setTextValueSuffix(" m");
    distanceSlider.onValueChange = [this] {
      if (!isUpdatingUi) {
        const double distM = distanceSlider.getValue();
        engine.matrixPlusEngine.applyDistanceMeters(
            static_cast<size_t>(selectedMatrix), distM);
        auto &ch = engine.matrixPlusEngine.getChannel(
            static_cast<size_t>(selectedMatrix));
        isUpdatingUi = true;
        delayMsSlider.setValue(ch.getDelayMs(), juce::dontSendNotification);
        distFeetLabel.setText("= " + juce::String(distM * 3.28084, 1) + " ft",
                              juce::dontSendNotification);
        isUpdatingUi = false;
      }
    };
    addAndMakeVisible(distanceSlider);

    distFeetLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    distFeetLabel.setColour(juce::Label::textColourId,
                            juce::Colour(0xff8b949e));
    addAndMakeVisible(distFeetLabel);

    // Polarity Ø Invert Button
    polarityBtn.setClickingTogglesState(true);
    polarityBtn.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xffda3633));
    polarityBtn.onClick = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setPolarityInvert(polarityBtn.getToggleState());
      polarityBtn.setButtonText(ch.getPolarityInvert()
                                    ? "POLARITY: \xc3\x98 INVERTED"
                                    : "POLARITY: NORMAL");
    };
    addAndMakeVisible(polarityBtn);

    // All-Pass Crossover Phase Alignment
    allPassEnableBtn.setClickingTogglesState(true);
    allPassEnableBtn.setColour(juce::TextButton::buttonOnColourId,
                               juce::Colour(0xff1f6feb));
    allPassEnableBtn.onClick = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setAllPassEnabled(allPassEnableBtn.getToggleState());
      allPassEnableBtn.setButtonText(
          ch.isAllPassEnabled() ? "CROSSOVER AP: ON" : "CROSSOVER AP: OFF");
    };
    addAndMakeVisible(allPassEnableBtn);

    allPassFreqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    allPassFreqSlider.setRange(20.0, 2000.0, 1.0);
    allPassFreqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70,
                                      20);
    allPassFreqSlider.setTextValueSuffix(" Hz");
    allPassFreqSlider.onValueChange = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setAllPassFrequency(allPassFreqSlider.getValue());
    };
    addAndMakeVisible(allPassFreqSlider);

    allPassQSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    allPassQSlider.setRange(0.2, 5.0, 0.05);
    allPassQSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    allPassQSlider.setTextValueSuffix(" Q");
    allPassQSlider.onValueChange = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setAllPassQ(allPassQSlider.getValue());
    };
    addAndMakeVisible(allPassQSlider);

    // Gain Trim & Mute
    trimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trimSlider.setRange(-24.0, 12.0, 0.5);
    trimSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    trimSlider.setTextValueSuffix(" dB");
    trimSlider.onValueChange = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setGainTrimDb(trimSlider.getValue());
    };
    addAndMakeVisible(trimSlider);

    muteBtn.setClickingTogglesState(true);
    muteBtn.setColour(juce::TextButton::buttonOnColourId,
                      juce::Colour(0xffda3633));
    muteBtn.onClick = [this] {
      auto &ch = engine.matrixPlusEngine.getChannel(
          static_cast<size_t>(selectedMatrix));
      ch.setMuted(muteBtn.getToggleState());
      muteBtn.setButtonText(ch.isMuted() ? "MUTED" : "MUTE");
    };
    addAndMakeVisible(muteBtn);

    peakLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    peakLabel.setColour(juce::Label::textColourId, juce::Colour(0xff48d597));
    addAndMakeVisible(peakLabel);

    // 24-Matrix Overview Editor
    overviewEditor.setMultiLine(true);
    overviewEditor.setReadOnly(true);
    overviewEditor.setFont(juce::Font(juce::FontOptions(11.0f)));
    overviewEditor.setColour(juce::TextEditor::backgroundColourId,
                             juce::Colour(0xff0d1117));
    overviewEditor.setColour(juce::TextEditor::textColourId,
                             juce::Colour(0xffc9d1d9));
    addAndMakeVisible(overviewEditor);

    updateEnvReadout();
    updateSelectedChannelUi();
    startTimerHz(10);
  }

  void updateEnvReadout() {
    const double c =
        engine.matrixPlusEngine.getEnvironment().calculateSpeedOfSound();
    const double cFps =
        engine.matrixPlusEngine.getEnvironment().calculateSpeedOfSoundFps();
    speedOfSoundLabel.setText("Speed of Sound: c = " + juce::String(c, 1) +
                                  " m/s (" + juce::String(cFps, 1) + " ft/s)",
                              juce::dontSendNotification);
  }

  void updateSelectedChannelUi() {
    isUpdatingUi = true;
    auto &ch =
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(selectedMatrix));
    delayEnableBtn.setToggleState(ch.isDelayEnabled(),
                                  juce::dontSendNotification);
    delayEnableBtn.setButtonText(ch.isDelayEnabled() ? "DELAY: ON"
                                                     : "DELAY: OFF");

    delayMsSlider.setValue(ch.getDelayMs(), juce::dontSendNotification);
    const double distM =
        engine.matrixPlusEngine.getEnvironment().delayMsToDistanceMeters(
            ch.getDelayMs());
    distanceSlider.setValue(distM, juce::dontSendNotification);
    distFeetLabel.setText("= " + juce::String(distM * 3.28084, 1) + " ft",
                          juce::dontSendNotification);

    polarityBtn.setToggleState(ch.getPolarityInvert(),
                               juce::dontSendNotification);
    polarityBtn.setButtonText(ch.getPolarityInvert()
                                  ? "POLARITY: \xc3\x98 INVERTED"
                                  : "POLARITY: NORMAL");

    allPassEnableBtn.setToggleState(ch.isAllPassEnabled(),
                                    juce::dontSendNotification);
    allPassEnableBtn.setButtonText(ch.isAllPassEnabled() ? "CROSSOVER AP: ON"
                                                         : "CROSSOVER AP: OFF");
    allPassFreqSlider.setValue(ch.getAllPassFrequency(),
                               juce::dontSendNotification);
    allPassQSlider.setValue(ch.getAllPassQ(), juce::dontSendNotification);

    trimSlider.setValue(ch.getGainTrimDb(), juce::dontSendNotification);
    muteBtn.setToggleState(ch.isMuted(), juce::dontSendNotification);
    muteBtn.setButtonText(ch.isMuted() ? "MUTED" : "MUTE");
    isUpdatingUi = false;
  }

  void timerCallback() override {
    const auto &ch =
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(selectedMatrix));
    const float pk = ch.getPeak();
    const double pkDb = (pk > 1e-5f) ? 20.0 * std::log10(pk) : -99.9;
    peakLabel.setText("Peak: " + (pkDb < -90.0
                                      ? "-inf dBFS"
                                      : juce::String(pkDb, 1) + " dBFS"),
                      juce::dontSendNotification);

    // Refresh Overview
    juce::String text =
        "--- MATRIX PLUS 24-CHANNEL SPEAKER ALIGNMENT NETWORK ---\n";
    for (size_t m = 0; m < 24; ++m) {
      const auto &c = engine.matrixPlusEngine.getChannel(m);
      const double dM =
          engine.matrixPlusEngine.getEnvironment().delayMsToDistanceMeters(
              c.getDelayMs());
      const float p = c.getPeak();
      const double pDb = (p > 1e-5f) ? 20.0 * std::log10(p) : -99.9;
      text << "MTX " << (m < 9 ? "0" : "") << juce::String(m + 1) << ": "
           << (c.isDelayEnabled() ? "DELAY ON " : "DELAY OFF") << " | "
           << juce::String(c.getDelayMs(), 2).paddedLeft(' ', 6) << " ms ("
           << juce::String(dM, 2).paddedLeft(' ', 6) << " m) | "
           << (c.getPolarityInvert() ? "\xc3\x98 INV" : "\xc3\x98 NORM")
           << " | "
           << (c.isAllPassEnabled()
                   ? ("AP " + juce::String(c.getAllPassFrequency(), 0) + "Hz")
                         .paddedLeft(' ', 9)
                   : "   AP OFF")
           << " | "
           << "Trim: " << juce::String(c.getGainTrimDb(), 1).paddedLeft(' ', 5)
           << " dB | "
           << (c.isMuted() ? "[MUTED]"
                           : ("Peak: " +
                              (pDb < -90.0 ? "-inf dBFS"
                                           : juce::String(pDb, 1) + " dBFS")))
           << "\n";
    }
    overviewEditor.setText(text);
  }

  void resized() override {
    const bool isCompact = (getHeight() < 360);
    auto area = getLocalBounds().reduced(isCompact ? 4 : 12);
    title.setBounds(area.removeFromTop(isCompact ? 18 : 24));
    if (!isCompact) {
      subTitle.setBounds(area.removeFromTop(18));
      area.removeFromTop(6);
    } else {
      subTitle.setBounds(0, 0, 0, 0);
      area.removeFromTop(2);
    }

    const int sp = isCompact ? 2 : 6;

    // Row 1: Environmental
    auto envRow = area.removeFromTop(isCompact ? 22 : 28);
    tempLabel.setBounds(envRow.removeFromLeft(isCompact ? 75 : 95).reduced(0, 2));
    tempSlider.setBounds(envRow.removeFromLeft(isCompact ? 130 : 180).reduced(1));
    envRow.removeFromLeft(sp);
    humidityLabel.setBounds(envRow.removeFromLeft(isCompact ? 80 : 105).reduced(0, 2));
    humiditySlider.setBounds(envRow.removeFromLeft(isCompact ? 120 : 160).reduced(1));
    envRow.removeFromLeft(sp);
    speedOfSoundLabel.setBounds(envRow);
    area.removeFromTop(sp);

    // Row 2: Matrix & Preset selection
    auto selRow = area.removeFromTop(isCompact ? 22 : 30);
    matrixSelector.setBounds(selRow.removeFromLeft(isCompact ? 160 : 220).reduced(1));
    selRow.removeFromLeft(sp);
    presetSelector.setBounds(selRow.removeFromLeft(isCompact ? 200 : 260).reduced(1));
    area.removeFromTop(sp);

    // Row 3: Delay Controls
    auto delayRow = area.removeFromTop(isCompact ? 22 : 32);
    delayEnableBtn.setBounds(delayRow.removeFromLeft(isCompact ? 90 : 110).reduced(1));
    delayRow.removeFromLeft(sp);
    delayMsSlider.setBounds(delayRow.removeFromLeft(isCompact ? 220 : 300).reduced(1));
    delayRow.removeFromLeft(sp);
    distanceSlider.setBounds(delayRow.removeFromLeft(isCompact ? 200 : 280).reduced(1));
    distFeetLabel.setBounds(delayRow.removeFromLeft(isCompact ? 60 : 80).reduced(0, 2));
    area.removeFromTop(sp);

    // Row 4: Polarity & Phase Alignment
    auto phaseRow = area.removeFromTop(isCompact ? 22 : 32);
    polarityBtn.setBounds(phaseRow.removeFromLeft(isCompact ? 110 : 160).reduced(1));
    phaseRow.removeFromLeft(sp);
    allPassEnableBtn.setBounds(phaseRow.removeFromLeft(isCompact ? 120 : 160).reduced(1));
    phaseRow.removeFromLeft(sp);
    allPassFreqSlider.setBounds(phaseRow.removeFromLeft(isCompact ? 180 : 240).reduced(1));
    phaseRow.removeFromLeft(sp);
    allPassQSlider.setBounds(phaseRow.removeFromLeft(isCompact ? 140 : 180).reduced(1));
    area.removeFromTop(sp);

    // Row 5: Trim & Mute
    auto trimRow = area.removeFromTop(isCompact ? 22 : 30);
    trimSlider.setBounds(trimRow.removeFromLeft(isCompact ? 180 : 240).reduced(1));
    trimRow.removeFromLeft(sp);
    muteBtn.setBounds(trimRow.removeFromLeft(isCompact ? 70 : 90).reduced(1));
    trimRow.removeFromLeft(sp);
    peakLabel.setBounds(trimRow.removeFromLeft(isCompact ? 110 : 150).reduced(0, 2));
    area.removeFromTop(sp);

    // Bottom overview
    overviewEditor.setBounds(area);
  }

private:
  MixerEngine &engine;
  int selectedMatrix = 0;
  bool isUpdatingUi = false;

  juce::Label title, subTitle;
  juce::Label tempLabel, humidityLabel, speedOfSoundLabel;
  juce::Slider tempSlider, humiditySlider;

  juce::ComboBox matrixSelector, presetSelector;
  juce::TextButton delayEnableBtn, polarityBtn, allPassEnableBtn, muteBtn;
  juce::Slider delayMsSlider, distanceSlider, allPassFreqSlider, allPassQSlider,
      trimSlider;
  juce::Label distFeetLabel, peakLabel;

  juce::TextEditor overviewEditor;
};

// ==============================================================================
// Phase 96: DAW 64x64 Streamer & Global Virtual Soundcheck Panel (Chapter 26
// UI)
// ==============================================================================

} // namespace nova
