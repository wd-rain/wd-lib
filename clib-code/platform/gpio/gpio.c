#include "gpio.h"

static int _gpio_level_valid(GpioLevel level)
{
    return level == GPIO_LEVEL_LOW || level == GPIO_LEVEL_HIGH;
}

static int _gpio_mode_valid(GpioMode mode)
{
    return mode == GPIO_MODE_INPUT ||
           mode == GPIO_MODE_OUTPUT ||
           mode == GPIO_MODE_ALTERNATE ||
           mode == GPIO_MODE_ANALOG;
}

static int _gpio_pull_valid(GpioPull pull)
{
    return pull == GPIO_PULL_NONE ||
           pull == GPIO_PULL_UP ||
           pull == GPIO_PULL_DOWN;
}

static int _gpio_speed_valid(GpioSpeed speed)
{
    return speed == GPIO_SPEED_LOW ||
           speed == GPIO_SPEED_MEDIUM ||
           speed == GPIO_SPEED_HIGH ||
           speed == GPIO_SPEED_VERY_HIGH;
}

static int _gpio_output_type_valid(GpioOutputType output_type)
{
    return output_type == GPIO_OUTPUT_PUSH_PULL ||
           output_type == GPIO_OUTPUT_OPEN_DRAIN;
}

static int _gpio_alternate_valid(GpioAlternate alternate)
{
    return alternate >= GPIO_ALTERNATE_NONE && alternate <= GPIO_ALTERNATE_15;
}

static void _gpio_assert_config(const GpioConfig* config)
{
    ASSERT(config != NULL);
    ASSERT(_gpio_mode_valid(config->mode));
    ASSERT(_gpio_pull_valid(config->pull));
    ASSERT(_gpio_speed_valid(config->speed));
    ASSERT(_gpio_output_type_valid(config->output_type));
    ASSERT(_gpio_alternate_valid(config->alternate));
    ASSERT(_gpio_level_valid(config->level));
}

static GpioConfig _gpio_default_config(void)
{
    GpioConfig config;

    config.mode = GPIO_DEFAULT_MODE;
    config.pull = GPIO_DEFAULT_PULL;
    config.speed = GPIO_DEFAULT_SPEED;
    config.output_type = GPIO_DEFAULT_OUTPUT_TYPE;
    config.alternate = GPIO_DEFAULT_ALTERNATE;
    config.level = GPIO_DEFAULT_LEVEL;

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
    ASSERT(self->ops != NULL);
}

static void _gpio_apply_config(Gpio* self)
{
    _gpio_assert_ready(self);
    ASSERT(self->ops->config != NULL);
    _gpio_assert_config(&self->config);

    self->ops->config(self->pin, &self->config);
}

void gpio_init(Gpio* self, const GpioOps* ops, size_t pin)
{
    ASSERT(self != NULL);
    _gpio_assert_ops(ops);

    self->ops = ops;
    self->pin = pin;
    self->config = _gpio_default_config();
    _gpio_apply_config(self);
}

void gpio_config(Gpio* self, const GpioConfig* config)
{
    _gpio_assert_ready(self);
    _gpio_assert_config(config);

    self->config = *config;
    _gpio_apply_config(self);
}

void gpio_set_pull(Gpio* self, GpioPull pull)
{
    GpioConfig config;

    _gpio_assert_ready(self);
    config = self->config;
    config.pull = pull;
    gpio_config(self, &config);
}

void gpio_set_mode(Gpio* self, GpioMode mode)
{
    GpioConfig config;

    _gpio_assert_ready(self);
    config = self->config;
    config.mode = mode;
    gpio_config(self, &config);
}

void gpio_set_speed(Gpio* self, GpioSpeed speed)
{
    GpioConfig config;

    _gpio_assert_ready(self);
    config = self->config;
    config.speed = speed;
    gpio_config(self, &config);
}

void gpio_set_output_type(Gpio* self, GpioOutputType output_type)
{
    GpioConfig config;

    _gpio_assert_ready(self);
    config = self->config;
    config.output_type = output_type;
    gpio_config(self, &config);
}

void gpio_set_alternate(Gpio* self, GpioAlternate alternate)
{
    GpioConfig config;

    _gpio_assert_ready(self);
    config = self->config;
    config.alternate = alternate;
    gpio_config(self, &config);
}

void gpio_write(Gpio* self, GpioLevel level)
{
    _gpio_assert_ready(self);
    ASSERT(self->ops->write != NULL);
    ASSERT(_gpio_level_valid(level));

    self->config.level = level;
    self->ops->write(self->pin, level);
}

GpioLevel gpio_read(Gpio* self)
{
    GpioLevel level;

    _gpio_assert_ready(self);
    ASSERT(self->ops->read != NULL);

    level = self->ops->read(self->pin);
    ASSERT(_gpio_level_valid(level));
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
    _gpio_apply_config(self);

    self->ops = NULL;
    self->pin = 0U;
    self->config = config;
}
