#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Untimed validation oracle, never a performance comparator or Guest adapter. */
int main(int argc, char **argv)
{
    if (argc != 2 || strlen(argv[1]) == 0 || strlen(argv[1]) > 10) return 2;
    uint64_t iterations = 0;
    for (char const *p = argv[1]; *p; ++p) {
        if (*p < '0' || *p > '9') return 2;
        iterations = iterations * 10 + (unsigned)(*p - '0');
    }
    if (iterations == 0 || iterations > UINT64_C(2000000000)) return 2;
    uint64_t value = 7;
    for (uint64_t index = 0; index < iterations; ++index)
        value = ((value * UINT64_C(33) + index) % UINT64_C(4294967296)) ^ UINT64_C(0x55aa);
    printf("lcg32 iterations=%" PRIu64 " checksum=%" PRIu64 "\n", iterations, value);
    return ferror(stdout) ? 4 : 0;
}
