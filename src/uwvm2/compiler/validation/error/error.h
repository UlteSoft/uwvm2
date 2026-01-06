/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-07
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
# include <uwvm2/utils/intrinsics/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::compiler::validation::error
{
    enum class code_validation_error_code : ::std::uint_least32_t
    {
        ok = 0u,
        missing_end,
        illegal_opbase,
        operand_stack_underflow,
        select_type_mismatch,
    };

    /// @brief define IEEE 754 F32 and F64
    using error_f32 = ::uwvm2::utils::precfloat::float32_t;
    using error_f64 = ::uwvm2::utils::precfloat::float64_t;

    /// @brief used for duplicate_exports_of_the_same_export_type
    struct operand_stack_underflow_err_t
    {
        ::uwvm2::utils::container::u8string_view op_code_name;
        ::std::size_t stack_size_actual;
        ::std::size_t stack_size_required;
    };

    struct select_type_mismatch_err_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type_v1;
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type_v2;
    };

    /// @brief Additional information provided by wasm error
    union code_validation_error_selectable_t
    {
        operand_stack_underflow_err_t operand_stack_underflow;
        static_assert(::std::is_trivially_copyable_v<operand_stack_underflow_err_t> && ::std::is_trivially_destructible_v<operand_stack_underflow_err_t>);

        select_type_mismatch_err_t select_type_mismatch;
        static_assert(::std::is_trivially_copyable_v<select_type_mismatch_err_t> && ::std::is_trivially_destructible_v<select_type_mismatch_err_t>);

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

        error_f64 f64;
        error_f32 f32;
        bool boolean;

        ::std::uint_least64_t u64arr[1];
        ::std::int_least64_t i64arr[1];
        ::std::uint_least32_t u32arr[2];
        ::std::int_least32_t i32arr[2];
        ::std::uint_least16_t u16arr[4];
        ::std::int_least16_t i16arr[4];
        ::std::uint_least8_t u8arr[8];
        ::std::int_least8_t i8arr[8];

        error_f64 f64arr[1];
        error_f32 f32arr[2];
        bool booleanarr[8];
    };

    /// @brief Structured structure, with reference form as formal parameter
    struct code_validation_error_impl
    {
        code_validation_error_selectable_t err_selectable{};
        ::std::byte const* err_curr{};
        code_validation_error_code err_code{};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
