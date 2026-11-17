#pragma once

#include "GUI/AKGBX20Panel.h"
#include "GUI/DVR2Panel.h"
#include "GUI/EMT140Panel.h"
#include "GUI/EffectsRackPanel.h"
#include "GUI/FlangerBL20Panel.h"
#include "GUI/GBusCompressorPanel.h"
#include "GUI/KT1176Panel.h"
#include "GUI/ManipulatorPitchPanel.h"
#include "GUI/MasterXPanel.h"
#include "GUI/NebulaDelayPanel.h"
#include "GUI/OpticalPhaserPanel.h"
#include "GUI/PrecisionDeEsserPanel.h"
#include "GUI/TC2290Panel.h"
#include "GUI/NonLin2Panel.h"
#include "GUI/TCSCFPanel.h"
#include "GUI/VSS4Panel.h"
#include "GUI/VocalPitchPanel.h"
#include "GUI/RackDynamicsPanel.h"
#include "GUI/SubHarmonicPanel.h"
#include "GUI/DN360Panel.h"
#include "GUI/DistortionPanel.h"
#include "GUI/ConvolutionReverbPanel.h"
#include "GUI/DN780Panel.h"
#include "GUI/HybridReverbPanel.h"
#include "GUI/VSS3Panel.h"
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class EmbeddedFxHostPanel : public juce::Component {
public:
  explicit EmbeddedFxHostPanel(MixerEngine &e)
      : engine(e), rackPanel(e), vss4Panel(e.effectsRack.getSlot(0).vss4Reverb),
        nebulaPanel(e.effectsRack.getSlot(1).nebulaDelay),
        emtPanel(e.effectsRack.getSlot(2).plate),
        masterXPanel(e.effectsRack.getSlot(4).masterX),
        akgPanel(e.effectsRack.getSlot(3).spring),
        pitchPanel(e.effectsRack.getSlot(5).manipulator),
        dvr2Panel(e.effectsRack.getSlot(13).dvr2),
        kt1176Panel(e.effectsRack.getSlot(6).kt1176),
        gBusPanel(e.effectsRack.getSlot(7).busComp),
        deEsserPanel(e.effectsRack.getSlot(8).deEsser),
        scfPanel(e.effectsRack.getSlot(9).scf),
        flangerBL20Panel(e.effectsRack.getSlot(10).flanger),
        opticalPhaserPanel(e.effectsRack.getSlot(11).phaser),
        tc2290Panel(&e.effectsRack.getSlot(12).tc2290),
        nonLin2Panel(&e.effectsRack.getSlot(14).nonLin2),
        vocalPitchPanel(e.effectsRack.getSlot(19).vocalPitch),
        rackDynamicsPanel(e.effectsRack.getSlot(23).rackDynamics),
        subHarmonicPanel(e.effectsRack.getSlot(16).subHarmonic),
        dn360Panel(e.effectsRack.getSlot(17).dn360),
        distortionPanel(e.effectsRack.getSlot(18).distortion),
        convReverbPanel(e.effectsRack.getSlot(20).convReverb),
        dn780Panel(e.effectsRack.getSlot(15).plate),
        hybridReverbPanel(e.effectsRack.getSlot(21).hybridReverb),
        vss3Panel(e.effectsRack.getSlot(22).vss3Reverb) {
    addAndMakeVisible(rackPanel);
    addChildComponent(vss4Panel);
    addChildComponent(nebulaPanel);
    addChildComponent(emtPanel);
    addChildComponent(masterXPanel);
    addChildComponent(akgPanel);
    addChildComponent(pitchPanel);
    addChildComponent(dvr2Panel);
    addChildComponent(kt1176Panel);
    addChildComponent(gBusPanel);
    addChildComponent(deEsserPanel);
    addChildComponent(scfPanel);
    addChildComponent(flangerBL20Panel);
    addChildComponent(opticalPhaserPanel);
    addChildComponent(tc2290Panel);
    addChildComponent(nonLin2Panel);
    addChildComponent(vocalPitchPanel);
    addChildComponent(rackDynamicsPanel);
    addChildComponent(subHarmonicPanel);
    addChildComponent(dn360Panel);
    addChildComponent(distortionPanel);
    addChildComponent(convReverbPanel);
    addChildComponent(dn780Panel);
    addChildComponent(hybridReverbPanel);
    addChildComponent(vss3Panel);

    btnBackToRack.setButtonText("<- BACK TO 24-SLOT RACK");
    btnBackToRack.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff1f6feb));
    btnBackToRack.setColour(juce::TextButton::textColourOffId,
                            juce::Colours::white);
    btnBackToRack.onClick = [this] { showSubPage(0); };
    addChildComponent(btnBackToRack);

    lblEditorTitle.setFont(
        juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    lblEditorTitle.setColour(juce::Label::textColourId,
                             juce::Colour(0xfff0f3f6));
    addChildComponent(lblEditorTitle);

    // Double-click on any of the 24 slots -> directly opens dedicated editor on
    // embedded screen!
    rackPanel.onSlotDoubleClicked = [this](int slotIdx) {
      openSlotEditor(slotIdx);
    };
  }

  std::function<void(int)> onSubPageChanged;

  void setSelectedChannel(int ch) {
    selectedChannel = ch;
    rackPanel.setSelectedChannel(ch);
  }

  void openSlotEditor(int slotIdx) {
    const auto &slot = engine.effectsRack.getSlot(static_cast<size_t>(slotIdx));
    const auto t = static_cast<nova::graviton::FxType>(
        slot.type.load(std::memory_order_relaxed));

    rackPanel.setSelectedSlot(slotIdx);

    if (t == nova::graviton::FxType::Reverb_VSS3 || slotIdx == 22) {
      vss3Panel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).vss3Reverb);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - TC ELECTRONIC VSS3 STEREO REVERB",
                             juce::dontSendNotification);
      showSubPage(23);
    } else if (t == nova::graviton::FxType::Hybrid_Reverb || slotIdx == 21) {
      hybridReverbPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).hybridReverb);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - HYBRID REVERB (IR + ALGORITHMIC)",
                             juce::dontSendNotification);
      showSubPage(22);
    } else if (t == nova::graviton::FxType::Convolution_Reverb || slotIdx == 20) {
      convReverbPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).convReverb);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - TRUE IR CONVOLUTION REVERB",
                             juce::dontSendNotification);
      showSubPage(21);
    } else if (t == nova::graviton::FxType::Distortion || slotIdx == 18) {
      distortionPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).distortion);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA HD SATURATOR & DISTORTION",
                             juce::dontSendNotification);
      showSubPage(19);
    } else if (t == nova::graviton::FxType::DN360_InductorGEQ || slotIdx == 17) {
      dn360Panel.setEngine(const_cast<nova::graviton::FxSlot &>(slot).dn360);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - KLARK TEKNIK DN360 30-BAND GRAPHIC EQ",
                             juce::dontSendNotification);
      showSubPage(18);
    } else if (t == nova::graviton::FxType::SubHarmonic_Synth || slotIdx == 16) {
      subHarmonicPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).subHarmonic);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA HD SUBHARMONIC SYNTH (bx_subsynth)",
                             juce::dontSendNotification);
      showSubPage(17);
    } else if (t == nova::graviton::FxType::Compressor ||
        t == nova::graviton::FxType::NoiseGate || slotIdx == 23) {
      auto &dyn = const_cast<nova::graviton::FxSlot &>(slot).rackDynamics;
      if (t == nova::graviton::FxType::NoiseGate)
        dyn.setMode(1);
      rackDynamicsPanel.setEngine(dyn);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA HD RACK DYNAMICS (7-CIRCUIT + DIE-PLUGINS)",
                             juce::dontSendNotification);
      showSubPage(24);
    } else if (t == nova::graviton::FxType::Vocal_Pitch_Corrector || slotIdx == 19) {
      vocalPitchPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).vocalPitch);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA EVO VOCAL PITCH CORRECTOR",
                             juce::dontSendNotification);
      showSubPage(20);
    } else if (t == nova::graviton::FxType::TC_NonLin2 || slotIdx == 14) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - TC SYSTEM 6000 NONLIN2 ENVELOPE REVERB",
                             juce::dontSendNotification);
      showSubPage(15);
    } else if (t == nova::graviton::FxType::DVR2_NonLinear || slotIdx == 13) {
      dvr2Panel.setEngine(const_cast<nova::graviton::FxSlot &>(slot).dvr2);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - TC SYSTEM 6000 DVR2 VINTAGE REVERB",
                             juce::dontSendNotification);
      showSubPage(14);
    } else if (t == nova::graviton::FxType::Reverb_VSS4 || slotIdx == 0) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - TC ELECTRONIC VSS4 REVERB HD",
                             juce::dontSendNotification);
      showSubPage(1);
    } else if (t == nova::graviton::FxType::TC_2290_Delay || slotIdx == 12) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA HD 2290 DYNAMIC DIGITAL DELAY",
                             juce::dontSendNotification);
      showSubPage(13);
    } else if (t == nova::graviton::FxType::StereoDelay || slotIdx == 1) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - STEREO DELAY ENGINE",
                             juce::dontSendNotification);
      showSubPage(2);
    } else if (t == nova::graviton::FxType::KT_DN780 || slotIdx == 15) {
      dn780Panel.setEngine(const_cast<nova::graviton::FxSlot &>(slot).plate);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - KLARK TEKNIK DN780 DIGITAL REVERB",
                             juce::dontSendNotification);
      showSubPage(16);
    } else if (t == nova::graviton::FxType::EMT140_Plate || slotIdx == 2) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - EMT 140 VINTAGE PLATE REVERB",
                             juce::dontSendNotification);
      showSubPage(3);
    } else if (t == nova::graviton::FxType::MasterX_Multiband || slotIdx == 4) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - TC MASTER X HD SYSTEM 6000",
                             juce::dontSendNotification);
      showSubPage(5);
    } else if (t == nova::graviton::FxType::SpringReverb ||
               t == nova::graviton::FxType::DualTank_Spring || slotIdx == 3) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - AKG BX20 VINTAGE SPRING REVERB",
                             juce::dontSendNotification);
      showSubPage(4);
    } else if (t == nova::graviton::FxType::PitchShifter || slotIdx == 5) {
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - MANIPULATOR PITCH SHIFTER & ADT",
                             juce::dontSendNotification);
      showSubPage(6);
    } else if (t == nova::graviton::FxType::KT_1176_Limiter || slotIdx == 6) {
      kt1176Panel.setEngine(const_cast<nova::graviton::FxSlot &>(slot).kt1176);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - 1176LN FET LIMITING AMPLIFIER",
                             juce::dontSendNotification);
      showSubPage(7);
    } else if (t == nova::graviton::FxType::KT_BusCompressor || slotIdx == 7) {
      gBusPanel.setEngine(const_cast<nova::graviton::FxSlot &>(slot).busComp);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA HD G-BUS STEREO VCA COMPRESSOR",
                             juce::dontSendNotification);
      showSubPage(8);
    } else if (t == nova::graviton::FxType::Precision_DeEsser || slotIdx == 8) {
      deEsserPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).deEsser);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(slotIdx + 1) +
                                 " - NOVA HD PRECISION DE-ESSER",
                             juce::dontSendNotification);
      showSubPage(9);
    } else if (t == nova::graviton::FxType::TC_SCF || slotIdx == 9) {
      scfPanel.setEngine(const_cast<nova::graviton::FxSlot &>(slot).scf);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(slotIdx + 1) +
              " - TC ELECTRONIC SCF GOLD STEREO CHORUS / FLANGER",
          juce::dontSendNotification);
      showSubPage(10);
    } else if (t == nova::graviton::FxType::Flanger || slotIdx == 10) {
      flangerBL20Panel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).flanger);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(slotIdx + 1) +
              " - NOVA HD FLANGER BL-20 STEREO BBD & FUNCTION GENERATOR",
          juce::dontSendNotification);
      showSubPage(11);
    } else if (t == nova::graviton::FxType::Phaser || slotIdx == 11) {
      opticalPhaserPanel.setEngine(
          const_cast<nova::graviton::FxSlot &>(slot).phaser);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(slotIdx + 1) +
              " - NOVA HD 12-STAGE OPTICAL STEREO PHASER (BENIDUB PHASE)",
          juce::dontSendNotification);
      showSubPage(12);
    } else {
      // For any other slot, rackPanel displays its dedicated focused full
      // editor embedded
      rackPanel.openDetailView(slotIdx);
      showSubPage(0);
    }
  }

  void showSubPage(int pageIdx) {
    activePage = juce::jlimit(0, 24, pageIdx);
    auto findSlot = [this](nova::graviton::FxType fxType, size_t fallback) {
      for (size_t s = 0; s < 24; ++s) {
        if (engine.effectsRack.getSlot(s).type.load(
                std::memory_order_relaxed) == static_cast<int>(fxType))
          return s;
      }
      return fallback;
    };

    if (activePage == 1) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Reverb_VSS4, 0);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - TC ELECTRONIC VSS4 REVERB HD",
                             juce::dontSendNotification);
    } else if (activePage == 2) {
      size_t targetSlot = findSlot(nova::graviton::FxType::StereoDelay, 1);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - STEREO DELAY ENGINE",
                             juce::dontSendNotification);
    } else if (activePage == 3) {
      size_t targetSlot = findSlot(nova::graviton::FxType::EMT140_Plate, 2);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - EMT 140 VINTAGE PLATE REVERB",
                             juce::dontSendNotification);
    } else if (activePage == 4) {
      size_t targetSlot = findSlot(nova::graviton::FxType::SpringReverb, 3);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - AKG BX20 VINTAGE SPRING REVERB",
                             juce::dontSendNotification);
    } else if (activePage == 5) {
      size_t targetSlot = findSlot(nova::graviton::FxType::MasterX_Multiband, 4);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - TC MASTER X HD SYSTEM 6000",
                             juce::dontSendNotification);
    } else if (activePage == 6) {
      size_t targetSlot = findSlot(nova::graviton::FxType::PitchShifter, 5);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - MANIPULATOR PITCH SHIFTER & ADT",
                             juce::dontSendNotification);
    } else if (activePage == 7) {
      size_t targetSlot = findSlot(nova::graviton::FxType::KT_1176_Limiter, 6);
      kt1176Panel.setEngine(engine.effectsRack.getSlot(targetSlot).kt1176);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - 1176LN FET LIMITING AMPLIFIER",
                             juce::dontSendNotification);
    } else if (activePage == 8) {
      size_t targetSlot = findSlot(nova::graviton::FxType::KT_BusCompressor, 7);
      gBusPanel.setEngine(engine.effectsRack.getSlot(targetSlot).busComp);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - NOVA HD G-BUS STEREO VCA COMPRESSOR",
                             juce::dontSendNotification);
    } else if (activePage == 9) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Precision_DeEsser, 8);
      deEsserPanel.setEngine(engine.effectsRack.getSlot(targetSlot).deEsser);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - NOVA HD PRECISION DE-ESSER",
                             juce::dontSendNotification);
    } else if (activePage == 10) {
      size_t targetSlot = findSlot(nova::graviton::FxType::TC_SCF, 9);
      scfPanel.setEngine(engine.effectsRack.getSlot(targetSlot).scf);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(targetSlot + 1) +
              " - TC ELECTRONIC SCF GOLD STEREO CHORUS / FLANGER",
          juce::dontSendNotification);
    } else if (activePage == 11) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Flanger, 10);
      flangerBL20Panel.setEngine(
          engine.effectsRack.getSlot(targetSlot).flanger);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(targetSlot + 1) +
              " - NOVA HD FLANGER BL-20 STEREO BBD & FUNCTION GENERATOR",
          juce::dontSendNotification);
    } else if (activePage == 12) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Phaser, 11);
      opticalPhaserPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).phaser);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(targetSlot + 1) +
              " - NOVA HD 12-STAGE OPTICAL STEREO PHASER (BENIDUB PHASE)",
          juce::dontSendNotification);
    } else if (activePage == 13) {
      size_t targetSlot = findSlot(nova::graviton::FxType::TC_2290_Delay, 12);
      tc2290Panel.setEngine(&engine.effectsRack.getSlot(targetSlot).tc2290);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(targetSlot + 1) +
              " - NOVA HD 2290 DYNAMIC DIGITAL DELAY (TC 2290)",
          juce::dontSendNotification);
    } else if (activePage == 14) {
      size_t targetSlot = findSlot(nova::graviton::FxType::DVR2_NonLinear, 13);
      dvr2Panel.setEngine(engine.effectsRack.getSlot(targetSlot).dvr2);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - TC SYSTEM 6000 DVR2 VINTAGE REVERB",
                             juce::dontSendNotification);
    } else if (activePage == 15) {
      size_t targetSlot = findSlot(nova::graviton::FxType::TC_NonLin2, 14);
      nonLin2Panel.setEngine(&engine.effectsRack.getSlot(targetSlot).nonLin2);
      lblEditorTitle.setText(
          "EDITING: SLOT " + juce::String(targetSlot + 1) +
              " - TC SYSTEM 6000 NONLIN2 ENVELOPE REVERB",
          juce::dontSendNotification);
    } else if (activePage == 16) {
      size_t targetSlot = findSlot(nova::graviton::FxType::KT_DN780, 15);
      dn780Panel.setEngine(engine.effectsRack.getSlot(targetSlot).plate);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - KLARK TEKNIK DN780 DIGITAL REVERB",
                             juce::dontSendNotification);
    } else if (activePage == 17) {
      size_t targetSlot = findSlot(nova::graviton::FxType::SubHarmonic_Synth, 16);
      subHarmonicPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).subHarmonic);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - NOVA HD SUBHARMONIC SYNTH (bx_subsynth)",
                             juce::dontSendNotification);
    } else if (activePage == 18) {
      size_t targetSlot = findSlot(nova::graviton::FxType::DN360_InductorGEQ, 17);
      dn360Panel.setEngine(engine.effectsRack.getSlot(targetSlot).dn360);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - KLARK TEKNIK DN360 30-BAND GRAPHIC EQ",
                             juce::dontSendNotification);
    } else if (activePage == 19) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Distortion, 18);
      distortionPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).distortion);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - NOVA HD SATURATOR & DISTORTION",
                             juce::dontSendNotification);
    } else if (activePage == 20) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Vocal_Pitch_Corrector, 19);
      vocalPitchPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).vocalPitch);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - NOVA EVO VOCAL PITCH CORRECTOR",
                             juce::dontSendNotification);
    } else if (activePage == 21) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Convolution_Reverb, 20);
      convReverbPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).convReverb);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - TRUE IR CONVOLUTION REVERB",
                             juce::dontSendNotification);
    } else if (activePage == 22) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Hybrid_Reverb, 21);
      hybridReverbPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).hybridReverb);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - HYBRID REVERB (IR + ALGORITHMIC)",
                             juce::dontSendNotification);
    } else if (activePage == 23) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Reverb_VSS3, 22);
      vss3Panel.setEngine(engine.effectsRack.getSlot(targetSlot).vss3Reverb);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - TC ELECTRONIC VSS3 STEREO REVERB",
                             juce::dontSendNotification);
    } else if (activePage == 24) {
      size_t targetSlot = findSlot(nova::graviton::FxType::Compressor, 23);
      rackDynamicsPanel.setEngine(
          engine.effectsRack.getSlot(targetSlot).rackDynamics);
      lblEditorTitle.setText("EDITING: SLOT " + juce::String(targetSlot + 1) +
                                 " - NOVA HD RACK DYNAMICS (7-CIRCUIT + DIE-PLUGINS)",
                             juce::dontSendNotification);
    }

    rackPanel.setVisible(activePage == 0);
    vss4Panel.setVisible(activePage == 1);
    nebulaPanel.setVisible(activePage == 2);
    emtPanel.setVisible(activePage == 3);
    akgPanel.setVisible(activePage == 4);
    masterXPanel.setVisible(activePage == 5);
    pitchPanel.setVisible(activePage == 6);
    kt1176Panel.setVisible(activePage == 7);
    gBusPanel.setVisible(activePage == 8);
    deEsserPanel.setVisible(activePage == 9);
    scfPanel.setVisible(activePage == 10);
    flangerBL20Panel.setVisible(activePage == 11);
    opticalPhaserPanel.setVisible(activePage == 12);
    tc2290Panel.setVisible(activePage == 13);
    dvr2Panel.setVisible(activePage == 14);
    nonLin2Panel.setVisible(activePage == 15);
    dn780Panel.setVisible(activePage == 16);
    subHarmonicPanel.setVisible(activePage == 17);
    dn360Panel.setVisible(activePage == 18);
    distortionPanel.setVisible(activePage == 19);
    vocalPitchPanel.setVisible(activePage == 20);
    convReverbPanel.setVisible(activePage == 21);
    hybridReverbPanel.setVisible(activePage == 22);
    vss3Panel.setVisible(activePage == 23);
    rackDynamicsPanel.setVisible(activePage == 24);

    const bool isDedicatedEditor = (activePage > 0);
    btnBackToRack.setVisible(isDedicatedEditor);
    lblEditorTitle.setVisible(isDedicatedEditor);

    if (activePage == 0) {
      rackPanel.closeDetailView();
    }

    if (onSubPageChanged)
      onSubPageChanged(activePage);

    resized();
    repaint();
  }

  void syncSummaryFromRack(float /*vss4Decay*/, float /*vss4PreDelay*/,
                           float /*delayMs*/, float /*plateDecay*/,
                           float /*masterXCeiling*/) {
    // Telemetry hook
  }

  void resized() override {
    auto b = getLocalBounds();
    if (activePage == 0) {
      rackPanel.setBounds(b);
    } else {
      auto topNav = b.removeFromTop(34);
      btnBackToRack.setBounds(topNav.removeFromLeft(210).reduced(2));
      lblEditorTitle.setBounds(topNav.reduced(4, 2));

      vss4Panel.setBounds(b);
      nebulaPanel.setBounds(b);
      emtPanel.setBounds(b);
      masterXPanel.setBounds(b);
      akgPanel.setBounds(b);
      pitchPanel.setBounds(b);
      dvr2Panel.setBounds(b);
      kt1176Panel.setBounds(b);
      gBusPanel.setBounds(b);
      deEsserPanel.setBounds(b);
      scfPanel.setBounds(b);
      flangerBL20Panel.setBounds(b);
      opticalPhaserPanel.setBounds(b);
      tc2290Panel.setBounds(b);
      nonLin2Panel.setBounds(b);
      vocalPitchPanel.setBounds(b);
      rackDynamicsPanel.setBounds(b);
      subHarmonicPanel.setBounds(b);
      dn360Panel.setBounds(b);
      distortionPanel.setBounds(b);
      convReverbPanel.setBounds(b);
      dn780Panel.setBounds(b);
      hybridReverbPanel.setBounds(b);
      vss3Panel.setBounds(b);
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff0d1117));
  }

private:
  MixerEngine &engine;
  EffectsRackPanel rackPanel;
  VSS4Panel vss4Panel;
  NebulaDelayPanel nebulaPanel;
  EMT140Panel emtPanel;
  MasterXPanel masterXPanel;
  AKGBX20Panel akgPanel;
  ManipulatorPitchPanel pitchPanel;
  DVR2Panel dvr2Panel;
  KT1176Panel kt1176Panel;
  GBusCompressorPanel gBusPanel;
  PrecisionDeEsserPanel deEsserPanel;
  TCSCFPanel scfPanel;
  FlangerBL20Panel flangerBL20Panel;
  OpticalPhaserPanel opticalPhaserPanel;
  TC2290Panel tc2290Panel;
  NonLin2Panel nonLin2Panel;
  VocalPitchPanel vocalPitchPanel;
  RackDynamicsPanel rackDynamicsPanel;
  SubHarmonicPanel subHarmonicPanel;
  DN360Panel dn360Panel;
  DistortionPanel distortionPanel;
  ConvolutionReverbPanel convReverbPanel;
  DN780Panel dn780Panel;
  HybridReverbPanel hybridReverbPanel;
  VSS3Panel vss3Panel;
  juce::TextButton btnBackToRack;
  juce::Label lblEditorTitle;
  int activePage = 0;
  int selectedChannel = 0;
};

} // namespace nova
