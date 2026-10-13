/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <type_traits>
# include "memory_immediate.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // https://webassembly.github.io/threads/core/binary/instructions.html
    // Threads' complete FE opcode space. Memory64 changes address/offset types
    // separately; this descriptor does not silently enable a different memory model.
    enum class atomic_instruction_kind : unsigned
    {
        invalid, notify, wait32, wait64, fence, load, store,
        add, sub, and_, or_, xor_, exchange, compare_exchange
    };
    struct atomic_instruction_descriptor
    {
        atomic_instruction_kind kind{};
        unsigned natural_alignment{}; // log2(bytes), required EXACTLY in an atomic memarg
        bool value_i64{};
        unsigned operand_count{}; // includes address, and wait timeout where applicable
        bool has_result{};
        bool result_i64{};
    };

    [[nodiscard]] inline constexpr atomic_instruction_descriptor describe_atomic_instruction(::std::uint_least32_t opcode) noexcept
    {
        using kind = atomic_instruction_kind;
        switch(opcode)
        {
            case 0u: return {kind::notify, 2u, false, 2u, true, false};
            case 1u: return {kind::wait32, 2u, false, 3u, true, false};
            case 2u: return {kind::wait64, 3u, true, 3u, true, false};
            case 3u: return {kind::fence, 0u, false, 0u, false, false};
            default: break;
        }
        if(opcode < 0x10u || opcode > 0x4eu) { return {}; }
        // Seven width variants per family: i32, i64, i32.8, i32.16,
        // i64.8, i64.16, i64.32. RMW families share this exact numbering.
        constexpr unsigned alignment[]{2u, 3u, 0u, 1u, 0u, 1u, 2u};
        constexpr kind families[]{kind::load, kind::store, kind::add, kind::sub,
            kind::and_, kind::or_, kind::xor_, kind::exchange, kind::compare_exchange};
        // The checked [0x10, 0x4e] range proves family <= 8 and variant <= 6
        // before either fixed array is indexed.
        auto const family{(opcode - 0x10u) / 7u};
        auto const variant{(opcode - 0x10u) % 7u};
        bool const wide{variant == 1u || variant >= 4u};
        bool const returns{family != 1u};
        return {families[family], alignment[variant], wide,
                family == 0u ? 1u : family == 8u ? 3u : 2u, returns, returns && wide};
    }

    enum class atomic_immediate_error : unsigned { ok, opcode, reserved, memory_argument, alignment };
    struct atomic_instruction_immediate
    {
        ::std::uint_least32_t opcode{};
        atomic_instruction_descriptor descriptor{};
        memory_argument memory{};
        atomic_immediate_error error{};
    };

    struct atomic_instruction64_immediate
    {
        ::std::uint_least32_t opcode{};
        atomic_instruction_descriptor descriptor{};
        memory64_argument memory{};
        atomic_immediate_error error{};
    };

    // Binary decoding only: validation must also check the threads feature gate,
    // memory index, address type and typed stack. Runtime wait additionally checks
    // sharedness; all atomic accesses check effective-address alignment and bounds.
    // Neither a legal memarg alignment nor a shared declaration proves runtime alignment.
    template<bool Address64Encoding>
    [[nodiscard]] inline constexpr auto scan_atomic_instruction_impl(
        ::std::byte const*& code_curr, ::std::byte const* code_end, bool multi_memory, bool wide_encoding) noexcept
    {
        ::std::conditional_t<Address64Encoding, atomic_instruction64_immediate, atomic_instruction_immediate> result{};
        // [FE] subopcode immediate ... (code_end)
        // [safe] unsafe (could be code_end)
        //        ^^ cursor borrows code_curr; all failures leave code_curr unchanged.
        auto cursor{code_curr};
        if(!scan_memory_unsigned(cursor, code_end, result.opcode))
        { result.error = atomic_immediate_error::opcode; return result; }
        // [FE subopcode] immediate ... (code_end)
        // [safe        ] unsafe (could be code_end)
        //                ^^ cursor: bounded u32 LEB proved by the scanner.
        result.descriptor = describe_atomic_instruction(result.opcode);
        if(result.descriptor.kind == atomic_instruction_kind::invalid)
        { result.error = atomic_immediate_error::opcode; return result; }
        if(result.descriptor.kind == atomic_instruction_kind::fence)
        {
            if(cursor == code_end || *cursor != ::std::byte{})
            { result.error = atomic_immediate_error::reserved; return result; }
            // [FE subopcode] 00 ... (code_end)
            // [safe        ][safe] unsafe (could be code_end)
            //                 ^^ cursor: cursor != code_end above proves the reserved byte readable.
            ++cursor;
            // [FE subopcode 00] ... (code_end)
            // [safe           ] unsafe (could be code_end)
            //                   ^^ cursor: consumed one checked literal reserved byte.
        }
        else
        {
            // [FE subopcode] memarg ... (code_end)
            // [safe        ] unsafe (could be code_end); transactional bounded memarg scan.
            if constexpr(Address64Encoding)
            { result.memory = scan_memory_argument64(cursor, code_end, multi_memory, wide_encoding); }
            else { result.memory = scan_memory_argument(cursor, code_end, multi_memory); }
            // [FE subopcode memarg] ... (code_end)
            // [safe              ] unsafe (could be code_end)
            //                      ^^ cursor on success; no memory record dereferenced.
            if(result.memory.error != memory_immediate_error::ok)
            { result.error = atomic_immediate_error::memory_argument; return result; }
            if(result.memory.alignment != result.descriptor.natural_alignment)
            { result.error = atomic_immediate_error::alignment; return result; }
        }
        // atomic instruction immediate ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
        code_curr = cursor;
        // [FE subopcode immediate] ... (code_end)
        // [safe                 ] unsafe (could be code_end)
        //                         ^^ code_curr: commit only the complete accepted immediate.
        return result;
    }

    [[nodiscard]] inline constexpr atomic_instruction_immediate scan_atomic_instruction(
        ::std::byte const*& code_curr, ::std::byte const* code_end, bool multi_memory) noexcept
    {
        // [FE] subopcode immediate ... (code_end)
        // [safe] unsafe (could be code_end); bounded shared scanner commits only success.
        return scan_atomic_instruction_impl<false>(code_curr, code_end, multi_memory, multi_memory);
    }

    [[nodiscard]] inline constexpr atomic_instruction64_immediate scan_atomic_instruction64(
        ::std::byte const*& code_curr, ::std::byte const* code_end, bool multi_memory, bool wide_encoding = true) noexcept
    {
        // [FE] subopcode immediate ... (code_end)
        // [safe] unsafe (could be code_end); offset stays u64 until selected-memory validation.
        return scan_atomic_instruction_impl<true>(code_curr, code_end, multi_memory, wide_encoding);
    }

}
