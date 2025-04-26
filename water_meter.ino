#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define relayPin 5
#define flowSensorPin 34

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi credentials
const char* ssid = "WiFi 2";
const char* password = "Anaklusmos";

// FastAPI server URL
const char* serverUrl = "http://192.168.1.3:8000";

// NTP settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 3600000); // UTC+3, update every hour

// Flow sensor variables
float calibrationFactor = 500; // Pulses per liter, verify with YF-S201 datasheet
volatile int pulseCount = 0;
float flowRateLitresPerMinute = 0.0;
float totalLitres = 0.0; // Changed to float for precision
bool valveOpen = true; // Valve open by default
const float flowThreshold = 20; // L/min
bool ntpInitialized = false;

unsigned long oldTime = 0;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; // Send data every 5 seconds
unsigned long lastNTPUpdate = 0;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Booted - Starting Setup");

  pinMode(relayPin, OUTPUT);
  pinMode(flowSensorPin, INPUT_PULLUP);
  digitalWrite(relayPin, LOW); // Valve open (active-low)
  Serial.println("Relay initialized: Valve open");

  // Initialize OLED
  Wire.begin(21, 22); // SDA (D21), SCL (D22)
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Water Meter");
  display.display();

  // Connect to Wi-Fi
  connectWiFi();

  // Initialize NTP
  timeClient.begin();
  if (timeClient.update()) {
    ntpInitialized = true;
    lastNTPUpdate = millis();
    Serial.println("NTP initialized");
  } else {
    Serial.println("NTP initialization failed");
  }

  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  Serial.println("Setup complete");
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting WiFi...");
    display.display();
  }
  Serial.println("WiFi connected, IP: " + WiFi.localIP().toString());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
    if (ntpInitialized && !timeClient.update()) {
      ntpInitialized = false;
      Serial.println("NTP re-initialization failed");
    }
  }

  // Update NTP every hour
  if (ntpInitialized && (millis() - lastNTPUpdate > 3600000)) {
    if (timeClient.update()) {
      lastNTPUpdate = millis();
      Serial.println("NTP updated");
    } else {
      ntpInitialized = false;
      Serial.println("NTP update failed");
    }
  }

  // Update display and calculations every second
  if (millis() - oldTime > 1000) {
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));

    unsigned long currentTime = millis();
    float intervalSeconds = (currentTime - oldTime) / 1000.0;
    oldTime = currentTime;

    // Calculate volume and flow rate
    float litresInInterval = (float)pulseCount / calibrationFactor;
    totalLitres += litresInInterval;
    flowRateLitresPerMinute = (litresInInterval / intervalSeconds) * 60.0;

    // Update OLED
    String currentTimeStr = getTimestamp();
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Water Meter");
    display.print("Flow: ");
    display.print(flowRateLitresPerMinute);
    display.println(" L/min");
    display.print("Total: ");
    if (totalLitres < 1000) {
      display.print(totalLitres);
      display.println(" L");
    } else {
      float units = totalLitres / 1000.0;
      display.print(units);
      display.println(" Units");
    }
    display.print("Valve: ");
    display.println(valveOpen ? "Open" : "Closed");
    display.print("Time: ");
    display.println(currentTimeStr.substring(11, 19));
    if (flowRateLitresPerMinute > flowThreshold) {
      display.println("High Flow Alert!");
    }
    display.println(WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    display.display();

    // Serial output for debugging
    Serial.print("Flow: ");
    Serial.print(flowRateLitresPerMinute);
    Serial.print(" L/min, Total: ");
    Serial.print(totalLitres);
    Serial.print(" L, Valve: ");
    Serial.print(valveOpen ? "Open" : "Closed");
    Serial.print(", Time: ");
    Serial.println(currentTimeStr);

    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  }

  // Server communication every 5 seconds
  if (millis() - lastSendTime > sendInterval) {
    checkValveCommand();
    sendDataToServer(flowRateLitresPerMinute, totalLitres, totalLitres / 1000.0, valveOpen);
    lastSendTime = millis();
  }
}

String getTimestamp() {
  if (ntpInitialized) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&epochTime);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeInfo);
    return String(buf);
  } else {
    unsigned long epochTime = millis() / 1000;
    char buf[20];
    snprintf(buf, sizeof(buf), "1970-01-01 %02d:%02d:%02d", epochTime / 3600, (epochTime % 3600) / 60, epochTime % 60);
    return String(buf);
  }
}

void sendDataToServer(float flowRate, float totalVolume, float units, bool valveState) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverUrl) + "/data";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String timestamp = getTimestamp();
    String payload = "{\"flow_rate\":";
    payload += flowRate;
    payload += ",\"total_volume\":";
    payload += totalVolume;
    payload += ",\"quality_units\":";
    payload += units;
    payload += ",\"valve_state\":";
    payload += valveState ? "true" : "false";
    payload += ",\"timestamp\":\"";
    payload += timestamp;
    payload += "\"}";

    Serial.println("Sending payload: " + payload);
    int httpCode = http.POST(payload);
    Serial.println("Data send HTTP code: " + String(httpCode));
    if (httpCode <= 0) {
      Serial.println("Error: " + http.getString());
    }
    http.end();
  } else {
    Serial.println("WiFi not connected, skipping data send");
  }
}

void checkValveCommand() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverUrl) + "/valve";
    http.begin(url);
    int httpCode = http.GET();
    Serial.println("Valve check HTTP code: " + String(httpCode));
    if (httpCode == 200) {
      String response = http.getString();
      Serial.println("Server valve response: " + response);
      
      // Remove any quotes from the response
      response.replace("\"", "");
      
      if (response == "open" && !valveOpen) {
        digitalWrite(relayPin, LOW);
        valveOpen = true;
        Serial.println("Valve opened by server");
      } else if (response == "close" && valveOpen) {
        digitalWrite(relayPin, HIGH);
        valveOpen = false;
        Serial.println("Valve closed by server");
      } else {
        Serial.println("No valve state change needed");
      }
    } else {
      Serial.println("Valve check failed, HTTP code: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected, skipping valve check");
  }
}