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

    // Minimal uleb128 encoder with optional redundant continuation zeros
    inline void append_uleb128(u8string& out, ::std::uint32_t value, bool add_redundant, ::std::uint32_t redundant_count) noexcept
    {
        ::std::uint32_t v = value;
        // encode minimal form
        do
        {
            ::std::uint8_t byte = static_cast<::std::uint8_t>(v & 0x7Fu);
            v >>= 7u;
            if(v != 0u) { byte = static_cast<::std::uint8_t>(byte | 0x80u); }
            append_byte(out, byte);
        }
        while(v != 0u);

        if(add_redundant && redundant_count != 0u)
        {
            // Turn last terminator into a continuation, then chain zeros, then final terminator 0x00
            auto const n = out.size();
            if(n != 0u)
            {
                out[n - 1u] = static_cast<char8_t>(static_cast<::std::uint8_t>(out[n - 1u]) | static_cast<::std::uint8_t>(0x80u));
                for(::std::uint32_t i{}; i + 1u < redundant_count; ++i) { append_byte(out, 0x80u); }
                append_byte(out, 0x00u);
            }
        }
    }

    // Write a random section: [sec_id][sec_len][payload]
    inline void append_random_section(u8string& out, ::fast_io::ibuf_white_hole_engine& eng, ::std::uniform_int_distribution<::std::uint32_t>& len_eng) noexcept
    {
        // Random sec id (0..20) to cover custom(0) and invalid/unknown ids
        ::std::uniform_int_distribution<int> id_eng{0, 20};
        ::std::uint8_t sec_id = static_cast<::std::uint8_t>(id_eng(eng));
        append_byte(out, sec_id);

        // Error knobs
        ::std::uniform_int_distribution<int> prob{0, 99};
        bool make_incomplete_leb = (prob(eng) < 3);                        // 3%: incomplete leb128 for sec_len
        bool make_len_mismatch = !make_incomplete_leb && (prob(eng) < 8);  // 8%: length mismatch
        bool add_redundant = (prob(eng) < 20);                             // 20%: redundant leb128 bytes

        if(make_incomplete_leb)
        {
            // Force an unterminated LEB128 sequence (single 0x80)
            append_byte(out, 0x80u);
            return;  // payload omitted on purpose
        }

        // Decide payload length (cap to avoid huge buffers)
        ::std::uint32_t want_len = len_eng(eng) % 1024u;  // 0..1023

        // Encode sec_len
        append_uleb128(out, want_len, add_redundant, static_cast<::std::uint32_t>(1u + prob(eng) % 6));

        // Decide actual payload bytes (maybe mismatch)
        ::std::uint32_t real_len = want_len;
        if(make_len_mismatch)
        {
            // 50% shorter, 50% longer (longer will be truncated by buffer limit below)
            if(prob(eng) < 50) { real_len = want_len == 0u ? 0u : (want_len - 1u); }
            else
            {
                real_len = want_len + 1u;
            }
        }

        // Emit payload
        for(::std::uint32_t i{}; i != real_len; ++i) { append_byte(out, static_cast<::std::uint8_t>(prob(eng))); }
    }
}  // namespace test

int main()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    ::fast_io::io::perr("Starting full-module fuzzer for wasm binfmt v1 (wasm1) ...\n");

    ::fast_io::ibuf_white_hole_engine eng;
    ::std::uniform_int_distribution<::std::uint32_t> len_eng{0u, 4096u};
    ::std::uniform_int_distribution<int> prob{0, 99};

    for(unsigned round{}; round != 50000u; ++round)
    {
        ::uwvm2::utils::container::u8string mod;

        bool valid_header = (prob(eng) < 85);  // 85% valid header
        if(valid_header)
        {
            // wasm magic + version: 00 61 73 6D 01 00 00 00
            mod.push_back(u8'\0');
            mod.push_back(u8'a');
            mod.push_back(u8's');
            mod.push_back(u8'm');
            mod.push_back(static_cast<char8_t>(0x01));
            mod.push_back(u8'\0');
            mod.push_back(u8'\0');
            mod.push_back(u8'\0');
        }
        else
        {
            // Corrupt magic/version
            mod.push_back(u8'\0');
            mod.push_back(u8'a');
            mod.push_back(u8's');
            mod.push_back(u8'X');
            mod.push_back(static_cast<char8_t>(0x02));
            mod.push_back(u8'\0');
            mod.push_back(u8'\0');
            mod.push_back(u8'\0');
        }

        // Number of sections
        ::std::uniform_int_distribution<int> sec_cnt_eng{0, 20};
        int sec_cnt = sec_cnt_eng(eng);

        for(int i{}; i != sec_cnt; ++i) { test::append_random_section(mod, eng, len_eng); }

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

    ::fast_io::io::perr("Full-module fuzzing finished.\n");
}

