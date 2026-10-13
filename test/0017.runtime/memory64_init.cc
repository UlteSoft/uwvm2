#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64_bulk.h>
#include "memory64_init_cases.h"
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;namespace d=o::details;namespace test=memory64_init_test;
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false},tail{.is_tail_call=true};
void UWVM_INTERPRETER_OPFUNC_TYPE_MACRO bounds_trap(uwvm2::object::memory::error::memory_error_t const& error) noexcept
{
 if(error.memory_idx||error.memory_static_offset||error.memory_offset.offset!=test::expected_address||error.memory_offset.offset_65_bit||
    error.memory_length!=test::expected_bound||error.memory_type_size!=test::expected_length){_Exit(95);}
 test::complete_trap();
}
struct observation{std::byte* sp{};};
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*,std::byte* sp,std::byte* local) UWVM_THROWS{reinterpret_cast<observation*>(local)->sp=sp;}
template<typename T>void append(std::byte*& cursor,T value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<auto Option>
void invoke(d::native_memory_t& memory,test::storage::local_defined_data_storage_t& data,std::uint64_t dst,std::uint32_t src,std::uint32_t length)
{
 std::array<std::byte,128> operands{},code{};std::memset(operands.data(),0xa7,16);auto sp=operands.data()+16,cursor=code.data();
 append(sp,dst);append(sp,src);append(sp,length);observation out{};auto local=reinterpret_cast<std::byte*>(&out);std::byte const* ip=code.data();
 auto fn=[]
 {
  if constexpr(Option.is_tail_call){return static_cast<o::uwvm_interpreter_opfunc_t<std::byte const*,std::byte*,std::byte*>>(o::uwvmint_memory64_init<Option,std::byte const*,std::byte*,std::byte*>);}
  else{return static_cast<o::uwvm_interpreter_opfunc_byref_t<std::byte const*,std::byte*,std::byte*>>(o::uwvmint_memory64_init<Option,std::byte const*,std::byte*,std::byte*>);}
 }();
 append(cursor,fn);append(cursor,&memory);append(cursor,&data);if constexpr(Option.is_tail_call){append(cursor,&finish);}
 decltype(fn) volatile dispatch{fn};dispatch(ip,sp,local);
 if constexpr(!Option.is_tail_call){out.sp=sp;INIT_CHECK(ip==code.data()+sizeof(fn)+sizeof(&memory)+sizeof(&data));}
 INIT_CHECK(out.sp==operands.data()+16);for(unsigned i=0;i<16;++i){INIT_CHECK(operands[i]==std::byte{0xa7});}
}
int main()
{
 o::trap_memory_out_of_bounds_func=bounds_trap;
 test::cases<d::native_memory_t>(invoke<byref>);test::cases<d::native_memory_t>(invoke<tail>);
 std::printf("PASS memory64 interpreter init: %u checks; i64 destination/i32 source+length, dropped/active/empty data, high sparse address, exact diagnostics, no partial writes, byref/musttail\n",test::checks);
}
