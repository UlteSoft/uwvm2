// No float calls, even at -O0: exercise the real vendored table with -mno-sse.
// Integer references also cover inputs beyond float's exact-integer range and
// reject impossible reservations before touching an allocator-sized buffer.
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <new>

#if defined(BOOST_NO_EXCEPTIONS)
// Boost deliberately leaves this application hook undefined in no-exception
// builds. at() still references it even though every lookup below is present.
namespace boost
{
    BOOST_NORETURN void throw_exception(std::exception const&) { std::terminate(); }
    BOOST_NORETURN void throw_exception(std::exception const&, boost::source_location const&) { std::terminate(); }
}
#endif

namespace f = boost::unordered::detail::foa;
static_assert(f::slots_for_size(0) == 0);
static_assert(f::slots_for_size(7) == 8);
static_assert(f::slots_for_size(8) == 10);
static_assert(f::max_load_for_capacity(8) == 7);
static_assert(f::max_load_for_capacity(15) == 13);

int main()
{
    unsigned errors{};
    for (std::size_t n{}; n != 1000000; ++n)
    {
        errors += f::slots_for_size(n) != (n * 8 + 6) / 7;
        errors += f::max_load_for_capacity(n) != n * 7 / 8;
    }
    constexpr auto maximum = (std::numeric_limits<std::size_t>::max)();
    for (auto n : {maximum / 8, maximum / 4, maximum / 2, maximum / 2 + 1})
    {
        auto slots = f::slots_for_size(n);
        errors += slots < n || f::max_load_for_capacity(slots) != n;
    }
    boost::unordered_flat_map<std::size_t, std::size_t> values;
    values.reserve(20000);
    auto capacity = values.bucket_count();
    for (std::size_t i{}; i != 20000; ++i) { values.emplace(i, i ^ 12345); }
    errors += values.bucket_count() != capacity;
    auto copy = values;
    values.clear();
    values = copy;
    values.rehash(40000);
    for (std::size_t i{}; i != 20000; ++i) { errors += values.at(i) != (i ^ 12345); }
#if !defined(BOOST_NO_EXCEPTIONS)
    for (auto request : {maximum, maximum / 8 * 7})
    {
        try { values.reserve(request); ++errors; }
        catch (std::bad_alloc const&) {}
    }
    // The table remains usable after both rejected reservations.
    errors += values.size() != 20000 || values.at(19) != (19 ^ 12345);
#endif
    std::printf("Integer hash capacity: %u failures\n", errors);
    return errors != 0;
}
