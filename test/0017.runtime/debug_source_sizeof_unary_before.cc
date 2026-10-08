#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char const* const* argv)
{
 for(int i{1};i<argc;++i)
 {
  auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};scalar::program code{};
  ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(scalar::parse({text.data(),text.size()},code)));
 }
}
