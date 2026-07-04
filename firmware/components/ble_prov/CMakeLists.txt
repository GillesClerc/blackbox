idf_component_register(SRCS "ble_prov.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES bt esp_timer freertos log hal_box_auth hal_wifi)
