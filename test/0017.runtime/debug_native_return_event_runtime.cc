// Genuine full-runtime trap/CFI/live-stack/parent-owner join. No constructed
// native stack or LLVM DATA fixture can satisfy the positive caller counts.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/debugger/native_step.h>
#include <uwvm2/uwvm/debugger/native_continuation_linux.h>
#include <type_traits>
#include <uwvm2/uwvm/debugger/native_wasm_step_boundary.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace dbg = uwvm2::uwvm::debugger;
namespace threads = uwvm2::utils::thread;
using domain = threads::cooperative_pause_domain;
static void check(bool value, char const* message)
{
    if(!value) { ::fast_io::io::perrln("debug_native_return_event_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
static void await_phase(dbg::native_step::session& session, dbg::native_step::phase expected)
{
    auto const end{deadline()};
    while(session.state.load(::std::memory_order_acquire) != expected)
    { if(::std::chrono::steady_clock::now() >= end)
      {
          ::fast_io::io::perrln("bounded phase failure state=",static_cast<unsigned>(session.state.load(::std::memory_order_seq_cst)),
              " abort=",session.abort_requested.load(::std::memory_order_seq_cst),
              " reason=",dbg::native_step::details::continuation_failure.load(::std::memory_order_relaxed),
              " other-stack-hits=",dbg::native_step::details::continuation_other_stack_hits.load(::std::memory_order_relaxed));
          ::fast_io::fast_terminate();
      } ::std::this_thread::yield(); }
}
// This fixture qualifies the protected return-proof/wake transaction with
// a separately authenticated contained instruction. It does NOT install a
// cross-owner finish event or treat a callback count as successful finish.
struct return_resume_probe
{
    dbg::native_step::session* session{};
    lib::llvm_jit_debug_native_return_continuation_owner proof{};
    bool execute{}, reject_wake{}, called{}, committed{}, retry_denied{}, reentry_denied{};
    lib::llvm_jit_debug_native_caller_view parent{};
    static void resume(void* context, lib::llvm_jit_debug_native_caller_view const& parent,
        domain::external_resume_borrow& borrow) noexcept
    {
        auto& self{*static_cast<return_resume_probe*>(context)};
        check(!self.called && parent.valid && parent.incarnation != parent.current_incarnation,
            "protected callback has a freshly authenticated distinct Wasm parent");
        self.called = true; self.parent = parent;
        // Nested public entry is denied by depth BEFORE it can try to reenter
        // the real domain mutex. No fabricated domain/worker state is used.
        self.reentry_denied = !lib::llvm_jit_debug_resume_native_return_continuation_host_api(
            self.proof, self.session, context, resume);
        check(self.reentry_denied, "protected callback rejects nested resume before domain reentry");
        if(self.reject_wake)
        {
            self.committed = borrow.commit([]() noexcept { return false; });
            self.retry_denied = !borrow.commit([]() noexcept { return true; });
            check(!self.committed && self.retry_denied, "failed wake rolls back and consumes the one-shot borrow");
        }
        else if(self.execute)
        { self.committed = borrow.commit([&]() noexcept { return dbg::native_step::continue_from_trap(*self.session); }); }
    }
};
// The positive count below requires actual cross-owner execution and a new
// synchronous kernel trap. A callback invocation/accepted wake alone is no hit.
static_assert(!::std::is_constructible_v<lib::llvm_jit_debug_native_return_event, ::std::uintptr_t>);
struct return_event_probe
{
    dbg::native_step::session* session{};
    lib::llvm_jit_debug_native_return_continuation_owner proof{};
    lib::llvm_jit_debug_native_return_event plan{};
    ::fast_io::native_file file{};
    bool execute{}, reject{}, called{}, committed{}, reentry_denied{}, retry_denied{};
    static void resume(void* opaque, lib::llvm_jit_debug_native_return_event const& plan,
        domain::external_resume_borrow& borrow) noexcept
    {
        auto& self{*static_cast<return_event_probe*>(opaque)};
        check(!self.called && plan.valid() && plan.thread() == self.session->target_thread,
            "fresh sealed return event in protected callback"); self.called = true; self.plan = plan;
        self.reentry_denied = !lib::llvm_jit_debug_resume_native_return_event_host_api(
            self.proof,self.session,opaque,resume);
        check(self.reentry_denied,"nested return event resume denied before domain mutex");
        if(self.reject)
        {
            self.committed = borrow.commit([]() noexcept { return false; });
            self.retry_denied = !borrow.commit([]() noexcept { return true; }); return;
        }
        if(!self.execute) { return; }
        static ::std::uint64_t cookie{0x5530523000000000ull}; ++cookie;
        ::perf_event_attr attributes{};
        check(dbg::native_perf_signal_linux::kernel_supports_async_delivery() &&
            dbg::native_continuation_linux::make_attributes(plan.event_pc(),cookie,attributes),"precise kernel event ABI");
        auto const fd{dbg::posix_abi::syscall_noexcept(SYS_perf_event_open,::std::addressof(attributes),
            static_cast<int>(plan.thread()),-1,-1,static_cast<unsigned long>(PERF_FLAG_FD_CLOEXEC))};
        check(fd >= 0 && fd <= INT_MAX,"real parent return event open"); self.file.reset(static_cast<int>(fd));
        check(dbg::posix_abi::syscall_noexcept(SYS_ioctl,self.file.native_handle(),
            static_cast<unsigned long>(PERF_EVENT_IOC_ENABLE),0ul) == 0,"enable owned parent event while worker trapped");
        self.committed = borrow.commit([&]() noexcept
        { return dbg::native_step::continue_to_return(*self.session,plan,cookie,self.file.native_handle()); });
        check(self.committed,"sealed cross-owner return backend wake");
    }
    void disable() noexcept
    {
        if(!file) { return; }
        check(dbg::posix_abi::syscall_noexcept(SYS_ioctl,file.native_handle(),
            static_cast<unsigned long>(PERF_EVENT_IOC_DISABLE),0ul) == 0,"real event disable before ACK");
        dbg::native_step::acknowledge_continuation_event_retired(*session);
    }
};
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{}; lib::llvm_jit_debug_activation_capture_owner capture{};
    lib::llvm_jit_debug_native_step_site site{};
    ::std::size_t serial{}; bool done{};
    static void stop(void* context, ::std::uint_least64_t, threads::cooperative_pause_location) noexcept
    {
        auto& self{*static_cast<observer*>(context)};
        auto ticket{self.control->request_pause()};
        // Native release keeps the cooperative ticket outstanding until ACK.
        // A following real safe point can encounter that same ticket; it must
        // park on it without inventing another observer episode.
        if(!ticket) { return; }
        ::std::lock_guard lock{self.mutex}; self.ticket = ::std::move(ticket); self.capture.reset(); self.site = {};
        ++self.serial; self.changed.notify_all();
    }
    static void before_park(void* context, ::std::uint_least64_t, threads::cooperative_pause_location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(context)};
        ::std::lock_guard lock{self.mutex};
        self.capture = lib::llvm_jit_debug_capture_activation_host_api(self.ticket);
        self.site = lib::llvm_jit_capture_debug_native_step_site_host_api();
    }
};
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    if(!dbg::native_step::platform_available()) { return 77; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    auto const scenario{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
    if(scenario != "shallow" && scenario != "deep") { return 2; }
    bool const deep{scenario == "deep"};
    auto const entry{deep ? 3u : 2u}; auto const expected_result{deep ? 48u : 14u};
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{uwvm2::uwvm::cmdline::parsing_result};
    uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(uwvm2::utils::cmdline::parameter_parsing_results{
        u8"physical-caller", nullptr, uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(uwvm2::utils::cmdline::parameter_parsing_results{
        uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"physical-caller";
    check(uwvm2::uwvm::run::prepare_owned_full_cli_source() == static_cast<int>(uwvm2::uwvm::run::retval::ok), "actual owning initializer");
    auto state{::std::make_shared<observer>()};
    check(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::stop, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "actual debug-full opt-in");
    check(lib::llvm_jit_prepare_debug_host_api() && lib::llvm_jit_enable_debug_native_step_host_api(), "real full JIT/native admission");
    ::std::uint32_t result{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config config{}; config.entry_function_index = entry;
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result));
        config.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"physical-caller", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    ::std::size_t serial{}, physical{}, recursive{}, host_refused{}, stale_refused{}, boundary_refused{}, replacement_callers{};
    ::std::size_t multiple{}, complete{}, bounded{}, maximum_depth{};
    bool replaced{};
    ::std::size_t event_hits{}, event_recursive{}, event_replaced{}, event_retained{}, event_rollback{}, event_refused{};
    ::std::size_t return_proofs{}, return_refused{}, return_alias_refused{}, return_stale_refused{};
    ::std::size_t return_advanced_refused{}, return_fresh_after_step{};
    ::std::size_t resume_committed{}, resume_retained{}, resume_rollback{}, resume_refused{}, resume_reentry_refused{};
    lib::llvm_jit_debug_native_return_continuation_owner retained_return{};
    lib::llvm_jit_debug_native_activation_cursor_owner retained{};
    dbg::native_step::session session{};
    lib::llvm_jit_debug_native_return_stop retained_parent{};
    ::std::size_t parent_captures{}, parent_backtraces{}, parent_steps{}, parent_step_refused{}, parent_refused{};
    ::std::size_t chained_hits{}, chained_captures{}, chained_refused{}, maximum_chained_depth{}, chained_root_refused{}, chained_recursive{};
    for(;;)
    {
        domain::pause_ticket ticket{};
        {
            ::std::unique_lock lock{state->mutex};
            check(state->changed.wait_until(lock, deadline(), [&] { return state->done || state->serial > serial; }), "actual event wait");
            if(state->done) { break; }
            check(state->serial == serial + 1u && serial < 1024u, "bounded event count"); serial = state->serial; ticket = state->ticket;
        }
        check(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused, "real cooperative park");
        lib::llvm_jit_debug_activation_capture_owner capture{}; lib::llvm_jit_debug_native_step_site site{};
        { ::std::lock_guard lock{state->mutex}; capture = state->capture; site = state->site; }
        lib::llvm_jit_debug_activation_snapshot frames{};
        check(capture && lib::llvm_jit_debug_query_activation_host_api(capture, frames), "canonical actual dynamic chain");
        if(!replaced)
        {
            check(frames.frames.size() == 1u && frames.frames.back().function == entry, "actual inactive leaf before replacement");
            // Genuine Wasm body (locals + local.get0 + i32.const7 + add + end),
            // validated by the real replacement compiler, never native bytes.
            ::std::array<::std::byte,7u> body{::std::byte{0},::std::byte{0x20},::std::byte{0},
                ::std::byte{0x41},::std::byte{7},::std::byte{0x6a},::std::byte{0x0b}};
            auto prepared{lib::llvm_jit_debug_prepare_function_replacement_host_api(frames.location.code_unit, 0u, 1u, body.data(), body.size())};
            check(prepared.status == lib::llvm_jit_debug_replace_status::replaced && prepared.transaction, "real replacement validator and private JIT");
            lib::llvm_jit_debug_replace_result committed{};
            check(state->control->while_stopped(ticket, [&]
            { committed = lib::llvm_jit_debug_commit_function_replacement_host_api(prepared.transaction); }), "actual all-stopped replacement commit");
            lib::llvm_jit_debug_discard_function_replacement_host_api(prepared.transaction);
            check(committed.status == lib::llvm_jit_debug_replace_status::replaced && committed.generation == 2u, "real generation two published");
            replaced = true;
        }
        lib::llvm_jit_debug_native_caller_view view{}; view.valid = true;
        check(!lib::llvm_jit_debug_native_return_continuation_host_api(retained_return, ::std::addressof(session), view) && !view.valid &&
            !view.module && !view.function && !view.function_generation && !view.runtime_epoch && !view.incarnation && !view.current_incarnation,
            "return proof from a prior real episode is revoked and clears all output"); ++return_stale_refused;
        return_resume_probe departed{::std::addressof(session), retained_return};
        check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(retained_return, ::std::addressof(session),
            ::std::addressof(departed), return_resume_probe::resume) && !departed.called,
            "prior episode cannot enter a resume callback at a new cooperative stop"); ++resume_refused;
        check(!lib::llvm_jit_debug_native_caller_host_api(retained, ::std::addressof(session), view) && !view.valid && !view.incarnation,
            "old cursor cannot recover a caller at a new cooperative episode"); ++stale_refused;
        lib::llvm_jit_debug_native_function_image image{};
        check(lib::llvm_jit_debug_copy_native_function_host_api(capture, nullptr, image), "actual bounded owned function image");
        dbg::native_disassembly::decoder display{image.target}; dbg::native_owned_instruction_semantics::decoder semantics{image.target};
        auto const permitted{dbg::native_wasm_step_boundary::prepare(display, semantics, {image.bytes.data(), image.size},
            image.owner_begin, image.owner_end, image.stop_pc, false)};
        if(!site.valid || !permitted)
        { ++boundary_refused; check(state->control->resume(ticket), "unproved successor retains native denial and resumes genuine Wasm"); continue; }
        auto cursor{lib::llvm_jit_debug_mint_native_activation_host_api(capture, ::std::addressof(session))};
        lib::llvm_jit_debug_native_activation_provider provider{};
        check(cursor && lib::llvm_jit_debug_native_activation_provider_host_api(cursor, provider), "real activation provider");
        check(!lib::llvm_jit_debug_native_caller_host_api(cursor, ::std::addressof(session), view) && !view.valid, "cooperative frame is not a physical caller proof");
        check(!lib::llvm_jit_debug_mint_native_return_continuation_host_api(cursor, ::std::addressof(session)),
            "cooperative capture cannot mint a physical return capability");
        check(dbg::native_step::request(session, site.native_thread, site.owner_begin, site.owner_end, site.return_pc, provider), "real native request");
        check(state->control->release_one_for_native_step(ticket, site.participant), "actual selected worker release");
        await_phase(session, dbg::native_step::phase::at_guest_pc);
        check(dbg::native_step::continue_one(session), "contained native instruction execution");
        await_phase(session, dbg::native_step::phase::trapped);
        check(state->control->external_park(ticket, site.participant, frames.location), "actual kernel trap domain marker");
        check(lib::llvm_jit_debug_native_activation_host_api(cursor, ::std::addressof(session)), "same real native witness");
        return_event_probe executed{::std::addressof(session)};
        ::std::array<return_event_probe,3u> chained_events{};
        ::std::array<lib::llvm_jit_debug_native_return_stop,3u> chained_stops{};
        bool const found{lib::llvm_jit_debug_native_caller_host_api(cursor, ::std::addressof(session), view)};
        auto return_proof{lib::llvm_jit_debug_mint_native_return_continuation_host_api(cursor, ::std::addressof(session))};
        check(static_cast<bool>(return_proof) == found, "return capability requires exactly the genuine first physical parent proof");
        if(found)
        {
            check(frames.frames.size() >= 2u && view.valid, "physical caller requires a real parent");
            auto const& parent{frames.frames[frames.frames.size() - 2u]}; auto const& current{frames.frames.back()};
            check(view.module == parent.module && view.function == parent.function && view.function_generation == parent.function_generation &&
                view.runtime_epoch == parent.runtime_epoch && view.incarnation == parent.incarnation && view.current_incarnation == current.incarnation &&
                current.parent == parent.incarnation, "actual CFI caller owner/epoch and distinct dynamic incarnations join");
            lib::llvm_jit_debug_native_backtrace_view trace{};
            check(lib::llvm_jit_debug_native_backtrace_host_api(cursor, ::std::addressof(session), trace), "actual multi-frame trace");
            auto const parents{frames.frames.size() - 1u}; auto const required{parents < 32u ? parents : 32u};
            check(trace.count == required && trace.complete == (parents <= 32u), "complete physical chain or explicit bounded prefix");
            for(::std::size_t i{}; i != trace.count; ++i)
            {
                auto const& actual{trace.frames[i]}; auto const& logical{frames.frames[parents - 1u - i]};
                auto const child{frames.frames[parents - i].incarnation};
                check(actual.valid && actual.module == logical.module && actual.function == logical.function &&
                    actual.function_generation == logical.function_generation && actual.runtime_epoch == logical.runtime_epoch &&
                    actual.incarnation == logical.incarnation && actual.current_incarnation == child && actual.incarnation != child,
                    "every recovered physical frame joins the actual dynamic Wasm chain");
            }
            check(trace.frames[0u].incarnation == view.incarnation, "legacy single caller remains the same authenticated first frame");
            if(trace.count > 1u) { ++multiple; } if(trace.complete) { ++complete; } if(!trace.complete && trace.count == 32u) { ++bounded; }
            if(trace.count > maximum_depth) { maximum_depth = trace.count; }
            check(lib::llvm_jit_debug_native_backtrace_host_api(cursor, ::std::addressof(session), trace, 1u) && trace.count == 1u &&
                trace.complete == (parents == 1u), "explicit one-frame limit returns a labelled prefix");
            check(!lib::llvm_jit_debug_native_backtrace_host_api(cursor, ::std::addressof(session), trace, 0u) && !trace.count && !trace.complete,
                "zero limit refuses and clears output");
            check(!lib::llvm_jit_debug_native_backtrace_host_api(cursor, ::std::addressof(session), trace, 33u) && !trace.count && !trace.complete,
                "out-of-capacity limit refuses before reading");
            lib::llvm_jit_debug_native_caller_view return_view{};
            check(lib::llvm_jit_debug_native_return_continuation_host_api(return_proof, ::std::addressof(session), return_view) && return_view.valid &&
                return_view.module == view.module && return_view.function == view.function &&
                return_view.function_generation == view.function_generation && return_view.runtime_epoch == view.runtime_epoch &&
                return_view.incarnation == view.incarnation && return_view.current_incarnation == view.current_incarnation,
                "sealed return proof reauthenticates actual CFI target and emits only parent Wasm identity"); ++return_proofs;
            lib::llvm_jit_debug_native_return_continuation_owner unreadable{return_proof,
                reinterpret_cast<lib::llvm_jit_debug_native_return_continuation const*>(::std::uintptr_t{1u})};
            check(!lib::llvm_jit_debug_native_return_continuation_host_api(unreadable, ::std::addressof(session), return_view) && !return_view.valid,
                "unreadable return alias rejected before dereference"); ++return_alias_refused;
            lib::llvm_jit_debug_native_return_continuation_owner counterfeit{return_proof.get(),
                [](lib::llvm_jit_debug_native_return_continuation const*) noexcept {}};
            check(!lib::llvm_jit_debug_native_return_continuation_host_api(counterfeit, ::std::addressof(session), return_view) && !return_view.valid,
                "same pointer with a different control block cannot counterfeit return authority"); ++return_alias_refused;
            check(!lib::llvm_jit_debug_native_return_continuation_host_api(return_proof,
                reinterpret_cast<void const*>(::std::uintptr_t{1u}), return_view) && !return_view.valid,
                "different session cannot query the sealed physical return"); ++return_alias_refused;
            return_resume_probe refused{::std::addressof(session), return_proof};
            for(auto const& invalid : {unreadable, counterfeit})
            {
                check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(invalid, ::std::addressof(session),
                    ::std::addressof(refused), return_resume_probe::resume) && !refused.called,
                    "forged return owner cannot enter a protected resume callback"); ++resume_refused;
            }
            check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof,
                reinterpret_cast<void const*>(::std::uintptr_t{1u}), ::std::addressof(refused), return_resume_probe::resume) && !refused.called,
                "wrong session cannot enter return resume callback"); ++resume_refused;
            check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
                ::std::addressof(refused), nullptr) && !refused.called, "null return resume callback refuses"); ++resume_refused;
            return_resume_probe keep{::std::addressof(session), return_proof};
            check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
                ::std::addressof(keep), return_resume_probe::resume) && keep.called && keep.reentry_denied && !keep.committed,
                "inspection-only callback keeps the genuine current stop"); ++resume_retained; ++resume_reentry_refused;
            check(lib::llvm_jit_debug_native_return_continuation_host_api(return_proof, ::std::addressof(session), return_view),
                "uncommitted callback preserves exact trap and actual park ownership");
            return_resume_probe rollback{::std::addressof(session), return_proof}; rollback.reject_wake = true;
            check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
                ::std::addressof(rollback), return_resume_probe::resume) && rollback.called && rollback.retry_denied && rollback.reentry_denied,
                "failed synchronous wake restores genuine domain accounting"); ++resume_rollback; ++resume_reentry_refused;
            check(lib::llvm_jit_debug_native_return_continuation_host_api(return_proof, ::std::addressof(session), return_view),
                "failed wake leaves the original return proof and external trap usable");
            // Advance one MORE proved instruction in this SAME native session.
            // Even when the parent/return target is unchanged, an old handle
            // must not authorize the later real kernel trap revision.
            lib::llvm_jit_debug_native_function_image next_image{};
            if(lib::llvm_jit_debug_copy_native_function_host_api(capture, ::std::addressof(session), next_image))
            {
                auto const next{dbg::native_wasm_step_boundary::prepare(display, semantics, {next_image.bytes.data(), next_image.size},
                    next_image.owner_begin, next_image.owner_end, next_image.stop_pc, false)};
                if(next)
                {
                    return_resume_probe wake{::std::addressof(session), return_proof}; wake.execute = true;
                    check(lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
                        ::std::addressof(wake), return_resume_probe::resume) && wake.called && wake.committed && wake.reentry_denied &&
                        wake.parent.incarnation == parent.incarnation && wake.parent.current_incarnation == current.incarnation,
                        "fresh physical proof and actual contained instruction wake share one domain/publication scope");
                    ++resume_committed; ++resume_reentry_refused;
                    await_phase(session, dbg::native_step::phase::trapped);
                    check(state->control->external_park(ticket, site.participant, frames.location), "publish second real trap marker");
                    check(lib::llvm_jit_debug_native_activation_host_api(cursor, ::std::addressof(session)), "second real trap has genuine activation");
                    check(!lib::llvm_jit_debug_native_return_continuation_host_api(return_proof, ::std::addressof(session), return_view) && !return_view.valid,
                        "old return capability rejects a later real trap in the same physical invocation"); ++return_advanced_refused;
                    return_resume_probe stale{::std::addressof(session), return_proof};
                    check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
                        ::std::addressof(stale), return_resume_probe::resume) && !stale.called,
                        "later real trap rejects old return execution proof before invoking callback"); ++resume_refused;
                    auto fresh{lib::llvm_jit_debug_mint_native_return_continuation_host_api(cursor, ::std::addressof(session))};
                    if(fresh)
                    {
                        check(lib::llvm_jit_debug_native_return_continuation_host_api(fresh, ::std::addressof(session), return_view) && return_view.valid &&
                            return_view.incarnation == parent.incarnation && return_view.current_incarnation == current.incarnation,
                            "fresh second-trap return capability reauthenticates the same actual parent"); ++return_fresh_after_step;
                        lib::llvm_jit_debug_native_caller_view ended{};
                        return_event_probe keep_event{::std::addressof(session),fresh};
                        check(!lib::llvm_jit_debug_resume_native_return_event_host_api(fresh,::std::addressof(session),
                            ::std::addressof(keep_event),return_event_probe::resume) && keep_event.called && !keep_event.committed,
                            "uncommitted return event retains exact origin trap"); ++event_retained;
                        check(!lib::llvm_jit_debug_query_native_return_event_host_api(keep_event.plan,::std::addressof(session),ended) && !ended.valid,
                            "sealed plan before actual return is not a completed event"); ++event_refused;
                        check(!dbg::native_step::continue_to_return(session,keep_event.plan,1u,0) &&
                            session.state.load(::std::memory_order_acquire) == dbg::native_step::phase::trapped,
                            "retained uncommitted plan cannot wake outside its issuing callback"); ++event_refused;
                        return_event_probe failed_event{::std::addressof(session),fresh}; failed_event.reject = true;
                        check(!lib::llvm_jit_debug_resume_native_return_event_host_api(fresh,::std::addressof(session),
                            ::std::addressof(failed_event),return_event_probe::resume) && failed_event.called && failed_event.retry_denied && !failed_event.committed,
                            "failed return wake preserves actual domain accounting"); ++event_rollback;
                        lib::llvm_jit_debug_native_return_stop premature{};
                        check(!lib::llvm_jit_debug_capture_native_return_stop_host_api(keep_event.plan,::std::addressof(session),premature) &&
                            !premature.capture && !premature.cursor, "unexecuted event cannot publish a parent capture"); ++parent_refused;
                        check(lib::llvm_jit_debug_native_return_continuation_host_api(fresh,::std::addressof(session),ended),
                            "rollback preserves genuine origin proof");
                        lib::llvm_jit_debug_native_return_event empty{};
                        check(!dbg::native_step::continue_to_return(session,empty,1u,-1) &&
                            session.state.load(::std::memory_order_acquire) == dbg::native_step::phase::trapped,
                            "unissued event cannot switch native owner or wake"); ++event_refused;
                        return_event_probe forged{::std::addressof(session),fresh};
                        check(!lib::llvm_jit_debug_resume_native_return_event_host_api(counterfeit,::std::addressof(session),
                            ::std::addressof(forged),return_event_probe::resume) && !forged.called,
                            "different control block cannot issue a return event"); ++event_refused;
                        // Reach the recursive depth/registered-CFI prefix first.
                        // Then actually finish both recursive and generation-two
                        // leaf functions; their Wasm result must remain unchanged.
                        if((current.function == 0u && frames.frames.size() >= 5u) ||
                           (event_replaced != 0u && current.function == parent.function && frames.frames.size() >= 3u))
                        {
                            executed.proof = fresh; executed.execute = true;
                            check(lib::llvm_jit_debug_resume_native_return_event_host_api(fresh,::std::addressof(session),
                                ::std::addressof(executed),return_event_probe::resume) && executed.committed,
                                "actual protected parent return wake");
                            await_phase(session,dbg::native_step::phase::trapped);
                            check(state->control->external_park(ticket,site.participant,frames.location),
                                "original real ticket owns completed kernel event; no public parent snapshot claimed");
                            check(lib::llvm_jit_debug_query_native_return_event_host_api(executed.plan,::std::addressof(session),ended) && ended.valid &&
                                ended.module == parent.module && ended.function == parent.function && ended.function_generation == parent.function_generation &&
                                ended.runtime_epoch == parent.runtime_epoch && ended.incarnation == parent.incarnation && ended.current_incarnation == current.incarnation,
                                "genuine normal leave, exact parent kernel PC/CFA/revision and published owner join");
                            ++event_hits; if(parent.function == current.function) { ++event_recursive; }
                            if(current.function == 0u && current.function_generation == 2u) { ++event_replaced; }
                            check(!lib::llvm_jit_debug_query_native_return_event_host_api(executed.plan,
                                reinterpret_cast<void const*>(::std::uintptr_t{1u}),ended) && !ended.valid,
                                "wrong session cannot query the real parent event"); ++event_refused;
                            check(!lib::llvm_jit_debug_query_native_return_event_host_api(keep_event.plan,::std::addressof(session),ended) && !ended.valid,
                                "a different unexecuted sealed plan cannot claim the actual return witness"); ++event_refused;
                            lib::llvm_jit_debug_native_function_image stale_image{};
                            check(!lib::llvm_jit_debug_copy_native_function_host_api(capture,::std::addressof(session),stale_image) && !stale_image.size,
                                "old child capture never exposes the new parent or VM code");
                            lib::llvm_jit_debug_native_code_bytes stale_window{};
                            lib::llvm_jit_debug_native_position stale_position{};
                            check(!lib::llvm_jit_debug_copy_native_code_host_api(capture,::std::addressof(session),stale_window) &&
                                !stale_window.size && !stale_window.pc &&
                                !lib::llvm_jit_debug_native_position_host_api(capture,::std::addressof(session),stale_position) &&
                                stale_position.status == lib::llvm_jit_debug_native_position_status::unavailable && !stale_position.pc,
                                "recursive same-body return refuses old code window and source position");
                            lib::llvm_jit_debug_native_return_stop parent_stop{};
                            check(lib::llvm_jit_debug_capture_native_return_stop_host_api(executed.plan,::std::addressof(session),parent_stop) &&
                                parent_stop.capture && parent_stop.cursor && parent_stop.capture.get()!=capture.get() &&
                                parent_stop.cursor.get()!=cursor.get(),
                                "real completed event mints a separate native-only parent capture and cursor"); ++parent_captures;
                            check(lib::llvm_jit_debug_native_activation_host_api(parent_stop.cursor,::std::addressof(session)),
                                "fresh parent cursor owns the actual kernel trap");
                            lib::llvm_jit_debug_native_return_stop repeated{};
                            check(lib::llvm_jit_debug_capture_native_return_stop_host_api(executed.plan,::std::addressof(session),repeated) &&
                                repeated.capture.get()==parent_stop.capture.get() && repeated.cursor.get()==parent_stop.cursor.get() &&
                                !repeated.capture.owner_before(parent_stop.capture) && !parent_stop.capture.owner_before(repeated.capture),
                                "repeat publication returns the same canonical parent owner");
                            check(!lib::llvm_jit_debug_capture_native_return_stop_host_api(keep_event.plan,::std::addressof(session),repeated) &&
                                !repeated.capture && !repeated.cursor, "unexecuted plan cannot claim a published parent"); ++parent_refused;
                            check(!lib::llvm_jit_debug_capture_native_return_stop_host_api(executed.plan,
                                reinterpret_cast<void const*>(::std::uintptr_t{1u}),repeated) && !repeated.capture && !repeated.cursor,
                                "wrong session cannot publish parent owners"); ++parent_refused;
                            lib::llvm_jit_debug_activation_snapshot no_locals{};
                            lib::llvm_jit_debug_source_activation_snapshot no_source{};
                            check(!lib::llvm_jit_debug_query_activation_host_api(parent_stop.capture,no_locals) && no_locals.frames.empty() &&
                                !lib::llvm_jit_debug_query_source_activation_host_api(parent_stop.capture,{},no_source) && !no_source.source_available,
                                "native parent never promotes copied child locals or a cooperative source PC"); ++parent_refused;
                            lib::llvm_jit_debug_native_function_image parent_image{};
                            lib::llvm_jit_debug_native_code_bytes parent_window{};
                            lib::llvm_jit_debug_native_position parent_position{};
                            check(lib::llvm_jit_debug_copy_native_function_host_api(parent_stop.capture,::std::addressof(session),parent_image) &&
                                lib::llvm_jit_debug_copy_native_code_host_api(parent_stop.capture,::std::addressof(session),parent_window) &&
                                lib::llvm_jit_debug_native_position_host_api(parent_stop.capture,::std::addressof(session),parent_position) &&
                                parent_image.module==parent.module && parent_image.function==parent.function &&
                                parent_image.function_generation==parent.function_generation && parent_window.module==parent.module &&
                                parent_window.function==parent.function && parent_window.pc==parent_image.stop_pc &&
                                parent_position.module==parent.module && parent_position.function==parent.function &&
                                parent_position.pc==parent_image.stop_pc,
                                "same parent witness authorizes only its genuine owned function, code window and provenance");
                            check(!lib::llvm_jit_debug_copy_native_function_host_api(parent_stop.capture,nullptr,stale_image) && !stale_image.size,
                                "native-only parent cannot manufacture a cooperative code view"); ++parent_refused;
                            lib::llvm_jit_debug_activation_capture_owner parent_alias{parent_stop.capture,
                                reinterpret_cast<lib::llvm_jit_debug_activation_capture const*>(::std::uintptr_t{1u})};
                            lib::llvm_jit_debug_activation_capture_owner parent_counterfeit{parent_stop.capture.get(),
                                [](lib::llvm_jit_debug_activation_capture const*) noexcept {}};
                            for(auto const& invalid : {parent_alias,parent_counterfeit})
                            {
                                check(!lib::llvm_jit_debug_copy_native_function_host_api(invalid,::std::addressof(session),stale_image) &&
                                    !stale_image.size, "alias or foreign control block cannot impersonate parent"); ++parent_refused;
                            }
                            lib::llvm_jit_debug_native_backtrace_view parent_trace{};
                            check(lib::llvm_jit_debug_native_backtrace_host_api(parent_stop.cursor,::std::addressof(session),parent_trace) &&
                                parent_trace.count!=0u && parent_trace.frames[0u].incarnation==frames.frames[frames.frames.size()-3u].incarnation &&
                                parent_trace.frames[0u].current_incarnation==parent.incarnation,
                                "fresh parent can unwind the actual next Wasm parent without exposing stack bytes"); ++parent_backtraces;
                            // Use THIS exact parent's real MC target and an ordinary
                            // contained successor. Return/host escape remains denied.
                            dbg::native_disassembly::decoder parent_display{parent_image.target};
                            dbg::native_owned_instruction_semantics::decoder parent_semantics{parent_image.target};
                            auto const parent_step{dbg::native_wasm_step_boundary::prepare(parent_display,parent_semantics,
                                {parent_image.bytes.data(),parent_image.size},parent_image.owner_begin,parent_image.owner_end,
                                parent_image.stop_pc,false)};
                            if(parent_step)
                            {
                                auto parent_return{lib::llvm_jit_debug_mint_native_return_continuation_host_api(parent_stop.cursor,::std::addressof(session))};
                                check(static_cast<bool>(parent_return),"fresh parent reauthenticates its own current CFI continuation");
                                executed.disable();
                                return_resume_probe parent_wake{::std::addressof(session),parent_return}; parent_wake.execute=true;
                                check(lib::llvm_jit_debug_resume_native_return_continuation_host_api(parent_return,::std::addressof(session),
                                    ::std::addressof(parent_wake),return_resume_probe::resume) && parent_wake.committed,
                                    "fresh parent's bounded SI and actual domain wake share one transaction");
                                await_phase(session,dbg::native_step::phase::trapped);
                                check(state->control->external_park(ticket,site.participant,frames.location),
                                    "original ticket accounts for fresh parent SI completion");
                                check(lib::llvm_jit_debug_native_activation_host_api(parent_stop.cursor,::std::addressof(session)) &&
                                    lib::llvm_jit_debug_copy_native_function_host_api(parent_stop.capture,::std::addressof(session),parent_image) &&
                                    parent_image.function==parent.function,
                                    "fresh parent capture stays current after genuine bounded SI"); ++parent_steps;
                                check(!lib::llvm_jit_debug_capture_native_return_stop_host_api(executed.plan,::std::addressof(session),repeated) &&
                                    !repeated.capture && !repeated.cursor,
                                    "an advanced native revision cannot remint from the old return event"); ++parent_refused;
                            }
                            else { ++parent_step_refused; }
                            retained_parent = parent_stop;

                            // Exercise several genuine parent returns in ONE
                            // kernel session. The original cooperative ticket
                            // stays unchanged; no copied source frame is rebased.
                            auto active_parent{parent_stop};
                            for(::std::size_t hop{}; hop != chained_events.size(); ++hop)
                            {
                                lib::llvm_jit_debug_native_caller_view next{};
                                auto proof{lib::llvm_jit_debug_mint_native_return_continuation_host_api(
                                    active_parent.cursor,::std::addressof(session))};
                                check(proof && lib::llvm_jit_debug_native_return_continuation_host_api(
                                    proof,::std::addressof(session),next) && next.valid,
                                    "fresh native parent issues its own exact continuation");
                                if(hop == 0u) { executed.disable(); }
                                else { chained_events[hop-1u].disable(); }
                                auto& chained{chained_events[hop]};
                                chained.session=::std::addressof(session); chained.proof=proof; chained.execute=true;
                                check(lib::llvm_jit_debug_resume_native_return_event_host_api(proof,::std::addressof(session),
                                    ::std::addressof(chained),return_event_probe::resume) && chained.committed,
                                    "parent really returns to its separately proved ancestor");
                                await_phase(session,dbg::native_step::phase::trapped);
                                check(state->control->external_park(ticket,site.participant,frames.location),
                                    "each chained return retains the original domain anchor");
                                lib::llvm_jit_debug_native_caller_view reached{};
                                check(lib::llvm_jit_debug_query_native_return_event_host_api(chained.plan,::std::addressof(session),reached) &&
                                    reached.valid && reached.incarnation==next.incarnation && reached.current_incarnation==next.current_incarnation,
                                    "exact kernel revision and genuine normal leave identify the ancestor"); ++chained_hits;
                                if(next.function==frames.frames[frames.frames.size()-hop-2u].function) { ++chained_recursive; }
                                auto& ancestor{chained_stops[hop]};
                                check(lib::llvm_jit_debug_capture_native_return_stop_host_api(chained.plan,::std::addressof(session),ancestor) &&
                                    ancestor.capture && ancestor.cursor && ancestor.capture.get()!=active_parent.capture.get() &&
                                    ancestor.cursor.get()!=active_parent.cursor.get(),
                                    "chained return publishes a distinct canonical ancestor capture"); ++chained_captures;
                                check(lib::llvm_jit_debug_capture_native_return_stop_host_api(chained.plan,::std::addressof(session),repeated) &&
                                    repeated.capture.get()==ancestor.capture.get() && repeated.cursor.get()==ancestor.cursor.get(),
                                    "repeated chained publication retains the same canonical owners");
                                lib::llvm_jit_debug_native_function_image ancestor_image{};
                                lib::llvm_jit_debug_native_code_bytes ancestor_window{};
                                lib::llvm_jit_debug_native_position ancestor_position{};
                                check(lib::llvm_jit_debug_native_activation_host_api(ancestor.cursor,::std::addressof(session)) &&
                                    lib::llvm_jit_debug_copy_native_function_host_api(ancestor.capture,::std::addressof(session),ancestor_image) &&
                                    lib::llvm_jit_debug_copy_native_code_host_api(ancestor.capture,::std::addressof(session),ancestor_window) &&
                                    lib::llvm_jit_debug_native_position_host_api(ancestor.capture,::std::addressof(session),ancestor_position) &&
                                    ancestor_image.module==next.module && ancestor_image.function==next.function &&
                                    ancestor_image.function_generation==next.function_generation && ancestor_image.runtime_epoch==next.runtime_epoch &&
                                    ancestor_window.pc==ancestor_image.stop_pc && ancestor_position.pc==ancestor_image.stop_pc,
                                    "each ancestor exposes only its actual owned Wasm code and provenance");
                                check(!lib::llvm_jit_debug_copy_native_function_host_api(active_parent.capture,::std::addressof(session),stale_image) &&
                                    !stale_image.size && !lib::llvm_jit_debug_native_activation_host_api(active_parent.cursor,::std::addressof(session)),
                                    "departed parent cannot expose recursive same-body ancestor code"); ++chained_refused;
                                check(!lib::llvm_jit_debug_query_activation_host_api(ancestor.capture,no_locals) && no_locals.frames.empty() &&
                                    !lib::llvm_jit_debug_query_source_activation_host_api(ancestor.capture,{},no_source) && !no_source.source_available,
                                    "chained native capture never promotes source locals"); ++chained_refused;
                                auto const& previous_plan{hop==0u ? executed.plan : chained_events[hop-1u].plan};
                                check(!lib::llvm_jit_debug_capture_native_return_stop_host_api(previous_plan,::std::addressof(session),repeated) &&
                                    !repeated.capture && !repeated.cursor,
                                    "old return event cannot remint after another physical return"); ++chained_refused;
                                active_parent=ancestor; retained_parent=ancestor;
                                if(hop+1u > maximum_chained_depth) { maximum_chained_depth=hop+1u; }
                                if(frames.frames.size()==hop+3u)
                                {
                                    lib::llvm_jit_debug_native_backtrace_view root_trace{};
                                    check(!lib::llvm_jit_debug_native_backtrace_host_api(ancestor.cursor,::std::addressof(session),root_trace) &&
                                        !root_trace.count && !lib::llvm_jit_debug_mint_native_return_continuation_host_api(
                                            ancestor.cursor,::std::addressof(session)),
                                        "actual Wasm root refuses host CFI and host return execution"); ++chained_root_refused;
                                    break;
                                }
                                lib::llvm_jit_debug_native_backtrace_view ancestor_trace{};
                                auto const expected_ancestors{frames.frames.size()-hop-3u};
                                check(lib::llvm_jit_debug_native_backtrace_host_api(ancestor.cursor,::std::addressof(session),ancestor_trace) &&
                                    ancestor_trace.count==(expected_ancestors<32u ? expected_ancestors : 32u) &&
                                    ancestor_trace.frames[0u].current_incarnation==next.incarnation,
                                    "each chained ancestor still has a genuine bounded Wasm backtrace");
                            }

                        }
                    }
                }
            }
            ++physical; if(parent.function == current.function) { ++recursive; }
            if(current.function == 0u && current.function_generation == 2u) { ++replacement_callers; }
        }
        else
        {
            check(!view.valid && !view.incarnation, "unproved caller clears every public field");
            return_resume_probe root{::std::addressof(session), return_proof};
            check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
                ::std::addressof(root), return_resume_probe::resume) && !root.called &&
                session.state.load(::std::memory_order_acquire) == dbg::native_step::phase::trapped,
                "unproved or host parent never enters return resume and retains the actual native trap"); ++resume_refused;
            if(frames.frames.size() == 1u) { ++host_refused; }
            ++return_refused;
        }
        lib::llvm_jit_debug_native_activation_cursor_owner alias{cursor, reinterpret_cast<lib::llvm_jit_debug_native_activation_cursor const*>(::std::uintptr_t{1u})};
        check(!lib::llvm_jit_debug_native_caller_host_api(alias, ::std::addressof(session), view) && !view.valid, "unreadable cursor alias is refused before dereference");
        lib::llvm_jit_debug_native_backtrace_view refused_trace{}; refused_trace.count = 32u; refused_trace.complete = true;
        check(!lib::llvm_jit_debug_native_backtrace_host_api(alias, ::std::addressof(session), refused_trace) &&
            !refused_trace.count && !refused_trace.complete, "multi-frame alias refuses without reading");
        check(!lib::llvm_jit_debug_native_caller_host_api(cursor, reinterpret_cast<void const*>(::std::uintptr_t{1u}), view) && !view.valid,
            "forged session comparison cannot read host state");
        check(!lib::llvm_jit_debug_mint_native_return_continuation_host_api(alias, ::std::addressof(session)),
            "unreadable cursor alias cannot mint a return capability");
        check(!lib::llvm_jit_debug_mint_native_return_continuation_host_api(cursor, reinterpret_cast<void const*>(::std::uintptr_t{1u})),
            "different session cannot mint a return capability");
        check(state->control->external_unpark(ticket, site.participant), "actual domain marker removal");
        check(!lib::llvm_jit_debug_native_return_continuation_host_api(return_proof, ::std::addressof(session), view) && !view.valid,
            "real return capability is unusable without actual external park ownership");
        return_resume_probe unparked{::std::addressof(session), return_proof};
        check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(return_proof, ::std::addressof(session),
            ::std::addressof(unparked), return_resume_probe::resume) && !unparked.called,
            "missing original external park ownership refuses return resume"); ++resume_refused;
        check(!lib::llvm_jit_debug_mint_native_return_continuation_host_api(cursor, ::std::addressof(session)),
            "missing domain ownership cannot mint a return capability");
        check(!lib::llvm_jit_debug_native_caller_host_api(cursor, ::std::addressof(session), view) && !view.valid, "missing domain ownership refuses even a real trap");
        check(!lib::llvm_jit_debug_native_backtrace_host_api(cursor, ::std::addressof(session), refused_trace) && !refused_trace.count,
            "multi-frame query requires actual domain ownership");
        executed.disable();
        for(auto& chained : chained_events) { chained.disable(); }
        dbg::native_step::release(session); await_phase(session, dbg::native_step::phase::released);
        check(dbg::native_step::clear(session), "real worker ACK before native owner retirement");
        check(!lib::llvm_jit_debug_query_native_return_event_host_api(executed.plan,::std::addressof(session),view) && !view.valid,
            "actual worker ACK revokes the event before descriptor close"); ++event_refused;
        if(retained_parent.capture)
        {
            lib::llvm_jit_debug_native_function_image expired{};
            check(!lib::llvm_jit_debug_native_activation_host_api(retained_parent.cursor,::std::addressof(session)) &&
                !lib::llvm_jit_debug_copy_native_function_host_api(retained_parent.capture,::std::addressof(session),expired) && !expired.size,
                "actual worker ACK revokes every retained parent capture"); ++parent_refused;
        }
        for(::std::size_t hop{}; hop!=chained_events.size(); ++hop)
        {
            auto& chained{chained_events[hop]}; auto const& stop{chained_stops[hop]};
            if(stop.capture)
            {
                lib::llvm_jit_debug_native_function_image expired{};
                check(!lib::llvm_jit_debug_copy_native_function_host_api(stop.capture,::std::addressof(session),expired) && !expired.size &&
                    !lib::llvm_jit_debug_query_native_return_event_host_api(chained.plan,::std::addressof(session),view) && !view.valid,
                    "actual worker ACK revokes every chained capture and event before descriptor close"); ++chained_refused;
            }
            chained.file.reset(); chained.plan={};
        }
        executed.file.reset(); executed.plan = {};
        retained = cursor;
        if(return_proof) { retained_return = return_proof; }
        check(!lib::llvm_jit_debug_native_return_continuation_host_api(retained_return, ::std::addressof(session), view) && !view.valid,
            "actual native worker ACK revokes the retained return proof");
        return_resume_probe acknowledged{::std::addressof(session), retained_return};
        check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(retained_return, ::std::addressof(session),
            ::std::addressof(acknowledged), return_resume_probe::resume) && !acknowledged.called,
            "actual worker ACK revokes retained return execution authority"); ++resume_refused;
        check(!lib::llvm_jit_debug_native_caller_host_api(retained, ::std::addressof(session), view) && !view.valid, "retired session refuses old caller");
        check(state->control->resume(ticket), "resume actual Wasm ticket after ACK");
    }
    guest.join();
    check(multiple != 0u && complete != 0u && (deep ? bounded != 0u && maximum_depth == 32u : maximum_depth >= 4u),
        "genuine complete and bounded multi-frame coverage required");
    check(result == expected_result && physical != 0u && recursive != 0u && host_refused != 0u && stale_refused != 0u && replacement_callers != 0u,
        "required genuine physical/recursive/host-denial coverage; refusals are not positive coverage");
    check(return_proofs == physical && return_proofs != 0u && return_refused != 0u && return_alias_refused == 3u * return_proofs &&
        return_stale_refused != 0u && return_advanced_refused != 0u && return_fresh_after_step != 0u, "positive sealed return capability coverage and genuine root/alias/stale refusals required");
    check(resume_committed == return_advanced_refused && resume_committed != 0u &&
        resume_retained == return_proofs && resume_rollback == return_proofs &&
        resume_reentry_refused == 2u * return_proofs + resume_committed && resume_refused != 0u,
        "real protected wake and retained/rollback/reentry coverage required");
    check(event_hits != 0u && (event_recursive != 0u || chained_recursive != 0u) && event_replaced != 0u && event_retained != 0u && event_rollback != 0u,
        "real parent return events including recursion and replacement required");
    check(parent_captures==event_hits && parent_backtraces==event_hits && parent_steps!=0u &&
        parent_steps+parent_step_refused==event_hits && parent_refused!=0u,
        "every real return must publish a parent, with positive actual parent SI and safe refusals");
    check(chained_hits!=0u && chained_captures==chained_hits && maximum_chained_depth==3u && chained_refused!=0u &&
        chained_recursive!=0u && chained_root_refused!=0u,
        "real multi-return ancestor publication and stale/source/ACK refusals required");
    lib::reset_runtime_state_host_api();
    lib::llvm_jit_debug_native_caller_view view{};
    check(!lib::llvm_jit_debug_native_caller_host_api(retained, ::std::addressof(session), view) && !view.valid, "scope exit/reset revokes retained stack identity");
    check(!lib::llvm_jit_debug_native_return_continuation_host_api(retained_return, ::std::addressof(session), view) && !view.valid,
        "runtime reset and worker-stack retirement revoke every retained return capability");
    return_resume_probe reset{::std::addressof(session), retained_return};
    check(!lib::llvm_jit_debug_resume_native_return_continuation_host_api(retained_return, ::std::addressof(session),
        ::std::addressof(reset), return_resume_probe::resume) && !reset.called,
        "actual runtime reset revokes retained return execution authority"); ++resume_refused;
    check(dbg::native_step::uninstall(), "exact native handler retirement");
    ::fast_io::io::println("debug_native_return_event_runtime: PASS policy=", policy, " scenario=", scenario, " physical=", physical,
        " recursive=", recursive, " host-refused=", host_refused, " stale-refused=", stale_refused, " boundary-refused=", boundary_refused,
        " replacement-callers=", replacement_callers, " multiple=", multiple, " complete=", complete,
        " bounded=", bounded, " maximum-depth=", maximum_depth, " result=", result, " return-proofs=", return_proofs, " return-refused=", return_refused,
        " return-alias-refused=", return_alias_refused, " return-stale-refused=", return_stale_refused,
        " return-advanced-refused=", return_advanced_refused, " return-fresh-after-step=", return_fresh_after_step,
        " resume-committed=", resume_committed, " resume-retained=", resume_retained, " resume-rollback=", resume_rollback,
        " resume-refused=", resume_refused, " resume-reentry-refused=", resume_reentry_refused,
        " event-hits=",event_hits," event-recursive=",event_recursive," event-replaced=",event_replaced,
        " event-retained=",event_retained," event-rollback=",event_rollback," event-refused=",event_refused,
        " parent-captures=",parent_captures," parent-backtraces=",parent_backtraces," parent-si=",parent_steps,
        " parent-step-refused=",parent_step_refused," parent-refused=",parent_refused,
        " chained-hits=",chained_hits," chained-captures=",chained_captures," chained-refused=",chained_refused,
        " maximum-chained-depth=",maximum_chained_depth," chained-root-refused=",chained_root_refused," chained-recursive=",chained_recursive,
        " public-native-stack-bytes=0 protected-contained-wake-qualified=true return-event-qualified=true parent-native-capture-qualified=true chained-return-qualified=true finish-qualified=false");
}
