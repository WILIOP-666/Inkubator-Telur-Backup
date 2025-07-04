# Inkubator Telur Mini Otomatis berbasis ESP32

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Sebuah proyek IoT untuk membangun prototipe inkubator telur ayam otomatis yang cerdas, stabil, dan dapat dipantau dari jarak jauh. Sistem ini menggunakan mikrokontroler ESP32 sebagai unit pemrosesan utama, menerapkan algoritma kontrol PID untuk menjaga suhu secara presisi, dan menyediakan antarmuka web modern untuk monitoring dan kontrol secara *real-time* melalui WebSocket.

![Visualisasi Proyek](topologi.png)

## Deskripsi Proyek

Proyek ini bertujuan untuk mengatasi tantangan utama dalam proses penetasan telur, yaitu menjaga stabilitas lingkungan mikro di dalam inkubator. Dengan otomatisasi, sistem ini meminimalkan kebutuhan intervensi manual dan mengurangi risiko kegagalan akibat fluktuasi suhu. Pengguna dapat dengan mudah memantau suhu, kelembapan, dan status perangkat, serta mengatur parameter penting seperti suhu target dan kecepatan kipas langsung dari browser web di perangkat apa pun (komputer, tablet, atau ponsel).

## Fitur Utama

-   🌡️ **Kontrol Suhu Presisi:** Menggunakan algoritma **PID (Proportional-Integral-Derivative)** untuk menjaga suhu tetap stabil pada titik yang diinginkan (setpoint) dengan overshoot dan fluktuasi minimal.
-   💨 **Kontrol Sirkulasi Udara:** Kecepatan kipas DC dapat diatur menggunakan sinyal **PWM (Pulse Width Modulation)** untuk memastikan distribusi panas yang merata.
-   🌐 **Monitoring & Kontrol Real-Time:** Antarmuka web yang responsif dan modern, terhubung melalui **WebSocket**, menampilkan data secara langsung tanpa perlu me-refresh halaman.
-   ⏰ **Sinkronisasi Waktu:** Dilengkapi dengan modul **RTC (Real-Time Clock) DS1302** untuk pencatatan waktu yang akurat, bahkan saat ESP32 mati.
-   📊 **Antarmuka Informatif:** Tampilan web yang bersih menggunakan Bootstrap 5, menampilkan data suhu, kelembapan, status pemanas, dan kontrol interaktif.
-   🔌 **Setup Sederhana:** Dirancang agar mudah dirakit dan dikonfigurasi, bahkan untuk pemula di bidang elektronika dan IoT.

## Komponen yang Dibutuhkan

| No. | Komponen                      | Jumlah | Keterangan                                        |
| :-- | :---------------------------- | :----: | :------------------------------------------------ |
| 1   | Wemos D1 R32 (ESP32)          |   1    | Pusat kendali utama proyek.                       |
| 2   | Sensor Suhu & Kelembapan DHT11|   1    | Membaca kondisi lingkungan di dalam inkubator.    |
| 3   | Modul Relay 2-Channel 5V      |   1    | Sebagai saklar elektronik untuk pemanas (lampu).  |
| 4   | Modul MOSFET IRF520           |   1    | Mengontrol kecepatan kipas DC 12V.                |
| 5   | Kipas DC 12V                  |   1    | Untuk sirkulasi udara di dalam inkubator.         |
| 6   | Lampu Pijar 5W & Fitting      |   1    | Sumber panas utama.                               |
| 7   | Modul RTC DS1302              |   1    | Menjaga data waktu tetap berjalan.                |
| 8   | Modul Step Up MT3608          |   1    | Menaikkan tegangan dari 5V ke 12V untuk kipas.    |
| 9   | Adapter DC 5V (min. 2A)       |   1    | Sumber daya utama untuk seluruh rangkaian.        |
| 10  | Kabel Jumper                  | Cukup  | Untuk menghubungkan semua komponen.               |
| 11  | Boks Styrofoam/Kayu           |   1    | Sebagai bodi/ruang inkubator.                     |

## Skema Rangkaian (Wiring)

Hubungkan semua komponen sesuai dengan skema `topologi.png` yang disertakan. Berikut adalah detail koneksi pin utama ke ESP32:

-   **Sensor DHT11:**
    -   `DATA` → `GPIO 23`
-   **Modul Relay (Pemanas):**
    -   `IN1` → `GPIO 18`
-   **Modul MOSFET IRF520 (Kipas):**
    -   `SIG` → `GPIO 19`
-   **Modul RTC DS1302:**
    -   `CLK` → `GPIO 5`
    -   `DAT` → `GPIO 4`
    -   `RST` → `GPIO 2`
-   **Sumber Daya:**
    -   Pastikan semua komponen yang membutuhkan `5V` dan `GND` terhubung ke jalur daya dari Adapter 5V.
    -   Output dari modul Step Up 12V dihubungkan ke input `VIN` dan `GND` pada modul IRF520.

## Instalasi & Konfigurasi

Ikuti langkah-langkah berikut untuk menjalankan proyek ini.

### 1. Persiapan Perangkat Lunak

1.  **Instal Arduino IDE:** Unduh dan instal versi terbaru dari [situs resmi Arduino](https://www.arduino.cc/en/software).
2.  **Instal Board ESP32:**
    -   Buka Arduino IDE, masuk ke `File > Preferences`.
    -   Tambahkan URL berikut ke "Additional Board Manager URLs":
        ```
        https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
        ```
    -   Buka `Tools > Board > Boards Manager...`, cari "esp32", dan instal paket dari Espressif Systems.
3.  **Pilih Board:**
    -   Pilih board yang sesuai di `Tools > Board > ESP32 Arduino > LOLIN D32` atau `ESP32 Dev Module`.
4.  **Instal Pustaka (Libraries):**
    -   Buka `Sketch > Include Library > Manage Libraries...`.
    -   Cari dan instal semua pustaka berikut:
        -   `DHT sensor library` oleh Adafruit
        -   `Adafruit Unified Sensor` (biasanya terinstal bersama DHT)
        -   `WebSockets` oleh Markus Sattler
        -   `ArduinoJson` oleh Benoit Blanchon
        -   `PID` oleh Brett Beauregard
        -   `Virtuabotix RTC`

### 2. Konfigurasi Firmware (Kode ESP32)

1.  Buka file `incubator_firmware.ino` di Arduino IDE.
2.  **Atur Kredensial WiFi:** Ubah nama dan password WiFi sesuai dengan jaringan Anda.
    ```cpp
    const char* ssid = "NAMA_WIFI_ANDA";
    const char* password = "PASSWORD_WIFI_ANDA";
    ```
3.  **(Opsional) Atur Waktu RTC:**
    -   Saat pertama kali mengunggah, hapus komentar pada baris di bawah ini dan isi dengan waktu saat ini.
    -   Format: `myRTC.setDS1302Time(detik, menit, jam, hari_minggu, tanggal, bulan, tahun);`
    -   Setelah selesai mengatur, beri komentar lagi pada baris tersebut dan unggah ulang kodenya.
    ```cpp
    // myRTC.setDS1302Time(00, 30, 18, 5, 7, 2024, 4);
    ```

### 3. Unggah Firmware

1.  Hubungkan ESP32 ke komputer Anda melalui USB.
2.  Pilih Port yang benar di `Tools > Port`.
3.  Klik tombol **Upload**.
4.  Setelah selesai, buka **Serial Monitor** (`Tools > Serial Monitor`) dengan baud rate `9600`. Tunggu hingga ESP32 terhubung ke WiFi dan menampilkan **alamat IP**. Catat alamat IP tersebut.

### 4. Konfigurasi Antarmuka Web

1.  Buka file `app.js` dengan teks editor.
2.  Cari baris berikut dan ganti alamat IP dengan yang Anda dapatkan dari Serial Monitor.
    ```javascript
    const ESP32_IP = "192.168.1.8"; // <-- GANTI DENGAN ALAMAT IP ESP32 ANDA
    ```
3.  Simpan file tersebut.

## Cara Penggunaan

1.  Pastikan seluruh rangkaian perangkat keras sudah menyala.
2.  Buka file `index.html` di browser web modern (Google Chrome, Firefox, dll.). Cukup klik dua kali pada file tersebut.
3.  Halaman web akan secara otomatis mencoba terhubung ke ESP32 Anda. Perhatikan indikator status koneksi di pojok kanan atas.
4.  Jika berhasil terhubung, semua data akan ditampilkan dan diperbarui secara otomatis.
5.  Anda kini dapat memantau kondisi inkubator dan mengatur suhu target serta kecepatan kipas melalui panel kontrol.

## Struktur Folder Proyek

```
/Inkubator-ESP32/
|
|-- incubator_firmware.ino      # Kode utama untuk ESP32
|-- index.html                  # File antarmuka web (struktur)
|-- style.css                   # File styling untuk web
|-- app.js                      # File JavaScript (logika web & WebSocket)
|-- topologi.png                # Gambar skema/diagram rangkaian
|-- README.md                   # File dokumentasi ini
```

## Pengembangan Lanjutan

Proyek ini memiliki banyak ruang untuk dikembangkan lebih lanjut, antara lain:
-   **Kontrol Kelembapan Aktif:** Menambahkan *humidifier* atau *nebulizer* untuk menjaga kelembapan secara otomatis.
-   **Pemutar Telur Otomatis:** Mengintegrasikan motor servo untuk memutar telur secara berkala.
-   **Pencatatan Data (Data Logging):** Menyimpan data suhu dan kelembapan ke kartu SD atau layanan cloud seperti Firebase/Thingspeak untuk analisis historis.
-   **Notifikasi:** Mengirim pemberitahuan ke ponsel (misalnya via Telegram atau IFTTT) jika terjadi kondisi abnormal (suhu terlalu tinggi/rendah).

## Lisensi

Proyek ini dilisensikan di bawah **Lisensi MIT**. Lihat file `LICENSE` untuk detail lebih lanjut. Anda bebas untuk memodifikasi, mendistribusikan, dan menggunakan kode ini untuk proyek pribadi maupun komersial.

---
Dibuat dengan semangat berbagi. Semoga bermanfaat
