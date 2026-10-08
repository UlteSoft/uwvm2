/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_types.h"
# include <string_view>
# include <fast_io.h>
# include <algorithm>
# include <memory>
# include <utility>
# include <limits>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_step
{
    // METADATA POLICY ONLY: nothing here authenticates an input, pauses/resumes
    // a guest, executes a plan or grants frame/local/memory/address access.
    // The controller must first verify the genuine source binding, captured
    // ticket/full location, code owner and actual per-function generation with
    // the runtime, then supply a concrete physical + outer-to-inner DIE path.
    // A line-only producer has no concrete DIE path. Keep that distinction
    // explicit; never manufacture a scope key from a function number or name.
    enum class mapping_kind { dwarf_scope, line_only };
    enum class policy { into, over, out };
    enum class destination_policy { none, until, advance };
    enum class position_status { mapped, unmapped, ambiguous, native, stale };
    enum class trace_status { missing, complete, truncated };
    enum class activation_relation { unknown, same, deeper, returned, tail_successor };
    // Bounded metadata comparison ONLY. Production must supply two owned
    // chains from genuine joint runtime queries; integers here mint no permit.
    // A typed tail successor has a fresh incarnation and the same continuation.
    // Native symbol depth/CFA/PC/function equality is never used as identity.
    template<typename Frame, ::std::size_t InitialExtent, ::std::size_t CurrentExtent>
    [[nodiscard]] inline activation_relation compare_event_chains(::std::span<Frame const, InitialExtent> initial,
        ::std::span<Frame const, CurrentExtent> current, ::std::size_t max_frames = (::std::numeric_limits<::std::size_t>::max)()) noexcept
    {
        auto const valid{[&](::std::span<Frame const> frames) noexcept
        {
            if(frames.empty() || frames.size() > max_frames) { return false; }
            for(::std::size_t i{}; i != frames.size(); ++i)
            {
                auto const& frame{frames[i]};
                if(frame.incarnation == 0u || frame.continuation == 0u || frame.runtime_epoch == 0u || frame.function_generation == 0u ||
                   frame.runtime_epoch != frames.front().runtime_epoch || frame.parent != (i == 0u ? 0u : frames[i - 1u].incarnation)) { return false; }
                // Actual generated identities are minted monotonically, including
                // tail successors. This also excludes duplicates/cycles in O(depth).
                if(i != 0u && frame.incarnation <= frame.parent) { return false; }
            }
            return true;
        }};
        if(!valid(initial) || !valid(current) || initial.front().runtime_epoch != current.front().runtime_epoch)
        { return activation_relation::unknown; }
        auto const same{[](Frame const& a, Frame const& b) noexcept
        { return a.incarnation == b.incarnation && a.parent == b.parent && a.continuation == b.continuation &&
                 a.module == b.module && a.function == b.function && a.function_generation == b.function_generation && a.runtime_epoch == b.runtime_epoch; }};
        auto const prefix_count{(::std::min)(initial.size() - 1u, current.size())};
        for(::std::size_t i{}; i != prefix_count; ++i)
        { if(!same(initial[i], current[i])) { return activation_relation::unknown; } }
        if(current.size() < initial.size()) { return activation_relation::returned; }
        auto const& previous{initial.back()}; auto const& at_origin_depth{current[initial.size() - 1u]};
        if(same(previous, at_origin_depth))
        { return current.size() == initial.size() ? activation_relation::same : activation_relation::deeper; }
        if(previous.incarnation != at_origin_depth.incarnation && previous.parent == at_origin_depth.parent &&
           previous.continuation == at_origin_depth.continuation)
        { return current.size() == initial.size() ? activation_relation::tail_successor : activation_relation::deeper; }
        return activation_relation::unknown;
    }
    struct position
    {
        // Own the actual source binding ONLY for lifetime/control-block equality.
        // Neither a shared_ptr nor numeric fields prove the precondition above.
        ::std::shared_ptr<void const> source_owner{};
        ::std::uint64_t module{}, function{}, runtime_epoch{}, function_generation{};
        // First key is the concrete physical subprogram; remaining keys are
        // concrete inline DIE instances, not names or abstract-origin keys.
        ::std::vector<source_dwarf::die_key> scope_path{};
        mapping_kind mapping{mapping_kind::dwarf_scope};
        ::std::string file{};
        ::std::uint64_t line{}, column{}, discriminator{};
        bool is_statement{};
        ::std::size_t physical_depth{};
        position_status status{position_status::unmapped};
        trace_status trace{trace_status::missing};
        // DEFAULT UNKNOWN. Symbol-only depth does not prove same activation,
        // especially after tail calls or reuse of a native frame/CFA.
        activation_relation activation{activation_relation::unknown};
        // Metadata setup only: the controller has successfully parsed this
        // actual source module's line and concrete-scope tables. A missing row
        // may then be a prologue/epilogue hole; this flag grants no permission.
        bool metadata_ready{};
    };
    struct limits { ::std::size_t max_path{64u}, max_depth{(::std::numeric_limits<::std::size_t>::max)()}, max_file_bytes{4096u}; };
    enum class error
    {
        none, unmapped, ambiguous, native_position, stale_generation, missing_trace,
        truncated_trace, unknown_activation, inconsistent_activation, malformed_path,
        limit_exceeded, allocation_failure, unsupported_policy, malformed_position
    };
    enum class action { keep_running, stop, decline };
    // Wasm opcode policy needs no line table or language scope. The controller
    // supplies a relation from two fresh, authenticated runtime event chains.
    // A tail successor inherits the continuation, so over/out follow it until
    // it returns or unwinds to the caller; native frame depth is never an input.
    [[nodiscard]] inline constexpr action wasm_instruction_action(policy requested, activation_relation relation) noexcept
    {
        if((relation != activation_relation::same && relation != activation_relation::deeper &&
            relation != activation_relation::returned && relation != activation_relation::tail_successor) ||
           (requested != policy::into && requested != policy::over && requested != policy::out))
        { return action::decline; }
        if(requested == policy::into || relation == activation_relation::returned ||
           (requested == policy::over && relation == activation_relation::same)) { return action::stop; }
        if(relation == activation_relation::same || relation == activation_relation::deeper ||
           relation == activation_relation::tail_successor) { return action::keep_running; }
        return action::decline;
    }
    struct outcome
    {
        action result{action::decline}; error reason{error::unmapped};
        // Owned bounded metadata labels for a stop only. These tokens never
        // become DAP references or credentials for reading another frame.
        ::std::vector<source_dwarf::die_key> stopped_path{};
    };
    struct origin
    {
        position initial{};
        policy requested{policy::into};
        bool ready{}; // Only indicates bounded metadata setup, NOT authority.
        destination_policy destination{};
        ::std::string destination_file{};
        ::std::uint64_t destination_line{};
        // A genuinely observed return can precede a source row. The controller
        // retains THAT complete ancestor chain separately and compares every
        // future fresh capture against it; this metadata grants no runtime read.
        position returned_anchor{};
        bool returned_anchor_ready{};
    };
    namespace details
    {
        [[nodiscard]] inline bool same_owner(::std::shared_ptr<void const> const& a,
            ::std::shared_ptr<void const> const& b) noexcept
        {
            // [comparison-only opaque owner borrows] no dereference/advance.
            // Equal object addresses with foreign control blocks are different.
            return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
        }
        [[nodiscard]] inline bool prefix(::std::span<source_dwarf::die_key const> a,
            ::std::span<source_dwarf::die_key const> b) noexcept
        {
            if(a.size() > b.size()) { return false; }
            for(::std::size_t i{}; i != a.size(); ++i)
            { if(a[i] != b[i]) { return false; } } // same checked owned-span index; no pointer is exposed.
            return true;
        }
        [[nodiscard]] inline error inspect(position const& value, limits const& cap, bool need_mapping) noexcept
        {
            if(value.mapping != mapping_kind::dwarf_scope && value.mapping != mapping_kind::line_only)
            { return error::malformed_position; }
            if(value.mapping == mapping_kind::line_only && !value.scope_path.empty())
            { return error::malformed_path; }
            switch(value.status)
            { case position_status::mapped: case position_status::unmapped: case position_status::ambiguous:
              case position_status::native: case position_status::stale: break; default: return error::malformed_position; }
            switch(value.trace)
            { case trace_status::missing: case trace_status::complete: case trace_status::truncated: break; default: return error::malformed_position; }
            switch(value.activation)
            { case activation_relation::unknown: case activation_relation::same: case activation_relation::deeper:
              case activation_relation::returned: case activation_relation::tail_successor: break; default: return error::malformed_position; }
            if(value.physical_depth > cap.max_depth || value.scope_path.size() > cap.max_path || value.file.size() > cap.max_file_bytes)
            { return error::limit_exceeded; }
            if(value.trace == trace_status::truncated) { return error::truncated_trace; }
            if(value.status == position_status::native) { return error::native_position; }
            if(value.status == position_status::stale || value.runtime_epoch == 0u || value.function_generation == 0u)
            { return error::stale_generation; }
            if(value.status == position_status::ambiguous) { return error::ambiguous; }
            if(value.trace == trace_status::complete && value.physical_depth == 0u) { return error::inconsistent_activation; }
            if(value.status != position_status::mapped) { return need_mapping ? error::unmapped : error::none; }
            if(!value.source_owner || value.file.empty() || value.line == 0u) { return error::unmapped; }
            if(value.mapping == mapping_kind::line_only) { return error::none; }
            if(value.scope_path.empty()) { return error::malformed_path; }
            for(::std::size_t i{}; i != value.scope_path.size(); ++i)
            { for(::std::size_t j{}; j != i; ++j) { if(value.scope_path[i] == value.scope_path[j]) { return error::malformed_path; } } }
            return error::none;
        }
    }
    // This setup copies bounded metadata; it cannot mint runtime stop authority.
    [[nodiscard]] inline error prepare(position const& initial, policy requested, origin& out, limits const& cap = {}) noexcept
    {
        out = {};
        if(requested != policy::into && requested != policy::over && requested != policy::out) { return error::unsupported_policy; }
        auto const checked{details::inspect(initial, cap, true)};
        if(checked != error::none) { return checked; }
        if((requested != policy::into || initial.mapping == mapping_kind::line_only) &&
           initial.trace != trace_status::complete) { return error::missing_trace; }
        if((requested != policy::into || initial.mapping == mapping_kind::line_only) &&
           initial.activation != activation_relation::same) { return error::unknown_activation; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            out.initial = initial; out.requested = requested; out.ready = true; return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return error::allocation_failure; }
#endif
    }
    // Controller first resolves this exact statement against the current
    // publication's emitted Wasm safe points; labels here remain policy DATA.
    [[nodiscard]] inline error prepare_destination(position const& initial, destination_policy requested,
        ::std::string_view file, ::std::uint64_t line, origin& out, limits const& cap = {}) noexcept
    {
        out = {};
        if(requested != destination_policy::until && requested != destination_policy::advance) { return error::unsupported_policy; }
        if(file.empty() || file.size() > cap.max_file_bytes || line == 0u) { return error::malformed_position; }
        auto const checked{prepare(initial,policy::over,out,cap)};
        if(checked != error::none) { return checked; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            out.destination_file = ::fast_io::concat_std(file);
            out.destination_line = line; out.destination = requested; return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return error::allocation_failure; }
#endif
    }
    // Each future use requires a NEW real paused query/capture. An origin ticket
    // is necessarily retired after resume and cannot authorize the next position.
    // Likewise caller-supplied activation labels below are policy inputs only.
    [[nodiscard]] inline outcome decide(origin const& saved, position const& current, limits const& cap = {}) noexcept
    {
        auto const decline{[](error why) noexcept { return outcome{action::decline, why, {}}; }};
        if(!saved.ready) { return decline(error::unmapped); }
        if(saved.requested != policy::into && saved.requested != policy::over && saved.requested != policy::out)
        { return decline(error::unsupported_policy); }
        if(saved.requested != policy::into && saved.initial.trace != trace_status::complete) { return decline(error::missing_trace); }
        if(saved.requested != policy::into && saved.initial.activation != activation_relation::same) { return decline(error::unknown_activation); }
        auto const original_checked{details::inspect(saved.initial, cap, true)};
        if(original_checked != error::none) { return decline(original_checked); }
        auto const checked{details::inspect(current, cap, false)};
        if(checked != error::none) { return decline(checked); }
        auto const& initial{saved.initial};
        if(current.runtime_epoch != initial.runtime_epoch) { return decline(error::stale_generation); }
        bool const same_function{current.module == initial.module && current.function == initial.function};
        if((current.module == initial.module && current.source_owner && !details::same_owner(current.source_owner, initial.source_owner)) ||
           (same_function && (current.function_generation != initial.function_generation ||
                             !details::same_owner(current.source_owner, initial.source_owner))))
        { return decline(error::stale_generation); }
        if(same_function && current.mapping != initial.mapping) { return decline(error::stale_generation); }
        bool const mapped{current.status == position_status::mapped};
        if(saved.destination != destination_policy::none && saved.destination != destination_policy::until &&
           saved.destination != destination_policy::advance) { return decline(error::unsupported_policy); }
        if(saved.destination != destination_policy::none &&
           (saved.requested != policy::over || saved.destination_file.empty() || saved.destination_file.size() > cap.max_file_bytes ||
            saved.destination_line == 0u)) { return decline(error::malformed_position); }
        if(saved.destination != destination_policy::none && !saved.returned_anchor_ready)
        {
            if(current.trace != trace_status::complete) { return decline(error::missing_trace); }
            if(current.activation == activation_relation::unknown) { return decline(error::unknown_activation); }
            bool const valid_relation{
                current.activation == activation_relation::same ? same_function && current.physical_depth == initial.physical_depth :
                current.activation == activation_relation::deeper ? current.physical_depth > initial.physical_depth :
                current.activation == activation_relation::returned ? current.physical_depth < initial.physical_depth :
                current.activation == activation_relation::tail_successor && current.physical_depth == initial.physical_depth};
            if(!valid_relation) { return decline(error::inconsistent_activation); }
            if(!mapped)
            {
                // Deeper calls can have no source. After a proven return we
                // need real source metadata before rebasing the ancestor.
                if(current.activation != activation_relation::deeper && (!current.metadata_ready || !current.source_owner))
                { return decline(error::unmapped); }
                return {action::keep_running,error::none,{}};
            }
            if(current.activation == activation_relation::same && initial.mapping == mapping_kind::dwarf_scope &&
               current.scope_path.front() != initial.scope_path.front()) { return decline(error::malformed_path); }
            bool const returned{current.activation == activation_relation::returned};
            bool const inline_exit{current.activation == activation_relation::same && initial.scope_path.size() > 1u &&
                !details::prefix(initial.scope_path,current.scope_path)};
            bool const hit{current.module == initial.module && current.file == saved.destination_file &&
                current.line == saved.destination_line &&
                (saved.destination == destination_policy::advance ||
                 (current.activation == activation_relation::same && !inline_exit))};
            if(!current.is_statement || (!returned && !inline_exit && !hit)) { return {action::keep_running,error::none,{}}; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                return {action::stop,error::none,current.scope_path};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { return decline(error::allocation_failure); }
#endif
        }
        if(saved.returned_anchor_ready)
        {
            auto const& anchor{saved.returned_anchor};
            auto const anchored{details::inspect(anchor,cap,false)};
            if(anchored != error::none) { return decline(anchored); }
            if(saved.requested == policy::into || anchor.trace != trace_status::complete ||
               !anchor.source_owner || !anchor.metadata_ready || anchor.runtime_epoch != initial.runtime_epoch ||
               anchor.physical_depth >= initial.physical_depth) { return decline(error::inconsistent_activation); }
            if(current.trace != trace_status::complete) { return decline(error::missing_trace); }
            if(current.activation == activation_relation::unknown) { return decline(error::unknown_activation); }
            if(!mapped && (!current.metadata_ready || !current.source_owner)) { return decline(error::unmapped); }
            bool const same_anchor{current.module == anchor.module && current.function == anchor.function};
            if((current.module == anchor.module && !details::same_owner(current.source_owner,anchor.source_owner)) ||
               (same_anchor && current.function_generation != anchor.function_generation)) { return decline(error::stale_generation); }
            bool const valid_relation{
                current.activation == activation_relation::same ? same_anchor && current.physical_depth == anchor.physical_depth :
                current.activation == activation_relation::deeper ? current.physical_depth > anchor.physical_depth :
                current.activation == activation_relation::returned ? current.physical_depth < anchor.physical_depth :
                current.activation == activation_relation::tail_successor && current.physical_depth == anchor.physical_depth};
            if(!valid_relation) { return decline(error::inconsistent_activation); }
            // A newly called helper under the proven returned-to ancestor is
            // not that ancestor's next statement, even if its source is mapped.
            if(current.activation == activation_relation::deeper || current.activation == activation_relation::tail_successor ||
               !mapped || !current.is_statement) { return {action::keep_running,error::none,{}}; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                return {action::stop,error::none,current.scope_path};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { return decline(error::allocation_failure); }
#endif
        }
        if(initial.mapping == mapping_kind::line_only || current.mapping == mapping_kind::line_only)
        {
            if(current.trace != trace_status::complete) { return decline(error::missing_trace); }
            if(current.activation == activation_relation::unknown) { return decline(error::unknown_activation); }
            bool const valid_relation{
                current.activation == activation_relation::same ? same_function && current.physical_depth == initial.physical_depth :
                current.activation == activation_relation::deeper ? current.physical_depth > initial.physical_depth :
                current.activation == activation_relation::returned ? current.physical_depth < initial.physical_depth :
                current.activation == activation_relation::tail_successor && current.physical_depth == initial.physical_depth};
            if(!valid_relation) { return decline(error::inconsistent_activation); }
        }
        if(!mapped && current.metadata_ready && current.source_owner && current.trace == trace_status::complete &&
           current.activation != activation_relation::unknown)
        {
            // Production already authenticated this NEW pause's complete
            // activation chain, publication and original source owner jointly.
            // A real line/scope hole must not turn into a fabricated statement
            // or into an error before a child's first mapped instruction.
            bool const relation_valid{
                current.activation == activation_relation::same ? same_function && current.physical_depth == initial.physical_depth :
                current.activation == activation_relation::deeper ? current.physical_depth > initial.physical_depth :
                current.activation == activation_relation::returned ? current.physical_depth < initial.physical_depth :
                current.activation == activation_relation::tail_successor && current.physical_depth == initial.physical_depth};
            if(!relation_valid) { return decline(error::inconsistent_activation); }
            // The controller bounds both elapsed time and genuine stop count.
            // No origin ticket, inline path or guessed caller PC is reused.
            return {action::keep_running, error::none, {}};
        }
        bool const changed_statement{current.file != initial.file || current.line != initial.line ||
            current.column != initial.column || current.discriminator != initial.discriminator};
        bool const same_path{current.scope_path == initial.scope_path};
        bool stop{};
        if(saved.requested == policy::into)
        {
            if(!mapped) { return decline(error::unmapped); }
            stop = !same_function || !same_path || changed_statement ||
                current.activation == activation_relation::deeper || current.activation == activation_relation::tail_successor;
        }
        else
        {
            if(current.trace != trace_status::complete) { return decline(error::missing_trace); }
            if(current.activation == activation_relation::unknown) { return decline(error::unknown_activation); }
            if(current.physical_depth > initial.physical_depth)
            {
                if(current.activation != activation_relation::deeper) { return decline(error::inconsistent_activation); }
                // Proven real deeper calls may lack source metadata; over/out
                // skip them without fabricating the caller's PC or inline path.
                return {action::keep_running, error::none, {}};
            }
            if(current.physical_depth < initial.physical_depth)
            {
                if(current.activation != activation_relation::returned) { return decline(error::inconsistent_activation); }
                if(!mapped) { return decline(error::unmapped); }
                stop = true; // THIS current caller location came from its own actual pause, never origin call-site metadata.
            }
            else if(current.activation == activation_relation::tail_successor)
            {
                // next/finish follow the retiring frame's actual continuation.
                // A fresh tail callee at equal physical depth is neither the
                // same activation nor a returned caller. Wait for its return.
                return {action::keep_running, error::none, {}};
            }
            else
            {
                if(current.activation != activation_relation::same || !same_function || !mapped)
                { return decline(current.activation == activation_relation::same ? error::inconsistent_activation : error::unknown_activation); }
                if(initial.mapping == mapping_kind::dwarf_scope &&
                   current.scope_path.front() != initial.scope_path.front()) { return decline(error::malformed_path); }
                if(saved.requested == policy::out)
                {
                    // Physical-frame out (one path key) still needs actual return;
                    // inline out stops once THAT concrete inline instance is left.
                    stop = initial.scope_path.size() > 1u && !details::prefix(initial.scope_path, current.scope_path);
                }
                else
                {
                    bool const in_inline_child{current.scope_path.size() > initial.scope_path.size() &&
                        details::prefix(initial.scope_path, current.scope_path)};
                    stop = !in_inline_child && (!same_path || changed_statement);
                }
            }
        }
        // Same line/column with a changed discriminator is a distinct statement,
        // but nonstatement rows are never stops. Individual Wasm/native code
        // offsets are deliberately not compared as source statement identities.
        if(!stop || !current.is_statement) { return {action::keep_running, error::none, {}}; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            return {action::stop, error::none, current.scope_path};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { return decline(error::allocation_failure); }
#endif
    }
    // Policy DATA only. Call after a fresh joint runtime capture established
    // `returned` and decide() accepted continuing without a source statement.
    // The production controller must ALSO retain that exact activation chain.
    [[nodiscard]] inline error observe_return(origin& saved, position const& current, limits const& cap = {}) noexcept
    {
        if(!saved.ready || saved.requested == policy::into) { return error::unsupported_policy; }
        auto const depth{saved.returned_anchor_ready ? saved.returned_anchor.physical_depth : saved.initial.physical_depth};
        if(current.activation != activation_relation::returned || current.physical_depth >= depth ||
           !current.source_owner || !current.metadata_ready) { return error::inconsistent_activation; }
        auto const result{decide(saved,current,cap)};
        if(result.reason != error::none) { return result.reason; }
        if(result.result != action::keep_running) { return error::inconsistent_activation; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            position copy{current}; saved.returned_anchor=::std::move(copy); saved.returned_anchor_ready=true; return error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { return error::allocation_failure; }
#endif
    }
}
