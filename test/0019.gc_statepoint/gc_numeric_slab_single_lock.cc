// Private numeric-slab candidate correctness: real allocation, byte readback,
// closed admission collection, stale/forged keys and concurrent native actors.
// No VM timing or algorithm-speed assertion.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
using ref = gc::gc_reference;
using val = gc::gc_object_value;
using status = gc::gc_object_status;

// Actual O3/native object proof entry, test-only and never bound by generated
// Wasm. Caller owns this real store, source carrier, result and admission lease.
// Symbol is retained so LLVM objdump can inspect actual reserve/commit paths.
extern "C" [[gnu::noinline, gnu::used]] ::std::uint32_t
uwvm2_test_numeric_slab_one_field(gc::gc_object_store* store,
    val const* input, ref* output) noexcept
{
    return static_cast<::std::uint32_t>(store->struct_new(0u, input, 1uz, *output));
}
namespace
{
    ::std::atomic_size_t checks{};
    void require(bool condition, unsigned line) noexcept
    {
        checks.fetch_add(1uz, ::std::memory_order_relaxed);
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL numeric_slab_single_lock line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    t::value_kind numeric_kind(::std::size_t field) noexcept
    {
        constexpr ::std::array kinds{t::value_kind::i32, t::value_kind::i64,
            t::value_kind::f32, t::value_kind::f64, t::value_kind::v128};
        return kinds[field % kinds.size()];
    }
    val numeric_value(::std::size_t field, ::std::uint32_t number) noexcept
    {
        switch(numeric_kind(field))
        {
            case t::value_kind::i32: return val::i32(number);
            case t::value_kind::f32: return val::i32(0x7f800001u | (number & 0x003fffffu));
            case t::value_kind::i64: return val::i64((::std::uint64_t{number} << 32u) | number);
            case t::value_kind::f64: return val::i64(0x7ff0000000000001ull | number);
            case t::value_kind::v128: return val::from(::std::array<::std::uint64_t, 2uz>{number, ~::std::uint64_t{number}});
            default: ::fast_io::fast_terminate();
        }
    }
    auto schema()
    {
        t::recursive_type_section s{}; s.type_count = 8u;
        t::recursive_group group{};
        for(::std::size_t count{1uz}; count != 9uz; ++count)
        {
            t::sub_type type{}; type.kind = t::composite_kind::struct_;
            for(::std::size_t f{}; f != count; ++f)
            {
                t::field_type field{}; field.mutable_ = true;
                field.storage.value.kind = numeric_kind(f);
                type.fields.push_back(field);
            }
            group.types.push_back(::std::move(type));
        }
        s.groups.push_back(::std::move(group)); return s;
    }
    auto make(::std::shared_ptr<gc::gc_object_store> const& store,
              ::std::size_t count, ::std::uint32_t number)
    {
        ::std::array<val, 8uz> values{};
        for(::std::size_t f{}; f != count; ++f) { values[f] = numeric_value(f, number); }
        ref result{};
        if(count == 1uz)
        {
            CHECK(uwvm2_test_numeric_slab_one_field(store.get(), values.data(), ::std::addressof(result)) ==
                static_cast<::std::uint32_t>(status::ok));
        }
        else
        {
            CHECK(store->struct_new(static_cast<::std::uint_least32_t>(count - 1uz),
                values.data(), count, result) == status::ok);
        }
        for(::std::size_t f{}; f != count; ++f)
        {
            val observed{};
            CHECK(store->struct_get(result, f, false, observed) == status::ok);
            CHECK(::fast_io::freestanding::my_memcmp(observed.bits.data(), values[f].bits.data(), 16uz) == 0);
        }
        return result;
    }
    void collect(::std::shared_ptr<gc::gc_object_store> const& store,
                 ref const* roots, ::std::size_t count, status wanted, ::std::size_t dead)
    {
        auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        CHECK(static_cast<bool>(exclusive));
        ::std::array pins{store};
        auto const before{store->native_numeric_slab_statistics()};
        ::std::size_t reclaimed{999uz};
        CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
            pins.data(), pins.size(), roots, count, reclaimed) == wanted);
        CHECK(reclaimed == dead);
        auto const after{store->native_numeric_slab_statistics()};
        CHECK(after.reserved_slots == 0uz);
        if(wanted != status::ok)
        {
            CHECK(after.epoch == before.epoch);
            CHECK(after.allocated_slots == before.allocated_slots);
        }
    }
}
int main()
{
    auto declarations{schema()};
    auto leases{::std::make_shared<gc::gc_lease_owner>()};
    auto store{::std::make_shared<gc::gc_object_store>(declarations, leases)};
    CHECK(store->valid());
    constexpr ::std::size_t initial{8uz * 257uz};
    ::std::array<ref, initial + 512uz> roots{};
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        for(::std::size_t count{1uz}; count != 9uz; ++count)
        {
            for(::std::size_t n{}; n != 257uz; ++n)
            { roots[(count - 1uz) * 257uz + n] = make(store, count, static_cast<::std::uint32_t>(n)); }
        }
    }
    auto const first{store->native_numeric_slab_statistics()};
    CHECK(first.chunks == 16uz && first.allocated_slots == initial && first.reserved_slots == 0uz);
    auto forged{roots[0uz]};
    forged.storage.ptr = reinterpret_cast<void*>((::std::numeric_limits<::std::uintptr_t>::max)());
    ::std::array bad{roots[0uz], forged};
    collect(store, bad.data(), bad.size(), status::invalid_reference, 0uz);
    collect(store, roots.data(), initial, status::ok, 0uz);
    // Complete owning store/root vectors stay alive; no collector runs while
    // either shared native actor is admitted. Each worker owns a disjoint slice.
    auto worker{[&](::std::size_t actor) noexcept
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        for(::std::size_t n{}; n != 256uz; ++n)
        { roots[initial + actor * 256uz + n] = make(store, 4uz, static_cast<::std::uint32_t>(n + actor * 256uz)); }
    }};
    ::std::thread a{worker, 0uz}, b{worker, 1uz};
    a.join(); b.join();
    CHECK(store->native_numeric_slab_statistics().allocated_slots == roots.size());
    collect(store, roots.data(), roots.size(), status::ok, 0uz);
    auto const stale{roots[0uz]};
    roots.fill(ref{}); // Explicit semantic root retirement; stale is a refusal probe only.
    collect(store, nullptr, 0uz, status::ok, roots.size());
    auto const empty{store->native_numeric_slab_statistics()};
    CHECK(empty.chunks == 0uz && empty.allocated_slots == 0uz && empty.reserved_slots == 0uz && empty.backing_bytes == 0uz);
    ref replacement{};
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        CHECK(store->struct_new_default(0u, replacement) == status::ok);
        val zero{};
        CHECK(store->struct_get(replacement, 0uz, false, zero) == status::ok);
        CHECK(zero.as<::std::uint32_t>() == 0u);
    }
    CHECK(replacement.storage.ptr != stale.storage.ptr);
    val ignored{};
    CHECK(store->struct_get(stale, 0uz, false, ignored) == status::invalid_reference);
    collect(store, nullptr, 0uz, status::ok, 1uz);
    ::fast_io::io::println("numeric_slab_single_lock checks=", checks.load(),
        " initial=", initial, " concurrent=", 512u);
}
