#include "gpio.h"

static void _gpio_assert_config(const GpioConfig* config)
{
    ASSERT(config != NULL);
    ASSERT(config->mode >= GPIO_MODE_INPUT && config->mode <= GPIO_MODE_ANALOG);
    ASSERT(config->pull >= GPIO_PULL_NONE && config->pull <= GPIO_PULL_DOWN);
    ASSERT(config->speed >= GPIO_SPEED_LOW && config->speed <= GPIO_SPEED_VERY_HIGH);
    ASSERT(config->output_type >= GPIO_OUTPUT_PUSH_PULL && config->output_type <= GPIO_OUTPUT_OPEN_DRAIN);
    ASSERT(config->alternate >= GPIO_ALTERNATE_NONE && config->alternate <= GPIO_ALTERNATE_15);
    ASSERT(config->level >= GPIO_LEVEL_LOW && config->level <= GPIO_LEVEL_HIGH);
}

static void _gpio_assert_level(GpioLevel level)
{
    ASSERT(level >= GPIO_LEVEL_LOW && level <= GPIO_LEVEL_HIGH);
}

static GpioConfig _gpio_default_config(void)
{
    GpioConfig config = {
        GPIO_DEFAULT_MODE,
        GPIO_DEFAULT_PULL,
        GPIO_DEFAULT_SPEED,
        GPIO_DEFAULT_OUTPUT_TYPE,
        GPIO_DEFAULT_ALTERNATE,
        GPIO_DEFAULT_LEVEL
    };

    _gpio_assert_config(&config);
    return config;
}

static void _gpio_assert_ops(const GpioOps* ops)
{
    ASSERT(ops != NULL);
    ASSERT(ops->config != NULL);
    ASSERT(ops->write != NULL);
    ASSERT(ops->read != NULL);
}

static void _gpio_assert_ready(const Gpio* self)
{
    ASSERT(self != NULL);
    _gpio_assert_ops(self->ops);
}

static void _gpio_apply_config_checked(Gpio* self)
{
    self->ops->config(self->pin, &self->config);
}

static void _gpio_config_checked(Gpio* self, const GpioConfig* config)
{
    self->config = *config;
    _gpio_apply_config_checked(self);
}

void gpio_init(Gpio* self, const GpioOps* ops, size_t pin)
{
    ASSERT(self != NULL);
    _gpio_assert_ops(ops);

    self->ops = ops;
    self->pin = pin;
    self->config = _gpio_default_config();
    _gpio_apply_config_checked(self);
}

void gpio_config(Gpio* self, const GpioConfig* config)
{
    _gpio_assert_ready(self);
    _gpio_assert_config(config);

    _gpio_config_checked(self, config);
}

void gpio_write(Gpio* self, GpioLevel level)
{
    _gpio_assert_ready(self);
    _gpio_assert_level(level);

    self->config.level = level;
    self->ops->write(self->pin, level);
}

GpioLevel gpio_read(Gpio* self)
{
    GpioLevel level;

    _gpio_assert_ready(self);

    level = self->ops->read(self->pin);
    _gpio_assert_level(level);
    self->config.level = level;

    return level;
}

void gpio_toggle(Gpio* self)
{
    GpioLevel level = gpio_read(self);

    gpio_write(self, level == GPIO_LEVEL_LOW ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

void gpio_deinit(Gpio* self)
{
    GpioConfig config;

    _gpio_assert_ready(self);

    config = _gpio_default_config();
    self->config = config;
    _gpio_apply_config_checked(self);

    self->ops = NULL;
    self->pin = 0U;
}
