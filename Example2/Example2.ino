#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== WIFI AP =====
const char* ssid = "LINKY-TIC";
const char* password = "12345678";

ESP8266WebServer server(80);

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

  // ===== WIFI AP =====
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.begin();

  bufferTIC = "Demarrage...\n";
}

void loop() {

  server.handleClient();

  // Lecture TIC
  while (Serial.available()) {

    char c = Serial.read();

    bufferTIC += c;

    // limite mémoire (important ESP8266)
    if (bufferTIC.length() > 4000) {
      bufferTIC.remove(0, 2000);
    }
  }
}