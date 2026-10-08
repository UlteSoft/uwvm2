#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64_simd.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;
namespace d=o::details;namespace simd=uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using i64=d::wasm_i64;using v128=simd::wasm_v128;using opcode=simd::simd_code;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false},tail{.is_tail_call=true};
constexpr o::uwvm_interpreter_translate_option_t address_only{.is_tail_call=true,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t value_only{.is_tail_call=true,.v128_stack_top_begin_pos=3,.v128_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t separate{.is_tail_call=true,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5,.v128_stack_top_begin_pos=5,.v128_stack_top_end_pos=7};
constexpr o::uwvm_interpreter_translate_option_t tiny{.is_tail_call=true,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=4,.v128_stack_top_begin_pos=4,.v128_stack_top_end_pos=5};
unsigned checks{};
struct observation{std::byte* sp{};std::array<std::byte,16> bits{};};
#include "memory64_simd_reference.h"
using memory64_simd_reference::describe;
using memory64_simd_reference::expected;
template<typename T>void append(std::byte*& p,T const& v){std::memcpy(p,&v,sizeof(v));p+=sizeof(v);}
template<bool Store,auto O,std::size_t Pos,typename... T>
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(T... args) UWVM_THROWS
{
 auto& result=*reinterpret_cast<observation*>(args...[2]);result.sp=args...[1];
 if constexpr(!Store)
 {
  if constexpr(O.v128_stack_top_begin_pos==O.v128_stack_top_end_pos){std::memcpy(result.bits.data(),args...[1]-16,16);}
  else {auto value=o::get_curr_val_from_operand_stack_top<O,v128,Pos>(args...);std::memcpy(result.bits.data(),&value,16);}
 }
}
template<unsigned Code,auto O,std::size_t AP=O.i64_stack_top_begin_pos,std::size_t VP=O.v128_stack_top_begin_pos,typename... Slots>
void run(d::native_memory_t& memory,unsigned lane,std::uint64_t dynamic,Slots... slots)
{
 constexpr auto op=static_cast<opcode>(Code);constexpr auto spc=describe(Code);
 constexpr auto vb=O.v128_stack_top_begin_pos,ve=O.v128_stack_top_end_pos,ab=O.i64_stack_top_begin_pos,ae=O.i64_stack_top_end_pos;
 constexpr auto result_pos=spc.consumes?VP:(vb!=ve?(VP==vb?ve-1:VP-1):0);
 std::array<std::byte,16> wire{},old{};
 for(unsigned n=0;n<16;++n){wire[n]=std::byte(0x83+13*n);old[n]=std::byte(0x31+17*n);}
 v128 value{};std::memcpy(&value,old.data(),16);
 std::array<std::byte,128> operands{};std::memset(operands.data(),0xa7,16);auto sp=operands.data()+16;
 std::array<std::byte,128> code{};auto cursor=code.data();std::byte const* ip=code.data();
 observation out{};auto local=reinterpret_cast<std::byte*>(&out);
 auto inputs=[&]<typename... T>(T&... args)
 {
  auto address=std::bit_cast<i64>(dynamic);
  if constexpr(ab!=ae){d::set_curr_val_to_stacktop_cache<O,i64,AP>(address,args...);}else {append(sp,address);}
  if constexpr(spc.consumes)
  {if constexpr(vb!=ve){d::set_curr_val_to_stacktop_cache<O,v128,VP>(value,args...);}else {append(sp,value);}}
 };
 inputs(ip,sp,local,slots...);
 o::uwvm_interpreter_stacktop_currpos_t positions{};positions.i64_stack_top_curr_pos=AP;positions.v128_stack_top_curr_pos=VP;
 auto fn=o::translate::get_uwvmint_memory64_simd_fptr_from_tuple<op,O>(positions,memory,
  uwvm2::utils::container::tuple<std::byte const*,std::byte*,std::byte*,Slots...>{});
 append(cursor,fn);append(cursor,&memory);append(cursor,std::uint64_t{4});
 if constexpr(spc.lane){append(cursor,static_cast<simd::u8>(lane));}
 if constexpr(O.is_tail_call){append(cursor,&finish<spc.store,O,result_pos,std::byte const*,std::byte*,std::byte*,Slots...>);}
 if constexpr(spc.store){std::memset(memory.memory_begin+512,0xa5,18);}else {std::memcpy(memory.memory_begin+513,wire.data(),16);}
 decltype(fn) volatile dispatch=fn;dispatch(ip,sp,local,slots...);
 if(dynamic!=509)return;
 if constexpr(!O.is_tail_call)
 {out.sp=sp;if constexpr(!spc.store){std::memcpy(out.bits.data(),sp-16,16);}CHECK(ip==cursor);}
 CHECK(out.sp==operands.data()+16+(!spc.store&&vb==ve?16:0));
 for(unsigned n=0;n<16;++n){CHECK(operands[n]==std::byte{0xa7});}
 auto reference=expected(Code,lane,wire,old);
 if constexpr(spc.store)
 {CHECK(memory.memory_begin[512]==std::byte{0xa5});CHECK(!std::memcmp(memory.memory_begin+513,reference.data(),spc.width));
  for(unsigned n=spc.width;n<17;++n){CHECK(memory.memory_begin[513+n]==std::byte{0xa5});}}
 else {CHECK(!std::memcmp(out.bits.data(),reference.data(),16));}
 ++checks;
}
std::byte const* protected_suffix{};
void trap(int){for(unsigned i=0;i<32;++i)if(protected_suffix[i]!=std::byte{0x5a})_Exit(91);_Exit(92);}
template<unsigned Code>void partial_store(d::native_memory_t& memory,std::size_t length=65536)
{
 constexpr auto spc=describe(Code);static_assert(spc.store);
 std::memset(memory.memory_begin+length-32,0x5a,32);auto pid=fork();CHECK(pid>=0);
 if(!pid)
 {
  protected_suffix=memory.memory_begin+length-32;
  for(int sig:{SIGSEGV,SIGBUS,SIGABRT,SIGILL,SIGTRAP})std::signal(sig,trap);
  run<Code,tail>(memory,16/spc.width-1,length-spc.width+1-4);_Exit(94);
 }
 int status{};CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==92);++checks;
}
template<unsigned Code>void operation(d::native_memory_t& memory)
{
 constexpr auto spc=describe(Code);
 for(unsigned lane=0;lane<(spc.lane?16/spc.width:1);++lane)
 {
  run<Code,byref>(memory,lane,509);run<Code,tail>(memory,lane,509);
  run<Code,address_only>(memory,lane,509,i64{},i64{});run<Code,address_only,4>(memory,lane,509,i64{},i64{});
  run<Code,value_only>(memory,lane,509,v128{},v128{});run<Code,value_only,0,4>(memory,lane,509,v128{},v128{});
  run<Code,separate>(memory,lane,509,i64{},i64{},v128{},v128{});
  run<Code,separate,4,6>(memory,lane,509,i64{},i64{},v128{},v128{});
  run<Code,tiny>(memory,lane,509,i64{},v128{});
 }
 if constexpr(spc.store){partial_store<Code>(memory);}
}
int main()
{
 d::native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#endif
 memory.init_by_page_count(1);
 []<unsigned... Op>(d::native_memory_t& memory,std::integer_sequence<unsigned,Op...>){(operation<Op>(memory),...);}
 (memory,std::integer_sequence<unsigned,0,1,2,3,4,5,6,7,8,9,10,11,84,85,86,87,88,89,90,91,92,93>{});
#if defined(UWVM_SUPPORT_MMAP)
 for(auto page:{1uz,4096uz})
 {
  d::native_memory_t custom{page,uwvm2::object::memory::linear::mmap_memory_status_t::wasm64};auto length=page==1?4097uz:65536uz;
  custom.init_by_page_count(length/page);partial_store<11>(custom,length);partial_store<88>(custom,length);
  partial_store<89>(custom,length);partial_store<90>(custom,length);partial_store<91>(custom,length);
 }
#endif
 std::printf("PASS memory64 SIMD: %u checks, all 22 memory encodings/all lanes, sign extension/splats/zero, byref/musttail, wrapping and tiny register rings, store atomicity\n",checks);
}
