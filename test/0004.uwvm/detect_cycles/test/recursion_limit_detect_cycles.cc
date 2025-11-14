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

#include <uwvm2/utils/container/impl.h>
#include <uwvm2/uwvm/utils/depend/depend_check.h>
#include <uwvm2/uwvm/wasm/loader/impl.h>
#include <uwvm2/uwvm/cmdline/callback/impl.h>

using namespace uwvm2::utils::container;
using uwvm2::uwvm::wasm::loader::detect_cycles;

namespace
{

    struct GraphBuilder
    {
        unordered_flat_map<u8string_view, vector<u8string_view>> adj;
        vector<u8string> storage;  // own strings to keep stable memory for views

        u8string_view add_node(u8string name)
        {
            storage.push_back(std::move(name));
            u8string_view v{storage.back().data(), storage.back().size()};
            adj.emplace(v, vector<u8string_view>{});
            return v;
        }

        u8string_view add_node(u8string_view name)
        {
            storage.emplace_back(name);
            u8string_view v{storage.back().data(), storage.back().size()};
            adj.emplace(v, vector<u8string_view>{});
            return v;
        }

        void add_edge(u8string_view u, u8string_view v) { adj[u].push_back(v); }
    };

}  // namespace

int main()
{
    // Save and restore global recursion limit
    auto const saved_limit = ::uwvm2::uwvm::utils::depend::recursion_depth_limit;

    // Baseline: unlimited (0) should detect known cycles correctly
    {
        ::uwvm2::uwvm::utils::depend::recursion_depth_limit = 0uz;  // unlimited

        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        auto B = gb.add_node(u8string_view{u8"B", 1});
        auto C = gb.add_node(u8string_view{u8"C", 1});
        auto D = gb.add_node(u8string_view{u8"D", 1});
        gb.add_edge(A, B);
        gb.add_edge(B, C);
        gb.add_edge(C, D);
        gb.add_edge(D, A);  // 4-cycle

        auto res = detect_cycles(gb.adj);
        if(res.empty())
        {
            // Expected at least one cycle when unlimited
            ::uwvm2::uwvm::utils::depend::recursion_depth_limit = saved_limit;
            ::fast_io::fast_terminate();
        }
    }

    // With tight recursion limit, detection should early-terminate and return empty (no cycles enumerated)
    {
        ::uwvm2::uwvm::utils::depend::recursion_depth_limit = 1uz;  // very small

        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        auto B = gb.add_node(u8string_view{u8"B", 1});
        auto C = gb.add_node(u8string_view{u8"C", 1});
        auto D = gb.add_node(u8string_view{u8"D", 1});
        gb.add_edge(A, B);
        gb.add_edge(B, C);
        gb.add_edge(C, D);
        gb.add_edge(D, A);  // 4-cycle requires deeper traversal than limit

        auto res = detect_cycles(gb.adj);
        if(!res.empty())
        {
            // Expect early termination -> empty result under strict limit
            ::uwvm2::uwvm::utils::depend::recursion_depth_limit = saved_limit;
            ::fast_io::fast_terminate();
        }
    }

    // Restore limit
    ::uwvm2::uwvm::utils::depend::recursion_depth_limit = saved_limit;
}


