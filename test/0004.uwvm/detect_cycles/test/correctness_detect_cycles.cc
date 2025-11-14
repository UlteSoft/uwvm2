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

    // Canonicalize a cycle for comparison: remove duplicate last node, rotate so lexicographically smallest name comes first
    vector<u8string> canonicalize(vector<u8string_view> const& cyc)
    {
        // guard
        if(cyc.size() < 2) { return {}; }
        size_t n = cyc.size();
        // drop last if equal to first
        size_t m = (cyc.front() == cyc.back() ? n - 1 : n);
        vector<u8string> tmp;
        tmp.reserve(m);
        for(size_t i = 0; i < m; ++i) { tmp.emplace_back(cyc[i]); }
        // find min index
        size_t min_idx = 0;
        for(size_t i = 1; i < m; ++i)
        {
            if(tmp[i] < tmp[min_idx]) { min_idx = i; }
        }
        // rotate
        vector<u8string> norm;
        norm.reserve(m);
        for(size_t k = 0; k < m; ++k) { norm.emplace_back(tmp[(min_idx + k) % m]); }
        return norm;
    }

    // Canonical stringify: join with '\x01' to build unique key
    u8string make_key(vector<u8string> const& norm)
    {
        u8string key;
        // compute total length
        size_t total = 0;
        for(size_t i = 0; i < norm.size(); ++i) { total += norm[i].size() + 1; }
        key.reserve(total);
        for(size_t i = 0; i < norm.size(); ++i)
        {
            key.append(u8string_view{norm[i].data(), norm[i].size()});
            key.push_back_unchecked(static_cast<char8_t>(1));
        }
        return key;
    }

    // Compare set equality of cycles ignoring rotation and start-end duplication
    bool equal_cycles(vector<vector<u8string_view>> const& got, vector<vector<u8string>> const& expect)
    {
        unordered_flat_map<u8string, int> count;
        count.reserve(expect.size() * 2);
        for(auto const& e: expect)
        {
            auto key = make_key(e);
            ++count[key];
        }
        for(auto const& g: got)
        {
            auto norm = canonicalize(g);
            auto key = make_key(norm);
            auto it = count.find(key);
            if(it == count.end() || it->second == 0) { return false; }
            --it->second;
        }
        for(auto const& [_, v]: count)
        {
            if(v != 0) { return false; }
        }
        return true;
    }

}  // namespace

int main()
{
    // Case 1: no cycle
    {
        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        auto B = gb.add_node(u8string_view{u8"B", 1});
        gb.add_edge(A, B);
        auto res = detect_cycles(gb.adj);
        if(!res.empty()) { ::fast_io::fast_terminate(); }
    }

    // Case 2: 2-cycle A<->B (should only report once)
    {
        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        auto B = gb.add_node(u8string_view{u8"B", 1});
        gb.add_edge(A, B);
        gb.add_edge(B, A);
        auto res = detect_cycles(gb.adj);
        vector<vector<u8string>> expect{};  // rotation-insensitive
        {
            vector<u8string> cyc;
            cyc.emplace_back(u8string_view{u8"A", 1});
            cyc.emplace_back(u8string_view{u8"B", 1});
            expect.push_back(std::move(cyc));
        }
        if(!equal_cycles(res, expect)) { ::fast_io::fast_terminate(); }
    }

    // Case 3: 3-cycle A->B->C->A
    {
        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        auto B = gb.add_node(u8string_view{u8"B", 1});
        auto C = gb.add_node(u8string_view{u8"C", 1});
        gb.add_edge(A, B);
        gb.add_edge(B, C);
        gb.add_edge(C, A);
        auto res = detect_cycles(gb.adj);
        vector<vector<u8string>> expect{};
        {
            vector<u8string> cyc;
            cyc.emplace_back(u8string_view{u8"A", 1});
            cyc.emplace_back(u8string_view{u8"B", 1});
            cyc.emplace_back(u8string_view{u8"C", 1});
            expect.push_back(std::move(cyc));
        }
        if(!equal_cycles(res, expect)) { ::fast_io::fast_terminate(); }
    }

    // Case 4: multiple independent cycles
    {
        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        auto B = gb.add_node(u8string_view{u8"B", 1});
        auto C = gb.add_node(u8string_view{u8"C", 1});
        auto D = gb.add_node(u8string_view{u8"D", 1});
        auto E = gb.add_node(u8string_view{u8"E", 1});
        // A<->B, C->D->E->C
        gb.add_edge(A, B);
        gb.add_edge(B, A);
        gb.add_edge(C, D);
        gb.add_edge(D, E);
        gb.add_edge(E, C);
        auto res = detect_cycles(gb.adj);
        vector<vector<u8string>> expect{};
        {
            vector<u8string> c1;
            c1.emplace_back(u8string_view{u8"A", 1});
            c1.emplace_back(u8string_view{u8"B", 1});
            expect.push_back(std::move(c1));
            vector<u8string> c2;
            c2.emplace_back(u8string_view{u8"C", 1});
            c2.emplace_back(u8string_view{u8"D", 1});
            c2.emplace_back(u8string_view{u8"E", 1});
            expect.push_back(std::move(c2));
        }
        if(!equal_cycles(res, expect)) { ::fast_io::fast_terminate(); }
    }

    // Case 5: self-loop
    {
        GraphBuilder gb;
        auto A = gb.add_node(u8string_view{u8"A", 1});
        gb.add_edge(A, A);
        auto res = detect_cycles(gb.adj);
        if(res.size() != 1) { ::fast_io::fast_terminate(); }
        auto cyc = res.front();
        if(!(cyc.size() == 2 && cyc[0] == u8"A" && cyc[1] == u8"A")) { ::fast_io::fast_terminate(); }
    }
}

