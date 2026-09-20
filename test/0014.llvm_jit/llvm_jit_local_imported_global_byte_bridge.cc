#ifndef UWVM2TEST_RUNNER_USE_LLVM_JIT
# define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"

#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace llvm_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    namespace wasm_type = ::uwvm2::uwvm::wasm::type;

    using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
    using wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;
    using wasm_externref = ::uwvm2::object::global::wasm_externref_t;
    using value_type = llvm_details::runtime_operand_stack_value_type;
    using bridge_abi = llvm_details::llvm_jit_local_imported_global_bridge_abi;

    inline constexpr ::std::uint8_t k_val_v128{0x7bu};
    inline constexpr ::std::uint8_t k_val_externref{0x6fu};

    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::i32) == bridge_abi::scalar_value);
    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::i64) == bridge_abi::scalar_value);
    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::f32) == bridge_abi::scalar_value);
    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::f64) == bridge_abi::scalar_value);
    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::v128) == bridge_abi::byte_buffer);
    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::funcref) == bridge_abi::byte_buffer);
    static_assert(llvm_details::get_llvm_jit_local_imported_global_bridge_abi(value_type::externref) == bridge_abi::byte_buffer);
    static_assert(llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<llvm_details::runtime_wasm_i32>);
    static_assert(llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<llvm_details::runtime_wasm_i64>);
    static_assert(llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<llvm_details::runtime_wasm_f32>);
    static_assert(llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<llvm_details::runtime_wasm_f64>);
    static_assert(!llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<wasm_v128>);
    static_assert(!llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<wasm_funcref>);
    static_assert(!llvm_details::llvm_jit_local_imported_global_scalar_bridge_type<wasm_externref>);
    static_assert(llvm_details::get_runtime_wasm_value_type_abi_size(value_type::v128) == sizeof(wasm_v128));
    static_assert(llvm_details::get_runtime_wasm_value_type_abi_size(value_type::funcref) == sizeof(wasm_funcref));
    static_assert(llvm_details::get_runtime_wasm_value_type_abi_size(value_type::externref) == sizeof(wasm_externref));

    template <typename ValueType, ::std::size_t Index>
    struct mutable_host_global
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{
            []() constexpr noexcept -> ::uwvm2::utils::container::u8string_view
            {
                if constexpr(Index == 0uz) { return u8"g_v128"; }
                else if constexpr(Index == 1uz) { return u8"g_funcref"; }
                else { return u8"g_externref"; }
            }()};
        inline static constexpr bool is_mutable{true};
        using value_type = ValueType;

        inline static ::std::size_t get_count{};
        inline static ::std::size_t set_count{};
        value_type value{};

        friend value_type global_get(mutable_host_global& global) noexcept
        {
            ++get_count;
            return global.value;
        }

        friend void global_set(mutable_host_global& global, value_type value) noexcept
        {
            ++set_count;
            global.value = value;
        }
    };

    using host_v128_global = mutable_host_global<wasm_v128, 0uz>;
    using host_funcref_global = mutable_host_global<wasm_funcref, 1uz>;
    using host_externref_global = mutable_host_global<wasm_externref, 2uz>;

    struct host_globals_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"host_globals"};
        using local_global_tuple = ::uwvm2::utils::container::tuple<host_v128_global, host_funcref_global, host_externref_global>;
        local_global_tuple local_global{};
    };

    static_assert(wasm_type::is_local_imported_module<host_globals_module>);

    [[nodiscard]] strict::byte_vec build_roundtrip_module()
    {
        strict::module_builder module{};
        module.add_import_global("host_globals", "g_v128", k_val_v128, true);
        module.add_import_global("host_globals", "g_funcref", strict::k_ref_funcref, true);
        module.add_import_global("host_globals", "g_externref", k_val_externref, true);

        auto const add_roundtrip{[&](::std::uint8_t type, ::std::uint32_t global_index)
                                 {
                                     strict::func_type function_type{{type}, {type}};
                                     strict::func_body function_body{};
                                     auto& code{function_body.code};
                                     strict::append_u8(code, strict::u8(strict::wasm_op::local_get));
                                     strict::append_u32_leb(code, 0u);
                                     strict::append_u8(code, strict::u8(strict::wasm_op::global_set));
                                     strict::append_u32_leb(code, global_index);
                                     strict::append_u8(code, strict::u8(strict::wasm_op::global_get));
                                     strict::append_u32_leb(code, global_index);
                                     strict::append_u8(code, strict::u8(strict::wasm_op::end));
                                     static_cast<void>(module.add_func(::std::move(function_type), ::std::move(function_body)));
                                 }};

        add_roundtrip(k_val_v128, 0u);
        add_roundtrip(strict::k_ref_funcref, 1u);
        add_roundtrip(k_val_externref, 2u);
        return module.build();
    }

    template <typename ValueType>
    [[nodiscard]] strict::byte_vec pack_value(ValueType const& value)
    {
        strict::byte_vec bytes(sizeof(value));
        ::std::memcpy(bytes.data(), ::std::addressof(value), sizeof(value));
        return bytes;
    }

    template <typename ValueType>
    [[nodiscard]] ValueType run_roundtrip(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module,
                                          ::std::uint_least32_t function_index,
                                          ValueType const& value)
    {
        auto const parameters{pack_value(value)};
        ValueType result{};
        ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(
            module, function_index, ::std::addressof(result), sizeof(result), parameters.data(), parameters.size());
        return result;
    }

    [[nodiscard]] int fail(int line, char const* message) noexcept
    {
        ::std::fprintf(stderr, "llvm_jit_local_imported_global_byte_bridge:%d: %s\n", line, message);
        return 1;
    }

#define LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(condition, message) \
    do                                                        \
    {                                                         \
        if(!(condition)) [[unlikely]]                         \
        {                                                     \
            return fail(__LINE__, message);                   \
        }                                                     \
    } while(false)

    [[nodiscard]] int test_local_imported_global_byte_bridge()
    {
        auto wasm{build_roundtrip_module()};
        auto features{strict::make_wasm1p1_feature_parameter()};
        wasm_type::local_imported_t provider{host_globals_module{}};
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"llvm_jit_local_imported_global_byte_bridge", {}, features, {provider})};
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(prepared.mod != nullptr, "runtime module preparation failed");

        wasm_v128 expected_v128{};
        auto expected_v128_bytes{reinterpret_cast<unsigned char*>(::std::addressof(expected_v128))};
        for(::std::size_t index{}; index != sizeof(expected_v128); ++index)
        {
            expected_v128_bytes[index] = static_cast<unsigned char>(index * 29uz + 11uz);
        }
        auto const actual_v128{run_roundtrip(prepared.mod, 0u, expected_v128)};
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(::std::memcmp(::std::addressof(actual_v128), ::std::addressof(expected_v128), sizeof(expected_v128)) == 0,
                                        "v128 byte-buffer roundtrip changed payload bits");
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(host_v128_global::set_count != 0uz && host_v128_global::get_count != 0uz,
                                        "v128 global.set/global.get did not reach the provider");

        wasm_funcref expected_funcref{};
        expected_funcref.ref.storage.func_idx = 0u;
        expected_funcref.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func;
        auto const actual_funcref{run_roundtrip(prepared.mod, 1u, expected_funcref)};
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(actual_funcref.ref.storage.func_idx == expected_funcref.ref.storage.func_idx &&
                                            actual_funcref.ref.kind == expected_funcref.ref.kind,
                                        "funcref byte-buffer roundtrip changed the tagged carrier");
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(host_funcref_global::set_count != 0uz && host_funcref_global::get_count != 0uz,
                                        "funcref dispatch fell outside the provider bridge");

        static int external_object{42};
        wasm_externref expected_externref{};
        expected_externref.ref.storage.ptr = ::std::addressof(external_object);
        expected_externref.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
        auto const actual_externref{run_roundtrip(prepared.mod, 2u, expected_externref)};
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(actual_externref.ref.storage.ptr == expected_externref.ref.storage.ptr &&
                                            actual_externref.ref.kind == expected_externref.ref.kind,
                                        "externref byte-buffer roundtrip changed the tagged carrier");
        LLVM_GLOBAL_BYTE_BRIDGE_REQUIRE(host_externref_global::set_count != 0uz && host_externref_global::get_count != 0uz,
                                        "externref dispatch fell outside the provider bridge");
        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_local_imported_global_byte_bridge();
    }
    catch(...)
    {
        return fail(__LINE__, "uncaught exception");
    }
}
