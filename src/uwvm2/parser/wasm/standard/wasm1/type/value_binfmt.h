/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
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
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include "value_type.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::type
{
    /// @brief      Value Types
    /// @details    Value types are encoded by a single byte.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.1
    enum class value_type : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Number types
        i32 = 0x7F,
        i64 = 0x7E,
        f32 = 0x7D,
        f64 = 0x7C,
    };

    /// @brief Wrapper for the value type
    struct value_type_section_details_wrapper_t
    {
        value_type type{};
    };

    inline constexpr value_type_section_details_wrapper_t section_details(value_type type) noexcept { return {type}; }

    /// @brief      Result Types
    /// @details    The only result types occurring in the binary format are the types of blocks. These are encoded in special com
    ///             pressed form, by either the byte 0x40 indicating the empty type or as a single value type.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.2
    enum class result_type : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Empty Types
        empty = 0x40,
        // t:valtype
        i32 = 0x7F,
        i64 = 0x7E,
        f32 = 0x7D,
        f64 = 0x7C,
    };

    /// @brief      Function Types Prefix
    /// @details    Function types are encoded by the byte 0x60 followed by the respective vectors of parameter and result types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.3
    enum class function_type_prefix : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Function Types
        functype = 0x60,
    };

    /// @brief      vec(valtype)
    /// @details    the respective vectors of value type
    struct vec_value_type
    {
        value_type const* begin{};
        value_type const* end{};
    };

    /// @brief      Limits preceding flag
    /// @details    A prefix to limit to indicate whether to provide a maximum value.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.4
    enum class limits_preceding_flag : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        non_max = 0x00,
        have_max = 0x01,
    };

    /// @brief      vec(byte)
    /// @details    the respective vectors of value byte
    struct vec_byte
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* byte_begin{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* byte_end{};
    };

    template <::std::integral char_type>
    inline constexpr auto get_value_name(value_type_section_details_wrapper_t valtype_wrapper) noexcept
    {
        auto const valtype{valtype_wrapper.type};
        if constexpr(::std::same_as<char_type, char>)
        {
            switch(valtype)
            {
                case value_type::i32: return ::uwvm2::utils::container::string_view{"i32"};
                case value_type::i64: return ::uwvm2::utils::container::string_view{"i64"};
                case value_type::f32: return ::uwvm2::utils::container::string_view{"f32"};
                case value_type::f64: return ::uwvm2::utils::container::string_view{"f64"};
                default: return ::uwvm2::utils::container::string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            switch(valtype)
            {
                case value_type::i32: return ::uwvm2::utils::container::wstring_view{L"i32"};
                case value_type::i64: return ::uwvm2::utils::container::wstring_view{L"i64"};
                case value_type::f32: return ::uwvm2::utils::container::wstring_view{L"f32"};
                case value_type::f64: return ::uwvm2::utils::container::wstring_view{L"f64"};
                default: return ::uwvm2::utils::container::wstring_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            switch(valtype)
            {
                case value_type::i32: return ::uwvm2::utils::container::u8string_view{u8"i32"};
                case value_type::i64: return ::uwvm2::utils::container::u8string_view{u8"i64"};
                case value_type::f32: return ::uwvm2::utils::container::u8string_view{u8"f32"};
                case value_type::f64: return ::uwvm2::utils::container::u8string_view{u8"f64"};
                default: return ::uwvm2::utils::container::u8string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            switch(valtype)
            {
                case value_type::i32: return ::uwvm2::utils::container::u16string_view{u"i32"};
                case value_type::i64: return ::uwvm2::utils::container::u16string_view{u"i64"};
                case value_type::f32: return ::uwvm2::utils::container::u16string_view{u"f32"};
                case value_type::f64: return ::uwvm2::utils::container::u16string_view{u"f64"};
                default: return ::uwvm2::utils::container::u16string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            switch(valtype)
            {
                case value_type::i32: return ::uwvm2::utils::container::u32string_view{U"i32"};
                case value_type::i64: return ::uwvm2::utils::container::u32string_view{U"i64"};
                case value_type::f32: return ::uwvm2::utils::container::u32string_view{U"f32"};
                case value_type::f64: return ::uwvm2::utils::container::u32string_view{U"f64"};
                default: return ::uwvm2::utils::container::u32string_view{};
            }
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, value_type_section_details_wrapper_t>) noexcept
    {
        return 3u;  // max
    }

    namespace details
    {
        template <::std::integral char_type>
        inline constexpr char_type* print_reserve_value_type_impl(char_type* iter, value_type_section_details_wrapper_t valtype_wrapper) noexcept
        {
            auto const valtype{valtype_wrapper.type};
            if constexpr(::std::same_as<char_type, char>)
            {
                switch(valtype)
                {
                    case value_type::i32: return ::fast_io::freestanding::my_copy_n("i32", 3u, iter);
                    case value_type::i64: return ::fast_io::freestanding::my_copy_n("i64", 3u, iter);
                    case value_type::f32: return ::fast_io::freestanding::my_copy_n("f32", 3u, iter);
                    case value_type::f64: return ::fast_io::freestanding::my_copy_n("f64", 3u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                switch(valtype)
                {
                    case value_type::i32: return ::fast_io::freestanding::my_copy_n(L"i32", 3u, iter);
                    case value_type::i64: return ::fast_io::freestanding::my_copy_n(L"i64", 3u, iter);
                    case value_type::f32: return ::fast_io::freestanding::my_copy_n(L"f32", 3u, iter);
                    case value_type::f64: return ::fast_io::freestanding::my_copy_n(L"f64", 3u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                switch(valtype)
                {
                    case value_type::i32: return ::fast_io::freestanding::my_copy_n(u8"i32", 3u, iter);
                    case value_type::i64: return ::fast_io::freestanding::my_copy_n(u8"i64", 3u, iter);
                    case value_type::f32: return ::fast_io::freestanding::my_copy_n(u8"f32", 3u, iter);
                    case value_type::f64: return ::fast_io::freestanding::my_copy_n(u8"f64", 3u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                switch(valtype)
                {
                    case value_type::i32: return ::fast_io::freestanding::my_copy_n(u"i32", 3u, iter);
                    case value_type::i64: return ::fast_io::freestanding::my_copy_n(u"i64", 3u, iter);
                    case value_type::f32: return ::fast_io::freestanding::my_copy_n(u"f32", 3u, iter);
                    case value_type::f64: return ::fast_io::freestanding::my_copy_n(u"f64", 3u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                switch(valtype)
                {
                    case value_type::i32: return ::fast_io::freestanding::my_copy_n(U"i32", 3u, iter);
                    case value_type::i64: return ::fast_io::freestanding::my_copy_n(U"i64", 3u, iter);
                    case value_type::f32: return ::fast_io::freestanding::my_copy_n(U"f32", 3u, iter);
                    case value_type::f64: return ::fast_io::freestanding::my_copy_n(U"f64", 3u, iter);
                    default: return iter;
                }
            }
        }
    }  // namespace details

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, value_type_section_details_wrapper_t>,
                                                     char_type * iter,
                                                     value_type_section_details_wrapper_t valtype_wrapper) noexcept
    { return details::print_reserve_value_type_impl(iter, valtype_wrapper); }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif
