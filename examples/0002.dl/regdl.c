/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
 * @version     2.0.0
 * @copyright   MIT
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// Minimal example dynamic library for UWVM2 DL loader

#include <stddef.h>
#include <stdint.h>

// Local C ABI definitions matching UWVM loader expectations
typedef void (*capi_wasm_function)(unsigned char* res, unsigned char* para);

typedef struct capi_module_name_t_def
{
    char const* name;
    size_t name_length;
} capi_module_name_t;

typedef void (*imported_c_handlefunc_ptr_t)(void);

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

// Define function descriptors expected by UWVM loader (binary-compatible with uwvm2::uwvm::wasm::type::capi_function_t)
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

// WASM value_type encodings (wasm 1.0): i32=0x7F, i64=0x7E, f32=0x7D, f64=0x7C

// c23, or you can use macro
enum : uint_least8_t
{
    WASM_VALTYPE_I32 = 0x7F,
    WASM_VALTYPE_I64 = 0x7E,
    WASM_VALTYPE_F32 = 0x7D,
    WASM_VALTYPE_F64 = 0x7C
};

// Packed parameter/result structs for the example function
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_para_t
{
    int_least32_t a;
    int_least32_t b;
};

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_res_t
{
    int_least32_t sum;
};
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(pop)
#endif

static void add_i32_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    struct add_i32_para_t const* const para = (struct add_i32_para_t const*)para_bytes;
    struct add_i32_res_t* const res = (struct add_i32_res_t*)res_bytes;
    res->sum = para->a + para->b;
}

// Static metadata for the function
static uint_least8_t const add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
static uint_least8_t const add_i32_res_types[] = {WASM_VALTYPE_I32};

static char const module_name_str[] = "example";
static char const func_add_i32_name[] = "add_i32";
static char const func_do_nothing_name[] = "do_nothing";
static char const custom_name_demo[] = "demo_custom";

// void() -> void
static void do_nothing_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    (void)res_bytes;
    (void)para_bytes;
}

// demo custom handler
static void demo_custom_handle(void) {}

// Exported C symbols queried by UWVM loader
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
        {
         /* custom_name_ptr */ custom_name_demo,
         /* custom_name_length */ (size_t)(sizeof(custom_name_demo) - 1u),
         /* custom_handle_func */ &demo_custom_handle,
         },
    };

    capi_custom_handler_vec_t v;
    v.custom_handler_begin = handlers;
    v.custom_handler_size = (size_t)(sizeof(handlers) / sizeof(handlers[0]));
    return v;
}

capi_function_vec_t uwvm_function(void)
{
    static capi_function_t const functions[] = {
        {
         /* func_name_ptr */ func_add_i32_name,
         /* func_name_length */ (size_t)(sizeof(func_add_i32_name) - 1u),
         /* para_type_vec_begin */ add_i32_para_types,
         /* para_type_vec_size */ (size_t)(sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0])),
         /* res_type_vec_begin */ add_i32_res_types,
         /* res_type_vec_size */ (size_t)(sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0])),
         /* func_ptr */ (capi_wasm_function)&add_i32_impl,
         },
        {
         /* func_name_ptr */ func_do_nothing_name,
         /* func_name_length */ (size_t)(sizeof(func_do_nothing_name) - 1u),
         /* para_type_vec_begin */ (uint_least8_t const*)0,
         /* para_type_vec_size */ 0u,
         /* res_type_vec_begin */ (uint_least8_t const*)0,
         /* res_type_vec_size */ 0u,
         /* func_ptr */ (capi_wasm_function)&do_nothing_impl,
         },
    };

    capi_function_vec_t vec;
    vec.function_begin = functions;
    vec.function_size = (size_t)(sizeof(functions) / sizeof(functions[0]));
    return vec;
}

// clang -shared -o regdl regdl.c  --sysroot=$SYSROOT -fuse-ld=lld
// gcc -shared -o regdl regdl.c
