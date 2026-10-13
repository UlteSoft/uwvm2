#pragma once

#include <cstddef>

// The macOS 4 GiB qualification build keeps command-line parsing and the
// execution driver in separate translation units. The public CLI behavior is
// unchanged; these are startup-only calls outside every Wasm hot path.
namespace uwvm2::uwvm::crtmain::low_memory
{
    enum class parse_status : unsigned
    {
        def,
        return0,
        returnm1
    };

    parse_status parse(::std::size_t argc, char8_t const* const* argv) noexcept;
    int run() noexcept;
}
