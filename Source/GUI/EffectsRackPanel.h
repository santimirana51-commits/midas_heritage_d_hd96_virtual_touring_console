#pragma once

#include "Mixer/MixerEngine.h"

#include "GUI/VSS4Panel.h"

#include "GUI/NebulaDelayPanel.h"

#include "GUI/ManipulatorPitchPanel.h"

#include "GUI/MasterXPanel.h"

#include "GUI/EMT140Panel.h"

#include "GUI/AKGBX20Panel.h"

#include "GUI/DVR2Panel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

// ==============================================================================

// 1. Helper Type Formatter & Category Colours

// ==============================================================================

static inline juce::String getFxTypeName(nova::graviton::FxType t) {

  using nova::graviton::FxType;

  switch (t) {

  case FxType::Reverb_VSS4:
    return "VSS4 REVERB";

  case FxType::StereoDelay:
    return "NEBULA DELAY";

  case FxType::PitchShifter:
    return "PITCH & ADT";

  case FxType::Chorus:
    return "STEREO CHORUS";

  case FxType::Flanger:
    return "BL-20 FLANGER";

  case FxType::Phaser:
    return "STEREO PHASER";

  case FxType::Compressor:
    return "CH COMPRESSOR";

  case FxType::NoiseGate:
    return "NOISE GATE";

  case FxType::Distortion:
    return "TUBE SATURATION";

  case FxType::MasterX_Multiband:
    return "MASTER X HD";

  case FxType::EMT140_Plate:
    return "EMT 140 PLATE";

  case FxType::SpringReverb:

  case FxType::DualTank_Spring:
    return "AKG BX20 SPRING";

  case FxType::DVR2_NonLinear:
    return "DVR2 REVERB";

  case FxType::TC_SCF:
    return "TC SCF CHORUS";

  case FxType::TC_NonLin2:
    return "NONLIN2 GATED";

  case FxType::KT_DN780:
    return "KT DN780 PLATE";

  case FxType::ASIO_Bridge:
    return "ASIO BRIDGE";

  case FxType::SubHarmonic_Synth:
    return "SUB-SYNTH";

  case FxType::DN360_InductorGEQ:
    return "DN360 30B GEQ";

  case FxType::Precision_DeEsser:
    return "DE-ESSER";

  case FxType::Convolution_Reverb:
    return "CONVOLUTION";

  case FxType::Hybrid_Reverb:
    return "HYBRID REVERB";

  case FxType::TC_2290_Delay:
    return "TC 2290 DELAY";

  case FxType::KT_1176_Limiter:
    return "1176 LN FET";

  case FxType::KT_BusCompressor:
    return "G-BUS COMP";

  case FxType::Reverb_VSS3:
    return "VSS3 REVERB";

  case FxType::Vocal_Pitch_Corrector:
    return "VOCAL PITCH EVO";

  case FxType::None:

  default:
    return "(EMPTY)";
  }
}

static inline juce::Colour getFxTypeColour(nova::graviton::FxType t) {

  using nova::graviton::FxType;

  switch (t) {

  case FxType::Reverb_VSS4:

  case FxType::DVR2_NonLinear:

  case FxType::TC_NonLin2:

  case FxType::Convolution_Reverb:

  case FxType::Hybrid_Reverb:

  case FxType::Reverb_VSS3:

    return juce::Colour(0xff38bdf8); // Cyan

  case FxType::EMT140_Plate:

  case FxType::KT_DN780:

  case FxType::SpringReverb:

  case FxType::DualTank_Spring:

    return juce::Colour(0xff34d399); // Mint green

  case FxType::StereoDelay:

  case FxType::TC_2290_Delay:

    return juce::Colour(0xfffacc15); // Gold

  case FxType::MasterX_Multiband:

    return juce::Colour(0xffc084fc); // Purple

  case FxType::KT_1176_Limiter:

  case FxType::KT_BusCompressor:

  case FxType::Compressor:

  case FxType::NoiseGate:

    return juce::Colour(0xfff87171); // Red / Coral

  case FxType::PitchShifter:

  case FxType::Chorus:

  case FxType::Flanger:

  case FxType::Phaser:

  case FxType::TC_SCF:

    return juce::Colour(0xff818cf8); // Indigo / Violet

  case FxType::SubHarmonic_Synth:

  case FxType::DN360_InductorGEQ:

  case FxType::Distortion:

  case FxType::Precision_DeEsser:

    return juce::Colour(0xfffb923c); // Amber / Orange

  default:

    return juce::Colour(0xff64748b);
  }
}

enum class FxCategory {
  All = 0,
  Reverb,
  Delay,
  ModPitch,
  Dynamics,
  ToneMaster,
  Utility
};

static inline FxCategory getFxCategory(nova::graviton::FxType t) {
  using nova::graviton::FxType;
  switch (t) {
  case FxType::Reverb_VSS4:
  case FxType::DVR2_NonLinear:
  case FxType::TC_NonLin2:
  case FxType::Convolution_Reverb:
  case FxType::Hybrid_Reverb:
  case FxType::Reverb_VSS3:
  case FxType::EMT140_Plate:
  case FxType::KT_DN780:
  case FxType::SpringReverb:
  case FxType::DualTank_Spring:
    return FxCategory::Reverb;
  case FxType::StereoDelay:
  case FxType::TC_2290_Delay:
    return FxCategory::Delay;
  case FxType::PitchShifter:
  case FxType::Vocal_Pitch_Corrector:
  case FxType::Chorus:
  case FxType::Flanger:
  case FxType::Phaser:
  case FxType::TC_SCF:
    return FxCategory::ModPitch;
  case FxType::KT_1176_Limiter:
  case FxType::KT_BusCompressor:
  case FxType::Compressor:
  case FxType::NoiseGate:
  case FxType::Precision_DeEsser:
    return FxCategory::Dynamics;
  case FxType::MasterX_Multiband:
  case FxType::SubHarmonic_Synth:
  case FxType::DN360_InductorGEQ:
  case FxType::Distortion:
    return FxCategory::ToneMaster;
  default:
    return FxCategory::Utility;
  }
}

static inline juce::String getFxCategoryTag(nova::graviton::FxType t) {
  using nova::graviton::FxType;
  switch (t) {
  case FxType::Reverb_VSS4:
  case FxType::DVR2_NonLinear:
  case FxType::TC_NonLin2:
  case FxType::Convolution_Reverb:
  case FxType::Hybrid_Reverb:
  case FxType::Reverb_VSS3:
  case FxType::EMT140_Plate:
  case FxType::KT_DN780:
  case FxType::SpringReverb:
  case FxType::DualTank_Spring:
    return "REVERB";
  case FxType::StereoDelay:
  case FxType::TC_2290_Delay:
    return "DELAY";
  case FxType::PitchShifter:
  case FxType::Vocal_Pitch_Corrector:
    return "PITCH";
  case FxType::Chorus:
  case FxType::Flanger:
  case FxType::Phaser:
  case FxType::TC_SCF:
    return "MOD";
  case FxType::KT_1176_Limiter:
  case FxType::KT_BusCompressor:
  case FxType::Compressor:
  case FxType::NoiseGate:
  case FxType::Precision_DeEsser:
    return "DYN";
  case FxType::MasterX_Multiband:
    return "MASTER";
  case FxType::DN360_InductorGEQ:
    return "EQ";
  case FxType::SubHarmonic_Synth:
  case FxType::Distortion:
    return "TONE";
  case FxType::ASIO_Bridge:
    return "I/O";
  default:
    return "EMPTY";
  }
}

static inline juce::String getFxPurposeText(nova::graviton::FxType t) {
  using nova::graviton::FxType;
  switch (t) {
  case FxType::Reverb_VSS4:
    return "Reverb Hall & Vokal";
  case FxType::StereoDelay:
    return "Stereo / Ping-Pong Delay";
  case FxType::EMT140_Plate:
    return "Plate Reverb Vokal";
  case FxType::SpringReverb:
  case FxType::DualTank_Spring:
    return "Spring Tank Gitar/Vintage";
  case FxType::MasterX_Multiband:
    return "Mastering Multiband Limiter";
  case FxType::PitchShifter:
    return "Pitch Shifter & Stereo ADT";
  case FxType::KT_1176_Limiter:
    return "FET Comp Vokal / Snare";
  case FxType::KT_BusCompressor:
    return "Glue Bus VCA Compressor";
  case FxType::Precision_DeEsser:
    return "Redam Desis Vokal (Sibilance)";
  case FxType::TC_SCF:
    return "Chorus / Flanger Analog";
  case FxType::Flanger:
    return "Jet Sweep BBD Flanger";
  case FxType::Phaser:
    return "12-Stage Optical Phaser";
  case FxType::TC_2290_Delay:
    return "Dynamic Ducking Delay";
  case FxType::DVR2_NonLinear:
    return "Vintage Digital Reverb";
  case FxType::TC_NonLin2:
    return "Gated Reverb Drum / Snare";
  case FxType::KT_DN780:
    return "Classic Dual-Engine Reverb";
  case FxType::SubHarmonic_Synth:
    return "Sub-Bass Octave Enhancer";
  case FxType::DN360_InductorGEQ:
    return "30-Band Graphic EQ";
  case FxType::Distortion:
    return "Warm Tube & Tape Saturator";
  case FxType::Vocal_Pitch_Corrector:
    return "Auto Vocal Pitch Correction";
  case FxType::Convolution_Reverb:
    return "True IR Acoustic Spaces";
  case FxType::Hybrid_Reverb:
    return "IR + Algorithmic Hybrid";
  case FxType::Reverb_VSS3:
    return "TC Stereo Hall / Room";
  case FxType::Compressor:
    return "7-Circuit Comp / Dynamics";
  case FxType::NoiseGate:
    return "Noise Gate & Expander";
  case FxType::Chorus:
    return "Stereo Ensemble Chorus";
  case FxType::ASIO_Bridge:
    return "External ASIO Hardware Insert";
  case FxType::None:
  default:
    return "Slot Kosong (Klik Pilih FX)";
  }
}

// ==============================================================================

// 2. Interactive FX Slot Faceplate Tile

// ==============================================================================

class FxSlotTile final : public juce::Component {

public:
  int slotIndex = 0;

  bool isSelected = false;

  bool isBypassed = false;

  juce::String name{"(EMPTY)"};

  juce::String categoryTag{"EMPTY"};

  juce::String purposeText{"Slot Kosong"};

  FxCategory category = FxCategory::Utility;

  bool isDimmed = false;

  juce::Colour categoryColour{0xff38bdf8};

  juce::String routingBadge{"OFF"};

  bool isChannelInsert = false;

  float inPeak = 0.0f;

  float outPeak = 0.0f;

  std::function<void(int)> onClick;

  std::function<void(int)> onDoubleClick;

  void mouseUp(const juce::MouseEvent &e) override {
    if (e.mouseWasDraggedSinceMouseDown())
      return;

    if (e.mods.isLeftButtonDown() || !e.mods.isPopupMenu()) {

      if (onClick)
        onClick(slotIndex);
    }
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {

    if (onDoubleClick)
      onDoubleClick(slotIndex);
  }

  void paint(juce::Graphics &g) override {

    auto b = getLocalBounds().toFloat();

    const float h = b.getHeight();

    // Background tile card

    if (isSelected) {

      g.setColour(juce::Colour(0xff182a42));

      g.fillRoundedRectangle(b, 10.0f);

      g.setColour(juce::Colour(0xffeab308)); // Bright gold selected border

      g.drawRoundedRectangle(b.reduced(0.5f), 10.0f, 1.8f);

    } else {

      g.setColour(juce::Colour(0xff121b2a));

      g.fillRoundedRectangle(b, 10.0f);

      g.setColour(isChannelInsert ? juce::Colour(0xff10b981)
                                  : juce::Colour(0xff233146));

      g.drawRoundedRectangle(b.reduced(0.5f), 10.0f,
                             isChannelInsert ? 1.5f : 1.0f);
    }

    // Left Category Accent Stripe
    g.setColour((isBypassed ? juce::Colour(0xff475569) : categoryColour)
                    .withAlpha(isSelected ? 0.95f : 0.80f));
    g.fillRoundedRectangle(b.getX() + 2.0f, b.getY() + 6.0f, 3.5f,
                           b.getHeight() - 12.0f, 1.75f);

    // Dynamic vertical sections to guarantee zero text collision even if height
    // is small

    const float topH = juce::jlimit(12.0f, 15.0f, h * 0.25f);

    const float botH = juce::jlimit(12.0f, 15.0f, h * 0.25f);

    // Top Header: Slot Tag, Category Pill & Bypass Dot

    auto top = b.reduced(6.0f, 2.0f).removeFromTop(topH);
    top.removeFromLeft(3.0f);

    g.setFont(juce::Font(juce::FontOptions(
        juce::jlimit(8.5f, 10.0f, topH * 0.72f), juce::Font::bold)));

    g.setColour(isSelected ? juce::Colour(0xffeab308)
                           : juce::Colour(0xff94a3b8));

    g.drawText("S" + juce::String(slotIndex + 1 < 10 ? "0" : "") +
                   juce::String(slotIndex + 1),

               top.removeFromLeft(28.0f), juce::Justification::centredLeft);

    // Bypass LED Dot

    auto ledRect = top.removeFromRight(10.0f).withSizeKeepingCentre(6.0f, 6.0f);

    g.setColour(isBypassed ? juce::Colour(0xffef4444)
                           : juce::Colour(0xff10b981));

    g.fillEllipse(ledRect);

    // Category Group Pill in Top Row
    if (categoryTag.isNotEmpty() && top.getWidth() > 36.0f) {
      auto catPill = top.removeFromRight(juce::jmin(56.0f, top.getWidth() - 2.0f))
                         .reduced(1.0f, 1.0f);
      g.setColour(categoryColour.withAlpha(0.18f));
      g.fillRoundedRectangle(catPill, 2.5f);
      g.setColour(categoryColour.withAlpha(0.75f));
      g.drawRoundedRectangle(catPill, 2.5f, 0.8f);
      g.setFont(juce::Font(juce::FontOptions(
          juce::jlimit(7.0f, 8.5f, topH * 0.62f), juce::Font::bold)));
      g.drawText(categoryTag, catPill, juce::Justification::centred, false);
    }

    // Bottom Area: Routing Badge & Live Stereo Peak Meter

    auto bot = b.reduced(5.0f, 2.0f).removeFromBottom(botH);

    // Stereo Peak Meters (In & Out vertical level bars)

    auto meterArea = bot.removeFromRight(22.0f);

    auto inBar = meterArea.removeFromLeft(10.0f).reduced(1.5f, 0.0f);

    auto outBar = meterArea.reduced(1.5f, 0.0f);

    auto drawMeter = [&](juce::Rectangle<float> rect, float peak) {
      g.setColour(juce::Colour(0xff090d14));

      g.fillRect(rect);

      const float normalized = juce::jlimit(
          0.0f, 1.0f,
          (juce::Decibels::gainToDecibels(std::max(1e-4f, peak), -60.0f) +
           60.0f) /
              60.0f);

      if (normalized > 0.02f) {

        auto fill = rect;

        fill = fill.removeFromBottom(fill.getHeight() * normalized);

        juce::Colour fillCol = (normalized > 0.90f) ? juce::Colour(0xffef4444)

                               : (normalized > 0.70f)
                                   ? juce::Colour(0xffeab308)

                                   : juce::Colour(0xff10b981);

        g.setColour(fillCol);

        g.fillRect(fill);
      }
    };

    drawMeter(inBar, inPeak);

    drawMeter(outBar, outPeak);

    // Routing Badge Pill

    auto badgeArea = bot.reduced(1.0f, 0.0f);

    juce::Colour badgeCol =
        isChannelInsert ? juce::Colour(0xff10b981)

        : routingBadge.startsWith("INS: M") ? juce::Colour(0xffa855f7)

        : routingBadge.startsWith("AUX") ? juce::Colour(0xff0ea5e9)

                                         : juce::Colour(0xff334155);

    g.setColour(badgeCol.withAlpha(0.20f));

    g.fillRoundedRectangle(badgeArea, 2.0f);

    g.setColour(badgeCol);

    g.drawRoundedRectangle(badgeArea, 2.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions(
        juce::jlimit(7.5f, 8.5f, botH * 0.65f), juce::Font::bold)));

    g.drawText(routingBadge, badgeArea, juce::Justification::centred);

    // Center FX Name & Purpose (middle remainder)

    auto mid = b.reduced(6.0f, 1.0f);
    mid.removeFromLeft(3.0f);

    mid.removeFromTop(topH);

    mid.removeFromBottom(botH);

    if (mid.getHeight() >= 22.0f && purposeText.isNotEmpty()) {
      auto nameRect = mid.removeFromTop(mid.getHeight() * 0.54f);
      const float fontH = juce::jlimit(9.5f, 12.5f, nameRect.getHeight() * 0.78f);
      g.setFont(juce::Font(juce::FontOptions(fontH, juce::Font::bold)));
      g.setColour(isBypassed ? juce::Colour(0xff64748b) : categoryColour);
      g.drawText(name, nameRect, juce::Justification::centredLeft, true);

      const float subH = juce::jlimit(8.0f, 10.0f, mid.getHeight() * 0.76f);
      g.setFont(juce::Font(juce::FontOptions(subH, juce::Font::plain)));
      g.setColour(isBypassed ? juce::Colour(0xff475569)
                             : juce::Colour(0xffcbd5e1));
      g.drawText(purposeText, mid, juce::Justification::centredLeft, true);
    } else {
      const float fontH = juce::jlimit(9.0f, 12.0f, mid.getHeight() * 0.68f);
      g.setFont(juce::Font(juce::FontOptions(fontH, juce::Font::bold)));
      g.setColour(isBypassed ? juce::Colour(0xff64748b) : categoryColour);
      g.drawText(name, mid, juce::Justification::centredLeft, true);
    }

    if (isDimmed && !isSelected) {
      g.setColour(juce::Colour(0xff090d14).withAlpha(0.64f));
      g.fillRoundedRectangle(b, 10.0f);
    }
  }
};

// ==============================================================================

// 3. EffectsRackPanel (24-Slot Virtual Studio Rack)

// ==============================================================================

class EffectsRackPanel final : public juce::Component, private juce::Timer {

public:
  explicit EffectsRackPanel(MixerEngine &e) : engine(e) {

    title.setText("GRAVITON VIRTUAL FX RACK (24 SLOTS) - STUDIO RACK BAY",
                  juce::dontSendNotification);

    title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

    title.setColour(juce::Label::textColourId, juce::Colour(0xfff0f3f6));

    addAndMakeVisible(title);

    lblSelectedChannel.setFont(
        juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

    lblSelectedChannel.setColour(juce::Label::textColourId,
                                 juce::Colour(0xff10b981));

    lblSelectedChannel.setText("SEL CH: CH 1", juce::dontSendNotification);

    addAndMakeVisible(lblSelectedChannel);

    static const struct {
      const char *label;
      FxCategory cat;
      juce::uint32 col;
    } kFilterDefs[6] = {
        {"ALL (24)", FxCategory::All, 0xff94a3b8},
        {"REVERB", FxCategory::Reverb, 0xff38bdf8},
        {"DELAY", FxCategory::Delay, 0xfffacc15},
        {"DYNAMICS", FxCategory::Dynamics, 0xfff87171},
        {"MOD/PITCH", FxCategory::ModPitch, 0xff818cf8},
        {"TONE/MASTER", FxCategory::ToneMaster, 0xfffb923c},
    };
    for (int f = 0; f < 6; ++f) {
      auto &btn = categoryFilterButtons[static_cast<size_t>(f)];
      btn.setButtonText(kFilterDefs[f].label);
      btn.setClickingTogglesState(false);
      btn.onClick = [this, cat = kFilterDefs[f].cat] { setCategoryFilter(cat); };
      addAndMakeVisible(btn);
    }
    setCategoryFilter(FxCategory::All);

    selectedEqBand.fill(16);

    // Pre-populate all 24 slots with curated studio processors

    ensureDefaultSlots();

    gridViewport.setViewedComponent(&gridContent, false);
    gridViewport.setScrollBarsShown(true, true);
    gridViewport.setScrollBarThickness(6);
    gridViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(gridViewport);

    // Instantiate 24 visual slot tiles

    for (int i = 0; i < 24; ++i) {

      slotTiles[static_cast<size_t>(i)] = std::make_unique<FxSlotTile>();

      auto &tile = *slotTiles[static_cast<size_t>(i)];

      tile.slotIndex = i;

      tile.onClick = [this, i](int) { setSelectedSlot(i); };

      tile.onDoubleClick = [this, i](int) {
        if (onSlotDoubleClicked)

          onSlotDoubleClicked(i);

        else

          openDetailView(i);
      };

      gridContent.addAndMakeVisible(tile);
    }

    slotTitle.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));

    slotTitle.setColour(juce::Label::textColourId, juce::Colour(0xffd29922));

    addAndMakeVisible(slotTitle);

    typeCombo.addItem("(Empty / Off)", 1);

    typeCombo.addItem("TC Electronic VSS4 Algorithmic Reverb", 2);

    typeCombo.addItem("Stereo Tape & Digital Delay", 3);

    typeCombo.addItem("Studio Pitch Shifter & Voice Doubler (ADT)", 4);

    typeCombo.addItem("Stereo Chorus", 5);

    typeCombo.addItem("NOVA HD Flanger BL-20 (Stereo BBD & Function Generator)",
                      6);

    typeCombo.addItem("NOVA HD Optical Stereo Phaser (12-Stage Benidub)", 7);

    typeCombo.addItem("Compressor (Insert)", 8);

    typeCombo.addItem("Noise Gate (Insert)", 9);

    typeCombo.addItem("Distortion / Saturation", 10);

    typeCombo.addItem("TC Electronic Master X HD (System 6000)", 11);

    typeCombo.addItem("EMT 140 Vintage Plate Reverb", 12);

    typeCombo.addItem("AKG BX20 Vintage Spring Reverb (Dual-Tank)", 13);

    typeCombo.addItem("TC Electronic System 6000 DVR2 Non-Linear Reverb", 14);

    typeCombo.addItem("TC Electronic Stereo Chorus / Flanger (SCF)", 15);

    typeCombo.addItem("TC Electronic NonLin2 Gated / Reverse Reverb", 16);

    typeCombo.addItem("Klark Teknik DN780 Digital Plate Reverb", 17);

    typeCombo.addItem("Sub-Harmonic Synthesizer (Low Octave Engine)", 20);

    typeCombo.addItem("Klark Teknik DN360 30-Band Inductor GEQ", 21);

    typeCombo.addItem("Precision De-Esser (Split / Broadband)", 22);

    typeCombo.addItem("TC Electronic 2290 Dynamic Delay", 24);

    typeCombo.addItem("Klark Teknik 1176 LN FET Limiting Amplifier", 25);

    typeCombo.addItem("Klark Teknik Bus Compressor (G-Bus Glue)", 26);

    typeCombo.addItem("TC Electronic VSS3 Stereo Reverb", 27);

    typeCombo.addItem("Studio Convolution Reverb (True IR)", 28);

    typeCombo.addItem("Studio Hybrid Reverb (IR + Algorithmic)", 29);

    typeCombo.addItem("Vocal Pitch Corrector (Auto-Tune Evo Style)", 30);

    typeCombo.onChange = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      const int selId = typeCombo.getSelectedId();

      nova::graviton::FxType t = nova::graviton::FxType::None;

      if (selId == 2)
        t = nova::graviton::FxType::Reverb_VSS4;

      else if (selId == 3)
        t = nova::graviton::FxType::StereoDelay;

      else if (selId == 4)
        t = nova::graviton::FxType::PitchShifter;

      else if (selId == 5)
        t = nova::graviton::FxType::Chorus;

      else if (selId == 6)
        t = nova::graviton::FxType::Flanger;

      else if (selId == 7)
        t = nova::graviton::FxType::Phaser;

      else if (selId == 8)
        t = nova::graviton::FxType::Compressor;

      else if (selId == 9)
        t = nova::graviton::FxType::NoiseGate;

      else if (selId == 10)
        t = nova::graviton::FxType::Distortion;

      else if (selId == 11)
        t = nova::graviton::FxType::MasterX_Multiband;

      else if (selId == 12) {
        t = nova::graviton::FxType::EMT140_Plate;
        slot.plate.setMode(0);
      }

      else if (selId == 13 || selId == 18)
        t = nova::graviton::FxType::SpringReverb;

      else if (selId == 14)
        t = nova::graviton::FxType::DVR2_NonLinear;

      else if (selId == 15)
        t = nova::graviton::FxType::TC_SCF;

      else if (selId == 16)
        t = nova::graviton::FxType::TC_NonLin2;

      else if (selId == 17) {
        t = nova::graviton::FxType::KT_DN780;
        slot.plate.setMode(1);
      }

      else if (selId == 20)
        t = nova::graviton::FxType::SubHarmonic_Synth;

      else if (selId == 21)
        t = nova::graviton::FxType::DN360_InductorGEQ;

      else if (selId == 22)
        t = nova::graviton::FxType::Precision_DeEsser;

      else if (selId == 24)
        t = nova::graviton::FxType::TC_2290_Delay;

      else if (selId == 25)
        t = nova::graviton::FxType::KT_1176_Limiter;

      else if (selId == 26)
        t = nova::graviton::FxType::KT_BusCompressor;

      else if (selId == 27)
        t = nova::graviton::FxType::Reverb_VSS3;

      else if (selId == 28)
        t = nova::graviton::FxType::Convolution_Reverb;

      else if (selId == 29)
        t = nova::graviton::FxType::Hybrid_Reverb;

      else if (selId == 30)
        t = nova::graviton::FxType::Vocal_Pitch_Corrector;

      slot.type.store(static_cast<int>(t), std::memory_order_relaxed);

      updateControls();
    };

    addAndMakeVisible(typeCombo);

    bypassButton.setButtonText("BYPASS");

    bypassButton.setClickingTogglesState(true);

    bypassButton.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour(0xffd73a49));

    bypassButton.onClick = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      slot.bypassed.store(bypassButton.getToggleState(),
                          std::memory_order_relaxed);

      repaint();
    };

    addAndMakeVisible(bypassButton);

    // Direct Channel Insert buttons ("Tampa Ada pilihan chanel lagi")

    btnInsertChannel.setButtonText("INSERT TO CH 1");

    btnInsertChannel.setColour(juce::TextButton::buttonColourId,
                               juce::Colour(0xff1e293b));

    btnInsertChannel.onClick = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      const int curIns = slot.insertChannel.load(std::memory_order_relaxed);

      if (curIns == selectedChannel) {

        slot.insertChannel.store(-1, std::memory_order_relaxed);

      } else {

        slot.insertChannel.store(selectedChannel, std::memory_order_relaxed);
      }

      updateControls();

      repaint();
    };

    addAndMakeVisible(btnInsertChannel);

    btnInsertMaster.setButtonText("INS: MASTER LR");

    btnInsertMaster.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff1e293b));

    btnInsertMaster.onClick = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      const int curIns = slot.insertChannel.load(std::memory_order_relaxed);

      if (curIns == nova::graviton::EffectsRack::kMasterInsertChannel) {

        slot.insertChannel.store(-1, std::memory_order_relaxed);

      } else {

        slot.insertChannel.store(
            nova::graviton::EffectsRack::kMasterInsertChannel,
            std::memory_order_relaxed);
      }

      updateControls();

      repaint();
    };

    addAndMakeVisible(btnInsertMaster);

    btnInsertNone.setButtonText("CLEAR INS");

    btnInsertNone.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff1e293b));

    btnInsertNone.onClick = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      slot.insertChannel.store(-1, std::memory_order_relaxed);

      updateControls();

      repaint();
    };

    addAndMakeVisible(btnInsertNone);

    btnEditFullScreen.setButtonText("EDIT FULLSCREEN (2x CLICK)");

    btnEditFullScreen.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff0284c7));

    btnEditFullScreen.onClick = [this] {
      if (onSlotDoubleClicked)

        onSlotDoubleClicked(selectedSlot);

      else

        openDetailView(selectedSlot);
    };

    addAndMakeVisible(btnEditFullScreen);

    btnBackFromDetail.setButtonText("<- BACK TO 24-SLOT RACK");

    btnBackFromDetail.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff1f6feb));

    btnBackFromDetail.onClick = [this] { closeDetailView(); };

    addChildComponent(btnBackFromDetail);

    // Aux Send combo

    auxSendCombo.addItem("Aux Send: (None)", 1);

    for (int a = 1; a <= kVisibleAuxSends; ++a)

      auxSendCombo.addItem("Aux Send: AUX " + juce::String(a), a + 1);

    auxSendCombo.onChange = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      const int a = auxSendCombo.getSelectedId() - 2;

      slot.auxSendSource.store(a, std::memory_order_relaxed);

      repaint();
    };

    addAndMakeVisible(auxSendCombo);

    returnLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);

    returnLevelSlider.setRange(-60.0, 10.0, 0.1);

    returnLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65,
                                      20);

    returnLevelSlider.setTextValueSuffix(" dB RET");

    returnLevelSlider.onValueChange = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      slot.returnLevelDb.store(static_cast<float>(returnLevelSlider.getValue()),
                               std::memory_order_relaxed);
    };

    addAndMakeVisible(returnLevelSlider);

    returnMuteButton.setButtonText("RET MUTE");

    returnMuteButton.setClickingTogglesState(true);

    returnMuteButton.setColour(juce::TextButton::buttonOnColourId,
                               juce::Colour(0xffe53935));

    returnMuteButton.onClick = [this] {
      auto &slot =
          engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

      slot.returnMute.store(returnMuteButton.getToggleState(),
                            std::memory_order_relaxed);
    };

    addAndMakeVisible(returnMuteButton);

    for (int i = 0; i < 5; ++i) {

      paramSliders[static_cast<size_t>(i)].setSliderStyle(
          juce::Slider::LinearHorizontal);

      paramSliders[static_cast<size_t>(i)].setTextBoxStyle(
          juce::Slider::TextBoxRight, false, 75, 20);

      addAndMakeVisible(paramSliders[static_cast<size_t>(i)]);

      paramLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId,
                                                    juce::Colour(0xff8b949e));

      addAndMakeVisible(paramLabels[static_cast<size_t>(i)]);
    }

    toggleParam.setClickingTogglesState(true);

    addAndMakeVisible(toggleParam);

    openEditorButton.setClickingTogglesState(false);

    addAndMakeVisible(openEditorButton);

    updateControls();

    startTimerHz(15);
  }

  std::function<void(int)> onSlotDoubleClicked;

  void ensureDefaultSlots() {

    using nova::graviton::FxType;

    struct SlotDef {

      size_t idx;

      FxType type;

      int auxSrc;

      int insertCh;
    };

    static const SlotDef kFactorySlots[24] = {

        {0, FxType::Reverb_VSS4, 0, -1}, // Slot 1: VSS4 Reverb (Aux 1)

        {1, FxType::StereoDelay, 1, -1}, // Slot 2: Nebula Stereo Delay (Aux 2)

        {2, FxType::EMT140_Plate, 2,
         -1}, // Slot 3: EMT 140 Plate Reverb (Aux 3)

        {3, FxType::SpringReverb, 3,
         -1}, // Slot 4: AKG BX20 Spring Reverb (Aux 4)

        {4, FxType::MasterX_Multiband, -1,
         1000}, // Slot 5: TC Master X HD (Master LR Insert)

        {5, FxType::PitchShifter, 4,
         -1}, // Slot 6: Manipulator Pitch ADT (Aux 5)

        {6, FxType::KT_1176_Limiter, -1,
         -1}, // Slot 7: Klark Teknik 1176 LN FET Limiter

        {7, FxType::KT_BusCompressor, -1,
         -1}, // Slot 8: Klark Teknik Bus Compressor

        {8, FxType::Precision_DeEsser, -1,
         -1}, // Slot 9: Precision Dynamic De-Esser

        {9, FxType::TC_SCF, 5,
         -1}, // Slot 10: TC Electronic SCF Chorus/Flanger (Aux 6)

        {10, FxType::Flanger, -1, -1}, // Slot 11: Studio Jet Flanger

        {11, FxType::Phaser, -1, -1}, // Slot 12: Optical Stereo Phaser

        {12, FxType::TC_2290_Delay, 6,
         -1}, // Slot 13: TC Electronic 2290 Dynamic Delay (Aux 7)

        {13, FxType::DVR2_NonLinear, 7,
         -1}, // Slot 14: TC DVR2 Digital Reverb (Aux 8)

        {14, FxType::TC_NonLin2, -1, -1}, // Slot 15: TC NonLin2 Gated Reverb

        {15, FxType::KT_DN780, -1, -1}, // Slot 16: Klark Teknik DN780 Plate

        {16, FxType::SubHarmonic_Synth, -1,
         -1}, // Slot 17: Sub-Harmonic Low Octave Synth

        {17, FxType::DN360_InductorGEQ, -1,
         -1}, // Slot 18: Klark Teknik DN360 30-Band GEQ

        {18, FxType::Distortion, -1,
         -1}, // Slot 19: Warm Tube & Tape Saturation

        {19, FxType::Vocal_Pitch_Corrector, -1,
         -1}, // Slot 20: Vocal Pitch Corrector (Evo)

        {20, FxType::Convolution_Reverb, -1,
         -1}, // Slot 21: True IR Convolution Reverb

        {21, FxType::Hybrid_Reverb, -1,
         -1}, // Slot 22: Studio Hybrid IR + Algorithmic Reverb

        {22, FxType::Reverb_VSS3, -1,
         -1}, // Slot 23: TC Electronic VSS3 Stereo Reverb

        {23, FxType::Compressor, -1, -1} // Slot 24: Precision Channel Dynamics

    };

    for (const auto &def : kFactorySlots) {

      auto &sl = engine.effectsRack.getSlot(def.idx);

      if (sl.type.load(std::memory_order_relaxed) ==
          static_cast<int>(FxType::None)) {

        sl.type.store(static_cast<int>(def.type), std::memory_order_relaxed);

        sl.auxSendSource.store(def.auxSrc, std::memory_order_relaxed);

        if (def.insertCh >= 0)

          sl.insertChannel.store(def.insertCh, std::memory_order_relaxed);
      }
    }
  }

  void setSelectedSlot(int idx) {

    selectedSlot = juce::jlimit(0, 23, idx);

    updateControls();

    repaint();
  }

  void setSelectedChannel(int chIdx) {

    selectedChannel = juce::jlimit(0, kLogicalChannels - 1, chIdx);

    lblSelectedChannel.setText("SEL CH: CH " +
                                   juce::String(selectedChannel + 1),
                               juce::dontSendNotification);

    updateChannelInsertButtons();

    repaint();
  }

  void updateChannelInsertButtons() {

    auto &slot = engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

    const int curIns = slot.insertChannel.load(std::memory_order_relaxed);

    if (curIns == selectedChannel) {

      btnInsertChannel.setButtonText("INSERTED: CH " +
                                     juce::String(selectedChannel + 1));

      btnInsertChannel.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff10b981));

    } else {

      btnInsertChannel.setButtonText("INSERT TO CH " +
                                     juce::String(selectedChannel + 1));

      btnInsertChannel.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff1e293b));
    }

    if (curIns == nova::graviton::EffectsRack::kMasterInsertChannel) {

      btnInsertMaster.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xffa855f7));

    } else {

      btnInsertMaster.setColour(juce::TextButton::buttonColourId,
                                juce::Colour(0xff1e293b));
    }
  }

  void openDetailView(int slotIdx) {

    selectedSlot = juce::jlimit(0, 23, slotIdx);

    isDetailViewActive = true;

    updateControls();

    resized();

    repaint();
  }

  void closeDetailView() {

    isDetailViewActive = false;

    resized();

    repaint();
  }

  void updateControls() {

    auto &slot = engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

    const auto t = static_cast<nova::graviton::FxType>(
        slot.type.load(std::memory_order_relaxed));

    openEditorButton.setVisible(false);

    for (int i = 0; i < 5; ++i) {

      paramSliders[static_cast<size_t>(i)].onValueChange = {};

      paramSliders[static_cast<size_t>(i)].setEnabled(true);
    }

    const int tId = (t == nova::graviton::FxType::Reverb_VSS4) ? 2

                    : (t == nova::graviton::FxType::StereoDelay) ? 3

                    : (t == nova::graviton::FxType::PitchShifter) ? 4

                    : (t == nova::graviton::FxType::Chorus) ? 5

                    : (t == nova::graviton::FxType::Flanger) ? 6

                    : (t == nova::graviton::FxType::Phaser) ? 7

                    : (t == nova::graviton::FxType::Compressor) ? 8

                    : (t == nova::graviton::FxType::NoiseGate) ? 9

                    : (t == nova::graviton::FxType::Distortion) ? 10

                    : (t == nova::graviton::FxType::MasterX_Multiband) ? 11

                    : (t == nova::graviton::FxType::EMT140_Plate) ? 12

                    : (t == nova::graviton::FxType::SpringReverb ||

                       t == nova::graviton::FxType::DualTank_Spring)
                        ? 13

                    : (t == nova::graviton::FxType::DVR2_NonLinear) ? 14

                    : (t == nova::graviton::FxType::TC_SCF) ? 15

                    : (t == nova::graviton::FxType::TC_NonLin2) ? 16

                    : (t == nova::graviton::FxType::KT_DN780) ? 17

                    : (t == nova::graviton::FxType::SubHarmonic_Synth) ? 20

                    : (t == nova::graviton::FxType::DN360_InductorGEQ) ? 21

                    : (t == nova::graviton::FxType::Precision_DeEsser) ? 22

                    : (t == nova::graviton::FxType::TC_2290_Delay) ? 24

                    : (t == nova::graviton::FxType::KT_1176_Limiter) ? 25

                    : (t == nova::graviton::FxType::KT_BusCompressor) ? 26

                    : (t == nova::graviton::FxType::Reverb_VSS3) ? 27

                    : (t == nova::graviton::FxType::Convolution_Reverb) ? 28

                    : (t == nova::graviton::FxType::Hybrid_Reverb) ? 29

                    : (t == nova::graviton::FxType::Vocal_Pitch_Corrector) ? 30

                                                                           : 1;

    typeCombo.setSelectedId(tId, juce::dontSendNotification);

    bypassButton.setToggleState(slot.bypassed.load(std::memory_order_relaxed),
                                juce::dontSendNotification);

    auxSendCombo.setSelectedId(
        slot.auxSendSource.load(std::memory_order_relaxed) + 2,
        juce::dontSendNotification);

    returnLevelSlider.setValue(
        slot.returnLevelDb.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    returnMuteButton.setToggleState(
        slot.returnMute.load(std::memory_order_relaxed),
        juce::dontSendNotification);

    updateChannelInsertButtons();

    slotTitle.setText("SLOT " + juce::String(selectedSlot + 1) + " - " +
                          getFxTypeName(t),

                      juce::dontSendNotification);

    if (t == nova::graviton::FxType::Reverb_VSS4) {

      paramLabels[0].setText("Master Decay (s):", juce::dontSendNotification);

      paramSliders[0].setRange(0.1, 20.0, 0.1);

      paramSliders[0].setValue(slot.vss4Reverb.getMasterDecay(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" s");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.vss4Reverb.setMasterDecay(
            static_cast<float>(paramSliders[0].getValue()));
      };

      paramLabels[1].setText("Reverb Size:", juce::dontSendNotification);

      paramSliders[1].setRange(1.0, 25.0, 1.0);

      paramSliders[1].setValue(slot.vss4Reverb.getReverbSize(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix("");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.vss4Reverb.setReverbSize(
            static_cast<float>(paramSliders[1].getValue()));
      };

      paramLabels[2].setText("Pre-Delay (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 300.0, 1.0);

      paramSliders[2].setValue(slot.vss4Reverb.getPreDelayMs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.vss4Reverb.setPreDelayMs(
            static_cast<float>(paramSliders[2].getValue()));
      };

      paramLabels[3].setText("Hi-Cut Damping:", juce::dontSendNotification);

      paramSliders[3].setRange(500.0, 20000.0, 50.0);

      paramSliders[3].setValue(slot.vss4Reverb.getHiCutHz(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" Hz");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.vss4Reverb.setHiCutHz(
            static_cast<float>(paramSliders[3].getValue()));
      };

      paramLabels[4].setText("Early Decrease:", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.vss4Reverb.getEarlyDecrease(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.vss4Reverb.setEarlyDecrease(
            static_cast<float>(paramSliders[4].getValue()));
      };

      toggleParam.setVisible(true);

      toggleParam.setButtonText("OPEN VSS4 HD TOUCHSCREEN EDITOR");

      toggleParam.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff238636));

      toggleParam.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::StereoDelay) {

      paramLabels[0].setText("Left Delay (ms):", juce::dontSendNotification);

      paramSliders[0].setRange(1.0, 2500.0, 1.0);

      paramSliders[0].setValue(slot.nebulaDelay.getDelayTimeMsL(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" ms");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.nebulaDelay.setDelayTimeMsL(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Right Delay (ms):", juce::dontSendNotification);

      paramSliders[1].setRange(1.0, 2500.0, 1.0);

      paramSliders[1].setValue(slot.nebulaDelay.getDelayTimeMsR(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.nebulaDelay.setDelayTimeMsR(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Feedback L (%):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 100.0, 1.0);

      paramSliders[2].setValue(slot.nebulaDelay.getFeedbackL(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" %");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.nebulaDelay.setFeedbackL(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Crossfeed (%):", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 100.0, 1.0);

      paramSliders[3].setValue(slot.nebulaDelay.getCrossfeedLR(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" %");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.nebulaDelay.setCrossfeedLR(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.nebulaDelay.getWetLevelL(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.nebulaDelay.setWetLevelL(paramSliders[4].getValue());

        slot.nebulaDelay.setWetLevelR(paramSliders[4].getValue());
      };

      toggleParam.setVisible(true);

      toggleParam.setButtonText("OPEN NEBULA STEREO DELAY EDITOR");

      toggleParam.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff238636));

      toggleParam.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::PitchShifter) {

      paramLabels[0].setText("Pitch Shift (st):", juce::dontSendNotification);

      paramSliders[0].setRange(-24.0, 24.0, 1.0);

      paramSliders[0].setValue(slot.manipulator.getPitch(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" st");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.manipulator.setPitch(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Formant (st):", juce::dontSendNotification);

      paramSliders[1].setRange(-12.0, 12.0, 0.5);

      paramSliders[1].setValue(slot.manipulator.getFormant(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" st");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.manipulator.setFormant(paramSliders[1].getValue());
      };

      paramLabels[2].setText("FM Amount:", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 24.0, 0.5);

      paramSliders[2].setValue(slot.manipulator.getFm(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix("");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.manipulator.setFm(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Detune (ct):", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 50.0, 1.0);

      paramSliders[3].setValue(slot.manipulator.getDetune(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" ct");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.manipulator.setDetune(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Dry/Wet (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.manipulator.getDryWet(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.manipulator.setDryWet(paramSliders[4].getValue());
      };

      toggleParam.setVisible(true);

      toggleParam.setButtonText("OPEN MANIPULATOR PITCH SHIFTER EDITOR");

      toggleParam.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff238636));

      toggleParam.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Chorus) {

      paramLabels[0].setText("Delay (ms):", juce::dontSendNotification);

      paramSliders[0].setRange(5.0, 50.0, 0.5);

      paramSliders[0].setValue(slot.chorus.getDelayMs(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" ms");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.chorus.setDelayMs(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Mod Rate (Hz):", juce::dontSendNotification);

      paramSliders[1].setRange(0.05, 5.0, 0.05);

      paramSliders[1].setValue(slot.chorus.getModRateHz(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" Hz");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.chorus.setModRateHz(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Mod Depth (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 20.0, 0.1);

      paramSliders[2].setValue(slot.chorus.getModDepthMs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.chorus.setModDepthMs(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Wet Mix:", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 100.0, 1.0);

      paramSliders[3].setValue(slot.chorus.getMix() * 100.0,

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" %");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.chorus.setMix(paramSliders[3].getValue() * 0.01);
      };

      paramLabels[4].setText("---", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 1.0, 1.0);

      paramSliders[4].setValue(0.0, juce::dontSendNotification);

    } else if (t == nova::graviton::FxType::Vocal_Pitch_Corrector) {

      paramLabels[0].setText("Retune Speed (ms):", juce::dontSendNotification);
      paramSliders[0].setRange(0.0, 400.0, 1.0);
      paramSliders[0].setValue(slot.vocalPitch.getRetuneSpeedMs(),
                               juce::dontSendNotification);
      paramSliders[0].setTextValueSuffix(" ms");
      paramSliders[0].onValueChange = [&slot, this] {
        slot.vocalPitch.setRetuneSpeedMs(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Humanize (%):", juce::dontSendNotification);
      paramSliders[1].setRange(0.0, 100.0, 1.0);
      paramSliders[1].setValue(slot.vocalPitch.getHumanizePct(),
                               juce::dontSendNotification);
      paramSliders[1].setTextValueSuffix(" %");
      paramSliders[1].onValueChange = [&slot, this] {
        slot.vocalPitch.setHumanizePct(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Key (0=C .. 11=B):", juce::dontSendNotification);
      paramSliders[2].setRange(0.0, 11.0, 1.0);
      paramSliders[2].setValue(static_cast<double>(slot.vocalPitch.getKey()),
                               juce::dontSendNotification);
      paramSliders[2].setTextValueSuffix("");
      paramSliders[2].onValueChange = [&slot, this] {
        slot.vocalPitch.setKey(static_cast<int>(paramSliders[2].getValue()));
      };

      paramLabels[3].setText("Scale (0..12):", juce::dontSendNotification);
      paramSliders[3].setRange(0.0, 12.0, 1.0);
      paramSliders[3].setValue(static_cast<double>(slot.vocalPitch.getScale()),
                               juce::dontSendNotification);
      paramSliders[3].setTextValueSuffix("");
      paramSliders[3].onValueChange = [&slot, this] {
        slot.vocalPitch.setScale(static_cast<int>(paramSliders[3].getValue()));
      };

      paramLabels[4].setText("Throat Length (%):", juce::dontSendNotification);
      paramSliders[4].setRange(-50.0, 50.0, 1.0);
      paramSliders[4].setValue(slot.vocalPitch.getThroatLengthPct(),
                               juce::dontSendNotification);
      paramSliders[4].setTextValueSuffix(" %");
      paramSliders[4].onValueChange = [&slot, this] {
        slot.vocalPitch.setThroatLengthPct(paramSliders[4].getValue());
      };

      const bool fp = slot.vocalPitch.getFormantPreserve();
      toggleParam.setButtonText(fp ? "FORMANT: ON" : "FORMANT: OFF");
      toggleParam.setToggleState(fp, juce::dontSendNotification);
      toggleParam.setVisible(true);
      toggleParam.onClick = [&slot, this] {
        const bool on = toggleParam.getToggleState();
        slot.vocalPitch.setFormantPreserve(on);
        toggleParam.setButtonText(on ? "FORMANT: ON" : "FORMANT: OFF");
      };

      openEditorButton.setButtonText("OPEN VOCAL PITCH EVO EDITOR");
      openEditorButton.setVisible(true);
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Flanger) {

      paramLabels[0].setText("Doppler Mix:", juce::dontSendNotification);
      paramSliders[0].setRange(0.0, 100.0, 1.0);
      paramSliders[0].setValue(slot.flanger.getMix() * 100.0,
                               juce::dontSendNotification);
      paramSliders[0].setTextValueSuffix(" %");
      paramSliders[0].onValueChange = [&slot, this] {
        slot.flanger.setMix(paramSliders[0].getValue() * 0.01);
      };

      paramLabels[1].setText("Regen (FB):", juce::dontSendNotification);
      paramSliders[1].setRange(0.0, 98.0, 1.0);
      paramSliders[1].setValue(slot.flanger.getRegen() * 100.0,
                               juce::dontSendNotification);
      paramSliders[1].setTextValueSuffix(" %");
      paramSliders[1].onValueChange = [&slot, this] {
        slot.flanger.setRegen(paramSliders[1].getValue() * 0.01);
      };

      paramLabels[2].setText("Auto Rate:", juce::dontSendNotification);
      paramSliders[2].setRange(0.02, 12.0, 0.02);
      paramSliders[2].setValue(slot.flanger.getAutoRateHz(),
                               juce::dontSendNotification);
      paramSliders[2].setTextValueSuffix(" Hz");
      paramSliders[2].onValueChange = [&slot, this] {
        slot.flanger.setAutoRateHz(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Auto Depth:", juce::dontSendNotification);
      paramSliders[3].setRange(0.0, 100.0, 1.0);
      paramSliders[3].setValue(slot.flanger.getAutoDepth() * 100.0,
                               juce::dontSendNotification);
      paramSliders[3].setTextValueSuffix(" %");
      paramSliders[3].onValueChange = [&slot, this] {
        slot.flanger.setAutoDepth(paramSliders[3].getValue() * 0.01);
      };

      paramLabels[4].setText("Wet Hi-Pass:", juce::dontSendNotification);
      paramSliders[4].setRange(15.0, 3000.0, 5.0);
      paramSliders[4].setValue(slot.flanger.getHiPassHz(),
                               juce::dontSendNotification);
      paramSliders[4].setTextValueSuffix(" Hz");
      paramSliders[4].onValueChange = [&slot, this] {
        slot.flanger.setHiPassHz(paramSliders[4].getValue());
      };

      toggleParam.setVisible(true);
      toggleParam.setButtonText("OPEN FLANGER BL-20 EDITOR");
      toggleParam.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff1d4ed8));
      toggleParam.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Phaser) {

      paramLabels[0].setText("LFO Depth (%):", juce::dontSendNotification);

      paramSliders[0].setRange(0.0, 100.0, 1.0);

      paramSliders[0].setValue(slot.phaser.getDepthPct(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" %");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.phaser.setDepthPct(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Centre Freq (%):", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 100.0, 1.0);

      paramSliders[1].setValue(slot.phaser.getCenterPct(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" %");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.phaser.setCenterPct(paramSliders[1].getValue());
      };

      paramLabels[2].setText("LFO Rate (%):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 100.0, 1.0);

      paramSliders[2].setValue(slot.phaser.getRatePct(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" %");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.phaser.setRatePct(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Feedback (%):", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 100.0, 1.0);

      paramSliders[3].setValue(slot.phaser.getFeedbackPct(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" %");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.phaser.setFeedbackPct(paramSliders[3].getValue());
      };

      paramLabels[4].setText("AUX Wet (0/1):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 1.0, 1.0);

      paramSliders[4].setValue(slot.phaser.isAuxRouting() ? 1.0 : 0.0,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix("");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.phaser.setRoutingMode(

            paramSliders[4].getValue() >= 0.5

                ? nova::graviton::OpticalPhaserEngine::RoutingMode::AuxFullWet

                : nova::graviton::OpticalPhaserEngine::RoutingMode::
                      Insert50_50);
      };

      toggleParam.setVisible(true);

      toggleParam.setButtonText("OPEN OPTICAL PHASER EDITOR");

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff4f46e5));

      toggleParam.onClick = [this] {
        if (onSlotDoubleClicked)

          onSlotDoubleClicked(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Compressor) {

      paramLabels[0].setText("Threshold (dB):", juce::dontSendNotification);

      paramSliders[0].setRange(-40.0, 0.0, 0.5);

      paramSliders[0].setValue(slot.getCompressorThresholdDb(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" dB");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.setCompressorThresholdDb(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Ratio:", juce::dontSendNotification);

      paramSliders[1].setRange(1.0, 20.0, 0.5);

      paramSliders[1].setValue(slot.getCompressorRatio(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" :1");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.setCompressorRatio(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Attack (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(0.1, 100.0, 0.1);

      paramSliders[2].setValue(slot.getCompressorAttackMs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.setCompressorAttackMs(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Release (ms):", juce::dontSendNotification);

      paramSliders[3].setRange(10.0, 500.0, 1.0);

      paramSliders[3].setValue(slot.getCompressorReleaseMs(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" ms");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.setCompressorReleaseMs(paramSliders[3].getValue());
      };

      const auto getModeName = [](int m) -> juce::String {
        switch (static_cast<nova::graviton::CompressorMode>(m)) {

        case nova::graviton::CompressorMode::Corrective:

          return "CORRECTIVE";

        case nova::graviton::CompressorMode::Adaptive:

          return "ADAPTIVE";

        case nova::graviton::CompressorMode::Creative:

          return "CREATIVE";

        case nova::graviton::CompressorMode::Vintage:

          return "VINTAGE";

        default:

          return "OFF";
        }
      };

      paramLabels[4].setText("Comp Mode (1-4):", juce::dontSendNotification);

      paramSliders[4].setRange(1.0, 4.0, 1.0);

      paramSliders[4].setValue(slot.getCompressorMode(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix("");

      paramSliders[4].onValueChange = [&slot, this, getModeName] {
        const int m = static_cast<int>(paramSliders[4].getValue());

        slot.setCompressorMode(m);

        toggleParam.setButtonText("MODE: " + getModeName(m));
      };

      toggleParam.setVisible(true);

      const int curMode = slot.getCompressorMode();

      toggleParam.setButtonText("MODE: " + getModeName(curMode));

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff238636));

      toggleParam.onClick = [&slot, this, getModeName] {
        int m = slot.getCompressorMode() + 1;

        if (m > 4)

          m = 1;

        slot.setCompressorMode(m);

        paramSliders[4].setValue(m, juce::dontSendNotification);

        toggleParam.setButtonText("MODE: " + getModeName(m));
      };

      openEditorButton.setButtonText("OPEN RACK DYNAMICS EDITOR");
      openEditorButton.setVisible(true);
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::NoiseGate) {

      paramLabels[0].setText("Threshold (dB):", juce::dontSendNotification);

      paramSliders[0].setRange(-60.0, 0.0, 0.5);

      paramSliders[0].setValue(slot.getGateThresholdDb(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" dB");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.setGateThresholdDb(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Ratio:", juce::dontSendNotification);

      paramSliders[1].setRange(1.0, 10.0, 0.5);

      paramSliders[1].setValue(slot.getGateRatio(), juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" :1");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.setGateRatio(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Attack (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(0.1, 50.0, 0.1);

      paramSliders[2].setValue(slot.getGateAttackMs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.setGateAttackMs(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Release (ms):", juce::dontSendNotification);

      paramSliders[3].setRange(10.0, 200.0, 1.0);

      paramSliders[3].setValue(slot.getGateReleaseMs(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" ms");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.setGateReleaseMs(paramSliders[3].getValue());
      };

      paramLabels[4].setText("---", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 1.0, 1.0);

      paramSliders[4].setValue(0.0, juce::dontSendNotification);

      toggleParam.setVisible(false);

      openEditorButton.setButtonText("OPEN RACK DYNAMICS EDITOR");
      openEditorButton.setVisible(true);
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Distortion) {

      paramLabels[0].setText("Drive (dB):", juce::dontSendNotification);

      paramSliders[0].setRange(0.0, 24.0, 0.5);

      paramSliders[0].setValue(slot.getDistortionDriveDb(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" dB");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.setDistortionDriveDb(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Wet Mix:", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 100.0, 1.0);

      paramSliders[1].setValue(slot.getDistortionMix() * 100.0,

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" %");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.setDistortionMix(paramSliders[1].getValue() * 0.01);
      };

      paramLabels[2].setText("---", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 1.0, 1.0);

      paramSliders[2].setValue(0.0, juce::dontSendNotification);

      paramLabels[3].setText("---", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 1.0, 1.0);

      paramSliders[3].setValue(0.0, juce::dontSendNotification);

      paramLabels[4].setText("---", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 1.0, 1.0);

      paramSliders[4].setValue(0.0, juce::dontSendNotification);

      toggleParam.setVisible(false);

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN DISTORTION / SATURATOR EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xffc2410c));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::MasterX_Multiband) {

      toggleParam.setVisible(true);

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff8957e5));

      toggleParam.setButtonText(

          masterXEditTab == 0 ? "TAB: LOW BAND (EDITING)"

          : masterXEditTab == 1 ? "TAB: MID BAND (EDITING)"

          : masterXEditTab == 2 ? "TAB: HIGH BAND (EDITING)"

                                : "TAB: XOVER & LIMITER (EDITING)");

      toggleParam.onClick = [this] {
        masterXEditTab = (masterXEditTab + 1) % 4;

        updateControls();
      };

      openEditorButton.setVisible(true);

      openEditorButton.setButtonText("OPEN MASTER X HD EDITOR");

      openEditorButton.setColour(juce::TextButton::buttonColourId,

                                 juce::Colour(0xff1f6feb));

      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

      if (masterXEditTab < 3) {

        const size_t band = static_cast<size_t>(masterXEditTab);

        paramLabels[0].setText("Threshold (dB):", juce::dontSendNotification);

        paramSliders[0].setRange(-40.0, 0.0, 0.5);

        paramSliders[0].setValue(slot.masterX.getBandThresholdDb(band),

                                 juce::dontSendNotification);

        paramSliders[0].setTextValueSuffix(" dB");

        paramSliders[0].onValueChange = [&slot, band, this] {
          slot.masterX.setBandThresholdDb(band, paramSliders[0].getValue());
        };

        paramLabels[1].setText("Ratio:", juce::dontSendNotification);

        paramSliders[1].setRange(1.0, 20.0, 0.5);

        paramSliders[1].setValue(slot.masterX.getBandRatio(band),

                                 juce::dontSendNotification);

        paramSliders[1].setTextValueSuffix(" :1");

        paramSliders[1].onValueChange = [&slot, band, this] {
          slot.masterX.setBandRatio(band, paramSliders[1].getValue());
        };

        paramLabels[2].setText("Attack (ms):", juce::dontSendNotification);

        paramSliders[2].setRange(0.1, 100.0, 0.1);

        paramSliders[2].setValue(slot.masterX.getBandAttackMs(band),

                                 juce::dontSendNotification);

        paramSliders[2].setTextValueSuffix(" ms");

        paramSliders[2].onValueChange = [&slot, band, this] {
          slot.masterX.setBandAttackMs(band, paramSliders[2].getValue());
        };

        paramLabels[3].setText("Release (ms):", juce::dontSendNotification);

        paramSliders[3].setRange(10.0, 2000.0, 5.0);

        paramSliders[3].setValue(slot.masterX.getBandReleaseMs(band),

                                 juce::dontSendNotification);

        paramSliders[3].setTextValueSuffix(" ms");

        paramSliders[3].onValueChange = [&slot, band, this] {
          slot.masterX.setBandReleaseMs(band, paramSliders[3].getValue());
        };

        paramLabels[4].setText("Makeup Gain:", juce::dontSendNotification);

        paramSliders[4].setRange(-12.0, 18.0, 0.5);

        paramSliders[4].setValue(slot.masterX.getBandMakeupGainDb(band),

                                 juce::dontSendNotification);

        paramSliders[4].setTextValueSuffix(" dB");

        paramSliders[4].onValueChange = [&slot, band, this] {
          slot.masterX.setBandMakeupGainDb(band, paramSliders[4].getValue());
        };

      } else {

        paramLabels[0].setText("Low/Mid Xover:", juce::dontSendNotification);

        paramSliders[0].setRange(40.0, 1000.0, 5.0);

        paramSliders[0].setValue(slot.masterX.getCrossoverLowMidHz(),

                                 juce::dontSendNotification);

        paramSliders[0].setTextValueSuffix(" Hz");

        paramSliders[0].onValueChange = [&slot, this] {
          slot.masterX.setCrossoverLowMidHz(paramSliders[0].getValue());
        };

        paramLabels[1].setText("Mid/High Xover:", juce::dontSendNotification);

        paramSliders[1].setRange(1000.0, 16000.0, 50.0);

        paramSliders[1].setValue(slot.masterX.getCrossoverMidHighHz(),

                                 juce::dontSendNotification);

        paramSliders[1].setTextValueSuffix(" Hz");

        paramSliders[1].onValueChange = [&slot, this] {
          slot.masterX.setCrossoverMidHighHz(paramSliders[1].getValue());
        };

        paramLabels[2].setText("Limiter Ceiling:", juce::dontSendNotification);

        paramSliders[2].setRange(-12.0, 0.0, 0.1);

        paramSliders[2].setValue(slot.masterX.getLimiterCeilingDb(),

                                 juce::dontSendNotification);

        paramSliders[2].setTextValueSuffix(" dBFS");

        paramSliders[2].onValueChange = [&slot, this] {
          slot.masterX.setLimiterCeilingDb(paramSliders[2].getValue());
        };

        paramLabels[3].setText("Lim Release:", juce::dontSendNotification);

        paramSliders[3].setRange(10.0, 500.0, 5.0);

        paramSliders[3].setValue(slot.masterX.getLimiterReleaseMs(),

                                 juce::dontSendNotification);

        paramSliders[3].setTextValueSuffix(" ms");

        paramSliders[3].onValueChange = [&slot, this] {
          slot.masterX.setLimiterReleaseMs(paramSliders[3].getValue());
        };

        paramLabels[4].setText("Soft Knee (dB):", juce::dontSendNotification);

        paramSliders[4].setRange(0.0, 12.0, 0.5);

        paramSliders[4].setValue(slot.masterX.getBandKneeDb(0),

                                 juce::dontSendNotification);

        paramSliders[4].setTextValueSuffix(" dB");

        paramSliders[4].onValueChange = [&slot, this] {
          const double k = paramSliders[4].getValue();

          slot.masterX.setBandKneeDb(0, k);

          slot.masterX.setBandKneeDb(1, k);

          slot.masterX.setBandKneeDb(2, k);
        };
      }

    } else if (t == nova::graviton::FxType::EMT140_Plate ||

               t == nova::graviton::FxType::KT_DN780) {

      if (t == nova::graviton::FxType::KT_DN780)

        slot.plate.setMode(1);

      else

        slot.plate.setMode(0);

      paramLabels[0].setText("Decay Time (s):", juce::dontSendNotification);

      paramSliders[0].setRange(0.5, 12.0, 0.1);

      paramSliders[0].setValue(slot.plate.getDecay(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" s");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.plate.setDecay(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Pre-Delay (ms):", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 250.0, 1.0);

      paramSliders[1].setValue(slot.plate.getPreDelay(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.plate.setPreDelay(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Drive (dB):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 24.0, 0.5);

      paramSliders[2].setValue(slot.plate.getDriveDb(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" dB");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.plate.setDriveDb(paramSliders[2].getValue());
      };

      paramLabels[3].setText("High Damping:", juce::dontSendNotification);

      paramSliders[3].setRange(1000.0, 16000.0, 100.0);

      paramSliders[3].setValue(slot.plate.getDampingHz(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" Hz");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.plate.setDampingHz(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix:", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.plate.getMix() * 100.0,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.plate.setMix(paramSliders[4].getValue() * 0.01);
      };

      toggleParam.setVisible(true);

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff238636));

      toggleParam.setButtonText(slot.plate.getMode() == 0

                                    ? "MODE: EMT 140 STEEL PLATE"

                                    : "MODE: KT DN780 DIGITAL");

      toggleParam.onClick = [&slot, this] {
        const int nextMode = (slot.plate.getMode() + 1) % 2;

        slot.plate.setMode(nextMode);

        toggleParam.setButtonText(nextMode == 0 ? "MODE: EMT 140 STEEL PLATE"

                                                : "MODE: KT DN780 DIGITAL");
      };

      openEditorButton.setVisible(true);

      openEditorButton.setButtonText(slot.plate.getMode() == 0

                                         ? "OPEN EMT 140 PLATE EDITOR"

                                         : "OPEN KT DN780 PLATE EDITOR");

      openEditorButton.setColour(juce::TextButton::buttonColourId,

                                 juce::Colour(0xff238636));

      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::SpringReverb ||

               t == nova::graviton::FxType::DualTank_Spring) {

      paramLabels[0].setText("Decay Time (s):", juce::dontSendNotification);

      paramSliders[0].setRange(0.5, 6.0, 0.1);

      paramSliders[0].setValue(slot.spring.getCh1Decay(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" s");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.spring.setCh1Decay(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Pre-Delay:", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 250.0, 1.0);

      paramSliders[1].setValue(slot.spring.getPreDelay(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.spring.setPreDelay(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Bass / Treble:", juce::dontSendNotification);

      paramSliders[2].setRange(-12.0, 12.0, 0.5);

      paramSliders[2].setValue(slot.spring.getBassDb(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" dB");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.spring.setBassDb(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Input Gain:", juce::dontSendNotification);

      paramSliders[3].setRange(-12.0, 12.0, 0.5);

      paramSliders[3].setValue(slot.spring.getInputGainDb(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" dB");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.spring.setInputGainDb(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix:", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.spring.getMix() * 100.0,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.spring.setMix(paramSliders[4].getValue() * 0.01);
      };

      toggleParam.setVisible(true);

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xffd29922));

      toggleParam.setButtonText("SPRING TANK: AKG BX20 DUAL COIL");

      toggleParam.onClick = nullptr;

      openEditorButton.setVisible(true);

      openEditorButton.setButtonText("OPEN AKG BX20 REVERB EDITOR");

      openEditorButton.setColour(juce::TextButton::buttonColourId,

                                 juce::Colour(0xff8b4513));

      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::DVR2_NonLinear) {

      paramLabels[0].setText("Decay (s):", juce::dontSendNotification);

      paramSliders[0].setRange(0.2, 20.0, 0.1);

      paramSliders[0].setValue(slot.dvr2.getDecay(),
                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" s");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.dvr2.setDecay(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Pre Delay (ms):", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 500.0, 1.0);

      paramSliders[1].setValue(slot.dvr2.getPreDelay(),
                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.dvr2.setPreDelay(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Lo Decay Mult:", juce::dontSendNotification);

      paramSliders[2].setRange(0.1, 2.5, 0.05);

      paramSliders[2].setValue(slot.dvr2.getLoDecay(),
                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" x");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.dvr2.setLoDecay(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Hi Cut (Hz):", juce::dontSendNotification);

      paramSliders[3].setRange(1000.0, 18000.0, 100.0);

      paramSliders[3].setValue(slot.dvr2.getHiCut(),
                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" Hz");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.dvr2.setHiCut(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Modulation:", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 200.0, 1.0);

      paramSliders[4].setValue(slot.dvr2.getModulation(),
                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.dvr2.setModulation(paramSliders[4].getValue());
      };

      toggleParam.setVisible(true);

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff38bdf8));

      toggleParam.setButtonText("DVR2 EMT 250 EMULATION ACTIVE");

      toggleParam.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

      openEditorButton.setVisible(true);

      openEditorButton.setButtonText("OPEN DVR2 SYSTEM 6000 EDITOR");

      openEditorButton.setColour(juce::TextButton::buttonColourId,

                                 juce::Colour(0xff1f6feb));

      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::TC_SCF) {

      paramLabels[0].setText("Speed (0.1-10 Hz):", juce::dontSendNotification);
      paramSliders[0].setRange(0.1, 10.0, 0.05);
      paramSliders[0].setValue(slot.scf.getSpeed(), juce::dontSendNotification);
      paramSliders[0].setTextValueSuffix(" Hz");
      paramSliders[0].onValueChange = [&slot, this] {
        slot.scf.setSpeed(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Width (0-6):", juce::dontSendNotification);
      paramSliders[1].setRange(0.0, 6.0, 0.05);
      paramSliders[1].setValue(slot.scf.getWidthDial(),
                               juce::dontSendNotification);
      paramSliders[1].setTextValueSuffix("");
      paramSliders[1].onValueChange = [&slot, this] {
        slot.scf.setWidthDial(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Intensity (0-6):", juce::dontSendNotification);
      paramSliders[2].setRange(0.0, 6.0, 0.05);
      paramSliders[2].setValue(slot.scf.getIntensityDial(),
                               juce::dontSendNotification);
      paramSliders[2].setTextValueSuffix("");
      paramSliders[2].onValueChange = [&slot, this] {
        slot.scf.setIntensityDial(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Input Gain (0-15 dB):",
                             juce::dontSendNotification);
      paramSliders[3].setRange(0.0, 15.0, 0.1);
      paramSliders[3].setValue(slot.scf.getInputGainDb(),
                               juce::dontSendNotification);
      paramSliders[3].setTextValueSuffix(" dB");
      paramSliders[3].onValueChange = [&slot, this] {
        slot.scf.setInputGainDb(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);
      paramSliders[4].setRange(0.0, 100.0, 1.0);
      paramSliders[4].setValue(slot.scf.getMix() * 100.0,
                               juce::dontSendNotification);
      paramSliders[4].setTextValueSuffix(" %");
      paramSliders[4].onValueChange = [&slot, this] {
        slot.scf.setMix(paramSliders[4].getValue() * 0.01);
      };

      toggleParam.setVisible(true);
      toggleParam.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xffd4af37));
      auto m = slot.scf.getMode();
      toggleParam.setButtonText(m == nova::graviton::SCFMode::Chorus
                                    ? "MODE: CHORUS"
                                    : (m == nova::graviton::SCFMode::Flanger
                                           ? "MODE: FLANGER"
                                           : "MODE: PITCH MOD (P.M.)"));
      toggleParam.onClick = [&slot, this] {
        auto cur = slot.scf.getMode();
        auto next = (cur == nova::graviton::SCFMode::Chorus)
                        ? nova::graviton::SCFMode::PitchMod
                        : ((cur == nova::graviton::SCFMode::PitchMod)
                               ? nova::graviton::SCFMode::Flanger
                               : nova::graviton::SCFMode::Chorus);
        slot.scf.setMode(next);
        toggleParam.setButtonText(
            next == nova::graviton::SCFMode::Chorus
                ? "MODE: CHORUS"
                : (next == nova::graviton::SCFMode::Flanger
                       ? "MODE: FLANGER"
                       : "MODE: PITCH MOD (P.M.)"));
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN TC SCF GOLD EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xffb45309));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::TC_NonLin2) {

      paramLabels[0].setText("Pre-Delay (ms):", juce::dontSendNotification);

      paramSliders[0].setRange(0.0, 500.0, 1.0);

      paramSliders[0].setValue(slot.nonLin2.getPreDelayMs(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" ms");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.nonLin2.setPreDelayMs(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Hold Time (ms):", juce::dontSendNotification);

      paramSliders[1].setRange(10.0, 1000.0, 5.0);

      paramSliders[1].setValue(slot.nonLin2.getHoldMs(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.nonLin2.setHoldMs(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Release (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(10.0, 1000.0, 5.0);

      paramSliders[2].setValue(slot.nonLin2.getReleaseMs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.nonLin2.setReleaseMs(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Decay Slope:", juce::dontSendNotification);

      paramSliders[3].setRange(-1.0, 1.0, 0.05);

      paramSliders[3].setValue(slot.nonLin2.getSlope(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" SLOPE");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.nonLin2.setSlope(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.nonLin2.getMix() * 100.0,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.nonLin2.setMix(paramSliders[4].getValue() * 0.01);
      };

      toggleParam.setVisible(true);

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff9a3412));

      const double sl = slot.nonLin2.getSlope();

      toggleParam.setButtonText(sl < -0.05

                                    ? "ENVELOPE: REVERSE REVERB"

                                    : (sl > 0.05 ? "ENVELOPE: EXPONENTIAL DECAY"

                                                 : "ENVELOPE: GATED BURST"));

      toggleParam.onClick = [&slot, this] {
        double cur = slot.nonLin2.getSlope();

        if (cur == 0.0)

          slot.nonLin2.setSlope(-1.0);

        else if (cur < 0.0)

          slot.nonLin2.setSlope(1.0);

        else

          slot.nonLin2.setSlope(0.0);

        const double newSl = slot.nonLin2.getSlope();

        toggleParam.setButtonText(

            newSl < -0.05 ? "ENVELOPE: REVERSE REVERB"

                          : (newSl > 0.05 ? "ENVELOPE: EXPONENTIAL DECAY"

                                          : "ENVELOPE: GATED BURST"));

        paramSliders[3].setValue(newSl, juce::dontSendNotification);
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN NONLIN2 EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xFF2B6EA8));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::SubHarmonic_Synth) {

      paramLabels[0].setText("Split Frequency (Hz):",

                             juce::dontSendNotification);

      paramSliders[0].setRange(40.0, 250.0, 1.0);

      paramSliders[0].setValue(slot.subHarmonic.getSplitFrequencyHz(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" Hz");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.subHarmonic.setSplitFrequencyHz(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Sub Level (%):", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 100.0, 1.0);

      paramSliders[1].setValue(slot.subHarmonic.getSubLevel() * 100.0,

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" %");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.subHarmonic.setSubLevel(paramSliders[1].getValue() * 0.01);
      };

      paramLabels[2].setText("Direct Level (%):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 100.0, 1.0);

      paramSliders[2].setValue(slot.subHarmonic.getDirectLevel() * 100.0,

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" %");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.subHarmonic.setDirectLevel(paramSliders[2].getValue() * 0.01);
      };

      paramLabels[3].setText("Response (ms):", juce::dontSendNotification);

      paramSliders[3].setRange(10.0, 500.0, 1.0);

      paramSliders[3].setValue(slot.subHarmonic.getResponseMs(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" ms");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.subHarmonic.setResponseMs(paramSliders[3].getValue());
      };

      paramLabels[4].setText("---", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 1.0, 1.0);

      paramSliders[4].setValue(0.0, juce::dontSendNotification);

      toggleParam.setVisible(true);

      toggleParam.setButtonText("RE-LOCK SUB OCTAVE");

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff1f6feb));

      toggleParam.onClick = [&slot] { slot.subHarmonic.reset(); };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN SUBHARMONIC SYNTH EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff9c1414));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::DN360_InductorGEQ) {

      const size_t band = static_cast<size_t>(juce::jlimit(

          0, static_cast<int>(nova::graviton::DN360InductorGEQ::kNumBands) - 1,

          selectedEqBand[static_cast<size_t>(selectedSlot)]));

      paramLabels[0].setText("GEQ Band (1-30):", juce::dontSendNotification);

      paramSliders[0].setRange(

          1.0, static_cast<double>(nova::graviton::DN360InductorGEQ::kNumBands),

          1.0);

      paramSliders[0].setValue(static_cast<double>(band) + 1.0,

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" / 30");

      paramSliders[0].onValueChange = [this, &slot] {
        const int newBand = juce::jlimit(

            0,

            static_cast<int>(nova::graviton::DN360InductorGEQ::kNumBands) - 1,

            static_cast<int>(paramSliders[0].getValue()) - 1);

        selectedEqBand[static_cast<size_t>(selectedSlot)] = newBand;

        paramSliders[1].setValue(

            slot.dn360.getBandGain(static_cast<size_t>(newBand)),

            juce::dontSendNotification);

        paramSliders[2].setValue(

            nova::graviton::DN360InductorGEQ::kBandFrequencies

                [static_cast<size_t>(newBand)],

            juce::dontSendNotification);
      };

      paramLabels[1].setText("Band Gain (dB):", juce::dontSendNotification);

      paramSliders[1].setRange(nova::graviton::DN360InductorGEQ::kMinGainDb,

                               nova::graviton::DN360InductorGEQ::kMaxGainDb,

                               0.1);

      paramSliders[1].setValue(slot.dn360.getBandGain(band),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" dB");

      paramSliders[1].onValueChange = [this, &slot] {
        slot.dn360.setBandGain(

            static_cast<size_t>(

                selectedEqBand[static_cast<size_t>(selectedSlot)]),

            paramSliders[1].getValue());
      };

      paramLabels[2].setText("Centre Frequency:", juce::dontSendNotification);

      paramSliders[2].setRange(20.0, 20000.0, 1.0);

      paramSliders[2].setValue(

          nova::graviton::DN360InductorGEQ::kBandFrequencies[band],

          juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" Hz");

      paramSliders[2].setEnabled(

          false); // display only: frequency of the selected band

      paramLabels[3].setText("---", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 1.0, 1.0);

      paramSliders[3].setValue(0.0, juce::dontSendNotification);

      paramLabels[4].setText("---", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 1.0, 1.0);

      paramSliders[4].setValue(0.0, juce::dontSendNotification);

      toggleParam.setVisible(true);

      toggleParam.setButtonText("FLATTEN ALL 30 BANDS");

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff9a3412));

      toggleParam.onClick = [this, &slot] {
        slot.dn360.flatten();

        paramSliders[1].setValue(0.0, juce::dontSendNotification);

        toggleParam.setToggleState(false, juce::dontSendNotification);
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN DN360 GRAPHIC EQ EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff0e7490));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Precision_DeEsser) {

      paramLabels[0].setText("Threshold (dB):", juce::dontSendNotification);

      paramSliders[0].setRange(-60.0, 0.0, 0.5);

      paramSliders[0].setValue(slot.deEsser.getThresholdDb(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" dB");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.deEsser.setThresholdDb(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Ratio:", juce::dontSendNotification);

      paramSliders[1].setRange(1.0, 20.0, 0.5);

      paramSliders[1].setValue(slot.deEsser.getRatio(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" :1");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.deEsser.setRatio(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Sibilance Freq (Hz):",

                             juce::dontSendNotification);

      paramSliders[2].setRange(

          nova::graviton::PrecisionDeEsser::kMinFrequencyHz,

          nova::graviton::PrecisionDeEsser::kMaxFrequencyHz, 50.0);

      paramSliders[2].setValue(slot.deEsser.getFrequencyHz(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" Hz");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.deEsser.setFrequencyHz(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Max Reduction (dB):", juce::dontSendNotification);

      paramSliders[3].setRange(

          0.0, nova::graviton::PrecisionDeEsser::kMaxReductionCapDb, 0.5);

      paramSliders[3].setValue(slot.deEsser.getMaxReductionDb(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" dB");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.deEsser.setMaxReductionDb(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Attack (ms):", juce::dontSendNotification);

      paramSliders[4].setRange(0.1, 50.0, 0.1);

      paramSliders[4].setValue(slot.deEsser.getAttackMs(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" ms");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.deEsser.setAttackMs(paramSliders[4].getValue());
      };

      const bool splitMode = slot.deEsser.getMode() ==

                             nova::graviton::PrecisionDeEsser::Mode::Split;

      toggleParam.setVisible(true);

      toggleParam.setButtonText(splitMode ? "MODE: SPLIT BAND"

                                          : "MODE: BROADBAND");

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            juce::Colour(0xff238636));

      toggleParam.onClick = [&slot, this] {
        const bool split = slot.deEsser.getMode() !=

                           nova::graviton::PrecisionDeEsser::Mode::Split;

        slot.deEsser.setMode(

            split ? nova::graviton::PrecisionDeEsser::Mode::Split

                  : nova::graviton::PrecisionDeEsser::Mode::Broadband);

        toggleParam.setButtonText(split ? "MODE: SPLIT BAND"

                                        : "MODE: BROADBAND");
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN PRECISION DE-ESSER EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff1f6feb));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::TC_2290_Delay) {

      paramLabels[0].setText("Delay Time (ms):", juce::dontSendNotification);

      paramSliders[0].setRange(1.0, 5000.0, 1.0);

      paramSliders[0].setValue(slot.tc2290.getDelayTimeMs(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" ms");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.tc2290.setDelayTimeMs(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Feedback (%):", juce::dontSendNotification);

      paramSliders[1].setRange(-100.0, 100.0, 1.0);

      paramSliders[1].setValue(slot.tc2290.getFeedback(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" %");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.tc2290.setFeedback(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Ducking Thresh (dB):",

                             juce::dontSendNotification);

      paramSliders[2].setRange(-50.0, 0.0, 0.5);

      paramSliders[2].setValue(slot.tc2290.getDuckingThresholdDb(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" dB");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.tc2290.setDuckingThresholdDb(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Mod Depth (%):", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 100.0, 1.0);

      paramSliders[3].setValue(slot.tc2290.getModDepth(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" %");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.tc2290.setModDepth(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.tc2290.getMix(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.tc2290.setMix(paramSliders[4].getValue());
      };

      toggleParam.setVisible(true);

      toggleParam.setButtonText(slot.tc2290.getDuckingEnabled()

                                    ? "DYNAMIC DUCKING: ON"

                                    : "DYNAMIC DUCKING: OFF");

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            slot.tc2290.getDuckingEnabled()

                                ? juce::Colour(0xff238636)

                                : juce::Colour(0xff2a3440));

      toggleParam.onClick = [&slot, this] {
        const bool d = !slot.tc2290.getDuckingEnabled();

        slot.tc2290.setDuckingEnabled(d);

        toggleParam.setButtonText(d ? "DYNAMIC DUCKING: ON"

                                    : "DYNAMIC DUCKING: OFF");

        toggleParam.setColour(juce::TextButton::buttonColourId,

                              d ? juce::Colour(0xff238636)

                                : juce::Colour(0xff2a3440));
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN TC 2290 DELAY EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xFF9E2626));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::KT_1176_Limiter) {

      paramLabels[0].setText("Input Drive (dB):", juce::dontSendNotification);

      paramSliders[0].setRange(-20.0, 20.0, 0.5);

      paramSliders[0].setValue(slot.kt1176.getInputDb(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" dB");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.kt1176.setInputDb(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Output Makeup (dB):",

                             juce::dontSendNotification);

      paramSliders[1].setRange(-20.0, 20.0, 0.5);

      paramSliders[1].setValue(slot.kt1176.getOutputDb(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" dB");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.kt1176.setOutputDb(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Attack (\xc2\xb5s):",

                             juce::dontSendNotification);

      paramSliders[2].setRange(20.0, 800.0, 10.0);

      paramSliders[2].setValue(slot.kt1176.getAttackUs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" \xc2\xb5s");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.kt1176.setAttackUs(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Release (ms):", juce::dontSendNotification);

      paramSliders[3].setRange(50.0, 1100.0, 10.0);

      paramSliders[3].setValue(slot.kt1176.getReleaseMs(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" ms");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.kt1176.setReleaseMs(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Parallel Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.kt1176.getMix(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.kt1176.setMix(paramSliders[4].getValue());
      };

      const auto r = slot.kt1176.getRatio();

      const char *rText =
          (r == nova::graviton::KT1176Ratio::Ratio_4_1)

              ? "RATIO: 4:1"

              : (r == nova::graviton::KT1176Ratio::Ratio_8_1)

                    ? "RATIO: 8:1"

                    : (r == nova::graviton::KT1176Ratio::Ratio_12_1)

                          ? "RATIO: 12:1"

                          : (r == nova::graviton::KT1176Ratio::Ratio_20_1)

                                ? "RATIO: 20:1"

                                : "RATIO: ALL-BUTTONS (NUKE)";

      toggleParam.setVisible(true);

      toggleParam.setButtonText(rText);

      toggleParam.setColour(

          juce::TextButton::buttonColourId,

          (r == nova::graviton::KT1176Ratio::AllButtons)

              ? juce::Colour(0xffd29922)

              : juce::Colour(0xff238636));

      toggleParam.onClick = [&slot, this] {
        auto curR = static_cast<int>(slot.kt1176.getRatio());

        curR = (curR + 1) % 5;

        slot.kt1176.setRatio(static_cast<nova::graviton::KT1176Ratio>(curR));

        const auto newR = slot.kt1176.getRatio();

        const char *newText =

            (newR == nova::graviton::KT1176Ratio::Ratio_4_1)

                ? "RATIO: 4:1"

                : (newR == nova::graviton::KT1176Ratio::Ratio_8_1)

                      ? "RATIO: 8:1"

                      : (newR == nova::graviton::KT1176Ratio::Ratio_12_1)

                            ? "RATIO: 12:1"

                            : (newR == nova::graviton::KT1176Ratio::Ratio_20_1)

                                  ? "RATIO: 20:1"

                                  : "RATIO: ALL-BUTTONS (NUKE)";

        toggleParam.setButtonText(newText);

        toggleParam.setColour(

            juce::TextButton::buttonColourId,

            (newR == nova::graviton::KT1176Ratio::AllButtons)

                ? juce::Colour(0xffd29922)

                : juce::Colour(0xff238636));
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN 1176LN FET LIMITER EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff1f6feb));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::KT_BusCompressor) {

      paramLabels[0].setText("Threshold (dB):", juce::dontSendNotification);

      paramSliders[0].setRange(-30.0, 15.0, 0.5);

      paramSliders[0].setValue(slot.busComp.getThresholdDb(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" dB");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.busComp.setThresholdDb(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Ratio:", juce::dontSendNotification);

      paramSliders[1].setRange(1.5, 10.0, 0.5);

      paramSliders[1].setValue(slot.busComp.getRatio(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" :1");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.busComp.setRatio(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Attack (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(0.1, 30.0, 0.1);

      paramSliders[2].setValue(slot.busComp.getAttackMs(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.busComp.setAttackMs(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Release (ms):", juce::dontSendNotification);

      paramSliders[3].setRange(50.0, 1200.0, 10.0);

      paramSliders[3].setValue(slot.busComp.getReleaseMs(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" ms");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.busComp.setReleaseMs(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Makeup Gain (dB):",

                             juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 20.0, 0.5);

      paramSliders[4].setValue(slot.busComp.getMakeupGainDb(),

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" dB");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.busComp.setMakeupGainDb(paramSliders[4].getValue());
      };

      const bool hpfOn = slot.busComp.getSidechainHpfHz() > 10.0;

      toggleParam.setVisible(true);

      toggleParam.setButtonText(hpfOn ? "SC HPF: 80 HZ" : "SC HPF: OFF");

      toggleParam.setColour(juce::TextButton::buttonColourId,

                            hpfOn ? juce::Colour(0xff238636)

                                  : juce::Colour(0xff2a3440));

      toggleParam.onClick = [&slot, this] {
        const bool on = slot.busComp.getSidechainHpfHz() <= 10.0;

        slot.busComp.setSidechainHpfHz(on ? 80.0 : 0.0);

        toggleParam.setButtonText(on ? "SC HPF: 80 HZ" : "SC HPF: OFF");

        toggleParam.setColour(juce::TextButton::buttonColourId,

                              on ? juce::Colour(0xff238636)

                                 : juce::Colour(0xff2a3440));
      };

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN G-BUS VCA COMPRESSOR EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff1f6feb));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Reverb_VSS3) {

      paramLabels[0].setText("Decay Time (s):", juce::dontSendNotification);

      paramSliders[0].setRange(0.1, 20.0, 0.1);

      paramSliders[0].setValue(slot.vss3Reverb.getDecay(),

                               juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" s");

      paramSliders[0].onValueChange = [&slot, this] {
        slot.vss3Reverb.setDecay(paramSliders[0].getValue());
      };

      paramLabels[1].setText("Room Size:", juce::dontSendNotification);

      paramSliders[1].setRange(0.1, 2.0, 0.05);

      paramSliders[1].setValue(slot.vss3Reverb.getRoomSize(),

                               juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix("");

      paramSliders[1].onValueChange = [&slot, this] {
        slot.vss3Reverb.setRoomSize(paramSliders[1].getValue());
      };

      paramLabels[2].setText("Pre-Delay (ms):", juce::dontSendNotification);

      paramSliders[2].setRange(0.0, 300.0, 1.0);

      paramSliders[2].setValue(slot.vss3Reverb.getPreDelay(),

                               juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" ms");

      paramSliders[2].onValueChange = [&slot, this] {
        slot.vss3Reverb.setPreDelay(paramSliders[2].getValue());
      };

      paramLabels[3].setText("Hi-Cut Damping (Hz):",

                             juce::dontSendNotification);

      paramSliders[3].setRange(500.0, 20000.0, 50.0);

      paramSliders[3].setValue(slot.vss3Reverb.getHiCut(),

                               juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix(" Hz");

      paramSliders[3].onValueChange = [&slot, this] {
        slot.vss3Reverb.setHiCut(paramSliders[3].getValue());
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(slot.vss3Reverb.getMix() * 100.0,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        slot.vss3Reverb.setMix(paramSliders[4].getValue() * 0.01);
      };

      toggleParam.setVisible(false);

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN TC VSS3 REVERB EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff0284c7));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Convolution_Reverb) {

      const auto p = slot.convReverb.getParams();

      paramLabels[0].setText("Decay Scale:", juce::dontSendNotification);

      paramSliders[0].setRange(0.2, 2.0, 0.05);

      paramSliders[0].setValue(p.decayScale, juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix("x");

      paramSliders[0].onValueChange = [&slot, this] {
        auto params = slot.convReverb.getParams();
        params.decayScale = static_cast<float>(paramSliders[0].getValue());
        slot.convReverb.setParams(params);
      };

      paramLabels[1].setText("Pre-Delay (ms):", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 200.0, 1.0);

      paramSliders[1].setValue(p.preDelayMs, juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        auto params = slot.convReverb.getParams();
        params.preDelayMs = static_cast<float>(paramSliders[1].getValue());
        slot.convReverb.setParams(params);
      };

      paramLabels[2].setText("Damping Freq (Hz):",

                             juce::dontSendNotification);

      paramSliders[2].setRange(1000.0, 20000.0, 50.0);

      paramSliders[2].setValue(p.damping, juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix(" Hz");

      paramSliders[2].onValueChange = [&slot, this] {
        auto params = slot.convReverb.getParams();
        params.damping = static_cast<float>(paramSliders[2].getValue());
        slot.convReverb.setParams(params);
      };

      paramLabels[3].setText("Stereo Width:", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 2.0, 0.05);

      paramSliders[3].setValue(p.stereoWidth, juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix("x");

      paramSliders[3].onValueChange = [&slot, this] {
        auto params = slot.convReverb.getParams();
        params.stereoWidth = static_cast<float>(paramSliders[3].getValue());
        slot.convReverb.setParams(params);
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(p.dryWet * 100.0f,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        auto params = slot.convReverb.getParams();
        params.dryWet = static_cast<float>(paramSliders[4].getValue() * 0.01);
        slot.convReverb.setParams(params);
      };

      toggleParam.setVisible(false);

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN CONVOLUTION REVERB EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff0369a1));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else if (t == nova::graviton::FxType::Hybrid_Reverb) {

      const auto p = slot.hybridReverb.getParams();

      paramLabels[0].setText("Reverb Decay (s):", juce::dontSendNotification);

      paramSliders[0].setRange(0.5, 15.0, 0.1);

      paramSliders[0].setValue(p.decay, juce::dontSendNotification);

      paramSliders[0].setTextValueSuffix(" s");

      paramSliders[0].onValueChange = [&slot, this] {
        auto params = slot.hybridReverb.getParams();
        params.decay = static_cast<float>(paramSliders[0].getValue());
        slot.hybridReverb.setParams(params);
      };

      paramLabels[1].setText("Pre-Delay (ms):", juce::dontSendNotification);

      paramSliders[1].setRange(0.0, 200.0, 1.0);

      paramSliders[1].setValue(p.preDelayMs, juce::dontSendNotification);

      paramSliders[1].setTextValueSuffix(" ms");

      paramSliders[1].onValueChange = [&slot, this] {
        auto params = slot.hybridReverb.getParams();
        params.preDelayMs = static_cast<float>(paramSliders[1].getValue());
        slot.hybridReverb.setParams(params);
      };

      paramLabels[2].setText("Room Size:", juce::dontSendNotification);

      paramSliders[2].setRange(0.2, 2.0, 0.05);

      paramSliders[2].setValue(p.size, juce::dontSendNotification);

      paramSliders[2].setTextValueSuffix("x");

      paramSliders[2].onValueChange = [&slot, this] {
        auto params = slot.hybridReverb.getParams();
        params.size = static_cast<float>(paramSliders[2].getValue());
        slot.hybridReverb.setParams(params);
      };

      paramLabels[3].setText("High Damping:", juce::dontSendNotification);

      paramSliders[3].setRange(0.0, 1.0, 0.05);

      paramSliders[3].setValue(p.highDamping, juce::dontSendNotification);

      paramSliders[3].setTextValueSuffix("");

      paramSliders[3].onValueChange = [&slot, this] {
        auto params = slot.hybridReverb.getParams();
        params.highDamping = static_cast<float>(paramSliders[3].getValue());
        slot.hybridReverb.setParams(params);
      };

      paramLabels[4].setText("Wet Mix (%):", juce::dontSendNotification);

      paramSliders[4].setRange(0.0, 100.0, 1.0);

      paramSliders[4].setValue(p.dryWet * 100.0f,

                               juce::dontSendNotification);

      paramSliders[4].setTextValueSuffix(" %");

      paramSliders[4].onValueChange = [&slot, this] {
        auto params = slot.hybridReverb.getParams();
        params.dryWet = static_cast<float>(paramSliders[4].getValue() * 0.01);
        slot.hybridReverb.setParams(params);
      };

      toggleParam.setVisible(false);

      openEditorButton.setVisible(true);
      openEditorButton.setButtonText("OPEN HYBRID REVERB EDITOR");
      openEditorButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xffd97706));
      openEditorButton.onClick = [this] {
        if (onSlotDoubleClicked)
          onSlotDoubleClicked(selectedSlot);
        else
          openDetailView(selectedSlot);
      };

    } else {

      for (int i = 0; i < 5; ++i) {

        paramLabels[static_cast<size_t>(i)].setText("---",

                                                    juce::dontSendNotification);

        paramSliders[static_cast<size_t>(i)].setRange(0.0, 1.0, 1.0);

        paramSliders[static_cast<size_t>(i)].setValue(

            0.0, juce::dontSendNotification);
      }

      toggleParam.setVisible(false);
    }
  }

  void timerCallback() override {

    for (int i = 0; i < 24; ++i) {

      if (auto &tile = slotTiles[static_cast<size_t>(i)]) {

        const auto &slot = engine.effectsRack.getSlot(static_cast<size_t>(i));

        const auto t = static_cast<nova::graviton::FxType>(
            slot.type.load(std::memory_order_relaxed));

        const int ins = slot.insertChannel.load(std::memory_order_relaxed);

        const int aux = slot.auxSendSource.load(std::memory_order_relaxed);

        tile->name = getFxTypeName(t);

        tile->categoryTag = getFxCategoryTag(t);

        tile->purposeText = getFxPurposeText(t);

        tile->category = getFxCategory(t);

        tile->categoryColour = getFxTypeColour(t);

        tile->isDimmed = (activeCategoryFilter != FxCategory::All &&
                          tile->category != activeCategoryFilter);

        tile->isBypassed = slot.bypassed.load(std::memory_order_relaxed);

        tile->isSelected = (i == selectedSlot);

        tile->isChannelInsert = (ins == selectedChannel);

        if (ins == selectedChannel) {

          tile->routingBadge = "INS: CH " + juce::String(selectedChannel + 1);

        } else if (ins == nova::graviton::EffectsRack::kMasterInsertChannel) {

          tile->routingBadge = "INS: MASTER";

        } else if (ins >= 0) {

          tile->routingBadge = "INS: CH " + juce::String(ins + 1);

        } else if (aux >= 0) {

          tile->routingBadge = "AUX " + juce::String(aux + 1);

        } else {

          tile->routingBadge = "DIRECT";
        }

        tile->inPeak = slot.inputPeak.load(std::memory_order_relaxed);

        tile->outPeak = slot.outputPeak.load(std::memory_order_relaxed);

        tile->repaint();
      }
    }

    const auto &selSlot =

        engine.effectsRack.getSlot(static_cast<size_t>(selectedSlot));

    const auto selType = static_cast<nova::graviton::FxType>(

        selSlot.type.load(std::memory_order_relaxed));

    if (selType == nova::graviton::FxType::MasterX_Multiband) {

      const float grLow = selSlot.masterX.getBandGainReductionDb(0);

      const float grMid = selSlot.masterX.getBandGainReductionDb(1);

      const float grHi = selSlot.masterX.getBandGainReductionDb(2);

      const float grLim = selSlot.masterX.getLimiterGainReductionDb();

      slotTitle.setText("SLOT " + juce::String(selectedSlot + 1) +

                            " \xe2\x80\x94 TC Master X HD | GR: Low -" +

                            juce::String(grLow, 1) + " Mid -" +

                            juce::String(grMid, 1) + " High -" +

                            juce::String(grHi, 1) + " Lim -" +

                            juce::String(grLim, 1) + " dB",

                        juce::dontSendNotification);

    } else if (selType == nova::graviton::FxType::SubHarmonic_Synth) {

      const bool locked = selSlot.subHarmonic.isLocked();

      slotTitle.setText(

          "SLOT " + juce::String(selectedSlot + 1) +

              " \xe2\x80\x94 Sub-Harmonic Synthesizer | " +

              (locked ? "SUB OCTAVE LOCK " +

                            juce::String(

                                selSlot.subHarmonic.getSubFrequencyHz(), 1) +

                            " Hz"

                      : juce::String("SEARCHING...")) +

              " | SPLIT " +

              juce::String(selSlot.subHarmonic.getSplitFrequencyHz(), 0) +

              " Hz",

          juce::dontSendNotification);

    } else if (selType == nova::graviton::FxType::DN360_InductorGEQ) {

      const size_t band = static_cast<size_t>(juce::jlimit(

          0, static_cast<int>(nova::graviton::DN360InductorGEQ::kNumBands) - 1,

          selectedEqBand[static_cast<size_t>(selectedSlot)]));

      slotTitle.setText(

          "SLOT " + juce::String(selectedSlot + 1) +

              " \xe2\x80\x94 DN360 Inductor GEQ | BAND " +

              juce::String(band + 1) + " (" +

              juce::String(

                  nova::graviton::DN360InductorGEQ::kBandFrequencies[band], 0) +

              " Hz) " + juce::String(selSlot.dn360.getBandGain(band), 1) +

              " dB",

          juce::dontSendNotification);

    } else if (selType == nova::graviton::FxType::Precision_DeEsser) {

      const bool split = selSlot.deEsser.getMode() ==

                         nova::graviton::PrecisionDeEsser::Mode::Split;

      slotTitle.setText(

          "SLOT " + juce::String(selectedSlot + 1) +

              " \xe2\x80\x94 Precision De-Esser | GR: -" +

              juce::String(selSlot.deEsser.getGainReductionDb(), 1) + " dB | " +

              (split ? "SPLIT BAND" : "BROADBAND") + " @ " +

              juce::String(selSlot.deEsser.getFrequencyHz(), 0) + " Hz",

          juce::dontSendNotification);

    } else if (selType == nova::graviton::FxType::KT_1176_Limiter) {

      slotTitle.setText(

          "SLOT " + juce::String(selectedSlot + 1) +

              " \xe2\x80\x94 Klark Teknik 1176 LN | GR: -" +

              juce::String(selSlot.kt1176.getGainReductionDb(), 1) + " dB",

          juce::dontSendNotification);

    } else if (selType == nova::graviton::FxType::KT_BusCompressor) {

      slotTitle.setText(

          "SLOT " + juce::String(selectedSlot + 1) +

              " \xe2\x80\x94 Klark Teknik Bus Compressor | GR: -" +

              juce::String(selSlot.busComp.getGainReductionDb(), 1) + " dB",

          juce::dontSendNotification);
    }
  }

  void paint(juce::Graphics &g) override {

    g.fillAll(juce::Colour(0xff0e141d));

    g.setColour(juce::Colour(0xff1e293b));

    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {

    auto area = getLocalBounds().reduced(6);

    if (isDetailViewActive) {
      gridViewport.setVisible(false);

      for (auto &sl : paramSliders)
        sl.setVisible(true);

      // Hide all 24 grid tiles and full editor button

      for (auto &tile : slotTiles) {

        if (tile)
          tile->setVisible(false);
      }

      btnEditFullScreen.setVisible(false);

      btnBackFromDetail.setVisible(true);

      toggleParam.setVisible(true);

      openEditorButton.setVisible(true);

      for (auto &lbl : paramLabels)
        lbl.setVisible(true);

      for (auto &btn : categoryFilterButtons)
        btn.setVisible(false);

      auto topRow = area.removeFromTop(28);
      const int topW = topRow.getWidth();
      const int backW = juce::jmin(200, static_cast<int>(topW * 0.28f));
      const int bypassW = juce::jmin(75, static_cast<int>(topW * 0.12f));
      const int comboW = juce::jmin(180, static_cast<int>(topW * 0.26f));

      btnBackFromDetail.setBounds(topRow.removeFromLeft(backW).reduced(2));
      bypassButton.setBounds(topRow.removeFromRight(bypassW).reduced(2));
      typeCombo.setBounds(topRow.removeFromRight(comboW).reduced(2));
      slotTitle.setBounds(topRow.reduced(2));

      area.removeFromTop(6);

      auto routeRow = area.removeFromTop(28);
      const int rw = routeRow.getWidth();
      lblSelectedChannel.setBounds(
          routeRow.removeFromLeft(juce::jmin(90, rw / 9)).reduced(2));
      btnInsertChannel.setBounds(
          routeRow.removeFromLeft(juce::jmin(130, rw / 6)).reduced(2));
      btnInsertMaster.setBounds(
          routeRow.removeFromLeft(juce::jmin(120, rw / 6)).reduced(2));
      btnInsertNone.setBounds(
          routeRow.removeFromLeft(juce::jmin(85, rw / 9)).reduced(2));
      auxSendCombo.setBounds(
          routeRow.removeFromLeft(juce::jmin(130, rw / 6)).reduced(2));
      returnMuteButton.setBounds(
          routeRow.removeFromRight(juce::jmin(75, rw / 9)).reduced(2));
      returnLevelSlider.setBounds(routeRow.reduced(2));

      area.removeFromTop(4);

      for (int i = 0; i < 5; ++i) {

        auto row = area.removeFromTop(24);

        paramLabels[static_cast<size_t>(i)].setBounds(
            row.removeFromLeft(130).reduced(2));

        paramSliders[static_cast<size_t>(i)].setBounds(row.reduced(2));
      }

      area.removeFromTop(4);

      auto btnRow = area.removeFromTop(26);

      toggleParam.setBounds(btnRow.removeFromLeft(200).reduced(2));

      openEditorButton.setBounds(btnRow.removeFromLeft(220).reduced(2));

      return;
    }

    // Normal 24-Rack Grid Mode: Ensure detail widgets are hidden so they never
    // overlap!
    gridViewport.setVisible(true);

    for (auto &tile : slotTiles) {

      if (tile)
        tile->setVisible(true);
    }

    btnBackFromDetail.setVisible(false);

    toggleParam.setVisible(false);

    openEditorButton.setVisible(false);

    for (auto &lbl : paramLabels)
      lbl.setVisible(false);

    btnEditFullScreen.setVisible(true);

    // Responsive Header Bar with Category Group Filter Pills
    const int availW = area.getWidth();
    if (availW < 960) {
      auto topHeader = area.removeFromTop(22);
      lblSelectedChannel.setBounds(
          topHeader.removeFromRight(juce::jmin(110, availW / 4)));
      title.setBounds(topHeader);

      auto filterRow = area.removeFromTop(24).reduced(2, 1);
      const int filterW = filterRow.getWidth() / 6;
      for (int f = 0; f < 6; ++f) {
        auto &btn = categoryFilterButtons[static_cast<size_t>(f)];
        btn.setVisible(true);
        btn.setBounds(filterRow.removeFromLeft(filterW).reduced(1, 0));
      }
      area.removeFromTop(2);
    } else {
      auto topHeader = area.removeFromTop(24);
      title.setBounds(
          topHeader.removeFromLeft(juce::jmin(320, availW / 3)));
      lblSelectedChannel.setBounds(topHeader.removeFromRight(115));

      auto filterArea = topHeader.reduced(4, 1);
      const int filterW = juce::jmin(92, filterArea.getWidth() / 6);
      for (int f = 0; f < 6; ++f) {
        auto &btn = categoryFilterButtons[static_cast<size_t>(f)];
        btn.setVisible(true);
        btn.setBounds(filterArea.removeFromLeft(filterW).reduced(1, 0));
      }
    }

    // Responsive Bottom Inspector Bar (1 row on wide desktop, 2 compact rows
    // on embedded screens so nothing overlaps or clips)
    for (auto &sl : paramSliders)
      sl.setVisible(false);

    if (availW >= 1260) {
      auto bottomBar = area.removeFromBottom(30).reduced(2);
      slotTitle.setBounds(bottomBar.removeFromLeft(180).reduced(2));
      typeCombo.setBounds(bottomBar.removeFromLeft(180).reduced(2));
      bypassButton.setBounds(bottomBar.removeFromLeft(68).reduced(2));
      btnInsertChannel.setBounds(bottomBar.removeFromLeft(125).reduced(2));
      btnInsertMaster.setBounds(bottomBar.removeFromLeft(115).reduced(2));
      btnInsertNone.setBounds(bottomBar.removeFromLeft(75).reduced(2));
      auxSendCombo.setBounds(bottomBar.removeFromLeft(125).reduced(2));
      returnLevelSlider.setBounds(bottomBar.removeFromLeft(140).reduced(2));
      returnMuteButton.setBounds(bottomBar.removeFromLeft(72).reduced(2));
      btnEditFullScreen.setBounds(bottomBar.removeFromRight(200).reduced(2));
    } else {
      auto inspector = area.removeFromBottom(54).reduced(2, 0);
      auto row1 = inspector.removeFromTop(26);
      const int w1 = row1.getWidth();
      slotTitle.setBounds(
          row1.removeFromLeft(juce::jmin(155, static_cast<int>(w1 * 0.23f)))
              .reduced(2, 1));
      btnEditFullScreen.setBounds(
          row1.removeFromRight(juce::jmin(180, static_cast<int>(w1 * 0.25f)))
              .reduced(2, 1));
      bypassButton.setBounds(
          row1.removeFromRight(juce::jmin(68, static_cast<int>(w1 * 0.11f)))
              .reduced(2, 1));
      typeCombo.setBounds(row1.reduced(2, 1));

      inspector.removeFromTop(2);
      auto row2 = inspector.removeFromTop(26);
      const int w2 = row2.getWidth();
      btnInsertChannel.setBounds(
          row2.removeFromLeft(static_cast<int>(w2 * 0.19f)).reduced(2, 1));
      btnInsertMaster.setBounds(
          row2.removeFromLeft(static_cast<int>(w2 * 0.17f)).reduced(2, 1));
      btnInsertNone.setBounds(
          row2.removeFromLeft(static_cast<int>(w2 * 0.11f)).reduced(2, 1));
      auxSendCombo.setBounds(
          row2.removeFromLeft(static_cast<int>(w2 * 0.19f)).reduced(2, 1));
      returnMuteButton.setBounds(
          row2.removeFromRight(juce::jmin(68, static_cast<int>(w2 * 0.11f)))
              .reduced(2, 1));
      returnLevelSlider.setBounds(row2.reduced(2, 1));
    }

    area.removeFromBottom(4);

    // 24-Slot Grid inside scrollable Viewport for embedded screens:
    // Use 3x8 on compact-height embedded screens, 4x6 on taller screens, with
    // minimum readable tile size and touch-drag / wheel sliding.
    const int rows = (area.getHeight() < 260) ? 3 : 4;
    const int cols = 24 / rows;
    constexpr int minTileW = 158;
    constexpr int minTileH = 66;

    const int contentW = juce::jmax(area.getWidth(), cols * minTileW);
    const int contentH = juce::jmax(area.getHeight(), rows * minTileH);

    gridViewport.setBounds(area);
    gridContent.setSize(contentW, contentH);

    const int cellW = contentW / cols;
    const int cellH = contentH / rows;

    for (int i = 0; i < 24; ++i) {

      const int c = i % cols;

      const int r = i / cols;

      if (slotTiles[static_cast<size_t>(i)]) {

        slotTiles[static_cast<size_t>(i)]->setBounds(
            c * cellW + 2, r * cellH + 2, cellW - 4, cellH - 4);
      }
    }
  }

private:
  MixerEngine &engine;

  int selectedSlot = 0;

  int selectedChannel = 0;

  bool isDetailViewActive = false;

  int masterXEditTab = 0;

  juce::Label title, slotTitle, lblSelectedChannel;

  juce::Viewport gridViewport;
  juce::Component gridContent;

  std::array<std::unique_ptr<FxSlotTile>, 24> slotTiles;

  juce::ComboBox typeCombo, auxSendCombo;

  juce::TextButton bypassButton, returnMuteButton, toggleParam,
      openEditorButton;

  juce::TextButton btnInsertChannel, btnInsertMaster, btnInsertNone,
      btnEditFullScreen, btnBackFromDetail;

  juce::Slider returnLevelSlider;

  std::array<juce::Label, 5> paramLabels;

  std::array<juce::Slider, 5> paramSliders;

  void setCategoryFilter(FxCategory cat) {
    activeCategoryFilter = cat;
    static const struct {
      FxCategory cat;
      juce::uint32 col;
    } kCols[6] = {
        {FxCategory::All, 0xff94a3b8},
        {FxCategory::Reverb, 0xff38bdf8},
        {FxCategory::Delay, 0xfffacc15},
        {FxCategory::Dynamics, 0xfff87171},
        {FxCategory::ModPitch, 0xff818cf8},
        {FxCategory::ToneMaster, 0xfffb923c},
    };
    for (int f = 0; f < 6; ++f) {
      auto &btn = categoryFilterButtons[static_cast<size_t>(f)];
      const bool active = (kCols[f].cat == activeCategoryFilter);
      const juce::Colour c(kCols[f].col);
      btn.setColour(juce::TextButton::buttonColourId,
                    active ? c.withAlpha(0.28f) : juce::Colour(0xff101722));
      btn.setColour(juce::TextButton::textColourOffId,
                    active ? c : juce::Colour(0xff94a3b8));
    }
    repaint();
  }

  FxCategory activeCategoryFilter = FxCategory::All;

  std::array<juce::TextButton, 6> categoryFilterButtons;

  // Per-slot working band for the DN360 30-band slot editor (0..29).

  std::array<int, 24> selectedEqBand{};
};

} // namespace nova
