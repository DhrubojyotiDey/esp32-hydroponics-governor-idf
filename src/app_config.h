#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/gpio.h"

/* ── Hardware pins ──────────────────────────────────────────
 * Match physical wiring. Do NOT change without rewiring.     */
#define DHT_GPIO        GPIO_NUM_14   /* DHT11 data pin        */
#define FLOW_GPIO       GPIO_NUM_27   /* YF-S201 pulse output  */
#define LED_GPIO        GPIO_NUM_2    /* Onboard LED           */

/* ── Network identifiers ────────────────────────────────────*/
#define AP_SSID             "Hydroponics_Setup"
#define AP_IP_ADDR          "192.168.4.1"
#define MDNS_HOSTNAME       "hydroponics"  /* → hydroponics.local */
#define HTTP_PORT           80
#define DNS_PORT            53
#define TELNET_PORT         23

/* ── NVS credential storage ─────────────────────────────────*/
#define NVS_NAMESPACE       "wifi"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASS        "pass"

/* ── WiFi connection timing ─────────────────────────────────*/
#define WIFI_CONNECT_TIMEOUT_MS    30000  /* hard timeout           */
#define WIFI_RECONNECT_INTERVAL_MS 30000  /* backoff after drop     */

/* ── Sensor health timeouts ─────────────────────────────────
 * Sensor marked DEAD if no reading received within timeout.  */
#define SENSOR_DHT_TIMEOUT_MS      10000
#define SENSOR_FLOW_TIMEOUT_MS     5000

/* ── Flow measurement window ────────────────────────────────*/
#define FLOW_MEASURE_INTERVAL_MS   500
/* YF-S201: 7.5 pulses/s = 1 L/min.
 * Over 500ms window: lpm = count / (7.5 × 0.5) = count / 3.75 */
#define FLOW_PULSES_PER_LPM        3.75f

/* ── WebSocket ───────────────────────────────────────────────*/
#define MAX_WS_CLIENTS      8
#define WS_JSON_BUF_SIZE    384

/* ── Log queue (fixed char buffers, no heap fragmentation) ──*/
#define LOG_MSG_LEN         128
#define LOG_QUEUE_DEPTH     16

/* ── OTA upload chunk size ───────────────────────────────────*/
#define OTA_BUF_SIZE        1024

/* ── Sensor registry capacity ───────────────────────────────*/
#define MAX_SENSORS         10

/* ── FreeRTOS task stack sizes (bytes) ──────────────────────*/
#define STACK_DHT           4096
#define STACK_FLOW          2048
#define STACK_PUSH          4096
#define STACK_LOGGER        4096
#define STACK_VIEW          4096
#define STACK_LED           1024
#define STACK_DNS           3072

#endif /* APP_CONFIG_H */
