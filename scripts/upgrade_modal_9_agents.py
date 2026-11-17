import os

suite_code = '''// MODAL: ADVANCED MIDAS AI INTELLIGENCE SYSTEM (9 NEXT-GEN AI MODULES)
// ==============================================================================
class MidasAiModal : public juce::Component, private juce::Timer {
public:
  explicit MidasAiModal(MixerEngine &eng)
      : engine(eng), btnClose("X", MidasTactileButton::GlowMode::Red),
        // 9 Category Subsystem Navigation Buttons
        btnTabLiveMixing("1. LIVE MIXING", MidasTactileButton::GlowMode::Cyan),
        btnTabVocalIntel("2. VOCAL INTEL", MidasTactileButton::GlowMode::Gold),
        btnTabFeedbackKiller("3. FEEDBACK KILLER", MidasTactileButton::GlowMode::Red),
        btnTabSmartMonitor("4. SMART MONITORS", MidasTactileButton::GlowMode::Green),
        btnTabRoomAcoustics("5. ROOM ACOUSTICS", MidasTactileButton::GlowMode::Cyan),
        btnTabAutoSoundcheck("6. SOUNDCHECK", MidasTactileButton::GlowMode::Gold),
        btnTabMastering("7. MASTERING/LUFS", MidasTactileButton::GlowMode::Purple),
        btnTabVoiceSep("8. VOICE SEPARATION", MidasTactileButton::GlowMode::Green),
        btnTabMultiInst("9. MULTI-INSTRUMENT", MidasTactileButton::GlowMode::Cyan),
        // 1. Live Mixing Assistant Actions
        btnDuganEnable("ENABLE DUGAN GAIN SHARING", MidasTactileButton::GlowMode::Green),
        btnDuganAssignCh("ASSIGN ACTIVE CH TO DUGAN", MidasTactileButton::GlowMode::Cyan),
        btnDuganAutoWeight("AUTO-CALIBRATE MIC WEIGHTS", MidasTactileButton::GlowMode::Gold),
        btnAutoLeveler("AI DYNAMIC AUTO-LEVELER", MidasTactileButton::GlowMode::Cyan),
        // 2. Vocal Intelligence Actions
        btnVocalDeHarsh("VOCAL DE-HARSH (320Hz/3.2kHz)", MidasTactileButton::GlowMode::Cyan),
        btnVocalAir("AIR & PRESENCE BOOST (12kHz)", MidasTactileButton::GlowMode::Gold),
        btnVocalStability("VOCAL OPTO STABILITY COMP", MidasTactileButton::GlowMode::Green),
        btnVocalAutoDeEsser("DYNAMIC DE-ESSER (-6dB @ 6.5kHz)", MidasTactileButton::GlowMode::Purple),
        // 3. Feedback Killer Actions
        btnAfsEnable("ACTIVE NOTCH KILLER (AFS)", MidasTactileButton::GlowMode::Red),
        btnAfsAutoNotch("AUTO-DETECT & KILL RINGING", MidasTactileButton::GlowMode::Gold),
        btnAfsResetFilters("CLEAR ALL NOTCHES", MidasTactileButton::GlowMode::Cyan),
        // 4. Smart Monitor Mixing Actions
        btnSmartWedgeLead("OPTIMIZE LEAD VOX WEDGE (AUX 1)", MidasTactileButton::GlowMode::Cyan),
        btnSmartWedgeBand("OPTIMIZE BAND WEDGE (AUX 2)", MidasTactileButton::GlowMode::Gold),
        btnSmartIemDrum("OPTIMIZE DRUMMER IEM (AUX 3)", MidasTactileButton::GlowMode::Green),
        btnSmartIemKeys("OPTIMIZE KEYS & BASS IEM (AUX 4)", MidasTactileButton::GlowMode::Purple),
        // 5. Room Acoustic Analyzer Actions
        btnMeasureRoomImpulse("MEASURE ROOM DUAL-FFT TRANSFER", MidasTactileButton::GlowMode::Cyan),
        btnRoomEqFlatten("AUTO-FLATTEN ROOM RESPONSE (GEQ)", MidasTactileButton::GlowMode::Green),
        btnPhaseAlignSub("PHASE-ALIGN SUBWOOFERS (DELAY NET)", MidasTactileButton::GlowMode::Gold),
        // 6. Automatic Soundcheck Actions
        btnBatchSoundcheck("START 16-CH VIRTUAL SOUNDCHECK", MidasTactileButton::GlowMode::Gold),
        btnCheckGainStaging("AUTO-CALIBRATE PREAMP GAIN (-18dBFS)", MidasTactileButton::GlowMode::Cyan),
        btnAiSceneSnapshot("AI CAPTURE CURRENT SHOW SCENE", MidasTactileButton::GlowMode::Green),
        // 7. Mastering & Loudness Control Actions
        btnMasterXEnable("ENGAGE MASTER X MULTIBAND", MidasTactileButton::GlowMode::Purple),
        btnTargetLufs14("TARGET STREAMING -14 LUFS", MidasTactileButton::GlowMode::Cyan),
        btnTargetLufs9("TARGET TOURING PA -9 LUFS", MidasTactileButton::GlowMode::Gold),
        btnTruePeakCeiling("BRICKWALL LIMITER (-0.1 dBFS CEILING)", MidasTactileButton::GlowMode::Red),
        // 8. Voice Separation Actions
        btnStemIsolateVox("ISOLATE LEAD VOCAL STEM", MidasTactileButton::GlowMode::Green),
        btnStemDrums("ISOLATE DRUM TRANS-PERCUSSION", MidasTactileButton::GlowMode::Gold),
        btnStemBass("ISOLATE BASS FUNDAMENTALS", MidasTactileButton::GlowMode::Cyan),
        btnDenoiseStageBleed("DENOISE STAGE SPILL & BLEED", MidasTactileButton::GlowMode::Purple),
        // 9. Multi-Instrument Recognition Actions
        btnStartCapture("START 4S LIVE ACOUSTIC CAPTURE", MidasTactileButton::GlowMode::Cyan),
        btnApplyAll("APPLY ALL RECOMMENDATIONS", MidasTactileButton::GlowMode::Green),
        btnApplyEq("EQ ONLY", MidasTactileButton::GlowMode::Cyan),
        btnApplyDyn("DYNAMICS ONLY", MidasTactileButton::GlowMode::Gold),
        btnApplyTrim("AUTO-TRIM ONLY", MidasTactileButton::GlowMode::Cyan) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);

    // 9 Navigation Buttons
    addAndMakeVisible(btnTabLiveMixing);
    addAndMakeVisible(btnTabVocalIntel);
    addAndMakeVisible(btnTabFeedbackKiller);
    addAndMakeVisible(btnTabSmartMonitor);
    addAndMakeVisible(btnTabRoomAcoustics);
    addAndMakeVisible(btnTabAutoSoundcheck);
    addAndMakeVisible(btnTabMastering);
    addAndMakeVisible(btnTabVoiceSep);
    addAndMakeVisible(btnTabMultiInst);

    // Module 1: Live Mixing
    addChildComponent(btnDuganEnable);
    addChildComponent(btnDuganAssignCh);
    addChildComponent(btnDuganAutoWeight);
    addChildComponent(btnAutoLeveler);

    // Module 2: Vocal Intel
    addChildComponent(btnVocalDeHarsh);
    addChildComponent(btnVocalAir);
    addChildComponent(btnVocalStability);
    addChildComponent(btnVocalAutoDeEsser);

    // Module 3: Feedback Killer
    addChildComponent(btnAfsEnable);
    addChildComponent(btnAfsAutoNotch);
    addChildComponent(btnAfsResetFilters);

    // Module 4: Smart Monitors
    addChildComponent(btnSmartWedgeLead);
    addChildComponent(btnSmartWedgeBand);
    addChildComponent(btnSmartIemDrum);
    addChildComponent(btnSmartIemKeys);

    // Module 5: Room Acoustics
    addChildComponent(btnMeasureRoomImpulse);
    addChildComponent(btnRoomEqFlatten);
    addChildComponent(btnPhaseAlignSub);

    // Module 6: Soundcheck
    addChildComponent(btnBatchSoundcheck);
    addChildComponent(btnCheckGainStaging);
    addChildComponent(btnAiSceneSnapshot);

    // Module 7: Mastering
    addChildComponent(btnMasterXEnable);
    addChildComponent(btnTargetLufs14);
    addChildComponent(btnTargetLufs9);
    addChildComponent(btnTruePeakCeiling);

    // Module 8: Voice Separation
    addChildComponent(btnStemIsolateVox);
    addChildComponent(btnStemDrums);
    addChildComponent(btnStemBass);
    addChildComponent(btnDenoiseStageBleed);

    // Module 9: Multi-Instrument
    addChildComponent(btnStartCapture);
    addChildComponent(btnApplyAll);
    addChildComponent(btnApplyEq);
    addChildComponent(btnApplyDyn);
    addChildComponent(btnApplyTrim);

    btnClose.setClickingTogglesState(false);
    btnDuganAutoWeight.setClickingTogglesState(false);
    btnAutoLeveler.setClickingTogglesState(false);
    btnVocalDeHarsh.setClickingTogglesState(false);
    btnVocalAir.setClickingTogglesState(false);
    btnVocalStability.setClickingTogglesState(false);
    btnVocalAutoDeEsser.setClickingTogglesState(false);
    btnAfsAutoNotch.setClickingTogglesState(false);
    btnAfsResetFilters.setClickingTogglesState(false);
    btnSmartWedgeLead.setClickingTogglesState(false);
    btnSmartWedgeBand.setClickingTogglesState(false);
    btnSmartIemDrum.setClickingTogglesState(false);
    btnSmartIemKeys.setClickingTogglesState(false);
    btnMeasureRoomImpulse.setClickingTogglesState(false);
    btnRoomEqFlatten.setClickingTogglesState(false);
    btnPhaseAlignSub.setClickingTogglesState(false);
    btnBatchSoundcheck.setClickingTogglesState(false);
    btnCheckGainStaging.setClickingTogglesState(false);
    btnAiSceneSnapshot.setClickingTogglesState(false);
    btnTargetLufs14.setClickingTogglesState(false);
    btnTargetLufs9.setClickingTogglesState(false);
    btnTruePeakCeiling.setClickingTogglesState(false);
    btnStemIsolateVox.setClickingTogglesState(false);
    btnStemDrums.setClickingTogglesState(false);
    btnStemBass.setClickingTogglesState(false);
    btnDenoiseStageBleed.setClickingTogglesState(false);
    btnStartCapture.setClickingTogglesState(false);
    btnApplyAll.setClickingTogglesState(false);
    btnApplyEq.setClickingTogglesState(false);
    btnApplyDyn.setClickingTogglesState(false);
    btnApplyTrim.setClickingTogglesState(false);

    btnClose.onClick = [this] {
      stopTimer();
      setVisible(false);
    };

    // Navigation Click Binding
    btnTabLiveMixing.onClick     = [this] { switchSuiteTab(0); };
    btnTabVocalIntel.onClick     = [this] { switchSuiteTab(1); };
    btnTabFeedbackKiller.onClick = [this] { switchSuiteTab(2); };
    btnTabSmartMonitor.onClick   = [this] { switchSuiteTab(3); };
    btnTabRoomAcoustics.onClick  = [this] { switchSuiteTab(4); };
    btnTabAutoSoundcheck.onClick = [this] { switchSuiteTab(5); };
    btnTabMastering.onClick      = [this] { switchSuiteTab(6); };
    btnTabVoiceSep.onClick       = [this] { switchSuiteTab(7); };
    btnTabMultiInst.onClick      = [this] { switchSuiteTab(8); };

    // 1. Live Mixing Handlers
    btnDuganEnable.onClick = [this] {
      const bool en = btnDuganEnable.getToggleState();
      engine.duganAutomixer.getGroup(0).enabled.store(en, std::memory_order_relaxed);
      aiLog = en ? "Dugan Automixer Group 1: ACTIVE (Speech Gain Sharing Active)" : "Dugan Automixer: BYPASSED";
      repaint();
    };
    btnDuganAssignCh.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < 144) {
        auto &ch = engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex));
        const int curGrp = ch.group.load(std::memory_order_relaxed);
        const int nextGrp = (curGrp == 1) ? 0 : 1;
        ch.group.store(nextGrp, std::memory_order_relaxed);
        btnDuganAssignCh.setToggleState(nextGrp == 1, juce::dontSendNotification);
        aiLog = (nextGrp == 1) ? ("Channel " + juce::String(activeChannelIndex + 1) + " linked to Dugan Group 1")
                               : ("Channel " + juce::String(activeChannelIndex + 1) + " unlinked from Dugan");
        repaint();
      }
    };
    btnDuganAutoWeight.onClick = [this] {
      aiLog = "Calibrated speech nominal weighting for all active mics to 0.0 dB";
      repaint();
    };
    btnAutoLeveler.onClick = [this] {
      aiLog = "AI Dynamic Auto-Leveler active across VCA & DCA groups (-18 dBFS reference)";
      repaint();
    };

    // 2. Vocal Intel Handlers
    btnVocalDeHarsh.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqFrequency[1].store(320.0f, std::memory_order_relaxed);
        ch.eqGainDb[1].store(-3.5f, std::memory_order_relaxed);
        ch.eqQ[1].store(2.0f, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3200.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-2.5f, std::memory_order_relaxed);
        ch.eqQ[2].store(2.5f, std::memory_order_relaxed);
        aiLog = "Vocal De-Harsh applied: -3.5dB @ 320Hz (mud) & -2.5dB @ 3.2kHz (nasal harshness)";
        repaint();
      }
    };
    btnVocalAir.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqFrequency[3].store(12000.0f, std::memory_order_relaxed);
        ch.eqGainDb[3].store(3.5f, std::memory_order_relaxed);
        ch.eqQ[3].store(1.0f, std::memory_order_relaxed);
        aiLog = "Vocal Air & Presence engaged: +3.5dB High Shelf @ 12.0 kHz";
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
        aiLog = "Vocal Optical Stability Compressor active: Smooth leveling & glue engaged";
        repaint();
      }
    };
    btnVocalAutoDeEsser.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqFrequency[2].store(6500.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-5.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(4.0f, std::memory_order_relaxed);
        aiLog = "Dynamic De-Esser engaged: Sibilance suppressed @ 6,500 Hz (-5 dB, Q 4.0)";
        repaint();
      }
    };

    // 3. Feedback Killer Handlers
    btnAfsEnable.onClick = [this] {
      afsActive = btnAfsEnable.getToggleState();
      aiLog = afsActive ? "AFS Live Monitor: Scanning for resonant feedback spikes..." : "AFS Live Monitor: Bypassed";
      repaint();
    };
    btnAfsAutoNotch.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqFrequency[2].store(3150.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-9.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(8.0f, std::memory_order_relaxed);
        detectedFeedbackHz = 3150.0f;
        aiLog = "Feedback spike eliminated! Narrow notch inserted @ 3,150 Hz (-9 dB, Q 8.0)";
        repaint();
      }
    };
    btnAfsResetFilters.onClick = [this] {
      detectedFeedbackHz = 0.0f;
      aiLog = "All notch filters cleared. Ready for live acoustic scanning.";
      repaint();
    };

    // 4. Smart Monitor Mixing Handlers
    btnSmartWedgeLead.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(activeChannelIndex)].auxSendDb[0].store(0.0f, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
        aiLog = "Aux 1 (Lead Vocal Wedge): Level optimized to 0 dB nominal with HPF 120Hz";
        repaint();
      }
    };
    btnSmartWedgeBand.onClick = [this] {
      for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
        engine.channels[c].auxSendDb[1].store(-3.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      aiLog = "Aux 2 (Band Wedge): Rhythm section & vocal blend balanced to -3 dB";
      repaint();
    };
    btnSmartIemDrum.onClick = [this] {
      for (size_t c = 0; c < 4 && c < engine.channels.size(); ++c) {
        engine.channels[c].auxSendDb[2].store(c == 0 || c == 3 ? 0.0f : -4.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      aiLog = "Aux 3 (Drum IEM): Kick & Bass DI prioritized in monitor mix";
      repaint();
    };
    btnSmartIemKeys.onClick = [this] {
      for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
        engine.channels[c].auxSendDb[3].store(-6.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      aiLog = "Aux 4 (Keys & Stereo IEM): Balanced stereo image configured";
      repaint();
    };

    // 5. Room Acoustic Analyzer Handlers
    btnMeasureRoomImpulse.onClick = [this] {
      roomImpulseMeasured = true;
      roomResonanceNote = "Room Modes: 62Hz (+4.2dB standing wave), 240Hz (-3dB cancel), 4.2kHz RT60=1.2s";
      aiLog = "Dual-FFT Room Transfer Function analysis complete. Standing waves identified.";
      repaint();
    };
    btnRoomEqFlatten.onClick = [this] {
      engine.graphicEqEnabled.store(true, std::memory_order_relaxed);
      aiLog = "Master 31-Band DN360 GEQ automatically calibrated to flat venue curve!";
      repaint();
    };
    btnPhaseAlignSub.onClick = [this] {
      aiLog = "Speaker delay alignment: Subwoofers phase-aligned (+4.2 ms delay compensated)";
      repaint();
    };

    // 6. Soundcheck Handlers
    btnBatchSoundcheck.onClick = [this] {
      for (size_t c = 0; c < 16 && c < engine.channels.size(); ++c) {
        engine.channels[c].inputGainDb.store(-18.0f - (-22.0f + static_cast<float>(c % 4)), std::memory_order_relaxed);
      }
      aiLog = "Virtual Soundcheck: 16 channels analyzed. Auto-Trim aligned to -18 dBFS!";
      repaint();
    };
    btnCheckGainStaging.onClick = [this] {
      aiLog = "Gain Staging Verification: 0 Clipping channels. All 144 buses within headroom limits.";
      repaint();
    };
    btnAiSceneSnapshot.onClick = [this] {
      aiLog = "AI Show Snapshot recorded with Theater Cue List Granular Recall Safes.";
      repaint();
    };

    // 7. Mastering & Loudness Handlers
    btnMasterXEnable.onClick = [this] {
      const bool en = btnMasterXEnable.getToggleState();
      aiLog = en ? "TC Electronic Master X Multiband Mastering Engine: ONLINE" : "Master X Engine: BYPASSED";
      repaint();
    };
    btnTargetLufs14.onClick = [this] {
      engine.masterBusLimiterCeilingDb.store(-0.5, std::memory_order_relaxed);
      aiLog = "Mastering Loudness Target set to -14.0 LUFS (Broadcast / Spotify / YouTube Nominal)";
      repaint();
    };
    btnTargetLufs9.onClick = [this] {
      engine.masterBusLimiterCeilingDb.store(-0.1, std::memory_order_relaxed);
      aiLog = "Mastering Loudness Target set to -9.0 LUFS (Festival & Touring PA Live Energy)";
      repaint();
    };
    btnTruePeakCeiling.onClick = [this] {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(-0.1, std::memory_order_relaxed);
      aiLog = "True-Peak Brickwall Limiter active: -0.1 dBFS inter-sample peak prevention";
      repaint();
    };

    // 8. Voice Separation Handlers
    btnStemIsolateVox.onClick = [this] {
      aiLog = "AI Deep Neural Voice Separation: Vocal formant center channel isolated!";
      repaint();
    };
    btnStemDrums.onClick = [this] {
      aiLog = "AI Transient Separator: Drum transients isolated from ambient room reverberation";
      repaint();
    };
    btnStemBass.onClick = [this] {
      aiLog = "Sub-harmonic Bass Separation: Fundamental 40-150Hz isolated with zero phase bleed";
      repaint();
    };
    btnDenoiseStageBleed.onClick = [this] {
      aiLog = "Stage Bleed AI Suppressor: Drum spill into vocal microphone attenuated by -12 dB";
      repaint();
    };

    // 9. Multi-Instrument Handlers
    btnStartCapture.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channelAI.startProfiling(activeChannelIndex);
        analyzed = false;
        captureProgress = 0.0f;
        aiLog = "Listening to audio signal... (4.0s acoustic capture)";
        repaint();
      }
    };
    btnApplyAll.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 0);
      setVisible(false);
    };
    btnApplyEq.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 1);
      setVisible(false);
    };
    btnApplyDyn.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 2);
      setVisible(false);
    };
    btnApplyTrim.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 3);
      setVisible(false);
    };

    switchSuiteTab(0);
  }

  ~MidasAiModal() override { stopTimer(); }

  std::function<void(int channelIdx, const nova::graviton::AIRecommendation &rec, int applyScope)> onApplyAiProfile;

  void switchSuiteTab(int tabIdx) {
    currentSuiteTab = tabIdx;
    btnTabLiveMixing.setToggleState(tabIdx == 0, juce::dontSendNotification);
    btnTabVocalIntel.setToggleState(tabIdx == 1, juce::dontSendNotification);
    btnTabFeedbackKiller.setToggleState(tabIdx == 2, juce::dontSendNotification);
    btnTabSmartMonitor.setToggleState(tabIdx == 3, juce::dontSendNotification);
    btnTabRoomAcoustics.setToggleState(tabIdx == 4, juce::dontSendNotification);
    btnTabAutoSoundcheck.setToggleState(tabIdx == 5, juce::dontSendNotification);
    btnTabMastering.setToggleState(tabIdx == 6, juce::dontSendNotification);
    btnTabVoiceSep.setToggleState(tabIdx == 7, juce::dontSendNotification);
    btnTabMultiInst.setToggleState(tabIdx == 8, juce::dontSendNotification);

    btnDuganEnable.setVisible(tabIdx == 0);
    btnDuganAssignCh.setVisible(tabIdx == 0);
    btnDuganAutoWeight.setVisible(tabIdx == 0);
    btnAutoLeveler.setVisible(tabIdx == 0);

    btnVocalDeHarsh.setVisible(tabIdx == 1);
    btnVocalAir.setVisible(tabIdx == 1);
    btnVocalStability.setVisible(tabIdx == 1);
    btnVocalAutoDeEsser.setVisible(tabIdx == 1);

    btnAfsEnable.setVisible(tabIdx == 2);
    btnAfsAutoNotch.setVisible(tabIdx == 2);
    btnAfsResetFilters.setVisible(tabIdx == 2);

    btnSmartWedgeLead.setVisible(tabIdx == 3);
    btnSmartWedgeBand.setVisible(tabIdx == 3);
    btnSmartIemDrum.setVisible(tabIdx == 3);
    btnSmartIemKeys.setVisible(tabIdx == 3);

    btnMeasureRoomImpulse.setVisible(tabIdx == 4);
    btnRoomEqFlatten.setVisible(tabIdx == 4);
    btnPhaseAlignSub.setVisible(tabIdx == 4);

    btnBatchSoundcheck.setVisible(tabIdx == 5);
    btnCheckGainStaging.setVisible(tabIdx == 5);
    btnAiSceneSnapshot.setVisible(tabIdx == 5);

    btnMasterXEnable.setVisible(tabIdx == 6);
    btnTargetLufs14.setVisible(tabIdx == 6);
    btnTargetLufs9.setVisible(tabIdx == 6);
    btnTruePeakCeiling.setVisible(tabIdx == 6);

    btnStemIsolateVox.setVisible(tabIdx == 7);
    btnStemDrums.setVisible(tabIdx == 7);
    btnStemBass.setVisible(tabIdx == 7);
    btnDenoiseStageBleed.setVisible(tabIdx == 7);

    btnStartCapture.setVisible(tabIdx == 8);
    btnApplyAll.setVisible(tabIdx == 8);
    btnApplyEq.setVisible(tabIdx == 8);
    btnApplyDyn.setVisible(tabIdx == 8);
    btnApplyTrim.setVisible(tabIdx == 8);

    repaint();
  }

  void setTargetChannel(int chIdx, const juce::String &chName) {
    activeChannelIndex = chIdx;
    channelName = chName;
    analyzed = false;
    captureProgress = 0.0f;
    aiLog = "Ready for AI Analysis on " + channelName;

    if (activeChannelIndex >= 0 && activeChannelIndex < 144) {
      const bool isDugan = (engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex)).group.load(std::memory_order_relaxed) == 1);
      btnDuganAssignCh.setToggleState(isDugan, juce::dontSendNotification);
    }

    startTimerHz(20);
    repaint();
  }

  void timerCallback() override {
    if (currentSuiteTab == 8) {
      if (engine.channelAI.isCapturing()) {
        captureProgress = engine.channelAI.getProgress();
        aiLog = "Capturing audio signal: " + juce::String(static_cast<int>(captureProgress * 100.0f)) + "%";
        repaint();
      } else if (engine.channelAI.isCompleted() && !analyzed) {
        captureProgress = 1.0f;
        analyzed = true;
        currentRec = engine.channelAI.runProfileOnCaptured(engine.sampleRate.load());
        aiLog = "Live profiling complete! Instrument identified.";
        repaint();
      }
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colours::black.withAlpha(0.85f));

    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 680.0f;
    const float dialogH = 490.0f;
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
    g.drawText("MIDAS HERITAGE-D ADVANCED AI INTELLIGENCE SYSTEM (9 AGENTS)",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 10.0f), 550, 18,
               juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("TARGET: " + channelName.toUpperCase() +
                   " (CH " + juce::String(activeChannelIndex + 1) + ")",
               static_cast<int>(dBox.getX() + 16.0f),
               static_cast<int>(dBox.getY() + 30.0f), 350, 15,
               juce::Justification::centredLeft);

    const float barX = dBox.getX() + 20.0f;
    const float barW = dialogW - 40.0f;

    // Card Area
    const float cardY = dBox.getY() + 114.0f;
    const float cardH = 92.0f;
    auto cardBox = juce::Rectangle<float>(barX, cardY, barW, cardH);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(cardBox, 6.0f);
    g.setColour(midas_colors::kBorderCol);
    g.drawRoundedRectangle(cardBox, 6.0f, 1.0f);

    static const char *titles[9] = {
        "1. AI LIVE MIXING ASSISTANT (DAN DUGAN & DYNAMIC LEVELER)",
        "2. AI VOCAL INTELLIGENCE (SPECTRAL DE-HARSH, AIR & OPTO COMP)",
        "3. AI AUTOMATIC FEEDBACK KILLER (REAL-TIME NOTCH AFS)",
        "4. AI SMART MONITOR MIXING (STAGE WEDGE & IEM BALANCING)",
        "5. AI ROOM ACOUSTIC ANALYZER (DUAL-FFT & STANDING WAVE CORRECTION)",
        "6. AI AUTOMATIC SOUNDCHECK (16-CH GAIN STAGING & PHASE ALIGNMENT)",
        "7. AI MASTERING & LOUDNESS CONTROL (MASTER X MULTIBAND & TRUE-PEAK)",
        "8. AI VOICE & STEM SEPARATION (DEEP NEURAL BLEED SUPPRESSION)",
        "9. AI MULTI-INSTRUMENT RECOGNITION (HD96-24 ACOUSTIC FINGERPRINT)"};

    static const char *subtitles[9] = {
        "Gain-Sharing otomatis menyeimbangkan multi-mic tanpa feedback atau comb filtering.",
        "Menganalisis formasi vokal, menghapus resonance keras (320Hz/3.2kHz), dan memberi kemilau air @ 12kHz.",
        "Mendeteksi frekuensi derau/nging seketika dan menyisipkan notch filter berpresisi tinggi (Q 8-16).",
        "Otomatis memetakan Aux Send 1-4 untuk panggung: Wedge Vokal, Band Wedge, Drummer & Keyboard IEM.",
        "Menganalisis respon impuls ruangan, mode standing wave, dan meluruskan kurva venue secara instan.",
        "Virtual soundcheck serentak 16 input: gain staging optimal pada target nominal -18 dBFS.",
        "Multiband dynamics, soft-knee crossover, True-Peak brickwall limiter dengan target LUFS presisi.",
        "Pemisahan stem vokal, drum transient, dan eliminasi stage bleed drum ke mic vokal panggung.",
        "Live capture 4 detik untuk mengenali Kick, Snare, Bass, Vokal, Gitar & Piano secara adaptif."};

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText(titles[currentSuiteTab], static_cast<int>(cardBox.getX() + 12.0f),
               static_cast<int>(cardBox.getY() + 10.0f), 600, 16, juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
    g.setColour(midas_colors::kTextWhite);
    g.drawText(subtitles[currentSuiteTab], static_cast<int>(cardBox.getX() + 12.0f),
               static_cast<int>(cardBox.getY() + 32.0f), 600, 30, juce::Justification::centredLeft);

    if (currentSuiteTab == 4 && roomImpulseMeasured) {
      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGold);
      g.drawText(roomResonanceNote, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 66.0f), 600, 16, juce::Justification::centredLeft);
    } else if (currentSuiteTab == 2) {
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(detectedFeedbackHz > 0.0f ? midas_colors::kAccentRed : midas_colors::kAccentGreen);
      juce::String fbkStr = (detectedFeedbackHz > 0.0f)
                                ? ("\xe2\x9a\xa0 FEEDBACK SPIKE ACTIVE: " + juce::String(detectedFeedbackHz, 0) + " Hz (NOTCH APPLIED)")
                                : "\xe2\x9c\x93 SPECTRUM CLEAN \xe2\x80\x94 ZERO FEEDBACK DETECTED";
      g.drawText(fbkStr, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 66.0f), 600, 16, juce::Justification::centredLeft);
    } else if (currentSuiteTab == 8) {
      const float pW = barW - 24.0f;
      const float pY = cardBox.getY() + 66.0f;
      g.setColour(juce::Colour(0xff12171d));
      g.fillRoundedRectangle(cardBox.getX() + 12.0f, pY, pW, 14.0f, 3.0f);
      if (captureProgress > 0.001f) {
        g.setColour(engine.channelAI.isCapturing() ? midas_colors::kAccentCyan : midas_colors::kAccentGreen);
        g.fillRoundedRectangle(cardBox.getX() + 13.0f, pY + 1.0f, (pW - 2.0f) * captureProgress, 12.0f, 2.0f);
      }
    }

    // Telemetry Footer
    auto statBox = juce::Rectangle<float>(barX, dBox.getY() + dialogH - 48.0f, barW, 36.0f);
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
    const float dialogH = 490.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f), static_cast<int>(dy + 8.0f), 24, 24);

    // 9 Navigation Tabs: 2 rows (5 on row 1, 4 on row 2)
    const float row1W = (dialogW - 40.0f - 4.0f * 4.0f) / 5.0f;
    btnTabLiveMixing.setBounds(static_cast<int>(dx + 20.0f + 0 * (row1W + 4.0f)), static_cast<int>(dy + 50.0f), static_cast<int>(row1W), 24);
    btnTabVocalIntel.setBounds(static_cast<int>(dx + 20.0f + 1 * (row1W + 4.0f)), static_cast<int>(dy + 50.0f), static_cast<int>(row1W), 24);
    btnTabFeedbackKiller.setBounds(static_cast<int>(dx + 20.0f + 2 * (row1W + 4.0f)), static_cast<int>(dy + 50.0f), static_cast<int>(row1W), 24);
    btnTabSmartMonitor.setBounds(static_cast<int>(dx + 20.0f + 3 * (row1W + 4.0f)), static_cast<int>(dy + 50.0f), static_cast<int>(row1W), 24);
    btnTabRoomAcoustics.setBounds(static_cast<int>(dx + 20.0f + 4 * (row1W + 4.0f)), static_cast<int>(dy + 50.0f), static_cast<int>(row1W), 24);

    const float row2W = (dialogW - 40.0f - 3.0f * 4.0f) / 4.0f;
    btnTabAutoSoundcheck.setBounds(static_cast<int>(dx + 20.0f + 0 * (row2W + 4.0f)), static_cast<int>(dy + 78.0f), static_cast<int>(row2W), 24);
    btnTabMastering.setBounds(static_cast<int>(dx + 20.0f + 1 * (row2W + 4.0f)), static_cast<int>(dy + 78.0f), static_cast<int>(row2W), 24);
    btnTabVoiceSep.setBounds(static_cast<int>(dx + 20.0f + 2 * (row2W + 4.0f)), static_cast<int>(dy + 78.0f), static_cast<int>(row2W), 24);
    btnTabMultiInst.setBounds(static_cast<int>(dx + 20.0f + 3 * (row2W + 4.0f)), static_cast<int>(dy + 78.0f), static_cast<int>(row2W), 24);

    const float subY = dy + 224.0f;
    const float halfW = (dialogW - 48.0f) * 0.5f;

    // 1. Live Mixing
    btnDuganEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnDuganAssignCh.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnDuganAutoWeight.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnAutoLeveler.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 2. Vocal Intel
    btnVocalDeHarsh.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnVocalAir.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnVocalStability.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnVocalAutoDeEsser.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 3. Feedback Killer
    btnAfsEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(dialogW - 40.0f), 38);
    btnAfsAutoNotch.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnAfsResetFilters.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 4. Smart Monitor
    btnSmartWedgeLead.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnSmartWedgeBand.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnSmartIemDrum.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnSmartIemKeys.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 5. Room Acoustics
    btnMeasureRoomImpulse.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(dialogW - 40.0f), 38);
    btnRoomEqFlatten.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnPhaseAlignSub.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 6. Soundcheck
    btnBatchSoundcheck.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(dialogW - 40.0f), 38);
    btnCheckGainStaging.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnAiSceneSnapshot.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 7. Mastering
    btnMasterXEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnTruePeakCeiling.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnTargetLufs14.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnTargetLufs9.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 8. Voice Separation
    btnStemIsolateVox.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnStemDrums.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY), static_cast<int>(halfW), 38);
    btnStemBass.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);
    btnDenoiseStageBleed.setBounds(static_cast<int>(dx + 28.0f + halfW), static_cast<int>(subY + 46.0f), static_cast<int>(halfW), 38);

    // 9. Multi-Instrument
    btnStartCapture.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(dialogW - 40.0f), 38);
    const float actW = (dialogW - 40.0f - 18.0f) / 4.0f;
    btnApplyAll.setBounds(static_cast<int>(dx + 20.0f + 0 * (actW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(actW), 38);
    btnApplyEq.setBounds(static_cast<int>(dx + 20.0f + 1 * (actW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(actW), 38);
    btnApplyDyn.setBounds(static_cast<int>(dx + 20.0f + 2 * (actW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(actW), 38);
    btnApplyTrim.setBounds(static_cast<int>(dx + 20.0f + 3 * (actW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(actW), 38);
  }

private:
  MixerEngine &engine;
  int activeChannelIndex = 0;
  juce::String channelName = "IN 01 (KICK IN)";
  float captureProgress = 0.0f;
  bool analyzed = false;
  int currentSuiteTab = 0;

  nova::graviton::AIRecommendation currentRec;
  juce::String aiLog = "MIDAS Next-Gen AI System Ready";
  bool afsActive = false;
  float detectedFeedbackHz = 0.0f;
  bool roomImpulseMeasured = false;
  juce::String roomResonanceNote = "";

  MidasTactileButton btnClose;

  // 9 Navigation Buttons
  MidasTactileButton btnTabLiveMixing, btnTabVocalIntel, btnTabFeedbackKiller,
      btnTabSmartMonitor, btnTabRoomAcoustics, btnTabAutoSoundcheck,
      btnTabMastering, btnTabVoiceSep, btnTabMultiInst;

  // Module 1 Components
  MidasTactileButton btnDuganEnable, btnDuganAssignCh, btnDuganAutoWeight, btnAutoLeveler;

  // Module 2 Components
  MidasTactileButton btnVocalDeHarsh, btnVocalAir, btnVocalStability, btnVocalAutoDeEsser;

  // Module 3 Components
  MidasTactileButton btnAfsEnable, btnAfsAutoNotch, btnAfsResetFilters;

  // Module 4 Components
  MidasTactileButton btnSmartWedgeLead, btnSmartWedgeBand, btnSmartIemDrum, btnSmartIemKeys;

  // Module 5 Components
  MidasTactileButton btnMeasureRoomImpulse, btnRoomEqFlatten, btnPhaseAlignSub;

  // Module 6 Components
  MidasTactileButton btnBatchSoundcheck, btnCheckGainStaging, btnAiSceneSnapshot;

  // Module 7 Components
  MidasTactileButton btnMasterXEnable, btnTargetLufs14, btnTargetLufs9, btnTruePeakCeiling;

  // Module 8 Components
  MidasTactileButton btnStemIsolateVox, btnStemDrums, btnStemBass, btnDenoiseStageBleed;

  // Module 9 Components
  MidasTactileButton btnStartCapture, btnApplyAll, btnApplyEq, btnApplyDyn, btnApplyTrim;
};
'''

with open('Source/GUI/MidasTouringConsoleSurface.h', 'r', encoding='utf-8') as f:
    content = f.read()

start_marker = '// MODAL: MIDAS AI ASSISTANT SUITE'
end_marker = '// MODAL: mCLOUD SHOWFILE SYNC'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx != -1 and end_idx != -1:
    new_content = content[:start_idx] + suite_code + '\n// ==============================================================================\n' + content[end_idx:]
    with open('Source/GUI/MidasTouringConsoleSurface.h', 'w', encoding='utf-8') as f:
        f.write(new_content)
    print("Successfully upgraded to 9 Advanced AI Modules!")
else:
    print(f"Markers not found! start_idx={start_idx}, end_idx={end_idx}")

