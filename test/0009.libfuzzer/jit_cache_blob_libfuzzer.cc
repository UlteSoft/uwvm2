/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#ifndef UWVM_MODULE
# include <uwvm2/runtime/llvm_jit_cache/compress.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    inline constexpr ::std::size_t max_fuzz_input_size{64uz * 1024uz};
    inline constexpr ::std::size_t max_decode_expected_size{64uz * 1024uz};
    inline constexpr ::std::byte empty_byte{};

    using byte_vector = ::uwvm2::utils::container::vector<::std::byte>;
    using cache_blob_view = ::uwvm2::runtime::llvm_jit_cache::cache_blob_view;
    using cache_fixed_header = ::uwvm2::runtime::llvm_jit_cache::cache_fixed_header;
    using cache_status = ::uwvm2::runtime::llvm_jit_cache::cache_status;
    using compression_kind = ::uwvm2::runtime::llvm_jit_cache::compression_kind;

    [[noreturn]] inline void fuzz_trap() noexcept
    {
        __builtin_trap();
    }

    [[nodiscard]] inline ::std::byte const* bytes_or_empty(byte_vector const& bytes) noexcept
    {
        return bytes.size() == 0uz ? ::std::addressof(empty_byte) : bytes.data();
    }

    [[nodiscard]] inline ::std::byte const* input_or_empty(::std::byte const* data, ::std::size_t size) noexcept
    {
        return size == 0uz ? ::std::addressof(empty_byte) : data;
    }

    [[nodiscard]] inline ::std::size_t bounded_expected_size(::std::uint8_t const* data, ::std::size_t size, ::std::uint_least64_t salt) noexcept
    {
        ::std::size_t value{static_cast<::std::size_t>(salt)};
        auto limit{size};
        if(limit > sizeof(::std::size_t)) { limit = sizeof(::std::size_t); }

        for(::std::size_t pos{}; pos != limit; ++pos)
        {
            value = (value << 5u) ^ (value >> 2u) ^ static_cast<::std::size_t>(data[pos]);
        }

        return value % (max_decode_expected_size + 1uz);
    }

    inline void append_input_bytes(byte_vector& out, ::std::byte const* first, ::std::size_t size) noexcept
    {
        ::uwvm2::runtime::llvm_jit_cache::details::append_bytes(out, first, first + size);
    }

    inline void check_parsed_view(::std::byte const* first, ::std::byte const* last, cache_blob_view const& view) noexcept;

    [[nodiscard]] inline cache_status parse_blob(byte_vector const& blob, cache_blob_view& view) noexcept
    {
        auto const* first{bytes_or_empty(blob)};
        return ::uwvm2::runtime::llvm_jit_cache::parse_cache_blob(first, first + blob.size(), view);
    }

    inline void expect_parse_status(byte_vector const& blob, cache_status expected) noexcept
    {
        cache_blob_view view{};
        auto const status{parse_blob(blob, view)};
        if(status != expected) { fuzz_trap(); }
        if(status == cache_status::ok) { check_parsed_view(bytes_or_empty(blob), bytes_or_empty(blob) + blob.size(), view); }
    }

    [[nodiscard]] inline byte_vector copy_bytes(::std::byte const* first, ::std::size_t size) noexcept
    {
        byte_vector out{};
        out.reserve(size);
        append_input_bytes(out, first, size);
        return out;
    }

    [[nodiscard]] inline byte_vector make_blob(cache_fixed_header const& header,
                                               byte_vector const& isa_metadata,
                                               byte_vector const& context_metadata,
                                               byte_vector const& signature,
                                               byte_vector const& payload) noexcept
    {
        auto blob{::uwvm2::runtime::llvm_jit_cache::serialize_fixed_header(header)};
        blob.reserve(blob.size() + isa_metadata.size() + context_metadata.size() + signature.size() + payload.size());
        append_input_bytes(blob, bytes_or_empty(isa_metadata), isa_metadata.size());
        append_input_bytes(blob, bytes_or_empty(context_metadata), context_metadata.size());
        append_input_bytes(blob, bytes_or_empty(signature), signature.size());
        append_input_bytes(blob, bytes_or_empty(payload), payload.size());
        return blob;
    }

    inline void check_metadata_equal(::std::byte const* first, ::std::size_t size) noexcept
    {
        auto expected{copy_bytes(first, size)};
        if(!::uwvm2::runtime::llvm_jit_cache::metadata_equal(first, size, expected)) { fuzz_trap(); }
        if(!expected.empty())
        {
            expected.index_unchecked(0uz) ^= ::std::byte{0x80u};
            if(::uwvm2::runtime::llvm_jit_cache::metadata_equal(first, size, expected)) { fuzz_trap(); }
        }
    }

    inline void check_decompress(cache_blob_view const& view) noexcept
    {
        auto const compression{static_cast<compression_kind>(view.header.compression)};
        if(compression != compression_kind::none && compression != compression_kind::uwvm_lzss &&
           compression != compression_kind::uwvm_native_lz)
        {
            return;
        }
        if(view.header.uncompressed_size > max_decode_expected_size) { return; }
        if(view.header.payload_size > max_fuzz_input_size) { return; }

        byte_vector decoded{};
        auto const ok{::uwvm2::runtime::llvm_jit_cache::decompress_payload(compression,
                                                                           view.payload,
                                                                           static_cast<::std::size_t>(view.header.payload_size),
                                                                           static_cast<::std::size_t>(view.header.uncompressed_size),
                                                                           decoded)};
        if(ok && decoded.size() != static_cast<::std::size_t>(view.header.uncompressed_size)) { fuzz_trap(); }
    }

    inline void check_parsed_view(::std::byte const* first, ::std::byte const* last, cache_blob_view const& view) noexcept
    {
        auto const isa_size{static_cast<::std::size_t>(view.header.isa_metadata_size)};
        auto const context_size{static_cast<::std::size_t>(view.header.context_metadata_size)};
        auto const signature_size{static_cast<::std::size_t>(view.header.signature_size)};
        auto const payload_size{static_cast<::std::size_t>(view.header.payload_size)};

        if(view.isa_metadata < first || view.isa_metadata > last) { fuzz_trap(); }
        if(view.context_metadata < view.isa_metadata || view.context_metadata > last) { fuzz_trap(); }
        if(view.signature < view.context_metadata || view.signature > last) { fuzz_trap(); }
        if(view.payload < view.signature || view.payload > last) { fuzz_trap(); }
        if(static_cast<::std::size_t>(last - view.payload) != payload_size) { fuzz_trap(); }
        if(static_cast<::std::size_t>(view.context_metadata - view.isa_metadata) != isa_size) { fuzz_trap(); }
        if(static_cast<::std::size_t>(view.signature - view.context_metadata) != context_size) { fuzz_trap(); }
        if(static_cast<::std::size_t>(view.payload - view.signature) != signature_size) { fuzz_trap(); }

        check_metadata_equal(view.isa_metadata, isa_size);
        check_metadata_equal(view.context_metadata, context_size);
        check_decompress(view);
    }

    inline void check_raw_parse(::std::byte const* input, ::std::size_t size) noexcept
    {
        auto const* first{input_or_empty(input, size)};
        auto const* last{first + size};
        cache_blob_view view{};
        auto const status{::uwvm2::runtime::llvm_jit_cache::parse_cache_blob(first, last, view)};
        if(status == cache_status::ok) { check_parsed_view(first, last, view); }
    }

    [[nodiscard]] inline compression_kind compression_from_control(unsigned control) noexcept
    {
        switch(control % 4u)
        {
            case 0u: return compression_kind::none;
            case 1u: return compression_kind::uwvm_lzss;
            case 2u: return compression_kind::uwvm_native_lz;
            default: return static_cast<compression_kind>(0x8000u | control);
        }
    }

    inline void append_slice(byte_vector& out, ::std::byte const* input, ::std::size_t size, ::std::size_t& cursor, ::std::size_t count) noexcept
    {
        if(cursor > size) { cursor = size; }
        auto const available{size - cursor};
        if(count > available) { count = available; }
        append_input_bytes(out, input + cursor, count);
        cursor += count;
    }

    inline void check_synthetic_blob(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        auto const* input{reinterpret_cast<::std::byte const*>(data)};
        input = input_or_empty(input, size);

        auto const control0{size == 0uz ? 0u : static_cast<unsigned>(data[0])};
        auto const control1{size <= 1uz ? 0u : static_cast<unsigned>(data[1])};
        auto const control2{size <= 2uz ? 0u : static_cast<unsigned>(data[2])};
        auto const compression{compression_from_control(control0)};

        ::std::size_t cursor{3uz};
        if(cursor > size) { cursor = size; }

        auto const isa_request{static_cast<::std::size_t>(control1 % 31u)};
        auto const context_request{static_cast<::std::size_t>(control2 % 63u)};
        auto const signature_request{(control0 & 0x08u) == 0u ? 0uz : static_cast<::std::size_t>((control1 % 65u))};

        byte_vector isa_metadata{};
        byte_vector context_metadata{};
        byte_vector signature{};
        byte_vector payload{};
        append_slice(isa_metadata, input, size, cursor, isa_request);
        append_slice(context_metadata, input, size, cursor, context_request);
        append_slice(signature, input, size, cursor, signature_request);
        append_slice(payload, input, size, cursor, size - cursor);

        cache_fixed_header header{};
        ::std::copy(::uwvm2::runtime::llvm_jit_cache::cache_magic,
                    ::uwvm2::runtime::llvm_jit_cache::cache_magic + 8uz,
                    header.magic);
        header.version = ::uwvm2::runtime::llvm_jit_cache::cache_format_version;
        header.fixed_header_size = static_cast<::std::uint_least32_t>(::uwvm2::runtime::llvm_jit_cache::cache_fixed_header_size);
        header.compression = static_cast<::std::uint_least32_t>(compression);
        header.signature = signature.empty() ? static_cast<::std::uint_least32_t>(::uwvm2::runtime::llvm_jit_cache::signature_kind::none)
                                             : static_cast<::std::uint_least32_t>(::uwvm2::runtime::llvm_jit_cache::signature_kind::ed25519_identity);
        header.payload_size = static_cast<::std::uint_least64_t>(payload.size());
        header.uncompressed_size = compression == compression_kind::none ? header.payload_size
                                                                         : bounded_expected_size(data, size, 0xa0761d6478bd642full);
        header.isa_metadata_size = static_cast<::std::uint_least64_t>(isa_metadata.size());
        header.context_metadata_size = static_cast<::std::uint_least64_t>(context_metadata.size());
        header.signature_size = static_cast<::std::uint_least64_t>(signature.size());

        auto blob{::uwvm2::runtime::llvm_jit_cache::serialize_fixed_header(header)};
        blob.reserve(blob.size() + isa_metadata.size() + context_metadata.size() + signature.size() + payload.size());
        append_input_bytes(blob, bytes_or_empty(isa_metadata), isa_metadata.size());
        append_input_bytes(blob, bytes_or_empty(context_metadata), context_metadata.size());
        append_input_bytes(blob, bytes_or_empty(signature), signature.size());
        append_input_bytes(blob, bytes_or_empty(payload), payload.size());

        cache_blob_view view{};
        auto const status{::uwvm2::runtime::llvm_jit_cache::parse_cache_blob(blob.cbegin(), blob.cend(), view)};
        if(status != cache_status::ok) { fuzz_trap(); }
        check_parsed_view(blob.cbegin(), blob.cend(), view);
    }

    inline void check_header_edge_cases(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        auto const* input{reinterpret_cast<::std::byte const*>(data)};
        input = input_or_empty(input, size);

        auto const control0{size == 0uz ? 0u : static_cast<unsigned>(data[0])};
        auto const control1{size <= 1uz ? 0u : static_cast<unsigned>(data[1])};
        auto const control2{size <= 2uz ? 0u : static_cast<unsigned>(data[2])};

        ::std::size_t cursor{3uz};
        if(cursor > size) { cursor = size; }

        byte_vector isa_metadata{};
        byte_vector context_metadata{};
        byte_vector payload{};
        append_slice(isa_metadata, input, size, cursor, static_cast<::std::size_t>(control0 % 17u));
        append_slice(context_metadata, input, size, cursor, static_cast<::std::size_t>(control1 % 29u));
        append_slice(payload, input, size, cursor, size - cursor);

        cache_fixed_header header{};
        ::std::copy(::uwvm2::runtime::llvm_jit_cache::cache_magic,
                    ::uwvm2::runtime::llvm_jit_cache::cache_magic + 8uz,
                    header.magic);
        header.version = ::uwvm2::runtime::llvm_jit_cache::cache_format_version;
        header.fixed_header_size = static_cast<::std::uint_least32_t>(::uwvm2::runtime::llvm_jit_cache::cache_fixed_header_size);
        header.compression = static_cast<::std::uint_least32_t>(compression_kind::none);
        header.signature = static_cast<::std::uint_least32_t>(::uwvm2::runtime::llvm_jit_cache::signature_kind::none);
        header.uncompressed_size = static_cast<::std::uint_least64_t>(payload.size());
        header.payload_size = static_cast<::std::uint_least64_t>(payload.size());
        header.isa_metadata_size = static_cast<::std::uint_least64_t>(isa_metadata.size());
        header.context_metadata_size = static_cast<::std::uint_least64_t>(context_metadata.size());
        header.signature_size = 0u;

        byte_vector signature{};
        auto valid_blob{make_blob(header, isa_metadata, context_metadata, signature, payload)};
        expect_parse_status(valid_blob, cache_status::ok);

        auto bad_magic_header{header};
        bad_magic_header.magic[control0 % 8u] ^= ::std::byte{0x80u};
        expect_parse_status(make_blob(bad_magic_header, isa_metadata, context_metadata, signature, payload), cache_status::invalid_magic);

        auto bad_version_header{header};
        bad_version_header.version += 1u + static_cast<::std::uint_least32_t>(control0);
        expect_parse_status(make_blob(bad_version_header, isa_metadata, context_metadata, signature, payload),
                            cache_status::unsupported_version);

        auto bad_fixed_header_size{header};
        bad_fixed_header_size.fixed_header_size += 1u + static_cast<::std::uint_least32_t>(control1);
        expect_parse_status(make_blob(bad_fixed_header_size, isa_metadata, context_metadata, signature, payload),
                            cache_status::unsupported_version);

        auto bad_payload_size_header{header};
        bad_payload_size_header.payload_size += 1u + static_cast<::std::uint_least64_t>(control2);
        expect_parse_status(make_blob(bad_payload_size_header, isa_metadata, context_metadata, signature, payload), cache_status::malformed);

        auto overflow_header{header};
        overflow_header.isa_metadata_size = (::std::numeric_limits<::std::uint_least64_t>::max)();
        expect_parse_status(make_blob(overflow_header, isa_metadata, context_metadata, signature, payload), cache_status::malformed);

        auto extra_tail_blob{valid_blob};
        extra_tail_blob.push_back(static_cast<::std::byte>(control0 ^ 0xa5u));
        expect_parse_status(extra_tail_blob, cache_status::malformed);

        if(valid_blob.size() != 0uz)
        {
            auto truncated_blob{valid_blob};
            truncated_blob.resize(truncated_blob.size() - 1uz);
            expect_parse_status(truncated_blob, cache_status::malformed);
        }

        byte_vector ed25519_signature{};
        ed25519_signature.reserve(::uwvm2::runtime::llvm_jit_cache::cache_ed25519_signature_size);
        for(::std::size_t pos{}; pos != ::uwvm2::runtime::llvm_jit_cache::cache_ed25519_signature_size; ++pos)
        {
            auto const value{size == 0uz ? static_cast<unsigned>(pos) : static_cast<unsigned>(data[pos % size]) ^ static_cast<unsigned>(pos)};
            ed25519_signature.push_back(static_cast<::std::byte>(value));
        }

        auto signed_header{header};
        signed_header.signature = static_cast<::std::uint_least32_t>(::uwvm2::runtime::llvm_jit_cache::signature_kind::ed25519_identity);
        signed_header.signature_size = ::uwvm2::runtime::llvm_jit_cache::cache_ed25519_signature_size;
        expect_parse_status(make_blob(signed_header, isa_metadata, context_metadata, ed25519_signature, payload), cache_status::ok);

        auto odd_signature_header{header};
        odd_signature_header.signature = 0x80000000u | static_cast<::std::uint_least32_t>(control0);
        odd_signature_header.signature_size = static_cast<::std::uint_least64_t>(control1 % 9u);
        byte_vector odd_signature{};
        odd_signature.reserve(static_cast<::std::size_t>(odd_signature_header.signature_size));
        for(::std::size_t pos{}; pos != static_cast<::std::size_t>(odd_signature_header.signature_size); ++pos)
        {
            odd_signature.push_back(static_cast<::std::byte>(control2 + pos));
        }
        expect_parse_status(make_blob(odd_signature_header, isa_metadata, context_metadata, odd_signature, payload), cache_status::ok);

        auto unsupported_compression_header{header};
        unsupported_compression_header.compression = 0x40000000u | static_cast<::std::uint_least32_t>(control2);
        expect_parse_status(make_blob(unsupported_compression_header, isa_metadata, context_metadata, signature, payload), cache_status::ok);
    }
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    auto fuzz_size{size};
    if(fuzz_size > max_fuzz_input_size) { fuzz_size = max_fuzz_input_size; }

    auto const* input{reinterpret_cast<::std::byte const*>(data)};
    check_raw_parse(input, fuzz_size);
    check_synthetic_blob(data, fuzz_size);
    check_header_edge_cases(data, fuzz_size);
    return 0;
}
