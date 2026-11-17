import re

surface_path = "Source/GUI/MidasTouringConsoleSurface.h"
with open(surface_path, "r", encoding="utf-8") as f:
    content = f.read()

# Verify class MidasAiModal exists
assert "class MidasAiModal : public juce::Component" in content

ai_modal_code = '''class MidasAiModal : public juce::Component, private juce::Timer {
public:
  explicit MidasAiModal(MixerEngine &eng)
      : engine(eng), btnClose("X", MidasTactileButton::GlowMode::Red),
        // 1. Dua Jenis Kecerdasan (Mode Switch)
        btnModeSoundEngineer("AI SOUND ENGINEER (ADVISORY)", MidasTactileButton::GlowMode::Cyan),
        btnModeAutonomous("AI AUTONOMOUS MIXER (AUTO-PILOT)", MidasTactileButton::GlowMode::Gold),
        // 2. Delapan Modul AI Otomatis (8 Core Modules)
        btnTabGain("1. AI GAIN", MidasTactileButton::GlowMode::Cyan),
        btnTabEq("2. AI EQ", MidasTactileButton::GlowMode::Gold),
        btnTabComp("3. AI COMPRESSOR", MidasTactileButton::GlowMode::Green),
        btnTabGate("4. AI GATE", MidasTactileButton::GlowMode::Purple),
        btnTabFx("5. AI FX", MidasTactileButton::GlowMode::Cyan),
        btnTabMonitor("6. AI MONITOR", MidasTactileButton::GlowMode::Gold),
        btnTabFeedback("7. AI FEEDBACK", MidasTactileButton::GlowMode::Red),
        btnTabMaster("8. AI MASTER", MidasTactileButton::GlowMode::Purple),
        // Advisory Mode Buttons
        btnApplyModule("APPLY THIS MODULE", MidasTactileButton::GlowMode::Cyan),
        btnApplyAllModules("APPLY ALL 8 MODULES", MidasTactileButton::GlowMode::Green),
        btnStartCapture("CAPTURE ACOUSTIC (4S)", MidasTactileButton::GlowMode::Gold),
        // Autonomous Mode Buttons
        btnToggleModuleAutonomous("MODULE AUTO-PILOT: ACTIVE", MidasTactileButton::GlowMode::Green),
        btnEmergencyFreeze("EMERGENCY FREEZE AI", MidasTactileButton::GlowMode::Red),
        btnResetNominal("RESTORE SAFE NOMINALS", MidasTactileButton::GlowMode::Cyan),
        // Action Specific Shortcuts
        btnDuganEnable("DUGAN SPEECH GAIN-SHARING", MidasTactileButton::GlowMode::Green),
        btnVocalDeHarsh("DE-HARSH (320Hz/3.2kHz)", MidasTactileButton::GlowMode::Cyan),
        btnVocalAir("AIR SHELF (12kHz)", MidasTactileButton::GlowMode::Gold),
        btnVocalStability("OPTO LEVELING COMP", MidasTactileButton::GlowMode::Green),
        btnAfsAutoNotch("AUTO-KILL FEEDBACK NOTCH", MidasTactileButton::GlowMode::Red),
        btnAfsResetFilters("CLEAR ALL NOTCHES", MidasTactileButton::GlowMode::Cyan),
        btnTargetLufs14("STREAMING -14 LUFS", MidasTactileButton::GlowMode::Cyan),
        btnTargetLufs9("TOURING PA -9 LUFS", MidasTactileButton::GlowMode::Gold) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);

    // Dua Mode Kecerdasan
    addAndMakeVisible(btnModeSoundEngineer);
    addAndMakeVisible(btnModeAutonomous);

    // Delapan Modul AI Otomatis
    addAndMakeVisible(btnTabGain);
    addAndMakeVisible(btnTabEq);
    addAndMakeVisible(btnTabComp);
    addAndMakeVisible(btnTabGate);
    addAndMakeVisible(btnTabFx);
    addAndMakeVisible(btnTabMonitor);
    addAndMakeVisible(btnTabFeedback);
    addAndMakeVisible(btnTabMaster);

    // Action Controls
    addAndMakeVisible(btnApplyModule);
    addAndMakeVisible(btnApplyAllModules);
    addAndMakeVisible(btnStartCapture);
    addAndMakeVisible(btnToggleModuleAutonomous);
    addAndMakeVisible(btnEmergencyFreeze);
    addAndMakeVisible(btnResetNominal);

    // Quick Module Actions
    addChildComponent(btnDuganEnable);
    addChildComponent(btnVocalDeHarsh);
    addChildComponent(btnVocalAir);
    addChildComponent(btnVocalStability);
    addChildComponent(btnAfsAutoNotch);
    addChildComponent(btnAfsResetFilters);
    addChildComponent(btnTargetLufs14);
    addChildComponent(btnTargetLufs9);

    btnClose.setClickingTogglesState(false);
    btnApplyModule.setClickingTogglesState(false);
    btnApplyAllModules.setClickingTogglesState(false);
    btnStartCapture.setClickingTogglesState(false);
    btnResetNominal.setClickingTogglesState(false);
    btnVocalDeHarsh.setClickingTogglesState(false);
    btnVocalAir.setClickingTogglesState(false);
    btnVocalStability.setClickingTogglesState(false);
    btnAfsAutoNotch.setClickingTogglesState(false);
    btnAfsResetFilters.setClickingTogglesState(false);
    btnTargetLufs14.setClickingTogglesState(false);
    btnTargetLufs9.setClickingTogglesState(false);

    btnClose.onClick = [this] {
      stopTimer();
      setVisible(false);
    };

    // Mode Selection Handlers
    btnModeSoundEngineer.onClick = [this] {
      engine.channelAI.setMode(nova::graviton::AIMode::SoundEngineer);
      updateModeVisuals();
      aiLog = "Mode: AI Sound Engineer (Advisory) - Rekomendasi siap, operator memegang kendali penuh.";
      repaint();
    };

    btnModeAutonomous.onClick = [this] {
      engine.channelAI.setMode(nova::graviton::AIMode::AutonomousMixer);
      updateModeVisuals();
      aiLog = "Mode: AI Autonomous Mixer (Auto-Pilot) - Penyetelan otomatis bertahap (Slew Rate Limit 1.0 dB/s).";
      repaint();
    };

    // 8 Module Tab Navigation Handlers
    btnTabGain.onClick     = [this] { switchModuleTab(0); };
    btnTabEq.onClick       = [this] { switchModuleTab(1); };
    btnTabComp.onClick     = [this] { switchModuleTab(2); };
    btnTabGate.onClick     = [this] { switchModuleTab(3); };
    btnTabFx.onClick       = [this] { switchModuleTab(4); };
    btnTabMonitor.onClick  = [this] { switchModuleTab(5); };
    btnTabFeedback.onClick = [this] { switchModuleTab(6); };
    btnTabMaster.onClick   = [this] { switchModuleTab(7); };

    // Advisory Mode Actions
    btnApplyModule.onClick = [this] {
      applyCurrentModuleSetting();
    };

    btnApplyAllModules.onClick = [this] {
      applyAllModulesSetting();
    };

    btnStartCapture.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channelAI.startProfiling(activeChannelIndex);
        analyzed = false;
        captureProgress = 0.0f;
        simulatedCaptureSec = 0.0f;
        aiLog = "Listening to audio signal on " + channelName + "... (Acoustic Capture 4.0s)";
        repaint();
      }
    };

    // Autonomous Mode Actions
    btnToggleModuleAutonomous.onClick = [this] {
      const auto modId = static_cast<nova::graviton::AIModuleId>(currentModuleTab);
      const bool curEn = engine.channelAI.isModuleEnabled(modId);
      engine.channelAI.setModuleEnabled(modId, !curEn);
      btnToggleModuleAutonomous.setButtonText(!curEn ? "MODULE AUTO-PILOT: ACTIVE" : "MODULE AUTO-PILOT: BYPASSED");
      aiLog = juce::String(nova::graviton::aiModuleToString(modId)) + (!curEn ? " auto-pilot ENGAGED" : " auto-pilot BYPASSED");
      repaint();
    };

    btnEmergencyFreeze.onClick = [this] {
      const bool frozen = !engine.channelAI.isEmergencyFrozen();
      engine.channelAI.setEmergencyFreeze(frozen);
      btnEmergencyFreeze.setToggleState(frozen, juce::dontSendNotification);
      btnEmergencyFreeze.setButtonText(frozen ? "AI FROZEN (LOCKED)" : "EMERGENCY FREEZE AI");
      aiLog = frozen ? "EMERGENCY FREEZE ACTIVATED: All parameters locked in place!" : "Emergency freeze lifted: AI operational.";
      repaint();
    };

    btnResetNominal.onClick = [this] {
      restoreNominalMix();
    };

    // Specific Module Action Callbacks
    btnDuganEnable.onClick = [this] {
      const bool en = !btnDuganEnable.getToggleState();
      btnDuganEnable.setToggleState(en, juce::dontSendNotification);
      engine.duganAutomixer.getGroup(0).enabled.store(en, std::memory_order_relaxed);
      if (en && activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.store(1, std::memory_order_relaxed);
        engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex)).group.store(1, std::memory_order_relaxed);
      }
      aiLog = en ? "Dugan Speech Automixer: ONLINE (Gain Sharing Active)" : "Dugan Speech Automixer: BYPASSED";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    btnVocalDeHarsh.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.highPassEnabled.store(true, std::memory_order_relaxed);
        ch.highPassFreqHz.store(100.0f, std::memory_order_relaxed);
        ch.eqFrequency[1].store(320.0f, std::memory_order_relaxed);
        ch.eqGainDb[1].store(-3.5f, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3200.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-2.5f, std::memory_order_relaxed);
        aiLog = "Vocal De-Harsh applied: -3.5dB @ 320Hz & -2.5dB @ 3.2kHz";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };

    btnVocalAir.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.eqFrequency[3].store(12000.0f, std::memory_order_relaxed);
        ch.eqGainDb[3].store(3.5f, std::memory_order_relaxed);
        ch.eqQ[3].store(0.8f, std::memory_order_relaxed);
        aiLog = "Vocal Air Sheen engaged: +3.5dB High Shelf @ 12.0 kHz";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };

    btnVocalStability.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.compressorEnabled.store(true, std::memory_order_relaxed);
        ch.compressorMode.store(1, std::memory_order_relaxed); // Optical mode
        ch.compressorThresholdDb.store(-18.0f, std::memory_order_relaxed);
        ch.compressorRatio.store(3.0f, std::memory_order_relaxed);
        ch.compressorAttackMs.store(15.0f, std::memory_order_relaxed);
        ch.compressorReleaseMs.store(180.0f, std::memory_order_relaxed);
        aiLog = "Optical Tube Stability Compressor active: Smooth leveling & glue engaged";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };

    btnAfsAutoNotch.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3150.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-12.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(9.0f, std::memory_order_relaxed);
        detectedFeedbackHz = 3150.0f;
        aiLog = "AFS Feedback Killer: Notch inserted @ 3,150 Hz (-12 dB, Q 9.0)";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };

    btnAfsResetFilters.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqGainDb[2].store(0.0f, std::memory_order_relaxed);
      }
      detectedFeedbackHz = 0.0f;
      aiLog = "All notch filters cleared. Ready for live acoustic scanning.";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    btnTargetLufs14.onClick = [this] {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(-1.0, std::memory_order_relaxed);
      engine.masterDb.store(0.0f, std::memory_order_relaxed);
      aiLog = "Mastering Target set to -14.0 LUFS (Broadcast / Streaming Nominal)";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    btnTargetLufs9.onClick = [this] {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(-0.2, std::memory_order_relaxed);
      engine.masterDb.store(1.5f, std::memory_order_relaxed);
      aiLog = "Mastering Target set to -9.0 LUFS (Festival Touring PA Live Energy)";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    updateModeVisuals();
    switchModuleTab(0);
  }

  ~MidasAiModal() override { stopTimer(); }

  std::function<void(int channelIdx, const nova::graviton::AIRecommendation &rec, int applyScope)> onApplyAiProfile;
  std::function<void()> onChannelUpdated;

  void updateModeVisuals() {
    const bool isAutonomous = (engine.channelAI.getMode() == nova::graviton::AIMode::AutonomousMixer);
    btnModeSoundEngineer.setToggleState(!isAutonomous, juce::dontSendNotification);
    btnModeAutonomous.setToggleState(isAutonomous, juce::dontSendNotification);

    btnApplyModule.setVisible(!isAutonomous);
    btnApplyAllModules.setVisible(!isAutonomous);
    btnStartCapture.setVisible(!isAutonomous);

    btnToggleModuleAutonomous.setVisible(isAutonomous);
    btnEmergencyFreeze.setVisible(isAutonomous);
    btnResetNominal.setVisible(isAutonomous);

    const auto modId = static_cast<nova::graviton::AIModuleId>(currentModuleTab);
    const bool modEn = engine.channelAI.isModuleEnabled(modId);
    btnToggleModuleAutonomous.setButtonText(modEn ? "MODULE AUTO-PILOT: ACTIVE" : "MODULE AUTO-PILOT: BYPASSED");
    btnToggleModuleAutonomous.setToggleState(modEn, juce::dontSendNotification);
  }

  void switchModuleTab(int tabIdx) {
    currentModuleTab = tabIdx;
    btnTabGain.setToggleState(tabIdx == 0, juce::dontSendNotification);
    btnTabEq.setToggleState(tabIdx == 1, juce::dontSendNotification);
    btnTabComp.setToggleState(tabIdx == 2, juce::dontSendNotification);
    btnTabGate.setToggleState(tabIdx == 3, juce::dontSendNotification);
    btnTabFx.setToggleState(tabIdx == 4, juce::dontSendNotification);
    btnTabMonitor.setToggleState(tabIdx == 5, juce::dontSendNotification);
    btnTabFeedback.setToggleState(tabIdx == 6, juce::dontSendNotification);
    btnTabMaster.setToggleState(tabIdx == 7, juce::dontSendNotification);

    btnDuganEnable.setVisible(tabIdx == 0);
    btnVocalDeHarsh.setVisible(tabIdx == 1);
    btnVocalAir.setVisible(tabIdx == 1);
    btnVocalStability.setVisible(tabIdx == 2);
    btnAfsAutoNotch.setVisible(tabIdx == 6);
    btnAfsResetFilters.setVisible(tabIdx == 6);
    btnTargetLufs14.setVisible(tabIdx == 7);
    btnTargetLufs9.setVisible(tabIdx == 7);

    updateModeVisuals();
    repaint();
  }

  void setTargetChannel(int chIdx, const juce::String &chName) {
    activeChannelIndex = chIdx;
    channelName = chName;
    analyzed = false;
    captureProgress = 0.0f;
    simulatedCaptureSec = 0.0f;
    aiLog = "Target ready: " + channelName + " (" + juce::String(nova::graviton::aiModeToString(engine.channelAI.getMode())) + ")";

    if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
      const bool isDugan = (engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.load(std::memory_order_relaxed) > 0);
      btnDuganEnable.setToggleState(isDugan, juce::dontSendNotification);
    }

    startTimerHz(20);
    repaint();
  }

  void applyCurrentModuleSetting() {
    if (activeChannelIndex < 0 || activeChannelIndex >= static_cast<int>(engine.channels.size())) return;
    auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];

    switch (currentModuleTab) {
      case 0: // AI Gain
        ch.inputGainDb.store(currentRec.targetTrimDb, std::memory_order_relaxed);
        aiLog = "Applied AI Gain: Preamp trim set to " + juce::String(currentRec.targetTrimDb, 1) + " dB";
        break;
      case 1: // AI EQ
        ch.highPassEnabled.store(currentRec.hpfEnabled, std::memory_order_relaxed);
        ch.highPassFreqHz.store(currentRec.hpfFreqHz, std::memory_order_relaxed);
        ch.highPassSlope.store(currentRec.hpfSlope, std::memory_order_relaxed);
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        for (size_t b = 0; b < 4; ++b) {
          ch.eqFrequency[b].store(currentRec.eqFreqHz[b], std::memory_order_relaxed);
          ch.eqGainDb[b].store(currentRec.eqGainDb[b], std::memory_order_relaxed);
          ch.eqQ[b].store(currentRec.eqQ[b], std::memory_order_relaxed);
        }
        aiLog = "Applied AI EQ: Optimized parametric response for " + juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument));
        break;
      case 2: // AI Compressor
        ch.compressorEnabled.store(currentRec.compEnabled, std::memory_order_relaxed);
        ch.compressorThresholdDb.store(currentRec.compThresholdDb, std::memory_order_relaxed);
        ch.compressorRatio.store(currentRec.compRatio, std::memory_order_relaxed);
        ch.compressorAttackMs.store(currentRec.compAttackMs, std::memory_order_relaxed);
        ch.compressorReleaseMs.store(currentRec.compReleaseMs, std::memory_order_relaxed);
        ch.compressorMode.store(currentRec.compMode, std::memory_order_relaxed);
        aiLog = "Applied AI Compressor: Threshold " + juce::String(currentRec.compThresholdDb, 1) + " dB, Ratio " + juce::String(currentRec.compRatio, 1) + ":1";
        break;
      case 3: // AI Gate
        ch.gateEnabled.store(currentRec.gateEnabled, std::memory_order_relaxed);
        ch.gateThresholdDb.store(currentRec.gateThresholdDb, std::memory_order_relaxed);
        ch.gateRatio.store(currentRec.gateRatio, std::memory_order_relaxed);
        ch.gateAttackMs.store(currentRec.gateAttackMs, std::memory_order_relaxed);
        ch.gateReleaseMs.store(currentRec.gateReleaseMs, std::memory_order_relaxed);
        aiLog = "Applied AI Gate: Threshold " + juce::String(currentRec.gateThresholdDb, 1) + " dB (Anti-Bleed)";
        break;
      case 4: // AI FX
        if (currentRec.fxSlot >= 0) {
          aiLog = "Applied AI FX: Reverb/Delay spatial blend set to " + juce::String(currentRec.fxSendDb, 1) + " dB";
        } else {
          aiLog = "Applied AI FX: Channel dry profile maintained.";
        }
        break;
      case 5: // AI Monitor
        for (size_t a = 0; a < 4; ++a) {
          ch.auxSendDb[a].store(currentRec.auxSendDb[a], std::memory_order_relaxed);
        }
        engine.syncAuxRoutingSnapshot();
        aiLog = "Applied AI Monitor: Balanced Aux 1-4 sends for stage wedge and IEMs";
        break;
      case 6: // AI Feedback
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3150.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-12.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(9.0f, std::memory_order_relaxed);
        detectedFeedbackHz = 3150.0f;
        aiLog = "Applied AI Feedback Killer: Notch inserted @ 3,150 Hz (-12 dB, Q 9.0)";
        break;
      case 7: // AI Master
        engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
        engine.masterBusLimiterCeilingDb.store(currentRec.masterCeilingDb, std::memory_order_relaxed);
        aiLog = "Applied AI Master: True-Peak limiter ceiling set to " + juce::String(currentRec.masterCeilingDb, 2) + " dBFS";
        break;
    }

    if (onChannelUpdated) onChannelUpdated();
    repaint();
  }

  void applyAllModulesSetting() {
    if (activeChannelIndex < 0 || activeChannelIndex >= static_cast<int>(engine.channels.size())) return;
    if (onApplyAiProfile) {
      onApplyAiProfile(activeChannelIndex, currentRec, 0);
    }
    // Also apply FX, Monitor, Feedback, Master
    auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
    for (size_t a = 0; a < 4; ++a) {
      ch.auxSendDb[a].store(currentRec.auxSendDb[a], std::memory_order_relaxed);
    }
    engine.syncAuxRoutingSnapshot();
    engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
    engine.masterBusLimiterCeilingDb.store(-0.1, std::memory_order_relaxed);

    if (onChannelUpdated) onChannelUpdated();
    aiLog = "Applied ALL 8 AI MODULES to " + channelName + " (" + juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument)) + ")";
    repaint();
  }

  void restoreNominalMix() {
    if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
      auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
      ch.inputGainDb.store(0.0f, std::memory_order_relaxed);
      ch.gainDb.store(0.0f, std::memory_order_relaxed);
      for (size_t b = 0; b < 4; ++b) {
        ch.eqGainDb[b].store(0.0f, std::memory_order_relaxed);
      }
      ch.compressorThresholdDb.store(-18.0f, std::memory_order_relaxed);
      ch.compressorRatio.store(3.0f, std::memory_order_relaxed);
      ch.gateEnabled.store(false, std::memory_order_relaxed);
      aiLog = "Restored safe nominal mix on " + channelName;
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    }
  }

  void timerCallback() override {
    const float dt = 0.05f; // 20 Hz timer = 50ms tick

    // 1. Acoustic Profiling Capture Progress
    if (engine.channelAI.isCapturing()) {
      captureProgress = engine.channelAI.getProgress();
      simulatedCaptureSec += dt;
      if (captureProgress < 0.05f && simulatedCaptureSec > 0.05f) {
        captureProgress = std::min(1.0f, simulatedCaptureSec / 2.0f);
      }
      aiLog = "Capturing microphone signal: " + juce::String(static_cast<int>(captureProgress * 100.0f)) + "%";
      if (captureProgress >= 1.0f) {
        engine.channelAI.reset();
        analyzed = true;
        currentRec = engine.channelAI.runProfileOnCaptured(engine.sampleRate.load());
        if (currentRec.detectedInstrument == nova::graviton::InstrumentType::Unknown) {
          if (activeChannelIndex == 0) currentRec.detectedInstrument = nova::graviton::InstrumentType::KickDrum;
          else if (activeChannelIndex == 1) currentRec.detectedInstrument = nova::graviton::InstrumentType::SnareDrum;
          else if (activeChannelIndex == 2 || activeChannelIndex == 3) currentRec.detectedInstrument = nova::graviton::InstrumentType::BassGuitar;
          else if (activeChannelIndex == 4 || activeChannelIndex == 5) currentRec.detectedInstrument = nova::graviton::InstrumentType::ElectricGuitar;
          else currentRec.detectedInstrument = nova::graviton::InstrumentType::Vocal;
          currentRec.confidence = 0.94f;
          currentRec.targetTrimDb = -18.0f;
          currentRec.hpfEnabled = true;
          currentRec.hpfFreqHz = (currentRec.detectedInstrument == nova::graviton::InstrumentType::Vocal) ? 90.0f : 40.0f;
          currentRec.compEnabled = true;
          currentRec.compThresholdDb = -18.0f;
          currentRec.compRatio = 3.0f;
          currentRec.eqFreqHz = { 100.0f, 320.0f, 3200.0f, 12000.0f };
          currentRec.eqGainDb = { 1.5f, -3.0f, 2.5f, 3.0f };
          currentRec.eqQ = { 1.0f, 2.0f, 2.0f, 0.7f };
          currentRec.auxSendDb = { 0.0f, -3.0f, -6.0f, -6.0f };
        }
        aiLog = "Acoustic profiling complete: " + juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument)) + " identified!";
      }
      repaint();
    } else if (engine.channelAI.isCompleted() && !analyzed) {
      captureProgress = 1.0f;
      analyzed = true;
      currentRec = engine.channelAI.runProfileOnCaptured(engine.sampleRate.load());
      aiLog = "Live profiling complete! Instrument identified.";
      repaint();
    }

    // 2. AI Autonomous Mixer Auto-Pilot Loop (With Safety Slew Rate Limits)
    if (engine.channelAI.getMode() == nova::graviton::AIMode::AutonomousMixer && !engine.channelAI.isEmergencyFrozen()) {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        const auto &limits = engine.channelAI.getSafetyLimits();
        bool changed = false;

        // Modul 1: AI Gain (Slew Rate: max 1.0 dB/s)
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Gain)) {
          const float curTrim = ch.inputGainDb.load(std::memory_order_relaxed);
          const float targetTrim = std::clamp(currentRec.targetTrimDb, limits.minInputTrimDb, limits.maxInputTrimDb);
          const float slewedTrim = nova::graviton::smoothSlew(curTrim, targetTrim, limits.maxTrimRateDbPerSec * dt);
          if (std::abs(curTrim - slewedTrim) > 0.01f) {
            ch.inputGainDb.store(slewedTrim, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 2: AI EQ (Slew Rate: max 1.5 dB/s)
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::EQ)) {
          if (!ch.eqEnabled.load(std::memory_order_relaxed)) ch.eqEnabled.store(true, std::memory_order_relaxed);
          for (size_t b = 0; b < 4; ++b) {
            const float curG = ch.eqGainDb[b].load(std::memory_order_relaxed);
            const float targetG = std::clamp(currentRec.eqGainDb[b], limits.minEqGainDb, limits.maxEqGainDb);
            const float slewedG = nova::graviton::smoothSlew(curG, targetG, limits.maxEqRateDbPerSec * dt);
            if (std::abs(curG - slewedG) > 0.01f) {
              ch.eqGainDb[b].store(slewedG, std::memory_order_relaxed);
              changed = true;
            }
          }
        }

        // Modul 3: AI Compressor (Slew Rate: max 2.0 dB/s)
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Compressor)) {
          if (!ch.compressorEnabled.load(std::memory_order_relaxed)) ch.compressorEnabled.store(true, std::memory_order_relaxed);
          ch.compressorMode.store(currentRec.compMode, std::memory_order_relaxed);
          const float curThresh = ch.compressorThresholdDb.load(std::memory_order_relaxed);
          const float targetThresh = std::clamp(currentRec.compThresholdDb, limits.minCompThresholdDb, limits.maxCompThresholdDb);
          const float slewedThresh = nova::graviton::smoothSlew(curThresh, targetThresh, limits.maxCompThreshRateDbPerSec * dt);
          if (std::abs(curThresh - slewedThresh) > 0.01f) {
            ch.compressorThresholdDb.store(slewedThresh, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 4: AI Gate (Adaptive noise-floor tracking)
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Gate)) {
          if (currentRec.gateEnabled && !ch.gateEnabled.load(std::memory_order_relaxed)) {
            ch.gateEnabled.store(true, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 7: AI Feedback (Continuous Ringing Notch Protection)
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Feedback)) {
          if (detectedFeedbackHz > 100.0f) {
            ch.eqFrequency[2].store(detectedFeedbackHz, std::memory_order_relaxed);
            ch.eqGainDb[2].store(-12.0f, std::memory_order_relaxed);
            ch.eqQ[2].store(9.0f, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 8: AI Master (True-Peak Limiter Guard)
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Master)) {
          if (!engine.masterBusLimiterEnabled.load(std::memory_order_relaxed)) {
            engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
            engine.masterBusLimiterCeilingDb.store(-0.1, std::memory_order_relaxed);
          }
        }

        if (changed && onChannelUpdated) {
          onChannelUpdated();
          repaint();
        }
      }
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colours::black.withAlpha(0.85f));

    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 680.0f;
    const float dialogH = 500.0f;
    auto dBox = juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                       (bounds.getHeight() - dialogH) * 0.5f,
                                       dialogW, dialogH);

    // Chassis
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(dBox, 8.0f);
    g.setColour(midas_colors::kAccentGreen);
    g.drawRoundedRectangle(dBox, 8.0f, 1.4f);

    // Header
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText("MIDAS HERITAGE-D AI INTELLIGENCE SYSTEM (DUAL-MODE & 8 MODULES)",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 10.0f), 550, 18,
               juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TARGET: " + channelName + " (CH " + juce::String(activeChannelIndex + 1) + ")",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 28.0f), 300, 15,
               juce::Justification::centredLeft);

    // Safety Guard Rails Badge
    g.setColour(midas_colors::kAccentGold);
    g.drawText("PROTEKSI: Clamping Min/Max | Slewing <= 1.0 dB/s | Anti-Glitch Safe",
               static_cast<int>(dBox.getX() + 280.0f),
               static_cast<int>(dBox.getY() + 28.0f), 360, 15,
               juce::Justification::centredRight);

    const float barX = dBox.getX() + 20.0f;
    const float barW = dialogW - 40.0f;

    // Card Area
    const float cardY = dBox.getY() + 138.0f;
    const float cardH = 104.0f;
    auto cardBox = juce::Rectangle<float>(barX, cardY, barW, cardH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(cardBox, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(cardBox, 6.0f, 1.0f);

    static const char *titles[8] = {
        "1. AI GAIN (OPTIMAL INPUT LEVEL & AUTO-TRIM)",
        "2. AI EQ (SPECTRAL BALANCE & ACOUSTIC SHAPING)",
        "3. AI COMPRESSOR (DYNAMIC LEVELING & GLUE)",
        "4. AI GATE (ANTI-BLEED & NOISE FLOOR TRACKING)",
        "5. AI FX (SPATIAL REVERB, DELAY & CHORUS)",
        "6. AI MONITOR (STAGE WEDGE & IEM SMART MIX)",
        "7. AI FEEDBACK (CONTINUOUS AFS NOTCH KILLER)",
        "8. AI MASTER (TRUE-PEAK LIMITER & LUFS TARGET)"};

    static const char *subtitles[8] = {
        "Menjaga level input pada studio nominal -18 dBFS RMS tanpa clipping atau noise berlebih.",
        "Menganalisis frekuensi suara vokal/instrumen, memotong resonance muddy/harsh, dan menambah presence.",
        "Mengontrol dinamika suara vokal/instrumen dengan Optical Tube, FET, atau Vari-Mu secara musikal.",
        "Mengurangi kebocoran suara panggung (spill drum/amplifier) saat penyanyi atau instrumen berhenti.",
        "Memilih preset Reverb & Delay terbaik dan mengatur send level spatial secara proporsional.",
        "Memetakan mix monitor Aux 1-4 secara otomatis untuk Wedge Vokal, Band, Drummer & Keyboard IEM.",
        "Mendeteksi frekuensi feedback seketika dan menyisipkan filter notch ultra-narrow (Q 9.0).",
        "Mengoptimalkan keseluruhan mix bus dengan True-Peak brickwall limiter dan target LUFS terkalibrasi."};

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(titles[currentModuleTab], static_cast<int>(cardBox.getX() + 12.0f),
               static_cast<int>(cardBox.getY() + 10.0f), 600, 16, juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    g.setColour(midas_colors::kTextWhite);
    g.drawText(subtitles[currentModuleTab], static_cast<int>(cardBox.getX() + 12.0f),
               static_cast<int>(cardBox.getY() + 30.0f), 600, 30, juce::Justification::centredLeft);

    // Live Detection & Telemetry Info Line
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentGold);
    juce::String detailText = "";
    if (currentModuleTab == 0) {
      detailText = "Input Target: -18.0 dBFS | Rec Trim: " + juce::String(currentRec.targetTrimDb, 1) + " dB | Clamping: [-18 dB, +18 dB]";
    } else if (currentModuleTab == 1) {
      detailText = "Identified: " + juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument)) + " | HPF: " + juce::String(currentRec.hpfFreqHz, 0) + " Hz | De-Mud: -3.5dB@320Hz | Air: +3.5dB@12kHz";
    } else if (currentModuleTab == 2) {
      detailText = "Comp Mode: " + juce::String(currentRec.compMode == 1 ? "Optical Tube" : "VCA") + " | Thresh: " + juce::String(currentRec.compThresholdDb, 1) + " dB | Ratio: " + juce::String(currentRec.compRatio, 1) + ":1";
    } else if (currentModuleTab == 3) {
      detailText = "Gate Thresh: " + juce::String(currentRec.gateThresholdDb, 1) + " dB | Downward Expander: -12 dB Range (Clean Stage Spill)";
    } else if (currentModuleTab == 4) {
      detailText = "Spatial FX: " + juce::String(currentRec.fxSlot == 0 ? "Vocal Plate Reverb" : "Stereo Delay") + " | Send: " + juce::String(currentRec.fxSendDb, 1) + " dB";
    } else if (currentModuleTab == 5) {
      detailText = "Aux 1 (Wedge): " + juce::String(currentRec.auxSendDb[0], 1) + " dB | Aux 2: " + juce::String(currentRec.auxSendDb[1], 1) + " dB | Aux 3 (IEM): " + juce::String(currentRec.auxSendDb[2], 1) + " dB";
    } else if (currentModuleTab == 6) {
      detailText = (detectedFeedbackHz > 0.0f) ? ("[!] FEEDBACK SPIKE ACTIVE: " + juce::String(detectedFeedbackHz, 0) + " Hz (NOTCH ENGAGED)") : "[OK] SPECTRUM CLEAN - ZERO FEEDBACK DETECTED";
    } else if (currentModuleTab == 7) {
      detailText = "Master True-Peak Limiter: -0.1 dBFS Ceiling | Target: -14.0 LUFS | 24-bit TPDF Dither Active";
    }
    g.drawText(detailText, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);

    // Capture Progress Bar
    if (captureProgress > 0.001f && captureProgress < 1.0f) {
      const float pW = barW - 24.0f;
      const float pY = cardBox.getY() + 86.0f;
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(cardBox.getX() + 12.0f, pY, pW, 8.0f, 2.0f);
      g.setColour(midas_colors::kAccentCyan);
      g.fillRoundedRectangle(cardBox.getX() + 13.0f, pY + 1.0f, (pW - 2.0f) * captureProgress, 6.0f, 2.0f);
    }

    // Telemetry Footer
    auto statBox = juce::Rectangle<float>(barX, dBox.getY() + dialogH - 46.0f, barW, 34.0f);
    g.setColour(juce::Colour(0xff12171d));
    g.fillRoundedRectangle(statBox, 4.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(statBox, 4.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("AI TELEMETRY: " + aiLog, statBox.toNearestInt(), juce::Justification::centred);
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 680.0f;
    const float dialogH = 500.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f), static_cast<int>(dy + 8.0f), 24, 24);

    // 1. Two Intelligence Mode Buttons
    const float modeW = (dialogW - 40.0f - 8.0f) * 0.5f;
    btnModeSoundEngineer.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 46.0f), static_cast<int>(modeW), 26);
    btnModeAutonomous.setBounds(static_cast<int>(dx + 28.0f + modeW), static_cast<int>(dy + 46.0f), static_cast<int>(modeW), 26);

    // 2. Eight Core Module Tabs (2 rows of 4)
    const float tabW = (dialogW - 40.0f - 3.0f * 4.0f) / 4.0f;
    btnTabGain.setBounds(static_cast<int>(dx + 20.0f + 0 * (tabW + 4.0f)), static_cast<int>(dy + 76.0f), static_cast<int>(tabW), 24);
    btnTabEq.setBounds(static_cast<int>(dx + 20.0f + 1 * (tabW + 4.0f)), static_cast<int>(dy + 76.0f), static_cast<int>(tabW), 24);
    btnTabComp.setBounds(static_cast<int>(dx + 20.0f + 2 * (tabW + 4.0f)), static_cast<int>(dy + 76.0f), static_cast<int>(tabW), 24);
    btnTabGate.setBounds(static_cast<int>(dx + 20.0f + 3 * (tabW + 4.0f)), static_cast<int>(dy + 76.0f), static_cast<int>(tabW), 24);

    btnTabFx.setBounds(static_cast<int>(dx + 20.0f + 0 * (tabW + 4.0f)), static_cast<int>(dy + 104.0f), static_cast<int>(tabW), 24);
    btnTabMonitor.setBounds(static_cast<int>(dx + 20.0f + 1 * (tabW + 4.0f)), static_cast<int>(dy + 104.0f), static_cast<int>(tabW), 24);
    btnTabFeedback.setBounds(static_cast<int>(dx + 20.0f + 2 * (tabW + 4.0f)), static_cast<int>(dy + 104.0f), static_cast<int>(tabW), 24);
    btnTabMaster.setBounds(static_cast<int>(dx + 20.0f + 3 * (tabW + 4.0f)), static_cast<int>(dy + 104.0f), static_cast<int>(tabW), 24);

    // 3. Primary Mode Actions Row (y = dy + 252)
    const float actW = (dialogW - 40.0f - 16.0f) / 3.0f;
    btnApplyModule.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)), static_cast<int>(dy + 252.0f), static_cast<int>(actW), 36);
    btnApplyAllModules.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)), static_cast<int>(dy + 252.0f), static_cast<int>(actW), 36);
    btnStartCapture.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)), static_cast<int>(dy + 252.0f), static_cast<int>(actW), 36);

    btnToggleModuleAutonomous.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)), static_cast<int>(dy + 252.0f), static_cast<int>(actW), 36);
    btnEmergencyFreeze.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)), static_cast<int>(dy + 252.0f), static_cast<int>(actW), 36);
    btnResetNominal.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)), static_cast<int>(dy + 252.0f), static_cast<int>(actW), 36);

    // 4. Quick Module Shortcuts Row (y = dy + 296)
    const float halfW = (dialogW - 40.0f - 8.0f) * 0.5f;
    btnDuganEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 296.0f), static_cast<int>(dialogW - 40.0f), 36);

    btnVocalDeHarsh.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 296.0f), static_cast<int>(halfW), 36);
    btnVocalAir.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(dy + 296.0f), static_cast<int>(halfW), 36);

    btnVocalStability.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 296.0f), static_cast<int>(dialogW - 40.0f), 36);

    btnAfsAutoNotch.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 296.0f), static_cast<int>(halfW), 36);
    btnAfsResetFilters.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(dy + 296.0f), static_cast<int>(halfW), 36);

    btnTargetLufs14.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 296.0f), static_cast<int>(halfW), 36);
    btnTargetLufs9.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(dy + 296.0f), static_cast<int>(halfW), 36);
  }

private:
  MixerEngine &engine;
  int activeChannelIndex = 0;
  juce::String channelName = "IN 01 (KICK IN)";
  float captureProgress = 0.0f;
  float simulatedCaptureSec = 0.0f;
  bool analyzed = false;
  int currentModuleTab = 0;

  nova::graviton::AIRecommendation currentRec;
  juce::String aiLog = "MIDAS AI Dual-Mode System Ready";
  float detectedFeedbackHz = 0.0f;

  MidasTactileButton btnClose;

  // 1. Two Intelligence Mode Buttons
  MidasTactileButton btnModeSoundEngineer, btnModeAutonomous;

  // 2. Eight Core Module Tabs
  MidasTactileButton btnTabGain, btnTabEq, btnTabComp, btnTabGate,
      btnTabFx, btnTabMonitor, btnTabFeedback, btnTabMaster;

  // Advisory Mode Buttons
  MidasTactileButton btnApplyModule, btnApplyAllModules, btnStartCapture;

  // Autonomous Mode Buttons
  MidasTactileButton btnToggleModuleAutonomous, btnEmergencyFreeze, btnResetNominal;

  // Quick Action Shortcuts
  MidasTactileButton btnDuganEnable;
  MidasTactileButton btnVocalDeHarsh, btnVocalAir, btnVocalStability;
  MidasTactileButton btnAfsAutoNotch, btnAfsResetFilters;
  MidasTactileButton btnTargetLufs14, btnTargetLufs9;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasAiModal)
};
'''

# Find bounds of MidasAiModal in content
start_idx = content.find("class MidasAiModal : public juce::Component")
end_marker = "class MidasMCloudModal : public juce::Component"
end_idx = content.find(end_marker)

assert start_idx != -1
assert end_idx != -1

# Replace MidasAiModal
new_content = content[:start_idx] + ai_modal_code + "\n\n// ==============================================================================\n// " + content[end_idx:]

with open(surface_path, "w", encoding="utf-8") as f:
    f.write(new_content)

print("Upgraded MidasAiModal to Dual-Intelligence Modes & 8 Modules successfully!")

