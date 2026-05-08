#ifndef _RB_H_
#define _RB_H_

#include <stddef.h>
#include <stdint.h>

// 依赖
#include "../../until/until.h"

// 配置

// 类型定义
typedef struct rb_t
{
    uint8_t *buffer;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t size;
} Rb;

// 接口
void rb_init(Rb *self, uint8_t *buffer, size_t capacity);
void rb_deinit(Rb *self);

size_t rb_write(Rb *self, const uint8_t *data, size_t size);
size_t rb_read(Rb *self, uint8_t *data, size_t size);
size_t rb_peek(const Rb *self, uint8_t *data, size_t size);
size_t rb_drop(Rb *self, size_t size);

void rb_clear(Rb *self);
size_t rb_size(const Rb *self);
size_t rb_free_size(const Rb *self);
size_t rb_capacity(const Rb *self);
int rb_is_empty(const Rb *self);
int rb_is_full(const Rb *self);
#endif
