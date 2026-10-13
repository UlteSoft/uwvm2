// Internal memory64 RMW handler qualification; frontend integration is separate.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64_atomic.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;
namespace d=o::details;
using i32=d::wasm_i32;using i64=d::wasm_i64;using op=d::atomic_rmw_operation;
using slot=o::wasm_stack_top_i32_with_i64_u;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false},tail{.is_tail_call=true};
constexpr o::uwvm_interpreter_translate_option_t merged1{.is_tail_call=true,.i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=4,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=4};
constexpr o::uwvm_interpreter_translate_option_t merged2{.is_tail_call=true,.i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t merged3{.is_tail_call=true,.i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=6,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=6};
constexpr o::uwvm_interpreter_translate_option_t split{.is_tail_call=true,.i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5,.i64_stack_top_begin_pos=5,.i64_stack_top_end_pos=7};
constexpr o::uwvm_interpreter_translate_option_t address_only{.is_tail_call=true,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t value_only{.is_tail_call=true,.i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5};
struct observation{std::byte* sp{};std::uint64_t value{};};
unsigned checks{};
template<typename T>void append(std::byte*& cursor,T value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<typename Value,auto O,std::size_t Pos,typename... T>
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(T... args) UWVM_THROWS
{
 auto& out=*reinterpret_cast<observation*>(args...[2]);out.sp=args...[1];
 auto value=o::get_curr_val_from_operand_stack_top<O,Value,Pos>(args...);std::memcpy(&out.value,&value,sizeof(value));
}
template<op Operation,typename Value,std::size_t Bytes,auto O,typename... Slots>
void run(d::native_memory_t& memory,std::uint64_t input,std::uint64_t compare,std::uint64_t dynamic,std::uint64_t offset,Slots... slots)
{
 constexpr bool cas=Operation==op::compare_exchange;
 constexpr auto vb=d::stacktop_range_begin_pos<O,Value>(),ve=d::stacktop_range_end_pos<O,Value>(),vn=ve-vb;
 constexpr auto ab=O.i64_stack_top_begin_pos,ae=O.i64_stack_top_end_pos;
 constexpr bool merged=vn&&vb==ab&&ve==ae;
 // Start at the final slot to force a ring wrap on every multi-input operation.
 constexpr auto vp=vn?ve-1:0,ap=ae!=ab?(merged?vp:ae-1):0;
 constexpr auto result_pos=vn?vb+(vp-vb+(cas?2:1)+(merged?1:0)-1)%vn:0;
 std::array<std::byte,128> operands{},code{};std::memset(operands.data(),0xa7,16);
 auto sp=operands.data()+16,cursor=code.data();std::byte const* ip=code.data();observation out{};auto local=reinterpret_cast<std::byte*>(&out);
 auto setup=[&]<typename... T>(T&... args)
 {
  auto address=std::bit_cast<i64>(dynamic);
  constexpr unsigned address_depth=cas?2:1;
  if constexpr(ae!=ab&&(!merged||vn>address_depth))
  {constexpr auto pos=merged?ab+(vp-ab+address_depth)%(ae-ab):ap;d::set_curr_val_to_stacktop_cache<O,i64,pos>(address,args...);}
  else{append(sp,address);}
  if constexpr(cas)
  {
   if constexpr(vn>=2){constexpr auto pos=vb+(vp-vb+1)%vn;d::set_curr_val_to_stacktop_cache<O,Value,pos>(static_cast<Value>(compare),args...);}
   else{append(sp,static_cast<Value>(compare));}
  }
  if constexpr(vn){d::set_curr_val_to_stacktop_cache<O,Value,vp>(static_cast<Value>(input),args...);}
  else{append(sp,static_cast<Value>(input));}
 };
 setup(ip,sp,local,slots...);
 o::uwvm_interpreter_stacktop_currpos_t positions{};
 positions.i32_stack_top_curr_pos=O.i32_stack_top_end_pos?O.i32_stack_top_end_pos-1:0;
 positions.i64_stack_top_curr_pos=ap;
 auto tuple=uwvm2::utils::container::tuple<std::byte const*,std::byte*,std::byte*,Slots...>{};
 auto fn=o::translate::get_uwvmint_memory64_rmw_fptr_from_tuple<Operation,sizeof(Value)==8,Bytes,O>(positions,memory,tuple);
 append(cursor,fn);append(cursor,&memory);append(cursor,offset);
 if constexpr(O.is_tail_call){append(cursor,&finish<Value,O,result_pos,std::byte const*,std::byte*,std::byte*,Slots...>);}
 fn(ip,sp,local,slots...);
 if(dynamic!=512||offset!=8){return;} // A missing trap must not be hidden by later assertions.
 if constexpr(!O.is_tail_call){out.sp=sp;std::memcpy(&out.value,sp-sizeof(Value),sizeof(Value));CHECK(ip==code.data()+sizeof(fn)+sizeof(&memory)+sizeof(offset));}
 CHECK(out.sp==operands.data()+16+(vn?0:sizeof(Value)));
 for(unsigned i=0;i<16;++i){CHECK(operands[i]==std::byte{0xa7});}
 constexpr auto mask=~std::uint64_t{}>>(64-Bytes*8);constexpr auto initial=0x8585858585858585ull&mask;
 CHECK(out.value==initial);auto expected=initial;
 if constexpr(Operation==op::add){expected+=input;}
 if constexpr(Operation==op::sub){expected-=input;}
 if constexpr(Operation==op::and_){expected&=input;}
 if constexpr(Operation==op::or_){expected|=input;}
 if constexpr(Operation==op::xor_){expected^=input;}
 if constexpr(Operation==op::exchange){expected=input;}
 if constexpr(cas){if((compare&mask)==initial){expected=input;}}
 for(unsigned i=0;i<Bytes;++i){CHECK(memory.memory_begin[520+i]==std::byte(expected>>(i*8)));}
 CHECK(memory.memory_begin[519]==std::byte{0x85});CHECK(memory.memory_begin[520+Bytes]==std::byte{0x85});++checks;
}
template<op Operation,typename Value,std::size_t Bytes,auto O,typename... Slots>
void width(d::native_memory_t& memory,Slots... slots)
{
 for(auto input:{0ull,~0ull,0x88776655aabbccddull})for(bool match:{true,false})
 {
  std::memset(memory.memory_begin+519,0x85,10);
  constexpr auto mask=~std::uint64_t{}>>(64-Bytes*8);
  auto compare=(0x8585858585858585ull&mask)^(match?~mask:1ull);
  run<Operation,Value,Bytes,O>(memory,input,compare,512,8,slots...);
 }
}
template<op Operation,auto O,typename... Slots>void operation(d::native_memory_t& memory,Slots... slots)
{
 width<Operation,i32,1,O>(memory,slots...);width<Operation,i32,2,O>(memory,slots...);width<Operation,i32,4,O>(memory,slots...);
 width<Operation,i64,1,O>(memory,slots...);width<Operation,i64,2,O>(memory,slots...);width<Operation,i64,4,O>(memory,slots...);width<Operation,i64,8,O>(memory,slots...);
}
template<auto O,typename... Slots>void configuration(d::native_memory_t& memory,Slots... slots)
{
 operation<op::add,O>(memory,slots...);operation<op::sub,O>(memory,slots...);operation<op::and_,O>(memory,slots...);
 operation<op::or_,O>(memory,slots...);operation<op::xor_,O>(memory,slots...);operation<op::exchange,O>(memory,slots...);operation<op::compare_exchange,O>(memory,slots...);
}
std::byte const* trap_bytes{};
void UWVM_INTERPRETER_OPFUNC_TYPE_MACRO unaligned_trap() noexcept
{for(unsigned i=0;i<10;++i){if(trap_bytes[i]!=std::byte{0x85}){_Exit(93);}}_Exit(92);}
int main()
{
 d::native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#endif
 memory.init_by_page_count(1);
 configuration<byref>(memory);configuration<tail>(memory);
 configuration<merged1>(memory,slot{});configuration<merged2>(memory,slot{},slot{});configuration<merged3>(memory,slot{},slot{},slot{});
 configuration<split>(memory,i32{},i32{},i64{},i64{});configuration<address_only>(memory,i64{},i64{});configuration<value_only>(memory,i32{},i32{});
 for(auto values:{std::array<std::uint64_t,2>{~0ull,1},{~0ull-7,8},{8,~0ull-7},{1ull<<32,0},{1ull<<48,0},{513,8}})
 {
  auto pid=fork();CHECK(pid>=0);
  if(!pid)
  {
   std::memset(memory.memory_begin+519,0x85,10);trap_bytes=memory.memory_begin+519;o::trap_unaligned_atomic_func=unaligned_trap;
   run<op::compare_exchange,i64,8,tail>(memory,0,0,values[0],values[1]);_Exit(94);
  }
  int status{};CHECK(waitpid(pid,&status,0)==pid);
  if(values[0]==513){CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==92);}else{CHECK(WIFSIGNALED(status));}++checks;
 }
 std::printf("PASS memory64 RMW: %u checks; all 49 operations, narrow wrap/zero extension, CAS match/mismatch, byref and wrapping register rings, u65/alignment traps\n",checks);
}
