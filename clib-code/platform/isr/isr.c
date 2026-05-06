#include "isr.h"

static void _isr_assert_ops(const IsrOps *ops)
{
    ASSERT(ops != NULL);
    ASSERT(ops->pending != NULL);
}

static void _isr_assert_ready(const Isr *self)
{
    ASSERT(self != NULL);
    _isr_assert_ops(self->ops);
}

static void _isr_assert_enable_ready(const Isr *self)
{
    _isr_assert_ready(self);
    ASSERT(self->ops->enable != NULL);
}

static void _isr_assert_disable_ready(const Isr *self)
{
    _isr_assert_ready(self);
    ASSERT(self->ops->disable != NULL);
}

static void _isr_clear(Isr *self)
{
    self->ops = NULL;
    self->source = 0U;
    self->action = NULL;
}

void isr_init(Isr *self, const IsrOps *ops, size_t source, isr_action_fn action)
{
    ASSERT(self != NULL);
    _isr_assert_ops(ops);

    self->ops = ops;
    self->source = source;
    self->action = action;
}

void isr_enable(Isr *self)
{
    _isr_assert_enable_ready(self);

    self->ops->enable(self->source);
}

void isr_disable(Isr *self)
{
    _isr_assert_disable_ready(self);

    self->ops->disable(self->source);
}

void isr_handle(Isr *self)
{
    _isr_assert_ready(self);

    if (!self->ops->pending(self->source))
    {
        return;
    }

    if (self->ops->ack != NULL)
    {
        self->ops->ack(self->source);
    }

    if (self->action != NULL)
    {
        self->action(self);
    }
}

void isr_deinit(Isr *self)
{
    _isr_assert_disable_ready(self);

    self->ops->disable(self->source);
    _isr_clear(self);
}
