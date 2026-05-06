#include "stm32f4xx_hal.h"
#include "bsp_i2c.h"

static void _bsp_i2c_assert_bus(size_t bus)
{
    ASSERT(bus < (size_t)WD_BSP_I2C_BUS_COUNT);
}

static void _bsp_i2c_assert_config(const I2cConfig *config)
{
    ASSERT(config != NULL);
    ASSERT(config->clock_hz > 0U);
    ASSERT(config->clock_hz <= 400000U);
}

static void _bsp_i2c_assert_transfer(uint8_t address, const void *data, size_t len)
{
    ASSERT(address <= I2C_ADDRESS_MAX);
    ASSERT(data != NULL || len == 0U);
    ASSERT(len <= UINT16_MAX);
}

static void _bsp_i2c_assert_mem_address_size(I2cMemAddressSize mem_address_size)
{
    ASSERT(mem_address_size <= WD_I2C_MEM_ADDRESS_SIZE_16BIT);
}

static I2C_TypeDef *_bsp_i2c_instance(size_t bus)
{
    I2C_TypeDef *instances[] = {
        I2C1,
        I2C2,
        I2C3
    };

    _bsp_i2c_assert_bus(bus);
    return instances[bus];
}

static I2C_HandleTypeDef *_bsp_i2c_handle(size_t bus)
{
    static I2C_HandleTypeDef handles[WD_BSP_I2C_BUS_COUNT];

    _bsp_i2c_assert_bus(bus);
    return &handles[bus];
}

static void _bsp_i2c_enable_clock(size_t bus)
{
    _bsp_i2c_assert_bus(bus);

    switch (bus)
    {
    case WD_BSP_I2C_BUS_1:
        __HAL_RCC_I2C1_CLK_ENABLE();
        break;
    case WD_BSP_I2C_BUS_2:
        __HAL_RCC_I2C2_CLK_ENABLE();
        break;
    case WD_BSP_I2C_BUS_3:
        __HAL_RCC_I2C3_CLK_ENABLE();
        break;
    default:
        ASSERT(0);
    }
}

static void _bsp_i2c_disable_clock(size_t bus)
{
    _bsp_i2c_assert_bus(bus);

    switch (bus)
    {
    case WD_BSP_I2C_BUS_1:
        __HAL_RCC_I2C1_CLK_DISABLE();
        break;
    case WD_BSP_I2C_BUS_2:
        __HAL_RCC_I2C2_CLK_DISABLE();
        break;
    case WD_BSP_I2C_BUS_3:
        __HAL_RCC_I2C3_CLK_DISABLE();
        break;
    default:
        ASSERT(0);
    }
}

static uint16_t _bsp_i2c_hal_address(uint8_t address)
{
    ASSERT(address <= I2C_ADDRESS_MAX);

    return (uint16_t)((uint16_t)address << 1U);
}

static uint16_t _bsp_i2c_hal_mem_address_size(I2cMemAddressSize mem_address_size)
{
    _bsp_i2c_assert_mem_address_size(mem_address_size);

    switch (mem_address_size)
    {
    case WD_I2C_MEM_ADDRESS_SIZE_8BIT:
        return I2C_MEMADD_SIZE_8BIT;
    case WD_I2C_MEM_ADDRESS_SIZE_16BIT:
        return I2C_MEMADD_SIZE_16BIT;
    default:
        ASSERT(0);
    }
}

static I2cStatus _bsp_i2c_error_status(uint32_t error_code)
{
    if ((error_code & HAL_I2C_ERROR_TIMEOUT) != 0U)
    {
        return WD_I2C_STATUS_TIMEOUT;
    }

    if ((error_code & HAL_I2C_ERROR_ARLO) != 0U)
    {
        return WD_I2C_STATUS_ARBITRATION_LOST;
    }

    if ((error_code & HAL_I2C_ERROR_AF) != 0U)
    {
        return WD_I2C_STATUS_NACK;
    }

    if ((error_code & HAL_I2C_ERROR_OVR) != 0U)
    {
        return WD_I2C_STATUS_OVERFLOW;
    }

    return WD_I2C_STATUS_ERROR;
}

static I2cStatus _bsp_i2c_status(const I2C_HandleTypeDef *handle, HAL_StatusTypeDef status)
{
    ASSERT(handle != NULL);

    switch (status)
    {
    case HAL_OK:
        return WD_I2C_STATUS_OK;
    case HAL_ERROR:
        return _bsp_i2c_error_status(handle->ErrorCode);
    case HAL_BUSY:
        return WD_I2C_STATUS_BUSY;
    case HAL_TIMEOUT:
        return WD_I2C_STATUS_TIMEOUT;
    default:
        ASSERT(0);
    }
}

static I2cStatus _bsp_i2c_probe(I2C_HandleTypeDef *handle, uint8_t address, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;

    ASSERT(handle != NULL);
    ASSERT(address <= I2C_ADDRESS_MAX);

    status = HAL_I2C_Master_Transmit(handle, _bsp_i2c_hal_address(address), NULL, 0U, timeout_ms);
    return _bsp_i2c_status(handle, status);
}

static I2cStatus _bsp_i2c_config(size_t bus, const I2cConfig *config)
{
    I2C_HandleTypeDef *handle;
    HAL_StatusTypeDef status;

    _bsp_i2c_assert_bus(bus);
    _bsp_i2c_assert_config(config);

    handle = _bsp_i2c_handle(bus);
    _bsp_i2c_enable_clock(bus);

    handle->Instance = _bsp_i2c_instance(bus);
    handle->Init.ClockSpeed = config->clock_hz;
    handle->Init.DutyCycle = I2C_DUTYCYCLE_2;
    handle->Init.OwnAddress1 = 0U;
    handle->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    handle->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    handle->Init.OwnAddress2 = 0U;
    handle->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    handle->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    status = HAL_I2C_Init(handle);
    if (status != HAL_OK)
    {
        _bsp_i2c_disable_clock(bus);
    }

    return _bsp_i2c_status(handle, status);
}

static I2cStatus _bsp_i2c_write(size_t bus, uint8_t address, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *handle;
    HAL_StatusTypeDef status;

    _bsp_i2c_assert_bus(bus);
    _bsp_i2c_assert_transfer(address, data, len);

    handle = _bsp_i2c_handle(bus);
    if (len == 0U)
    {
        return _bsp_i2c_probe(handle, address, timeout_ms);
    }

    status = HAL_I2C_Master_Transmit(handle, _bsp_i2c_hal_address(address), (uint8_t *)data, (uint16_t)len, timeout_ms);
    return _bsp_i2c_status(handle, status);
}

static I2cStatus _bsp_i2c_read(size_t bus, uint8_t address, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *handle;
    HAL_StatusTypeDef status;

    _bsp_i2c_assert_bus(bus);
    _bsp_i2c_assert_transfer(address, data, len);

    handle = _bsp_i2c_handle(bus);
    if (len == 0U)
    {
        return _bsp_i2c_probe(handle, address, timeout_ms);
    }

    status = HAL_I2C_Master_Receive(handle, _bsp_i2c_hal_address(address), data, (uint16_t)len, timeout_ms);
    return _bsp_i2c_status(handle, status);
}

static I2cStatus _bsp_i2c_mem_write(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *handle;
    HAL_StatusTypeDef status;

    _bsp_i2c_assert_bus(bus);
    _bsp_i2c_assert_transfer(address, data, len);
    _bsp_i2c_assert_mem_address_size(mem_address_size);

    handle = _bsp_i2c_handle(bus);
    status = HAL_I2C_Mem_Write(handle, _bsp_i2c_hal_address(address), mem_address, _bsp_i2c_hal_mem_address_size(mem_address_size), (uint8_t *)data, (uint16_t)len, timeout_ms);
    return _bsp_i2c_status(handle, status);
}

static I2cStatus _bsp_i2c_mem_read(size_t bus, uint8_t address, uint16_t mem_address, I2cMemAddressSize mem_address_size, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *handle;
    HAL_StatusTypeDef status;

    _bsp_i2c_assert_bus(bus);
    _bsp_i2c_assert_transfer(address, data, len);
    _bsp_i2c_assert_mem_address_size(mem_address_size);

    handle = _bsp_i2c_handle(bus);
    status = HAL_I2C_Mem_Read(handle, _bsp_i2c_hal_address(address), mem_address, _bsp_i2c_hal_mem_address_size(mem_address_size), data, (uint16_t)len, timeout_ms);
    return _bsp_i2c_status(handle, status);
}

static void _bsp_i2c_deinit(size_t bus)
{
    I2C_HandleTypeDef *handle;

    _bsp_i2c_assert_bus(bus);

    handle = _bsp_i2c_handle(bus);
    handle->Instance = _bsp_i2c_instance(bus);
    HAL_I2C_DeInit(handle);
    _bsp_i2c_disable_clock(bus);
}

static GpioConfig _bsp_i2c_default_pin_config(void)
{
    return bsp_gpio_alternate_config(WD_GPIO_ALTERNATE_4, WD_GPIO_PULL_UP, WD_GPIO_SPEED_VERY_HIGH, WD_GPIO_OUTPUT_OPEN_DRAIN, WD_GPIO_LEVEL_HIGH);
}

static size_t _bsp_i2c_scl_pin(size_t bus)
{
    _bsp_i2c_assert_bus(bus);

    switch (bus)
    {
    case WD_BSP_I2C_BUS_1:
        return BSP_I2C1_SCL_PIN;
    case WD_BSP_I2C_BUS_2:
        return BSP_I2C2_SCL_PIN;
    case WD_BSP_I2C_BUS_3:
        return BSP_I2C3_SCL_PIN;
    default:
        ASSERT(0);
    }
}

static size_t _bsp_i2c_sda_pin(size_t bus)
{
    _bsp_i2c_assert_bus(bus);

    switch (bus)
    {
    case WD_BSP_I2C_BUS_1:
        return BSP_I2C1_SDA_PIN;
    case WD_BSP_I2C_BUS_2:
        return BSP_I2C2_SDA_PIN;
    case WD_BSP_I2C_BUS_3:
        return BSP_I2C3_SDA_PIN;
    default:
        ASSERT(0);
    }
}

const I2cOps *bsp_i2c_ops(void)
{
    static const I2cOps ops = {
        _bsp_i2c_config,
        _bsp_i2c_write,
        _bsp_i2c_read,
        _bsp_i2c_mem_write,
        _bsp_i2c_mem_read,
        _bsp_i2c_deinit
    };

    return &ops;
}

void bsp_i2c_default_gpio_config(size_t bus, I2cGpioConfig *config)
{
    GpioConfig pin_config;

    ASSERT(config != NULL);
    _bsp_i2c_assert_bus(bus);

    pin_config = _bsp_i2c_default_pin_config();
    config->gpio_ops = bsp_gpio_ops();
    config->scl_pin = _bsp_i2c_scl_pin(bus);
    config->sda_pin = _bsp_i2c_sda_pin(bus);
    config->scl_config = pin_config;
    config->sda_config = pin_config;
}
