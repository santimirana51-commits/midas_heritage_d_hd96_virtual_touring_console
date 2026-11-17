#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class OscServerPanel final : public juce::Component, private juce::Timer {
public:
  explicit OscServerPanel(MixerEngine &e) : engine(e) {
    title.setText(
        "OSC & WIRELESS IPAD REMOTE PROTOCOL SERVER (MIDAS CHAPTER 27)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText("High-Speed UDP Network Daemon for Wireless Remote Mixing "
                     "(Mixing Station, TouchOSC, Lemur, QLab)",
                     juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    statusLed.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    statusLed.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLed);

    portLabel.setText("UDP Port:", juce::dontSendNotification);
    portLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d1d9));
    addAndMakeVisible(portLabel);

    portEditor.setText("53000");
    portEditor.setInputRestrictions(5, "0123456789");
    addAndMakeVisible(portEditor);

    startBtn.setButtonText("START SERVER");
    startBtn.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff238636));
    startBtn.onClick = [this] {
      const int port = portEditor.getText().getIntValue();
      if (port > 1024 && port < 65535) {
        engine.oscServer.startServer(port);
      }
    };
    addAndMakeVisible(startBtn);

    stopBtn.setButtonText("STOP SERVER");
    stopBtn.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xffda3633));
    stopBtn.onClick = [this] { engine.oscServer.stopServer(); };
    addAndMakeVisible(stopBtn);

    pingBtn.setButtonText("BROADCAST PING (/ping)");
    pingBtn.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xff1f6feb));
    pingBtn.onClick = [this] {
      nova::graviton::OscMessage pingMsg;
      pingMsg.address = "/ping";
      pingMsg.typeTags = "s";
      pingMsg.stringArgs.push_back("NOVA_HD_PING");
      engine.oscServer.sendToAllClients(pingMsg);
      engine.logOsc("TX: /ping broadcast sent");
    };
    addAndMakeVisible(pingBtn);

    clearLogBtn.setButtonText("CLEAR LOG");
    clearLogBtn.onClick = [this] {
      {
        const juce::ScopedLock sl(engine.oscLogLock);
        engine.oscLogMessages.clear();
      }
      logEditor.clear();
    };
    addAndMakeVisible(clearLogBtn);

    statsLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    statsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    statsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statsLabel);

    logEditor.setMultiLine(true);
    logEditor.setReadOnly(true);
    logEditor.setCaretVisible(false);
    logEditor.setColour(juce::TextEditor::backgroundColourId,
                        juce::Colour(0xff161b22));
    logEditor.setColour(juce::TextEditor::textColourId,
                        juce::Colour(0xff58a6ff));
    logEditor.setColour(juce::TextEditor::outlineColourId,
                        juce::Colour(0xff30363d));
    addAndMakeVisible(logEditor);

    guideLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    guideLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    guideLabel.setText("Commands: /ch/{1..144}/fader <0..1> | /ch/{1..144}/pan "
                       "<-1..1> | /ch/{1..144}/mute <0|1> | /ch/{1..144}/solo "
                       "<0|1> | /spatial/{1..144}/xyz <x y z> | /main/fader "
                       "<0..1> | /talkback/on <0|1>",
                       juce::dontSendNotification);
    addAndMakeVisible(guideLabel);

    startTimerHz(10);
  }

  ~OscServerPanel() override { stopTimer(); }

  void timerCallback() override {
    const bool isRunning = engine.oscServer.isServerRunning();
    if (isRunning) {
      statusLed.setText("\xe2\x97\x8f SERVER ONLINE (PORT " +
                            juce::String(engine.oscServer.getServerPort()) +
                            ")",
                        juce::dontSendNotification);
      statusLed.setColour(juce::Label::textColourId, juce::Colour(0xff3fb950));
    } else {
      statusLed.setText("\xe2\x97\x8b SERVER STOPPED",
                        juce::dontSendNotification);
      statusLed.setColour(juce::Label::textColourId, juce::Colour(0xfff85149));
    }

    statsLabel.setText(
        "Connected Clients: " +
            juce::String(engine.oscServer.getActiveClientCount()) +
            "  |  Packets RX: " +
            juce::String(engine.oscServer.getPacketsReceived()) +
            "  |  Packets TX: " +
            juce::String(engine.oscServer.getPacketsSent()),
        juce::dontSendNotification);

    // Refresh log
    juce::String fullText;
    {
      const juce::ScopedLock sl(engine.oscLogLock);
      for (const auto &line : engine.oscLogMessages) {
        fullText << line << "\n";
      }
    }
    if (fullText != logEditor.getText()) {
      logEditor.setText(fullText);
      logEditor.moveCaretToEnd();
    }
  }

  void resized() override {
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(48);
    title.setBounds(header.removeFromTop(24));
    subTitle.setBounds(header.removeFromTop(20));

    auto ctrlRow = area.removeFromTop(36);
    statusLed.setBounds(ctrlRow.removeFromLeft(220));
    portLabel.setBounds(ctrlRow.removeFromLeft(65).reduced(0, 4));
    portEditor.setBounds(ctrlRow.removeFromLeft(70).reduced(2));
    ctrlRow.removeFromLeft(10);
    startBtn.setBounds(ctrlRow.removeFromLeft(120).reduced(2));
    stopBtn.setBounds(ctrlRow.removeFromLeft(110).reduced(2));
    ctrlRow.removeFromLeft(10);
    pingBtn.setBounds(ctrlRow.removeFromLeft(180).reduced(2));
    clearLogBtn.setBounds(ctrlRow.removeFromRight(90).reduced(2));

    area.removeFromTop(8);
    statsLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);

    guideLabel.setBounds(area.removeFromBottom(24));
    area.removeFromBottom(4);

    logEditor.setBounds(area);
  }

private:
  MixerEngine &engine;

  juce::Label title, subTitle, statusLed, portLabel, statsLabel, guideLabel;
  juce::TextEditor portEditor;
  juce::TextButton startBtn, stopBtn, pingBtn, clearLogBtn;
  juce::TextEditor logEditor;
};

// ==============================================================================
// Phase 88: Matrix Plus & Speaker Delay Alignment Network Panel (MIDAS Chapter
// 29)
// ==============================================================================

} // namespace nova
