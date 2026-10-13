#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;
namespace d=o::details;
using i32=d::wasm_i32;using i64=d::wasm_i64;using f32=d::wasm_f32;using f64=d::wasm_f64;
using v128=uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
using scalar_slot=o::wasm_stack_top_i32_i64_f32_f64_u;
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();} } while(false)
struct observation { std::byte* sp{};std::array<std::byte,16> bits{}; };
unsigned checks{};
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false};
constexpr o::uwvm_interpreter_translate_option_t tail{.is_tail_call=true};
constexpr o::uwvm_interpreter_translate_option_t merged1{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=4,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=4,
 .f32_stack_top_begin_pos=3,.f32_stack_top_end_pos=4,.f64_stack_top_begin_pos=3,.f64_stack_top_end_pos=4};
constexpr o::uwvm_interpreter_translate_option_t merged2{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5,
 .f32_stack_top_begin_pos=3,.f32_stack_top_end_pos=5,.f64_stack_top_begin_pos=3,.f64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t address_only{.is_tail_call=true,
 .i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
template<typename V,bool Addresses>consteval auto separate()
{
 auto opt=tail;
 if constexpr(Addresses){opt.i64_stack_top_begin_pos=3;opt.i64_stack_top_end_pos=5;}
 constexpr std::size_t begin=Addresses?5:3;
 if constexpr(std::same_as<V,f32>){opt.f32_stack_top_begin_pos=begin;opt.f32_stack_top_end_pos=begin+2;}
 else if constexpr(std::same_as<V,f64>){opt.f64_stack_top_begin_pos=begin;opt.f64_stack_top_end_pos=begin+2;}
 else {opt.v128_stack_top_begin_pos=begin;opt.v128_stack_top_end_pos=begin+2;}
 return opt;
}
template<bool Store,typename Value,auto O,std::size_t Pos,typename... T>
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(T... args) UWVM_THROWS
{
 auto& out=*reinterpret_cast<observation*>(args...[2]);out.sp=args...[1];
 if constexpr(!Store)
 {
  if constexpr(d::stacktop_range_begin_pos<O,Value>()==d::stacktop_range_end_pos<O,Value>())
  {std::memcpy(out.bits.data(),args...[1]-sizeof(Value),sizeof(Value));}
  else {auto value=o::get_curr_val_from_operand_stack_top<O,Value,Pos>(args...);std::memcpy(out.bits.data(),&value,sizeof(value));}
 }
}
template<typename T>void append(std::byte*& cursor,T const& value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<auto O,typename Value,bool Store,typename... Slots>
void run(d::native_memory_t& memory,std::array<std::byte,16> const& wire,std::uint64_t dynamic=509,Slots... slots)
{
 constexpr auto vb=d::stacktop_range_begin_pos<O,Value>(),ve=d::stacktop_range_end_pos<O,Value>();
 constexpr auto ab=O.i64_stack_top_begin_pos,ae=O.i64_stack_top_end_pos;
 constexpr bool merged=vb!=ve&&vb==ab&&ve==ae;
 constexpr auto result_pos=merged?vb:(vb!=ve?ve-1:0);
 auto native=wire;
 if constexpr(!std::same_as<Value,v128> && std::endian::native==std::endian::big)
 {for(std::size_t i=0;i<sizeof(Value)/2;++i){std::swap(native[i],native[sizeof(Value)-1-i]);}}
 Value value;std::memcpy(&value,native.data(),sizeof(value));
 std::array<std::byte,128> operands{};std::memset(operands.data(),0xa7,16);
 auto sp=operands.data()+16;observation out{};auto local=reinterpret_cast<std::byte*>(&out);
 std::array<std::byte,128> code{};auto cursor=code.data();std::byte const* ip=code.data();
 auto set_inputs=[&]<typename... Args>(Args&... args)
 {
  auto address=std::bit_cast<i64>(dynamic);
  if constexpr(Store&&merged&&(ae-ab==1)){append(sp,address);}
  else if constexpr(ab!=ae)
  {constexpr auto position=Store&&merged?ab+1:ab;d::set_curr_val_to_stacktop_cache<O,i64,position>(address,args...);}
  else {append(sp,address);}
  if constexpr(Store)
  {
   if constexpr(vb!=ve){d::set_curr_val_to_stacktop_cache<O,Value,vb>(value,args...);}
   else {append(sp,value);}
  }
 };
 set_inputs(ip,sp,local,slots...);
 o::uwvm_interpreter_stacktop_currpos_t positions{};
 positions.i64_stack_top_curr_pos=O.i64_stack_top_begin_pos;
 positions.f32_stack_top_curr_pos=O.f32_stack_top_begin_pos;
 positions.f64_stack_top_curr_pos=O.f64_stack_top_begin_pos;
 positions.v128_stack_top_curr_pos=O.v128_stack_top_begin_pos;
 auto fn=o::translate::get_uwvmint_memory64_load_store_fptr_from_tuple<Value,sizeof(Value),false,Store,O>(positions,memory,
   uwvm2::utils::container::tuple<std::byte const*,std::byte*,std::byte*,Slots...>{});
 append(cursor,fn);append(cursor,&memory);append(cursor,std::uint64_t{4});
 if constexpr(O.is_tail_call){append(cursor,&finish<Store,Value,O,result_pos,std::byte const*,std::byte*,std::byte*,Slots...>);}
 if constexpr(Store){std::memset(memory.memory_begin+512,0xa5,18);}
 else {std::memcpy(memory.memory_begin+513,wire.data(),sizeof(Value));}
 fn(ip,sp,local,slots...);
 if(dynamic!=509){return;} // No assertion may conceal a missing negative-probe trap.
 if constexpr(!O.is_tail_call)
 {
  out.sp=sp;
  if constexpr(!Store){std::memcpy(out.bits.data(),sp-sizeof(Value),sizeof(Value));}
  CHECK(ip==code.data()+sizeof(fn)+sizeof(&memory)+sizeof(std::uint64_t));
 }
 CHECK(out.sp==operands.data()+16+(!Store&&vb==ve?sizeof(Value):0));
 for(unsigned i{};i!=16;++i){CHECK(operands[i]==std::byte{0xa7});}
 if constexpr(Store)
 {
  CHECK(memory.memory_begin[512]==std::byte{0xa5});
  CHECK(!std::memcmp(memory.memory_begin+513,wire.data(),sizeof(Value)));
  for(std::size_t i=sizeof(Value);i!=17;++i){CHECK(memory.memory_begin[513+i]==std::byte{0xa5});}
 }
 else {CHECK(!std::memcmp(out.bits.data(),native.data(),sizeof(Value)));}
 ++checks;
}
template<auto O,typename Value,typename... Slots>void configuration(d::native_memory_t& memory,std::array<std::byte,16> const& bits,Slots... slots)
{run<O,Value,false>(memory,bits,509,slots...);run<O,Value,true>(memory,bits,509,slots...);}
template<typename Value>void value(d::native_memory_t& memory,std::uint64_t pattern)
{
 std::array<std::byte,16> wire{};
 for(unsigned i=0;i<16;++i){wire[i]=std::byte((i<8?pattern:~pattern)>>(8*(i%8)));}
 configuration<byref,Value>(memory,wire);configuration<tail,Value>(memory,wire);
 configuration<address_only,Value>(memory,wire,i64{},i64{});
 configuration<separate<Value,false>(),Value>(memory,wire,Value{},Value{});
 configuration<separate<Value,true>(),Value>(memory,wire,i64{},i64{},Value{},Value{});
 if constexpr(!std::same_as<Value,v128>)
 {configuration<merged1,Value>(memory,wire,scalar_slot{});configuration<merged2,Value>(memory,wire,scalar_slot{},scalar_slot{});}
}
std::byte const volatile* protected_suffix{};
void trap_handler(int)
{
 for(unsigned i=0;i<32;++i){if(protected_suffix[i]!=std::byte{0x5a}){_Exit(93);}}
 _Exit(92);
}
template<typename Value>void partial_store(d::native_memory_t& memory,std::size_t length=65536)
{
 std::memset(memory.memory_begin+length-32,0x5a,32);
 auto pid=fork();CHECK(pid>=0);
 if(!pid)
 {
  protected_suffix=memory.memory_begin+length-32;
  std::signal(SIGSEGV,trap_handler);std::signal(SIGBUS,trap_handler);std::signal(SIGABRT,trap_handler);std::signal(SIGILL,trap_handler);std::signal(SIGTRAP,trap_handler);
  run<tail,Value,true>(memory,{},length-sizeof(Value)+1-4);_Exit(94);
 }
 int status{};CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==92);++checks;
}
int main()
{
 d::native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#endif
 memory.init_by_page_count(1);
 for(auto bits:{0ull,~0ull,0x8000000080000000ull,0x7f800000ull,0x7f800001ull,0x7fc10001ull,
               0xff800007ull,0x7ff0000000000000ull,0x7ff0000000000001ull,0xfff0000000000007ull,
               0x0123456789abcdefull,0xfedcba9876543210ull})
 {value<f32>(memory,bits);value<f64>(memory,bits);value<v128>(memory,bits);}
 partial_store<f32>(memory);partial_store<f64>(memory);partial_store<v128>(memory);
#if defined(UWVM_SUPPORT_MMAP)
 // Nonstandard pages must retain the generic policy. A byte-sized Wasm page
 // also needs software length checks inside a partially committed host page.
 for(auto page:{1uz,4096uz})
 {
  d::native_memory_t custom{page,uwvm2::object::memory::linear::mmap_memory_status_t::wasm64};
  auto const length=page==1?4097uz:65536uz;
  custom.init_by_page_count(length/page);
  value<f32>(custom,0x7f800001);value<f64>(custom,0x7ff0000000000001ull);value<v128>(custom,0xfedcba9876543210ull);
  partial_store<f32>(custom,length);partial_store<f64>(custom,length);partial_store<v128>(custom,length);
 }
#endif
 std::printf("PASS memory64 floating/vector handlers: %u checks, exact NaN bits, endian order, byref/tail/rings, partial-store atomicity\n",checks);
}
