// Exact predecessor reproduction; finite syntax DATA only.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
int main(int argc,char const* const* argv)
{
 for(int i{1};i<argc;++i)
 {
  auto const input{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
  s::program code{};s::integer out{};::std::size_t reads{};
  auto values{[&](s::dwarf::source_expression const&,bool,s::integer&) { ++reads;return false; }};
  auto status{s::parse({input.data(),input.size()},code)};
  if(status==s::error::none)status=s::evaluate(code,values,out,32u,s::details::no_type_resolver{},s::language_semantics::cpp);
  ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",out.bits,"\t",out.width,"\t",reads);
 }
}
