#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;
namespace d=o::details;
using i64=d::wasm_i64;using slot=o::wasm_stack_top_i32_with_i64_u;
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();} } while(false)
unsigned checks{};
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false},tail{.is_tail_call=true};
constexpr o::uwvm_interpreter_translate_option_t ring1{.is_tail_call=true,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=4};
constexpr o::uwvm_interpreter_translate_option_t ring2{.is_tail_call=true,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
constexpr o::uwvm_interpreter_translate_option_t merged{.is_tail_call=true,
 .i32_stack_top_begin_pos=3,.i32_stack_top_end_pos=5,.i64_stack_top_begin_pos=3,.i64_stack_top_end_pos=5};
struct observation{std::byte* sp{};std::uint64_t bits{};};
template<auto O,std::size_t Pos,typename... T>
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(T... args) UWVM_THROWS
{
 auto& out=*reinterpret_cast<observation*>(args...[2]);out.sp=args...[1];
 auto value=o::get_curr_val_from_operand_stack_top<O,i64,Pos>(args...);std::memcpy(&out.bits,&value,sizeof(value));
}
template<typename T>void append(std::byte*& cursor,T value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<bool Grow,auto O,typename... Slots>
std::uint64_t invoke(d::native_memory_t& memory,std::uint64_t delta,Slots... slots)
{
 constexpr auto begin=O.i64_stack_top_begin_pos,end=O.i64_stack_top_end_pos;
 constexpr auto output=begin==end?0:Grow?begin:end-1;
 std::array<std::byte,128> operands{};std::memset(operands.data(),0xa7,16);
 auto sp=operands.data()+16;observation out{};auto local=reinterpret_cast<std::byte*>(&out);
 std::array<std::byte,128> code{};auto cursor=code.data();std::byte const* ip=code.data();
 if constexpr(Grow)
 {
  auto value=std::bit_cast<i64>(delta);
  if constexpr(begin!=end){d::set_curr_val_to_stacktop_cache<O,i64,begin>(value,ip,sp,local,slots...);}
  else {append(sp,value);}
 }
 o::uwvm_interpreter_stacktop_currpos_t positions{};positions.i64_stack_top_curr_pos=begin;
 auto fn=o::translate::get_uwvmint_memory64_pages_fptr_from_tuple<Grow,O>(positions,
  uwvm2::utils::container::tuple<std::byte const*,std::byte*,std::byte*,Slots...>{});
 append(cursor,fn);append(cursor,&memory);
 if constexpr(Grow){append(cursor,std::size_t{4*65536});}
 if constexpr(O.is_tail_call){append(cursor,&finish<O,output,std::byte const*,std::byte*,std::byte*,Slots...>);}
 fn(ip,sp,local,slots...);
 if constexpr(!O.is_tail_call)
 {
  out.sp=sp;std::memcpy(&out.bits,sp-sizeof(i64),sizeof(i64));
  CHECK(ip==code.data()+sizeof(fn)+sizeof(&memory)+(Grow?sizeof(std::size_t):0));
 }
 CHECK(out.sp==operands.data()+16+(begin==end?sizeof(i64):0));
 for(unsigned i=0;i<16;++i){CHECK(operands[i]==std::byte{0xa7});}
 ++checks;return out.bits;
}
template<typename Memory>void initialize(Memory& memory)
{
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(Memory::can_mmap){memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;}
#endif
 memory.init_by_page_count(1);
}
template<auto O,typename... Slots>void configuration(Slots... slots)
{
 d::native_memory_t memory{};initialize(memory);memory.memory_begin[127]=std::byte{0x5a};
 CHECK((invoke<false,O>(memory,0,slots...)==1));CHECK((invoke<true,O>(memory,0,slots...)==1));
 CHECK((invoke<true,O>(memory,1,slots...)==1));CHECK((invoke<false,O>(memory,0,slots...)==2));
 CHECK(memory.memory_begin[127]==std::byte{0x5a});
 for(unsigned i=65536;i<65536+128;++i){CHECK(memory.memory_begin[i]==std::byte{});}
 CHECK((invoke<true,O>(memory,2,slots...)==2));
 for(auto delta:{1ull,0xffffffffull,0x100000000ull,0x100000001ull,1ull<<48,~0ull})
 {CHECK((invoke<true,O>(memory,delta,slots...)==~0ull));CHECK((invoke<false,O>(memory,0,slots...)==4));}
 CHECK((invoke<true,O>(memory,0,slots...)==4));
 // A missing explicit maximum still must reject a delta wider than native
 // size_t/page bytes before reaching the fail-fast host allocation policy.
 auto got=uwvm2::runtime::compiler::shared::wasm_memory64::grow(memory,SIZE_MAX,~0ull,uwvm2::object::memory::flags::grow_strict);
 CHECK(got==~0ull);++checks;
}
template<typename Memory>void concurrent(bool strict)
{
 if constexpr(Memory::support_multi_thread)
 {
  Memory memory{};initialize(memory);
  std::array<std::thread,8> workers{};std::array<std::atomic<unsigned>,32> seen{};
  for(auto& worker:workers)
  {
   worker=std::thread([&]{for(unsigned i=0;i<8;++i)
   {
    auto result=uwvm2::runtime::compiler::shared::wasm_memory64::grow(memory,32*65536,1,strict);
    if(result!=~0ull){CHECK(result>0&&result<32);seen[result].fetch_add(1,std::memory_order_relaxed);}
   }});
  }
  for(auto& worker:workers){worker.join();}
  CHECK(memory.get_page_size()==32);
  for(unsigned i=1;i<32;++i){CHECK(seen[i].load()==1);}
  ++checks;
 }
}
namespace wide=uwvm2::runtime::compiler::shared::wasm_memory64;
static_assert(wide::maximum_bytes_from_pages<std::uint32_t>(65535,16)==0xffff0000u);
static_assert(wide::maximum_bytes_from_pages<std::uint32_t>(65536,16)==0xffffffffu);
static_assert(wide::maximum_bytes_from_pages<std::uint32_t>(0x100000001ull,16)==0xffffffffu);
static_assert(wide::maximum_bytes_from_pages<std::uint64_t>(0x100000001ull,16)==0x1000000010000ull);
static_assert(wide::maximum_bytes_from_pages<std::uint64_t>(1ull<<48,16)==~0ull);
static_assert(wide::maximum_bytes_from_pages<std::uint32_t>(1,32)==0xffffffffu);
int main()
{
 for(bool strict:{false,true})
 {
  uwvm2::object::memory::flags::grow_strict=strict;
  configuration<byref>();configuration<tail>();configuration<ring1>(i64{});configuration<ring2>(i64{},i64{});configuration<merged>(slot{},slot{});
  concurrent<d::native_memory_t>(strict);
 }
 std::printf("PASS memory64 size/grow: %u checks, i64 results, high deltas, byte preservation, zero growth, ring dispatch, concurrent old-size publication\n",checks);
}
