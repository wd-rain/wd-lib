#ifndef _GPIO_ISR_H_
#define _GPIO_ISR_H_

#include <stddef.h>

// 依赖
#include "gpio.h"
#include "../isr/isr.h"

// 类型定义
typedef enum gpio_isr_trigger_t
{
    WD_GPIO_ISR_RISING = 0,
    WD_GPIO_ISR_FALLING,
    WD_GPIO_ISR_BOTH
} GpioIsrTrigger;

typedef struct gpio_isr_config_t
{
    GpioIsrTrigger trigger;
} GpioIsrConfig;

typedef void (*gpio_isr_config_fn)(size_t pin, const GpioIsrConfig *config);

typedef struct gpio_isr_ops_t
{
    gpio_isr_config_fn config;
    IsrOps isr;
} GpioIsrOps;

typedef struct gpio_isr_t GpioIsr;

typedef void (*gpio_isr_callback_fn)(GpioIsr *self);

struct gpio_isr_t
{
    Isr isr;
    Gpio *gpio;
    const GpioIsrOps *ops;
    gpio_isr_callback_fn callback;
};

// 接口
void gpio_isr_init(GpioIsr *self, Gpio *gpio, const GpioIsrOps *ops, const GpioIsrConfig *config, gpio_isr_callback_fn callback);
void gpio_isr_config(GpioIsr *self, const GpioIsrConfig *config);
Isr *gpio_isr_base(GpioIsr *self);
void gpio_isr_set_callback(GpioIsr *self, gpio_isr_callback_fn callback);
void gpio_isr_enable(GpioIsr *self);
void gpio_isr_disable(GpioIsr *self);
void gpio_isr_deinit(GpioIsr *self);

#endif
