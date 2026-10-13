// Genuine Core3 LLVM-full before-park -> canonical capture -> actual host gate
// -> complete cohort/N/publication -> bounded WASIp1 query/edit -> resume.
// No constructor bypass, fake paused scalar, native FD injection or WASI rewrite.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/uwvm/debugger/wasip1_calls.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_tell.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_seek.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_mount_identity.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h>
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
static unsigned checks{};
static void require(bool good, char const* message)
{
    ++checks; if(!good) { ::fast_io::io::perrln("debug_wasip1_portable_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
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
    ::std::uint64_t trace_cursor{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        // Diagnostic trace observes the actual generated import call/errno. It
        // grants no capture or environment mutation authority.
        ws::request read_trace{};read_trace.operation=ws::action::trace_read;read_trace.first=self.trace_cursor;read_trace.count=64u;
        auto traced=::uwvm2::uwvm::debugger::wasip1_calls::apply(read_trace);
        if(!traced.records.empty()) { ::uwvm2::uwvm::debugger::wasip1_calls::print(::fast_io::out(),traced);self.trace_cursor=traced.next; }
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
int main(int argc,char** argv)
{
    if(argc!=6 && argc!=7) { return 64; }
    bool const recording_label_before{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="recording-label-before"};
    bool const recording_label{recording_label_before || (argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="recording-label")};
    bool const resource_alias_before{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="resource-alias-before"};
    bool const resource_alias{resource_alias_before || (argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="resource-alias")};
    bool const fd_slot_before{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="fd-slot-before"};
    bool const fd_slot{fd_slot_before || (argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="fd-slot")};
    bool const sparse_reserved{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="sparse-reserved"};
    bool const root_choice_before{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="root-choice-before"};
    bool const root_choice{root_choice_before || (argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="root-choice")};
    bool const root_flags_before{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="root-flags-before"};
    bool const root_flags_fresh_before{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="root-flags-fresh-before"};
    bool const root_flags{root_choice || root_flags_before || root_flags_fresh_before || (argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="root-flags")};
    bool const root_alias{root_flags || (argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="root-alias")};
    bool const normalized_path{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="normalized-path"};
    bool const dsync_only{argc==7 && ::fast_io::string_view{argv[6],::fast_io::cstr_len(argv[6])}=="dsync"};
    if(argc==7 && !recording_label && !resource_alias && !fd_slot && !sparse_reserved && !root_alias && !normalized_path && !dsync_only) { return 64; }
#if defined(_WIN32) && !defined(__CYGWIN__)
    // Original Windows WASI path_open rejects DSYNC; the four-OS fdstat
    // test covers that explicit ENOTSUP contract.
    if(dsync_only) { ::fast_io::io::println("SKIP DSYNC-only native runtime case: target mode unsupported");return 77; }
#endif
    auto policy=::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]));
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    ::uwvm2::uwvm::wasm::storage::execute_wasm_mode=::uwvm2::uwvm::wasm::base::mode::debug_jit;
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
    // Set the original host configuration before its transactional initializer.
    // The scan-boundary test has one spare occupied FD slot, isolating scan policy.
    if(fd_slot) { wasi_storage::default_wasip1_env.fd_storage.fd_limit=65537u; }
    wasi_storage::wasip1_noinherit_system_environment = true;
    wasi_storage::wasip1_add_or_replace_environment.emplace(u8"UWVM_DEBUG_KEY", ::fast_io::string_view{argv[4],::fast_io::cstr_len(argv[4])}=="save" ? ::fast_io::u8string_view{u8"old"} : ::fast_io::u8string_view{u8"target"});
    wasi_storage::wasip1_force_args_is_set = true;
    wasi_storage::wasip1_force_argument_storage.emplace_back(u8"argv0");
    wasi_storage::wasip1_force_argument_storage.emplace_back(::fast_io::string_view{argv[4],::fast_io::cstr_len(argv[4])}=="save" ? ::fast_io::u8string_view{u8"before"} : ::fast_io::u8string_view{u8"target-before"});
    // Use the ORIGINAL mount/environment initializer to issue the directory
    // FD. Native handle injection and direct FD table fabrication are absent.
    namespace wasi_env=::uwvm2::imported::wasi::wasip1::environment;
    wasi_env::mount_dir_root_t mount{};mount.preload_dir=u8"portable-root";
    mount.entry=::fast_io::dir_file{::fast_io::mnp::os_c_str(argv[3])};
    wasi_storage::default_wasip1_env.mount_dir_roots.push_back(::std::move(mount));
    wasi_storage::default_wasip1_env.disable_utf8_check=::fast_io::string_view{argv[4],::fast_io::cstr_len(argv[4])}=="restore";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok), "real initializer and original WASI environment");

    auto& actual_environment=wasi_storage::default_wasip1_env;
    namespace wasi_functions=::uwvm2::imported::wasi::wasip1::func;
    namespace wasi_abi=::uwvm2::imported::wasi::wasip1::abi;
    require(wasi_functions::fd_renumber_base(actual_environment,3,151)==wasi_abi::errno_t::esuccess,"original preopen renumbered");
    if(normalized_path)
    {
        ::fast_io::dir_file directory{::fast_io::mnp::os_c_str(argv[3])};
        ::fast_io::native_mkdirat(::fast_io::at(directory),u8"nested");
    }
    auto target_file=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[3])),
        normalized_path ? ::fast_io::u8string_view{u8"/nested/state.bin"} : ::fast_io::u8string_view{u8"/state.bin"});
    bool source=::fast_io::string_view{argv[4],::fast_io::cstr_len(argv[4])}=="save";
    { ::fast_io::native_file file{target_file,::fast_io::open_mode::out|::fast_io::open_mode::creat|::fast_io::open_mode::trunc};
      ::fast_io::io::print(file,::fast_io::mnp::os_c_str(source ? "SOURCE-CONTENT-MUST-NOT-BE-SAVED" : "TARGET-CONTENT-UNCHANGED")); }
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok, "real debug session configured");
    require(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok, "genuine host gate and observation profile installed");
    require(lib::llvm_jit_prepare_debug_host_api(), "real fused validator/compiler publication");
    // The original owned-source initializer defers active segments until the
    // full fused compiler has published. Apply its real remaining stage.
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasi-debug");
    ws::request trace{};trace.operation=ws::action::trace_enable;
    (void)::uwvm2::uwvm::debugger::wasip1_calls::apply(trace);
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

    using cs=lib::llvm_jit_wasip1_environment_capsule_status;
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
    auto call=[&](auto const& input) { return lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,owners,input); };
    lib::llvm_jit_wasip1_environment_capsule_request request{};request.recording_label[0]=::std::byte{0x71};
    bool saving=::fast_io::string_view{argv[4],::fast_io::cstr_len(argv[4])}=="save";
    pp::text cp_path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[5])));
    ::std::shared_ptr<pp::snapshot const> snapshot{};
    auto restrict_mount=[&]
    {
        ws::request q{};q.operation=ws::action::descriptors;auto state=query(q);
        auto mount=::std::find_if(state.descriptors.begin(),state.descriptors.end(),[](auto const& fd) { return fd.descriptor==151u; });
        require(state.result==ws::status::ok && mount!=state.descriptors.end(),"query real mount inheriting rights");
        q.operation=ws::action::reduce_rights;q.descriptor=151u;
        q.expected_base=mount->base_rights;q.expected_inheriting=mount->inheriting_rights;
        q.new_base=static_cast<::std::uint64_t>(wasi_abi::rights_t::right_path_open) |
            (dsync_only ? static_cast<::std::uint64_t>(wasi_abi::rights_t::right_fd_datasync) : 0u);
        q.new_inheriting=mount->inheriting_rights & ~(root_choice ? static_cast<::std::uint64_t>(wasi_abi::rights_t::right_path_open) : 0u) & ~(dsync_only ? static_cast<::std::uint64_t>(wasi_abi::rights_t::right_fd_sync) : 0u);
        auto reduced=query(q);require(reduced.result==ws::status::ok && reduced.mutation_applied,"retain child capabilities with parent PATH_OPEN alone");
    };
    auto check_fd_quota=[&](pp::snapshot const& current)
    {
        require(!current.reserved.empty(),"real checkpoint includes reserved null-resource slots");
        auto capture=request;capture.portable_restore.reset();capture.maximum_descriptors=current.bindings.size();
        capture.portable_metadata_only=true;
        require(call(capture).status==cs::resource_limit,"portable quota charges live aliases and reserved slots");
        capture.portable_metadata_only=false;
        for(unsigned n{};n!=20u;++n)
        {
            auto denied=call(capture);
            require(denied.status==cs::resource_limit && !denied.capsule && !denied.portable,
                "native under-quota capture rejected without consuming capsule registry or issuing partial owner");
        }
        capture.maximum_descriptors=current.bindings.size()+current.reserved.size();
        auto exact=call(capture);
        require(exact.status==cs::captured && exact.capsule,"native exact occupied quota accepts reserved and sparse layout");
        auto data=lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(exact.capsule);
        require(data.status==cs::captured && data.data.descriptors.size()==current.bindings.size(),
            "reserved slots count toward quota without becoming live descriptor rows");
        exact.capsule.reset();
        capture.portable_metadata_only=true;auto after=call(capture);
        require(after.status==cs::captured && after.portable,"portable recapture succeeds after rejected and exact native captures");
        ::std::vector<::std::byte> before_wire{},after_wire{};
        require(pp::encode(current,before_wire) && pp::encode(*after.portable,after_wire) && before_wire==after_wire,
            "quota checks preserve complete FD graph rights flags cursor free list argv and env");
    };


    if(recording_label)
    {
        auto capture=request;capture.portable_metadata_only=true;
        auto saved=call(capture);::std::vector<::std::byte> baseline{},wire{};
        require(saved.status==cs::captured && saved.portable && pp::encode(*saved.portable,baseline),
            "genuine recording metadata captured under the complete current stop");
        auto unchanged=[&]
        {
            auto after=call(capture);wire.clear();
            require(after.status==cs::captured && after.portable && pp::encode(*after.portable,wire) && wire==baseline,
                "label check preserves full FD graph rights flags cursors allocator argv and env");
        };
        auto restore=request;restore.portable_restore=saved.portable;
        for(unsigned index{};index!=restore.recording_label.size();++index)
        {
            restore.recording_label=request.recording_label;restore.recording_label[index]^=::std::byte{0x80u};
            auto result=call(restore);
            if(recording_label_before)
            { require(result.status==cs::restored,"old single import accepts a mismatched request recording label"); }
            else
            {
                require(result.status==cs::invalid_portable_snapshot && !result.capsule && !result.portable &&
                    result.diagnostic==u8"recording label differs from import metadata",
                    "every mismatched request label byte rejected without an output owner");
            }
            unchanged();
        }
        restore.recording_label=request.recording_label;
        auto changed=::std::make_shared<pp::snapshot>(*saved.portable);changed->recording_label[15u]^=::std::byte{0x40u};
        ::std::vector<::std::byte> changed_wire{};pp::snapshot decoded{};
        require(pp::encode(*changed,changed_wire) && pp::decode(changed_wire,decoded) &&
            decoded.recording_label==changed->recording_label,"changed metadata label is structurally valid with its actual checksum");
        restore.portable_restore=::std::make_shared<pp::snapshot>(::std::move(decoded));
        auto result=call(restore);
        require(result.status==(recording_label_before ? cs::restored : cs::invalid_portable_snapshot),
            "snapshot label mismatch rejected even with a valid wire checksum");
        unchanged();
        restore.portable_restore=saved.portable;restore.recording_label={};
        require(call(restore).status==cs::invalid_recording_label,"zero request label keeps the existing rejection contract");unchanged();
        restore.recording_label=request.recording_label;
        require(call(restore).status==cs::restored,"matching recording accepted with the original source provider and stop proof");unchanged();
        require(lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket,
            ::std::span<lib::llvm_jit_checkpoint_thread_capture_owner const>{},restore).status==cs::unavailable_capture,
            "matching label alone never grants capture or restore authority");
        if(recording_label_before) { ::fast_io::io::println("RECORDING_LABEL_NEGATIVE verified mismatched labels accepted"); }
    }

    if(resource_alias)
    {
        // Source rows come from actual managed construction under this same
        // stop. Detached DATA cannot mint the target resource or capture proof.
        auto original=call(request);
        require(original.status==cs::captured && original.capsule,"actual native baseline before alias rebind test");
        ws::request edit{};
        require(ws::parse("set wasip1 file 0 410042",edit),"real console anonymous construction parses");
        auto first=query(edit),second=query(edit);
        require(first.result==ws::status::ok && first.mutation_applied &&
            second.result==ws::status::ok && second.mutation_applied &&
            first.affected_descriptor!=second.affected_descriptor,"two genuine independent anonymous files constructed");
        edit={};edit.operation=ws::action::duplicate_descriptor;edit.descriptor=first.affected_descriptor;edit.expected_base=0x60006eu;
        auto alias=query(edit);
        require(alias.result==ws::status::ok && alias.mutation_applied,"actual alias of first anonymous resource constructed");
        auto capture=request;capture.portable_metadata_only=true;
        auto captured=call(capture);::std::vector<::std::byte> before_wire{},after_wire{};
        require(captured.status==cs::captured && captured.portable && pp::encode(*captured.portable,before_wire),
            "genuine portable snapshot records independent anonymous resources and a shared binding");
        auto const& graph=*captured.portable;
        auto binding=[&](::std::uint64_t n)->pp::binding const&
        {
            auto b=::std::find_if(graph.bindings.begin(),graph.bindings.end(),[&](auto const& row) { return row.descriptor==n; });
            require(b!=graph.bindings.end(),"real anonymous binding appears in captured graph");return *b;
        };
        auto first_index=binding(first.affected_descriptor).resource_index;
        auto second_index=binding(second.affected_descriptor).resource_index;
        require(first_index!=second_index && binding(alias.affected_descriptor).resource_index==first_index &&
            graph.resources[first_index].type==pp::kind::external && graph.resources[second_index].type==pp::kind::external,
            "source has two independent external resources; its alias is one resource with two bindings");
        auto restore=request;restore.portable_restore=captured.portable;
        restore.portable_rebindings.push_back({first_index,static_cast<::std::uint32_t>(first.affected_descriptor)});
        restore.portable_rebindings.push_back({second_index,static_cast<::std::uint32_t>(first.affected_descriptor)});
        auto merged=call(restore);
        if(resource_alias_before)
        {
            require(merged.status==cs::restored,"old consumer accepts collapsing distinct resources onto one target");
            auto after=call(capture);
            require(after.status==cs::captured && after.portable &&
                after.portable->resources.size()+1u==graph.resources.size(),"old accepted rebind collapses the captured resource graph");
            ::fast_io::io::println("RESOURCE_ALIAS_NEGATIVE verified distinct resources collapsed");
        }
        else
        {
            require(merged.status==cs::unsupported_resource && !merged.capsule && !merged.portable,
                "different saved resources cannot bind the same target FD");
            auto after=call(capture);
            require(after.status==cs::captured && after.portable && pp::encode(*after.portable,after_wire) && before_wire==after_wire,
                "rejected same-FD rebind preserves full environment including flags cursors and allocator");
            restore.portable_rebindings[1].target_descriptor=alias.affected_descriptor;
            merged=call(restore);
            require(merged.status==cs::unsupported_resource && !merged.capsule && !merged.portable,
                "different target FD numbers sharing one owner also cannot merge saved resources");
            after=call(capture);after_wire.clear();
            require(after.status==cs::captured && after.portable && pp::encode(*after.portable,after_wire) && before_wire==after_wire,
                "rejected alias-FD rebind leaves the entire target graph unchanged");
            restore.portable_rebindings[1].target_descriptor=second.affected_descriptor;
            require(call(restore).status==cs::restored,"independent target owners accept the original alias graph");
            after=call(capture);after_wire.clear();
            require(after.status==cs::captured && after.portable && pp::encode(*after.portable,after_wire) && before_wire==after_wire,
                "valid rebind preserves both independent resources and the existing shared binding");
            auto owner=[&](::std::uint64_t n)->auto&
            {
                auto& table=actual_environment.fd_storage;
                if(n<table.opens.size()) { return table.opens.index_unchecked(n).fd_p->wasi_fd; }
                return table.renumber_map.find(static_cast<::std::int32_t>(n))->second.fd_p->wasi_fd;
            };
            auto file=[](auto const& r)->::fast_io::native_file&
            {
#if defined(_WIN32) && !defined(__CYGWIN__)
                return r.ptr->wasi_fd_storage.storage.file_fd.file;
#else
                return r.ptr->wasi_fd_storage.storage.file_fd;
#endif
            };
            require(owner(first.affected_descriptor).ptr!=owner(second.affected_descriptor).ptr &&
                owner(first.affected_descriptor).ptr==owner(alias.affected_descriptor).ptr,
                "published actual owners retain independence and the captured alias");
            ::fast_io::operations::io_stream_seek_bytes(file(owner(first.affected_descriptor)),7,::fast_io::seekdir::beg);
            require(::fast_io::operations::io_stream_seek_bytes(file(owner(alias.affected_descriptor)),0,::fast_io::seekdir::cur)==7 &&
                ::fast_io::operations::io_stream_seek_bytes(file(owner(second.affected_descriptor)),0,::fast_io::seekdir::cur)==0,
                "valid restore shares cursor only with the original alias, not the independent file");
            ::fast_io::operations::io_stream_seek_bytes(file(owner(first.affected_descriptor)),0,::fast_io::seekdir::beg);
        }
        require(lib::llvm_jit_checkpoint_restore_wasip1_environment_host_api(ticket,owners,original.capsule,{0u,false})==cs::restored,
            "genuine native restore retires test files and restores the original layout before guest regression");
    }
    if(saving)
    {
        restrict_mount();
        ws::request edit{};edit.operation=ws::action::duplicate_descriptor;edit.descriptor=91u;edit.expected_base=0x60006cu;
        auto duplicated=query(edit);require(duplicated.result==ws::status::ok && duplicated.mutation_applied,"genuine manager duplicates existing guest file");
        request.portable_metadata_only=true;auto saved=call(request);
        if(saved.status!=cs::captured) { ::fast_io::io::perrln("capture status=",static_cast<unsigned>(saved.status)," ",::fast_io::mnp::code_cvt(saved.diagnostic)); }
        require(saved.status==cs::captured && saved.portable && !saved.capsule,"metadata issued under real current proof without old native capsule");
        snapshot=saved.portable;require(pp::valid(*snapshot),"portable graph canonical");
        if(sparse_reserved)
        {
            require(snapshot->reserved.size()==1u && snapshot->reserved.front().descriptor==INT32_MAX &&
                snapshot->reserved.front().null_resource && snapshot->reserved.front().base==0u && snapshot->reserved.front().inheriting==0u,
                "actual guest moved the original Rdbg empty FD0 into sparse INT32_MAX without granting input rights");
        }

        if(root_alias)
        {
            unsigned roots{},followed{};
            for(auto const& r:snapshot->resources)
            { if(r.type==pp::kind::directory && r.path.empty()) { ++roots;followed+=r.follow; } }
            require(roots==(root_choice ? 5u : root_flags ? 4u : 3u) && followed==1u,"original path_open dot issued distinct root resources and preserved follow");
            if(root_flags)
            {
                auto binding=::std::find_if(snapshot->bindings.begin(),snapshot->bindings.end(),[](auto const& b) { return b.descriptor==171u; });
                require(binding!=snapshot->bindings.end() && snapshot->resources[binding->resource_index].flags==4u,
                    "original guest dot open captures NONBLOCK on its independent root");
            }
        }
        if(root_choice)
        {
            auto binding=::std::find_if(snapshot->bindings.begin(),snapshot->bindings.end(),[](auto const& b) { return b.descriptor==174u; });
            require(binding!=snapshot->bindings.end() && binding->base==0x2000u && binding->inheriting==0u &&
                snapshot->resources[binding->resource_index].flags==4u,
                "genuine higher root holds PATH_OPEN and NONBLOCK without inherited PATH_OPEN");
        }
        check_fd_quota(*snapshot);
        bool alias{};unsigned count{};
        for(auto const& b:snapshot->bindings)
        {
            if(snapshot->resources[b.resource_index].type==pp::kind::file)
            { auto const& r=snapshot->resources[b.resource_index];require(r.mount==u8"portable-root" && r.path==(normalized_path ? ::fast_io::u8string_view{u8"nested/state.bin"} : ::fast_io::u8string_view{u8"state.bin"}) && r.offset==128u && r.flags==(dsync_only ? 2u : 0u),"real path provenance, WASI flags, cursor");++count; }
        }
        alias=count==2u;require(alias,"shared file has two guest bindings");
        ::std::vector<::std::byte> bytes{};require(pp::encode(*snapshot,bytes),"serialize without native handles");
        constexpr char secret[]="SOURCE-CONTENT-MUST-NOT-BE-SAVED";
        auto first=reinterpret_cast<::std::byte const*>(secret);
        require(::std::search(bytes.begin(),bytes.end(),first,first+sizeof(secret)-1u)==bytes.end(),"file content excluded");
        require(pp::save_file(*snapshot,pp::text_view{cp_path.data(),cp_path.size()}),"persistent metadata saved with FastIO");
    }
    else
    {
        auto loaded=::std::make_shared<pp::snapshot>();require(pp::load_file(pp::text_view{cp_path.data(),cp_path.size()},*loaded),"read source-OS snapshot in fresh process");
        snapshot=loaded;request.recording_label=loaded->recording_label;request.portable_restore=loaded;
        if(root_flags_before || root_flags_fresh_before || root_choice_before)
        {
            restrict_mount();
            ws::request duplicate{};duplicate.operation=ws::action::duplicate_descriptor;duplicate.descriptor=91u;duplicate.expected_base=0x60006cu;
            auto alias=query(duplicate);
            require(alias.result==ws::status::ok && alias.mutation_applied && alias.affected_descriptor==3u,"negative restore probe creates the genuine guest resume alias");
            if(root_flags_fresh_before || root_choice_before)
            {
                ws::request close{};close.operation=ws::action::close_descriptor;close.descriptor=173u;close.expected_base=8u;
                auto closed=query(close);require(closed.result==ws::status::ok && closed.mutation_applied,"fresh-root negative probe retires the only held SET_FLAGS child");
            }
            auto capture=request;capture.portable_restore.reset();capture.portable_metadata_only=true;
            auto before=call(capture);::std::vector<::std::byte> before_wire{},after_wire{};
            require(before.status==cs::captured && before.portable && pp::encode(*before.portable,before_wire),"negative root restore has a genuine baseline environment");
            auto refused=call(request);
            require(refused.status==((root_flags_fresh_before || root_choice_before) ? cs::capability_denied : cs::incompatible_flags),
                "old root restore refuses a freshly authorized child or incorrectly requires parent SET_FLAGS");
            auto after=call(capture);
            require(after.status==cs::captured && after.portable && pp::encode(*after.portable,after_wire) && after_wire==before_wire,
                "old refused root restore leaves complete target environment and native flags unchanged");
            ::fast_io::io::println(::fast_io::mnp::os_c_str(root_choice_before ? "ROOT_CHOICE_NEGATIVE verified status=" : root_flags_fresh_before ? "ROOT_FLAGS_FRESH_NEGATIVE verified status=" : "ROOT_FLAGS_NEGATIVE verified status="),static_cast<unsigned>(refused.status));
        }
        else
        {
        ws::request fd_query{};fd_query.operation=ws::action::descriptors;auto current=query(fd_query);
        require(current.result==ws::status::ok,"query actual target aliases under current proof");
        for(auto descriptor:{151u,1u})
        {
            auto original=::std::find_if(current.descriptors.begin(),current.descriptors.end(),[&](auto const& row) { return row.descriptor==descriptor; });
            require(original!=current.descriptors.end(),"target original mount/stdio FD exists");
            ws::request duplicate{};duplicate.operation=ws::action::duplicate_descriptor;duplicate.descriptor=descriptor;
            duplicate.expected_base=original->base_rights;duplicate.expected_inheriting=original->inheriting_rights;
            auto alias=query(duplicate);require(alias.result==ws::status::ok && alias.mutation_applied,"target original capability has real alias");
            duplicate.operation=ws::action::reduce_rights;duplicate.descriptor=alias.affected_descriptor;
            auto reduced=query(duplicate);require(reduced.result==ws::status::ok && reduced.mutation_applied,"low-rights target alias does not hide stronger FD");
        }
        auto bad=::std::make_shared<pp::snapshot>(*loaded);bad->original_wasm[0]^=::std::byte{1};request.portable_restore=bad;
        require(call(request).status==cs::stale_environment,"wrong original Wasm rejected without native epoch reuse");
        bad=::std::make_shared<pp::snapshot>(*loaded);bad->bindings[0].resource_index=UINT32_MAX;request.portable_restore=bad;
        require(call(request).status==cs::invalid_portable_snapshot,"untrusted resource index rejected");
        bad=::std::make_shared<pp::snapshot>(*loaded);
        for(auto& r:bad->resources) { if(r.type==pp::kind::file) { r.mount=pp::text{u8"missing-mount"}; } }
        request.portable_restore=bad;require(call(request).status==cs::missing_mount,"missing target mount rejected before commit");
        bad=::std::make_shared<pp::snapshot>(*loaded);
        for(auto& r:bad->resources) { if(r.type==pp::kind::file) { r.path=pp::text{u8"../escape"}; } }
        request.portable_restore=bad;require(call(request).status==cs::invalid_portable_snapshot,"traversal rejected before opening");
        request.portable_restore=loaded;
        ::std::size_t file_index{};while(file_index<loaded->resources.size() && loaded->resources[file_index].type!=pp::kind::file) { ++file_index; }
        require(file_index<loaded->resources.size(),"saved path resource exists");
        auto capture_request=request;capture_request.portable_restore.reset();capture_request.portable_metadata_only=true;
        auto before_scan=call(capture_request);::std::vector<::std::byte> before_scan_wire{};
        require(before_scan.status==cs::captured && before_scan.portable && pp::encode(*before_scan.portable,before_scan_wire),
            "genuine target checkpoint available before oversized import");
        check_fd_quota(*before_scan.portable);
        if(sparse_reserved)
        {
            auto protected_before=before_scan_wire;
            auto moved=::std::make_shared<pp::snapshot>(*loaded);moved->reserved.front().descriptor=INT32_MAX-1u;
            require(pp::valid(*moved),"metadata with shifted high reserved slot is structurally valid");
            request.portable_restore=moved;
            require(call(request).status==cs::capability_denied,"target sparse reserved cell cannot be dropped or moved by import");
            moved=::std::make_shared<pp::snapshot>(*loaded);moved->reserved.front().null_resource=false;request.portable_restore=moved;
            require(call(request).status==cs::capability_denied,"target sparse null-resource construction state cannot be replaced");
            auto after_protected=call(capture_request);::std::vector<::std::byte> protected_after{};
            require(after_protected.status==cs::captured && after_protected.portable &&
                pp::encode(*after_protected.portable,protected_after) && protected_after==protected_before,
                "failed sparse reserved imports leave all live aliases rights cursors text and allocator state unchanged");
            request.portable_restore=loaded;
        }

        auto oversized=::std::make_shared<pp::snapshot>(*loaded);oversized->opens_size=pp::max_rows;oversized->closed.clear();
        for(::std::uint32_t n{};n!=pp::max_rows;++n)
        {
            bool occupied{};for(auto const& b:oversized->bindings) { occupied=occupied || b.descriptor==n; }
            for(auto const& r:oversized->reserved) { occupied=occupied || r.descriptor==n; }
            if(!occupied) { oversized->closed.push_back(n); }
        }
        auto high_alias=oversized->bindings.front();high_alias.descriptor=sparse_reserved ? INT32_MAX-1u : INT32_MAX;oversized->bindings.push_back(high_alias);
        request.portable_restore=oversized;
        require(call(request).status==cs::invalid_portable_snapshot,"oversized FD scan table refused before native publication");
        auto after_scan=call(capture_request);::std::vector<::std::byte> after_scan_wire{};
        require(after_scan.status==cs::captured && after_scan.portable && pp::encode(*after_scan.portable,after_scan_wire) && after_scan_wire==before_scan_wire,
            "refused oversized import leaves FD graph rights cursor allocator and argv/env exactly unchanged and recapturable");
        require(query(fd_query).result==ws::status::ok,"FD debugger remains usable after refused oversized import");
        bad=::std::make_shared<pp::snapshot>(*loaded);bad->resources[file_index].path=pp::text{u8"malformed-"};bad->resources[file_index].path.push_back(static_cast<char8_t>(0xffu));
        require(actual_environment.disable_utf8_check,"target ordinary raw-byte path policy configured before guest publication");
        request.portable_restore=bad;auto invalid_path=call(request);
        require(invalid_path.status==cs::invalid_portable_snapshot,
            "portable malformed UTF-8 refused before target lookup even if ordinary target paths allow raw bytes");
        bad=::std::make_shared<pp::snapshot>(*loaded);bad->resources[file_index].mount.push_back(static_cast<char8_t>(0xffu));
        request.portable_restore=bad;
        require(call(request).status==cs::invalid_portable_snapshot,"malformed portable guest mount rejected before lookup");
        bad=::std::make_shared<pp::snapshot>(*loaded);bad->resources[file_index].path=pp::text{u8"absent.bin"};request.portable_restore=bad;
        require(call(request).status==cs::native_operation_failed,"missing target file fails without creating it");
        bad=::std::make_shared<pp::snapshot>(*loaded);bad->resources[file_index].type=pp::kind::external;bad->resources[file_index].mount.clear();bad->resources[file_index].path.clear();request.portable_restore=bad;
        require(call(request).status==cs::missing_rebind,"untracked file cannot invent target authority");
        request.portable_restore=loaded;request.portable_rebindings={{static_cast<::std::uint32_t>(file_index),151u}};
        require(call(request).status==cs::unsupported_resource,"directory cannot satisfy regular-file binding");
        bad=::std::make_shared<pp::snapshot>(*loaded);for(auto& b:bad->bindings) { if(b.resource_index==file_index) { b.base|=2u; } }request.portable_restore=bad;
        request.portable_rebindings={{static_cast<::std::uint32_t>(file_index),91u}};
        require(call(request).status==cs::capability_denied,"explicit binding cannot gain read rights from a write-only target FD");
        request.portable_rebindings.clear();request.portable_restore=loaded;
        request.maximum_descriptors=0u;require(call(request).status==cs::resource_limit,"import descriptor quota honored");request.maximum_descriptors=65536u;
        request.maximum_owned_text_bytes=0u;require(call(request).status==cs::resource_limit,"import owned text quota honored");request.maximum_owned_text_bytes=1048576u;
        ws::request unchanged{};unchanged.operation=ws::action::arguments;auto before=query(unchanged);
        require(before.result==ws::status::ok && before.strings[1].bytes==u8"target-before","failed import left target text intact");
        require(::fast_io::operations::io_stream_seek_bytes(
#if defined(_WIN32) && !defined(__CYGWIN__)
            actual_environment.fd_storage.renumber_map.find(91)->second.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file,
#else
            actual_environment.fd_storage.renumber_map.find(91)->second.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd,
#endif
            0,::fast_io::seekdir::cur)==128,"failed import left target native cursor intact");
        restrict_mount();
        if(root_flags)
        {
            ws::request close{};close.operation=ws::action::close_descriptor;close.descriptor=173u;close.expected_base=8u;
            auto closed=query(close);require(closed.result==ws::status::ok && closed.mutation_applied,
                "target has no held child SET_FLAGS authority; inheriting rights must authorize a fresh root");
        }
        auto restored=call(request);
        if(restored.status!=cs::restored) { ::fast_io::io::perrln("restore status=",static_cast<unsigned>(restored.status)," ",::fast_io::mnp::code_cvt(restored.diagnostic)); }
        require(restored.status==cs::restored,"fresh process restores source metadata under target authority");
        ws::request q{};q.operation=ws::action::arguments;auto args=query(q);
        require(args.result==ws::status::ok && args.strings.size()==2u && args.strings[1].bytes==u8"before","owned argv rebound");
        q.operation=ws::action::environment;auto env=query(q);require(env.result==ws::status::ok && env.strings.size()==1u && env.strings[0].bytes==u8"UWVM_DEBUG_KEY=old","owned environment rebound");
        q.operation=ws::action::descriptors;auto fds=query(q);require(fds.result==ws::status::ok && fds.descriptors.size()==snapshot->bindings.size(),"complete FD table rebound");
        auto& file=*actual_environment.fd_storage.renumber_map.find(91)->second.fd_p;
#if defined(_WIN32) && !defined(__CYGWIN__)
        auto& reopened=file.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file;
#else
        auto& reopened=file.wasi_fd.ptr->wasi_fd_storage.storage.file_fd;
#endif
        require(::fast_io::operations::io_stream_seek_bytes(reopened,0,::fast_io::seekdir::cur)==128,"restored guest fd cursor");
        auto const& shared=*actual_environment.fd_storage.opens.index_unchecked(sparse_reserved ? 0u : 3u).fd_p;
        require(shared.wasi_fd.ptr==file.wasi_fd.ptr,"aliases share one native open state after import");
        if(sparse_reserved)
        {
            auto reserved=actual_environment.fd_storage.renumber_map.find(INT32_MAX);
            require(reserved!=actual_environment.fd_storage.renumber_map.end() && reserved->second.fd_p &&
                reserved->second.fd_p->close_pos==SIZE_MAX && reserved->second.fd_p->wasi_fd.ptr &&
                reserved->second.fd_p->wasi_fd.ptr->wasi_fd_storage.type==::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null &&
                reserved->second.fd_p->rights_base==wasi_abi::rights_t{} && reserved->second.fd_p->rights_inherit==wasi_abi::rights_t{},
                "actual restore rebuilds high reserved FD in sparse map with zero capabilities");
        }

        ::fast_io::native_file content{target_file,::fast_io::open_mode::in};char bytes[64u]{};
        auto end=::fast_io::operations::read_some(content,bytes,bytes+64u);
        require(::fast_io::string_view{bytes,static_cast<::std::size_t>(end-bytes)}=="TARGET-CONTENT-UNCHANGED","target file was neither copied nor truncated");
        // Every failure above left the original target preopen usable.
        require(actual_environment.fd_storage.renumber_map.find(151)!=actual_environment.fd_storage.renumber_map.end(),"saved preopen FD number restored");
        auto boundary=::std::make_shared<pp::snapshot>(*oversized);
        auto const excess=boundary->bindings.size()+boundary->reserved.size()+boundary->closed.size()-pp::max_rows;
        boundary->opens_size-=static_cast<::std::uint32_t>(excess);boundary->closed.resize(boundary->closed.size()-excess);
        require(pp::valid(*boundary),"actual import boundary has exactly 65536 scan cells including sparse INT32_MAX FD");
        request.portable_restore=boundary;
        require(call(request).status==cs::restored,"native import accepts exact scan boundary");
        auto boundary_fds=query(fd_query);
        require(boundary_fds.result==ws::status::ok && boundary_fds.total_entries==boundary->bindings.size(),"FD debugger scans restored boundary without resource_limit");

        if(fd_slot)
        {
            auto baseline=call(capture_request);::std::vector<::std::byte> baseline_wire{},current_wire{};
            require(baseline.status==cs::captured && baseline.portable && pp::encode(*baseline.portable,baseline_wire),
                "real boundary baseline captured before FD allocation");
            auto const scan_cells=actual_environment.fd_storage.opens.size()+actual_environment.fd_storage.renumber_map.size();
            require(scan_cells==pp::max_rows && actual_environment.fd_storage.closes.size()>=2u &&
                actual_environment.fd_storage.fd_limit==pp::max_rows+1u,
                "real original initializer leaves FD admission headroom at the exact debugger scan boundary");
            ws::request edits[2u]{};
            require(ws::parse("set wasip1 file 0 410042",edits[0]) &&
                ws::parse("set wasip1 fd-dup 0 91 0x60006c 0",edits[1]),"actual console create and duplicate commands parse");
            ::std::uint64_t issued[2u]{};
            for(unsigned n{};n!=2u;++n)
            {
                auto expected=actual_environment.fd_storage.closes.back_unchecked();
                auto edited=query(edits[n]);
                if(fd_slot_before)
                { require(edited.result==ws::status::resource_limit && !edited.mutation_applied,
                    "old manager incorrectly refuses reuse of a closed boundary slot"); }
                else
                {
                    require(edited.result==ws::status::ok && edited.mutation_applied && edited.affected_descriptor==expected,
                        "genuine manager reuses the original last closed slot");
                    issued[n]=edited.affected_descriptor;
                    require(actual_environment.fd_storage.opens.size()+actual_environment.fd_storage.renumber_map.size()==scan_cells,
                        "successful boundary allocation does not enlarge the scan table");
                }
            }
            if(!fd_slot_before)
            {
                auto file=[](auto const& resource)->::fast_io::native_file&
                {
#if defined(_WIN32) && !defined(__CYGWIN__)
                    return resource.ptr->wasi_fd_storage.storage.file_fd.file;
#else
                    return resource.ptr->wasi_fd_storage.storage.file_fd;
#endif
                };
                auto& created=actual_environment.fd_storage.opens.index_unchecked(issued[0]).fd_p->wasi_fd;
                auto& duplicate=actual_environment.fd_storage.opens.index_unchecked(issued[1]).fd_p->wasi_fd;
                require(91u<actual_environment.fd_storage.opens.size() &&
                    actual_environment.fd_storage.opens.index_unchecked(91u).fd_p &&
                    actual_environment.fd_storage.opens.index_unchecked(91u).fd_p->wasi_fd.ptr,
                    "boundary import places FD91 in the actual dense prefix");
                auto& original=actual_environment.fd_storage.opens.index_unchecked(91u).fd_p->wasi_fd;
                ::std::byte content[3u]{};
                ::uwvm2::runtime::lib::wasip1_native_file::read_content(file(created),content,content+3u);
                require(created.ptr->checkpoint_managed_identity!=0u && content[0]==::std::byte{'A'} &&
                    content[1]==::std::byte{} && content[2]==::std::byte{'B'} &&
                    ::fast_io::operations::io_stream_seek_bytes(file(created),0,::fast_io::seekdir::cur)==0,
                    "boundary construction publishes genuine managed binary content with cursor zero");
                require(duplicate.ptr==original.ptr,"boundary duplication retains the actual shared native resource");
                ::fast_io::operations::io_stream_seek_bytes(file(duplicate),129,::fast_io::seekdir::beg);
                require(::fast_io::operations::io_stream_seek_bytes(file(original),0,::fast_io::seekdir::cur)==129,
                    "duplicated boundary FD shares the original cursor");
                ::fast_io::operations::io_stream_seek_bytes(file(duplicate),128,::fast_io::seekdir::beg);
                for(unsigned n{2u};n!=0u;--n)
                {
                    ws::request close{};close.operation=ws::action::close_descriptor;close.descriptor=issued[n-1u];
                    close.expected_base=n==2u ? 0x60006cu : 0x60006eu;
                    auto closed=query(close);require(closed.result==ws::status::ok && closed.mutation_applied,
                        "genuine close returns each boundary slot in original free-list order");
                }
            }
            auto after=call(capture_request);
            require(after.status==cs::captured && after.portable && pp::encode(*after.portable,current_wire) && current_wire==baseline_wire,
                "boundary allocation/close or old refusal preserves complete graph cursor rights text and allocator order");
            // Fill the existing holes with aliases through the real portable
            // consumer. No new native handles or direct table edits are used.
            auto full=::std::make_shared<pp::snapshot>(*boundary);
            auto source_binding=::std::find_if(full->bindings.begin(),full->bindings.end(),[](auto const& b) { return b.descriptor==91u; });
            require(source_binding!=full->bindings.end(),"real saved file supplies full-table aliases");
            auto prototype=*source_binding;
            for(auto n:full->closed) { auto alias=prototype;alias.descriptor=n;full->bindings.push_back(alias); }
            full->closed.clear();require(pp::valid(*full),"exact full table retains its reserved management input");
            request.portable_restore=full;
            require(call(request).status==cs::restored && actual_environment.fd_storage.closes.empty(),
                "actual consumer publishes a full scan table under the configured 65537 occupied-slot allowance");
            auto full_before=call(capture_request);::std::vector<::std::byte> full_wire{},refused_wire{};
            require(full_before.status==cs::captured && full_before.portable && pp::encode(*full_before.portable,full_wire),
                "full target checkpoint captured without fabricated table state");
            for(auto const& edit:edits)
            { auto refused=query(edit);require(refused.result==ws::status::resource_limit && !refused.mutation_applied,
                "full scan table refuses growth despite separate FD admission headroom"); }
            auto full_after=call(capture_request);
            require(full_after.status==cs::captured && full_after.portable && pp::encode(*full_after.portable,refused_wire) && refused_wire==full_wire,
                "full-table rejection leaves the entire target environment unchanged and checkpointable");
            request.portable_restore=boundary;require(call(request).status==cs::restored,"actual consumer restores reusable boundary holes");
            ::fast_io::io::println(::fast_io::mnp::os_c_str(fd_slot_before ? "FD_SLOT_NEGATIVE verified" : "FD_SLOT_REUSE verified"),
                " scan-cells=",scan_cells," occupied-limit=",actual_environment.fd_storage.fd_limit);
        }

        auto boundary_saved=call(capture_request);
        require(boundary_saved.status==cs::captured && boundary_saved.portable && pp::valid(*boundary_saved.portable),"restored exact scan boundary can checkpoint again");
        request.portable_restore=loaded;require(call(request).status==cs::restored,"restore original sparse layout after boundary test");
        if(dsync_only)
        {
#if (!defined(_WIN32) || defined(__CYGWIN__)) && defined(O_DSYNC) && defined(O_SYNC)
            // Boundary imports above replace FD owners. Reacquire the current
            // live file before inspecting its native flags.
            auto& current_file=actual_environment.fd_storage.renumber_map.find(91)->second.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd;
            auto const native_flags=::fast_io::posix_getfl_nothrow(::fast_io::native_io_observer{current_file});
            require(native_flags.error==0 && (native_flags.flags&O_DSYNC)==O_DSYNC &&
                (native_flags.flags&O_SYNC)!=O_SYNC,"portable restore reopens DSYNC without silently upgrading to native SYNC");
#endif
            auto recaptured=call(capture_request);::std::vector<::std::byte> current_wire{},loaded_wire{};
            require(recaptured.status==cs::captured && recaptured.portable && pp::encode(*recaptured.portable,current_wire) &&
                pp::encode(*loaded,loaded_wire) && current_wire==loaded_wire,
                "DSYNC-only portable restore recaptures the complete graph byte-identically");
            auto strong=::std::make_shared<pp::snapshot>(*loaded);
            for(auto& r:strong->resources) { if(r.type==pp::kind::file) { r.flags|=24u; } }
            request.portable_restore=strong;
            require(call(request).status==cs::capability_denied,"genuinely stronger SYNC/RSYNC still needs target FD_SYNC authority");
            auto after_denied=call(capture_request);::std::vector<::std::byte> after_wire{};
            require(after_denied.status==cs::captured && after_denied.portable && pp::encode(*after_denied.portable,after_wire) &&
                after_wire==current_wire,"strong sync rejection leaves complete target state unchanged");
            request.portable_restore=loaded;
        }
        if(normalized_path)
        {
            auto recaptured=call(capture_request);::std::vector<::std::byte> current_wire{},loaded_wire{};
            require(recaptured.status==cs::captured && recaptured.portable && pp::encode(*recaptured.portable,current_wire) &&
                pp::encode(*loaded,loaded_wire) && current_wire==loaded_wire,
                "normalized original guest path saves and restores a canonical mounted-file graph byte-identically");
        }
        if(root_alias)
        {
            auto recaptured=call(capture_request);::std::vector<::std::byte> current_wire{},loaded_wire{};
            require(recaptured.status==cs::captured && recaptured.portable && pp::encode(*recaptured.portable,current_wire) &&
                pp::encode(*loaded,loaded_wire) && current_wire==loaded_wire,
                "independent root resources follow metadata aliases and full state recapture byte-identically");
            auto& root=actual_environment.fd_storage.renumber_map.find(151)->second.fd_p->wasi_fd;
            auto& first=actual_environment.fd_storage.renumber_map.find(171)->second.fd_p->wasi_fd;
            auto& second=actual_environment.fd_storage.renumber_map.find(172)->second.fd_p->wasi_fd;
            require(root.ptr!=first.ptr && root.ptr!=second.ptr && first.ptr!=second.ptr &&
                ::uwvm2::runtime::lib::wasip1_mount_identity::same(root,first) &&
                ::uwvm2::runtime::lib::wasip1_mount_identity::same(root,second) &&
                first.ptr->checkpoint_follow && !second.ptr->checkpoint_follow,
                "one actual configured mount retains three distinct directory RCs and saved follow modes");
            if(root_flags)
            {
                auto& third=actual_environment.fd_storage.renumber_map.find(173)->second.fd_p->wasi_fd;
                auto handle=[](auto const& r) { return r.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.back_unchecked().ptr->dir_stack.storage.file.native_handle(); };
                require(handle(root)!=handle(first) && handle(root)!=handle(second) && handle(root)!=handle(third) &&
                    handle(first)!=handle(second) && handle(first)!=handle(third) && handle(second)!=handle(third),
                    "every saved root resource restores an independent native open description");
                // The controller owns this explicit scratch buffer; the parked
                // guest's thread-scoped memory binding grants no ambient access.
                ::uwvm2::object::memory::linear::native_memory_t probe_memory{};
                probe_memory.init_by_page_count(1u, 1u);
                auto flags=[&](int fd)
                {
                    wasi_storage::scoped_current_wasip1_memory_t probe_binding{actual_environment, ::std::addressof(probe_memory)};
                    require(wasi_functions::fd_fdstat_get(actual_environment,fd,256u)==wasi_abi::errno_t::esuccess,"original guest ABI reads restored root flags");
                    return ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint16_t>(probe_memory,258u);
                };
                require(flags(151)==0u && flags(171)==4u && flags(172)==0u && flags(173)==4u,"restored roots retain distinct supported flags");
                require(wasi_functions::fd_fdstat_set_flags_base(actual_environment,173,wasi_abi::fdflags_t{})==wasi_abi::errno_t::esuccess,
                    "original setter changes only the restored root that actually holds FD_FDSTAT_SET_FLAGS");
                require(flags(151)==0u && flags(171)==4u && flags(172)==0u && flags(173)==0u,
                    "restored child flag edit cannot mutate another saved resource or configured preopen");
                require(wasi_functions::fd_fdstat_set_flags_base(actual_environment,173,wasi_abi::fdflags_t::fdflag_nonblock)==wasi_abi::errno_t::esuccess,
                    "original setter restores supported independent NONBLOCK");
                auto after_edit=call(capture_request);::std::vector<::std::byte> edited_wire{};
                require(after_edit.status==cs::captured && after_edit.portable && pp::encode(*after_edit.portable,edited_wire) && edited_wire==loaded_wire,
                    "independent original flag edits round-trip the complete portable graph");
                auto stronger=::std::make_shared<pp::snapshot>(*loaded);
                auto root_binding=::std::find_if(stronger->bindings.begin(),stronger->bindings.end(),[](auto const& b) { return b.descriptor==171u; });
                require(root_binding!=stronger->bindings.end(),"stronger root probe uses an authentic saved binding");
                stronger->resources[root_binding->resource_index].flags|=26u;
                request.portable_restore=stronger;
                require(call(request).status==cs::capability_denied,
                    "PATH_OPEN and a separate mutable alias cannot authorize stronger directory synchronization");
                auto after_denied=call(capture_request);::std::vector<::std::byte> denied_wire{};
                require(after_denied.status==cs::captured && after_denied.portable && pp::encode(*after_denied.portable,denied_wire) && denied_wire==loaded_wire,
                    "denied stronger root restore preserves the complete target graph and native flags");
                request.portable_restore=loaded;
                if(root_choice)
                {
                    auto& fourth=actual_environment.fd_storage.renumber_map.find(174)->second.fd_p->wasi_fd;
                    require(flags(174)==4u && handle(fourth)!=handle(root) && handle(fourth)!=handle(first) &&
                        handle(fourth)!=handle(second) && handle(fourth)!=handle(third),
                        "matching higher root restores its exact flags on an independent native description");
                    ws::request remove{};remove.operation=ws::action::close_descriptor;remove.descriptor=174u;remove.expected_base=0x2000u;
                    auto closed=query(remove);require(closed.result==ws::status::ok && closed.mutation_applied,
                        "remove the only complete matching root authority");
                    auto baseline=call(capture_request);::std::vector<::std::byte> baseline_wire{},refused_wire{};
                    require(baseline.status==cs::captured && baseline.portable && pp::encode(*baseline.portable,baseline_wire),
                        "genuine complete target graph captured after matching root removal");
                    require(call(request).status==cs::capability_denied,
                        "wrong-state PATH_OPEN plus another root cannot invent inherited PATH_OPEN");
                    auto refused=call(capture_request);
                    require(refused.status==cs::captured && refused.portable && pp::encode(*refused.portable,refused_wire) &&
                        refused_wire==baseline_wire,"missing complete root authority rejects without changing any target state");
                }
            }
        }
        if(sparse_reserved)
        {
            auto recaptured=call(capture_request);::std::vector<::std::byte> recaptured_wire{},loaded_wire{};
            require(recaptured.status==cs::captured && recaptured.portable && pp::encode(*recaptured.portable,recaptured_wire) &&
                pp::encode(*loaded,loaded_wire) && recaptured_wire==loaded_wire,
                "sparse reserved import can checkpoint again with byte-identical allocator and resource metadata");
        }

    }
    } // fresh-process restore branch
    lib::llvm_jit_wasip1_environment_capsule_request denied=request;denied.portable_restore.reset();denied.portable_metadata_only=true;
    require(lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api({},owners,denied).status==cs::unavailable_capture,"serialized metadata never replaces a real stop");
    require(state->control->resume(ticket),"real native guest resumed");guest.join();
    require(result==7u,"Core3 guest resumes normally");
    ::fast_io::io::println("debug_wasip1_portable_runtime PASS checks=",checks," mode=",::fast_io::mnp::os_c_str(argv[4])," policy=",::fast_io::mnp::os_c_str(argv[2]));
}
