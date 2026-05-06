#include "stm32f4xx_hal.h"
#include "bsp_gpio_isr.h"

static void _bsp_gpio_isr_assert_pin_index(size_t index)
{
    ASSERT(index <= 15U);
}

static size_t _bsp_gpio_isr_pin_index(size_t pin)
{
    size_t index;

    index = bsp_gpio_pin_index(pin);
    _bsp_gpio_isr_assert_pin_index(index);

    return index;
}

static uint32_t _bsp_gpio_isr_line(size_t pin)
{
    return 1UL << _bsp_gpio_isr_pin_index(pin);
}

static IRQn_Type _bsp_gpio_isr_irqn(size_t pin)
{
    size_t index;

    index = _bsp_gpio_isr_pin_index(pin);
    if (index <= 4U)
    {
        IRQn_Type irqns[] = {
            EXTI0_IRQn,
            EXTI1_IRQn,
            EXTI2_IRQn,
            EXTI3_IRQn,
            EXTI4_IRQn
        };

        return irqns[index];
    }

    if (index <= 9U)
    {
        return EXTI9_5_IRQn;
    }

    return EXTI15_10_IRQn;
}

static void _bsp_gpio_isr_config_port_source(size_t pin)
{
    size_t index;
    uint32_t shift;
    uint32_t mask;
    uint32_t value;

    index = _bsp_gpio_isr_pin_index(pin);
    shift = (uint32_t)((index & 3U) * 4U);
    mask = 0xFUL << shift;
    value = ((uint32_t)bsp_gpio_pin_port(pin)) << shift;

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    REG_SET(SYSCFG->EXTICR[index >> 2U], mask, value);
}

static void _bsp_gpio_isr_config_edge(size_t pin, GpioIsrTrigger trigger)
{
    uint32_t line;

    line = _bsp_gpio_isr_line(pin);
    EXTI->RTSR &= ~line;
    EXTI->FTSR &= ~line;

    switch (trigger)
    {
    case WD_GPIO_ISR_RISING:
        EXTI->RTSR |= line;
        break;
    case WD_GPIO_ISR_FALLING:
        EXTI->FTSR |= line;
        break;
    case WD_GPIO_ISR_BOTH:
        EXTI->RTSR |= line;
        EXTI->FTSR |= line;
        break;
    default:
        ASSERT(0);
    }
}

static void _bsp_gpio_isr_config(size_t pin, const GpioIsrConfig *config)
{
    uint32_t line;

    ASSERT(config != NULL);
    ASSERT(config->trigger <= WD_GPIO_ISR_BOTH);

    line = _bsp_gpio_isr_line(pin);
    _bsp_gpio_isr_config_port_source(pin);
    _bsp_gpio_isr_config_edge(pin, config->trigger);
    EXTI->EMR &= ~line;
    EXTI->PR = line;
}

static void _bsp_gpio_isr_enable(size_t pin)
{
    uint32_t line;

    line = _bsp_gpio_isr_line(pin);
    EXTI->PR = line;
    EXTI->IMR |= line;
    HAL_NVIC_EnableIRQ(_bsp_gpio_isr_irqn(pin));
}

static void _bsp_gpio_isr_disable(size_t pin)
{
    EXTI->IMR &= ~_bsp_gpio_isr_line(pin);
}

static bool _bsp_gpio_isr_pending(size_t pin)
{
    uint32_t line;

    line = _bsp_gpio_isr_line(pin);

    return (EXTI->IMR & line) != 0U && (EXTI->PR & line) != 0U;
}

static void _bsp_gpio_isr_ack(size_t pin)
{
    EXTI->PR = _bsp_gpio_isr_line(pin);
}

const GpioIsrOps *bsp_gpio_isr_ops(void)
{
    static const GpioIsrOps ops = {
        _bsp_gpio_isr_config,
        {
            _bsp_gpio_isr_enable,
            _bsp_gpio_isr_disable,
            _bsp_gpio_isr_pending,
            _bsp_gpio_isr_ack
        }
    };

    return &ops;
}

void bsp_gpio_isr_set_nvic_priority(BspGpioPin pin, uint32_t preempt_priority, uint32_t sub_priority)
{
    HAL_NVIC_SetPriority(_bsp_gpio_isr_irqn(pin), preempt_priority, sub_priority);
}
