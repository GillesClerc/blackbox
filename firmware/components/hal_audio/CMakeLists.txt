idf_component_register(SRCS "hal_audio.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_i2s esp_driver_i2c esp_driver_gpio freertos log esp_timer hal_i2c_bus minimp3)
