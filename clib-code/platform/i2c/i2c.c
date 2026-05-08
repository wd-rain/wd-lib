#include "i2c.h"

static void _i2c_assert_mem_address(uint16_t mem_address, I2cMemAddressSize mem_address_size)
{
    WD_ASSERT(mem_address_size <= WD_I2C_MEM_ADDRESS_SIZE_16BIT);

    if (mem_address_size == WD_I2C_MEM_ADDRESS_SIZE_8BIT)
    {
        WD_ASSERT(mem_address <= UINT8_MAX);
    }
}

static void _i2c_assert_transfer(uint8_t address, const void *data, size_t len)
{
    WD_ASSERT(address <= I2C_ADDRESS_MAX);
    WD_ASSERT(data != NULL || len == 0U);
}

static void _i2c_assert_mem_transfer(uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const void *data, size_t len)
{
    _i2c_assert_transfer(address, data, len);
    _i2c_assert_mem_address(mem_address, mem_address_size);
}

static void _i2c_assert_config(const I2cConfig *config)
{
    WD_ASSERT(config != NULL);
    WD_ASSERT(config->clock_hz > 0U);
}

static I2cConfig _i2c_default_config(void)
{
    I2cConfig config = {
        I2C_DEFAULT_CLOCK_HZ
    };

    return config;
}

static void _i2c_assert_ops(const I2cOps *ops)
{
    WD_ASSERT(ops != NULL);
    WD_ASSERT(ops->config != NULL);
    WD_ASSERT(ops->write != NULL);
    WD_ASSERT(ops->read != NULL);
    WD_ASSERT(ops->mem_write != NULL);
    WD_ASSERT(ops->mem_read != NULL);
    WD_ASSERT(ops->deinit != NULL);
}

static void _i2c_assert_gpio_pin_config(const GpioConfig *config)
{
    WD_ASSERT(config != NULL);
    WD_ASSERT(config->mode <= WD_GPIO_MODE_ANALOG);
    WD_ASSERT(config->pull <= WD_GPIO_PULL_DOWN);
    WD_ASSERT(config->speed <= WD_GPIO_SPEED_VERY_HIGH);
    WD_ASSERT(config->output_type <= WD_GPIO_OUTPUT_OPEN_DRAIN);
    WD_ASSERT(config->alternate <= WD_GPIO_ALTERNATE_15);
    WD_ASSERT(config->level <= WD_GPIO_LEVEL_HIGH);
}

static void _i2c_assert_gpio_config(const I2cGpioConfig *gpio_cfg)
{
    if (gpio_cfg != NULL)
    {
        WD_ASSERT(gpio_cfg->gpio_ops != NULL);
        _i2c_assert_gpio_pin_config(&gpio_cfg->scl_config);
        _i2c_assert_gpio_pin_config(&gpio_cfg->sda_config);
    }
}

static void _i2c_assert_ready(const I2c *self)
{
    WD_ASSERT(self != NULL);
    _i2c_assert_ops(self->ops);
}

static void _i2c_clear(I2c *self)
{
    self->ops = NULL;
    self->bus = 0U;
    self->config = _i2c_default_config();
    self->scl.ops = NULL;
    self->scl.pin = 0U;
    self->sda.ops = NULL;
    self->sda.pin = 0U;
}

static void _i2c_init_gpio(I2c *self, const I2cGpioConfig *gpio_cfg)
{
    gpio_init(&self->scl, gpio_cfg->gpio_ops, gpio_cfg->scl_pin);
    gpio_config(&self->scl, &gpio_cfg->scl_config);
    gpio_init(&self->sda, gpio_cfg->gpio_ops, gpio_cfg->sda_pin);
    gpio_config(&self->sda, &gpio_cfg->sda_config);
}

static void _i2c_deinit_gpio(I2c *self)
{
    if (self->scl.ops != NULL && self->sda.ops != NULL)
    {
        gpio_deinit(&self->scl);
        gpio_deinit(&self->sda);
    }
}

static void _i2c_assert_probe(uint8_t address, size_t trials)
{
    WD_ASSERT(address <= I2C_ADDRESS_MAX);
    WD_ASSERT(trials > 0U);
}

static void _i2c_assert_scan(uint8_t *addresses, size_t capacity, const size_t *found_count)
{
    WD_ASSERT(I2C_SCAN_ADDRESS_MIN <= I2C_SCAN_ADDRESS_MAX);
    WD_ASSERT(I2C_SCAN_ADDRESS_MAX <= I2C_ADDRESS_MAX);
    WD_ASSERT(addresses != NULL || capacity == 0U);
    WD_ASSERT(found_count != NULL);
}

static I2cStatus _i2c_config_checked(I2c *self, const I2cConfig *config)
{
    I2cStatus status;

    status = self->ops->config(self->bus, config);

    if (status == WD_I2C_STATUS_OK)
    {
        self->config = *config;
    }

    return status;
}

static I2cStatus _i2c_write_checked(I2c *self, uint8_t address, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2cStatus status;

    status = self->ops->write(self->bus, address, data, len, timeout_ms);

    return status;
}

static I2cStatus _i2c_read_checked(I2c *self, uint8_t address, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2cStatus status;

    status = self->ops->read(self->bus, address, data, len, timeout_ms);

    return status;
}

static I2cStatus _i2c_mem_write_checked(I2c *self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2cStatus status;

    status = self->ops->mem_write(self->bus, address, mem_address, mem_address_size, data, len, timeout_ms);

    return status;
}

static I2cStatus _i2c_mem_read_checked(I2c *self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2cStatus status;

    status = self->ops->mem_read(self->bus, address, mem_address, mem_address_size, data, len, timeout_ms);

    return status;
}

static I2cStatus _i2c_is_device_ready_checked(I2c *self, uint8_t address, size_t trials, uint32_t timeout_ms)
{
    I2cStatus status;
    size_t i;

    status = WD_I2C_STATUS_ERROR;
    for (i = 0U; i < trials; ++i)
    {
        status = _i2c_write_checked(self, address, NULL, 0U, timeout_ms);
        if (status == WD_I2C_STATUS_OK)
        {
            return WD_I2C_STATUS_OK;
        }
    }

    return status;
}

I2cStatus i2c_init(I2c *self, const I2cOps *ops, size_t bus, const I2cConfig *config, const I2cGpioConfig *gpio_cfg)
{
    I2cStatus status;

    WD_ASSERT(self != NULL);
    _i2c_assert_ops(ops);
    _i2c_assert_config(config);
    _i2c_assert_gpio_config(gpio_cfg);

    _i2c_clear(self);
    self->ops = ops;
    self->bus = bus;

    if (gpio_cfg != NULL)
    {
        _i2c_init_gpio(self, gpio_cfg);
    }

    status = _i2c_config_checked(self, config);

    if (status != WD_I2C_STATUS_OK)
    {
        _i2c_deinit_gpio(self);
        _i2c_clear(self);
        return status;
    }

    self->config = *config;
    return WD_I2C_STATUS_OK;
}

I2cStatus i2c_config(I2c *self, const I2cConfig *config)
{
    _i2c_assert_ready(self);
    _i2c_assert_config(config);

    return _i2c_config_checked(self, config);
}

I2cStatus i2c_write(I2c *self, uint8_t address, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    _i2c_assert_ready(self);
    _i2c_assert_transfer(address, data, len);

    return _i2c_write_checked(self, address, data, len, timeout_ms);
}

I2cStatus i2c_read(I2c *self, uint8_t address, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    _i2c_assert_ready(self);
    _i2c_assert_transfer(address, data, len);

    return _i2c_read_checked(self, address, data, len, timeout_ms);
}

I2cStatus i2c_mem_write(I2c *self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    _i2c_assert_ready(self);
    _i2c_assert_mem_transfer(address, mem_address, mem_address_size, data, len);

    return _i2c_mem_write_checked(self, address, mem_address, mem_address_size, data, len, timeout_ms);
}

I2cStatus i2c_mem_read(I2c *self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    _i2c_assert_ready(self);
    _i2c_assert_mem_transfer(address, mem_address, mem_address_size, data, len);

    return _i2c_mem_read_checked(self, address, mem_address, mem_address_size, data, len, timeout_ms);
}

I2cStatus i2c_is_device_ready(I2c *self, uint8_t address, size_t trials, uint32_t timeout_ms)
{
    _i2c_assert_ready(self);
    _i2c_assert_probe(address, trials);

    return _i2c_is_device_ready_checked(self, address, trials, timeout_ms);
}

I2cStatus i2c_scan(I2c *self, uint8_t *addresses, size_t capacity, size_t *found_count, uint32_t timeout_ms)
{
    I2cStatus status;
    I2cStatus probe;
    size_t address;
    size_t count;

    _i2c_assert_ready(self);
    _i2c_assert_scan(addresses, capacity, found_count);

    status = WD_I2C_STATUS_OK;
    count = 0U;
    for (address = I2C_SCAN_ADDRESS_MIN; address <= I2C_SCAN_ADDRESS_MAX; ++address)
    {
        probe = _i2c_is_device_ready_checked(self, (uint8_t)address, 1U, timeout_ms);
        if (probe == WD_I2C_STATUS_OK)
        {
            if (count >= capacity)
            {
                status = WD_I2C_STATUS_OVERFLOW;
                break;
            }
            addresses[count] = (uint8_t)address;
            ++count;
        }
        else if (probe != WD_I2C_STATUS_NACK)
        {
            status = probe;
            break;
        }
    }

    *found_count = count;
    return status;
}

void i2c_deinit(I2c *self)
{
    _i2c_assert_ready(self);

    self->ops->deinit(self->bus);
    _i2c_deinit_gpio(self);
    _i2c_clear(self);
}
