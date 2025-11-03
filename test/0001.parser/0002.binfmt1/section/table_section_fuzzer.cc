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
#include <vector>
#include <random>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/base/impl.h>
#endif

namespace
{
    using byte = std::byte;

    inline void push_byte(std::vector<byte>& buf, std::uint8_t v) { buf.push_back(static_cast<byte>(v)); }
    inline void push_mem(std::vector<byte>& buf, void const* p, std::size_t n)
    { auto const* b = static_cast<byte const*>(p); buf.insert(buf.end(), b, b + n); }
    inline void push_leb_u32(std::vector<byte>& buf, std::uint32_t v)
    {
        while(true){ std::uint8_t b=static_cast<std::uint8_t>(v & 0x7Fu); v >>= 7u; if(v){ b|=0x80u; push_byte(buf,b);} else { push_byte(buf,b); break; } }
    }
    inline void push_header(std::vector<byte>& out){ push_mem(out, "\0asm", 4); push_mem(out, "\x01\0\0\0", 4); }
    inline void push_section(std::vector<byte>& out, std::uint8_t id, std::vector<byte> const& payload)
    { push_byte(out, id); push_leb_u32(out, static_cast<std::uint32_t>(payload.size())); push_mem(out, payload.data(), payload.size()); }

    // Import section with 0 or 1 imported table (names non-empty)
    inline std::vector<byte> make_import_section_with_optional_table(bool enable, bool with_max)
    {
        std::vector<byte> p; if(!enable){ push_leb_u32(p,0); return p; }
        push_leb_u32(p,1);                        // import_count=1
        push_leb_u32(p,1); push_byte(p,'m');      // module name
        push_leb_u32(p,1); push_byte(p,'t');      // extern  name
        push_byte(p,0x01);                        // kind: table
        // table_type: reftype + limits
        push_byte(p,0x70);                        // funcref
        if(with_max){ push_byte(p,0x01); push_leb_u32(p,1); push_leb_u32(p,2); }
        else         { push_byte(p,0x00); push_leb_u32(p,1); }
        return p;
    }

    inline std::vector<byte> make_table_section_payload(std::mt19937_64& rng)
    {
        std::vector<byte> p;
        std::uniform_int_distribution<int> cntd(0, 2);
        std::uniform_int_distribution<int> reftd(0, 7); // sometimes invalid reftype
        std::uniform_int_distribution<int> flagd(0, 5);

        auto const table_count = static_cast<std::uint32_t>(cntd(rng));
        push_leb_u32(p, table_count);

        for(std::uint32_t i=0;i<table_count;++i)
        {
            // reftype
            int r = reftd(rng);
            std::uint8_t reftype = (r < 6) ? 0x70 : static_cast<std::uint8_t>(0x60 + (rng() & 0x1Fu)); // mostly 0x70, sometimes junk
            push_byte(p, reftype);

            int f = flagd(rng);
            switch(f)
            {
                case 0: // valid min only
                    push_byte(p, 0x00); push_leb_u32(p, 1 + static_cast<std::uint32_t>(rng()%3u)); break;
                case 1: // valid min+max
                {
                    push_byte(p, 0x01);
                    std::uint32_t minv = 1 + static_cast<std::uint32_t>(rng()%3u);
                    std::uint32_t maxv = minv + static_cast<std::uint32_t>(rng()%3u);
                    push_leb_u32(p, minv); push_leb_u32(p, maxv); break;
                }
                case 2: // invalid flag
                    push_byte(p, 0xFF); push_leb_u32(p, 0); break;
                case 3: // min>max
                {
                    push_byte(p, 0x01);
                    std::uint32_t maxv = 1 + static_cast<std::uint32_t>(rng()%2u);
                    std::uint32_t minv = maxv + 1u;
                    push_leb_u32(p, minv); push_leb_u32(p, maxv); break;
                }
                case 4: // degenerate
                    push_byte(p, 0x00); push_leb_u32(p, 0); break;
                default: // odd combos
                    push_byte(p, 0x01); push_leb_u32(p, 0); push_leb_u32(p, 0); break;
            }
        }
        return p;
    }
}

int main()
{
    ::fast_io::io::perr("Starting table-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    std::mt19937_64 rng{std::random_device{}()};

    for(unsigned round{}; round != 50000u; ++round)
    {
        bool include_import = (rng() & 1u) != 0u;
        bool import_with_max = (rng() & 1u) != 0u;

        std::vector<std::byte> mod; mod.reserve(2048);
        push_header(mod);

        auto import_sec = make_import_section_with_optional_table(include_import, import_with_max);
        push_section(mod, 2, import_sec);

        auto table_payload = make_table_section_payload(rng);
        push_section(mod, 4, table_payload);

        try
        {
            using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
            ::uwvm2::parser::wasm::base::error_impl err{};
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
            (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(
                reinterpret_cast<::std::byte const*>(mod.data()),
                reinterpret_cast<::std::byte const*>(mod.data() + mod.size()),
                err,
                fs_para);
        }
        catch(...){ }
    }

    ::fast_io::io::perr("Table-section fuzzing finished.\n");

    // Per-function fuzzer: table_section_table_handler (curr != end ensured)
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    std::uniform_int_distribution<int> flagd(0, 5);
    for(unsigned round{}; round != 5000u; ++round)
    {
        std::vector<std::byte> one;
        // reftype (often valid 0x70, sometimes junk)
        if((rng() & 7u) != 0u) push_byte(one, 0x70);
        else push_byte(one, static_cast<std::uint8_t>(0x20 + (rng() & 0x7Fu)));

        int f = flagd(rng);
        switch(f)
        {
            case 0: // valid min only
                push_byte(one, 0x00); push_leb_u32(one, 1 + static_cast<std::uint32_t>(rng()%3u)); break;
            case 1: // valid min+max
            {
                push_byte(one, 0x01);
                std::uint32_t minv = 1 + static_cast<std::uint32_t>(rng()%3u);
                std::uint32_t maxv = minv + static_cast<std::uint32_t>(rng()%3u);
                push_leb_u32(one, minv); push_leb_u32(one, maxv); break;
            }
            case 2: // invalid flag
                push_byte(one, 0xFF); push_leb_u32(one, 0); break;
            case 3: // min>max
            {
                push_byte(one, 0x01);
                std::uint32_t maxv = 1 + static_cast<std::uint32_t>(rng()%2u);
                std::uint32_t minv = maxv + 1u;
                push_leb_u32(one, minv); push_leb_u32(one, maxv); break;
            }
            default: // degenerate
                push_byte(one, 0x00); push_leb_u32(one, 0); break;
        }

        ::uwvm2::parser::wasm::base::error_impl e{};
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> strg{};

        try
        {
            auto const* b = reinterpret_cast<::std::byte const*>(one.data());
            auto const* eend = b + one.size();

            ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Feature> table{};
            (void)::uwvm2::parser::wasm::standard::wasm1::features::table_section_table_handler<Feature>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Feature>>{},
                table,
                strg,
                b,
                eend,
                e,
                fs_para);
        }
        catch(...) { }
    }

    return 0;
}


