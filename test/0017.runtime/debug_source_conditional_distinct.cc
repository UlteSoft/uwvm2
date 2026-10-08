// Numeric type/value DATA only; this fixture grants no stopped VM authority.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
#include <type_traits>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{};
static void check(bool ok,char const* why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("distinct conditional FAIL: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
template<typename T> static constexpr char const* name()
{
    if constexpr(::std::is_same_v<T,bool>) { return "bool"; }
    else if constexpr(::std::is_same_v<T,signed char>) { return "signed char"; }
    else if constexpr(::std::is_same_v<T,unsigned char>) { return "unsigned char"; }
    else if constexpr(::std::is_same_v<T,short>) { return "short"; }
    else { return "unsigned short"; }
}
template<typename T> static constexpr auto data(T x)
{
    return scalar::from_dwarf_numeric(::std::is_same_v<T,bool> ? dwarf::numeric_kind::boolean :
        ::std::is_unsigned_v<T> ? dwarf::numeric_kind::unsigned_integer : dwarf::numeric_kind::signed_integer,
        static_cast<::std::uint64_t>(x),sizeof(T)*8u);
}
template<typename T> static constexpr ::std::array<T,5u> samples()
{
    if constexpr(::std::is_same_v<T,bool>) { return {false,true,false,true,true}; }
    else if constexpr(::std::is_same_v<T,signed char>) { return {-128,-1,0,1,127}; }
    else if constexpr(::std::is_same_v<T,unsigned char>) { return {0,1,127,128,255}; }
    else if constexpr(::std::is_same_v<T,short>) { return {-32768,-1,0,1,32767}; }
    else { return {0,1,32767,32768,65535}; }
}
template<typename A,typename B> static void matrix()
{
    static_assert(!::std::is_same_v<A,B>);
    static_assert(::std::is_same_v<decltype(true ? static_cast<A>(0) : static_cast<B>(0)),int>);
    for(auto a:samples<A>()) { for(auto b:samples<B>()) { for(bool yes:{false,true}) { for(unsigned guest:{32u,64u})
    {
        auto const expected{yes ? a : b};
        auto const text{::fast_io::concat_std(yes ? "1 ? (" : "0 ? (",::fast_io::mnp::os_c_str(name<A>()),")",
            static_cast<long long>(a)," : (",::fast_io::mnp::os_c_str(name<B>()),")",static_cast<long long>(b))};
        scalar::program code{};scalar::integer out{};
        auto no_values{[](dwarf::source_expression const&,bool,scalar::integer&) { return false; }};
        check(scalar::parse(text,code)==scalar::error::none,"typed syntax");
        check(scalar::evaluate(code,no_values,out,guest)==scalar::error::none,"typed evaluation");
        check(out.bits==(static_cast<::std::uint64_t>(expected)&0xffffffffu) && out.width==32u &&
            !out.unsigned_value && !out.floating,"compiler native prvalue type and value equivalence");
        ::std::size_t left_reads{},right_reads{},type_queries{};
        auto metadata{[&](dwarf::source_expression const& leaf,scalar::integer& value)
        { ++type_queries;if(leaf.root_name=="left") { value=data(A{});return true; }
          if(leaf.root_name=="right") { value=data(B{});return true; }return false; }};
        auto values{[&](dwarf::source_expression const& leaf,bool size,scalar::integer& value)
        { if(size) { return false; }if(leaf.root_name=="left") { ++left_reads;value=data(a);return yes; }
          if(leaf.root_name=="right") { ++right_reads;value=data(b);return !yes; }return false; }};
        check(scalar::parse(yes ? "1 ? left : right" : "0 ? left : right",code)==scalar::error::none,"DWARF declaration syntax");
        check(scalar::evaluate(code,values,out,guest,metadata)==scalar::error::none,"declared numeric evaluation");
        check(out.bits==(static_cast<::std::uint64_t>(expected)&0xffffffffu) && out.width==32u &&
            !out.unsigned_value && !out.floating && left_reads==static_cast<unsigned>(yes) &&
            right_reads==static_cast<unsigned>(!yes) && type_queries==2u,"both declaration queries; selected value only");
    } } } }
}
int main(int argc,char** argv)
{
    if(argc>1)
    {
        for(int i{1};i<argc;++i)
        {
            scalar::program code{};scalar::integer out{};auto values{[](dwarf::source_expression const&,bool,scalar::integer&) { return false; }};
            auto const p{scalar::parse(::std::string_view{argv[i]},code)};
            auto const e{p==scalar::error::none ? scalar::evaluate(code,values,out) : p};
            ::fast_io::io::println(static_cast<unsigned>(p),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width);
        }
        return 0;
    }
    matrix<bool,signed char>();
    matrix<bool,unsigned char>();
    matrix<bool,short>();
    matrix<bool,unsigned short>();
    matrix<signed char,bool>();
    matrix<signed char,unsigned char>();
    matrix<signed char,short>();
    matrix<signed char,unsigned short>();
    matrix<unsigned char,bool>();
    matrix<unsigned char,signed char>();
    matrix<unsigned char,short>();
    matrix<unsigned char,unsigned short>();
    matrix<short,bool>();
    matrix<short,signed char>();
    matrix<short,unsigned char>();
    matrix<short,unsigned short>();
    matrix<unsigned short,bool>();
    matrix<unsigned short,signed char>();
    matrix<unsigned short,unsigned char>();
    matrix<unsigned short,short>();
    for(unsigned guest:{32u,64u})
    {
        ::std::size_t reads{};
        auto values{[&](dwarf::source_expression const&,bool,scalar::integer& out)
        { ++reads;out=scalar::from_dwarf_numeric(dwarf::numeric_kind::unsigned_integer,1u,8u);return true; }};
        auto metadata{[](dwarf::source_expression const& leaf,scalar::integer& out)
        {
            if(leaf.root_name=="unknown8") { out={0u,8u,true};return true; }
            if(leaf.root_name=="known16") { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,0u,16u);return true; }
            if(leaf.root_name=="bool32") { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,0u,32u);return true; }
            return false;
        }};
        for(auto text:{"1 ? (char)1 : (char)2","1 ? (signed char)1 : (char)2","1 ? (short)1 : (short)2",
            "1 ? true : false","1 ? (bool)1 : (bool)0","1 ? true : bool32","1 ? bool32 : (bool)0",
            "1 ? unknown8 : known16","1 ? true : unknown8"})
        {
            scalar::program code{};scalar::integer out{99u,64u,true};check(scalar::parse(text,code)==scalar::error::none,"ambiguous syntax");
            auto before{reads};check(scalar::evaluate(code,values,out,guest,metadata)==scalar::error::unsupported &&
                reads==before && out.bits==0u,"same-type and unclassified narrow rejection before values");
        }
        for(auto text:{"1 ? (signed char)-1 : (short)(1/0)","0 ? (short)(1/0) : (unsigned char)255",
            "1 ? (0 ? (signed char)1 : (short)2) : 0","0 ? 0 : (1 ? true : (unsigned char)255)",
            "1 ? true : (unsigned char)255","0 ? true : (unsigned char)255"})
        {
            scalar::program code{};scalar::integer out{};check(scalar::parse(text,code)==scalar::error::none,"dead/nested syntax");
            auto before{reads};check(scalar::evaluate(code,values,out,guest,metadata)==scalar::error::none && out.width==32u &&
                !out.unsigned_value && reads==before,"nested inference and unselected arithmetic do not read values");
        }
        scalar::program code{};scalar::integer out{};check(scalar::parse("1 ? known16 : (unsigned char)0",code)==scalar::error::none,"mismatch syntax");
        check(scalar::evaluate(code,values,out,guest,metadata)==scalar::error::unavailable && out.bits==0u,"selected copy cannot change declaration");
        check(scalar::parse("1 ? bool32 : (unsigned char)0",code)==scalar::error::none,"noncanonical bool syntax");
        auto boolean_copy{[](dwarf::source_expression const&,bool,scalar::integer& value)
        { value=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,255u,32u);return true; }};
        check(scalar::evaluate(code,boolean_copy,out,guest,metadata)==scalar::error::none && out.bits==1u && out.width==32u,
            "noncanonical Boolean storage normalizes to truth");
    }
    for(auto text:{"print 1 41 1 ? (char)1 : (short)2","print-frame 1 41 2 0 ? true : (unsigned char)255"})
    { check(uwvm2::uwvm::debugger::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==
        uwvm2::uwvm::debugger::console_command_kind::source_value,"existing authenticated command grammar"); }
    ::fast_io::io::println("debug_source_conditional_distinct: PASS checks=",checks," numeric_pairs=20 guest_widths=2");
}
