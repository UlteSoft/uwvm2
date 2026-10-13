// Owned numeric/type DATA only. No VM stop, producer or guest memory authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("Boolean cast FAIL: ",why);::fast_io::fast_terminate(); } }
#ifdef UWVM_BOOL_CAST_BASELINE
int main(int argc,char const* const* argv)
{
    auto const mode{argc>1 ? ::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::string_view{}};
    check(mode=="integer" || mode=="f32" || mode=="f64","baseline selector");
    auto const resolve{[](dwarf::source_expression const&,bool,scalar::integer& out)
    { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,2u,32u);return true; }};
    auto const text{mode=="integer" ? "(int)flag" : mode=="f32" ? "(float)flag" : "(double)flag"};
    scalar::program code{};scalar::integer out{};
    check(scalar::parse(text,code)==scalar::error::none,"baseline syntax");
    check(scalar::evaluate(code,resolve,out)==scalar::error::none,"baseline conversion");
    if(mode=="integer") { check(out.bits==1u,"integer cast normalizes copied boolean"); }
    else { check(scalar::details::floating_number(out)==1.0,mode=="f32" ?
        "f32 cast normalizes copied boolean" : "f64 cast normalizes copied boolean"); }
}
#else
static ::std::size_t reads{},queries{};
using category = scalar::value_category;
static dwarf::type_record declared{};
static ::std::uint64_t bits{};
static bool missing{},invalid{};
static bool values(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++reads;if(missing || size || leaf.root_name!="flag" || !leaf.steps.empty()) { return false; }
    out=scalar::from_dwarf_numeric(dwarf::value_details::classify(declared),bits,static_cast<unsigned>(declared.byte_count)*8u);
    if(invalid) { out.category=static_cast<category>(255u); }return true;
}
static bool types(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++queries;if(size || leaf.root_name!="flag" || !leaf.steps.empty()) { return false; }
    ::std::vector<dwarf::object_node> layout{};
    if(uwvm2::uwvm::debugger::source_language_expression::type({::std::addressof(declared),1u},0u,leaf.steps,4u,layout)!=
        dwarf::inline_query_error::none || layout.size()!=1u || layout[0u].reason!=dwarf::object_unavailable_reason::none) { return false; }
    out=scalar::from_dwarf_numeric(layout[0u].scalar_kind,0u,static_cast<unsigned>(layout[0u].scalar_bytes)*8u);return true;
}
static ::std::string_view view(::fast_io::string const& s) { return {s.data(),s.size()}; }
static scalar::integer run(::std::string_view text,scalar::error expected,::std::size_t count,unsigned guest)
{
    scalar::program code{};scalar::integer out{123u,64u,true};auto const before{reads};
    check(scalar::parse(text,code)==scalar::error::none,"bounded numeric grammar");
    auto const status{scalar::evaluate(code,values,out,guest,types)};
    if(status!=expected) { ::fast_io::io::perrln(text," status=",static_cast<unsigned>(status)); }
    check(status==expected,"conversion status");check(reads-before==count,"selected/dead copied value reads");
    if(expected!=scalar::error::none)
    { check(out.bits==0u && out.width==32u && !out.unsigned_value && !out.floating && out.category==category::unspecified,"refusal publishes no value"); }
    return out;
}
static ::std::uint64_t fp_bits(unsigned value,unsigned width)
{ return width==32u ? ::std::bit_cast<::std::uint32_t>(static_cast<float>(value)) : ::std::bit_cast<::std::uint64_t>(static_cast<double>(value)); }
static void same(scalar::integer got,unsigned value,unsigned width,bool uns,bool floating,category kind)
{
    check(got.bits==(floating ? fp_bits(value,width) : value) && got.width==width &&
        got.unsigned_value==uns && got.floating==floating && got.category==kind,"native Boolean value and declared conversion result");
}
int main()
{
    struct target { ::std::string_view name;unsigned width;bool uns,floating,boolean; };
    target const targets[]{{"i8",8u,false,false,false},{"u8",8u,true,false,false},
        {"i16",16u,false,false,false},{"u16",16u,true,false,false},
        {"i32",32u,false,false,false},{"u32",32u,true,false,false},
        {"i64",64u,false,false,false},{"u64",64u,true,false,false},
        {"isize",0u,false,false,false},{"usize",0u,true,false,false},
        {"f32",32u,false,true,false},{"f64",64u,false,true,false},{"bool",8u,true,false,true}};
    for(unsigned guest:{32u,64u})
    {
        for(unsigned bytes:{1u,2u,4u,8u})
        {
            declared={};declared.kind=dwarf::type_kind::scalar;declared.encoding=2u;
            declared.byte_count=static_cast<::std::uint8_t>(bytes);declared.byte_size=bytes;declared.size_known=true;
            for(unsigned sample{};sample<259u;++sample)
            {
                auto const width{bytes*8u};
                bits=sample<256u ? sample : sample==256u ? (::std::uint64_t{1u}<<(width-1u)) :
                    sample==257u ? scalar::details::mask(width) : (::std::uint64_t{1u}<<(width-1u))|1u;
                // Construct a valid native bool from copied DATA, never write a
                // noncanonical representation into a native bool object.
                bool const truth{bits!=0u};unsigned const expected{static_cast<unsigned>(truth)};
                check(static_cast<int>(truth)==static_cast<int>(expected) &&
                    static_cast<float>(truth)==static_cast<float>(expected) &&
                    static_cast<double>(truth)==static_cast<double>(expected),"defined native Boolean conversion oracle");
                for(auto const& to:targets)
                {
                    auto const result_width{to.width ? to.width : guest};
                    auto const kind{to.boolean ? category::boolean : to.floating ? category::unspecified : category::integer};
                    ::fast_io::string const forms[]{::fast_io::concat_fast_io("(",to.name,")flag"),
                        ::fast_io::concat_fast_io("static_cast<",to.name,">(flag)"),
                        ::fast_io::concat_fast_io(to.name,"(flag)"),::fast_io::concat_fast_io("flag as ",to.name)};
                    for(auto const& text:forms)
                    {
                        same(run(view(text),scalar::error::none,1u,guest),expected,result_width,to.uns,to.floating,kind);
                        auto const common_width{to.floating ? result_width : result_width<32u ? 32u : result_width};
                        bool const common_uns{!to.floating && to.uns && result_width>=32u};
                        auto const selected{::fast_io::concat_fast_io("1 ? ",text," : 7")};
                        same(run(view(selected),scalar::error::none,1u,guest),expected,common_width,common_uns,to.floating,category::unspecified);
                        auto const dead{::fast_io::concat_fast_io("0 ? ",text," : 7")};
                        same(run(view(dead),scalar::error::none,0u,guest),7u,common_width,common_uns,to.floating,category::unspecified);
                    }
                }
            }
            bits=2u;
            same(run("(int)(double)flag + 1",scalar::error::none,1u,guest),2u,32u,false,false,category::unspecified);
            same(run("(double)(u8)flag",scalar::error::none,1u,guest),1u,64u,false,true,category::unspecified);
            same(run("0 && (double)flag",scalar::error::none,0u,guest),0u,32u,false,false,category::unspecified);
            same(run("1 || (int)flag",scalar::error::none,0u,guest),1u,32u,false,false,category::unspecified);
            run("@as(u64, flag)",scalar::error::unsupported,0u,guest);
            run("1 ? (bool)flag : (bool)flag",scalar::error::unsupported,0u,guest);
            missing=true;run("(double)flag",scalar::error::unavailable,1u,guest);
            same(run("0 ? (double)flag : 7",scalar::error::none,0u,guest),7u,64u,false,true,category::unspecified);
            missing=false;invalid=true;run("(int)flag",scalar::error::unavailable,1u,guest);invalid=false;
        }
        // Genuine integers retain their magnitude; classification is required
        // for Boolean normalization. Old unspecified wide callbacks stay compatible.
        declared.encoding=7u;declared.byte_count=4u;declared.byte_size=4u;bits=2u;
        same(run("(int)flag",scalar::error::none,1u,guest),2u,32u,false,false,category::integer);
        same(run("(double)flag",scalar::error::none,1u,guest),2u,64u,false,true,category::unspecified);
        same(run("(bool)flag",scalar::error::none,1u,guest),1u,8u,true,false,category::boolean);
        auto legacy{[](dwarf::source_expression const&,bool,scalar::integer& out){out={2u,32u,true};return true;}};
        scalar::program code{};scalar::integer out{};check(scalar::parse("(int)flag",code)==scalar::error::none,"legacy grammar");
        check(scalar::evaluate(code,legacy,out,guest)==scalar::error::none && out.bits==2u,"unclassified integer callback unchanged");
    }
    check(queries>0u,"real production declaration/type traversal exercised");
    ::fast_io::io::println("debug_source_boolean_cast: PASS checks=",checks," owned DATA only");
}
#endif
