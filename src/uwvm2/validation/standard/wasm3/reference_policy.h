/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <utility>
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include "tail_call.h"
# include "heap_immediate.h"
# include "value_immediate.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3 blocktype embeds these complete reference valtypes before the s33
    // type-index alternative. Legacy funcref/externref still use the old feature
    // gate, while this set is decoded with its own heap-specific Core 3 policy.
    [[nodiscard]] inline constexpr bool is_core3_extended_block_reference_prefix(unsigned prefix) noexcept
    {
        return prefix == 0x63u || prefix == 0x64u ||
            (prefix >= 0x69u && prefix <= 0x6eu) ||
            (prefix >= 0x71u && prefix <= 0x74u);
    }

    inline constexpr void require_function_references_enabled(bool enabled, unsigned opcode,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled) { return; }
        // [ref.as_non_null] next opcode ... end
        // [safe           ] unsafe (could be end)
        // ^^ op_begin / err_curr: borrow the dispatch-checked opcode; no read or increment.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = opcode, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references,
            .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Read one Core 3 ref.null heap immediate in a function-only indexed type context. Concrete
    // functions retain the legacy funcref carrier. The pure validator may explicitly opt in to a
    // distinct static exn token; no execution backend accidentally receives that reference kind.
    [[nodiscard]] inline constexpr unsigned read_function_ref_null_carrier(
        ::std::byte const*& cursor, ::std::byte const* end, bool enabled, ::std::size_t type_count,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err,
        bool exception_references = false, bool gc_enabled = false) UWVM_THROWS
    {
        // [ref.null] heap ... end
        // [safe    ] unsafe (could be end)
        //            ^^ begin borrows cursor. Decoder commits cursor only on success.
        auto const begin{cursor};
        auto const decoded{scan_function_ref_null_heap(cursor, end, enabled, type_count, exception_references, gc_enabled)};
        // [ref.null][checked heap] next ... end, or cursor == begin on failure.
        // [safe                 ] unsafe (could be end)
        //                         ^^ cursor after success; no unchecked pointer increment occurs here.
        if(decoded.error == function_heap_immediate_error::ok) { return decoded.carrier; }
        if(decoded.error == function_heap_immediate_error::function_references_disabled)
        { require_function_references_enabled(false, 0xd0u, op_begin, err); }
        if(decoded.error == function_heap_immediate_error::gc_disabled)
        {
            // [ref.null] checked opcode; diagnostics borrow only op_begin.
            // [safe    ] unsafe (possibly end)
            // ^^ err_curr: no byte read or cursor movement follows.
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {
                .value = 0xd0u, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // [ref.null] heap ... end
        // [safe    ] diagnostic borrows the dispatch-checked opcode, never dereferenced below.
        // ^^ err_curr
        err.err_curr = op_begin;
        using error = ::uwvm2::validation::error::code_validation_error_code;
        if(decoded.error == function_heap_immediate_error::unknown_type)
        {
            err.err_code = error::illegal_type_index;
            err.err_selectable.illegal_type_index.type_index = static_cast<::std::uint_least32_t>(decoded.heap.code);
            err.err_selectable.illegal_type_index.all_type_count = static_cast<::std::uint_least32_t>(type_count);
        }
        else
        {
            err.err_code = error::wasm1p1_invalid_reference_type;
            // A missing immediate has no readable byte; retain zero in that case. Nonempty begin is bounded by end.
            err.err_selectable.wasm1p1_invalid_reference_type.value = begin == end ? 0u : ::std::to_integer<unsigned>(*begin);
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    [[nodiscard]] inline constexpr unsigned read_core3_value_carrier(
        ::std::byte const*& cursor, ::std::byte const* end, bool enabled,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err,
        ::std::size_t known_function_type_count = 0uz,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type* exact_type = nullptr,
        bool gc_enabled = false) UWVM_THROWS
    {
        // [opcode ...][valtype ... end)
        // [safe      ] unsafe (could be end)
        //              ^^ begin borrows cursor; decoder preserves it on every failure.
        auto const begin{cursor};
        auto const decoded{scan_core3_value_carrier(cursor, end, enabled, known_function_type_count, gc_enabled)};
        // [opcode ...][checked valtype] next ... end
        // [safe                      ] unsafe (could be end)
        //                              ^^ cursor on success; equals begin on failure.
        if(decoded.error == value_carrier_error::ok)
        {
            if(exact_type != nullptr) { *exact_type = decoded.type; }
            return decoded.carrier;
        }
        // A feature failure requires a completely decoded value, proving begin readable.
        if(decoded.error == value_carrier_error::function_references_disabled)
        { require_function_references_enabled(false, ::std::to_integer<unsigned>(*begin), op_begin, err); }
        if(decoded.error == value_carrier_error::gc_disabled)
        {
            // [opcode][checked disabled heap] ... end
            // [safe  ] unsafe (possibly end)
            // ^^ err_curr: the caller-proven opcode is diagnostic only.
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {
                .value = begin == end ? 0u : ::std::to_integer<unsigned>(*begin),
                .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // [opcode ...] valtype ... end
        // [safe      ]
        // ^^ err_curr: borrow the caller-proven opcode address; no dereference or arithmetic.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
        // A binary failure may have begin == end; never read it for a missing-type diagnostic.
        err.err_selectable.wasm1p1_invalid_reference_type.value = begin == end ? 0u : ::std::to_integer<unsigned>(*begin);
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Mixed Core 3 type tables require a validated composite-kind context. This overload
    // preserves the exact heap witness while projecting only the VM's fixed-size carrier.
    [[nodiscard]] inline constexpr unsigned read_core3_value_carrier(
        ::std::byte const*& cursor, ::std::byte const* end, bool function_references,
        ::std::byte const* op_begin, ::uwvm2::validation::error::code_validation_error_impl& err,
        ::std::size_t known_type_count,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type* exact_type,
        recursive_type_context const& context, bool gc_enabled, bool exceptions_enabled) UWVM_THROWS
    {
        using e = value_carrier_error;
        // [opcode][valtype ... end)
        // [safe ] unsafe (end is one-past)
        //          ^^ begin/cursor: the decoder borrows the input and commits only on success.
        auto const begin{cursor};
        auto const decoded{scan_core3_value_carrier(cursor, end, function_references,
            known_type_count, context, gc_enabled, exceptions_enabled)};
        // [opcode][complete checked valtype] next ... end on success.
        // [safe                            ] unsafe (possibly end)
        //                                  ^^ cursor: bounded scanner commit, unchanged on failure.
        if(decoded.error == e::ok)
        {
            if(exact_type != nullptr) { *exact_type = decoded.type; }
            return decoded.carrier;
        }
        // op_begin is the caller's dispatch-checked opcode; no diagnostic dereferences it.
        // [opcode][invalid/disabled valtype ... end)
        // [safe ] unsafe (possibly end)
        // ^^ err_curr: borrowed opcode address, independent of failed decoder cursor.
        err.err_curr = op_begin;
        if(decoded.error == e::function_references_disabled || decoded.error == e::gc_disabled ||
           decoded.error == e::exceptions_disabled)
        {
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {
                .value = begin == end ? 0u : ::std::to_integer<unsigned>(*begin),
                .feature = decoded.error == e::function_references_disabled ?
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references :
                    decoded.error == e::gc_disabled ?
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc :
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction};
        }
        else if(decoded.error == e::unknown_type)
        {
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
            err.err_selectable.illegal_type_index = {
                .type_index = static_cast<::std::uint_least32_t>(decoded.type.heap.code),
                .all_type_count = static_cast<::std::uint_least32_t>(known_type_count)};
        }
        else
        {
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
            err.err_selectable.wasm1p1_invalid_reference_type.value =
                begin == end ? 0u : ::std::to_integer<unsigned>(*begin);
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Adapter for the existing one-byte carrier-type stacks. Heap-bottom is a reference,
    // never the polymorphic value bottom: after unreachable; ref.as_non_null it can match
    // either supported reference carrier, but cannot satisfy a numeric/vector operand.
    // This is compile-time validation metadata; it adds no fields to guest reference values.
    template<typename ValueType>
    [[nodiscard]] inline constexpr bool is_legacy_reference_carrier(ValueType type) noexcept
    {
        // Core 3 exception references have a distinct validation carrier. Branch/reference
        // instructions may consume it, but numeric operands still cannot match it.
        auto const byte{static_cast<unsigned>(type)};
        return byte == 0x70u || byte == 0x6fu || byte == 0x69u;
    }

    template<typename Operand, typename ValueType>
    [[nodiscard]] inline constexpr bool reference_carrier_matches(Operand const& operand, ValueType expected) noexcept
    {
        return operand.is_unknown || (operand.is_reference_bottom ? is_legacy_reference_carrier(expected) : operand.type == expected);
    }

    // Runtime signatures borrow one retained parser allocation. Callers construct this view
    // only after checking both endpoints; type inspection never touches guest memory.
    template<typename Signature>
    struct core3_signature_view
    {
        Signature const* begin{};
        ::std::size_t count{};
        [[nodiscard]] inline constexpr ::std::size_t size() const noexcept { return count; }
        [[nodiscard]] inline constexpr Signature const& index_unchecked(::std::size_t index) const noexcept
        {
            // [begin, begin + count) is a retained type-section allocation.
            // [safe               ] caller has proved index < count before this read.
            //         ^^ begin[index] borrows one live signature; no pointer advances.
            return begin[index];
        }
    };

    // Function-reference subtyping is checked during translation/validation, never on
    // the interpreter's memory hot path. 0x60 function types remain structurally
    // equivalent in the absence of an explicit recursive subtype declaration. The
    // worklist makes self-recursive type uses finite without native recursion.
    template<typename Signatures>
    [[nodiscard]] inline bool core3_function_types_equivalent(
        ::std::size_t first, ::std::size_t second, Signatures const& signatures) noexcept
    {
        using pair = ::std::pair<::std::size_t, ::std::size_t>;
        using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        ::uwvm2::utils::container::vector<pair> pending{}, seen{};
        pending.push_back({first, second});
        while(!pending.empty())
        {
            auto const current{pending.back_unchecked()};
            pending.pop_back_unchecked();
            if(current.first >= signatures.size() || current.second >= signatures.size()) { return false; }
            if(current.first == current.second) { continue; }
            bool visited{};
            for(auto const& pair_seen: seen)
            {
                if(pair_seen == current) { visited = true; break; }
            }
            if(visited) { continue; }
            seen.push_back(current);
            auto const& left{signatures.index_unchecked(current.first)};
            auto const& right{signatures.index_unchecked(current.second)};
            if(left.parameters.size() != right.parameters.size() || left.results.size() != right.results.size()) { return false; }
            auto const compare_value{[&](auto a, auto b) noexcept
            {
                if(a.kind != b.kind) { return false; }
                if(a.kind != kind::reference) { return true; }
                if(a.nullable != b.nullable) { return false; }
                if(a.heap == b.heap) { return true; }
                if(!a.heap.is_defined() || !b.heap.is_defined()) { return false; }
                pending.push_back({static_cast<::std::size_t>(a.heap.code), static_cast<::std::size_t>(b.heap.code)});
                return true;
            }};
            for(::std::size_t i{}; i != left.parameters.size(); ++i)
            {
                if(!compare_value(left.parameters.index_unchecked(i), right.parameters.index_unchecked(i))) { return false; }
            }
            for(::std::size_t i{}; i != left.results.size(); ++i)
            {
                if(!compare_value(left.results.index_unchecked(i), right.results.index_unchecked(i))) { return false; }
            }
        }
        return true;
    }

    // A legacy plain function definition closes as a final singleton recursive
    // group without a declared parent. Carrier equality cannot identify an open
    // function, its child, or one member of a larger recursive group.
    [[nodiscard]] inline constexpr bool core3_is_plain_function_definition(
        ::uwvm2::parser::wasm::standard::wasm3::type::recursive_type_section const* section,
        ::std::size_t index) noexcept
    {
        if(section == nullptr || index >= section->type_count) { return false; }
        for(auto const& group : section->groups)
        {
            // [validated, ordered recursive groups][owning type vectors] one-past
            // [safe                                                     ] first_type_index
            // is immutable metadata, and no pointer or input cursor is advanced here.
            if(group.first_type_index > index) { return false; }
            if(group.first_type_index != index) { continue; }
            if(group.types.size() != 1uz) { return false; }
            auto const& type{group.types.index_unchecked(0uz)};
            return type.kind == ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind::function &&
                type.final_ && type.supertypes.empty();
        }
        return false;
    }

    enum class core3_defined_type_comparison : unsigned { structural, equivalent, different };

    // Compare complete signatures from two modules at a cold linking/validation boundary.
    // `canonical_compare(a,b)` must return equivalent/different for any explicit Core 3
    // recursive-group member; only standalone 0x60 function types may return structural.
    // This worklist preserves recursive heap identity and nullability without ever accepting
    // equal one-byte ABI carriers as proof of Core 3 type equivalence.
    template<typename LeftSignatures, typename RightSignatures, typename CanonicalCompare>
    [[nodiscard]] inline bool core3_function_types_equivalent_across_sections(
        ::std::size_t left_index, LeftSignatures const& left_signatures,
        ::std::size_t right_index, RightSignatures const& right_signatures,
        CanonicalCompare canonical_compare) noexcept
    {
        using pair = ::std::pair<::std::size_t, ::std::size_t>;
        using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        ::uwvm2::utils::container::vector<pair> pending{}, seen{};
        pending.push_back({left_index, right_index});
        while(!pending.empty())
        {
            auto const current{pending.back_unchecked()};
            pending.pop_back_unchecked();
            if(current.first >= left_signatures.size() || current.second >= right_signatures.size()) { return false; }
            bool visited{};
            for(auto const& prior: seen) { if(prior == current) { visited = true; break; } }
            if(visited) { continue; }
            seen.push_back(current);
            switch(canonical_compare(current.first, current.second))
            {
                case core3_defined_type_comparison::equivalent: continue;
                case core3_defined_type_comparison::different: return false;
                case core3_defined_type_comparison::structural: break;
            }
            // [left/right retained signature arrays] current indices were checked above.
            // [safe                            ] index_unchecked borrows parser-owned records;
            //                                  neither pointer nor guest cursor advances.
            auto const& left{left_signatures.index_unchecked(current.first)};
            auto const& right{right_signatures.index_unchecked(current.second)};
            if(left.parameters.size() != right.parameters.size() || left.results.size() != right.results.size()) { return false; }
            auto const same_value{[&](auto a, auto b) noexcept
            {
                if(a.kind != b.kind) { return false; }
                if(a.kind != kind::reference) { return true; }
                if(a.nullable != b.nullable) { return false; }
                if(a.heap.is_defined() != b.heap.is_defined()) { return false; }
                if(!a.heap.is_defined()) { return a.heap == b.heap; }
                pending.push_back({static_cast<::std::size_t>(a.heap.code), static_cast<::std::size_t>(b.heap.code)});
                return true;
            }};
            for(::std::size_t i{}; i != left.parameters.size(); ++i)
            {
                if(!same_value(left.parameters.index_unchecked(i), right.parameters.index_unchecked(i))) { return false; }
            }
            for(::std::size_t i{}; i != left.results.size(); ++i)
            {
                if(!same_value(left.results.index_unchecked(i), right.results.index_unchecked(i))) { return false; }
            }
        }
        return true;
    }

    template<typename Signatures>
    [[nodiscard]] inline bool core3_value_type_matches(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type actual,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected,
        Signatures const& signatures) noexcept
    {
        using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        using heap = ::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
        if(actual.kind != expected.kind) { return false; }
        if(actual.kind != kind::reference) { return true; }
        if(actual.nullable && !expected.nullable) { return false; }
        if(actual.heap == expected.heap || actual.heap.code == ::uwvm2::parser::wasm::standard::wasm3::type::heap_type::bottom_code)
        { return true; }
        if(actual.heap.code == static_cast<::std::int_least64_t>(heap::nofunc))
        {
            return expected.heap.is_defined() || expected.heap.code == static_cast<::std::int_least64_t>(heap::func);
        }
        if(actual.heap.is_defined() && expected.heap.code == static_cast<::std::int_least64_t>(heap::func)) { return true; }
        if(actual.heap.is_defined() && expected.heap.is_defined())
        {
            return core3_function_types_equivalent(static_cast<::std::size_t>(actual.heap.code),
                                                   static_cast<::std::size_t>(expected.heap.code), signatures);
        }
        return false;
    }

    // Complete contexts retain canonical subtyping for every composite kind;
    // ABI placeholder signatures must not establish aggregate equivalence. A
    // plain 0x60 function-only section may have an empty or absent context when
    // GC is disabled. Abstract heap matching is still Core 3 in that case:
    // noexn <: exn and noextern <: extern are independent of the GC feature gate.
    template<typename Signatures>
    [[nodiscard]] inline bool core3_value_type_matches_with_context(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type actual,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected,
        Signatures const& signatures, recursive_type_context const* context) noexcept
    {
        using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        auto const abstract_pair{actual.kind == kind::reference && expected.kind == kind::reference &&
            !actual.heap.is_defined() && !expected.heap.is_defined()};
        // [retained parser context] compiler owner pins this immutable allocation.
        // [safe                   ] nullptr is rejected before the metadata read;
        // ^^ context is a synchronous borrow, never advanced or retained here.
        if(context != nullptr && (!context->records.empty() || abstract_pair))
        { return context->matches(actual, expected); }
        if(abstract_pair)
        {
            // No defined heap is consulted: an owned empty context uses exactly
            // the shared abstract hierarchy/nullability/Bot rules without allocation.
            recursive_type_context const empty{};
            return empty.matches(actual, expected);
        }
        return core3_value_type_matches(actual, expected, signatures);
    }

    template<typename TypeSection>
    [[nodiscard]] inline bool core3_value_type_matches_in_section(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type actual,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected,
        TypeSection const& section) noexcept
    {
        // section owns both signature/context tables throughout this synchronous
        // validation call; constructing this borrow does not alter any source cursor.
        return core3_value_type_matches_with_context(actual, expected, section.owned_signatures,
            ::std::addressof(section.core3_context));
    }

    template<typename ValueType>
    [[nodiscard]] inline constexpr ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type
        core3_legacy_carrier_type(ValueType carrier) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        using kind = t::value_kind;
        using heap = t::abstract_heap_type;
        switch(static_cast<unsigned>(carrier))
        {
            case 0x7fu: return {kind::i32};
            case 0x7eu: return {kind::i64};
            case 0x7du: return {kind::f32};
            case 0x7cu: return {kind::f64};
            case 0x7bu: return {kind::v128};
            case 0x70u: return {kind::reference, {static_cast<::std::int_least64_t>(heap::func)}, true};
            case 0x6fu: return {kind::reference, {static_cast<::std::int_least64_t>(heap::extern_)}, true};
            case 0x69u: return {kind::reference, {static_cast<::std::int_least64_t>(heap::exn)}, true};
            default: return {kind::i32};
        }
    }

    template<typename Declaration, typename ValueType>
    [[nodiscard]] inline constexpr ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type
        core3_declaration_effective_type(Declaration const& declaration, ValueType carrier) noexcept
    { return declaration.has_core_type ? declaration.core_type : core3_legacy_carrier_type(carrier); }

    template<typename Operand>
    [[nodiscard]] inline constexpr ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type
        core3_operand_effective_type(Operand const& operand) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(operand.has_core_type) { return operand.core_type; }
        // Core 3 pop_ref reifies polymorphic bottom as the non-null reference bottom.
        if(operand.is_reference_bottom) { return {t::value_kind::reference, {t::heap_type::bottom_code}, false}; }
        return core3_legacy_carrier_type(operand.type);
    }
    // One normative call_ref reference check for the pure validator and both
    // fused translators. The caller already decoded and bounded typeidx in
    // THIS instruction walk; no raw bytes, stack copies or second pass occur.
    // Core 3 requires (ref null typeidx), including its declared subtype tree:
    // https://webassembly.github.io/spec/core/valid/instructions.html#valid-call-ref
    // Full type metadata must take precedence over legacy Bot/index witnesses.
    // A carrier or a structurally equal ABI cannot replace canonical subtyping.
    template<typename Operand, typename RichMatches, typename LegacyTypeAt>
    [[nodiscard]] inline constexpr bool core3_call_ref_reference_matches(
        Operand const& reference, ::std::size_t type_index, ::std::size_t type_count,
        bool rich_signature_available, RichMatches const& rich_matches,
        LegacyTypeAt const& legacy_type_at) noexcept
    {
        if(type_index >= type_count) { return false; }
        if(!reference.from_stack || reference.is_unknown) { return true; }
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(rich_signature_available)
        {
            t::core_value_type const expected{
                t::value_kind::reference, {static_cast<::std::int_least64_t>(type_index)}, true};
            return rich_matches(core3_operand_effective_type(reference), expected);
        }
        if(reference.is_reference_bottom) { return true; }
        if(static_cast<unsigned>(reference.type) != 0x70u) { return false; }
        constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
        if(reference.exact_function_type_index == maximum - 1uz) { return true; }
        if(reference.exact_function_type_index >= type_count) { return false; }

        // [retained carrier type records: 0 ... checked indices ... type_count)
        // [safe] Both indices were bounded BEFORE each synchronous provider.
        // The provider borrows immutable parser-owned objects; no guest pointer
        // or bytecode cursor is advanced or retained by this helper.
        auto const& actual{legacy_type_at(reference.exact_function_type_index)};
        auto const& expected{legacy_type_at(type_index)};
        auto const actual_parameters{actual.parameter.begin == actual.parameter.end ? 0uz :
            static_cast<::std::size_t>(actual.parameter.end - actual.parameter.begin)};
        auto const expected_parameters{expected.parameter.begin == expected.parameter.end ? 0uz :
            static_cast<::std::size_t>(expected.parameter.end - expected.parameter.begin)};
        auto const actual_results{actual.result.begin == actual.result.end ? 0uz :
            static_cast<::std::size_t>(actual.result.end - actual.result.begin)};
        auto const expected_results{expected.result.begin == expected.result.end ? 0uz :
            static_cast<::std::size_t>(expected.result.end - expected.result.begin)};
        if(actual_parameters != expected_parameters || actual_results != expected_results) { return false; }
        for(::std::size_t index{}; index != actual_parameters; ++index)
        {
            // [actual/expected parameter begin ... index ... equal checked arity)
            // [safe] index<count bounds both reads before either begin[index].
            if(actual.parameter.begin[index] != expected.parameter.begin[index]) { return false; }
        }
        for(::std::size_t index{}; index != actual_results; ++index)
        {
            // [actual/expected result begin ... index ... equal checked arity)
            // [safe] index<count bounds both reads before either begin[index].
            if(actual.result.begin[index] != expected.result.begin[index]) { return false; }
        }
        return true;
    }

}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
