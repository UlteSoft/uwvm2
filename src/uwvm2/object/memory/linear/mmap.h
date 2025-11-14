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
# include <climits>
# include <limits>
# include <memory>
# include <new>
# include <atomic>
# include <bit>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if defined(UWVM_SUPPORT_MMAP) && !(defined(_WIN32) || defined(__CYGWIN__))  // posix mmap
#  include <sys/mman.h>
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/object/memory/wasm_page/impl.h>
# include <uwvm2/object/memory/platform_page/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_SUPPORT_MMAP)
# if CHAR_BIT != 8
#  error "mmap unsupported platform (The POSIX standard requires CHAR_BIT == 8)"
# endif
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::linear
{
#if defined(UWVM_SUPPORT_MMAP)

    enum class mmap_memory_status_t : unsigned
    {
        wasm32,
        wasm64
    };

    // max_full_protection_wasm64_length not supported
    inline constexpr ::std::uint_least64_t max_partial_protection_wasm64_length{static_cast<::std::uint_least64_t>(1u) << 48u};  // 256 TB
    inline constexpr ::std::uint_least64_t max_full_protection_wasm32_length{static_cast<::std::uint_least64_t>(1u) << 33u};     // 8 GB
    inline constexpr ::std::uint_least32_t max_partial_protection_wasm32_length{static_cast<::std::uint_least32_t>(1u) << 28u};  // 256 MB

    struct mmap_memory_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view name{u8"mmap"};

        using allocator_t = ::fast_io::native_global_allocator;
        using atomic_size_allcator_t = ::fast_io::typed_generic_allocator_adapter<allocator_t, ::std::atomic_size_t>;
        using mutex_allcator_t = ::fast_io::typed_generic_allocator_adapter<allocator_t, ::uwvm2::utils::mutex::mutex_t>;

        ::std::byte* memory_begin{};
        ::std::atomic_size_t* memory_length_p{};
        // This memory_size is not used in the program unless custom_page is smaller than platform_page.
        // The number of platform pages can be obtained by rounding up the memory size to the nearest integer multiple of the platform page size.

        unsigned custom_page_size_log2{};
        mmap_memory_status_t status{};

        // This lock is used to prevent multithreaded growth.
        ::uwvm2::utils::mutex::mutex_t* growing_mutex_p{};

        /// @brief This macro is used to control the behavior of the non-img compiler.
        inline static constexpr bool can_mmap{true};

        /// @brief When handling multithreaded growth, only the atomic variable `memory_length_p` is required. When `custom_page_size >= platform_page_size`,
        ///        dynamic determination of `memory_size` is unnecessary.
        inline static constexpr bool support_multi_thread{true};

        /// @brief      Default constructor.
        /// @note       The default page size is 65536 bytes.
        inline constexpr mmap_memory_t() noexcept
        {
            this->memory_length_p = atomic_size_allcator_t::allocate(1uz);
            ::new(this->memory_length_p)::std::atomic_size_t{};

            this->growing_mutex_p = mutex_allcator_t::allocate(1uz);
            ::new(this->growing_mutex_p)::uwvm2::utils::mutex::mutex_t{};

            constexpr ::std::size_t default_wasm_page_size{static_cast<::std::size_t>(::uwvm2::object::memory::wasm_page::default_wasm32_page_size)};
            constexpr unsigned default_wasm_page_size_log2{static_cast<unsigned>(::std::countr_zero(default_wasm_page_size))};
            this->custom_page_size_log2 = default_wasm_page_size_log2;

            this->status = mmap_memory_status_t::wasm32;
        }

        inline constexpr mmap_memory_t(::std::size_t custom_page_size, mmap_memory_status_t memory_status) noexcept
        {
            this->memory_length_p = atomic_size_allcator_t::allocate(1uz);
            ::new(this->memory_length_p)::std::atomic_size_t{};

            this->growing_mutex_p = mutex_allcator_t::allocate(1uz);
            ::new(this->growing_mutex_p)::uwvm2::utils::mutex::mutex_t{};

            // The same method as set_custom_page_size, but without adding a lock.

            // Check if it is a power of 2
            if(!::std::has_single_bit(custom_page_size)) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Since there is only one pop, directly counting the number of zeros suffices to calculate log2
            this->custom_page_size_log2 = ::std::countr_zero(custom_page_size);

            this->status = memory_status;
        }

        inline constexpr bool require_dynamic_determination_memory_size() const noexcept
        {
            auto const [page_size, success]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
            if(!success) [[unlikely]] { ::fast_io::fast_terminate(); }

            // UB will never appear; it has been preemptively checked.
            auto const custom_page_size{1uz << this->custom_page_size_log2};

            // When `custom_page_size` is less than one page, `mmap` can only guarantee the stability of the starting address and cannot provide page protection
            // checks. Additional dynamic verification is required.
            return custom_page_size < page_size;
        }

        inline constexpr bool is_full_page_protection() const noexcept
        {
            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                // 64bit platform
                switch(this->status)
                {
                    case mmap_memory_status_t::wasm32:
                    {
                        // full protection
                        return true;
                    }
                    case mmap_memory_status_t::wasm64:
                    {
                        // partial protection
                        return false;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
            }
            else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t))
            {
                // 32bit platform
                // both wasm64 and wasm32 are partial protection.
                return false;
            }
            else
            {
                static_assert(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t), "mmap unsupported platform");
            }
        }

        /// @brief      Initialize the memory.
        /// @note       Maximum value checks are not provided; maximum value checks should be performed outside of memory management.
        /// @note       You can use it after clear().
        inline constexpr void init_by_page_count(::std::size_t init_page_count) noexcept
        {
            ::std::size_t max_init_page_count;  // No initlization is required

            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                // 64bit platform
                switch(this->status)
                {
                    case mmap_memory_status_t::wasm32:
                    {
                        // In full protection mode, the maximum memory allocation permitted is half of the full protection size.
                        // max_full_protection_wasm32_length_half == (u64)u32max + 1ui64
                        constexpr auto max_full_protection_wasm32_length_half{max_full_protection_wasm32_length / 2u};

                        max_init_page_count = static_cast<::std::size_t>(max_full_protection_wasm32_length_half >> this->custom_page_size_log2);
                        break;
                    }
                    case mmap_memory_status_t::wasm64:
                    {
                        // partial protection. No need to provide double protection, as static checks are in place.
                        max_init_page_count = static_cast<::std::size_t>(max_partial_protection_wasm64_length >> this->custom_page_size_log2);
                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
            }
            else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t))
            {
                // 32bit platform
                // On 32-bit platforms, both wasm64 and wasm32 use 32-bit versions.
                // partial protection. No need to provide double protection, as static checks are in place.
                max_init_page_count = static_cast<::std::size_t>(max_partial_protection_wasm32_length >> this->custom_page_size_log2);
            }
            else
            {
                static_assert(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t), "mmap unsupported platform");
            }

            // Page Protection
            if(init_page_count > max_init_page_count) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(this->memory_begin == nullptr) [[likely]]
            {
                // UB will never appear; it has been preemptively checked.
                auto const custom_page_size{1uz << this->custom_page_size_log2};

                auto const memory_length{init_page_count << this->custom_page_size_log2};

                if(this->memory_length_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                // Reserve max space

                ::std::size_t max_protection_space;

                if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
                {
                    // 64bit platform
                    switch(this->status)
                    {
                        case mmap_memory_status_t::wasm32:
                        {
                            // full protection
                            max_protection_space = max_full_protection_wasm32_length;
                            break;
                        }
                        case mmap_memory_status_t::wasm64:
                        {
                            // partial protection
                            max_protection_space = max_partial_protection_wasm64_length;
                            break;
                        }
                        [[unlikely]] default:
                        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                            ::std::unreachable();
                        }
                    }
                }
                else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t))
                {
                    // 32bit platform
                    // On 32-bit platforms, both wasm64 and wasm32 use 32-bit versions.
                    // partial protection
                    max_protection_space = max_partial_protection_wasm32_length;
                }
                else
                {
                    static_assert(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t), "mmap unsupported platform");
                }

                if(custom_page_size > ::std::numeric_limits<::std::size_t>::max() - max_protection_space) [[unlikely]] { ::fast_io::fast_terminate(); }

                // Provides a 64-byte type protection mechanism (no foreseeable future type will exceed this size) to prevent custom page sizes (arbitrary
                // sizes) from being smaller than the type size.
                constexpr ::std::size_t max_type_size{64uz};

                // Does checking the size of custom_page cause subsequent calculations to overflow
                // max_protection_space + max_type_size never overflow
                auto const max_protection_space_plus_max_type_size{max_protection_space + max_type_size};
                if(custom_page_size > ::std::numeric_limits<::std::size_t>::max() - max_protection_space_plus_max_type_size) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                // | max_protection_space | custom_page_size | max_type_size | ... align to platform page size ... |
                auto const max_space{max_protection_space_plus_max_type_size + custom_page_size};

                // If the page alignment size of max_space cannot be reserved, the vm will trap.

# if defined(_WIN32) || defined(__CYGWIN__)                                                          // Windows
#  if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // WIN32
                {
                    auto const vamemory{reinterpret_cast<::std::byte*>(
                        ::fast_io::win32::VirtualAlloc(nullptr, max_space, 0x00002000u /*MEM_RESERVE*/, 0x01u /*PAGE_NOACCESS*/))};

                    if(vamemory == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    this->memory_begin = vamemory;
                }
#  else  // NT
                {
                    // BaseAddress A pointer to a variable that will receive the base address of the allocated region of pages. If the initial value is
                    // not zero, the region is allocated at the specified virtual address
                    // vamemory must be initialized to nullptr.
                    ::std::byte* vamemory{};

                    // Since const values cannot be used, a copy must be made.
                    ::std::size_t max_space_tmp{max_space};

                    constexpr bool zw{false};
                    auto const status{::fast_io::win32::nt::nt_allocate_virtual_memory<zw>(reinterpret_cast<void*>(-1),
                                                                                           reinterpret_cast<void**>(::std::addressof(vamemory)),
                                                                                           0u,
                                                                                           ::std::addressof(max_space_tmp),
                                                                                           0x00002000u /*MEM_RESERVE*/,
                                                                                           0x01u /*PAGE_NOACCESS*/)};

                    if(status) [[unlikely]] { ::fast_io::fast_terminate(); }

                    this->memory_begin = vamemory;
                }
#  endif
# elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&   \
     __has_include(<sys/mman.h>)  // POSIX
                {
                    // // sys_mmap may throw ::fast_io::error
                    // To align with Windows behavior, only reserve addresses; do not reserve swap space.

                    constexpr auto mmap_flags{MAP_PRIVATE |
                                              MAP_ANONYMOUS
#  if defined(MAP_NORESERVE)
                                              // Platforms that do not provide this macro are assumed to use lazy allocation.
                                              | MAP_NORESERVE
#  endif

                    };

#  ifdef UWVM_CPP_EXCEPTIONS
                    try
#  endif
                    {
                        this->memory_begin = ::fast_io::details::sys_mmap(nullptr, max_space, PROT_NONE, mmap_flags, -1, 0u);
                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                    catch(::fast_io::error)
                    {
                        ::fast_io::fast_terminate();
                    }
#  endif
                }
# endif

                // Set pages according to the initialized size
                // The minimum allocation unit for mmap is a page. Even if you request a size smaller than a page, the kernel will ultimately manage and
                // map the memory in page-sized units.

# if defined(_WIN32) || defined(__CYGWIN__)                                                          // Windows
#  if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // WIN32
                {
                    auto const vamemory2{reinterpret_cast<::std::byte*>(
                        ::fast_io::win32::VirtualAlloc(this->memory_begin, memory_length, 0x00001000u /*MEM_COMMIT*/, 0x04u /*PAGE_READWRITE*/))};
                    if(vamemory2 != this->memory_begin) [[unlikely]] { ::fast_io::fast_terminate(); }
                }
#  else  // NT
                {
                    ::std::byte* vamemory2{this->memory_begin};
                    ::std::size_t memory_length_tmp{memory_length};
                    constexpr bool zw{false};
                    auto const status{::fast_io::win32::nt::nt_allocate_virtual_memory<zw>(reinterpret_cast<void*>(-1),
                                                                                           reinterpret_cast<void**>(::std::addressof(vamemory2)),
                                                                                           0,
                                                                                           ::std::addressof(memory_length_tmp),
                                                                                           0x00001000u /*MEM_COMMIT*/,
                                                                                           0x04u /*PAGE_READWRITE*/)};

                    if(status || vamemory2 != this->memory_begin) [[unlikely]] { ::fast_io::fast_terminate(); }
                }
#  endif
# elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&   \
     __has_include(<sys/mman.h>)  // POSIX
                {

                    // sys_mprotect may throw ::fast_io::error

#  ifdef UWVM_CPP_EXCEPTIONS
                    try
#  endif
                    {
                        // The length argument must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE).
                        // Since custom_page may appear, it must be aligned to the top here.

                        auto const [page_size, success]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
                        if(!success) [[unlikely]] { ::fast_io::fast_terminate(); }
                        if(page_size == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                        auto const page_size_minus_1{(page_size - 1uz)};
                        if(memory_length > ::std::numeric_limits<::std::size_t>::max() - page_size_minus_1) [[unlikely]] { ::fast_io::fast_terminate(); }
                        auto const memory_length_ceil{(memory_length + page_size_minus_1) & ~page_size_minus_1};

                        ::fast_io::details::sys_mprotect(this->memory_begin, memory_length_ceil, PROT_READ | PROT_WRITE);
                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                    catch(::fast_io::error)
                    {
                        ::fast_io::fast_terminate();
                    }
#  endif
                }
# endif

                // Publish initialized length only after pages are successfully committed
                this->memory_length_p->store(memory_length, ::std::memory_order_release);
            }
        }

        /// @brief      Grow the memory.
        /// @param      max_limit_memory_length     This maximum value is derived from the maximum memory limit.
        inline constexpr void grow(::std::size_t page_grow_size, ::std::size_t max_limit_memory_length = ::std::numeric_limits<::std::size_t>::max()) noexcept
        {
            if(page_grow_size == 0uz) [[unlikely]] { return; }

            if(this->memory_begin == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(page_grow_size > ::std::numeric_limits<::std::size_t>::max() >> this->custom_page_size_log2) [[unlikely]]
            {
                // This situation cannot occur; it is due to user input error.
                ::fast_io::fast_terminate();
            }

            // Once initialized, the custom_page_size_log2 will not change.
            auto const memory_grow_size{page_grow_size << this->custom_page_size_log2};

            // Acquiring internal data requires locking.
            if(this->growing_mutex_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::uwvm2::utils::mutex::mutex_guard_t growing_mutex_guard_1{*this->growing_mutex_p};

            if(this->memory_length_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            // This atomic operation prevents the dynamic check from retrieving an erroneous value when reading the length.
            // Here, with the lock's support, memory access is already guaranteed to be relaxed. However, during dynamic checks, an acquire is still required.
            auto const current_length{this->memory_length_p->load(::std::memory_order_relaxed)};

            // Select the smaller value between the manually set maximum and the maximum already mmapped by the platform.
            // Beyond the boundaries of the VMA (adjacent virtual address space), protection is not automatically enforced; these regions are simply “unmapped.”
            // These unmapped areas remain as potentially available address space. Subsequent mmap operations may automatically allocate memory within these
            // spaces. Failing to impose such restrictions during memory expansion risks corrupting memory allocated to other parts of the program.
            ::std::size_t max_page_memory_length;  // No initlization is required

            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                // 64bit platform
                switch(this->status)
                {
                    case mmap_memory_status_t::wasm32:
                    {
                        // In full protection mode, the maximum memory allocation permitted is half of the full protection size.
                        // max_full_protection_wasm32_length_half == (u64)u32max + 1ui64
                        constexpr auto max_full_protection_wasm32_length_half{max_full_protection_wasm32_length / 2u};

                        max_page_memory_length = static_cast<::std::size_t>(max_full_protection_wasm32_length_half);
                        break;
                    }
                    case mmap_memory_status_t::wasm64:
                    {
                        // partial protection. No need to provide double protection, as static checks are in place.
                        max_page_memory_length = static_cast<::std::size_t>(max_partial_protection_wasm64_length);
                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
            }
            else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t))
            {
                // 32bit platform
                // On 32-bit platforms, both wasm64 and wasm32 use 32-bit versions.
                // partial protection. No need to provide double protection, as static checks are in place.
                max_page_memory_length = static_cast<::std::size_t>(max_partial_protection_wasm32_length);
            }
            else
            {
                static_assert(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t), "mmap unsupported platform");
            }

            max_limit_memory_length = ::std::min(max_limit_memory_length, max_page_memory_length);

            // start checking
            if(max_limit_memory_length < current_length) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const left_memory_size{max_limit_memory_length - current_length};
            if(memory_grow_size > left_memory_size) [[unlikely]] { ::fast_io::fast_terminate(); }

            // grown region [current_length, grow_final_memory_length)
            auto const grow_final_memory_length{current_length + memory_grow_size};

            // Set pages according to the initialized size
            // The minimum allocation unit for mmap is a page. Even if you request a size smaller than a page, the kernel will ultimately manage and
            // map the memory in page-sized units.

# if defined(_WIN32) || defined(__CYGWIN__)                                                          // windows
#  if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // win32
            {
                auto const vamemory{reinterpret_cast<::std::byte*>(
                    ::fast_io::win32::VirtualAlloc(this->memory_begin, grow_final_memory_length, 0x00001000u /*MEM_COMMIT*/, 0x04u /*PAGE_READWRITE*/))};
                if(vamemory != this->memory_begin) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
#  else  // nt
            {
                ::std::byte* vamemory{this->memory_begin};

                // Since const values cannot be used, a copy must be made.
                ::std::size_t grow_final_memory_length_tmp{grow_final_memory_length};

                constexpr bool zw{false};
                auto const status{::fast_io::win32::nt::nt_allocate_virtual_memory<zw>(reinterpret_cast<void*>(-1),
                                                                                       reinterpret_cast<void**>(::std::addressof(vamemory)),
                                                                                       0u,
                                                                                       ::std::addressof(grow_final_memory_length_tmp),
                                                                                       0x00001000u /*MEM_COMMIT*/,
                                                                                       0x04u /*PAGE_READWRITE*/)};

                if(status || vamemory != this->memory_begin) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
#  endif
# elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&   \
     __has_include(<sys/mman.h>)  // posix
            {

                // sys_mprotect may throw ::fast_io::error

#  ifdef UWVM_CPP_EXCEPTIONS
                try
#  endif
                {
                    // The length argument must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE).
                    // Since custom_page may appear, it must be aligned to the top here.

                    auto const [page_size, success]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
                    if(!success) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(page_size == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto const page_size_minus_1{(page_size - 1uz)};
                    if(grow_final_memory_length > ::std::numeric_limits<::std::size_t>::max() - page_size_minus_1) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const grow_final_memory_length_ceil{(grow_final_memory_length + page_size_minus_1) & ~page_size_minus_1};

                    ::fast_io::details::sys_mprotect(this->memory_begin, grow_final_memory_length_ceil, PROT_READ | PROT_WRITE);
                }
#  ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    ::fast_io::fast_terminate();
                }
#  endif
            }
# endif

            // Only after the memory has been successfully "committed" is the new length released to other threads.
            this->memory_length_p->store(grow_final_memory_length, ::std::memory_order_release);

            // growing_mutex_guard_1 destruct here
            return;
        }

        inline constexpr ::std::size_t get_page_size() const noexcept
        {
            if(this->memory_length_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            // This can be used in the WASM runtime.
            auto const all_memory_length{this->memory_length_p->load(::std::memory_order_acquire)};

            return all_memory_length >> this->custom_page_size_log2;
        }

        inline constexpr mmap_memory_t(mmap_memory_t const& other) noexcept = delete;

        inline constexpr mmap_memory_t& operator= (mmap_memory_t const& other) noexcept = delete;

        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done before the
        ///             WASM execution.
        inline constexpr mmap_memory_t(mmap_memory_t&& other) noexcept
        {
            this->memory_begin = other.memory_begin;
            this->memory_length_p = other.memory_length_p;
            this->custom_page_size_log2 = other.custom_page_size_log2;
            this->status = other.status;
            this->growing_mutex_p = other.growing_mutex_p;

            // clear destory other
            other.memory_begin = nullptr;
            other.memory_length_p = nullptr;
            other.custom_page_size_log2 = 0u;
            other.status = mmap_memory_status_t{};
            other.growing_mutex_p = nullptr;
        }

        /// @note      This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done before the
        ///             WASM execution.
        inline constexpr mmap_memory_t& operator= (mmap_memory_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            clear_destroy();

            this->memory_begin = other.memory_begin;
            this->memory_length_p = other.memory_length_p;
            this->custom_page_size_log2 = other.custom_page_size_log2;
            this->status = other.status;
            this->growing_mutex_p = other.growing_mutex_p;

            // clear destory other
            other.memory_begin = nullptr;
            other.memory_length_p = nullptr;
            other.custom_page_size_log2 = 0u;
            other.status = mmap_memory_status_t{};
            other.growing_mutex_p = nullptr;

            return *this;
        }

        inline constexpr ::std::size_t get_acquire_reserved_space() const noexcept
        {
            // UB will never appear; it has been preemptively checked.
            auto const custom_page_size{1uz << this->custom_page_size_log2};

            // Reserve max space
            ::std::size_t max_protection_space;

            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                // 64bit platform
                switch(this->status)
                {
                    case mmap_memory_status_t::wasm32:
                    {
                        // full protection
                        max_protection_space = max_full_protection_wasm32_length;
                        break;
                    }
                    case mmap_memory_status_t::wasm64:
                    {
                        // partial protection
                        max_protection_space = max_partial_protection_wasm64_length;
                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
            }
            else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t))
            {
                // 32bit platform
                // On 32-bit platforms, both wasm64 and wasm32 use 32-bit versions.
                // partial protection
                max_protection_space = max_partial_protection_wasm32_length;
            }
            else
            {
                static_assert(sizeof(::std::size_t) >= sizeof(::std::uint_least32_t), "mmap unsupported platform");
            }

            // Provides a 64-byte type protection mechanism (no foreseeable future type will exceed this size) to prevent custom page sizes (arbitrary
            // sizes) from being smaller than the type size.
            constexpr ::std::size_t max_type_size{64uz};

            // | max_protection_space | custom_page_size | max_type_size | ... align to platform page size ... |
            // This section does not check for overflow because it was already checked during initialization.
            auto const max_space{max_protection_space + custom_page_size + max_type_size};

            // The content retrieved here is not aligned with the platform page. To obtain the reserved space, simply align the platform page upward.
            return max_space;
        }

        /// @brief      Clear the memory.
        /// @note       Simply clearing the memory without altering the page size.
        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done after the
        ///             WASM execution.
        inline constexpr void clear() noexcept
        {
# if defined(_WIN32) || defined(__CYGWIN__)                                                          // windows
#  if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // win32
            {
                if(!::fast_io::win32::VirtualFree(this->memory_begin, 0u, 0x00008000u /*MEM_RELEASE*/)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
#  else  // nt
            {
                ::std::byte* vfmemory{this->memory_begin};
                ::std::size_t free_type{0uz};

                constexpr bool zw{false};
                if(::fast_io::win32::nt::nt_free_virtual_memory<zw>(reinterpret_cast<void*>(-1),
                                                                    reinterpret_cast<void**>(::std::addressof(vfmemory)),
                                                                    ::std::addressof(free_type),
                                                                    0x00008000u /*MEM_RELEASE*/)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
            }
#  endif
# elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&   \
     __has_include(<sys/mman.h>)  // posix
            {
                // The length argument must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE).
                // Since custom_page may appear, it must be aligned to the top here.

                auto const [page_size, success]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
                if(!success) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const acquire_reserved_space_ceil{(get_acquire_reserved_space() + page_size - 1uz) & ~(page_size - 1uz)};

                // fast_io::details::sys_munmap_nothrow is noexcept, manually throws exceptions.
                if(::fast_io::details::sys_munmap_nothrow(this->memory_begin, acquire_reserved_space_ceil)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
# endif

            this->memory_begin = nullptr;

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            // The clear operation completes before the object is destroyed, so nullptr causes a crash.
            if(this->memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            // This can only be used after the WASM execution has completed, so use relaxed instead of acquired.
            this->memory_length_p->store(0uz, ::std::memory_order_relaxed);
        }

        /// @brief      Clear the memory and destroy all flag.
        /// @note       After calling this, you must reinitialize it according to the initialization process.
        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done after the
        ///             WASM execution.
        inline constexpr void clear_destroy() noexcept
        {
            // clear_destroy can be called at any time, so nullptr is not prohibited.

            // Only discuss the recovery of already allocated funds.

            // This can only be used after the WASM execution has completed, so use relaxed instead of acquired.
            // UB will never appear; it has been preemptively checked.

# if defined(_WIN32) || defined(__CYGWIN__)                                                          // windows
#  if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // win32
            {
                if(!::fast_io::win32::VirtualFree(this->memory_begin, 0u, 0x00008000u /*MEM_RELEASE*/)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
#  else  // nt
            {
                ::std::byte* vfmemory{this->memory_begin};
                ::std::size_t free_type{0uz};

                constexpr bool zw{false};
                if(::fast_io::win32::nt::nt_free_virtual_memory<zw>(reinterpret_cast<void*>(-1),
                                                                    reinterpret_cast<void**>(::std::addressof(vfmemory)),
                                                                    ::std::addressof(free_type),
                                                                    0x00008000u /*MEM_RELEASE*/)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
            }
#  endif
# elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&   \
     __has_include(<sys/mman.h>)  // posix
            {
                // The length argument must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE).
                // Since custom_page may appear, it must be aligned to the top here.

                auto const [page_size, success]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
                if(!success) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const acquire_reserved_space_ceil{(get_acquire_reserved_space() + page_size - 1uz) & ~(page_size - 1uz)};

                // fast_io::details::sys_munmap_nothrow is noexcept, manually throws exceptions.
                if(::fast_io::details::sys_munmap_nothrow(this->memory_begin, acquire_reserved_space_ceil)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
# endif

            if(this->memory_length_p != nullptr) [[likely]] { ::std::destroy_at(this->memory_length_p); }
            atomic_size_allcator_t::deallocate_n(this->memory_length_p, 1uz);  // dealloc includes built-in nullptr checking

            if(this->growing_mutex_p != nullptr) [[likely]] { ::std::destroy_at(this->growing_mutex_p); }
            mutex_allcator_t::deallocate_n(this->growing_mutex_p, 1uz);  // dealloc includes built-in nullptr checking

            this->memory_begin = nullptr;
            this->memory_length_p = nullptr;
            this->custom_page_size_log2 = 0u;
            this->status = mmap_memory_status_t{};
            this->growing_mutex_p = nullptr;
        }

        /// @note       This function is designed to be lock-free and cannot be executed during WASM execution (multi-threaded). It can only be done after the
        ///             WASM execution.
        inline constexpr ~mmap_memory_t()
        {
            // clear_destroy can be called at any time, so nullptr is not prohibited.
            // UB will never appear; it has been preemptively checked.

            // Only discuss the recovery of already allocated funds.

# if defined(_WIN32) || defined(__CYGWIN__)                                                          // windows
#  if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // win32
            {
                if(!::fast_io::win32::VirtualFree(this->memory_begin, 0u, 0x00008000u /*MEM_RELEASE*/)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
#  else  // nt
            {
                ::std::byte* vfmemory{this->memory_begin};
                ::std::size_t free_type{0uz};

                constexpr bool zw{false};
                if(::fast_io::win32::nt::nt_free_virtual_memory<zw>(reinterpret_cast<void*>(-1),
                                                                    reinterpret_cast<void**>(::std::addressof(vfmemory)),
                                                                    ::std::addressof(free_type),
                                                                    0x00008000u /*MEM_RELEASE*/)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
            }
#  endif
# elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&   \
     __has_include(<sys/mman.h>)  // posix
            {
                // The length argument must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE).
                // Since custom_page may appear, it must be aligned to the top here.

                auto const [page_size, success]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
                if(!success) [[unlikely]] { ::fast_io::fast_terminate(); }
                if(page_size == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                // Overflow "all_memory_length + (page_size - 1uz)" will never occur here because such a situation cannot be allocated.
                auto const acquire_reserved_space_ceil{(get_acquire_reserved_space() + (page_size - 1uz)) & ~(page_size - 1uz)};

                // fast_io::details::sys_munmap_nothrow is noexcept, manually throws exceptions.
                if(::fast_io::details::sys_munmap_nothrow(this->memory_begin, acquire_reserved_space_ceil)) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
# endif

            if(this->memory_length_p != nullptr) [[likely]] { ::std::destroy_at(this->memory_length_p); }
            atomic_size_allcator_t::deallocate_n(this->memory_length_p, 1uz);  // dealloc includes built-in nullptr checking

            if(this->growing_mutex_p != nullptr) [[likely]] { ::std::destroy_at(this->growing_mutex_p); }
            mutex_allcator_t::deallocate_n(this->growing_mutex_p, 1uz);  // dealloc includes built-in nullptr checking

            // multiple call to destructor is undefined behavior, so never set to default value
        }
    };

#endif

}  // namespace uwvm2::object::memory::linear

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
#if defined(UWVM_SUPPORT_MMAP)
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::object::memory::linear::mmap_memory_t>
    {
        inline static constexpr bool value = true;
    };
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
