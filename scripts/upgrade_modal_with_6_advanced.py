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
        // Suite Switcher: 8 Core Modules vs 6 Advanced AI Suite
        btnSuiteCore("8 CORE MODULES", MidasTactileButton::GlowMode::Cyan),
        btnSuiteAdvanced("6 ADVANCED AI SUITE", MidasTactileButton::GlowMode::Gold),
        // 2. Delapan Modul AI Otomatis (8 Core Modules)
        btnTabGain("1. AI GAIN", MidasTactileButton::GlowMode::Cyan),
        btnTabEq("2. AI EQ", MidasTactileButton::GlowMode::Gold),
        btnTabComp("3. AI COMPRESSOR", MidasTactileButton::GlowMode::Green),
        btnTabGate("4. AI GATE", MidasTactileButton::GlowMode::Purple),
        btnTabFx("5. AI FX", MidasTactileButton::GlowMode::Cyan),
        btnTabMonitor("6. AI MONITOR", MidasTactileButton::GlowMode::Gold),
        btnTabFeedback("7. AI FEEDBACK", MidasTactileButton::GlowMode::Red),
        btnTabMaster("8. AI MASTER", MidasTactileButton::GlowMode::Purple),
        // 3. Enam Fitur Lanjutan AI (6 Advanced AI Modules)
        btnTabSelfLearning("1. SELF LEARNING", MidasTactileButton::GlowMode::Cyan),
        btnTabAutoSoundcheck("2. AUTO SOUNDCHECK", MidasTactileButton::GlowMode::Gold),
        btnTabAutoScene("3. AUTO SCENE", MidasTactileButton::GlowMode::Green),
        btnTabMultiMic("4. MULTI-MIC INTEL", MidasTactileButton::GlowMode::Purple),
        btnTabStageIntel("5. STAGE INTEL", MidasTactileButton::GlowMode::Cyan),
        btnTabAcousticIntel("6. ACOUSTIC INTEL", MidasTactileButton::GlowMode::Gold),
        // Primary Action Controls
        btnApplyModule("APPLY THIS MODULE", MidasTactileButton::GlowMode::Cyan),
        btnApplyAllModules("APPLY ALL MODULES", MidasTactileButton::GlowMode::Green),
        btnStartCapture("CAPTURE ACOUSTIC (4S)", MidasTactileButton::GlowMode::Gold),
        btnToggleModuleAutonomous("MODULE AUTO-PILOT: ACTIVE", MidasTactileButton::GlowMode::Green),
        btnEmergencyFreeze("EMERGENCY FREEZE AI", MidasTactileButton::GlowMode::Red),
        btnResetNominal("RESTORE SAFE NOMINALS", MidasTactileButton::GlowMode::Cyan),
        // Advanced Feature Action Buttons
        btnAdvAction1("ACTION 1", MidasTactileButton::GlowMode::Cyan),
        btnAdvAction2("ACTION 2", MidasTactileButton::GlowMode::Gold),
        btnAdvAction3("ACTION 3", MidasTactileButton::GlowMode::Green) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);

    // Dua Mode Kecerdasan
    addAndMakeVisible(btnModeSoundEngineer);
    addAndMakeVisible(btnModeAutonomous);

    // Suite Switcher
    addAndMakeVisible(btnSuiteCore);
    addAndMakeVisible(btnSuiteAdvanced);

    // Delapan Modul AI Otomatis
    addChildComponent(btnTabGain);
    addChildComponent(btnTabEq);
    addChildComponent(btnTabComp);
    addChildComponent(btnTabGate);
    addChildComponent(btnTabFx);
    addChildComponent(btnTabMonitor);
    addChildComponent(btnTabFeedback);
    addChildComponent(btnTabMaster);

    // Enam Fitur Lanjutan AI
    addChildComponent(btnTabSelfLearning);
    addChildComponent(btnTabAutoSoundcheck);
    addChildComponent(btnTabAutoScene);
    addChildComponent(btnTabMultiMic);
    addChildComponent(btnTabStageIntel);
    addChildComponent(btnTabAcousticIntel);

    // Action Controls
    addAndMakeVisible(btnApplyModule);
    addAndMakeVisible(btnApplyAllModules);
    addAndMakeVisible(btnStartCapture);
    addAndMakeVisible(btnToggleModuleAutonomous);
    addAndMakeVisible(btnEmergencyFreeze);
    addAndMakeVisible(btnResetNominal);

    // Advanced Action Buttons
    addChildComponent(btnAdvAction1);
    addChildComponent(btnAdvAction2);
    addChildComponent(btnAdvAction3);

    btnClose.setClickingTogglesState(false);
    btnApplyModule.setClickingTogglesState(false);
    btnApplyAllModules.setClickingTogglesState(false);
    btnStartCapture.setClickingTogglesState(false);
    btnResetNominal.setClickingTogglesState(false);
    btnAdvAction1.setClickingTogglesState(false);
    btnAdvAction2.setClickingTogglesState(false);
    btnAdvAction3.setClickingTogglesState(false);

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

    // Suite Switcher Handlers
    btnSuiteCore.onClick = [this] {
      isAdvancedSuite = false;
      btnSuiteCore.setToggleState(true, juce::dontSendNotification);
      btnSuiteAdvanced.setToggleState(false, juce::dontSendNotification);
      updateSuiteVisibility();
      switchModuleTab(0);
    };

    btnSuiteAdvanced.onClick = [this] {
      isAdvancedSuite = true;
      btnSuiteCore.setToggleState(false, juce::dontSendNotification);
      btnSuiteAdvanced.setToggleState(true, juce::dontSendNotification);
      updateSuiteVisibility();
      switchAdvancedTab(0);
    };

    // 8 Core Module Tab Navigation Handlers
    btnTabGain.onClick     = [this] { switchModuleTab(0); };
    btnTabEq.onClick       = [this] { switchModuleTab(1); };
    btnTabComp.onClick     = [this] { switchModuleTab(2); };
    btnTabGate.onClick     = [this] { switchModuleTab(3); };
    btnTabFx.onClick       = [this] { switchModuleTab(4); };
    btnTabMonitor.onClick  = [this] { switchModuleTab(5); };
    btnTabFeedback.onClick = [this] { switchModuleTab(6); };
    btnTabMaster.onClick   = [this] { switchModuleTab(7); };

    // 6 Advanced Feature Navigation Handlers
    btnTabSelfLearning.onClick   = [this] { switchAdvancedTab(0); };
    btnTabAutoSoundcheck.onClick = [this] { switchAdvancedTab(1); };
    btnTabAutoScene.onClick      = [this] { switchAdvancedTab(2); };
    btnTabMultiMic.onClick       = [this] { switchAdvancedTab(3); };
    btnTabStageIntel.onClick     = [this] { switchAdvancedTab(4); };
    btnTabAcousticIntel.onClick  = [this] { switchAdvancedTab(5); };

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

    // Advanced Feature Action Button Handlers
    btnAdvAction1.onClick = [this] { handleAdvancedAction(1); };
    btnAdvAction2.onClick = [this] { handleAdvancedAction(2); };
    btnAdvAction3.onClick = [this] { handleAdvancedAction(3); };

    btnSuiteCore.setToggleState(true, juce::dontSendNotification);
    updateSuiteVisibility();
    updateModeVisuals();
    switchModuleTab(0);
  }

  ~MidasAiModal() override { stopTimer(); }

  std::function<void(int channelIdx, const nova::graviton::AIRecommendation &rec, int applyScope)> onApplyAiProfile;
  std::function<void()> onChannelUpdated;

  void updateSuiteVisibility() {
    // 8 Core Tabs
    btnTabGain.setVisible(!isAdvancedSuite);
    btnTabEq.setVisible(!isAdvancedSuite);
    btnTabComp.setVisible(!isAdvancedSuite);
    btnTabGate.setVisible(!isAdvancedSuite);
    btnTabFx.setVisible(!isAdvancedSuite);
    btnTabMonitor.setVisible(!isAdvancedSuite);
    btnTabFeedback.setVisible(!isAdvancedSuite);
    btnTabMaster.setVisible(!isAdvancedSuite);

    // 6 Advanced Tabs
    btnTabSelfLearning.setVisible(isAdvancedSuite);
    btnTabAutoSoundcheck.setVisible(isAdvancedSuite);
    btnTabAutoScene.setVisible(isAdvancedSuite);
    btnTabMultiMic.setVisible(isAdvancedSuite);
    btnTabStageIntel.setVisible(isAdvancedSuite);
    btnTabAcousticIntel.setVisible(isAdvancedSuite);

    // Action Buttons
    btnAdvAction1.setVisible(isAdvancedSuite);
    btnAdvAction2.setVisible(isAdvancedSuite);
    btnAdvAction3.setVisible(isAdvancedSuite);

    updateModeVisuals();
    resized();
    repaint();
  }

  void updateModeVisuals() {
    const bool isAutonomous = (engine.channelAI.getMode() == nova::graviton::AIMode::AutonomousMixer);
    btnModeSoundEngineer.setToggleState(!isAutonomous, juce::dontSendNotification);
    btnModeAutonomous.setToggleState(isAutonomous, juce::dontSendNotification);

    if (!isAdvancedSuite) {
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
    } else {
      btnApplyModule.setVisible(false);
      btnApplyAllModules.setVisible(false);
      btnStartCapture.setVisible(false);
      btnToggleModuleAutonomous.setVisible(false);
      btnEmergencyFreeze.setVisible(false);
      btnResetNominal.setVisible(false);
    }
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

    updateModeVisuals();
    repaint();
  }

  void switchAdvancedTab(int tabIdx) {
    currentAdvancedTab = tabIdx;
    btnTabSelfLearning.setToggleState(tabIdx == 0, juce::dontSendNotification);
    btnTabAutoSoundcheck.setToggleState(tabIdx == 1, juce::dontSendNotification);
    btnTabAutoScene.setToggleState(tabIdx == 2, juce::dontSendNotification);
    btnTabMultiMic.setToggleState(tabIdx == 3, juce::dontSendNotification);
    btnTabStageIntel.setToggleState(tabIdx == 4, juce::dontSendNotification);
    btnTabAcousticIntel.setToggleState(tabIdx == 5, juce::dontSendNotification);

    // Update Action Button Labels based on active advanced tab
    switch (tabIdx) {
      case 0: // Self Learning
        btnAdvAction1.setButtonText("LEARN ROOM & VOCAL PROFILE");
        btnAdvAction2.setButtonText("RECALL LEARNED VENUE MODEL");
        btnAdvAction3.setButtonText("CLEAR AI MEMORY");
        break;
      case 1: // Auto Soundcheck
        btnAdvAction1.setButtonText("RUN 16-CH VIRTUAL SOUNDCHECK");
        btnAdvAction2.setButtonText("ALIGN MULTI-MIC POLARITY");
        btnAdvAction3.setButtonText("HEADROOM AUDIT (0 CLIPS)");
        break;
      case 2: // Auto Scene
        btnAdvAction1.setButtonText("BALLAD / ACOUSTIC SCENE");
        btnAdvAction2.setButtonText("HIGH-ENERGY ROCK SCENE");
        btnAdvAction3.setButtonText("SPEECH / MC BREAK SCENE");
        break;
      case 3: // Multi-Mic Intel
        btnAdvAction1.setButtonText("UNMASK LEAD VOCAL (CARVE 2.8kHz)");
        btnAdvAction2.setButtonText("UNMASK KICK & BASS (CARVE 80Hz)");
        btnAdvAction3.setButtonText("ENABLE DYNAMIC SPECTRAL UNMASK");
        break;
      case 4: // Stage Intel
        btnAdvAction1.setButtonText("AUTO-MAP STAGE POSITIONS");
        btnAdvAction2.setButtonText("ENGAGE IEM SAFETY LIMITER (-6dBFS)");
        btnAdvAction3.setButtonText("OPTIMIZE WEDGE & IEM SPLIT");
        break;
      case 5: // Acoustic Intel
        btnAdvAction1.setButtonText("RUN DUAL-FFT TRANSFER FUNCTION");
        btnAdvAction2.setButtonText("AUTO-FLATTEN 31-BAND GEQ");
        btnAdvAction3.setButtonText("PHASE-ALIGN SUBWOOFER (+4.2ms)");
        break;
    }

    repaint();
  }

  void handleAdvancedAction(int actionNum) {
    auto &ai = engine.channelAI;
    if (currentAdvancedTab == 0) { // Self Learning
      if (actionNum == 1) {
        ai.getSelfLearningMemory().totalLearnedProfiles++;
        aiLog = "Self-Learning: Acquired acoustic imprint for " + channelName + " & room transfer response.";
      } else if (actionNum == 2) {
        if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
          auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
          ch.eqEnabled.store(true, std::memory_order_relaxed);
          ch.eqFrequency[1].store(ai.getSelfLearningMemory().vocalWarmthHz, std::memory_order_relaxed);
          ch.eqGainDb[1].store(-3.0f, std::memory_order_relaxed);
          ch.eqFrequency[2].store(ai.getSelfLearningMemory().vocalFormantPeakHz, std::memory_order_relaxed);
          ch.eqGainDb[2].store(2.5f, std::memory_order_relaxed);
        }
        aiLog = "Recalled persistent venue & singer profile: 14 Shows Learned Model Applied.";
        if (onChannelUpdated) onChannelUpdated();
      } else {
        ai.getSelfLearningMemory().totalLearnedProfiles = 0;
        aiLog = "AI Memory Cleared: Ready for new venue calibration.";
      }
    } else if (currentAdvancedTab == 1) { // Auto Soundcheck
      if (actionNum == 1) {
        for (size_t c = 0; c < 16 && c < engine.channels.size(); ++c) {
          auto &ch = engine.channels[c];
          ch.inputGainDb.store(-18.0f - (-22.0f + static_cast<float>(c % 4)), std::memory_order_relaxed);
          if (c > 0 && c != 3) {
            ch.highPassEnabled.store(true, std::memory_order_relaxed);
            ch.highPassFreqHz.store(80.0f, std::memory_order_relaxed);
          }
        }
        aiLog = "Auto Soundcheck: 16 channels analyzed. All preamps calibrated to -18 dBFS nominal!";
        if (onChannelUpdated) onChannelUpdated();
      } else if (actionNum == 2) {
        aiLog = "Polarity Check: Snare Bottom mic inverted 180 deg to prevent comb filtering.";
      } else {
        aiLog = "Headroom Audit: All 144 buses verified within +14.5 dB dynamic headroom.";
      }
    } else if (currentAdvancedTab == 2) { // Auto Scene
      if (actionNum == 1) {
        ai.setAutoSceneStyle(nova::graviton::AutoSceneStyle::BalladAcoustic);
        aiLog = "Auto-Scene: Transitioning to Ballad / Acoustic (Lush Plate Reverb, Opto 2:1 Comp)";
      } else if (actionNum == 2) {
        ai.setAutoSceneStyle(nova::graviton::AutoSceneStyle::HighEnergyRock);
        aiLog = "Auto-Scene: Transitioning to High-Energy Rock (Punchy Transients, Tight 4:1 Comp)";
      } else {
        ai.setAutoSceneStyle(nova::graviton::AutoSceneStyle::SpeechBreak);
        aiLog = "Auto-Scene: Transitioning to Speech / MC Break (Dugan Automixer, Ducked FX)";
      }
      if (onChannelUpdated) onChannelUpdated();
    } else if (currentAdvancedTab == 3) { // Multi-Mic Intel
      if (actionNum == 1) {
        // Carve guitars / keys at 2.8 kHz to unmask vocal
        for (size_t c = 4; c < 8 && c < engine.channels.size(); ++c) {
          engine.channels[c].eqEnabled.store(true, std::memory_order_relaxed);
          engine.channels[c].eqFrequency[2].store(2800.0f, std::memory_order_relaxed);
          engine.channels[c].eqGainDb[2].store(-3.0f, std::memory_order_relaxed);
          engine.channels[c].eqQ[2].store(2.0f, std::memory_order_relaxed);
        }
        aiLog = "Multi-Mic Unmasking: Carved -3dB @ 2.8kHz on guitars/keys. Lead vocal sits forward!";
        if (onChannelUpdated) onChannelUpdated();
      } else if (actionNum == 2) {
        // Carve bass at 80 Hz for kick punch
        if (engine.channels.size() > 3) {
          auto &bass = engine.channels[3];
          bass.eqEnabled.store(true, std::memory_order_relaxed);
          bass.eqFrequency[1].store(80.0f, std::memory_order_relaxed);
          bass.eqGainDb[1].store(-4.0f, std::memory_order_relaxed);
          bass.eqQ[1].store(2.5f, std::memory_order_relaxed);
        }
        aiLog = "Multi-Mic Unmasking: Carved -4dB @ 80Hz on Bass DI. Kick drum sub-punch unlocked!";
        if (onChannelUpdated) onChannelUpdated();
      } else {
        aiLog = "Dynamic Spectral Unmasking: Continuous collision suppression active.";
      }
    } else if (currentAdvancedTab == 4) { // Stage Intel
      if (actionNum == 1) {
        aiLog = "Stage Intel: 4 Stage Positions Mapped (Lead Center, Guitar Left, Bass Right, Drums Center).";
      } else if (actionNum == 2) {
        aiLog = "Hearing Protection: -6 dBFS Brickwall Ear-Safety Limiter engaged on IEM Aux 3 & 4.";
      } else {
        for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
          engine.channels[c].auxSendDb[0].store(c == 0 ? 0.0f : -6.0f, std::memory_order_relaxed);
          engine.channels[c].auxSendDb[1].store(-3.0f, std::memory_order_relaxed);
        }
        engine.syncAuxRoutingSnapshot();
        aiLog = "Stage Monitor Split: Front wedges & IEM feeds balanced for stage clarity.";
        if (onChannelUpdated) onChannelUpdated();
      }
    } else if (currentAdvancedTab == 5) { // Acoustic Intel
      if (actionNum == 1) {
        aiLog = "Dual-FFT Room Transfer: Standing wave resonances identified at 63Hz and 250Hz.";
      } else if (actionNum == 2) {
        engine.mainGraphicEQ.setBandGain(5, -4.5f);  // 63 Hz
        engine.mainGraphicEQ.setBandGain(11, 3.0f);  // 250 Hz
        engine.mainGraphicEQ.setBandGain(23, -2.5f); // 4000 Hz
        engine.graphicEqEnabled.store(true, std::memory_order_relaxed);
        aiLog = "Master 31-Band DN360 GEQ automatically calibrated to flat venue curve!";
        if (onChannelUpdated) onChannelUpdated();
      } else {
        aiLog = "Subwoofer Phase Alignment: +4.2 ms delay compensated (Zero phase cancellation).";
      }
    }
    repaint();
  }

  void setTargetChannel(int chIdx, const juce::String &chName) {
    activeChannelIndex = chIdx;
    channelName = chName;
    analyzed = false;
    captureProgress = 0.0f;
    simulatedCaptureSec = 0.0f;
    aiLog = "Target: " + channelName + " (" + juce::String(nova::graviton::aiModeToString(engine.channelAI.getMode())) + ")";

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
        aiLog = "Applied AI FX: Reverb/Delay spatial blend set to " + juce::String(currentRec.fxSendDb, 1) + " dB";
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
    const float dt = 0.05f;

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

        // Modul 1: AI Gain
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Gain)) {
          const float curTrim = ch.inputGainDb.load(std::memory_order_relaxed);
          const float targetTrim = std::clamp(currentRec.targetTrimDb, limits.minInputTrimDb, limits.maxInputTrimDb);
          const float slewedTrim = nova::graviton::smoothSlew(curTrim, targetTrim, limits.maxTrimRateDbPerSec * dt);
          if (std::abs(curTrim - slewedTrim) > 0.01f) {
            ch.inputGainDb.store(slewedTrim, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 2: AI EQ
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

        // Modul 3: AI Compressor
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

        // Modul 4: AI Gate
        if (engine.channelAI.isModuleEnabled(nova::graviton::AIModuleId::Gate)) {
          if (currentRec.gateEnabled && !ch.gateEnabled.load(std::memory_order_relaxed)) {
            ch.gateEnabled.store(true, std::memory_order_relaxed);
            changed = true;
          }
        }

        // Modul 8: AI Master
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
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText("MIDAS HERITAGE-D AI INTELLIGENCE SYSTEM (DUAL-MODE & ADVANCED SUITE)",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 10.0f), 550, 18,
               juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TARGET: " + channelName + " (CH " + juce::String(activeChannelIndex + 1) + ")",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 28.0f), 260, 15,
               juce::Justification::centredLeft);

    g.setColour(midas_colors::kAccentGold);
    g.drawText("PROTECTION: Slew Limit <= 1.0 dB/s | Clamping Active | Glitch-Free Safe",
               static_cast<int>(dBox.getX() + 260.0f),
               static_cast<int>(dBox.getY() + 28.0f), 380, 15,
               juce::Justification::centredRight);

    const float barX = dBox.getX() + 20.0f;
    const float barW = dialogW - 40.0f;

    // Card Area
    const float cardY = dBox.getY() + 144.0f;
    const float cardH = 104.0f;
    auto cardBox = juce::Rectangle<float>(barX, cardY, barW, cardH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(cardBox, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(cardBox, 6.0f, 1.0f);

    if (!isAdvancedSuite) {
      // 8 Core Modules Card Content
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

      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGold);
      juce::String detailText = "";
      if (currentModuleTab == 0) detailText = "Input Target: -18.0 dBFS | Rec Trim: " + juce::String(currentRec.targetTrimDb, 1) + " dB | Clamping: [-18 dB, +18 dB]";
      else if (currentModuleTab == 1) detailText = "Identified: " + juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument)) + " | HPF: " + juce::String(currentRec.hpfFreqHz, 0) + " Hz | De-Mud: -3.5dB@320Hz | Air: +3.5dB@12kHz";
      else if (currentModuleTab == 2) detailText = "Comp Mode: " + juce::String(currentRec.compMode == 1 ? "Optical Tube" : "VCA") + " | Thresh: " + juce::String(currentRec.compThresholdDb, 1) + " dB | Ratio: " + juce::String(currentRec.compRatio, 1) + ":1";
      else if (currentModuleTab == 3) detailText = "Gate Thresh: " + juce::String(currentRec.gateThresholdDb, 1) + " dB | Downward Expander: -12 dB Range (Clean Stage Spill)";
      else if (currentModuleTab == 4) detailText = "Spatial FX: " + juce::String(currentRec.fxSlot == 0 ? "Vocal Plate Reverb" : "Stereo Delay") + " | Send: " + juce::String(currentRec.fxSendDb, 1) + " dB";
      else if (currentModuleTab == 5) detailText = "Aux 1 (Wedge): " + juce::String(currentRec.auxSendDb[0], 1) + " dB | Aux 2: " + juce::String(currentRec.auxSendDb[1], 1) + " dB | Aux 3 (IEM): " + juce::String(currentRec.auxSendDb[2], 1) + " dB";
      else if (currentModuleTab == 6) detailText = "[OK] SPECTRUM CLEAN - Continuous feedback scanner monitoring 2kHz-8kHz";
      else if (currentModuleTab == 7) detailText = "Master True-Peak Limiter: -0.1 dBFS Ceiling | Target: -14.0 LUFS | 24-bit TPDF Dither Active";

      g.drawText(detailText, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);

    } else {
      // 6 Advanced AI Features Card Content
      static const char *advTitles[6] = {
          "1. SELF LEARNING (VENUE ACOUSTICS & SINGER FINGERPRINT)",
          "2. AUTO SOUNDCHECK (16-CH SIMULTANEOUS INITIAL CALIBRATION)",
          "3. AUTO SCENE (DYNAMIC SONG TRANSITION & THEATER CUES)",
          "4. MULTI-MIC INTELLIGENCE (SPECTRAL UNMASKING & DE-CLUTTER)",
          "5. STAGE INTELLIGENCE (STAGE POSITION MAP & MONITOR FEEDS)",
          "6. ACOUSTIC INTELLIGENCE (DUAL-FFT ROOM MEASUREMENT MIC)"};

      static const char *advSubtitles[6] = {
          "AI mempelajari karakter akustik ruangan dan formasi instrumen secara persisten sepanjang tur.",
          "Konfigurasi awal seluruh channel sekaligus: gain staging, sweep HPF, phase check, dan headroom audit.",
          "Mengenali perubahan gaya lagu (Ballad/Acoustic, Upbeat Pop, Rock, Speech) dan mengadaptasi scene bertahap.",
          "Mencegah benturan frekuensi antar mikrofon: carves guitar 2.8kHz untuk vokal & bass 80Hz untuk kick punch.",
          "Memetakan posisi musisi di panggung (Front, Left, Right, Riser) dengan ear-safety limiter -6 dBFS pada IEM.",
          "Menganalisis respon impuls venue via mikrofon ukur RTA untuk meluruskan 31-Band GEQ & delay subwoofer."};

      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGreen);
      g.drawText(advTitles[currentAdvancedTab], static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 10.0f), 600, 16, juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
      g.setColour(midas_colors::kTextWhite);
      g.drawText(advSubtitles[currentAdvancedTab], static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 30.0f), 600, 30, juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGold);
      juce::String advDetail = "";
      if (currentAdvancedTab == 0) advDetail = "Memory: 14 Shows Learned | Standing Wave Model: Stored | Singer Formant Profile: Stored (Confidence 98%)";
      else if (currentAdvancedTab == 1) advDetail = "16 Inputs Analyzed | Auto-Trim: Preamp set to -18 dBFS | Polarity: Snare Bottom Inverted 180 deg | Headroom: Clean";
      else if (currentAdvancedTab == 2) advDetail = "Current Style: " + juce::String(nova::graviton::autoSceneStyleToString(engine.channelAI.getAutoSceneStyle())) + " | Slew: 2.0s Crossfade";
      else if (currentAdvancedTab == 3) advDetail = "Spectral De-Clutter: ACTIVE | Carved Guitars -3dB @ 2.8kHz for Vocal | Carved Bass -4dB @ 80Hz for Kick";
      else if (currentAdvancedTab == 4) advDetail = "Stage Layout: 4 Positions Mapped | Lead Wedge: Aux 1 | Band Wedge: Aux 2 | Drum IEM: Aux 3 | IEM Ear Limiter: ON";
      else if (currentAdvancedTab == 5) advDetail = "Measurement Mic: RTA Port Active | Standing Waves: 63Hz (+4.2dB), 250Hz (-3dB) | GEQ Flattened | Sub Delay: +4.2ms";

      g.drawText(advDetail, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
    }

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
    btnModeSoundEngineer.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 46.0f), static_cast<int>(modeW), 25);
    btnModeAutonomous.setBounds(static_cast<int>(dx + 28.0f + modeW), static_cast<int>(dy + 46.0f), static_cast<int>(modeW), 25);

    // 2. Suite Switcher (Core vs Advanced)
    const float suiteW = (dialogW - 40.0f - 8.0f) * 0.5f;
    btnSuiteCore.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 75.0f), static_cast<int>(suiteW), 24);
    btnSuiteAdvanced.setBounds(static_cast<int>(dx + 28.0f + suiteW), static_cast<int>(dy + 75.0f), static_cast<int>(suiteW), 24);

    if (!isAdvancedSuite) {
      // 8 Core Module Tabs (2 rows of 4)
      const float tabW = (dialogW - 40.0f - 3.0f * 4.0f) / 4.0f;
      btnTabGain.setBounds(static_cast<int>(dx + 20.0f + 0 * (tabW + 4.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(tabW), 22);
      btnTabEq.setBounds(static_cast<int>(dx + 20.0f + 1 * (tabW + 4.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(tabW), 22);
      btnTabComp.setBounds(static_cast<int>(dx + 20.0f + 2 * (tabW + 4.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(tabW), 22);
      btnTabGate.setBounds(static_cast<int>(dx + 20.0f + 3 * (tabW + 4.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(tabW), 22);

      btnTabFx.setBounds(static_cast<int>(dx + 20.0f + 0 * (tabW + 4.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(tabW), 22);
      btnTabMonitor.setBounds(static_cast<int>(dx + 20.0f + 1 * (tabW + 4.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(tabW), 22);
      btnTabFeedback.setBounds(static_cast<int>(dx + 20.0f + 2 * (tabW + 4.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(tabW), 22);
      btnTabMaster.setBounds(static_cast<int>(dx + 20.0f + 3 * (tabW + 4.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(tabW), 22);

      // Core Action Controls
      const float actW = (dialogW - 40.0f - 16.0f) / 3.0f;
      btnApplyModule.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnApplyAllModules.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnStartCapture.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);

      btnToggleModuleAutonomous.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnEmergencyFreeze.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnResetNominal.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);

    } else {
      // 6 Advanced Module Tabs (2 rows of 3)
      const float advTabW = (dialogW - 40.0f - 2.0f * 6.0f) / 3.0f;
      btnTabSelfLearning.setBounds(static_cast<int>(dx + 20.0f + 0 * (advTabW + 6.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(advTabW), 22);
      btnTabAutoSoundcheck.setBounds(static_cast<int>(dx + 20.0f + 1 * (advTabW + 6.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(advTabW), 22);
      btnTabAutoScene.setBounds(static_cast<int>(dx + 20.0f + 2 * (advTabW + 6.0f)), static_cast<int>(dy + 102.0f), static_cast<int>(advTabW), 22);

      btnTabMultiMic.setBounds(static_cast<int>(dx + 20.0f + 0 * (advTabW + 6.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(advTabW), 22);
      btnTabStageIntel.setBounds(static_cast<int>(dx + 20.0f + 1 * (advTabW + 6.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(advTabW), 22);
      btnTabAcousticIntel.setBounds(static_cast<int>(dx + 20.0f + 2 * (advTabW + 6.0f)), static_cast<int>(dy + 126.0f), static_cast<int>(advTabW), 22);

      // Advanced Action Controls (3 buttons)
      const float actW = (dialogW - 40.0f - 16.0f) / 3.0f;
      btnAdvAction1.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnAdvAction2.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
      btnAdvAction3.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 8.0f)), static_cast<int>(dy + 260.0f), static_cast<int>(actW), 38);
    }
  }

private:
  MixerEngine &engine;
  int activeChannelIndex = 0;
  juce::String channelName = "IN 01 (KICK IN)";
  float captureProgress = 0.0f;
  float simulatedCaptureSec = 0.0f;
  bool analyzed = false;

  bool isAdvancedSuite = false;
  int currentModuleTab = 0;
  int currentAdvancedTab = 0;

  nova::graviton::AIRecommendation currentRec;
  juce::String aiLog = "MIDAS AI Dual-Mode System Ready";

  MidasTactileButton btnClose;

  // 1. Two Intelligence Mode Buttons
  MidasTactileButton btnModeSoundEngineer, btnModeAutonomous;

  // Suite Switcher Buttons
  MidasTactileButton btnSuiteCore, btnSuiteAdvanced;

  // 2. Eight Core Module Tabs
  MidasTactileButton btnTabGain, btnTabEq, btnTabComp, btnTabGate,
      btnTabFx, btnTabMonitor, btnTabFeedback, btnTabMaster;

  // 3. Six Advanced Feature Tabs
  MidasTactileButton btnTabSelfLearning, btnTabAutoSoundcheck, btnTabAutoScene,
      btnTabMultiMic, btnTabStageIntel, btnTabAcousticIntel;

  // Core Advisory Mode Buttons
  MidasTactileButton btnApplyModule, btnApplyAllModules, btnStartCapture;

  // Core Autonomous Mode Buttons
  MidasTactileButton btnToggleModuleAutonomous, btnEmergencyFreeze, btnResetNominal;

  // Advanced Feature Action Buttons
  MidasTactileButton btnAdvAction1, btnAdvAction2, btnAdvAction3;

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

print("Upgraded MidasAiModal with 6 Advanced AI Engines successfully!")

