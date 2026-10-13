#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char** argv)
{ auto types{[](s::dwarf::source_expression const& e,s::integer& out)
 { if(e.root_name!="left" && e.root_name!="right") { return false; }out=s::from_dwarf_numeric(s::dwarf::numeric_kind::signed_integer,0u,16u);out.declaration_identity={4u,90u};out.declaration_identity_known=true;return true; }};
 auto values{[&](s::dwarf::source_expression const& e,bool size,s::integer& out) { if(size || !types(e,out)) { return false; }out.bits=65535u;return true; }};
 for(int i=1;i<argc;++i) { s::program code{};s::integer out{};auto p{s::parse(::std::string_view{argv[i]},code)};auto e{p==s::error::none ? s::evaluate(code,values,out,32u,types,s::language_semantics::cpp) : p};
  ::fast_io::io::println(static_cast<unsigned>(p),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",static_cast<unsigned>(out.category)); } }
