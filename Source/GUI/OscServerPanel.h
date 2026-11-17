#pragma once
#include "Mixer/MixerEngine.h"
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

/**
 * OSC server control + embedded mirror of the Android remote:
 * 8 channel strips (18 banks x 8 = 144 ch), vertical faders, MUTE/SOLO,
 * Main L/R fader and Talkback. All controls read/write the same atomics the
 * OSC handler uses, so PC and Android always show the same state.
 */
class OscServerPanel final : public juce::Component, private juce::Timer {
public:
  static constexpr int kStrips = 8;
  static constexpr int kBanks = kLogicalChannels / kStrips; // 18

  explicit OscServerPanel(MixerEngine &e) : engine(e) {
    title.setText("OSC SERVER + REMOTE MIRROR (HERITAGE HD96 REMOTE)",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(title);

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
      if (port > 1024 && port < 65535)
        engine.oscServer.startServer(port);
    };
    addAndMakeVisible(startBtn);

    stopBtn.setButtonText("STOP SERVER");
    stopBtn.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xffda3633));
    stopBtn.onClick = [this] { engine.oscServer.stopServer(); };
    addAndMakeVisible(stopBtn);

    pingBtn.setButtonText("BROADCAST PING");
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

    // ---- Bank bar ----
    prevBankBtn.setButtonText("< BANK");
    prevBankBtn.onClick = [this] {
      if (bank > 0) {
        --bank;
        syncFromEngine(true);
      }
    };
    addAndMakeVisible(prevBankBtn);

    nextBankBtn.setButtonText("BANK >");
    nextBankBtn.onClick = [this] {
      if (bank < kBanks - 1) {
        ++bank;
        syncFromEngine(true);
      }
    };
    addAndMakeVisible(nextBankBtn);

    bankLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    bankLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffacc15));
    bankLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bankLabel);

    talkbackBtn.setButtonText("TALKBACK");
    talkbackBtn.setClickingTogglesState(true);
    talkbackBtn.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff334155));
    talkbackBtn.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xffef4444));
    talkbackBtn.onClick = [this] {
      engine.talkbackEngine.isTalking.store(talkbackBtn.getToggleState(),
                                            std::memory_order_relaxed);
    };
    addAndMakeVisible(talkbackBtn);

    // ---- Channel strips ----
    for (int i = 0; i < kStrips; ++i) {
      auto &s = strips[static_cast<size_t>(i)];

      s.name.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
      s.name.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
      s.name.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(s.name);

      s.db.setFont(juce::Font(juce::FontOptions(11.0f)));
      s.db.setColour(juce::Label::textColourId, juce::Colours::white);
      s.db.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(s.db);

      s.mute.setButtonText("MUTE");
      s.mute.setClickingTogglesState(true);
      s.mute.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff1e293b));
      s.mute.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xffef4444));
      s.mute.onClick = [this, i] {
        const int ch = channelFor(i);
        engine.channels[static_cast<size_t>(ch)].muted.store(
            strips[static_cast<size_t>(i)].mute.getToggleState(),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(s.mute);

      s.solo.setButtonText("SOLO");
      s.solo.setClickingTogglesState(true);
      s.solo.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff1e293b));
      s.solo.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xfffacc15));
      s.solo.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
      s.solo.onClick = [this, i] {
        const int ch = channelFor(i);
        engine.channels[static_cast<size_t>(ch)].solo.store(
            strips[static_cast<size_t>(i)].solo.getToggleState(),
            std::memory_order_relaxed);
      };
      addAndMakeVisible(s.solo);

      configureFader(s.fader, juce::Colour(0xff38bdf8));
      s.fader.onValueChange = [this, i] {
        auto &st = strips[static_cast<size_t>(i)];
        const float v = static_cast<float>(st.fader.getValue());
        engine.channels[static_cast<size_t>(channelFor(i))].gainDb.store(
            v, std::memory_order_relaxed);
        st.db.setText(formatDb(v), juce::dontSendNotification);
      };
      addAndMakeVisible(s.fader);
    }

    // ---- Master ----
    masterName.setText("MAIN L/R", juce::dontSendNotification);
    masterName.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    masterName.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    masterName.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(masterName);

    masterDbLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    masterDbLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    masterDbLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(masterDbLabel);

    configureFader(masterFader, juce::Colour(0xfff59e0b));
    masterFader.onValueChange = [this] {
      const float v = static_cast<float>(masterFader.getValue());
      engine.masterDb.store(v, std::memory_order_relaxed);
      masterDbLabel.setText(formatDb(v), juce::dontSendNotification);
    };
    addAndMakeVisible(masterFader);

    // ---- Log ----
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

    syncFromEngine(true);
    startTimerHz(20);
  }

  ~OscServerPanel() override { stopTimer(); }

  void paint(juce::Graphics &g) override {
    g.setColour(juce::Colour(0xff0d1117));
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(0xff161b22));
    for (const auto &r : stripBackgrounds)
      g.fillRoundedRectangle(r.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.fillRoundedRectangle(masterBackground.toFloat(), 4.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);

    title.setBounds(area.removeFromTop(24));

    auto ctrlRow = area.removeFromTop(32);
    statusLed.setBounds(ctrlRow.removeFromLeft(230));
    portLabel.setBounds(ctrlRow.removeFromLeft(65).reduced(0, 4));
    portEditor.setBounds(ctrlRow.removeFromLeft(70).reduced(2));
    ctrlRow.removeFromLeft(8);
    startBtn.setBounds(ctrlRow.removeFromLeft(120).reduced(2));
    stopBtn.setBounds(ctrlRow.removeFromLeft(110).reduced(2));
    pingBtn.setBounds(ctrlRow.removeFromLeft(140).reduced(2));
    clearLogBtn.setBounds(ctrlRow.removeFromRight(90).reduced(2));

    statsLabel.setBounds(area.removeFromTop(20));

    // Log at the bottom
    logEditor.setBounds(area.removeFromBottom(juce::jlimit(70, 120, area.getHeight() / 5)));
    area.removeFromBottom(6);

    // Bank bar
    auto bankRow = area.removeFromTop(32);
    prevBankBtn.setBounds(bankRow.removeFromLeft(90).reduced(2));
    bankLabel.setBounds(bankRow.removeFromLeft(130));
    nextBankBtn.setBounds(bankRow.removeFromLeft(90).reduced(2));
    talkbackBtn.setBounds(bankRow.removeFromRight(120).reduced(2));
    area.removeFromTop(4);

    // Mixer row: 8 strips + master
    const int masterW = juce::jmax(70, area.getWidth() / 9);
    auto masterArea = area.removeFromRight(masterW);
    area.removeFromRight(8);
    masterBackground = masterArea;
    layoutStripColumn(masterArea, masterName, masterDbLabel, nullptr, nullptr,
                      masterFader);

    const int stripW = area.getWidth() / kStrips;
    for (int i = 0; i < kStrips; ++i) {
      auto col = area.removeFromLeft(stripW).reduced(2, 0);
      stripBackgrounds[static_cast<size_t>(i)] = col;
      auto &s = strips[static_cast<size_t>(i)];
      layoutStripColumn(col, s.name, s.db, &s.mute, &s.solo, s.fader);
    }
  }

private:
  struct Strip {
    juce::Label name, db;
    juce::TextButton mute, solo;
    juce::Slider fader;
  };

  static void configureFader(juce::Slider &f, juce::Colour accent) {
    f.setSliderStyle(juce::Slider::LinearVertical);
    f.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    f.setRange(-60.0, 10.0, 0.1);
    f.setDoubleClickReturnValue(true, 0.0);
    f.setColour(juce::Slider::thumbColourId, accent);
    f.setColour(juce::Slider::trackColourId, accent.withAlpha(0.5f));
    f.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff060a12));
  }

  static void layoutStripColumn(juce::Rectangle<int> col, juce::Label &name,
                                juce::Label &db, juce::TextButton *mute,
                                juce::TextButton *solo, juce::Slider &fader) {
    col = col.reduced(4, 6);
    name.setBounds(col.removeFromTop(18));
    db.setBounds(col.removeFromTop(16));
    if (mute != nullptr)
      mute->setBounds(col.removeFromTop(28).reduced(0, 2));
    if (solo != nullptr)
      solo->setBounds(col.removeFromTop(28).reduced(0, 2));
    col.removeFromTop(4);
    fader.setBounds(col);
  }

  static juce::String formatDb(float v) {
    return juce::String(v, 1) + " dB";
  }

  int channelFor(int slot) const noexcept {
    return juce::jlimit(0, kLogicalChannels - 1, bank * kStrips + slot);
  }

  // Pull the current engine state into the widgets. When `force` is true
  // (bank change) fader values are always refreshed; otherwise a fader that is
  // being dragged is left untouched so the user's gesture is never fought.
  void syncFromEngine(bool force) {
    const int start = bank * kStrips;
    bankLabel.setText("CH " + juce::String(start + 1) + "-" +
                          juce::String(start + kStrips),
                      juce::dontSendNotification);

    for (int i = 0; i < kStrips; ++i) {
      auto &s = strips[static_cast<size_t>(i)];
      const int ch = channelFor(i);
      const auto &state = engine.channels[static_cast<size_t>(ch)];

      const auto &nm = engine.channelNames[static_cast<size_t>(ch)];
      s.name.setText(nm.isNotEmpty() ? nm : ("CH " + juce::String(ch + 1)),
                     juce::dontSendNotification);

      const float g = state.gainDb.load(std::memory_order_relaxed);
      if (force || !s.fader.isMouseButtonDown()) {
        if (std::abs(static_cast<float>(s.fader.getValue()) - g) > 0.05f)
          s.fader.setValue(g, juce::dontSendNotification);
        s.db.setText(formatDb(g), juce::dontSendNotification);
      }

      const bool m = state.muted.load(std::memory_order_relaxed);
      if (s.mute.getToggleState() != m)
        s.mute.setToggleState(m, juce::dontSendNotification);
      const bool so = state.solo.load(std::memory_order_relaxed);
      if (s.solo.getToggleState() != so)
        s.solo.setToggleState(so, juce::dontSendNotification);
    }

    const float mdb = engine.masterDb.load(std::memory_order_relaxed);
    if (force || !masterFader.isMouseButtonDown()) {
      if (std::abs(static_cast<float>(masterFader.getValue()) - mdb) > 0.05f)
        masterFader.setValue(mdb, juce::dontSendNotification);
      masterDbLabel.setText(formatDb(mdb), juce::dontSendNotification);
    }

    const bool tb = engine.talkbackEngine.isTalking.load(std::memory_order_relaxed);
    if (talkbackBtn.getToggleState() != tb)
      talkbackBtn.setToggleState(tb, juce::dontSendNotification);
  }

  void timerCallback() override {
    const bool isRunning = engine.oscServer.isServerRunning();
    if (isRunning) {
      statusLed.setText("\xe2\x97\x8f SERVER ONLINE (PORT " +
                            juce::String(engine.oscServer.getServerPort()) + ")",
                        juce::dontSendNotification);
      statusLed.setColour(juce::Label::textColourId, juce::Colour(0xff3fb950));
    } else {
      statusLed.setText("\xe2\x97\x8b SERVER STOPPED", juce::dontSendNotification);
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

    syncFromEngine(false);

    juce::String fullText;
    {
      const juce::ScopedLock sl(engine.oscLogLock);
      for (const auto &line : engine.oscLogMessages)
        fullText << line << "\n";
    }
    if (fullText != logEditor.getText()) {
      logEditor.setText(fullText);
      logEditor.moveCaretToEnd();
    }
  }

  MixerEngine &engine;
  int bank = 0;

  juce::Label title, statusLed, portLabel, statsLabel, bankLabel, masterName,
      masterDbLabel;
  juce::TextEditor portEditor, logEditor;
  juce::TextButton startBtn, stopBtn, pingBtn, clearLogBtn, prevBankBtn,
      nextBankBtn, talkbackBtn;
  std::array<Strip, kStrips> strips;
  juce::Slider masterFader;
  std::array<juce::Rectangle<int>, kStrips> stripBackgrounds{};
  juce::Rectangle<int> masterBackground;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscServerPanel)
};

} // namespace nova
