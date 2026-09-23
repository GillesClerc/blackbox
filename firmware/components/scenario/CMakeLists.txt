idf_component_register(SRCS "scenario_engine.c" "scenario_validate.c" "cJSON.c"
                    INCLUDE_DIRS "include" "."
                    REQUIRES freertos log)
