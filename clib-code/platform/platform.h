#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifndef PLATFORM_USE_GPIO
#define PLATFORM_USE_GPIO 1
#endif

#ifndef PLATFORM_USE_I2C
#define PLATFORM_USE_I2C 1
#endif

#ifndef PLATFORM_USE_GPIO_ISR
#define PLATFORM_USE_GPIO_ISR PLATFORM_USE_GPIO
#endif

#ifndef PLATFORM_USE_ISR
#define PLATFORM_USE_ISR PLATFORM_USE_GPIO_ISR
#endif

#if PLATFORM_USE_GPIO != 0 && PLATFORM_USE_GPIO != 1
#error "PLATFORM_USE_GPIO must be 0 or 1"
#endif

#if PLATFORM_USE_I2C != 0 && PLATFORM_USE_I2C != 1
#error "PLATFORM_USE_I2C must be 0 or 1"
#endif

#if PLATFORM_USE_GPIO_ISR != 0 && PLATFORM_USE_GPIO_ISR != 1
#error "PLATFORM_USE_GPIO_ISR must be 0 or 1"
#endif

#if PLATFORM_USE_ISR != 0 && PLATFORM_USE_ISR != 1
#error "PLATFORM_USE_ISR must be 0 or 1"
#endif

#if PLATFORM_USE_I2C && !PLATFORM_USE_GPIO
#error "PLATFORM_USE_I2C requires PLATFORM_USE_GPIO"
#endif

#if PLATFORM_USE_GPIO_ISR && !PLATFORM_USE_GPIO
#error "PLATFORM_USE_GPIO_ISR requires PLATFORM_USE_GPIO"
#endif

#if PLATFORM_USE_GPIO_ISR && !PLATFORM_USE_ISR
#error "PLATFORM_USE_GPIO_ISR requires PLATFORM_USE_ISR"
#endif

#if PLATFORM_USE_ISR
#include "isr/isr.h"
#endif

#if PLATFORM_USE_GPIO
#include "gpio/gpio.h"
#endif

#if PLATFORM_USE_GPIO_ISR
#include "gpio/gpio_isr.h"
#endif

#if PLATFORM_USE_I2C
#include "i2c/i2c.h"
#endif

#endif
