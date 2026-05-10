idf_component_register(SRCS "scenario_engine.c" "cJSON.c"
                    INCLUDE_DIRS "include" "."
                    REQUIRES freertos log)
