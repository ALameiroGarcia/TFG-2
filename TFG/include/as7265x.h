#ifndef AS7265X_H
#define AS7265X_H

#include "driver/i2c.h"
#include "esp_err.h"

// Dirección I2C del AS7265x
#define AS7265X_I2C_ADDR 0x49

// Registros importantes
#define AS7265X_SLAVE_STATUS_REG  0x00
#define AS7265X_SLAVE_WRITE_REG   0x01
#define AS7265X_SLAVE_READ_REG    0x02
#define AS7265X_DATA_START        0x08

// Bit de estado en AS7265X_SLAVE_STATUS_REG
#define TX_VALID 0x01 // Indica si el dato está listo

// Funciones del driver
void as7265x_init();
void gpio_init();
void sensor_task(void *pvParameter);
#endif // AS7265X_H
