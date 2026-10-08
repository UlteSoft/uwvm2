// Reproduce pre-fix anonymous sizeof rank against captured predecessor bytes.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
int main()
{
 auto never{[](uwvm2::uwvm::debugger::source_dwarf::source_expression const&,bool,s::integer&) { return false; }};
 for(::std::string_view text:{"sizeof(int)","sizeof(1+2)","sizeof(sizeof(int))","(size_t)1","sizeof(int)+(long)1","1?sizeof(int):(long)1"})
 { s::program p{};s::integer out{};auto e=s::parse(text,p);if(e==s::error::none)e=s::evaluate(p,never,out,32u,s::details::no_type_resolver{},s::language_semantics::cpp);
   ::fast_io::io::println(text,"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",s::copied_type_name(out)); }
}
