/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 * @date        2025-11-01
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
#include <random>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string.h>
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

    // Build one data entry
    // modes:
    //  0: valid (memory_idx=0, expr=i32.const 0; end, byte_size match)
    //  1: expr missing end
    //  2: byte_size mismatch (declared > actual)
    //  3: invalid data_count LEB will be constructed outside (skip entries)
    //  4: global_get 0 (no imported global) - type/ref checks
    inline void append_one_data_entry(u8string& payload,
                                      ::fast_io::ibuf_white_hole_engine& eng,
                                      int mode) noexcept
    {
        ::std::uniform_int_distribution<int> prob{0, 99};
        ::std::uniform_int_distribution<int> data_len_eng{0, 16};

        // memory_idx (uleb): use 0 by default (we created 1 memory)
        append_uleb128(payload, 0u);

        // expr
        if(mode == 4)
        {
            // global_get 0; end
            append_byte(payload, 0x23u); // global.get
            append_uleb128(payload, 0u); // index 0 (will be invalid without imported globals)
            append_byte(payload, 0x0Bu); // end
        }
        else if(mode == 1)
        {
            // i32.const 0; (no end)
            append_byte(payload, 0x41u); // i32.const
            append_byte(payload, 0x00u); // sLEB 0
        }
        else
        {
            // i32.const 0; end
            append_byte(payload, 0x41u); // i32.const
            append_byte(payload, 0x00u); // sLEB 0
            append_byte(payload, 0x0Bu); // end
        }

        // byte_size and bytes
        ::std::uint32_t const real_len = static_cast<::std::uint32_t>(data_len_eng(eng));
        ::std::uint32_t declared = real_len;
        if(mode == 2 && real_len > 0u) { declared = real_len + 1u; }
        append_uleb128(payload, declared);
        for(::std::uint32_t i{}; i < real_len; ++i) { append_byte(payload, static_cast<::std::uint8_t>(prob(eng))); }
    }

    inline void build_data_section(u8string& out, ::fast_io::ibuf_white_hole_engine& eng) noexcept
    {
        u8string payload;
        ::std::uniform_int_distribution<int> prob{0, 99};
        ::std::uniform_int_distribution<int> cnt_eng{0, 4};

        bool invalid_count_leb = (prob(eng) < 4); // 4% invalid LEB for data_count
        if(invalid_count_leb)
        {
            payload.push_back(static_cast<char8_t>(0x80u)); // unterminated LEB128
            append_section_with_payload(out, /*data id*/ 11u, payload);
            return;
        }

        int count = cnt_eng(eng);
        append_uleb128(payload, static_cast<::std::uint32_t>(count));

        for(int i{}; i < count; ++i)
        {
            int mode = 0;
            int p = prob(eng);
            if(p < 6) mode = 1;       // 6% missing end
            else if(p < 12) mode = 2; // 6% size mismatch
            else if(p < 16) mode = 4; // 4% global_get
            append_one_data_entry(payload, eng, mode);
        }

        append_section_with_payload(out, /*data id*/ 11u, payload);
    }
}

int main()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    ::fast_io::io::perr("Starting data-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    ::fast_io::ibuf_white_hole_engine eng;

    for(unsigned round{}; round != 50000u; ++round)
    {
        ::uwvm2::utils::container::u8string mod;

        // wasm magic + version
        mod.push_back(u8'\0');
        mod.push_back(u8'a');
        mod.push_back(u8's');
        mod.push_back(u8'm');
        mod.push_back(static_cast<char8_t>(0x01));
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');

        // Memory section: ensure memory_idx=0 is valid in most runs
        test::build_memory_section_min(mod);

        // Data section
        test::build_data_section(mod, eng);

        auto const* begin = reinterpret_cast<::std::byte const*>(mod.data());
        auto const* end = begin + mod.size();

        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, err, {});
        }
        catch(::fast_io::error const&)
        {
            // expected on invalid cases; keep fuzzing
        }
    }

    ::fast_io::io::perr("Data-section fuzzing finished.\n");
}


