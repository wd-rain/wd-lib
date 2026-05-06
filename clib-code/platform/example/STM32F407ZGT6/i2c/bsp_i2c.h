#ifndef _BSP_I2C_H_
#define _BSP_I2C_H_

#include <stddef.h>

// Dependencies
#include "bsp_gpio.h"
#include "platform/i2c/i2c.h"

// Config
#define BSP_I2C1_SCL_PIN BSP_GPIO_PIN_PB6
#define BSP_I2C1_SDA_PIN BSP_GPIO_PIN_PB7
#define BSP_I2C2_SCL_PIN BSP_GPIO_PIN_PB10
#define BSP_I2C2_SDA_PIN BSP_GPIO_PIN_PB11
#define BSP_I2C3_SCL_PIN BSP_GPIO_PIN_PA8
#define BSP_I2C3_SDA_PIN BSP_GPIO_PIN_PC9

// Types
typedef enum bsp_i2c_bus_t
{
    WD_BSP_I2C_BUS_1 = 0,
    WD_BSP_I2C_BUS_2,
    WD_BSP_I2C_BUS_3,
    WD_BSP_I2C_BUS_COUNT
} BspI2cBus;

// Interface
const I2cOps *bsp_i2c_ops(void);
void bsp_i2c_default_gpio_config(size_t bus, I2cGpioConfig *config);

#endif
