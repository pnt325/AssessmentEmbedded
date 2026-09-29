/* gwproto.h - Binary gateway protocol (CAN frames over UDP).
 * Provided header: must not be changed. */
#ifndef GWPROTO_H
#define GWPROTO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "canbuf.h"

#define GWP_MAGIC0      0x47u
#define GWP_MAGIC1      0x57u
#define GWP_VERSION     0x01u
#define GWP_MAX_FRAMES  8u
#define GWP_HDR_LEN     6u
#define GWP_REC_LEN     13u
#define GWP_CRC_LEN     2u
#define GWP_PKT_LEN(n)  (GWP_HDR_LEN + GWP_REC_LEN * (n) + GWP_CRC_LEN)
#define GWP_MAX_LEN     GWP_PKT_LEN(GWP_MAX_FRAMES)

typedef enum {
    GWP_OK          =  0,
    GWP_ERR_ARG     = -1,
    GWP_ERR_LEN     = -2,
    GWP_ERR_MAGIC   = -3,
    GWP_ERR_VERSION = -4,
    GWP_ERR_COUNT   = -5,
    GWP_ERR_FRAME   = -6,
    GWP_ERR_CRC     = -7
} gwp_err_t;

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout */
uint16_t gwp_crc16(const uint8_t *data, size_t len);

/* true if the frame is valid according to the protocol rules */
bool gwp_frame_valid(const can_frame_t *f);

/* Returns number of bytes written (> 0) or a negative gwp_err_t. */
int gwp_encode(uint16_t seq, const can_frame_t *frames, size_t n,
               uint8_t *out, size_t out_size);

/* Returns number of decoded frames (> 0) or a negative gwp_err_t. */
int gwp_decode(const uint8_t *in, size_t len, uint16_t *seq,
               can_frame_t *frames, size_t max_frames);

#endif /* GWPROTO_H */
