#ifndef HLL_NATIVE_BRIDGE_H
#define HLL_NATIVE_BRIDGE_H
#include <stdint.h>
#define HLL_TOKEN UINT64_C(0xFEDCBA9876543210)
#if defined(__GNUC__) || defined(__clang__)
#define HLL_PUBLIC __attribute__((visibility("default"), noinline))
#else
#define HLL_PUBLIC
#endif
HLL_PUBLIC uint64_t hll_bridge_copy(int32_t memory_index, uint64_t input_offset,
    uint64_t input_length, uint64_t output_offset, uint64_t capacity);
HLL_PUBLIC uint64_t hll_roundtrip_u64(uint64_t value);
#endif
