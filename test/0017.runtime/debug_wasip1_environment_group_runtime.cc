// Two independently initialized WASIp1 environments, one authentic complete
// cooperative stop, private prepare-all/publish-all. No full-VM restore claim.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <fast_io_dsal/string_view.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace full = ::uwvm2::uwvm::runtime::full;
namespace threads = ::uwvm2::utils::thread;
namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
namespace storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
using domain = threads::cooperative_pause_domain;
static unsigned checks{};
static void require(bool valid, char const* message)
{
    ++checks;
    if(!valid)
    {
        ::fast_io::io::perrln("debug_wasip1_environment_group_runtime: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{};
    ::std::condition_variable changed{};
    ::std::size_t ordinal{}, main_module{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    bool finished{};
    bool worker_chain{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        if(where.code_unit!=self.main_module || where.function!=1u || self.ordinal++!=(self.worker_chain?3u:2u)) { return; }
        auto requested{self.control->request_pause()};
        ::std::lock_guard lock{self.mutex}; self.ticket=::std::move(requested); self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        ::std::lock_guard lock{self.mutex};
        auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        if(actual.status==lib::llvm_jit_checkpoint_capture_status::captured) { self.capture=::std::move(actual.capture); }
        self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc!=5 && argc!=6 && argc!=7) { return 64; }
    bool const worker_chain{argc==6 && ::fast_io::concat(::fast_io::mnp::os_c_str(argv[5]))=="worker-chain"};
    auto const policy{::fast_io::concat(::fast_io::mnp::os_c_str(argv[3]))};
    require(policy=="instruction" || policy=="unwind", "explicit stack policy");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=policy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u; mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false;features.explicit_enable_gc=true;
    features.disable_reference_types=false;features.explicit_enable_reference_types=true;
    features.disable_function_references=false;features.explicit_enable_function_references=true;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto const provider_path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[2])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"wasi-group-main",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"wasi-group-main";
    auto& preload{::uwvm2::uwvm::wasm::storage::preloaded_wasm.emplace_back()};
    preload.file_name=::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(provider_path.c_str())};
    preload.module_name=u8"wasi-group-provider";
    preload.wasm_parameter=::uwvm2::uwvm::wasm::storage::wasm_parameter;
    auto& shared_preload{::uwvm2::uwvm::wasm::storage::preloaded_wasm.emplace_back()};
    shared_preload.file_name=::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(provider_path.c_str())};
    shared_preload.module_name=u8"wasi-group-shared";shared_preload.wasm_parameter=::uwvm2::uwvm::wasm::storage::wasm_parameter;
    storage::wasip1_noinherit_system_environment=true;
    storage::wasip1_force_args_is_set=true;
    storage::wasip1_force_argument_storage.emplace_back(u8"main-before");
    storage::wasip1_add_or_replace_environment.emplace(u8"GROUP_KEY",u8"main-before");
    auto const override{storage::try_create_targetless_wasip1_module_override(u8"wasi-group-provider")};
    require(override!=nullptr,"real configured provider override");
    override->enabled_is_set=true; override->enabled=true;
    override->noinherit_system_environment_is_set=true; override->noinherit_system_environment=true;
    override->force_args_is_set=true; override->force_argument_storage.emplace_back(u8"provider-before");
    override->add_or_replace_environment.emplace(u8"GROUP_KEY",u8"provider-before");
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"original initializer owns both environments");
    auto const source{full::selected_full_source_owner_pin()};
    require(bool(source),"actual source owner");
    auto const state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::point,observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"actual debug session");
    require(lib::llvm_jit_configure_debug_value_observation_host_api({})==lib::llvm_jit_debug_configure_result::ok,"actual finite typed observation profile");
    require(lib::llvm_jit_prepare_debug_host_api(),"real fused validator/compiler");
    auto const main_id{source->assigned_main_module_id()};
    auto const found{source->registry().find(u8"wasi-group-provider")};
    require(found!=source->registry().end(),"actual provider member");
    auto const provider_id{source->bound_initialized_module_id(::std::addressof(found->second))};
    require(main_id<3u && provider_id<3u && main_id!=provider_id,"distinct actual validated module IDs");
    auto const shared_found{source->registry().find(u8"wasi-group-shared")};require(shared_found!=source->registry().end(),"shared-environment source member is genuinely loaded");
    auto const shared_id{source->bound_initialized_module_id(::std::addressof(shared_found->second))};require(shared_id<3u && shared_id!=main_id && shared_id!=provider_id,"third genuine module shares the main default environment");
    state->main_module=worker_chain?provider_id:main_id;
    state->worker_chain=worker_chain;
    // Complete the actual instantiation of unused preload members before
    // asking for a complete-world census; active data is consumed only here.
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasi-group-provider");
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasi-group-shared");
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasi-group-main");
    ::std::uint32_t result{};
    ::fast_io::native_thread guest{[&]() noexcept
    {
        lib::full_compile_run_config run{}; run.entry_function_index=worker_chain?2u:1u;
        run.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(result)); run.entry_abi_buffers.result_bytes=sizeof(result);
        lib::full_compile_and_run_main_module(u8"wasi-group-main",run);
        ::std::lock_guard lock{state->mutex}; state->finished=true; state->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock,deadline(),[&] { return bool(state->capture) || state->finished; }) && state->capture && !state->finished,"actual before-park capture");
        ticket=state->ticket;
    }
    require(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused,"complete actual cohort parked");
    lib::llvm_jit_checkpoint_thread_capture_owner owners[]{state->capture};
    auto const query{[&](::std::size_t module,ws::request requested)
    { requested.module=module; return lib::llvm_jit_debug_query_wasip1_state_host_api(ticket,owners,requested); }};
    ::std::array<::std::size_t,2u> ids{main_id,provider_id};
    ::std::array<::std::uint64_t,2u> files{},aliases{};
    for(::std::size_t n{};n!=ids.size();++n)
    {
        ws::request requested{}; requested.operation=ws::action::descriptors;
        auto rows{query(ids[n],requested)};
        require(rows.result==ws::status::ok && rows.descriptors.size()==3u && rows.shared_environment==(n==0u),"provider owns a distinct environment; main shares with the third module");
        for(auto const& row:rows.descriptors)
        {
            requested={}; requested.operation=ws::action::close_descriptor; requested.descriptor=row.descriptor;
            requested.expected_base=row.base_rights; requested.expected_inheriting=row.inheriting_rights;
            require(query(ids[n],requested).mutation_applied,"close genuine external descriptor through debugger host API");
        }
        requested={}; requested.operation=ws::action::create_file;
        requested.value.push_back(n==0u ? u8'A' : u8'C'); requested.value.push_back(u8'\0'); requested.value.push_back(n==0u ? u8'B' : u8'D');
        auto created{query(ids[n],requested)}; require(created.mutation_applied,"genuine anonymous managed file"); files[n]=created.affected_descriptor;
        requested={}; requested.operation=ws::action::descriptors; rows=query(ids[n],requested);
        require(rows.descriptors.size()==1u && rows.descriptors.front().managed_resource!=0u,"one owned managed resource");
        auto const row{rows.descriptors.front()}; requested={}; requested.operation=ws::action::duplicate_descriptor;
        requested.descriptor=row.descriptor; requested.expected_base=row.base_rights; requested.expected_inheriting=row.inheriting_rights;
        auto duplicate{query(ids[n],requested)}; require(duplicate.mutation_applied,"genuine same-environment FD alias"); aliases[n]=duplicate.affected_descriptor;
    }
    ::std::array<::std::byte,16u> label{}; label[0u]=::std::byte{0x72u};
    auto saved{lib::llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(ticket,owners,label,{})};
    ::fast_io::io::perrln("GROUP_JOINT_CAPTURE status=",static_cast<unsigned>(saved.status)," error=",static_cast<unsigned>(saved.data_error)," graph=",bool(saved.graph)," environments=",saved.wasip1_environments.size()," together=",saved.wasip1_captured_together);
    require(saved.status==lib::llvm_jit_checkpoint_instance_capture_status::captured && saved.graph && saved.wasip1_captured_together && saved.wasip1_environments.size()==2u,"joint capture includes two distinct environments");
    using status=lib::llvm_jit_wasip1_environment_capsule_status;
    lib::llvm_jit_checkpoint_prepare_request prepare{};prepare.recording_label=label;prepare.include_wasip1=true;
    auto prepared{lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,owners,prepare)};
    ::fast_io::io::perrln("PRIVATE_WASIP1_WORLD status=",static_cast<unsigned>(prepared.status),
        " environments=",prepared.wasip1_environments," modules=",prepared.prepared_wasip1_modules,
        " memories=",prepared.prepared_wasip1_memories," shared=",prepared.prepared_wasip1_shared_modules,
        " wasi_workers=",prepared.verified_wasip1_dispatch_workers,
        " wasi_visits=",prepared.verified_wasip1_dispatch_module_visits,
        " wasi_tls_restored=",prepared.wasip1_dispatch_tls_restored,
        " joint_workers=",prepared.enrolled_private_wasip1_workers,
        " joint_visits=",prepared.private_worker_wasip1_frame_visits,
        " joint_held=",prepared.private_wasip1_cohort_held,
        " joint_restored=",prepared.private_wasip1_tls_restored);
    require(prepared.prepared_frames==(worker_chain?2u:1u) && prepared.started_private_root_workers==1u &&
        prepared.joined_private_root_workers==1u && prepared.private_worker_roots_restored &&
        prepared.enrolled_private_debug_workers==1u && prepared.private_debug_cohort_held && prepared.private_debug_tls_restored &&
        prepared.prepared_private_wasip1_workers==1u && prepared.enrolled_private_wasip1_workers==1u &&
        prepared.private_worker_wasip1_frame_visits==(worker_chain?2u:1u) &&
        prepared.private_wasip1_cohort_held && prepared.private_wasip1_tls_restored,
        "actual root/debug worker holds leaf WASIp1 and validates every real caller context before joint TLS restoration");
    auto joint_worker_quota=prepare;joint_worker_quota.maximum_private_root_workers=0u;
    auto const joint_worker_denied{lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,owners,joint_worker_quota)};
    require(joint_worker_denied.status==lib::llvm_jit_checkpoint_prepare_status::worker_root_preparation_declined &&
        joint_worker_denied.started_private_root_workers==0u && joint_worker_denied.joined_private_root_workers==0u &&
        joint_worker_denied.prepared_private_wasip1_workers==0u && joint_worker_denied.enrolled_private_wasip1_workers==0u &&
        joint_worker_denied.private_worker_wasip1_frame_visits==0u &&
        !joint_worker_denied.private_wasip1_cohort_held && !joint_worker_denied.private_wasip1_tls_restored,
        "shared environment joint worker quota rejects all before actual launch");
    require(prepared.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
        prepared.wasip1_prepared_together && prepared.wasip1_installed_privately && prepared.wasip1_environments==2u,
        "two distinct WASIp1 environments install into an actual unpublished three-module world");
    require(prepared.modules==3u && prepared.engines==3u && prepared.prepared_wasip1_modules==3u &&
        prepared.prepared_wasip1_memories==3u && prepared.prepared_wasip1_shared_modules==1u,
        "private roster preserves two modules sharing default environment and separate module memories");
    require(prepared.wasip1_dispatch_prepared && prepared.prepared_wasip1_dispatch_contexts==3u &&
        prepared.prepared_wasip1_trace_bindings==0u,"final native dispatch cache preserves every genuine module and private environment alias");
    require(prepared.verified_wasip1_dispatch_workers==2u && prepared.verified_wasip1_dispatch_module_visits==6u &&
        prepared.wasip1_dispatch_tls_restored,"actual native workers simultaneously select shared environment with different module memories");
    for(auto const& capsule:saved.wasip1_environments)
    {
        auto data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(capsule)};
        require(data.status==status::captured && data.data.recording_label==label && data.data.managed_resources==1u && data.data.retained_external_resources==0u,"one coherent managed-only recording");
    }
    auto const environment{[&](::std::size_t n) -> auto& { return n==0u ? storage::default_wasip1_env : override->env; }};
    auto const file{[&](::std::size_t n) -> ::fast_io::native_file&
    {
        auto& env{environment(n)}; require(files[n]<env.fd_storage.opens.size(),"owned vector slot bounded");
        auto const slot{env.fd_storage.opens.index_unchecked(files[n]).fd_p};
        require(slot!=nullptr && slot->wasi_fd.ptr!=nullptr && slot->wasi_fd.ptr->checkpoint_managed_identity!=0u,"factory-issued resource only");
#if defined(_WIN32) && !defined(__CYGWIN__)
        return slot->wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file;
#else
        return slot->wasi_fd.ptr->wasi_fd_storage.storage.file_fd;
#endif
    }};
    auto const content{[&](::std::size_t n)
    {
        ::fast_io::u8string bytes{}; auto const size{::fast_io::status(file(n)).size}; require(size<16u,"bounded owned file content"); bytes.resize(size);
        if(!bytes.empty()) { auto const first{reinterpret_cast<::std::byte*>(bytes.data())}; lib::wasip1_native_file::read_content(file(n),first,first+bytes.size()); }
        return bytes;
    }};
    for(::std::size_t n{};n!=ids.size();++n)
    {
        constexpr ::std::array<::std::byte,4u> changed{::std::byte{'l'},::std::byte{'a'},::std::byte{'t'},::std::byte{'e'}};
        ::fast_io::operations::pwrite_all_bytes(file(n),changed.data(),changed.data()+changed.size(),0u);
        ws::request requested{}; requested.operation=ws::action::replace_argument; requested.value=u8"later";
        require(query(ids[n],requested).mutation_applied,"actual owned argument mutation");
        requested={}; requested.operation=ws::action::set_environment; requested.name=u8"GROUP_KEY"; requested.value=u8"later";
        require(query(ids[n],requested).mutation_applied,"actual owned environment mutation");
    }
    lib::llvm_jit_wasip1_environment_capsule_request different_recording{};
    different_recording.module=provider_id;different_recording.recording_label=label;different_recording.recording_label[0]^=::std::byte{1u};
    auto other_recording{lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,owners,different_recording)};
    require(other_recording.status==status::captured && other_recording.capsule,"genuine second recording retained");
    auto mixed_recordings{saved.wasip1_environments};
    for(auto& capsule:mixed_recordings)
    {
        auto data{lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(capsule)};
        if(data.data.module==provider_id) { capsule=other_recording.capsule; }
    }
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,mixed_recordings,true)==status::invalid_capsule_owner,
        "native group rejects mixed authentic recording labels before publication");
    auto const limit{override->env.fd_storage.fd_limit}; override->env.fd_storage.fd_limit=0u;
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,saved.wasip1_environments,true)==status::resource_limit,"second target policy failure rejects entire group");
    override->env.fd_storage.fd_limit=limit;
    for(::std::size_t n{};n!=ids.size();++n)
    {
        ws::request requested{}; auto current{query(ids[n],requested)};
        require(content(n)==u8"late" && current.strings.size()==1u && current.strings.front().bytes==u8"later","failed prepare publishes neither content nor text");
    }
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,saved.wasip1_environments,true)==status::restored,"two actual environments commit under one current proof");
    for(::std::size_t n{};n!=ids.size();++n)
    {
        auto bytes{content(n)};
        require(bytes.size()==3u && bytes[0]==(n==0u ? u8'A' : u8'C') && bytes[1]==0u && bytes[2]==(n==0u ? u8'B' : u8'D'),"both independent binary payloads restored");
        auto& env{environment(n)}; require(aliases[n]<env.fd_storage.opens.size(),"alias slot bounded");
        require(env.fd_storage.opens.index_unchecked(files[n]).fd_p->wasi_fd.ptr==env.fd_storage.opens.index_unchecked(aliases[n]).fd_p->wasi_fd.ptr,"per-environment genuine alias topology restored");
        ws::request requested{}; auto current{query(ids[n],requested)};
        auto const expected_argument{n==0u ? ::fast_io::u8string_view{u8"main-before"} : ::fast_io::u8string_view{u8"provider-before"}};
        require(current.strings.size()==1u && current.strings.front().bytes==expected_argument,"distinct argument vectors restored");
        requested.operation=ws::action::environment; current=query(ids[n],requested);
        auto const expected_environment{n==0u ? ::fast_io::u8string_view{u8"GROUP_KEY=main-before"} : ::fast_io::u8string_view{u8"GROUP_KEY=provider-before"}};
        require(current.strings.size()==1u && current.strings.front().bytes==expected_environment,"distinct environment vectors restored");
    }
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
    auto const artifact_root{::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[4])))};
    ::std::array<lib::llvm_jit_wasip1_environment_capsule_request,2u> exports{};
    for(::std::size_t n{};n!=ids.size();++n) { exports[n].module=ids[n];exports[n].recording_label=label;exports[n].portable_metadata_only=true; }
    auto const export_call=[&](auto const& rows)
    { return lib::llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(ticket,owners,rows); };
    auto batch=export_call(exports);
    require(batch.status==status::captured && batch.portable.environments.size()==2u && batch.observed_runtime_epoch!=0u && pp::valid_group(batch.portable),"one authentic transaction captures both environments");
    auto bad_exports=exports;bad_exports[1]=bad_exports[0];auto duplicate_export=export_call(bad_exports);
    require(duplicate_export.status==status::invalid_portable_snapshot && duplicate_export.portable.environments.empty(),"duplicate actual environment returns no partial export");
    bad_exports=exports;bad_exports[1].module=shared_id;auto alias_export=export_call(bad_exports);
    require(alias_export.status==status::invalid_portable_snapshot && alias_export.portable.environments.empty(),"distinct module IDs sharing one actual environment cannot export duplicate rows");
    bad_exports=exports;bad_exports[1].recording_label[1]=::std::byte{1u};auto mixed_export=export_call(bad_exports);
    require(mixed_export.status==status::invalid_portable_snapshot && mixed_export.portable.environments.empty(),"mixed-label export returns no partial data");
    bad_exports=exports;bad_exports[1].portable_metadata_only=false;
    require(export_call(bad_exports).status==status::invalid_portable_snapshot,"native capture cannot be mixed into portable export");
    bad_exports=exports;bad_exports[1].maximum_descriptors=0u;auto bounded_export=export_call(bad_exports);
    require(bounded_export.status==status::resource_limit && bounded_export.portable.environments.empty(),"later quota failure withholds earlier export");
    require(export_call(::std::span<lib::llvm_jit_wasip1_environment_capsule_request const>{}).status==status::invalid_portable_snapshot,"empty batch export rejected");
    ::std::vector<::std::byte> encoded{};require(pp::encode_group(batch.portable,encoded),"group codec includes independently checked nested snapshots");
    pp::group_snapshot decoded_group{};require(pp::decode_group(encoded,decoded_group) && decoded_group.environments.size()==2u,"group checksum and round trip");
    auto sentinel=decoded_group;auto reject_codec=[&](auto const& bytes)
    { require(!pp::decode_group(bytes,sentinel) && sentinel.environments.size()==2u && sentinel.environments.front().recording_label==label,"malformed group leaves caller output intact"); };
    auto corrupt=encoded;corrupt.back()^=::std::byte{1u};reject_codec(corrupt);
    for(::std::size_t extent{};extent!=212u;++extent) { reject_codec(::std::span<::std::byte const>{encoded.data(),extent}); }
    auto resign=[](auto& bytes) { auto hash=pp::detail::digest(::std::span<::std::byte const>{bytes.data(),bytes.size()-32u});::std::copy(hash.begin(),hash.end(),bytes.end()-32u); };
    for(::std::size_t byte:{0u,8u,12u,16u,20u}) { corrupt=encoded;corrupt[byte]=::std::byte{0xffu};resign(corrupt);reject_codec(corrupt); }
    corrupt=encoded;corrupt.insert(corrupt.end()-32u,::std::byte{});resign(corrupt);reject_codec(corrupt);
    auto mixed=decoded_group;mixed.environments.back().recording_label[0]^=::std::byte{1u};
    require(!pp::encode_group(mixed,corrupt),"codec refuses mixed-label environments");
    auto group_path=::fast_io::u8concat_fast_io(pp::text_view{artifact_root.data(),artifact_root.size()},u8"/group.uwpg");
    require(pp::save_group_file(batch.portable,pp::text_view{group_path.data(),group_path.size()}),"FastIO writes one bounded portable group file");
    pp::group_snapshot loaded_group{};
    require(pp::load_group_file(pp::text_view{group_path.data(),group_path.size()},loaded_group),"OS-native group file read");
    if(argc==6 && !worker_chain)
    { auto foreign=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[5])));require(pp::load_group_file(pp::text_view{foreign.data(),foreign.size()},loaded_group),"actual source OS group file decoded on target OS"); }
    ws::request grammar{};
    require(ws::parse("set wasip1 export-group 2f746d702f67726f7570 0 1",grammar) && grammar.environments.size()==2u,"console parses group export");
    require(ws::parse("set wasip1 import-group 2f746d702f67726f7570 0:0=2 1:0=2",grammar) && grammar.environments.back().rebindings==u8"0=2","console parses per-environment bindings");
    for(auto command:{"set wasip1 export-group 2f 0 0","set wasip1 export-group 2f 0:0=2","set wasip1 import-group 2f 0:","set wasip1 import-group 2f true","set wasip1 export-group - 0","set wasip1 import-group 2f 0:0=2:1"})
    { require(!ws::parse(::fast_io::string_view{::fast_io::mnp::os_c_str(command)},grammar),"console rejects invalid group grammar"); }
    ::std::array<lib::llvm_jit_wasip1_environment_capsule_request,2u> imports{};
    for(::std::size_t n{};n!=ids.size();++n)
    {
        lib::llvm_jit_wasip1_environment_capsule_request exported{};
        exported.module=ids[n];exported.recording_label=label;exported.portable_metadata_only=true;
        auto metadata{lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,owners,exported)};
        require(metadata.status==status::captured && metadata.portable && !metadata.capsule,"real portable metadata for each distinct environment");
        auto path{::fast_io::u8concat_fast_io(::fast_io::u8string_view{artifact_root.data(),artifact_root.size()},
            n==0u ? pp::text_view{u8"/group-main.uwp"} : pp::text_view{u8"/group-provider.uwp"})};
        require(pp::save_file(*metadata.portable,pp::text_view{path.data(),path.size()}),"OS-native FastIO persists detached environment metadata");
        if(argc==7) { path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[5+n]))); }
        auto decoded{::std::make_shared<pp::snapshot>()};
        require(pp::load_file(pp::text_view{path.data(),path.size()},*decoded),"persistent metadata loaded in target OS");
        if(argc==6 && !worker_chain) { *decoded=loaded_group.environments[n]; }
        require(decoded->resources.size()==1u && decoded->resources.front().type==pp::kind::external && decoded->bindings.size()==2u,
            "anonymous file aliases require explicit target binding");
        imports[n].module=ids[n];imports[n].recording_label=decoded->recording_label;imports[n].portable_restore=decoded;
        imports[n].portable_rebindings.push_back({0u,static_cast<::std::uint32_t>(files[n])});
        ws::request requested{};requested.operation=ws::action::replace_argument;requested.value=u8"portable-later";
        require(query(ids[n],requested).mutation_applied,"target argument differs from persistent metadata");
        requested={};requested.operation=ws::action::set_environment;requested.name=u8"GROUP_KEY";requested.value=u8"portable-later";
        require(query(ids[n],requested).mutation_applied,"target environment differs from persistent metadata");
        constexpr ::std::array<::std::byte,4u> later{::std::byte{'l'},::std::byte{'a'},::std::byte{'t'},::std::byte{'e'}};
        ::fast_io::operations::pwrite_all_bytes(file(n),later.data(),later.data()+later.size(),0u);
        ::fast_io::operations::io_stream_seek_bytes(file(n),0u,::fast_io::seekdir::beg);
    }
    auto const portable_call{[&](auto const& rows)
    { return lib::llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(ticket,owners,rows); }};
    auto unchanged=[&]
    {
        for(::std::size_t n{};n!=ids.size();++n)
        {
            ws::request requested{};auto text{query(ids[n],requested)};
            require(text.result==ws::status::ok && text.strings.size()==1u && text.strings.front().bytes==u8"portable-later" && content(n)==u8"late",
                "failed group leaves every target text and file content unchanged");
            requested.operation=ws::action::environment;auto variables{query(ids[n],requested)};
            require(variables.strings.size()==1u && variables.strings.front().bytes==u8"GROUP_KEY=portable-later",
                "failed group leaves every target environment unchanged");
        }
    };
    require(portable_call(::std::span<lib::llvm_jit_wasip1_environment_capsule_request const>{}).status==status::invalid_portable_snapshot,"empty group rejected");
    auto bad=imports;bad[1].portable_restore.reset();
    require(portable_call(bad).status==status::invalid_portable_snapshot,"every request must contain import metadata");
    bad=imports;bad[1].portable_metadata_only=true;
    require(portable_call(bad).status==status::invalid_portable_snapshot,"export requests cannot be smuggled into import group");
    bad=imports;auto different_label=::std::make_shared<pp::snapshot>(*bad[1].portable_restore);
    different_label->recording_label[0]^=::std::byte{1u};bad[1].portable_restore=different_label;bad[1].recording_label=different_label->recording_label;
    require(portable_call(bad).status==status::invalid_portable_snapshot,"mixed recordings rejected before preparation");
    bad=imports;bad[1]=bad[0];auto duplicate=portable_call(bad);
    require(duplicate.status==status::invalid_portable_snapshot && duplicate.diagnostic==u8"request=1 duplicate target environment","one environment cannot be published twice");unchanged();
    override->env.fd_storage.fd_limit=0u;auto no_capacity=portable_call(imports);override->env.fd_storage.fd_limit=limit;
    require(no_capacity.status==status::resource_limit && ::fast_io::u8string_view{no_capacity.diagnostic.data(),no_capacity.diagnostic.size()}.starts_with(u8"request=1 "),
        "later target quota rejects earlier prepared environment with request diagnostic");unchanged();
    auto before_scan_group=export_call(exports);::std::vector<::std::byte> before_scan_wire{};
    require(before_scan_group.status==status::captured && pp::encode_group(before_scan_group.portable,before_scan_wire),
        "both actual target environments checkpoint before oversized later import");
    bad=imports;auto oversized_scan=::std::make_shared<pp::snapshot>(*bad[1].portable_restore);
    oversized_scan->opens_size=pp::max_rows;oversized_scan->closed.clear();
    for(::std::uint32_t n{};n!=pp::max_rows;++n)
    {
        bool occupied{};for(auto const& b:oversized_scan->bindings) { occupied=occupied || b.descriptor==n; }
        for(auto const& r:oversized_scan->reserved) { occupied=occupied || r.descriptor==n; }
        if(!occupied) { oversized_scan->closed.push_back(n); }
    }
    auto high_alias=oversized_scan->bindings.front();high_alias.descriptor=INT32_MAX;oversized_scan->bindings.push_back(high_alias);
    bad[1].portable_restore=oversized_scan;auto scan_refused=portable_call(bad);
    require(scan_refused.status==status::invalid_portable_snapshot &&
        ::fast_io::u8string_view{scan_refused.diagnostic.data(),scan_refused.diagnostic.size()}.starts_with(u8"request=1 "),
        "oversized later FD scan table rejects complete genuine native group before publication");unchanged();
    auto after_scan_group=export_call(exports);::std::vector<::std::byte> after_scan_wire{};
    require(after_scan_group.status==status::captured && pp::encode_group(after_scan_group.portable,after_scan_wire) && after_scan_wire==before_scan_wire,
        "both FD graphs rights cursors allocators argv/env remain identical and recapturable after oversized later failure");
    bad=imports;auto wrong_wasm=::std::make_shared<pp::snapshot>(*bad[1].portable_restore);wrong_wasm->original_wasm[0]^=::std::byte{1u};bad[1].portable_restore=wrong_wasm;
    require(portable_call(bad).status==status::stale_environment,"later original-Wasm mismatch rejects entire group");unchanged();
    bad=imports;bad[1].portable_rebindings.clear();
    require(portable_call(bad).status==status::missing_rebind,"later anonymous resource needs explicit actual target FD");unchanged();
    ::fast_io::operations::io_stream_seek_bytes(file(1u),1u,::fast_io::seekdir::beg);
    require(portable_call(imports).status==status::unsupported_resource,"anonymous target cursor cannot be silently changed");
    require(::fast_io::operations::io_stream_seek_bytes(file(1u),0u,::fast_io::seekdir::cur)==1u,"failed import preserves target cursor");
    ::fast_io::operations::io_stream_seek_bytes(file(1u),0u,::fast_io::seekdir::beg);unchanged();
    bad=imports;
    for(auto& request:bad)
    {
        auto oversized{::std::make_shared<pp::snapshot>(*request.portable_restore)};oversized->arguments.clear();
        pp::text large{};large.resize(4095u);::std::fill(large.begin(),large.end(),u8'x');
        for(unsigned n{};n!=160u;++n) { oversized->arguments.push_back(large); }
        require(pp::valid(*oversized),"each metadata graph individually bounded");request.portable_restore=oversized;
    }
    require(portable_call(bad).status==status::resource_limit,"aggregate text budget enforced before target allocations");unchanged();
    auto restored_portable=portable_call(imports);
    require(restored_portable.status==status::restored && restored_portable.observed_runtime_epoch!=0u &&
        !restored_portable.capsule && !restored_portable.portable,"one genuine stop commits complete portable environment group");
    for(::std::size_t n{};n!=ids.size();++n)
    {
        require(content(n)==u8"late","portable import excludes file-content rollback");
        auto& env{environment(n)};
        require(env.fd_storage.opens.index_unchecked(files[n]).fd_p->wasi_fd.ptr==env.fd_storage.opens.index_unchecked(aliases[n]).fd_p->wasi_fd.ptr,"portable import preserves explicit target alias topology");
        ws::request requested{};auto text{query(ids[n],requested)};
        require(text.strings.size()==1u && text.strings.front().bytes==(n==0u ? pp::text_view{u8"main-before"} : pp::text_view{u8"provider-before"}),"both imported argument owners restored");
        requested.operation=ws::action::environment;auto variables{query(ids[n],requested)};
        require(variables.strings.size()==1u && variables.strings.front().bytes==(n==0u ? pp::text_view{u8"GROUP_KEY=main-before"} : pp::text_view{u8"GROUP_KEY=provider-before"}),"both imported environment owners restored");
    }
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,saved.wasip1_environments,true)==status::restored,"same-process managed group separately restores bytes for actual guest continuation");
    require(files[0u]==2u,"original closed-slot allocator yields actual guest fd2");
    require(state->control->resume(ticket),"resume genuine guest"); guest.join();
    require(result==91u,"actual WASI read consumes restored main payload");
    require(lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(ticket,owners,saved.wasip1_environments,true)!=status::restored,"retired stop cannot publish");
    require(portable_call(imports).status!=status::restored,"stale stop cannot commit a portable group");
    auto retired_export=export_call(exports);require(retired_export.status!=status::captured && retired_export.portable.environments.empty(),"retired stop cannot issue group snapshots");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("debug_wasip1_environment_group_runtime PASS checks=",checks," policy=",::fast_io::mnp::os_c_str(argv[3]));
}
