idf_component_register(SRCS "ui_face.c" "eyes_anim.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "." "data"
                    REQUIRES hal_display freertos log esp_timer esp_hw_support)
