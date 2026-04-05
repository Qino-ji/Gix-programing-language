#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
