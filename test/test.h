/* test.h - Minimal test helpers shared by all test files. */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>

extern int g_run;
extern int g_failed;

#define CHECK(cond)                                                     \
    do {                                                                \
        g_run++;                                                        \
        if (!(cond)) {                                                  \
            g_failed++;                                                 \
            printf("         FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

/* Run one test case and print its name, description and result. */
#define RUN_TEST(fn, desc)                                              \
    do {                                                                \
        int run0_ = g_run;                                              \
        int failed0_ = g_failed;                                        \
        printf("[ RUN  ] %s\n         %s\n", #fn, desc);                \
        fn();                                                           \
        int run_ = g_run - run0_;                                       \
        int failed_ = g_failed - failed0_;                              \
        if (failed_ == 0) {                                             \
            printf("[ PASS ] %s (%d checks)\n", #fn, run_);             \
        } else {                                                        \
            printf("[ FAIL ] %s (%d of %d checks failed)\n",            \
                   #fn, failed_, run_);                                 \
        }                                                               \
    } while (0)

void test_canbuf(void);
void test_gwproto(void);

#endif /* TEST_H */
