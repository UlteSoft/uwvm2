#ifndef UWVM2TEST_RUNNER_USE_LLVM_JIT
# define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>

#include <bit>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

#if defined(__unix__) || defined(__APPLE__)
# include <sys/wait.h>
# include <unistd.h>
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
# include <xmmintrin.h>
#endif

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace wasm_type = ::uwvm2::uwvm::wasm::type;

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
# if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
    inline constexpr unsigned flush_control_mask{(1u << 15u) | (1u << 6u)};
# else
    inline constexpr unsigned flush_control_mask{1u << 15u};
# endif
    [[nodiscard]] unsigned read_fp_control() noexcept { return _mm_getcsr(); }
    void write_fp_control(unsigned value) noexcept { _mm_setcsr(value); }
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    inline constexpr ::std::uint_least64_t flush_control_mask{1ull << 24u};
    [[nodiscard]] ::std::uint_least64_t read_fp_control() noexcept
    {
        ::std::uint_least64_t value{};
        __asm__ volatile("mrs %0, fpcr" : "=r"(value));
        return value;
    }
    void write_fp_control(::std::uint_least64_t value) noexcept
    { __asm__ volatile("msr fpcr, %0" : : "r"(value)); }
#else
    inline constexpr unsigned flush_control_mask{};
    [[nodiscard]] unsigned read_fp_control() noexcept { return 0u; }
    void write_fp_control(unsigned) noexcept {}
#endif

    [[nodiscard]] bool poison_fp_environment() noexcept
    {
        if(::std::fesetround(FE_UPWARD) != 0) { return false; }
        write_fp_control(read_fp_control() | flush_control_mask);
        if constexpr(flush_control_mask == 0u) { return ::std::fegetround() == FE_UPWARD; }
        return ::std::fegetround() == FE_UPWARD && (read_fp_control() & flush_control_mask) == flush_control_mask;
    }

    [[nodiscard]] bool flush_controls_enabled() noexcept
    {
        if constexpr(flush_control_mask == 0u) { return true; }
        return (read_fp_control() & flush_control_mask) == flush_control_mask;
    }

    struct initial_environment_restore
    {
        ::std::fenv_t environment{};
        decltype(read_fp_control()) fp_control{};
        bool valid{};

        initial_environment_restore() noexcept
            : fp_control{read_fp_control()}, valid{::std::fegetenv(::std::addressof(environment)) == 0}
        {}

        ~initial_environment_restore() noexcept
        {
            if(valid) { static_cast<void>(::std::fesetenv(::std::addressof(environment))); }
            write_fp_control(fp_control);
        }
    };

    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using feature_list = wasm_type::feature_list<wasm1>;
    using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

    [[nodiscard]] bool internal_bridge_rejects_direct_host_call() noexcept;

    struct poison_environment_import
    {
        // Intentionally no wasm_fp_control_policy: undeclared callbacks must retain the complete environment guard.
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"poison"};
        using result_tuple = wasm_type::import_function_result_tuple_t<feature_list>;
        using parameter_tuple = wasm_type::import_function_parameter_tuple_t<feature_list>;
        using local_imported_function_type = wasm_type::local_imported_function_type_t<result_tuple, parameter_tuple>;

        inline static ::std::size_t call_count{};
        inline static bool poison_succeeded{true};
        inline static ::std::size_t reentry_call_count{};
        inline static bool reentry_environment_preserved{true};
        inline static bool reentry_active{};
        inline static bool callback_suspend_checked{};
        inline static ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* reentry_module{};

        static void call(local_imported_function_type&) noexcept
        {
            ++call_count;
            poison_succeeded = poison_fp_environment() && poison_succeeded;
            if(!callback_suspend_checked)
            {
                if(!internal_bridge_rejects_direct_host_call()) { ::fast_io::fast_terminate(); }
                callback_suspend_checked = true;
            }
            if(!reentry_active)
            {
                if(reentry_module == nullptr) { ::fast_io::fast_terminate(); }
                reentry_active = true;
                ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(reentry_module, 3u, nullptr, 0uz, nullptr, 0uz);
                ++reentry_call_count;
                reentry_environment_preserved =
                    reentry_environment_preserved && ::std::fegetround() == FE_UPWARD && flush_controls_enabled();
                reentry_active = false;
            }
        }
    };

    struct fp_host_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"fp-host"};
        using local_function_tuple = ::uwvm2::utils::container::tuple<poison_environment_import>;
    };

    struct malformed_fp_policy_declaration
    {
        inline static constexpr unsigned wasm_fp_control_policy{};
    };

    struct unknown_fp_policy_declaration
    {
        inline static constexpr wasm_type::local_imported_wasm_fp_control_policy_t wasm_fp_control_policy{
            static_cast<wasm_type::local_imported_wasm_fp_control_policy_t>(0xffu)};
    };

    struct nonconstant_fp_policy_declaration
    {
        inline static wasm_type::local_imported_wasm_fp_control_policy_t wasm_fp_control_policy;
    };

    static_assert(wasm_type::is_local_imported_function<poison_environment_import>);
    static_assert(wasm_type::is_local_imported_module<fp_host_module>);
    static_assert(wasm_type::details::local_imported_function_wasm_fp_control_policy<poison_environment_import>() ==
                  wasm_type::local_imported_wasm_fp_control_policy_t::may_modify);
    static_assert(wasm_type::details::local_imported_function_wasm_fp_control_policy<malformed_fp_policy_declaration>() ==
                  wasm_type::local_imported_wasm_fp_control_policy_t::may_modify);
    static_assert(wasm_type::details::local_imported_function_wasm_fp_control_policy<unknown_fp_policy_declaration>() ==
                  wasm_type::local_imported_wasm_fp_control_policy_t::may_modify);
    static_assert(wasm_type::details::local_imported_function_wasm_fp_control_policy<nonconstant_fp_policy_declaration>() ==
                  wasm_type::local_imported_wasm_fp_control_policy_t::may_modify);

    [[nodiscard]] strict::byte_vec build_fp_module()
    {
        strict::module_builder module{};
        auto op = [&](strict::byte_vec& code, strict::wasm_op opcode) { strict::append_u8(code, strict::u8(opcode)); };
        auto u32 = [&](strict::byte_vec& code, ::std::uint32_t value) { strict::append_u32_leb(code, value); };

        strict::func_type host_type{{}, {}};
        module.types.push_back(host_type);
        module.add_import_func("fp-host", "poison", 0u);

        auto add_binary_function = [&](strict::wasm_op binary_opcode)
        {
            strict::func_type type{{strict::k_val_f32, strict::k_val_f32}, {strict::k_val_f32}};
            strict::func_body body{};
            auto& code{body.code};
            op(code, strict::wasm_op::call);
            u32(code, 0u);
            op(code, strict::wasm_op::local_get);
            u32(code, 0u);
            op(code, strict::wasm_op::local_get);
            u32(code, 1u);
            op(code, binary_opcode);
            op(code, strict::wasm_op::end);
            static_cast<void>(module.add_func(::std::move(type), ::std::move(body)));
        };

        add_binary_function(strict::wasm_op::f32_add);
        add_binary_function(strict::wasm_op::f32_mul);

        // The hostile callback re-enters this no-op through the public raw API. Its index is 3: one imported function
        // followed by the two binary functions above.
        strict::func_type reentry_type{{}, {}};
        strict::func_body reentry_body{};
        op(reentry_body.code, strict::wasm_op::end);
        static_cast<void>(module.add_func(::std::move(reentry_type), ::std::move(reentry_body)));
        return module.build();
    }

    [[nodiscard]] bool internal_bridge_rejects_direct_host_call() noexcept
    {
#if defined(__unix__) || defined(__APPLE__)
        auto const child{::fork()};
        if(child < 0) { return false; }
        if(child == 0)
        {
            ::uwvm2::runtime::lib::details::llvm_jit_call_raw_from_generated_wasm(nullptr, 0u, nullptr, 0uz, nullptr, 0uz);
            ::_exit(127);
        }

        int status{};
        if(::waitpid(child, ::std::addressof(status), 0) != child) { return false; }
        return WIFSIGNALED(status);
#else
        // The generated bridge still enforces the capability on these platforms; process-isolated death-test plumbing
        // is supplied by the platform test runner rather than this portable executable.
        return true;
#endif
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

    [[nodiscard]] int test_llvm_fp_environment() noexcept
    {
        initial_environment_restore restore_initial{};
        if(!restore_initial.valid) { return 1; }

        auto wasm{build_fp_module()};
        wasm_type::local_imported_t host_module{fp_host_module{}};
        if(host_module.function_wasm_fp_control_policy_from_index(0uz) !=
           wasm_type::local_imported_wasm_fp_control_policy_t::may_modify)
        {
            return 12;
        }
        if(host_module.function_wasm_fp_control_policy_from_index(1uz) !=
               wasm_type::local_imported_wasm_fp_control_policy_t::may_modify ||
           host_module.function_wasm_fp_control_policy_from_index(SIZE_MAX) !=
               wasm_type::local_imported_wasm_fp_control_policy_t::may_modify)
        {
            return 16;
        }
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"llvm_aot_fp_environment", {}, {}, {host_module})};
        if(prepared.mod == nullptr) { return 2; }
        poison_environment_import::reentry_module = prepared.mod;

        if(!internal_bridge_rejects_direct_host_call()) { return 13; }

        if(::std::fesetround(FE_DOWNWARD) != 0) { return 3; }
        write_fp_control(read_fp_control() | flush_control_mask);
        if(::std::fegetround() != FE_DOWNWARD || !flush_controls_enabled()) { return 4; }

        // 1.0 + 2^-24 is exactly halfway between adjacent f32 values. Wasm must choose the even 1.0 result even though
        // both the embedding thread and the imported callback select hostile rounding modes.
        auto const half_ulp{::std::bit_cast<float>(::std::uint32_t{0x33800000u})};
        if(run_binary(prepared.mod, 1u, 1.0f, half_ulp) != 0x3f800000u) { return 5; }
        if(::std::fegetround() != FE_DOWNWARD || !flush_controls_enabled()) { return 6; }

        // DAZ must not erase a subnormal operand.
        auto const minimum_subnormal{::std::bit_cast<float>(::std::uint32_t{1u})};
        if(run_binary(prepared.mod, 1u, minimum_subnormal, 0.0f) != 1u) { return 7; }
        if(::std::fegetround() != FE_DOWNWARD || !flush_controls_enabled()) { return 8; }

        // FTZ must not erase a subnormal result.
        auto const minimum_normal{::std::numeric_limits<float>::min()};
        if(run_binary(prepared.mod, 2u, minimum_normal, 0.5f) != 0x00400000u) { return 9; }
        if(::std::fegetround() != FE_DOWNWARD || !flush_controls_enabled()) { return 10; }

        if(poison_environment_import::call_count != 3uz || !poison_environment_import::poison_succeeded) { return 11; }
        if(poison_environment_import::reentry_call_count != 3uz || !poison_environment_import::reentry_environment_preserved) { return 14; }
        if(!poison_environment_import::callback_suspend_checked) { return 15; }
        return 0;
    }
}

int main()
{
    return test_llvm_fp_environment();
}
