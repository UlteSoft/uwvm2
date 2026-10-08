/* Authentic primitive parameter ABI; no host or guest I/O. */
#ifdef __cplusplus
#define SOURCE_BOOL bool
extern "C" {
#else
#define SOURCE_BOOL _Bool
#endif
static volatile unsigned SOURCE_PRIMITIVE_OBSERVED;
__attribute__((noinline)) void source_primitive_probe(
    signed char s8_min,
    signed char s8_max,
    unsigned char u8_zero,
    unsigned char u8_max,
    short s16_min,
    short s16_max,
    unsigned short u16_zero,
    unsigned short u16_max,
    int s32_min,
    int s32_max,
    unsigned u32_zero,
    unsigned u32_max,
    long long s64_min,
    long long s64_max,
    unsigned long long u64_zero,
    unsigned long long u64_max,
    long slong_min,
    long slong_max,
    unsigned long ulong_zero,
    unsigned long ulong_max,
    SOURCE_BOOL enabled,
    SOURCE_BOOL disabled)
{
    SOURCE_PRIMITIVE_OBSERVED = 1; /* SOURCE_PRIMITIVE_DAP_STOP */
    if (s8_min != (signed char)(-128)) __builtin_trap();
    if (s8_max != (signed char)(127)) __builtin_trap();
    if (u8_zero != (unsigned char)(0)) __builtin_trap();
    if (u8_max != (unsigned char)(255)) __builtin_trap();
    if (s16_min != (short)(-32768)) __builtin_trap();
    if (s16_max != (short)(32767)) __builtin_trap();
    if (u16_zero != (unsigned short)(0)) __builtin_trap();
    if (u16_max != (unsigned short)(65535)) __builtin_trap();
    if (s32_min != (int)((-2147483647 - 1))) __builtin_trap();
    if (s32_max != (int)(2147483647)) __builtin_trap();
    if (u32_zero != (unsigned)(0u)) __builtin_trap();
    if (u32_max != (unsigned)(4294967295u)) __builtin_trap();
    if (s64_min != (long long)((-9223372036854775807LL - 1LL))) __builtin_trap();
    if (s64_max != (long long)(9223372036854775807LL)) __builtin_trap();
    if (u64_zero != (unsigned long long)(0ull)) __builtin_trap();
    if (u64_max != (unsigned long long)(18446744073709551615ull)) __builtin_trap();
    if (slong_min != (long)((-__LONG_MAX__ - 1L))) __builtin_trap();
    if (slong_max != (long)(__LONG_MAX__)) __builtin_trap();
    if (ulong_zero != (unsigned long)(0UL)) __builtin_trap();
    if (ulong_max != (unsigned long)((~0UL))) __builtin_trap();
    if (enabled != (SOURCE_BOOL)(1)) __builtin_trap();
    if (disabled != (SOURCE_BOOL)(0)) __builtin_trap();
}
void _start(void)
{
    source_primitive_probe(
        (signed char)(-128),
        (signed char)(127),
        (unsigned char)(0),
        (unsigned char)(255),
        (short)(-32768),
        (short)(32767),
        (unsigned short)(0),
        (unsigned short)(65535),
        (int)((-2147483647 - 1)),
        (int)(2147483647),
        (unsigned)(0u),
        (unsigned)(4294967295u),
        (long long)((-9223372036854775807LL - 1LL)),
        (long long)(9223372036854775807LL),
        (unsigned long long)(0ull),
        (unsigned long long)(18446744073709551615ull),
        (long)((-__LONG_MAX__ - 1L)),
        (long)(__LONG_MAX__),
        (unsigned long)(0UL),
        (unsigned long)((~0UL)),
        (SOURCE_BOOL)(1),
        (SOURCE_BOOL)(0));
    if (SOURCE_PRIMITIVE_OBSERVED != 1) __builtin_trap();
}
#ifdef __cplusplus
}
#endif
