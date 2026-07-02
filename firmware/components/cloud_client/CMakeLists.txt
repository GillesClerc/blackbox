# cJSON est vendoré dans le composant scenario (pas de composant "json" en IDF v6).
idf_component_register(SRCS "cloud_client.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_http_client mbedtls scenario esp_timer
                                  esp_app_format nvs_flash fatfs hal_box_auth hal_wifi log)
