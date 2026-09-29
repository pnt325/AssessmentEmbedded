#include "canbuf.h"

void canbuf_init(canbuf_t *b)
{
    if (b == NULL)
    {
        return;
    }

    b->drops = 0;
    b->head = 0;
    b->tail = 0;
}

bool canbuf_push(canbuf_t *b, const can_frame_t *f)
{
    if ((b == NULL) || (f == NULL))
    {
        return false;
    }

    /** buffer is full */
    if (canbuf_count(b) == CANBUF_SIZE)
    {
        b->drops++;
        return false;
    }

    /** Copy data */
    uint32_t head = b->head % CANBUF_SIZE;
    b->slots[head].id = f->id;
    b->slots[head].dlc = f->dlc;
    b->slots[head].data[0] = f->data[0];
    b->slots[head].data[1] = f->data[1];
    b->slots[head].data[2] = f->data[2];
    b->slots[head].data[3] = f->data[3];
    b->slots[head].data[4] = f->data[4];
    b->slots[head].data[5] = f->data[5];
    b->slots[head].data[6] = f->data[6];
    b->slots[head].data[7] = f->data[7];

    b->head++;

    return true;
}

bool canbuf_pop(canbuf_t *b, can_frame_t *out)
{
    /** Argument invalid or buffer is empty */
    if ((b == NULL) || (out == NULL) || (b->tail == b->head))
    {
        return false;
    }

    uint32_t tail = b->tail % CANBUF_SIZE;

    out->id = b->slots[tail].id;
    out->dlc = b->slots[tail].dlc;
    out->data[0] = b->slots[tail].data[0];
    out->data[1] = b->slots[tail].data[1];
    out->data[2] = b->slots[tail].data[2];
    out->data[3] = b->slots[tail].data[3];
    out->data[4] = b->slots[tail].data[4];
    out->data[5] = b->slots[tail].data[5];
    out->data[6] = b->slots[tail].data[6];
    out->data[7] = b->slots[tail].data[7];

    b->tail++;

    return true;
}

uint32_t canbuf_count(const canbuf_t *b)
{
    if (b == NULL)
    {
        return 0;
    }
    return (uint32_t)(b->head - b->tail);
}

uint32_t canbuf_drops(const canbuf_t *b)
{
    if (b == NULL)
    {
        return 0;
    }
    return b->drops;
}
