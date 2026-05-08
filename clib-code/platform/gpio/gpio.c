#include "gpio.h"

static void _gpio_assert_config(const GpioConfig *config)
{
    WD_ASSERT(config != NULL);
    WD_ASSERT(config->mode <= WD_GPIO_MODE_ANALOG);
    WD_ASSERT(config->pull <= WD_GPIO_PULL_DOWN);
    WD_ASSERT(config->speed <= WD_GPIO_SPEED_VERY_HIGH);
    WD_ASSERT(config->output_type <= WD_GPIO_OUTPUT_OPEN_DRAIN);
    WD_ASSERT(config->alternate <= WD_GPIO_ALTERNATE_15);
    WD_ASSERT(config->level <= WD_GPIO_LEVEL_HIGH);
}

static void _gpio_assert_level(GpioLevel level)
{
    WD_ASSERT(level <= WD_GPIO_LEVEL_HIGH);
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

    return config;
}

static void _gpio_assert_ops(const GpioOps *ops)
{
    WD_ASSERT(ops != NULL);
    WD_ASSERT(ops->config != NULL);
    WD_ASSERT(ops->write != NULL);
    WD_ASSERT(ops->read != NULL);
}

static void _gpio_assert_ready(const Gpio *self)
{
    WD_ASSERT(self != NULL);
    _gpio_assert_ops(self->ops);
}

static void _gpio_apply_config_checked(Gpio *self)
{
    self->ops->config(self->pin, &self->config);
}

static void _gpio_config_checked(Gpio *self, const GpioConfig *config)
{
    self->config = *config;
    _gpio_apply_config_checked(self);
}

void gpio_init(Gpio *self, const GpioOps *ops, size_t pin)
{
    WD_ASSERT(self != NULL);
    _gpio_assert_ops(ops);

    self->ops = ops;
    self->pin = pin;
    self->config = _gpio_default_config();
    _gpio_apply_config_checked(self);
}

void gpio_config(Gpio *self, const GpioConfig *config)
{
    _gpio_assert_ready(self);
    _gpio_assert_config(config);

    _gpio_config_checked(self, config);
}

const GpioConfig *gpio_get_config(const Gpio *self)
{
    _gpio_assert_ready(self);

    return &self->config;
}

void gpio_write(Gpio *self, GpioLevel level)
{
    _gpio_assert_ready(self);
    _gpio_assert_level(level);

    self->config.level = level;
    self->ops->write(self->pin, level);
}

GpioLevel gpio_read(Gpio *self)
{
    GpioLevel level;

    _gpio_assert_ready(self);

    level = self->ops->read(self->pin);
    self->config.level = level;

    return level;
}

void gpio_toggle(Gpio *self)
{
    GpioLevel level;

    _gpio_assert_ready(self);

    level = gpio_read(self);
    gpio_write(self, level == WD_GPIO_LEVEL_LOW ? WD_GPIO_LEVEL_HIGH : WD_GPIO_LEVEL_LOW);
}

void gpio_deinit(Gpio *self)
{
    GpioConfig config;

    _gpio_assert_ready(self);

    config = _gpio_default_config();
    self->config = config;
    _gpio_apply_config_checked(self);

    self->ops = NULL;
    self->pin = 0U;
}
