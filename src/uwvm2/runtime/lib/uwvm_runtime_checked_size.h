#pragma once

#include <limits>
#include <type_traits>

namespace uwvm2::runtime::lib::details
{
    // Check each addition before it occurs. Checking max - (padding + bytes)
    // alone is insufficient: the parenthesized sum can already have wrapped.
    template <typename Size>
        requires (::std::is_unsigned_v<Size> && !::std::is_same_v<Size, bool>)
    [[nodiscard]] inline constexpr bool try_add_runtime_byte_extents(Size base, Size padding, Size bytes, Size& result) noexcept
    {
        constexpr Size maximum{::std::numeric_limits<Size>::max()};
        if(padding > maximum - base) { return false; }
        Size const padded{static_cast<Size>(base + padding)};
        if(bytes > maximum - padded) { return false; }
        result = static_cast<Size>(padded + bytes);
        return true;
    }
}
