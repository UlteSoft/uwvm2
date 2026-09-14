// Exercise the production full/lazy entries, not a header-only interpreter runner. Build this test both with and
// without LLVM, and with both TLS representations: selecting uwvm-int in a combined binary used to disable the guard.
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../strict/uwvm_int_translate_strict_common.h"
#include "../../0008.imported/wasi/wasip1/func/fp_control_probe.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_local_imported_provider_callbacks.h>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace fp = ::uwvm2test::wasip1_fp_control;
    namespace runtime = ::uwvm2::runtime::lib;
    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
    namespace type = ::uwvm2::uwvm::wasm::type;
    using features = type::feature_list<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>;

    ::std::size_t callback_count{};
    bool callback_controls_valid{true};

    void poison_fp_control() noexcept
    {
        ++callback_count;
        callback_controls_valid = callback_controls_valid && ::std::fesetround(fp::hostile_rounding_up) == 0;
        fp::enable_flush_control();
    }

    template <typename Float>
    struct hostile_global
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{sizeof(Float) == 4u ? u8"f32" : u8"f64"};
        inline static constexpr bool is_mutable{true};
        using value_type = Float;
        Float value{};
        friend Float global_get(hostile_global& global) noexcept { poison_fp_control(); return global.value; }
        friend void global_set(hostile_global& global, Float value) noexcept { poison_fp_control(); global.value = value; }
    };

    struct hostile_function
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"poison"};
        using result_tuple = type::import_function_result_tuple_t<features>;
        using parameter_tuple = type::import_function_parameter_tuple_t<features>;
        using local_imported_function_type = type::local_imported_function_type_t<result_tuple, parameter_tuple>;
        static void call(local_imported_function_type&) noexcept { poison_fp_control(); }
    };

    struct provider_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"fp-host"};
        using local_function_tuple = ::uwvm2::utils::container::tuple<hostile_function>;
        // GCC's precise Wasm carriers may be distinct _Float32/_Float64 types, not float/double.
        using local_global_tuple = ::uwvm2::utils::container::tuple<
            hostile_global<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>,
            hostile_global<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>>;
        local_global_tuple local_global{};
    };
    static_assert(type::has_local_global_tuple<provider_module>);

    enum class boundary : unsigned { entry, global_get, global_set, function_call };

    [[nodiscard]] strict::byte_vec build_module()
    {
        strict::module_builder builder{};
        builder.types.push_back({{}, {}});
        builder.add_import_func("fp-host", "poison", 0u);
        builder.add_import_global("fp-host", "f32", strict::k_val_f32, true);
        builder.add_import_global("fp-host", "f64", strict::k_val_f64, true);
        for(unsigned wide{}; wide != 2u; ++wide)
        {
            auto const value_type{wide == 0u ? strict::k_val_f32 : strict::k_val_f64};
            for(unsigned multiply{}; multiply != 2u; ++multiply)
            {
                for(unsigned operation{}; operation != 4u; ++operation)
                {
                    strict::func_type signature{{value_type, value_type}, {value_type}};
                    strict::func_body body{};
                    auto const op{[&](strict::wasm_op value) { strict::append_u8(body.code, strict::u8(value)); }};
                    auto const imm{[&](unsigned value) { strict::append_u32_leb(body.code, value); }};
                    switch(static_cast<boundary>(operation))
                    {
                        case boundary::entry: break;
                        case boundary::global_get:
                            op(strict::wasm_op::global_get); imm(wide); op(strict::wasm_op::drop); break;
                        case boundary::global_set:
                            op(strict::wasm_op::local_get); imm(0u); op(strict::wasm_op::global_set); imm(wide); break;
                        case boundary::function_call: op(strict::wasm_op::call); imm(0u); break;
                    }
                    op(strict::wasm_op::local_get); imm(0u);
                    op(strict::wasm_op::local_get); imm(1u);
                    op(wide == 0u ? (multiply == 0u ? strict::wasm_op::f32_add : strict::wasm_op::f32_mul)
                                  : (multiply == 0u ? strict::wasm_op::f64_add : strict::wasm_op::f64_mul));
                    op(strict::wasm_op::end);
                    static_cast<void>(builder.add_func(::std::move(signature), ::std::move(body)));
                }
            }
        }
        return builder.build();
    }

    template <typename Float, typename Bits>
    [[nodiscard]] bool check_entry(unsigned function_index, Float left, Float right, Bits expected, bool lazy)
    {
        Float const parameters[2]{left, right};
        Float result{};
        runtime::entry_function_abi_buffers buffers{reinterpret_cast<::std::byte const*>(parameters), sizeof(parameters),
                                                    reinterpret_cast<::std::byte*>(::std::addressof(result)), sizeof(result)};
        if(lazy) { runtime::lazy_compile_and_run_main_module(u8"fp-main", {function_index, buffers, false}); }
        else { runtime::full_compile_and_run_main_module(u8"fp-main", {function_index, buffers}); }
        auto const actual{::std::bit_cast<Bits>(result)};
        if(actual == expected) { return true; }
        ::std::fprintf(stderr, "uwvm_int_fp_environment: lazy=%d function=%u expected=%llx actual=%llx\n",
                       lazy, function_index, static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
        return false;
    }

    [[nodiscard]] int run_suite(mode::runtime_compiler_t backend, bool lazy)
    {
        runtime::reset_runtime_state_host_api();
        auto wasm{build_module()};
        type::local_imported_t provider{provider_module{}};
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"fp-main", {}, {}, {provider})};
        struct reset_on_exit { ~reset_on_exit() { runtime::reset_runtime_state_host_api(); } } reset_guard{};
        if(prepared.mod == nullptr) { return 1; }
        mode::global_runtime_compiler = backend;
        mode::global_runtime_mode = lazy ? mode::runtime_mode_t::lazy_compile : mode::runtime_mode_t::full_compile;
        mode::global_runtime_compile_threads_resolved = 16uz;
#if defined(UWVM_RUNTIME_LLVM_JIT)
        mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
#endif
        callback_count = 0uz;
        for(unsigned hostile_host{}; hostile_host != 2u; ++hostile_host)
        {
            for(unsigned index{}; index != 16u; ++index)
            {
                if(::std::fesetenv(FE_DFL_ENV) != 0) { return 2; }
                fp::snapshot host{};
                if(hostile_host != 0u && !fp::prepare_hostile(host)) { return 3; }
                host = {::std::fegetround(), fp::read_arch_control()};
                if(::std::feraiseexcept(FE_INVALID) != 0) { return 4; }
                auto const host_exceptions{::std::fetestexcept(FE_ALL_EXCEPT)};
                bool const multiply{(index & 4u) != 0u};
                bool ok{};
                if(index < 8u)
                {
                    ok = check_entry(index + 1u, multiply ? ::std::numeric_limits<float>::min() : 1.0f,
                                     multiply ? 0.5f : ::std::bit_cast<float>(::std::uint32_t{0x33800000u}),
                                     ::std::uint32_t{multiply ? 0x00400000u : 0x3f800000u}, lazy);
                }
                else
                {
                    ok = check_entry(index + 1u, multiply ? ::std::numeric_limits<double>::min() : 1.0,
                                     multiply ? 0.5 : ::std::bit_cast<double>(::std::uint64_t{0x3ca0000000000001ull}),
                                     ::std::uint64_t{multiply ? 0x0008000000000000ull : 0x3ff0000000000001ull}, lazy);
                }
                if(!ok || !fp::unchanged(host) || ::std::fetestexcept(FE_ALL_EXCEPT) != host_exceptions) { return 5; }
            }
        }
#if ((defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC) && !defined(_SOFT_FLOAT)) || \
    (defined(__m68k__) && defined(__HAVE_68881__))
        // Build byte-ABI inputs using integers: on m68k a hostile PC=24 also rounds FMOVE into FP registers,
        // so a C++ double argument could lose bits before the runtime even gets control.
        ::std::uint64_t const raw_parameters[]{0x3ff0000000000000ull, 0x3ca0000000000001ull};
        ::std::uint64_t raw_result{};
        if(::std::fesetenv(FE_DFL_ENV) != 0) { return 7; }
# if defined(__m68k__)
        unsigned const narrow_control{0x40u};
        unsigned restored_control{};
        unsigned original_control{};
        __asm__ volatile("fmove.l %%fpcr,%0" : "=dm"(original_control) : : "memory");
        __asm__ volatile("fmove.l %0,%%fpcr" : : "dm"(narrow_control) : "memory");
# else
        unsigned short const narrow_control{0x007fu};
        unsigned short restored_control{};
        __asm__ volatile("fnclex; fldcw %0" : : "m"(narrow_control) : "memory");
# endif
        runtime::entry_function_abi_buffers narrow_buffers{reinterpret_cast<::std::byte const*>(raw_parameters), sizeof(raw_parameters),
            reinterpret_cast<::std::byte*>(&raw_result), sizeof(raw_result)};
        if(lazy) { runtime::lazy_compile_and_run_main_module(u8"fp-main", {9u, narrow_buffers, false}); }
        else { runtime::full_compile_and_run_main_module(u8"fp-main", {9u, narrow_buffers}); }
# if defined(__m68k__)
        __asm__ volatile("fmove.l %%fpcr,%0" : "=dm"(restored_control) : : "memory");
# else
        __asm__ volatile("fnstcw %0" : "=m"(restored_control) : : "memory");
# endif
        auto const restore_status{::std::fesetenv(FE_DFL_ENV)};
# if defined(__m68k__)
        // Restore the test's precision explicitly too: libc's multi-CSR transfer under QEMU can leave PC=24
        // active, corrupting C++ input preparation for the NEXT suite before its byte-ABI entry is called.
        __asm__ volatile("fmove.l %0,%%fpcr" : : "dm"(original_control) : "memory");
# endif
        if(restore_status != 0 || restored_control != narrow_control || raw_result != 0x3ff0000000000001ull) { return 8; }
#endif
        return callback_count == 24uz && callback_controls_valid ? 0 : 6;
    }
}

int main()
{
    fp::initial_state_restore restore_initial{};
    if(!restore_initial.valid) { return 1; }
#if defined(UWVM_RUNTIME_LLVM_JIT)
    // Combined-build header-only runners have no active production-entry marker. The global bridge must still
    // restore the incoming control state and must not depend on that marker being set.
    {
        type::local_imported_t provider{provider_module{}};
        fp::snapshot host{};
        if(!fp::prepare_hostile(host)) { return 2; }
        float value{};
        runtime::details::invoke_local_imported_provider_global_get(&provider, 0uz, reinterpret_cast<::std::byte*>(&value));
        if(!fp::unchanged(host)) { return 3; }
        if(!runtime::details::invoke_local_imported_provider_global_set(&provider, 0uz, reinterpret_cast<::std::byte const*>(&value)) ||
           !fp::unchanged(host)) { return 4; }
    }
#endif
    for(bool lazy : {false, true})
    {
        if(auto const result{run_suite(mode::runtime_compiler_t::uwvm_interpreter_only, lazy)}; result != 0) { return result; }
#if defined(UWVM_RUNTIME_LLVM_JIT)
        if(auto const result{run_suite(mode::runtime_compiler_t::llvm_jit_only, lazy)}; result != 0) { return result; }
#endif
    }
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    if(auto const result{run_suite(mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered, true)}; result != 0) { return result; }
#endif
    return 0;
}

#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
