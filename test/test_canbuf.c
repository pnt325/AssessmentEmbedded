#include <stdbool.h>
#include <stdint.h>

#include "canbuf.h"
#include "test.h"

static can_frame_t make_frame(uint32_t id, uint8_t seed)
{
    can_frame_t f;
    f.id = id;
    f.dlc = 8u;
    for (uint8_t i = 0; i < 8u; i++) {
        f.data[i] = (uint8_t)(seed + i);
    }
    return f;
}

static bool frame_eq(const can_frame_t *a, const can_frame_t *b)
{
    if (a->id != b->id || a->dlc != b->dlc) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}

static void test_canbuf_init(void)
{
    canbuf_t b;
    b.head = 5u;
    b.tail = 3u;
    b.drops = 7u;
    canbuf_init(&b);
    CHECK(canbuf_count(&b) == 0u);
    CHECK(canbuf_drops(&b) == 0u);
}

static void test_canbuf_null_args(void)
{
    canbuf_t b;
    can_frame_t f = make_frame(0x123u, 0u);
    canbuf_init(&b);

    canbuf_init(NULL); /* must not crash */
    CHECK(!canbuf_push(NULL, &f));
    CHECK(!canbuf_push(&b, NULL));
    CHECK(!canbuf_pop(NULL, &f));
    CHECK(canbuf_push(&b, &f));
    CHECK(!canbuf_pop(&b, NULL));
    CHECK(canbuf_count(&b) == 1u); /* failed pop must not consume */
    CHECK(canbuf_count(NULL) == 0u);
    CHECK(canbuf_drops(NULL) == 0u);
}

static void test_canbuf_pop_empty(void)
{
    canbuf_t b;
    can_frame_t out = make_frame(0xAAu, 0xAAu);
    can_frame_t orig = out;
    canbuf_init(&b);

    CHECK(!canbuf_pop(&b, &out));
    CHECK(frame_eq(&out, &orig)); /* output untouched */
    CHECK(canbuf_count(&b) == 0u);
}

static void test_canbuf_push_pop_single(void)
{
    canbuf_t b;
    can_frame_t in = make_frame(CAN_ID_IDE | 0x1ABCDEFu, 0x10u);
    can_frame_t out = { 0 };
    canbuf_init(&b);

    CHECK(canbuf_push(&b, &in));
    CHECK(canbuf_count(&b) == 1u);
    CHECK(canbuf_pop(&b, &out));
    CHECK(frame_eq(&in, &out));
    CHECK(canbuf_count(&b) == 0u);
    CHECK(!canbuf_pop(&b, &out));
}

static void test_canbuf_fifo_order(void)
{
    canbuf_t b;
    can_frame_t f;
    canbuf_init(&b);

    for (uint32_t i = 0; i < 5u; i++) {
        f = make_frame(0x100u + i, (uint8_t)i);
        CHECK(canbuf_push(&b, &f));
    }
    CHECK(canbuf_count(&b) == 5u);
    for (uint32_t i = 0; i < 5u; i++) {
        can_frame_t exp = make_frame(0x100u + i, (uint8_t)i);
        CHECK(canbuf_pop(&b, &f));
        CHECK(frame_eq(&f, &exp));
    }
}

static void test_canbuf_full_and_drops(void)
{
    canbuf_t b;
    can_frame_t f;
    canbuf_init(&b);

    /* all CANBUF_SIZE slots are usable */
    for (uint32_t i = 0; i < CANBUF_SIZE; i++) {
        f = make_frame(i, (uint8_t)i);
        CHECK(canbuf_push(&b, &f));
    }
    CHECK(canbuf_count(&b) == CANBUF_SIZE);
    CHECK(canbuf_drops(&b) == 0u);

    /* further pushes are rejected and counted */
    f = make_frame(0x7FFu, 0xFFu);
    CHECK(!canbuf_push(&b, &f));
    CHECK(!canbuf_push(&b, &f));
    CHECK(!canbuf_push(&b, &f));
    CHECK(canbuf_drops(&b) == 3u);
    CHECK(canbuf_count(&b) == CANBUF_SIZE);

    /* rejected frames did not overwrite stored ones */
    for (uint32_t i = 0; i < CANBUF_SIZE; i++) {
        can_frame_t exp = make_frame(i, (uint8_t)i);
        CHECK(canbuf_pop(&b, &f));
        CHECK(frame_eq(&f, &exp));
    }
    CHECK(canbuf_count(&b) == 0u);

    /* space is available again, drops are kept */
    CHECK(canbuf_push(&b, &f));
    CHECK(canbuf_drops(&b) == 3u);
}

static void test_canbuf_wraparound(void)
{
    canbuf_t b;
    can_frame_t f;
    uint32_t next_in = 0u;
    uint32_t next_out = 0u;
    bool ok = true;
    canbuf_init(&b);

    /* interleave pushes and pops so indices wrap the ring many times */
    for (int round = 0; round < 100; round++) {
        for (int k = 0; k < 3; k++) {
            f = make_frame(next_in, (uint8_t)next_in);
            ok = ok && canbuf_push(&b, &f);
            next_in++;
        }
        for (int k = 0; k < 2; k++) {
            can_frame_t exp = make_frame(next_out, (uint8_t)next_out);
            ok = ok && canbuf_pop(&b, &f) && frame_eq(&f, &exp);
            next_out++;
        }
        /* drain whenever near full to keep the ring from overflowing */
        if (canbuf_count(&b) >= CANBUF_SIZE - 3u) {
            while (canbuf_pop(&b, &f)) {
                can_frame_t exp = make_frame(next_out, (uint8_t)next_out);
                ok = ok && frame_eq(&f, &exp);
                next_out++;
            }
        }
    }
    CHECK(ok);
    CHECK(canbuf_count(&b) == next_in - next_out);
    CHECK(canbuf_drops(&b) == 0u);
}

static void test_canbuf_index_overflow(void)
{
    canbuf_t b;
    can_frame_t f;
    canbuf_init(&b);

    /* start just before uint32_t overflow of head/tail */
    b.head = 0xFFFFFFF8u;
    b.tail = 0xFFFFFFF8u;
    for (uint32_t i = 0; i < CANBUF_SIZE; i++) {
        f = make_frame(i, (uint8_t)i);
        CHECK(canbuf_push(&b, &f));
    }
    CHECK(canbuf_count(&b) == CANBUF_SIZE);
    CHECK(!canbuf_push(&b, &f));
    CHECK(canbuf_drops(&b) == 1u);
    for (uint32_t i = 0; i < CANBUF_SIZE; i++) {
        can_frame_t exp = make_frame(i, (uint8_t)i);
        CHECK(canbuf_pop(&b, &f));
        CHECK(frame_eq(&f, &exp));
    }
    CHECK(canbuf_count(&b) == 0u);
    CHECK(!canbuf_pop(&b, &f));
}

void test_canbuf(void)
{
    printf("===== canbuf =====\n");
    RUN_TEST(test_canbuf_init,
             "init resets count and drop counter");
    RUN_TEST(test_canbuf_null_args,
             "NULL arguments are rejected without side effects");
    RUN_TEST(test_canbuf_pop_empty,
             "pop on empty buffer fails and leaves output untouched");
    RUN_TEST(test_canbuf_push_pop_single,
             "single frame round-trips with id, dlc and all data bytes");
    RUN_TEST(test_canbuf_fifo_order,
             "frames are popped in the order they were pushed");
    RUN_TEST(test_canbuf_full_and_drops,
             "all slots usable; pushes to a full buffer are dropped and counted");
    RUN_TEST(test_canbuf_wraparound,
             "interleaved push/pop wraps the ring many times without corruption");
    RUN_TEST(test_canbuf_index_overflow,
             "head/tail uint32_t overflow keeps count and slot mapping correct");
}
