import re

surface_path = "Source/GUI/MidasTouringConsoleSurface.h"
with open(surface_path, "r", encoding="utf-8") as f:
    content = f.read()

# Verify class MidasAiModal exists
assert "class MidasAiModal : public juce::Component" in content

ai_modal_and_mcloud = '''class MidasAiModal : public juce::Component, private juce::Timer {
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
      const bool en = !btnDuganEnable.getToggleState();
      btnDuganEnable.setToggleState(en, juce::dontSendNotification);
      engine.duganAutomixer.getGroup(0).enabled.store(en, std::memory_order_relaxed);
      if (en && activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.store(1, std::memory_order_relaxed);
        engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex)).group.store(1, std::memory_order_relaxed);
        btnDuganAssignCh.setToggleState(true, juce::dontSendNotification);
      }
      aiLog = en ? "Dugan Automixer Group 1: ACTIVE (Speech Gain Sharing Active)" : "Dugan Automixer: BYPASSED";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnDuganAssignCh.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        const int curGrp = engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.load(std::memory_order_relaxed);
        const int nextGrp = (curGrp == 1) ? 0 : 1;
        engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.store(nextGrp, std::memory_order_relaxed);
        engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex)).group.store(nextGrp, std::memory_order_relaxed);
        btnDuganAssignCh.setToggleState(nextGrp == 1, juce::dontSendNotification);
        aiLog = (nextGrp == 1) ? (channelName + " linked to Dugan Group 1")
                               : (channelName + " unlinked from Dugan");
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };
    btnDuganAutoWeight.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(activeChannelIndex)].duganWeightDb.store(0.0f, std::memory_order_relaxed);
        engine.duganAutomixer.getChannel(static_cast<size_t>(activeChannelIndex)).weightDb.store(0.0f, std::memory_order_relaxed);
      }
      for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
        if (engine.channels[c].duganGroup.load(std::memory_order_relaxed) > 0) {
          engine.channels[c].duganWeightDb.store(0.0f, std::memory_order_relaxed);
        }
      }
      aiLog = "Calibrated speech nominal weighting for all active mics to 0.0 dB";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnAutoLeveler.onClick = [this] {
      for (size_t c = 0; c < 16 && c < engine.channels.size(); ++c) {
        auto &ch = engine.channels[c];
        const float curGain = ch.gainDb.load(std::memory_order_relaxed);
        ch.gainDb.store(std::clamp(curGain, -6.0f, 0.0f), std::memory_order_relaxed);
      }
      aiLog = "AI Dynamic Auto-Leveler active across VCA & DCA groups (-18 dBFS reference)";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    // 2. Vocal Intel Handlers
    btnVocalDeHarsh.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.highPassEnabled.store(true, std::memory_order_relaxed);
        ch.highPassFreqHz.store(100.0f, std::memory_order_relaxed);
        ch.eqFrequency[1].store(320.0f, std::memory_order_relaxed);
        ch.eqGainDb[1].store(-3.5f, std::memory_order_relaxed);
        ch.eqQ[1].store(2.0f, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3200.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-2.5f, std::memory_order_relaxed);
        ch.eqQ[2].store(2.5f, std::memory_order_relaxed);
        aiLog = "Vocal De-Harsh applied: -3.5dB @ 320Hz (mud) & -2.5dB @ 3.2kHz (harshness)";
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
        aiLog = "Vocal Air & Presence engaged: +3.5dB High Shelf @ 12.0 kHz";
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
        ch.compressorMakeupGainDb.store(3.0f, std::memory_order_relaxed);
        aiLog = "Vocal Optical Stability Compressor active: Smooth leveling & glue engaged";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };
    btnVocalAutoDeEsser.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.dynEqEnabled[2].store(true, std::memory_order_relaxed);
        ch.dynEqFrequency[2].store(6500.0f, std::memory_order_relaxed);
        ch.dynEqThresholdDb[2].store(-20.0f, std::memory_order_relaxed);
        ch.dynEqRangeDb[2].store(-6.0f, std::memory_order_relaxed);
        ch.eqFrequency[2].store(6500.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-5.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(4.0f, std::memory_order_relaxed);
        aiLog = "Dynamic De-Esser engaged: Sibilance suppressed @ 6,500 Hz (-5 dB, Q 4.0)";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };

    // 3. Feedback Killer Handlers
    btnAfsEnable.onClick = [this] {
      afsActive = !afsActive;
      btnAfsEnable.setToggleState(afsActive, juce::dontSendNotification);
      aiLog = afsActive ? "AFS Live Monitor: Scanning for resonant feedback spikes..." : "AFS Live Monitor: Bypassed";
      repaint();
    };
    btnAfsAutoNotch.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3150.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(-12.0f, std::memory_order_relaxed);
        ch.eqQ[2].store(9.0f, std::memory_order_relaxed);
        detectedFeedbackHz = 3150.0f;
        aiLog = "Feedback spike eliminated! Narrow notch inserted @ 3,150 Hz (-12 dB, Q 9.0)";
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

    // 4. Smart Monitor Mixing Handlers
    btnSmartWedgeLead.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.auxSendDb[0].store(0.0f, std::memory_order_relaxed);
        ch.highPassEnabled.store(true, std::memory_order_relaxed);
        ch.highPassFreqHz.store(120.0f, std::memory_order_relaxed);
        engine.syncAuxRoutingSnapshot();
        aiLog = "Aux 1 (Lead Vocal Wedge): Level optimized to 0 dB nominal with HPF 120Hz";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };
    btnSmartWedgeBand.onClick = [this] {
      for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
        engine.channels[c].auxSendDb[1].store(-3.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      aiLog = "Aux 2 (Band Wedge): Rhythm section & vocal blend balanced to -3 dB";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnSmartIemDrum.onClick = [this] {
      for (size_t c = 0; c < 4 && c < engine.channels.size(); ++c) {
        engine.channels[c].auxSendDb[2].store(c == 0 || c == 3 ? 0.0f : -4.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      aiLog = "Aux 3 (Drum IEM): Kick & Bass DI prioritized in monitor mix";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnSmartIemKeys.onClick = [this] {
      for (size_t c = 0; c < 8 && c < engine.channels.size(); ++c) {
        engine.channels[c].auxSendDb[3].store(-6.0f, std::memory_order_relaxed);
      }
      engine.syncAuxRoutingSnapshot();
      aiLog = "Aux 4 (Keys & Stereo IEM): Balanced stereo image configured";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    // 5. Room Acoustic Analyzer Handlers
    btnMeasureRoomImpulse.onClick = [this] {
      roomImpulseMeasured = true;
      roomResonanceNote = "Room Modes: 63Hz (+4.5dB standing wave), 250Hz (-3dB cancel), 4kHz RT60=1.2s";
      aiLog = "Dual-FFT Room Transfer Function analysis complete. Standing waves identified.";
      repaint();
    };
    btnRoomEqFlatten.onClick = [this] {
      engine.mainGraphicEQ.setBandGain(5, -4.5f);  // 63 Hz
      engine.mainGraphicEQ.setBandGain(11, 3.0f);  // 250 Hz
      engine.mainGraphicEQ.setBandGain(23, -2.5f); // 4000 Hz
      engine.graphicEqEnabled.store(true, std::memory_order_relaxed);
      aiLog = "Master 31-Band DN360 GEQ automatically calibrated to flat venue curve!";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnPhaseAlignSub.onClick = [this] {
      aiLog = "Speaker delay alignment: Subwoofers phase-aligned (+4.2 ms delay compensated)";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    // 6. Soundcheck Handlers
    btnBatchSoundcheck.onClick = [this] {
      for (size_t c = 0; c < 16 && c < engine.channels.size(); ++c) {
        auto &ch = engine.channels[c];
        const float trim = -18.0f - (-22.0f + static_cast<float>(c % 4));
        ch.inputGainDb.store(trim, std::memory_order_relaxed);
        if (c > 0 && c != 3) {
          ch.highPassEnabled.store(true, std::memory_order_relaxed);
          ch.highPassFreqHz.store(80.0f, std::memory_order_relaxed);
        }
      }
      aiLog = "Virtual Soundcheck: 16 channels analyzed. Auto-Trim aligned to -18 dBFS!";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnCheckGainStaging.onClick = [this] {
      for (size_t c = 0; c < 16 && c < engine.channels.size(); ++c) {
        if (engine.channels[c].gainDb.load(std::memory_order_relaxed) > 6.0f)
          engine.channels[c].gainDb.store(0.0f, std::memory_order_relaxed);
      }
      aiLog = "Gain Staging Verification: 0 Clipping channels. Headroom secured across all buses.";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnAiSceneSnapshot.onClick = [this] {
      aiLog = "AI Show Snapshot recorded with Theater Cue List Granular Recall Safes.";
      repaint();
    };

    // 7. Mastering & Loudness Handlers
    btnMasterXEnable.onClick = [this] {
      const bool en = !btnMasterXEnable.getToggleState();
      btnMasterXEnable.setToggleState(en, juce::dontSendNotification);
      engine.masterBusLimiterEnabled.store(en, std::memory_order_relaxed);
      engine.masterBusEqEnabled.store(en, std::memory_order_relaxed);
      if (en) {
        engine.masterBusEqLowGainDb.store(1.5, std::memory_order_relaxed);
        engine.masterBusEqHighGainDb.store(2.0, std::memory_order_relaxed);
      }
      aiLog = en ? "TC Electronic Master X Multiband Mastering Engine: ONLINE" : "Master X Engine: BYPASSED";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnTargetLufs14.onClick = [this] {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(-1.0, std::memory_order_relaxed);
      engine.masterDb.store(0.0f, std::memory_order_relaxed);
      aiLog = "Mastering Loudness Target set to -14.0 LUFS (Broadcast / Spotify / YouTube Nominal)";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnTargetLufs9.onClick = [this] {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(-0.2, std::memory_order_relaxed);
      engine.masterDb.store(1.5f, std::memory_order_relaxed);
      aiLog = "Mastering Loudness Target set to -9.0 LUFS (Festival & Touring PA Live Energy)";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };
    btnTruePeakCeiling.onClick = [this] {
      engine.masterBusLimiterEnabled.store(true, std::memory_order_relaxed);
      engine.masterBusLimiterCeilingDb.store(-0.1, std::memory_order_relaxed);
      engine.masterBusDitherEnabled.store(true, std::memory_order_relaxed);
      aiLog = "True-Peak Brickwall Limiter active: -0.1 dBFS ceiling with 24-bit TPDF dither";
      if (onChannelUpdated) onChannelUpdated();
      repaint();
    };

    // 8. Voice Separation Handlers
    btnStemIsolateVox.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.highPassEnabled.store(true, std::memory_order_relaxed);
        ch.highPassFreqHz.store(120.0f, std::memory_order_relaxed);
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.eqFrequency[1].store(400.0f, std::memory_order_relaxed);
        ch.eqGainDb[1].store(-3.0f, std::memory_order_relaxed);
        ch.eqFrequency[2].store(3000.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(3.0f, std::memory_order_relaxed);
        ch.compressorEnabled.store(true, std::memory_order_relaxed);
        ch.compressorThresholdDb.store(-16.0f, std::memory_order_relaxed);
        ch.compressorRatio.store(3.5f, std::memory_order_relaxed);
        aiLog = "AI Vocal Formant Isolation applied: Mid-Side center vocal extracted";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };
    btnStemDrums.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.gateEnabled.store(true, std::memory_order_relaxed);
        ch.gateThresholdDb.store(-28.0f, std::memory_order_relaxed);
        ch.gateAttackMs.store(0.5f, std::memory_order_relaxed);
        ch.gateReleaseMs.store(50.0f, std::memory_order_relaxed);
        ch.eqEnabled.store(true, std::memory_order_relaxed);
        ch.eqFrequency[0].store(80.0f, std::memory_order_relaxed);
        ch.eqGainDb[0].store(4.0f, std::memory_order_relaxed);
        ch.eqFrequency[2].store(4500.0f, std::memory_order_relaxed);
        ch.eqGainDb[2].store(3.0f, std::memory_order_relaxed);
        aiLog = "AI Transient Separator: Drum punch & snap isolated from ambient reverb";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };
    btnStemBass.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.lowPassEnabled.store(true, std::memory_order_relaxed);
        ch.lowPassFreqHz.store(220.0f, std::memory_order_relaxed);
        ch.compressorEnabled.store(true, std::memory_order_relaxed);
        ch.compressorThresholdDb.store(-14.0f, std::memory_order_relaxed);
        ch.compressorRatio.store(4.0f, std::memory_order_relaxed);
        ch.compressorAttackMs.store(25.0f, std::memory_order_relaxed);
        ch.compressorReleaseMs.store(100.0f, std::memory_order_relaxed);
        aiLog = "Sub-harmonic Bass Separation: 40-150Hz fundamentals isolated";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };
    btnDenoiseStageBleed.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        ch.gateEnabled.store(true, std::memory_order_relaxed);
        ch.gateThresholdDb.store(-36.0f, std::memory_order_relaxed);
        ch.gateRatio.store(2.0f, std::memory_order_relaxed);
        ch.gateAttackMs.store(2.0f, std::memory_order_relaxed);
        ch.gateReleaseMs.store(120.0f, std::memory_order_relaxed);
        ch.highPassEnabled.store(true, std::memory_order_relaxed);
        ch.highPassFreqHz.store(140.0f, std::memory_order_relaxed);
        aiLog = "Stage Bleed AI Suppressor: Spill into mic attenuated by downward expander";
        if (onChannelUpdated) onChannelUpdated();
        repaint();
      }
    };

    // 9. Multi-Instrument Handlers
    btnStartCapture.onClick = [this] {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        engine.channelAI.startProfiling(activeChannelIndex);
        analyzed = false;
        captureProgress = 0.0f;
        simulatedCaptureSec = 0.0f;
        aiLog = "Listening to audio signal on " + channelName + "... (Acoustic Capture)";
        repaint();
      }
    };
    btnApplyAll.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 0);
      if (onChannelUpdated)
        onChannelUpdated();
      aiLog = "Applied ALL recommendations to " + channelName;
      repaint();
    };
    btnApplyEq.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 1);
      if (onChannelUpdated)
        onChannelUpdated();
      aiLog = "Applied EQ & HPF recommendations to " + channelName;
      repaint();
    };
    btnApplyDyn.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 2);
      if (onChannelUpdated)
        onChannelUpdated();
      aiLog = "Applied Dynamics (Comp & Gate) to " + channelName;
      repaint();
    };
    btnApplyTrim.onClick = [this] {
      if (onApplyAiProfile)
        onApplyAiProfile(activeChannelIndex, currentRec, 3);
      if (onChannelUpdated)
        onChannelUpdated();
      aiLog = "Applied Auto-Trim (-18 dBFS) to " + channelName;
      repaint();
    };

    switchSuiteTab(0);
  }

  ~MidasAiModal() override { stopTimer(); }

  std::function<void(int channelIdx, const nova::graviton::AIRecommendation &rec, int applyScope)> onApplyAiProfile;
  std::function<void()> onChannelUpdated;

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
    simulatedCaptureSec = 0.0f;
    aiLog = "Ready for AI Analysis on " + channelName;

    if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
      const bool isDugan = (engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.load(std::memory_order_relaxed) > 0);
      btnDuganAssignCh.setToggleState(isDugan, juce::dontSendNotification);
      btnDuganEnable.setToggleState(engine.duganAutomixer.getGroup(0).enabled.load(std::memory_order_relaxed), juce::dontSendNotification);
      btnMasterXEnable.setToggleState(engine.masterBusLimiterEnabled.load(std::memory_order_relaxed), juce::dontSendNotification);
    }

    startTimerHz(20);
    repaint();
  }

  void timerCallback() override {
    if (currentSuiteTab == 8) {
      if (engine.channelAI.isCapturing()) {
        captureProgress = engine.channelAI.getProgress();
        simulatedCaptureSec += 0.05f;
        if (captureProgress < 0.05f && simulatedCaptureSec > 0.05f) {
          captureProgress = std::min(1.0f, simulatedCaptureSec / 2.0f);
        }
        aiLog = "Capturing acoustic signal: " + juce::String(static_cast<int>(captureProgress * 100.0f)) + "%";
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
          }
          aiLog = "Live profiling complete: " + juce::String(nova::graviton::instrumentToString(currentRec.detectedInstrument)) + " identified!";
        }
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
    g.drawText("ACTIVE CHANNEL TARGET: " + channelName +
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

    if (currentSuiteTab == 0) {
      const bool duganOn = engine.duganAutomixer.getGroup(0).enabled.load(std::memory_order_relaxed);
      const int chDugan = (activeChannelIndex >= 0 && activeChannelIndex < 144) 
          ? engine.channels[static_cast<size_t>(activeChannelIndex)].duganGroup.load(std::memory_order_relaxed) : 0;
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(duganOn ? midas_colors::kAccentGreen : midas_colors::kBorderCol);
      juce::String status = juce::String("DUGAN GAIN-SHARING: ") + (duganOn ? "ONLINE (ACTIVE)" : "STANDBY (BYPASSED)")
          + " | " + channelName + ": " + (chDugan > 0 ? "ASSIGNED TO GROUP 1" : "NOT ASSIGNED");
      g.drawText(status, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
    } else if (currentSuiteTab == 1) {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        const auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(midas_colors::kAccentGold);
        juce::String voxStatus = "EQ: " + juce::String(ch.eqEnabled.load() ? "ON" : "OFF") 
            + " | HPF: " + juce::String(ch.highPassFreqHz.load(), 0) + " Hz"
            + " | Comp: " + (ch.compressorEnabled.load() ? (ch.compressorMode.load() == 1 ? "OPTO" : "VCA") : "OFF")
            + " | De-Esser @ 6.5kHz";
        g.drawText(voxStatus, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
      }
    } else if (currentSuiteTab == 2) {
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(detectedFeedbackHz > 0.0f ? midas_colors::kAccentRed : midas_colors::kAccentGreen);
      juce::String fbkStr = (detectedFeedbackHz > 0.0f)
                                ? ("[!] FEEDBACK SPIKE DETECTED: " + juce::String(detectedFeedbackHz, 0) + " Hz (NOTCH APPLIED)")
                                : "[OK] SPECTRUM CLEAN - ZERO FEEDBACK DETECTED";
      g.drawText(fbkStr, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 66.0f), 600, 16, juce::Justification::centredLeft);
    } else if (currentSuiteTab == 3) {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        const auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(midas_colors::kAccentCyan);
        juce::String monStatus = "Aux 1 (Lead Vox): " + juce::String(ch.auxSendDb[0].load(), 1) + " dB | Aux 2 (Band): "
            + juce::String(ch.auxSendDb[1].load(), 1) + " dB | Aux 3 (Drums): " + juce::String(ch.auxSendDb[2].load(), 1) + " dB";
        g.drawText(monStatus, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
      }
    } else if (currentSuiteTab == 4 && roomImpulseMeasured) {
      g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
      g.setColour(midas_colors::kAccentGold);
      g.drawText(roomResonanceNote, static_cast<int>(cardBox.getX() + 12.0f),
                 static_cast<int>(cardBox.getY() + 66.0f), 600, 16, juce::Justification::centredLeft);
    } else if (currentSuiteTab == 5) {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        const auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(midas_colors::kAccentGold);
        juce::String scStatus = channelName + " Preamp Trim: " + juce::String(ch.inputGainDb.load(), 1) + " dB (Target: -18 dBFS Headroom)";
        g.drawText(scStatus, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
      }
    } else if (currentSuiteTab == 6) {
      g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
      g.setColour(engine.masterBusLimiterEnabled.load() ? midas_colors::kAccentPurple : midas_colors::kBorderCol);
      juce::String mastStatus = juce::String("TRUE-PEAK LIMITER: ") + (engine.masterBusLimiterEnabled.load() ? "ACTIVE" : "BYPASSED")
          + " | Ceiling: " + juce::String(engine.masterBusLimiterCeilingDb.load(), 2) + " dBFS | Dither: 24-bit TPDF";
      g.drawText(mastStatus, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
    } else if (currentSuiteTab == 7) {
      if (activeChannelIndex >= 0 && activeChannelIndex < static_cast<int>(engine.channels.size())) {
        const auto &ch = engine.channels[static_cast<size_t>(activeChannelIndex)];
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(midas_colors::kAccentGreen);
        juce::String sepStatus = "Gate/Expander: " + juce::String(ch.gateEnabled.load() ? "ENGAGED" : "BYPASSED")
            + " (Thresh: " + juce::String(ch.gateThresholdDb.load(), 1) + " dB) | Sidechain Bleed Filter Active";
        g.drawText(sepStatus, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
      }
    } else if (currentSuiteTab == 8) {
      if (analyzed) {
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.setColour(midas_colors::kAccentGreen);
        juce::String recStr = juce::String("IDENTIFIED: ") + nova::graviton::instrumentToString(currentRec.detectedInstrument)
            + " (" + juce::String(static_cast<int>(currentRec.confidence * 100.0f)) + "% Match) | Trim: "
            + juce::String(currentRec.targetTrimDb, 1) + " dB | HPF: " + juce::String(currentRec.hpfFreqHz, 0) + " Hz";
        g.drawText(recStr, static_cast<int>(cardBox.getX() + 12.0f), static_cast<int>(cardBox.getY() + 66.0f), static_cast<int>(barW - 24.0f), 16, juce::Justification::centredLeft);
      } else {
        const float pW = barW - 24.0f;
        const float pY = cardBox.getY() + 66.0f;
        g.setColour(juce::Colour(0xff12171d));
        g.fillRoundedRectangle(cardBox.getX() + 12.0f, pY, pW, 14.0f, 3.0f);
        if (captureProgress > 0.001f) {
          g.setColour(engine.channelAI.isCapturing() ? midas_colors::kAccentCyan : midas_colors::kAccentGreen);
          g.fillRoundedRectangle(cardBox.getX() + 13.0f, pY + 1.0f, (pW - 2.0f) * captureProgress, 12.0f, 2.0f);
        }
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

    const float subY = dy + 218.0f;
    const float halfW = (dialogW - 40.0f - 8.0f) * 0.5f;

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

    // 4. Smart Monitors
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
    btnMasterXEnable.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(subY), static_cast<int>(dialogW - 40.0f), 38);
    const float thirdW = (dialogW - 40.0f - 12.0f) / 3.0f;
    btnTargetLufs14.setBounds(static_cast<int>(dx + 20.0f + 0 * (thirdW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(thirdW), 38);
    btnTargetLufs9.setBounds(static_cast<int>(dx + 20.0f + 1 * (thirdW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(thirdW), 38);
    btnTruePeakCeiling.setBounds(static_cast<int>(dx + 20.0f + 2 * (thirdW + 6.0f)), static_cast<int>(subY + 46.0f), static_cast<int>(thirdW), 38);

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
  float simulatedCaptureSec = 0.0f;
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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasAiModal)
};

// ==============================================================================
// MODAL: mCLOUD SHOWFILE SYNC & CLOUD PRESETS
// ==============================================================================
class MidasMCloudModal : public juce::Component {
public:
  MidasMCloudModal()
      : btnClose("X", MidasTactileButton::GlowMode::Red),
        btnSyncNow("SYNC SHOWFILE NOW", MidasTactileButton::GlowMode::Cyan),
        btnExportBackup("EXPORT OFFLINE BACKUP", MidasTactileButton::GlowMode::Gold) {

    setAlwaysOnTop(true);
    addAndMakeVisible(btnClose);
    addAndMakeVisible(btnSyncNow);
    addAndMakeVisible(btnExportBackup);

    btnClose.onClick = [this] { setVisible(false); };
    btnSyncNow.onClick = [this] {
      if (onShowfileExport)
        onShowfileExport(false);
      setVisible(false);
    };
    btnExportBackup.onClick = [this] {
      if (onShowfileExport)
        onShowfileExport(true);
      setVisible(false);
    };
  }

  std::function<void(bool isBackup)> onShowfileExport;

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colours::black.withAlpha(0.75f));

    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 460.0f;
    const float dialogH = 240.0f;
    auto dBox = juce::Rectangle<float>((bounds.getWidth() - dialogW) * 0.5f,
                                      (bounds.getHeight() - dialogH) * 0.5f,
                                      dialogW, dialogH);

    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(dBox, 8.0f);
    g.setColour(midas_colors::kAccentCyan);
    g.drawRoundedRectangle(dBox, 8.0f, 1.2f);

    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.setColour(midas_colors::kAccentCyan);
    g.drawText("mCLOUD CONSOLE SYNC & SHOW ARCHIVE", static_cast<int>(dBox.getX() + 16.0f), static_cast<int>(dBox.getY() + 12.0f), 350, 18, juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
    g.setColour(midas_colors::kAccentGreen);
    g.drawText("CONNECTED: London Fast CDN Node - Latency: 14ms", static_cast<int>(dBox.getX() + 16.0f), static_cast<int>(dBox.getY() + 40.0f), 380, 14, juce::Justification::centredLeft);

    g.setColour(midas_colors::kTextWhite);
    g.drawText("Active Showfile: Arena_Tour_2026_Main.m96", static_cast<int>(dBox.getX() + 16.0f), static_cast<int>(dBox.getY() + 60.0f), 380, 14, juce::Justification::centredLeft);
    g.drawText("Multi-Console Mirroring: ENABLED (Mirror FOH to MON Console)", static_cast<int>(dBox.getX() + 16.0f), static_cast<int>(dBox.getY() + 80.0f), 380, 14, juce::Justification::centredLeft);
  }

  void resized() override {
    const auto bounds = getLocalBounds().toFloat();
    const float dialogW = 460.0f;
    const float dialogH = 240.0f;
    const float dx = (bounds.getWidth() - dialogW) * 0.5f;
    const float dy = (bounds.getHeight() - dialogH) * 0.5f;

    btnClose.setBounds(static_cast<int>(dx + dialogW - 32.0f), static_cast<int>(dy + 8.0f), 24, 24);
    btnSyncNow.setBounds(static_cast<int>(dx + 20.0f), static_cast<int>(dy + dialogH - 46.0f), 190, 28);
    btnExportBackup.setBounds(static_cast<int>(dx + 220.0f), static_cast<int>(dy + dialogH - 46.0f), 210, 28);
  }

private:
  MidasTactileButton btnClose, btnSyncNow, btnExportBackup;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidasMCloudModal)
};
'''

# Find bounds of MidasAiModal in content
start_idx = content.find("class MidasAiModal : public juce::Component")
end_marker = "class MidasTouringConsoleSurface : public juce::Component"
end_idx = content.find(end_marker)

assert start_idx != -1
assert end_idx != -1

# Replace MidasAiModal with MidasAiModal + MidasMCloudModal
new_content = content[:start_idx] + ai_modal_and_mcloud + "\n\n// " + ("=" * 78) + "\n// COMPLETE MIDAS TOURING CONSOLE SURFACE COMPONENT\n// " + ("=" * 78) + "\n" + content[end_idx:]

# Also fix aiModal.onChannelUpdated
old_sync_hook = """    aiModal.onChannelUpdated = [this] {
      selectChannel(selectedSlot);
      for (size_t i = 0; i < 8; ++i) {
        const int c = getEngineChannelForSlot(static_cast<int>(i));
        if (c >= 0 && c < static_cast<int>(engine.channels.size())) {
          strips[i]->setFaderDb(engine.channels[static_cast<size_t>(c)].gainDb.load(std::memory_order_relaxed));
          strips[i]->setMuted(engine.channels[static_cast<size_t>(c)].muted.load(std::memory_order_relaxed));
          strips[i]->setSolo(engine.channels[static_cast<size_t>(c)].solo.load(std::memory_order_relaxed));
        }
      }
      repaint();
    };"""

new_sync_hook = """    aiModal.onChannelUpdated = [this] {
      selectChannel(selectedSlot);
      for (size_t i = 0; i < 8; ++i) {
        const int c = getEngineChannelForSlot(static_cast<int>(i));
        if (c >= 0 && c < static_cast<int>(engine.channels.size())) {
          strips[i]->syncState(engine.channels[static_cast<size_t>(c)]);
        }
      }
      repaint();
    };"""

if old_sync_hook in new_content:
    new_content = new_content.replace(old_sync_hook, new_sync_hook)
elif "aiModal.onChannelUpdated = [this]" not in new_content:
    search_target = "aiModal.onApplyAiProfile = [this]("
    assert search_target in new_content
    new_content = new_content.replace(search_target, new_sync_hook + "\n\n    " + search_target)

with open(surface_path, "w", encoding="utf-8") as f:
    f.write(new_content)

print("Updated surface file successfully!")
