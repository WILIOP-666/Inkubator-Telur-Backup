/*
 ===================================================================================
 * NAMA PROYEK: INKUBATOR TELUR MINI BERBASIS ESP32 (VERSI PEMULA)
 * DESKRIPSI:   Firmware lengkap untuk mengontrol suhu inkubator menggunakan PID,
 *              mengatur kipas dengan PWM, dan memonitor data melalui WebSocket.
 *              Kode ini dirancang agar mudah dipahami oleh pemula dengan
 *              penjelasan di setiap bagiannya.
 * BOARD:       WEMOS/LOLIN D1 R32 (atau ESP32 Dev Module lainnya)
 * AUTHOR:      Gemini
 ===================================================================================
*/

// =================================================================================
// LANGKAH 1: MENYERTAKAN PUSTAKA (LIBRARIES)
// ---------------------------------------------------------------------------------
// Pustaka ini seperti "alat bantu" yang sudah dibuat orang lain agar kita tidak
// perlu membuat semuanya dari nol.
// =================================================================================

#include <WiFi.h>             // Untuk menghubungkan ESP32 ke jaringan WiFi.
#include <WebSocketsServer.h> // Untuk membuat server komunikasi real-time (WebSocket).
#include <DHT.h>              // Untuk membaca data dari sensor suhu dan kelembapan DHT.
#include <ArduinoJson.h>      // Untuk membuat dan membaca data dalam format JSON.
#include <PID_v1.h>           // Untuk algoritma kontrol PID yang presisi.
#include <virtuabotixRTC.h>   // Untuk membaca waktu dari modul jam (RTC DS1302).


// =================================================================================
// LANGKAH 2: KONFIGURASI UTAMA
// ---------------------------------------------------------------------------------
// Di bagian ini, kita mengatur semua variabel utama seperti nama WiFi, password,
// dan pin yang terhubung ke perangkat keras.
// =================================================================================

// --- Konfigurasi Jaringan WiFi ---
const char* ssid = "NYUDIS";      // <-- GANTI DENGAN NAMA WIFI ANDA
const char* password = "87654321C"; // <-- GANTI DENGAN PASSWORD WIFI ANDA

// --- Konfigurasi Pin Perangkat Keras ---
// Memberi nama pada nomor pin agar kode lebih mudah dibaca.
#define DHT_PIN     23 // Pin digital tempat sensor DHT11 terhubung.
#define DHT_TYPE    DHT11 // Tipe sensor yang digunakan (DHT11 atau DHT22).
#define RELAY_PIN_1 18 // Pin yang terhubung ke IN1 pada modul relay untuk pemanas.
#define FAN_PWM_PIN 19 // Pin yang terhubung ke sinyal (SIG) modul IRF520 untuk kipas.

// Pin untuk modul Real-Time Clock (RTC) DS1302
#define RTC_CLK_PIN 5  // Pin CLK pada RTC.
#define RTC_DAT_PIN 4  // Pin DAT (I/O) pada RTC.
#define RTC_RST_PIN 2  // Pin RST (Reset) pada RTC.

// --- Inisialisasi Objek dari Pustaka ---
// Membuat objek agar kita bisa menggunakan fungsi-fungsi dari pustaka yang ada.
WebSocketsServer webSocket = WebSocketsServer(81); // Membuat server WebSocket di port 81.
DHT dht(DHT_PIN, DHT_TYPE); // Membuat objek sensor DHT.
virtuabotixRTC myRTC(RTC_CLK_PIN, RTC_DAT_PIN, RTC_RST_PIN); // Membuat objek RTC.

// --- Konfigurasi Kontrol PID untuk Suhu ---
// PID adalah cara cerdas untuk menjaga suhu agar tetap stabil.
double Setpoint, Input, Output; // Variabel yang wajib ada untuk pustaka PID.
// Setpoint: Suhu target yang kita inginkan (misal: 37.5°C).
// Input: Suhu aktual yang dibaca dari sensor.
// Output: Hasil perhitungan PID (seberapa keras pemanas harus bekerja).
double Kp = 5.0, Ki = 0.3, Kd = 1.0; // Konstanta tuning PID. Mungkin perlu disesuaikan.
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT); // Membuat objek PID.

// --- Konfigurasi PWM (Pulse Width Modulation) untuk Kipas ---
// PWM digunakan untuk mengatur kecepatan kipas, bukan hanya ON/OFF.
const int pwmChannel = 0;      // ESP32 punya 16 channel PWM (0-15). Kita pakai channel 0.
const int pwmFrequency = 5000; // Frekuensi sinyal PWM dalam Hz.
const int pwmResolution = 8;   // Resolusi 8-bit berarti kecepatan kipas dari 0 hingga 255.

// --- Variabel Global ---
// Variabel yang bisa diakses dari semua bagian kode.
int fanSpeed = 128; // Kecepatan kipas awal (nilai 0-255, 128 sekitar 50%).
float currentTemp = 0.0;
float currentHumidity = 0.0;
bool heater_status = false;

// Variabel untuk timer, agar tidak menggunakan delay().
unsigned long lastTime = 0;
const long interval = 2000; // Interval pengiriman data (2000 ms = 2 detik).


// =================================================================================
// LANGKAH 3: FUNGSI SETUP()
// ---------------------------------------------------------------------------------
// Fungsi ini hanya berjalan satu kali saat ESP32 pertama kali dinyalakan atau
// di-reset. Isinya adalah semua persiapan awal.
// =================================================================================

void setup() {
  // Memulai komunikasi Serial dengan komputer pada kecepatan 9600 baud.
  // Ini sangat berguna untuk melihat pesan debug di Serial Monitor (Tools > Serial Monitor).
  Serial.begin(9600);
  Serial.println("\n\n--- INKUBATOR MINI ESP32 MULAI ---");

  // --- Inisialisasi Perangkat Keras ---
  dht.begin(); // Memulai sensor DHT.
  Serial.println("Sensor DHT telah dimulai.");

  pinMode(RELAY_PIN_1, OUTPUT);     // Mengatur pin relay sebagai OUTPUT.
  digitalWrite(RELAY_PIN_1, LOW); // Memastikan pemanas MATI saat pertama kali nyala.
  Serial.println("Pin Relay pemanas telah diatur sebagai OUTPUT dan dimatikan.");

  // Konfigurasi dan mulai channel PWM untuk kipas.
  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(FAN_PWM_PIN, pwmChannel);
  ledcWrite(pwmChannel, fanSpeed); // Set kecepatan kipas ke nilai awal.
  Serial.printf("PWM untuk kipas telah dimulai pada kecepatan %d.\n", fanSpeed);

  /*
   * PENTING! Untuk mengatur waktu RTC pertama kali, hapus tanda komentar di baris bawah ini,
   * isi dengan waktu saat ini, unggah kodenya, lalu beri komentar lagi dan unggah ulang.
   * Format: myRTC.setDS1302Time(detik, menit, jam, hari_minggu, tanggal, bulan, tahun);
   * Contoh: myRTC.setDS1302Time(00, 30, 18, 5, 7, 2024, 4); // Jam 18:30:00, Jumat, 5 Juli 2024
   */
  // myRTC.setDS1302Time(00, 30, 18, 5, 7, 2024, 4); 

  // --- Koneksi ke Jaringan WiFi ---
  Serial.print("Menyambungkan ke WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Memulai koneksi WiFi.

  // Loop ini akan terus berjalan sampai ESP32 berhasil terhubung ke WiFi.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
  Serial.print("Alamat IP ESP32: ");
  Serial.println(WiFi.localIP()); // Cetak alamat IP, ini penting untuk diakses dari web.

  // --- Memulai Server WebSocket ---
  webSocket.begin();                 // Memulai server.
  webSocket.onEvent(webSocketEvent); // Mendaftarkan fungsi 'webSocketEvent' untuk menangani semua pesan.
  Serial.println("Server WebSocket telah dimulai.");

  // --- Memulai Kontrol PID ---
  Setpoint = 37.5;              // Mengatur suhu target ideal untuk telur ayam.
  myPID.SetMode(AUTOMATIC);     // Menghidupkan PID.
  myPID.SetOutputLimits(0, interval); // Mengatur agar output PID berada di antara 0 dan 2000.
                                      // Ini digunakan untuk kontrol relay "time-proportioned".
  Serial.printf("Kontrol PID dimulai dengan Setpoint: %.1f C\n", Setpoint);
  Serial.println("--- Setup Selesai, Sistem Berjalan ---");
}


// =================================================================================
// LANGKAH 4: FUNGSI LOOP()
// ---------------------------------------------------------------------------------
// Fungsi ini berjalan terus-menerus setelah setup() selesai. Ini adalah
// jantung dari program kita.
// =================================================================================

void loop() {
  // 1. Selalu jalankan loop WebSocket untuk memeriksa koneksi dan pesan baru.
  webSocket.loop();

  // 2. Gunakan timer non-blocking untuk menjalankan tugas secara berkala.
  // Ini jauh lebih baik daripada menggunakan delay(), karena tidak menghentikan
  // program dan membuat WebSocket tetap responsif.
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= interval) {
    lastTime = currentTime; // Reset timer.

    // --- Baca Data dari Sensor ---
    currentHumidity = dht.readHumidity();
    currentTemp = dht.readTemperature();

    // Periksa apakah pembacaan sensor berhasil. Jika tidak, cetak pesan error.
    if (isnan(currentHumidity) || isnan(currentTemp)) {
      Serial.println("Gagal membaca data dari sensor DHT!");
      return; // Keluar dari blok ini jika sensor error.
    }

    // --- Jalankan Logika Kontrol PID ---
    Input = currentTemp; // Beri tahu PID berapa suhu saat ini.
    myPID.Compute();     // Minta PID untuk menghitung seberapa panas relay harus bekerja.

    // Logika "Time-Proportioned Output" untuk Relay
    // Jika Output PID = 500, maka relay akan menyala selama 500ms dalam interval 2000ms.
    // Jika Output PID = 2000, maka relay akan menyala terus selama interval tersebut.
    if (millis() - lastTime < Output) {
      digitalWrite(RELAY_PIN_1, HIGH); // Nyalakan pemanas.
      heater_status = true;
    } else {
      digitalWrite(RELAY_PIN_1, LOW); // Matikan pemanas.
      heater_status = false;
    }

    // --- Dapatkan Waktu dari RTC ---
    myRTC.updateTime();

    // --- Kirim Data ke Antarmuka Web ---
    sendDataToWeb();

    // --- Cetak Status ke Serial Monitor (untuk Debugging) ---
    Serial.printf("Suhu: %.2fC | Kelembapan: %.2f%% | Kipas: %d | Pemanas: %s\n",
                  currentTemp, currentHumidity, fanSpeed, heater_status ? "ON" : "OFF");
  }
}


// =================================================================================
// LANGKAH 5: FUNGSI-FUNGSI BANTUAN
// ---------------------------------------------------------------------------------
// Memecah kode menjadi fungsi-fungsi kecil membuat program lebih rapi dan
// mudah dikelola.
// =================================================================================

/**
 * @brief Menangani semua event (kejadian) dari WebSocket.
 * @param num Nomor klien yang terhubung.
 * @param type Tipe event (terhubung, terputus, menerima pesan).
 * @param payload Pesan yang diterima dari klien.
 * @param length Panjang pesan.
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Gunakan 'switch' untuk menangani tipe event yang berbeda.
  switch(type) {
    case WStype_DISCONNECTED: // Jika klien terputus
      Serial.printf("[%u] Klien terputus!\n", num);
      break;

    case WStype_CONNECTED: { // Jika ada klien baru yang terhubung
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Klien terhubung dari IP: %s\n", num, ip.toString().c_str());
      
      // Langsung kirim data status terbaru ke klien yang baru terhubung.
      sendDataToWeb();
      break;
    }

    case WStype_TEXT: // Jika kita menerima pesan teks dari klien (web)
      Serial.printf("[%u] Pesan diterima: %s\n", num, payload);
      
      // Gunakan ArduinoJson untuk mengubah teks JSON menjadi data yang bisa dibaca.
      StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("Gagal parsing JSON: ");
        Serial.println(error.c_str());
        return;
      }

      // Ambil perintah dari JSON. Misal: {"command":"setFan", "value":200}
      const char* command = doc["command"];
      
      if (strcmp(command, "setSetpoint") == 0) {
        Setpoint = doc["value"]; // Ubah Setpoint PID.
        Serial.printf("Setpoint suhu diubah menjadi: %.2f\n", Setpoint);
      } else if (strcmp(command, "setFan") == 0) {
        fanSpeed = doc["value"];     // Ubah kecepatan kipas.
        ledcWrite(pwmChannel, fanSpeed); // Terapkan kecepatan baru ke kipas.
        Serial.printf("Kecepatan kipas diubah menjadi: %d\n", fanSpeed);
      }
      
      // Kirim data terbaru ke semua klien sebagai konfirmasi.
      sendDataToWeb();
      break;
  }
}

/**
 * @brief Mengemas semua data sensor dan status ke dalam format JSON
 *        dan mengirimkannya ke semua klien web yang terhubung.
 */
void sendDataToWeb() {
  // Siapkan dokumen JSON dengan kapasitas 256 byte.
  StaticJsonDocument<256> doc;
  
  // Isi dokumen dengan data-data yang ingin dikirim.
  doc["temperature"] = currentTemp;
  doc["humidity"] = currentHumidity;
  doc["setpoint"] = Setpoint;
  // Ubah nilai PWM (0-255) menjadi persentase (0-100) agar lebih mudah dibaca di web.
  doc["fanSpeed"] = map(fanSpeed, 0, 255, 0, 100); 
  doc["heaterStatus"] = heater_status;
  
  // Format waktu dari RTC menjadi string "HH:MM:SS".
  char timeString[10];
  sprintf(timeString, "%02d:%02d:%02d", myRTC.hours, myRTC.minutes, myRTC.seconds);
  doc["currentTime"] = timeString;

  // Ubah dokumen JSON menjadi sebuah String.
  String output;
  serializeJson(doc, output);

  // Kirim string JSON ke SEMUA klien yang sedang terhubung.
  webSocket.broadcastTXT(output);
}
