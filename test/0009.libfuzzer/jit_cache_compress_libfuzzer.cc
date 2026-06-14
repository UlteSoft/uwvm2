/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <cstddef>
#include <cstdint>
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
    using compression_kind = ::uwvm2::runtime::llvm_jit_cache::compression_kind;

    [[noreturn]] inline void fuzz_trap() noexcept
    {
        __builtin_trap();
    }

    [[nodiscard]] inline ::std::byte const* vector_data_or_empty(byte_vector const& bytes) noexcept
    {
        return bytes.size() == 0uz ? ::std::addressof(empty_byte) : bytes.data();
    }

    [[nodiscard]] inline bool equal_bytes(byte_vector const& bytes, ::std::byte const* expected, ::std::size_t expected_size) noexcept
    {
        if(bytes.size() != expected_size) { return false; }
        for(::std::size_t pos{}; pos != expected_size; ++pos)
        {
            if(bytes.index_unchecked(pos) != expected[pos]) { return false; }
        }
        return true;
    }

    [[nodiscard]] inline ::std::size_t bounded_expected_size(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        ::std::size_t value{};
        auto limit{size};
        if(limit > sizeof(::std::size_t)) { limit = sizeof(::std::size_t); }

        for(::std::size_t pos{}; pos != limit; ++pos)
        {
            value = (value << 8u) ^ static_cast<::std::size_t>(data[pos]);
        }

        return value % (max_decode_expected_size + 1uz);
    }

    inline void check_roundtrip(compression_kind kind, ::std::byte const* input, ::std::size_t size) noexcept
    {
        auto compressed{kind == compression_kind::uwvm_lzss ? ::uwvm2::runtime::llvm_jit_cache::compress_lzss(input, size)
                                                            : ::uwvm2::runtime::llvm_jit_cache::compress_native_lz(input, size)};
        auto const* compressed_first{vector_data_or_empty(compressed)};
        auto const* compressed_last{compressed_first + compressed.size()};

        byte_vector decoded{};
        auto ok{false};
        if(kind == compression_kind::uwvm_lzss)
        {
            ok = ::uwvm2::runtime::llvm_jit_cache::decompress_lzss(compressed_first, compressed_last, size, decoded);
        }
        else
        {
            ok = ::uwvm2::runtime::llvm_jit_cache::decompress_native_lz(compressed_first, compressed_last, size, decoded);
        }

        if(!ok || !equal_bytes(decoded, input, size)) { fuzz_trap(); }

        byte_vector payload_decoded{};
        if(!::uwvm2::runtime::llvm_jit_cache::decompress_payload(kind, compressed_first, compressed.size(), size, payload_decoded) ||
           !equal_bytes(payload_decoded, input, size))
        {
            fuzz_trap();
        }
    }

    inline void check_direct_copy(::std::byte const* input, ::std::size_t size, ::std::size_t mismatched_size) noexcept
    {
        byte_vector decoded{};
        if(!::uwvm2::runtime::llvm_jit_cache::decompress_payload(compression_kind::none, input, size, size, decoded) ||
           !equal_bytes(decoded, input, size))
        {
            fuzz_trap();
        }

        if(mismatched_size != size)
        {
            byte_vector rejected{};
            if(::uwvm2::runtime::llvm_jit_cache::decompress_payload(compression_kind::none, input, size, mismatched_size, rejected))
            {
                fuzz_trap();
            }
        }
    }

    inline void check_decode_dispatch(compression_kind kind, ::std::byte const* input, ::std::size_t size, ::std::size_t expected_size) noexcept
    {
        byte_vector direct{};
        auto direct_ok{false};

        if(kind == compression_kind::uwvm_lzss)
        {
            direct_ok = ::uwvm2::runtime::llvm_jit_cache::decompress_lzss(input, input + size, expected_size, direct);
        }
        else
        {
            direct_ok = ::uwvm2::runtime::llvm_jit_cache::decompress_native_lz(input, input + size, expected_size, direct);
        }

        if(direct_ok && direct.size() != expected_size) { fuzz_trap(); }

        byte_vector dispatched{};
        auto const dispatched_ok{::uwvm2::runtime::llvm_jit_cache::decompress_payload(kind, input, size, expected_size, dispatched)};
        if(dispatched_ok != direct_ok) { fuzz_trap(); }
        if(dispatched_ok && !(dispatched == direct)) { fuzz_trap(); }
    }
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    auto fuzz_size{size};
    if(fuzz_size > max_fuzz_input_size) { fuzz_size = max_fuzz_input_size; }

    auto const* input{reinterpret_cast<::std::byte const*>(data)};
    if(fuzz_size == 0uz) { input = ::std::addressof(empty_byte); }

    auto const expected_size{bounded_expected_size(data, fuzz_size)};

    check_roundtrip(compression_kind::uwvm_lzss, input, fuzz_size);
    check_roundtrip(compression_kind::uwvm_native_lz, input, fuzz_size);
    check_direct_copy(input, fuzz_size, expected_size);

    check_decode_dispatch(compression_kind::uwvm_lzss, input, fuzz_size, expected_size);
    check_decode_dispatch(compression_kind::uwvm_native_lz, input, fuzz_size, expected_size);

    byte_vector rejected{};
    auto const invalid_kind{static_cast<compression_kind>(3u + static_cast<unsigned>(size == 0uz ? 0u : data[0]))};
    if(::uwvm2::runtime::llvm_jit_cache::decompress_payload(invalid_kind, input, fuzz_size, expected_size, rejected)) { fuzz_trap(); }

    return 0;
}
