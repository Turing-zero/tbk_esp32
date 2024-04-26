idf_component_register(SRCS "tbk_nvs.c" "cmd_nvs.c"
                    INCLUDE_DIRS .
                    REQUIRES console nvs_flash)
