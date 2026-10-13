// Actual owning CLI parse/initializer -> fused full compiler -> MCJIT/relocated
// FDE publication -> ordinary guest entry -> stop/drain. This is a cold native
// component test, never a timing result or a caller-created publication proof.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/run/owned_source.h>
#include <fast_io.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace full = ::uwvm2::uwvm::runtime::full;
namespace threads = ::uwvm2::utils::thread;
static bool reject_actual_cfi{};
static unsigned actual_cfi_checks{}, retired_private_engines{};
static void check(bool value, unsigned line)
{
    if(!value)
    {
        ::fast_io::io::perrln("FAIL private leaf native publication line=", line);
        ::fast_io::fast_terminate();
    }
}
#define PRIVATE_PUBLICATION_CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1 && \
    defined(UWVM2TEST_NATIVE_EH_PRIVATE_LEAF_CFI_FAILURE)
namespace uwvm2::runtime::lib
{
    extern "C++" bool uwvm2test_private_leaf_reject_after_actual_cfi() noexcept
    {
        ++actual_cfi_checks;
        return reject_actual_cfi;
    }
    extern "C++" void uwvm2test_private_leaf_failed_engine_retired() noexcept
    {
        ++retired_private_engines;
    }
}
#endif
static ::std::uint32_t run(::std::size_t function, ::std::uint32_t input, bool parameter = true)
{
    ::std::uint32_t output{};
    lib::full_compile_run_config config{};
    config.entry_function_index = function;
    // [live host input/output scalars][checked original bounded host ABI]
    // [safe] These stack allocations remain live throughout the synchronous
    // real guest entry. No guest pointer or publication permission is supplied.
    if(parameter)
    {
        config.entry_abi_buffers.param_buffer = reinterpret_cast<::std::byte const*>(::std::addressof(input));
        config.entry_abi_buffers.param_bytes = sizeof(input);
    }
    config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
    config.entry_abi_buffers.result_bytes = sizeof(output);
    lib::full_compile_and_run_main_module(u8"private-leaf-native", config);
    return output;
}
static full::full_source_instance::owner prepare(char const* file)
{
    ::uwvm2::utils::cmdline::parameter_parsing_results path{};
    path.str = ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(file)};
    // [host-owned path descriptor] borrowed only by synchronous prepare. Clear
    // the global borrow before the descriptor retires on this function's return.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(path);
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"private-leaf-native";
    auto const result{::uwvm2::uwvm::run::prepare_owned_full_cli_source()};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    PRIVATE_PUBLICATION_CHECK(result == static_cast<int>(::uwvm2::uwvm::run::retval::ok));
    auto owner{full::selected_full_source_owner_pin()};
    PRIVATE_PUBLICATION_CHECK(full::full_source_instance::has_canonical_owner(owner) &&
        owner->initialized_from_actual_state() && owner->actual_full_validation_epoch() == 0u);
    return owner;
}
int main(int argc, char** argv)
{
#if !defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) || UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER != 1 || \
    !defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) || UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF != 1 || \
    !defined(UWVM2TEST_NATIVE_EH_PRIVATE_LEAF_CFI_FAILURE)
    static_cast<void>(argc); static_cast<void>(argv);
    ::fast_io::io::perrln("publication fixture requires observer=1/private-leaf=1/actual-CFI-failure seam");
    return 2;
#else
    PRIVATE_PUBLICATION_CHECK(argc == 3 || argc == 4);
    auto const test{::std::string_view{argv[2]}};
    bool const ordinary{test == "public"}, rollback{test == "rollback"}, debug_decline{test == "debug-decline"};
    bool const uncaught{test == "uncaught"}, instruction_decline{test == "instruction-decline"};
    PRIVATE_PUBLICATION_CHECK(ordinary || rollback || debug_decline || instruction_decline || uncaught || test == "private");
    ::uwvm2test::uwvm_int_strict::configure_llvm_jit_runner_runtime();
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = instruction_decline ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    auto const workers{argc == 4 ? ::std::string_view{argv[3]} : ::std::string_view{"serial"}};
    PRIVATE_PUBLICATION_CHECK(workers == "serial" || workers == "parallel");
    mode::global_runtime_compile_threads = workers == "parallel" ? 2u : 0u; mode::runtime_compile_threads_existed = true;
    ::uwvm2::uwvm::io::u8log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
    auto features{::uwvm2test::uwvm_int_strict::make_wasm1p1_feature_parameter()};
    auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    policy.disable_exceptions = false; policy.disable_function_references = false; policy.disable_gc = false;
    ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para = features;
    ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features = features;
    auto owner{prepare(argv[1])};
    if(!ordinary) { PRIVATE_PUBLICATION_CHECK(owner->request_native_eh_private_leaf_after_actual_initializer()); }
    reject_actual_cfi = rollback;
    ::std::shared_ptr<threads::cooperative_pause_domain> control{};
    if(debug_decline)
    {
        control = ::std::make_shared<threads::cooperative_pause_domain>(1u);
        PRIVATE_PUBLICATION_CHECK(lib::llvm_jit_configure_debug_safe_points_host_api(control) == lib::llvm_jit_debug_configure_result::ok);
    }
    PRIVATE_PUBLICATION_CHECK(run(1u, 1u) == 49u && run(1u, 0u) == 82u);
    auto const module{owner->bound_initialized_main_module_id()};
    auto const view{lib::llvm_jit_full_source_publication_host_api(module)};
    PRIVATE_PUBLICATION_CHECK(module == 0u && view.ready && view.canonical_source && view.publication_owns_source &&
        view.engine_address != 0u && view.llvm_context_address != 0u && !view.pending_numeric_plan);
    bool const selected{!ordinary && !rollback && !debug_decline && !instruction_decline};
    PRIVATE_PUBLICATION_CHECK(view.private_eh_leaf_present == selected && view.private_eh_leaf_actual_binding == selected &&
        view.private_eh_leaf_clone_count == (selected ? 1u : 0u));
    PRIVATE_PUBLICATION_CHECK(actual_cfi_checks == (ordinary || debug_decline || instruction_decline ? 0u : 1u) &&
        retired_private_engines == (rollback ? 1u : 0u));
    if(selected)
    {
        PRIVATE_PUBLICATION_CHECK(owner->actual_full_validation_epoch() == view.runtime_epoch &&
            !owner->request_native_eh_private_leaf_after_actual_initializer());
        auto fresh_control{::std::make_shared<threads::cooperative_pause_domain>(1u)};
        PRIVATE_PUBLICATION_CHECK(lib::llvm_jit_configure_debug_safe_points_host_api(fresh_control) ==
            lib::llvm_jit_debug_configure_result::already_published);
        PRIVATE_PUBLICATION_CHECK(!lib::llvm_jit_prepare_debug_host_api() && !lib::llvm_jit_enable_debug_native_step_host_api() &&
            !lib::llvm_jit_capture_debug_native_step_site_host_api().valid &&
            !lib::llvm_jit_debug_safe_points_host_api(module, 1u) && !lib::llvm_jit_debug_bind_source_host_api(module));
        ::std::array<::std::byte, 4u> replacement{::std::byte{0u}, ::std::byte{0x41u}, ::std::byte{1u}, ::std::byte{0x0bu}};
        auto const prepared{lib::llvm_jit_debug_prepare_function_replacement_host_api(module, 0u, 1u,
            replacement.data(), replacement.size())};
        PRIVATE_PUBLICATION_CHECK(prepared.status == lib::llvm_jit_debug_replace_status::unsupported_mode && !prepared.transaction);
        PRIVATE_PUBLICATION_CHECK(lib::llvm_jit_debug_commit_function_replacement_host_api(nullptr).status ==
            lib::llvm_jit_debug_replace_status::unsupported_mode);
    }
    // Public ordinary entry and ref-retaining caller remain unmodified: both
    // exact tags/payloads still use their first real matching ordered handlers.
    PRIVATE_PUBLICATION_CHECK(run(2u, 0u, false) == 107u && run(3u, 1u) == 49u && run(3u, 0u) == 82u);
    if(uncaught)
    {
        ::fast_io::io::println("uncaught control: calling original public leaf after actual private publication");
        static_cast<void>(run(0u, 1u));
        PRIVATE_PUBLICATION_CHECK(false); // Expected detailed fatal, never an exit-0 PASS.
    }
    lib::reset_runtime_state_host_api();
    auto const retired{lib::llvm_jit_full_source_publication_host_api(module)};
    PRIVATE_PUBLICATION_CHECK(!retired.private_eh_leaf_present && !retired.private_eh_leaf_actual_binding &&
        retired.private_eh_leaf_clone_count == 0u && owner->actual_full_validation_epoch() == 0u);
    if(control) { PRIVATE_PUBLICATION_CHECK(control->is_closed()); }
    auto fresh{prepare(argv[1])};
    PRIVATE_PUBLICATION_CHECK(fresh.get() != owner.get() &&
        (fresh.owner_before(owner) || owner.owner_before(fresh)) && !fresh->native_eh_private_leaf_requested());
    // A fresh genuine initializer needs its own request; old source observation
    // supplies no new generation permission. Execute its original public path.
    PRIVATE_PUBLICATION_CHECK(run(1u, 1u) == 49u && !lib::llvm_jit_full_source_publication_host_api(
        fresh->bound_initialized_main_module_id()).private_eh_leaf_present);
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("PASS actual native publication mode=", ::fast_io::mnp::os_c_str(argv[2]),
        " workers=", ::fast_io::mnp::os_c_str(argc == 4 ? argv[3] : "serial"),
        " exact_tags/payloads public_refcatch original_fallback source_generation stop_drain; timing=false");
    return 0;
#endif
}
