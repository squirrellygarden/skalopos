// userland/libc/include/skl/channel.h
//
// Typed message channels. See docs/pillars/04-events.md.

#ifndef SKL_CHANNEL_H
#define SKL_CHANNEL_H

#include <stdint.h>
#include <stddef.h>
#include <skl/handle.h>
#include <sys/status.h>

#define CHAN_NONBLOCK 0x1

/// Create a new channel with the given message capacity.
status_t chan_create(uint32_t capacity, uint32_t flags, handle_t* out_chan_h);

/// Send a message (and optionally handles) on a channel. Blocks if full,
/// unless CHAN_NONBLOCK was set at creation.
status_t chan_send(handle_t chan_h,
                   const void* msg, size_t msg_len,
                   const handle_t* handles, size_t handles_count);

/// Receive a message from a channel. Blocks if empty (subject to flags).
status_t chan_recv(handle_t chan_h,
                   void* buf, size_t buf_cap, size_t* out_msg_len,
                   handle_t* handles_buf, size_t handles_buf_cap,
                   size_t* out_handles_count);

/// Close one side of the channel from the calling process's perspective.
/// If the last sender closes, recv eventually returns STATUS_CHAN_CLOSED.
/// If the last receiver closes, send returns STATUS_CHAN_CLOSED.
status_t chan_close(handle_t chan_h);

#endif
