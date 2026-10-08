// Real shared DSO setter registered as both DL and ELF weak-symbol modules.
#include <uwvm2/uwvm/wasm/loader/dl.h>
#include <uwvm2/uwvm/wasm/loader/weak_symbol.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace st=::uwvm2::uwvm::imported::wasi::wasip1::storage;
namespace ws=::uwvm2::uwvm::wasm::storage;
namespace ty=::uwvm2::uwvm::wasm::type;
namespace loader=::uwvm2::uwvm::wasm::loader;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
#define CHECK(v) do {if(!(v)){::fast_io::io::perrln("FAIL shared plugin setter line=",__LINE__);return 1;}} while(false)
int main(int argc,char** argv) {
#if !defined(__ELF__)
 (void)argc;(void)argv;
 ::fast_io::io::println("SKIP mixed weak-symbol registration: ELF-only public interface");
 return 77;
#else
 CHECK(argc==2);
 auto& dl=ws::preloaded_dl.emplace_back();
 dl.module_name=::uwvm2::utils::container::u8string_view{u8"dl-permitted",12};
 dl.import_dll_file=::fast_io::native_dll_file{::fast_io::mnp::os_c_str(argv[1]),::fast_io::dll_mode::posix_rtld_lazy};
 auto const getter=reinterpret_cast<ty::capi_get_function_vec_t>(::fast_io::dll_load_symbol(dl.import_dll_file,u8"uwvm_function"));
 auto const setter=reinterpret_cast<ty::uwvm_set_wasip1_host_api_v1_t>(::fast_io::dll_load_symbol(dl.import_dll_file,u8"uwvm_set_wasip1_host_api_v1"));
 CHECK(getter && setter);
 dl.wasm_dl_storage.capi_function_vec=getter();ws::register_preloaded_dl_capi_functions(0);
 ws::preload_expose_wasip1_host_api=false;
 auto* allow=st::try_create_targetless_wasip1_module_override(dl.module_name);
 CHECK(allow);allow->expose_host_api_is_set=true;allow->expose_host_api=true;
 loader::apply_wasip1_host_api_to_loaded_dl(dl);
 auto& env=st::default_wasip1_env;env.argv.emplace_back(u8"x");
 ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1,4);
 st::scoped_current_wasip1_memory_t binding{env,&memory};
 st::scoped_current_wasip1_env_t selected{env};
 auto invoke=[&](st::wasip1_module_target_kind_t kind,auto name) {
  st::scoped_current_wasip1_target_t target{kind,name};
  ::std::array<::std::byte,8> parameters{};::std::array<::std::byte,4> results{};
  ::std::uint_least32_t size_offset=4,code{};::std::memcpy(parameters.data()+4,&size_offset,4);
  dl.wasm_dl_storage.capi_function_vec.function_begin[1].func_ptr(results.data(),parameters.data());
  ::std::memcpy(&code,results.data(),4);return static_cast<abi::errno_t>(code);
 };
 CHECK(invoke(st::wasip1_module_target_kind_t::preloaded_dl,dl.module_name)==abi::errno_t::esuccess);
 auto& weak=ws::weak_symbol.emplace_back();weak.module_name=::uwvm2::utils::container::u8string_view{u8"weak-denied",11};
 weak.set_wasip1_host_api_v1=setter;weak.wasm_wws_storage.capi_function_vec=getter();
 ws::register_weak_symbol_capi_functions(0);
 loader::apply_wasip1_host_api_to_loaded_weak_symbol(weak);
 CHECK(invoke(st::wasip1_module_target_kind_t::preloaded_dl,dl.module_name)==abi::errno_t::esuccess);
 CHECK(invoke(st::wasip1_module_target_kind_t::weak_symbol,weak.module_name)==abi::errno_t::enotcapable);
 // Reverse the granted registration kind and refresh in the opposite order.
 allow->expose_host_api=false;
 auto* weak_allow=st::try_create_targetless_wasip1_module_override(weak.module_name);
 CHECK(weak_allow);weak_allow->expose_host_api_is_set=true;weak_allow->expose_host_api=true;
 loader::apply_wasip1_host_api_to_loaded_weak_symbol(weak);
 loader::apply_wasip1_host_api_to_loaded_dl(dl);
 CHECK(invoke(st::wasip1_module_target_kind_t::weak_symbol,weak.module_name)==abi::errno_t::esuccess);
 CHECK(invoke(st::wasip1_module_target_kind_t::preloaded_dl,dl.module_name)==abi::errno_t::enotcapable);
 weak_allow->expose_host_api=false;
 loader::refresh_preloaded_dl_wasip1_host_api();loader::refresh_loaded_weak_symbol_wasip1_host_api();
 CHECK(invoke(st::wasip1_module_target_kind_t::weak_symbol,weak.module_name)==abi::errno_t::enotcapable);
 ::fast_io::io::println("PASS genuine DL/weak shared setter: both grant orders, per-call denial, all-disabled refresh");
#endif
}
