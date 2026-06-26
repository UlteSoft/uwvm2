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
    // The cache reports precise failure reasons because most misses are intentional compatibility rejections, not fatal runtime errors.
    enum class cache_status : unsigned
    {
        ok,                       // The on-disk object is accepted and ready for execution.
        disabled,                 // The caller explicitly disabled the cache, so no filesystem work should be attempted.
        io_error,                 // File operations failed; the JIT can still compile from source.
        malformed,                // The blob violates structural invariants and must be treated as untrusted input.
        invalid_magic,            // The file is not an UWVM LLVM JIT cache object.
        unsupported_version,      // The format contract changed, so older/newer readers must fail closed.
        isa_mismatch,             // Native code cannot be reused across incompatible target triples or CPU features.
        context_mismatch,         // ABI, LLVM, or codegen policy differs from the environment that produced the blob.
        signature_missing,        // Verification is enabled, but the blob was produced without an identity signature.
        signature_mismatch,       // The signed bytes no longer match the deterministic cache identity.
        unsupported_signature,    // The signature kind is unknown or unavailable in this build.
        unsupported_compression,  // The payload codec is unknown to this reader.
        decompression_failed,     // The compressed payload is invalid after all metadata checks passed.
        size_limit_exceeded       // The object is larger than the caller's configured safety limit.
    };

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view cache_status_name(cache_status status) noexcept
    {
        switch(status)
        {
            case cache_status::ok: return u8"ok";
            case cache_status::disabled: return u8"disabled";
            case cache_status::io_error: return u8"io-error";
            case cache_status::malformed: return u8"malformed";
            case cache_status::invalid_magic: return u8"invalid-magic";
            case cache_status::unsupported_version: return u8"unsupported-version";
            case cache_status::isa_mismatch: return u8"isa-mismatch";
            case cache_status::context_mismatch: return u8"context-mismatch";
            case cache_status::signature_missing: return u8"signature-missing";
            case cache_status::signature_mismatch: return u8"signature-mismatch";
            case cache_status::unsupported_signature: return u8"unsupported-signature";
            case cache_status::unsupported_compression: return u8"unsupported-compression";
            case cache_status::decompression_failed: return u8"decompression-failed";
            case cache_status::size_limit_exceeded: return u8"size-limit-exceeded";
        }
        return u8"unknown";
    }

    // The codec id is stored in the fixed header so readers can reject unknown encodings before touching the payload.
    enum class compression_kind : ::std::uint_least32_t
    {
        none = 0u,
        uwvm_lzss = 1u,
        uwvm_native_lz = 2u
    };

    // Signatures are versioned independently from the file format so trust policy can evolve without changing layout.
    enum class signature_kind : ::std::uint_least32_t
    {
        none = 0u,
        ed25519_identity = 1u
    };

    inline constexpr ::std::size_t cache_ed25519_seed_size{32uz};

    struct cache_policy
    {
        bool enable{true};                                               // A single switch keeps all cache I/O opt-in at call sites.
        bool generate_signature{true};                                   // Writers sign by default so later runs can detect tampering.
        bool verify_signature{true};                                     // Readers verify by default because cached code is executable native code.
        compression_kind compression{compression_kind::uwvm_native_lz};  // Native-LZ is the default balance for object-file-like byte streams.
        ::std::size_t max_object_bytes{512uz * 1024uz * 1024uz};         // The limit bounds memory use before allocation or decompression.
    };

    struct cache_context
    {
        ::uwvm2::utils::container::u8string cache_dir{};       // The root remains explicit so embedders can isolate cache state per run.
        ::uwvm2::utils::container::u8string cache_key{};       // The semantic key names the source/module identity before ISA hardening is added.
        ::uwvm2::utils::container::u8string target_triple{};   // The target triple is part of the ABI of generated machine code.
        ::uwvm2::utils::container::u8string cpu_name{};        // CPU selection can change instruction scheduling and enabled code paths.
        ::uwvm2::utils::container::u8string cpu_features{};    // Feature bits prevent AVX/SIMD or similar code from crossing host boundaries.
        ::uwvm2::utils::container::u8string llvm_version{};    // LLVM upgrades can change object emission even for identical IR.
        ::uwvm2::utils::container::u8string uwvm_abi{};        // Runtime ABI changes invalidate calls, relocations, and imported symbols.
        ::uwvm2::utils::container::u8string codegen_policy{};  // Optimization policy participates because it affects emitted native code.
        ::uwvm2::utils::container::array<::std::byte, cache_ed25519_seed_size> signature_seed{};  // The seed binds signatures to this cache identity.
        bool has_signature_seed{};     // A missing seed disables signing/verification instead of producing weak output.
        bool cache_key_is_complete{};  // Complete keys skip module hashing when the caller already supplied full identity.
    };

    struct cache_load_result
    {
        cache_status status{cache_status::disabled};              // Callers branch on status rather than exceptions in the JIT hot path.
        ::uwvm2::utils::container::vector<::std::byte> object{};  // The object bytes are owned after load so LLVM can consume a stable buffer.
        bool signature_verified{};                                // Logging exposes whether the hit passed trust checks.
        bool isa_matched{};                                       // Diagnostics can distinguish target misses from later context misses.
    };

    struct cache_fixed_header
    {
        ::std::byte magic[8]{};                         // Magic bytes make accidental file collisions cheap to reject.
        ::std::uint_least32_t version{};                // Versioning lets readers fail closed when the binary contract changes.
        ::std::uint_least32_t fixed_header_size{};      // The size is stored to reserve a clear path for future fixed-header extensions.
        ::std::uint_least32_t compression{};            // Codec choice is authenticated as part of the header and checked before decode.
        ::std::uint_least32_t signature{};              // Signature kind is explicit so unsigned and signed blobs are unambiguous.
        ::std::uint_least64_t uncompressed_size{};      // Expected size bounds allocations and verifies decompressor output exactly.
        ::std::uint_least64_t payload_size{};           // Payload size separates the byte stream from trailing garbage or truncation.
        ::std::uint_least64_t isa_metadata_size{};      // ISA metadata is length-delimited to support byte-for-byte equality checks.
        ::std::uint_least64_t context_metadata_size{};  // Context metadata is kept separate from ISA metadata for better diagnostics.
        ::std::uint_least64_t signature_size{};         // Signature size is validated before cryptographic verification.
    };

    struct cache_blob_view
    {
        cache_fixed_header header{};
        ::std::byte const* isa_metadata{};
        ::std::byte const* context_metadata{};
        ::std::byte const* signature{};
        ::std::byte const* payload{};
    };

    // All multibyte fields are serialized explicitly as little-endian bytes, so the file is host-endian independent.
    inline constexpr ::std::byte cache_magic[8]{::std::byte{'U'},
                                                ::std::byte{'W'},
                                                ::std::byte{'V'},
                                                ::std::byte{'M'},
                                                ::std::byte{'L'},
                                                ::std::byte{'J'},
                                                ::std::byte{'C'},
                                                ::std::byte{0x01u}};
    inline constexpr ::std::uint_least32_t cache_format_version{3u};
    inline constexpr ::std::size_t cache_fixed_header_size{64uz};
    inline constexpr ::std::size_t cache_sha256_digest_size{32uz};
    inline constexpr ::std::size_t cache_ed25519_signature_size{64uz};
    inline constexpr bool cache_ed25519_identity_signature_available{true};

    namespace details
    {
        inline constexpr ::std::size_t uint_least64_leb128_buffer_size{((::std::numeric_limits<unsigned char>::digits * sizeof(::std::uint_least64_t)) + 6uz) /
                                                                       7uz};
        static_assert(uint_least64_leb128_buffer_size >= 10uz);

        // LEB128 keeps metadata compact while preserving an unambiguous byte representation for hashes and signatures.
        [[nodiscard]] inline constexpr ::std::size_t uleb128_size(::std::uint_least64_t v) noexcept
        {
            ::std::size_t n{1uz};
            while((v >>= 7u) != 0u) { ++n; }
            return n;
        }

        [[nodiscard]] inline constexpr ::std::size_t key_value_size(::uwvm2::utils::container::u8string_view key, ::std::size_t value_size) noexcept
        {
            // The exact reserve size avoids reallocations while building metadata in the JIT path.
            return uleb128_size(static_cast<::std::uint_least64_t>(key.size())) + key.size() + uleb128_size(static_cast<::std::uint_least64_t>(value_size)) +
                   value_size;
        }

        inline constexpr void append_bytes(::uwvm2::utils::container::vector<::std::byte>& out, ::std::byte const* first, ::std::byte const* last) noexcept
        {
            for(; first != last; ++first) { out.push_back(*first); }
        }

        inline constexpr void append_u32_le(::uwvm2::utils::container::vector<::std::byte>& out, ::std::uint_least32_t v) noexcept
        {
            static_assert(sizeof(::std::uint_least32_t) >= 4uz);
            // The cache format is defined in bytes, not in native integer layout.
            auto const le{::fast_io::little_endian(v)};
            auto const first{reinterpret_cast<::std::byte const*>(::std::addressof(le))};
            append_bytes(out, first, first + 4uz);
        }

        inline constexpr void append_u64_le(::uwvm2::utils::container::vector<::std::byte>& out, ::std::uint_least64_t v) noexcept
        {
            static_assert(sizeof(::std::uint_least64_t) >= 8uz);
            // Fixed little-endian encoding makes cache blobs portable across supported host endianness.
            auto const le{::fast_io::little_endian(v)};
            auto const first{reinterpret_cast<::std::byte const*>(::std::addressof(le))};
            append_bytes(out, first, first + 8uz);
        }

        inline constexpr void append_u64_leb128(::uwvm2::utils::container::vector<::std::byte>& out, ::std::uint_least64_t v) noexcept
        {
            char buffer[uint_least64_leb128_buffer_size]{};
            ::fast_io::obuffer_view buffer_ref{buffer, buffer + sizeof(buffer)};
            ::fast_io::io::print(buffer_ref, ::fast_io::mnp::leb128_put(v));
            auto const first{reinterpret_cast<::std::byte const*>(buffer_ref.cbegin())};
            append_bytes(out, first, first + static_cast<::std::size_t>(buffer_ref.cend() - buffer_ref.cbegin()));
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

        inline constexpr void append_u8string_bytes(::uwvm2::utils::container::vector<::std::byte>& out, ::uwvm2::utils::container::u8string_view sv) noexcept
        {
            auto const first{reinterpret_cast<::std::byte const*>(sv.data())};
            append_bytes(out, first, first + sv.size());
        }

        inline constexpr void append_u8string_bytes(::uwvm2::utils::container::vector<::std::byte>& out, ::uwvm2::utils::container::u8string const& s) noexcept
        {
            auto const first{reinterpret_cast<::std::byte const*>(s.data())};
            append_bytes(out, first, first + s.size());
        }

        inline constexpr void append_cache_key_raw(::uwvm2::utils::container::u8string& out, ::std::byte const* first, ::std::size_t size) noexcept
        {
            if(size == 0uz) { return; }
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(first), size});
        }

        inline constexpr void append_cache_key_uleb128(::uwvm2::utils::container::u8string& out, ::std::uint_least64_t v) noexcept
        {
            char buffer[uint_least64_leb128_buffer_size]{};
            ::fast_io::obuffer_view buffer_ref{buffer, buffer + sizeof(buffer)};
            ::fast_io::io::print(buffer_ref, ::fast_io::mnp::leb128_put(v));
            append_cache_key_raw(out,
                                 reinterpret_cast<::std::byte const*>(buffer_ref.cbegin()),
                                 static_cast<::std::size_t>(buffer_ref.cend() - buffer_ref.cbegin()));
        }

        inline constexpr void append_cache_key_value_bytes(::uwvm2::utils::container::u8string& out,
                                                           ::uwvm2::utils::container::u8string_view key,
                                                           ::std::byte const* value,
                                                           ::std::size_t value_size) noexcept
        {
            // Length prefixes prevent ambiguous keys such as ("ab", "c") and ("a", "bc") from colliding.
            append_cache_key_uleb128(out, static_cast<::std::uint_least64_t>(key.size()));
            append_cache_key_raw(out, reinterpret_cast<::std::byte const*>(key.data()), key.size());
            append_cache_key_uleb128(out, static_cast<::std::uint_least64_t>(value_size));
            append_cache_key_raw(out, value, value_size);
        }

        inline constexpr void append_cache_key_value(::uwvm2::utils::container::u8string& out,
                                                     ::uwvm2::utils::container::u8string_view key,
                                                     ::uwvm2::utils::container::u8string_view value) noexcept
        { append_cache_key_value_bytes(out, key, reinterpret_cast<::std::byte const*>(value.data()), value.size()); }

        inline constexpr void append_cache_key_value(::uwvm2::utils::container::u8string& out,
                                                     ::uwvm2::utils::container::u8string_view key,
                                                     ::uwvm2::utils::container::u8string const& value) noexcept
        { append_cache_key_value(out, key, ::uwvm2::utils::container::u8string_view{value.data(), value.size()}); }

        inline constexpr void append_cache_key_value_u64(::uwvm2::utils::container::u8string& out,
                                                         ::uwvm2::utils::container::u8string_view key,
                                                         ::std::uint_least64_t value) noexcept
        {
            char buffer[uint_least64_leb128_buffer_size]{};
            ::fast_io::obuffer_view buffer_ref{buffer, buffer + sizeof(buffer)};
            ::fast_io::io::print(buffer_ref, ::fast_io::mnp::leb128_put(value));
            append_cache_key_value_bytes(out,
                                         key,
                                         reinterpret_cast<::std::byte const*>(buffer_ref.cbegin()),
                                         static_cast<::std::size_t>(buffer_ref.cend() - buffer_ref.cbegin()));
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string make_cache_key(::uwvm2::utils::container::u8string_view domain) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            out.reserve(64uz + domain.size());
            // A schema marker and domain keep independently produced cache keys from sharing the same hash namespace.
            append_cache_key_value(out, u8"format", u8"uwvm-ljc-key-v1");
            append_cache_key_value(out, u8"domain", domain);
            return out;
        }

        inline constexpr void append_key_value_bytes(::uwvm2::utils::container::vector<::std::byte>& out,
                                                     ::uwvm2::utils::container::u8string_view key,
                                                     ::std::byte const* value,
                                                     ::std::size_t value_size) noexcept
        {
            append_u64_leb128(out, static_cast<::std::uint_least64_t>(key.size()));
            append_u8string_bytes(out, key);
            append_u64_leb128(out, static_cast<::std::uint_least64_t>(value_size));
            append_bytes(out, value, value + value_size);
        }

        inline constexpr void append_key_value(::uwvm2::utils::container::vector<::std::byte>& out,
                                               ::uwvm2::utils::container::u8string_view key,
                                               ::uwvm2::utils::container::u8string_view value) noexcept
        { append_key_value_bytes(out, key, reinterpret_cast<::std::byte const*>(value.data()), value.size()); }

        inline constexpr void append_key_value(::uwvm2::utils::container::vector<::std::byte>& out,
                                               ::uwvm2::utils::container::u8string_view key,
                                               ::uwvm2::utils::container::u8string const& value) noexcept
        { append_key_value(out, key, ::uwvm2::utils::container::u8string_view{value.data(), value.size()}); }

        inline constexpr void append_key_value(::uwvm2::utils::container::vector<::std::byte>& out,
                                               ::uwvm2::utils::container::u8string_view key,
                                               ::uwvm2::utils::container::vector<::std::byte> const& value) noexcept
        { append_key_value_bytes(out, key, value.cbegin(), value.size()); }

        [[nodiscard]] inline constexpr bool checked_add(::std::size_t& v, ::std::uint_least64_t add) noexcept
        {
            // Blob sizes come from disk; every addition is checked before converting to host size_t.
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
        out.reserve(details::key_value_size(u8"target", ctx.target_triple.size()) + details::key_value_size(u8"cpu", ctx.cpu_name.size()) +
                    details::key_value_size(u8"features", ctx.cpu_features.size()));
        // ISA fields are isolated so a target miss can be reported before more general context validation.
        details::append_key_value(out, u8"target", ctx.target_triple);
        details::append_key_value(out, u8"cpu", ctx.cpu_name);
        details::append_key_value(out, u8"features", ctx.cpu_features);
        return out;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> make_context_metadata(cache_context const& ctx) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        out.reserve(details::key_value_size(u8"key", ctx.cache_key.size()) + details::key_value_size(u8"llvm", ctx.llvm_version.size()) +
                    details::key_value_size(u8"uwvm_abi", ctx.uwvm_abi.size()) + details::key_value_size(u8"codegen", ctx.codegen_policy.size()));
        // Context metadata captures the non-ISA inputs that can silently change symbol layout or object contents.
        details::append_key_value(out, u8"key", ctx.cache_key);
        details::append_key_value(out, u8"llvm", ctx.llvm_version);
        details::append_key_value(out, u8"uwvm_abi", ctx.uwvm_abi);
        details::append_key_value(out, u8"codegen", ctx.codegen_policy);
        return out;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::byte> serialize_fixed_header(cache_fixed_header const& header) noexcept
    {
        ::uwvm2::utils::container::vector<::std::byte> out{};
        out.reserve(cache_fixed_header_size);
        // Serialization is field-by-field so padding and compiler ABI never leak into the on-disk format.
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
        // Parsing mirrors serialization exactly so unsupported layout changes become version failures, not partial reads.
        for(::std::size_t i{}; i != 8uz; ++i) { header.magic[i] = first[i]; }
        first += 8uz;
        return details::read_u32_le(first, last, header.version) && details::read_u32_le(first, last, header.fixed_header_size) &&
               details::read_u32_le(first, last, header.compression) && details::read_u32_le(first, last, header.signature) &&
               details::read_u64_le(first, last, header.uncompressed_size) && details::read_u64_le(first, last, header.payload_size) &&
               details::read_u64_le(first, last, header.isa_metadata_size) && details::read_u64_le(first, last, header.context_metadata_size) &&
               details::read_u64_le(first, last, header.signature_size);
    }

    [[nodiscard]] inline constexpr cache_status parse_cache_blob(::std::byte const* first, ::std::byte const* last, cache_blob_view& view) noexcept
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

        // Views point into the mapped file to avoid copies; size validation above must therefore be exact.
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
        // Metadata is compared as canonical bytes; reparsing would add more failure modes and no extra trust.
        if(n != expected.size()) { return false; }
        if(n == 0u) { return true; }
        return ::std::memcmp(first, expected.data(), static_cast<::std::size_t>(n)) == 0;
    }
}  // namespace uwvm2::runtime::llvm_jit_cache

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
