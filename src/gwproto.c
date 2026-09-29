#include "gwproto.h"
#include "canbuf.h"

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout */
uint16_t gwp_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }
            else
            {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

/* true if the frame is valid according to the protocol rules */
bool gwp_frame_valid(const can_frame_t *f)
{
    if ((f == NULL) || (f->dlc > 8) || (f->id & CAN_ID_RESERVED))
    {
        return false;
    }

    if (((f->id & CAN_ID_IDE) == 0) && ((f->id & (~CAN_ID_STD_MASK)) != 0))
    {
        return false;
    }

    return true;
}

/* Returns number of bytes written (> 0) or a negative gwp_err_t. */
int gwp_encode(uint16_t seq, const can_frame_t *frames, size_t n,
               uint8_t *out, size_t out_size)
{
    if ((frames == NULL) || (out == NULL) || (n == 0) || (n > GWP_MAX_FRAMES))
    {
        return GWP_ERR_ARG;
    }

    if (out_size < GWP_PKT_LEN(n))
    {
        return GWP_ERR_LEN;
    }

    for (size_t i = 0; i < n; i++)
    {
        if (!gwp_frame_valid(&frames[i]))
        {
            return GWP_ERR_FRAME;
        }
    }

    int index = 0;
    /** Magic */
    out[index++] = GWP_MAGIC0;
    out[index++] = GWP_MAGIC1;
    /** version */
    out[index++] = GWP_VERSION;
    /** num of frame */
    out[index++] = (uint8_t)n;
    /** sequence */
    out[index++] = (uint8_t)(seq >> 8);
    out[index++] = (uint8_t)(seq >> 0);
    for (size_t i = 0; i < n; i++)
    {
        const can_frame_t *frame = &frames[i];

        /** id */
        out[index++] = (uint8_t)(frame->id >> 24);
        out[index++] = (uint8_t)(frame->id >> 16);
        out[index++] = (uint8_t)(frame->id >> 8);
        out[index++] = (uint8_t)(frame->id >> 0);

        /** dlc */
        out[index++] = frame->dlc;

        /** data */
        for (int j = 0; j < 8; j++)
        {
            if (j >= frame->dlc)
            {
                out[index++] = 0;
            }
            else
            {
                out[index++] = frame->data[j];
            }
        }
    }

    uint16_t crc = gwp_crc16(out, index);
    out[index++] = (uint8_t)(crc >> 8);
    out[index++] = (uint8_t)(crc >> 0);

    return index;
}

/* Returns number of decoded frames (> 0) or a negative gwp_err_t. */
int gwp_decode(const uint8_t *in, size_t len, uint16_t *seq,
               can_frame_t *frames, size_t max_frames)
{
    if ((in == NULL) || (seq == NULL) || (frames == NULL))
    {
        return GWP_ERR_ARG;
    }

    if (len < GWP_HDR_LEN)
    {
        return GWP_ERR_LEN;
    }

    if ((in[0] != GWP_MAGIC0) || (in[1] != GWP_MAGIC1))
    {
        return GWP_ERR_MAGIC;
    }

    if (in[2] != GWP_VERSION)
    {
        return GWP_ERR_VERSION;
    }

    size_t n = in[3];
    if ((n == 0) || (n > GWP_MAX_FRAMES))
    {
        return GWP_ERR_COUNT;
    }

    if (n > max_frames)
    {
        return GWP_ERR_ARG;
    }

    if (len != GWP_PKT_LEN(n))
    {
        return GWP_ERR_LEN;
    }

    *seq = (uint16_t)(((uint16_t)in[4] << 8) | in[5]);

    uint16_t in_crc = (uint16_t)((in[len - 2] << 8) | in[len - 1]);
    uint16_t crc = gwp_crc16(in, len - 2);
    if (in_crc != crc)
    {
        return GWP_ERR_CRC;
    }

    int index = 6;
    for (size_t i = 0; i < n; i++)
    {
        can_frame_t *frame = &frames[i];

        frame->id = (uint32_t)in[index++] << 24;
        frame->id |= (uint32_t)in[index++] << 16;
        frame->id |= (uint32_t)in[index++] << 8;
        frame->id |= (uint32_t)in[index++];

        frame->dlc = in[index++];
        for (int j = 0; j < frame->dlc; j++)
        {
            if (j < frame->dlc)
            {
                frame->data[j] = in[index++];
            }
            else
            {
                frame->data[j] = 0;
                index++;
            }
        }

        if (!gwp_frame_valid(frame))
        {
            return GWP_ERR_FRAME;
        }
    }

    return (int)n;
}
