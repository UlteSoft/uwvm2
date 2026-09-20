// Fusion can bypass the fixed ordinary opfuncs. Exercise immediate/local
// copysign variants with integer observations, including aliasing and both
// dispatch forms; passing the scalar test alone does not validate these paths.
// All copysign fusion families must retain the magnitude operand's NaN payload.
#define UWVM_ENABLE_UWVM_INT_COMBINE_OPS 1
#define UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS 1
#define UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT 1
#define UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY 1
#define UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS 1
#include <uwvm2/runtime/compiler/uwvm_int/optable/conbine_heavy.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/delay_local.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <cstdio>
namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace n = o::numeric_details;

UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*, std::byte*, std::byte*) noexcept {}

template <class T>
void append(std::byte*& p, T const& v)
{
    std::memcpy(p, &v, sizeof(v));
    p += sizeof(v);
}

template <bool Tail, class Float, class UInt>
UWVM_NOINLINE UInt evaluate(unsigned op, UInt input, UInt sign)
{
    constexpr o::uwvm_interpreter_translate_option_t option{.is_tail_call = Tail};
    std::byte code[96]{}, values[32]{}, locals[32]{};
    std::memcpy(values, &input, sizeof(input));
    std::memcpy(locals, &input, sizeof(input));
    std::memcpy(locals + 8, &sign, sizeof(sign));
    auto immediate = code + sizeof(void*);
    auto fp = &finish;
    if(op < 3)
    {
        append(immediate, std::size_t{8});
        if(op != 0)
        {
            append(immediate, fp);
            append(immediate, std::size_t{});
        }
    }
    if(op == 4 || op == 6 || op == 7) { append(immediate, std::size_t{}); }
    if(op >= 3 && op != 5) { append(immediate, sign); }
    if(op == 5)
    {
        append(immediate, std::size_t{});
        append(immediate, std::size_t{8});
    }
    append(immediate, fp);
    std::byte const* ip = code;
    std::byte* sp = values + (op < 4 ? sizeof(Float) : 0);
    std::byte* lp = locals;
#define CALL(NS, NAME)                                                                                                                                         \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        if constexpr(Tail)                                                                                                                                     \
            o::NS NAME<option, n::float_binop::copysign, SIZE_MAX, std::byte const*, std::byte*, std::byte*>(ip, sp, lp);                                      \
        else                                                                                                                                                   \
            o::NS NAME<option, n::float_binop::copysign, std::byte const*, std::byte*, std::byte*>(ip, sp, lp);                                                \
    }                                                                                                                                                          \
    while(false)
#define CASES(W)                                                                                                                                               \
    case 0: CALL(delay_local_details::, uwvmint_##W##_binop_localget_rhs); break;                                                                              \
    case 1: CALL(delay_local_details::, uwvmint_##W##_binop_localget_rhs_local_set); break;                                                                    \
    case 2: CALL(delay_local_details::, uwvmint_##W##_binop_localget_rhs_local_tee); break;                                                                    \
    case 3: CALL(, uwvmint_##W##_binop_imm_stack); break;                                                                                                      \
    case 4: CALL(, uwvmint_##W##_binop_imm_localget); break;                                                                                                   \
    case 5: CALL(, uwvmint_##W##_binop_2localget); break;                                                                                                      \
    case 6: CALL(, uwvmint_##W##_binop_imm_local_set_same); break;                                                                                             \
    case 7: CALL(, uwvmint_##W##_binop_imm_local_tee_same); break;
    if constexpr(sizeof(Float) == 4)
    {
        switch(op) { CASES(f32) }
    }
    else
    {
        switch(op) { CASES(f64) }
    }
#undef CASES
#undef CALL
    UInt result{};
    std::memcpy(&result, op == 1 || op == 6 ? locals : values, sizeof(result));
    return result;
}

template <bool Tail, class Float, class UInt>
unsigned check(UInt input)
{
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    for(unsigned op = 0; op != 8; ++op)
    {
        for(UInt rhs: {UInt{}, sign, static_cast<UInt>(input | sign)})
        {
            auto actual = evaluate<Tail, Float>(op, input, rhs);
            auto expected = (input & ~sign) | (rhs & sign);
            if(actual != expected)
            {
                ++errors;
                std::fprintf(stderr,
                             "fusion tail=%d f%zu op=%u expected=%llx actual=%llx\n",
                             Tail,
                             sizeof(UInt) * 8,
                             op,
                             static_cast<unsigned long long>(expected),
                             static_cast<unsigned long long>(actual));
            }
        }
    }
    return errors;
}

int main()
{
    unsigned errors{};
    for(std::uint32_t bits: {0u, 1u, 0x80000000u, 0x7f800001u, 0xff800123u, 0x7fc00123u})
    {
        errors += check<true, n::wasm_f32>(bits);
        errors += check<false, n::wasm_f32>(bits);
    }
    for(std::uint64_t bits: {0ull, 1ull, 0x8000000000000000ull, 0x7ff0000000000001ull, 0xfff0000000000123ull})
    {
        errors += check<true, n::wasm_f64>(bits);
        errors += check<false, n::wasm_f64>(bits);
    }
    std::printf("FP copysign fusions: %u failures\n", errors);
    return errors != 0;
}
