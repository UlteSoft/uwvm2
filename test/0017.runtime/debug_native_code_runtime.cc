// Genuine runtime code-copy witness, derived from the actual entry/exit/capture fixture.
// New readonly API accepts a private event owner, never a caller site/PC/range.
// This source has not been compiled/run locally. Native Linux qualification uses
// the SAME fresh runtime/CLI layout macros, original official WAT, and two policies.
// Genuine debug-full native entry/exit/capture witness. The official Wasm
// fixture uses Core 3 return_call and recursive calls. No CFA/source/PC guesses.
#include <uwvm2/uwvm/run/owned_source.h>
#include <fast_io.h>
#include <uwvm2/utils/container/string_concat.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <thread>
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace threads = uwvm2::utils::thread;
using domain = threads::cooperative_pause_domain;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_native_code_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{}; lib::llvm_jit_debug_activation_capture_owner capture{};
    ::std::size_t serial{}; bool done{};
    static void stop(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location) noexcept
    {
        // [runtime-owned host callback context] end
        // [safe                               ] this immutable provider owns the
        //  ^^ observer; no guest-supplied address or pointer advance.
        auto& self{*static_cast<observer*>(opaque)};
        auto ticket{self.control->request_pause()}; check(static_cast<bool>(ticket), "real request pause");
        ::std::lock_guard lock{self.mutex}; self.ticket = ::std::move(ticket); self.capture.reset();
        ++self.serial; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        // [same runtime-owned host callback context] end
        // [safe                                    ] borrowed synchronously;
        //  ^^ mint only while the runtime's genuine before-park marker is live.
        auto& self{*static_cast<observer*>(opaque)};
        ::std::lock_guard lock{self.mutex};
        self.capture = lib::llvm_jit_debug_capture_activation_host_api(self.ticket);
    }
};
int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 2; }
    bool const gc_roots{argc == 4};
    if(gc_roots && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])} != "gc-roots") { return 2; }
    ::std::optional<::uwvm2::runtime::gc::scoped_cli_gc_execution> root_launch{};
    if(gc_roots) { root_launch.emplace(); }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    uwvm2::utils::cmdline::parameter_parsing_results path{};
    // [host-owned terminated argv[1]] argv_end
    // [safe                        ] code_cvt scans the host argument once;
    //  ^^ the owning UTF-8 string remains live through every guest join/reset.
    // The view borrows char8_t storage, never an aliased char pointer or a
    // temporary concat result whose storage could expire before initialization.
    auto const owned_wasm_path{::uwvm2::utils::container::u8concat_uwvm(
        ::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    path.str = uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(owned_wasm_path.c_str())};
    path.type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
    auto& parsed_arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old global parsing-result allocation][no guest/observer has started]
    // [safe ] retire the old cursor BEFORE clear/emplace can invalidate it.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    parsed_arguments.clear();
    parsed_arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"uwvm-debug-runtime-fixture", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    parsed_arguments.emplace_back(path);
    // [actual global program argument][actual global Wasm-file argument] end
    // [safe ] both insertions completed BEFORE borrowing back(). No later
    // vector growth/clear occurs through preparation, guest join or reset.
    // The UTF-8 string above owns this view for the same entire main lifetime.
    // Native WASI initialization may subtract this cursor from THIS array end.
    auto& global_path{parsed_arguments.back()};
    // [actual global parsing-result array][its final live argument]
    // [safe ] this cursor and the initializer's end() belong to one allocation.
    uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(global_path);
    // This positive fixture invokes the same explicit feature policy as the
    // CLI flags below, before parsing/initialization/fused compilation. The
    // original body needs only tail-call; aggregate syntax is opted in only
    // for the separately selected gc-roots fixture. Unrelated gates retain
    // their production defaults, so a real disabled-feature failure survives.
    // [host-owned global binfmt policy] end
    // [safe ] this stable reference is borrowed before any guest starts.
    auto& fixture_features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    // --wasm-feature-enable-tail-call
    fixture_features.disable_tail_call = false;
    fixture_features.explicit_enable_tail_call = true;
    if(gc_roots)
    {
        // --wasm-feature-enable-gc --wasm-feature-enable-function-references
        // --wasm-feature-enable-reference-types
        fixture_features.disable_gc = false;
        fixture_features.explicit_enable_gc = true;
        fixture_features.disable_function_references = false;
        fixture_features.explicit_enable_function_references = true;
        fixture_features.disable_reference_types = false;
        fixture_features.explicit_enable_reference_types = true;
    }
    uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"activation-test";
    check(uwvm2::uwvm::run::prepare_owned_full_cli_source() == static_cast<int>(uwvm2::uwvm::run::retval::ok), "actual CLI source initializer");
    if(gc_roots)
    {
        auto const source{uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
        check(uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) &&
              source->initialized_main_module() != nullptr && source->initialized_main_module()->type_section_storage.requires_gc,
              "actual initialized aggregate cohort required for root witness");
        check(lib::runtime_gc_prepare_cli_collection_host_api() == ::uwvm2::runtime::gc::managed_gc_configure_result::requested &&
              lib::runtime_gc_collection_metrics_host_api().precise_roots_requested,
              "actual cold root IR request before any full publication");
    }
    auto state{::std::make_shared<observer>()};
    check(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::stop, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "actual host-only debug full opt-in");
    check(lib::llvm_jit_prepare_debug_host_api(), "actual fused full publication");
    auto const source{uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
    check(uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source), "actual canonical initialized source");
    auto binding{lib::llvm_jit_debug_bind_source_host_api(source->bound_initialized_main_module_id())};
    // This official plain-WAT fixture has no language DWARF. Native ownership
    // must work independently and must not manufacture a language source map.
    check(!binding, "plain Wasm fixture has no fabricated language source binding");
    check(!lib::llvm_jit_debug_capture_activation_host_api({}), "no observer/ticket cannot mint");
    ::std::uint32_t output{};
    ::std::thread guest{[&]
    {
        ::std::optional<::uwvm2::runtime::gc::scoped_cli_gc_execution> guest_root_launch{};
        if(gc_roots) { guest_root_launch.emplace(); }
        lib::full_compile_run_config config{}; config.entry_function_index = 2u;
        // [host-owned result carrier] end
        // [safe                     ] normal typed raw ABI owns this slot through join.
        //  ^^ no activation ID/native pointer is represented in guest memory.
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
        config.entry_abi_buffers.result_bytes = sizeof(output);
        lib::full_compile_and_run_main_module(u8"activation-test", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    ::std::size_t serial{}, max_depth{}; bool replacement{}, saw_tail{}, saw_generation_two{}, saw_recursion{}, saw_owner_boundary{};
    lib::llvm_jit_debug_activation_capture_owner previous{};
    lib::llvm_jit_debug_activation_snapshot prior{};
    for(;;)
    {
        domain::pause_ticket ticket{};
        {
            ::std::unique_lock lock{state->mutex};
            check(state->changed.wait_until(lock, deadline(), [&] { return state->done || state->serial > serial; }), "bounded actual event wait");
            if(state->done) { break; }
            check(state->serial == serial + 1u && serial < 256u, "bounded event serial");
            serial = state->serial; ticket = state->ticket;
        }
        check(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused, "real complete cooperative stop");
        lib::llvm_jit_debug_activation_capture_owner capture{};
        { ::std::lock_guard lock{state->mutex}; capture = state->capture; }
        check(static_cast<bool>(capture), "qualified true-entry producer minted inside before-park");
        lib::llvm_jit_debug_activation_snapshot now{};
        check(lib::llvm_jit_debug_query_activation_host_api(capture, now) && !now.frames.empty(), "single guarded real identity query");
        check(now.location.function == now.frames.back().function && now.location.code_unit == now.frames.back().module,
            "current domain location matches exact top activation");
        lib::llvm_jit_debug_native_code_bytes code{};
        check(lib::llvm_jit_debug_copy_native_code_host_api(capture, nullptr, code) && code.size != 0u && code.pc != 0u &&
              code.size <= lib::llvm_jit_debug_max_native_code_bytes && code.participant == now.participant &&
              code.module == now.location.code_unit && code.function == now.location.function &&
              code.runtime_epoch == now.location.code_generation && code.function_generation == now.frames.back().function_generation,
              "actual private capture -> one guarded exact owner -> bounded owned native bytes");
        if(code.size < lib::llvm_jit_debug_max_native_code_bytes)
        {
            saw_owner_boundary = true;
            for(::std::size_t byte{code.size}; byte != lib::llvm_jit_debug_max_native_code_bytes; ++byte)
            {
                // [owned code bytes][zero padding ... fixed end] no native borrow.
                // [safe                                        ] scalar index stays <480.
                check(code.bytes[byte] == 0u, "exact owner boundary leaves padding untouched; no neighboring function copied");
            }
        }
        lib::llvm_jit_debug_native_code_bytes bad_code{}; bad_code.size = 99u; bad_code.pc = 1u;
        // [comparison-only unreadable native session identity] not readable
        // [unsafe                                            ] cooperative
        //  ^^ park cannot admit any external-session identity or dereference it.
        check(!lib::llvm_jit_debug_copy_native_code_host_api(capture, reinterpret_cast<void const*>(::std::uintptr_t{1u}), bad_code) &&
              bad_code.size == 0u && bad_code.pc == 0u, "cooperative stop refuses forged native session and clears output");
        if(now.frames.size() > max_depth) { max_depth = now.frames.size(); }
        for(::std::size_t i{1u}; i != now.frames.size(); ++i)
        {
            check(now.frames[i].incarnation != now.frames[i - 1u].incarnation && now.frames[i].parent == now.frames[i - 1u].incarnation,
                "recursive entries distinct and exact parent chain");
            if(now.frames[i].function == now.frames[i - 1u].function) { saw_recursion = true; }
        }
        if(!prior.frames.empty() && prior.frames.size() == now.frames.size())
        {
            auto const& old{prior.frames.back()}; auto const& current{now.frames.back()};
            if(old.function == 1u && current.function == 0u)
            { check(old.incarnation != current.incarnation && old.continuation == current.continuation && old.parent == current.parent,
                "real typed musttail gets new identity and inherited continuation"); saw_tail = true; }
        }
        if(now.frames.back().function == 0u)
        { check(now.frames.back().function_generation == 2u, "entry carries newly compiled replacement generation"); saw_generation_two = true; }
        lib::llvm_jit_debug_source_activation_snapshot joint{};
        check(lib::llvm_jit_debug_query_source_activation_host_api(capture, binding, joint) &&
              joint.activation.participant == now.participant && joint.activation.frames.size() == now.frames.size() &&
              joint.activation.location == now.location && !joint.source_available && joint.source.runtime_epoch == 0u,
              "actual native identity remains valid with language source explicitly unavailable");
        auto marker{::std::make_shared<int>(0)};
        lib::llvm_jit_debug_activation_capture_owner alias{marker, capture.get()};
        lib::llvm_jit_debug_activation_snapshot rejected{}; rejected.participant = 99u;
        check(!lib::llvm_jit_debug_query_activation_host_api(alias, rejected) && rejected.participant == 0u && rejected.frames.empty(),
            "foreign alias control block rejected before dereference");
        check(!lib::llvm_jit_debug_query_source_activation_host_api(alias, binding, joint) && joint.activation.frames.empty() && !joint.source_available,
            "foreign capture owner rejects whole joint output");
        bad_code.size = 99u;
        check(!lib::llvm_jit_debug_copy_native_code_host_api(alias, nullptr, bad_code) && bad_code.size == 0u && bad_code.pc == 0u,
              "native copy rejects foreign capture control block before dereference");
        // [invalid comparison-only public capture/source pointers] never readable
        // [unsafe                                               ] private registry/owner
        //  ^^ must reject them before dereference; no pointer advance or guest access.
        lib::llvm_jit_debug_activation_capture_owner unreadable_capture{marker,
            reinterpret_cast<lib::llvm_jit_debug_activation_capture const*>(::std::uintptr_t{1u})};
        lib::llvm_jit_debug_source_binding_owner unreadable_binding{marker,
            reinterpret_cast<lib::llvm_jit_debug_source_binding const*>(::std::uintptr_t{1u})};
        check(!lib::llvm_jit_debug_query_source_activation_host_api(unreadable_capture, binding, joint) && joint.activation.frames.empty(),
            "unreadable capture address rejected before dereference");
        bad_code.size = 99u;
        check(!lib::llvm_jit_debug_copy_native_code_host_api(unreadable_capture, nullptr, bad_code) && bad_code.size == 0u,
              "native copy rejects unreadable capture alias before dereference");
        check(!lib::llvm_jit_debug_query_source_activation_host_api(capture, unreadable_binding, joint) && joint.activation.frames.empty() && !joint.source_available,
            "unreadable source address rejects whole joint transaction before dereference");
        check(!lib::llvm_jit_debug_capture_activation_host_api(ticket), "manager cannot mint from current numeric/location/ticket state");
        if(previous)
        { check(!lib::llvm_jit_debug_query_activation_host_api(previous, rejected), "old stop capture cannot authenticate later same-site pause"); }
        if(!replacement)
        {
            check(now.frames.size() == 1u && now.frames.back().function == 2u, "inactive helper before first Wasm opcode");
            ::std::array<::std::byte, 4u> body{::std::byte{0u}, ::std::byte{0x41u}, ::std::byte{9u}, ::std::byte{0x0bu}};
            auto prepared{lib::llvm_jit_debug_prepare_function_replacement_host_api(now.location.code_unit, 0u, 1u, body.data(), body.size())};
            check(prepared.status == lib::llvm_jit_debug_replace_status::replaced && prepared.transaction != nullptr, "private typed same ABI replacement");
            lib::llvm_jit_debug_replace_result committed{};
            check(state->control->while_stopped(ticket, [&]
            { committed = lib::llvm_jit_debug_commit_function_replacement_host_api(prepared.transaction); }), "actual stopped publication transaction");
            lib::llvm_jit_debug_discard_function_replacement_host_api(prepared.transaction);
            check(committed.status == lib::llvm_jit_debug_replace_status::replaced && committed.generation == 2u, "replacement generation committed");
            replacement = true;
        }
        prior = now; previous = capture;
        check(state->control->resume(ticket), "actual resume");
        check(!lib::llvm_jit_debug_query_activation_host_api(capture, rejected) && rejected.frames.empty(), "resume immediately retires capture authority");
        check(!lib::llvm_jit_debug_copy_native_code_host_api(capture, nullptr, bad_code) && bad_code.size == 0u && bad_code.pc == 0u,
              "actual resume retires private code-copy authority");
        check(!lib::llvm_jit_debug_query_source_activation_host_api(capture, binding, joint) && joint.activation.frames.empty() && !joint.source_available,
            "resume retires both joint source and activation output");
    }
    guest.join();
    check(output == 18u && serial != 0u && max_depth == 5u && saw_recursion && saw_tail && saw_generation_two && saw_owner_boundary,
        "actual recursion/typed tails/new generation and guest result");
    lib::reset_runtime_state_host_api();
    lib::llvm_jit_debug_activation_snapshot retired{};
    check(!lib::llvm_jit_debug_query_activation_host_api(previous, retired) && retired.frames.empty(), "reset closes old runtime/control epoch");
    lib::llvm_jit_debug_native_code_bytes reset_code{}; reset_code.size = 99u;
    check(!lib::llvm_jit_debug_copy_native_code_host_api(previous, nullptr, reset_code) && reset_code.size == 0u,
          "actual reset rejects old executable owner and clears output");
    ::fast_io::io::println("debug_native_code_runtime: PASS actual entries, recursive identities, musttail continuation, generation two, alias, stale ticket, reset");
}
