#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class ManchinoPanel final : public juce::Component, private juce::Timer {
public:
  explicit ManchinoPanel(MixerEngine &e) : engine(e) {
    title.setText(
        "MANCHINO MULTI-EDIT MATRIX & SHOW SPREADSHEET (CHAPTERS 24 & 33)",
        juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    auto setupBtn = [this](juce::TextButton &btn, const juce::String &text,
                           auto action) {
      btn.setButtonText(text);
      btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
      btn.onClick = action;
      addAndMakeVisible(btn);
    };

    setupBtn(btnAll, "ALL (32)", [this] {
      engine.manchinoSelection.selectAll();
      updateSelectionView();
    });
    setupBtn(btnDrums, "DRUMS (1-8)", [this] {
      engine.manchinoSelection.selectRange(0, 8);
      updateSelectionView();
    });
    setupBtn(btnVocals, "VOCALS (9-16)", [this] {
      engine.manchinoSelection.selectRange(8, 16);
      updateSelectionView();
    });
    setupBtn(btnGuitars, "GUITARS (17-24)", [this] {
      engine.manchinoSelection.selectRange(16, 24);
      updateSelectionView();
    });
    setupBtn(btnKeys, "KEYS (25-32)", [this] {
      engine.manchinoSelection.selectRange(24, 32);
      updateSelectionView();
    });
    setupBtn(btnClear, "CLEAR", [this] {
      engine.manchinoSelection.clearAll();
      updateSelectionView();
    });

    selectionCountLabel.setFont(
        juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    selectionCountLabel.setColour(juce::Label::textColourId,
                                  juce::Colour(0xff58a6ff));
    addAndMakeVisible(selectionCountLabel);

    for (int i = 0; i < 32; ++i) {
      chButtons[static_cast<size_t>(i)].setButtonText(juce::String(i + 1));
      chButtons[static_cast<size_t>(i)].setClickingTogglesState(false);
      chButtons[static_cast<size_t>(i)].setColour(
          juce::TextButton::buttonColourId, juce::Colour(0xff21262d));
      chButtons[static_cast<size_t>(i)].onClick = [this, i] {
        engine.manchinoSelection.toggleChannel(static_cast<size_t>(i));
        updateSelectionView();
      };
      addAndMakeVisible(chButtons[static_cast<size_t>(i)]);
    }

    trimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trimSlider.setRange(-20.0, 60.0, 0.5);
    trimSlider.setValue(0.0, juce::dontSendNotification);
    trimSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    trimSlider.setTextValueSuffix(" dB TRIM");
    addAndMakeVisible(trimSlider);

    hpfSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hpfSlider.setRange(20.0, 2000.0, 5.0);
    hpfSlider.setValue(80.0, juce::dontSendNotification);
    hpfSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    hpfSlider.setTextValueSuffix(" Hz HPF");
    addAndMakeVisible(hpfSlider);

    hpfToggle.setButtonText("ENABLE HPF");
    hpfToggle.setClickingTogglesState(true);
    hpfToggle.setColour(juce::TextButton::buttonOnColourId,
                        juce::Colour(0xff2ea44f));
    addAndMakeVisible(hpfToggle);

    polarityToggle.setButtonText("INVERT PHASE");
    polarityToggle.setClickingTogglesState(true);
    polarityToggle.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xffd73a49));
    addAndMakeVisible(polarityToggle);

    for (int g = 0; g < 8; ++g) {
      mgButtons[static_cast<size_t>(g)].setButtonText("MG " +
                                                      juce::String(g + 1));
      mgButtons[static_cast<size_t>(g)].setClickingTogglesState(true);
      mgButtons[static_cast<size_t>(g)].setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0xffe53935));
      addAndMakeVisible(mgButtons[static_cast<size_t>(g)]);
    }

    for (int aux = 1; aux <= kAuxBusCapacity; ++aux)
      auxCombo.addItem("AUX " + juce::String(aux), aux);
    auxCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(auxCombo);

    auxSendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    auxSendSlider.setRange(-60.0, 10.0, 0.5);
    auxSendSlider.setValue(-60.0, juce::dontSendNotification);
    auxSendSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    auxSendSlider.setTextValueSuffix(" dB SEND");
    addAndMakeVisible(auxSendSlider);

    applyBatchBtn.setButtonText("APPLY BATCH SETTINGS TO SELECTED");
    applyBatchBtn.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff1f6feb));
    applyBatchBtn.onClick = [this] { applyBatchChanges(); };
    addAndMakeVisible(applyBatchBtn);

    exportCsvBtn.setButtonText("EXPORT SHOW SPREADSHEET (CSV)");
    exportCsvBtn.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xff238636));
    exportCsvBtn.onClick = [this] { exportSpreadsheet(); };
    addAndMakeVisible(exportCsvBtn);

    importCsvBtn.setButtonText("IMPORT SHOW SPREADSHEET (CSV)");
    importCsvBtn.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xff8957e5));
    importCsvBtn.onClick = [this] { importSpreadsheet(); };
    addAndMakeVisible(importCsvBtn);

    statusLabel.setText("Show Spreadsheet Matrix ready (RFC-4180 standard)",
                        juce::dontSendNotification);
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(statusLabel);

    updateSelectionView();
    startTimerHz(5);
  }

  void updateSelectionView() {
    const size_t count = engine.manchinoSelection.getSelectedCount();
    selectionCountLabel.setText("Selected: " + juce::String(count) +
                                    " / 32 Channels",
                                juce::dontSendNotification);

    for (size_t i = 0; i < 32; ++i) {
      const bool sel = engine.manchinoSelection.isSelected(i);
      chButtons[i].setColour(juce::TextButton::buttonColourId,
                             sel ? juce::Colour(0xff1f6feb)
                                 : juce::Colour(0xff21262d));
    }
  }

  void timerCallback() override { updateSelectionView(); }

  void applyBatchChanges() {
    const float trimVal = static_cast<float>(trimSlider.getValue());
    const float hpfVal = static_cast<float>(hpfSlider.getValue());
    const bool hpfOn = hpfToggle.getToggleState();
    const bool pol = polarityToggle.getToggleState();
    const int auxIdx = nova::graviton::manchinoAuxBusIndexFromSelectionId(
        auxCombo.getSelectedId(), kAuxBusCapacity);
    const float auxSendVal = static_cast<float>(auxSendSlider.getValue());

    uint8_t mgMask = 0;
    for (int g = 0; g < 8; ++g) {
      if (mgButtons[static_cast<size_t>(g)].getToggleState())
        mgMask |= static_cast<uint8_t>(1u << g);
    }

    size_t count = 0;
    for (size_t c = 0; c < 32; ++c) {
      if (engine.manchinoSelection.isSelected(c)) {
        auto &ch = engine.channels[c];
        ch.inputGainDb.store(trimVal, std::memory_order_relaxed);
        ch.highPassFreqHz.store(hpfVal, std::memory_order_relaxed);
        ch.highPassEnabled.store(hpfOn, std::memory_order_relaxed);
        ch.polarityInverted.store(pol, std::memory_order_relaxed);
        if (mgMask != 0)
          ch.muteGroupMask.store(mgMask, std::memory_order_relaxed);
        if (auxIdx >= 0) {
          ch.auxSendDb[static_cast<size_t>(auxIdx)].store(
              auxSendVal, std::memory_order_relaxed);
        }
        count++;
      }
    }
    engine.syncAuxRoutingSnapshot();
    statusLabel.setText("Batch settings applied to " + juce::String(count) +
                            " channels!",
                        juce::dontSendNotification);
  }

  void exportSpreadsheet() {
    std::string csv = nova::graviton::ShowSpreadsheet::generateHeader();
    for (int c = 0; c < 32; ++c) {
      const auto &ch = engine.channels[static_cast<size_t>(c)];
      nova::graviton::SpreadsheetChannelData data;
      data.channelIndex = c;
      data.name = "CH " + std::to_string(c + 1);
      data.trimDb = ch.inputGainDb.load(std::memory_order_relaxed);
      data.delayMs = ch.inputDelayMs.load(std::memory_order_relaxed);
      data.hpfHz = ch.highPassFreqHz.load(std::memory_order_relaxed);
      data.hpfOn = ch.highPassEnabled.load(std::memory_order_relaxed);
      data.hpfSlope = ch.highPassSlope.load(std::memory_order_relaxed);
      data.lpfHz = ch.lowPassFreqHz.load(std::memory_order_relaxed);
      data.lpfOn = ch.lowPassEnabled.load(std::memory_order_relaxed);
      for (size_t b = 0; b < 4; ++b) {
        data.eqFreq[b] = ch.eqFrequency[b].load(std::memory_order_relaxed);
        data.eqGainDb[b] = ch.eqGainDb[b].load(std::memory_order_relaxed);
        data.eqQ[b] = ch.eqQ[b].load(std::memory_order_relaxed);
      }
      data.gateOn = ch.gateEnabled.load(std::memory_order_relaxed);
      data.gateThreshDb = ch.gateThresholdDb.load(std::memory_order_relaxed);
      data.compOn = ch.compressorEnabled.load(std::memory_order_relaxed);
      data.compThreshDb =
          ch.compressorThresholdDb.load(std::memory_order_relaxed);
      data.muteGroups = ch.muteGroupMask.load(std::memory_order_relaxed);
      data.pan = ch.pan.load(std::memory_order_relaxed);
      data.faderDb = ch.gainDb.load(std::memory_order_relaxed);
      data.muted = ch.muted.load(std::memory_order_relaxed);
      csv += nova::graviton::ShowSpreadsheet::channelToCsvLine(data);
    }

    auto outDir =
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("NOVA_HD");
    outDir.createDirectory();
    auto exportFile = outDir.getChildFile("ShowSpreadsheet_Export.csv");
    if (exportFile.replaceWithText(csv)) {
      statusLabel.setText("Exported 32 channels to: " +
                              exportFile.getFileName(),
                          juce::dontSendNotification);
    } else {
      statusLabel.setText("Failed to export spreadsheet CSV",
                          juce::dontSendNotification);
    }
  }

  void importSpreadsheet() {
    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Select Show Spreadsheet CSV to Import",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.csv");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this, fileChooser](const juce::FileChooser &fc) {
          auto result = fc.getResult();
          if (result.existsAsFile()) {
            const auto lines =
                juce::StringArray::fromLines(result.loadFileAsString());
            size_t imported = 0;
            for (int i = 0; i < lines.size(); ++i) {
              nova::graviton::SpreadsheetChannelData chData;
              if (nova::graviton::ShowSpreadsheet::parseCsvLine(
                      lines[i].toStdString(), chData)) {
                if (chData.channelIndex >= 0 &&
                    chData.channelIndex < kLogicalChannels) {
                  auto &ch =
                      engine.channels[static_cast<size_t>(chData.channelIndex)];
                  ch.inputGainDb.store(chData.trimDb,
                                       std::memory_order_relaxed);
                  ch.inputDelayMs.store(chData.delayMs,
                                        std::memory_order_relaxed);
                  ch.highPassFreqHz.store(chData.hpfHz,
                                          std::memory_order_relaxed);
                  ch.highPassEnabled.store(chData.hpfOn,
                                           std::memory_order_relaxed);
                  ch.highPassSlope.store(chData.hpfSlope,
                                         std::memory_order_relaxed);
                  ch.lowPassFreqHz.store(chData.lpfHz,
                                         std::memory_order_relaxed);
                  ch.lowPassEnabled.store(chData.lpfOn,
                                          std::memory_order_relaxed);
                  for (size_t b = 0; b < 4; ++b) {
                    ch.eqFrequency[b].store(chData.eqFreq[b],
                                            std::memory_order_relaxed);
                    ch.eqGainDb[b].store(chData.eqGainDb[b],
                                         std::memory_order_relaxed);
                    ch.eqQ[b].store(chData.eqQ[b], std::memory_order_relaxed);
                  }
                  ch.gateEnabled.store(chData.gateOn,
                                       std::memory_order_relaxed);
                  ch.gateThresholdDb.store(chData.gateThreshDb,
                                           std::memory_order_relaxed);
                  ch.compressorEnabled.store(chData.compOn,
                                             std::memory_order_relaxed);
                  ch.compressorThresholdDb.store(chData.compThreshDb,
                                                 std::memory_order_relaxed);
                  ch.muteGroupMask.store(chData.muteGroups,
                                         std::memory_order_relaxed);
                  ch.pan.store(chData.pan, std::memory_order_relaxed);
                  ch.gainDb.store(chData.faderDb, std::memory_order_relaxed);
                  ch.muted.store(chData.muted, std::memory_order_relaxed);
                  imported++;
                }
              }
            }
            statusLabel.setText("Imported " + juce::String(imported) +
                                    " channels from CSV!",
                                juce::dontSendNotification);
          }
        });
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff161b22));
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    title.setBounds(area.removeFromTop(26));
    area.removeFromTop(6);

    auto selRow = area.removeFromTop(28);
    btnAll.setBounds(selRow.removeFromLeft(90).reduced(2));
    btnDrums.setBounds(selRow.removeFromLeft(105).reduced(2));
    btnVocals.setBounds(selRow.removeFromLeft(105).reduced(2));
    btnGuitars.setBounds(selRow.removeFromLeft(110).reduced(2));
    btnKeys.setBounds(selRow.removeFromLeft(95).reduced(2));
    btnClear.setBounds(selRow.removeFromLeft(75).reduced(2));
    selectionCountLabel.setBounds(selRow.reduced(4, 2));

    area.removeFromTop(8);

    auto gridArea = area.removeFromTop(120).reduced(2);
    const int cols = 8, rows = 4;
    const int btnW = gridArea.getWidth() / cols;
    const int btnH = gridArea.getHeight() / rows;
    for (int i = 0; i < 32; ++i) {
      const int col = i % cols, row = i / cols;
      chButtons[static_cast<size_t>(i)].setBounds(
          gridArea.getX() + col * btnW + 1, gridArea.getY() + row * btnH + 1,
          btnW - 2, btnH - 2);
    }

    area.removeFromTop(8);
    auto batchRow1 = area.removeFromTop(32);
    trimSlider.setBounds(batchRow1.removeFromLeft(220).reduced(2));
    hpfSlider.setBounds(batchRow1.removeFromLeft(220).reduced(2));
    hpfToggle.setBounds(batchRow1.removeFromLeft(110).reduced(2));
    polarityToggle.setBounds(batchRow1.removeFromLeft(115).reduced(2));

    area.removeFromTop(6);
    auto mgRow = area.removeFromTop(28);
    const int mgW = 65;
    for (int g = 0; g < 8; ++g)
      mgButtons[static_cast<size_t>(g)].setBounds(
          mgRow.removeFromLeft(mgW).reduced(2));

    area.removeFromTop(6);
    auto auxRow = area.removeFromTop(32);
    auxCombo.setBounds(auxRow.removeFromLeft(100).reduced(2));
    auxSendSlider.setBounds(auxRow.removeFromLeft(220).reduced(2));
    applyBatchBtn.setBounds(auxRow.removeFromLeft(260).reduced(2));

    area.removeFromTop(10);
    auto csvRow = area.removeFromTop(34);
    exportCsvBtn.setBounds(csvRow.removeFromLeft(250).reduced(2));
    importCsvBtn.setBounds(csvRow.removeFromLeft(250).reduced(2));
    statusLabel.setBounds(csvRow.reduced(6, 2));
  }

private:
  MixerEngine &engine;
  juce::Label title, selectionCountLabel, statusLabel;
  juce::TextButton btnAll, btnDrums, btnVocals, btnGuitars, btnKeys, btnClear;
  std::array<juce::TextButton, 32> chButtons;
  juce::Slider trimSlider, hpfSlider, auxSendSlider;
  juce::TextButton hpfToggle, polarityToggle, applyBatchBtn, exportCsvBtn,
      importCsvBtn;
  juce::ComboBox auxCombo;
  std::array<juce::TextButton, 8> mgButtons;
};

} // namespace nova
