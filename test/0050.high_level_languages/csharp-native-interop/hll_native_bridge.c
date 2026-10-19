#include "hll_native_bridge.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(int32_t) == 4 && sizeof(uint64_t) == 8, "fixed ABI widths");

static int no_overflow(uint64_t offset, uint64_t length)
{
    return length <= UINT64_MAX - offset;
}

HLL_PUBLIC uint64_t hll_bridge_copy(int32_t memory_index, uint64_t input_offset,
    uint64_t input_length, uint64_t output_offset, uint64_t capacity)
{
    /* Validate every numeric extent before narrowing or touching output. */
    if (memory_index != 0 || input_length > capacity ||
        !no_overflow(input_offset, input_length) ||
        !no_overflow(output_offset, capacity)) return 0;
#if defined(__wasm32__)
    const uint64_t limit = (uint64_t)__builtin_wasm_memory_size(0) * UINT64_C(65536);
    if (input_offset > limit || input_length > limit - input_offset ||
        output_offset > limit || capacity > limit - output_offset) return 0;
#else
    /* Native oracle receives actual AllocHGlobal pointers, not a virtual
       linear-memory extent. It cannot certify Wasm bounds; those tests are
       explicitly skipped by the same managed program on 64-bit native. */
    _Static_assert(sizeof(uintptr_t) == 8 && sizeof(size_t) == 8, "64-bit native oracle only");
    if (input_offset == 0 || output_offset == 0) return 0;
#endif
    if (input_length != 0)
        memmove((void *)(uintptr_t)output_offset, (const void *)(uintptr_t)input_offset,
                (size_t)input_length);
    return HLL_TOKEN;
}

HLL_PUBLIC uint64_t hll_roundtrip_u64(uint64_t value)
{
    return value;
}
