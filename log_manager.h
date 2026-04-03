#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>
#include <AsyncTCP.h>

// ============================================================
// LOG LEVELS
//   DEBUG — verbose internal state (scan, WiFi tick, tray)
//   INFO  — normal lifecycle events (boot, connect, sensor OK)
//   WARN  — recoverable issues (sensor dead, reconnect)
//   ERROR — unrecoverable or unexpected failures
//
// Set LOG_LEVEL to filter noise in production:
//   LOG_LEVEL_DEBUG  → everything
//   LOG_LEVEL_INFO   → skip debug
//   LOG_LEVEL_WARN   → skip debug + info
//   LOG_LEVEL_ERROR  → errors only
// ============================================================
#define LOG_LEVEL_DEBUG  0
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_ERROR  3

// ── Active level — change this one line to filter output ──
#define ACTIVE_LOG_LEVEL LOG_LEVEL_DEBUG

// ============================================================
// QUEUE CONFIG
// Fixed char[LOG_MSG_LEN] buffers — no heap allocation,
// no fragmentation over long uptimes.
// ============================================================
#define LOG_MSG_LEN    160
#define LOG_QUEUE_DEPTH 24

QueueHandle_t logQueue = NULL;

// ── Forward declaration — telnetClient lives in ota_manager.h ──
extern AsyncClient* telnetClient;

// ============================================================
// initLogger()
// Call once from setup() before any tasks start.
// ============================================================
void initLogger() {
  logQueue = xQueueCreate(LOG_QUEUE_DEPTH, LOG_MSG_LEN);
}

// ============================================================
// loggerTask  [Core 1]
// Blocks on queue with portMAX_DELAY — zero CPU when idle.
// Drains messages to Serial and any connected Telnet client.
// ============================================================
void loggerTask(void* pv) {
  char buf[LOG_MSG_LEN];
  while (true) {
    if (xQueueReceive(logQueue, buf, portMAX_DELAY)) {
      Serial.println(buf);
      if (telnetClient && telnetClient->connected()) {
        telnetClient->add(buf, strlen(buf));
        telnetClient->add("\r\n", 2);
        telnetClient->send();
      }
    }
  }
}

// ============================================================
// _enqueueLog  (internal — use LOG_x macros instead)
// Formats: [123456ms] [LEVEL] [TAG] message
// Drops silently if queue is full — never blocks callers.
// ============================================================
void _enqueueLog(int level, const char* tag, const char* msg) {
  if (!logQueue) {
    // Queue not yet initialised — fall back to direct Serial
    Serial.printf("[%7lums] [%-5s] [%s] %s\n",
      millis(),
      level==LOG_LEVEL_DEBUG?"DEBUG":
      level==LOG_LEVEL_INFO ?"INFO" :
      level==LOG_LEVEL_WARN ?"WARN" :"ERROR",
      tag, msg);
    return;
  }
  char buf[LOG_MSG_LEN];
  snprintf(buf, sizeof(buf), "[%7lums] [%-5s] [%-8s] %s",
    millis(),
    level==LOG_LEVEL_DEBUG?"DEBUG":
    level==LOG_LEVEL_INFO ?"INFO" :
    level==LOG_LEVEL_WARN ?"WARN" :"ERROR",
    tag, msg);
  xQueueSend(logQueue, buf, 0);  // drop if full, never block
}

// ============================================================
// Public macros
// Usage:  LOG_INFO("WiFi", "Connected to Ruti Torkarii");
//         LOG_WARN("DHT",  "Sensor read timeout");
//         LOG_DEBUG("Scan","scanComplete=%d", n);
// ============================================================
#define LOG_DEBUG(tag, fmt, ...) \
  do { if (ACTIVE_LOG_LEVEL <= LOG_LEVEL_DEBUG) { \
    char _lbuf[LOG_MSG_LEN]; \
    snprintf(_lbuf, sizeof(_lbuf), fmt, ##__VA_ARGS__); \
    _enqueueLog(LOG_LEVEL_DEBUG, tag, _lbuf); \
  }} while(0)

#define LOG_INFO(tag, fmt, ...) \
  do { if (ACTIVE_LOG_LEVEL <= LOG_LEVEL_INFO) { \
    char _lbuf[LOG_MSG_LEN]; \
    snprintf(_lbuf, sizeof(_lbuf), fmt, ##__VA_ARGS__); \
    _enqueueLog(LOG_LEVEL_INFO, tag, _lbuf); \
  }} while(0)

#define LOG_WARN(tag, fmt, ...) \
  do { if (ACTIVE_LOG_LEVEL <= LOG_LEVEL_WARN) { \
    char _lbuf[LOG_MSG_LEN]; \
    snprintf(_lbuf, sizeof(_lbuf), fmt, ##__VA_ARGS__); \
    _enqueueLog(LOG_LEVEL_WARN, tag, _lbuf); \
  }} while(0)

#define LOG_ERROR(tag, fmt, ...) \
  do { if (ACTIVE_LOG_LEVEL <= LOG_LEVEL_ERROR) { \
    char _lbuf[LOG_MSG_LEN]; \
    snprintf(_lbuf, sizeof(_lbuf), fmt, ##__VA_ARGS__); \
    _enqueueLog(LOG_LEVEL_ERROR, tag, _lbuf); \
  }} while(0)

// ── Legacy compatibility shim ──────────────────────────────
// sensor_manager.h and other modules call enqueueLog(const char*).
// This keeps that call-site working without changes.
void enqueueLog(const char* msg) {
  _enqueueLog(LOG_LEVEL_INFO, "SYS", msg);
}

#endif // LOG_MANAGER_H
