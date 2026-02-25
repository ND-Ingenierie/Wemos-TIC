#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ===== WIFI AP =====
const char* ssid = "LINKY-TIC";
const char* password = "12345678";

ESP8266WebServer server(80);

// ===== BME280 =====
Adafruit_BME280 bme;
bool bmeOk = false;
float envTemp = 0, envHum = 0, envPres = 0;

String bufferTIC = "";

// ===== PAGE WEB =====
void handleRoot() {
  String page =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='1'>"
    "<style>body{background:black;color:#00ff00;font-family:monospace;}</style>"
    "</head><body><pre>" +
    bufferTIC +
    "</pre></body></html>";

  server.send(200, "text/html", page);
}

void setup() {
  // ===== TIC Linky =====
  Serial.begin(1200, SERIAL_7E1);   // RX GPIO3

  // ===== BME280 =====
  Wire.begin();
  bmeOk = bme.begin(0x76);
  if (!bmeOk) bmeOk = bme.begin(0x77);

  // ===== WIFI AP =====
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.begin();

  bufferTIC = "Demarrage...\n";
}

void loop() {
  server.handleClient();

  // Lecture BME280 toutes les 10s
  static unsigned long lastBME = 0;
  if (bmeOk && millis() - lastBME > 10000) {
    envTemp = bme.readTemperature();
    envHum  = bme.readHumidity();
    envPres = bme.readPressure() / 100.0;
    lastBME = millis();
  }

  // Lecture TIC
  while (Serial.available()) {
    char c = Serial.read();
    bufferTIC += c;
    if (bufferTIC.length() > 4000) {
      bufferTIC.remove(0, 2000);
    }
  }
}
