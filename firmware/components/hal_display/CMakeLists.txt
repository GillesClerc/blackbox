idf_component_register(SRCS "hal_display.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_lcd freertos log espressif__esp_lcd_gc9a01)
