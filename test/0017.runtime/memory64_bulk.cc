#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/memory64_bulk.h>
#include "memory64_bulk_cases.h"
namespace o=uwvm2::runtime::compiler::uwvm_int::optable;
namespace d=o::details;
namespace test=memory64_bulk_test;
namespace wide=uwvm2::runtime::compiler::shared::wasm_memory64;
constexpr o::uwvm_interpreter_translate_option_t byref{.is_tail_call=false},tail{.is_tail_call=true};
static_assert(wide::range_valid(std::uint32_t{0xffffffff},0xffffffff,0));
static_assert(!wide::range_valid(std::uint32_t{0xffffffff},0x100000000,0));
static_assert(!wide::range_valid(std::uint32_t{0xffffffff},0,0x100000000));
static_assert(!wide::range_valid(std::uint64_t{~0ull},~0ull,1));
static_assert(wide::range_valid(std::uint64_t{~0ull},0,~0ull));
struct observation{std::byte* sp{};};
UWVM_INTERPRETER_OPFUNC_HOT_MACRO void finish(std::byte const*,std::byte* sp,std::byte* local) UWVM_THROWS
{reinterpret_cast<observation*>(local)->sp=sp;}
template<typename T>void append(std::byte*& cursor,T value){std::memcpy(cursor,&value,sizeof(value));cursor+=sizeof(value);}
template<bool Wide>void operand(std::byte*& cursor,std::uint64_t value)
{if constexpr(Wide){append(cursor,value);}else{append(cursor,static_cast<std::uint32_t>(value));}}
template<bool Copy,bool Destination64,bool Source64,auto Option>
void invoke(d::native_memory_t& destination,d::native_memory_t& source,std::uint64_t dst,std::uint64_t src,std::uint64_t n)
{
 std::array<std::byte,128> operands{};std::memset(operands.data(),0xa7,16);auto sp=operands.data()+16;
 operand<Destination64>(sp,dst);operand<Copy&&Source64>(sp,src);operand<Copy?(Destination64&&Source64):Destination64>(sp,n);
 observation out{};auto local=reinterpret_cast<std::byte*>(&out);
 std::array<std::byte,64> code{};auto cursor=code.data();std::byte const* ip=code.data();
 auto fn=o::translate::get_uwvmint_memory64_bulk_fptr_from_tuple<Copy,Destination64,Source64,Option>(
  uwvm2::utils::container::tuple<std::byte const*,std::byte*,std::byte*>{});
 append(cursor,fn);append(cursor,&destination);if constexpr(Copy){append(cursor,&source);}
 if constexpr(Option.is_tail_call){append(cursor,&finish);}
 fn(ip,sp,local);
 if constexpr(!Option.is_tail_call)
 {out.sp=sp;BULK_CHECK(ip==code.data()+sizeof(fn)+sizeof(&destination)*(Copy?2:1));}
 BULK_CHECK(out.sp==operands.data()+16);for(unsigned i=0;i<16;++i){BULK_CHECK(operands[i]==std::byte{0xa7});}
}
template<bool Copy,bool Destination64,bool Source64,auto Option>void configuration()
{
 d::native_memory_t destination{},source{};test::initialize(destination);test::initialize(source);
 auto fn=invoke<Copy,Destination64,Source64,Option>;
 test::cases<Copy,Destination64,Source64>(destination,source,fn);
 test::high_addresses<Copy,Destination64,Source64,d::native_memory_t>(fn);
}
template<auto Option>void all()
{
 configuration<true,false,false,Option>();configuration<true,false,true,Option>();
 configuration<true,true,false,Option>();configuration<true,true,true,Option>();
 configuration<false,false,false,Option>();configuration<false,true,false,Option>();
}
int main()
{
 all<byref>();all<tail>();
 test::concurrent_grow<d::native_memory_t>(invoke<true,true,true,tail>);
 std::printf("PASS memory64 bulk interpreter: %u checks, mixed address widths, zero endpoints, high sparse addresses, overlap, no partial mutation\n",test::checks);
}
