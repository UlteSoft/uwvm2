/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
# include <cstddef>
# include <cstdint>
# include <climits>
# include <type_traits>
# include <limits>
# include <utility>
# include <algorithm>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/depend/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/custom/impl.h>
# include <uwvm2/uwvm/wasm/warning/impl.h>
# include "wasm_file.h"
# include "dl.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::loader
{
    /// @brief Detect all cycles in dependency graph (deduplicated and efficient).
    ///        Based on "SCC decomposition + ordered DFS (Johnson's idea)": first decompose the graph into strongly connected components,
    ///        then perform restricted DFS within each SCC, only starting enumeration from the minimum id vertex on the cycle,
    ///        thus avoiding rotation duplicates like A->B->A and B->A->B.
    /// @param adjacency_list The adjacency list of the dependency graph
    /// @return All unique cycles (each cycle has same start and end: v0 -> ... -> v0)
    inline constexpr auto detect_cycles(
        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view,
                                                      ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string_view>> const&
            adjacency_list) noexcept
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer detect_cycles_timer{u8"detect cycles"};
#endif

        using module_name_t = ::uwvm2::utils::container::u8string_view;
        using cycle_t = ::uwvm2::utils::container::vector<module_name_t>;
        using cycles_t = ::uwvm2::utils::container::vector<cycle_t>;

        auto const curr_recursion_depth_limit{::uwvm2::uwvm::utils::depend::recursion_depth_limit};

        cycles_t all_cycles{};
        if(adjacency_list.empty()) [[unlikely]] { return all_cycles; }

        // Check dependency count limit
        // The number of recursions is greater than or equal to the number of dependencies, so it can be checked in advance.
        if(auto const dependency_count{adjacency_list.size()}; dependency_count > curr_recursion_depth_limit && curr_recursion_depth_limit != 0uz)
        {
            // Output warning about dependency count exceeding limit
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Dependency count \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                dependency_count,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" exceeds limit \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                curr_recursion_depth_limit,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Dependency check skipped. Use \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"--wasm-depend-recursion-limit",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" to force check. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(depend)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            if(::uwvm2::uwvm::io::depend_warning_fatal) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(depend)\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            return all_cycles;
        }

        // 1) Compress names to integer ids for efficient computation (avoid frequent hashing of string_view)
        // unordered_flat_map use xxh3 hash
        ::uwvm2::utils::container::unordered_flat_map<module_name_t, ::std::size_t> name_to_id{};
        ::uwvm2::utils::container::vector<module_name_t> id_to_name{};
        auto const adjacency_list_size{adjacency_list.size()};
        id_to_name.reserve(adjacency_list_size);
        name_to_id.reserve(adjacency_list_size);
        // id_to_name, name_to_id are all of size adjacency_list_size
        for(auto const& [name, _]: adjacency_list)
        {
            auto const id{id_to_name.size()};
            // Safe: we just reserved space for id_to_name.size() elements
            id_to_name.push_back_unchecked(name);
            name_to_id.emplace(name, id);
        }

        // 2) Build compact adjacency list (by id)
        ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<::std::size_t>> graph{};
        graph.resize(id_to_name.size());
        for(auto const& [u_name, outs]: adjacency_list)
        {
            auto const u_id{name_to_id[u_name]};
            // Safe: u_id is guaranteed to be < graph.size() because we just built name_to_id
            auto& u_out{graph.index_unchecked(u_id)};
            u_out.reserve(outs.size());
            for(auto const v_name: outs)
            {
                auto const it{name_to_id.find(v_name)};
                if(it != name_to_id.end())
                {
                    // Safe: we just reserved space for outs.size() elements
                    u_out.push_back_unchecked(it->second);
                }
            }
        }

        // 3) Tarjan's algorithm to find strongly connected components (O(V+E))
        // Use std::make_signed_t<std::size_t> for index/low to ensure type compatibility with size_t
        // Check for overflow when graph size exceeds signed type maximum
        using signed_size_t = ::std::make_signed_t<::std::size_t>;
        constexpr auto signed_size_max{::std::numeric_limits<signed_size_t>::max()};

        auto const graph_size{graph.size()};

        if(graph_size > static_cast<::std::size_t>(signed_size_max)) [[unlikely]]
        {
            // Output warning about cycle detection being impossible due to graph size
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Graph size \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                graph_size,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" exceeds maximum signed size \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                signed_size_max,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Cycle detection skipped. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(depend)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            if(::uwvm2::uwvm::io::depend_warning_fatal) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(depend)\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            return all_cycles;
        }

        ::uwvm2::utils::container::vector<signed_size_t> index(graph_size, static_cast<signed_size_t>(-1));
        ::uwvm2::utils::container::vector<signed_size_t> low(graph_size, signed_size_t{});
        ::uwvm2::utils::container::vector<bool> on_stack(graph_size, false);
        ::uwvm2::utils::container::vector<::std::size_t> st{};
        st.reserve(graph_size);
        // index, low, on_stack, st are all of size graph_size

        // The reason for using size_t and converting it to signed_size_t is that the maximum number of times this function can be called is signed_size_t_max.
        // When dfs_idx++ is called later, even if the value before the UB occurred is used, UB still cannot be avoided. Therefore, unsigned storage is used
        // here to ensure that UB will never occur during subsequent increments, while also ensuring that all returns are signed. Converting unsigned to signed
        // also does not cause any UB.

        ::std::size_t dfs_idx{};

        ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<::std::size_t>> sccs{};

        for(::std::size_t v{}; v != graph_size; ++v)
        {
            // Safe: v is guaranteed to be < graph.size() because we iterate over graph.size()
            if(index.index_unchecked(v) != static_cast<signed_size_t>(-1)) { continue; }

            // Iterative implementation to avoid stack overflow with recursion depth limit
            struct stack_frame_t
            {
                ::std::size_t v;
                ::std::size_t neighbor_idx;
                bool is_initial_call;
                ::std::size_t depth;

                inline constexpr stack_frame_t(::std::size_t vertex, ::std::size_t current_depth, bool initial = true) noexcept :
                    v{vertex}, neighbor_idx{0uz}, is_initial_call{initial}, depth{current_depth}
                {
                }
            };

            // Use explicit stack to avoid recursion with depth limit
            ::uwvm2::utils::container::vector<stack_frame_t> call_stack{};
            call_stack.reserve(graph_size);  // Reserve maximum possible depth, so we can use push_back_unchecked

            // size of graph_size != 0
            call_stack.emplace_back_unchecked(v, 0uz, true);

            while(!call_stack.empty()) [[likely]]
            {
                auto& frame{call_stack.back_unchecked()};

                // Check recursion depth limit
                if(frame.depth > curr_recursion_depth_limit && curr_recursion_depth_limit != 0uz) [[unlikely]]
                {
                    // Output warning about recursion depth limit exceeded
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        u8"[warn]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Recursion depth limit \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        curr_recursion_depth_limit,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\" exceeded at vertex \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        frame.v,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\". Skipping further checks. Use \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"--wasm-depend-recursion-limit",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\" to force check. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(depend)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                    if(::uwvm2::uwvm::io::depend_warning_fatal) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"Convert warnings to fatal errors. ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                            u8"(depend)\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    // Unified early return to avoid inconsistent Tarjan state
                    return all_cycles;
                }

                if(frame.is_initial_call) [[unlikely]]
                {
                    // Initial call - set up the vertex

                    // The reason for using size_t and converting it to signed_size_t is that the maximum number of times this function can be called is
                    // signed_size_t_max. When dfs_idx++ is called later, even if the value before the UB occurred is used, UB still cannot be avoided.
                    // Therefore, unsigned storage is used here to ensure that UB will never occur during subsequent increments, while also ensuring that all
                    // returns are signed. Converting unsigned to signed also does not cause any UB.

                    index.index_unchecked(frame.v) = low.index_unchecked(frame.v) = static_cast<signed_size_t>(dfs_idx);
                    ++dfs_idx;
                    // st size < graph_size, so push_back_unchecked is safe
                    st.push_back_unchecked(frame.v);
                    on_stack.index_unchecked(frame.v) = true;
                    frame.is_initial_call = false;
                }

                // Process neighbors
                auto const& neighbors{graph.index_unchecked(frame.v)};
                if(frame.neighbor_idx < neighbors.size()) [[likely]]
                {
                    auto const w{neighbors.index_unchecked(frame.neighbor_idx)};
                    ++frame.neighbor_idx;

                    // Safe: w is guaranteed to be < graph.size() because it comes from graph[v]
                    if(index.index_unchecked(w) == static_cast<signed_size_t>(-1))
                    {
                        // Recursive call - push new frame with increased depth
                        // size_t operations never wrap around, because the maximum size is signed size_t max.
                        call_stack.emplace_back_unchecked(w, frame.depth + 1uz, true);
                    }
                    else if(on_stack.index_unchecked(w)) { low.index_unchecked(frame.v) = ::std::min(low.index_unchecked(frame.v), index.index_unchecked(w)); }
                }
                else
                {
                    // All neighbors processed - check if this is a root of SCC
                    if(low.index_unchecked(frame.v) == index.index_unchecked(frame.v))
                    {
                        ::uwvm2::utils::container::vector<::std::size_t> comp{};
                        for(;;)
                        {
                            auto const w{st.back()};
                            st.pop_back_unchecked();
                            // Safe: w is guaranteed to be < graph.size() because it came from st
                            on_stack.index_unchecked(w) = false;
                            comp.push_back(w);
                            if(w == frame.v) { break; }
                        }
                        sccs.push_back(::std::move(comp));
                    }

                    // Update parent's low value if this is not the root call
                    if(call_stack.size() > 1uz) [[likely]]
                    {
                        auto& parent_frame{call_stack.index_unchecked(call_stack.size() - 2uz)};
                        low.index_unchecked(parent_frame.v) = ::std::min(low.index_unchecked(parent_frame.v), low.index_unchecked(frame.v));
                    }

                    // Pop this frame
                    call_stack.pop_back_unchecked();
                }
            }
        }

        // 4) Perform ordered DFS within each SCC, only starting from the minimum id vertex on the cycle, avoiding rotation duplicates
        //    Additionally maintain a 64-bit signature set for safe deduplication (based on xxh3), with extremely low collision probability
        ::uwvm2::utils::container::unordered_flat_set<::std::uint_least64_t> seen_signatures{};
        seen_signatures.reserve(256uz);  // Reserve space for cycle signatures to avoid reallocations

        // Temporary buffer for signatures (avoid repeated allocation)
        ::uwvm2::utils::container::vector<::std::size_t> temp_cycle_ids{};
        temp_cycle_ids.reserve(256uz);

        // DFS (restricted to only visit points >= start_id, ensuring minimum id is start_id)
        // Use bool for boolean flags to save memory and improve cache locality
        ::uwvm2::utils::container::vector<bool> in_scc(graph_size, false);
        ::uwvm2::utils::container::vector<bool> on_path(graph_size, false);
        ::uwvm2::utils::container::vector<::std::size_t> path{};
        path.reserve(graph_size);

        // Iterative DFS implementation with recursion depth limit
        struct DfsStackFrame
        {
            ::std::size_t v;
            ::std::size_t start_id;
            ::std::size_t neighbor_idx;
            ::std::size_t depth;
            bool is_initial_call;

            inline constexpr DfsStackFrame(::std::size_t vertex, ::std::size_t start, ::std::size_t current_depth, bool initial = true) noexcept :
                v{vertex}, start_id{start}, neighbor_idx{0uz}, depth{current_depth}, is_initial_call{initial}
            {
            }
        };

        for(auto& comp: sccs)
        {
            // Skip single-node SCCs without cycles (no self-loops)
            if(comp.size() == 1uz)
            {
                auto const u{comp.front_unchecked()};
                bool self_loop{};
                // Safe: u is guaranteed to be < graph.size() because it comes from our graph
                for(auto const w: graph.index_unchecked(u))
                {
                    if(w == u)
                    {
                        self_loop = true;
                        break;
                    }
                }
                if(self_loop)
                {
                    // A -> A
                    temp_cycle_ids.clear();
                    temp_cycle_ids.push_back_unchecked(u);
                    temp_cycle_ids.push_back_unchecked(u);

                    auto const bytes_ptr{reinterpret_cast<::std::byte const*>(temp_cycle_ids.data())};
                    auto const bytes_len{temp_cycle_ids.size() * sizeof(::std::size_t)};
                    auto const sig{::uwvm2::utils::hash::xxh3_64bits(bytes_ptr, bytes_len)};
                    if(!seen_signatures.contains(sig))
                    {
                        seen_signatures.insert(sig);
                        cycle_t cyc{};
                        cyc.reserve(2uz);
                        // Safe: u is guaranteed to be < id_to_name.size() because it comes from our graph
                        cyc.push_back_unchecked(id_to_name.index_unchecked(u));
                        cyc.push_back_unchecked(id_to_name.index_unchecked(u));
                        // Unknown size Use version with checks
                        all_cycles.push_back(::std::move(cyc));
                    }
                }
                continue;
            }

            // Mark nodes in this SCC
            for(auto const v: comp)
            {
                // Safe: v is guaranteed to be < graph.size() because it comes from our graph
                in_scc.index_unchecked(v) = true;
            }

            // Sort by id ascending to ensure cycles starting from minimum id vertex are only enumerated once
            // (comp order is Tarjan's pop order, not guaranteed to be sorted, no forced sorting here to save overhead,
            //  only sort when stability is needed:
            //  std::sort(comp.begin(), comp.end());)

            // If stronger deduplication stability is needed, uncomment the following sort:
            // ::std::sort(comp.begin(), comp.end());

            bool abort_comp{};

            for(auto const start_id: comp)
            {
                // Clear path state
                for(auto const v: comp)
                {
                    // Safe: v is guaranteed to be < graph.size() because it comes from our graph
                    on_path.index_unchecked(v) = false;
                }
                path.clear();

                auto start_v{start_id};
                // Use explicit stack to avoid recursion with depth limit
                ::uwvm2::utils::container::vector<DfsStackFrame> dfs_stack{};
                dfs_stack.reserve(graph_size);  // Reserve maximum possible depth

                dfs_stack.emplace_back_unchecked(start_v, start_id, 0uz, true);

                while(!dfs_stack.empty()) [[likely]]
                {
                    auto& frame{dfs_stack.back_unchecked()};

                    // Check recursion depth limit
                    if(frame.depth > curr_recursion_depth_limit && curr_recursion_depth_limit != 0uz) [[unlikely]]
                    {
                        // Output warning about recursion depth limit exceeded
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            u8"[warn]  ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"DFS recursion depth limit \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                            curr_recursion_depth_limit,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\" exceeded at vertex \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                            frame.v,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\". Skipping further checks. Use \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                            u8"--wasm-depend-recursion-limit",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\" to force check. ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                            u8"(depend)\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                        if(::uwvm2::uwvm::io::depend_warning_fatal) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"Convert warnings to fatal errors. ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                                u8"(depend)\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        // Abort current SCC enumeration: cleanup on_path/path and drain stack
                        for(auto const id_in_path: path) { on_path.index_unchecked(id_in_path) = false; }
                        path.clear();
                        dfs_stack.clear();
                        abort_comp = true;

                        break;
                    }

                    if(frame.is_initial_call)
                    {
                        // Initial call - set up the vertex
                        on_path.index_unchecked(frame.v) = true;
                        path.push_back_unchecked(frame.v);
                        frame.is_initial_call = false;
                    }

                    // Process neighbors
                    auto const& neighbors{graph.index_unchecked(frame.v)};
                    if(frame.neighbor_idx < neighbors.size()) [[likely]]
                    {
                        auto const w{neighbors.index_unchecked(frame.neighbor_idx)};
                        ++frame.neighbor_idx;

                        // Safe: w is guaranteed to be < graph.size() because it comes from graph[v]
                        if(!in_scc.index_unchecked(w)) { continue; }
                        if(w < frame.start_id) { continue; }
                        if(w == frame.start_id)
                        {
                            ::uwvm2::utils::container::vector<::std::size_t> const& path_ids{path};
                            // Normalize to cycle representation starting with minimum id (i.e., start_id): start, ..., start
                            temp_cycle_ids.clear();
                            // size_t operations never wrap around, because the maximum size is signed size_t max.
                            temp_cycle_ids.reserve(path_ids.size() + 1uz);
                            for(auto const id: path_ids) { temp_cycle_ids.push_back_unchecked(id); }
                            temp_cycle_ids.push_back_unchecked(frame.start_id);

                            // Calculate hash signature
                            auto const bytes_ptr{reinterpret_cast<::std::byte const*>(temp_cycle_ids.data())};
                            auto const temp_cycle_ids_size{temp_cycle_ids.size()};

                            // Never overflow, the vector container ensures that the storage size of its contents is always less than the maximum storage
                            // capacity.
                            constexpr auto max_size_dev_size_t{::std::numeric_limits<std::size_t>::max() / sizeof(::std::size_t)};
                            [[assume(temp_cycle_ids_size <= max_size_dev_size_t)]];

                            auto const bytes_len{temp_cycle_ids_size * sizeof(::std::size_t)};
                            auto const sig{::uwvm2::utils::hash::xxh3_64bits(bytes_ptr, bytes_len)};

                            if(seen_signatures.contains(sig)) { continue; }
                            seen_signatures.insert(sig);

                            // Convert back to names and write to result
                            cycle_t cyc{};
                            cyc.reserve(temp_cycle_ids_size);
                            for(auto const id: temp_cycle_ids)
                            {
                                // Safe: id is guaranteed to be < id_to_name.size() because it comes from our graph
                                cyc.push_back_unchecked(id_to_name.index_unchecked(id));
                            }
                            all_cycles.push_back(::std::move(cyc));
                        }
                        else if(!on_path.index_unchecked(w))
                        {
                            // Recursive call - push new frame with increased depth
                            // size_t operations never wrap around, because the maximum size is signed size_t max.
                            dfs_stack.emplace_back_unchecked(w, frame.start_id, frame.depth + 1uz, true);
                            continue;  // Process the new frame first
                        }
                    }
                    else
                    {
                        // All neighbors processed - cleanup
                        path.pop_back_unchecked();
                        on_path.index_unchecked(frame.v) = false;

                        // Pop this frame
                        dfs_stack.pop_back_unchecked();
                    }
                }

                if(abort_comp) [[unlikely]] { break; }
            }

            // Clear marks
            for(auto const v: comp)
            {
                // Safe: v is guaranteed to be < graph.size() because it comes from our graph
                in_scc.index_unchecked(v) = false;
            }
        }

        return all_cycles;
    }

}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
