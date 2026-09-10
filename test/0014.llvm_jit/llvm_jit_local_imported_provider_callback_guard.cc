#ifndef UWVM2TEST_RUNNER_USE_LLVM_JIT
# define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include "../0008.imported/wasi/wasip1/func/fp_control_probe.h"
#include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_local_imported_provider_callbacks.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>

#include <array>
#include <bit>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

#if defined(__unix__) || defined(__APPLE__)
# include <sys/wait.h>
# include <unistd.h>
#endif

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace fp_probe = ::uwvm2test::wasip1_fp_control;
    namespace wasm_type = ::uwvm2::uwvm::wasm::type;

    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using feature_list = wasm_type::feature_list<wasm1>;
    using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

    struct hostile_signature_function
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"signature"};
        using result_tuple = wasm_type::import_function_result_tuple_t<feature_list, value_type::f32>;
        using parameter_tuple = wasm_type::import_function_parameter_tuple_t<feature_list, value_type::i32, value_type::f64>;
        using local_imported_function_type = wasm_type::local_imported_function_type_t<result_tuple, parameter_tuple>;

        static void call(local_imported_function_type&) noexcept {}
    };

    inline ::std::size_t provider_callback_count{};
    inline bool provider_poison_succeeded{true};
    inline bool attempt_generated_raw_reentry{};
    inline ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* reentry_module{};

    [[nodiscard]] bool flush_control_enabled() noexcept
    {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        constexpr ::std::uint_least64_t ftz_mask{1ull << 15u};
# if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
        constexpr ::std::uint_least64_t daz_mask{1ull << 6u};
        return (fp_probe::read_arch_control() & (ftz_mask | daz_mask)) == (ftz_mask | daz_mask);
# else
        return (fp_probe::read_arch_control() & ftz_mask) == ftz_mask;
# endif
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        constexpr ::std::uint_least64_t fz_mask{1ull << 24u};
        return (fp_probe::read_arch_control() & fz_mask) == fz_mask;
#else
        return true;
#endif
    }

    void provider_callback() noexcept
    {
        ++provider_callback_count;
        auto const rounding_changed{::std::fesetround(FE_UPWARD) == 0};
        fp_probe::enable_flush_control();
        provider_poison_succeeded = provider_poison_succeeded && rounding_changed && ::std::fegetround() == FE_UPWARD && flush_control_enabled();

        if(attempt_generated_raw_reentry)
        {
            if(reentry_module == nullptr) { ::fast_io::fast_terminate(); }
            // This internal bridge is valid only while the generated-only depth token is live. A provider callback must
            // see that token suspended, even though the surrounding LLVM-Wasm FP scope remains active.
            ::uwvm2::runtime::lib::details::llvm_jit_call_raw_from_generated_wasm(
                reentry_module, 1u, nullptr, 0uz, nullptr, 0uz);
        }
    }

    struct hostile_global
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{u8"value"};
        inline static constexpr bool is_mutable{true};
        using value_type = wasm_f32;

        value_type value{};

        friend value_type global_get(hostile_global& global) noexcept
        {
            provider_callback();
            return global.value;
        }

        friend void global_set(hostile_global& global, value_type value) noexcept
        {
            provider_callback();
            global.value = value;
        }
    };

    struct hostile_memory
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"memory"};
        inline static constexpr ::std::uint_least64_t page_size{65536u};

        ::std::array<::std::byte, static_cast<::std::size_t>(page_size)> bytes{};

        friend bool memory_grow(hostile_memory&, ::std::uint_least64_t delta_pages) noexcept
        {
            provider_callback();
            return delta_pages == 0u;
        }

        friend ::std::byte* memory_begin(hostile_memory& memory) noexcept
        {
            provider_callback();
            return memory.bytes.data();
        }

        friend ::std::uint_least64_t memory_size(hostile_memory&) noexcept
        {
            provider_callback();
            return 1u;
        }
    };

    struct hostile_provider_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"provider-guard-host"};
        using local_function_tuple = ::uwvm2::utils::container::tuple<hostile_signature_function>;
        using local_global_tuple = ::uwvm2::utils::container::tuple<hostile_global>;
        using local_memory_tuple = ::uwvm2::utils::container::tuple<hostile_memory>;
        local_global_tuple local_global{};
        local_memory_tuple local_memory{};
    };

    static_assert(wasm_type::is_local_imported_global<hostile_global>);
    static_assert(wasm_type::is_local_imported_memory<hostile_memory>);
    static_assert(wasm_type::is_local_imported_function<hostile_signature_function>);
    static_assert(wasm_type::is_local_imported_module<hostile_provider_module>);

    [[nodiscard]] strict::byte_vec build_provider_guard_module()
    {
        strict::module_builder module{};
        module.add_import_memory("provider-guard-host", "memory", 1u, 1u, true);
        module.add_import_global("provider-guard-host", "value", strict::k_val_f32, true);

        auto const op{[](strict::byte_vec& code, strict::wasm_op opcode) { strict::append_u8(code, strict::u8(opcode)); }};
        auto const u32{[](strict::byte_vec& code, ::std::uint32_t value) { strict::append_u32_leb(code, value); }};
        auto const i32{[](strict::byte_vec& code, ::std::int32_t value) { strict::append_i32_leb(code, value); }};

        auto const add_probe{[&](strict::wasm_op arithmetic_opcode)
                             {
                                 strict::func_type type{{strict::k_val_f32, strict::k_val_f32}, {strict::k_val_f32}};
                                 strict::func_body body{};
                                 auto& code{body.code};

                                 op(code, strict::wasm_op::i32_const); i32(code, 0);
                                 op(code, strict::wasm_op::i32_const); i32(code, 17);
                                 op(code, strict::wasm_op::i32_store); u32(code, 2u); u32(code, 0u);
                                 op(code, strict::wasm_op::i32_const); i32(code, 0);
                                 op(code, strict::wasm_op::i32_load); u32(code, 2u); u32(code, 0u);
                                 op(code, strict::wasm_op::drop);

                                 op(code, strict::wasm_op::global_get); u32(code, 0u);
                                 op(code, strict::wasm_op::drop);
                                 op(code, strict::wasm_op::local_get); u32(code, 0u);
                                 op(code, strict::wasm_op::global_set); u32(code, 0u);

                                 op(code, strict::wasm_op::memory_size); u32(code, 0u);
                                 op(code, strict::wasm_op::drop);
                                 op(code, strict::wasm_op::i32_const); i32(code, 0);
                                 op(code, strict::wasm_op::memory_grow); u32(code, 0u);
                                 op(code, strict::wasm_op::drop);

                                 op(code, strict::wasm_op::local_get); u32(code, 0u);
                                 op(code, strict::wasm_op::local_get); u32(code, 1u);
                                 op(code, arithmetic_opcode);
                                 op(code, strict::wasm_op::end);
                                 static_cast<void>(module.add_func(::std::move(type), ::std::move(body)));
                             }};

        add_probe(strict::wasm_op::f32_add);

        // A valid provider-side raw re-entry target. It must remain unreachable through the internal generated bridge
        // while any provider callback is active, but the death test would return normally if token suspension regressed.
        strict::func_type no_op_type{{}, {}};
        strict::func_body no_op_body{};
        op(no_op_body.code, strict::wasm_op::end);
        static_cast<void>(module.add_func(::std::move(no_op_type), ::std::move(no_op_body)));

        add_probe(strict::wasm_op::f32_mul);
        return module.build();
    }

    template <typename Value>
    void append_abi_value(strict::byte_vec& bytes, Value value)
    {
        auto const old_size{bytes.size()};
        bytes.resize(old_size + sizeof(Value));
        ::std::memcpy(bytes.data() + old_size, ::std::addressof(value), sizeof(Value));
    }

    [[nodiscard]] ::std::uint32_t run_binary(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module,
                                             ::std::uint_least32_t function_index,
                                             float left,
                                             float right) noexcept
    {
        strict::byte_vec parameters{};
        parameters.reserve(sizeof(float) * 2u);
        append_abi_value(parameters, left);
        append_abi_value(parameters, right);

        float result{};
        ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(
            module, function_index, ::std::addressof(result), sizeof(result), parameters.data(), parameters.size());
        return ::std::bit_cast<::std::uint32_t>(result);
    }

    [[nodiscard]] bool provider_generated_raw_reentry_is_rejected(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module) noexcept
    {
#if defined(__unix__) || defined(__APPLE__)
        auto const child{::fork()};
        if(child < 0) { return false; }
        if(child == 0)
        {
            attempt_generated_raw_reentry = true;
            float const parameters[2]{1.0f, 0.0f};
            float result{};
            ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(
                module, 0u, ::std::addressof(result), sizeof(result), parameters, sizeof(parameters));
            ::_exit(0);
        }

        int status{};
        if(::waitpid(child, ::std::addressof(status), 0) != child) { return false; }
        return WIFSIGNALED(status);
#else
        // Process-isolated death-test plumbing for non-POSIX targets belongs to the platform runner. The runtime
        // precondition is still checked on every platform by llvm_jit_call_raw_from_generated_wasm.
        static_cast<void>(module);
        return true;
#endif
    }

    [[nodiscard]] int fail(int line, char const* message) noexcept
    {
        ::std::fprintf(stderr, "llvm_jit_local_imported_provider_callback_guard:%d: %s\n", line, message);
        return 1;
    }

#define LLVM_PROVIDER_GUARD_REQUIRE(condition, message) \
    do                                                    \
    {                                                     \
        if(!(condition)) [[unlikely]]                     \
        {                                                 \
            return fail(__LINE__, message);               \
        }                                                 \
    } while(false)

    [[nodiscard]] int test_owned_provider_signature_snapshot()
    {
        wasm_type::local_imported_t provider{hostile_provider_module{}};
        auto const source{provider.get_function_information_from_index(0uz)};
        LLVM_PROVIDER_GUARD_REQUIRE(source.successed, "failed to get the provider signature source");

        ::uwvm2::runtime::lib::details::local_imported_provider_function_signature_t snapshot{};
        LLVM_PROVIDER_GUARD_REQUIRE(
            ::uwvm2::runtime::lib::details::invoke_local_imported_provider_function_signature(::std::addressof(provider), 0uz, snapshot),
            "failed to acquire the owned provider signature snapshot");
        LLVM_PROVIDER_GUARD_REQUIRE(snapshot.parameter_types.size() == 2uz && snapshot.result_types.size() == 1uz,
                                    "owned provider signature has the wrong arity");
        LLVM_PROVIDER_GUARD_REQUIRE(snapshot.parameter_types[0] == static_cast<::std::uint_least8_t>(value_type::i32) &&
                                        snapshot.parameter_types[1] == static_cast<::std::uint_least8_t>(value_type::f64) &&
                                        snapshot.result_types[0] == static_cast<::std::uint_least8_t>(value_type::f32),
                                    "owned provider signature has the wrong value types");
        LLVM_PROVIDER_GUARD_REQUIRE(static_cast<void const*>(snapshot.parameter_types.data()) !=
                                        static_cast<void const*>(source.function_type.parameter.begin) &&
                                        static_cast<void const*>(snapshot.result_types.data()) !=
                                        static_cast<void const*>(source.function_type.result.begin),
                                    "provider signature snapshot still aliases provider-owned storage");

        provider.clear();
        LLVM_PROVIDER_GUARD_REQUIRE(snapshot.parameter_types[0] == static_cast<::std::uint_least8_t>(value_type::i32) &&
                                        snapshot.parameter_types[1] == static_cast<::std::uint_least8_t>(value_type::f64) &&
                                        snapshot.result_types[0] == static_cast<::std::uint_least8_t>(value_type::f32),
                                    "owned provider signature changed after provider destruction");
        return 0;
    }

    [[nodiscard]] int test_provider_callback_guard()
    {
        fp_probe::initial_state_restore restore_initial{};
        LLVM_PROVIDER_GUARD_REQUIRE(restore_initial.valid, "failed to save the initial FP environment");

        auto wasm{build_provider_guard_module()};
        wasm_type::local_imported_t provider{hostile_provider_module{}};
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"llvm_jit_provider_callback_guard", {}, {}, {provider})};
        LLVM_PROVIDER_GUARD_REQUIRE(prepared.mod != nullptr, "runtime module preparation failed");
        reentry_module = prepared.mod;

        fp_probe::snapshot expected_host_control{};
        LLVM_PROVIDER_GUARD_REQUIRE(fp_probe::prepare_hostile(expected_host_control), "failed to install the host FP control probe");

        // The callback changes rounding and FTZ/DAZ before the final arithmetic instruction. Generated Wasm must still
        // execute with round-to-nearest and gradual underflow, while the embedding thread gets its own controls back.
        auto const half_ulp{::std::bit_cast<float>(::std::uint32_t{0x33800000u})};
        LLVM_PROVIDER_GUARD_REQUIRE(run_binary(prepared.mod, 0u, 1.0f, half_ulp) == 0x3f800000u,
                                    "provider rounding change escaped into generated Wasm");
        LLVM_PROVIDER_GUARD_REQUIRE(fp_probe::unchanged(expected_host_control), "host FP control was not restored after the rounding probe");

        auto const minimum_normal{::std::numeric_limits<float>::min()};
        LLVM_PROVIDER_GUARD_REQUIRE(run_binary(prepared.mod, 2u, minimum_normal, 0.5f) == 0x00400000u,
                                    "provider FTZ change escaped into generated Wasm");
        LLVM_PROVIDER_GUARD_REQUIRE(fp_probe::unchanged(expected_host_control), "host FP control was not restored after the FTZ probe");
        LLVM_PROVIDER_GUARD_REQUIRE(provider_callback_count != 0uz && provider_poison_succeeded,
                                    "hostile provider callbacks did not exercise the expected FP controls");

        LLVM_PROVIDER_GUARD_REQUIRE(provider_generated_raw_reentry_is_rejected(prepared.mod),
                                    "provider callback inherited the generated-only raw bridge token");
        return 0;
    }
}  // namespace

int main()
{
    if(auto const result{test_owned_provider_signature_snapshot()}; result != 0) { return result; }
    return test_provider_callback_guard();
}
