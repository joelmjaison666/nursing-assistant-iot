#include <Wire.h> 
#include <Adafruit_MLX90614.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ✅ WiFi Credentials
const char* ssid = "Umb";
const char* password = "123456789";

// ✅ WebSocket Server Details
const char* websocket_host = "172.20.10.4";
const uint16_t websocket_port = 9000;
const char* websocket_path = "/";

WebSocketsClient webSocket;

// MLX90614 Sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Settings
const int SDA_PIN = 21;
const int SCL_PIN = 22;

const int numReadings = 10;       // ✅ Reduced for quicker sampling
const float objectOffset = 0.0;   // Offset adjustment if needed

// Timing control
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000;  // 5 seconds

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize I2C and MLX90614
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mlx.begin()) {
        Serial.println("❌ ERROR: Could not detect MLX90614 sensor. Halting.");
        while (true); // Halt if no sensor found
    }

    // ✅ Connect to WiFi
    connectToWiFi();

    // ✅ Setup WebSocket
    webSocket.begin(websocket_host, websocket_port, websocket_path);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000); // 5 seconds

    // ✅ Enable WebSocket heartbeat
    webSocket.enableHeartbeat(3000, 1000, 2);  // ping every 3s, expect pong in 1s, 2 failures allowed

    Serial.println("✅ Setup complete.");
}

void loop() {
    // ✅ Keep WebSocket alive
    webSocket.loop();

    // ✅ WiFi reconnect logic in loop
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("🔄 WiFi disconnected! Reconnecting...");
        connectToWiFi();
    }

    // ✅ Non-blocking send logic based on millis()
    unsigned long now = millis();
    if (now - lastSendTime >= sendInterval) {
        lastSendTime = now;

        if (webSocket.isConnected()) {
            float temperatureC = getProcessedTemperature();

            StaticJsonDocument<200> doc;
            doc["sensor"] = "temperature";
            doc["temperature_c"] = temperatureC;

            char jsonBuffer[256];
            serializeJson(doc, jsonBuffer);

            webSocket.sendTXT(jsonBuffer);

            Serial.println("📤 Sent temperature to WebSocket Server:");
            Serial.println(jsonBuffer);
        } else {
            Serial.println("🔴 WebSocket not connected...");
        }
    }
}

// ✅ WiFi Connection Function
void connectToWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    WiFi.begin(ssid, password);

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        attempt++;
        if (attempt >= 20) {  // 10 seconds timeout
            Serial.println("❌ WiFi connection failed! Restarting ESP32...");
            ESP.restart();  // Optional: restart ESP32 if WiFi fails repeatedly
        }
    }

    Serial.println("\n✅ Connected to WiFi!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
}

// ✅ WebSocket Event Handling with IDENTITY Message
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("❌ Disconnected from WebSocket Server");
            break;

        case WStype_CONNECTED:
            Serial.println("✅ Connected to WebSocket Server");
            // ✅ Send identification message when connected
            webSocket.sendTXT("{\"message\": \"Body temp sensor connected\"}");
            break;

        case WStype_TEXT:
            Serial.printf("📨 Received message from Server: %s\n", payload);
            break;

        case WStype_ERROR:
            Serial.println("❌ WebSocket Error");
            break;

        default:
            break;
    }
}

// ✅ Function to process and average the temperature readings
float getProcessedTemperature() {
    float temperatureReadings[numReadings];
    float sum = 0.0;
    int validCount = 0;

    for (int i = 0; i < numReadings; i++) {
        float temp = mlx.readObjectTempC();

        // ✅ NAN check for invalid readings
        if (isnan(temp)) {
            Serial.println("⚠️ Warning: Invalid temp reading!");
            continue; // Skip this reading
        }

        temperatureReadings[validCount] = temp;
        sum += temp;
        validCount++;

        webSocket.loop();  // ✅ Keep WebSocket alive during readings
        delay(25); // Optional: consider replacing with millis() if you want zero blocking
    }

    if (validCount == 0) {
        Serial.println("⚠️ No valid temperature readings available.");
        return -1; // Return an error value or handle as needed
    }

    float avg = sum / validCount + objectOffset;

    // ✅ Optional: Standard deviation calculation for outlier filtering
    float variance = 0.0;
    for (int i = 0; i < validCount; i++) {
        variance += pow(temperatureReadings[i] - avg, 2);
    }
    float stdDev = sqrt(variance / validCount);

    // ✅ Filter out outliers (±2σ)
    float filteredSum = 0.0;
    int filteredCount = 0;
    for (int i = 0; i < validCount; i++) {
        if (abs(temperatureReadings[i] - avg) <= 2 * stdDev) {
            filteredSum += temperatureReadings[i];
            filteredCount++;
        }
    }

    if (filteredCount > 0) {
        avg = filteredSum / filteredCount + objectOffset;
    }

    Serial.printf("Processed Body Temperature: %.2f°C\n", avg);
    return avg;
}
