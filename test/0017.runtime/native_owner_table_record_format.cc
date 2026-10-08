// Independent literal wire DATA model. Values are unrelocated test integers,
// never loaded addresses, VM captures, generated machine code or permissions.
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_table_format.h>
#include <array>
#include <cstdint>
#include <span>
namespace wire = ::uwvm2::runtime::lib::details::native_owner_table_format;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("native owner record DATA FAIL line=",__LINE__); return 1; } } while(false)
constexpr ::std::array<unsigned char,48u> little32{
 'U','W','V','M','N','E','2',0, 2,4,1,1,1,0,0,0,
 3,0,0,0, 3,0,0,0, 0,0,0,0, 48,0,0,0,
 0x78,0x56,0x34,0x12, 0xF0,0xDE,0xBC,0x9A,
 'a','b','c','d','e','f',0,0};
constexpr ::std::array<unsigned char,48u> big32{
 'U','W','V','M','N','E','2',0, 2,4,2,1,2,0,0,0,
 0,0,0,3, 0,0,0,3, 0,0,0,0, 0,0,0,48,
 0x12,0x34,0x56,0x78, 0x9A,0xBC,0xDE,0xF0,
 'a','b','c','d','e','f',0,0};
constexpr ::std::array<unsigned char,56u> little64{
 'U','W','V','M','N','E','2',0, 2,8,1,1,3,0,0,0,
 3,0,0,0, 3,0,0,0, 0,0,0,0, 56,0,0,0,
 0xEF,0xCD,0xAB,0x89,0x67,0x45,0x23,0x01,
 0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE,
 'a','b','c','d','e','f',0,0};
constexpr ::std::array<unsigned char,56u> big64{
 'U','W','V','M','N','E','2',0, 2,8,2,1,0,0,0,0,
 0,0,0,3, 0,0,0,3, 0,0,0,0, 0,0,0,56,
 0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
 0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,
 'a','b','c','d','e','f',0,0};
template<::std::size_t N>
static int exercise(::std::array<unsigned char,N> const& input, unsigned pointer, bool little,
    unsigned role, ::std::uint64_t first, ::std::uint64_t last)
{
    wire::record row{};
    CHECK(wire::decode(input,pointer,little,row));
    CHECK(row.bytes==N && row.pointer_bytes==pointer && row.byte_order==(little ? 1u:2u) &&
          row.continuous_shape==1u && row.role==role && row.original_ir_name=="abc" &&
          row.entry_object_name=="def" && row.local_entry_object_name.empty() &&
          row.unrelocated_begin_bits==first && row.unrelocated_end_bits==last &&
          row.begin_relocation_offset==32u && row.end_relocation_offset==32u+pointer);
    for(::std::size_t n{}; n<N; ++n)
    {
        row.original_ir_name="prior";row.entry_object_name="prior-entry";row.bytes=123u;
        // [one owned complete literal array][n<N]
        // [safe] every truncated view stays within this same literal array.
        CHECK(!wire::decode(::std::span<unsigned char const>{input.data(),n},pointer,little,row));
        CHECK(row.bytes==0u && row.original_ir_name.empty() && row.entry_object_name.empty());
    }
    CHECK(!wire::decode(input,pointer,!little,row));
    CHECK(!wire::decode(input,pointer==4u ? 8u:4u,little,row));
    for(::std::size_t index: ::std::array<::std::size_t,11u>{0u,7u,8u,9u,10u,11u,12u,13u,14u,15u,N-1u})
    { auto changed{input};changed[index]=0xFFu;CHECK(!wire::decode(changed,pointer,little,row)); }
    for(::std::size_t index{16u};index<32u;++index)
    { auto changed{input};changed[index]=0xFFu;CHECK(!wire::decode(changed,pointer,little,row)); }
    auto nul_ir{input};nul_ir[32u+2u*pointer+1u]=0u;CHECK(!wire::decode(nul_ir,pointer,little,row));
    auto nul_entry{input};nul_entry[32u+2u*pointer+4u]=0u;CHECK(!wire::decode(nul_entry,pointer,little,row));
    ::std::size_t cursor{N};::std::uint32_t bits{0xA5A5A5A5u};
    CHECK(!wire::get<32u>(input,cursor,1u,bits) && cursor==N && bits==0xA5A5A5A5u);
    cursor=N+1u;CHECK(!wire::get<32u>(input,cursor,1u,bits) && cursor==N+1u && bits==0xA5A5A5A5u);
    cursor=16u;CHECK(!wire::get<32u>(input,cursor,0u,bits) && cursor==16u && bits==0xA5A5A5A5u);
    return 0;
}
int main()
{
    CHECK(exercise(little32,4u,true,1u,0x12345678u,0x9ABCDEF0u)==0);
    CHECK(exercise(big32,4u,false,2u,0x12345678u,0x9ABCDEF0u)==0);
    CHECK(exercise(little64,8u,true,3u,0x0123456789ABCDEFu,0xFEDCBA9876543210u)==0);
    CHECK(exercise(big64,8u,false,0u,0x0123456789ABCDEFu,0xFEDCBA9876543210u)==0);
    constexpr ::std::array<unsigned char,52u> local_entry{
     'U','W','V','M','N','E','2',0, 2,4,1,1,1,0,0,0,
     3,0,0,0, 3,0,0,0, 3,0,0,0, 52,0,0,0,
     0,0,0,0, 8,0,0,0, 'a','b','c','d','e','f','l','o','c',0,0,0};
    wire::record row{};
    CHECK(wire::decode(local_entry,4u,true,row) && row.original_ir_name=="abc" &&
          row.entry_object_name=="def" && row.local_entry_object_name=="loc" && row.bytes==52u);
    auto equal_local{local_entry};equal_local[46u]='d';equal_local[47u]='e';equal_local[48u]='f';
    CHECK(!wire::decode(equal_local,4u,true,row));
    auto nul_local{local_entry};nul_local[47u]=0u;CHECK(!wire::decode(nul_local,4u,true,row));
    constexpr ::std::array<unsigned char,40u> unknown{
     'U','W','V','M','N','E','2',0, 2,4,1,0,0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 40,0,0,0, 0,0,0,0, 0,0,0,0};
    CHECK(wire::decode(unknown,4u,true,row) && row.bytes==40u && row.continuous_shape==0u &&
          row.role==0u && row.original_ir_name.empty() && row.entry_object_name.empty() && row.local_entry_object_name.empty());
    auto falsely_typed{unknown};falsely_typed[12u]=1u;CHECK(!wire::decode(falsely_typed,4u,true,row));
    auto falsely_continuous{unknown};falsely_continuous[11u]=1u;CHECK(!wire::decode(falsely_continuous,4u,true,row));
    ::fast_io::io::println("PASS owner record DATA v2 pointer32/64 little/big literal + actual-name/local-name + truncation checks=",
        ::fast_io::mnp::dec(checks)," loaded-ownership-qualified=false");
}
#undef CHECK
