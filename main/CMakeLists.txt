idf_component_register(SRCS "main.c"
                            "hardware/servo.c"
                            "hardware/button.c"
                            "http/model_api.c"
                            "wifi/wifi_connect.c"
                            "wifi/provision/provision.c"
                            "wifi/provision/http_server.c"
                            "wifi/provision/captive_portal.c"
                            "config/nvs_config.c"
                            "hardware/sensor.c"
                  INCLUDE_DIRS "."
                   REQUIRES esp_driver_ledc esp_driver_gpio app_trace nvs_flash esp_event esp_http_client esp_http_server lwip esp_netif
                   esp_wifi driver)
