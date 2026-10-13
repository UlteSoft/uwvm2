// Same one-run Core 3 bytes as the industry control. Native cold qualification
// of publication V3 must pass first. This is source-only benchmark preparation.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/run/owned_source.h>
#include <fast_io.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace full = ::uwvm2::uwvm::runtime::full;
static void require(bool value, char const* message)
{
    if(!value)
    {
        ::fast_io::io::perrln("FAIL native EH benchmark ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
static ::std::uint32_t decimal32(char const* argument)
{
    ::std::string_view const bytes{argument};
    require(!bytes.empty(), "empty integer");
    ::std::uint32_t value{};
    // [live argument bytes][size() checked by string_view] end
    // [safe] Only this complete host-owned argument extent is scanned.
    //                              ^^ end selects its one-past pointer.
    auto const end{bytes.data() + bytes.size()};
    auto const parsed{::fast_io::parse_by_scan(bytes.data(), end, ::fast_io::mnp::dec_get<true, true>(value))};
    require(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end, "integer syntax/range");
    return value;
}
static constexpr ::std::uint32_t scalar_lcg_result(::std::uint32_t iterations) noexcept
{
    ::std::uint32_t value{}, multiplier{1664525u}, increment{1013904223u};
    while(iterations != 0u)
    {
        if(iterations & 1u) { value = value * multiplier + increment; }
        increment *= multiplier + 1u; multiplier *= multiplier; iterations >>= 1u;
    }
    return value;
}
int main(int argc, char** argv)
{
#if !defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) || UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF != 1 || \
    !defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) || UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER != 1
    static_cast<void>(argc); static_cast<void>(argv);
    ::fast_io::io::perrln("benchmark requires aligned observer=1/private-leaf=1; the source request still selects public/private");
    return 2;
#else
    require(argc == 5, "argv: same.wasm public|private iterations expected-checksum-u32");
    ::std::string_view const selection{argv[2]};
    bool const private_request{selection == "private"};
    require(private_request || selection == "public", "selection");
    auto const iterations{decimal32(argv[3])}, expected_checksum{decimal32(argv[4])};
    require(iterations >= 16u && iterations < 0x80000000u && iterations % 16u == 0u, "iteration bounds");
    require(expected_checksum == scalar_lcg_result(iterations), "independent plan iteration/checksum pairing");
    ::uwvm2test::uwvm_int_strict::configure_llvm_jit_runner_runtime();
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    ::uwvm2::uwvm::io::u8log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
    auto features{::uwvm2test::uwvm_int_strict::make_wasm1p1_feature_parameter()};
    auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    policy.disable_exceptions = false; policy.disable_function_references = false; policy.disable_gc = false;
    ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para = features;
    ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features = features;
    ::uwvm2::utils::cmdline::parameter_parsing_results path{};
    path.str = ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(argv[1])};
    // [live local path descriptor] borrowed only until synchronous preparation.
    // [safe] Clear this global borrow before its actual stack owner retires.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(path);
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"native-eh-leaf-benchmark";
    auto const preparation{::uwvm2::uwvm::run::prepare_owned_full_cli_source()};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    require(preparation == static_cast<int>(::uwvm2::uwvm::run::retval::ok), "real owning source initializer");
    auto owner{full::selected_full_source_owner_pin()};
    require(full::full_source_instance::has_canonical_owner(owner) && owner->initialized_from_actual_state() &&
        owner->actual_full_validation_epoch() == 0u, "canonical source before full compile");
    if(private_request) { require(owner->request_native_eh_private_leaf_after_actual_initializer(), "real source request"); }
    ::std::uint32_t observed{};
    lib::full_compile_run_config config{};
    config.entry_function_index = 1u;
    // [live 4-byte host output][checked original bounded raw result ABI]
    // [safe] This owner remains live through the one synchronous real guest run.
    config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(observed));
    config.entry_abi_buffers.result_bytes = sizeof(observed);
    auto const begin{::std::chrono::steady_clock::now()};
    lib::full_compile_and_run_main_module(u8"native-eh-leaf-benchmark", config);
    auto const end{::std::chrono::steady_clock::now()};
    auto const invocation_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count()};
    auto const view{lib::llvm_jit_full_source_publication_host_api(owner->bound_initialized_main_module_id())};
    require(observed == expected_checksum && view.ready && view.canonical_source && view.publication_owns_source &&
        view.private_eh_leaf_present == private_request && view.private_eh_leaf_actual_binding == private_request &&
        view.private_eh_leaf_clone_count == (private_request ? 1u : 0u) && invocation_ns > 0,
        "actual checksum and real native selection; a fallback is not a private timing pass");
    // Formatting and read-only publication inspection occur after the timer.
    // invocation_ns includes fused validation, JIT and the host entry wrapper;
    // external whole-process counters also include initializer/teardown. Neither
    // number isolates throw/catch cost or a Wasm-only hardware ROI.
    ::fast_io::io::println("native-eh-leaf-benchmark selection=", ::fast_io::mnp::os_c_str(argv[2]),
        " iterations=", ::fast_io::mnp::dec(iterations), " observed-checksum=", ::fast_io::mnp::dec(observed),
        " selfcheck=passed actual-private-binding=", ::fast_io::mnp::dec(static_cast<unsigned>(view.private_eh_leaf_actual_binding)),
        " clones=", ::fast_io::mnp::dec(view.private_eh_leaf_clone_count), " invocation-total-ns=", ::fast_io::mnp::dec(invocation_ns),
        " runs=1 wasm-only-ns=unavailable");
    lib::reset_runtime_state_host_api();
    require(owner->actual_full_validation_epoch() == 0u, "real stop/drain");
    return 0;
#endif
}
