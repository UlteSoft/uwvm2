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

    // Build one custom section payload under different error modes
    // modes:
    //  0: valid name(len 0..8), payload 0..16
    //  1: invalid LEB for name_len (single 0x80)
    //  2: name_len > remaining bytes (illegal_custom_name_length)
    inline void build_one_custom_payload(u8string& payload,
                                         ::fast_io::ibuf_white_hole_engine& eng,
                                         int mode) noexcept
    {
        ::std::uniform_int_distribution<int> prob{0, 99};
        ::std::uniform_int_distribution<int> name_len_eng{0, 8};
        ::std::uniform_int_distribution<int> data_len_eng{0, 16};

        if(mode == 1)
        {
            // invalid LEB128 for name_len
            append_byte(payload, 0x80u);
            return;
        }

        // choose a name
        bool use_name_custom = (prob(eng) < 35); // 35% use "name" to trigger the sequential mapping logic
        u8string name_bytes;
        if(use_name_custom)
        {
            name_bytes.push_back(u8'n');
            name_bytes.push_back(u8'a');
            name_bytes.push_back(u8'm');
            name_bytes.push_back(u8'e');
        }
        else
        {
            int nl = name_len_eng(eng);
            for(int i{}; i < nl; ++i) { append_byte(name_bytes, static_cast<::std::uint8_t>(prob(eng))); }
        }

        if(mode == 2)
        {
            // name_len declared larger than remaining bytes
            ::std::uint32_t const declared = static_cast<::std::uint32_t>(name_bytes.size() + 1u);
            append_uleb128(payload, declared);

            // intentionally write only the remaining bytes
            ::std::uint32_t keep = 0u;
            if(!name_bytes.empty()) { keep = static_cast<::std::uint32_t>(name_bytes.size() - 1u); }
            for(::std::uint32_t i{}; i < keep; ++i) { payload.push_back(name_bytes[i]); }
            return;
        }

        // mode 0: fully valid
        append_uleb128(payload, static_cast<::std::uint32_t>(name_bytes.size()));
        for(auto ch: name_bytes) { payload.push_back(ch); }

        // custom payload bytes
        ::std::uint32_t const data_len = static_cast<::std::uint32_t>(data_len_eng(eng));
        for(::std::uint32_t i{}; i < data_len; ++i) { append_byte(payload, static_cast<::std::uint8_t>(prob(eng))); }
    }
}

int main()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    ::fast_io::io::perr("Starting custom-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    ::fast_io::ibuf_white_hole_engine eng;
    ::std::uniform_int_distribution<int> prob{0, 99};
    ::std::uniform_int_distribution<int> sec_cnt_eng{1, 6};

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

        int sec_cnt = sec_cnt_eng(eng);
        for(int i{}; i < sec_cnt; ++i)
        {
            ::uwvm2::utils::container::u8string payload;

            int mode = 0; // valid
            int p = prob(eng);
            if(p < 5) mode = 1;         // 5% invalid LEB for name_len
            else if(p < 10) mode = 2;   // 5% illegal length

            test::build_one_custom_payload(payload, eng, mode);

            // append custom section (id = 0)
            test::append_section_with_payload(mod, 0u, payload);
        }

        auto const* begin = reinterpret_cast<::std::byte const*>(mod.data());
        auto const* end = begin + mod.size();

        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, err, {});
        }
        catch(::fast_io::error const&)
        {
            // Expected on invalid inputs; keep fuzzing
        }
    }

    ::fast_io::io::perr("Custom-section fuzzing finished.\n");
    
    // Per-function fuzzer: handle_binfmt_ver1_extensible_section_define for custom section
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    for(unsigned round{}; round != 5000u; ++round)
    {
        ::uwvm2::utils::container::u8string payload;
        int mode = prob(eng) < 5 ? 1 : (prob(eng) < 10 ? 2 : 0);
        test::build_one_custom_payload(payload, eng, mode);

        auto const* begin = reinterpret_cast<::std::byte const*>(payload.data());
        auto const* end = begin + payload.size();

        ::uwvm2::parser::wasm::base::error_impl err{};
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> strg{};
        ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t wasm_order{};
        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::handle_binfmt_ver1_extensible_section_define<Feature>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::custom_section_storage_t>{},
                strg,
                begin,
                end,
                err,
                fs_para,
                wasm_order,
                /*sec_id_module_ptr*/ begin);
        }
        catch(...) { }
    }
}


