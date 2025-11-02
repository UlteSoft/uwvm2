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
# include <climits>
# include <concepts>
# include <bit>
# include <limits>
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/precfloat/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::type
{
    /// @brief (Q) Why the base type uses (u)int_leastN_t?
    ///
    /// @details (A) :
    /// Here the use of (u)int_leastN_t is guaranteed to be greater than or equal to the specified number of bits and the closest of the optional types to the
    /// specified number of bits. On some platforms where CHAR_BIT is not 8 (the standard specifies that CHAR_BIT is greater than or equal to 8),
    /// (u)int_leastN_t guarantees that a data type that is greater than the specified number of bits and closest to the specified number of bits will be used,
    /// the It is guaranteed to hold just the specified number of bits needed, and is common across all platforms.
    ///
    /// (u)int_fastN_t is guaranteed to be the type that is greater than or equal to the specified number of bits and is the most isa-friendly type, e.g., on
    /// pdp11 int_fast8_t is a 16-bit type like int_fast16_t, but int_least8_t is an 8-bit type. So (u)int_fastN_t may be an arbitrary value, and care must be
    /// taken before using it, if there is a need for performance of some of the methods and there is a guarantee that the potentially arbitrary bit types of
    /// (u)int_fastN_t can be handled correctly.
    ///
    /// (u)intN_t, on the other hand, is a type where the number of bits is exactly equal to the number of bits specified, and may not be provided on some
    /// platforms where CHAR_BIT is greater than 8, so try to avoid using it within general-purpose code, and it can be used after qualifying the platform.
    ///
    /// For integer types other than char, unsigned char, and signed char, (short, unsigned short, int, unsigned, long, unsigned long, long long, unsigned long
    /// long). the standard does not specify the exact size.
    /// e.g.
    /// sizeof(signed char) == 1uz, sizeof(short) >= 2uz, sizeof(int) >= 2uz, sizeof(long) >= 4uz, sizeof(long long) >= 8uz
    /// sizeof(signed char) < sizeof(short) <= sizeof(int) <= sizeof(long) <= sizeof(long long)
    ///
    /// The standard specifies that size_t must be large enough to represent the size of the largest single object that may exist in the system. However, there
    /// is no hard constraint on the exact number of bits. So you need to judge when converting types to size_t.
    /// e.g. sizeof(::std::size_t) >= 2uz
    ///
    /// The standard does not explicitly require sizeof(::std::ptrdiff_t) ≤ sizeof(::std::size_t), but the design goal of the two implicitly implies a common
    /// relationship in actual implementations.

    // On systems with CHAR_BIT == 128, uint_least8_t, uint_least16_t, uint_least32_t, and uint_least64_t can be the same 128-bit type, which is allowed by the
    // standard and is the logical implementation. However, this can lead to conceptual errors at this point, so it needs to be avoided.
    static_assert(!::std::same_as<::std::uint_least8_t, ::std::uint_least16_t> && !::std::same_as<::std::uint_least8_t, ::std::uint_least32_t> &&
                  !::std::same_as<::std::uint_least8_t, ::std::uint_least64_t> && !::std::same_as<::std::uint_least16_t, ::std::uint_least32_t> &&
                  !::std::same_as<::std::uint_least16_t, ::std::uint_least64_t> && !::std::same_as<::std::uint_least32_t, ::std::uint_least64_t>);

    /// @brief      Bytes
    /// @details    The simplest form of value are raw uninterpreted bytes. In the abstract syntax they are represented as hexadecimal
    ///             literals.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.2.1
    using wasm_byte = ::std::uint_least8_t;

    /// @brief      Integers
    /// @details    Different classes of integers with different value ranges are distinguished by their bit width N and by whether they
    ///             are unsigned or signed.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.2.2
    using wasm_i8 = ::std::int_least8_t;   // nonstandard
    using wasm_u8 = ::std::uint_least8_t;  // nonstandard

    using wasm_i16 = ::std::int_least16_t;   // nonstandard
    using wasm_u16 = ::std::uint_least16_t;  // nonstandard

    using wasm_i32 = ::std::int_least32_t;
    using wasm_u32 = ::std::uint_least32_t;

    using wasm_i64 = ::std::int_least64_t;
    using wasm_u64 = ::std::uint_least64_t;

    /// @brief      Floating-Point
    /// @details    Floating-point data represents 32 or 64 bit values that correspond to the respective binary formats of the IEEE
    ///             754-20089 standard (Section 3.3).
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.2.3
    using error_f32 = ::uwvm2::utils::precfloat::float32_t;
    using error_f64 = ::uwvm2::utils::precfloat::float64_t;

    /// @brief      wasm base instruction, using a byte composition, instruction or extension instruction.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.4.5
    using op_basic_type = wasm_byte;

    /// @brief      wasm extended instructions, using u32, indicate the extended instruction section.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.4.5
    using op_exten_type = wasm_u32;

    // func
    namespace details
    {
        /// @brief      Type of varint converted to fixed-length int
        /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.2.2
        template <::std::size_t Varint, bool Unsigned>
            requires (0 < Varint && Varint <= 64)
        inline consteval auto get_varint_type_from_type_bit_size_impl() noexcept
        {
            if constexpr(Varint <= 8)
            {
                if constexpr(Unsigned) { return wasm_u8{}; }
                else
                {
                    return wasm_i8{};
                }
            }
            else if constexpr(Varint <= 16)
            {
                if constexpr(Unsigned) { return wasm_u16{}; }
                else
                {
                    return wasm_i16{};
                }
            }
            else if constexpr(Varint <= 32)
            {
                if constexpr(Unsigned) { return wasm_u32{}; }
                else
                {
                    return wasm_i32{};
                }
            }
            else if constexpr(Varint <= 64)
            {
                if constexpr(Unsigned) { return wasm_u64{}; }
                else
                {
                    return wasm_i64{};
                }
            }
        }

        /// @brief      Getting Variable Types from Maximum Storage Capacity
        /// @details    All integers are encoded using the LEB12829 variable-length integer encoding, in either unsigned or signed variant.
        ///             Unsigned integers are encoded in unsigned LEB12830 format. As an additional constraint, the total number of
        ///             bytes encoding a value of type uN must not exceed ceil(N / 7) bytes
        ///             Signed integers are encoded in signed LEB12831 format, which uses a two’s complement representation. As an
        ///             additional constraint, the total number of bytes encoding a value of type sN must not exceed ceil(N / 7) bytes.
        /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.2.2
        template <::std::size_t StorageSize, bool Unsigned>
            requires (0 < StorageSize && StorageSize <= 10)
        inline consteval auto get_varint_type_from_max_storage_size_impl() noexcept
        {
            if constexpr(StorageSize <= 1)
            {
                if constexpr(Unsigned) { return wasm_u8{}; }
                else
                {
                    return wasm_i8{};
                }
            }
            else if constexpr(StorageSize <= 3)
            {
                if constexpr(Unsigned) { return wasm_u16{}; }
                else
                {
                    return wasm_i16{};
                }
            }
            else if constexpr(StorageSize <= 5)
            {
                if constexpr(Unsigned) { return wasm_u32{}; }
                else
                {
                    return wasm_i32{};
                }
            }
            else if constexpr(StorageSize <= 10)
            {
                if constexpr(Unsigned) { return wasm_u64{}; }
                else
                {
                    return wasm_i64{};
                }
            }
        }
    }  // namespace details

    template <::std::size_t Varint, bool Unsigned>
    using varint_type_from_type_bsz = decltype(details::get_varint_type_from_type_bit_size_impl<Varint, Unsigned>());

    template <::std::size_t StorageSize, bool Unsigned>
    using varint_type_from_max_ssz = decltype(details::get_varint_type_from_max_storage_size_impl<StorageSize, Unsigned>());
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif
