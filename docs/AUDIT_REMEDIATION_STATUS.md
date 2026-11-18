# Audit remediation status

Branch: `audit/full-review-fixes`

This branch contains the first safe remediation batch from the repository audit.
It is not a declaration that every audit finding is resolved.

## Changes applied

- Reject OSC messages containing NaN or infinity before they reach mixer controls.
- Protect channel-name reads and writes with a JUCE critical section and route GUI/OSC access through snapshot accessors.
- Harden OSC integer decoding by assembling unsigned network-order bits before copying into the signed value.
- Reject unsupported OSC argument tags whose payload length is unknown, rather than continuing with a desynchronized parser offset.
- Validate OSC string padding offsets before reading subsequent fields.
- Synchronize MIDI Learn state and group access; return a copy from `getGroup()` rather than exposing a reference into a concurrently-mutated vector.
- Validate mapping-group indices and clamp/repair loaded active-group state.
- Make `NOVA_MAX_DELAY_MS` a validated CMake cache string and use it for the delay-line capacity (the engine's documented 96 kHz design point).
- Remove the developer-specific Windows path from startup logging; logs now go to the user's application-data directory.
- Add an initial Windows configure/build/CTest workflow.

## Remaining work before this audit can be marked complete

1. **Convolution reverb parameter/filter race** — requires an audio-thread-owned parameter publication and filter-coefficient update design. A mutex must not be added to `processStereo()`; the current implementation is not changed in this batch because a superficial lock would create real-time audio dropouts.
2. **Spectrogram real-time behavior** — confirm whether any production callback invokes `pushSamples()`. If it does, replace its mutex/vector/deque work with preallocated buffers and a bounded producer/consumer handoff. Its current integration status must be verified before changing ownership semantics.
3. **OSC network trust boundary** — add an explicit configurable source allowlist or authenticated control session, plus rate limiting. Firewall reachability is deployment-specific and cannot be inferred from source code.
4. **Delay capacity above 96 kHz** — current compile-time capacity follows `NOVA_MAX_DELAY_MS * 96`, matching the project's documented 96 kHz design point. If higher device sample rates are supported, allocate the delay lines from the actual device sample rate in `audioDeviceAboutToStart()` and replace the fixed modulo bounds.
5. **Fresh validation** — the GitHub connector does not provide a local compiler/runtime here. The new CI workflow must run on GitHub; inspect its build and CTest results, including the known Dugan Automixer assertions, before merging.

## Validation status

- Source edits were applied to this branch.
- Build: not run in this environment.
- CTest: not run in this environment.
- CI: workflow added; no run result is available yet.
- `main` has not been modified.
