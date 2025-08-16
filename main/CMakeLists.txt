idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES rk_wifi rk_led rk_ota nvs_flash)
