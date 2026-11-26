#ifndef OLED_H
#define OLED_H

#include "driver/i2c_master.h"

/* SSD1306 128x64 OLED on the I2C bus. */
void oled_init(i2c_master_bus_handle_t bus);

/* Called by the sensor tasks to update what the screen shows. */
void oled_set_temperature(float temperature);
void oled_set_soil(int percent);

/* Redraws the screen periodically. */
void oled_task(void *pvParameter);

#endif
