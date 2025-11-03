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

static inline void push_byte(std::vector<std::byte>& buf, std::uint8_t v) { buf.push_back(static_cast<std::byte>(v)); }

static inline void push_mem(std::vector<std::byte>& buf, void const* p, std::size_t n)
{
    auto const* b = static_cast<std::byte const*>(p);
    buf.insert(buf.end(), b, b + n);
}

static inline void push_leb_u32(std::vector<std::byte>& buf, std::uint32_t v)
{
    while(true)
    {
        std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
        v >>= 7u;
        if(v) { b |= 0x80u; push_byte(buf, b); }
        else { push_byte(buf, b); break; }
    }
}

static inline void push_header(std::vector<std::byte>& out)
{
    push_mem(out, "\0asm", 4);
    push_mem(out, "\x01\0\0\0", 4);
}

static inline void push_section(std::vector<std::byte>& out, std::uint8_t id, std::vector<std::byte> const& payload)
{
    push_byte(out, id);
    push_leb_u32(out, static_cast<std::uint32_t>(payload.size()));
    push_mem(out, payload.data(), payload.size());
}

static inline std::vector<std::byte> make_import_section_for_mode(std::uint8_t mode)
{
    // 0: no import, 1: import memory (min only), 2: import memory (min+max)
    std::vector<std::byte> p;
    if(mode == 0) { push_leb_u32(p, 0); return p; }
    push_leb_u32(p, 1);
    // module name "m"
    push_leb_u32(p, 1); push_byte(p, 'm');
    // extern name "mem"
    push_leb_u32(p, 3); push_byte(p, 'm'); push_byte(p, 'e'); push_byte(p, 'm');
    // kind: memory
    push_byte(p, 0x02);
    if(mode == 2)
    {
        push_byte(p, 0x01); // min+max
        push_leb_u32(p, 1);
        push_leb_u32(p, 2);
    }
    else
    {
        push_byte(p, 0x00); // min only
        push_leb_u32(p, 1);
    }
    return p;
}

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    std::uint8_t mode = size ? (data[0] % 3u) : 0u;
    auto const* payload = (size > 0) ? (data + 1) : data;
    std::size_t payload_size = (size > 0) ? (size - 1) : 0u;

    std::vector<std::byte> mod; mod.reserve(64 + payload_size + 64);
    push_header(mod);

    // Import
    push_section(mod, 2, make_import_section_for_mode(mode));

    // Memory: payload from fuzzer
    std::vector<std::byte> memory_payload;
    memory_payload.insert(memory_payload.end(), reinterpret_cast<std::byte const*>(payload), reinterpret_cast<std::byte const*>(payload) + payload_size);
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

    return 0;
}


