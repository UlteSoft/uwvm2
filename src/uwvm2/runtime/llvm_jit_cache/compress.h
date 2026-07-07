/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-14
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
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <uwvm2/utils/container/impl.h>
# include "format.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    namespace details
    {
        // The LZSS shape is intentionally small and deterministic because cache compression should be cheap during JIT compilation.
        inline constexpr ::std::size_t lzss_window_size{4096uz};
        inline constexpr ::std::size_t lzss_min_match{3uz};
        inline constexpr ::std::size_t lzss_max_match{18uz};
        inline constexpr ::std::size_t lzss_hash_size{65536uz};
        // Native-LZ uses a wider offset and LZ4-like sequence layout for better object-file compression without an external dependency.
        inline constexpr ::std::size_t native_lz_hash_size{65536uz};
        inline constexpr ::std::size_t native_lz_min_match{4uz};
        inline constexpr ::std::size_t native_lz_max_offset{65535uz};

        [[nodiscard]] inline constexpr ::std::uint_least16_t lzss_hash3(::std::byte const* ptr) noexcept
        {
            // A 3-byte hash is enough for the minimum match and keeps the compressor single-pass.
            auto const b0{::std::to_integer<::std::uint_least32_t>(ptr[0])};
            auto const b1{::std::to_integer<::std::uint_least32_t>(ptr[1])};
            auto const b2{::std::to_integer<::std::uint_least32_t>(ptr[2])};
            auto const v{(b0 * 251u) ^ (b1 * 911u) ^ (b2 * 3571u)};
            return static_cast<::std::uint_least16_t>((v ^ (v >> 16u)) & 0xffffu);
        }

        inline constexpr void append_lzss_token(::uwvm2::utils::container::vector<::std::byte>& out, ::std::size_t offset, ::std::size_t length) noexcept
        {
            // The 16-bit token stores a 12-bit backward distance and a 4-bit length delta to cap decoder work.
            auto const token{static_cast<::std::uint_least16_t>(((length - lzss_min_match) << 12uz) | (offset - 1uz))};
            out.push_back(static_cast<::std::byte>(token & 0xffu));
            out.push_back(static_cast<::std::byte>((token >> 8u) & 0xffu));
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t native_lz_read_u32(::std::byte const* ptr) noexcept
        {
            ::std::uint32_t value{};
            ::std::memcpy(::std::addressof(value), ptr, sizeof(value));
            return ::fast_io::little_endian(value);
        }

        [[nodiscard]] inline constexpr ::std::uint_least16_t native_lz_hash4(::std::byte const* ptr) noexcept
        {
            auto const v{native_lz_read_u32(ptr)};
            // Knuth-style multiplicative hashing spreads common instruction/object patterns across the table.
            return static_cast<::std::uint_least16_t>((v * 2654435761u) >> 16u);
        }

        inline constexpr void append_native_lz_length(::uwvm2::utils::container::vector<::std::byte>& out, ::std::size_t len) noexcept
        {
            // Chained 255-byte extensions mirror LZ4, making long literals/matches compact without fixed-size fields.
            while(len >= 255uz)
            {
                out.push_back(static_cast<::std::byte>(255u));
                len -= 255uz;
            }
            out.push_back(static_cast<::std::byte>(len));
        }

        inline constexpr void append_native_lz_sequence(::uwvm2::utils::container::vector<::std::byte>& out,
                                                        ::std::byte const* literal_first,
                                                        ::std::size_t literal_size,
                                                        ::std::size_t offset,
                                                        ::std::size_t match_size,
                                                        bool has_match) noexcept
        {
            auto const literal_token{::std::min(literal_size, 15uz)};
            auto const match_token{has_match ? ::std::min(match_size - native_lz_min_match, 15uz) : 0uz};
            // A single token records the short literal and match lengths so common sequences cost one control byte.
            out.push_back(static_cast<::std::byte>((literal_token << 4uz) | match_token));

            if(literal_size >= 15uz) { append_native_lz_length(out, literal_size - 15uz); }
            append_bytes(out, literal_first, literal_first + literal_size);

            if(has_match)
            {
                // Offsets are little-endian because the cache format must be independent of host alignment and endian.
                out.push_back(static_cast<::std::byte>(offset & 0xffuz));
                out.push_back(static_cast<::std::byte>((offset >> 8uz) & 0xffuz));
                auto const encoded_match_size{match_size - native_lz_min_match};
                if(encoded_match_size >= 15uz) { append_native_lz_length(out, encoded_match_size - 15uz); }
            }
        }

        [[nodiscard]] inline constexpr bool read_native_lz_length(::std::byte const*& first, ::std::byte const* last, ::std::size_t& len) noexcept
        {
            for(;;)
            {
                if(first == last) [[unlikely]] { return false; }
                auto const v{::std::to_integer<::std::size_t>(*first++)};
                // Length fields are attacker-controlled on load, so overflow turns into a decode failure.
                if(len > (::std::numeric_limits<::std::size_t>::max)() - v) [[unlikely]] { return false; }
                len += v;
                if(v != 255uz) { return true; }
            }
        }
    }  // namespace details

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> compress_lzss(::std::byte const* first, ::std::size_t size) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        if(size == 0uz) { return out; }

        // Reserve a slight expansion budget because incompressible data may still emit flags and literals.
        out.reserve(size + (size / 8uz) + 16uz);

        constexpr auto npos{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::utils::container::vector<::std::size_t> table{};
        table.resize(details::lzss_hash_size);
        ::std::fill(table.begin(), table.end(), npos);

        ::std::size_t pos{};
        while(pos != size)
        {
            auto const flag_pos{out.size()};
            out.push_back(::std::byte{});

            ::std::uint_least8_t flags{};
            for(unsigned bit{}; bit != 8u && pos != size; ++bit)
            {
                ::std::size_t best_len{};
                ::std::size_t best_offset{};

                if(pos + details::lzss_min_match <= size)
                {
                    auto const h{details::lzss_hash3(first + pos)};
                    auto const prev{table.index_unchecked(h)};
                    // Keeping only the most recent position bounds memory and favors small offsets that decode cheaply.
                    table.index_unchecked(h) = pos;

                    if(prev != npos && prev < pos)
                    {
                        auto const offset{pos - prev};
                        if(offset <= details::lzss_window_size)
                        {
                            auto const limit{::std::min(details::lzss_max_match, size - pos)};
                            ::std::size_t len{};
                            while(len != limit && first[prev + len] == first[pos + len]) { ++len; }
                            if(len >= details::lzss_min_match)
                            {
                                best_len = len;
                                best_offset = offset;
                            }
                        }
                    }
                }

                if(best_len)
                {
                    flags = static_cast<::std::uint_least8_t>(flags | (1u << bit));
                    details::append_lzss_token(out, best_offset, best_len);

                    // Hash positions inside the match so the next search remains useful after the jump.
                    for(::std::size_t k{1uz}; k != best_len && pos + k + details::lzss_min_match <= size; ++k)
                    {
                        table.index_unchecked(details::lzss_hash3(first + pos + k)) = pos + k;
                    }
                    pos += best_len;
                }
                else
                {
                    out.push_back(first[pos]);
                    ++pos;
                }
            }

            out.index_unchecked(flag_pos) = static_cast<::std::byte>(flags);
        }

        return out;
    }

    [[nodiscard]] inline constexpr bool decompress_lzss(::std::byte const* first,
                                                        ::std::byte const* last,
                                                        ::std::size_t expected_size,
                                                        ::uwvm2::utils::container::vector<::std::byte>& out) noexcept
    {
        out = {};
        out.reserve(expected_size);

        // The loop stops at the declared uncompressed size so truncated or overlong payloads are rejected exactly.
        while(out.size() != expected_size)
        {
            if(first == last) [[unlikely]] { return false; }
            auto const flags{::std::to_integer<::std::uint_least8_t>(*first++)};

            for(unsigned bit{}; bit != 8u && out.size() != expected_size; ++bit)
            {
                if((flags & (1u << bit)) != 0u)
                {
                    if(static_cast<::std::size_t>(last - first) < 2uz) [[unlikely]] { return false; }
                    auto const lo{::std::to_integer<::std::uint_least16_t>(first[0])};
                    auto const hi{::std::to_integer<::std::uint_least16_t>(first[1])};
                    first += 2uz;

                    auto const token{static_cast<::std::uint_least16_t>(lo | (hi << 8u))};
                    auto const length{static_cast<::std::size_t>((token >> 12u) + details::lzss_min_match)};
                    auto const offset{static_cast<::std::size_t>((token & 0x0fffu) + 1u)};
                    // Back references must point into already-produced bytes to avoid reading uninitialized output.
                    if(offset == 0uz || offset > out.size()) [[unlikely]] { return false; }
                    if(length > expected_size - out.size()) [[unlikely]] { return false; }

                    for(::std::size_t i{}; i != length; ++i) { out.push_back(out.index_unchecked(out.size() - offset)); }
                }
                else
                {
                    if(first == last) [[unlikely]] { return false; }
                    out.push_back(*first++);
                }
            }
        }

        return first == last;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> compress_native_lz(::std::byte const* first, ::std::size_t size) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        if(size == 0uz) { return out; }

        // Native-LZ is optimized for fast cache writes: one hash lookup per position and no entropy coding.
        out.reserve(size + (size / 255uz) + 16uz);

        constexpr auto npos{(::std::numeric_limits<::std::size_t>::max)()};
        ::uwvm2::utils::container::vector<::std::size_t> table{};
        table.resize(details::native_lz_hash_size);
        ::std::fill(table.begin(), table.end(), npos);

        ::std::size_t anchor{};
        ::std::size_t pos{};
        while(pos + details::native_lz_min_match <= size)
        {
            auto const h{details::native_lz_hash4(first + pos)};
            auto const prev{table.index_unchecked(h)};
            table.index_unchecked(h) = pos;

            if(prev != npos && prev < pos && pos - prev <= details::native_lz_max_offset &&
               details::native_lz_read_u32(first + prev) == details::native_lz_read_u32(first + pos))
            {
                ::std::size_t match_size{details::native_lz_min_match};
                while(pos + match_size != size && first[prev + match_size] == first[pos + match_size]) { ++match_size; }

                // The anchor separates pending literals from the match so the decoder can replay the same byte stream.
                details::append_native_lz_sequence(out, first + anchor, pos - anchor, pos - prev, match_size, true);

                auto const match_end{pos + match_size};
                // Populate skipped positions to keep later matches available after consuming a long run.
                for(auto p{pos + 1uz}; p + details::native_lz_min_match <= match_end; ++p) { table.index_unchecked(details::native_lz_hash4(first + p)) = p; }

                pos = match_end;
                anchor = pos;
            }
            else
            {
                ++pos;
            }
        }

        if(anchor != size) { details::append_native_lz_sequence(out, first + anchor, size - anchor, 0uz, 0uz, false); }
        return out;
    }

    [[nodiscard]] inline constexpr bool decompress_native_lz(::std::byte const* first,
                                                             ::std::byte const* last,
                                                             ::std::size_t expected_size,
                                                             ::uwvm2::utils::container::vector<::std::byte>& out) noexcept
    {
        out = {};
        out.reserve(expected_size);

        // Decoding validates every length before copying so malformed cache files cannot overrun the output budget.
        while(out.size() != expected_size)
        {
            if(first == last) [[unlikely]] { return false; }
            auto const token{::std::to_integer<::std::size_t>(*first++)};

            auto literal_size{token >> 4uz};
            if(literal_size == 15uz && !details::read_native_lz_length(first, last, literal_size)) [[unlikely]] { return false; }

            if(static_cast<::std::size_t>(last - first) < literal_size) [[unlikely]] { return false; }
            if(literal_size > expected_size - out.size()) [[unlikely]] { return false; }
            details::append_bytes(out, first, first + literal_size);
            first += literal_size;

            // A literal-only terminal sequence is valid only if it consumes the entire compressed payload.
            if(out.size() == expected_size) { return first == last; }

            if(static_cast<::std::size_t>(last - first) < 2uz) [[unlikely]] { return false; }
            auto const offset{static_cast<::std::size_t>(::std::to_integer<unsigned>(first[0])) |
                              (static_cast<::std::size_t>(::std::to_integer<unsigned>(first[1])) << 8uz)};
            first += 2uz;
            // Matches copy from the existing output buffer, so offset zero and forward references are invalid.
            if(offset == 0uz || offset > out.size()) [[unlikely]] { return false; }

            auto match_size{details::native_lz_min_match + (token & 0x0fuz)};
            if((token & 0x0fuz) == 15uz && !details::read_native_lz_length(first, last, match_size)) [[unlikely]] { return false; }
            if(match_size > expected_size - out.size()) [[unlikely]] { return false; }

            for(::std::size_t i{}; i != match_size; ++i) { out.push_back(out.index_unchecked(out.size() - offset)); }
        }

        return first == last;
    }

    [[nodiscard]] inline constexpr bool decompress_payload(compression_kind kind,
                                                           ::std::byte const* first,
                                                           ::std::size_t compressed_size,
                                                           ::std::size_t expected_size,
                                                           ::uwvm2::utils::container::vector<::std::byte>& out) noexcept
    {
        switch(kind)
        {
            case compression_kind::none:
                // Uncompressed payloads still verify their size so the caller never receives trailing bytes as object data.
                if(compressed_size != expected_size) [[unlikely]] { return false; }
                out = {};
                out.reserve(expected_size);
                details::append_bytes(out, first, first + compressed_size);
                return true;
            case compression_kind::uwvm_lzss: return decompress_lzss(first, first + compressed_size, expected_size, out);
            case compression_kind::uwvm_native_lz: return decompress_native_lz(first, first + compressed_size, expected_size, out);
            default: return false;
        }
    }
}  // namespace uwvm2::runtime::llvm_jit_cache

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
