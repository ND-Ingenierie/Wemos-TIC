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

// ===== TIC PARSED DATA =====
// Mode historique : trames STX(0x02)...ETX(0x03)
// Chaque groupe : LF <label> SP <valeur> SP <checksum> CR
#define MAX_TIC_FIELDS 24
String ticKey[MAX_TIC_FIELDS];
String ticVal[MAX_TIC_FIELDS];
int    ticCount = 0;
bool   ticReady = false;

String frameBuffer = "";
bool   inFrame = false;

// Extrait clé/valeur d'une ligne TIC
void parseTICFrame(const String& frame) {
  int newCount = 0;
  String newKey[MAX_TIC_FIELDS];
  String newVal[MAX_TIC_FIELDS];

  int pos = 0;
  while (pos < (int)frame.length() && newCount < MAX_TIC_FIELDS) {
    int lf = frame.indexOf('\x0A', pos);
    if (lf < 0) break;
    int cr = frame.indexOf('\x0D', lf);
    if (cr < 0) break;

    String line = frame.substring(lf + 1, cr);

    if (line.length() > 3) {
      int sp1 = line.indexOf(' ');
      int sp2 = line.lastIndexOf(' ');
      if (sp1 > 0 && sp2 > sp1) {
        newKey[newCount] = line.substring(0, sp1);
        newVal[newCount] = line.substring(sp1 + 1, sp2);
        newCount++;
      }
    }
    pos = cr + 1;
  }

  if (newCount > 0) {
    for (int i = 0; i < newCount; i++) {
      ticKey[i] = newKey[i];
      ticVal[i] = newVal[i];
    }
    ticCount = newCount;
    ticReady = true;
  }
}

String getTIC(const String& key) {
  for (int i = 0; i < ticCount; i++) {
    if (ticKey[i] == key) return ticVal[i];
  }
  return "";
}

// ===== API JSON =====
void handleAPI() {
  String json = "{";
  for (int i = 0; i < ticCount; i++) {
    if (i > 0) json += ",";
    json += "\"" + ticKey[i] + "\":\"" + ticVal[i] + "\"";
  }
  if (ticCount > 0) json += ",";
  json += "\"bme_ok\":" + String(bmeOk ? "true" : "false");
  if (bmeOk) {
    json += ",\"bme_temp\":\"" + String(envTemp, 1) + "\"";
    json += ",\"bme_hum\":\""  + String(envHum,  1) + "\"";
    json += ",\"bme_pres\":\"" + String(envPres, 1) + "\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

// ===== PAGE WEB =====
void handleRoot() {
  String page =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='1'>"
    "<style>body{background:black;color:#00ff00;font-family:monospace;}</style>"
    "</head><body><pre>";

  for (int i = 0; i < ticCount; i++) {
    page += ticKey[i] + "\t" + ticVal[i] + "\n";
  }
  if (!ticReady) page += "En attente de données TIC...\n";

  page += "</pre></body></html>";
  server.send(200, "text/html; charset=utf-8", page);
}

void setup() {
  Serial.begin(1200, SERIAL_7E1);

  Wire.begin();
  bmeOk = bme.begin(0x76);
  if (!bmeOk) bmeOk = bme.begin(0x77);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/api", handleAPI);
  server.begin();
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

  // Lecture TIC - détection trames STX/ETX
  while (Serial.available()) {
    char c = Serial.read();

    if (c == 0x02) {          // STX : début de trame
      frameBuffer = "";
      inFrame = true;
    } else if (c == 0x03) {   // ETX : fin de trame
      if (inFrame) {
        parseTICFrame(frameBuffer);
        inFrame = false;
        frameBuffer = "";
      }
    } else if (inFrame) {
      frameBuffer += c;
      if (frameBuffer.length() > 2000) {  // protection mémoire
        inFrame = false;
        frameBuffer = "";
      }
    }
  }
}
