#include <uwvm2/uwvm/crtmain/low_memory_entry.h>

// Parameter tables refer to inline callbacks supplied by the separately
// compiled callback groups. The final link resolves every callback symbol.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>

namespace uwvm2::uwvm::crtmain::low_memory
{
    parse_status parse(::std::size_t argc, char8_t const* const* argv) noexcept
    {
        switch(::uwvm2::uwvm::cmdline::parsing(argc, argv))
        {
            case ::uwvm2::uwvm::cmdline::parsing_return_val::def: return parse_status::def;
            case ::uwvm2::uwvm::cmdline::parsing_return_val::return0: return parse_status::return0;
            case ::uwvm2::uwvm::cmdline::parsing_return_val::returnm1: return parse_status::returnm1;
        }
        return parse_status::returnm1;
    }
}
