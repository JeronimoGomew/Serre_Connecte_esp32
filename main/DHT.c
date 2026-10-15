#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "mqtt.h"


#define DHT_GPIO GPIO_NUM_18

void set_DHT(void){
    gpio_reset_pin(DHT_GPIO);
    ESP_ERROR_CHECK(gpio_set_direction(DHT_GPIO,GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(DHT_GPIO,0));
    esp_rom_delay_us(2000);
    ESP_ERROR_CHECK(gpio_set_direction(DHT_GPIO,GPIO_MODE_INPUT));
    while(gpio_get_level(DHT_GPIO)==1){};
    while(gpio_get_level(DHT_GPIO)==0){};
    while(gpio_get_level(DHT_GPIO)==1){};

}

void bits_to_bytes(int64_t bits[40],uint8_t bytes[5]){

    for (int i = 0; i < 40; i++) {
        bytes[i / 8] = (bytes[i / 8] << 1) | bits[i];
    }

}

bool checksum(uint8_t bytes[5]){
    uint8_t sum = 0;
    for(int i=0;i<4;i++){
        sum += bytes[i];
    }
    if (sum == bytes[4]){return true;}
    else{return false;}
}

void DHT_Task(void *pvParameter){
    QueueHandle_t queue_dht = pvParameter;

    while(1){
        set_DHT();

        int64_t message[40];
        int threshold = 50;
        static portMUX_TYPE dht_mux = portMUX_INITIALIZER_UNLOCKED;

        taskENTER_CRITICAL(&dht_mux);
        for(int i = 0;i<40;i++){
            
            while(gpio_get_level(DHT_GPIO)==0){};
            int64_t beg = esp_timer_get_time();
            while(gpio_get_level(DHT_GPIO)==1){};
            int64_t end = esp_timer_get_time();
            int64_t time = end - beg;
            if (time<threshold){ message[i] = 0;}
            else{message[i] = 1;}

        }
        taskEXIT_CRITICAL(&dht_mux);

        uint8_t bytes[5] = {0};
        bits_to_bytes(message,bytes);
        if (checksum(bytes)){
            ESP_LOGI("DHT22", "OK: %02X %02X %02X %02X %02X", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
            xQueueSend(queue_dht,bytes,0);
        }
        else{
            ESP_LOGW("DHT22", "checksum failed");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        float humidity = ((bytes[0]<<8)|bytes[1])/10.0;
        float temperature = ((bytes[2]<<8)|bytes[3])/10.0;
        mqtt_publish_dht(temperature, humidity);

        vTaskDelay(pdMS_TO_TICKS(3000));   
    }
}