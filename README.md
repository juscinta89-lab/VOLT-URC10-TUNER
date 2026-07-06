# URC10 Tuner by VOLT Robotic

App tuning untuk firmware `urc10_gagoh_tuner.ino`. Sambung phone/laptop ke board guna USB,
tune FWD/BWD trim, max speed, deadzone, turn gain, expo, dan kalibrasi RC (CH1/CH2) terus
dari browser — tak payah buka Arduino IDE setiap kali nak ubah setting.

## Struktur fail (jangan ubah nama/lokasi)

```
/
├── index.html
├── manifest.json
├── service-worker.js
└── icons/
    ├── icon-192.png
    ├── icon-512.png
    ├── icon-192-maskable.png
    └── icon-512-maskable.png
```

## Langkah 1 — Upload ke GitHub

1. Pergi ke [github.com/new](https://github.com/new), buat repo baru (contoh nama: `URC10-tuner`).
   Boleh public atau private — GitHub Pages jalan untuk kedua-dua (private perlukan akaun
   GitHub Pro/Team/Enterprise untuk Pages; kalau nak percuma, guna **public**).
2. Upload semua fail dalam folder ni (termasuk folder `icons/`) ke repo tu — boleh guna
   "Add file → Upload files" di web GitHub, drag semua sekali gus, then commit.

## Langkah 2 — Aktifkan GitHub Pages

1. Dalam repo tu, pergi **Settings → Pages**.
2. Bawah "Build and deployment", pilih **Source: Deploy from a branch**.
3. Branch pilih **main**, folder pilih **/ (root)**, tekan **Save**.
4. Tunggu 1-2 minit, GitHub bagi URL macam:
   `https://<username>.github.io/URC10-tuner/`

Fail **kena** guna HTTPS (bukan `file://`) sebab Web Serial API dan Service Worker cuma jalan
atas HTTPS (atau localhost) — GitHub Pages automatik bagi HTTPS, jadi tak payah setting apa-apa
tambahan.

## Langkah 3 — Install di phone Android

1. Buka URL GitHub Pages tu dalam **Google Chrome** (kena Chrome, bukan app lain).
2. Chrome akan tunjuk banner "Add to Home screen" / "Install app" — tekan install.
   Kalau banner tak muncul automatik, tekan menu titik-tiga (⋮) → **Add to Home screen**.
3. Lepas install, app akan ada icon sendiri kat home screen phone, buka macam app biasa
   (tiada address bar Chrome, full standalone).

## Langkah 4 — Sambung ke board

1. Pasang kabel **USB-OTG** (USB-C atau Micro-USB ikut phone) antara phone dan board URC10.
2. Buka app "URC10 Tuner" dari home screen.
3. Tekan **SAMBUNG USB**, phone akan minta pilih device serial — pilih board tu.
4. Slider terus live, tekan **SAVE KE EEPROM** bila dah puas hati dengan setting.

## Nota penting

- **Web Serial cuma jalan kat Chrome/Edge** (Android/desktop). Safari/iOS **tak sokong**
  Web Serial langsung — kalau anda guna iPhone, kaedah USB terus ni tak boleh pakai, kena
  guna cara lain (contoh Bluetooth, macam ROBOSOCCER BY VOLT punya app dulu).
- Kalau nanti nak update app (contoh tambah slider baru), edit `index.html`, commit &
  push semula ke GitHub — GitHub Pages auto re-deploy dalam masa singkat. App yang dah
  install kat phone akan auto-update sebab service worker guna strategi *network-first*.
- Kalau nak tukar icon/warna tema, edit `manifest.json` (`theme_color`, `background_color`)
  dan fail dalam `icons/`.
