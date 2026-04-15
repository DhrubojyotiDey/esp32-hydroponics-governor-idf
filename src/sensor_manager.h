#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── push_task_handle ───────────────────────────────────────
 * Set in main.c after xTaskCreatePinnedToCore().
 * sensor_update_dht/flow call xTaskNotifyGive() on it.       */
extern TaskHandle_t push_task_handle;

/* ── Lifecycle ───────────────────────────────────────────── */

/** Initialise mutex, log queue, zero sensor tray. Call first. */
void sensor_manager_init(void);

/** Register a named sensor with a dead-detection timeout (ms). */
void sensor_register(const char *name, uint32_t timeout_ms);

/* ── Producers ───────────────────────────────────────────── */

/** Called on every valid DHT read; updates tray, notifies push_task. */
void sensor_update_dht(float temp, float hum);

/** Called every FLOW_MEASURE_INTERVAL_MS; updates tray, notifies push_task. */
void sensor_update_flow(float lpm);

/** Mark sensor as alive without updating tray values.
 *  Call on every valid raw read so lastSeen stays fresh even
 *  when the value hasn't changed (prevents false-DEAD alerts). */
void sensor_mark_alive(const char *name);

/* ── Health engine ───────────────────────────────────────── */

/** Run once per second from the main loop.
 *  Emits DEAD/RECOVERED log events; notifies push_task on RECOVERED. */
void sensor_update_health(void);

/* ── Output ──────────────────────────────────────────────── */

/** Write JSON to caller-provided buffer.
 *  Returns ESP_OK on success, ESP_ERR_NO_MEM if buffer is too small. */
esp_err_t sensor_get_json(char *buf, size_t len);

/* ── AP mode flag ────────────────────────────────────────── */

/** Set by wifi_manager; read by sensor_view_task to decide log verbosity. */
void sensor_set_ap_mode(bool ap);
bool sensor_is_ap_mode(void);

/* ── Log queue ───────────────────────────────────────────── */

/** Queue a fixed-size message for the logger_task.
 *  Non-blocking: drops silently if queue is full.             */
void enqueue_log(const char *msg);

/** Return the log queue handle (used by logger_task in main.c). */
void *sensor_get_log_queue(void);

#endif /* SENSOR_MANAGER_H */
