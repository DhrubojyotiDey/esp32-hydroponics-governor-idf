#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <Preferences.h>

const char* ota_hostname = "hydroponics-esp32";
const int   WIFI_TIMEOUT = 20;        
const char* AP_SSID      = "ESP32_SETUP_AP";

DNSServer      dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); 
bool           _apMode = false;
bool           shouldReboot = false; 

#define LOG(msg) { Serial.println(msg); }

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS Client #%u connected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    if (len >= 6 && strncmp((char*)data, "reboot", 6) == 0) {
      shouldReboot = true; 
    }
  }
}

const char _login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Setup</title><style>body{font-family:Arial;text-align:center;padding:50px;background:#f4f4f4;}
.c{background:white;padding:20px;border-radius:10px;display:inline-block;}
input{display:block;margin:10px auto;padding:10px;width:80%;}</style></head>
<body><div class="c"><h2>WiFi Setup</h2><form action="/save" method="POST">
<input name="s" placeholder="SSID"><input name="p" type="password" placeholder="Password">
<button type="submit">Connect</button></form></div></body></html>)rawliteral";

void _startProvisioning() {
  _apMode = true;
  WiFi.softAP(AP_SSID);
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){ req->send_P(200, "text/html", _login_html); });
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest* req){
    Preferences p; p.begin("wifi", false);
    p.putString("s", req->arg("s")); p.putString("p", req->arg("p")); p.end();
    req->send(200, "text/plain", "Saved. Rebooting...");
    delay(2000); ESP.restart();
  });
  server.begin();
}

void setupWiFiAndOTA() {
  Preferences prefs;
  prefs.begin("wifi", true);
  String ssid = prefs.getString("s", "");
  String pass = prefs.getString("p", "");
  prefs.end();

  if (ssid == "") { _startProvisioning(); return; }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int c = 0; while (WiFi.status() != WL_CONNECTED && c < 40) { delay(500); Serial.print("."); c++; }

  if (WiFi.status() != WL_CONNECTED) {
    Preferences p; p.begin("wifi", false); p.clear(); p.end();
    ESP.restart();
  }

  MDNS.begin(ota_hostname);
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.begin();
  server.begin();
}

void handleOTA() {
  if (_apMode) {
    dnsServer.processNextRequest();
  } else {
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 5000) {
      ws.cleanupClients();
      lastCleanup = millis();
    }
    ArduinoOTA.handle();
  }
}

bool isAPMode() { return _apMode; }
#endif