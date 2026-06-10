idf_component_register(SRCS "hal_imu.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_i2c hal_i2c_bus freertos log)
