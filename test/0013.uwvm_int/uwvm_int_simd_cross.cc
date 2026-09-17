// Target-side differential adapter for the actual SIMD evaluator used by uwvm-int.
// Link with a host-generated simd_direct_lowering C oracle, redirecting simd_OP_LANE wrappers
// to shared_simd(OP, LANE, out, in). Expected bytes must be generated on an independent host;
// this adapter intentionally does not normalize NaNs or compute its own expected result.
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#include <array>
#include <utility>
namespace s = uwvm2::runtime::compiler::shared;
namespace v = s::wasm1p1_simd_details;
using code = v::simd_code;
using kind = s::wasm1p1_simd_instruction_kind;
using scalar = s::wasm1p1_simd_scalar_kind;
using bytes = std::array<std::byte, 64>;
template <typename T> T read(std::byte const* p)
{
    std::array<std::byte, sizeof(T)> raw;
    std::memcpy(raw.data(), p, sizeof(T));
    if constexpr(std::is_arithmetic_v<T> && std::endian::native == std::endian::big)
    { for(std::size_t i{}; i < sizeof(T) / 2; ++i) { std::swap(raw[i], raw[sizeof(T) - i - 1]); } }
    return std::bit_cast<T>(raw);
}
template <typename T> void write(std::byte* p, T value)
{
    auto raw{std::bit_cast<std::array<std::byte, sizeof(T)>>(value)};
    if constexpr(std::is_arithmetic_v<T> && std::endian::native == std::endian::big)
    { for(std::size_t i{}; i < sizeof(T) / 2; ++i) { std::swap(raw[i], raw[sizeof(T) - i - 1]); } }
    std::memcpy(p, raw.data(), sizeof(T));
}

constexpr unsigned char shuffle_lanes[16]{31, 0, 17, 14, 29, 2, 19, 12, 27, 4, 21, 10, 25, 6, 23, 8};
template <code Op, kind Kind, scalar Scalar>
__attribute__((noinline)) void reference(bytes& out, bytes const& in, unsigned lane)
{
    auto a{read<v::wasm_v128>(in.data())};
    auto b{read<v::wasm_v128>(in.data() + 16)};
    auto c{read<v::wasm_v128>(in.data() + 32)};
    if constexpr(Kind == kind::constant) { write(out.data(), read<v::wasm_v128>(reinterpret_cast<std::byte const*>(shuffle_lanes))); }
    else if constexpr(Kind == kind::shuffle) { write(out.data(), v::eval_shuffle(a, b, v::make_shuffle_controls(shuffle_lanes))); }
    else if constexpr(Kind == kind::unary) { write(out.data(), v::eval_full_unop<Op>(a)); }
    else if constexpr(Kind == kind::binary) { write(out.data(), v::eval_full_binop<Op>(a, b)); }
    else if constexpr(Kind == kind::ternary) { write(out.data(), v::eval_bitselect(a, b, c)); }
    else if constexpr(Kind == kind::shift) { write(out.data(), v::eval_full_shift<Op>(a, read<v::wasm_i32>(in.data() + 48))); }
    else if constexpr(Kind == kind::test) { write(out.data(), v::eval_full_test<Op>(a)); }
    else if constexpr(Kind == kind::memory_load) { write(out.data(), v::eval_memory_load<Op>(in.data() + 1, a, lane)); }
    else if constexpr(Kind == kind::memory_store) { v::eval_memory_store<Op>(out.data() + 3, a, lane); }
#define SCALAR_REFERENCE(K, T) \
    else if constexpr(Scalar == scalar::K) \
    { \
        auto x{read<v::wasm_##T>(in.data() + 48)}; \
        if constexpr(Kind == kind::splat) { write(out.data(), v::eval_full_splat_##T<Op>(x)); } \
        else if constexpr(Kind == kind::extract_lane) { write(out.data(), v::eval_extract_lane_##T<Op>(a, lane)); } \
        else if constexpr(Kind == kind::replace_lane) { write(out.data(), v::eval_replace_lane_##T<Op>(a, x, lane)); } \
    }
    SCALAR_REFERENCE(i32, i32)
    SCALAR_REFERENCE(i64, i64)
    SCALAR_REFERENCE(f32, f32)
    SCALAR_REFERENCE(f64, f64)
#undef SCALAR_REFERENCE
}


extern "C" void shared_simd(unsigned opcode, unsigned lane, unsigned char* output, unsigned char const* input)
{
    bytes in, out;
    std::memcpy(in.data(), input, in.size());
    std::memcpy(out.data(), output, out.size());
    bool const handled{s::visit_wasm1p1_simd_instruction(static_cast<code>(opcode),
        [&]<code Op, kind Kind, scalar Scalar, std::size_t Lanes, unsigned Align>() {
            reference<Op, Kind, Scalar>(out, in, lane);
            return true;
        })};
    if(!handled) { __builtin_trap(); }
    std::memcpy(output, out.data(), out.size());
}

#ifndef UWVM2TEST_SIMD_CROSS_ADAPTER
// A small independent oracle also makes this file a normal standalone regression target.
// Include subnormal arithmetic and comparisons: ARM32 NEON ignores FPSCR.FZ for these.
int main()
{
    bytes a{}, b{}, out{};
    auto check{[&](code op, std::uint32_t lhs, std::uint32_t rhs, std::uint32_t expected) {
        for(unsigned i{}; i != 4; ++i) { write(a.data() + i * 4, lhs); write(b.data() + i * 4, rhs); }
        std::memcpy(a.data() + 16, b.data(), 16);
        shared_simd(unsigned(op), 0, reinterpret_cast<unsigned char*>(out.data()), reinterpret_cast<unsigned char const*>(a.data()));
        for(unsigned i{}; i != 4; ++i) { if(read<std::uint32_t>(out.data() + i * 4) != expected) { return false; } }
        return true;
    }};
    if(!check(code::f32x4_eq, 1u, 2u, 0u) ||
       !check(code::f32x4_ne, 1u, 2u, 0xffffffffu) ||
       !check(code::f32x4_lt, 1u, 2u, 0xffffffffu) ||
       !check(code::f32x4_add, 1u, 1u, 2u) ||
       !check(code::f32x4_sub, 2u, 1u, 1u) ||
       !check(code::f32x4_mul, 1u, 0x40000000u, 2u) ||
       !check(code::f32x4_pmin, 2u, 1u, 1u) ||
       !check(code::f32x4_pmax, 1u, 2u, 2u)) { return 1; }
    for(unsigned i{}; i != 16; ++i) { a[i] = std::byte(i + 1); }
    for(unsigned base{}; base != 256; base += 16)
    {
        for(unsigned i{}; i != 16; ++i) { a[16 + i] = std::byte(base + i); }
        shared_simd(unsigned(code::i8x16_swizzle), 0, reinterpret_cast<unsigned char*>(out.data()), reinterpret_cast<unsigned char const*>(a.data()));
        for(unsigned i{}; i != 16; ++i) { if(out[i] != (base ? std::byte{} : std::byte(i + 1))) { return 2; } }
    }
    return 0;
}
#endif
