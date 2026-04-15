#include "led_manager.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LED";

static void led_task(void *arg) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(LED_GPIO, 0);

    while (true) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t led_manager_start(void) {
    BaseType_t ret = xTaskCreatePinnedToCore(
        led_task, "LED", STACK_LED, NULL, 1, NULL, 1);
    ESP_LOGI(TAG, "Task started on Core 1");
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
