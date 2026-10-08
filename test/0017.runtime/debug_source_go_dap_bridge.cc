// Parser DATA only: no stop, frame, address, descriptor or runtime read authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
int main(int argc,char const* const* argv)
{
    namespace dbg = uwvm2::uwvm::debugger;
    for(int i{1};i<argc;++i)
    {
        auto const view{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
        dbg::source_scalar_expression::program code{}; dbg::source_dwarf::source_expression reference{};
        auto const scalar{dbg::source_scalar_expression::parse({view.data(),view.size()},code)};
        auto const selector{dbg::source_dwarf::parse_source_expression({view.data(),view.size()},reference)};
        ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(scalar),"\t",static_cast<unsigned>(selector));
    }
}
