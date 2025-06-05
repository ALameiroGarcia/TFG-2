#pragma once

void oled_init(void);
void oled_hud_task(void *pvParameters);
void hud_display_wifi(bool connected);
void hud_display_message(const char* msg, uint8_t page);
void hud_display_sensor_status(bool sensor_ok);
