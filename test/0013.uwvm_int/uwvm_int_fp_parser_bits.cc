// Global initializers are bit patterns, not native floating-point expressions.
#include <uwvm2/parser/wasm/standard/wasm1/impl.h>
#include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
#include <cstdio>
namespace wasm = uwvm2::parser::wasm;

template <class UInt, wasm::concepts::wasm_feature... Features>
unsigned check(UInt bits)
{
    wasm::concepts::feature_parameter_t<Features...> parameters{};
    wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Features...> storage{};
    wasm::standard::wasm1::features::final_local_global_type<Features...> global{};
    global.global.type = static_cast<decltype(global.global.type)>(sizeof(UInt) == 4 ? 0x7d : 0x7c);
    std::byte expr[sizeof(UInt) + 2]{};
    expr[0] = static_cast<std::byte>(sizeof(UInt) == 4 ? 0x43 : 0x44);
    for(unsigned i = 0; i != sizeof(UInt); ++i) { expr[1 + i] = static_cast<std::byte>((bits >> (8 * i)) & 255u); }
    expr[sizeof(UInt) + 1] = std::byte{0x0b};
    wasm::base::error_impl error{};
    using reserve = wasm::concepts::feature_reserve_type_t<wasm::standard::wasm1::features::global_section_storage_t<Features...>>;
    wasm::standard::wasm1::features::parse_and_check_global_expr_valid<Features...>(reserve{},
                                                                                    global.global,
                                                                                    global.expr,
                                                                                    storage,
                                                                                    expr,
                                                                                    expr + sizeof(expr),
                                                                                    error,
                                                                                    parameters);
    UInt actual{};
    if constexpr(sizeof(UInt) == 4) { std::memcpy(&actual, &global.expr.opcodes.front_unchecked().storage.f32, sizeof(actual)); }
    else
    {
        std::memcpy(&actual, &global.expr.opcodes.front_unchecked().storage.f64, sizeof(actual));
    }
    if(actual == bits) { return 0; }
    std::fprintf(stderr,
                 "global parser features=%zu f%zu expected=%llx actual=%llx\n",
                 sizeof...(Features),
                 sizeof(UInt) * 8,
                 static_cast<unsigned long long>(bits),
                 static_cast<unsigned long long>(actual));
    return 1;
}

int main()
{
    using v1 = wasm::standard::wasm1::features::wasm1;
    using v11 = wasm::standard::wasm1p1::features::wasm1p1;
    unsigned errors{};
    for(std::uint32_t bits: {0u, 1u, 0x80000000u, 0x7f800001u, 0xff800123u, 0x7fc00123u})
    {
        errors += check<std::uint32_t, v1>(bits);
        errors += check<std::uint32_t, v1, v11>(bits);
    }
    for(std::uint64_t bits: {0ull, 1ull, 0x8000000000000000ull, 0x7ff0000000000001ull, 0xfff0000000000123ull})
    {
        errors += check<std::uint64_t, v1>(bits);
        errors += check<std::uint64_t, v1, v11>(bits);
    }
    std::printf("FP global parsing: %u failures\n", errors);
    return errors != 0;
}
