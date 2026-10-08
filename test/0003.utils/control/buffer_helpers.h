#pragma once
#include <uwvm2/utils/control/protocol.h>

namespace control_test
{
    namespace ctl = uwvm2::utils::control;
    constexpr ctl::input_buffer input(ctl::input_buffer value) noexcept { return value; }
    template<typename R>
    constexpr ctl::input_buffer input(R const& value) noexcept
    {
        if(value.empty()) { return {}; }
        return {value.data(), value.data() + value.size()};
    }
    template<typename R>
    constexpr ctl::input_buffer prefix(R const& value, std::size_t count) noexcept
    {
        if(count == 0u) { return {}; }
        return {value.data(), value.data() + count};
    }
    template<typename R>
    constexpr ctl::input_buffer slice(R const& value, std::size_t offset, std::size_t count) noexcept
    {
        if(count == 0u) { return {}; }
        return {value.data() + offset, value.data() + offset + count};
    }
    template<typename R>
    constexpr ctl::input_buffer suffix(R const& value, std::size_t offset) noexcept
    { return slice(value, offset, value.size() - offset); }

    // Field mutation is intentional in malformed-wire tests; production parses
    // and emits whole records with one continuous fast_io cursor.
    template<typename U, typename R>
    constexpr void put_le(R& bytes, std::size_t offset, U value)
    {
        ctl::output_buffer output{bytes.data() + offset, bytes.data() + bytes.size()};
        fast_io::io::print(output, fast_io::mnp::le_put<sizeof(U) * 8u>(value));
    }
    template<typename U, typename R>
    constexpr U get_le(R const& bytes, std::size_t offset)
    {
        ctl::input_buffer source{bytes.data() + offset, bytes.data() + bytes.size()};
        U value{};
        fast_io::io::scan(source, fast_io::mnp::le_get<sizeof(U) * 8u>(value));
        return value;
    }
}
