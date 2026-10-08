/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

// Private, unqualified compiler infrastructure. Inputs come only from a
// complete validated native call graph. This does not decode Wasm, authorize
// a pending-return ABI, change an LLVM attribute, or activate the new protocol.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared::wasm_exception_effect
{
    enum class effect : unsigned char
    {
        none = 0u, guest_escape = 1u, native_unwind = 2u, both = 3u
    };
    [[nodiscard]] constexpr unsigned bits(effect value) noexcept
    { return static_cast<unsigned>(value); }
    [[nodiscard]] constexpr bool valid(effect value) noexcept
    { return bits(value) <= bits(effect::both); }
    [[nodiscard]] constexpr effect unite(effect first, effect second) noexcept
    { return static_cast<effect>(bits(first) | bits(second)); }
    [[nodiscard]] constexpr effect project(effect value, effect escaping) noexcept
    { return static_cast<effect>(bits(value) & bits(escaping)); }
    [[nodiscard]] constexpr bool subset(effect candidate, effect admitted) noexcept
    { return valid(candidate) && valid(admitted) && (bits(candidate) & ~bits(admitted)) == 0u; }

    inline constexpr auto unknown_target{(::std::numeric_limits<::std::size_t>::max)()};
    struct function
    {
        // Own escaping throws, observable materialization/allocation or native
        // calls not represented below. Locally consumed throws may be removed
        // ONLY by the validated identity/handler proof, not a feature flag.
        effect local{effect::none};
    };
    struct call
    {
        ::std::size_t caller{}, target{unknown_target};
        // A genuine Wasm catch_all can consume every guest exception from
        // this call. It cannot consume an arbitrary foreign C++ exception:
        // native_unwind stays set for opaque/native/foreign targets.
        // Specific tag catches need a separate tag-set proof to clear guest.
        effect escaping{effect::both};
    };
    enum class status : unsigned char { ok, invalid_graph, size_overflow };
    struct result
    {
        status state{status::invalid_graph};
        ::std::vector<effect> functions{};
        ::std::size_t propagation_visits{};
    };

    // Compute the least conservative fixed point, including recursive SCCs.
    // Each bit is added at most once per node; a reversed work list revisits
    // an edge at most twice. No recursion or depth-dependent native stack use.
    // There are no run-time checks on numeric/linear-memory instructions here.
    // An incomplete graph MUST seed omitted edges with both; unknown, mutable
    // table/ref, cross-generation and imported targets are never assumed pure.
    [[nodiscard]] inline result analyze(::std::span<function const> functions,
                                       ::std::span<call const> calls)
    {
        constexpr auto limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
        auto const count{functions.size()};
        if(count > limit / (2uz * sizeof(::std::size_t)) ||
           calls.size() > limit / (2uz * sizeof(::std::size_t)))
        { return {status::size_overflow, {}, 0uz}; }
        for(auto const& entry : functions)
        { if(!valid(entry.local)) { return {}; } }
        for(auto const& edge : calls)
        {
            if(edge.caller >= count || !valid(edge.escaping) ||
               (edge.target != unknown_target && edge.target >= count) ||
               (edge.target == unknown_target &&
                (bits(edge.escaping) & bits(effect::native_unwind)) == 0u))
            { return {}; }
        }

        result output{status::ok, ::std::vector<effect>(count), 0uz};
        ::std::vector<::std::size_t> heads(count, unknown_target);
        ::std::vector<::std::size_t> next(calls.size(), unknown_target);
        ::std::vector<::std::size_t> queue(2uz * count);
        ::std::size_t read{}, written{};
        for(::std::size_t index{}; index != count; ++index)
        {
            // [count immutable validated function records][count owned effects]
            // [safe                                   ] index names complete
            // source/destination elements; no raw address escapes the analysis.
            output.functions[index] = functions[index].local;
        }
        for(::std::size_t index{}; index != calls.size(); ++index)
        {
            auto const& edge{calls[index]};
            if(edge.target == unknown_target)
            {
                output.functions[edge.caller] = unite(output.functions[edge.caller], edge.escaping);
                continue;
            }
            // [calls.size() owned reverse links][count owned heads]
            // [safe                           ] indices were preflighted.
            // ^^ next[index] retains an index or sentinel, never a borrowed
            //    vector-element pointer which reallocation could invalidate.
            next[index] = heads[edge.target];
            heads[edge.target] = index;
        }
        for(::std::size_t index{}; index != count; ++index)
        {
            if(output.functions[index] != effect::none)
            {
                // [0,written) initialized indices][free suffix ... 2*count]
                // [safe                         ] one initial entry per node.
                queue[written++] = index;
            }
        }
        while(read != written)
        {
            // [read,written) owns initialized node indices; each is < count.
            // ^^ consume one index before visiting its immutable reverse list.
            auto const target{queue[read++]};
            for(auto edge_index{heads[target]}; edge_index != unknown_target;)
            {
                // [preflighted reverse-edge allocation] edge_index is either
                // a written call index or the sentinel. No native borrow is
                // retained by this work list or by the returned summary.
                auto const& edge{calls[edge_index]};
                auto const prior{output.functions[edge.caller]};
                auto const updated{unite(prior, project(output.functions[target], edge.escaping))};
                ++output.propagation_visits;
                if(updated != prior)
                {
                    // Each new queue entry adds at least one previously absent
                    // bit to this node. All initial entries also account for a
                    // bit, so 2*count owns every possible work-list entry.
                    if(written == queue.size()) { return {status::invalid_graph, {}, 0uz}; }
                    output.functions[edge.caller] = updated;
                    // [0,written) initialized indices][one free owned slot]
                    // ^^ publish the fully updated caller before queueing it.
                    queue[written++] = edge.caller;
                }
                // [immutable reverse index chain] advance only through the
                // previously initialized in-range link or its sentinel.
                // ^^ no pointer arithmetic or graph node address is followed.
                edge_index = next[edge_index];
            }
        }
        return output;
    }
}
