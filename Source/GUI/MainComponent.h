#pragma once
#include "Mixer/MixerEngine.h"
#include "MIDI/MixerMidiCallback.h"
#include "GUI/Panels.h"
#include "GUI/NovaConsoleSurfaceJuce.h"
#include "GUI/MidasTouringConsoleSurface.h"
#include "GUI/NovaLookAndFeel.h"
#include "Audio/SecondaryAudioStreamer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace nova {

class MainComponent final : public juce::Component, private juce::Timer {
public:
  MainComponent(MixerEngine &e, juce::AudioDeviceManager &dm,
                MixerMidiCallback &midi,
                SecondaryAudioStreamer *secondaryStreamer = nullptr)
      : engine(e), deviceManager(dm), midiCallback(midi),
        secondaryAudioStreamer(secondaryStreamer),
        masterMeterL(engine.masterPeakL), masterMeterR(engine.masterPeakR) {
    setLookAndFeel(&lookAndFeel);
    title.setText("NOVA HD  /  DIGITAL MIXING SYSTEM",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    title.setTooltip("MIDI CC map: CC0-7 = CH1-8 gain; CC16-23 = CH1-8 mute "
                     "toggle; CC32 = Main; CC40-43 = AUX1-4 master");
    addAndMakeVisible(title);

    status.setColour(juce::Label::textColourId, juce::Colour(0xff48d597));
    status.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(status);

    for (auto *label : {&mainReadoutL, &mainReadoutR, &mainClipReadout}) {
      label->setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
      label->setJustificationType(juce::Justification::centredLeft);
      addAndMakeVisible(*label);
    }
    mainReadoutL.setText("L  -inf dBFS", juce::dontSendNotification);
    mainReadoutR.setText("R  -inf dBFS", juce::dontSendNotification);
    mainClipReadout.setText("OUTPUT OK", juce::dontSendNotification);
    clearClipButton.setButtonText("CLEAR CLIP");
    clearClipButton.setTooltip("Clear latched output clipping indicators");
    clearClipButton.onClick = [this] {
      engine.mainClipL.store(false, std::memory_order_relaxed);
      engine.mainClipR.store(false, std::memory_order_relaxed);
      for (auto &clip : engine.auxClip)
        clip.store(false, std::memory_order_relaxed);
      for (auto &clip : engine.matrixClip)
        clip.store(false, std::memory_order_relaxed);
    };
    addAndMakeVisible(clearClipButton);

    settings.setButtonText("AUDIO SETTINGS");
    settings.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::System, 1);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD - Audio Device Settings";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new juce::AudioDeviceSelectorComponent(
          deviceManager, 1, kInitialPhysicalInputs, 1, 8, true, true, true,
          false));
      options.content->setSize(650, 480);
      options.launchAsync();
    };
    addAndMakeVisible(settings);

    diagnosticsButton.setButtonText("AUDIO INFO");
    diagnosticsButton.setTooltip(
        "Show audio device, format, routing counts and runtime status");
    diagnosticsButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::System, 4);
        return;
      }
      auto *device = deviceManager.getCurrentAudioDevice();
      juce::String details;
      details << "NOVA HD — AUDIO DIAGNOSTICS\n\n";
      details << "Engine: "
              << (engine.running.load(std::memory_order_relaxed) ? "ONLINE"
                                                                 : "STOPPED")
              << "\n";
      details << "Device type: " << deviceManager.getCurrentAudioDeviceType()
              << "\n";
      details << "Device: "
              << (device != nullptr ? device->getName()
                                    : juce::String("No active device"))
              << "\n";
      details << "Sample rate: "
              << juce::String(engine.sampleRate.load(std::memory_order_relaxed),
                              0)
              << " Hz\n";
      details << "Buffer size: "
              << engine.blockSize.load(std::memory_order_relaxed)
              << " samples\n";
      details << "Active inputs: "
              << engine.inputCount.load(std::memory_order_relaxed) << "\n";
      details << "Active outputs: "
              << engine.outputCount.load(std::memory_order_relaxed) << "\n";
      details << "Recording: " << (engine.recorder.isRecording() ? "YES" : "NO")
              << "\n";
      details << "Recorded frames: "
              << juce::String(static_cast<juce::int64>(
                     engine.recorder.getRecordedFrames()))
              << "\n";
      details << "Dropped recording frames: "
              << juce::String(static_cast<juce::int64>(
                     engine.recorder.getDroppedFrames()))
              << "\n";
      details << "WAV playback loaded: "
              << (engine.playbackLoaded.load(std::memory_order_relaxed) ? "YES"
                                                                        : "NO")
              << "\n";
      const auto engineHealth =
          engine.deviceFailure.load(std::memory_order_relaxed)
              ? "ERROR"
              : (engine.running.load(std::memory_order_relaxed)
                     ? "RUNNING"
                     : (device != nullptr ? "DEVICE OPEN / ENGINE STOPPED"
                                          : "NO ACTIVE DEVICE"));
      details << "Health state: " << engineHealth << "\\n";
      details << "Device start events: "
              << juce::String(static_cast<juce::int64>(
                     engine.deviceStartCount.load(std::memory_order_relaxed)))
              << "\\n";
      details << "Device stop events: "
              << juce::String(static_cast<juce::int64>(
                     engine.deviceStopCount.load(std::memory_order_relaxed)))
              << "\\n";
      details << "Device error events: "
              << juce::String(static_cast<juce::int64>(
                     engine.deviceErrorCount.load(std::memory_order_relaxed)))
              << "\\n";
      details << "Audio callbacks: "
              << juce::String(static_cast<juce::int64>(
                     engine.callbackCount.load(std::memory_order_relaxed)))
              << "\\n";
      details << "Callback frames: "
              << juce::String(static_cast<juce::int64>(
                     engine.callbackFrames.load(std::memory_order_relaxed)))
              << "\\n";
      details << "Largest callback block: "
              << engine.largestCallbackBlock.load(std::memory_order_relaxed)
              << " samples\\n";
      if (device != nullptr && device->getLastError().isNotEmpty())
        details << "\nDevice error: " << device->getLastError();
      details << "Device stop events: "
              << juce::String(static_cast<juce::int64>(
                     engine.deviceStopCount.load(std::memory_order_relaxed)))
              << "\\n";
      if (engine.lastDeviceError.isNotEmpty())
        details << "\\nEngine device error: " << engine.lastDeviceError
                << "\\n";
      juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                             "NOVA HD — Audio Diagnostics",
                                             details, "CLOSE", this);
    };
    addAndMakeVisible(diagnosticsButton);

    offlineBounceButton.setButtonText("BOUNCE");
    offlineBounceButton.setTooltip(
        "Open Offline Mixdown & Multi-Track Stems Bounce window: Master L/R + "
        "bus stems to WAV/FLAC, faster than realtime (Chapter 36)");
    offlineBounceButton.setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(0xff238636));
    offlineBounceButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Show, 2);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD \xe2\x80\x94 Offline Mixdown & "
                            "Multi-Track Stems Bounce (Chapter 36)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(
          new OfflineBouncePanel(engine, bounceAudioGate()));
      options.content->setSize(900, 640);
      options.launchAsync();
    };
    addAndMakeVisible(offlineBounceButton);

    eqWindowButton.setButtonText("CHANNEL EQ");
    eqWindowButton.setTooltip("Open four-band parametric EQ editor");
    eqWindowButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 1);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Parametric EQ";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new EqPanel(engine));
      options.content->setSize(620, 300);
      options.launchAsync();
    };
    addAndMakeVisible(eqWindowButton);

    dynEqWindowButton.setButtonText("DYN EQ");
    dynEqWindowButton.setTooltip(
        "Open 4-Band Dynamic Parametric EQ Editor (MIDAS Chapter 16)");
    dynEqWindowButton.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff8957e5));
    dynEqWindowButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 2);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 4-Band Dynamic Parametric EQ (Chapter 16)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new DynamicEqPanel(engine));
      options.content->setSize(920, 560);
      options.launchAsync();
    };
    addAndMakeVisible(dynEqWindowButton);

    dynamicsWindowButton.setButtonText("DYNAMICS");
    dynamicsWindowButton.setTooltip(
        "Open gate/expander and compressor controls");
    dynamicsWindowButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 3);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Channel Dynamics";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new DynamicsPanel(engine));
      options.content->setSize(900, 480);
      options.launchAsync();
    };
    addAndMakeVisible(dynamicsWindowButton);

    selectedChannelButton.setButtonText("SELECTED CH");
    selectedChannelButton.setTooltip(
        "Open selected-channel controls and AUX sends");
    selectedChannelButton.setColour(juce::TextButton::buttonColourId,
                                    juce::Colour(0xff2563eb));
    selectedChannelButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Selected Channel";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new SelectedChannelPanel(engine));
      options.content->setSize(680, 440);
      options.launchAsync();
    };
    addAndMakeVisible(selectedChannelButton);

    channelAiButton.setButtonText("CHANNEL AI");
    channelAiButton.setTooltip("Open Channel AI Acoustic Profiler & Adaptive "
                               "Assistant (MIDAS Chapter 35)");
    channelAiButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff7c3aed));
    channelAiButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->openChannelAiModal();
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Channel AI Acoustic Profiler";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.useNativeTitleBar = true;
      options.resizable = true;
      options.content.setOwned(new ChannelAIPanel(engine, 0));
      options.content->setSize(860, 560);
      options.launchAsync();
    };
    addAndMakeVisible(channelAiButton);

    vcaWindowButton.setButtonText("VCA GROUPS");
    vcaWindowButton.setTooltip(
        "Open 24 VCA master controls & fader associations (MIDAS Chapter 10)");
    vcaWindowButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xffd97706));
    vcaWindowButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Routing, 2);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — VCA Groups";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new VcaPanel(engine));
      options.content->setSize(840, 560);
      options.launchAsync();
    };
    addAndMakeVisible(vcaWindowButton);

    matrixGridButton.setButtonText("MATRIX GRID");
    matrixGridButton.setTooltip("Open matrix crosspoint routing grid dialog");
    matrixGridButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Routing, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Matrix Crosspoint Routing Grid";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new MatrixRoutingPanel(engine));
      options.content->setSize(680, 420);
      options.launchAsync();
    };
    addAndMakeVisible(matrixGridButton);

    oscillatorButton.setButtonText("SIGNAL GEN");
    oscillatorButton.setTooltip("Open Signal Generator & System Alignment "
                                "Dialog (Sine / Pink / White)");
    oscillatorButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::System, 4);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD — Signal Generator";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new OscillatorPanel(engine));
      options.content->setSize(580, 400);
      options.launchAsync();
    };
    addAndMakeVisible(oscillatorButton);

    geqButton.setButtonText("GRAPHIC EQ");
    geqButton.setTooltip(
        "Open Klark Teknik DN370 31-band Graphic Equaliser dialog");
    geqButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 1);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 Klark Teknik DN370 Graphic EQ";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new GraphicEqPanel(engine));
      options.content->setSize(960, 320);
      options.launchAsync();
    };
    addAndMakeVisible(geqButton);

    monitorButton.setButtonText("MONITOR CR");
    monitorButton.setTooltip("Open Monitor & Control Room Section Dialog (Solo "
                             "A/B, Dim, Ducker, SIP)");
    monitorButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 5);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 Monitor & Control Room Section";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new MonitorPanel(engine));
      options.content->setSize(520, 360);
      options.launchAsync();
    };
    addAndMakeVisible(monitorButton);

    fxRackButton.setButtonText("FX RACK");
    fxRackButton.setTooltip(
        "Open Graviton 24-slot Virtual FX Rack dialog (TC VSS4, Delay, Pitch, "
        "Chorus, Flanger, Phaser, Comp, Gate, Dist)");
    fxRackButton.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xffd29922));
    fxRackButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 Graviton 24-Slot Virtual FX Rack";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new EffectsRackPanel(engine));
      options.content->setSize(980, 560);
      options.launchAsync();
    };
    addAndMakeVisible(fxRackButton);

    manchinoButton.setButtonText("MANCHINO");
    manchinoButton.setTooltip(
        "Open Manchino Multi-Edit Matrix & Show Spreadsheet Dialog");
    manchinoButton.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1f6feb));
    manchinoButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Routing, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 Manchino Multi-Edit Matrix & Show Spreadsheet";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new ManchinoPanel(engine));
      options.content->setSize(1020, 600);
      options.launchAsync();
    };
    addAndMakeVisible(manchinoButton);

    duganButton.setButtonText("DUGAN");
    duganButton.setTooltip(
        "Open Dan Dugan Speech Automixer Dialog (Chapter 28)");
    duganButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff238636));
    duganButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 4);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 Dan Dugan Speech Automixer Engine (Chapter 28)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new DuganAutomixerPanel(engine));
      options.content->setSize(860, 480);
      options.launchAsync();
    };
    addAndMakeVisible(duganButton);

    stageBoxPatchButton.setButtonText("PATCH I/O");
    stageBoxPatchButton.setTooltip("Open AES50 SuperMAC & Dante 64x64 Stage "
                                   "Box Patch Matrix (Chapters 27 & 31)");
    stageBoxPatchButton.setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(0xff0969da));
    stageBoxPatchButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Routing, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "OPENLIVE HD \xe2\x80\x94 Pro Audio Crosspoint Patch Matrix";
      options.dialogBackgroundColour = juce::Colour(0xff0d1117);
      options.content.setOwned(new StageBoxPatchPanel(engine));
      options.content->setSize(1120, 700);
      options.launchAsync();
    };
    addAndMakeVisible(stageBoxPatchButton);

    surroundPanButton.setButtonText("SURROUND 3D");
    surroundPanButton.setTooltip("Open 7.1.4 Multichannel Surround & Immersive "
                                 "3D Spatial Panner (MIDAS Chapter 30)");
    surroundPanButton.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff8957e5));
    surroundPanButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Routing, 1);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD \xe2\x80\x94 7.1.4 Surround & Immersive "
                            "3D Spatial Panner (Chapter 30)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new SurroundPanPanel(engine));
      options.content->setSize(980, 640);
      options.launchAsync();
    };
    addAndMakeVisible(surroundPanButton);

    talkbackButton.setButtonText("TALKBACK");
    talkbackButton.setTooltip(
        "Open Talkback & Comm Bus Intercom Matrix (MIDAS Chapter 32)");
    talkbackButton.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xffda3633));
    talkbackButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Mix, 5);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 Talkback & Intercom Matrix (Chapter 32)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new TalkbackPanel(engine));
      options.content->setSize(840, 520);
      options.launchAsync();
    };
    addAndMakeVisible(talkbackButton);

    oscButton.setButtonText("OSC REMOTE");
    oscButton.setTooltip(
        "Open OSC & Wireless iPad Remote Protocol Server (Chapter 27)");
    oscButton.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff1f6feb));
    oscButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::System, 3);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle =
          "NOVA HD \xe2\x80\x94 OSC Wireless Protocol Server (Chapter 27)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new OscServerPanel(engine));
      options.content->setSize(880, 540);
      options.launchAsync();
    };
    addAndMakeVisible(oscButton);

    dawStreamButton.setButtonText("DAW / V-CHECK");
    dawStreamButton.setTooltip("Open 64x64 USB Multi-Track DAW Manager & "
                               "Global Virtual Soundcheck (Chapter 26)");
    dawStreamButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff9a3412));
    dawStreamButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Show, 1);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD \xe2\x80\x94 64x64 DAW Streamer & Virtual "
                            "Soundcheck (Chapter 26)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new DAWStreamingPanel(engine));
      options.content->setSize(960, 620);
      options.launchAsync();
    };
    addAndMakeVisible(dawStreamButton);

    popGroupsButton.setButtonText("POP GROUPS");
    popGroupsButton.setTooltip("Open 24 POPulation Groups & Multi-Channel "
                               "Spill Manager (MIDAS Chapter 11)");
    popGroupsButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xffa21caf));
    popGroupsButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Routing, 2);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD \xe2\x80\x94 POPulation Groups & "
                            "Multi-Channel Spill (Chapter 11)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new POPulationGroupsPanel(engine));
      options.content->setSize(960, 640);
      options.launchAsync();
    };
    addAndMakeVisible(popGroupsButton);

    cueListButton.setButtonText("CUE LIST / MSC");
    cueListButton.setTooltip("Open Scene Automation Cue List & MIDI Show "
                             "Control Engine (MIDAS Chapter 34)");
    cueListButton.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff0f766e));
    cueListButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Show, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "NOVA HD \xe2\x80\x94 Scene Automation Cue List & "
                            "MIDI Show Control (Chapter 34)";
      options.dialogBackgroundColour = juce::Colour(0xff20262d);
      options.content.setOwned(new CueListPanel(
          engine,
          [safe = juce::Component::SafePointer<MainComponent>(this)](
              int sceneSlot, const nova::graviton::CueSafes &safes,
              float crossfadeSeconds) {
            return safe != nullptr &&
                   safe->recallCueSceneSlot(sceneSlot, safes, crossfadeSeconds);
          },
          [safe = juce::Component::SafePointer<MainComponent>(this)] {
            if (safe != nullptr)
              safe->cancelSceneFaderCrossfade();
          }));
      options.content->setSize(960, 640);
      options.launchAsync();
    };
    addAndMakeVisible(cueListButton);

    asioBridgeButton.setButtonText("ASIO BRIDGE");
    asioBridgeButton.setTooltip("Open Multichannel USB Audio Class 2.0 ASIO "
                                "Bridge Panel (MIDAS Chapter 26)");
    asioBridgeButton.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff1d4ed8));
    asioBridgeButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::System, 0);
        return;
      }
      juce::DialogWindow::LaunchOptions options;
      options.dialogTitle = "ASIO Bridge — Chapter 26";
      options.dialogBackgroundColour = juce::Colour(0xff0d1117);
      options.useNativeTitleBar = true;
      options.resizable = true;
      options.content.setOwned(new ASIOBridgePanel(engine));
      options.content->setSize(720, 400);
      options.launchAsync();
    };
    addAndMakeVisible(asioBridgeButton);

    crossfadeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfadeSlider.setRange(0.0, 10.0, 0.1);
    crossfadeSlider.setValue(0.0, juce::dontSendNotification);
    crossfadeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    crossfadeSlider.setTextValueSuffix(" s FADE");
    crossfadeSlider.setTooltip(
        "Scene recall crossfade transition time (0 = instant, up to 10s)");
    addAndMakeVisible(crossfadeSlider);

    saveSession.setButtonText("SAVE");
    saveSession.setTooltip("Save mixer state to a JSON session file");
    saveSession.onClick = [this] { chooseSaveSession(); };
    addAndMakeVisible(saveSession);
    loadSession.setButtonText("LOAD");
    loadSession.setTooltip("Load mixer state from a JSON session file");
    loadSession.onClick = [this] { chooseLoadSession(); };
    addAndMakeVisible(loadSession);

    sceneSlot.addItem("SCENE 1", 1);
    sceneSlot.addItem("SCENE 2", 2);
    sceneSlot.addItem("SCENE 3", 3);
    sceneSlot.addItem("SCENE 4", 4);
    sceneSlot.addItem("SCENE 5", 5);
    sceneSlot.addItem("SCENE 6", 6);
    sceneSlot.addItem("SCENE 7", 7);
    sceneSlot.addItem("SCENE 8", 8);
    sceneSlot.setSelectedId(1, juce::dontSendNotification);
    sceneSlot.setTooltip("Select one of eight quick-recall scene slots");
    addAndMakeVisible(sceneSlot);
    saveSceneButton.setButtonText("SAVE SCENE");
    saveSceneButton.setTooltip(
        "Capture current mixer state into selected scene slot");
    saveSceneButton.onClick = [this] { saveScene(); };
    addAndMakeVisible(saveSceneButton);
    recallSceneButton.setButtonText("RECALL");
    recallSceneButton.setTooltip("Recall selected scene slot");
    recallSceneButton.onClick = [this] { recallScene(); };
    addAndMakeVisible(recallSceneButton);
    saveShowButton.setButtonText("SAVE SHOW");
    saveShowButton.setTooltip(
        "Export all eight scene slots to a portable NOVA HD show file");
    saveShowButton.onClick = [this] { chooseShowFile(true); };
    addAndMakeVisible(saveShowButton);
    loadShowButton.setButtonText("LOAD SHOW");
    loadShowButton.setTooltip(
        "Import a NOVA HD show file containing scene slots");
    loadShowButton.onClick = [this] { chooseShowFile(false); };
    addAndMakeVisible(loadShowButton);

    midiTarget.addItem("CH 1 GAIN", 1);
    midiTarget.addItem("CH 2 GAIN", 2);
    midiTarget.addItem("CH 3 GAIN", 3);
    midiTarget.addItem("CH 4 GAIN", 4);
    midiTarget.addItem("CH 5 GAIN", 5);
    midiTarget.addItem("CH 6 GAIN", 6);
    midiTarget.addItem("CH 7 GAIN", 7);
    midiTarget.addItem("CH 8 GAIN", 8);
    for (int c = 1; c <= 8; ++c)
      midiTarget.addItem("CH " + juce::String(c) + " MUTE", 10 + c);
    midiTarget.addItem("MAIN MASTER", 30);
    for (int a = 1; a <= kAuxBusCapacity; ++a)
      midiTarget.addItem("AUX " + juce::String(a) + " MASTER", 1000 + a - 1);
    for (int v = 1; v <= 24; ++v)
      midiTarget.addItem("VCA " + juce::String(v) + " MASTER", 50 + v);
    midiTarget.setSelectedId(1, juce::dontSendNotification);
    midiTarget.setTooltip(
        "Select parameter, then press LEARN and move one MIDI controller");
    addAndMakeVisible(midiTarget);
    midiLearnButton.setButtonText("LEARN CC");
    midiLearnButton.setTooltip("Wait for the next incoming MIDI CC message and "
                               "map it to the selected target");
    midiLearnButton.onClick = [this] {
      midiCallback.armLearn(midiTarget.getSelectedId());
      status.setText("MIDI LEARN ARMED — move one control",
                     juce::dontSendNotification);
    };
    addAndMakeVisible(midiLearnButton);

    recordButton.setButtonText("REC WAV");
    recordButton.setTooltip("Record the stereo Main output to a WAV file");
    recordButton.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xffd64d5b));
    recordButton.onClick = [this] {
      if (midasSurface) {
        midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Show, 1);
        return;
      }
      chooseRecordFile();
    };
    addAndMakeVisible(recordButton);
    stopRecordButton.setButtonText("STOP REC");
    stopRecordButton.setEnabled(false);
    stopRecordButton.onClick = [this] { stopRecording(); };
    addAndMakeVisible(stopRecordButton);

    loadWavButton.setButtonText("LOAD WAV");
    loadWavButton.setTooltip(
        "Load a WAV or other supported audio file for Main playback");
    loadWavButton.onClick = [this] { choosePlaybackFile(); };
    addAndMakeVisible(loadWavButton);
    playWavButton.setButtonText("PLAY");
    playWavButton.onClick = [this] { engine.playPlayback(); };
    addAndMakeVisible(playWavButton);
    stopWavButton.setButtonText("STOP PLAY");
    stopWavButton.onClick = [this] { engine.stopPlayback(); };
    addAndMakeVisible(stopWavButton);

    pageSize.addItem("8 FADERS", 8);
    pageSize.addItem("16 FADERS", 16);
    pageSize.addItem("24 FADERS", 24);
    pageSize.setSelectedId(8, juce::dontSendNotification);

    faderLayoutSelector.addItem("STANDARD LAYOUT", 1);
    for (int layout = 0; layout < 8; ++layout)
      faderLayoutSelector.addItem("USER " + juce::String(layout + 1),
                                  layout + 2);
    faderLayoutSelector.setSelectedId(1, juce::dontSendNotification);
    faderLayoutSelector.setTooltip(
        "Select standard channel order or one of eight custom fader layouts");
    faderLayoutSelector.onChange = [this] {
      activeFaderLayout = faderLayoutSelector.getSelectedId() - 2;
      currentBank = 0;
      updateBankVisibility();
      resized();
      persistFaderLayouts();
    };
    addAndMakeVisible(faderLayoutSelector);

    editFaderLayoutButton.setButtonText("EDIT LAYOUT");
    editFaderLayoutButton.setTooltip(
        "Create custom channel orders for the fader banks");
    editFaderLayoutButton.onClick = [this] { openFaderLayoutEditor(); };
    addAndMakeVisible(editFaderLayoutButton);

    faderModeSelector.addItem("MAIN FADERS", 1);
    for (int a = 1; a <= kAuxBusCapacity; ++a)
      faderModeSelector.addItem("AUX " + juce::String(a) + " SENDS", a + 1);
    faderModeSelector.setSelectedId(1, juce::dontSendNotification);
    faderModeSelector.setTooltip(
        "Sends on Faders: choose Main or any of the 96 AUX send buses");
    faderModeSelector.onChange = [this] {
      setSofMode(faderModeSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(faderModeSelector);

    exitSofButton.setButtonText("◀ EXIT SOF");
    exitSofButton.setTooltip(
        "Exit Sends on Faders and return to Main Mix (Esc)");
    exitSofButton.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xffe6a218));
    exitSofButton.setColour(juce::TextButton::textColourOffId,
                            juce::Colour(0xff12161a));
    exitSofButton.onClick = [this] { setSofMode(0); };
    exitSofButton.setVisible(false);
    addAndMakeVisible(exitSofButton);

    exitSpillButton.setButtonText("◀ EXIT SPILL");
    exitSpillButton.setTooltip(
        "Exit VCA Spill and return to normal fader banking (Esc)");
    exitSpillButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff00b4d8));
    exitSpillButton.setColour(juce::TextButton::textColourOffId,
                              juce::Colour(0xff12161a));
    exitSpillButton.onClick = [this] { exitSpill(); };
    exitSpillButton.setVisible(false);
    addAndMakeVisible(exitSpillButton);

    for (int g = 0; g < 8; ++g) {
      muteGroupButtons[static_cast<size_t>(g)] =
          std::make_unique<juce::TextButton>("MG " + juce::String(g + 1));
      auto &btn = *muteGroupButtons[static_cast<size_t>(g)];
      btn.setTooltip("Global Mute Group " + juce::String(g + 1) + " master");
      btn.onClick = [this, g] {
        const bool next = !engine.muteGroupMasters[static_cast<size_t>(g)].load(
            std::memory_order_relaxed);
        engine.muteGroupMasters[static_cast<size_t>(g)].store(
            next, std::memory_order_relaxed);
        updateMuteGroupButton(static_cast<size_t>(g));
      };
      addAndMakeVisible(btn);
      updateMuteGroupButton(static_cast<size_t>(g));
    }

    for (int a = 0; a < kVisibleAuxSends; ++a) {
      auxSofButtons[static_cast<size_t>(a)] =
          std::make_unique<juce::TextButton>("SOF");
      auto &btn = *auxSofButtons[static_cast<size_t>(a)];
      btn.setTooltip("Sends on Faders for AUX " + juce::String(a + 1));
      btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3540));
      btn.setColour(juce::TextButton::textColourOffId,
                    juce::Colour(0xfff0f3f6));
      btn.onClick = [this, a] {
        const int current = faderMode.load(std::memory_order_relaxed);
        setSofMode(current == (a + 1) ? 0 : (a + 1));
      };
      addAndMakeVisible(btn);
    }
    pageSize.onChange = [this] { setPageSize(pageSize.getSelectedId()); };
    addAndMakeVisible(pageSize);

    previousBank.setButtonText("◀ BANK");
    previousBank.onClick = [this] { changeBank(-1); };
    addAndMakeVisible(previousBank);
    nextBank.setButtonText("BANK ▶");
    nextBank.onClick = [this] { changeBank(1); };
    addAndMakeVisible(nextBank);
    bankLabel.setJustificationType(juce::Justification::centred);
    bankLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(bankLabel);

    for (int i = 0; i < kLogicalChannels; ++i) {
      strips[static_cast<size_t>(i)] = std::make_unique<ChannelStrip>(
          i, engine.channels[static_cast<size_t>(i)], faderMode, engine);
      addChildComponent(*strips[static_cast<size_t>(i)]);
    }
    loadPersistentFaderLayouts();
    updateBankVisibility();

    masterLabel.setText("STEREO MASTER", juce::dontSendNotification);
    masterLabel.setJustificationType(juce::Justification::centred);
    masterLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(masterLabel);

    master.setSliderStyle(juce::Slider::LinearVertical);
    master.setRange(-60.0, 10.0, 0.1);
    master.setValue(0.0);
    master.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 22);
    master.setTextValueSuffix(" dB");
    master.setColour(juce::Slider::trackColourId, juce::Colour(0xff56a8ff));
    master.onValueChange = [this] {
      engine.masterDb.store((float)master.getValue());
    };
    addAndMakeVisible(master);
    for (int bank = 0; bank < kAuxBusCapacity / kVisibleAuxSends; ++bank)
      auxMasterBankSelector.addItem(
          "AUX " + juce::String(bank * kVisibleAuxSends + 1) + "-" +
              juce::String((bank + 1) * kVisibleAuxSends),
          bank + 1);
    auxMasterBankSelector.setSelectedId(1, juce::dontSendNotification);
    auxMasterBankSelector.onChange = [this] { syncAuxMasterControls(); };
    addAndMakeVisible(auxMasterBankSelector);
    addAndMakeVisible(masterMeterL);
    addAndMakeVisible(masterMeterR);
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      auxMeters[static_cast<size_t>(a)] =
          std::make_unique<Meter>(engine.auxPeak[static_cast<size_t>(a)]);
      addAndMakeVisible(*auxMeters[static_cast<size_t>(a)]);
    }
    for (int m = 0; m < kVisibleMatrixBuses; ++m) {
      matrixMeters[static_cast<size_t>(m)] =
          std::make_unique<Meter>(engine.matrixPeak[static_cast<size_t>(m)]);
      addAndMakeVisible(*matrixMeters[static_cast<size_t>(m)]);
    }
    for (int a = 0; a < kVisibleAuxSends; ++a)
      createAuxBusControls(a);
    for (int m = 0; m < kVisibleMatrixBuses; ++m)
      createBusControls(kVisibleAuxSends + m, "M" + juce::String(m + 1),
                        engine.matrixMasterDb[static_cast<size_t>(m)],
                        engine.matrixMuted[static_cast<size_t>(m)]);

    // Hide all legacy MainComponent child widgets so only MidasTouringConsoleSurface is rendered
    for (auto &strip : strips) {
      if (strip != nullptr)
        strip->stopStripTimer();
    }
    for (int i = 0; i < getNumChildComponents(); ++i) {
      if (auto *child = getChildComponent(i))
        child->setVisible(false);
    }

    midasSurface = std::make_unique<MidasTouringConsoleSurface>(
        engine, deviceManager, midiCallback, secondaryAudioStreamer);
    midasSurface->setOpaque(true);
    addAndMakeVisible(*midasSurface);
    setSize(1380, 820);
    midasSurface->setBounds(getLocalBounds());

    midasSurface->onChannelSelected = [this](int chIdx) {
      activeSelectedChannel = juce::jlimit(0, kLogicalChannels - 1, chIdx);
    };
    midasSurface->onOpenFxRack = [this] {
      if (fxRackButton.onClick)
        fxRackButton.onClick();
    };
    midasSurface->onOpenAudioSettings = [this] {
      if (settings.onClick)
        settings.onClick();
    };
    midasSurface->onOpenDN360 = [this] {
      if (geqButton.onClick)
        geqButton.onClick();
    };
    midasSurface->onOpenDugan = [this] {
      if (duganButton.onClick)
        duganButton.onClick();
    };
    midasSurface->onOpenPatchMatrix = [this] {
      if (stageBoxPatchButton.onClick)
        stageBoxPatchButton.onClick();
    };
    midasSurface->onOpenOfflineBounce = [this] {
      if (offlineBounceButton.onClick)
        offlineBounceButton.onClick();
    };
    midasSurface->onOpenCueList = [this] {
      if (cueListButton.onClick)
        cueListButton.onClick();
    };
    midasSurface->onOpenRecording = [this] {
      if (dawStreamButton.onClick)
        dawStreamButton.onClick();
    };
    midasSurface->onOpenDynEq = [this] {
      if (dynEqWindowButton.onClick)
        dynEqWindowButton.onClick();
    };
    midasSurface->setCueListCallbacks(
        [safe = juce::Component::SafePointer<MainComponent>(this)](
            int sceneSlot, const nova::graviton::CueSafes &safes,
            float crossfadeSeconds) {
          return safe != nullptr &&
                 safe->recallCueSceneSlot(sceneSlot, safes, crossfadeSeconds);
        },
        [safe = juce::Component::SafePointer<MainComponent>(this)] {
          if (safe != nullptr)
            safe->cancelSceneFaderCrossfade();
        });

    midasSurface->onOpenChannelAi = [this] {
      if (midasSurface)
        midasSurface->openChannelAiModal();
    };
    midasSurface->onOpenDynamics = [this] {
      if (dynamicsWindowButton.onClick)
        dynamicsWindowButton.onClick();
    };
    midasSurface->onOpenSurroundPan = [this] {
      if (surroundPanButton.onClick)
        surroundPanButton.onClick();
    };
    midasSurface->onOpenManchino = [this] {
      if (manchinoButton.onClick)
        manchinoButton.onClick();
    };
    midasSurface->onOpenVcaPanel = [this] {
      if (vcaWindowButton.onClick)
        vcaWindowButton.onClick();
    };
    midasSurface->onOpenPopGroups = [this] {
      if (popGroupsButton.onClick)
        popGroupsButton.onClick();
    };
    midasSurface->onOpenMatrixGrid = [this] {
      if (matrixGridButton.onClick)
        matrixGridButton.onClick();
    };
    midasSurface->onOpenTalkback = [this] {
      if (talkbackButton.onClick)
        talkbackButton.onClick();
    };
    midasSurface->onOpenMonitorCr = [this] {
      if (monitorButton.onClick)
        monitorButton.onClick();
    };
    midasSurface->onLoadWav = [this] {
      if (loadWavButton.onClick)
        loadWavButton.onClick();
    };
    midasSurface->onOpenAsioBridge = [this] {
      if (asioBridgeButton.onClick)
        asioBridgeButton.onClick();
    };
    midasSurface->onOpenOscServer = [this] {
      if (oscButton.onClick)
        oscButton.onClick();
    };
    midasSurface->onOpenHardwareFxEditor = [this](int editorType) {
      if (midasSurface != nullptr) {
        if (editorType == 0)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 1);
        else if (editorType == 1)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 2);
        else if (editorType == 2)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 3);
        else if (editorType == 4)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 4);
        else if (editorType == 3)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 5);
        else if (editorType == 5)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 6);
        else if (editorType == 6 || editorType == 7)
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 7);
        else
          midasSurface->selectNav(MidasTouringConsoleSurface::MainNavGroup::Fx, 0);
      }
    };

    loadMidiMappings();
    loadPersistentScenes();
    startTimerHz(10);
  }

  ~MainComponent() override { setLookAndFeel(nullptr); }

  void resized() override {
    if (midasSurface != nullptr) {
      midasSurface->setBounds(getLocalBounds());
      midasSurface->toFront(false);
      return;
    }
    auto area = getLocalBounds().reduced(12);

    // Row 1: Console Header & Channel Processing Modules
    auto topRow1 = area.removeFromTop(32);
    title.setBounds(topRow1.removeFromLeft(290));
    settings.setBounds(topRow1.removeFromRight(95).reduced(2));
    offlineBounceButton.setBounds(topRow1.removeFromRight(80).reduced(2));
    manchinoButton.setBounds(topRow1.removeFromRight(82).reduced(2));
    fxRackButton.setBounds(topRow1.removeFromRight(76).reduced(2));
    duganButton.setBounds(topRow1.removeFromRight(68).reduced(2));
    surroundPanButton.setBounds(topRow1.removeFromRight(82).reduced(2));
    geqButton.setBounds(topRow1.removeFromRight(85).reduced(2));
    dynamicsWindowButton.setBounds(topRow1.removeFromRight(76).reduced(2));
    dynEqWindowButton.setBounds(topRow1.removeFromRight(72).reduced(2));
    eqWindowButton.setBounds(topRow1.removeFromRight(56).reduced(2));
    popGroupsButton.setBounds(topRow1.removeFromRight(88).reduced(2));
    vcaWindowButton.setBounds(topRow1.removeFromRight(88).reduced(2));
    channelAiButton.setBounds(topRow1.removeFromRight(88).reduced(2));
    selectedChannelButton.setBounds(topRow1.removeFromRight(86).reduced(2));

    area.removeFromTop(4);

    // Row 2: Live Routing, I/O, DAW, Cue Automation, Monitoring & Transport
    auto topRow2 = area.removeFromTop(30);
    status.setBounds(topRow2.removeFromLeft(260));
    stopRecordButton.setBounds(topRow2.removeFromRight(62).reduced(2));
    recordButton.setBounds(topRow2.removeFromRight(56).reduced(2));
    stopWavButton.setBounds(topRow2.removeFromRight(56).reduced(2));
    playWavButton.setBounds(topRow2.removeFromRight(48).reduced(2));
    loadWavButton.setBounds(topRow2.removeFromRight(65).reduced(2));
    loadSession.setBounds(topRow2.removeFromRight(50).reduced(2));
    saveSession.setBounds(topRow2.removeFromRight(50).reduced(2));
    oscillatorButton.setBounds(topRow2.removeFromRight(78).reduced(2));
    oscButton.setBounds(topRow2.removeFromRight(72).reduced(2));
    talkbackButton.setBounds(topRow2.removeFromRight(78).reduced(2));
    monitorButton.setBounds(topRow2.removeFromRight(82).reduced(2));
    cueListButton.setBounds(topRow2.removeFromRight(95).reduced(2));
    asioBridgeButton.setBounds(topRow2.removeFromRight(85).reduced(2));
    dawStreamButton.setBounds(topRow2.removeFromRight(90).reduced(2));
    matrixGridButton.setBounds(topRow2.removeFromRight(86).reduced(2));
    stageBoxPatchButton.setBounds(topRow2.removeFromRight(80).reduced(2));

    area.removeFromTop(4);
    auto bankBar = area.removeFromTop(36);
    previousBank.setBounds(bankBar.removeFromLeft(100).reduced(2));
    faderLayoutSelector.setBounds(bankBar.removeFromLeft(145).reduced(2));
    editFaderLayoutButton.setBounds(bankBar.removeFromLeft(86).reduced(2));
    nextBank.setBounds(bankBar.removeFromRight(100).reduced(2));
    pageSize.setBounds(bankBar.removeFromRight(110).reduced(2));
    diagnosticsButton.setBounds(bankBar.removeFromRight(100).reduced(2));
    faderModeSelector.setBounds(bankBar.removeFromRight(135).reduced(2));
    if (engine.activeSpillVca.load(std::memory_order_relaxed) > 0 ||
        engine.activeSpillPop.load(std::memory_order_relaxed) > 0)
      exitSpillButton.setBounds(bankBar.removeFromRight(120).reduced(2));
    else
      exitSpillButton.setBounds(0, 0, 0, 0);
    if (faderMode.load(std::memory_order_relaxed) > 0)
      exitSofButton.setBounds(bankBar.removeFromRight(110).reduced(2));
    else
      exitSofButton.setBounds(0, 0, 0, 0);
    midiLearnButton.setBounds(bankBar.removeFromRight(78).reduced(2));
    midiTarget.setBounds(bankBar.removeFromRight(120).reduced(2));
    loadShowButton.setBounds(bankBar.removeFromRight(86).reduced(2));
    saveShowButton.setBounds(bankBar.removeFromRight(86).reduced(2));
    recallSceneButton.setBounds(bankBar.removeFromRight(70).reduced(2));
    saveSceneButton.setBounds(bankBar.removeFromRight(90).reduced(2));
    sceneSlot.setBounds(bankBar.removeFromRight(95).reduced(2));
    crossfadeSlider.setBounds(bankBar.removeFromRight(105).reduced(2));
    bankLabel.setBounds(bankBar.reduced(2));
    area.removeFromTop(8);
    auto masterArea = area.removeFromRight(190);
    area.removeFromRight(10);
    const int gap = 8;
    const bool isSpilling =
        (engine.activeSpillVca.load(std::memory_order_relaxed) > 0 ||
         engine.activeSpillPop.load(std::memory_order_relaxed) > 0);
    faderLayoutSelector.setEnabled(!isSpilling);
    editFaderLayoutButton.setEnabled(!isSpilling);
    if (isSpilling) {
      const auto members = getSpillMembers();
      const int totalMembers = static_cast<int>(members.size());
      const int visibleCount =
          juce::jmin(pageCount, totalMembers - (currentBank * pageCount));
      if (visibleCount > 0) {
        const int stripWidth = juce::jmax(
            1, (area.getWidth() - gap * juce::jmax(0, visibleCount - 1)) /
                   juce::jmax(1, visibleCount));
        for (int slot = 0; slot < visibleCount; ++slot) {
          const int chIdx =
              members[static_cast<size_t>(currentBank * pageCount + slot)];
          auto r = area.removeFromLeft(stripWidth);
          strips[static_cast<size_t>(chIdx)]->setBounds(r);
          if (slot < visibleCount - 1)
            area.removeFromLeft(gap);
        }
      }
    } else {
      const int totalChannels = normalChannelCount();
      const int visibleCount = juce::jmax(
          0, juce::jmin(pageCount, totalChannels - currentBank * pageCount));
      const int stripWidth = juce::jmax(
          1, (area.getWidth() - gap * juce::jmax(0, visibleCount - 1)) /
                 juce::jmax(1, visibleCount));
      for (int slot = 0; slot < visibleCount; ++slot) {
        const int channelIndex =
            normalChannelAt(currentBank * pageCount + slot);
        if (channelIndex < 0 || channelIndex >= kLogicalChannels)
          continue;
        auto r = area.removeFromLeft(stripWidth);
        strips[static_cast<size_t>(channelIndex)]->setBounds(r);
        if (slot < visibleCount - 1)
          area.removeFromLeft(gap);
      }
    }
    masterLabel.setBounds(masterArea.removeFromTop(25));
    auto meters = masterArea.removeFromTop(68);
    masterMeterL.setBounds(meters.removeFromLeft(18).reduced(1, 0));
    meters.removeFromLeft(5);
    masterMeterR.setBounds(meters.removeFromLeft(18).reduced(1, 0));
    meters.removeFromLeft(8);
    auto readouts = meters;
    mainReadoutL.setBounds(readouts.removeFromTop(22));
    mainReadoutR.setBounds(readouts.removeFromTop(22));
    mainClipReadout.setBounds(readouts.removeFromTop(22));
    clearClipButton.setBounds(masterArea.removeFromTop(25).reduced(0, 1));
    masterArea.removeFromTop(4);
    auxMasterBankSelector.setBounds(masterArea.removeFromTop(26).reduced(0, 2));
    syncAuxMasterControls();
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      auto row = masterArea.removeFromTop(35);
      auxMeters[static_cast<size_t>(a)]->setBounds(
          row.removeFromLeft(12).reduced(0, 2));
      row.removeFromLeft(3);
      busMaster[static_cast<size_t>(a)]->setBounds(
          row.removeFromLeft(48).reduced(0, 2));
      busMute[static_cast<size_t>(a)]->setBounds(
          row.removeFromLeft(28).reduced(0, 2));
      if (auxSofButtons[static_cast<size_t>(a)] != nullptr)
        auxSofButtons[static_cast<size_t>(a)]->setBounds(
            row.removeFromLeft(34).reduced(0, 2));
      busReadout[static_cast<size_t>(a)]->setBounds(row.reduced(1, 2));
    }
    for (int m = 0; m < kVisibleMatrixBuses; ++m) {
      auto row = masterArea.removeFromTop(35);
      matrixMeters[static_cast<size_t>(m)]->setBounds(
          row.removeFromLeft(12).reduced(0, 2));
      row.removeFromLeft(3);
      const int idx = kVisibleAuxSends + m;
      busMaster[static_cast<size_t>(idx)]->setBounds(
          row.removeFromLeft(62).reduced(0, 2));
      busMute[static_cast<size_t>(idx)]->setBounds(
          row.removeFromLeft(34).reduced(0, 2));
      busReadout[static_cast<size_t>(idx)]->setBounds(row.reduced(1, 2));
    }
    masterArea.removeFromTop(4);
    auto mgArea = masterArea.removeFromTop(48);
    auto mgRow1 = mgArea.removeFromTop(23);
    auto mgRow2 = mgArea;
    const int mgW = mgRow1.getWidth() / 4;
    for (int g = 0; g < 4; ++g) {
      if (muteGroupButtons[static_cast<size_t>(g)] != nullptr)
        muteGroupButtons[static_cast<size_t>(g)]->setBounds(
            mgRow1.removeFromLeft(mgW).reduced(1));
      if (muteGroupButtons[static_cast<size_t>(g + 4)] != nullptr)
        muteGroupButtons[static_cast<size_t>(g + 4)]->setBounds(
            mgRow2.removeFromLeft(mgW).reduced(1));
    }
    masterArea.removeFromTop(4);
    master.setBounds(masterArea.reduced(5, 0));
  }

private:
  int normalChannelCount() const noexcept {
    const bool isCustom =
        activeFaderLayout >= 0 &&
        activeFaderLayout < static_cast<int>(customFaderLayouts.size());
    static const std::vector<int> emptyLayout;
    const auto &channels =
        isCustom ? customFaderLayouts[static_cast<size_t>(activeFaderLayout)]
                       .channels
                 : emptyLayout;
    return nova::faderLayoutChannelCount(channels, isCustom, kLogicalChannels);
  }

  int normalChannelAt(int position) const noexcept {
    const bool isCustom =
        activeFaderLayout >= 0 &&
        activeFaderLayout < static_cast<int>(customFaderLayouts.size());
    static const std::vector<int> emptyLayout;
    const auto &channels =
        isCustom ? customFaderLayouts[static_cast<size_t>(activeFaderLayout)]
                       .channels
                 : emptyLayout;
    return nova::faderLayoutChannelAt(channels, isCustom, position,
                                      kLogicalChannels);
  }

  juce::File faderLayoutFile() const {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
        .getChildFile("NOVA_HD")
        .getChildFile("fader-layouts.json");
  }

  void refreshFaderLayoutSelector() {
    const int selectedId = activeFaderLayout + 2;
    faderLayoutSelector.clear(juce::dontSendNotification);
    faderLayoutSelector.addItem("STANDARD LAYOUT", 1);
    for (size_t index = 0; index < customFaderLayouts.size(); ++index) {
      const auto &layout = customFaderLayouts[index];
      const auto label = layout.name.isNotEmpty()
                             ? layout.name
                             : "USER " + juce::String(index + 1);
      faderLayoutSelector.addItem(label, static_cast<int>(index + 2));
    }
    faderLayoutSelector.setSelectedId(
        juce::jlimit(1, static_cast<int>(customFaderLayouts.size()) + 1,
                     selectedId),
        juce::dontSendNotification);
  }

  void openFaderLayoutEditor() {
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "NOVA HD — Custom Fader Layouts";
    options.dialogBackgroundColour = juce::Colour(0xff20262d);
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.content.setOwned(new UserFaderLayoutEditor(
        customFaderLayouts,
        [safe = juce::Component::SafePointer<MainComponent>(this)](
            const UserFaderLayoutEditor::Layouts &updatedLayouts) {
          if (safe == nullptr)
            return;
          safe->customFaderLayouts = updatedLayouts;
          safe->refreshFaderLayoutSelector();
          safe->updateBankVisibility();
          safe->resized();
          safe->persistFaderLayouts();
        }));
    options.content->setSize(520, 250);
    options.launchAsync();
  }

  bool persistFaderLayouts() {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("format", "NOVA_HD_FADER_LAYOUTS");
    root->setProperty("version", 1);
    root->setProperty("activeLayout", activeFaderLayout);
    juce::Array<juce::var> layoutList;
    for (const auto &layout : customFaderLayouts) {
      auto *item = new juce::DynamicObject();
      item->setProperty("name", layout.name);
      juce::Array<juce::var> channels;
      for (const int channel : layout.channels)
        channels.add(channel);
      item->setProperty("channels", channels);
      layoutList.add(juce::var(item));
    }
    root->setProperty("layouts", layoutList);
    const auto file = faderLayoutFile();
    const auto parent = file.getParentDirectory();
    if ((!parent.createDirectory() && !parent.isDirectory()) ||
        !file.replaceWithText(
            juce::JSON::toString(juce::var(root.get()), true))) {
      status.setText("FADER LAYOUT SAVE FAILED", juce::dontSendNotification);
      return false;
    }
    return true;
  }

  bool decodeFaderLayouts(const juce::var &layoutValue,
                          UserFaderLayoutEditor::Layouts &destination) const {
    const auto *list = layoutValue.getArray();
    if (list == nullptr || list->size() != static_cast<int>(destination.size()))
      return false;
    UserFaderLayoutEditor::Layouts decoded;
    for (size_t index = 0; index < decoded.size(); ++index) {
      const auto *item = (*list)[static_cast<int>(index)].getDynamicObject();
      if (item == nullptr)
        return false;
      decoded[index].name =
          item->getProperty("name").toString().trim().substring(0, 32);
      const auto channelListVar = item->getProperty("channels");
      const auto *channels = channelListVar.getArray();
      if (channels == nullptr)
        return false;
      for (const auto &value : *channels) {
        const double channelNumber = static_cast<double>(value);
        if ((!value.isInt() && !value.isInt64() && !value.isDouble()) ||
            !std::isfinite(channelNumber) ||
            std::floor(channelNumber) != channelNumber || channelNumber < 0.0 ||
            channelNumber >= kLogicalChannels)
          return false;
        const int channel = static_cast<int>(channelNumber);
        decoded[index].channels.push_back(channel);
      }
      if (!nova::isValidFaderLayout(decoded[index].channels, kLogicalChannels))
        return false;
    }
    destination = std::move(decoded);
    return true;
  }

  void loadPersistentFaderLayouts() {
    const auto file = faderLayoutFile();
    if (!file.existsAsFile())
      return;
    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    const auto *root = parsed.getDynamicObject();
    if (root == nullptr ||
        root->getProperty("format").toString() != "NOVA_HD_FADER_LAYOUTS") {
      status.setText("FADER LAYOUT FILE INVALID; DEFAULTS LOADED",
                     juce::dontSendNotification);
      return;
    }
    UserFaderLayoutEditor::Layouts loaded;
    if (!decodeFaderLayouts(root->getProperty("layouts"), loaded)) {
      status.setText("FADER LAYOUT FILE INVALID; DEFAULTS LOADED",
                     juce::dontSendNotification);
      return;
    }
    customFaderLayouts = std::move(loaded);
    activeFaderLayout =
        juce::jlimit(-1, static_cast<int>(customFaderLayouts.size()) - 1,
                     static_cast<int>(root->getProperty("activeLayout")));
    refreshFaderLayoutSelector();
  }

  juce::File midiMappingFile() const {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
        .getChildFile("NOVA_HD")
        .getChildFile("midi-map.json");
  }

  void loadMidiMappings() {
    const auto file = midiMappingFile();
    if (!file.existsAsFile())
      return;
    const auto parsed = juce::JSON::parse(file);
    midiCallback.loadMappings(parsed);
  }

  void saveMidiMappings() {
    const auto file = midiMappingFile();
    if (!file.getParentDirectory().createDirectory() &&
        !file.getParentDirectory().isDirectory()) {
      status.setText("MIDI MAP SAVE FAILED: cannot create folder",
                     juce::dontSendNotification);
      return;
    }
    const auto json = juce::JSON::toString(midiCallback.exportMappings(), true);
    if (!file.replaceWithText(json))
      status.setText("MIDI MAP SAVE FAILED", juce::dontSendNotification);
  }

  void saveScene() {
    const int slot = sceneSlot.getSelectedId() - 1;
    if (slot < 0 || slot >= static_cast<int>(sceneData.size()))
      return;
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("NOVA_HD_scene_capture.json");
    saveSessionToFile(temp);
    const auto json = temp.loadFileAsString();
    if (json.isNotEmpty() && json.contains("NOVA_HD_SESSION")) {
      sceneData[static_cast<size_t>(slot)] = json;
      persistScenes();
      status.setText("SCENE " + juce::String(slot + 1) + " SAVED",
                     juce::dontSendNotification);
    } else
      status.setText("SCENE SAVE FAILED", juce::dontSendNotification);
  }

  juce::File getSceneStoreFile() const {
    auto folder =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("NOVA_HD");
    folder.createDirectory();
    return folder.getChildFile("scene-slots.json");
  }

  juce::var makeSceneDocument() const {
    auto *obj = new juce::DynamicObject();
    obj->setProperty("format", "NOVA_HD_SHOW");
    obj->setProperty("version", 1);
    juce::Array<juce::var> scenes;
    for (int i = 0; i < static_cast<int>(sceneData.size()); ++i)
      scenes.add(sceneData[static_cast<size_t>(i)]);
    obj->setProperty("scenes", scenes);
    return juce::var(obj);
  }

  bool importSceneDocument(const juce::var &doc) {
    if (!doc.isObject())
      return false;
    auto *obj = doc.getDynamicObject();
    if (obj == nullptr ||
        obj->getProperty("format").toString() != "NOVA_HD_SHOW")
      return false;
    const auto scenes = obj->getProperty("scenes");
    if (!scenes.isArray() ||
        scenes.getArray()->size() != static_cast<int>(sceneData.size()))
      return false;
    for (int i = 0; i < static_cast<int>(sceneData.size()); ++i) {
      const auto value = scenes.getArray()->getReference(i).toString();
      if (value.isNotEmpty() && !value.contains("NOVA_HD_SESSION"))
        return false;
    }
    for (int i = 0; i < static_cast<int>(sceneData.size()); ++i)
      sceneData[static_cast<size_t>(i)] =
          scenes.getArray()->getReference(i).toString();
    persistScenes();
    return true;
  }

  void persistScenes() {
    const auto file = getSceneStoreFile();
    if (!file.replaceWithText(juce::JSON::toString(makeSceneDocument(), true)))
      status.setText("SCENE AUTOSAVE FAILED", juce::dontSendNotification);
  }

  void loadPersistentScenes() {
    const auto file = getSceneStoreFile();
    if (!file.existsAsFile())
      return;
    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (importSceneDocument(parsed))
      status.setText("SCENE SLOTS RESTORED", juce::dontSendNotification);
    else
      status.setText("SAVED SCENE FILE INVALID; IGNORED",
                     juce::dontSendNotification);
  }

  void chooseShowFile(bool exporting) {
    auto chooser = std::make_shared<juce::FileChooser>(
        exporting ? "Save NOVA HD Show File" : "Load NOVA HD Show File",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.novashow;*.json");
    const auto flags = exporting ? (juce::FileBrowserComponent::saveMode |
                                    juce::FileBrowserComponent::canSelectFiles)
                                 : (juce::FileBrowserComponent::openMode |
                                    juce::FileBrowserComponent::canSelectFiles);
    chooser->launchAsync(
        flags, [safe = juce::Component::SafePointer<MainComponent>(this),
                chooser, exporting](const juce::FileChooser &fc) {
          if (safe == nullptr || fc.getResult().getFullPathName().isEmpty())
            return;
          auto file = fc.getResult();
          if (exporting) {
            if (file.getFileExtension().isEmpty())
              file = file.withFileExtension(".novashow");
            const bool ok = file.replaceWithText(
                juce::JSON::toString(safe->makeSceneDocument(), true));
            safe->status.setText(ok ? "SHOW FILE EXPORTED"
                                    : "SHOW EXPORT FAILED",
                                 juce::dontSendNotification);
          } else {
            const auto parsed = juce::JSON::parse(file.loadFileAsString());
            if (safe->importSceneDocument(parsed))
              safe->status.setText("SHOW FILE LOADED",
                                   juce::dontSendNotification);
            else
              safe->status.setText("INVALID SHOW FILE; SCENES UNCHANGED",
                                   juce::dontSendNotification);
          }
        });
  }

  void recallScene() {
    const int slot = sceneSlot.getSelectedId() - 1;
    if (slot < 0 || slot >= static_cast<int>(sceneData.size()))
      return;
    const auto sceneJson = sceneData[static_cast<size_t>(slot)];
    if (sceneJson.isEmpty()) {
      status.setText("SCENE " + juce::String(slot + 1) + " EMPTY",
                     juce::dontSendNotification);
      return;
    }

    auto *dialog =
        new juce::AlertWindow("Safe Scene Recall",
                              "Choose which parameter groups to recall. "
                              "Physical input patch is always protected.",
                              juce::AlertWindow::NoIcon);
    dialog->addComboBox("scope",
                        {"FULL MIX (except input patch)", "MIX ONLY",
                         "PROCESSING ONLY", "SAFE: FADER/PAN/MUTE"},
                        "Recall scope");
    dialog->addButton("RECALL", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(
        true,
        juce::ModalCallbackFunction::create([this, dialog, sceneJson,
                                             slot](int result) {
          if (result == 1) {
            const int scope =
                dialog->getComboBoxComponent("scope")->getSelectedId();
            const double xfSec = crossfadeSlider.getValue();
            if (recallSceneWithScope(sceneJson, scope, {},
                                     static_cast<float>(xfSec))) {
              status.setText(
                  "SCENE " + juce::String(slot + 1) + " RECALLED (scope " +
                      juce::String(scope) +
                      (xfSec > 0.05 ? ", " + juce::String(xfSec, 1) + "s X-FADE"
                                    : "") +
                      ")",
                  juce::dontSendNotification);
            } else
              status.setText("SCENE RECALL FAILED: INVALID DATA",
                             juce::dontSendNotification);
          }
          dialog->exitModalState(result);
          delete dialog;
        }),
        true);
  }

public:
  bool recallSceneWithScope(const juce::String &sceneJson, int scope,
                            const nova::graviton::CueSafes &cueSafes = {},
                            float crossfadeSeconds = 0.0f) {
    const auto sceneVar = juce::JSON::parse(sceneJson);
    auto *sceneRoot = sceneVar.getDynamicObject();
    if (sceneRoot == nullptr ||
        sceneRoot->getProperty("format").toString() != "NOVA_HD_SESSION")
      return false;

    const auto tempCurrent =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("NOVA_HD_scene_current.json");
    saveSessionToFile(tempCurrent);
    const auto currentVar = juce::JSON::parse(tempCurrent.loadFileAsString());
    auto *currentRoot = currentVar.getDynamicObject();
    if (currentRoot == nullptr)
      return false;

    const auto sceneChannelsVar = sceneRoot->getProperty("channels");
    const auto currentChannelsVar = currentRoot->getProperty("channels");
    auto *sceneChannels = sceneChannelsVar.getArray();
    auto *currentChannels = currentChannelsVar.getArray();
    if (sceneChannels == nullptr || currentChannels == nullptr ||
        sceneChannels->size() != kLogicalChannels ||
        currentChannels->size() != kLogicalChannels)
      return false;

    const auto copyProperty = [&cueSafes](juce::DynamicObject *dst,
                                          juce::DynamicObject *src,
                                          const juce::Identifier &key) {
      if (dst != nullptr && src != nullptr && src->hasProperty(key) &&
          !nova::graviton::cueSafeBlocksProperty(cueSafes,
                                                 key.toString().toStdString()))
        dst->setProperty(key, src->getProperty(key));
    };

    for (int c = 0; c < kLogicalChannels; ++c) {
      auto *src = (*sceneChannels)[c].getDynamicObject();
      auto *dst = (*currentChannels)[c].getDynamicObject();
      if (src == nullptr || dst == nullptr)
        continue;

      const bool isMuteSafe = dst->hasProperty("muteSafe") &&
                              static_cast<bool>(dst->getProperty("muteSafe"));
      const bool isSoloSafe = dst->hasProperty("soloSafe") &&
                              static_cast<bool>(dst->getProperty("soloSafe"));

      const auto copyChannelProp = [&](const char *propName) {
        if (nova::graviton::cueSafeBlocksProperty(cueSafes, propName))
          return;
        if (std::strcmp(propName, "muted") == 0 && isMuteSafe)
          return;
        if (std::strcmp(propName, "solo") == 0 && isSoloSafe)
          return;
        copyProperty(dst, src, juce::Identifier(propName));
      };

      // Scope 1 = full channel state except physical input patch.
      if (scope == 1) {
        for (const auto *key : {"vcaGroup",
                                "muteGroupMask",
                                "inputGainDb",
                                "polarityInverted",
                                "inputDelayMs",
                                "postEqDynamics",
                                "gainDb",
                                "pan",
                                "muted",
                                "solo",
                                "soloBus",
                                "soloPfl",
                                "allPassEnabled",
                                "allPassDegrees",
                                "allPassRange",
                                "allPassFreqMode",
                                "highPass",
                                "highPassFreqHz",
                                "highPassSlope",
                                "lowPassEnabled",
                                "lowPassFreqHz",
                                "lowPassSlope",
                                "compressor",
                                "auxSendDb",
                                "auxPreFader",
                                "eqFrequency",
                                "eqGainDb",
                                "eqQ",
                                "dynEqEnabled",
                                "dynEqMode",
                                "dynEqThresholdDb",
                                "dynEqRangeDb",
                                "dynEqRatio",
                                "dynEqKneeDb",
                                "dynEqAttackMs",
                                "dynEqReleaseMs",
                                "gateEnabled",
                                "gateThresholdDb",
                                "gateRatio",
                                "gateAttackMs",
                                "gateReleaseMs",
                                "compressorThresholdDb",
                                "compressorRatio",
                                "compressorAttackMs",
                                "compressorReleaseMs"})
          copyChannelProp(key);
      } else if (scope == 2) // Mix only
      {
        for (const auto *key :
             {"gainDb", "pan", "muted", "solo", "soloBus", "soloPfl",
              "auxSendDb", "auxPreFader", "vcaGroup", "muteGroupMask"})
          copyChannelProp(key);
      } else if (scope == 3) // Processing only
      {
        for (const auto *key : {"inputGainDb",        "polarityInverted",
                                "inputDelayMs",       "postEqDynamics",
                                "allPassEnabled",     "allPassDegrees",
                                "allPassRange",       "allPassFreqMode",
                                "highPass",           "highPassFreqHz",
                                "highPassSlope",      "lowPassEnabled",
                                "lowPassFreqHz",      "lowPassSlope",
                                "compressor",         "eqFrequency",
                                "eqGainDb",           "eqQ",
                                "dynEqEnabled",       "dynEqMode",
                                "dynEqThresholdDb",   "dynEqRangeDb",
                                "dynEqRatio",         "dynEqKneeDb",
                                "dynEqAttackMs",      "dynEqReleaseMs",
                                "gateEnabled",        "gateThresholdDb",
                                "gateRatio",          "gateAttackMs",
                                "gateReleaseMs",      "compressorThresholdDb",
                                "compressorRatio",    "compressorAttackMs",
                                "compressorReleaseMs"})
          copyChannelProp(key);
      } else // Safe scope: fader/pan/mute/solo only
      {
        for (const auto *key : {"gainDb", "pan", "muted", "solo"})
          copyChannelProp(key);
      }
    }

    if (scope == 1) {
      for (const auto *key :
           {"masterDb",         "vcaMasterDb",        "vcaNames",
            "vcaMuted",         "muteGroupMasters",   "auxMasterDb",
            "auxMuted",         "matrixMasterDb",     "matrixMuted",
            "matrixMainLeftDb", "matrixMainRightDb",  "matrixMainMuted",
            "matrixAuxSendDb",  "matrixAuxSendMuted", "oscEnabled",
            "oscType",          "oscFrequency",       "oscLevelDb",
            "oscRouteMain",     "oscRouteAux",        "oscRouteMatrix",
            "stereoLink",       "pageSize",           "bank"})
        copyProperty(currentRoot, sceneRoot, juce::Identifier(key));
    }

    const auto mergedFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("NOVA_HD_scene_merged.json");
    if (!mergedFile.replaceWithText(juce::JSON::toString(currentVar, true)))
      return false;
    std::array<float, kLogicalChannels> channelFaderStart{};
    std::array<float, 24> vcaFaderStart{};
    std::array<float, kAuxBusCapacity> auxFaderStart{};
    std::array<float, kMatrixBusCapacity> matrixFaderStart{};
    for (int c = 0; c < kLogicalChannels; ++c)
      channelFaderStart[static_cast<size_t>(c)] =
          engine.channels[static_cast<size_t>(c)].gainDb.load(
              std::memory_order_relaxed);
    for (size_t v = 0; v < vcaFaderStart.size(); ++v)
      vcaFaderStart[v] = engine.vcaMasterDb[v].load(std::memory_order_relaxed);
    for (size_t a = 0; a < auxFaderStart.size(); ++a)
      auxFaderStart[a] = engine.auxMasterDb[a].load(std::memory_order_relaxed);
    for (size_t m = 0; m < matrixFaderStart.size(); ++m)
      matrixFaderStart[m] =
          engine.matrixMasterDb[m].load(std::memory_order_relaxed);
    const float masterFaderStart =
        engine.masterDb.load(std::memory_order_relaxed);
    loadSessionFromFile(mergedFile);
    if (std::isfinite(crossfadeSeconds) && crossfadeSeconds > 0.05f)
      beginSceneFaderCrossfade(channelFaderStart, vcaFaderStart, auxFaderStart,
                               matrixFaderStart, masterFaderStart,
                               crossfadeSeconds);
    return true;
  }

  bool recallCueSceneSlot(int cueSceneSlot,
                          const nova::graviton::CueSafes &cueSafes,
                          float crossfadeSeconds) {
    if (cueSceneSlot < 0 ||
        cueSceneSlot >= static_cast<int>(sceneData.size())) {
      status.setText("CUE RECALL FAILED: INVALID SCENE SLOT",
                     juce::dontSendNotification);
      return false;
    }
    const auto &sceneJson = sceneData[static_cast<size_t>(cueSceneSlot)];
    if (sceneJson.isEmpty()) {
      status.setText("CUE RECALL FAILED: SCENE " +
                         juce::String(cueSceneSlot + 1) + " IS EMPTY",
                     juce::dontSendNotification);
      return false;
    }
    if (!recallSceneWithScope(sceneJson, 1, cueSafes, crossfadeSeconds)) {
      status.setText("CUE RECALL FAILED: INVALID SCENE DATA",
                     juce::dontSendNotification);
      return false;
    }
    status.setText("CUE SCENE " + juce::String(cueSceneSlot + 1) +
                       (crossfadeSeconds > 0.05f
                            ? " RECALLING WITH FADER CROSSFADE"
                            : " RECALLED"),
                   juce::dontSendNotification);
    return true;
  }

  void advanceSceneFaderCrossfade(float elapsedSeconds) {
    if (!sceneFaderCrossfade.active || !std::isfinite(elapsedSeconds) ||
        elapsedSeconds <= 0.0f)
      return;
    auto &fade = sceneFaderCrossfade;
    fade.elapsedSeconds =
        juce::jmin(fade.durationSeconds, fade.elapsedSeconds + elapsedSeconds);
    const float progress = fade.elapsedSeconds / fade.durationSeconds;
    const auto interpolate = [progress](float from, float to) {
      return from + (to - from) * progress;
    };
    for (size_t c = 0; c < fade.channelFrom.size(); ++c)
      engine.channels[c].gainDb.store(
          interpolate(fade.channelFrom[c], fade.channelTo[c]),
          std::memory_order_relaxed);
    for (size_t v = 0; v < fade.vcaFrom.size(); ++v)
      engine.vcaMasterDb[v].store(interpolate(fade.vcaFrom[v], fade.vcaTo[v]),
                                  std::memory_order_relaxed);
    for (size_t a = 0; a < fade.auxFrom.size(); ++a)
      engine.auxMasterDb[a].store(interpolate(fade.auxFrom[a], fade.auxTo[a]),
                                  std::memory_order_relaxed);
    for (size_t m = 0; m < fade.matrixFrom.size(); ++m)
      engine.matrixMasterDb[m].store(
          interpolate(fade.matrixFrom[m], fade.matrixTo[m]),
          std::memory_order_relaxed);
    engine.masterDb.store(interpolate(fade.masterFrom, fade.masterTo),
                          std::memory_order_relaxed);
    master.setValue(engine.masterDb.load(std::memory_order_relaxed),
                    juce::dontSendNotification);
    syncAuxMasterControls();
    for (int m = 0; m < kVisibleMatrixBuses; ++m)
      busMaster[static_cast<size_t>(kVisibleAuxSends + m)]->setValue(
          engine.matrixMasterDb[static_cast<size_t>(m)].load(
              std::memory_order_relaxed),
          juce::dontSendNotification);
    if (progress >= 1.0f)
      fade.active = false;
  }

  void cancelSceneFaderCrossfade() noexcept {
    sceneFaderCrossfade.active = false;
  }

private:
  void beginSceneFaderCrossfade(
      const std::array<float, kLogicalChannels> &channelStart,
      const std::array<float, 24> &vcaStart,
      const std::array<float, kAuxBusCapacity> &auxStart,
      const std::array<float, kMatrixBusCapacity> &matrixStart,
      float masterStart, float durationSeconds) {
    auto &fade = sceneFaderCrossfade;
    fade.active = false;
    fade.elapsedSeconds = 0.0f;
    fade.durationSeconds = juce::jlimit(0.05f, 10.0f, durationSeconds);
    fade.channelFrom = channelStart;
    fade.vcaFrom = vcaStart;
    fade.auxFrom = auxStart;
    fade.matrixFrom = matrixStart;
    fade.masterFrom = masterStart;
    for (size_t c = 0; c < fade.channelTo.size(); ++c) {
      fade.channelTo[c] =
          engine.channels[c].gainDb.load(std::memory_order_relaxed);
      engine.channels[c].gainDb.store(fade.channelFrom[c],
                                      std::memory_order_relaxed);
    }
    for (size_t v = 0; v < fade.vcaTo.size(); ++v) {
      fade.vcaTo[v] = engine.vcaMasterDb[v].load(std::memory_order_relaxed);
      engine.vcaMasterDb[v].store(fade.vcaFrom[v], std::memory_order_relaxed);
    }
    for (size_t a = 0; a < fade.auxTo.size(); ++a) {
      fade.auxTo[a] = engine.auxMasterDb[a].load(std::memory_order_relaxed);
      engine.auxMasterDb[a].store(fade.auxFrom[a], std::memory_order_relaxed);
    }
    for (size_t m = 0; m < fade.matrixTo.size(); ++m) {
      fade.matrixTo[m] =
          engine.matrixMasterDb[m].load(std::memory_order_relaxed);
      engine.matrixMasterDb[m].store(fade.matrixFrom[m],
                                     std::memory_order_relaxed);
    }
    fade.masterTo = engine.masterDb.load(std::memory_order_relaxed);
    engine.masterDb.store(fade.masterFrom, std::memory_order_relaxed);
    fade.active = true;
    master.setValue(fade.masterFrom, juce::dontSendNotification);
    syncAuxMasterControls();
  }

  struct SceneFaderCrossfade {
    bool active = false;
    float elapsedSeconds = 0.0f;
    float durationSeconds = 0.0f;
    float masterFrom = 0.0f;
    float masterTo = 0.0f;
    std::array<float, kLogicalChannels> channelFrom{};
    std::array<float, kLogicalChannels> channelTo{};
    std::array<float, 24> vcaFrom{};
    std::array<float, 24> vcaTo{};
    std::array<float, kAuxBusCapacity> auxFrom{};
    std::array<float, kAuxBusCapacity> auxTo{};
    std::array<float, kMatrixBusCapacity> matrixFrom{};
    std::array<float, kMatrixBusCapacity> matrixTo{};
  };
  SceneFaderCrossfade sceneFaderCrossfade;

public:
  // Phase 89: the offline bounce renders through the live engine state, so the
  // audio device is closed for the duration of the render and restarted
  // afterwards. Going through AudioDeviceManager keeps its bookkeeping and the
  // engine start/stop notifications correct.
  std::function<void(bool)> bounceAudioGate() {
    return [safe = juce::Component::SafePointer<MainComponent>(this)](
               bool suspend) {
      if (safe == nullptr)
        return;
      if (suspend) {
        if (!safe->audioSuspendedForBounce &&
            safe->engine.running.load(std::memory_order_relaxed)) {
          safe->deviceManager.closeAudioDevice();
          safe->audioSuspendedForBounce = true;
        }
      } else if (safe->audioSuspendedForBounce) {
        safe->audioSuspendedForBounce = false;
        safe->deviceManager.restartLastAudioDevice();
      }
    };
  }

  void choosePlaybackFile() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load audio for NOVA HD Main playback",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.aiff;*.aif;*.flac;*.ogg");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<MainComponent>(this),
         chooser](const juce::FileChooser &fc) {
          if (safe == nullptr || !fc.getResult().existsAsFile())
            return;
          if (safe->engine.loadPlaybackFile(fc.getResult()))
            safe->status.setText("PLAYBACK LOADED: " +
                                     fc.getResult().getFileName(),
                                 juce::dontSendNotification);
          else
            safe->status.setText("PLAYBACK LOAD FAILED",
                                 juce::dontSendNotification);
        });
  }

  void chooseRecordFile() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Record NOVA HD Main to WAV",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("NOVA_HD_Record.wav"),
        "*.wav");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<MainComponent>(this),
         chooser](const juce::FileChooser &fc) {
          if (safe == nullptr || fc.getResult().getFullPathName().isEmpty())
            return;
          const auto file = fc.getResult().withFileExtension(".wav");
          const double sr =
              safe->engine.sampleRate.load(std::memory_order_relaxed);
          if (safe->engine.recorder.startRecording(file, sr)) {
            safe->recordButton.setEnabled(false);
            safe->stopRecordButton.setEnabled(true);
            safe->status.setText("RECORDING: " + file.getFileName(),
                                 juce::dontSendNotification);
          } else {
            safe->status.setText("RECORD FAILED: check audio device / file",
                                 juce::dontSendNotification);
          }
        });
  }

  void stopRecording() {
    engine.recorder.stopRecording();
    recordButton.setEnabled(true);
    stopRecordButton.setEnabled(false);
    status.setText("RECORDING STOPPED | frames written: " +
                       juce::String(static_cast<juce::int64>(
                           engine.recorder.getRecordedFrames())) +
                       " | dropped: " +
                       juce::String(static_cast<juce::int64>(
                           engine.recorder.getDroppedFrames())),
                   juce::dontSendNotification);
  }

  void chooseSaveSession() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save NOVA HD session",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("NOVA_HD_Session.json"),
        "*.json");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<MainComponent>(this),
         chooser](const juce::FileChooser &fc) {
          if (safe != nullptr && fc.getResult().existsAsFile() == false &&
              fc.getResult().getFullPathName().isNotEmpty())
            safe->saveSessionToFile(fc.getResult());
          else if (safe != nullptr &&
                   fc.getResult().getFullPathName().isNotEmpty())
            safe->saveSessionToFile(fc.getResult());
        });
  }

  void chooseLoadSession() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load NOVA HD session",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<MainComponent>(this),
         chooser](const juce::FileChooser &fc) {
          if (safe != nullptr && fc.getResult().existsAsFile())
            safe->loadSessionFromFile(fc.getResult());
        });
  }

  void saveSessionToFile(const juce::File &file) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("format", "NOVA_HD_SESSION");
    root->setProperty("version", 7);
    root->setProperty("logicalChannels", kLogicalChannels);
    root->setProperty("pageSize", pageCount);
    root->setProperty("bank", currentBank);
    root->setProperty("customFaderLayout", activeFaderLayout);
    juce::Array<juce::var> customLayoutList;
    for (const auto &layout : customFaderLayouts) {
      auto *layoutObject = new juce::DynamicObject();
      layoutObject->setProperty("name", layout.name);
      juce::Array<juce::var> channelList;
      for (const int channel : layout.channels)
        channelList.add(channel);
      layoutObject->setProperty("channels", channelList);
      customLayoutList.add(juce::var(layoutObject));
    }
    root->setProperty("customFaderLayouts", customLayoutList);
    root->setProperty("masterDb",
                      engine.masterDb.load(std::memory_order_relaxed));
    juce::Array<juce::var> vcaLevels;
    for (const auto &level : engine.vcaMasterDb)
      vcaLevels.add(level.load(std::memory_order_relaxed));
    root->setProperty("vcaMasterDb", vcaLevels);
    juce::Array<juce::var> vcaNames, vcaMutes;
    for (size_t i = 0; i < engine.vcaMasterDb.size(); ++i) {
      vcaNames.add(engine.vcaNames[i].isEmpty() ? "VCA " + juce::String(i + 1)
                                                : engine.vcaNames[i]);
      vcaMutes.add(engine.vcaMuted[i].load(std::memory_order_relaxed));
    }
    root->setProperty("vcaNames", vcaNames);
    root->setProperty("vcaMuted", vcaMutes);
    juce::Array<juce::var> mgMasters;
    for (const auto &m : engine.muteGroupMasters)
      mgMasters.add(m.load(std::memory_order_relaxed));
    root->setProperty("muteGroupMasters", mgMasters);

    juce::Array<juce::var> matrixMainL, matrixMainR, matrixMainM;
    juce::Array<juce::var> matrixAuxSendsArray, matrixAuxMutesArray;
    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      const auto mIdx = static_cast<size_t>(m);
      matrixMainL.add(
          engine.matrixMainLeftDb[mIdx].load(std::memory_order_relaxed));
      matrixMainR.add(
          engine.matrixMainRightDb[mIdx].load(std::memory_order_relaxed));
      matrixMainM.add(
          engine.matrixMainMuted[mIdx].load(std::memory_order_relaxed));

      juce::Array<juce::var> auxSendList, auxMuteList;
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        auxSendList.add(
            engine.matrixAuxSendDb[mIdx][static_cast<size_t>(a)].load(
                std::memory_order_relaxed));
        auxMuteList.add(
            engine.matrixAuxSendMuted[mIdx][static_cast<size_t>(a)].load(
                std::memory_order_relaxed));
      }
      matrixAuxSendsArray.add(auxSendList);
      matrixAuxMutesArray.add(auxMuteList);
    }
    root->setProperty("matrixMainLeftDb", matrixMainL);
    root->setProperty("matrixMainRightDb", matrixMainR);
    root->setProperty("matrixMainMuted", matrixMainM);
    root->setProperty("matrixAuxSendDb", matrixAuxSendsArray);
    root->setProperty("matrixAuxSendMuted", matrixAuxMutesArray);

    // Phase 70: Signal Generator session serialization
    root->setProperty("oscEnabled",
                      engine.oscEnabled.load(std::memory_order_relaxed));
    root->setProperty("oscType",
                      engine.oscType.load(std::memory_order_relaxed));
    root->setProperty("oscFrequency",
                      engine.oscFrequency.load(std::memory_order_relaxed));
    root->setProperty("oscLevelDb",
                      engine.oscLevelDb.load(std::memory_order_relaxed));
    root->setProperty("oscRouteMain",
                      engine.oscRouteMain.load(std::memory_order_relaxed));
    juce::Array<juce::var> oscAux, oscMat;
    for (int a = 0; a < kAuxBusCapacity; ++a)
      oscAux.add(engine.oscRouteAux[static_cast<size_t>(a)].load(
          std::memory_order_relaxed));
    for (int m = 0; m < kMatrixBusCapacity; ++m)
      oscMat.add(engine.oscRouteMatrix[static_cast<size_t>(m)].load(
          std::memory_order_relaxed));
    root->setProperty("oscRouteAux", oscAux);
    root->setProperty("oscRouteMatrix", oscMat);

    // Phase 71: Stereo Linking session serialization
    juce::Array<juce::var> stereoLinks;
    for (size_t p = 0; p < engine.stereoLink.size(); ++p)
      stereoLinks.add(engine.stereoLink[p].load(std::memory_order_relaxed));
    root->setProperty("stereoLink", stereoLinks);

    juce::Array<juce::var> channelArray;
    for (const auto &channel : engine.channels) {
      juce::DynamicObject::Ptr item = new juce::DynamicObject();
      item->setProperty("sourceInput",
                        channel.sourceInput.load(std::memory_order_relaxed));
      item->setProperty("vcaGroup",
                        channel.vcaGroup.load(std::memory_order_relaxed));
      item->setProperty("muteGroupMask",
                        static_cast<int>(channel.muteGroupMask.load(
                            std::memory_order_relaxed)));
      item->setProperty("inputGainDb",
                        channel.inputGainDb.load(std::memory_order_relaxed));
      item->setProperty("polarityInverted", channel.polarityInverted.load(
                                                std::memory_order_relaxed));
      item->setProperty("inputDelayMs",
                        channel.inputDelayMs.load(std::memory_order_relaxed));
      item->setProperty("postEqDynamics",
                        channel.postEqDynamics.load(std::memory_order_relaxed));
      item->setProperty("gainDb",
                        channel.gainDb.load(std::memory_order_relaxed));
      item->setProperty("pan", channel.pan.load(std::memory_order_relaxed));
      item->setProperty("muted", channel.muted.load(std::memory_order_relaxed));
      item->setProperty("muteSafe",
                        channel.muteSafe.load(std::memory_order_relaxed));
      item->setProperty("solo", channel.solo.load(std::memory_order_relaxed));
      item->setProperty("soloSafe",
                        channel.soloSafe.load(std::memory_order_relaxed));
      item->setProperty("soloBus",
                        channel.soloBus.load(std::memory_order_relaxed));
      item->setProperty("soloPfl",
                        channel.soloPfl.load(std::memory_order_relaxed));
      item->setProperty("allPassEnabled",
                        channel.allPassEnabled.load(std::memory_order_relaxed));
      item->setProperty("allPassDegrees",
                        channel.allPassDegrees.load(std::memory_order_relaxed));
      item->setProperty("allPassRange",
                        channel.allPassRange.load(std::memory_order_relaxed));
      item->setProperty("allPassFreqMode", channel.allPassFreqMode.load(
                                               std::memory_order_relaxed));
      item->setProperty(
          "highPass", channel.highPassEnabled.load(std::memory_order_relaxed));
      item->setProperty("highPassFreqHz",
                        channel.highPassFreqHz.load(std::memory_order_relaxed));
      item->setProperty("highPassSlope",
                        channel.highPassSlope.load(std::memory_order_relaxed));
      item->setProperty("lowPassEnabled",
                        channel.lowPassEnabled.load(std::memory_order_relaxed));
      item->setProperty("lowPassFreqHz",
                        channel.lowPassFreqHz.load(std::memory_order_relaxed));
      item->setProperty("lowPassSlope",
                        channel.lowPassSlope.load(std::memory_order_relaxed));
      item->setProperty("compressor", channel.compressorEnabled.load(
                                          std::memory_order_relaxed));
      item->setProperty("sidechainSourceType", channel.sidechainSourceType.load(
                                                   std::memory_order_relaxed));
      item->setProperty(
          "sidechainExternalChannel",
          channel.sidechainExternalChannel.load(std::memory_order_relaxed));
      item->setProperty("sidechainFilterType", channel.sidechainFilterType.load(
                                                   std::memory_order_relaxed));
      item->setProperty(
          "sidechainFilterFreqHz",
          channel.sidechainFilterFreqHz.load(std::memory_order_relaxed));
      item->setProperty("sidechainFilterQ", channel.sidechainFilterQ.load(
                                                std::memory_order_relaxed));
      item->setProperty("sidechainKeyListen", channel.sidechainKeyListen.load(
                                                  std::memory_order_relaxed));
      juce::Array<juce::var> sends, preFader;
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        const auto idx = static_cast<size_t>(a);
        sends.add(channel.auxSendDb[idx].load(std::memory_order_relaxed));
        preFader.add(channel.auxPreFader[idx].load(std::memory_order_relaxed));
      }
      item->setProperty("auxSendDb", sends);
      item->setProperty("auxPreFader", preFader);
      juce::Array<juce::var> eqFrequency, eqGainDb, eqQ;
      for (int band = 0; band < 4; ++band) {
        const auto b = static_cast<size_t>(band);
        eqFrequency.add(channel.eqFrequency[b].load(std::memory_order_relaxed));
        eqGainDb.add(channel.eqGainDb[b].load(std::memory_order_relaxed));
        eqQ.add(channel.eqQ[b].load(std::memory_order_relaxed));
      }
      item->setProperty("eqFrequency", eqFrequency);
      item->setProperty("eqGainDb", eqGainDb);
      item->setProperty("eqQ", eqQ);
      juce::Array<juce::var> dynEqEnabled, dynEqMode, dynEqThresholdDb,
          dynEqRangeDb, dynEqRatio, dynEqKneeDb, dynEqAttackMs, dynEqReleaseMs;
      for (int band = 0; band < 4; ++band) {
        const auto b = static_cast<size_t>(band);
        dynEqEnabled.add(
            channel.dynEqEnabled[b].load(std::memory_order_relaxed));
        dynEqMode.add(channel.dynEqMode[b].load(std::memory_order_relaxed));
        dynEqThresholdDb.add(
            channel.dynEqThresholdDb[b].load(std::memory_order_relaxed));
        dynEqRangeDb.add(
            channel.dynEqRangeDb[b].load(std::memory_order_relaxed));
        dynEqRatio.add(channel.dynEqRatio[b].load(std::memory_order_relaxed));
        dynEqKneeDb.add(channel.dynEqKneeDb[b].load(std::memory_order_relaxed));
        dynEqAttackMs.add(
            channel.dynEqAttackMs[b].load(std::memory_order_relaxed));
        dynEqReleaseMs.add(
            channel.dynEqReleaseMs[b].load(std::memory_order_relaxed));
      }
      item->setProperty("dynEqEnabled", dynEqEnabled);
      item->setProperty("dynEqMode", dynEqMode);
      item->setProperty("dynEqThresholdDb", dynEqThresholdDb);
      item->setProperty("dynEqRangeDb", dynEqRangeDb);
      item->setProperty("dynEqRatio", dynEqRatio);
      item->setProperty("dynEqKneeDb", dynEqKneeDb);
      item->setProperty("dynEqAttackMs", dynEqAttackMs);
      item->setProperty("dynEqReleaseMs", dynEqReleaseMs);
      item->setProperty("gateEnabled",
                        channel.gateEnabled.load(std::memory_order_relaxed));
      item->setProperty("gateThresholdDb", channel.gateThresholdDb.load(
                                               std::memory_order_relaxed));
      item->setProperty("gateRatio",
                        channel.gateRatio.load(std::memory_order_relaxed));
      item->setProperty("gateAttackMs",
                        channel.gateAttackMs.load(std::memory_order_relaxed));
      item->setProperty("gateReleaseMs",
                        channel.gateReleaseMs.load(std::memory_order_relaxed));
      item->setProperty(
          "compressorThresholdDb",
          channel.compressorThresholdDb.load(std::memory_order_relaxed));
      item->setProperty("compressorRatio", channel.compressorRatio.load(
                                               std::memory_order_relaxed));
      item->setProperty("compressorAttackMs", channel.compressorAttackMs.load(
                                                  std::memory_order_relaxed));
      item->setProperty("compressorReleaseMs", channel.compressorReleaseMs.load(
                                                   std::memory_order_relaxed));
      // Phase 85: Surround & Immersive parameters
      item->setProperty("surroundMode",
                        channel.surroundMode.load(std::memory_order_relaxed));
      item->setProperty("surroundX",
                        channel.surroundX.load(std::memory_order_relaxed));
      item->setProperty("surroundY",
                        channel.surroundY.load(std::memory_order_relaxed));
      item->setProperty("surroundZ",
                        channel.surroundZ.load(std::memory_order_relaxed));
      item->setProperty("surroundDivergence", channel.surroundDivergence.load(
                                                  std::memory_order_relaxed));
      item->setProperty("surroundCenterPct", channel.surroundCenterPct.load(
                                                 std::memory_order_relaxed));
      item->setProperty("surroundLfeGainDb", channel.surroundLfeGainDb.load(
                                                 std::memory_order_relaxed));
      channelArray.add(juce::var(item.get()));
    }
    root->setProperty("channels", channelArray);

    juce::Array<juce::var> auxLevels, auxMutes, matrixLevels, matrixMutes;
    for (int a = 0; a < kAuxBusCapacity; ++a) {
      auxLevels.add(engine.auxMasterDb[static_cast<size_t>(a)].load(
          std::memory_order_relaxed));
      auxMutes.add(engine.auxMuted[static_cast<size_t>(a)].load(
          std::memory_order_relaxed));
    }
    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      matrixLevels.add(engine.matrixMasterDb[static_cast<size_t>(m)].load(
          std::memory_order_relaxed));
      matrixMutes.add(engine.matrixMuted[static_cast<size_t>(m)].load(
          std::memory_order_relaxed));
    }
    root->setProperty("auxMasterDb", auxLevels);
    root->setProperty("auxMuted", auxMutes);
    root->setProperty("matrixMasterDb", matrixLevels);
    root->setProperty("matrixMuted", matrixMutes);

    // Phase 75 & 77: Monitor & Graphic EQ session serialization
    root->setProperty("monitorLevelA",
                      engine.monitorLevelA.load(std::memory_order_relaxed));
    root->setProperty("monitorLevelB",
                      engine.monitorLevelB.load(std::memory_order_relaxed));
    root->setProperty("monitorMuteA",
                      engine.monitorMuteA.load(std::memory_order_relaxed));
    root->setProperty("monitorMuteB",
                      engine.monitorMuteB.load(std::memory_order_relaxed));
    root->setProperty("monitorDimA",
                      engine.monitorDimA.load(std::memory_order_relaxed));
    root->setProperty("monitorDimB",
                      engine.monitorDimB.load(std::memory_order_relaxed));
    root->setProperty("talkbackActive",
                      engine.talkbackActive.load(std::memory_order_relaxed));
    root->setProperty("talkbackDuckerDb",
                      engine.talkbackDuckerDb.load(std::memory_order_relaxed));
    root->setProperty("sipEnabled",
                      engine.sipEnabled.load(std::memory_order_relaxed));
    root->setProperty("graphicEqEnabled",
                      engine.graphicEqEnabled.load(std::memory_order_relaxed));
    juce::Array<juce::var> geqBands;
    for (size_t b = 0; b < nova::graviton::GraphicEQ::kNumBands; ++b)
      geqBands.add(engine.mainGraphicEQ.getBandGain(b));
    root->setProperty("graphicEqBands", geqBands);

    // Phase 84: Stage Box Patch Matrix session serialization
    root->setProperty(
        "stageBoxClockSource",
        static_cast<int>(engine.stageBoxPatchMatrix.getClockSource()));
    juce::Array<juce::var> sbInputs;
    for (int c = 0; c < kLogicalChannels; ++c)
      sbInputs.add(engine.stageBoxPatchMatrix.getInputPatch(c));
    root->setProperty("stageBoxInputPatches", sbInputs);

    juce::Array<juce::var> sbOutTypes, sbOutIndices;
    for (size_t p = 0;
         p < nova::graviton::StageBoxPatchMatrix::kTotalOutputPorts; ++p) {
      nova::graviton::OutputBusSourceType bType;
      int bIdx;
      engine.stageBoxPatchMatrix.getOutputPatch(static_cast<int>(p), bType,
                                                bIdx);
      sbOutTypes.add(static_cast<int>(bType));
      sbOutIndices.add(bIdx);
    }
    root->setProperty("stageBoxOutTypes", sbOutTypes);
    root->setProperty("stageBoxOutIndices", sbOutIndices);

    juce::Array<juce::var> chNamesArr;
    for (int c = 0; c < kLogicalChannels; ++c)
      chNamesArr.add(engine.getChannelName(c));
    root->setProperty("channelNames", chNamesArr);

    // Phase 85: Binaural Preview Serialization
    root->setProperty(
        "surroundBinauralMonitorEnabled",
        engine.surroundBinauralMonitorEnabled.load(std::memory_order_relaxed));

    // Phase 86: Talkback session serialization
    root->setProperty("talkbackGainDb", engine.talkbackEngine.gainDb.load(
                                            std::memory_order_relaxed));
    root->setProperty("talkbackHpfHz", engine.talkbackEngine.hpfHz.load(
                                           std::memory_order_relaxed));
    root->setProperty(
        "talkbackHpfEnabled",
        engine.talkbackEngine.hpfEnabled.load(std::memory_order_relaxed));
    root->setProperty(
        "talkbackGateDb",
        engine.talkbackEngine.gateThresholdDb.load(std::memory_order_relaxed));
    root->setProperty(
        "talkbackGateEnabled",
        engine.talkbackEngine.gateEnabled.load(std::memory_order_relaxed));
    root->setProperty("talkbackDuckerDb", engine.talkbackEngine.duckerDb.load(
                                              std::memory_order_relaxed));
    root->setProperty(
        "talkbackSwitchMode",
        engine.talkbackEngine.switchMode.load(std::memory_order_relaxed));
    root->setProperty("talkbackSoloA", engine.talkbackEngine.destSoloA.load(
                                           std::memory_order_relaxed));
    root->setProperty("talkbackSoloB", engine.talkbackEngine.destSoloB.load(
                                           std::memory_order_relaxed));
    root->setProperty("talkbackMainLR", engine.talkbackEngine.destMainLR.load(
                                            std::memory_order_relaxed));

    // Phase 87: OSC Server serialization
    root->setProperty("oscServerPort", engine.oscServer.getServerPort());

    // Phase 88: Matrix Plus session serialization
    root->setProperty(
        "matrixPlusTemp",
        engine.matrixPlusEngine.getEnvironment().temperatureCelsius);
    root->setProperty(
        "matrixPlusHumidity",
        engine.matrixPlusEngine.getEnvironment().relativeHumidityPct);
    juce::Array<juce::var> mtxDelays, mtxEnabled, mtxPol, mtxApEnabled,
        mtxApFreq, mtxApQ, mtxTrim;
    for (size_t m = 0; m < 24; ++m) {
      const auto &ch = engine.matrixPlusEngine.getChannel(m);
      mtxDelays.add(ch.getDelayMs());
      mtxEnabled.add(ch.isDelayEnabled());
      mtxPol.add(ch.getPolarityInvert());
      mtxApEnabled.add(ch.isAllPassEnabled());
      mtxApFreq.add(ch.getAllPassFrequency());
      mtxApQ.add(ch.getAllPassQ());
      mtxTrim.add(ch.getGainTrimDb());
    }
    root->setProperty("matrixPlusDelays", mtxDelays);
    root->setProperty("matrixPlusEnabled", mtxEnabled);
    root->setProperty("matrixPlusPol", mtxPol);
    root->setProperty("matrixPlusApEnabled", mtxApEnabled);
    root->setProperty("matrixPlusApFreq", mtxApFreq);
    root->setProperty("matrixPlusApQ", mtxApQ);
    root->setProperty("matrixPlusTrim", mtxTrim);

    const auto json = juce::JSON::toString(juce::var(root.get()), true);
    if (file.replaceWithText(json))
      status.setText("SESSION SAVED: " + file.getFileName(),
                     juce::dontSendNotification);
    else
      status.setText("SAVE FAILED: " + file.getFileName(),
                     juce::dontSendNotification);
  }

  void loadSessionFromFile(const juce::File &file) {
    juce::var parsed = juce::JSON::parse(file);
    auto *root = parsed.getDynamicObject();
    if (root == nullptr ||
        root->getProperty("format").toString() != "NOVA_HD_SESSION") {
      status.setText("LOAD FAILED: invalid session file",
                     juce::dontSendNotification);
      return;
    }

    const auto channelsVar = root->getProperty("channels");
    auto *channelArray = channelsVar.getArray();
    if (channelArray == nullptr || channelArray->size() != kLogicalChannels) {
      status.setText("LOAD FAILED: incompatible channel count",
                     juce::dontSendNotification);
      return;
    }
    UserFaderLayoutEditor::Layouts loadedFaderLayouts;
    const bool containsFaderLayouts = root->hasProperty("customFaderLayouts");
    if (containsFaderLayouts &&
        !decodeFaderLayouts(root->getProperty("customFaderLayouts"),
                            loadedFaderLayouts)) {
      status.setText("LOAD FAILED: invalid custom fader layouts",
                     juce::dontSendNotification);
      return;
    }

    if (root->hasProperty("matrixMainLeftDb")) {
      if (const auto *arr = root->getProperty("matrixMainLeftDb").getArray()) {
        for (int m = 0; m < juce::jmin(kMatrixBusCapacity, arr->size()); ++m)
          engine.matrixMainLeftDb[static_cast<size_t>(m)].store(
              juce::jlimit(-60.0f, 10.0f, static_cast<float>((*arr)[m])),
              std::memory_order_relaxed);
      }
    }
    if (root->hasProperty("matrixMainRightDb")) {
      if (const auto *arr = root->getProperty("matrixMainRightDb").getArray()) {
        for (int m = 0; m < juce::jmin(kMatrixBusCapacity, arr->size()); ++m)
          engine.matrixMainRightDb[static_cast<size_t>(m)].store(
              juce::jlimit(-60.0f, 10.0f, static_cast<float>((*arr)[m])),
              std::memory_order_relaxed);
      }
    }
    if (root->hasProperty("matrixMainMuted")) {
      if (const auto *arr = root->getProperty("matrixMainMuted").getArray()) {
        for (int m = 0; m < juce::jmin(kMatrixBusCapacity, arr->size()); ++m)
          engine.matrixMainMuted[static_cast<size_t>(m)].store(
              static_cast<bool>((*arr)[m]), std::memory_order_relaxed);
      }
    }
    if (root->hasProperty("matrixAuxSendDb")) {
      if (const auto *mArr = root->getProperty("matrixAuxSendDb").getArray()) {
        for (int m = 0; m < juce::jmin(kMatrixBusCapacity, mArr->size()); ++m) {
          if (const auto *aArr = (*mArr)[m].getArray()) {
            for (int a = 0; a < juce::jmin(kAuxBusCapacity, aArr->size()); ++a)
              engine
                  .matrixAuxSendDb[static_cast<size_t>(m)]
                                  [static_cast<size_t>(a)]
                  .store(juce::jlimit(-60.0f, 10.0f,
                                      static_cast<float>((*aArr)[a])),
                         std::memory_order_relaxed);
          }
        }
      }
    }
    if (root->hasProperty("matrixAuxSendMuted")) {
      if (const auto *mArr =
              root->getProperty("matrixAuxSendMuted").getArray()) {
        for (int m = 0; m < juce::jmin(kMatrixBusCapacity, mArr->size()); ++m) {
          if (const auto *aArr = (*mArr)[m].getArray()) {
            for (int a = 0; a < juce::jmin(kAuxBusCapacity, aArr->size()); ++a)
              engine
                  .matrixAuxSendMuted[static_cast<size_t>(m)]
                                     [static_cast<size_t>(a)]
                  .store(static_cast<bool>((*aArr)[a]),
                         std::memory_order_relaxed);
          }
        }
      }
    }

    // Phase 70: Signal Generator deserialization
    if (root->hasProperty("oscEnabled"))
      engine.oscEnabled.store(
          static_cast<bool>(root->getProperty("oscEnabled")),
          std::memory_order_relaxed);
    if (root->hasProperty("oscType"))
      engine.oscType.store(
          juce::jlimit(0, 2, static_cast<int>(root->getProperty("oscType"))),
          std::memory_order_relaxed);
    if (root->hasProperty("oscFrequency"))
      engine.oscFrequency.store(
          juce::jlimit(20.0f, 20000.0f,
                       static_cast<float>(root->getProperty("oscFrequency"))),
          std::memory_order_relaxed);
    if (root->hasProperty("oscLevelDb"))
      engine.oscLevelDb.store(
          juce::jlimit(-60.0f, 0.0f,
                       static_cast<float>(root->getProperty("oscLevelDb"))),
          std::memory_order_relaxed);
    if (root->hasProperty("oscRouteMain"))
      engine.oscRouteMain.store(
          static_cast<bool>(root->getProperty("oscRouteMain")),
          std::memory_order_relaxed);
    if (root->hasProperty("oscRouteAux")) {
      if (const auto *aArr = root->getProperty("oscRouteAux").getArray()) {
        for (int a = 0; a < juce::jmin(kAuxBusCapacity, aArr->size()); ++a)
          engine.oscRouteAux[static_cast<size_t>(a)].store(
              static_cast<bool>((*aArr)[a]), std::memory_order_relaxed);
      }
    }
    if (root->hasProperty("oscRouteMatrix")) {
      if (const auto *mArr = root->getProperty("oscRouteMatrix").getArray()) {
        for (int m = 0; m < juce::jmin(kMatrixBusCapacity, mArr->size()); ++m)
          engine.oscRouteMatrix[static_cast<size_t>(m)].store(
              static_cast<bool>((*mArr)[m]), std::memory_order_relaxed);
      }
    }

    // Phase 71: Stereo Linking deserialization
    if (root->hasProperty("stereoLink")) {
      if (const auto *sArr = root->getProperty("stereoLink").getArray()) {
        for (int p = 0;
             p < juce::jmin(static_cast<int>(engine.stereoLink.size()),
                            sArr->size());
             ++p) {
          const bool linked = static_cast<bool>((*sArr)[p]);
          engine.setStereoLink(p, linked);
        }
      }
    }

    // Phase 75 & 77: Monitor & Graphic EQ deserialization
    if (root->hasProperty("monitorLevelA"))
      engine.monitorLevelA.store(
          juce::jlimit(
              -60.0f, 10.0f,
              static_cast<float>((double)root->getProperty("monitorLevelA"))),
          std::memory_order_relaxed);
    if (root->hasProperty("monitorLevelB"))
      engine.monitorLevelB.store(
          juce::jlimit(
              -60.0f, 10.0f,
              static_cast<float>((double)root->getProperty("monitorLevelB"))),
          std::memory_order_relaxed);
    if (root->hasProperty("monitorMuteA"))
      engine.monitorMuteA.store(
          static_cast<bool>(root->getProperty("monitorMuteA")),
          std::memory_order_relaxed);
    if (root->hasProperty("monitorMuteB"))
      engine.monitorMuteB.store(
          static_cast<bool>(root->getProperty("monitorMuteB")),
          std::memory_order_relaxed);
    if (root->hasProperty("monitorDimA"))
      engine.monitorDimA.store(
          static_cast<bool>(root->getProperty("monitorDimA")),
          std::memory_order_relaxed);
    if (root->hasProperty("monitorDimB"))
      engine.monitorDimB.store(
          static_cast<bool>(root->getProperty("monitorDimB")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackActive"))
      engine.talkbackActive.store(
          static_cast<bool>(root->getProperty("talkbackActive")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackDuckerDb"))
      engine.talkbackDuckerDb.store(
          static_cast<float>((double)root->getProperty("talkbackDuckerDb")),
          std::memory_order_relaxed);
    if (root->hasProperty("sipEnabled"))
      engine.sipEnabled.store(
          static_cast<bool>(root->getProperty("sipEnabled")),
          std::memory_order_relaxed);
    if (root->hasProperty("graphicEqEnabled"))
      engine.graphicEqEnabled.store(
          static_cast<bool>(root->getProperty("graphicEqEnabled")),
          std::memory_order_relaxed);
    if (root->hasProperty("graphicEqBands")) {
      if (const auto *arr = root->getProperty("graphicEqBands").getArray()) {
        for (size_t b = 0;
             b <
             std::min(arr->size(),
                      static_cast<int>(nova::graviton::GraphicEQ::kNumBands));
             ++b)
          engine.mainGraphicEQ.setBandGain(
              b, static_cast<float>((double)(*arr)[static_cast<int>(b)]));
      }
    }

    // Phase 84: Stage Box Patch Matrix deserialization
    if (root->hasProperty("stageBoxClockSource"))
      engine.stageBoxPatchMatrix.setClockSource(
          static_cast<nova::graviton::ClockSyncSource>(
              static_cast<int>(root->getProperty("stageBoxClockSource"))));
    if (const auto *inArr =
            root->getProperty("stageBoxInputPatches").getArray()) {
      for (int c = 0; c < std::min(inArr->size(), kLogicalChannels); ++c)
        engine.stageBoxPatchMatrix.patchInput(c, static_cast<int>((*inArr)[c]));
    }
    if (const auto *tArr = root->getProperty("stageBoxOutTypes").getArray()) {
      if (const auto *iArr =
              root->getProperty("stageBoxOutIndices").getArray()) {
        for (int p = 0;
             p <
             std::min(
                 {tArr->size(), iArr->size(),
                  static_cast<int>(
                      nova::graviton::StageBoxPatchMatrix::kTotalOutputPorts)});
             ++p) {
          engine.stageBoxPatchMatrix.patchOutput(
              p,
              static_cast<nova::graviton::OutputBusSourceType>(
                  static_cast<int>((*tArr)[p])),
              static_cast<int>((*iArr)[p]));
        }
      }
    }
    if (const auto *nArr = root->getProperty("channelNames").getArray()) {
      for (int c = 0; c < std::min(nArr->size(), kLogicalChannels); ++c)
        engine.channelNames[static_cast<size_t>(c)] = (*nArr)[c].toString();
    }

    // Phase 86: Talkback session deserialization
    if (root->hasProperty("talkbackGainDb"))
      engine.talkbackEngine.gainDb.store(
          static_cast<float>(root->getProperty("talkbackGainDb")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackHpfHz"))
      engine.talkbackEngine.hpfHz.store(
          static_cast<float>(root->getProperty("talkbackHpfHz")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackHpfEnabled"))
      engine.talkbackEngine.hpfEnabled.store(
          static_cast<bool>(root->getProperty("talkbackHpfEnabled")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackGateDb"))
      engine.talkbackEngine.gateThresholdDb.store(
          static_cast<float>(root->getProperty("talkbackGateDb")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackGateEnabled"))
      engine.talkbackEngine.gateEnabled.store(
          static_cast<bool>(root->getProperty("talkbackGateEnabled")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackDuckerDb"))
      engine.talkbackEngine.duckerDb.store(
          static_cast<float>(root->getProperty("talkbackDuckerDb")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackSwitchMode"))
      engine.talkbackEngine.switchMode.store(
          static_cast<int>(root->getProperty("talkbackSwitchMode")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackSoloA"))
      engine.talkbackEngine.destSoloA.store(
          static_cast<bool>(root->getProperty("talkbackSoloA")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackSoloB"))
      engine.talkbackEngine.destSoloB.store(
          static_cast<bool>(root->getProperty("talkbackSoloB")),
          std::memory_order_relaxed);
    if (root->hasProperty("talkbackMainLR"))
      engine.talkbackEngine.destMainLR.store(
          static_cast<bool>(root->getProperty("talkbackMainLR")),
          std::memory_order_relaxed);

    // Phase 88: Matrix Plus session deserialization
    if (root->hasProperty("matrixPlusTemp"))
      engine.matrixPlusEngine.setTemperatureCelsius(
          static_cast<double>(root->getProperty("matrixPlusTemp")));
    if (root->hasProperty("matrixPlusHumidity"))
      engine.matrixPlusEngine.setRelativeHumidity(
          static_cast<double>(root->getProperty("matrixPlusHumidity")));
    if (const auto *dArr = root->getProperty("matrixPlusDelays").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(dArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setDelayMs(static_cast<double>((*dArr)[m]));
      }
    }
    if (const auto *eArr = root->getProperty("matrixPlusEnabled").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(eArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setDelayEnabled(static_cast<bool>((*eArr)[m]));
      }
    }
    if (const auto *pArr = root->getProperty("matrixPlusPol").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(pArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setPolarityInvert(static_cast<bool>((*pArr)[m]));
      }
    }
    if (const auto *apArr =
            root->getProperty("matrixPlusApEnabled").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(apArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setAllPassEnabled(static_cast<bool>((*apArr)[m]));
      }
    }
    if (const auto *fArr = root->getProperty("matrixPlusApFreq").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(fArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setAllPassFrequency(static_cast<double>((*fArr)[m]));
      }
    }
    if (const auto *qArr = root->getProperty("matrixPlusApQ").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(qArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setAllPassQ(static_cast<double>((*qArr)[m]));
      }
    }
    if (const auto *trArr = root->getProperty("matrixPlusTrim").getArray()) {
      for (int m = 0; m < std::min(static_cast<int>(trArr->size()), 24); ++m) {
        engine.matrixPlusEngine.getChannel(static_cast<size_t>(m))
            .setGainTrimDb(static_cast<double>((*trArr)[m]));
      }
    }

    for (int c = 0; c < kLogicalChannels; ++c) {
      auto *item = (*channelArray)[c].getDynamicObject();
      if (item == nullptr)
        continue;
      auto &channel = engine.channels[static_cast<size_t>(c)];
      const int input = static_cast<int>(item->getProperty("sourceInput"));
      channel.sourceInput.store(
          input >= 0 && input < kInitialPhysicalInputs ? input : -1,
          std::memory_order_relaxed);
      const int vcaGroup = item->hasProperty("vcaGroup")
                               ? static_cast<int>(item->getProperty("vcaGroup"))
                               : 0;
      channel.vcaGroup.store(juce::jlimit(0, 24, vcaGroup),
                             std::memory_order_relaxed);
      const int mgMask =
          item->hasProperty("muteGroupMask")
              ? static_cast<int>(item->getProperty("muteGroupMask"))
              : 0;
      channel.muteGroupMask.store(
          static_cast<uint8_t>(juce::jlimit(0, 255, mgMask)),
          std::memory_order_relaxed);
      const float inputGainDb =
          item->hasProperty("inputGainDb")
              ? static_cast<float>((double)item->getProperty("inputGainDb"))
              : 0.0f;
      channel.inputGainDb.store(juce::jlimit(-20.0f, 60.0f, inputGainDb),
                                std::memory_order_relaxed);
      if (item->hasProperty("polarityInverted"))
        channel.polarityInverted.store(
            static_cast<bool>(item->getProperty("polarityInverted")),
            std::memory_order_relaxed);
      if (item->hasProperty("inputDelayMs"))
        channel.inputDelayMs.store(
            juce::jlimit(
                0.0f, 500.0f,
                static_cast<float>((double)item->getProperty("inputDelayMs"))),
            std::memory_order_relaxed);
      if (item->hasProperty("postEqDynamics"))
        channel.postEqDynamics.store(
            static_cast<bool>(item->getProperty("postEqDynamics")),
            std::memory_order_relaxed);
      channel.gainDb.store(
          static_cast<float>(
              juce::jlimit(-60.0, 10.0, (double)item->getProperty("gainDb"))),
          std::memory_order_relaxed);
      channel.pan.store(static_cast<float>(juce::jlimit(
                            -1.0, 1.0, (double)item->getProperty("pan"))),
                        std::memory_order_relaxed);
      channel.muted.store((bool)item->getProperty("muted"),
                          std::memory_order_relaxed);
      if (item->hasProperty("muteSafe"))
        channel.muteSafe.store((bool)item->getProperty("muteSafe"),
                               std::memory_order_relaxed);
      channel.solo.store((bool)item->getProperty("solo"),
                         std::memory_order_relaxed);
      if (item->hasProperty("soloSafe"))
        channel.soloSafe.store((bool)item->getProperty("soloSafe"),
                               std::memory_order_relaxed);
      if (item->hasProperty("soloBus"))
        channel.soloBus.store(
            juce::jlimit(0, 1, static_cast<int>(item->getProperty("soloBus"))),
            std::memory_order_relaxed);
      if (item->hasProperty("soloPfl"))
        channel.soloPfl.store(static_cast<bool>(item->getProperty("soloPfl")),
                              std::memory_order_relaxed);
      if (item->hasProperty("allPassEnabled"))
        channel.allPassEnabled.store(
            static_cast<bool>(item->getProperty("allPassEnabled")),
            std::memory_order_relaxed);
      if (item->hasProperty("allPassDegrees"))
        channel.allPassDegrees.store(
            juce::jlimit(0.0f, 180.0f,
                         static_cast<float>(
                             (double)item->getProperty("allPassDegrees"))),
            std::memory_order_relaxed);
      if (item->hasProperty("allPassRange"))
        channel.allPassRange.store(
            juce::jlimit(0, 1,
                         static_cast<int>(item->getProperty("allPassRange"))),
            std::memory_order_relaxed);
      if (item->hasProperty("allPassFreqMode"))
        channel.allPassFreqMode.store(
            juce::jlimit(
                0, 1, static_cast<int>(item->getProperty("allPassFreqMode"))),
            std::memory_order_relaxed);
      channel.highPassEnabled.store((bool)item->getProperty("highPass"),
                                    std::memory_order_relaxed);
      if (item->hasProperty("highPassFreqHz"))
        channel.highPassFreqHz.store(
            juce::jlimit(20.0f, 2000.0f,
                         static_cast<float>(
                             (double)item->getProperty("highPassFreqHz"))),
            std::memory_order_relaxed);
      if (item->hasProperty("highPassSlope"))
        channel.highPassSlope.store(
            juce::jlimit(0, 1,
                         static_cast<int>(item->getProperty("highPassSlope"))),
            std::memory_order_relaxed);
      if (item->hasProperty("lowPassEnabled"))
        channel.lowPassEnabled.store((bool)item->getProperty("lowPassEnabled"),
                                     std::memory_order_relaxed);
      if (item->hasProperty("lowPassFreqHz"))
        channel.lowPassFreqHz.store(
            juce::jlimit(
                1000.0f, 20000.0f,
                static_cast<float>((double)item->getProperty("lowPassFreqHz"))),
            std::memory_order_relaxed);
      if (item->hasProperty("lowPassSlope"))
        channel.lowPassSlope.store(
            juce::jlimit(0, 1,
                         static_cast<int>(item->getProperty("lowPassSlope"))),
            std::memory_order_relaxed);
      channel.compressorEnabled.store((bool)item->getProperty("compressor"),
                                      std::memory_order_relaxed);
      if (item->hasProperty("sidechainSourceType"))
        channel.sidechainSourceType.store(
            juce::jlimit(
                0, 1,
                static_cast<int>(item->getProperty("sidechainSourceType"))),
            std::memory_order_relaxed);
      if (item->hasProperty("sidechainExternalChannel"))
        channel.sidechainExternalChannel.store(
            juce::jlimit(0, kLogicalChannels - 1,
                         static_cast<int>(
                             item->getProperty("sidechainExternalChannel"))),
            std::memory_order_relaxed);
      if (item->hasProperty("sidechainFilterType"))
        channel.sidechainFilterType.store(
            juce::jlimit(
                0, 4,
                static_cast<int>(item->getProperty("sidechainFilterType"))),
            std::memory_order_relaxed);
      if (item->hasProperty("sidechainFilterFreqHz"))
        channel.sidechainFilterFreqHz.store(
            juce::jlimit(20.0f, 20000.0f,
                         static_cast<float>((double)item->getProperty(
                             "sidechainFilterFreqHz"))),
            std::memory_order_relaxed);
      if (item->hasProperty("sidechainFilterQ"))
        channel.sidechainFilterQ.store(
            juce::jlimit(0.1f, 10.0f,
                         static_cast<float>(
                             (double)item->getProperty("sidechainFilterQ"))),
            std::memory_order_relaxed);
      if (item->hasProperty("sidechainKeyListen"))
        channel.sidechainKeyListen.store(
            static_cast<bool>(item->getProperty("sidechainKeyListen")),
            std::memory_order_relaxed);
      auto sendsVar = item->getProperty("auxSendDb");
      auto *sends = sendsVar.getArray();
      auto preVar = item->getProperty("auxPreFader");
      auto *pres = preVar.getArray();
      for (int a = 0; a < kAuxBusCapacity; ++a) {
        const auto idx = static_cast<size_t>(a);
        if (sends != nullptr && sends->size() > a)
          channel.auxSendDb[idx].store(
              static_cast<float>(juce::jlimit(-60.0, 6.0, (double)(*sends)[a])),
              std::memory_order_relaxed);
        if (pres != nullptr && pres->size() > a)
          channel.auxPreFader[idx].store((bool)(*pres)[a],
                                         std::memory_order_relaxed);
      }
      const auto eqFreqVar = item->getProperty("eqFrequency");
      const auto eqGainVar = item->getProperty("eqGainDb");
      const auto eqQVar = item->getProperty("eqQ");
      const auto *eqFreq = eqFreqVar.getArray();
      const auto *eqGain = eqGainVar.getArray();
      const auto *eqQValues = eqQVar.getArray();
      const std::array<float, 4> defaultFreq{100.0f, 500.0f, 2000.0f, 8000.0f};
      for (int band = 0; band < 4; ++band) {
        const auto b = static_cast<size_t>(band);
        const float f = eqFreq != nullptr && eqFreq->size() > band
                            ? static_cast<float>((*eqFreq)[band])
                            : defaultFreq[b];
        const float g = eqGain != nullptr && eqGain->size() > band
                            ? static_cast<float>((*eqGain)[band])
                            : 0.0f;
        const float qValue = eqQValues != nullptr && eqQValues->size() > band
                                 ? static_cast<float>((*eqQValues)[band])
                                 : 1.0f;
        channel.eqFrequency[b].store(juce::jlimit(20.0f, 20000.0f, f),
                                     std::memory_order_relaxed);
        channel.eqGainDb[b].store(juce::jlimit(-18.0f, 18.0f, g),
                                  std::memory_order_relaxed);
        channel.eqQ[b].store(juce::jlimit(0.1f, 10.0f, qValue),
                             std::memory_order_relaxed);
      }
      const auto *dEnArr = item->getProperty("dynEqEnabled").getArray();
      const auto *dModeArr = item->getProperty("dynEqMode").getArray();
      const auto *dThreshArr = item->getProperty("dynEqThresholdDb").getArray();
      const auto *dRangeArr = item->getProperty("dynEqRangeDb").getArray();
      const auto *dRatioArr = item->getProperty("dynEqRatio").getArray();
      const auto *dKneeArr = item->getProperty("dynEqKneeDb").getArray();
      const auto *dAttArr = item->getProperty("dynEqAttackMs").getArray();
      const auto *dRelArr = item->getProperty("dynEqReleaseMs").getArray();
      for (int band = 0; band < 4; ++band) {
        const auto b = static_cast<size_t>(band);
        if (dEnArr != nullptr && dEnArr->size() > band)
          channel.dynEqEnabled[b].store(static_cast<bool>((*dEnArr)[band]),
                                        std::memory_order_relaxed);
        if (dModeArr != nullptr && dModeArr->size() > band)
          channel.dynEqMode[b].store(static_cast<int>((*dModeArr)[band]),
                                     std::memory_order_relaxed);
        if (dThreshArr != nullptr && dThreshArr->size() > band)
          channel.dynEqThresholdDb[b].store(
              static_cast<float>((*dThreshArr)[band]),
              std::memory_order_relaxed);
        if (dRangeArr != nullptr && dRangeArr->size() > band)
          channel.dynEqRangeDb[b].store(static_cast<float>((*dRangeArr)[band]),
                                        std::memory_order_relaxed);
        if (dRatioArr != nullptr && dRatioArr->size() > band)
          channel.dynEqRatio[b].store(static_cast<float>((*dRatioArr)[band]),
                                      std::memory_order_relaxed);
        if (dKneeArr != nullptr && dKneeArr->size() > band)
          channel.dynEqKneeDb[b].store(static_cast<float>((*dKneeArr)[band]),
                                       std::memory_order_relaxed);
        if (dAttArr != nullptr && dAttArr->size() > band)
          channel.dynEqAttackMs[b].store(static_cast<float>((*dAttArr)[band]),
                                         std::memory_order_relaxed);
        if (dRelArr != nullptr && dRelArr->size() > band)
          channel.dynEqReleaseMs[b].store(static_cast<float>((*dRelArr)[band]),
                                          std::memory_order_relaxed);
      }
      const auto getFloatOr = [item](const juce::Identifier &key,
                                     float fallback) {
        const auto value = item->getProperty(key);
        return value.isVoid() ? fallback : static_cast<float>(value);
      };
      channel.gateEnabled.store(
          static_cast<bool>(item->getProperty("gateEnabled")),
          std::memory_order_relaxed);
      channel.gateThresholdDb.store(
          juce::jlimit(-80.0f, 0.0f, getFloatOr("gateThresholdDb", -45.0f)),
          std::memory_order_relaxed);
      channel.gateRatio.store(
          juce::jlimit(1.0f, 10.0f, getFloatOr("gateRatio", 4.0f)),
          std::memory_order_relaxed);
      channel.gateAttackMs.store(
          juce::jlimit(0.1f, 100.0f, getFloatOr("gateAttackMs", 2.0f)),
          std::memory_order_relaxed);
      channel.gateReleaseMs.store(
          juce::jlimit(10.0f, 1000.0f, getFloatOr("gateReleaseMs", 120.0f)),
          std::memory_order_relaxed);
      channel.compressorThresholdDb.store(
          juce::jlimit(-60.0f, 0.0f,
                       getFloatOr("compressorThresholdDb", -18.0f)),
          std::memory_order_relaxed);
      channel.compressorRatio.store(
          juce::jlimit(1.0f, 20.0f, getFloatOr("compressorRatio", 4.0f)),
          std::memory_order_relaxed);
      channel.compressorAttackMs.store(
          juce::jlimit(0.1f, 100.0f, getFloatOr("compressorAttackMs", 10.0f)),
          std::memory_order_relaxed);
      channel.compressorReleaseMs.store(
          juce::jlimit(10.0f, 1000.0f,
                       getFloatOr("compressorReleaseMs", 120.0f)),
          std::memory_order_relaxed);

      // Phase 85: Surround & Immersive deserialization
      if (item->hasProperty("surroundMode"))
        channel.surroundMode.store(
            juce::jlimit(0, 5,
                         static_cast<int>(item->getProperty("surroundMode"))),
            std::memory_order_relaxed);
      if (item->hasProperty("surroundX"))
        channel.surroundX.store(
            juce::jlimit(
                -1.0f, 1.0f,
                static_cast<float>((double)item->getProperty("surroundX"))),
            std::memory_order_relaxed);
      if (item->hasProperty("surroundY"))
        channel.surroundY.store(
            juce::jlimit(
                -1.0f, 1.0f,
                static_cast<float>((double)item->getProperty("surroundY"))),
            std::memory_order_relaxed);
      if (item->hasProperty("surroundZ"))
        channel.surroundZ.store(
            juce::jlimit(
                0.0f, 1.0f,
                static_cast<float>((double)item->getProperty("surroundZ"))),
            std::memory_order_relaxed);
      if (item->hasProperty("surroundDivergence"))
        channel.surroundDivergence.store(
            juce::jlimit(0.0f, 1.0f,
                         static_cast<float>(
                             (double)item->getProperty("surroundDivergence"))),
            std::memory_order_relaxed);
      if (item->hasProperty("surroundCenterPct"))
        channel.surroundCenterPct.store(
            juce::jlimit(0.0f, 100.0f,
                         static_cast<float>(
                             (double)item->getProperty("surroundCenterPct"))),
            std::memory_order_relaxed);
      if (item->hasProperty("surroundLfeGainDb"))
        channel.surroundLfeGainDb.store(
            juce::jlimit(-60.0f, 10.0f,
                         static_cast<float>(
                             (double)item->getProperty("surroundLfeGainDb"))),
            std::memory_order_relaxed);
    }

    const auto vcaLevelsVar = root->getProperty("vcaMasterDb");
    const auto *vcaLevels = vcaLevelsVar.getArray();
    for (int v = 0; v < 24; ++v) {
      const float db = (vcaLevels != nullptr && vcaLevels->size() > v)
                           ? static_cast<float>(juce::jlimit(
                                 -60.0, 10.0, (double)(*vcaLevels)[v]))
                           : 0.0f;
      engine.vcaMasterDb[static_cast<size_t>(v)].store(
          db, std::memory_order_relaxed);
    }
    const auto vcaNamesVar = root->getProperty("vcaNames");
    const auto *savedVcaNames = vcaNamesVar.getArray();
    const auto vcaMutesVar = root->getProperty("vcaMuted");
    const auto *savedVcaMutes = vcaMutesVar.getArray();
    for (int v = 0; v < 24; ++v) {
      const auto idx = static_cast<size_t>(v);
      const auto fallbackName = "VCA " + juce::String(v + 1);
      const auto name = (savedVcaNames != nullptr && savedVcaNames->size() > v)
                            ? (*savedVcaNames)[v].toString().trim()
                            : fallbackName;
      engine.vcaNames[idx] =
          name.isEmpty() ? fallbackName : name.substring(0, 24);
      const bool muted = savedVcaMutes != nullptr && savedVcaMutes->size() > v
                             ? static_cast<bool>((*savedVcaMutes)[v])
                             : false;
      engine.vcaMuted[idx].store(muted, std::memory_order_relaxed);
    }
    engine.masterDb.store(
        static_cast<float>(
            juce::jlimit(-60.0, 10.0, (double)root->getProperty("masterDb"))),
        std::memory_order_relaxed);
    const auto auxLevelsVar = root->getProperty("auxMasterDb");
    const auto *auxLevels = auxLevelsVar.getArray();
    const auto auxMutesVar = root->getProperty("auxMuted");
    const auto *auxMutes = auxMutesVar.getArray();
    for (int a = 0; a < kAuxBusCapacity; ++a) {
      const auto idx = static_cast<size_t>(a);
      if (auxLevels != nullptr && auxLevels->size() > a)
        engine.auxMasterDb[idx].store(
            static_cast<float>(
                juce::jlimit(-60.0, 10.0, (double)(*auxLevels)[a])),
            std::memory_order_relaxed);
      if (auxMutes != nullptr && auxMutes->size() > a)
        engine.auxMuted[idx].store((bool)(*auxMutes)[a],
                                   std::memory_order_relaxed);
    }
    const auto matrixLevelsVar = root->getProperty("matrixMasterDb");
    const auto *matrixLevels = matrixLevelsVar.getArray();
    const auto matrixMutesVar = root->getProperty("matrixMuted");
    const auto *matrixMutes = matrixMutesVar.getArray();
    for (int m = 0; m < kMatrixBusCapacity; ++m) {
      const auto idx = static_cast<size_t>(m);
      if (matrixLevels != nullptr && matrixLevels->size() > m)
        engine.matrixMasterDb[idx].store(
            static_cast<float>(
                juce::jlimit(-60.0, 10.0, (double)(*matrixLevels)[m])),
            std::memory_order_relaxed);
      if (matrixMutes != nullptr && matrixMutes->size() > m)
        engine.matrixMuted[idx].store((bool)(*matrixMutes)[m],
                                      std::memory_order_relaxed);
    }

    if (root->hasProperty("muteGroupMasters")) {
      if (const auto *mgArray =
              root->getProperty("muteGroupMasters").getArray()) {
        for (int g = 0; g < juce::jmin(8, mgArray->size()); ++g)
          engine.muteGroupMasters[static_cast<size_t>(g)].store(
              static_cast<bool>((*mgArray)[g]), std::memory_order_relaxed);
      }
    }

    if (containsFaderLayouts)
      customFaderLayouts = std::move(loadedFaderLayouts);
    activeFaderLayout =
        root->hasProperty("customFaderLayout")
            ? juce::jlimit(
                  -1, static_cast<int>(customFaderLayouts.size()) - 1,
                  static_cast<int>(root->getProperty("customFaderLayout")))
            : -1;
    refreshFaderLayoutSelector();
    const int requestedPage = static_cast<int>(root->getProperty("pageSize"));
    setPageSize(requestedPage == 16 || requestedPage == 24 ? requestedPage : 8);
    currentBank =
        juce::jlimit(0, juce::jmax(0, (normalChannelCount() - 1) / pageCount),
                     static_cast<int>(root->getProperty("bank")));
    updateBankVisibility();
    const bool layoutsPersisted = persistFaderLayouts();
    master.setValue(engine.masterDb.load(), juce::dontSendNotification);
    for (int c = 0; c < kLogicalChannels; ++c)
      strips[static_cast<size_t>(c)]->syncFromState();
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      busMaster[static_cast<size_t>(a)]->setValue(
          engine.auxMasterDb[static_cast<size_t>(a)].load(),
          juce::dontSendNotification);
      busMute[static_cast<size_t>(a)]->setToggleState(
          engine.auxMuted[static_cast<size_t>(a)].load(),
          juce::dontSendNotification);
    }
    for (int m = 0; m < kVisibleMatrixBuses; ++m) {
      const auto idx = static_cast<size_t>(m);
      const auto ui = static_cast<size_t>(kVisibleAuxSends + m);
      busMaster[ui]->setValue(engine.matrixMasterDb[idx].load(),
                              juce::dontSendNotification);
      busMute[ui]->setToggleState(engine.matrixMuted[idx].load(),
                                  juce::dontSendNotification);
    }
    engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
    engine.syncAuxRoutingSnapshot();
    engine.matrixRoutingDirty.store(true, std::memory_order_relaxed);
    engine.syncMatrixRoutingSnapshot();
    if (root->hasProperty("surroundBinauralMonitorEnabled"))
      engine.surroundBinauralMonitorEnabled.store(
          static_cast<bool>(
              root->getProperty("surroundBinauralMonitorEnabled")),
          std::memory_order_relaxed);
    status.setText(layoutsPersisted
                       ? "SESSION LOADED: " + file.getFileName()
                       : "SESSION LOADED; FADER LAYOUT PROFILE SAVE FAILED",
                   juce::dontSendNotification);
  }

  void timerCallback() override {
    advanceSceneFaderCrossfade(0.1f);
    if (engine.auxRoutingDirty.exchange(false, std::memory_order_relaxed))
      engine.syncAuxRoutingSnapshot();
    else
      engine.retryPendingAuxRouting();

    if (engine.matrixRoutingDirty.exchange(false, std::memory_order_relaxed))
      engine.syncMatrixRoutingSnapshot();
    else
      engine.retryPendingMatrixRouting();

    if (engine.midiLearnedDirty.exchange(false, std::memory_order_relaxed)) {
      saveMidiMappings();
      status.setText("MIDI CC " +
                         juce::String(midiCallback.getLastLearnedCC()) +
                         " MAPPED — saved",
                     juce::dontSendNotification);
    }
    if (midasSurface != nullptr)
      return;
    if (midiCallback.isLearnArmed())
      midiLearnButton.setButtonText("WAITING...");
    else
      midiLearnButton.setButtonText("LEARN CC");

    auto dbfsText = [](float peak) {
      if (peak <= 1.0e-6f)
        return juce::String("-inf dBFS");
      return juce::String(juce::Decibels::gainToDecibels(peak), 1) + " dBFS";
    };
    const bool mainLClip = engine.mainClipL.load(std::memory_order_relaxed);
    const bool mainRClip = engine.mainClipR.load(std::memory_order_relaxed);
    mainReadoutL.setText(
        "L  " + dbfsText(engine.masterPeakL.load(std::memory_order_relaxed)),
        juce::dontSendNotification);
    mainReadoutR.setText(
        "R  " + dbfsText(engine.masterPeakR.load(std::memory_order_relaxed)),
        juce::dontSendNotification);
    mainClipReadout.setText((mainLClip || mainRClip)
                                ? ("CLIP " +
                                   juce::String(mainLClip ? "L " : "") +
                                   juce::String(mainRClip ? "R" : ""))
                                : "OUTPUT OK",
                            juce::dontSendNotification);
    mainClipReadout.setColour(juce::Label::textColourId,
                              (mainLClip || mainRClip)
                                  ? juce::Colour(0xfff05a67)
                                  : juce::Colour(0xff48d597));
    for (int i = 0; i < kVisibleAuxSends + kVisibleMatrixBuses; ++i) {
      const bool isAux = i < kVisibleAuxSends;
      const float peak =
          isAux ? engine.auxPeak[static_cast<size_t>(i)].load(
                      std::memory_order_relaxed)
                : engine.matrixPeak[static_cast<size_t>(i - kVisibleAuxSends)]
                      .load(std::memory_order_relaxed);
      const bool clipping =
          isAux ? engine.auxClip[static_cast<size_t>(i)].load(
                      std::memory_order_relaxed)
                : engine.matrixClip[static_cast<size_t>(i - kVisibleAuxSends)]
                      .load(std::memory_order_relaxed);
      busReadout[static_cast<size_t>(i)]->setText((clipping ? "CLIP " : "") +
                                                      dbfsText(peak),
                                                  juce::dontSendNotification);
      busReadout[static_cast<size_t>(i)]->setColour(
          juce::Label::textColourId,
          clipping ? juce::Colour(0xfff05a67) : juce::Colour(0xfff0f3f6));
    }
    const bool active = engine.running.load();
    if (engine.deviceFailure.load(std::memory_order_relaxed)) {
      status.setText("AUDIO DEVICE ERROR  |  Open AUDIO INFO / SETTINGS",
                     juce::dontSendNotification);
      status.setColour(juce::Label::textColourId, juce::Colour(0xfff05a67));
    } else if (engine.recorder.isRecording())
      status.setText("RECORDING WAV  |  " +
                         juce::String(static_cast<juce::int64>(
                             engine.recorder.getRecordedFrames())) +
                         " frames written  |  " +
                         juce::String((int)engine.sampleRate.load()) + " Hz",
                     juce::dontSendNotification);
    else if (!active) {
      status.setText("ENGINE STOPPED  |  Select/reconnect an audio device",
                     juce::dontSendNotification);
      status.setColour(juce::Label::textColourId, juce::Colour(0xffffc857));
    } else if (!status.getText().startsWith("SESSION SAVED:") &&
               !status.getText().startsWith("SESSION LOADED:") &&
               !status.getText().startsWith("SAVE FAILED:") &&
               !status.getText().startsWith("LOAD FAILED:") &&
               !status.getText().startsWith("RECORDING STOPPED") &&
               !status.getText().startsWith("RECORD FAILED:") &&
               !status.getText().startsWith("MIDI LEARN") &&
               !status.getText().startsWith("MIDI CC") &&
               !status.getText().startsWith("MIDI MAP") &&
               !status.getText().startsWith("CUE SCENE") &&
               !status.getText().startsWith("CUE RECALL FAILED"))
      status.setText(active ? "ENGINE ONLINE  |  " +
                                  juce::String((int)engine.sampleRate.load()) +
                                  " Hz  |  " +
                                  juce::String(engine.blockSize.load()) +
                                  " samples"
                            : "ENGINE STOPPED  |  Select an audio device",
                     juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, active
                                                    ? juce::Colour(0xff48d597)
                                                    : juce::Colour(0xffffc857));
    // Reset peak holds slowly so the meters decay instead of sticking forever.
    for (auto &ch : engine.channels)
      ch.peak.store(ch.peak.load() * 0.88f);
    updateBankLabel();
    engine.masterPeakL.store(engine.masterPeakL.load() * 0.88f);
    engine.masterPeakR.store(engine.masterPeakR.load() * 0.88f);
    for (auto &peak : engine.auxPeak)
      peak.store(peak.load() * 0.88f);
    for (auto &peak : engine.matrixPeak)
      peak.store(peak.load() * 0.88f);

    const int currentSpill =
        engine.activeSpillVca.load(std::memory_order_relaxed);
    const int currentPopSpill =
        engine.activeSpillPop.load(std::memory_order_relaxed);
    if (currentSpill != lastObservedSpillVca ||
        currentPopSpill != lastObservedSpillPop) {
      lastObservedSpillVca = currentSpill;
      lastObservedSpillPop = currentPopSpill;
      if (midasSurface == nullptr) {
        exitSpillButton.setVisible(currentSpill > 0 || currentPopSpill > 0);
        updateBankVisibility();
        resized();
        repaint();
      }
    }
    for (size_t g = 0; g < 8; ++g)
      updateMuteGroupButton(g);
  }

  MixerEngine &engine;
  juce::AudioDeviceManager &deviceManager;
  MixerMidiCallback &midiCallback;
  SecondaryAudioStreamer *secondaryAudioStreamer{nullptr};
  std::unique_ptr<MidasTouringConsoleSurface> midasSurface;
  int activeSelectedChannel = 0;
  juce::Component::SafePointer<juce::DialogWindow> channelAiDialog;
  std::array<juce::Component::SafePointer<juce::DialogWindow>, 6> hwFxDialogs;
  juce::LookAndFeel_V4 lookAndFeel;
  juce::Label title, status, masterLabel;
  juce::TextButton settings, diagnosticsButton, offlineBounceButton,
      eqWindowButton, dynEqWindowButton, dynamicsWindowButton,
      selectedChannelButton, channelAiButton, vcaWindowButton, matrixGridButton,
      oscillatorButton, geqButton, monitorButton, fxRackButton, manchinoButton,
      duganButton, stageBoxPatchButton, surroundPanButton, talkbackButton,
      oscButton, dawStreamButton, popGroupsButton,
      cueListButton, asioBridgeButton, saveSession, loadSession, recordButton,
      stopRecordButton, loadWavButton, playWavButton, stopWavButton;
  juce::ComboBox sceneSlot, midiTarget;
  juce::TextButton saveSceneButton, recallSceneButton, saveShowButton,
      loadShowButton, midiLearnButton;
  juce::Slider crossfadeSlider;
  std::array<juce::String, 8> sceneData{};
  std::array<std::unique_ptr<ChannelStrip>, kLogicalChannels> strips;
  juce::ComboBox pageSize, faderModeSelector, faderLayoutSelector;
  juce::TextButton editFaderLayoutButton;
  UserFaderLayoutEditor::Layouts customFaderLayouts{};
  int activeFaderLayout = -1;
  std::atomic<int> faderMode{0};
  juce::TextButton previousBank, nextBank;
  juce::Label bankLabel;
  int pageCount = 8;
  int currentBank = 0;
  juce::Slider master;
  juce::Label mainReadoutL, mainReadoutR, mainClipReadout;
  juce::TextButton clearClipButton;
  juce::TextButton exitSofButton;
  juce::TextButton exitSpillButton;
  std::array<std::unique_ptr<juce::TextButton>, 8> muteGroupButtons;
  int lastObservedSpillVca = 0;
  int lastObservedSpillPop = 0;
  // Phase 89: set while an offline bounce owns the engine (live audio paused).
  bool audioSuspendedForBounce = false;
  std::array<std::unique_ptr<juce::TextButton>, kVisibleAuxSends> auxSofButtons;
  juce::ComboBox auxMasterBankSelector;
  std::array<std::unique_ptr<juce::Label>,
             kVisibleAuxSends + kVisibleMatrixBuses>
      busReadout;

  void syncAuxMasterControls() {
    const int bank = juce::jlimit(0, kAuxBusCapacity / kVisibleAuxSends - 1,
                                  auxMasterBankSelector.getSelectedId() - 1);
    for (int slot = 0; slot < kVisibleAuxSends; ++slot) {
      const auto busIndex = static_cast<size_t>(bank * kVisibleAuxSends + slot);
      if (busMaster[static_cast<size_t>(slot)] != nullptr)
        busMaster[static_cast<size_t>(slot)]->setValue(
            engine.auxMasterDb[busIndex].load(std::memory_order_relaxed),
            juce::dontSendNotification);
      if (busMute[static_cast<size_t>(slot)] != nullptr) {
        const auto label = "A" + juce::String(busIndex + 1);
        busMute[static_cast<size_t>(slot)]->setButtonText(label);
        busMute[static_cast<size_t>(slot)]->setTooltip(
            "Mute AUX " + juce::String(busIndex + 1));
        busMute[static_cast<size_t>(slot)]->setToggleState(
            engine.auxMuted[busIndex].load(std::memory_order_relaxed),
            juce::dontSendNotification);
      }
      if (busReadout[static_cast<size_t>(slot)] != nullptr)
        busReadout[static_cast<size_t>(slot)]->setText(
            juce::String(
                engine.auxMasterDb[busIndex].load(std::memory_order_relaxed),
                1) +
                " dB",
            juce::dontSendNotification);
    }
  }

  void createAuxBusControls(int slot) {
    const auto index = static_cast<size_t>(slot);
    busReadout[index] = std::make_unique<juce::Label>();
    busReadout[index]->setColour(juce::Label::textColourId,
                                 juce::Colour(0xfff0f3f6));
    busReadout[index]->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*busReadout[index]);
    busMaster[index] = std::make_unique<juce::Slider>();
    auto &slider = *busMaster[index];
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setRange(-60.0, 10.0, 0.1);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.onValueChange = [this, slot, &slider] {
      const int bank = juce::jlimit(0, kAuxBusCapacity / kVisibleAuxSends - 1,
                                    auxMasterBankSelector.getSelectedId() - 1);
      const auto busIndex = static_cast<size_t>(bank * kVisibleAuxSends + slot);
      engine.auxMasterDb[busIndex].store(static_cast<float>(slider.getValue()),
                                         std::memory_order_relaxed);
      engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncAuxRoutingSnapshot();
      syncAuxMasterControls();
    };
    addAndMakeVisible(slider);

    busMute[index] = std::make_unique<juce::TextButton>();
    auto &button = *busMute[index];
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonOnColourId,
                     juce::Colour(0xffd64d5b));
    button.onClick = [this, slot, &button] {
      const int bank = juce::jlimit(0, kAuxBusCapacity / kVisibleAuxSends - 1,
                                    auxMasterBankSelector.getSelectedId() - 1);
      const auto busIndex = static_cast<size_t>(bank * kVisibleAuxSends + slot);
      engine.auxMuted[busIndex].store(button.getToggleState(),
                                      std::memory_order_relaxed);
      engine.auxRoutingDirty.store(true, std::memory_order_relaxed);
      engine.syncAuxRoutingSnapshot();
    };
    addAndMakeVisible(button);
  }

  void createBusControls(int index, const juce::String &label,
                         std::atomic<float> &gainValue,
                         std::atomic<bool> &muteValue) {
    busReadout[static_cast<size_t>(index)] = std::make_unique<juce::Label>();
    busReadout[static_cast<size_t>(index)]->setColour(juce::Label::textColourId,
                                                      juce::Colour(0xfff0f3f6));
    busReadout[static_cast<size_t>(index)]->setJustificationType(
        juce::Justification::centredLeft);
    addAndMakeVisible(*busReadout[static_cast<size_t>(index)]);
    busMaster[static_cast<size_t>(index)] = std::make_unique<juce::Slider>();
    auto &slider = *busMaster[static_cast<size_t>(index)];
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setRange(-60.0, 10.0, 0.1);
    slider.setValue(gainValue.load(), juce::dontSendNotification);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(label + " master level (dB)");
    slider.onValueChange = [&gainValue, &slider] {
      gainValue.store(static_cast<float>(slider.getValue()),
                      std::memory_order_relaxed);
    };
    addAndMakeVisible(slider);

    busMute[static_cast<size_t>(index)] =
        std::make_unique<juce::TextButton>(label);
    auto &button = *busMute[static_cast<size_t>(index)];
    button.setClickingTogglesState(true);
    button.setToggleState(muteValue.load(), juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonOnColourId,
                     juce::Colour(0xffd64d5b));
    button.setTooltip("Mute " + label);
    button.onClick = [&muteValue, &button] {
      muteValue.store(button.getToggleState(), std::memory_order_relaxed);
    };
    addAndMakeVisible(button);
  }

  Meter masterMeterL, masterMeterR;
  std::array<std::unique_ptr<Meter>, kVisibleAuxSends> auxMeters;
  std::array<std::unique_ptr<Meter>, kVisibleMatrixBuses> matrixMeters;
  std::array<std::unique_ptr<juce::Slider>,
             kVisibleAuxSends + kVisibleMatrixBuses>
      busMaster;
  std::array<std::unique_ptr<juce::TextButton>,
             kVisibleAuxSends + kVisibleMatrixBuses>
      busMute;

  void updateMuteGroupButton(size_t g) {
    if (g >= muteGroupButtons.size() || muteGroupButtons[g] == nullptr)
      return;
    const bool active =
        engine.muteGroupMasters[g].load(std::memory_order_relaxed);
    muteGroupButtons[g]->setButtonText(active ? "MG " + juce::String(g + 1) +
                                                    " [M]"
                                              : "MG " + juce::String(g + 1));
    muteGroupButtons[g]->setColour(juce::TextButton::buttonColourId,
                                   active ? juce::Colour(0xffe53935)
                                          : juce::Colour(0xff2d3540));
    muteGroupButtons[g]->setColour(juce::TextButton::textColourOffId,
                                   active ? juce::Colour(0xffffffff)
                                          : juce::Colour(0xfff0f3f6));
  }

  void exitSpill() {
    engine.activeSpillVca.store(0, std::memory_order_relaxed);
    engine.activeSpillPop.store(0, std::memory_order_relaxed);
    lastObservedSpillVca = 0;
    lastObservedSpillPop = 0;
    currentBank = 0;
    exitSpillButton.setVisible(false);
    faderLayoutSelector.setEnabled(true);
    editFaderLayoutButton.setEnabled(true);
    updateBankVisibility();
    resized();
    repaint();
  }

  std::vector<int> getSpillMembers() const {
    std::vector<int> members;
    const int spillVca = engine.activeSpillVca.load(std::memory_order_relaxed);
    const int spillPop = engine.activeSpillPop.load(std::memory_order_relaxed);
    if (spillVca >= 1 && spillVca <= 24) {
      for (int c = 0; c < kLogicalChannels; ++c) {
        if (engine.channels[static_cast<size_t>(c)].vcaGroup.load(
                std::memory_order_relaxed) == spillVca)
          members.push_back(c);
      }
    } else if (spillPop >= 1 && spillPop <= 24) {
      for (int c = 0; c < kLogicalChannels; ++c) {
        if (engine.channels[static_cast<size_t>(c)].popGroup.load(
                std::memory_order_relaxed) == spillPop)
          members.push_back(c);
      }
    }
    return members;
  }

  void setSofMode(int mode) {
    const int safeMode = juce::jlimit(0, kAuxBusCapacity, mode);
    faderMode.store(safeMode, std::memory_order_relaxed);
    faderModeSelector.setSelectedId(safeMode + 1, juce::dontSendNotification);
    for (int a = 0; a < kVisibleAuxSends; ++a) {
      if (auxSofButtons[static_cast<size_t>(a)] != nullptr) {
        const bool onThis = (safeMode == a + 1);
        auxSofButtons[static_cast<size_t>(a)]->setColour(
            juce::TextButton::buttonColourId,
            onThis ? juce::Colour(0xffffc857) : juce::Colour(0xff2d3540));
        auxSofButtons[static_cast<size_t>(a)]->setColour(
            juce::TextButton::textColourOffId,
            onThis ? juce::Colour(0xff12161a) : juce::Colour(0xfff0f3f6));
      }
    }
    exitSofButton.setVisible(safeMode > 0);
    for (auto &strip : strips)
      strip->syncFromState();
    status.setText(safeMode == 0 ? "MAIN FADERS ACTIVE"
                                 : "SENDS ON FADERS ACTIVE — AUX " +
                                       juce::String(safeMode),
                   juce::dontSendNotification);
    resized();
    repaint();
  }

  bool keyPressed(const juce::KeyPress &key) override {
    if (key.isKeyCode(juce::KeyPress::escapeKey)) {
      bool handled = false;
      if (engine.activeSpillVca.load(std::memory_order_relaxed) > 0) {
        exitSpill();
        handled = true;
      }
      if (faderMode.load(std::memory_order_relaxed) > 0) {
        setSofMode(0);
        handled = true;
      }
      if (handled)
        return true;
    }
    return false;
  }

  void setPageSize(int requested) {
    pageCount = requested == 16 || requested == 24 ? requested : 8;
    const bool isSpilling =
        (engine.activeSpillVca.load(std::memory_order_relaxed) > 0 ||
         engine.activeSpillPop.load(std::memory_order_relaxed) > 0);
    const int totalCount = isSpilling
                               ? static_cast<int>(getSpillMembers().size())
                               : normalChannelCount();
    const int lastBank = juce::jmax(0, (totalCount - 1) / pageCount);
    currentBank = juce::jmin(currentBank, lastBank);
    updateBankVisibility();
    resized();
  }

  void changeBank(int direction) {
    const bool isSpilling =
        (engine.activeSpillVca.load(std::memory_order_relaxed) > 0 ||
         engine.activeSpillPop.load(std::memory_order_relaxed) > 0);
    const int totalCount = isSpilling
                               ? static_cast<int>(getSpillMembers().size())
                               : normalChannelCount();
    const int lastBank = juce::jmax(0, (totalCount - 1) / pageCount);
    currentBank = juce::jlimit(0, lastBank, currentBank + direction);
    updateBankVisibility();
    resized();
  }

  void updateBankVisibility() {
    if (midasSurface != nullptr)
      return;
    const bool isSpilling =
        (engine.activeSpillVca.load(std::memory_order_relaxed) > 0 ||
         engine.activeSpillPop.load(std::memory_order_relaxed) > 0);
    if (isSpilling) {
      const auto members = getSpillMembers();
      const int totalMembers = static_cast<int>(members.size());
      const int first = currentBank * pageCount;
      const int last = juce::jmin(totalMembers, first + pageCount);

      for (int i = 0; i < kLogicalChannels; ++i)
        strips[static_cast<size_t>(i)]->setVisible(false);

      for (int m = first; m < last; ++m) {
        const int chIdx = members[static_cast<size_t>(m)];
        strips[static_cast<size_t>(chIdx)]->setVisible(true);
      }
      updateBankLabel();
      return;
    }

    const int first = currentBank * pageCount;
    const int last = juce::jmin(normalChannelCount(), first + pageCount);
    for (int i = 0; i < kLogicalChannels; ++i)
      strips[static_cast<size_t>(i)]->setVisible(false);
    for (int position = first; position < last; ++position) {
      const int channel = normalChannelAt(position);
      if (channel >= 0 && channel < kLogicalChannels)
        strips[static_cast<size_t>(channel)]->setVisible(true);
    }
    updateBankLabel();
  }

  void updateBankLabel() {
    const int spillPop = engine.activeSpillPop.load(std::memory_order_relaxed);
    if (spillPop > 0) {
      const auto members = getSpillMembers();
      const int totalMembers = static_cast<int>(members.size());
      const auto popName =
          engine.popNames[static_cast<size_t>(spillPop - 1)].isEmpty()
              ? "POP " + juce::String(spillPop)
              : engine.popNames[static_cast<size_t>(spillPop - 1)];

      if (totalMembers == 0) {
        bankLabel.setText(
            "[POP " + juce::String(spillPop) + " SPILL] \"" + popName +
                "\" \xe2\x80\x94 NO MEMBER CHANNELS (Press Esc to Exit)",
            juce::dontSendNotification);
        previousBank.setEnabled(false);
        nextBank.setEnabled(false);
      } else {
        const int totalPages =
            juce::jmax(1, (totalMembers + pageCount - 1) / pageCount);
        bankLabel.setText("[POP " + juce::String(spillPop) + " SPILL] \"" +
                              popName + "\" \xe2\x80\x94 " +
                              juce::String(totalMembers) + " CHANNELS (PAGE " +
                              juce::String(currentBank + 1) + "/" +
                              juce::String(totalPages) + ")",
                          juce::dontSendNotification);
        previousBank.setEnabled(currentBank > 0);
        nextBank.setEnabled((currentBank + 1) * pageCount < totalMembers);
      }
      return;
    }

    const int spillVca = engine.activeSpillVca.load(std::memory_order_relaxed);
    if (spillVca > 0) {
      const auto members = getSpillMembers();
      const int totalMembers = static_cast<int>(members.size());
      const auto vcaName =
          engine.vcaNames[static_cast<size_t>(spillVca - 1)].isEmpty()
              ? "VCA " + juce::String(spillVca)
              : engine.vcaNames[static_cast<size_t>(spillVca - 1)];

      if (totalMembers == 0) {
        bankLabel.setText(
            "[VCA " + juce::String(spillVca) + " SPILL] \"" + vcaName +
                "\" \xe2\x80\x94 NO MEMBER CHANNELS (Press Esc to Exit)",
            juce::dontSendNotification);
        previousBank.setEnabled(false);
        nextBank.setEnabled(false);
      } else {
        const int totalPages =
            juce::jmax(1, (totalMembers + pageCount - 1) / pageCount);
        bankLabel.setText("[VCA " + juce::String(spillVca) + " SPILL] \"" +
                              vcaName + "\" \xe2\x80\x94 " +
                              juce::String(totalMembers) + " CHANNELS (PAGE " +
                              juce::String(currentBank + 1) + "/" +
                              juce::String(totalPages) + ")",
                          juce::dontSendNotification);
        previousBank.setEnabled(currentBank > 0);
        nextBank.setEnabled((currentBank + 1) * pageCount < totalMembers);
      }
      return;
    }

    const int totalChannels = normalChannelCount();
    const int totalPages =
        juce::jmax(1, nova::faderLayoutPageCount(totalChannels, pageCount));
    if (activeFaderLayout >= 0) {
      const auto &layout =
          customFaderLayouts[static_cast<size_t>(activeFaderLayout)];
      const auto layoutName =
          layout.name.isNotEmpty()
              ? layout.name
              : "USER " + juce::String(activeFaderLayout + 1);
      bankLabel.setText(layoutName + " — " + juce::String(totalChannels) +
                            " CHANNELS — PAGE " +
                            juce::String(currentBank + 1) + "/" +
                            juce::String(totalPages),
                        juce::dontSendNotification);
    } else {
      const int first = currentBank * pageCount + 1;
      const int last = juce::jmin(totalChannels, (currentBank + 1) * pageCount);
      bankLabel.setText("BANK " + juce::String(currentBank + 1) + " / " +
                            juce::String(totalPages) + "     CHANNELS " +
                            juce::String(first) + "\u2013" + juce::String(last),
                        juce::dontSendNotification);
    }
    previousBank.setEnabled(currentBank > 0);
    nextBank.setEnabled((currentBank + 1) * pageCount < totalChannels);
  }
};

} // namespace nova
