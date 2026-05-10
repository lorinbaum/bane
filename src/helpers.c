#include <stdio.h>
#include <stdlib.h>
#include "helpers.h"

SS_Status report_status(const char *file, int line, const char *code_str, int code, const char *f, const char *message, ...) {
    SS_Status status = {.code = code};
    int len = snprintf(status.message, STATUS_LENGTH, "%s:%i %s: %s: ", file, line, f, code_str);
    if ((STATUS_LENGTH - len) > 0) {
        va_list args;
        va_start(args, message);
        vsnprintf(status.message + len, STATUS_LENGTH-len, message, args);
        va_end(args);
    }
    return status;
}

void ensure_fail(const char *file, int line, const char *func, char *expr) {
    fprintf(stderr, "%s:%i %s ENSURE (%s) failed.\n", file, line, func, expr);
    abort();
}