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
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/value_binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/base/impl.h>
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

    // Type section: 1 function type: 0x60 () -> ()
    inline void build_type_section_min(u8string& mod) noexcept
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm1::type;
        u8string payload;
        append_uleb128(payload, 1u); // type count
        append_byte(payload, static_cast<::std::uint8_t>(function_type_prefix::functype));
        append_uleb128(payload, 0u); // params
        append_uleb128(payload, 0u); // results
        append_section_with_payload(mod, /*type id*/ 1u, payload);
    }

    // Import section: import one function (module "m", name "f") with typeidx 0
    inline void build_import_section_one_func(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u); // import count
        // module name
        payload.push_back(static_cast<char8_t>(1));
        payload.push_back(u8'm');
        // extern name
        payload.push_back(static_cast<char8_t>(1));
        payload.push_back(u8'f');
        // import desc: func
        payload.push_back(static_cast<char8_t>(0x00));
        // typeidx
        append_uleb128(payload, 0u);
        append_section_with_payload(mod, /*import id*/ 2u, payload);
    }

    // Table section: 1 table (funcref=0x70), limits flag=0x00, min=1
    inline void build_table_section_min(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u); // table count
        append_byte(payload, 0x70u); // elemtype funcref
        append_byte(payload, 0x00u); // limits flag
        append_uleb128(payload, 1u); // min
        append_section_with_payload(mod, /*table id*/ 4u, payload);
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

    // Minimal prerequisites for element section
    test::build_type_section_min(mod);
    test::build_import_section_one_func(mod); // provides 1 imported func
    test::build_table_section_min(mod);       // provides table 0

    // Append one Element section (id=9) using fuzz bytes as payload
    // Payload format (wasm1): [elem_count LEB] { [table_idx LEB][expr ... 0x0B][funcidx_count LEB][funcidx*] }*
    test::append_byte(mod, 9u);
    test::append_uleb128(mod, static_cast<::std::uint32_t>(size));
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


