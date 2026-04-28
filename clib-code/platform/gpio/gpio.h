#ifndef _GPIO_H_
#define _GPIO_H_

#include <stddef.h>

// 依赖
#include "../../until/until.h"

// 配置
#ifndef GPIO_DEFAULT_MODE
#define GPIO_DEFAULT_MODE GPIO_MODE_INPUT
#endif

#ifndef GPIO_DEFAULT_PULL
#define GPIO_DEFAULT_PULL GPIO_PULL_NONE
#endif

#ifndef GPIO_DEFAULT_SPEED
#define GPIO_DEFAULT_SPEED GPIO_SPEED_LOW
#endif

#ifndef GPIO_DEFAULT_OUTPUT_TYPE
#define GPIO_DEFAULT_OUTPUT_TYPE GPIO_OUTPUT_PUSH_PULL
#endif

#ifndef GPIO_DEFAULT_ALTERNATE
#define GPIO_DEFAULT_ALTERNATE GPIO_ALTERNATE_NONE
#endif

#ifndef GPIO_DEFAULT_LEVEL
#define GPIO_DEFAULT_LEVEL GPIO_LEVEL_LOW
#endif

// 类型定义
typedef enum gpio_level_t
{
    GPIO_LEVEL_LOW = 0,
    GPIO_LEVEL_HIGH
} GpioLevel;

typedef enum gpio_mode_t
{
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_ALTERNATE,
    GPIO_MODE_ANALOG
} GpioMode;

typedef enum gpio_pull_t
{
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN
} GpioPull;

typedef enum gpio_speed_t
{
    GPIO_SPEED_LOW = 0,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_HIGH,
    GPIO_SPEED_VERY_HIGH
} GpioSpeed;

typedef enum gpio_output_type_t
{
    GPIO_OUTPUT_PUSH_PULL = 0,
    GPIO_OUTPUT_OPEN_DRAIN
} GpioOutputType;

typedef enum gpio_alternate_t
{
    GPIO_ALTERNATE_NONE = 0,
    GPIO_ALTERNATE_0,
    GPIO_ALTERNATE_1,
    GPIO_ALTERNATE_2,
    GPIO_ALTERNATE_3,
    GPIO_ALTERNATE_4,
    GPIO_ALTERNATE_5,
    GPIO_ALTERNATE_6,
    GPIO_ALTERNATE_7,
    GPIO_ALTERNATE_8,
    GPIO_ALTERNATE_9,
    GPIO_ALTERNATE_10,
    GPIO_ALTERNATE_11,
    GPIO_ALTERNATE_12,
    GPIO_ALTERNATE_13,
    GPIO_ALTERNATE_14,
    GPIO_ALTERNATE_15
} GpioAlternate;

typedef struct gpio_config_t
{
    GpioMode mode;
    GpioPull pull;
    GpioSpeed speed;
    GpioOutputType output_type;
    GpioAlternate alternate;
    GpioLevel level;
} GpioConfig;

typedef void (*gpio_config_fn)(size_t pin, const GpioConfig* config);
typedef void (*gpio_write_fn)(size_t pin, GpioLevel level);
typedef GpioLevel (*gpio_read_fn)(size_t pin);

typedef struct gpio_ops_t
{
    gpio_config_fn config;
    gpio_write_fn write;
    gpio_read_fn read;
} GpioOps;

typedef struct gpio_t
{
    const GpioOps* ops;
    size_t pin;
    GpioConfig config;
} Gpio;

// 接口
void gpio_init(Gpio* self, const GpioOps* ops, size_t pin); // 会将其设置成悬空输入
void gpio_config(Gpio* self, const GpioConfig* config);
void gpio_set_pull(Gpio* self, GpioPull pull);
void gpio_set_mode(Gpio* self, GpioMode mode);
void gpio_set_speed(Gpio* self, GpioSpeed speed);
void gpio_set_output_type(Gpio* self, GpioOutputType output_type);
void gpio_set_alternate(Gpio* self, GpioAlternate alternate);
void gpio_write(Gpio* self, GpioLevel level);
GpioLevel gpio_read(Gpio* self);
void gpio_toggle(Gpio* self);
void gpio_deinit(Gpio* self); // 会将其设置成悬空输入

#endif
