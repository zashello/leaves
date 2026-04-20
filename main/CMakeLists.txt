idf_component_register(SRCS "main.c"
                            "pwm.c"
                  INCLUDE_DIRS "."
                  REQUIRES esp_driver_ledc)