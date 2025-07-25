#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include "DFRobot_Alcohol.h"
#include <WebServer.h>

// ====================== WiFi ========================
const char *ssid = "Nữ sinh đáng thương";
const char *password = "lammmmmm";
const char *serverName = "http://192.168.193.172:5000/update";
String ipAddress = "";

// ====================== GPS =========================
TinyGPSPlus gps;
#define GPS_RX 16
#define GPS_TX 17
float latitude = 0.0;
float longitude = 0.0;
String gpsTime = "N/A";  // Thời gian GPS dạng chuỗi
HardwareSerial gpsSerial(2);

// ================== Alcohol Sensor ==================
#define ALCOHOL_I2C_ADDRESS 0x75
#define COLLECT_NUMBER 5
DFRobot_Alcohol_I2C Alcohol(&Wire, ALCOHOL_I2C_ADDRESS);
float alcoholPPM = 0.0;

// ================== Heart Rate Sensor ===============
const int heartPin = 32;
int heartValue = 0;
#define THRESHOLD 540
#define SAMPLE_COUNT 10

// ================== Web Server ======================
WebServer server(80);

// ================== Mutex ===========================
SemaphoreHandle_t dataMutex;
// ================== Web Handlers =====================
void handleDashboard() {
  String ipAddress = WiFi.localIP().toString();

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset='utf-8'>
    <title>ĐỒ ÁN TỐT NGHIỆP</title>
    <style>
      body {
        font-family: 'Segoe UI', sans-serif;
        background: linear-gradient(to right, #74ebd5, #9face6);
        color: #333;
        margin: 0;
        padding: 0;
      }
      .container {
        max-width: 720px;
        margin: 50px auto;
        background: #ffffffee;
        padding: 40px;
        border-radius: 16px;
        box-shadow: 0 8px 24px rgba(0,0,0,0.2);
      }
      .logo {
        display: block;
        margin: 0 auto 25px;
        max-width: 140px;
        border: 2px solid #3E4095;
        border-radius: 10px;
        transition: transform 0.4s ease, box-shadow 0.4s ease;
        box-shadow: 0 6px 20px rgba(62, 64, 149, 0.5);
        animation: glow 2.5s infinite;
      }
      .logo:hover {
        transform: scale(1.05) rotate(-1deg);
        box-shadow: 0 10px 30px rgba(237, 50, 55, 0.6);
      }
      @keyframes glow {
        0% { box-shadow: 0 0 5px #ED3237; }
        50% { box-shadow: 0 0 20px #ED3237; }
        100% { box-shadow: 0 0 5px #ED3237; }
      }
      h2 {
        text-align: center;
        color: #3E4095;
        font-size: 28px;
        margin-bottom: 15px;
      }
      .ip-info {
        text-align: center;
        font-size: 18px;
        margin-bottom: 20px;
        color: #222;
      }
      .nav-links {
        text-align: center;
        margin-bottom: 30px;
      }
      .nav-links a {
        color: #ffffff;
        background: #3E4095;
        font-weight: bold;
        padding: 10px 20px;
        text-decoration: none;
        border-radius: 8px;
        font-size: 18px;
        transition: background 0.3s;
      }
      .nav-links a:hover {
        background: #ED3237;
      }
      .data-block {
        margin-bottom: 30px;
        padding: 20px;
        background: #f0f8ff;
        border-left: 8px solid #3E4095;
        border-radius: 12px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.1);
      }
      .data-block h3 {
        margin-bottom: 12px;
        color: #ED3237;
        font-size: 22px;
      }
      .data-item {
        font-size: 20px;
        margin: 6px 0;
      }
    </style>
  </head>
  <body>
    <div class='container'>
      <img src='https://upload.wikimedia.org/wikipedia/commons/b/b9/Logo_Tr%C6%B0%E1%BB%9Dng_%C4%90%E1%BA%A1i_H%E1%BB%8Dc_S%C6%B0_Ph%E1%BA%A1m_K%E1%BB%B9_Thu%E1%BA%ADt_TP_H%E1%BB%93_Ch%C3%AD_Minh.png' alt='HCMUTE Logo' class='logo'>
      <h2>ĐỒ ÁN TỐT NGHIỆP</h2>
      <div class='ip-info'>
        <strong>📡 IP Address:</strong> )rawliteral"
                + ipAddress + R"rawliteral(
      </div>
      <div class="nav-links">
        <a href="/map">Xem Bản Đồ 📍</a>
        <a href="/charts" style="margin-left: 10px;">Xem Biểu Đồ 📊</a>
      </div>

      <div class='data-block'>
        <h3>📍 GPS Data</h3>
        <div class='data-item'><strong>Vĩ độ:</strong> <span id='lat'>Loading...</span></div>
        <div class='data-item'><strong>Kinh độ:</strong> <span id='lng'>Loading...</span></div>
        <div class='data-item'><strong>Thời gian (UTC):</strong> <span id='time'>Loading...</span></div>
      </div>

      <div class='data-block'>
        <h3>🍺 Cảm biến nồng độ cồn</h3>
        <div class='data-item'><strong>Nồng độ:</strong> <span id='alcohol'>Loading...</span></div>
      </div>

      <div class='data-block'>
        <h3>❤️ Nhịp tim</h3>
        <div class='data-item'><strong>BPM:</strong> <span id='bpm'>Loading...</span></div>
      </div>
    </div>

    <script>
      function fetchData() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            document.getElementById('lat').textContent = data.latitude.toFixed(6);
            document.getElementById('lng').textContent = data.longitude.toFixed(6);
            document.getElementById('time').textContent = data.time;
            document.getElementById('alcohol').textContent = data.alcohol.toFixed(2) + ' PPM';
            document.getElementById('bpm').textContent = data.heart_rate;
          })
          .catch(console.error);
      }
      setInterval(fetchData, 2000);
      fetchData();
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleCharts() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset='utf-8'>
    <title>Biểu đồ cảm biến</title>
    <style>
      body {
        font-family: 'Segoe UI', sans-serif;
        background: linear-gradient(135deg, #74ebd5, #ACB6E5);
        margin: 0;
        padding: 20px;
        text-align: center;
        animation: fadeIn 1s ease-in;
      }
      @keyframes fadeIn {
        from { opacity: 0; }
        to   { opacity: 1; }
      }
      h2 {
        color: #3E4095;
        margin-bottom: 0.5rem;
      }
      #clock {
        font-size: 18px;
        margin-bottom: 20px;
        color: #333;
      }
      .chart-container {
        width: 90%;
        max-width: 800px;
        margin: 30px auto;
        background: #fff;
        padding: 20px;
        border-radius: 16px;
        box-shadow: 0 6px 16px rgba(0,0,0,0.15);
      }
      .warning {
        font-size: 18px;
        font-weight: bold;
        color: red;
        margin-top: 10px;
        animation: blink 1s infinite;
      }
      @keyframes blink {
        0% { opacity: 1; }
        50% { opacity: 0.3; }
        100% { opacity: 1; }
      }
      .back-link {
        display: inline-block;
        margin-top: 20px;
        background: #3E4095;
        color: white;
        padding: 10px 20px;
        border-radius: 10px;
        text-decoration: none;
        font-size: 16px;
        box-shadow: 0 2px 6px rgba(0,0,0,0.2);
      }
      .back-link:hover {
        background: #ED3237;
      }
      @media screen and (max-width: 600px) {
        .chart-container {
          padding: 15px;
          margin: 20px auto;
        }
        .back-link {
          font-size: 14px;
          padding: 8px 16px;
        }
      }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  </head>
  <body>
    <h2>📊 SENSOR CHART</h2>
    <div id="clock">🕒 --:--:--</div>

    <div class="chart-container">
      <canvas id="alcoholChart" height="150"></canvas>
      <div id="alcoholWarning" class="warning"></div>
    </div>

    <div class="chart-container">
      <canvas id="bpmChart" height="150"></canvas>
      <div id="bpmWarning" class="warning"></div>
    </div>

    <a class="back-link" href="/">← Quay lại Dashboard</a>

    <script>
      function updateClock() {
        const now = new Date();
        document.getElementById('clock').textContent = '🕒 ' + now.toLocaleTimeString('vi-VN', { hour12: false });
      }
      setInterval(updateClock, 1000);
      updateClock();

      const alcoholCtx = document.getElementById('alcoholChart').getContext('2d');
      const alcoholGradient = alcoholCtx.createLinearGradient(0, 0, 0, 150);
      alcoholGradient.addColorStop(0, 'rgba(237, 50, 55, 0.4)');
      alcoholGradient.addColorStop(1, 'rgba(237, 50, 55, 0)');

      const bpmCtx = document.getElementById('bpmChart').getContext('2d');
      const bpmGradient = bpmCtx.createLinearGradient(0, 0, 0, 150);
      bpmGradient.addColorStop(0, 'rgba(62, 64, 149, 0.4)');
      bpmGradient.addColorStop(1, 'rgba(62, 64, 149, 0)');

      const alcoholData = {
        labels: [],
        datasets: [{
          label: 'Nồng độ cồn (PPM)',
          data: [],
          borderColor: '#ED3237',
          backgroundColor: alcoholGradient,
          tension: 0.4,
          fill: true,
          pointRadius: 0
        }]
      };

      const bpmData = {
        labels: [],
        datasets: [{
          label: 'Nhịp tim (BPM)',
          data: [],
          borderColor: '#3E4095',
          backgroundColor: bpmGradient,
          tension: 0.4,
          fill: true,
          pointRadius: 0
        }]
      };

      const alcoholChart = new Chart(alcoholCtx, {
        type: 'line',
        data: alcoholData,
        options: {
          responsive: true,
          animation: { duration: 500 },
          scales: {
            x: {
              ticks: {
                maxRotation: 75,
                minRotation: 45,
                autoSkip: true,
                maxTicksLimit: 10,
                color: '#444',
                font: { size: 12 }
              },
              grid: { display: false }
            },
            y: {
              beginAtZero: true,
              ticks: {
                color: '#333',
                font: { size: 13 }
              }
            }
          }
        }
      });

      const bpmChart = new Chart(bpmCtx, {
        type: 'line',
        data: bpmData,
        options: {
          responsive: true,
          animation: { duration: 500 },
          scales: {
            x: {
              ticks: {
                maxRotation: 75,
                minRotation: 45,
                autoSkip: true,
                maxTicksLimit: 10,
                color: '#444',
                font: { size: 12 }
              },
              grid: { display: false }
            },
            y: {
              beginAtZero: true,
              ticks: {
                color: '#333',
                font: { size: 13 }
              }
            }
          }
        }
      });

      function fetchData() {
        fetch('/data')
          .then(res => res.json())
          .then(data => {
            const timestamp = new Date().toLocaleTimeString('vi-VN', { hour12: false });

            // Alcohol
            alcoholData.labels.push(timestamp);
            alcoholData.datasets[0].data.push(data.alcohol);
            if (alcoholData.labels.length > 150) {
              alcoholData.labels.shift();
              alcoholData.datasets[0].data.shift();
            }
            alcoholChart.update();

            const alcoholWarning = document.getElementById('alcoholWarning');
            if (data.alcohol > 0.25) {
              alcoholWarning.textContent = '🔴 RẤT NGUY HIỂM: Nồng độ cồn quá cao! DỪNG XE NGAY!';
            } else if (data.alcohol > 0.13) {
              alcoholWarning.textContent = '🚨 VI PHẠM: Nồng độ cồn vượt ngưỡng cho phép!';
            } else if (data.alcohol > 0.05) {
              alcoholWarning.textContent = '⚠️ Có dấu hiệu sử dụng rượu nhẹ.';
            } else {
              alcoholWarning.textContent = '';
            }

            // BPM
            bpmData.labels.push(timestamp);
            bpmData.datasets[0].data.push(data.heart_rate);
            if (bpmData.labels.length > 150) {
              bpmData.labels.shift();
              bpmData.datasets[0].data.shift();
            }
            bpmChart.update();

            const bpmWarning = document.getElementById('bpmWarning');
            bpmWarning.textContent =
              (data.heart_rate > 120 || data.heart_rate < 50)
                ? '⚠️ CẢNH BÁO: Nhịp tim bất thường!'
                : '';
          })
          .catch(err => {
            console.error(err);
            document.getElementById('alcoholWarning').textContent = '⚠️ Không thể kết nối cảm biến!';
            document.getElementById('bpmWarning').textContent = '⚠️ Không thể kết nối cảm biến!';
          });
      }

      setInterval(fetchData, 10000); // 2 giây mỗi lần = 150 điểm trong 5 phút
      fetchData();
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}


void handleData() {
  // Trả về dữ liệu JSON cho client
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  String json = "{";
  json += "\"heart_rate\":" + String(heartValue);
  json += ",\"latitude\":" + String(latitude, 6);
  json += ",\"longitude\":" + String(longitude, 6);
  json += ",\"alcohol\":" + String(alcoholPPM, 2);
  json += ",\"time\":\"" + gpsTime + "\"";
  json += "}";
  xSemaphoreGive(dataMutex);

  server.send(200, "application/json", json);
}

// ==================== Task ============================
void TaskReadGPS(void *pvParameters) {
  while (true) {
    while (gpsSerial.available()) {
      char c = gpsSerial.read();
      gps.encode(c);
      if (gps.location.isUpdated()) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        xSemaphoreGive(dataMutex);
      }
      if (gps.time.isUpdated()) {
        int hour = gps.time.hour();
        int minute = gps.time.minute();
        int second = gps.time.second();

        // Chuyển sang GMT+7
        hour += 7;
        if (hour >= 24) hour -= 24;  // Xử lý nếu vượt quá 23h

        char buf[20];
        sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        gpsTime = String(buf);
        xSemaphoreGive(dataMutex);
      }
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void TaskReadAlcohol(void *pvParameters) {
  while (true) {
    float val = Alcohol.readAlcoholData(COLLECT_NUMBER);
    if (val != ERROR) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      alcoholPPM = val;
      xSemaphoreGive(dataMutex);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void TaskHeartRate(void *pvParameters) {
  int analogValue;
  unsigned long now, lastBeat = 0;
  bool risingEdge = false;
  int bpmBuffer[SAMPLE_COUNT] = { 0 };
  int bpmIndex = 0;

  while (true) {
    analogValue = analogRead(heartPin);
    now = millis();
    if (analogValue > THRESHOLD && !risingEdge) {
      risingEdge = true;
      unsigned long interval = now - lastBeat;
      lastBeat = now;
      if (interval > 300 && interval < 2000) {
        int bpm = 60000 / interval;
        bpmBuffer[bpmIndex] = bpm;
        bpmIndex = (bpmIndex + 1) % SAMPLE_COUNT;
        int sum = 0, count = 0;
        for (int i = 0; i < SAMPLE_COUNT; i++) {
          if (bpmBuffer[i] > 0) {
            sum += bpmBuffer[i];
            count++;
          }
        }
        if (count > 0) {
          xSemaphoreTake(dataMutex, portMAX_DELAY);
          heartValue = sum / count;
          xSemaphoreGive(dataMutex);
        }
      }
    }
    if (analogValue < THRESHOLD) {
      risingEdge = false;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void TaskSendHTTP(void *pvParameters) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      String jsonData = "{\"heart_rate\":" + String(heartValue) + ",\"gps_lat\":" + String(latitude, 6) + ",\"gps_lng\":" + String(longitude, 6) + ",\"alcohol\":" + String(alcoholPPM, 2) + "}";
      xSemaphoreGive(dataMutex);

      Serial.println("Sending JSON: " + jsonData);
      int httpResponseCode = http.POST(jsonData);
      Serial.print("HTTP Response: ");
      Serial.println(httpResponseCode);
      http.end();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskWebServer(void *pvParameters) {
  while (true) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// =============== Setup ==============================
void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  // IPAddress local_IP(192, 168, 137, 20);     // IP tĩnh bạn muốn dùng
  // IPAddress gateway(172,24,219,172);        // Thường là IP router
  // IPAddress subnet(255, 255, 255, 0);         // Subnet mask
  // IPAddress primaryDNS(8, 8, 8, 8);           // DNS chính (Google DNS)
  // IPAddress secondaryDNS(8, 8, 4, 4);         // DNS phụ
  //  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  //   Serial.println("⚠️  Cấu hình IP tĩnh thất bại");
  // }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  ipAddress = WiFi.localIP().toString();
  Serial.println("ESP32 IP address: " + ipAddress);

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Wire.begin(21, 22);

  if (!Alcohol.begin()) {
    Serial.println("Lỗi kết nối cảm biến alcohol!");
  } else {
    Alcohol.setModes(MEASURE_MODE_AUTOMATIC);
    Serial.println("Alcohol sensor initialized.");
  }

  // Đăng ký các route web
  server.on("/", handleDashboard);
  server.on("/dashboard", handleDashboard);
  server.on("/map", handleMap);
  server.on("/charts", handleCharts);
  server.on("/data", handleData);
  server.on("/map_data", []() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  String json = "{\"latitude\":" + String(latitude, 6) + ",\"longitude\":" + String(longitude, 6) + "}";
  xSemaphoreGive(dataMutex);
  server.send(200, "application/json", json);
});
  server.begin();
  // Tạo mutex bảo vệ dữ liệu chung
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("Failed to create data mutex!");
    while (1) { delay(1000); }  // dừng chương trình nếu tạo mutex thất bại
  }

  // Tạo các task xử lý riêng biệt trên các core ESP32
  xTaskCreatePinnedToCore(TaskReadGPS, "GPS", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskReadAlcohol, "Alcohol", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskHeartRate, "Heart", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskSendHTTP, "HTTP", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskWebServer, "WebServer", 4096, NULL, 1, NULL, 0);

  Serial.println("Setup completed.");
}

void loop() {
  // Không dùng loop, xử lý qua các task
}
