idf_component_register(SRCS "mqtt.c" "wifi.c" "DHT.c" "pump.c" "main.c" "soil_sensor.c" 
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_adc 
                    PRIV_REQUIRES esp_wifi nvs_flash esp_event esp_netif
                    PRIV_REQUIRES 
                    PRIV_REQUIRES esp_driver_gpio esp_timer)