/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
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
# include <cstddef>
# include <cstdint>
# include <version>
# include <limits>
# include <memory>
# include <new>
# include <bit>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/allocator/fast_io_strict/impl.h>
# include <uwvm2/object/memory/wasm_page/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::linear
{
    /// @brief      The basic allocator memory class for single-thread usage.
    /// @defailt    Supports platforms without mmap or when custom_page is smaller than platform_page.
    template <typename Alloc>
    struct basic_single_thread_allocator_memory_t 
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view name{u8"single-thread allocator"};

        // The default allocator is unaligned.
        using allocator_t = Alloc;
        using strict_allocator_unadapted_t = ::uwvm2::utils::allocator::fast_io_strict::fast_io_allocator_to_strict<typename Alloc::allocator_type>;
        using strict_allocator_t = ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_generic_allocator_adapter<strict_allocator_unadapted_t>;

        /// @brief Ensure alignment. Typically, the maximum allowed alignment size for WASM memory operation instructions is 16 (v128). Here, align to the size
        ///        of a cache line, which is usually 64.
        /// @note  All WASM memory must be aligned when allocated and released.
        inline static constexpr ::std::size_t alignment{64uz};

        // data
        ::std::byte* memory_begin{};
        ::std::size_t memory_length{};

        unsigned custom_page_size_log2{};

        // constexpr data
        inline static constexpr bool can_mmap{false};
        inline static constexpr bool support_multi_thread{false};

        // function

        /// @brief      Default constructor.
        /// @note       The default page size is 65536 bytes.
        inline constexpr basic_single_thread_allocator_memory_t() noexcept
        {
            constexpr ::std::size_t default_wasm_page_size{static_cast<::std::size_t>(::uwvm2::object::memory::wasm_page::default_wasm32_page_size)};
            constexpr unsigned default_wasm_page_size_log2{static_cast<unsigned>(::std::countr_zero(default_wasm_page_size))};
            this->custom_page_size_log2 = default_wasm_page_size_log2;
        }

        inline constexpr basic_single_thread_allocator_memory_t(::std::size_t custom_page_size) noexcept
        {
            // Check if it is a power of 2
            if(!::std::has_single_bit(custom_page_size)) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Since there is only one pop, directly counting the number of zeros suffices to calculate log2
            this->custom_page_size_log2 = ::std::countr_zero(custom_page_size);
        }

        /// @brief      Initialize the memory.
        /// @note       Maximum value checks are not provided; maximum value checks should be performed outside of memory management.
        /// @note       This function is intended for single-thread usage and must not be called concurrently.
        /// @note       You can use it after clear().
        inline constexpr void init_by_page_count(::std::size_t init_page_count) noexcept
        {
            if(init_page_count > ::std::numeric_limits<::std::size_t>::max() >> this->custom_page_size_log2) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(this->memory_begin == nullptr) [[likely]]
            {
                // UB will never appear; it has been preemptively checked.
                this->memory_length = init_page_count << this->custom_page_size_log2;

                // Ensure alignment. Typically, the maximum allowed alignment size for WASM memory operation instructions is 16 (v128). Here, align to the size
                // of a cache line, which is usually 64.
                auto const temp_memory_begin{allocator_t::allocate_aligned_zero(alignment, this->memory_length)};

                this->memory_begin = ::std::assume_aligned<alignment>(reinterpret_cast<::std::byte*>(temp_memory_begin));
            }
        }

        /// @brief      Grow the memory.
        /// @param      max_limit_memory_length     This maximum value is derived from the maximum memory limit.
        inline constexpr void grow_silently(::std::size_t page_grow_size,
                                            ::std::size_t max_limit_memory_length = ::std::numeric_limits<::std::size_t>::max()) noexcept
        {
            if(page_grow_size == 0uz) [[unlikely]] { return; }

            if(this->memory_begin == nullptr) [[unlikely]]
            {
                // this is a bug
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            auto const this_custom_page_size_log2{this->custom_page_size_log2};

            if(page_grow_size > ::std::numeric_limits<::std::size_t>::max() >> this_custom_page_size_log2) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                // slient
                ::fast_io::fast_terminate();
            }

            auto const memory_grow_size{page_grow_size << this_custom_page_size_log2};
            auto const curr_memory_length{this->memory_length};

            if(max_limit_memory_length < curr_memory_length) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                // slient
                ::fast_io::fast_terminate();
            }

            auto const left_memory_size{max_limit_memory_length - curr_memory_length};

            if(memory_grow_size > left_memory_size) [[unlikely]]
            {
                // Exceeded the maximum allowed value, error reported.
                // slient
                ::fast_io::fast_terminate();
            }

            // After realloc, write new_memory_length
            auto const new_memory_length{curr_memory_length + memory_grow_size};

            ::std::byte* temp_memory_begin;  // no init required

            // All WASM memory must be aligned when allocated and released.
            if constexpr(allocator_t::has_reallocate_aligned_zero)
            {
                temp_memory_begin = reinterpret_cast<::std::byte*>(allocator_t::reallocate_aligned_zero(this->memory_begin, alignment, new_memory_length));

                // fast_io::allocator is slient, nonecessary check
            }
            else if constexpr(allocator_t::has_reallocate_aligned)
            {
                temp_memory_begin = reinterpret_cast<::std::byte*>(allocator_t::reallocate_aligned(this->memory_begin, alignment, new_memory_length));

                // fast_io::allocator is slient, nonecessary check

                // Manually clear the contents from the old boundary to the new boundary.
                ::fast_io::freestanding::bytes_clear_n(temp_memory_begin + curr_memory_length, memory_grow_size);
            }
            else
            {
                // General Implementation

                // Using `aligned zero` directly instead of `aligned` allocation followed by memory clearing leverages the platform's default page
                // initialization mechanism, which sets all pages to zero.
                temp_memory_begin = reinterpret_cast<::std::byte*>(allocator_t::allocate_aligned_zero(alignment, new_memory_length));

                // fast_io::allocator is slient, nonecessary check

                // Copy all old content to the new memory.
                ::fast_io::freestanding::my_memcpy(temp_memory_begin, this->memory_begin, curr_memory_length);

                // Deallocate the old memory.
                allocator_t::deallocate_aligned_n(this->memory_begin, alignment, curr_memory_length);
            }

            this->memory_begin = ::std::assume_aligned<alignment>(temp_memory_begin);
            this->memory_length = new_memory_length;
        }

        /// @brief     Strictly use a non-silent allocator (which may return nullptr), then indicates allocation success via the return value.
        inline constexpr bool grow_strictly(::std::size_t page_grow_size,
                                            ::std::size_t max_limit_memory_length = ::std::numeric_limits<::std::size_t>::max()) noexcept
        {
            if(page_grow_size == 0uz) [[unlikely]] { return true; }

            if(this->memory_begin == nullptr) [[unlikely]]
            {
                // this is a bug
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }

            auto const this_custom_page_size_log2{this->custom_page_size_log2};

            if(page_grow_size > ::std::numeric_limits<::std::size_t>::max() >> this_custom_page_size_log2) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                return false;
            }

            auto const memory_grow_size{page_grow_size << this_custom_page_size_log2};
            auto const curr_memory_length{this->memory_length};

            if(max_limit_memory_length < curr_memory_length) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                return false;
            }

            auto const left_memory_size{max_limit_memory_length - curr_memory_length};

            if(memory_grow_size > left_memory_size) [[unlikely]]
            {
                // Exceeded the maximum allowed value, error reported.
                return false;
            }

            // After realloc, write new_memory_length
            auto const new_memory_length{curr_memory_length + memory_grow_size};

            ::std::byte* temp_memory_begin;  // no init required

            // All WASM memory must be aligned when allocated and released.
            if constexpr(strict_allocator_t::has_reallocate_aligned_zero)
            {
                temp_memory_begin =
                    reinterpret_cast<::std::byte*>(strict_allocator_t::reallocate_aligned_zero(this->memory_begin, alignment, new_memory_length));

                if(temp_memory_begin == nullptr) [[unlikely]] { return false; }
            }
            else if constexpr(strict_allocator_t::has_reallocate_aligned)
            {
                temp_memory_begin = reinterpret_cast<::std::byte*>(strict_allocator_t::reallocate_aligned(this->memory_begin, alignment, new_memory_length));

                if(temp_memory_begin == nullptr) [[unlikely]] { return false; }

                // Manually clear the contents from the old boundary to the new boundary.
                ::fast_io::freestanding::bytes_clear_n(temp_memory_begin + curr_memory_length, memory_grow_size);
            }
            else
            {
                // General Implementation

                // Using `aligned zero` directly instead of `aligned` allocation followed by memory clearing leverages the platform's default page
                // initialization mechanism, which sets all pages to zero.
                temp_memory_begin = reinterpret_cast<::std::byte*>(strict_allocator_t::allocate_aligned_zero(alignment, new_memory_length));

                if(temp_memory_begin == nullptr) [[unlikely]] { return false; }

                // Copy all old content to the new memory.
                ::fast_io::freestanding::my_memcpy(temp_memory_begin, this->memory_begin, curr_memory_length);

                // Deallocate the old memory.
                strict_allocator_t::deallocate_aligned_n(this->memory_begin, alignment, curr_memory_length);
            }

            this->memory_begin = ::std::assume_aligned<alignment>(temp_memory_begin);
            this->memory_length = new_memory_length;

            return true;
        }

        inline constexpr ::std::size_t get_page_size() const noexcept { return this->memory_length >> this->custom_page_size_log2; }

        inline constexpr basic_single_thread_allocator_memory_t(basic_single_thread_allocator_memory_t const& other) noexcept = delete;

        inline constexpr basic_single_thread_allocator_memory_t& operator= (basic_single_thread_allocator_memory_t const& other) noexcept = delete;

        inline constexpr basic_single_thread_allocator_memory_t(basic_single_thread_allocator_memory_t&& other) noexcept
        {
            this->memory_begin = other.memory_begin;
            this->memory_length = other.memory_length;
            this->custom_page_size_log2 = other.custom_page_size_log2;

            // clear destroy other
            other.memory_begin = nullptr;
            other.memory_length = 0uz;
            other.custom_page_size_log2 = 0u;
        }

        inline constexpr basic_single_thread_allocator_memory_t& operator= (basic_single_thread_allocator_memory_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            clear_destroy();

            this->memory_begin = other.memory_begin;
            this->memory_length = other.memory_length;
            this->custom_page_size_log2 = other.custom_page_size_log2;

            // clear destroy other
            other.memory_begin = nullptr;
            other.memory_length = 0uz;
            other.custom_page_size_log2 = 0u;

            return *this;
        }

        /// @brief      Clear the memory.
        /// @note       Simply clearing the memory without altering the page size.
        inline constexpr void clear() noexcept
        {
            Alloc::deallocate_aligned_n(this->memory_begin, alignment, this->memory_length);  // dealloc includes built-in nullptr checking

            this->memory_length = 0uz;
            this->memory_begin = nullptr;
        }

        /// @brief      Clear the memory and reset state.
        /// @note       After calling this, you must reinitialize it according to the initialization process.
        inline constexpr void clear_destroy() noexcept
        {
            Alloc::deallocate_aligned_n(this->memory_begin, alignment, this->memory_length);  // dealloc includes built-in nullptr checking

            this->memory_length = 0uz;
            this->memory_begin = nullptr;
            this->custom_page_size_log2 = 0u;
        }

        inline constexpr ~basic_single_thread_allocator_memory_t()
        {
            Alloc::deallocate_aligned_n(this->memory_begin, alignment, this->memory_length);  // dealloc includes built-in nullptr checking
        }
    };

    using single_thread_allocator_memory_t = basic_single_thread_allocator_memory_t<::fast_io::native_global_allocator>;

}  // namespace uwvm2::object::memory::linear

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <typename Alloc>
    struct is_trivially_copyable_or_relocatable<::uwvm2::object::memory::linear::basic_single_thread_allocator_memory_t<Alloc>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
