#include <stdio.h>

#include "test.h"

int g_run;
int g_failed;

int main(void)
{
    test_canbuf();
    test_gwproto();

    printf("\n%d checks, %d passed, %d failed -> %s\n",
           g_run, g_run - g_failed, g_failed, g_failed ? "FAILED" : "PASSED");
    return g_failed ? 1 : 0;
}
