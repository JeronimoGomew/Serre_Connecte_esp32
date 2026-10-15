#include "soil_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "pump.h"
#include "DHT.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "mqtt.h"

static const char *TAG = "main";

void app_main(void){
  esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG,"ESP_WIFI_MODE_STA");
    wifi_init_sta();


  mqtt_app_start();
  QueueHandle_t queue_moisture = xQueueCreate(5,sizeof(int));
  QueueHandle_t queue_dht = xQueueCreate(5,5*sizeof(uint8_t));

  xTaskCreate(DHT_Task,"DHT_Task",4096,queue_dht,5,NULL);
  xTaskCreate(soil_sensor_task,"soil_sensor_task",4096,queue_moisture,5,NULL);
  xTaskCreate(pump_task,"pump_task",2048,queue_moisture,5,NULL);
}