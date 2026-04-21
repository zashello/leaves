idf_component_register(SRCS "main.c"
                            "hardware/servo.c"
                            "http/model_api.c"
                  INCLUDE_DIRS "." 
                  REQUIRES esp_driver_ledc esp_driver_gpio app_trace nvs_flash esp_event  esp_http_client lwip esp_netif)