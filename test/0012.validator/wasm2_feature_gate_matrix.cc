/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly 2.0 independent feature-gate matrix
 * @details     Every Release 2.0 feature is disabled alone against a minimal instruction/module that requires it.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-07-11
 */

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
# include <uwvm2/validation/concepts/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;
    using feature_kind = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;

    enum class feature_bit : ::std::uint_least16_t
    {
        sign_extension = 1u << 0u,
        nontrapping_float_to_int = 1u << 1u,
        multi_value = 1u << 2u,
        reference_types = 1u << 3u,
        table_instructions = 1u << 4u,
        multiple_tables = 1u << 5u,
        bulk_memory = 1u << 6u,
        simd = 1u << 7u
    };

    [[nodiscard]] inline constexpr ::std::uint_least16_t bit(feature_bit const value) noexcept
    { return static_cast<::std::uint_least16_t>(value); }

    // (func i32.const 0 i32.extend8_s drop)
    inline constexpr ::std::uint8_t sign_extension_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00, 0x0a, 0x08, 0x01, 0x06, 0x00, 0x41, 0x00, 0xc0, 0x1a, 0x0b};

    // (func f32.const 0 i32.trunc_sat_f32_s drop)
    inline constexpr ::std::uint8_t nontrapping_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x0a, 0x0c, 0x01, 0x0a, 0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x1a, 0x0b};

    // A block uses type index 0 with two results.
    inline constexpr ::std::uint8_t multi_value_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x00, 0x02, 0x7f, 0x7e,
        0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x01, 0x0a, 0x0d, 0x01, 0x0b, 0x00, 0x02, 0x00, 0x41,
        0x01, 0x42, 0x02, 0x0b, 0x1a, 0x1a, 0x0b};

    // (func ref.null func drop)
    inline constexpr ::std::uint8_t reference_types_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00, 0x0a, 0x07, 0x01, 0x05, 0x00, 0xd0, 0x70, 0x1a, 0x0b};

    // (table 1 funcref) (func i32.const 0 table.get 0 drop)
    inline constexpr ::std::uint8_t table_instruction_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x00, 0x25,
        0x00, 0x1a, 0x0b};

    // Two tables and table.get 1 require both table-instructions and multiple-tables.
    inline constexpr ::std::uint8_t multiple_tables_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x07, 0x02, 0x70, 0x00, 0x01, 0x70, 0x00, 0x01, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x41,
        0x00, 0x25, 0x01, 0x1a, 0x0b};

    // (memory 1) (func i32.const 0 i32.const 0 i32.const 0 memory.fill)
    inline constexpr ::std::uint8_t bulk_memory_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x0a, 0x0d, 0x01, 0x0b, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41,
        0x00, 0xfc, 0x0b, 0x00, 0x0b};

    // (func v128.const i32x4 0 0 0 0 drop)
    inline constexpr ::std::uint8_t simd_module[]{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x00, 0x0a, 0x17, 0x01, 0x15, 0x00, 0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x0b};

    struct module_case
    {
        ::std::uint8_t const* bytes{};
        ::std::size_t size{};
        ::std::uint_least16_t required_features{};
    };

    struct parse_attempt
    {
        module_storage_t module{};
        ::uwvm2::parser::wasm::base::error_impl error{};
        bool accepted{};
    };

    inline constexpr module_case cases[]{
        {sign_extension_module, sizeof(sign_extension_module), bit(feature_bit::sign_extension)},
        {nontrapping_module, sizeof(nontrapping_module), bit(feature_bit::nontrapping_float_to_int)},
        {multi_value_module, sizeof(multi_value_module), bit(feature_bit::multi_value)},
        {reference_types_module, sizeof(reference_types_module), bit(feature_bit::reference_types)},
        {table_instruction_module, sizeof(table_instruction_module), bit(feature_bit::table_instructions)},
        {multiple_tables_module, sizeof(multiple_tables_module), bit(feature_bit::table_instructions) | bit(feature_bit::multiple_tables)},
        {bulk_memory_module, sizeof(bulk_memory_module), bit(feature_bit::bulk_memory)},
        {simd_module, sizeof(simd_module), bit(feature_bit::simd)},
    };

    [[noreturn]] inline void fail(char const* message)
    {
        ::fast_io::io::perrln("wasm2_feature_gate_matrix: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }

    inline void expect(bool const condition, char const* message)
    {
        if(!condition) [[unlikely]] { fail(message); }
    }

    [[nodiscard]] parse_attempt parse(module_case const& test_case, fs_para_t const& fs_para)
    {
        parse_attempt result{};
        auto const* begin{reinterpret_cast<::std::byte const*>(test_case.bytes)};
        try
        {
            result.module = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, begin + test_case.size, result.error, fs_para);
        }
        catch(::fast_io::error const&)
        {
            return result;
        }
        result.accepted = result.error.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok;
        return result;
    }

    inline void disable_feature(fs_para_t& fs_para, feature_bit const feature) noexcept
    {
        auto& para{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(fs_para)};
        switch(feature)
        {
            case feature_bit::sign_extension: para.disable_sign_extension = true; break;
            case feature_bit::nontrapping_float_to_int: para.disable_nontrapping_float_to_int = true; break;
            case feature_bit::multi_value: para.disable_multi_value = true; break;
            case feature_bit::reference_types: para.disable_reference_types = true; break;
            case feature_bit::table_instructions: para.disable_table_instructions = true; break;
            case feature_bit::multiple_tables: para.disable_multiple_tables = true; break;
            case feature_bit::bulk_memory: para.disable_bulk_memory = true; break;
            case feature_bit::simd: para.disable_simd = true; break;
        }
    }

    [[nodiscard]] error_code validate(module_storage_t const& module, fs_para_t const& fs_para)
    {
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        expect(codes.codes.size() == 1uz, "matrix module must contain one function");
        auto const& code{codes.codes.index_unchecked(0uz)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::concepts::dispatch_validate_code(module,
                                                                   imports.importdesc.index_unchecked(0u).size(),
                                                                   reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                                                                   reinterpret_cast<::std::byte const*>(code.body.code_end),
                                                                   err,
                                                                   fs_para);
        }
        catch(::fast_io::error const&)
        {
            return err.err_code;
        }
        return err.err_code;
    }

    [[nodiscard]] bool parser_feature_error_matches(feature_bit const feature,
                                                     ::uwvm2::parser::wasm::base::error_impl const& err) noexcept
    {
        using parse_error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        using wasm1p1_feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using wasm2_feature = ::uwvm2::parser::wasm::base::wasm2_feature_kind;
        switch(feature)
        {
            case feature_bit::sign_extension:
                return err.err_code == parse_error::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::sign_extension;
            case feature_bit::nontrapping_float_to_int:
                return err.err_code == parse_error::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::nontrapping_float_to_int;
            case feature_bit::multi_value:
                // The shared wasm1 type-section decoder owns the structural single-result-vector check and therefore
                // keeps its established parser error. Instruction-level multi-value checks use the versioned validator ECO.
                return err.err_code == parse_error::wasm1_not_allow_multi_value ||
                       (err.err_code == parse_error::wasm1p1_feature_required &&
                        err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::multi_value);
            case feature_bit::reference_types:
                return err.err_code == parse_error::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::reference_types;
            case feature_bit::table_instructions:
                return err.err_code == parse_error::wasm2_feature_required &&
                       err.err_selectable.wasm2_feature_required.feature == wasm2_feature::table_instructions;
            case feature_bit::multiple_tables:
                // As with function result vectors, the shared wasm1 section decoder owns the structural table-count
                // limit. Uses of a non-zero table index that reach Wasm 2.0 validation use wasm2_feature_required.
                return err.err_code == parse_error::wasm1_not_allow_multi_table ||
                       (err.err_code == parse_error::wasm2_feature_required &&
                        err.err_selectable.wasm2_feature_required.feature == wasm2_feature::multiple_tables);
            case feature_bit::bulk_memory:
                return err.err_code == parse_error::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::bulk_memory;
            case feature_bit::simd:
                return err.err_code == parse_error::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature::simd;
        }
        return false;
    }
}

int main()
{
    constexpr feature_bit all_features[]{
        feature_bit::sign_extension,
        feature_bit::nontrapping_float_to_int,
        feature_bit::multi_value,
        feature_bit::reference_types,
        feature_bit::table_instructions,
        feature_bit::multiple_tables,
        feature_bit::bulk_memory,
        feature_bit::simd,
    };

    for(auto const& test_case: cases)
    {
        fs_para_t defaults{};
        auto default_parse{parse(test_case, defaults)};
        expect(default_parse.accepted, "default parser policy rejected a matrix module");
        expect(validate(default_parse.module, defaults) == error_code::ok, "default validator policy rejected a matrix module");

        for(auto const feature: all_features)
        {
            fs_para_t policy{};
            disable_feature(policy, feature);
            auto const required{(test_case.required_features & bit(feature)) != 0u};

            // Exercise the same parser+validator pipeline used by the CLI. Some feature-dependent section syntax is
            // rejected during decoding; opcode-only features proceed to code validation. Either phase must reject only
            // the module families owned by the disabled feature.
            auto policy_parse{parse(test_case, policy)};
            if(!policy_parse.accepted)
            {
                expect(required, "a single-feature disable made the parser reject an unrelated module");
                if(!parser_feature_error_matches(feature, policy_parse.error))
                {
                    ::fast_io::io::perrln("wasm2_feature_gate_matrix: feature=",
                                          static_cast<unsigned>(feature),
                                          " parse_error=",
                                          static_cast<unsigned>(policy_parse.error.err_code));
                    fail("parser rejection used the wrong feature diagnostic");
                }
                continue;
            }

            auto const result{validate(policy_parse.module, policy)};
            expect((result != error_code::ok) == required, "a single-feature disable affected the wrong instruction family");

            if(required)
            {
                auto const wasm2_owned{feature == feature_bit::table_instructions || feature == feature_bit::multiple_tables};
                expect(result == (wasm2_owned ? error_code::wasm2_feature_required : error_code::wasm1p1_feature_required),
                       "feature rejection used the wrong versioned ECO");
            }
        }
    }

    // Legacy COP controls and the canonical disable fields must select the same validator policy.
    {
        fs_para_t policy{};
        ::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(policy).controllable_allow_multi_result_vector = true;
        auto parsed{parse(cases[2], policy)};
        expect((!parsed.accepted && parser_feature_error_matches(feature_bit::multi_value, parsed.error)) ||
                   (parsed.accepted && validate(parsed.module, policy) == error_code::wasm1p1_feature_required),
               "legacy single-result COP control diverged from the wasm2 parser/validator policy");
    }
    {
        fs_para_t policy{};
        ::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(policy).controllable_allow_multi_table = true;
        auto parsed{parse(cases[5], policy)};
        expect((!parsed.accepted && parser_feature_error_matches(feature_bit::multiple_tables, parsed.error)) ||
                   (parsed.accepted && validate(parsed.module, policy) == error_code::wasm2_feature_required),
               "legacy single-table COP control diverged from the wasm2 parser/validator policy");
    }
}
