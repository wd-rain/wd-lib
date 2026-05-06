#ifndef _BSP_GPIO_ISR_H_
#define _BSP_GPIO_ISR_H_

#include <stddef.h>
#include <stdint.h>

// Dependencies
#include "bsp_gpio.h"
#include "platform/gpio/gpio_isr.h"

// Interface
const GpioIsrOps *bsp_gpio_isr_ops(void);
void bsp_gpio_isr_set_nvic_priority(BspGpioPin pin, uint32_t preempt_priority, uint32_t sub_priority);

#endif
