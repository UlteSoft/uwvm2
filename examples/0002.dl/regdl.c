/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include "interface.h"

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

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    probe_host_apis_para_t
{
    wasi_void_ptr_t argc_ptr;
    wasi_void_ptr_t argv_buf_size_ptr;
};

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    probe_host_apis_res_t
{
    wasm_i32 status;
};
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(pop)
#endif

static uwvm_preload_host_api_v1 const* g_preload_host_api;
static uwvm_wasip1_host_api_v1 const* g_wasip1_host_api;

static char const module_name_str[] = "dl.example";
static char const func_add_i32_name[] = "add_i32";
static char const func_inspect_memory_name[] = "inspect_memory";
static char const func_probe_host_apis_name[] = "probe_host_apis";
static char const custom_name_demo[] = "demo_custom";

static uint_least8_t const add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
static uint_least8_t const add_i32_res_types[] = {WASM_VALTYPE_I32};
static uint_least8_t const inspect_memory_res_types[] = {WASM_VALTYPE_I32};
static uint_least8_t const probe_host_apis_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
static uint_least8_t const probe_host_apis_res_types[] = {WASM_VALTYPE_I32};

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
    size_t count;
    size_t i;

    if(g_preload_host_api == 0 || out == 0) { return 0; }
    if(g_preload_host_api->abi_version != UWVM_EXAMPLE_PRELOAD_HOST_API_V1_ABI_VERSION) { return 0; }
    if(g_preload_host_api->struct_size < sizeof(*g_preload_host_api)) { return 0; }

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
            return range_is_valid(descriptor->byte_length, offset, size);
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
            if(range_is_valid(descriptor->partial_protection_limit_bytes, offset, size)) { return 1; }
            return range_is_valid(descriptor->byte_length, offset, size);
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
            return range_is_valid(descriptor->byte_length, offset, size);
        default:
            return 0;
    }
}

static int read_memory_bytes(uwvm_preload_memory_descriptor_t const* descriptor, uint_least64_t offset, void* out, size_t size)
{
    if(descriptor == 0 || out == 0) { return 0; }

    switch(descriptor->delivery_state)
    {
        case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
            return g_preload_host_api != 0 && g_preload_host_api->memory_read(descriptor->memory_index, offset, out, size);
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
            if(descriptor->mmap_view_begin == 0 || !direct_range_is_valid(descriptor, offset, size)) { return 0; }
            memcpy(out, (unsigned char const*)descriptor->mmap_view_begin + (size_t)offset, size);
            return 1;
        default:
            return 0;
    }
}

static int write_memory_byte(uwvm_preload_memory_descriptor_t const* descriptor, uint_least64_t offset, uint_least8_t value)
{
    if(descriptor == 0) { return 0; }

    switch(descriptor->delivery_state)
    {
        case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
            return g_preload_host_api != 0 && g_preload_host_api->memory_write(descriptor->memory_index, offset, &value, sizeof(value));
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
        case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
            if(descriptor->mmap_view_begin == 0 || !direct_range_is_valid(descriptor, offset, sizeof(value))) { return 0; }
            memcpy((unsigned char*)descriptor->mmap_view_begin + (size_t)offset, &value, sizeof(value));
            return 1;
        default:
            return 0;
    }
}

static int read_wasm_u32_le(uwvm_preload_memory_descriptor_t const* descriptor, uint_least64_t offset, wasm_u32* out)
{
    uint_least8_t bytes[4];
    if(out == 0) { return 0; }
    if(!read_memory_bytes(descriptor, offset, bytes, sizeof(bytes))) { return 0; }
    *out = (wasm_u32)bytes[0] | ((wasm_u32)bytes[1] << 8u) | ((wasm_u32)bytes[2] << 16u) | ((wasm_u32)bytes[3] << 24u);
    return 1;
}

static void write_i32_result(unsigned char* res_bytes, wasm_i32 value)
{
    if(res_bytes != 0)
    {
        wasm_i32 tmp = value;
        memcpy(res_bytes, &tmp, sizeof(tmp));
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

static void inspect_memory_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    uwvm_preload_memory_descriptor_t descriptor;
    uint_least8_t b0, b1, b2, b3;
    wasm_i32 sum;

    (void)para_bytes;

    if(!find_memory_descriptor(0u, &descriptor))
    {
        fprintf(stderr, "probe_host_apis: preload api missing\n");
        fflush(stderr);
        write_i32_result(res_bytes, -1);
        return;
    }

    if(!read_memory_bytes(&descriptor, 0u, &b0, 1u) || !read_memory_bytes(&descriptor, 1u, &b1, 1u) ||
       !read_memory_bytes(&descriptor, 2u, &b2, 1u) || !read_memory_bytes(&descriptor, 3u, &b3, 1u))
    {
        fprintf(stderr, "probe_host_apis: preload api abi mismatch\n");
        fflush(stderr);
        write_i32_result(res_bytes, -2);
        return;
    }

    if(!write_memory_byte(&descriptor, 1u, (uint_least8_t)'Z'))
    {
        fprintf(stderr, "probe_host_apis: memory[0] descriptor missing\n");
        fflush(stderr);
        write_i32_result(res_bytes, -3);
        return;
    }

    sum = (wasm_i32)(b0 + b1 + b2 + b3);
    write_i32_result(res_bytes, sum);

    fprintf(stderr,
            "example-dl-c: state=%s backend=%u bytes=[%u,%u,%u,%u] sum=%d\n",
            delivery_state_name(descriptor.delivery_state),
            descriptor.backend_kind,
            (unsigned)b0,
            (unsigned)b1,
            (unsigned)b2,
            (unsigned)b3,
            (int)sum);
    fflush(stderr);
}

static void probe_host_apis_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    struct probe_host_apis_para_t para;
    uwvm_preload_memory_descriptor_t descriptor;
    wasm_u32 argc_value;
    wasm_u32 argv_buf_size_value;
    uwvm_wasi_errno_t err;

    if(res_bytes == 0 || para_bytes == 0)
    {
        return;
    }

    memcpy(&para, para_bytes, sizeof(para));

    if(g_preload_host_api == 0)
    {
        write_i32_result(res_bytes, -1);
        return;
    }
    if(g_preload_host_api->abi_version != UWVM_EXAMPLE_PRELOAD_HOST_API_V1_ABI_VERSION || g_preload_host_api->struct_size < sizeof(*g_preload_host_api))
    {
        write_i32_result(res_bytes, -2);
        return;
    }
    if(!find_memory_descriptor(0u, &descriptor))
    {
        write_i32_result(res_bytes, -3);
        return;
    }
    if(g_wasip1_host_api == 0)
    {
        fprintf(stderr, "probe_host_apis: wasip1 api missing (gate off or setter not called)\n");
        fflush(stderr);
        write_i32_result(res_bytes, -4);
        return;
    }
    if(g_wasip1_host_api->abi_version != UWVM_EXAMPLE_WASIP1_HOST_API_V1_ABI_VERSION || g_wasip1_host_api->struct_size < sizeof(*g_wasip1_host_api))
    {
        fprintf(stderr, "probe_host_apis: wasip1 api abi mismatch\n");
        fflush(stderr);
        write_i32_result(res_bytes, -5);
        return;
    }

    err = g_wasip1_host_api->args_sizes_get(para.argc_ptr, para.argv_buf_size_ptr);
    if(err != UWVM_EXAMPLE_WASI_ERRNO_SUCCESS)
    {
        fprintf(stderr, "probe_host_apis: args_sizes_get returned error\n");
        fflush(stderr);
        write_i32_result(res_bytes, -6);
        return;
    }

    if(!read_wasm_u32_le(&descriptor, para.argc_ptr, &argc_value))
    {
        fprintf(stderr, "probe_host_apis: failed to read argc value\n");
        fflush(stderr);
        write_i32_result(res_bytes, -7);
        return;
    }
    if(!read_wasm_u32_le(&descriptor, para.argv_buf_size_ptr, &argv_buf_size_value))
    {
        fprintf(stderr, "probe_host_apis: failed to read argv_buf_size value\n");
        fflush(stderr);
        write_i32_result(res_bytes, -8);
        return;
    }
    fprintf(stderr, "example-dl-c: wasi argc=%" PRIuLEAST32 " argv_buf_size=%" PRIuLEAST32 "\n", argc_value, argv_buf_size_value);
    fflush(stderr);
    write_i32_result(res_bytes, 0);
}

static void demo_custom_handle(void) {}

void uwvm_set_preload_host_api_v1(uwvm_preload_host_api_v1 const* api)
{
    g_preload_host_api = api;
    fprintf(stderr, "setter: preload api=%p\n", (void const*)api);
    fflush(stderr);
}

void uwvm_set_wasip1_host_api_v1(uwvm_wasip1_host_api_v1 const* api)
{
    g_wasip1_host_api = api;
    fprintf(stderr, "setter: wasip1 api=%p\n", (void const*)api);
    fflush(stderr);
}

capi_module_name_t uwvm_get_module_name(void)
{
    capi_module_name_t ret;
    ret.name = module_name_str;
    ret.name_length = sizeof(module_name_str) - 1u;
    return ret;
}

capi_custom_handler_vec_t uwvm_get_custom_handler(void)
{
    static capi_custom_handler_t const handlers[] = {
        {custom_name_demo, sizeof(custom_name_demo) - 1u, &demo_custom_handle},
    };
    capi_custom_handler_vec_t ret;
    ret.custom_handler_begin = handlers;
    ret.custom_handler_size = sizeof(handlers) / sizeof(handlers[0]);
    return ret;
}

capi_function_vec_t uwvm_function(void)
{
    static capi_function_t const functions[] = {
        {func_add_i32_name,
         sizeof(func_add_i32_name) - 1u,
         add_i32_para_types,
         sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0]),
         add_i32_res_types,
         sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0]),
         &add_i32_impl},
        {func_inspect_memory_name,
         sizeof(func_inspect_memory_name) - 1u,
         0,
         0u,
         inspect_memory_res_types,
         sizeof(inspect_memory_res_types) / sizeof(inspect_memory_res_types[0]),
         &inspect_memory_impl},
        {func_probe_host_apis_name,
         sizeof(func_probe_host_apis_name) - 1u,
         probe_host_apis_para_types,
         sizeof(probe_host_apis_para_types) / sizeof(probe_host_apis_para_types[0]),
         probe_host_apis_res_types,
         sizeof(probe_host_apis_res_types) / sizeof(probe_host_apis_res_types[0]),
         &probe_host_apis_impl},
    };
    capi_function_vec_t ret;
    ret.function_begin = functions;
    ret.function_size = sizeof(functions) / sizeof(functions[0]);
    return ret;
}
