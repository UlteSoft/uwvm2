/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-19
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

#include <stdint.h>
#include <stddef.h>
#include <float.h>

#define UWAPI 2

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define UWAPI_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
# define UWAPI_STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#endif

#if !(defined(__STDC_IEC_559__) || defined(__STDC_IEC_60559_BFP__))
# error "IEC 559 BFP (IEEE 754) floating-point not supported"
#endif

UWAPI_STATIC_ASSERT(DBL_MANT_DIG == 53, "wasm_f64 must have 53-bit mantissa");
UWAPI_STATIC_ASSERT(DBL_MAX_EXP == 1024, "wasm_f64 max exponent must be 1024");
UWAPI_STATIC_ASSERT(DBL_MIN_EXP == -1021, "wasm_f64 min exponent must be -1021");

UWAPI_STATIC_ASSERT(FLT_RADIX == 2, "Radix must be 2 for IEEE 754 compliance");
UWAPI_STATIC_ASSERT(FLT_MANT_DIG == 24, "wasm_f32 must have 24-bit mantissa");
UWAPI_STATIC_ASSERT(FLT_MAX_EXP == 128, "wasm_f32 max exponent must be 128");
UWAPI_STATIC_ASSERT(FLT_MIN_EXP == -125, "wasm_f32 min exponent must be -125");

typedef void (*imported_c_handlefunc_ptr_t)(void);

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

typedef void (*capi_wasm_function)(unsigned char* res, unsigned char* para);

typedef int_least32_t wasm_i32;
typedef uint_least32_t _wasm_u32;  // non-standard
typedef int_least64_t wasm_i64;
typedef uint_least64_t _wasm_u64;  // non-standard

typedef float wasm_f32;
typedef double wasm_f64;
