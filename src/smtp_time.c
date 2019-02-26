#include "smtp_time.h"

struct timespec smtpgettime()
{
    struct timespec result;
    clock_gettime(CLOCK_MONOTONIC, &result);
    return result;
}
