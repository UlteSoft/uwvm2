/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      YexuanXiao
 * @version     2.0.0
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
# include <version>
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <concepts>
# include <memory>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::container
{
    template <typename T>
    struct fast_io_global_std_allocator
    {
        static_assert(::std::is_object_v<T> && !::std::is_const_v<T> && !std::is_volatile_v<T>);
        using value_type = T;
        using size_type = ::std::size_t;
        using difference_type = ::std::ptrdiff_t;
        using propagate_on_container_move_assignment = ::std::true_type;

        using fast_io_type_allocator = ::fast_io::native_typed_global_allocator<T>;

        inline constexpr fast_io_global_std_allocator() noexcept = default;

        inline constexpr fast_io_global_std_allocator(fast_io_global_std_allocator const&) noexcept = default;

        inline constexpr fast_io_global_std_allocator& operator= (fast_io_global_std_allocator const&) noexcept = default;

        template <typename U>
        inline constexpr fast_io_global_std_allocator(fast_io_global_std_allocator<U> const&) noexcept
        {
        }

        [[nodiscard]] inline constexpr T* allocate(::std::size_t n) noexcept { return fast_io_type_allocator::allocate(n); }

#if __cpp_lib_allocate_at_least >= 202302L
        [[nodiscard]] inline constexpr ::std::allocation_result<T*> allocate_at_least(::std::size_t n) noexcept
        {
            auto const [ptr, count]{fast_io_type_allocator::allocate_at_least(n)};
            return {ptr, count};
        }
#endif

        inline constexpr void deallocate(T* p, ::std::size_t n) noexcept { fast_io_type_allocator::deallocate_n(p, n); }

        inline constexpr ::std::size_t max_size() const noexcept { return ::std::numeric_limits<::std::size_t>::max() / sizeof(T); }
    };

    template <typename U, typename V>
    inline constexpr bool operator== (fast_io_global_std_allocator<U> const&, fast_io_global_std_allocator<V> const&) noexcept
    { return true; }

    template <typename T>
    struct fast_io_thread_local_std_allocator
    {
        static_assert(::std::is_object_v<T> && !::std::is_const_v<T> && !std::is_volatile_v<T>);
        using value_type = T;
        using size_type = ::std::size_t;
        using difference_type = ::std::ptrdiff_t;
        using propagate_on_container_move_assignment = ::std::true_type;

        using fast_io_type_allocator = ::fast_io::native_typed_thread_local_allocator<T>;

        inline constexpr fast_io_thread_local_std_allocator() noexcept = default;

        inline constexpr fast_io_thread_local_std_allocator(fast_io_thread_local_std_allocator const&) noexcept = default;

        inline constexpr fast_io_thread_local_std_allocator& operator= (fast_io_thread_local_std_allocator const&) noexcept = default;

        template <typename U>
        inline constexpr fast_io_thread_local_std_allocator(fast_io_thread_local_std_allocator<U> const&) noexcept
        {
        }

        [[nodiscard]] inline constexpr T* allocate(::std::size_t n) noexcept { return fast_io_type_allocator::allocate(n); }

#if __cpp_lib_allocate_at_least >= 202302L
        [[nodiscard]] inline constexpr ::std::allocation_result<T*> allocate_at_least(::std::size_t n) noexcept
        {
            auto const [ptr, count]{fast_io_type_allocator::allocate_at_least(n)};
            return {ptr, count};
        }
#endif

        inline constexpr void deallocate(T* p, ::std::size_t n) noexcept { fast_io_type_allocator::deallocate_n(p, n); }

        inline constexpr ::std::size_t max_size() const noexcept { return ::std::numeric_limits<::std::size_t>::max() / sizeof(T); }
    };

    template <typename U, typename V>
    inline constexpr bool operator== (fast_io_thread_local_std_allocator<U> const&, fast_io_thread_local_std_allocator<V> const&) noexcept
    { return true; }
}
