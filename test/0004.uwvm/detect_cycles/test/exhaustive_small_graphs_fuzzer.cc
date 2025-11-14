/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
 * @version     2.0.0
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

#include <bit>
#include <uwvm2/utils/container/impl.h>
#include <uwvm2/uwvm/wasm/loader/impl.h>
#include <uwvm2/uwvm/cmdline/callback/impl.h>

using namespace uwvm2::utils::container;
using uwvm2::uwvm::wasm::loader::detect_cycles;

namespace
{

    struct GraphBuilder
    {
        unordered_flat_map<u8string_view, vector<u8string_view>> adj;
        vector<u8string> storage;

        u8string_view add_node(u8string name)
        {
            storage.push_back(std::move(name));
            u8string_view v{storage.back().data(), storage.back().size()};
            adj.emplace(v, vector<u8string_view>{});
            return v;
        }

        void add_edge(u8string_view u, u8string_view v) { adj[u].push_back(v); }
    };

    // Build graph with n nodes and edges from bitmask over ordered pairs (i->j, i!=j)
    GraphBuilder build_graph_from_mask(std::size_t n, std::uint64_t mask)
    {
        GraphBuilder gb;
        vector<u8string_view> nodes;
        nodes.reserve(n);
        for(std::size_t i = 0; i < n; ++i)
        {
            u8string name = ::fast_io::u8to<u8string>(u8"N", i);
            nodes.push_back(gb.add_node(std::move(name)));
        }

        std::size_t bit = 0;
        for(std::size_t i = 0; i < n; ++i)
        {
            for(std::size_t j = 0; j < n; ++j)
            {
                if(i == j) { continue; }
                if((mask >> bit) & 1ull) { gb.add_edge(nodes[i], nodes[j]); }
                ++bit;
            }
        }

        return gb;
    }

}  // namespace

int main()
{
    // Exhaustive up to 4 nodes: at most 12 directed edges (n*(n-1))
    // We cap masks to those with <= 10 edges to constrain runtime.
    for(std::size_t n = 1; n <= 4; ++n)
    {
        std::size_t total_pairs = n * (n - 1);
        std::uint64_t total_masks = (total_pairs >= 64 ? ~0ull : (1ull << total_pairs));

        std::uint64_t limit_masks = total_masks;  // iterate all combinations but filter by popcount

        for(std::uint64_t m = 0; m < limit_masks; ++m)
        {
            if(::std::popcount(m) > 10) { continue; }
            auto gb = build_graph_from_mask(n, m);
            auto res = detect_cycles(gb.adj);

            // Sanity: if cycle exists, each reported cycle must have size >=2 and start==end
            for(auto const& cyc: res)
            {
                if(cyc.size() < 2) { ::fast_io::fast_terminate(); }
                if(cyc.front() != cyc.back()) { ::fast_io::fast_terminate(); }
            }
        }
    }

}


