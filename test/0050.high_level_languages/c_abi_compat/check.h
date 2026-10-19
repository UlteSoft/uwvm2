#ifndef UWVM_WASI_C_ABI_COMPAT_CHECK_H
#define UWVM_WASI_C_ABI_COMPAT_CHECK_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" char **environ;
#else
extern char **environ;
#endif

#define ABI_WORLD "\xe4\xb8\x96\xe7\x95\x8c"

static int check_abi(int argc, char **argv, const char *language)
{
    assert(argc == 1 || argc == 2 || argc == 4);
    assert(argv != NULL && argv[0] != NULL && argv[0][0] != '\0');
    assert(argv[argc] == NULL);
    if(argc >= 2) { assert(strcmp(argv[1], ABI_WORLD) == 0); }
    if(argc == 4)
    {
        assert(strcmp(argv[2], "") == 0);
        assert(strcmp(argv[3], "tail") == 0);
    }

    const char *a = getenv("ABI_A");
    const char *empty = getenv("ABI_EMPTY");
    const char *utf8 = getenv("ABI_UTF8");
    size_t expected_count = 0;
    if(a != NULL) { assert(strcmp(a, "one") == 0); ++expected_count; }
    if(empty != NULL) { assert(strcmp(empty, "") == 0); ++expected_count; }
    if(utf8 != NULL) { assert(strcmp(utf8, ABI_WORLD) == 0); ++expected_count; }
    assert(expected_count == 0 || expected_count == 1 || expected_count == 3);
    if(expected_count == 1) { assert(a != NULL); }

    assert(environ != NULL);
    size_t count = 0;
    while(environ[count] != NULL)
    {
        assert(count < 3);
        assert(strcmp(environ[count], "ABI_A=one") == 0 ||
               strcmp(environ[count], "ABI_EMPTY=") == 0 ||
               strcmp(environ[count], "ABI_UTF8=" ABI_WORLD) == 0);
        ++count;
    }
    assert(count == expected_count && environ[count] == NULL);
    printf("c-abi compat: PASS language=%s argc=%d env=%zu\n", language, argc, count);
    return 0;
}
#endif
