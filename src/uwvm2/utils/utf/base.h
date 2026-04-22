/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-05-31
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <memory>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::utf
{
    enum class utf_error_code : unsigned
    {
        success = 0u,

        long_header_bits,
        too_short_sequence,
        too_long_sequence,
        overlong_encoding,
        excessive_codepoint,
        illegal_surrogate,

        contains_empty_characters
    };

    template <::std::integral char_type>
    struct result_base
    {
        char_type const* pos{};
        utf_error_code err{};
    };

    using result = result_base<char>;
    using wresult = result_base<wchar_t>;
    using u8result = result_base<char8_t>;
    using u16result = result_base<char16_t>;
    using u32result = result_base<char32_t>;

    template <::std::integral char_type2>
    inline constexpr ::uwvm2::utils::container::basic_string_view<char_type2> get_utf_error_description(::uwvm2::utils::utf::utf_error_code type) noexcept
    {
        switch(type)
        {
            case utf_error_code::success:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"Succeeded"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"Succeeded"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"Succeeded"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"Succeeded"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"Succeeded"}; }
            }
            case utf_error_code::long_header_bits:
            {
                if constexpr(::std::same_as<char_type2, char>)
                {
                    return {"Leading bytes in UTF-8 encoding are invalid, any byte must be less than 5 header parts"};
                }
                else if constexpr(::std::same_as<char_type2, wchar_t>)
                {
                    return {L"Leading bytes in UTF-8 encoding are invalid, any byte must be less than 5 header parts"};
                }
                else if constexpr(::std::same_as<char_type2, char8_t>)
                {
                    return {u8"Leading bytes in UTF-8 encoding are invalid, any byte must be less than 5 header parts"};
                }
                else if constexpr(::std::same_as<char_type2, char16_t>)
                {
                    return {u"Leading bytes in UTF-8 encoding are invalid, any byte must be less than 5 header parts"};
                }
                else if constexpr(::std::same_as<char_type2, char32_t>)
                {
                    return {U"Leading bytes in UTF-8 encoding are invalid, any byte must be less than 5 header parts"};
                }
            }
            case utf_error_code::too_short_sequence:
            {
                if constexpr(::std::same_as<char_type2, char>)
                {
                    return {
                        "The UTF sequence is incomplete. The leading byte must be followed by N-1 continuation bytes, where N is the UTF-8 character length"};
                }
                else if constexpr(::std::same_as<char_type2, wchar_t>)
                {
                    return {
                        L"The UTF sequence is incomplete. The leading byte must be followed by N-1 continuation bytes, where N is the UTF-8 character length"};
                }
                else if constexpr(::std::same_as<char_type2, char8_t>)
                {
                    return {
                        u8"The UTF sequence is incomplete. The leading byte must be followed by N-1 continuation bytes, where N is the UTF-8 character length"};
                }
                else if constexpr(::std::same_as<char_type2, char16_t>)
                {
                    return {
                        u"The UTF sequence is incomplete. The leading byte must be followed by N-1 continuation bytes, where N is the UTF-8 character length"};
                }
                else if constexpr(::std::same_as<char_type2, char32_t>)
                {
                    return {
                        U"The UTF sequence is incomplete. The leading byte must be followed by N-1 continuation bytes, where N is the UTF-8 character length"};
                }
            }
            case utf_error_code::too_long_sequence:
            {
                if constexpr(::std::same_as<char_type2, char>)
                {
                    return {"There are too many consecutive continuation bytes, or the string starts with a continuation byte"};
                }
                else if constexpr(::std::same_as<char_type2, wchar_t>)
                {
                    return {L"There are too many consecutive continuation bytes, or the string starts with a continuation byte"};
                }
                else if constexpr(::std::same_as<char_type2, char8_t>)
                {
                    return {u8"There are too many consecutive continuation bytes, or the string starts with a continuation byte"};
                }
                else if constexpr(::std::same_as<char_type2, char16_t>)
                {
                    return {u"There are too many consecutive continuation bytes, or the string starts with a continuation byte"};
                }
                else if constexpr(::std::same_as<char_type2, char32_t>)
                {
                    return {U"There are too many consecutive continuation bytes, or the string starts with a continuation byte"};
                }
            }
            case utf_error_code::overlong_encoding:
            {
                if constexpr(::std::same_as<char_type2, char>)
                {
                    return {
                        "Overlength encoding error. Decoded characters must be greater than U+7F for two-byte characters, greater than U+7FF for three-byte characters, and greater than U+FFFF for four-byte characters"};
                }
                else if constexpr(::std::same_as<char_type2, wchar_t>)
                {
                    return {
                        L"Overlength encoding error. Decoded characters must be greater than U+7F for two-byte characters, greater than U+7FF for three-byte characters, and greater than U+FFFF for four-byte characters"};
                }
                else if constexpr(::std::same_as<char_type2, char8_t>)
                {
                    return {
                        u8"Overlength encoding error. Decoded characters must be greater than U+7F for two-byte characters, greater than U+7FF for three-byte characters, and greater than U+FFFF for four-byte characters"};
                }
                else if constexpr(::std::same_as<char_type2, char16_t>)
                {
                    return {
                        u"Overlength encoding error. Decoded characters must be greater than U+7F for two-byte characters, greater than U+7FF for three-byte characters, and greater than U+FFFF for four-byte characters"};
                }
                else if constexpr(::std::same_as<char_type2, char32_t>)
                {
                    return {
                        U"Overlength encoding error. Decoded characters must be greater than U+7F for two-byte characters, greater than U+7FF for three-byte characters, and greater than U+FFFF for four-byte characters"};
                }
            }
            case utf_error_code::excessive_codepoint:
            {
                if constexpr(::std::same_as<char_type2, char>)
                {
                    return {
                        "The code point exceeds the limit. The decoded character must be less than or equal to U+10FFFF, for ASCII it must be less than or equal to U+7F, for Latin1 it must be less than or equal to U+FF"};
                }
                else if constexpr(::std::same_as<char_type2, wchar_t>)
                {
                    return {
                        L"The code point exceeds the limit. The decoded character must be less than or equal to U+10FFFF, for ASCII it must be less than or equal to U+7F, for Latin1 it must be less than or equal to U+FF"};
                }
                else if constexpr(::std::same_as<char_type2, char8_t>)
                {
                    return {
                        u8"The code point exceeds the limit. The decoded character must be less than or equal to U+10FFFF, for ASCII it must be less than or equal to U+7F, for Latin1 it must be less than or equal to U+FF"};
                }
                else if constexpr(::std::same_as<char_type2, char16_t>)
                {
                    return {
                        u"The code point exceeds the limit. The decoded character must be less than or equal to U+10FFFF, for ASCII it must be less than or equal to U+7F, for Latin1 it must be less than or equal to U+FF"};
                }
                else if constexpr(::std::same_as<char_type2, char32_t>)
                {
                    return {
                        U"The code point exceeds the limit. The decoded character must be less than or equal to U+10FFFF, for ASCII it must be less than or equal to U+7F, for Latin1 it must be less than or equal to U+FF"};
                }
            }
            case utf_error_code::illegal_surrogate:
            {
                if constexpr(::std::same_as<char_type2, char>)
                {
                    return {
                        "Illegal proxy entry error. The decoded character cannot be in the U+D800...DFFF range (UTF-8 or UTF-32), or the high proxy term must follow the low proxy term, and the low proxy term must be preceded by the high proxy term (UTF-16)"};
                }
                else if constexpr(::std::same_as<char_type2, wchar_t>)
                {
                    return {
                        L"Illegal proxy entry error. The decoded character cannot be in the U+D800...DFFF range (UTF-8 or UTF-32), or the high proxy term must follow the low proxy term, and the low proxy term must be preceded by the high proxy term (UTF-16)"};
                }
                else if constexpr(::std::same_as<char_type2, char8_t>)
                {
                    return {
                        u8"Illegal proxy entry error. The decoded character cannot be in the U+D800...DFFF range (UTF-8 or UTF-32), or the high proxy term must follow the low proxy term, and the low proxy term must be preceded by the high proxy term (UTF-16)"};
                }
                else if constexpr(::std::same_as<char_type2, char16_t>)
                {
                    return {
                        u"Illegal proxy entry error. The decoded character cannot be in the U+D800...DFFF range (UTF-8 or UTF-32), or the high proxy term must follow the low proxy term, and the low proxy term must be preceded by the high proxy term (UTF-16)"};
                }
                else if constexpr(::std::same_as<char_type2, char32_t>)
                {
                    return {
                        U"Illegal proxy entry error. The decoded character cannot be in the U+D800...DFFF range (UTF-8 or UTF-32), or the high proxy term must follow the low proxy term, and the low proxy term must be preceded by the high proxy term (UTF-16)"};
                }
            }
            case utf_error_code::contains_empty_characters:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"Contains null characters or errors related to null character validation"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"Contains null characters or errors related to null character validation"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"Contains null characters or errors related to null character validation"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"Contains null characters or errors related to null character validation"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"Contains null characters or errors related to null character validation"}; }
            }
            [[unlikely]] default:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"Unknown"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"Unknown"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"Unknown"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"Unknown"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"Unknown"}; }
            }
        }
    }
}
