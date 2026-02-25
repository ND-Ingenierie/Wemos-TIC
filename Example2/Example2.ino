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
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Linky TIC</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0d0d;color:#e0e0e0;font-family:'Segoe UI',Arial,sans-serif;padding:20px;max-width:900px;margin:0 auto}
h1{font-size:1.3rem;color:#fff;letter-spacing:3px;text-transform:uppercase;margin-bottom:20px;padding-bottom:10px;border-bottom:1px solid #222}
.section{font-size:.65rem;color:#555;text-transform:uppercase;letter-spacing:2px;margin:20px 0 10px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:10px;margin-bottom:4px}
.card{background:#161616;border:1px solid #2a2a2a;border-radius:6px;padding:14px}
.card-label{font-size:.6rem;color:#666;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
.card-value{font-size:1.7rem;font-weight:700;color:#fff;line-height:1}
.card-unit{font-size:.75rem;color:#555;margin-left:3px;font-weight:400}
.badge{display:inline-flex;align-items:center;padding:6px 14px;border-radius:4px;font-size:.8rem;font-weight:700;letter-spacing:1px;gap:6px}
.badge-hp{background:#5b21b6;color:#fff}
.badge-hc{background:#1e3a8a;color:#fff}
.badge-base{background:#1e1e1e;color:#aaa;border:1px solid #333}
.badge-ejp{background:#7c1d1d;color:#fff}
.badge-blue{background:#1d4ed8;color:#fff}
.badge-white{background:#e5e7eb;color:#111}
.badge-red{background:#b91c1c;color:#fff}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block}
.dot-blue{background:#60a5fa}
.dot-white{background:#e5e7eb}
.dot-red{background:#f87171}
.info-table{width:100%;border-collapse:collapse;font-size:.8rem}
.info-table td{padding:6px 8px;border-bottom:1px solid #1e1e1e}
.info-table td:first-child{color:#555;width:42%}
.info-table td:last-child{color:#ccc;font-family:monospace}
.footer{color:#333;font-size:.65rem;margin-top:20px;text-align:right}
.wait{color:#333;padding:40px 0}
</style>
</head>
<body>
<h1>Linky TIC</h1>
<div id="app"><p class="wait">Connexion au compteur...</p></div>
<div class="footer" id="footer"></div>
<script>
function q(v,u){
  if(!v||v==='')return'<span class="card-value" style="color:#2a2a2a">--</span>';
  return'<span class="card-value">'+v+'</span><span class="card-unit">'+u+'</span>';
}
function badge(cls,txt,dot){
  return'<span class="badge '+cls+'">'+(dot?'<span class="dot dot-'+dot+'"></span>':'')+txt+'</span>';
}
function periodeBadge(ptec,optarif){
  if(!ptec)return'';
  var isTempo=optarif&&optarif.startsWith('BBR');
  var isHP=ptec.startsWith('HP');
  var isHC=ptec.startsWith('HC');
  if(isTempo){
    var color=ptec.endsWith('JB')?'blue':ptec.endsWith('JW')?'white':'red';
    var per=isHP?'HEURE PLEINE':'HEURE CREUSE';
    return badge('badge-'+color,per,color);
  }
  if(isHP)return badge('badge-hp','HEURE PLEINE');
  if(isHC)return badge('badge-hc','HEURE CREUSE');
  if(ptec==='TH..')return badge('badge-base','TOUTES HEURES');
  if(ptec.startsWith('PM'))return badge('badge-ejp','POINTE MOBILE');
  return badge('badge-base',ptec);
}
function demainBadge(d){
  if(!d)return'';
  var m={'BLEU':'badge-blue','BLAN':'badge-white','ROUG':'badge-red'};
  var l={'BLEU':'BLEU','BLAN':'BLANC','ROUG':'ROUGE'};
  var dot={'BLEU':'blue','BLAN':'white','ROUG':'red'};
  var k=d.substring(0,4).toUpperCase();
  return'<span style="font-size:.65rem;color:#444;margin-left:10px">demain :</span> '+badge(m[k]||'badge-base',l[k]||d,dot[k]);
}
function render(d){
  var html='';
  var optarif=d.OPTARIF||'';
  var ptec=d.PTEC||'';
  var isTempo=optarif.startsWith('BBR');
  var isHCHP=optarif.startsWith('HC')||isTempo;

  /* Période */
  html+='<div class="section">Période tarifaire</div>';
  html+='<div style="margin-bottom:16px">'+periodeBadge(ptec,optarif)+demainBadge(d.DEMAIN)+'</div>';

  /* Puissance */
  html+='<div class="section">Consommation instantanée</div>';
  html+='<div class="grid">';
  html+='<div class="card"><div class="card-label">Puissance apparente</div>'+q(d.PAPP,'VA')+'</div>';
  html+='<div class="card"><div class="card-label">Intensité</div>'+q(d.IINST,'A')+'</div>';
  html+='<div class="card"><div class="card-label">Intensité max</div>'+q(d.IMAX,'A')+'</div>';
  html+='<div class="card"><div class="card-label">Intensité souscrite</div>'+q(d.ISOUSC,'A')+'</div>';
  html+='</div>';

  /* Index */
  html+='<div class="section">Index</div>';
  html+='<div class="grid">';
  if(isTempo){
    html+='<div class="card"><div class="card-label"><span class="dot dot-blue"></span> HC Bleu</div>'+q(d.BBRHCJB,'Wh')+'</div>';
    html+='<div class="card"><div class="card-label"><span class="dot dot-blue"></span> HP Bleu</div>'+q(d.BBRHPJB,'Wh')+'</div>';
    html+='<div class="card"><div class="card-label"><span class="dot dot-white"></span> HC Blanc</div>'+q(d.BBRHCJW,'Wh')+'</div>';
    html+='<div class="card"><div class="card-label"><span class="dot dot-white"></span> HP Blanc</div>'+q(d.BBRHPJW,'Wh')+'</div>';
    html+='<div class="card"><div class="card-label"><span class="dot dot-red"></span> HC Rouge</div>'+q(d.BBRHCJR,'Wh')+'</div>';
    html+='<div class="card"><div class="card-label"><span class="dot dot-red"></span> HP Rouge</div>'+q(d.BBRHPJR,'Wh')+'</div>';
  }else if(isHCHP){
    html+='<div class="card"><div class="card-label">Heures creuses</div>'+q(d.HCHC,'Wh')+'</div>';
    html+='<div class="card"><div class="card-label">Heures pleines</div>'+q(d.HCHP,'Wh')+'</div>';
  }else if(d.BASE){
    html+='<div class="card"><div class="card-label">Index base</div>'+q(d.BASE,'Wh')+'</div>';
  }
  if(d.EJPHN)html+='<div class="card"><div class="card-label">EJP Normal</div>'+q(d.EJPHN,'Wh')+'</div>';
  if(d.EJPHPM)html+='<div class="card"><div class="card-label">EJP Pointe</div>'+q(d.EJPHPM,'Wh')+'</div>';
  html+='</div>';

  /* Environnement */
  if(d.bme_ok===true||d.bme_ok==='true'){
    html+='<div class="section">Environnement</div>';
    html+='<div class="grid">';
    html+='<div class="card"><div class="card-label">Température</div>'+q(d.bme_temp,'°C')+'</div>';
    html+='<div class="card"><div class="card-label">Humidité</div>'+q(d.bme_hum,'%')+'</div>';
    html+='<div class="card"><div class="card-label">Pression</div>'+q(d.bme_pres,'hPa')+'</div>';
    html+='</div>';
  }

  /* Infos compteur */
  html+='<div class="section">Compteur</div>';
  html+='<table class="info-table"><tbody>';
  if(d.ADCO)html+='<tr><td>N° compteur</td><td>'+d.ADCO+'</td></tr>';
  if(d.OPTARIF)html+='<tr><td>Option tarifaire</td><td>'+d.OPTARIF+'</td></tr>';
  if(d.HHPHC)html+='<tr><td>Groupe HC</td><td>'+d.HHPHC+'</td></tr>';
  if(d.PEJP)html+='<tr><td>Préavis EJP</td><td>'+d.PEJP+' min</td></tr>';
  if(d.MOTDETAT)html+='<tr><td>Mot d\'état</td><td>'+d.MOTDETAT+'</td></tr>';
  html+='</tbody></table>';

  document.getElementById('app').innerHTML=html;
  document.getElementById('footer').textContent='Mis à jour : '+new Date().toLocaleTimeString('fr-FR');
}
function update(){
  fetch('/api').then(function(r){return r.json();}).then(render).catch(function(){
    document.getElementById('footer').textContent='Erreur de connexion';
  });
}
update();
setInterval(update,2000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", HTML_PAGE);
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
