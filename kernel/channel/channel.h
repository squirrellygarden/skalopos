// kernel/channel/channel.h — bounded MPMC channels with handle-passing.
//
// See docs/pillars/04-events.md.

#ifndef SKALOPS_KERNEL_CHANNEL_CHANNEL_H
#define SKALOPS_KERNEL_CHANNEL_CHANNEL_H

#include <stdint.h>
#include <stddef.h>

#include "../handle/handle.h"

#define CHAN_NONBLOCK 0x1

struct channel;

// TODO: implement in channel.c. Sketch:
//   struct channel* channel_create(uint32_t capacity, uint32_t flags);
//   int32_t channel_send (struct channel*, const void* msg, size_t len,
//                         struct handle_table* sender_handles,
//                         const handle_t* handles_to_send, size_t handles_count,
//                         uint32_t flags);
//   int32_t channel_recv (struct channel*, void* buf, size_t cap,
//                         size_t* out_len,
//                         struct handle_table* receiver_handles,
//                         handle_t* handles_out, size_t handles_cap,
//                         size_t* out_handles_count,
//                         uint32_t flags);
//   void    channel_close(struct channel*, /*from_sender*/ bool);

#endif
