#pragma once
/*  GravitonShowArchive.h  –  Show Archive Package Manager (.show format)
    =====================================================================
    Manages complete show packages for MIDAS HD96-24 style workflow.
    
    A .show package bundles:
    - Show Info (title, venue, artist, date, notes, version)
    - Up to 100 Scene Cues (with all channel/bus/FX states)
    - Patch Matrix configuration
    - FX Rack presets (all 24 slots)
    - Input/Output routing
    - VCA / POP group assignments
    - Monitor & talkback settings
    - Custom scribble strip names & colors
    - MIDI mapping presets
    - User preferences
    
    File format: JSON-based with optional compression (gzip)
    Extension: .novashow
*/

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <map>
#include <ctime>
#include <chrono>
#include <algorithm>

namespace nova {

// ─── Show Info ────────────────────────────────────────────────
struct ShowInfo {
    std::string title = "Untitled Show";
    std::string venue;
    std::string artist;
    std::string engineer;
    std::string date;
    std::string notes;
    int version = 1;
    std::string createdWith = "NOVA HD v2.0.0";
    std::string uuid;
    
    ShowInfo() {
        // Generate simple UUID
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        uuid = "show-" + std::to_string(ms);
        
        auto t = std::time(nullptr);
        char buf[32];
        std::tm tmBuf{};
#if defined(_WIN32)
        localtime_s(&tmBuf, &t);
#else
        localtime_r(&t, &tmBuf);
#endif
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmBuf);
        date = buf;
    }
    
    juce::var toVar() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("title", juce::String(title));
        obj->setProperty("venue", juce::String(venue));
        obj->setProperty("artist", juce::String(artist));
        obj->setProperty("engineer", juce::String(engineer));
        obj->setProperty("date", juce::String(date));
        obj->setProperty("notes", juce::String(notes));
        obj->setProperty("version", version);
        obj->setProperty("createdWith", juce::String(createdWith));
        obj->setProperty("uuid", juce::String(uuid));
        return juce::var(obj);
    }
    
    static ShowInfo fromVar(const juce::var& v) {
        ShowInfo info;
        if (auto* obj = v.getDynamicObject()) {
            info.title = obj->getProperty("title").toString().toStdString();
            info.venue = obj->getProperty("venue").toString().toStdString();
            info.artist = obj->getProperty("artist").toString().toStdString();
            info.engineer = obj->getProperty("engineer").toString().toStdString();
            info.date = obj->getProperty("date").toString().toStdString();
            info.notes = obj->getProperty("notes").toString().toStdString();
            info.version = obj->getProperty("version");
            info.createdWith = obj->getProperty("createdWith").toString().toStdString();
            info.uuid = obj->getProperty("uuid").toString().toStdString();
        }
        return info;
    }
};

// ─── Channel State (snapshot of one channel) ──────────────────
struct ChannelSnapshot {
    int channelIndex = 0;
    std::string name;
    float fader = 0.0f;       // dB
    float pan = 0.0f;         // -1..+1
    float gain = 0.0f;        // dB
    bool mute = false;
    bool solo = false;
    int color = 0;            // 0-15
    float hpfFreq = 20.0f;
    bool hpfEnabled = false;
    bool phantomPower = false;
    bool phaseInvert = false;
    
    // EQ (4 bands)
    struct EqBand { float freq = 1000.0f; float gain = 0.0f; float q = 1.0f; bool enabled = true; };
    std::array<EqBand, 4> eq;
    
    // Dynamics
    float compThreshold = 0.0f;
    float compRatio = 1.0f;
    float compAttack = 10.0f;
    float compRelease = 100.0f;
    float compMakeup = 0.0f;
    bool compEnabled = false;
    
    float gateThreshold = -80.0f;
    float gateRange = -80.0f;
    float gateAttack = 0.5f;
    float gateRelease = 50.0f;
    bool gateEnabled = false;
    
    // Aux sends (up to 24)
    std::vector<float> auxSends;
    std::vector<bool> auxSendEnabled;
    std::vector<bool> auxSendPreFader;
    
    // VCA assignment
    int vcaGroup = 0;  // 0 = none
    
    // Input patch
    int inputPatch = 0;
    
    juce::var toVar() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("ch", channelIndex);
        obj->setProperty("name", juce::String(name));
        obj->setProperty("fader", static_cast<double>(fader));
        obj->setProperty("pan", static_cast<double>(pan));
        obj->setProperty("gain", static_cast<double>(gain));
        obj->setProperty("mute", mute);
        obj->setProperty("solo", solo);
        obj->setProperty("color", color);
        obj->setProperty("hpfFreq", static_cast<double>(hpfFreq));
        obj->setProperty("hpfEnabled", hpfEnabled);
        obj->setProperty("phantom", phantomPower);
        obj->setProperty("phaseInvert", phaseInvert);
        obj->setProperty("vcaGroup", vcaGroup);
        obj->setProperty("inputPatch", inputPatch);
        
        // EQ
        juce::Array<juce::var> eqArr;
        for (auto& b : eq) {
            auto bo = new juce::DynamicObject();
            bo->setProperty("freq", static_cast<double>(b.freq));
            bo->setProperty("gain", static_cast<double>(b.gain));
            bo->setProperty("q", static_cast<double>(b.q));
            bo->setProperty("enabled", b.enabled);
            eqArr.add(juce::var(bo));
        }
        obj->setProperty("eq", eqArr);
        
        // Dynamics
        obj->setProperty("compThresh", static_cast<double>(compThreshold));
        obj->setProperty("compRatio", static_cast<double>(compRatio));
        obj->setProperty("compAttack", static_cast<double>(compAttack));
        obj->setProperty("compRelease", static_cast<double>(compRelease));
        obj->setProperty("compMakeup", static_cast<double>(compMakeup));
        obj->setProperty("compEnabled", compEnabled);
        obj->setProperty("gateThresh", static_cast<double>(gateThreshold));
        obj->setProperty("gateRange", static_cast<double>(gateRange));
        obj->setProperty("gateAttack", static_cast<double>(gateAttack));
        obj->setProperty("gateRelease", static_cast<double>(gateRelease));
        obj->setProperty("gateEnabled", gateEnabled);
        
        // Aux sends
        juce::Array<juce::var> auxArr;
        for (size_t i = 0; i < auxSends.size(); ++i) {
            auto ao = new juce::DynamicObject();
            ao->setProperty("level", static_cast<double>(auxSends[i]));
            ao->setProperty("enabled", i < auxSendEnabled.size() ? auxSendEnabled[i] : true);
            ao->setProperty("preFader", i < auxSendPreFader.size() ? auxSendPreFader[i] : false);
            auxArr.add(juce::var(ao));
        }
        obj->setProperty("auxSends", auxArr);
        
        return juce::var(obj);
    }
    
    static ChannelSnapshot fromVar(const juce::var& v) {
        ChannelSnapshot cs;
        if (auto* obj = v.getDynamicObject()) {
            cs.channelIndex = obj->getProperty("ch");
            cs.name = obj->getProperty("name").toString().toStdString();
            cs.fader = static_cast<float>((double)obj->getProperty("fader"));
            cs.pan = static_cast<float>((double)obj->getProperty("pan"));
            cs.gain = static_cast<float>((double)obj->getProperty("gain"));
            cs.mute = obj->getProperty("mute");
            cs.solo = obj->getProperty("solo");
            cs.color = obj->getProperty("color");
            cs.hpfFreq = static_cast<float>((double)obj->getProperty("hpfFreq"));
            cs.hpfEnabled = obj->getProperty("hpfEnabled");
            cs.phantomPower = obj->getProperty("phantom");
            cs.phaseInvert = obj->getProperty("phaseInvert");
            cs.vcaGroup = obj->getProperty("vcaGroup");
            cs.inputPatch = obj->getProperty("inputPatch");
            
            if (auto* eqArr = obj->getProperty("eq").getArray()) {
                for (int i = 0; i < std::min((int)eqArr->size(), 4); ++i) {
                    if (auto* bo = (*eqArr)[i].getDynamicObject()) {
                        cs.eq[i].freq = static_cast<float>((double)bo->getProperty("freq"));
                        cs.eq[i].gain = static_cast<float>((double)bo->getProperty("gain"));
                        cs.eq[i].q = static_cast<float>((double)bo->getProperty("q"));
                        cs.eq[i].enabled = bo->getProperty("enabled");
                    }
                }
            }
            
            cs.compThreshold = static_cast<float>((double)obj->getProperty("compThresh"));
            cs.compRatio = static_cast<float>((double)obj->getProperty("compRatio"));
            cs.compAttack = static_cast<float>((double)obj->getProperty("compAttack"));
            cs.compRelease = static_cast<float>((double)obj->getProperty("compRelease"));
            cs.compMakeup = static_cast<float>((double)obj->getProperty("compMakeup"));
            cs.compEnabled = obj->getProperty("compEnabled");
            cs.gateThreshold = static_cast<float>((double)obj->getProperty("gateThresh"));
            cs.gateRange = static_cast<float>((double)obj->getProperty("gateRange"));
            cs.gateAttack = static_cast<float>((double)obj->getProperty("gateAttack"));
            cs.gateRelease = static_cast<float>((double)obj->getProperty("gateRelease"));
            cs.gateEnabled = obj->getProperty("gateEnabled");
            
            if (auto* auxArr = obj->getProperty("auxSends").getArray()) {
                for (auto& item : *auxArr) {
                    if (auto* ao = item.getDynamicObject()) {
                        cs.auxSends.push_back(static_cast<float>((double)ao->getProperty("level")));
                        cs.auxSendEnabled.push_back(ao->getProperty("enabled"));
                        cs.auxSendPreFader.push_back(ao->getProperty("preFader"));
                    }
                }
            }
        }
        return cs;
    }
};

// ─── Scene Cue ────────────────────────────────────────────────
struct SceneCue {
    int cueNumber = 0;
    std::string name = "Scene";
    std::string notes;
    std::vector<ChannelSnapshot> channels;
    
    // FX Rack state
    struct FxSlotState {
        int slotIndex = 0;
        int fxType = 0;
        std::map<std::string, float> params;
    };
    std::vector<FxSlotState> fxStates;
    
    // Master
    float masterFader = 0.0f;
    
    // Recall filters (which parameters to recall)
    bool recallFaders = true;
    bool recallMutes = true;
    bool recallEQ = true;
    bool recallDynamics = true;
    bool recallAuxSends = true;
    bool recallPanning = true;
    bool recallFX = true;
    bool recallPatching = false;
    
    juce::var toVar() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("cueNumber", cueNumber);
        obj->setProperty("name", juce::String(name));
        obj->setProperty("notes", juce::String(notes));
        obj->setProperty("masterFader", static_cast<double>(masterFader));
        
        // Channels
        juce::Array<juce::var> chArr;
        for (auto& ch : channels) chArr.add(ch.toVar());
        obj->setProperty("channels", chArr);
        
        // FX states
        juce::Array<juce::var> fxArr;
        for (auto& fx : fxStates) {
            auto fo = new juce::DynamicObject();
            fo->setProperty("slot", fx.slotIndex);
            fo->setProperty("type", fx.fxType);
            auto po = new juce::DynamicObject();
            for (auto& [k, v] : fx.params)
                po->setProperty(juce::String(k), static_cast<double>(v));
            fo->setProperty("params", juce::var(po));
            fxArr.add(juce::var(fo));
        }
        obj->setProperty("fxStates", fxArr);
        
        // Recall filters
        obj->setProperty("recallFaders", recallFaders);
        obj->setProperty("recallMutes", recallMutes);
        obj->setProperty("recallEQ", recallEQ);
        obj->setProperty("recallDynamics", recallDynamics);
        obj->setProperty("recallAuxSends", recallAuxSends);
        obj->setProperty("recallPanning", recallPanning);
        obj->setProperty("recallFX", recallFX);
        obj->setProperty("recallPatching", recallPatching);
        
        return juce::var(obj);
    }
    
    static SceneCue fromVar(const juce::var& v) {
        SceneCue sc;
        if (auto* obj = v.getDynamicObject()) {
            sc.cueNumber = obj->getProperty("cueNumber");
            sc.name = obj->getProperty("name").toString().toStdString();
            sc.notes = obj->getProperty("notes").toString().toStdString();
            sc.masterFader = static_cast<float>((double)obj->getProperty("masterFader"));
            
            if (auto* chArr = obj->getProperty("channels").getArray())
                for (auto& item : *chArr)
                    sc.channels.push_back(ChannelSnapshot::fromVar(item));
            
            if (auto* fxArr = obj->getProperty("fxStates").getArray()) {
                for (auto& item : *fxArr) {
                    if (auto* fo = item.getDynamicObject()) {
                        SceneCue::FxSlotState fs;
                        fs.slotIndex = fo->getProperty("slot");
                        fs.fxType = fo->getProperty("type");
                        if (auto* po = fo->getProperty("params").getDynamicObject()) {
                            for (auto& prop : po->getProperties())
                                fs.params[prop.name.toString().toStdString()] = 
                                    static_cast<float>((double)prop.value);
                        }
                        sc.fxStates.push_back(fs);
                    }
                }
            }
            
            sc.recallFaders = obj->getProperty("recallFaders");
            sc.recallMutes = obj->getProperty("recallMutes");
            sc.recallEQ = obj->getProperty("recallEQ");
            sc.recallDynamics = obj->getProperty("recallDynamics");
            sc.recallAuxSends = obj->getProperty("recallAuxSends");
            sc.recallPanning = obj->getProperty("recallPanning");
            sc.recallFX = obj->getProperty("recallFX");
            sc.recallPatching = obj->getProperty("recallPatching");
        }
        return sc;
    }
};

// ─── Show Archive Engine ─────────────────────────────────────
class ShowArchiveEngine {
public:
    static constexpr int MAX_SCENES = 100;
    static constexpr const char* FILE_EXTENSION = ".novashow";
    
    ShowArchiveEngine() = default;
    
    // ── Show Info ──
    ShowInfo& getInfo() { return info_; }
    const ShowInfo& getInfo() const { return info_; }
    void setInfo(const ShowInfo& info) { info_ = info; }
    
    // ── Scene Management ──
    int addScene(const SceneCue& scene) {
        if ((int)scenes_.size() >= MAX_SCENES) return -1;
        SceneCue s = scene;
        s.cueNumber = (int)scenes_.size();
        scenes_.push_back(s);
        modified_ = true;
        return s.cueNumber;
    }
    
    bool updateScene(int index, const SceneCue& scene) {
        if (index < 0 || index >= (int)scenes_.size()) return false;
        scenes_[index] = scene;
        scenes_[index].cueNumber = index;
        modified_ = true;
        return true;
    }
    
    bool removeScene(int index) {
        if (index < 0 || index >= (int)scenes_.size()) return false;
        scenes_.erase(scenes_.begin() + index);
        // Re-number
        for (int i = 0; i < (int)scenes_.size(); ++i)
            scenes_[i].cueNumber = i;
        modified_ = true;
        return true;
    }
    
    bool moveScene(int fromIdx, int toIdx) {
        if (fromIdx < 0 || fromIdx >= (int)scenes_.size()) return false;
        if (toIdx < 0 || toIdx >= (int)scenes_.size()) return false;
        auto scene = scenes_[fromIdx];
        scenes_.erase(scenes_.begin() + fromIdx);
        scenes_.insert(scenes_.begin() + toIdx, scene);
        for (int i = 0; i < (int)scenes_.size(); ++i)
            scenes_[i].cueNumber = i;
        modified_ = true;
        return true;
    }
    
    int getSceneCount() const { return (int)scenes_.size(); }
    
    const SceneCue& getScene(int index) const {
        static SceneCue empty;
        if (index >= 0 && index < (int)scenes_.size()) return scenes_[index];
        return empty;
    }
    
    SceneCue& getSceneMut(int index) { return scenes_[index]; }
    
    // ── Current Scene Navigation ──
    void setCurrentScene(int idx) { currentScene_ = idx; }
    int getCurrentScene() const { return currentScene_; }
    
    bool nextScene() {
        if (currentScene_ + 1 < (int)scenes_.size()) {
            currentScene_++;
            return true;
        }
        return false;
    }
    
    bool prevScene() {
        if (currentScene_ > 0) {
            currentScene_--;
            return true;
        }
        return false;
    }
    
    // ── File Path ──
    void setFilePath(const juce::String& path) { filePath_ = path; }
    juce::String getFilePath() const { return filePath_; }
    bool isModified() const { return modified_; }
    void clearModified() { modified_ = false; }
    
    // ── Save / Load (.novashow JSON) ──
    bool saveToFile(const juce::File& file) {
        auto root = new juce::DynamicObject();
        root->setProperty("format", juce::String("NOVA_HD_SHOW"));
        root->setProperty("formatVersion", 2);
        root->setProperty("showInfo", info_.toVar());
        root->setProperty("currentScene", currentScene_);
        
        // Scenes
        juce::Array<juce::var> sceneArr;
        for (auto& s : scenes_) sceneArr.add(s.toVar());
        root->setProperty("scenes", sceneArr);
        
        // Custom data
        if (!customData_.empty()) {
            auto cd = new juce::DynamicObject();
            for (auto& [k, v] : customData_)
                cd->setProperty(juce::String(k), juce::String(v));
            root->setProperty("customData", juce::var(cd));
        }
        
        juce::String json = juce::JSON::toString(juce::var(root), true);
        bool ok = file.replaceWithText(json);
        if (ok) {
            filePath_ = file.getFullPathName();
            modified_ = false;
        }
        return ok;
    }
    
    bool loadFromFile(const juce::File& file) {
        if (!file.existsAsFile()) return false;
        
        auto json = file.loadFileAsString();
        auto parsed = juce::JSON::parse(json);
        auto* root = parsed.getDynamicObject();
        if (!root) return false;
        
        // Validate format
        auto format = root->getProperty("format").toString();
        if (format != "NOVA_HD_SHOW") return false;
        
        info_ = ShowInfo::fromVar(root->getProperty("showInfo"));
        currentScene_ = root->getProperty("currentScene");
        
        scenes_.clear();
        if (auto* sceneArr = root->getProperty("scenes").getArray())
            for (auto& item : *sceneArr)
                scenes_.push_back(SceneCue::fromVar(item));
        
        // Custom data
        customData_.clear();
        if (auto* cd = root->getProperty("customData").getDynamicObject())
            for (auto& prop : cd->getProperties())
                customData_[prop.name.toString().toStdString()] = prop.value.toString().toStdString();
        
        filePath_ = file.getFullPathName();
        modified_ = false;
        return true;
    }
    
    // ── New Show ──
    void newShow(const std::string& title = "Untitled Show") {
        info_ = ShowInfo();
        info_.title = title;
        scenes_.clear();
        customData_.clear();
        filePath_ = "";
        currentScene_ = 0;
        modified_ = false;
    }
    
    // ── Custom Data Store ──
    void setCustomData(const std::string& key, const std::string& value) {
        customData_[key] = value;
        modified_ = true;
    }
    
    std::string getCustomData(const std::string& key) const {
        auto it = customData_.find(key);
        return (it != customData_.end()) ? it->second : "";
    }
    
    // ── Show Summary ──
    juce::String getSummary() const {
        juce::String s;
        s += "Show: " + juce::String(info_.title) + "\n";
        s += "Venue: " + juce::String(info_.venue) + "\n";
        s += "Artist: " + juce::String(info_.artist) + "\n";
        s += "Engineer: " + juce::String(info_.engineer) + "\n";
        s += "Date: " + juce::String(info_.date) + "\n";
        s += "Scenes: " + juce::String(getSceneCount()) + "/" + juce::String(MAX_SCENES) + "\n";
        s += "Current Scene: " + juce::String(currentScene_) + "\n";
        if (!filePath_.isEmpty())
            s += "File: " + filePath_ + "\n";
        s += "Modified: " + juce::String(modified_ ? "Yes" : "No") + "\n";
        return s;
    }
    
    // ── Merge (import scenes from another show) ──
    int importScenesFrom(const ShowArchiveEngine& other, int startIdx = 0, int count = -1) {
        int imported = 0;
        int total = other.getSceneCount();
        if (count < 0) count = total;
        
        for (int i = startIdx; i < std::min(startIdx + count, total); ++i) {
            if ((int)scenes_.size() >= MAX_SCENES) break;
            scenes_.push_back(other.getScene(i));
            scenes_.back().cueNumber = (int)scenes_.size() - 1;
            imported++;
        }
        if (imported > 0) modified_ = true;
        return imported;
    }
    
private:
    ShowInfo info_;
    std::vector<SceneCue> scenes_;
    std::map<std::string, std::string> customData_;
    juce::String filePath_;
    int currentScene_ = 0;
    bool modified_ = false;
};

} // namespace nova

