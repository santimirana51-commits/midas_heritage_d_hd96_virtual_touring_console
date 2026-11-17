# NOVA HD Digital Mixing System

Windows 10/11 x64 live sound mixer prototype using C++20, JUCE 8, and the Graviton Mk 2 64-bit DSP audio engine. The MIDAS Heritage-D HD96-24 specification is a design reference; this software is not an equivalent console or hardware implementation.

## Implementation boundaries

The following limitations apply to the current source, regardless of features described later in this document:

- The mixer controls expose all 96 AUX sends in 4-send banks and all 24 Matrix buses through the Matrix routing selector. The master surface displays 4 AUX and 2 Matrix strips at a time; the software output map has 64 physical-output slots, so this is not 120-channel simultaneous hardware I/O.
- External sidechain detection taps the selected source's input after trim/polarity, before its channel processing; it is not a tap of the source channel's full processed signal.
- The Stage Box AES50/Dante patch matrix is a software routing model. It does not implement AES50, Dante, network transport, or external hardware clock/PLL synchronization. Its clock-lock and jitter values are simulated model state, not measurements of a device or network.
- The ASIO-named FX bridge is a software ring-buffer stub, not an ASIO driver or USB Audio Class device backend. The separate 64×64 DAW streamer is an in-process software streaming model; its tests do not establish external DAW, ASIO, or USB hardware interoperability. Enabling `USE_ASIO_SDK` only configures SDK headers and does not provide a device backend.
- On Windows, the application selects JUCE's `Windows Audio` (WASAPI shared-mode) device type when available, then initializes the existing `AudioDeviceManager` and `MixerEngine` callback. JUCE provides the WASAPI implementation; the separate custom Exclusive backend is not part of this project. The startup fallback retries with 2 inputs/2 outputs and logs a second initialization failure.
- The Stage 28-strip surface is connected to the existing `MixerEngine`. The separate Stage 8 shadow graph and Stage 9 routing controller are not a second active audio path; routing remains on the existing mixer implementation until an explicit migration is undertaken.
- MIDI control supports incoming CC learn/mappings and optional Mackie Control-compatible output feedback for the first eight visible faders, including 14-bit pitch-wheel motor positions and Mute/Solo LEDs. Output-port availability and individual controller firmware behavior depend on the attached hardware; redundant device failover is not implemented.
- Surround panning and binaural monitoring are processed in software, but a multichannel hardware output path is not available in this build.
- The RTA analyzer receives the post-master Main L/R signal pair. Its two analyzer inputs are not independently routed source and microphone taps, so the application does not currently provide a source-versus-microphone transfer-function measurement.
- Eight custom fader layouts can define ordered channel subsets. They are saved to the user profile and included in session files; GUI automation and cross-computer session portability have not been independently verified.
- Scene recall crossfades interpolate channel, Main, VCA, AUX, and Matrix fader levels; non-fader scene parameters are recalled immediately. Cue-list definitions can be edited and reordered in the offline playlist editor and exported to `.novashow` archives.

## Console Workflow
- The main surface starts at 1280×760 and is resizable. Choose 8, 16, or 24 faders per page and navigate all 144 input channels or any of eight custom layouts.
- The FOH surface uses the graphite design system in `Source/UI/Theme/NovaLookAndFeel.h`, with cyan selection, green active DSP, amber warning, and red mute/clip emphasis. Each strip shows its channel number/name, source selector, live engine meter, HPF/Gate/Compressor/EQ activity, pan, polarity, Mute, Solo, and numeric fader level. The Select and Gate buttons write to the selected-channel UI state and the actual channel gate parameter; the fader and pan remain connected to mixer state.
- Hidden channel-bank strips skip their periodic UI synchronization and repaint work; visible strips update from atomic engine snapshots on the existing UI timer. This does not change the audio callback or add a processing path.
- The main header groups existing controls into Channel, Bus, Routing, FX, Scenes, Tools, and System menus. It reports the selected scene slot and only identifies an audio device as online when both the JUCE device and MixerEngine are active. CPU/DSP load is shown as unavailable because the engine does not currently publish a measured load value.
- The 24-slot FX rack displays each slot's effect type, bypass state, and engine input/output peak meters. The selected slot's editor includes controls for the implemented effect; Convolution and Hybrid Reverb expose their existing parameter sets. These peak readings come from the FX engine, not generated UI data.
- **CONSOLE SURFACE** opens the added 28-strip Stage surface across six input banks. Its faders and mute controls read/write the existing mixer channel atomics; channel meters, names, mute, solo, and stereo-linked fader/mute changes follow the active engine state. This is a second view of the existing engine, not a second audio graph or hardware motor-fader implementation.
- The standalone Stage 8-14 graph, patchbay, Windows-device-panel, and DSP prototypes were removed from this source tree because they were not wired into the application and depended on graph/device implementations that are not present here. The active application continues to use its existing `MixerEngine`; Windows WASAPI support comes from JUCE's device manager.
- Create a custom layout by naming it, adding/removing channels, and reordering its faders. Empty layouts are valid; duplicate and out-of-range channel entries are rejected when loading.
- POP/VCA spill remains a temporary view and takes precedence over the selected custom layout.
- Custom layouts are stored in `%APPDATA%\NOVA_HD\fader-layouts.json` and embedded in session files for portability.
- Manchino batch edit can target any of the 96 AUX sends for the selected channels.
- Dugan automixing, Channel AI analysis, RTA, show/session handling, and JUCE device selection remain available through their existing panels. There is no feedback-suppression engine, automatic backup-device failover, or measured CPU/DSP-load API in this build; the interface does not simulate these capabilities.

## Available Console Surfaces
- The current application has a main FOH mixer surface and separate feature panels opened from its navigation menus and controls. It does **not** contain the unified Overview/Inputs/AUX/Matrix/Main/VCA/POP/Signal Flow **CONSOLE VIEW** workspace described in earlier documentation.
- Use the main fader-mode selector for Main or AUX Sends on Faders, the AUX master bank selector for AUX bus masters, and the Matrix routing controls for supported Matrix paths. The selected-channel panel and routing/patch editors are separate windows, not pages inside a unified console dashboard.
- Device/sample-rate/engine status and the selected scene slot are shown in the main header. CPU/DSP load is marked unavailable because the engine does not expose a measured load value.

## Channel View
- Open **SELECTED CH** from the Channel navigation menu. The resizable graphite workspace has a scrollable 144-channel list, previous/next and 8-channel bank navigation, channel name/color/instrument tag, input/VCA/POP assignment, copy/paste/reset, and a shared selected-channel index synchronized with the FOH surface.
- The persistent right strip contains the selected channel fader, pan/balance, mute/solo, PFL/AFL selection, channel output meter, plus actual stereo Main L/R peak meters and master level control. Main mute/solo are unavailable because the mixer does not model those master controls.
- **CONFIG** controls the existing channel trim, polarity, input delay (0–500 ms), HPF/LPF enable/cutoff/slope, variable all-pass phase, and processing order. Changes write the channel atomics consumed by the mixer/DSP path; stereo-linked channels follow the existing link behavior.
- **EQ** embeds the eight-band parametric EQ editor with a 20 Hz–20 kHz response graph and draggable frequency/gain/Q interaction. **DYNAMICS** embeds the gate/expander and compressor controls already implemented by the channel engine.
- **SENDS** displays eight AUX sends per bank (AUX 1–8 through AUX 89–96), with actual send level, enable, send mute, supported pre/post-fader tap, and per-send peak meter. Changes update stereo-linked peers and the lock-free AUX routing snapshot. The engine does not model direct input-to-Matrix sends; AUX-to-Matrix routing remains in the Matrix routing view. Send pan is unavailable because AUX routing is mono.
- **ROUTING** edits input, VCA, POP, mute-group, and solo-safety assignments. **METER** displays raw input, post-trim, channel output, clipping, and dynamics-reduction readings.
- **INSERT** reports actual active channel-insert slots; slot type, bypass, send/return, and insert routing remain managed by the Virtual FX Rack. **DIRECT OUT** is informational because per-channel direct output routing is not present in the current output model.
- Session save/load now includes channel color, instrument tag, and POP assignment alongside the existing name, input, VCA, delay, EQ, dynamics, routing, and fader state. Older sessions without the new metadata remain loadable.
- Not implemented and intentionally not presented as active controls: phantom power, PAD, M/S mode, stereo width, per-channel insert enable/return controls, Direct Out and output tap-off routing, direct channel-to-Matrix routing, EQ band enable/filter-type/flat switches, gate range/hold, compressor knee/makeup gain, and stereo master mute/solo. These need engine/state support before the UI can safely expose them.

## Routing / Patch Matrix
- Open **ROUTING** from the console navigation or main toolbar. The resizable graphite matrix shows 16×16 virtualized crosspoints with source/destination banks, channel-color indicators, active route highlighting, shift multi-selection, route details, batch enable/invert/disable, copy/paste, clear, and a live selected-route overview. Batch changes publish one routing snapshot per action; **MATRIX BUS** opens the existing Matrix bus-master controls.
- Editable paths are **Input→AUX** (144×96), **AUX→Matrix** (96×24), and **Main L/R→Matrix** (2×24). Input→AUX level, enable, mute, and supported pre/post-fader tap write the existing channel state and publish the bounded AUX routing snapshot. AUX/Main→Matrix gain and mute publish the existing lock-free Matrix snapshot. These paths preserve the audio callback's no-allocation/no-blocking behavior.
- Save/Load writes a versioned `.novaroute` JSON preset for channel sends, AUX/Matrix masters and crosspoints, plus the current software Stage Box input/output patch assignments. Preset loading validates schema, counts, finite gain ranges, and port/bus indices before applying any state; invalid presets leave the live configuration unchanged. Session save/load continues to include routing as well.
- **PATCH I/O** opens the existing Stage Box mapping editor. It is a software patch model only; AES50/Dante device transport is not available in this build.
- Routing intentionally does not expose unsupported active crosspoints: Input→Main is fixed by the channel path; direct Input→Matrix, AUX→Main, Matrix→Main, insert send/return patching, and per-channel Direct Out are not independently routed by the current engine. Main Center, pan/balance, filter/EQ/insert/dynamics tap points, and phase controls are unavailable for these bus paths. Input→AUX PRE/POST changes only the supported fader tap; both are post-processing. The supported forward routes are acyclic and contain no bus-return path.

## Sends Controls
- Select Main or an AUX bus in the main fader-mode selector to use Sends on Faders. All 96 AUX sends are available; the channel strips expose four send controls at a time. The AUX master bank selector controls the displayed AUX masters, and the Matrix routing controls edit the Matrix paths implemented by the engine. There is no separate unified Sends workspace or independent Main Center bus.
- AUX send level, enable, mute, and the engine-supported pre-fader/post-fader tap are written to channel state and published through the bounded AUX routing snapshot. The tap choices are both post-channel-processing; filter/EQ/insert/dynamics tap points are explicitly unavailable. Send mute is distinct from channel mute, while the current channel mute gate also gates AUX sends.
- Matrix controls edit actual AUX-to-Matrix and Main L/R-to-Matrix crosspoints, Matrix master level, and mute. Matrix routing does not expose a direct input-channel-to-Matrix send. Clear Sends confirms before setting the selected AUX or Matrix crosspoints to silence.
- Meter readouts use engine peak/clip atomics. Bus master levels/mutes, AUX and Matrix names, send levels/enables/mutes/taps, Matrix crosspoints, and display/default-send options are included in session save/load; missing optional fields in older sessions retain compatibility.
- Send pan/stereo balance and independent AUX/Matrix bus solo are unavailable because the engine does not model those controls and AUX/Matrix routing is mono. Main L/R share the engine's stereo master gain and Main Center is not modeled as an independent bus, so the view does not offer a fake Main send fader or Center control.

## Scene Automation & Cue Playlist
- Cue List GO/PREV follows the playlist order and recalls each cue's mapped one of eight console scene slots. Cue rows support drag-and-drop ordering, cue name and scene-slot editing, and per-cue 0–10 second crossfade duration.
- Configure per-cue FADERS, EQ, DYNAMICS, ROUTING, and MUTES safes; STOP cancels an active fader crossfade at its current level.
- The AUTOMATION workspace provides a resizable multitrack timeline, transport, point insertion by double-click, point dragging, and linear/smooth/step curve evaluation. Playback evaluates enabled, unmuted tracks (solo tracks take precedence) and writes supported parameter atomics from a 30 Hz UI/control timer; the audio callback does not perform automation file I/O or blocking work.
- The automation model includes EQ frequency/gain/Q tracks with a per-band index; live binding to the active eight-band channel EQ remains unverified. Other documented automation tracks include channel/AUX/Matrix/Main faders, channel pan, AUX send, input gain, compressor threshold/ratio, and gate threshold. Live write/touch/latch capture modes and DSP-rate smoothing are not available.
- Session files persist the cue playlist and automation tracks. SAVE .SHOW / LOAD .SHOW stores playlist and automation metadata in the `cuePlaylist` and `automation` custom-data entries of the NOVA `.novashow` archive. The playlist references the console's eight scene slots; it does not embed or import their scene snapshots.
- Manual scene recall supports full, mix-only, processing-only, and fader/pan/mute scopes. Channel Mute Safe and Solo Safe remain additional protections.
- Scene-recall crossfade continues to interpolate fader families only; multi-parameter scene crossfade is not yet implemented.

---

## Feature History (unversioned)
1. **TC Electronic Master X HD / System 6000 Multiband Dynamics Processor (MIDAS Chapter 18):**
   - **Phase-Coherent Linkwitz-Riley 24 dB/octave (LR4) 3-Way Crossover:**
     - High-order crossover splitting audio into Low, Mid, and High bands with swept cutoff frequencies ($40\text{–}1000\text{ Hz}$ Low/Mid, $1\text{–}16\text{ kHz}$ Mid/High).
     - Low band incorporates 2nd-order Mid/High All-Pass phase compensation, delivering mathematically flat $0.000\text{ dB}$ magnitude response across $20\text{ Hz} - 20\text{ kHz}$ without comb-filtering.
   - **Independent 3-Band Soft-Knee Dynamics Engine:**
     - Independent controls per band: Threshold ($-40\text{ to }0\text{ dB}$), Ratio ($1:1\text{ to }20:1$), Attack ($0.1\text{–}100\text{ ms}$), Release ($10\text{–}2000\text{ ms}$), Soft Knee ($0\text{–}12\text{ dB}$), and Makeup Gain ($-12\text{ to }+18\text{ dB}$).
     - Stereo-linked peak detectors, Solo/Mute band auditioning logic, and real-time atomic Gain Reduction meters.
   - **Brickwall True-Peak Lookahead Limiter:**
     - $1.5\text{ ms}$ circular lookahead delay buffer with smooth pre-emptive gain reduction ramp and strict $-0.1\text{ dBFS}$ true-peak ceiling, guaranteeing intersample peak immunity for Master L/R broadcast feeds and hearing protection for stage In-Ear Monitors.
   - **Virtual FX Rack UI Integration:**
     - 4-tab interactive editor (Low Band, Mid Band, High Band, Xover & Limiter) with real-time composite multi-metering.

2. **Vintage EMT 140 Plate Reverb & Klark Teknik DN780 Digital Plate (MIDAS Chapter 21):**
   - **Physical Electromechanical Steel Plate Damping:** $0.5\text{ s}$ to $12.0\text{ s}$ decay time modeled after the physical damping pad.
   - **Dattorro High-Density Tank Architecture:** 4-stage prime input diffusers and figure-eight cross-coupled modulated delay network for instant, lush plate modal density without metallic flutter.
   - **Input Drive Saturation:** Warm tube/transistor driver preamp emulation ($0\text{ to }24\text{ dB}$).
   - **Dual Operating Modes:** Toggle between vintage EMT 140 warm analog plate character and Klark Teknik DN780 1980s crisp digital plate tank.
   - **Filtering:** High-damping pad absorption filter ($1\text{–}16\text{ kHz}$) and low-cut rumble filter ($20\text{–}500\text{ Hz}$).

3. **Vintage Dual-Tank Spring Reverb (MIDAS Chapter 22):**
   - **Dual Torsion Coil Emulation:** Parallel helical spring tanks (Tank A ~33.7 ms and Tank B ~41.3 ms) with tank tray mechanical cross-coupling.
   - **Transient Dispersion Network:** 6-stage cascaded dispersive all-pass filters producing the iconic spring "chirp / drip / boing" on snare drum and electric guitar transients.
   - **Spring Tension & Tone Control:** Modulates helical coil dispersion resonant frequency and high-frequency coil tone damping.
   - **Overdrive Saturation:** Tube amp spring driver overload saturation.

4. **Comprehensive Talkback & Comm Bus Intercom Matrix (MIDAS Chapter 32):**
   - **Engine-Wide Intercom Routing:**
     - Discrete routing crosspoint to 96 AUX buses (Musicians' Stage In-Ear Monitors), 24 Matrix outputs, Solo Bus A (FOH Engineer Headphones), Solo Bus B (Stage Monitor Engineer Headphones), and Main L/R (Audience PA System / Emergency Rehearsal Announcement).
     - Bank toggles for Aux 1-16, Aux 17-32, Aux 33-48, Aux 49-64, Aux 65-80, and Aux 81-96, plus 1-touch `ALL AUX` and `ALL MATRIX` broadcast buttons.
   - **Dedicated Voice Preamp & Processing Chain:**
     - Preamp Trim Gain ($-20\text{ dB}$ to $+40\text{ dB}$) with lock-free real-time VU peak metering.
     - Swept 1st-order Butterworth High-Pass Filter ($40\text{ Hz}$ to $400\text{ Hz}$) for boomy proximity effect and handling noise reduction.
     - Fast Voice Noise Gate / Downward Expander ($-60\text{ dB}$ to $-20\text{ dB}$ threshold) to keep open-mic booth ambient chatter out of stage monitors.
     - Ear-Protection Soft-Knee Peak Limiter clamped strictly at $-1.0\text{ dBFS}$ to safeguard stage artists' hearing against vocal transients or shouted cues.
   - **Intelligent Music Auto-Ducker:**
     - Continuous music attenuation ($-40\text{ dB}$ to $-6\text{ dB}$) with $5\text{ ms}$ attack and $200\text{ ms}$ release smoothing, preventing speech masking while eliminating pumping between sentences.
   - **Flexible Switch & Footswitch Logic:**
     - `AutoLatchMomentary` mode: Taps under $300\text{ ms}$ toggle latching on/off; holding $\ge 300\text{ ms}$ functions as momentary push-to-talk (deactivates immediately upon release).
     - `LatchOnly` and `MomentaryOnly` hardware footswitch emulation modes.

5. **Monitor / Control Room / Talkback Signal Paths:**
   - Two independent monitor paths select Main L/R, AUX 1–96, Matrix 1–24, direct 2TR inputs 1–2, loaded Playback, or the software OSC generator. Each path has its own level, dim, mono, mute, and lock-free peak readout; Headphone/Listen Bus B can optionally follow Control Room A.
   - Independent four-band parametric EQs process the monitor signals only. Frequency, gain, Q, and enable state are applied by the audio callback and persisted with the session; they do not modify channel EQ.
   - Talkback uses a selectable active device input, the existing HPF/gate/limiter and latch/momentary engine, and explicit routing to AUX, Matrix, Solo A/B, Control Room, or Main L/R. No internal microphone or second talkback input is implemented. Control Room-only assignment is covered by a signal-flow test and does not inject into Main.
   - PFL and AFL remain channel-selectable solo taps: PFL takes the pre-fader channel signal and AFL takes the post-fader/pan signal. Monitor source selection yields to an active solo on its corresponding solo bus. Solo does not gate Main by default; the explicit In-Place Solo switch enables Main solo gating while honoring channel Solo Safe, and Solo Clear resets channel solo/key-listen selections.
   - Assigned Stage Box **Local I/O output ports 1–16** are now delivered to active callback outputs; patching Monitor Left/Right there provides a real Control Room/Listen Bus device path without changing Main unless that physical output is explicitly reassigned. Remote AES50/Dante network ports are still modeled only; no network hardware transport is implemented.
   - **Limits:** The engine exposes two stereo monitor paths, not four independently routed headphone outputs. The 2TR source is direct callback inputs 1–2 (not a dedicated USB return); FX Returns and external OSC inputs are not selectable monitor sources. PFL/AFL trims, separate Speaker A/B calibration, automatic measurement, monitor delay, configurable monitor matrix, and physical-output availability validation are unavailable. The active device must expose the chosen local output channel; Stage Box output assignment is explicit.

2. **OSC (Open Sound Control) & Wireless iPad Remote Protocol Server (MIDAS Chapter 27):**
   - **High-Speed UDP Network Daemon:**
     - Real-time `juce::DatagramSocket` server operating on port 53000 (configurable) for low-latency tablet/laptop wireless mixing.
     - Compatible out-of-the-box with standard tablet software: **Mixing Station**, **TouchOSC**, **Lemur**, and **QLab**.
   - **Standards-Compliant OSC 1.0 Binary Codec (`OscCodec`):**
     - Full 4-byte padding boundary alignment and big-endian network byte order conversion for floats, 32-bit integers, and null-terminated strings.
   - **Bi-Directional Command Support Across All 144 Channels:**
     - `/ch/{id}/fader` or `/ch/{id}/mix/fader <float 0..1>`: Channel fader control.
     - `/ch/{id}/pan` or `/ch/{id}/mix/pan <float -1..1>`: Channel pan control.
     - `/ch/{id}/mute` or `/ch/{id}/mix/mute <int 0|1>`: Channel mute control.
     - `/ch/{id}/solo` or `/ch/{id}/mix/solo <int 0|1>`: Channel solo control.
     - `/ch/{id}/spatial/xyz <float x> <float y> <float z>`: 3D spatial object positioning.
     - `/ch/{id}/name <string>`: Channel strip label synchronization.
     - `/main/fader <float 0..1>`: Master bus fader.
     - `/talkback/on <int 0|1>`: Remote talkback switch control.
     - `/xinfo` / `/ping`: Automated handshake replying with console model, engine version, logical channel count, and sample rate.
   - **GUI Monitor Panel & Activity Log:**
     - Status LED indicator (Online/Offline), live client IP count, packet counters (RX/TX), one-click test broadcast ping, and scrolling live activity log.

3. **Surround Panning & Spatial Monitoring (MIDAS Chapter 30):**
   - Per-channel panning supports Stereo 2.0 (the normal Main L/R path), LCR 3.0, Quad 4.0, 5.1, 7.1, and 7.1.4. The speaker stem order is L, R, C, LFE, Lss, Rss, Lsr, Rsr, TFL, TFR, TRL, TRR. Panner gains are calculated by `SurroundPanner`; speaker stems contribute to the Main stereo fold-down and can be patched through Local I/O outputs 1–16 to active device output channels.
   - The Surround panel follows the selected channel across the mixer, shows mapped Stage Box speaker sources versus active device output count, and provides a drag panner plus X/Y/Z, divergence, center, LFE send, and distance controls. Distance applies smoothed inverse-distance attenuation relative to 1 m, clamped to avoid gain boost.
   - Optional LFE low-pass is a sample-rate-aware second-order Butterworth filter with selectable 40, 60, 80, 100, or 120 Hz cutoff. This filters the explicit LFE send only; it is not bass management or redirection from full-range speakers.
   - Stereo fold-down sums center at -3 dB, side/rear surrounds at -3 dB, and height speakers at -6 dB; the LFE channel is omitted. Binaural headphone monitoring is available through the existing spatializer.
   - **Limits:** 9.1.6, Auro 3D 9.1, and custom layouts are unavailable. Only the 16 Local I/O output patches are delivered to the audio device callback; AES50/Dante network ports are modeled but lack hardware transport. Physical multichannel operation requires a device with enough active outputs and explicit output assignments. There is no independent surround bus master, speaker delay alignment, per-speaker mute/solo, configurable downmix matrix, or low-frequency redirection.

4. **Software Stage Box Patch Matrix (MIDAS Chapters 27 & 31 design reference):**
   - The patch model represents 152 input and 152 output ports across Local I/O, AES50-labelled ports, and Dante-labelled ports. These are software routing entries only: there is no AES50/Dante transport or hardware clock synchronization. Displayed clock-lock and jitter values are simulated defaults, not measurements of a device or network.

2. **Frequency-Conscious Key Sidechain & External Key Routing (MIDAS Chapter 17):**
   - **Dedicated Detector Key Filter:** Direct Form II Transposed biquad filter with sample-rate awareness providing continuous frequency ($20\text{ Hz}$ to $20\text{ kHz}$) and Q ($0.1$ to $10.0$) controls across selectable filter topologies (`Bypass`, `HPF`, `LPF`, `Bandpass`, `Notch`).
   - **External Key Sidechain Routing:** Any channel can be selected as the key detector source for any other channel across all 144 channels via lock-free real-time pre-pass tapping.
   - **Key Listen Functionality:** One-touch toggle button to audition the filtered sidechain detector audio directly through the Solo / Monitor bus (Solo A or Solo B).
   - **Channel Dynamics Presets:** `KICK SUB (60Hz BP)`, `SNARE SNAP (2.5kHz BP)`, `BASS DUCK (EXT CH 1)`.

3. **4-Band Dynamic Parametric EQ Engine (MIDAS Chapter 16):**
   - High-precision Orfanidis / Regalia-Mitra digital filter topology with continuous per-sample dynamic gain modulation.
   - Independent controls per band: Frequency, Static Gain, Q, Dynamic Enable, Mode (`Above Threshold` / `Below Threshold`), Threshold, Range, Ratio, Knee, Attack, and Release.
   - Presets for Vocal De-Esser (6–8 kHz), Bass Resonance Tamer (120 Hz), and Detail Lift.

4. **Dan Dugan Speech Automixer Engine (MIDAS Chapter 28):**
   - Industry-standard Gain-Sharing Automixer for up to 32 speech channels across 3 independent Groups (Group 1, 2, 3).
   - Normalizes total system acoustic gain so that $\sum g_k \le 1.0$ ($0\text{ dB}$ acoustic gain).

5. **Virtual FX Rack Processing Engine (Chapter 15):**
   - 24-slot Graviton virtual FX rack with TC Electronic VSS4 Reverb, Stereo Tape & Digital Delay, and Studio Pitch Shifter / ADT Doubler.

6. **Channel AI & Automatic Acoustic Profiling (Chapter 35):**
   - Real-time 4-second acoustic fingerprint analyzer with zero-crossing rate, crest factor, and instrument classification.

7. **Manchino Multi-Edit Matrix & Show Spreadsheet Import/Export (Chapters 24 & 33):**
   - Global 144-channel visual matrix grid for simultaneous multi-channel batch parameter editing and RFC-4180 CSV import/export.

---

## Core System Architecture Overview (MIDAS HD96-24 Specification Reference)
- **144 logical channels** with software fader banking in 8/16/24-channel pages; this does not imply motorized fader hardware.
- **96 AUX sends and 24 Matrix buses**, plus the stereo Main L/R bus. Main Center is not modeled as an independent bus.
- **Dual Solo System (Solo A & Solo B):** Independent listening buses for FOH (In-Ear Monitors / Headphones) and Monitor engineers (Stage Wedges / Control Room).
- **Control Room / Headphone Monitor Section:** Resizable console-style Monitor/Talkback surface with quick source selection, two source-selectable stereo monitor paths, independent level, Dim, Mono, Mute, four-band parametric EQ, session recall, real L/R peak meters, and latch/momentary talkback. Physical output requires an explicit Local I/O Stage Box patch. Four independent headphone feeds, speaker A/B switching, monitor delay, and automated calibration are not available.
- **Variable Phase Filter Engine:** Continuous $0^\circ$ to $180^\circ$ and $0^\circ$ to $90^\circ$ all-pass acoustic phase alignment with LOW (100 Hz) and HIGH (1.5 kHz) optimizations.
- **Klark Teknik DN370 31-Band Graphic EQ:** 31 ISO 1/3-octave peaking filters with Proportional-Q and Constant-Q modes, dual surgical notch filters, and bypass.
- **Interactive 8-Band Parametric EQ:** Real-time composite response curve with eight draggable frequency/gain nodes and Q adjustment through the Q slider or mouse wheel. The channel EQ and Dynamic EQ visualizer stay synchronized with their parameter sliders.
- **Scene Crossfade Automation:** Ramped parameter glides (10 ms to 10 seconds) preventing pops/clicks during live show scene recalls.
- **Touring Channel Processing:** $180^\circ$ Polarity Invert ($\varnothing$), Precision Input Delay (0–500 ms), Pre/Post-EQ Dynamics Topology, Swept Butterworth HPF (20–2000 Hz) & LPF (1–20 kHz) with 12/24 dB/oct slopes.
- **Dynamic Control:** 4-Mode Compressor (Corrective, Adaptive, Creative, Vintage), Gate/Ducker with frequency-conscious sidechain filter & external routing, and 24 VCA / 24 POPulation groups with VCA Spill.
- **Mute Safe & Solo Safe Isolation:** Immunity from Mute Groups, VCA Mutes, and Scene Recalls.
- **Real-time Engine:** the mixer uses its existing JUCE audio callback and bounded/atomic routing snapshots where implemented. Real-time guarantees apply to specific documented paths; this README does not claim that every engine operation is lock-free or allocation-free.

---

## Build on Windows
Requirements: Visual Studio 2022 / Build Tools with Desktop development with C++, CMake 3.22+, Git.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target NOVA_HD
```

JUCE is fetched automatically by CMake from the official repository at tag `8.0.10`.

---

## Verification Matrix (software tests, not hardware certification)
The PASS labels below describe software test coverage from the last recorded test run. They do not certify external hardware interoperability. CTest has not been rerun after the latest UI/documentation changes; rerun it before using this table as a release sign-off.
| Verification Suite | Target / Test Name | Result | Details |
| :--- | :--- | :---: | :--- |
| **NOVA HD Tests** | `NOVA_HD_Tests.exe` | **PASS (last recorded run)** | Historical run passed; assertion count is omitted because it has not been reverified against the current source. |
| **Custom Fader Layout** | `EngineTests.cpp` | **PASS** | Ordered channel mapping, empty/invalid/duplicate entries, and 8/16/24 paging math; GUI interaction and session round-trip are not automated |
| **Manchino AUX Batch Edit** | Suite 86 | **PASS** | Selection mapping covers AUX 1 and AUX 96; invalid AUX selection IDs rejected |
| **Scene Safes & Fader Crossfade** | `EngineTests.cpp` | **PASS** | Cue safe groups preserve protected parameters; scene faders interpolate and complete over the configured duration |
| **Graviton DSP** | `GravitonDSP_Tests` | **PASS** | Biquad filters, PEQ, Butterworth HPF/LPF, All-Pass |
| **Graviton Stress** | `GravitonStress_Tests` | **PASS** | Extreme load scaling across 144 channels |
| **Aux Snapshot** | `GravitonAuxSnapshot_Tests` | **PASS** | Lock-free real-time AUX routing snapshot queue |
| **Safe AUX Bus Graph** | Phase 65 Runtime Integration | **PASS** | SafeBusGraphActivation plan publish/activate/execute wired into MixerEngine AUX callback |
| **Final Master Bus Chain** | Phase 66 Final Chain | **PASS** | True-Peak Limiter, Master Parametric EQ, TPD Dither, final GR metering integrated into master output stage |
| **Offline Render & Export** | Phase 67 Offline Render | **PASS** | MixerEngine renderOfflineBlocks/exportOfflineToWav wired via deterministic test seam to JUCE WAV writer |
| **Matrix Routing** | `GravitonMatrix_Tests` | **PASS** | Lock-free real-time Matrix crosspoint routing |
| **Virtual FX Rack** | Suite 84 | **PASS** | VSS4 Reverb, Tape Delay, Pitch Shifter, Insert/Aux |
| **Channel AI** | Suite 85 | **PASS** | Acoustic features, classification, adaptive trim/EQ/dyn |
| **Manchino Matrix** | Suite 86 | **PASS** | 144-Ch batch edit, RFC-4180 CSV export/import |
| **Dan Dugan Automixer**| Suite 87 | **PASS** | Gain-sharing, 3 Groups, Weight offset, Music Ducker |
| **Dynamic EQ Engine** | Suite 88 | **PASS** | Above/Below modes, De-Esser, Bass Tamer, Knee |
| **Key Sidechain Engine**| Suite 89 | **PASS** | HPF/LPF/BP/Notch filter, external key ducking, key listen |
| **Stage Box Patch Matrix**| Suite 90 | **PASS** | Software port-patching/model tests only; no AES50/Dante transport or hardware clock synchronization |
| **Surround & 3D Spatial** | Suite 91 | **PASS** | 5.1 / 7.1 / 7.1.4, LCR divergence, binaural preview |
| **Talkback Intercom Matrix**| Suite 92 + monitor tests | **PASS** | Auto-latch logic, selectable input, HPF, Gate, Limiter, destination routing and Control Room signal path |
| **OSC Remote Daemon** | Suite 93 | **PASS** | OSC 1.0 binary codec, 144-Ch wireless mixing dispatcher |
| **Matrix Plus Alignment**| Suite 94 | **PASS** | 500 ms sub-mm delay, acoustic environmental calculator (temp/RH), crossover all-pass phase alignment |
| **TC DVR2 Reverb** | Suite 95 | **PASS** | TC Electronic System 6000 DVR2 8-line FDN non-linear reverb, Hadamard matrix, soft-saturation |
| **Dual-FFT RTA Analyzer**| Suite 96 | **PASS** | Analyzer FFT/transfer/coherence calculations tested; application feeds post-master Main L/R, not independent source/microphone taps |
| **DAW 64×64 Streamer**| Suite 97 | **PASS** | In-process software ring-buffer/virtual-soundcheck model; does not test external ASIO, USB Audio Class, or DAW hardware interoperability |
| **TC Electronic SCF** | Suite 98 | **PASS** | TC SCF 3-mode (Chorus, PitchMod, Flanger) golden-ratio multi-voice delay, FX Rack Slot 14 |
| **Color & Instrument Tags**| Suite 99 | **PASS** | 16-color MIDAS concert palette, 16 instrument tags (GEN..PERC), scribble strip popup picker |
| **Virtual Soundcheck UI** | Suite 100 | **PASS** | 1-click global VSC input substitution, 64x64 multi-track monitoring, RTA spectral pipeline |
| **POPulation Groups Spill**| Suite 101 | **PASS** | 24 POPulation groups, 1-click POP spill isolating member channels onto active fader bank, color sync |
| **TC NonLin2 Reverb** | Suite 102 | **PASS** | TC Electronic System 6000 NonLin2 non-linear envelope, 80s gated snare & reverse reverb crescendo |
| **Cue List & MSC Show** | Suite 103 | **PASS** | Cue GO/PREV recalls mapped scene slots with per-cue safes; channel/Main/VCA/AUX/Matrix fader crossfade; MIDI Show Control (MSC); playlist reorder, edits, validation, and `.novashow` archive round-trip |
| **Dynamic EQ Visualizer**| Suite 104 | **PASS** | Real-time dynamic composite curve and 100-sample gain-reduction history; draggable frequency/gain/Q handles, synchronized parameter sliders, and graph-mapping tests |
| **ASIO-named FX Bridge**| Suite 105 | **PASS** | Software ring-buffer stub tests only; not an ASIO driver or USB Audio Class implementation |
| **Offline Multi-Track Bounce** | Suites 106–107 | **PASS** | Single-pass Master L/R + AUX 1–4 / MATRIX 1–2 stem capture, faster-than-realtime render, stereo Master WAV + mono stems, 16/24-bit, 48k/96k/source rate, unique file naming |
| **Offline Rate & Format** | Suites 108–109 | **PASS** | Lagrange-interpolated sample-rate conversion (48k → 96k), FLAC export, non-destructive output verification |
| **Offline Cancel & Patch** | Suites 110–112 | **PASS** | Cancellation deletes every partial file, `PATCH TRACKS 1:1` applies/restores operator patch, invalid jobs rejected without writing |
| **Sub-Harmonic Synth** | Suite 113 | **PASS** | Virtual FX Slot 19 sub-bass generator, zero-crossing fundamental tracker, phase-continuous octave divider (20–160 Hz) |
| **Klark Teknik DN360 GEQ** | Suite 114 | **PASS** | Virtual FX Slot 20 classic inductor graphic EQ, 30 ISO 1/3-octave bands (25 Hz–20 kHz), +/-15 dB range, flat transparency |
| **Precision De-Esser** | Suite 115 | **PASS** | Virtual FX Slot 21 split-band & broadband dynamic sibilance control (2–12 kHz), stereo-linked sidechain detector |
| **FX Rack Dispatch (19–21)**| Suite 116 | **PASS** | FX Rack 24-slot dispatch, channel insert injection, sample-rate propagation, bit-exact bypass verification |
| **MIDI Learn & Hardware Mapper** | Suite 117 | **PASS** | Incoming CC learn/mappings and optional Mackie Control output: 8 visible motor faders (14-bit pitch wheel), Mute/Solo LED feedback, Sends-on-Fader levels; tested protocol encoding |
| **Spectrogram & 3D Waterfall** | Suite 118 | **PASS** | MIDAS Chapter 27 standalone analyzer, Smaart-style 16k FFT, waterfall cascading frames, A/C/Z-weighting, CSV export |
| **Show Archive (.novashow)** | Suite 119 | **PASS** | MIDAS Chapter 30 full show bundle, 100 scenes, patch matrix, FX states, metadata, fast JSON serialize/deserialize |
| **Convolution Reverb** | Suite 120 | **PASS** | Virtual FX Slot 22 overlap-add IR reverb, 8 synthetic presets (Hall, Plate, Room, Spring, etc.), custom WAV IR load |
| **Hybrid Algorithmic Reverb** | Suite 121 | **PASS** | Virtual FX Slot 23 8-comb/4-allpass FDN late tail + early reflections, shimmer (+1 oct), freeze hold, ducking |
| **FX Rack 24-Slot Full Dispatch** | Suite 122 | **PASS** | Full 24-slot Virtual FX Rack complete: Slots 22 (Convolution) & 23 (Hybrid) active, channel/master inserts |
| **Channel View Integration** | Channel View selection/state test | **PASS** | 144-channel selection, trim/delay/fader UI writes, stereo-linked parameter propagation, and channel state/session metadata round-trip |
| **CTest Overall** | `ctest -C Release` | **PASS (last recorded run)** | 5/5 test suites passed in the last recorded run; rerun against the current build before treating this as a release certification |

---

## License
Proprietary — NOVA Audio Systems. Built with JUCE 8. All rights reserved.
