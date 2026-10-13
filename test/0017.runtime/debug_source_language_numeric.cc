// Finite source language/type/value DATA; no VM stop or read capability.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_frames.h>
#include <fast_io.h>
#include <type_traits>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
namespace frames=uwvm2::uwvm::debugger::source_frames;
using language=scalar::language_semantics;
static ::std::size_t checks{};
static void check(bool ok,char const* why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("language numeric FAIL: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static void verify(::std::string_view text,language lang,::std::uint64_t bits,unsigned width,bool uns,
    scalar::value_category category=scalar::value_category::unspecified)
{
    for(unsigned guest:{32u,64u})
    {
        scalar::program code{};scalar::integer out{};::std::size_t reads{};
        auto value{[&](dwarf::source_expression const&,bool,scalar::integer&) { ++reads;return false; }};
        check(scalar::parse(text,code)==scalar::error::none,"syntax");
        check(scalar::evaluate(code,value,out,guest,scalar::details::no_type_resolver{},lang)==scalar::error::none,"evaluation");
        check(out.bits==bits && out.width==width && out.unsigned_value==uns && !out.floating && out.category==category && reads==0u,"compiler result and no value reads");
    }
}
template<typename T> static constexpr char const* name()
{
    if constexpr(::std::is_same_v<T,bool>) { return "bool"; }
    else if constexpr(::std::is_same_v<T,signed char>) { return "signed char"; }
    else if constexpr(::std::is_same_v<T,unsigned char>) { return "unsigned char"; }
    else if constexpr(::std::is_same_v<T,short>) { return "short"; }
    else { return "unsigned short"; }
}
template<typename T> static constexpr auto data(T x)
{ return scalar::from_dwarf_numeric(::std::is_same_v<T,bool> ? dwarf::numeric_kind::boolean :
    ::std::is_unsigned_v<T> ? dwarf::numeric_kind::unsigned_integer : dwarf::numeric_kind::signed_integer,
    static_cast<::std::uint64_t>(x),sizeof(T)*8u); }
template<typename T> static constexpr ::std::array<T,5u> samples()
{
    if constexpr(::std::is_same_v<T,bool>) { return {false,true,false,true,true}; }
    else if constexpr(::std::is_same_v<T,signed char>) { return {-128,-1,0,1,127}; }
    else if constexpr(::std::is_same_v<T,unsigned char>) { return {0,1,127,128,255}; }
    else if constexpr(::std::is_same_v<T,short>) { return {-32768,-1,0,1,32767}; }
    else { return {0,1,32767,32768,65535}; }
}
template<typename T> static void same_narrow()
{
    // Independent C17/C23 _Generic compiler oracle accompanies these rows.
    // C++ same integer representation deliberately has no exact-type proof.
    static_assert(::std::is_same_v<decltype(true ? static_cast<T>(0) : static_cast<T>(0)),T>);
    for(auto a:samples<T>()) { for(auto b:samples<T>()) { for(bool yes:{false,true}) { for(unsigned guest:{32u,64u})
    {
        auto const expected{yes ? static_cast<int>(a) : static_cast<int>(b)};
        auto const text{::fast_io::concat_std(yes ? "1 ? (" : "0 ? (",::fast_io::mnp::os_c_str(name<T>()),")",static_cast<long long>(a),
            " : (",::fast_io::mnp::os_c_str(name<T>()),")",static_cast<long long>(b))};
        scalar::program code{};scalar::integer out{};::std::size_t reads{},queries{};
        auto values{[&](dwarf::source_expression const& leaf,bool size,scalar::integer& result)
        { ++reads;if(size) { return false; }if(leaf.root_name==(yes ? "left" : "right")) { result=data(yes ? a : b);return true; }return false; }};
        auto types{[&](dwarf::source_expression const& leaf,scalar::integer& result)
        { ++queries;if(leaf.root_name!="left" && leaf.root_name!="right") { return false; }result=data(T{});return true; }};
        check(scalar::parse(text,code)==scalar::error::none,"same typed syntax");
        check(scalar::evaluate(code,values,out,guest,types,language::c)==scalar::error::none &&
            out.bits==(static_cast<::std::uint64_t>(expected)&0xffffffffu) && out.width==32u && !out.unsigned_value && reads==0u,"C same narrow arithmetic promotion");
        auto const status{scalar::evaluate(code,values,out,guest,types,language::cpp)};
        if constexpr(::std::is_same_v<T,bool>)
        { check(status==scalar::error::none && out.bits==static_cast<unsigned>(expected) && out.width==8u && out.category==scalar::value_category::boolean,"C++ bool retains bool"); }
        else { check(status==scalar::error::none && out.width==sizeof(T)*8u && out.bits==(static_cast<::std::uint64_t>(yes ? a : b)&scalar::details::mask(sizeof(T)*8u)) && out.unsigned_value==::std::is_unsigned_v<T>,"explicit same builtin cast identity retains narrow type"); }
        check(scalar::parse(yes ? "1 ? left : right" : "0 ? left : right",code)==scalar::error::none,"declared same narrow syntax");
        check(scalar::evaluate(code,values,out,guest,types,language::c)==scalar::error::none &&
            out.bits==(static_cast<::std::uint64_t>(expected)&0xffffffffu) && out.width==32u && !out.unsigned_value && reads==1u && queries==2u,"C declared type queries and selected copy only");
        reads=0u;queries=0u;
        auto const cpp{scalar::evaluate(code,values,out,guest,types,language::cpp)};
        if constexpr(::std::is_same_v<T,bool>)
        { check(cpp==scalar::error::none && out.bits==static_cast<unsigned>(expected) && out.width==8u && out.unsigned_value &&
            out.category==scalar::value_category::boolean && reads==1u && queries==2u,"C++ copied bool value projection"); }
        else { check(cpp==scalar::error::unsupported && reads==0u && queries==2u && out.bits==0u,"unsupported native identity before reads"); }
    } } } }
}
int main(int argc,char** argv)
{
    if(argc>=3)
    {
        auto lang{::std::string_view{argv[1]}=="cpp" ? language::cpp : ::std::string_view{argv[1]}=="c" ? language::c : language::shared_numeric};
        for(int i{2};i<argc;++i)
        {
            scalar::program code{};scalar::integer out{};auto value{[](dwarf::source_expression const&,bool,scalar::integer&) { return false; }};
            auto p{scalar::parse(::std::string_view{argv[i]},code)};auto e{p==scalar::error::none ? scalar::evaluate(code,value,out,32u,scalar::details::no_type_resolver{},lang) : p};
            ::fast_io::io::println(static_cast<unsigned>(p),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",static_cast<unsigned>(out.category));
        }
        return 0;
    }
    same_narrow<bool>();same_narrow<signed char>();same_narrow<unsigned char>();same_narrow<short>();same_narrow<unsigned short>();
    for(auto lang:{language::c,language::cpp})
    {
        auto const width{lang==language::cpp ? 8u : 32u};auto const category{lang==language::cpp ? scalar::value_category::boolean : scalar::value_category::unspecified};
        bool const uns{lang==language::cpp};
        for(int a{-4};a<=4;++a) { for(int b{-4};b<=4;++b)
        {
            auto text{[&](char const* op) { return ::fast_io::concat_std(a," ",::fast_io::mnp::os_c_str(op)," ",b); }};
            verify(text("<"),lang,a<b,width,uns,category);verify(text("<="),lang,a<=b,width,uns,category);
            verify(text(">"),lang,a>b,width,uns,category);verify(text(">="),lang,a>=b,width,uns,category);
            verify(text("=="),lang,a==b,width,uns,category);verify(text("!="),lang,a!=b,width,uns,category);
            verify(text("&&"),lang,a && b,width,uns,category);verify(text("||"),lang,a || b,width,uns,category);
        } }
        verify("!0",lang,1u,width,uns,category);verify("!1.0",lang,0u,width,uns,category);
        verify("1.0 < 2.0",lang,1u,width,uns,category);verify("1.0f == 2.0f",lang,0u,width,uns,category);
        verify(lang==language::shared_numeric ? "0 && absent" : "0 && (1/0)",lang,0u,width,uns,category);verify(lang==language::shared_numeric ? "1 || absent" : "1 || (1/0)",lang,1u,width,uns,category);
        verify("1 ? true : false",lang,1u,width,uns,category);
        verify("1 ? !false : false",lang,1u,width,uns,category);
        verify("0 ? true : (false || true)",lang,1u,width,uns,category);
        verify("1 ? (1.0 != 2.0) : true",lang,1u,width,uns,category);
        verify("1 ? true : 1/0",lang,1u,32u,false);
        verify("1 ? (0 ? true : false) : true",lang,0u,width,uns,category);
    }
    for(unsigned guest:{32u,64u})
    {
        ::std::size_t reads{};auto values{[&](dwarf::source_expression const&,bool,scalar::integer&) { ++reads;return false; }};
        auto type{[](dwarf::source_expression const& leaf,scalar::integer& out)
        { if(leaf.root_name=="unknown8") { out={0u,8u,true};return true; }
          if(leaf.root_name=="bool32") { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,0u,32u);return true; }return false; }};
        for(auto lang:{language::shared_numeric,language::c,language::cpp})
        {
            scalar::program code{};scalar::integer out{};check(scalar::parse("1 ? unknown8 : (unsigned char)1",code)==scalar::error::none,"unclassified syntax");
            check(scalar::evaluate(code,values,out,guest,type,lang)==scalar::error::unsupported && reads==0u && out.bits==0u,"no native promotion proof from unclassified byte");
        }
        scalar::program code{};scalar::integer out{};check(scalar::parse("1 ? bool32 : true",code)==scalar::error::none,"wide Boolean metadata syntax");
        check(scalar::evaluate(code,values,out,guest,type,language::cpp)==scalar::error::unsupported && reads==0u,"C++ bool extent must match guest bool");
        check(scalar::parse("left",code)==scalar::error::none,"invalid profile syntax");
        check(scalar::evaluate(code,values,out,guest,type,static_cast<language>(255))==scalar::error::unavailable && reads==0u && out.bits==0u,"invalid profile rejected before read");
    }
    for(auto code:{0x01u,0x02u,0x0cu,0x10u,0x1du,0x2cu}) { check(scalar::language_from_dwarf(code,false)==language::c,"C/ObjC language registry"); }
    for(auto code:{0x04u,0x11u,0x19u,0x1au,0x21u,0x2au,0x2bu,0x3au}) { check(scalar::language_from_dwarf(code,false)==language::cpp,"C++/ObjC++ language registry"); }
    for(auto code:{0u,0xffffu}) { check(scalar::language_from_dwarf(code,false)==language::shared_numeric,"unknown and other language families remain shared"); }
    check(scalar::language_from_dwarf(0x0cu,true)==language::go,"TinyGo C99 CU is Go");
    ::std::vector<dwarf::scope_record> scopes(4u);
    scopes[0].kind=dwarf::scope_kind::compile_unit;scopes[0].identity={0u,0u};
    scopes[1].kind=dwarf::scope_kind::subprogram;scopes[1].identity={0u,10u};scopes[1].parent=0u;scopes[1].concrete=true;scopes[1].ranges.push_back({1u,40u});scopes[1].language=0x1du;
    scopes[2].kind=dwarf::scope_kind::inline_subprogram;scopes[2].identity={0u,20u};scopes[2].parent=1u;scopes[2].concrete=true;scopes[2].ranges.push_back({5u,25u});scopes[2].language=0x1au;
    scopes[3].kind=dwarf::scope_kind::inline_subprogram;scopes[3].identity={0u,30u};scopes[3].parent=2u;scopes[3].concrete=true;scopes[3].ranges.push_back({10u,15u});scopes[3].language=0x0cu;scopes[3].tinygo_producer=true;
    for(::std::size_t i{};i!=3u;++i)
    { frames::language_context out{};check(frames::current_language(scopes,12u,i,out)==frames::error::none && out.language==scopes[3u-i].language && out.tinygo_producer==scopes[3u-i].tinygo_producer,"language belongs to selected concrete frame"); }
    frames::language_context context{99u,true};check(frames::current_language(scopes,12u,3u,context)==frames::error::bounds && context.language==0u,"no caller ordinal language substitution");
    scopes[3].language=0u;check(frames::current_language(scopes,12u,0u,context)==frames::error::unavailable && context.language==0u,"missing CU does not inherit different frame");
    scopes[3].parent=3u;check(frames::current_language(scopes,12u,0u,context)==frames::error::malformed && context.language==0u,"malformed active scope chain refused");
    ::fast_io::io::println("debug_source_language_numeric: PASS checks=",checks," profiles=C,CPP,shared guest_widths=2");
}
