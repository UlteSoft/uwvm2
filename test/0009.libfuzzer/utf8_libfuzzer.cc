/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <uwvm2/utils/utf/utf8.h>
#endif

static inline ::uwvm2::utils::utf::u8result reference_check_legal_utf8_rfc3629_unchecked(char8_t const* str_begin, char8_t const* str_end) noexcept
{
    char8_t const* current = str_begin;
    while(current < str_end)
    {
        ::std::uint_least8_t byte = (*current & 0xFFu);
        ::std::size_t len = 0;

        if(byte < 0x80)
        {
            ++current;
            continue;
        }
        else if((byte & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
        {
            len = 2;
        }
        else if((byte & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
        {
            len = 3;
        }
        else if((byte & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
        {
            len = 4;
        }
        else
        {
            if((byte & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
            {
                return {current, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
            }
            else
            {
                return {current, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
            }
        }

        if(current + len > str_end) { return {current, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

        for(::std::size_t i = 1; i < len; ++i)
        {
            if((current[i] & 0xC0) != 0x80) { return {current, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }
        }

        char32_t cp = 0;
        if(len == 2)
        {
            cp = ((static_cast<::std::uint_least32_t>(byte) & 0x1F) << 6) | (current[1] & 0x3F);
            if(cp < 0x80) { return {current, ::uwvm2::utils::utf::utf_error_code::overlong_encoding}; }
        }
        else if(len == 3)
        {
            cp = ((static_cast<::std::uint_least32_t>(byte) & 0x0F) << 12) | ((static_cast<::std::uint_least32_t>(current[1]) & 0x3F) << 6) |
                 (current[2] & 0x3F);
            if(cp < 0x800) { return {current, ::uwvm2::utils::utf::utf_error_code::overlong_encoding}; }
            if(cp >= 0xD800 && cp <= 0xDFFF) { return {current, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate}; }
        }
        else if(len == 4)
        {
            cp = ((static_cast<::std::uint_least32_t>(byte) & 0x07) << 18) | ((static_cast<::std::uint_least32_t>(current[1]) & 0x3F) << 12) |
                 ((static_cast<::std::uint_least32_t>(current[2]) & 0x3F) << 6) | (current[3] & 0x3F);
            if(cp < 0x10000) { return {current, ::uwvm2::utils::utf::utf_error_code::overlong_encoding}; }
            if(cp > 0x10FFFF) { return {current, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint}; }
        }

        current += len;
    }
    return {str_end, ::uwvm2::utils::utf::utf_error_code::success};
}

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    auto const* b8 = reinterpret_cast<char8_t const*>(data);
    auto const* e8 = reinterpret_cast<char8_t const*>(data + size);

    try
    {
        auto const test_rfc3629 =
            ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(b8, e8);
        auto const ref_rfc3629 = reference_check_legal_utf8_rfc3629_unchecked(b8, e8);
        if(test_rfc3629.err != ref_rfc3629.err || test_rfc3629.pos != ref_rfc3629.pos) { __builtin_trap(); }

        // (void)::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(b8, e8);
        auto const test_rfc3629_zero_illegal =
            ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(b8, e8);
        char8_t const* first_null{};
        for(char8_t const* p = b8; p != e8; ++p)
        {
            if(*p == u8'\0')
            {
                first_null = p;
                break;
            }
        }
        if(first_null && test_rfc3629_zero_illegal.err == ::uwvm2::utils::utf::utf_error_code::contains_empty_characters &&
           test_rfc3629_zero_illegal.pos != first_null)
        {
            __builtin_trap();
        }

        // (void)::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(b8, e8);
    }
    catch(...)
    {
    }

    return 0;
}
