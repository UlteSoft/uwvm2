#include <fast_io.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace r = ::uwvm2::uwvm::runtime::storage;

static void check(bool condition)
{
    if(!condition) { ::fast_io::fast_terminate(); }
}

int main()
{
    t::recursive_type_section section{};
    section.type_count = 1u;
    t::recursive_group group{};
    group.first_type_index = 0u;
    t::sub_type structure{};
    structure.kind = t::composite_kind::struct_;
    t::field_type immutable{};
    immutable.storage.value.kind = t::value_kind::i32;
    t::field_type mutable_field{immutable};
    mutable_field.mutable_ = true;
    structure.fields.push_back(immutable);
    structure.fields.push_back(mutable_field);
    group.types.push_back(::std::move(structure));
    section.groups.push_back(::std::move(group));

    r::gc_object_store store{section};
    check(store.valid());
    r::gc_object_value values[]{r::gc_object_value::i32(7u), r::gc_object_value::i32(0u)};
    r::gc_reference reference{};
    check(store.struct_new(0u, values, 2u, reference) == r::gc_object_status::ok);
    constexpr ::std::uint32_t iterations{10000000u};
    ::std::uint64_t sum{};
    r::gc_object_value value{};
    auto elapsed = [&](auto&& body)
    {
        auto const begin{::std::chrono::steady_clock::now()};
        body();
        auto const end{::std::chrono::steady_clock::now()};
        return static_cast<double>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count()) /
               iterations;
    };
    auto const native{elapsed([&]
    {
        for(::std::uint32_t i{}; i != iterations; ++i)
        {
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            sum += values[0].as<::std::uint32_t>();
        }
    })};
    auto const immutable_get{elapsed([&]
    {
        for(::std::uint32_t i{}; i != iterations; ++i)
        {
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            check(store.struct_get(reference, 0u, false, value) == r::gc_object_status::ok);
            sum += value.as<::std::uint32_t>();
        }
    })};
    auto const mutable_get{elapsed([&]
    {
        for(::std::uint32_t i{}; i != iterations; ++i)
        {
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            check(store.struct_get(reference, 1u, false, value) == r::gc_object_status::ok);
            sum += value.as<::std::uint32_t>();
        }
    })};
    auto const mutable_pair{elapsed([&]
    {
        for(::std::uint32_t i{}; i != iterations; ++i)
        {
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            check(store.struct_set(reference, 1u, r::gc_object_value::i32(i)) == r::gc_object_status::ok);
            check(store.struct_get(reference, 1u, false, value) == r::gc_object_status::ok);
            sum += value.as<::std::uint32_t>();
        }
    })};
    ::fast_io::io::println("gc_object_perf native ", ::fast_io::mnp::fixed(native, 3u),
        " immutable_get ", ::fast_io::mnp::fixed(immutable_get, 3u),
        " mutable_get ", ::fast_io::mnp::fixed(mutable_get, 3u),
        " mutable_pair ", ::fast_io::mnp::fixed(mutable_pair, 3u), " checksum ", sum);
}
