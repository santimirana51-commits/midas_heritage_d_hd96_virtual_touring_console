#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class ASIOBridgePanel final : public juce::Component, private juce::Timer {
public:
  explicit ASIOBridgePanel(MixerEngine &e) : engine(e) {
    title.setText(
        "MULTICHANNEL USB AUDIO CLASS 2.0 ASIO BRIDGE (MIDAS CHAPTER 26)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText("Lock-Free 64k Ring Buffer Stereo Bridge \u2014 Device "
                     "Control & Latency Monitor",
                     juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    statusLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff58a6ff));
    addAndMakeVisible(statusLabel);

    latencyLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff3fb950));
    addAndMakeVisible(latencyLabel);

    sampleRateLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    sampleRateLabel.setColour(juce::Label::textColourId,
                              juce::Colour(0xffd2a8ff));
    addAndMakeVisible(sampleRateLabel);

    bufferInfoLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    bufferInfoLabel.setColour(juce::Label::textColourId,
                              juce::Colour(0xff8b949e));
    addAndMakeVisible(bufferInfoLabel);

    deviceCombo.addItem("Default ASIO Device", 1);
    deviceCombo.addItem("Focusrite Scarlett 18i20", 2);
    deviceCombo.addItem("RME Fireface UCX II", 3);
    deviceCombo.addItem("Universal Audio Apollo x16", 4);
    deviceCombo.addItem("MOTU 828es", 5);
    deviceCombo.addItem("Dante Virtual Soundcard", 6);
    deviceCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(deviceCombo);

    deviceLabel.setText("Device:", juce::dontSendNotification);
    deviceLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    deviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d1d9));
    addAndMakeVisible(deviceLabel);

    startButton.setButtonText("START");
    startButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff238636));
    startButton.onClick = [this] {
      auto &bridge = engine.effectsRack.getSlot(0).asioBridge;
      bridge.start();
      updateReadouts();
    };
    addAndMakeVisible(startButton);

    stopButton.setButtonText("STOP");
    stopButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xffda3633));
    stopButton.onClick = [this] {
      auto &bridge = engine.effectsRack.getSlot(0).asioBridge;
      bridge.stop();
      updateReadouts();
    };
    addAndMakeVisible(stopButton);

    resetButton.setButtonText("RESET");
    resetButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff6e7681));
    resetButton.onClick = [this] {
      auto &bridge = engine.effectsRack.getSlot(0).asioBridge;
      bridge.stop();
      bridge.reset();
      updateReadouts();
    };
    addAndMakeVisible(resetButton);

    updateReadouts();
    startTimerHz(10);
  }

  ~ASIOBridgePanel() override { stopTimer(); }

  void timerCallback() override { updateReadouts(); }

  void updateReadouts() {
    auto &bridge = engine.effectsRack.getSlot(0).asioBridge;
    statusLabel.setText(juce::String("Status: ") + bridge.status(),
                        juce::dontSendNotification);
    latencyLabel.setText("Ring Buffer Latency: " +
                             juce::String(bridge.getLatencyMs(), 2) + " ms",
                         juce::dontSendNotification);
    sampleRateLabel.setText(
        "Sample Rate: " +
            juce::String(static_cast<int>(bridge.getSampleRate())) + " Hz",
        juce::dontSendNotification);
    bufferInfoLabel.setText(
        "Buffer: 64k samples / " +
            juce::String(
                static_cast<int>(nova::graviton::ASIOBridge::kBufferSize)) +
            " | Write: " +
            juce::String(static_cast<int>(bridge.getWritePos())) +
            " | Read: " + juce::String(static_cast<int>(bridge.getReadPos())),
        juce::dontSendNotification);

    statusLabel.setColour(juce::Label::textColourId,
                          bridge.isRunning() ? juce::Colour(0xff3fb950)
                                             : juce::Colour(0xffda3633));
  }

  void resized() override {
    const bool isCompact = (getHeight() < 340);
    auto area = getLocalBounds().reduced(isCompact ? 6 : 12);
    title.setBounds(area.removeFromTop(isCompact ? 18 : 24));
    if (!isCompact) {
      subTitle.setBounds(area.removeFromTop(18));
      area.removeFromTop(6);
    } else {
      subTitle.setBounds(0, 0, 0, 0);
      area.removeFromTop(2);
    }

    if (isCompact) {
      const int colW = area.getWidth() / 2;
      auto leftCol = area.removeFromLeft(colW).reduced(2);
      statusLabel.setBounds(leftCol.removeFromTop(18));
      latencyLabel.setBounds(leftCol.removeFromTop(18));
      sampleRateLabel.setBounds(leftCol.removeFromTop(18));
      bufferInfoLabel.setBounds(leftCol.removeFromTop(18));

      auto rightCol = area.reduced(2);
      auto deviceRow = rightCol.removeFromTop(24);
      deviceLabel.setBounds(deviceRow.removeFromLeft(60));
      deviceCombo.setBounds(deviceRow);

      rightCol.removeFromTop(6);
      auto btnRow = rightCol.removeFromTop(26);
      const int bW = std::max(60, btnRow.getWidth() / 3);
      startButton.setBounds(btnRow.removeFromLeft(bW).reduced(1));
      stopButton.setBounds(btnRow.removeFromLeft(bW).reduced(1));
      resetButton.setBounds(btnRow.reduced(1));
    } else {
      statusLabel.setBounds(area.removeFromTop(22));
      latencyLabel.setBounds(area.removeFromTop(20));
      sampleRateLabel.setBounds(area.removeFromTop(20));
      bufferInfoLabel.setBounds(area.removeFromTop(18));
      area.removeFromTop(8);

      auto deviceRow = area.removeFromTop(26);
      deviceLabel.setBounds(deviceRow.removeFromLeft(60));
      deviceCombo.setBounds(deviceRow.removeFromLeft(300));
      area.removeFromTop(8);

      auto btnRow = area.removeFromTop(32);
      startButton.setBounds(btnRow.removeFromLeft(100).reduced(2));
      stopButton.setBounds(btnRow.removeFromLeft(100).reduced(2));
      resetButton.setBounds(btnRow.removeFromLeft(100).reduced(2));
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff161b22));
  }

private:
  MixerEngine &engine;
  juce::Label title, subTitle, statusLabel, latencyLabel, sampleRateLabel;
  juce::Label bufferInfoLabel, deviceLabel;
  juce::ComboBox deviceCombo;
  juce::TextButton startButton, stopButton, resetButton;
};

// ==============================================================================
// Phase 89: Offline Mixdown & Multi-Track Stems Bounce Window (Chapter 36)
// Faster-than-realtime export of the stereo Master L/R plus every bus stem
// (AUX / MATRIX) from a multitrack source file to WAV or FLAC.
// ==============================================================================

} // namespace nova
