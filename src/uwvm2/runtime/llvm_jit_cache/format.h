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
# include <array>
# include <bit>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    enum class cache_status : unsigned
    {
        ok,
        disabled,
        io_error,
        malformed,
        invalid_magic,
        unsupported_version,
        isa_mismatch,
        context_mismatch,
        signature_missing,
        signature_mismatch,
        unsupported_signature,
        unsupported_compression,
        decompression_failed,
        size_limit_exceeded
    };

    enum class compression_kind : ::std::uint_least32_t
    {
        none = 0u,
        uwvm_lzss = 1u,
        uwvm_native_lz = 2u
    };

    enum class signature_kind : ::std::uint_least32_t
    {
        none = 0u,
        ed25519_identity = 1u
    };

    inline constexpr ::std::size_t cache_ed25519_seed_size{32uz};

    struct cache_policy
    {
        bool enable{true};
        bool generate_signature{true};
        bool verify_signature{true};
        compression_kind compression{compression_kind::uwvm_native_lz};
        ::std::size_t max_object_bytes{512uz * 1024uz * 1024uz};
    };

    struct cache_context
    {
        ::uwvm2::utils::container::u8string cache_dir{};
        ::uwvm2::utils::container::u8string cache_key{};
        ::uwvm2::utils::container::u8string target_triple{};
        ::uwvm2::utils::container::u8string cpu_name{};
        ::uwvm2::utils::container::u8string cpu_features{};
        ::uwvm2::utils::container::u8string llvm_version{};
        ::uwvm2::utils::container::u8string uwvm_abi{};
        ::uwvm2::utils::container::u8string codegen_policy{};
        ::uwvm2::utils::container::array<::std::byte, cache_ed25519_seed_size> signature_seed{};
        bool has_signature_seed{};
    };

    struct cache_load_result
    {
        cache_status status{cache_status::disabled};
        ::uwvm2::utils::container::vector<::std::byte> object{};
        bool signature_verified{};
        bool isa_matched{};
    };

    struct cache_fixed_header
    {
        ::std::byte magic[8]{};
        ::std::uint_least32_t version{};
        ::std::uint_least32_t fixed_header_size{};
        ::std::uint_least32_t compression{};
        ::std::uint_least32_t signature{};
        ::std::uint_least64_t uncompressed_size{};
        ::std::uint_least64_t payload_size{};
        ::std::uint_least64_t isa_metadata_size{};
        ::std::uint_least64_t context_metadata_size{};
        ::std::uint_least64_t signature_size{};
    };

    struct cache_blob_view
    {
        cache_fixed_header header{};
        ::std::byte const* isa_metadata{};
        ::std::byte const* context_metadata{};
        ::std::byte const* signature{};
        ::std::byte const* payload{};
    };

    inline constexpr ::std::byte cache_magic[8]{::std::byte{'U'},
                                                ::std::byte{'W'},
                                                ::std::byte{'V'},
                                                ::std::byte{'M'},
                                                ::std::byte{'L'},
                                                ::std::byte{'J'},
                                                ::std::byte{'C'},
                                                ::std::byte{0x01u}};
    inline constexpr ::std::uint_least32_t cache_format_version{2u};
    inline constexpr ::std::size_t cache_fixed_header_size{64uz};
    inline constexpr ::std::size_t cache_sha256_digest_size{32uz};
    inline constexpr ::std::size_t cache_ed25519_signature_size{64uz};
    inline constexpr bool cache_ed25519_identity_signature_available{true};

    namespace details
    {
        inline constexpr void append_bytes(::uwvm2::utils::container::vector<::std::byte>& out, ::std::byte const* first, ::std::byte const* last) noexcept
        {
            for(; first != last; ++first) { out.push_back(*first); }
        }

        inline constexpr void append_u32_le(::uwvm2::utils::container::vector<::std::byte>& out, ::std::uint_least32_t v) noexcept
        {
            static_assert(sizeof(::std::uint_least32_t) >= 4uz);
            auto const le{::fast_io::little_endian(v)};
            auto const first{reinterpret_cast<::std::byte const*>(::std::addressof(le))};
            append_bytes(out, first, first + 4uz);
        }

        inline constexpr void append_u64_le(::uwvm2::utils::container::vector<::std::byte>& out, ::std::uint_least64_t v) noexcept
        {
            static_assert(sizeof(::std::uint_least64_t) >= 8uz);
            auto const le{::fast_io::little_endian(v)};
            auto const first{reinterpret_cast<::std::byte const*>(::std::addressof(le))};
            append_bytes(out, first, first + 8uz);
        }

        [[nodiscard]] inline constexpr bool read_u32_le(::std::byte const*& first, ::std::byte const* last, ::std::uint_least32_t& out) noexcept
        {
            static_assert(sizeof(::std::uint_least32_t) >= 4uz);
            if(static_cast<::std::size_t>(last - first) < 4uz) [[unlikely]] { return false; }
            ::std::uint_least32_t v{};
            ::std::memcpy(::std::addressof(v), first, 4uz);
            first += 4uz;
            out = ::fast_io::little_endian(v);
            return true;
        }

        [[nodiscard]] inline constexpr bool read_u64_le(::std::byte const*& first, ::std::byte const* last, ::std::uint_least64_t& out) noexcept
        {
            static_assert(sizeof(::std::uint_least64_t) >= 8uz);
            if(static_cast<::std::size_t>(last - first) < 8uz) [[unlikely]] { return false; }
            ::std::uint_least64_t v{};
            ::std::memcpy(::std::addressof(v), first, 8uz);
            first += 8uz;
            out = ::fast_io::little_endian(v);
            return true;
        }

        inline constexpr void append_u8string_bytes(::uwvm2::utils::container::vector<::std::byte>& out,
                                                    ::uwvm2::utils::container::u8string_view sv) noexcept
        {
            auto const first{reinterpret_cast<::std::byte const*>(sv.data())};
            append_bytes(out, first, first + sv.size());
        }

        inline constexpr void append_u8string_bytes(::uwvm2::utils::container::vector<::std::byte>& out,
                                                    ::uwvm2::utils::container::u8string const& s) noexcept
        {
            auto const first{reinterpret_cast<::std::byte const*>(s.data())};
            append_bytes(out, first, first + s.size());
        }

        inline constexpr void append_key_value(::uwvm2::utils::container::vector<::std::byte>& out,
                                               ::uwvm2::utils::container::u8string_view key,
                                               ::uwvm2::utils::container::u8string_view value) noexcept
        {
            append_u8string_bytes(out, key);
            out.push_back(static_cast<::std::byte>('='));
            append_u8string_bytes(out, value);
            out.push_back(static_cast<::std::byte>('\n'));
        }

        inline constexpr void append_key_value(::uwvm2::utils::container::vector<::std::byte>& out,
                                               ::uwvm2::utils::container::u8string_view key,
                                               ::uwvm2::utils::container::u8string const& value) noexcept
        {
            append_u8string_bytes(out, key);
            out.push_back(static_cast<::std::byte>('='));
            append_u8string_bytes(out, value);
            out.push_back(static_cast<::std::byte>('\n'));
        }

        [[nodiscard]] inline constexpr bool checked_add(::std::size_t& v, ::std::uint_least64_t add) noexcept
        {
            if(add > static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)())) [[unlikely]] { return false; }
            auto const add_sz{static_cast<::std::size_t>(add)};
            if(v > (::std::numeric_limits<::std::size_t>::max)() - add_sz) [[unlikely]] { return false; }
            v += add_sz;
            return true;
        }
    }  // namespace details

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> make_isa_metadata(cache_context const& ctx) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        details::append_key_value(out, u8"target", ctx.target_triple);
        details::append_key_value(out, u8"cpu", ctx.cpu_name);
        details::append_key_value(out, u8"features", ctx.cpu_features);
        return out;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> make_context_metadata(cache_context const& ctx) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        details::append_key_value(out, u8"key", ctx.cache_key);
        details::append_key_value(out, u8"llvm", ctx.llvm_version);
        details::append_key_value(out, u8"uwvm_abi", ctx.uwvm_abi);
        details::append_key_value(out, u8"codegen", ctx.codegen_policy);
        return out;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte>
        serialize_fixed_header(cache_fixed_header const& header) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        out.reserve(cache_fixed_header_size);
        details::append_bytes(out, header.magic, header.magic + 8uz);
        details::append_u32_le(out, header.version);
        details::append_u32_le(out, header.fixed_header_size);
        details::append_u32_le(out, header.compression);
        details::append_u32_le(out, header.signature);
        details::append_u64_le(out, header.uncompressed_size);
        details::append_u64_le(out, header.payload_size);
        details::append_u64_le(out, header.isa_metadata_size);
        details::append_u64_le(out, header.context_metadata_size);
        details::append_u64_le(out, header.signature_size);
        return out;
    }

    [[nodiscard]] inline constexpr bool parse_fixed_header(::std::byte const* first, ::std::byte const* last, cache_fixed_header& header) noexcept
    {
        if(static_cast<::std::size_t>(last - first) < cache_fixed_header_size) [[unlikely]] { return false; }
        for(::std::size_t i{}; i != 8uz; ++i) { header.magic[i] = first[i]; }
        first += 8uz;
        return details::read_u32_le(first, last, header.version) && details::read_u32_le(first, last, header.fixed_header_size) &&
               details::read_u32_le(first, last, header.compression) && details::read_u32_le(first, last, header.signature) &&
               details::read_u64_le(first, last, header.uncompressed_size) && details::read_u64_le(first, last, header.payload_size) &&
               details::read_u64_le(first, last, header.isa_metadata_size) && details::read_u64_le(first, last, header.context_metadata_size) &&
               details::read_u64_le(first, last, header.signature_size);
    }

    [[nodiscard]] inline constexpr cache_status parse_cache_blob(::std::byte const* first,
                                                                 ::std::byte const* last,
                                                                 cache_blob_view& view) noexcept
    {
        if(!parse_fixed_header(first, last, view.header)) [[unlikely]] { return cache_status::malformed; }
        if(!::std::equal(view.header.magic, view.header.magic + 8uz, cache_magic)) [[unlikely]] { return cache_status::invalid_magic; }
        if(view.header.version != cache_format_version || view.header.fixed_header_size != cache_fixed_header_size) [[unlikely]]
        {
            return cache_status::unsupported_version;
        }

        ::std::size_t total{cache_fixed_header_size};
        if(!details::checked_add(total, view.header.isa_metadata_size) || !details::checked_add(total, view.header.context_metadata_size) ||
           !details::checked_add(total, view.header.signature_size) || !details::checked_add(total, view.header.payload_size)) [[unlikely]]
        {
            return cache_status::malformed;
        }

        if(static_cast<::std::size_t>(last - first) != total) [[unlikely]] { return cache_status::malformed; }

        auto cursor{first + cache_fixed_header_size};
        view.isa_metadata = cursor;
        cursor += static_cast<::std::size_t>(view.header.isa_metadata_size);
        view.context_metadata = cursor;
        cursor += static_cast<::std::size_t>(view.header.context_metadata_size);
        view.signature = cursor;
        cursor += static_cast<::std::size_t>(view.header.signature_size);
        view.payload = cursor;
        return cache_status::ok;
    }

    [[nodiscard]] inline constexpr bool metadata_equal(::std::byte const* first,
                                                       ::std::uint_least64_t n,
                                                       ::uwvm2::utils::container::vector<::std::byte> const& expected) noexcept
    {
        if(n != expected.size()) { return false; }
        return ::std::equal(first, first + static_cast<::std::size_t>(n), expected.begin());
    }
}  // namespace uwvm2::runtime::llvm_jit_cache

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
