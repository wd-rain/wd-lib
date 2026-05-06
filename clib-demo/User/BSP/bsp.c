#include "bsp.h"

Gpio led0;
Gpio led1;
Gpio pe3_gpio;
GpioIsr pe3_isr;
I2c eeprom_i2c;

static void _bsp_init_eeprom_i2c(void)
{
    I2cConfig i2c_config;
    I2cGpioConfig gpio_config;
    GpioConfig pin_config;
    I2cStatus status;

    i2c_config.clock_hz = I2C_DEFAULT_CLOCK_HZ;
    pin_config = bsp_gpio_alternate_config(WD_GPIO_ALTERNATE_4, WD_GPIO_PULL_UP, WD_GPIO_SPEED_VERY_HIGH, WD_GPIO_OUTPUT_OPEN_DRAIN, WD_GPIO_LEVEL_HIGH);

    gpio_config.gpio_ops = bsp_gpio_ops();
    gpio_config.scl_pin = BSP_GPIO_PIN_PB8;
    gpio_config.sda_pin = BSP_GPIO_PIN_PB9;
    gpio_config.scl_config = pin_config;
    gpio_config.sda_config = pin_config;

    status = i2c_init(&eeprom_i2c, bsp_i2c_ops(), WD_BSP_I2C_BUS_1, &i2c_config, &gpio_config);
    ASSERT(status == WD_I2C_STATUS_OK);
}

static void _bsp_init_pe3_isr(void)
{
    GpioConfig pin_config;
    GpioIsrConfig isr_config;

    pin_config = bsp_gpio_input_config(WD_GPIO_PULL_UP);
    gpio_init(&pe3_gpio, bsp_gpio_ops(), BSP_GPIO_PIN_PE3);
    gpio_config(&pe3_gpio, &pin_config);

    isr_config.trigger = WD_GPIO_ISR_FALLING;
    gpio_isr_init(&pe3_isr, &pe3_gpio, bsp_gpio_isr_ops(), &isr_config, NULL);
    bsp_gpio_isr_set_nvic_priority(BSP_GPIO_PIN_PE3, 5U, 0U);
    gpio_isr_enable(&pe3_isr);
}

void bsp_init(void)
{
    gpio_init(&led0, bsp_gpio_ops(), BSP_GPIO_PIN_PF9);
    gpio_init(&led1, bsp_gpio_ops(), BSP_GPIO_PIN_PF10);
    _bsp_init_pe3_isr();
    _bsp_init_eeprom_i2c();
}
