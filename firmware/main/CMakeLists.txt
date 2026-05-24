# Pour tester le hardware : remplacer "main.c" par "test.c" dans SRCS

idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "../scenarios/capitaine_verdier.json"
                    REQUIRES display sensors audio leds scenario freertos log esp_driver_i2c)
