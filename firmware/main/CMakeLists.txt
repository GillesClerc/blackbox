idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES display sensors audio leds freertos log esp_driver_i2c)
