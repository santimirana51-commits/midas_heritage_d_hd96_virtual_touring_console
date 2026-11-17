import os

path = r"Source/GUI/MidasTouringConsoleSurface.h"
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Update MidasMasterProcessingPanel class
old_panel_def = """  std::function<void(float lowDb, float midDb, float highDb)> onMasterEqChanged;
  std::function<void(float thrDb, float ratio)> onMasterCompChanged;
  std::function<void(bool bypassed)> onMasterBypassToggled;
  std::function<void(float mstDelayMs)> onMasterDelayChanged;
  std::function<void(float monDelayMs)> onMonitorDelayChanged;
  std::function<void()> onOpenAudioSettings;

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kAccentGold.withAlpha(0.6f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MAIN STEREO PROCESSING (3-BAND EQ • MASTER BUS COMP • STEREO "
               "ENHANCER • LIMITER)",
               12, 4, 520, 14, juce::Justification::centredLeft);
  }"""

new_panel_def = """  std::function<void(float lowDb, float midDb, float highDb)> onMasterEqChanged;
  std::function<void(float thrDb, float ratio)> onMasterCompChanged;
  std::function<void(bool bypassed)> onMasterBypassToggled;
  std::function<void(float mstDelayMs)> onMasterDelayChanged;
  std::function<void(float monDelayMs)> onMonitorDelayChanged;
  std::function<void()> onOpenAudioSettings;

  void setFifoTelemetry(int depthSamples, float latencyMs, uint32_t overruns, uint32_t underruns) {
    juce::String str = "MON FIFO: " + juce::String(depthSamples) + " smp (" +
                       juce::String(latencyMs, 1) + " ms) | Ovr: " + juce::String(overruns) +
                       " Und: " + juce::String(underruns);
    if (fifoTelemetryStr != str) {
      fifoTelemetryStr = str;
      repaint();
    }
  }

  void updateBtSyncVisual(float mstMs) {
    if (mstMs < 1.0f) {
      btnBtSync.setButtonText("SYNC BT: OFF");
      btnBtSync.setToggleState(false, juce::dontSendNotification);
      btnBtSync.setGlowMode(MidasTactileButton::GlowMode::Gray);
    } else if (std::abs(mstMs - 120.0f) < 2.0f) {
      btnBtSync.setButtonText("SYNC BT: 120ms");
      btnBtSync.setToggleState(true, juce::dontSendNotification);
      btnBtSync.setGlowMode(MidasTactileButton::GlowMode::Cyan);
    } else {
      btnBtSync.setButtonText("SYNC BT: " + juce::String(static_cast<int>(mstMs)) + "ms");
      btnBtSync.setToggleState(true, juce::dontSendNotification);
      btnBtSync.setGlowMode(MidasTactileButton::GlowMode::Gold);
    }
  }

  void setDelays(float mstMs, float monMs) {
    if (std::abs(outDelay.getValue() - mstMs) > 0.05f) {
      outDelay.setValue(mstMs, juce::dontSendNotification);
      updateBtSyncVisual(mstMs);
    }
    if (std::abs(monDelay.getValue() - monMs) > 0.05f) {
      monDelay.setValue(monMs, juce::dontSendNotification);
    }
  }

  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.setColour(midas_colors::kDeckBg);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(midas_colors::kAccentGold.withAlpha(0.6f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(midas_ui::getFont(9.5f, true));
    g.setColour(midas_colors::kAccentGold);
    g.drawText("MAIN STEREO PROCESSING (3-BAND EQ • MASTER BUS COMP • STEREO ENHANCER • LIMITER)",
               12, 4, 460, 14, juce::Justification::centredLeft);

    // Live Secondary Monitor FIFO & Drift Telemetry (Priority 3)
    if (fifoTelemetryStr.isNotEmpty()) {
      g.setFont(midas_ui::getFont(8.5f, true));
      g.setColour(juce::Colour(0xff4dd0e1));
      g.drawText(fifoTelemetryStr, static_cast<int>(area.getWidth() - 480.0f), 4, 255, 14, juce::Justification::centredRight);
    }
  }"""

if old_panel_def in content:
    content = content.replace(old_panel_def, new_panel_def)
    print("MidasMasterProcessingPanel definition updated.")
else:
    print("WARNING: old_panel_def not found!")

# 2. Add fifoTelemetryStr member to MidasMasterProcessingPanel
old_members = """private:
  MidasKnob lowEq, midEq, highEq, compThr, compRatio, stWidth, outDelay, monDelay;
  MidasTactileButton btnBtSync, btnAudioOut, btnBypass;
};"""

new_members = """private:
  MidasKnob lowEq, midEq, highEq, compThr, compRatio, stWidth, outDelay, monDelay;
  MidasTactileButton btnBtSync, btnAudioOut, btnBypass;
  juce::String fifoTelemetryStr;
};"""

if old_members in content:
    content = content.replace(old_members, new_members)
    print("fifoTelemetryStr member added.")
else:
    print("WARNING: old_members not found!")

# 3. Update outDelay, monDelay, btnBtSync callbacks in MidasMasterProcessingPanel::resized()
old_callbacks = """    outDelay.onValueChanged = [this](float ms) {
      if (onMasterDelayChanged)
        onMasterDelayChanged(ms);
    };
    monDelay.onValueChanged = [this](float ms) {
      if (onMonitorDelayChanged)
        onMonitorDelayChanged(ms);
    };
    btnBtSync.onClick = [this] {
      // Toggle / apply typical Bluetooth 120ms latency sync on Master
      const float cur = outDelay.getValue();
      const float nextVal = (cur < 10.0f) ? 120.0f : 0.0f;
      outDelay.setValue(nextVal);
    };"""

new_callbacks = """    outDelay.onValueChanged = [this](float ms) {
      updateBtSyncVisual(ms);
      if (onMasterDelayChanged)
        onMasterDelayChanged(ms);
    };
    monDelay.onValueChanged = [this](float ms) {
      if (onMonitorDelayChanged)
        onMonitorDelayChanged(ms);
    };
    btnBtSync.onClick = [this] {
      // Priority 1: Strictly toggle between 0ms (default OFF) and 120ms (Bluetooth Latency Alignment)
      const float cur = outDelay.getValue();
      const float nextVal = (cur < 10.0f) ? 120.0f : 0.0f;
      outDelay.setValue(nextVal);
    };"""

if old_callbacks in content:
    content = content.replace(old_callbacks, new_callbacks)
    print("Delay callbacks updated.")
else:
    print("WARNING: old_callbacks not found!")

# 4. Update showAudioOutMenu with Buffer size selector (Priority 2) and FIFO reset/status (Priority 3)
old_menu = """    auto showAudioOutMenu = [this] {
      juce::PopupMenu m;
      m.addItem(1, "1. Master Output Setup (WASAPI 1 / ASIO / Soundcard)...");
      m.addSeparator();
      m.addSectionHeader("Monitor Output (WASAPI 2 / Bluetooth Device):");
      juce::StringArray btDevices;
      if (secondaryStreamer != nullptr) {
        btDevices = secondaryStreamer->getAvailableOutputDeviceNames();
        const auto activeName = secondaryStreamer->getCurrentDeviceName();
        for (int i = 0; i < btDevices.size(); ++i) {
          const bool isCurrent = (btDevices[i] == activeName && secondaryStreamer->isDeviceOpen());
          m.addItem(100 + i, btDevices[i], true, isCurrent);
        }
        if (secondaryStreamer->isDeviceOpen())
          m.addItem(99, "Disconnect Monitor (WASAPI 2)");
      } else {
        m.addItem(98, "Secondary Streamer Initializing...", false);
      }
      m.addSeparator();
      const bool quadOn = engine.dualOutput4ChMode.load(std::memory_order_relaxed);
      m.addItem(200, "Dual Output 4-Channel Mode (Ch 1-2 Master, Ch 3-4 Monitor)", true, quadOn);

      m.showMenuAsync(juce::PopupMenu::Options(), [this, btDevices](int result) {
        if (result == 1) {
          if (onOpenAudioSettings)
            onOpenAudioSettings();
        } else if (result == 99) {
          if (secondaryStreamer != nullptr)
            secondaryStreamer->closeDevice();
        } else if (result >= 100 && result < 100 + btDevices.size()) {
          const int idx = result - 100;
          if (secondaryStreamer != nullptr && idx < btDevices.size()) {
            secondaryStreamer->openDevice(btDevices[idx]);
          }
        } else if (result == 200) {
          const bool cur = engine.dualOutput4ChMode.load(std::memory_order_relaxed);
          engine.dualOutput4ChMode.store(!cur, std::memory_order_relaxed);
        }
      });
    };"""

new_menu = """    auto showAudioOutMenu = [this] {
      juce::PopupMenu m;
      m.addItem(1, "1. Master Output Setup (WASAPI 1 / ASIO / Soundcard)...");
      m.addSeparator();

      // Priority 2: Primary Device Buffer Size / Latency Selector
      juce::PopupMenu bufMenu;
      auto setup = deviceManager.getAudioDeviceSetup();
      const int currentBuf = setup.bufferSize;
      const double sr = (setup.sampleRate > 1000.0) ? setup.sampleRate : 48000.0;

      auto addBuf = [&](int id, int samples) {
        const float ms = (static_cast<float>(samples) / static_cast<float>(sr)) * 1000.0f;
        juce::String desc = (samples <= 64 ? " (1.3 ms - Ultra Low)" :
                             samples <= 128 ? " (2.7 ms - Pro Live Low)" :
                             samples <= 256 ? " (5.3 ms - Balanced Touring)" :
                             samples <= 512 ? " (10.7 ms - Safe High)" : " (21.3 ms - Heavy Load)");
        juce::String label = juce::String(samples) + " samples" + desc;
        bufMenu.addItem(id, label, true, currentBuf == samples);
      };
      addBuf(10, 64);
      addBuf(11, 128);
      addBuf(12, 256);
      addBuf(13, 512);
      addBuf(14, 1024);
      m.addSubMenu("Primary Device Buffer Size / Latency", bufMenu);

      m.addSeparator();
      m.addSectionHeader("Monitor Output (WASAPI 2 / Bluetooth Device):");
      juce::StringArray btDevices;
      if (secondaryStreamer != nullptr) {
        btDevices = secondaryStreamer->getAvailableOutputDeviceNames();
        const auto activeName = secondaryStreamer->getCurrentDeviceName();
        for (int i = 0; i < btDevices.size(); ++i) {
          const bool isCurrent = (btDevices[i] == activeName && secondaryStreamer->isDeviceOpen());
          m.addItem(100 + i, btDevices[i], true, isCurrent);
        }
        if (secondaryStreamer->isDeviceOpen())
          m.addItem(99, "Disconnect Monitor (WASAPI 2)");
      } else {
        m.addItem(98, "Secondary Streamer Initializing...", false);
      }

      // Priority 3: Monitor FIFO Real-Time Queue Depth & Overrun/Underrun Telemetry
      m.addSeparator();
      const int qDepth = engine.getMonitorFifoQueueDepth();
      const float qMs = engine.getMonitorFifoLatencyMs();
      const uint32_t ovr = engine.getMonitorFifoOverruns();
      const uint32_t und = engine.getMonitorFifoUnderruns();
      juce::String fifoStatus = "Monitor FIFO Queue: " + juce::String(qDepth) + " smp (" +
                                juce::String(qMs, 1) + " ms) | Overrun: " + juce::String(ovr) +
                                " | Underrun: " + juce::String(und);
      m.addItem(300, fifoStatus, false);
      m.addItem(301, "Reset Monitor FIFO & Re-align Queue (Zero Delay)");

      m.addSeparator();
      const bool quadOn = engine.dualOutput4ChMode.load(std::memory_order_relaxed);
      m.addItem(200, "Dual Output 4-Channel Mode (Ch 1-2 Master, Ch 3-4 Monitor)", true, quadOn);

      m.showMenuAsync(juce::PopupMenu::Options(), [this, btDevices](int result) {
        if (result == 1) {
          if (onOpenAudioSettings)
            onOpenAudioSettings();
        } else if (result >= 10 && result <= 14) {
          int newBuf = 256;
          if (result == 10) newBuf = 64;
          else if (result == 11) newBuf = 128;
          else if (result == 12) newBuf = 256;
          else if (result == 13) newBuf = 512;
          else if (result == 14) newBuf = 1024;
          auto setup = deviceManager.getAudioDeviceSetup();
          setup.bufferSize = newBuf;
          deviceManager.setAudioDeviceSetup(setup, true);
        } else if (result == 301) {
          engine.resetMonitorFifoStats();
        } else if (result == 99) {
          if (secondaryStreamer != nullptr)
            secondaryStreamer->closeDevice();
        } else if (result >= 100 && result < 100 + btDevices.size()) {
          const int idx = result - 100;
          if (secondaryStreamer != nullptr && idx < btDevices.size()) {
            secondaryStreamer->openDevice(btDevices[idx]);
          }
        } else if (result == 200) {
          const bool cur = engine.dualOutput4ChMode.load(std::memory_order_relaxed);
          engine.dualOutput4ChMode.store(!cur, std::memory_order_relaxed);
        }
      });
    };"""

if old_menu in content:
    content = content.replace(old_menu, new_menu)
    print("showAudioOutMenu updated with Buffer size selector and FIFO telemetry.")
else:
    print("WARNING: old_menu not found!")

# 5. In timerCallback(), call masterProcessing.setFifoTelemetry
old_timer_end = """      eqView.setDynEqGains(
          curCh.dynEqGainDb[0].load(std::memory_order_relaxed),
          curCh.dynEqGainDb[1].load(std::memory_order_relaxed),
          curCh.dynEqGainDb[2].load(std::memory_order_relaxed),
          curCh.dynEqGainDb[3].load(std::memory_order_relaxed));
    }"""

new_timer_end = """      eqView.setDynEqGains(
          curCh.dynEqGainDb[0].load(std::memory_order_relaxed),
          curCh.dynEqGainDb[1].load(std::memory_order_relaxed),
          curCh.dynEqGainDb[2].load(std::memory_order_relaxed),
          curCh.dynEqGainDb[3].load(std::memory_order_relaxed));
    }

    // Priority 3: Real-time Monitor FIFO Depth & Overrun/Underrun Telemetry Update (25 Hz)
    masterProcessing.setFifoTelemetry(
        engine.getMonitorFifoQueueDepth(),
        engine.getMonitorFifoLatencyMs(),
        engine.getMonitorFifoOverruns(),
        engine.getMonitorFifoUnderruns());"""

if old_timer_end in content:
    content = content.replace(old_timer_end, new_timer_end)
    print("setFifoTelemetry added to timerCallback.")
else:
    print("WARNING: old_timer_end not found!")

# 6. Ensure default MST DLY and MON DLY = 0 ms in constructor (Priority 1)
old_init_delays = """    static const float initialPans[8] = { 0.0f, 0.05f, -0.25f, 0.0f, -0.40f, 0.40f, -0.15f, 0.0f };
    static const float initialFaders[8] = { 1.2f, 0.0f, -4.2f, 2.0f, -1.8f, -1.8f, -3.0f, 3.6f };
    for (int i = 0; i < 8; ++i) {
      if (i < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(i)].pan.store(initialPans[i], std::memory_order_relaxed);
        engine.channels[static_cast<size_t>(i)].gainDb.store(initialFaders[i], std::memory_order_relaxed);
      }
    }"""

new_init_delays = """    static const float initialPans[8] = { 0.0f, 0.05f, -0.25f, 0.0f, -0.40f, 0.40f, -0.15f, 0.0f };
    static const float initialFaders[8] = { 1.2f, 0.0f, -4.2f, 2.0f, -1.8f, -1.8f, -3.0f, 3.6f };
    for (int i = 0; i < 8; ++i) {
      if (i < static_cast<int>(engine.channels.size())) {
        engine.channels[static_cast<size_t>(i)].pan.store(initialPans[i], std::memory_order_relaxed);
        engine.channels[static_cast<size_t>(i)].gainDb.store(initialFaders[i], std::memory_order_relaxed);
      }
    }

    // Priority 1: Ensure MST DLY and MON DLY strictly initialized to 0.0 ms
    engine.masterDelayMs.store(0.0f, std::memory_order_relaxed);
    engine.monitorDelayMs.store(0.0f, std::memory_order_relaxed);
    masterProcessing.setDelays(0.0f, 0.0f);"""

if old_init_delays in content:
    content = content.replace(old_init_delays, new_init_delays)
    print("Initial 0 ms delay enforcement added.")
else:
    print("WARNING: old_init_delays not found!")

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("Finished applying delay and FIFO priorities.")

