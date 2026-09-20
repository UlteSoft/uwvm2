// Legacy specialized lane moves must not quiet signaling NaNs through an x87 return ABI.
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/wasm1p1.h>
#include <cstdint>
#include <cstdio>
namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace s = o::wasm1p1_simd_details;
[[gnu::noinline]] std::uint32_t move_bits(std::uint32_t input, bool extract)
{
    constexpr o::uwvm_interpreter_translate_option_t options{.is_tail_call = false};
    std::byte code[32]{}, values[32]{}, local[8]{};
    for(unsigned i{}; i != 4; ++i) { std::memcpy(values + 4*i, &input, 4); }
    std::byte const* ip{code};
    std::byte* sp{values + (extract ? 16 : 4)};
    std::byte* locals{local};
    if(extract) { o::uwvmint_simd_f32x4_extract_lane<options, 3uz>(ip, sp, locals); }
    else { o::uwvmint_simd_f32x4_splat<options, s::v128_splatop::f32x4>(ip, sp, locals); }
    std::uint32_t output{};
    std::memcpy(&output, values, 4);
    return output;
}
int main()
{
    unsigned errors{};
    for(std::uint32_t value : {0u, 0x80000000u, 1u, 0x80000001u, 0x7f800001u, 0xff800123u, 0x7fc00123u})
    {
        volatile std::uint32_t input{value};
        for(bool extract : {false, true})
        {
            auto actual{move_bits(input, extract)};
            if(actual != value) { std::printf("extract=%d input=%08x actual=%08x\n", extract, value, actual); ++errors; }
        }
    }
    std::printf("legacy SIMD lane bits: %u failures\n", errors);
    return errors != 0;
}
