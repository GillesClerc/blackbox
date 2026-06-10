idf_component_register(SRCS "hal_touch.c" "mpr121.c" "mtch2120.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_i2c hal_i2c_bus freertos log)
