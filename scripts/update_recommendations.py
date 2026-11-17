file_path = "Source/DSP/GravitonChannelAI.h"
with open(file_path, "r", encoding="utf-8") as f:
    code = f.read()

# Update Kick Drum section
old_kick = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb + 2.0, -24.0, -12.0));
            rec.compRatio = 4.0f;
            rec.compAttackMs = 20.0f;
            rec.compReleaseMs = 100.0f;

        } else if (feat.lowEnergyRatio > 0.35 && feat.crestFactor <= 3.2 && feat.zcr < 0.07) {"""

new_kick = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb + 2.0, -24.0, -12.0));
            rec.compRatio = 4.0f;
            rec.compAttackMs = 20.0f;
            rec.compReleaseMs = 100.0f;
            rec.compMode = 0; // VCA Punch
            rec.fxSlot = -1;  // Dry
            rec.fxSendDb = -90.0f;
            rec.auxSendDb = { -12.0f, -6.0f, 0.0f, -12.0f };

        } else if (feat.lowEnergyRatio > 0.35 && feat.crestFactor <= 3.2 && feat.zcr < 0.07) {"""

assert old_kick in code
code = code.replace(old_kick, new_kick)

# Update Bass section
old_bass = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 2.0, -26.0, -14.0));
            rec.compRatio = 4.0f;
            rec.compAttackMs = 10.0f;
            rec.compReleaseMs = 220.0f;

        } else if (feat.crestFactor > 3.0 && (feat.energyHigh + feat.energyHiMid) > (feat.energyLow + feat.energyLoMid)) {"""

new_bass = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 2.0, -26.0, -14.0));
            rec.compRatio = 4.0f;
            rec.compAttackMs = 10.0f;
            rec.compReleaseMs = 220.0f;
            rec.compMode = 3; // Vari-Mu Bass Glue
            rec.fxSlot = -1;  // Dry
            rec.fxSendDb = -90.0f;
            rec.auxSendDb = { -8.0f, -3.0f, 0.0f, 0.0f };

        } else if (feat.crestFactor > 3.0 && (feat.energyHigh + feat.energyHiMid) > (feat.energyLow + feat.energyLoMid)) {"""

assert old_bass in code
code = code.replace(old_bass, new_bass)

# Update Snare section
old_snare = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb + 2.0, -22.0, -12.0));
            rec.compRatio = 3.5f;
            rec.compAttackMs = 15.0f;
            rec.compReleaseMs = 120.0f;

        } else if (feat.spectralCentroid >= 600.0 && feat.spectralCentroid <= 2200.0 && feat.zcr >= 0.04 && feat.zcr <= 0.16) {"""

new_snare = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb + 2.0, -22.0, -12.0));
            rec.compRatio = 3.5f;
            rec.compAttackMs = 15.0f;
            rec.compReleaseMs = 120.0f;
            rec.compMode = 2; // FET 76 Snap
            rec.fxSlot = 0;  // Plate Reverb
            rec.fxSendDb = -16.0f;
            rec.auxSendDb = { -10.0f, -4.0f, 0.0f, -8.0f };

        } else if (feat.spectralCentroid >= 600.0 && feat.spectralCentroid <= 2200.0 && feat.zcr >= 0.04 && feat.zcr <= 0.16) {"""

assert old_snare in code
code = code.replace(old_snare, new_snare)

# Update Vocal section
old_vocal = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 4.0, -26.0, -16.0));
            rec.compRatio = 2.8f;
            rec.compAttackMs = 15.0f;
            rec.compReleaseMs = 180.0f;

        } else if (feat.lowEnergyRatio < 0.25 && (feat.energyLoMid + feat.energyHiMid) > 0.65 * (feat.energyLow + feat.energyLoMid + feat.energyHiMid + feat.energyHigh)) {"""

new_vocal = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 4.0, -26.0, -16.0));
            rec.compRatio = 2.8f;
            rec.compAttackMs = 15.0f;
            rec.compReleaseMs = 180.0f;
            rec.compMode = 1; // Optical Tube Stability
            rec.fxSlot = 0;  // Vocal Plate Reverb
            rec.fxSendDb = -12.0f;
            rec.fxDecaySec = 2.4f;
            rec.auxSendDb = { 0.0f, -3.0f, -8.0f, -4.0f }; // Aux 1 Lead Wedge prioritized
            rec.afsActive = true;
            rec.afsFreqHz = 3150.0f;

        } else if (feat.lowEnergyRatio < 0.25 && (feat.energyLoMid + feat.energyHiMid) > 0.65 * (feat.energyLow + feat.energyLoMid + feat.energyHiMid + feat.energyHigh)) {"""

assert old_vocal in code
code = code.replace(old_vocal, new_vocal)

# Update Guitar section
old_guitar = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 2.0, -24.0, -14.0));
            rec.compRatio = 2.5f;
            rec.compAttackMs = 25.0f;
            rec.compReleaseMs = 150.0f;

        } else {"""

new_guitar = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 2.0, -24.0, -14.0));
            rec.compRatio = 2.5f;
            rec.compAttackMs = 25.0f;
            rec.compReleaseMs = 150.0f;
            rec.compMode = 0; // VCA Corrective
            rec.fxSlot = 2;  // Stereo Chorus / Delay
            rec.fxSendDb = -14.0f;
            rec.auxSendDb = { -6.0f, -2.0f, -6.0f, -4.0f };

        } else {"""

assert old_guitar in code
code = code.replace(old_guitar, new_guitar)

# Update Piano section
old_piano = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 1.0, -22.0, -14.0));
            rec.compRatio = 2.0f;
            rec.compAttackMs = 30.0f;
            rec.compReleaseMs = 250.0f;
        }"""

new_piano = """            rec.compEnabled = true;
            rec.compThresholdDb = static_cast<float>(std::clamp(feat.rmsDb - 1.0, -22.0, -14.0));
            rec.compRatio = 2.0f;
            rec.compAttackMs = 30.0f;
            rec.compReleaseMs = 250.0f;
            rec.compMode = 1; // Optical Tube
            rec.fxSlot = 0;  // Warm Reverb
            rec.fxSendDb = -10.0f;
            rec.auxSendDb = { -6.0f, -3.0f, -8.0f, 0.0f };
        }"""

assert old_piano in code
code = code.replace(old_piano, new_piano)

with open(file_path, "w", encoding="utf-8") as f:
    f.write(code)

print("Updated classifyAndRecommend with 8-module details for all instruments!")

