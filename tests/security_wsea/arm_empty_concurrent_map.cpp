#include <boost/unordered/concurrent_node_map.hpp>
#include <atomic>
#include <cstdio>
#include <thread>
#include <utility>

int main()
{
    using map_type = boost::unordered::concurrent_node_map<int, int>;
    map_type empty;
    std::atomic<bool> okay{true};
    auto misses = [&] {
        for (int i{}; i != 256; ++i)
            if (empty.cvisit(i, [](auto const&) {}) != 0 || empty.count(i) != 0)
                okay.store(false, std::memory_order_relaxed);
    };
    std::thread first{misses}, second{misses};
    misses();
    first.join();
    second.join();
    if (!okay.load()) return 1;
    if (!empty.try_emplace(7, 11)) return 2;
    int value{};
    if (empty.cvisit(7, [&](auto const& item) { value = item.second; }) != 1 || value != 11) return 3;
    map_type moved{std::move(empty)};
    if (empty.cvisit(7, [](auto const&) {}) != 0 || moved.count(7) != 1) return 4;
    moved.clear();
    moved.rehash(0);
    if (moved.cvisit(7, [](auto const&) {}) != 0) return 5;
    std::puts("PASS concurrent empty, populated, moved-from and released-group lookups");
}
