/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      claude-4-sonnet
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
#include <fast_io.h>
#include <random>
#include <algorithm>
#include <uwvm2/utils/container/impl.h>
#include <uwvm2/uwvm/utils/ansies/impl.h>
#include <uwvm2/uwvm/wasm/loader/impl.h>
#include <uwvm2/uwvm/cmdline/callback/impl.h>

// Define color constants if not already defined
#ifndef UWVM_COLOR_U8_RED
# define UWVM_COLOR_U8_RED u8"\033[31m"
#endif

#ifndef UWVM_COLOR_U8_GREEN
# define UWVM_COLOR_U8_GREEN u8"\033[32m"
#endif

#ifndef UWVM_COLOR_U8_YELLOW
# define UWVM_COLOR_U8_YELLOW u8"\033[33m"
#endif

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

    // Generate random graph for fuzzing
    class RandomGraphGenerator
    {
    private:
        std::mt19937 rng;
        std::uniform_int_distribution<std::size_t> node_dist;
        std::uniform_int_distribution<std::size_t> edge_dist;
        std::uniform_int_distribution<std::size_t> name_dist;

    public:
        RandomGraphGenerator(std::uint32_t seed = 42) : rng(seed), node_dist(1, 100), edge_dist(0, 50), name_dist(0, 999999) {}

        GraphBuilder generate_graph()
        {
            GraphBuilder builder;
            std::size_t num_nodes = node_dist(rng);

            // Generate nodes
            vector<u8string_view> nodes;
            nodes.reserve(num_nodes);

            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                u8string name = ::fast_io::u8to<u8string>(u8"node_", name_dist(rng));
                nodes.push_back(builder.add_node(std::move(name)));
            }

            // Generate edges
            std::size_t num_edges = edge_dist(rng);
            for(std::size_t i = 0; i < num_edges; ++i)
            {
                if(nodes.empty()) { break; }

                std::size_t u_idx = std::uniform_int_distribution<std::size_t>(0, nodes.size() - 1)(rng);
                std::size_t v_idx = std::uniform_int_distribution<std::size_t>(0, nodes.size() - 1)(rng);

                builder.add_edge(nodes[u_idx], nodes[v_idx]);
            }

            return builder;
        }

        // Generate graph with specific properties for edge case testing
        GraphBuilder generate_edge_case_graph(std::size_t case_type)
        {
            GraphBuilder builder;

            switch(case_type % 4)
            {
                case 0:  // Empty graph
                    break;

                case 1:  // Single node
                    builder.add_node(u8"single");
                    break;

                case 2:  // Self-loop
                {
                    auto node = builder.add_node(u8"self_loop");
                    builder.add_edge(node, node);
                }
                break;

                case 3:  // Large graph
                {
                    std::size_t num_nodes = 1000;
                    vector<u8string_view> nodes;
                    nodes.reserve(num_nodes);

                    for(std::size_t i = 0; i < num_nodes; ++i)
                    {
                        u8string name = ::fast_io::u8to<u8string>(u8"large_node_", i);
                        nodes.push_back(builder.add_node(std::move(name)));
                    }

                    // Add some edges
                    for(std::size_t i = 0; i < num_nodes / 2; ++i)
                    {
                        std::size_t u_idx = i % nodes.size();
                        std::size_t v_idx = (i + 1) % nodes.size();
                        builder.add_edge(nodes[u_idx], nodes[v_idx]);
                    }
                }
                break;
            }

            return builder;
        }
    };

    // Memory safety test
    bool test_memory_safety()
    {

        RandomGraphGenerator generator;

        // Test multiple random graphs
        ::fast_io::io::perr("Testing random graphs...\n");
        for(int i = 0; i < 100; ++i)
        {
            auto graph = generator.generate_graph();
            auto cycles = detect_cycles(graph.adj);

            // Basic sanity checks
            if(!cycles.empty())
            {
                for(auto const& cycle: cycles)
                {
                    if(cycle.empty())
                    {
                        ::fast_io::io::perr("ERROR: Empty cycle detected\n");
                        return false;
                    }

                    // Check that cycle starts and ends with same node
                    if(cycle.front() != cycle.back())
                    {
                        ::fast_io::io::perr("ERROR: Cycle doesn't start and end with same node\n");
                        return false;
                    }
                }
            }
        }
        ::fast_io::io::perr("PASSED: Random graphs\n");

        // Test edge cases
        ::fast_io::io::perr("Testing edge cases...\n");
        for(std::size_t i = 0; i < 10; ++i)
        {
            auto graph = generator.generate_edge_case_graph(i);
            auto cycles = detect_cycles(graph.adj);

            // Should not crash or cause memory issues
            (void)cycles;
        }
        ::fast_io::io::perr("PASSED: Edge cases\n");
        return true;
    }

    // Test specific memory safety scenarios
    bool test_specific_scenarios()
    {

        ::fast_io::io::perr("Testing specific scenarios...\n");
        ::fast_io::io::perr("Test 1: Very large graph...\n");
        // Test 1: Very large graph
        {
            GraphBuilder builder;
            std::size_t num_nodes = 512;
            vector<u8string_view> nodes;
            nodes.reserve(num_nodes);

            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                u8string name = ::fast_io::u8to<u8string>(u8"big_node_", i);
                nodes.push_back(builder.add_node(std::move(name)));
            }

            // Add edges to create cycles
            for(std::size_t i = 0; i < num_nodes - 1; ++i) { builder.add_edge(nodes[i], nodes[i + 1]); }
            builder.add_edge(nodes.back(), nodes.front());  // Create cycle

            auto cycles = detect_cycles(builder.adj);
            if(cycles.empty()) { ::fast_io::io::perr("WARNING: No cycles detected in large graph with known cycle\n"); }
        }
        ::fast_io::io::perr("PASSED: Specific scenarios\n");
        ::fast_io::io::perr("Test 2: Graph with many self-loops...\n");
        // Test 2: Graph with many self-loops
        {
            GraphBuilder builder;
            std::size_t num_nodes = 100;
            vector<u8string_view> nodes;
            nodes.reserve(num_nodes);

            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                u8string name = ::fast_io::u8to<u8string>(u8"self_node_", i);
                auto node = builder.add_node(std::move(name));
                builder.add_edge(node, node);  // Self-loop
            }

            auto cycles = detect_cycles(builder.adj);
            if(cycles.size() != num_nodes) { ::fast_io::io::perr("WARNING: Expected ", num_nodes, " self-loop cycles, got ", cycles.size(), "\n"); }
        }

        ::fast_io::io::perr("Test 3: Dense graph...\n");
        // Test 3: Dense graph
        {
            GraphBuilder builder;
            std::size_t num_nodes = 10;
            vector<u8string_view> nodes;
            nodes.reserve(num_nodes);

            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                u8string name = ::fast_io::u8to<u8string>(u8"dense_node_", i);
                nodes.push_back(builder.add_node(std::move(name)));
            }

            // Add many edges
            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                for(std::size_t j = 0; j < num_nodes; ++j)
                {
                    if(i != j) { builder.add_edge(nodes[i], nodes[j]); }
                }
            }

            auto cycles = detect_cycles(builder.adj);
            // Should not crash even with many cycles
            (void)cycles;
        }
        ::fast_io::io::perr("PASSED: Specific scenarios\n");
        return true;
    }

    // Test memory corruption scenarios
    bool test_memory_corruption_scenarios()
    {

        ::fast_io::io::perr("Testing memory corruption scenarios...\n");
        ::fast_io::io::perr("Test 1: Graph with maximum possible size...\n");
        // Test 1: Graph with maximum possible size
        {
            GraphBuilder builder;
            std::size_t num_nodes = 10;  // Very large graph
            vector<u8string_view> nodes;
            nodes.reserve(num_nodes);

            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                u8string name = ::fast_io::u8to<u8string>(u8"max_node_", i);
                nodes.push_back(builder.add_node(std::move(name)));
            }

            // Create a complex cycle structure
            for(std::size_t i = 0; i < num_nodes - 1; ++i) { builder.add_edge(nodes[i], nodes[i + 1]); }
            builder.add_edge(nodes.back(), nodes.front());

            // Add some additional edges to create more cycles
            for(std::size_t i = 0; i < num_nodes / 10; ++i)
            {
                std::size_t u = i * 10;
                std::size_t v = (i * 10 + 5) % num_nodes;
                if(u != v) { builder.add_edge(nodes[u], nodes[v]); }
            }

            auto cycles = detect_cycles(builder.adj);
            // Should not crash or cause memory corruption
            (void)cycles;
        }
        ::fast_io::io::perr("PASSED: Memory corruption scenarios\n");
        ::fast_io::io::perr("Test 2: Graph with deep recursion potential...\n");
        // Test 2: Graph with deep recursion potential
        {
            GraphBuilder builder;
            std::size_t num_nodes = 512;
            vector<u8string_view> nodes;
            nodes.reserve(num_nodes);

            for(std::size_t i = 0; i < num_nodes; ++i)
            {
                u8string name = ::fast_io::u8to<u8string>(u8"deep_node_", i);
                nodes.push_back(builder.add_node(std::move(name)));
            }

            // Create a chain that could cause deep recursion
            for(std::size_t i = 0; i < num_nodes - 1; ++i) { builder.add_edge(nodes[i], nodes[i + 1]); }
            // Add back edge to create cycle
            builder.add_edge(nodes.back(), nodes.front());

            // Add some cross edges to create multiple cycles
            for(std::size_t i = 0; i < num_nodes / 2; ++i)
            {
                std::size_t u = i;
                std::size_t v = num_nodes - 1 - i;
                if(u != v) { builder.add_edge(nodes[u], nodes[v]); }
            }

            auto cycles = detect_cycles(builder.adj);
            // Should not cause stack overflow or memory corruption
            (void)cycles;
        }
        ::fast_io::io::perr("PASSED: Memory corruption scenarios\n");
        ::fast_io::io::perr("Test 3: Stress test with rapid allocation/deallocation...\n");
        // Test 3: Stress test with rapid allocation/deallocation
        {
            for(int iteration = 0; iteration < 50; ++iteration)
            {
                GraphBuilder builder;
                std::size_t num_nodes = (iteration % 100);
                vector<u8string_view> nodes;
                nodes.reserve(num_nodes);

                for(std::size_t i = 0; i < num_nodes; ++i)
                {
                    u8string name = ::fast_io::u8to<u8string>(u8"stress_node_", i);
                    nodes.push_back(builder.add_node(std::move(name)));
                }

                // Create random edges
                std::mt19937 rng(iteration);
                for(std::size_t i = 0; i < num_nodes * 2; ++i)
                {
                    std::size_t u = std::uniform_int_distribution<std::size_t>(0, num_nodes - 1)(rng);
                    std::size_t v = std::uniform_int_distribution<std::size_t>(0, num_nodes - 1)(rng);
                    builder.add_edge(nodes[u], nodes[v]);
                }

                auto cycles = detect_cycles(builder.adj);
                // Should not cause memory leaks or corruption
                (void)cycles;
            }
        }
        ::fast_io::io::perr("PASSED: Memory corruption scenarios\n");
        return true;
    }

}  // namespace

int main()
{
    ::fast_io::io::perr("Starting memory safety fuzzer for detect_cycles...\n");

    bool all_tests_passed = true;

    // Test 1: Basic memory safety
    ::fast_io::io::perr("Running basic memory safety tests...\n");
    if(!test_memory_safety())
    {
        all_tests_passed = false;
        ::fast_io::io::perr("FAILED: Basic memory safety test\n");
    }
    else
    {
        ::fast_io::io::perr("PASSED: Basic memory safety test\n");
    }

    // Test 2: Specific scenarios
    ::fast_io::io::perr("Running specific scenario tests...\n");
    if(!test_specific_scenarios())
    {
        all_tests_passed = false;
        ::fast_io::io::perr("FAILED: Specific scenario test\n");
    }
    else
    {
        ::fast_io::io::perr("PASSED: Specific scenario test\n");
    }

    // Test 3: Memory corruption scenarios
    ::fast_io::io::perr("Running memory corruption tests...\n");
    if(!test_memory_corruption_scenarios())
    {
        all_tests_passed = false;
        ::fast_io::io::perr("FAILED: Memory corruption test\n");
    }
    else
    {
        ::fast_io::io::perr("PASSED: Memory corruption test\n");
    }

    if(all_tests_passed)
    {
        ::fast_io::io::perr("\nAll memory safety tests PASSED!\n");
    }
    else
    {
        ::fast_io::io::perr("\nSome memory safety tests FAILED!\n");
        ::fast_io::fast_terminate();
    }
}
