#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;
namespace d=o::details;
using i32=d::wasm_i32;using i64=d::wasm_i64;
using slot=o::wasm_stack_top_i32_with_i64_u;
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();} } while(false)
struct observation { std::byte* sp{};std::uint64_t bits{}; };
unsigned checks{};
constexpr std::uint64_t payload=0x88776655ddbbaa80ull;
#if defined(UWVM_TEST_MEMORY64_ATOMIC)
constexpr bool atomic_test=true;
#else
constexpr bool atomic_test=false;
#endif
constexpr std::uint64_t data_address=atomic_test?520:513,dynamic_address=data_address-4;

constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false};
constexpr o::uwvm_interpreter_translate_option_t tail{.is_tail_call=true};
constexpr o::uwvm_interpreter_translate_option_t merged1{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=4,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=4};
constexpr o::uwvm_interpreter_translate_option_t merged2{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t split{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5,.i64_stack_top_begin_pos=5,.i64_stack_top_end_pos=7};
constexpr o::uwvm_interpreter_translate_option_t address_only{.is_tail_call=true,
 .i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t value_only{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5};
template<bool Store,typename Value,auto O,std::size_t Pos,typename... T>
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(T... args) UWVM_THROWS
{
 auto& out=*reinterpret_cast<observation*>(args...[2]);out.sp=args...[1];
 if constexpr(!Store)
 {
  auto value=o::get_curr_val_from_operand_stack_top<O,Value,Pos>(args...);
  std::memcpy(&out.bits,&value,sizeof(value));
 }
}
template<typename T>void append(std::byte*& cursor,T value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<auto O,typename Value,std::size_t Bytes,bool Signed,bool Store,typename... Slots>
void run(d::native_memory_t& memory,std::uint64_t dynamic=dynamic_address,std::uint64_t offset=4,Slots... slots)
{
 constexpr auto vb=d::stacktop_range_begin_pos<O,Value>(),ve=d::stacktop_range_end_pos<O,Value>();
 constexpr auto ab=O.i64_stack_top_begin_pos,ae=O.i64_stack_top_end_pos;
 constexpr bool merged=vb!=ve&&vb==ab&&ve==ae;
 constexpr auto result_pos=merged?vb:(vb!=ve?ve-1:0);
 std::array<std::byte,128> operands{};std::memset(operands.data(),0xa7,16);
 auto sp=operands.data()+16;observation out{};auto local=reinterpret_cast<std::byte*>(&out);
 std::array<std::byte,128> code{};auto cursor=code.data();std::byte const* ip=code.data();
 auto set_inputs=[&]<typename... Args>(Args&... args)
 {
  auto address=std::bit_cast<i64>(dynamic);
  Value value{};std::memcpy(&value,&payload,sizeof(value));
  if constexpr(Store&&merged&&(ae-ab==1)){append(sp,address);}
  else if constexpr(ab!=ae)
  {
   constexpr auto position=Store&&merged?ab+1:ab;
   d::set_curr_val_to_stacktop_cache<O,i64,position>(address,args...);
  }
  else {append(sp,address);}
  if constexpr(Store)
  {
   if constexpr(vb!=ve){d::set_curr_val_to_stacktop_cache<O,Value,vb>(value,args...);}
   else {append(sp,value);}
  }
 };
 set_inputs(ip,sp,local,slots...);
 o::uwvm_interpreter_stacktop_currpos_t positions{};
 positions.i32_stack_top_curr_pos=O.i32_stack_top_begin_pos;
 positions.i64_stack_top_curr_pos=O.i64_stack_top_begin_pos;
 auto fn=[&]
 {
  auto tuple=uwvm2::utils::container::tuple<std::byte const*,std::byte*,std::byte*,Slots...>{};
  if constexpr(atomic_test){return o::translate::get_uwvmint_memory64_atomic_fptr_from_tuple<Value,Bytes,Store,O>(positions,memory,tuple);}
  else {return o::translate::get_uwvmint_memory64_load_store_fptr_from_tuple<Value,Bytes,Signed,Store,O>(positions,memory,tuple);}
 }();
 append(cursor,fn);append(cursor,&memory);append(cursor,offset);
 if constexpr(O.is_tail_call){append(cursor,&finish<Store,Value,O,result_pos,std::byte const*,std::byte*,std::byte*,Slots...>);}
 if constexpr(Store){std::memset(memory.memory_begin+data_address-1,0xa5,10);}
 else {auto bits=fast_io::little_endian(payload);std::memcpy(memory.memory_begin+data_address,&bits,8);}
 fn(ip,sp,local,slots...);
 // A negative probe that returned must exit normally: later value assertions
 // must not manufacture a signal and conceal a missing bounds trap.
 if(dynamic!=dynamic_address || offset!=4){return;}
 if constexpr(!O.is_tail_call)
 {
  out.sp=sp;
  if constexpr(!Store){std::memcpy(&out.bits,sp-sizeof(Value),sizeof(Value));}
  CHECK(ip==code.data()+sizeof(fn)+sizeof(&memory)+sizeof(offset));
 }
 CHECK(out.sp==operands.data()+16+(!Store&&vb==ve?sizeof(Value):0));
 for(unsigned i{};i!=16;++i){CHECK(operands[i]==std::byte{0xa7});}
 if constexpr(Store)
 {
  CHECK(memory.memory_begin[data_address-1]==std::byte{0xa5});
  for(std::size_t i{};i!=Bytes;++i){CHECK(memory.memory_begin[data_address+i]==std::byte((payload>>(8*i))&255));}
  for(std::size_t i=Bytes;i!=9;++i){CHECK(memory.memory_begin[data_address+i]==std::byte{0xa5});}
 }
 else
 {
  std::uint64_t expected=Bytes==1?0x80ull:Bytes==2?0xaa80ull:Bytes==4?0xddbbaa80ull:payload;
  if constexpr(Signed&&Bytes<sizeof(Value)){expected|=(~0ull)<<(Bytes*8);}
  if constexpr(sizeof(Value)==4){expected&=0xffffffffull;}
  CHECK(out.bits==expected);
 }
 ++checks;
}
template<auto O,typename Value,std::size_t Bytes,typename... Slots>void width(d::native_memory_t& memory,Slots... slots)
{
 run<O,Value,Bytes,false,false>(memory,dynamic_address,4,slots...);
 if constexpr(!atomic_test){run<O,Value,Bytes,true,false>(memory,dynamic_address,4,slots...);}
 run<O,Value,Bytes,false,true>(memory,dynamic_address,4,slots...);
}
template<auto O,typename... Slots>void configuration(d::native_memory_t& memory,Slots... slots)
{
 width<O,i32,1>(memory,slots...);width<O,i32,2>(memory,slots...);width<O,i32,4>(memory,slots...);
 width<O,i64,1>(memory,slots...);width<O,i64,2>(memory,slots...);width<O,i64,4>(memory,slots...);width<O,i64,8>(memory,slots...);
}
std::byte const* atomic_trap_bytes{};
void UWVM_INTERPRETER_OPFUNC_TYPE_MACRO unaligned_store_trap() noexcept
{for(unsigned i=0;i<10;++i){if(atomic_trap_bytes[i]!=std::byte{0xa5}){_Exit(93);}}_Exit(92);}
int main()
{
 d::native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#endif
 memory.init_by_page_count(1);
 configuration<byref>(memory);configuration<tail>(memory);
 configuration<merged1>(memory,slot{});configuration<merged2>(memory,slot{},slot{});
 configuration<split>(memory,i32{},i32{},i64{},i64{});
 configuration<address_only>(memory,i64{},i64{});configuration<value_only>(memory,i32{},i32{});
 for(auto values:{std::array<std::uint64_t,2>{~0ull,1},{~0ull,~0ull},{1,~0ull},{1ull<<32,0},{1ull<<48,0}})
 {
  auto pid=fork();CHECK(pid>=0);
  if(!pid){run<tail,i64,8,false,false>(memory,values[0],values[1]);_Exit(0);}
  int status{};CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFSIGNALED(status));++checks;
 }
 if constexpr(atomic_test)
 {
  for(auto displacement:{1ull,3ull,7ull})
  {
   auto pid=fork();CHECK(pid>=0);
   if(!pid)
   {
    atomic_trap_bytes=memory.memory_begin+data_address-1;o::trap_unaligned_atomic_func=unaligned_store_trap;
    run<tail,i64,8,false,true>(memory,dynamic_address+displacement,4);_Exit(94);
   }
   int status{};CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==92);++checks;
  }
 }
 std::printf("PASS memory64 %s handlers: %u checks, byte widths/sign extension, byref, uncached, shared/split/one-slot/partial rings, u65 and high-address traps\n",atomic_test?"atomic load/store":"integer",checks);
}
