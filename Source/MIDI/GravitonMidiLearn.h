#pragma once
/*  GravitonMidiLearn.h  –  MIDI Learn & Hardware Controller Mapper
    ================================================================
    Full interactive MIDI Learn system inspired by MIDAS HD96-24 Chapter 17.
    
    Features:
    - CC / NRPN / Note / PitchBend mapping
    - Learn mode: auto-detect incoming MIDI → assign to parameter
    - Multi-page mapping table (up to 512 mappings)
    - Value scaling (min/max range, curve)
    - Save / Load mapping presets (JSON)
    - Motor fader support (bidirectional feedback)
    - Per-channel & global mappings
    - Mapping groups for different controllers
*/

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <functional>
#include <algorithm>

namespace nova {

struct MackieControlProtocol final {
    static constexpr int FADER_COUNT = 8;
    static constexpr int MIDI_CHANNEL = 1;
    static constexpr int FIRST_MUTE_NOTE = 0x10;
    static constexpr int FIRST_SOLO_NOTE = 0x08;

    static int faderPositionFromDb(float decibels) noexcept {
        const float bounded = std::clamp(decibels, -60.0f, 10.0f);
        const float normalized = (bounded + 60.0f) / 70.0f;
        return static_cast<int>(std::lround(normalized * 16383.0f));
    }

    static juce::MidiMessage faderMessage(int faderIndex,
                                          float decibels) noexcept {
        const int channel = std::clamp(faderIndex, 0, FADER_COUNT - 1) + 1;
        return juce::MidiMessage::pitchWheel(
            channel, faderPositionFromDb(decibels));
    }

    static juce::MidiMessage muteLedMessage(int faderIndex,
                                            bool enabled) noexcept {
        const int note = FIRST_MUTE_NOTE +
                         std::clamp(faderIndex, 0, FADER_COUNT - 1);
        return juce::MidiMessage::noteOn(
            MIDI_CHANNEL, note,
            static_cast<juce::uint8>(enabled ? 0x7f : 0));
    }

    static juce::MidiMessage soloLedMessage(int faderIndex,
                                            bool enabled) noexcept {
        const int note = FIRST_SOLO_NOTE +
                         std::clamp(faderIndex, 0, FADER_COUNT - 1);
        return juce::MidiMessage::noteOn(
            MIDI_CHANNEL, note,
            static_cast<juce::uint8>(enabled ? 0x7f : 0));
    }
};

// ─── MIDI Message Types ───────────────────────────────────────
enum class MidiMsgType {
    CC = 0,
    NRPN,
    Note,
    PitchBend,
    ProgramChange,
    Aftertouch,
    PolyAftertouch
};

// ─── Parameter Target ─────────────────────────────────────────
enum class ParamTarget {
    None = 0,
    ChannelFader,
    ChannelPan,
    ChannelMute,
    ChannelSolo,
    ChannelGain,
    ChannelHPF,
    EqBand1Gain, EqBand2Gain, EqBand3Gain, EqBand4Gain,
    EqBand1Freq, EqBand2Freq, EqBand3Freq, EqBand4Freq,
    EqBand1Q, EqBand2Q, EqBand3Q, EqBand4Q,
    CompThreshold, CompRatio, CompAttack, CompRelease, CompMakeup,
    GateThreshold, GateRange, GateAttack, GateRelease,
    AuxSend1, AuxSend2, AuxSend3, AuxSend4,
    AuxSend5, AuxSend6, AuxSend7, AuxSend8,
    AuxSend9, AuxSend10, AuxSend11, AuxSend12,
    FxReturn1, FxReturn2, FxReturn3, FxReturn4,
    MasterFader,
    MasterMute,
    VcaFader,
    VcaMute,
    DcaFader,
    DcaMute,
    MonitorLevel,
    TalkbackLevel,
    FxParam1, FxParam2, FxParam3, FxParam4,
    SceneRecall,
    SceneNext,
    ScenePrev,
    TapTempo,
    Transport_Play,
    Transport_Stop,
    Transport_Record
};

inline const char* paramTargetName(ParamTarget t) {
    switch (t) {
        case ParamTarget::ChannelFader: return "Ch Fader";
        case ParamTarget::ChannelPan: return "Ch Pan";
        case ParamTarget::ChannelMute: return "Ch Mute";
        case ParamTarget::ChannelSolo: return "Ch Solo";
        case ParamTarget::ChannelGain: return "Ch Gain";
        case ParamTarget::ChannelHPF: return "Ch HPF";
        case ParamTarget::EqBand1Gain: return "EQ1 Gain";
        case ParamTarget::EqBand2Gain: return "EQ2 Gain";
        case ParamTarget::EqBand3Gain: return "EQ3 Gain";
        case ParamTarget::EqBand4Gain: return "EQ4 Gain";
        case ParamTarget::CompThreshold: return "Comp Thresh";
        case ParamTarget::CompRatio: return "Comp Ratio";
        case ParamTarget::GateThreshold: return "Gate Thresh";
        case ParamTarget::MasterFader: return "Master Fader";
        case ParamTarget::VcaFader: return "VCA Fader";
        case ParamTarget::MonitorLevel: return "Monitor Level";
        case ParamTarget::SceneRecall: return "Scene Recall";
        case ParamTarget::TapTempo: return "Tap Tempo";
        default: return "Unknown";
    }
}

// ─── Value Curve ──────────────────────────────────────────────
enum class MidiCurve {
    Linear = 0,
    Log,
    Exp,
    SCurve,
    Toggle
};

// ─── Single Mapping Entry ─────────────────────────────────────
struct MidiMapping {
    int id = 0;
    bool enabled = true;
    
    // Source
    MidiMsgType msgType = MidiMsgType::CC;
    int midiChannel = 0;       // 0 = omni, 1-16
    int ccNumber = 0;          // CC 0-127 or NRPN MSB
    int nrpnLSB = 0;          // NRPN LSB (if NRPN type)
    int noteNumber = 60;       // Note number (if Note type)
    
    // Target
    ParamTarget target = ParamTarget::None;
    int targetChannel = 1;     // 1-144 (mixer channel)
    int targetBus = 0;         // Bus number for aux sends
    int targetSlot = 0;        // FX slot number
    
    // Value scaling
    MidiCurve curve = MidiCurve::Linear;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    bool inverted = false;
    
    // Feedback (for motorized faders)
    bool feedbackEnabled = false;
    
    // Label
    std::string label;
    
    float applyValue(int rawMidi, int maxRaw = 127) const {
        float norm = static_cast<float>(rawMidi) / static_cast<float>(maxRaw);
        if (inverted) norm = 1.0f - norm;
        
        switch (curve) {
            case MidiCurve::Log:
                norm = std::log10(1.0f + norm * 9.0f);
                break;
            case MidiCurve::Exp:
                norm = (std::pow(10.0f, norm) - 1.0f) / 9.0f;
                break;
            case MidiCurve::SCurve:
                norm = 0.5f * (1.0f + std::tanh(4.0f * (norm - 0.5f)) / std::tanh(2.0f));
                break;
            case MidiCurve::Toggle:
                norm = (rawMidi >= 64) ? 1.0f : 0.0f;
                break;
            default: break;
        }
        
        return minValue + norm * (maxValue - minValue);
    }
    
    juce::var toVar() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("id", id);
        obj->setProperty("enabled", enabled);
        obj->setProperty("msgType", static_cast<int>(msgType));
        obj->setProperty("midiChannel", midiChannel);
        obj->setProperty("ccNumber", ccNumber);
        obj->setProperty("nrpnLSB", nrpnLSB);
        obj->setProperty("noteNumber", noteNumber);
        obj->setProperty("target", static_cast<int>(target));
        obj->setProperty("targetChannel", targetChannel);
        obj->setProperty("targetBus", targetBus);
        obj->setProperty("targetSlot", targetSlot);
        obj->setProperty("curve", static_cast<int>(curve));
        obj->setProperty("minValue", static_cast<double>(minValue));
        obj->setProperty("maxValue", static_cast<double>(maxValue));
        obj->setProperty("inverted", inverted);
        obj->setProperty("feedbackEnabled", feedbackEnabled);
        obj->setProperty("label", juce::String(label));
        return juce::var(obj);
    }
    
    static MidiMapping fromVar(const juce::var& v) {
        MidiMapping m;
        if (auto* obj = v.getDynamicObject()) {
            m.id = obj->getProperty("id");
            m.enabled = obj->getProperty("enabled");
            m.msgType = static_cast<MidiMsgType>((int)obj->getProperty("msgType"));
            m.midiChannel = obj->getProperty("midiChannel");
            m.ccNumber = obj->getProperty("ccNumber");
            m.nrpnLSB = obj->getProperty("nrpnLSB");
            m.noteNumber = obj->getProperty("noteNumber");
            m.target = static_cast<ParamTarget>((int)obj->getProperty("target"));
            m.targetChannel = obj->getProperty("targetChannel");
            m.targetBus = obj->getProperty("targetBus");
            m.targetSlot = obj->getProperty("targetSlot");
            m.curve = static_cast<MidiCurve>((int)obj->getProperty("curve"));
            m.minValue = static_cast<float>((double)obj->getProperty("minValue"));
            m.maxValue = static_cast<float>((double)obj->getProperty("maxValue"));
            m.inverted = obj->getProperty("inverted");
            m.feedbackEnabled = obj->getProperty("feedbackEnabled");
            m.label = obj->getProperty("label").toString().toStdString();
        }
        return m;
    }
};

// ─── Mapping Group (e.g. "Behringer X-Touch", "iPad Surface") ─
struct MappingGroup {
    std::string name = "Default";
    std::vector<MidiMapping> mappings;
    bool active = true;
    
    juce::var toVar() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("name", juce::String(name));
        obj->setProperty("active", active);
        juce::Array<juce::var> arr;
        for (auto& m : mappings) arr.add(m.toVar());
        obj->setProperty("mappings", arr);
        return juce::var(obj);
    }
    
    static MappingGroup fromVar(const juce::var& v) {
        MappingGroup g;
        if (auto* obj = v.getDynamicObject()) {
            g.name = obj->getProperty("name").toString().toStdString();
            g.active = obj->getProperty("active");
            if (auto* arr = obj->getProperty("mappings").getArray()) {
                for (auto& item : *arr)
                    g.mappings.push_back(MidiMapping::fromVar(item));
            }
        }
        return g;
    }
};

// ─── Learn State ──────────────────────────────────────────────
struct LearnState {
    bool active = false;
    ParamTarget pendingTarget = ParamTarget::None;
    int pendingChannel = 1;
    int pendingBus = 0;
    int lastDetectedCC = -1;
    int lastDetectedChannel = -1;
    MidiMsgType lastDetectedType = MidiMsgType::CC;
};

// ─── MIDI Learn Engine ───────────────────────────────────────
class MidiLearnEngine {
public:
    static constexpr int MAX_MAPPINGS = 512;
    static constexpr int MAX_GROUPS = 16;
    
    MidiLearnEngine() {
        groups_.emplace_back();  // Default group
    }
    
    // ── Learn Mode ──
    void startLearn(ParamTarget target, int channel = 1, int bus = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        learnState_.active = true;
        learnState_.pendingTarget = target;
        learnState_.pendingChannel = channel;
        learnState_.pendingBus = bus;
        learnState_.lastDetectedCC = -1;
    }
    
    void cancelLearn() {
        std::lock_guard<std::mutex> lock(mutex_);
        learnState_.active = false;
    }
    
    bool isLearning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return learnState_.active;
    }
    LearnState getLearnState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return learnState_;
    }
    
    // ── Process Incoming MIDI ──
    struct MidiResult {
        bool mapped = false;
        ParamTarget target = ParamTarget::None;
        int targetChannel = 0;
        int targetBus = 0;
        float value = 0.0f;
        bool learned = false;  // true if new mapping was created via learn
    };
    
    MidiResult processCC(int channel, int cc, int value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Learn mode
        if (learnState_.active) {
            learnState_.lastDetectedCC = cc;
            learnState_.lastDetectedChannel = channel;
            learnState_.lastDetectedType = MidiMsgType::CC;
            
            // Create mapping
            MidiMapping m;
            m.id = nextId_++;
            m.msgType = MidiMsgType::CC;
            m.midiChannel = channel;
            m.ccNumber = cc;
            m.target = learnState_.pendingTarget;
            m.targetChannel = learnState_.pendingChannel;
            m.targetBus = learnState_.pendingBus;
            m.label = std::string(paramTargetName(m.target)) + " Ch" + std::to_string(m.targetChannel);
            
            if (activeGroup_ >= 0 &&
                activeGroup_ < static_cast<int>(groups_.size()) &&
                groups_[static_cast<size_t>(activeGroup_)].mappings.size() <
                    MAX_MAPPINGS)
                groups_[static_cast<size_t>(activeGroup_)].mappings.push_back(m);
            
            learnState_.active = false;
            
            MidiResult r;
            r.mapped = true;
            r.learned = true;
            r.target = m.target;
            r.targetChannel = m.targetChannel;
            r.value = m.applyValue(value);
            return r;
        }
        
        // Normal lookup
        return lookupCC(channel, cc, value);
    }
    
    MidiResult processNote(int channel, int note, int velocity) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (learnState_.active) {
            learnState_.lastDetectedCC = note;
            learnState_.lastDetectedChannel = channel;
            learnState_.lastDetectedType = MidiMsgType::Note;
            
            MidiMapping m;
            m.id = nextId_++;
            m.msgType = MidiMsgType::Note;
            m.midiChannel = channel;
            m.noteNumber = note;
            m.target = learnState_.pendingTarget;
            m.targetChannel = learnState_.pendingChannel;
            m.curve = MidiCurve::Toggle;
            m.label = std::string(paramTargetName(m.target)) + " Ch" + std::to_string(m.targetChannel);
            
            if (!groups_.empty())
                groups_[activeGroup_].mappings.push_back(m);
            
            learnState_.active = false;
            
            MidiResult r;
            r.mapped = true;
            r.learned = true;
            r.target = m.target;
            r.targetChannel = m.targetChannel;
            r.value = velocity > 0 ? 1.0f : 0.0f;
            return r;
        }
        
        // Normal lookup for notes
        for (auto& g : groups_) {
            if (!g.active) continue;
            for (auto& mp : g.mappings) {
                if (!mp.enabled) continue;
                if (mp.msgType != MidiMsgType::Note) continue;
                if (mp.midiChannel != 0 && mp.midiChannel != channel) continue;
                if (mp.noteNumber == note) {
                    MidiResult r;
                    r.mapped = true;
                    r.target = mp.target;
                    r.targetChannel = mp.targetChannel;
                    r.targetBus = mp.targetBus;
                    r.value = mp.applyValue(velocity);
                    return r;
                }
            }
        }
        return {};
    }
    
    MidiResult processPitchBend(int channel, int value14bit) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& g : groups_) {
            if (!g.active) continue;
            for (auto& mp : g.mappings) {
                if (!mp.enabled) continue;
                if (mp.msgType != MidiMsgType::PitchBend) continue;
                if (mp.midiChannel != 0 && mp.midiChannel != channel) continue;
                MidiResult r;
                r.mapped = true;
                r.target = mp.target;
                r.targetChannel = mp.targetChannel;
                r.value = mp.applyValue(value14bit, 16383);
                return r;
            }
        }
        return {};
    }
    
    // ── Mapping Management ──
    int addMapping(const MidiMapping& m, int groupIdx = -1) {
        std::lock_guard<std::mutex> lock(mutex_);
        int gi = (groupIdx >= 0) ? groupIdx : activeGroup_;
        if (gi >= (int)groups_.size()) return -1;
        auto& g = groups_[gi];
        if ((int)g.mappings.size() >= MAX_MAPPINGS) return -1;
        MidiMapping nm = m;
        nm.id = nextId_++;
        g.mappings.push_back(nm);
        return nm.id;
    }
    
    bool removeMapping(int mappingId) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& g : groups_) {
            auto it = std::remove_if(g.mappings.begin(), g.mappings.end(),
                [mappingId](const MidiMapping& m) { return m.id == mappingId; });
            if (it != g.mappings.end()) {
                g.mappings.erase(it, g.mappings.end());
                return true;
            }
        }
        return false;
    }
    
    void clearAllMappings() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& g : groups_) g.mappings.clear();
    }
    
    void clearGroup(int idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (idx >= 0 && idx < (int)groups_.size())
            groups_[idx].mappings.clear();
    }
    
    // ── Group Management ──
    int addGroup(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if ((int)groups_.size() >= MAX_GROUPS) return -1;
        MappingGroup g;
        g.name = name;
        groups_.push_back(g);
        return (int)groups_.size() - 1;
    }
    
    void setActiveGroup(int idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (idx >= 0 && idx < static_cast<int>(groups_.size()))
            activeGroup_ = idx;
    }

    int getActiveGroup() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeGroup_;
    }

    int getGroupCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(groups_.size());
    }

    // Return a snapshot: never expose references into the concurrently-mutated
    // mapping vector.
    MappingGroup getGroup(int idx) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (idx >= 0 && idx < static_cast<int>(groups_.size()))
            return groups_[static_cast<size_t>(idx)];
        return {};
    }

    int getMappingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        int total = 0;
        for (const auto& g : groups_)
            total += static_cast<int>(g.mappings.size());
        return total;
    }
    
    // ── Feedback (for motor faders) ──
    struct FeedbackMsg {
        int midiChannel = 1;
        int cc = 0;
        int value = 0;
    };
    
    std::vector<FeedbackMsg> generateFeedback(ParamTarget target, int ch, float normValue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FeedbackMsg> msgs;
        for (auto& g : groups_) {
            if (!g.active) continue;
            for (auto& mp : g.mappings) {
                if (!mp.feedbackEnabled) continue;
                if (mp.target != target) continue;
                if (mp.targetChannel != ch) continue;
                if (mp.msgType == MidiMsgType::CC) {
                    FeedbackMsg fb;
                    fb.midiChannel = mp.midiChannel;
                    fb.cc = mp.ccNumber;
                    fb.value = static_cast<int>(normValue * 127.0f);
                    msgs.push_back(fb);
                }
            }
        }
        return msgs;
    }
    
    // ── Serialization ──
    juce::String saveToJSON() const {
        std::lock_guard<std::mutex> lock(mutex_);
        juce::Array<juce::var> arr;
        for (auto& g : groups_) arr.add(g.toVar());
        auto root = new juce::DynamicObject();
        root->setProperty("version", 1);
        root->setProperty("activeGroup", activeGroup_);
        root->setProperty("groups", arr);
        return juce::JSON::toString(juce::var(root));
    }
    
    bool loadFromJSON(const juce::String& json) {
        auto parsed = juce::JSON::parse(json);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return false;

        std::vector<MappingGroup> loadedGroups;
        if (auto* arr = obj->getProperty("groups").getArray()) {
            loadedGroups.reserve(static_cast<size_t>(arr->size()));
            for (const auto& item : *arr)
                loadedGroups.push_back(MappingGroup::fromVar(item));
        }
        if (loadedGroups.empty())
            loadedGroups.emplace_back();

        int loadedActiveGroup = static_cast<int>(obj->getProperty("activeGroup"));
        loadedActiveGroup = std::clamp(
            loadedActiveGroup, 0, static_cast<int>(loadedGroups.size()) - 1);
        int loadedNextId = 1;
        for (const auto& group : loadedGroups)
            for (const auto& mapping : group.mappings)
                if (mapping.id >= loadedNextId)
                    loadedNextId = mapping.id + 1;

        std::lock_guard<std::mutex> lock(mutex_);
        groups_ = std::move(loadedGroups);
        activeGroup_ = loadedActiveGroup;
        nextId_ = loadedNextId;
        learnState_.active = false;
        return true;
    }
    
private:
    MidiResult lookupCC(int channel, int cc, int value) {
        for (auto& g : groups_) {
            if (!g.active) continue;
            for (auto& mp : g.mappings) {
                if (!mp.enabled) continue;
                if (mp.msgType != MidiMsgType::CC) continue;
                if (mp.midiChannel != 0 && mp.midiChannel != channel) continue;
                if (mp.ccNumber == cc) {
                    MidiResult r;
                    r.mapped = true;
                    r.target = mp.target;
                    r.targetChannel = mp.targetChannel;
                    r.targetBus = mp.targetBus;
                    r.value = mp.applyValue(value);
                    return r;
                }
            }
        }
        return {};
    }
    
    std::vector<MappingGroup> groups_;
    int activeGroup_ = 0;
    int nextId_ = 1;
    LearnState learnState_;
    mutable std::mutex mutex_;
};

} // namespace nova
