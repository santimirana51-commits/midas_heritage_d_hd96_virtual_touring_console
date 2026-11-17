#pragma once
#include "GUI/ChannelAIPanel.h"
#include "Mixer/MixerEngine.h"
#include "GUI/Meter.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace nova {

class ChannelStrip final : public juce::Component, private juce::Timer {
public:
  ChannelStrip(int index, ChannelState &state, std::atomic<int> &faderModeRef,
               MixerEngine &engineRef)
      : channelIndex(index), channel(state), faderMode(faderModeRef),
        engine(engineRef), meter(state.peak) {
    const auto chName = engine.channelNames[static_cast<size_t>(index)];
    name.setText(chName.isEmpty() ? ("CH " + juce::String(index + 1)) : chName,
                 juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    name.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    name.setEditable(true);
    name.onTextChange = [this] {
      engine.channelNames[static_cast<size_t>(channelIndex)] = name.getText();
    };
    addAndMakeVisible(name);

    inputSource.addItem("OFF", 1);
    for (int input = 0; input < kInitialPhysicalInputs; ++input)
      inputSource.addItem("IN " + juce::String(input + 1), input + 2);
    inputSource.setSelectedId(
        channel.sourceInput.load() < 0 ? 1 : channel.sourceInput.load() + 2,
        juce::dontSendNotification);
    inputSource.onChange = [this] {
      const int selected = inputSource.getSelectedId();
      channel.sourceInput.store(selected <= 1 ? -1 : selected - 2,
                                std::memory_order_relaxed);
    };
    inputSource.setTooltip(
        "Patch bay: choose the physical input feeding this logical channel.");
    addAndMakeVisible(inputSource);

    vcaGroup.addItem("NO VCA", 1);
    for (int group = 1; group <= 24; ++group)
      vcaGroup.addItem("VCA " + juce::String(group), group + 1);
    vcaGroup.setSelectedId(channel.vcaGroup.load(std::memory_order_relaxed) + 1,
                           juce::dontSendNotification);
    vcaGroup.setTooltip("Assign this channel to one VCA group. VCA level "
                        "offsets channel gain.");
    vcaGroup.onChange = [this] {
      const int v = juce::jlimit(0, 24, vcaGroup.getSelectedId() - 1);
      channel.vcaGroup.store(v, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner))
        engine.channels[static_cast<size_t>(partner)].vcaGroup.store(
            v, std::memory_order_relaxed);
    };
    addAndMakeVisible(vcaGroup);

    // ── VCA Spill Button
    // ────────────────────────────────────────────────────── Lights up when
    // channel is in a VCA group; click to trigger VCA spill view
    vcaButton.setButtonText("VCA");
    vcaButton.setClickingTogglesState(false);
    vcaButton.setTooltip("VCA Group indicator. Click to spill (show) all "
                         "channels in this VCA group on fader bank.");
    vcaButton.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff1e2a38));
    vcaButton.setColour(juce::TextButton::buttonOnColourId,
                        juce::Colour(0xff00b4d8)); // Cyan = VCA active
    vcaButton.onClick = [this] {
      const int grp = channel.vcaGroup.load(std::memory_order_relaxed);
      if (grp > 0) {
        const int current =
            engine.activeSpillVca.load(std::memory_order_relaxed);
        engine.activeSpillVca.store(current == grp ? 0 : grp,
                                    std::memory_order_relaxed);
        repaint();
      }
    };
    addAndMakeVisible(vcaButton);

    // ── Channel AI Button
    // ───────────────────────────────────────────────────── Opens Acoustic
    // Profiler for this specific channel
    aiButton.setButtonText("AI");
    aiButton.setClickingTogglesState(false);
    aiButton.setTooltip("Channel AI: Acoustic Profiler & Adaptive EQ/Comp "
                        "suggestion for this channel.");
    aiButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff1b2a1b));
    aiButton.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff2ea44f)); // Green = AI active
    aiButton.onClick = [this] {
      auto *dlg = new juce::DialogWindow::LaunchOptions();
      dlg->dialogTitle = "Channel AI — CH " + juce::String(channelIndex + 1);
      dlg->dialogBackgroundColour = juce::Colour(0xff1a1f27);
      dlg->content.setOwned(new ChannelAIPanel(engine, channelIndex));
      dlg->content->setSize(460, 360);
      dlg->useNativeTitleBar = false;
      dlg->resizable = false;
      dlg->launchAsync();
    };
    addAndMakeVisible(aiButton);

    gain.setSliderStyle(juce::Slider::LinearVertical);
    gain.setRange(-60.0, 10.0, 0.1);
    gain.setValue(0.0);
    gain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 22);
    gain.setTextValueSuffix(" dB");
    gain.setColour(juce::Slider::trackColourId, juce::Colour(0xff48d597));
    gain.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff4f6f8));
    gain.onValueChange = [this] {
      const int mode = faderMode.load(std::memory_order_relaxed);
      const float val = static_cast<float>(gain.getValue());
      if (mode <= 0) {
        channel.gainDb.store(val, std::memory_order_relaxed);
        int partner = -1;
        if (engine.isChannelStereoLinked(channelIndex, &partner))
          engine.channels[static_cast<size_t>(partner)].gainDb.store(
              val, std::memory_order_relaxed);
      } else {
        const auto aIdx =
            static_cast<size_t>(juce::jlimit(1, kAuxBusCapacity, mode) - 1);
        channel.auxSendDb[aIdx].store(val, std::memory_order_relaxed);
        if (aIdx < auxSends.size())
          auxSends[aIdx].setValue(val, juce::dontSendNotification);
        int partner = -1;
        if (engine.isChannelStereoLinked(channelIndex, &partner))
          engine.channels[static_cast<size_t>(partner)].auxSendDb[aIdx].store(
              val, std::memory_order_relaxed);
        engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      }
    };
    addAndMakeVisible(gain);

    pan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setRange(-1.0, 1.0, 0.01);
    pan.setValue(0.0);
    pan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 18);
    pan.setTextValueSuffix(" PAN");
    addAndMakeVisible(pan);
    pan.onValueChange = [this] {
      const float val = static_cast<float>(pan.getValue());
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner)) {
        const int chL = (channelIndex / 2) * 2;
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
        channel.pan.store(val, std::memory_order_relaxed);
      }
    };

    mute.setButtonText("MUTE");
    solo.setButtonText("SOLO");
    mute.setClickingTogglesState(true);
    solo.setClickingTogglesState(true);
    mute.setColour(juce::TextButton::buttonOnColourId,
                   juce::Colour(0xffd64d5b));
    solo.setColour(juce::TextButton::buttonOnColourId,
                   juce::Colour(0xffffc857));
    mute.onClick = [this] {
      const bool m = mute.getToggleState();
      channel.muted.store(m, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner))
        engine.channels[static_cast<size_t>(partner)].muted.store(
            m, std::memory_order_relaxed);
    };
    solo.onClick = [this] {
      const bool s = solo.getToggleState();
      channel.solo.store(s, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner))
        engine.channels[static_cast<size_t>(partner)].solo.store(
            s, std::memory_order_relaxed);
    };
    addAndMakeVisible(mute);
    addAndMakeVisible(solo);

    highPass.setButtonText("HPF");
    highPass.setClickingTogglesState(true);
    highPass.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff397bbd));
    highPass.onClick = [this] {
      const bool h = highPass.getToggleState();
      channel.highPassEnabled.store(h, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner))
        engine.channels[static_cast<size_t>(partner)].highPassEnabled.store(
            h, std::memory_order_relaxed);
    };
    addAndMakeVisible(highPass);

    compressor.setButtonText("COMP");
    compressor.setClickingTogglesState(true);
    compressor.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour(0xff8d68c5));
    compressor.onClick = [this] {
      const bool c = compressor.getToggleState();
      channel.compressorEnabled.store(c, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner))
        engine.channels[static_cast<size_t>(partner)].compressorEnabled.store(
            c, std::memory_order_relaxed);
    };
    addAndMakeVisible(compressor);

    polarity.setButtonText(juce::CharPointer_UTF8("\xc3\x98"));
    polarity.setClickingTogglesState(true);
    polarity.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xffffa000));
    polarity.setTooltip("Polarity Invert / Phase Flip (180 deg)");
    polarity.onClick = [this] {
      const bool p = polarity.getToggleState();
      channel.polarityInverted.store(p, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelIndex, &partner))
        engine.channels[static_cast<size_t>(partner)].polarityInverted.store(
            p, std::memory_order_relaxed);
    };
    addAndMakeVisible(polarity);
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      auto &send = auxSends[static_cast<size_t>(a)];
      send.setSliderStyle(juce::Slider::LinearHorizontal);
      send.setRange(-60.0, 6.0, 0.5);
      send.setValue(channel.auxSendDb[static_cast<size_t>(a)].load(),
                    juce::dontSendNotification);
      send.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      send.setTooltip("AUX " + juce::String(a + 1) + " send level (dB)");
      send.onValueChange = [this, a] {
        const float val =
            static_cast<float>(auxSends[static_cast<size_t>(a)].getValue());
        channel.auxSendDb[static_cast<size_t>(a)].store(
            val, std::memory_order_relaxed);
        int partner = -1;
        if (engine.isChannelStereoLinked(channelIndex, &partner))
          engine.channels[static_cast<size_t>(partner)]
              .auxSendDb[static_cast<size_t>(a)]
              .store(val, std::memory_order_relaxed);
        engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      };
      addAndMakeVisible(send);

      auto &prePost = auxPrePost[static_cast<size_t>(a)];
      prePost.setButtonText("POST");
      prePost.setClickingTogglesState(true);
      prePost.setTooltip("Toggle PRE/POST fader send for AUX " +
                         juce::String(a + 1));
      prePost.onClick = [this, a] {
        const bool isPre = auxPrePost[static_cast<size_t>(a)].getToggleState();
        channel.auxPreFader[static_cast<size_t>(a)].store(
            isPre, std::memory_order_relaxed);
        auxPrePost[static_cast<size_t>(a)].setButtonText(isPre ? "PRE"
                                                               : "POST");
        int partner = -1;
        if (engine.isChannelStereoLinked(channelIndex, &partner))
          engine.channels[static_cast<size_t>(partner)]
              .auxPreFader[static_cast<size_t>(a)]
              .store(isPre, std::memory_order_relaxed);
        engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
      };
      addAndMakeVisible(prePost);
    }
    addAndMakeVisible(meter);
    startTimerHz(20);
  }

  void stopStripTimer() noexcept { stopTimer(); }

  void syncFromState() {
    const int mode = faderMode.load(std::memory_order_relaxed);
    if (mode <= 0) {
      gain.setRange(-60.0, 10.0, 0.1);
      gain.setTextValueSuffix(" dB MAIN");
      gain.setValue(channel.gainDb.load(std::memory_order_relaxed),
                    juce::dontSendNotification);
      gain.setColour(juce::Slider::trackColourId, juce::Colour(0xff48d597));
      gain.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff4f6f8));
    } else {
      const auto auxIndex =
          static_cast<size_t>(juce::jlimit(1, kAuxBusCapacity, mode) - 1);
      gain.setRange(-60.0, 6.0, 0.5);
      gain.setTextValueSuffix(" dB AUX " + juce::String(mode));
      gain.setValue(channel.auxSendDb[auxIndex].load(std::memory_order_relaxed),
                    juce::dontSendNotification);
      gain.setColour(juce::Slider::trackColourId, juce::Colour(0xffffc857));
      gain.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffe082));
    }
    pan.setEnabled(mode == 0);
    pan.setAlpha(mode == 0 ? 1.0f : 0.25f);
    const bool isLinked = engine.isChannelStereoLinked(channelIndex);
    pan.setTextValueSuffix(isLinked ? " BAL" : " PAN");
    pan.setValue(channel.pan.load(std::memory_order_relaxed),
                 juce::dontSendNotification);
    const auto chName = engine.channelNames[static_cast<size_t>(channelIndex)];
    const auto expectedLabel =
        chName.isEmpty() ? ("CH " + juce::String(channelIndex + 1)) : chName;
    if (name.getText() != expectedLabel && !name.isBeingEdited()) {
      name.setText(expectedLabel, juce::dontSendNotification);
    }
    const bool isMSafe = channel.muteSafe.load(std::memory_order_relaxed);
    const bool isSSafe = channel.soloSafe.load(std::memory_order_relaxed);
    mute.setTooltip(isMSafe ? "MUTE (M-SAFE: IMMUNE TO MG/VCA/SCENE)"
                            : "MUTE channel");
    solo.setTooltip(isSSafe ? "SOLO (S-SAFE: IMMUNE TO OTHER SOLOS/SCENE)"
                            : "SOLO channel");
    mute.setToggleState(channel.muted.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    solo.setToggleState(channel.solo.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    highPass.setToggleState(
        channel.highPassEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    const float hpfF = channel.highPassFreqHz.load(std::memory_order_relaxed);
    const int hpfS = channel.highPassSlope.load(std::memory_order_relaxed);
    highPass.setTooltip("HPF: " + juce::String(static_cast<int>(hpfF)) +
                        " Hz (" + (hpfS == 0 ? "12dB/oct" : "24dB/oct") + ")");
    compressor.setToggleState(
        channel.compressorEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    polarity.setToggleState(
        channel.polarityInverted.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    const int source = channel.sourceInput.load(std::memory_order_relaxed);
    inputSource.setSelectedId(source < 0 ? 1 : source + 2,
                              juce::dontSendNotification);
    vcaGroup.setSelectedId(channel.vcaGroup.load(std::memory_order_relaxed) + 1,
                           juce::dontSendNotification);
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      const auto index = static_cast<size_t>(a);
      auxSends[index].setValue(
          channel.auxSendDb[index].load(std::memory_order_relaxed),
          juce::dontSendNotification);
      const bool isPre =
          channel.auxPreFader[index].load(std::memory_order_relaxed);
      auxPrePost[index].setToggleState(isPre, juce::dontSendNotification);
      auxPrePost[index].setButtonText(isPre ? "PRE" : "POST");
    }
  }

  void resized() override {
    const bool compact = getWidth() < 70;
    inputSource.setJustificationType(juce::Justification::centred);
    inputSource.setTooltip("Input patch: OFF or physical input 1-8");
    mute.setButtonText(compact ? "M" : "MUTE");
    solo.setButtonText(compact ? "S" : "SOLO");
    gain.setTextBoxStyle(compact ? juce::Slider::NoTextBox
                                 : juce::Slider::TextBoxBelow,
                         false, 66, 22);
    pan.setTextBoxStyle(compact ? juce::Slider::NoTextBox
                                : juce::Slider::TextBoxBelow,
                        false, 48, 18);

    auto r = getLocalBounds().reduced(compact ? 2 : 5);
    r.removeFromTop(compact ? 16 : 18);
    name.setBounds(r.removeFromTop(compact ? 20 : 24));
    inputSource.setBounds(r.removeFromTop(compact ? 22 : 26).reduced(1));
    vcaGroup.setJustificationType(juce::Justification::centred);
    vcaGroup.setBounds(r.removeFromTop(compact ? 20 : 24).reduced(1));

    // VCA + AI row — two small buttons side-by-side below vcaGroup
    {
      auto row = r.removeFromTop(compact ? 16 : 18);
      const int halfW = row.getWidth() / 2;
      vcaButton.setBounds(row.removeFromLeft(halfW).reduced(1));
      aiButton.setBounds(row.reduced(1));
    }

    meter.setBounds(r.removeFromTop(12).reduced(compact ? 2 : 8, 0));
    r.removeFromTop(4);
    const int sendRowHeight = compact ? 13 : 16;
    for (int a = kVisibleAuxSends - 1; a >= 0; --a) {
      auto row = r.removeFromBottom(sendRowHeight);
      auxSends[static_cast<size_t>(a)].setBounds(
          row.removeFromLeft(row.getWidth() - (compact ? 22 : 30)));
      auxPrePost[static_cast<size_t>(a)].setBounds(row.reduced(1));
    }
    auto buttons2 = r.removeFromBottom(compact ? 22 : 26);
    polarity.setBounds(buttons2.removeFromLeft(compact ? 18 : 24).reduced(1));
    highPass.setButtonText(compact ? "H" : "HPF");
    compressor.setButtonText(compact ? "C" : "COMP");
    highPass.setBounds(
        buttons2.removeFromLeft(buttons2.getWidth() / 2).reduced(1));
    compressor.setBounds(buttons2.reduced(1));
    auto buttons = r.removeFromBottom(compact ? 22 : 26);
    mute.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2).reduced(1));
    solo.setBounds(buttons.reduced(1));
    pan.setBounds(r.removeFromBottom(compact ? 44 : 66));
    gain.setBounds(r.reduced(1));
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.y <= 20) {
      juce::PopupMenu menu;
      juce::PopupMenu colorSubMenu;
      static const char *const kColorNames[16] = {
          "Slate (Default)", "Red",         "Pink/Magenta", "Purple",
          "Indigo",          "Blue",        "Cyan (MIDAS)", "Teal",
          "Green",           "Light Green", "Lime",         "Yellow",
          "Amber",           "Orange",      "Brown",        "Grey"};
      for (int i = 0; i < 16; ++i) {
        colorSubMenu.addItem(100 + i, kColorNames[i]);
      }
      menu.addSubMenu("Channel Color", colorSubMenu);

      juce::PopupMenu iconSubMenu;
      for (int i = 0; i < 16; ++i) {
        iconSubMenu.addItem(200 + i, kInstrumentTags[i]);
      }
      menu.addSubMenu("Instrument Icon", iconSubMenu);

      juce::PopupMenu popSubMenu;
      popSubMenu.addItem(300, "None (No POP Group)");
      for (int i = 0; i < 24; ++i) {
        popSubMenu.addItem(301 + i, engine.popNames[static_cast<size_t>(i)]);
      }
      menu.addSubMenu("POPulation Group (MIDAS Ch 11)", popSubMenu);

      menu.showMenuAsync(
          juce::PopupMenu::Options().withTargetComponent(this),
          [this](int result) {
            if (result >= 100 && result < 116) {
              const uint32_t chosen = kMidasColors[result - 100];
              channel.colorRgb.store(chosen, std::memory_order_relaxed);
              int partner = -1;
              if (engine.isChannelStereoLinked(channelIndex, &partner))
                engine.channels[static_cast<size_t>(partner)].colorRgb.store(
                    chosen, std::memory_order_relaxed);
              repaint();
            } else if (result >= 200 && result < 216) {
              const int chosen = result - 200;
              channel.instrumentIcon.store(chosen, std::memory_order_relaxed);
              int partner = -1;
              if (engine.isChannelStereoLinked(channelIndex, &partner))
                engine.channels[static_cast<size_t>(partner)]
                    .instrumentIcon.store(chosen, std::memory_order_relaxed);
              repaint();
            } else if (result >= 300 && result <= 324) {
              const int chosen = result - 300;
              channel.popGroup.store(chosen, std::memory_order_relaxed);
              int partner = -1;
              if (engine.isChannelStereoLinked(channelIndex, &partner))
                engine.channels[static_cast<size_t>(partner)].popGroup.store(
                    chosen, std::memory_order_relaxed);
              repaint();
            }
          });
      return;
    }
    juce::Component::mouseDown(e);
  }

  void paint(juce::Graphics &g) override {
    const int mode = faderMode.load(std::memory_order_relaxed);
    const bool isSof = (mode > 0);
    const int spillVca = engine.activeSpillVca.load(std::memory_order_relaxed);
    const int spillPop = engine.activeSpillPop.load(std::memory_order_relaxed);
    const bool isVcaSpill =
        (spillVca > 0 &&
         channel.vcaGroup.load(std::memory_order_relaxed) == spillVca);
    const bool isPopSpill =
        (spillPop > 0 &&
         channel.popGroup.load(std::memory_order_relaxed) == spillPop);

    juce::Colour bgColour = juce::Colour(0xff242a31);
    juce::Colour borderColour = juce::Colour(0xff3a444e);
    float borderWidth = 1.0f;

    if (isPopSpill) {
      bgColour = juce::Colour(0xff2d172e);
      borderColour = juce::Colour(0xffd946ef); // Neon Magenta
      borderWidth = 2.0f;
    } else if (isVcaSpill) {
      bgColour = juce::Colour(0xff142834);
      borderColour = juce::Colour(0xff00b4d8); // Cyan
      borderWidth = 2.0f;
    } else if (isSof) {
      bgColour = juce::Colour(0xff2a2318);
      borderColour = juce::Colour(0xffffc857); // Amber
      borderWidth = 2.0f;
    }

    g.setColour(bgColour);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 7.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 7.0f,
                           borderWidth);

    // Chapter 12: Color Coding & Instrument Icon on Scribble Strip
    const auto bannerBounds =
        getLocalBounds().removeFromTop(18).toFloat().reduced(3.0f, 2.0f);
    const uint32_t cRgb = channel.colorRgb.load(std::memory_order_relaxed);
    g.setColour(juce::Colour(cRgb));
    g.fillRoundedRectangle(bannerBounds, 3.0f);

    const int iconIdx = std::clamp(
        channel.instrumentIcon.load(std::memory_order_relaxed), 0, 15);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(kInstrumentTags[iconIdx], bannerBounds,
               juce::Justification::centred);

    auto badgeArea =
        getLocalBounds().withTrimmedTop(18).removeFromTop(18).reduced(4, 2);

    if (isPopSpill) {
      g.setColour(juce::Colour(0xffd946ef));
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.drawText("POP " + juce::String(spillPop) + " SPILL", badgeArea,
                 juce::Justification::topRight);
    } else if (isVcaSpill) {
      g.setColour(juce::Colour(0xff00b4d8));
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.drawText("VCA " + juce::String(spillVca) + " SPILL", badgeArea,
                 juce::Justification::topRight);
    } else if (isSof) {
      g.setColour(juce::Colour(0xffffc857));
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.drawText("SOF A" + juce::String(mode), badgeArea,
                 juce::Justification::topRight);
    }

    const bool isStereo = channel.stereoLinked.load(std::memory_order_relaxed);
    if (isStereo) {
      g.setColour(juce::Colour(0xff2ea44f));
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      const juce::String linkBadge = (channelIndex % 2 == 0) ? "ST L" : "ST R";
      g.drawText(linkBadge, badgeArea, juce::Justification::topLeft);
    }

    const bool isMSafe = channel.muteSafe.load(std::memory_order_relaxed);
    const bool isSSafe = channel.soloSafe.load(std::memory_order_relaxed);
    if (isMSafe || isSSafe) {
      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      if (isMSafe && isSSafe) {
        g.setColour(juce::Colour(0xffe57373));
        g.drawText("M/S-SAFE", badgeArea, juce::Justification::centred);
      } else if (isMSafe) {
        g.setColour(juce::Colour(0xffe57373));
        g.drawText("M-SAFE", badgeArea, juce::Justification::centred);
      } else {
        g.setColour(juce::Colour(0xffffb74d));
        g.drawText("S-SAFE", badgeArea, juce::Justification::centred);
      }
    }
  }

private:
  void timerCallback() override {
    if (!isVisible())
      return;
    if (!gain.isMouseButtonDown()) {
      const float currentG = channel.gainDb.load(std::memory_order_relaxed);
      if (std::abs(static_cast<float>(gain.getValue()) - currentG) > 0.05f)
        gain.setValue(currentG, juce::dontSendNotification);
    }
    if (!pan.isMouseButtonDown()) {
      const float currentP = channel.pan.load(std::memory_order_relaxed);
      if (std::abs(static_cast<float>(pan.getValue()) - currentP) > 0.005f)
        pan.setValue(currentP, juce::dontSendNotification);
    }
    const bool m = channel.muted.load(std::memory_order_relaxed);
    if (mute.getToggleState() != m)
      mute.setToggleState(m, juce::dontSendNotification);
    const bool s = channel.solo.load(std::memory_order_relaxed);
    if (solo.getToggleState() != s)
      solo.setToggleState(s, juce::dontSendNotification);
    const bool h = channel.highPassEnabled.load(std::memory_order_relaxed);
    if (highPass.getToggleState() != h)
      highPass.setToggleState(h, juce::dontSendNotification);
    const bool c = channel.compressorEnabled.load(std::memory_order_relaxed);
    if (compressor.getToggleState() != c)
      compressor.setToggleState(c, juce::dontSendNotification);
    const bool p = channel.polarityInverted.load(std::memory_order_relaxed);
    if (polarity.getToggleState() != p)
      polarity.setToggleState(p, juce::dontSendNotification);
    repaint();
  }
  int channelIndex;
  ChannelState &channel;
  std::atomic<int> &faderMode; // 0 = Main fader, 1..96 = AUX send fader
  MixerEngine &engine;
  juce::Label name;
  juce::ComboBox inputSource, vcaGroup;
  juce::Slider gain, pan;
  std::array<juce::Slider, kVisibleAuxSends> auxSends;
  std::array<juce::TextButton, kVisibleAuxSends> auxPrePost;
  juce::TextButton mute, solo, highPass, compressor, polarity, vcaButton, aiButton;
  Meter meter;
};

} // namespace nova
