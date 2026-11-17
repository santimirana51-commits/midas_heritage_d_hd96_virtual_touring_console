# Heritage Virtual Audio Driver (Basis Microsoft SYSVAD / MS-PL)

Direktori ini berisi definisi paket driver (`HeritageVirtualAudio.inf`) dan panduan integrasi **Fase 2** untuk menghubungkan endpoint audio virtual Windows (**4 pasang perangkat Stereo terpisah**: `Heritage Cable 1-2` s/d `7-8`) dengan transport Shared Memory di user-mode ([HeritageVirtualShmBridge.h](../../Source/Audio/HeritageVirtualShmBridge.h)).

> **Status Build pada Mesin Ini**: Windows Driver Kit (`km/portcls.h`) belum terpasang di mesin build saat ini, sehingga binary kernel `.sys` harus dikompilasi di lingkungan yang memiliki **Visual Studio + WDK 10/11**. Sisi user-mode Shared Memory IPC (`HeritageShmBlock`, `HeritageShmMapping`, dan `VirtualCableTransport`) sudah terkompilasi dan teruji via CTest.

---

## 1. Struktur Endpoint (4 Pasang Stereo Terpisah)
Untuk kompatibilitas maksimal dengan Chrome, Zoom, Discord, OBS, dan DAW:
- **Pair 0**: `Heritage Cable 1-2 (Play -> Mixer)` [Render] & `Heritage Cable 1-2 (Mixer -> Rec)` [Capture]
- **Pair 1**: `Heritage Cable 3-4 (Play -> Mixer)` [Render] & `Heritage Cable 3-4 (Mixer -> Rec)` [Capture]
- **Pair 2**: `Heritage Cable 5-6 (Play -> Mixer)` [Render] & `Heritage Cable 5-6 (Mixer -> Rec)` [Capture]
- **Pair 3**: `Heritage Cable 7-8 (Play -> Mixer)` [Render] & `Heritage Cable 7-8 (Mixer -> Rec)` [Capture]

---

## 2. Kontrak Shared Memory IPC (`HeritageShmBlock`)
Diambil langsung dari [HeritageVirtualShmBridge.h](../../Source/Audio/HeritageVirtualShmBridge.h):
- **Mapping Name**: `Local\HeritageVirtualAudioShmV1` (atau `Global\HeritageVirtualAudioShmV1` untuk akses lintas sesi/service dengan DACL eksplisit).
- **Magic & Version**: `0x48564143` (`"HVAC"`), `version = 1`.
- **Per Pair (`pairs[0..3]`)**:
  - `renderToMixer`: WaveRT Render DPC/Timer menyalin frame stereo dari cyclic buffer Windows Audio Engine ke `renderToMixer.push(...)`.
  - `mixerToCapture`: WaveRT Capture DPC/Timer menarik frame stereo dari `mixerToCapture.pop(...)` (otomatis mengeluarkan silence `0.0f` jika `streamActive == 0` atau mixer tidak berjalan).

---

## 3. Langkah Build & Uji dengan Microsoft SYSVAD
1. Clone atau siapkan [Microsoft Windows-driver-samples (`audio/sysvad`)](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad) (lisensi **MS-PL**).
2. Duplikasi sub-project `TabletAudioSample` / `EndpointsCommon` menjadi `HeritageVirtualAudio`, lalu daftarkan 4 pasang pin WaveRT Stereo sesuai `HeritageVirtualAudio.inf`.
3. Pada `CMiniportWaveRTStream::UpdatePosition` / timer DPC, hubungkan cyclic buffer Render & Capture ke `HeritageShmBlock::pairs[0..3]`.
4. **Pengujian Lokal (Test Signing)**:
   ```bat
   bcdedit /set testsigning on
   pnputil /add-driver HeritageVirtualAudio.inf /install
   ```
5. **Distribusi Publik (Produksi Windows 10/11)**:
   - Wajib menggunakan **EV Code Signing Certificate** dan **Microsoft Hardware Dev Center (Attestation / WHQL Signing)** agar driver dapat diinstal tanpa *Test Signing Mode* dan kompatibel dengan *Secure Boot*.
