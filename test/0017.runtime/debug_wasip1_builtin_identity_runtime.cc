// Real builtin binding-only census fixture. This test uses ONLY
// existing public actual capture_instance entry. No new friend/callback issuer.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/uwvm/debugger/checkpoint_state.h>
#include <fast_io.h>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace full = ::uwvm2::uwvm::runtime::full;
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
namespace images = ::uwvm2::utils::control;
namespace wasi = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
using domain = threads::cooperative_pause_domain;
static void require(bool okay, unsigned line)
{
    if(!okay)
    {
        ::fast_io::io::perrln("debug_wasip1_builtin_identity_runtime FAIL line=", ::fast_io::mnp::dec(line));
        ::fast_io::fast_terminate();
    }
}
#define REQUIRE(condition) require(bool(condition), __LINE__)
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct setup
{
    ::std::u8string path{},provider_path{};
    images::owned_file_image::owner immutable{},provider_image{};
    full::full_source_instance::mutable_owner source{};
    static bool prepare(void* context) noexcept
    {
        // [actual synchronous retained setup object] end
        // [safe] native API holds drained maintenance until callback returns.
        auto& state{*static_cast<setup*>(context)};
        try
        {
            ::std::vector<full::full_preload_input> inputs{};full::full_preload_input preload{};
            preload.file_name=state.provider_path;preload.module_name=u8"builtin-identity-provider";
            preload.parameters=::uwvm2::uwvm::wasm::storage::wasm_parameter;
            preload.image=::std::move(state.provider_image);inputs.push_back(::std::move(preload));
            auto owner{full::full_source_instance::create_unparsed(state.path,u8"builtin-identity",::std::move(inputs))};
            if(!full::select_unparsed_full_source_after_drain(owner)) { return false; }
            auto loaded{::uwvm2::uwvm::wasm::loader::load_wasm_file(owner->file_for_native_initialization(),
                owner->owned_file_name(),owner->owned_rename(),::uwvm2::uwvm::wasm::storage::wasm_parameter,
                ::std::move(state.immutable))};
            if(loaded != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok) { return false; }
            auto& provider{owner->preloaded_files_for_native_initialization().index_unchecked(0u)};
            if(::uwvm2::uwvm::wasm::loader::load_wasm_file(provider,provider.file_name,provider.module_name,provider.wasm_parameter,
                owner->take_preloaded_image_for_native_initialization(0u)) != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok ||
                ::uwvm2::uwvm::run::load_local_modules() != static_cast<int>(::uwvm2::uwvm::run::retval::ok) ||
                ::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() !=
                    ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok ||
                ::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() !=
                    ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) { return false; }
            ::uwvm2::uwvm::runtime::initializer::initialize_runtime(true);
            if(!owner->seal_actual_initializer()) { return false; }
            state.source = ::std::move(owner); return true;
        }
        catch(...) { return false; }
    }
};
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t points{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    threads::cooperative_pause_location location{};
    bool captured{}, finished{};
    static void point(void* context, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        auto& state{*static_cast<observer*>(context)};
        if(where.function != 2u || state.points.fetch_add(1u,::std::memory_order_relaxed) != 19u) { return; }
        auto ticket{state.control->request_pause()}; REQUIRE(ticket);
        ::std::lock_guard guard{state.mutex}; state.ticket=::std::move(ticket);state.location=where;
    }
    static void before_park(void* context, ::std::uint_least64_t, threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& state{*static_cast<observer*>(context)};
        ::std::lock_guard guard{state.mutex}; REQUIRE(state.ticket && state.location==where && !state.captured);
        auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(state.ticket)};
        REQUIRE(actual.status==lib::llvm_jit_checkpoint_capture_status::captured && actual.capture);
        state.capture=::std::move(actual.capture);state.captured=true;state.changed.notify_all();
    }
};
static cp::object const& item(cp::state const& graph,cp::object_id id,cp::object_kind kind)
{
    REQUIRE(id!=0u && id<=graph.objects.size());
    // [owned dense object IDs1..N] end
    // [safe] bound BEFORE subtract/index; graph contains detached DATA only.
    auto const& value{graph.objects[static_cast<::std::size_t>(id-1u)]}; REQUIRE(value.kind==kind);return value;
}
static ::std::uint64_t le64(::std::vector<::std::byte> const& bytes,::std::size_t offset)
{
    REQUIRE(offset<=bytes.size() && 8u<=bytes.size()-offset);
    // [owned exact binding DATA ... offset][8 bytes] end
    // [safe] subtraction bounds BEFORE forming/advancing both cursors.
    auto const* first{reinterpret_cast<unsigned char const*>(bytes.data())+offset};
    auto const* last{first+8u};::std::uint64_t value{};
    auto parsed{::fast_io::parse_by_scan(first,last,::fast_io::mnp::le_get<64u>(value))};
    REQUIRE(parsed.code==::fast_io::parse_code::ok && parsed.iter==last);return value;
}
static void check_graph(cp::state const& graph,cp::limits const& cap,::std::array<::std::byte,16u> const& label)
{
    REQUIRE(cp::validate_graph(graph,cap)==cp::error::none && graph.recording_id==label && graph.root_instances.size()==2u);
    cp::object_id previous_function{},main_alias{},provider_function{};::std::vector<::std::byte> previous_binding{};
    for(auto instance_id:graph.root_instances)
    {
        auto const& instance{item(graph,instance_id,cp::object_kind::instance)};
        REQUIRE((instance.words[0u]==2u || instance.words[0u]==3u) && instance.words[1u]==0u && instance.words[2u]==1u && instance.links.size()>=4u);
        auto const& imported{item(graph,instance.links[1u],cp::object_kind::function)};
        REQUIRE(imported.flags==1u && imported.words[0u]==0u && imported.words[1u]==1u && imported.links.size()==2u &&
            imported.links[0u]==instance_id && imported.bytes.empty() && instance.links[1u]!=previous_function);
        auto const& binding{item(graph,imported.links[1u],cp::object_kind::host_resource)};
        // Flag3 is explicitly binding-only DATA, NEVER a pure/replay/effect adapter.
        // Genuine runtime source and loader witnesses are mandatory. A structural
        // codec/test-only bypass cannot issue this capture. Full restore is not claimed.
        REQUIRE(binding.flags==3u && binding.words[0u]==cp::builtin_wasip1_binding_kind && binding.words[1u]==1u && binding.bytes.size()==108u);
        constexpr ::fast_io::string_view magic{"UWVMWASIP1B1"};
        for(::std::size_t i{};i!=magic.size();++i)
        { REQUIRE(binding.bytes[i]==static_cast<::std::byte>(magic[i])); }
        auto const index{le64(binding.bytes,12u)};
        REQUIRE(index<128u && binding.words[2u]==index+1u && le64(binding.bytes,20u)==1u);
        REQUIRE(le64(binding.bytes,60u)==2u && le64(binding.bytes,68u)==1u);
        bool digest{};
        for(::std::size_t i{28u};i!=60u;++i) { digest=digest || binding.bytes[i]!=::std::byte{}; }
        REQUIRE(digest);
        for(::std::size_t i{};i!=16u;++i)
        {
            REQUIRE(binding.bytes[76u+i]==(i<2u ? ::std::byte{0x7fu}: ::std::byte{}));
            REQUIRE(binding.bytes[92u+i]==(i==0u ? ::std::byte{0x7fu}: ::std::byte{}));
        }
        if(previous_function!=0u) { REQUIRE(binding.bytes==previous_binding); }
        previous_function=instance.links[1u];previous_binding=binding.bytes;
        // The two genuine host ABI bindings may have identical DATA, but each
        // logical function still retains its actual receiving instance/origin.
        auto const is_main{instance.words[0u]==3u};
        auto const defined_index{is_main ? 2u:1u};
        REQUIRE(defined_index+1u<instance.links.size());
        auto const& defined{item(graph,instance.links[defined_index+1u],cp::object_kind::function)};
        REQUIRE(defined.flags==0u && defined.words[0u]==defined_index && defined.words[1u]==1u && defined.links[0u]==instance_id);
        if(is_main) { main_alias=instance.links[2u]; } else { provider_function=instance.links[1u]; }
    }
    REQUIRE(main_alias!=0u && main_alias==provider_function);
    // A genuine main import aliases the provider's original terminal import;
    // alias map identity is shared, while two direct origins stay distinct.
}
int main(int argc,char** argv)
{
    if(argc!=4) { return 64; }
    auto const strategy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    REQUIRE(strategy=="instruction" || strategy=="unwind");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=strategy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction:mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u;mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    setup state{};state.path=::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    state.provider_path=::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[2])));
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"builtin-identity",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(state.path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false;features.explicit_enable_gc=true;
    features.disable_reference_types=false;features.explicit_enable_reference_types=true;
    features.disable_function_references=false;features.explicit_enable_function_references=true;
    wasi::wasip1_noinherit_system_environment=true;wasi::wasip1_force_args_is_set=true;
    wasi::wasip1_force_argument_storage.emplace_back(u8"argv0");wasi::wasip1_force_argument_storage.emplace_back(u8"before");
    auto bytes{images::owned_file_image::read(state.path,1048576u)};REQUIRE(bytes);state.immutable=::std::move(bytes.image);
    bytes=images::owned_file_image::read(state.provider_path,1048576u);REQUIRE(bytes);state.provider_image=::std::move(bytes.image);
    REQUIRE(lib::replace_full_source_after_drain_host_api(setup::prepare,::std::addressof(state)) && state.source && state.source->file().has_owned_source_image());
    REQUIRE(state.source->registry().size()==2u);
    auto stop{::std::make_shared<observer>()};
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(stop->control,{stop,observer::point,observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(lib::llvm_jit_configure_debug_value_observation_host_api()==lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(lib::llvm_jit_prepare_debug_host_api());
    ::std::uint32_t result{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{};run.entry_function_index=2u;
        run.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(result));run.entry_abi_buffers.result_bytes=sizeof(result);
        lib::full_compile_and_run_main_module(u8"builtin-identity",run);
        ::std::lock_guard guard{stop->mutex};stop->finished=true;stop->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock guard{stop->mutex};REQUIRE(stop->changed.wait_until(guard,deadline(),[&]{return stop->captured || stop->finished;}));
        REQUIRE(stop->captured && !stop->finished);ticket=stop->ticket;
    }
    REQUIRE(stop->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,1u> captures{stop->capture};
    ::std::array<::std::byte,16u> label{};label[0u]=::std::byte{0x71u};label[15u]=::std::byte{0x19u};cp::limits cap{};
    auto actual{lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,captures,label,cap)};
    REQUIRE(actual.status==lib::llvm_jit_checkpoint_instance_capture_status::captured && actual.graph && actual.data_error==cp::error::none);
    check_graph(*actual.graph,cap,label);
    auto tiny{cap};tiny.max_objects=1u;
    auto declined{lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,captures,label,tiny)};
    REQUIRE(!declined.graph && declined.status!=lib::llvm_jit_checkpoint_instance_capture_status::captured);
    auto foreign{::std::make_shared<int const>(7)};
    lib::llvm_jit_checkpoint_thread_capture_owner forged{foreign,captures.front().get()};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,1u> forged_captures{forged};
    declined=lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,forged_captures,label,cap);
    REQUIRE(!declined.graph && declined.status!=lib::llvm_jit_checkpoint_instance_capture_status::captured);
    REQUIRE(stop->control->resume(ticket));guest.join();REQUIRE(result==91u);
    ::std::uint32_t provider_result{};lib::full_compile_run_config provider_run{};provider_run.entry_function_index=1u;
    provider_run.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(provider_result));
    provider_run.entry_abi_buffers.result_bytes=sizeof(provider_result);
    lib::full_compile_and_run_main_module(u8"builtin-identity-provider",provider_run);REQUIRE(provider_result==91u);
    declined=lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,captures,label,cap);
    REQUIRE(!declined.graph && declined.status!=lib::llvm_jit_checkpoint_instance_capture_status::captured);
    lib::reset_runtime_state_host_api();check_graph(*actual.graph,cap,label);
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;
    ::fast_io::io::println("WASIp1 real builtin binding DATA PASS actualloader/capture/census/twoorigins/originalimports; externalrestore=false");
}
