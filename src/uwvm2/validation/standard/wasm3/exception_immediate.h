/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <utility>
# include "recursive_type_binary.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3, not the obsolete try/catch proposal:
    // https://webassembly.github.io/spec/core/binary/instructions.html#exception-handling
    enum class exception_catch_kind : unsigned { tagged, tagged_ref, all, all_ref };
    struct exception_catch_clause
    {
        exception_catch_kind kind{};
        ::std::uint_least32_t tag_index{}, label_index{};
        ::std::size_t binary_offset{};
    };
    enum class exception_immediate_error : unsigned { ok, binary, invalid_catch_kind };
    struct exception_catch_vector
    {
        ::uwvm2::utils::container::vector<exception_catch_clause> clauses{};
        exception_immediate_error error{};
        recursive_type_binary_result binary{};
    };
    // Starts immediately AFTER the checked try_table blocktype. Catch labels are resolved against
    // the OUTER context; the caller must not push the try_table label before validating these clauses.
    [[nodiscard]] inline constexpr exception_catch_vector scan_exception_catches(
        ::std::byte const*& code_curr, ::std::byte const* code_end) noexcept
    {
        // [try_table blocktype] count catches ... code_end
        // [safe               ] unsafe (could be code_end)
        //                       ^^ code_curr; caller proves the common allocation, equal endpoints may be null.
        recursive_binary_details::reader input{{code_curr, code_curr == code_end ? 0uz : static_cast<::std::size_t>(code_end - code_curr)}};
        exception_catch_vector result{};
        // The shortest clause is [kind][labelidx], two bytes. list() bounds the declared count by
        // actual remaining input and PTRDIFF_MAX / sizeof(clause) BEFORE reserving owned storage.
        if(!input.list(result.clauses, 2uz, [&](exception_catch_clause& clause) constexpr noexcept
        {
            clause.binary_offset = input.position;
            unsigned kind{};
            if(!input.byte(kind)) { return false; }
            // Kind is one literal byte, never a padded unsigned LEB.
            if(kind > 3u) { result.error = exception_immediate_error::invalid_catch_kind; return false; }
            clause.kind = static_cast<exception_catch_kind>(kind);
            return (kind >= 2u || input.u32(clause.tag_index)) && input.u32(clause.label_index);
        }))
        {
            if(result.error == exception_immediate_error::ok) { result.error = exception_immediate_error::binary; }
            result.binary = input.status;
            if(result.error == exception_immediate_error::invalid_catch_kind) { result.binary.error_offset = input.position - 1uz; }
            result.clauses.clear(); // Failed input never exposes a partially validated handler table.
            return result;
        }
        // [try_table blocktype][complete bounded catch vector] next ... code_end
        // [safe                                             ] unsafe (could be code_end)
        //                                                     ^^ code_curr after the sole successful commit.
        // The vector count consumed at least one byte; position <= original span size proves this addition.
        code_curr += input.position;
        // [try_table blocktype][complete bounded catch vector] next ... code_end
        // [safe                                             ] unsafe (could be code_end)
        //                                                     ^^ code_curr: one-past is legal; no read follows.
        return result;
    }
}
