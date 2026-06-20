idf_component_register(SRCS "hal_wifi.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash log)
