import glob, os, re

src_root = r"C:\Users\miran\Documents\WORKSPACE\OPENLIVE_REFACTORED\Source"
all_files = glob.glob(os.path.join(src_root, "**", "*.*"), recursive=True)

basename_map = {}
for f in all_files:
    rel = os.path.relpath(f, src_root).replace("\\", "/")
    base = os.path.basename(f)
    basename_map[base] = rel

print(f"Total mapped files: {len(basename_map)}")

for f in all_files:
    if not f.endswith((".h", ".cpp")):
        continue
    with open(f, "r", encoding="utf-8") as fp:
        lines = fp.readlines()
    changed = False
    for i, line in enumerate(lines):
        m = re.match(r'^(\s*#include\s*["<])([^">]+)([">].*)$', line)
        if m:
            inc = m.group(2)
            if inc.endswith(".h") and not inc.startswith("juce_") and not os.path.exists(os.path.join(src_root, inc)):
                base = os.path.basename(inc)
                if base in basename_map:
                    correct = basename_map[base]
                    lines[i] = f'{m.group(1)}{correct}{m.group(3)}\n'
                    print(f"Fixed {os.path.basename(f)}: {inc} -> {correct}")
                    changed = True

    # Also check ChannelState.h specifically to ensure GravitonDynamics.h is included
    if os.path.basename(f) == "ChannelState.h":
        content = "".join(lines)
        if "GravitonDynamics.h" not in content:
            for j, l in enumerate(lines):
                if "GravitonDSP.h" in l:
                    lines.insert(j + 1, '#include "DSP/GravitonDynamics.h"\n')
                    changed = True
                    print("Added GravitonDynamics.h to ChannelState.h")
                    break

    if changed:
        with open(f, "w", encoding="utf-8") as fp:
            fp.writelines(lines)

