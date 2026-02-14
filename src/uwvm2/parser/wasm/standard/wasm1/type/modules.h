/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-05
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
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <bit>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include "value_type.h"
# include "value_binfmt.h"
# include "section_type.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::type
{
    /// @brief      Indices
    /// @details    Definitionsarereferencedwithzero-basedindices. Eachclassofdefinitionhasitsownindexspace, asdistinguished
    ///             by the following classes.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.1
    using indices = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;

    /// @brief      Function Types
    /// @details    Function types are encoded by the byte 0x60 followed by the respective vectors of parameter and result types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.2
    struct local_function_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::function_type const* func_type{};
    };

    /// @brief      Functions
    /// @details    The funcs component of a module defines a vector of functions with the following structure.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.3
    struct local_entry
    {
        indices count{};
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type{};
    };

    /// @brief      Exports
    /// @details    The exports component of a module defines a set of exports that become accessible to the host environment once
    ///             the module has been instantiated.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.10
    struct export_type
    {
        ::uwvm2::utils::container::u8string_view export_name{};
        indices index{};
        ::uwvm2::parser::wasm::standard::wasm1::type::external_types kind{};
    };

    /// @brief      Imports
    /// @details    The imports component of a module defines a set of imports that are required for instantiation
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.11
    struct import_type
    {
        ::uwvm2::utils::container::u8string_view module_name{};
        ::uwvm2::utils::container::u8string_view extern_name{};
        ::uwvm2::parser::wasm::standard::wasm1::type::external_types importdesc{};
    };

    /// @brief      Used to store the contents of the result of a global constant expression calculation.
    union global_storage_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
        ::std::size_t ref;
    };

    /// @brief      Expressions
    /// @details    Function bodies, initialization values for globals, and offsets of element or data segments are given as expressions,
    ///             which are sequences of instructions terminated by an end marker.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.4.6
    struct initializer_exp
    {
        global_storage_t global_storage{};

        ::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type type_opcode{};
    };

    /// @brief      Globals
    /// @details    The globals component of a module defines a vector of global variables (or globals for short)
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.6
    struct local_global_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::global_type type{};
        initializer_exp init{};
    };

    /// @brief      Element Segments
    /// @details    The initial contents of a table is uninitialized. The elem component of a module defines a vector of element segments
    ///             that initialize a subrange of a table, at a given offset, from a static vector of elements.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.7
    struct elem_segment_type 
    {
        indices table{};
        initializer_exp offset{};
        ::uwvm2::utils::container::vector<indices> init{};
    };

    /// @brief      Data Segments
    /// @details    The initial contents of a memory are zero-valued bytes. The data component of a module defines a vector of data
    ///             segments that initialize a range of memory, at a given offset, with a static vector of bytes.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.8
    struct data_segment
    {
        indices data{};
        initializer_exp offset{};
        ::uwvm2::parser::wasm::standard::wasm1::type::vec_byte init{};
    };
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::local_function_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::local_entry>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::import_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::initializer_exp>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::local_global_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::export_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::type::elem_segment_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::elem_segment_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::data_segment>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
