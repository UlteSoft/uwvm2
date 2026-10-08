#include <uwvm2/uwvm/wasm/storage/preload_module_attribute.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace ty=::uwvm2::uwvm::wasm::type;
namespace st=::uwvm2::uwvm::wasm::storage;
#define CHECK(v) do { if(!(v)){::fast_io::io::perrln("FAIL registry line=",__LINE__);return 1;} } while(false)
static void load(char const* path, char8_t const* name, ty::preload_module_memory_access_mode_t mode) {
 auto& module=st::preloaded_dl.emplace_back();
 module.import_dll_file=::fast_io::native_dll_file{::fast_io::mnp::os_c_str(path),::fast_io::dll_mode::posix_rtld_lazy};
 module.module_name=::uwvm2::utils::container::u8string_view{name,::fast_io::cstr_len(name)};
 module.preload_module_memory_attribute.memory_access_mode=mode;
 module.preload_module_memory_attribute.apply_to_all_memories=true;
 module.wasm_dl_storage.get_function_vec=reinterpret_cast<ty::capi_get_function_vec_t>(::fast_io::dll_load_symbol(module.import_dll_file,u8"uwvm_function"));
 module.wasm_dl_storage.capi_function_vec=module.wasm_dl_storage.get_function_vec();
 st::register_preloaded_dl_capi_functions(st::preloaded_dl.size()-1);
}
int main(int argc,char** argv) {
 CHECK(argc>=2);
 using mode=ty::preload_module_memory_access_mode_t;
 load(argv[1],u8"restricted",mode::none);
 auto const first=st::preloaded_dl[0].wasm_dl_storage.capi_function_vec.function_begin;
 load(argv[1],u8"permitted",mode::copy);
 auto const second=st::preloaded_dl[1].wasm_dl_storage.capi_function_vec.function_begin;
 auto const first_owner=st::find_preload_capi_function_owner(first);
 CHECK(first_owner!=nullptr);
 auto const first_policy=st::find_loaded_preload_module_memory_attribute(first);
 CHECK(first_policy!=nullptr);
 if(argc==3) {
  CHECK(first==second && first_owner->module_index==1 && first_policy->memory_access_mode==mode::copy);
  ::fast_io::io::println("REPRO same compliant DSO table aliases: restricted owner=1; policy=copy");
  return 0;
 }
 CHECK(first!=second);
 CHECK(first_owner->module_index==0 && first_policy->memory_access_mode==mode::none);
 CHECK(st::find_preload_capi_function_owner(second)->module_index==1);
 CHECK(st::find_loaded_preload_module_memory_attribute(second)->memory_access_mode==mode::copy);
 // Normal vector relocation must retain the registered descriptor identities.
 for(::std::size_t i{};i!=256;++i) st::preloaded_dl.emplace_back();
 CHECK(st::preloaded_dl[0].wasm_dl_storage.capi_function_vec.function_begin==first);
 CHECK(st::find_preload_capi_function_owner(first)->module_index==0);
 ::std::array<::std::byte,8> args{}; ::std::array<::std::byte,4> results{};
 ::std::uint_least32_t a=17,b=25,actual{};
 ::std::memcpy(args.data(),&a,4);::std::memcpy(args.data()+4,&b,4);
 first->func_ptr(results.data(),args.data());::std::memcpy(&actual,results.data(),4);
 CHECK(actual==42);
 ::fast_io::io::println("PASS compliant DSO alias ownership, policies, relocation and native entry");
}
