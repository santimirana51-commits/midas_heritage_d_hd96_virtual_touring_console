#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class OfflineBouncePanel final : public juce::Component, private juce::Timer {
public:
  using AudioGate = std::function<void(bool)>;

  OfflineBouncePanel(MixerEngine &e, AudioGate gate)
      : engine(e), audioGate(std::move(gate)) {
    title.setText("OFFLINE MIXDOWN & MULTI-TRACK BOUNCE",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText(
        "Master L/R + Bus Stems rendered faster than realtime (MIDAS HD96-24 "
        "Chapter 36)",
        juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    sourceButton.setButtonText("CHOOSE SOURCE...");
    sourceButton.setTooltip(
        "Pick the multitrack (or stereo) source audio file to render");
    sourceButton.onClick = [this] { chooseSourceFile(); };
    addAndMakeVisible(sourceButton);

    sourceLabel.setText("No source file selected", juce::dontSendNotification);
    sourceLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    sourceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d1d9));
    sourceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sourceLabel);

    folderButton.setButtonText("OUTPUT FOLDER...");
    folderButton.setTooltip(
        "Choose where the bounced WAV/FLAC files are written");
    folderButton.onClick = [this] { chooseOutputFolder(); };
    addAndMakeVisible(folderButton);

    folderLabel.setText("No output folder selected",
                        juce::dontSendNotification);
    folderLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    folderLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d1d9));
    folderLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(folderLabel);

    formatLabel.setText("FORMAT", juce::dontSendNotification);
    styleSmallLabel(formatLabel);
    addAndMakeVisible(formatLabel);
    formatCombo.addItem("WAV", 1);
    formatCombo.addItem("FLAC", 2);
    formatCombo.setSelectedId(1, juce::dontSendNotification);
    formatCombo.setTooltip("Output container (WAV or lossless FLAC)");
    addAndMakeVisible(formatCombo);

    bitDepthLabel.setText("BIT DEPTH", juce::dontSendNotification);
    styleSmallLabel(bitDepthLabel);
    addAndMakeVisible(bitDepthLabel);
    bitDepthCombo.addItem("16 BIT", 1);
    bitDepthCombo.addItem("24 BIT", 2);
    bitDepthCombo.setSelectedId(2, juce::dontSendNotification);
    bitDepthCombo.setTooltip("Word length of the bounced files");
    addAndMakeVisible(bitDepthCombo);

    rateLabel.setText("SAMPLE RATE", juce::dontSendNotification);
    styleSmallLabel(rateLabel);
    addAndMakeVisible(rateLabel);
    rateCombo.addItem("SOURCE RATE", 1);
    rateCombo.addItem("48 000 Hz", 2);
    rateCombo.addItem("96 000 Hz", 3);
    rateCombo.setSelectedId(1, juce::dontSendNotification);
    rateCombo.setTooltip(
        "Output sample rate; conversion is applied per source channel");
    addAndMakeVisible(rateCombo);

    patchToggle.setButtonText("PATCH TRACKS 1:1");
    patchToggle.setTooltip(
        "Route source track 1 to channel 1, track 2 to channel 2 ... for the "
        "duration of the bounce; your patch is restored afterwards");
    patchToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(patchToggle);

    targetsHeader.setText("BOUNCE TARGETS", juce::dontSendNotification);
    targetsHeader.setFont(
        juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    targetsHeader.setColour(juce::Label::textColourId,
                            juce::Colour(0xff58a6ff));
    addAndMakeVisible(targetsHeader);

    masterToggle.setButtonText("MASTER L/R (stereo)");
    masterToggle.setToggleState(true, juce::dontSendNotification);
    masterToggle.setTooltip(
        "Write the full mix: Main bus, GEQ, FX and limiter");
    addAndMakeVisible(masterToggle);

    allStemsToggle.setButtonText("ALL STEMS");
    allStemsToggle.setToggleState(true, juce::dontSendNotification);
    allStemsToggle.onClick = [this] {
      const bool on = allStemsToggle.getToggleState();
      for (auto &toggle : stemToggles)
        toggle->setToggleState(on, juce::dontSendNotification);
    };
    addAndMakeVisible(allStemsToggle);

    // One toggle per stem bus so Drum/Band/Vocal style submixes can be bounced
    // together with the master in a single click.
    {
      MixerEngineBounceTarget naming(engine);
      for (int s = 0; s < naming.getNumStems(); ++s) {
        auto toggle = std::make_unique<juce::ToggleButton>(
            naming.getStemName(s).toUpperCase());
        toggle->setToggleState(true, juce::dontSendNotification);
        toggle->onClick = [this] {
          bool allOn = true;
          for (const auto &other : stemToggles)
            if (!other->getToggleState()) {
              allOn = false;
              break;
            }
          allStemsToggle.setToggleState(allOn, juce::dontSendNotification);
        };
        addAndMakeVisible(*toggle);
        stemToggles.push_back(std::move(toggle));
      }
    }

    notes.setText("The audio device is paused while rendering, so any active "
                  "recording stops at bounce start. Rendering runs offline "
                  "from the source file and is faster than realtime; the "
                  "console state is untouched.",
                  juce::dontSendNotification);
    notes.setFont(juce::Font(juce::FontOptions(11.0f)));
    notes.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    notes.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(notes);

    progressBar.setColour(juce::ProgressBar::foregroundColourId,
                          juce::Colour(0xff2ea44f));
    addAndMakeVisible(progressBar);

    statusLabel.setText("Choose a source file, an output folder and the "
                        "bounce targets, then press START BOUNCE.",
                        juce::dontSendNotification);
    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff58a6ff));
    addAndMakeVisible(statusLabel);

    startButton.setButtonText("START BOUNCE");
    startButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff238636));
    startButton.onClick = [this] { startBounce(); };
    addAndMakeVisible(startButton);

    cancelButton.setButtonText("CANCEL");
    cancelButton.setEnabled(false);
    cancelButton.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xffda3633));
    cancelButton.onClick = [this] { cancelBounce(); };
    addAndMakeVisible(cancelButton);

    setSize(900, 640);
  }

  ~OfflineBouncePanel() override {
    stopTimer();
    abortBounce();
    restoreConsoleState();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff20262d));
    g.setColour(juce::Colour(0xff3a444e));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(16);
    title.setBounds(area.removeFromTop(26));
    subTitle.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);

    auto sourceRow = area.removeFromTop(30);
    sourceButton.setBounds(sourceRow.removeFromLeft(160).reduced(2));
    sourceLabel.setBounds(sourceRow.reduced(6, 4));

    auto folderRow = area.removeFromTop(30);
    folderButton.setBounds(folderRow.removeFromLeft(160).reduced(2));
    folderLabel.setBounds(folderRow.reduced(6, 4));

    area.removeFromTop(6);
    auto formatRow = area.removeFromTop(30);
    formatLabel.setBounds(formatRow.removeFromLeft(55));
    formatCombo.setBounds(formatRow.removeFromLeft(110).reduced(4, 3));
    bitDepthLabel.setBounds(formatRow.removeFromLeft(80));
    bitDepthCombo.setBounds(formatRow.removeFromLeft(110).reduced(4, 3));
    rateLabel.setBounds(formatRow.removeFromLeft(90));
    rateCombo.setBounds(formatRow.removeFromLeft(170).reduced(4, 3));
    patchToggle.setBounds(formatRow.reduced(6, 4));

    area.removeFromTop(8);
    targetsHeader.setBounds(area.removeFromTop(20));
    auto masterRow = area.removeFromTop(28);
    masterToggle.setBounds(masterRow.removeFromLeft(220).reduced(2));
    allStemsToggle.setBounds(masterRow.removeFromLeft(150).reduced(2));

    const int columns = 3;
    const int cellWidth = area.getWidth() / columns;
    const int gridTop = area.getY();
    for (size_t i = 0; i < stemToggles.size(); ++i) {
      const int row = static_cast<int>(i) / columns;
      const int column = static_cast<int>(i) % columns;
      stemToggles[i]->setBounds(area.getX() + column * cellWidth,
                                gridTop + row * 28, cellWidth, 26);
    }
    const int gridRows = juce::jmax(
        1, (static_cast<int>(stemToggles.size()) + columns - 1) / columns);
    area.removeFromTop(gridRows * 28);

    area.removeFromTop(8);
    notes.setBounds(area.removeFromTop(46));
    area.removeFromTop(6);
    progressBar.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    statusLabel.setBounds(area.removeFromTop(26));
    area.removeFromTop(8);

    auto buttonRow = area.removeFromTop(34);
    startButton.setBounds(buttonRow.removeFromLeft(200).reduced(2));
    cancelButton.setBounds(buttonRow.removeFromLeft(140).reduced(2));
  }

private:
  // ------------------------------------------------------------- worker thread
  class BounceWorker final : public juce::Thread {
  public:
    BounceWorker(MixerEngineBounceTarget &target,
                 const nova::graviton::OfflineBounceRequest &request)
        : juce::Thread("NOVA HD Offline Bounce"), target(target),
          request(request) {}

    void run() override {
      nova::graviton::OfflineBouncer bouncer;
      result = bouncer.run(
          target, request, &cancelFlag,
          [this](const nova::graviton::OfflineBounceProgress &update) {
            framesDone.store(update.framesDone, std::memory_order_relaxed);
            totalFrames.store(update.totalFrames, std::memory_order_relaxed);
            speedMilli.store(
                static_cast<juce::int64>(update.speedFactor * 1000.0),
                std::memory_order_relaxed);
          });
      finished.store(true, std::memory_order_release);
    }

    void requestCancel() { cancelFlag.store(true, std::memory_order_relaxed); }
    bool isFinished() const { return finished.load(std::memory_order_acquire); }

    nova::graviton::OfflineBounceResult result;
    std::atomic<juce::int64> framesDone{0};
    std::atomic<juce::int64> totalFrames{0};
    std::atomic<juce::int64> speedMilli{0};

  private:
    MixerEngineBounceTarget &target;
    nova::graviton::OfflineBounceRequest request;
    std::atomic<bool> cancelFlag{false};
    std::atomic<bool> finished{false};
  };

  static void styleSmallLabel(juce::Label &label) {
    label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    label.setJustificationType(juce::Justification::centredLeft);
  }

  void chooseSourceFile() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose source audio for offline bounce",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<OfflineBouncePanel>(this),
         chooser](const juce::FileChooser &fc) {
          if (safe != nullptr && fc.getResult().existsAsFile())
            safe->inspectSourceFile(fc.getResult());
        });
  }

  void chooseOutputFolder() {
    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose output folder for bounced files",
        outputFolder.isDirectory() ? outputFolder
                                   : juce::File::getSpecialLocation(
                                         juce::File::userDocumentsDirectory),
        "");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectDirectories,
        [safe = juce::Component::SafePointer<OfflineBouncePanel>(this),
         chooser](const juce::FileChooser &fc) {
          if (safe == nullptr)
            return;
          const auto result = fc.getResult();
          if (result.isDirectory()) {
            safe->outputFolder = result;
            safe->folderLabel.setText(result.getFullPathName(),
                                      juce::dontSendNotification);
          }
        });
  }

  void inspectSourceFile(const juce::File &file) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(
        formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0) {
      sourceFile = juce::File();
      sourceChannels = 0;
      sourceLabel.setText("Unsupported source file: " + file.getFileName(),
                          juce::dontSendNotification);
      setStatus("SOURCE FILE CANNOT BE READ", juce::Colour(0xffda3633));
      return;
    }
    sourceFile = file;
    sourceChannels =
        juce::jlimit(1, nova::graviton::OfflineBouncer::kMaxBounceInputs,
                     static_cast<int>(reader->numChannels));
    const double seconds =
        static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    sourceLabel.setText(file.getFileName() + "  |  " +
                            juce::String(sourceChannels) + " ch  |  " +
                            juce::String(static_cast<int>(reader->sampleRate)) +
                            " Hz  |  " + juce::String(seconds, 1) + " s",
                        juce::dontSendNotification);
    if (outputFolder.getFullPathName().isEmpty()) {
      outputFolder = file.getParentDirectory();
      folderLabel.setText(outputFolder.getFullPathName(),
                          juce::dontSendNotification);
    }
    setStatus("SOURCE LOADED — choose targets and press START BOUNCE",
              juce::Colour(0xff58a6ff));
  }

  void setStatus(const juce::String &text, juce::Colour colour) {
    statusLabel.setColour(juce::Label::textColourId, colour);
    statusLabel.setText(text, juce::dontSendNotification);
  }

  void startBounce() {
    if (worker != nullptr)
      return;

    if (!sourceFile.existsAsFile()) {
      setStatus("SELECT A SOURCE AUDIO FILE FIRST", juce::Colour(0xffda3633));
      return;
    }
    if (!outputFolder.isDirectory()) {
      setStatus("SELECT AN OUTPUT FOLDER FIRST", juce::Colour(0xffda3633));
      return;
    }

    nova::graviton::OfflineBounceRequest request;
    request.sourceFile = sourceFile;
    request.outputDirectory = outputFolder;
    request.fileNamePrefix = sourceFile.getFileNameWithoutExtension();
    request.exportMaster = masterToggle.getToggleState();
    for (size_t i = 0; i < stemToggles.size(); ++i)
      if (stemToggles[i]->getToggleState())
        request.stemSelection.push_back(static_cast<int>(i));
    request.container = (formatCombo.getSelectedId() == 2) ? "flac" : "wav";
    request.bitDepth = (bitDepthCombo.getSelectedId() == 1) ? 16 : 24;
    request.sampleRate = (rateCombo.getSelectedId() == 2)   ? 48000.0
                         : (rateCombo.getSelectedId() == 3) ? 96000.0
                                                            : 0.0;
    request.chunkFrames = 4096;

    if (!request.exportMaster && request.stemSelection.empty()) {
      setStatus("SELECT THE MASTER AND/OR AT LEAST ONE STEM",
                juce::Colour(0xffda3633));
      return;
    }

    // The offline render drives the very same engine state as the live
    // callback, so live audio (and any recording) pauses for the bounce.
    if (audioGate != nullptr)
      audioGate(true);
    consoleSuspended = true;
    playbackWasPlaying = engine.playbackTransport.isPlaying();
    if (playbackWasPlaying)
      engine.playbackTransport.stop();

    target = std::make_unique<MixerEngineBounceTarget>(engine);
    if (patchToggle.getToggleState())
      target->applyTrackPatch(sourceChannels);

    worker = std::make_unique<BounceWorker>(*target, request);
    progressValue = 0.0;
    if (!worker->startThread()) {
      worker.reset();
      restoreConsoleState();
      setStatus("FAILED TO START THE RENDER THREAD", juce::Colour(0xffda3633));
      return;
    }

    setControlsBusy(true);
    setStatus("RENDERING — offline, faster than realtime...",
              juce::Colour(0xff3fb950));
    startTimerHz(15);
  }

  void cancelBounce() {
    if (worker != nullptr) {
      worker->requestCancel();
      setStatus("CANCELLING — finishing the current block...",
                juce::Colour(0xffe6a218));
    }
  }

  void timerCallback() override {
    if (worker == nullptr) {
      stopTimer();
      return;
    }

    if (worker->isFinished()) {
      finishBounce();
      return;
    }

    const auto done = worker->framesDone.load(std::memory_order_relaxed);
    const auto total = worker->totalFrames.load(std::memory_order_relaxed);
    if (total > 0)
      progressValue = static_cast<double>(done) / static_cast<double>(total);
    const double speed = static_cast<double>(worker->speedMilli.load(
                             std::memory_order_relaxed)) /
                         1000.0;
    setStatus("RENDERING " + juce::String(done) + " / " + juce::String(total) +
                  " frames  |  " + juce::String(speed, 2) + "x realtime",
              juce::Colour(0xff3fb950));
  }

  void finishBounce() {
    stopTimer();
    worker->stopThread(30000);
    const auto result = worker->result;
    worker.reset();
    restoreConsoleState();
    setControlsBusy(false);

    if (result.ok) {
      progressValue = 1.0;
      const int files = result.files.size();
      setStatus("BOUNCE OK — " + juce::String(files) + " file(s) written in " +
                    juce::String(result.elapsedSeconds, 2) + " s  |  " +
                    juce::String(result.speedFactor, 2) + "x realtime",
                juce::Colour(0xff3fb950));
    } else if (result.cancelled) {
      progressValue = 0.0;
      setStatus("BOUNCE CANCELLED — partial files removed",
                juce::Colour(0xffe6a218));
    } else {
      progressValue = 0.0;
      setStatus("BOUNCE FAILED — " + result.error, juce::Colour(0xffda3633));
    }
  }

  void abortBounce() {
    if (worker == nullptr)
      return;
    worker->requestCancel();
    worker->stopThread(30000);
    worker.reset();
  }

  void restoreConsoleState() {
    if (target != nullptr) {
      target->restoreTrackPatch();
      target.reset();
    }
    if (playbackWasPlaying) {
      playbackWasPlaying = false;
      engine.playbackTransport.start();
    }
    if (consoleSuspended) {
      consoleSuspended = false;
      if (audioGate != nullptr)
        audioGate(false);
    }
  }

  void setControlsBusy(bool busy) {
    sourceButton.setEnabled(!busy);
    folderButton.setEnabled(!busy);
    formatCombo.setEnabled(!busy);
    bitDepthCombo.setEnabled(!busy);
    rateCombo.setEnabled(!busy);
    patchToggle.setEnabled(!busy);
    masterToggle.setEnabled(!busy);
    allStemsToggle.setEnabled(!busy);
    for (auto &toggle : stemToggles)
      toggle->setEnabled(!busy);
    startButton.setEnabled(!busy);
    cancelButton.setEnabled(busy);
  }

  MixerEngine &engine;
  AudioGate audioGate;

  juce::Label title, subTitle, sourceLabel, folderLabel;
  juce::Label formatLabel, bitDepthLabel, rateLabel, targetsHeader, notes;
  juce::Label statusLabel;
  juce::TextButton sourceButton, folderButton, startButton, cancelButton;
  juce::ComboBox formatCombo, bitDepthCombo, rateCombo;
  juce::ToggleButton patchToggle, masterToggle, allStemsToggle;
  std::vector<std::unique_ptr<juce::ToggleButton>> stemToggles;
  juce::ProgressBar progressBar{progressValue};
  double progressValue = 0.0;

  juce::File sourceFile, outputFolder;
  int sourceChannels = 0;

  std::unique_ptr<MixerEngineBounceTarget> target;
  std::unique_ptr<BounceWorker> worker;
  bool consoleSuspended = false;
  bool playbackWasPlaying = false;
};

} // namespace nova
