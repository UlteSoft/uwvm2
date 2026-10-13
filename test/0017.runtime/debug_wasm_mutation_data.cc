// Bounded grammar/wire DATA test only. Does not authenticate a runtime stop.
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/wasm_mutation.h>
#include <fast_io.h>
#include <array>
#include <bit>
#include <cstdint>
namespace dbg=::uwvm2::uwvm::debugger;
namespace wm=dbg::wasm_mutation;
namespace ws=dbg::wasm_state;
static void require(bool good,char const* name)
{ if(!good) { ::fast_io::io::perrln("Wasm mutation DATA FAIL: ",::fast_io::mnp::os_c_str(name));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc==2) { require(::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])}=="--require-big" &&
        ::std::endian::native==::std::endian::big,"actual big endian target"); }
    else { require(argc==1,"finite argument count"); }
    auto parse=[](::fast_io::string_view text)
    {
        auto value{dbg::parse_console_command(text)};
        require(value.kind==dbg::console_command_kind::wasm_mutation && wm::valid(value.wasm_mutation_request),"actual complete mutation command");
        return value.wasm_mutation_request;
    };
    auto i32{parse("set wasm global 3 4 7 bits i32 ffffffff")};
    require(i32.source==wm::source_kind::numeric_bits&&i32.numeric_kind==ws::value_kind::i32,"i32 precise kind");
    for(::std::size_t index{};index!=i32.bits.size();++index)
    { require(i32.bits[index]==(index<4u?::std::byte{0xffu} : ::std::byte{}),"numeric width zero tail"); }
    auto f32{parse("set wasm global 3 4 7 bits f32 7fc12345")};
    require(f32.bits[0u]==::std::byte{0x45u}&&f32.bits[3u]==::std::byte{0x7fu},"FP raw NaN LE bits");
    auto i64{parse("set wasm global 3 4 7 bits i64 0123456789abcdef")};
    require(i64.bits[0u]==::std::byte{0xefu}&&i64.bits[7u]==::std::byte{0x01u},"i64 full LE bits");
    auto v{parse("set wasm global 3 4 7 bits v128 0706050403020100 0f0e0d0c0b0a0908")};
    for(::std::size_t i{};i!=16u;++i) { require(v.bits[i]==static_cast<::std::byte>(i),"v128 exact byte lanes"); }
    auto table{parse("set wasm table 3 2 4294967301 7 null")};
    require(table.target==wm::destination::table&&table.element==4294967301ull,"table64 destination remains u64");
    auto small{parse("set wasm global 3 4 7 i31 2147483647")};require(small.i31_bits==0x7fffffffu,"i31 bit31 range");
    auto f{parse("set wasm table 3 2 0 7 function 8 99")};require(f.function_module==8u&&f.function_index==99u,"logical function identity only");
    auto root{parse("set wasm global 3 4 7 from locals 2 6 path 0 1000")};
    require(root.original.selected==ws::selection::locals&&root.original.frame==2u&&root.original.first==6u&&
        root.original.path_size==2u&&root.original.path[1u]==1000u,"original local/member path");
    auto slot{parse("set wasm global 3 4 7 from table 8 2 4294967301")};
    require(slot.original.module==8u&&slot.original.index==2u&&slot.original.first==4294967301ull,"table64 source full logical index");
    auto handle{parse("set wasm global 3 4 7 from handle 101 9")};
    require(handle.source==wm::source_kind::original_path&&handle.path_session==101u&&handle.path_handle==9u&&
        handle.path_suffix_size==0u&&handle.original.long_path.empty(),"path label remains DATA with no native root");
    auto tail{parse("set wasm table 3 2 0 7 from handle 101 9 path 0 4294967301")};
    require(tail.path_suffix_size==2u&&tail.path_suffix[1u]==4294967301ull,"table64 original suffix retains u64");
    tail.path_suffix_size=17u;require(!wm::valid(tail),"fixed suffix size guard BEFORE span formation");
    tail=handle;tail.path_suffix[15u]=1u;require(!wm::valid(tail),"inactive suffix bytes must be zero");
    tail=handle;tail.original.long_path.push_back(0u);require(!wm::valid(tail),"unresolved handle cannot carry caller-owned root path");
    tail=handle;tail.original.path_session=101u;require(!wm::valid(tail),"unresolved handle cannot carry compressed original root");
    auto member{parse("set wasm member 7 globals 3 4 at 1 bits i32 1ff")};
    require(member.target==wm::destination::member&&member.element==1u&&member.target_original.member_count==1u&&
        member.target_original.module==3u&&member.target_original.first==4u&&member.numeric_kind==ws::value_kind::i32,
        "mutable member target original root and exact raw numeric bits");
    auto array{parse("set wasm member 7 table 3 2 4294967301 path 0 15 at 4294967302 null")};
    require(array.target_original.selected==ws::selection::table&&array.target_original.first==4294967301ull&&
        array.target_original.path_size==2u&&array.target_original.path[1u]==15u&&array.element==4294967302ull,
        "table64 root and immutable target member path retain u64");
    auto from{parse("set wasm member 7 saved 0 6 at 1 from locals 0 7 path 0")};
    require(from.target_original.selected==ws::selection::saved_parameters&&from.original.selected==ws::selection::locals&&
        from.original.path_size==1u,"target and source carry distinct original typed roots");
    auto dual{parse("set wasm member 7 handle 101 9 path 0 1 at 3 from handle 101 10 path 15")};
    require(dual.target==wm::destination::member_path&&dual.target_path_session==101u&&dual.target_path_handle==9u&&
        dual.target_path_suffix_size==2u&&dual.target_path_suffix[1u]==1u&&dual.source==wm::source_kind::original_path&&
        dual.path_handle==10u&&dual.path_suffix_size==1u&&dual.path_suffix[0u]==15u,"two DATA selectors remain separate and unresolved");
    auto bad_target{dual};bad_target.target_path_suffix_size=17u;require(!wm::valid(bad_target),"target suffix extent before span");
    bad_target=dual;bad_target.target_path_suffix[15u]=1u;require(!wm::valid(bad_target),"target inactive suffix zero");
    bad_target=dual;bad_target.target_original.long_path.push_back(0u);require(!wm::valid(bad_target),"target label cannot smuggle native original path");
    bad_target=member;bad_target.target_original.participant=8u;require(!wm::valid(bad_target),"target original participant must equal actual requested thread");
    bad_target=member;bad_target.target_original.member_count=0u;require(!wm::valid(bad_target),"GC target always requests exactly one original root");
    bad_target=member;bad_target.module=1u;require(!wm::valid(bad_target),"unrelated destination module label denied for member target");
    bad_target=i32;bad_target.target_original.long_path.push_back(0u);
    require(!wm::valid(bad_target),"inactive target cannot hide allocated path in scalar mutation request");
    bad_target=dual;bad_target.source=wm::source_kind::null;bad_target.original.long_path.push_back(0u);
    require(!wm::valid(bad_target),"inactive source cannot hide allocated path in target label request");
    for(auto text:{"set wasm member 0 globals 3 4 at 1 null","set wasm member 7 globals 3 4 at -1 null",
        "set wasm member 7 native 4096 at 1 null","set wasm member 7 object 1 at 1 null",
        "set wasm member 7 handle 0 1 at 1 null","set wasm member 7 handle 1 0 at 1 null",
        "set wasm member 7 handle 1 2 path at 1 null","set wasm member 7 globals 3 4 path at 1 null",
        "set wasm member 7 globals 3 4 path 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 at 1 null",
        "set wasm member 7 globals 3 4 at 18446744073709551616 null",
        "set wasm member 7 globals 3 4 1 null","set wasm member 7 globals 3 4 at 1 bits ref ffffffffffffffff",
        "set wasm member 7 globals 3 4 at 1 bits v128 1","set wasm member 7 globals 3 4 at 1 null extra"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::invalid,
        "member target invalid original locus/path/address/range/type/labels refused"); }
    for(auto text:{"set wasm global 3 4 0 null","set wasm global 3 -1 7 null","set wasm native 3 4 7 null",
        "set wasm global 3 4 7 from handle 0 9","set wasm global 3 4 7 from handle 1 0",
        "set wasm global 3 4 7 from handle -1 9","set wasm global 3 4 7 from handle 1 18446744073709551616",
        "set wasm global 3 4 7 from handle 1","set wasm global 3 4 7 from handle 1 2 path",
        "set wasm global 3 4 7 from handle 1 2 native 4096",
        "set wasm global 3 4 7 from handle 1 2 path 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "set wasm global 3 4 7 bits i32 100000000","set wasm global 3 4 7 bits i64 10000000000000000",
        "set wasm global 3 4 7 bits ref ffffffffffffffff","set wasm table 3 4 0 7 bits i32 1",
        "set wasm global 3 4 7 i31 2147483648","set wasm global 3 4 7 i31 -1",
        "set wasm global 3 4 7 function 8 99 extra","set wasm global 3 4 7 from native 0 4096",
        "set wasm global 3 4 7 from locals 0 1 path","set wasm global 3 4 7 bits v128 0",
        "set wasm global 3 4 7 from globals 0 1 path 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::invalid,"invalid width/root/path/address/overflow refused"); }
    wm::result result{};result.status=ws::status::available;result.reason=wm::refusal::immutable_global;
    auto text{wm::format(result)};
    require(text.find("applied=0")!=::std::string::npos,"explicit no-commit diagnostic");
    ::fast_io::io::println("Wasm mutation DATA PASS endian=",::std::endian::native==::std::endian::little?::fast_io::string_view{"little"} : ::fast_io::string_view{"big"}," runtime-authority=false");
}
