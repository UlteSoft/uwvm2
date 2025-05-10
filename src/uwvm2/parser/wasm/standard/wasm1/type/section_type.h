/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import :value_type;
import :value_binfmt;
#else
// std
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <bit>
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include "value_type.h"
# include "value_binfmt.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::type
{
    /// @brief      Limits
    /// @details    Limits classify the size range of resizeable storage associated with memory types and table types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.4
    struct limits_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 min{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max{};
        bool present_max{};
    };

    /// @brief      Function Types
    /// @details    Function types are encoded by the byte 0x60 followed by the respective vectors of parameter and result types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.3
    struct function_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::vec_value_type parameter{};
        ::uwvm2::parser::wasm::standard::wasm1::type::vec_value_type result{};
    };

    /// @brief      Memory Types
    /// @details    Memory types classify linear memories and their size range.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.5
    struct memory_type
    {
        limits_type limits{};

        bool is_exported{};
    };

    /// @brief      Table Types
    /// @details    Memory types classify linear memories and their size range.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.6
    struct table_type
    {
        limits_type limits{};

        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte reftype{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(0x70)};

        bool is_exported{};
    };

    /// @brief      Global Types
    /// @details    Global types classify global variables, which hold a value and can either be mutable or immutable
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.7
    struct global_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type{};
        bool is_mutable{};

        bool is_exported{};
    };

    /// @brief      External Types
    /// @details    External types classify imports and external values with their respective types
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.8
    enum class external_types : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        func = 0x00u,
        table = 0x01u,
        memory = 0x02u,
        global = 0x03u,
        // extern_type_end: for concept, not standard
        external_type_end = global
    };

    /// @brief      Exports
    /// @details    The exports component of a module defines a set of exports that become accessible to the host environment once
    ///             the module has been instantiated
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.10
    struct extern_type
    {
        union storage_t
        {
            function_type const* function;
            table_type table;
            memory_type memory;
            global_type global;
        } storage;

        external_types type{};
    };

    // print func

    template <::std::integral char_type>
    inline constexpr auto get_extern_kind_name(external_types exttype) noexcept
    {
        if constexpr(::std::same_as<char_type, char>)
        {
            switch(exttype)
            {
                case external_types::func: return ::fast_io::string_view{"func"};
                case external_types::table: return ::fast_io::string_view{"table"};
                case external_types::memory: return ::fast_io::string_view{"memory"};
                case external_types::global: return ::fast_io::string_view{"global"};
                default: return ::fast_io::string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::fast_io::wstring_view{L"func"};
                case external_types::table: return ::fast_io::wstring_view{L"table"};
                case external_types::memory: return ::fast_io::wstring_view{L"memory"};
                case external_types::global: return ::fast_io::wstring_view{L"global"};
                default: return ::fast_io::wstring_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::fast_io::u8string_view{u8"func"};
                case external_types::table: return ::fast_io::u8string_view{u8"table"};
                case external_types::memory: return ::fast_io::u8string_view{u8"memory"};
                case external_types::global: return ::fast_io::u8string_view{u8"global"};
                default: return ::fast_io::u8string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::fast_io::u16string_view{u"func"};
                case external_types::table: return ::fast_io::u16string_view{u"table"};
                case external_types::memory: return ::fast_io::u16string_view{u"memory"};
                case external_types::global: return ::fast_io::u16string_view{u"global"};
                default: return ::fast_io::u16string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::fast_io::u32string_view{U"func"};
                case external_types::table: return ::fast_io::u32string_view{U"table"};
                case external_types::memory: return ::fast_io::u32string_view{U"memory"};
                case external_types::global: return ::fast_io::u32string_view{U"global"};
                default: return ::fast_io::u32string_view{};
            }
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, external_types>) noexcept
    {
        return 6u;
    }

    namespace details
    {
        template <::std::integral char_type>
        inline constexpr char_type* print_reserve_extern_kind_impl(char_type* iter, external_types exttype) noexcept
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n("func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n("table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n("memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n("global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(L"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(L"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(L"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(L"global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(u8"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(u8"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(u8"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(u8"global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(u"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(u"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(u"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(u"global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(U"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(U"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(U"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(U"global", 6u, iter);
                    default: return iter;
                }
            }
        }
    }  // namespace details

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, external_types>, char_type * iter, external_types exttype) noexcept
    {
        return details::print_reserve_extern_kind_impl(iter, exttype);
    }

}  // namespace uwvm2::parser::wasm::standard::wasm1::type

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::function_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::table_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::memory_type>
    {
        inline static constexpr bool value = true;
    };
}  // namespace fast_io::freestanding

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif