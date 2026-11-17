#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class TalkbackPanel final : public juce::Component, private juce::Timer {
public:
  explicit TalkbackPanel(MixerEngine &e) : engine(e) {
    title.setText("TALKBACK & INTERCOM MATRIX (MIDAS HD96 CHAPTER 32)",
                  juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));
    addAndMakeVisible(title);

    subTitle.setText("Engine-wide Intercom Routing to 96 AUX (IEMs), 24 "
                     "Matrix, Solo Bus A/B, and Music Auto-Ducking",
                     juce::dontSendNotification);
    subTitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subTitle.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    addAndMakeVisible(subTitle);

    // Big Push-To-Talk Button with custom mouse tracking
    pttButton.setButtonText("PUSH TO TALK");
    pttButton.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff21262d));
    pttButton.onStateChange = [this] {
      if (pttButton.isDown()) {
        engine.talkbackEngine.onButtonPressed(
            juce::Time::getMillisecondCounter());
      } else {
        engine.talkbackEngine.onButtonReleased(
            juce::Time::getMillisecondCounter());
      }
    };
    addAndMakeVisible(pttButton);

    latchToggleBtn.setButtonText("TALK LATCH");
    latchToggleBtn.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff238636));
    latchToggleBtn.onClick = [this] { engine.talkbackEngine.toggleLatched(); };
    addAndMakeVisible(latchToggleBtn);

    modeSelector.addItem("Auto-Latch / Momentary (300ms)", 1);
    modeSelector.addItem("Latch Only (Toggle)", 2);
    modeSelector.addItem("Momentary Only (PTT)", 3);
    modeSelector.setSelectedId(
        engine.talkbackEngine.switchMode.load(std::memory_order_relaxed) + 1,
        juce::dontSendNotification);
    modeSelector.onChange = [this] {
      engine.talkbackEngine.switchMode.store(modeSelector.getSelectedId() - 1,
                                             std::memory_order_relaxed);
    };
    addAndMakeVisible(modeSelector);

    // DSP sliders
    setupSlider(gainSlider, "dB", -20.0, 40.0, 0.5, [this] {
      engine.talkbackEngine.gainDb.store(
          static_cast<float>(gainSlider.getValue()), std::memory_order_relaxed);
    });
    gainSlider.setValue(
        engine.talkbackEngine.gainDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    setupSlider(hpfSlider, "Hz", 40.0, 400.0, 5.0, [this] {
      engine.talkbackEngine.hpfHz.store(
          static_cast<float>(hpfSlider.getValue()), std::memory_order_relaxed);
    });
    hpfSlider.setValue(
        engine.talkbackEngine.hpfHz.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    hpfBtn.setButtonText("HPF");
    hpfBtn.setClickingTogglesState(true);
    hpfBtn.setToggleState(
        engine.talkbackEngine.hpfEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    hpfBtn.onClick = [this] {
      engine.talkbackEngine.hpfEnabled.store(hpfBtn.getToggleState(),
                                             std::memory_order_relaxed);
    };
    addAndMakeVisible(hpfBtn);

    setupSlider(gateSlider, "dB", -60.0, -20.0, 1.0, [this] {
      engine.talkbackEngine.gateThresholdDb.store(
          static_cast<float>(gateSlider.getValue()), std::memory_order_relaxed);
    });
    gateSlider.setValue(
        engine.talkbackEngine.gateThresholdDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    gateBtn.setButtonText("GATE");
    gateBtn.setClickingTogglesState(true);
    gateBtn.setToggleState(
        engine.talkbackEngine.gateEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    gateBtn.onClick = [this] {
      engine.talkbackEngine.gateEnabled.store(gateBtn.getToggleState(),
                                              std::memory_order_relaxed);
    };
    addAndMakeVisible(gateBtn);

    setupSlider(duckerSlider, "dB", -40.0, -6.0, 1.0, [this] {
      engine.talkbackEngine.duckerDb.store(
          static_cast<float>(duckerSlider.getValue()),
          std::memory_order_relaxed);
    });
    duckerSlider.setValue(
        engine.talkbackEngine.duckerDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    // Destination Toggles
    soloABtn.setButtonText("SOLO A (FOH Headphones)");
    soloABtn.setClickingTogglesState(true);
    soloABtn.setToggleState(
        engine.talkbackEngine.destSoloA.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    soloABtn.onClick = [this] {
      engine.talkbackEngine.destSoloA.store(soloABtn.getToggleState(),
                                            std::memory_order_relaxed);
    };
    addAndMakeVisible(soloABtn);

    soloBBtn.setButtonText("SOLO B (Monitor Engineer)");
    soloBBtn.setClickingTogglesState(true);
    soloBBtn.setToggleState(
        engine.talkbackEngine.destSoloB.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    soloBBtn.onClick = [this] {
      engine.talkbackEngine.destSoloB.store(soloBBtn.getToggleState(),
                                            std::memory_order_relaxed);
    };
    addAndMakeVisible(soloBBtn);

    mainLrBtn.setButtonText("MAIN L/R (Audience PA)");
    mainLrBtn.setClickingTogglesState(true);
    mainLrBtn.setToggleState(
        engine.talkbackEngine.destMainLR.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    mainLrBtn.onClick = [this] {
      engine.talkbackEngine.destMainLR.store(mainLrBtn.getToggleState(),
                                             std::memory_order_relaxed);
    };
    addAndMakeVisible(mainLrBtn);

    allAuxBtn.setButtonText("ALL AUX (1-96)");
    allAuxBtn.onClick = [this] {
      const bool current = engine.talkbackEngine.isAuxTarget(0);
      engine.talkbackEngine.setAllAux(!current);
      for (int i = 0; i < 6; ++i)
        auxBankBtns[static_cast<size_t>(i)].setToggleState(
            !current, juce::dontSendNotification);
    };
    addAndMakeVisible(allAuxBtn);

    allMatrixBtn.setButtonText("ALL MATRIX (1-24)");
    allMatrixBtn.onClick = [this] {
      const bool current = engine.talkbackEngine.isMatrixTarget(0);
      engine.talkbackEngine.setAllMatrix(!current);
    };
    addAndMakeVisible(allMatrixBtn);

    for (int b = 0; b < 6; ++b) {
      auxBankBtns[static_cast<size_t>(b)].setButtonText(
          "AUX " + juce::String(b * 16 + 1) + "-" + juce::String((b + 1) * 16));
      auxBankBtns[static_cast<size_t>(b)].setClickingTogglesState(true);
      auxBankBtns[static_cast<size_t>(b)].setToggleState(
          engine.talkbackEngine.isAuxTarget(static_cast<size_t>(b * 16)),
          juce::dontSendNotification);
      auxBankBtns[static_cast<size_t>(b)].onClick = [this, b] {
        engine.talkbackEngine.setAuxBank(
            b, auxBankBtns[static_cast<size_t>(b)].getToggleState());
      };
      addAndMakeVisible(auxBankBtns[static_cast<size_t>(b)]);
    }

    startTimerHz(25);
  }

  ~TalkbackPanel() override { stopTimer(); }

  void timerCallback() override {
    const bool talking = engine.talkbackEngine.isActive();
    if (talking) {
      pttButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xffda3633));
      latchToggleBtn.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xffda3633));
    } else {
      pttButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff21262d));
      latchToggleBtn.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff238636));
    }
    repaint();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff0d1117));

    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(55);

    // Left Column: DSP & Switch Controls
    auto leftCol = area.removeFromLeft(360);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(leftCol.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff30363d));
    g.drawRoundedRectangle(leftCol.toFloat(), 6.0f, 1.0f);

    auto lHeader = leftCol.removeFromTop(24);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff58a6ff));
    g.drawText("TALKBACK PREAMP & PROCESSOR", lHeader.reduced(8, 0),
               juce::Justification::left);

    // Draw VU meter bar
    auto vuArea = leftCol.removeFromBottom(24).reduced(8, 2);
    g.setColour(juce::Colour(0xff21262d));
    g.fillRect(vuArea);
    const float pk = engine.talkbackEngine.getLivePeak();
    const int barW =
        static_cast<int>(vuArea.getWidth() * std::clamp(pk, 0.0f, 1.0f));
    if (barW > 0) {
      auto bar = vuArea.removeFromLeft(barW);
      g.setColour(engine.talkbackEngine.isActive() ? juce::Colour(0xfff85149)
                                                   : juce::Colour(0xff238636));
      g.fillRect(bar);
    }
    g.setColour(juce::Colour(0xffc9d1d9));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("MIC PEAK LEVEL", vuArea, juce::Justification::centred);

    // Right Column: Destination Matrix
    area.removeFromLeft(16);
    auto rightCol = area;
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(rightCol.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff30363d));
    g.drawRoundedRectangle(rightCol.toFloat(), 6.0f, 1.0f);

    auto rHeader = rightCol.removeFromTop(24);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff58a6ff));
    g.drawText("COMMUNICATION BUS DESTINATIONS", rHeader.reduced(8, 0),
               juce::Justification::left);
  }

  void resized() override {
    const bool isCompact = (getHeight() < 340 || getWidth() < 640);
    auto area = getLocalBounds().reduced(isCompact ? 6 : 16);
    title.setBounds(area.removeFromTop(isCompact ? 18 : 24));
    if (!isCompact) {
      subTitle.setBounds(area.removeFromTop(20));
      area.removeFromTop(6);
    } else {
      subTitle.setBounds(0, 0, 0, 0);
      area.removeFromTop(2);
    }

    const int leftW = isCompact ? (area.getWidth() * 52 / 100) : std::min(360, area.getWidth() / 2);
    auto leftCol = area.removeFromLeft(leftW).reduced(isCompact ? 3 : 8);

    const int pttH = isCompact ? 32 : 56;
    auto pttRow = leftCol.removeFromTop(pttH);
    const int pttW = isCompact ? std::max(80, pttRow.getWidth() * 6 / 10) : 200;
    pttButton.setBounds(pttRow.removeFromLeft(pttW).reduced(1));
    latchToggleBtn.setBounds(pttRow.reduced(1));

    leftCol.removeFromTop(isCompact ? 4 : 8);
    modeSelector.setBounds(leftCol.removeFromTop(isCompact ? 22 : 28).reduced(1));
    leftCol.removeFromTop(isCompact ? 3 : 6);

    const int rowH = isCompact ? 20 : 32;
    gainSlider.setBounds(leftCol.removeFromTop(rowH).reduced(1));

    auto hpfRow = leftCol.removeFromTop(rowH);
    hpfBtn.setBounds(hpfRow.removeFromLeft(isCompact ? 42 : 50).reduced(1));
    hpfSlider.setBounds(hpfRow.reduced(1));

    auto gateRow = leftCol.removeFromTop(rowH);
    gateBtn.setBounds(gateRow.removeFromLeft(isCompact ? 42 : 50).reduced(1));
    gateSlider.setBounds(gateRow.reduced(1));

    duckerSlider.setBounds(leftCol.removeFromTop(rowH).reduced(1));

    area.removeFromLeft(isCompact ? 6 : 16);
    auto rightCol = area.reduced(isCompact ? 3 : 8);

    const int btnH = isCompact ? 20 : 32;
    soloABtn.setBounds(rightCol.removeFromTop(btnH).reduced(1));
    soloBBtn.setBounds(rightCol.removeFromTop(btnH).reduced(1));
    mainLrBtn.setBounds(rightCol.removeFromTop(btnH).reduced(1));

    rightCol.removeFromTop(isCompact ? 3 : 8);
    auto bulkRow = rightCol.removeFromTop(btnH);
    allAuxBtn.setBounds(
        bulkRow.removeFromLeft(bulkRow.getWidth() / 2).reduced(1));
    allMatrixBtn.setBounds(bulkRow.reduced(1));

    rightCol.removeFromTop(isCompact ? 3 : 8);
    const int auxBankH = isCompact ? 16 : 28;
    for (int b = 0; b < 6; ++b) {
      auxBankBtns[static_cast<size_t>(b)].setBounds(
          rightCol.removeFromTop(auxBankH).reduced(1));
    }
  }

private:
  MixerEngine &engine;

  juce::Label title, subTitle;
  juce::TextButton pttButton, latchToggleBtn;
  juce::ComboBox modeSelector;

  juce::Slider gainSlider, hpfSlider, gateSlider, duckerSlider;
  juce::TextButton hpfBtn, gateBtn;

  juce::TextButton soloABtn, soloBBtn, mainLrBtn;
  juce::TextButton allAuxBtn, allMatrixBtn;
  std::array<juce::TextButton, 6> auxBankBtns;

  void setupSlider(juce::Slider &s, const juce::String &suffix, double minVal,
                   double maxVal, double step, std::function<void()> onChange) {
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setRange(minVal, maxVal, step);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 20);
    s.setTextValueSuffix(" " + suffix);
    s.onValueChange = std::move(onChange);
    addAndMakeVisible(s);
  }
};

// ==============================================================================
// Phase 87: OSC & Wireless iPad Remote Protocol Server (MIDAS Chapter 27)
// ==============================================================================

} // namespace nova
