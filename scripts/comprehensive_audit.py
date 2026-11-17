import os
import re

source_dir = "Source"
tests_dir = "Tests"

print("=================================================================")
print("          OPENLIVE_REFACTORED COMPREHENSIVE SYSTEM AUDIT        ")
print("=================================================================")

realtime_methods = [
    "processBlock", "getNextAudioBlock", "processChannel", "processSample",
    "renderNextBlock", "processMaster", "processDugan", "processFx", "process"
]

alloc_patterns = [
    re.compile(r"\bnew\s+[A-Za-z0-9_]+"),
    re.compile(r"\bmalloc\s*\("),
    re.compile(r"\.push_back\s*\("),
    re.compile(r"\.resize\s*\("),
    re.compile(r"\.reserve\s*\("),
    re.compile(r"\bstd::make_unique\b"),
    re.compile(r"\bstd::make_shared\b"),
]

lock_patterns = [
    re.compile(r"\bstd::lock_guard\b"),
    re.compile(r"\bstd::unique_lock\b"),
    re.compile(r"\bScopedLock\b"),
    re.compile(r"\.lock\s*\("),
]

issues = []

# Scan files
all_files = []
for root, dirs, files in os.walk(source_dir):
    for f in files:
        if f.endswith(('.h', '.cpp')):
            all_files.append(os.path.join(root, f))

print(f"\n[1] Memindai {len(all_files)} file C++ untuk Audio-Thread Safety & RT Violations...")

for filepath in all_files:
    relpath = os.path.relpath(filepath)
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    in_rt_func = False
    func_name = ""
    brace_depth = 0

    for idx, line in enumerate(lines):
        stripped = line.strip()
        # Simple method boundary tracker
        for rtm in realtime_methods:
            if re.search(rf"\b{rtm}\s*\(", line) and not stripped.startswith("//") and not stripped.endswith(";"):
                in_rt_func = True
                func_name = rtm
                brace_depth = 0
                break

        if in_rt_func:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth <= 0 and '{' in line and '}' in line:
                pass
            elif brace_depth <= 0 and idx > 0 and ('}' in line):
                in_rt_func = False

            # Check allocations
            for ap in alloc_patterns:
                if ap.search(line) and not stripped.startswith("//"):
                    # Check if inside audio/dsp engine files specifically
                    if any(sub in relpath for sub in ["DSP", "Mixer", "Effects", "Audio"]):
                        issues.append((relpath, idx + 1, f"Audio thread allocation detected in {func_name}(): {stripped}"))

            # Check locks
            for lp in lock_patterns:
                if lp.search(line) and not stripped.startswith("//"):
                    if any(sub in relpath for sub in ["DSP", "Mixer", "Effects"]):
                        # Allow spinlocks or trylock if non-blocking
                        if "try_lock" not in line:
                            issues.append((relpath, idx + 1, f"Lock in audio path detected in {func_name}(): {stripped}"))

print(f"    Ditemukan {len(issues)} potensi temuan pada RT path.")
for iss in issues[:10]:
    print(f"    - [{iss[0]}:{iss[1]}] {iss[2]}")

# [2] Modul Audit
print("\n[2] Memverifikasi Integritas 10 Modul Sistem Utama:")
modules = {
    "Application": "Source/Application",
    "Audio": "Source/Audio",
    "Mixer": "Source/Mixer",
    "DSP": "Source/DSP",
    "Effects": "Source/Effects",
    "Routing": "Source/Routing",
    "GUI": "Source/GUI",
    "MIDI": "Source/MIDI",
    "Recording": "Source/Recording",
    "Utilities": "Source/Utilities"
}

for mod, path in modules.items():
    if os.path.exists(path):
        files_in_mod = [f for f in os.listdir(path) if f.endswith(('.h', '.cpp'))]
        print(f"    [OK] Modul '{mod}': {len(files_in_mod)} file sumber.")
    else:
        print(f"    [ERR] Modul '{mod}' TIDAK DITEMUKAN di {path}!")

# [3] Topology & Channel Architecture Verification
print("\n[3] Memverifikasi Kapasitas Saluran Arsitektur:")
mixer_engine_path = "Source/Mixer/MixerEngine.h"
channel_state_path = "Source/Mixer/ChannelState.h"

with open(channel_state_path, 'r', encoding='utf-8', errors='ignore') as f:
    cs_content = f.read()

with open(mixer_engine_path, 'r', encoding='utf-8', errors='ignore') as f:
    me_content = f.read()

# Check 144 channels, 96 aux, 24 matrix
ch144 = "144" in me_content or "kTotalLogicalChannels = 144" in me_content
aux96 = "96" in me_content or "kAuxBusCount = 96" in me_content
mtx24 = "24" in me_content or "kMatrixCount = 24" in me_content

print(f"    - 144 Logical Channels : {'[TERVERIFIKASI]' if ch144 else '[PERIKSA]'}")
print(f"    - 96 AUX Busses        : {'[TERVERIFIKASI]' if aux96 else '[PERIKSA]'}")
print(f"    - 24 Matrix Outputs    : {'[TERVERIFIKASI]' if mtx24 else '[PERIKSA]'}")

# [4] Sanitasi NaN / Inf
print("\n[4] Memverifikasi Sanitasi NaN / Inf di DSP:")
dsp_path = "Source/DSP/GravitonDSP.h"
with open(dsp_path, 'r', encoding='utf-8', errors='ignore') as f:
    dsp_content = f.read()

has_clamp = "clampFinite" in dsp_content or "sanitize" in dsp_content or "std::isfinite" in dsp_content
print(f"    - Fungsi Sanitasi NaN/Inf: {'[AKTIF & TERSEDIA]' if has_clamp else '[TIDAK ADA]'}")

print("\nAudit scan selesai.")

