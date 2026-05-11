idf_component_register(SRCS "i2c_bus.c"
                             "lsm6dsotr.c"
                             "as5600.c"
                             "veml7700.c"
                             "mtch2120.c"
                             "mpr121.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c freertos log)
