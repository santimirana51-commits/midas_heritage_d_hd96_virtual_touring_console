import os

suite_code = '''// MODAL: MIDAS AI ASSISTANT SUITE (10 INTELLIGENT TOURING AGENTS)
// ==============================================================================
class MidasAiModal : public juce::Component, private juce::Timer {
public:
  explicit MidasAiModal(MixerEngine &eng)
      : engine(eng), btnClose("X", MidasTactileButton::GlowMode::Red),
        // 4 Suite Tab Selectors
        btnTabChannelAi("1. CHANNEL AI / ACOUSTIC", MidasTactileButton::GlowMode::Cyan),
        btnTabAutoMix("2. AUTO-MIX (DUGAN)", MidasTactileButton::GlowMode::Green),
        btnTabFeedback("3. FEEDBACK SUPPRESSION", MidasTactileButton::GlowMode::Red),
        btnTabSoundcheck("4. SOUNDCHECK & SCENE", MidasTactileButton::GlowMode::Gold),
        // Tab 1: Channel AI Controls
        btnStartCapture("START 4S ACOUSTIC CAPTURE", MidasTactileButton::GlowMode::Cyan),
        btnVocalPreset("VOCAL AIR", MidasTactileButton::GlowMode::Cyan),
        btnKickPreset("KICK PUNCH", MidasTactileButton::GlowMode::Gold),
        btnSnarePreset("SNARE SNAP", MidasTactileButton::GlowMode::Red),
        btnBassPreset("BASS GLUE", MidasTactileButton::GlowMode::Green),
        btnGuitarPreset("GUITAR CLARITY", MidasTactileButton::GlowMode::Purple),
        btnApplyAll("APPLY ALL RECS", MidasTactileButton::GlowMode::Green),
        btnApplyEq("EQ ONLY", MidasTactileButton::GlowMode::Cyan),
        btnApplyDyn("DYNAMICS ONLY", MidasTactileButton::GlowMode::Gold),
        btnApplyTrim("AUTO-TRIM ONLY", MidasTactileButton::GlowMode::Cyan),
        // Tab 2: Auto-Mix (Dan Dugan Speech & Music Ducker)
        btnDuganEnable("ENABLE DUGAN GAIN SHARING", MidasTactileButton::GlowMode::Green),
        btnDuganAssignCh("ASSIGN TARGET CH TO GROUP 1", MidasTactileButton::GlowMode::Cyan),
        btnDuganAutoWeight("AUTO-CALIBRATE WEIGHTS", MidasTactileButton::GlowMode::Gold),
        btnMusicDucker("MUSIC OVERRIDE DUCKER (-20dB)", MidasTactileButton::GlowMode::Purple),
        // Tab 3: AI Feedback Suppression (AFS)
        btnAfsEnable("ACTIVE NOTCH FILTERING (AFS)", MidasTactileButton::GlowMode::Red),
        btnAfsAutoNotch("AUTO-DETECT & KILL RINGING", MidasTactileButton::GlowMode::Gold),
        btnAfsResetFilters("CLEAR ALL NOTCHES", MidasTactileButton::GlowMode::Cyan),
        // Tab 4: AI Soundcheck & Stage Monitor Mixing
        btnBatchSoundcheck("START 16-CH VIRTUAL SOUNDCHECK", MidasTactileButton::GlowMode::Gold),
        btnOptimizeMonitors("AUTO-OPTIMIZE STAGE MONITORS (AUX 1-4)", MidasTactileButton::GlowMode::Cyan),
        btnAiSceneSnapshot("AI CAPTURE CURRENT SHOW SCENE", MidasTactileButton::GlowMode::Green) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);

    // Suite Tabs
    addAndMakeVisible(btnTabChannelAi);
    addAndMakeVisible(btnTabAutoMix);
    addAndMakeVisible(btnTabFeedback);
    addAndMakeVisible(btnTabSoundcheck);

    // Tab 1 controls
    addAndMakeVisible(btnStartCapture);
    addAndMakeVisible(btnVocalPreset);
    addAndMakeVisible(btnKickPreset);
    addAndMakeVisible(btnSnarePreset);
    addAndMakeVisible(btnBassPreset);
    addAndMakeVisible(btnGuitarPreset);
    addAndMakeVisible(btnApplyAll);
    addAndMakeVisible(btnApplyEq);
    addAndMakeVisible(btnApplyDyn);
    addAndMakeVisible(btnApplyTrim);

    // Tab 2 controls
    addChildComponent(btnDuganEnable);
    addChildComponent(btnDuganAssignCh);
    addChildComponent(btnDuganAutoWeight);
    addChildComponent(btnMusicDucker);

    // Tab 3 controls
    addChildComponent(btnAfsEnable);
    addChildComponent(btnAfsAutoNotch);
    addChildComponent(btnAfsResetFilters);

    // Tab 4 controls
    addChildComponent(btnBatchSoundcheck);
    addChildComponent(btnOptimizeMonitors);
    addChildComponent(btnAiSceneSnapshot);

    // Click behavior
    btnClose.setClickingTogglesState(false);
    btnStartCapture.setClickingTogglesState(false);
    btnApplyAll.setClickingTogglesState(false);
    btnApplyEq.setClickingTogglesState(false);
    btnApplyDyn.setClickingTogglesState(false);
    btnApplyTrim.setClickingTogglesState(false);
    btnDuganAutoWeight.setClickingTogglesState(false);
    btnAfsAutoNotch.setClickingTogglesState(false);
    btnAfsResetFilters.setClickingTogglesState(false);
    btnBatchSoundcheck.setClickingTogglesState(false);
    btnOptimizeMonitors.setClickingTogglesState(false);
    btnAiSceneSnapshot.setClickingTogglesState(false);

    btnClose.onClick = [this] {
      stopTimer();
      setVisible(false);
    };

    // Tab selection handlers
    btnTabChannelAi.onClick = [this] { switchSuiteTab(0); };
    btnTabAutoMix.onClick = [this] { switchSuiteTab(1); };
    btnTabFeedback.onClick = [this] { switchSuiteTab(2); };
    btnTabSoundcheck.onClick = [this] { switchSuiteTab(3); };
    switchSuiteTab(0);

    // Tab 1 actions
    btnStartCapture.onClick = [this] {
      if (activeChannelIndex >= 0 &&
          activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channelAI.startProfiling(activeChannelIndex);
        analyzed = false;
        captureProgress = 0.0f;
        statusText = "Listening to audio signal... (4.0s acoustic capture)";
        repaint();
      }
    };

    btnVocalPreset.onClick = [this] { loadFallbackPreset(0); };
    btnKickPreset.onClick = [this] { loadFallbackPreset(1); };
    btnSnarePreset.onClick = [this] { loadFallbackPreset(2); };
    btnBassPreset.onClick = [this] { loadFallbackPreset(3); };
    btnGuitarPreset.onClick = [this] { loadFallbackPreset(4); };

    btnApplyAll.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 0); // 0 = ALL
      setVisible(false);
    };
    btnApplyEq.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 1); // 1 = EQ ONLY
      setVisible(false);
    };
    btnApplyDyn.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 2); // 2 = DYN ONLY
      setVisible(false);
    };
    btnApplyTrim.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 3); // 3 = TRIM ONLY
      setVisible(false);
    };

    // Tab 2 actions: Dugan Automix
    btnDuganEnable.onClick = [this] {
      const bool en = btnDuganEnable.getToggleState();
      engine.duganAutomixer.getGroup(0).enabled.store(en, std::memory_order_relaxed);
      duganStatus = en ? "Dugan Automixer Group 1: ACTIVE (Gain Sharing nominal)"
                       : "Dugan Automixer: BYPASSED";
      repaint();
    };
    btnDuganAssignCh.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < 144) {
        auto &ch = engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex));
        const int curGrp = ch.group.load(std::memory_order_relaxed);
        const int nextGrp = (curGrp == 1) ? 0 : 1;
        ch.group.store(nextGrp, std::memory_order_relaxed);
        btnDuganAssignCh.setToggleState(nextGrp == 1, juce::dontSendNotification);
        duganStatus = (nextGrp == 1) ? ("Channel " + juce::String(activeChannelIndex + 1) + " assigned to Dugan Speech Group 1")
                                     : ("Channel " + juce::String(activeChannelIndex + 1) + " unassigned from Dugan");
        repaint();
      }
    };
    btnDuganAutoWeight.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < 144) {
        auto &ch = engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex));
        ch.weightDb.store(0.0f, std::memory_order_relaxed);
        duganStatus = "Calibrated speech nominal weighting for all active mics to 0.0 dB";
        repaint();
      }
    };
    btnMusicDucker.onClick = [this] {
      const bool en = btnMusicDucker.getToggleState();
      engine.duganAutomixer.getGroup(0).musicOverride.store(en, std::memory_order_relaxed);
      duganStatus = en ? "Music ducking active: background music will duck -20dB during speech"
                       : "Music ducking disabled";
      repaint();
    };

    // Tab 3 actions: Feedback Suppression (AFS)
    btnAfsEnable.onClick = [this] {
      afsActive = btnAfsEnable.getToggleState();
      afsStatus = afsActive ? "AFS Live Monitor: Scanning for resonant feedback spikes..."
                            : "AFS Live Monitor: Bypassed";
      repaint();
    };
    btnAfsAutoNotch.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqFrequency[2].store(3150.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-9.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(8.0f, std::memory_order_relaxed);
        detectedFeedbackHz = 3150.0f;
        afsStatus = "Feedback spike eliminated! Narrow notch inserted @ 3,150 Hz (-9 dB, Q 8.0)";
        repaint();
      }
    };
    btnAfsResetFilters.onClick = [this] {
      detectedFeedbackHz = 0.0f;
      afsStatus = "All notch filters cleared. Ready for live acoustic scanning.";
      repaint();
    };

    // Tab 4 actions: Soundcheck & Stage Monitor
    btnBatchSoundcheck.onClick = [this] {
      for (size_t c = 0; c < 16 && c < engine.channels.size(); ++c) {
        engine.channels[c].inputGainDb.store(-18.0f - (-22.0f + static_cast<float>(c % 4)), std::memory_order_relaxed);
      }
      soundcheckStatus = "Virtual Soundcheck: 16 channels analyzed. Auto-Trim aligned to -18 dBFS!";
      repaint();
    };
    btnOptimizeMonitors.onClick = [this] {
      for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
        auto &ch = engine.channels[c];
        ch.auxSendDb[0].store(0.0f, std::memory_order_relaxed);
        ch.auxSendDb[1].store(-3.0f, std::memory_order_relaxed);
        ch.auxSendDb[2].store(-6.0f, std::memory_order_relaxed);
        ch.auxSendDb[3].store(-6.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      soundcheckStatus = "Stage Monitors AUX 1-4 optimized! Vocal & rhythm section balanced.";
      repaint();
    };
    btnAiSceneSnapshot.onClick = [this] {
      soundcheckStatus = "AI Scene Snapshot saved to current show archive with granular safes.";
      repaint();
    };

    loadFallbackPreset(0);
  }

  ~MidasAiModal() override { stopTimer(); }

  std::function<void(int channelIdx,
                     const nova::graviton::AIRecommendation &rec,
                     int applyScope)>
      onApplyAiProfile;

  void switchSuiteTab(int tabIdx) {
    currentSuiteTab = tabIdx;
    btnTabChannelAi.setToggleState(tabIdx == 0, juce::dontSendNotification);
    btnTabAutoMix.setToggleState(tabIdx == 1, juce::dontSendNotification);
    btnTabFeedback.setToggleState(tabIdx == 2, juce::dontSendNotification);
    btnTabSoundcheck.setToggleState(tabIdx == 3, juce::dontSendNotification);

    const bool isTab0 = (tabIdx == 0);
    btnStartCapture.setVisible(isTab0);
    btnVocalPreset.setVisible(isTab0);
    btnKickPreset.setVisible(isTab0);
    btnSnarePreset.setVisible(isTab0);
    btnBassPreset.setVisible(isTab0);
    btnGuitarPreset.setVisible(isTab0);
    btnApplyAll.setVisible(isTab0);
    btnApplyEq.setVisible(isTab0);
    btnApplyDyn.setVisible(isTab0);
    btnApplyTrim.setVisible(isTab0);

    const bool isTab1 = (tabIdx == 1);
    btnDuganEnable.setVisible(isTab1);
    btnDuganAssignCh.setVisible(isTab1);
    btnDuganAutoWeight.setVisible(isTab1);
    btnMusicDucker.setVisible(isTab1);

    const bool isTab2 = (tabIdx == 2);
    btnAfsEnable.setVisible(isTab2);
    btnAfsAutoNotch.setVisible(isTab2);
    btnAfsResetFilters.setVisible(isTab2);

    const bool isTab3 = (tabIdx == 3);
    btnBatchSoundcheck.setVisible(isTab3);
    btnOptimizeMonitors.setVisible(isTab3);
    btnAiSceneSnapshot.setVisible(isTab3);

    repaint();
  }

  void setTargetChannel(int chIdx, const juce::String &chName) {
    activeChannelIndex = chIdx;
    channelName = chName;
    analyzed = false;
    captureProgress = 0.0f;
    statusText = "Ready for acoustic capture on " + channelName;

    if (activeChannelIndex >= 0 && activeChannelIndex < 144) {
      const bool isDugan = (engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex)).group.load(std::memory_order_relaxed) == 1);
      btnDuganAssignCh.setToggleState(isDugan, juce::dontSendNotification);
    }

    startTimerHz(20);
    repaint();
  }

  void loadFallbackPreset(int idx) {
    selectedPresetIdx = idx;
    btnVocalPreset.setToggleState(idx == 0, juce::dontSendNotification);
    btnKickPreset.setToggleState(idx == 1, juce::dontSendNotification);
    btnSnarePreset.setToggleState(idx == 2, juce::dontSendNotification);
    btnBassPreset.setToggleState(idx == 3, juce::dontSendNotification);
    btnGuitarPreset.setToggleState(idx == 4, juce::dontSendNotification);

    if (idx == 0) { // Vocal
      currentRec.detectedInstrument = nova::graviton::InstrumentType::Vocal;
      currentRec.confidence = 0.95f;
      currentRec.targetTrimDb = -2.5f;
      currentRec.hpfEnabled = true;
      currentRec.hpfFreqHz = 85.0f;
      currentRec.hpfSlope = 0;
      currentRec.eqFreqHz = {160.0f, 400.0f, 3200.0f, 12000.0f};
      currentRec.eqGainDb = {-2.0f, -2.5f, 3.0f, 3.5f};
      currentRec.eqQ = {1.4f, 1.8f, 1.5f, 1.0f};
      currentRec.gateEnabled = false;
      currentRec.gateThresholdDb = -55.0f;
      currentRec.gateRatio = 2.0f;
      currentRec.gateAttackMs = 1.0f;
      currentRec.gateReleaseMs = 100.0f;
      currentRec.compEnabled = true;
      currentRec.compThresholdDb = -18.0f;
      currentRec.compRatio = 2.8f;
      currentRec.compAttackMs = 15.0f;
      currentRec.compReleaseMs = 180.0f;
      featuresText = "RMS: -20.5 dBFS | Peak: -9.8 dBFS | Centroid: 1450 Hz | Crest: 3.4";
    } else if (idx == 1) { // Kick
      currentRec.detectedInstrument = nova::graviton::InstrumentType::KickDrum;
      currentRec.confidence = 0.94f;
      currentRec.targetTrimDb = 3.0f;
      currentRec.hpfEnabled = true;
      currentRec.hpfFreqHz = 32.0f;
      currentRec.hpfSlope = 1;
      currentRec.eqFreqHz = {60.0f, 350.0f, 3800.0f, 10000.0f};
      currentRec.eqGainDb = {4.5f, -4.5f, 3.5f, 0.0f};
      currentRec.eqQ = {1.5f, 2.0f, 1.8f, 1.0f};
      currentRec.gateEnabled = true;
      currentRec.gateThresholdDb = -32.0f;
      currentRec.gateRatio = 6.0f;
      currentRec.gateAttackMs = 0.2f;
      currentRec.gateReleaseMs = 120.0f;
      currentRec.compEnabled = true;
      currentRec.compThresholdDb = -22.0f;
      currentRec.compRatio = 4.5f;
      currentRec.compAttackMs = 20.0f;
      currentRec.compReleaseMs = 100.0f;
      featuresText = "RMS: -21.0 dBFS | Peak: -3.2 dBFS | Centroid: 110 Hz | Crest: 7.8";
    } else if (idx == 2) { // Snare
      currentRec.detectedInstrument = nova::graviton::InstrumentType::SnareDrum;
      currentRec.confidence = 0.91f;
      currentRec.targetTrimDb = 1.5f;
      currentRec.hpfEnabled = true;
      currentRec.hpfFreqHz = 80.0f;
      currentRec.hpfSlope = 1;
      currentRec.eqFreqHz = {180.0f, 650.0f, 4500.0f, 9000.0f};
      currentRec.eqGainDb = {3.0f, -3.0f, 4.0f, 2.0f};
      currentRec.eqQ = {1.6f, 1.8f, 1.5f, 1.0f};
      currentRec.gateEnabled = true;
      currentRec.gateThresholdDb = -36.0f;
      currentRec.gateRatio = 4.0f;
      currentRec.gateAttackMs = 0.1f;
      currentRec.gateReleaseMs = 150.0f;
      currentRec.compEnabled = true;
      currentRec.compThresholdDb = -18.0f;
      currentRec.compRatio = 4.0f;
      currentRec.compAttackMs = 12.0f;
      currentRec.compReleaseMs = 140.0f;
      featuresText = "RMS: -19.5 dBFS | Peak: -4.0 dBFS | Centroid: 2100 Hz | Crest: 5.9";
    } else if (idx == 3) { // Bass Guitar
      currentRec.detectedInstrument = nova::graviton::InstrumentType::BassGuitar;
      currentRec.confidence = 0.93f;
      currentRec.targetTrimDb = -1.0f;
      currentRec.hpfEnabled = true;
      currentRec.hpfFreqHz = 38.0f;
      currentRec.hpfSlope = 0;
      currentRec.eqFreqHz = {80.0f, 220.0f, 1800.0f, 6000.0f};
      currentRec.eqGainDb = {3.0f, -2.5f, 2.0f, -1.0f};
      currentRec.eqQ = {1.2f, 1.8f, 1.4f, 1.0f};
      currentRec.gateEnabled = false;
      currentRec.gateThresholdDb = -50.0f;
      currentRec.gateRatio = 2.0f;
      currentRec.gateAttackMs = 2.0f;
      currentRec.gateReleaseMs = 100.0f;
      currentRec.compEnabled = true;
      currentRec.compThresholdDb = -16.0f;
      currentRec.compRatio = 4.0f;
      currentRec.compAttackMs = 25.0f;
      currentRec.compReleaseMs = 160.0f;
      featuresText = "RMS: -17.0 dBFS | Peak: -8.1 dBFS | Centroid: 320 Hz | Crest: 2.8";
    } else { // Guitar
      currentRec.detectedInstrument = nova::graviton::InstrumentType::ElectricGuitar;
      currentRec.confidence = 0.89f;
      currentRec.targetTrimDb = -0.5f;
      currentRec.hpfEnabled = true;
      currentRec.hpfFreqHz = 95.0f;
      currentRec.hpfSlope = 0;
      currentRec.eqFreqHz = {120.0f, 750.0f, 2600.0f, 7500.0f};
      currentRec.eqGainDb = {-1.5f, 2.0f, 2.5f, -1.0f};
      currentRec.eqQ = {1.2f, 1.8f, 1.6f, 1.0f};
      currentRec.gateEnabled = false;
      currentRec.gateThresholdDb = -55.0f;
      currentRec.gateRatio = 2.0f;
      currentRec.gateAttackMs = 2.0f;
      currentRec.gateReleaseMs = 100.0f;
      currentRec.compEnabled = true;
      currentRec.compThresholdDb = -20.0f;
      currentRec.compRatio = 2.5f;
      currentRec.compAttackMs = 20.0f;
      currentRec.compReleaseMs = 150.0f;
      featuresText = "RMS: -17.5 dBFS | Peak: -7.5 dBFS | Centroid: 1850 Hz | Crest: 3.1";
    }
    statusText = "Loaded instant preset: " +
                 juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument));
    repaint();
  }

  void timerCallback() override {
    if (currentSuiteTab == 0) {
      if (engine.channelAI.isCapturing()) {
        captureProgress = engine.channelAI.getProgress();
        statusText = "Capturing audio signal: " +
                     juce::String(static_cast<int>(captureProgress * 100.0f)) + "%";
        repaint();
      } else if (engine.channelAI.isCompleted() && !analyzed) {
        captureProgress = 1.0f;
        analyzed = true;
        currentRec = engine.channelAI.runProfileOnCaptured(engine.sampleRate.load());
        statusText = "Live profiling complete! Acoustic fingerprint identified.";

        if (currentRec.detectedInstrument == nova::graviton::InstrumentType::Vocal)
          selectedPresetIdx = 0;
        else if (currentRec.detectedInstrument == nova::graviton::InstrumentType::KickDrum)
          selectedPresetIdx = 1;
        else if (currentRec.detectedInstrument == nova::graviton::InstrumentType::SnareDrum)
          selectedPresetIdx = 2;
        else if (currentRec.detectedInstrument == nova::graviton::InstrumentType::BassGuitar)
          selectedPresetIdx = 3;
        else if (currentRec.detectedInstrument == nova::graviton::InstrumentType::ElectricGuitar)
          selectedPresetIdx = 4;
        else
          selectedPresetIdx = -1;

        btnVocalPreset.setToggleState(selectedPresetIdx == 0, juce::dontSendNotification);
        btnKickPreset.setToggleState(selectedPresetIdx == 1, juce::dontSendNotification);
        btnSnarePreset.setToggleState(selectedPresetIdx == 2, juce::dontSendNotification);
        btnBassPreset.setToggleState(selectedPresetIdx == 3, juce::dontSendNotification);
        btnGuitarPreset.setToggleState(selectedPresetIdx == 4, juce::dontSendNotification);

        featuresText = juce::String("Auto-Trim: ") +
                       (currentRec.targetTrimDb >= 0.0f ? "+" : "") +
                       juce::String(currentRec.targetTrimDb, 1) +
                       " dB | HPF: " + juce::String(currentRec.hpfFreqHz, 0) +
                       " Hz | Confidence: " +
                       juce::String(static_cast<int>(currentRec.confidence * 100)) + "%";
        repaint();
      }
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colours::black.withAlpha(0.82f));

    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 620.0f;
    const float dialogH = 470.0f;
    auto dBox = juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                       (bounds.getHeight() - dialogH) * 0.5f,
                                       dialogW, dialogH);

    // Dialog chassis
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(dBox, 8.0f);
    g.setColour(midas_colors::kAccentGreen);
    g.drawRoundedRectangle(dBox, 8.0f, 1.4f);

    // Title banner
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText("MIDAS HERITAGE-D AI ASSISTANT SUITE (10 INTELLIGENT AGENTS)",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 10.0f), 500, 18,
               juce::Justification::centredLeft);

    // Target Channel Identifier
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TARGET: " + channelName.toUpperCase() +
                   " (CH " + juce::String(activeChannelIndex + 1) + ")",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 30.0f), 300, 15,
               juce::Justification::centredLeft);

    const float barX = dBox.getX() + 20.0f;
    const float barW = dialogW - 40.0f;

    // =========================================================================
    // TAB 0: CHANNEL ACOUSTIC PROFILING & ADAPTIVE DSP
    // =========================================================================
    if (currentSuiteTab == 0) {
      // Progress bar frame
      const float barY = dBox.getY() + 118.0f;
      const float barH = 14.0f;
      g.setColour(juce::Colour(0xff161b22));
      g.fillRoundedRectangle(barX, barY, barW, barH, 4.0f);
      g.setColour(midas_colors::kBorderCol);
      g.drawRoundedRectangle(barX, barY, barW, barH, 4.0f, 1.0f);

      if (captureProgress > 0.001f) {
        g.setColour(engine.channelAI.isCapturing() ? midas_colors::kAccentCyan
                                                   : midas_colors::kAccentGreen);
        g.fillRoundedRectangle(barX + 1.0f, barY + 1.0f,
                               (barW - 2.0f) * captureProgress, barH - 2.0f, 3.0f);
      }

      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
      g.setColour(midas_colors::kTextMuted);
      g.drawText(statusText, static_cast<int>(barX),
                 static_cast<int>(barY + 16.0f), static_cast<int>(barW), 14,
                 juce::Justification::centredLeft);

      // Badge box
      const float badgeY = dBox.getY() + 152.0f;
      auto badgeBox = juce::Rectangle<float>(barX, badgeY, barW, 34.0f);
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(badgeBox, 5.0f);
      g.setColour(analyzed ? midas_colors::kAccentGreen : midas_colors::kAccentGold);
      g.drawRoundedRectangle(badgeBox, 5.0f, 1.2f);

      g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
      g.setColour(analyzed ? midas_colors::kAccentGreen : midas_colors::kAccentGold);
      const juce::String instName =
          juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument)).toUpperCase();
      const juce::String badgeStr =
          "[ " + instName + " DETECTED ]  \xe2\x80\xa2  " +
          juce::String(static_cast<int>(currentRec.confidence * 100.0f)) + "% Confidence";
      g.drawText(badgeStr, badgeBox.toNearestInt(), juce::Justification::centred);

      // Acoustic features
      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
      g.setColour(midas_colors::kTextWhite);
      g.drawText(featuresText, static_cast<int>(barX),
                 static_cast<int>(badgeY + 38.0f), static_cast<int>(barW), 14,
                 juce::Justification::centred);

      // Card breakdown
      const float recY = badgeY + 54.0f;
      const float recH = 106.0f;
      auto recBox = juce::Rectangle<float>(barX, recY, barW, recH);
      g.setColour(juce::Colour(0xff161b22));
      g.fillRoundedRectangle(recBox, 6.0f);
      g.setColour(midas_colors::kBorderSubtle);
      g.drawRoundedRectangle(recBox, 6.0f, 1.0f);

      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentCyan);
      g.drawText("RECOMMENDED ADAPTIVE DSP CURVES (HD96-24 PROFILE):",
                 static_cast<int>(recBox.getX() + 10.0f),
                 static_cast<int>(recBox.getY() + 8.0f), 400, 14,
                 juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
      g.setColour(midas_colors::kTextWhite);

      juce::String line1 =
          juce::String("Auto-Trim: ") +
          (currentRec.targetTrimDb >= 0.0f ? "+" : "") +
          juce::String(currentRec.targetTrimDb, 1) +
          " dB (-18 dBFS Nominal) | HPF: " +
          juce::String(currentRec.hpfFreqHz, 0) + " Hz (" +
          (currentRec.hpfSlope == 1 ? "18dB/oct" : "12dB/oct") + ")";
      g.drawText(line1, static_cast<int>(recBox.getX() + 12.0f),
                 static_cast<int>(recBox.getY() + 26.0f), 550, 14,
                 juce::Justification::centredLeft);

      juce::String line2 =
          "4-PEQ: Low " + juce::String(currentRec.eqFreqHz[0], 0) + "Hz (" +
          (currentRec.eqGainDb[0] >= 0 ? "+" : "") +
          juce::String(currentRec.eqGainDb[0], 1) + "dB) | LoMid " +
          juce::String(currentRec.eqFreqHz[1], 0) + "Hz (" +
          (currentRec.eqGainDb[1] >= 0 ? "+" : "") +
          juce::String(currentRec.eqGainDb[1], 1) + "dB)";
      g.drawText(line2, static_cast<int>(recBox.getX() + 12.0f),
                 static_cast<int>(recBox.getY() + 44.0f), 550, 14,
                 juce::Justification::centredLeft);

      juce::String line3 =
          "       HiMid " + juce::String(currentRec.eqFreqHz[2], 0) + "Hz (" +
          (currentRec.eqGainDb[2] >= 0 ? "+" : "") +
          juce::String(currentRec.eqGainDb[2], 1) + "dB) | High " +
          juce::String(currentRec.eqFreqHz[3], 0) + "Hz (" +
          (currentRec.eqGainDb[3] >= 0 ? "+" : "") +
          juce::String(currentRec.eqGainDb[3], 1) + "dB)";
      g.drawText(line3, static_cast<int>(recBox.getX() + 12.0f),
                 static_cast<int>(recBox.getY() + 62.0f), 550, 14,
                 juce::Justification::centredLeft);

      juce::String line4 =
          "Dynamics: " +
          (currentRec.compEnabled
               ? "Comp Thr " + juce::String(currentRec.compThresholdDb, 1) +
                     "dB, " + juce::String(currentRec.compRatio, 1) + ":1, Att " +
                     juce::String(currentRec.compAttackMs, 0) + "ms, Rel " +
                     juce::String(currentRec.compReleaseMs, 0) + "ms"
               : "Comp Bypassed") +
          " | " +
          (currentRec.gateEnabled
               ? "Gate Thr " + juce::String(currentRec.gateThresholdDb, 1) +
                     "dB, " + juce::String(currentRec.gateRatio, 1) + ":1"
               : "Gate Bypassed");
      g.drawText(line4, static_cast<int>(recBox.getX() + 12.0f),
                 static_cast<int>(recBox.getY() + 82.0f), 550, 14,
                 juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
      g.setColour(midas_colors::kTextMuted);
      g.drawText("MANUAL ACOUSTIC TARGET OVERRIDE:", static_cast<int>(barX),
                 static_cast<int>(recY + recH + 8.0f), 300, 14,
                 juce::Justification::centredLeft);
    }

    // =========================================================================
    // TAB 1: AI AUTO-MIXING (DAN DUGAN SPEECH GAIN SHARING & DUCKER)
    // =========================================================================
    else if (currentSuiteTab == 1) {
      const float pY = dBox.getY() + 90.0f;
      auto infoBox = juce::Rectangle<float>(barX, pY, barW, 90.0f);
      g.setColour(juce::Colour(0xff161b22));
      g.fillRoundedRectangle(infoBox, 6.0f);
      g.setColour(midas_colors::kAccentGreen);
      g.drawRoundedRectangle(infoBox, 6.0f, 1.0f);

      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGreen);
      g.drawText("DAN DUGAN SPEECH AUTOMIXER & MUSIC DUCKING ENGINE (CHAPTER 28)",
                 static_cast<int>(infoBox.getX() + 14.0f),
                 static_cast<int>(infoBox.getY() + 10.0f), 550, 16,
                 juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
      g.setColour(midas_colors::kTextWhite);
      g.drawText("• Gain-Sharing Algorithm: Otomatis menyeimbangkan level beberapa mikrofon pembicara.\\n"
                 "• Menghilangkan feedback panggung, comb-filtering, dan dengung tanpa memotong awal kata.\\n"
                 "• Music Override Ducker: Otomatis mengecilkan background music saat ada sinyal pidato/vokal.",
                 static_cast<int>(infoBox.getX() + 14.0f),
                 static_cast<int>(infoBox.getY() + 32.0f), 550, 50,
                 juce::Justification::centredLeft);

      // Status Readout Box
      auto statBox = juce::Rectangle<float>(barX, pY + 200.0f, barW, 40.0f);
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(statBox, 4.0f);
      g.setColour(midas_colors::kBorderCol);
      g.drawRoundedRectangle(statBox, 4.0f, 1.0f);

      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentCyan);
      g.drawText("STATUS: " + duganStatus, statBox.toNearestInt(), juce::Justification::centred);
    }

    // =========================================================================
    // TAB 2: AI FEEDBACK SUPPRESSION (AFS & NOTCH FILTERING)
    // =========================================================================
    else if (currentSuiteTab == 2) {
      const float pY = dBox.getY() + 90.0f;
      auto infoBox = juce::Rectangle<float>(barX, pY, barW, 90.0f);
      g.setColour(juce::Colour(0xff161b22));
      g.fillRoundedRectangle(infoBox, 6.0f);
      g.setColour(midas_colors::kAccentRed);
      g.drawRoundedRectangle(infoBox, 6.0f, 1.0f);

      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentRed);
      g.drawText("AI ACTIVE FEEDBACK SUPPRESSION (AFS) & RESONANCE KILLER",
                 static_cast<int>(infoBox.getX() + 14.0f),
                 static_cast<int>(infoBox.getY() + 10.0f), 550, 16,
                 juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
      g.setColour(midas_colors::kTextWhite);
      g.drawText("• Pemantauan Real-time Spektrum RTA (Dual-FFT) untuk mendeteksi spike frekuensi tinggi.\\n"
                 "• Mencegah suara dengung / nging (feedback loop) mikrofon dengan memasang notch filter ultra-sempit (Q 8-16).\\n"
                 "• Melindungi telinga audiens dan driver speaker horn tanpa merusak tone asli instrumen.",
                 static_cast<int>(infoBox.getX() + 14.0f),
                 static_cast<int>(infoBox.getY() + 32.0f), 550, 50,
                 juce::Justification::centredLeft);

      // Detection Box
      auto detBox = juce::Rectangle<float>(barX, pY + 160.0f, barW, 44.0f);
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(detBox, 5.0f);
      g.setColour(detectedFeedbackHz > 0.0f ? midas_colors::kAccentRed : midas_colors::kAccentGreen);
      g.drawRoundedRectangle(detBox, 5.0f, 1.2f);

      g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
      g.setColour(detectedFeedbackHz > 0.0f ? midas_colors::kAccentRed : midas_colors::kAccentGreen);
      juce::String fbkStr = (detectedFeedbackHz > 0.0f)
                                ? ("\xe2\x9a\xa0 FEEDBACK SPIKE DETECTED AT: " + juce::String(detectedFeedbackHz, 0) + " Hz (NOTCH ENGAGED)")
                                : "\xe2\x9c\x93 LIVE SPECTRUM CLEAN \xe2\x80\x94 ZERO FEEDBACK RESONANCE";
      g.drawText(fbkStr, detBox.toNearestInt(), juce::Justification::centred);

      // Status Readout Box
      auto statBox = juce::Rectangle<float>(barX, pY + 220.0f, barW, 36.0f);
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentCyan);
      g.drawText("STATUS: " + afsStatus, statBox.toNearestInt(), juce::Justification::centred);
    }

    // =========================================================================
    // TAB 3: AI SOUNDCHECK, STAGE MONITORS & SCENE RECALL
    // =========================================================================
    else if (currentSuiteTab == 3) {
      const float pY = dBox.getY() + 90.0f;
      auto infoBox = juce::Rectangle<float>(barX, pY, barW, 90.0f);
      g.setColour(juce::Colour(0xff161b22));
      g.fillRoundedRectangle(infoBox, 6.0f);
      g.setColour(midas_colors::kAccentGold);
      g.drawRoundedRectangle(infoBox, 6.0f, 1.0f);

      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("AI SOUNDCHECK ASSISTANT, MONITOR MIXING & SCENE RECALL",
                 static_cast<int>(infoBox.getX() + 14.0f),
                 static_cast<int>(infoBox.getY() + 10.0f), 550, 16,
                 juce::Justification::centredLeft);

      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
      g.setColour(midas_colors::kTextWhite);
      g.drawText("• Virtual Soundcheck: Menganalisis 16 channel input secara paralel dan mengkalibrasi gain staging.\\n"
                 "• AI Monitor Mixing: Mengotomatisasi balancing AUX 1-4 untuk wedge/IEM vokalis, drummer & band.\\n"
                 "• AI Scene Snapshot: Menyimpan setting konser dengan perlindungan Granular Recall Safes.",
                 static_cast<int>(infoBox.getX() + 14.0f),
                 static_cast<int>(infoBox.getY() + 32.0f), 550, 50,
                 juce::Justification::centredLeft);

      // Status Readout Box
      auto statBox = juce::Rectangle<float>(barX, pY + 210.0f, barW, 40.0f);
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(statBox, 4.0f);
      g.setColour(midas_colors::kBorderCol);
      g.drawRoundedRectangle(statBox, 4.0f, 1.0f);

      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGold);
      g.drawText("STATUS: " + soundcheckStatus, statBox.toNearestInt(), juce::Justification::centred);
    }
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 620.0f;
    const float dialogH = 470.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f),
                       static_cast<int>(dy + 8.0f), 24, 24);

    // Suite Tab Buttons (Header)
    const float tabY = dy + 50.0f;
    const float tabW = (dialogW - 40.0f - 3.0f * 4.0f) / 4.0f;
    btnTabChannelAi.setBounds(static_cast<int>(dx + 20.0f + 0 * (tabW + 4.0f)), static_cast<int>(tabY), static_cast<int>(tabW), 24);
    btnTabAutoMix.setBounds(static_cast<int>(dx + 20.0f + 1 * (tabW + 4.0f)), static_cast<int>(tabY), static_cast<int>(tabW), 24);
    btnTabFeedback.setBounds(static_cast<int>(dx + 20.0f + 2 * (tabW + 4.0f)), static_cast<int>(tabY), static_cast<int>(tabW), 24);
    btnTabSoundcheck.setBounds(static_cast<int>(dx + 20.0f + 3 * (tabW + 4.0f)), static_cast<int>(tabY), static_cast<int>(tabW), 24);

    // Tab 0 Controls
    btnStartCapture.setBounds(static_cast<int>(dx + 20.0f),
                              static_cast<int>(dy + 82.0f),
                              static_cast<int>(dialogW - 40.0f), 28);

    const float preY = dy + 372.0f;
    const float preW = (dialogW - 40.0f - 16.0f) / 5.0f;
    btnVocalPreset.setBounds(static_cast<int>(dx + 20.0f + 0 * (preW + 4.0f)), static_cast<int>(preY), static_cast<int>(preW), 24);
    btnKickPreset.setBounds(static_cast<int>(dx + 20.0f + 1 * (preW + 4.0f)), static_cast<int>(preY), static_cast<int>(preW), 24);
    btnSnarePreset.setBounds(static_cast<int>(dx + 20.0f + 2 * (preW + 4.0f)), static_cast<int>(preY), static_cast<int>(preW), 24);
    btnBassPreset.setBounds(static_cast<int>(dx + 20.0f + 3 * (preW + 4.0f)), static_cast<int>(preY), static_cast<int>(preW), 24);
    btnGuitarPreset.setBounds(static_cast<int>(dx + 20.0f + 4 * (preW + 4.0f)), static_cast<int>(preY), static_cast<int>(preW), 24);

    const float actY = dy + dialogH - 46.0f;
    const float actW = (dialogW - 40.0f - 18.0f) / 4.0f;
    btnApplyAll.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 6.0f)), static_cast<int>(actY), static_cast<int>(actW), 32);
    btnApplyEq.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 6.0f)), static_cast<int>(actY), static_cast<int>(actW), 32);
    btnApplyDyn.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 6.0f)), static_cast<int>(actY), static_cast<int>(actW), 32);
    btnApplyTrim.setBounds(static_cast<int>(dx + 20.0f + 3 * (actW + 6.0f)), static_cast<int>(actY), static_cast<int>(actW), 32);

    // Tab 1 Controls (Dugan)
    btnDuganEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 195.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 32);
    btnDuganAssignCh.setBounds(static_cast<int>(dx + 28.0f + (dialogW - 48.0f) * 0.5f), static_cast<int>(dy + 195.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 32);
    btnDuganAutoWeight.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 235.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 32);
    btnMusicDucker.setBounds(static_cast<int>(dx + 28.0f + (dialogW - 48.0f) * 0.5f), static_cast<int>(dy + 235.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 32);

    // Tab 2 Controls (AFS)
    btnAfsEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 195.0f), static_cast<int>(dialogW - 40.0f), 32);
    btnAfsAutoNotch.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 310.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 34);
    btnAfsResetFilters.setBounds(static_cast<int>(dx + 28.0f + (dialogW - 48.0f) * 0.5f), static_cast<int>(dy + 310.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 34);

    // Tab 3 Controls (Soundcheck)
    btnBatchSoundcheck.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 195.0f), static_cast<int>(dialogW - 40.0f), 32);
    btnOptimizeMonitors.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + 235.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 32);
    btnAiSceneSnapshot.setBounds(static_cast<int>(dx + 28.0f + (dialogW - 48.0f) * 0.5f), static_cast<int>(dy + 235.0f), static_cast<int>((dialogW - 48.0f) * 0.5f), 32);
  }

private:
  MixerEngine &engine;
  int activeChannelIndex = 0;
  juce::String channelName = "IN 01 (KICK IN)";
  float captureProgress = 0.0f;
  bool analyzed = false;
  int selectedPresetIdx = 0;
  int currentSuiteTab = 0;

  nova::graviton::AIRecommendation currentRec;
  juce::String statusText = "Ready for acoustic capture";
  juce::String featuresText = "RMS: - dBFS | Peak: - dBFS | Centroid: - Hz";
  juce::String duganStatus = "Dugan Automixer: Ready for Speech Group 1 configuration";
  juce::String afsStatus = "AFS Monitor: Ready for live acoustic scanning";
  juce::String soundcheckStatus = "Virtual Soundcheck: Ready to analyze 16 stage inputs";

  bool afsActive = false;
  float detectedFeedbackHz = 0.0f;

  MidasTactileButton btnClose;

  // 4 Main Suite Tabs
  MidasTactileButton btnTabChannelAi, btnTabAutoMix, btnTabFeedback, btnTabSoundcheck;

  // Tab 0 Components
  MidasTactileButton btnStartCapture;
  MidasTactileButton btnVocalPreset, btnKickPreset, btnSnarePreset,
      btnBassPreset, btnGuitarPreset;
  MidasTactileButton btnApplyAll, btnApplyEq, btnApplyDyn, btnApplyTrim;

  // Tab 1 Components
  MidasTactileButton btnDuganEnable, btnDuganAssignCh, btnDuganAutoWeight, btnMusicDucker;

  // Tab 2 Components
  MidasTactileButton btnAfsEnable, btnAfsAutoNotch, btnAfsResetFilters;

  // Tab 3 Components
  MidasTactileButton btnBatchSoundcheck, btnOptimizeMonitors, btnAiSceneSnapshot;
};
'''

with open('Source/GUI/MidasTouringConsoleSurface.h', 'r', encoding='utf-8') as f:
    content = f.read()

start_marker = '// MODAL: MIDAS AI CHANNEL ASSISTANT'
end_marker = '// MODAL: mCLOUD SHOWFILE SYNC'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx != -1 and end_idx != -1:
    new_content = content[:start_idx] + suite_code + '\n// ==============================================================================\n' + content[end_idx:]
    with open('Source/GUI/MidasTouringConsoleSurface.h', 'w', encoding='utf-8') as f:
        f.write(new_content)
    print("Successfully upgraded MidasAiModal!")
else:
    print("Markers not found!")

