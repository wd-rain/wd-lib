#include "app.h"

#include <stdbool.h>
#include <stdint.h>

#include "main.h"
#include "bsp.h"

#define AT24C02_I2C_ADDRESS 0x50U
#define AT24C02_START_ADDRESS 0x00U
#define AT24C02_READ_SIZE 21U
#define AT24C02_READ_TIMEOUT_MS 1000U

bool flag = false;
uint8_t eeprom_data[AT24C02_READ_SIZE];

void gpio_callback(GpioIsr *self)
{
    gpio_toggle(&led1);
}

void app_init(void)
{
    GpioConfig config;
    I2cStatus status;

    config = bsp_gpio_output_config(WD_GPIO_OUTPUT_PUSH_PULL, WD_GPIO_SPEED_LOW, WD_GPIO_LEVEL_LOW);
    gpio_config(&led0, &config);
    gpio_config(&led1, &config);

    status = i2c_mem_read(&eeprom_i2c, AT24C02_I2C_ADDRESS, AT24C02_START_ADDRESS, WD_I2C_MEM_ADDRESS_SIZE_8BIT, eeprom_data, AT24C02_READ_SIZE, AT24C02_READ_TIMEOUT_MS);
    flag = status == WD_I2C_STATUS_OK;
    gpio_isr_set_callback(&pe3_isr, gpio_callback);
}

void app_task(void)
{
    if (flag)
    {
        gpio_write(&led0, WD_GPIO_LEVEL_HIGH);
        HAL_Delay(500);
        gpio_write(&led0, WD_GPIO_LEVEL_LOW);
        HAL_Delay(500);
    }
    else
    {
        gpio_write(&led1, WD_GPIO_LEVEL_LOW);
        HAL_Delay(500);
        gpio_write(&led1, WD_GPIO_LEVEL_HIGH);
        HAL_Delay(500);
    }
}
