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
    {
        auto const* b = static_cast<byte const*>(p);
        buf.insert(buf.end(), b, b + n);
    }

    inline void push_leb_u32(std::vector<byte>& buf, std::uint32_t v)
    {
        while(true)
        {
            std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
            v >>= 7u;
            if(v) { b |= 0x80u; push_byte(buf, b); }
            else { push_byte(buf, b); break; }
        }
    }

    inline void push_header(std::vector<byte>& out)
    {
        push_mem(out, "\0asm", 4);
        push_mem(out, "\x01\0\0\0", 4);
    }

    inline void push_section(std::vector<byte>& out, std::uint8_t id, std::vector<byte> const& payload)
    {
        push_byte(out, id);
        push_leb_u32(out, static_cast<std::uint32_t>(payload.size()));
        push_mem(out, payload.data(), payload.size());
    }

    inline std::vector<byte> make_type_entry_valid(std::mt19937_64& rng)
    {
        std::vector<byte> p;
        // prefix 0x60
        push_byte(p, 0x60);
        // params
        std::uint32_t pc = static_cast<std::uint32_t>(rng() % 4u);
        push_leb_u32(p, pc);
        static constexpr std::uint8_t vts[4]{0x7F,0x7E,0x7D,0x7C};
        for(std::uint32_t i=0;i<pc;++i) push_byte(p, vts[rng() & 3u]);
        // results: 0..1 per MVP
        std::uint32_t rc = static_cast<std::uint32_t>(rng() & 1u);
        push_leb_u32(p, rc);
        for(std::uint32_t i=0;i<rc;++i) push_byte(p, vts[rng() & 3u]);
        return p;
    }

    inline std::vector<byte> make_type_entry_illegal_value_type(std::mt19937_64& rng)
    {
        (void)rng;
        std::vector<byte> p; push_byte(p, 0x60); push_leb_u32(p, 1); push_byte(p, 0xFF); push_leb_u32(p, 0); return p;
    }

    inline std::vector<byte> make_type_entry_multi_results(std::mt19937_64& rng)
    {
        (void)rng;
        std::vector<byte> p; push_byte(p, 0x60); push_leb_u32(p, 0); push_leb_u32(p, 2); push_byte(p, 0x7F); push_byte(p, 0x7F); return p;
    }

    inline std::vector<byte> make_type_entry_illegal_prefix(std::mt19937_64& rng)
    {
        (void)rng;
        std::vector<byte> p; push_byte(p, 0x61); return p; // bad prefix
    }

    inline std::vector<byte> make_type_entry_invalid_param_len_leb(std::mt19937_64& rng)
    {
        (void)rng;
        std::vector<byte> p; push_byte(p, 0x60); p.push_back(static_cast<byte>(0x80)); return p; // unterminated leb128
    }

    inline std::vector<byte> make_type_section_payload(std::mt19937_64& rng)
    {
        std::vector<byte> p;
        std::uint32_t type_count = 1u + static_cast<std::uint32_t>(rng() % 4u);
        push_leb_u32(p, type_count);
        for(std::uint32_t i=0;i<type_count;++i)
        {
            switch(rng() % 6u)
            {
                case 0: { auto e = make_type_entry_valid(rng); p.insert(p.end(), e.begin(), e.end()); break; }
                case 1: { auto e = make_type_entry_illegal_value_type(rng); p.insert(p.end(), e.begin(), e.end()); break; }
                case 2: { auto e = make_type_entry_multi_results(rng); p.insert(p.end(), e.begin(), e.end()); break; }
                case 3: { auto e = make_type_entry_illegal_prefix(rng); p.insert(p.end(), e.begin(), e.end()); break; }
                case 4: { auto e = make_type_entry_invalid_param_len_leb(rng); p.insert(p.end(), e.begin(), e.end()); break; }
                default:{ auto e = make_type_entry_valid(rng); p.insert(p.end(), e.begin(), e.end()); break; }
            }
        }
        return p;
    }
}

int main()
{
    ::fast_io::io::perr("Starting type-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    std::mt19937_64 rng{std::random_device{}()};

    for(unsigned round{}; round != 50000u; ++round)
    {
        // Build module
        std::vector<std::byte> mod; mod.reserve(2048);
        push_header(mod);

        auto type_payload = make_type_section_payload(rng);
        push_section(mod, 1, type_payload);

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
        catch(...)
        {
            // swallow
        }
    }

    ::fast_io::io::perr("Type-section fuzzing finished.\n");
    return 0;
}


