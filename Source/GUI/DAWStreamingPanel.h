#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class DAWStreamingPanel final : public juce::Component, private juce::Timer {
public:
  explicit DAWStreamingPanel(MixerEngine &e) : engine(e) {
    title.setText("DAW 64\xc3\x97\x36\x34 USB MULTI-TRACK STREAMER & VIRTUAL "
                  "SOUNDCHECK (MIDAS CHAPTER 26)",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText(
        "1-Click Global Virtual Soundcheck Switching, 64-Channel ASIO "
        "Multi-Track I/O, Buffer Latency & Drop-out xRun Monitor",
        juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    // Global Virtual Soundcheck Button
    const bool isVsc = engine.dawStreamer.isVirtualSoundcheckActive();
    vscToggleButton.setButtonText(
        isVsc ? "GLOBAL VIRTUAL SOUNDCHECK: ACTIVE (PLAYBACK)"
              : "GLOBAL VIRTUAL SOUNDCHECK: OFF (STAGE MIC)");
    vscToggleButton.setClickingTogglesState(true);
    vscToggleButton.setToggleState(isVsc, juce::dontSendNotification);
    vscToggleButton.setColour(juce::TextButton::buttonOnColourId,
                              juce::Colour(0xffd97706)); // Amber
    vscToggleButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff1f2937));
    vscToggleButton.onClick = [this] {
      const bool active = vscToggleButton.getToggleState();
      engine.dawStreamer.setVirtualSoundcheck(active);
      vscToggleButton.setButtonText(
          active ? "GLOBAL VIRTUAL SOUNDCHECK: ACTIVE (PLAYBACK)"
                 : "GLOBAL VIRTUAL SOUNDCHECK: OFF (STAGE MIC)");
    };
    addAndMakeVisible(vscToggleButton);

    // DAW Stream Master Toggle
    const bool isStream = engine.dawStreamer.isStreaming();
    streamToggleButton.setButtonText(isStream
                                         ? "64x64 MULTI-TRACK STREAM: RUNNING"
                                         : "64x64 MULTI-TRACK STREAM: STOPPED");
    streamToggleButton.setClickingTogglesState(true);
    streamToggleButton.setToggleState(isStream, juce::dontSendNotification);
    streamToggleButton.setColour(juce::TextButton::buttonOnColourId,
                                 juce::Colour(0xff22c55e)); // Green
    streamToggleButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff374151));
    streamToggleButton.onClick = [this] {
      const bool active = streamToggleButton.getToggleState();
      if (active)
        engine.dawStreamer.startStreaming();
      else
        engine.dawStreamer.stopStreaming();
      streamToggleButton.setButtonText(
          active ? "64x64 MULTI-TRACK STREAM: RUNNING"
                 : "64x64 MULTI-TRACK STREAM: STOPPED");
    };
    addAndMakeVisible(streamToggleButton);

    // Auto-Map 1:1 Channels Button
    autoMapButton.setButtonText("AUTO-MAP 1:1 CHANNELS");
    autoMapButton.setTooltip("Reset DAW Multi-track I/O Patch bay to identity "
                             "1:1 mapping (CH 1-64 -> DAW 1-64)");
    autoMapButton.onClick = [this] {
      for (int i = 0; i < 64; ++i) {
        engine.dawStreamer.setInputRouting(i, i);
        engine.dawStreamer.setOutputRouting(i, i);
      }
    };
    addAndMakeVisible(autoMapButton);

    // Clear xRun Button
    clearXRunButton.setButtonText("RESET xRUNs");
    clearXRunButton.setTooltip(
        "Clear ASIO audio engine buffer overflow/underflow counter");
    clearXRunButton.onClick = [this] { engine.dawStreamer.clearXRunCount(); };
    addAndMakeVisible(clearXRunButton);

    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(statusLabel);

    gridEditor.setMultiLine(true);
    gridEditor.setReadOnly(true);
    gridEditor.setScrollbarsShown(true);
    gridEditor.setFont(juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    gridEditor.setColour(juce::TextEditor::backgroundColourId,
                         juce::Colour(0xff0d1117));
    gridEditor.setColour(juce::TextEditor::textColourId,
                         juce::Colour(0xffc9d1d9));
    addAndMakeVisible(gridEditor);

    startTimerHz(10);
  }

  void timerCallback() override {
    const bool isVsc = engine.dawStreamer.isVirtualSoundcheckActive();
    const bool isStream = engine.dawStreamer.isStreaming();
    const int xRuns = engine.dawStreamer.getXRunCount();
    const double sr = engine.sampleRate.load(std::memory_order_relaxed);
    const double rttMs = (sr > 1000.0) ? (4096.0 / sr * 1000.0) : 42.66;

    juce::String stat =
        "STREAM BUFFER: 4096 smp | SR: " + juce::String(static_cast<int>(sr)) +
        " Hz | RTT Latency: ~" + juce::String(rttMs, 1) +
        " ms | xRun Drops: " + juce::String(xRuns) +
        (xRuns > 0 ? " [WARN: Buffer Glitch]" : " [OK: Zero Glitch]");
    statusLabel.setText(stat, juce::dontSendNotification);

    juce::String grid = "--- 64x64 ASIO DIRECT MULTI-TRACK STREAM & VIRTUAL "
                        "SOUNDCHECK STATUS ---\n";
    grid << "GLOBAL MODE: "
         << (isVsc ? "VIRTUAL SOUNDCHECK (DAW PLAYBACK INTO CONSOLE)"
                   : "LIVE SHOW (PHYSICAL STAGE BOX & ANALOG INPUTS)")
         << "\n";
    grid << "STREAMING ENGINE: "
         << (isStream ? "ACTIVE RECORDING & PLAYBACK" : "STOPPED / STANDBY")
         << "\n\n";
    grid << "CH   | CONSOLE STRIP LABEL  | INPUT SOURCE ROUTE        | DAW "
            "MULTI-TRACK RECORD\n";
    grid << "-----|----------------------|---------------------------|---------"
            "--------------\n";

    for (int ch = 0; ch < 64; ++ch) {
      const auto chName = engine.channelNames[static_cast<size_t>(ch)];
      const juce::String label =
          chName.isEmpty() ? ("CH " + juce::String(ch + 1)) : chName;
      const int dawIn = engine.dawStreamer.getInputRouting(ch);
      const int dawOut = engine.dawStreamer.getOutputRouting(ch);

      juce::String chStr = juce::String(ch + 1).paddedLeft(' ', 3);
      juce::String nameStr = label.paddedRight(' ', 20);
      juce::String inRoute =
          isVsc
              ? ("DAW PLAYBACK " + juce::String(dawIn + 1)).paddedRight(' ', 25)
              : ("STAGE INPUT " + juce::String(ch + 1)).paddedRight(' ', 25);
      juce::String outRoute =
          isStream ? ("REC TO DAW TRK " + juce::String(dawOut + 1)) : "STANDBY";

      grid << chStr << "  | " << nameStr << " | " << inRoute << " | "
           << outRoute << "\n";
    }

    gridEditor.setText(grid);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    title.setBounds(area.removeFromTop(24));
    subTitle.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);

    auto topBar = area.removeFromTop(36);
    vscToggleButton.setBounds(topBar.removeFromLeft(330).reduced(2));
    topBar.removeFromLeft(8);
    streamToggleButton.setBounds(topBar.removeFromLeft(280).reduced(2));
    topBar.removeFromLeft(8);
    autoMapButton.setBounds(topBar.removeFromLeft(170).reduced(2));
    topBar.removeFromLeft(8);
    clearXRunButton.setBounds(topBar.reduced(2));
    area.removeFromTop(8);

    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    gridEditor.setBounds(area);
  }

private:
  MixerEngine &engine;
  juce::Label title, subTitle, statusLabel;
  juce::TextButton vscToggleButton, streamToggleButton, autoMapButton,
      clearXRunButton;
  juce::TextEditor gridEditor;
};

// ==============================================================================
// Phase 98: POPulation Groups Navigation & Multi-Channel Spill (Chapter 11)
// ==============================================================================

} // namespace nova
