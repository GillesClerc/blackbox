idf_component_register(SRCS "storage.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_spi fatfs vfs log)
