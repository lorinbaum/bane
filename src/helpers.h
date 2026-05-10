#ifndef SPIRITHELPERS_H
#define SPIRITHELPERS_H

#include <assert.h>
#include <stdarg.h>
#include "spiritlib.h"

#if __STDC_VERSION__ >= 202311L
#  define TYPEOF typeof
#else
// legacy GCC / Clang
#  define TYPEOF __typeof__
#endif

#define ARRAY_AUTO_EXPAND(STRUCT) \
    static inline void auto_expand_##STRUCT(STRUCT *a) { \
        if (a->count >= a->cap) { \
            a->cap = a->cap > 0 ? a->cap * 2 : 16; \
            a->items = realloc(a->items, sizeof(a->items[0]) * a->cap); \
            ensure(a->items != NULL); \
        }\
    }

#define ARRAY_APPEND(STRUCT) \
    static inline void append_##STRUCT(STRUCT *a, TYPEOF(*a->items) item) { \
        auto_expand_##STRUCT(a); \
        a->items[a->count] = item; \
        a->count++; \
    }
    
#define ARRAY_DELETE(STRUCT) \
    static inline void delete_##STRUCT(STRUCT *a, int index) { \
        assert(index < a->count && index >= 0); \
        for (int i = index; i + 1 < a->count; i++) { a->items[i] = a->items[i+1]; } \
        a->count--; \
    }

#define ARRAY_INSERT(STRUCT) \
    static inline void insert_##STRUCT(STRUCT *a, int index, TYPEOF(*a->items) item) { \
        assert(index <= a->count && index >= 0); \
        auto_expand_##STRUCT(a); \
        for (int i = a->count; i > index; i--) { a->items[i] = a->items[i-1]; } \
        a->items[index] = item; \
        a->count++; \
    }

static inline int imin(int a, int b) { return a > b ? b : a; }
static inline int imax(int a, int b) { return a > b ? a : b; }

SS_Status report_status(const char *file, int line, const char *code_str, int code, const char *f, const char *message, ...);

#define report(CODE, ...) report_status(__FILE__, __LINE__, #CODE, CODE, __func__, __VA_ARGS__)

void ensure_fail(const char *file, int line, const char *func, char *expr);
#define ensure(expr) ((expr) ? (void) (0) : ensure_fail(__FILE__, __LINE__, __func__, #expr))

#endif