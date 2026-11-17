#pragma once
#include "Mixer/MixerEngine.h"
#include "GUI/EqPanel.h"
#include "GUI/DynamicsPanel.h"
#include "GUI/ChannelAIPanel.h"
#include "GUI/DynamicEqPanel.h"
#include "GUI/SidechainSettingsPanel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class SelectedChannelPanel final : public juce::Component, private juce::Timer {
public:
  explicit SelectedChannelPanel(MixerEngine &e, int initialChannel = 1)
      : engine(e) {
    for (int i = 1; i <= kLogicalChannels; ++i)
      channelSelector.addItem("CH " + juce::String(i), i);
    channelSelector.setSelectedId(
        juce::jlimit(1, kLogicalChannels, initialChannel),
        juce::dontSendNotification);
    addAndMakeVisible(channelSelector);
    previous.setButtonText("< PREV");
    next.setButtonText("NEXT >");
    addAndMakeVisible(previous);
    addAndMakeVisible(next);
    previous.onClick = [this] {
      channelSelector.setSelectedId(
          juce::jmax(1, channelSelector.getSelectedId() - 1));
    };
    next.onClick = [this] {
      channelSelector.setSelectedId(
          juce::jmin(kLogicalChannels, channelSelector.getSelectedId() + 1));
    };

    stereoLinkButton.setClickingTogglesState(false);
    stereoLinkButton.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff2a3440));
    stereoLinkButton.onClick = [this] {
      const int ch = channelSelector.getSelectedId() - 1;
      const int pair = ch / 2;
      const bool currentlyLinked =
          engine.stereoLink[static_cast<size_t>(pair)].load(
              std::memory_order_relaxed);
      engine.setStereoLink(pair, !currentlyLinked);
      syncFromState();
    };
    addAndMakeVisible(stereoLinkButton);

    channelAiButton.setButtonText("CHANNEL AI");
    channelAiButton.setTooltip(
        "Open Channel AI & Automatic Acoustic Profiling (Chapter 35)");
    channelAiButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff6f42c1));
    channelAiButton.onClick = [this] {
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD \xe2\x80\x94 Channel AI Profiler (CH " +
                            juce::String(channelSelector.getSelectedId()) + ")";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(
          new ChannelAIPanel(engine, channelSelector.getSelectedId() - 1));
      options.content->setSize(620, 480);
      options.launchAsync();
    };
    addAndMakeVisible(channelAiButton);
    sidechainButton.setButtonText("KEY SIDECHAIN");
    sidechainButton.setTooltip(
        "Configure internal/external detector filtering and Key Listen");
    sidechainButton.onClick = [this] {
      const int channelIndex = channelSelector.getSelectedId() - 1;
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "KEY SIDECHAIN — CH " + juce::String(channelIndex + 1);
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(
          new SidechainSettingsPanel(engine, channelIndex));
      options.content->setSize(600, 170);
      options.launchAsync();
    };
    addAndMakeVisible(sidechainButton);

    inputGain.setSliderStyle(juce::Slider::LinearVertical);
    inputGain.setRange(-20.0, 60.0, 0.5);
    inputGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    inputGain.setTextValueSuffix(" dB TRIM");
    addAndMakeVisible(inputGain);
    inputGain.onValueChange = [this] {
      const float val = static_cast<float>(inputGain.getValue());
      current().inputGainDb.store(val, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].inputGainDb.store(
            val, std::memory_order_relaxed);
    };

    delaySlider.setSliderStyle(juce::Slider::LinearVertical);
    delaySlider.setRange(0.0, 500.0, 0.1);
    delaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    delaySlider.setTextValueSuffix(" ms");
    delaySlider.setTooltip(
        "Input Delay Alignment (0-500 ms / up to 171.5 meters)");
    addAndMakeVisible(delaySlider);
    delaySlider.onValueChange = [this] {
      const float val = static_cast<float>(delaySlider.getValue());
      current().inputDelayMs.store(val, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].inputDelayMs.store(
            val, std::memory_order_relaxed);
    };

    gain.setSliderStyle(juce::Slider::LinearVertical);
    gain.setRange(-60.0, 10.0, 0.1);
    gain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    gain.setTextValueSuffix(" dB");
    addAndMakeVisible(gain);
    gain.onValueChange = [this] {
      const float val = static_cast<float>(gain.getValue());
      current().gainDb.store(val, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].gainDb.store(
            val, std::memory_order_relaxed);
    };
    pan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setRange(-1.0, 1.0, 0.01);
    pan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 22);
    pan.setTextValueSuffix(" PAN");
    addAndMakeVisible(pan);
    pan.onValueChange = [this] {
      const float val = static_cast<float>(pan.getValue());
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner)) {
        const int chL = (ch / 2) * 2;
        const int chR = chL + 1;
        float panL = -1.0f;
        float panR = 1.0f;
        if (val <= 0.0f) {
          panL = -1.0f;
          panR = std::clamp(1.0f + 2.0f * val, -1.0f, 1.0f);
        } else {
          panL = std::clamp(-1.0f + 2.0f * val, -1.0f, 1.0f);
          panR = 1.0f;
        }
        engine.channels[static_cast<size_t>(chL)].pan.store(
            panL, std::memory_order_relaxed);
        engine.channels[static_cast<size_t>(chR)].pan.store(
            panR, std::memory_order_relaxed);
      } else {
        current().pan.store(val, std::memory_order_relaxed);
      }
    };

    mute.setButtonText("MUTE");
    solo.setButtonText("SOLO");
    hpf.setButtonText("HPF");
    gate.setButtonText("GATE");
    comp.setButtonText("COMP");
    eqEditor.setButtonText("OPEN EQ");
    dynEqEditor.setButtonText("DYN EQ");
    dynEqEditor.setTooltip(
        "Open 4-Band Dynamic Parametric EQ Editor (MIDAS Chapter 16)");
    dynEqEditor.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff8957e5));
    dynamicsEditor.setButtonText("DYNAMICS...");
    addAndMakeVisible(eqEditor);
    addAndMakeVisible(dynEqEditor);
    addAndMakeVisible(dynamicsEditor);
    eqEditor.onClick = [this] {
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Parametric EQ";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(
          new EqPanel(engine, channelSelector.getSelectedId()));
      options.content->setSize(620, 300);
      options.launchAsync();
    };
    dynEqEditor.onClick = [this] {
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD — 4-Band Dynamic Parametric EQ (Chapter 16)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(
          new DynamicEqPanel(engine, channelSelector.getSelectedId()));
      options.content->setSize(920, 560);
      options.launchAsync();
    };
    dynamicsEditor.onClick = [this] {
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Channel Dynamics";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(
          new DynamicsPanel(engine, channelSelector.getSelectedId()));
      options.content->setSize(900, 480);
      options.launchAsync();
    };
    for (auto *button :
         {&mute, &muteSafeButton, &solo, &soloSafeButton, &soloBusButton,
          &soloPflButton, &polarityButton, &allPassButton, &allPassRangeButton,
          &allPassFreqModeButton, &hpf, &hpfSlope, &lpf, &lpfSlope, &gate,
          &comp, &orderButton}) {
      button->setClickingTogglesState(true);
      addAndMakeVisible(*button);
    }
    soloBusButton.setButtonText("SOLO A");
    soloBusButton.setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(0xff29b6f6));
    soloBusButton.setTooltip("Toggle Solo target bus: Solo A (Monitor A/IEM) "
                             "vs Solo B (Monitor B/Wedge)");
    soloBusButton.onClick = [this] {
      const int bus = soloBusButton.getToggleState() ? 1 : 0;
      current().soloBus.store(bus, std::memory_order_relaxed);
      soloBusButton.setButtonText(bus == 1 ? "SOLO B" : "SOLO A");
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].soloBus.store(
            bus, std::memory_order_relaxed);
    };

    soloPflButton.setButtonText("PFL");
    soloPflButton.setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(0xffab47bc));
    soloPflButton.setTooltip("Toggle Solo listen tap: PFL (Pre-Fade Listen) vs "
                             "AFL (After-Fade Listen)");
    soloPflButton.onClick = [this] {
      const bool pfl = !soloPflButton.getToggleState();
      current().soloPfl.store(pfl, std::memory_order_relaxed);
      soloPflButton.setButtonText(pfl ? "PFL" : "AFL");
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].soloPfl.store(
            pfl, std::memory_order_relaxed);
    };

    allPassButton.setButtonText("ALL-PASS");
    allPassButton.setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(0xff26a69a));
    allPassButton.setTooltip(
        "Variable Phase All-Pass Filter: continuous phase adjustment");
    allPassButton.onClick = [this] {
      const bool ap = allPassButton.getToggleState();
      current().allPassEnabled.store(ap, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].allPassEnabled.store(
            ap, std::memory_order_relaxed);
    };

    allPassRangeButton.setButtonText("180\xc2\xb0 MAX");
    allPassRangeButton.setColour(juce::TextButton::buttonOnColourId,
                                 juce::Colour(0xff00897b));
    allPassRangeButton.setTooltip("All-Pass Max Range: 180 deg vs 90 deg");
    allPassRangeButton.onClick = [this] {
      const int rng = allPassRangeButton.getToggleState() ? 0 : 1;
      current().allPassRange.store(rng, std::memory_order_relaxed);
      allPassRangeButton.setButtonText(rng == 0 ? "90\xc2\xb0 MAX"
                                                : "180\xc2\xb0 MAX");
      allPassSlider.setRange(0.0, rng == 0 ? 90.0 : 180.0, 1.0);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].allPassRange.store(
            rng, std::memory_order_relaxed);
    };

    allPassFreqModeButton.setButtonText("PHS LOW");
    allPassFreqModeButton.setColour(juce::TextButton::buttonOnColourId,
                                    juce::Colour(0xff004d40));
    allPassFreqModeButton.setTooltip(
        "All-Pass Frequency Optimization: LOW (bass/kick ~100 Hz) vs HIGH "
        "(vocal/guitar ~1.5 kHz)");
    allPassFreqModeButton.onClick = [this] {
      const int mode = allPassFreqModeButton.getToggleState() ? 1 : 0;
      current().allPassFreqMode.store(mode, std::memory_order_relaxed);
      allPassFreqModeButton.setButtonText(mode == 1 ? "PHS HIGH" : "PHS LOW");
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].allPassFreqMode.store(
            mode, std::memory_order_relaxed);
    };

    allPassSlider.setSliderStyle(juce::Slider::LinearVertical);
    allPassSlider.setRange(0.0, 180.0, 1.0);
    allPassSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    allPassSlider.setTextValueSuffix(" DEG");
    allPassSlider.setTooltip("Variable Phase Shift Degrees (0 to 180 deg)");
    addAndMakeVisible(allPassSlider);
    allPassSlider.onValueChange = [this] {
      const float val = static_cast<float>(allPassSlider.getValue());
      current().allPassDegrees.store(val, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].allPassDegrees.store(
            val, std::memory_order_relaxed);
    };
    muteSafeButton.setButtonText("M-SAFE");
    muteSafeButton.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xffe53935));
    muteSafeButton.setTooltip("Mute Safe: channel is immune to Mute Groups, "
                              "VCA Mutes, and Scene Recalls");
    muteSafeButton.onClick = [this] {
      const bool ms = muteSafeButton.getToggleState();
      current().muteSafe.store(ms, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].muteSafe.store(
            ms, std::memory_order_relaxed);
    };

    soloSafeButton.setButtonText("S-SAFE");
    soloSafeButton.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xffffa000));
    soloSafeButton.setTooltip("Solo Safe: channel remains active during other "
                              "solos and is immune to Scene Recall solos");
    soloSafeButton.onClick = [this] {
      const bool ss = soloSafeButton.getToggleState();
      current().soloSafe.store(ss, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].soloSafe.store(
            ss, std::memory_order_relaxed);
    };
    polarityButton.setButtonText(juce::String::fromUTF8("\xc3\x98 PHASE"));
    polarityButton.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xffffa000));
    polarityButton.setTooltip("Polarity Invert / 180 deg phase flip");
    polarityButton.onClick = [this] {
      const bool p = polarityButton.getToggleState();
      current().polarityInverted.store(p, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].polarityInverted.store(
            p, std::memory_order_relaxed);
    };

    orderButton.setButtonText("EQ > COMP");
    orderButton.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xff5c6bc0));
    orderButton.setTooltip(
        "Toggle processing order: EQ into Compressor or Compressor into EQ");
    orderButton.onClick = [this] {
      const bool postEq = orderButton.getToggleState();
      current().postEqDynamics.store(postEq, std::memory_order_relaxed);
      orderButton.setButtonText(postEq ? "EQ > COMP" : "COMP > EQ");
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].postEqDynamics.store(
            postEq, std::memory_order_relaxed);
    };
    mute.onClick = [this] {
      const bool m = mute.getToggleState();
      current().muted.store(m, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].muted.store(
            m, std::memory_order_relaxed);
    };
    solo.onClick = [this] {
      const bool s = solo.getToggleState();
      current().solo.store(s, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].solo.store(
            s, std::memory_order_relaxed);
    };
    hpf.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff397bbd));
    hpf.onClick = [this] {
      const bool h = hpf.getToggleState();
      current().highPassEnabled.store(h, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].highPassEnabled.store(
            h, std::memory_order_relaxed);
    };
    hpfSlope.setButtonText("HPF 12dB");
    hpfSlope.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff2962ff));
    hpfSlope.setTooltip("HPF Slope: 12 dB/oct (2nd order) vs 24 dB/oct (4th "
                        "order Butterworth)");
    hpfSlope.onClick = [this] {
      const int slope = hpfSlope.getToggleState() ? 1 : 0;
      current().highPassSlope.store(slope, std::memory_order_relaxed);
      hpfSlope.setButtonText(slope == 1 ? "HPF 24dB" : "HPF 12dB");
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].highPassSlope.store(
            slope, std::memory_order_relaxed);
    };
    hpfFreqSlider.setSliderStyle(juce::Slider::LinearVertical);
    hpfFreqSlider.setRange(20.0, 2000.0, 1.0);
    hpfFreqSlider.setSkewFactorFromMidPoint(100.0);
    hpfFreqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    hpfFreqSlider.setTextValueSuffix(" Hz HPF");
    hpfFreqSlider.setTooltip(
        "High-Pass Filter Cutoff Frequency (20 - 2000 Hz)");
    addAndMakeVisible(hpfFreqSlider);
    hpfFreqSlider.onValueChange = [this] {
      const float val = static_cast<float>(hpfFreqSlider.getValue());
      current().highPassFreqHz.store(val, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].highPassFreqHz.store(
            val, std::memory_order_relaxed);
    };

    lpf.setButtonText("LPF");
    lpf.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff009688));
    lpf.setTooltip("Low-Pass Filter Enable");
    lpf.onClick = [this] {
      const bool l = lpf.getToggleState();
      current().lowPassEnabled.store(l, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].lowPassEnabled.store(
            l, std::memory_order_relaxed);
    };
    lpfSlope.setButtonText("LPF 12dB");
    lpfSlope.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff00897b));
    lpfSlope.setTooltip("LPF Slope: 12 dB/oct (2nd order) vs 24 dB/oct (4th "
                        "order Butterworth)");
    lpfSlope.onClick = [this] {
      const int slope = lpfSlope.getToggleState() ? 1 : 0;
      current().lowPassSlope.store(slope, std::memory_order_relaxed);
      lpfSlope.setButtonText(slope == 1 ? "LPF 24dB" : "LPF 12dB");
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].lowPassSlope.store(
            slope, std::memory_order_relaxed);
    };
    lpfFreqSlider.setSliderStyle(juce::Slider::LinearVertical);
    lpfFreqSlider.setRange(1000.0, 20000.0, 10.0);
    lpfFreqSlider.setSkewFactorFromMidPoint(10000.0);
    lpfFreqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    lpfFreqSlider.setTextValueSuffix(" Hz LPF");
    lpfFreqSlider.setTooltip(
        "Low-Pass Filter Cutoff Frequency (1000 - 20000 Hz)");
    addAndMakeVisible(lpfFreqSlider);
    lpfFreqSlider.onValueChange = [this] {
      const float val = static_cast<float>(lpfFreqSlider.getValue());
      current().lowPassFreqHz.store(val, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].lowPassFreqHz.store(
            val, std::memory_order_relaxed);
    };
    gate.onClick = [this] {
      const bool g = gate.getToggleState();
      current().gateEnabled.store(g, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].gateEnabled.store(
            g, std::memory_order_relaxed);
    };
    comp.onClick = [this] {
      const bool c = comp.getToggleState();
      current().compressorEnabled.store(c, std::memory_order_relaxed);
      const int ch = channelSelector.getSelectedId() - 1;
      int partner = -1;
      if (engine.isChannelStereoLinked(ch, &partner))
        engine.channels[static_cast<size_t>(partner)].compressorEnabled.store(
            c, std::memory_order_relaxed);
    };

    duganGroupButton.setButtonText("DUGAN: OFF");
    duganGroupButton.setTooltip(
        "Dan Dugan Speech Automixer Group (Chapter 28)");
    duganGroupButton.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff2a3440));
    duganGroupButton.onClick = [this] {
      const int nextGrp =
          (current().duganGroup.load(std::memory_order_relaxed) + 1) % 4;
      current().duganGroup.store(nextGrp, std::memory_order_relaxed);
      syncFromState();
    };
    addAndMakeVisible(duganGroupButton);

    duganWeightSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    duganWeightSlider.setRange(-12.0, 12.0, 0.5);
    duganWeightSlider.setValue(0.0, juce::dontSendNotification);
    duganWeightSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55,
                                      18);
    duganWeightSlider.setTextValueSuffix(" dB WT");
    duganWeightSlider.setTooltip(
        "Dan Dugan Gain-Sharing Weight (-12 dB to +12 dB)");
    duganWeightSlider.onValueChange = [this] {
      current().duganWeightDb.store(
          static_cast<float>(duganWeightSlider.getValue()),
          std::memory_order_relaxed);
    };
    addAndMakeVisible(duganWeightSlider);

    duganGainLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    duganGainLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xff2ea44f));
    duganGainLabel.setText("DUG 0.0 dB", juce::dontSendNotification);
    addAndMakeVisible(duganGainLabel);

    for (auto *label : {&meterReadout, &clipReadout}) {
      label->setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
      label->setJustificationType(juce::Justification::centredLeft);
      addAndMakeVisible(*label);
    }
    meterReadout.setText("COMP GR 0.0 dB  •  GATE RED 0.0 dB",
                         juce::dontSendNotification);
    clipReadout.setText("RAW IN —  |  TRIMMED —", juce::dontSendNotification);
    for (int bank = 0; bank < kAuxBusCapacity / kVisibleAuxSends; ++bank)
      auxSendBankSelector.addItem(
          "AUX " + juce::String(bank * kVisibleAuxSends + 1) + "-" +
              juce::String((bank + 1) * kVisibleAuxSends),
          bank + 1);
    auxSendBankSelector.setSelectedId(1, juce::dontSendNotification);
    auxSendBankSelector.onChange = [this] { syncFromState(); };
    addAndMakeVisible(auxSendBankSelector);
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      auto &slider = sends[static_cast<size_t>(a)];
      slider.setSliderStyle(juce::Slider::LinearHorizontal);
      slider.setRange(-60.0, 6.0, 0.5);
      slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 20);
      slider.setTextValueSuffix(" dB");
      addAndMakeVisible(slider);
      slider.onValueChange = [this, a] {
        const float val =
            static_cast<float>(sends[static_cast<size_t>(a)].getValue());
        const auto busIndex = static_cast<size_t>(getSelectedAuxSendIndex(a));
        current().auxSendDb[busIndex].store(val, std::memory_order_relaxed);
        const int ch = channelSelector.getSelectedId() - 1;
        int partner = -1;
        if (engine.isChannelStereoLinked(ch, &partner))
          engine.channels[static_cast<size_t>(partner)]
              .auxSendDb[busIndex]
              .store(val, std::memory_order_relaxed);
        engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      };
      sendLabels[static_cast<size_t>(a)].setColour(juce::Label::textColourId,
                                                   juce::Colour(0xfff0f3f6));
      addAndMakeVisible(sendLabels[static_cast<size_t>(a)]);
    }
    channelSelector.onChange = [this] { syncFromState(); };
    copyFrom.setButtonText("COPY");
    pasteTo.setButtonText("PASTE TO...");
    resetChannel.setButtonText("RESET CH");
    addAndMakeVisible(copyFrom);
    addAndMakeVisible(pasteTo);
    addAndMakeVisible(resetChannel);
    for (auto *button : {&copyInput, &copyFaderPan, &copySwitches, &copyEq,
                         &copyDynamics, &copySends, &copyVca}) {
      button->setClickingTogglesState(true);
      button->setToggleState(true, juce::dontSendNotification);
      addAndMakeVisible(*button);
    }
    copyInput.setButtonText("INPUT");
    copyFaderPan.setButtonText("FADER/PAN");
    copySwitches.setButtonText("MUTE/SOLO/HPF");
    copyEq.setButtonText("EQ");
    copyDynamics.setButtonText("DYNAMICS");
    copySends.setButtonText("AUX SENDS");
    copyVca.setButtonText("VCA");
    copyFrom.onClick = [this] {
      copyBuffer = capture(current());
      copySourceChannel = channelSelector.getSelectedId();
      copyValid = true;
      status.setText("Copied CH " + juce::String(copySourceChannel),
                     juce::dontSendNotification);
    };
    pasteTo.onClick = [this] { showPasteTargetDialog(); };
    resetChannel.onClick = [this] {
      juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::WarningIcon, "Reset channel",
          "Reset channel-local settings for CH " +
              juce::String(channelSelector.getSelectedId()) +
              "? Input patch will be preserved.",
          "RESET", "CANCEL", this,
          juce::ModalCallbackFunction::create([this](int result) {
            if (result == 1)
              resetCurrentChannel();
          }));
    };
    status.setColour(juce::Label::textColourId, juce::Colour(0xff48d597));
    addAndMakeVisible(status);

    mgLabel.setText("MUTE GRP:", juce::dontSendNotification);
    mgLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(mgLabel);
    for (int g = 0; g < 8; ++g) {
      mgButtons[static_cast<size_t>(g)].setButtonText("MG " +
                                                      juce::String(g + 1));
      mgButtons[static_cast<size_t>(g)].setClickingTogglesState(true);
      mgButtons[static_cast<size_t>(g)].setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0xffe53935));
      mgButtons[static_cast<size_t>(g)].onClick = [this, g] {
        uint8_t m = current().muteGroupMask.load(std::memory_order_relaxed);
        if (mgButtons[static_cast<size_t>(g)].getToggleState())
          m |= static_cast<uint8_t>(1u << g);
        else
          m &= ~static_cast<uint8_t>(1u << g);
        current().muteGroupMask.store(m, std::memory_order_relaxed);
        const int ch = channelSelector.getSelectedId() - 1;
        int partner = -1;
        if (engine.isChannelStereoLinked(ch, &partner))
          engine.channels[static_cast<size_t>(partner)].muteGroupMask.store(
              m, std::memory_order_relaxed);
      };
      addAndMakeVisible(mgButtons[static_cast<size_t>(g)]);
    }

    setSize(900, 520);
    syncFromState();
    startTimerHz(10);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff20262d));
    g.setColour(juce::Colour(0xfff0f3f6));
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText("SELECTED CHANNEL", 16, 8, getWidth() - 32, 28,
               juce::Justification::centredLeft);
    g.setFont(juce::Font(12.0f));
    g.drawText("Core controls and AUX sends • EQ and detailed dynamics remain "
               "in their dedicated editors",
               16, getHeight() - 22, getWidth() - 32, 16,
               juce::Justification::centred);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(14);
    area.removeFromTop(32);
    auto nav = area.removeFromTop(34);
    channelSelector.setBounds(nav.removeFromLeft(170).reduced(2));
    previous.setBounds(nav.removeFromLeft(95).reduced(2));
    next.setBounds(nav.removeFromLeft(95).reduced(2));
    sidechainButton.setBounds(nav.removeFromRight(135).reduced(2));
    channelAiButton.setBounds(nav.removeFromRight(110).reduced(2));
    stereoLinkButton.setBounds(nav.removeFromRight(140).reduced(2));
    area.removeFromTop(8);
    auto copyRow = area.removeFromTop(30);
    copyFrom.setBounds(copyRow.removeFromLeft(80).reduced(2));
    pasteTo.setBounds(copyRow.removeFromLeft(110).reduced(2));
    resetChannel.setBounds(copyRow.removeFromLeft(100).reduced(2));
    status.setBounds(copyRow.reduced(3, 2));
    area.removeFromTop(3);
    auto options1 = area.removeFromTop(27);
    copyInput.setBounds(options1.removeFromLeft(75).reduced(1));
    copyFaderPan.setBounds(options1.removeFromLeft(100).reduced(1));
    copySwitches.setBounds(options1.removeFromLeft(140).reduced(1));
    copyEq.setBounds(options1.removeFromLeft(55).reduced(1));
    copyDynamics.setBounds(options1.removeFromLeft(95).reduced(1));
    copySends.setBounds(options1.removeFromLeft(95).reduced(1));
    copyVca.setBounds(options1.removeFromLeft(55).reduced(1));
    area.removeFromTop(3);
    auto mgRow = area.removeFromTop(26);
    mgLabel.setBounds(mgRow.removeFromLeft(80).reduced(2));
    const int mgBtnW = juce::jmax(1, mgRow.getWidth() / 8);
    for (int g = 0; g < 8; ++g)
      mgButtons[static_cast<size_t>(g)].setBounds(
          mgRow.removeFromLeft(mgBtnW).reduced(2));
    area.removeFromTop(4);
    auto buttons1 = area.removeFromTop(28);
    mute.setBounds(buttons1.removeFromLeft(48).reduced(1));
    muteSafeButton.setBounds(buttons1.removeFromLeft(58).reduced(1));
    solo.setBounds(buttons1.removeFromLeft(48).reduced(1));
    soloSafeButton.setBounds(buttons1.removeFromLeft(58).reduced(1));
    soloBusButton.setBounds(buttons1.removeFromLeft(64).reduced(1));
    soloPflButton.setBounds(buttons1.removeFromLeft(54).reduced(1));
    polarityButton.setBounds(buttons1.removeFromLeft(64).reduced(1));
    allPassButton.setBounds(buttons1.removeFromLeft(70).reduced(1));
    allPassRangeButton.setBounds(buttons1.removeFromLeft(70).reduced(1));
    allPassFreqModeButton.setBounds(buttons1.removeFromLeft(70).reduced(1));
    eqEditor.setBounds(buttons1.removeFromLeft(70).reduced(1));
    dynEqEditor.setBounds(buttons1.removeFromLeft(68).reduced(1));
    dynamicsEditor.setBounds(buttons1.removeFromLeft(80).reduced(1));

    area.removeFromTop(2);
    auto buttons2 = area.removeFromTop(28);
    hpf.setBounds(buttons2.removeFromLeft(48).reduced(1));
    hpfSlope.setBounds(buttons2.removeFromLeft(68).reduced(1));
    lpf.setBounds(buttons2.removeFromLeft(48).reduced(1));
    lpfSlope.setBounds(buttons2.removeFromLeft(68).reduced(1));
    gate.setBounds(buttons2.removeFromLeft(55).reduced(1));
    comp.setBounds(buttons2.removeFromLeft(55).reduced(1));
    orderButton.setBounds(buttons2.removeFromLeft(85).reduced(1));
    duganGroupButton.setBounds(buttons2.removeFromLeft(95).reduced(1));
    duganWeightSlider.setBounds(buttons2.removeFromLeft(130).reduced(1));
    duganGainLabel.setBounds(buttons2.removeFromLeft(95).reduced(1));
    area.removeFromTop(4);
    meterReadout.setBounds(area.removeFromTop(22).reduced(2));
    clipReadout.setBounds(area.removeFromTop(20).reduced(2));
    area.removeFromTop(2);
    auto lower = area.removeFromTop(area.getHeight() - 20);
    inputGain.setBounds(lower.removeFromLeft(78).reduced(2));
    delaySlider.setBounds(lower.removeFromLeft(78).reduced(2));
    allPassSlider.setBounds(lower.removeFromLeft(78).reduced(2));
    hpfFreqSlider.setBounds(lower.removeFromLeft(78).reduced(2));
    lpfFreqSlider.setBounds(lower.removeFromLeft(78).reduced(2));
    gain.setBounds(lower.removeFromLeft(78).reduced(2));
    pan.setBounds(lower.removeFromLeft(72).reduced(2));
    lower.removeFromLeft(10);
    auto sendsArea = lower;
    auxSendBankSelector.setBounds(sendsArea.removeFromTop(26).reduced(2));
    const int rowH = juce::jmax(24, sendsArea.getHeight() / kVisibleAuxSends);
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      auto row = sendsArea.removeFromTop(rowH);
      sendLabels[static_cast<size_t>(a)].setText(
          "AUX " + juce::String(getSelectedAuxSendIndex(a) + 1),
          juce::dontSendNotification);
      sendLabels[static_cast<size_t>(a)].setBounds(
          row.removeFromLeft(55).reduced(2));
      sends[static_cast<size_t>(a)].setBounds(row.reduced(2));
    }
  }

private:
  struct ChannelSnapshot {
    float inputGainDb = 0.0f, gainDb = 0.0f, pan = 0.0f;
    bool muted = false, solo = false, hpf = false;
    bool muteSafe = false, soloSafe = false;
    int soloBus = 0;
    bool soloPfl = true;
    bool allPassEnabled = false;
    float allPassDegrees = 0.0f;
    int allPassRange = 1;
    int allPassFreqMode = 0;
    float hpfFreq = 80.0f;
    int hpfSlope = 0;
    bool lpf = false;
    float lpfFreq = 12000.0f;
    int lpfSlope = 0;
    bool polarityInverted = false;
    float inputDelayMs = 0.0f;
    bool postEqDynamics = true;
    std::array<float, kAuxBusCapacity> sends{};
    std::array<bool, kAuxBusCapacity> preFader{};
    int vcaGroup = 0;
    uint8_t muteGroupMask = 0;
    std::array<float, 4> eqFrequency{100.0f, 500.0f, 2000.0f, 8000.0f};
    std::array<float, 4> eqGainDb{};
    std::array<float, 4> eqQ{1.0f, 1.0f, 1.0f, 1.0f};
    bool gateEnabled = false, compEnabled = false;
    float gateThresholdDb = -45.0f, gateRatio = 4.0f, gateAttackMs = 2.0f,
          gateReleaseMs = 120.0f;
    float compThresholdDb = -18.0f, compRatio = 4.0f, compAttackMs = 10.0f,
          compReleaseMs = 120.0f;
    int duganGroup = 0;
    float duganWeightDb = 0.0f;
    std::array<bool, 4> dynEqEnabled{};
    std::array<int, 4> dynEqMode{};
    std::array<float, 4> dynEqThresholdDb{};
    std::array<float, 4> dynEqRangeDb{};
    std::array<float, 4> dynEqRatio{};
    std::array<float, 4> dynEqKneeDb{};
    std::array<float, 4> dynEqAttackMs{};
    std::array<float, 4> dynEqReleaseMs{};
    int surroundMode = 0;
    float surroundX = 0.0f, surroundY = 1.0f, surroundZ = 0.0f;
    float surroundDivergence = 0.0f, surroundCenterPct = 100.0f,
          surroundLfeGainDb = -60.0f;
  };

  ChannelSnapshot capture(ChannelState &ch) {
    ChannelSnapshot v;
    v.inputGainDb = ch.inputGainDb.load(std::memory_order_relaxed);
    v.gainDb = ch.gainDb.load(std::memory_order_relaxed);
    v.pan = ch.pan.load(std::memory_order_relaxed);
    v.muted = ch.muted.load(std::memory_order_relaxed);
    v.solo = ch.solo.load(std::memory_order_relaxed);
    v.muteSafe = ch.muteSafe.load(std::memory_order_relaxed);
    v.soloSafe = ch.soloSafe.load(std::memory_order_relaxed);
    v.soloBus = ch.soloBus.load(std::memory_order_relaxed);
    v.soloPfl = ch.soloPfl.load(std::memory_order_relaxed);
    v.allPassEnabled = ch.allPassEnabled.load(std::memory_order_relaxed);
    v.allPassDegrees = ch.allPassDegrees.load(std::memory_order_relaxed);
    v.allPassRange = ch.allPassRange.load(std::memory_order_relaxed);
    v.allPassFreqMode = ch.allPassFreqMode.load(std::memory_order_relaxed);
    v.hpf = ch.highPassEnabled.load(std::memory_order_relaxed);
    v.hpfFreq = ch.highPassFreqHz.load(std::memory_order_relaxed);
    v.hpfSlope = ch.highPassSlope.load(std::memory_order_relaxed);
    v.lpf = ch.lowPassEnabled.load(std::memory_order_relaxed);
    v.lpfFreq = ch.lowPassFreqHz.load(std::memory_order_relaxed);
    v.lpfSlope = ch.lowPassSlope.load(std::memory_order_relaxed);
    v.polarityInverted = ch.polarityInverted.load(std::memory_order_relaxed);
    v.inputDelayMs = ch.inputDelayMs.load(std::memory_order_relaxed);
    v.postEqDynamics = ch.postEqDynamics.load(std::memory_order_relaxed);
    v.vcaGroup = ch.vcaGroup.load(std::memory_order_relaxed);
    v.muteGroupMask = ch.muteGroupMask.load(std::memory_order_relaxed);
    v.duganGroup = ch.duganGroup.load(std::memory_order_relaxed);
    v.duganWeightDb = ch.duganWeightDb.load(std::memory_order_relaxed);
    v.gateEnabled = ch.gateEnabled.load(std::memory_order_relaxed);
    v.gateThresholdDb = ch.gateThresholdDb.load(std::memory_order_relaxed);
    v.gateRatio = ch.gateRatio.load(std::memory_order_relaxed);
    v.gateAttackMs = ch.gateAttackMs.load(std::memory_order_relaxed);
    v.gateReleaseMs = ch.gateReleaseMs.load(std::memory_order_relaxed);
    v.compEnabled = ch.compressorEnabled.load(std::memory_order_relaxed);
    v.compThresholdDb =
        ch.compressorThresholdDb.load(std::memory_order_relaxed);
    v.compRatio = ch.compressorRatio.load(std::memory_order_relaxed);
    v.compAttackMs = ch.compressorAttackMs.load(std::memory_order_relaxed);
    v.compReleaseMs = ch.compressorReleaseMs.load(std::memory_order_relaxed);
    for (int a = 0; a < kAuxBusCapacity; ++a) {
      v.sends[static_cast<size_t>(a)] =
          ch.auxSendDb[static_cast<size_t>(a)].load(std::memory_order_relaxed);
      v.preFader[static_cast<size_t>(a)] =
          ch.auxPreFader[static_cast<size_t>(a)].load(
              std::memory_order_relaxed);
    }
    for (int b = 0; b < 4; ++b) {
      v.eqFrequency[static_cast<size_t>(b)] =
          ch.eqFrequency[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
      v.eqGainDb[static_cast<size_t>(b)] =
          ch.eqGainDb[static_cast<size_t>(b)].load(std::memory_order_relaxed);
      v.eqQ[static_cast<size_t>(b)] =
          ch.eqQ[static_cast<size_t>(b)].load(std::memory_order_relaxed);
      v.dynEqEnabled[static_cast<size_t>(b)] =
          ch.dynEqEnabled[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
      v.dynEqMode[static_cast<size_t>(b)] =
          ch.dynEqMode[static_cast<size_t>(b)].load(std::memory_order_relaxed);
      v.dynEqThresholdDb[static_cast<size_t>(b)] =
          ch.dynEqThresholdDb[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
      v.dynEqRangeDb[static_cast<size_t>(b)] =
          ch.dynEqRangeDb[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
      v.dynEqRatio[static_cast<size_t>(b)] =
          ch.dynEqRatio[static_cast<size_t>(b)].load(std::memory_order_relaxed);
      v.dynEqKneeDb[static_cast<size_t>(b)] =
          ch.dynEqKneeDb[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
      v.dynEqAttackMs[static_cast<size_t>(b)] =
          ch.dynEqAttackMs[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
      v.dynEqReleaseMs[static_cast<size_t>(b)] =
          ch.dynEqReleaseMs[static_cast<size_t>(b)].load(
              std::memory_order_relaxed);
    }
    v.surroundMode = ch.surroundMode.load(std::memory_order_relaxed);
    v.surroundX = ch.surroundX.load(std::memory_order_relaxed);
    v.surroundY = ch.surroundY.load(std::memory_order_relaxed);
    v.surroundZ = ch.surroundZ.load(std::memory_order_relaxed);
    v.surroundDivergence =
        ch.surroundDivergence.load(std::memory_order_relaxed);
    v.surroundCenterPct = ch.surroundCenterPct.load(std::memory_order_relaxed);
    v.surroundLfeGainDb = ch.surroundLfeGainDb.load(std::memory_order_relaxed);
    return v;
  }

  void applySnapshot(ChannelState &ch, const ChannelSnapshot &v) {
    if (copyInput.getToggleState()) {
      ch.inputGainDb.store(juce::jlimit(-20.0f, 60.0f, v.inputGainDb),
                           std::memory_order_relaxed);
      ch.polarityInverted.store(v.polarityInverted, std::memory_order_relaxed);
      ch.inputDelayMs.store(juce::jlimit(0.0f, 500.0f, v.inputDelayMs),
                            std::memory_order_relaxed);
    }
    if (copyFaderPan.getToggleState()) {
      ch.gainDb.store(juce::jlimit(-60.0f, 10.0f, v.gainDb),
                      std::memory_order_relaxed);
      ch.pan.store(juce::jlimit(-1.0f, 1.0f, v.pan), std::memory_order_relaxed);
      ch.surroundMode.store(v.surroundMode, std::memory_order_relaxed);
      ch.surroundX.store(v.surroundX, std::memory_order_relaxed);
      ch.surroundY.store(v.surroundY, std::memory_order_relaxed);
      ch.surroundZ.store(v.surroundZ, std::memory_order_relaxed);
      ch.surroundDivergence.store(v.surroundDivergence,
                                  std::memory_order_relaxed);
      ch.surroundCenterPct.store(v.surroundCenterPct,
                                 std::memory_order_relaxed);
      ch.surroundLfeGainDb.store(v.surroundLfeGainDb,
                                 std::memory_order_relaxed);
    }
    if (copySwitches.getToggleState()) {
      ch.muted.store(v.muted, std::memory_order_relaxed);
      ch.solo.store(v.solo, std::memory_order_relaxed);
      ch.muteSafe.store(v.muteSafe, std::memory_order_relaxed);
      ch.soloSafe.store(v.soloSafe, std::memory_order_relaxed);
      ch.soloBus.store(v.soloBus, std::memory_order_relaxed);
      ch.soloPfl.store(v.soloPfl, std::memory_order_relaxed);
      ch.allPassEnabled.store(v.allPassEnabled, std::memory_order_relaxed);
      ch.allPassDegrees.store(v.allPassDegrees, std::memory_order_relaxed);
      ch.allPassRange.store(v.allPassRange, std::memory_order_relaxed);
      ch.allPassFreqMode.store(v.allPassFreqMode, std::memory_order_relaxed);
      ch.highPassEnabled.store(v.hpf, std::memory_order_relaxed);
      ch.highPassFreqHz.store(juce::jlimit(20.0f, 2000.0f, v.hpfFreq),
                              std::memory_order_relaxed);
      ch.highPassSlope.store(juce::jlimit(0, 1, v.hpfSlope),
                             std::memory_order_relaxed);
      ch.lowPassEnabled.store(v.lpf, std::memory_order_relaxed);
      ch.lowPassFreqHz.store(juce::jlimit(1000.0f, 20000.0f, v.lpfFreq),
                             std::memory_order_relaxed);
      ch.lowPassSlope.store(juce::jlimit(0, 1, v.lpfSlope),
                            std::memory_order_relaxed);
      ch.muteGroupMask.store(v.muteGroupMask, std::memory_order_relaxed);
      ch.duganGroup.store(juce::jlimit(0, 3, v.duganGroup),
                          std::memory_order_relaxed);
      ch.duganWeightDb.store(juce::jlimit(-12.0f, 12.0f, v.duganWeightDb),
                             std::memory_order_relaxed);
    }
    if (copyEq.getToggleState())
      for (int b = 0; b < 4; ++b) {
        const auto bIdx = static_cast<size_t>(b);
        ch.eqFrequency[bIdx].store(v.eqFrequency[bIdx],
                                   std::memory_order_relaxed);
        ch.eqGainDb[bIdx].store(v.eqGainDb[bIdx], std::memory_order_relaxed);
        ch.eqQ[bIdx].store(v.eqQ[bIdx], std::memory_order_relaxed);
        ch.dynEqEnabled[bIdx].store(v.dynEqEnabled[bIdx],
                                    std::memory_order_relaxed);
        ch.dynEqMode[bIdx].store(v.dynEqMode[bIdx], std::memory_order_relaxed);
        ch.dynEqThresholdDb[bIdx].store(v.dynEqThresholdDb[bIdx],
                                        std::memory_order_relaxed);
        ch.dynEqRangeDb[bIdx].store(v.dynEqRangeDb[bIdx],
                                    std::memory_order_relaxed);
        ch.dynEqRatio[bIdx].store(v.dynEqRatio[bIdx],
                                  std::memory_order_relaxed);
        ch.dynEqKneeDb[bIdx].store(v.dynEqKneeDb[bIdx],
                                   std::memory_order_relaxed);
        ch.dynEqAttackMs[bIdx].store(v.dynEqAttackMs[bIdx],
                                     std::memory_order_relaxed);
        ch.dynEqReleaseMs[bIdx].store(v.dynEqReleaseMs[bIdx],
                                      std::memory_order_relaxed);
      }
    if (copyDynamics.getToggleState()) {
      ch.gateEnabled.store(v.gateEnabled, std::memory_order_relaxed);
      ch.gateThresholdDb.store(v.gateThresholdDb, std::memory_order_relaxed);
      ch.gateRatio.store(v.gateRatio, std::memory_order_relaxed);
      ch.gateAttackMs.store(v.gateAttackMs, std::memory_order_relaxed);
      ch.gateReleaseMs.store(v.gateReleaseMs, std::memory_order_relaxed);
      ch.compressorEnabled.store(v.compEnabled, std::memory_order_relaxed);
      ch.compressorThresholdDb.store(v.compThresholdDb,
                                     std::memory_order_relaxed);
      ch.compressorRatio.store(v.compRatio, std::memory_order_relaxed);
      ch.compressorAttackMs.store(v.compAttackMs, std::memory_order_relaxed);
      ch.compressorReleaseMs.store(v.compReleaseMs, std::memory_order_relaxed);
    }
    if (copyDynamics.getToggleState() || copyEq.getToggleState()) {
      ch.postEqDynamics.store(v.postEqDynamics, std::memory_order_relaxed);
    }
    if (copySends.getToggleState())
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        ch.auxSendDb[static_cast<size_t>(a)].store(
            v.sends[static_cast<size_t>(a)], std::memory_order_relaxed);
        ch.auxPreFader[static_cast<size_t>(a)].store(
            v.preFader[static_cast<size_t>(a)], std::memory_order_relaxed);
      }
    if (copyVca.getToggleState())
      ch.vcaGroup.store(juce::jlimit(0, 24, v.vcaGroup),
                        std::memory_order_relaxed);
  }

  void showPasteTargetDialog() {
    if (!copyValid) {
      status.setText("COPY a source channel first", juce::dontSendNotification);
      return;
    }
    auto *chooser = new juce::AlertWindow(
        "Paste Channel Settings",
        "Choose target channel (1-144):", juce::AlertWindow::NoIcon);
    chooser->addTextEditor(
        "target",
        juce::String(juce::jmin(kLogicalChannels, copySourceChannel + 1)),
        "Target channel");
    chooser->addButton("PASTE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    chooser->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    chooser->enterModalState(
        true, juce::ModalCallbackFunction::create([this, chooser](int result) {
          if (result == 1) {
            const int target = juce::jlimit(
                1, kLogicalChannels,
                chooser->getTextEditorContents("target").getIntValue());
            if (target == copySourceChannel)
              status.setText("Target equals source; nothing changed",
                             juce::dontSendNotification);
            else {
              applySnapshot(engine.channels[static_cast<size_t>(target - 1)],
                            copyBuffer);
              engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
              engine.syncAuxRoutingSnapshot();
              status.setText("Pasted CH " + juce::String(copySourceChannel) +
                                 " -> CH " + juce::String(target),
                             juce::dontSendNotification);
            }
          }
          chooser->exitModalState(result);
          delete chooser;
        }),
        true);
  }

  void resetCurrentChannel() {
    auto &ch = current();
    ch.inputGainDb.store(0.0f, std::memory_order_relaxed);
    ch.polarityInverted.store(false, std::memory_order_relaxed);
    ch.inputDelayMs.store(0.0f, std::memory_order_relaxed);
    ch.postEqDynamics.store(true, std::memory_order_relaxed);
    ch.gainDb.store(0.0f, std::memory_order_relaxed);
    ch.pan.store(0.0f, std::memory_order_relaxed);
    ch.muted.store(false, std::memory_order_relaxed);
    ch.solo.store(false, std::memory_order_relaxed);
    ch.muteSafe.store(false, std::memory_order_relaxed);
    ch.soloSafe.store(false, std::memory_order_relaxed);
    ch.soloBus.store(0, std::memory_order_relaxed);
    ch.soloPfl.store(true, std::memory_order_relaxed);
    ch.allPassEnabled.store(false, std::memory_order_relaxed);
    ch.allPassDegrees.store(0.0f, std::memory_order_relaxed);
    ch.allPassRange.store(1, std::memory_order_relaxed);
    ch.allPassFreqMode.store(0, std::memory_order_relaxed);
    ch.highPassEnabled.store(false, std::memory_order_relaxed);
    ch.highPassFreqHz.store(80.0f, std::memory_order_relaxed);
    ch.highPassSlope.store(0, std::memory_order_relaxed);
    ch.lowPassEnabled.store(false, std::memory_order_relaxed);
    ch.lowPassFreqHz.store(12000.0f, std::memory_order_relaxed);
    ch.lowPassSlope.store(0, std::memory_order_relaxed);
    ch.gateEnabled.store(false, std::memory_order_relaxed);
    ch.gateThresholdDb.store(-45.0f, std::memory_order_relaxed);
    ch.gateRatio.store(4.0f, std::memory_order_relaxed);
    ch.gateAttackMs.store(2.0f, std::memory_order_relaxed);
    ch.gateReleaseMs.store(120.0f, std::memory_order_relaxed);
    ch.compressorEnabled.store(false, std::memory_order_relaxed);
    ch.compressorThresholdDb.store(-18.0f, std::memory_order_relaxed);
    ch.compressorRatio.store(4.0f, std::memory_order_relaxed);
    ch.compressorAttackMs.store(10.0f, std::memory_order_relaxed);
    ch.compressorReleaseMs.store(120.0f, std::memory_order_relaxed);
    ch.vcaGroup.store(0, std::memory_order_relaxed);
    ch.muteGroupMask.store(0, std::memory_order_relaxed);
    ch.duganGroup.store(0, std::memory_order_relaxed);
    ch.duganWeightDb.store(0.0f, std::memory_order_relaxed);
    ch.duganGainDb.store(0.0f, std::memory_order_relaxed);
    for (int a = 0; a < kAuxBusCapacity; ++a) {
      ch.auxSendDb[static_cast<size_t>(a)].store(-60.0f,
                                                 std::memory_order_relaxed);
      ch.auxPreFader[static_cast<size_t>(a)].store(false,
                                                   std::memory_order_relaxed);
    }
    engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
    engine.syncAuxRoutingSnapshot();
    engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
    engine.syncMatrixRoutingSnapshot();
    for (int b = 0; b < 4; ++b) {
      const std::array<float, 4> freq{100.0f, 500.0f, 2000.0f, 8000.0f};
      ch.eqFrequency[static_cast<size_t>(b)].store(freq[static_cast<size_t>(b)],
                                                   std::memory_order_relaxed);
      ch.eqGainDb[static_cast<size_t>(b)].store(0.0f,
                                                std::memory_order_relaxed);
      ch.eqQ[static_cast<size_t>(b)].store(1.0f, std::memory_order_relaxed);
      ch.dynEqEnabled[static_cast<size_t>(b)].store(false,
                                                    std::memory_order_relaxed);
      ch.dynEqMode[static_cast<size_t>(b)].store(0, std::memory_order_relaxed);
      ch.dynEqThresholdDb[static_cast<size_t>(b)].store(
          -20.0f, std::memory_order_relaxed);
      ch.dynEqRangeDb[static_cast<size_t>(b)].store(-6.0f,
                                                    std::memory_order_relaxed);
      ch.dynEqRatio[static_cast<size_t>(b)].store(4.0f,
                                                  std::memory_order_relaxed);
      ch.dynEqKneeDb[static_cast<size_t>(b)].store(3.0f,
                                                   std::memory_order_relaxed);
      ch.dynEqAttackMs[static_cast<size_t>(b)].store(10.0f,
                                                     std::memory_order_relaxed);
      ch.dynEqReleaseMs[static_cast<size_t>(b)].store(
          100.0f, std::memory_order_relaxed);
      ch.dynEqGainDb[static_cast<size_t>(b)].store(0.0f,
                                                   std::memory_order_relaxed);
    }
    // Preserve source patch and VCA master/group settings; reset only
    // channel-local processing.
    syncFromState();
    status.setText("Reset CH " + juce::String(channelSelector.getSelectedId()),
                   juce::dontSendNotification);
  }

  ChannelState &current() {
    const int c = juce::jlimit(0, kLogicalChannels - 1,
                               channelSelector.getSelectedId() - 1);
    return engine.channels[static_cast<size_t>(c)];
  }

  int getSelectedAuxSendIndex(int visibleSend) const noexcept {
    const int bank = juce::jlimit(0, kAuxBusCapacity / kVisibleAuxSends - 1,
                                  auxSendBankSelector.getSelectedId() - 1);
    return bank * kVisibleAuxSends + visibleSend;
  }

  void syncFromState() {
    auto &s = current();
    if (!inputGain.isMouseButtonDown())
      inputGain.setValue(s.inputGainDb.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    polarityButton.setToggleState(
        s.polarityInverted.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    if (!delaySlider.isMouseButtonDown())
      delaySlider.setValue(s.inputDelayMs.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    const bool postEq = s.postEqDynamics.load(std::memory_order_relaxed);
    orderButton.setToggleState(postEq, juce::dontSendNotification);
    orderButton.setButtonText(postEq ? "EQ > COMP" : "COMP > EQ");
    if (!gain.isMouseButtonDown())
      gain.setValue(s.gainDb.load(std::memory_order_relaxed),
                    juce::dontSendNotification);
    if (!pan.isMouseButtonDown())
      pan.setValue(s.pan.load(std::memory_order_relaxed),
                   juce::dontSendNotification);
    mute.setToggleState(s.muted.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    muteSafeButton.setToggleState(s.muteSafe.load(std::memory_order_relaxed),
                                  juce::dontSendNotification);
    solo.setToggleState(s.solo.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    soloSafeButton.setToggleState(s.soloSafe.load(std::memory_order_relaxed),
                                  juce::dontSendNotification);

    const int sb = s.soloBus.load(std::memory_order_relaxed);
    soloBusButton.setToggleState(sb == 1, juce::dontSendNotification);
    soloBusButton.setButtonText(sb == 1 ? "SOLO B" : "SOLO A");

    const bool pfl = s.soloPfl.load(std::memory_order_relaxed);
    soloPflButton.setToggleState(!pfl, juce::dontSendNotification);
    soloPflButton.setButtonText(pfl ? "PFL" : "AFL");

    const bool ap = s.allPassEnabled.load(std::memory_order_relaxed);
    allPassButton.setToggleState(ap, juce::dontSendNotification);

    const int apRng = s.allPassRange.load(std::memory_order_relaxed);
    allPassRangeButton.setToggleState(apRng == 0, juce::dontSendNotification);
    allPassRangeButton.setButtonText(apRng == 0 ? "90\xc2\xb0 MAX"
                                                : "180\xc2\xb0 MAX");
    allPassSlider.setRange(0.0, apRng == 0 ? 90.0 : 180.0, 1.0);

    const int apMode = s.allPassFreqMode.load(std::memory_order_relaxed);
    allPassFreqModeButton.setToggleState(apMode == 1,
                                         juce::dontSendNotification);
    allPassFreqModeButton.setButtonText(apMode == 1 ? "PHS HIGH" : "PHS LOW");

    allPassSlider.setValue(s.allPassDegrees.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    hpf.setToggleState(s.highPassEnabled.load(std::memory_order_relaxed),
                       juce::dontSendNotification);
    hpfFreqSlider.setValue(s.highPassFreqHz.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    const int hSlope = s.highPassSlope.load(std::memory_order_relaxed);
    hpfSlope.setToggleState(hSlope == 1, juce::dontSendNotification);
    hpfSlope.setButtonText(hSlope == 1 ? "HPF 24dB" : "HPF 12dB");

    lpf.setToggleState(s.lowPassEnabled.load(std::memory_order_relaxed),
                       juce::dontSendNotification);
    lpfFreqSlider.setValue(s.lowPassFreqHz.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    const int lSlope = s.lowPassSlope.load(std::memory_order_relaxed);
    lpfSlope.setToggleState(lSlope == 1, juce::dontSendNotification);
    lpfSlope.setButtonText(lSlope == 1 ? "LPF 24dB" : "LPF 12dB");
    gate.setToggleState(s.gateEnabled.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    comp.setToggleState(s.compressorEnabled.load(std::memory_order_relaxed),
                        juce::dontSendNotification);

    const int dg = s.duganGroup.load(std::memory_order_relaxed);
    duganGroupButton.setToggleState(dg > 0, juce::dontSendNotification);
    duganGroupButton.setButtonText(
        dg == 0 ? "DUGAN: OFF" : ("DUGAN: GRP " + juce::String(dg)));
    duganGroupButton.setColour(juce::TextButton::buttonColourId,
                               dg > 0 ? juce::Colour(0xff238636)
                                      : juce::Colour(0xff2a3440));
    duganWeightSlider.setValue(s.duganWeightDb.load(std::memory_order_relaxed),
                               juce::dontSendNotification);
    const float dGain = s.duganGainDb.load(std::memory_order_relaxed);
    duganGainLabel.setText(juce::String("DUG ") + (dGain >= 0.0f ? "+" : "") +
                               juce::String(dGain, 1) + " dB",
                           juce::dontSendNotification);
    duganGainLabel.setColour(juce::Label::textColourId,
                             dg > 0 ? (dGain > -1.0f ? juce::Colour(0xff58a6ff)
                                                     : juce::Colour(0xff2ea44f))
                                    : juce::Colour(0xff8b949e));
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      const int busIndex = getSelectedAuxSendIndex(a);
      sendLabels[static_cast<size_t>(a)].setText(
          "AUX " + juce::String(busIndex + 1), juce::dontSendNotification);
      if (!sends[static_cast<size_t>(a)].isMouseButtonDown()) {
        sends[static_cast<size_t>(a)].setValue(
            s.auxSendDb[static_cast<size_t>(busIndex)].load(
                std::memory_order_relaxed),
            juce::dontSendNotification);
      }
    }

    const float compGR =
        s.compressorGainReductionDb.load(std::memory_order_relaxed);
    const float gateGR = s.gateReductionDb.load(std::memory_order_relaxed);
    meterReadout.setText("COMP GR " + juce::String(compGR, 1) +
                             " dB  •  GATE RED " + juce::String(gateGR, 1) +
                             " dB",
                         juce::dontSendNotification);
    const float rawPeak = s.inputPeak.load(std::memory_order_relaxed);
    const float trimmedPeak = s.trimmedPeak.load(std::memory_order_relaxed);
    const bool rawClipping = rawPeak >= 1.0f;
    const bool trimmedClipping = trimmedPeak >= 1.0f;
    const auto peakDbText = [](float peak) {
      return peak <= 1.0e-6f
                 ? juce::String("-inf")
                 : juce::String(juce::Decibels::gainToDecibels(peak), 1);
    };
    clipReadout.setText(
        "RAW IN " + peakDbText(rawPeak) + " dBFS  |  TRIMMED " +
            peakDbText(trimmedPeak) + " dBFS" +
            ((rawClipping || trimmedClipping) ? "  •  CLIP" : ""),
        juce::dontSendNotification);
    clipReadout.setColour(juce::Label::textColourId,
                          (rawClipping || trimmedClipping)
                              ? juce::Colour(0xfff05a67)
                              : juce::Colour(0xfff0f3f6));
    previous.setEnabled(channelSelector.getSelectedId() > 1);
    next.setEnabled(channelSelector.getSelectedId() < kLogicalChannels);
    const uint8_t mgMask = s.muteGroupMask.load(std::memory_order_relaxed);
    for (int g = 0; g < 8; ++g)
      mgButtons[static_cast<size_t>(g)].setToggleState(
          (mgMask & (1u << g)) != 0, juce::dontSendNotification);

    const int ch = channelSelector.getSelectedId() - 1;
    const int pair = ch / 2;
    int partner = -1;
    const bool linked = engine.isChannelStereoLinked(ch, &partner);
    if (linked) {
      stereoLinkButton.setButtonText("LINKED (CH " +
                                     juce::String(pair * 2 + 1) + "+" +
                                     juce::String(pair * 2 + 2) + ")");
      stereoLinkButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff2ea44f));
      pan.setTextValueSuffix(" BAL");
    } else {
      stereoLinkButton.setButtonText("LINK WITH CH " +
                                     juce::String(partner + 1));
      stereoLinkButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff2a3440));
      pan.setTextValueSuffix(" PAN");
    }
  }

  void timerCallback() override { syncFromState(); }

  MixerEngine &engine;
  juce::ComboBox channelSelector;
  juce::TextButton previous, next, sidechainButton, mute, muteSafeButton, solo,
      soloSafeButton, soloBusButton, soloPflButton, polarityButton,
      allPassButton, allPassRangeButton, allPassFreqModeButton, hpf, hpfSlope,
      lpf, lpfSlope, gate, comp, eqEditor, dynEqEditor, dynamicsEditor,
      stereoLinkButton, orderButton, channelAiButton, duganGroupButton;
  juce::Slider inputGain, delaySlider, allPassSlider, hpfFreqSlider,
      lpfFreqSlider, gain, pan, duganWeightSlider;
  juce::Label meterReadout, clipReadout, status, duganGainLabel;
  juce::Label mgLabel;
  std::array<juce::TextButton, 8> mgButtons;
  juce::TextButton copyFrom, pasteTo, resetChannel;
  juce::ToggleButton copyInput, copyFaderPan, copySwitches, copyEq,
      copyDynamics, copySends, copyVca;
  juce::ComboBox auxSendBankSelector;
  ChannelSnapshot copyBuffer;
  int copySourceChannel = 0;
  bool copyValid = false;
  std::array<juce::Slider, kVisibleAuxSends> sends;
  std::array<juce::Label, kVisibleAuxSends> sendLabels;
};

} // namespace nova
