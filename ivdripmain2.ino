#include <WiFi.h>
#include <WebSocketsClient.h>

// ✅ SENSOR PIN
#define SENSOR_PIN 35

// ✅ Calibration values for your MPX5010DP sensor
const float Vmin = 0.116;
const float sensitivity = 0.45;
const float referenceVoltage = 3.3;
const int ADC_RESOLUTION = 4095;

// ✅ WiFi credentials (Update as needed)
const char* ssid = "Umb";
const char* password = "123456789";

// ✅ Flask WebSocket Server details
const char* websocket_host = "172.20.10.4";  // Your Flask server IP
const uint16_t websocket_port = 9000;          // Your Flask WebSocket port

// ✅ WebSocket Client instance
WebSocketsClient webSocket;

// ✅ Time tracking
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // send data every 1000ms

void setup() {
  Serial.begin(115200);
  
  // ✅ Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());

  // ✅ Initialize WebSocket connection to Flask server
  webSocket.begin(websocket_host, websocket_port, "/");

  // ✅ Assign the event handler
  webSocket.onEvent(webSocketEvent);

  // ✅ Try to reconnect every 5 seconds if disconnected
  webSocket.setReconnectInterval(5000);

  Serial.println("✅ WebSocket initialized. Attempting to connect...");
}

// ✅ Function to get the average ADC value from the sensor
float getAverageADC(int samples = 10, int delayMs = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(SENSOR_PIN);
    delay(delayMs);
  }
  return (float)sum / samples;
}

void loop() {
  // ✅ Keep WebSocket running
  webSocket.loop();

  // ✅ Send sensor data every `sendInterval` ms
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;

    if (webSocket.isConnected()) {
      float rawADC = getAverageADC();
      float voltage = (rawADC / ADC_RESOLUTION) * referenceVoltage;
      float pressure_kPa = max((voltage - Vmin) / sensitivity, 0.0f);

      // ✅ JSON message to send
      String message = "{\"device\": \"ESP32\", \"sensor\": \"pressure\", \"pressure\": " + String(pressure_kPa, 3) + "}";

      Serial.println("--------------------------------------------------");
      Serial.printf("ADC: %.2f | Voltage: %.3f V | Pressure: %.3f kPa\n", rawADC, voltage, pressure_kPa);
      Serial.print("📤 Sending JSON: ");
      Serial.println(message);
      
      // ✅ Send JSON data to WebSocket Server
      webSocket.sendTXT(message);
    } else {
      Serial.println("⚠️ WebSocket not connected. Skipping data send.");
    }
  }
}

// ✅ WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("❌ WebSocket disconnected!");
      break;

    case WStype_CONNECTED:
      Serial.println("✅ WebSocket connected to Flask server!");
      // Optional: Send a greeting message
      webSocket.sendTXT("{\"message\": \"ESP32 connected\"}");
      break;

    case WStype_TEXT:
      Serial.print("📨 Message from server: ");
      Serial.println((char*)payload);
      break;

    case WStype_BIN:
      Serial.println("📦 Received binary data (unsupported)");
      break;
  }
}
