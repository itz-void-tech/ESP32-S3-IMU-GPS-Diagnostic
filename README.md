# 📡 ESP32-S3 Advanced Telemetry Dashboard

<table>
  <tr>
    <td><img src="imu-images/dashboard.jfif" alt="Photo 1" width="200"></td>
    <td><img src="imu-images/dashboard2.jfif" alt="Photo 2" width="200"></td>
    <td><img src="imu-images/working.jfif" alt="Photo 3" width="200"></td>
  </tr>
</table>

## 📌 Overview

This project transforms the ESP32-S3 into a full telemetry station with:

- 🧭 Real-time compass heading
- 📡 GPS tracking with satellite status
- 📊 MPU6050 accelerometer + gyroscope data
- 🧠 Dynamic magnetometer calibration (auto-save)
- 🌐 Live web dashboard (Access Point mode)

The system hosts its own WiFi network and serves a responsive dashboard with live sensor data updates at 5Hz.

---

## 🚀 Features

- Auto-detects **HMC5883L or QMC5883L**
- Real-time heading calculation with declination support
- Persistent magnetometer calibration (stored in flash)
- Gyroscope bias calibration
- GPS fix detection
- SoftAP mode (no router required)
- Live compass UI + progress bar calibration

---

## 🧰 Hardware Required

- ESP32-S3
- MPU6050 (I2C)
- HMC5883L or QMC5883L Magnetometer (I2C)
- NEO-6M GPS Module (UART)
- Jumper wires
- Stable 5V power source

---

## 🔌 Wiring Connections

### I2C Bus (Shared)

| Device | ESP32-S3 Pin |
|--------|--------------|
| SDA    | GPIO 8 |
| SCL    | GPIO 9 |
| VCC    | 3.3V |
| GND    | GND |

---

### MPU6050 Connections

| MPU6050 Pin | Connect To ESP32 |
|-------------|------------------|
| VCC         | 3.3V |
| GND         | GND |
| SDA         | GPIO 8 |
| SCL         | GPIO 9 |

I2C Address: `0x68`

---

### Magnetometer Connections (HMC5883L / QMC5883L)

| Magnetometer Pin | Connect To ESP32 |
|------------------|------------------|
| VCC              | 3.3V |
| GND              | GND |
| SDA              | GPIO 8 |
| SCL              | GPIO 9 |

Auto-detected addresses:
- QMC5883L → `0x0D`
- HMC5883L → `0x1E`

---

### GPS (NEO-6M) Connections

| GPS Pin | Connect To ESP32 |
|----------|------------------|
| TX       | GPIO 17 |
| RX       | GPIO 18 |
| VCC      | 5V |
| GND      | GND |

Baud Rate: `9600`

---

## 🌐 WiFi Access

After boot:

- SSID: `ESP32_TELEMETRY`
- Password: `12345678`
- Open browser → `192.168.4.1`

No router needed.

---

## 🧭 Magnetometer Calibration

1. Click **Start Real Calibration**
2. Move sensor in slow 3D figure-8 motion
3. Tilt diagonally (not just flat)
4. Wait until progress reaches 100%
5. Offsets auto-save to flash

Reset option available.

---

## 📊 Dashboard Displays

- Heading (°)
- Compass UI
- Accelerometer values
- Magnetometer offsets
- GPS satellite count
- Latitude / Longitude
- Sensor health status

---

## 📦 Required Libraries

Install via Arduino Library Manager:

- TinyGPSPlus
- Preferences (built-in ESP32)
- WebServer (ESP32 built-in)
- Wire (built-in)

Board Package:
- ESP32 by Espressif Systems

---

## ⚡ Power Notes

- Use stable 5V supply.
- Keep I2C wires short.
- Avoid metal objects during magnetometer calibration.
- GPS requires outdoor sky visibility for fix.

---

## 🔮 Future Expansion Ideas

- Sensor fusion (Kalman filter)
- Data logging to SD card
- Real-time map integration
- WebSocket streaming instead of polling
- OTA firmware updates
- Web-based declination auto calculation

---

## 👨‍💻 Author

**Swarnendu**  
Founder — **itz-void-tech**

Embedded Systems | Robotics | IoT | ESP32 Development  
Building intelligent hardware with real-world capability.

---

