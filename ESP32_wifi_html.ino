#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ModbusMaster.h>
#include "website.h"

// --- WiFi Credentials ---
const char* ssid = "Marshalz";
const char* password = "abcdefgh";

// --- Hardware Definitions ---
#define RXD2 16
#define TXD2 17
#define MAX485_DE 5
#define MAX485_RE_NEG 18

// --- Objects ---
ModbusMaster node1; // Soil Sensor 1 (Humid, Temp, pH)
ModbusMaster node2; // Soil Sensor 2 (NPK)
AsyncWebServer server(80);

// --- Global Sensor Variables ---
float soilHumid = 0, soilTemp = 0,  soilEC = 0, soilPH = 0, soilNitro = 0, soilPhospho = 0, soilPotass = 0;

// --- Helper Functions for RS485 ---
void preTransmission() { digitalWrite(MAX485_RE_NEG, 1); digitalWrite(MAX485_DE, 1); }
void postTransmission() { digitalWrite(MAX485_RE_NEG, 0); digitalWrite(MAX485_DE, 0); }

// --- The Processor ---
// Swaps %PLACEHOLDERS% in HTML with real values
String processor(const String& var) {
  if (var == "HUMID") return String(soilHumid, 1);
  if (var == "TEMP")  return String(soilTemp, 1);
  if (var == "PH")    return String(soilPH, 1);
  // if (var == "EC")    return String(soilEC, 1);
  if (var == "N")    return String(soilNitro, 0);
  if (var == "P")    return String(soilPhospho, 0);
  if (var == "K")    return String(soilPotass, 0);
  return String();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);

  // Connect WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected. IP:");
  Serial.println(WiFi.localIP());

  // Initialize Modbus Nodes
  node1.begin(1, Serial2);
  node1.preTransmission(preTransmission);
  node1.postTransmission(postTransmission);

  node2.begin(2, Serial2);
  node2.preTransmission(preTransmission);
  node2.postTransmission(postTransmission);

  // --- Web Route ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.begin();
}

void loop() {
  static unsigned long lastCheck = 0;
  
  // Read sensors every 5 seconds
  if (millis() - lastCheck > 5000) {
    
    // Read Node 1: Humid, Temp, EC, pH
    uint8_t result1 = node1.readHoldingRegisters(0x0000, 4);
    if (result1 == node1.ku8MBSuccess) {
      soilHumid = node1.getResponseBuffer(0) / 10.0;
      soilTemp  = node1.getResponseBuffer(1) / 10.0;
      // soilEC    = node1.getResponseBuffer(2) / 10.0;
      soilPH    = node1.getResponseBuffer(3) / 10.0;
    }

    delay(100); // Small pause between nodes

    // Read Node 2: N, P, K
    uint8_t result2 = node2.readHoldingRegisters(0x001E, 3);
    if (result2 == node2.ku8MBSuccess) {
      soilNitro = node2.getResponseBuffer(0);
      soilPhospho = node2.getResponseBuffer(1);
      soilPotass = node2.getResponseBuffer(2);
    }

    lastCheck = millis();
  }
}