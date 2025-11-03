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

    // Import section with optional imported memory (names non-empty)
    inline std::vector<byte> make_import_section_with_optional_memory(bool enable, bool with_max)
    {
        std::vector<byte> p;
        if(!enable)
        {
            push_leb_u32(p, 0); // import_count=0
            return p;
        }
        push_leb_u32(p, 1); // one import
        // module name "m"
        push_leb_u32(p, 1); push_byte(p, 'm');
        // extern name "mem"
        push_leb_u32(p, 3); push_byte(p, 'm'); push_byte(p, 'e'); push_byte(p, 'm');
        // kind: memory
        push_byte(p, 0x02);
        // memory_type
        if(with_max)
        {
            push_byte(p, 0x01); // min+max
            push_leb_u32(p, 1); // min
            push_leb_u32(p, 2); // max
        }
        else
        {
            push_byte(p, 0x00); // min only
            push_leb_u32(p, 1); // min
        }
        return p;
    }

    inline std::vector<byte> make_memory_section_payload(std::mt19937_64& rng, std::uint32_t imported_memories)
    {
        std::vector<byte> p;
        std::uniform_int_distribution<int> cntd(0, 2);
        std::uniform_int_distribution<int> flagd(0, 5);

        auto const mem_count = static_cast<std::uint32_t>(cntd(rng));
        push_leb_u32(p, mem_count);

        for(std::uint32_t i = 0; i < mem_count; ++i)
        {
            int f = flagd(rng);
            switch(f)
            {
                case 0: // valid: min only
                    push_byte(p, 0x00);
                    push_leb_u32(p, 1 + static_cast<std::uint32_t>(rng() % 3u));
                    break;
                case 1: // valid: min<=max
                {
                    push_byte(p, 0x01);
                    std::uint32_t minv = 1 + static_cast<std::uint32_t>(rng() % 3u);
                    std::uint32_t maxv = minv + static_cast<std::uint32_t>(rng() % 3u);
                    push_leb_u32(p, minv);
                    push_leb_u32(p, maxv);
                    break;
                }
                case 2: // invalid flag
                    push_byte(p, 0xFF);
                    push_leb_u32(p, 0);
                    break;
                case 3: // min>max (illegal)
                {
                    push_byte(p, 0x01);
                    std::uint32_t maxv = 1 + static_cast<std::uint32_t>(rng() % 2u);
                    std::uint32_t minv = maxv + 1u;
                    push_leb_u32(p, minv);
                    push_leb_u32(p, maxv);
                    break;
                }
                case 4: // shared-like 0x03 (likely illegal without threads)
                {
                    push_byte(p, 0x03);
                    push_leb_u32(p, 1);
                    push_leb_u32(p, 2);
                    break;
                }
                default: // degenerate small numbers
                    push_byte(p, 0x00);
                    push_leb_u32(p, 0);
                    break;
            }
        }

        // Occasionally force multi-memory violation: if no import and mem_count==2, keep; if import_count==1 and mem_count==1, keep
        (void)imported_memories;
        return p;
    }
}

int main()
{
    ::fast_io::io::perr("Starting memory-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    std::mt19937_64 rng{std::random_device{}()};

    for(unsigned round{}; round != 50000u; ++round)
    {
        bool include_import = (rng() & 1u) != 0u;
        bool import_with_max = (rng() & 1u) != 0u;

        // Build module
        std::vector<std::byte> mod;
        mod.reserve(2048);
        push_header(mod);

        // Import
        auto import_sec = make_import_section_with_optional_memory(include_import, import_with_max);
        push_section(mod, 2, import_sec);

        // Memory section
        std::uint32_t imported_memories = include_import ? 1u : 0u;
        auto memory_payload = make_memory_section_payload(rng, imported_memories);
        push_section(mod, 5, memory_payload);

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

    ::fast_io::io::perr("Memory-section fuzzing finished.\n");
    return 0;
}


