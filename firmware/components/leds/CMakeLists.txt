idf_component_register(SRCS "leds.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_rmt freertos log)
