#include "dht_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rom/ets_sys.h"   /* ets_delay_us — ROM busy-wait, not preemptible */

static const char *TAG = "DHT";

/* ── Timing helpers ──────────────────────────────────────── */

/**
 * Spin-wait until GPIO reaches `target` level.
 * Returns elapsed microseconds, or -1 on timeout.
 *
 * Uses esp_timer_get_time() (64-bit us counter backed by hardware timer).
 * This is more accurate than vTaskDelay for sub-ms windows, but NOTE:
 * if the FreeRTOS scheduler preempts this loop during a DHT bit window,
 * the timing will be wrong → caller retries on CRC failure.
 */
static int64_t wait_level(gpio_num_t gpio, int target, int64_t timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(gpio) != target) {
        if ((esp_timer_get_time() - start) >= timeout_us) return -1;
    }
    return esp_timer_get_time() - start;
}

/* ── Single read attempt ─────────────────────────────────── */

static esp_err_t dht11_read_once(gpio_num_t gpio, float *temp, float *humidity) {
    uint8_t data[5] = {0};

    /* ── 1. Host start signal ───────────────────────────────
     * Pull LOW for 20ms (spec: ≥18ms), then release HIGH.    */
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(gpio, 1);
    ets_delay_us(40);   /* brief high before switching to input */

    /* Switch to input with pull-up so line idles high */
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);

    /* ── 2. DHT11 response signal ───────────────────────────
     * Sensor pulls LOW ~80µs, then HIGH ~80µs to signal ready. */
    if (wait_level(gpio, 0, 100) < 0) return ESP_ERR_TIMEOUT;  /* wait for LOW  */
    if (wait_level(gpio, 1, 100) < 0) return ESP_ERR_TIMEOUT;  /* wait for HIGH */
    if (wait_level(gpio, 0, 100) < 0) return ESP_ERR_TIMEOUT;  /* data start LOW*/

    /* ── 3. Read 40 bits (5 bytes) ──────────────────────────
     * Each bit: ~50µs LOW, then HIGH pulse.
     *   HIGH 26-28µs → bit 0
     *   HIGH ~70µs   → bit 1
     * We measure the HIGH pulse; >40µs = 1.                  */
    for (int i = 0; i < 40; i++) {
        if (wait_level(gpio, 1, 80)  < 0) return ESP_ERR_TIMEOUT; /* wait rising edge  */
        int64_t high_start = esp_timer_get_time();
        if (wait_level(gpio, 0, 100) < 0) return ESP_ERR_TIMEOUT; /* wait falling edge */
        int64_t pulse_us = esp_timer_get_time() - high_start;

        data[i / 8] <<= 1;
        if (pulse_us > 40) data[i / 8] |= 1;
    }

    /* ── 4. Checksum ────────────────────────────────────────*/
    uint8_t chk = (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFF);
    if (data[4] != chk) {
        ESP_LOGD(TAG, "CRC fail: got 0x%02X expected 0x%02X", data[4], chk);
        return ESP_ERR_INVALID_CRC;
    }

    /* DHT11 only uses integer bytes; data[1] and data[3] are always 0 */
    *humidity = (float)data[0];
    *temp     = (float)data[2];
    return ESP_OK;
}

/* ── Public API ──────────────────────────────────────────── */

esp_err_t dht11_read(gpio_num_t gpio, float *temp, float *humidity) {
    /* Retry up to 3 times — scheduler preemption can cause single failures */
    for (int attempt = 0; attempt < 3; attempt++) {
        esp_err_t ret = dht11_read_once(gpio, temp, humidity);
        if (ret == ESP_OK) return ESP_OK;
        /* Brief pause: lets the DHT11 reset its output stage */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGD(TAG, "All 3 read attempts failed");
    return ESP_FAIL;
}
