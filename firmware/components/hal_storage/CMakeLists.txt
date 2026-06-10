idf_component_register(SRCS "hal_storage.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_spi fatfs vfs log)
