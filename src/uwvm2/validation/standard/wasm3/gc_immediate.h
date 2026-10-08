/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "recursive_type_binary.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class gc_immediate_error : unsigned { ok, malformed, unknown_opcode, cast_flags };
    struct gc_instruction_immediate
    {
        ::std::uint_least32_t opcode{};
        // Aggregate operations: typeidx and fieldidx/typeidx/segmentidx/fixed count.
        // Branch-cast operations: first is labelidx; from/to contain both heap and nullability.
        ::std::uint_least32_t first{}, second{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type from{}, to{};
        gc_immediate_error error{};
        recursive_type_binary_error binary_error{};
        ::std::size_t error_offset{};
    };
    // Core 3's complete 0xfb grammar (subopcodes 0..30). This scanner starts AFTER the 0xfb prefix.
    // Feature policy, type/segment/label resolution and operand typing are subsequent shared-validation steps.
    // https://webassembly.github.io/spec/core/binary/instructions.html
    [[nodiscard]] inline constexpr gc_instruction_immediate scan_gc_instruction(
        ::std::byte const*& code_curr, ::std::byte const* code_end) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        // [FB] subopcode immediate ... code_end
        // [safe] unsafe (could be code_end)
        //        ^^ code_curr; caller proves the common allocation. Equal endpoints may both be null.
        recursive_binary_details::reader input{{code_curr, code_curr == code_end ? 0uz : static_cast<::std::size_t>(code_end - code_curr)}};
        gc_instruction_immediate result{};
        auto fail{[&](gc_immediate_error error) constexpr noexcept
        { result.error = error; result.binary_error = input.status.error; result.error_offset = input.position; return result; }};
        if(!input.u32(result.opcode)) { return fail(gc_immediate_error::malformed); }
        if(result.opcode > 30u) { return fail(gc_immediate_error::unknown_opcode); }
        if(result.opcode <= 19u)
        {
            // array.len is the only aggregate opcode without a type index.
            if(result.opcode != 15u && !input.u32(result.first)) { return fail(gc_immediate_error::malformed); }
            if((result.opcode >= 2u && result.opcode <= 5u) || result.opcode == 8u || result.opcode == 9u ||
               result.opcode == 10u || result.opcode == 17u || result.opcode == 18u || result.opcode == 19u)
            { if(!input.u32(result.second)) { return fail(gc_immediate_error::malformed); } }
        }
        else if(result.opcode <= 23u)
        {
            result.to.kind = t::value_kind::reference;
            result.to.nullable = (result.opcode & 1u) != 0u;
            if(!input.heap(result.to.heap)) { return fail(gc_immediate_error::malformed); }
        }
        else if(result.opcode <= 25u)
        {
            unsigned flags{};
            if(!input.byte(flags)) { return fail(gc_immediate_error::malformed); }
            // This is one literal byte, not a padded LEB. All bits except source/target nullability are reserved.
            if(flags > 3u) { return fail(gc_immediate_error::cast_flags); }
            result.from.kind = result.to.kind = t::value_kind::reference;
            result.from.nullable = (flags & 1u) != 0u; result.to.nullable = (flags & 2u) != 0u;
            if(!input.u32(result.first) || !input.heap(result.from.heap) || !input.heap(result.to.heap))
            { return fail(gc_immediate_error::malformed); }
        }
        // All byte access and local offsets were checked by reader. Success consumed at least the opcode byte,
        // so code_curr is non-null and input.position <= the original range length proves this sole pointer addition.
        // [FB][bounded subopcode/immediate] next ... code_end
        // [safe                           ] unsafe (could be code_end)
        // ^^ code_curr: the input reader has proved the committed span lies in the same allocation.
        code_curr += input.position;
        // [FB subopcode complete immediate] next ... code_end
        // [safe                           ] unsafe (could be code_end)
        //                                   ^^ code_curr; failures never change the caller cursor.
        return result;
    }
}
