# Changelog

## Unreleased — Monitor / Talkback

- Added independent Control Room and Headphone/Listen Bus source selection for Main L/R, AUX 1–96, Matrix 1–24, direct 2TR inputs 1–2, Playback, and the software OSC generator, plus optional Headphone source-follow.
- Routed the selected monitor sources through independent level, Dim, Mono, Mute, four-band parametric EQ, and actual DSP peak metering without changing channel EQ or Main level.
- Added selectable talkback input, Control Room assignment, and a session-persisted Monitor/Talkback panel state; retained the existing PFL/AFL signal taps, added Solo Clear and global PFL/AFL selection, and made In-Place Solo an explicit Main-bus gating option with Solo Safe respected.
- Connected explicitly assigned Stage Box Local I/O outputs 1–16 to active audio callback channels so Monitor Left/Right patching produces a real device signal. Remote AES50/Dante ports remain unconnected without their hardware transport.
- Added signal-based coverage for source selection, mono/mute, four-band EQ response, talkback-to-Control-Room isolation from Main, patched local monitor output, session persistence, and solo isolation behavior.
- Documented that only two stereo monitor paths are active; four independent headphones, speaker calibration/measurement, monitor delay, and remote network transport are unavailable.
- Redesigned the Monitor/Talkback dialog as a resizable console surface with source shortcuts, independent Control Room and Headphone controls, live L/R meters, monitor EQ response display, PFL/AFL and solo controls, and a true momentary push-to-talk button; unavailable speaker A/B switching and monitor delay are labeled rather than represented as active controls.

## Unreleased — Surround Processing

- Extended the existing surround panner with per-channel distance control, smoothed inverse-distance attenuation, and session/copy-paste persistence.
- Added an optional sample-rate-aware second-order LFE low-pass with selectable 40–120 Hz crossover; the filter is applied only to the explicit LFE send and does not claim bass redirection.
- Synchronized the Surround panel's channel selector with the shared mixer channel selection and added live reporting of Stage Box speaker-source assignments and active device output count.
- Clarified active layouts (Stereo, LCR, Quad, 5.1, 7.1, 7.1.4) and unavailable 9.1.6/Auro/custom formats and hardware multichannel output, matching the engine's current DSP path.
- Added signal-based tests for distance attenuation, LFE pass-band/stop-band behavior, supported crossover rates, and bypass pass-through.

## Unreleased — Routing / Patch Matrix

- Added a resizable Routing View with a 16×16 banked virtual crosspoint grid, active-route highlighting, source/destination details, selected-route overview, batch enable/invert/disable, shift multi-selection, copy/paste, and clear.
- Connected Input→AUX, AUX→Matrix, and Main L/R→Matrix crosspoints to the existing channel state and lock-free AUX/Matrix snapshot publication; supported route level, mute, and Input→AUX PRE/POST controls affect live DSP routing.
- Added versioned `.novaroute` save/load presets for all modeled channel sends, AUX/Matrix masters and crosspoints, and software Stage Box patch assignments. Loading validates the complete document before mutating engine state.
- Integrated Routing navigation with Console View and the existing Stage Box Patch I/O panel. Explicitly identifies fixed or unavailable Input→Main, Input→Matrix, AUX→Main, Matrix→Main, Insert, Direct Out, Center, pan, and non-fader tap paths.
- Added channel-color indicators, restored direct access to Matrix bus-master controls, and consolidated each batch route action to a single routing-snapshot publication.
- Added Routing View tests for crosspoint state publication, three live route families, Stage Box patch preset round-trip, and transactional rejection of invalid preset data.

## Unreleased — Automation View

- Added a resizable Automation View with multitrack parameter lanes, 30 Hz transport, linear/smooth/step curve playback, point insertion and dragging, timeline zoom range, and direct access to the existing cue-list editor.
- Bound playback to existing channel, AUX, Matrix, Main, EQ, dynamics, pan, and input-gain atomics with target validation and DSP range clamping; playback runs on the control/UI thread and does not block the audio callback.
- Persisted cue playlists and automation tracks in session files and in `.novashow` custom data; invalid automation/show data is rejected before applying a loaded session or show.
- Added automation interpolation, bounds, transport, and serialization regression coverage. Live write/touch/latch capture, DSP-rate smoothing, and multi-parameter scene crossfades remain unavailable.

## Unreleased — Console View

- Added a resizable Console View workspace with Overview, Inputs, AUX, Matrix, Main, VCA, POP, Mute Groups, and Signal Flow pages plus bottom navigation wired to existing application surfaces.
- Connected channel/bus meters and clip indicators, AUX/Matrix masters, stereo Main master, VCA masters/assignments, POP assignments/spill, and mute-group masters to existing engine state.
- Added live device/engine status and channel signal-flow visualization driven by actual channel configuration; unavailable Center, CPU/DSP telemetry, and per-channel Insert/Direct Out paths are identified explicitly.
- Persisted POP group names in session files while retaining backward compatibility with sessions that omit the optional name list.
- Added Console View tests for channel/bus selection, atomic peak snapshots, Main metering, VCA/POP state changes, processing order, and live status reporting.

## Unreleased — Channel View

- Added a scrollable color-coded 144-channel selector and shared selected-channel state across Channel, Console, and FOH strip interaction; retained bank/previous/next navigation, channel identity, metadata, copy/paste, and reset.
- Expanded Channel Sends to eight AUX controls per page with actual send enable/mute, pre/post-fader tap, send-level updates, stereo-linked peer updates, and live send peak readouts.
- Kept a persistent right-side channel strip with PFL/AFL plus actual stereo Main L/R meters and master-level control; unavailable master mute/solo are labeled rather than simulated.
- Embedded the existing channel EQ and dynamics editors, preserving their DSP parameter pathways and interactive EQ curve behavior.
- Added real-time raw input, post-trim, channel output, clipping, and gate/compressor reduction readouts.
- Persisted channel color, instrument tag, and POP assignment in session files; existing files without these optional properties remain compatible.
- Clearly labels insert status as FX-Rack-owned and Direct Out/Matrix direct sends as unavailable because the mixer has no per-channel models for them. Phantom power, PAD, M/S, width, unsupported EQ band types, gate hold/range, and compressor knee/makeup remain unavailable rather than decorative.
- Added Channel View selection synchronization, AUX send controls, PFL/AFL, master-level binding, and channel state round-trip coverage to `Tests/EngineTests.cpp`.

## Unreleased — Sends View

- Added a resizable Sends View workspace accessible from the Console View bottom navigation, with AUX/Matrix/Main target selection, 8/16/24 channel banks, search/rename bus selection, and Sends-on-Faders, read-only overview, and bus-master modes.
- Connected AUX send level, enable, mute, and supported pre/post-fader tap controls to the existing lock-free routing snapshot; added AUX send peak metering and stereo-linked send updates.
- Connected AUX-to-Matrix and Main L/R-to-Matrix crosspoints, bus master level/mute, and confirmed Clear Sends operations to existing mixer routing state.
- Persisted AUX send enable/mute state, bus names, send display/default preferences, tap positions, and AUX/Matrix routing configuration with backward-compatible loading of older sessions.
- Explicitly identifies unsupported pre/post-processing tap points, mono send-pan, direct channel-to-Matrix sends, Main Center, and independent Main send controls rather than exposing inactive controls.
- Added Sends View tests for actual AUX routing/mute snapshots, Matrix crosspoints/master and clear behavior, Main Center protection, and session round-trip persistence.
