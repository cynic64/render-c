#ifndef TIMER_H
#define TIMER_H

#include <time.h>

struct timespec timer_start() {
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &time);
        return time;
}

double timer_get_elapsed(const struct timespec* start_time) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        return (double) ((now.tv_sec * 1000000000 + now.tv_nsec)
                       - (start_time->tv_sec * 1000000000 + start_time->tv_nsec)) / 1000000000.0F;
}

#endif // TIMER_H

