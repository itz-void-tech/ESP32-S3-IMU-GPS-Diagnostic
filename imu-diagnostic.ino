#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <math.h>

// ================= PINS & CONFIG =================
#define I2C_SDA 8
#define I2C_SCL 9
#define GPS_RX_PIN 17 // Connected to GPS TX
#define GPS_TX_PIN 18 // Connected to GPS RX

#define MPU_ADDR 0x68
#define HMC_ADDR 0x1E
#define QMC_ADDR 0x0D

// Calibration Tuning
const int REQUIRED_UNIQUE_SAMPLES = 800; // How many distinct points needed for good cal

// ================= GLOBALS =================
WebServer server(80);
Preferences preferences;
TinyGPSPlus gps;

// Sensor Status Flags
bool mpuOk = false;
bool magOk = false;
uint8_t magAddr = 0x00;
String magType = "NONE";
bool isHMC = false;

// Sensor Data
float accX, accY, accZ;
float gyroX, gyroY, gyroZ;
float magX, magY, magZ;
float heading = 0.0;
float declination = 0.0; // Adjust for your city (e.g., -14.0 for deep negative, +ve for positive)

// Calibration Data
float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;
float magOffsetX = 0, magOffsetY = 0, magOffsetZ = 0;

// Dynamic Mag Calibration State Machine
bool isCalibratingMag = false;
int magCalSamplesCollected = 0;
int16_t lastRawX, lastRawY, lastRawZ; // For uniqueness check
float magMinX = 32767, magMaxX = -32768;
float magMinY = 32767, magMaxY = -32768;
float magMinZ = 32767, magMaxZ = -32768;

// Timing
unsigned long lastConsoleUpdate = 0;

// ================= WEB DASHBOARD HTML/CSS/JS =================
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Advanced Telemetry</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { background-color: #0d1117; color: #00ff00; font-family: 'Courier New', Courier, monospace; margin: 10px; display: flex; flex-direction: column; align-items: center;}
    h1 { color: #58a6ff; border-bottom: 1px solid #30363d; padding-bottom: 10px; width: 100%; text-align: center; font-size: 1.2em;}
    .container { max-width: 600px; width: 100%; }
    .card { background: #161b22; border: 1px solid #30363d; border-radius: 6px; padding: 15px; margin-bottom: 15px; }
    button { background: #238636; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; margin-right: 10px; width: 100%; margin-bottom: 10px;}
    button:hover { background: #2ea043; }
    .btn-danger { background: #da3633; }
    .btn-danger:hover { background: #f85149; }
    button:disabled { background: #444; cursor: not_allowed; }
    table { width: 100%; text-align: left; font-size: 0.8em; }
    th, td { padding: 3px; }
    
    /* Progress Bar */
    #progress-container { width: 100%; background-color: #30363d; border-radius: 5px; margin: 10px 0; display: none; }
    #progress-bar { width: 0%; height: 20px; background-color: #238636; text-align: center; line-height: 20px; color: white; border-radius: 5px; transition: width 0.1s; }
    
    #instructions { color: #8b949e; font-size: 0.85em; display: none; border-left: 3px solid #f2cc60; padding-left: 10px; margin-bottom: 10px;}

    /* Compass Graphic */
    #compass-svg { width: 150px; height: 150px; display: block; margin: 10px auto; }
    .compass-ring { stroke: #30363d; stroke-width: 2; fill: none; }
    .compass-degree-mark { stroke: #30363d; stroke-width: 1; }
    .compass-text { fill: #8b949e; font-size: 14px; text-anchor: middle; font-family: sans-serif;}
    .compass-text-main { fill: #ffffff; font-weight: bold; font-size: 18px;}
    #compass-needle { fill: #da3633; transition: transform 0.2s ease-out; transform-origin: 75px 75px; }
    #heading-val { font-size: 2em; color: #58a6ff; text-align: center; }
  </style>
</head>
<body>
  <h1>[ ESP32-S3 ] TELEMETRY DASHBOARD</h1>
  
  <div class="container">
    <div class="card">
      <h3>MAGNETOMETER CALIBRATION</h3>
      
      <div id="instructions">
        <strong>ACTION REQUIRED:</strong><br>
        1. Keep sensor FLAT on a wooden/plastic table for 2 seconds.<br>
        2. Slowly lift and rotate in a slow Figure-8 motion in the air.<br>
        3. Tilt the sensor diagonally while rotating (not just flat yaw).<br>
        4. Keep away from metal/batteries.<br>
        <em>Calibration completes automatically when bar reaches 100%.</em>
      </div>

      <div id="progress-container">
        <div id="progress-bar">0%</div>
      </div>
      
      <p id="cal-status" style="color:#00ff00; text-align:center;">Status: IDLE (Offsets loaded)</p>
      
      <button id="btn-start" onclick="startCalibration()">Start Real Calibration</button>
      <button class="btn-danger" id="btn-reset" onclick="fetch('/reset_cal')">Reset to Defaults</button>
    </div>

    <div class="card">
      <div id="heading-val">0.0&deg;</div>
      <svg id="compass-svg" viewBox="0 0 150 150">
        <circle class="compass-ring" cx="75" cy="75" r="70"/>
        
        <line class="compass-degree-mark" x1="75" y1="5" x2="75" y2="15" /> <line class="compass-degree-mark" x1="145" y1="75" x2="135" y2="75" /> <line class="compass-degree-mark" x1="75" y1="145" x2="75" y2="135" /> <line class="compass-degree-mark" x1="5" y1="75" x2="15" y2="75" /> <text x="75" y="30" class="compass-text compass-text-main">N</text>
        <text x="130" y="80" class="compass-text">E</text>
        <text x="75" y="135" class="compass-text">S</text>
        <text x="20" y="80" class="compass-text">W</text>
        
        <g id="compass-needle">
          <polygon points="75,10 85,75 75,90 65,75" />
          <circle cx="75" cy="75" r="5" fill="#58a6ff"/>
        </g>
      </svg>
    </div>

    <div class="card">
      <h3>SYSTEM DATA</h3>
      <div id="data">Loading...</div>
    </div>
  </div>

  <script>
    function startCalibration() {
      document.getElementById('instructions').style.display = 'block';
      document.getElementById('progress-container').style.display = 'block';
      document.getElementById('btn-start').disabled = true;
      document.getElementById('btn-reset').disabled = true;
      fetch('/start_cal');
    }

    setInterval(() => {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          // Update Compass
          document.getElementById('compass-needle').style.transform = `rotate(${data.heading}deg)`;
          document.getElementById('heading-val').innerHTML = `${data.heading.toFixed(1)}&deg;`;

          // Update Calibration UI
          if(data.calibrating) {
            document.getElementById('cal-status').innerText = `Status: COLLECTING DATA...`;
            document.getElementById('cal-status').style.color = "#f2cc60";
            document.getElementById('progress-bar').style.width = `${data.cal_pct}%`;
            document.getElementById('progress-bar').innerText = `${data.cal_pct}%`;
            document.getElementById('instructions').style.display = 'block';
            document.getElementById('progress-container').style.display = 'block';
            document.getElementById('btn-start').disabled = true;
            document.getElementById('btn-reset').disabled = true;
          } else {
            document.getElementById('cal-status').innerText = "Status: IDLE (Offsets active)";
            document.getElementById('cal-status').style.color = "#00ff00";
            document.getElementById('instructions').style.display = 'none';
            document.getElementById('progress-container').style.display = 'none';
            document.getElementById('btn-start').disabled = false;
            document.getElementById('btn-reset').disabled = false;
          }

          // Update Raw Data Table
          document.getElementById('data').innerHTML = `
            <table>
              <tr><th>MPU6050</th><td>${data.mpuOk ? 'OK' : '<span style="color:red">FAIL</span>'}</td></tr>
              <tr><th>Mag Sensor</th><td>${data.magOk ? 'OK ('+data.magType+')' : '<span style="color:red">FAIL</span>'}</td></tr>
              <tr><th>Mag Offsets</th><td>X:${data.offX}, Y:${data.offY}, Z:${data.offZ}</td></tr>
              <tr><th>GPS Satellites</th><td>${data.sats} (${data.fix ? 'YES FIX' : '<span style="color:orange; font-weight:bold;">NO FIX</span>'})</td></tr>
              <tr><th>Latitude</th><td>${data.lat}</td></tr>
              <tr><th>Longitude</th><td>${data.lon}</td></tr>
              <tr><th>Accel (X,Y,Z)</th><td>${data.aX.toFixed(2)}, ${data.aY.toFixed(2)}, ${data.aZ.toFixed(2)}</td></tr>
            </table>
          `;
        });
    }, 200); // 5Hz update
  </script>
</body>
</html>
)=====";

// ================= FUNCTIONS =================

void scanI2C() {
  Serial.println("================================================");
  Serial.println("[ SYSTEM BOOT ] ESP32-S3 ONLINE");
  Serial.print("[I2C SCAN] Found: ");
  bool foundAny = false;
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() == 0) { Serial.print("0x68 (MPU) "); foundAny = true; }
  
  Wire.beginTransmission(QMC_ADDR);
  if (Wire.endTransmission() == 0) { Serial.print("0x0D (QMC) "); foundAny = true; }
  else {
    Wire.beginTransmission(HMC_ADDR);
    if (Wire.endTransmission() == 0) { Serial.print("0x1E (HMC) "); foundAny = true; }
  }

  if (!foundAny) Serial.print("NONE");
  Serial.println();
}

void initMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00); // Wake up MPU
  if (Wire.endTransmission() == 0) {
    mpuOk = true; Serial.println("[MPU6050] STATUS: OK");
  } else { Serial.println("[MPU6050] STATUS: FAIL"); }
}

void initMag() {
  // Try QMC5883L
  Wire.beginTransmission(QMC_ADDR);
  if (Wire.endTransmission() == 0) {
    magOk = true; magAddr = QMC_ADDR; magType = "QMC5883L"; isHMC = false;
    Wire.beginTransmission(magAddr); Wire.write(0x09); Wire.write(0x1D); Wire.endTransmission(); // 200Hz, 8G
    Wire.beginTransmission(magAddr); Wire.write(0x0B); Wire.write(0x01); Wire.endTransmission();
    Serial.println("[MAG] STATUS: OK (QMC5883L)");
    return;
  }
  
  // Try HMC5883L
  Wire.beginTransmission(HMC_ADDR);
  if (Wire.endTransmission() == 0) {
    magOk = true; magAddr = HMC_ADDR; magType = "HMC5883L"; isHMC = true;
    Wire.beginTransmission(magAddr); Wire.write(0x02); Wire.write(0x00); Wire.endTransmission(); // Continuous
    Serial.println("[MAG] STATUS: OK (HMC5883L)");
    return;
  }
  Serial.println("[MAG] STATUS: FAIL");
}

void initGPS() {
  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("[GPS] STATUS: INITIALIZED (WAITING)");
}

void calibrateGyro() {
  if (!mpuOk) return;
  Serial.println("[GYRO] Calibrating... Keep sensor FLAT and STILL for 2 seconds.");
  long sumX = 0, sumY = 0, sumZ = 0;
  for (int i = 0; i < 2000; i++) {
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x43); Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true);
    sumX += (int16_t)(Wire.read() << 8 | Wire.read());
    sumY += (int16_t)(Wire.read() << 8 | Wire.read());
    sumZ += (int16_t)(Wire.read() << 8 | Wire.read());
    delay(1);
  }
  gyroBiasX = sumX / 2000.0; gyroBiasY = sumY / 2000.0; gyroBiasZ = sumZ / 2000.0;
  Serial.println("[GYRO] STATUS: CALIBRATED");
}

void loadCalibration() {
  preferences.begin("mag_cal", true); // Read-only mode
  magOffsetX = preferences.getFloat("offX", 0.0);
  magOffsetY = preferences.getFloat("offY", 0.0);
  magOffsetZ = preferences.getFloat("offZ", 0.0);
  preferences.end();
}

void saveCalibration() {
  preferences.begin("mag_cal", false); // Read/Write mode
  preferences.putFloat("offX", magOffsetX);
  preferences.putFloat("offY", magOffsetY);
  preferences.putFloat("offZ", magOffsetZ);
  preferences.end();
}

void readMPU() {
  if (!mpuOk) return;
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);
  accX = (int16_t)(Wire.read() << 8 | Wire.read()) / 16384.0;
  accY = (int16_t)(Wire.read() << 8 | Wire.read()) / 16384.0;
  accZ = (int16_t)(Wire.read() << 8 | Wire.read()) / 16384.0;
  Wire.read(); Wire.read(); // Skip Temp
  gyroX = ((int16_t)(Wire.read() << 8 | Wire.read()) - gyroBiasX) / 131.0;
  gyroY = ((int16_t)(Wire.read() << 8 | Wire.read()) - gyroBiasY) / 131.0;
  gyroZ = ((int16_t)(Wire.read() << 8 | Wire.read()) - gyroBiasZ) / 131.0;
}

void readMag() {
  if (!magOk) return;
  int16_t rawX, rawY, rawZ;
  if (isHMC) {
    Wire.beginTransmission(magAddr); Wire.write(0x03); Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)magAddr, (uint8_t)6, (uint8_t)true);
    rawX = (Wire.read() << 8) | Wire.read(); rawZ = (Wire.read() << 8) | Wire.read(); rawY = (Wire.read() << 8) | Wire.read();
  } else {
    Wire.beginTransmission(magAddr); Wire.write(0x00); Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)magAddr, (uint8_t)6, (uint8_t)true);
    rawX = Wire.read() | (Wire.read() << 8); rawY = Wire.read() | (Wire.read() << 8); rawZ = Wire.read() | (Wire.read() << 8);
  }

  // Handle Dynamic Calibration Sampling
  if (isCalibratingMag) {
    // Check if sensor is moving enough to count as a unique point
    if (abs(rawX - lastRawX) > 15 || abs(rawY - lastRawY) > 15 || abs(rawZ - lastRawZ) > 15) {
        if (rawX < magMinX) magMinX = rawX; if (rawX > magMaxX) magMaxX = rawX;
        if (rawY < magMinY) magMinY = rawY; if (rawY > magMaxY) magMaxY = rawY;
        if (rawZ < magMinZ) magMinZ = rawZ; if (rawZ > magMaxZ) magMaxZ = rawZ;
        magCalSamplesCollected++;
        lastRawX = rawX; lastRawY = rawY; lastRawZ = rawZ;
    }
    
    // Real completion check: Enough unique points collected
    if (magCalSamplesCollected >= REQUIRED_UNIQUE_SAMPLES) {
      isCalibratingMag = false;
      magOffsetX = (magMaxX + magMinX) / 2.0;
      magOffsetY = (magMaxY + magMinY) / 2.0;
      magOffsetZ = (magMaxZ + magMinZ) / 2.0;
      saveCalibration();
      Serial.println("[MAG] Dynamic Calibration COMPLETE. Saved.");
    }
  }

  // Apply Offset correction
  magX = rawX - magOffsetX; magY = rawY - magOffsetY; magZ = rawZ - magOffsetZ;
}

void readGPS() {
  while (Serial2.available() > 0) { gps.encode(Serial2.read()); }
}

void calculateHeading() {
  if (!magOk) return;
  heading = atan2(magY, magX) * 180.0 / M_PI;
  heading += declination;
  if (heading < 0) heading += 360;
  if (heading > 360) heading -= 360;
}

void printDiagnostics() {
  if (isCalibratingMag) return; // Don't spam console during cal
  Serial.println("------------------------------------------------");
  Serial.printf("[ACC] X=%.2f Y=%.2f Z=%.2f\n", accX, accY, accZ);
  Serial.printf("[MAG] X=%.0f Y=%.0f Z=%.0f | HEADING=%.1f\n", magX, magY, magZ, heading);
  bool fix = (gps.satellites.value() >= 4);
  Serial.printf("[GPS] SAT=%d FIX=%s LAT=%.6f\n", gps.satellites.value(), fix ? "YES" : "NO", gps.location.lat());
}

void handleWebRequests() {
  server.on("/", []() { server.send(200, "text/html", INDEX_HTML); });

  server.on("/data", []() {
    bool fix = (gps.satellites.value() >= 4);
    int progress = 0;
    if (isCalibratingMag) {
        progress = (int)(( (float)magCalSamplesCollected / REQUIRED_UNIQUE_SAMPLES ) * 100);
        if (progress > 100) progress = 100;
    }

    String json = "{";
    json += "\"mpuOk\":" + String(mpuOk ? "true" : "false") + ",";
    json += "\"magOk\":" + String(magOk ? "true" : "false") + ",";
    json += "\"magType\":\"" + magType + "\",";
    json += "\"heading\":" + String(heading, 1) + ",";
    json += "\"aX\":" + String(accX, 2) + ",\"aY\":" + String(accY, 2) + ",\"aZ\":" + String(accZ, 2) + ",";
    json += "\"offX\":" + String(magOffsetX, 0) + ",\"offY\":" + String(magOffsetY, 0) + ",\"offZ\":" + String(magOffsetZ, 0) + ",";
    json += "\"lat\":" + String(gps.location.lat(), 6) + ",\"lon\":" + String(gps.location.lng(), 6) + ",";
    json += "\"sats\":" + String(gps.satellites.value()) + ",";
    json += "\"fix\":" + String(fix ? "true" : "false") + ",";
    json += "\"calibrating\":" + String(isCalibratingMag ? "true" : "false") + ",";
    json += "\"cal_pct\":" + String(progress);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/start_cal", []() {
    // Reset calibration variables
    magMinX = 32767; magMaxX = -32768; magMinY = 32767; magMaxY = -32768; magMinZ = 32767; magMaxZ = -32768;
    magCalSamplesCollected = 0;
    lastRawX = 0; lastRawY = 0; lastRawZ = 0;
    isCalibratingMag = true; // Start sampling
    Serial.println("[MAG] Starting Dynamic Calibration Sampling...");
    server.send(200, "text/plain", "OK");
  });

  server.on("/reset_cal", []() {
    magOffsetX = 0; magOffsetY = 0; magOffsetZ = 0;
    saveCalibration();
    Serial.println("[MAG] Calibration Reset to Defaults.");
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Wire.begin(I2C_SDA, I2C_SCL);
  
  scanI2C();
  initMPU();
  initMag();
  initGPS();
  
  loadCalibration(); // Load existing magnetometer offsets
  calibrateGyro();   // Calibrate gyro (requires stillness)
  
  WiFi.softAP("ESP32_TELEMETRY", "12345678");
  Serial.print("[WIFI] Access Point Started. IP: "); Serial.println(WiFi.softAPIP());
  
  handleWebRequests();
}

void loop() {
  readMPU();
  readMag(); // Dynamic calibration happens inside here if isCalibratingMag is true
  readGPS();
  calculateHeading();
  server.handleClient();
  
  if (millis() - lastConsoleUpdate > 1000) {
    printDiagnostics();
    lastConsoleUpdate = millis();
  }
}
