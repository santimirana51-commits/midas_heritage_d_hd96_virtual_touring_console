# NOVA HD Responsive Layout Fix Report

## Status

**PARTIALLY VERIFIED — All four fader modes have visual checks at 1366×768,
1600×900, and 1920×1080 window/display sizes at 100% host scaling. The 1366×768
and 1600×900 checks resized the application window on a 1920×1080 monitor; they
are not native-display tests. The requested DPI matrix and touch/audio QA remain
incomplete.**

Responsive layout changes were made in `Source/Main.cpp`. After fixing the
compile blockers encountered during validation, a Release application binary
was produced. The channel bank sidebar has since been removed and the mixer
strips now use the area it occupied. Existing screenshots predate that change,
so visual verification of the sidebar-free build remains outstanding. Native
display/DPI, touchscreen, and audio regression validation also remains
incomplete.

## Changes made

- Scale channel-strip control rows using the actual strip height and selected
  fader count (8, 12, 16, or 24), leaving the remaining area for the signal
  meter and channel fader.
- Recalculate strip width and master-panel width when the window size or fader
  count changes.
- Remove the channel bank sidebar and use its former width for channel strips.
- Scale master-panel controls to preserve at least 80 logical pixels for the
  master fader.
- Reduce the main window's minimum size so it can fit smaller display work areas.
- Restore saved 12-fader layouts correctly.
- Update the fader-count tooltip to list all four modes.

The responsive-layout changes themselves did not modify DSP processing, audio
routing, channel mapping, or persisted fader values. The separate Phase 2
eight-band PEQ work changes only channel EQ state and processing; it does not
alter routing, channel mapping, or fader levels.

## Build results

| Step | Result |
|---|---|
| CMake configure | **Passed** using the JUCE source fetched from the project's declared CMake dependency. |
| CMake Tools build | **Failed** with “Unable to configure the project”; the CMake Tools diagnostics panel reported no diagnostics. |
| Initial Release build | **Failed** with pre-existing JUCE fallback/type and source errors. The identified blockers were corrected. |
| Current-source Release build | **Passed** after channel sidebar removal for both `NOVA_HD` and `NOVA_HD_Tests`; the updated app and test executables were produced. |
| Configured CTest suite | **4/5 passed**. `GravitonDSP_Tests`, stress, AUX snapshot, and Matrix tests passed. `NOVA_HD_Tests` failed five assertions in the known automixer/gate-related test group; the responsive-layout and eight-band PEQ groups completed without failures. |
| Release application launch | **Passed** after sidebar removal; the current window was not visually inspected, so layout verification remains pending. |
| Current-session direct Release build | **Passed** with `cmake --build build --config Release --target NOVA_HD NOVA_HD_Tests` after sidebar removal. CMake Tools still reports “Unable to configure the project.” |

The initial compiler errors included JUCE type redefinitions in
`Source/GravitonConvolutionReverb.h` and duplicate global `jlimit` templates in
`Source/GravitonDuganAutomixer.h` and `Source/ScalableBusGraph.h`. Those
blockers were corrected so the application Release build could complete.

## Display and visual validation

The host monitor is **1920 × 1080 at 100% scaling** (system DPI 96). The
1366×768 and 1600×900 results below are application-window resize checks on
that host, not tests on monitors configured to those display resolutions. All
screenshots include the window frame and were captured at the named pixel size.

| Display resolution | DPI scaling | 8 faders | 12 faders | 16 faders | 24 faders |
|---|---:|---|---|---|---|
| 1366 × 768 | 100% | **Window-size visual check** | **Window-size visual check** | **Window-size visual check** | **Window-size visual check** |
| 1366 × 768 | 125% | Not tested | Not tested | Not tested | Not tested |
| 1366 × 768 | 150% | Not tested | Not tested | Not tested | Not tested |
| 1600 × 900 | 100% | **Window-size visual check** | **Window-size visual check** | **Window-size visual check** | **Window-size visual check** |
| 1600 × 900 | 125% | Not tested | Not tested | Not tested | Not tested |
| 1600 × 900 | 150% | Not tested | Not tested | Not tested | Not tested |
| 1920 × 1080 | 100% | **Visually verified** | **Visually verified** | **Visually verified** | **Visually verified** |
| 1920 × 1080 | 125% | Not tested | Not tested | Not tested | Not tested |
| 1920 × 1080 | 150% | Not tested | Not tested | Not tested | Not tested |
| 2560 × 1440 | 100% | Not tested | Not tested | Not tested | Not tested |
| 2560 × 1440 | 125% | Not tested | Not tested | Not tested | Not tested |
| 2560 × 1440 | 150% | Not tested | Not tested | Not tested | Not tested |

**Screenshots:** Existing captures in [`validation_screenshots/`](./validation_screenshots/):

- [8 faders](./validation_screenshots/faders_8.png) — 1920×1080
- [12 faders](./validation_screenshots/faders_12.png) — 1920×1080
- [16 faders](./validation_screenshots/faders_16.png) — 1920×1080
- [24 faders](./validation_screenshots/faders_24.png) — 1920×1080

Additional 100%-host-scaling window-size captures:

- 1366×768: [8](./validation_screenshots/matrix_1366x768_8.png),
  [12](./validation_screenshots/matrix_1366x768_12.png),
  [16](./validation_screenshots/matrix_1366x768_16.png),
  [24](./validation_screenshots/matrix_1366x768_24.png)
- 1600×900: [8](./validation_screenshots/matrix_1600x900_8.png),
  [12](./validation_screenshots/matrix_1600x900_12.png),
  [16](./validation_screenshots/matrix_1600x900_16.png),
  [24](./validation_screenshots/matrix_1600x900_24.png)

All listed captures predate the sidebar-removal build and therefore do not
verify the current sidebar-free layout. Native 1920×1080 captures from the
earlier build are available for [8 faders](./validation_screenshots/native_1920x1080_8.png),
[12 faders](./validation_screenshots/native_1920x1080_12.png), and
[16 faders](./validation_screenshots/native_1920x1080_16.png). The previously
verified [24-fader capture](./validation_screenshots/faders_24.png) predates the
latest build.

The captures' image metadata does not independently identify the process
binary. The new window-size captures were taken from the existing Release
executable. Visual inspection shows the channel fader tracks/handles and
master fader fit vertically in all four modes in the checked sizes. The master
fader remains visible. The 24-fader view has very narrow strips and abbreviated
text, especially in the 1366×768 window check; this does not establish that its
touch targets are comfortable or accessible. The new resized-window captures do
not establish behavior at native 1366×768/1600×900 display modes or at
125%/150% scaling.

## Remaining validation

- Resolve or separately validate the five Dan Dugan assertions before claiming
  the complete CTest suite passes; they are outside the responsive-layout and
  PEQ changes.
- Verify all four fader modes on native 1366×768, 1600×900, and 2560×1440
  displays, and at 125%/150% Windows scaling.
- Capture screenshots for the remaining native-resolution/DPI combinations.
- Confirm that channel-strip controls, all fader handles, the master fader, and
  lower controls remain visible and touchscreen-accessible.
- Check audio-engine functionality and confirm that no audio regression was
  introduced. The current full suite is not green because of the Dan Dugan
  failures listed above.
