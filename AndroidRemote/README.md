# Heritage HD96 Android Remote (OSC UDP)

Aplikasi remote kontrol Android untuk mengendalikan **OPENLIVE_REFACTORED** secara nirkabel melalui **OSC Server** (`Source/Audio/GravitonOscServer.h`, port default `53000`).

---

## Fitur yang Didukung
- **Koneksi & Ping (`/ping` -> `/info`)**: Memverifikasi koneksi ke PC mixer di jaringan Wi-Fi yang sama.
- **Kontrol 144 Channel (18 Bank × 8 Channel)**:
  - Fader Level (`/ch/{id}/mix/fader`, rentang `-60.0` dB s/d `+10.0` dB)
  - Mute (`/ch/{id}/mix/mute`)
  - Solo (`/ch/{id}/mix/solo`)
- **Master & Talkback**:
  - Main L/R Fader (`/main/fader`)
  - Talkback Toggle (`/talkback/on`)

---

## Cara Build & Install ke HP/Tablet Android
> **Catatan**: Mesin build saat ini hanya memiliki `adb.exe` dan `Android/Sdk` tanpa `java`/`gradle` di `PATH`, sehingga APK belum dikompilasi di mesin ini.

1. Buka folder `AndroidRemote/` di **Android Studio** (atau jalankan Gradle di PC dengan JDK 17 + Android SDK 34):
   ```bat
   cd AndroidRemote
   gradle assembleDebug
   ```
2. Install APK ke perangkat Android lewat ADB:
   ```bat
   adb install -r app/build/outputs/apk/debug/app-debug.apk
   ```
3. Di PC **OPENLIVE_REFACTORED**, buka **`SYSTEM` → `OSC SERVER`**, lalu nyalakan OSC Server pada port **`53000`** (pastikan firewall Windows mengizinkan UDP port `53000`).
4. Di aplikasi **Heritage HD96 Remote** di Android, masukkan **IP lokal PC**, port **`53000`**, lalu tekan **`PING`**.
