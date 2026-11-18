# Audit remediation status

Branch: `audit/full-review-fixes`

This branch contains the remediation batch for the five requested high-priority areas. It is not merged to `main`.

## Applied changes

### Convolution reverb
- Removed audio-thread reads of the mutable control-thread `params_` object.
- Added a versioned snapshot made from lock-free 32-bit atomic words. The audio thread retries only twice and retains its last coherent snapshot rather than waiting.
- Moved tone-filter coefficient changes into the audio processing thread; the GUI/control thread no longer mutates Biquad coefficients.
- Changed parameter getters to return synchronized value snapshots and routed GUI writes through `setParams()`.
- Made the sample rate and IR-loaded flag atomic.
- Added a concurrent parameter-update/audio-processing regression test.

### Spectrogram
- Removed mutex acquisition from `pushSamples()` and `pushReferenceSamples()`.
- Removed reference-buffer resizing from the sample push path.
- Replaced per-FFT `FFTFrame` allocation and deque growth with a preallocated 512-slot history ring.
- Added slot ownership states so the audio producer never blocks behind a UI reader; a frame is skipped if its slot is busy.
- Published current spectrum arrays through a non-blocking snapshot state; UI getters return copies.
- Made sample rate, weighting, peak-hold controls, freeze and summary statistics atomic.
- FFT-size/window changes and reset must be performed while the producer is stopped; the code documents this requirement.

### OSC security
- UDP OSC is default-deny for non-loopback senders.
- Added explicit trusted sender IP allowlisting and a corresponding comma-separated field in the OSC Server panel.
- Added a global 1,000-packet/second ceiling applied after allowlist validation.
- This is IP allowlisting, not cryptographic authentication. Use a trusted, segmented LAN and firewall rules; do not expose the UDP port to the public internet.

### Delay capacity
- Delay buffers are now sized in `audioDeviceAboutToStart()` from the actual device sample rate and configured `NOVA_MAX_DELAY_MS`.
- Channel, monitor and master delay modulo/clamp calculations use the runtime capacity.
- Added regression assertions for 96, 192 and 384 kHz capacities.

## Validation

- Regression tests added for concurrent convolution parameter publication, spectrogram snapshots/history, delay sizing, and OSC allowlist configuration.
- GitHub Actions Windows CI workflow is configured to run CMake configure, build and CTest on branch pushes and pull requests.
- Build/CTest status must be taken from the latest workflow run linked below; do not treat queued or in-progress runs as passing.
- The Spectrogram API now returns copies for current spectra and waterfall history. This avoids exposing concurrently-mutated internal storage.

## Operational note

The OSC panel's trusted sender field defaults to loopback. Enter each controller's exact source IP, comma-separated, before starting the server. An empty list denies all senders.
