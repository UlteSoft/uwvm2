/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-12
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

#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UWVM_EXAMPLE_PRELOAD_HOST_API_V1_ABI_VERSION 1u
#define UWVM_EXAMPLE_WASIP1_HOST_API_V1_ABI_VERSION 1u
#define UWVM_EXAMPLE_WASI_ERRNO_SUCCESS ((uint_least16_t)0u)

#if defined(__cplusplus)
# define UWVM_EXAMPLE_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
# define UWVM_EXAMPLE_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

UWVM_EXAMPLE_STATIC_ASSERT(DBL_MANT_DIG == 53, "wasm_f64 must have a 53-bit mantissa");
UWVM_EXAMPLE_STATIC_ASSERT(DBL_MAX_EXP == 1024, "wasm_f64 max exponent must be 1024");
UWVM_EXAMPLE_STATIC_ASSERT(DBL_MIN_EXP == -1021, "wasm_f64 min exponent must be -1021");
UWVM_EXAMPLE_STATIC_ASSERT(FLT_RADIX == 2, "IEEE 754 radix must be 2");
UWVM_EXAMPLE_STATIC_ASSERT(FLT_MANT_DIG == 24, "wasm_f32 must have a 24-bit mantissa");
UWVM_EXAMPLE_STATIC_ASSERT(FLT_MAX_EXP == 128, "wasm_f32 max exponent must be 128");
UWVM_EXAMPLE_STATIC_ASSERT(FLT_MIN_EXP == -125, "wasm_f32 min exponent must be -125");

typedef void (*imported_c_handlefunc_ptr_t)(void);
typedef void (*capi_wasm_function)(unsigned char* res, unsigned char* para);

typedef int_least32_t wasm_i32;
typedef uint_least32_t wasm_u32;
typedef int_least64_t wasm_i64;
typedef uint_least64_t wasm_u64;
typedef float wasm_f32;
typedef double wasm_f64;

enum
{
    WASM_VALTYPE_I32 = 0x7F,
    WASM_VALTYPE_I64 = 0x7E,
    WASM_VALTYPE_F32 = 0x7D,
    WASM_VALTYPE_F64 = 0x7C
};

typedef struct capi_module_name_t_def
{
    char const* name;
    size_t name_length;
} capi_module_name_t;

typedef capi_module_name_t (*capi_get_module_name_t)(void);

typedef struct capi_custom_handler_t_def
{
    char const* custom_name_ptr;
    size_t custom_name_length;
    imported_c_handlefunc_ptr_t custom_handle_func;
} capi_custom_handler_t;

typedef struct capi_custom_handler_vec_t_def
{
    capi_custom_handler_t const* custom_handler_begin;
    size_t custom_handler_size;
} capi_custom_handler_vec_t;

typedef capi_custom_handler_vec_t (*capi_get_custom_handler_vec_t)(void);

typedef struct capi_function_t_def
{
    char const* func_name_ptr;
    size_t func_name_length;
    uint_least8_t const* para_type_vec_begin;
    size_t para_type_vec_size;
    uint_least8_t const* res_type_vec_begin;
    size_t res_type_vec_size;
    capi_wasm_function func_ptr;
} capi_function_t;

typedef struct capi_function_vec_t_def
{
    capi_function_t const* function_begin;
    size_t function_size;
} capi_function_vec_t;

typedef enum uwvm_preload_memory_delivery_state_t_def
{
    UWVM_PRELOAD_MEMORY_DELIVERY_NONE = 0u,
    UWVM_PRELOAD_MEMORY_DELIVERY_COPY,
    UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION,
    UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION,
    UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS
} uwvm_preload_memory_delivery_state_t;

typedef enum uwvm_preload_memory_backend_kind_t_def
{
    UWVM_PRELOAD_MEMORY_BACKEND_NATIVE_DEFINED = 0u,
    UWVM_PRELOAD_MEMORY_BACKEND_LOCAL_IMPORTED
} uwvm_preload_memory_backend_kind_t;

typedef struct uwvm_preload_memory_descriptor_t_def
{
    size_t memory_index;
    unsigned delivery_state;
    unsigned backend_kind;
    unsigned reserved0;
    unsigned reserved1;
    uint_least64_t page_count;
    uint_least64_t page_size_bytes;
    uint_least64_t byte_length;
    uint_least64_t partial_protection_limit_bytes;
    void* mmap_view_begin;
    void const* dynamic_length_atomic_object;
} uwvm_preload_memory_descriptor_t;

typedef size_t (*uwvm_preload_memory_descriptor_count_t)(void);
typedef bool (*uwvm_preload_memory_descriptor_at_t)(size_t, uwvm_preload_memory_descriptor_t*);
typedef bool (*uwvm_preload_memory_read_t)(size_t, uint_least64_t, void*, size_t);
typedef bool (*uwvm_preload_memory_write_t)(size_t, uint_least64_t, void const*, size_t);

typedef struct uwvm_preload_host_api_v1_def
{
    size_t struct_size;
    uint_least32_t abi_version;
    uwvm_preload_memory_descriptor_count_t memory_descriptor_count;
    uwvm_preload_memory_descriptor_at_t memory_descriptor_at;
    uwvm_preload_memory_read_t memory_read;
    uwvm_preload_memory_write_t memory_write;
} uwvm_preload_host_api_v1;

typedef void (*uwvm_set_preload_host_api_v1_t)(uwvm_preload_host_api_v1 const*);

/*
    This is intentionally a prefix projection of the full UWVM2 WASI Preview 1 table.
    The real runtime object is larger; examples only need the first two entries.
*/
typedef wasm_u32 wasi_void_ptr_t;
typedef uint_least16_t uwvm_wasi_errno_t;

typedef uwvm_wasi_errno_t (*uwvm_wasip1_args_get_t)(wasi_void_ptr_t argv_ptrsz, wasi_void_ptr_t argv_buf_ptrsz);
typedef uwvm_wasi_errno_t (*uwvm_wasip1_args_sizes_get_t)(wasi_void_ptr_t argc_ptrsz, wasi_void_ptr_t argv_buf_size_ptrsz);

typedef struct uwvm_wasip1_host_api_v1_def
{
    size_t struct_size;
    uint_least32_t abi_version;
    uwvm_wasip1_args_get_t args_get;
    uwvm_wasip1_args_sizes_get_t args_sizes_get;
} uwvm_wasip1_host_api_v1;

typedef void (*uwvm_set_wasip1_host_api_v1_t)(uwvm_wasip1_host_api_v1 const*);

typedef struct uwvm_weak_symbol_module_c_def
{
    char const* module_name_ptr;
    size_t module_name_length;
    capi_custom_handler_vec_t custom_handler_vec;
    capi_function_vec_t function_vec;
    uwvm_set_preload_host_api_v1_t set_preload_host_api_v1;
    uwvm_set_wasip1_host_api_v1_t set_wasip1_host_api_v1;
} uwvm_weak_symbol_module_c;

typedef struct uwvm_weak_symbol_module_vector_c_def
{
    uwvm_weak_symbol_module_c const* module_ptr;
    size_t module_count;
} uwvm_weak_symbol_module_vector_c;
