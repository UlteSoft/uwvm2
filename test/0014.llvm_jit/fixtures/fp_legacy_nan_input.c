// Freestanding raw-byte ABI: no host FP parameter/result conversion can hide errors.
#define CALC(F, W, S)                                                                                                                                          \
    void calc##W(unsigned op, void const* a, void const* b, void* out)                                                                                         \
    {                                                                                                                                                          \
        F x, y, r;                                                                                                                                             \
        __builtin_memcpy(&x, a, sizeof x);                                                                                                                     \
        __builtin_memcpy(&y, b, sizeof y);                                                                                                                     \
        switch(op)                                                                                                                                             \
        {                                                                                                                                                      \
            case 0: r = x + y; break;                                                                                                                          \
            case 1: r = x - y; break;                                                                                                                          \
            case 2: r = x * y; break;                                                                                                                          \
            case 3: r = x / y; break;                                                                                                                          \
            case 4: r = __builtin_sqrt##S(x); break;                                                                                                           \
            case 5: r = __builtin_fabs##S(x); break;                                                                                                           \
            case 6: r = -x; break;                                                                                                                             \
            case 7: r = __builtin_copysign##S(x, y); break;                                                                                                    \
            default: r = x; break;                                                                                                                             \
        }                                                                                                                                                      \
        __builtin_memcpy(out, &r, sizeof r);                                                                                                                   \
    }
CALC(float, 32, f)
CALC(double, 64, )

void promote(void const* a, void* out)
{
    float x;
    __builtin_memcpy(&x, a, 4);
    double y = x;
    __builtin_memcpy(out, &y, 8);
}

void demote(void const* a, void* out)
{
    double x;
    __builtin_memcpy(&x, a, 8);
    float y = x;
    __builtin_memcpy(out, &y, 4);
}

typedef float vf __attribute__((vector_size(16)));

void vector_add(void const* a, void const* b, void* out)
{
    vf x, y;
    __builtin_memcpy(&x, a, 16);
    __builtin_memcpy(&y, b, 16);
    vf r = x + y;
    __builtin_memcpy(out, &r, 16);
}
