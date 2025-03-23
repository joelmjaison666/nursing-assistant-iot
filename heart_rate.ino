#include <Wire.h>
#include "MAX30105.h"
#include <WiFi.h>
#include <WebSocketsClient.h>

// ✅ Function declarations
void sendBPMtoServer(float bpm);
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);

// ✅ MAX30102 sensor instance
MAX30105 particleSensor;

// ✅ WiFi credentials
const char* ssid = "Umb";             // 🌟 NEW SSID 🌟
const char* password = "123456789";   // 🌟 NEW PASSWORD 🌟

// ✅ WebSocket client instance
WebSocketsClient webSocket;

// ✅ Local WebSocket server IP and port
const char* websocket_host = "172.20.10.4";  // 🌟 NEW IP ADDRESS 🌟
const uint16_t websocket_port = 9000;        // 🌟 PORT (unchanged) 🌟

// ✅ Heart rate measurement variables
#define HISTORY_SIZE 10
float bpmHistory[HISTORY_SIZE];

bool recalibrating = false;

#define BUFFER_SIZE 60
float bpmBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool bufferFull = false;

long lastBeat = 0;
float finalBPM = 0;

const int MIN_BPM = 60;
const int MAX_BPM = 120;

unsigned long startTime = 0;
const unsigned long MEASUREMENT_DURATION = 30000;  // 30 seconds
bool measurementComplete = false;

void setup() {
  Serial.begin(115200);
  Serial.println("MAX30102 Heart Rate Monitor - WebSocket Client");

  // ✅ Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // ✅ Initialize I2C (Explicit for ESP32)
  Wire.begin(21, 22); // SDA = 21, SCL = 22

  // ✅ Initialize MAX30102 sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ MAX30102 not found. Check wiring.");
    while (1);  // Stop if sensor isn't found
  }

  particleSensor.setup(60, 1, 2, 400, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x0A);

  Serial.println("✅ Place your finger on the sensor and keep it still.");
  startTime = millis();

  // ✅ Initialize WebSocket connection to server with IP & Port
  webSocket.begin(websocket_host, websocket_port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);  // Try reconnecting every 5 seconds
}

void loop() {
  webSocket.loop();  // ✅ Keep WebSocket connection alive

  // ✅ Optional: Send ping to keep connection stable
  webSocket.sendPing();

  uint32_t irValue = particleSensor.getIR();

  if (recalibrating) {
    delay(50);
    return;
  }

  if (irValue < 5000) {
    Serial.println("No finger detected...");
    delay(500);
    return;
  }

  if (!measurementComplete) {
    if (checkForBeat(irValue)) {
      long delta = millis() - lastBeat;
      lastBeat = millis();

      float tempBPM = 60 / (delta / 1000.0);
      if (tempBPM >= MIN_BPM && tempBPM <= MAX_BPM) {
        bpmBuffer[bufferIndex] = tempBPM;
        bufferIndex++;

        if (bufferIndex >= BUFFER_SIZE) {
          bufferIndex = 0;
          bufferFull = true;
        }
      }
    }

    if (millis() - startTime >= MEASUREMENT_DURATION) {
      calculateFinalBPM();
      measurementComplete = true;

      // ✅ Send data to WebSocket server
      sendBPMtoServer(finalBPM);

      recalibrating = true;
      Serial.println("Recalibrating... Waiting 3 seconds");
      delay(3000);
      recalibrating = false;

      resetMeasurement();
    }
  }

  delay(50);
}

void calculateFinalBPM() {
  int validReadings = 0;
  float totalBPM = 0;

  int readingsToProcess = bufferFull ? BUFFER_SIZE : bufferIndex;

  for (int i = 0; i < readingsToProcess; i++) {
    if (bpmBuffer[i] >= MIN_BPM && bpmBuffer[i] <= MAX_BPM) {
      totalBPM += bpmBuffer[i];
      validReadings++;
    }
  }

  if (validReadings > 0) {
    finalBPM = totalBPM / validReadings;
    Serial.print("✅ Final Average BPM: ");
    Serial.println(finalBPM, 1);

    // ✅ Add BPM to history
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
      bpmHistory[i] = bpmHistory[i + 1];
    }
    bpmHistory[HISTORY_SIZE - 1] = finalBPM;
  } else {
    Serial.println("❌ No valid readings. Try again.");
  }
}

void resetMeasurement() {
  bufferIndex = 0;
  bufferFull = false;
  measurementComplete = false;
  startTime = millis();

  for (int i = 0; i < BUFFER_SIZE; i++) {
    bpmBuffer[i] = 0;
  }

  Serial.println("🔄 Restarting measurement...");
}

bool checkForBeat(uint32_t irValue) {
  static uint32_t threshold = 0;
  static uint32_t previousValue = 0;
  static bool isPeak = false;
  static uint32_t peakAge = 0;

  peakAge++;

  if (irValue > previousValue) {
    threshold = (threshold * 3 + irValue) / 4;
  }

  previousValue = irValue;

  if (irValue > threshold && !isPeak && peakAge > 10) {
    isPeak = true;
    peakAge = 0;
    return true;
  } else if (irValue < threshold) {
    isPeak = false;
  }

  return false;
}

void sendBPMtoServer(float bpm) {
  String message = "{\"sensor\": \"heart_rate\", \"bpm\": " + String(bpm, 1) + "}";
  Serial.print("📡 Sending BPM: ");
  Serial.println(message);
  webSocket.sendTXT(message);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("❌ [WebSocket] Disconnected from server.");
      break;
    case WStype_CONNECTED:
      Serial.println("✅ [WebSocket] Connected to WebSocket server.");
      break;
    case WStype_TEXT:
      Serial.printf("📨 [WebSocket] Received message: %s\n", payload);
      break;
  }
}
