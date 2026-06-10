# i2c_bus.h (bus owner) est le seul header public qui expose driver/i2c_master.h
# → esp_driver_i2c doit rester public tant que i2c_bus vit dans ce composant.
idf_component_register(SRCS "i2c_bus.c"
                             "lsm6dsotr.c"
                             "veml7700.c"
                             "mtch2120.c"
                             "mpr121.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c
                    PRIV_REQUIRES freertos log)
