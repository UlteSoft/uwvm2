
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
# include <cstring>
# include <climits>
# include <version>
# include <limits>
# include <concepts>
# include <bit>
# include <type_traits>
# include <memory>
// import
# include <fast_io.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/object/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if __cpp_lib_atomic_wait >= 201907L

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::memory
{
    /// @brief      Read a Wasm value from linear memory (allocator backend) with concurrency safety and bounds double-checking.
    /// @details    - Concurrency safety: use the double-atom guard (growing_flag_p + active_ops_p) to synchronize with grow operations.
    ///             - Bounds double-checks:
    ///               (1) Using the size_t parameter supports both u32 and u64. Comparison checks between u32 or u64 and size_t will be performed by the calling
    ///                   function.
    ///               (2) Compare with the actual memory length to prevent out-of-bounds access.
    ///             - Endianness: decode as little-endian; byteswap on big-endian hosts, PDP-11 Segment Sequence and Non-8-Bit Charbit Platform-Specific
    ///                   Handling.
    /// @note       WebAssembly 1.0 (MVP) specifies that a module can have at most one memory. WASI Preview 1 is an API designed under this constraint, so it
    ///             defaults to considering only a single memory. All system calls accept offsets relative to this unique memory. Therefore, only memory[0] can
    ///             be passed subsequently.
    /// @note       Although optional distributors are provided, remember to use multiple distributors.

    template <typename Alloc>
    inline constexpr auto lock_memory(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory) noexcept
    { return ::uwvm2::object::memory::linear::memory_operation_guard_t{memory.growing_flag_p, memory.active_ops_p}; }

    template <typename Alloc>
    inline constexpr void check_memory_bounds_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                       ::std::size_t offset,
                                                       ::std::size_t wasm_bytes) noexcept
    {
        // Locking is required before accessing memory_op_guard.

        // Conduct a full inspection of the memory.
        [[maybe_unused]] auto const memory_begin{memory.memory_begin};

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(memory_begin == nullptr) [[unlikely]]
        {
            // Since this is a path frequently accessed during WASM execution, we should strive to avoid branches related to the virtual machine's own bug
            // checks (which are verified during debugging).
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
# endif

        auto const memory_length{memory.memory_length};

        // The remaining size does not support reading a single type. Can read N bytes at the last legal position
        if(wasm_bytes > memory_length || offset > memory_length - wasm_bytes) [[unlikely]]
        {
            ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                .memory_idx = 0uz,
                .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                .memory_static_offset = 0u,
                .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                .memory_type_size = wasm_bytes
            });
        }
    }

    template <typename Alloc>
    inline constexpr void check_memory_bounds_wasm32_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                              ::std::size_t wasm_bytes) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = wasm_bytes
                });
            }
        }

        check_memory_bounds_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), wasm_bytes);
    }

    template <typename Alloc>
    inline constexpr void check_memory_bounds_wasm64_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                              ::std::size_t wasm_bytes) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = wasm_bytes
                });
            }
        }

        check_memory_bounds_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), wasm_bytes);
    }

    template <typename Alloc>
    inline constexpr void check_memory_bounds(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                              ::std::size_t offset,
                                              ::std::size_t wasm_bytes) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        // After acquiring the lock, call the unlocked version.
        check_memory_bounds_unlocked(memory, offset, wasm_bytes);
    }

    template <typename Alloc>
    inline constexpr void check_memory_bounds_wasm32(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                     ::std::size_t wasm_bytes) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = wasm_bytes
                });
            }
        }

        check_memory_bounds(memory, static_cast<::std::size_t>(offset), wasm_bytes);
    }

    template <typename Alloc>
    inline constexpr void check_memory_bounds_wasm64(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                     ::std::size_t wasm_bytes) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = wasm_bytes
                });
            }
        }

        check_memory_bounds(memory, static_cast<::std::size_t>(offset), wasm_bytes);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                       ::std::size_t offset) noexcept
    {
        // unlocked version

        constexpr auto wasm_bytes{sizeof(WasmType)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        if constexpr(::std::integral<WasmType>)
        {
# if CHAR_BIT != 8
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{};  // no init required

            auto memory_curr{memory.memory_begin + offset};

            for(::std::size_t i{}; i != sizeof(WasmType); ++i)
            {
                auto const ul8_byte{::std::to_integer<::std::uint_least8_t>(*memory_curr) & 0xFFu};
                ++memory_curr;
                u |= static_cast<unsigned_wasm_type>(ul8_byte) << (i * 8u);
            }

            return static_cast<WasmType>(u);
# else
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u;  // no init required

            // never overflow
            ::std::memcpy(::std::addressof(u), memory.memory_begin + offset, sizeof(u));

            // Supports big-endian, little-endian, and PDP-11 endian
            u = ::fast_io::little_endian(u);

            return static_cast<WasmType>(u);
# endif
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }

        return {};
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                              ::std::size_t offset) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        return get_basic_wasm_type_from_memory_unlocked<WasmType, Alloc>(memory, offset);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                   ::std::size_t offset,
                                                                   WasmType value) noexcept
    {
        // unlocked version

        constexpr auto wasm_bytes{sizeof(WasmType)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        if constexpr(::std::integral<WasmType>)
        {
# if CHAR_BIT != 8
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{static_cast<unsigned_wasm_type>(value)};

            auto memory_curr{memory.memory_begin + offset};

            for(::std::size_t i{}; i != sizeof(WasmType); ++i)
            {
                *memory_curr = static_cast<::std::byte>(u & 0xFFu);
                ++memory_curr;
                u >>= 8u;
            }
# else
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{static_cast<unsigned_wasm_type>(value)};

            u = ::fast_io::little_endian(u);

            // never overflow
            ::std::memcpy(memory.memory_begin + offset, ::std::addressof(u), sizeof(u));
# endif
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                          WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                          WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                          ::std::size_t offset,
                                                          WasmType value) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        store_basic_wasm_type_to_memory_unlocked<WasmType, Alloc>(memory, offset, value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                 WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                 WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                        ::std::size_t offset,
                                                        ::std::byte* begin,
                                                        ::std::byte* end) noexcept
    {
        // unlocked version

        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        ::std::memcpy(begin, memory.memory_begin + offset, wasm_bytes);

# if CHAR_BIT != 8
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        for(auto curr{begin}; curr != end; ++curr)
        {
            auto const v{::std::to_integer<unsigned>(*curr)};
            if((v & static_cast<unsigned>(~0xFFu)) != 0u) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
        }
#  endif
# endif
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm32_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                               ::std::byte* begin,
                                                               ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm64_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                               ::std::byte* begin,
                                                               ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                               ::std::size_t offset,
                                               ::std::byte* begin,
                                               ::std::byte* end) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        read_all_from_memory_unlocked<Alloc>(memory, offset, begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm32(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                      ::std::byte* begin,
                                                      ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm64(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                      ::std::byte* begin,
                                                      ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                       ::std::size_t offset,
                                                       ::std::byte const* begin,
                                                       ::std::byte const* end) noexcept
    {
        // unlocked version

        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        ::std::memcpy(memory.memory_begin + offset, begin, wasm_bytes);

# if CHAR_BIT != 8
        auto const memory_write_begin{memory.memory_begin + offset};
        auto const memory_write_end{memory_write_begin + wasm_bytes};
        for(auto curr{memory_write_begin}; curr != memory_write_end; ++curr)
        {
            *curr = static_cast<::std::byte>(::std::to_integer<::std::uint_least8_t>(*curr) & 0xFFu);
        }
# endif
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm32_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                              ::std::byte const* begin,
                                                              ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm64_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                              ::std::byte const* begin,
                                                              ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                              ::std::size_t offset,
                                              ::std::byte const* begin,
                                              ::std::byte const* end) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        write_all_to_memory_unlocked<Alloc>(memory, offset, begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm32(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                     ::std::byte const* begin,
                                                     ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm64(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                     ::std::byte const* begin,
                                                     ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                ::std::size_t offset,
                                                ::std::size_t size) noexcept
    {
        // unlocked version

        check_memory_bounds_unlocked(memory, offset, size);

        ::std::memset(memory.memory_begin + offset, 0, size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm32_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                       ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm64_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                       ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                       ::std::size_t offset,
                                       ::std::size_t size) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        clear_memory_unlocked<Alloc>(memory, offset, size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm32(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                              ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm64(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                              ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                                 ::std::size_t offset) noexcept
    {
        // unlocked version

        if constexpr(::std::integral<WasmType>)
        {
# if CHAR_BIT != 8
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{};  // no init required

            auto memory_curr{memory.memory_begin + offset};

            for(::std::size_t i{}; i != sizeof(WasmType); ++i)
            {
                auto const ul8_byte{::std::to_integer<::std::uint_least8_t>(*memory_curr) & 0xFFu};
                ++memory_curr;
                u |= static_cast<unsigned_wasm_type>(ul8_byte) << (i * 8u);
            }

            return static_cast<WasmType>(u);
# else
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u;  // no init required

            // never overflow
            ::std::memcpy(::std::addressof(u), memory.memory_begin + offset, sizeof(u));

            // Supports big-endian, little-endian, and PDP-11 endian
            u = ::fast_io::little_endian(u);

            return static_cast<WasmType>(u);
# endif
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }

        return {};
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked(
        ::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unchecked_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked(
        ::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unchecked_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                        ::std::size_t offset) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        return get_basic_wasm_type_from_memory_unchecked_unlocked<WasmType, Alloc>(memory, offset);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unchecked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unchecked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                             ::std::size_t offset,
                                                                             WasmType value) noexcept
    {
        // unlocked version

        if constexpr(::std::integral<WasmType>)
        {
# if CHAR_BIT != 8
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{static_cast<unsigned_wasm_type>(value)};

            auto memory_curr{memory.memory_begin + offset};

            for(::std::size_t i{}; i != sizeof(WasmType); ++i)
            {
                *memory_curr = static_cast<::std::byte>(u & 0xFFu);
                ++memory_curr;
                u >>= 8u;
            }
# else
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{static_cast<unsigned_wasm_type>(value)};

            u = ::fast_io::little_endian(u);

            // never overflow
            ::std::memcpy(memory.memory_begin + offset, ::std::addressof(u), sizeof(u));
# endif
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked(
        ::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
        WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unchecked_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64_unchecked_unlocked(
        ::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
        WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unchecked_unlocked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    /// @note      This helper intentionally does not require that callers share the same
    ///            `memory_operation_guard_t` instance that was used for any prior bounds
    ///            check. Linear allocator-backed memories are grow-only: grow operations
    ///            may increase `memory_length` but never shrink it. Unlike the mmap-based
    ///            linear memory backend, which keeps a stable base address across grows,
    ///            the allocator-backed variant may grow via a `realloc`-style relocation,
    ///            so a grow can copy the underlying buffer and change `memory_begin`.
    ///            Therefore, once a particular (offset, sizeof(WasmType)) range has been
    ///            validated against some `memory_length` value, the same range remains
    ///            contained within the memory after any subsequent grow. The guard
    ///            constructed here only needs to synchronize with concurrent grow
    ///            operations to avoid races while the underlying buffer may be relocated;
    ///            its purpose is to ensure that no grow can occur while this write is in
    ///            progress, not to revalidate an earlier bounds check. Call sites that
    ///            require a single lock to cover both explicit bounds checks and the
    ///            write itself should instead acquire a long-lived guard via `lock_memory`
    ///            and use the `_unchecked_unlocked` variants.

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                    ::std::size_t offset,
                                                                    WasmType value) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        store_basic_wasm_type_to_memory_unchecked_unlocked<WasmType, Alloc>(memory, offset, value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                           WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unchecked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType, typename Alloc>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                           WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unchecked<WasmType, Alloc>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                  ::std::size_t offset,
                                                                  ::std::byte* begin,
                                                                  ::std::byte* end) noexcept
    {
        // unlocked version

        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        ::std::memcpy(begin, memory.memory_begin + offset, wasm_bytes);

# if CHAR_BIT != 8
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        for(auto curr{begin}; curr != end; ++curr)
        {
            auto const v{::std::to_integer<unsigned>(*curr)};
            if((v & static_cast<unsigned>(~0xFFu)) != 0u) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
        }
#  endif
# endif
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                         ::std::byte* begin,
                                                                         ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unchecked_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                         ::std::byte* begin,
                                                                         ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unchecked_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                         ::std::size_t offset,
                                                         ::std::byte* begin,
                                                         ::std::byte* end) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        read_all_from_memory_unchecked_unlocked<Alloc>(memory, offset, begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm32_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                ::std::byte* begin,
                                                                ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unchecked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void read_all_from_memory_wasm64_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                ::std::byte* begin,
                                                                ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unchecked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                 ::std::size_t offset,
                                                                 ::std::byte const* begin,
                                                                 ::std::byte const* end) noexcept
    {
        // unlocked version

        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        ::std::memcpy(memory.memory_begin + offset, begin, wasm_bytes);

# if CHAR_BIT != 8
        auto const memory_write_begin{memory.memory_begin + offset};
        auto const memory_write_end{memory_write_begin + wasm_bytes};
        for(auto curr{memory_write_begin}; curr != memory_write_end; ++curr)
        {
            *curr = static_cast<::std::byte>(::std::to_integer<::std::uint_least8_t>(*curr) & 0xFFu);
        }
# endif
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                        ::std::byte const* begin,
                                                                        ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unchecked_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                        ::std::byte const* begin,
                                                                        ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unchecked_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                        ::std::size_t offset,
                                                        ::std::byte const* begin,
                                                        ::std::byte const* end) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        write_all_to_memory_unchecked_unlocked<Alloc>(memory, offset, begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm32_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                               ::std::byte const* begin,
                                                               ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unchecked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void write_all_to_memory_wasm64_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                               ::std::byte const* begin,
                                                               ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unchecked<Alloc>(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                          ::std::size_t offset,
                                                          ::std::size_t size) noexcept
    {
        // unlocked version

        ::std::memset(memory.memory_begin + offset, 0, size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                 ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unchecked_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                 ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unchecked_unlocked<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                 ::std::size_t offset,
                                                 ::std::size_t size) noexcept
    {
        // Mutual exclusion between concurrent read/write operations and memory growth: Entering the memory operation region
        ::uwvm2::object::memory::linear::memory_operation_guard_t memory_op_guard{memory.growing_flag_p, memory.active_ops_p};

        clear_memory_unchecked_unlocked<Alloc>(memory, offset, size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm32_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                        ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u32
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unchecked<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

    template <typename Alloc>
    inline constexpr void clear_memory_wasm64_unchecked(::uwvm2::object::memory::linear::basic_allocator_memory_t<Alloc> const& memory,
                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                        ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            // The size_t of current platforms is smaller than u64
            if(offset > size_t_max) [[unlikely]]
            {
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory.memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unchecked<Alloc>(memory, static_cast<::std::size_t>(offset), size);
    }

}  // namespace uwvm2::imported::wasi::wasip1::memory

#endif
