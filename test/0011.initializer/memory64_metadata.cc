#ifndef UWVM
#define UWVM 2
#endif
#include <uwvm2/uwvm/runtime/initializer/init.h>
#include <uwvm2/validation/standard/wasm3/address_limits.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
namespace features=uwvm2::parser::wasm::standard::wasm1p1::features;
namespace init=uwvm2::uwvm::runtime::initializer::details;
namespace w3=uwvm2::validation::standard::wasm3;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
constexpr features::memory_limits_type declared{.min=1,.max=0x1'0000'0000'0000ull,.present_max=true};
static_assert(declared.max==(1ull<<48));
static_assert(std::numeric_limits<decltype(declared.max)>::digits>=64);
static_assert(features::memory_limits_type::default_max==std::numeric_limits<std::uint_least64_t>::max());
static_assert(init::parser_memory_limits_to_runtime_limits(declared).min==1);
static_assert(init::parser_memory_limits_to_runtime_limits(declared).max==
 ((1ull<<48)>std::numeric_limits<std::size_t>::max()?std::numeric_limits<std::size_t>::max():(1ull<<48)));
template<typename Char> void check_print()
{
 features::memory_type memory{.limits=declared,.shared=true,.address64=true};
 auto wrapper=features::section_details(memory);
 auto reservation=features::print_reserve_size(fast_io::io_reserve_type<Char,decltype(wrapper)>,wrapper);
 std::array<Char,256> output{};CHECK(reservation<output.size());
 output[reservation]=static_cast<Char>(0x7f);
 auto end=features::print_reserve_define(fast_io::io_reserve_type<Char,decltype(wrapper)>,output.data(),wrapper);
 CHECK(end<=output.data()+reservation);CHECK(output[reservation]==static_cast<Char>(0x7f));
 constexpr std::string_view expected="limits: {min: 1, max: 281474976710656}, shared: 1, address64: 1";
 CHECK(static_cast<std::size_t>(end-output.data())==expected.size());
 for(std::size_t i=0;i<expected.size();++i){CHECK(output[i]==static_cast<Char>(expected[i]));}
}
int main()
{
 check_print<char>();check_print<wchar_t>();check_print<char8_t>();check_print<char16_t>();check_print<char32_t>();
 using native=uwvm2::uwvm::wasm::type::module_memory_limit_t;
 CHECK(!init::runtime_memory_limits_widen_declared(declared,native{.min=1,.max=2,.present_max=true}));
 CHECK(init::runtime_memory_limits_widen_declared(declared,native{.min=0,.max=2,.present_max=true}));
 CHECK(init::wasm1_limits_match(declared,features::memory_limits_type{.min=2,.max=(1ull<<48)-1,.present_max=true}));
 CHECK(!init::wasm1_limits_match(declared,features::memory_limits_type{.min=2,.max=(1ull<<48)+1,.present_max=true}));
 features::memory_type wide{.limits=declared,.shared=true,.address64=true};
 features::memory_type narrow{.limits=declared,.shared=true,.address64=false};
 CHECK(init::wasm_memory_is_address64(wide));CHECK(!init::wasm_memory_is_address64(narrow));
 CHECK(!init::wasm_memory_is_address64(uwvm2::parser::wasm::standard::wasm1::type::memory_type{}));
 if constexpr(sizeof(std::size_t)<sizeof(std::uint_least64_t))
 {
  auto pid=fork();CHECK(pid>=0);
  if(pid==0)
  {
   static_cast<void>(freopen("/dev/null","w",stderr));
   auto impossible=declared;impossible.min=static_cast<std::uint_least64_t>(std::numeric_limits<std::size_t>::max())+1;
   static_cast<void>(init::parser_memory_limits_to_runtime_limits(impossible));_exit(0);
  }
  int status{};CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFSIGNALED(status)||(WIFEXITED(status)&&WEXITSTATUS(status)!=0));
 }
 std::puts("PASS u64 memory metadata: native maximum saturation, full-width limit matching, all five formatter character types, address identity; ISA32 additionally rejects an unrepresentable minimum");
}
