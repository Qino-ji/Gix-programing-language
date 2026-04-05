#ifndef VIX_IMPORT_H
#define VIX_IMPORT_H

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void* checked_malloc(size_t size) {
    void* ptr = malloc(size);
    assert(ptr != NULL);
    return ptr;
}

static inline void* checked_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    assert(new_ptr != NULL);
    return new_ptr;
}

#endif
