#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "gwproto.h"
#include "test.h"

/* ------------------------------------------------------------- helpers */

/* Spec 5.7 vector 1: seq 0x0102, std ID 0x123, DLC 3, data DE AD BE */
static const uint8_t k_vec1[] = {
    0x47, 0x57, 0x01, 0x01, 0x01, 0x02,
    0x00, 0x00, 0x01, 0x23, 0x03, 0xDE, 0xAD, 0xBE, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xD0, 0x76
};

/* Spec 5.7 vector 2: seq 0xFFFF, ext ID 0x18FF50E5 DLC 8 01..08; std 0x7FF DLC 0 */
static const uint8_t k_vec2[] = {
    0x47, 0x57, 0x01, 0x02, 0xFF, 0xFF,
    0x98, 0xFF, 0x50, 0xE5, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x00, 0x00, 0x07, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xDD, 0xAA
};

static can_frame_t frame(uint32_t id, uint8_t dlc, const uint8_t *data)
{
    can_frame_t f;
    memset(&f, 0, sizeof f);
    f.id = id;
    f.dlc = dlc;
    if (data != NULL) {
        memcpy(f.data, data, dlc <= 8u ? dlc : 8u);
    }
    return f;
}

static void vec1_frames(can_frame_t *f)
{
    static const uint8_t d[] = { 0xDE, 0xAD, 0xBE };
    f[0] = frame(0x123u, 3u, d);
}

static void vec2_frames(can_frame_t *f)
{
    static const uint8_t d[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    f[0] = frame(CAN_ID_IDE | 0x18FF50E5u, 8u, d);
    f[1] = frame(0x7FFu, 0u, NULL);
}

static bool frame_eq(const can_frame_t *a, const can_frame_t *b)
{
    return a->id == b->id && a->dlc == b->dlc &&
           memcmp(a->data, b->data, sizeof a->data) == 0;
}

/* Recompute the trailing CRC after a packet has been modified. */
static void fix_crc(uint8_t *pkt, size_t len)
{
    uint16_t crc = gwp_crc16(pkt, len - GWP_CRC_LEN);
    pkt[len - 2u] = (uint8_t)(crc >> 8);
    pkt[len - 1u] = (uint8_t)crc;
}

/* ---------------------------------------------------------------- crc16 */

static void test_gwp_crc16(void)
{
    const uint8_t check[] = "123456789";
    uint8_t dummy = 0u;

    CHECK(gwp_crc16(check, 9u) == 0x29B1u);
    CHECK(gwp_crc16(&dummy, 0u) == 0xFFFFu);
    CHECK(gwp_crc16(k_vec1, sizeof k_vec1 - 2u) == 0xD076u);
    CHECK(gwp_crc16(k_vec2, sizeof k_vec2 - 2u) == 0xDDAAu);
}

/* ---------------------------------------------------------- frame_valid */

static void test_gwp_frame_valid(void)
{
    can_frame_t f;

    CHECK(!gwp_frame_valid(NULL));

    /* DLC range */
    f = frame(0x123u, 0u, NULL);  CHECK(gwp_frame_valid(&f));
    f = frame(0x123u, 8u, NULL);  CHECK(gwp_frame_valid(&f));
    f = frame(0x123u, 9u, NULL);  CHECK(!gwp_frame_valid(&f));
    f = frame(0x123u, 255u, NULL); CHECK(!gwp_frame_valid(&f));

    /* standard IDs */
    f = frame(0x000u, 0u, NULL);  CHECK(gwp_frame_valid(&f));
    f = frame(0x7FFu, 0u, NULL);  CHECK(gwp_frame_valid(&f));
    f = frame(0x800u, 0u, NULL);  CHECK(!gwp_frame_valid(&f));
    f = frame(0x1FFFFFFFu, 0u, NULL); CHECK(!gwp_frame_valid(&f));

    /* extended IDs */
    f = frame(CAN_ID_IDE | 0x00000000u, 0u, NULL); CHECK(gwp_frame_valid(&f));
    f = frame(CAN_ID_IDE | 0x1FFFFFFFu, 0u, NULL); CHECK(gwp_frame_valid(&f));

    /* reserved bits 29 and 30 */
    f = frame(0x20000000u, 0u, NULL);              CHECK(!gwp_frame_valid(&f));
    f = frame(0x40000000u, 0u, NULL);              CHECK(!gwp_frame_valid(&f));
    f = frame(CAN_ID_IDE | 0x20000000u, 0u, NULL); CHECK(!gwp_frame_valid(&f));
    f = frame(CAN_ID_IDE | 0x40000000u, 0u, NULL); CHECK(!gwp_frame_valid(&f));
    f = frame(0xFFFFFFFFu, 0u, NULL);              CHECK(!gwp_frame_valid(&f));
}

/* --------------------------------------------------------------- encode */

static void test_gwp_encode_vectors(void)
{
    can_frame_t f[2];
    uint8_t out[GWP_MAX_LEN];

    vec1_frames(f);
    memset(out, 0xCC, sizeof out);
    CHECK(gwp_encode(0x0102u, f, 1u, out, sizeof out) == (int)sizeof k_vec1);
    CHECK(memcmp(out, k_vec1, sizeof k_vec1) == 0);

    vec2_frames(f);
    memset(out, 0xCC, sizeof out);
    CHECK(gwp_encode(0xFFFFu, f, 2u, out, sizeof out) == (int)sizeof k_vec2);
    CHECK(memcmp(out, k_vec2, sizeof k_vec2) == 0);
}

static void test_gwp_encode_zero_padding(void)
{
    /* bytes at index >= DLC must be 0x00 in the packet, whatever f.data holds */
    static const uint8_t junk[] = { 0xDE, 0xAD, 0xBE, 0x11, 0x22, 0x33, 0x44, 0x55 };
    can_frame_t f = frame(0x123u, 8u, junk);
    uint8_t out[GWP_MAX_LEN];

    f.dlc = 3u;
    CHECK(gwp_encode(0x0102u, &f, 1u, out, sizeof out) == (int)sizeof k_vec1);
    CHECK(memcmp(out, k_vec1, sizeof k_vec1) == 0);
}

static void test_gwp_encode_max_frames(void)
{
    can_frame_t f[GWP_MAX_FRAMES];
    uint8_t out[GWP_MAX_LEN];
    bool ok = true;

    for (uint32_t i = 0; i < GWP_MAX_FRAMES; i++) {
        uint8_t d[8];
        for (uint32_t k = 0; k < 8u; k++) {
            d[k] = (uint8_t)(i * 16u + k);
        }
        f[i] = frame(0x100u + i, 8u, d);
    }
    CHECK(gwp_encode(0x1234u, f, GWP_MAX_FRAMES, out, sizeof out) == 112);
    CHECK(out[3] == 8u);
    CHECK(out[4] == 0x12u && out[5] == 0x34u);

    /* check each record's position and content */
    for (uint32_t i = 0; i < GWP_MAX_FRAMES; i++) {
        const uint8_t *r = &out[GWP_HDR_LEN + GWP_REC_LEN * i];
        ok = ok && r[0] == 0u && r[1] == 0u && r[2] == 0x01u && r[3] == (uint8_t)i;
        ok = ok && r[4] == 8u && r[5] == (uint8_t)(i * 16u) && r[12] == (uint8_t)(i * 16u + 7u);
    }
    CHECK(ok);
    CHECK(((uint16_t)(out[110] << 8) | out[111]) == gwp_crc16(out, 110u));
}

static void test_gwp_encode_errors(void)
{
    can_frame_t f[GWP_MAX_FRAMES + 1u];
    uint8_t out[GWP_MAX_LEN + 13u];

    for (uint32_t i = 0; i < GWP_MAX_FRAMES + 1u; i++) {
        f[i] = frame(i, 0u, NULL);
    }

    /* 1: ARG */
    CHECK(gwp_encode(0u, NULL, 1u, out, sizeof out) == GWP_ERR_ARG);
    CHECK(gwp_encode(0u, f, 1u, NULL, sizeof out) == GWP_ERR_ARG);
    CHECK(gwp_encode(0u, f, 0u, out, sizeof out) == GWP_ERR_ARG);
    CHECK(gwp_encode(0u, f, 9u, out, sizeof out) == GWP_ERR_ARG);

    /* 2: LEN, exact size is enough */
    CHECK(gwp_encode(0u, f, 1u, out, GWP_PKT_LEN(1u) - 1u) == GWP_ERR_LEN);
    CHECK(gwp_encode(0u, f, 1u, out, 0u) == GWP_ERR_LEN);
    CHECK(gwp_encode(0u, f, 1u, out, GWP_PKT_LEN(1u)) == (int)GWP_PKT_LEN(1u));
    CHECK(gwp_encode(0u, f, 8u, out, GWP_MAX_LEN - 1u) == GWP_ERR_LEN);

    /* 3: FRAME, invalid frame anywhere in the list */
    f[2].dlc = 9u;
    CHECK(gwp_encode(0u, f, 3u, out, sizeof out) == GWP_ERR_FRAME);
    CHECK(gwp_encode(0u, f, 2u, out, sizeof out) == (int)GWP_PKT_LEN(2u));
    f[2].dlc = 0u;
    f[7].id = 0x800u;
    CHECK(gwp_encode(0u, f, 8u, out, sizeof out) == GWP_ERR_FRAME);

    /* check order: first matching condition wins */
    CHECK(gwp_encode(0u, f, 9u, out, 1u) == GWP_ERR_ARG);   /* ARG before LEN   */
    CHECK(gwp_encode(0u, f, 8u, out, 1u) == GWP_ERR_LEN);   /* LEN before FRAME */
}

/* --------------------------------------------------------------- decode */

static void test_gwp_decode_vectors(void)
{
    can_frame_t exp[2];
    can_frame_t out[GWP_MAX_FRAMES];
    uint16_t seq = 0u;

    vec1_frames(exp);
    CHECK(gwp_decode(k_vec1, sizeof k_vec1, &seq, out, GWP_MAX_FRAMES) == 1);
    CHECK(seq == 0x0102u);
    CHECK(frame_eq(&out[0], &exp[0]));

    vec2_frames(exp);
    CHECK(gwp_decode(k_vec2, sizeof k_vec2, &seq, out, GWP_MAX_FRAMES) == 2);
    CHECK(seq == 0xFFFFu);
    CHECK(frame_eq(&out[0], &exp[0]));
    CHECK(frame_eq(&out[1], &exp[1]));

    /* max_frames equal to N is enough */
    CHECK(gwp_decode(k_vec2, sizeof k_vec2, &seq, out, 2u) == 2);
}

static void test_gwp_decode_zero_padding(void)
{
    /* padding bytes in the packet are non-zero, output must still be 0 */
    uint8_t pkt[sizeof k_vec1];
    can_frame_t out;
    uint16_t seq;

    memcpy(pkt, k_vec1, sizeof pkt);
    for (size_t i = 14u; i < 19u; i++) {
        pkt[i] = 0xA5u;
    }
    fix_crc(pkt, sizeof pkt);
    memset(&out, 0xFF, sizeof out);

    CHECK(gwp_decode(pkt, sizeof pkt, &seq, &out, 1u) == 1);
    CHECK(out.data[0] == 0xDEu && out.data[1] == 0xADu && out.data[2] == 0xBEu);
    CHECK(out.data[3] == 0u && out.data[4] == 0u && out.data[5] == 0u &&
          out.data[6] == 0u && out.data[7] == 0u);
}

static void test_gwp_decode_errors(void)
{
    uint8_t pkt[sizeof k_vec2 + 1u];
    can_frame_t out[GWP_MAX_FRAMES];
    uint16_t seq;
    const size_t n = sizeof k_vec2;

    /* 1: ARG */
    CHECK(gwp_decode(NULL, n, &seq, out, 8u) == GWP_ERR_ARG);
    CHECK(gwp_decode(k_vec2, n, NULL, out, 8u) == GWP_ERR_ARG);
    CHECK(gwp_decode(k_vec2, n, &seq, NULL, 8u) == GWP_ERR_ARG);

    /* 2: LEN, shorter than header */
    CHECK(gwp_decode(k_vec2, 0u, &seq, out, 8u) == GWP_ERR_LEN);
    CHECK(gwp_decode(k_vec2, 5u, &seq, out, 8u) == GWP_ERR_LEN);

    /* 3: MAGIC */
    memcpy(pkt, k_vec2, n); pkt[0] = 0x48u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_MAGIC);
    memcpy(pkt, k_vec2, n); pkt[1] = 0x00u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_MAGIC);

    /* 4: VERSION */
    memcpy(pkt, k_vec2, n); pkt[2] = 0x02u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_VERSION);
    memcpy(pkt, k_vec2, n); pkt[2] = 0x00u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_VERSION);

    /* 5: COUNT */
    memcpy(pkt, k_vec2, n); pkt[3] = 0u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_COUNT);
    memcpy(pkt, k_vec2, n); pkt[3] = 9u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_COUNT);

    /* 6: N > max_frames */
    CHECK(gwp_decode(k_vec2, n, &seq, out, 1u) == GWP_ERR_ARG);
    CHECK(gwp_decode(k_vec2, n, &seq, out, 0u) == GWP_ERR_ARG);

    /* 7: LEN, length does not match N */
    CHECK(gwp_decode(k_vec2, n - 1u, &seq, out, 8u) == GWP_ERR_LEN);
    memcpy(pkt, k_vec2, n); pkt[n] = 0u;
    CHECK(gwp_decode(pkt, n + 1u, &seq, out, 8u) == GWP_ERR_LEN);
    CHECK(gwp_decode(k_vec2, 6u, &seq, out, 8u) == GWP_ERR_LEN);

    /* 8: CRC, corrupted payload or CRC field */
    memcpy(pkt, k_vec2, n); pkt[10] ^= 0x01u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_CRC);
    memcpy(pkt, k_vec2, n); pkt[n - 1u] ^= 0x80u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_CRC);

    /* 9: FRAME, invalid record with correct CRC */
    memcpy(pkt, k_vec2, n); pkt[10] = 9u;                  /* DLC 9 */
    fix_crc(pkt, n);
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_FRAME);
    memcpy(pkt, k_vec2, n); pkt[6] |= 0x20u;               /* reserved bit 29 */
    fix_crc(pkt, n);
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_FRAME);
    memcpy(pkt, k_vec2, n); pkt[21] = 0x08u;               /* std ID 0x8FF */
    fix_crc(pkt, n);
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_FRAME);
}

static void test_gwp_decode_error_order(void)
{
    uint8_t pkt[sizeof k_vec2];
    can_frame_t out[GWP_MAX_FRAMES];
    uint16_t seq;
    const size_t n = sizeof k_vec2;

    /* LEN(<6) before MAGIC */
    memcpy(pkt, k_vec2, n); pkt[0] = 0u;
    CHECK(gwp_decode(pkt, 5u, &seq, out, 8u) == GWP_ERR_LEN);

    /* MAGIC before VERSION */
    memcpy(pkt, k_vec2, n); pkt[0] = 0u; pkt[2] = 0u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_MAGIC);

    /* VERSION before COUNT */
    memcpy(pkt, k_vec2, n); pkt[2] = 0u; pkt[3] = 0u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_VERSION);

    /* COUNT before N > max_frames */
    memcpy(pkt, k_vec2, n); pkt[3] = 9u;
    CHECK(gwp_decode(pkt, n, &seq, out, 0u) == GWP_ERR_COUNT);

    /* N > max_frames before length mismatch */
    CHECK(gwp_decode(k_vec2, n - 1u, &seq, out, 1u) == GWP_ERR_ARG);

    /* length mismatch before CRC */
    memcpy(pkt, k_vec2, n); pkt[3] = 1u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_LEN);

    /* CRC before FRAME */
    memcpy(pkt, k_vec2, n); pkt[10] = 9u;
    CHECK(gwp_decode(pkt, n, &seq, out, 8u) == GWP_ERR_CRC);
}

/* ------------------------------------------------------------ roundtrip */

static void test_gwp_roundtrip(void)
{
    static const uint16_t seqs[] = { 0x0000u, 0x0001u, 0x00FFu, 0x8000u, 0xFFFFu };
    can_frame_t in[GWP_MAX_FRAMES];
    can_frame_t out[GWP_MAX_FRAMES];
    uint8_t pkt[GWP_MAX_LEN];
    bool ok = true;

    for (size_t s = 0; s < sizeof seqs / sizeof seqs[0]; s++) {
        for (size_t n = 1u; n <= GWP_MAX_FRAMES; n++) {
            for (size_t i = 0; i < n; i++) {
                uint8_t d[8];
                uint8_t dlc = (uint8_t)((i + n) % 9u);
                uint32_t id = (i % 2u) ? (CAN_ID_IDE | (0x1ABCDE0u + (uint32_t)i))
                                       : (uint32_t)(0x700u + i);
                for (size_t k = 0; k < 8u; k++) {
                    d[k] = (uint8_t)(s * 31u + i * 7u + k + 1u);
                }
                in[i] = frame(id, dlc, d);
            }
            uint16_t seq = 0u;
            int len = gwp_encode(seqs[s], in, n, pkt, sizeof pkt);
            ok = ok && len == (int)GWP_PKT_LEN(n);
            ok = ok && gwp_decode(pkt, (size_t)len, &seq, out, GWP_MAX_FRAMES) == (int)n;
            ok = ok && seq == seqs[s];
            for (size_t i = 0; i < n; i++) {
                ok = ok && frame_eq(&in[i], &out[i]);
            }
        }
    }
    CHECK(ok);
}

void test_gwproto(void)
{
    printf("===== gwproto =====\n");
    RUN_TEST(test_gwp_crc16,
             "CRC-16/CCITT-FALSE check value, empty input and spec vectors");
    RUN_TEST(test_gwp_frame_valid,
             "DLC range, standard/extended ID limits and reserved bits");
    RUN_TEST(test_gwp_encode_vectors,
             "encode matches spec test vectors 1 and 2 byte for byte");
    RUN_TEST(test_gwp_encode_zero_padding,
             "encode writes 0x00 for data bytes at index >= DLC");
    RUN_TEST(test_gwp_encode_max_frames,
             "encode 8 frames: 112 bytes, record layout and CRC");
    RUN_TEST(test_gwp_encode_errors,
             "encode ARG/LEN/FRAME errors and their check order");
    RUN_TEST(test_gwp_decode_vectors,
             "decode spec test vectors 1 and 2");
    RUN_TEST(test_gwp_decode_zero_padding,
             "decode sets output data bytes at index >= DLC to 0");
    RUN_TEST(test_gwp_decode_errors,
             "decode returns the right error for each invalid input");
    RUN_TEST(test_gwp_decode_error_order,
             "decode reports the first matching error in spec order");
    RUN_TEST(test_gwp_roundtrip,
             "encode -> decode round-trip for N = 1..8 and edge sequence numbers");
}
