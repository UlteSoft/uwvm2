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
# include "reference_policy.h"
# include "value_immediate.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3 projects struct/array declarations into the legacy flat carrier table,
    // so a bounded typeidx alone does not prove a callable function signature.
    // The same check covers indexed blocktypes and indirect/reference calls.
    template<typename Name>
    inline constexpr void require_core3_function_type_index_policy(
        recursive_type_context const* context, ::std::size_t type_index,
        ::std::byte const* op_begin, Name const& op_name,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(context == nullptr || context->records.empty()) { return; }
        if(context->contains(type_index) &&
           context->records.index_unchecked(type_index).kind == t3::composite_kind::function) { return; }
        // [caller-saved opcode/prefix] complete typeidx ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced.
        // ^^ op_begin -> err.err_curr: diagnostic borrow only; no input cursor changes.
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = op_name;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // call_indirect and return_call_indirect require a table reference type
    // below (ref null func). A projected 0x70 carrier also represents any/eq
    // and GC aggregates, so carrier equality alone cannot establish this.
    template<typename TypeSection>
    [[nodiscard]] inline bool core3_indirect_call_table_type_matches(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type actual,
        TypeSection const& section) noexcept
    {
        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        t3::core_value_type const expected{t3::value_kind::reference,
            {static_cast<::std::int_least64_t>(t3::abstract_heap_type::func)}, true};
        return core3_value_type_matches_in_section(actual, expected, section);
    }

    // Match the runtime facade's code-version choice without scanning a body.
    // Core 3 uses u64 memarg encoding even for memory32; explicit legacy modes
    // retain their earlier binary grammar in every fused compiler as well.
    template<typename Policy>
    [[nodiscard]] inline constexpr bool uses_core3_validation_policy(Policy const& policy) noexcept
    {
        return policy.cli_mode == decltype(policy.cli_mode)::scoped || !policy.disable_extended_const ||
            !policy.disable_table_initializer || !policy.disable_relaxed_simd || !policy.disable_multi_memory ||
            !policy.disable_threads || !policy.disable_tail_call || !policy.disable_memory64 || !policy.disable_table64 ||
            !policy.disable_function_references || !policy.disable_gc || !policy.disable_exceptions;
    }

    // A compiler can receive a stricter feature policy than the parser/initializer used. Retain
    // declaration encoding requirements through lossless carrier projection and check them before emission.
    template<typename TypeSection>
    [[nodiscard]] inline constexpr bool signatures_require_function_references(TypeSection const& section) noexcept
    {
        return section.requires_function_references;
    }

    // Parser-proven declaration requirements survive runtime projection. This is
    // constant-size host metadata, including unused imports/types and zero-count locals.
    struct core3_exception_declaration_requirements
    {
        bool types{}, tables{}, globals{}, elements{}, locals{}, tags{};
    };
    struct core3_exception_declaration_requirement
    {
        bool required{};
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject{};
        unsigned value{};
    };
    [[nodiscard]] inline constexpr core3_exception_declaration_requirement get_exception_declaration_requirement(
        bool enabled, core3_exception_declaration_requirements requirements) noexcept
    {
        if(enabled) { return {}; }
        using subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
        if(requirements.tags) { return {true, subject::tag_type, 13u}; }
        if(requirements.types) { return {true, subject::function_type, 0x69u}; }
        if(requirements.tables) { return {true, subject::table_type, 0x69u}; }
        if(requirements.globals) { return {true, subject::global_type, 0x69u}; }
        if(requirements.elements) { return {true, subject::element_segment, 0x69u}; }
        if(requirements.locals) { return {true, subject::local_type, 0x69u}; }
        return {};
    }
    inline constexpr void require_exception_declaration_policy(bool enabled,
        core3_exception_declaration_requirements requirements, ::std::byte const* diagnostic,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_exception_declaration_requirement(enabled, requirements)};
        if(!required.required) { return; }
        // [caller-proven body bytes ...] body_end, or null for a module without bodies.
        // [safe                      ] diagnostic is only borrowed, never advanced or dereferenced.
        // ^^ err_curr: an endpoint/null diagnostic does not grant a bytecode read.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = required.value, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
            .subject = required.subject};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Exact encoded typed-reference requirements are retained by the actual
    // declaration decoder. Abstract GC/exn heaps do not imply this feature.
    struct core3_function_reference_declaration_requirements
    {
        bool types{}, tables{}, globals{}, elements{};
    };
    struct core3_function_reference_declaration_requirement
    {
        bool required{};
        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature{};
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject{};
    };
    [[nodiscard]] inline constexpr core3_function_reference_declaration_requirement
        get_function_reference_declaration_requirement(bool function_references_enabled, bool reference_types_enabled,
            core3_function_reference_declaration_requirements requirements) noexcept
    {
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
        if(!function_references_enabled || !reference_types_enabled)
        {
            auto const required{!function_references_enabled ? feature::function_references : feature::reference_types};
            if(requirements.tables) { return {true, required, subject::table_type}; }
            if(requirements.globals) { return {true, required, subject::global_type}; }
            if(requirements.elements) { return {true, required, subject::element_segment}; }
        }
        if(!function_references_enabled && requirements.types)
        { return {true, feature::function_references, subject::function_type}; }
        return {};
    }
    inline constexpr void require_function_reference_declaration_policy(bool function_references_enabled,
        bool reference_types_enabled, core3_function_reference_declaration_requirements requirements,
        ::std::byte const* diagnostic, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_function_reference_declaration_requirement(
            function_references_enabled, reference_types_enabled, requirements)};
        if(!required.required) { return; }
        // [caller-proven module/body span ...] end, or null for a module without bodies.
        // [safe                              ] diagnostic borrow only; never advanced or dereferenced.
        // ^^ err_curr: the actual cold formatter bounds this address against the real module owner.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = 0x63u, .feature = required.feature, .subject = required.subject};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // The declarations admitted here need only these proposal controls. Keep
    // the same constant early return in pure and every fused compiler entrance.
    template<typename Policy>
    [[nodiscard]] inline constexpr bool core3_value_declaration_policy_fully_enabled(Policy const& policy) noexcept
    {
        return !policy.disable_gc && !policy.disable_exceptions && !policy.disable_function_references &&
            !policy.disable_reference_types && !policy.disable_simd && !policy.disable_multi_value &&
            !policy.controllable_allow_multi_result_vector;
    }
    template<typename Policy>
    [[nodiscard]] inline constexpr bool core3_declaration_policy_fully_enabled(Policy const& policy) noexcept
    {
        return core3_value_declaration_policy_fully_enabled(policy) && !policy.disable_memory64 &&
            !policy.disable_table64 && !policy.disable_threads && !policy.disable_multi_memory &&
            !policy.disable_table_initializer && !policy.disable_extended_const;
    }
    struct core3_type_declaration_requirements
    {
        bool simd{}, reference_types{}, multi_value{};
    };
    struct core3_type_declaration_requirement
    {
        bool required{};
        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature{};
        unsigned value{};
    };
    [[nodiscard]] inline constexpr core3_type_declaration_requirement get_type_declaration_requirement(
        bool reference_types_enabled, bool simd_enabled, bool multi_value_enabled,
        core3_type_declaration_requirements requirements) noexcept
    {
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        // One canonical cold priority for every unused/used type declaration.
        // Aggregate fields and function parameters/results share this rule.
        if(!reference_types_enabled && requirements.reference_types) { return {true, feature::reference_types, 0x70u}; }
        if(!simd_enabled && requirements.simd) { return {true, feature::simd, 0x7bu}; }
        if(!multi_value_enabled && requirements.multi_value) { return {true, feature::multi_value, 0x60u}; }
        return {};
    }
    inline constexpr void require_type_declaration_policy(bool reference_types_enabled, bool simd_enabled,
        bool multi_value_enabled, core3_type_declaration_requirements requirements, ::std::byte const* diagnostic,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_type_declaration_requirement(
            reference_types_enabled, simd_enabled, multi_value_enabled, requirements)};
        if(!required.required) { return; }
        // [caller-proven module/body span ...] end, or null for zero-body runtime metadata.
        // [safe                              ] diagnostic copy only, never advanced/read.
        // ^^ err_curr: actual cold consumers bound this borrowed address against the source owner.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = required.value, .feature = required.feature,
            .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::function_type};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    struct core3_storage_declaration_requirements
    {
        bool tables_reference_types{}, globals_reference_types{}, elements_reference_types{}, globals_simd{};
        unsigned table_reference_value{}, global_reference_value{}, element_reference_value{};
    };
    struct core3_storage_declaration_requirement
    {
        bool required{};
        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature{};
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject{};
        unsigned value{};
    };
    [[nodiscard]] inline constexpr core3_storage_declaration_requirement get_storage_declaration_requirement(
        bool reference_types_enabled, bool simd_enabled, core3_storage_declaration_requirements requirements) noexcept
    {
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
        // A legacy short-form funcref TABLE needs no reference-types proposal.
        // References in globals and expression-form element segments do. Bits
        // and the first real required encoding were collected in existing parsers.
        if(!reference_types_enabled)
        {
            if(requirements.tables_reference_types)
            { return {true, feature::reference_types, subject::table_type, requirements.table_reference_value}; }
            if(requirements.globals_reference_types)
            { return {true, feature::reference_types, subject::global_type, requirements.global_reference_value}; }
            if(requirements.elements_reference_types)
            { return {true, feature::reference_types, subject::element_segment, requirements.element_reference_value}; }
        }
        if(!simd_enabled && requirements.globals_simd)
        { return {true, feature::simd, subject::global_type, 0x7bu}; }
        return {};
    }
    inline constexpr void require_storage_declaration_policy(bool reference_types_enabled, bool simd_enabled,
        core3_storage_declaration_requirements requirements, ::std::byte const* diagnostic,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_storage_declaration_requirement(reference_types_enabled, simd_enabled, requirements)};
        if(!required.required) { return; }
        // [caller-proven module/body span ...] end, or null for zero-body module metadata.
        // [safe                              ] diagnostic borrow only; no pointer advance or byte read.
        // ^^ err_curr: existing cold consumers normalize/bound against the actual byte owner.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = required.value, .feature = required.feature, .subject = required.subject};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    struct core3_address_declaration_requirements
    { bool memory64{}, table64{}, shared{}, multi_memory{}; };
    [[nodiscard]] inline constexpr bool core3_has_multiple_declarations(::std::size_t imported, ::std::size_t local) noexcept
    {
        // Counts already belong to the actual decoded vectors. No addition,
        // narrowing, pointer selection, second declaration traversal or body scan.
        return imported > 1uz || local > 1uz || (imported == 1uz && local == 1uz);
    }
    [[nodiscard]] inline constexpr core3_storage_declaration_requirement get_address_declaration_requirement(
        bool memory64_enabled, bool table64_enabled, bool threads_enabled, bool multi_memory_enabled,
        core3_address_declaration_requirements requirements) noexcept
    {
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
        if(!memory64_enabled && requirements.memory64)
        { return {true, feature::memory64, subject::memory_type, 4u}; }
        if(!table64_enabled && requirements.table64)
        { return {true, feature::table64, subject::table_type, 4u}; }
        if(!threads_enabled && requirements.shared)
        { return {true, feature::threads, subject::memory_type, 3u}; }
        if(!multi_memory_enabled && requirements.multi_memory)
        { return {true, feature::multi_memory, subject::memory_type, 2u}; }
        return {};
    }
    inline constexpr void require_address_declaration_policy(bool memory64_enabled, bool table64_enabled,
        bool threads_enabled, bool multi_memory_enabled, core3_address_declaration_requirements requirements,
        ::std::byte const* diagnostic, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_address_declaration_requirement(memory64_enabled, table64_enabled,
            threads_enabled, multi_memory_enabled, requirements)};
        if(!required.required) { return; }
        // [caller-proven actual module/body allocation ...] end, or null runtime diagnostic.
        // [safe                                           ] borrow only; no pointer arithmetic or byte read.
        // ^^ err_curr: the existing owner-bounded cold formatter normalizes this borrow.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = required.value, .feature = required.feature, .subject = required.subject};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Module admission must also run when no function body exists. All these
    // bits come from successful parser declaration decoders, not guest bytes.
    // Aggregate over the original successful typed constexpr decoders. A
    // table initializer belongs only to a definition; extended_const can occur
    // in globals, table initializers, element values, or active data/element offsets.
    // Constant instruction requirements retain their originating opcode and
    // expression site independently of declaration/result-type requirements.
    // All callers use the same fixed scalar selector; fully-enabled module
    // admission returns before reaching it and no opcode is decoded twice.
    template<typename Policy>
    [[nodiscard]] inline constexpr core3_storage_declaration_requirement get_constant_opcode_requirement(
        Policy const& policy, ::uwvm2::parser::wasm::base::constant_expression_opcode_requirements const& requirements) noexcept
    {
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        // ref.null/ref.func first require reference-types, then the independently
        // classified GC/EH/indexed-function heap family. Declaration admission
        // remains ahead of this instruction-origin selector in every facade.
        if(policy.disable_reference_types && requirements.reference_types.required)
        { return {true, feature::reference_types, requirements.reference_types.subject, requirements.reference_types.value}; }
        if(policy.disable_gc && requirements.gc.required)
        { return {true, feature::gc, requirements.gc.subject, requirements.gc.value}; }
        if(policy.disable_exceptions && requirements.exceptions.required)
        { return {true, feature::exceptions, requirements.exceptions.subject, requirements.exceptions.value}; }
        if(policy.disable_function_references && requirements.function_references.required)
        { return {true, feature::function_references, requirements.function_references.subject, requirements.function_references.value}; }
        if(policy.disable_simd && requirements.simd.required)
        { return {true, feature::simd, requirements.simd.subject, requirements.simd.value}; }
        return {};
    }
    template<typename Policy>
    inline constexpr void require_constant_opcode_policy(Policy const& policy,
        ::uwvm2::parser::wasm::base::constant_expression_opcode_requirements const& requirements,
        ::std::byte const* diagnostic, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_constant_opcode_requirement(policy, requirements)};
        if(!required.required) { return; }
        // [caller-proven actual source/body allocation ...] end, or null module diagnostic.
        // [safe                                          ] copy only; no read or pointer advance.
        // ^^ err_curr: the existing cold formatter bounds/normalizes this actual owner borrow.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = required.value, .feature = required.feature, .subject = required.subject};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    struct core3_constant_declaration_requirements
    {
        bool table_initializer{}, extended_const{};
        unsigned extended_const_value{};
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject extended_const_subject{
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::global_type};
        ::uwvm2::parser::wasm::base::constant_expression_opcode_requirements opcodes{};
    };
    [[nodiscard]] inline constexpr core3_storage_declaration_requirement get_constant_declaration_requirement(
        bool table_initializer_enabled, bool extended_const_enabled,
        core3_constant_declaration_requirements requirements) noexcept
    {
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
        if(!table_initializer_enabled && requirements.table_initializer)
        { return {true, feature::table_initializer, subject::table_type, 0x40u}; }
        if(!extended_const_enabled && requirements.extended_const)
        { return {true, feature::extended_const, requirements.extended_const_subject, requirements.extended_const_value}; }
        return {};
    }
    inline constexpr void require_constant_declaration_policy(bool table_initializer_enabled, bool extended_const_enabled,
        core3_constant_declaration_requirements requirements, ::std::byte const* diagnostic,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        auto const required{get_constant_declaration_requirement(table_initializer_enabled, extended_const_enabled, requirements)};
        if(!required.required) { return; }
        // [caller-proven actual module/body allocation ...] end, or null runtime diagnostic.
        // [safe                                           ] borrow only; no pointer arithmetic or byte read.
        // ^^ err_curr: the existing owner-bounded cold formatter normalizes this borrow.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = required.value, .feature = required.feature, .subject = required.subject};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    struct core3_gc_declaration_requirements
    {
        bool types{}, tables{}, globals{}, elements{};
    };
    inline constexpr void require_gc_type_declaration_policy(bool enabled, bool required,
        ::std::byte const* diagnostic, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled || !required) { return; }
        // [caller-proven module/body span ...] end, or null for a runtime-only module diagnostic.
        // [safe                              ] borrow only; no byte is read or pointer advanced.
        // ^^ err_curr: cold formatters must normalize a null diagnostic against the real module owner.
        err.err_curr = diagnostic;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
        err.err_selectable.wasm1p1_feature_required = {
            .value = 0x4eu, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
            .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::function_type};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    template<typename Policy>
    inline constexpr void require_module_declaration_policy(Policy const& policy,
        core3_gc_declaration_requirements gc, core3_exception_declaration_requirements exceptions,
        ::std::byte const* diagnostic, ::uwvm2::validation::error::code_validation_error_impl& err,
        core3_function_reference_declaration_requirements function_references = {},
        core3_type_declaration_requirements types = {},
        core3_storage_declaration_requirements storage = {},
        core3_address_declaration_requirements address = {},
        core3_constant_declaration_requirements constants = {}) UWVM_THROWS
    {
        if(core3_declaration_policy_fully_enabled(policy)) { return; }
        // Preserve the existing pure/fused per-function order: encoded GC type,
        // EH declarations, then GC table/global/element declarations.
        require_gc_type_declaration_policy(!policy.disable_gc, gc.types, diagnostic, err);
        require_exception_declaration_policy(!policy.disable_exceptions, exceptions, diagnostic, err);
        if(policy.disable_gc)
        {
            using site = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
            auto fail = [&](site subject) constexpr UWVM_THROWS
            {
                // [caller-proven module/body span ...] end, or null runtime diagnostic.
                // [safe                              ] diagnostic copy only; no pointer arithmetic/read.
                // ^^ err_curr: preserve the same source position for every declaration subject.
                err.err_curr = diagnostic;
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
                err.err_selectable.wasm1p1_feature_required = {
                    .value = 0x63u, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                    .subject = subject};
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            };
            if(gc.tables) { fail(site::table_type); }
            if(gc.globals) { fail(site::global_type); }
            if(gc.elements) { fail(site::element_segment); }
        }
        // Preserve the fused per-function typed declaration order after GC/EH admission.
        require_function_reference_declaration_policy(!policy.disable_function_references,
            !policy.disable_reference_types, function_references, diagnostic, err);
        require_type_declaration_policy(!policy.disable_reference_types, !policy.disable_simd,
            !(policy.disable_multi_value || policy.controllable_allow_multi_result_vector), types, diagnostic, err);
        require_storage_declaration_policy(!policy.disable_reference_types, !policy.disable_simd, storage, diagnostic, err);
        require_address_declaration_policy(!policy.disable_memory64, !policy.disable_table64,
            !policy.disable_threads, !policy.disable_multi_memory, address, diagnostic, err);
        require_constant_declaration_policy(!policy.disable_table_initializer, !policy.disable_extended_const,
            constants, diagnostic, err);
        require_constant_opcode_policy(policy, constants.opcodes, diagnostic, err);
        // A local declaration belongs to an actual code body, even for count 0.
        // Per-function typed validation already checks its exact type in that pass.
        // A module with zero bodies cannot contain encoded local declarations.
    }

    template<typename Signature, typename Locals, typename Policy>
    inline constexpr void require_function_declaration_policy(Signature const& signature, Locals const& locals,
        bool explicit_signatures, Policy const& policy, ::std::byte const* diagnostic,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        bool explicit_tables = false, bool explicit_globals = false, bool explicit_elements = false, bool has_tag_section = false,
        recursive_type_context const* core_context = nullptr, ::std::size_t known_type_count = 0uz,
        bool gc_tables = false, bool gc_globals = false, bool gc_elements = false,
        core3_exception_declaration_requirements exception_declarations = {},
        core3_type_declaration_requirements type_declarations = {},
        core3_storage_declaration_requirements storage_declarations = {},
        core3_address_declaration_requirements address_declarations = {},
        core3_constant_declaration_requirements constant_declarations = {}) UWVM_THROWS
    {
        // The pre-split COP flag is a compatibility disable, matching the parser/initializer's Core 2 policy.
        bool const multi_value_disabled{policy.disable_multi_value || policy.controllable_allow_multi_result_vector};
        if(core3_declaration_policy_fully_enabled(policy))
        { return; } // Common fully enabled policy: no declaration walks and no guest-code changes.
        using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
        using subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
        auto fail = [&](feature required, subject site, unsigned value) constexpr UWVM_THROWS
        {
            // [function body ...] body_end
            // [safe             ] diagnostic borrows a caller-proven address, possibly exactly body_end.
            // ^^ err_curr: diagnostics do not read an opcode or dereference this address.
            err.err_curr = diagnostic;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = value, .feature = required, .subject = site};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        };
        exception_declarations.tags |= has_tag_section;
        // The caller has already admitted the module's encoded GC type bit.
        // Reuse precisely the same declaration semantics as zero-body module admission.
        require_module_declaration_policy(policy,
            {.tables = gc_tables, .globals = gc_globals, .elements = gc_elements},
            exception_declarations, diagnostic, err,
            {.types = explicit_signatures, .tables = explicit_tables,
             .globals = explicit_globals, .elements = explicit_elements}, type_declarations, storage_declarations, address_declarations, constant_declarations);
        // Only address/shared/count/constant-expression switches disabled: after their constant module
        // gate, do not turn the existing signature/local checks into another walk.
        if(core3_value_declaration_policy_fully_enabled(policy)) { return; }
        auto check_value = [&](auto value, subject site) constexpr UWVM_THROWS
        {
            auto const carrier{static_cast<unsigned>(value)};
            if(policy.disable_reference_types && (carrier == 0x70u || carrier == 0x6fu || carrier == 0x69u))
            { fail(feature::reference_types, site, carrier); }
            if(policy.disable_simd && carrier == 0x7bu) { fail(feature::simd, site, carrier); }
            // exn/noexn rich heaps both retain the 0x69 carrier. They need the
            // exception policy even without a tag declaration or any throw.
            if(policy.disable_exceptions && carrier == 0x69u) { fail(feature::exceptions, site, carrier); }
        };
        auto check_range = [&](auto range) constexpr UWVM_THROWS
        {
            // [begin ... end) is a parser/initializer-proven carrier allocation; an empty range may be null.
            // ^^ curr borrows begin without arithmetic. Each != end check proves a live element.
            for(auto curr{range.begin}; curr != range.end;)
            {
                check_value(*curr, subject::function_type);
                // [visited carriers][current] next ... end
                // [safe                     ] current is live; +1 stays inside the allocation or becomes end.
                ++curr;
                // [visited carriers] next ... end
                // [safe            ] ^^ curr, possibly end; the next iteration checks before reading.
            }
        };
        if(policy.disable_reference_types || policy.disable_simd || policy.disable_exceptions)
        { check_range(signature.parameter); check_range(signature.result); }
        // Empty endpoints can both be null. Subtract only endpoints of a nonempty, proven allocation.
        auto const results{signature.result.begin == signature.result.end ? 0uz :
            static_cast<::std::size_t>(signature.result.end - signature.result.begin)};
        if(multi_value_disabled && results > 1uz) { fail(feature::multi_value, subject::function_type, 0x60u); }
        for(auto const& local : locals)
        {
            if(policy.disable_function_references && local.requires_function_references)
            { fail(feature::function_references, subject::local_type, 0x63u); }
            if(policy.disable_gc && local.has_core_type &&
               core3_value_requires_gc(local.core_type, core_context, known_type_count))
            { fail(feature::gc, subject::local_type, local.core_type.source_prefix); }
            // The encoded type of a zero-count run is still subject to the feature policy.
            check_value(local.type, subject::local_type);
        }
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
