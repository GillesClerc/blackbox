#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7735.h"

#define TAG "main"

void app_main(void) {
    ESP_LOGI(TAG, "Démarrage EscapeBox");
    display_init();

    while (1) {
        display_demo_plasma();
    }
}
