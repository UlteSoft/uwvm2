// Keep all test inputs/observations in integer bits: native FP return ABIs can
// quiet a signaling NaN before the interpreter is entered.
#include <uwvm2/runtime/compiler/uwvm_int/optable/numeric.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/constop.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/variable.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/stack.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/convert.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/conbine_heavy.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <cinttypes>
#include <cstdio>

namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace n = o::numeric_details;
using memory_type = uwvm2::object::memory::linear::native_memory_t;
using global_type = uwvm2::object::global::wasm_global_storage_t;

UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*, std::byte*, std::byte*) noexcept {}

template <class T>
void append(std::byte*& p, T const& v)
{
    std::memcpy(p, &v, sizeof(v));
    p += sizeof(v);
}

template <bool Tail, class Float, class UInt>
UWVM_NOINLINE UInt evaluate(unsigned op, UInt input, UInt sign, memory_type& memory)
{
    constexpr o::uwvm_interpreter_translate_option_t options{.is_tail_call = Tail};
    std::byte code[96]{}, values[64]{}, local[32]{};
    auto* immediate = code + sizeof(void*);
    std::memcpy(values, &input, sizeof(input));
    std::memcpy(values + sizeof(input), &sign, sizeof(sign));
    std::memcpy(local, &input, sizeof(input));
    std::byte const* ip = code;
    std::byte* sp = values + sizeof(input);
    std::byte* locals = local;
    global_type global{};
    if constexpr(sizeof(Float) == 4) { std::memcpy(&global.storage.f32, &input, sizeof(input)); }
    else
    {
        std::memcpy(&global.storage.f64, &input, sizeof(input));
    }
    if(op == 2) { sp += sizeof(input); }
    if(op == 3 || op == 6 || op == 10 || op == 11 || op == 12 || op >= 16) { sp = values; }
    if((op >= 3 && op <= 5) || op == 10 || op == 11) { append(immediate, std::size_t{}); }
    if(op == 6) { append(immediate, input); }
    if(op == 7 || op >= 16)
    {
        UInt other = input ^ UInt{0x13579};
        UInt a = sign ? input : other, b = sign ? other : input;
        n::wasm_i32 condition = sign != 0;
        if(op == 7)
        {
            std::memcpy(values, &a, sizeof(a));
            std::memcpy(values + sizeof(input), &b, sizeof(b));
            std::memcpy(values + 2 * sizeof(input), &condition, sizeof(condition));
            sp = values + 2 * sizeof(input) + sizeof(condition);
        }
        else
        {
            // Destination aliases a source; both select arms have distinct bits.
            std::memcpy(local, &a, sizeof(a));
            std::memcpy(local + 8, &b, sizeof(b));
            std::memcpy(local + 16, &condition, sizeof(condition));
            for(std::size_t off: {0uz, 8uz, 16uz, 0uz}) { append(immediate, off); }
        }
    }
    if(op == 12 || op == 13) { append(immediate, &global); }
    if(op == 14 || op == 15)
    {
        append(immediate, &memory);
        append(immediate, n::wasm_u32{});
        n::wasm_u32 address{};
        std::memcpy(values, &address, sizeof(address));
        if constexpr(sizeof(Float) == 4) { o::details::store_u32_le(memory.memory_begin, input); }
        else
        {
            o::details::store_u64_le(memory.memory_begin, input);
        }
        sp = values + sizeof(address);
        if(op == 15)
        {
            std::memcpy(sp, &input, sizeof(input));
            sp += sizeof(input);
            std::memset(memory.memory_begin, 0, sizeof(input));
        }
    }
    if constexpr(Tail)
    {
        auto end = &finish;
        append(immediate, end);
    }
#define INVOKE(NAME, ...)                                                                                                                                      \
    if constexpr(Tail)                                                                                                                                         \
        o::NAME<options __VA_OPT__(, ) __VA_ARGS__, 0uz>(ip, sp, locals);                                                                                      \
    else                                                                                                                                                       \
        o::NAME<options __VA_OPT__(, ) __VA_ARGS__>(ip, sp, locals)
#define FLOAT_CASES(W, I)                                                                                                                                      \
    case 0: INVOKE(uwvmint_##W##_unop, n::float_unop::abs); break;                                                                                             \
    case 1: INVOKE(uwvmint_##W##_unop, n::float_unop::neg); break;                                                                                             \
    case 2: INVOKE(uwvmint_##W##_binop, n::float_binop::copysign); break;                                                                                      \
    case 6: INVOKE(uwvmint_##W##_const); break;                                                                                                                \
    case 8: INVOKE(uwvmint_##I##_reinterpret_##W); break;                                                                                                      \
    case 9: INVOKE(uwvmint_##W##_reinterpret_##I); break;                                                                                                      \
    case 14: o::translate::get_uwvmint_##W##_load_fptr<options, std::byte const*, std::byte*, std::byte*>({})(ip, sp, locals); break;                          \
    case 15: o::translate::get_uwvmint_##W##_store_fptr<options, std::byte const*, std::byte*, std::byte*>({})(ip, sp, locals); break;
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
# define FUSED_CASES(W)                                                                                                                                        \
     case 10: INVOKE(uwvmint_##W##_unop_localget, n::float_unop::abs); break;                                                                                  \
     case 11: INVOKE(uwvmint_##W##_unop_localget, n::float_unop::neg); break;
#else
# define FUSED_CASES(W)
#endif
    switch(op)
    {
        case 3: INVOKE(uwvmint_local_get_typed, Float); break;
        case 4: INVOKE(uwvmint_local_set_typed, Float); break;
        case 5: INVOKE(uwvmint_local_tee_typed, Float); break;
        case 7: INVOKE(uwvmint_select_typed, Float); break;
        case 12: INVOKE(uwvmint_global_get_typed, Float); break;
        case 13: INVOKE(uwvmint_global_set_typed, Float); break;
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        case 16:
            if constexpr(sizeof(Float) == 4) { INVOKE(uwvmint_f32_select_local_settee, false); }
            break;
        case 17:
            if constexpr(sizeof(Float) == 4) { INVOKE(uwvmint_f32_select_local_settee, true); }
            break;
#endif
        default:
            if constexpr(sizeof(Float) == 4)
            {
                switch(op) { FLOAT_CASES(f32, i32) FUSED_CASES(f32) }
            }
            else
            {
                switch(op) { FLOAT_CASES(f64, i64) FUSED_CASES(f64) }
            }
    }
#undef FLOAT_CASES
#undef FUSED_CASES
#undef INVOKE
    UInt result;
    if(op == 13)
    {
        if constexpr(sizeof(Float) == 4) { std::memcpy(&result, &global.storage.f32, sizeof(result)); }
        else
        {
            std::memcpy(&result, &global.storage.f64, sizeof(result));
        }
    }
    else if(op == 15)
    {
        if constexpr(sizeof(Float) == 4) { result = std::bit_cast<UInt>(o::details::load_i32_le(memory.memory_begin)); }
        else
        {
            result = std::bit_cast<UInt>(o::details::load_i64_le(memory.memory_begin));
        }
    }
    else
    {
        std::memcpy(&result, op == 4 || op == 5 || op == 16 ? local : values, sizeof(result));
    }
    return result;
}

template <bool Tail, class Float, class UInt>
unsigned check(UInt value, memory_type& memory)
{
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    for(unsigned op = 0; op != 18; ++op)
    {
        for(UInt rhs: {UInt{}, sign})
        {
#if !defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            if(op == 10 || op == 11 || op >= 16) { continue; }
#endif
            if(sizeof(Float) != 4 && op >= 16) { continue; }
            UInt expected = op == 0 || op == 10 ? value & ~sign : op == 1 || op == 11 ? value ^ sign : op == 2 ? (value & ~sign) | rhs : value;
            UInt actual = evaluate<Tail, Float>(op, value, rhs, memory);
            if(actual != expected)
            {
                if(errors < 12)
                {
                    std::fprintf(stderr,
                                 "tail=%d f%zu op=%u input=%016" PRIx64 " expected=%016" PRIx64 " actual=%016" PRIx64 "\n",
                                 Tail,
                                 sizeof(UInt) * 8,
                                 op,
                                 std::uint64_t(value),
                                 std::uint64_t(expected),
                                 std::uint64_t(actual));
                }
                ++errors;
            }
        }
    }
    return errors;
}

int main()
{
    memory_type memory{};
    memory.init_by_page_count(1);
    unsigned errors{};
    for(std::uint32_t bits: {0u, 0x80000000u, 1u, 0x80000001u, 0x7f800000u, 0x7f800001u, 0xff800123u, 0x7fc00123u})
    {
        errors += check<false, n::wasm_f32>(bits, memory);
        errors += check<true, n::wasm_f32>(bits, memory);
    }
    for(std::uint64_t bits:
        {0ull, 0x8000000000000000ull, 1ull, 0x8000000000000001ull, 0x7ff0000000000000ull, 0x7ff0000000000001ull, 0xfff0000000000123ull, 0x7ff8000000000123ull})
    {
        errors += check<false, n::wasm_f64>(bits, memory);
        errors += check<true, n::wasm_f64>(bits, memory);
    }
    std::printf("FP bit transport: %u failures\n", errors);
    return errors != 0;
}
