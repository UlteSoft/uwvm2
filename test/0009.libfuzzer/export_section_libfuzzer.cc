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
#include <vector>

#ifndef UWVM_MODULE
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/base/impl.h>
#endif

inline static void push_byte(std::vector<std::byte>& buf, std::uint8_t v) { buf.push_back(static_cast<std::byte>(v)); }

inline static void push_mem(std::vector<std::byte>& buf, void const* p, std::size_t n)
{
    auto const* b = static_cast<std::byte const*>(p);
    buf.insert(buf.end(), b, b + n);
}

inline static void push_leb_u32(std::vector<std::byte>& buf, std::uint32_t v)
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

inline static void push_header(std::vector<std::byte>& out)
{
    push_mem(out, "\0asm", 4);
    push_mem(out, "\x01\0\0\0", 4);
}

inline static void push_section(std::vector<std::byte>& out, std::uint8_t id, std::vector<std::byte> const& payload)
{
    push_byte(out, id);
    push_leb_u32(out, static_cast<std::uint32_t>(payload.size()));
    push_mem(out, payload.data(), payload.size());
}

inline static std::vector<std::byte> make_type_section()
{
    std::vector<std::byte> p;
    push_leb_u32(p, 1);
    push_byte(p, 0x60);
    push_leb_u32(p, 0);
    push_leb_u32(p, 0);
    return p;
}

inline static std::vector<std::byte> make_function_section()
{
    std::vector<std::byte> p;
    push_leb_u32(p, 1);
    push_leb_u32(p, 0);
    return p;
}

inline static std::vector<std::byte> make_table_section()
{
    std::vector<std::byte> p;
    push_leb_u32(p, 1);
    push_byte(p, 0x70);
    push_byte(p, 0x00);
    push_leb_u32(p, 1);
    return p;
}

inline static std::vector<std::byte> make_memory_section()
{
    std::vector<std::byte> p;
    push_leb_u32(p, 1);
    push_byte(p, 0x00);
    push_leb_u32(p, 1);
    return p;
}

inline static std::vector<std::byte> make_global_section()
{
    std::vector<std::byte> p;
    push_leb_u32(p, 1);
    push_byte(p, 0x7F);
    push_byte(p, 0x00);
    push_byte(p, 0x41);
    push_leb_u32(p, 0);
    push_byte(p, 0x0B);
    return p;
}

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    // Build minimal module where Export section payload is fuzzer-controlled
    std::vector<std::byte> mod;
    mod.reserve(16 + size + 128);
    push_header(mod);

    // Type, Import(empty), Function(1), Table(1), Memory(1), Global(1)
    push_section(mod, 1, make_type_section());
    std::vector<std::byte> import_payload;
    push_leb_u32(import_payload, 0);
    push_section(mod, 2, import_payload);
    push_section(mod, 3, make_function_section());
    push_section(mod, 4, make_table_section());
    push_section(mod, 5, make_memory_section());
    push_section(mod, 6, make_global_section());

    // Export section: use fuzz bytes directly
    std::vector<std::byte> export_payload;
    export_payload.insert(export_payload.end(), reinterpret_cast<std::byte const*>(data), reinterpret_cast<std::byte const*>(data) + size);
    push_section(mod, 7, export_payload);

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
        // swallow parse exceptions for fuzzing
    }

    return 0;
}

