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
            if(v)
            {
                b |= 0x80u;
                push_byte(buf, b);
            }
            else
            {
                push_byte(buf, b);
                break;
            }
        }
    }

    inline void push_header(std::vector<byte>& out)
    {
        // \0asm + version 1
        push_mem(out, "\0asm", 4);
        push_mem(out, "\x01\0\0\0", 4);
    }

    inline void push_section(std::vector<byte>& out, std::uint8_t id, std::vector<byte> const& payload)
    {
        push_byte(out, id);
        push_leb_u32(out, static_cast<std::uint32_t>(payload.size()));
        push_mem(out, payload.data(), payload.size());
    }

    // type section: 1 func type ()->()
    inline std::vector<byte> make_type_section()
    {
        std::vector<byte> p;
        // count
        push_leb_u32(p, 1);
        // func type 0x60
        push_byte(p, 0x60);
        // param_count=0
        push_leb_u32(p, 0);
        // result_count=0
        push_leb_u32(p, 0);
        return p;
    }

    // function section: 1 function referencing type 0
    inline std::vector<byte> make_function_section()
    {
        std::vector<byte> p;
        push_leb_u32(p, 1);
        push_leb_u32(p, 0);  // typeidx 0
        return p;
    }

    // table section: 1 funcref table min=1
    inline std::vector<byte> make_table_section()
    {
        std::vector<byte> p;
        push_leb_u32(p, 1);  // table count
        push_byte(p, 0x70);  // reftype funcref
        push_byte(p, 0x00);  // limits flags: min only
        push_leb_u32(p, 1);  // min = 1
        return p;
    }

    // memory section: 1 memory min=1
    inline std::vector<byte> make_memory_section()
    {
        std::vector<byte> p;
        push_leb_u32(p, 1);  // memory count
        push_byte(p, 0x00);  // limits flags: min only
        push_leb_u32(p, 1);  // min = 1 page
        return p;
    }

    // global section: 1 immutable i32 with const 0 init
    inline std::vector<byte> make_global_section()
    {
        std::vector<byte> p;
        push_leb_u32(p, 1);  // global count
        push_byte(p, 0x7F);  // valtype i32
        push_byte(p, 0x00);  // mut const
        // init expr: i32.const 0; end
        push_byte(p, 0x41);  // i32.const
        push_leb_u32(p, 0);
        push_byte(p, 0x0B);  // end
        return p;
    }

    // export section payload generator
    inline std::vector<byte> make_export_section_payload(std::mt19937_64& rng)
    {
        std::vector<byte> p;
        std::uniform_int_distribution<int> cntd(1, 4);
        std::uniform_int_distribution<int> kindd(0, 3);  // func/table/mem/global
        std::uniform_int_distribution<int> namelen_d(0, 8);
        std::bernoulli_distribution valid_idx_b(0.7);

        auto const export_cnt = static_cast<std::uint32_t>(cntd(rng));
        push_leb_u32(p, export_cnt);

        for(std::uint32_t i = 0; i < export_cnt; ++i)
        {
            // name
            auto const nl = static_cast<std::uint32_t>(namelen_d(rng));
            push_leb_u32(p, nl);
            for(std::uint32_t j = 0; j < nl; ++j)
            {
                // mostly valid ASCII, sometimes high bytes to challenge UTF-8
                std::uint8_t ch = static_cast<std::uint8_t>((rng() % 10) ? (33 + (rng() % 90)) : (0xC0 | (rng() & 0x3F)));
                push_byte(p, ch);
            }

            // kind
            std::uint8_t kind = static_cast<std::uint8_t>(kindd(rng));
            // sometimes generate illegal kind to hit illegal_exportdesc_prefix
            if((rng() & 15u) == 0u) { kind = static_cast<std::uint8_t>(4 + (rng() % 251)); }
            push_byte(p, kind);

            // idx
            std::uint32_t idx = 0;
            if(valid_idx_b(rng))
            {
                idx = 0;  // within range, as we define 1 of each
            }
            else
            {
                idx = 2 + static_cast<std::uint32_t>(rng() & 0xFFu);  // out of range
            }
            push_leb_u32(p, idx);
        }

        return p;
    }
}  // namespace

int main()
{
    ::fast_io::io::perr("Starting export-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    std::mt19937_64 rng{std::random_device{}()};

    for(unsigned round{}; round != 10000u; ++round)
    {
        // Build module
        std::vector<std::byte> mod;
        mod.reserve(1024);
        push_header(mod);

        // Type
        auto type_sec = make_type_section();
        push_section(mod, 1, type_sec);

        // Import: empty (id=2, payload=0 count)
        std::vector<std::byte> import_payload;
        push_leb_u32(import_payload, 0);
        push_section(mod, 2, import_payload);

        // Function (1)
        auto func_sec = make_function_section();
        push_section(mod, 3, func_sec);

        // Table (1)
        auto tab_sec = make_table_section();
        push_section(mod, 4, tab_sec);

        // Memory (1)
        auto mem_sec = make_memory_section();
        push_section(mod, 5, mem_sec);

        // Global (1)
        auto glo_sec = make_global_section();
        push_section(mod, 6, glo_sec);

        // Export (randomized)
        auto exp_payload = make_export_section_payload(rng);
        push_section(mod, 7, exp_payload);

        // Parse
        try
        {
            using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
            ::uwvm2::parser::wasm::base::error_impl err{};
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};

            (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(reinterpret_cast<::std::byte const*>(mod.data()),
                                                                                             reinterpret_cast<::std::byte const*>(mod.data() + mod.size()),
                                                                                             err,
                                                                                             fs_para);
        }
        catch(...)
        {
            // swallow parse exceptions for fuzzer
        }
    }

    ::fast_io::io::perr("Export-section fuzzing finished.\n");
    return 0;
}

