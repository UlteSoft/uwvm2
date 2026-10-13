// Real full-JIT guest threads, actual before-unwind exception observation and
// one complete stopped cohort. No WASIp1, fake activation or native VM access.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasm_mutation.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>

namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
namespace wm = ::uwvm2::uwvm::debugger::wasm_mutation;
namespace container = ::uwvm2::utils::container;
using domain = threads::cooperative_pause_domain;
static void require(bool value, ::fast_io::string_view message)
{
    if(!value) { ::fast_io::io::perrln("threaded uncaught: ", message); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{120}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(2u)};
    ::std::mutex mutex{};
    ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    ::std::uint_least64_t companion{}, throwing{};
    threads::cooperative_pause_location thrown_at{};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> captures{};
    ::std::array<::std::uint_least64_t, 2u> captured_ids{};
    ::std::size_t captured{}, uncaught_events{};
    bool ready{}, handled{}, finished{};
    static void point(void* context, ::std::uint_least64_t id, threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained observer owner] lifetime covers both workers.
        // [safe] installed native context; no guest descriptor chooses this borrow.
        auto& self{*static_cast<observer*>(context)};
        if(where.function < 3u)
        {
            auto const observed{lib::llvm_jit_observe_checkpoint_recording_host_api()};
            ::fast_io::io::println(::fast_io::out(), "THREAD_THROW_SITE function=", where.function, " offset=", where.offset,
                " shadow-status=", static_cast<unsigned>(observed.status), " frames=", observed.native_frames,
                " slots=", observed.typed_slots, " site=", observed.site, " current-opcode=", observed.at_current_opcode);
        }
        if(where.function != 4u || where.offset < 6u) { return; }
        ::std::lock_guard lock{self.mutex};
        if(!self.ready) { self.companion = id; self.ready = true; self.changed.notify_all(); }
    }
    static bool uncaught(void* context, ::std::uint_least64_t id, threads::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(context)};
        ::std::lock_guard lock{self.mutex};
        require(!self.handled && self.ready && self.companion != id && self.uncaught_events++ == 0u,
            "only the actual unhandled throwing worker issues the terminal event");
        require(where.function < 3u && !self.ticket, "throw attributed to the real leaf before unwind");
        self.throwing = id; self.thrown_at = where;
        self.ticket = self.control->request_pause(); require(bool(self.ticket), "actual two-participant pause requested");
        self.changed.notify_all(); return true;
    }
    static void before_park(void* context, ::std::uint_least64_t id, threads::cooperative_pause_location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(context)};
        ::std::lock_guard lock{self.mutex};
        require(bool(self.ticket) && self.captured < self.captures.size(), "one capture for each actual paused worker");
        for(::std::size_t i{}; i != self.captured; ++i) { require(self.captured_ids[i] != id, "no duplicate participant capture"); }
        auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(actual.status == lib::llvm_jit_checkpoint_capture_status::captured && bool(actual.capture),
            "authentic canonical typed capture issued on the live worker before park");
        self.captured_ids[self.captured] = id; self.captures[self.captured++] = ::std::move(actual.capture);
        self.changed.notify_all();
    }
};
static ::std::uint64_t bits(ws::value const& value)
{
    require(value.available, "numeric row is actually available");
    auto const width{value.type.kind == ws::value_kind::i32 ? 4u : 8u};
    require(value.type.kind == ws::value_kind::i32 || value.type.kind == ws::value_kind::i64, "original numeric type");
    ::std::uint64_t result{};
    for(unsigned i{}; i != width; ++i) { result |= ::std::uint64_t{::std::to_integer<unsigned char>(value.bits[i])} << (i * 8u); }
    return result;
}
int main(int argc, char** argv)
{
    if(argc != 5) { return 2; }
    auto const stack{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    auto const jit{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    auto const disposition{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[4]))};
    require(stack == "instruction" || stack == "unwind", "explicit call stack policy");
    require(jit == "default" || jit == "max", "explicit JIT optimization policy");
    require(disposition == "caught" || disposition == "uncaught", "explicit actual exception disposition");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = stack == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    mode::global_runtime_llvm_jit_policy = jit == "max" ? mode::runtime_llvm_jit_policy_t::max :
        mode::runtime_llvm_jit_policy_t::default_policy;
    auto const path{container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"wasm-threaded-uncaught", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    features.disable_simd = false; features.explicit_enable_simd = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"wasm-threaded-uncaught";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "original Wasm parse, validate, initialize and canonical source ownership");
    auto state{::std::make_shared<observer>()}; state->handled = disposition == "caught";
    lib::llvm_jit_debug_observer callbacks{}; callbacks.context = state; callbacks.on_safe_point = observer::point;
    callbacks.on_before_park = observer::before_park; callbacks.on_uncaught = observer::uncaught;
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, ::std::move(callbacks),
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "actual debug-only publication policy");
    require(lib::llvm_jit_configure_debug_value_observation_host_api({}) == lib::llvm_jit_debug_configure_result::ok,
        "actual observation profile, without executable restoration");
    require(lib::llvm_jit_prepare_debug_host_api(), "actual full LLVM engine publication");
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasm-threaded-uncaught");
    ::std::uint32_t companion_result{0xa5a5a5a5u}, throw_result{0xa5a5a5a5u};
    auto run{[](unsigned function, ::std::uint32_t parameter, ::std::uint32_t& result)
    {
        lib::full_compile_run_config config{}; config.entry_function_index = function;
        // [live worker-owned scalar argument/result] exact ABI extents
        // [safe] synchronous entry; both owners outlive the native call/join.
        config.entry_abi_buffers.param_buffer = reinterpret_cast<::std::byte const*>(::std::addressof(parameter));
        config.entry_abi_buffers.param_bytes = sizeof(parameter);
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result));
        config.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"wasm-threaded-uncaught", config);
    }};
    ::fast_io::native_thread companion{[&] { run(4u, 222u, companion_result); }};
    { ::std::unique_lock lock{state->mutex}; require(state->changed.wait_until(lock, deadline(), [&] { return state->ready; }),
        "second real guest is executing its Wasm loop"); }
    ::fast_io::native_thread throwing{[&]
    {
        run(3u, 111u, throw_result);
        ::std::lock_guard lock{state->mutex}; state->finished = true; state->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock, deadline(), [&] { return state->finished || bool(state->ticket); }),
            "actual handled return or before-unwind exception event");
        if(state->handled)
        {
            require(state->finished && state->uncaught_events == 0u && throw_result == 111u, "real matching handler prevents an uncaught event");
            state->ticket = state->control->request_pause(); require(bool(state->ticket), "pause remaining real guest after handled return");
        }
        else { require(!state->finished && state->uncaught_events == 1u && throw_result == 0xa5a5a5a5u, "terminal stop precedes return and unwind"); }
        ticket = state->ticket;
    }
    if(state->handled) { throwing.join(); }
    require(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused,
        "complete actual participant cohort is parked");
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> owners{};
    ::std::size_t count{};
    { ::std::lock_guard lock{state->mutex}; count = state->captured; owners = state->captures;
        require(count == (state->handled ? 1u : 2u), "one canonical capture per actual living Wasm thread"); }
    ::std::span roster{owners.data(), count};
    auto query{[&](::std::uint64_t id, ws::selection selected, ::std::uint64_t frame, ::std::uint64_t first, ::std::uint64_t size)
    {
        ws::request request{}; request.participant = id; request.selected = selected; request.frame = frame;
        request.first = first; request.count = size;
        auto view{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, roster, request)};
        require(view.result == ws::status::available && view.rows.size() == size, "real coherent typed data from selected worker/frame");
        return view;
    }};
    auto sibling{query(state->companion, ws::selection::locals, 0u, 0u, 2u)};
    require(bits(sibling.rows[0u].data) == 222u && bits(sibling.rows[1u].data) == 1234u,
        "other worker retains distinct original i32/i64 locals");
    ws::request selector{}; selector.participant = state->companion; selector.selected = ws::selection::locals; selector.count = 1u;
    if(!state->handled)
    {
        auto leaf{query(state->throwing, ws::selection::locals, 0u, 0u, 1u)};
        auto caller{query(state->throwing, ws::selection::locals, 1u, 0u, 2u)};
        require(bits(leaf.rows[0u].data) == 111u && bits(caller.rows[0u].data) == 111u &&
            bits(caller.rows[1u].data) == static_cast<::std::uint64_t>(-888ll), "throwing leaf and pending real caller remain intact");
        auto operands{query(state->throwing, ws::selection::operands, 0u, 0u, 1u)};
        require(operands.rows[0u].data.type.kind == ws::value_kind::i64 &&
            bits(operands.rows[0u].data) == static_cast<::std::uint64_t>(-991ll), "actual saved Wasm operand prefix survives native exception construction");
        auto const leaf_function{state->thrown_at.function};
        auto payload{query(state->throwing, ws::selection::operands, 0u, 0u, leaf_function == 1u ? 5u : 2u)};
        if(leaf_function != 2u)
        {
            require(payload.rows[1u].data.type.kind == ws::value_kind::i32 && bits(payload.rows[1u].data) == 111u,
                "exact original numeric thrown operand");
            if(leaf_function == 1u)
            {
                require(bits(payload.rows[2u].data) == static_cast<::std::uint64_t>(-1234567890123ll) &&
                    payload.rows[3u].data.available && payload.rows[3u].data.type.kind == ws::value_kind::v128,
                    "exact original i64 and v128 tuple types");
                for(unsigned i{}; i != 16u; ++i)
                { require(payload.rows[3u].data.bits[i] == static_cast<::std::byte>(i % 4u == 0u ? i / 4u + 1u : 0u), "canonical vector lane bits"); }
            }
        }
        auto const rooted{leaf_function == 0u ? nullptr : ::std::addressof(payload.rows[leaf_function == 1u ? 4u : 1u].data)};
        if(rooted != nullptr)
        {
            require(rooted->available && rooted->type.kind == ws::value_kind::reference && payload.objects.size() == 1u,
                "actual rooted reference remains inspectable across the two-worker pause");
            auto const& object{payload.objects.front()};
            require(object.identifier == rooted->ref.object && object.members.size() == 1u &&
                bits(object.members[0u].data) == (leaf_function == 1u ? 42u : 111u), "exact live GC/exception payload member");
            require(leaf_function == 1u ? object.kind == ws::object_kind::structure :
                object.kind == ws::object_kind::exception && object.tag_identity_available && object.tag_module == 0u && object.tag_index == 0u,
                "real structure or current exception tag identity");
        }
        auto parent_stack{query(state->throwing, ws::selection::operands, 1u, 0u, 1u)};
        require(bits(parent_stack.rows[0u].data) == static_cast<::std::uint64_t>(-777ll), "pending caller's data stack remains readable before unwind");
        auto incomplete{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, roster.first(1u), selector)};
        require(incomplete.result == ws::status::incomplete_cohort && incomplete.rows.empty(), "omitting a live worker refuses all typed rows");
        ::std::array repeated{owners[0u], owners[0u]};
        auto duplicate{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, repeated, selector)};
        require(duplicate.result != ws::status::available && duplicate.rows.empty(), "duplicate capture cannot substitute for complete cohort");
        auto forged{owners};
        forged[0u] = lib::llvm_jit_checkpoint_thread_capture_owner{owners[0u].get(), [](auto*) noexcept {}};
        auto foreign{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, forged, selector)};
        require(foreign.result != ws::status::available && foreign.rows.empty(), "same native pointer with a foreign control block cannot mint read permission");
        ::fast_io::io::println(::fast_io::out(), "WASM_THREADED_UNCAUGHT PASS threads=2 captures=2 throwing=", state->throwing,
            " companion=", state->companion, " function=", state->thrown_at.function,
            " before-unwind=1 caller-stack=1 payload-exact=1 rooted-payload=1 incomplete-refused=1 duplicate-refused=1 foreign-owner-refused=1 fatal-continuation=expected");
        require(state->control->resume(ticket), "continue original actual unhandled exception");
        throwing.join(); require(false, "uncaught VM continuation must retain fatal behavior");
    }
    wm::request release{}; release.participant = state->companion; release.source = wm::source_kind::numeric_bits;
    release.bits[0u] = ::std::byte{1u};
    auto committed{lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket, roster, release)};
    require(committed.status == ws::status::available && committed.applied, "actual closed-cohort global commit releases companion");
    require(state->control->resume(ticket), "resume real companion after handled negative case"); companion.join();
    require(companion_result == 222u && throw_result == 111u, "both original result ABIs remain correct");
    auto stale{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, roster, selector)};
    require(stale.result != ws::status::available && stale.rows.empty(), "departed native workers revoke old captured read permission");
    lib::reset_runtime_state_host_api(); require(state->control->is_closed(), "actual session owner closes after native joins");
    ::fast_io::io::println(::fast_io::out(), "WASM_THREADED_CAUGHT PASS uncaught-events=0 other-thread-data=1 native-joins=2 stale-refused=1");
}
