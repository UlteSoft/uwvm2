#include <uwvm2/runtime/lib/uwvm_runtime_execution_entry.h>

#define BOOST_UNORDERED_DISABLE_PARALLEL_ALGORITHMS
#include <boost/unordered/concurrent_node_map.hpp>

#include <cstddef>
#include <exception>
#include <limits>
#include <memory>

namespace
{
    struct simulated_thread_state
    {
        ::std::size_t metadata_callback_depth{};
    };

    using simulated_thread_state_map = ::boost::unordered::concurrent_node_map<::std::size_t, simulated_thread_state>;

    class simulated_metadata_callback_scope
    {
        simulated_thread_state_map* states{};
        ::std::size_t thread_id{};
        bool entered{};
        bool owns_inserted_node{};

    public:
        explicit simulated_metadata_callback_scope(simulated_thread_state_map& map, ::std::size_t id) noexcept
            : states{::std::addressof(map)}, thread_id{id}
        {
            bool enter_succeeded{};
            owns_inserted_node = states->try_emplace_and_visit(
                thread_id,
                [&](auto& kv) constexpr noexcept
                {
                    enter_succeeded = ::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_enter(
                        kv.second.metadata_callback_depth);
                },
                [&](auto& kv) constexpr noexcept
                {
                    enter_succeeded = ::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_enter(
                        kv.second.metadata_callback_depth);
                });
            entered = enter_succeeded;
        }

        simulated_metadata_callback_scope(simulated_metadata_callback_scope const&) = delete;
        simulated_metadata_callback_scope& operator= (simulated_metadata_callback_scope const&) = delete;

        [[nodiscard]] constexpr bool ready() const noexcept { return entered; }
        [[nodiscard]] constexpr bool owns_node() const noexcept { return owns_inserted_node; }

        [[nodiscard]] bool close() noexcept
        {
            if(!entered) { return false; }
            entered = false;

            bool leave_succeeded{};
            ::std::size_t remaining_depth{(::std::numeric_limits<::std::size_t>::max)()};
            auto const visited_count{states->visit(
                thread_id,
                [&](auto& kv) constexpr noexcept
                {
                    leave_succeeded = ::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_leave(
                        kv.second.metadata_callback_depth);
                    remaining_depth = kv.second.metadata_callback_depth;
                })};
            if(visited_count != 1u || !leave_succeeded) { return false; }

            if(owns_inserted_node)
            {
                if(!::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_owned_node_should_erase(
                       owns_inserted_node, remaining_depth))
                {
                    return false;
                }
                // Match production ordering: erase only after the visitor and its shard lock have returned.
                if(states->erase(thread_id) != 1u) { return false; }
            }
            return true;
        }

        ~simulated_metadata_callback_scope()
        {
            if(entered && !close()) { ::std::terminate(); }
        }
    };

    [[nodiscard]] bool depth_is(simulated_thread_state_map& states, ::std::size_t id, ::std::size_t expected) noexcept
    {
        bool matched{};
        auto const visited_count{states.visit(id, [&](auto const& kv) noexcept { matched = kv.second.metadata_callback_depth == expected; })};
        return visited_count == 1u && matched;
    }
}

int main()
{
    namespace execution = ::uwvm2::runtime::lib::details;

    // A metadata-only worker owns the node it inserted. A nested callback observes the same node but cannot erase it.
    simulated_thread_state_map worker_states{};
    simulated_metadata_callback_scope outer{worker_states, 11u};
    if(!outer.ready() || !outer.owns_node() || worker_states.size() != 1u || !depth_is(worker_states, 11u, 1u)) { return 1; }
    {
        simulated_metadata_callback_scope inner{worker_states, 11u};
        if(!inner.ready() || inner.owns_node() || worker_states.size() != 1u || !depth_is(worker_states, 11u, 2u)) { return 2; }
        if(execution::runtime_compilation_metadata_callback_owned_node_should_erase(outer.owns_node(), 2u)) { return 3; }
        if(!inner.close() || worker_states.size() != 1u || !depth_is(worker_states, 11u, 1u)) { return 4; }
    }
    if(!outer.close() || !worker_states.empty()) { return 5; }

    // A pre-existing execution/main-thread node is owned elsewhere. Leaving its outermost metadata callback restores
    // depth to zero but must preserve that node for the surrounding runtime entry.
    if(!worker_states.try_emplace(22u)) { return 6; }
    simulated_metadata_callback_scope existing_node_scope{worker_states, 22u};
    if(!existing_node_scope.ready() || existing_node_scope.owns_node() || !depth_is(worker_states, 22u, 1u)) { return 7; }
    if(!existing_node_scope.close() || worker_states.size() != 1u || !depth_is(worker_states, 22u, 0u)) { return 8; }

    if(execution::runtime_compilation_metadata_callback_owned_node_should_erase(false, 0u) ||
       execution::runtime_compilation_metadata_callback_owned_node_should_erase(true, 1u))
    {
        return 9;
    }
    return 0;
}
