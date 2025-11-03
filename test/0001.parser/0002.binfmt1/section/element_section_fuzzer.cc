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
#include <random>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/value_binfmt.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    using ::uwvm2::utils::container::u8string;

    inline void append_byte(u8string& out, ::std::uint8_t b) noexcept { out.push_back(static_cast<char8_t>(b)); }

    inline void append_uleb128(u8string& out, ::std::uint32_t value) noexcept
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
    }

    inline void append_section_with_payload(u8string& mod, ::std::uint8_t sec_id, u8string const& payload) noexcept
    {
        append_byte(mod, sec_id);
        append_uleb128(mod, static_cast<::std::uint32_t>(payload.size()));
        for(auto ch: payload) { mod.push_back(ch); }
    }

    // Type section: 1 function type: 0x60 () -> ()
    inline void build_type_section_min(u8string& mod) noexcept
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm1::type;
        u8string payload;
        append_uleb128(payload, 1u); // type count
        append_byte(payload, static_cast<::std::uint8_t>(function_type_prefix::functype));
        append_uleb128(payload, 0u); // param vec
        append_uleb128(payload, 0u); // result vec
        append_section_with_payload(mod, /*type id*/ 1u, payload);
    }

    // Import section: import one function: module "m", name "f", kind=func, typeidx=0
    inline void build_import_section_one_func(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u); // import count
        // module name
        payload.push_back(static_cast<char8_t>(1));
        payload.push_back(u8'm');
        // extern name
        payload.push_back(static_cast<char8_t>(1));
        payload.push_back(u8'f');
        // desc: func
        payload.push_back(static_cast<char8_t>(0x00));
        // typeidx
        append_uleb128(payload, 0u);
        append_section_with_payload(mod, /*import id*/ 2u, payload);
    }

    // Table section: 1 table (elemtype=0x70 funcref), limits flag=0x00, min=1
    inline void build_table_section_min(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);   // table count = 1
        append_byte(payload, 0x70u);   // elemtype funcref
        append_byte(payload, 0x00u);   // limits flag: no max
        append_uleb128(payload, 1u);   // min = 1
        append_section_with_payload(mod, /*table id*/ 4u, payload);
    }

    // Build one element entry
    // modes:
    //  0: valid (table_idx=0, expr=i32.const 0; end, funcidx_count 1, funcidx 0)
    //  1: expr missing end
    //  2: table_idx OOB (1 while only 1 table -> OOB)
    //  3: funcidx OOB (> all_func_size)
    //  4: invalid funcidx_count LEB (handled outside as elem_count invalid)
    inline void append_one_element_entry(u8string& payload,
                                         ::fast_io::ibuf_white_hole_engine& eng,
                                         int mode,
                                         ::std::uint32_t all_func_size) noexcept
    {
        ::std::uniform_int_distribution<int> prob{0, 99};

        // table_idx
        ::std::uint32_t table_idx = 0u;
        if(mode == 2) { table_idx = 1u; }
        append_uleb128(payload, table_idx);

        // expr
        if(mode == 1)
        {
            payload.push_back(static_cast<char8_t>(0x41u)); // i32.const
            payload.push_back(static_cast<char8_t>(0x00u)); // 0
            // missing end
        }
        else
        {
            payload.push_back(static_cast<char8_t>(0x41u)); // i32.const
            payload.push_back(static_cast<char8_t>(0x00u)); // 0
            payload.push_back(static_cast<char8_t>(0x0Bu)); // end
        }

        // funcidx_count
        ::std::uint32_t funcidx_count = 1u;
        append_uleb128(payload, funcidx_count);

        // funcidx entries
        ::std::uint32_t idx = 0u;
        if(mode == 3)
        {
            idx = (all_func_size == 0u ? 1u : all_func_size); // out of bounds
        }
        append_uleb128(payload, idx);
        (void)eng; (void)prob;
    }

    inline void build_element_section(u8string& mod, ::fast_io::ibuf_white_hole_engine& eng, ::std::uint32_t all_func_size) noexcept
    {
        u8string payload;
        ::std::uniform_int_distribution<int> prob{0, 99};
        ::std::uniform_int_distribution<int> cnt_eng{0, 3};

        bool invalid_count_leb = (prob(eng) < 4); // 4% invalid LEB for elem_count
        if(invalid_count_leb)
        {
            payload.push_back(static_cast<char8_t>(0x80u)); // unterminated LEB128
            append_section_with_payload(mod, /*element id*/ 9u, payload);
            return;
        }

        int count = cnt_eng(eng);
        append_uleb128(payload, static_cast<::std::uint32_t>(count));

        for(int i{}; i < count; ++i)
        {
            int mode = 0;
            int p = prob(eng);
            if(p < 6) mode = 1;       // 6% missing end
            else if(p < 12) mode = 2; // 6% table idx OOB
            else if(p < 18) mode = 3; // 6% funcidx OOB
            append_one_element_entry(payload, eng, mode, all_func_size);
        }

        append_section_with_payload(mod, /*element id*/ 9u, payload);
    }
}

int main()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    ::fast_io::io::perr("Starting element-section fuzzer for wasm binfmt v1 (wasm1) ...\n");

    ::fast_io::ibuf_white_hole_engine eng;

    for(unsigned round{}; round != 50000u; ++round)
    {
        ::uwvm2::utils::container::u8string mod;

        // wasm magic + version
        mod.push_back(u8'\0');
        mod.push_back(u8'a');
        mod.push_back(u8's');
        mod.push_back(u8'm');
        mod.push_back(static_cast<char8_t>(0x01));
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');

        // type + import(func) + table
        test::build_type_section_min(mod);
        test::build_import_section_one_func(mod);
        test::build_table_section_min(mod);

        // all_func_size = imported(func=1) + defined(0)
        ::std::uint32_t const all_func_size = 1u;

        // element section
        test::build_element_section(mod, eng, all_func_size);

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

    ::fast_io::io::perr("Element-section fuzzing finished.\n");
}


