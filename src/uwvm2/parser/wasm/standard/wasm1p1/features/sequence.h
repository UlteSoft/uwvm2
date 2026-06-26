/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Section order with data count before code
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <initializer_list>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// @brief Section ordering table for wasm1.1 binary modules.
    /// @details Data count (id 12) is ordered after element and before code as required by the 1.1 draft.
    struct wasm1p1_sequence_storage_t
    {
        // Index is the section id; value is the canonical order. 0 keeps custom sections free-positioned.
        // WebAssembly 1.1 inserts data count (id 12) between element and code.
        inline static constexpr ::uwvm2::utils::container::array<::uwvm2::parser::wasm::binfmt::ver1::wasm_order_t, 13uz>
            section_id_sequential_mapping_table{{
                0u,   // custom
                1u,   // type
                2u,   // import
                3u,   // function
                4u,   // table
                5u,   // memory
                6u,   // global
                7u,   // export
                8u,   // start
                9u,   // element
                11u,  // code
                12u,  // data
                10u   // data count
            }};

        inline static constexpr ::uwvm2::parser::wasm::binfmt::ver1::wasm_order_t custom_name_order{
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_order_add_or_overflow_die_chain(12u, 1u)};

        inline static constexpr ::std::initializer_list<::uwvm2::parser::wasm::binfmt::ver1::details::mapping_entry>
            custom_section_sequential_mapping_table_entries{
                {u8"name", custom_name_order}
        };

        inline static constexpr auto custom_section_sequential_mapping_table_size{
            ::uwvm2::parser::wasm::binfmt::ver1::details::calculate_hash_table_size(custom_section_sequential_mapping_table_entries)};

        inline static constexpr auto custom_section_sequential_mapping_table{
            ::uwvm2::parser::wasm::binfmt::ver1::details::generate_custom_section_sequential_mapping_table<
                custom_section_sequential_mapping_table_size.hash_table_size,
                custom_section_sequential_mapping_table_size.extra_size,
                custom_section_sequential_mapping_table_size.real_max_conflict_size>(custom_section_sequential_mapping_table_entries)};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
