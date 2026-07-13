#include <stdio.h>
#include <stdlib.h>
#include "bane.h"

void ensure_fail(const char *file, int line, const char *func, const char *expr) {
    fprintf(stderr, "%s:%i %s ENSURE (%s) failed.\n", file, line, func, expr);
    abort();
}