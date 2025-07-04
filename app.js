// =========================================================================
// PENTING: GANTI ALAMAT IP DI BAWAH INI SESUAI DENGAN ALAMAT IP ESP32 ANDA!
// =========================================================================
const ESP32_IP = "192.168.1.8"; // <-- !! GANTI ALAMAT IP INI !!


// --- LANGKAH 1: MENYIAPKAN REFERENSI KE ELEMEN HTML ---
// Status Koneksi
const wsStatus = document.getElementById('ws-status');
// Panel Monitoring
const tempValue = document.getElementById('temp-value');
const humidityValue = document.getElementById('humidity-value');
const setpointDisplay = document.getElementById('setpoint-display');
// Panel Penghitung Waktu
const counterDays = document.getElementById('counter-days');
const counterHours = document.getElementById('counter-hours');
const counterMins = document.getElementById('counter-mins');
const counterSecs = document.getElementById('counter-secs');
// Panel Kontrol
const setTempBtn = document.getElementById('set-temp-btn');
const setpointInput = document.getElementById('setpoint-input');
const toggleHeaterBtn = document.getElementById('toggle-heater-btn');
const startCounterBtn = document.getElementById('start-counter-btn');
const resetCounterBtn = document.getElementById('reset-counter-btn');
// Panel Status Aktuator
const heaterStatus = document.getElementById('heater-status');
const fanStatus = document.getElementById('fan-status');

let socket; // Variabel untuk menyimpan objek WebSocket.

// --- LANGKAH 2: FUNGSI KONEKSI WEBSOCKET (Sama seperti sebelumnya) ---
function connectWebSocket() {
    socket = new WebSocket(`ws://${ESP32_IP}:81/`);

    socket.onopen = function(event) {
        console.log("Koneksi WebSocket berhasil dibuka.");
        wsStatus.textContent = "Terhubung";
        wsStatus.className = 'badge bg-success';
    };

    socket.onclose = function(event) {
        console.log("Koneksi WebSocket ditutup. Mencoba menghubungkan kembali dalam 3 detik...");
        wsStatus.textContent = "Terputus";
        wsStatus.className = 'badge bg-danger';
        setTimeout(connectWebSocket, 3000);
    };

    socket.onerror = function(error) {
        console.error("Terjadi error pada WebSocket:", error);
        socket.close();
    };

    socket.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            console.log("Data diterima dari ESP32:", data);
            updateUI(data);
        } catch (e) {
            console.error("Gagal mem-parsing data JSON:", e);
        }
    };
}

// --- LANGKAH 3: FUNGSI UNTUK MEMPERBARUI TAMPILAN (UI) ---
function updateUI(data) {
    // Perbarui panel monitoring
    tempValue.textContent = `${data.temperature.toFixed(1)} °C`;
    humidityValue.textContent = `${data.humidity.toFixed(0)} %`;
    setpointDisplay.textContent = data.setpoint.toFixed(1);

    // Perbarui panel penghitung waktu
    counterDays.textContent = data.counterDays;
    counterHours.textContent = data.counterHours;
    counterMins.textContent = data.counterMins;
    counterSecs.textContent = data.counterSecs;

    // Perbarui panel status aktuator
    updateBadge(heaterStatus, data.heaterStatus, "AKTIF", "MATI", "success", "secondary");
    fanStatus.textContent = `${data.fanSpeed}%`; // Langsung tampilkan persentase kecepatan kipas
}

// Fungsi bantuan untuk mengubah tampilan badge, kini lebih fleksibel
function updateBadge(element, status, onText, offText, onClass, offClass) {
    if (status) {
        element.textContent = onText;
        element.className = `badge bg-${onClass} rounded-pill fs-6`;
    } else {
        element.textContent = offText;
        element.className = `badge bg-${offClass} rounded-pill fs-6`;
    }
}

// --- LANGKAH 4: FUNGSI UNTUK MENGIRIM PERINTAH KE ESP32 ---
function sendCommand(command, value = null) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        const payload = JSON.stringify({ command, value });
        socket.send(payload);
        console.log("Perintah dikirim ke ESP32:", payload);
    } else {
        console.error("WebSocket tidak terhubung. Perintah tidak dapat dikirim.");
    }
}

// --- LANGKAH 5: MENAMBAHKAN EVENT LISTENER PADA ELEMEN KONTROL ---

// Tombol "Atur" suhu
setTempBtn.addEventListener('click', () => {
    const value = parseFloat(setpointInput.value);
    if (!isNaN(value)) {
        sendCommand('setSetpoint', value);
    }
});

// Tombol Kontrol Pemanas
toggleHeaterBtn.addEventListener('click', () => {
    sendCommand('toggleHeater');
});

// Tombol Mulai Penghitung
startCounterBtn.addEventListener('click', () => {
    if (confirm("Apakah Anda yakin ingin memulai penghitung waktu inkubasi? Tindakan ini tidak dapat diulang kecuali direset.")) {
        sendCommand('startCounter');
    }
});

// Tombol Reset Penghitung
resetCounterBtn.addEventListener('click', () => {
    if (confirm("PERINGATAN! Apakah Anda yakin ingin mereset penghitung waktu? Seluruh progres waktu akan hilang.")) {
        sendCommand('resetCounter');
    }
});


// --- LANGKAH 6: MEMULAI SEMUANYA ---
window.addEventListener('load', connectWebSocket);
