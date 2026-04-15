#ifndef DHT_DRIVER_H
#define DHT_DRIVER_H

#include "driver/gpio.h"
#include "esp_err.h"

/**
 * @brief Read temperature and humidity from a DHT11 sensor.
 *
 * Uses busy-wait GPIO timing via esp_timer_get_time().
 * Retries up to 3 times internally on CRC or timeout failures.
 *
 * Call only from a low-priority task that can afford to block for ~5ms.
 * A 1-second minimum interval between calls is required by the DHT11
 * (enforced by the caller — dht_task uses vTaskDelay(1000ms)).
 *
 * @param gpio      GPIO number wired to DHT11 data pin
 * @param temp      Output: temperature in °C (DHT11 is integer precision)
 * @param humidity  Output: relative humidity in % (integer precision)
 * @return ESP_OK on success; ESP_ERR_TIMEOUT or ESP_ERR_INVALID_CRC on failure
 */
esp_err_t dht11_read(gpio_num_t gpio, float *temp, float *humidity);

#endif /* DHT_DRIVER_H */
