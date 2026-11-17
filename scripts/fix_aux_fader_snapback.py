import sys
import re

path = 'Source/GUI/MidasTouringConsoleSurface.h'
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# 1. Update MidasFader with isDragging protection and mouseUp
target_fader_def = """class MidasFader : public juce::Component {
public:
  MidasFader() { setRepaintsOnMouseActivity(true); }

  std::function<void(float db)> onFaderMoved;

  void setGainDb(float db,
                 juce::NotificationType notif = juce::sendNotification) {"""

replace_fader_def = """class MidasFader : public juce::Component {
public:
  MidasFader() { setRepaintsOnMouseActivity(true); }

  std::function<void(float db)> onFaderMoved;
  bool isDraggingFader() const noexcept { return isDragging; }

  void setGainDb(float db,
                 juce::NotificationType notif = juce::sendNotification) {"""

if target_fader_def in content:
    content = content.replace(target_fader_def, replace_fader_def, 1)
    print("1. Added isDraggingFader to MidasFader")
else:
    print("WARN: target_fader_def not found")

# Update mouseDown, mouseDrag, mouseUp and private isDragging in MidasFader
target_fader_mouse = """  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
      setGainDb(0.0f);
      return;
    }
    dragStartY = e.position.y;
    dbAtStart = currentDb;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    const float trackTop = 8.0f;
    const float trackBottom = getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;
    const float deltaNorm = (dragStartY - e.position.y) / trackH;
    const float startNorm = dbToNormalized(dbAtStart);
    setGainDb(normalizedToDb(startNorm + deltaNorm));
  }

  void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override {
    setGainDb(currentDb + w.deltaY * 0.5f);
  }

private:
  float currentDb = 0.0f;
  float dragStartY = 0.0f;
  float dbAtStart = 0.0f;
  bool isMaster = false;"""

replace_fader_mouse = """  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
      setGainDb(0.0f);
      return;
    }
    isDragging = true;
    const float trackTop = 8.0f;
    const float trackBottom = getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;
    const float norm = juce::jlimit(0.0f, 1.0f, (trackBottom - e.position.y) / trackH);
    setGainDb(normalizedToDb(norm));
    dragStartY = e.position.y;
    dbAtStart = currentDb;
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    isDragging = true;
    const float trackTop = 8.0f;
    const float trackBottom = getHeight() - 8.0f;
    const float trackH = trackBottom - trackTop;
    const float deltaNorm = (dragStartY - e.position.y) / trackH;
    const float startNorm = dbToNormalized(dbAtStart);
    setGainDb(normalizedToDb(startNorm + deltaNorm));
  }

  void mouseUp(const juce::MouseEvent &) override {
    isDragging = false;
  }

  void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override {
    setGainDb(currentDb + w.deltaY * 0.5f);
  }

private:
  float currentDb = 0.0f;
  float dragStartY = 0.0f;
  float dbAtStart = 0.0f;
  bool isDragging = false;
  bool isMaster = false;"""

if target_fader_mouse in content:
    content = content.replace(target_fader_mouse, replace_fader_mouse, 1)
    print("2. Updated mouse interaction and isDragging state in MidasFader")
else:
    print("WARN: target_fader_mouse not found")

# 2. Update MidasConsoleStrip sync methods with isDragging protection
target_cs_sync = """  void syncState(const ChannelState &state) {
    const float fDb = state.gainDb.load(std::memory_order_relaxed);
    fader.setGainDb(fDb, juce::dontSendNotification);
    btnMute.setToggleState(state.muted.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    btnSolo.setToggleState(state.solo.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    panKnob.setValue(state.pan.load(std::memory_order_relaxed),
                     juce::dontSendNotification);

    const float pk = state.peak.load(std::memory_order_relaxed);
    const float pkDb =
        (pk > 1.0e-5f) ? juce::Decibels::gainToDecibels(pk, -60.0f) : -60.0f;
    meter.setLevels(pkDb);
  }

  void syncBusOrVcaState(float faderDb, bool muted, float linearPeak) {
    fader.setGainDb(faderDb, juce::dontSendNotification);
    btnMute.setToggleState(muted, juce::dontSendNotification);
    const float pkDb = (linearPeak > 1.0e-5f)
                           ? juce::Decibels::gainToDecibels(linearPeak, -60.0f)
                           : -60.0f;
    meter.setLevels(pkDb);
    repaint();
  }"""

replace_cs_sync = """  bool isFaderDragging() const noexcept { return fader.isDraggingFader(); }

  void syncState(const ChannelState &state) {
    if (!fader.isDraggingFader()) {
      if (isSofMonitorMode) {
        fader.setGainDb(state.monitorSendDb.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
        fader.setFaderCapColor(midas_colors::kAccentCyan);
      } else {
        const float fDb = state.gainDb.load(std::memory_order_relaxed);
        fader.setGainDb(fDb, juce::dontSendNotification);
        fader.setFaderCapColor(accentColor);
      }
    }
    btnMute.setToggleState(state.muted.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    btnSolo.setToggleState(state.solo.load(std::memory_order_relaxed),
                           juce::dontSendNotification);
    panKnob.setValue(state.pan.load(std::memory_order_relaxed),
                     juce::dontSendNotification);

    isPreFader = state.monitorPreFader.load(std::memory_order_relaxed);

    const float pk = state.peak.load(std::memory_order_relaxed);
    const float pkDb =
        (pk > 1.0e-5f) ? juce::Decibels::gainToDecibels(pk, -60.0f) : -60.0f;
    meter.setLevels(pkDb);
  }

  void syncBusOrVcaState(float faderDb, bool muted, float linearPeak) {
    if (!fader.isDraggingFader()) {
      fader.setGainDb(faderDb, juce::dontSendNotification);
    }
    btnMute.setToggleState(muted, juce::dontSendNotification);
    const float pkDb = (linearPeak > 1.0e-5f)
                           ? juce::Decibels::gainToDecibels(linearPeak, -60.0f)
                           : -60.0f;
    meter.setLevels(pkDb);
    repaint();
  }"""

if target_cs_sync in content:
    content = content.replace(target_cs_sync, replace_cs_sync, 1)
    print("3. Updated MidasConsoleStrip syncState & syncBusOrVcaState with dragging guards")
else:
    print("WARN: target_cs_sync not found")

# 3. Update MidasMasterStrip setFaderDb with isDragging guard
target_mst_setfader = """  void setFaderDb(float db) {
    masterFader.setGainDb(db, juce::dontSendNotification);
    repaint();
  }"""
replace_mst_setfader = """  void setFaderDb(float db) {
    if (!masterFader.isDraggingFader()) {
      masterFader.setGainDb(db, juce::dontSendNotification);
      repaint();
    }
  }"""
if target_mst_setfader in content:
    content = content.replace(target_mst_setfader, replace_mst_setfader, 1)
    print("4. Updated MidasMasterStrip setFaderDb with dragging guard")
else:
    print("WARN: target_mst_setfader not found")

# 4. Clean up MidasTopStatusBar SOF click so it doesn't fire both callbacks
target_stat_sof = """      if (onSofModeChanged)
        onSofModeChanged(sofMode);
      if (onSofToggled)
        onSofToggled(sofMode != 0);"""

replace_stat_sof = """      if (onSofModeChanged)
        onSofModeChanged(sofMode);
      else if (onSofToggled)
        onSofToggled(sofMode != 0);"""

if target_stat_sof in content:
    content = content.replace(target_stat_sof, replace_stat_sof, 1)
    print("5. Cleaned up MidasTopStatusBar SOF click dispatch")
else:
    print("WARN: target_stat_sof not found")

# 5. Fix onFaderChanged in MidasTouringConsoleSurface
target_fader_chg = """        if (currentSofMode == 1) { // SOF: MONITOR
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)]
                .monitorSendDb.store(db, std::memory_order_relaxed);
          }
          return;
        }
        if (sofActive) {
          const int selCh = getEngineChannelForSlot(selectedSlot);
          if (selCh >= 0 && selCh < static_cast<int>(engine.channels.size()) &&
              slot >= 0 && slot < 8) {
            engine.channels[static_cast<size_t>(selCh)]
                .auxSendDb[static_cast<size_t>(slot)]
                .store(db, std::memory_order_relaxed);
            engine.syncAuxRoutingSnapshot();
          }
          return;
        }
        if (currentBank == 2) {
          engine.auxMasterDb[static_cast<size_t>(slot)].store(
              db, std::memory_order_relaxed);
          return;
        }"""

replace_fader_chg = """        if (currentSofMode == 1) { // SOF: MONITOR (Controls channel monitor send level)
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)]
                .monitorSendDb.store(db, std::memory_order_relaxed);
          }
          return;
        }
        if (currentSofMode == 2) { // SOF: MIX 1-2 (Controls each channel send to Mix 1-2)
          const int chIdx = getEngineChannelForSlot(slot);
          if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
            engine.channels[static_cast<size_t>(chIdx)]
                .auxSendDb[0].store(db, std::memory_order_relaxed);
            engine.channels[static_cast<size_t>(chIdx)]
                .auxSendDb[1].store(db, std::memory_order_relaxed);
            engine.syncAuxRoutingSnapshot();
          }
          return;
        }
        if (currentBank == 2) { // Bank 2: MIX BUS / AUX Masters (Aux 1-8 Masters)
          engine.auxMasterDb[static_cast<size_t>(slot)].store(
              db, std::memory_order_relaxed);
          engine.syncAuxRoutingSnapshot();
          return;
        }"""

if target_fader_chg in content:
    content = content.replace(target_fader_chg, replace_fader_chg, 1)
    print("6. Corrected onFaderChanged logic for SOF and Aux Masters")
else:
    print("WARN: target_fader_chg not found")

# 6. Replace the entire conflicting strip telemetry loop in timerCallback
target_timer_loops = """    if (currentSofMode == 1) {
      for (int i = 0; i < 8; ++i) {
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
          strips[static_cast<size_t>(i)]->setSofMonitorMode(true, ch.monitorPreFader.load(std::memory_order_relaxed));
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(
              ch.monitorSendDb.load(std::memory_order_relaxed),
              !ch.monitorAssigned.load(std::memory_order_relaxed),
              ch.peak.load(std::memory_order_relaxed));
        }
      }
    }

    // EBU R128 Loudness telemetry calculations
    const float mstMaxPeak = std::max(mstDbL, mstDbR);
    const float momLufs = std::clamp(mstMaxPeak - 3.0f, -70.0f, 0.0f);
    shortTermLufsSmooth = shortTermLufsSmooth * 0.85f + momLufs * 0.15f;
    integratedLufsSmooth = integratedLufsSmooth * 0.98f + momLufs * 0.02f;
    broadcastView.updateLoudness(integratedLufsSmooth, shortTermLufsSmooth,
                                 momLufs, mstMaxPeak);

    // Multitrack recorder SMPTE timecode progression
    if (engine.recorder.isRecording()) {
      double sr = 96000.0;
      if (auto *dev = deviceManager.getCurrentAudioDevice()) {
        sr = dev->getCurrentSampleRate();
        if (sr < 8000.0)
          sr = 96000.0;
      }
      const uint64_t frames = engine.recorder.getRecordedFrames();
      const double totalSec = static_cast<double>(frames) / sr;
      const int hours = static_cast<int>(totalSec / 3600.0);
      const int mins = static_cast<int>(std::fmod(totalSec, 3600.0) / 60.0);
      const int secs = static_cast<int>(std::fmod(totalSec, 60.0));
      const int framesRemainder =
          static_cast<int>(std::fmod(totalSec, 1.0) * 25.0);
      const auto tc = juce::String::formatted("%02d:%02d:%02d:%02d", hours,
                                              mins, secs, framesRemainder);
      recorderView.setTimecode(tc);
      recorderView.setProgress(
          static_cast<float>(std::fmod(totalSec, 300.0) / 300.0));
    }

    if ((++storageTelemetryTick % 50) == 0) {
      recorderView.updateStorageInfo();
    }

    const int selCh = getEngineChannelForSlot(selectedSlot);
    for (int i = 0; i < 8; ++i) {
      if (sofActive && selCh >= 0 &&
          selCh < static_cast<int>(engine.channels.size())) {
        const float sendDb = engine.channels[static_cast<size_t>(selCh)]
                                 .auxSendDb[static_cast<size_t>(i)]
                                 .load(std::memory_order_relaxed);
        const float auxPk = engine.auxPeak[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(sendDb, false, auxPk);
      } else if (currentBank == 2) {
        const float auxDb = engine.auxMasterDb[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        const bool auxMt = engine.auxMuted[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        const float auxPk = engine.auxPeak[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(auxDb, auxMt, auxPk);
      } else if (currentBank == 3) {
        const float vcaDb = engine.vcaMasterDb[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        const bool vcaMt = engine.vcaMuted[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(vcaDb, vcaMt, 0.0f);
      } else {
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          strips[static_cast<size_t>(i)]->syncState(
              engine.channels[static_cast<size_t>(chIdx)]);
        }
      }
    }"""

replace_timer_loops = """    // Consolidated, non-conflicting strip telemetry update loop
    for (int i = 0; i < 8; ++i) {
      if (currentSofMode == 1) { // 1. SOF: MONITOR (Controls and reflects channel Monitor Send Level)
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
          strips[static_cast<size_t>(i)]->setSofMonitorMode(true, ch.monitorPreFader.load(std::memory_order_relaxed));
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(
              ch.monitorSendDb.load(std::memory_order_relaxed),
              !ch.monitorAssigned.load(std::memory_order_relaxed),
              ch.peak.load(std::memory_order_relaxed));
        }
      } else if (currentSofMode == 2) { // 2. SOF: MIX 1-2 (Reflects channel send level to Mix 1-2)
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          const auto &ch = engine.channels[static_cast<size_t>(chIdx)];
          strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
          const float sendDb = ch.auxSendDb[0].load(std::memory_order_relaxed);
          const float pk = ch.peak.load(std::memory_order_relaxed);
          strips[static_cast<size_t>(i)]->syncBusOrVcaState(sendDb, false, pk);
        }
      } else if (currentBank == 2) { // 3. Bank 2: MIX BUS / AUX (Aux 1-8 Masters)
        strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        const float auxDb = engine.auxMasterDb[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        const bool auxMt = engine.auxMuted[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        const float auxPk = engine.auxPeak[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(auxDb, auxMt, auxPk);
      } else if (currentBank == 3) { // 4. Bank 3: VCA GROUPS
        strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        const float vcaDb = engine.vcaMasterDb[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        const bool vcaMt = engine.vcaMuted[static_cast<size_t>(i)].load(
            std::memory_order_relaxed);
        strips[static_cast<size_t>(i)]->syncBusOrVcaState(vcaDb, vcaMt, 0.0f);
      } else { // 5. Normal input channels
        strips[static_cast<size_t>(i)]->setSofMonitorMode(false);
        const int chIdx = getEngineChannelForSlot(i);
        if (chIdx >= 0 && chIdx < static_cast<int>(engine.channels.size())) {
          strips[static_cast<size_t>(i)]->syncState(
              engine.channels[static_cast<size_t>(chIdx)]);
        }
      }
    }

    // EBU R128 Loudness telemetry calculations
    const float mstMaxPeak = std::max(mstDbL, mstDbR);
    const float momLufs = std::clamp(mstMaxPeak - 3.0f, -70.0f, 0.0f);
    shortTermLufsSmooth = shortTermLufsSmooth * 0.85f + momLufs * 0.15f;
    integratedLufsSmooth = integratedLufsSmooth * 0.98f + momLufs * 0.02f;
    broadcastView.updateLoudness(integratedLufsSmooth, shortTermLufsSmooth,
                                 momLufs, mstMaxPeak);

    // Multitrack recorder SMPTE timecode progression
    if (engine.recorder.isRecording()) {
      double sr = 96000.0;
      if (auto *dev = deviceManager.getCurrentAudioDevice()) {
        sr = dev->getCurrentSampleRate();
        if (sr < 8000.0)
          sr = 96000.0;
      }
      const uint64_t frames = engine.recorder.getRecordedFrames();
      const double totalSec = static_cast<double>(frames) / sr;
      const int hours = static_cast<int>(totalSec / 3600.0);
      const int mins = static_cast<int>(std::fmod(totalSec, 3600.0) / 60.0);
      const int secs = static_cast<int>(std::fmod(totalSec, 60.0));
      const int framesRemainder =
          static_cast<int>(std::fmod(totalSec, 1.0) * 25.0);
      const auto tc = juce::String::formatted("%02d:%02d:%02d:%02d", hours,
                                              mins, secs, framesRemainder);
      recorderView.setTimecode(tc);
      recorderView.setProgress(
          static_cast<float>(std::fmod(totalSec, 300.0) / 300.0));
    }

    if ((++storageTelemetryTick % 50) == 0) {
      recorderView.updateStorageInfo();
    }"""

if target_timer_loops in content:
    content = content.replace(target_timer_loops, replace_timer_loops, 1)
    print("7. Consolidated non-conflicting strip update loops in timerCallback")
else:
    print("WARN: target_timer_loops not found")

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)

print("SUCCESS: fix_aux_fader_snapback.py completed successfully")

