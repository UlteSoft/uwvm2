// Finite source/type/value DATA; no VM pause, producer, guest or host authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{},reads{},queries{};
static void check(bool ok, ::std::string_view reason)
{
    ++checks;
    if(!ok) { ::fast_io::io::perrln("Zig preflight FAIL: ",reason); ::fast_io::fast_terminate(); }
}
struct source { ::std::string_view name; scalar::integer value; };
static source const sources[]{
    {"value",{7u,32u,false}},{"negative",{0xfffffff9u,32u,false}},
    {"short",{0xfff9u,16u,false}},{"wide",{7u,64u,false}},
    {"unsigned16",{65535u,16u,true}},{"unsigned32",{0xffffffffu,32u,true}},
    {"unsigned64",{0xffffffffffffffffull,64u,true}},
    {"real",{::std::bit_cast<::std::uint32_t>(1.25f),32u,false,true}},
    {"real64",{::std::bit_cast<::std::uint64_t>(1.25),64u,false,true}},
    {"bool8",{1u,8u,true}},{"byte8",{7u,8u,false}}
};
enum class mutation { none, width, sign, floating, zero_width, odd_width, excess_width, absent };
static mutation drift{};static bool metadata_missing{};
static bool values(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++reads;if(size || !leaf.steps.empty() || drift == mutation::absent) { return false; }
    for(auto const& s : sources)
    {
        if(s.name != leaf.root_name) { continue; }out = s.value;
        switch(drift)
        {
            case mutation::width: out.width = out.width == 64u ? 32u : 64u; break;
            case mutation::sign: out.unsigned_value = !out.unsigned_value; break;
            case mutation::floating: out.floating = !out.floating; break;
            case mutation::zero_width: out.width = 0u; break;
            case mutation::odd_width: out.width = 3u; break;
            case mutation::excess_width: out.width = 128u; break;
            default: break;
        }
        return true;
    }
    return false;
}
static bool types(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++queries;if(metadata_missing || size || !leaf.steps.empty()) { return false; }
    for(auto const& s : sources)
    { if(s.name == leaf.root_name) { out = s.value;out.bits = 0xffffffffffffffffull;return true; } }
    return false;
}
static scalar::integer run(::std::string_view text,scalar::error expected,::std::size_t expected_reads,unsigned guest,
    ::std::string_view reason)
{
    scalar::program code{};check(scalar::parse(text,code)==scalar::error::none,"bounded syntax");
    auto const before{reads};scalar::integer result{123u,64u,true};
    check(scalar::evaluate(code,values,result,guest,types)==expected,reason);
    check(reads-before==expected_reads,"root type proof must precede value reads");
    if(expected!=scalar::error::none)
    { check(result.bits==0u && result.width==32u && !result.unsigned_value && !result.floating,"failure publishes no value"); }
    return result;
}
static void same(scalar::integer got,scalar::integer expected)
{
    check(got.bits==expected.bits && got.width==expected.width && got.unsigned_value==expected.unsigned_value &&
        got.floating==expected.floating,"matching declared/carrier type and value");
}
int main(int argc,char const* const* argv)
{
    auto const mode{argc > 1 ? ::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::string_view{}};
    bool const all{mode.empty()};check(all || mode=="read" || mode=="missing" || mode=="drift","known baseline selector");
    if(all || mode=="read")
    { run("@as(i16, value)",scalar::error::unsupported,0u,32u,"unsafe root declaration rejected"); }
    if(all || mode=="missing")
    {
        metadata_missing=true;run("@as(i64, value)",scalar::error::unavailable,0u,32u,"missing source metadata cannot borrow a value");
        metadata_missing=false;
    }
    if(all || mode=="drift")
    {
        drift=mutation::width;run("@as(i64, value)",scalar::error::unavailable,1u,32u,"root coercion refuses a changed source type");
        drift=mutation::none;
    }
    if(!all) { return 0; }
    struct destination { ::std::string_view name;unsigned width;bool uns{},floating{}; };
    destination const destinations[]{
        {"i8",8u},{"u8",8u,true},{"i16",16u},{"u16",16u,true},{"i32",32u},{"u32",32u,true},
        {"i64",64u},{"u64",64u,true},{"isize",0u},{"usize",0u,true},{"f32",32u,false,true},{"f64",64u,false,true}
    };
    for(unsigned guest : {32u,64u})
    {
        for(auto const& from : sources)
        {
            for(auto const& to : destinations)
            {
                unsigned const width{to.width ? to.width : guest};
                bool const safe{from.value.width!=8u && (to.floating ? from.value.floating && width>=from.value.width :
                    !from.value.floating && (to.uns ? from.value.unsigned_value && width>=from.value.width :
                        from.value.unsigned_value ? width>from.value.width : width>=from.value.width))};
                auto const text{::fast_io::concat_std("@as(",to.name,", ",from.name,")")};
                for(auto fault : {mutation::none,mutation::width,mutation::sign,mutation::floating,
                    mutation::zero_width,mutation::odd_width,mutation::excess_width,mutation::absent})
                {
                    drift=fault;
                    auto const expected{!safe ? scalar::error::unsupported : fault==mutation::none ? scalar::error::none : scalar::error::unavailable};
                    auto got{run(text,expected,safe ? 1u : 0u,guest,"declaration inclusion and actual carrier agreement")};
                    if(safe && fault==mutation::none)
                    {
                        auto result{from.value};result.width=width;result.unsigned_value=to.uns;
                        if(result.floating)
                        { result.bits=width==32u ? ::std::bit_cast<::std::uint32_t>(1.25f) : ::std::bit_cast<::std::uint64_t>(1.25); }
                        else if(from.name=="negative" || from.name=="short")
                        { result.bits=width==64u ? 0xfffffffffffffff9ull : width==32u ? 0xfffffff9u : 0xfff9u; }
                        same(got,result);
                    }
                }
                drift=mutation::none;metadata_missing=true;
                run(text,scalar::error::unavailable,0u,guest,"unknown declaration is refused before copied values");
                metadata_missing=false;
            }
        }
        for(auto text : {"@as(i64, @as(i16, value))","@as(f64, @as(f32, real64))","@as(u64, negative) + value",
            "@as(i32, bool8)","@as(i64, value + 2)"})
        { run(text,scalar::error::unsupported,0u,guest,"nested unsafe type or unsupported shape requires no value"); }
        same(run("@as(i64, @as(i32, negative))",scalar::error::none,1u,guest,"nested matching declaration"),
            {0xfffffffffffffff9ull,64u,false});
        drift=mutation::width;
        for(auto text : {"@as(i64, value)","@as(i64, @as(i32, value))","1 ? @as(i64, value) : 0"})
        { run(text,scalar::error::unavailable,1u,guest,"conversion cannot conceal a changed source width"); }
        drift=mutation::none;metadata_missing=true;
        same(run("@as(u8, 255)",scalar::error::none,0u,guest,"owned literal needs no metadata"),{255u,8u,true});
        auto const before{queries};
        same(run("1 || @as(i64, missing)",scalar::error::none,0u,guest,"logical short circuit"),{1u,32u,false});
        same(run("0 && @as(i64, missing)",scalar::error::none,0u,guest,"logical short circuit"),{0u,32u,false});
        check(queries==before,"short circuit performs no metadata query");metadata_missing=false;
        for(auto text : {"@as(i64, value)","@as(i64, negative)","@as(f64, real)"})
        {
            scalar::program code{};check(scalar::parse(text,code)==scalar::error::none,"legacy finite syntax");
            scalar::integer result{};auto const before_reads{reads};auto const before_queries{queries};
            check(scalar::evaluate(code,values,result,guest)==scalar::error::none,"value-only API remains supported");
            check(reads-before_reads==1u && queries==before_queries,"legacy API does not acquire metadata");
            scalar::details::no_type_resolver const absent{};
            check(scalar::evaluate(code,values,result,guest,absent)==scalar::error::none,"const absent resolver retains legacy behavior");
        }
        {
            scalar::program code{};check(scalar::parse("@as(i64, value)",code)==scalar::error::none,"two-argument metadata syntax");
            auto const two{[](dwarf::source_expression const& leaf,scalar::integer& out){return types(leaf,false,out);}};
            scalar::integer result{};auto const before_reads{reads};auto const before_queries{queries};
            check(scalar::evaluate(code,values,result,guest,two)==scalar::error::none && reads-before_reads==1u &&
                queries-before_queries==1u,"two-argument metadata preflight");
        }
        {
            // Actual production DWARF pointee type traversal, with no guest
            // memory reader, VM, producer, frame or stop token in this DATA.
            ::std::vector<dwarf::type_record> records(2u);
            records[0u].kind=dwarf::type_kind::scalar;records[0u].encoding=5u;records[0u].byte_count=4u;
            records[0u].byte_size=4u;records[0u].size_known=true;
            records[1u].kind=dwarf::type_kind::pointer;records[1u].referenced_type=0u;records[1u].byte_count=guest/8u;
            records[1u].byte_size=guest/8u;records[1u].size_known=true;
            auto const metadata{[&](dwarf::source_expression const& leaf,bool size,scalar::integer& out)
            {
                ++queries;if(size || leaf.root_name!="nullp") { return false; }
                ::std::vector<dwarf::object_node> layout{};
                if(uwvm2::uwvm::debugger::source_language_expression::type(records,1u,leaf.steps,guest/8u,layout)!=
                    dwarf::inline_query_error::none || layout.size()!=1u || layout[0u].scalar_kind!=dwarf::numeric_kind::signed_integer ||
                    layout[0u].scalar_bytes!=4u) { return false; }
                out={123u,32u,false};return true;
            }};
            for(auto text : {"@as(i16, *nullp)","@as(u64, *nullp)"})
            {
                scalar::program code{};check(scalar::parse(text,code)==scalar::error::none,"pointee declaration syntax");
                scalar::integer result{};auto const before_reads{reads};
                check(scalar::evaluate(code,values,result,guest,metadata)==scalar::error::unsupported &&
                    reads==before_reads && result.bits==0u,"unsafe pointee declaration cannot read a pointer or value");
            }
        }
    }
    check(queries>0u,"declaration callbacks exercised");
    ::fast_io::io::println("debug_source_zig_preflight: PASS checks=",checks," owned DATA only");
}
