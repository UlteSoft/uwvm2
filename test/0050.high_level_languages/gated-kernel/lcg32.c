#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Gated performance fixture; reference.c uses a separate wider-integer oracle. */
__attribute__((noinline)) static uint32_t kernel(uint32_t iterations)
{
    uint32_t value = 7;
    for (uint32_t index = 0; index < iterations; ++index)
        value = (value * UINT32_C(33) + index) ^ UINT32_C(0x55aa);
    return value;
}

static void fail(char const *message, int status)
{
    fprintf(stderr, "lcg32 %s\n", message);
    exit(status);
}

static uint32_t parse_iterations(int argc, char **argv)
{
    if (argc == 1) return UINT32_C(2000000000);
    if (argc != 2 || strlen(argv[1]) == 0 || strlen(argv[1]) > 10)
        fail("invalid iterations", 2);
    uint64_t value = 0;
    for (char const *p = argv[1]; *p; ++p) {
        if (*p < '0' || *p > '9') fail("invalid iterations", 2);
        value = value * 10 + (unsigned)(*p - '0');
    }
    if (value == 0 || value > UINT32_C(2000000000)) fail("invalid iterations", 2);
    return (uint32_t)value;
}

static uint64_t now_ns(void)
{
    struct timespec stamp;
    if (clock_gettime(CLOCK_MONOTONIC, &stamp) != 0) fail("clock unavailable", 4);
    return (uint64_t)stamp.tv_sec * UINT64_C(1000000000) + (uint64_t)stamp.tv_nsec;
}

static void marker(char const *text)
{
    if (fputs(text, stderr) == EOF || fflush(stderr) != 0) fail("marker output failed", 4);
}

static void gate(int wanted)
{
    if (getchar() != wanted) fail("invalid gate", 3);
}

int main(int argc, char **argv)
{
    uint32_t iterations = parse_iterations(argc, argv);
    marker("UWVM_PROFILE_READY\n");
    gate('G');
    uint64_t start = now_ns();
    uint32_t checksum = kernel(iterations);
    marker("UWVM_PROFILE_DONE\n");
    uint64_t elapsed = now_ns() - start;
    gate('D');
    printf("lcg32 iterations=%" PRIu32 " checksum=%" PRIu32 "\n", iterations, checksum);
    printf("lcg32 kernel ns=%" PRIu64 "\n", elapsed);
    return ferror(stdout) ? 4 : 0;
}
