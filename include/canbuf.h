/* canbuf.h - Single-producer / single-consumer ring buffer for CAN frames.
 * Provided header: the API must not be changed. The member layout of
 * canbuf_t may be adapted (e.g. to use _Atomic types). */
#ifndef CANBUF_H
#define CANBUF_H

#include <stdint.h>
#include <stdbool.h>

#define CAN_ID_IDE       0x80000000u  /* bit 31: extended (29-bit) identifier */
#define CAN_ID_RESERVED  0x60000000u  /* bits 30..29: must be zero           */
#define CAN_ID_EXT_MASK  0x1FFFFFFFu
#define CAN_ID_STD_MASK  0x000007FFu

typedef struct {
    uint32_t id;       /* identifier incl. IDE flag in bit 31 */
    uint8_t  dlc;      /* 0..8 */
    uint8_t  data[8];
} can_frame_t;

#define CANBUF_SIZE 16u  /* power of two, all slots usable */

typedef struct {
    can_frame_t       slots[CANBUF_SIZE];
    volatile uint32_t head;   /* written by producer only */
    volatile uint32_t tail;   /* written by consumer only */
    volatile uint32_t drops;  /* written by producer only */
} canbuf_t;

void     canbuf_init(canbuf_t *b);
bool     canbuf_push(canbuf_t *b, const can_frame_t *f);  /* ISR context  */
bool     canbuf_pop(canbuf_t *b, can_frame_t *out);       /* main context */
uint32_t canbuf_count(const canbuf_t *b);
uint32_t canbuf_drops(const canbuf_t *b);

#endif /* CANBUF_H */
