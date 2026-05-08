#include "rb.h"

#include <string.h>

static size_t _rb_advance_index_checked(size_t index, size_t size, size_t capacity)
{
    index += size;
    if (index >= capacity)
    {
        index -= capacity;
    }

    return index;
}

static void _rb_copy_from_buffer_checked(const Rb *self, uint8_t *data, size_t size)
{
    size_t first_size;
    size_t second_size;

    first_size = WD_MIN(size, self->capacity - self->read_index);
    second_size = size - first_size;

    (void)memcpy(data, &self->buffer[self->read_index], first_size);
    if (second_size > 0U)
    {
        (void)memcpy(data + first_size, self->buffer, second_size);
    }
}

static void _rb_drop_checked(Rb *self, size_t size)
{
    self->read_index = _rb_advance_index_checked(self->read_index, size, self->capacity);
    self->size -= size;
}

void rb_init(Rb *self, uint8_t *buffer, size_t capacity)
{
    WD_ASSERT(self != NULL);
    WD_ASSERT(buffer != NULL);
    WD_ASSERT(capacity > 0U);

    self->buffer = buffer;
    self->capacity = capacity;
    self->read_index = 0U;
    self->write_index = 0U;
    self->size = 0U;
}

void rb_deinit(Rb *self)
{
    WD_ASSERT(self != NULL);

    self->buffer = NULL;
    self->capacity = 0U;
    self->read_index = 0U;
    self->write_index = 0U;
    self->size = 0U;
}

size_t rb_write(Rb *self, const uint8_t *data, size_t size)
{
    size_t write_size;
    size_t first_size;
    size_t second_size;

    WD_ASSERT(self != NULL);
    WD_ASSERT(data != NULL);

    write_size = WD_MIN(size, self->capacity - self->size);

    first_size = WD_MIN(write_size, self->capacity - self->write_index);
    second_size = write_size - first_size;

    (void)memcpy(&self->buffer[self->write_index], data, first_size);
    if (second_size > 0U)
    {
        (void)memcpy(self->buffer, data + first_size, second_size);
    }

    self->write_index = _rb_advance_index_checked(self->write_index, write_size, self->capacity);
    self->size += write_size;

    return write_size;
}

size_t rb_read(Rb *self, uint8_t *data, size_t size)
{
    size_t read_size;

    WD_ASSERT(self != NULL);
    WD_ASSERT(data != NULL);

    read_size = WD_MIN(size, self->size);
    _rb_copy_from_buffer_checked(self, data, read_size);
    _rb_drop_checked(self, read_size);

    return read_size;
}

size_t rb_peek(const Rb *self, uint8_t *data, size_t size)
{
    size_t peek_size;

    WD_ASSERT(self != NULL);
    WD_ASSERT(data != NULL);

    peek_size = WD_MIN(size, self->size);
    _rb_copy_from_buffer_checked(self, data, peek_size);

    return peek_size;
}

size_t rb_drop(Rb *self, size_t size)
{
    size_t drop_size;

    WD_ASSERT(self != NULL);

    drop_size = WD_MIN(size, self->size);
    _rb_drop_checked(self, drop_size);

    return drop_size;
}

void rb_clear(Rb *self)
{
    WD_ASSERT(self != NULL);

    self->read_index = 0U;
    self->write_index = 0U;
    self->size = 0U;
}

size_t rb_size(const Rb *self)
{
    WD_ASSERT(self != NULL);

    return self->size;
}

size_t rb_free_size(const Rb *self)
{
    WD_ASSERT(self != NULL);

    return self->capacity - self->size;
}

size_t rb_capacity(const Rb *self)
{
    WD_ASSERT(self != NULL);

    return self->capacity;
}

int rb_is_empty(const Rb *self)
{
    WD_ASSERT(self != NULL);

    return self->size == 0U ? 1U : 0U;
}

int rb_is_full(const Rb *self)
{
    WD_ASSERT(self != NULL);

    return self->size == self->capacity ? 1U : 0U;
}
