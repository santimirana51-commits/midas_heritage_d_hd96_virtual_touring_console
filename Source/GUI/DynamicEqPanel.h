#pragma once
#include "GUI/Meter.h"
#include "GUI/NovaRotaryKnob.h"
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

// ==============================================================================
// UNIFIED TDR NOVA-STYLE PARALLEL DYNAMIC EQUALIZER
// ==============================================================================
class DynamicEqPanel : public juce::Component, private juce::Timer {
public:
  explicit DynamicEqPanel(MixerEngine &e, int initialChannel = 1) : engine(e) {
    setOpaque(true);

    // Channel Selector (hidden backwards-compatible combobox for tests/dialogs)
    channelSelector.addItem("CH 1", 1);
    for (int i = 2; i <= kLogicalChannels; ++i)
      channelSelector.addItem("CH " + juce::String(i), i);
    channelSelector.setSelectedId(
        juce::jlimit(1, kLogicalChannels, initialChannel),
        juce::dontSendNotification);
    channelSelector.onChange = [this] { syncFromState(); };
    addChildComponent(channelSelector);

    // -------------------------------------------------------------------------
    // AREA 1: RTA CONTROLS (IN, OUT, SC, OFF)
    // -------------------------------------------------------------------------
    static const char *rtaLabels[4] = {"IN", "OUT", "SC", "OFF"};
    for (int i = 0; i < 4; ++i) {
      btnRtaMode[i] = std::make_unique<juce::TextButton>(rtaLabels[i]);
      btnRtaMode[i]->setClickingTogglesState(true);
      btnRtaMode[i]->setRadioGroupId(1001);
      btnRtaMode[i]->setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff1e293b));
      btnRtaMode[i]->setColour(juce::TextButton::buttonOnColourId,
                               juce::Colour(0xff0284c7));
      btnRtaMode[i]->setColour(juce::TextButton::textColourOnId,
                               juce::Colours::white);
      btnRtaMode[i]->setColour(juce::TextButton::textColourOffId,
                               juce::Colour(0xff94a3b8));
      addAndMakeVisible(*btnRtaMode[i]);
      const int m = i;
      btnRtaMode[i]->onClick = [this, m] {
        rtaSourceMode = m;
        repaint();
      };
    }
    btnRtaMode[1]->setToggleState(true,
                                  juce::dontSendNotification); // Default: OUT

    // -------------------------------------------------------------------------
    // AREA 2: BAND SELECTOR (I, II, III, IV + ON + SOLO + ORDER + BYPASS + GR)
    // -------------------------------------------------------------------------
    static const char *bandNames[4] = {"I", "II", "III", "IV"};
    for (int b = 0; b < 4; ++b) {
      btnBandSelect[b] = std::make_unique<juce::TextButton>(bandNames[b]);
      btnBandSelect[b]->setClickingTogglesState(true);
      btnBandSelect[b]->setRadioGroupId(1002);
      btnBandSelect[b]->setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(0xff1e293b));
      btnBandSelect[b]->setColour(juce::TextButton::buttonOnColourId,
                                  juce::Colour(bandColours[b]));
      btnBandSelect[b]->setColour(juce::TextButton::textColourOnId,
                                  juce::Colours::white);
      addAndMakeVisible(*btnBandSelect[b]);
      const int bandIdx = b;
      btnBandSelect[b]->onClick = [this, bandIdx] { selectBand(bandIdx); };

      btnBandOn[b] = std::make_unique<juce::TextButton>("ON");
      btnBandOn[b]->setClickingTogglesState(true);
      btnBandOn[b]->setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff0f172a));
      btnBandOn[b]->setColour(juce::TextButton::buttonOnColourId,
                              juce::Colour(0xff0ea5e9));
      btnBandOn[b]->setToggleState(true, juce::dontSendNotification);
      addAndMakeVisible(*btnBandOn[b]);
      btnBandOn[b]->onClick = [this, bandIdx] {
        auto &s = current();
        const bool active = btnBandOn[bandIdx]->getToggleState();
        s.dynEqEnabled[static_cast<size_t>(bandIdx)].store(
            active, std::memory_order_relaxed);
        repaint();
      };
    }
    btnBandSelect[0]->setToggleState(true, juce::dontSendNotification);

    btnWBandSelect.setButtonText("W");
    btnWBandSelect.setClickingTogglesState(true);
    btnWBandSelect.setRadioGroupId(1002);
    btnWBandSelect.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1e293b));
    btnWBandSelect.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff38bdf8));
    btnWBandSelect.setColour(juce::TextButton::textColourOnId,
                             juce::Colours::white);
    addAndMakeVisible(btnWBandSelect);
    btnWBandSelect.onClick = [this] { selectWBand(); };

    btnBandSolo.setButtonText("BAND\nSOLO");
    btnBandSolo.setClickingTogglesState(true);
    btnBandSolo.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff1e293b));
    btnBandSolo.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xfff59e0b));
    addAndMakeVisible(btnBandSolo);
    btnBandSolo.onClick = [this] {
      current().dynEqSoloBand.store(
          btnBandSolo.getToggleState() ? selectedBand : -1,
          std::memory_order_relaxed);
      repaint();
    };

    btnOrderByFreq.setButtonText("ORDER\nBY\nFREQ");
    btnOrderByFreq.setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1e293b));
    btnOrderByFreq.setColour(juce::TextButton::textColourOffId,
                             juce::Colour(0xffcbd5e1));
    addAndMakeVisible(btnOrderByFreq);
    btnOrderByFreq.onClick = [this] { orderBandsByFrequency(); };

    btnSplit.setButtonText("SPLIT");
    btnSplit.setClickingTogglesState(true);
    btnSplit.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff1e293b));
    btnSplit.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff0284c7));
    addAndMakeVisible(btnSplit);
    btnSplit.onClick = [this] {
      current().dynEqSplit[static_cast<size_t>(selectedBand)].store(
          btnSplit.getToggleState(), std::memory_order_relaxed);
      repaint();
    };

    btnStereoMode.setButtonText("STEREO");
    btnStereoMode.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff1e293b));
    btnStereoMode.setColour(juce::TextButton::textColourOffId,
                            juce::Colour(0xffcbd5e1));
    addAndMakeVisible(btnStereoMode);
    btnStereoMode.onClick = [this] {
      auto &s = current();
      const int next =
          (s.dynEqStereoMode[static_cast<size_t>(selectedBand)].load(
               std::memory_order_relaxed) +
           1) %
          6;
      s.dynEqStereoMode[static_cast<size_t>(selectedBand)].store(
          next, std::memory_order_relaxed);
      updateStereoModeLabel(next);
      repaint();
    };

    btnExtSc.setButtonText("INT SC");
    btnExtSc.setClickingTogglesState(true);
    btnExtSc.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff1e293b));
    btnExtSc.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xff8b5cf6));
    addAndMakeVisible(btnExtSc);
    btnExtSc.onClick = [this] {
      const bool ext = btnExtSc.getToggleState();
      current().dynEqExtSc.store(ext, std::memory_order_relaxed);
      btnExtSc.setButtonText(ext ? "EXT SC" : "INT SC");
      repaint();
    };

    btnElBypass.setButtonText("EL");
    btnElBypass.setClickingTogglesState(true);
    btnElBypass.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff1e293b));
    btnElBypass.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colour(0xff0ea5e9));
    addAndMakeVisible(btnElBypass);
    btnElBypass.onClick = [this] {
      current().dynEqElBypass.store(btnElBypass.getToggleState(),
                                    std::memory_order_relaxed);
      repaint();
    };

    btnBypass.setButtonText("BYPASS");
    btnBypass.setClickingTogglesState(true);
    btnBypass.setColour(juce::TextButton::buttonColourId,
                        juce::Colour(0xff1e293b));
    btnBypass.setColour(juce::TextButton::buttonOnColourId,
                        juce::Colour(0xffef4444));
    addAndMakeVisible(btnBypass);
    btnBypass.onClick = [this] {
      auto &s = current();
      s.eqEnabled.store(!btnBypass.getToggleState(), std::memory_order_relaxed);
      repaint();
    };

    btnGrDelta.setButtonText("GR DELTA");
    btnGrDelta.setClickingTogglesState(true);
    btnGrDelta.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff1e293b));
    btnGrDelta.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour(0xff10b981));
    addAndMakeVisible(btnGrDelta);
    btnGrDelta.onClick = [this] {
      current().dynEqGrDelta.store(btnGrDelta.getToggleState(),
                                   std::memory_order_relaxed);
      repaint();
    };

    // -------------------------------------------------------------------------
    // AREA 3: GROUP A - FILTER CONTROL (HP & LP)
    // -------------------------------------------------------------------------
    btnHp.setButtonText("HP");
    btnHp.setClickingTogglesState(true);
    btnHp.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnHp.setColour(juce::TextButton::buttonOnColourId,
                    juce::Colour(0xff38bdf8));
    addAndMakeVisible(btnHp);
    btnHp.onClick = [this] {
      const bool on = btnHp.getToggleState();
      current().highPassEnabled.store(on, std::memory_order_relaxed);
      current().dynEqHpfEnabled.store(on, std::memory_order_relaxed);
      repaint();
    };

    btnLp.setButtonText("LP");
    btnLp.setClickingTogglesState(true);
    btnLp.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnLp.setColour(juce::TextButton::buttonOnColourId,
                    juce::Colour(0xff38bdf8));
    addAndMakeVisible(btnLp);
    btnLp.onClick = [this] {
      const bool on = btnLp.getToggleState();
      current().lowPassEnabled.store(on, std::memory_order_relaxed);
      current().dynEqLpfEnabled.store(on, std::memory_order_relaxed);
      repaint();
    };

    lblHpFreq.setRange(10.0f, 1000.0f);
    lblHpFreq.setFreq(33.0f, false);
    lblHpFreq.onFreqChanged = [this](float f) {
      current().highPassFreqHz.store(f, std::memory_order_relaxed);
      current().dynEqHpfFreqHz.store(f, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(lblHpFreq);

    lblLpFreq.setRange(1000.0f, 40000.0f);
    lblLpFreq.setFreq(30000.0f, false);
    lblLpFreq.onFreqChanged = [this](float f) {
      current().lowPassFreqHz.store(f, std::memory_order_relaxed);
      current().dynEqLpfFreqHz.store(f, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(lblLpFreq);

    btnHpSlope.setButtonText("24dB/oct");
    btnHpSlope.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff0f172a));
    btnHpSlope.setColour(juce::TextButton::textColourOffId,
                         juce::Colour(0xff94a3b8));
    addAndMakeVisible(btnHpSlope);
    btnHpSlope.onClick = [this] {
      int s = (current().dynEqHpfSlope.load(std::memory_order_relaxed) + 1) % 4;
      current().dynEqHpfSlope.store(s, std::memory_order_relaxed);
      current().highPassSlope.store(s <= 1 ? 0 : 1, std::memory_order_relaxed);
      static const char *slopeTxt[4] = {"6dB/oct", "12dB/oct", "18dB/oct",
                                        "24dB/oct"};
      btnHpSlope.setButtonText(slopeTxt[s]);
      repaint();
    };

    btnLpSlope.setButtonText("24dB/oct");
    btnLpSlope.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff0f172a));
    btnLpSlope.setColour(juce::TextButton::textColourOffId,
                         juce::Colour(0xff94a3b8));
    addAndMakeVisible(btnLpSlope);
    btnLpSlope.onClick = [this] {
      int s = (current().dynEqLpfSlope.load(std::memory_order_relaxed) + 1) % 4;
      current().dynEqLpfSlope.store(s, std::memory_order_relaxed);
      current().lowPassSlope.store(s <= 1 ? 0 : 1, std::memory_order_relaxed);
      static const char *slopeTxt[4] = {"6dB/oct", "12dB/oct", "18dB/oct",
                                        "24dB/oct"};
      btnLpSlope.setButtonText(slopeTxt[s]);
      repaint();
    };

    // -------------------------------------------------------------------------
    // AREA 3: GROUP B - FILTER SHAPES (Low Shelf, Bell, High Shelf)
    // -------------------------------------------------------------------------
    static const char *shapeNames[3] = {"__/", "/\\", "\\__"};
    for (int i = 0; i < 3; ++i) {
      btnShape[i] = std::make_unique<juce::TextButton>(shapeNames[i]);
      btnShape[i]->setClickingTogglesState(true);
      btnShape[i]->setRadioGroupId(1003);
      btnShape[i]->setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1e293b));
      btnShape[i]->setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff0284c7));
      addAndMakeVisible(*btnShape[i]);
      const int sIdx = i;
      btnShape[i]->onClick = [this, sIdx] {
        bandShapes[selectedBand] = sIdx;
        current().dynEqShape[static_cast<size_t>(selectedBand)].store(
            sIdx, std::memory_order_relaxed);
        repaint();
      };
    }
    btnShape[1]->setToggleState(true, juce::dontSendNotification);

    // -------------------------------------------------------------------------
    // AREA 3: GROUP C - PARAMETRIC EQ CONTROLS (Q, FREQ, GAIN)
    // -------------------------------------------------------------------------
    knobQ = std::make_unique<NovaRotaryKnob>("Q", "", 0.2f, 10.0f, 1.0f);
    knobQ->setDisplayDecimals(2);
    knobQ->onValueChanged = [this](float v) {
      current().eqQ[static_cast<size_t>(selectedBand)].store(
          v, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(*knobQ);

    knobFreq = std::make_unique<NovaRotaryKnob>("FREQ", "Hz", 10.0f, 40000.0f,
                                                1000.0f);
    knobFreq->setLogarithmic(true);
    knobFreq->onValueChanged = [this](float v) {
      current().eqFrequency[static_cast<size_t>(selectedBand)].store(
          v, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(*knobFreq);

    knobGain = std::make_unique<NovaRotaryKnob>(
        "GAIN", "dB", -12.0f, 12.0f, 0.0f, true, juce::Colour(0xfff59e0b));
    knobGain->setDisplayDecimals(1);
    knobGain->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandGainDb.store(v, std::memory_order_relaxed);
      else
        current().eqGainDb[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(*knobGain);

    // -------------------------------------------------------------------------
    // AREA 3: GROUP D - DYNAMIC EQ CONTROLS (THRESHOLD, WB, RATIO, ATTACK, REL,
    // MODE, RANGE, KNEE)
    // -------------------------------------------------------------------------
    knobThreshold = std::make_unique<NovaRotaryKnob>("THRESHOLD", "dB", -60.0f,
                                                     0.0f, -20.0f, true,
                                                     juce::Colour(0xff0284c7));
    knobThreshold->setDisplayDecimals(1);
    knobThreshold->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandThresholdDb.store(v, std::memory_order_relaxed);
      else
        current().dynEqThresholdDb[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(*knobThreshold);

    btnWb.setButtonText("WB");
    btnWb.setClickingTogglesState(true);
    btnWb.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnWb.setColour(juce::TextButton::buttonOnColourId,
                    juce::Colour(0xff38bdf8));
    addAndMakeVisible(btnWb);
    btnWb.onClick = [this] {
      current().dynEqWBandEnabled.store(btnWb.getToggleState(),
                                        std::memory_order_relaxed);
      repaint();
    };

    knobRatio =
        std::make_unique<NovaRotaryKnob>("RATIO", ":1", 1.0f, 20.0f, 4.0f);
    knobRatio->setDisplayDecimals(1);
    knobRatio->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandRatio.store(v, std::memory_order_relaxed);
      else
        current().dynEqRatio[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(*knobRatio);

    knobAttack =
        std::make_unique<NovaRotaryKnob>("ATTACK", "ms", 0.5f, 500.0f, 10.0f);
    knobAttack->setLogarithmic(true);
    knobAttack->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandAttackMs.store(v, std::memory_order_relaxed);
      else
        current().dynEqAttackMs[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
    };
    addAndMakeVisible(*knobAttack);

    knobRelease = std::make_unique<NovaRotaryKnob>("RELEASE", "ms", 10.0f,
                                                   2000.0f, 100.0f);
    knobRelease->setLogarithmic(true);
    knobRelease->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandReleaseMs.store(v, std::memory_order_relaxed);
      else
        current().dynEqReleaseMs[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
    };
    addAndMakeVisible(*knobRelease);

    btnModeAboveBelow.setButtonText("ABOVE");
    btnModeAboveBelow.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff1e293b));
    btnModeAboveBelow.setColour(juce::TextButton::textColourOffId,
                                juce::Colour(0xff38bdf8));
    addAndMakeVisible(btnModeAboveBelow);
    btnModeAboveBelow.onClick = [this] {
      auto &s = current();
      if (editingWBand) {
        int m = (s.dynEqWBandMode.load(std::memory_order_relaxed) == 0) ? 1 : 0;
        s.dynEqWBandMode.store(m, std::memory_order_relaxed);
        btnModeAboveBelow.setButtonText(m == 0 ? "ABOVE" : "BELOW");
      } else {
        int m = s.dynEqMode[static_cast<size_t>(selectedBand)].load(
            std::memory_order_relaxed);
        m = (m == 0) ? 1 : 0;
        s.dynEqMode[static_cast<size_t>(selectedBand)].store(
            m, std::memory_order_relaxed);
        btnModeAboveBelow.setButtonText(m == 0 ? "ABOVE" : "BELOW");
      }
      repaint();
    };

    knobRange =
        std::make_unique<NovaRotaryKnob>("RANGE", "dB", -24.0f, 24.0f, -6.0f);
    knobRange->setDisplayDecimals(1);
    knobRange->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandRangeDb.store(v, std::memory_order_relaxed);
      else
        current().dynEqRangeDb[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
      repaint();
    };
    addAndMakeVisible(*knobRange);

    knobKnee =
        std::make_unique<NovaRotaryKnob>("KNEE", "dB", 0.0f, 12.0f, 3.0f);
    knobKnee->setDisplayDecimals(1);
    knobKnee->onValueChanged = [this](float v) {
      if (editingWBand)
        current().dynEqWBandKneeDb.store(v, std::memory_order_relaxed);
      else
        current().dynEqKneeDb[static_cast<size_t>(selectedBand)].store(
            v, std::memory_order_relaxed);
    };
    addAndMakeVisible(*knobKnee);

    // -------------------------------------------------------------------------
    // AREA 3: GROUP E - OUTPUT & DRY MIX CONTROLS
    // -------------------------------------------------------------------------
    knobEqGain =
        std::make_unique<NovaRotaryKnob>("EQ GAIN", "dB", -18.0f, 18.0f, 0.0f);
    knobEqGain->setDisplayDecimals(1);
    knobEqGain->onValueChanged = [this](float v) {
      current().dynEqEqGainDb.store(v, std::memory_order_relaxed);
    };
    addAndMakeVisible(*knobEqGain);

    knobDryMix =
        std::make_unique<NovaRotaryKnob>("DRY MIX", "%", 0.0f, 100.0f, 0.0f);
    knobDryMix->setDisplayDecimals(1);
    knobDryMix->onValueChanged = [this](float v) {
      current().dynEqDryMixPct.store(v, std::memory_order_relaxed);
    };
    addAndMakeVisible(*knobDryMix);

    knobOutGain =
        std::make_unique<NovaRotaryKnob>("OUT GAIN", "dB", -18.0f, 18.0f, 0.0f);
    knobOutGain->setDisplayDecimals(1);
    knobOutGain->onValueChanged = [this](float v) {
      current().dynEqOutGainDb.store(v, std::memory_order_relaxed);
    };
    addAndMakeVisible(*knobOutGain);

    setSize(960, 580);
    syncFromState();
    startTimerHz(30);
  }

  ~DynamicEqPanel() override { stopTimer(); }

  void setSelectedChannel(int chIdx) {
    channelSelector.setSelectedId(juce::jlimit(1, kLogicalChannels, chIdx + 1),
                                  juce::dontSendNotification);
    syncFromState();
  }

  void selectBand(int b) {
    editingWBand = false;
    selectedBand = juce::jlimit(0, 3, b);
    for (int i = 0; i < 4; ++i)
      btnBandSelect[i]->setToggleState(i == selectedBand,
                                       juce::dontSendNotification);
    btnWBandSelect.setToggleState(false, juce::dontSendNotification);

    btnShape[bandShapes[selectedBand]]->setToggleState(
        true, juce::dontSendNotification);

    auto &s = current();
    knobQ->setValue(s.eqQ[static_cast<size_t>(selectedBand)].load(
                        std::memory_order_relaxed),
                    juce::dontSendNotification);
    knobFreq->setValue(s.eqFrequency[static_cast<size_t>(selectedBand)].load(
                           std::memory_order_relaxed),
                       juce::dontSendNotification);
    knobGain->setValue(s.eqGainDb[static_cast<size_t>(selectedBand)].load(
                           std::memory_order_relaxed),
                       juce::dontSendNotification);
    knobGain->setAccentColour(juce::Colour(bandColours[selectedBand]));

    knobThreshold->setValue(
        s.dynEqThresholdDb[static_cast<size_t>(selectedBand)].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    knobRatio->setValue(s.dynEqRatio[static_cast<size_t>(selectedBand)].load(
                            std::memory_order_relaxed),
                        juce::dontSendNotification);
    knobAttack->setValue(
        s.dynEqAttackMs[static_cast<size_t>(selectedBand)].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    knobRelease->setValue(
        s.dynEqReleaseMs[static_cast<size_t>(selectedBand)].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    knobRange->setValue(s.dynEqRangeDb[static_cast<size_t>(selectedBand)].load(
                            std::memory_order_relaxed),
                        juce::dontSendNotification);
    knobKnee->setValue(s.dynEqKneeDb[static_cast<size_t>(selectedBand)].load(
                           std::memory_order_relaxed),
                       juce::dontSendNotification);

    const int m = s.dynEqMode[static_cast<size_t>(selectedBand)].load(
        std::memory_order_relaxed);
    btnModeAboveBelow.setButtonText(m == 0 ? "ABOVE" : "BELOW");

    btnSplit.setToggleState(
        s.dynEqSplit[static_cast<size_t>(selectedBand)].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    updateStereoModeLabel(
        s.dynEqStereoMode[static_cast<size_t>(selectedBand)].load(
            std::memory_order_relaxed));
    btnBandSolo.setToggleState(
        s.dynEqSoloBand.load(std::memory_order_relaxed) == selectedBand,
        juce::dontSendNotification);

    repaint();
  }

  void selectWBand() {
    editingWBand = true;
    for (int i = 0; i < 4; ++i)
      btnBandSelect[i]->setToggleState(false, juce::dontSendNotification);
    btnWBandSelect.setToggleState(true, juce::dontSendNotification);

    auto &s = current();
    knobGain->setValue(s.dynEqWBandGainDb.load(std::memory_order_relaxed),
                       juce::dontSendNotification);
    knobGain->setAccentColour(juce::Colour(0xff38bdf8));
    knobThreshold->setValue(
        s.dynEqWBandThresholdDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    knobRatio->setValue(s.dynEqWBandRatio.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    knobAttack->setValue(s.dynEqWBandAttackMs.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    knobRelease->setValue(s.dynEqWBandReleaseMs.load(std::memory_order_relaxed),
                          juce::dontSendNotification);
    knobRange->setValue(s.dynEqWBandRangeDb.load(std::memory_order_relaxed),
                        juce::dontSendNotification);
    knobKnee->setValue(s.dynEqWBandKneeDb.load(std::memory_order_relaxed),
                       juce::dontSendNotification);
    const int wm = s.dynEqWBandMode.load(std::memory_order_relaxed);
    btnModeAboveBelow.setButtonText(wm == 0 ? "ABOVE" : "BELOW");
    repaint();
  }

  void orderBandsByFrequency() {
    auto &s = current();
    struct BandData {
      float f, g, q, thresh, range, ratio, knee, att, rel;
      int shape, mode;
      bool dynOn;
    };
    std::array<BandData, 4> bands;
    for (size_t b = 0; b < 4; ++b) {
      bands[b].f = s.eqFrequency[b].load(std::memory_order_relaxed);
      bands[b].g = s.eqGainDb[b].load(std::memory_order_relaxed);
      bands[b].q = s.eqQ[b].load(std::memory_order_relaxed);
      bands[b].thresh = s.dynEqThresholdDb[b].load(std::memory_order_relaxed);
      bands[b].range = s.dynEqRangeDb[b].load(std::memory_order_relaxed);
      bands[b].ratio = s.dynEqRatio[b].load(std::memory_order_relaxed);
      bands[b].knee = s.dynEqKneeDb[b].load(std::memory_order_relaxed);
      bands[b].att = s.dynEqAttackMs[b].load(std::memory_order_relaxed);
      bands[b].rel = s.dynEqReleaseMs[b].load(std::memory_order_relaxed);
      bands[b].shape = bandShapes[b];
      bands[b].mode = s.dynEqMode[b].load(std::memory_order_relaxed);
      bands[b].dynOn = s.dynEqEnabled[b].load(std::memory_order_relaxed);
    }
    std::sort(bands.begin(), bands.end(),
              [](const BandData &a, const BandData &b) { return a.f < b.f; });
    for (size_t b = 0; b < 4; ++b) {
      s.eqFrequency[b].store(bands[b].f, std::memory_order_relaxed);
      s.eqGainDb[b].store(bands[b].g, std::memory_order_relaxed);
      s.eqQ[b].store(bands[b].q, std::memory_order_relaxed);
      s.dynEqThresholdDb[b].store(bands[b].thresh, std::memory_order_relaxed);
      s.dynEqRangeDb[b].store(bands[b].range, std::memory_order_relaxed);
      s.dynEqRatio[b].store(bands[b].ratio, std::memory_order_relaxed);
      s.dynEqKneeDb[b].store(bands[b].knee, std::memory_order_relaxed);
      s.dynEqAttackMs[b].store(bands[b].att, std::memory_order_relaxed);
      s.dynEqReleaseMs[b].store(bands[b].rel, std::memory_order_relaxed);
      bandShapes[b] = bands[b].shape;
      s.dynEqMode[b].store(bands[b].mode, std::memory_order_relaxed);
      s.dynEqEnabled[b].store(bands[b].dynOn, std::memory_order_relaxed);
    }
    selectBand(selectedBand);
  }

  void syncFromState() {
    auto &s = current();
    btnBypass.setToggleState(!s.eqEnabled.load(std::memory_order_relaxed),
                             juce::dontSendNotification);
    btnHp.setToggleState(s.highPassEnabled.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    btnLp.setToggleState(s.lowPassEnabled.load(std::memory_order_relaxed),
                         juce::dontSendNotification);

    const float hpF = s.highPassFreqHz.load(std::memory_order_relaxed);
    lblHpFreq.setFreq(hpF, false);

    const float lpF = s.lowPassFreqHz.load(std::memory_order_relaxed);
    lblLpFreq.setFreq(lpF, false);

    for (int b = 0; b < 4; ++b) {
      btnBandOn[b]->setToggleState(s.dynEqEnabled[static_cast<size_t>(b)].load(
                                       std::memory_order_relaxed),
                                   juce::dontSendNotification);
      bandShapes[b] = std::clamp(
          s.dynEqShape[static_cast<size_t>(b)].load(std::memory_order_relaxed),
          0, 2);
    }

    btnWb.setToggleState(s.dynEqWBandEnabled.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    btnElBypass.setToggleState(s.dynEqElBypass.load(std::memory_order_relaxed),
                               juce::dontSendNotification);
    btnGrDelta.setToggleState(s.dynEqGrDelta.load(std::memory_order_relaxed),
                              juce::dontSendNotification);
    const bool extSc = s.dynEqExtSc.load(std::memory_order_relaxed);
    btnExtSc.setToggleState(extSc, juce::dontSendNotification);
    btnExtSc.setButtonText(extSc ? "EXT SC" : "INT SC");

    knobEqGain->setValue(s.dynEqEqGainDb.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    knobDryMix->setValue(s.dynEqDryMixPct.load(std::memory_order_relaxed),
                         juce::dontSendNotification);
    knobOutGain->setValue(s.dynEqOutGainDb.load(std::memory_order_relaxed),
                          juce::dontSendNotification);

    selectBand(selectedBand);
  }

  float calculateEqResponseAt(float freq, bool withDynamics) const noexcept {
    auto &s = current();
    if (!s.eqEnabled.load(std::memory_order_relaxed))
      return 0.0f;

    float totalDb = 0.0f;

    // HPF contribution
    if (s.highPassEnabled.load(std::memory_order_relaxed)) {
      const float hpf = s.highPassFreqHz.load(std::memory_order_relaxed);
      if (freq < hpf) {
        const float slopeDb =
            (s.highPassSlope.load(std::memory_order_relaxed) == 0 ? 12.0f
                                                                  : 24.0f);
        totalDb -= std::log2(hpf / std::max(1.0f, freq)) * slopeDb;
      }
    }

    // LPF contribution
    if (s.lowPassEnabled.load(std::memory_order_relaxed)) {
      const float lpf = s.lowPassFreqHz.load(std::memory_order_relaxed);
      if (freq > lpf) {
        const float slopeDb =
            (s.lowPassSlope.load(std::memory_order_relaxed) == 0 ? 12.0f
                                                                 : 24.0f);
        totalDb -= std::log2(freq / std::max(1.0f, lpf)) * slopeDb;
      }
    }

    // 4 PEQ Bands with shape handling + Spectral Complement W-Band
    float bandCoverage = 0.0f;
    for (size_t b = 0; b < 4; ++b) {
      const float bf = s.eqFrequency[b].load(std::memory_order_relaxed);
      const float bg = s.eqGainDb[b].load(std::memory_order_relaxed);
      const float bq = std::max(0.1f, s.eqQ[b].load(std::memory_order_relaxed));
      const int shape = bandShapes[b];

      float shapeGain = 0.0f;
      if (shape == 0) { // Low Shelf
        const float ratio = freq / std::max(10.0f, bf);
        shapeGain = 1.0f / (1.0f + std::pow(ratio, 2.0f * bq));
      } else if (shape == 2) { // High Shelf
        const float ratio = std::max(10.0f, bf) / std::max(1.0f, freq);
        shapeGain = 1.0f / (1.0f + std::pow(ratio, 2.0f * bq));
      } else { // Bell
        const float octDist = std::log2(freq / std::max(10.0f, bf));
        shapeGain = std::exp(-0.5f * std::pow(octDist * bq * 1.5f, 2.0f));
      }

      float effectiveGain = bg;
      if (withDynamics && s.dynEqEnabled[b].load(std::memory_order_relaxed)) {
        effectiveGain += liveGrDb[b];
      }
      totalDb += effectiveGain * shapeGain;
      bandCoverage = std::min(1.0f, bandCoverage + shapeGain);
    }

    // Spectral Complement W-Band contribution outside active band coverage
    const float wStatic = s.dynEqWBandGainDb.load(std::memory_order_relaxed);
    const float wDyn =
        (withDynamics && s.dynEqWBandEnabled.load(std::memory_order_relaxed))
            ? liveWBandGrDb
            : 0.0f;
    totalDb += (wStatic + wDyn) * (1.0f - bandCoverage);
    totalDb += s.dynEqEqGainDb.load(std::memory_order_relaxed);

    return totalDb;
  }

  void timerCallback() override {
    auto &s = current();
    for (size_t b = 0; b < 4; ++b) {
      liveGrDb[b] = s.dynEqGainDb[b].load(std::memory_order_relaxed);
    }
    liveWBandGrDb = s.dynEqWBandDynGainDb.load(std::memory_order_relaxed);

    // Live Dual-FFT RTA Spectrum Acquisition
    if (rtaSourceMode != 3) { // not OFF
      const double sr =
          engine.getSampleRate() > 1000.0 ? engine.getSampleRate() : 48000.0;
      const float linPk = s.peak.load(std::memory_order_relaxed);
      const float chPeakDb = (linPk > 1.0e-5f)
                                 ? juce::Decibels::gainToDecibels(linPk, -60.0f)
                                 : -60.0f;

      if (engine.rtaAnalyzer.hasNewData()) {
        std::array<double, nova::graviton::DualFFTAnalyzer::kFFTHalf> specA{},
            specB{}, coh{}, tf{};
        int bins = 0;
        engine.rtaAnalyzer.getSpectrumData(specA.data(), specB.data(),
                                           coh.data(), tf.data(), bins);

        for (int i = 0; i < kRtaPoints; ++i) {
          const float t = i / static_cast<float>(kRtaPoints - 1);
          const float freq = 10.0f * std::pow(4000.0f, t);
          const int bin = std::clamp(
              static_cast<int>(std::round(freq * 2048.0 / sr)), 0, bins - 1);

          float rawDb = -60.0f;
          if (rtaSourceMode == 0) { // IN (Pre-EQ)
            rawDb = static_cast<float>(specA[static_cast<size_t>(bin)]);
          } else if (rtaSourceMode == 1) { // OUT (Post-EQ)
            const float eqResp = calculateEqResponseAt(freq, true);
            rawDb =
                static_cast<float>(specA[static_cast<size_t>(bin)]) + eqResp;
          } else if (rtaSourceMode == 2) { // SC (Sidechain / Transfer Function)
            rawDb = static_cast<float>(specA[static_cast<size_t>(bin)] +
                                       tf[static_cast<size_t>(bin)]);
          }

          if (chPeakDb > -58.0f) {
            const float pinkTilt =
                -3.0f * std::log2(std::max(10.0f, freq) / 1000.0f);
            const float chFloor =
                chPeakDb - 14.0f + std::clamp(pinkTilt, -12.0f, 6.0f);
            rawDb = std::max(rawDb, chFloor);
          }

          // Fast attack, smooth ballistic natural decay (~25 dB/s)
          if (rawDb > rtaLevelsDb[static_cast<size_t>(i)]) {
            rtaLevelsDb[static_cast<size_t>(i)] = rawDb;
          } else {
            rtaLevelsDb[static_cast<size_t>(i)] =
                rtaLevelsDb[static_cast<size_t>(i)] * 0.84f + rawDb * 0.16f;
          }
        }
      } else {
        // Natural decay between frames or during signal pauses
        for (int i = 0; i < kRtaPoints; ++i) {
          const float t = i / static_cast<float>(kRtaPoints - 1);
          const float freq = 10.0f * std::pow(4000.0f, t);
          float floorDb = -60.0f;
          if (chPeakDb > -58.0f) {
            const float eqResp =
                (rtaSourceMode == 1) ? calculateEqResponseAt(freq, true) : 0.0f;
            const float pinkTilt =
                -3.0f * std::log2(std::max(10.0f, freq) / 1000.0f);
            floorDb =
                chPeakDb - 14.0f + std::clamp(pinkTilt, -12.0f, 6.0f) + eqResp;
          }
          if (rtaLevelsDb[static_cast<size_t>(i)] > floorDb)
            rtaLevelsDb[static_cast<size_t>(i)] =
                std::max(floorDb, rtaLevelsDb[static_cast<size_t>(i)] - 1.6f);
          else
            rtaLevelsDb[static_cast<size_t>(i)] =
                rtaLevelsDb[static_cast<size_t>(i)] * 0.85f + floorDb * 0.15f;
        }
      }
    }

    repaint();
  }

  // ---------------------------------------------------------------------------
  // COORDINATE MAPPING (10 Hz to 40 kHz, -12 dB to +12 dB)
  // ---------------------------------------------------------------------------
  float freqToPlotX(float freqHz,
                    const juce::Rectangle<float> &b) const noexcept {
    const float minF = 10.0f, maxF = 40000.0f;
    const float logMin = std::log10(minF), logMax = std::log10(maxF);
    const float logF = std::log10(std::clamp(freqHz, minF, maxF));
    return b.getX() + b.getWidth() * (logF - logMin) / (logMax - logMin);
  }

  float plotXToFreq(float x, const juce::Rectangle<float> &b) const noexcept {
    const float minF = 10.0f, maxF = 40000.0f;
    const float norm = std::clamp((x - b.getX()) / b.getWidth(), 0.0f, 1.0f);
    return minF * std::pow(maxF / minF, norm);
  }

  float gainToPlotY(float gainDb,
                    const juce::Rectangle<float> &b) const noexcept {
    const float midY = b.getCentreY();
    const float halfH = b.getHeight() * 0.44f;
    return midY - (std::clamp(gainDb, -14.0f, 14.0f) / 12.0f) * halfH;
  }

  float plotYToGain(float y, const juce::Rectangle<float> &b) const noexcept {
    const float midY = b.getCentreY();
    const float halfH = b.getHeight() * 0.44f;
    if (halfH <= 1.0e-4f)
      return 0.0f;
    return std::clamp(-((y - midY) / halfH) * 12.0f, -12.0f, 12.0f);
  }

  juce::Rectangle<float> getCurvePlotBounds() const noexcept {
    return graphRect;
  }
  juce::Rectangle<float> getGraphBounds() const noexcept { return graphRect; }
  float freqToX(float freqHz, const juce::Rectangle<float> &b) const noexcept {
    return freqToPlotX(freqHz, b);
  }
  float xToFreq(float x, const juce::Rectangle<float> &b) const noexcept {
    return plotXToFreq(x, b);
  }
  float gainToY(float gainDb, const juce::Rectangle<float> &b) const noexcept {
    return gainToPlotY(gainDb, b);
  }
  float yToGain(float y, const juce::Rectangle<float> &b) const noexcept {
    return plotYToGain(y, b);
  }

  // ---------------------------------------------------------------------------
  // MOUSE INTERACTION (DRAGGING NODES)
  // ---------------------------------------------------------------------------
  void mouseDown(const juce::MouseEvent &e) override {
    if (!graphRect.contains(e.position))
      return;

    auto &s = current();
    draggedNode = -1;

    // Check HP Node
    const float hpF = s.highPassFreqHz.load(std::memory_order_relaxed);
    const float hpX = freqToPlotX(hpF, graphRect);
    const float hpY = gainToPlotY(0.0f, graphRect);
    if (e.position.getDistanceFrom(juce::Point<float>(hpX, hpY)) <= 15.0f) {
      draggedNode = 100; // HP
      repaint();
      return;
    }

    // Check Bands 1..4 Nodes
    for (size_t b = 0; b < 4; ++b) {
      const float f = s.eqFrequency[b].load(std::memory_order_relaxed);
      const float g = s.eqGainDb[b].load(std::memory_order_relaxed);
      const float nx = freqToPlotX(f, graphRect);
      const float ny = gainToPlotY(g, graphRect);
      if (e.position.getDistanceFrom(juce::Point<float>(nx, ny)) <= 16.0f) {
        draggedNode = static_cast<int>(b);
        selectBand(draggedNode);
        repaint();
        return;
      }
    }

    // Check LP Node
    const float lpF = s.lowPassFreqHz.load(std::memory_order_relaxed);
    const float lpX = freqToPlotX(lpF, graphRect);
    const float lpY = gainToPlotY(0.0f, graphRect);
    if (e.position.getDistanceFrom(juce::Point<float>(lpX, lpY)) <= 15.0f) {
      draggedNode = 200; // LP
      repaint();
      return;
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    auto &s = current();
    if (draggedNode == 100) { // HP
      const float newF =
          std::clamp(plotXToFreq(e.position.x, graphRect), 10.0f, 1000.0f);
      s.highPassFreqHz.store(newF, std::memory_order_relaxed);
      lblHpFreq.setFreq(newF, false);
      repaint();
    } else if (draggedNode == 200) { // LP
      const float newF =
          std::clamp(plotXToFreq(e.position.x, graphRect), 1000.0f, 40000.0f);
      s.lowPassFreqHz.store(newF, std::memory_order_relaxed);
      lblLpFreq.setFreq(newF, false);
      repaint();
    } else if (draggedNode >= 0 && draggedNode < 4) {
      const float newF =
          std::clamp(plotXToFreq(e.position.x, graphRect), 20.0f, 20000.0f);
      const float newG =
          std::clamp(plotYToGain(e.position.y, graphRect), -12.0f, 12.0f);
      s.eqFrequency[static_cast<size_t>(draggedNode)].store(
          newF, std::memory_order_relaxed);
      s.eqGainDb[static_cast<size_t>(draggedNode)].store(
          newG, std::memory_order_relaxed);
      knobFreq->setValue(newF, juce::dontSendNotification);
      knobGain->setValue(newG, juce::dontSendNotification);
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent &) override {
    draggedNode = -1;
    repaint();
  }

  void mouseWheelMove(const juce::MouseEvent &e,
                      const juce::MouseWheelDetails &wheel) override {
    if (!graphRect.contains(e.position))
      return;

    auto &s = current();
    int targetBand = draggedNode;
    if (targetBand < 0) {
      float bestDist = 36.0f;
      for (size_t b = 0; b < 4; ++b) {
        const float f = s.eqFrequency[b].load(std::memory_order_relaxed);
        const float g = s.eqGainDb[b].load(std::memory_order_relaxed);
        const float nx = freqToPlotX(f, graphRect);
        const float ny = gainToPlotY(g, graphRect);
        const float d = e.position.getDistanceFrom(juce::Point<float>(nx, ny));
        if (d < bestDist) {
          bestDist = d;
          targetBand = static_cast<int>(b);
        }
      }
    }
    if (targetBand >= 0 && targetBand < 4) {
      const float curQ = s.eqQ[static_cast<size_t>(targetBand)].load(
          std::memory_order_relaxed);
      const float newQ = std::clamp(curQ + wheel.deltaY * 1.5f, 0.2f, 10.0f);
      s.eqQ[static_cast<size_t>(targetBand)].store(newQ,
                                                   std::memory_order_relaxed);
      if (targetBand == selectedBand)
        knobQ->setValue(newQ, juce::dontSendNotification);
      repaint();
    }
  }

  // ---------------------------------------------------------------------------
  // PAINT
  // ---------------------------------------------------------------------------
  void paint(juce::Graphics &g) override {
    const auto area = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff090d13)); // deep dark studio console background

    // -------------------------------------------------------------------------
    // AREA 1: FREQUENCY GRAPH BACKGROUND & GRID
    // -------------------------------------------------------------------------
    if (graphRect.getWidth() > 20.0f && graphRect.getHeight() > 20.0f) {
      g.setColour(juce::Colour(0xff0e131b));
      g.fillRoundedRectangle(graphRect, 6.0f);
      g.setColour(juce::Colour(0xff1e293b));
      g.drawRoundedRectangle(graphRect, 6.0f, 1.2f);

      // Frequency Vertical Grid Lines
      const bool isCompactH = (graphRect.getHeight() < 140.0f);
      static const float gridFreqs[] = {10.0f,   20.0f,    50.0f,    100.0f,
                                        200.0f,  500.0f,   1000.0f,  2000.0f,
                                        5000.0f, 10000.0f, 20000.0f, 40000.0f};
      static const char *gridFreqLabels[] = {"10",  "20",  "50",  "100",
                                             "200", "500", "1k",  "2k",
                                             "5k",  "10k", "20k", "40k"};
      g.setFont(juce::Font(
          juce::FontOptions(isCompactH ? 8.0f : 9.0f, juce::Font::plain)));
      for (size_t i = 0; i < 12; ++i) {
        const float gx = freqToPlotX(gridFreqs[i], graphRect);
        g.setColour(juce::Colour(0x18ffffff));
        g.drawVerticalLine(static_cast<int>(gx), graphRect.getY(),
                           graphRect.getBottom());

        if (!isCompactH || (i % 2 == 1) || i == 0 || i == 11) {
          g.setColour(juce::Colour(0xff64748b));
          g.drawText(gridFreqLabels[i], static_cast<int>(gx) - 15,
                     static_cast<int>(graphRect.getBottom() -
                                      (isCompactH ? 12.0f : 15.0f)),
                     30, isCompactH ? 11 : 14, juce::Justification::centred);
        }
      }

      // Gain Horizontal Grid Lines (-12 to +12 dB)
      static const float gridGains[] = {12.0f, 9.0f,  6.0f,  3.0f,  0.0f,
                                        -3.0f, -6.0f, -9.0f, -12.0f};
      for (float gv : gridGains) {
        const float gy = gainToPlotY(gv, graphRect);
        g.setColour(gv == 0.0f ? juce::Colour(0x35ffffff)
                               : juce::Colour(0x12ffffff));
        g.drawHorizontalLine(static_cast<int>(gy), graphRect.getX(),
                             graphRect.getRight());

        // In compact mode, label every 6 dB; in normal mode, label every 3 dB
        if (!isCompactH || static_cast<int>(std::abs(gv)) % 6 == 0) {
          g.setColour(juce::Colour(0xff94a3b8));
          g.drawText(juce::String(static_cast<int>(gv)),
                     static_cast<int>(graphRect.getX() + 4.0f),
                     static_cast<int>(gy - 6.0f), 22, 12,
                     juce::Justification::left);
        }
      }

      // Right dBFS scale (+12, 0, -12, -24, -36, -48 dBFS)
      static const float rtaDbfsVals[] = {12.0f,  0.0f,   -12.0f,
                                          -24.0f, -36.0f, -48.0f};
      for (float rdb : rtaDbfsVals) {
        if (!isCompactH || static_cast<int>(std::abs(rdb)) % 24 == 0) {
          const float norm = (rdb + 48.0f) / 60.0f;
          const float ry = graphRect.getBottom() - norm * graphRect.getHeight();
          g.setColour(juce::Colour(0xff475569));
          g.drawText(juce::String(static_cast<int>(rdb)),
                     static_cast<int>(graphRect.getRight() - 26.0f),
                     static_cast<int>(ry - 6.0f), 22, 12,
                     juce::Justification::right);
        }
      }

      // -----------------------------------------------------------------------
      // Real-Time Blue Spectrum Analyzer (RTA)
      // -----------------------------------------------------------------------
      if (rtaSourceMode != 3) { // not OFF
        juce::Path rtaPath, rtaFill;
        const float rBottom = graphRect.getBottom();
        rtaFill.startNewSubPath(graphRect.getX(), rBottom);
        bool firstRta = true;

        for (int i = 0; i < kRtaPoints; ++i) {
          const float t = i / static_cast<float>(kRtaPoints - 1);
          const float freq = 10.0f * std::pow(4000.0f, t);
          const float rx = freqToPlotX(freq, graphRect);

          const float valDb = rtaLevelsDb[static_cast<size_t>(i)];
          const float rtaNorm = std::clamp((valDb + 48.0f) / 60.0f, 0.0f, 1.0f);
          const float ry = rBottom - rtaNorm * graphRect.getHeight();

          if (firstRta) {
            rtaPath.startNewSubPath(rx, ry);
            rtaFill.lineTo(rx, ry);
            firstRta = false;
          } else {
            rtaPath.lineTo(rx, ry);
            rtaFill.lineTo(rx, ry);
          }
        }
        rtaFill.lineTo(graphRect.getRight(), rBottom);
        rtaFill.closeSubPath();

        // TDR NOVA Blue Translucent Spectrum Gradient Fill
        juce::ColourGradient rtaGrad(
            juce::Colour(0x240284c7), graphRect.getCentreX(), rBottom,
            juce::Colour(0x550284c7), graphRect.getCentreX(), graphRect.getY(),
            false);
        g.setGradientFill(rtaGrad);
        g.fillPath(rtaFill);
        g.setColour(juce::Colour(0xa038bdf8));
        g.strokePath(rtaPath,
                     juce::PathStrokeType(1.4f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
      }

      // -----------------------------------------------------------------------
      // Yellow Curve (Static Composite Parametric EQ) & Blue Curve (Dynamic EQ)
      // -----------------------------------------------------------------------
      auto &s = current();
      const bool eqOn = s.eqEnabled.load(std::memory_order_relaxed);
      juce::Path yellowPath, yellowFill, bluePath;
      const float zeroY = gainToPlotY(0.0f, graphRect);
      yellowFill.startNewSubPath(graphRect.getX(), zeroY);
      bool firstPt = true;

      const int curveSteps = 220;
      for (int i = 0; i < curveSteps; ++i) {
        const float t = i / static_cast<float>(curveSteps - 1);
        const float freq = 10.0f * std::pow(4000.0f, t);
        const float cx = freqToPlotX(freq, graphRect);

        const float staticDb = eqOn ? calculateEqResponseAt(freq, false) : 0.0f;
        const float dynamicDb = eqOn ? calculateEqResponseAt(freq, true) : 0.0f;

        const float cyStatic = gainToPlotY(staticDb, graphRect);
        const float cyDynamic = gainToPlotY(dynamicDb, graphRect);

        if (firstPt) {
          yellowPath.startNewSubPath(cx, cyStatic);
          yellowFill.lineTo(cx, cyStatic);
          bluePath.startNewSubPath(cx, cyDynamic);
          firstPt = false;
        } else {
          yellowPath.lineTo(cx, cyStatic);
          yellowFill.lineTo(cx, cyStatic);
          bluePath.lineTo(cx, cyDynamic);
        }
      }
      yellowFill.lineTo(graphRect.getRight(), zeroY);
      yellowFill.closeSubPath();

      // Draw Yellow Filled Curve (TDR Nova Yellow Curve)
      g.setColour(juce::Colour(0x32f59e0b)); // warm translucent amber fill
      g.fillPath(yellowFill);
      g.setColour(juce::Colour(0xfff59e0b)); // solid yellow curve stroke
      g.strokePath(yellowPath,
                   juce::PathStrokeType(2.4f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));

      // Draw Blue Dynamic Response Curve
      g.setColour(juce::Colour(0xff38bdf8)); // neon cyan dynamic response curve
      g.strokePath(bluePath,
                   juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));

      // -----------------------------------------------------------------------
      // Interactive Control Nodes (HP, I, II, III, IV, LP)
      // -----------------------------------------------------------------------
      // 1. HP Node
      const float hpF = s.highPassFreqHz.load(std::memory_order_relaxed);
      const float hpX = freqToPlotX(hpF, graphRect);
      const float hpY = gainToPlotY(0.0f, graphRect);
      drawNode(g, hpX, hpY, "HP", draggedNode == 100, juce::Colour(0xff0284c7));

      // 2. Bands I, II, III, IV Nodes
      static const char *romanNum[4] = {"I", "II", "III", "IV"};
      for (size_t b = 0; b < 4; ++b) {
        const float f = s.eqFrequency[b].load(std::memory_order_relaxed);
        const float gainVal = s.eqGainDb[b].load(std::memory_order_relaxed);
        const float nx = freqToPlotX(f, graphRect);
        const float ny = gainToPlotY(gainVal, graphRect);
        const bool isSel = (static_cast<int>(b) == selectedBand);
        drawNode(g, nx, ny, romanNum[b], isSel, juce::Colour(bandColours[b]));

        // Readout badge floating over selected band
        if (isSel) {
          const float qVal = s.eqQ[b].load(std::memory_order_relaxed);
          juce::String tip =
              "Band " + juce::String(romanNum[b]) + ": " +
              (f >= 1000.0f ? (juce::String(f * 0.001f, 2) + " kHz")
                            : (juce::String(f, 0) + " Hz")) +
              " | " + (gainVal >= 0.0f ? "+" : "") + juce::String(gainVal, 1) +
              " dB | Q " + juce::String(qVal, 2);
          if (s.dynEqEnabled[b].load(std::memory_order_relaxed)) {
            tip += juce::String(" | GR: ") + (liveGrDb[b] >= 0.0f ? "+" : "") +
                   juce::String(liveGrDb[b], 1) + " dB";
          }
          const int tipW = 220, tipH = 20;
          int tipX = static_cast<int>(nx) - tipW / 2;
          int tipY = static_cast<int>(ny) - 28;
          tipX = juce::jlimit(static_cast<int>(graphRect.getX()) + 4,
                              static_cast<int>(graphRect.getRight()) - tipW - 4,
                              tipX);
          if (tipY < static_cast<int>(graphRect.getY()) + 4)
            tipY = static_cast<int>(ny) + 14;

          g.setColour(juce::Colour(0xe00f172a));
          g.fillRoundedRectangle(
              static_cast<float>(tipX), static_cast<float>(tipY),
              static_cast<float>(tipW), static_cast<float>(tipH), 4.0f);
          g.setColour(juce::Colour(0xff38bdf8));
          g.drawRoundedRectangle(
              static_cast<float>(tipX), static_cast<float>(tipY),
              static_cast<float>(tipW), static_cast<float>(tipH), 4.0f, 1.0f);
          g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
          g.setColour(juce::Colours::white);
          g.drawText(tip, tipX, tipY, tipW, tipH, juce::Justification::centred);
        }
      }

      // 3. LP Node
      const float lpF = s.lowPassFreqHz.load(std::memory_order_relaxed);
      const float lpX = freqToPlotX(lpF, graphRect);
      const float lpY = gainToPlotY(0.0f, graphRect);
      drawNode(g, lpX, lpY, "LP", draggedNode == 200, juce::Colour(0xff0284c7));
    }

    // -------------------------------------------------------------------------
    // AREA 2: BAND SELECTOR BAR (Middle Separator & Logo)
    // -------------------------------------------------------------------------
    auto midBar = juce::Rectangle<float>(
        6.0f, barRect.getY(), area.getWidth() - 12.0f, barRect.getHeight());
    g.setColour(juce::Colour(0xff161e27));
    g.fillRoundedRectangle(midBar, 4.0f);
    g.setColour(juce::Colour(0xff24303e));
    g.drawRoundedRectangle(midBar, 4.0f, 1.0f);

    // NOVA Logo & Subtitle (scales gracefully for embedded touchscreen)
    const bool isCompactW = (area.getWidth() < 900.0f);
    g.setFont(juce::Font(
        juce::FontOptions(isCompactW ? 16.0f : 20.0f, juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText("NOVA", 14, static_cast<int>(barRect.getY() + 1.0f),
               isCompactW ? 55 : 75,
               static_cast<int>(barRect.getHeight() * 0.6f),
               juce::Justification::left);
    g.setFont(juce::Font(
        juce::FontOptions(isCompactW ? 7.0f : 8.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText(isCompactW ? "DYNAMIC EQ" : "PARALLEL DYNAMIC EQUALIZER", 14,
               static_cast<int>(barRect.getY() + barRect.getHeight() * 0.55f),
               isCompactW ? 85 : 150,
               static_cast<int>(barRect.getHeight() * 0.4f),
               juce::Justification::left);

    // -------------------------------------------------------------------------
    // AREA 3: PARAMETER CONTROL DECK PANELS
    // -------------------------------------------------------------------------
    auto botDeck = juce::Rectangle<float>(
        6.0f, paramRect.getY(), area.getWidth() - 12.0f, paramRect.getHeight());
    g.setColour(juce::Colour(0xff111720));
    g.fillRoundedRectangle(botDeck, 5.0f);
    g.setColour(juce::Colour(0xff1f2937));
    g.drawRoundedRectangle(botDeck, 5.0f, 1.0f);

    // Vertical Group Dividers in Deck (dynamically aligned to bays)
    if (bay1Right > 0.0f) {
      g.setColour(juce::Colour(0x25ffffff));
      g.drawVerticalLine(static_cast<int>(bay1Right), botDeck.getY() + 4.0f,
                         botDeck.getBottom() - 4.0f);
      g.drawVerticalLine(static_cast<int>(bay2Right), botDeck.getY() + 4.0f,
                         botDeck.getBottom() - 4.0f);
      g.drawVerticalLine(static_cast<int>(bay3Right), botDeck.getY() + 4.0f,
                         botDeck.getBottom() - 4.0f);
    }
  }

  void drawNode(juce::Graphics &g, float x, float y, const juce::String &text,
                bool isSelected, juce::Colour col) {
    const bool isCompact = (graphRect.getHeight() < 140.0f);
    const float radius =
        isSelected ? (isCompact ? 10.0f : 12.0f) : (isCompact ? 8.0f : 9.5f);

    // Glowing halo when selected
    if (isSelected) {
      g.setColour(col.withAlpha(0.35f));
      g.fillEllipse(x - radius - 3.5f, y - radius - 3.5f,
                    (radius + 3.5f) * 2.0f, (radius + 3.5f) * 2.0f);
    }

    g.setColour(col);
    g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(juce::Colours::white);
    g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f,
                  isSelected ? 2.0f : 1.2f);

    g.setFont(juce::Font(juce::FontOptions(
        isSelected ? (isCompact ? 9.5f : 10.5f) : (isCompact ? 8.0f : 9.0f),
        juce::Font::bold)));
    g.setColour(juce::Colours::white);
    g.drawText(text, static_cast<int>(x - radius), static_cast<int>(y - radius),
               static_cast<int>(radius * 2.0f), static_cast<int>(radius * 2.0f),
               juce::Justification::centred);
  }

  // ---------------------------------------------------------------------------
  // RESIZED
  // ---------------------------------------------------------------------------
  void resized() override {
    if (knobOutGain == nullptr || btnRtaMode[3] == nullptr)
      return;

    const auto area = getLocalBounds().toFloat();
    channelSelector.setVisible(false);

    const float pad = 4.0f;
    const float totalW = area.getWidth();
    const float totalH = area.getHeight();

    // Determine compact mode for touchscreen embedded view
    const bool isCompactH = (totalH < 340.0f);
    const bool isCompactW = (totalW < 900.0f);

    // Area 2: Band Selector Strip Height
    const float barH = isCompactH ? 26.0f : 32.0f;

    // Area 3: Parameter Deck Height (proportional allocation so RTA never
    // starves)
    const float paramH = isCompactH ? std::clamp(totalH * 0.38f, 75.0f, 95.0f)
                                    : std::clamp(totalH * 0.35f, 95.0f, 130.0f);

    // Area 1: Graph & RTA (takes the dominant height: at least 55% of available
    // space)
    const float graphH = std::max(75.0f, totalH - paramH - barH - pad * 4.0f);

    // Area 1: Graph Bounds
    graphRect = juce::Rectangle<float>(pad, pad, totalW - pad * 2.0f, graphH);

    // RTA Selector Buttons in top right of graph
    const float rtaW = isCompactW ? 28.0f : 32.0f;
    const float rtaH = isCompactH ? 16.0f : 18.0f;
    for (int i = 0; i < 4; ++i) {
      btnRtaMode[i]->setBounds(static_cast<int>(graphRect.getRight() -
                                                (4 - i) * (rtaW + 3.0f) - 6.0f),
                               static_cast<int>(graphRect.getY() + 4.0f),
                               static_cast<int>(rtaW), static_cast<int>(rtaH));
    }

    // Area 2: Band Selector Strip
    barRect = juce::Rectangle<float>(pad, graphRect.getBottom() + pad,
                                     totalW - pad * 2.0f, barH);
    const float logoW = isCompactW ? 95.0f : 145.0f;
    const float bColW = isCompactW ? 26.0f : 30.0f;
    const float bSpacing = 2.0f;
    const float bandGroupW = 5.0f * bColW + 4.0f * bSpacing;
    const float bStart = barRect.getX() + logoW + 6.0f;

    for (int b = 0; b < 4; ++b) {
      const float bx = bStart + b * (bColW + bSpacing);
      const float h1 = isCompactH ? 12.0f : 15.0f;
      const float h2 = isCompactH ? 10.0f : 12.0f;
      btnBandSelect[b]->setBounds(
          static_cast<int>(bx), static_cast<int>(barRect.getY() + 2.0f),
          static_cast<int>(bColW), static_cast<int>(h1));
      btnBandOn[b]->setBounds(
          static_cast<int>(bx),
          static_cast<int>(barRect.getY() + 2.0f + h1 + 1.0f),
          static_cast<int>(bColW), static_cast<int>(h2));
    }
    btnWBandSelect.setBounds(
        static_cast<int>(bStart + 4.0f * (bColW + bSpacing)),
        static_cast<int>(barRect.getY() + 2.0f), static_cast<int>(bColW),
        static_cast<int>(barH - 4.0f));

    const float soloX = bStart + bandGroupW + 6.0f;
    const float funcBtnW = isCompactW ? 44.0f : 52.0f;
    const float funcBtnH = barH - 4.0f;
    btnBandSolo.setBounds(
        static_cast<int>(soloX), static_cast<int>(barRect.getY() + 2.0f),
        static_cast<int>(funcBtnW), static_cast<int>(funcBtnH));
    btnOrderByFreq.setBounds(static_cast<int>(soloX + funcBtnW + 3.0f),
                             static_cast<int>(barRect.getY() + 2.0f),
                             static_cast<int>(funcBtnW),
                             static_cast<int>(funcBtnH));
    btnSplit.setBounds(static_cast<int>(soloX + (funcBtnW + 3.0f) * 2.0f),
                       static_cast<int>(barRect.getY() + 2.0f),
                       static_cast<int>(funcBtnW), static_cast<int>(funcBtnH));
    btnStereoMode.setBounds(
        static_cast<int>(soloX + (funcBtnW + 3.0f) * 3.0f),
        static_cast<int>(barRect.getY() + 2.0f),
        static_cast<int>(funcBtnW + 6.0f), static_cast<int>(funcBtnH));
    btnExtSc.setBounds(
        static_cast<int>(soloX + (funcBtnW + 3.0f) * 4.0f + 6.0f),
        static_cast<int>(barRect.getY() + 2.0f),
        static_cast<int>(funcBtnW + 2.0f), static_cast<int>(funcBtnH));

    const float rightBtnW = isCompactW ? 52.0f : 64.0f;
    const float elBtnW = isCompactW ? 34.0f : 40.0f;
    btnGrDelta.setBounds(
        static_cast<int>(barRect.getRight() - rightBtnW - 4.0f),
        static_cast<int>(barRect.getY() + 2.0f), static_cast<int>(rightBtnW),
        static_cast<int>(funcBtnH));
    btnBypass.setBounds(
        static_cast<int>(barRect.getRight() - rightBtnW * 2.0f - 8.0f),
        static_cast<int>(barRect.getY() + 2.0f), static_cast<int>(rightBtnW),
        static_cast<int>(funcBtnH));
    btnElBypass.setBounds(
        static_cast<int>(barRect.getRight() - rightBtnW * 2.0f - elBtnW - 12.0f),
        static_cast<int>(barRect.getY() + 2.0f), static_cast<int>(elBtnW),
        static_cast<int>(funcBtnH));

    // Area 3: Parameter Deck (Four Adaptive Bays)
    paramRect = juce::Rectangle<float>(pad, barRect.getBottom() + pad,
                                       totalW - pad * 2.0f, paramH);
    const float innerPad = 4.0f;
    const float deckW = paramRect.getWidth() - innerPad * 2.0f;
    const float deckH = paramRect.getHeight();

    // Bay width allocation
    const float bay1W = std::clamp(deckW * 0.14f, 96.0f, 130.0f);
    const float bay4W = std::clamp(deckW * 0.18f, 120.0f, 180.0f);
    const float remW = deckW - bay1W - bay4W;
    const float bay2W = remW * 0.38f;
    const float bay3W = remW - bay2W;

    const float bay1X = paramRect.getX() + innerPad;
    const float bay2X = bay1X + bay1W;
    const float bay3X = bay2X + bay2W;
    const float bay4X = bay3X + bay3W;

    bay1Right = bay2X;
    bay2Right = bay3X;
    bay3Right = bay4X;

    // --- Bay 1: Filter Controls (HP & LP) ---
    const float fltBtnW = (bay1W - 8.0f) * 0.5f;
    const float fltRowH = std::clamp((deckH - 12.0f) / 3.0f, 16.0f, 26.0f);
    btnHp.setBounds(static_cast<int>(bay1X + 2.0f),
                    static_cast<int>(paramRect.getY() + 3.0f),
                    static_cast<int>(fltBtnW), static_cast<int>(fltRowH));
    btnLp.setBounds(static_cast<int>(bay1X + 4.0f + fltBtnW),
                    static_cast<int>(paramRect.getY() + 3.0f),
                    static_cast<int>(fltBtnW), static_cast<int>(fltRowH));

    lblHpFreq.setBounds(static_cast<int>(bay1X + 2.0f),
                        static_cast<int>(paramRect.getY() + 5.0f + fltRowH),
                        static_cast<int>(fltBtnW), static_cast<int>(fltRowH));
    lblLpFreq.setBounds(static_cast<int>(bay1X + 4.0f + fltBtnW),
                        static_cast<int>(paramRect.getY() + 5.0f + fltRowH),
                        static_cast<int>(fltBtnW), static_cast<int>(fltRowH));

    btnHpSlope.setBounds(
        static_cast<int>(bay1X + 2.0f),
        static_cast<int>(paramRect.getY() + 7.0f + fltRowH * 2.0f),
        static_cast<int>(fltBtnW), static_cast<int>(fltRowH));
    btnLpSlope.setBounds(
        static_cast<int>(bay1X + 4.0f + fltBtnW),
        static_cast<int>(paramRect.getY() + 7.0f + fltRowH * 2.0f),
        static_cast<int>(fltBtnW), static_cast<int>(fltRowH));

    // --- Bay 2: Shapes & PEQ Knobs (Q, Freq, Gain) ---
    const float shapeBtnW = std::clamp((bay2W - 12.0f) / 3.0f, 18.0f, 26.0f);
    const float shapeBtnH = isCompactH ? 16.0f : 20.0f;
    for (int i = 0; i < 3; ++i) {
      btnShape[i]->setBounds(
          static_cast<int>(bay2X + 4.0f + i * (shapeBtnW + 2.0f)),
          static_cast<int>(paramRect.getY() + 3.0f),
          static_cast<int>(shapeBtnW), static_cast<int>(shapeBtnH));
    }
    const float peqKnobTop = paramRect.getY() + shapeBtnH + 3.0f;
    const float peqKnobH = deckH - shapeBtnH - 6.0f;
    const float peqColW = (bay2W - 8.0f) / 3.0f;
    knobQ->setBounds(static_cast<int>(bay2X + 2.0f),
                     static_cast<int>(peqKnobTop), static_cast<int>(peqColW),
                     static_cast<int>(peqKnobH));
    knobFreq->setBounds(static_cast<int>(bay2X + 2.0f + peqColW),
                        static_cast<int>(peqKnobTop), static_cast<int>(peqColW),
                        static_cast<int>(peqKnobH));
    knobGain->setBounds(static_cast<int>(bay2X + 2.0f + peqColW * 2.0f),
                        static_cast<int>(paramRect.getY() + 3.0f),
                        static_cast<int>(peqColW),
                        static_cast<int>(deckH - 6.0f));

    // --- Bay 3: Dynamic EQ Controls ---
    const float dColW = (bay3W - 10.0f) / 5.0f;
    knobThreshold->setBounds(static_cast<int>(bay3X + 2.0f),
                             static_cast<int>(paramRect.getY() + 3.0f),
                             static_cast<int>(dColW),
                             static_cast<int>(deckH - 6.0f));

    const float btnTopH = isCompactH ? 16.0f : 20.0f;
    btnWb.setBounds(static_cast<int>(bay3X + 2.0f + dColW),
                    static_cast<int>(paramRect.getY() + 3.0f),
                    static_cast<int>(dColW - 2.0f), static_cast<int>(btnTopH));
    knobRatio->setBounds(static_cast<int>(bay3X + 2.0f + dColW),
                         static_cast<int>(paramRect.getY() + btnTopH + 3.0f),
                         static_cast<int>(dColW),
                         static_cast<int>(deckH - btnTopH - 6.0f));

    btnModeAboveBelow.setBounds(static_cast<int>(bay3X + 2.0f + dColW * 2.0f),
                                static_cast<int>(paramRect.getY() + 3.0f),
                                static_cast<int>(dColW - 2.0f),
                                static_cast<int>(btnTopH));
    knobRange->setBounds(static_cast<int>(bay3X + 2.0f + dColW * 2.0f),
                         static_cast<int>(paramRect.getY() + btnTopH + 3.0f),
                         static_cast<int>(dColW),
                         static_cast<int>(deckH - btnTopH - 6.0f));

    knobKnee->setBounds(static_cast<int>(bay3X + 2.0f + dColW * 3.0f),
                        static_cast<int>(paramRect.getY() + 3.0f),
                        static_cast<int>(dColW),
                        static_cast<int>(deckH - 6.0f));

    const float attRelH = (deckH - 6.0f) * 0.48f;
    knobAttack->setBounds(static_cast<int>(bay3X + 2.0f + dColW * 4.0f),
                          static_cast<int>(paramRect.getY() + 2.0f),
                          static_cast<int>(dColW), static_cast<int>(attRelH));
    knobRelease->setBounds(static_cast<int>(bay3X + 2.0f + dColW * 4.0f),
                           static_cast<int>(paramRect.getY() + attRelH + 3.0f),
                           static_cast<int>(dColW), static_cast<int>(attRelH));

    // --- Bay 4: Output & Dry Mix Controls ---
    const float outColW = (bay4W - 8.0f) / 3.0f;
    knobEqGain->setBounds(static_cast<int>(bay4X + 2.0f),
                          static_cast<int>(paramRect.getY() + 3.0f),
                          static_cast<int>(outColW),
                          static_cast<int>(deckH - 6.0f));
    knobDryMix->setBounds(static_cast<int>(bay4X + 2.0f + outColW),
                          static_cast<int>(paramRect.getY() + 3.0f),
                          static_cast<int>(outColW),
                          static_cast<int>(deckH - 6.0f));
    knobOutGain->setBounds(static_cast<int>(bay4X + 2.0f + outColW * 2.0f),
                           static_cast<int>(paramRect.getY() + 3.0f),
                           static_cast<int>(outColW),
                           static_cast<int>(deckH - 6.0f));
  }

private:
  MixerEngine &engine;
  juce::ComboBox channelSelector;

  // Layout rectangles & bays
  juce::Rectangle<float> graphRect;
  juce::Rectangle<float> barRect;
  juce::Rectangle<float> paramRect;
  float bay1Right = 0.0f;
  float bay2Right = 0.0f;
  float bay3Right = 0.0f;

  // Selected state
  int selectedBand = 0;
  bool editingWBand = false;
  int draggedNode = -1;
  int rtaSourceMode = 1;                     // 0=IN, 1=OUT, 2=SC, 3=OFF
  std::array<int, 4> bandShapes{1, 1, 1, 1}; // 0=LowShelf, 1=Bell, 2=HighShelf
  std::array<float, 4> liveGrDb{0.0f, 0.0f, 0.0f, 0.0f};
  float liveWBandGrDb = 0.0f;

  // Live RTA Telemetry Array (160 log-spaced points)
  static constexpr int kRtaPoints = 160;
  std::array<float, kRtaPoints> rtaLevelsDb{};

  static constexpr uint32_t bandColours[4] = {0xfff59e0b, 0xff0ea5e9,
                                              0xff10b981, 0xff8b5cf6};

  // Area 1: RTA
  std::array<std::unique_ptr<juce::TextButton>, 4> btnRtaMode;

  // Area 2: Band Selector & Manager
  std::array<std::unique_ptr<juce::TextButton>, 4> btnBandSelect;
  std::array<std::unique_ptr<juce::TextButton>, 4> btnBandOn;
  juce::TextButton btnWBandSelect;
  juce::TextButton btnBandSolo, btnOrderByFreq, btnSplit, btnStereoMode,
      btnExtSc, btnElBypass, btnBypass, btnGrDelta;

  void updateStereoModeLabel(int mode) {
    static const char *labels[6] = {"STEREO", "SUM", "DIFF",
                                    "LEFT",   "RIGHT", "DUAL"};
    btnStereoMode.setButtonText(labels[std::clamp(mode, 0, 5)]);
  }

  // Area 3: Parameter Controls
  juce::TextButton btnHp, btnLp;
  FreqDraggableLabel lblHpFreq, lblLpFreq;
  juce::TextButton btnHpSlope, btnLpSlope;
  std::array<std::unique_ptr<juce::TextButton>, 3> btnShape;

  std::unique_ptr<NovaRotaryKnob> knobQ, knobFreq, knobGain;
  std::unique_ptr<NovaRotaryKnob> knobThreshold, knobRatio, knobRange, knobKnee,
      knobAttack, knobRelease;
  juce::TextButton btnWb, btnModeAboveBelow;

  std::unique_ptr<NovaRotaryKnob> knobEqGain, knobDryMix, knobOutGain;

  ChannelState &current() noexcept {
    const int idx = juce::jlimit(0, kLogicalChannels - 1,
                                 channelSelector.getSelectedId() - 1);
    return engine.channels[static_cast<size_t>(idx)];
  }

  const ChannelState &current() const noexcept {
    const int idx = juce::jlimit(0, kLogicalChannels - 1,
                                 channelSelector.getSelectedId() - 1);
    return engine.channels[static_cast<size_t>(idx)];
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEqPanel)
};

} // namespace nova
