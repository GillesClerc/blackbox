# ── Assets binaires (MP3, images) ────────────────────────────────────────────
# Déposer le fichier dans firmware/assets/audio/ambient.mp3 pour l'activer.
# Le build le détecte automatiquement. Sans fichier → fallback tons synthétiques.
set(EMBED_BIN "")
set(AMBIENT_MP3 "${CMAKE_CURRENT_SOURCE_DIR}/../assets/audio/ambient.mp3")
if(EXISTS "${AMBIENT_MP3}")
    list(APPEND EMBED_BIN "../assets/audio/ambient.mp3")
    message(STATUS "[EscapeBox] ambient.mp3 détecté → sera embarqué dans le firmware")
else()
    message(STATUS "[EscapeBox] ambient.mp3 absent → ambiance par synthèse de tons")
endif()

# Pour tester le hardware : remplacer "main.c" par "test.c" dans SRCS
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "../scenarios/capitaine_verdier.json"
                    EMBED_FILES    ${EMBED_BIN}
                    REQUIRES display sensors audio leds scenario freertos log esp_driver_i2c)
