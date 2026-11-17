#pragma once

#include <algorithm>
#include <bitset>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace nova::graviton {

inline int manchinoAuxBusIndexFromSelectionId(int selectedId,
                                              int auxBusCount) noexcept {
  const int index = selectedId - 1;
  return index >= 0 && index < auxBusCount ? index : -1;
}

// ==============================================================================
// MIDAS Heritage-D HD96-24 Chapters 24 & 33: Manchino Multi-Edit Matrix & Show
// CSV
// ==============================================================================
class ManchinoSelection final {
public:
  static constexpr size_t kMaxChannels = 144;

  ManchinoSelection() noexcept { selectionMask.reset(); }

  void selectAll() noexcept { selectionMask.set(); }

  void clearAll() noexcept { selectionMask.reset(); }

  void setChannel(size_t index, bool selected) noexcept {
    if (index < kMaxChannels) {
      selectionMask.set(index, selected);
    }
  }

  void toggleChannel(size_t index) noexcept {
    if (index < kMaxChannels) {
      selectionMask.flip(index);
    }
  }

  bool isSelected(size_t index) const noexcept {
    return index < kMaxChannels && selectionMask.test(index);
  }

  size_t getSelectedCount() const noexcept { return selectionMask.count(); }

  void selectRange(size_t start, size_t end) noexcept {
    const size_t s = std::min(start, kMaxChannels - 1);
    const size_t e = std::min(end, kMaxChannels - 1);
    for (size_t i = s; i <= e; ++i) {
      selectionMask.set(i, true);
    }
  }

  // Stem quick-select presets
  void selectDrums() noexcept {
    clearAll();
    selectRange(0, 15); // Channels 1-16
  }

  void selectVocals() noexcept {
    clearAll();
    selectRange(16, 23); // Channels 17-24
  }

  void selectGuitars() noexcept {
    clearAll();
    selectRange(24, 31); // Channels 25-32
  }

  void selectKeyboards() noexcept {
    clearAll();
    selectRange(32, 39); // Channels 33-40
  }

  const std::bitset<kMaxChannels> &getMask() const noexcept {
    return selectionMask;
  }

private:
  std::bitset<kMaxChannels> selectionMask;
};

// ==============================================================================
// Show Spreadsheet CSV Exporter and Importer
// ==============================================================================
struct SpreadsheetChannelData {
  int channelIndex = 0; // 0-143
  std::string name = "Ch";
  std::string color = "#2A82E4";
  float trimDb = 0.0f;
  float delayMs = 0.0f;
  float hpfHz = 80.0f;
  bool hpfOn = false;
  int hpfSlope = 0; // 0 = 12dB, 1 = 18dB
  float lpfHz = 16000.0f;
  bool lpfOn = false;
  std::array<float, 8> eqFreq = {100.0f, 400.0f, 2500.0f, 10000.0f,
                                 60.0f,  250.0f, 4000.0f, 16000.0f};
  std::array<float, 8> eqGainDb{};
  std::array<float, 8> eqQ{1.0f, 1.4f, 1.4f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  size_t eqBandCount = 8;
  bool gateOn = false;
  float gateThreshDb = -45.0f;
  bool compOn = false;
  float compThreshDb = -18.0f;
  uint8_t muteGroups = 0;
  float pan = 0.0f;
  float faderDb = 0.0f;
  bool muted = false;
};

class ShowSpreadsheet final {
public:
  static std::string generateHeader() {
    return "Channel,Name,Color,Trim_dB,Delay_ms,HPF_Hz,HPF_On,HPF_Slope,LPF_Hz,"
           "LPF_On,"
           "EQ_B1_Freq,EQ_B1_Gain,EQ_B1_Q,EQ_B2_Freq,EQ_B2_Gain,EQ_B2_Q,"
           "EQ_B3_Freq,EQ_B3_Gain,EQ_B3_Q,EQ_B4_Freq,EQ_B4_Gain,EQ_B4_Q,"
           "EQ_B5_Freq,EQ_B5_Gain,EQ_B5_Q,EQ_B6_Freq,EQ_B6_Gain,EQ_B6_Q,"
           "EQ_B7_Freq,EQ_B7_Gain,EQ_B7_Q,EQ_B8_Freq,EQ_B8_Gain,EQ_B8_Q,"
           "Gate_On,Gate_Thresh,Comp_On,Comp_Thresh,Mute_Groups,Pan,Fader_dB,"
           "Muted\n";
  }

  static std::string channelToCsvLine(const SpreadsheetChannelData &ch) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << (ch.channelIndex + 1) << ","
       << "\"" << sanitizeCsvString(ch.name) << "\"," << ch.color << ","
       << ch.trimDb << "," << ch.delayMs << "," << ch.hpfHz << ","
       << (ch.hpfOn ? 1 : 0) << "," << ch.hpfSlope << "," << ch.lpfHz << ","
       << (ch.lpfOn ? 1 : 0) << ",";
    for (size_t band = 0; band < ch.eqFreq.size(); ++band) {
      ss << ch.eqFreq[band] << "," << ch.eqGainDb[band] << "," << ch.eqQ[band]
         << ",";
    }
    ss << (ch.gateOn ? 1 : 0) << "," << ch.gateThreshDb << ","
       << (ch.compOn ? 1 : 0) << "," << ch.compThreshDb << ","
       << static_cast<int>(ch.muteGroups) << "," << ch.pan << "," << ch.faderDb
       << "," << (ch.muted ? 1 : 0) << "\n";
    return ss.str();
  }

  static std::vector<std::string> splitCsvLine(const std::string &line) {
    std::vector<std::string> tokens;
    tokens.reserve(42);
    std::string token;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
      const char c = line[i];
      if (c == '\"') {
        inQuotes = !inQuotes;
      } else if (c == ',' && !inQuotes) {
        tokens.push_back(token);
        token.clear();
      } else if (c != '\r' && c != '\n') {
        token += c;
      }
    }
    tokens.push_back(token);
    return tokens;
  }

  static bool parseCsvLine(const std::string &line,
                           SpreadsheetChannelData &outData) {
    if (line.empty() || line[0] == '#' || line.rfind("Channel", 0) == 0) {
      return false;
    }

    const auto tokens = splitCsvLine(line);
    if (tokens.size() < 28)
      return false;

    try {
      const bool hasEightBandEq = tokens.size() >= 40;
      const size_t processingOffset = hasEightBandEq ? 34 : 22;
      outData.eqBandCount = hasEightBandEq ? 8 : 4;
      int chNum = std::stoi(tokens[0]);
      outData.channelIndex = std::clamp(chNum - 1, 0, 143);
      outData.name = tokens[1];
      outData.color = tokens[2].empty() ? "#2A82E4" : tokens[2];
      outData.trimDb = std::clamp(std::stof(tokens[3]), -20.0f, 60.0f);
      outData.delayMs = std::clamp(std::stof(tokens[4]), 0.0f, 500.0f);
      outData.hpfHz = std::clamp(std::stof(tokens[5]), 20.0f, 2000.0f);
      outData.hpfOn = (std::stoi(tokens[6]) != 0);
      outData.hpfSlope = std::clamp(std::stoi(tokens[7]), 0, 1);
      outData.lpfHz = std::clamp(std::stof(tokens[8]), 1000.0f, 20000.0f);
      outData.lpfOn = (std::stoi(tokens[9]) != 0);

      const size_t eqBandCount = outData.eqBandCount;
      for (size_t band = 0; band < eqBandCount; ++band) {
        const size_t offset = 10 + band * 3;
        outData.eqFreq[band] =
            std::clamp(std::stof(tokens[offset]), 20.0f, 20000.0f);
        outData.eqGainDb[band] =
            std::clamp(std::stof(tokens[offset + 1]), -18.0f, 18.0f);
        outData.eqQ[band] =
            std::clamp(std::stof(tokens[offset + 2]), 0.1f, 10.0f);
      }

      outData.gateOn = (std::stoi(tokens[processingOffset]) != 0);
      outData.gateThreshDb =
          std::clamp(std::stof(tokens[processingOffset + 1]), -80.0f, 0.0f);
      outData.compOn = (std::stoi(tokens[processingOffset + 2]) != 0);
      outData.compThreshDb =
          std::clamp(std::stof(tokens[processingOffset + 3]), -60.0f, 0.0f);
      outData.muteGroups = static_cast<uint8_t>(
          std::clamp(std::stoi(tokens[processingOffset + 4]), 0, 255));
      outData.pan =
          std::clamp(std::stof(tokens[processingOffset + 5]), -1.0f, 1.0f);
      if (tokens.size() > processingOffset + 6)
        outData.faderDb =
            std::clamp(std::stof(tokens[processingOffset + 6]), -120.0f, 10.0f);
      if (tokens.size() > processingOffset + 7)
        outData.muted = (std::stoi(tokens[processingOffset + 7]) != 0);

      return true;
    } catch (...) {
      return false;
    }
  }

private:
  static std::string sanitizeCsvString(const std::string &input) {
    std::string res;
    for (char c : input) {
      if (c == '\"')
        res += "\"\"";
      else
        res += c;
    }
    return res;
  }
};

} // namespace nova::graviton
