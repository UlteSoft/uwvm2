/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <climits>
# include <cstring>
# include <concepts>
# include <memory>
# include <bit>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/intrinsics/impl.h>
# include "base.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::utf
{
    enum class utf8_specification : unsigned
    {
        utf8_rfc3629,
        utf8_rfc3629_and_zero_illegal
    };

    inline constexpr char32_t get_max_excessive_codepoint(utf8_specification u8spe) noexcept
    {
        switch(u8spe)
        {
            case utf8_specification::utf8_rfc3629:
            {
                [[fallthrough]];
            }
            case utf8_specification::utf8_rfc3629_and_zero_illegal:
            {
                return static_cast<char32_t>(0x10FFFFu);
            }
            [[unlikely]] default:
            {
                ::fast_io::fast_terminate();
            }
        }
    }

    template <bool zero_illegal>
    inline constexpr ::uwvm2::utils::utf::u8result check_legal_utf8_rfc3629_unchecked(char8_t const* const str_begin, char8_t const* const str_end) noexcept
    {
        // Since most of the import/export and wasi used within wasm are ascii, the utf8 path is less efficient.
        // sse4, avx2, avx512vbmi, neon, lsx, lasx using the same algorithm as simdutf, see https://github.com/simdutf/simdutf

        auto str_curr{str_begin};

        if UWVM_IF_CONSTEVAL
        {
            // byte-by-byte processing
            // Compatible with platforms where CHAR_BIT is greater than 8, because it is guaranteed that only a meaningful portion of each byte operation is
            // fetched

            while(str_curr != str_end)
            {
                // [xx] xx xx ... (end)
                // [  ] unsafe (could be the section_end)
                //  ^^ section_curr

                auto str_curr_val{*str_curr};

                // A standards-compliant UTF-8 decoder must only care about the low 8 bits of each byte when verifying or decoding a byte sequence.

                // Subsequent judgment is required for < static_cast<char8_t>(0b1000'0000u), so for platforms where CHAR_BIT is not equal to 8 the high bit
                // needs to be cleared.

                // support CHAR_BIT > 8
                str_curr_val &= 0xFFu;

                if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                {
                    // ??0xxxxxxx

                    if constexpr(zero_illegal)
                    {
                        if(str_curr_val == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                    }

                    ++str_curr;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                {
                    // ??110xxxxx ??10xxxxxx

                    if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{2u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    str_curr += utf8_length;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                {
                    // ??1110xxxx ??10xxxxxx ??10xxxxxx

                    if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p2_val{*(str_curr + 2u)};

                    if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{3u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                    utf8_b2 &= 0x3Fu;
                    utf8_b2 <<= (utf8_length - 3u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                    }

                    str_curr += utf8_length;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                {
                    // ??11110xxx ??10xxxxxx ??10xxxxxx ??10xxxxxx

                    if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p2_val{*(str_curr + 2u)};

                    if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p3_val{*(str_curr + 3u)};

                    if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{4u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                    utf8_b2 &= 0x3Fu;
                    utf8_b2 <<= (utf8_length - 3u) * 6u;

                    ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                    utf8_b3 &= 0x3Fu;
                    utf8_b3 <<= (utf8_length - 4u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint}; }

                    str_curr += utf8_length;
                }
                else
                {
                    if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                    }
                    else
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                    }
                }
            }

            return {str_curr, ::uwvm2::utils::utf::utf_error_code::success};
        }
        else
        {
#if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__AVX512BW__)

            using i64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int64_t;
            using u64x8simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint64_t;
            using u32x16simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint32_t;
            using u16x32simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint16_t;
            using c8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = char;
            using u8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::uint8_t;
            using i8x64simd [[__gnu__::__vector_size__(64)]] [[maybe_unused]] = ::std::int8_t;

            auto general_algorithm{[&]() constexpr noexcept -> ::uwvm2::utils::utf::u8result
                                   {
                                       while(str_curr != str_end)
                                       {
                                           auto const str_curr_val{*str_curr};

                                           if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                                           {
                                               if constexpr(zero_illegal)
                                               {
                                                   if(str_curr_val == u8'\0') [[unlikely]]
                                                   {
                                                       return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters};
                                                   }
                                               }

                                               ++str_curr;
                                           }
                                           else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                                           {
                                               if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               auto const str_curr_p1_val{*(str_curr + 1u)};

                                               if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               constexpr auto utf8_length{2u};

                                               ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                                               constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                                               utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                                               utf8_b0 <<= (utf8_length - 1u) * 6u;

                                               ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                                               utf8_b1 &= 0x3Fu;
                                               utf8_b1 <<= (utf8_length - 2u) * 6u;

                                               char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                                               constexpr auto test_overlong{utf8_length - 2u};
                                               constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                                               constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                                      << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                                               if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                                               }

                                               str_curr += utf8_length;
                                           }
                                           else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                                           {
                                               if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               auto const str_curr_p1_val{*(str_curr + 1u)};

                                               if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               auto const str_curr_p2_val{*(str_curr + 2u)};

                                               if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               constexpr auto utf8_length{3u};

                                               ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                                               constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                                               utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                                               utf8_b0 <<= (utf8_length - 1u) * 6u;

                                               ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                                               utf8_b1 &= 0x3Fu;
                                               utf8_b1 <<= (utf8_length - 2u) * 6u;

                                               ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                                               utf8_b2 &= 0x3Fu;
                                               utf8_b2 <<= (utf8_length - 3u) * 6u;

                                               char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                                               constexpr auto test_overlong{utf8_length - 2u};
                                               constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                                               constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                                      << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                                               if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                                               }

                                               if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                                               }

                                               str_curr += utf8_length;
                                           }
                                           else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                                           {
                                               if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               auto const str_curr_p1_val{*(str_curr + 1u)};

                                               if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               auto const str_curr_p2_val{*(str_curr + 2u)};

                                               if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               auto const str_curr_p3_val{*(str_curr + 3u)};

                                               if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                               }

                                               constexpr auto utf8_length{4u};

                                               ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                                               constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                                               utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                                               utf8_b0 <<= (utf8_length - 1u) * 6u;

                                               ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                                               utf8_b1 &= 0x3Fu;
                                               utf8_b1 <<= (utf8_length - 2u) * 6u;

                                               ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                                               utf8_b2 &= 0x3Fu;
                                               utf8_b2 <<= (utf8_length - 3u) * 6u;

                                               ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                                               utf8_b3 &= 0x3Fu;
                                               utf8_b3 <<= (utf8_length - 4u) * 6u;

                                               char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                                               constexpr auto test_overlong{utf8_length - 2u};
                                               constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                                               constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                                      << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                                               if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                                               }

                                               if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]]
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint};
                                               }

                                               str_curr += utf8_length;
                                           }
                                           else
                                           {
                                               if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) ==
                                                  static_cast<::std::uint_least32_t>(0b1000'0000u))
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                                               }
                                               else
                                               {
                                                   return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                                               }
                                           }
                                       }

                                       return {str_curr, ::uwvm2::utils::utf::utf_error_code::success};
                                   }};

            constexpr auto check_special_cases{
                [] UWVM_ALWAYS_INLINE(u8x64simd const input, u8x64simd const prev1) constexpr noexcept -> u8x64simd
                {
                    constexpr u8x64simd lookup_b1h{::std::bit_cast<u8x64simd>(u64x8simd{0x0202020202020202u,
                                                                                        0x4915012180808080u,
                                                                                        0x0202020202020202u,
                                                                                        0x4915012180808080u,
                                                                                        0x0202020202020202u,
                                                                                        0x4915012180808080u,
                                                                                        0x0202020202020202u,
                                                                                        0x4915012180808080u})};

                    auto const prev1_shr4{::std::bit_cast<u8x64simd>(::std::bit_cast<u16x32simd>(prev1) >> static_cast<::std::uint16_t>(4u)) &
                                          static_cast<::std::uint8_t>(0xFFu >> 4u)};

                    u8x64simd b1h;
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb512_mask)  // GCC
                    b1h = __builtin_ia32_pshufb512_mask(lookup_b1h, prev1_shr4, u8x64simd{}, UINT64_MAX);
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb512)  // Clang
                    b1h = __builtin_ia32_pshufb512(lookup_b1h, prev1_shr4);
# else
#  error "missing instruction"
# endif

                    constexpr u8x64simd lookup_b1l{::std::bit_cast<u8x64simd>(u64x8simd{0xCBCBCB8B8383A3E7u,
                                                                                        0xCBCBDBCBCBCBCBCBu,
                                                                                        0xCBCBCB8B8383A3E7u,
                                                                                        0xCBCBDBCBCBCBCBCBu,
                                                                                        0xCBCBCB8B8383A3E7u,
                                                                                        0xCBCBDBCBCBCBCBCBu,
                                                                                        0xCBCBCB8B8383A3E7u,
                                                                                        0xCBCBDBCBCBCBCBCBu})};

                    auto const prev1_andF{prev1 & static_cast<::std::uint8_t>(0x0Fu)};

                    u8x64simd b1l;

# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb512_mask)  // GCC
                    b1l = __builtin_ia32_pshufb512_mask(lookup_b1l, prev1_andF, u8x64simd{}, UINT64_MAX);
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb512)  // Clang
                    b1l = __builtin_ia32_pshufb512(lookup_b1l, prev1_andF);
# else
#  error "missing instruction"
# endif

                    constexpr u8x64simd lookup_b2h{::std::bit_cast<u8x64simd>(u64x8simd{0x101010101010101u,
                                                                                        0x1010101BABAAEE6u,
                                                                                        0x101010101010101u,
                                                                                        0x1010101BABAAEE6u,
                                                                                        0x101010101010101u,
                                                                                        0x1010101BABAAEE6u,
                                                                                        0x101010101010101u,
                                                                                        0x1010101BABAAEE6u})};

                    auto const inp_shr4{::std::bit_cast<u8x64simd>(::std::bit_cast<u16x32simd>(input) >> static_cast<::std::uint16_t>(4u)) &
                                        static_cast<::std::uint8_t>(0xFFu >> 4u)};

                    u8x64simd b2h;

# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb512_mask)  // GCC
                    b2h = __builtin_ia32_pshufb512_mask(lookup_b2h, inp_shr4, u8x64simd{}, UINT64_MAX);
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb512)  // Clang
                    b2h = __builtin_ia32_pshufb512(lookup_b2h, inp_shr4);
# else
#  error "missing instruction"
# endif

                    return b1h & b1l & b2h;
                }};

            constexpr auto check_multibyte_lengths{
                [] UWVM_ALWAYS_INLINE(u8x64simd const input, u8x64simd const prev_input, u8x64simd const sc) constexpr noexcept -> u8x64simd
                {
                    auto prev2{__builtin_shufflevector(prev_input,
                                                       input,
                                                       62,   // 0
                                                       63,   // 1
                                                       64,   // 2
                                                       65,   // 3
                                                       66,   // 4
                                                       67,   // 5
                                                       68,   // 6
                                                       69,   // 7
                                                       70,   // 8
                                                       71,   // 9
                                                       72,   // 10
                                                       73,   // 11
                                                       74,   // 12
                                                       75,   // 13
                                                       76,   // 14
                                                       77,   // 15
                                                       78,   // 16
                                                       79,   // 17
                                                       80,   // 18
                                                       81,   // 19
                                                       82,   // 20
                                                       83,   // 21
                                                       84,   // 22
                                                       85,   // 23
                                                       86,   // 24
                                                       87,   // 25
                                                       88,   // 26
                                                       89,   // 27
                                                       90,   // 28
                                                       91,   // 29
                                                       92,   // 30
                                                       93,   // 31
                                                       94,   // 32
                                                       95,   // 33
                                                       96,   // 34
                                                       97,   // 35
                                                       98,   // 36
                                                       99,   // 37
                                                       100,  // 38
                                                       101,  // 39
                                                       102,  // 40
                                                       103,  // 41
                                                       104,  // 42
                                                       105,  // 43
                                                       106,  // 44
                                                       107,  // 45
                                                       108,  // 46
                                                       109,  // 47
                                                       110,  // 48
                                                       111,  // 49
                                                       112,  // 50
                                                       113,  // 51
                                                       114,  // 52
                                                       115,  // 53
                                                       116,  // 54
                                                       117,  // 55
                                                       118,  // 56
                                                       119,  // 57
                                                       120,  // 58
                                                       121,  // 59
                                                       122,  // 60
                                                       123,  // 61
                                                       124,  // 62
                                                       125   // 63
                                                       )};

                    auto prev3{__builtin_shufflevector(prev_input,
                                                       input,
                                                       61,   // 0
                                                       62,   // 1
                                                       63,   // 2
                                                       64,   // 3
                                                       65,   // 4
                                                       66,   // 5
                                                       67,   // 6
                                                       68,   // 7
                                                       69,   // 8
                                                       70,   // 9
                                                       71,   // 10
                                                       72,   // 11
                                                       73,   // 12
                                                       74,   // 13
                                                       75,   // 14
                                                       76,   // 15
                                                       77,   // 16
                                                       78,   // 17
                                                       79,   // 18
                                                       80,   // 19
                                                       81,   // 20
                                                       82,   // 21
                                                       83,   // 22
                                                       84,   // 23
                                                       85,   // 24
                                                       86,   // 25
                                                       87,   // 26
                                                       88,   // 27
                                                       89,   // 28
                                                       90,   // 29
                                                       91,   // 30
                                                       92,   // 31
                                                       93,   // 32
                                                       94,   // 33
                                                       95,   // 34
                                                       96,   // 35
                                                       97,   // 36
                                                       98,   // 37
                                                       99,   // 38
                                                       100,  // 39
                                                       101,  // 40
                                                       102,  // 41
                                                       103,  // 42
                                                       104,  // 43
                                                       105,  // 44
                                                       106,  // 45
                                                       107,  // 46
                                                       108,  // 47
                                                       109,  // 48
                                                       110,  // 49
                                                       111,  // 50
                                                       112,  // 51
                                                       113,  // 52
                                                       114,  // 53
                                                       115,  // 54
                                                       116,  // 55
                                                       117,  // 56
                                                       118,  // 57
                                                       119,  // 58
                                                       120,  // 59
                                                       121,  // 60
                                                       122,  // 61
                                                       123,  // 62
                                                       124   // 63
                                                       )};

                    constexpr auto prev2_needsubs{static_cast<::std::uint8_t>(0xE0u - 0x80u)};
                    u8x64simd const prev2_needsubs_simd{
                        prev2_needsubs,  // 0
                        prev2_needsubs,  // 1
                        prev2_needsubs,  // 2
                        prev2_needsubs,  // 3
                        prev2_needsubs,  // 4
                        prev2_needsubs,  // 5
                        prev2_needsubs,  // 6
                        prev2_needsubs,  // 7
                        prev2_needsubs,  // 8
                        prev2_needsubs,  // 9
                        prev2_needsubs,  // 10
                        prev2_needsubs,  // 11
                        prev2_needsubs,  // 12
                        prev2_needsubs,  // 13
                        prev2_needsubs,  // 14
                        prev2_needsubs,  // 15
                        prev2_needsubs,  // 16
                        prev2_needsubs,  // 17
                        prev2_needsubs,  // 18
                        prev2_needsubs,  // 19
                        prev2_needsubs,  // 20
                        prev2_needsubs,  // 21
                        prev2_needsubs,  // 22
                        prev2_needsubs,  // 23
                        prev2_needsubs,  // 24
                        prev2_needsubs,  // 25
                        prev2_needsubs,  // 26
                        prev2_needsubs,  // 27
                        prev2_needsubs,  // 28
                        prev2_needsubs,  // 29
                        prev2_needsubs,  // 30
                        prev2_needsubs,  // 31
                        prev2_needsubs,  // 32
                        prev2_needsubs,  // 33
                        prev2_needsubs,  // 34
                        prev2_needsubs,  // 35
                        prev2_needsubs,  // 36
                        prev2_needsubs,  // 37
                        prev2_needsubs,  // 38
                        prev2_needsubs,  // 39
                        prev2_needsubs,  // 40
                        prev2_needsubs,  // 41
                        prev2_needsubs,  // 42
                        prev2_needsubs,  // 43
                        prev2_needsubs,  // 44
                        prev2_needsubs,  // 45
                        prev2_needsubs,  // 46
                        prev2_needsubs,  // 47
                        prev2_needsubs,  // 48
                        prev2_needsubs,  // 49
                        prev2_needsubs,  // 50
                        prev2_needsubs,  // 51
                        prev2_needsubs,  // 52
                        prev2_needsubs,  // 53
                        prev2_needsubs,  // 54
                        prev2_needsubs,  // 55
                        prev2_needsubs,  // 56
                        prev2_needsubs,  // 57
                        prev2_needsubs,  // 58
                        prev2_needsubs,  // 59
                        prev2_needsubs,  // 60
                        prev2_needsubs,  // 61
                        prev2_needsubs,  // 62
                        prev2_needsubs   // 63
                    };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                    prev2 = __builtin_elementwise_sub_sat(prev2, prev2_needsubs_simd);
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb512_mask)  // GCC
                    prev2 = __builtin_ia32_psubusb512_mask(prev2, prev2_needsubs_simd, u8x64simd{}, UINT64_MAX);
# else
#  error "missing instruction"
# endif

                    constexpr auto prev3_needsubs{static_cast<::std::uint8_t>(0xF0u - 0x80u)};
                    u8x64simd const prev3_needsubs_simd{
                        prev3_needsubs,  // 0
                        prev3_needsubs,  // 1
                        prev3_needsubs,  // 2
                        prev3_needsubs,  // 3
                        prev3_needsubs,  // 4
                        prev3_needsubs,  // 5
                        prev3_needsubs,  // 6
                        prev3_needsubs,  // 7
                        prev3_needsubs,  // 8
                        prev3_needsubs,  // 9
                        prev3_needsubs,  // 10
                        prev3_needsubs,  // 11
                        prev3_needsubs,  // 12
                        prev3_needsubs,  // 13
                        prev3_needsubs,  // 14
                        prev3_needsubs,  // 15
                        prev3_needsubs,  // 16
                        prev3_needsubs,  // 17
                        prev3_needsubs,  // 18
                        prev3_needsubs,  // 19
                        prev3_needsubs,  // 20
                        prev3_needsubs,  // 21
                        prev3_needsubs,  // 22
                        prev3_needsubs,  // 23
                        prev3_needsubs,  // 24
                        prev3_needsubs,  // 25
                        prev3_needsubs,  // 26
                        prev3_needsubs,  // 27
                        prev3_needsubs,  // 28
                        prev3_needsubs,  // 29
                        prev3_needsubs,  // 30
                        prev3_needsubs,  // 31
                        prev3_needsubs,  // 32
                        prev3_needsubs,  // 33
                        prev3_needsubs,  // 34
                        prev3_needsubs,  // 35
                        prev3_needsubs,  // 36
                        prev3_needsubs,  // 37
                        prev3_needsubs,  // 38
                        prev3_needsubs,  // 39
                        prev3_needsubs,  // 40
                        prev3_needsubs,  // 41
                        prev3_needsubs,  // 42
                        prev3_needsubs,  // 43
                        prev3_needsubs,  // 44
                        prev3_needsubs,  // 45
                        prev3_needsubs,  // 46
                        prev3_needsubs,  // 47
                        prev3_needsubs,  // 48
                        prev3_needsubs,  // 49
                        prev3_needsubs,  // 50
                        prev3_needsubs,  // 51
                        prev3_needsubs,  // 52
                        prev3_needsubs,  // 53
                        prev3_needsubs,  // 54
                        prev3_needsubs,  // 55
                        prev3_needsubs,  // 56
                        prev3_needsubs,  // 57
                        prev3_needsubs,  // 58
                        prev3_needsubs,  // 59
                        prev3_needsubs,  // 60
                        prev3_needsubs,  // 61
                        prev3_needsubs,  // 62
                        prev3_needsubs   // 63
                    };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                    prev3 = __builtin_elementwise_sub_sat(prev3, prev3_needsubs_simd);
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb512_mask)  // GCC
                    prev3 = __builtin_ia32_psubusb512_mask(prev3, prev3_needsubs_simd, u8x64simd{}, UINT64_MAX);
# else
#  error "missing instruction"
# endif

                    return ((prev2 | prev3) & static_cast<::std::uint8_t>(0x80u)) ^ sc;
                }};

            constexpr auto check_utf8_bytes{
                [check_special_cases, check_multibyte_lengths] UWVM_ALWAYS_INLINE(u8x64simd const input,
                                                                                  u8x64simd const prev_input) constexpr noexcept -> u8x64simd
                {
                    auto const prev1{__builtin_shufflevector(prev_input,
                                                             input,
                                                             63,   // 0
                                                             64,   // 1
                                                             65,   // 2
                                                             66,   // 3
                                                             67,   // 4
                                                             68,   // 5
                                                             69,   // 6
                                                             70,   // 7
                                                             71,   // 8
                                                             72,   // 9
                                                             73,   // 10
                                                             74,   // 11
                                                             75,   // 12
                                                             76,   // 13
                                                             77,   // 14
                                                             78,   // 15
                                                             79,   // 16
                                                             80,   // 17
                                                             81,   // 18
                                                             82,   // 19
                                                             83,   // 20
                                                             84,   // 21
                                                             85,   // 22
                                                             86,   // 23
                                                             87,   // 24
                                                             88,   // 25
                                                             89,   // 26
                                                             90,   // 27
                                                             91,   // 28
                                                             92,   // 29
                                                             93,   // 30
                                                             94,   // 31
                                                             95,   // 32
                                                             96,   // 33
                                                             97,   // 34
                                                             98,   // 35
                                                             99,   // 36
                                                             100,  // 37
                                                             101,  // 38
                                                             102,  // 39
                                                             103,  // 40
                                                             104,  // 41
                                                             105,  // 42
                                                             106,  // 43
                                                             107,  // 44
                                                             108,  // 45
                                                             109,  // 46
                                                             110,  // 47
                                                             111,  // 48
                                                             112,  // 49
                                                             113,  // 50
                                                             114,  // 51
                                                             115,  // 52
                                                             116,  // 53
                                                             117,  // 54
                                                             118,  // 55
                                                             119,  // 56
                                                             120,  // 57
                                                             121,  // 58
                                                             122,  // 59
                                                             123,  // 60
                                                             124,  // 61
                                                             125,  // 62
                                                             126   // 63
                                                             )};

                    auto const sc{check_special_cases(input, prev1)};
                    return check_multibyte_lengths(input, prev_input, sc);
                }};

            constexpr auto is_incomplete{[] UWVM_ALWAYS_INLINE(u8x64simd input) constexpr noexcept -> u8x64simd
                                         {
                                             constexpr u8x64simd max_value{::std::bit_cast<u8x64simd>(u64x8simd{0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xFFFFFFFFFFFFFFFFu,
                                                                                                                0xBFDFEFFFFFFFFFFFu})};

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                                             input = __builtin_elementwise_sub_sat(input, max_value);
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb512_mask)  // GCC
                                             input = __builtin_ia32_psubusb512_mask(input, max_value, u8x64simd{}, UINT64_MAX);
# else
#  error "missing instruction"
# endif
                                             return input;
                                         }};

            u8x64simd error{};
            u8x64simd prev_input_block{};
            u8x64simd prev_incomplete{};

            // Rewind so that scalar validation starts at (or before) the previous
            // leading byte, matching simdutf's error pinpointing strategy.
            auto const rewind_for_scalar{[&] UWVM_ALWAYS_INLINE() constexpr noexcept
                                         {
                                             if(str_curr == str_begin) { return; }

                                             auto rewind_ptr{str_curr - 1uz};
                                             for(unsigned i{}; i != 4u && rewind_ptr != str_begin; ++i)
                                             {
                                                 auto const byte{static_cast<::std::uint8_t>(*rewind_ptr)};
                                                 if((byte & static_cast<::std::uint8_t>(0b1100'0000u)) != static_cast<::std::uint8_t>(0b1000'0000u)) { break; }
                                                 --rewind_ptr;
                                             }

                                             str_curr = rewind_ptr;
                                         }};

            while(static_cast<::std::size_t>(str_end - str_curr) >= sizeof(u8x64simd))
            {
                u8x64simd curr0;

                ::std::memcpy(::std::addressof(curr0), str_curr + sizeof(u8x64simd) * 0uz, sizeof(u8x64simd));

                // check null
                if constexpr(zero_illegal)
                {
                    if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                        __builtin_ia32_ucmpb512_mask(curr0, u8x64simd{}, 0x00, UINT64_MAX)
# else
#  error "missing instruction"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        return general_algorithm();
                    }
                }

                // check ascii
                bool const check_has_non_ascii{static_cast<bool>(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_movepi8_mask)
                    __builtin_ia32_movepi8_mask(::std::bit_cast<i8x64simd>(curr0))
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                    __builtin_ia32_ucmpb512_mask(curr0 & static_cast<::std::uint8_t>(0x80u), u8x64simd{}, 0x04, UINT64_MAX)
# else
#  error "missing instruction"
# endif
                        )};

                if(check_has_non_ascii)
                {
                    // utf-8

                    error |= check_utf8_bytes(curr0, prev_input_block);

                    prev_incomplete = is_incomplete(curr0);
                    prev_input_block = curr0;
                }
                else
                {
                    // ascii

                    error |= prev_incomplete;
                }

                if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                    __builtin_ia32_ucmpb512_mask(error, u8x64simd{}, 0x04, UINT64_MAX)
# else
#  error "missing instruction"
# endif
                        ) [[unlikely]]
                {
                    // Jump out to scalar processing
                    rewind_for_scalar();
                    return general_algorithm();
                }

                str_curr += sizeof(u8x64simd);

                // prefetch
                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                    reinterpret_cast<::std::byte const*>(str_curr) + 64u);
            }

            // tail handling

            ::std::uint64_t load_mask{UINT64_MAX};
            auto const last_load_predicate_size{static_cast<::std::size_t>(str_end - str_curr)};

            if(last_load_predicate_size)
            {
                load_mask >>= 64uz - last_load_predicate_size;

                u8x64simd curr0;

                using loaddquqi512_para_const_may_alias_ptr UWVM_GNU_MAY_ALIAS =
# if defined(__clang__)
                    c8x64simd const*
# else
                    char const*
# endif
                    ;

# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_loaddquqi512_mask)
                curr0 = __builtin_ia32_loaddquqi512_mask(reinterpret_cast<loaddquqi512_para_const_may_alias_ptr>(str_curr),
                                                         u8x64simd{},
                                                         load_mask);  // Ensure that only part of the loaded or calculated
# else
#  error "missing instruction"
# endif

                // check null
                if constexpr(zero_illegal)
                {
                    if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                        __builtin_ia32_ucmpb512_mask(curr0, u8x64simd{}, 0x00, load_mask)
# else
#  error "missing instruction"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        return general_algorithm();
                    }
                }

                // check ascii
                bool const check_has_non_ascii{static_cast<bool>(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_movepi8_mask)
                    __builtin_ia32_movepi8_mask(::std::bit_cast<i8x64simd>(curr0)) & load_mask
# elif defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                    __builtin_ia32_ucmpb512_mask(curr0 & static_cast<::std::uint8_t>(0x80u), u8x64simd{}, 0x04, load_mask)
# else
#  error "missing instruction"
# endif
                    )};

                if(check_has_non_ascii)
                {
                    // utf-8

                    error |= check_utf8_bytes(curr0, prev_input_block);

                    prev_incomplete = is_incomplete(curr0);
                    prev_input_block = curr0;
                }
                else
                {
                    // ascii

                    error |= prev_incomplete;
                }

                if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                    __builtin_ia32_ucmpb512_mask(error, u8x64simd{}, 0x04, UINT64_MAX)  // full
# else
#  error "missing instruction"
# endif
                    ) [[unlikely]]
                {
                    // Jump out to scalar processing
                    rewind_for_scalar();
                    return general_algorithm();
                }

                str_curr += last_load_predicate_size;
            }

            // simdutf-style EOF check: if the last processed block ended with an
            // incomplete multibyte sequence, it is an error at the end of input.
            error |= prev_incomplete;
            if(
# if defined(__AVX512BW__) && UWVM_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
                __builtin_ia32_ucmpb512_mask(error, u8x64simd{}, 0x04, UINT64_MAX)
# else
#  error "missing instruction"
# endif
                    ) [[unlikely]]
            {
                rewind_for_scalar();
                return general_algorithm();
            }

            return {str_curr, ::uwvm2::utils::utf::utf_error_code::success};

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && (defined(__AVX2__) || defined(__loongarch_asx))
            // Same algorithm as simdutf

            using i64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int64_t;
            using u64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint64_t;
            using u32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint32_t;
            using u16x16simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint16_t;
            using c8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = char;
            using u8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::uint8_t;
            using i8x32simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::std::int8_t;

            constexpr auto check_special_cases{
                [] UWVM_ALWAYS_INLINE(u8x32simd const input, u8x32simd const prev1) constexpr noexcept -> u8x32simd
                {
                    constexpr ::std::uint8_t too_short{1u << 0u};
                    // 11??'???? 0???'????
                    // 11??'???? 11??'????
                    constexpr ::std::uint8_t too_long{1u << 1u};
                    // 0???'???? 10??'????
                    constexpr ::std::uint8_t overlong_3{1u << 2u};
                    // 1110'0000 100?'????
                    constexpr ::std::uint8_t surrogate{1u << 4u};
                    // 1110'1101 101?'????
                    constexpr ::std::uint8_t overlong_2{1u << 5u};
                    // 1100'000? 10??'????
                    constexpr ::std::uint8_t two_conts{1u << 7u};
                    // 10??'???? 10??'????
                    constexpr ::std::uint8_t too_large{1u << 3u};
                    // 1111'0100 1001'????
                    // 1111'0100 101?'????
                    // 1111'0101 1001'????
                    // 1111'0101 101?'????
                    // 1111'011? 1001'????
                    // 1111'011? 101?'????
                    // 1111'1??? 1001'????
                    // 1111'1??? 101?'????
                    constexpr ::std::uint8_t too_large_1000{1u << 6u};
                    // 1111'0101 1000'????
                    // 1111'011? 1000'????
                    // 1111'1??? 1000'????
                    constexpr ::std::uint8_t overlong_4{1u << 6u};
                    // 1111'0000 1000'????
                    [[maybe_unused]] constexpr ::std::uint8_t forbidden{0xFFu};

                    constexpr u8x32simd lookup_b1h{// 0 - 15
                                                   // 0???'???? ????'???? <ASCII in byte 1>
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   // 10??'???? ????'???? <continuation in byte 1>
                                                   two_conts,
                                                   two_conts,
                                                   two_conts,
                                                   two_conts,
                                                   // 1100'???? ????'???? <two byte lead in byte 1>
                                                   too_short | overlong_2,
                                                   // 1101'???? ????'???? <two byte lead in byte 1>
                                                   too_short,
                                                   // 1110'???? ????'???? <three byte lead in byte 1>
                                                   too_short | overlong_3 | surrogate,
                                                   // 1111'???? ????'???? <four+ byte lead in byte 1>
                                                   too_short | too_large | too_large_1000 | overlong_4,
                                                   // 16 - 31
                                                   // 0???'???? ????'???? <ASCII in byte 1>
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   // 10??'???? ????'???? <continuation in byte 1>
                                                   two_conts,
                                                   two_conts,
                                                   two_conts,
                                                   two_conts,
                                                   // 1100'???? ????'???? <two byte lead in byte 1>
                                                   too_short | overlong_2,
                                                   // 1101'???? ????'???? <two byte lead in byte 1>
                                                   too_short,
                                                   // 1110'???? ????'???? <three byte lead in byte 1>
                                                   too_short | overlong_3 | surrogate,
                                                   // 1111'???? ????'???? <four+ byte lead in byte 1>
                                                   too_short | too_large | too_large_1000 | overlong_4};

                    // These all have ???? in byte 1.
                    constexpr ::std::uint8_t carry{too_short | too_long | two_conts};

                    constexpr u8x32simd lookup_b1l{// 0 - 15
                                                   // ????'0000 ????'????
                                                   carry | overlong_3 | overlong_2 | overlong_4,
                                                   // ????'0001 ????'????
                                                   carry | overlong_2,
                                                   // ????'001? ????'????
                                                   carry,
                                                   carry,
                                                   // ????'0100 ????'????
                                                   carry | too_large,
                                                   // ????'0101 ????'????
                                                   carry | too_large | too_large_1000,
                                                   // ????'011? ????'????
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // ????'1??? ????'????
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // ????'1101 ????'????
                                                   carry | too_large | too_large_1000 | surrogate,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // 16 - 31
                                                   // ????'0000 ????'????
                                                   carry | overlong_3 | overlong_2 | overlong_4,
                                                   // ????'0001 ????'????
                                                   carry | overlong_2,
                                                   // ????'001? ????'????
                                                   carry,
                                                   carry,
                                                   // ????'0100 ????'????
                                                   carry | too_large,
                                                   // ????'0101 ????'????
                                                   carry | too_large | too_large_1000,
                                                   // ????'011? ????'????
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // ????'1??? ????'????
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // ????'1101 ????'????
                                                   carry | too_large | too_large_1000 | surrogate,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000};

                    constexpr u8x32simd lookup_b2h{// 0 - 15
                                                   // ????'???? 0???'????
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   // ????'???? 1000'????
                                                   too_long | overlong_2 | two_conts | overlong_3 | too_large_1000 | overlong_4,
                                                   // ????'???? 1001'????
                                                   too_long | overlong_2 | two_conts | overlong_3 | too_large,
                                                   // ????'???? 101?'????
                                                   too_long | overlong_2 | two_conts | surrogate | too_large,
                                                   too_long | overlong_2 | two_conts | surrogate | too_large,
                                                   // ????'???? 11??'????
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   // 16 - 31
                                                   // ????'???? 0???'????
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   // ????'???? 1000'????
                                                   too_long | overlong_2 | two_conts | overlong_3 | too_large_1000 | overlong_4,
                                                   // ????'???? 1001'????
                                                   too_long | overlong_2 | two_conts | overlong_3 | too_large,
                                                   // ????'???? 101?'????
                                                   too_long | overlong_2 | two_conts | surrogate | too_large,
                                                   too_long | overlong_2 | two_conts | surrogate | too_large,
                                                   // ????'???? 11??'????
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short};

                    auto const prev1_shr4{::std::bit_cast<u8x32simd>(::std::bit_cast<u16x16simd>(prev1) >> static_cast<::std::uint16_t>(4u)) &
                                          static_cast<::std::uint8_t>(0xFFu >> 4u)};

                    u8x32simd b1h;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                    b1h = ::std::bit_cast<u8x32simd>(__builtin_ia32_pshufb256(lookup_b1h, prev1_shr4));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                    b1h = ::std::bit_cast<u8x32simd>(__builtin_lasx_xvshuf_b(u8x32simd{}, lookup_b1h, prev1_shr4 & static_cast<::std::uint8_t>(0x1Fu)));
# else
#  error "missing instructions"
# endif

                    auto const prev1_andF{prev1 & static_cast<::std::uint8_t>(0x0Fu)};

                    u8x32simd b1l;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                    b1l = ::std::bit_cast<u8x32simd>(__builtin_ia32_pshufb256(lookup_b1l, prev1_andF));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                    b1l = ::std::bit_cast<u8x32simd>(__builtin_lasx_xvshuf_b(u8x32simd{}, lookup_b1l, prev1_andF & static_cast<::std::uint8_t>(0x1Fu)));
# else
#  error "missing instructions"
# endif

                    auto const inp_shr4{::std::bit_cast<u8x32simd>(::std::bit_cast<u16x16simd>(input) >> static_cast<::std::uint16_t>(4u)) &
                                        static_cast<::std::uint8_t>(0xFFu >> 4u)};

                    u8x32simd b2h;

# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb256)
                    b2h = ::std::bit_cast<u8x32simd>(__builtin_ia32_pshufb256(lookup_b2h, inp_shr4));
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvshuf_b)
                    b2h = ::std::bit_cast<u8x32simd>(__builtin_lasx_xvshuf_b(u8x32simd{}, lookup_b2h, inp_shr4 & static_cast<::std::uint8_t>(0x1Fu)));
# else
#  error "missing instructions"
# endif

                    return b1h & b1l & b2h;
                }};

            constexpr auto check_multibyte_lengths{
                [] UWVM_ALWAYS_INLINE(u8x32simd const input, u8x32simd const prev_input, u8x32simd const sc) constexpr noexcept -> u8x32simd
                {
                    auto prev2{__builtin_shufflevector(prev_input,
                                                       input,
                                                       30,  // 0
                                                       31,  // 1
                                                       32,  // 2
                                                       33,  // 3
                                                       34,  // 4
                                                       35,  // 5
                                                       36,  // 6
                                                       37,  // 7
                                                       38,  // 8
                                                       39,  // 9
                                                       40,  // 10
                                                       41,  // 11
                                                       42,  // 12
                                                       43,  // 13
                                                       44,  // 14
                                                       45,  // 15
                                                       46,  // 16
                                                       47,  // 17
                                                       48,  // 18
                                                       49,  // 19
                                                       50,  // 20
                                                       51,  // 21
                                                       52,  // 22
                                                       53,  // 23
                                                       54,  // 24
                                                       55,  // 25
                                                       56,  // 26
                                                       57,  // 27
                                                       58,  // 28
                                                       59,  // 29
                                                       60,  // 30
                                                       61   // 31
                                                       )};

                    auto prev3{__builtin_shufflevector(prev_input,
                                                       input,
                                                       29,  // 0
                                                       30,  // 1
                                                       31,  // 2
                                                       32,  // 3
                                                       33,  // 4
                                                       34,  // 5
                                                       35,  // 6
                                                       36,  // 7
                                                       37,  // 8
                                                       38,  // 9
                                                       39,  // 10
                                                       40,  // 11
                                                       41,  // 12
                                                       42,  // 13
                                                       43,  // 14
                                                       44,  // 15
                                                       45,  // 16
                                                       46,  // 17
                                                       47,  // 18
                                                       48,  // 19
                                                       49,  // 20
                                                       50,  // 21
                                                       51,  // 22
                                                       52,  // 23
                                                       53,  // 24
                                                       54,  // 25
                                                       55,  // 26
                                                       56,  // 27
                                                       57,  // 28
                                                       58,  // 29
                                                       59,  // 30
                                                       60   // 31
                                                       )};

                    constexpr auto prev2_needsubs{static_cast<::std::uint8_t>(0xE0u - 0x80u)};
                    u8x32simd const prev2_needsubs_simd{
                        prev2_needsubs,  // 0
                        prev2_needsubs,  // 1
                        prev2_needsubs,  // 2
                        prev2_needsubs,  // 3
                        prev2_needsubs,  // 4
                        prev2_needsubs,  // 5
                        prev2_needsubs,  // 6
                        prev2_needsubs,  // 7
                        prev2_needsubs,  // 8
                        prev2_needsubs,  // 9
                        prev2_needsubs,  // 10
                        prev2_needsubs,  // 11
                        prev2_needsubs,  // 12
                        prev2_needsubs,  // 13
                        prev2_needsubs,  // 14
                        prev2_needsubs,  // 15
                        prev2_needsubs,  // 16
                        prev2_needsubs,  // 17
                        prev2_needsubs,  // 18
                        prev2_needsubs,  // 19
                        prev2_needsubs,  // 20
                        prev2_needsubs,  // 21
                        prev2_needsubs,  // 22
                        prev2_needsubs,  // 23
                        prev2_needsubs,  // 24
                        prev2_needsubs,  // 25
                        prev2_needsubs,  // 26
                        prev2_needsubs,  // 27
                        prev2_needsubs,  // 28
                        prev2_needsubs,  // 29
                        prev2_needsubs,  // 30
                        prev2_needsubs   // 31
                    };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                    prev2 = __builtin_elementwise_sub_sat(prev2, prev2_needsubs_simd);
# elif defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb256)
                    prev2 = __builtin_ia32_psubusb256(prev2, prev2_needsubs_simd);
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvssub_bu)
                    prev2 = __builtin_lasx_xvssub_bu(prev2, prev2_needsubs_simd);  /// @todo need test
# else
#  error "missing instruction"
# endif

                    constexpr auto prev3_needsubs{static_cast<::std::uint8_t>(0xF0u - 0x80u)};
                    u8x32simd const prev3_needsubs_simd{
                        prev3_needsubs,  // 0
                        prev3_needsubs,  // 1
                        prev3_needsubs,  // 2
                        prev3_needsubs,  // 3
                        prev3_needsubs,  // 4
                        prev3_needsubs,  // 5
                        prev3_needsubs,  // 6
                        prev3_needsubs,  // 7
                        prev3_needsubs,  // 8
                        prev3_needsubs,  // 9
                        prev3_needsubs,  // 10
                        prev3_needsubs,  // 11
                        prev3_needsubs,  // 12
                        prev3_needsubs,  // 13
                        prev3_needsubs,  // 14
                        prev3_needsubs,  // 15
                        prev3_needsubs,  // 16
                        prev3_needsubs,  // 17
                        prev3_needsubs,  // 18
                        prev3_needsubs,  // 19
                        prev3_needsubs,  // 20
                        prev3_needsubs,  // 21
                        prev3_needsubs,  // 22
                        prev3_needsubs,  // 23
                        prev3_needsubs,  // 24
                        prev3_needsubs,  // 25
                        prev3_needsubs,  // 26
                        prev3_needsubs,  // 27
                        prev3_needsubs,  // 28
                        prev3_needsubs,  // 29
                        prev3_needsubs,  // 30
                        prev3_needsubs   // 31
                    };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                    prev3 = __builtin_elementwise_sub_sat(prev3, prev3_needsubs_simd);
# elif defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb256)
                    prev3 = __builtin_ia32_psubusb256(prev3, prev3_needsubs_simd);
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvssub_bu)
                    prev3 = __builtin_lasx_xvssub_bu(prev3, prev3_needsubs_simd);  /// @todo need test
# else
#  error "missing instruction"
# endif

                    return ((prev2 | prev3) & static_cast<::std::uint8_t>(0x80u)) ^ sc;
                }};

            constexpr auto check_utf8_bytes{
                [check_special_cases, check_multibyte_lengths] UWVM_ALWAYS_INLINE(u8x32simd const input,
                                                                                  u8x32simd const prev_input) constexpr noexcept -> u8x32simd
                {
                    auto const prev1{__builtin_shufflevector(prev_input,
                                                             input,
                                                             31,  // 0
                                                             32,  // 1
                                                             33,  // 2
                                                             34,  // 3
                                                             35,  // 4
                                                             36,  // 5
                                                             37,  // 6
                                                             38,  // 7
                                                             39,  // 8
                                                             40,  // 9
                                                             41,  // 10
                                                             42,  // 11
                                                             43,  // 12
                                                             44,  // 13
                                                             45,  // 14
                                                             46,  // 15
                                                             47,  // 16
                                                             48,  // 17
                                                             49,  // 18
                                                             50,  // 19
                                                             51,  // 20
                                                             52,  // 21
                                                             53,  // 22
                                                             54,  // 23
                                                             55,  // 24
                                                             56,  // 25
                                                             57,  // 26
                                                             58,  // 27
                                                             59,  // 28
                                                             60,  // 29
                                                             61,  // 30
                                                             62   // 31
                                                             )};

                    auto const sc{check_special_cases(input, prev1)};
                    return check_multibyte_lengths(input, prev_input, sc);
                }};

            constexpr auto is_incomplete{[] UWVM_ALWAYS_INLINE(u8x32simd input) constexpr noexcept -> u8x32simd
                                         {
                                             constexpr u8x32simd max_value{
                                                 255u,              // 0
                                                 255u,              // 1
                                                 255u,              // 2
                                                 255u,              // 3
                                                 255u,              // 4
                                                 255u,              // 5
                                                 255u,              // 6
                                                 255u,              // 7
                                                 255u,              // 8
                                                 255u,              // 9
                                                 255u,              // 10
                                                 255u,              // 11
                                                 255u,              // 12
                                                 255u,              // 13
                                                 255u,              // 14
                                                 255u,              // 15
                                                 255u,              // 16
                                                 255u,              // 17
                                                 255u,              // 18
                                                 255u,              // 19
                                                 255u,              // 20
                                                 255u,              // 21
                                                 255u,              // 22
                                                 255u,              // 23
                                                 255u,              // 24
                                                 255u,              // 25
                                                 255u,              // 26
                                                 255u,              // 27
                                                 255u,              // 28
                                                 0b11110000u - 1u,  // 29
                                                 0b11100000u - 1u,  // 30
                                                 0b11000000u - 1u   // 31
                                             };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                                             input = __builtin_elementwise_sub_sat(input, max_value);
# elif defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb256)
                                             input = __builtin_ia32_psubusb256(input, max_value);
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xvssub_bu)
                                             input = __builtin_lasx_xvssub_bu(input, max_value);  /// @todo need test
# else
#  error "missing instruction"
# endif
                                             return input;
                                         }};

            u8x32simd error{};
            u8x32simd prev_input_block{};
            u8x32simd prev_incomplete{};

            while(static_cast<::std::size_t>(str_end - str_curr) >= sizeof(u8x32simd) * 2uz)
            {
                u8x32simd curr0;
                u8x32simd curr1;

                ::std::memcpy(::std::addressof(curr0), str_curr + sizeof(u8x32simd) * 0uz, sizeof(u8x32simd));
                ::std::memcpy(::std::addressof(curr1), str_curr + sizeof(u8x32simd) * 1uz, sizeof(u8x32simd));

                // check null
                if constexpr(zero_illegal)
                {
                    auto const curr0_test{curr0 == static_cast<::std::uint8_t>(0u)};

                    if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                        !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(curr0_test), ::std::bit_cast<i64x4simd>(curr0_test))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                        __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(curr0_test))  /// @todo need check
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        break;
                    }

                    auto const curr1_test{curr1 == static_cast<::std::uint8_t>(0u)};

                    if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                        !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(curr1_test), ::std::bit_cast<i64x4simd>(curr1_test))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                        __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(curr1_test))  /// @todo need check
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        break;
                    }
                }

                // check ascii

                auto const check_or{curr0 | curr1};
                bool const check_has_non_ascii{static_cast<bool>(
# if defined(__AVX2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
                    __builtin_ia32_pmovmskb256(::std::bit_cast<c8x32simd>(check_or))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                    __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(check_or & static_cast<::std::uint8_t>(0x80u)))  /// @todo need check
# else
#  error "missing instructions"
# endif
                        )};

                if(check_has_non_ascii)
                {
                    // utf-8

                    error |= check_utf8_bytes(curr0, prev_input_block);
                    error |= check_utf8_bytes(curr1, curr0);

                    prev_incomplete = is_incomplete(curr1);
                    prev_input_block = curr1;
                }
                else
                {
                    // ascii

                    error |= prev_incomplete;
                }
                if(
# if defined(__AVX__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz256)
                    !__builtin_ia32_ptestz256(::std::bit_cast<i64x4simd>(error), ::std::bit_cast<i64x4simd>(error))
# elif defined(__loongarch_asx) && UWVM_HAS_BUILTIN(__builtin_lasx_xbnz_v)
                    __builtin_lasx_xbnz_v(::std::bit_cast<u8x32simd>(error))  /// @todo need check
# else
#  error "missing instructions"
# endif
                        ) [[unlikely]]
                {
                    // Jump out to scalar processing
                    break;
                }

                str_curr += sizeof(u8x32simd) * 2uz;

                // prefetch
                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                    reinterpret_cast<::std::byte const*>(str_curr) + 64u);
            }

            // tail handling
            // If the previous SIMD block ended with an incomplete UTF-8 sequence,
            // rewind so that the scalar loop validates it together with the
            // remainder (simdutf-style).
            if(str_curr != str_begin)
            {
                auto const processed{static_cast<::std::size_t>(str_curr - str_begin)};
                unsigned rewind{};  // 0..3

                if(processed >= 3uz && static_cast<::std::uint8_t>(*(str_curr - 3u)) >= static_cast<::std::uint8_t>(0b1111'0000u)) { rewind = 3u; }
                else if(processed >= 2uz && static_cast<::std::uint8_t>(*(str_curr - 2u)) >= static_cast<::std::uint8_t>(0b1110'0000u)) { rewind = 2u; }
                else if(processed >= 1uz && static_cast<::std::uint8_t>(*(str_curr - 1u)) >= static_cast<::std::uint8_t>(0b1100'0000u)) { rewind = 1u; }

                str_curr -= rewind;
            }

            while(str_curr != str_end)
            {
                auto const str_curr_val{*str_curr};

                if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                {
                    if constexpr(zero_illegal)
                    {
                        if(str_curr_val == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                    }

                    ++str_curr;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{2u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    str_curr += utf8_length;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p2_val{*(str_curr + 2u)};

                    if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{3u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                    utf8_b2 &= 0x3Fu;
                    utf8_b2 <<= (utf8_length - 3u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                    }

                    str_curr += utf8_length;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p2_val{*(str_curr + 2u)};

                    if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p3_val{*(str_curr + 3u)};

                    if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{4u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                    utf8_b2 &= 0x3Fu;
                    utf8_b2 <<= (utf8_length - 3u) * 6u;

                    ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                    utf8_b3 &= 0x3Fu;
                    utf8_b3 <<= (utf8_length - 4u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint}; }

                    str_curr += utf8_length;
                }
                else
                {
                    if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                    }
                    else
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                    }
                }
            }

            return {str_curr, ::uwvm2::utils::utf::utf_error_code::success};

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && UWVM_HAS_BUILTIN(__builtin_shufflevector) && defined(__LITTLE_ENDIAN__) &&                           \
    (defined(__SSSE3__) || defined(__ARM_NEON) || defined(__loongarch_sx))

            // Same algorithm as simdutf

            using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
            using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
            using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
            using u16x8simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint16_t;
            using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
            using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
            using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

            constexpr auto check_special_cases{
                [] UWVM_ALWAYS_INLINE(u8x16simd const input, u8x16simd const prev1) constexpr noexcept -> u8x16simd
                {
                    constexpr ::std::uint8_t too_short{1u << 0u};
                    // 11??'???? 0???'????
                    // 11??'???? 11??'????
                    constexpr ::std::uint8_t too_long{1u << 1u};
                    // 0???'???? 10??'????
                    constexpr ::std::uint8_t overlong_3{1u << 2u};
                    // 1110'0000 100?'????
                    constexpr ::std::uint8_t surrogate{1u << 4u};
                    // 1110'1101 101?'????
                    constexpr ::std::uint8_t overlong_2{1u << 5u};
                    // 1100'000? 10??'????
                    constexpr ::std::uint8_t two_conts{1u << 7u};
                    // 10??'???? 10??'????
                    constexpr ::std::uint8_t too_large{1u << 3u};
                    // 1111'0100 1001'????
                    // 1111'0100 101?'????
                    // 1111'0101 1001'????
                    // 1111'0101 101?'????
                    // 1111'011? 1001'????
                    // 1111'011? 101?'????
                    // 1111'1??? 1001'????
                    // 1111'1??? 101?'????
                    constexpr ::std::uint8_t too_large_1000{1u << 6u};
                    // 1111'0101 1000'????
                    // 1111'011? 1000'????
                    // 1111'1??? 1000'????
                    constexpr ::std::uint8_t overlong_4{1u << 6u};
                    // 1111'0000 1000'????
                    [[maybe_unused]] constexpr ::std::uint8_t forbidden{0xFFu};

                    constexpr u8x16simd lookup_b1h{// 0???'???? ????'???? <ASCII in byte 1>
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   too_long,
                                                   // 10??'???? ????'???? <continuation in byte 1>
                                                   two_conts,
                                                   two_conts,
                                                   two_conts,
                                                   two_conts,
                                                   // 1100'???? ????'???? <two byte lead in byte 1>
                                                   too_short | overlong_2,
                                                   // 1101'???? ????'???? <two byte lead in byte 1>
                                                   too_short,
                                                   // 1110'???? ????'???? <three byte lead in byte 1>
                                                   too_short | overlong_3 | surrogate,
                                                   // 1111'???? ????'???? <four+ byte lead in byte 1>
                                                   too_short | too_large | too_large_1000 | overlong_4};

                    // These all have ???? in byte 1.
                    constexpr ::std::uint8_t carry{too_short | too_long | two_conts};

                    constexpr u8x16simd lookup_b1l{// ????'0000 ????'????
                                                   carry | overlong_3 | overlong_2 | overlong_4,
                                                   // ????'0001 ????'????
                                                   carry | overlong_2,
                                                   // ????'001? ????'????
                                                   carry,
                                                   carry,
                                                   // ????'0100 ????'????
                                                   carry | too_large,
                                                   // ????'0101 ????'????
                                                   carry | too_large | too_large_1000,
                                                   // ????'011? ????'????
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // ????'1??? ????'????
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000,
                                                   // ????'1101 ????'????
                                                   carry | too_large | too_large_1000 | surrogate,
                                                   carry | too_large | too_large_1000,
                                                   carry | too_large | too_large_1000};

                    constexpr u8x16simd lookup_b2h{// ????'???? 0???'????
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   // ????'???? 1000'????
                                                   too_long | overlong_2 | two_conts | overlong_3 | too_large_1000 | overlong_4,
                                                   // ????'???? 1001'????
                                                   too_long | overlong_2 | two_conts | overlong_3 | too_large,
                                                   // ????'???? 101?'????
                                                   too_long | overlong_2 | two_conts | surrogate | too_large,
                                                   too_long | overlong_2 | two_conts | surrogate | too_large,
                                                   // ????'???? 11??'????
                                                   too_short,
                                                   too_short,
                                                   too_short,
                                                   too_short};

                    auto const prev1_shr4{::std::bit_cast<u8x16simd>(::std::bit_cast<u16x8simd>(prev1) >> static_cast<::std::uint16_t>(4u)) &
                                          static_cast<::std::uint8_t>(0xFFu >> 4u)};

                    u8x16simd b1h;

# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                    b1h = ::std::bit_cast<u8x16simd>(__builtin_ia32_pshufb128(lookup_b1h, prev1_shr4));
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_qtbl1v16qi_uuu)  // GCC
                    b1h = __builtin_aarch64_qtbl1v16qi_uuu(lookup_b1h, prev1_shr4);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqtbl1q_v)          // Clang
                    b1h = ::std::bit_cast<u8x16simd>(
                        __builtin_neon_vqtbl1q_v(::std::bit_cast<i8x16simd>(lookup_b1h), ::std::bit_cast<i8x16simd>(prev1_shr4), 48));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                    b1h = ::std::bit_cast<u8x16simd>(__builtin_lsx_vshuf_b(lookup_b1h, lookup_b1h, prev1_shr4));  /// @todo need test
# else
#  error "missing instructions"
# endif

                    auto const prev1_andF{prev1 & static_cast<::std::uint8_t>(0x0Fu)};

                    u8x16simd b1l;

# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                    b1l = ::std::bit_cast<u8x16simd>(__builtin_ia32_pshufb128(lookup_b1l, prev1_andF));
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_qtbl1v16qi_uuu)  // GCC
                    b1l = __builtin_aarch64_qtbl1v16qi_uuu(lookup_b1l, prev1_andF);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqtbl1q_v)          // Clang
                    b1l = ::std::bit_cast<u8x16simd>(
                        __builtin_neon_vqtbl1q_v(::std::bit_cast<i8x16simd>(lookup_b1l), ::std::bit_cast<i8x16simd>(prev1_andF), 48));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                    b1l = ::std::bit_cast<u8x16simd>(__builtin_lsx_vshuf_b(lookup_b1l, lookup_b1l, prev1_andF));  /// @todo need test
# else
#  error "missing instructions"
# endif

                    auto const inp_shr4{::std::bit_cast<u8x16simd>(::std::bit_cast<u16x8simd>(input) >> static_cast<::std::uint16_t>(4u)) &
                                        static_cast<::std::uint8_t>(0xFFu >> 4u)};

                    u8x16simd b2h;
# if defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
                    b2h = ::std::bit_cast<u8x16simd>(__builtin_ia32_pshufb128(lookup_b2h, inp_shr4));
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_qtbl1v16qi_uuu)  // GCC
                    b2h = __builtin_aarch64_qtbl1v16qi_uuu(lookup_b2h, inp_shr4);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqtbl1q_v)          // Clang
                    b2h =
                        ::std::bit_cast<u8x16simd>(__builtin_neon_vqtbl1q_v(::std::bit_cast<i8x16simd>(lookup_b2h), ::std::bit_cast<i8x16simd>(inp_shr4), 48));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vshuf_b)
                    b2h = ::std::bit_cast<u8x16simd>(__builtin_lsx_vshuf_b(lookup_b2h, lookup_b2h, inp_shr4));  /// @todo need test
# else
#  error "missing instructions"
# endif

                    return b1h & b1l & b2h;
                }};

            constexpr auto check_multibyte_lengths{
                [] UWVM_ALWAYS_INLINE(u8x16simd const input, u8x16simd const prev_input, u8x16simd const sc) constexpr noexcept -> u8x16simd
                {
                    auto prev2{__builtin_shufflevector(prev_input,
                                                       input,
                                                       14,  // 0
                                                       15,  // 1
                                                       16,  // 2
                                                       17,  // 3
                                                       18,  // 4
                                                       19,  // 5
                                                       20,  // 6
                                                       21,  // 7
                                                       22,  // 8
                                                       23,  // 9
                                                       24,  // 10
                                                       25,  // 11
                                                       26,  // 12
                                                       27,  // 13
                                                       28,  // 14
                                                       29   // 15
                                                       )};

                    auto prev3{__builtin_shufflevector(prev_input,
                                                       input,
                                                       13,  // 0
                                                       14,  // 1
                                                       15,  // 2
                                                       16,  // 3
                                                       17,  // 4
                                                       18,  // 5
                                                       19,  // 6
                                                       20,  // 7
                                                       21,  // 8
                                                       22,  // 9
                                                       23,  // 10
                                                       24,  // 11
                                                       25,  // 12
                                                       26,  // 13
                                                       27,  // 14
                                                       28   // 15
                                                       )};

                    constexpr auto prev2_needsubs{static_cast<::std::uint8_t>(0xE0u - 0x80u)};
                    u8x16simd const prev2_needsubs_simd{
                        prev2_needsubs,  // 0
                        prev2_needsubs,  // 1
                        prev2_needsubs,  // 2
                        prev2_needsubs,  // 3
                        prev2_needsubs,  // 4
                        prev2_needsubs,  // 5
                        prev2_needsubs,  // 6
                        prev2_needsubs,  // 7
                        prev2_needsubs,  // 8
                        prev2_needsubs,  // 9
                        prev2_needsubs,  // 10
                        prev2_needsubs,  // 11
                        prev2_needsubs,  // 12
                        prev2_needsubs,  // 13
                        prev2_needsubs,  // 14
                        prev2_needsubs   // 15
                    };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                    prev2 = __builtin_elementwise_sub_sat(prev2, prev2_needsubs_simd);
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb128)
                    prev2 = __builtin_ia32_psubusb128(prev2, prev2_needsubs_simd);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_ussubv16qi_uuu)  // GCC
                    prev2 = __builtin_aarch64_ussubv16qi_uuu(prev2, prev2_needsubs_simd);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqsubq_v)           // Clang
                    prev2 = ::std::bit_cast<u8x16simd>(
                        __builtin_neon_vqsubq_v(::std::bit_cast<i8x16simd>(prev2), ::std::bit_cast<i8x16simd>(prev2_needsubs_simd), 48));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vssub_bu)
                    prev2 = __builtin_lsx_vssub_bu(prev2, prev2_needsubs_simd);  /// @todo need test
# else
#  error "missing instruction"
# endif

                    constexpr auto prev3_needsubs{static_cast<::std::uint8_t>(0xF0u - 0x80u)};
                    u8x16simd const prev3_needsubs_simd{
                        prev3_needsubs,  // 0
                        prev3_needsubs,  // 1
                        prev3_needsubs,  // 2
                        prev3_needsubs,  // 3
                        prev3_needsubs,  // 4
                        prev3_needsubs,  // 5
                        prev3_needsubs,  // 6
                        prev3_needsubs,  // 7
                        prev3_needsubs,  // 8
                        prev3_needsubs,  // 9
                        prev3_needsubs,  // 10
                        prev3_needsubs,  // 11
                        prev3_needsubs,  // 12
                        prev3_needsubs,  // 13
                        prev3_needsubs,  // 14
                        prev3_needsubs   // 15
                    };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                    prev3 = __builtin_elementwise_sub_sat(prev3, prev3_needsubs_simd);
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb128)
                    prev3 = __builtin_ia32_psubusb128(prev3, prev3_needsubs_simd);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_ussubv16qi_uuu)  // GCC
                    prev3 = __builtin_aarch64_ussubv16qi_uuu(prev3, prev3_needsubs_simd);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqsubq_v)           // Clang
                    prev3 = ::std::bit_cast<u8x16simd>(
                        __builtin_neon_vqsubq_v(::std::bit_cast<i8x16simd>(prev3), ::std::bit_cast<i8x16simd>(prev3_needsubs_simd), 48));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vssub_bu)
                    prev3 = __builtin_lsx_vssub_bu(prev3, prev3_needsubs_simd);  /// @todo need test
# else
#  error "missing instruction"
# endif

                    return ((prev2 | prev3) & static_cast<::std::uint8_t>(0x80u)) ^ sc;
                }};

            constexpr auto check_utf8_bytes{
                [check_special_cases, check_multibyte_lengths] UWVM_ALWAYS_INLINE(u8x16simd const input,
                                                                                  u8x16simd const prev_input) constexpr noexcept -> u8x16simd
                {
                    auto const prev1{__builtin_shufflevector(prev_input,
                                                             input,
                                                             15,  // 0
                                                             16,  // 1
                                                             17,  // 2
                                                             18,  // 3
                                                             19,  // 4
                                                             20,  // 5
                                                             21,  // 6
                                                             22,  // 7
                                                             23,  // 8
                                                             24,  // 9
                                                             25,  // 10
                                                             26,  // 11
                                                             27,  // 12
                                                             28,  // 13
                                                             29,  // 14
                                                             30   // 15
                                                             )};

                    auto const sc{check_special_cases(input, prev1)};
                    return check_multibyte_lengths(input, prev_input, sc);
                }};

            constexpr auto is_incomplete{[] UWVM_ALWAYS_INLINE(u8x16simd input) constexpr noexcept -> u8x16simd
                                         {
                                             constexpr u8x16simd max_value{
                                                 255u,              // 0
                                                 255u,              // 1
                                                 255u,              // 2
                                                 255u,              // 3
                                                 255u,              // 4
                                                 255u,              // 5
                                                 255u,              // 6
                                                 255u,              // 7
                                                 255u,              // 8
                                                 255u,              // 9
                                                 255u,              // 10
                                                 255u,              // 11
                                                 255u,              // 12
                                                 0b11110000u - 1u,  // 13
                                                 0b11100000u - 1u,  // 14
                                                 0b11000000u - 1u   // 15
                                             };

# if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)  // Clang
                                             input = __builtin_elementwise_sub_sat(input, max_value);
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_psubusb128)
                                             input = __builtin_ia32_psubusb128(input, max_value);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_ussubv16qi_uuu)  // GCC
                                             input = __builtin_aarch64_ussubv16qi_uuu(input, max_value);
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqsubq_v)           // Clang
                                             input = ::std::bit_cast<u8x16simd>(
                                                 __builtin_neon_vqsubq_v(::std::bit_cast<i8x16simd>(input), ::std::bit_cast<i8x16simd>(max_value), 48));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vssub_bu)
                                             input = __builtin_lsx_vssub_bu(input, max_value);  /// @todo need test
# else
#  error "missing instruction"
# endif

                                             return input;
                                         }};

            u8x16simd error{};
            u8x16simd prev_input_block{};
            u8x16simd prev_incomplete{};

            while(static_cast<::std::size_t>(str_end - str_curr) >= sizeof(u8x16simd) * 4uz)
            {
                u8x16simd curr0;
                u8x16simd curr1;
                u8x16simd curr2;
                u8x16simd curr3;

                ::std::memcpy(::std::addressof(curr0), str_curr + sizeof(u8x16simd) * 0uz, sizeof(u8x16simd));
                ::std::memcpy(::std::addressof(curr1), str_curr + sizeof(u8x16simd) * 1uz, sizeof(u8x16simd));
                ::std::memcpy(::std::addressof(curr2), str_curr + sizeof(u8x16simd) * 2uz, sizeof(u8x16simd));
                ::std::memcpy(::std::addressof(curr3), str_curr + sizeof(u8x16simd) * 3uz, sizeof(u8x16simd));

                // check null
                if constexpr(zero_illegal)
                {
                    auto const curr0_test{curr0 == static_cast<::std::uint8_t>(0u)};

                    if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                        !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(curr0_test), ::std::bit_cast<i64x2simd>(curr0_test))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                        __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(curr0_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                        !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~curr0_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                        __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(curr0_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                        __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(curr0_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                        __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(curr0_test))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                        __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(curr0_test))  /// @todo need check
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        break;
                    }

                    auto const curr1_test{curr1 == static_cast<::std::uint8_t>(0u)};

                    if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                        !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(curr1_test), ::std::bit_cast<i64x2simd>(curr1_test))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                        __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(curr1_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                        !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~curr1_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                        __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(curr1_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                        __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(curr1_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                        __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(curr1_test))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                        __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(curr1_test))  /// @todo need check
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        break;
                    }

                    auto const curr2_test{curr2 == static_cast<::std::uint8_t>(0u)};

                    if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                        !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(curr2_test), ::std::bit_cast<i64x2simd>(curr2_test))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                        __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(curr2_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                        !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~curr2_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                        __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(curr2_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                        __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(curr2_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                        __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(curr2_test))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                        __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(curr2_test))  /// @todo need check
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        break;
                    }

                    auto const curr3_test{curr3 == static_cast<::std::uint8_t>(0u)};

                    if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                        !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(curr3_test), ::std::bit_cast<i64x2simd>(curr3_test))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                        __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(curr3_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                        !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~curr3_test))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                        __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(curr3_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                        __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(curr3_test))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                        __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(curr3_test))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                        __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(curr3_test))  /// @todo need check
# else
#  error "missing instructions"
# endif
                            ) [[unlikely]]
                    {
                        // Jump out to scalar processing
                        break;
                    }
                }

                // check ascii

                auto const check_or{curr0 | curr1 | curr2 | curr3};
                bool const check_has_non_ascii{static_cast<bool>(
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                    __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(check_or))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                    __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(check_or))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u8)                   // Only supported by clang
                    __builtin_neon_vmaxvq_u8(check_or) & static_cast<::std::uint8_t>(0x80u)
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                    __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(check_or) & static_cast<::std::uint32_t>(0x80808080u))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                    __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(check_or) & static_cast<::std::uint32_t>(0x80808080u))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                    __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(check_or & static_cast<::std::uint8_t>(0x80u)))  /// @todo need check
# else
#  error "missing instructions"
# endif
                        )};

                if(check_has_non_ascii)
                {
                    // utf-8

                    error |= check_utf8_bytes(curr0, prev_input_block);
                    error |= check_utf8_bytes(curr1, curr0);
                    error |= check_utf8_bytes(curr2, curr1);
                    error |= check_utf8_bytes(curr3, curr2);

                    prev_incomplete = is_incomplete(curr3);
                    prev_input_block = curr3;
                }
                else
                {
                    // ascii

                    error |= prev_incomplete;
                }
                if(
# if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
                    !__builtin_ia32_ptestz128(::std::bit_cast<i64x2simd>(error), ::std::bit_cast<i64x2simd>(error))
# elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                    __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(error != static_cast<::std::uint8_t>(0u)))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                    !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~(error != static_cast<::std::uint8_t>(0u))))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                    __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(error != static_cast<::std::uint8_t>(0u)))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)                  // Only supported by clang
                    __builtin_neon_vmaxvq_u32(::std::bit_cast<u32x4simd>(error))
# elif defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)  // Only supported by GCC
                    __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<u32x4simd>(error))
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_bnz_v)
                    __builtin_lsx_bnz_v(::std::bit_cast<u8x16simd>(error))  /// @todo need check
# else
#  error "missing instructions"
# endif
                        ) [[unlikely]]
                {
                    // Jump out to scalar processing
                    break;
                }

                str_curr += sizeof(u8x16simd) * 4uz;

                // prefetch
                ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                                ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                                ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(
                    reinterpret_cast<::std::byte const*>(str_curr) + 64u);
            }

            // tail handling
            // If the previous SIMD block ended with an incomplete UTF-8 sequence,
            // rewind so that the scalar loop validates it together with the
            // remainder (simdutf-style).
            if(str_curr != str_begin)
            {
                auto const processed{static_cast<::std::size_t>(str_curr - str_begin)};
                unsigned rewind{};  // 0..3

                if(processed >= 3uz && static_cast<::std::uint8_t>(*(str_curr - 3u)) >= static_cast<::std::uint8_t>(0b1111'0000u)) { rewind = 3u; }
                else if(processed >= 2uz && static_cast<::std::uint8_t>(*(str_curr - 2u)) >= static_cast<::std::uint8_t>(0b1110'0000u)) { rewind = 2u; }
                else if(processed >= 1uz && static_cast<::std::uint8_t>(*(str_curr - 1u)) >= static_cast<::std::uint8_t>(0b1100'0000u)) { rewind = 1u; }

                str_curr -= rewind;
            }

            while(str_curr != str_end)
            {
                auto const str_curr_val{*str_curr};

                if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                {
                    if constexpr(zero_illegal)
                    {
                        if(str_curr_val == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                    }

                    ++str_curr;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{2u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    str_curr += utf8_length;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p2_val{*(str_curr + 2u)};

                    if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{3u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                    utf8_b2 &= 0x3Fu;
                    utf8_b2 <<= (utf8_length - 3u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                    }

                    str_curr += utf8_length;
                }
                else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p1_val{*(str_curr + 1u)};

                    if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p2_val{*(str_curr + 2u)};

                    if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    auto const str_curr_p3_val{*(str_curr + 3u)};

                    if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                    }

                    constexpr auto utf8_length{4u};

                    ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                    constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                    utf8_b0 &= ((1u << utf8_b0_shu1) - 1u);
                    utf8_b0 <<= (utf8_length - 1u) * 6u;

                    ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                    utf8_b1 &= 0x3Fu;
                    utf8_b1 <<= (utf8_length - 2u) * 6u;

                    ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                    utf8_b2 &= 0x3Fu;
                    utf8_b2 <<= (utf8_length - 3u) * 6u;

                    ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                    utf8_b3 &= 0x3Fu;
                    utf8_b3 <<= (utf8_length - 4u) * 6u;

                    char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                    constexpr auto test_overlong{utf8_length - 2u};
                    constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                    constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                           << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                    if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                    }

                    if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint}; }

                    str_curr += utf8_length;
                }
                else
                {
                    if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                    }
                    else
                    {
                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                    }
                }
            }

            return {str_curr, ::uwvm2::utils::utf::utf_error_code::success};

#elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && (defined(__SSE2__) || defined(__wasm_simd128__))

            // Algorithm author MacroModel

            using i64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int64_t;
            using u64x2simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint64_t;
            using u32x4simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint32_t;
            using c8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = char;
            using u8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::uint8_t;
            using i8x16simd [[__gnu__::__vector_size__(16)]] [[maybe_unused]] = ::std::int8_t;

            for(;;)
            {
                if(static_cast<::std::size_t>(str_end - str_curr) >= sizeof(u8x16simd)) [[likely]]
                {
                    u8x16simd need_check;

                    ::std::memcpy(::std::addressof(need_check), str_curr, sizeof(u8x16simd));

                    bool const check_has_non_ascii{static_cast<bool>(
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                        __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(need_check))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                        !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~need_check))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                        __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(need_check))
# else
#  error "missing instructions"
# endif
                            )};

                    if(!check_has_non_ascii)
                    {
                        // all ascii

                        if constexpr(zero_illegal)
                        {
                            constexpr auto u8_zero{static_cast<::std::uint8_t>(0u)};

                            auto const has_zero{need_check == u8_zero};

                            bool const check_has_zero{static_cast<bool>(
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
                                __builtin_ia32_pmovmskb128(::std::bit_cast<c8x16simd>(has_zero))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
                                !__builtin_wasm_all_true_i8x16(::std::bit_cast<i8x16simd>(~has_zero))
# elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
                                __builtin_wasm_bitmask_i8x16(::std::bit_cast<i8x16simd>(has_zero))
# else
#  error "missing instructions"
# endif
                                    )};

                            if(check_has_zero) [[unlikely]]
                            {
                                // Get the exact error address
                                for(auto const str_curr_end{str_curr + sizeof(u8x16simd)}; str_curr != str_curr_end; ++str_curr)
                                {
                                    if(*str_curr == u8'\0') { break; }
                                }

                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters};
                            }
                        }

                        str_curr += sizeof(u8x16simd);
                        continue;
                    }
                }

                if(str_curr == str_end) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::success}; }

                while(*str_curr < static_cast<char8_t>(0b1000'0000u))
                {
                    if constexpr(zero_illegal)
                    {
                        if(*str_curr == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                    }

                    if(++str_curr == str_end) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::success}; }
                }

                if(static_cast<::std::size_t>(str_end - str_curr) >= sizeof(::std::uint_least32_t)) [[likely]]
                {
                    ::std::uint_least32_t utf_8;

                    ::std::memcpy(::std::addressof(utf_8), str_curr, sizeof(::std::uint_least32_t));

                    if((utf_8 & static_cast<::std::uint_least32_t>(0b1110'0000u)) == static_cast<::std::uint_least32_t>(0b1100'0000u))
                    {
                        // Check if it is a utf_8 sequence
                        auto utf8_mask_C0C0C000{utf_8 & static_cast<::std::uint_least32_t>(0x0000'C000u)};

                        constexpr auto utf8_need_chean{16u};
                        constexpr auto utf8_length{2u};

                        constexpr auto checker{static_cast<::std::uint_least32_t>(0x0000'8000u)};

                        if(utf8_mask_C0C0C000 != checker) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

                        ::std::uint_least32_t utf8_clean{utf_8};
                        // Automatic optimization into `and`
                        utf8_clean <<= utf8_need_chean;
                        utf8_clean >>= utf8_need_chean;

                        ::std::uint_least32_t utf8_b0{utf8_clean};
                        constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                        utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                        utf8_b0 <<= (utf8_length - 1u) * 6u;

                        ::std::uint_least32_t utf8_b1{utf8_clean};
                        utf8_b1 >>= 8u;
                        utf8_b1 &= 0x3Fu;
                        utf8_b1 <<= (utf8_length - 2u) * 6u;

                        char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                        constexpr auto test_overlong{utf8_length - 2u};
                        constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                        constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                               << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                        if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                        }

                        str_curr += utf8_length;
                    }
                    else if((utf_8 & static_cast<::std::uint_least32_t>(0b1111'0000u)) == static_cast<::std::uint_least32_t>(0b1110'0000u))
                    {
                        // Check if it is a utf_8 sequence
                        auto utf8_mask_C0C0C000{utf_8 & static_cast<::std::uint_least32_t>(0x00C0'C000u)};

                        constexpr auto utf8_need_chean{8u};
                        constexpr auto utf8_length{3u};

                        constexpr auto checker{static_cast<::std::uint_least32_t>(0x0080'8000u)};

                        if(utf8_mask_C0C0C000 != checker) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

                        ::std::uint_least32_t utf8_clean{utf_8};
                        // Automatic optimization into `and`
                        utf8_clean <<= utf8_need_chean;
                        utf8_clean >>= utf8_need_chean;

                        ::std::uint_least32_t utf8_b0{utf8_clean};
                        constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                        utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                        utf8_b0 <<= (utf8_length - 1u) * 6u;

                        ::std::uint_least32_t utf8_b1{utf8_clean};
                        utf8_b1 >>= 8u;
                        utf8_b1 &= 0x3Fu;
                        utf8_b1 <<= (utf8_length - 2u) * 6u;

                        ::std::uint_least32_t utf8_b2{utf8_clean};
                        utf8_b2 >>= 16u;
                        utf8_b2 &= 0x3Fu;
                        utf8_b2 <<= (utf8_length - 3u) * 6u;

                        char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                        constexpr auto test_overlong{utf8_length - 2u};
                        constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                        constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                               << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                        if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                        }

                        if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                        }

                        str_curr += utf8_length;
                    }
                    else if((utf_8 & static_cast<::std::uint_least32_t>(0b1111'1000u)) == static_cast<::std::uint_least32_t>(0b1111'0000u))
                    {
                        // Check if it is a utf_8 sequence
                        auto utf8_mask_C0C0C000{utf_8 & static_cast<::std::uint_least32_t>(0xC0C0'C000u)};

                        constexpr auto utf8_length{4u};

                        constexpr auto checker{static_cast<::std::uint_least32_t>(0x8080'8000u)};

                        if(utf8_mask_C0C0C000 != checker) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

                        ::std::uint_least32_t utf8_clean{utf_8};  // utf8_need_chean == 0u

                        ::std::uint_least32_t utf8_b0{utf8_clean};
                        constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                        utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                        utf8_b0 <<= (utf8_length - 1u) * 6u;

                        ::std::uint_least32_t utf8_b1{utf8_clean};
                        utf8_b1 >>= 8u;
                        utf8_b1 &= 0x3Fu;
                        utf8_b1 <<= (utf8_length - 2u) * 6u;

                        ::std::uint_least32_t utf8_b2{utf8_clean};
                        utf8_b2 >>= 16u;
                        utf8_b2 &= 0x3Fu;
                        utf8_b2 <<= (utf8_length - 3u) * 6u;

                        ::std::uint_least32_t utf8_b3{utf8_clean};
                        utf8_b3 >>= 24u;
                        utf8_b3 &= 0x3Fu;
                        utf8_b3 <<= (utf8_length - 4u) * 6u;

                        char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                        constexpr auto test_overlong{utf8_length - 2u};
                        constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                        constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                               << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                        if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                        }

                        if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint};
                        }

                        str_curr += utf8_length;
                    }
                    else [[unlikely]]
                    {
                        if((utf_8 & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                        }
                        else
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                        }
                    }

                    // continue;
                }
                else
                {
                    // byte-by-byte processing

                    while(str_curr != str_end)
                    {
                        auto str_curr_val{*str_curr};

                        if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                        {
                            if constexpr(zero_illegal)
                            {
                                if(str_curr_val == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                            }

                            ++str_curr;
                        }
                        else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                        {
                            if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            auto const str_curr_p1_val{*(str_curr + 1u)};

                            if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            constexpr auto utf8_length{2u};

                            ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                            constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                            utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                            utf8_b0 <<= (utf8_length - 1u) * 6u;

                            ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                            utf8_b1 &= 0x3Fu;
                            utf8_b1 <<= (utf8_length - 2u) * 6u;

                            char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                            constexpr auto test_overlong{utf8_length - 2u};
                            constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                            constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                   << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                            if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                            }

                            str_curr += utf8_length;
                        }
                        else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                        {
                            if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            auto const str_curr_p1_val{*(str_curr + 1u)};

                            if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            auto const str_curr_p2_val{*(str_curr + 2u)};

                            if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            constexpr auto utf8_length{3u};

                            ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                            constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                            utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                            utf8_b0 <<= (utf8_length - 1u) * 6u;

                            ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                            utf8_b1 &= 0x3Fu;
                            utf8_b1 <<= (utf8_length - 2u) * 6u;

                            ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                            utf8_b2 &= 0x3Fu;
                            utf8_b2 <<= (utf8_length - 3u) * 6u;

                            char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                            constexpr auto test_overlong{utf8_length - 2u};
                            constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                            constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                   << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                            if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                            }

                            if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                            }

                            str_curr += utf8_length;
                        }
                        else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                        {
                            if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            auto const str_curr_p1_val{*(str_curr + 1u)};

                            if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            auto const str_curr_p2_val{*(str_curr + 2u)};

                            if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            auto const str_curr_p3_val{*(str_curr + 3u)};

                            if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                            }

                            constexpr auto utf8_length{4u};

                            ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                            constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                            utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                            utf8_b0 <<= (utf8_length - 1u) * 6u;

                            ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                            utf8_b1 &= 0x3Fu;
                            utf8_b1 <<= (utf8_length - 2u) * 6u;

                            ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                            utf8_b2 &= 0x3Fu;
                            utf8_b2 <<= (utf8_length - 3u) * 6u;

                            ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                            utf8_b3 &= 0x3Fu;
                            utf8_b3 <<= (utf8_length - 4u) * 6u;

                            char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                            constexpr auto test_overlong{utf8_length - 2u};
                            constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                            constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                   << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                            if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                            }

                            if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint};
                            }

                            str_curr += utf8_length;
                        }
                        else
                        {
                            if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                            }
                            else
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                            }
                        }
                    }

                    // jump out of a loop
                    break;
                }
            }

#else
            // Algorithm author MacroModel

            constexpr unsigned char_bit{static_cast<unsigned>(CHAR_BIT)};
            constexpr bool char_bit_is_8{char_bit == 8u};
            constexpr bool is_little_endian{::std::endian::native == ::std::endian::little};

            if constexpr(is_little_endian && char_bit_is_8 && ::std::same_as<::std::size_t, ::std::uint_least64_t> &&
                         static_cast<unsigned>(::std::numeric_limits<::std::uint_least64_t>::digits) == 64u)
            {
                // SWAR algorithm

                // Avoid using (u)intN_t in non-macro-qualified environments, as it may not be provided, use (u)int_leastN_t and determine its digits to
                // determine if it is the same type as the expected (u)intN_t

                // Since there is no external macro protection, it is not possible to use (U)INTN_MAX, which the standard is not guaranteed to provide.
                // However, on platforms where CHAR_BIT is equal to 8, LEAST must have the same size as the N value.

                for(;;)
                {
                    if(static_cast<::std::size_t>(str_end - str_curr) >= 2uz * sizeof(::std::uint_least64_t)) [[likely]]
                    {
                        ::std::uint_least64_t need_check1;

                        ::std::memcpy(::std::addressof(need_check1), str_curr, sizeof(::std::uint_least64_t));

                        ::std::uint_least64_t need_check2;

                        ::std::memcpy(::std::addressof(need_check2), str_curr + sizeof(::std::uint_least64_t), sizeof(::std::uint_least64_t));

                        ::std::uint_least64_t const check_has_non_ascii{(need_check1 | need_check2) &
                                                                        static_cast<::std::uint_least64_t>(0x8080'8080'8080'8080u)};

                        if(!check_has_non_ascii)
                        {
                            if constexpr(zero_illegal)
                            {
                                if(~((((need_check1 & static_cast<::std::uint_least64_t>(0x7F7F'7F7F'7F7F'7F7Fu)) + 0x7F7F'7F7F'7F7F'7F7Fu) | need_check1) |
                                     0x7F7F'7F7F'7F7F'7F7Fu)) [[unlikely]]
                                {
                                    // Get the exact error address
                                    for(auto const str_curr_end{str_curr + sizeof(::std::uint_least64_t)}; str_curr != str_curr_end; ++str_curr)
                                    {
                                        if(*str_curr == u8'\0') { break; }
                                    }

                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters};
                                }

                                if(~((((need_check2 & static_cast<::std::uint_least64_t>(0x7F7F'7F7F'7F7F'7F7Fu)) + 0x7F7F'7F7F'7F7F'7F7Fu) | need_check2) |
                                     0x7F7F'7F7F'7F7F'7F7Fu)) [[unlikely]]
                                {
                                    // Get the exact error address
                                    str_curr += sizeof(::std::uint_least64_t);

                                    for(auto const str_curr_end{str_curr + sizeof(::std::uint_least64_t)}; str_curr != str_curr_end; ++str_curr)
                                    {
                                        if(*str_curr == u8'\0') { break; }
                                    }

                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters};
                                }
                            }

                            str_curr += 2u * sizeof(::std::uint_least64_t);
                            continue;
                        }
                    }

                    if(str_curr == str_end) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::success}; }

                    while(*str_curr < static_cast<char8_t>(0b1000'0000u))
                    {
                        if constexpr(zero_illegal)
                        {
                            if(*str_curr == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                        }

                        if(++str_curr == str_end) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::success}; }
                    }

                    if(static_cast<::std::size_t>(str_end - str_curr) >= sizeof(::std::uint_least32_t)) [[likely]]
                    {
                        ::std::uint_least32_t utf_8;

                        ::std::memcpy(::std::addressof(utf_8), str_curr, sizeof(::std::uint_least32_t));

                        if((utf_8 & static_cast<::std::uint_least32_t>(0b1110'0000u)) == static_cast<::std::uint_least32_t>(0b1100'0000u))
                        {
                            // Check if it is a utf_8 sequence
                            auto utf8_mask_C0C0C000{utf_8 & static_cast<::std::uint_least32_t>(0x0000'C000u)};

                            constexpr auto utf8_need_chean{16u};
                            constexpr auto utf8_length{2u};

                            constexpr auto checker{static_cast<::std::uint_least32_t>(0x0000'8000u)};

                            if(utf8_mask_C0C0C000 != checker) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

                            ::std::uint_least32_t utf8_clean{utf_8};
                            // Automatic optimization into `and`
                            utf8_clean <<= utf8_need_chean;
                            utf8_clean >>= utf8_need_chean;

                            ::std::uint_least32_t utf8_b0{utf8_clean};
                            constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                            utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                            utf8_b0 <<= (utf8_length - 1u) * 6u;

                            ::std::uint_least32_t utf8_b1{utf8_clean};
                            utf8_b1 >>= 8u;
                            utf8_b1 &= 0x3Fu;
                            utf8_b1 <<= (utf8_length - 2u) * 6u;

                            char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                            constexpr auto test_overlong{utf8_length - 2u};
                            constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                            constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                   << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                            if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                            }

                            str_curr += utf8_length;
                        }
                        else if((utf_8 & static_cast<::std::uint_least32_t>(0b1111'0000u)) == static_cast<::std::uint_least32_t>(0b1110'0000u))
                        {
                            // Check if it is a utf_8 sequence
                            auto utf8_mask_C0C0C000{utf_8 & static_cast<::std::uint_least32_t>(0x00C0'C000u)};

                            constexpr auto utf8_need_chean{8u};
                            constexpr auto utf8_length{3u};

                            constexpr auto checker{static_cast<::std::uint_least32_t>(0x0080'8000u)};

                            if(utf8_mask_C0C0C000 != checker) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

                            ::std::uint_least32_t utf8_clean{utf_8};
                            // Automatic optimization into `and`
                            utf8_clean <<= utf8_need_chean;
                            utf8_clean >>= utf8_need_chean;

                            ::std::uint_least32_t utf8_b0{utf8_clean};
                            constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                            utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                            utf8_b0 <<= (utf8_length - 1u) * 6u;

                            ::std::uint_least32_t utf8_b1{utf8_clean};
                            utf8_b1 >>= 8u;
                            utf8_b1 &= 0x3Fu;
                            utf8_b1 <<= (utf8_length - 2u) * 6u;

                            ::std::uint_least32_t utf8_b2{utf8_clean};
                            utf8_b2 >>= 16u;
                            utf8_b2 &= 0x3Fu;
                            utf8_b2 <<= (utf8_length - 3u) * 6u;

                            char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                            constexpr auto test_overlong{utf8_length - 2u};
                            constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                            constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                   << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                            if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                            }

                            if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                            }

                            str_curr += utf8_length;
                        }
                        else if((utf_8 & static_cast<::std::uint_least32_t>(0b1111'1000u)) == static_cast<::std::uint_least32_t>(0b1111'0000u))
                        {
                            // Check if it is a utf_8 sequence
                            auto utf8_mask_C0C0C000{utf_8 & static_cast<::std::uint_least32_t>(0xC0C0'C000u)};

                            constexpr auto utf8_length{4u};

                            constexpr auto checker{static_cast<::std::uint_least32_t>(0x8080'8000u)};

                            if(utf8_mask_C0C0C000 != checker) [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence}; }

                            ::std::uint_least32_t utf8_clean{utf_8};  // utf8_need_chean == 0u

                            ::std::uint_least32_t utf8_b0{utf8_clean};
                            constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                            utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                            utf8_b0 <<= (utf8_length - 1u) * 6u;

                            ::std::uint_least32_t utf8_b1{utf8_clean};
                            utf8_b1 >>= 8u;
                            utf8_b1 &= 0x3Fu;
                            utf8_b1 <<= (utf8_length - 2u) * 6u;

                            ::std::uint_least32_t utf8_b2{utf8_clean};
                            utf8_b2 >>= 16u;
                            utf8_b2 &= 0x3Fu;
                            utf8_b2 <<= (utf8_length - 3u) * 6u;

                            ::std::uint_least32_t utf8_b3{utf8_clean};
                            utf8_b3 >>= 24u;
                            utf8_b3 &= 0x3Fu;
                            utf8_b3 <<= (utf8_length - 4u) * 6u;

                            char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                            constexpr auto test_overlong{utf8_length - 2u};
                            constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                            constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                   << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                            if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                            }

                            if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]]
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint};
                            }

                            str_curr += utf8_length;
                        }
                        else [[unlikely]]
                        {
                            if((utf_8 & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                            }
                            else
                            {
                                return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                            }
                        }

                        // continue;
                    }
                    else
                    {
                        // byte-by-byte processing

                        while(str_curr != str_end)
                        {
                            auto const str_curr_val{*str_curr};

                            if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                            {
                                if constexpr(zero_illegal)
                                {
                                    if(str_curr_val == u8'\0') [[unlikely]]
                                    {
                                        return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters};
                                    }
                                }

                                ++str_curr;
                            }
                            else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                            {
                                if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                auto const str_curr_p1_val{*(str_curr + 1u)};

                                if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                constexpr auto utf8_length{2u};

                                ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                                constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                                utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                                utf8_b0 <<= (utf8_length - 1u) * 6u;

                                ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                                utf8_b1 &= 0x3Fu;
                                utf8_b1 <<= (utf8_length - 2u) * 6u;

                                char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                                constexpr auto test_overlong{utf8_length - 2u};
                                constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                                constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                       << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                                if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                                }

                                str_curr += utf8_length;
                            }
                            else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                            {
                                if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                auto const str_curr_p1_val{*(str_curr + 1u)};

                                if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                auto const str_curr_p2_val{*(str_curr + 2u)};

                                if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                constexpr auto utf8_length{3u};

                                ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                                constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                                utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                                utf8_b0 <<= (utf8_length - 1u) * 6u;

                                ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                                utf8_b1 &= 0x3Fu;
                                utf8_b1 <<= (utf8_length - 2u) * 6u;

                                ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                                utf8_b2 &= 0x3Fu;
                                utf8_b2 <<= (utf8_length - 3u) * 6u;

                                char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                                constexpr auto test_overlong{utf8_length - 2u};
                                constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                                constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                       << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                                if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                                }

                                if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                                }

                                str_curr += utf8_length;
                            }
                            else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                            {
                                if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                auto const str_curr_p1_val{*(str_curr + 1u)};

                                if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                auto const str_curr_p2_val{*(str_curr + 2u)};

                                if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                auto const str_curr_p3_val{*(str_curr + 3u)};

                                if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                                }

                                constexpr auto utf8_length{4u};

                                ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                                constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                                utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                                utf8_b0 <<= (utf8_length - 1u) * 6u;

                                ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                                utf8_b1 &= 0x3Fu;
                                utf8_b1 <<= (utf8_length - 2u) * 6u;

                                ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                                utf8_b2 &= 0x3Fu;
                                utf8_b2 <<= (utf8_length - 3u) * 6u;

                                ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                                utf8_b3 &= 0x3Fu;
                                utf8_b3 <<= (utf8_length - 4u) * 6u;

                                char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                                constexpr auto test_overlong{utf8_length - 2u};
                                constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                                constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                                       << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                                if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                                }

                                if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]]
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint};
                                }

                                str_curr += utf8_length;
                            }
                            else
                            {
                                if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                                }
                                else
                                {
                                    return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                                }
                            }
                        }

                        // jump out of a loop
                        break;
                    }
                }
            }
            else
            {
                // byte-by-byte processing
                // Compatible with platforms where CHAR_BIT is greater than 8, because it is guaranteed that only a meaningful portion of each byte operation is
                // fetched

                while(str_curr != str_end)
                {
                    // [xx] xx xx ... (end)
                    // [  ] unsafe (could be the section_end)
                    //  ^^ section_curr

                    auto str_curr_val{*str_curr};

                    // A standards-compliant UTF-8 decoder must only care about the low 8 bits of each byte when verifying or decoding a byte sequence.

                    // Subsequent judgment is required for < static_cast<char8_t>(0b1000'0000u), so for platforms where CHAR_BIT is not equal to 8 the high bit
                    // needs to be cleared.

                    if constexpr(!char_bit_is_8) { str_curr_val &= 0xFFu; }

                    if(str_curr_val < static_cast<char8_t>(0b1000'0000u))
                    {
                        // ??0xxxxxxx

                        if constexpr(zero_illegal)
                        {
                            if(str_curr_val == u8'\0') [[unlikely]] { return {str_curr, ::uwvm2::utils::utf::utf_error_code::contains_empty_characters}; }
                        }

                        ++str_curr;
                    }
                    else if((str_curr_val & static_cast<char8_t>(0b1110'0000u)) == static_cast<char8_t>(0b1100'0000u))
                    {
                        // ??110xxxxx ??10xxxxxx

                        if(static_cast<::std::size_t>(str_end - str_curr) < 2uz) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        auto const str_curr_p1_val{*(str_curr + 1u)};

                        if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        constexpr auto utf8_length{2u};

                        ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                        constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                        utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                        utf8_b0 <<= (utf8_length - 1u) * 6u;

                        ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                        utf8_b1 &= 0x3Fu;
                        utf8_b1 <<= (utf8_length - 2u) * 6u;

                        char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1)};

                        constexpr auto test_overlong{utf8_length - 2u};
                        constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                        constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                               << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                        if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                        }

                        str_curr += utf8_length;
                    }
                    else if((str_curr_val & static_cast<char8_t>(0b1111'0000u)) == static_cast<char8_t>(0b1110'0000u))
                    {
                        // ??1110xxxx ??10xxxxxx ??10xxxxxx

                        if(static_cast<::std::size_t>(str_end - str_curr) < 3uz) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        auto const str_curr_p1_val{*(str_curr + 1u)};

                        if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        auto const str_curr_p2_val{*(str_curr + 2u)};

                        if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        constexpr auto utf8_length{3u};

                        ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                        constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                        utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                        utf8_b0 <<= (utf8_length - 1u) * 6u;

                        ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                        utf8_b1 &= 0x3Fu;
                        utf8_b1 <<= (utf8_length - 2u) * 6u;

                        ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                        utf8_b2 &= 0x3Fu;
                        utf8_b2 <<= (utf8_length - 3u) * 6u;

                        char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2)};

                        constexpr auto test_overlong{utf8_length - 2u};
                        constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                        constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                               << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                        if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                        }

                        if(static_cast<char32_t>(0xD7FFu) < utf8_c && utf8_c < static_cast<char32_t>(0xE000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::illegal_surrogate};
                        }

                        str_curr += utf8_length;
                    }
                    else if((str_curr_val & static_cast<char8_t>(0b1111'1000u)) == static_cast<char8_t>(0b1111'0000u))
                    {
                        // ??11110xxx ??10xxxxxx ??10xxxxxx ??10xxxxxx

                        if(static_cast<::std::size_t>(str_end - str_curr) < 4uz) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        auto const str_curr_p1_val{*(str_curr + 1u)};

                        if((str_curr_p1_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        auto const str_curr_p2_val{*(str_curr + 2u)};

                        if((str_curr_p2_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        auto const str_curr_p3_val{*(str_curr + 3u)};

                        if((str_curr_p3_val & static_cast<char8_t>(0b1100'0000u)) != static_cast<char8_t>(0b1000'0000u)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_short_sequence};
                        }

                        constexpr auto utf8_length{4u};

                        ::std::uint_least32_t utf8_b0{static_cast<::std::uint_least32_t>(str_curr_val)};
                        constexpr auto utf8_b0_shu1{8u - (utf8_length + 1u)};
                        utf8_b0 &= ((static_cast<::std::uint_least32_t>(1u) << utf8_b0_shu1) - 1u);
                        utf8_b0 <<= (utf8_length - 1u) * 6u;

                        ::std::uint_least32_t utf8_b1{static_cast<::std::uint_least32_t>(str_curr_p1_val)};
                        utf8_b1 &= 0x3Fu;
                        utf8_b1 <<= (utf8_length - 2u) * 6u;

                        ::std::uint_least32_t utf8_b2{static_cast<::std::uint_least32_t>(str_curr_p2_val)};
                        utf8_b2 &= 0x3Fu;
                        utf8_b2 <<= (utf8_length - 3u) * 6u;

                        ::std::uint_least32_t utf8_b3{static_cast<::std::uint_least32_t>(str_curr_p3_val)};
                        utf8_b3 &= 0x3Fu;
                        utf8_b3 <<= (utf8_length - 4u) * 6u;

                        char32_t const utf8_c{static_cast<char32_t>(utf8_b0 | utf8_b1 | utf8_b2 | utf8_b3)};

                        constexpr auto test_overlong{utf8_length - 2u};
                        constexpr auto test_overlong_pc{(test_overlong + 1u) >> 1u};

                        constexpr auto test_overlong_low_bound{static_cast<::std::uint_least32_t>(1u)
                                                               << (6u * test_overlong + (8u - utf8_length + (1u - test_overlong_pc)))};
                        if(utf8_c < static_cast<char32_t>(test_overlong_low_bound)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::overlong_encoding};
                        }

                        if(utf8_c > static_cast<char32_t>(0x10FFFFu)) [[unlikely]]
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::excessive_codepoint};
                        }

                        str_curr += utf8_length;
                    }
                    else
                    {
                        if((str_curr_val & static_cast<::std::uint_least32_t>(0b1100'0000u)) == static_cast<::std::uint_least32_t>(0b1000'0000u))
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::too_long_sequence};
                        }
                        else
                        {
                            return {str_curr, ::uwvm2::utils::utf::utf_error_code::long_header_bits};
                        }
                    }
                }
            }
#endif

            return {str_curr, ::uwvm2::utils::utf::utf_error_code::success};
        }
    }

    template <utf8_specification spec>
        requires (static_cast<unsigned>(spec) <= static_cast<unsigned>(utf8_specification::utf8_rfc3629_and_zero_illegal))
    inline constexpr ::uwvm2::utils::utf::u8result check_legal_utf8_unchecked(char8_t const* const str_begin, char8_t const* const str_end) noexcept
    {
        if constexpr(spec == utf8_specification::utf8_rfc3629) { return check_legal_utf8_rfc3629_unchecked<false>(str_begin, str_end); }
        else if constexpr(spec == utf8_specification::utf8_rfc3629_and_zero_illegal) { return check_legal_utf8_rfc3629_unchecked<true>(str_begin, str_end); }
    }

    template <utf8_specification spec>
        requires (static_cast<unsigned>(spec) <= static_cast<unsigned>(utf8_specification::utf8_rfc3629_and_zero_illegal))
    inline constexpr ::uwvm2::utils::utf::u8result check_legal_utf8(char8_t const* const str_begin, char8_t const* const str_end) noexcept
    {
        if(str_begin > str_end) [[unlikely]] { ::fast_io::fast_terminate(); }
        return check_legal_utf8_unchecked<spec>(str_begin, str_end);
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
