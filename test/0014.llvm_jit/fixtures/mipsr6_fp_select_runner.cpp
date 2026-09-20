// Freestanding Linux O32 runner for the UNMODIFIED independent backend input.
// Build this TU separately: its integer-only oracle must not optimize together
// with the failing FP function. No host library or floating-point oracle is used.
using bits = unsigned long long;
using vector = double __attribute__((vector_size(16)));
struct pair { bits lane[2]; };
extern "C" vector mipsr6_pmin_after_select(vector, vector, bool);
extern "C" double mipsr6_spill(double);
#ifdef UWVM_TEST_MIPS_JIT
extern "C" void mipsr6_wasm_pmin(unsigned char*, unsigned char const*, unsigned char const*, unsigned);
#endif

static bool less(bits x, bits y)
{
    constexpr bits sign{1ull << 63u}, magnitude{sign - 1u}, infinity{0x7ff0000000000000ull};
    auto ax{x & magnitude}, ay{y & magnitude};
    if(ax > infinity || ay > infinity || (ax == 0u && ay == 0u)) { return false; }
    if((x ^ y) & sign) { return (x & sign) != 0u; }
    return (x & sign) ? x > y : x < y;
}

static bool check(pair a, pair b, bool condition)
{
    auto output{__builtin_bit_cast(pair, mipsr6_pmin_after_select(
        __builtin_bit_cast(vector, a), __builtin_bit_cast(vector, b), condition))};
#ifdef UWVM_TEST_MIPS_JIT
    // Wasm lanes use little-endian bytes even on a big-endian MIPS host.
    unsigned char left[16], right[16], result[16];
    for(unsigned i{}; i != 16u; ++i)
    {
        left[i] = static_cast<unsigned char>(a.lane[i / 8u] >> (8u * (i % 8u)));
        right[i] = static_cast<unsigned char>(b.lane[i / 8u] >> (8u * (i % 8u)));
    }
    mipsr6_wasm_pmin(result, left, right, condition);
#endif
    for(unsigned lane{}; lane != 2u; ++lane)
    {
        if(__builtin_bit_cast(bits, mipsr6_spill(__builtin_bit_cast(double, a.lane[lane]))) != a.lane[lane]) { return false; }
        bits selected{condition ? b.lane[lane] : a.lane[lane]};
        bits expected{less(a.lane[lane], selected) ? a.lane[lane] : selected};
        // Selection preserves sNaN payloads and the chosen sign of zero. An
        // arithmetic-NaN equivalence check would hide precisely these failures.
        if(output.lane[lane] != expected) { return false; }
#ifdef UWVM_TEST_MIPS_JIT
        bits jit{};
        for(unsigned byte{}; byte != 8u; ++byte) { jit |= bits{result[lane * 8u + byte]} << (8u * byte); }
        if(jit != expected) { return false; }
#endif
    }
    return true;
}

static int run()
{
    constexpr bits cases[]{0u, 1ull << 63u, 1u, 0x8000000000000001ull,
        0x000fffffffffffffull, 0x800fffffffffffffull, 0x0010000000000000ull,
        0x8010000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull,
        0x7fefffffffffffffull, 0xffefffffffffffffull, 0x7ff0000000000000ull,
        0xfff0000000000000ull, 0x7ff8000000000000ull, 0xfff8000000000000ull,
        0x7ff0000000000001ull, 0xfff0000000000001ull, 0x7ff123456789abcdull,
        0xfffabcde12345678ull};
    constexpr unsigned count{sizeof(cases) / sizeof(cases[0])};
    for(unsigned i{}; i != count; ++i) for(unsigned j{}; j != count; ++j)
    {
        pair a{{cases[i], cases[(i + 7u) % count]}};
        pair b{{cases[j], cases[(j + 13u) % count]}};
        if(!check(a, b, false) || !check(a, b, true)) { return 1; }
    }
    bits state{0xfedcba9876543210ull};
    auto random{[&]() { state ^= state << 13u; state ^= state >> 7u; state ^= state << 17u; return state; }};
    for(unsigned i{}; i != 100000u; ++i)
    {
        pair a{{random(), random()}}, b{{random(), random()}};
        if(!check(a, b, false) || !check(a, b, true)) { return 2; }
    }
    return 0;
}

extern "C" [[noreturn]] void __start()
{
    static_assert(sizeof(void*) == 4u, "This runner uses the Linux MIPS O32 syscall ABI");
    register unsigned status asm("$4"){static_cast<unsigned>(run())};
    register unsigned syscall_number asm("$2"){4001u};
    asm volatile("syscall" : "+r"(syscall_number) : "r"(status) : "memory");
    __builtin_unreachable();
}
