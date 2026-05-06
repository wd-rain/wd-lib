#include "gpio_isr.h"

static void _gpio_isr_assert_config(const GpioIsrConfig *config)
{
    ASSERT(config != NULL);
    ASSERT(config->trigger <= WD_GPIO_ISR_BOTH);
}

static void _gpio_isr_assert_ops(const GpioIsrOps *ops)
{
    ASSERT(ops != NULL);
    ASSERT(ops->config != NULL);
    ASSERT(ops->isr.pending != NULL);
}

static void _gpio_isr_assert_ready(const GpioIsr *self)
{
    ASSERT(self != NULL);
    ASSERT(self->gpio != NULL);
    _gpio_isr_assert_ops(self->ops);
}

static void _gpio_isr_clear(GpioIsr *self)
{
    self->isr.ops = NULL;
    self->isr.source = 0U;
    self->isr.action = NULL;
    self->gpio = NULL;
    self->ops = NULL;
    self->callback = NULL;
}

static void _gpio_isr_config_checked(GpioIsr *self, const GpioIsrConfig *config)
{
    self->ops->config(self->gpio->pin, config);
}

static void _gpio_isr_action(Isr *isr)
{
    GpioIsr *self;

    ASSERT(isr != NULL);

    self = CONTAINER_OF(isr, GpioIsr, isr);

    if (self->callback != NULL)
    {
        self->callback(self);
    }
}

void gpio_isr_init(GpioIsr *self, Gpio *gpio, const GpioIsrOps *ops, const GpioIsrConfig *config, gpio_isr_callback_fn callback)
{
    ASSERT(self != NULL);
    ASSERT(gpio != NULL);
    _gpio_isr_assert_ops(ops);
    _gpio_isr_assert_config(config);

    _gpio_isr_clear(self);
    self->gpio = gpio;
    self->ops = ops;
    self->callback = callback;

    _gpio_isr_config_checked(self, config);
    isr_init(&self->isr, &ops->isr, gpio->pin, _gpio_isr_action);
}

void gpio_isr_config(GpioIsr *self, const GpioIsrConfig *config)
{
    _gpio_isr_assert_ready(self);
    _gpio_isr_assert_config(config);

    _gpio_isr_config_checked(self, config);
}

Isr *gpio_isr_base(GpioIsr *self)
{
    _gpio_isr_assert_ready(self);

    return &self->isr;
}

void gpio_isr_set_callback(GpioIsr *self, gpio_isr_callback_fn callback)
{
    _gpio_isr_assert_ready(self);

    self->callback = callback;
}

void gpio_isr_enable(GpioIsr *self)
{
    _gpio_isr_assert_ready(self);

    isr_enable(&self->isr);
}

void gpio_isr_disable(GpioIsr *self)
{
    _gpio_isr_assert_ready(self);

    isr_disable(&self->isr);
}

void gpio_isr_deinit(GpioIsr *self)
{
    _gpio_isr_assert_ready(self);

    isr_deinit(&self->isr);
    _gpio_isr_clear(self);
}
