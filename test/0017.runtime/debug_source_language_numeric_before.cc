#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char** argv)
{
 for(int i=1;i<argc;++i)
 { scalar::program code{};scalar::integer out{};auto value{[](scalar::dwarf::source_expression const&,bool,scalar::integer&) { return false; }};
   auto p{scalar::parse(::std::string_view{argv[i]},code)};auto e{p==scalar::error::none ? scalar::evaluate(code,value,out) : p};
   ::fast_io::io::println(static_cast<unsigned>(p),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value); }
}
