idf_component_register(SRCS "hal_nfc.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_i2c freertos log hal_i2c_bus)
