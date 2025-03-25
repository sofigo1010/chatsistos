#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <time.h>
#include <stdio.h>

static inline const char* debug_timestamp(void) {
    static __thread char buff[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", tm_info);
    return buff;
}

#endif
