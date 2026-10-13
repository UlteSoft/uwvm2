/* Authentic primitive floating arguments; their bits are checked by the guest. */
static volatile unsigned FLOAT_OBSERVED;
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((noinline)) void source_float_probe(
    float f_finite,
    float f_negative,
    float f_zero,
    float f_minus_zero,
    float f_maximum,
    float f_normal_min,
    float f_subnormal,
    float f_infinity,
    float f_negative_infinity,
    float f_nan,
    float f_negative_nan,
    float f_negative_subnormal,
    double d_finite,
    double d_negative,
    double d_zero,
    double d_minus_zero,
    double d_maximum,
    double d_normal_min,
    double d_subnormal,
    double d_infinity,
    double d_negative_infinity,
    double d_nan,
    double d_negative_nan,
    double d_negative_subnormal) {
    FLOAT_OBSERVED = 1; /* SOURCE_FLOAT_DAP_STOP */
    unsigned actual32;
    unsigned long long actual64;
    __builtin_memcpy(&actual32, &f_finite, sizeof actual32);
    if (actual32 != 0x3fa00000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_negative, sizeof actual32);
    if (actual32 != 0xbfa00000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_zero, sizeof actual32);
    if (actual32 != 0x00000000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_minus_zero, sizeof actual32);
    if (actual32 != 0x80000000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_maximum, sizeof actual32);
    if (actual32 != 0x7f7fffffu) __builtin_trap();
    __builtin_memcpy(&actual32, &f_normal_min, sizeof actual32);
    if (actual32 != 0x00800000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_subnormal, sizeof actual32);
    if (actual32 != 0x00000001u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_infinity, sizeof actual32);
    if (actual32 != 0x7f800000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_negative_infinity, sizeof actual32);
    if (actual32 != 0xff800000u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_nan, sizeof actual32);
    if (actual32 != 0x7fc12345u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_negative_nan, sizeof actual32);
    if (actual32 != 0xffc23456u) __builtin_trap();
    __builtin_memcpy(&actual32, &f_negative_subnormal, sizeof actual32);
    if (actual32 != 0x80000001u) __builtin_trap();
    __builtin_memcpy(&actual64, &d_finite, sizeof actual64);
    if (actual64 != 0x3ff4000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_negative, sizeof actual64);
    if (actual64 != 0xbff4000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_zero, sizeof actual64);
    if (actual64 != 0x0000000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_minus_zero, sizeof actual64);
    if (actual64 != 0x8000000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_maximum, sizeof actual64);
    if (actual64 != 0x7fefffffffffffffull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_normal_min, sizeof actual64);
    if (actual64 != 0x0010000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_subnormal, sizeof actual64);
    if (actual64 != 0x0000000000000001ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_infinity, sizeof actual64);
    if (actual64 != 0x7ff0000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_negative_infinity, sizeof actual64);
    if (actual64 != 0xfff0000000000000ull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_nan, sizeof actual64);
    if (actual64 != 0x7ff8123456789abcull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_negative_nan, sizeof actual64);
    if (actual64 != 0xfff823456789abcdull) __builtin_trap();
    __builtin_memcpy(&actual64, &d_negative_subnormal, sizeof actual64);
    if (actual64 != 0x8000000000000001ull) __builtin_trap();
}
void _start(void) {
    unsigned bits32[] = {0x3fa00000u, 0xbfa00000u, 0x00000000u, 0x80000000u, 0x7f7fffffu, 0x00800000u, 0x00000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc23456u, 0x80000001u};
    unsigned long long bits64[] = {0x3ff4000000000000ull, 0xbff4000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull, 0x7fefffffffffffffull, 0x0010000000000000ull, 0x0000000000000001ull, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8123456789abcull, 0xfff823456789abcdull, 0x8000000000000001ull};
    float f[12];
    double d[12];
    for (unsigned i = 0; i != 12; ++i) {
        __builtin_memcpy(f + i, bits32 + i, sizeof(float));
        __builtin_memcpy(d + i, bits64 + i, sizeof(double));
    }
    source_float_probe(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11]);
    if (FLOAT_OBSERVED != 1) __builtin_trap();
}
#ifdef __cplusplus
}
#endif
