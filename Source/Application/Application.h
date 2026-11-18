#pragma once
#include "Audio/SecondaryAudioStreamer.h"
#include "GUI/MainComponent.h"
#include "MIDI/MixerMidiCallback.h"
#include "Mixer/MixerEngine.h"
#include <cmath>
#include <fstream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

inline void appendStartupLog(const std::string &msg) {
  const auto logDirectory =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getChildFile("OPENLIVE_HD");
  if (logDirectory.createDirectory().failed())
    return;

  const auto logFile = logDirectory.getChildFile("app_startup.log");
  std::ofstream ofs(logFile.getFullPathName().toStdString(), std::ios::app);
  if (ofs.is_open()) {
    ofs << msg << '\\n';
    ofs.flush();
  }
}

class Application final : public juce::JUCEApplication,
                          private juce::ChangeListener {
public:
  Application() {}
  const juce::String getApplicationName() override { return "NOVA HD"; }
  const juce::String getApplicationVersion() override { return "1.7.0"; }
  bool moreThanOneInstanceAllowed() override { return false; }

  void initialise(const juce::String &) override {
    appendStartupLog("Application::initialise entered");

    auto cfgFile = getAudioConfigFile();
    std::unique_ptr<juce::XmlElement> savedXml;
    if (cfgFile.existsAsFile())
      savedXml = juce::XmlDocument::parse(cfgFile);

    juce::String err;
    bool usedSavedConfig = false;
    if (savedXml != nullptr) {
      err = deviceManager.initialise(2, 2, savedXml.get(), true);
      usedSavedConfig =
          err.isEmpty() && (deviceManager.getCurrentAudioDevice() != nullptr);
      appendStartupLog(
          std::string("deviceManager.initialise(savedXml) result: ") +
          (usedSavedConfig ? "OK" : err.toStdString()));
      if (usedSavedConfig) {
        if (auto *dev = deviceManager.getCurrentAudioDevice()) {
          const auto typeName = deviceManager.getCurrentAudioDeviceType();
          const bool isAsio = typeName.containsIgnoreCase("ASIO") ||
                              dev->getTypeName().containsIgnoreCase("ASIO");
          const double sr = dev->getCurrentSampleRate();
          const int minSafeWasapi = (sr >= 88200.0) ? 480 : 384;
          if (!isAsio && dev->getCurrentBufferSizeSamples() < minSafeWasapi) {
            applyLowLatencyBufferIfSupported();
          }
        }
      }
    }

    if (!usedSavedConfig) {
      // Prefer standard Windows Audio (WASAPI Shared Mode with native ~10ms
      // period & AUTOCONVERTPCM, per AudioHQ architecture) on fresh boot
      deviceManager.setCurrentAudioDeviceType("Windows Audio", true);
      err = deviceManager.initialiseWithDefaultDevices(2, 2);
      if (err.isNotEmpty() ||
          deviceManager.getCurrentAudioDevice() == nullptr) {
        err = deviceManager.initialiseWithDefaultDevices(0, 2);
      }
      appendStartupLog(std::string("WASAPI Shared Mode init result: ") +
                       (err.isEmpty() ? "OK" : err.toStdString()));

      if (err.isNotEmpty() ||
          deviceManager.getCurrentAudioDevice() == nullptr) {
        for (auto *t : deviceManager.getAvailableDeviceTypes()) {
          if (t != nullptr &&
              t->getTypeName() == "Windows Audio (Low Latency Mode)") {
            deviceManager.setCurrentAudioDeviceType(
                "Windows Audio (Low Latency Mode)", true);
            err = deviceManager.initialiseWithDefaultDevices(2, 2);
            break;
          }
        }
      }

      applyLowLatencyBufferIfSupported();
    }

    {
      auto setup = deviceManager.getAudioDeviceSetup();
      appendStartupLog(std::string("Active device type: ") +
                       deviceManager.getCurrentAudioDeviceType().toStdString() +
                       " | buffer size: " + std::to_string(setup.bufferSize));
    }

    deviceManager.addChangeListener(this);
    deviceManager.addAudioCallback(&engine);
    appendStartupLog("engine callback added");

    for (const auto &device : juce::MidiInput::getAvailableDevices()) {
      deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
      deviceManager.addMidiInputDeviceCallback(device.identifier,
                                               &midiCallback);
    }
    appendStartupLog("MIDI inputs mapped");

    appendStartupLog("Creating MainWindow...");
    mainWindow = std::make_unique<MainWindow>(getApplicationName(), engine,
                                              deviceManager, midiCallback,
                                              secondaryAudioStreamer);
    appendStartupLog("MainWindow created successfully!");
  }

  void shutdown() override {
    deviceManager.removeChangeListener(this);
    saveAudioDeviceSettings();
    secondaryAudioStreamer.closeDevice();
    deviceManager.removeAudioCallback(&engine);
    for (const auto &device : juce::MidiInput::getAvailableDevices()) {
      deviceManager.removeMidiInputDeviceCallback(device.identifier,
                                                  &midiCallback);
    }
    deviceManager.closeAudioDevice();
    mainWindow = nullptr;
  }

  void changeListenerCallback(juce::ChangeBroadcaster *source) override {
    if (source == &deviceManager)
      saveAudioDeviceSettings();
  }

  void systemRequestedQuit() override { quit(); }

  void anotherInstanceStarted(const juce::String &) override {}

private:
  static juce::File getAudioConfigFile() {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
        .getChildFile("OPENLIVE_HD")
        .getChildFile("audio_device_settings.xml");
  }

  void saveAudioDeviceSettings() {
    if (auto xml = deviceManager.createStateXml()) {
      auto cfgFile = getAudioConfigFile();
      cfgFile.getParentDirectory().createDirectory();
      xml->writeTo(cfgFile);
    }
  }

  void applyLowLatencyBufferIfSupported() {
    auto *dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr)
      return;
    const auto sizes = dev->getAvailableBufferSizes();
    if (sizes.isEmpty())
      return;

    const auto typeName = deviceManager.getCurrentAudioDeviceType();
    const bool isAsio = typeName.containsIgnoreCase("ASIO") ||
                        dev->getTypeName().containsIgnoreCase("ASIO");
    const double rawSr = dev->getCurrentSampleRate();
    const double sr = (rawSr > 1000.0) ? rawSr : 48000.0;

    int targetBuf = 0;
    if (isAsio) {
      constexpr int asioTarget = 128;
      if (sizes.contains(asioTarget)) {
        targetBuf = asioTarget;
      } else {
        int bestInRange = 0;
        int bestDist = 999999;
        int smallestAny = 999999;
        for (int s : sizes) {
          if (s >= 96 && s <= 256) {
            const int dist = std::abs(s - asioTarget);
            if (bestInRange == 0 || dist < bestDist) {
              bestInRange = s;
              bestDist = dist;
            }
          }
          if (s >= 64 && s < smallestAny)
            smallestAny = s;
        }
        if (bestInRange > 0)
          targetBuf = bestInRange;
        else if (smallestAny < 999999)
          targetBuf = smallestAny;
      }
    } else {
      // Windows Audio / WASAPI shared mode schedules user-mode audiodg threads
      // with ~3-10 ms jitter; tiny buffers (128 samples = 1.33 ms @ 96 kHz)
      // cause continuous underrun crackling. Target a safe ~10 ms buffer.
      const int wasapiTarget = (sr >= 88200.0) ? 960 : 480;
      const int minCandidate = (sr >= 88200.0) ? 384 : 256;
      const int maxCandidate = (sr >= 88200.0) ? 2048 : 1024;
      int bestInRange = 0;
      int bestDist = 999999;
      int fallbackAboveMin = 999999;
      int largestAny = 0;
      for (int s : sizes) {
        if (s >= minCandidate && s <= maxCandidate) {
          const int dist = std::abs(s - wasapiTarget);
          if (bestInRange == 0 || dist < bestDist ||
              (dist == bestDist && s > bestInRange)) {
            bestInRange = s;
            bestDist = dist;
          }
        }
        if (s >= minCandidate && s < fallbackAboveMin)
          fallbackAboveMin = s;
        if (s > largestAny)
          largestAny = s;
      }
      if (bestInRange > 0)
        targetBuf = bestInRange;
      else if (fallbackAboveMin < 999999)
        targetBuf = fallbackAboveMin;
      else
        targetBuf = largestAny;
    }

    auto setup = deviceManager.getAudioDeviceSetup();
    if (targetBuf > 0 && setup.bufferSize != targetBuf) {
      setup.bufferSize = targetBuf;
      deviceManager.setAudioDeviceSetup(setup, true);
    }
  }
  class MainWindow final : public juce::DocumentWindow {
  public:
    MainWindow(juce::String name, MixerEngine &engine,
               juce::AudioDeviceManager &dm, MixerMidiCallback &midi,
               SecondaryAudioStreamer &secondaryStreamer)
        : DocumentWindow(name, juce::Colour(0xff171b20), allButtons) {
      appendStartupLog("MainWindow constructor entered");
      setUsingNativeTitleBar(true);
      appendStartupLog("Creating MainComponent...");
      auto *comp = new MainComponent(engine, dm, midi, &secondaryStreamer);
      appendStartupLog("MainComponent instantiated, setting content owned...");
      setContentOwned(comp, true);
      appendStartupLog("setContentOwned complete");
      setResizable(true, true);
      setResizeLimits(1080, 640, 3840, 2160);
      centreWithSize(1380, 820);
      appendStartupLog("centreWithSize complete, setting visible...");
      setVisible(true);
      appendStartupLog("setVisible complete, toFront...");
      toFront(true);
      appendStartupLog("MainWindow constructor finished");
    }
    void closeButtonPressed() override {
      juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

  MixerEngine engine;
  MixerMidiCallback midiCallback{engine};
  std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
  juce::AudioDeviceManager deviceManager;
  SecondaryAudioStreamer secondaryAudioStreamer{engine};
  std::unique_ptr<MainWindow> mainWindow;
};

} // namespace nova
