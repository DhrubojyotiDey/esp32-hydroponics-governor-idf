#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

// ============================================================
//  SENSOR TRAY
//  A single shared snapshot of the latest sensor state.
//  Producers call updateX() to drop new readings in.
//  Viewers call getSensorJSON() to read the latest state.
//
//  When a producer writes, it directly notifies pushTaskHandle
//  via xTaskNotifyGive() — no polling, no dirty flag needed.
// ============================================================

// Forward declaration — defined in the .ino
extern TaskHandle_t pushTaskHandle;

struct SensorState {
  // --- DHT11 ---
  float    temp;
  float    hum;
  uint32_t ts_dht;
  bool     valid_dht;

  // --- YF-S201 Flow ---
  float    flow;
  uint32_t ts_flow;
  bool     valid_flow;
};

// --- Globals ---
SensorState       tray;
SemaphoreHandle_t trayMutex;

// --- Init ---
void initSensorManager() {
  trayMutex = xSemaphoreCreateMutex();

  tray.temp       = 0.0f;
  tray.hum        = 0.0f;
  tray.ts_dht     = 0;
  tray.valid_dht  = false;

  tray.flow       = 0.0f;
  tray.ts_flow    = 0;
  tray.valid_flow = false;
}

// --- Producer: DHT11 ---
void updateDHT(float t, float h) {
  xSemaphoreTake(trayMutex, portMAX_DELAY);
  tray.temp      = t;
  tray.hum       = h;
  tray.ts_dht    = millis();
  tray.valid_dht = true;
  xSemaphoreGive(trayMutex);

  // Wake pushTask immediately — no polling needed
  if (pushTaskHandle) xTaskNotifyGive(pushTaskHandle);
}

// --- Producer: YF-S201 Flow ---
void updateFlow(float lpm) {
  xSemaphoreTake(trayMutex, portMAX_DELAY);
  tray.flow       = lpm;
  tray.ts_flow    = millis();
  tray.valid_flow = true;
  xSemaphoreGive(trayMutex);

  // Wake pushTask immediately — no polling needed
  if (pushTaskHandle) xTaskNotifyGive(pushTaskHandle);
}

// --- Viewer: full JSON snapshot ---
// Safe to call from any task at any time.
String getSensorJSON() {
  char buf[128];

  xSemaphoreTake(trayMutex, portMAX_DELAY);

  snprintf(buf, sizeof(buf),
    "{\"temp\":%.1f,\"hum\":%.1f,\"flow\":%.2f,\"valid\":%s}",
    tray.temp,
    tray.hum,
    tray.flow,
    (tray.valid_dht && tray.valid_flow) ? "true" : "false"
  );

  xSemaphoreGive(trayMutex);

  return String(buf);
}

#endif // SENSOR_MANAGER_H
