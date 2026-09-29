/* gateway.h - CAN to UDP gateway core.
 * Provided header: the API must not be changed. */
#ifndef GATEWAY_H
#define GATEWAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "canbuf.h"
#include "gwproto.h"

#define GW_MAX_FILTERS       4u
#define GW_BATCH_TIMEOUT_MS  10u

/* Transmit hook (e.g. mapped to a UDP send). Return value is informational. */
typedef int (*gw_send_fn)(void *ctx, const uint8_t *pkt, size_t len);

typedef struct {
    uint32_t match;
    uint32_t mask;
} gw_filter_t;

typedef struct {
    canbuf_t    *rx;
    gw_send_fn   send;
    void        *send_ctx;
    gw_filter_t  filters[GW_MAX_FILTERS];
    size_t       n_filters;
    can_frame_t  batch[GWP_MAX_FRAMES];
    size_t       batch_n;
    uint32_t     batch_start_ms;
    uint16_t     seq;
} gw_t;

void gw_init(gw_t *gw, canbuf_t *rx, gw_send_fn send, void *send_ctx);
bool gw_add_filter(gw_t *gw, uint32_t match, uint32_t mask); /* false if full */
void gw_poll(gw_t *gw, uint32_t now_ms);

#endif /* GATEWAY_H */
