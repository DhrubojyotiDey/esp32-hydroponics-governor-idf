#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

extern TaskHandle_t pushTaskHandle;

#include "log_manager.h"

// ============================================================
// CONFIG
// ============================================================
#define MAX_SENSORS 10

// ============================================================
// SENSOR META (HEALTH TRACKING)
// ============================================================
struct SensorMeta {
  const char* name;
  uint32_t    timeout;
  uint32_t    lastSeen;

  bool        active;
  bool        alive;
  bool        prevAlive;
};

// ============================================================
// SENSOR DATA (TRAY)
// ============================================================
struct SensorState {
  float temp;
  float hum;
  float flow;
};

// ============================================================
// GLOBALS
// ============================================================
SensorMeta        sensors[MAX_SENSORS];
uint8_t           sensorCount = 0;

SensorState       tray;
SemaphoreHandle_t trayMutex;

// ============================================================
// INIT
// ============================================================
void initSensorManager() {
  trayMutex = xSemaphoreCreateMutex();
  sensorCount = 0;

  tray.temp = 0;
  tray.hum  = 0;
  tray.flow = 0;
}

// ============================================================
// REGISTER SENSOR
// ============================================================
void registerSensor(const char* name, uint32_t timeout) {
  if (sensorCount >= MAX_SENSORS) return;

  sensors[sensorCount++] = {
    name,
    timeout,
    0,
    false,
    false,
    false
  };
}

// ============================================================
// MARK SENSOR ALIVE
// ============================================================
void markSensorAlive(const char* name) {
  uint32_t now = millis();

  xSemaphoreTake(trayMutex, portMAX_DELAY);
  for (int i = 0; i < sensorCount; i++) {
    if (strcmp(sensors[i].name, name) == 0) {
      sensors[i].lastSeen = now;
      sensors[i].active   = true;
      // Do NOT set alive=true here — the health engine computes
      // aliveness from lastSeen every cycle. Setting it directly
      // bypasses transition detection and breaks RECOVERED alerts.
      break;
    }
  }
  xSemaphoreGive(trayMutex);
}

// ============================================================
// HEALTH ENGINE (CORE LOGIC)
// Runs every 1s from loop().
// DEAD alert:      fires every cycle while sensor is dead
//                  so Serial / Telnet keeps printing until
//                  the sensor is plugged back in.
// RECOVERED alert: fires once on the DEAD → ALIVE transition.
// All mutex/notify work happens outside the mutex to avoid
// priority inversion.
// ============================================================
void updateSensorHealth() {
  uint32_t now = millis();

  struct Event { bool dead; bool transition; char name[16]; };
  Event events[MAX_SENSORS];
  int evtCount = 0;

  xSemaphoreTake(trayMutex, portMAX_DELAY);

  for (int i = 0; i < sensorCount; i++) {
    SensorMeta &s = sensors[i];

    s.prevAlive = s.alive;

    if (!s.active) {
      s.alive = false;
    } else {
      s.alive = (now - s.lastSeen) < s.timeout;
    }

    bool transition = (s.prevAlive != s.alive);

    // Record event if:
    //   - sensor just died (transition ALIVE→DEAD)
    //   - sensor is already dead (repeat alert every cycle)
    //   - sensor just recovered (transition DEAD→ALIVE, once)
    if (!s.alive || (s.alive && transition)) {
      if (evtCount < MAX_SENSORS) {
        events[evtCount].dead       = !s.alive;
        events[evtCount].transition = transition;
        strncpy(events[evtCount].name, s.name, 15);
        events[evtCount].name[15] = '\0';
        evtCount++;
      }
    }
  }

  xSemaphoreGive(trayMutex);

  // Act outside mutex
  bool shouldNotify = false;
  for (int i = 0; i < evtCount; i++) {
    if (events[i].dead) {
      // Fires every cycle while dead
      LOG_WARN("Health", "Sensor '%s' is DEAD — no data received", events[i].name);
    } else {
      // Fires once on recovery
      LOG_INFO("Health", "Sensor '%s' RECOVERED", events[i].name);
      shouldNotify = true;
    }
  }

  if (shouldNotify && pushTaskHandle) {
    xTaskNotifyGive(pushTaskHandle);
  }
}

// ============================================================
// PRODUCERS
// ============================================================
void updateDHT(float t, float h) {
  if (isnan(t) || isnan(h)) return;

  xSemaphoreTake(trayMutex, portMAX_DELAY);
  tray.temp = t;
  tray.hum  = h;
  xSemaphoreGive(trayMutex);

  markSensorAlive("dht");

  if (pushTaskHandle) xTaskNotifyGive(pushTaskHandle);
}

void updateFlow(float lpm) {
  if (isnan(lpm)) return;

  xSemaphoreTake(trayMutex, portMAX_DELAY);
  tray.flow = lpm;
  xSemaphoreGive(trayMutex);

  markSensorAlive("flow");

  if (pushTaskHandle) xTaskNotifyGive(pushTaskHandle);
}

// ============================================================
// JSON BUILDER
// ============================================================
String getSensorJSON() {
  char buf[256];
  // MAX_SENSORS * ~20 chars per entry + wrapping = safe at 256
  char sensorBuf[256] = "{";
  int  sensorBufLen   = 1;  // tracks current length, avoids unsafe strcat

  xSemaphoreTake(trayMutex, portMAX_DELAY);

  for (int i = 0; i < sensorCount; i++) {
    char entry[32];
    int wrote = snprintf(entry, sizeof(entry),
      "\"%s\":%s%s",
      sensors[i].name,
      sensors[i].alive ? "true" : "false",
      (i < sensorCount - 1) ? "," : ""
    );
    // Only append if it fits — silently skip if buffer would overflow
    if (sensorBufLen + wrote < (int)sizeof(sensorBuf) - 2) {
      memcpy(sensorBuf + sensorBufLen, entry, wrote);
      sensorBufLen += wrote;
    }
  }
  sensorBuf[sensorBufLen++] = '}';
  sensorBuf[sensorBufLen]   = '\0';

  snprintf(buf, sizeof(buf),
    "{\"temp\":%.1f,\"hum\":%.1f,\"flow\":%.2f,\"sensors\":%s}",
    tray.temp,
    tray.hum,
    tray.flow,
    sensorBuf
  );

  xSemaphoreGive(trayMutex);
  return String(buf);
}

#endif