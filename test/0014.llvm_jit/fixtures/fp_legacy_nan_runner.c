typedef unsigned int u32;
typedef unsigned long long u64;
typedef __SIZE_TYPE__ usize;
#if !defined(__mips__)
extern int printf(char const*, ...);
# define CHECK_BAD(condition, label, input, output)                                                                                                            \
     do                                                                                                                                                        \
     {                                                                                                                                                         \
         if(condition)                                                                                                                                         \
         {                                                                                                                                                     \
             ++errors;                                                                                                                                         \
             printf("%s input=%llx output=%llx\n", label, (u64)(input), (u64)(output));                                                                        \
         }                                                                                                                                                     \
     }                                                                                                                                                         \
     while(0)
#else
# define CHECK_BAD(condition, label, input, output)                                                                                                            \
     do                                                                                                                                                        \
     {                                                                                                                                                         \
         if(condition) { ++errors; }                                                                                                                           \
     }                                                                                                                                                         \
     while(0)
#endif
void calc32(unsigned, void const*, void const*, void*);
void calc64(unsigned, void const*, void const*, void*);
void promote(void const*, void*);
void demote(void const*, void*);
void vector_add(void const*, void const*, void*);

#if defined(__mips__)
static void exit_process(unsigned value)
{
    register long number __asm__("$2") = sizeof(void*) == 8 ? 5058 : 4001;
    register long argument __asm__("$4") = value;
    __asm__ volatile("syscall" : "+r"(number) : "r"(argument) : "$7", "memory");
    __builtin_trap();
}
#endif
void* memcpy(void* out, void const* in, usize size)
{
    unsigned char* d = out;
    unsigned char const* s = in;
    for(usize i = 0; i < size; ++i) { d[i] = s[i]; }
    return out;
}

void* memset(void* out, int value, usize size)
{
    unsigned char* d = out;
    for(usize i = 0; i < size; ++i) { d[i] = (unsigned char)value; }
    return out;
}

#define SUITE(U, W, INF, QUIET, SIGN, ONE)                                                                                                                     \
    static unsigned suite##W(void)                                                                                                                             \
    {                                                                                                                                                          \
        U samples[] = {INF | 1, INF | QUIET, INF | QUIET | 123, SIGN | INF | 1, SIGN | INF | QUIET};                                                           \
        U one = ONE, out;                                                                                                                                      \
        unsigned errors = 0;                                                                                                                                   \
        for(unsigned i = 0; i < 5; ++i)                                                                                                                        \
            for(unsigned op = 0; op < 9; ++op)                                                                                                                 \
            {                                                                                                                                                  \
                U input = samples[i];                                                                                                                          \
                calc##W(op, &input, &one, &out);                                                                                                               \
                if(op < 5)                                                                                                                                     \
                {                                                                                                                                              \
                    int canonical = (input & ~SIGN) == (INF | QUIET);                                                                                          \
                    CHECK_BAD(canonical ? (out & ~SIGN) != (INF | QUIET) : (out & (INF | QUIET)) != (INF | QUIET), "arithmetic" #W, input, out);               \
                }                                                                                                                                              \
                else                                                                                                                                           \
                {                                                                                                                                              \
                    U expected = op == 5 ? input & ~SIGN : op == 6 ? input ^ SIGN : op == 7 ? input & ~SIGN : input;                                           \
                    CHECK_BAD(out != expected, "bits" #W, input, out);                                                                                         \
                }                                                                                                                                              \
            }                                                                                                                                                  \
        U infinity = INF, minus = SIGN | INF, zero = 0;                                                                                                        \
        calc##W(0, &infinity, &minus, &out);                                                                                                                   \
        CHECK_BAD((out & ~SIGN) != (INF | QUIET), "invalid add" #W, infinity, out);                                                                            \
        calc##W(3, &zero, &zero, &out);                                                                                                                        \
        CHECK_BAD((out & ~SIGN) != (INF | QUIET), "invalid div" #W, zero, out);                                                                                \
        calc##W(0, &one, &one, &out);                                                                                                                          \
        CHECK_BAD(out != (ONE + ((U)1 << (W == 32 ? 23 : 52))), "finite add" #W, one, out);                                                                    \
        return errors;                                                                                                                                         \
    }
SUITE(u32, 32, 0x7f800000u, 0x00400000u, 0x80000000u, 0x3f800000u)
SUITE(u64, 64, 0x7ff0000000000000ull, 0x0008000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull)

static int test_main(void)
{
    unsigned errors = suite32() + suite64();
    for(unsigned i = 0; i < 2; ++i)
    {
        u32 a = i ? 0x7fc00000u : 0x7f800001u, out32;
        u64 b = i ? 0x7ff8000000000000ull : 0x7ff0000000000001ull, out64;
        promote(&a, &out64);
        demote(&b, &out32);
        CHECK_BAD(out64 != 0x7ff8000000000000ull, "promote", a, out64);
        CHECK_BAD(out32 != 0x7fc00000u, "demote", b, out32);
    }
    u32 a[4] = {0x7f800001u, 0x7fc00000u, 0x3f800000u, 0xff800123u};
    u32 b[4] = {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u}, out[4];
    vector_add(a, b, out);
    for(unsigned i = 0; i < 4; ++i) { CHECK_BAD(out[i] != (i == 2 ? 0x40000000u : 0x7fc00000u), "vector add", a[i], out[i]); }
    return errors ? 1 : 0;
}
#if defined(__mips__)
__attribute__((noreturn)) void _start(void)
{
    exit_process(test_main());
    __builtin_unreachable();
}
# if __SIZEOF_POINTER__ == 8
__asm__(".text\n.globl bare_entry\n.option pic0\n.set noreorder\nbare_entry:\n dla $25, _start\n jr $25\n nop\n.set reorder\n.option pic2\n");
# else
__asm__(".text\n.globl bare_entry\n.option pic0\n.set noreorder\nbare_entry:\n la $25, _start\n jr $25\n nop\n.set reorder\n.option pic2\n");
# endif
#else
int main(void) { return test_main(); }
#endif
