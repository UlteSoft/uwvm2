// Differential corpus for every SIMD unary/binary evaluator. Compile this
// separately for each ABI/optimization/product and compare complete stdout.
// Separate processes avoid ODR/COMDAT mixing between SSE and no-SSE helpers.
// This is a cross-configuration oracle, not independent spec certification;
// fixed-encoding regressions separately establish conversion/NaN boundaries.
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <cstdio>

namespace s = uwvm2::runtime::compiler::shared;
namespace v = s::wasm1p1_simd_details;
using code = s::simd_code;
using kind = s::wasm1p1_simd_instruction_kind;
using scalar = s::wasm1p1_simd_scalar_kind;

template <code Op> constexpr unsigned nan_width()
{
    switch (Op)
    {
#define FP(P, N) case code::P##_sqrt: case code::P##_ceil: case code::P##_floor: case code::P##_trunc: \
        case code::P##_nearest: case code::P##_add: case code::P##_sub: case code::P##_mul: \
        case code::P##_div: case code::P##_min: case code::P##_max: return N;
        FP(f32x4, 32)
        FP(f64x2, 64)
#undef FP
        case code::f32x4_demote_f64x2_zero: return 32;
        case code::f64x2_promote_low_f32x4: return 64;
        default: return 0;
    }
}

template <code Op, kind Kind>
[[gnu::noinline]] void evaluate(void* output, v::wasm_v128 a, v::wasm_v128 b)
{
    v::wasm_v128 result;
    if constexpr (Kind == kind::unary) { result = v::eval_full_unop<Op>(a); }
    else { result = v::eval_full_binop<Op>(a, b); }
    if constexpr (nan_width<Op>() != 0)
    {
        using UInt = std::conditional_t<nan_width<Op>() == 32, v::u32, v::u64>;
        constexpr UInt infinity = sizeof(UInt) == 4 ? UInt{0x7f800000u} : static_cast<UInt>(0x7ff0000000000000ull);
        constexpr UInt quiet = UInt{1} << (sizeof(UInt) == 4 ? 22 : 51);
        auto bits = v::load_uint_lanes<UInt, 16 / sizeof(UInt)>(result);
        for (auto& lane : bits.lane)
        {
            // Normalize allowed arithmetic qNaN payload/sign variation only.
            // sNaN results remain visible failures. Never normalize pmin/pmax,
            // abs/neg or integer instructions, whose payload bits are exact.
            if ((lane & (infinity | quiet)) == (infinity | quiet)) { lane = infinity | quiet; }
        }
        result = v::store_uint_lanes<UInt, 16 / sizeof(UInt)>(bits);
    }
    std::memcpy(output, &result, 16);
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if (!environment.ready()) { return 2; }
    constexpr v::u64 edges[]{
        0, 0x8000000000000000ull, 1, 0x000fffffffffffffull, 0x0010000000000000ull,
        0x3fefffffffffffffull, 0x3ff0000000000000ull, 0xbff0000000000000ull,
        0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff0000000000001ull, 0x7ff8000000000000ull,
        0xfff8123456789abcull, 0x7fefffffffffffffull, 0x3ca0000000000001ull,
        0x7f8000017fc00000ull, 0xff8000007f800000ull, 0x8000000000000000ull,
        0x0000000100800000ull, 0x4f8000004effffffull, 0xcf0000004f7fffffull,
        0x41efffffffffffffull, 0x41f0000000000000ull, 0xffffffffffffffffull,
    };
    unsigned opcodes{};
    for (unsigned op{}; op != 256; ++op)
    {
        bool covered = s::visit_wasm1p1_simd_instruction(static_cast<code>(op),
            [&]<code Op, kind Kind, scalar Scalar, std::size_t Lanes, unsigned Alignment>() {
                if constexpr (Kind == kind::unary || Kind == kind::binary)
                {
                    v::u64 seed = 0x0c71c6fe554a0312ull;
                    auto next = [&] { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; };
                    constexpr unsigned edge_count = sizeof(edges) / sizeof(edges[0]);
                    for (unsigned sample{}; sample != edge_count * edge_count + 256; ++sample)
                    {
                        v::lane_array<v::u64, 2> a{}, b{};
                        for (unsigned i{}; i != 2; ++i)
                        {
                            a.lane[i] = sample < edge_count * edge_count ? edges[(sample / edge_count + i) % edge_count] : next();
                            b.lane[i] = sample < edge_count * edge_count ? edges[(sample % edge_count + i) % edge_count] : next();
                        }
                        unsigned char out[16];
                        evaluate<Op, Kind>(out, v::store_uint_lanes<v::u64, 2>(a), v::store_uint_lanes<v::u64, 2>(b));
                        std::printf("%03u %03u ", op, sample);
                        for (auto byte : out) { std::printf("%02x", unsigned(byte)); }
                        std::putchar('\n');
                    }
                    return true;
                }
                else { return false; }
            });
        opcodes += covered;
    }
    // Protect against a accidentally empty/filtered visitor being called PASS.
    std::fprintf(stderr, "numeric opcode count=%u\n", opcodes);
    return opcodes == 170 ? 0 : 3;
}
