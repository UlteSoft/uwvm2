// Native semantic probe for opaque aggregate references. This is not a GC
// collector test: all objects in a live store remain allocated until teardown.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace r = ::uwvm2::uwvm::runtime::storage;
    namespace g = ::uwvm2::object::global;
    constexpr unsigned worker_count{4u};
    constexpr unsigned objects_per_worker{8192u};

    void check(bool condition, char const* label)
    {
        if(!condition)
        {
            ::fast_io::io::perr("FAIL opaque GC token: ", ::fast_io::mnp::os_c_str(label), "\n");
            ::fast_io::fast_terminate();
        }
    }

    t::recursive_type_section make_types()
    {
        t::recursive_type_section section{};
        section.type_count = 1u;
        t::recursive_group group{};
        group.first_type_index = 0u;
        t::sub_type structure{};
        structure.kind = t::composite_kind::struct_;
        t::field_type field{};
        field.storage.value.kind = t::value_kind::i32;
        field.mutable_ = true;
        structure.fields.push_back(::std::move(field));
        group.types.push_back(::std::move(structure));
        section.groups.push_back(::std::move(group));
        return section;
    }
}

int main()
{
    auto section{make_types()};
    ::std::array<::std::shared_ptr<r::gc_lease_owner>, 2u> owners{
        ::std::make_shared<r::gc_lease_owner>(), ::std::make_shared<r::gc_lease_owner>()};
    ::std::array<::std::shared_ptr<r::gc_object_store>, 2u> stores{
        ::std::make_shared<r::gc_object_store>(section, owners[0]),
        ::std::make_shared<r::gc_object_store>(section, owners[1])};
    check(stores[0]->valid() && stores[1]->valid(), "both type layouts validate");
    ::std::array<::std::weak_ptr<r::gc_object_store>, 2u> weak{stores[0], stores[1]};
    ::std::vector<g::wasm_global_ref_t> tokens(worker_count * objects_per_worker);
    ::std::atomic<unsigned> ready{}, start{};
    ::std::array<::std::thread, worker_count> workers{};
    for(unsigned worker{}; worker != worker_count; ++worker)
    {
        workers[worker] = ::std::thread{[&, worker]
        {
            auto& store{*stores[worker & 1u]};
            ready.fetch_add(1u, ::std::memory_order_release);
            while(start.load(::std::memory_order_acquire) == 0u)
            { ::std::this_thread::yield(); }
            for(unsigned index{}; index != objects_per_worker; ++index)
            {
                auto& token{tokens[worker * objects_per_worker + index]};
                check(store.struct_new_default(0u, token) == r::gc_object_status::ok,
                      "parallel struct.new_default");
                check(token.kind == g::wasm_ref_kind::wasm_struct && token.storage.ptr != nullptr,
                      "published aggregate token kind");
            }
        }};
    }
    while(ready.load(::std::memory_order_acquire) != worker_count)
    { ::std::this_thread::yield(); }
    start.store(1u, ::std::memory_order_release);
    for(auto& worker : workers) { worker.join(); }

    ::std::unordered_set<::std::uintptr_t> issued{};
    issued.reserve(tokens.size() * 2uz);
    ::std::uintptr_t low{(::std::numeric_limits<::std::uintptr_t>::max)()}, high{};
    for(auto const& token : tokens)
    {
        auto const id{reinterpret_cast<::std::uintptr_t>(token.storage.ptr)};
        check(id >= 0x10000u && issued.insert(id).second,
              "all four threads and both stores issue unique nonzero token IDs");
        if(id < low) { low = id; }
        if(id > high) { high = id; }
    }
    r::gc_object_value value{};
    for(unsigned worker{}; worker != worker_count; ++worker)
    {
        auto& store{*stores[worker & 1u]};
        for(unsigned index{}; index < objects_per_worker; index += 257u)
        {
            auto const& token{tokens[worker * objects_per_worker + index]};
            check(store.struct_get(token, 0u, false, value) == r::gc_object_status::ok &&
                  value.as<::std::uint32_t>() == 0u, "parallel local token lookup");
        }
    }
    g::wasm_global_ref_t forged{};
    forged.kind = g::wasm_ref_kind::wasm_struct;
    forged.storage.ptr = reinterpret_cast<void*>(1u);
    check(stores[0]->struct_get(forged, 0u, false, value) ==
          r::gc_object_status::invalid_reference, "forged token 1 rejected without dereference");
    forged.storage.ptr = reinterpret_cast<void*>((::std::numeric_limits<::std::uintptr_t>::max)());
    check(stores[1]->struct_get(forged, 0u, false, value) ==
          r::gc_object_status::invalid_reference, "forged high token rejected without dereference");

    // The block-registry candidate publishes ownership for a 1024-ID block
    // before all its IDs have been issued. An unissued ID in that real block
    // must still fail the exact local membership lookup without dereference.
    auto probe_owner{::std::make_shared<r::gc_lease_owner>()};
    auto probe_store{::std::make_shared<r::gc_object_store>(section, probe_owner)};
    check(probe_store->valid(), "unissued-ID source store validates");
    g::wasm_global_ref_t issued_in_block{};
    check(probe_store->struct_new_default(0u, issued_in_block) == r::gc_object_status::ok,
          "one ID issued from a fresh block");
    auto const block_first{reinterpret_cast<::std::uintptr_t>(issued_in_block.storage.ptr)};
    check((block_first & 1023u) == 0u, "first issued ID starts an aligned token block");
    forged.storage.ptr = reinterpret_cast<void*>(block_first + 1u);
    check(stores[0]->struct_get(forged, 0u, false, value) ==
          r::gc_object_status::invalid_reference,
          "foreign unissued ID inside registered owner block rejected");
    probe_store.reset();
    probe_owner.reset();

    // This intentionally does not retain a cross-store reference or lease.
    // [tokens] contains opaque guest identities only, never owning pointers.
    owners[0].reset();
    owners[1].reset();
    stores[0].reset();
    stores[1].reset();
    check(weak[0].expired() && weak[1].expired(), "both module stores unload");

    auto fresh_owner{::std::make_shared<r::gc_lease_owner>()};
    auto fresh_store{::std::make_shared<r::gc_object_store>(section, fresh_owner)};
    check(fresh_store->valid(), "new module validates after old unload");
    for(unsigned index{}; index != objects_per_worker; ++index)
    {
        g::wasm_global_ref_t fresh{};
        check(fresh_store->struct_new_default(0u, fresh) == r::gc_object_status::ok,
              "new module publishes aggregate");
        check(issued.insert(reinterpret_cast<::std::uintptr_t>(fresh.storage.ptr)).second,
              "new module never reissues a retired token");
    }
    for(auto const& stale : tokens)
    {
        check(fresh_store->struct_get(stale, 0u, false, value) ==
              r::gc_object_status::invalid_reference,
              "stale token rejected after source module unload");
    }
    ::fast_io::io::println("PASS opaque GC token; workers=", ::fast_io::mnp::dec(worker_count),
                           " stores=2 initial_objects=",
                           ::fast_io::mnp::dec(worker_count * objects_per_worker),
                           " new_objects=", ::fast_io::mnp::dec(objects_per_worker),
                           " min_token=", ::fast_io::mnp::dec(low),
                           " max_token=", ::fast_io::mnp::dec(high));
}
