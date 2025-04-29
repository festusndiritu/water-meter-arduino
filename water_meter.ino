#include <WiFi.h>
#include <WiFiClientSecure.h>
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
const char* serverUrl = "https://web-production-83fd.up.railway.app";

// Let's Encrypt Root CA Certificate - ISRG Root X1
// This is the root certificate for Let's Encrypt
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

// NTP settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 3600000); // UTC+3, updates every hour

// Flow sensor variables
float calibrationFactor = 500.0; // Pulses per liter
volatile unsigned long pulseCount = 0;
float flowRateLitresPerMinute = 0.0;
float totalLitres = 0.0;
bool valveOpen = true; // Valve open by default
const float flowThreshold = 20.0; // L/min
bool ntpInitialized = false;

unsigned long lastPulseTime = 0;
unsigned long oldTime = 0;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // Send data every 1 second for real-time
unsigned long lastNTPUpdate = 0;

void IRAM_ATTR pulseCounter() {
  unsigned long currentTime = micros();
  if (currentTime - lastPulseTime > 1000) { // Debounce: ignore pulses <1ms apart
    pulseCount++;
    lastPulseTime = currentTime;
  }
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

  // Connect to WiFi
  connectWiFi();

  // Initialize NTP
  timeClient.begin();
  for (int i = 0; i < 3 && !ntpInitialized; i++) {
    if (timeClient.update()) {
      ntpInitialized = true;
      lastNTPUpdate = millis();
      Serial.println("NTP initialized");
    } else {
      Serial.println("NTP attempt " + String(i + 1) + " failed");
      delay(1000);
    }
  }

  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  Serial.println("Setup complete");
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 10000; // 10s timeout

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);
    Serial.print(".");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting WiFi...");
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed");
    display.display();
  }
}

void loop() {
  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
  }

  // Update NTP every hour
  if (ntpInitialized && millis() - lastNTPUpdate > 3600000) {
    if (timeClient.update()) {
      lastNTPUpdate = millis();
      Serial.println("NTP updated");
    } else {
      ntpInitialized = false;
      Serial.println("NTP update failed");
    }
  }

  // Update flow calculations and display every 1s
  if (millis() - oldTime >= 1000) {
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));

    unsigned long currentTime = millis();
    float intervalSeconds = (currentTime - oldTime) / 1000.0;
    oldTime = currentTime;

    // Calculate flow rate and volume
    float litresInInterval = pulseCount / calibrationFactor;
    totalLitres += litresInInterval;
    flowRateLitresPerMinute = (litresInInterval / intervalSeconds) * 60.0;

    // Update OLED
    String currentTimeStr = getTimestamp();
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Water Meter");
    display.print("Flow: ");
    display.print(flowRateLitresPerMinute, 1);
    display.println(" L/min");
    display.print("Total: ");
    if (totalLitres < 1000) {
      display.print(totalLitres, 1);
      display.println(" L");
    } else {
      display.print(totalLitres / 1000.0, 2);
      display.println(" kL");
    }
    display.print("Valve: ");
    display.println(valveOpen ? "Open" : "Closed");
    display.print("Time: ");
    display.println(currentTimeStr.substring(11, 19));
    if (flowRateLitresPerMinute > flowThreshold) {
      display.println("High Flow Alert");
    }
    display.println(WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Disconnected");
    display.display();

    // Serial output
    Serial.printf("Flow: %.1f L/min, Total: %.1f L, Valve: %s, Time: %s\n",
                  flowRateLitresPerMinute, totalLitres, valveOpen ? "Open" : "Closed", currentTimeStr.c_str());

    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  }

  // Send data and check valve every 1s
  if (millis() - lastSendTime >= sendInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      checkValveCommand();
      sendDataToServer(flowRateLitresPerMinute, totalLitres, totalLitres / 1000.0, valveOpen);
    }
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
    unsigned long uptime = millis() / 1000;
    char buf[20];
    snprintf(buf, sizeof(buf), "1970-01-01 %02lu:%02lu:%02lu", uptime / 3600, (uptime % 3600) / 60, uptime % 60);
    return String(buf);
  }
}

void sendDataToServer(float flowRate, float totalVolume, float units, bool valveState) {
  WiFiClientSecure client;
  HTTPClient http;

  // Set certificate
  client.setCACert(rootCACertificate);
  
  // Add debug logs for connection
  Serial.println("Connecting to: " + String(serverUrl) + "/data");
  
  String url = String(serverUrl) + "/data";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Add more detailed debugging
  http.setReuse(false);
  
  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"flow_rate\":%.1f,\"total_volume\":%.1f,\"quality_units\":%.2f,\"valve_state\":%s,\"timestamp\":\"%s\"}",
           flowRate, totalVolume, units, valveState ? "true" : "false", getTimestamp().c_str());

  Serial.println("Sending payload: " + String(payload));
  
  // Try with timeout
  http.setTimeout(10000); // 10 second timeout
  int httpCode = http.POST(payload);
  
  Serial.printf("Data send HTTP code: %d\n", httpCode);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Server responded with code: " + String(httpCode));
      Serial.println("Response: " + http.getString());
    }
  } else {
    Serial.println("HTTP error: " + http.errorToString(httpCode));
  }

  http.end();
}

void checkValveCommand() {
  WiFiClientSecure client;
  HTTPClient http;

  // Set certificate
  client.setCACert(rootCACertificate);
  
  // Add debug logs
  Serial.println("Checking valve at: " + String(serverUrl) + "/valve");
  
  String url = String(serverUrl) + "/valve";
  http.begin(client, url);
  
  // Add timeout
  http.setTimeout(10000); // 10 second timeout
  int httpCode = http.GET();
  
  Serial.printf("Valve check HTTP code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    response.trim();
    response.replace("\"", "");
    Serial.println("Server valve response: " + response);

    if (response == "open" && !valveOpen) {
      digitalWrite(relayPin, LOW);
      valveOpen = true;
      Serial.println("Valve opened by server");
    } else if (response == "close" && valveOpen) {
      digitalWrite(relayPin, HIGH);
      valveOpen = false;
      Serial.println("Valve closed by server");
    }
  } else {
    Serial.println("Valve check failed with code: " + String(httpCode));
    Serial.println("Error: " + http.errorToString(httpCode));
  }

  http.end();
}