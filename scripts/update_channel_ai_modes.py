import re

file_path = "Source/DSP/GravitonChannelAI.h"
with open(file_path, "r", encoding="utf-8") as f:
    code = f.read()

# Let's inspect where InstrumentType is defined
assert "enum class InstrumentType" in code

new_header_types = """// ==============================================================================
// 1. DUA JENIS KECERDASAN (TWO AI INTELLIGENCE MODES)
// ==============================================================================
enum class AIMode {
    SoundEngineer = 0,    // Mode Penasihat: Analisis & rekomendasi untuk operator (Aman untuk Live Show)
    AutonomousMixer = 1   // Mode Otonom: Otomatis mengatur Gain, EQ, Comp, Gate, FX, Monitor secara berkala
};

inline const char* aiModeToString(AIMode mode) noexcept {
    switch (mode) {
        case AIMode::SoundEngineer:   return "AI Sound Engineer (Advisory Mode)";
        case AIMode::AutonomousMixer: return "AI Autonomous Mixer (Auto-Pilot Mode)";
        default:                      return "AI Sound Engineer";
    }
}

// ==============================================================================
// 2. DELAPAN MODUL AI OTOMATIS (8 CORE AUTONOMOUS MODULES)
// ==============================================================================
enum class AIModuleId {
    Gain = 0,       // Menjaga level input optimal (-18 dBFS nominal)
    EQ = 1,         // Mengatur frekuensi sesuai karakter instrumen
    Compressor = 2, // Mengontrol dinamika suara
    Gate = 3,       // Mengurangi kebocoran suara panggung
    FX = 4,         // Memilih & mengatur Reverb, Delay, Chorus
    Monitor = 5,    // Membuat mix monitor tiap musisi (Aux 1-4)
    Feedback = 6,   // Mendeteksi & mengurangi feedback (AFS Notch)
    Master = 7,     // Mengoptimalkan keseluruhan mix (True-Peak, LUFS)
    TotalModules = 8
};

inline const char* aiModuleToString(AIModuleId mod) noexcept {
    switch (mod) {
        case AIModuleId::Gain:       return "AI Gain (Auto-Trim)";
        case AIModuleId::EQ:         return "AI EQ (Spectral Balance)";
        case AIModuleId::Compressor: return "AI Compressor (Dynamics)";
        case AIModuleId::Gate:       return "AI Gate (Anti-Bleed)";
        case AIModuleId::FX:         return "AI FX (Spatial & Time)";
        case AIModuleId::Monitor:    return "AI Monitor (Stage Aux)";
        case AIModuleId::Feedback:   return "AI Feedback (AFS Killer)";
        case AIModuleId::Master:     return "AI Master (Loudness)";
        default:                     return "AI Module";
    }
}

// ==============================================================================
// 3. SAFETY LIMITS & SLEW RATE GUARD RAILS (PROTEKSI SUARA MENDADAK)
// ==============================================================================
struct AISafetyLimits {
    // Batas minimum & maksimum (Clamping Protection)
    float minInputTrimDb = -18.0f;
    float maxInputTrimDb = 18.0f;
    float minEqGainDb = -12.0f;
    float maxEqGainDb = 6.0f;
    float minCompThresholdDb = -36.0f;
    float maxCompThresholdDb = -6.0f;
    float minGateThresholdDb = -60.0f;
    float maxGateThresholdDb = -20.0f;
    float minFaderDb = -60.0f;
    float maxFaderDb = 6.0f;

    // Batas kecepatan perubahan per detik (Slewing Rate Limit)
    // Mencegah perubahan suara mendadak, zipper noise, dan audio clicks
    float maxTrimRateDbPerSec = 1.0f;       // Maks. 1.0 dB/detik
    float maxEqRateDbPerSec = 1.5f;         // Maks. 1.5 dB/detik
    float maxCompThreshRateDbPerSec = 2.0f; // Maks. 2.0 dB/detik
    float maxGateThreshRateDbPerSec = 3.0f; // Maks. 3.0 dB/detik
    float maxFaderRateDbPerSec = 1.5f;      // Maks. 1.5 dB/detik
};

inline float smoothSlew(float current, float target, float maxDelta) noexcept {
    const float diff = target - current;
    if (std::abs(diff) <= maxDelta) return target;
    return (diff > 0.0f) ? (current + maxDelta) : (current - maxDelta);
}

"""

# Insert before enum class InstrumentType
code = code.replace("enum class InstrumentType {", new_header_types + "enum class InstrumentType {")

# Update AIRecommendation struct with the 8 modules
old_rec_end = """    // Compressor recommendations
    bool compEnabled = true;
    float compThresholdDb = -18.0f;
    float compRatio = 3.0f;
    float compAttackMs = 15.0f;
    float compReleaseMs = 150.0f;
};"""

new_rec_end = """    // Compressor recommendations (Modul 3: AI Compressor)
    bool compEnabled = true;
    float compThresholdDb = -18.0f;
    float compRatio = 3.0f;
    float compAttackMs = 15.0f;
    float compReleaseMs = 150.0f;
    int compMode = 1; // 0 = VCA, 1 = Optical Tube, 2 = FET 76, 3 = Vari-Mu

    // Modul 5: AI FX (Spatial Reverb, Delay & Modulation)
    int fxSlot = 0;              // 0: Plate Reverb, 1: Tape Delay, 2: Stereo Chorus
    float fxSendDb = -12.0f;     // Send level
    float fxDecaySec = 2.2f;     // Decay time

    // Modul 6: AI Monitor (Stage Aux Sends 1-4)
    std::array<float, 4> auxSendDb = { 0.0f, -3.0f, -6.0f, -6.0f }; // Aux 1 Lead Wedge, Aux 2 Band, Aux 3 Drum IEM, Aux 4 Keys IEM

    // Modul 7: AI Feedback Suppression (AFS Notch)
    bool afsActive = false;
    float afsFreqHz = 0.0f;      // Detected ringing frequency
    float afsNotchGainDb = -12.0f;
    float afsQ = 9.0f;

    // Modul 8: AI Master & Loudness Control
    bool masterLimiterEnabled = true;
    float masterCeilingDb = -0.1f;
    float targetLufs = -14.0f;
};"""

assert old_rec_end in code
code = code.replace(old_rec_end, new_rec_end)

# In ChannelAIAnalyzer, add mode & safety state
old_analyzer_init = """    ChannelAIAnalyzer() noexcept {
        captureBuffer.resize(kMaxCaptureSamples, 0.0f);
        reset();
    }"""

new_analyzer_init = """    ChannelAIAnalyzer() noexcept {
        captureBuffer.resize(kMaxCaptureSamples, 0.0f);
        for (auto& en : moduleEnabled) en.store(true, std::memory_order_relaxed);
        reset();
    }

    AIMode getMode() const noexcept { return static_cast<AIMode>(aiMode.load(std::memory_order_relaxed)); }
    void setMode(AIMode m) noexcept { aiMode.store(static_cast<int>(m), std::memory_order_relaxed); }

    bool isModuleEnabled(AIModuleId mod) const noexcept {
        const size_t idx = static_cast<size_t>(mod);
        return (idx < 8) ? moduleEnabled[idx].load(std::memory_order_relaxed) : false;
    }
    void setModuleEnabled(AIModuleId mod, bool en) noexcept {
        const size_t idx = static_cast<size_t>(mod);
        if (idx < 8) moduleEnabled[idx].store(en, std::memory_order_relaxed);
    }

    bool isEmergencyFrozen() const noexcept { return emergencyFreeze.load(std::memory_order_relaxed); }
    void setEmergencyFreeze(bool freeze) noexcept { emergencyFreeze.store(freeze, std::memory_order_relaxed); }

    const AISafetyLimits& getSafetyLimits() const noexcept { return safetyLimits; }"""

assert old_analyzer_init in code
code = code.replace(old_analyzer_init, new_analyzer_init)

# In ChannelAIAnalyzer members, add atomic fields
old_members = """    std::atomic<size_t> writeIndex{ 0 };
    std::atomic<size_t> capturedCount{ 0 };
    std::atomic<bool> capturing{ false };
    std::atomic<bool> completed{ false };
    std::atomic<int> channelToProfile{ -1 };
};"""

new_members = """    std::atomic<size_t> writeIndex{ 0 };
    std::atomic<size_t> capturedCount{ 0 };
    std::atomic<bool> capturing{ false };
    std::atomic<bool> completed{ false };
    std::atomic<int> channelToProfile{ -1 };

    // AI Dual-Intelligence Mode & Module Activation
    std::atomic<int> aiMode{ static_cast<int>(AIMode::SoundEngineer) };
    std::array<std::atomic<bool>, 8> moduleEnabled;
    std::atomic<bool> emergencyFreeze{ false };
    AISafetyLimits safetyLimits;
};"""

assert old_members in code
code = code.replace(old_members, new_members)

with open(file_path, "w", encoding="utf-8") as f:
    f.write(code)

print("GravitonChannelAI.h updated successfully with Dual-Intelligence Modes & 8 Modules!")

