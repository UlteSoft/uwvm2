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
# include "recursive_type_validation.h"
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    struct core3_operand
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
        bool unknown{};
    };
    enum class core3_reference_error : unsigned
    {
        ok, stack_underflow, expected_reference, type_mismatch, unknown_type,
        expected_function_type, tail_result_mismatch, invalid_branch_signature,
        feature_disabled, unsupported_gc_opcode, expected_struct_type, expected_array_type,
        unknown_field, immutable_field, packed_access_mismatch, nondefaultable_field,
        incompatible_storage, unknown_segment
    };
    [[nodiscard]] inline constexpr core3_reference_error core3_reference_error_from_typed_stack(typed_stack_error error) noexcept
    {
        switch(error)
        {
            case typed_stack_error::ok: return core3_reference_error::ok;
            case typed_stack_error::stack_underflow: return core3_reference_error::stack_underflow;
            case typed_stack_error::type_mismatch: return core3_reference_error::type_mismatch;
            case typed_stack_error::expected_reference: return core3_reference_error::expected_reference;
        }
        return core3_reference_error::type_mismatch;
    }
    // Shared validation operations use this interface, so compiler stacks can adapt without executing a second
    // validator. The concrete implementation supports standalone consumers and conformance fixtures.
    class core3_operand_stack
    {
        ::uwvm2::utils::container::vector<core3_operand> values_{};
        ::std::size_t height_{};
        bool unreachable_{};
    public:
        [[nodiscard]] inline constexpr ::std::size_t size() const noexcept { return values_.size(); }
        [[nodiscard]] inline constexpr bool set_control_frame(::std::size_t height, bool unreachable) noexcept
        {
            if(height > values_.size()) { return false; }
            height_ = height; unreachable_ = unreachable; return true;
        }
        inline constexpr void push(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type) noexcept
        { values_.push_back({type, false}); }
        [[nodiscard]] inline constexpr bool pop(core3_operand& value) noexcept
        {
            auto const count{[&]() constexpr noexcept { return values_.size() - height_; }};
            auto const consume{[&]() constexpr noexcept
            {
                // Count > 0 was proved by the shared kernel: copy the live top before removal.
                auto const owned{values_.back_unchecked()}; values_.pop_back_unchecked(); return owned;
            }};
            return pop_core3_typed_operand(unreachable_, count, consume, value) == typed_stack_error::ok;
        }
        [[nodiscard]] inline constexpr core3_reference_error pop_expected(
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected, recursive_type_context const& context) noexcept
        {
            auto const count{[&]() constexpr noexcept { return values_.size() - height_; }};
            auto const consume{[&]() constexpr noexcept
            {
                // The shared kernel proved a concrete top above height_ before this borrow/removal.
                auto const value{values_.back_unchecked()}; values_.pop_back_unchecked(); return value;
            }};
            auto const matches{[&](auto actual, auto wanted) constexpr noexcept { return context.matches(actual, wanted); }};
            return core3_reference_error_from_typed_stack(
                pop_core3_expected_operand(unreachable_, count, consume, expected, matches));
        }
        // A u32 array.new_fixed count may be 2^32-1 in unreachable code. Check only concrete operands,
        // then consume the polymorphic remainder in O(1), without allocating or iterating over that count.
        [[nodiscard]] inline constexpr core3_reference_error pop_repeated(
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected,
            ::std::uint_least32_t count, recursive_type_context const& context) noexcept
        {
            auto const concrete_count{[&]() constexpr noexcept { return values_.size() - height_; }};
            auto const consume{[&]() constexpr noexcept
            {
                // The shared bounded repetition proved one concrete top above height_ before this copy/removal.
                auto const value{values_.back_unchecked()}; values_.pop_back_unchecked(); return value;
            }};
            auto const matches{[&](auto actual, auto wanted) constexpr noexcept { return context.matches(actual, wanted); }};
            return core3_reference_error_from_typed_stack(
                pop_core3_repeated_operands(unreachable_, concrete_count, consume, expected, count, matches));
        }
        [[nodiscard]] inline constexpr bool check_signature(
            ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const> expected,
            recursive_type_context const& context) const noexcept
        {
            auto const available{values_.size() - height_};
            if(!unreachable_ && expected.size() > available) { return false; }
            auto const count{expected.size() < available ? expected.size() : available};
            for(::std::size_t i{}; i != count; ++i)
            {
                // i < available <= values_.size() and i < expected.size() prove both reverse-index accesses.
                auto const& actual{values_.index_unchecked(values_.size() - 1uz - i)};
                if(!actual.unknown && !context.matches(actual.type, expected[expected.size() - 1uz - i])) { return false; }
            }
            return true;
        }
        inline constexpr void make_unreachable() noexcept
        {
            // The private height invariant guarantees resize only removes operands belonging to this frame.
            make_core3_frame_unreachable(unreachable_, [&]() constexpr noexcept { values_.resize(height_); });
        }
    };
    namespace reference_validation_details
    {
        namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
        template<typename Stack>
        [[nodiscard]] inline constexpr core3_reference_error pop_reference(Stack& stack, types::core_value_type& output) noexcept
        {
            core3_operand operand{};
            if(!stack.pop(operand)) { return core3_reference_error::stack_underflow; }
            if(operand.unknown)
            {
                output = {types::value_kind::reference, {types::heap_type::bottom_code}, true};
                return core3_reference_error::ok;
            }
            if(operand.type.kind != types::value_kind::reference) { return core3_reference_error::expected_reference; }
            output = operand.type; return core3_reference_error::ok;
        }
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_ref_null(Stack& stack,
        ::uwvm2::parser::wasm::standard::wasm3::type::heap_type heap, recursive_type_context const& context) noexcept
    {
        if(heap.is_defined() ? !context.contains(static_cast<::std::uint_least64_t>(heap.code)) : !recursive_validation_details::abstract_valid(heap))
        { return core3_reference_error::unknown_type; }
        stack.push({::uwvm2::parser::wasm::standard::wasm3::type::value_kind::reference, heap, true});
        return core3_reference_error::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_ref_func_type(Stack& stack,
        ::std::uint_least32_t type_index, recursive_type_context const& context) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(!context.contains(type_index)) { return core3_reference_error::unknown_type; }
        // The caller separately checks the function index and its declarative reference set. Only then is this
        // resolved type index supplied; it must classify a function, not merely name an existing GC type.
        if(context.records.index_unchecked(type_index).kind != t::composite_kind::function) { return core3_reference_error::expected_function_type; }
        stack.push({t::value_kind::reference, {type_index}, false}); return core3_reference_error::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_ref_is_null(Stack& stack) noexcept
    {
        reference_validation_details::types::core_value_type value{};
        auto const error{reference_validation_details::pop_reference(stack, value)};
        if(error != core3_reference_error::ok) { return error; }
        stack.push({reference_validation_details::types::value_kind::i32}); return core3_reference_error::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_ref_as_non_null(Stack& stack) noexcept
    {
        core3_operand operand{};
        if(!stack.pop(operand)) { return core3_reference_error::stack_underflow; }
        reference_validation_details::types::core_value_type value{};
        auto const error{core3_reference_error_from_typed_stack(narrow_core3_non_null_reference(operand, value))};
        if(error != core3_reference_error::ok) { return error; }
        stack.push(value); return core3_reference_error::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_call_ref(Stack& stack,
        ::std::uint_least32_t type_index, ::uwvm2::parser::wasm::standard::wasm3::type::sub_type const& signature,
        recursive_type_context const& context, bool tail,
        ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const> caller_results = {}) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(!context.contains(type_index)) { return core3_reference_error::unknown_type; }
        // The caller resolves signature from the validated type-index table. Check both metadata categories
        // before borrowing its parameter/result vectors; keeping type resolution outside avoids copied stacks.
        if(context.records.index_unchecked(type_index).kind != t::composite_kind::function || signature.kind != t::composite_kind::function)
        { return core3_reference_error::expected_function_type; }
        if(tail)
        {
            if(signature.results.size() != caller_results.size()) { return core3_reference_error::tail_result_mismatch; }
            for(::std::size_t i{}; i != caller_results.size(); ++i)
            { if(!context.matches(signature.results.index_unchecked(i), caller_results[i])) { return core3_reference_error::tail_result_mismatch; } }
        }
        auto error{stack.pop_expected({t::value_kind::reference, {type_index}, true}, context)};
        if(error != core3_reference_error::ok) { return error; }
        for(::std::size_t i{signature.parameters.size()}; i != 0uz; --i)
        {
            // i in [1, parameter_count] proves i - 1 is a live signature operand.
            error = stack.pop_expected(signature.parameters.index_unchecked(i - 1uz), context);
            if(error != core3_reference_error::ok) { return error; }
        }
        if(tail) { stack.make_unreachable(); }
        else { for(auto result : signature.results) { stack.push(result); } }
        return core3_reference_error::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_reference_error validate_core3_branch_on_null(Stack& stack,
        bool branch_on_non_null,
        ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const> label,
        recursive_type_context const& context) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        t::core_value_type reference{};
        auto const error{reference_validation_details::pop_reference(stack, reference)};
        if(error != core3_reference_error::ok) { return error; }
        reference.nullable = false;
        if(branch_on_non_null)
        {
            if(label.empty() || label.back().kind != t::value_kind::reference) { return core3_reference_error::invalid_branch_signature; }
            if(!context.matches(reference, label.back())) { return core3_reference_error::type_mismatch; }
            // label is nonempty. The prefix view stays inside the same live label type array, with no subtraction on null.
            label = label.first(label.size() - 1uz);
        }
        for(::std::size_t i{label.size()}; i != 0uz; --i)
        {
            if(stack.pop_expected(label[i - 1uz], context) != core3_reference_error::ok)
            { return core3_reference_error::invalid_branch_signature; }
        }
        // Reify the label signature even in unreachable code. A mere peek would leave missing operands
        // polymorphic, incorrectly permitting later numeric operations on a known reference label (or vice versa).
        for(auto type : label) { stack.push(type); }
        if(!branch_on_non_null) { stack.push(reference); }
        return core3_reference_error::ok;
    }
}
