// Genuine Core3 LLVM-full before-park -> canonical capture -> actual host gate
// -> complete cohort/N/publication -> bounded WASIp1 query/edit -> resume.
// No constructor bypass, fake paused scalar, native FD injection or WASI rewrite.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <cerrno>
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
// Match the ORIGINAL WASIp1 initializer's stdio ownership branch. Native
// io_dup already owns these resources; the capsule retains that genuine RC.
// Only the original fallback observer branch needs a new capsule duplicate.
#if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
inline constexpr ::std::size_t expected_stdio_capsule_duplicates{0u};
inline constexpr auto expected_stdio_kind{ws::descriptor_kind::native_file};
#else
inline constexpr ::std::size_t expected_stdio_capsule_duplicates{3u};
inline constexpr auto expected_stdio_kind{ws::descriptor_kind::native_file_observer};
#endif
static void require(bool good, char const* message)
{
    if(!good) { ::fast_io::io::perrln("debug_wasip1_environment_owned_capsule_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
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
        if(where.function != 5u || self.ordinal.fetch_add(1u, ::std::memory_order_relaxed) != 3u) { return; }
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
    if(argc != 4) { return 64; }
#if defined(__AVR__) || ((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) || defined(__MSDOS__) || defined(__DJGPP__) || (defined(__NEWLIB__) && !defined(__CYGWIN__)) || defined(_PICOLIBC__) || defined(__wasm__)
    // The original initializer cannot issue an owned preopen on these
    // configurations. Never count a borrowed-directory lifetime as this test.
    ::fast_io::io::println("debug_wasip1_environment_owned_capsule_runtime UNSUPPORTED original-owned-directory-provider");return 77;
#endif
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
    // Use the ORIGINAL mount/environment initializer to issue the directory
    // FD. Native handle injection and direct FD table fabrication are absent.
    namespace wasi_env=::uwvm2::imported::wasi::wasip1::environment;
    wasi_env::mount_dir_root_t mount{};mount.preload_dir=u8"capsule-root";
    mount.entry=::fast_io::dir_file{::fast_io::mnp::os_c_str(argv[3])};
    wasi_storage::default_wasip1_env.mount_dir_roots.push_back(::std::move(mount));
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
    require(actual_environment.fd_storage.opens.size()>3u && actual_environment.fd_storage.opens.index_unchecked(3u).fd_p!=nullptr,
        "original initializer issued actual preopened guest directory");
    auto const* original_slot{actual_environment.fd_storage.opens.index_unchecked(3u).fd_p};
    require(original_slot->wasi_fd.ptr!=nullptr && original_slot->wasi_fd.ptr->wasi_fd_storage.type==::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir,
        "genuine initialized preopen storage kind");
    auto const& original_chain{original_slot->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack};
    require(original_chain.size()==1u && original_chain.front_unchecked().ptr!=nullptr && !original_chain.front_unchecked().ptr->dir_stack.is_observer,
        "actual owned original directory entry before any native guest entry");
    ::fast_io::dir_io_observer original_directory{original_chain.front_unchecked().ptr->dir_stack.storage.file};
    require(wasi_functions::fd_renumber_base(actual_environment,3,151)==wasi_abi::errno_t::esuccess,
        "original WASI renumber moves owned preopen beyond stdio without duplicating its RC");
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok, "real debug session configured");
    require(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok, "genuine host gate and observation profile installed");
    require(lib::llvm_jit_prepare_debug_host_api(), "real fused validator/compiler publication");
    ::std::uint32_t result{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = 5u;
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
    using capsule_status=lib::llvm_jit_wasip1_environment_capsule_status;
    lib::llvm_jit_wasip1_environment_capsule_request native_request{};
    native_request.recording_label[0u]=::std::byte{7u};
    auto const capture_capsule{[&](lib::llvm_jit_wasip1_environment_capsule_request const& input)
    { return lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,owners,input); }};
    auto native_saved{capture_capsule(native_request)};
    require(native_saved.status==capsule_status::captured && native_saved.capsule,
        "real complete before-park/closedhost/N native environment capsule issued");
    auto original_capsule{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(native_saved.capsule)};
    require(original_capsule.status==capsule_status::captured && original_capsule.data.issuer_serial!=0u &&
        original_capsule.data.observed_runtime_epoch!=0u && original_capsule.data.module==0u &&
        original_capsule.data.arguments.size()==2u && original_capsule.data.arguments[1u]==u8"before" &&
        original_capsule.data.environment.size()==1u && original_capsule.data.environment[0u]==u8"UWVM_DEBUG_KEY=old" &&
        original_capsule.data.descriptors.size()==4u && original_capsule.data.distinct_native_bindings==4u &&
        original_capsule.data.duplicated_observers==expected_stdio_capsule_duplicates && original_capsule.data.captured_directory_entries==1u,
        "original initializer stdio ownership plus genuine owned directory RC captured");
    require(original_capsule.data.descriptors[0u].descriptor==17u && original_capsule.data.descriptors[1u].descriptor==57u &&
        original_capsule.data.descriptors[2u].descriptor==101u &&
        original_capsule.data.descriptors[0u].kind==expected_stdio_kind &&
        original_capsule.data.descriptors[1u].kind==expected_stdio_kind &&
        original_capsule.data.descriptors[2u].kind==expected_stdio_kind && original_capsule.data.descriptors[3u].descriptor==151u &&
        original_capsule.data.descriptors[3u].preopened && original_capsule.data.descriptors[3u].guest_preopen_name==u8"capsule-root" &&
        !original_capsule.data.persisted_resource_restore_available && !original_capsule.data.kernel_state_rollback_available &&
        !original_capsule.data.deterministic_external_replay_available,
        "metadata order and separate native-lifetime-only contract");
    bool actual_wasm_hash{},actual_builtin_hash{};
    for(auto byte:original_capsule.data.original_wasm) { actual_wasm_hash=actual_wasm_hash || byte!=::std::byte{}; }
    for(auto byte:original_capsule.data.builtin_interface) { actual_builtin_hash=actual_builtin_hash || byte!=::std::byte{}; }
    require(actual_wasm_hash && actual_builtin_hash,"actual owned source and real builtin tuple identity hashed");
    auto invalid_native_request{native_request};invalid_native_request.recording_label={};
    auto invalid_label{capture_capsule(invalid_native_request)};
    require(invalid_label.status==capsule_status::invalid_recording_label && !invalid_label.capsule,"empty DATA label cannot mint a capsule");
    invalid_native_request=native_request;invalid_native_request.module=UINT64_MAX;
    auto invalid_module{capture_capsule(invalid_native_request)};
    require(invalid_module.status==capsule_status::unavailable_environment && !invalid_module.capsule,"guest module bound before access");
    invalid_native_request=native_request;invalid_native_request.maximum_descriptors=0u;
    auto invalid_quota{capture_capsule(invalid_native_request)};
    require(invalid_quota.status==capsule_status::resource_limit && !invalid_quota.capsule,"actual descriptors exceed zero quota before native publication");
    invalid_native_request=native_request;invalid_native_request.maximum_owned_text_bytes=0u;
    auto invalid_text{capture_capsule(invalid_native_request)};
    require(invalid_text.status==capsule_status::unavailable_owned_text && !invalid_text.capsule,"owned text quota refused without partial capsule");
    ::std::shared_ptr<void const> capsule_alias_control{::std::make_shared<int const>(2)};
    lib::llvm_jit_wasip1_environment_capsule_owner fake_capsule{capsule_alias_control,native_saved.capsule.get()};
    auto fake_capsule_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(fake_capsule)};
    require(fake_capsule_data.status==capsule_status::invalid_capsule_owner && fake_capsule_data.data.descriptors.empty(),
        "same-address different owner refused before capsule payload access");
    original_capsule.data.arguments[1u]=u8"copied-DATA-only";original_capsule.data.issuer_serial=0u;
    auto unchanged_capsule{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(native_saved.capsule)};
    require(unchanged_capsule.status==capsule_status::captured && unchanged_capsule.data.arguments[1u]==u8"before" &&
        unchanged_capsule.data.issuer_serial!=0u,"copied DATA cannot edit the authentic native capsule");
    ::std::vector<lib::llvm_jit_wasip1_environment_capsule_owner> registry_owners{};
    ::std::uint64_t last_serial{unchanged_capsule.data.issuer_serial};
    for(unsigned n{};n!=15u;++n)
    {
        auto additional{capture_capsule(native_request)};require(additional.status==capsule_status::captured && additional.capsule,"bounded genuine native capsule registry slot");
        auto additional_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(additional.capsule)};
        require(additional_data.status==capsule_status::captured && additional_data.data.issuer_serial>last_serial,"native issuer serial monotonic within real proof");
        last_serial=additional_data.data.issuer_serial;registry_owners.push_back(::std::move(additional.capsule));
    }
    auto exhausted_capsule{capture_capsule(native_request)};
    require(exhausted_capsule.status==capsule_status::registry_exhausted && !exhausted_capsule.capsule,"native registry limit never publishes a partial owner");
    registry_owners.clear();
    auto reopened_registry{capture_capsule(native_request)};require(reopened_registry.status==capsule_status::captured && reopened_registry.capsule,"released native owners free registry slots");
    auto reopened_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(reopened_registry.capsule)};
    require(reopened_data.status==capsule_status::captured && reopened_data.data.issuer_serial>last_serial,"expired registry slot never reuses native issuer serial");
    reopened_registry.capsule.reset();
    ws::request selected{}; selected.operation = ws::action::arguments;
    auto before{query(selected)}; require(before.result == ws::status::ok && before.strings.size() == 2u && before.strings[1].bytes == u8"before", "original arguments copied from real owned backing");
    selected.operation = ws::action::replace_argument; selected.index = 1u; selected.value.resize(ws::maximum_text_bytes, u8'z');
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
    native_request.recording_label[0u]=::std::byte{8u};
    auto edited_capsule{capture_capsule(native_request)};require(edited_capsule.status==capsule_status::captured && edited_capsule.capsule,"second genuine native environment snapshot after edit");
    auto edited_capsule_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(edited_capsule.capsule)};
    auto historical_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(native_saved.capsule)};
    require(edited_capsule_data.status==capsule_status::captured && edited_capsule_data.data.arguments[1u]==u8"after" &&
        edited_capsule_data.data.environment[0u]==u8"UWVM_DEBUG_KEY=new" && historical_data.status==capsule_status::captured &&
        historical_data.data.arguments[1u]==u8"before" && historical_data.data.environment[0u]==u8"UWVM_DEBUG_KEY=old",
        "historical owning text immutable across actual live WASI environment edits");

    selected = {}; selected.operation = ws::action::descriptors;
    auto descriptors{query(selected)}; require(descriptors.result == ws::status::ok && !descriptors.descriptors.empty(), "guest FD metadata without native handles");
    require(descriptors.descriptors.size() == 4u && descriptors.descriptors[0u].descriptor == 17u && descriptors.descriptors[1u].descriptor == 57u &&
        descriptors.descriptors[2u].descriptor == 101u && descriptors.descriptors[3u].descriptor==151u, "actual unordered renumber map yields monotonic complete FD list");
    selected.count = 1u; selected.first = 0u;
    auto fd_page1{query(selected)}; require(fd_page1.result == ws::status::ok && fd_page1.descriptors.size() == 1u && fd_page1.descriptors[0u].descriptor == 17u &&
        fd_page1.more && fd_page1.next == 18u, "first sorted descriptor page");
    selected.first = fd_page1.next;
    auto fd_page2{query(selected)}; require(fd_page2.result == ws::status::ok && fd_page2.descriptors.size() == 1u && fd_page2.descriptors[0u].descriptor == 57u &&
        fd_page2.more && fd_page2.next == 58u, "second sorted descriptor page does not skip lower ID");
    selected.first = fd_page2.next;
    auto fd_page3{query(selected)}; require(fd_page3.result == ws::status::ok && fd_page3.descriptors.size() == 1u && fd_page3.descriptors[0u].descriptor == 101u &&
        fd_page3.more && fd_page3.next == 102u, "third sorted descriptor page before owned preopen");
    selected.first=fd_page3.next;
    auto fd_page4{query(selected)};require(fd_page4.result==ws::status::ok && fd_page4.descriptors.size()==1u &&
        fd_page4.descriptors[0u].descriptor==151u && !fd_page4.more && fd_page4.next==152u,"fourth sorted actual owned preopen page");
    auto const fd{descriptors.descriptors[0u]};
    selected = {}; selected.operation = ws::action::reduce_rights; selected.descriptor = fd.descriptor;
    selected.expected_base = fd.base_rights; selected.expected_inheriting = fd.inheriting_rights;
    selected.new_base = fd.base_rights; selected.new_inheriting = fd.inheriting_rights;
    auto unchanged{query(selected)}; require(unchanged.result == ws::status::ok && unchanged.mutation_applied, "same-subset transaction admitted");
    selected.expected_base ^= 1u;
    auto stale{query(selected)}; require(stale.result == ws::status::changed_descriptor && !stale.mutation_applied, "stale FD comparison declined without edit");
    // A descriptor may start with every bit; remove one before probing an
    // increase, so the probe always requests a currently absent capability.
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
    auto denied_capsule{lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,forged_owners,native_request)};
    require(denied_capsule.status==capsule_status::unavailable_capture && !denied_capsule.capsule,"forged capture owner cannot issue native resources");

    require(state->control->resume(ticket), "resume actual guest"); guest.join();
    require(result == 91u, "real original WASI import ABI read edited argv/env byte counts and bytes");
    require(::fast_io::status(original_directory).type==::fast_io::file_type::directory,
        "actual original directory handle remains alive after guest original fd_close through captured native RC");

    selected.operation = ws::action::replace_argument; selected.index = 1u; selected.value = u8"wrong";
    auto retired{query(selected)}; require(retired.result != ws::status::ok && !retired.mutation_applied, "retired actual pause cannot edit");
    auto retired_capsule{capture_capsule(native_request)};
    require(retired_capsule.status!=capsule_status::captured && !retired_capsule.capsule,"retired cooperative stop cannot issue another native capsule");
    lib::reset_runtime_state_host_api();
    auto post_reset{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(native_saved.capsule)};
    require(post_reset.status==capsule_status::captured && post_reset.data.arguments[1u]==u8"before" &&
        post_reset.data.issuer_serial==historical_data.data.issuer_serial,"native capsule metadata survives VM reset without old source/engine/cohort lifetime");

    native_saved.capsule.reset();edited_capsule.capsule.reset();
    bool closed_after_final_native_owner{};
    try { (void)::fast_io::status(original_directory); }
    catch(::fast_io::error const& error)
    {
        // A random I/O failure is NOT proof of final close. FastIO compares
        // POSIX EBADF across its genuine POSIX/Win32/NT native error domains;
        // EINTR/access/other status errors cannot satisfy this lifetime oracle.
        closed_after_final_native_owner=(error==::fast_io::freestanding::errc{EBADF});
    }
    require(closed_after_final_native_owner,"final native capsule owner release closes original owned directory after guest close");
    ::fast_io::io::println("debug_wasip1_environment_owned_capsule_runtime PASS actual owned directory survives guest close then final capsule release closes it; no resource restore/replay claim");
}
