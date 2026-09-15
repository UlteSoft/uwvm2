// Produce native rounding IR with a raw-storage boundary. RISC-V's fallback
// may leave sNaNs untouched; this input must reach the shared LLVM repair pass,
// not just the C++ interpreter's already-correct rounding helper.
typedef unsigned int u32;
typedef unsigned long long u64;
#define ROUND(U, F, W, S)                                                                                                                                      \
    U rounding##W(U raw, unsigned op)                                                                                                                          \
    {                                                                                                                                                          \
        F v, r;                                                                                                                                                \
        __builtin_memcpy(&v, &raw, sizeof v);                                                                                                                  \
        switch(op)                                                                                                                                             \
        {                                                                                                                                                      \
            case 0: r = __builtin_ceil##S(v); break;                                                                                                           \
            case 1: r = __builtin_floor##S(v); break;                                                                                                          \
            case 2: r = __builtin_trunc##S(v); break;                                                                                                          \
            case 3: r = __builtin_nearbyint##S(v); break;                                                                                                      \
            default: r = __builtin_roundeven##S(v); break;                                                                                                     \
        }                                                                                                                                                      \
        __builtin_memcpy(&raw, &r, sizeof r);                                                                                                                  \
        return raw;                                                                                                                                            \
    }
ROUND(u32, float, 32, f)
ROUND(u64, double, 64, )
