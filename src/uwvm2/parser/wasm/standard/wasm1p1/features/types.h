/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Extended constant-expression FP literals use the same raw-storage contract as
// wasm1 global initializers: activate f32/f64 storage, then copy decoded integer
// bits into it. An intermediate Float-return bit_cast/helper can quiet an sNaN
// on GCC -O0/i386 or 68881 before either interpreter or JIT sees the expression.
// This is bit transport, so arithmetic NaN canonicalization would also be wrong.
// See documents/runtime/floating-point-change-rationale.md.

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     antecedent dependency: WebAssembly Release 1.0 (2019-07-20)
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <bit>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/validation/standard/wasm3/function_signature.h>
# include <uwvm2/validation/standard/wasm3/heap_immediate.h>
# include <uwvm2/validation/standard/wasm3/value_immediate.h>
# include <uwvm2/validation/standard/wasm3/recursive_type_validation.h>
# include "def.h"
# include "feature_def.h"
# include <uwvm2/validation/standard/wasm3/constant_expression.h>
# include <uwvm2/validation/standard/wasm3/address_limits.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    namespace core3_initializer_type_details
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        template<typename Signatures>
        [[nodiscard]] inline constexpr bool equivalent_function_types(
            ::std::size_t first, ::std::size_t second, Signatures const& signatures) noexcept
        {
            using pair = ::std::pair<::std::size_t, ::std::size_t>;
            ::uwvm2::utils::container::vector<pair> pending{}, seen{};
            pending.push_back({first, second});
            while(!pending.empty())
            {
                auto const current{pending.back_unchecked()};
                pending.pop_back_unchecked();
                if(current.first >= signatures.size() || current.second >= signatures.size()) { return false; }
                if(current.first == current.second) { continue; }
                bool visited{};
                for(auto const& item: seen) { if(item == current) { visited = true; break; } }
                if(visited) { continue; }
                seen.push_back(current);
                auto const& left{signatures.index_unchecked(current.first)};
                auto const& right{signatures.index_unchecked(current.second)};
                if(left.parameters.size() != right.parameters.size() || left.results.size() != right.results.size()) { return false; }
                auto const compare_value{[&](t::core_value_type a, t::core_value_type b) noexcept
                {
                    if(a.kind != b.kind) { return false; }
                    if(a.kind != t::value_kind::reference) { return true; }
                    if(a.nullable != b.nullable) { return false; }
                    if(a.heap == b.heap) { return true; }
                    if(!a.heap.is_defined() || !b.heap.is_defined()) { return false; }
                    pending.push_back({static_cast<::std::size_t>(a.heap.code), static_cast<::std::size_t>(b.heap.code)});
                    return true;
                }};
                for(::std::size_t i{}; i != left.parameters.size(); ++i)
                { if(!compare_value(left.parameters.index_unchecked(i), right.parameters.index_unchecked(i))) { return false; } }
                for(::std::size_t i{}; i != left.results.size(); ++i)
                { if(!compare_value(left.results.index_unchecked(i), right.results.index_unchecked(i))) { return false; } }
            }
            return true;
        }

        template<typename Signatures>
        [[nodiscard]] inline constexpr bool reference_matches(t::core_value_type actual, t::core_value_type expected,
            Signatures const& signatures,
            ::uwvm2::validation::standard::wasm3::recursive_type_context const* context = nullptr) noexcept
        {
            using heap = t::abstract_heap_type;
            if(actual.kind != t::value_kind::reference || expected.kind != t::value_kind::reference) { return false; }
            // The validated context knows every Core 3 heap hierarchy and nominal group.
            // The old signature graph fallback below is only for legacy function-only tables.
            if(context != nullptr && (!context->records.empty() ||
               (!actual.heap.is_defined() && !expected.heap.is_defined())))
            { return context->matches(actual, expected); }
            if(actual.nullable && !expected.nullable) { return false; }
            if(actual.heap == expected.heap) { return true; }
            if(actual.heap.code == static_cast<::std::int_least64_t>(heap::nofunc))
            { return expected.heap.is_defined() || expected.heap.code == static_cast<::std::int_least64_t>(heap::func); }
            if(actual.heap.is_defined() && expected.heap.code == static_cast<::std::int_least64_t>(heap::func)) { return true; }
            if(actual.heap.is_defined() && expected.heap.is_defined())
            {
                return equivalent_function_types(static_cast<::std::size_t>(actual.heap.code),
                    static_cast<::std::size_t>(expected.heap.code), signatures);
            }
            return false;
        }

        [[nodiscard]] inline constexpr t::core_value_type declared_reference_type(
            ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type const& declaration) noexcept
        {
            if(declaration.has_core_type) { return declaration.core_type; }
            using value = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
            return {t::value_kind::reference,
                {declaration.type == value::funcref ? static_cast<::std::int_least64_t>(t::abstract_heap_type::func) :
                    declaration.type == static_cast<value>(0x69u) ? static_cast<::std::int_least64_t>(t::abstract_heap_type::exn) :
                    static_cast<::std::int_least64_t>(t::abstract_heap_type::extern_)}, true};
        }

        [[nodiscard]] inline constexpr t::core_value_type declared_value_type(
            ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type const& declaration) noexcept
        {
            if(declaration.has_core_type) { return declaration.core_type; }
            switch(static_cast<unsigned>(declaration.type))
            {
                case 0x7fu: return {t::value_kind::i32};
                case 0x7eu: return {t::value_kind::i64};
                case 0x7du: return {t::value_kind::f32};
                case 0x7cu: return {t::value_kind::f64};
                case 0x7bu: return {t::value_kind::v128};
                default: return declared_reference_type(declaration);
            }
        }
    }

    // Constant expressions share the same signed-33 heap decoder as both integrated compilers.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::core3_ref_null_result
    parse_core3_ref_null_heap(::std::byte const*& cursor, ::std::byte const* end,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters,
        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        namespace w3 = ::uwvm2::validation::standard::wasm3;
        using error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        using section = ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>;
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<section>(module.sections)};
        auto const& policy{get_wasm1p1_parameter(parameters)};
        // [ref.null] heap ... end
        // [safe    ] unsafe (could be end)
        //            ^^ begin borrows cursor; the decoder checks and commits every advance.
        auto const begin{cursor};
        auto result{w3::scan_core3_ref_null_heap(cursor, end, typesec.core3_context,
            !policy.disable_gc, !policy.disable_function_references, !policy.disable_exceptions)};
        // With GC disabled, plain 0x60 function types use the legacy parser and
        // do not populate the mixed-kind context. Only that function-only table
        // may satisfy an indexed ref.null; the indexed bound and the independent
        // function-references gate are checked by the same bounded decoder used
        // for function-body immediates. A GC type table never takes this path.
        if(result.error == w3::core3_ref_null_error::unknown_type && policy.disable_gc &&
           typesec.core3_context.records.empty() && typesec.core3_type_kinds.empty() &&
           result.heap.is_defined() &&
           static_cast<::std::uint_least64_t>(result.heap.code) < typesec.types.size())
        {
            // [ref.null] heap ... end
            // [safe    ] unsafe (could be end)
            //            ^^ cursor is still begin: the mixed-kind decoder made no
            //               pointer move on failure. The fallback commits only a
            //               complete, in-range signed-33 heap immediate.
            auto const function_result{w3::scan_function_ref_null_heap(cursor, end,
                !policy.disable_function_references, typesec.types.size())};
            // [ref.null][checked heap] next ... end on success; otherwise cursor == begin.
            // [safe                 ] unsafe (could be end)
            //                         ^^ cursor: no unchecked increment occurs here.
            if(function_result.error == w3::function_heap_immediate_error::ok)
            { result = {w3::core3_ref_null_error::ok, function_result.heap, 0uz, function_result.carrier}; }
            else if(function_result.error == w3::function_heap_immediate_error::function_references_disabled)
            { result.error = w3::core3_ref_null_error::function_references_disabled; }
        }
        // [ref.null][checked heap] next ... end on success; otherwise cursor remains begin.
        // [safe                 ] unsafe (could be end)
        //                         ^^ cursor
        if(result.error == w3::core3_ref_null_error::ok) { return result; }
        // [begin ... error offset ...] end
        // [safe                     ] error_offset <= end - begin; the diagnostic may point one-past.
        //            ^^ err_curr: no dereference.
        err.err_curr = begin + result.error_offset;
        if(result.error == w3::core3_ref_null_error::function_references_disabled ||
           result.error == w3::core3_ref_null_error::gc_disabled ||
           result.error == w3::core3_ref_null_error::exceptions_disabled)
        {
            err.err_code = error::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = 0xd0u,
                .feature = result.error == w3::core3_ref_null_error::gc_disabled ?
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc :
                    result.error == w3::core3_ref_null_error::exceptions_disabled ?
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions :
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null};
        }
        else if(result.error == w3::core3_ref_null_error::unknown_type)
        { err.err_code = error::illegal_type_index; err.err_selectable.u32 = static_cast<::std::uint_least32_t>(result.heap.code); }
        else
        {
            err.err_code = error::wasm1p1_invalid_reference_type;
            err.err_selectable.wasm1p1_reference_type = {.value = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(begin == end ? 0u : ::std::to_integer<unsigned>(*begin)),
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null};
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Element initializers still use the function/externref-only projection. Keep
    // this decoder separate from the full Core 3 global initializer above until
    // each element segment carries its rich reference type through the table ABI.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    [[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::function_heap_immediate_result
    parse_function_ref_null_heap(::std::byte const*& cursor, ::std::byte const* end,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters,
        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        namespace w3 = ::uwvm2::validation::standard::wasm3;
        using error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        using section = ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>;
        auto const& types{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<section>(module.sections).types};
        // [ref.null] heap ... end
        // [safe    ] unsafe (possibly end)
        //            ^^ begin borrows the original cursor; decoder commits only on full success.
        auto const begin{cursor};
        auto const& policy{get_wasm1p1_parameter(parameters)};
        auto const result{w3::scan_function_ref_null_heap(cursor, end,
            !policy.disable_function_references, types.size(), false, !policy.disable_gc)};
        // [ref.null][checked heap] next ... end on success; cursor unchanged on failure.
        // [safe                 ] unsafe (possibly end)
        //                         ^^ cursor
        if(result.error == w3::function_heap_immediate_error::ok) { return result; }
        // [begin ... error offset ...] end
        // [safe                     ] ^^ err_curr may be exactly one-past and is never dereferenced.
        err.err_curr = begin + result.error_offset;
        if(result.error == w3::function_heap_immediate_error::function_references_disabled)
        {
            err.err_code = error::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = 0xd0u,
                .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null};
        }
        else if(result.error == w3::function_heap_immediate_error::gc_disabled)
        {
            err.err_code = error::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = 0xd0u,
                .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null};
        }
        else if(result.error == w3::function_heap_immediate_error::unknown_type)
        { err.err_code = error::illegal_type_index; err.err_selectable.u32 = static_cast<::std::uint_least32_t>(result.heap.code); }
        else
        {
            err.err_code = error::wasm1p1_invalid_reference_type;
            err.err_selectable.wasm1p1_reference_type = {.value = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(begin == end ? 0u : ::std::to_integer<unsigned>(*begin)),
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null};
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    // The GC path parses the complete type section before publishing any ABI projection.
    // This keeps every flat type index aligned with its true Core 3 composite kind, including
    // recursive groups and aggregate definitions that are not function signatures.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...) &&
                 ::std::same_as<final_type_type_t<Fs...>, final_function_type<Fs...>>)
    inline constexpr bool define_parse_core3_complete_type_section(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* const section_begin, ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        namespace base = ::uwvm2::parser::wasm::base;
        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        namespace v3 = ::uwvm2::validation::standard::wasm3;
        auto const& options{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(parameters)};
        if(options.disable_gc)
        {
            // Most pre-GC type sections contain none of the Core 3 composite/group prefixes.
            // Keep their legacy parser path allocation-free; inspect a candidate only with the
            // bounded Core 3 syntax decoder so bytes inside a signature cannot impersonate a type.
            // [section_begin ... section_end) is the caller-proven type-section allocation.
            // [safe                         ] unsafe (section_end is one-past)
            // ^^ section_begin; the equal-endpoint case avoids subtracting null pointers.
            auto const section_size{section_begin == section_end ? 0uz :
                static_cast<::std::size_t>(section_end - section_begin)};
            bool possible_gc_prefix{};
            for(::std::size_t offset{}; offset != section_size; ++offset)
            {
                // [already inspected][one checked byte][remaining bytes] section_end
                // [safe             ][safe            ][safe          ] unsafe (one-past)
                //                  ^^ section_begin + offset; offset < section_size dominates this read.
                auto const byte{::std::to_integer<unsigned>(section_begin[offset])};
                possible_gc_prefix |= byte == 0x4eu || byte == 0x4fu || byte == 0x50u ||
                                      byte == 0x5eu || byte == 0x5fu;
            }
            if(!possible_gc_prefix) { return false; }

            t3::recursive_type_section disabled_definitions{};
            // [section_begin ... section_end) borrowed by the bounded scanner.
            // [safe                         ] unsafe (section_end is one-past)
            // ^^ disabled_probe starts at section_begin and is committed only on complete success.
            auto disabled_probe{section_begin};
            if(v3::scan_core3_type_section(disabled_probe, section_end, disabled_definitions).error !=
               v3::recursive_type_binary_error::ok) { return false; }
            // [complete type section] section_end
            // [safe                ] unsafe (section_end is one-past)
            //                      ^^ disabled_probe after successful scan; never dereferenced.

            for(auto const& group : disabled_definitions.groups)
            {
                auto report_offset{group.binary_offset};
                // The decoder recorded every group/subtype offset before a checked prefix read.
                if(report_offset >= section_size) { continue; }
                // [section_begin ... checked prefix ... section_end)
                // [safe                              ] unsafe (one-past)
                //                    ^^ section_begin + report_offset; checked above.
                auto prefix{::std::to_integer<unsigned>(section_begin[report_offset])};
                bool requires_gc{prefix == 0x4eu};
                if(!requires_gc)
                {
                    for(auto const& definition : group.types)
                    {
                        if(definition.binary_offset >= section_size) { continue; }
                        // A direct 0x60 remains available without GC; an explicit 0x4f/0x50
                        // subtype wrapper requires GC even when its supertype list is empty.
                        if(definition.kind == t3::composite_kind::function &&
                           section_begin[definition.binary_offset] == ::std::byte{0x60}) { continue; }
                        report_offset = definition.binary_offset;
                        // [section_begin ... checked subtype prefix ... section_end)
                        // [safe                                      ] unsafe (one-past)
                        //                    ^^ section_begin + report_offset; checked above.
                        prefix = ::std::to_integer<unsigned>(section_begin[report_offset]);
                        requires_gc = true;
                        break;
                    }
                }
                if(!requires_gc) { continue; }
                // [section_begin ... checked Core 3 prefix ... section_end)
                // [safe                                          ] unsafe (one-past)
                //                    ^^ err_curr: report_offset < section_size; diagnostic only.
                err.err_curr = section_begin + report_offset;
                err.err_code = base::wasm_parse_error_code::wasm1p1_feature_required;
                err.err_selectable.wasm1p1_feature_required = {
                    .value = static_cast<::std::uint_least8_t>(prefix),
                    .feature = base::wasm1p1_feature_kind::gc,
                    .subject = base::wasm1p1_error_subject::function_type};
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            return false;
        }
        auto& section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        t3::recursive_type_section definitions{};
        // [section_begin ... section_end) is the caller-proven type-section allocation.
        // [safe                         ] unsafe (section_end is one-past)
        // ^^ probe borrows section_begin; the decoder commits only after all bounded reads.
        auto probe{section_begin};
        auto const decoded{v3::scan_core3_type_section(probe, section_end, definitions)};
        // [complete decoded section] section_end on success; probe remains section_begin on failure.
        // [safe                    ] unsafe (section_end is one-past)
        //                          ^^ probe: this local cursor is never dereferenced after the scan.
        if(decoded.error != v3::recursive_type_binary_error::ok) [[unlikely]]
        {
            // [section_begin ... error_offset ... section_end]
            // [safe                              ] unsafe (possibly section_end)
            //                    ^^ err_curr: decoder bounds error_offset by the section length.
            err.err_curr = section_begin == section_end ? section_begin : section_begin + decoded.error_offset;
            err.err_code = decoded.error == v3::recursive_type_binary_error::impossible_count ?
                base::wasm_parse_error_code::invalid_type_count :
                decoded.error == v3::recursive_type_binary_error::invalid_value_type ||
                decoded.error == v3::recursive_type_binary_error::invalid_heap_type ?
                    base::wasm_parse_error_code::illegal_value_type : base::wasm_parse_error_code::illegal_type_prefix;
            err.err_selectable.u8 = 0u;
            base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        v3::recursive_type_context context{};
        auto const valid{v3::validate_core3_type_section(definitions, context)};
        if(valid.error != v3::recursive_type_validation_error::ok) [[unlikely]]
        {
            // [section_begin ... invalid subtype offset ... section_end]
            // [safe                                         ] unsafe (possibly section_end)
            //                    ^^ err_curr: each subtype offset was captured by the bounded decoder.
            err.err_curr = section_begin == section_end ? section_begin : section_begin + valid.binary_offset;
            err.err_code = base::wasm_parse_error_code::illegal_type_index;
            err.err_selectable.u32 = static_cast<::std::uint_least32_t>(valid.type_index);
            base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& limit{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(parameters).parser_limit};
            if(definitions.groups.size() > limit.max_type_sec_types ||
               definitions.type_count > limit.max_type_sec_types) [[unlikely]]
            {
                // [complete type section] section_end
                // [safe                 ] unsafe (one-past)
                //                       ^^ err_curr: endpoint is diagnostic only.
                err.err_curr = section_end;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"typesec_types";
                err.err_selectable.exceed_the_max_parser_limit.value = definitions.type_count;
                err.err_selectable.exceed_the_max_parser_limit.maxval = limit.max_type_sec_types;
                err.err_code = base::wasm_parse_error_code::exceed_the_max_parser_limit;
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
        auto const flat_count{static_cast<::std::size_t>(definitions.type_count)};
        // [section_begin ... section_end) is the decoder-proven type-section allocation.
        // [safe                         ] section_size bounds each recorded prefix read below.
        auto const section_size{section_begin == section_end ? 0uz :
            static_cast<::std::size_t>(section_end - section_begin)};
        auto checked_prefix = [&](::std::size_t offset) constexpr UWVM_THROWS
        {
            if(offset >= section_size) [[unlikely]]
            {
                err.err_curr = section_end; // Endpoint is diagnostic only, never dereferenced.
                err.err_code = base::wasm_parse_error_code::illegal_type_prefix;
                err.err_selectable.u8 = 0u;
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            // [checked bytes ... prefix ... remaining bytes] section_end
            // [safe                                        ] offset < section_size precedes this read.
            return ::std::to_integer<unsigned>(section_begin[offset]);
        };
        section.types.reserve(flat_count);
        section.owned_signatures.reserve(flat_count);
        section.core3_type_kinds.reserve(flat_count);
        using carrier_t = final_value_type_t<Fs...>;
        auto function_reference_syntax = [&](t3::core_value_type value) noexcept
        {
            if(value.kind != t3::value_kind::reference) { return false; }
            // Nullable func/extern retain the reference-types gate. Their non-nullable
            // forms need typed function references; nofunc/noextern belong to GC.
            if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::func) ||
               value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_))
            { return value.source_prefix == 0x64u; }
            // Type validation proved every defined heap index is within this complete context.
            return value.heap.is_defined() &&
                context.records.index_unchecked(static_cast<::std::size_t>(value.heap.code)).kind ==
                    t3::composite_kind::function;
        };
        auto project = [&](t3::core_value_type value, ::std::size_t offset) UWVM_THROWS -> carrier_t
        {
            using kind = t3::value_kind;
            unsigned byte{};
            switch(value.kind)
            {
                case kind::i32: byte = 0x7fu; break;
                case kind::i64: byte = 0x7eu; break;
                case kind::f32: byte = 0x7du; break;
                case kind::f64: byte = 0x7cu; break;
                case kind::v128: byte = 0x7bu; break;
                case kind::reference:
                    byte = value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                           value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern) ? 0x6fu :
                           value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                           value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn) ? 0x69u : 0x70u;
                    break;
            }
            auto const needs_function_references{function_reference_syntax(value)};
            if((byte == 0x7bu && options.disable_simd) ||
               ((byte == 0x70u || byte == 0x6fu || byte == 0x69u) && options.disable_reference_types) ||
               (byte == 0x69u && options.disable_exceptions) ||
               (needs_function_references && options.disable_function_references)) [[unlikely]]
            {
                // [section_begin ... complete value declaration ... section_end]
                // [safe                                               ] unsafe (possibly section_end)
                //                    ^^ err_curr: enclosing subtype offset was validated by the decoder.
                err.err_curr = section_begin == section_end ? section_begin : section_begin + offset;
                err.err_selectable.wasm1p1_feature_required = {
                    .value = byte,
                    .feature = byte == 0x7bu ? base::wasm1p1_feature_kind::simd :
                        byte == 0x69u && options.disable_exceptions ? base::wasm1p1_feature_kind::exceptions :
                        options.disable_reference_types ? base::wasm1p1_feature_kind::reference_types :
                        base::wasm1p1_feature_kind::function_references,
                    .subject = base::wasm1p1_error_subject::function_type};
                err.err_code = base::wasm_parse_error_code::wasm1p1_feature_required;
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            section.requires_function_references |= needs_function_references;
            section.requires_gc |= v3::core3_value_requires_gc(value, ::std::addressof(context), flat_count);
            // Includes unused signatures and every nonpacked aggregate field in the existing projection.
            section.requires_exceptions |= v3::core3_value_requires_exceptions(value);
            // This existing projection visits parameters/results and nonpacked fields.
            // Numeric/packed aggregate fields cannot become SIMD or reference requirements.
            section.requires_simd |= value.kind == kind::v128;
            section.requires_reference_types |= value.kind == kind::reference;
            return static_cast<carrier_t>(byte);
        };
        for(auto const& group : definitions.groups)
        {
            auto const group_prefix{checked_prefix(group.binary_offset)};
            section.requires_gc |= group_prefix == 0x4eu;
            for(auto const& definition : group.types)
            {
                // A direct singleton definition shares the already checked group prefix.
                auto const definition_prefix{definition.binary_offset == group.binary_offset ?
                    group_prefix : checked_prefix(definition.binary_offset)};
                section.requires_gc |= definition.kind != t3::composite_kind::function ||
                    definition_prefix == 0x4fu || definition_prefix == 0x50u;
                t3::owned_function_signature<carrier_t> signature{};
                signature.type_index = section.types.size();
                if(definition.kind == t3::composite_kind::function)
                {
                    // Only function result vectors carry the multi-value declaration requirement.
                    signature.requires_multi_value = definition.results.size() > 1uz;
                    section.requires_multi_value |= signature.requires_multi_value;
                    if((options.disable_multi_value || options.controllable_allow_multi_result_vector) &&
                       definition.results.size() > 1uz) [[unlikely]]
                    {
                        // [section_begin ... decoded function subtype ... section_end]
                        // [safe                                          ] unsafe (possibly section_end)
                        //                    ^^ err_curr: decoder-proven binary_offset; diagnostic only.
                        err.err_curr = section_begin == section_end ? section_begin : section_begin + definition.binary_offset;
                        err.err_selectable.wasm1p1_feature_required = {
                            .value = 0x60u, .feature = base::wasm1p1_feature_kind::multi_value,
                            .subject = base::wasm1p1_error_subject::function_type};
                        err.err_code = base::wasm_parse_error_code::wasm1p1_feature_required;
                        base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    signature.parameters = definition.parameters;
                    signature.results = definition.results;
                    for(auto value : signature.parameters)
                    {
                        signature.requires_function_references |= function_reference_syntax(value);
                        signature.requires_exceptions |= v3::core3_value_requires_exceptions(value);
                        signature.requires_simd |= value.kind == t3::value_kind::v128;
                        signature.requires_reference_types |= value.kind == t3::value_kind::reference;
                        signature.carriers.push_back(project(value, definition.binary_offset));
                    }
                    for(auto value : signature.results)
                    {
                        signature.requires_function_references |= function_reference_syntax(value);
                        signature.requires_exceptions |= v3::core3_value_requires_exceptions(value);
                        signature.requires_simd |= value.kind == t3::value_kind::v128;
                        signature.requires_reference_types |= value.kind == t3::value_kind::reference;
                        signature.carriers.push_back(project(value, definition.binary_offset));
                    }
                }
                for(auto const& field : definition.fields)
                {
                    if(field.storage.packed == t3::packed_kind::none)
                    { static_cast<void>(project(field.storage.value, definition.binary_offset)); }
                }
                signature.carriers.push_back(carrier_t{}); // A live sentinel makes empty parameter/result ranges valid.
                section.owned_signatures.push_back(::std::move(signature));
                final_function_type<Fs...> function{};
                t3::bind_owned_function_signature(section.owned_signatures.back_unchecked(), function);
                section.types.push_back_unchecked(::std::move(function));
                section.core3_type_kinds.push_back_unchecked(definition.kind);
            }
        }
        section.core3_recursive_types = ::std::move(definitions);
        section.core3_context = ::std::move(context);
        return true;
    }

    /// @brief Decode a Core 3 recursive group consisting only of executable function types.
    /// @details The outer type-section count counts rec groups, while each member contributes a
    ///          separate flat type index. Reserve all member slots before the existing 0x60 decoder
    ///          uses unchecked insertion, and expose the whole group to bounded forward references.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...) &&
                 ::std::same_as<final_type_type_t<Fs...>, final_function_type<Fs...>>)
    inline constexpr ::std::byte const* define_parse_core3_recursive_function_group(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* cursor, ::std::byte const* const end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters,
        ::std::byte const* const group_prefix) UWVM_THROWS
    {
        namespace base = ::uwvm2::parser::wasm::base;
        auto const& options{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(parameters)};
        if(options.disable_gc) [[unlikely]]
        {
            // [0x4e] group body ... end
            // [safe ] unsafe (possibly end)
            // ^^ group_prefix / err_curr: caller checked the prefix byte; diagnostic borrow only.
            err.err_curr = group_prefix;
            err.err_code = base::wasm_parse_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {
                .value = 0x4eu, .feature = base::wasm1p1_feature_kind::gc,
                .subject = base::wasm1p1_error_subject::function_type};
            base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [0x4e] member_count ... end
        // [safe ] unsafe (possibly end)
        //        ^^ cursor: parse_by_scan bounds the complete unsigned LEB against end.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 member_count{};
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        auto const [after_count, count_error]{::fast_io::parse_by_scan(
            reinterpret_cast<char8_t_const_may_alias_ptr>(cursor),
            reinterpret_cast<char8_t_const_may_alias_ptr>(end),
            ::fast_io::mnp::leb128_get(member_count))};
        if(count_error != ::fast_io::parse_code::ok) [[unlikely]]
        {
            // [0x4e] member_count ... end
            // [safe ] unsafe (possibly end)
            //        ^^ cursor / err_curr: failed scan does not advance cursor or dereference end.
            err.err_curr = cursor;
            err.err_code = base::wasm_parse_error_code::invalid_type_count;
            base::throw_wasm_parse_code(count_error);
        }
        // [0x4e member_count] subtype ... end
        // [safe               ] unsafe (possibly end)
        //                      ^^ cursor: after_count is returned inside [old cursor, end].
        cursor = reinterpret_cast<::std::byte const*>(after_count);

        auto& section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        constexpr auto size_max{::std::numeric_limits<::std::size_t>::max()};
        if constexpr(size_max < ::std::numeric_limits<decltype(member_count)>::max())
        {
            if(member_count > size_max) [[unlikely]]
            {
                err.err_curr = cursor;
                err.err_selectable.u64 = member_count;
                err.err_code = base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
        auto const count{static_cast<::std::size_t>(member_count)};
        // [0x4e member_count] member bytes ... end
        // [safe               ] unsafe (possibly end)
        //                      ^^ cursor: successful bounded LEB decoding proves cursor <= end.
        // A function member needs at least its 0x60 prefix and two vector lengths. This also
        // prevents an attacker-controlled count from causing a large allocation before decoding.
        if(count > static_cast<::std::size_t>(end - cursor) / 3uz) [[unlikely]]
        {
            err.err_curr = cursor;
            err.err_code = base::wasm_parse_error_code::invalid_type_count;
            base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if(count > size_max - section.types.size()) [[unlikely]]
        {
            err.err_curr = cursor;
            err.err_selectable.u64 = member_count;
            err.err_code = base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
            base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& limit{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(parameters).parser_limit};
            if(section.types.size() > limit.max_type_sec_types ||
               count > limit.max_type_sec_types - section.types.size()) [[unlikely]]
            {
                err.err_curr = cursor;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"typesec_types";
                err.err_selectable.exceed_the_max_parser_limit.value = section.types.size() + count;
                err.err_selectable.exceed_the_max_parser_limit.maxval = limit.max_type_sec_types;
                err.err_code = base::wasm_parse_error_code::exceed_the_max_parser_limit;
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
        section.types.reserve(section.types.size() + count);
        // All group members are syntactically required below to be function composites. The
        // owned signature scanner may therefore provisionally resolve their flat type indices.
        section.active_recursive_group_visible_types = section.types.size() + count;
        for(::std::size_t member{}; member != count; ++member)
        {
            if(cursor == end) [[unlikely]]
            {
                // [0x4e member_count][preceding members] end
                // [safe                               ] unsafe (cursor equals end)
                //                                      ^^ cursor / err_curr: endpoint is diagnostic only.
                err.err_curr = cursor;
                err.err_code = base::wasm_parse_error_code::illegal_type_prefix;
                err.err_selectable.u8 = 0u;
                base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
            }
            // [0x4e member_count][preceding members] prefix ... end
            // [safe                               ][safe ] unsafe (possibly end)
            //                                       ^^ cursor: cursor != end proves this byte readable.
            auto const member_prefix{::std::to_integer<::std::uint_least8_t>(*cursor)};
            if(member_prefix != 0x60u) [[unlikely]]
            {
                // Explicit 0x4f/0x50 subtypes and GC aggregates need retained subtype metadata.
                // Reject these until their hierarchy can be checked, never erase it to a 0x60 carrier.
                err.err_curr = cursor;
                err.err_selectable.u8 = member_prefix;
                err.err_code = base::wasm_parse_error_code::wasm3_rich_signature_not_integrated;
                base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            // [0x4e member_count][preceding members 0x60] signature ... end
            // [safe                                         ] unsafe (possibly end)
            //                                              ^^ cursor: the checked prefix alone is skipped.
            ++cursor;
            // [0x4e member_count][preceding members 0x60][checked signature] ... end
            // [safe                                                              ] unsafe (possibly end)
            //                                                                    ^^ cursor: decoder commits only after bounds checks.
            cursor = handle_type_prefix_functype(sec_adl, module_storage, cursor, end, err, parameters);
        }
        section.active_recursive_group_visible_types = 0uz;
        section.requires_gc = true;
        return cursor;
    }

    // Core 3 address-limits decoder is shared by the parser and the standalone
    // validator. Core 3 memory64 is independent of the threads extension; the
    // memory64 policy enables the u64 limits grammar, including memory32 limits.
    // ADL extension of the existing 0x60 parser. The disabled path retains borrowed-byte MVP/Core 2 signatures.
    // The legacy borrowed-signature parser calls this from its already bounded
    // value-check loop. No second scan or bytecode cursor is introduced.
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...) &&
            ::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>,
                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>)
    inline constexpr void define_record_typesec_value_requirements(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>& section,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> value) noexcept
    {
        auto const carrier{static_cast<unsigned>(value)};
        section.requires_simd |= carrier == 0x7bu;
        section.requires_reference_types |= carrier == 0x70u || carrier == 0x6fu || carrier == 0x69u;
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...) &&
                 ::std::same_as<final_type_type_t<Fs...>, final_function_type<Fs...>>)
    inline constexpr bool define_parse_owned_function_signature(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const*& cursor, ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        namespace w1 = ::uwvm2::parser::wasm::standard::wasm1::features;
        namespace w3 = ::uwvm2::validation::standard::wasm3;
        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        using error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        auto const& options{get_wasm1p1_parameter(parameters)};
        // The exceptions extension may use `(ref exn)`/`(ref noexn)` in a plain
        // 0x60 signature while function references are independently disabled.
        if(options.disable_function_references && options.disable_exceptions && options.disable_gc) { return false; }
        auto& section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<w1::type_section_storage_t<Fs...>>(module_storage.sections)};
        t3::owned_function_signature<typename w1::final_value_type_t<Fs...>> signature{};
        // [0x60] parameter/result vectors ... end
        // [safe] unsafe (could be end)
        //        ^^ begin borrows cursor; the transactional decoder proves each consumed byte.
        auto const begin{cursor};
        auto const decoded{w3::scan_core3_function_signature(cursor, end,
            {.simd = !options.disable_simd, .reference_types = !options.disable_reference_types,
             .function_references = !options.disable_function_references,
             .exceptions = !options.disable_exceptions,
             .gc = !options.disable_gc,
             .multi_value = !(options.disable_multi_value || options.controllable_allow_multi_result_vector),
             .function_type_context = true,
             .current_function_type_index = section.active_recursive_group_visible_types ?
                 section.active_recursive_group_visible_types - 1uz : section.types.size()}, signature)};
        // [0x60][complete vectors] ... end on success; cursor is unchanged on any decoding/policy failure.
        // [safe                 ] unsafe (could be end)
        //                         ^^ cursor after success; no partial signature has been published.
        if(decoded.error != w3::function_signature_error::ok) [[unlikely]]
        {
            // [begin ... error offset ...] end
            // [safe                     ] offset <= end - begin, including a truncation exactly at end.
            //            ^^ err_curr: diagnostic only; this assignment never dereferences an endpoint.
            err.err_curr = begin + decoded.error_offset;
            if(decoded.error == w3::function_signature_error::rich_type_not_integrated)
            {
                err.err_code = error::wasm3_rich_signature_not_integrated;
                // This policy error follows a successfully decoded value at error_offset < end - begin.
                err.err_selectable.u8 = ::std::to_integer<::std::uint_least8_t>(begin[decoded.error_offset]);
            }
            else if(decoded.error == w3::function_signature_error::unknown_function_type)
            {
                err.err_code = error::illegal_type_index;
                err.err_selectable.u32 = decoded.offending_type_index;
            }
            else if(decoded.error == w3::function_signature_error::binary)
            {
                err.err_code = decoded.in_results ? error::invalid_result_length : error::invalid_parameter_length;
                if(decoded.binary_error == w3::recursive_type_binary_error::invalid_value_type ||
                   decoded.binary_error == w3::recursive_type_binary_error::invalid_heap_type)
                {
                    err.err_code = error::illegal_value_type;
                    // A value/heap grammar error follows a checked byte read; report that byte, never end.
                    err.err_selectable.u8 = decoded.error_offset ? ::std::to_integer<::std::uint_least8_t>(begin[decoded.error_offset - 1uz]) : 0u;
                }
            }
            else
            {
                err.err_code = error::wasm1p1_feature_required;
                err.err_selectable.wasm1p1_feature_required = {
                    .value = 0x60u,
                    .feature = decoded.error == w3::function_signature_error::simd_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd :
                        decoded.error == w3::function_signature_error::exceptions_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions :
                        decoded.error == w3::function_signature_error::function_references_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references :
                        decoded.error == w3::function_signature_error::gc_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc :
                        decoded.error == w3::function_signature_error::reference_types_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types :
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value,
                    .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::function_type};
            }
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        section.requires_function_references |= signature.requires_function_references;
        section.requires_exceptions |= signature.requires_exceptions;
        section.requires_simd |= signature.requires_simd;
        section.requires_reference_types |= signature.requires_reference_types;
        section.requires_multi_value |= signature.requires_multi_value;
        signature.type_index = section.types.size();
        section.owned_signatures.push_back(::std::move(signature));
        w1::final_function_type<Fs...> function{};
        // The owner is committed first. Moving/reallocating outer records preserves each inner carrier allocation.
        t3::bind_owned_function_signature(section.owned_signatures.back_unchecked(), function);
        section.types.push_back_unchecked(::std::move(function)); // Section count/reserve is proved by the caller.
        return true;
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...) &&
            ::std::same_as<final_value_type_t<Fs...>, ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>)
    inline constexpr bool parse_explicit_declaration_value_type(
        ::std::byte const*& cursor, ::std::byte const* end, final_value_type_t<Fs...>& value,
        bool& explicit_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type* core_type = nullptr,
        ::std::size_t known_function_type_count = 0uz,
        ::uwvm2::validation::standard::wasm3::recursive_type_context const* context = nullptr,
        bool* requires_function_references = nullptr) UWVM_THROWS
    {
        namespace w3 = ::uwvm2::validation::standard::wasm3;
        using error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        // [declaration prefix] value type ... bound
        // [safe       ] cursor may equal end; the short circuit precedes every prefix read.
        if(cursor == end) { return false; }
        // [declaration prefix][first byte] ... bound
        // [safe                      ] unsafe (possibly end)
        //                      ^^ cursor is proven live before this one-byte read; no pointer moves.
        auto const prefix{::std::to_integer<unsigned>(*cursor)};
        // Core 3 element, table and global declarations may use shorthand abstract heaps.
        // The MVP funcref/externref bytes still take the legacy path for their existing policy.
        // Every Core 3 abstract shorthand is a complete valtype in a local just
        // as in a table/global/element declaration. Preserve its heap witness
        // instead of letting the legacy one-byte carrier erase the distinction.
        if(prefix != 0x63u && prefix != 0x64u &&
           !((prefix >= 0x69u && prefix <= 0x6eu) ||
             (prefix >= 0x71u && prefix <= 0x74u))) { return false; }
        auto const& policy{get_wasm1p1_parameter(parameters)};
        // [declaration prefix][prefix] heap ... bound
        // [safe               ]
        //              ^^ begin and next borrow a proven nonempty range; the caller cursor remains unchanged.
        auto const begin{cursor};
        auto next{cursor};
        auto const decoded{context == nullptr ?
            w3::scan_core3_value_carrier(next, end, !policy.disable_function_references,
                known_function_type_count, !policy.disable_gc) :
            w3::scan_core3_value_carrier(next, end, !policy.disable_function_references,
                known_function_type_count, *context, !policy.disable_gc, !policy.disable_exceptions)};
        // [declaration prefix][checked valtype] next ... bound on success; next == begin on failure.
        // [safe                        ] unsafe (could be end)
        //                                ^^ next; no borrowed view or partially decoded declaration is published.
        if(decoded.error != w3::value_carrier_error::ok || policy.disable_reference_types) [[unlikely]]
        {
            // [begin ... error_offset ...] bound
            // [safe                      ] offset is bounded by the decoder, including exactly bound.
            //            ^^ err_curr is a diagnostic address, never dereferenced here.
            err.err_curr = begin + decoded.error_offset;
            if(decoded.error == w3::value_carrier_error::function_references_disabled ||
               decoded.error == w3::value_carrier_error::gc_disabled ||
               decoded.error == w3::value_carrier_error::exceptions_disabled ||
               (decoded.error == w3::value_carrier_error::ok && policy.disable_reference_types))
            {
                err.err_code = error::wasm1p1_feature_required;
                err.err_selectable.wasm1p1_feature_required = {
                    .value = ::std::to_integer<unsigned>(*begin),
                    .feature = decoded.error == w3::value_carrier_error::function_references_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references :
                        decoded.error == w3::value_carrier_error::gc_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc :
                        decoded.error == w3::value_carrier_error::exceptions_disabled ?
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions :
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                    .subject = subject};
            }
            else
            {
                if(decoded.error == w3::value_carrier_error::unknown_function_type ||
                   decoded.error == w3::value_carrier_error::unknown_type)
                {
                    err.err_code = error::illegal_type_index;
                    err.err_selectable.u32 = static_cast<::std::uint_least32_t>(decoded.type.heap.code);
                }
                else
                {
                    err.err_code = decoded.error == w3::value_carrier_error::rich_type_not_integrated ?
                        error::wasm3_rich_value_not_integrated : error::illegal_value_type;
                    err.err_selectable.u8 = ::std::to_integer<::std::uint_least8_t>(*begin); // Prefix proved readable above.
                }
            }
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        value = static_cast<final_value_type_t<Fs...>>(decoded.carrier);
        if(core_type) { *core_type = decoded.type; }
        explicit_type = true;
        if(requires_function_references != nullptr)
        { *requires_function_references = decoded.requires_function_references; }
        // Zero-count local runs still validate their encoded heap and independent feature gates.
        // [declaration prefix][complete checked valtype] next declaration/instruction ... bound
        // [safe                                ] unsafe (could be end)
        //                                        ^^ cursor: publish the bounded temporary cursor only on success.
        cursor = next;
        return true;
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...) &&
            ::std::same_as<final_value_type_t<Fs...>, ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>)
    inline constexpr bool define_parse_extended_codesec_value_type(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<code_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const*& cursor, ::std::byte const* end, final_value_type_t<Fs...>& value,
        bool& requires_function_references,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type& core_type, bool& has_core_type,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        // The shared decoder commits cursor only after proving the entire valtype inside [cursor, end).
        // [checked valtype] next ... end on success; cursor is unchanged on failure or an unhandled prefix.
        // [safe           ] ^^ cursor may equal end; the code-section caller supplies the function-body bound.
        auto const& type_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
        bool explicit_type{}, needs_function_references{};
        if(!parse_explicit_declaration_value_type(cursor, end, value, explicit_type,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::local_type, err, parameters,
            ::std::addressof(core_type), type_section.types.size(), ::std::addressof(type_section.core3_context),
            ::std::addressof(needs_function_references))) { return false; }
        has_core_type = true;
        requires_function_references |= needs_function_references;
        // This hook has decoded and checked the run type, independently of its run count.
        auto& code_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections)};
        code_section.locals_require_exceptions |=
            ::uwvm2::validation::standard::wasm3::core3_value_requires_exceptions(core_type);
        return true;
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_threads_memory_type(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::memory_type& memory,
        ::std::byte const* curr, ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        namespace w3 = ::uwvm2::validation::standard::wasm3;
        using error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        auto const& options{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(parameters)};
        // flags min max? ... (end)
        // unsafe (could be end)
        // ^^ begin borrows curr; the transactional decoder checks each consumed byte.
        auto const begin{curr};
        auto const decoded{w3::scan_address_limits(curr, end, w3::address_limits_kind::memory,
            !options.disable_memory64, !options.disable_threads, !options.disable_memory64)};
        // [flags min max?] ... (end) on success, or unchanged begin on failure.
        // [safe          ] unsafe (could be end)
        //                  ^^ curr on success; no partially decoded type is published.
        if(decoded.error != w3::address_limits_error::ok) [[unlikely]]
        {
            // [input ...] (end)
            //             ^^ diagnostic may equal end and is never dereferenced here.
            // error_offset is within [begin, end], including truncated encodings.
            err.err_curr = begin + decoded.error_offset;
            switch(decoded.error)
            {
                case w3::address_limits_error::missing_flags: err.err_code = error::limit_type_cannot_find_flag; break;
                case w3::address_limits_error::minimum_encoding: err.err_code = error::limit_type_invalid_min; break;
                case w3::address_limits_error::maximum_encoding: err.err_code = error::limit_type_invalid_max; break;
                case w3::address_limits_error::maximum_below_minimum:
                    if(!options.disable_memory64)
                    {
                        err.err_code = error::wasm3_limit_type_max_lt_min;
                        err.err_selectable.u64arr[0] = decoded.limits.max;
                        err.err_selectable.u64arr[1] = decoded.limits.min;
                    }
                    else
                    {
                        err.err_code = error::limit_type_max_lt_min;
                        err.err_selectable.u32arr[0] = static_cast<::std::uint_least32_t>(decoded.limits.max);
                        err.err_selectable.u32arr[1] = static_cast<::std::uint_least32_t>(decoded.limits.min);
                    }
                    break;
                case w3::address_limits_error::limit_out_of_range:
                {
                    // [flags min max?] ... (end)
                    // [safe          ] complete bounded limits; point at their flag.
                    // ^^ err_curr borrows begin, without advancing or dereferencing end.
                    err.err_curr = begin;
                    auto const maximum{w3::address_limit_maximum(w3::address_limits_kind::memory, decoded.limits.address_type)};
                    auto const actual{decoded.limits.min > maximum ? decoded.limits.min : decoded.limits.max};
                    if(!options.disable_memory64)
                    {
                        err.err_code = error::wasm3_memory_limit_out_of_range;
                        err.err_selectable.u64arr[0] = actual;
                        err.err_selectable.u64arr[1] = maximum;
                    }
                    else
                    {
                        err.err_code = error::memory_section_resolved_exceeded_the_maximum_value;
                        err.err_selectable.u32 = static_cast<::std::uint_least32_t>(actual);
                    }
                    break;
                }
                case w3::address_limits_error::address64_disabled:
                    err.err_code = error::wasm1p1_feature_required;
                    err.err_selectable.wasm1p1_feature_required = {
                        .value = 4u,
                        .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::memory64,
                        .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::memory_type};
                    break;
                case w3::address_limits_error::threads_disabled:
                    err.err_code = error::wasm1p1_feature_required;
                    err.err_selectable.wasm1p1_feature_required = {
                        .value = 3u,
                        .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::threads,
                        .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::memory_type};
                    break;
                default:
                    err.err_code = error::limit_type_illegal_flag;
                    // A missing flag was handled above; every remaining error
                    // proves begin points at a live flag byte.
                    err.err_selectable.u8 = ::std::to_integer<::std::uint_least8_t>(*begin);
                    break;
            }
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        memory.limits.min = decoded.limits.min;
        memory.limits.max = decoded.limits.present_max ? decoded.limits.max :
            ::uwvm2::parser::wasm::standard::wasm1p1::features::memory_limits_type::default_max;
        memory.limits.present_max = decoded.limits.present_max;
        memory.shared = decoded.limits.shared;
        memory.address64 = decoded.limits.address_type == w3::storage_address_type::i64;
        return curr;
    }

    // Core 3 table addresses have an independent policy. Tables never accept
    // the shared bit, and their legal u64 limits are not host allocation limits.
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_core3_table_type(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type& table,
        ::std::byte const* curr, ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        namespace w3 = ::uwvm2::validation::standard::wasm3;
        using error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        auto const& options{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(parameters)};
        // flags min max? ... (end)
        // unsafe (could be end)
        // ^^ begin borrows curr; the transactional decoder checks each consumed byte.
        auto const begin{curr};
        auto const decoded{w3::scan_address_limits(curr, end, w3::address_limits_kind::table,
            !options.disable_table64, false, !options.disable_table64)};
        // [flags min max?] ... (end) on success, or unchanged begin on failure.
        // [safe          ] unsafe (could be end)
        //                  ^^ curr on success; no partially decoded type is published.
        if(decoded.error != w3::address_limits_error::ok) [[unlikely]]
        {
            // [input ...] (end)
            //             ^^ diagnostic may equal end and is never dereferenced here.
            // error_offset is within [begin, end], including truncated encodings.
            err.err_curr = begin + decoded.error_offset;
            switch(decoded.error)
            {
                case w3::address_limits_error::missing_flags: err.err_code = error::limit_type_cannot_find_flag; break;
                case w3::address_limits_error::minimum_encoding: err.err_code = error::limit_type_invalid_min; break;
                case w3::address_limits_error::maximum_encoding: err.err_code = error::limit_type_invalid_max; break;
                case w3::address_limits_error::maximum_below_minimum:
                    if(!options.disable_table64)
                    {
                        err.err_code = error::wasm3_limit_type_max_lt_min;
                        err.err_selectable.u64arr[0] = decoded.limits.max;
                        err.err_selectable.u64arr[1] = decoded.limits.min;
                    }
                    else
                    {
                        err.err_code = error::limit_type_max_lt_min;
                        err.err_selectable.u32arr[0] = static_cast<::std::uint_least32_t>(decoded.limits.max);
                        err.err_selectable.u32arr[1] = static_cast<::std::uint_least32_t>(decoded.limits.min);
                    }
                    break;
                case w3::address_limits_error::limit_out_of_range:
                {
                    // [flags min max?] ... (end)
                    // [safe          ] complete bounded limits; point at their flag.
                    // ^^ err_curr borrows begin, without advancing or dereferencing end.
                    err.err_curr = begin;
                    auto const maximum{w3::address_limit_maximum(w3::address_limits_kind::table, decoded.limits.address_type)};
                    auto const actual{decoded.limits.min > maximum ? decoded.limits.min : decoded.limits.max};
                    err.err_code = error::wasm3_table_limit_out_of_range;
                    err.err_selectable.u64arr[0] = actual;
                    err.err_selectable.u64arr[1] = maximum;
                    break;
                }
                case w3::address_limits_error::address64_disabled:
                    err.err_code = error::wasm1p1_feature_required;
                    err.err_selectable.wasm1p1_feature_required = {
                        .value = 4u,
                        .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::table64,
                        .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type};
                    break;
                default:
                    err.err_code = error::limit_type_illegal_flag;
                    // A missing flag was handled above; every remaining error
                    // proves begin points at a live flag byte.
                    err.err_selectable.u8 = ::std::to_integer<::std::uint_least8_t>(*begin);
                    break;
            }
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        table.limits.min = decoded.limits.min;
        table.limits.max = decoded.limits.present_max ? decoded.limits.max :
            w3::address_limit_maximum(w3::address_limits_kind::table, decoded.limits.address_type);
        table.limits.present_max = decoded.limits.present_max;
        table.address64 = decoded.limits.address_type == w3::storage_address_type::i64;
        return curr;
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* memory_section_memory_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<memory_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::memory_type& memory,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* curr, ::std::byte const* end, ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        // [limits bytes ...] end: the original scanner proves every byte and its returned endpoint.
        // ^^ curr is borrowed, not advanced here; the existing caller commits the returned pointer.
        auto const checked_end{scan_threads_memory_type(memory, curr, end, err, parameters)};
        auto& section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections)};
        section.requires_memory64 |= memory.address64;
        section.requires_threads |= memory.shared;
        // [complete checked limits] next ... end
        // [safe                   ] ^^ checked_end is within the original span or exactly one-past; no read follows.
        return checked_end;
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_memory_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::memory_type& memory,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* curr, ::std::byte const* end, ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        // Imports share the exact successful limits decoder and requirement publication.
        return memory_section_memory_handler(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
            ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>{},
            memory, module_storage, curr, end, err, parameters);
    }

    inline constexpr bool is_valid_value_type(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_value_type(value_type); }

    /// @brief Check whether a wasm1.1 type-section value type is enabled by runtime feature flags.
    /// @details This 3-argument hook is intentionally named differently from the wasm1 two-argument hook.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool define_check_typesec_value_type_with_feature_parameter(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para); }

    /// @brief Check whether a wasm1.1 code-section local value type is enabled by runtime feature flags.
    /// @details This 3-argument hook is intentionally named differently from the wasm1 two-argument hook.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool define_check_codesec_value_type_with_feature_parameter(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<code_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para); }

    /// @brief Parse a wasm1.1 table type from a table section.
    /// @details Non-MVP table element types are gated by the table-instructions group, then by the feature that defines the reference type.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* scan_extended_table_type(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type & table_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [before_table_type ...] reftype limits ... (section_end)
        // [        safe         ] unsafe (could be the section_end)
        //                         ^^ section_curr
        if(section_curr == section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_type_cannot_find_element;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
        value_type carrier{};
        bool explicit_type{}, needs_function_references{};
        // [table type ...] section_end: section_curr != section_end was proved above.
        // ^^ next borrows the current byte; the bounded decoder commits it only after a complete valtype.
        auto next{section_curr};
        auto const& known_type_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
        if(!parse_explicit_declaration_value_type(next, section_end, carrier, explicit_type,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type, err, fs_para,
            ::std::addressof(table_r.core_type), known_type_section.types.size(),
            ::std::addressof(known_type_section.core3_context), ::std::addressof(needs_function_references)))
        {
            carrier = static_cast<value_type>((::std::to_integer<unsigned>(*next) & 0xffu));
            // [one-byte legacy reftype] limits ... section_end
            // [safe                  ] ^^ next advances past the byte proved readable above, possibly to end.
            ++next;
        }
        // [complete reftype] limits ... section_end
        // [safe            ] ^^ next may equal section_end; limits are not read here.
        auto const elemtype{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(carrier)};
        auto const reftype{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(carrier)};
        if(elemtype != 0x69u &&
           !::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_reference_type(
               ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(reftype))) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.wasm1p1_reference_type.value = elemtype;
            err.err_selectable.wasm1p1_reference_type.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_reference_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
        if(explicit_type || reftype != reference_type::funcref)
        {
            auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
            auto const require_wasm1p1_feature = [&](::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature) UWVM_THROWS
            {
                err.err_curr = section_curr;
                err.err_selectable.wasm1p1_feature_required.value = elemtype;
                err.err_selectable.wasm1p1_feature_required.feature = feature;
                err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            };

            auto const require_wasm2_feature = [&](::uwvm2::parser::wasm::base::wasm2_feature_kind feature) UWVM_THROWS
            {
                err.err_curr = section_curr;
                err.err_selectable.wasm2_feature_required.value = elemtype;
                err.err_selectable.wasm2_feature_required.feature = feature;
                err.err_selectable.wasm2_feature_required.subject = ::uwvm2::parser::wasm::base::wasm2_error_subject::table_type;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm2_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            };

            if(para.disable_table_instructions) [[unlikely]]
            {
                require_wasm2_feature(::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions);
            }
            if(para.disable_reference_types) [[unlikely]]
            {
                require_wasm1p1_feature(::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types);
            }
            if(elemtype == 0x69u && para.disable_exceptions) [[unlikely]]
            {
                require_wasm1p1_feature(::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions);
            }
        }

        table_r.reftype = reftype;
        table_r.has_core_type = explicit_type;
        auto& table_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};
        // This successful existing reftype traversal also handles actual imports.
        // Preserve short-form MVP funcref tables when reference-types is disabled.
        bool const needs_reference_types{explicit_type || reftype != reference_type::funcref};
        if(needs_reference_types && !table_section.requires_reference_types)
        { table_section.reference_types_diagnostic_value = explicit_type ? table_r.core_type.source_prefix : elemtype; }
        table_section.requires_reference_types |= needs_reference_types;
        table_section.requires_function_references |= needs_function_references; // Includes imported table types.
        table_section.requires_gc |= explicit_type && ::uwvm2::validation::standard::wasm3::core3_value_requires_gc(
            table_r.core_type, ::std::addressof(known_type_section.core3_context), known_type_section.types.size());
        table_section.requires_exceptions |= explicit_type &&
            ::uwvm2::validation::standard::wasm3::core3_value_requires_exceptions(table_r.core_type);
        // [complete checked reftype] limits ... section_end
        // [safe                   ] unsafe (could be section_end)
        //                           ^^ section_curr: commit the bounded decoded endpoint; limits keep the same bound.
        section_curr = next;

        // [before_table_type ... reftype] limits ... (section_end)
        // [            safe             ] unsafe (could be the section_end)
        //                                 ^^ section_curr
        //
        // scan_limit_type continues with the same section_end bound and returns the first byte after limits.
        // [complete reftype] limits ... section_end; the unchanged bounded scanner proves its returned endpoint.
        auto const checked_end{scan_core3_table_type(table_r, section_curr, section_end, err, fs_para)};
        table_section.requires_table64 |= table_r.address64;
        // [complete checked table type] next ... section_end
        // [safe                       ] ^^ checked_end is within the original allocation or exactly one-past.
        return checked_end;
    }

    /// @brief Parse a wasm1.1 table type from an import descriptor.
    /// @details Imports parse only a table type; the definition-only 0x40 0x00 prefix is never accepted.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_table_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type & table_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        return scan_extended_table_type(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>>{},
                                           table_r,
                                           module_storage,
                                           section_curr,
                                           section_end,
                                           err,
                                           fs_para);
    }

    /// @brief Parse a wasm1.1 global type from a global section.
    /// @details Extended value types are rejected unless their corresponding runtime feature flag is enabled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* global_section_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type & global_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [before_global_type ...] valtype mut ... (section_end)
        // [        safe          ] unsafe (could be the section_end)
        //                          ^^ section_curr
        if(section_curr == section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_valtype;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type{};
        bool explicit_type{}, needs_function_references{};
        // [global type ...] section_end: section_curr != section_end was proved above.
        // ^^ next borrows the current byte; explicit decoding remains bounded by section_end.
        auto next{section_curr};
        auto const& known_type_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
        if(!parse_explicit_declaration_value_type(next, section_end, value_type, explicit_type,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::global_type, err, fs_para,
            ::std::addressof(global_r.core_type), known_type_section.types.size(),
            ::std::addressof(known_type_section.core3_context), ::std::addressof(needs_function_references)))
        {
            value_type = static_cast<decltype(value_type)>((::std::to_integer<unsigned>(*next) & 0xffu));
            // [one-byte legacy valtype] mut ... section_end
            // [safe                  ] ^^ next advances past the proven live byte, possibly to end.
            ++next;
        }
        // [complete valtype] mut ... section_end
        // [safe            ] ^^ next may equal section_end; mutability is checked after this type is accepted.
        auto const raw_type{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type)};
        if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para)) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u8 = raw_type;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_valtype;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        global_r.type = value_type;
        global_r.has_core_type = explicit_type;
        // [complete checked valtype] mut ... section_end
        // [safe                   ] unsafe (could be section_end)
        //                           ^^ section_curr: commit the bounded decoded endpoint before checking mutability.
        section_curr = next;

        // [before_global_type ... valtype] mut ... (section_end)
        // [            safe              ] unsafe (could be the section_end)
        //                                  ^^ section_curr
        if(section_curr == section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_mut;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [before_global_type ... valtype mut] ... (section_end)
        // [            safe                  ] unsafe
        //                                 ^^ section_curr
        //
        // section_curr != section_end proves that the one-byte mutability read is in bounds.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte mut;
        ::std::memcpy(::std::addressof(mut), section_curr, sizeof(mut));
#if CHAR_BIT > 8
        mut = static_cast<decltype(mut)>(static_cast<::std::uint_least8_t>(mut) & 0xFFu);
#endif

        if(mut != 0u && mut != 1u) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u8 = mut;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_mut;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto& global_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<global_section_storage_t<Fs...>>(module_storage.sections)};
        // Mutability/value admission succeeded in the same actual definition/import traversal.
        bool const needs_reference_types{raw_type == 0x70u || raw_type == 0x6fu || raw_type == 0x69u};
        if(needs_reference_types && !global_section.requires_reference_types)
        { global_section.reference_types_diagnostic_value = explicit_type ? global_r.core_type.source_prefix : raw_type; }
        global_section.requires_reference_types |= needs_reference_types;
        global_section.requires_simd |= raw_type == 0x7bu;
        global_section.requires_function_references |= needs_function_references; // Includes imported global types.
        global_section.requires_gc |= explicit_type && ::uwvm2::validation::standard::wasm3::core3_value_requires_gc(
            global_r.core_type, ::std::addressof(known_type_section.core3_context), known_type_section.types.size());
        global_section.requires_exceptions |= explicit_type &&
            ::uwvm2::validation::standard::wasm3::core3_value_requires_exceptions(global_r.core_type);
        global_r.is_mutable = static_cast<bool>(mut);
        // section_curr points at the one-byte mutability flag already proven safe by section_curr != section_end and validated above.
        // Pointer move: advance to the first byte after the checked global type.
        ++section_curr;

        // [before_global_type ... valtype mut] tail ... (section_end)
        // [              safe                ] unsafe (could be the section_end)
        //                                      ^^ section_curr
        return section_curr;
    }

    /// @brief Parse a wasm1.1 global type from an import descriptor.
    /// @details Imports share the same global-type parser as local global definitions.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type & global_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        return global_section_global_handler(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>>{},
                                             global_r,
                                             module_storage,
                                             section_curr,
                                             section_end,
                                             err,
                                             fs_para);
    }

    /// @brief Parse and validate a wasm1.1 global initializer expression.
    /// @details Supports MVP constants plus reference and v128 constants when their subfeatures are enabled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* parse_and_check_global_expr_valid(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type const& global_r,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>& global_expr,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        bool allow_defined_globals = true,
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject subject =
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::global_type) UWVM_THROWS
    {
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 3uz);
        // importdesc has at least four buckets by static_assert; bucket 3 is the global-import bucket.
        auto const& imported_global{importsec.importdesc.index_unchecked(3uz)};
        auto const imported_global_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global.size())};

        auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
        // importdesc bucket 0 is the function-import bucket.
        auto const imported_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};
        auto const defined_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};
        auto const all_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_func_size + defined_func_size)};

        // [before_expr] opcode ... end ... (section_end)
        // [safe       ] unsafe (could be the section_end)
        //               ^^ section_curr; begin records this bounded cursor, without dereferencing it.
        global_expr.begin = section_curr;
        ::std::size_t constant_stack_depth{};
        // Accumulate in this existing typed decode; publish only after its
        // final stack/result checks succeed. No opcode is decoded twice.
        bool requires_extended_const{};
        unsigned extended_const_value{};
        // Host scalar evidence from this one typed decode. Do not publish an
        // instruction requirement until terminal/result validation succeeds.
        ::uwvm2::parser::wasm::base::constant_expression_opcode_requirements opcode_requirements{};
        using core_type = ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type;
        using core_kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        using core_heap = ::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
        ::uwvm2::utils::container::vector<core_type> constant_types{};
        auto const& const_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        auto const& declared_globals{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            global_section_storage_t<Fs...>>(module_storage.sections).local_globals};

        using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        auto const type_matches{[&](core_type actual, core_type expected) constexpr noexcept
        {
            if(actual.kind != expected.kind) { return false; }
            return actual.kind != core_kind::reference ||
                ::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::reference_matches(actual, expected,
                    typesec.owned_signatures, ::std::addressof(typesec.core3_context));
        }};
        auto const expression_error{[&](::std::byte const* at,
            ::uwvm2::parser::wasm::base::wasm_parse_error_code code) UWVM_THROWS
        {
            err.err_curr = at;
            err.err_code = code;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }};

        for(;;)
        {
            // [before_expr ...] opcode ... expr_tail ... end ... (section_end)
            // [      safe     ] unsafe (could be the section_end)
            //                   ^^ section_curr
            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_terminator_not_found;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [before_expr ... opcode] ... expr_tail ... end ... (section_end)
            // [      safe            ] unsafe
            //                  ^^ section_curr
            //
            // section_curr != section_end proves that reading the one-byte opcode is in bounds.
            ::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type opcode;
            ::std::memcpy(::std::addressof(opcode), section_curr, sizeof(opcode));
#if CHAR_BIT > 8
            opcode &= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFFu);
#endif

            if(opcode ==
               static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::end))
            {
                // section_curr points at the one-byte end opcode already proven safe by the opcode read.
                // Pointer move: advance to the first byte after the checked terminator.
                ++section_curr;
                break;
            }

            // Core 3 GC constructors are constant instructions in their own
            // right. They may consume earlier constant operands even when the
            // extended-const feature is disabled (for example, i32.const
            // followed by ref.i31). Gate arithmetic and defined global.get at
            // their opcode handlers below; the final stack check still rejects
            // a sequence that leaves more than one value.

            auto const integer_width{::uwvm2::validation::standard::wasm3::constant_integer_width(opcode)};
            if(integer_width != 0u)
            {
                if(const_para.disable_extended_const) [[unlikely]]
                {
                    // [before_expr ... opcode] tail ... (section_end)
                    // [safe                  ] unsafe
                    //                  ^^ section_curr; copy this checked address into the diagnostic.
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm3_extended_const_disabled;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                auto const expected{integer_width == 32u ? core_kind::i32 : core_kind::i64};
                // Core 3 constructors may consume numeric leaves inside a reference expression.
                // Check the two actual operand types; the final result is checked at end.
                if(constant_types.size() < 2uz ||
                   constant_types.back_unchecked().kind != expected ||
                   constant_types.index_unchecked(constant_types.size() - 2uz).kind != expected) [[unlikely]]
                {
                    // [before_expr ... opcode] tail ... (section_end)
                    // [safe                  ] unsafe
                    //                  ^^ section_curr; copy this checked address into the diagnostic.
                    err.err_curr = section_curr;
                    err.err_selectable.u8arr[0] = static_cast<unsigned char>(global_r.type);
                    err.err_selectable.u8arr[1] = static_cast<unsigned char>(
                        integer_width == 32u ? value_type::i32 : value_type::i64);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                if(!requires_extended_const)
                { requires_extended_const = true; extended_const_value = static_cast<unsigned>(opcode); }
                constant_types.pop_back_unchecked();
                --constant_stack_depth;
                global_expr.opcodes.emplace_back(
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{},
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(opcode));
                // [before_expr ... arithmetic] next ... (section_end)
                // [safe                      ] unsafe (could be the section_end)
                //                  ^^ section_curr; the opcode read proved one byte available.
                ++section_curr;
                // [before_expr ... arithmetic] next ... (section_end)
                // [safe                      ] unsafe (could be the section_end)
                //                              ^^ section_curr
                continue;
            }
            ++constant_stack_depth;

            switch(opcode)
            {
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const):
                {


                    // section_curr points at the one-byte i32.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... opcode] i32_leb ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ section_curr
                    //
                    // parse_by_scan bounds-checks the LEB128 immediate against section_end.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 value;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(value))};
                    if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                    }

                    // [before_expr ... opcode i32_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                         ^^ section_curr

                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the i32 LEB128 immediate, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked i32 LEB128 immediate.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... opcode i32_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                                      ^^ section_curr
                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.i32 = value},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const);
                    constant_types.push_back({core_kind::i32});
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const):
                {


                    // section_curr points at the one-byte i64.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... opcode] i64_leb ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                         ^^ section_curr

                    // parse_by_scan bounds-checks the LEB128 immediate against section_end.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 value;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(value))};
                    if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                    }

                    // [before_expr ... opcode i64_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                         ^^ section_curr

                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the i64 LEB128 immediate, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked i64 LEB128 immediate.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... opcode i64_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                                      ^^ section_curr

                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.i64 = value},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const);
                    constant_types.push_back({core_kind::i64});
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const):
                {


                    // section_curr points at the one-byte f32.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the fixed-width f32 payload.
                    ++section_curr;

                    // [before_expr ... opcode] f32_payload[4] ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ section_curr
                    if(static_cast<::std::size_t>(section_end - section_curr) < 4uz) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [before_expr ... opcode f32_payload[4]] ... expr_tail ... end ... (section_end)
                    // [         safe                        ] unsafe
                    //                         ^^ section_curr
                    // The length check above proves [section_curr, section_curr + 4) is safe inside the current section.
                    ::std::uint_least32_t raw;
                    ::std::memcpy(::std::addressof(raw), section_curr, 4uz);
                    raw = ::fast_io::little_endian(raw);
                    // Pointer move: advance by the 4 bytes proven safe by the fixed-width payload check.
                    section_curr += 4uz;

                    // [before_expr ... opcode f32_payload[4]] expr_tail ... end ... (section_end)
                    // [                 safe                ] unsafe (could be the section_end)
                    //                                        ^^ section_curr

                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.f32 = {}},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const);
                    ::std::memcpy(::std::addressof(global_expr.opcodes.back_unchecked().storage.f32), ::std::addressof(raw), sizeof(raw));
                    constant_types.push_back({core_kind::f32});
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const):
                {


                    // section_curr points at the one-byte f64.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the fixed-width f64 payload.
                    ++section_curr;

                    // [before_expr ... opcode] f64_payload[8] ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ section_curr
                    if(static_cast<::std::size_t>(section_end - section_curr) < 8uz) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [before_expr ... opcode f64_payload[8]] ... expr_tail ... end ... (section_end)
                    // [         safe                        ] unsafe
                    //                         ^^ section_curr
                    // The length check above proves [section_curr, section_curr + 8) is safe inside the current section.
                    ::std::uint_least64_t raw;
                    ::std::memcpy(::std::addressof(raw), section_curr, 8uz);
                    raw = ::fast_io::little_endian(raw);
                    // Pointer move: advance by the 8 bytes proven safe by the fixed-width payload check.
                    section_curr += 8uz;

                    // [before_expr ... opcode f64_payload[8]] expr_tail ... end ... (section_end)
                    // [                 safe                ] unsafe (could be the section_end)
                    //                                        ^^ section_curr
                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.f64 = {}},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const);
                    ::std::memcpy(::std::addressof(global_expr.opcodes.back_unchecked().storage.f64), ::std::addressof(raw), sizeof(raw));
                    constant_types.push_back({core_kind::f64});
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get):
                {
                    // section_curr points at the one-byte global.get opcode already proven safe by the opcode read.
                    // Pointer move: advance to the globalidx LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... global.get] globalidx ... expr_tail ... end ... (section_end)
                    // [          safe            ] unsafe (could be the section_end)
                    //                             ^^ section_curr
                    //
                    // parse_by_scan bounds-checks the LEB128 global index before imported-global lookup.
                    wasm_u32 global_idx;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(global_idx))};
                    if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                    }

                    // [before_expr ... global.get globalidx ...] expr_tail ... end ... (section_end)
                    // [                  safe                  ] unsafe (could be the section_end)
                    //                             ^^ section_curr

                    if(global_idx >= imported_global_size &&
                       (!allow_defined_globals || const_para.disable_extended_const ||
                        static_cast<::std::size_t>(global_idx - imported_global_size) >= declared_globals.size())) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32arr[0] = imported_global_size;
                        err.err_selectable.u32arr[1] = global_idx;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_illegal_imported_global;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // Bounds above select either an imported global or an already appended local.
                    // The global currently being parsed has not been appended: self/forward references fail.
                    auto const& curr_imported_global{global_idx < imported_global_size
                        ? imported_global.index_unchecked(global_idx)->imports.storage.global
                        : declared_globals.index_unchecked(global_idx - imported_global_size).global};


                    constant_types.push_back(::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(curr_imported_global));
                    if(curr_imported_global.is_mutable) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u32 = global_idx;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_mutable_imported_global;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    if(global_idx >= imported_global_size && !requires_extended_const)
                    { requires_extended_const = true; extended_const_value = static_cast<unsigned>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get); }
                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the globalidx LEB128, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked globalidx.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... global.get globalidx ...] expr_tail ... end ... (section_end)
                    // [                  safe                  ] unsafe (could be the section_end)
                    //                                            ^^ section_curr

                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.imported_global_idx = global_idx},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD0u):
                {
                    auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                    if(para.disable_reference_types) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required.value = 0xD0u;
                        err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types;
                        err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    // section_curr points at the one-byte ref.null opcode already proven safe by the opcode read.
                    // Pointer move: advance to the reference-type immediate.
                    ++section_curr;

                    // [before_expr ... ref.null] reftype ... expr_tail ... end ... (section_end)
                    // [          safe          ] unsafe (could be the section_end)
                    //                           ^^ section_curr

                    auto const heap{::uwvm2::parser::wasm::standard::wasm1p1::features::parse_core3_ref_null_heap(
                        section_curr, section_end, module_storage, fs_para, err)};
                    auto const raw_ref{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(heap.carrier)};
                    if(raw_ref != 0x70u && raw_ref != 0x6fu && raw_ref != 0x69u) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_reference_type.value = raw_ref;
                        err.err_selectable.wasm1p1_reference_type.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [ref.null][complete heap] next expression opcode ... section_end
                    // [safe                  ] unsafe (could be section_end)
                    //                          ^^ section_curr: decoder committed the checked immediate exactly once.
                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.ref_null_heap = heap.heap.code},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD0u));
                    constant_types.push_back({core_kind::reference, heap.heap, true});
                    // Heap/stack checks above succeeded. Classify the already
                    // decoded heap only; no byte is read and no cursor moves.
                    using namespace ::uwvm2::parser::wasm::base;
                    record_constant_expression_opcode_requirement(opcode_requirements.reference_types, 0xD0u, subject);
                    core_type const decoded_reference{core_kind::reference, heap.heap, true};
                    bool const requires_gc{::uwvm2::validation::standard::wasm3::core3_value_requires_gc(
                        decoded_reference, ::std::addressof(typesec.core3_context), typesec.types.size())};
                    if(requires_gc) { record_constant_expression_opcode_requirement(opcode_requirements.gc, 0xD0u, subject); }
                    if(::uwvm2::validation::standard::wasm3::core3_value_requires_exceptions(decoded_reference))
                    { record_constant_expression_opcode_requirement(opcode_requirements.exceptions, 0xD0u, subject); }
                    // The successful heap decoder proved an indexed non-GC
                    // heap names a function, including its actual legacy fallback.
                    if(heap.heap.is_defined() && !requires_gc)
                    { record_constant_expression_opcode_requirement(opcode_requirements.function_references, 0xD0u, subject); }
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD2u):
                {
                    auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                    if(para.disable_reference_types) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required.value = 0xD2u;
                        err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types;
                        err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // section_curr points at the one-byte ref.func opcode already proven safe by the opcode read.
                    // Pointer move: advance to the funcidx LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... ref.func] funcidx ... expr_tail ... end ... (section_end)
                    // [         safe           ] unsafe (could be the section_end)
                    //                           ^^ section_curr

                    // parse_by_scan bounds-checks the LEB128 funcidx before index validation.
                    wasm_u32 func_idx;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(func_idx))};
                    if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                    }

                    // [before_expr ... ref.func funcidx ...] expr_tail ... end ... (section_end)
                    // [                safe                ] unsafe (could be the section_end)
                    //                           ^^ section_curr

                    if(func_idx >= all_func_size) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_func_index_exceeds_maxvul.idx = func_idx;
                        err.err_selectable.wasm1p1_func_index_exceeds_maxvul.maxval = all_func_size;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_init_ref_func_index_exceeds_maxvul;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    ::std::size_t function_type_index{};
                    if(func_idx < imported_func_size)
                    {
                        // Bucket zero and func_idx bounds were checked before this lookup.
                        auto const* const imported_type{
                            importsec.importdesc.index_unchecked(0uz).index_unchecked(func_idx)->imports.storage.function};
                        bool found{};
                        for(::std::size_t candidate{}; candidate != typesec.types.size(); ++candidate)
                        {
                            // candidate is bounded by types.size() before forming an address.
                            if(imported_type == ::std::addressof(typesec.types.index_unchecked(candidate)))
                            { function_type_index = candidate; found = true; break; }
                        }
                        if(!found) [[unlikely]] { expression_error(section_curr,
                            ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index); }
                    }
                    else
                    {
                        // func_idx < all_func_size proves this defined-function index.
                        function_type_index = funcsec.funcs.index_unchecked(func_idx - imported_func_size);
                    }
                    // Both legacy and Core 3 decoders populate the flat type table;
                    // only the Core 3 decoder owns a parallel signature graph.
                    // The parsed function section already proved this is a
                    // function-type slot, so the common flat bound is sufficient.
                    if(function_type_index >= typesec.types.size()) [[unlikely]]
                    { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index); }
                    constant_types.push_back({core_kind::reference,
                        {static_cast<::std::int_least64_t>(function_type_index)}, false});
                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the funcidx LEB128, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked funcidx.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... ref.func funcidx ...] expr_tail ... end ... (section_end)
                    // [                safe                ] unsafe (could be the section_end)
                    //                                      ^^ section_curr
                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.ref_func_idx = func_idx},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD2u));
                    // The existing ref.func handler and index/type checks require
                    // reference-types, not the independent function-reference proposal.
                    ::uwvm2::parser::wasm::base::record_constant_expression_opcode_requirement(
                        opcode_requirements.reference_types, 0xD2u, subject);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFDu):
                {
                    auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                    if(para.disable_simd) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required.value = 0xFDu;
                        err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd;
                        err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }


                    // section_curr points at the one-byte SIMD prefix already proven safe by the opcode read.
                    // Pointer move: advance to the SIMD subopcode LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... simd_prefix] simd_subopcode ... v128_payload[16] ... end ... (section_end)
                    // [            safe          ] unsafe (could be the section_end)
                    //                              ^^ section_curr
                    //
                    // parse_by_scan bounds-checks the SIMD subopcode LEB128 against section_end.
                    wasm_u32 simd_subopcode;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(simd_subopcode))};
                    if(perr != ::fast_io::parse_code::ok || simd_subopcode != static_cast<wasm_u32>(0x0Cu)) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_instruction;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr == ::fast_io::parse_code::ok ? ::fast_io::parse_code::invalid : perr);
                    }
                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the SIMD subopcode LEB128, and next marks the v128 payload.
                    // Pointer move: advance section_curr to the fixed-width v128 payload.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... simd_prefix simd_subopcode ...] v128_payload[16] ... end ... (section_end)
                    // [                     safe                     ] unsafe (could be the section_end)
                    //                                                  ^^ section_curr

                    if(static_cast<::std::size_t>(section_end - section_curr) <
                       sizeof(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_v128_storage_t)) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [before_expr ... simd_prefix simd_subopcode v128_payload[16]] expr_tail ... end ... (section_end)
                    // [                              safe                         ] unsafe (could be the section_end)
                    //                                             ^^ section_curr

                    // The length check above proves [section_curr, section_curr + 16) is safe inside the current section.
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_v128_storage_t v128_value{};
                    ::std::memcpy(::std::addressof(v128_value), section_curr, sizeof(v128_value));
                    // Pointer move: advance by the 16 bytes proven safe by the fixed-width payload check.
                    section_curr += sizeof(v128_value);

                    // [before_expr ... simd_prefix simd_subopcode v128_payload[16]] expr_tail ... end ... (section_end)
                    // [                              safe                         ] unsafe (could be the section_end)
                    //                                                               ^^ section_curr
                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.v128 = v128_value},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xFDu));
                    constant_types.push_back({core_kind::v128});
                    // The subopcode and complete 16-byte payload were checked
                    // above; retain their opcode without reading them again.
                    ::uwvm2::parser::wasm::base::record_constant_expression_opcode_requirement(
                        opcode_requirements.simd, 0xFDu, subject);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFBu):
                {
                    if(const_para.disable_gc) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required = {
                            .value = 0xFBu,
                            .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
                            .subject = subject};
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    // [before_expr ... 0xfb] subopcode ... end ... (section_end)
                    // [         safe         ] unsafe (could be section_end)
                    //                          ^^ section_curr: the one-byte prefix was checked above.
                    ++section_curr;
                    // [before_expr ... 0xfb] subopcode ... end ... (section_end)
                    // [         safe         ] unsafe (could be section_end)
                    //                          ^^ section_curr: bounded LEB decoding begins here.
                    auto const read_u32{[&]() UWVM_THROWS -> wasm_u32
                    {
                        wasm_u32 value{};
                        // [safe prior opcode bytes] immediate ... (section_end)
                        // [safe                  ] unsafe (could be section_end)
                        //                         ^^ section_curr; parse_by_scan bounds the entire LEB.
                        auto const [next, perr]{::fast_io::parse_by_scan(
                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                            ::fast_io::mnp::leb128_get(value))};
                        if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                        }
                        // [safe prior bytes immediate] next ... (section_end)
                        // [safe                      ] unsafe (could be section_end)
                        //             ^^ section_curr before move; next is inside [section_curr, section_end].
                        section_curr = reinterpret_cast<::std::byte const*>(next);
                        // [safe prior bytes immediate] next ... (section_end)
                        // [safe                      ] unsafe (could be section_end)
                        //                              ^^ section_curr after bounded move.
                        return value;
                    }};
                    auto const subopcode{read_u32()};
                    if(subopcode != 0u && subopcode != 1u && subopcode != 6u &&
                       subopcode != 7u && subopcode != 8u && subopcode != 26u &&
                       subopcode != 27u && subopcode != 28u) [[unlikely]]
                    { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_instruction); }
                    wasm_u32 typeidx{}, count{};
                    if(subopcode <= 8u)
                    {
                        typeidx = read_u32();
                        if(subopcode == 8u) { count = read_u32(); }
                    }
                    auto const pop_expected{[&](core_type expected) UWVM_THROWS
                    {
                        if(constant_types.empty() || !type_matches(constant_types.back_unchecked(), expected)) [[unlikely]]
                        { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch); }
                        constant_types.pop_back_unchecked();
                    }};
                    if(subopcode == 28u)
                    {
                        pop_expected({core_kind::i32});
                        constant_types.push_back({core_kind::reference, {static_cast<::std::int_least64_t>(core_heap::i31)}, false});
                    }
                    else if(subopcode == 26u || subopcode == 27u)
                    {
                        auto const input{constant_types.empty() ? core_type{} : constant_types.back_unchecked()};
                        pop_expected({core_kind::reference, {static_cast<::std::int_least64_t>(
                            subopcode == 26u ? core_heap::extern_ : core_heap::any)}, true});
                        constant_types.push_back({core_kind::reference, {static_cast<::std::int_least64_t>(
                            subopcode == 26u ? core_heap::any : core_heap::extern_)}, input.nullable});
                    }
                    else
                    {
                        ::uwvm2::parser::wasm::standard::wasm3::type::sub_type const* definition{};
                        if(typeidx < typesec.core3_recursive_types.type_count)
                        {
                            for(auto const& group: typesec.core3_recursive_types.groups)
                            {
                                auto const flat{static_cast<::std::uint_least64_t>(typeidx)};
                                if(flat >= group.first_type_index &&
                                   flat - group.first_type_index < group.types.size())
                                {
                                    // [group.types begin, group.types end) contains this checked flat index.
                                    definition = ::std::addressof(group.types.index_unchecked(
                                        static_cast<::std::size_t>(flat - group.first_type_index)));
                                    break;
                                }
                            }
                        }
                        if(definition == nullptr ||
                           (subopcode <= 1u && definition->kind != ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind::struct_) ||
                           (subopcode >= 6u && definition->kind != ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind::array)) [[unlikely]]
                        { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index); }
                        auto const field_value{[&](::uwvm2::parser::wasm::standard::wasm3::type::field_type const& field) constexpr noexcept
                        {
                            return field.storage.packed == ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind::none ?
                                field.storage.value : core_type{core_kind::i32};
                        }};
                        if(subopcode == 0u)
                        {
                            for(::std::size_t i{definition->fields.size()}; i != 0uz; --i)
                            {
                                // i>0 and i<=fields.size() prove field i-1 exists.
                                pop_expected(field_value(definition->fields.index_unchecked(i - 1uz)));
                            }
                        }
                        else if(subopcode == 1u || subopcode == 7u)
                        {
                            for(auto const& field: definition->fields)
                            {
                                if(field.storage.packed == ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind::none &&
                                   field.storage.value.kind == core_kind::reference && !field.storage.value.nullable) [[unlikely]]
                                { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch); }
                            }
                        }
                        if(subopcode == 6u)
                        {
                            if(definition->fields.size() != 1uz) [[unlikely]]
                            { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index); }
                            pop_expected({core_kind::i32});
                            pop_expected(field_value(definition->fields.front_unchecked()));
                        }
                        else if(subopcode == 7u) { pop_expected({core_kind::i32}); }
                        else if(subopcode == 8u)
                        {
                            if(definition->fields.size() != 1uz || static_cast<::std::size_t>(count) > constant_types.size()) [[unlikely]]
                            { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch); }
                            for(::std::size_t i{}; i != count; ++i)
                            { pop_expected(field_value(definition->fields.front_unchecked())); }
                        }
                        constant_types.push_back({core_kind::reference, {static_cast<::std::int_least64_t>(typeidx)}, false});
                    }
                    constant_stack_depth = constant_types.size();
                    global_expr.opcodes.emplace_back(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{
                            .gc_immediate = {subopcode, typeidx, count}},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xFBu));
                    // All actual allowed constructor/conversion subopcodes and
                    // their typed operands succeeded in this original handler.
                    ::uwvm2::parser::wasm::base::record_constant_expression_opcode_requirement(
                        opcode_requirements.gc, 0xFBu, subject);
                    break;
                }
                [[unlikely]] default:
                {
                    /// @warning Extension point: new global const-expression opcodes need storage, type checking, and initializer evaluation before this
                    /// fallback.
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_instruction;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        if(constant_stack_depth != 1uz || constant_types.size() != 1uz) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = constant_stack_depth == 0uz
                ? ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_empty
                : ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if(!type_matches(constant_types.front_unchecked(),
                         ::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(global_r))) [[unlikely]]
        { expression_error(section_curr, ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch); }

        // [before_expr ... expr ... end] tail ... (section_end)
        // [            safe            ] unsafe (could be the section_end)
        //                                ^^ section_curr
        global_expr.end = section_curr;
        if(requires_extended_const)
        {
            auto& requirements{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                global_section_storage_t<Fs...>>(module_storage.sections)};
            if(!requirements.constant_expressions_require_extended_const)
            {
                requirements.constant_expressions_require_extended_const = true;
                requirements.extended_const_diagnostic_value = extended_const_value;
                requirements.extended_const_diagnostic_subject = subject;
            }
        }
        // Both the terminator and the exact single declared result succeeded.
        // Publish first real opcode/site records, including expressions whose
        // GC/intermediate types were converted to a legacy final carrier.
        auto& opcode_metadata{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            global_section_storage_t<Fs...>>(module_storage.sections).constant_expression_opcode_requirements};
        ::uwvm2::parser::wasm::base::merge_constant_expression_opcode_requirements(opcode_metadata, opcode_requirements);
        // [complete checked expr ... end] next ... section_end
        // [safe                        ] ^^ section_curr is still the checked endpoint; no cursor advance/read here.
        return section_curr;
    }

    /// @brief Parse Core 3's optional table initializer without extending the import table-type grammar.
    /// @see https://webassembly.github.io/spec/core/binary/modules.html#table-section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* table_section_table_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type& table_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [before_table] prefix/type ... (section_end)
        // [safe        ] unsafe (could be the section_end)
        //                ^^ section_curr; short-circuiting proves the byte read is bounded.
        bool const explicit_initializer{section_curr != section_end && *section_curr == ::std::byte{0x40u}};
        if(explicit_initializer)
        {
            auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
            if(para.disable_table_initializer) [[unlikely]]
            {
                // [before_table 0x40] reserved ... (section_end)
                // [safe             ] unsafe (could be the section_end)
                //               ^^ section_curr; explicit_initializer already proved this prefix byte exists.
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm3_table_initializer_disabled;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            if(section_end - section_curr < 2 || section_curr[1] != ::std::byte{0}) [[unlikely]]
            {
                // [before_table 0x40] reserved ... (section_end)
                // [safe             ] unsafe (could be the section_end)
                //               ^^ section_curr; record the bounded prefix without reading the reserved byte.
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_instruction;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            // [before_table 0x40 0x00] tabletype expr ... (section_end)
            // [safe                 ] unsafe (could be the section_end)
            //               ^^ section_curr; the subtraction check proves both prefix bytes exist.
            section_curr += 2;
            // [before_table 0x40 0x00] tabletype expr ... (section_end)
            // [safe                 ] unsafe (could be the section_end)
            //                         ^^ section_curr
        }
        // The type scanner bounds-checks the type and limits; its result is in [section_curr, section_end].
        section_curr = scan_extended_table_type(sec_adl, table_r, module_storage, section_curr, section_end, err, fs_para);
        // [before_table ... tabletype] expr/next_table ... (section_end)
        // [safe                      ] unsafe (could be the section_end)
        //                              ^^ section_curr
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> expr{};
        if(explicit_initializer)
        {
            // Tables are decoded before globals. Consequently this context contains imported globals only,
            // matching Core 3 C' (table initializers cannot read local globals, even with extended-const enabled).
            ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type const expected{
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(table_r.reftype), false,
                table_r.core_type, table_r.has_core_type};
            section_curr = parse_and_check_global_expr_valid(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>>{},
                expected, expr, module_storage, section_curr, section_end, err, fs_para, false,
                ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type);
            // [before_table ... tabletype expr end] next_table ... (section_end)
            // [safe                              ] unsafe (could be the section_end)
            //                                      ^^ section_curr; returned by the bounded expression decoder.
        }
        else if(table_r.has_core_type && !table_r.core_type.nullable) [[unlikely]]
        {
            // The implicit table initializer is ref.null. A non-nullable element type requires an explicit expression.
            err.err_curr = section_curr; // The bounded type scanner may have returned section_end; diagnostics never dereference it.
            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(table_r.reftype));
            err.err_selectable.u8arr[1] = err.err_selectable.u8arr[0];
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        auto& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};
        // The original typed initializer has fully succeeded before this scalar publication.
        tablesec.requires_table_initializer |= explicit_initializer;
        tablesec.initializers.push_back(::std::move(expr));
        return section_curr;
    }

}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
