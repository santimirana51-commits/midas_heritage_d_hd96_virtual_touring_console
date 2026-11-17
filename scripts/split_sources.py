import os

orig_path = r'C:\Users\miran\Documents\WORKSPACE\NOVA_HD_ORIGINAL\Source\Main.cpp'
dest_root = r'C:\Users\miran\Documents\WORKSPACE\OPENLIVE_REFACTORED\Source'

with open(orig_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

def get_slice(start, end):
    return ''.join(lines[start-1:end])

def save_header(rel_path, includes, code, ns="nova"):
    full_path = os.path.join(dest_root, rel_path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n")
        for inc in includes:
            f.write(inc + "\n")
        f.write("\n")
        if ns:
            f.write(f"namespace {ns} {{\n\n")
        f.write(code.rstrip())
        f.write("\n")
        if ns:
            f.write(f"\n}} // namespace {ns}\n")
    print(f"Saved {rel_path} ({len(code.splitlines())} lines)")

# 1. ChannelState.h
save_header("Mixer/ChannelState.h", [
    '#include "DSP/GravitonDSP.h"',
    '#include <array>',
    '#include <atomic>',
    '#include <cstdint>',
    '#include <juce_core/juce_core.h>'
], get_slice(51, 219))

# 2. RecordingWorker.h
save_header("Recording/RecordingWorker.h", [
    '#include <juce_core/juce_core.h>',
    '#include <juce_audio_basics/juce_audio_basics.h>',
    '#include <juce_audio_formats/juce_audio_formats.h>',
    '#include <atomic>',
    '#include <cstdint>',
    '#include <memory>',
    '#include <vector>'
], get_slice(220, 334))

# 3. MixerEngine.h
save_header("Mixer/MixerEngine.h", [
    '#include "Mixer/ChannelState.h"',
    '#include "Recording/RecordingWorker.h"',
    '#include "Audio/AudioBufferOps.h"',
    '#include "Audio/AudioBusOutputRouter.h"',
    '#include "Audio/GainSmoothing.h"',
    '#include "Audio/GravitonASIOStreamer.h"',
    '#include "Audio/GravitonOfflineBounce.h"',
    '#include "Audio/GravitonOscServer.h"',
    '#include "DSP/DspCoefficients.h"',
    '#include "DSP/GravitonChannelAI.h"',
    '#include "DSP/GravitonDSP.h"',
    '#include "DSP/GravitonDeEsser.h"',
    '#include "DSP/GravitonDN360.h"',
    '#include "DSP/GravitonDynamicEQ.h"',
    '#include "DSP/GravitonDynamics.h"',
    '#include "DSP/GravitonGraphicEQ.h"',
    '#include "DSP/GravitonRTA.h"',
    '#include "DSP/GravitonSignalGenerator.h"',
    '#include "DSP/GravitonSpectrogram.h"',
    '#include "DSP/PeakingEq.h"',
    '#include "Effects/GravitonConvolutionReverb.h"',
    '#include "Effects/GravitonDVR2Reverb.h"',
    '#include "Effects/GravitonEffectsRack.h"',
    '#include "Effects/GravitonHybridReverb.h"',
    '#include "Effects/GravitonSCFChorus.h"',
    '#include "GUI/FaderLayout.h"',
    '#include "Mixer/GravitonBusIntegration.h"',
    '#include "Mixer/GravitonDuganAutomixer.h"',
    '#include "Mixer/GravitonMasterBus.h"',
    '#include "Mixer/GravitonMonitor.h"',
    '#include "Mixer/GravitonSidechain.h"',
    '#include "Mixer/GravitonTalkback.h"',
    '#include "Mixer/MixMath.h"',
    '#include "Routing/GravitonAuxRoutingSnapshot.h"',
    '#include "Routing/GravitonMatrixPlus.h"',
    '#include "Routing/GravitonMatrixRouting.h"',
    '#include "Routing/GravitonPatchMatrix.h"',
    '#include "Routing/GravitonSurround.h"',
    '#include "Routing/ProductionBusMath.h"',
    '#include "Routing/SafeBusGraphActivation.h"',
    '#include "Routing/ScalableBusGraph.h"',
    '#include "Utilities/GravitonAutomation.h"',
    '#include "Utilities/GravitonCueList.h"',
    '#include "Utilities/GravitonManchino.h"',
    '#include "Utilities/GravitonShowArchive.h"',
    '#include <algorithm>',
    '#include <array>',
    '#include <atomic>',
    '#include <cmath>',
    '#include <functional>',
    '#include <juce_audio_devices/juce_audio_devices.h>',
    '#include <juce_audio_utils/juce_audio_utils.h>',
    '#include <utility>',
    '#include <vector>'
], get_slice(335, 2388))

# 4. MixerMidiCallback.h
save_header("MIDI/MixerMidiCallback.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_audio_devices/juce_audio_devices.h>',
    '#include <array>',
    '#include <atomic>'
], get_slice(2389, 2545))

# 5. Meter.h
save_header("GUI/Meter.h", [
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include <atomic>'
], get_slice(2546, 2566))

# 6. ChannelStrip.h
save_header("GUI/ChannelStrip.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include "GUI/Meter.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include <juce_audio_utils/juce_audio_utils.h>'
], get_slice(2567, 3161))

# 7. VcaPanel.h
save_header("GUI/VcaPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(3162, 3281))

# 8. EqPanel.h
save_header("GUI/EqPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(3282, 3762))

# 9. DynamicsPanel.h
save_header("GUI/DynamicsPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(3763, 3977))

# 10. ChannelAIPanel.h
save_header("GUI/ChannelAIPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(3978, 4231))

# 11. DynamicEqPanel.h
save_header("GUI/DynamicEqPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(4232, 4880))

# 12. SidechainSettingsPanel.h
save_header("GUI/SidechainSettingsPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(4881, 5001))

# 13. SelectedChannelPanel.h
save_header("GUI/SelectedChannelPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include "GUI/EqPanel.h"',
    '#include "GUI/DynamicsPanel.h"',
    '#include "GUI/ChannelAIPanel.h"',
    '#include "GUI/DynamicEqPanel.h"',
    '#include "GUI/SidechainSettingsPanel.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(5002, 6264))

# 14. MatrixRoutingPanel.h
save_header("GUI/MatrixRoutingPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(6265, 6517))

# 15. OscillatorPanel.h
save_header("GUI/OscillatorPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(6518, 6752))

# 16. GraphicEqPanel.h
save_header("GUI/GraphicEqPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(6753, 6872))

# 17. MonitorPanel.h
save_header("GUI/MonitorPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(6873, 7053))

# 18. EffectsRackPanel.h
save_header("GUI/EffectsRackPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(7054, 8435))

# 19. ManchinoPanel.h
save_header("GUI/ManchinoPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(8436, 8810))

# 20. DuganAutomixerPanel.h
save_header("GUI/DuganAutomixerPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(8811, 8970))

# 21. StageBoxPatchPanel.h
save_header("GUI/StageBoxPatchPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(8971, 9523))

# 22. SpatialRadarDisplay.h
save_header("GUI/SpatialRadarDisplay.h", [
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include <atomic>'
], get_slice(9524, 9752))

# 23. SurroundPanPanel.h
save_header("GUI/SurroundPanPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include "GUI/SpatialRadarDisplay.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(9753, 10121))

# 24. TalkbackPanel.h
save_header("GUI/TalkbackPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(10122, 10442))

# 25. OscServerPanel.h
save_header("GUI/OscServerPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(10443, 10618))

# 26. MatrixPlusPanel.h
save_header("GUI/MatrixPlusPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(10619, 11060))

# 27. DAWStreamingPanel.h
save_header("GUI/DAWStreamingPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(11061, 11242))

# 28. POPulationGroupsPanel.h
save_header("GUI/POPulationGroupsPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(11243, 11378))

# 29. CueListPanel.h
save_header("GUI/CueListPanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(11379, 11656))

# 30. ASIOBridgePanel.h
save_header("GUI/ASIOBridgePanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(11657, 11808))

# 31. OfflineBouncePanel.h
save_header("GUI/OfflineBouncePanel.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>'
], get_slice(11809, 12337))

# 32. UserFaderLayoutEditor.h
save_header("GUI/UserFaderLayoutEditor.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include <vector>',
    '#include <functional>'
], get_slice(12338, 12531))

# 33. Panels.h (umbrella header)
panels_h = """#pragma once
#include "GUI/Meter.h"
#include "GUI/ChannelStrip.h"
#include "GUI/VcaPanel.h"
#include "GUI/EqPanel.h"
#include "GUI/DynamicsPanel.h"
#include "GUI/ChannelAIPanel.h"
#include "GUI/DynamicEqPanel.h"
#include "GUI/SidechainSettingsPanel.h"
#include "GUI/SelectedChannelPanel.h"
#include "GUI/MatrixRoutingPanel.h"
#include "GUI/OscillatorPanel.h"
#include "GUI/GraphicEqPanel.h"
#include "GUI/MonitorPanel.h"
#include "GUI/EffectsRackPanel.h"
#include "GUI/ManchinoPanel.h"
#include "GUI/DuganAutomixerPanel.h"
#include "GUI/StageBoxPatchPanel.h"
#include "GUI/SpatialRadarDisplay.h"
#include "GUI/SurroundPanPanel.h"
#include "GUI/TalkbackPanel.h"
#include "GUI/OscServerPanel.h"
#include "GUI/MatrixPlusPanel.h"
#include "GUI/DAWStreamingPanel.h"
#include "GUI/POPulationGroupsPanel.h"
#include "GUI/CueListPanel.h"
#include "GUI/ASIOBridgePanel.h"
#include "GUI/OfflineBouncePanel.h"
#include "GUI/UserFaderLayoutEditor.h"
"""
with open(os.path.join(dest_root, "GUI", "Panels.h"), "w", encoding="utf-8") as f:
    f.write(panels_h)
print("Saved GUI/Panels.h")

# 34. MainComponent.h
save_header("GUI/MainComponent.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include "MIDI/MixerMidiCallback.h"',
    '#include "GUI/Panels.h"',
    '#include "GUI/NovaConsoleSurfaceJuce.h"',
    '#include "GUI/NovaLookAndFeel.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include <juce_audio_utils/juce_audio_utils.h>'
], get_slice(12532, 15910))

# 35. Application.h
save_header("Application/Application.h", [
    '#include "Mixer/MixerEngine.h"',
    '#include "MIDI/MixerMidiCallback.h"',
    '#include "GUI/MainComponent.h"',
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include <juce_audio_utils/juce_audio_utils.h>'
], get_slice(15911, 15980))

# 36. Main.cpp
main_cpp = """#include "Application/Application.h"

#if !defined(NOVA_HD_TESTING)
START_JUCE_APPLICATION(nova::Application)
#endif
"""
with open(os.path.join(dest_root, "Application", "Main.cpp"), "w", encoding="utf-8") as f:
    f.write(main_cpp)
print("Saved Application/Main.cpp")

print("All components extracted successfully!")

