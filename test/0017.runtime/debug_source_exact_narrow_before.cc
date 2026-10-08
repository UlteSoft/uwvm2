#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char** argv)
{ if(argc<3) { return 2; }auto mode{::std::string_view{argv[1]}=="cpp" ? s::language_semantics::cpp : s::language_semantics::c};
 for(int i=2;i<argc;++i) { s::program code{};s::integer out{};auto r{[](s::dwarf::source_expression const&,bool,s::integer&) { return false; }};
  auto p{s::parse(::std::string_view{argv[i]},code)};auto e{p==s::error::none ? s::evaluate(code,r,out,32u,s::details::no_type_resolver{},mode) : p};
  ::fast_io::io::println(static_cast<unsigned>(p),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",static_cast<unsigned>(out.category)); } }
