#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64_atomic.h>
#include "memory64_wait_cases.h"
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;namespace d=o::details;namespace test=memory64_wait_test;
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false},tail{.is_tail_call=true};
void UWVM_INTERPRETER_OPFUNC_TYPE_MACRO alignment_trap() noexcept {test::complete_trap(1);}
void UWVM_INTERPRETER_OPFUNC_TYPE_MACRO bounds_trap(uwvm2::object::memory::error::memory_error_t const& error) noexcept
{
 auto sum=test::expected_offset+test::expected_address;
 if(error.memory_idx||error.memory_static_offset!=test::expected_offset||error.memory_offset.offset!=sum||
    error.memory_offset.offset_65_bit!=(sum<test::expected_address)||error.memory_length!=test::expected_length||
    error.memory_type_size!=test::expected_bytes){_Exit(95);}
 test::complete_trap(2);
}
void UWVM_INTERPRETER_OPFUNC_TYPE_MACRO wait_trap(unsigned value) noexcept
{test::complete_trap(value==static_cast<unsigned>(test::waiting::wait_status::not_shared)?3:value==static_cast<unsigned>(test::waiting::wait_status::cancelled)?4:9);}
struct observation{std::byte* sp{};};
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*,std::byte* sp,std::byte* local) UWVM_THROWS{reinterpret_cast<observation*>(local)->sp=sp;}
template<typename T>void append(std::byte*& cursor,T value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<unsigned Operation,auto Option>
unsigned invoke(d::native_memory_t& memory,std::uint64_t offset,std::uint64_t address,std::uint64_t expected,std::int64_t timeout)
{
 std::array<std::byte,128> operands{},code{};std::memset(operands.data(),0xa7,16);auto sp=operands.data()+16,cursor=code.data();
 append(sp,address);if constexpr(Operation==2){append(sp,expected);}else{append(sp,static_cast<std::uint32_t>(expected));}
 if constexpr(Operation!=0){append(sp,timeout);}observation out{};auto local=reinterpret_cast<std::byte*>(&out);std::byte const* ip=code.data();
 auto fn=[]
 {
  if constexpr(Option.is_tail_call){return static_cast<o::uwvm_interpreter_opfunc_t<std::byte const*,std::byte*,std::byte*>>(o::uwvmint_memory64_wait_notify<Operation,Option,std::byte const*,std::byte*,std::byte*>);}
  else{return static_cast<o::uwvm_interpreter_opfunc_byref_t<std::byte const*,std::byte*,std::byte*>>(o::uwvmint_memory64_wait_notify<Operation,Option,std::byte const*,std::byte*,std::byte*>);}
 }();
 append(cursor,fn);append(cursor,&memory);append(cursor,offset);if constexpr(Option.is_tail_call){append(cursor,&finish);}
 // Force real bytecode dispatch even for notify, whose body can otherwise be devirtualized away.
 decltype(fn) volatile dispatch{fn};dispatch(ip,sp,local);
 if constexpr(!Option.is_tail_call){out.sp=sp;WAIT_CHECK(ip==code.data()+sizeof(fn)+sizeof(&memory)+sizeof(offset));}
 WAIT_CHECK(out.sp==operands.data()+20);for(unsigned i=0;i<16;++i){WAIT_CHECK(operands[i]==std::byte{0xa7});}
 std::uint32_t result{};std::memcpy(&result,operands.data()+16,4);return result;
}
template<auto Option>void run(){test::cases<d::native_memory_t>([]<unsigned Operation>(auto& memory,auto offset,auto address,auto expected,auto timeout){return invoke<Operation,Option>(memory,offset,address,expected,timeout);});}
int main()
{
 o::trap_unaligned_atomic_func=alignment_trap;o::trap_memory_out_of_bounds_func=bounds_trap;o::trap_atomic_wait_func=wait_trap;
 run<byref>();run<tail>();std::printf("PASS memory64 interpreter wait/notify: %u checks; byref/musttail, exact diagnostics; shared policies also cover high addresses, growth, cancellation\n",test::checks);
}
