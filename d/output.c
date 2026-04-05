#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
typedef struct { char* ptr; int64_t len; } String;

typedef struct Math {
    String name;
    int32_t age;
} Math;

typedef struct {
    uint8_t tag;
    union {
        String ok;
        int32_t err;
    } data;
} Result_str_int32;


int32_t main() {
String t1 = { .ptr = "Hello", .len = 5 };
int32_t t2 = 10;
int32_t t5 = 0;
return t5;
}

Math Math_new() {
    Math instance;
String t6 = { .ptr = "", .len = 0 };
    instance.name = t6;
int32_t t7 = 0;
    instance.age = t7;
    return instance;
}

void Math_add(const Math* self, String input_name, int32_t input_age) {
int32_t t8 = 0;
int32_t t9 = 0;
    return;
}

typedef struct {
    uint8_t tag;
    union {
        Math* ok;
        String err;
    } data;
} Result__Math_str;


Result_str_int32 Math_return_data(const Math* self) {
Result__Math_str t10 = { .tag = 0, .data.ok = self };
return t10;
}

