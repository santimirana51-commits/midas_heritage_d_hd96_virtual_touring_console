import os

gui_dir = r"C:\Users\miran\Documents\WORKSPACE\OPENLIVE_REFACTORED\Source\GUI"

panel_includes = {
    "ChannelStrip.h": ['#include "GUI/Meter.h"', '#include "GUI/ChannelAIPanel.h"'],
    "DynamicEqPanel.h": ['#include "GUI/Meter.h"', '#include "GUI/EqPanel.h"'],
    "GraphicEqPanel.h": ['#include "GUI/EqPanel.h"'],
    "MatrixPlusPanel.h": ['#include "GUI/Meter.h"'],
    "SurroundPanPanel.h": ['#include "GUI/Meter.h"', '#include "GUI/SpatialRadarDisplay.h"'],
    "SelectedChannelPanel.h": [
        '#include "GUI/EqPanel.h"',
        '#include "GUI/DynamicsPanel.h"',
        '#include "GUI/ChannelAIPanel.h"',
        '#include "GUI/DynamicEqPanel.h"',
        '#include "GUI/SidechainSettingsPanel.h"'
    ]
}

for fname, incs in panel_includes.items():
    fpath = os.path.join(gui_dir, fname)
    with open(fpath, "r", encoding="utf-8") as f:
        content = f.read()

    missing = [inc for inc in incs if inc not in content]
    if missing:
        lines = content.splitlines()
        pragma_idx = -1
        for i, l in enumerate(lines):
            if "#pragma once" in l:
                pragma_idx = i
                break
        if pragma_idx != -1:
            for m in reversed(missing):
                lines.insert(pragma_idx + 1, m)
            with open(fpath, "w", encoding="utf-8") as f:
                f.write("\n".join(lines) + "\n")
            print(f"Updated {fname} with: {missing}")
        else:
            print(f"No #pragma once in {fname}")
    else:
        print(f"{fname} already has all required includes.")

