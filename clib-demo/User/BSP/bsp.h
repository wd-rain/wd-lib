#ifndef _BSP_H_
#define _BSP_H_

#include "bsp_gpio.h"
#include "bsp_gpio_isr.h"
#include "bsp_i2c.h"

// Interface
extern Gpio led0;
extern Gpio led1;
extern Gpio pe3_gpio;
extern GpioIsr pe3_isr;
extern I2c eeprom_i2c;

void bsp_init(void);

#endif
