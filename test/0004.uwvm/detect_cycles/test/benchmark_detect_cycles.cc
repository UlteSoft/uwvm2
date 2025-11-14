/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5-High
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

#include <uwvm2/utils/container/impl.h>
#include <uwvm2/utils/debug/timer.h>
#include <uwvm2/uwvm/wasm/loader/impl.h>
#include <uwvm2/uwvm/cmdline/callback/impl.h>

using namespace uwvm2::utils::container;
using uwvm2::uwvm::wasm::loader::detect_cycles;

struct GraphBuilder
{
    unordered_flat_map<u8string_view, vector<u8string_view>> adj;
    vector<u8string> storage;  // own strings

    u8string_view add_node(u8string name)
    {
        storage.push_back(std::move(name));
        u8string_view v{storage.back().data(), storage.back().size()};
        adj.emplace(v, vector<u8string_view>{});
        return v;
    }

    void add_edge(u8string_view u, u8string_view v) { adj[u].push_back(v); }
};

// Construct a graph with k disjoint cycles, each of length L
GraphBuilder build_disjoint_cycles(std::size_t k, std::size_t L)
{
    GraphBuilder gb;
    vector<vector<u8string_view>> groups;
    groups.resize(k);
    for(std::size_t i = 0; i < k; ++i)
    {
        groups[i].reserve(L);
        for(std::size_t j = 0; j < L; ++j)
        {
            u8string name;
            name.append(u8string_view{u8"N_", 2});
            auto is = ::fast_io::u8to<u8string>(i);
            name.append(is);
            name.push_back(u8'_');
            auto js = ::fast_io::u8to<u8string>(j);
            name.append(u8string_view{reinterpret_cast<char8_t const*>(js.data()), js.size()});
            auto v = gb.add_node(std::move(name));
            groups[i].push_back(v);
        }
    }
    for(std::size_t i = 0; i < k; ++i)
    {
        for(std::size_t j = 0; j < L; ++j)
        {
            auto u = groups[i][j];
            auto v = groups[i][(j + 1) % L];
            gb.add_edge(u, v);
        }
    }
    return gb;
}

int main()
{
    // Warmup small graph
    {
        auto gb = build_disjoint_cycles(2, 3);
        (void)detect_cycles(gb.adj);
    }

    using uwvm2::utils::debug::timer;

    // Scale k (number of cycles) and L (cycle length) to observe growth
    for(std::size_t k: {1uz, 4uz, 16uz, 64uz})
    {
        for(std::size_t L: {3uz, 8uz, 16uz, 32uz})
        {
            u8string label;
            label.append(u8string_view{u8"detect_cycles k="});
            auto ks = ::fast_io::u8to<u8string>(k);
            label.append(ks);
            label.append(u8string_view{u8" L="});
            auto Ls = ::fast_io::u8to<u8string>(L);
            label.append(Ls);
            timer t{
                u8string_view{label.data(), label.size()}
            };

            auto gb = build_disjoint_cycles(k, L);
            auto res = detect_cycles(gb.adj);
            // Basic sanity: expect exactly k cycles
            if(res.size() != k) { ::fast_io::fast_terminate(); }
        }
    }

}

