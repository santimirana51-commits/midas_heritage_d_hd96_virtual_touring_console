# OPENLIVE_REFACTORED

**OpenLive HD Digital Mixing System** adalah workstation pencampuran audio digital modular berperforma tinggi yang dibangun di atas framework **JUCE 8** dan standar **C++20**. Proyek ini merupakan hasil refaktorisasi terstruktur dari sistem NOVA_HD.

---

## 📁 Struktur Direktori

```
OPENLIVE_REFACTORED/
├── AGENTS.md                  # Dokumentasi arsitektur dan pedoman kontributor / AI agent
├── CMakeLists.txt             # Skrip build CMake modern
├── README.md                  # Dokumentasi umum proyek
├── docs/                      # Dokumentasi teknis, audit modul, dan catatan rilis
├── Resources/                 # Aset grafis, tangkapan layar validasi, manual hardware
├── Source/
│   ├── Application/           # Entry point aplikasi dan JUCE application lifecycle
│   ├── Audio/                 # Driver audio I/O (WASAPI/ASIO), OSC server, offline bounce
│   ├── Mixer/                 # Engine pencampuran, channel state, automixer, monitor
│   ├── DSP/                   # Algoritma pemrosesan sinyal (EQ, Dynamics, Analisis, AI)
│   ├── Effects/               # Rak efek virtual (Reverbs, Delays, Modulation, Multi-band)
│   ├── Routing/               # Matriks routing patching, bus scalable graph, surround pan
│   ├── GUI/                   # Antarmuka fader console, channel strip, dialog panel
│   ├── MIDI/                  # Binding MIDI kontroler dan MIDI learn
│   ├── Recording/             # Perekaman multi-track WAV berkinerja tinggi
│   └── Utilities/             # Otomasi, cue list, show archive, protokol parsing
├── Tests/                     # Suite pengujian unit dan benchmark algoritma
└── scripts/                   # Skrip otomatisasi build dan pengujian
```

---

## 🛠️ Persyaratan Sistem & Dependensi

- **Sistem Operasi**: Windows 10/11 x64 (atau macOS / Linux dengan toolchain JUCE)
- **Kompiler**: Visual Studio 2022 (MSVC 19.41+) dengan dukungan C++20
- **CMake**: Versi 3.22 atau lebih baru
- **JUCE Framework**: Versi 8.0.10 (diunduh secara otomatis via `FetchContent`)

---

## 🚀 Panduan Build

### Menggunakan Command Line (CMake)

1. Buka PowerShell atau Command Prompt pada root folder `OPENLIVE_REFACTORED`.
2. Generate project build:
   ```powershell
   cmake -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
   ```
3. Lakukan kompilasi:
   ```powershell
   cmake --build build --config Release --parallel
   ```

### Menggunakan Skrip Cepat
- Jalankan `scripts\build.bat` untuk melakukan kompilasi otomatis.
- Jalankan `scripts\run_tests.bat` untuk mengeksekusi rangkaian unit test.

---

## 🧪 Pengujian Unit & Verifikasi

Suite pengujian mencakup:
- **OpenLive_Tests**: Pengujian logika mixer engine, routing bus, channel state, dan integrasi callback audio.
- **GravitonDSP_Tests**: Verifikasi numerik algoritma filter, kompresor, gate, dan automixer.
- **GravitonStress_Tests**: Pengujian batas beban skenario routing bus kompleks.
- **GravitonAuxSnapshot_Tests**: Pengujian queue thread-safe routing AUX non-blocking.
- **GravitonMatrix_Tests**: Pengujian queue matriks non-blocking.
- **GravitonBenchmark**: Profiling performa algoritma DSP inti.

---

## 📱 Wireless Android Remote Control
Aplikasi remote kontrol tablet & HP Android mandiri untuk mengendalikan mixer ini secara nirkabel melalui protokol OSC UDP (Port default `53000`):
👉 **[Repositori Android Remote: santimirana51-commits/heritage_d_hd96_remote](https://github.com/santimirana51-commits/heritage_d_hd96_remote)** (Tersedia file APK siap pakai).


