idf_component_register(SRCS "nfc.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_i2c freertos log sensors)
