// SIMD float lanes are bit patterns for moves, sign operations and pseudo-min/max.
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/wasm1p1.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <cstdio>
#include <cinttypes>
namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace s = o::wasm1p1_simd_details;
using code = s::simd_code;

UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish_simd(std::byte const*, std::byte*, std::byte*) noexcept {}

template <bool Tail, class Float, class UInt>
UWVM_NOINLINE unsigned check_simd(UInt input)
{
    constexpr o::uwvm_interpreter_translate_option_t options{.is_tail_call = Tail};
    constexpr std::size_t count = 16 / sizeof(UInt);
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    for(unsigned op = 0; op != 9; ++op)
    {
        for(std::size_t lane = 0; lane != count; ++lane)
        {
            if(sizeof(Float) != 4 && op >= 7) { continue; }
            s::lane_array<UInt, count> left{}, right{};
            for(std::size_t i = 0; i != count; ++i)
            {
                left.lane[i] = input;
                right.lane[i] = UInt{};
            }
            auto v = s::store_uint_lanes<UInt, count>(left);
            auto r = s::store_uint_lanes<UInt, count>(right);
            std::byte bytecode[32]{}, values[64]{}, local[8]{};
            std::byte const* ip = bytecode;
            std::byte* sp = values + sizeof(v);
            std::byte* locals = local;
            std::memcpy(values, &v, sizeof(v));
            auto* immediate = bytecode + sizeof(void*);
            if(op == 2 || op == 3)
            {
                std::memcpy(sp, &r, sizeof(r));
                sp += sizeof(r);
            }
            if(op == 4 || op == 7)
            {
                std::memcpy(values, &input, sizeof(input));
                sp = values + sizeof(input);
            }
            if(op == 5 || op == 6) { *immediate++ = std::byte(lane); }
            if(op == 6)
            {
                std::memcpy(sp, &input, sizeof(input));
                sp += sizeof(input);
            }
            if constexpr(Tail)
            {
                auto end = &finish_simd;
                std::memcpy(immediate, &end, sizeof(end));
            }
#define CASES(SHAPE)                                                                                                                                           \
    case 0: o::uwvmint_simd_full_unop<options, code::SHAPE##_abs>(ip, sp, locals); break;                                                                      \
    case 1: o::uwvmint_simd_full_unop<options, code::SHAPE##_neg>(ip, sp, locals); break;                                                                      \
    case 2: o::uwvmint_simd_full_binop<options, code::SHAPE##_pmin>(ip, sp, locals); break;                                                                    \
    case 3: o::uwvmint_simd_full_binop<options, code::SHAPE##_pmax>(ip, sp, locals); break;                                                                    \
    case 4: o::uwvmint_simd_full_splat<options, code::SHAPE##_splat, Float>(ip, sp, locals); break;                                                            \
    case 5: o::uwvmint_simd_full_extract_lane<options, code::SHAPE##_extract_lane, Float>(ip, sp, locals); break;                                              \
    case 6: o::uwvmint_simd_full_replace_lane<options, code::SHAPE##_replace_lane, Float>(ip, sp, locals); break;
            if constexpr(sizeof(Float) == 4)
            {
                switch(op)
                {
                    CASES(f32x4)
                    case 7: o::uwvmint_simd_f32x4_splat<options, s::v128_splatop::f32x4>(ip, sp, locals); break;
                    case 8: o::uwvmint_simd_f32x4_extract_lane<options, 3uz>(ip, sp, locals); break;
                }
            }
            else
            {
                switch(op) { CASES(f64x2) }
            }
#undef CASES
            UInt expected = op == 0 ? input & ~sign : op == 1 ? input ^ sign : input;
            // Against +0: NaNs and either zero select the left operand unchanged.
            UInt magnitude = input & ~sign;
            UInt infinity;
            if constexpr(sizeof(UInt) == 4) { infinity = UInt{0x7f800000u}; }
            else
            {
                infinity = UInt{0x7ff0000000000000ull};
            }
            if((op == 2 || op == 3) && magnitude != 0 && magnitude <= infinity && (op == 2 ? (input & sign) == 0 : (input & sign) != 0)) { expected = 0; }
            if(op == 5 || op == 8)
            {
                UInt actual;
                std::memcpy(&actual, values, sizeof(actual));
                if(actual != expected) { ++errors; }
            }
            else
            {
                std::memcpy(&v, values, sizeof(v));
                auto actual = s::load_uint_lanes<UInt, count>(v);
                for(auto value: actual.lane)
                {
                    if(value != expected) { ++errors; }
                }
            }
            if(errors)
            {
                std::fprintf(stderr, "SIMD tail=%d f%zu op=%u lane=%zu input=%016" PRIx64 "\n", Tail, sizeof(UInt) * 8, op, lane, std::uint64_t(input));
                return errors;
            }
        }
    }
    return errors;
}

int main()
{
    unsigned errors{};
    for(std::uint32_t value: {0u, 0x80000000u, 1u, 0x80000001u, 0x7f800001u, 0xff800123u, 0x7fc00123u})
    {
        errors += check_simd<false, s::wasm_f32>(value);
        errors += check_simd<true, s::wasm_f32>(value);
    }
    for(std::uint64_t value: {0ull, 0x8000000000000000ull, 1ull, 0x8000000000000001ull, 0x7ff0000000000001ull, 0xfff0000000000123ull})
    {
        errors += check_simd<false, s::wasm_f64>(value);
        errors += check_simd<true, s::wasm_f64>(value);
    }
    std::printf("SIMD FP bit transport: %u failures\n", errors);
    return errors != 0;
}
