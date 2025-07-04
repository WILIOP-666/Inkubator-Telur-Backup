/*
 ===================================================================================
 * NAMA PROYEK: INKUBATOR TELUR MINI (REVISI LOGIKA KONTROL)
 * DESKRIPSI:   Firmware yang telah direvisi total.
 *              - PID mengontrol KIPAS PENDINGIN (mode REVERSE).
 *              - Lampu pemanas dikontrol secara manual via web.
 *              - Menggunakan RTC DS3231 (via RTClib) dan Preferences.h.
 *              - Mengimplementasikan penghitung hari (day counter) non-volatile.
 * BOARD:       WEMOS/LOLIN D1 R32 (ESP32)
 * AUTHOR:      Gemini
 ===================================================================================
*/

// =================================================================================
// LANGKAH 1: MENYERTAKAN PUSTAKA (LIBRARIES)
// =================================================================================
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <PID_v1.h>

// Pustaka baru untuk RTC I2C dan penyimpanan non-volatile
#include <Wire.h>             // Diperlukan untuk komunikasi I2C
#include <RTClib.h>           // Pustaka dari Adafruit untuk modul RTC
#include <Preferences.h>      // Untuk menyimpan data di memori flash ESP32

// =================================================================================
// LANGKAH 2: KONFIGURASI UTAMA
// =================================================================================

// --- Konfigurasi Jaringan WiFi ---
const char* ssid = "NYUDIS";      // <-- GANTI DENGAN NAMA WIFI ANDA
const char* password = "87654321C"; // <-- GANTI DENGAN PASSWORD WIFI ANDA

// --- Konfigurasi Pin Perangkat Keras ---
#define DHT_PIN     23 // Pin sensor suhu dan kelembapan
#define DHT_TYPE    DHT11
#define HEATER_PIN  18 // Pin relay untuk lampu pemanas (sekarang manual)
#define FAN_PWM_PIN 19 // Pin PWM untuk kipas (sekarang dikontrol PID)

// --- Inisialisasi Objek dari Pustaka ---
WebSocketsServer webSocket = WebSocketsServer(81);
DHT dht(DHT_PIN, DHT_TYPE);
RTC_DS3231 rtc;      // Membuat objek untuk modul RTC DS3231
Preferences preferences; // Membuat objek untuk penyimpanan NVS

// --- Konfigurasi Kontrol PID untuk KIPAS PENDINGIN ---
double Setpoint, Input, Output;
double Kp = 150.0, Ki = 10.0, Kd = 5.0; // Tuning PID untuk kipas mungkin perlu nilai Kp yang lebih tinggi
// REVERSE: Jika suhu (Input) NAIK di atas Setpoint, kecepatan kipas (Output) akan NAIK.
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, REVERSE);

// --- Konfigurasi PWM untuk Kipas ---
const int pwmChannel = 0;
const int pwmFrequency = 5000;
const int pwmResolution = 8; // Menghasilkan output 0-255

// --- Variabel Global untuk Penghitung Hari (Day Counter) ---
uint32_t startTime = 0; // Menyimpan Unix timestamp kapan counter dimulai. 0 = belum dimulai.
// Variabel untuk menyimpan hasil perhitungan waktu yang telah berlalu
uint8_t elapsedDays = 0, elapsedHours = 0, elapsedMins = 0, elapsedSecs = 0;

// Variabel Global lainnya
float currentTemp = 0.0;
float currentHumidity = 0.0;
int fanSpeed = 0;
bool heater_status = false;

// Variabel untuk timer
unsigned long lastTime = 0;
const long interval = 2000; // Interval 2 detik

// =================================================================================
// LANGKAH 3: FUNGSI SETUP()
// =================================================================================
void setup() {
  Serial.begin(9600);
  Serial.println("\n\n--- INKUBATOR MINI (REVISI LOGIKA) MULAI ---");

  // --- Mulai Penyimpanan Preferences ---
  preferences.begin("incubator", false); // Membuka namespace "incubator". false = mode baca/tulis.
  Serial.println("Penyimpanan NVS (Preferences) dimulai.");

  // --- Inisialisasi RTC DS3231 ---
  Wire.begin(); // Memulai bus I2C (SDA=21, SCL=22)
  if (!rtc.begin()) {
    Serial.println("Tidak dapat menemukan modul RTC! Periksa koneksi.");
    while (1);
  }
  // Jika RTC kehilangan daya (misal: baterai habis), atur waktu sesuai waktu kompilasi.
  if (rtc.lostPower()) {
    Serial.println("RTC kehilangan daya, waktu diatur sesuai waktu kompilasi!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("Modul RTC DS3231 berhasil dimulai.");

  // --- Memuat Data Penghitung dari Penyimpanan ---
  startTime = preferences.getUInt("startTime", 0); // Ambil startTime. Jika tidak ada, default ke 0.
  if (startTime == 0) {
    Serial.println("Penghitung hari belum dimulai.");
  } else {
    Serial.printf("Penghitung hari ditemukan, dimulai pada timestamp: %u\n", startTime);
  }

  // --- Inisialisasi Perangkat Keras Lainnya ---
  dht.begin();
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, LOW); // Pastikan pemanas mati saat awal
  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(FAN_PWM_PIN, pwmChannel);

  // --- Koneksi WiFi ---
  Serial.print("Menyambungkan ke WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
  Serial.print("Alamat IP ESP32: ");
  Serial.println(WiFi.localIP());

  // --- Server WebSocket ---
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // --- Memulai Kontrol PID untuk Kipas ---
  Setpoint = 37.8; // Target suhu MAKSIMUM. Kipas akan berusaha menjaga suhu di bawah ini.
  myPID.SetOutputLimits(0, 255); // Batas output PID adalah rentang PWM kipas (0-255).
  myPID.SetMode(AUTOMATIC);
  Serial.printf("Kontrol PID untuk kipas dimulai dengan Setpoint: %.1f C\n", Setpoint);
  Serial.println("--- Setup Selesai, Sistem Berjalan ---");
}

// =================================================================================
// LANGKAH 4: FUNGSI LOOP()
// =================================================================================
void loop() {
  webSocket.loop(); // Selalu jalankan loop WebSocket

  unsigned long currentTime = millis();
  if (currentTime - lastTime >= interval) {
    lastTime = currentTime;

    // Baca sensor
    currentHumidity = dht.readHumidity();
    currentTemp = dht.readTemperature();
    if (isnan(currentHumidity) || isnan(currentTemp)) {
      Serial.println("Gagal membaca data dari sensor DHT!");
      return;
    }

    // Jalankan Logika PID untuk Kipas
    Input = currentTemp;
    myPID.Compute();
    fanSpeed = (int)Output; // Simpan nilai output PID sebagai kecepatan kipas
    ledcWrite(pwmChannel, fanSpeed);

    // Hitung waktu yang telah berlalu
    calculateElapsedTime();

    // Kirim semua data ke web
    sendDataToWeb();
    
    // Cek status pemanas (dikontrol manual)
    heater_status = digitalRead(HEATER_PIN);
  }
}

// =================================================================================
// LANGKAH 5: FUNGSI-FUNGSI BANTUAN
// =================================================================================

/**
 * @brief Menghitung waktu yang telah berlalu sejak counter dimulai.
 */
void calculateElapsedTime() {
  if (startTime == 0) { // Jika counter belum dimulai, set semua ke 0.
    elapsedDays = 0; elapsedHours = 0; elapsedMins = 0; elapsedSecs = 0;
    return;
  }
  
  DateTime now = rtc.now();
  unsigned long elapsedSeconds = now.unixtime() - startTime;

  elapsedDays = elapsedSeconds / 86400;
  unsigned long remainder = elapsedSeconds % 86400;
  elapsedHours = remainder / 3600;
  remainder = remainder % 3600;
  elapsedMins = remainder / 60;
  elapsedSecs = remainder % 60;
}


/**
 * @brief Menangani semua event dari WebSocket.
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Klien terputus!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Klien terhubung dari IP: %s\n", num, ip.toString().c_str());
      sendDataToWeb();
      break;
    }
    case WStype_TEXT:
      Serial.printf("[%u] Pesan diterima: %s\n", num, payload);
      
      StaticJsonDocument<128> doc;
      deserializeJson(doc, payload);

      const char* command = doc["command"];
      
      if (strcmp(command, "setSetpoint") == 0) {
        Setpoint = doc["value"];
      } else if (strcmp(command, "toggleHeater") == 0) {
        digitalWrite(HEATER_PIN, !digitalRead(HEATER_PIN));
      } else if (strcmp(command, "startCounter") == 0) {
        // Jika counter belum dimulai, mulai sekarang.
        if (startTime == 0) {
          DateTime now = rtc.now();
          startTime = now.unixtime();
          preferences.putUInt("startTime", startTime); // Simpan ke NVS!
          Serial.printf("Penghitung dimulai pada timestamp: %u\n", startTime);
        }
      } else if (strcmp(command, "resetCounter") == 0) {
        startTime = 0;
        preferences.putUInt("startTime", 0); // Simpan nilai 0 ke NVS!
        Serial.println("Penghitung direset.");
      }
      
      sendDataToWeb(); // Kirim data terbaru sebagai konfirmasi
      break;
  }
}

/**
 * @brief Mengemas dan mengirim semua data ke antarmuka web.
 */
void sendDataToWeb() {
  StaticJsonDocument<256> doc;
  
  doc["temperature"] = currentTemp;
  doc["humidity"] = currentHumidity;
  doc["setpoint"] = Setpoint;
  doc["fanSpeed"] = map(fanSpeed, 0, 255, 0, 100);
  doc["heaterStatus"] = digitalRead(HEATER_PIN);
  
  // Tambahkan data penghitung
  doc["counterDays"] = elapsedDays;
  doc["counterHours"] = elapsedHours;
  doc["counterMins"] = elapsedMins;
  doc["counterSecs"] = elapsedSecs;
  
  String output;
  serializeJson(doc, output);
  webSocket.broadcastTXT(output);
}
