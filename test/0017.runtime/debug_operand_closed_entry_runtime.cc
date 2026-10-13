// Real observer-profile generated code after the pause domain closes.
// Closed participant admission must preserve native packet storage ownership.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <memory>
namespace lib=::uwvm2::runtime::lib;
namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
namespace cp=::uwvm2::runtime::checkpoint;
using domain=::uwvm2::utils::thread::cooperative_pause_domain;
static void require(bool value,char const* message)
{ if(!value) { ::fast_io::io::perrln("closed debug entry: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc!=5) { return 2; }
    auto const strategy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    auto const fixture{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
    auto const entry{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[4])}};
    require(strategy=="instruction" || strategy=="unwind","explicit stack strategy");
    require(fixture=="nested" || fixture=="saved-gc","explicit actual fixture");
    require(entry=="full" || entry=="raw","explicit public entry");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=strategy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u; mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"closed-debug-entry",nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete stable argument vector] end
    // [safe] no later growth; borrow original last file argument only now.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false; features.explicit_enable_gc=true;
    features.disable_function_references=false; features.explicit_enable_function_references=true;
    features.disable_reference_types=false; features.explicit_enable_reference_types=true;
    features.disable_exceptions=false; features.explicit_enable_exceptions=true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"closed-debug-entry";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"actual official-validated modern input parsed/initialized once");

    auto control{::std::make_shared<domain>(1u)};
    require(lib::llvm_jit_configure_debug_session_host_api(control,{},lib::llvm_jit_debug_safe_point_granularity::instruction)
        ==lib::llvm_jit_debug_configure_result::ok,"actual debug configuration before native publication");
    require(lib::llvm_jit_configure_debug_value_observation_host_api()==lib::llvm_jit_debug_configure_result::ok,
        "actual observer heap profile, not a resumable/native packet");
    require(lib::llvm_jit_prepare_debug_host_api(),"actual complete instrumented native publication");
    auto source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
    require(source && source->initialized_main_module()!=nullptr,"actual retained initialized module owner");
    ::fast_io::io::println("closed debug entry publication PASS; entering");
    control->close(); require(control->is_closed(),"actual pause-domain close before entry");
    for(unsigned call{};call!=3u;++call)
    {
        ::std::uint32_t value{};
        if(entry=="full")
        {
            lib::full_compile_run_config config{};config.entry_function_index=0u;
            // [actual live result owner through this synchronous native entry]
            config.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(value));
            config.entry_abi_buffers.result_bytes=sizeof(value);
            lib::full_compile_and_run_main_module(u8"closed-debug-entry",config);
        }
        else { lib::llvm_jit_call_raw_host_api(source->initialized_main_module(),0u,::std::addressof(value),sizeof(value),nullptr,0u); }
        require(value==(fixture=="nested" ? 316u : 7u),"real closed-domain guest result after calls/EH/GC and arena cleanup");
    }
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("closed debug entry observer/full-or-raw/nested-EH-GC/3-entries PASS preview_available=false");
}
