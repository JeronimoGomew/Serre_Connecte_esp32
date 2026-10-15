#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "soil_sensor.h"
#include "mqtt.h"



const static char *TAG = "soil_sensor";
#define SOIL_SENSOR_CHANNEL ADC_CHANNEL_6 //GPIO 34
#define DRY 3300
#define WET 1250


static int raw_to_percent(int raw){
    if(raw > DRY){ raw = DRY;}
    if(raw < WET){ raw = WET;}

    int percent = 100* (DRY - raw)/(DRY - WET);

    return percent;
}



void soil_sensor_task(void *pvParameter){
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SOIL_SENSOR_CHANNEL, &config));

  
   
    int raw_val = 0;

    while(1){
        QueueHandle_t queue_moisture =  pvParameter;

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, SOIL_SENSOR_CHANNEL, &raw_val));
        int percent = raw_to_percent(raw_val);
        ESP_LOGI(TAG, "Raw Data: %d, percentage: %d%%",  raw_val,percent);
        
        xQueueSend(queue_moisture, &percent,0);
        mqtt_publish_moisture(percent);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
   
}