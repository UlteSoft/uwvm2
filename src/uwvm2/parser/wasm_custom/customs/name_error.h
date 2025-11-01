/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-09
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
# include <climits>
# include <limits>
# include <concepts>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm_custom::customs
{
    enum class name_err_type_t : unsigned
    {
        invalid_name_map_length,
        size_exceeds_the_maximum_value_of_size_t,
        illegal_name_map_length,
        invalid_module_name_length,
        illegal_module_name_length,
        illegal_char_sequence,
        duplicate_module_name,
        duplicate_function_name,
        invalid_data_exists,
        invalid_function_name_count,
        invalid_function_index,
        invalid_function_name_length,
        illegal_section_id,
        illegal_function_name_length,
        duplicate_func_idx,
        duplicate_local_name,
        invalid_local_count,
        invalid_function_local_count,
        invalid_function_local_index,
        invalid_function_local_name_length,
        illegal_function_local_name_length,
        duplicate_code_function_index,
        duplicate_code_local_name_function_index,
        invalid_section_canonical_order,
        duplicate_name_section,
        invalid_function_index_order,
        invalid_function_local_index_order,
        exceed_the_max_name_parser_limit
    };

    /// @brief Used to set the output of exceed_the_max_parser_limit errors
    struct exceed_the_max_name_parser_limit_t
    {
        ::uwvm2::utils::container::u8string_view name;
        ::std::size_t value;
        ::std::size_t maxval;
    };

    union name_err_storage_t
    {
        exceed_the_max_name_parser_limit_t exceed_the_max_name_parser_limit;
        static_assert(::std::is_trivially_copyable_v<exceed_the_max_name_parser_limit_t> && ::std::is_trivially_destructible_v<exceed_the_max_name_parser_limit_t>);

        ::std::byte const* err_end;
        ::std::size_t err_uz;
        ::std::ptrdiff_t err_pdt;

        ::std::uint_least64_t u64;
        ::std::int_least64_t i64;
        ::std::uint_least32_t u32;
        ::std::int_least32_t i32;
        ::std::uint_least16_t u16;
        ::std::int_least16_t i16;
        ::std::uint_least8_t u8;
        ::std::int_least8_t i8;

        ::uwvm2::parser::wasm::base::error_f64 f64;
        ::uwvm2::parser::wasm::base::error_f32 f32;
        bool boolean;

        ::std::uint_least64_t u64arr[1];
        ::std::int_least64_t i64arr[1];
        ::std::uint_least32_t u32arr[2];
        ::std::int_least32_t i32arr[2];
        ::std::uint_least16_t u16arr[4];
        ::std::int_least16_t i16arr[4];
        ::std::uint_least8_t u8arr[8];
        ::std::int_least8_t i8arr[8];

        ::uwvm2::parser::wasm::base::error_f64 f64arr[1];
        ::uwvm2::parser::wasm::base::error_f32 f32arr[2];
        bool booleanarr[8];
    };

    struct name_err_t
    {
        ::std::byte const* curr{};
        name_err_type_t type{};
        name_err_storage_t err{};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
