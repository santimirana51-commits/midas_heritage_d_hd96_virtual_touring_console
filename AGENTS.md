# OPENLIVE_REFACTORED — Petunjuk & Konvensi untuk AI Agents & Developers

Proyek ini adalah refaktorisasi modular dari arsitektur mixing digital **NOVA_HD** menjadi sistem **OPENLIVE_REFACTORED**.

---

## 1. Struktur Modul & Tanggung Jawab

| Modul | Path | Tanggung Jawab Utama |
|---|---|---|
| **Application** | `Source/Application/` | Entrypoint aplikasi (`Main.cpp`), manajemen window JUCE (`Application.h`). |
| **Audio** | `Source/Audio/` | ASIO Bridge, ASIO Streamer, Offline Bounce, OSC Server, Health Monitoring, Test Harness. |
| **Mixer** | `Source/Mixer/` | State channel (`ChannelState.h`), audio engine core (`MixerEngine.h`), Master bus, Dugan Automixer, Monitor & Talkback. |
| **DSP** | `Source/DSP/` | Peaking EQ, Dynamic EQ, 31-Band GEQ, DN360, Channel AI, Dynamics (Compressor/Gate), RTA, Spectrogram, Signal Generator. |
| **Effects** | `Source/Effects/` | Virtual FX Rack (`GravitonEffectsRack`), Plate, Hybrid, Convolution, DVR2 Reverb, NonLin2, SCF Chorus, SubHarmonic, Master X. |
| **Routing** | `Source/Routing/` | Patch Matrix, Matrix Plus, AUX Routing Snapshots, Scalable Bus Graph, Surround Panning, Output Router. |
| **GUI** | `Source/GUI/` | Look and Feel kustom, Channel Strip, Panel parameter (EQ, Dyn, FX, AI, dll), Main Component, Surface Model. |
| **MIDI** | `Source/MIDI/` | MIDI Controller mapping callback (`MixerMidiCallback.h`), MIDI Learn engine (`GravitonMidiLearn.h`). |
| **Recording** | `Source/Recording/` | Multi-channel WAV background recorder non-blocking FIFO (`RecordingWorker.h`). |
| **Utilities** | `Source/Utilities/` | Cue List, Automation engine, Show Archive serializer, Manchino protocol, Bus persistence. |

---

## 2. Aturan Rekayasa & Konvensi Kode

1. **DSP & Audio Thread Safety**:
   - Dilarang keras melakukan alokasi memori dinamis (`new`, `malloc`, resize vector), locking mutex berdurasi panjang, atau I/O disk di dalam callback audio real-time.
   - Gunakan `std::atomic` untuk transfer nilai float/int/bool antar GUI/control thread dan audio callback thread.
   - Algoritma DSP harus tetap sanitasi NaN/Inf menggunakan fungsi `clampFinite()` atau `sanitize()`.

2. **Inklusi Header**:
   - Selalu sertakan `#pragma once` pada setiap header.
   - Gunakan path inklusi modular, contoh: `#include "DSP/GravitonDSP.h"`, `#include "Mixer/MixerEngine.h"`.
   - Hindari include sirkular; gunakan forward declaration bila hanya membutuhkan pointer/referensi.

3. **Integritas Fitur Asli**:
   - Jangan membuat fungsi atau class placeholder menggantikan algoritma produksi.
   - Pertahankan seluruh parameter, channel mapping (144 logical channel, 96 AUX, 24 Matrix), serta preset audio asli.

4. **Workflow Pengujian**:
   - Setiap perubahan pada DSP atau routing wajib diverifikasi menggunakan unit tests di folder `Tests/`.

