idf_component_register(SRCS "st7735.c" "ili9488.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio esp_driver_spi freertos log)
