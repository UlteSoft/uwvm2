/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <bit>
# include <cstddef>
# include <cstdint>
# include <type_traits>
# include <uwvm2/utils/macro/push_macros.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared::wasm_threads
{
    // Guest bytes use compiler atomic builtins, not atomic_ref<T> over a
    // fabricated C++ T object. Natural alignment and a live memory reservation
    // are established by the caller. may_alias permits typed access to the raw
    // linear-memory allocation; the compiler's atomic operation owns the access.
    template <::std::size_t Bytes>
    struct atomic_access_traits
    {
        static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
        using type UWVM_GNU_MAY_ALIAS = ::std::conditional_t<Bytes == 1uz, ::std::uint8_t,
            ::std::conditional_t<Bytes == 2uz, ::std::uint16_t,
            ::std::conditional_t<Bytes == 4uz, ::std::uint32_t, ::std::uint64_t>>>;
    };

#if defined(__clang__) || defined(__GNUC__)
    template <>
    struct atomic_access_traits<8uz>
    {
        // 32-bit ABIs may give uint64_t only 4-byte type alignment even though
        // Wasm atomics require 8. Native linear-memory bases and checked guest
        // offsets provide 8; expose that fact to the builtin's access type so
        // i386 emits an inline atomic instruction instead of a libatomic call.
        using type UWVM_GNU_MAY_ALIAS __attribute__((aligned(8))) = ::std::uint64_t;
    };
#endif

    template <::std::size_t Bytes>
    UWVM_ALWAYS_INLINE inline ::std::uint64_t atomic_load_le(::std::byte const* pointer) noexcept
    {
        static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
        using access_type = typename atomic_access_traits<Bytes>::type;
        // [live allocation/reservation ... aligned Bytes ...]
        //                                  ^^ borrowed pointer; a hardware
        // guard may trap on an uncommitted page, before any result is produced.
        auto value{__atomic_load_n(reinterpret_cast<access_type const*>(pointer), __ATOMIC_SEQ_CST)};
        if constexpr(Bytes != 1uz && ::std::endian::native == ::std::endian::big) { value = ::std::byteswap(value); }
        return value;
    }

    template <::std::size_t Bytes>
    UWVM_ALWAYS_INLINE inline void atomic_store_le(::std::byte* pointer, ::std::uint64_t input) noexcept
    {
        static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
        using access_type = typename atomic_access_traits<Bytes>::type;
        auto value{static_cast<access_type>(input)}; // Guest narrow stores truncate high bits.
        if constexpr(Bytes != 1uz && ::std::endian::native == ::std::endian::big) { value = ::std::byteswap(value); }
        // [live allocation/reservation ... aligned Bytes ...]
        //                                  ^^ borrowed pointer; the caller proves
        // the complete range or a hardware guard prevents this atomic write.
        __atomic_store_n(reinterpret_cast<access_type*>(pointer), value, __ATOMIC_SEQ_CST);
    }

    enum class atomic_rmw_operation { add, sub, and_, or_, xor_, exchange, compare_exchange };

    // All widths return the old guest value, zero-extended. The expected and
    // replacement operands of narrow cmpxchg are both wrapped to Bytes first.
    // The builtins are sequentially consistent on success AND on a failed CAS.
    template <atomic_rmw_operation Operation, ::std::size_t Bytes>
    UWVM_ALWAYS_INLINE inline ::std::uint64_t atomic_rmw_le(::std::byte* pointer, ::std::uint64_t input,
                                                          ::std::uint64_t expected_input = 0u) noexcept
    {
        static_assert(Bytes == 1uz || Bytes == 2uz || Bytes == 4uz || Bytes == 8uz);
        using access_type = typename atomic_access_traits<Bytes>::type;
        constexpr bool swap{Bytes != 1uz && ::std::endian::native == ::std::endian::big};
        auto endian{[](access_type value) noexcept -> access_type
        {
            if constexpr(swap) { return ::std::byteswap(value); }
            else { return value; }
        }};
        // [live allocation/reservation ... aligned Bytes ...]
        //                                  ^^ location borrows the validated raw
        // allocation; the caller has proved the whole bounds/guard and alignment.
        auto const location{reinterpret_cast<access_type*>(pointer)};
        auto const operand{static_cast<access_type>(input)};
        access_type old{};
        if constexpr(Operation == atomic_rmw_operation::compare_exchange)
        {
            old = endian(static_cast<access_type>(expected_input));
            (void)__atomic_compare_exchange_n(location, &old, endian(operand), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
        else if constexpr(Operation == atomic_rmw_operation::exchange)
        { old = __atomic_exchange_n(location, endian(operand), __ATOMIC_SEQ_CST); }
        else if constexpr(Operation == atomic_rmw_operation::and_)
        { old = __atomic_fetch_and(location, endian(operand), __ATOMIC_SEQ_CST); }
        else if constexpr(Operation == atomic_rmw_operation::or_)
        { old = __atomic_fetch_or(location, endian(operand), __ATOMIC_SEQ_CST); }
        else if constexpr(Operation == atomic_rmw_operation::xor_)
        { old = __atomic_fetch_xor(location, endian(operand), __ATOMIC_SEQ_CST); }
        else if constexpr(!swap && Operation == atomic_rmw_operation::add)
        { old = __atomic_fetch_add(location, operand, __ATOMIC_SEQ_CST); }
        else if constexpr(!swap && Operation == atomic_rmw_operation::sub)
        { old = __atomic_fetch_sub(location, operand, __ATOMIC_SEQ_CST); }
        else
        {
            static_assert(swap && (Operation == atomic_rmw_operation::add || Operation == atomic_rmw_operation::sub));
            // Byte swap does not commute with carries/borrows. Compute guest
            // little-endian arithmetic inside a CAS loop on the native bit pattern.
            old = __atomic_load_n(location, __ATOMIC_SEQ_CST);
            for(;;)
            {
                auto const guest{static_cast<::std::uint64_t>(endian(old))};
                auto const replacement{endian(static_cast<access_type>(Operation == atomic_rmw_operation::add ? guest + input : guest - input))};
                if(__atomic_compare_exchange_n(location, &old, replacement, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) { break; }
                // Failed CAS refreshes old; location and its pin remain unchanged.
            }
        }
        return endian(old);
    }

}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
