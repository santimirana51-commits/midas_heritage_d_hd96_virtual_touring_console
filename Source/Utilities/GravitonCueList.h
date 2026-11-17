#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <juce_core/juce_core.h>

namespace nova::graviton {

// ==============================================================================
// Scene Automation Cue List & MIDI Show Control (MSC) Engine (MIDAS Chapter 34)
// Theater, Broadway & Concert Tour Scene Automation with Granular Recall Safes
// ==============================================================================

struct CueSafes {
    bool safeFaders = false;
    bool safeEQ = false;
    bool safeDynamics = false;
    bool safeRouting = false;
    bool safeMutes = false;
};

inline bool cueSafeBlocksProperty(const CueSafes& safes,
                                  std::string_view property) noexcept {
    if (safes.safeFaders &&
        (property == "gainDb" || property == "masterDb" ||
         property == "vcaMasterDb" || property == "auxMasterDb" ||
         property == "matrixMasterDb" || property == "matrixMainLeftDb" ||
         property == "matrixMainRightDb"))
        return true;

    if (safes.safeEQ &&
        (property == "eqFrequency" || property == "eqGainDb" ||
         property == "eqQ"))
        return true;

    if (safes.safeDynamics &&
        (property == "compressor" || property == "compressorThresholdDb" ||
         property == "compressorRatio" || property == "compressorAttackMs" ||
         property == "compressorReleaseMs" || property == "gateEnabled" ||
         property == "gateThresholdDb" || property == "gateRatio" ||
         property == "gateAttackMs" || property == "gateReleaseMs" ||
         property == "postEqDynamics" || property == "dynEqEnabled" ||
         property == "dynEqMode" || property == "dynEqThresholdDb" ||
         property == "dynEqRangeDb" || property == "dynEqRatio" ||
         property == "dynEqKneeDb" || property == "dynEqAttackMs" ||
         property == "dynEqReleaseMs"))
        return true;

    if (safes.safeRouting &&
        (property == "vcaGroup" || property == "muteGroupMask" ||
         property == "auxSendDb" || property == "auxPreFader" ||
         property == "matrixAuxSendDb" || property == "matrixAuxSendMuted" ||
         property == "oscRouteMain" || property == "oscRouteAux" ||
         property == "oscRouteMatrix"))
        return true;

    return safes.safeMutes &&
           (property == "muted" || property == "solo" ||
            property == "soloBus" || property == "soloPfl" ||
            property == "auxMuted" || property == "matrixMuted" ||
            property == "matrixMainMuted" || property == "muteGroupMasters" ||
            property == "vcaMuted");
}

struct CueItem {
    float cueNumber = 1.0f;
    juce::String name = "New Cue";
    int sceneSlot = 0;              // 0..7 (maps to console scene memory)
    float crossfadeSec = 0.0f;       // 0.0 to 10.0 seconds
    CueSafes safes;
    juce::String timecode = "00:00:00:00"; // SMPTE LTC timecode trigger
};

class CueListManager final {
public:
    static constexpr int kMaxCues = 128;

    CueListManager() {
        populateDefaultConcertShow();
    }

    void populateDefaultConcertShow() {
        cues.clear();
        cues.push_back({ 1.0f, "01. House Music / Pre-Show", 0, 0.0f, { true, true, true, true, true }, "00:59:00:00" });
        cues.push_back({ 2.0f, "02. Walk-On / Intro Video Cue", 1, 3.0f, { false, false, false, false, false }, "01:00:00:00" });
        cues.push_back({ 3.0f, "03. Track 1: Explosive Opening", 2, 0.5f, { false, false, false, false, false }, "01:03:30:00" });
        cues.push_back({ 4.0f, "04. Track 2: Up-Tempo Rock", 3, 1.0f, { false, false, false, false, false }, "01:07:15:00" });
        cues.push_back({ 5.0f, "05. Track 3: Acoustic Guitar & Cello", 4, 4.0f, { false, true, false, false, false }, "01:11:00:00" });
        cues.push_back({ 6.0f, "06. Track 4: Drum Solo / Bass Feature", 5, 2.0f, { false, false, false, false, false }, "01:15:45:00" });
        cues.push_back({ 7.0f, "07. Track 5: Ballad / Piano Feature", 6, 5.0f, { false, false, false, false, false }, "01:20:10:00" });
        cues.push_back({ 8.0f, "08. Encore / Grand Finale", 7, 1.5f, { false, false, false, false, false }, "01:25:00:00" });
        currentCueIdx.store(0, std::memory_order_relaxed);
    }

    int getNumCues() const noexcept { return static_cast<int>(cues.size()); }

    const CueItem& getCue(int index) const noexcept {
        const int idx = std::clamp(index, 0, std::max(0, static_cast<int>(cues.size()) - 1));
        return cues[static_cast<size_t>(idx)];
    }

    CueItem& getCue(int index) noexcept {
        const int idx = std::clamp(index, 0, std::max(0, static_cast<int>(cues.size()) - 1));
        return cues[static_cast<size_t>(idx)];
    }

    void addCue(const CueItem& item) {
        if (cues.size() < kMaxCues) {
            cues.push_back(item);
        }
    }

    bool moveCue(int fromIndex, int toIndex) {
        if (fromIndex < 0 || fromIndex >= static_cast<int>(cues.size()) ||
            toIndex < 0 || toIndex >= static_cast<int>(cues.size()))
            return false;
        if (fromIndex == toIndex)
            return true;

        auto cue = std::move(cues[static_cast<size_t>(fromIndex)]);
        cues.erase(cues.begin() + fromIndex);
        cues.insert(cues.begin() + toIndex, std::move(cue));

        int current = currentCueIdx.load(std::memory_order_relaxed);
        if (current == fromIndex)
            current = toIndex;
        else if (fromIndex < current && toIndex >= current)
            --current;
        else if (fromIndex > current && toIndex <= current)
            ++current;
        currentCueIdx.store(current, std::memory_order_relaxed);
        return true;
    }

    juce::var toVar() const {
        auto root = new juce::DynamicObject();
        root->setProperty("format", "NOVA_HD_CUE_PLAYLIST");
        root->setProperty("version", 1);
        root->setProperty("currentCueIndex",
                          currentCueIdx.load(std::memory_order_relaxed));

        juce::Array<juce::var> cueArray;
        for (const auto& cue : cues) {
            auto item = new juce::DynamicObject();
            item->setProperty("cueNumber", static_cast<double>(cue.cueNumber));
            item->setProperty("name", cue.name);
            item->setProperty("sceneSlot", cue.sceneSlot);
            item->setProperty("crossfadeSec",
                              static_cast<double>(cue.crossfadeSec));
            item->setProperty("timecode", cue.timecode);
            auto safes = new juce::DynamicObject();
            safes->setProperty("faders", cue.safes.safeFaders);
            safes->setProperty("eq", cue.safes.safeEQ);
            safes->setProperty("dynamics", cue.safes.safeDynamics);
            safes->setProperty("routing", cue.safes.safeRouting);
            safes->setProperty("mutes", cue.safes.safeMutes);
            item->setProperty("safes", juce::var(safes));
            cueArray.add(juce::var(item));
        }
        root->setProperty("cues", cueArray);
        return juce::var(root);
    }

    bool loadFromVar(const juce::var& value) {
        auto* root = value.getDynamicObject();
        if (root == nullptr ||
            root->getProperty("format").toString() !=
                "NOVA_HD_CUE_PLAYLIST" ||
            static_cast<int>(root->getProperty("version")) != 1)
            return false;

        const auto cueValue = root->getProperty("cues");
        const auto* cueArray = cueValue.getArray();
        if (cueArray == nullptr || cueArray->isEmpty() ||
            cueArray->size() > kMaxCues)
            return false;

        std::vector<CueItem> restored;
        restored.reserve(static_cast<size_t>(cueArray->size()));
        for (const auto& valueItem : *cueArray) {
            auto* item = valueItem.getDynamicObject();
            if (item == nullptr)
                return false;

            CueItem cue;
            cue.cueNumber =
                static_cast<float>(item->getProperty("cueNumber"));
            cue.name = item->getProperty("name").toString().trim();
            cue.sceneSlot = static_cast<int>(item->getProperty("sceneSlot"));
            cue.crossfadeSec =
                static_cast<float>(item->getProperty("crossfadeSec"));
            cue.timecode = item->getProperty("timecode").toString();
            auto* safes = item->getProperty("safes").getDynamicObject();
            if (safes == nullptr || !std::isfinite(cue.cueNumber) ||
                cue.name.isEmpty() || cue.sceneSlot < 0 || cue.sceneSlot > 7 ||
                !std::isfinite(cue.crossfadeSec) || cue.crossfadeSec < 0.0f ||
                cue.crossfadeSec > 10.0f || cue.timecode.isEmpty())
                return false;

            cue.safes.safeFaders = safes->getProperty("faders");
            cue.safes.safeEQ = safes->getProperty("eq");
            cue.safes.safeDynamics = safes->getProperty("dynamics");
            cue.safes.safeRouting = safes->getProperty("routing");
            cue.safes.safeMutes = safes->getProperty("mutes");
            restored.push_back(std::move(cue));
        }

        const int requestedCurrent =
            static_cast<int>(root->getProperty("currentCueIndex"));
        if ((restored.empty() && requestedCurrent != 0) ||
            (!restored.empty() &&
             (requestedCurrent < 0 ||
              requestedCurrent >= static_cast<int>(restored.size()))))
            return false;

        cues = std::move(restored);
        currentCueIdx.store(requestedCurrent, std::memory_order_relaxed);
        isFading.store(false, std::memory_order_relaxed);
        fadeProgress.store(0.0f, std::memory_order_relaxed);
        return true;
    }

    void removeCue(int index) {
        if (cues.size() > 1 && index >= 0 &&
            index < static_cast<int>(cues.size())) {
            cues.erase(cues.begin() + index);
            int current = currentCueIdx.load(std::memory_order_relaxed);
            if (index < current)
                --current;
            else if (index == current)
                current = std::min(current,
                                   static_cast<int>(cues.size()) - 1);
            currentCueIdx.store(current, std::memory_order_relaxed);
            isFading.store(false, std::memory_order_relaxed);
            fadeProgress.store(0.0f, std::memory_order_relaxed);
        }
    }

    int getCurrentCueIndex() const noexcept {
        return currentCueIdx.load(std::memory_order_relaxed);
    }

    void setCurrentCueIndex(int idx) noexcept {
        if (idx >= 0 && idx < static_cast<int>(cues.size()))
            currentCueIdx.store(idx, std::memory_order_relaxed);
    }

    // Cue Transport: GO, STOP, PREV
    bool fireGo() noexcept {
        if (cues.empty()) return false;
        const int cur = currentCueIdx.load(std::memory_order_relaxed);
        const int next = std::min(cur + 1, static_cast<int>(cues.size()) - 1);
        currentCueIdx.store(next, std::memory_order_relaxed);
        isFading.store(true, std::memory_order_relaxed);
        fadeProgress.store(0.0f, std::memory_order_relaxed);
        return true;
    }

    void firePrev() noexcept {
        if (cues.empty()) return;
        const int cur = currentCueIdx.load(std::memory_order_relaxed);
        const int prev = std::max(0, cur - 1);
        currentCueIdx.store(prev, std::memory_order_relaxed);
        isFading.store(false, std::memory_order_relaxed);
        fadeProgress.store(0.0f, std::memory_order_relaxed);
    }

    void fireStop() noexcept {
        isFading.store(false, std::memory_order_relaxed);
    }

    bool isTransitioning() const noexcept {
        return isFading.load(std::memory_order_relaxed);
    }

    float getFadeProgress() const noexcept {
        return fadeProgress.load(std::memory_order_relaxed);
    }

    void advanceFadeProgress(float delta) noexcept {
        if (!isFading.load(std::memory_order_relaxed)) return;
        const float cur = fadeProgress.load(std::memory_order_relaxed);
        const float next = cur + delta;
        if (next >= 1.0f) {
            fadeProgress.store(1.0f, std::memory_order_relaxed);
            isFading.store(false, std::memory_order_relaxed);
        } else {
            fadeProgress.store(next, std::memory_order_relaxed);
        }
    }

    /**
     * MIDI Show Control (MSC) parser for standard 0x7F Sysex:
     * F0 7F <devID> 02 <cmdFmt> <cmd> [data] F7
     * Commands: 0x01 = GO, 0x02 = STOP, 0x03 = RESUME, 0x04 = TIMED_GO
     */
    bool handleMidiShowControl(const uint8_t* data, int length) {
        if (data == nullptr || length < 6) return false;
        if (data[0] != 0xF0 || data[1] != 0x7F || data[3] != 0x02) return false;
        // const uint8_t cmdFmt = data[4];
        const uint8_t cmd = data[5];

        if (cmd == 0x01) { // GO
            fireGo();
            return true;
        } else if (cmd == 0x02) { // STOP
            fireStop();
            return true;
        } else if (cmd == 0x03) { // RESUME
            isFading.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

private:
    std::vector<CueItem> cues;
    std::atomic<int> currentCueIdx{ 0 };
    std::atomic<bool> isFading{ false };
    std::atomic<float> fadeProgress{ 0.0f };
};

} // namespace nova::graviton
