/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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

#include "interface.h"

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    struct
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((packed))
#endif
        add_i32_para_t
    {
        wasm_i32 lhs;
        wasm_i32 rhs;
    };

    struct
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((packed))
#endif
        add_i32_res_t
    {
        wasm_i32 value;
    };

    struct
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((packed))
#endif
        inspect_memory_res_t
    {
        wasm_i32 sum;
    };

    inline uwvm_preload_host_api_v1 const* g_preload_host_api{};

    inline constexpr char module_name_str[] = "dl.example";
    inline constexpr char func_add_i32_name[] = "add_i32";
    inline constexpr char func_do_nothing_name[] = "do_nothing";
    inline constexpr char func_inspect_memory_name[] = "inspect_memory";
    inline constexpr char custom_name_demo[] = "demo_custom";

    inline constexpr std::uint_least8_t add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
    inline constexpr std::uint_least8_t add_i32_res_types[] = {WASM_VALTYPE_I32};
    inline constexpr std::uint_least8_t inspect_memory_res_types[] = {WASM_VALTYPE_I32};

    [[nodiscard]] char const* delivery_state_name(unsigned state) noexcept
    {
        switch(state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
                return "copy";
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
                return "mmap_full_protection";
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
                return "mmap_partial_protection";
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
                return "mmap_dynamic_bounds";
            case UWVM_PRELOAD_MEMORY_DELIVERY_NONE:
            default:
                return "none";
        }
    }

    [[nodiscard]] bool range_is_valid(std::uint_least64_t byte_length, std::uint_least64_t offset, std::size_t size) noexcept
    {
        auto const size64{static_cast<std::uint_least64_t>(size)};
        return offset <= byte_length && size64 <= (byte_length - offset);
    }

    [[nodiscard]] bool find_memory_descriptor(std::size_t memory_index, uwvm_preload_memory_descriptor_t& out) noexcept
    {
        if(g_preload_host_api == nullptr) { return false; }

        auto const count{g_preload_host_api->memory_descriptor_count()};
        for(std::size_t i{}; i != count; ++i)
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

    [[nodiscard]] bool direct_range_is_valid(uwvm_preload_memory_descriptor_t const& descriptor,
                                             std::uint_least64_t offset,
                                             std::size_t size) noexcept
    {
        switch(descriptor.delivery_state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
            {
                return range_is_valid(descriptor.byte_length, offset, size);
            }
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
            {
                if(range_is_valid(descriptor.partial_protection_limit_bytes, offset, size)) { return true; }
                return range_is_valid(descriptor.byte_length, offset, size);
            }
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
            {
                if(descriptor.dynamic_length_atomic_object == nullptr) { return false; }
                auto const* const current_length_atomic{
                    static_cast<std::atomic_size_t const*>(descriptor.dynamic_length_atomic_object)};
                auto const current_length{static_cast<std::uint_least64_t>(current_length_atomic->load(std::memory_order_acquire))};
                return range_is_valid(current_length, offset, size);
            }
            default:
            {
                return false;
            }
        }
    }

    [[nodiscard]] bool read_memory_byte(uwvm_preload_memory_descriptor_t const& descriptor,
                                        std::uint_least64_t offset,
                                        std::uint_least8_t& out) noexcept
    {
        switch(descriptor.delivery_state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
            {
                return g_preload_host_api != nullptr &&
                       g_preload_host_api->memory_read(descriptor.memory_index, offset, &out, sizeof(out));
            }
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
            {
                if(descriptor.mmap_view_begin == nullptr || !direct_range_is_valid(descriptor, offset, sizeof(out))) { return false; }
                std::memcpy(&out, static_cast<unsigned char const*>(descriptor.mmap_view_begin) + static_cast<std::size_t>(offset), sizeof(out));
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    [[nodiscard]] bool write_memory_byte(uwvm_preload_memory_descriptor_t const& descriptor,
                                         std::uint_least64_t offset,
                                         std::uint_least8_t value) noexcept
    {
        switch(descriptor.delivery_state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
            {
                return g_preload_host_api != nullptr &&
                       g_preload_host_api->memory_write(descriptor.memory_index, offset, &value, sizeof(value));
            }
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
            {
                if(descriptor.mmap_view_begin == nullptr || !direct_range_is_valid(descriptor, offset, sizeof(value))) { return false; }
                std::memcpy(static_cast<unsigned char*>(descriptor.mmap_view_begin) + static_cast<std::size_t>(offset), &value, sizeof(value));
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    void add_i32_impl(unsigned char* res_bytes, unsigned char* para_bytes)
    {
        if(res_bytes == nullptr || para_bytes == nullptr) { return; }

        add_i32_para_t para{};
        add_i32_res_t res{};
        std::memcpy(&para, para_bytes, sizeof(para));
        res.value = static_cast<wasm_i32>(para.lhs + para.rhs);
        std::memcpy(res_bytes, &res, sizeof(res));
    }

    void do_nothing_impl(unsigned char* res_bytes, unsigned char* para_bytes)
    {
        static_cast<void>(res_bytes);
        static_cast<void>(para_bytes);
    }

    void inspect_memory_impl(unsigned char* res_bytes, unsigned char* para_bytes)
    {
        static_cast<void>(para_bytes);
        if(res_bytes == nullptr) { return; }

        inspect_memory_res_t res{.sum = -1};
        uwvm_preload_memory_descriptor_t descriptor{};
        if(!find_memory_descriptor(0u, descriptor))
        {
            std::memcpy(res_bytes, &res, sizeof(res));
            std::fprintf(stderr, "example-dl-cxx: unable to resolve memory[0]\n");
            std::fflush(stderr);
            return;
        }

        std::uint_least8_t b0{}, b1{}, b2{}, b3{};
        if(!read_memory_byte(descriptor, 0u, b0) || !read_memory_byte(descriptor, 1u, b1) || !read_memory_byte(descriptor, 2u, b2) ||
           !read_memory_byte(descriptor, 3u, b3))
        {
            res.sum = -2;
            std::memcpy(res_bytes, &res, sizeof(res));
            std::fprintf(stderr, "example-dl-cxx: unable to read the first four bytes of memory[0]\n");
            std::fflush(stderr);
            return;
        }

        if(!write_memory_byte(descriptor, 1u, static_cast<std::uint_least8_t>('Z')))
        {
            res.sum = -3;
            std::memcpy(res_bytes, &res, sizeof(res));
            std::fprintf(stderr, "example-dl-cxx: unable to write byte 1 of memory[0]\n");
            std::fflush(stderr);
            return;
        }

        res.sum = static_cast<wasm_i32>(b0 + b1 + b2 + b3);
        std::memcpy(res_bytes, &res, sizeof(res));

        std::fprintf(stderr,
                     "example-dl-cxx: state=%s backend=%u bytes=[%u,%u,%u,%u] sum=%d byte_length=%" PRIuLEAST64 " partial_limit=%" PRIuLEAST64 "\n",
                     delivery_state_name(descriptor.delivery_state),
                     descriptor.backend_kind,
                     static_cast<unsigned>(b0),
                     static_cast<unsigned>(b1),
                     static_cast<unsigned>(b2),
                     static_cast<unsigned>(b3),
                     static_cast<int>(res.sum),
                     descriptor.byte_length,
                     descriptor.partial_protection_limit_bytes);
        std::fflush(stderr);
    }

    void demo_custom_handle() {}
}

extern "C" void uwvm_set_preload_host_api_v1(uwvm_preload_host_api_v1 const* api)
{
    g_preload_host_api = api;
}

extern "C" capi_module_name_t uwvm_get_module_name()
{
    return capi_module_name_t{module_name_str, sizeof(module_name_str) - 1u};
}

extern "C" capi_custom_handler_vec_t uwvm_get_custom_handler()
{
    static capi_custom_handler_t const handlers[] = {
        {custom_name_demo, sizeof(custom_name_demo) - 1u, &demo_custom_handle},
    };
    return capi_custom_handler_vec_t{handlers, sizeof(handlers) / sizeof(handlers[0])};
}

extern "C" capi_function_vec_t uwvm_function()
{
    static capi_function_t const functions[] = {
        {func_add_i32_name,
         sizeof(func_add_i32_name) - 1u,
         add_i32_para_types,
         sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0]),
         add_i32_res_types,
         sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0]),
         &add_i32_impl},
        {func_do_nothing_name, sizeof(func_do_nothing_name) - 1u, nullptr, 0u, nullptr, 0u, &do_nothing_impl},
        {func_inspect_memory_name,
         sizeof(func_inspect_memory_name) - 1u,
         nullptr,
         0u,
         inspect_memory_res_types,
         sizeof(inspect_memory_res_types) / sizeof(inspect_memory_res_types[0]),
         &inspect_memory_impl},
    };
    return capi_function_vec_t{functions, sizeof(functions) / sizeof(functions[0])};
}
