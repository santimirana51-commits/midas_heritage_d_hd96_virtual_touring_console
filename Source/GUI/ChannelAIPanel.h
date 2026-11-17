#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class ChannelAIPanel final : public juce::Component, private juce::Timer {
public:
  explicit ChannelAIPanel(MixerEngine &e, int chIndex)
      : engine(e), targetChannel(chIndex) {
    title.setText(
        "CHANNEL AI \xe2\x80\x94 AUTOMATIC ACOUSTIC PROFILING (CHAPTER 35)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    channelLabel.setText("ANALYZING: CHANNEL " +
                             juce::String(targetChannel + 1),
                         juce::dontSendNotification);
    channelLabel.setFont(
        juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xff58a6ff));
    addAndMakeVisible(channelLabel);

    startCaptureBtn.setButtonText("START 4S ACOUSTIC CAPTURE");
    startCaptureBtn.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff6f42c1));
    startCaptureBtn.onClick = [this] {
      const double sr = engine.sampleRate.load(std::memory_order_relaxed);
      engine.channelAI.startProfiling(targetChannel, sr, 4.0);
      profileAnalyzed = false;
      progressVal = 0.0;
      statusLabel.setText("Listening to audio signal... (4.0s @ " +
                              juce::String(static_cast<int>(sr)) + " Hz)",
                          juce::dontSendNotification);
      applyBtn.setEnabled(false);
    };
    addAndMakeVisible(startCaptureBtn);

    progressBar.setColour(juce::ProgressBar::foregroundColourId,
                          juce::Colour(0xff2ea44f));
    addAndMakeVisible(progressBar);

    detectionBadge.setText("[ LISTENING OR IDLE ]", juce::dontSendNotification);
    detectionBadge.setFont(
        juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    detectionBadge.setJustificationType(juce::Justification::centred);
    detectionBadge.setColour(juce::Label::backgroundColourId,
                             juce::Colour(0xff21262d));
    detectionBadge.setColour(juce::Label::textColourId,
                             juce::Colour(0xffd29922));
    addAndMakeVisible(detectionBadge);

    featuresReadout.setText("Acoustic Features: RMS: - dBFS | Peak: - dBFS | "
                            "Centroid: - Hz | ZCR: -",
                            juce::dontSendNotification);
    featuresReadout.setFont(juce::Font(12.0f));
    featuresReadout.setColour(juce::Label::textColourId,
                              juce::Colour(0xff8b949e));
    addAndMakeVisible(featuresReadout);

    recommendationsBox.setMultiLine(true);
    recommendationsBox.setReadOnly(true);
    recommendationsBox.setFont(juce::Font(juce::FontOptions(12.0f)));
    recommendationsBox.setText(
        "Adaptive Recommendations:\n- Awaiting acoustic profiling "
        "analysis...\n- Click 'START 4S ACOUSTIC CAPTURE' while audio is "
        "playing on this channel.");
    recommendationsBox.setColour(juce::TextEditor::backgroundColourId,
                                 juce::Colour(0xff161b22));
    recommendationsBox.setColour(juce::TextEditor::textColourId,
                                 juce::Colour(0xfff0f3f6));
    addAndMakeVisible(recommendationsBox);

    applyBtn.setButtonText("APPLY AI RECOMMENDATIONS TO CHANNEL " +
                           juce::String(targetChannel + 1));
    applyBtn.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff238636));
    applyBtn.setEnabled(false);
    applyBtn.onClick = [this] { applyCurrentRecommendations(); };
    addAndMakeVisible(applyBtn);

    undoBtn.setButtonText("UNDO / REVERT");
    undoBtn.setColour(juce::TextButton::buttonColourId,
                      juce::Colour(0xff6e7681));
    undoBtn.setEnabled(false);
    undoBtn.onClick = [this] {
      if (engine.restoreChannelAISnapshot(targetChannel, preAiSnapshot)) {
        undoBtn.setEnabled(false);
        statusLabel.setText("Reverted Channel " +
                                juce::String(targetChannel + 1) +
                                " to pre-AI state.",
                            juce::dontSendNotification);
      }
    };
    addAndMakeVisible(undoBtn);

    statusLabel.setText("Status: Ready", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff58a6ff));
    addAndMakeVisible(statusLabel);

    startTimerHz(15);
  }

  void timerCallback() override {
    if (engine.channelAI.getTargetChannel() != targetChannel)
      return;

    if (engine.channelAI.isCapturing()) {
      progressVal = static_cast<double>(engine.channelAI.getProgress());
      statusLabel.setText(
          "Profiling in progress: " +
              juce::String(static_cast<int>(progressVal * 100.0)) + "%",
          juce::dontSendNotification);
    } else if (engine.channelAI.isCompleted() && !profileAnalyzed) {
      progressVal = 1.0;
      profileAnalyzed = true;
      runAnalysis();
    }
  }

  void runAnalysis() {
    const double sr = engine.sampleRate.load(std::memory_order_relaxed);
    const auto feat = engine.channelAI.extractCapturedFeatures(sr);
    currentRec = engine.channelAI.runProfileOnCaptured(sr);

    featuresReadout.setText(
        "RMS: " + juce::String(feat.rmsDb, 1) +
            " dBFS | Peak: " + juce::String(feat.peakDb, 1) +
            " dBFS | Crest: " + juce::String(feat.crestFactor, 2) +
            " | Centroid: " + juce::String(feat.spectralCentroid, 0) +
            " Hz | Dur: " + juce::String(feat.durationSec, 2) + "s",
        juce::dontSendNotification);

    if (!currentRec.valid || currentRec.insufficientData) {
      detectionBadge.setText("[ DATA TIDAK CUKUP / SINYAL SUNYI ]",
                             juce::dontSendNotification);
      detectionBadge.setColour(juce::Label::textColourId,
                               juce::Colour(0xfff85149));
      recommendationsBox.setText(
          "Analisis dibatalkan: sinyal audio terlalu pelan (<= -65 dBFS) atau "
          "durasi capture tidak mencukupi (< 0.45s).\n"
          "Pastikan instrumen/mikrofon berbunyi pada Channel " +
          juce::String(targetChannel + 1) + " lalu ulangi capture.");
      applyBtn.setEnabled(false);
      statusLabel.setText("Data tidak cukup — rekomendasi tidak diterapkan.",
                          juce::dontSendNotification);
      return;
    }

    detectionBadge.setText(
        "[ " +
            juce::String(nova::graviton::instrumentToString(
                             currentRec.detectedInstrument))
                .toUpperCase() +
            " DETECTED ] (" +
            juce::String(static_cast<int>(currentRec.confidence * 100.0f)) +
            "% Confidence)" + (currentRec.clipped ? " [CLIPPING]" : ""),
        juce::dontSendNotification);
    detectionBadge.setColour(juce::Label::textColourId,
                             currentRec.clipped ? juce::Colour(0xffd29922)
                                                : juce::Colour(0xff2ea44f));

    juce::String text = "OPTIMIZED ADAPTIVE DSP PROFILE (SAFETY CLAMPED):\n\n";
    text += juce::String("1. AUTO-TRIM: ") +
            (currentRec.targetTrimDb >= 0 ? "+" : "") +
            juce::String(currentRec.targetTrimDb, 1) +
            " dB (Calibrated to -18 dBFS nominal, clamped [-18, +18] dB)\n";
    text += "2. HIGH-PASS FILTER: " + juce::String(currentRec.hpfFreqHz, 0) +
            " Hz (" +
            (currentRec.hpfSlope == 1 ? "18 dB/oct Butterworth"
                                      : "12 dB/oct Butterworth") +
            ")\n";
    text += "3. SMART 4-BAND PARAMETRIC EQ:\n";
    text += juce::String("   - Band 1 (Bass):   ") +
            juce::String(currentRec.eqFreqHz[0], 0) + " Hz | " +
            (currentRec.eqGainDb[0] >= 0 ? "+" : "") +
            juce::String(currentRec.eqGainDb[0], 1) + " dB | Q " +
            juce::String(currentRec.eqQ[0], 1) + "\n";
    text += juce::String("   - Band 2 (Lo-Mid): ") +
            juce::String(currentRec.eqFreqHz[1], 0) + " Hz | " +
            (currentRec.eqGainDb[1] >= 0 ? "+" : "") +
            juce::String(currentRec.eqGainDb[1], 1) + " dB | Q " +
            juce::String(currentRec.eqQ[1], 1) + "\n";
    text += juce::String("   - Band 3 (Hi-Mid): ") +
            juce::String(currentRec.eqFreqHz[2], 0) + " Hz | " +
            (currentRec.eqGainDb[2] >= 0 ? "+" : "") +
            juce::String(currentRec.eqGainDb[2], 1) + " dB | Q " +
            juce::String(currentRec.eqQ[2], 1) + "\n";
    text += juce::String("   - Band 4 (Treble): ") +
            juce::String(currentRec.eqFreqHz[3], 0) + " Hz | " +
            (currentRec.eqGainDb[3] >= 0 ? "+" : "") +
            juce::String(currentRec.eqGainDb[3], 1) + " dB | Q " +
            juce::String(currentRec.eqQ[3], 1) + "\n";
    text += "4. ADAPTIVE DYNAMICS:\n";
    text += "   - Noise Gate:  " +
            juce::String(currentRec.gateEnabled
                             ? "ENABLED (" +
                                   juce::String(currentRec.gateThresholdDb, 1) +
                                   " dBFS, " +
                                   juce::String(currentRec.gateRatio, 1) + ":1)"
                             : "BYPASS") +
            "\n";
    text +=
        "   - Compressor:  " +
        juce::String(
            currentRec.compEnabled
                ? "ENABLED (" + juce::String(currentRec.compThresholdDb, 1) +
                      " dBFS, " + juce::String(currentRec.compRatio, 1) +
                      ":1, Att " + juce::String(currentRec.compAttackMs, 1) +
                      "ms, Rel " + juce::String(currentRec.compReleaseMs, 0) +
                      "ms)"
                : "BYPASS") +
        "\n";

    recommendationsBox.setText(text);
    applyBtn.setEnabled(true);
    statusLabel.setText(
        currentRec.clipped
            ? "WARNING: Input clipping terdeteksi! Periksa level preamp."
            : "Acoustic profiling complete. Ready to apply.",
        juce::dontSendNotification);
  }

  void applyCurrentRecommendations() {
    if (targetChannel < 0 || targetChannel >= kLogicalChannels ||
        !currentRec.valid || currentRec.insufficientData) {
      statusLabel.setText("Tidak dapat menerapkan: profil belum valid.",
                          juce::dontSendNotification);
      return;
    }

    if (!preAiSnapshot.valid)
      preAiSnapshot = engine.captureChannelAISnapshot(targetChannel);

    if (engine.applyAIRecommendation(targetChannel, currentRec, 0)) {
      undoBtn.setEnabled(preAiSnapshot.valid);
      statusLabel.setText(
          "All AI recommendations successfully applied to Channel " +
              juce::String(targetChannel + 1) + "!",
          juce::dontSendNotification);
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff161b22));
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(14);
    title.setBounds(area.removeFromTop(26));
    channelLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    auto captureRow = area.removeFromTop(32);
    startCaptureBtn.setBounds(captureRow.removeFromLeft(240).reduced(2));
    progressBar.setBounds(captureRow.reduced(2));

    area.removeFromTop(8);
    detectionBadge.setBounds(area.removeFromTop(36).reduced(2));
    featuresReadout.setBounds(area.removeFromTop(22).reduced(2));
    area.removeFromTop(6);
    recommendationsBox.setBounds(
        area.removeFromTop(area.getHeight() - 68).reduced(2));
    area.removeFromTop(6);
    auto actionRow = area.removeFromTop(36);
    undoBtn.setBounds(actionRow.removeFromRight(150).reduced(2));
    applyBtn.setBounds(actionRow.reduced(2));
    statusLabel.setBounds(area.reduced(2));
  }

private:
  MixerEngine &engine;
  int targetChannel = 0;
  double progressVal = 0.0;
  bool profileAnalyzed = false;
  nova::graviton::AIRecommendation currentRec;
  MixerEngine::ChannelAISnapshot preAiSnapshot;

  juce::Label title, channelLabel, detectionBadge, featuresReadout, statusLabel;
  juce::TextButton startCaptureBtn, applyBtn, undoBtn;
  juce::ProgressBar progressBar{progressVal};
  juce::TextEditor recommendationsBox;
};

// ==============================================================================
// MIDAS Heritage-D HD96-24 Chapter 16: 4-Band Dynamic Parametric EQ Editor
// Panel
// ==============================================================================

} // namespace nova
