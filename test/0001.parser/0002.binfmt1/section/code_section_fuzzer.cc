/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 * @date        2025-10-31
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
#include <random>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/value_binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    using ::uwvm2::utils::container::u8string;

    inline void append_byte(u8string& out, ::std::uint8_t b) noexcept { out.push_back(static_cast<char8_t>(b)); }

    inline void append_bytes(u8string& out, char const* p, ::std::size_t n) noexcept
    {
        for(::std::size_t i{}; i < n; ++i) { out.push_back(static_cast<char8_t>(static_cast<::std::uint8_t>(p[i]))); }
    }

    // Minimal uleb128 encoder with optional redundant continuation zeros
    inline void append_uleb128(u8string& out, ::std::uint32_t value, bool add_redundant = false, ::std::uint32_t redundant_count = 1u) noexcept
    {
        ::std::uint32_t v = value;
        do
        {
            ::std::uint8_t byte = static_cast<::std::uint8_t>(v & 0x7Fu);
            v >>= 7u;
            if(v != 0u) { byte = static_cast<::std::uint8_t>(byte | 0x80u); }
            append_byte(out, byte);
        }
        while(v != 0u);

        if(add_redundant && redundant_count != 0u)
        {
            auto const n = out.size();
            if(n != 0u)
            {
                out[n - 1u] = static_cast<char8_t>(static_cast<::std::uint8_t>(out[n - 1u]) | static_cast<::std::uint8_t>(0x80u));
                for(::std::uint32_t i{}; i + 1u < redundant_count; ++i) { append_byte(out, 0x80u); }
                append_byte(out, 0x00u);
            }
        }
    }

    inline void append_section_with_payload(u8string& mod, ::std::uint8_t sec_id, u8string const& payload) noexcept
    {
        append_byte(mod, sec_id);
        // section length as uleb128
        append_uleb128(mod, static_cast<::std::uint32_t>(payload.size()));
        // payload
        for(auto ch: payload) { mod.push_back(ch); }
    }

    // Build a minimal type section: N function types; each: 0x60, vec(param), vec(result<=1)
    inline void build_type_section(u8string& out, ::std::uint32_t type_cnt, ::fast_io::ibuf_white_hole_engine& eng) noexcept
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm1::type;
        u8string payload;
        append_uleb128(payload, type_cnt);
        ::std::uniform_int_distribution<int> choice{0, 99};
        for(::std::uint32_t i{}; i < type_cnt; ++i)
        {
            // functype prefix
            append_byte(payload, static_cast<::std::uint8_t>(function_type_prefix::functype));
            // params
            ::std::uint32_t const param_n = static_cast<::std::uint32_t>(choice(eng) % 3); // 0..2
            append_uleb128(payload, param_n);
            for(::std::uint32_t p{}; p < param_n; ++p)
            {
                // random valid valtype
                ::std::uint8_t vt = static_cast<::std::uint8_t>(value_type::i32);
                switch(choice(eng) % 4)
                {
                    case 0: vt = static_cast<::std::uint8_t>(value_type::i32); break;
                    case 1: vt = static_cast<::std::uint8_t>(value_type::i64); break;
                    case 2: vt = static_cast<::std::uint8_t>(value_type::f32); break;
                    default: vt = static_cast<::std::uint8_t>(value_type::f64); break;
                }
                append_byte(payload, vt);
            }
            // results (MVP: 0 or 1)
            ::std::uint32_t const res_n = static_cast<::std::uint32_t>(choice(eng) % 2); // 0..1
            append_uleb128(payload, res_n);
            if(res_n)
            {
                ::std::uint8_t vt = static_cast<::std::uint8_t>(value_type::i32);
                switch(choice(eng) % 4)
                {
                    case 0: vt = static_cast<::std::uint8_t>(value_type::i32); break;
                    case 1: vt = static_cast<::std::uint8_t>(value_type::i64); break;
                    case 2: vt = static_cast<::std::uint8_t>(value_type::f32); break;
                    default: vt = static_cast<::std::uint8_t>(value_type::f64); break;
                }
                append_byte(payload, vt);
            }
        }
        append_section_with_payload(out, /*type id*/ 1u, payload);
    }

    // Build a function section: func_count entries of typeidx in [0, type_cnt)
    inline void build_function_section(u8string& out, ::std::uint32_t func_cnt, ::std::uint32_t type_cnt, ::fast_io::ibuf_white_hole_engine& eng) noexcept
    {
        u8string payload;
        append_uleb128(payload, func_cnt);
        ::std::uniform_int_distribution<::std::uint32_t> pick_type{0u, type_cnt ? (type_cnt - 1u) : 0u};
        for(::std::uint32_t i{}; i < func_cnt; ++i)
        {
            ::std::uint32_t const t = type_cnt ? pick_type(eng) : 0u;
            append_uleb128(payload, t);
        }
        append_section_with_payload(out, /*function id*/ 3u, payload);
    }

    // Build a code entry body into tmp, then wrap with length and append into payload
    inline void append_one_code_entry(u8string& payload,
                                      ::fast_io::ibuf_white_hole_engine& eng,
                                      bool make_invalid_local_type,
                                      bool missing_end,
                                      int mismatch_body_mode) noexcept
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm1::type;

        u8string body; // local_count + locals + expr

        ::std::uniform_int_distribution<int> prob{0, 99};
        ::std::uniform_int_distribution<int> small{0, 3};

        // local_count: number of groups
        ::std::uint32_t local_groups = static_cast<::std::uint32_t>(small(eng));
        append_uleb128(body, local_groups);

        for(::std::uint32_t g{}; g < local_groups; ++g)
        {
            // group count (can be 0..5)
            ::std::uint32_t const cnt = static_cast<::std::uint32_t>(prob(eng) % 6);
            append_uleb128(body, cnt);

            ::std::uint8_t vt{};
            if(make_invalid_local_type && prob(eng) < 10) // 10% inject invalid valtype
            {
                vt = 0x00u; // illegal value type
            }
            else
            {
                switch(prob(eng) % 4)
                {
                    case 0: vt = static_cast<::std::uint8_t>(value_type::i32); break;
                    case 1: vt = static_cast<::std::uint8_t>(value_type::i64); break;
                    case 2: vt = static_cast<::std::uint8_t>(value_type::f32); break;
                    default: vt = static_cast<::std::uint8_t>(value_type::f64); break;
                }
            }
            append_byte(body, vt);
        }

        // expr: random bytes, ensure last is 0x0B unless missing_end
        ::std::uint32_t const expr_len = static_cast<::std::uint32_t>(prob(eng) % 16); // 0..15
        for(::std::uint32_t i{}; i < expr_len; ++i) { append_byte(body, static_cast<::std::uint8_t>(prob(eng))); }

        if(!missing_end)
        {
            append_byte(body, 0x0Bu); // end
        }
        else
        {
            // 50% omit, 50% put random non-0x0B
            if(prob(eng) < 50) { /* omit */ }
            else { append_byte(body, 0x0Au); }
        }

        // declared body size
        ::std::uint32_t real_size = static_cast<::std::uint32_t>(body.size());
        ::std::uint32_t declared = real_size;
        if(mismatch_body_mode == 1 && real_size > 0) { declared = real_size - 1u; }
        else if(mismatch_body_mode == 2) { declared = real_size + 1u; }

        append_uleb128(payload, declared);
        for(auto ch: body) { payload.push_back(ch); }
    }

    // Build a code section with code_cnt entries
    inline void build_code_section(u8string& out,
                                   ::std::uint32_t code_cnt,
                                   ::fast_io::ibuf_white_hole_engine& eng,
                                   bool incomplete_leb_for_codecount) noexcept
    {
        u8string payload;

        if(incomplete_leb_for_codecount)
        {
            // an unterminated LEB128 (single 0x80)
            append_byte(payload, 0x80u);
            // payload may stop here; parser should report invalid_code_count
            append_section_with_payload(out, /*code id*/ 10u, payload);
            return;
        }

        append_uleb128(payload, code_cnt);

        ::std::uniform_int_distribution<int> prob{0, 99};
        for(::std::uint32_t i{}; i < code_cnt; ++i)
        {
            bool invalid_local = (prob(eng) < 5);     // 5%
            bool missing_end   = (prob(eng) < 5);     // 5%
            int  mismatch_mode = 0;                   // 0: none, 1: declared smaller, 2: declared larger
            if(prob(eng) < 8) { mismatch_mode = 1; }
            else if(prob(eng) < 16) { mismatch_mode = 2; }

            append_one_code_entry(payload, eng, invalid_local, missing_end, mismatch_mode);
        }

        append_section_with_payload(out, /*code id*/ 10u, payload);
    }
}

int main()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    ::fast_io::io::perr("Starting code-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    ::fast_io::ibuf_white_hole_engine eng;
    ::std::uniform_int_distribution<::std::uint32_t> type_cnt_eng{1u, 4u};
    ::std::uniform_int_distribution<::std::uint32_t> func_cnt_eng{0u, 8u};
    ::std::uniform_int_distribution<int> prob{0, 99};

    // rounds
    for(unsigned round{}; round != 30000u; ++round)
    {
        ::uwvm2::utils::container::u8string mod;

        // Header: magic + version
        mod.push_back(u8'\0');
        mod.push_back(u8'a');
        mod.push_back(u8's');
        mod.push_back(u8'm');
        mod.push_back(static_cast<char8_t>(0x01));
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');

        // Type section
        ::std::uint32_t const type_cnt = type_cnt_eng(eng);
        test::build_type_section(mod, type_cnt, eng);

        // Function section
        ::std::uint32_t const func_cnt = func_cnt_eng(eng);
        test::build_function_section(mod, func_cnt, type_cnt, eng);

        // Code section
        bool mismatch_code_func_cnt = (prob(eng) < 15);          // 15%: code_count != func_count
        bool incomplete_leb_cc      = !mismatch_code_func_cnt && (prob(eng) < 3); // 3%: code_count leb incomplete
        ::std::uint32_t code_cnt    = mismatch_code_func_cnt ? (func_cnt + 1u) : func_cnt;
        test::build_code_section(mod, code_cnt, eng, incomplete_leb_cc);

        auto const* begin = reinterpret_cast<::std::byte const*>(mod.data());
        auto const* end = begin + mod.size();

        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            (void)::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, err, {});
        }
        catch(::fast_io::error const&)
        {
            // expected on invalid cases; keep fuzzing
        }
    }

    ::fast_io::io::perr("Code-section fuzzing finished.\n");
}


