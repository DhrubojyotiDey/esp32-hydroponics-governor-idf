#include <Arduino.h>
#include "log_manager.h"    // ← must be first — others depend on it
#include "ota_manager.h"
#include "sensor_manager.h"
#include "DHT.h"

// ============================================================
//  Pin definitions
// ============================================================
#define DHTPIN    14
#define DHTTYPE   DHT11
#define FLOW_PIN  27
#define LED_PIN   2

// ============================================================
//  Sensor objects
// ============================================================
DHT dht(DHTPIN, DHTTYPE);

// ============================================================
//  Flow ISR
// ============================================================
portMUX_TYPE      flowMux    = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t pulseCount = 0;

void IRAM_ATTR flowISR() {
  portENTER_CRITICAL_ISR(&flowMux);
  pulseCount++;
  portEXIT_CRITICAL_ISR(&flowMux);
}

// ============================================================
//  Task handles
//  pushTaskHandle is extern'd in sensor_manager.h so producers
//  can call xTaskNotifyGive() directly after a tray update.
// ============================================================
TaskHandle_t pushTaskHandle       = NULL;
TaskHandle_t dhtTaskHandle        = NULL;
TaskHandle_t flowTaskHandle       = NULL;
TaskHandle_t loggerTaskHandle     = NULL;
TaskHandle_t sensorViewTaskHandle = NULL;
TaskHandle_t ledTaskHandle        = NULL;

// ============================================================
//  TASK: DHT Producer  [Core 0]
// ============================================================
void dhtTask(void* pv) {
  dht.begin();
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  float lastT = NAN;
  float lastH = NAN;

  while (true) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      markSensorAlive("dht");
      if (t != lastT || h != lastH) {
        updateDHT(t, h);
        lastT = t;
        lastH = h;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// ============================================================
//  TASK: Flow Producer  [Core 0]
// ============================================================
void flowTask(void* pv) {
  while (true) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    portENTER_CRITICAL(&flowMux);
    uint32_t count = pulseCount;
    pulseCount = 0;
    portEXIT_CRITICAL(&flowMux);

    float lpm = (float)count / 3.75f;
    updateFlow(lpm);
  }
}

// ============================================================
//  TASK: Push  [Core 1]
//  Blocks on TaskNotification — woken by sensor producers.
// ============================================================
void pushTask(void* pv) {
  uint32_t notifyCount;
  while (true) {
    notifyCount = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (notifyCount > 1) {
      LOG_DEBUG("Push", "Burst: %lu events coalesced", notifyCount);
    }
    pushSensorUpdate();
    taskYIELD();
  }
}

// ============================================================
//  TASK: Sensor View Logger  [Core 1]
//  During AP/setup: minimal one-liner so WiFi logs stay readable.
//  After setup: full JSON every 1s.
// ============================================================
void sensorViewTask(void* pv) {
  while (true) {
    if (isAPMode()) {
      String j = getSensorJSON();
      bool dhtOk  = (j.indexOf("\"dht\":true")  >= 0);
      bool flowOk = (j.indexOf("\"flow\":true") >= 0);
      LOG_DEBUG("SENSOR", "dht:%s flow:%s",
        dhtOk  ? "OK" : "DEAD",
        flowOk ? "OK" : "DEAD");
    } else {
      LOG_INFO("SENSOR", "%s", getSensorJSON().c_str());
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// ============================================================
//  TASK: LED Heartbeat  [Core 1]
// ============================================================
void ledTask(void* pv) {
  pinMode(LED_PIN, OUTPUT);
  while (true) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP32 Hydroponics Governor");

  // Logger must init first — everything else uses it
  initLogger();

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);

  initSensorManager();

  registerSensor("dht",  10000);
  registerSensor("flow",  5000);

  LOG_INFO("BOOT", "Sensor registry ready — dht(10s) flow(5s)");

  setupWiFiAndOTA();

  // ── Core 0: sensor producers ──────────────────────────────
  xTaskCreatePinnedToCore(dhtTask,        "DHT",    4096, NULL, 2, &dhtTaskHandle,        0);
  xTaskCreatePinnedToCore(flowTask,       "Flow",   2048, NULL, 2, &flowTaskHandle,       0);

  // ── Core 1: network consumers ────────────────────────────
  xTaskCreatePinnedToCore(pushTask,       "Push",   4096, NULL, 2, &pushTaskHandle,       1);
  xTaskCreatePinnedToCore(loggerTask,     "Logger", 4096, NULL, 1, &loggerTaskHandle,     1);
  xTaskCreatePinnedToCore(sensorViewTask, "View",   4096, NULL, 1, &sensorViewTaskHandle, 1);
  xTaskCreatePinnedToCore(ledTask,        "LED",    1024, NULL, 1, &ledTaskHandle,        1);

  LOG_INFO("BOOT", "All tasks started");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  handleOTA();

  static unsigned long lastHealthCheck = 0;
  unsigned long now = millis();

  if (now - lastHealthCheck >= 1000) {
    updateSensorHealth();
    lastHealthCheck = now;
  }

  if (shouldReboot) {
    LOG_WARN("SYS", "Rebooting on request...");
    delay(500);
    ESP.restart();
  }

  vTaskDelay(10 / portTICK_PERIOD_MS);
}
