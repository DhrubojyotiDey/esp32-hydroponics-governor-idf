#ifndef LED_MANAGER_H
#define LED_MANAGER_H
#include "esp_err.h"
/** Create the LED heartbeat task. Returns ESP_OK on success. */
esp_err_t led_manager_start(void);
#endif /* LED_MANAGER_H */
