/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
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
#include <type_traits>
#include <concepts>
#include <memory>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/handler_def.h>
# include <uwvm2/uwvm/io/impl.h>
#else
# error "Module testing is not currently supported"
#endif

using ::uwvm2::parser::wasm::binfmt::ver1::has_custom_section_sequential_mapping_table_define;

namespace ver1 = ::uwvm2::parser::wasm::binfmt::ver1;
namespace csdt = ::uwvm2::parser::wasm::binfmt::ver1::details;

// Test subject that exposes the required member for concept checking
struct custom_section_test_holder
{
    using wasm_order_t = ver1::wasm_order_t;

    inline static constexpr ::std::initializer_list<csdt::mapping_entry> custom_section_sequential_mapping_table_entries{
        {u8"section1", 1},
        {u8"section2", 2},
        {u8"section3", 3}
    };
    inline static constexpr auto custom_section_sequential_mapping_table_size{csdt::calculate_hash_table_size(custom_section_sequential_mapping_table_entries)};

    inline static constexpr auto custom_section_sequential_mapping_table =
        csdt::generate_custom_section_sequential_mapping_table<custom_section_sequential_mapping_table_size.hash_table_size,
                                                               custom_section_sequential_mapping_table_size.extra_size,
                                                               custom_section_sequential_mapping_table_size.real_max_conflict_size>(
            custom_section_sequential_mapping_table_entries);
};

// 1) Compile-time generation and lookup tests
static_assert(csdt::find_from_custom_section_sequential_mapping_table(custom_section_test_holder::custom_section_sequential_mapping_table, u8"section1") == 1u);
static_assert(csdt::find_from_custom_section_sequential_mapping_table(custom_section_test_holder::custom_section_sequential_mapping_table, u8"section2") == 2u);
static_assert(csdt::find_from_custom_section_sequential_mapping_table(custom_section_test_holder::custom_section_sequential_mapping_table, u8"section3") == 3u);
static_assert(csdt::find_from_custom_section_sequential_mapping_table(custom_section_test_holder::custom_section_sequential_mapping_table, u8"sectionX") == 0u);

// 2) Runtime lookup tests
int main()
{
    using wasm_order_t = ver1::wasm_order_t;
    auto const& table = custom_section_test_holder::custom_section_sequential_mapping_table;

    auto const v1{csdt::find_from_custom_section_sequential_mapping_table(table, u8"section1")};
    auto const v2{csdt::find_from_custom_section_sequential_mapping_table(table, u8"section2")};
    auto const v3{csdt::find_from_custom_section_sequential_mapping_table(table, u8"section3")};
    auto const vx{csdt::find_from_custom_section_sequential_mapping_table(table, u8"sectionX")};

    // Print to stderr to align with existing test output style
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output,
                          u8"custom_section lookup: ",
                          u8"section1=",
                          static_cast<unsigned>(v1),
                          u8", ",
                          u8"section2=",
                          static_cast<unsigned>(v2),
                          u8", ",
                          u8"section3=",
                          static_cast<unsigned>(v3),
                          u8", ",
                          u8"sectionX=",
                          static_cast<unsigned>(vx));

    // Simple runtime validations (terminate fast on mismatch)
    if(v1 != static_cast<wasm_order_t>(1) || v2 != static_cast<wasm_order_t>(2) || v3 != static_cast<wasm_order_t>(3) || vx != static_cast<wasm_order_t>(0))
    {
        ::fast_io::fast_terminate();
    }

}

// macro
#include <uwvm2/utils/macro/pop_macros.h>

