// Real compliant DSO + actual stable WASI function. No runtime/control mocks.
#include <uwvm2/uwvm/imported/wasi/wasip1/storage/env.h>
#include <uwvm2/uwvm/wasm/storage/impl.h>
#include <uwvm2/uwvm/wasm/type/dl.h>
#include <uwvm2/uwvm/wasm/type/wasip1_api.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <barrier>
#include <cstring>
#include <thread>
namespace st=::uwvm2::uwvm::imported::wasi::wasip1::storage;
namespace ty=::uwvm2::uwvm::wasm::type;
namespace abi=::uwvm2::imported::wasi::wasip1::abi;
namespace lin=::uwvm2::object::memory::linear;
extern "C" abi::errno_t uwvm_wasip1_args_sizes_get(abi::wasi_void_ptr_t,abi::wasi_void_ptr_t) noexcept;
#define CHECK(v) do { if(!(v)){::fast_io::io::perrln("FAIL WASI binding line=",__LINE__);return 1;} } while(false)
template<typename Fn> static void with_memory(st::wasip1_env_type& env,lin::native_memory_t* mem,Fn&& fn) {
#if defined(UWVM_TEST_PLUGIN_BASELINE)
 env.wasip1_memory=mem;
#else
 st::scoped_current_wasip1_memory_t memory{env,mem};
#endif
 st::scoped_current_wasip1_env_t selected{env};
 fn();
}
static void fill(lin::native_memory_t& mem) { ::std::memset(mem.memory_begin,0xcc,16); }
static ::std::uint_least32_t read(lin::native_memory_t& mem) {
 ::std::uint_least32_t value{};::std::memcpy(&value,mem.memory_begin,4);return value;
}
int main(int argc,char** argv) {
 CHECK(argc==2);
 lin::native_memory_t first{},second{};first.init_by_page_count(1,4);second.init_by_page_count(1,4);
 auto& env=st::default_wasip1_env;env.argv.emplace_back(u8"x");
 ::uwvm2::uwvm::wasm::storage::preload_expose_wasip1_host_api=true;
 ::fast_io::native_dll_file library{::fast_io::mnp::os_c_str(argv[1]),::fast_io::dll_mode::posix_rtld_lazy};
 auto getter=reinterpret_cast<ty::capi_get_function_vec_t>(::fast_io::dll_load_symbol(library,u8"uwvm_function"));
 auto setter=reinterpret_cast<ty::uwvm_set_wasip1_host_api_v1_t>(::fast_io::dll_load_symbol(library,u8"uwvm_set_wasip1_host_api_v1"));
 CHECK(getter!=nullptr && setter!=nullptr);
 auto const functions=getter();CHECK(functions.function_size==2);
 // Only the exercised table entry is retained; its implementation is linked
 // from the genuine host_api.default.cpp, never copied into this fixture.
 ty::uwvm_wasip1_host_api_v1 api{};api.struct_size=sizeof(api);api.abi_version=ty::wasip1_host_api_v1_abi_version;
 api.args_sizes_get=uwvm_wasip1_args_sizes_get;setter(&api);
 auto invoke=[&] {
  ::std::array<::std::byte,8> parameters{};::std::array<::std::byte,4> results{};
  ::std::uint_least32_t size_offset=4,code{};::std::memcpy(parameters.data()+4,&size_offset,4);
  functions.function_begin[1].func_ptr(results.data(),parameters.data());
  ::std::memcpy(&code,results.data(),4);return static_cast<abi::errno_t>(code);
 };
 fill(first);fill(second);
 abi::errno_t first_error{},second_error{};
 ::std::barrier synchronized{2};
 ::std::thread a{[&] {
  with_memory(env,&first,[&] {synchronized.arrive_and_wait();synchronized.arrive_and_wait();first_error=invoke();});
 }};
 ::std::thread b{[&] {
  synchronized.arrive_and_wait();
  with_memory(env,&second,[&] {second_error=invoke();synchronized.arrive_and_wait();});
 }};
 a.join();b.join();CHECK(first_error==abi::errno_t::esuccess && second_error==abi::errno_t::esuccess);
#if defined(UWVM_TEST_PLUGIN_BASELINE)
 CHECK(read(first)==0xccccccccu && read(second)==1);
 ::fast_io::io::println("REPRO overlapping compliant DSO calls: caller A's WASI write reached B");
#else
 CHECK(read(first)==1 && read(second)==1);
#endif
 fill(first);fill(second);
 with_memory(env,&first,[&] {
  with_memory(env,&second,[&] {second_error=invoke();});
  first_error=invoke();
 });
 CHECK(first_error==abi::errno_t::esuccess && second_error==abi::errno_t::esuccess);
#if defined(UWVM_TEST_PLUGIN_BASELINE)
 CHECK(read(first)==0xccccccccu && read(second)==1);
 ::fast_io::io::println("REPRO nested binding: outer compliant DSO call retained inner caller B");
#else
 CHECK(read(first)==1 && read(second)==1);
 with_memory(env,&first,[&] {
  with_memory(env,nullptr,[&] {second_error=invoke();});
  first_error=invoke();
 });
 CHECK(second_error==abi::errno_t::efault && first_error==abi::errno_t::esuccess);
 // No ambient caller, even if someone leaves a raw pointer on this managed env.
 env.wasip1_memory=&first;CHECK(invoke()==abi::errno_t::efault);env.wasip1_memory=nullptr;
 auto denied=st::try_create_targetless_wasip1_module_override(::uwvm2::utils::container::u8string_view{u8"restricted",10});
 CHECK(denied!=nullptr);denied->expose_host_api_is_set=true;denied->expose_host_api=false;
 with_memory(env,&first,[&] {
  st::scoped_current_wasip1_target_t target{st::wasip1_module_target_kind_t::preloaded_dl,u8"restricted"};
  first_error=invoke();
 });
 CHECK(first_error==abi::errno_t::enotcapable);
 // Ordinary native environments keep explicit memory-field compatibility.
 st::wasip1_env_type native{.wasip1_memory=&first};
 {st::scoped_current_wasip1_env_t selected{native};CHECK(invoke()==abi::errno_t::esuccess);}
 ::fast_io::io::println("PASS compliant DSO concurrent/nested/null/outside-call bindings, cached API target denial, native compatibility");
#endif
}
