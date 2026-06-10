# hal_i2c_bus_priv.h expose driver/i2c_master.h aux autres composants hal_*
# → esp_driver_i2c doit rester en REQUIRES public.
idf_component_register(SRCS "hal_i2c_bus.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c
                    PRIV_REQUIRES freertos log)
