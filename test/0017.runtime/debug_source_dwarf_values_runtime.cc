// Actual native-full numeric-slot generation/stop witness. The scope/type below
// is SYNTHETIC finite-query metadata, not proof of compiler source locations.
// Product official-DWARF source-value qualification is a separate CLI runner.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/debugger/source_dwarf_values.h>
#include <fast_io.h>
#include <uwvm2/utils/container/string_concat.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace threads = uwvm2::utils::thread;
namespace source = uwvm2::uwvm::runtime::full;
using domain = threads::cooperative_pause_domain;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_values_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct event
{
    domain::pause_ticket ticket{}; threads::cooperative_pause_location location{};
    ::std::uint_least64_t participant{};
    uwvm2::runtime::exception::diagnostic_trace_ref trace{};
    ::std::array<uwvm2::uwvm::debugger::source_dwarf::copied_numeric_local, lib::llvm_jit_debug_max_captured_locals> locals{};
    ::std::size_t captured{}, total{}; ::std::uint_least64_t function_generation{};
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
        lib::llvm_jit_debug_local_view locals) noexcept
    {
        // [same actual native owner] no pointer advance; all borrows below stay
        // within the live compiler snapshot and runtime-owned type array until
        // this synchronous callback returns; only copied scalar slots survive.
        auto& self{*static_cast<observer*>(opaque)};
        auto trace{lib::llvm_jit_capture_debug_stack_host_api()};
        ::std::lock_guard lock{self.mutex};
        check(self.current.participant == participant && self.current.location == location, "cold capture belongs to actual stop");
        self.current.trace = ::std::move(trace);
        check(locals.function_generation != 0u && locals.captured_count <= self.current.locals.size() &&
              locals.captured_count <= locals.total_count && locals.types != nullptr &&
              (locals.captured_count == 0u || (locals.values != nullptr && locals.availability != nullptr)), "actual producer generation, availability and slot bounds");
        // [real compiler flags ... captured_count <= owned slot capacity] end
        // [safe                                                       ] all counts and
        //  ^^ required borrows are checked before indexing; validate EVERY 0/1
        //     marker before any payload read, matching controller capture.
        for(::std::size_t i{}; i != locals.captured_count; ++i)
        { check(locals.availability[i] <= 1u, "actual producer availability is an exact 0/1 flag"); }
        for(::std::size_t i{}; i != locals.captured_count; ++i)
        {
            // [live declaration types][owned copied slots ... i ... capacity] end
            // [safe                                                         ] i < checked count.
            //  ^^ preserve each ORIGINAL index/type; false is never compacted.
            self.current.locals[i] = {};
            self.current.locals[i].wasm_type = locals.types[i];
            self.current.locals[i].available = locals.availability[i] != 0u;
            if(!self.current.locals[i].available) { continue; } // No payload borrow/read for flag0.
            for(::std::size_t byte{}; byte != self.current.locals[i].bytes.size(); ++byte)
            {
                // [real generated captured_count * 16 bytes ... i*16+byte] end
                // [safe                                                  ] i < count <= 256;
                //  ^^ byte < owned 16-byte slot; multiplication is bounded.
                //     Only a PROVEN readable live slot forms this payload index.
                self.current.locals[i].bytes[byte] = locals.values[i * self.current.locals[i].bytes.size() + byte];
            }
        }
        self.current.captured = locals.captured_count; self.current.total = locals.total_count;
        self.current.function_generation = locals.function_generation;
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
    ::std::size_t module, bool replace, lib::llvm_jit_debug_source_binding_owner const& old = {}, bool nondefaultable = false)
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
    bool replaced{}, saw_replacement{}; ::std::size_t serial{}; event previous{};
    bool saw_unavailable_ref{}, saw_available_ref{}, saw_numeric_default{}, saw_numeric_write{}, have_ref_type{};
    ::std::uint_least8_t actual_ref_type{};
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
        if(previous.ticket)
        {
            check(!lib::llvm_jit_debug_source_position_host_api(binding, state->control, previous.ticket, previous.participant, position),
                  "previous captured ticket stays invalid after a fresh stop, even if participant/PC happens to match");
        }
        bool const available{lib::llvm_jit_debug_source_position_host_api(binding, state->control, current.ticket, current.participant, position)};
        if(replaced && current.location.function == 0u)
        { check(!available && position.runtime_epoch == 0u, "old function source unavailable after replacement"); saw_replacement = true; }
        else { check(available && position.module == module && position.function == current.location.function &&
            position.runtime_epoch == current.location.code_generation && position.function_generation == current.function_generation &&
            current.function_generation == 1u, "actual original source position agrees with copied carrier generation"); }
        if(current.location.function == 0u)
        {
            check(current.captured >= 1u && current.total == (replaced ? 1u : (nondefaultable ? 4u : 2u)) &&
                  current.function_generation == (replaced ? 2u : 1u), "actual parameter/local layout follows actual published generation");
            check(current.locals[0u].available, "actual initialized parameter carries its producer availability");
            ::std::uint32_t parameter{};
            // [owned complete 16-byte copied parameter] end
            // [safe                                  ] decode complete actual Wasm i32 native carrier, including BE.
            ::std::memcpy(::std::addressof(parameter), current.locals[0].bytes.data(), sizeof(parameter));
            check(current.locals[0].wasm_type == 0x7fu && parameter == 5u, "parameter comes from actual local snapshot, no stack-top guess");
            ::std::uint32_t later_numeric{};
            if(nondefaultable)
            {
                check(!replaced && current.captured >= 4u && current.total == 4u, "actual Core3 helper keeps all original local slots");
                // [owned copied parameter0, nondefaultable1, numeric2, nullable3] end
                // [safe                                                          ] captured >= 4.
                //  ^^ inspect flags/types before reading any available carrier;
                //     ref type is the genuine retained declaration, never guessed.
                if(!have_ref_type)
                {
                    actual_ref_type = current.locals[1u].wasm_type; have_ref_type = true;
                    check(!current.locals[1u].available, "actual first helper stop precedes nondefaultable local.set");
                }
                check(current.locals[1u].wasm_type == actual_ref_type, "actual nondefaultable declaration type stays at original index1");
                if(!current.locals[1u].available)
                {
                    saw_unavailable_ref = true;
                    check(::std::all_of(current.locals[1u].bytes.begin(), current.locals[1u].bytes.end(),
                        [](auto byte) { return byte == ::std::byte{}; }), "false slot retains no copied payload");
                }
                else { saw_available_ref = true; }
                check(current.locals[2u].available && current.locals[2u].wasm_type == 0x7fu && current.locals[3u].available,
                      "defaultable numeric2/nullable3 flags remain available without compacting index1");
                // [owned full native i32 carrier at original local2] end
                // [safe                                           ] complete flag/type/count
                //  ^^ checks precede addressof/data; ref payload is never inspected.
                ::std::memcpy(::std::addressof(later_numeric), current.locals[2u].bytes.data(), sizeof(later_numeric));
                check(later_numeric == 0u || later_numeric == 7u, "later original numeric slot carries only its actual initial/written value");
                saw_numeric_default |= later_numeric == 0u; saw_numeric_write |= later_numeric == 7u;
            }
            if(available)
            {
                lib::llvm_jit_debug_source_image image{};
                check(lib::llvm_jit_debug_copy_source_image_host_api(binding, image), "actual private source-image copy");
                auto const function{::std::find_if(image.functions.begin(), image.functions.end(), [](auto const& value) { return value.function == 0u; })};
                check(function != image.functions.end() && current.location.offset < function->expression_size &&
                      position.code_offset == function->expression_begin + current.location.offset, "full captured expression offset binding");
                namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
                ::std::array<dwarf::scope_record, 2u> scopes{}; scopes[0].kind = dwarf::scope_kind::compile_unit;
                scopes[1].parent = 0u; scopes[1].kind = dwarf::scope_kind::subprogram; scopes[1].concrete = true;
                scopes[1].own_ranges_declared = true; scopes[1].ranges = {{function->expression_begin, function->expression_begin + function->expression_size}};
                ::std::array<dwarf::type_record, 1u> types{{{{}, "synthetic i32", dwarf::type_kind::scalar, 0x05u, 4u}}};
                ::std::vector<dwarf::variable_record> variables(nondefaultable ? 3u : 1u); variables[0].scope = 1u; variables[0].type = 0u;
                variables[0].name = ::fast_io::concat_std(::std::string_view{"synthetic parameter"}); variables[0].parameter = true;
                dwarf::location_plan plan{}; plan.kind = dwarf::plan_kind::wasm_local_value; plan.local_index = 0u;
                variables[0].locations.push_back({{}, plan});
                if(nondefaultable)
                {
                    // Deliberately synthetic i32 metadata for the real ref slot:
                    // flag0 must win before carrier mismatch, and a readable ref
                    // must STILL never be interpreted as an integer/address.
                    variables[1u] = variables[0u]; variables[1u].parameter = false;
                    variables[1u].name = ::fast_io::concat_std(::std::string_view{"synthetic unavailable ref"});
                    variables[1u].locations[0u].plan.local_index = 1u;
                    variables[2u] = variables[0u]; variables[2u].parameter = false;
                    variables[2u].name = ::fast_io::concat_std(::std::string_view{"synthetic later numeric"});
                    variables[2u].locations[0u].plan.local_index = 2u;
                }
                ::std::vector<dwarf::numeric_variable> values{};
                check(dwarf::query_numeric_variables(scopes, types, variables, position.code_offset,
                    {current.locals.data(), current.captured}, current.total, values) == dwarf::inline_query_error::none &&
                    values.size() == variables.size() && values[0u].reason == dwarf::numeric_unavailable_reason::none &&
                    values[0].bits == 5u, "finite numeric query only after actual source/stop/generation authentication");
                if(nondefaultable)
                {
                    check(values[1u].kind == dwarf::numeric_kind::unavailable &&
                        values[1u].reason == (current.locals[1u].available ? dwarf::numeric_unavailable_reason::carrier_mismatch :
                            dwarf::numeric_unavailable_reason::local_unavailable), "actual unreadable/ref slot stays explicitly unavailable to numeric display");
                    check(values[2u].reason == dwarf::numeric_unavailable_reason::none && values[2u].bits == later_numeric,
                          "finite query retains the genuine later original numeric index/value");
                }
            }
        }
        // Same native object address with a foreign shared_ptr control block is
        // not the runtime-minted binding. No source/stop/read authority follows.
        auto marker{::std::make_shared<int>(0)};
        lib::llvm_jit_debug_source_binding_owner alias{marker, binding.get()};
        check(!lib::llvm_jit_debug_source_position_host_api(alias, state->control, current.ticket, current.participant, position), "foreign binding owner rejected");
        lib::llvm_jit_debug_source_image copied{}; copied.code_section_content_size = 99u;
        check(!lib::llvm_jit_debug_copy_source_image_host_api(alias, copied) && copied.code_section_content_size == 0u, "foreign image copy rejected and cleared");
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
        check(!lib::llvm_jit_debug_source_position_host_api(binding, state->control, current.ticket, current.participant, position), "old ticket unusable immediately after resume");
        previous = current; // retain copied scalar bytes, never permission to query the new pause.
    }
    guest.join(); check(serial != 0u && output == (replace ? 9u : 7u), "actual guest result");
    check(!replace || saw_replacement, "actual replaced function stop observed");
    check(!nondefaultable || (saw_unavailable_ref && saw_available_ref && saw_numeric_default && saw_numeric_write),
          "actual Core3 first-set readability flags and later original numeric initialization observed");
}
int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 2; }
    bool nondefaultable{};
    if(argc == 4)
    {
        auto const scenario{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
        if(scenario != "availability") { return 2; }
        nondefaultable = true;
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
    run(state, binding, module, !nondefaultable, {}, nondefaultable);
    lib::reset_runtime_state_host_api();
    lib::llvm_jit_debug_source_image image{};
    check(!lib::llvm_jit_debug_copy_source_image_host_api(binding, image) && image.functions.empty(), "retired binding image unavailable after drain");
    auto [next, fresh, next_module]{prepare(global_path)};
    check(binding.get() != fresh.get() && (binding.owner_before(fresh) || fresh.owner_before(binding)), "fresh source/control publication identity");
    run(next, fresh, next_module, false, binding, nondefaultable);
    lib::reset_runtime_state_host_api();
    check(!lib::llvm_jit_debug_copy_source_image_host_api(fresh, image), "fresh reset retires metadata query authority");
    ::fast_io::io::println("debug_source_dwarf_values_runtime: PASS actual numeric slots and generations; real source/stop/owner/replacement/reset boundary",
        nondefaultable ? ::std::string_view{"; actual Core3 nondefaultable readability flags and original indices"} : ::std::string_view{});
}
