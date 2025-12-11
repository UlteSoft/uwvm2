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
# include <atomic>
# include <bit>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/object/memory/wasm_page/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if __cpp_lib_atomic_wait >= 201907L

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::linear
{
    /// @brief      The guard for the growing flag.
    /// @note       This guard is only used in `grow` operations to acquire exclusive access to memory.
    /// @details    Memory ordering ensures proper synchronization:
    ///             1. test_and_set(acquire): Establishes lock ownership and prevents later accesses from moving before lock
    ///             2. wait(acquire): Ensures that when woken up, the thread sees all memory updates from the grow operation
    ///             3. clear(release): Publishes all critical section updates before unlocking
    ///             4. notify_all(): Wakes up waiting threads after memory updates are published
    struct growing_flag_guard_t
    {
        ::std::atomic_flag* growing_flag_p{};

        inline constexpr growing_flag_guard_t(::std::atomic_flag* other_growing_flag_p) noexcept : growing_flag_p{other_growing_flag_p}
        {
            if(this->growing_flag_p != nullptr) [[likely]]
            {
                // acquire: establish exclusive lock ownership; prevent later accesses from moving before lock
                // acquire: waiting must use acquire to see memory updates from grow operation

                unsigned spin_count{};

                for(;;)
                {
                    if(!this->growing_flag_p->test_and_set(::std::memory_order_acquire)) { break; }

                    if(++spin_count > 1000u)
                    {
                        // Long-term contention: sleep until the grow flag is cleared.
                        this->growing_flag_p->wait(true, ::std::memory_order_acquire);
                        spin_count = 0u;
                    }
                    else
                    {
                        ::uwvm2::utils::mutex::rwlock_pause();
                    }
                }
            }
        }

        inline constexpr growing_flag_guard_t(growing_flag_guard_t const& other) noexcept = delete;
        inline constexpr growing_flag_guard_t& operator= (growing_flag_guard_t const& other) noexcept = delete;
        inline constexpr growing_flag_guard_t(growing_flag_guard_t&& other) noexcept = delete;
        inline constexpr growing_flag_guard_t& operator= (growing_flag_guard_t&& other) noexcept = delete;

        inline constexpr ~growing_flag_guard_t()
        {
            if(this->growing_flag_p != nullptr) [[likely]]
            {
                // release: publish critical-section updates before unlocking
                // This ensures that all memory modifications made during grow are visible to threads
                // that will be woken up by the subsequent notify_all()
                this->growing_flag_p->clear(::std::memory_order_release);

                // Notify all waiting threads that the lock has been released
                // The release semantics of clear() ensure that waiting threads will see
                // all memory updates made during the critical section
                this->growing_flag_p->notify_all();
            }
        }
    };

    /// @brief      The guard for memory operations (read/write instructions).
    /// @note       This guard implements the enter/exit protocol for memory operations to ensure thread safety
    ///             during memory growth operations. It prevents race conditions between memory operations and
    ///             the memory relocation process.
    /// @details    The protocol ensures:
    ///             1. Memory operations wait for any ongoing grow operation to complete
    ///             2. Active operations are properly counted to prevent grow from starting while operations are in progress
    ///             3. Double-check mechanism prevents race conditions where grow starts immediately after entry
    ///             4. Proper memory ordering (acquire/release) ensures visibility of memory updates
    struct memory_operation_guard_t
    {
        ::std::atomic_flag* growing_flag_p{};
        ::std::atomic_size_t* active_ops_p{};

        inline constexpr memory_operation_guard_t(::std::atomic_flag* other_growing_flag_p, ::std::atomic_size_t* other_active_ops_p) noexcept :
            growing_flag_p{other_growing_flag_p}, active_ops_p{other_active_ops_p}
        {
            // Since this is a path frequently accessed during WASM execution, we should strive to avoid branches related to the virtual machine's own bug
            // checks (which are verified during debugging).

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(this->growing_flag_p == nullptr || this->active_ops_p == nullptr) [[unlikely]]
            {
                // This is a bug in uwvm rather than a bug in the program running in WASM.
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }
# endif

            enter_operation();
        }

        inline constexpr memory_operation_guard_t(memory_operation_guard_t const& other) noexcept = delete;
        inline constexpr memory_operation_guard_t& operator= (memory_operation_guard_t const& other) noexcept = delete;

        inline constexpr memory_operation_guard_t(memory_operation_guard_t&& other) noexcept
        {
            // Directly take over the lock from the other process.
            this->growing_flag_p = other.growing_flag_p;
            this->active_ops_p = other.active_ops_p;

            other.growing_flag_p = nullptr;
            other.active_ops_p = nullptr;
        }

        inline constexpr memory_operation_guard_t& operator= (memory_operation_guard_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // Wait for the end of its own lock, then acquire the lock on other.
            // This function includes built-in null pointer checks.
            exit_operation();

            this->growing_flag_p = other.growing_flag_p;
            this->active_ops_p = other.active_ops_p;

            other.growing_flag_p = nullptr;
            other.active_ops_p = nullptr;

            return *this;
        }

        inline constexpr ~memory_operation_guard_t() { exit_operation(); }

        /// @brief      Enter the memory operation safely.
        /// @details    Implements the enter protocol to prevent race conditions:
        ///             1. Wait for grow operation to complete (acquire semantics)
        ///             2. Increment active operations counter (acq_rel semantics)
        ///             3. Double-check that grow hasn't started (acquire semantics)
        ///             4. If grow started, undo increment and retry
        inline constexpr void enter_operation() noexcept
        {
            // Due to the inclusion of mobile semantics, null pointer checks must be performed.
            if(this->growing_flag_p == nullptr || this->active_ops_p == nullptr) [[unlikely]] { return; }

            unsigned spin_count{};

            for(;;)
            {
                // 1) Wait for grow operation to complete, must use acquire to see memory updates
                while(this->growing_flag_p->test(::std::memory_order_acquire))
                {
                    if(++spin_count > 1000u)
                    {
                        this->growing_flag_p->wait(true, ::std::memory_order_acquire);
                        spin_count = 0u;
                    }
                    else
                    {
                        ::uwvm2::utils::mutex::rwlock_pause();
                    }
                }

                // 2) Declare entry into active region; relaxed is sufficient for pure reference counting
                this->active_ops_p->fetch_add(1uz, ::std::memory_order_relaxed);

                // 3) Double-check that grow hasn't started after we "entered"
                if(!this->growing_flag_p->test(::std::memory_order_acquire))
                {
                    break;  // Successfully entered, grow is not active
                }

                // 4) Grow started after we entered: undo increment and retry
                this->active_ops_p->fetch_sub(1uz, ::std::memory_order_release);

                // Only one grow can await active_ops: the grow path holds growing_flag exclusively.
                // Other grow attempts block on growing_flag->wait until the flag is cleared.
                // Thus the waiter set for active_ops==0 contains at most one grow thread.
                // Wake a single grow waiter to re-check the counter; at most one grow can await active_ops
                this->active_ops_p->notify_one();  // Wake up a waiting grow operation
            }
        }

        /// @brief      Exit the memory operation safely.
        /// @details    Implements the exit protocol:
        ///             1. Decrement active operations counter (release semantics)
        ///             2. Notify waiting grow operations
        inline constexpr void exit_operation() noexcept
        {
            // Due to the inclusion of mobile semantics, null pointer checks must be performed.
            if(this->growing_flag_p == nullptr || this->active_ops_p == nullptr) [[unlikely]] { return; }

            // Decrement counter with release semantics to publish our updates
            this->active_ops_p->fetch_sub(1uz, ::std::memory_order_release);

            // Only one grow can await active_ops: the grow path holds growing_flag exclusively.
            // Other grow attempts block on growing_flag->wait until the flag is cleared.
            // Thus the waiter set for active_ops==0 contains at most one grow thread.
            // Wake a single grow waiter to re-check the counter; at most one grow can await active_ops
            this->active_ops_p->notify_one();
        }
    };

    /// @brief   Safe Memory Growth under Concurrency for Linear Memory (Non-mmap Fallback)
    /// @details When linear memory growth requires relocating the underlying buffer (e.g., realloc or dealloc + alloc + memcpy on platforms without mmap),
    ///          concurrent loads/stores—including atomic RMW operations—may race with the relocation process. A pathological case arises when:
    ///
    ///          - Thread A acquires a cache line with an atomic operation.
    ///          - Thread B contends on the same cache line with another atomic.
    ///          - Thread C performs memory.grow, which relocates the buffer and stalls while copying the very cache line.
    ///          - After A releases the line, B and C race; if C proceeds first and copies “old” data into the new buffer, then B writes to the old buffer,
    ///            yielding divergence between the new buffer and reality.
    ///
    ///          Therefore, the grow_flag must be set during the grow phase to prevent new threads from attempting to read or write memory. Reference counting
    ///          is performed for instructions currently being read or written. When the reference count reaches zero, the grow process begins.
    ///
    /// @note    Usage example for memory operations:
    ///          ```cpp
    ///          void memory_read_operation() {
    ///              memory_operation_guard_t guard{this->growing_flag_p, this->active_ops_p};
    ///              // Safe to access memory here - guard ensures thread safety
    ///              // Memory access code...
    ///          }  // Guard automatically exits when function ends
    ///          ```

    /// @brief      Summary of thread safety mechanisms for alloca path
    /// @details    This module provides two complementary protection mechanisms:
    ///             1. growing_flag_guard_t: Used by grow() operations to acquire exclusive access
    ///             2. memory_operation_guard_t: Used by memory operations (read/write) to safely enter/exit
    ///
    ///             The combination ensures:
    ///             - No memory operation can start while grow is in progress
    ///             - No grow can start while memory operations are active
    ///             - Proper memory ordering for visibility of updates
    ///             - Automatic cleanup via RAII pattern

    /// @brief      The basic allocator memory class.
    /// @defailt    The allocator memory supports the following scenarios: First, platforms that do not support mmap. Second, when the size of custom_page is
    ///             smaller than platform_page.
    template <typename Alloc>
    struct basic_allocator_memory_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view name{u8"mult-thread allocator"};

        // The default allocator is unaligned.
        using allocator_t = Alloc;
        // A type allocator must be an aligned allocator.
        using atomic_flag_allcator_t = ::fast_io::typed_generic_allocator_adapter<allocator_t, ::std::atomic_flag>;
        using atomic_size_allcator_t = ::fast_io::typed_generic_allocator_adapter<allocator_t, ::std::atomic_size_t>;

        /// @brief Ensure alignment. Typically, the maximum allowed alignment size for WASM memory operation instructions is 16 (v128). Here, align to the size
        ///        of a cache line, which is usually 64.
        /// @note  All WASM memory must be aligned when allocated and released.
        inline static constexpr ::std::size_t alignment{64uz};

        // data
        ::std::byte* memory_begin{};
        ::std::size_t memory_length{};

        unsigned custom_page_size_log2{};

        // Querying lock status itself can cause race conditions, so a double-atom model is used here.
        ::std::atomic_flag* growing_flag_p{};
        ::std::atomic_size_t* active_ops_p{};
        // constexpr data

        /// @brief If mmap is not possible, it indicates that realloc is required. This means the content may grow, potentially changing the base address,
        ///        necessitating lock control.
        inline static constexpr bool can_mmap{false};

        /// @brief If multithreading is not supported, it cannot be used in a multithreaded environment. The reason for supporting multithreading here is to
        ///        accommodate certain embedded platforms that do not support mmap but do support multithreading.
        inline static constexpr bool support_multi_thread{true};

        // function

        /// @brief      Default constructor.
        /// @note       The default page size is 65536 bytes.
        inline constexpr basic_allocator_memory_t() noexcept
        {
            this->growing_flag_p = atomic_flag_allcator_t::allocate(1uz);
            ::new(this->growing_flag_p)::std::atomic_flag{};

            this->active_ops_p = atomic_size_allcator_t::allocate(1uz);
            ::new(this->active_ops_p)::std::atomic_size_t{};

            constexpr ::std::size_t default_wasm_page_size{static_cast<::std::size_t>(::uwvm2::object::memory::wasm_page::default_wasm32_page_size)};
            constexpr unsigned default_wasm_page_size_log2{static_cast<unsigned>(::std::countr_zero(default_wasm_page_size))};
            this->custom_page_size_log2 = default_wasm_page_size_log2;
        }

        inline constexpr basic_allocator_memory_t(::std::size_t custom_page_size) noexcept
        {
            this->growing_flag_p = atomic_flag_allcator_t::allocate(1uz);
            ::new(this->growing_flag_p)::std::atomic_flag{};

            this->active_ops_p = atomic_size_allcator_t::allocate(1uz);
            ::new(this->active_ops_p)::std::atomic_size_t{};

            // The same method as set_custom_page_size, but without adding a lock.

            // Check if it is a power of 2
            if(!::std::has_single_bit(custom_page_size)) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Since there is only one pop, directly counting the number of zeros suffices to calculate log2
            this->custom_page_size_log2 = ::std::countr_zero(custom_page_size);
        }

        /// @brief      Initialize the memory.
        /// @note       Maximum value checks are not provided; maximum value checks should be performed outside of memory management.
        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done before the
        ///             WASM execution.
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
        inline constexpr void grow(::std::size_t page_grow_size, ::std::size_t max_limit_memory_length = ::std::numeric_limits<::std::size_t>::max()) noexcept
        {
            if(page_grow_size == 0uz) [[unlikely]] { return; }

            // Prevent new memory operation instructions from being read for speculation
            growing_flag_guard_t growing_flag_guard{this->growing_flag_p};

            // Stop-the-world: wait for all in-flight operations to finish

            if(this->active_ops_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Wait for all existing memory read instructions to complete.
            // acquire: observe decrements published with release; ensures quiescence is visible
            unsigned spin_count{};

            for(auto v{this->active_ops_p->load(::std::memory_order_acquire)}; v != 0uz; v = this->active_ops_p->load(::std::memory_order_acquire))
            {
                if(++spin_count > 1000u)
                {
                    // acquire: pair with operation's release decrement before proceeding after wake
                    this->active_ops_p->wait(v, ::std::memory_order_acquire);
                    spin_count = 0u;
                }
                else
                {
                    ::uwvm2::utils::mutex::rwlock_pause();
                }
            }

            if(this->memory_begin == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const this_custom_page_size_log2{this->custom_page_size_log2};

            if(page_grow_size > ::std::numeric_limits<::std::size_t>::max() >> this_custom_page_size_log2) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                ::fast_io::fast_terminate();
            }

            auto const memory_grow_size{page_grow_size << this_custom_page_size_log2};
            auto const curr_memory_length{this->memory_length};

            if(max_limit_memory_length < curr_memory_length) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                ::fast_io::fast_terminate();
            }

            auto const left_memory_size{max_limit_memory_length - curr_memory_length};

            if(memory_grow_size > left_memory_size) [[unlikely]]
            {
                // Exceeded the maximum allowed value, error reported.
                ::fast_io::fast_terminate();
            }

            // After realloc, write new_memory_length
            auto const new_memory_length{curr_memory_length + memory_grow_size};

            // In this model, regardless of whether `memory_begin` changes, no thread safety issues will arise.
            // Therefore, realloc_zero is used by default here.
            ::std::byte* temp_memory_begin;  // no init required

            // All WASM memory must be aligned when allocated and released.
            if constexpr(allocator_t::has_reallocate_aligned_zero)
            {
                // Very few platforms directly support

                temp_memory_begin = reinterpret_cast<::std::byte*>(allocator_t::reallocate_aligned_zero(this->memory_begin, alignment, new_memory_length));
            }
            else if constexpr(allocator_t::has_reallocate_aligned)
            {
                // Very few platforms directly support

                temp_memory_begin = reinterpret_cast<::std::byte*>(allocator_t::reallocate_aligned(this->memory_begin, alignment, new_memory_length));

                // Manually clear the contents from the old boundary to the new boundary.
                ::fast_io::freestanding::bytes_clear_n(temp_memory_begin + curr_memory_length, memory_grow_size);
            }
            else
            {
                // General Implementation

                // Using `aligned zero` directly instead of `aligned` allocation followed by memory clearing leverages the platform's default page
                // initialization mechanism, which sets all pages to zero.
                temp_memory_begin = reinterpret_cast<::std::byte*>(allocator_t::allocate_aligned_zero(alignment, new_memory_length));

                // Copy all old content to the new memory.
                ::fast_io::freestanding::my_memcpy(temp_memory_begin, this->memory_begin, curr_memory_length);

                // Deallocate the old memory.
                allocator_t::deallocate_aligned_n(this->memory_begin, alignment, curr_memory_length);
            }

            this->memory_begin = ::std::assume_aligned<alignment>(temp_memory_begin);
            this->memory_length = new_memory_length;

            // growing_flag_guard destruct here
        }

        inline constexpr ::std::size_t get_page_size() const noexcept
        {
            memory_operation_guard_t memory_op_guard{this->growing_flag_p, this->active_ops_p};
            // UB will never appear; it has been preemptively checked.

            // memory_op_guard destruct here
            return this->memory_length >> this->custom_page_size_log2;
        }

        inline constexpr basic_allocator_memory_t(basic_allocator_memory_t const& other) noexcept = delete;

        inline constexpr basic_allocator_memory_t& operator= (basic_allocator_memory_t const& other) noexcept = delete;

        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done before the
        ///             WASM execution.
        inline constexpr basic_allocator_memory_t(basic_allocator_memory_t&& other) noexcept
        {
            this->memory_begin = other.memory_begin;
            this->memory_length = other.memory_length;
            this->custom_page_size_log2 = other.custom_page_size_log2;
            this->growing_flag_p = other.growing_flag_p;
            this->active_ops_p = other.active_ops_p;

            // clear destory other
            other.memory_begin = nullptr;
            other.memory_length = 0uz;
            other.custom_page_size_log2 = 0u;
            other.growing_flag_p = nullptr;
            other.active_ops_p = nullptr;
        }

        /// @note      This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done before the
        ///             WASM execution.
        inline constexpr basic_allocator_memory_t& operator= (basic_allocator_memory_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            clear_destroy();

            this->memory_begin = other.memory_begin;
            this->memory_length = other.memory_length;
            this->custom_page_size_log2 = other.custom_page_size_log2;
            this->growing_flag_p = other.growing_flag_p;
            this->active_ops_p = other.active_ops_p;

            // clear destory other
            other.memory_begin = nullptr;
            other.memory_length = 0uz;
            other.custom_page_size_log2 = 0u;
            other.growing_flag_p = nullptr;
            other.active_ops_p = nullptr;

            return *this;
        }

        /// @brief      Clear the memory.
        /// @note       Simply clearing the memory without altering the page size.
        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done after the
        ///             WASM execution.
        inline constexpr void clear() noexcept
        {
            Alloc::deallocate_aligned_n(this->memory_begin, alignment, this->memory_length);  // dealloc includes built-in nullptr checking

            this->memory_length = 0uz;
            this->memory_begin = nullptr;
        }

        /// @brief      Clear the memory and destroy all flag.
        /// @note       After calling this, you must reinitialize it according to the initialization process.
        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done after the
        ///             WASM execution.
        inline constexpr void clear_destroy() noexcept
        {
            Alloc::deallocate_aligned_n(this->memory_begin, alignment, this->memory_length);  // dealloc includes built-in nullptr checking

            if(this->growing_flag_p != nullptr) [[likely]] { ::std::destroy_at(this->growing_flag_p); }
            atomic_flag_allcator_t::deallocate_n(this->growing_flag_p, 1uz);  // dealloc includes built-in nullptr checking

            if(this->active_ops_p != nullptr) [[likely]] { ::std::destroy_at(this->active_ops_p); }
            atomic_size_allcator_t::deallocate_n(this->active_ops_p, 1uz);  // dealloc includes built-in nullptr checking

            this->memory_length = 0uz;
            this->memory_begin = nullptr;
            this->custom_page_size_log2 = 0u;
            this->growing_flag_p = nullptr;
            this->active_ops_p = nullptr;
        }

        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done after the
        ///             WASM execution.
        inline constexpr ~basic_allocator_memory_t()
        {
            Alloc::deallocate_aligned_n(this->memory_begin, alignment, this->memory_length);  // dealloc includes built-in nullptr checking

            if(this->growing_flag_p != nullptr) [[likely]] { ::std::destroy_at(this->growing_flag_p); }
            atomic_flag_allcator_t::deallocate_n(this->growing_flag_p, 1uz);  // dealloc includes built-in nullptr checking

            if(this->active_ops_p != nullptr) [[likely]] { ::std::destroy_at(this->active_ops_p); }
            atomic_size_allcator_t::deallocate_n(this->active_ops_p, 1uz);  // dealloc includes built-in nullptr checking

            // multiple call to destructor is undefined behavior, so never set to default value
        }
    };

    using allocator_memory_t = basic_allocator_memory_t<::fast_io::native_global_allocator>;

}  // namespace uwvm2::object::memory::linear

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <typename Alloc>
    struct is_trivially_copyable_or_relocatable<::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc>>
    {
        inline static constexpr bool value = true;
    };
}

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
