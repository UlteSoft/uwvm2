/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_types.h"
# include <array>
# include <memory>
# include <mutex>
# include <new>
# include <utility>
# include <vector>
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
            ::std::size_t ranges{}, physical{no_record}; bool ambiguous_physical{};
            // Parents must precede children. Compute each full ancestry depth
            // once, retaining validation of every inactive record and cap.
            ::std::vector<::std::size_t> ancestry_depth(scopes.size());
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
                if(scope.parent != no_record)
                {
                    auto const depth{ancestry_depth[scope.parent]};
                    if(depth >= cap.max_depth) { return scope_path_error::limit_exceeded; }
                    ancestry_depth[i] = depth + 1u;
                }
                // Subtract-before-add avoids wrapping the sum, including host
                // callers supplying extreme caps. No allocation follows failure.
                if(!budget::charge(scope.ranges.size(), cap.max_ranges, ranges)) { return scope_path_error::limit_exceeded; }
                bool active{};
                for(auto const& range : scope.ranges)
                {
                    if(range.begin >= range.end) { return scope_path_error::malformed; }
                    active |= range.begin <= pc && pc < range.end;
                }
                if(active && scope.kind == scope_kind::subprogram && scope.concrete)
                {
                    if(physical != no_record) { ambiguous_physical = true; }
                    else { physical = i; }
                }
            }
            // Metadata validation keeps precedence over ambiguous selection.
            if(ambiguous_physical) { return scope_path_error::ambiguous; }
            auto const contains{[&](::std::size_t i) noexcept
            {
                // [owned immutable scope span] end
                // [safe                      ] every caller bounded i; no pointer advance.
                for(auto const& range : scopes[i].ranges)
                { if(range.begin <= pc && pc < range.end) { return true; } }
                return false;
            }};
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
            // Resolve the nearest active parent only for concrete inline
            // instances covering this PC. Repeated path levels never rescan
            // unrelated CUs or walk these same lexical ancestors again.
            ::std::vector<::std::pair<::std::size_t,::std::size_t>> active_inline{};
            for(::std::size_t i{}; i != scopes.size(); ++i)
            {
                if(scopes[i].kind != scope_kind::inline_subprogram || !scopes[i].concrete || !contains(i)) { continue; }
                auto const parent{nearest_active_parent(i)};
                if(parent != no_record) { active_inline.emplace_back(i,parent); }
            }
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
                for(auto const& candidate : active_inline)
                {
                    if(candidate.second != current) { continue; }
                    if(next != no_record) { return scope_path_error::ambiguous; }
                    next = candidate.first; // bounded owned concrete metadata index only.
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
    // An independent immutable copy owns every field used by scope selection.
    // Only scalar PCs/caps and at most sixteen successful metadata paths are
    // cached. There is no guest value, read lease, runtime frame or source token.
    class owned_scope_paths final
    {
        ::std::vector<scope_record> scopes_{};
        struct entry
        {
            bool ready{};::std::uint64_t pc{};scope_path_limits cap{};concrete_scope_path path{};
        };
        mutable ::std::mutex mutex_{};
        mutable ::std::array<entry,16u> paths_{};
        mutable ::std::size_t next_{};
        owned_scope_paths() = default;
    public:
        owned_scope_paths(owned_scope_paths const&)=delete;
        owned_scope_paths& operator=(owned_scope_paths const&)=delete;
        owned_scope_paths(owned_scope_paths&&)=delete;
        owned_scope_paths& operator=(owned_scope_paths&&)=delete;
        [[nodiscard]] ::std::span<scope_record const> scopes() const noexcept { return scopes_; }
        // Refuse an oversized or recursively nested metadata copy. Ineligible
        // indexes retain the original fully checked uncached query path.
        [[nodiscard]] static bool copy(::std::span<scope_record const> scopes,
            ::std::unique_ptr<owned_scope_paths>& out) noexcept
        {
            out.reset();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                constexpr ::std::size_t maximum{8u*1024u*1024u};
                if(scopes.size()>65536u) { return false; }
                ::std::size_t bytes{},ranges{};
                auto const charge{[&](::std::size_t count,::std::size_t extent)
                { return extent!=0u && count<=maximum/extent && budget::charge(count*extent,maximum,bytes); }};
                if(!charge(scopes.size(),sizeof(scope_record))) { return false; }
                for(auto const& scope:scopes)
                {
                    if(!charge(scope.name.size(),1u) || !charge(scope.call_file.size(),1u) ||
                       !budget::charge(scope.ranges.size(),65536u,ranges) ||
                       !charge(scope.ranges.size(),sizeof(code_range)) ||
                       !charge(scope.frame_base.size(),sizeof(location_record))) { return false; }
                    for(auto const& location:scope.frame_base)
                    {
                        if(!charge(location.plan.pieces.size(),sizeof(location_piece))) { return false; }
                        for(auto const& piece:location.plan.pieces)
                        { if(!piece.atom.pieces.empty()) { return false; } }
                    }
                }
                ::std::unique_ptr<owned_scope_paths> pending{new (::std::nothrow) owned_scope_paths{}};
                if(!pending) { return false; }
                pending->scopes_.assign(scopes.begin(),scopes.end());
                out=::std::move(pending);return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { out.reset();return false; }
#endif
        }
        [[nodiscard]] scope_path_error query(::std::span<scope_record const> scopes,::std::uint64_t pc,
            concrete_scope_path& out,scope_path_limits const& cap) const noexcept
        {
            out.clear();
            // Equal contents, size, names or DIE keys cannot substitute another
            // metadata owner. A raw span always gets the original validation.
            if(scopes.data()!=scopes_.data() || scopes.size()!=scopes_.size())
            { return query_concrete_scope_path(scopes,pc,out,cap); }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                ::std::unique_lock lock{mutex_,::std::try_to_lock};
                if(!lock.owns_lock()) { return query_concrete_scope_path(scopes,pc,out,cap); }
                for(auto const& saved:paths_)
                {
                    if(saved.ready && saved.pc==pc && saved.cap.max_scopes==cap.max_scopes &&
                       saved.cap.max_ranges==cap.max_ranges && saved.cap.max_depth==cap.max_depth &&
                       saved.cap.max_keys==cap.max_keys)
                    { out=saved.path;return scope_path_error::none; }
                }
                concrete_scope_path pending{};
                auto const status{query_concrete_scope_path(scopes,pc,pending,cap)};
                if(status!=scope_path_error::none) { return status; }
                if(pending.record_indices.size()<=64u && pending.physical_and_inline.size()<=64u)
                {
                    auto& saved{paths_[next_]};saved.ready=false;
                    saved.path=pending;saved.pc=pc;saved.cap=cap;saved.ready=true;
                    next_=(next_+1u)%paths_.size();
                }
                out=::std::move(pending);return scope_path_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { out.clear();return scope_path_error::allocation_failure; }
#endif
        }
    };
    [[nodiscard]] inline scope_path_error query_concrete_scope_path(::std::span<scope_record const> scopes,
        ::std::uint64_t pc,concrete_scope_path& out,scope_path_limits const& cap,
        owned_scope_paths const* owner) noexcept
    {
        return owner==nullptr ? query_concrete_scope_path(scopes,pc,out,cap) : owner->query(scopes,pc,out,cap);
    }

}
