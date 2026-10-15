#include "driver/gpio.h"
#include "pump.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h" 

#define GPIO_PUMP GPIO_NUM_33
#define THRESHOLD_L 10
#define THRESHOLD_H 25

void pump_init(void){
    gpio_reset_pin(GPIO_PUMP);
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_PUMP,GPIO_MODE_OUTPUT));
}

void pump_set(bool on){ 
    ESP_ERROR_CHECK(gpio_set_level(GPIO_PUMP, on?1:0));
}

void pump_task(void *pvParameter){
    QueueHandle_t queue_moisture = pvParameter;

    pump_init();

    static bool pump_on = false;
    int percent;
    while(1){
    if(xQueueReceive(queue_moisture,&percent,portMAX_DELAY)){
        if (!pump_on && percent<THRESHOLD_L){
            pump_on = true;
        }else if (pump_on && percent>THRESHOLD_H){
            pump_on = false;
        }
        pump_set(pump_on);
    }
    }
}