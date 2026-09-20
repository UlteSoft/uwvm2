/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       Differential fuzzer for the WebAssembly 2.0 validator and WABT
 * @details     The first input byte independently selects all eight Release 2.0 feature groups.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-07-11
 */

// WABT uses assert() in malformed-input paths. A fuzzer input must be reported as invalid instead of aborting there.
#ifndef NDEBUG
# define NDEBUG 1
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
# include <uwvm2/validation/concepts/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#  pragma clang diagnostic ignored "-Wnested-anon-types"
#  pragma clang diagnostic ignored "-Wunused-function"
#  pragma clang diagnostic ignored "-Wunused-parameter"
# endif

# include "wabt/binary-reader-ir.h"
# include "wabt/validator.h"

# include "wasm1_code_section_module_builder.h"

# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1_feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1_feature = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2_feature = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1_feature, wasm1p1_feature, wasm2_feature>;
    using module_storage_t =
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1_feature, wasm1p1_feature, wasm2_feature>;

    extern "C" char const* __asan_default_options() { return "detect_leaks=0"; }

    struct feature_config_t
    {
        ::std::uint8_t mask{};
        bool sign_extension{};
        bool nontrapping_float_to_int{};
        bool multi_value{};
        bool reference_types{};
        bool table_instructions{};
        bool multiple_tables{};
        bool bulk_memory{};
        bool simd{};
    };

    struct validation_result_t
    {
        bool parsed{};
        bool validated{};
        ::uwvm2::parser::wasm::base::wasm_parse_error_code parse_error{};
        ::uwvm2::validation::error::code_validation_error_code validation_error{};
        ::std::size_t function_index{};
        ::std::uint32_t feature_required_value{};
        ::std::uint32_t feature_required_kind{};
        bool known_wabt_oracle_gap{};
    };

    /// @brief Read the u32 LEB immediate immediately following one expected single-byte opcode.
    /// @details Feature-required diagnostics point at the opcode byte, before UWVM consumes a prefixed subopcode.
    [[nodiscard]] inline constexpr bool read_u32_leb_after_opcode(::std::byte const* const op_begin,
                                                                  ::std::byte const* const code_end,
                                                                  ::std::uint8_t const expected_opcode,
                                                                  ::std::uint32_t& value) noexcept
    {
        if(op_begin == nullptr || code_end == nullptr || op_begin >= code_end ||
           ::std::to_integer<::std::uint8_t>(*op_begin) != expected_opcode)
        {
            return false;
        }

        auto curr{op_begin + 1uz};
        ::std::uint32_t decoded{};
        for(unsigned byte_index{}; byte_index != 5u && curr != code_end; ++byte_index, ++curr)
        {
            auto const byte{::std::to_integer<::std::uint8_t>(*curr)};
            if(byte_index == 4u && (byte & 0xf0u) != 0u) { return false; }
            decoded |= static_cast<::std::uint32_t>(byte & 0x7fu) << (byte_index * 7u);
            if((byte & 0x80u) == 0u)
            {
                value = decoded;
                return true;
            }
        }
        return false;
    }

    /// @brief Return whether WABT Release 2 currently omits the SIMD feature gate for one standardized subopcode.
    /// @details These 80 entries are the exact difference between WABT's Release 2 SIMD opcode table and the cases routed
    ///          to `features.simd_enabled()` by `Opcode::IsEnabled`. No other disabled-feature rejection is relaxed.
    [[nodiscard]] inline constexpr bool wabt_release2_missing_simd_opcode_gate(::std::uint32_t const subopcode) noexcept
    {
        using simd_opcode = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;
        switch(static_cast<simd_opcode>(subopcode))
        {
            case simd_opcode::v128_load8x8_s:                        // 0x01 v128.load8x8_s
            case simd_opcode::v128_load8x8_u:                        // 0x02 v128.load8x8_u
            case simd_opcode::v128_load16x4_s:                       // 0x03 v128.load16x4_s
            case simd_opcode::v128_load16x4_u:                       // 0x04 v128.load16x4_u
            case simd_opcode::v128_load32x2_s:                       // 0x05 v128.load32x2_s
            case simd_opcode::v128_load32x2_u:                       // 0x06 v128.load32x2_u
            case simd_opcode::v128_andnot:                           // 0x4f v128.andnot
            case simd_opcode::v128_load32_zero:                      // 0x5c v128.load32_zero
            case simd_opcode::v128_load64_zero:                      // 0x5d v128.load64_zero
            case simd_opcode::f32x4_demote_f64x2_zero:               // 0x5e f32x4.demote_f64x2_zero
            case simd_opcode::f64x2_promote_low_f32x4:               // 0x5f f64x2.promote_low_f32x4
            case simd_opcode::i8x16_popcnt:                          // 0x62 i8x16.popcnt
            case simd_opcode::i8x16_narrow_i16x8_s:                  // 0x65 i8x16.narrow_i16x8_s
            case simd_opcode::i8x16_narrow_i16x8_u:                  // 0x66 i8x16.narrow_i16x8_u
            case simd_opcode::f32x4_ceil:                            // 0x67 f32x4.ceil
            case simd_opcode::f32x4_floor:                           // 0x68 f32x4.floor
            case simd_opcode::f32x4_trunc:                           // 0x69 f32x4.trunc
            case simd_opcode::f32x4_nearest:                         // 0x6a f32x4.nearest
            case simd_opcode::f64x2_ceil:                            // 0x74 f64x2.ceil
            case simd_opcode::f64x2_floor:                           // 0x75 f64x2.floor
            case simd_opcode::i8x16_min_s:                           // 0x76 i8x16.min_s
            case simd_opcode::i8x16_min_u:                           // 0x77 i8x16.min_u
            case simd_opcode::i8x16_max_s:                           // 0x78 i8x16.max_s
            case simd_opcode::i8x16_max_u:                           // 0x79 i8x16.max_u
            case simd_opcode::f64x2_trunc:                           // 0x7a f64x2.trunc
            case simd_opcode::i8x16_avgr_u:                          // 0x7b i8x16.avgr_u
            case simd_opcode::i16x8_extadd_pairwise_i8x16_s:         // 0x7c i16x8.extadd_pairwise_i8x16_s
            case simd_opcode::i16x8_extadd_pairwise_i8x16_u:         // 0x7d i16x8.extadd_pairwise_i8x16_u
            case simd_opcode::i32x4_extadd_pairwise_i16x8_s:         // 0x7e i32x4.extadd_pairwise_i16x8_s
            case simd_opcode::i32x4_extadd_pairwise_i16x8_u:         // 0x7f i32x4.extadd_pairwise_i16x8_u
            case simd_opcode::i16x8_q15mulr_sat_s:                   // 0x82 i16x8.q15mulr_sat_s
            case simd_opcode::i16x8_narrow_i32x4_s:                  // 0x85 i16x8.narrow_i32x4_s
            case simd_opcode::i16x8_narrow_i32x4_u:                  // 0x86 i16x8.narrow_i32x4_u
            case simd_opcode::i16x8_extend_low_i8x16_s:              // 0x87 i16x8.extend_low_i8x16_s
            case simd_opcode::i16x8_extend_high_i8x16_s:             // 0x88 i16x8.extend_high_i8x16_s
            case simd_opcode::i16x8_extend_low_i8x16_u:              // 0x89 i16x8.extend_low_i8x16_u
            case simd_opcode::i16x8_extend_high_i8x16_u:             // 0x8a i16x8.extend_high_i8x16_u
            case simd_opcode::f64x2_nearest:                         // 0x94 f64x2.nearest
            case simd_opcode::i16x8_min_s:                           // 0x96 i16x8.min_s
            case simd_opcode::i16x8_min_u:                           // 0x97 i16x8.min_u
            case simd_opcode::i16x8_max_s:                           // 0x98 i16x8.max_s
            case simd_opcode::i16x8_max_u:                           // 0x99 i16x8.max_u
            case simd_opcode::i16x8_avgr_u:                          // 0x9b i16x8.avgr_u
            case simd_opcode::i16x8_extmul_low_i8x16_s:              // 0x9c i16x8.extmul_low_i8x16_s
            case simd_opcode::i16x8_extmul_high_i8x16_s:             // 0x9d i16x8.extmul_high_i8x16_s
            case simd_opcode::i16x8_extmul_low_i8x16_u:              // 0x9e i16x8.extmul_low_i8x16_u
            case simd_opcode::i16x8_extmul_high_i8x16_u:             // 0x9f i16x8.extmul_high_i8x16_u
            case simd_opcode::i32x4_extend_low_i16x8_s:              // 0xa7 i32x4.extend_low_i16x8_s
            case simd_opcode::i32x4_extend_high_i16x8_s:             // 0xa8 i32x4.extend_high_i16x8_s
            case simd_opcode::i32x4_extend_low_i16x8_u:              // 0xa9 i32x4.extend_low_i16x8_u
            case simd_opcode::i32x4_extend_high_i16x8_u:             // 0xaa i32x4.extend_high_i16x8_u
            case simd_opcode::i32x4_min_s:                           // 0xb6 i32x4.min_s
            case simd_opcode::i32x4_min_u:                           // 0xb7 i32x4.min_u
            case simd_opcode::i32x4_max_s:                           // 0xb8 i32x4.max_s
            case simd_opcode::i32x4_max_u:                           // 0xb9 i32x4.max_u
            case simd_opcode::i32x4_dot_i16x8_s:                     // 0xba i32x4.dot_i16x8_s
            case simd_opcode::i32x4_extmul_low_i16x8_s:              // 0xbc i32x4.extmul_low_i16x8_s
            case simd_opcode::i32x4_extmul_high_i16x8_s:             // 0xbd i32x4.extmul_high_i16x8_s
            case simd_opcode::i32x4_extmul_low_i16x8_u:              // 0xbe i32x4.extmul_low_i16x8_u
            case simd_opcode::i32x4_extmul_high_i16x8_u:             // 0xbf i32x4.extmul_high_i16x8_u
            case simd_opcode::i64x2_abs:                             // 0xc0 i64x2.abs
            case simd_opcode::i64x2_extend_low_i32x4_s:              // 0xc7 i64x2.extend_low_i32x4_s
            case simd_opcode::i64x2_extend_high_i32x4_s:             // 0xc8 i64x2.extend_high_i32x4_s
            case simd_opcode::i64x2_extend_low_i32x4_u:              // 0xc9 i64x2.extend_low_i32x4_u
            case simd_opcode::i64x2_extend_high_i32x4_u:             // 0xca i64x2.extend_high_i32x4_u
            case simd_opcode::i64x2_mul:                             // 0xd5 i64x2.mul
            case simd_opcode::i64x2_eq:                              // 0xd6 i64x2.eq
            case simd_opcode::i64x2_ne:                              // 0xd7 i64x2.ne
            case simd_opcode::i64x2_lt_s:                            // 0xd8 i64x2.lt_s
            case simd_opcode::i64x2_gt_s:                            // 0xd9 i64x2.gt_s
            case simd_opcode::i64x2_le_s:                            // 0xda i64x2.le_s
            case simd_opcode::i64x2_ge_s:                            // 0xdb i64x2.ge_s
            case simd_opcode::i64x2_extmul_low_i32x4_s:              // 0xdc i64x2.extmul_low_i32x4_s
            case simd_opcode::i64x2_extmul_high_i32x4_s:             // 0xdd i64x2.extmul_high_i32x4_s
            case simd_opcode::i64x2_extmul_low_i32x4_u:              // 0xde i64x2.extmul_low_i32x4_u
            case simd_opcode::i64x2_extmul_high_i32x4_u:             // 0xdf i64x2.extmul_high_i32x4_u
            case simd_opcode::i32x4_trunc_sat_f64x2_s_zero:          // 0xfc i32x4.trunc_sat_f64x2_s_zero
            case simd_opcode::i32x4_trunc_sat_f64x2_u_zero:          // 0xfd i32x4.trunc_sat_f64x2_u_zero
            case simd_opcode::f64x2_convert_low_i32x4_s: // 0xfe f64x2.convert_low_i32x4_s
            case simd_opcode::f64x2_convert_low_i32x4_u: // 0xff f64x2.convert_low_i32x4_u
                return true;
            default: return false;
        }
    }

    [[nodiscard]] inline constexpr bool known_wabt_simd_gate_gap(::std::byte const* const op_begin,
                                                                 ::std::byte const* const code_end) noexcept
    {
        using basic_opcode = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
        ::std::uint32_t subopcode{};
        return read_u32_leb_after_opcode(op_begin, code_end, static_cast<::std::uint8_t>(basic_opcode::simd_prefix), subopcode) &&
               wabt_release2_missing_simd_opcode_gate(subopcode);
    }

    [[nodiscard]] inline constexpr bool known_wabt_select_zero_arity_gap(::std::byte const* const op_begin,
                                                                         ::std::byte const* const code_end) noexcept
    {
        using basic_opcode = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
        ::std::uint32_t result_count{};
        return read_u32_leb_after_opcode(op_begin, code_end, static_cast<::std::uint8_t>(basic_opcode::select_t), result_count) &&
               result_count == 0u;
    }

    [[nodiscard]] inline constexpr feature_config_t feature_config_from_mask(::std::uint8_t const mask) noexcept
    {
        return {.mask = mask,
                .sign_extension = (mask & 0x01u) != 0u,
                .nontrapping_float_to_int = (mask & 0x02u) != 0u,
                .multi_value = (mask & 0x04u) != 0u,
                .reference_types = (mask & 0x08u) != 0u,
                .table_instructions = (mask & 0x10u) != 0u,
                .multiple_tables = (mask & 0x20u) != 0u,
                .bulk_memory = (mask & 0x40u) != 0u,
                .simd = (mask & 0x80u) != 0u};
    }

    inline constexpr void configure_uwvm_features(fs_para_t& fs_para, feature_config_t const config) noexcept
    {
        auto& para{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(fs_para)};
        para.disable_sign_extension = !config.sign_extension;
        para.disable_nontrapping_float_to_int = !config.nontrapping_float_to_int;
        para.disable_multi_value = !config.multi_value;
        para.disable_reference_types = !config.reference_types;
        para.disable_table_instructions = !config.table_instructions;
        para.disable_multiple_tables = !config.multiple_tables;
        para.disable_bulk_memory = !config.bulk_memory;
        para.disable_simd = !config.simd;

        // Keep the older wasm1 COP controls synchronized so parser, initializer, validator, and backends observe one policy.
        para.controllable_allow_multi_result_vector = para.disable_multi_value;
        para.controllable_allow_multi_table = para.disable_multiple_tables;
    }

    [[nodiscard]] static validation_result_t validate_with_uwvm(::std::uint8_t const* data,
                                                                 ::std::size_t const size,
                                                                 feature_config_t const parser_config,
                                                                 feature_config_t const validator_config)
    {
        validation_result_t result{};
        auto const* begin{reinterpret_cast<::std::byte const*>(data)};
        auto const* end{begin + size};

        fs_para_t parser_parameter{};
        configure_uwvm_features(parser_parameter, parser_config);

        ::uwvm2::parser::wasm::base::error_impl parse_error{};
        module_storage_t module{};
        try
        {
            module = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature, wasm2_feature>(
                begin, end, parse_error, parser_parameter);
        }
        catch(::fast_io::error const&)
        {
            result.parse_error = parse_error.err_code;
            return result;
        }

        result.parse_error = parse_error.err_code;
        if(parse_error.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return result; }
        result.parsed = true;

        fs_para_t validator_parameter{};
        configure_uwvm_features(validator_parameter, validator_config);

        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1_feature, wasm1p1_feature, wasm2_feature>>(
            module.sections)};
        auto const import_function_count{imports.importdesc.index_unchecked(0u).size()};
        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1_feature, wasm1p1_feature, wasm2_feature>>(
            module.sections)};

        for(::std::size_t local_index{}; local_index != codes.codes.size(); ++local_index)
        {
            auto const& code{codes.codes.index_unchecked(local_index)};
            auto const* const code_begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* const code_end{reinterpret_cast<::std::byte const*>(code.body.code_end)};
            ::uwvm2::validation::error::code_validation_error_impl validation_error{};
            try
            {
                ::uwvm2::validation::concepts::dispatch_validate_code(module,
                                                                       import_function_count + local_index,
                                                                       code_begin,
                                                                       code_end,
                                                                       validation_error,
                                                                       validator_parameter);
            }
            catch(::fast_io::error const&)
            {
                result.validation_error = validation_error.err_code;
                result.function_index = import_function_count + local_index;
                if(validation_error.err_code == ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required)
                {
                    using feature_kind = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
                    result.feature_required_value = validation_error.err_selectable.wasm1p1_feature_required.value;
                    result.feature_required_kind =
                        static_cast<::std::uint32_t>(validation_error.err_selectable.wasm1p1_feature_required.feature);
                    if(!validator_config.simd &&
                       validation_error.err_selectable.wasm1p1_feature_required.feature == feature_kind::simd)
                    {
                        result.known_wabt_oracle_gap = known_wabt_simd_gate_gap(validation_error.err_curr, code_end);
                    }
                    else if(!validator_config.reference_types &&
                            validation_error.err_selectable.wasm1p1_feature_required.feature == feature_kind::reference_types)
                    {
                        // WABT decodes select_t with a zero-length result vector as the legacy untyped select.  UWVM
                        // correctly reaches the typed-select/reference-types gate before its exact-one-result arity check, so recognize
                        // the same narrowly identified oracle defect in both possible UWVM diagnostic phases.
                        result.known_wabt_oracle_gap = known_wabt_select_zero_arity_gap(validation_error.err_curr, code_end);
                    }
                }
                else if(validation_error.err_code == ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate &&
                        validation_error.err_selectable.invalid_const_immediate.op_code_name == u8"select.result_types")
                {
                    // Release 2 requires exactly one select_t result type. WABT rejects arity > 1 but currently
                    // interprets arity 0 as untyped select, so only that one encoding is an oracle gap.
                    result.known_wabt_oracle_gap = known_wabt_select_zero_arity_gap(validation_error.err_curr, code_end);
                }
                return result;
            }
            if(validation_error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok)
            {
                result.validation_error = validation_error.err_code;
                result.function_index = import_function_count + local_index;
                return result;
            }
        }

        result.validated = true;
        result.validation_error = ::uwvm2::validation::error::code_validation_error_code::ok;
        return result;
    }

    [[nodiscard]] inline constexpr bool wabt_can_exactly_model(feature_config_t const config) noexcept
    {
        // WABT exposes six relevant switches for the eight Release 2.0 groups:
        //
        //   UWVM reference-types + table-instructions + multiple-tables -> WABT reference-types
        //   every other UWVM group                                  -> its same-named WABT switch
        //
        // WABT also normalizes reference-types to disabled whenever bulk-memory is disabled (Features::UpdateDependencies).
        // Therefore these, and only these, are the complete-module policies which one WABT Features object can express
        // without enabling or disabling a different UWVM feature group. All other masks are still sent through UWVM's
        // parser and code-validation COP by the caller; only their strict differential leg is omitted.
        return config.reference_types == config.table_instructions && config.reference_types == config.multiple_tables &&
               (!config.reference_types || config.bulk_memory);
    }

    [[nodiscard]] inline constexpr ::std::size_t wabt_exactly_modelled_mask_count() noexcept
    {
        ::std::size_t count{};
        for(::std::uint_least16_t mask{}; mask != 0x100u; ++mask)
        {
            if(wabt_can_exactly_model(feature_config_from_mask(static_cast<::std::uint8_t>(mask)))) { ++count; }
        }
        return count;
    }

    // 16 combinations of the four one-to-one non-table groups, two bulk-memory states with the WABT umbrella disabled,
    // and one bulk-memory-enabled state with that umbrella enabled: 16 * (2 + 1) == 48.
    static_assert(wabt_exactly_modelled_mask_count() == 48uz);

    [[nodiscard]] inline constexpr bool accepted(validation_result_t const result) noexcept { return result.parsed && result.validated; }

    [[nodiscard]] static ::wabt::Features wabt_features(feature_config_t const config)
    {
        ::wabt::Features features;
        features.set_sign_extension_enabled(config.sign_extension);
        features.set_sat_float_to_int_enabled(config.nontrapping_float_to_int);
        features.set_multi_value_enabled(config.multi_value);
        features.set_bulk_memory_enabled(config.bulk_memory);
        features.set_reference_types_enabled(config.reference_types);
        features.set_simd_enabled(config.simd);
        features.disable_exceptions();
        features.disable_threads();
        features.disable_function_references();
        features.disable_tail_call();
        features.disable_code_metadata();
        features.disable_annotations();
        features.disable_gc();
        features.disable_memory64();
        features.disable_multi_memory();
        features.disable_extended_const();
        features.disable_relaxed_simd();
        features.disable_custom_page_sizes();
        return features;
    }

    [[nodiscard]] static validation_result_t validate_with_wabt(::std::uint8_t const* data,
                                                                 ::std::size_t const size,
                                                                 feature_config_t const config,
                                                                 ::wabt::Errors* output_errors)
    {
        validation_result_t result{};
        ::wabt::Errors errors{};
        ::wabt::Module module{};
        auto features{wabt_features(config)};
        ::wabt::ReadBinaryOptions read_options(features, nullptr, false, true, false);
        // WABT's binary reader can recover from malformed sections, append diagnostics, and still return `Ok` so
        // callers can inspect a partial IR. Differential acceptance must treat any reader diagnostic as a parse
        // rejection; otherwise a recoverable WABT error is incorrectly compared with UWVM as an accepted module.
        if(::wabt::Failed(::wabt::ReadBinaryIr("<fuzz-input>", data, size, read_options, &errors, &module)) || !errors.empty())
        {
            if(output_errors != nullptr) { *output_errors = ::std::move(errors); }
            return result;
        }

        result.parsed = true;
        ::wabt::ValidateOptions validate_options(features);
        result.validated = ::wabt::Succeeded(::wabt::ValidateModule(&module, &errors, validate_options)) && errors.empty();
        if(output_errors != nullptr) { *output_errors = ::std::move(errors); }
        return result;
    }

    [[nodiscard]] static bool debug_enabled() noexcept
    {
        auto const* value{::std::getenv("UWVM_FUZZ_DEBUG")};
        return value != nullptr && *value != '\0' && !(value[0] == '0' && value[1] == '\0');
    }
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t const size)
{
    if(data == nullptr || size == 0uz || size > (1u << 20u)) { return 0; }

    auto const mask{data[0]};
    auto const generated_module{
        ::test::wasm1_code_section_module_builder::build_module_from_code_validation_bytes(data + 1uz, size - 1uz)};
    auto const* wasm{reinterpret_cast<::std::uint8_t const*>(generated_module.data())};
    auto const wasm_size{generated_module.size()};

    auto const config{feature_config_from_mask(mask)};
    validation_result_t uwvm_result{};
    try
    {
        // Build canonical section structure around the mutated code bytes. This keeps the differential focused on
        // `standard/wasm2/validator.h`: UWVM's section parser does not validate function instructions, while WABT may
        // reject feature-gated opcodes during binary reading. Comparing combined parse+validate acceptance therefore
        // remains phase-independent without admitting unrelated raw-section parser differences.
        uwvm_result = validate_with_uwvm(wasm, wasm_size, config, config);
    }
    catch(::fast_io::error const&)
    {
        // Parser/validator diagnostics normally terminate inside validate_with_uwvm. Keep the fuzz boundary defensive in
        // case a future COP handler throws after moving or returning its intermediate module storage.
        return 0;
    }

    if(!wabt_can_exactly_model(config)) { return 0; }
    auto const wabt_result{validate_with_wabt(wasm, wasm_size, config, nullptr)};

    // Compare final module acceptance, not the phase at which rejection occurs. WABT deliberately performs some feature
    // and semantic checks in ValidateModule that UWVM performs while parsing sections. A rejection by either UWVM phase
    // must therefore be compared with the combined WABT parse+validate result instead of being unconditionally skipped.
    auto const uwvm_accepted{accepted(uwvm_result)};
    auto const wabt_accepted{accepted(wabt_result)};
    if(uwvm_accepted != wabt_accepted)
    {
        // The canonical builder emits no element/data segments. The only accepted UWVM-reject/WABT-accept cases are
        // therefore the explicitly enumerated missing WABT SIMD Opcode::IsEnabled gates and select_t arity zero.
        // Every other feature-policy or semantic disagreement remains strict in both directions.
        if(!uwvm_accepted && wabt_accepted && uwvm_result.known_wabt_oracle_gap) { return 0; }

        if(debug_enabled())
        {
            ::wabt::Errors wabt_errors{};
            (void)validate_with_wabt(wasm, wasm_size, config, &wabt_errors);
            ::std::fprintf(stderr,
                           "wasm2 differential mismatch: uwvm_accepted=%d wabt_accepted=%d "
                           "uwvm_parsed=%d uwvm_validated=%d wabt_parsed=%d wabt_validated=%d size=%zu mask=0x%02x "
                           "sign=%d sat=%d multivalue=%d refs=%d table=%d multitables=%d bulk=%d simd=%d "
                           "parse_error=%u validation_error=%u function=%zu feature_kind=%u feature_value=0x%x wabt_errors=%zu\n",
                           static_cast<int>(uwvm_accepted),
                           static_cast<int>(wabt_accepted),
                           static_cast<int>(uwvm_result.parsed),
                           static_cast<int>(uwvm_result.validated),
                           static_cast<int>(wabt_result.parsed),
                           static_cast<int>(wabt_result.validated),
                           wasm_size,
                           static_cast<unsigned>(config.mask),
                           static_cast<int>(config.sign_extension),
                           static_cast<int>(config.nontrapping_float_to_int),
                           static_cast<int>(config.multi_value),
                           static_cast<int>(config.reference_types),
                           static_cast<int>(config.table_instructions),
                           static_cast<int>(config.multiple_tables),
                           static_cast<int>(config.bulk_memory),
                           static_cast<int>(config.simd),
                           static_cast<unsigned>(uwvm_result.parse_error),
                           static_cast<unsigned>(uwvm_result.validation_error),
                           uwvm_result.function_index,
                           static_cast<unsigned>(uwvm_result.feature_required_kind),
                           static_cast<unsigned>(uwvm_result.feature_required_value),
                           wabt_errors.size());
            if(!wabt_errors.empty()) { ::std::fprintf(stderr, "wabt_error0: %s\n", wabt_errors.front().message.c_str()); }
            ::std::fflush(stderr);
        }
        __builtin_trap();
    }

    return 0;
}
