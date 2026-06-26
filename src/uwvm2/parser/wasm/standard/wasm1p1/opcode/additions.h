/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Opcode definitions for instructions added after WebAssembly 1.0.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstdint>
# include <cstddef>
# include <concepts>
// macro
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::opcode
{
    enum class op_basic : ::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type
    {
        // Parametric instructions
        select_t = 0x1c,

        // Table instructions
        table_get = 0x25,
        table_set = 0x26,

        // Sign-extension instructions
        i32_extend8_s = 0xc0,
        i32_extend16_s = 0xc1,
        i64_extend8_s = 0xc2,
        i64_extend16_s = 0xc3,
        i64_extend32_s = 0xc4,

        // Reference instructions
        ref_null = 0xd0,
        ref_is_null = 0xd1,
        ref_func = 0xd2,

        // Prefixed instruction spaces
        numeric_prefix = 0xfc,
        simd_prefix = 0xfd
    };

    enum class op_numeric : ::uwvm2::parser::wasm::standard::wasm1::type::op_exten_type
    {
        // Non-trapping float-to-int conversions
        i32_trunc_sat_f32_s = 0x00,
        i32_trunc_sat_f32_u = 0x01,
        i32_trunc_sat_f64_s = 0x02,
        i32_trunc_sat_f64_u = 0x03,
        i64_trunc_sat_f32_s = 0x04,
        i64_trunc_sat_f32_u = 0x05,
        i64_trunc_sat_f64_s = 0x06,
        i64_trunc_sat_f64_u = 0x07,

        // Bulk memory and reference-table instructions
        memory_init = 0x08,
        data_drop = 0x09,
        memory_copy = 0x0a,
        memory_fill = 0x0b,
        table_init = 0x0c,
        elem_drop = 0x0d,
        table_copy = 0x0e,
        table_grow = 0x0f,
        table_size = 0x10,
        table_fill = 0x11
    };

    enum class op_simd : ::uwvm2::parser::wasm::standard::wasm1::type::op_exten_type
    {
        v128_load = 0x00,
        v128_load8x8_s = 0x01,
        v128_load8x8_u = 0x02,
        v128_load16x4_s = 0x03,
        v128_load16x4_u = 0x04,
        v128_load32x2_s = 0x05,
        v128_load32x2_u = 0x06,
        v128_load8_splat = 0x07,
        v128_load16_splat = 0x08,
        v128_load32_splat = 0x09,
        v128_load64_splat = 0x0a,
        v128_store = 0x0b,
        v128_const = 0x0c,
        i8x16_shuffle = 0x0d,
        i8x16_swizzle = 0x0e,
        i8x16_splat = 0x0f,
        i16x8_splat = 0x10,
        i32x4_splat = 0x11,
        i64x2_splat = 0x12,
        f32x4_splat = 0x13,
        f64x2_splat = 0x14,
        i8x16_extract_lane_s = 0x15,
        i8x16_extract_lane_u = 0x16,
        i8x16_replace_lane = 0x17,
        i16x8_extract_lane_s = 0x18,
        i16x8_extract_lane_u = 0x19,
        i16x8_replace_lane = 0x1a,
        i32x4_extract_lane = 0x1b,
        i32x4_replace_lane = 0x1c,
        i64x2_extract_lane = 0x1d,
        i64x2_replace_lane = 0x1e,
        f32x4_extract_lane = 0x1f,
        f32x4_replace_lane = 0x20,
        f64x2_extract_lane = 0x21,
        f64x2_replace_lane = 0x22,

        i8x16_eq = 0x23,
        i8x16_ne = 0x24,
        i8x16_lt_s = 0x25,
        i8x16_lt_u = 0x26,
        i8x16_gt_s = 0x27,
        i8x16_gt_u = 0x28,
        i8x16_le_s = 0x29,
        i8x16_le_u = 0x2a,
        i8x16_ge_s = 0x2b,
        i8x16_ge_u = 0x2c,
        i16x8_eq = 0x2d,
        i16x8_ne = 0x2e,
        i16x8_lt_s = 0x2f,
        i16x8_lt_u = 0x30,
        i16x8_gt_s = 0x31,
        i16x8_gt_u = 0x32,
        i16x8_le_s = 0x33,
        i16x8_le_u = 0x34,
        i16x8_ge_s = 0x35,
        i16x8_ge_u = 0x36,
        i32x4_eq = 0x37,
        i32x4_ne = 0x38,
        i32x4_lt_s = 0x39,
        i32x4_lt_u = 0x3a,
        i32x4_gt_s = 0x3b,
        i32x4_gt_u = 0x3c,
        i32x4_le_s = 0x3d,
        i32x4_le_u = 0x3e,
        i32x4_ge_s = 0x3f,
        i32x4_ge_u = 0x40,
        f32x4_eq = 0x41,
        f32x4_ne = 0x42,
        f32x4_lt = 0x43,
        f32x4_gt = 0x44,
        f32x4_le = 0x45,
        f32x4_ge = 0x46,
        f64x2_eq = 0x47,
        f64x2_ne = 0x48,
        f64x2_lt = 0x49,
        f64x2_gt = 0x4a,
        f64x2_le = 0x4b,
        f64x2_ge = 0x4c,

        v128_not = 0x4d,
        v128_and = 0x4e,
        v128_andnot = 0x4f,
        v128_or = 0x50,
        v128_xor = 0x51,
        v128_bitselect = 0x52,
        v128_any_true = 0x53,
        v128_load8_lane = 0x54,
        v128_load16_lane = 0x55,
        v128_load32_lane = 0x56,
        v128_load64_lane = 0x57,
        v128_store8_lane = 0x58,
        v128_store16_lane = 0x59,
        v128_store32_lane = 0x5a,
        v128_store64_lane = 0x5b,
        v128_load32_zero = 0x5c,
        v128_load64_zero = 0x5d,
        f32x4_demote_f64x2_zero = 0x5e,
        f64x2_promote_low_f32x4 = 0x5f,

        i8x16_abs = 0x60,
        i8x16_neg = 0x61,
        i8x16_popcnt = 0x62,
        i8x16_all_true = 0x63,
        i8x16_bitmask = 0x64,
        i8x16_narrow_i16x8_s = 0x65,
        i8x16_narrow_i16x8_u = 0x66,
        f32x4_ceil = 0x67,
        f32x4_floor = 0x68,
        f32x4_trunc = 0x69,
        f32x4_nearest = 0x6a,
        i8x16_shl = 0x6b,
        i8x16_shr_s = 0x6c,
        i8x16_shr_u = 0x6d,
        i8x16_add = 0x6e,
        i8x16_add_sat_s = 0x6f,
        i8x16_add_sat_u = 0x70,
        i8x16_sub = 0x71,
        i8x16_sub_sat_s = 0x72,
        i8x16_sub_sat_u = 0x73,
        f64x2_ceil = 0x74,
        f64x2_floor = 0x75,
        i8x16_min_s = 0x76,
        i8x16_min_u = 0x77,
        i8x16_max_s = 0x78,
        i8x16_max_u = 0x79,
        f64x2_trunc = 0x7a,
        i8x16_avgr_u = 0x7b,
        i16x8_extadd_pairwise_i8x16_s = 0x7c,
        i16x8_extadd_pairwise_i8x16_u = 0x7d,
        i32x4_extadd_pairwise_i16x8_s = 0x7e,
        i32x4_extadd_pairwise_i16x8_u = 0x7f,

        i16x8_abs = 0x80,
        i16x8_neg = 0x81,
        i16x8_q15mulr_sat_s = 0x82,
        i16x8_all_true = 0x83,
        i16x8_bitmask = 0x84,
        i16x8_narrow_i32x4_s = 0x85,
        i16x8_narrow_i32x4_u = 0x86,
        i16x8_extend_low_i8x16_s = 0x87,
        i16x8_extend_high_i8x16_s = 0x88,
        i16x8_extend_low_i8x16_u = 0x89,
        i16x8_extend_high_i8x16_u = 0x8a,
        i16x8_shl = 0x8b,
        i16x8_shr_s = 0x8c,
        i16x8_shr_u = 0x8d,
        i16x8_add = 0x8e,
        i16x8_add_sat_s = 0x8f,
        i16x8_add_sat_u = 0x90,
        i16x8_sub = 0x91,
        i16x8_sub_sat_s = 0x92,
        i16x8_sub_sat_u = 0x93,
        f64x2_nearest = 0x94,
        i16x8_mul = 0x95,
        i16x8_min_s = 0x96,
        i16x8_min_u = 0x97,
        i16x8_max_s = 0x98,
        i16x8_max_u = 0x99,
        i16x8_avgr_u = 0x9b,
        i16x8_extmul_low_i8x16_s = 0x9c,
        i16x8_extmul_high_i8x16_s = 0x9d,
        i16x8_extmul_low_i8x16_u = 0x9e,
        i16x8_extmul_high_i8x16_u = 0x9f,

        i32x4_abs = 0xa0,
        i32x4_neg = 0xa1,
        i32x4_all_true = 0xa3,
        i32x4_bitmask = 0xa4,
        i32x4_extend_low_i16x8_s = 0xa7,
        i32x4_extend_high_i16x8_s = 0xa8,
        i32x4_extend_low_i16x8_u = 0xa9,
        i32x4_extend_high_i16x8_u = 0xaa,
        i32x4_shl = 0xab,
        i32x4_shr_s = 0xac,
        i32x4_shr_u = 0xad,
        i32x4_add = 0xae,
        i32x4_sub = 0xb1,
        i32x4_mul = 0xb5,
        i32x4_min_s = 0xb6,
        i32x4_min_u = 0xb7,
        i32x4_max_s = 0xb8,
        i32x4_max_u = 0xb9,
        i32x4_dot_i16x8_s = 0xba,
        i32x4_extmul_low_i16x8_s = 0xbc,
        i32x4_extmul_high_i16x8_s = 0xbd,
        i32x4_extmul_low_i16x8_u = 0xbe,
        i32x4_extmul_high_i16x8_u = 0xbf,

        i64x2_abs = 0xc0,
        i64x2_neg = 0xc1,
        i64x2_all_true = 0xc3,
        i64x2_bitmask = 0xc4,
        i64x2_extend_low_i32x4_s = 0xc7,
        i64x2_extend_high_i32x4_s = 0xc8,
        i64x2_extend_low_i32x4_u = 0xc9,
        i64x2_extend_high_i32x4_u = 0xca,
        i64x2_shl = 0xcb,
        i64x2_shr_s = 0xcc,
        i64x2_shr_u = 0xcd,
        i64x2_add = 0xce,
        i64x2_sub = 0xd1,
        i64x2_mul = 0xd5,
        i64x2_eq = 0xd6,
        i64x2_ne = 0xd7,
        i64x2_lt_s = 0xd8,
        i64x2_gt_s = 0xd9,
        i64x2_le_s = 0xda,
        i64x2_ge_s = 0xdb,
        i64x2_extmul_low_i32x4_s = 0xdc,
        i64x2_extmul_high_i32x4_s = 0xdd,
        i64x2_extmul_low_i32x4_u = 0xde,
        i64x2_extmul_high_i32x4_u = 0xdf,

        f32x4_abs = 0xe0,
        f32x4_neg = 0xe1,
        f32x4_sqrt = 0xe3,
        f32x4_add = 0xe4,
        f32x4_sub = 0xe5,
        f32x4_mul = 0xe6,
        f32x4_div = 0xe7,
        f32x4_min = 0xe8,
        f32x4_max = 0xe9,
        f32x4_pmin = 0xea,
        f32x4_pmax = 0xeb,
        f64x2_abs = 0xec,
        f64x2_neg = 0xed,
        f64x2_sqrt = 0xef,
        f64x2_add = 0xf0,
        f64x2_sub = 0xf1,
        f64x2_mul = 0xf2,
        f64x2_div = 0xf3,
        f64x2_min = 0xf4,
        f64x2_max = 0xf5,
        f64x2_pmin = 0xf6,
        f64x2_pmax = 0xf7,
        i32x4_trunc_sat_f32x4_s = 0xf8,
        i32x4_trunc_sat_f32x4_u = 0xf9,
        f32x4_convert_i32x4_s = 0xfa,
        f32x4_convert_i32x4_u = 0xfb,
        i32x4_trunc_sat_f64x2_s_zero = 0xfc,
        i32x4_trunc_sat_f64x2_u_zero = 0xfd,
        f64x2_convert_low_i32x4_s = 0xfe,
        f64x2_convert_low_i32x4_u = 0xff
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
#endif
