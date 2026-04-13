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
 * @date        2025-05-03
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
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    /// @brief These are proprietary implementations of wasm 1.0's types, which can be replaced by calls implemented via adl

    /// @brief      Value Types
    /// @details    Value types are encoded by a single byte.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.1
    /// @note       ADL for distribution to the correct handler function
    inline constexpr bool is_valid_value_type(::uwvm2::parser::wasm::standard::wasm1::type::value_type vt  // [adl]
                                              ) noexcept
    {
        switch(vt)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64:
            {
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    /// @brief      Limits
    /// @details    Limits classify the size range of resizeable storage associated with memory types and table types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.4
    /// @note       ADL for distribution to the correct handler function
    inline constexpr ::std::byte const* scan_limit_type(::uwvm2::parser::wasm::standard::wasm1::type::limits_type & limit_r,  // [adl]
                                                        ::std::byte const* const begin,
                                                        ::std::byte const* const end,
                                                        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        // uncheck begin > end
        auto curr{begin};

        // flag min ... (max ...)
        // unsafe (could be the end)
        // ^^ curr

        if(curr == end) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_cannot_find_flag;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [flag] min ... (max ...)
        // [safe] unsafe (could be the end)
        // ^^ curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte flags;
        ::std::memcpy(::std::addressof(flags), curr, sizeof(flags));

        static_assert(sizeof(flags) == 1uz);
        // Size equal to one does not need to do little-endian conversion

        // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
        flags = static_cast<decltype(flags)>(static_cast<::std::uint_least8_t>(flags) & 0xFFu);
#endif

        if(flags != 0u && flags != 1u) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_selectable.u8 = flags;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_illegal_flag;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // flags does not need to be stored

        ++curr;

        // [flag] min ... max (end)
        // [safe] unsafe (could be the end)
        //        ^^ curr

        // Add scope space to prevent subsequent access to variables that should not be accessed

        // min, alway exists

        // [flag] min ... max (end)
        // [safe] unsafe (could be the end)
        //        ^^ curr

        // scan min
        // Storing Temporary Variables into Modules
        limit_r.present_max = false;

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 n_min;  // No initialization necessary
        auto const [next_n_min, err_n_min]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(end),
                                                                    ::fast_io::mnp::leb128_get(n_min))};

        if(err_n_min != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_invalid_min;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(err_n_min);
        }

        // [flag min ...] max (end)
        // [    safe    ] unsafe (could be the end)
        //       ^^ curr

        // Storing Temporary Variables into Modules
        limit_r.min = n_min;

        curr = reinterpret_cast<::std::byte const*>(next_n_min);
        // [flag min ...] max (end)
        // [    safe    ] unsafe (could be the end)
        //                ^^ curr

        if(flags == 1u)
        {
            // [flag min ...] max ... (end)
            // [    safe    ] unsafe (could be the end)
            //                ^^ curr

            // scan max
            // Storing Temporary Variables into Modules
            limit_r.present_max = true;

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 n_max;  // No initialization necessary
            auto const [next_n_max, err_n_max]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(end),
                                                                        ::fast_io::mnp::leb128_get(n_max))};

            if(err_n_max != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_invalid_max;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(err_n_max);
            }

            // [flag min ... max ...] (end)
            // [        safe        ] unsafe (could be the end)
            //               ^^ curr

            if(n_max < n_min) [[unlikely]]
            {
                err.err_curr = curr;
                err.err_selectable.u32arr[0u] = n_max;
                err.err_selectable.u32arr[1u] = n_min;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_max_lt_min;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // Storing Temporary Variables into Modules
            limit_r.max = n_max;

            curr = reinterpret_cast<::std::byte const*>(next_n_max);

            // [flag min ... max ...] (end)
            // [        safe        ] unsafe (could be the end)
            //                        ^^ curr
        }

        return curr;
    }

    /// @brief      Table Types
    /// @details    Memory types classify linear memories and their size range.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.6
    /// @note       ADL for distribution to the correct handler function
    inline constexpr ::std::byte const* scan_table_type(::uwvm2::parser::wasm::standard::wasm1::type::table_type & table_r,  // [adl]
                                                        ::std::byte const* const begin,
                                                        ::std::byte const* const end,
                                                        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        // uncheck begin > end
        auto curr{begin};

        // reftype limits ...
        // unsafe (could be the end)
        // ^^ curr

        if(curr == end) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_type_cannot_find_element;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [reftype] limits ...
        // [ safe  ] unsafe (could be the end)
        // ^^ curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte elemtype;
        ::std::memcpy(::std::addressof(elemtype), curr, sizeof(elemtype));

        static_assert(sizeof(elemtype) == 1uz);
        // Size equal to one does not need to do little-endian conversion

        // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
        elemtype = static_cast<decltype(elemtype)>(static_cast<::std::uint_least8_t>(elemtype) & 0xFFu);
#endif

        // In wasm 1.0, the element type elemtype must be funcref
        if(elemtype != 0x70u) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_selectable.u8 = elemtype;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_type_illegal_element;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // no necessary to set reftype in wasm1, always 0x70

        ++curr;

        // [reftype] limits ... (end)
        // [  safe ] unsafe (could be the end)
        //           ^^ curr

        // set limits
        curr = scan_limit_type(table_r.limits, curr, end, err);

        // [reftype limits ...] (end)
        // [         safe     ] unsafe (could be the end)
        //                      ^^ curr

        return curr;
    }

    /// @brief      Memory Types
    /// @details    Memory types classify linear memories and their size range.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.5
    /// @note       ADL for distribution to the correct handler function
    inline constexpr ::std::byte const* scan_memory_type(::uwvm2::parser::wasm::standard::wasm1::type::memory_type & memory_r,  // [adl]
                                                         ::std::byte const* const begin,
                                                         ::std::byte const* const end,
                                                         ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        // uncheck begin > end
        auto curr{begin};

        // limits ... (end)
        // unsafe (could be the end)
        // ^^ curr

        // check in scan_limit_type

        curr = scan_limit_type(memory_r.limits, curr, end, err);

        // [limits ...] (end)
        // [   safe   ] unsafe (could be the end)
        //              ^^ curr

        return curr;
    }

    /// @brief      Global Types
    /// @details    Global types classify global variables, which hold a value and can either be mutable or immutable
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.7
    inline constexpr ::std::byte const* scan_global_type(::uwvm2::parser::wasm::standard::wasm1::type::global_type & global_r,
                                                         ::std::byte const* const begin,
                                                         ::std::byte const* const end,
                                                         ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        // uncheck begin > end
        auto curr{begin};

        // valtype ma (end)
        // unsafe (could be the end)
        // ^^ curr

        if(curr == end) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_valtype;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [valtype] ma (end)
        // [  safe ] unsafe (could be the end)
        // ^^ curr

        ::uwvm2::parser::wasm::standard::wasm1::type::value_type gvt;

        ::std::memcpy(::std::addressof(gvt), curr, sizeof(gvt));

        static_assert(sizeof(gvt) == 1uz);
        // Size equal to one does not need to do little-endian conversion

        // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
        gvt = static_cast<decltype(gvt)>(static_cast<::std::uint_least8_t>(gvt) & 0xFFu);
#endif

        if(!is_valid_value_type(gvt)) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_selectable.u8 = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(gvt);
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_valtype;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // Storing Temporary Variables into Modules
        global_r.type = gvt;

        ++curr;

        // [valtype] ma (end)
        // [  safe ] unsafe (could be the end)
        //           ^^ curr

        if(curr == end) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_mut;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [valtype ma] (end)
        // [   safe   ] unsafe (could be the end)
        //          ^^ curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte mut;

        ::std::memcpy(::std::addressof(mut), curr, sizeof(mut));

        static_assert(sizeof(mut) == 1uz);
        // Size equal to one does not need to do little-endian conversion

        // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
        mut = static_cast<decltype(mut)>(static_cast<::std::uint_least8_t>(mut) & 0xFFu);
#endif

        if(mut > 1) [[unlikely]]
        {
            err.err_curr = curr;
            err.err_selectable.u8 = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(mut);
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_mut;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // Storing Temporary Variables into Modules
        global_r.is_mutable = static_cast<bool>(mut);

        ++curr;

        // [valtype ma] (end)
        // [   safe   ] unsafe (could be the end)
        //              ^^ curr

        return curr;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
