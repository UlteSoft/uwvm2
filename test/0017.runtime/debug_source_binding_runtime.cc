// Actual native-full publication/stop boundary witness. No DWARF library,
// debugger component, fake publication epoch or caller-created source pin.
#include <uwvm2/uwvm/run/owned_source.h>
#include <fast_io.h>
#include <uwvm2/utils/container/string_concat.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <limits>
#include <mutex>
#include <thread>
#include <tuple>
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace threads = uwvm2::utils::thread;
namespace source = uwvm2::uwvm::runtime::full;
using domain = threads::cooperative_pause_domain;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_binding_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static ::std::uint_least8_t expected_memory_address_bytes{}; // optional actual single-memory witness
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct event
{
    domain::pause_ticket ticket{}; threads::cooperative_pause_location location{};
    ::std::uint_least64_t participant{};
    uwvm2::runtime::exception::diagnostic_trace_ref trace{};
    lib::llvm_jit_debug_activation_capture_owner activation{};
};
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    event current{}; ::std::size_t serial{}; bool done{};
    static void stop(void* opaque, ::std::uint_least64_t participant, threads::cooperative_pause_location location) noexcept
    {
        // [runtime-retained actual native observer owner] observer_end
        // [safe                                       ] opaque is this host-only
        //  ^^ synchronous callback owner, never a guest pointer or DAP ID.
        auto& self{*static_cast<observer*>(opaque)};
        auto ticket{self.control->request_pause()}; check(static_cast<bool>(ticket), "actual observer pause");
        ::std::lock_guard lock{self.mutex};
        self.current = {::std::move(ticket), location, participant, {}};
        ++self.serial; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t participant, threads::cooperative_pause_location location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        // [same actual native owner] no pointer advance; this callback copies
        // only an owning trace. Locals/values are deliberately ignored.
        auto& self{*static_cast<observer*>(opaque)};
        auto trace{lib::llvm_jit_capture_debug_stack_host_api()};
        ::std::lock_guard lock{self.mutex};
        check(self.current.participant == participant && self.current.location == location, "cold capture belongs to actual stop");
        self.current.trace = ::std::move(trace);
        self.current.activation = lib::llvm_jit_debug_capture_activation_host_api(self.current.ticket);
        check(static_cast<bool>(self.current.activation), "actual before-park private activation capture");
    }
};
static auto prepare(uwvm2::utils::cmdline::parameter_parsing_results& path)
{
    // [actual global parsing-result array][main-owned stable Wasm-file element]
    // [safe ] preparation/replacement/reset never grows this fixture's argv.
    check(!::uwvm2::uwvm::cmdline::parsing_result.empty() &&
        ::std::addressof(path) == ::std::addressof(::uwvm2::uwvm::cmdline::parsing_result.back()),
        "prepare receives actual global parsed argument");
    uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(path);
    uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"binding-test";
    check(uwvm2::uwvm::run::prepare_owned_full_cli_source() == static_cast<int>(uwvm2::uwvm::run::retval::ok), "actual owned CLI parse/initializer");
    auto state{::std::make_shared<observer>()};
    check(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::stop, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "real host observer configuration");
    check(lib::llvm_jit_prepare_debug_host_api(), "actual fused full validation and complete native publication");
    auto owner{source::selected_full_source_owner_pin()};
    check(source::full_source_instance::has_canonical_owner(owner), "actual canonical source");
    auto const module{owner->bound_initialized_main_module_id()};
    auto binding{lib::llvm_jit_debug_bind_source_host_api(module)};
    check(static_cast<bool>(binding), "factory from complete native publisher");
    lib::llvm_jit_debug_source_image image{};
    check(lib::llvm_jit_debug_copy_source_image_host_api(binding, image) && image.functions.size() == 2u, "source copy from actual owner");
    check(owner->actual_full_validation_epoch() != 0u, "actual native publisher recorded validation");
    return ::std::tuple{state, binding, module};
}
static void run(::std::shared_ptr<observer> const& state, lib::llvm_jit_debug_source_binding_owner const& binding,
    ::std::size_t module, bool replace, lib::llvm_jit_debug_source_binding_owner const& old = {})
{
    ::std::uint32_t output{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config config{}; config.entry_function_index = 1u;
        // [live host result slot] result_end
        // [safe                ] passed only to the normal bounded host ABI.
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
        config.entry_abi_buffers.result_bytes = sizeof(output);
        lib::full_compile_and_run_main_module(u8"binding-test", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    bool replaced{}, saw_replacement{}; ::std::size_t serial{};
    for(;;)
    {
        event current{};
        {
            ::std::unique_lock lock{state->mutex};
            check(state->changed.wait_until(lock, deadline(), [&] { return state->done || state->serial > serial; }), "observer event wait");
            if(state->done) { break; }
            check(state->serial == serial + 1u && serial < 64u, "bounded actual stop sequence"); serial = state->serial;
            current = state->current;
        }
        check(state->control->wait_until_paused(current.ticket, deadline()) == threads::cooperative_pause_result::paused, "actual complete pause");
        { ::std::lock_guard lock{state->mutex}; current = state->current; }
        lib::llvm_jit_debug_source_position position{};
        bool const available{lib::llvm_jit_debug_source_position_host_api(binding, state->control, current.ticket, current.participant, position)};
        if(replaced && current.location.function == 0u)
        { check(!available && position.runtime_epoch == 0u, "old function source unavailable after replacement"); saw_replacement = true; }
        else { check(available && position.module == module && position.function == current.location.function &&
            position.runtime_epoch == current.location.code_generation && position.function_generation == 1u, "actual original source position"); }
        lib::llvm_jit_debug_source_object_copy object{};
        bool const object_available{lib::llvm_jit_debug_copy_source_object_host_api(current.activation, binding,
            0u, 4u, expected_memory_address_bytes == 0u ? 4u : expected_memory_address_bytes, object)};
        if(available && expected_memory_address_bytes != 0u)
        {
            check(object_available && object.bytes.size() == 4u && object.position.source_available &&
                object.position.activation.participant == current.participant && object.position.source.module == module &&
                object.position.source.function == current.location.function && object.position.source.runtime_epoch == current.location.code_generation &&
                object.bytes[0u] == ::std::byte{0x11} && object.bytes[1u] == ::std::byte{0x22} &&
                object.bytes[2u] == ::std::byte{0x33} && object.bytes[3u] == ::std::byte{0x44}, "actual stopped single-memory owned copy");
        }
        else { check(!object_available && object.bytes.empty() && !object.position.source_available, "missing/retired memory source fails closed"); }
        check(!lib::llvm_jit_debug_copy_source_object_host_api(current.activation, binding,
            (::std::numeric_limits<::std::uint_least64_t>::max)(), 4u, 4u, object) && object.bytes.empty(), "offset overflow/range rejected and output cleared");
        check(!lib::llvm_jit_debug_copy_source_object_host_api(current.activation, binding, 0u, 65537u, 4u, object), "copy budget rejected");
        check(!lib::llvm_jit_debug_copy_source_object_host_api(current.activation, binding, 0u, 4u, 3u, object), "invalid source address width rejected");
        if(expected_memory_address_bytes != 0u)
        { check(!lib::llvm_jit_debug_copy_source_object_host_api(current.activation, binding, 0u, 4u,
                expected_memory_address_bytes == 4u ? 8u : 4u, object), "actual memory32/64 width mismatch rejected"); }
        // Same native object address with a foreign shared_ptr control block is
        // not the runtime-minted binding. No source/stop/read authority follows.
        auto marker{::std::make_shared<int>(0)};
        lib::llvm_jit_debug_source_binding_owner alias{marker, binding.get()};
        check(!lib::llvm_jit_debug_copy_source_object_host_api(current.activation, alias, 0u, 4u, 4u, object), "foreign source binding cannot authorize memory copy");
        lib::llvm_jit_debug_activation_capture_owner activation_alias{marker, current.activation.get()};
        check(!lib::llvm_jit_debug_copy_source_object_host_api(activation_alias, binding, 0u, 4u, 4u, object), "foreign activation control block rejected before dereference");
        // [deliberately unreadable alias address] never a native read capability.
        lib::llvm_jit_debug_activation_capture_owner unreadable_activation{marker,
            reinterpret_cast<lib::llvm_jit_debug_activation_capture const*>(::std::uintptr_t{1u})};
        check(!lib::llvm_jit_debug_copy_source_object_host_api(unreadable_activation, binding, 0u, 4u, 4u, object), "unreadable activation rejected before dereference");
        check(!lib::llvm_jit_debug_source_position_host_api(alias, state->control, current.ticket, current.participant, position), "foreign binding owner rejected");
        lib::llvm_jit_debug_source_image copied{}; copied.code_section_content_size = 99u;
        check(!lib::llvm_jit_debug_copy_source_image_host_api(alias, copied) && copied.code_section_content_size == 0u, "foreign image copy rejected and cleared");
        // [deliberately invalid comparison-only address] never readable
        // [unsafe                                     ] must be rejected before
        //  ^^ any dereference, independently of its shared_ptr control block.
        lib::llvm_jit_debug_source_binding_owner unreadable{marker,
            reinterpret_cast<lib::llvm_jit_debug_source_binding const*>(::std::uintptr_t{1u})};
        check(!lib::llvm_jit_debug_source_position_host_api(unreadable, state->control, current.ticket, current.participant, position),
            "unreadable foreign source address rejected before dereference");
        check(!lib::llvm_jit_debug_copy_source_image_host_api(unreadable, copied) && copied.functions.empty(),
            "unreadable foreign image address rejected before dereference");
        // [deliberately invalid comparison-only pause-domain address]
        // [unsafe                                                ] no method call is permitted.
        ::std::shared_ptr<domain> unreadable_control{marker, reinterpret_cast<domain*>(::std::uintptr_t{1u})};
        check(!lib::llvm_jit_debug_source_position_host_api(binding, unreadable_control, current.ticket, current.participant, position),
            "unreadable foreign domain address rejected before method call");
        ::std::shared_ptr<domain> control_alias{marker, state->control.get()};
        check(!lib::llvm_jit_debug_source_position_host_api(binding, control_alias, current.ticket, current.participant, position), "foreign control owner rejected");
        check(!lib::llvm_jit_debug_source_position_host_api(binding, state->control, {}, current.participant, position), "empty/stale ticket rejected");
        check(!lib::llvm_jit_debug_source_position_host_api(binding, state->control, current.ticket, current.participant + 1u, position), "unknown participant rejected");
        if(old) { check(!lib::llvm_jit_debug_source_position_host_api(old, state->control, current.ticket, current.participant, position), "old source/runtime epoch cannot query fresh code"); }
        if(replace && !replaced)
        {
            check(current.location.function == 1u && current.trace && !current.trace->truncated(), "actual inactive replacement target trace");
            for(auto const& frame : current.trace->frames())
            { check(frame.module_id != module || frame.function_index != 0u, "target absent from every real parked activation"); }
            ::std::array<::std::byte, 4u> body{::std::byte{0u}, ::std::byte{0x41u}, ::std::byte{9u}, ::std::byte{0x0bu}};
            auto prepared{lib::llvm_jit_debug_prepare_function_replacement_host_api(module, 0u, 1u, body.data(), body.size())};
            check(prepared.status == lib::llvm_jit_debug_replace_status::replaced && prepared.transaction != nullptr, "private same ABI fused replacement");
            lib::llvm_jit_debug_replace_result committed{};
            check(state->control->while_stopped(current.ticket, [&]
            { committed = lib::llvm_jit_debug_commit_function_replacement_host_api(prepared.transaction); }), "real stopped publication guard");
            lib::llvm_jit_debug_discard_function_replacement_host_api(prepared.transaction);
            check(committed.status == lib::llvm_jit_debug_replace_status::replaced && committed.generation == 2u, "actual generation two publication");
            replaced = true;
        }
        check(state->control->resume(current.ticket), "real resume");
        check(!lib::llvm_jit_debug_copy_source_object_host_api(current.activation, binding, 0u, 4u, 4u, object) && object.bytes.empty(), "resumed capture cannot authorize memory copy");
        check(!lib::llvm_jit_debug_source_position_host_api(binding, state->control, current.ticket, current.participant, position), "old ticket unusable immediately after resume");
    }
    guest.join(); check(serial != 0u && output == (replace ? 9u : 7u), "actual guest result");
    check(!replace || saw_replacement, "actual replaced function stop observed");
}
int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 2; }
    if(argc == 4)
    {
        auto const memory{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
        if(memory == "memory32") { expected_memory_address_bytes = 4u; }
        else if(memory == "memory64") { expected_memory_address_bytes = 8u; }
        else { return 2; }
    }
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
    check(!lib::llvm_jit_debug_bind_source_host_api(0u), "uninitialized publication unavailable");
    auto [state, binding, module]{prepare(global_path)};
    run(state, binding, module, true);
    lib::reset_runtime_state_host_api();
    lib::llvm_jit_debug_source_image image{};
    check(!lib::llvm_jit_debug_copy_source_image_host_api(binding, image) && image.functions.empty(), "retired binding image unavailable after drain");
    auto [next, fresh, next_module]{prepare(global_path)};
    check(binding.get() != fresh.get() && (binding.owner_before(fresh) || fresh.owner_before(binding)), "fresh source/control publication identity");
    run(next, fresh, next_module, false, binding);
    lib::reset_runtime_state_host_api();
    check(!lib::llvm_jit_debug_copy_source_image_host_api(fresh, image), "fresh reset retires metadata query authority");
    ::fast_io::io::println("debug_source_binding_runtime: PASS original position, alias owners, actual replacement, stale ticket, reset and fresh source");
}
