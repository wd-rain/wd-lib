#ifndef _I2C_H_
#define _I2C_H_

#include <stddef.h>
#include <stdint.h>

// 依赖
#include "../gpio/gpio.h"

// 配置
#ifndef I2C_DEFAULT_CLOCK_HZ
#define I2C_DEFAULT_CLOCK_HZ 100000U
#endif

#ifndef I2C_SCAN_ADDRESS_MIN
#define I2C_SCAN_ADDRESS_MIN 0x08U
#endif

#ifndef I2C_SCAN_ADDRESS_MAX
#define I2C_SCAN_ADDRESS_MAX 0x77U
#endif

#define I2C_ADDRESS_MAX 0x7FU

// 类型定义
typedef enum i2c_status_t
{
    I2C_STATUS_OK = 0,
    I2C_STATUS_ERROR,
    I2C_STATUS_BUSY,
    I2C_STATUS_TIMEOUT,
    I2C_STATUS_NACK,
    I2C_STATUS_ARBITRATION_LOST,
    I2C_STATUS_OVERFLOW
} I2cStatus;

typedef enum i2c_mem_address_size_t
{
    I2C_MEM_ADDRESS_SIZE_8BIT = 0,
    I2C_MEM_ADDRESS_SIZE_16BIT
} I2cMemAddressSize;

typedef struct i2c_config_t
{
    uint32_t clock_hz;
} I2cConfig;

typedef struct i2c_gpio_config_t
{
    const GpioOps *gpio_ops;
    size_t scl_pin;
    size_t sda_pin;
    GpioConfig scl_config;
    GpioConfig sda_config;
} I2cGpioConfig;

typedef I2cStatus (*i2c_config_fn)(size_t bus, const I2cConfig *config);
typedef I2cStatus (*i2c_write_fn)(size_t bus, uint8_t address, const uint8_t *data, size_t len, uint32_t timeout_ms);
typedef I2cStatus (*i2c_read_fn)(size_t bus, uint8_t address, uint8_t *data, size_t len, uint32_t timeout_ms);
typedef I2cStatus (*i2c_mem_write_fn)(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t *data, size_t len, uint32_t timeout_ms);
typedef I2cStatus (*i2c_mem_read_fn)(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t *data, size_t len, uint32_t timeout_ms);
typedef void (*i2c_deinit_fn)(size_t bus);

typedef struct i2c_ops_t
{
    i2c_config_fn config;
    i2c_write_fn write;
    i2c_read_fn read;
    i2c_mem_write_fn mem_write;
    i2c_mem_read_fn mem_read;
    i2c_deinit_fn deinit;
} I2cOps;

typedef struct i2c_t
{
    const I2cOps *ops;
    size_t bus;
    I2cConfig config;
    Gpio scl;
    Gpio sda;
} I2c;

// 接口
I2cStatus i2c_init(I2c *self, const I2cOps *ops, size_t bus, const I2cConfig *config, const I2cGpioConfig *gpio_cfg);
I2cStatus i2c_config(I2c *self, const I2cConfig *config);
I2cStatus i2c_write(I2c *self, uint8_t address, const uint8_t *data, size_t len, uint32_t timeout_ms);
I2cStatus i2c_read(I2c *self, uint8_t address, uint8_t *data, size_t len, uint32_t timeout_ms);
I2cStatus i2c_mem_write(I2c *self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t *data, size_t len, uint32_t timeout_ms);
I2cStatus i2c_mem_read(I2c *self, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t *data, size_t len, uint32_t timeout_ms);
I2cStatus i2c_is_device_ready(I2c *self, uint8_t address, size_t trials, uint32_t timeout_ms);
I2cStatus i2c_scan(I2c *self, uint8_t *addresses, size_t capacity, size_t *found_count, uint32_t timeout_ms);
void i2c_deinit(I2c *self);

#endif
