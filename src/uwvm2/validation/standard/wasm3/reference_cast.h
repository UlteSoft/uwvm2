/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <span>
# include "reference_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    namespace reference_cast_details
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        [[nodiscard]] inline constexpr bool heap_top(t::heap_type heap, recursive_type_context const& context,
                                                    t::heap_type& top) noexcept
        {
            if(heap.is_defined())
            {
                if(!context.contains(static_cast<::std::uint_least64_t>(heap.code))) { return false; }
                // contains() bounds the index before narrowing and accessing the validated record vector.
                auto const kind{context.records.index_unchecked(static_cast<::std::size_t>(heap.code)).kind};
                top.code = kind == t::composite_kind::function ? -16 : -18; return true;
            }
            // Heap bottom is a validation artifact, never a legal immediate. The four Core 3 hierarchies stay distinct.
            switch(heap.code)
            {
                case -12: case -23: top.code = -23; return true;
                case -13: case -16: top.code = -16; return true;
                case -14: case -17: top.code = -17; return true;
                case -15: case -18: case -19: case -20: case -21: case -22: top.code = -18; return true;
                default: return false;
            }
        }
        [[nodiscard]] inline constexpr bool valid(t::core_value_type type, recursive_type_context const& context) noexcept
        { t::heap_type ignored{}; return type.kind == t::value_kind::reference && heap_top(type.heap, context, ignored); }
    }
    // ref.test/ref.cast may start from any type in the target's heap hierarchy, including siblings. They do not
    // require the actual stack value to be a supertype of the target; the rule may choose their common top input.
    // https://webassembly.github.io/spec/core/valid/instructions.html#valid-ref.cast
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_ref_cast(Stack& stack,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target,
        recursive_type_context const& context, bool test_only, bool gc_enabled) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = core3_reference_error;
        if(!gc_enabled) { return e::feature_disabled; }
        t::heap_type top{};
        if(target.kind != t::value_kind::reference || !reference_cast_details::heap_top(target.heap, context, top))
        { return e::unknown_type; }
        auto const result{stack.pop_expected({t::value_kind::reference, top, true}, context)};
        if(result != e::ok) { return result; }
        stack.push(test_only ? t::core_value_type{t::value_kind::i32} : target); return e::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_convert_reference(Stack& stack,
        recursive_type_context const& context, bool extern_to_any, bool gc_enabled) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = core3_reference_error;
        if(!gc_enabled) { return e::feature_disabled; }
        core3_operand operand{};
        if(!stack.pop(operand)) { return e::stack_underflow; }
        t::core_value_type const expected{t::value_kind::reference, {extern_to_any ? -17 : -18}, true};
        if(!operand.unknown && !context.matches(operand.type, expected)) { return e::type_mismatch; }
        // A polymorphic input chooses the principal NON-NULL output, but still in the fixed any/extern hierarchy.
        // Producing whole-value bottom (or universal heap bottom) would wrongly permit numeric/unrelated results.
        stack.push({t::value_kind::reference, {extern_to_any ? -18 : -17}, !operand.unknown && operand.type.nullable});
        return e::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_branch_on_cast(Stack& stack,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type from,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type to,
        ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const> label,
        recursive_type_context const& context, bool branch_on_failure, bool gc_enabled) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using e = core3_reference_error;
        if(!gc_enabled) { return e::feature_disabled; }
        if(!reference_cast_details::valid(from, context) || !reference_cast_details::valid(to, context)) { return e::unknown_type; }
        if(!context.matches(to, from)) { return e::type_mismatch; }
        if(label.empty() || label.back().kind != t::value_kind::reference) { return e::invalid_branch_signature; }
        // Core 3 reference difference approximates only nullability, never removes heap types or makes code unreachable.
        auto difference{from}; if(to.nullable) { difference.nullable = false; }
        if(!context.matches(branch_on_failure ? difference : to, label.back())) { return e::invalid_branch_signature; }
        auto result{stack.pop_expected(from, context)}; if(result != e::ok) { return result; }
        // Nonempty label proves the prefix length; first() forms a bounded subspan and performs no null subtraction.
        auto const prefix{label.first(label.size() - 1uz)};
        for(::std::size_t i{prefix.size()}; i != 0uz; --i)
        {
            // i in [1, prefix.size()] proves prefix[i-1]; pop_expected owns its operand-stack bounds proof.
            result = stack.pop_expected(prefix[i - 1uz], context); if(result != e::ok) { return result; }
        }
        // Reify the complete fallthrough type even in unreachable code. Do not preserve a more specific actual operand.
        for(auto type : prefix) { stack.push(type); }
        stack.push(branch_on_failure ? to : difference); return e::ok;
    }
}
