# ── Assets binaires (MP3, images) ────────────────────────────────────────────
# Déposer le fichier dans firmware/assets/audio/ambient.mp3 pour l'activer.
# Le build le détecte automatiquement. Sans fichier → fallback tons synthétiques.
set(EMBED_BIN "")
set(COMPILE_DEFS "")
set(AMBIENT_MP3 "${CMAKE_CURRENT_SOURCE_DIR}/../assets/audio/ambient.mp3")
if(EXISTS "${AMBIENT_MP3}")
    list(APPEND EMBED_BIN "../assets/audio/ambient.mp3")
    list(APPEND COMPILE_DEFS "HAS_AMBIENT_MP3=1")
    message(STATUS "[EscapeBox] ambient.mp3 détecté → sera embarqué dans le firmware")
else()
    message(STATUS "[EscapeBox] ambient.mp3 absent → ambiance par synthèse de tons")
endif()

idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "../scenarios/capitaine_verdier.json"
                    EMBED_FILES    ${EMBED_BIN}
                    REQUIRES hal_display ui_manager hal_i2c_bus hal_touch hal_audio hal_leds hal_storage scenario config_manager hal_box_auth hal_wifi cloud_client freertos log esp_timer)

if(COMPILE_DEFS)
    target_compile_definitions(__idf_main PRIVATE ${COMPILE_DEFS})
endif()
