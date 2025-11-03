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
        while(true){ std::uint8_t b=static_cast<std::uint8_t>(v&0x7Fu); v>>=7u; if(v){ b|=0x80u; push_byte(buf,b);} else {push_byte(buf,b); break;} }
    }
    inline void push_header(std::vector<byte>& out){ push_mem(out, "\0asm", 4); push_mem(out, "\x01\0\0\0", 4); }
    inline void push_section(std::vector<byte>& out, std::uint8_t id, std::vector<byte> const& payload)
    { push_byte(out, id); push_leb_u32(out, static_cast<std::uint32_t>(payload.size())); push_mem(out, payload.data(), payload.size()); }

    // Build types: type0: ()->(), type1: (i32)->() or ()->(i32)
    inline std::vector<byte> make_type_section(bool mismatch_alt)
    {
        std::vector<byte> p; push_leb_u32(p, 2);
        // type 0: ()->()
        push_byte(p, 0x60); push_leb_u32(p, 0); push_leb_u32(p, 0);
        // type 1: mismatch
        push_byte(p, 0x60);
        if(mismatch_alt) { push_leb_u32(p, 1); push_byte(p, 0x7F); push_leb_u32(p, 0); }
        else { push_leb_u32(p, 0); push_leb_u32(p, 1); push_byte(p, 0x7F); }
        return p;
    }

    // Import section: optionally import 1 function with typeidx 0 or 1 (names non-empty)
    inline std::vector<byte> make_import_function(bool enable, bool type_mismatch)
    {
        std::vector<byte> p; if(!enable){ push_leb_u32(p,0); return p; }
        push_leb_u32(p,1);
        push_leb_u32(p,1); push_byte(p,'m');
        push_leb_u32(p,1); push_byte(p,'f');
        push_byte(p,0x00); // kind func
        push_leb_u32(p, type_mismatch ? 1u : 0u);
        return p;
    }

    // Function section: optionally define 1 function with typeidx 0 or 1
    inline std::vector<byte> make_function_section(bool enable, bool type_mismatch)
    {
        std::vector<byte> p; push_leb_u32(p, enable ? 1u : 0u); if(enable){ push_leb_u32(p, type_mismatch ? 1u : 0u);} return p;
    }

    // Code section for one empty function
    inline std::vector<byte> make_code_section(bool enable)
    {
        std::vector<byte> p; push_leb_u32(p, enable ? 1u : 0u); if(enable){ std::vector<byte> body; push_leb_u32(body,0); push_byte(body,0x0B); push_leb_u32(p, static_cast<std::uint32_t>(body.size())); push_mem(p, body.data(), body.size()); } return p;
    }

    inline std::vector<byte> make_start_section(std::uint32_t start_idx, bool with_extra)
    {
        std::vector<byte> p; push_leb_u32(p, start_idx); if(with_extra) push_byte(p, 0x00); return p;
    }
}

int main()
{
    ::fast_io::io::perr("Starting start-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    std::mt19937_64 rng{std::random_device{}()};

    for(unsigned round{}; round != 50000u; ++round)
    {
        // Decide composition
        bool have_import = (rng() & 1u) != 0u;
        bool have_defined = (rng() & 1u) != 0u;
        bool import_mismatch = (rng() & 3u) == 0u;   // sometimes mismatch
        bool defined_mismatch = (rng() & 3u) == 1u;  // sometimes mismatch
        bool extra_byte = (rng() & 7u) == 0u;

        // Build module
        std::vector<std::byte> mod; mod.reserve(1024);
        push_header(mod);
        push_section(mod, 1, make_type_section((rng() & 1u) != 0u));
        auto import_sec = make_import_function(have_import, import_mismatch);
        push_section(mod, 2, import_sec);
        push_section(mod, 3, make_function_section(have_defined, defined_mismatch));
        push_section(mod, 10, make_code_section(have_defined));

        // Choose start_idx
        std::uint32_t imported_funcs = have_import ? 1u : 0u;
        std::uint32_t defined_funcs = have_defined ? 1u : 0u;
        std::uint32_t all_funcs = imported_funcs + defined_funcs;
        std::uint32_t start_idx = 0u;
        if(all_funcs == 0u) start_idx = 0x7FFFFFFFu; // ensure OOB
        else
        {
            switch(rng() % 3u)
            {
                case 0: start_idx = 0u; break;                                 // valid path
                case 1: start_idx = all_funcs - 1u; break;                      // last valid
                default: start_idx = all_funcs + static_cast<std::uint32_t>(rng() & 0xFFu); // OOB
            }
        }
        push_section(mod, 8, make_start_section(start_idx, extra_byte));

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

    ::fast_io::io::perr("Start-section fuzzing finished.\n");
    return 0;
}


