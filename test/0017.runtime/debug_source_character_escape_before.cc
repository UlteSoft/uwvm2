// Exact retained-header refusal reproduction, no runtime authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char** argv)
{
 for(int i=1;i<argc;++i) { s::program p{};auto a{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
  ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(s::parse({a.data(),a.size()},p))); }
}
