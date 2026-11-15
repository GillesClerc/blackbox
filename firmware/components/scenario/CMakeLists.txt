idf_component_register(SRCS "scenario_engine.c" "scenario_validate.c" "cJSON.c"
                    INCLUDE_DIRS "include" "."
                    REQUIRES freertos log)

# cJSON_Parse est récursif : 1000 niveaux par défaut déborderaient la pile sur
# un JSON hostile (SD, package). Scénarios et manifests restent sous 10 niveaux.
target_compile_definitions(${COMPONENT_LIB} PUBLIC CJSON_NESTING_LIMIT=32)
