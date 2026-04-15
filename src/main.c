#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "app_config.h"
#include "sensor_manager.h"
#include "dht_driver.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "dns_server.h"
#include "led_manager.h"
#include "telnet_server.h"

static const char *TAG = "MAIN";

/* ── Task handles — extern'd in sensor_manager.h ──────────── */
TaskHandle_t push_task_handle = NULL;

/* ── Flow ISR state ──────────────────────────────────────── */
static portMUX_TYPE          s_flow_mux    = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t     s_pulse_count = 0;

/* Flow pulse ISR — placed in IRAM so it runs even during flash ops */
static void IRAM_ATTR flow_isr_handler(void *arg) {
    portENTER_CRITICAL_ISR(&s_flow_mux);
    s_pulse_count++;
    portEXIT_CRITICAL_ISR(&s_flow_mux);
}

/* ──────────────────────────────────────────────────────────
 * TASK: DHT Producer  [Core 0, priority 2]
 *
 * Reads DHT11 every 1s via the native dht11_read() driver.
 * Calls sensor_mark_alive() on every valid read so lastSeen
 * stays fresh even when the value is unchanged.
 * Calls sensor_update_dht() only when value changes to avoid
 * hammering the WebSocket with redundant pushes.
 * ──────────────────────────────────────────────────────────*/
static void dht_task(void *arg) {
    /* DHT11 needs 1s stabilisation after power-on */
    vTaskDelay(pdMS_TO_TICKS(2000));

    float last_t = -999.0f, last_h = -999.0f;

    while (true) {
        float t = 0.0f, h = 0.0f;
        esp_err_t ret = dht11_read(DHT_GPIO, &t, &h);

        if (ret == ESP_OK) {
            sensor_mark_alive("dht");      /* always refresh lastSeen */

            if (t != last_t || h != last_h) {
                sensor_update_dht(t, h);   /* push only on change    */
                last_t = t;
                last_h = h;
            }
        } else {
            ESP_LOGD(TAG, "DHT read failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ──────────────────────────────────────────────────────────
 * TASK: Flow Producer  [Core 0, priority 2]
 *
 * Every 500ms: snapshots and resets pulse counter under
 * critical section, converts to L/min, calls sensor_update_flow.
 * YF-S201 formula: lpm = count / (7.5 Hz/lpm × 0.5 s) = count / 3.75
 * ──────────────────────────────────────────────────────────*/
static void flow_task(void *arg) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(FLOW_MEASURE_INTERVAL_MS));

        portENTER_CRITICAL(&s_flow_mux);
        uint32_t count = s_pulse_count;
        s_pulse_count  = 0;
        portEXIT_CRITICAL(&s_flow_mux);

        float lpm = (float)count / FLOW_PULSES_PER_LPM;
        sensor_update_flow(lpm);
    }
}

/* ──────────────────────────────────────────────────────────
 * TASK: Push  [Core 1, priority 2]
 *
 * Truly event-driven: blocks on ulTaskNotifyTake(portMAX_DELAY).
 * Woken by sensor_update_dht/flow via xTaskNotifyGive.
 * Sends JSON to all WebSocket clients the instant data arrives.
 * taskYIELD() after send prevents push_task from starving
 * lower-priority tasks under burst notification conditions.
 * ──────────────────────────────────────────────────────────*/
static void push_task(void *arg) {
    char buf[WS_JSON_BUF_SIZE];

    while (true) {
        uint32_t n = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (n > 1) {
            ESP_LOGD(TAG, "[push] burst: %lu events coalesced", (unsigned long)n);
        }

        if (sensor_get_json(buf, sizeof(buf)) == ESP_OK) {
            web_server_push_sensor_update(buf);
        }

        taskYIELD();
    }
}

/* ──────────────────────────────────────────────────────────
 * TASK: Logger  [Core 1, priority 1]
 *
 * Blocks on the log queue. Receives fixed-size LOG_MSG_LEN
 * char buffers — no heap allocation per message.
 * In AP mode this still routes to ESP_LOGI; Telnet could be
 * added here later without changing any producer code.
 * ──────────────────────────────────────────────────────────*/
static void logger_task(void *arg) {
    QueueHandle_t q = (QueueHandle_t)sensor_get_log_queue();
    char buf[LOG_MSG_LEN];

    while (true) {
        if (xQueueReceive(q, buf, portMAX_DELAY)) {
            /* IDF log — always goes to UART0 (USB serial) */
            ESP_LOGI("LOG", "%s", buf);
            /* Telnet — forwarded if a client is connected */
            telnet_server_send(buf);
        }
    }
}

/* ──────────────────────────────────────────────────────────
 * TASK: Sensor View  [Core 1, priority 1]
 *
 * In AP mode:    terse one-liner showing each sensor OK/DEAD.
 * In STA mode:   full JSON every 1s to the log queue.
 * Kept separate from push_task so the log stream is always
 * available regardless of WebSocket client count.
 * ──────────────────────────────────────────────────────────*/
static void sensor_view_task(void *arg) {
    char buf[WS_JSON_BUF_SIZE];

    while (true) {
        if (sensor_is_ap_mode()) {
            /* Minimal heartbeat — keeps setup Serial readable */
            if (sensor_get_json(buf, sizeof(buf)) == ESP_OK) {
                bool dht_ok  = (strstr(buf, "\"dht\":true")  != NULL);
                bool flow_ok = (strstr(buf, "\"flow\":true") != NULL);
                char mini[64];
                snprintf(mini, sizeof(mini), "[SENSOR] dht:%s flow:%s",
                         dht_ok  ? "OK" : "DEAD",
                         flow_ok ? "OK" : "DEAD");
                enqueue_log(mini);
            }
        } else {
            if (sensor_get_json(buf, sizeof(buf)) == ESP_OK) {
                enqueue_log(buf);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ──────────────────────────────────────────────────────────
 * APP MAIN — ESP-IDF entry point (replaces Arduino setup/loop)
 * ──────────────────────────────────────────────────────────*/
void app_main(void) {
    ESP_LOGI(TAG, "══════════════════════════════════════");
    ESP_LOGI(TAG, " ESP32 Hydroponics Governor — boot");
    ESP_LOGI(TAG, "══════════════════════════════════════");

    /* ── 1. Sensor manager (mutex + log queue) ──────────── */
    sensor_manager_init();
    sensor_register("dht",  SENSOR_DHT_TIMEOUT_MS);
    sensor_register("flow", SENSOR_FLOW_TIMEOUT_MS);

    /* ── 2. Flow sensor ISR ─────────────────────────────── */
    gpio_config_t flow_cfg = {
        .pin_bit_mask = (1ULL << FLOW_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,  /* YF-S201 pulses LOW */
    };
    ESP_ERROR_CHECK(gpio_config(&flow_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(FLOW_GPIO, flow_isr_handler, NULL));

    /* ── 3. WiFi (NVS init, netif, event loop, connect) ── */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* Wire AP-shutdown → DNS stop so the DNS task is cleaned up
     * when the user reaches the dashboard from home WiFi.
     * This prevents a leaked socket + FreeRTOS task.          */
    wifi_manager_set_ap_shutdown_cb(dns_server_stop);

    ESP_ERROR_CHECK(wifi_manager_start());

    /* ── 4. DNS server (captive portal, AP mode only) ───── */
    if (wifi_manager_ap_active()) {
        ESP_ERROR_CHECK(dns_server_start());
    }

    /* ── 5. HTTP server (both AP and STA modes) ─────────── */
    ESP_ERROR_CHECK(web_server_start());

    /* ── 6. Telnet log server ───────────────────────────── */
    ESP_ERROR_CHECK(telnet_server_start());

    /* ── 7. LED heartbeat ────────────────────────────────── */
    ESP_ERROR_CHECK(led_manager_start());

    /* ── 7. FreeRTOS tasks ───────────────────────────────── */

    /* Core 0 — sensor producers (isolated from network stack) */
    xTaskCreatePinnedToCore(dht_task,  "DHT",  STACK_DHT,  NULL, 2, NULL,              0);
    xTaskCreatePinnedToCore(flow_task, "Flow", STACK_FLOW, NULL, 2, NULL,              0);

    /* Core 1 — network consumers */
    xTaskCreatePinnedToCore(push_task,        "Push",   STACK_PUSH,   NULL, 2, &push_task_handle, 1);
    xTaskCreatePinnedToCore(logger_task,      "Logger", STACK_LOGGER, NULL, 1, NULL,              1);
    xTaskCreatePinnedToCore(sensor_view_task, "View",   STACK_VIEW,   NULL, 1, NULL,              1);

    ESP_LOGI(TAG, "All tasks started");
    ESP_LOGI(TAG, "══════════════════════════════════════");

    /* ── 8. Main loop — health engine + periodic housekeeping ──
     *
     * In native IDF, app_main() runs in its own FreeRTOS task
     * (stack ~4KB by default). We keep it alive as the health
     * engine ticker rather than letting it return (which would
     * call esp_restart() internally).
     *
     * Health check: 1s interval, same as the Arduino loop().
     * Reconnect:    30s backoff after WiFi drop in STA mode.
     * ─────────────────────────────────────────────────────── */
    int64_t last_health_us  = 0;
    int64_t last_reconnect_us = 0;

    while (true) {
        int64_t now = esp_timer_get_time();

        /* Sensor dead/recovered detection */
        if ((now - last_health_us) >= 1000000LL) {
            sensor_update_health();
            last_health_us = now;
        }

        /* WiFi reconnect with 30s backoff (STA mode only).
         *
         * BUG FIX: original called wifi_manager_start() here which
         * re-runs esp_netif_init() and esp_event_loop_create_default()
         * — both abort() on a second call. wifi_manager_reconnect()
         * only calls esp_wifi_connect() with stored credentials.     */
        if (!wifi_manager_ap_active() &&
            wifi_manager_get_state() == WIFI_MGR_FAILED) {
            if ((now - last_reconnect_us) >= 30000000LL) {
                ESP_LOGW(TAG, "WiFi lost — reconnecting");
                wifi_manager_reconnect();
                last_reconnect_us = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
