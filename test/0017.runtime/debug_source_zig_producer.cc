// Copied CU producer semantics only. No location or runtime capability comes from metadata.
#include <uwvm2/uwvm/debugger/source_frames.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace d = ::uwvm2::uwvm::debugger::source_dwarf;
namespace f = ::uwvm2::uwvm::debugger::source_frames;
namespace s = ::uwvm2::uwvm::debugger::source_scalar_expression;
static ::std::uint64_t checks{};
static void check(bool ok, ::std::string_view why)
{ ++checks; if(!ok) { ::fast_io::io::perrln("Zig producer: ",why); ::fast_io::fast_terminate(); } }
int main()
{
    using producer = d::producer_language;
    for(auto text : {"zig 0.17.0", "zig 0.18.0-dev.35+5e754304d", "zig 0.17.0+build.1"})
    { check(d::copied_producer_language(0x0cu,text)==producer::zig,"actual stable/development Zig producer spellings"); }
    for(auto text : {"clang zig 0.17.0", "Zig 0.17.0", "zig", "zig ", "zig 0.17", "zig 0.17.0 extra", "zig 0.17.0-",
                    "zig 0.17.0+", "zig 0.17.0+.build", "zig 0.17.0+build+other", "zig 0.17.0-dev..1", "zig 0.17.0-dev/1",
                    "zig 00000000000.17.0", "TinyGo extra", "my.zig", "Packet", ""})
    { check(d::copied_producer_language(0x0cu,text)==producer::none,"near-match does not supply language metadata"); }
    check(d::copied_producer_language(0x0cu,"TinyGo")==producer::tinygo,"TinyGo exact producer preserved");
    for(auto language : {0u,0x04u,0x16u,0x1cu,0x27u,0xffffu})
    { check(d::copied_producer_language(language,"zig 0.17.0")==producer::none,"non-C99 metadata not overridden"); }
    ::fast_io::string long_text{::fast_io::concat_fast_io("zig 0.17.0-",::fast_io::mnp::chvw('a'))};
    for(unsigned i{}; i != 128u; ++i) { long_text.push_back('a'); }
    check(d::copied_producer_language(0x0cu,{long_text.data(),long_text.size()})==producer::none,"bounded producer string");
    check(s::language_from_dwarf(0x0cu,false,true)==s::language_semantics::zig,"owned Zig C99 metadata selects Zig");
    check(s::language_from_dwarf(0x0cu,true,true)==s::language_semantics::shared_numeric,"contradictory producer bits unavailable");
    check(s::language_from_dwarf(0x04u,false,true)==s::language_semantics::shared_numeric,"Zig bit cannot override incompatible CU");
    check(s::language_from_dwarf(0x0cu,false)==s::language_semantics::c,"ordinary C99 preserved");
    ::std::vector<d::scope_record> scopes(3u);
    scopes[0].kind=d::scope_kind::compile_unit;scopes[0].identity={0u,0u};
    for(unsigned i{1u}; i != 3u; ++i)
    { scopes[i].parent=i-1u;scopes[i].kind=i==1u ? d::scope_kind::subprogram : d::scope_kind::inline_subprogram;
      scopes[i].identity={0u,i};scopes[i].concrete=true;scopes[i].ranges.push_back({1u,10u}); }
    scopes[1].language=0x04u;scopes[2].language=0x0cu;scopes[2].zig_producer=true;
    f::language_context context{};
    check(f::current_language(scopes,5u,0u,context)==f::error::none && context.language==0x0cu && context.zig_producer && !context.tinygo_producer,"actual selected innermost metadata");
    check(f::current_language(scopes,5u,1u,context)==f::error::none && context.language==0x04u && !context.zig_producer,"different selected frame keeps original CU");
    check(f::current_language(scopes,5u,2u,context)==f::error::bounds && !context.zig_producer,"absent frame no partial language");
    auto absent{[](auto const&,bool,s::integer&) { return false; }};
    for(unsigned bits : {32u,64u}) for(auto text : {"!true","true and false","false or true","2 < 3"})
    {
        s::program code{};s::integer out{};
        check(s::parse(text,code)==s::error::none,"finite Zig predicate syntax");
        check(s::evaluate(code,absent,out,bits,s::details::no_type_resolver{},s::language_from_dwarf(0x0cu,false,true))==s::error::none && out.category==s::value_category::boolean,"Zig Boolean result category after CU adaptation");
    }
    ::fast_io::io::println("PASS copied Zig producer checks=",checks);
}
