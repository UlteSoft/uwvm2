/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/validation/error/impl.h>
# include "atomic_immediate.h"
# include "address_limits.h"
# include "memory_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Threads binary/instructions: FE, u32(3), literal 00. The subopcode may
    // use a padded unsigned LEB; the reserved byte MUST NOT be read as a LEB.
    // https://webassembly.github.io/threads/core/binary/instructions.html
    // This instruction has type [] -> [] and does not require a memory.
    [[nodiscard]] inline constexpr bool scan_atomic_fence_immediate(::std::byte const*& code_curr,
                                                                    ::std::byte const* code_end) noexcept
    {
        // [FE] subopcode reserved ... (code_end)
        // [safe] unsafe (could be code_end)
        //        ^^ cursor borrows code_curr; only a complete immediate is committed.
        auto cursor{code_curr};
        auto const instruction{scan_atomic_instruction(cursor, code_end, false)};
        // [FE subopcode immediate] ... (code_end)
        // [safe                 ] unsafe (could be code_end)
        //                         ^^ cursor only on a complete structural decode.
        if(instruction.error != atomic_immediate_error::ok || instruction.descriptor.kind != atomic_instruction_kind::fence) { return false; }
        // atomic fence immediate ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
        code_curr = cursor;
        // [FE subopcode 00] ... (code_end)
        // [safe           ] unsafe (could be code_end)
        //                   ^^ code_curr: commit; no read of the following instruction.
        return true;
    }

    // The complete threads instruction decoder is shared with both execution backends.
    // Validation never accepts an opcode that either execution backend drops.
    [[nodiscard]] inline constexpr atomic_instruction_immediate read_supported_atomic_instruction(
        ::std::byte const*& code_curr, ::std::byte const* code_end, ::std::byte const* op_begin,
        bool enabled, bool multi_memory, ::std::size_t memory_count,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        using code = ::uwvm2::validation::error::code_validation_error_code;
        if(!enabled)
        {
            err.err_code = code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::threads;
            err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction;
            err.err_selectable.wasm1p1_feature_required.value = 0xfeu;
        }
        else
        {
            // [FE] subopcode immediate ... (code_end)
            // [safe] unsafe (could be code_end); only a complete instruction commits.
            auto cursor{code_curr};
            auto const instruction{scan_atomic_instruction(cursor, code_end, multi_memory)};
            // [FE subopcode immediate] ... (code_end)
            // [safe                 ] unsafe (could be code_end)
            //                         ^^ cursor on success; no memory accessed.
            if(instruction.error == atomic_immediate_error::memory_argument)
            { require_memory_immediate(instruction.memory.error, op_begin, err); }
            if(instruction.error == atomic_immediate_error::alignment)
            {
                // Atomic alignment is an equality requirement. The existing <=
                // diagnostic would misdescribe an under-aligned atomic memarg.
                err.err_code = code::invalid_memarg_align;
            }
            else if(instruction.error == atomic_immediate_error::ok)
            {
                if(instruction.descriptor.kind != atomic_instruction_kind::fence)
                { validate_memory_index(instruction.memory.memory_index, memory_count, op_begin, err); }
                // [FE subopcode immediate] next ... code_end
                // [safe                 ] unsafe (could be code_end)
                //                         ^^ code_curr: cursor came from a bounded, successful atomic scan.
                code_curr = cursor;
                // [FE subopcode immediate] ... (code_end)
                // [safe                 ] unsafe (could be code_end)
                //                         ^^ code_curr: complete, enabled instruction committed.
                return instruction;
            }
            else
            {
                err.err_code = code::invalid_const_immediate;
                err.err_selectable.invalid_const_immediate.op_code_name = u8"atomic instruction";
            }
        }
        // [FE] immediate ... (code_end)
        // [safe] unsafe; the diagnostic borrows the dispatch-checked opcode.
        err.err_curr = op_begin;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    struct typed_atomic_instruction
    {
        atomic_instruction64_immediate immediate{};
        // The fence has no address operand; its default field is never consumed.
        storage_address_type address_type{};
    };

    // This address-aware decoder is a frontend building block. Accepting a wide
    // binary immediate alone does not enable memory64 execution in a backend.
    template<typename AddressTypeAt>
    [[nodiscard]] inline constexpr typed_atomic_instruction read_atomic_instruction64(
        ::std::byte const*& code_curr, ::std::byte const* code_end, ::std::byte const* op_begin,
        bool enabled, bool multi_memory, bool wide_encoding, ::std::size_t memory_count,
        AddressTypeAt&& address_type_at, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        using code = ::uwvm2::validation::error::code_validation_error_code;
        if(!enabled)
        {
            err.err_code = code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::threads;
            err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction;
            err.err_selectable.wasm1p1_feature_required.value = 0xfeu;
        }
        else
        {
            // [FE] subopcode immediate ... (code_end)
            // [safe] unsafe (could be code_end)
            //        ^^ cursor; caller's cursor is unchanged until all validation succeeds.
            auto cursor{code_curr};
            auto const instruction{scan_atomic_instruction64(cursor, code_end, multi_memory, wide_encoding)};
            // [FE subopcode immediate] ... (code_end)
            // [safe                 ] unsafe (could be code_end)
            //                         ^^ cursor only on a complete bounded decode.
            if(instruction.error == atomic_immediate_error::memory_argument)
            { require_memory_immediate(instruction.memory.error, op_begin, err); }
            if(instruction.error == atomic_immediate_error::alignment) { err.err_code = code::invalid_memarg_align; }
            else if(instruction.error == atomic_immediate_error::ok)
            {
                storage_address_type address_type{};
                if(instruction.descriptor.kind != atomic_instruction_kind::fence)
                {
                    validate_memory_index(instruction.memory.memory_index, memory_count, op_begin, err);
                    // The complete import/local count was checked before invoking
                    // the resolver. An unknown index never reaches a memory record.
                    address_type = address_type_at(instruction.memory.memory_index);
                    if(address_type == storage_address_type::i32 && instruction.memory.offset > 0xffff'ffffull)
                    { require_memory_immediate(memory_immediate_error::offset, op_begin, err); }
                }
                // [FE subopcode immediate] next ... code_end
                // [safe                 ] unsafe (could be code_end)
                //                         ^^ code_curr: all atomic immediate and memory-index checks succeeded.
                code_curr = cursor;
                // [FE subopcode immediate] ... (code_end)
                // [safe                 ] unsafe (could be code_end)
                //                         ^^ code_curr after selected width/index/alignment proof.
                return {instruction, address_type};
            }
            else
            {
                err.err_code = code::invalid_const_immediate;
                err.err_selectable.invalid_const_immediate.op_code_name = u8"atomic instruction";
            }
        }
        // [FE] immediate ... (code_end)
        // [safe] unsafe; diagnostic borrows the dispatch-checked opcode.
        err.err_curr = op_begin;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    inline constexpr void read_atomic_fence_immediate(::std::byte const*& code_curr, ::std::byte const* code_end,
        ::std::byte const* op_begin, bool enabled, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        using code = ::uwvm2::validation::error::code_validation_error_code;
        if(enabled)
        {
            // [FE] subopcode reserved ... (code_end)
            // [safe] unsafe (could be code_end); the scanner commits only on success.
            if(scan_atomic_fence_immediate(code_curr, code_end)) { return; }
            // [FE] invalid/truncated immediate ... (code_end)
            // [safe] unsafe; code_curr is unchanged on failure.
            err.err_code = code::invalid_const_immediate;
            err.err_selectable.invalid_const_immediate.op_code_name = u8"atomic.fence";
        }
        else
        {
            err.err_code = code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::threads;
            err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction;
            err.err_selectable.wasm1p1_feature_required.value = 0xfeu;
        }
        // [FE] immediate ... (code_end)
        // [safe] unsafe
        // ^^ err_curr borrows the opcode already checked by dispatch.
        err.err_curr = op_begin;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
