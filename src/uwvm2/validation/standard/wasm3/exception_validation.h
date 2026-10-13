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
# include "exception_immediate.h"
# include "reference_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class core3_exception_error : unsigned
    {
        ok, feature_disabled, unknown_tag, unknown_label, invalid_tag_type,
        label_type_mismatch, invalid_catch_kind, stack_underflow, operand_type_mismatch
    };
    namespace exception_validation_details
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        inline constexpr t::core_value_type exception_reference(bool nullable) noexcept
        { return {t::value_kind::reference, {static_cast<::std::int_least64_t>(t::abstract_heap_type::exn)}, nullable}; }
        [[nodiscard]] inline constexpr core3_exception_error operand_error(core3_reference_error error) noexcept
        {
            return error == core3_reference_error::ok ? core3_exception_error::ok :
                error == core3_reference_error::stack_underflow ? core3_exception_error::stack_underflow :
                core3_exception_error::operand_type_mismatch;
        }
    }
    // The compiler supplies indexed views of already-resolved tag parameters and OUTER label types.
    // No type vectors are copied, and no operand stack is inspected or consumed when checking handlers.
    // Valid/catch requires exactly the payload tuple, plus NON-NULL exn for the *_ref variants.
    // https://webassembly.github.io/spec/core/valid/instructions.html#valid-catch
    template<typename ParameterAt, typename LabelAt, typename Context>
    [[nodiscard]] inline constexpr core3_exception_error validate_exception_catch_signature(exception_catch_kind kind,
        ::std::size_t parameter_count, ParameterAt parameter_at, ::std::size_t label_count, LabelAt label_at,
        Context const& context) noexcept
    {
        using e = core3_exception_error;
        if(static_cast<unsigned>(kind) > 3u) { return e::invalid_catch_kind; }
        bool const tagged{kind == exception_catch_kind::tagged || kind == exception_catch_kind::tagged_ref};
        bool const with_reference{kind == exception_catch_kind::tagged_ref || kind == exception_catch_kind::all_ref};
        auto const payload_count{tagged ? parameter_count : 0uz};
        // Subtract only after label_count > 0, avoiding overflow in payload_count + 1.
        if(with_reference ? (label_count == 0uz || label_count - 1uz != payload_count) : label_count != payload_count)
        { return e::label_type_mismatch; }
        for(::std::size_t i{}; i != payload_count; ++i)
        {
            // [0, payload_count) lies in both tuples by the arity check; callbacks borrow live validated storage.
            if(!context.matches(parameter_at(i), label_at(i))) { return e::label_type_mismatch; }
        }
        if(with_reference && !context.matches(exception_validation_details::exception_reference(false), label_at(payload_count)))
        { return e::label_type_mismatch; }
        return e::ok;
    }
    struct core3_exception_environment
    {
        // A tag's entry is its resolved function type, NOT its runtime identity. Equivalent types still
        // belong to different tags; validation may compare signatures, runtime dispatch must compare identities.
        ::std::span<::uwvm2::parser::wasm::standard::wasm3::type::sub_type const* const> tags{};
        // Innermost OUTER label is index 0. try_table's own label must not yet be included.
        ::std::span<::std::span<::uwvm2::parser::wasm::standard::wasm3::type::core_value_type const> const> labels{};
    };
    [[nodiscard]] inline constexpr core3_exception_error validate_core3_exception_catch(exception_catch_clause const& clause,
        core3_exception_environment const& environment, recursive_type_context const& context, bool enabled) noexcept
    {
        using e = core3_exception_error;
        if(!enabled) { return e::feature_disabled; }
        if(static_cast<unsigned>(clause.kind) > 3u) { return e::invalid_catch_kind; }
        ::std::span<exception_validation_details::t::core_value_type const> parameters{};
        if(clause.kind == exception_catch_kind::tagged || clause.kind == exception_catch_kind::tagged_ref)
        {
            if(clause.tag_index >= environment.tags.size()) { return e::unknown_tag; }
            auto const tag{environment.tags[clause.tag_index]};
            if(tag == nullptr || tag->kind != exception_validation_details::t::composite_kind::function || !tag->results.empty())
            { return e::invalid_tag_type; }
            parameters = {tag->parameters.data(), tag->parameters.size()};
        }
        if(clause.label_index >= environment.labels.size()) { return e::unknown_label; }
        auto const label{environment.labels[clause.label_index]};
        return validate_exception_catch_signature(clause.kind, parameters.size(), [&](::std::size_t i) constexpr noexcept { return parameters[i]; },
            label.size(), [&](::std::size_t i) constexpr noexcept { return label[i]; }, context);
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_exception_error validate_core3_throw(Stack& stack, ::std::uint_least32_t tag_index,
        core3_exception_environment const& environment, recursive_type_context const& context, bool enabled) noexcept
    {
        using e = core3_exception_error;
        if(!enabled) { return e::feature_disabled; }
        if(tag_index >= environment.tags.size()) { return e::unknown_tag; }
        auto const tag{environment.tags[tag_index]};
        if(tag == nullptr || tag->kind != exception_validation_details::t::composite_kind::function || !tag->results.empty())
        { return e::invalid_tag_type; }
        for(::std::size_t i{tag->parameters.size()}; i != 0uz; --i)
        {
            // i in [1, parameter_count] proves i - 1 names a live payload type; pop in reverse stack order.
            auto const error{exception_validation_details::operand_error(stack.pop_expected(tag->parameters.index_unchecked(i - 1uz), context))};
            if(error != e::ok) { return error; }
        }
        stack.make_unreachable();
        return e::ok;
    }
    template<typename Stack>
    [[nodiscard]] inline constexpr core3_exception_error validate_core3_throw_ref(Stack& stack,
        recursive_type_context const& context, bool enabled) noexcept
    {
        if(!enabled) { return core3_exception_error::feature_disabled; }
        // Nullable exn is valid statically. A null value traps at runtime; it is not a catchable guest throw.
        auto const error{exception_validation_details::operand_error(stack.pop_expected(exception_validation_details::exception_reference(true), context))};
        if(error != core3_exception_error::ok) { return error; }
        stack.make_unreachable();
        return core3_exception_error::ok;
    }
}
