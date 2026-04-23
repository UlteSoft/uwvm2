/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-08
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    inline constexpr ::std::uint_least32_t preload_host_api_v1_abi_version{1u};

    /*
        Preload memory access command-line examples:

        1. Register a preload dynamic library and allow copy-based access to every memory:

           uwvm --wasm-register-dl ./libdemo.so demo --wasm-set-preload-module-attribute demo copy all

        2. Request direct mmap-backed access for memory indices 0 and 1 when the active platform/runtime supports it:

           uwvm --wasm-register-dl ./libinspect.so inspect --wasm-set-preload-module-attribute inspect mmap 0,1

        3. The descriptor delivery state tells the preload module which mmap contract is active for each memory:
           - copy
           - mmap with full protection
           - mmap with partial protection
           - mmap with dynamic bounds (custom page size / runtime length checks required)
    */

    extern "C"
    {
        enum uwvm_preload_memory_delivery_state_t : unsigned
        {
            uwvm_preload_memory_delivery_none = 0u,
            uwvm_preload_memory_delivery_copy,
            uwvm_preload_memory_delivery_mmap_full_protection,
            uwvm_preload_memory_delivery_mmap_partial_protection,
            uwvm_preload_memory_delivery_mmap_dynamic_bounds
        };

        enum uwvm_preload_memory_backend_kind_t : unsigned
        {
            uwvm_preload_memory_backend_native_defined = 0u,
            uwvm_preload_memory_backend_local_imported
        };

        struct uwvm_preload_memory_descriptor_t
        {
            ::std::size_t memory_index;
            unsigned delivery_state;
            unsigned backend_kind;
            unsigned reserved0;
            unsigned reserved1;
            ::std::uint_least64_t page_count;
            ::std::uint_least64_t page_size_bytes;
            ::std::uint_least64_t byte_length;
            ::std::uint_least64_t partial_protection_limit_bytes;
            void* mmap_view_begin;
            void const* dynamic_length_atomic_object;
        };

        using uwvm_preload_memory_descriptor_count_t = ::std::size_t (*)();
        using uwvm_preload_memory_descriptor_at_t = bool (*)(::std::size_t, uwvm_preload_memory_descriptor_t*);
        using uwvm_preload_memory_read_t = bool (*)(::std::size_t, ::std::uint_least64_t, void*, ::std::size_t);
        using uwvm_preload_memory_write_t = bool (*)(::std::size_t, ::std::uint_least64_t, void const*, ::std::size_t);

        struct uwvm_preload_host_api_v1
        {
            ::std::size_t struct_size;
            ::std::uint_least32_t abi_version;
            uwvm_preload_memory_descriptor_count_t memory_descriptor_count;
            uwvm_preload_memory_descriptor_at_t memory_descriptor_at;
            uwvm_preload_memory_read_t memory_read;
            uwvm_preload_memory_write_t memory_write;
        };

        using uwvm_set_preload_host_api_v1_t = void (*)(uwvm_preload_host_api_v1 const*);

        /*
            Recommended preload memory-access implementation pattern (C++):

            1. Export `uwvm_set_preload_host_api_v1` and cache the pointer.
            2. At the start of each plugin entrypoint, query the descriptors you need.
            3. Dispatch by `delivery_state` instead of assuming copy or mmap.
            4. Keep `memory_read()` / `memory_write()` as the correctness fallback.

            Example outline:

            #include <atomic>
            #include <cstring>

            static uwvm_preload_host_api_v1 const* g_preload_host_api{};

            extern "C" void uwvm_set_preload_host_api_v1(uwvm_preload_host_api_v1 const* api) noexcept
            {
                g_preload_host_api = api;
            }

            [[nodiscard]] inline bool find_memory_descriptor(::std::size_t memory_index,
                                                             uwvm_preload_memory_descriptor_t& out) noexcept
            {
                if(g_preload_host_api == nullptr) { return false; }

                auto const count{g_preload_host_api->memory_descriptor_count()};
                for(::std::size_t i{}; i != count; ++i)
                {
                    uwvm_preload_memory_descriptor_t curr{};
                    if(!g_preload_host_api->memory_descriptor_at(i, &curr)) { continue; }
                    if(curr.memory_index == memory_index)
                    {
                        out = curr;
                        return true;
                    }
                }
                return false;
            }

            [[nodiscard]] inline bool range_is_valid(::std::uint_least64_t byte_length,
                                                     ::std::uint_least64_t offset,
                                                     ::std::size_t size) noexcept
            {
                auto const size64{static_cast<::std::uint_least64_t>(size)};
                return offset <= byte_length && size64 <= (byte_length - offset);
            }

            [[nodiscard]] inline bool try_load_u32(uwvm_preload_memory_descriptor_t const& descriptor,
                                                   ::std::uint_least64_t offset,
                                                   unsigned& value) noexcept
            {
                switch(descriptor.delivery_state)
                {
                    case uwvm_preload_memory_delivery_copy:
                    {
                        return g_preload_host_api->memory_read(descriptor.memory_index, offset, &value, sizeof(value));
                    }
                    case uwvm_preload_memory_delivery_mmap_full_protection:
                    {
                        if(descriptor.mmap_view_begin == nullptr) { return false; }
                        if(!range_is_valid(descriptor.byte_length, offset, sizeof(value))) { return false; }
                        ::std::memcpy(&value,
                                      static_cast<::std::byte const*>(descriptor.mmap_view_begin) + static_cast<::std::size_t>(offset),
                                      sizeof(value));
                        return true;
                    }
                    case uwvm_preload_memory_delivery_mmap_partial_protection:
                    {
                        if(descriptor.mmap_view_begin == nullptr) { return false; }

                        auto const in_fast_region{
                            range_is_valid(descriptor.partial_protection_limit_bytes, offset, sizeof(value))};
                        if(!in_fast_region && !range_is_valid(descriptor.byte_length, offset, sizeof(value)))
                        {
                            return false;
                        }

                        ::std::memcpy(&value,
                                      static_cast<::std::byte const*>(descriptor.mmap_view_begin) + static_cast<::std::size_t>(offset),
                                      sizeof(value));
                        return true;
                    }
                    case uwvm_preload_memory_delivery_mmap_dynamic_bounds:
                    {
                        if(descriptor.mmap_view_begin == nullptr || descriptor.dynamic_length_atomic_object == nullptr) { return false; }
                        auto const length_atomic{
                            static_cast<::std::atomic_size_t const*>(descriptor.dynamic_length_atomic_object)};
                        auto const current_length{static_cast<::std::uint_least64_t>(length_atomic->load(::std::memory_order_acquire))};
                        if(!range_is_valid(current_length, offset, sizeof(value))) { return false; }
                        ::std::memcpy(&value,
                                      static_cast<::std::byte const*>(descriptor.mmap_view_begin) + static_cast<::std::size_t>(offset),
                                      sizeof(value));
                        return true;
                    }
                    case uwvm_preload_memory_delivery_none: [[fallthrough]];
                    default:
                    {
                        return false;
                    }
                }
            }

            How each delivery state should be handled:
            - `uwvm_preload_memory_delivery_copy`
              Use only `memory_read()` / `memory_write()`. Do not dereference `mmap_view_begin`.

            - `uwvm_preload_memory_delivery_mmap_full_protection`
              The runtime provides a stable mmap pointer and full hardware guard coverage for the mapped memory model.
              Raw-pointer access is available. A snapshot bounds check against `byte_length` is still the simplest portable choice.

            - `uwvm_preload_memory_delivery_mmap_partial_protection`
              Raw-pointer access is available, but protection is only guaranteed inside the fixed partial-protection region.
              `partial_protection_limit_bytes` tells the plugin where this region ends. A good pattern is:
              fast-path accesses fully inside that limit, otherwise run a normal explicit range check against `byte_length`
              before dereferencing, or fall back to `memory_read()` / `memory_write()`.

            - `uwvm_preload_memory_delivery_mmap_dynamic_bounds`
              Raw-pointer access is available, but the effective accessible length must be reloaded from
              `dynamic_length_atomic_object` before each raw access. This is the correct handling model for custom-page-size
              configurations that require dynamic bounds determination.

            Additional authoring notes:
            - Refresh descriptors when you need the latest memory size after a grow operation.
            - Writes follow the same state machine as reads: copy mode uses `memory_write()`, mmap modes write through
              `mmap_view_begin` only after the same state-specific bounds checks succeed.
            - Key logic off `delivery_state`, not `backend_kind`; future backends may reuse the same delivery contract.
            - Keep copy-mode helpers even if you expect mmap on your main platform.
            - Preload modules run in-process; this API is for stability and compatibility, not process isolation.
        */

        uwvm_preload_host_api_v1 const* uwvm_get_preload_host_api_v1() noexcept;
        ::std::size_t uwvm_preload_memory_descriptor_count() noexcept;
        bool uwvm_preload_memory_descriptor_at(::std::size_t, uwvm_preload_memory_descriptor_t*) noexcept;
        bool uwvm_preload_memory_read(::std::size_t, ::std::uint_least64_t, void*, ::std::size_t) noexcept;
        bool uwvm_preload_memory_write(::std::size_t, ::std::uint_least64_t, void const*, ::std::size_t) noexcept;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
