// Concurrent GC publication, foreign lookup, module lease, teardown, and
// stale-handle safety qualification. Run only in the remote test cgroup.
#include <fast_io.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <array>
#include <barrier>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace r = ::uwvm2::uwvm::runtime::storage;

[[nodiscard]] static t::recursive_type_section one_struct()
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
    structure.fields.push_back(field);
    group.types.push_back(::std::move(structure));
    section.groups.push_back(::std::move(group));
    return section;
}

static void require(bool condition)
{ if(!condition) { ::fast_io::fast_terminate(); } }

int main()
{
    auto section{one_struct()};
    constexpr ::std::size_t workers{4uz};
    constexpr ::std::size_t objects_per_worker{10000uz};
    ::std::array<::std::shared_ptr<r::gc_lease_owner>, workers> roots{};
    ::std::array<::std::shared_ptr<r::gc_object_store>, workers> stores{};
    ::std::array<::std::weak_ptr<r::gc_object_store>, workers> weak{};
    ::std::vector<r::gc_reference> references(workers * objects_per_worker);
    for(::std::size_t worker{}; worker != workers; ++worker)
    {
        roots[worker] = ::std::make_shared<r::gc_lease_owner>();
        stores[worker] = ::std::make_shared<r::gc_object_store>(section, roots[worker]);
        weak[worker] = stores[worker];
        require(stores[worker]->valid());
    }
    ::std::array<::std::thread, workers> publishers{};
    for(::std::size_t worker{}; worker != workers; ++worker)
    {
        publishers[worker] = ::std::thread([&, worker]
        {
            auto& store{*stores[worker]};
            for(::std::size_t i{}; i != objects_per_worker; ++i)
            {
                auto& reference{references[worker * objects_per_worker + i]};
                if(store.struct_new_default(0u, reference) != r::gc_object_status::ok)
                { ::fast_io::fast_terminate(); }
            }
        });
    }
    for(auto& publisher : publishers) { publisher.join(); }
    auto sink_roots{::std::make_shared<r::gc_lease_owner>()};
    auto sink{::std::make_shared<r::gc_object_store>(section, sink_roots)};
    require(sink->valid());
    for(auto const& reference : references)
    {
        r::gc_object_value observed{};
        require(sink->struct_get(reference, 0u, false, observed) == r::gc_object_status::ok &&
                observed.as<::std::uint32_t>() == 0u);
    }
    for(auto& store : stores) { store.reset(); }
    for(auto& root : roots) { root.reset(); }
    for(auto const& source : weak) { require(!source.expired()); }
    sink.reset();
    sink_roots.reset();
    for(auto const& source : weak) { require(source.expired()); }
    auto verifier_roots{::std::make_shared<r::gc_lease_owner>()};
    auto verifier{::std::make_shared<r::gc_object_store>(section, verifier_roots)};
    r::gc_object_value ignored{};
    for(::std::size_t worker{}; worker != workers; ++worker)
    {
        require(verifier->struct_get(references[worker * objects_per_worker], 0u, false, ignored) ==
                r::gc_object_status::invalid_reference);
    }

    // A source can reach zero owners concurrently with an untrusted foreign
    // lookup. The matching bucket stripe must exclude teardown until weak_ptr
    // promotion has either pinned the source or failed cleanly.
    ::std::barrier phase{2};
    r::gc_reference racing_reference{};
    ::std::shared_ptr<r::gc_object_store> racing_sink{};
    ::std::thread reader{[&]
    {
        for(unsigned round{}; round != 1000u; ++round)
        {
            phase.arrive_and_wait();
            r::gc_object_value observed{};
            auto const status{racing_sink->struct_get(racing_reference, 0u, false, observed)};
            require(status == r::gc_object_status::ok ||
                    status == r::gc_object_status::invalid_reference);
            if(status == r::gc_object_status::ok)
            { require(observed.as<::std::uint32_t>() == 0u); }
            phase.arrive_and_wait();
        }
    }};
    for(unsigned round{}; round != 1000u; ++round)
    {
        auto source_roots{::std::make_shared<r::gc_lease_owner>()};
        auto source{::std::make_shared<r::gc_object_store>(section, source_roots)};
        auto receiver_roots{::std::make_shared<r::gc_lease_owner>()};
        racing_sink = ::std::make_shared<r::gc_object_store>(section, receiver_roots);
        require(source->struct_new_default(0u, racing_reference) == r::gc_object_status::ok);
        ::std::weak_ptr<r::gc_object_store> retired{source};
        phase.arrive_and_wait();
        source.reset();
        source_roots.reset();
        phase.arrive_and_wait();
        racing_sink.reset();
        receiver_roots.reset();
        require(retired.expired());
        require(verifier->struct_get(racing_reference, 0u, false, ignored) ==
                r::gc_object_status::invalid_reference);
    }
    reader.join();
}
