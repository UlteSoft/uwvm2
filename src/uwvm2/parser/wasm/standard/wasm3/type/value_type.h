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
# include <climits>
# include <concepts>
# include <bit>
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
/// Core 3 ref.i31 retains precisely the low 31 bits of an i32. The host reference-kind
/// tag belongs to the enclosing runtime reference; no C++ bit-field layout or byte order
/// is part of the guest value. In particular, a signed 31-bit C++ bit-field has different
/// layout and even size across MSVC and GCC, so it cannot define the VM's cross-platform ABI.
/// https://webassembly.github.io/spec/core/exec/instructions.html#exec-ref.i31
    struct wasm_i31
    {
        static constexpr ::std::uint32_t value_mask{0x7fff'ffffu};
        static constexpr ::std::uint32_t sign_bit{0x4000'0000u};
        ::std::uint32_t bits;

        [[nodiscard]] static constexpr wasm_i31 from_i32(::std::int32_t value) noexcept
        { return {::std::bit_cast<::std::uint32_t>(value) & value_mask}; }

        [[nodiscard]] constexpr ::std::uint32_t get_u() const noexcept
        { return bits & value_mask; }

        [[nodiscard]] constexpr ::std::int32_t get_s() const noexcept
        {
            auto const low{get_u()};
            // Unsigned arithmetic wraps modulo 2^32, then bit_cast preserves the
            // two's-complement result without an implementation-defined signed cast.
            return ::std::bit_cast<::std::int32_t>((low ^ sign_bit) - sign_bit);
        }
    };
    static_assert(sizeof(wasm_i31) == sizeof(::std::uint32_t));
    static_assert(alignof(wasm_i31) == alignof(::std::uint32_t));
    static_assert(wasm_i31::from_i32(-1).get_s() == -1);
    static_assert(wasm_i31::from_i32(-1).get_u() == 0x7fff'ffffu);
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif
