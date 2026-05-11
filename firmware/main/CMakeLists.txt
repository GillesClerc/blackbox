idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "../scenarios/capitaine_verdier.json"
                    REQUIRES display scenario sensors nfc servo storage)
