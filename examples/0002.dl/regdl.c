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

/*
    This example demonstrates four practical parts of a preload DL:

    1. `uwvm_get_module_name()`
       Exposes the module name that WebAssembly imports will reference.

    2. `uwvm_function()`
       Registers ordinary imported functions such as `add_i32`.

    3. `uwvm_get_custom_handler()`
       Shows how to expose optional custom-section handlers.

    4. `uwvm_set_preload_host_api_v1()` + `inspect_memory`
       Demonstrates the stable preload memory API. The function reads the first four
       bytes of `memory[0]`, returns their sum, and replaces the second byte with `Z`.

    See `README.md` and `main.wat` in the same directory for build and runtime commands.
*/

#include "interface.h"

#include <stdatomic.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(push, 1)
#endif
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
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(pop)
#endif

static uwvm_preload_host_api_v1 const* g_preload_host_api;

static char const module_name_str[] = "dl.example";
static char const func_add_i32_name[] = "add_i32";
static char const func_do_nothing_name[] = "do_nothing";
static char const func_inspect_memory_name[] = "inspect_memory";
static char const custom_name_demo[] = "demo_custom";

static uint_least8_t const add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
static uint_least8_t const add_i32_res_types[] = {WASM_VALTYPE_I32};
static uint_least8_t const inspect_memory_res_types[] = {WASM_VALTYPE_I32};

static char const* delivery_state_name(unsigned state)
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

static int range_is_valid(uint_least64_t byte_length, uint_least64_t offset, size_t size)
{
    uint_least64_t const size64 = (uint_least64_t)size;
    return offset <= byte_length && size64 <= (byte_length - offset);
}

static int find_memory_descriptor(size_t memory_index, uwvm_preload_memory_descriptor_t* out)
{
    size_t i;
    size_t count;

    if(g_preload_host_api == 0 || out == 0) { return 0; }

    count = g_preload_host_api->memory_descriptor_count();
    for(i = 0u; i != count; ++i)
    {
        uwvm_preload_memory_descriptor_t curr;
        if(!g_preload_host_api->memory_descriptor_at(i, &curr)) { continue; }
        if(curr.memory_index == memory_index)
        {
            *out = curr;
            return 1;
        }
    }
    return 0;
}

static int direct_range_is_valid(uwvm_preload_memory_descriptor_t const* descriptor, uint_least64_t offset, size_t size)
{
    if(descriptor == 0) { return 0; }

    switch(descriptor->delivery_state)
    {
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
        {
            return range_is_valid(descriptor->byte_length, offset, size);
        }
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
        {
            if(range_is_valid(descriptor->partial_protection_limit_bytes, offset, size)) { return 1; }
            return range_is_valid(descriptor->byte_length, offset, size);
        }
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
        {
            atomic_size_t const* current_length_atomic;
            uint_least64_t current_length;

            if(descriptor->dynamic_length_atomic_object == 0) { return 0; }
            current_length_atomic = (atomic_size_t const*)descriptor->dynamic_length_atomic_object;
            current_length = (uint_least64_t)atomic_load_explicit(current_length_atomic, memory_order_acquire);
            return range_is_valid(current_length, offset, size);
        }
        default:
        {
            return 0;
        }
    }
}

static int read_memory_byte(uwvm_preload_memory_descriptor_t const* descriptor, uint_least64_t offset, uint_least8_t* out)
{
    if(descriptor == 0 || out == 0) { return 0; }

    switch(descriptor->delivery_state)
    {
        case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
        {
            return g_preload_host_api != 0 && g_preload_host_api->memory_read(descriptor->memory_index, offset, out, sizeof(*out));
        }
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
        {
            if(descriptor->mmap_view_begin == 0 || !direct_range_is_valid(descriptor, offset, sizeof(*out))) { return 0; }
            memcpy(out, (unsigned char const*)descriptor->mmap_view_begin + (size_t)offset, sizeof(*out));
            return 1;
        }
        default:
        {
            return 0;
        }
    }
}

static int write_memory_byte(uwvm_preload_memory_descriptor_t const* descriptor, uint_least64_t offset, uint_least8_t value)
{
    if(descriptor == 0) { return 0; }

    switch(descriptor->delivery_state)
    {
        case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
        {
            return g_preload_host_api != 0 && g_preload_host_api->memory_write(descriptor->memory_index, offset, &value, sizeof(value));
        }
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
        {
            if(descriptor->mmap_view_begin == 0 || !direct_range_is_valid(descriptor, offset, sizeof(value))) { return 0; }
            memcpy((unsigned char*)descriptor->mmap_view_begin + (size_t)offset, &value, sizeof(value));
            return 1;
        }
        default:
        {
            return 0;
        }
    }
}

static void add_i32_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    struct add_i32_para_t para;
    struct add_i32_res_t res;

    if(res_bytes == 0 || para_bytes == 0) { return; }

    memcpy(&para, para_bytes, sizeof(para));
    res.value = (wasm_i32)(para.lhs + para.rhs);
    memcpy(res_bytes, &res, sizeof(res));
}

static void do_nothing_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    (void)res_bytes;
    (void)para_bytes;
}

static void inspect_memory_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    struct inspect_memory_res_t res;
    uwvm_preload_memory_descriptor_t descriptor;
    uint_least8_t b0;
    uint_least8_t b1;
    uint_least8_t b2;
    uint_least8_t b3;

    (void)para_bytes;
    if(res_bytes == 0) { return; }

    res.sum = -1;
    if(!find_memory_descriptor(0u, &descriptor))
    {
        memcpy(res_bytes, &res, sizeof(res));
        fprintf(stderr, "example-dl-c: unable to resolve memory[0]\n");
        fflush(stderr);
        return;
    }

    if(!read_memory_byte(&descriptor, 0u, &b0) || !read_memory_byte(&descriptor, 1u, &b1) || !read_memory_byte(&descriptor, 2u, &b2) ||
       !read_memory_byte(&descriptor, 3u, &b3))
    {
        res.sum = -2;
        memcpy(res_bytes, &res, sizeof(res));
        fprintf(stderr, "example-dl-c: unable to read the first four bytes of memory[0]\n");
        fflush(stderr);
        return;
    }

    if(!write_memory_byte(&descriptor, 1u, (uint_least8_t)'Z'))
    {
        res.sum = -3;
        memcpy(res_bytes, &res, sizeof(res));
        fprintf(stderr, "example-dl-c: unable to write byte 1 of memory[0]\n");
        fflush(stderr);
        return;
    }

    res.sum = (wasm_i32)(b0 + b1 + b2 + b3);
    memcpy(res_bytes, &res, sizeof(res));

    fprintf(stderr,
            "example-dl-c: state=%s backend=%u bytes=[%u,%u,%u,%u] sum=%d byte_length=%" PRIuLEAST64 " partial_limit=%" PRIuLEAST64 "\n",
            delivery_state_name(descriptor.delivery_state),
            descriptor.backend_kind,
            (unsigned)b0,
            (unsigned)b1,
            (unsigned)b2,
            (unsigned)b3,
            (int)res.sum,
            descriptor.byte_length,
            descriptor.partial_protection_limit_bytes);
    fflush(stderr);
}

static void demo_custom_handle(void) {}

void uwvm_set_preload_host_api_v1(uwvm_preload_host_api_v1 const* api)
{
    g_preload_host_api = api;
}

capi_module_name_t uwvm_get_module_name(void)
{
    capi_module_name_t r;
    r.name = module_name_str;
    r.name_length = (size_t)(sizeof(module_name_str) - 1u);
    return r;
}

capi_custom_handler_vec_t uwvm_get_custom_handler(void)
{
    static capi_custom_handler_t const handlers[] = {
        {custom_name_demo, (size_t)(sizeof(custom_name_demo) - 1u), &demo_custom_handle},
    };
    capi_custom_handler_vec_t v;
    v.custom_handler_begin = handlers;
    v.custom_handler_size = (size_t)(sizeof(handlers) / sizeof(handlers[0]));
    return v;
}

capi_function_vec_t uwvm_function(void)
{
    static capi_function_t const functions[] = {
        {func_add_i32_name,
         (size_t)(sizeof(func_add_i32_name) - 1u),
         add_i32_para_types,
         (size_t)(sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0])),
         add_i32_res_types,
         (size_t)(sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0])),
         &add_i32_impl},
        {func_do_nothing_name, (size_t)(sizeof(func_do_nothing_name) - 1u), 0, 0u, 0, 0u, &do_nothing_impl},
        {func_inspect_memory_name,
         (size_t)(sizeof(func_inspect_memory_name) - 1u),
         0,
         0u,
         inspect_memory_res_types,
         (size_t)(sizeof(inspect_memory_res_types) / sizeof(inspect_memory_res_types[0])),
         &inspect_memory_impl},
    };
    capi_function_vec_t vec;
    vec.function_begin = functions;
    vec.function_size = (size_t)(sizeof(functions) / sizeof(functions[0]));
    return vec;
}
