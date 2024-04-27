idf_component_register(SRCS "tbk_cmd.c" "cmd_system_sleep.c" "cmd_system_common.c"
                    INCLUDE_DIRS "."
                    REQUIRES console fatfs spi_flash)