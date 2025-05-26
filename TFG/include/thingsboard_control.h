#ifndef THINGSBOARD_CONTROL_H
#define THINGSBOARD_CONTROL_H
void send_data_to_thingsboard_mqtt(uint16_t values[18], int temperature);
void mqtt_app_start();
#endif