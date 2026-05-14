// userland/libc/include/skalops/channel.h
//
// Typed message channels. See docs/pillars/04-events.md.

#ifndef SKALOPS_CHANNEL_H
#define SKALOPS_CHANNEL_H

#include <stdint.h>
#include <stddef.h>
#include <skalops/handle.h>
#include <sys/status.h>

#define CHAN_NONBLOCK 0x1

/// Create a new channel with the given message capacity.
status_t chnl_create(uint32_t capacity, uint32_t flags, handle_t * out_chnl_h);

/// Send a message (and optionally handles) on a channel. Blocks if full,
/// unless CHNL_NONBLOCK was set at creation.
status_t chnl_send(handle_t chnl_h,const void* msg, size_t msg_len,
                   const handle_t * handles, size_t handles_count);

/// Receive a message from a channel. Blocks if empty (subject to flags).
status_t chnl_recv(handle_t chnl_h, void* buf, size_t buf_sz, size_t* out_msg_len, 
                    handle_t * hdls_buf, size_t hdls_buf_sz, size_t* out_hdls_cnt);

/// Close one side of the channel from the calling process's perspective.
/// If the last sender closes, recv eventually returns STATUS_CHNL_CLOSED.
/// If the last receiver closes, send returns STATUS_CHNL_CLOSED.
status_t chnl_close(handle_t chnl_h);

#endif
