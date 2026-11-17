#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class CueListPanel final : public juce::Component, private juce::Timer {
public:
  using CueRecallCallback =
      std::function<bool(int, const nova::graviton::CueSafes &, float)>;
  using StopRecallCallback = std::function<void()>;

  explicit CueListPanel(MixerEngine &e, CueRecallCallback recallCue = {},
                        StopRecallCallback stopRecall = {})
      : engine(e), recallCue(std::move(recallCue)),
        stopRecall(std::move(stopRecall)) {
    title.setText(
        "SCENE AUTOMATION CUE LIST & MIDI SHOW CONTROL (MIDAS CHAPTER 34)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText("Automated Theater / Concert Show Sequence, Granular "
                     "Recall Safes & Crossfade Glide Engine",
                     juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    goButton.setButtonText("GO >>>");
    goButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff238636));
    goButton.onClick = [this] {
      if (engine.cueManager.fireGo())
        recallCurrentCue();
      updateReadouts();
    };
    addAndMakeVisible(goButton);

    stopButton.setButtonText("STOP");
    stopButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xffda3633));
    stopButton.onClick = [this] {
      engine.cueManager.fireStop();
      if (this->stopRecall)
        this->stopRecall();
      updateReadouts();
    };
    addAndMakeVisible(stopButton);

    prevButton.setButtonText("<<< PREV");
    prevButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xffd29922));
    prevButton.onClick = [this] {
      const int previousIndex = engine.cueManager.getCurrentCueIndex();
      engine.cueManager.firePrev();
      if (engine.cueManager.getCurrentCueIndex() != previousIndex)
        recallCurrentCue();
      updateReadouts();
    };
    addAndMakeVisible(prevButton);

    activeCueLabel.setFont(
        juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    activeCueLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xff38bdf8));
    addAndMakeVisible(activeCueLabel);

    for (int cueIndex = 0; cueIndex < engine.cueManager.getNumCues();
         ++cueIndex) {
      const auto &cue = engine.cueManager.getCue(cueIndex);
      cueSafeTarget.addItem("EDIT " + juce::String(cue.cueNumber, 1),
                            cueIndex + 1);
    }
    cueSafeTarget.setSelectedId(engine.cueManager.getCurrentCueIndex() + 1,
                                juce::dontSendNotification);
    cueSafeTarget.onChange = [this] { syncCueSafes(editableCue().safes); };
    addAndMakeVisible(cueSafeTarget);

    nextCueLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    nextCueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(nextCueLabel);

    fadersSafeBtn.setButtonText("FADERS SAFE");
    fadersSafeBtn.setClickingTogglesState(true);
    fadersSafeBtn.setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(0xff1f6feb));
    fadersSafeBtn.onClick = [this] {
      editableCue().safes.safeFaders = fadersSafeBtn.getToggleState();
    };
    addAndMakeVisible(fadersSafeBtn);

    eqSafeBtn.setButtonText("EQ SAFE");
    eqSafeBtn.setClickingTogglesState(true);
    eqSafeBtn.setColour(juce::TextButton::buttonOnColourId,
                        juce::Colour(0xff1f6feb));
    eqSafeBtn.onClick = [this] {
      editableCue().safes.safeEQ = eqSafeBtn.getToggleState();
    };
    addAndMakeVisible(eqSafeBtn);

    dynSafeBtn.setButtonText("DYNAMICS SAFE");
    dynSafeBtn.setClickingTogglesState(true);
    dynSafeBtn.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour(0xff1f6feb));
    dynSafeBtn.onClick = [this] {
      editableCue().safes.safeDynamics = dynSafeBtn.getToggleState();
    };
    addAndMakeVisible(dynSafeBtn);

    routingSafeBtn.setButtonText("ROUTING SAFE");
    routingSafeBtn.setClickingTogglesState(true);
    routingSafeBtn.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff1f6feb));
    routingSafeBtn.onClick = [this] {
      editableCue().safes.safeRouting = routingSafeBtn.getToggleState();
    };
    addAndMakeVisible(routingSafeBtn);

    mutesSafeBtn.setButtonText("MUTES SAFE");
    mutesSafeBtn.setClickingTogglesState(true);
    mutesSafeBtn.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xff1f6feb));
    mutesSafeBtn.onClick = [this] {
      editableCue().safes.safeMutes = mutesSafeBtn.getToggleState();
    };
    addAndMakeVisible(mutesSafeBtn);

    tableEditor.setMultiLine(true);
    tableEditor.setReadOnly(true);
    tableEditor.setScrollbarsShown(true);
    tableEditor.setFont(juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    tableEditor.setColour(juce::TextEditor::backgroundColourId,
                          juce::Colour(0xff0d1117));
    tableEditor.setColour(juce::TextEditor::textColourId,
                          juce::Colour(0xffc9d1d9));
    addAndMakeVisible(tableEditor);

    setSize(920, 600);
    syncCueSafes(editableCue().safes);
    startTimerHz(15);
    updateReadouts();
  }

  void setCallbacks(CueRecallCallback r, StopRecallCallback s = {}) {
    recallCue = std::move(r);
    stopRecall = std::move(s);
  }

  void timerCallback() override {
    if (engine.cueManager.isTransitioning()) {
      const auto &cue =
          engine.cueManager.getCue(engine.cueManager.getCurrentCueIndex());
      const float duration = juce::jmax(0.0f, cue.crossfadeSec);
      engine.cueManager.advanceFadeProgress(
          duration > 0.0f ? 1.0f / (duration * 15.0f) : 1.0f);
    }
    updateReadouts();
  }

  void updateReadouts() {
    const int curIdx = engine.cueManager.getCurrentCueIndex();
    const int total = engine.cueManager.getNumCues();
    if (curIdx >= 0 && curIdx < total) {
      const auto &cur = engine.cueManager.getCue(curIdx);
      const float prog = engine.cueManager.getFadeProgress() * 100.0f;
      activeCueLabel.setText(
          "ACTIVE CUE: " + juce::String(cur.cueNumber, 1) + " \xe2\x80\x94 " +
              cur.name +
              (engine.cueManager.isTransitioning()
                   ? (" [CROSSFADING: " + juce::String(static_cast<int>(prog)) +
                      "%]")
                   : " [STANDBY]"),
          juce::dontSendNotification);
      if (curIdx + 1 < total) {
        const auto &next = engine.cueManager.getCue(curIdx + 1);
        nextCueLabel.setText(
            "NEXT STANDBY: CUE " + juce::String(next.cueNumber, 1) +
                " \xe2\x80\x94 " + next.name +
                " | FADE: " + juce::String(next.crossfadeSec, 1) +
                "s | SMPTE: " + next.timecode,
            juce::dontSendNotification);
      } else {
        nextCueLabel.setText("NEXT STANDBY: [END OF SHOW]",
                             juce::dontSendNotification);
      }
    }

    juce::String tbl = "--- THEATER & CONCERT TOUR AUTOMATION CUE LIST (MIDAS "
                       "CHAPTER 34) ---\n";
    tbl << "CUE #  | STATUS  | SCENE | FADE TIME | SMPTE TIMECODE | CUE NAME & "
           "DESCRIPTION\n";
    tbl << "-------|---------|-------|-----------|----------------|------------"
           "----------------------------\n";
    for (int i = 0; i < total; ++i) {
      const auto &c = engine.cueManager.getCue(i);
      juce::String statusStr =
          (i == curIdx)
              ? (engine.cueManager.isTransitioning() ? ">> FADE" : "ACTIVE ")
          : (i == curIdx + 1) ? "NEXT   "
                              : "       ";
      tbl << juce::String(c.cueNumber, 1).paddedRight(' ', 6) << " | "
          << statusStr << " | SC " << juce::String(c.sceneSlot + 1) << "  | "
          << juce::String(c.crossfadeSec, 1).paddedLeft(' ', 4) << " s    | "
          << c.timecode.paddedRight(' ', 14) << " | " << c.name << "\n";
    }
    tableEditor.setText(tbl);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    title.setBounds(area.removeFromTop(24));
    subTitle.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);

    auto transRow = area.removeFromTop(38);
    goButton.setBounds(transRow.removeFromLeft(140).reduced(2));
    transRow.removeFromLeft(6);
    stopButton.setBounds(transRow.removeFromLeft(90).reduced(2));
    transRow.removeFromLeft(6);
    prevButton.setBounds(transRow.removeFromLeft(100).reduced(2));
    transRow.removeFromLeft(12);
    activeCueLabel.setBounds(transRow);
    area.removeFromTop(4);

    nextCueLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);

    auto safesRow = area.removeFromTop(28);
    cueSafeTarget.setBounds(safesRow.removeFromLeft(120).reduced(2));
    safesRow.removeFromLeft(4);
    fadersSafeBtn.setBounds(safesRow.removeFromLeft(120).reduced(2));
    safesRow.removeFromLeft(6);
    eqSafeBtn.setBounds(safesRow.removeFromLeft(90).reduced(2));
    safesRow.removeFromLeft(6);
    dynSafeBtn.setBounds(safesRow.removeFromLeft(130).reduced(2));
    safesRow.removeFromLeft(6);
    routingSafeBtn.setBounds(safesRow.removeFromLeft(120).reduced(2));
    safesRow.removeFromLeft(6);
    mutesSafeBtn.setBounds(safesRow.removeFromLeft(110).reduced(2));
    area.removeFromTop(8);

    tableEditor.setBounds(area);
  }

private:
  nova::graviton::CueItem &editableCue() {
    const int selected = cueSafeTarget.getSelectedId() - 1;
    return engine.cueManager.getCue(
        juce::jlimit(0, engine.cueManager.getNumCues() - 1, selected));
  }

  nova::graviton::CueItem &currentCue() {
    return engine.cueManager.getCue(engine.cueManager.getCurrentCueIndex());
  }

  void syncCueSafes(const nova::graviton::CueSafes &safes) {
    fadersSafeBtn.setToggleState(safes.safeFaders, juce::dontSendNotification);
    eqSafeBtn.setToggleState(safes.safeEQ, juce::dontSendNotification);
    dynSafeBtn.setToggleState(safes.safeDynamics, juce::dontSendNotification);
    routingSafeBtn.setToggleState(safes.safeRouting,
                                  juce::dontSendNotification);
    mutesSafeBtn.setToggleState(safes.safeMutes, juce::dontSendNotification);
  }

  void recallCurrentCue() {
    const auto &cue = currentCue();
    if (recallCue && !recallCue(cue.sceneSlot, cue.safes, cue.crossfadeSec))
      activeCueLabel.setText("SCENE RECALL FAILED — SCENE " +
                                 juce::String(cue.sceneSlot + 1),
                             juce::dontSendNotification);
  }

  MixerEngine &engine;
  CueRecallCallback recallCue;
  StopRecallCallback stopRecall;
  juce::Label title, subTitle, activeCueLabel, nextCueLabel;
  juce::TextButton goButton, stopButton, prevButton;
  juce::ComboBox cueSafeTarget;
  juce::TextButton fadersSafeBtn, eqSafeBtn, dynSafeBtn, routingSafeBtn,
      mutesSafeBtn;
  juce::TextEditor tableEditor;
};

// ==============================================================================
// Phase 100: ASIO Bridge Control Panel (Chapter 26 Hardware)
// ==============================================================================

} // namespace nova
