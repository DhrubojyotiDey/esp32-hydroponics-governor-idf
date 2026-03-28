#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "DHT.h"
#include "ota_manager.h"

#define DHTPIN   14  
#define FLOW_PIN 27  
#define DHTTYPE  DHT11
#define LED_PIN  2

DHT dht(DHTPIN, DHTTYPE);
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile long pulseCount = 0;
float temp = 0, hum = 0, flowRate = 0.0;

void IRAM_ATTR pulseCounter() {
  portENTER_CRITICAL_ISR(&mux);
  pulseCount++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  dht.begin();
  setupWiFiAndOTA(); 

  if (!isAPMode()) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      const char index_html[] PROGMEM = R"rawliteral(
      <!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body{font-family:Arial;text-align:center;background:#111;color:#00ffcc;padding:30px;}
        .box{border:2px solid #00ffcc;padding:20px;border-radius:10px;min-width:280px;}
        .data{font-size: 26px; margin: 15px 0;}
        .label{color:#888; font-size:14px; display:block;}
        button{margin-top:20px; padding:10px 20px; background:#ff4444; color:white; border:none; border-radius:5px; cursor:pointer;}
        #stat{font-size:10px; color:#444; margin-top:10px;}
      </style>
      </head><body><div class="box">
        <h1>Hydroponics v1</h1>
        <div class="data"><span class="label">TEMPERATURE</span><span id="t">--</span>°C</div>
        <div class="data"><span class="label">HUMIDITY</span><span id="h">--</span>%</div>
        <div class="data"><span class="label">WATER FLOW</span><span id="f">--</span> L/min</div>
        <button onclick="reboot()">RESTART DEVICE</button>
        <div id="stat">Connecting...</div>
      </div>
      <script>
        var gateway = `ws://${window.location.hostname}/ws`;
        var ws;
        function init() {
          ws = new WebSocket(gateway);
          ws.onopen = () => { document.getElementById('stat').innerHTML = 'LIVE'; };
          ws.onmessage = (e) => {
            var d = JSON.parse(e.data);
            document.getElementById('t').innerHTML = d.temp;
            document.getElementById('h').innerHTML = d.hum;
            document.getElementById('f').innerHTML = d.flow;
          };
          ws.onclose = () => { setTimeout(init, 2000); };
        }
        function reboot() { if(confirm("Restart?")) ws.send("reboot"); }
        window.onload = init;
      </script></body></html>)rawliteral";
      req->send_P(200, "text/html", index_html);
    });
  }
}

void loop() {
  handleOTA();

  if (shouldReboot) { delay(500); ESP.restart(); }

  if (!isAPMode()) {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    if (now - lastUpdate > 500) {
      if (ws.count() > 0 && ws.availableForWriteAll()) {
        lastUpdate = now;

        portENTER_CRITICAL(&mux);
        flowRate = (float)pulseCount / 7.5;
        pulseCount = 0;
        portEXIT_CRITICAL(&mux);

        static unsigned long lastDHT = 0;
        if (now - lastDHT > 3000) {
          temp = dht.readTemperature();
          hum  = dht.readHumidity();
          lastDHT = now;
        }

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "{\"temp\":\"%.1f\",\"hum\":\"%.1f\",\"flow\":\"%.2f\"}", 
                 temp, hum, flowRate);
        ws.textAll(buffer);
      }
    }

    static unsigned long lastBlink = 0;
    if (now - lastBlink > 1000) {
      lastBlink = now;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
  yield(); 
}