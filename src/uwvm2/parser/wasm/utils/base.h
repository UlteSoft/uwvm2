/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-06
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
# include <type_traits>
# include <concepts>
# include <utility>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::utils
{
    /// @brief ++i, Exception thrown on overflow
    template <::std::unsigned_integral I>
    UWVM_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr I& counter_selfinc_throw_when_overflow(I & i,
                                                                                              ::std::byte const* section_curr,
                                                                                              ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
#if defined(__ARM_FEATURE_SME)
        __arm_streaming_compatible
#endif
    {
        if(i == ::std::numeric_limits<I>::max()) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        return ++i;
    }

    /// @brief i + add, Exception thrown on overflow
    template <::std::unsigned_integral I>
    UWVM_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr I counter_add_throw_when_overflow(I i,
                                                                                         I add,
                                                                                         ::std::byte const* section_curr,
                                                                                         ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
#if defined(__ARM_FEATURE_SME)
        __arm_streaming_compatible
#endif
    {
#if defined(_MSC_VER) && !defined(__clang__)
        if UWVM_IF_NOT_CONSTEVAL
        {
# if defined(_M_X64) && !(defined(_M_ARM64EC) || defined(__arm64ec__))
            if constexpr(::std::same_as<I, ::std::uint64_t>)
            {
                ::std::uint64_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u64(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else if constexpr(::std::same_as<I, ::std::uint32_t>)
            {
                ::std::uint32_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u32(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else if constexpr(::std::same_as<I, ::std::uint16_t>)
            {
                ::std::uint16_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u16(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else if constexpr(::std::same_as<I, ::std::uint8_t>)
            {
                ::std::uint8_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u8(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else
            {
                // msvc's `_addcarry_u32` does not specialize for `unsigned long`

                I res{static_cast<I>(i + add)};
                if(res < i) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }

# elif defined(_M_X32)
            if constexpr(::std::same_as<I, ::std::uint32_t>)
            {
                ::std::uint32_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u32(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else if constexpr(::std::same_as<I, ::std::uint16_t>)
            {
                ::std::uint16_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u16(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else if constexpr(::std::same_as<I, ::std::uint8_t>)
            {
                ::std::uint8_t res;
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                if(::fast_io::intrinsics::msvc::x86::_addcarry_u8(false, i, add, ::std::addressof(res))) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }
            else
            {
                I res{static_cast<I>(i + add)};
                if(res < i) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                return res;
            }

# else  // ARM, ARM64, ARM64ec
            I res{static_cast<I>(i + add)};
            if(res < i) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return res;
# endif
        }
        else
        {
            I res{static_cast<I>(i + add)};
            if(res < i) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return res;
        }

#elif UWVM_HAS_BUILTIN(__builtin_add_overflow)
        I res;
        // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
        if(__builtin_add_overflow(i, add, ::std::addressof(res))) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        return res;

#else
        I res{static_cast<I>(i + add)};
        if(res < i) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::counter_overflows;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        return res;
#endif
    }

    /// @brief i += add, Exception thrown on overflow
    template <::std::unsigned_integral I>
    UWVM_GNU_ALWAYS_INLINE_ARTIFICIAL inline constexpr I& counter_addass_throw_when_overflow(I & i,
                                                                                             I add,
                                                                                             ::std::byte const* section_curr,
                                                                                             ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
#if defined(__ARM_FEATURE_SME)
        __arm_streaming_compatible
#endif
    { return i = counter_add_throw_when_overflow(i, add, section_curr, err); }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
