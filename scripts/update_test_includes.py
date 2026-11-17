import os, glob

test_dir = r"C:\Users\miran\Documents\WORKSPACE\OPENLIVE_REFACTORED\Tests"

replacements = {
    '#include "../Source/Main.cpp"': '#include "../Source/Application/Application.h"',
    '#include "../Source/AudioBlockTestHarness.h"': '#include "../Source/Audio/AudioBlockTestHarness.h"',
    '#include "../Source/AudioBufferOps.h"': '#include "../Source/Audio/AudioBufferOps.h"',
    '#include "../Source/BusConfigPersistence.h"': '#include "../Source/Utilities/BusConfigPersistence.h"',
    '#include "../Source/ProductionBusMath.h"': '#include "../Source/Routing/ProductionBusMath.h"',
    '#include "../Source/SafeBusGraphActivation.h"': '#include "../Source/Routing/SafeBusGraphActivation.h"',
    '#include "../Source/ScalableBusGraph.h"': '#include "../Source/Routing/ScalableBusGraph.h"',
    '#include "../Source/GravitonRTA.h"': '#include "../Source/DSP/GravitonRTA.h"',
    '#include "../Source/GravitonASIOStreamer.h"': '#include "../Source/Audio/GravitonASIOStreamer.h"',
    '#include "../Source/GravitonAutomation.h"': '#include "../Source/Utilities/GravitonAutomation.h"',
    '#include "../Source/GravitonMonitor.h"': '#include "../Source/Mixer/GravitonMonitor.h"',
    '#include "../Source/GravitonBusIntegration.h"': '#include "../Source/Mixer/GravitonBusIntegration.h"',
    '#include "../Source/GravitonAuxRoutingSnapshot.h"': '#include "../Source/Routing/GravitonAuxRoutingSnapshot.h"',
    '#include "../Source/GravitonDSP.h"': '#include "../Source/DSP/GravitonDSP.h"',
    '#include "../Source/GravitonDynamics.h"': '#include "../Source/DSP/GravitonDynamics.h"',
    '#include "../Source/GravitonMatrixRouting.h"': '#include "../Source/Routing/GravitonMatrixRouting.h"',
}

for tf in glob.glob(os.path.join(test_dir, "*.cpp")):
    with open(tf, "r", encoding="utf-8") as f:
        content = f.read()
    changed = False
    for old, new in replacements.items():
        if old in content:
            content = content.replace(old, new)
            changed = True
    if changed:
        with open(tf, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Updated {os.path.basename(tf)}")
    else:
        print(f"No changes in {os.path.basename(tf)}")

