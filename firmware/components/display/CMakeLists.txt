idf_component_register(SRCS "eyes.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio esp_driver_spi esp_lcd freertos log
                    PRIV_REQUIRES espressif__esp_lcd_gc9a01)
