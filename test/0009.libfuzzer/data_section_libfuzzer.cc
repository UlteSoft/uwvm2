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
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    using ::uwvm2::utils::container::u8string;

    inline void append_byte(u8string& out, ::std::uint8_t b) noexcept { out.push_back(static_cast<char8_t>(b)); }

    inline void append_uleb128(u8string& out, ::std::uint32_t value) noexcept
    {
        ::std::uint32_t v = value;
        do
        {
            ::std::uint8_t byte = static_cast<::std::uint8_t>(v & 0x7Fu);
            v >>= 7u;
            if(v != 0u) { byte = static_cast<::std::uint8_t>(byte | 0x80u); }
            append_byte(out, byte);
        }
        while(v != 0u);
    }

    inline void append_section_with_payload(u8string& mod, ::std::uint8_t sec_id, u8string const& payload) noexcept
    {
        append_byte(mod, sec_id);
        append_uleb128(mod, static_cast<::std::uint32_t>(payload.size()));
        for(auto ch: payload) { mod.push_back(ch); }
    }

    // Build Memory section with 1 memory: flags=0x00, min=1 page
    inline void build_memory_section_min(u8string& out) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);   // memory count = 1
        append_byte(payload, 0x00u);   // limits flag: no max
        append_uleb128(payload, 1u);   // min = 1 page
        append_section_with_payload(out, /*memory id*/ 5u, payload);
    }
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    ::uwvm2::utils::container::u8string mod;

    // wasm magic + version: 00 61 73 6D 01 00 00 00
    mod.push_back(u8'\0');
    mod.push_back(u8'a');
    mod.push_back(u8's');
    mod.push_back(u8'm');
    mod.push_back(static_cast<char8_t>(0x01));
    mod.push_back(u8'\0');
    mod.push_back(u8'\0');
    mod.push_back(u8'\0');

    // Prepend a minimal Memory section so memory_idx=0 can be valid
    test::build_memory_section_min(mod);

    // Append one Data section (id=11), using fuzz bytes as the entire payload
    // Data payload format (wasm1): [data_count LEB] { [memory_idx LEB][expr ... 0x0B][byte_size LEB][bytes...] }*
    // Fuzzer will mutate to explore valid/invalid variants.
    test::append_byte(mod, 11u); // section id = data
    test::append_uleb128(mod, static_cast<::std::uint32_t>(size)); // section length
    for(::std::size_t i{}; i < size; ++i) { mod.push_back(static_cast<char8_t>(data[i])); }

    auto const* begin = reinterpret_cast<::std::byte const*>(mod.data());
    auto const* end = begin + mod.size();

    ::uwvm2::parser::wasm::base::error_impl err{};
    try
    {
        (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, err, {});
    }
    catch(::fast_io::error const&)
    {
        // Expected on invalid inputs
    }
    return 0;
}


