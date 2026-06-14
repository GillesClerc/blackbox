idf_component_register(SRCS "hal_box_auth.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash mbedtls log)
