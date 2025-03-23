#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WebSocketsClient.h>

// ✅ WiFi credentials
const char* ssid = "BITS-WIFI";
const char* password = "bits@123";

// ✅ WebSocket client
WebSocketsClient webSocket;

// ✅ Updated WebSocket server IP and port
const char* websocket_host = "172.20.10.4";   // 🌟 NEW IP ADDRESS 🌟
const uint16_t websocket_port = 9000;          // WebSocket server port

Adafruit_MPU6050 mpu;

// Fall Detection Parameters
const float IMPACT_THRESHOLD = 3.0;
const float FALLING_THRESHOLD = 0.8;
const float REST_THRESHOLD = 1.2;
const int IMPACT_DURATION = 100;
const int REST_DURATION = 200;
const int FALL_COOLDOWN = 2000;

// State Variables
float currentX = 0, currentY = 0, currentZ = 0, totalAccel = 0;
float prevTotalAccel = 0;
bool impactDetected = false;
bool fallingDetected = false;
bool fallDetected = false;
unsigned long impactTime = 0;
unsigned long restTime = 0;
unsigned long lastFallTime = 0;

// Moving Average Filter
const int FILTER_SIZE = 5;
float accelHistory[FILTER_SIZE];
int filterIndex = 0;

void setup() {
  Serial.begin(115200);

  // ✅ Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  // ✅ Setup MPU6050
  if (!mpu.begin()) {
    Serial.println("❌ MPU6050 Failed");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // ✅ Initialize filter
  for (int i = 0; i < FILTER_SIZE; i++) {
    accelHistory[i] = 0;
  }

  // ✅ Initialize WebSocket Client
  webSocket.begin(websocket_host, websocket_port, "/");  // ✅ NEW IP USED HERE
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();  // ✅ Keep the WebSocket alive

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  currentX = a.acceleration.x;
  currentY = a.acceleration.y;
  currentZ = a.acceleration.z;

  totalAccel = sqrt(pow(currentX, 2) + pow(currentY, 2) + pow(currentZ, 2)) / 9.8;
  float filteredAccel = getFilteredAcceleration(totalAccel);

  detectFall(filteredAccel, currentZ);

  sendDataOverWebSocket();  // ✅ Send sensor data via WebSocket
  
  prevTotalAccel = totalAccel;
  delay(100);  // Adjust frequency of updates
}

void detectFall(float acceleration, float zAccel) {
  unsigned long currentTime = millis();

  if (currentTime - lastFallTime > FALL_COOLDOWN) {
    fallDetected = false;
  }

  if (!impactDetected && acceleration > IMPACT_THRESHOLD) {
    impactDetected = true;
    impactTime = currentTime;
    Serial.println("Impact detected!");
  }

  if (impactDetected && !fallingDetected &&
      currentTime - impactTime > IMPACT_DURATION) {
    if (zAccel < -FALLING_THRESHOLD) {
      fallingDetected = true;
      Serial.println("Falling motion detected!");
    } else {
      impactDetected = false;
    }
  }

  if (fallingDetected && !fallDetected) {
    if (abs(acceleration - 1.0) < REST_THRESHOLD) {
      if (restTime == 0) {
        restTime = currentTime;
      }

      if (currentTime - restTime > REST_DURATION) {
        fallDetected = true;
        lastFallTime = currentTime;
        Serial.println("FALL CONFIRMED!");
      }
    } else {
      restTime = 0;
    }
  }

  if (!fallDetected && currentTime - impactTime > 1000) {
    impactDetected = false;
    fallingDetected = false;
    restTime = 0;
  }
}

float getFilteredAcceleration(float newValue) {
  accelHistory[filterIndex] = newValue;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;

  float sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) {
    sum += accelHistory[i];
  }
  return sum / FILTER_SIZE;
}

void sendDataOverWebSocket() {
  String fallStatus = fallDetected ? "YES" : "NO";

  // ✅ Prepare JSON message
  String message = "{";
  message += "\"sensor\": \"fall_detector\",";
  message += "\"status\": \"" + fallStatus + "\","; 
  message += "\"accel\": {";
  message += "\"x\": " + String(currentX, 2) + ",";
  message += "\"y\": " + String(currentY, 2) + ",";
  message += "\"z\": " + String(currentZ, 2) + "},";
  message += "\"total_g\": " + String(totalAccel, 2);
  message += "}";

  Serial.println("📡 Sending Data: " + message);
  webSocket.sendTXT(message);
}

// ✅ WebSocket Event Callback (optional debug)
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("❌ [WebSocket] Disconnected.");
      break;
    case WStype_CONNECTED:
      Serial.println("✅ [WebSocket] Connected.");
      break;
    case WStype_TEXT:
      Serial.printf("📨 [WebSocket] Message: %s\n", payload);
      break;
  }
}
