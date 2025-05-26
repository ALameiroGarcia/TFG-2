#include <stdio.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "thingsboard_control.h"


#define I2C_MASTER_SCL_IO 22          // GPIO para SCL
#define I2C_MASTER_SDA_IO 21          // GPIO para SDA
#define I2C_MASTER_NUM I2C_NUM_0      // Puerto I2C
#define I2C_MASTER_FREQ_HZ 100000     // Frecuencia I2C
#define AS7263_ADDR 0x49              // Dirección I2C del AS7263

// Registros del AS7263
#define I2C_AS72XX_SLAVE_STATUS_REG 0x00
#define I2C_AS72XX_SLAVE_WRITE_REG  0x01
#define I2C_AS72XX_SLAVE_READ_REG   0x02

// Registro para la temperatura
#define VIRTUAL_REG_DEVICE_TEMP 0x06
#define CONFIG_REG 0x04  // Registro de configuración

// Registro para seleccionar el sensor activo
#define DEV_SEL_REG 0x4F

// Función para leer un registro virtual
typedef enum {
    SENSOR_1 = 0x00,  // RSTUVW
    SENSOR_2 = 0x01,  // GHIJKL
    SENSOR_3 = 0x02   // ABCDEF
} sensor_t;

// Función para leer un registro de un dispositivo I2C
esp_err_t i2c_master_read_slave_reg(uint8_t reg_addr, uint8_t *data) {
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AS7263_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AS7263_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Función para escribir en un registro de un dispositivo I2C
esp_err_t i2c_master_write_slave_reg(uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AS7263_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Escritura en un registro virtual
void write_virtual_register(uint8_t reg, uint8_t value) {
    uint8_t status;
    do {
        i2c_master_read_slave_reg(I2C_AS72XX_SLAVE_STATUS_REG, &status);
    } while (status & 0x02); // Espera hasta que TX_VALID sea 0

    // Escribe la dirección del registro
    uint8_t reg_with_write = reg | 0x80;
    i2c_master_write_slave_reg(I2C_AS72XX_SLAVE_WRITE_REG, reg_with_write);

    do {
        i2c_master_read_slave_reg(I2C_AS72XX_SLAVE_STATUS_REG, &status);
    } while (status & 0x02); // Espera hasta que TX_VALID sea 0

    // Escribe el valor
    i2c_master_write_slave_reg(I2C_AS72XX_SLAVE_WRITE_REG, value);
}

// Lectura de un registro virtual
uint8_t read_virtual_register(uint8_t reg) {
    uint8_t status, data;

    do {
        i2c_master_read_slave_reg(I2C_AS72XX_SLAVE_STATUS_REG, &status);
    } while (status & 0x02); // Espera hasta que TX_VALID sea 0

    // Escribe la dirección del registro
    i2c_master_write_slave_reg(I2C_AS72XX_SLAVE_WRITE_REG, reg);

    do {
        i2c_master_read_slave_reg(I2C_AS72XX_SLAVE_STATUS_REG, &status);
    } while (!(status & 0x01)); // Espera hasta que RX_VALID sea 1

    // Lee el dato
    i2c_master_read_slave_reg(I2C_AS72XX_SLAVE_READ_REG, &data);

    return data;
}

// Función para leer los valores crudos de los canales
uint16_t read_raw_value(uint8_t high_reg, uint8_t low_reg) {
    uint8_t high_byte = read_virtual_register(high_reg);  // Leer el byte alto
    uint8_t low_byte = read_virtual_register(low_reg);    // Leer el byte bajo
    return (high_byte << 8) | low_byte;                    // Combinar ambos bytes
}

// Función para seleccionar el sensor activo
void select_sensor(sensor_t sensor) {
    write_virtual_register(DEV_SEL_REG, sensor);
    vTaskDelay(pdMS_TO_TICKS(10));  // Pequeña espera para que el cambio tenga efecto
}

// Función para leer los valores crudos de los canales para un sensor específico
void read_sensor_values(sensor_t sensor,uint16_t *values) {
    select_sensor(sensor);
    vTaskDelay(pdMS_TO_TICKS(10)); // Esperar para estabilizar datos

    for (int i = 0; i < 6; i++) {
        values[i] = read_raw_value(0x08 + (i * 2), 0x09 + (i * 2));
    }

    char labels[3][6] = { "RSTUVW", "GHIJKL", "ABCDEF" };
    printf("%c: %u, %c: %u, %c: %u, %c: %u, %c: %u, %c: %u\n",
        labels[sensor][0], values[0],
        labels[sensor][1], values[1],
        labels[sensor][2], values[2],
        labels[sensor][3], values[3],
        labels[sensor][4], values[4],
        labels[sensor][5], values[5]);
}

// Función para leer la temperatura
int read_temperature() {
    uint8_t temperature = read_virtual_register(VIRTUAL_REG_DEVICE_TEMP);
    printf("Temperatura del sensor: %d °C\n", temperature);
    return temperature;
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


void as7265x_init() {
    printf("Inicializando bus I2C...\n");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

    printf("Escaneando dispositivos I2C...\n");
    i2c_scanner();

    // Leer el ID del sensor para verificar la comunicación
    uint8_t device_id = read_virtual_register(0x00);
    printf("ID del dispositivo AS7265X: 0x%02X\n", device_id);
    // Leer las versiones del hardware y firmware
    uint8_t hw_version = read_virtual_register(0x01);
    uint8_t fw_version = read_virtual_register(0x02);
    printf("HW Version: 0x%02X\n", hw_version);
    printf("FW Version: 0x%02X\n", fw_version);

    // Configurar el sensor para medir en modo continuo

    write_virtual_register(0x04, 0x28);  //gain x16 modo: 2 6 canales 
    write_virtual_register(0x05, 0x3B);  //Tint: 165ms

    printf("Configuración del sensor completada.\n");
}

#define LED_GPIO GPIO_NUM_2
void gpio_init() {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}
void sensor_task(void *pvParameter) {
    printf("Iniciando sensor AS7265X...\n");

    // Iniciar el dispositivo
    as7265x_init();
    
    // Leer datos del sensor
    while (1) {
        // Leer los valores crudos de los canales
        uint16_t values[18];  // 6 valores por cada uno de los 3 sensores

        for (int i=0; i<3;i++){
            read_sensor_values((sensor_t)i,&values[i*6]);
        }
        // Leer la temperatura
        int temperature = read_temperature();
        uint8_t tint = read_virtual_register(0x05);
        printf("Tint: %d\n", tint);
        uint8_t gain2 = read_virtual_register(0x04);
        printf("Gain 2: %d\n", gain2);

        send_data_to_thingsboard_mqtt(values, temperature); // Enviar datos a ThingsBoard
        // Esperar un segundo antes de la siguiente medición
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 segundo de espera
    }
}
