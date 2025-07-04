// =========================================================================
// PENTING: GANTI ALAMAT IP DI BAWAH INI SESUAI DENGAN ALAMAT IP ESP32 ANDA!
// Anda bisa melihat alamat IP ESP32 di Serial Monitor Arduino IDE saat
// pertama kali dinyalakan.
// =========================================================================
const ESP32_IP = "192.168.1.8"; // <-- !! GANTI ALAMAT IP INI !!


// --- LANGKAH 1: MENYIAPKAN REFERENSI KE ELEMEN HTML ---
// Mengambil semua elemen dari HTML yang akan kita manipulasi agar mudah diakses.
const wsStatus = document.getElementById('ws-status');
const tempValue = document.getElementById('temp-value');
const humidityValue = document.getElementById('humidity-value');
const timeValue = document.getElementById('time-value');
const setpointDisplay = document.getElementById('setpoint-display');
const heaterStatus = document.getElementById('heater-status');
const fanSpeedLabel = document.getElementById('fan-speed-label');
const setTempBtn = document.getElementById('set-temp-btn');
const setpointInput = document.getElementById('setpoint-input');
const fanSlider = document.getElementById('fan-slider');

let socket; // Variabel untuk menyimpan objek WebSocket.

// --- LANGKAH 2: MEMBUAT FUNGSI UNTUK KONEKSI WEBSOCKET ---
function connectWebSocket() {
    // Membuat koneksi WebSocket ke alamat IP ESP32 pada port 81.
    socket = new WebSocket(`ws://${ESP32_IP}:81/`);

    // Fungsi ini akan dijalankan saat koneksi BERHASIL dibuka.
    socket.onopen = function(event) {
        console.log("Koneksi WebSocket berhasil dibuka.");
        // Ubah tampilan badge status menjadi "Terhubung" dengan warna hijau.
        wsStatus.textContent = "Terhubung";
        wsStatus.className = 'badge bg-success';
    };

    // Fungsi ini dijalankan saat koneksi DITUTUP (baik sengaja atau karena error).
    socket.onclose = function(event) {
        console.log("Koneksi WebSocket ditutup. Mencoba menghubungkan kembali dalam 3 detik...");
        // Ubah tampilan badge status menjadi "Terputus" dengan warna merah.
        wsStatus.textContent = "Terputus";
        wsStatus.className = 'badge bg-danger';
        // Coba hubungkan kembali setelah 3 detik. Ini membuat sistem tangguh.
        setTimeout(connectWebSocket, 3000);
    };

    // Fungsi ini dijalankan jika terjadi ERROR pada koneksi.
    socket.onerror = function(error) {
        console.error("Terjadi error pada WebSocket:", error);
        socket.close(); // Tutup koneksi untuk memicu event 'onclose' dan reconnect.
    };

    // Fungsi ini adalah bagian paling penting, dijalankan setiap kali ada PESAN MASUK dari ESP32.
    socket.onmessage = function(event) {
        try {
            // Data dari ESP32 dikirim sebagai string JSON, jadi kita perlu mengubahnya menjadi objek.
            const data = JSON.parse(event.data);
            console.log("Data diterima dari ESP32:", data);

            // Panggil fungsi untuk memperbarui semua elemen di halaman web.
            updateUI(data);

        } catch (e) {
            console.error("Gagal mem-parsing data JSON:", e);
        }
    };
}

// --- LANGKAH 3: MEMBUAT FUNGSI UNTUK MEMPERBARUI TAMPILAN (UI) ---
function updateUI(data) {
    // Memperbarui teks pada elemen HTML dengan data yang diterima.
    // toFixed(1) digunakan untuk membulatkan angka menjadi 1 desimal.
    tempValue.textContent = `${data.temperature.toFixed(1)} °C`;
    humidityValue.textContent = `${data.humidity.toFixed(0)} %`;
    timeValue.textContent = data.currentTime;
    setpointDisplay.textContent = data.setpoint.toFixed(1);

    // Update label dan posisi slider kipas.
    fanSpeedLabel.textContent = data.fanSpeed;
    // Cek agar posisi slider tidak diupdate jika pengguna sedang menggesernya.
    if (document.activeElement !== fanSlider) {
        fanSlider.value = data.fanSpeed;
    }

    // Update status pemanas (heater) menggunakan fungsi bantuan.
    updateBadge(heaterStatus, data.heaterStatus, "AKTIF", "MATI");
}

// Fungsi bantuan untuk mengubah tampilan badge (status ON/OFF).
function updateBadge(element, status, onText, offText) {
    if (status) { // Jika statusnya true (ON)
        element.textContent = onText;
        element.className = 'badge bg-success rounded-pill fs-6'; // Warna hijau
    } else { // Jika statusnya false (OFF)
        element.textContent = offText;
        element.className = 'badge bg-secondary rounded-pill fs-6'; // Warna abu-abu
    }
}

// --- LANGKAH 4: MEMBUAT FUNGSI UNTUK MENGIRIM PERINTAH KE ESP32 ---
function sendCommand(command, value) {
    // Pastikan koneksi WebSocket sedang terbuka sebelum mengirim pesan.
    if (socket && socket.readyState === WebSocket.OPEN) {
        const payload = JSON.stringify({ command, value });
        socket.send(payload);
        console.log("Perintah dikirim ke ESP32:", payload);
    } else {
        console.error("WebSocket tidak terhubung. Perintah tidak dapat dikirim.");
    }
}

// --- LANGKAH 5: MENAMBAHKAN EVENT LISTENER PADA ELEMEN KONTROL ---

// Saat tombol "Atur" suhu di-klik.
setTempBtn.addEventListener('click', () => {
    const value = parseFloat(setpointInput.value);
    if (!isNaN(value)) { // Pastikan nilai yang dimasukkan adalah angka
        sendCommand('setSetpoint', value);
    }
});

// Saat slider kipas digeser.
// Event 'input' akan berjalan secara real-time saat slider digeser.
fanSlider.addEventListener('input', () => {
    const percentage = parseInt(fanSlider.value);
    fanSpeedLabel.textContent = percentage; // Update label persentase secara langsung.
    // Konversi persentase (0-100) ke nilai PWM (0-255) yang dimengerti ESP32.
    const pwmValue = Math.round(percentage * 2.55);
    sendCommand('setFan', pwmValue);
});

// --- LANGKAH 6: MEMULAI SEMUANYA ---
// Panggil fungsi connectWebSocket saat halaman selesai dimuat.
window.addEventListener('load', connectWebSocket);
