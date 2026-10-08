/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/validation/error/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3.0 valid/instructions.html#valid-return-call: the callee results
    // must match the enclosing FUNCTION's results, even in unreachable code.
    // Block labels and the caller's parameter list do not constrain this match.
    // Current value storage represents numeric/vector and nullable abstract
    // references; recursive reference subtyping will extend this comparison.
    inline constexpr void require_tail_call_enabled(bool enabled, unsigned opcode,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled) { return; }
        // [return_call opcode] immediate ... end
        // [safe              ] unsafe (could be end)
        // ^^ op_begin / err_curr; dispatch already checked this byte.
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = opcode,
            .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::tail_call,
            .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    template<typename Expected, typename Actual>
    inline constexpr void validate_tail_call_results(Expected const& expected, Actual const& actual,
        ::std::byte const* op_begin, ::uwvm2::utils::container::u8string_view op_name,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        using error_code = ::uwvm2::validation::error::code_validation_error_code;
        using diagnostic_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        // Both ranges borrow parser-validated type storage. Empty ranges can be
        // null: do not subtract null pointers, even when they compare equal.
        auto const expected_count{expected.begin == expected.end ? 0uz : static_cast<::std::size_t>(expected.end - expected.begin)};
        auto const actual_count{actual.begin == actual.end ? 0uz : static_cast<::std::size_t>(actual.end - actual.begin)};
        if(expected_count != actual_count)
        {
            // [return_call opcode] immediate ... end
            // [safe              ] unsafe; diagnostic pointer only, no read.
            // ^^ op_begin / err_curr
            err.err_curr = op_begin;
            err.err_code = error_code::end_result_mismatch;
            err.err_selectable.end_result_mismatch = {.block_kind = op_name,
                .expected_count = expected_count, .actual_count = actual_count,
                .expected_type = expected_count == 0uz ? diagnostic_type{} : static_cast<diagnostic_type>(expected.begin[0]),
                .actual_type = actual_count == 0uz ? diagnostic_type{} : static_cast<diagnostic_type>(actual.begin[0])};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        for(::std::size_t i{}; i != expected_count; ++i)
        {
            if(expected.begin[i] == actual.begin[i]) { continue; }
            // Both arrays contain expected_count elements; i is in bounds.
            // [return_call opcode] immediate ... end
            // [safe              ] unsafe; diagnostic pointer only, no read.
            // ^^ op_begin / err_curr
            err.err_curr = op_begin;
            err.err_code = error_code::br_value_type_mismatch;
            err.err_selectable.br_value_type_mismatch = {.op_code_name = op_name,
                .expected_type = static_cast<diagnostic_type>(expected.begin[i]),
                .actual_type = static_cast<diagnostic_type>(actual.begin[i])};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
