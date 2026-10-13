// Genuine Core3 LLVM-full before-park -> canonical capture -> actual host gate
// -> complete cohort/N/publication -> bounded WASIp1 query/edit -> resume.
// No constructor bypass, fake paused scalar, native FD injection or WASI rewrite.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags.h>
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
inline ::std::atomic_uint checks{};
static void require(bool good, char const* message)
{
    ++checks;
    if(!good) { ::fast_io::io::perrln("debug_wasip1_checkpoint_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
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
        bool const first{where.function==7u && self.ordinal.fetch_add(1u,::std::memory_order_relaxed)==3u};
        bool const second{where.function==8u && where.offset==33u};
        if(!first && !second) { return; }
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
    require(lib::llvm_jit_configure_debug_value_observation_host_api({}) == lib::llvm_jit_debug_configure_result::ok, "genuine host gate and observation profile installed");
    require(lib::llvm_jit_prepare_debug_host_api(), "real fused validator/compiler publication");
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasi-debug");
    ::std::uint32_t result{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = 7u;
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
    for(auto const operation : {ws::action::portable_export_group, ws::action::portable_import_group})
    {
        ws::request rejected{}; rejected.operation = operation; rejected.name = u8"data-only-group.uwpg";
        rejected.environments.push_back(ws::environment_selection{});
        require(ws::valid(rejected), "group DATA request is syntactically valid");
        auto const declined{query(rejected)};
        require(declined.result == ws::status::invalid_request && !declined.mutation_applied &&
            !declined.checkpoint_operation && !declined.portable_group_operation,
            "DATA-only query rejects group checkpoint operations at an actual cooperative stop");
    }
    using capsule_status=lib::llvm_jit_wasip1_environment_capsule_status;
    lib::llvm_jit_wasip1_environment_capsule_request request{};request.recording_label[0u]=::std::byte{9u};
    auto const capture{[&](auto const& r) { return lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,owners,r); }};
    auto const restore{[&](auto const& saved,bool strict=false) {
        return lib::llvm_jit_checkpoint_restore_wasip1_environment_host_api(ticket,owners,saved,{0u,strict}); }};
    auto const fds{[&] { ws::request r{};r.operation=ws::action::descriptors;return query(r); }};
    auto const close_fd{[&](::std::uint64_t number) {
        auto rows{fds()};ws::request r{};r.operation=ws::action::close_descriptor;r.descriptor=number;
        for(auto const& row:rows.descriptors) if(row.descriptor==number) { r.expected_base=row.base_rights;r.expected_inheriting=row.inheriting_rights; }
        return query(r);
    }};
    auto const contents{[&](::std::uint64_t number) -> ::fast_io::native_file& {
        require(number<actual_environment.fd_storage.opens.size(),"test reads only genuine factory-issued vector FD");
        auto const p{actual_environment.fd_storage.opens.index_unchecked(number).fd_p};
        require(p!=nullptr && p->wasi_fd.ptr!=nullptr && p->wasi_fd.ptr->checkpoint_managed_identity!=0u,"authentic managed resource identity");
#if defined(_WIN32) && !defined(__CYGWIN__)
        return p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file;
#else
        return p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd;
#endif
    }};
    auto const read_contents{[&](::std::uint64_t number) {
        auto& file{contents(number)};::fast_io::u8string bytes{};auto size{::fast_io::status(file).size};
        require(size<1024u,"bounded test content");bytes.resize(size);
        if(!bytes.empty()) { auto* first{reinterpret_cast<::std::byte*>(bytes.data())};::uwvm2::runtime::lib::wasip1_native_file::read_content(file,first,first+bytes.size()); }
        return bytes;
    }};
    ws::request edit{};edit.operation=ws::action::create_file;edit.value.push_back(u8'A');edit.value.push_back(u8'\0');edit.value.push_back(u8'B');
    auto created{query(edit)};require(created.result==ws::status::ok && created.mutation_applied && created.affected_descriptor==3u,"anonymous binary managed file uses original closed-slot allocator");
    auto rows{fds()};auto const original{rows.descriptors.front()}; // Sorted FD3 then external 17/57/101/151.
    require(original.descriptor==3u && original.managed_resource!=0u && original.base_rights==0x60006eu,"managed identity and factory capability bounds");
    edit={};edit.operation=ws::action::duplicate_descriptor;edit.descriptor=3u;edit.expected_base=original.base_rights;edit.expected_inheriting=original.inheriting_rights;
    auto duplicate{query(edit)};require(duplicate.mutation_applied && duplicate.affected_descriptor==2u,"duplicate preserves authentic binding and rights");
    auto const a{actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr};
    auto const b{actual_environment.fd_storage.opens.index_unchecked(2u).fd_p->wasi_fd.ptr};
    require(a==b,"guest FD aliases share one actual managed resource");
    edit.expected_base^=1u;require(query(edit).result==ws::status::changed_descriptor,"stale compare cannot duplicate");
    auto saved{capture(request)};require(saved.status==capsule_status::captured && saved.capsule,"immutable environment snapshot captured at actual stop");
    auto data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(saved.capsule)};
    require(data.status==capsule_status::captured && data.data.managed_resources==1u && data.data.retained_external_resources==4u &&
        data.data.managed_content_bytes==3u && data.data.descriptors.size()==6u,"aliases count one payload and external resources are explicit");
    auto small=request;small.maximum_managed_file_bytes=2u;
    auto quota{capture(small)};require(quota.status==capsule_status::resource_limit && !quota.capsule,"per-file budget rejects before payload copy");
    small=request;small.maximum_managed_total_bytes=2u;quota=capture(small);
    require(quota.status==capsule_status::resource_limit && !quota.capsule,"total managed payload budget enforced");
    small=request;small.maximum_managed_total_bytes=67108865u;quota=capture(small);
    require(quota.status==capsule_status::resource_limit && !quota.capsule,"caller cannot lift native snapshot budget ceiling");
    ::std::array<::std::byte,16u> label{};label[0u]=::std::byte{13u};
    auto wasm_only{lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,owners,label,{})};
    require(wasm_only.status==lib::llvm_jit_checkpoint_instance_capture_status::captured && wasm_only.graph &&
        wasm_only.wasip1_checkpoint_required && !wasm_only.wasip1_captured_together && wasm_only.wasip1_environments.empty(),"Wasm-only checkpoint returns required reminder metadata");
    auto together{lib::llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(ticket,owners,label,{})};
    require(together.status==lib::llvm_jit_checkpoint_instance_capture_status::captured && together.graph &&
        together.wasip1_captured_together && together.wasip1_environments.size()==1u,"joint capture obtains graph and environment in ONE coherent proof");
    auto joined_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(together.wasip1_environments.front())};
    require(joined_data.data.recording_label==together.graph->recording_id &&
        joined_data.data.managed_content_bytes==3u,"joint capture shares recording identity and exact payload cut");
    together={};wasm_only={};
    lib::llvm_jit_checkpoint_prepare_request prepare{};prepare.recording_label=label;prepare.include_wasip1=true;
    auto const rehearse{[&](auto const& selected)
    { return lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,owners,selected); }};
    auto const original_binding{actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr};
    auto strict_prepare{rehearse(prepare)};
    require(strict_prepare.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined &&
        strict_prepare.wasip1_status==capsule_status::unsupported_resource && strict_prepare.wasip1_checkpoint_required &&
        !strict_prepare.wasip1_prepared_together && strict_prepare.engines==0u,"joint preparation refuses external resources by default");
    auto binding_prepare=prepare;binding_prepare.require_managed_wasip1_resources=false;
    auto prepared_binding{rehearse(binding_prepare)};
    ::fast_io::io::perrln("JOINT_PREPARATION status=",static_cast<unsigned>(prepared_binding.status),
        " resource=",prepared_binding.resource_diagnostic," engine=",prepared_binding.engine_diagnostic,
        " wasi=",static_cast<unsigned>(prepared_binding.wasip1_status)," functions=",prepared_binding.functions,
        " payload=",prepared_binding.native_payload_bytes,
        " wasi_workers=",prepared_binding.verified_wasip1_dispatch_workers,
        " wasi_visits=",prepared_binding.verified_wasip1_dispatch_module_visits,
        " wasi_tls_restored=",prepared_binding.wasip1_dispatch_tls_restored,
        " joint_workers=",prepared_binding.enrolled_private_wasip1_workers,
        " joint_visits=",prepared_binding.private_worker_wasip1_frame_visits,
        " joint_held=",prepared_binding.private_wasip1_cohort_held,
        " joint_restored=",prepared_binding.private_wasip1_tls_restored);
    require(prepared_binding.started_private_root_workers==1u && prepared_binding.joined_private_root_workers==1u &&
        prepared_binding.prepared_private_debug_workers==1u && prepared_binding.enrolled_private_debug_workers==1u &&
        prepared_binding.private_worker_cohort_held && prepared_binding.private_debug_cohort_held &&
        prepared_binding.private_worker_roots_restored && prepared_binding.private_debug_tls_restored &&
        prepared_binding.prepared_private_wasip1_workers==1u && prepared_binding.enrolled_private_wasip1_workers==1u &&
        prepared_binding.private_worker_wasip1_frame_visits==1u && prepared_binding.private_wasip1_cohort_held &&
        prepared_binding.private_wasip1_tls_restored,"one genuine worker holds roots, debug TLS and actual WASIp1 context together and restores all three");
    auto joint_worker_quota=binding_prepare;joint_worker_quota.maximum_private_root_workers=0u;
    auto const joint_worker_denied{rehearse(joint_worker_quota)};
    require(joint_worker_denied.status==lib::llvm_jit_checkpoint_prepare_status::worker_root_preparation_declined &&
        joint_worker_denied.engines==1u && joint_worker_denied.started_private_root_workers==0u &&
        joint_worker_denied.joined_private_root_workers==0u && joint_worker_denied.prepared_private_wasip1_workers==0u &&
        joint_worker_denied.enrolled_private_wasip1_workers==0u && joint_worker_denied.private_worker_wasip1_frame_visits==0u &&
        !joint_worker_denied.private_wasip1_cohort_held && !joint_worker_denied.private_wasip1_tls_restored,
        "joint worker quota declines before native launch and cannot borrow earlier diagnostic worker success");
    require(prepared_binding.wasip1_installed_privately && prepared_binding.prepared_wasip1_modules==1u &&
        prepared_binding.prepared_wasip1_memories==1u && prepared_binding.prepared_wasip1_shared_modules==0u,
        "candidate owns an installed private environment and exact new module memory binding");
    require(prepared_binding.wasip1_dispatch_prepared && prepared_binding.prepared_wasip1_dispatch_contexts==1u &&
        prepared_binding.prepared_wasip1_trace_bindings==0u && prepared_binding.verified_wasip1_dispatch_workers==2u &&
        prepared_binding.verified_wasip1_dispatch_module_visits==2u && prepared_binding.wasip1_dispatch_tls_restored,
        "final dispatch context verifies real worker TLS selection, removal and native join");
    require(prepared_binding.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
        prepared_binding.wasip1_prepared_together && prepared_binding.wasip1_environments==1u &&
        prepared_binding.wasip1_checkpoint_required && prepared_binding.modules==1u && prepared_binding.engines==1u &&
        prepared_binding.functions==2u,"real WASIp1 builtins and managed FD clones prepare with an unpublished Wasm engine");
    require(actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr==original_binding &&
        read_contents(3u).size()==3u && ::fast_io::operations::io_stream_seek_bytes(contents(3u),0,::fast_io::seekdir::cur)==0,
        "successful rehearsal never swaps live descriptor bindings, content or cursor");
    auto no_wasi=prepare;no_wasi.include_wasip1=false;
    auto wasm_prepared{rehearse(no_wasi)};
    require(wasm_prepared.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
        wasm_prepared.wasip1_checkpoint_required && !wasm_prepared.wasip1_prepared_together && wasm_prepared.wasip1_environments==0u,
        "Wasm-only rehearsal reports the omitted visible WASIp1 state");
    require(!wasm_prepared.wasip1_installed_privately && wasm_prepared.prepared_wasip1_modules==0u && wasm_prepared.prepared_wasip1_memories==0u,
        "omitted WASIp1 never claims private installation or module bindings");
    require(!wasm_prepared.wasip1_dispatch_prepared && wasm_prepared.prepared_wasip1_dispatch_contexts==0u &&
        wasm_prepared.prepared_wasip1_trace_bindings==0u && wasm_prepared.verified_wasip1_dispatch_workers==0u &&
        !wasm_prepared.wasip1_dispatch_tls_restored && wasm_prepared.prepared_private_wasip1_workers==0u &&
        wasm_prepared.enrolled_private_wasip1_workers==0u && wasm_prepared.private_worker_wasip1_frame_visits==0u &&
        !wasm_prepared.private_wasip1_cohort_held && !wasm_prepared.private_wasip1_tls_restored,
        "omitted WASIp1 never prepares native WASIp1 dispatch state");
    ::std::vector<lib::llvm_jit_wasip1_environment_capsule_owner> occupied{};
    for(unsigned n{};n!=15u;++n) { auto extra{capture(request)};require(extra.status==capsule_status::captured,"fill genuine capsule registry");occupied.push_back(::std::move(extra.capsule)); }
    auto denied_joint{lib::llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(ticket,owners,label,{})};
    require(denied_joint.status!=lib::llvm_jit_checkpoint_instance_capture_status::captured && !denied_joint.graph && denied_joint.wasip1_environments.empty(),
        "joint capture resource failure never publishes a partial graph/capsule pair");
    auto denied_prepare{rehearse(binding_prepare)};
    require(denied_prepare.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined &&
        denied_prepare.wasip1_status==capsule_status::registry_exhausted && !denied_prepare.wasip1_prepared_together && denied_prepare.engines==0u,
        "joint rehearsal diagnoses exhausted native capsule registry without a partial Wasm preparation");occupied.clear();
    // Test-owned writes model continued guest mutation. They touch only the
    // real factory-issued resource while this actual single guest is parked.
    ::std::byte changed[]{::std::byte{'x'},::std::byte{'y'},::std::byte{'z'},::std::byte{'!'}};
    auto& file{contents(3u)};::fast_io::operations::pwrite_all_bytes(file,changed,changed+4u,0);
    ::fast_io::operations::io_stream_seek_bytes(file,2,::fast_io::seekdir::beg);
#if defined(_WIN32) && !defined(__CYGWIN__)
    require(::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_flags_base(actual_environment,3,
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)==
        ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup,"actual Windows WASI rejects immutable handle flags");
#else
    int const old_flags{::uwvm2::runtime::lib::wasip1_native_file::flags(file)};
    ::uwvm2::runtime::lib::wasip1_native_file::set_flags(file,old_flags|O_APPEND);
    require((::uwvm2::runtime::lib::wasip1_native_file::flags(file)&O_APPEND)!=0,"actual managed flags changed");
#endif
    auto with_flags{capture(request)};
    require(with_flags.status==capsule_status::captured && with_flags.capsule,"nonzero cursor and native flag snapshot captured");
    require(::fast_io::operations::io_stream_seek_bytes(file,0,::fast_io::seekdir::cur)==2,"capture itself leaves the live cursor unchanged");
    require(restore(with_flags.capsule)==capsule_status::restored,"native cursor and flags snapshot materializes");
    require(read_contents(3u)==u8"xyz!" && ::fast_io::operations::io_stream_seek_bytes(contents(3u),0,::fast_io::seekdir::cur)==2,
        "materialized content and nonzero cursor survive diagnostic reads");
#if !defined(_WIN32) || defined(__CYGWIN__)
    require((::uwvm2::runtime::lib::wasip1_native_file::flags(contents(3u))&O_APPEND)!=0,"saved append flag restored after payload writes");
#endif
    with_flags.capsule.reset();
    edit={};edit.operation=ws::action::replace_argument;edit.index=1u;edit.value=u8"different";require(query(edit).mutation_applied,"live argv changed after checkpoint");
    edit={};edit.operation=ws::action::set_environment;edit.name=u8"UWVM_DEBUG_KEY";edit.value=u8"different";require(query(edit).mutation_applied,"live env changed after checkpoint");
    auto const strict_status{restore(saved.capsule,true)};
    require(strict_status==capsule_status::unsupported_resource,"strict restore rejects external resources");
    require(read_contents(3u)==u8"xyz!","strict restore preserves mutated payload");
    auto const fd_limit{actual_environment.fd_storage.fd_limit};actual_environment.fd_storage.fd_limit=0u;
    require(restore(saved.capsule)==capsule_status::resource_limit && read_contents(3u)==u8"xyz!","policy limit failure leaves both descriptor table and content unchanged");
    actual_environment.fd_storage.fd_limit=fd_limit;
    require(restore(saved.capsule)==capsule_status::restored,"binding-mode restore commits managed files and text together");
    auto restored_bytes{read_contents(3u)};require(restored_bytes.size()==3u && restored_bytes[0]==u8'A' && restored_bytes[1]==0u && restored_bytes[2]==u8'B',"binary managed content restored exactly");
    require(::fast_io::operations::io_stream_seek_bytes(contents(3u),0,::fast_io::seekdir::cur)==0,
        "managed cursor restored independently of original native duplicate");
#if defined(_WIN32) && !defined(__CYGWIN__)
    require(actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd.fdflags==
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t{},"Windows wrapper flags restored");
#else
    require((::uwvm2::runtime::lib::wasip1_native_file::flags(contents(3u))&O_APPEND)==0,"POSIX managed flags restored");
#endif
    require(actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr==
        actual_environment.fd_storage.opens.index_unchecked(2u).fd_p->wasi_fd.ptr,"alias topology survives materialization");
    edit={};auto args{query(edit)};require(args.strings[1u].bytes==u8"before","argv restored");
    edit.operation=ws::action::environment;auto env{query(edit)};require(env.strings.front().bytes==u8"UWVM_DEBUG_KEY=old","environment restored");
    require(close_fd(3u).mutation_applied && close_fd(2u).mutation_applied,"guest close drops both bindings while snapshot retains its resources");
    edit={};edit.operation=ws::action::create_file;edit.value=u8"replacement";
    auto reuse{query(edit)};require(reuse.mutation_applied && reuse.affected_descriptor==2u,"closed FD number reused for another resource");
    require(restore(saved.capsule)==capsule_status::restored && read_contents(2u).size()==3u,"restore replaces reused FD with saved authentic resource");
    edit={};edit.operation=ws::action::create_file;auto allocation{query(edit)};
    require(allocation.mutation_applied && allocation.affected_descriptor==1u,"snapshot restores original free-list allocation order");
    require(restore(saved.capsule)==capsule_status::restored,"repeat restore removes post-checkpoint resource");
    auto before_rights{fds()};edit={};edit.operation=ws::action::reduce_rights;edit.descriptor=3u;
    edit.expected_base=original.base_rights;edit.expected_inheriting=original.inheriting_rights;edit.new_base=0u;edit.new_inheriting=0u;
    require(query(edit).mutation_applied && restore(saved.capsule)==capsule_status::restored,"canonical snapshot restores original rights after guest reduction");
    for(auto number:{17u,57u,101u,151u}) { require(close_fd(number).mutation_applied,"close retained external guest FD through authentic debugger API"); }
    auto managed_only{capture(request)};require(managed_only.status==capsule_status::captured,"managed-only capsule captured");
    auto managed_data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(managed_only.capsule)};
    require(managed_data.data.managed_resources==1u && managed_data.data.retained_external_resources==0u,"strict resource census proves managed-only domain");
    auto prepared_managed{rehearse(prepare)};
    require(prepared_managed.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
        prepared_managed.wasip1_prepared_together && prepared_managed.wasip1_environments==1u,
        "strict joint preparation accepts genuine managed-only environment");
    require(prepared_managed.wasip1_installed_privately && prepared_managed.prepared_wasip1_modules==1u && prepared_managed.prepared_wasip1_memories==1u,
        "strict candidate privately installs managed files and text before engine and frame preparation");
    require(prepared_managed.wasip1_dispatch_prepared && prepared_managed.prepared_wasip1_dispatch_contexts==1u &&
        prepared_managed.prepared_wasip1_trace_bindings==0u && prepared_managed.verified_wasip1_dispatch_workers==2u &&
        prepared_managed.verified_wasip1_dispatch_module_visits==2u && prepared_managed.wasip1_dispatch_tls_restored,
        "strict managed environment verifies real native dispatch workers");
    auto worker_quota=prepare;worker_quota.maximum_private_wasip1_workers=1u;
    auto const worker_denied{rehearse(worker_quota)};
    require(worker_denied.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined &&
        worker_denied.wasip1_status==capsule_status::resource_limit && worker_denied.engines==0u &&
        !worker_denied.wasip1_dispatch_prepared && worker_denied.verified_wasip1_dispatch_workers==0u &&
        worker_denied.verified_wasip1_dispatch_module_visits==0u && !worker_denied.wasip1_dispatch_tls_restored,
        "private worker quota refuses before OS launch and publishes no verification success");
    // An external trace handle is not a guest FD. Strict admission must still
    // reject it; binding-only copies the existing handle, never this path.
    auto const old_trace_path{actual_environment.trace_wasip1_output_file_path_storage};
    auto const old_trace_group{actual_environment.trace_wasip1_group_name_storage};
    require(!actual_environment.trace_wasip1_output_file,"fixture starts without an external trace binding");
    actual_environment.trace_wasip1_output_file=::fast_io::u8native_file{::fast_io::io_dup,::fast_io::u8native_io_observer{contents(3u).native_handle()}};
    actual_environment.trace_wasip1_output_file_path_storage=u8"/uwvm-checkpoint-trace-must-not-be-reopened";
    actual_environment.trace_wasip1_group_name_storage=u8"checkpoint-trace-binding";
    auto const trace_handle{actual_environment.trace_wasip1_output_file.native_handle()};
    auto const trace_offset{::fast_io::operations::io_stream_seek_bytes(contents(3u),0,::fast_io::seekdir::cur)};
    auto const trace_strict{rehearse(prepare)};
    require(trace_strict.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined &&
        trace_strict.wasip1_status==capsule_status::unsupported_resource && !trace_strict.wasip1_dispatch_prepared &&
        trace_strict.prepared_wasip1_trace_bindings==0u,"strict rejects a trace binding even with a managed-only guest FD census");
    auto const trace_binding{rehearse(binding_prepare)};
    require(trace_binding.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
        trace_binding.wasip1_dispatch_prepared && trace_binding.prepared_wasip1_dispatch_contexts==1u &&
        trace_binding.prepared_wasip1_trace_bindings==1u && trace_binding.verified_wasip1_dispatch_workers==2u &&
        trace_binding.wasip1_dispatch_tls_restored,"binding-only trace context verifies real worker TLS selection");
    require(actual_environment.trace_wasip1_output_file.native_handle()==trace_handle &&
        actual_environment.trace_wasip1_output_file_path_storage==u8"/uwvm-checkpoint-trace-must-not-be-reopened" &&
        actual_environment.trace_wasip1_group_name_storage==u8"checkpoint-trace-binding" &&
        ::fast_io::operations::io_stream_seek_bytes(contents(3u),0,::fast_io::seekdir::cur)==trace_offset,
        "candidate disposal preserves the original trace handle, configuration and shared cursor");
    actual_environment.trace_wasip1_output_file.close();
    actual_environment.trace_wasip1_output_file_path_storage=old_trace_path;
    actual_environment.trace_wasip1_group_name_storage=old_trace_group;
    auto const old_resolver{actual_environment.wasip1_memory_resolver};
    using actual_environment_type=::std::remove_reference_t<decltype(actual_environment)>;
    static unsigned resolver_calls{};
    actual_environment.wasip1_memory_resolver=+[](actual_environment_type const*) noexcept
        -> decltype(actual_environment.wasip1_memory) { ++resolver_calls;return nullptr; };
    auto const declined_resolver{rehearse(prepare)};
    actual_environment.wasip1_memory_resolver=old_resolver;
    require(declined_resolver.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined &&
        declined_resolver.wasip1_status==capsule_status::unsupported_resource && resolver_calls==0u &&
        declined_resolver.engines==0u && declined_resolver.verified_wasip1_dispatch_workers==0u &&
        !declined_resolver.wasip1_dispatch_tls_restored,
        "custom memory resolver is refused without invocation or private worker success");
    auto const old_yield{actual_environment.wasip1_sched_yield_func_ptr};
    actual_environment.wasip1_sched_yield_func_ptr=+[]() noexcept { return wasi_abi::errno_t::esuccess; };
    auto declined_callback{rehearse(prepare)};
    actual_environment.wasip1_sched_yield_func_ptr=old_yield;
    require(declined_callback.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined &&
        declined_callback.wasip1_status==capsule_status::unsupported_resource && !declined_callback.wasip1_installed_privately &&
        !declined_callback.wasip1_prepared_together && declined_callback.engines==0u,
        "unowned callback cannot enter a new candidate world; original policy restored without callback invocation");
    auto tiny=prepare;tiny.maximum_native_payload_bytes=1u;
    auto early_failure{rehearse(tiny)};
    require(early_failure.status==lib::llvm_jit_checkpoint_prepare_status::resource_preparation_declined &&
        early_failure.wasip1_status==capsule_status::captured && !early_failure.wasip1_prepared_together,
        "Wasm resource failure rolls back already prepared WASIp1 clones");
    require(!early_failure.wasip1_installed_privately && early_failure.prepared_wasip1_modules==0u && !early_failure.wasip1_dispatch_prepared &&
        early_failure.prepared_wasip1_dispatch_contexts==0u && early_failure.prepared_wasip1_trace_bindings==0u &&
        early_failure.verified_wasip1_dispatch_workers==0u && early_failure.verified_wasip1_dispatch_module_visits==0u &&
        !early_failure.wasip1_dispatch_tls_restored,
        "early failure publishes no private installation success metadata");
    auto late=prepare;require(prepared_managed.native_payload_bytes>1u,"real joint preparation payload measured");
    late.maximum_private_root_frames=0u; // Stable late-stage injection after real frames/engines.
    auto late_failure{rehearse(late)};
    require(late_failure.status==lib::llvm_jit_checkpoint_prepare_status::root_preparation_declined &&
        late_failure.wasip1_status==capsule_status::captured && late_failure.modules==1u && !late_failure.wasip1_prepared_together,
        "late real Wasm root quota failure rolls back both candidate domains");
    require(!late_failure.wasip1_installed_privately && late_failure.prepared_wasip1_modules==0u && !late_failure.wasip1_dispatch_prepared &&
        late_failure.prepared_wasip1_dispatch_contexts==0u && late_failure.prepared_wasip1_trace_bindings==0u &&
        late_failure.verified_wasip1_dispatch_workers==0u && late_failure.verified_wasip1_dispatch_module_visits==0u &&
        !late_failure.wasip1_dispatch_tls_restored,
        "late failure destroys world-owned WASIp1 candidates and reports no successful installation");
    require(read_contents(3u).size()==3u && actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr==
        actual_environment.fd_storage.opens.index_unchecked(2u).fd_p->wasi_fd.ptr,
        "failed joint preparation preserves binary content and actual shared alias");
    for(unsigned n{};n!=18u;++n)
    { require(rehearse(prepare).status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded,
        "discarded rehearsal releases native capsule registry slots and candidate engines"); }
    ::fast_io::truncate(contents(3u),16777217u);quota=capture(request);
    require(quota.status==capsule_status::resource_limit && restore(managed_only.capsule,true)==capsule_status::restored,
        "oversized current file cannot capture but valid saved image remains restorable without copying current payload");
    // Real group preparation must be atomic even after both detached tables
    // and the shared anonymous-file clone were prepared successfully.
    ::fast_io::truncate(contents(3u),0u);
    constexpr ::std::array<::std::byte,5u> group_bytes{
        ::std::byte{'g'},::std::byte{'r'},::std::byte{'o'},::std::byte{'u'},::std::byte{'p'}};
    ::fast_io::operations::pwrite_all_bytes(contents(3u),group_bytes.data(),group_bytes.data()+group_bytes.size(),0u);
    lib::llvm_jit_wasip1_environment_capsule_owner duplicate_environments[]{managed_only.capsule,managed_only.capsule};
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,duplicate_environments,true)==
        capsule_status::invalid_capsule_owner && read_contents(3u)==u8"group",
        "duplicate target after private group preparation rejects with no resource/table publication");
    lib::llvm_jit_wasip1_environment_capsule_owner strict_external[]{saved.capsule};
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,strict_external,true)==
        capsule_status::unsupported_resource && read_contents(3u)==u8"group",
        "strict group refuses retained external resources without changing managed payload");
    lib::llvm_jit_wasip1_environment_capsule_owner strict_group[]{managed_only.capsule};
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,strict_group,true)==capsule_status::restored,
        "authentic current-cohort strict resource group commits");
    auto group_restored{read_contents(3u)};
    require(group_restored.size()==3u && group_restored[0]==u8'A' && group_restored[1]==0u && group_restored[2]==u8'B' &&
        actual_environment.fd_storage.opens.index_unchecked(3u).fd_p->wasi_fd.ptr==
        actual_environment.fd_storage.opens.index_unchecked(2u).fd_p->wasi_fd.ptr,
        "strict group restores binary content and genuine shared FD alias topology");
    ::std::shared_ptr<void const> unrelated{::std::make_shared<int>(1)};
    lib::llvm_jit_wasip1_environment_capsule_owner forged{unrelated,saved.capsule.get()};
    require(restore(forged)==capsule_status::invalid_capsule_owner,"same-address foreign capsule owner rejected");
    lib::llvm_jit_wasip1_environment_capsule_owner forged_group[]{managed_only.capsule,forged};
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,forged_group,true)==capsule_status::invalid_capsule_owner &&
        read_contents(3u)==group_restored,"one forged capsule prevents every group mutation");
    lib::llvm_jit_checkpoint_thread_capture_owner forged_capture{unrelated,state->capture.get()};
    lib::llvm_jit_checkpoint_thread_capture_owner forged_cohort[]{forged_capture};
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_host_api(ticket,forged_cohort,saved.capsule,{})==capsule_status::unavailable_capture,
        "foreign before-park owner cannot restore native environment");
    require(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,forged_cohort,prepare).status==
        lib::llvm_jit_checkpoint_prepare_status::unavailable_capture,"foreign capture owner cannot prepare a joint world");
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_host_api(ticket,owners,saved.capsule,{1u,false})!=capsule_status::restored,
        "wrong module cannot consume snapshot");
    require(restore(saved.capsule)==capsule_status::restored,"restore original external bindings before real guest continuation");
    // Original fixture observes these final owned edits, while added WASI reads
    // prove restored binary bytes and shared cursor through the real guest ABI.
    edit={};edit.operation=ws::action::replace_argument;edit.index=1u;edit.value=u8"after";require(query(edit).mutation_applied,"final guest argv");
    edit={};edit.operation=ws::action::set_environment;edit.name=u8"UWVM_DEBUG_KEY";edit.value=u8"new";require(query(edit).mutation_applied,"final guest environment");
    auto running_saved{capture(request)};require(running_saved.status==capsule_status::captured,"snapshot for restoration after real guest progress");
    {
        ::std::lock_guard lock{state->mutex};state->requested=false;state->captured=false;state->capture.reset();owners[0].reset();
    }
    require(state->control->resume(ticket),"resume actual guest to mutate file through WASI write");
    {
        ::std::unique_lock lock{state->mutex};require(state->changed.wait_until(lock,deadline(),[&] { return state->captured || state->finished; }),"second genuine before-park after actual WASI write");
        require(state->captured && !state->finished,"guest parked in generated helper after write returned");ticket=state->ticket;owners[0]=state->capture;
    }
    require(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused,"second current cooperative cohort");
    require(read_contents(3u)==u8"xyz!","actual resumed guest mutated managed file");
    require(restore(running_saved.capsule)==capsule_status::restored && read_contents(3u).size()==3u,
        "historical capsule restores at a fresh genuine stop of same generation");
    require(state->control->resume(ticket),"resume restored guest to execute original WASI reads");guest.join();
    require(result==91u,"real WASI read ABI consumes restored content and alias cursor");running_saved.capsule.reset();
    require(restore(saved.capsule)!=capsule_status::restored,"retired pause cannot restore");
    lib::reset_runtime_state_host_api();
    require(lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(saved.capsule).status==capsule_status::captured,"immutable metadata and resource pins survive reset without execution authority");
    saved.capsule.reset();managed_only.capsule.reset();
    ::fast_io::io::println("debug_wasip1_checkpoint_runtime PASS checks=",checks.load(::std::memory_order_relaxed)," policy=",::fast_io::mnp::os_c_str(argv[2]));
}
