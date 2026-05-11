idf_component_register(SRCS "nfc.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c freertos log)
