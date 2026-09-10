#include <uwvm2/runtime/lib/uwvm_runtime_logical_activation_overlap.h>

#include <array>
#include <cstddef>

namespace
{
    struct frame
    {
        ::std::size_t module_id{};
        ::std::size_t function_index{};
    };
}

int main()
{
    namespace details = ::uwvm2::runtime::lib::details;

    // Repeated identities at different positions are separate recursive activations, not a set duplicate.
    constexpr ::std::array recursive_live{frame{1u, 7u}, frame{1u, 7u}, frame{2u, 9u}};
    constexpr ::std::array recursive_snapshot{frame{1u, 7u}, frame{1u, 7u}};
    static_assert(details::runtime_logical_activation_positional_overlap(
                      recursive_live.data(), recursive_live.size(), recursive_snapshot.data(), recursive_snapshot.size(), 0uz) == 2uz);

    // Only equal identities at the same activation position overlap; a later divergence is preserved for fallback output.
    constexpr ::std::array divergent_live{frame{1u, 7u}, frame{3u, 4u}};
    constexpr ::std::array divergent_snapshot{frame{1u, 7u}, frame{1u, 7u}};
    static_assert(details::runtime_logical_activation_positional_overlap(
                      divergent_live.data(), divergent_live.size(), divergent_snapshot.data(), divergent_snapshot.size(), 0uz) == 1uz);

    // A bounded snapshot can start below older live frames. Compare at its captured absolute position, not at live[0].
    constexpr ::std::array truncated_live{frame{9u, 1u}, frame{9u, 2u}, frame{1u, 7u}, frame{1u, 7u}};
    static_assert(details::runtime_logical_activation_positional_overlap(
                      truncated_live.data(), truncated_live.size(), recursive_snapshot.data(), recursive_snapshot.size(), 2uz) == 2uz);

    static_assert(details::runtime_logical_activation_positional_overlap<frame>(
                      nullptr, 0uz, recursive_snapshot.data(), recursive_snapshot.size(), 0uz) == 0uz);
    static_assert(details::runtime_logical_activation_positional_overlap(
                      recursive_live.data(), recursive_live.size(), recursive_snapshot.data(), recursive_snapshot.size(), 4uz) == 0uz);
    return 0;
}
