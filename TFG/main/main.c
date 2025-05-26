#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "as7265x.h"



void app_main() {
    // Inicializa almacenamiento no volátil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Iniciar ESP32 en modo AP y levantar el servidor web
    gpio_init();
    start_wifi_ap();
    try_auto_connect();

    xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}