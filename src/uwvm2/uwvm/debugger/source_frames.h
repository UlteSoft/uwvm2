/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_scope_path.h"
# include <algorithm>
# include <new>
# include <string>
# include <string_view>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
// Cold bounded metadata only. Actual stopped/source/activation proof and
// selected-frame retirement belong to the controller/runtime, never this API.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_frames
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    enum class error { none, unavailable, malformed, bounds, limit_exceeded, allocation_failure };
    struct frame
    {
        dwarf::die_key identity{};
        ::std::size_t scope_index{dwarf::no_record};
        dwarf::scope_kind kind{};
        ::std::string name{};
    };
    struct limits { dwarf::scope_path_limits path{}; ::std::size_t max_string_bytes{65536u}; };
    // Frame 0 is innermost concrete inline instance, then enclosing inline
    // instances, then THIS physical function. Callers are deliberately absent:
    // an unwind label alone cannot authenticate that activation's local values.
    [[nodiscard]] inline error current_frames(::std::span<dwarf::scope_record const> scopes, ::std::uint64_t pc,
        ::std::vector<frame>& out, limits const& cap = {}) noexcept
    {
        out.clear();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            dwarf::concrete_scope_path selected{};
            auto const status{dwarf::query_concrete_scope_path(scopes, pc, selected, cap.path)};
            switch(status)
            {
                case dwarf::scope_path_error::none: break;
                case dwarf::scope_path_error::unavailable: return error::unavailable;
                case dwarf::scope_path_error::malformed: return error::malformed;
                case dwarf::scope_path_error::ambiguous: return error::malformed;
                case dwarf::scope_path_error::limit_exceeded: return error::limit_exceeded;
                case dwarf::scope_path_error::allocation_failure: return error::allocation_failure;
                default: return error::malformed;
            }
            if(selected.record_indices.empty() || selected.record_indices.size() != selected.physical_and_inline.size())
            { return error::malformed; }
            ::std::vector<frame> pending{}; ::std::size_t strings{};
            auto remaining{selected.record_indices.size()};
            while(remaining != 0u)
            {
                --remaining; // [safe] positive scalar extent before decrement; no pointer changes.
                // [owned checked record_indices/keys ... remaining ... end]
                // [safe                                                  ] both equal extents
                //  ^^ and remaining < size are proved BEFORE either subscript.
                auto const index{selected.record_indices[remaining]};
                if(index >= scopes.size()) { return error::malformed; }
                // [same immutable scope records ... checked index ... end]
                // [safe                                                  ] index < size.
                //  ^^ synchronous metadata borrow; only integers/owned name escape.
                auto const& scope{scopes[index]};
                if(scope.identity != selected.physical_and_inline[remaining] ||
                   (remaining == 0u ? scope.kind != dwarf::scope_kind::subprogram : scope.kind != dwarf::scope_kind::inline_subprogram))
                { return error::malformed; }
                if(!dwarf::budget::charge(scope.name.size(), cap.max_string_bytes, strings)) { return error::limit_exceeded; }
                pending.push_back({scope.identity, index, scope.kind, ::fast_io::concat_std(::std::string_view{scope.name})});
            }
            out = ::std::move(pending); return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out.clear(); return error::allocation_failure; }
#endif
    }
    struct language_context { ::std::uint64_t language{}; bool tinygo_producer{}, zig_producer{}; };
    [[nodiscard]] inline error current_language(::std::span<dwarf::scope_record const> scopes, ::std::uint64_t pc,
        ::std::size_t selected, language_context& out, limits const& cap = {}) noexcept
    {
        out = {}; ::std::vector<frame> frames{};
        auto const status{current_frames(scopes,pc,frames,cap)};
        if(status != error::none) { return status; }
        if(selected >= frames.size()) { return error::bounds; }
        auto const index{frames[selected].scope_index};
        if(index >= scopes.size() || scopes[index].identity != frames[selected].identity) { return error::malformed; }
        // Copy metadata from precisely the selected active physical/inline DIE.
        // A type/global name or a different frame cannot choose its language.
        auto const& scope{scopes[index]};
        out = {scope.language,scope.tinygo_producer,scope.zig_producer};
        return scope.language == 0u ? error::unavailable : error::none;
    }
    // Metadata cursor only. The controller must invalidate its actual selected
    // frame on resume/replacement/native trap/source retirement before using
    // this ordinal; these scalar helpers grant no ticket/read permission.
    [[nodiscard]] inline error move_up(::std::span<frame const> frames, ::std::size_t& selected, ::std::uint64_t count = 1u) noexcept
    {
        if(selected >= frames.size()) { return error::bounds; }
        if(count > frames.size() - 1u - selected) { return error::bounds; }
        selected += static_cast<::std::size_t>(count); // [safe] upper bound proved by subtraction BEFORE addition.
        return error::none;
    }
    [[nodiscard]] inline error move_down(::std::span<frame const> frames, ::std::size_t& selected, ::std::uint64_t count = 1u) noexcept
    {
        if(selected >= frames.size() || count > selected) { return error::bounds; }
        selected -= static_cast<::std::size_t>(count); // [safe] count <= selected BEFORE subtraction.
        return error::none;
    }
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_frame_variables
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    namespace frames = ::uwvm2::uwvm::debugger::source_frames;
    enum class error { none, unavailable, malformed, ambiguous, bounds, limit_exceeded, allocation_failure };
    struct selection
    {
        ::std::size_t variable_index{dwarf::no_record}, scope_index{dwarf::no_record};
        dwarf::die_key variable_identity{}, selected_scope_identity{};
    };
    struct limits
    {
        frames::limits frame_limits{};
        ::std::size_t max_variables{65536u}, max_types{65536u}, max_string_bytes{1024u * 1024u}, max_name_bytes{4096u};
    };
    // A selected inline frame sees its own declarations and currently active
    // lexical descendants; nested INLINE frames have their own variables and
    // cannot shadow the selected outer frame. Nonselected physical callers and
    // globals intentionally remain outside this limited selection contract.
    [[nodiscard]] inline error named(::std::span<dwarf::scope_record const> scopes,
        ::std::span<dwarf::variable_record const> variables, ::std::size_t type_count,
        ::std::uint64_t pc, ::std::size_t frame_ordinal, ::std::string_view name,
        selection& out, limits const& cap = {}) noexcept
    {
        out = {};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            if(name.empty() || name.size() > cap.max_name_bytes) { return error::unavailable; }
            if(variables.size() > cap.max_variables || type_count > cap.max_types) { return error::limit_exceeded; }
            ::std::vector<frames::frame> path{};
            switch(frames::current_frames(scopes, pc, path, cap.frame_limits))
            {
                case frames::error::none: break;
                case frames::error::unavailable: return error::unavailable;
                case frames::error::malformed: return error::malformed;
                case frames::error::bounds: return error::bounds;
                case frames::error::limit_exceeded: return error::limit_exceeded;
                case frames::error::allocation_failure: return error::allocation_failure;
                default: return error::malformed;
            }
            if(frame_ordinal >= path.size()) { return error::bounds; }
            // [owned checked concrete path ... frame_ordinal ... end]
            // [safe                                                ] ordinal < size BEFORE subscript.
            auto const selected_scope{path[frame_ordinal].scope_index};
            auto const selected_key{path[frame_ordinal].identity};
            if(selected_scope >= scopes.size()) { return error::malformed; }
            ::std::size_t strings{};
            for(auto const& variable : variables)
            {
                if(variable.scope >= scopes.size() || (variable.type != dwarf::no_record && variable.type >= type_count))
                { return error::malformed; }
                if(!dwarf::budget::charge(variable.name.size(), cap.max_string_bytes, strings) ||
                   !dwarf::budget::charge(variable.qualified_name.size(), cap.max_string_bytes, strings) ||
                   !dwarf::budget::charge(variable.declaration_file.size(), cap.max_string_bytes, strings))
                { return error::limit_exceeded; }
            }
            // Every scope/parent/range/enum was checked by current_frames on
            // the SAME immutable input. This walk accepts lexical blocks only
            // until the selected scope is reached; an intervening inline or
            // physical subprogram explicitly terminates visibility.
            auto const visible_depth{[&](::std::size_t scope, ::std::size_t& depth) noexcept
            {
                depth = 0u;
                for(;;)
                {
                    if(scope >= scopes.size() || depth == cap.frame_limits.path.max_depth) { return false; }
                    if(scope == selected_scope) { return true; }
                    // [same checked immutable scope span ... scope ... end]
                    // [safe                                               ] scope < size BEFORE borrow.
                    auto const& record{scopes[scope]};
                    if(record.kind != dwarf::scope_kind::lexical_block) { return false; }
                    if(record.own_ranges_declared || !record.ranges.empty())
                    {
                        bool active{};
                        for(auto const& range : record.ranges) { if(range.begin <= pc && pc < range.end) { active = true; break; } }
                        if(!active) { return false; } // explicit empty range is unavailable, never inherited.
                    }
                    if(record.parent == dwarf::no_record || record.parent >= scope) { return false; }
                    scope = record.parent; // [safe] bounded preceding metadata index; no pointer modification.
                    ++depth; // [safe] strict depth cap was checked BEFORE scalar increment.
                }
            }};
            auto const ancestor{[&](::std::size_t outer, ::std::size_t inner) noexcept
            {
                for(::std::size_t depth{}; depth != cap.frame_limits.path.max_depth; ++depth)
                {
                    if(inner >= scopes.size()) { return false; }
                    if(outer == inner) { return true; }
                    // [checked immutable scopes ... inner ... end]
                    // [safe                                      ] inner < size BEFORE parent borrow.
                    auto const parent{scopes[inner].parent};
                    if(parent == dwarf::no_record || parent >= inner) { return false; }
                    inner = parent; // [safe] strictly decreasing bounded scalar index; no pointer advance.
                }
                return false;
            }};
            ::std::size_t chosen{dwarf::no_record}; bool ambiguous{};
            for(::std::size_t i{}; i != variables.size(); ++i)
            {
                // [owned immutable variable records ... i ... end]
                // [safe                                          ] i < size BEFORE borrow.
                auto const& variable{variables[i]};
                if(variable.global || variable.declaration || (variable.name != name && variable.qualified_name != name)) { continue; }
                ::std::size_t depth{};
                if(!visible_depth(variable.scope, depth)) { continue; }
                if(chosen == dwarf::no_record) { chosen = i; continue; }
                // [owned variables ... chosen and i ... end]
                // [safe                                   ] both indices were selected by this bounded loop.
                auto const previous_scope{variables[chosen].scope};
                if(previous_scope == variable.scope) { ambiguous = true; }
                else if(ancestor(previous_scope, variable.scope)) { chosen = i; ambiguous = false; }
                else if(!ancestor(variable.scope, previous_scope))
                { return error::ambiguous; } // incomparable overlapping lexical branches must never win by depth.
            }
            if(chosen == dwarf::no_record) { return error::unavailable; }
            if(ambiguous) { return error::ambiguous; }
            // [immutable variable records ... checked chosen ... end]
            // [safe                                                ] chosen was set only by bounded loop.
            auto const identity{variables[chosen].identity};
            for(::std::size_t i{}; i != variables.size(); ++i)
            { if(i != chosen && variables[i].identity == identity) { return error::ambiguous; } }
            selection pending{}; pending.variable_index = chosen; pending.scope_index = selected_scope;
            pending.variable_identity = identity; pending.selected_scope_identity = selected_key;
            out = pending; return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return error::allocation_failure; }
#endif
    }
}
