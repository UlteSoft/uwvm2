// Genuine Core3 LLVM-full before-park -> canonical capture -> actual host gate
// -> complete cohort/N/publication -> bounded WASIp1 query/edit -> resume.
// No constructor bypass, fake paused scalar, native FD injection or WASI rewrite.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
namespace wasi_storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
using domain = threads::cooperative_pause_domain;
static void require(bool good, char const* message)
{
    if(!good) { ::fast_io::io::perrln("debug_wasip1_environment_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t ordinal{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    threads::cooperative_pause_location location{};
    bool requested{}, captured{}, finished{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        if(where.function != 4u || self.ordinal.fetch_add(1u, ::std::memory_order_relaxed) != 3u) { return; }
        auto ticket{self.control->request_pause()}; require(bool(ticket), "real pause requested");
        ::std::lock_guard lock{self.mutex}; self.ticket = ::std::move(ticket); self.location = where;
        self.requested = true; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        ::std::lock_guard lock{self.mutex}; require(self.requested && self.location == where && !self.captured, "actual same before-park episode");
        auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(actual.status == lib::llvm_jit_checkpoint_capture_status::captured && actual.capture, "actual typed Core3 capture with GC root");
        self.capture = ::std::move(actual.capture); self.captured = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 3) { return 64; }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(policy == "instruction" || policy == "unwind", "explicit independent stack strategy");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"wasi-debug", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // Borrow the real final owner only after all emplacement; no later growth.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"wasi-debug";
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    wasi_storage::wasip1_noinherit_system_environment = true;
    wasi_storage::wasip1_add_or_replace_environment.emplace(u8"UWVM_DEBUG_KEY", u8"old");
    wasi_storage::wasip1_force_args_is_set = true;
    wasi_storage::wasip1_force_argument_storage.emplace_back(u8"argv0");
    wasi_storage::wasip1_force_argument_storage.emplace_back(u8"before");
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok), "real initializer and original WASI environment");
    // Before ANY native entry/publication, the original WASIp1 renumber function
    // moves its own issued stdio descriptors to deliberately unsorted IDs. No
    // arbitrary native descriptor is injected and no original implementation changes.
    auto& actual_environment{wasi_storage::default_wasip1_env};
    namespace wasi_functions = ::uwvm2::imported::wasi::wasip1::func;
    namespace wasi_abi = ::uwvm2::imported::wasi::wasip1::abi;
    require(wasi_functions::fd_renumber_base(actual_environment, 0, 101) == wasi_abi::errno_t::esuccess &&
        wasi_functions::fd_renumber_base(actual_environment, 1, 17) == wasi_abi::errno_t::esuccess &&
        wasi_functions::fd_renumber_base(actual_environment, 2, 57) == wasi_abi::errno_t::esuccess, "original issued stdio moved to unordered guest FD IDs");
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok, "real debug session configured");
    require(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok, "genuine host gate and observation profile installed");
    require(lib::llvm_jit_prepare_debug_host_api(), "real fused validator/compiler publication");
    ::std::uint32_t result{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = 4u;
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result)); run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"wasi-debug", run);
        ::std::lock_guard lock{state->mutex}; state->finished = true; state->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state->mutex}; require(state->changed.wait_until(lock, deadline(), [&] { return state->captured || state->finished; }), "finite actual park event");
        require(state->captured && !state->finished && state->capture, "genuine native execution remains parked"); ticket = state->ticket;
    }
    require(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused, "whole actual domain parked");
    lib::llvm_jit_checkpoint_thread_capture_owner owners[]{state->capture};
    auto const query{[&](ws::request const& selected) { return lib::llvm_jit_debug_query_wasip1_state_host_api(ticket, owners, selected); }};
    ws::request selected{}; selected.operation = ws::action::arguments;
    auto before{query(selected)}; require(before.result == ws::status::ok && before.strings.size() == 2u && before.strings[1].bytes == u8"before", "original arguments copied from real owned backing");
    selected.operation = ws::action::insert_argument; selected.index = 1u; selected.value = u8"middle";
    require(query(selected).mutation_applied, "insert into real owned argument backing");
    selected = {}; selected.operation = ws::action::arguments;
    auto inserted{query(selected)};
    require(inserted.strings.size() == 3u && inserted.strings[1].bytes == u8"middle" && inserted.strings[2].bytes == u8"before", "insert preserves shifted original arguments");
    selected.operation = ws::action::remove_argument; selected.index = 1u;
    require(query(selected).mutation_applied, "remove inserted argument");
    selected = {}; selected.operation = ws::action::insert_argument; selected.index = 2u; selected.value = u8"tail";
    require(query(selected).mutation_applied, "append at current argc");
    selected = {}; selected.operation = ws::action::remove_argument; selected.index = 2u;
    require(query(selected).mutation_applied, "remove tail argument");
    auto absent{query(selected)}; require(absent.result == ws::status::entry_not_found && !absent.mutation_applied, "absent argument removal is transactional");
    selected = {}; selected.operation = ws::action::insert_argument; selected.index = 3u; selected.value = u8"gap";
    auto gap{query(selected)}; require(gap.result == ws::status::entry_not_found && !gap.mutation_applied, "insert cannot leave a gap");
    selected = {}; selected.operation = ws::action::replace_argument; selected.index = 1u; selected.value.resize(ws::maximum_text_bytes, u8'z');
    require(query(selected).mutation_applied, "maximum individual owned string prepared");
    selected = {}; selected.operation = ws::action::arguments;
    auto first_page{query(selected)};
    require(first_page.result == ws::status::ok && first_page.strings.size() == 1u && first_page.more && first_page.next == 1u,
        "total raw-byte page stops before oversized remaining row");
    selected.first = first_page.next;
    auto second_page{query(selected)};
    require(second_page.result == ws::status::ok && second_page.strings.size() == 1u && second_page.strings[0].bytes.size() == 4096u &&
        !second_page.more && second_page.next == 2u, "exact next index reads omitted maximum-sized row");
    selected = {}; selected.operation = ws::action::replace_argument; selected.index = 1u; selected.value = u8"before";
    require(query(selected).mutation_applied, "restore small fixture argument before later imports");
    selected = {};
    selected.operation = ws::action::environment;
    auto variables{query(selected)}; require(variables.result == ws::status::ok && variables.strings.size() == 1u && variables.strings[0].bytes == u8"UWVM_DEBUG_KEY=old", "original environment");
    selected.operation = ws::action::replace_argument; selected.index = 1u; selected.value = u8"after";
    auto edited_argument{query(selected)}; require(edited_argument.result == ws::status::ok && edited_argument.mutation_applied, "committed owned argument edit");
    selected = {}; selected.operation = ws::action::set_environment; selected.name = u8"UWVM_DEBUG_KEY"; selected.value = u8"new";
    auto edited_environment{query(selected)}; require(edited_environment.result == ws::status::ok && edited_environment.mutation_applied, "committed owned environment edit");
    selected = {}; selected.operation = ws::action::descriptors;
    auto descriptors{query(selected)}; require(descriptors.result == ws::status::ok && !descriptors.descriptors.empty(), "guest FD metadata without native handles");
    require(descriptors.descriptors.size() == 3u && descriptors.descriptors[0u].descriptor == 17u && descriptors.descriptors[1u].descriptor == 57u &&
        descriptors.descriptors[2u].descriptor == 101u, "actual unordered renumber map yields monotonic complete FD list");
    selected.count = 1u; selected.first = 0u;
    auto fd_page1{query(selected)}; require(fd_page1.result == ws::status::ok && fd_page1.descriptors.size() == 1u && fd_page1.descriptors[0u].descriptor == 17u &&
        fd_page1.more && fd_page1.next == 18u, "first sorted descriptor page");
    selected.first = fd_page1.next;
    auto fd_page2{query(selected)}; require(fd_page2.result == ws::status::ok && fd_page2.descriptors.size() == 1u && fd_page2.descriptors[0u].descriptor == 57u &&
        fd_page2.more && fd_page2.next == 58u, "second sorted descriptor page does not skip lower ID");
    selected.first = fd_page2.next;
    auto fd_page3{query(selected)}; require(fd_page3.result == ws::status::ok && fd_page3.descriptors.size() == 1u && fd_page3.descriptors[0u].descriptor == 101u &&
        !fd_page3.more && fd_page3.next == 102u, "last sorted descriptor page");
    auto const fd{descriptors.descriptors[0u]};
    selected = {}; selected.operation = ws::action::reduce_rights; selected.descriptor = fd.descriptor;
    selected.expected_base = fd.base_rights; selected.expected_inheriting = fd.inheriting_rights;
    selected.new_base = fd.base_rights; selected.new_inheriting = fd.inheriting_rights;
    auto unchanged{query(selected)}; require(unchanged.result == ws::status::ok && unchanged.mutation_applied, "same-subset transaction admitted");
    selected.expected_base ^= 1u;
    auto stale{query(selected)}; require(stale.result == ws::status::changed_descriptor && !stale.mutation_applied, "stale FD comparison declined without edit");
    // Issued stdio may initially carry all bits. Remove one bit first so
    // the following escalation always requests a capability absent now.
    selected.expected_base = fd.base_rights; selected.new_base = fd.base_rights & ~(UINT64_C(1) << 63u);
    auto narrowed{query(selected)}; require(narrowed.result == ws::status::ok && narrowed.mutation_applied, "remove one capability before escalation probe");
    selected.expected_base = selected.new_base; selected.new_base |= UINT64_C(1) << 63u;
    auto increased{query(selected)}; require(increased.result == ws::status::capability_increase && !increased.mutation_applied, "capability escalation declined");
    selected.new_base = 0u; selected.new_inheriting = 0u;
    auto reduced{query(selected)}; require(reduced.result == ws::status::ok && reduced.mutation_applied, "both rights safely reduced");
    selected = {}; selected.operation = ws::action::arguments;
    ::std::shared_ptr<void const> unrelated_owner{::std::make_shared<int const>(1)};
    lib::llvm_jit_checkpoint_thread_capture_owner forged{unrelated_owner, state->capture.get()};
    lib::llvm_jit_checkpoint_thread_capture_owner forged_owners[]{forged};
    auto denied{lib::llvm_jit_debug_query_wasip1_state_host_api(ticket, forged_owners, selected)};
    require(denied.result == ws::status::incomplete_cohort && denied.strings.empty() && !denied.mutation_applied, "same-address foreign shared owner is not capture authority");
    require(state->control->resume(ticket), "resume actual guest"); guest.join();
    require(result == 91u, "real original WASI import ABI read edited argv/env byte counts and bytes");
    selected.operation = ws::action::replace_argument; selected.index = 1u; selected.value = u8"wrong";
    auto retired{query(selected)}; require(retired.result != ws::status::ok && !retired.mutation_applied, "retired actual pause cannot edit");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("debug_wasip1_environment_runtime PASS actual typed query/edit; no native FD injection");
}
