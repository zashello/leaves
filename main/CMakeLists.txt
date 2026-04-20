idf_component_register(SRCS "main.c"
                            "hardware/servo.c"
                  INCLUDE_DIRS "." 
                  REQUIRES esp_driver_ledc esp_driver_gpio app_trace)