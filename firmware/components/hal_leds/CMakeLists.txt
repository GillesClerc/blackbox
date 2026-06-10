idf_component_register(SRCS "hal_leds.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_rmt freertos log)
