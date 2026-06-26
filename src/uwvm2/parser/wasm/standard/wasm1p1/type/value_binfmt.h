/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include "value_type.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::type
{
    /// @brief      Number Types
    /// @details    Number types are encoded by a single byte.
    /// @details    New feature
    /// @see        WebAssembly Release 1.1 (Draft 2021-11-16) § 5.3.1
    enum class number_type : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Number types
        i32 = 0x7F,
        i64 = 0x7E,
        f32 = 0x7D,
        f64 = 0x7C
    };

    /// @brief      Vector Types
    /// @details    Vector types are also encoded by a single byte.
    /// @details    New feature
    /// @see        WebAssembly Release 1.1 (Draft 2021-11-16) § 5.3.2
    enum class vector_type : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Vector types
        v128 = 0x7B
    };

    /// @brief      Reference Types
    /// @details    Reference types are also encoded by a single byte.
    /// @details    New feature
    /// @see        WebAssembly Release 1.1 (Draft 2021-11-16) § 5.3.3
    enum class reference_type : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Reference Types
        funcref = 0x70,
        externref = 0x6F
    };

    /// @brief      Value Types
    /// @details    Value types are encoded with their respective encoding as a number type or reference type.
    /// @details    Extends wasm1's value_type
    /// @warning    Extension point: new value types must be mirrored in validity checks, printable names, parser feature gates, runtime storage, and ECO output.
    /// @see        WebAssembly Release 1.1 (Draft 2021-11-16) § 5.3.4
    enum class value_type : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        // Number types
        i32 = 0x7F,
        i64 = 0x7E,
        f32 = 0x7D,
        f64 = 0x7C,

        // Vector types
        v128 = 0x7B,

        // Reference Types
        funcref = 0x70,
        externref = 0x6F
    };

    inline constexpr value_type section_details(value_type type) noexcept { return type; }

    /// @brief      Result Types
    /// @details    Result types are encoded by the respective vectors of value types `.
    /// @details    Modify the result type of wasm1 to support multiple returns.
    /// @see        WebAssembly Release 1.1 (Draft 2021-11-16) § 5.3.5
    struct vec_value_type
    {
        value_type const* begin{};
        value_type const* end{};
    };

    using result_type = vec_value_type;

    // func

    /// @warning Extension point: keep this classifier synchronized with number_type/value_type when a future standard adds numeric types.
    inline constexpr bool is_valid_number_type(value_type type) noexcept
    {
        switch(type)
        {
            case value_type::i32: [[fallthrough]];
            case value_type::i64: [[fallthrough]];
            case value_type::f32: [[fallthrough]];
            case value_type::f64: return true;
            default: return false;
        }
    }

    /// @warning Extension point: keep this classifier synchronized with vector_type/value_type when a future standard adds vector types.
    inline constexpr bool is_valid_vector_type(value_type type) noexcept
    {
        switch(type)
        {
            case value_type::v128: return true;
            default: return false;
        }
    }

    /// @warning Extension point: keep this classifier synchronized with reference_type/value_type when a future standard adds reference types.
    inline constexpr bool is_valid_reference_type(value_type type) noexcept
    {
        switch(type)
        {
            case value_type::funcref: [[fallthrough]];
            case value_type::externref: return true;
            default: return false;
        }
    }

    /// @warning Extension point: keep this total value-type classifier synchronized with every value_type enumerator.
    inline constexpr bool is_valid_value_type(value_type type) noexcept
    {
        switch(type)
        {
            case value_type::i32: [[fallthrough]];
            case value_type::i64: [[fallthrough]];
            case value_type::f32: [[fallthrough]];
            case value_type::f64: [[fallthrough]];
            case value_type::v128: [[fallthrough]];
            case value_type::funcref: [[fallthrough]];
            case value_type::externref: return true;
            default: return false;
        }
    }

    template <::std::integral char_type>
    /// @warning Extension point: update every character-specialized switch when adding a new value_type enumerator.
    inline constexpr auto get_value_name(value_type valtype) noexcept
    {
        if constexpr(::std::same_as<char_type, char>)
        {
            switch(valtype)
            {
                case value_type::i32: return ::uwvm2::utils::container::string_view{"i32"};
                case value_type::i64: return ::uwvm2::utils::container::string_view{"i64"};
                case value_type::f32: return ::uwvm2::utils::container::string_view{"f32"};
                case value_type::f64: return ::uwvm2::utils::container::string_view{"f64"};
                case value_type::v128: return ::uwvm2::utils::container::string_view{"v128"};
                case value_type::funcref: return ::uwvm2::utils::container::string_view{"funcref"};
                case value_type::externref: return ::uwvm2::utils::container::string_view{"externref"};
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
                case value_type::v128: return ::uwvm2::utils::container::wstring_view{L"v128"};
                case value_type::funcref: return ::uwvm2::utils::container::wstring_view{L"funcref"};
                case value_type::externref: return ::uwvm2::utils::container::wstring_view{L"externref"};
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
                case value_type::v128: return ::uwvm2::utils::container::u8string_view{u8"v128"};
                case value_type::funcref: return ::uwvm2::utils::container::u8string_view{u8"funcref"};
                case value_type::externref: return ::uwvm2::utils::container::u8string_view{u8"externref"};
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
                case value_type::v128: return ::uwvm2::utils::container::u16string_view{u"v128"};
                case value_type::funcref: return ::uwvm2::utils::container::u16string_view{u"funcref"};
                case value_type::externref: return ::uwvm2::utils::container::u16string_view{u"externref"};
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
                case value_type::v128: return ::uwvm2::utils::container::u32string_view{U"v128"};
                case value_type::funcref: return ::uwvm2::utils::container::u32string_view{U"funcref"};
                case value_type::externref: return ::uwvm2::utils::container::u32string_view{U"externref"};
                default: return ::uwvm2::utils::container::u32string_view{};
            }
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, value_type>) noexcept
    {
        return 9u;  // max
    }

    namespace details
    {
        template <::std::integral char_type>
        /// @warning Extension point: update every character-specialized print switch when adding a new value_type enumerator.
        inline constexpr char_type* print_reserve_value_type_impl(char_type* iter, value_type valtype) noexcept
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                switch(valtype)
                {
                    case value_type::i32: return ::fast_io::freestanding::my_copy_n("i32", 3u, iter);
                    case value_type::i64: return ::fast_io::freestanding::my_copy_n("i64", 3u, iter);
                    case value_type::f32: return ::fast_io::freestanding::my_copy_n("f32", 3u, iter);
                    case value_type::f64: return ::fast_io::freestanding::my_copy_n("f64", 3u, iter);
                    case value_type::v128: return ::fast_io::freestanding::my_copy_n("v128", 4u, iter);
                    case value_type::funcref: return ::fast_io::freestanding::my_copy_n("funcref", 7u, iter);
                    case value_type::externref: return ::fast_io::freestanding::my_copy_n("externref", 9u, iter);
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
                    case value_type::v128: return ::fast_io::freestanding::my_copy_n(L"v128", 4u, iter);
                    case value_type::funcref: return ::fast_io::freestanding::my_copy_n(L"funcref", 7u, iter);
                    case value_type::externref: return ::fast_io::freestanding::my_copy_n(L"externref", 9u, iter);
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
                    case value_type::v128: return ::fast_io::freestanding::my_copy_n(u8"v128", 4u, iter);
                    case value_type::funcref: return ::fast_io::freestanding::my_copy_n(u8"funcref", 7u, iter);
                    case value_type::externref: return ::fast_io::freestanding::my_copy_n(u8"externref", 9u, iter);
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
                    case value_type::v128: return ::fast_io::freestanding::my_copy_n(u"v128", 4u, iter);
                    case value_type::funcref: return ::fast_io::freestanding::my_copy_n(u"funcref", 7u, iter);
                    case value_type::externref: return ::fast_io::freestanding::my_copy_n(u"externref", 9u, iter);
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
                    case value_type::v128: return ::fast_io::freestanding::my_copy_n(U"v128", 4u, iter);
                    case value_type::funcref: return ::fast_io::freestanding::my_copy_n(U"funcref", 7u, iter);
                    case value_type::externref: return ::fast_io::freestanding::my_copy_n(U"externref", 9u, iter);
                    default: return iter;
                }
            }
        }
    }  // namespace details

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, value_type>, char_type * iter, value_type valtype) noexcept
    { return details::print_reserve_value_type_impl(iter, valtype); }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif
