// Exercise the production imported-global opcode and reference-provider ABI.
#include <uwvm2/runtime/compiler/uwvm_int/optable/variable.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <cstdio>
namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace value = uwvm2::parser::wasm::standard::wasm1::type;
namespace type = uwvm2::uwvm::wasm::type;

template <class Float>
struct reference_global
{
    inline static constexpr uwvm2::utils::container::u8string_view global_name{sizeof(Float) == 4 ? u8"f32" : u8"f64"};
    inline static constexpr bool is_mutable{true};
    using value_type = Float;
    Float value{};

    friend Float const& global_get(reference_global& g) noexcept { return g.value; }

    friend void global_set(reference_global& g, Float const& v) noexcept { std::memcpy(&g.value, &v, sizeof(v)); }
};

struct provider
{
    uwvm2::utils::container::u8string_view module_name{u8"bits"};
    using local_global_tuple = uwvm2::utils::container::tuple<reference_global<value::wasm_f32>, reference_global<value::wasm_f64>>;
    local_global_tuple local_global{};
};

UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*, std::byte*, std::byte*) noexcept {}

template <class T>
void append(std::byte*& p, T const& v)
{
    std::memcpy(p, &v, sizeof(v));
    p += sizeof(v);
}

template <bool Tail, class Float, class UInt>
unsigned check(UInt bits, type::local_imported_t& host)
{
    constexpr o::uwvm_interpreter_translate_option_t options{.is_tail_call = Tail};
    std::size_t index = sizeof(Float) == 8;
    unsigned errors{};
    for(bool setter: {false, true})
    {
        std::byte code[64]{}, values[16]{}, locals[16]{};
        auto p = code + sizeof(void*);
        auto module = &host;
        auto next = &finish;
        append(p, module);
        append(p, index);
        append(p, next);
        std::memcpy(values, &bits, sizeof(bits));
        if(!host.global_set_from_index(index, reinterpret_cast<std::byte const*>(&bits))) { return 1; }
        std::byte const* ip = code;
        std::byte* sp = values + (setter ? sizeof(Float) : 0);
        std::byte* lp = locals;
#define CALL(NAME)                                                                                                                                             \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        if constexpr(Tail)                                                                                                                                     \
            o::NAME<options, Float, SIZE_MAX, std::byte const*, std::byte*, std::byte*>(ip, sp, lp);                                                           \
        else                                                                                                                                                   \
            o::NAME<options, Float, std::byte const*, std::byte*, std::byte*>(ip, sp, lp);                                                                     \
    }                                                                                                                                                          \
    while(false)
        if(setter) { CALL(uwvmint_local_imported_global_set_typed); }
        else
        {
            CALL(uwvmint_local_imported_global_get_typed);
        }
#undef CALL
        UInt actual{};
        if(setter) { host.global_get_from_index(index, reinterpret_cast<std::byte*>(&actual)); }
        else
        {
            std::memcpy(&actual, values, sizeof(actual));
        }
        if(actual != bits)
        {
            ++errors;
            std::fprintf(stderr,
                         "provider tail=%d set=%d f%zu expected=%llx actual=%llx\n",
                         Tail,
                         setter,
                         sizeof(UInt) * 8,
                         static_cast<unsigned long long>(bits),
                         static_cast<unsigned long long>(actual));
        }
    }
    return errors;
}

int main()
{
    type::local_imported_t host{provider{}};
    unsigned errors{};
    for(std::uint32_t bits: {0u, 1u, 0x80000000u, 0x7f800001u, 0xff800123u, 0x7fc00123u})
    {
        errors += check<true, value::wasm_f32>(bits, host);
        errors += check<false, value::wasm_f32>(bits, host);
    }
    for(std::uint64_t bits: {0ull, 1ull, 0x8000000000000000ull, 0x7ff0000000000001ull, 0xfff0000000000123ull})
    {
        errors += check<true, value::wasm_f64>(bits, host);
        errors += check<false, value::wasm_f64>(bits, host);
    }
    std::printf("FP imported globals: %u failures\n", errors);
    return errors != 0;
}
