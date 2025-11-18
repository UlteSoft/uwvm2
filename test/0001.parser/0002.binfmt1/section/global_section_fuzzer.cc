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

    // Import section with 0 or 1 imported global (names are non-empty)
    inline std::vector<byte> make_import_section_with_optional_global(bool enable, bool mutable_global, std::uint8_t valtype)
    {
        std::vector<byte> p;
        if(!enable)
        {
            push_leb_u32(p, 0); // import_count=0
            return p;
        }

        push_leb_u32(p, 1);               // import_count=1
        push_leb_u32(p, 1); push_byte(p, 'm'); // module name len=1, "m"
        push_leb_u32(p, 1); push_byte(p, 'g'); // extern  name len=1, "g"
        push_byte(p, 0x03);               // kind: global
        // global type: valtype + mut
        push_byte(p, valtype);             // 0x7F/0x7E/0x7D/0x7C
        push_byte(p, mutable_global ? 0x01 : 0x00);
        return p;
    }

    inline std::vector<byte> make_global_section_payload(std::mt19937_64& rng, bool has_imported_global, std::uint8_t imported_valtype, bool imported_mut)
    {
        (void)imported_valtype;
        (void)imported_mut;
        std::vector<byte> p;
        std::uniform_int_distribution<int> cntd(1, 3);
        std::uniform_int_distribution<int> kindd(0, 5); // choose expr kind
        std::uniform_int_distribution<int> vtd(0, 3);

        static constexpr std::uint8_t valtypes[4]{0x7F, 0x7E, 0x7D, 0x7C}; // i32,i64,f32,f64

        auto const global_count = static_cast<std::uint32_t>(cntd(rng));
        push_leb_u32(p, global_count);

        for(std::uint32_t i = 0; i < global_count; ++i)
        {
            // choose declared type and mutability
            std::uint8_t decl_vt = valtypes[vtd(rng)];
            bool decl_mut = (rng() & 3u) == 0u; // mostly immutable
            push_byte(p, decl_vt);
            push_byte(p, decl_mut ? 0x01 : 0x00);

            int mode = kindd(rng);
            switch(mode)
            {
                case 0: // valid const matching type
                {
                    if(decl_vt == 0x7F) { push_byte(p, 0x41); push_leb_u32(p, 0); }
                    else if(decl_vt == 0x7E) { push_byte(p, 0x42); push_leb_u32(p, 0); }
                    else if(decl_vt == 0x7D)
                    {
                        push_byte(p, 0x43);
                        // f32: write 4 bytes little-endian zeros
                        push_mem(p, "\0\0\0\0", 4);
                    }
                    else // 0x7C
                    {
                        push_byte(p, 0x44);
                        // f64: write 8 bytes little-endian zeros
                        push_mem(p, "\0\0\0\0\0\0\0\0", 8);
                    }
                    push_byte(p, 0x0B); // end
                    break;
                }
                case 1: // missing end (trigger init_const_expr_terminator_not_found)
                {
                    push_byte(p, 0x41); push_leb_u32(p, 0);
                    break;
                }
                case 2: // illegal instruction
                {
                    push_byte(p, 0x6A); // i32.add not allowed in const expr
                    push_byte(p, 0x0B);
                    break;
                }
                case 3: // type mismatch: declare i64 but emit i32.const, etc.
                {
                    push_byte(p, 0x41); push_leb_u32(p, 1); // i32.const 1
                    push_byte(p, 0x0B);
                    break;
                }
                case 4: // reference imported global
                {
                    push_byte(p, 0x23); // global.get
                    // choose idx
                    std::uint32_t idx = 0;
                    if(!has_imported_global || (rng() & 3u) == 0u) idx = 1; // often OOB
                    push_leb_u32(p, idx);
                    push_byte(p, 0x0B);
                    break;
                }
                default: // corrupt data payload size for f32/f64
                {
                    if(decl_vt == 0x7D) { push_byte(p, 0x43); push_mem(p, "\0\0\0", 3); push_byte(p, 0x0B); }
                    else if(decl_vt == 0x7C) { push_byte(p, 0x44); push_mem(p, "\0\0\0\0\0\0\0", 7); push_byte(p, 0x0B); }
                    else { push_byte(p, 0x41); push_leb_u32(p, 0); push_byte(p, 0x0B); }
                    break;
                }
            }
        }

        return p;
    }
}

int main()
{
    ::fast_io::io::perr("Starting global-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    std::mt19937_64 rng{std::random_device{}()};

    for(unsigned round{}; round != 50000u; ++round)
    {
        bool include_import = (rng() & 1u) != 0u;
        bool import_mut = (rng() & 7u) == 0u; // rarely mutable to trigger ref_mutable_imported_global
        static constexpr std::uint8_t import_vt_candidates[4]{0x7F,0x7E,0x7D,0x7C};
        std::uint8_t import_vt = import_vt_candidates[(rng() >> 3) & 3u];

        // Build module
        std::vector<std::byte> mod;
        mod.reserve(2048);
        push_header(mod);

        // Optional import
        auto import_sec = make_import_section_with_optional_global(include_import, import_mut, import_vt);
        push_section(mod, 2, import_sec);

        // Global section
        auto global_payload = make_global_section_payload(rng, include_import && !import_mut, import_vt, import_mut);
        push_section(mod, 6, global_payload);

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

    ::fast_io::io::perr("Global-section fuzzing finished.\n");
    
    // Per-function fuzzer: global_section_global_handler + parse_and_check_global_expr_valid
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    for(unsigned round{}; round != 5000u; ++round)
    {
        std::vector<std::byte> buf;
        // Global type: i32 + immutable
        push_byte(buf, 0x7F); // i32
        push_byte(buf, 0x00); // const
        // init expr: i32.const 0; end
        push_byte(buf, 0x41);
        push_leb_u32(buf, 0u);
        push_byte(buf, 0x0B);

        auto const* begin = reinterpret_cast<::std::byte const*>(buf.data());
        auto const* end = begin + buf.size();

        ::uwvm2::parser::wasm::base::error_impl err{};
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> strg{};

        try
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Feature> lg{};
            auto const* after_type = ::uwvm2::parser::wasm::standard::wasm1::features::global_section_global_handler<Feature>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Feature>>{},
                lg.global,
                strg,
                begin,
                end,
                err,
                fs_para);

            (void)::uwvm2::parser::wasm::standard::wasm1::features::parse_and_check_global_expr_valid<Feature>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Feature>>{},
                lg.global,
                lg.expr,
                strg,
                after_type,
                end,
                err,
                fs_para);
        }
        catch(...) { }
    }

    return 0;
}


