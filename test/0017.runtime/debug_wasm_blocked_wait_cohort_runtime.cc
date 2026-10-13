// Two actual full-JIT entries block in nested Wasm frames on the same wait key.
// Query genuine paused owners, then resume and notify through real guest code.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <uwvm2/uwvm/debugger/wasm_mutation.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace th = ::uwvm2::utils::thread;
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
namespace wm = ::uwvm2::uwvm::debugger::wasm_mutation;
using domain = th::cooperative_pause_domain;
static void require(bool value, unsigned line)
{
    if(!value)
    {
        ::fast_io::io::perrln("blocked wait cohort FAIL line=", line);
        ::fast_io::fast_terminate();
    }
}
#define REQUIRE(x) require(bool(x), __LINE__)
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{10}; }
static ::std::uint64_t numeric(ws::value const& value)
{
    REQUIRE(value.available && (value.type.kind == ws::value_kind::i32 || value.type.kind == ws::value_kind::i64));
    auto const* begin{reinterpret_cast<unsigned char const*>(value.bits.data())};
    ::std::uint64_t bits{};
    // The query owns a complete16-byte numeric slot. Only its bounded low8B
    // are parsed here; no pointer into guest or native wait storage is read.
    auto const parsed{::fast_io::parse_by_scan(begin, begin + 8u, ::fast_io::mnp::le_get<64u>(bits))};
    REQUIRE(parsed.code == ::fast_io::parse_code::ok && parsed.iter == begin + 8u);
    return bits;
}
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(3u)};
    ::std::mutex mutex{};
    ::std::condition_variable changed{};
    ::std::array<::std::uint64_t, 2u> participants{};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> captures{};
    ::std::array<th::cooperative_pause_location, 2u> locations{};
    domain::pause_ticket ticket{};
    unsigned reached{}, parks{};
    ::std::uint64_t points{};
    static void point(void* pointer, ::std::uint_least64_t who, th::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex}; ++self.points;
        // Both fixtures have the same genuine byte23 wait site. Instructions
        // before it initialize the GC local and the dynamic i64 stack prefix.
        if(where.function != 1u || where.offset != 23u) { return; }
        REQUIRE(self.reached < 2u && who != 0u);
        if(self.reached != 0u) { REQUIRE(self.participants[0u] != who); }
        self.participants[self.reached++] = who; self.changed.notify_all();
    }
    static void before_park(void* pointer, ::std::uint_least64_t who,
        th::cooperative_pause_location where, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex};
        REQUIRE(where.function == 1u && where.offset == 23u && self.ticket);
        unsigned index{};
        for(; index != 2u && self.participants[index] != who; ++index) {}
        REQUIRE(index < 2u && !self.captures[index]);
        REQUIRE(!lib::llvm_jit_capture_debug_native_code_site_host_api().valid &&
            !lib::llvm_jit_capture_debug_native_step_site_host_api().valid);
        auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        REQUIRE(actual.status == lib::llvm_jit_checkpoint_capture_status::captured && actual.capture);
        self.captures[index] = ::std::move(actual.capture); self.locations[index] = where;
        ++self.parks; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 6 && argc != 7) { return 64; }
    bool const shutdown{argc == 7};
    bool immediate_shutdown{};
    if(shutdown)
    {
        auto const kind{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(argv[6]))};
        REQUIRE(kind == "shutdown" || kind == "shutdown-immediate");
        immediate_shutdown = kind == "shutdown-immediate";
    }
    auto const strategy{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(argv[2]))};
    auto const purpose{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(argv[3]))};
    REQUIRE(strategy == "instruction" || strategy == "unwind");
    REQUIRE(purpose == "observe" || purpose == "resumable");
    unsigned address_bits{}, compare_bits{};
    for(auto const index : {4u, 5u})
    {
        auto const text{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(argv[index]))};
        auto& bits{index == 4u ? address_bits : compare_bits};
        auto const end{text.data() + text.size()};
        auto const parsed{::fast_io::parse_by_scan(text.data(), end, ::fast_io::mnp::dec_get<true, true>(bits))};
        REQUIRE(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end && (bits == 32u || bits == 64u));
    }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = strategy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_threads = false; features.explicit_enable_threads = true;
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_memory64 = false; features.explicit_enable_memory64 = true;
    auto const path{::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& args{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; args.clear();
    args.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"blocked-cohort", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    args.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(args.back());
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"blocked-cohort";
    REQUIRE(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok));
    auto self{::std::make_shared<observer>()};
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(self->control, {self, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok);
    if(purpose == "resumable")
    {
        auto profile{::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager()}; REQUIRE(profile);
        REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok);
    }
    else { REQUIRE(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok); }
    REQUIRE(lib::llvm_jit_prepare_debug_host_api());
    // Owned CLI preparation deliberately defers active segments until full
    // validation succeeds. This host fixture must perform the same real
    // instantiation step as CLI startup BEFORE admitting either guest.
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"blocked-cohort");
    ::std::barrier start{3};
    ::std::array<::std::uint64_t, 2u> tags{41u, 42u};
    ::std::array<::std::uint32_t, 2u> results{99u, 99u};
    ::std::atomic_uint completed{};
    ::std::array<::std::thread, 2u> guests{};
    for(unsigned i{}; i != 2u; ++i)
    {
        guests[i] = ::std::thread{[&, i]
        {
            start.arrive_and_wait();
            lib::full_compile_run_config config{}; config.entry_function_index = 0u;
            // Main-owned independent buffers remain live until both joins.
            config.entry_abi_buffers.param_buffer = reinterpret_cast<::std::byte*>(::std::addressof(tags[i]));
            config.entry_abi_buffers.param_bytes = sizeof(tags[i]);
            config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(results[i]));
            config.entry_abi_buffers.result_bytes = sizeof(results[i]);
            lib::full_compile_and_run_main_module(u8"blocked-cohort", config); ++completed;
        }};
    }
    start.arrive_and_wait();
    {
        ::std::unique_lock lock{self->mutex};
        REQUIRE(self->changed.wait_until(lock, deadline(), [&] { return self->reached == 2u; }));
    }
    // The callbacks prove entry to both actual wait instructions. A later
    // unchanged callback count independently proves in-flight suspension.
    ::std::this_thread::sleep_for(::std::chrono::milliseconds{50});
    domain::pause_ticket old_ticket{};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> old_captures{};
    ::std::uint64_t stopped_points{};
    for(unsigned episode{}; episode != 2u; ++episode)
    {
        domain::pause_ticket ticket{};
        {
            ::std::lock_guard lock{self->mutex}; self->captures = {}; self->parks = 0u;
            ticket = self->control->request_pause(); self->ticket = ticket;
        }
        REQUIRE(ticket && self->control->wait_until_paused(ticket, deadline()) == th::cooperative_pause_result::paused);
        auto const census{self->control->capture(ticket)};
        if(census.result != th::cooperative_pause_result::paused || census.participants.size() != 2u)
        {
            ::std::uint64_t actual_memory{};
            bool const read{lib::llvm_jit_debug_read_memory_host_api(0u, 0u, 16u, ::std::addressof(actual_memory), sizeof(actual_memory))};
            ::fast_io::io::perrln("actual wait census=", census.participants.size(), " completed=", completed.load(),
                " result0=", results[0u], " result1=", results[1u], " memory_read=", read, " memory=", actual_memory);
        }
        REQUIRE(census.result == th::cooperative_pause_result::paused && census.participants.size() == 2u);
        ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> captures{};
        {
            ::std::lock_guard lock{self->mutex}; REQUIRE(self->parks == 2u); captures = self->captures;
            if(episode == 0u) { stopped_points = self->points; }
            else { REQUIRE(stopped_points == self->points); }
            for(unsigned i{}; i != 2u; ++i)
            {
                bool matched{};
                for(auto const& actual : census.participants)
                { matched |= actual.id == self->participants[i] && actual.location == self->locations[i]; }
                REQUIRE(matched);
            }
        }
        REQUIRE(completed.load() == 0u);
        unsigned observed_tags{};
        for(auto const participant : self->participants)
        {
            ws::request query{}; query.participant = participant; query.count = 8u; query.selected = ws::selection::locals;
            auto const locals{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, captures, query)};
            REQUIRE(locals.result == ws::status::available && ws::valid(locals) && locals.total_values == 3u && locals.rows.size() == 3u);
            auto const tag{numeric(locals.rows[0u].data)}; REQUIRE(tag == 41u || tag == 42u);
            observed_tags |= 1u << static_cast<unsigned>(tag - 41u);
            REQUIRE(locals.rows[1u].data.available && locals.rows[1u].data.ref.kind == ws::reference_kind::structure);
            auto const id{locals.rows[1u].data.ref.object}; REQUIRE(id != 0u && id <= locals.objects.size());
            auto const& object{locals.objects[id - 1u]};
            REQUIRE(object.kind == ws::object_kind::structure && object.members.size() == 1u && numeric(object.members[0u].data) == tag + 100u);
            REQUIRE(numeric(locals.rows[2u].data) == 0u);
            query.selected = ws::selection::operands;
            auto const operands{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, captures, query)};
            REQUIRE(operands.result == ws::status::available && ws::valid(operands) && operands.total_values == 4u && operands.rows.size() == 4u);
            REQUIRE(operands.rows[0u].data.type.kind == ws::value_kind::i64 && numeric(operands.rows[0u].data) == tag + 1000u);
            REQUIRE(operands.rows[1u].data.type.kind == (address_bits == 64u ? ws::value_kind::i64 : ws::value_kind::i32) && numeric(operands.rows[1u].data) == 16u);
            REQUIRE(operands.rows[2u].data.type.kind == (compare_bits == 64u ? ws::value_kind::i64 : ws::value_kind::i32) && numeric(operands.rows[2u].data) == 7u);
            REQUIRE(operands.rows[3u].data.type.kind == ws::value_kind::i64 && numeric(operands.rows[3u].data) == UINT64_MAX);
            // The wrapper remains a genuine independently queryable frame.
            query.selected = ws::selection::locals; query.frame = 1u;
            auto const caller{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, captures, query)};
            REQUIRE(caller.result == ws::status::available && ws::valid(caller) && caller.rows.size() == 1u && numeric(caller.rows[0u].data) == tag);
            query.frame = 0u;
            auto const incomplete{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, {captures.data(), 1u}, query)};
            REQUIRE(incomplete.result == ws::status::incomplete_cohort && incomplete.rows.empty() && incomplete.objects.empty());
            if(episode != 0u)
            { REQUIRE(lib::llvm_jit_debug_query_wasm_state_host_api(old_ticket, old_captures, query).result != ws::status::available); }
        }
        REQUIRE(observed_tags == 3u);
        wm::request mutation{}; mutation.participant = self->participants[0u]; mutation.target = wm::destination::memory;
        mutation.source = wm::source_kind::bytes; mutation.element = 16u; mutation.memory_size = 1u; mutation.memory_bytes[0u] = ::std::byte{9u};
        REQUIRE(wm::valid(mutation));
        REQUIRE(lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket, captures, mutation).applied);
        ::std::array<::std::byte, 16u> label{}; label[0u] = ::std::byte{1u};
        REQUIRE(!lib::llvm_jit_checkpoint_capture_instance_host_api(ticket, captures, label, {}).graph);
        REQUIRE(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(ticket, captures) != lib::llvm_jit_checkpoint_execution_retirement_status::execution_retired);
        old_ticket = ticket; old_captures = captures;
        REQUIRE(self->control->resume(ticket));
        if(episode == 0u) { ::std::this_thread::sleep_for(::std::chrono::milliseconds{10}); }
    }
    if(shutdown)
    {
        // Exercise close after BOTH resumes can return to the linked wait.
        if(!immediate_shutdown) { ::std::this_thread::sleep_for(::std::chrono::milliseconds{50}); }
        // A genuine managed request closes/wakes the actual domain and wait
        // registry. Its private native signal must unwind both nested frames
        // and precise GC roots without inventing a Wasm wait result.
        auto request{lib::runtime_begin_llvm_jit_debug_shutdown_host_api(self->control)};
        REQUIRE(request.owner && request.status != lib::llvm_jit_debug_shutdown_status::unsupported_mode);
        auto const until{deadline()};
        while(completed.load() != 2u && ::std::chrono::steady_clock::now() < until) { ::std::this_thread::yield(); }
        REQUIRE(completed.load() == 2u);
        for(auto& guest : guests) { guest.join(); }
        REQUIRE(results[0u] == 99u && results[1u] == 99u);
        REQUIRE(lib::runtime_poll_llvm_jit_debug_shutdown_host_api(request.owner, 0u) == lib::llvm_jit_debug_shutdown_status::resources_quiescent);
        REQUIRE(lib::runtime_release_llvm_jit_debug_shutdown_host_api(request.owner));
    }
    else
    {
        // The debugger changed memory from7 to9. Re-comparing or restarting either
        // linked wait would return1 and lose this genuine two-node notification.
        ::std::uint32_t count{2u}, notified{};
        lib::full_compile_run_config notify{}; notify.entry_function_index = 2u;
        notify.entry_abi_buffers.param_buffer = reinterpret_cast<::std::byte*>(::std::addressof(count));
        notify.entry_abi_buffers.param_bytes = sizeof(count);
        notify.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(notified));
        notify.entry_abi_buffers.result_bytes = sizeof(notified);
        lib::full_compile_and_run_main_module(u8"blocked-cohort", notify); REQUIRE(notified == 2u);
        auto const until{deadline()};
        while(completed.load() != 2u && ::std::chrono::steady_clock::now() < until) { ::std::this_thread::yield(); }
        REQUIRE(completed.load() == 2u);
        for(auto& guest : guests) { guest.join(); }
        REQUIRE(results[0u] == 0u && results[1u] == 0u);
        lib::full_compile_and_run_main_module(u8"blocked-cohort", notify); REQUIRE(notified == 0u);
    }
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("blocked wait cohort PASS two workers, two real pauses, nested frames, GC locals, dynamic operand prefixes, ",
        shutdown ? ::fast_io::string_view{"managed quit, no fabricated Wasm result"} : ::fast_io::string_view{"notify2/0"},
        ", no replay, no VM asm");
}
