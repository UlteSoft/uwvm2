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
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/validation/error/impl.h>
# include "memory_immediate.h"
# include "address_limits.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    template<typename AddressTypeAt>
    inline constexpr void require_memory64_policy(bool enabled, ::std::size_t count, AddressTypeAt&& address_type_at,
        ::std::byte const* code_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled) { return; }
        for(::std::size_t index{}; index != count; ++index)
        {
            // Parsed import/local counts fit u32. The loop proves the resolver's
            // metadata index is in bounds before borrowing any declaration.
            if(address_type_at(static_cast<::std::uint_least32_t>(index)) != storage_address_type::i64) { continue; }
            // code ... (code_end)
            // unsafe (could be code_end)
            // ^^ err_curr borrows code_begin for a declaration-policy diagnostic;
            //    no opcode is read and an empty function body remains safe here.
            err.err_curr = code_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {
                .value = 4u, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::memory64,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::memory_type};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    inline constexpr void require_memory_immediate(memory_immediate_error status, ::std::byte const* op_begin,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        using code = ::uwvm2::validation::error::code_validation_error_code;
        if(status == memory_immediate_error::ok) { return; }
        // [opcode] immediates ... (code_end)
        // [safe  ] unsafe
        // ^^ op_begin is the dispatch-checked opcode; the diagnostic borrows this address.
        err.err_curr = op_begin;
        switch(status)
        {
            case memory_immediate_error::feature_disabled:
                err.err_code = code::wasm1p1_feature_required;
                err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_memory;
                err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction;
                err.err_selectable.wasm1p1_feature_required.value = 0u;
                break;
            case memory_immediate_error::alignment: err.err_code = code::invalid_memarg_align; break;
            case memory_immediate_error::index: err.err_code = code::invalid_memory_index; break;
            default: err.err_code = code::invalid_memarg_offset; break;
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    inline constexpr void validate_memory_index(::std::uint_least32_t index, ::std::size_t count, ::std::byte const* op_begin,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(index < count) { return; }
        // [opcode] immediates ... (code_end)
        // [safe  ] unsafe
        // ^^ op_begin; index is checked before selecting any memory record.
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
        err.err_selectable.illegal_memory_index.memory_index = index;
        err.err_selectable.illegal_memory_index.all_memory_count = count;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    [[nodiscard]] inline constexpr ::std::uint_least32_t read_memory_index(::std::byte const*& cursor, ::std::byte const* end,
        ::std::byte const* op_begin, bool enabled, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::std::uint_least32_t index{};
        // [consumed opcode] memidx ... (end)
        // [safe           ] unsafe (could be end); scanner bounds-checks each consumed byte.
        require_memory_immediate(scan_memory_index(cursor, end, enabled, index), op_begin, err);
        // [consumed opcode memidx] ... (end)
        // [safe                  ] unsafe (could be end)
        //                          ^^ cursor, or exception without an out-of-range advance.
        return index;
    }

    struct typed_memory_argument
    {
        memory64_argument immediate{};
        storage_address_type address_type{};
    };

    // The resolver is invoked only after validating the index against the complete
    // import/local memory count. It supplies the selected memory's declared address
    // type; decoding a wide offset never silently promotes a memory32 operand.
    template<typename AddressTypeAt>
    [[nodiscard]] inline constexpr typed_memory_argument read_memory_argument64(::std::byte const*& cursor, ::std::byte const* end,
        ::std::byte const* op_begin, bool enabled, bool wide_encoding, ::std::size_t count, AddressTypeAt&& address_type_at,
        ::std::uint_least32_t max_alignment,
        ::uwvm2::utils::container::u8string_view name, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // [consumed opcode] memarg ... (end)
        // [safe           ] unsafe (could be end); scanner leaves cursor unchanged on failure.
        auto next{cursor};
        auto const result{scan_memory_argument64(next, end, enabled, wide_encoding)};
        // [consumed opcode memarg] ... (end)
        // [safe                  ] unsafe (could be end)
        //                          ^^ next on successful decoding; caller cursor remains unchanged.
        require_memory_immediate(result.error, op_begin, err);
        validate_memory_index(result.memory_index, count, op_begin, err);
        auto const address_type{address_type_at(result.memory_index)};
        if(address_type == storage_address_type::i32 && result.offset > 0xffff'ffffull)
        { require_memory_immediate(memory_immediate_error::offset, op_begin, err); }
        if(result.alignment > max_alignment)
        {
            // [opcode] memarg ... (end)
            // [safe  ] unsafe; diagnostic borrows the checked opcode address.
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memarg_alignment;
            err.err_selectable.illegal_memarg_alignment.op_code_name = name;
            err.err_selectable.illegal_memarg_alignment.align = result.alignment;
            err.err_selectable.illegal_memarg_alignment.max_align = max_alignment;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // [consumed opcode memarg] next ... end
        // [safe                  ] unsafe (could be end)
        //                          ^^ cursor: bounded scanner plus index, width and alignment proofs precede commit.
        cursor = next;
        // [consumed opcode memarg] ... (end)
        // [safe                  ] unsafe (could be end)
        //                          ^^ cursor: commit after index, selected address
        // width and alignment validation; no host pointer is derived from offset.
        return {result, address_type};
    }

    // Existing memory32 translators retain their u32 immediate/bytecode ABI.
    // They share the selected-address checks and narrow only after that proof.
    [[nodiscard]] inline constexpr memory_argument read_memory_argument(::std::byte const*& cursor, ::std::byte const* end,
        ::std::byte const* op_begin, bool enabled, ::std::size_t count, ::std::uint_least32_t max_alignment,
        ::uwvm2::utils::container::u8string_view name, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        // [consumed opcode] memarg ... (end)
        // [safe           ] unsafe (could be end)
        //                   ^^ cursor: shared validator commits only a complete
        // memory32 memarg with an existing memory and legal natural alignment.
        // The memory32 result is narrowed after the selected-address range proof;
        // Core 3 still decodes its on-wire offset as u64.
        auto const validated{read_memory_argument64(cursor, end, op_begin, enabled, true, count,
            [](::std::uint_least32_t) constexpr noexcept { return storage_address_type::i32; }, max_alignment, name, err)};
        // [consumed opcode memarg] ... (end)
        // [safe                  ] unsafe (could be end)
        //                          ^^ cursor, or unchanged if validation threw.
        auto const& result{validated.immediate};
        return {result.alignment, result.memory_index, static_cast<::std::uint_least32_t>(result.offset), result.error};
    }

}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
