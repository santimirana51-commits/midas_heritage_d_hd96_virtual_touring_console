#pragma once

#include "GUI/NovaRotaryKnob.h"
#include "Mixer/MixerEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace nova {

// ==============================================================================
// MELDA MCOMPRESSOR-STYLE HIGH-PRECISION DYNAMICS PROCESSOR (COMP & GATE)
// ==============================================================================
class DynamicsPanel final : public juce::Component, private juce::Timer {
public:
  explicit DynamicsPanel(MixerEngine &e, int initialChannel = 1) : engine(e) {
    setOpaque(true);

    // Hidden backwards-compatible channel selector
    channelSelector.addItem("CH 1", 1);
    for (int i = 2; i <= kLogicalChannels; ++i)
      channelSelector.addItem("CH " + juce::String(i), i);
    channelSelector.setSelectedId(
        juce::jlimit(1, kLogicalChannels, initialChannel),
        juce::dontSendNotification);
    channelSelector.onChange = [this] { syncFromChannel(); };
    addChildComponent(channelSelector);

    // -------------------------------------------------------------------------
    // 1. TOOLBAR / HEADER CONTROLS
    // -------------------------------------------------------------------------
    btnPresetMenu.setButtonText("Default");
    btnPresetMenu.setTooltip("Select factory or custom compressor presets");
    btnPresetMenu.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff182230));
    btnPresetMenu.setColour(juce::TextButton::textColourOffId,
                            juce::Colour(0xfff1f5f9));
    btnPresetMenu.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(btnPresetMenu);

    btnPrevPreset.setButtonText("<");
    btnPrevPreset.setTooltip("Previous preset");
    btnPrevPreset.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff182230));
    btnPrevPreset.onClick = [this] { stepPreset(-1); };
    addAndMakeVisible(btnPrevPreset);

    btnNextPreset.setButtonText(">");
    btnNextPreset.setTooltip("Next preset");
    btnNextPreset.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff182230));
    btnNextPreset.onClick = [this] { stepPreset(1); };
    addAndMakeVisible(btnNextPreset);

    btnRandom.setButtonText("🎲");
    btnRandom.setTooltip("Generate musical random compressor settings");
    btnRandom.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff182230));
    btnRandom.onClick = [this] { applyRandomSettings(); };
    addAndMakeVisible(btnRandom);

    btnAddPreset.setButtonText("+");
    btnAddPreset.setTooltip("Save current parameters as custom user preset");
    btnAddPreset.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xff182230));
    btnAddPreset.onClick = [this] { saveUserPreset(); };
    addAndMakeVisible(btnAddPreset);

    btnAbCompare.setButtonText("A/B");
    btnAbCompare.setClickingTogglesState(true);
    btnAbCompare.setTooltip("Toggle between parameter Slot A and Slot B");
    btnAbCompare.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xff182230));
    btnAbCompare.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xff0284c7));
    btnAbCompare.onClick = [this] { toggleAbCompare(); };
    addAndMakeVisible(btnAbCompare);

    btnUndo.setButtonText("Undo");
    btnUndo.setTooltip("Undo last parameter adjustment");
    btnUndo.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xff182230));
    btnUndo.onClick = [this] { performUndo(); };
    addAndMakeVisible(btnUndo);

    btnRedo.setButtonText("Redo");
    btnRedo.setTooltip("Redo parameter adjustment");
    btnRedo.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xff182230));
    btnRedo.onClick = [this] { performRedo(); };
    addAndMakeVisible(btnRedo);

    btnBypass.setButtonText("Bypass");
    btnBypass.setClickingTogglesState(true);
    btnBypass.setTooltip("Bypass compressor processing");
    btnBypass.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff182230));
    btnBypass.setColour(juce::TextButton::buttonOnColourId,
                        juce::Colour(0xffdc2626));
    btnBypass.onClick = [this] {
      const bool bypassed = btnBypass.getToggleState();
      current().compressorEnabled.store(!bypassed, std::memory_order_relaxed);
      syncStereoPartner([bypassed](ChannelState &p) {
        p.compressorEnabled.store(!bypassed, std::memory_order_relaxed);
      });
      pushUndoSnapshot();
      repaint();
    };
    addAndMakeVisible(btnBypass);

    btnSettings.setButtonText("Settings");
    btnSettings.setTooltip("Dynamics DSP routing and topology settings");
    btnSettings.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff182230));
    btnSettings.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible(btnSettings);

    btnHelp.setButtonText("?");
    btnHelp.setTooltip("Help and guide for Dynamics processor");
    btnHelp.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xff182230));
    btnHelp.onClick = [this] { showHelpDialog(); };
    addAndMakeVisible(btnHelp);

    // -------------------------------------------------------------------------
    // 2. MAIN 8 COMPRESSOR ROTARY KNOBS
    // -------------------------------------------------------------------------
    // INPUT GAIN (-24 to +24 dB)
    knobInput = std::make_unique<NovaRotaryKnob>(
        "INPUT", "dB", -24.0f, 24.0f, 0.0f, false, juce::Colour(0xffe2e8f0));
    knobInput->setDisplayDecimals(2);
    knobInput->onValueChanged = [this](float v) {
      current().compressorInputGainDb.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorInputGainDb.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobInput);

    // OUTPUT MAKEUP GAIN (-24 to +24 dB)
    knobOutput = std::make_unique<NovaRotaryKnob>(
        "OUTPUT", "dB", -24.0f, 24.0f, 0.0f, false, juce::Colour(0xffe2e8f0));
    knobOutput->setDisplayDecimals(2);
    knobOutput->onValueChanged = [this](float v) {
      current().compressorMakeupGainDb.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorMakeupGainDb.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobOutput);

    // ATTACK (0.1 ms to 500 ms)
    knobAttack = std::make_unique<NovaRotaryKnob>(
        "ATTACK", "ms", 0.1f, 500.0f, 10.0f, false, juce::Colour(0xffe2e8f0));
    knobAttack->setLogarithmic(true);
    knobAttack->setDisplayDecimals(1);
    knobAttack->onValueChanged = [this](float v) {
      current().compressorAttackMs.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorAttackMs.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobAttack);

    // RELEASE (5 ms to 2000 ms)
    knobRelease = std::make_unique<NovaRotaryKnob>(
        "RELEASE", "ms", 5.0f, 2000.0f, 50.0f, false, juce::Colour(0xffe2e8f0));
    knobRelease->setLogarithmic(true);
    knobRelease->setDisplayDecimals(0);
    knobRelease->onValueChanged = [this](float v) {
      current().compressorReleaseMs.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorReleaseMs.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobRelease);

    // RMS WINDOW (0.0 ms to 100 ms)
    knobRms = std::make_unique<NovaRotaryKnob>("RMS", "ms", 0.0f, 100.0f, 1.0f,
                                               false, juce::Colour(0xffe2e8f0));
    knobRms->setDisplayDecimals(1);
    knobRms->onValueChanged = [this](float v) {
      current().compressorRmsMs.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorRmsMs.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobRms);

    // THRESHOLD (-60.0 to 0.0 dB)
    knobThreshold = std::make_unique<NovaRotaryKnob>("THRESHOLD", "dB", -60.0f,
                                                     0.0f, -12.6f, false,
                                                     juce::Colour(0xfff59e0b));
    knobThreshold->setDisplayDecimals(1);
    knobThreshold->onValueChanged = [this](float v) {
      current().compressorThresholdDb.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorThresholdDb.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobThreshold);

    // RATIO (1.0:1 to 20.0:1 / Inf)
    knobRatio = std::make_unique<NovaRotaryKnob>(
        "RATIO", ":1", 1.0f, 20.0f, 4.74f, false, juce::Colour(0xfff59e0b));
    knobRatio->setDisplayDecimals(2);
    knobRatio->onValueChanged = [this](float v) {
      current().compressorRatio.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.compressorRatio.store(v, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobRatio);

    // KNEE SIZE (0.0% to 100.0%)
    knobKnee = std::make_unique<NovaRotaryKnob>(
        "KNEE SIZE", "%", 0.0f, 100.0f, 25.0f, false, juce::Colour(0xffe2e8f0));
    knobKnee->setDisplayDecimals(1);
    knobKnee->onValueChanged = [this](float v) {
      const float kneeDb = (v * 0.01f) * 24.0f;
      current().compressorKneeDb.store(kneeDb, std::memory_order_relaxed);
      syncStereoPartner([kneeDb](ChannelState &p) {
        p.compressorKneeDb.store(kneeDb, std::memory_order_relaxed);
      });
      repaint();
    };
    addAndMakeVisible(*knobKnee);

    // -------------------------------------------------------------------------
    // 3. KNEE MODE SELECTOR (Hard, Linear, Soft)
    // -------------------------------------------------------------------------
    static const char *kneeNames[3] = {"Hard", "Linear", "Soft"};
    for (int i = 0; i < 3; ++i) {
      btnKneeMode[i] = std::make_unique<juce::TextButton>(kneeNames[i]);
      btnKneeMode[i]->setClickingTogglesState(true);
      btnKneeMode[i]->setRadioGroupId(2001);
      btnKneeMode[i]->setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff182230));
      btnKneeMode[i]->setColour(juce::TextButton::buttonOnColourId,
                                juce::Colour(0xffd97706)); // Golden highlight
      btnKneeMode[i]->setColour(juce::TextButton::textColourOnId,
                                juce::Colours::white);
      btnKneeMode[i]->setColour(juce::TextButton::textColourOffId,
                                juce::Colour(0xff94a3b8));
      addAndMakeVisible(*btnKneeMode[i]);
      const int kIdx = i;
      btnKneeMode[i]->onClick = [this, kIdx] {
        current().compressorKneeMode.store(kIdx, std::memory_order_relaxed);
        syncStereoPartner([kIdx](ChannelState &p) {
          p.compressorKneeMode.store(kIdx, std::memory_order_relaxed);
        });
        repaint();
      };
    }
    btnKneeMode[2]->setToggleState(true,
                                   juce::dontSendNotification); // Default: Soft

    // -------------------------------------------------------------------------
    // 4. ADDITIONAL OPTIONS (Link channels, Maximize to 0dB, Custom shape)
    // -------------------------------------------------------------------------
    btnLinkChannels.setButtonText("Link channels");
    btnLinkChannels.setClickingTogglesState(true);
    btnLinkChannels.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff182230));
    btnLinkChannels.setColour(juce::TextButton::buttonOnColourId,
                              juce::Colour(0xff0284c7));
    btnLinkChannels.onClick = [this] {
      const bool linked = btnLinkChannels.getToggleState();
      current().stereoLinked.store(linked, std::memory_order_relaxed);
      int partner = -1;
      if (engine.isChannelStereoLinked(channelSelector.getSelectedId() - 1,
                                       &partner)) {
        engine.channels[static_cast<size_t>(partner)].stereoLinked.store(
            linked, std::memory_order_relaxed);
      }
      repaint();
    };
    addAndMakeVisible(btnLinkChannels);

    btnMaximize.setButtonText("Maximize to 0dB");
    btnMaximize.setClickingTogglesState(true);
    btnMaximize.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff182230));
    btnMaximize.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xffd97706));
    btnMaximize.onClick = [this] {
      current().compressorMaximize.store(btnMaximize.getToggleState(),
                                         std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(btnMaximize);

    btnCustomShape.setButtonText("Custom shape");
    btnCustomShape.setClickingTogglesState(true);
    btnCustomShape.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff182230));
    btnCustomShape.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff0284c7));
    btnCustomShape.onClick = [this] {
      const bool cs = btnCustomShape.getToggleState();
      current().compressorCustomShape.store(cs, std::memory_order_relaxed);
      current().compressorMode.store(
          cs ? static_cast<int>(nova::graviton::CompressorMode::Creative)
             : static_cast<int>(nova::graviton::CompressorMode::Corrective),
          std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(btnCustomShape);

    // Toggle between COMPRESSOR view and GATE / EXPANDER view
    btnToggleGateView.setButtonText("GATE / EXP");
    btnToggleGateView.setClickingTogglesState(true);
    btnToggleGateView.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff182230));
    btnToggleGateView.setColour(juce::TextButton::buttonOnColourId,
                                juce::Colour(0xff10b981));
    btnToggleGateView.onClick = [this] {
      showGateControls = btnToggleGateView.getToggleState();
      resized();
      repaint();
    };
    addAndMakeVisible(btnToggleGateView);

    // -------------------------------------------------------------------------
    // 5. SIDE-CHAIN SECTION
    // -------------------------------------------------------------------------
    btnScEnable.setButtonText("Enable");
    btnScEnable.setClickingTogglesState(true);
    btnScEnable.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff182230));
    btnScEnable.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xff0ea5e9));
    btnScEnable.onClick = [this] {
      current().sidechainSourceType.store(btnScEnable.getToggleState() ? 1 : 0,
                                          std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(btnScEnable);

    scSourceSelector.addItem("Internal", 1);
    for (int i = 1; i <= 32; ++i)
      scSourceSelector.addItem("Ext CH " + juce::String(i), i + 1);
    scSourceSelector.setSelectedId(1, juce::dontSendNotification);
    scSourceSelector.onChange = [this] {
      const int sel = scSourceSelector.getSelectedId();
      if (sel <= 1) {
        current().sidechainSourceType.store(0, std::memory_order_relaxed);
      } else {
        current().sidechainSourceType.store(1, std::memory_order_relaxed);
        current().sidechainExternalChannel.store(sel - 2,
                                                 std::memory_order_relaxed);
      }
      repaint();
    };
    addAndMakeVisible(scSourceSelector);

    btnScListen.setButtonText("Listen");
    btnScListen.setClickingTogglesState(true);
    btnScListen.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff182230));
    btnScListen.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xfff59e0b));
    btnScListen.onClick = [this] {
      current().sidechainKeyListen.store(btnScListen.getToggleState(),
                                         std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(btnScListen);

    // -------------------------------------------------------------------------
    // 6. GATE CONTROLS (QUICK ACCESS DRAWER)
    // -------------------------------------------------------------------------
    btnGateActive.setButtonText("GATE ON");
    btnGateActive.setClickingTogglesState(true);
    btnGateActive.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff182230));
    btnGateActive.setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(0xff10b981));
    btnGateActive.onClick = [this] {
      current().gateEnabled.store(btnGateActive.getToggleState(),
                                  std::memory_order_relaxed);
      syncStereoPartner([this](ChannelState &p) {
        p.gateEnabled.store(btnGateActive.getToggleState(),
                            std::memory_order_relaxed);
      });
      repaint();
    };
    addChildComponent(btnGateActive);

    knobGateThreshold = std::make_unique<NovaRotaryKnob>("G-THRESH", "dB",
                                                         -80.0f, 0.0f, -45.0f);
    knobGateThreshold->onValueChanged = [this](float v) {
      current().gateThresholdDb.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.gateThresholdDb.store(v, std::memory_order_relaxed);
      });
    };
    addChildComponent(*knobGateThreshold);

    knobGateRatio =
        std::make_unique<NovaRotaryKnob>("G-RATIO", ":1", 1.0f, 10.0f, 4.0f);
    knobGateRatio->onValueChanged = [this](float v) {
      current().gateRatio.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.gateRatio.store(v, std::memory_order_relaxed);
      });
    };
    addChildComponent(*knobGateRatio);

    knobGateAttack =
        std::make_unique<NovaRotaryKnob>("G-ATTACK", "ms", 0.1f, 100.0f, 2.0f);
    knobGateAttack->setLogarithmic(true);
    knobGateAttack->onValueChanged = [this](float v) {
      current().gateAttackMs.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.gateAttackMs.store(v, std::memory_order_relaxed);
      });
    };
    addChildComponent(*knobGateAttack);

    knobGateHold =
        std::make_unique<NovaRotaryKnob>("G-HOLD", "ms", 10.0f, 1000.0f, 50.0f);
    knobGateHold->setLogarithmic(true);
    knobGateHold->onValueChanged = [this](float v) {
      current().gateHoldMs.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.gateHoldMs.store(v, std::memory_order_relaxed);
      });
    };
    addChildComponent(*knobGateHold);

    knobGateRelease = std::make_unique<NovaRotaryKnob>("G-RELEASE", "ms", 10.0f,
                                                       1000.0f, 120.0f);
    knobGateRelease->setLogarithmic(true);
    knobGateRelease->onValueChanged = [this](float v) {
      current().gateReleaseMs.store(v, std::memory_order_relaxed);
      syncStereoPartner([v](ChannelState &p) {
        p.gateReleaseMs.store(v, std::memory_order_relaxed);
      });
    };
    addChildComponent(*knobGateRelease);

    // Initial setup
    setSize(920, 560);
    syncFromChannel();
    saveSnapshot(slotA); // Default state to Slot A
    saveSnapshot(slotB);
    startTimerHz(30); // 30 fps smooth meter & graph telemetry
  }

  ~DynamicsPanel() override { stopTimer(); }

  void setSelectedChannel(int chIdx) {
    channelSelector.setSelectedId(juce::jlimit(1, kLogicalChannels, chIdx + 1),
                                  juce::dontSendNotification);
    syncFromChannel();
  }

  void syncFromChannel() {
    auto &s = current();
    btnBypass.setToggleState(
        !s.compressorEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    knobInput->setValue(s.compressorInputGainDb.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    knobOutput->setValue(
        s.compressorMakeupGainDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    knobAttack->setValue(s.compressorAttackMs.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    knobRelease->setValue(s.compressorReleaseMs.load(std::memory_order_relaxed),
                          juce::dontSendNotification);
    knobRms->setValue(s.compressorRmsMs.load(std::memory_order_relaxed),
                      juce::dontSendNotification);
    knobThreshold->setValue(
        s.compressorThresholdDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    knobRatio->setValue(s.compressorRatio.load(std::memory_order_relaxed),
                        juce::dontSendNotification);

    const float kneeDb = s.compressorKneeDb.load(std::memory_order_relaxed);
    knobKnee->setValue((kneeDb / 24.0f) * 100.0f, juce::dontSendNotification);

    const int kMode = juce::jlimit(
        0, 2, s.compressorKneeMode.load(std::memory_order_relaxed));
    for (int i = 0; i < 3; ++i)
      btnKneeMode[i]->setToggleState(i == kMode, juce::dontSendNotification);

    btnLinkChannels.setToggleState(
        s.stereoLinked.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    btnMaximize.setToggleState(
        s.compressorMaximize.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    btnCustomShape.setToggleState(
        s.compressorCustomShape.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    const bool scExt =
        (s.sidechainSourceType.load(std::memory_order_relaxed) == 1);
    btnScEnable.setToggleState(scExt, juce::dontSendNotification);
    const int extCh =
        s.sidechainExternalChannel.load(std::memory_order_relaxed);
    scSourceSelector.setSelectedId(scExt ? (extCh + 2) : 1,
                                   juce::dontSendNotification);
    btnScListen.setToggleState(
        s.sidechainKeyListen.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    btnGateActive.setToggleState(s.gateEnabled.load(std::memory_order_relaxed),
                                 juce::dontSendNotification);
    knobGateThreshold->setValue(
        s.gateThresholdDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    knobGateRatio->setValue(s.gateRatio.load(std::memory_order_relaxed),
                            juce::dontSendNotification);
    knobGateAttack->setValue(s.gateAttackMs.load(std::memory_order_relaxed),
                             juce::dontSendNotification);
    knobGateHold->setValue(s.gateHoldMs.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    knobGateRelease->setValue(s.gateReleaseMs.load(std::memory_order_relaxed),
                              juce::dontSendNotification);

    repaint();
  }

  // ---------------------------------------------------------------------------
  // INTERACTIVE TRANSFER CURVE MOUSE DRAGGING
  // ---------------------------------------------------------------------------
  void mouseDown(const juce::MouseEvent &e) override {
    if (curveRect.contains(e.position)) {
      isDraggingThreshold = false;
      isDraggingRatio = false;

      const float threshDb =
          current().compressorThresholdDb.load(std::memory_order_relaxed);
      const float threshX = dbToCurveX(threshDb);

      if (std::abs(e.position.x - threshX) < 14.0f) {
        isDraggingThreshold = true;
      } else {
        isDraggingRatio = true;
      }
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (isDraggingThreshold) {
      const float newThresh =
          std::clamp(curveXToDb(e.position.x), -60.0f, 0.0f);
      knobThreshold->setValue(newThresh);
    } else if (isDraggingRatio) {
      const float inDb = curveXToDb(e.position.x);
      const float outDb = curveYToDb(e.position.y);
      const float threshDb =
          current().compressorThresholdDb.load(std::memory_order_relaxed);
      if (inDb > threshDb + 2.0f) {
        const float deltaIn = inDb - threshDb;
        const float deltaOut = std::max(0.01f, outDb - threshDb);
        const float newRatio = std::clamp(deltaIn / deltaOut, 1.0f, 20.0f);
        knobRatio->setValue(newRatio);
      }
    }
  }

  void mouseUp(const juce::MouseEvent &) override {
    if (isDraggingThreshold || isDraggingRatio) {
      isDraggingThreshold = false;
      isDraggingRatio = false;
      pushUndoSnapshot();
    }
  }

  // ---------------------------------------------------------------------------
  // PAINT
  // ---------------------------------------------------------------------------
  void paint(juce::Graphics &g) override {
    auto area = getLocalBounds().toFloat();

    // Dark sleek Melda / Midas console background
    g.fillAll(juce::Colour(0xff0c1624));

    // Top Header Bar
    auto topBar = area.removeFromTop(38.0f);
    g.setColour(juce::Colour(0xff122033));
    g.fillRect(topBar);
    g.setColour(juce::Colour(0xff1e324d));
    g.drawHorizontalLine(static_cast<int>(topBar.getBottom()), 0.0f,
                         area.getWidth());

    // Red Logo Emblem
    {
      const float logoX = 14.0f;
      const float logoY = 10.0f;
      juce::Path logoPath;
      logoPath.startNewSubPath(logoX, logoY + 16.0f);
      logoPath.lineTo(logoX + 7.0f, logoY + 2.0f);
      logoPath.lineTo(logoX + 14.0f, logoY + 16.0f);
      logoPath.lineTo(logoX + 10.5f, logoY + 16.0f);
      logoPath.lineTo(logoX + 7.0f, logoY + 9.0f);
      logoPath.lineTo(logoX + 3.5f, logoY + 16.0f);
      logoPath.closeSubPath();
      g.setColour(juce::Colour(0xffef4444));
      g.fillPath(logoPath);
    }

    // Title Text: "MCompressor (17.00)" or Channel Title
    const int chNum = channelSelector.getSelectedId();
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("MCompressor", 36, 6, 120, 26, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
    g.setColour(juce::Colour(0xff64748b));
    g.drawText("CH " + juce::String(chNum), 138, 7, 50, 26,
               juce::Justification::centredLeft);

    // Activity State Badge ("ACTIVE" vs "SLEEPING")
    const auto &s = current();
    const float inLevel = s.dynamicsInDb.load(std::memory_order_relaxed);
    const bool isSleeping = (inLevel < -58.0f);
    const float badgeX = area.getWidth() - 84.0f;
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    if (isSleeping) {
      g.setColour(juce::Colour(0xff64748b));
      g.drawText("Sleeping", static_cast<int>(badgeX), 8, 70, 22,
                 juce::Justification::centredRight);
    } else {
      g.setColour(juce::Colour(0xff10b981));
      g.drawText("Active", static_cast<int>(badgeX), 8, 70, 22,
                 juce::Justification::centredRight);
    }

    // Divider Line above Sidechain
    g.setColour(juce::Colour(0xff15253b));
    g.drawHorizontalLine(static_cast<int>(sidechainBarRect.getY()), 8.0f,
                         mainAreaW);

    // Sidechain Header Title
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffcbd5e1));
    g.drawText("SIDE-CHAIN", static_cast<int>(sidechainBarRect.getX() + 8.0f),
               static_cast<int>(sidechainBarRect.getY() + 4.0f), 100, 22,
               juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
    g.setColour(juce::Colour(0xff64748b));
    g.drawText("Side-chain input",
               static_cast<int>(sidechainBarRect.getX() + 115.0f),
               static_cast<int>(sidechainBarRect.getY() + 4.0f), 100, 22,
               juce::Justification::centredLeft);

    // Draw Transfer Curve Display
    drawTransferCurve(g);

    // Draw Right Metering Panel
    drawMeters(g);
  }

  // ---------------------------------------------------------------------------
  // DRAW INTERACTIVE TRANSFER CURVE
  // ---------------------------------------------------------------------------
  void drawTransferCurve(juce::Graphics &g) {
    if (curveRect.isEmpty())
      return;

    // Background graph fill
    g.setColour(juce::Colour(0xff09111c));
    g.fillRect(curveRect);

    // Outer border
    g.setColour(juce::Colour(0xff1e324d));
    g.drawRect(curveRect, 1.0f);

    // Grid lines and dB labels (-80 to 0 dB)
    static const float gridDbs[] = {-72.0f, -60.0f, -48.0f, -36.0f, -24.0f,
                                    -18.0f, -12.0f, -6.0f,  0.0f};
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));

    for (float db : gridDbs) {
      const float gx = dbToCurveX(db);
      const float gy = dbToCurveY(db);

      // Vertical grid
      g.setColour(juce::Colour(0x1a38bdf8));
      g.drawVerticalLine(static_cast<int>(gx), curveRect.getY(),
                         curveRect.getBottom());

      // Horizontal grid
      g.drawHorizontalLine(static_cast<int>(gy), curveRect.getX(),
                           curveRect.getRight());

      // Coordinate Labels
      g.setColour(juce::Colour(0xff475569));
      if (db == 0.0f) {
        g.drawText("0 dB", static_cast<int>(curveRect.getX() + 4.0f),
                   static_cast<int>(gy - 12.0f), 40, 12,
                   juce::Justification::left);
        g.drawText("0 dB", static_cast<int>(gx - 32.0f),
                   static_cast<int>(curveRect.getBottom() - 14.0f), 30, 12,
                   juce::Justification::right);
      } else if (db >= -36.0f) {
        g.drawText(juce::String((int)db) + " dB",
                   static_cast<int>(curveRect.getX() + 4.0f),
                   static_cast<int>(gy - 6.0f), 44, 12,
                   juce::Justification::left);
        g.drawText(juce::String((int)db) + " dB", static_cast<int>(gx - 20.0f),
                   static_cast<int>(curveRect.getBottom() - 14.0f), 40, 12,
                   juce::Justification::centred);
      }
    }

    // 1:1 Diagonal Unity Reference Line (Dashed)
    {
      juce::Path diagPath;
      diagPath.startNewSubPath(curveRect.getX(), curveRect.getBottom());
      diagPath.lineTo(curveRect.getRight(), curveRect.getY());
      g.setColour(juce::Colour(0x33ffffff));
      static const float dashLengths[2] = {4.0f, 4.0f};
      juce::Path dashedPath;
      juce::PathStrokeType(1.0f).createDashedStroke(dashedPath, diagPath, dashLengths, 2);
      g.fillPath(dashedPath);
    }

    // Threshold Marker Vertical Line (Red line as in Melda)
    auto &s = current();
    const float threshDb =
        s.compressorThresholdDb.load(std::memory_order_relaxed);
    const float threshX = dbToCurveX(threshDb);
    g.setColour(juce::Colour(0x99ef4444));
    g.drawVerticalLine(static_cast<int>(threshX), curveRect.getY(),
                       curveRect.getBottom());

    // Draw Shaded Compression Gain-Reduction Area
    juce::Path fillPath;
    fillPath.startNewSubPath(curveRect.getX(), curveRect.getBottom());

    const int steps = 140;
    for (int i = 0; i <= steps; ++i) {
      const float t = (float)i / (float)steps;
      const float inDb = -80.0f + t * 80.0f;
      const float outDb = calculateTransferOutputDb(inDb);
      const float px = dbToCurveX(inDb);
      const float py = dbToCurveY(outDb);
      fillPath.lineTo(px, py);
    }
    fillPath.lineTo(curveRect.getRight(), curveRect.getBottom());
    fillPath.closeSubPath();
    g.setColour(juce::Colour(0x180284c7));
    g.fillPath(fillPath);

    // Thick Characteristic Compression Curve
    juce::Path curvePath;
    for (int i = 0; i <= steps; ++i) {
      const float t = (float)i / (float)steps;
      const float inDb = -80.0f + t * 80.0f;
      const float outDb = calculateTransferOutputDb(inDb);
      const float px = dbToCurveX(inDb);
      const float py = dbToCurveY(outDb);
      if (i == 0)
        curvePath.startNewSubPath(px, py);
      else
        curvePath.lineTo(px, py);
    }
    g.setColour(juce::Colour(0xfff8fafc));
    g.strokePath(curvePath,
                 juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Live Operating Point Dot (animated ball on curve)
    const float curIn = s.dynamicsInDb.load(std::memory_order_relaxed);
    if (curIn >= -78.0f) {
      const float curOut = calculateTransferOutputDb(curIn);
      const float bx = dbToCurveX(curIn);
      const float by = dbToCurveY(curOut);

      // Glow halo
      g.setColour(juce::Colour(0x4400d4ff));
      g.fillEllipse(bx - 8.0f, by - 8.0f, 16.0f, 16.0f);

      // Bright dot
      g.setColour(juce::Colour(0xff00d4ff));
      g.fillEllipse(bx - 4.5f, by - 4.5f, 9.0f, 9.0f);
      g.setColour(juce::Colours::white);
      g.drawEllipse(bx - 4.5f, by - 4.5f, 9.0f, 9.0f, 1.2f);
    }
  }

  // ---------------------------------------------------------------------------
  // DRAW RIGHT 4-COLUMN PRECISION METERING PANEL
  // ---------------------------------------------------------------------------
  void drawMeters(juce::Graphics &g) {
    if (meterBayRect.isEmpty())
      return;

    // Meter Background Box
    g.setColour(juce::Colour(0xff091320));
    g.fillRect(meterBayRect);
    g.setColour(juce::Colour(0xff182c44));
    g.drawRect(meterBayRect, 1.0f);

    const float bayX = meterBayRect.getX();
    const float bayY = meterBayRect.getY();
    const float bayW = meterBayRect.getWidth();
    const float bayH = meterBayRect.getHeight();

    // 4 Columns: In, Side, Out, R
    const float headerH = 22.0f;
    const float footerH = 24.0f;
    const float barH = bayH - headerH - footerH;

    // Widths
    const float colW = (bayW - 40.0f) / 4.0f;
    const float colGap = 4.0f;

    const float inX = bayX + 8.0f;
    const float sideX = inX + colW + colGap;
    const float outX = sideX + colW + colGap;
    const float rX = bayX + bayW - colW - 8.0f;
    const float scaleX = outX + colW + 2.0f;

    auto &s = current();
    const float inDb = s.dynamicsInDb.load(std::memory_order_relaxed);
    const float sideDb = s.dynamicsSideDb.load(std::memory_order_relaxed);
    const float outDb = s.dynamicsOutDb.load(std::memory_order_relaxed);
    const float rDb =
        s.compressorGainReductionDb.load(std::memory_order_relaxed);

    // Smooth meter decay
    liveInDb = std::max(inDb, liveInDb * 0.90f);
    liveSideDb = std::max(sideDb, liveSideDb * 0.90f);
    liveOutDb = std::max(outDb, liveOutDb * 0.90f);
    liveRDb = std::max(rDb, liveRDb * 0.88f);

    // Headers
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffcbd5e1));
    g.drawText("In", static_cast<int>(inX), static_cast<int>(bayY + 2.0f),
               static_cast<int>(colW), static_cast<int>(headerH),
               juce::Justification::centred);
    g.drawText("Side", static_cast<int>(sideX), static_cast<int>(bayY + 2.0f),
               static_cast<int>(colW), static_cast<int>(headerH),
               juce::Justification::centred);
    g.drawText("Out", static_cast<int>(outX), static_cast<int>(bayY + 2.0f),
               static_cast<int>(colW), static_cast<int>(headerH),
               juce::Justification::centred);
    g.drawText("R", static_cast<int>(rX), static_cast<int>(bayY + 2.0f),
               static_cast<int>(colW), static_cast<int>(headerH),
               juce::Justification::centred);

    // Meter track background
    const auto drawMeterTrack = [&](float x) {
      g.setColour(juce::Colour(0xff121e2d));
      g.fillRect(x, bayY + headerH, colW, barH);
      g.setColour(juce::Colour(0xff1e324d));
      g.drawRect(x, bayY + headerH, colW, barH, 1.0f);
    };

    drawMeterTrack(inX);
    drawMeterTrack(sideX);
    drawMeterTrack(outX);
    drawMeterTrack(rX);

    // Draw In / Side / Out Level Bars
    const auto drawLevelBar = [&](float x, float dbVal, bool isOutput) {
      const float norm = std::clamp((dbVal + 80.0f) / 80.0f, 0.0f, 1.0f);
      const float fillH = norm * barH;
      const float barY = bayY + headerH + barH - fillH;

      juce::ColourGradient grad(juce::Colour(0xff0ea5e9), x,
                                bayY + headerH + barH, juce::Colour(0xff38bdf8),
                                x, bayY + headerH, false);
      grad.addColour(0.75f, juce::Colour(0xff10b981));
      if (isOutput && dbVal >= -0.1f)
        grad.addColour(0.96f, juce::Colour(0xffef4444)); // Clip warning

      g.setGradientFill(grad);
      g.fillRect(x + 1.0f, barY, colW - 2.0f, fillH);

      // Clip indicator block at top if >= 0 dB
      if (isOutput && dbVal >= -0.1f) {
        g.setColour(juce::Colour(0xffef4444));
        g.fillRect(x + 1.0f, bayY + headerH, colW - 2.0f, 14.0f);
      }
    };

    drawLevelBar(inX, liveInDb, false);
    drawLevelBar(sideX, liveSideDb, false);
    drawLevelBar(outX, liveOutDb, true);

    // Draw Gain Reduction (R) Bar: Moves DOWN from top (0 dB)
    {
      const float rNorm = std::clamp(liveRDb / 24.0f, 0.0f, 1.0f);
      const float fillH = rNorm * barH;
      if (fillH > 1.0f) {
        g.setColour(juce::Colour(0xffd97706)); // Amber / Gold reduction bar
        g.fillRect(rX + 1.0f, bayY + headerH, colW - 2.0f, fillH);
      }
    }

    // Scale markings in the middle
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(juce::Colour(0xff64748b));
    static const int scaleMarks[] = {0, -10, -20, -30, -40, -50, -60, -70, -80};
    for (int sm : scaleMarks) {
      const float t = (float)(-sm) / 80.0f;
      const float sy = bayY + headerH + t * barH;
      g.drawText(juce::String(sm), static_cast<int>(scaleX - 2.0f),
                 static_cast<int>(sy - 5.0f), 24, 10,
                 juce::Justification::centred);
    }

    // Numeric Readouts at footer
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfff1f5f9));
    const float footY = bayY + headerH + barH + 4.0f;
    g.drawText(formatDb(inDb), static_cast<int>(inX - 2.0f),
               static_cast<int>(footY), static_cast<int>(colW + 4.0f), 14,
               juce::Justification::centred);
    g.drawText(formatDb(sideDb), static_cast<int>(sideX - 2.0f),
               static_cast<int>(footY), static_cast<int>(colW + 4.0f), 14,
               juce::Justification::centred);
    g.drawText(formatDb(outDb), static_cast<int>(outX - 2.0f),
               static_cast<int>(footY), static_cast<int>(colW + 4.0f), 14,
               juce::Justification::centred);
    g.setColour(juce::Colour(0xfff59e0b));
    g.drawText(rDb > 0.01f ? ("-" + juce::String(rDb, 1)) : "0.00",
               static_cast<int>(rX - 2.0f), static_cast<int>(footY),
               static_cast<int>(colW + 4.0f), 14, juce::Justification::centred);
  }

  // ---------------------------------------------------------------------------
  // RESIZED
  // ---------------------------------------------------------------------------
  void resized() override {
    if (knobKnee == nullptr || btnKneeMode[2] == nullptr)
      return;

    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // 1. Toolbar Row
    const float tbH = 34.0f;
    btnPresetMenu.setBounds(190, 4, 110, 26);
    btnPrevPreset.setBounds(304, 4, 22, 26);
    btnNextPreset.setBounds(328, 4, 22, 26);
    btnRandom.setBounds(354, 4, 26, 26);
    btnAddPreset.setBounds(382, 4, 22, 26);
    btnAbCompare.setBounds(408, 4, 34, 26);
    btnUndo.setBounds(446, 4, 42, 26);
    btnRedo.setBounds(490, 4, 42, 26);
    btnBypass.setBounds(536, 4, 52, 26);
    btnSettings.setBounds(592, 4, 58, 26);
    btnHelp.setBounds(654, 4, 24, 26);

    // Layout division: Left main controls & curve, Right metering bay
    const float pad = 8.0f;
    const float meterW = std::clamp(w * 0.28f, 180.0f, 260.0f);
    mainAreaW = w - meterW - pad * 3.0f;

    meterBayRect = juce::Rectangle<float>(w - meterW - pad, tbH + pad, meterW,
                                          h - tbH - pad * 2.0f);

    // 2. Main 8 Knob Grid (Row 1 & Row 2)
    const float knobGridY = tbH + pad;
    const float knobGridH = std::clamp(h * 0.36f, 130.0f, 180.0f);
    const float colW = (mainAreaW - pad * 3.0f) / 4.0f;
    const float rowH = knobGridH * 0.5f;

    // Row 1: INPUT, OUTPUT, ATTACK, RELEASE
    knobInput->setBounds(static_cast<int>(pad), static_cast<int>(knobGridY),
                         static_cast<int>(colW), static_cast<int>(rowH - 4.0f));
    knobOutput->setBounds(static_cast<int>(pad + colW),
                          static_cast<int>(knobGridY), static_cast<int>(colW),
                          static_cast<int>(rowH - 4.0f));
    knobAttack->setBounds(static_cast<int>(pad + colW * 2.0f),
                          static_cast<int>(knobGridY), static_cast<int>(colW),
                          static_cast<int>(rowH - 4.0f));
    knobRelease->setBounds(static_cast<int>(pad + colW * 3.0f),
                           static_cast<int>(knobGridY), static_cast<int>(colW),
                           static_cast<int>(rowH - 4.0f));

    // Row 2: RMS, THRESHOLD, RATIO, KNEE SIZE
    const float row2Y = knobGridY + rowH;
    knobRms->setBounds(static_cast<int>(pad), static_cast<int>(row2Y),
                       static_cast<int>(colW), static_cast<int>(rowH - 4.0f));
    knobThreshold->setBounds(static_cast<int>(pad + colW),
                             static_cast<int>(row2Y), static_cast<int>(colW),
                             static_cast<int>(rowH - 4.0f));
    knobRatio->setBounds(static_cast<int>(pad + colW * 2.0f),
                         static_cast<int>(row2Y), static_cast<int>(colW),
                         static_cast<int>(rowH - 4.0f));
    knobKnee->setBounds(static_cast<int>(pad + colW * 3.0f),
                        static_cast<int>(row2Y), static_cast<int>(colW),
                        static_cast<int>(rowH - 4.0f));

    // 3. Knee Mode & Options Row
    const float optY = knobGridY + knobGridH + 2.0f;
    const float optH = 26.0f;

    // Knee mode segmented buttons (Hard, Linear, Soft)
    const float kmBtnW = 54.0f;
    btnKneeMode[0]->setBounds(static_cast<int>(pad + 65.0f),
                              static_cast<int>(optY), static_cast<int>(kmBtnW),
                              static_cast<int>(optH));
    btnKneeMode[1]->setBounds(static_cast<int>(pad + 65.0f + kmBtnW + 2.0f),
                              static_cast<int>(optY), static_cast<int>(kmBtnW),
                              static_cast<int>(optH));
    btnKneeMode[2]->setBounds(
        static_cast<int>(pad + 65.0f + (kmBtnW + 2.0f) * 2.0f),
        static_cast<int>(optY), static_cast<int>(kmBtnW),
        static_cast<int>(optH));

    // Options toggles
    const float linkX = pad + 65.0f + (kmBtnW + 2.0f) * 3.0f + 12.0f;
    btnLinkChannels.setBounds(static_cast<int>(linkX), static_cast<int>(optY),
                              94, static_cast<int>(optH));
    btnMaximize.setBounds(static_cast<int>(linkX + 98.0f),
                          static_cast<int>(optY), 108, static_cast<int>(optH));
    btnCustomShape.setBounds(static_cast<int>(linkX + 210.0f),
                             static_cast<int>(optY), 94,
                             static_cast<int>(optH));
    btnToggleGateView.setBounds(static_cast<int>(mainAreaW - 84.0f),
                                static_cast<int>(optY), 84,
                                static_cast<int>(optH));

    // 4. Side-Chain Bar
    const float scY = optY + optH + 6.0f;
    const float scH = 28.0f;
    sidechainBarRect = juce::Rectangle<float>(pad, scY, mainAreaW, scH);

    btnScEnable.setBounds(static_cast<int>(pad + 220.0f),
                          static_cast<int>(scY + 2.0f), 58, 24);
    scSourceSelector.setBounds(static_cast<int>(pad + 282.0f),
                               static_cast<int>(scY + 2.0f), 104, 24);
    btnScListen.setBounds(static_cast<int>(pad + 390.0f),
                          static_cast<int>(scY + 2.0f), 54, 24);

    // 5. Bottom Interactive Transfer Curve (or Gate Drawer if active)
    const float curveY = scY + scH + 4.0f;
    const float curveH = h - curveY - pad;
    curveRect = juce::Rectangle<float>(pad, curveY, mainAreaW, curveH);

    // Gate Controls Layout (Shown when Gate tab active)
    btnGateActive.setVisible(showGateControls);
    knobGateThreshold->setVisible(showGateControls);
    knobGateRatio->setVisible(showGateControls);
    knobGateAttack->setVisible(showGateControls);
    knobGateHold->setVisible(showGateControls);
    knobGateRelease->setVisible(showGateControls);

    if (showGateControls) {
      const float gateColW = (mainAreaW - pad * 2.0f) / 6.0f;
      btnGateActive.setBounds(static_cast<int>(pad + 4.0f),
                              static_cast<int>(curveY + 12.0f),
                              static_cast<int>(gateColW - 8.0f), 30);
      knobGateThreshold->setBounds(
          static_cast<int>(pad + gateColW), static_cast<int>(curveY + 6.0f),
          static_cast<int>(gateColW), static_cast<int>(curveH - 12.0f));
      knobGateRatio->setBounds(static_cast<int>(pad + gateColW * 2.0f),
                               static_cast<int>(curveY + 6.0f),
                               static_cast<int>(gateColW),
                               static_cast<int>(curveH - 12.0f));
      knobGateAttack->setBounds(static_cast<int>(pad + gateColW * 3.0f),
                                static_cast<int>(curveY + 6.0f),
                                static_cast<int>(gateColW),
                                static_cast<int>(curveH - 12.0f));
      knobGateHold->setBounds(static_cast<int>(pad + gateColW * 4.0f),
                              static_cast<int>(curveY + 6.0f),
                              static_cast<int>(gateColW),
                              static_cast<int>(curveH - 12.0f));
      knobGateRelease->setBounds(static_cast<int>(pad + gateColW * 5.0f),
                                 static_cast<int>(curveY + 6.0f),
                                 static_cast<int>(gateColW),
                                 static_cast<int>(curveH - 12.0f));
    }
  }

private:
  MixerEngine &engine;
  juce::ComboBox channelSelector;

  // Layout rectangles
  juce::Rectangle<float> meterBayRect;
  juce::Rectangle<float> curveRect;
  juce::Rectangle<float> sidechainBarRect;
  float mainAreaW = 600.0f;

  // Toolbar widgets
  juce::TextButton btnPresetMenu, btnPrevPreset, btnNextPreset, btnRandom,
      btnAddPreset;
  juce::TextButton btnAbCompare, btnUndo, btnRedo, btnBypass, btnSettings,
      btnHelp;

  // 8 Main Rotary Knobs
  std::unique_ptr<NovaRotaryKnob> knobInput, knobOutput, knobAttack,
      knobRelease;
  std::unique_ptr<NovaRotaryKnob> knobRms, knobThreshold, knobRatio, knobKnee;

  // Knee Mode & Options
  std::array<std::unique_ptr<juce::TextButton>, 3> btnKneeMode;
  juce::TextButton btnLinkChannels, btnMaximize, btnCustomShape,
      btnToggleGateView;

  // Side-Chain
  juce::TextButton btnScEnable, btnScListen;
  juce::ComboBox scSourceSelector;

  // Gate Controls
  bool showGateControls = false;
  juce::TextButton btnGateActive;
  std::unique_ptr<NovaRotaryKnob> knobGateThreshold, knobGateRatio,
      knobGateAttack, knobGateHold, knobGateRelease;

  // Interactive mouse dragging on curve
  bool isDraggingThreshold = false;
  bool isDraggingRatio = false;

  // Telemetry smoothing
  float liveInDb = -80.0f;
  float liveSideDb = -80.0f;
  float liveOutDb = -80.0f;
  float liveRDb = 0.0f;

  // Parameter State Snapshot for Undo/Redo & A/B Compare
  struct DynamicsSnapshot {
    float inGain = 0.0f;
    float outGain = 0.0f;
    float attack = 10.0f;
    float release = 50.0f;
    float rms = 1.0f;
    float threshold = -12.6f;
    float ratio = 4.74f;
    float kneeDb = 6.0f;
    int kneeMode = 2;
    bool maximize = false;
    bool customShape = false;
  };

  DynamicsSnapshot slotA, slotB;
  bool isSlotBActive = false;
  std::vector<DynamicsSnapshot> undoStack;
  std::vector<DynamicsSnapshot> redoStack;
  int currentPresetIdx = 0;

  ChannelState &current() noexcept {
    const int idx = juce::jlimit(0, kLogicalChannels - 1,
                                 channelSelector.getSelectedId() - 1);
    return engine.channels[static_cast<size_t>(idx)];
  }

  const ChannelState &current() const noexcept {
    const int idx = juce::jlimit(0, kLogicalChannels - 1,
                                 channelSelector.getSelectedId() - 1);
    return engine.channels[static_cast<size_t>(idx)];
  }

  void syncStereoPartner(const std::function<void(ChannelState &)> &fn) {
    if (current().stereoLinked.load(std::memory_order_relaxed)) {
      int partner = -1;
      if (engine.isChannelStereoLinked(channelSelector.getSelectedId() - 1,
                                       &partner) &&
          partner >= 0) {
        fn(engine.channels[static_cast<size_t>(partner)]);
      }
    }
  }

  void timerCallback() override {
    if (isShowing()) {
      repaint();
    }
  }

  // ---------------------------------------------------------------------------
  // TRANSFER CURVE MATH
  // ---------------------------------------------------------------------------
  float calculateTransferOutputDb(float inDb) const noexcept {
    const auto &s = current();
    const float inputGain =
        s.compressorInputGainDb.load(std::memory_order_relaxed);
    const float makeupGain =
        s.compressorMakeupGainDb.load(std::memory_order_relaxed);
    const float threshold =
        s.compressorThresholdDb.load(std::memory_order_relaxed);
    const float ratio =
        std::max(1.0f, s.compressorRatio.load(std::memory_order_relaxed));
    const float kneeDb = s.compressorKneeDb.load(std::memory_order_relaxed);
    const int kneeMode = s.compressorKneeMode.load(std::memory_order_relaxed);

    float effectiveInDb = inDb + inputGain;
    float outputDb = effectiveInDb;

    if (kneeMode == 0 || kneeDb <= 0.01f) { // Hard Knee
      if (effectiveInDb > threshold) {
        outputDb = threshold + (effectiveInDb - threshold) / ratio;
      }
    } else { // Soft or Linear Knee
      const float halfKnee = kneeDb * 0.5f;
      const float kneeStart = threshold - halfKnee;
      const float kneeEnd = threshold + halfKnee;

      if (effectiveInDb > kneeStart) {
        if (effectiveInDb >= kneeEnd) {
          outputDb = threshold + (effectiveInDb - threshold) / ratio;
        } else if (kneeMode == 1) { // Linear
          const float t = (effectiveInDb - kneeStart) / kneeDb;
          const float slope = 1.0f - (1.0f / ratio);
          outputDb =
              effectiveInDb - (t * (effectiveInDb - kneeStart) * slope * 0.5f);
        } else { // Soft Quadratic
          const float norm =
              std::clamp((effectiveInDb - kneeStart) / kneeDb, 0.0f, 1.0f);
          const float softDelta = (norm * norm) * halfKnee;
          outputDb =
              threshold + (effectiveInDb - threshold) / ratio + softDelta;
        }
      }
    }

    if (s.compressorMaximize.load(std::memory_order_relaxed)) {
      outputDb += std::max(0.0f, (-threshold) * (1.0f - 1.0f / ratio));
    }
    outputDb += makeupGain;
    return outputDb;
  }

  float dbToCurveX(float db) const noexcept {
    const float norm = std::clamp((db + 80.0f) / 80.0f, 0.0f, 1.0f);
    return curveRect.getX() + norm * curveRect.getWidth();
  }

  float dbToCurveY(float db) const noexcept {
    const float norm = std::clamp((db + 80.0f) / 80.0f, 0.0f, 1.0f);
    return curveRect.getBottom() - norm * curveRect.getHeight();
  }

  float curveXToDb(float x) const noexcept {
    const float norm =
        std::clamp((x - curveRect.getX()) / curveRect.getWidth(), 0.0f, 1.0f);
    return -80.0f + norm * 80.0f;
  }

  float curveYToDb(float y) const noexcept {
    const float norm = std::clamp(
        (curveRect.getBottom() - y) / curveRect.getHeight(), 0.0f, 1.0f);
    return -80.0f + norm * 80.0f;
  }

  static juce::String formatDb(float db) {
    if (db <= -79.5f)
      return "-inf";
    return (db >= 0.0f ? "+" : "") + juce::String(db, 2);
  }

  // ---------------------------------------------------------------------------
  // PRESET & UTILITY ACTIONS
  // ---------------------------------------------------------------------------
  void saveSnapshot(DynamicsSnapshot &target) {
    const auto &s = current();
    target.inGain = s.compressorInputGainDb.load(std::memory_order_relaxed);
    target.outGain = s.compressorMakeupGainDb.load(std::memory_order_relaxed);
    target.attack = s.compressorAttackMs.load(std::memory_order_relaxed);
    target.release = s.compressorReleaseMs.load(std::memory_order_relaxed);
    target.rms = s.compressorRmsMs.load(std::memory_order_relaxed);
    target.threshold = s.compressorThresholdDb.load(std::memory_order_relaxed);
    target.ratio = s.compressorRatio.load(std::memory_order_relaxed);
    target.kneeDb = s.compressorKneeDb.load(std::memory_order_relaxed);
    target.kneeMode = s.compressorKneeMode.load(std::memory_order_relaxed);
    target.maximize = s.compressorMaximize.load(std::memory_order_relaxed);
    target.customShape =
        s.compressorCustomShape.load(std::memory_order_relaxed);
  }

  void restoreSnapshot(const DynamicsSnapshot &src) {
    pushUndoSnapshot();
    auto &s = current();
    s.compressorInputGainDb.store(src.inGain, std::memory_order_relaxed);
    s.compressorMakeupGainDb.store(src.outGain, std::memory_order_relaxed);
    s.compressorAttackMs.store(src.attack, std::memory_order_relaxed);
    s.compressorReleaseMs.store(src.release, std::memory_order_relaxed);
    s.compressorRmsMs.store(src.rms, std::memory_order_relaxed);
    s.compressorThresholdDb.store(src.threshold, std::memory_order_relaxed);
    s.compressorRatio.store(src.ratio, std::memory_order_relaxed);
    s.compressorKneeDb.store(src.kneeDb, std::memory_order_relaxed);
    s.compressorKneeMode.store(src.kneeMode, std::memory_order_relaxed);
    s.compressorMaximize.store(src.maximize, std::memory_order_relaxed);
    s.compressorCustomShape.store(src.customShape, std::memory_order_relaxed);
    syncFromChannel();
  }

  void pushUndoSnapshot() {
    DynamicsSnapshot snap;
    saveSnapshot(snap);
    undoStack.push_back(snap);
    if (undoStack.size() > 30)
      undoStack.erase(undoStack.begin());
    redoStack.clear();
  }

  void performUndo() {
    if (!undoStack.empty()) {
      DynamicsSnapshot currentSnap;
      saveSnapshot(currentSnap);
      redoStack.push_back(currentSnap);

      DynamicsSnapshot target = undoStack.back();
      undoStack.pop_back();

      auto &s = current();
      s.compressorInputGainDb.store(target.inGain, std::memory_order_relaxed);
      s.compressorMakeupGainDb.store(target.outGain, std::memory_order_relaxed);
      s.compressorAttackMs.store(target.attack, std::memory_order_relaxed);
      s.compressorReleaseMs.store(target.release, std::memory_order_relaxed);
      s.compressorRmsMs.store(target.rms, std::memory_order_relaxed);
      s.compressorThresholdDb.store(target.threshold,
                                    std::memory_order_relaxed);
      s.compressorRatio.store(target.ratio, std::memory_order_relaxed);
      s.compressorKneeDb.store(target.kneeDb, std::memory_order_relaxed);
      s.compressorKneeMode.store(target.kneeMode, std::memory_order_relaxed);
      s.compressorMaximize.store(target.maximize, std::memory_order_relaxed);
      s.compressorCustomShape.store(target.customShape,
                                    std::memory_order_relaxed);
      syncFromChannel();
    }
  }

  void performRedo() {
    if (!redoStack.empty()) {
      DynamicsSnapshot currentSnap;
      saveSnapshot(currentSnap);
      undoStack.push_back(currentSnap);

      DynamicsSnapshot target = redoStack.back();
      redoStack.pop_back();

      auto &s = current();
      s.compressorInputGainDb.store(target.inGain, std::memory_order_relaxed);
      s.compressorMakeupGainDb.store(target.outGain, std::memory_order_relaxed);
      s.compressorAttackMs.store(target.attack, std::memory_order_relaxed);
      s.compressorReleaseMs.store(target.release, std::memory_order_relaxed);
      s.compressorRmsMs.store(target.rms, std::memory_order_relaxed);
      s.compressorThresholdDb.store(target.threshold,
                                    std::memory_order_relaxed);
      s.compressorRatio.store(target.ratio, std::memory_order_relaxed);
      s.compressorKneeDb.store(target.kneeDb, std::memory_order_relaxed);
      s.compressorKneeMode.store(target.kneeMode, std::memory_order_relaxed);
      s.compressorMaximize.store(target.maximize, std::memory_order_relaxed);
      s.compressorCustomShape.store(target.customShape,
                                    std::memory_order_relaxed);
      syncFromChannel();
    }
  }

  void toggleAbCompare() {
    if (!isSlotBActive) {
      saveSnapshot(slotA);
      restoreSnapshot(slotB);
      isSlotBActive = true;
      btnAbCompare.setButtonText("B");
    } else {
      saveSnapshot(slotB);
      restoreSnapshot(slotA);
      isSlotBActive = false;
      btnAbCompare.setButtonText("A");
    }
  }

  void applyPreset(int idx) {
    static const struct PresetDef {
      const char *name;
      float inGain, outGain, attack, release, rms, thresh, ratio, kneeDb;
      int kneeMode;
    } presets[] = {
        {"Default", 0.0f, 0.0f, 10.0f, 50.0f, 1.0f, -12.6f, 4.74f, 6.0f, 2},
        {"Vocal Leveler", 0.0f, 2.0f, 15.0f, 120.0f, 5.0f, -18.0f, 3.0f, 10.0f,
         2},
        {"Snare Snap", 0.0f, 0.0f, 30.0f, 60.0f, 0.0f, -14.0f, 4.0f, 2.0f, 0},
        {"Kick Punch", 0.0f, 1.0f, 25.0f, 80.0f, 0.5f, -16.0f, 5.5f, 3.0f, 0},
        {"Bass Fat Glue", 0.0f, 2.5f, 20.0f, 150.0f, 8.0f, -20.0f, 4.0f, 8.0f,
         2},
        {"Acoustic Gentle", 0.0f, 0.0f, 20.0f, 200.0f, 10.0f, -15.0f, 2.2f,
         12.0f, 2},
        {"Drum Bus Glue", 0.0f, 3.0f, 10.0f, 100.0f, 2.0f, -24.0f, 8.0f, 5.0f,
         1},
        {"Fast Limiter", 0.0f, 0.0f, 0.5f, 30.0f, 0.0f, -2.0f, 20.0f, 0.0f, 0}};

    idx = juce::jlimit(0, 7, idx);
    currentPresetIdx = idx;
    const auto &p = presets[idx];
    btnPresetMenu.setButtonText(p.name);

    pushUndoSnapshot();
    auto &s = current();
    s.compressorInputGainDb.store(p.inGain, std::memory_order_relaxed);
    s.compressorMakeupGainDb.store(p.outGain, std::memory_order_relaxed);
    s.compressorAttackMs.store(p.attack, std::memory_order_relaxed);
    s.compressorReleaseMs.store(p.release, std::memory_order_relaxed);
    s.compressorRmsMs.store(p.rms, std::memory_order_relaxed);
    s.compressorThresholdDb.store(p.thresh, std::memory_order_relaxed);
    s.compressorRatio.store(p.ratio, std::memory_order_relaxed);
    s.compressorKneeDb.store(p.kneeDb, std::memory_order_relaxed);
    s.compressorKneeMode.store(p.kneeMode, std::memory_order_relaxed);
    syncFromChannel();
  }

  void stepPreset(int direction) {
    applyPreset((currentPresetIdx + direction + 8) % 8);
  }

  void showPresetMenu() {
    juce::PopupMenu m;
    static const char *names[] = {"1. Default",       "2. Vocal Leveler",
                                  "3. Snare Snap",    "4. Kick Punch",
                                  "5. Bass Fat Glue", "6. Acoustic Gentle",
                                  "7. Drum Bus Glue", "8. Fast Limiter"};
    for (int i = 0; i < 8; ++i)
      m.addItem(i + 1, names[i], true, i == currentPresetIdx);

    m.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&btnPresetMenu),
        [this](int result) {
          if (result > 0)
            applyPreset(result - 1);
        });
  }

  void applyRandomSettings() {
    pushUndoSnapshot();
    auto &s = current();
    auto &rnd = juce::Random::getSystemRandom();

    const float thresh = -30.0f + rnd.nextFloat() * 25.0f;
    const float ratio = 1.5f + rnd.nextFloat() * 8.0f;
    const float att = 2.0f + rnd.nextFloat() * 40.0f;
    const float rel = 20.0f + rnd.nextFloat() * 300.0f;
    const float knee = rnd.nextFloat() * 12.0f;
    const int kMode = rnd.nextInt(3);

    s.compressorThresholdDb.store(thresh, std::memory_order_relaxed);
    s.compressorRatio.store(ratio, std::memory_order_relaxed);
    s.compressorAttackMs.store(att, std::memory_order_relaxed);
    s.compressorReleaseMs.store(rel, std::memory_order_relaxed);
    s.compressorKneeDb.store(knee, std::memory_order_relaxed);
    s.compressorKneeMode.store(kMode, std::memory_order_relaxed);

    btnPresetMenu.setButtonText("Randomized");
    syncFromChannel();
  }

  void saveUserPreset() { btnPresetMenu.setButtonText("User Preset 1"); }

  void showSettingsMenu() {
    juce::PopupMenu m;
    m.addItem(1, "Topology: Post-EQ Dynamics", true,
              current().postEqDynamics.load(std::memory_order_relaxed));
    m.addItem(2, "Topology: Pre-EQ Dynamics", true,
              !current().postEqDynamics.load(std::memory_order_relaxed));
    m.addSeparator();
    m.addItem(3, "Compressor Mode: Corrective (Clean)", true,
              current().compressorMode.load(std::memory_order_relaxed) == 1);
    m.addItem(4, "Compressor Mode: Adaptive (Auto)", true,
              current().compressorMode.load(std::memory_order_relaxed) == 2);
    m.addItem(5, "Compressor Mode: Creative (Warm Glue)", true,
              current().compressorMode.load(std::memory_order_relaxed) == 3);
    m.addItem(6, "Compressor Mode: Vintage (Opto Style)", true,
              current().compressorMode.load(std::memory_order_relaxed) == 4);

    m.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&btnSettings),
        [this](int result) {
          if (result == 1)
            current().postEqDynamics.store(true, std::memory_order_relaxed);
          else if (result == 2)
            current().postEqDynamics.store(false, std::memory_order_relaxed);
          else if (result >= 3 && result <= 6)
            current().compressorMode.store(result - 2,
                                           std::memory_order_relaxed);
          repaint();
        });
  }

  void showHelpDialog() {
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon, "MCompressor & Gate Help",
        "NOVA HD High-Precision Dynamics:\n\n"
        "• THRESHOLD: Level above which compression begins (draggable on "
        "graph)\n"
        "• RATIO: Gain reduction ratio (e.g. 4:1 = +4dB input yields +1dB "
        "output)\n"
        "• KNEE: Hard (sharp transition), Linear (gradual), Soft (smooth "
        "polynomial)\n"
        "• RMS: Detector averaging time. 0 ms = Peak fast response\n"
        "• SIDE-CHAIN: Filterable internal or external key triggering\n"
        "• METERS: In (Input), Side (Sidechain Key), Out (Output), R (Gain "
        "Reduction)",
        "Close");
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicsPanel)
};

} // namespace nova
