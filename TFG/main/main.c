#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "as7265x.h"
#include "oled.h"

#define I2C_MASTER_SCL_IO 22          // GPIO para SCL
#define I2C_MASTER_SDA_IO 21          // GPIO para SDA
#define I2C_MASTER_NUM I2C_NUM_0      // Puerto I2C
#define I2C_MASTER_FREQ_HZ 100000     // Frecuencia I2C
void i2c_master_init();
void app_main() {
    // Inicializa almacenamiento no volátil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    //Inicialización de los componentes I2C

    gpio_init();

    i2c_master_init();

    as7265x_init();

    xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));
    
    oled_init();
    xTaskCreate(oled_hud_task, "oled_hud_task", 2048, NULL, 5, NULL);
    //Intenta conectar a Wi-Fi y si no, abre AP

    try_auto_connect();
}

// Función para realizar un escaneo I2C
void i2c_scanner() {
    printf("Escaneando el bus I2C...\n");
    for (int addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            printf("Dispositivo encontrado en dirección: 0x%02X\n", addr);
        }
    }
    printf("Escaneo finalizado.\n");
}

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

    printf("Escaneando dispositivos I2C...\n");
    i2c_scanner();
}