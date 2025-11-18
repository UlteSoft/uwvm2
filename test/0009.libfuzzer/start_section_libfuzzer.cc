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

// types: 0: ()->(), 1: (i32)->()
static inline std::vector<std::byte> type_sec_fixed()
{ std::vector<std::byte> p; push_leb_u32(p,2); push_byte(p,0x60); push_leb_u32(p,0); push_leb_u32(p,0); push_byte(p,0x60); push_leb_u32(p,1); push_byte(p,0x7F); push_leb_u32(p,0); return p; }

static inline std::vector<std::byte> import_func_mode(std::uint8_t mode)
{
    // 0: none; 1: import func type0; 2: import func type1
    std::vector<std::byte> p; if(mode==0){ push_leb_u32(p,0); return p; }
    push_leb_u32(p,1); push_leb_u32(p,1); push_byte(p,'m'); push_leb_u32(p,1); push_byte(p,'f'); push_byte(p,0x00); push_leb_u32(p, mode==2 ? 1u : 0u); return p;
}

static inline std::vector<std::byte> function_sec_mode(std::uint8_t mode)
{ std::vector<std::byte> p; if(mode==0){ push_leb_u32(p,0); } else { push_leb_u32(p,1); push_leb_u32(p, mode==2 ? 1u : 0u);} return p; }

static inline std::vector<std::byte> code_sec_if_defined(std::uint8_t mode)
{ std::vector<std::byte> p; if(mode==0){ push_leb_u32(p,0);} else { push_leb_u32(p,1); std::vector<std::byte> body; push_leb_u32(body,0); push_byte(body,0x0B); push_leb_u32(p, static_cast<std::uint32_t>(body.size())); push_mem(p, body.data(), body.size()); } return p; }

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    std::uint8_t import_mode = size>0 ? (data[0] % 3u) : 0u;      // 0 none, 1 type0, 2 type1
    std::uint8_t func_mode = size>1 ? (data[1] % 3u) : 1u;        // 0 none, 1 type0, 2 type1
    bool extra = size>2 ? ((data[2] & 1u)!=0u) : false;
    auto const* start_payload = (size>3) ? (data+3) : data;
    std::size_t start_payload_size = (size>3) ? (size-3) : 0u;

    std::vector<std::byte> mod; mod.reserve(256 + start_payload_size);
    push_header(mod);
    push_section(mod, 1, type_sec_fixed());
    push_section(mod, 2, import_func_mode(import_mode));
    push_section(mod, 3, function_sec_mode(func_mode));
    push_section(mod, 10, code_sec_if_defined(func_mode));

    // Start section payload from fuzzer (can be invalid LEB or with trailing extras)
    std::vector<std::byte> start_payload_vec;
    if(start_payload_size==0)
    {
        // ensure at least 1 byte to exercise path; choose 0 or OOB
        start_payload_vec.push_back(static_cast<std::byte>( (import_mode||func_mode)? 0u : 0xFFu));
        if(extra) start_payload_vec.push_back(static_cast<std::byte>(0x00));
    }
    else
    {
        start_payload_vec.insert(start_payload_vec.end(), reinterpret_cast<std::byte const*>(start_payload), reinterpret_cast<std::byte const*>(start_payload)+start_payload_size);
        if(extra) start_payload_vec.push_back(static_cast<std::byte>(0x00));
    }
    push_section(mod, 8, start_payload_vec);

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


