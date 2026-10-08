/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <memory>
# include <span>
# include "reference_validation.h"
# include "gc_i31_semantics.h"
# include "local_declarations.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // All types/segment declarations must already be validated against the same recursive_type_context.
    // These synchronous borrowed spans must not outlive, mutate, or reallocate their owning module vectors.
    // data_count is the declared data-count section (not the number of payloads seen so far).
    struct core3_gc_environment
    {
        ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::sub_type const* const> definitions{};
        ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const> elements{};
        ::std::uint_least64_t data_count{};
        // An optional parser-owned flat-index source avoids rebuilding a pointer vector for
        // every JIT GC instruction. The validated groups stay immutable for this module life.
        ::uwvm2::parser::wasm::standard::wasm3::type::recursive_type_section const* definition_section{};
    };
    namespace gc_validation_details
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        [[nodiscard]] inline constexpr t::core_value_type ref(::std::int_least64_t heap, bool nullable = true) noexcept
        { return {t::value_kind::reference, {heap}, nullable}; }
        [[nodiscard]] inline constexpr t::core_value_type unpack(t::storage_type storage) noexcept
        { return storage.packed == t::packed_kind::none ? storage.value : t::core_value_type{t::value_kind::i32}; }
        [[nodiscard]] inline constexpr bool storage_matches(t::storage_type from, t::storage_type to,
                                                           recursive_type_context const& context) noexcept
        {
            // Packed widths are invariant, even though both unpack to i32 on the operand stack.
            return from.packed == to.packed && (from.packed != t::packed_kind::none || context.matches(from.value, to.value));
        }
        [[nodiscard]] inline constexpr t::sub_type const* definition(core3_gc_environment const& environment,
            recursive_type_context const& context, ::std::uint_least32_t index) noexcept
        {
            if(!context.contains(index)) { return nullptr; }
            if(environment.definition_section != nullptr)
            {
                auto const& section{*environment.definition_section};
                if(section.type_count != context.records.size()) { return nullptr; }
                // Validated first_type_index values form sorted, non-overlapping flat ranges.
                // Binary search borrows group objects only after middle < groups.size().
                ::std::size_t first{}, last{section.groups.size()};
                while(first != last)
                {
                    auto const middle{first + (last - first) / 2uz};
                    auto const& group{section.groups.index_unchecked(middle)};
                    if(group.first_type_index <= index) { first = middle + 1uz; }
                    else { last = middle; }
                }
                if(first == 0uz) { return nullptr; }
                auto const& group{section.groups.index_unchecked(first - 1uz)};
                auto const local{static_cast<::std::uint_least64_t>(index) - group.first_type_index};
                if(local >= group.types.size()) { return nullptr; }
                // local < group.types.size() proves a live parser-owned subtype borrow.
                auto const* type{::std::addressof(group.types.index_unchecked(static_cast<::std::size_t>(local)))};
                return type->kind == context.records.index_unchecked(index).kind ? type : nullptr;
            }
            if(environment.definitions.size() != context.records.size()) { return nullptr; }
            // index < context.records.size() == definitions.size() proves the span access and size_t conversion.
            auto const type{environment.definitions[static_cast<::std::size_t>(index)]};
            // This copies an owned-module object pointer; no input/operand cursor changes or pointer arithmetic occurs.
            return type != nullptr && type->kind == context.records.index_unchecked(index).kind ? type : nullptr;
        }
    }
    // Shared aggregate/scalar-reference typing for compiler stack adapters. The caller decodes bounded unsigned
    // immediates and checks its feature policy; gc_enabled is explicit so disabled GC fails before stack mutation.
    // opcode is the Core 3 0xfb subopcode, first is typeidx and second is fieldidx/typeidx/segmentidx/fixed count.
    // Cast/branch-cast opcodes 20..25 are intentionally rejected until their separate control rules are wired in.
    // https://webassembly.github.io/spec/core/valid/instructions.html#aggregate-reference-instructions
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_gc_instruction(Stack& stack,
        ::std::uint_least32_t opcode, ::std::uint_least32_t first, ::std::uint_least32_t second,
        core3_gc_environment const& environment, recursive_type_context const& context, bool gc_enabled) noexcept
    {
        namespace d = gc_validation_details;
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = core3_reference_error;
        if(!gc_enabled) { return e::feature_disabled; }
        constexpr t::core_value_type i32{t::value_kind::i32};
        auto pop{[&](t::core_value_type expected) constexpr noexcept { return stack.pop_expected(expected, context); }};
        if(opcode == 15u)
        {
            auto const result{pop(d::ref(-22))}; if(result != e::ok) { return result; }
            stack.push(i32); return e::ok;
        }
        auto const i31_signature{describe_core3_i31_instruction(opcode)};
        if(i31_signature.supported)
        {
            // Stack::pop_expected uses the same shared current-frame kernel.
            auto const result{pop(i31_signature.input)}; if(result != e::ok) { return result; }
            stack.push(i31_signature.output); return e::ok;
        }
        if(opcode > 19u) { return e::unsupported_gc_opcode; }
        auto const type{d::definition(environment, context, first)};
        if(type == nullptr) { return e::unknown_type; }
        if(opcode <= 5u)
        {
            if(type->kind != t::composite_kind::struct_) { return e::expected_struct_type; }
            if(opcode <= 1u)
            {
                // Reverse field indices model operand pops; i > 0 proves i-1 lies inside the field vector.
                for(::std::size_t i{type->fields.size()}; i != 0uz; --i)
                {
                    auto const value{d::unpack(type->fields.index_unchecked(i - 1uz).storage)};
                    if(opcode == 1u)
                    { if(!core3_value_is_defaultable(value)) { return e::nondefaultable_field; } }
                    else { auto const result{pop(value)}; if(result != e::ok) { return result; } }
                }
                stack.push(d::ref(first, false)); return e::ok;
            }
            if(second >= type->fields.size()) { return e::unknown_field; }
            // second < fields.size() proves this field reference; the module storage is immutable during validation.
            auto const& field{type->fields.index_unchecked(second)};
            auto const value{d::unpack(field.storage)};
            if(opcode == 5u)
            {
                if(!field.mutable_) { return e::immutable_field; }
                auto const result{pop(value)}; if(result != e::ok) { return result; }
                return pop(d::ref(first));
            }
            if((opcode != 2u) != (field.storage.packed != t::packed_kind::none)) { return e::packed_access_mismatch; }
            auto const result{pop(d::ref(first))}; if(result != e::ok) { return result; }
            stack.push(value); return e::ok;
        }
        if(type->kind != t::composite_kind::array || type->fields.size() != 1uz) { return e::expected_array_type; }
        // The array shape check proves the one field exists before obtaining its reference.
        auto const& field{type->fields.index_unchecked(0uz)};
        auto const value{d::unpack(field.storage)};
        auto pop_i32{[&]() constexpr noexcept { return pop(i32); }};
        if(opcode == 6u || opcode == 7u || opcode == 8u)
        {
            if(opcode == 7u && !core3_value_is_defaultable(value)) { return e::nondefaultable_field; }
            e result{};
            if(opcode == 8u) { result = stack.pop_repeated(value, second, context); }
            else { result = pop_i32(); if(result == e::ok && opcode == 6u) { result = pop(value); } }
            if(result != e::ok) { return result; }
            stack.push(d::ref(first, false)); return e::ok;
        }
        if(opcode >= 11u && opcode <= 13u)
        {
            if((opcode != 11u) != (field.storage.packed != t::packed_kind::none)) { return e::packed_access_mismatch; }
            auto result{pop_i32()}; if(result != e::ok) { return result; }
            result = pop(d::ref(first)); if(result != e::ok) { return result; }
            stack.push(value); return e::ok;
        }
        if(opcode == 14u || opcode == 16u)
        {
            if(!field.mutable_) { return e::immutable_field; }
            if(opcode == 16u) { auto result{pop_i32()}; if(result != e::ok) { return result; } }
            auto result{pop(value)}; if(result != e::ok) { return result; }
            result = pop_i32(); if(result != e::ok) { return result; }
            return pop(d::ref(first));
        }
        if(opcode == 17u)
        {
            if(!field.mutable_) { return e::immutable_field; }
            auto const source{d::definition(environment, context, second)};
            if(source == nullptr) { return e::unknown_type; }
            if(source->kind != t::composite_kind::array || source->fields.size() != 1uz) { return e::expected_array_type; }
            // Source shape proves index zero; copy compatibility uses storage types, not their unpacked values.
            if(!d::storage_matches(source->fields.index_unchecked(0uz).storage, field.storage, context)) { return e::incompatible_storage; }
            auto result{pop_i32()}; if(result != e::ok) { return result; } // length
            result = pop_i32(); if(result != e::ok) { return result; } // source element offset
            result = pop(d::ref(second)); if(result != e::ok) { return result; }
            result = pop_i32(); if(result != e::ok) { return result; } // destination element offset
            return pop(d::ref(first));
        }
        // Remaining aggregate subopcodes: array.new_data/new_elem/init_data/init_elem.
        bool const initializing{opcode == 18u || opcode == 19u};
        bool const data{opcode == 9u || opcode == 18u};
        if(initializing && !field.mutable_) { return e::immutable_field; }
        if(data)
        {
            if(value.kind == t::value_kind::reference) { return e::incompatible_storage; }
            if(second >= environment.data_count) { return e::unknown_segment; }
        }
        else
        {
            if(second >= environment.elements.size()) { return e::unknown_segment; }
            // second < elements.size() proves access; element reference types cannot initialize packed storage.
            if(field.storage.packed != t::packed_kind::none || value.kind != t::value_kind::reference ||
               !context.matches(environment.elements[second], value)) { return e::incompatible_storage; }
        }
        auto result{pop_i32()}; if(result != e::ok) { return result; } // count
        result = pop_i32(); if(result != e::ok) { return result; } // segment offset
        if(initializing)
        {
            result = pop_i32(); if(result != e::ok) { return result; } // destination offset
            return pop(d::ref(first));
        }
        stack.push(d::ref(first, false)); return e::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_ref_eq(Stack& stack,
        recursive_type_context const& context, bool gc_enabled) noexcept
    {
        using e = core3_reference_error;
        if(!gc_enabled) { return e::feature_disabled; }
        for(unsigned i{}; i != 2u; ++i)
        {
            auto const result{stack.pop_expected(gc_validation_details::ref(-19), context)};
            if(result != e::ok) { return result; }
        }
        stack.push({::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i32}); return e::ok;
    }
}
