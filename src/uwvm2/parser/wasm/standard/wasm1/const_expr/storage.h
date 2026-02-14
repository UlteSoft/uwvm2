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
 * @date        2025-04-09
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
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

/// @brief   WebAssembly 1.0 Constant Expressions: Key Considerations
/// @details Constant expressions in WebAssembly 1.0 are a restricted subset of expressions that must be evaluated at compile time.
///          They are used for initializing global variables, defining table offsets, and specifying memory sizes.
///
///          PERMITTED INSTRUCTIONS:
///          - i32.const, i64.const, f32.const, f64.const (numeric constants)
///          - global.get (only for immutable imported globals)
///
///          CRITICAL RESTRICTIONS:
///          - global.get can only reference IMMUTABLE imported global variables
///          - No recursion: cannot reference globals defined in the same module
///          - Must produce exactly one value on the type stack
///          - All operations must be statically evaluable and deterministic
///          - No runtime dependencies or side effects allowed
///
///          VALIDATION REQUIREMENTS:
///          - Type consistency between expression result and target
///          - Stack validation (exactly one element after evaluation)
///          - Global variable mutability and scope verification
///          - Instruction set limitations enforcement
///
///          SECURITY BENEFITS:
///          - Deterministic initialization ensures predictable module behavior
///          - Static evaluation prevents initialization-time attacks
///          - Compile-time verification enables comprehensive security analysis
///
///          @see WebAssembly Release 1.0 (2019-07-20) § 2.4.6, § 3.4.10

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::const_expr
{
    union base_const_expr_opcode_storage_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 imported_global_idx;
    };

    struct base_const_expr_opcode_t
    {

        base_const_expr_opcode_storage_u storage{};
        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic opcode{};
    };
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(
        ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::const_expr
{
    struct wasm1_const_expr_storage_t 
    {
        ::std::byte const* begin{};
        ::std::byte const* end{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_t> opcodes{};
    };
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
