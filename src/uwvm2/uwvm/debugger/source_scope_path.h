/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_types.h"
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class scope_path_error { none, unavailable, malformed, ambiguous, limit_exceeded, allocation_failure };
    struct scope_path_limits
    {
        ::std::size_t max_scopes{65536u}, max_ranges{65536u}, max_depth{64u}, max_keys{64u};
    };
    struct concrete_scope_path
    {
        // OWNED metadata only: physical subprogram first, then actual inline
        // instances outer-to-inner. Names/abstract origins never supply identity.
        ::std::vector<die_key> physical_and_inline{};
        // Checked indices into the SAME immutable input index. No record pointer
        // or source owner escapes. These integers grant no frame/read authority.
        ::std::vector<::std::size_t> record_indices{};
        void clear() noexcept { physical_and_inline.clear(); record_indices.clear(); }
    };
    // Pure bounded metadata selection, not a source/pause/generation credential.
    // A future production caller must obtain pc from an authenticated CURRENT
    // runtime source query and keep the actual immutable index/source owner live.
    // No LLVM/DWO/file loader, expression execution, frame/native pointer or
    // value access occurs here. Existing frozen display/value queries are NOT
    // changed by adding this component.
    [[nodiscard]] inline scope_path_error query_concrete_scope_path(::std::span<scope_record const> scopes,
        ::std::uint64_t pc, concrete_scope_path& out, scope_path_limits const& cap = {}) noexcept
    {
        out.clear();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            if(scopes.size() > cap.max_scopes) { return scope_path_error::limit_exceeded; }
            ::std::size_t ranges{};
            for(::std::size_t i{}; i != scopes.size(); ++i)
            {
                // [immutable scope records ... i ... end]
                // [safe                                ] i < size; no pointer advances.
                auto const& scope{scopes[i]};
                switch(scope.kind)
                { case scope_kind::compile_unit: case scope_kind::subprogram:
                  case scope_kind::inline_subprogram: case scope_kind::lexical_block: break;
                  default: return scope_path_error::malformed; }
                if(scope.parent != no_record && scope.parent >= i) { return scope_path_error::malformed; }
                auto parent{scope.parent};
                for(::std::size_t depth{}; parent != no_record; ++depth)
                {
                    if(depth == cap.max_depth) { return scope_path_error::limit_exceeded; }
                    if(parent >= i) { return scope_path_error::malformed; }
                    // [preceding parent record ...] end
                    // [safe                       ] parent < i < size; ancestry strictly decreases.
                    parent = scopes[parent].parent;
                }
                // Subtract-before-add avoids wrapping the sum, including host
                // callers supplying extreme caps. No allocation follows failure.
                if(!budget::charge(scope.ranges.size(), cap.max_ranges, ranges)) { return scope_path_error::limit_exceeded; }
                for(auto const& range : scope.ranges)
                { if(range.begin >= range.end) { return scope_path_error::malformed; } }
            }
            auto const contains{[&](::std::size_t i) noexcept
            {
                // [owned immutable scope span] end
                // [safe                      ] every caller bounded i; no pointer advance.
                for(auto const& range : scopes[i].ranges)
                { if(range.begin <= pc && pc < range.end) { return true; } }
                return false;
            }};
            ::std::size_t physical{no_record};
            for(::std::size_t i{}; i != scopes.size(); ++i)
            {
                if(scopes[i].kind != scope_kind::subprogram || !scopes[i].concrete || !contains(i)) { continue; }
                if(physical != no_record) { return scope_path_error::ambiguous; }
                physical = i; // owned scalar index only, not a native/frame pointer.
            }
            if(physical == no_record) { return scope_path_error::unavailable; }
            auto const nearest_active_parent{[&](::std::size_t i) noexcept
            {
                // [owned scope span] end; i is a checked inline candidate.
                // [safe            ] parent borrows no address and never advances a pointer.
                auto parent{scopes[i].parent};
                for(::std::size_t depth{}; parent != no_record; ++depth)
                {
                    if(depth == cap.max_depth || parent >= scopes.size()) { return no_record; }
                    auto const& scope{scopes[parent]};
                    if(scope.kind == scope_kind::subprogram || scope.kind == scope_kind::inline_subprogram) { return parent; }
                    // Explicit empty ranges are inactive. ONLY absent range
                    // attributes inherit; descriptive abstract origins do not.
                    if(scope.kind != scope_kind::lexical_block ||
                       ((scope.own_ranges_declared || !scope.ranges.empty()) && !contains(parent))) { return no_record; }
                    parent = scope.parent; // prechecked ancestry; scalar only.
                }
                return no_record;
            }};
            concrete_scope_path pending{};
            auto const append{[&](::std::size_t i)
            {
                if(pending.physical_and_inline.size() == cap.max_keys) { return scope_path_error::limit_exceeded; }
                auto const key{scopes[i].identity}; // copy a checked metadata key; no borrowed pointer escapes.
                for(auto const previous : pending.physical_and_inline)
                { if(previous == key) { return scope_path_error::malformed; } }
                pending.physical_and_inline.push_back(key);
                pending.record_indices.push_back(i);
                return scope_path_error::none;
            }};
            auto appended{append(physical)};
            if(appended != scope_path_error::none) { return appended; }
            ::std::size_t current{physical};
            for(::std::size_t depth{};; ++depth)
            {
                if(depth > cap.max_depth) { return scope_path_error::limit_exceeded; }
                ::std::size_t next{no_record};
                for(::std::size_t i{}; i != scopes.size(); ++i)
                {
                    if(scopes[i].kind != scope_kind::inline_subprogram || !scopes[i].concrete || !contains(i) ||
                       nearest_active_parent(i) != current) { continue; }
                    if(next != no_record) { return scope_path_error::ambiguous; }
                    next = i; // own concrete range, no abstract-origin PC/caller location.
                }
                if(next == no_record) { break; }
                appended = append(next);
                if(appended != scope_path_error::none) { return appended; }
                current = next; // strictly later checked descendant index; no pointer is modified.
            }
            out = ::std::move(pending); return scope_path_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out.clear(); return scope_path_error::allocation_failure; }
#endif
    }
}
