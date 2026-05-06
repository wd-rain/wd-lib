#ifndef _ISR_H_
#define _ISR_H_

#include <stdbool.h>
#include <stddef.h>

// Dependencies
#include "../../until/until.h"

// Types
typedef struct isr_t Isr;

typedef void (*isr_enable_fn)(size_t source);
typedef void (*isr_disable_fn)(size_t source);
typedef bool (*isr_pending_fn)(size_t source);
typedef void (*isr_ack_fn)(size_t source);
typedef void (*isr_action_fn)(Isr *self);

typedef struct isr_ops_t
{
    isr_enable_fn enable;
    isr_disable_fn disable;
    isr_pending_fn pending;
    isr_ack_fn ack;
} IsrOps;

struct isr_t
{
    const IsrOps *ops;
    size_t source;
    isr_action_fn action;
};

// Interface
void isr_init(Isr *self, const IsrOps *ops, size_t source, isr_action_fn action);
void isr_enable(Isr *self);
void isr_disable(Isr *self);
void isr_handle(Isr *self);
void isr_deinit(Isr *self);

#endif
