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

static inline void push_byte(std::vector<std::byte>& buf, std::uint8_t v){ buf.push_back(static_cast<std::byte>(v)); }
static inline void push_mem(std::vector<std::byte>& buf, void const* p, std::size_t n){ auto const* b=static_cast<std::byte const*>(p); buf.insert(buf.end(), b, b+n);} 
static inline void push_leb_u32(std::vector<std::byte>& buf, std::uint32_t v){ while(true){ std::uint8_t b=static_cast<std::uint8_t>(v&0x7Fu); v>>=7u; if(v){b|=0x80u; push_byte(buf,b);} else {push_byte(buf,b); break;} } }
static inline void push_header(std::vector<std::byte>& out){ push_mem(out, "\0asm", 4); push_mem(out, "\x01\0\0\0", 4); }
static inline void push_section(std::vector<std::byte>& out, std::uint8_t id, std::vector<std::byte> const& payload){ push_byte(out,id); push_leb_u32(out, static_cast<std::uint32_t>(payload.size())); push_mem(out, payload.data(), payload.size()); }

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    std::vector<std::byte> mod; mod.reserve(64 + size + 16);
    push_header(mod);

    // Use fuzzer input as Type section payload directly
    std::vector<std::byte> type_payload;
    type_payload.insert(type_payload.end(), reinterpret_cast<std::byte const*>(data), reinterpret_cast<std::byte const*>(data)+size);
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
    catch(...){ }

    return 0;
}


