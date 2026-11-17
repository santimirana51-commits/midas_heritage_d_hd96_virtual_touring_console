# NOVA HD Channel Architecture Audit

**Status:** Phase 1 audit complete; Phase 2 eight-band PEQ code is built and
its focused groups pass; the full CTest suite has an unrelated Dugan failure,
and the Ultimate Channel System is not complete.  
**Scope:** Current channel model, active mixer/DSP path, channel UI, state and
automation subsystems, tests, and the user-provided Ultimate Channel System
specification. This audit describes the current implementation, not hardware
equivalence.

## Executive summary

NOVA HD has a single active `MixerEngine` with 144 logical channel states and a
JUCE main mixer plus separate feature panels. The existing model already
provides independent atomic channel parameters, eight-band static PEQ and
four-band dynamic EQ, HPF/LPF, gate/compressor, sidechain, channel delay,
fader/pan, AUX sends, routing, and several scene/session workflows.

The main gaps against the locked specification are:

1. The Phase 2 implementation now provides **eight static peaking bands per
   channel** in the shared model, DSP path, editor, channel copy, stereo link,
   session/show persistence, and spreadsheet import/export. Additional EQ
   features in the specification—such as per-band filter types and bypass,
   A/B compare, and band-copy—remain unimplemented.
2. Gate and compressor are implemented, but hold, dedicated expander range,
   compressor knee/makeup, multiband dynamics, limiter, and de-esser are not
   exposed as a complete per-channel processing chain.
3. The channel UI is split among a selected-channel panel and separate EQ,
   dynamic-EQ, dynamics, and sidechain dialogs; it is not the specified
   twelve-tab full-screen editor.
4. Existing channel processing settings and show/cue/automation models have
   different levels of state coverage and validation. Their current unit tests
   do not establish full-state round trips or safe, concurrent live updates.
5. Real-time safety is mixed: core channel parameters are atomic and much
   channel DSP is preallocated, but the wider MIDI, OSC, master-processing, and
   routing facilities need a targeted thread-affinity and allocation audit
   before they can be certified against the specification.

The architecture supports extending the existing channel model and audio path;
creating a second active mixer graph is unnecessary. Use CH 01 as the
integration reference while keeping the shared parameter/DSP design valid for
all 144 channels.

## Current architecture

### Channel model and mixer

- [`Source/Main.cpp`](./Source/Main.cpp) defines `ChannelState` as a per-channel
  collection of atomics, including trim, fader, mute/solo, HPF/LPF, compressor
  and gate, eight static EQ bands, four dynamic-EQ bands, 96 AUX sends, VCA,
  sidechain, delay, polarity, and surround settings ([lines 60–176](./Source/Main.cpp)).
- `MixerEngine` owns 144 channel states and per-channel filter and dynamic-EQ
  objects; mutable filter history is not shared between channels
  ([lines 339–462](./Source/Main.cpp)).
- Stereo linking is explicit and copies selected processing and fader state
  between paired channels. This is deliberate link behavior, not independent
  channel operation while the pair is linked
  ([lines 820–940](./Source/Main.cpp)).
- The product exposes 144 **logical** channels. The current input selector and
  default input mapping cover eight physical inputs; the README also documents
  a 64-slot software output map and the absence of an equivalent 120-channel
  simultaneous hardware-I/O path ([`Source/Main.cpp`](./Source/Main.cpp),
  lines 60–61 and 2646–2700; [`README.md`](./README.md), “Implementation
  boundaries”).

### Active channel signal path

The existing implementation uses the same `MixerEngine` callback for the
channel UI and audio path. Per-channel processing includes input trim/polarity,
filters, EQ, gate/compressor (with selectable pre/post-EQ order), sidechain
filter/keying, optional Dugan processing, channel gain/pan, and bus sends.
Relevant implementation is in [`Source/Main.cpp`](./Source/Main.cpp),
particularly lines 1400–1535 and 1780–1940. The EQ/dynamics order can be
selected; it is not a fixed implementation of every order in the proposed
master signal-flow diagram.

### User interface

- `ChannelStrip` binds channel identity/name, input source, VCA, mute/solo,
  pan, processing indicators, sends, meter, and fader to the active model
  ([`Source/Main.cpp`](./Source/Main.cpp), lines 2646–3360).
- The main page-size selector supports 8, 12, 16, and 24 faders, with banking
  and custom fader layouts ([same](./Source/Main.cpp), lines 13835–13955 and
  16570–16650).
- Channel details are separate components. `SelectedChannelPanel` opens
  dedicated `EqPanel`, `DynamicEqPanel`, `DynamicsPanel`, and sidechain
  windows. There is no single full-screen, twelve-tab channel editor
  ([same](./Source/Main.cpp), lines 3623–4090, 4095–4290, 4576–5160, and
  5304–5840).
- The static EQ editor exposes one of eight bands at a time. Static channel EQ
  uses eight frequency/gain/Q triplets; it has no per-band selectable
  shelf/notch types, individual band bypass, A/B compare, or band-copy workflow
  ([same](./Source/Main.cpp), lines 92–104, 3623–4090, and 4040–4085).

## Specification gap matrix

| Area | Present implementation | Gap against the locked specification |
|---|---|---|
| Channel count and routing | 144 logical channel states, channel banking, AUX/Matrix routing, and a shared active mixer graph. | Logical channel capacity does not imply 144 physical input paths. Physical/device limits and simulated software routing remain documented in [`README.md`](./README.md). |
| Static EQ | Eight per-channel peaking bands, frequency/gain/Q controls, coefficient design, eight-band curve/editor, stereo-link, channel-copy, session/show/spreadsheet persistence, and focused tests. | Multiple per-band filter types, individual bypass, A/B compare, and dedicated band-copy remain missing. |
| Dynamic EQ | Four independently parameterized dynamic-EQ bands with above/below mode, threshold, ratio, knee, range, attack/release. | It is a separate four-band dynamic module, not a configurable eight-band static PEQ or the specified multiband compressor. |
| Gate/expander | Gate enable, threshold, ratio and attack/release; DSP reports gate reduction and accepts a sidechain key. | No separately controlled hold time or expander range is present in the channel settings audited. Gate/expander is not a complete replacement for the requested advanced module set. |
| Compressor | Enable, threshold, ratio, attack/release, mode, sidechain input and reduction metering. | Channel control set does not include dedicated knee or makeup-gain controls; there is no per-channel multiband compressor/limiter chain. |
| Multiband/limiter/de-esser | Multiband/limiter processing and de-esser implementations exist in master/FX-related modules, including `GravitonMasterX.h`, `GravitonMasterBus.h`, `GravitonDeEsser.h`, and `GravitonEffectsRack.h`. | Presence in master or FX facilities does not establish a per-channel insert with independent channel state, bypass, ordering, and per-channel metering. |
| Sidechain | Internal/external key selection and filtering are connected to the active processing path. | The README documents that external detection taps the selected source after trim/polarity, before its full channel processing. This is not a tap of the source’s full processed signal. |
| Dedicated editor | Selected-channel UI plus separate processing panels and channel navigation. | Missing the single full-screen 12-tab editor and its global copy/paste, compare A/B, module reset, undo/redo, preset, and channel-safe workflow. |
| Phantom power/input hardware | Input source and software trim/polarity/delay are represented. `phantomPower` occurs in show archive serialization. | No matching per-channel phantom-power field/control in the active `ChannelState` and mixer UI was found. A serialized archive field alone does not demonstrate active hardware control. |
| AI assistance | `ChannelAIAnalyzer` performs heuristic channel analysis and returns suggestions; tests cover selected synthetic cases. | The analyzer is not evidence of broad classification accuracy or a complete preview/approval/apply workflow. Applying suggestions and confirmation must be audited and tested as a separate UI integration. |
| State and recall | Session/show, scenes, cues, safes, automation, custom fader-layout persistence, and parameter serialization exist in different subsystems. | Full channel-state fidelity, all safety categories, malformed/versioned-file behavior, and confirmed recall are not established end-to-end. See state/automation notes below. |
| Realtime and concurrency | Main channel parameters are atomic; channel filter and dynamics history is allocated per channel. | This does not certify every processor or control path. MIDI uses a mutex in processing APIs; some master processors expose ordinary mutable controls; coefficient design and callback allocation/lock behavior need explicit thread-aware validation. |
| Display and touch | Four fader modes appear in stored screenshots at 1920×1080; the Release application build completed. | Only one display size/scale has visual evidence. 125%/150%, the other target resolutions, touch operation, and app-to-screenshot build identity are not recorded. |

## State, safety, and AI findings

- `ShowArchive` serializes selected channel state, scene filters, FX, and custom
  data, but its snapshot is not a demonstrated serialization of every active
  `ChannelState` field. The reader checks the format string but does not check
  the saved format version before use; it also mutates some show/current-scene
  state before the remaining content has been validated
  ([`Source/GravitonShowArchive.h`](./Source/GravitonShowArchive.h),
  lines 94–247 and 446–499).
- Cue safe helpers block selected categories of property changes; the helper
  does not itself implement the full live scene application/confirmation
  workflow ([`Source/GravitonCueList.h`](./Source/GravitonCueList.h),
  lines 20–84 and 255–299).
- Automation JSON round-tripping exists, but its parameter enum is limited
  relative to the specification and target validation is not sufficient to
  prove every track addresses a valid, isolated live parameter
  ([`Source/GravitonAutomation.h`](./Source/GravitonAutomation.h),
  lines 13–28, 69–85, and 197–245).
- MIDI learn, mapping groups, JSON persistence, and feedback helpers exist.
  Some MIDI processing methods take `std::mutex`, which can block if reached
  from a real-time-sensitive thread. Mapping-file schema/version and concurrent
  access require stronger validation
  ([`Source/GravitonMidiLearn.h`](./Source/GravitonMidiLearn.h),
  lines 289–435, 437–558, and 586–591).
- `ChannelAIAnalyzer` is heuristic, holds one active capture/target at a time,
  and returns recommendations rather than independently serializing or
  applying them. Tests do not establish broad accuracy or a complete operator
  approval workflow ([`Source/GravitonChannelAI.h`](./Source/GravitonChannelAI.h),
  lines 56–145 and 215–375; [`Tests/EngineTests.cpp`](./Tests/EngineTests.cpp),
  lines 2899–2963).

## Validation evidence and limits

- The command-line CMake configuration completed successfully with the
  project's declared JUCE 8.0.10 source.
- The current source built successfully for both `NOVA_HD` and
  `NOVA_HD_Tests`. The Release application launched and exposed a main window.
- CTest ran against the rebuilt test targets: **4/5 tests passed**. The DSP,
  stress, AUX snapshot, and Matrix targets passed. `NOVA_HD_Tests` failed five
  assertions, all reported in its Dan Dugan automixer test group. The new
  eight-band channel-isolation test, session/scene round-trip checks, CSV
  eight-band/legacy round-trip checks, and other test groups completed without
  failures. The Dugan failure requires a separate investigation; it is outside
  this PEQ slice, so its DSP code was not changed here.
- Four screenshots are present in `validation_screenshots/`:
  [`faders_8.png`](./validation_screenshots/faders_8.png),
  [`faders_12.png`](./validation_screenshots/faders_12.png),
  [`faders_16.png`](./validation_screenshots/faders_16.png), and
  [`faders_24.png`](./validation_screenshots/faders_24.png). Each is 1920×1080
  and was captured shortly after the Release executable timestamp. On visual
  inspection, all channel fader tracks/handles and the master fader fit
  vertically in all four screenshots. The 24-fader mode has very narrow
  controls and heavily abbreviated text. Screenshot metadata does not bind the
  capture to a particular process binary.
- These screenshots do **not** validate 1366×768, 1600×900, or 2560×1440, do
  not validate 125% or 150% scaling, and do not establish touchscreen
  behavior or absence of audio regressions.

## Phased implementation recommendations

1. **Phase 2 — Eight-band PEQ foundation:** implementation now extends the
   shared per-channel EQ state and DSP to all 144 logical channels, using CH 01
   as the response-test reference. Stereo-link semantics, the eight-band editor
   and curve, channel copy, session/show/spreadsheet persistence, and focused
   response/isolation tests are implemented. The full CTest run remains
   partially blocked by the Dugan assertions described above.
2. **Phase 3 — Processing gaps:** implement only the missing channel dynamics
   modules and parameters, each with a defined signal-flow position, bypass,
   safe parameter-transfer mechanism, tests, and performance evidence. Do not
   represent existing master/FX processors as complete channel modules.
3. **Phase 4 — Dedicated channel UI:** consolidate the existing panels into
   the specified channel editor after the parameter model stabilizes; keep
   every visible control bound to an implemented parameter and preserve
   existing touchscreen targets.
4. **Phase 5 — AI operator workflow:** separate analysis results from state
   mutation, preview recommendations, and require explicit operator approval.
5. **Phase 6 — Integration and QA:** verify per-channel isolation, all 144
   channels, saved-state fidelity, every fader mode, target display/DPI
   combinations, audio-thread constraints, and release build output.

Do not declare the Ultimate Channel System complete until those implementation
phases have code, current-build tests, and visual/audio validation evidence.
