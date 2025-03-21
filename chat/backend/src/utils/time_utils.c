#include "time_utils.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

char *get_timestamp(void) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    // Reserva 20 bytes: "YYYY-MM-DD HH:MM:SS" + '\0'
    char *buffer = malloc(20);
    if (buffer) {
        strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    }
    return buffer;
}
