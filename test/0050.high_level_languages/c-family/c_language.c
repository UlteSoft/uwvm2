#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct counter {
    int value;
    int (*add)(struct counter *, int);
};

static int counter_add(struct counter *self, int amount) {
    return self->value += amount;
}

__attribute__((noinline)) int debug_value(void) {
    int value = 40;
    value += 2; /* DEBUG_BEFORE */
    return value; /* DEBUG_AFTER */
}

int main(void) {
    struct counter object = {40, counter_add};
    int numbers[10];
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        numbers[i] = i + 1;
        sum += numbers[i];
    }
    const char *text = "\xcf\x80\xe4\xbd\xa0\xe5\xa5\xbd\xf0\x9f\x99\x82";
    uint32_t checksum = 7;
    for (uint32_t i = 0; i < 100; ++i)
        checksum = (checksum * UINT32_C(33) + i) ^ UINT32_C(0x55aa);
    int classes = object.add(&object, 2);
    int value = debug_value();
    if (classes != 42 || sum != 55 || strlen(text) != 12 || value != 42)
        return 2;
    printf("c foundation classes=%d arrays=%d utf8=%zu value=%d checksum=%" PRIu32 "\n",
           classes, sum, strlen(text), value, checksum);
    return 0;
}
