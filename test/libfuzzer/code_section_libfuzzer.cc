/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
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

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/value_binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    using ::uwvm2::utils::container::u8string;

    inline void append_byte(u8string& out, ::std::uint8_t b) noexcept { out.push_back(static_cast<char8_t>(b)); }

    // Minimal uleb128 encoder
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

    // Build a minimal type section: one function type: 0x60, vec(param=0), vec(result=0)
    inline void build_min_type_section(u8string& out) noexcept
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm1::type;
        u8string payload;
        append_uleb128(payload, 1u);                                      // type count
        append_byte(payload, static_cast<::std::uint8_t>(function_type_prefix::functype));
        append_uleb128(payload, 0u);                                       // param count = 0
        append_uleb128(payload, 0u);                                       // result count = 0 (MVP)
        append_section_with_payload(out, /*type id*/ 1u, payload);
    }

    // Build a minimal function section: one function, typeidx=0
    inline void build_min_function_section(u8string& out) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u); // func count
        append_uleb128(payload, 0u); // typeidx 0
        append_section_with_payload(out, /*function id*/ 3u, payload);
    }

    // Build a code section with one body, whose content is derived from fuzz input
    inline void build_code_section_from_fuzz(u8string& out, ::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm1::type;

        u8string payload;
        append_uleb128(payload, 1u); // code_count = 1 (matches function count)

        // Construct code body: [local_count][locals...][expr...]
        u8string body;

        ::std::size_t pos{};
        ::std::uint32_t local_groups{};
        if(size != 0u)
        {
            local_groups = static_cast<::std::uint32_t>(data[pos] % 3u); // 0..2 groups
            ++pos;
        }
        append_uleb128(body, local_groups);

        for(::std::uint32_t g{}; g < local_groups; ++g)
        {
            ::std::uint32_t cnt{};
            if(pos < size) { cnt = static_cast<::std::uint32_t>(data[pos] % 4u); ++pos; } // 0..3 locals in this group
            append_uleb128(body, cnt);

            ::std::uint8_t vt_byte{0x7Fu}; // default i32
            if(pos < size)
            {
                auto const b = data[pos++];
                // 50% illegal value type
                if((b & 0x80u) == 0u)
                {
                    switch(b & 0x3u)
                    {
                        case 0: vt_byte = static_cast<::std::uint8_t>(value_type::i32); break;
                        case 1: vt_byte = static_cast<::std::uint8_t>(value_type::i64); break;
                        case 2: vt_byte = static_cast<::std::uint8_t>(value_type::f32); break;
                        default: vt_byte = static_cast<::std::uint8_t>(value_type::f64); break;
                    }
                }
                else
                {
                    vt_byte = b; // likely illegal
                }
            }
            append_byte(body, vt_byte);
        }

        // Append remaining bytes as expression body; do not force trailing 0x0B to allow libFuzzer to discover it
        for(; pos < size; ++pos) { append_byte(body, data[pos]); }

        // body_size declaration: allow size mismatch to increase coverage based on first byte flags
        ::std::uint32_t const real_size = static_cast<::std::uint32_t>(body.size());
        ::std::uint32_t declared_size = real_size;
        if(size != 0u)
        {
            if((data[0] & 0x40u) && real_size > 0u) { declared_size = real_size - 1u; }
            else if(data[0] & 0x20u) { declared_size = real_size + 1u; }
        }

        append_uleb128(payload, declared_size);
        for(auto ch: body) { payload.push_back(ch); }

        append_section_with_payload(out, /*code id*/ 10u, payload);
    }
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    // Assemble a minimal module: magic+version, Type, Function, Code(from fuzz)
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

    test::build_min_type_section(mod);
    test::build_min_function_section(mod);
    test::build_code_section_from_fuzz(mod, data, size);

    auto const* begin = reinterpret_cast<::std::byte const*>(mod.data());
    auto const* end = begin + mod.size();

    ::uwvm2::parser::wasm::base::error_impl err{};
    try
    {
        (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, err, {});
    }
    catch(::fast_io::error const&)
    {
        // Expected on invalid inputs; libFuzzer will keep mutating
    }
    return 0;
}


