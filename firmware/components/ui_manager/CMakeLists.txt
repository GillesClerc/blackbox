idf_component_register(SRCS "ui_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES display config_manager audio leds sensors freertos log esp_timer lvgl__lvgl)
