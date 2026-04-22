
/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-03
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "types.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    struct wasm1_sequence_storage_t
    {
        // custom order

        // The name section should appear only once in a module, and only after the data section.
        // After the data section is checked by the parser.
        // appear only once in a module is checked by the custom name parser.

        inline static constexpr ::uwvm2::parser::wasm::binfmt::ver1::wasm_order_t custom_name_order{
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_order_add_or_overflow_die_chain(
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::data_sec),
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(1u))};

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
