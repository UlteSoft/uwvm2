// Compile against exact retained prior scalar/type headers; no simulated old rules.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char** argv)
{ auto never{[](s::dwarf::source_expression const&,bool,s::integer&) { return false; }};
 for(int i=1;i<argc;++i) { s::program p{};s::integer out{};auto parse{s::parse(::std::string_view{argv[i]},p)};
  auto e{parse==s::error::none ? s::evaluate(p,never,out,32u,s::details::no_type_resolver{},s::language_semantics::cpp) : parse};
  ::fast_io::io::println(static_cast<unsigned>(parse),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",s::copied_type_name(out)); }
}
