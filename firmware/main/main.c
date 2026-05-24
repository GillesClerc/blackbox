#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9488.h"

#define TAG "main"

// Animation simple : le titre clignote entre deux couleurs
static void blink_task(void *arg)
{
    bool toggle = false;
    while (1) {
        uint16_t c = toggle ? COLOR_GOLD : COLOR_ORANGE;
        ili9488_draw_string(14, 200, "ESCAPEBOX", c, COLOR_BLACK, 3);
        toggle = !toggle;
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== EscapeBox S3 boot ===");

    ESP_ERROR_CHECK(ili9488_init());
    ili9488_test_screen();

    // Lance l'animation en tâche de fond pour valider que FreeRTOS tourne
    xTaskCreate(blink_task, "blink", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "affichage OK — en attente");
}
