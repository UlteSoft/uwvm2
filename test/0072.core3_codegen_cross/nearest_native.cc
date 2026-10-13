#include <fast_io.h>
#include <bit>
#include <cstdint>
#include <limits>
#include <uwvm2/runtime/compiler/uwvm_int/optable/numeric.h>

namespace numeric = uwvm2::runtime::compiler::uwvm_int::optable::numeric_details;
using f32 = uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
using f64 = uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

extern "C" [[gnu::noinline]] f32 uwvm2_test_nearest_f32(f32 value)
{
    return numeric::eval_float_unop<numeric::float_unop::nearest>(value);
}

extern "C" [[gnu::noinline]] f64 uwvm2_test_nearest_f64(f64 value)
{
    return numeric::eval_float_unop<numeric::float_unop::nearest>(value);
}

template<typename UInt>
struct nearest_case
{
    UInt input;
    UInt expected;
    UInt mask{::std::numeric_limits<UInt>::max()};
};

template<typename Float, typename UInt, ::std::size_t Count, typename Evaluator>
bool check(nearest_case<UInt> const (&cases)[Count], Evaluator evaluator)
{
    for(auto const entry: cases)
    {
        // Force runtime evaluation, including when the caller is optimized.
        volatile UInt source{entry.input};
        auto const result{::std::bit_cast<UInt>(evaluator(::std::bit_cast<Float>(static_cast<UInt>(source))))};
        if((result & entry.mask) != entry.expected)
        {
            ::fast_io::println("nearest bits mismatch: input=", entry.input, " expected=", entry.expected, " result=", result);
            return false;
        }
    }
    return true;
}

int main()
{
    // Exact Wasm nearest results: signed zero, subnormals, ties-to-even,
    // integral extrema and infinities. NaNs must remain quiet arithmetic NaNs.
    constexpr nearest_case<::std::uint32_t> cases32[]{
        {0u, 0u}, {0x80000000u, 0x80000000u}, {1u, 0u}, {0x80000001u, 0x80000000u},
        {0x3f000000u, 0u}, {0xbf000000u, 0x80000000u},
        {0x3f800000u, 0x3f800000u}, {0xbf800000u, 0xbf800000u},
        {0x3f7fffffu, 0x3f800000u}, {0x3f800001u, 0x3f800000u},
        {0x3fc00000u, 0x40000000u}, {0xbfc00000u, 0xc0000000u},
        {0x40200000u, 0x40000000u}, {0xc0200000u, 0xc0000000u},
        {0x7f7fffffu, 0x7f7fffffu}, {0xff7fffffu, 0xff7fffffu},
        {0x7f800000u, 0x7f800000u}, {0xff800000u, 0xff800000u},
        {0x7fc00000u, 0x7fc00000u, 0x7fffffffu},
        {0x7f800001u, 0x7fc00000u, 0x7fc00000u}, {0xffc00011u, 0x7fc00000u, 0x7fc00000u}};
    constexpr nearest_case<::std::uint64_t> cases64[]{
        {0ull, 0ull}, {0x8000000000000000ull, 0x8000000000000000ull},
        {1ull, 0ull}, {0x8000000000000001ull, 0x8000000000000000ull},
        {0x3fe0000000000000ull, 0ull}, {0xbfe0000000000000ull, 0x8000000000000000ull},
        {0x3ff0000000000000ull, 0x3ff0000000000000ull}, {0xbff0000000000000ull, 0xbff0000000000000ull},
        {0x3fefffffffffffffull, 0x3ff0000000000000ull}, {0x3ff0000000000001ull, 0x3ff0000000000000ull},
        {0x3ff8000000000000ull, 0x4000000000000000ull}, {0xbff8000000000000ull, 0xc000000000000000ull},
        {0x4004000000000000ull, 0x4000000000000000ull}, {0xc004000000000000ull, 0xc000000000000000ull},
        {0x7fefffffffffffffull, 0x7fefffffffffffffull}, {0xffefffffffffffffull, 0xffefffffffffffffull},
        {0x7ff0000000000000ull, 0x7ff0000000000000ull}, {0xfff0000000000000ull, 0xfff0000000000000ull},
        {0x7ff8000000000000ull, 0x7ff8000000000000ull, 0x7fffffffffffffffull},
        {0x7ff0000000000001ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull},
        {0xfff8000000000011ull, 0x7ff8000000000000ull, 0x7ff8000000000000ull}};
    if(!check<f32>(cases32, uwvm2_test_nearest_f32) || !check<f64>(cases64, uwvm2_test_nearest_f64)) { return 1; }
    ::fast_io::println("nearest: 42 exact/allowed-NaN results passed");
}
