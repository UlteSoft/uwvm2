
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
# include <cstring>
# include <limits>
# include <concepts>
# include <bit>
# include <type_traits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
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

#ifdef UWVM_SUPPORT_MMAP

# if CHAR_BIT != 8
#  error "CHAR_BIT must be 8"
# endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::memory
{
    /// @brief      Read a Wasm value from linear memory (mmap backend) with concurrency safety.
    /// @note       WebAssembly 1.0 (MVP) specifies that a module can have at most one memory. WASI Preview 1 is an API designed under this constraint, so it
    ///             defaults to considering only a single memory. All system calls accept offsets relative to this unique memory. Therefore, only memory[0] can
    ///             be passed subsequently.
    /// @note       In WASI, all mmap operations are handled by a single function rather than split across multiple functions. This approach ensures that the
    ///             binary size does not inflate, and WASI does not impose excessive performance demands on memory access.

    inline constexpr auto lock_memory(::uwvm2::object::memory::linear::mmap_memory_t const&) noexcept
    { return ::uwvm2::object::memory::linear::dummy_memory_operation_guard_t{}; }

    inline constexpr void check_memory_bounds_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                       ::std::size_t offset,
                                                       ::std::size_t wasm_bytes) noexcept
    {
        // mmap does not have a lock guard

        // Base address must exist once initialized
        // In an mmap environment, the memory beginning remains constant.
        [[maybe_unused]] auto const memory_begin{memory.memory_begin};

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        // Since this is a path frequently accessed during WASM execution, we should strive to avoid branches related to the virtual machine's own bug
        // checks (which are verified during debugging).
        if(memory_begin == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

        bool const need_dynamic_determination_memory_size{memory.require_dynamic_determination_memory_size()};

        if(need_dynamic_determination_memory_size)
        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};

            // There is no need to distinguish whether it is fully protected here, as the system will automatically check during expansion whether the expansion
            // exceeds the limit. Therefore, using `memory_length` directly is sufficient for this determination.

            // The remaining size does not support reading a single type.
            // Here, since the custom page size is smaller than the platform page size, page protection cannot be used to determine whether a cross-page read is
            // required. Therefore, an inter-page check must be performed.
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
        else
        {
            bool const is_full_page_protection{memory.is_full_page_protection()};

            // if (is_full_page_protection)
            // Any operation within the bounds of the memory length data type is safe.
            // | max_protection_space (SIZE_MAX) | custom_page_size | max_type_size | ... align to platform page size ... |
            // no need to check

            if(!is_full_page_protection)
            {

                // No dynamic checks are required, but static checks are necessary.

                switch(memory.status)
                {
                    case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32:
                    {
                        // Since there is a page at the boundary and a protection page of the maximum possible type, no type size reduction check is required
                        // here.
                        // | max_protection_space | custom_page_size | max_type_size | ... align to platform page size ... |
                        if constexpr(::std::numeric_limits<::std::remove_cvref_t<decltype(offset)>>::max() >
                                     ::std::numeric_limits<
                                         ::std::remove_cvref_t<decltype(::uwvm2::object::memory::linear::max_partial_protection_wasm32_length)>>::max())
                        {
                            if(offset > ::uwvm2::object::memory::linear::max_partial_protection_wasm32_length) [[unlikely]]
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                                    .memory_idx = 0uz,
                                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                                    .memory_static_offset = 0u,
                                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                                    .memory_type_size = wasm_bytes
                                });
                            }
                        }

                        break;
                    }
                    case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64:
                    {
                        // Since there is a page at the boundary and a protection page of the maximum possible type, no type size reduction check is required
                        // here.
                        // | max_protection_space | custom_page_size | max_type_size | ... align to platform page size ... |
                        if constexpr(::std::numeric_limits<::std::remove_cvref_t<decltype(offset)>>::max() >
                                     ::std::numeric_limits<
                                         ::std::remove_cvref_t<decltype(::uwvm2::object::memory::linear::max_partial_protection_wasm64_length)>>::max())
                        {
                            if(offset > ::uwvm2::object::memory::linear::max_partial_protection_wasm64_length) [[unlikely]]
                            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                                    .memory_idx = 0uz,
                                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                                    .memory_static_offset = 0u,
                                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                                    .memory_type_size = wasm_bytes
                                });
                            }
                        }

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
        }
    }

    inline constexpr void check_memory_bounds_wasm32_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
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
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};

                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = wasm_bytes
                });
            }
        }

        check_memory_bounds_unlocked(memory, static_cast<::std::size_t>(offset), wasm_bytes);
    }

    inline constexpr void check_memory_bounds_wasm64_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
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
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};

                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = wasm_bytes
                });
            }
        }

        check_memory_bounds_unlocked(memory, static_cast<::std::size_t>(offset), wasm_bytes);
    }

    inline constexpr void check_memory_bounds(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                              ::std::size_t offset,
                                              ::std::size_t wasm_bytes) noexcept
    { return check_memory_bounds_unlocked(memory, offset, wasm_bytes); }

    inline constexpr void check_memory_bounds_wasm32(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                     ::std::size_t wasm_bytes) noexcept
    { return check_memory_bounds_wasm32_unlocked(memory, offset, wasm_bytes); }

    inline constexpr void check_memory_bounds_wasm64(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                     ::std::size_t wasm_bytes) noexcept
    { return check_memory_bounds_wasm64_unlocked(memory, offset, wasm_bytes); }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                       ::std::size_t offset) noexcept
    {
        constexpr auto wasm_bytes{sizeof(WasmType)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        if constexpr(::std::integral<WasmType>)
        {
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u;  // no init required

            // never overflow
            ::std::memcpy(::std::addressof(u), memory.memory_begin + offset, sizeof(u));

            // Supports big-endian, little-endian. mmap platform without PDP-11 sequence.
            u = ::fast_io::little_endian(u);

            return static_cast<WasmType>(u);
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }

        return {};
    }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory(::uwvm2::object::memory::linear::mmap_memory_t const& memory, ::std::size_t offset) noexcept
    { return get_basic_wasm_type_from_memory_unlocked<WasmType>(memory, offset); }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    { return get_basic_wasm_type_from_memory_wasm32_unlocked<WasmType>(memory, offset); }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    { return get_basic_wasm_type_from_memory_wasm64_unlocked<WasmType>(memory, offset); }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                   ::std::size_t offset,
                                                                   WasmType value) noexcept
    {
        constexpr auto wasm_bytes{sizeof(WasmType)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        if constexpr(::std::integral<WasmType>)
        {
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{static_cast<unsigned_wasm_type>(value)};

            u = ::fast_io::little_endian(u);

            // never overflow
            ::std::memcpy(memory.memory_begin + offset, ::std::addressof(u), sizeof(u));
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }
    }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                          WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                          WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                          ::std::size_t offset,
                                                          WasmType value) noexcept
    { store_basic_wasm_type_to_memory_unlocked<WasmType>(memory, offset, value); }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                 WasmType value) noexcept
    { store_basic_wasm_type_to_memory_wasm32_unlocked<WasmType>(memory, offset, value); }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                 WasmType value) noexcept
    { store_basic_wasm_type_to_memory_wasm64_unlocked<WasmType>(memory, offset, value); }

    inline constexpr void read_all_from_memory_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                        ::std::size_t offset,
                                                        ::std::byte* begin,
                                                        ::std::byte* end) noexcept
    {
        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        ::std::memcpy(begin, memory.memory_begin + offset, wasm_bytes);
    }

    inline constexpr void read_all_from_memory_wasm32_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                               ::std::byte* begin,
                                                               ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void read_all_from_memory_wasm64_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                               ::std::byte* begin,
                                                               ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void read_all_from_memory(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                               ::std::size_t offset,
                                               ::std::byte* begin,
                                               ::std::byte* end) noexcept
    { read_all_from_memory_unlocked(memory, offset, begin, end); }

    inline constexpr void read_all_from_memory_wasm32(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                      ::std::byte* begin,
                                                      ::std::byte* end) noexcept
    { read_all_from_memory_wasm32_unlocked(memory, offset, begin, end); }

    inline constexpr void read_all_from_memory_wasm64(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                      ::std::byte* begin,
                                                      ::std::byte* end) noexcept
    { read_all_from_memory_wasm64_unlocked(memory, offset, begin, end); }

    inline constexpr void write_all_to_memory_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                       ::std::size_t offset,
                                                       ::std::byte const* begin,
                                                       ::std::byte const* end) noexcept
    {
        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        check_memory_bounds_unlocked(memory, offset, wasm_bytes);

        ::std::memcpy(memory.memory_begin + offset, begin, wasm_bytes);
    }

    inline constexpr void write_all_to_memory_wasm32_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                              ::std::byte const* begin,
                                                              ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void write_all_to_memory_wasm64_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                              ::std::byte const* begin,
                                                              ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void write_all_to_memory(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                              ::std::size_t offset,
                                              ::std::byte const* begin,
                                              ::std::byte const* end) noexcept
    { write_all_to_memory_unlocked(memory, offset, begin, end); }

    inline constexpr void write_all_to_memory_wasm32(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                     ::std::byte const* begin,
                                                     ::std::byte const* end) noexcept
    { write_all_to_memory_wasm32_unlocked(memory, offset, begin, end); }

    inline constexpr void write_all_to_memory_wasm64(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                     ::std::byte const* begin,
                                                     ::std::byte const* end) noexcept
    { write_all_to_memory_wasm64_unlocked(memory, offset, begin, end); }

    inline constexpr void clear_memory_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory, ::std::size_t offset, ::std::size_t size) noexcept
    {
        check_memory_bounds_unlocked(memory, offset, size);

        ::fast_io::freestanding::bytes_clear_n(memory.memory_begin + offset, size);
    }

    inline constexpr void clear_memory_wasm32_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                       ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unlocked(memory, static_cast<::std::size_t>(offset), size);
    }

    inline constexpr void clear_memory_wasm64_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                       ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unlocked(memory, static_cast<::std::size_t>(offset), size);
    }

    inline constexpr void clear_memory(::uwvm2::object::memory::linear::mmap_memory_t const& memory, ::std::size_t offset, ::std::size_t size) noexcept
    { clear_memory_unlocked(memory, offset, size); }

    inline constexpr void clear_memory_wasm32(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                              ::std::size_t size) noexcept
    { clear_memory_wasm32_unlocked(memory, offset, size); }

    inline constexpr void clear_memory_wasm64(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                              ::std::size_t size) noexcept
    { clear_memory_wasm64_unlocked(memory, offset, size); }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                                 ::std::size_t offset) noexcept
    {
        if constexpr(::std::integral<WasmType>)
        {
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u;  // no init required

            // never overflow
            ::std::memcpy(::std::addressof(u), memory.memory_begin + offset, sizeof(u));

            // Supports big-endian, little-endian. mmap platform without PDP-11 sequence.
            u = ::fast_io::little_endian(u);

            return static_cast<WasmType>(u);
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }

        return {};
    }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unchecked_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked(
        ::uwvm2::object::memory::linear::mmap_memory_t const& memory,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        return get_basic_wasm_type_from_memory_unchecked_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset));
    }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                        ::std::size_t offset) noexcept
    { return get_basic_wasm_type_from_memory_unchecked_unlocked<WasmType>(memory, offset); }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm32_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset) noexcept
    { return get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<WasmType>(memory, offset); }

    template <typename WasmType>
    inline constexpr WasmType get_basic_wasm_type_from_memory_wasm64_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset) noexcept
    { return get_basic_wasm_type_from_memory_wasm64_unchecked_unlocked<WasmType>(memory, offset); }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                             ::std::size_t offset,
                                                                             WasmType value) noexcept
    {
        if constexpr(::std::integral<WasmType>)
        {
            using unsigned_wasm_type = ::std::make_unsigned_t<WasmType>;
            unsigned_wasm_type u{static_cast<unsigned_wasm_type>(value)};

            u = ::fast_io::little_endian(u);

            // never overflow
            ::std::memcpy(memory.memory_begin + offset, ::std::addressof(u), sizeof(u));
        }
        else
        {
            static_assert(::std::integral<WasmType>, "wasi only supports the use of integer types.");
        }
    }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                                    WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unchecked_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                                    WasmType value) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = sizeof(WasmType)
                });
            }
        }

        store_basic_wasm_type_to_memory_unchecked_unlocked<WasmType>(memory, static_cast<::std::size_t>(offset), value);
    }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                    ::std::size_t offset,
                                                                    WasmType value) noexcept
    { store_basic_wasm_type_to_memory_unchecked_unlocked<WasmType>(memory, offset, value); }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm32_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                           WasmType value) noexcept
    { store_basic_wasm_type_to_memory_wasm32_unchecked_unlocked<WasmType>(memory, offset, value); }

    template <typename WasmType>
    inline constexpr void store_basic_wasm_type_to_memory_wasm64_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                           WasmType value) noexcept
    { store_basic_wasm_type_to_memory_wasm64_unchecked_unlocked<WasmType>(memory, offset, value); }

    inline constexpr void read_all_from_memory_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                  ::std::size_t offset,
                                                                  ::std::byte* begin,
                                                                  ::std::byte* end) noexcept
    {
        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        ::std::memcpy(begin, memory.memory_begin + offset, wasm_bytes);
    }

    inline constexpr void read_all_from_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                         ::std::byte* begin,
                                                                         ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unchecked_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void read_all_from_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                         ::std::byte* begin,
                                                                         ::std::byte* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        read_all_from_memory_unchecked_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void read_all_from_memory_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                         ::std::size_t offset,
                                                         ::std::byte* begin,
                                                         ::std::byte* end) noexcept
    { read_all_from_memory_unchecked_unlocked(memory, offset, begin, end); }

    inline constexpr void read_all_from_memory_wasm32_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                ::std::byte* begin,
                                                                ::std::byte* end) noexcept
    { read_all_from_memory_wasm32_unchecked_unlocked(memory, offset, begin, end); }

    inline constexpr void read_all_from_memory_wasm64_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                ::std::byte* begin,
                                                                ::std::byte* end) noexcept
    { read_all_from_memory_wasm64_unchecked_unlocked(memory, offset, begin, end); }

    inline constexpr void write_all_to_memory_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                 ::std::size_t offset,
                                                                 ::std::byte const* begin,
                                                                 ::std::byte const* end) noexcept
    {
        if(begin > end) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const wasm_bytes{static_cast<::std::size_t>(end - begin)};

        ::std::memcpy(memory.memory_begin + offset, begin, wasm_bytes);
    }

    inline constexpr void write_all_to_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                        ::std::byte const* begin,
                                                                        ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unchecked_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void write_all_to_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                        ::std::byte const* begin,
                                                                        ::std::byte const* end) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = static_cast<::std::size_t>(end - begin)
                });
            }
        }

        write_all_to_memory_unchecked_unlocked(memory, static_cast<::std::size_t>(offset), begin, end);
    }

    inline constexpr void write_all_to_memory_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                        ::std::size_t offset,
                                                        ::std::byte const* begin,
                                                        ::std::byte const* end) noexcept
    { write_all_to_memory_unchecked_unlocked(memory, offset, begin, end); }

    inline constexpr void write_all_to_memory_wasm32_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                               ::std::byte const* begin,
                                                               ::std::byte const* end) noexcept
    { write_all_to_memory_wasm32_unchecked_unlocked(memory, offset, begin, end); }

    inline constexpr void write_all_to_memory_wasm64_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                               ::std::byte const* begin,
                                                               ::std::byte const* end) noexcept
    { write_all_to_memory_wasm64_unchecked_unlocked(memory, offset, begin, end); }

    inline constexpr void clear_memory_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                          ::std::size_t offset,
                                                          ::std::size_t size) noexcept
    { ::fast_io::freestanding::bytes_clear_n(memory.memory_begin + offset, size); }

    inline constexpr void clear_memory_wasm32_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                                 ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unchecked_unlocked(memory, static_cast<::std::size_t>(offset), size);
    }

    inline constexpr void clear_memory_wasm64_unchecked_unlocked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                                 ::std::size_t size) noexcept
    {
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasi_void_ptr_wasm64_max{::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t>::max()};
        if constexpr(size_t_max < wasi_void_ptr_wasm64_max)
        {
            if(offset > size_t_max) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

                auto const memory_length{memory.memory_length_p->load(::std::memory_order_acquire)};
                ::uwvm2::object::memory::error::output_memory_error_and_terminate({
                    .memory_idx = 0uz,
                    .memory_offset = {.offset = static_cast<::std::uint_least64_t>(offset), .offset_65_bit = false},
                    .memory_static_offset = 0u,
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .memory_type_size = size
                });
            }
        }

        clear_memory_unchecked_unlocked(memory, static_cast<::std::size_t>(offset), size);
    }

    inline constexpr void clear_memory_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                 ::std::size_t offset,
                                                 ::std::size_t size) noexcept
    { clear_memory_unchecked_unlocked(memory, offset, size); }

    inline constexpr void clear_memory_wasm32_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t offset,
                                                        ::std::size_t size) noexcept
    { clear_memory_wasm32_unchecked_unlocked(memory, offset, size); }

    inline constexpr void clear_memory_wasm64_unchecked(::uwvm2::object::memory::linear::mmap_memory_t const& memory,
                                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t offset,
                                                        ::std::size_t size) noexcept
    { clear_memory_wasm64_unchecked_unlocked(memory, offset, size); }
}  // namespace uwvm2::imported::wasi::wasip1::memory
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
