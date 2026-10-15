#ifndef MQTT_H
#define MQTT_H

void mqtt_app_start(void);
void mqtt_publish_moisture(int percent);
void mqtt_publish_dht(float temperature, float humidity);

#endif