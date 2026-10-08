// Source-bound native GC publication/foreign-handoff witness. Run through the
// Linux keeper's bounded cgroup; this is not a Wasm/JIT performance ranking.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <fast_io.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace publication_handoff_20261003
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace admission = ::uwvm2::runtime::gc;
    using reference = gc::gc_reference;
    using value = gc::gc_object_value;
    using status = gc::gc_object_status;
#if defined(UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION) && UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION == 1
    constexpr bool single_cas_enabled{true};
#else
    constexpr bool single_cas_enabled{false};
#endif
    void require(bool condition, unsigned line) noexcept
    {
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_publication_handoff line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define PUBLICATION_HANDOFF_CHECK(...) ::publication_handoff_20261003::require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    [[nodiscard]] bool number(::std::string_view argument, ::std::uint32_t& result) noexcept
    {
        if(argument.empty() || !::fast_io::char_category::is_c_digit(argument.front())) { return false; }
        // [argument.data(), argument.data()+argument.size()) is the complete
        // OS argument extent. The owned string_view bounds the end advance.
        // [safe                                                     ]
        // ^^ form an end only for the complete, nonempty argument.
        auto const* end{argument.data() + argument.size()};
        auto const parsed{::fast_io::parse_by_scan(argument.data(), end, ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }
    [[nodiscard]] type::field_type field(type::value_kind kind, bool mutable_,
        type::packed_kind packed = type::packed_kind::none)
    {
        type::field_type result{};
        result.mutable_ = mutable_;
        result.storage.packed = packed;
        result.storage.value.kind = kind;
        if(kind == type::value_kind::reference)
        {
            result.storage.value.nullable = true;
            result.storage.value.heap.code = 0;
        }
        return result;
    }
    [[nodiscard]] type::recursive_type_section declarations()
    {
        type::recursive_type_section section{};
        section.type_count = 2u;
        type::recursive_group group{};
        type::sub_type node{};
        node.kind = type::composite_kind::struct_;
        node.fields.push_back(field(type::value_kind::i32, true));
        node.fields.push_back(field(type::value_kind::i32, true, type::packed_kind::i16));
        node.fields.push_back(field(type::value_kind::v128, false));
        node.fields.push_back(field(type::value_kind::reference, true));
        group.types.push_back(::std::move(node));
        type::sub_type roots{};
        roots.kind = type::composite_kind::array;
        roots.fields.push_back(field(type::value_kind::reference, true));
        group.types.push_back(::std::move(roots));
        section.groups.push_back(::std::move(group));
        return section;
    }
    [[nodiscard]] ::std::uint32_t expected_word(unsigned worker, ::std::size_t index) noexcept
    { return static_cast<::std::uint32_t>(index) ^ (0xa5a50000u + static_cast<::std::uint32_t>(worker) * 0x10101u); }
    [[nodiscard]] ::std::array<::std::byte, 16uz> expected_vector(unsigned worker, ::std::size_t index) noexcept
    {
        ::std::array<::std::byte, 16uz> result{};
        auto const word{expected_word(worker, index)};
        for(::std::size_t byte{}; byte != result.size(); ++byte)
        {
            // [0,16) is the complete initialized native byte array. This
            // pattern checks exact SIMD bytes independently of host endianness.
            result[byte] = static_cast<::std::byte>(((word >> ((byte & 3uz) * 8uz)) ^ (byte * 13uz)) & 0xffu);
        }
        return result;
    }
    struct lane
    {
        ::std::vector<reference> references{};
        ::std::atomic<::std::size_t> published{};
    };
    struct context
    {
        // Strong canonical pins and real module-like lease lists remain live
        // across every worker, foreign lookup, paused collection and readback.
        admission::managed_entry_admission::shared_lease reader{admission::runtime_gc_entry_admission.enter()};
        ::std::array<::std::shared_ptr<gc::gc_lease_owner>, 2uz> leases{
            ::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
        ::std::array<::std::shared_ptr<gc::gc_object_store>, 2uz> stores{};
        ::std::array<lane, 4uz> lanes{};
        ::std::atomic<unsigned> ready{};
        ::std::atomic<bool> start{};
        unsigned workers{};
        ::std::size_t count{};
        bool split_stores{};
        ::std::uint64_t consumer_checksum{};
        ::std::size_t shared_stripes{};
        explicit context(unsigned worker_count, ::std::size_t units, bool split)
            : workers{worker_count}, count{units}, split_stores{split}
        {
            auto const schema{declarations()};
            for(::std::size_t index{}; index != stores.size(); ++index)
            {
                // [0,2) owns both complete canonical store/lease slots.
                stores[index] = ::std::make_shared<gc::gc_object_store>(schema, leases[index]);
                PUBLICATION_HANDOFF_CHECK(stores[index]->valid());
            }
            for(unsigned worker{}; worker != workers; ++worker) { lanes[worker].references.resize(count); }
        }
        [[nodiscard]] ::std::size_t source_store(unsigned worker) const noexcept
        { return split_stores ? worker & 1u : 0uz; }
        [[nodiscard]] ::std::size_t collect(reference const* roots, ::std::size_t root_count,
            ::std::size_t expected) noexcept
        {
            // All native workers have joined. Protect THIS genuine outer
            // admission lease, rather than claiming safety from a TID/count.
            auto stopped{admission::runtime_gc_entry_admission.try_exclusive(1uz)};
            PUBLICATION_HANDOFF_CHECK(stopped && admission::runtime_gc_entry_admission.protects_shared(stopped, reader));
            ::std::size_t reclaimed{};
            PUBLICATION_HANDOFF_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                stores.data(), stores.size(), roots, root_count, reclaimed) == status::ok && reclaimed == expected);
            return reclaimed;
        }
    };
    void rendezvous(context& fixture) noexcept
    {
        fixture.ready.fetch_add(1u, ::std::memory_order_release);
        while(!fixture.start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
    }
    void producer(context& fixture, unsigned worker) noexcept
    {
        auto shared{admission::runtime_gc_entry_admission.enter()};
        PUBLICATION_HANDOFF_CHECK(shared && worker < fixture.workers && fixture.workers <= fixture.lanes.size());
        auto& output{fixture.lanes[worker]};
        rendezvous(fixture);
        for(::std::size_t index{}; index != fixture.count; ++index)
        {
            auto const word{expected_word(worker, index)};
            ::std::array<value, 4uz> fields{value::i32(word), value::i32(word ^ 0xffff1234u),
                value::from(expected_vector(worker, index)), value::reference(reference{})};
            // [0,count) contains complete fixture-owned output slots. No
            // consumer reads this slot until its following release publication.
            PUBLICATION_HANDOFF_CHECK(fixture.stores[fixture.source_store(worker)]->struct_new(0u,
                fields.data(), fields.size(), output.references[index]) == status::ok);
            output.published.store(index + 1uz, ::std::memory_order_release);
        }
    }
    void check_payload(gc::gc_object_store& store, reference object, unsigned worker, ::std::size_t index) noexcept
    {
        value actual{};
        auto const word{expected_word(worker, index)};
        PUBLICATION_HANDOFF_CHECK(store.struct_get(object, 0uz, false, actual) == status::ok &&
            actual.as<::std::uint32_t>() == word);
        PUBLICATION_HANDOFF_CHECK(store.struct_get(object, 1uz, false, actual) == status::ok &&
            actual.as<::std::uint32_t>() == ((word ^ 0xffff1234u) & 0xffffu));
        PUBLICATION_HANDOFF_CHECK(store.struct_get(object, 2uz, false, actual) == status::ok &&
            actual.bits == expected_vector(worker, index));
    }
    void consumer(context& fixture) noexcept
    {
        auto shared{admission::runtime_gc_entry_admission.enter()};
        PUBLICATION_HANDOFF_CHECK(shared);
        rendezvous(fixture);
        ::std::array<::std::size_t, 4uz> consumed{};
        auto remaining{static_cast<::std::size_t>(fixture.workers) * fixture.count};
        while(remaining != 0uz)
        {
            bool progressed{};
            for(unsigned worker{}; worker != fixture.workers; ++worker)
            {
                auto const& output{fixture.lanes[worker]};
                auto const available{output.published.load(::std::memory_order_acquire)};
                PUBLICATION_HANDOFF_CHECK(available <= output.references.size());
                while(consumed[worker] != available)
                {
                    auto const index{consumed[worker]};
                    // [0,available) slots were initialized before the observed
                    // release. Their guest tokens are only passed to real
                    // membership/foreign-owner lookup, never dereferenced.
                    auto const object{output.references[index]};
                    // The consumer always resolves through the OTHER real
                    // canonical store, including while that store publishes
                    // another worker's objects in split-store mode.
                    check_payload(*fixture.stores[fixture.source_store(worker) ^ 1uz], object, worker, index);
                    fixture.consumer_checksum += expected_word(worker, index);
                    ++consumed[worker];
                    --remaining;
                    progressed = true;
                }
            }
            if(!progressed) { ::std::this_thread::yield(); }
        }
    }
    void qualification(context& fixture) noexcept
    {
        auto const total{static_cast<::std::size_t>(fixture.workers) * fixture.count};
        ::std::vector<::std::uintptr_t> identities{};
        identities.reserve(total);
        ::std::uint64_t expected_checksum{};
        ::std::array<::std::array<bool, 256uz>, 2uz> seen_stripes{};
        for(unsigned worker{}; worker != fixture.workers; ++worker)
        {
            auto& output{fixture.lanes[worker]};
            auto const source{fixture.source_store(worker)};
            PUBLICATION_HANDOFF_CHECK(output.published.load(::std::memory_order_acquire) == fixture.count);
            for(::std::size_t index{}; index != fixture.count; ++index)
            {
                auto const object{output.references[index]};
                check_payload(*fixture.stores[source], object, worker, index);
                // Opaque identity comparison only: this integer is never a
                // backing address or permission to read an object header.
                auto const identity{reinterpret_cast<::std::uintptr_t>(object.storage.ptr)};
                identities.push_back(identity);
                // Source-coupled diagnostic of the current bucket_index/256-
                // stripe mapping. This integer arithmetic does not decode I/O,
                // dereference a token or grant any membership/stop authority.
                auto bucket{identity};
                bucket ^= bucket >> 17u;
                bucket ^= bucket >> 9u;
                seen_stripes[source][static_cast<::std::size_t>(bucket) & 255uz] = true;
                expected_checksum += expected_word(worker, index);
                PUBLICATION_HANDOFF_CHECK(fixture.stores[source]->struct_set(object, 3uz, value::reference(object)) == status::ok);
            }
        }
        PUBLICATION_HANDOFF_CHECK(expected_checksum == fixture.consumer_checksum);
        ::std::ranges::sort(identities);
        PUBLICATION_HANDOFF_CHECK(!identities.empty() && identities.front() != 0u);
        for(::std::size_t index{1uz}; index != identities.size(); ++index)
        { PUBLICATION_HANDOFF_CHECK(identities[index - 1uz] != identities[index]); }
        for(::std::size_t stripe{}; stripe != seen_stripes[0uz].size(); ++stripe)
        { fixture.shared_stripes += seen_stripes[0uz][stripe] && seen_stripes[1uz][stripe]; }
        if(fixture.split_stores && fixture.workers > 1u && fixture.count >= 1024uz)
        { PUBLICATION_HANDOFF_CHECK(fixture.shared_stripes != 0uz); }
        // The receiver's real array is the only semantic root. Its typed
        // fields independently retain source arenas after module transfers.
        reference receiver_root{};
        PUBLICATION_HANDOFF_CHECK(fixture.stores[1uz]->array_new_default(1u, fixture.workers, receiver_root) == status::ok);
        for(unsigned worker{}; worker != fixture.workers; ++worker)
        {
            PUBLICATION_HANDOFF_CHECK(fixture.stores[1uz]->array_set(receiver_root, worker,
                value::reference(fixture.lanes[worker].references[0uz])) == status::ok);
        }
        auto const retired{fixture.lanes[0uz].references[fixture.count - 1uz]};
        PUBLICATION_HANDOFF_CHECK(fixture.collect(::std::addressof(receiver_root), 1uz, total - fixture.workers) == total - fixture.workers);
        value discarded{};
        PUBLICATION_HANDOFF_CHECK(fixture.stores[0uz]->struct_get(retired, 0uz, false, discarded) == status::invalid_reference);
        PUBLICATION_HANDOFF_CHECK(fixture.stores[1uz]->struct_get(retired, 0uz, false, discarded) == status::invalid_reference);
        for(unsigned worker{}; worker != fixture.workers; ++worker)
        {
            auto const object{fixture.lanes[worker].references[0uz]};
            auto const foreign{fixture.source_store(worker) ^ 1uz};
            check_payload(*fixture.stores[foreign], object, worker, 0uz);
            PUBLICATION_HANDOFF_CHECK(fixture.stores[foreign]->struct_get(object, 3uz, false, discarded) == status::ok &&
                discarded.as<reference>().storage.ptr == object.storage.ptr);
        }
        PUBLICATION_HANDOFF_CHECK(fixture.collect(nullptr, 0uz, fixture.workers + 1uz) == fixture.workers + 1uz);
        ::std::size_t discarded_length{};
        PUBLICATION_HANDOFF_CHECK(fixture.stores[1uz]->array_length(receiver_root, discarded_length) == status::invalid_reference);
        for(unsigned worker{}; worker != fixture.workers; ++worker)
        {
            PUBLICATION_HANDOFF_CHECK(fixture.stores[fixture.source_store(worker)]->struct_get(fixture.lanes[worker].references[0uz],
                0uz, false, discarded) == status::invalid_reference);
        }
        ::std::array<value, 4uz> fields{value::i32(7u), value::i32(9u), value::from(expected_vector(0u, 0uz)), value::reference(reference{})};
        reference fresh{};
        PUBLICATION_HANDOFF_CHECK(fixture.stores[0uz]->struct_new(0u, fields.data(), fields.size(), fresh) == status::ok);
        PUBLICATION_HANDOFF_CHECK(!::std::ranges::binary_search(identities, reinterpret_cast<::std::uintptr_t>(fresh.storage.ptr)));
        PUBLICATION_HANDOFF_CHECK(fixture.collect(nullptr, 0uz, 1uz) == 1uz);
    }
}

int main(int argc, char** argv)
{
    namespace test = ::publication_handoff_20261003;
    ::std::uint32_t workers{}, count{};
    if(argc != 4 || !test::number(::std::string_view{argv[1]}, workers) ||
       !test::number(::std::string_view{argv[2]}, count) ||
       (workers != 1u && workers != 2u && workers != 4u) || count < 2u || count > 262144u ||
       (::std::string_view{argv[3]} != "same" && ::std::string_view{argv[3]} != "split"))
    {
        ::fast_io::io::perrln("usage: gc_publication_handoff 1|2|4 count[2,262144] same|split");
        return 2;
    }
    test::context fixture{workers, count, ::std::string_view{argv[3]} == "split"};
    ::std::vector<::std::thread> threads{};
    threads.reserve(workers + 1uz);
    for(unsigned worker{}; worker != workers; ++worker)
    { threads.emplace_back(test::producer, ::std::ref(fixture), worker); }
    threads.emplace_back(test::consumer, ::std::ref(fixture));
    while(fixture.ready.load(::std::memory_order_acquire) != workers + 1u) { ::std::this_thread::yield(); }
    // Actual native actors own real reader leases and cannot be stopped by a
    // fabricated "single active" count. This control must reject collection.
    PUBLICATION_HANDOFF_CHECK(!test::admission::runtime_gc_entry_admission.try_exclusive(1uz));
    auto const begin{::std::chrono::steady_clock::now()};
    fixture.start.store(true, ::std::memory_order_release);
    for(auto& thread : threads) { thread.join(); }
    auto const elapsed{::std::chrono::duration_cast<::std::chrono::nanoseconds>(::std::chrono::steady_clock::now() - begin).count()};
    test::qualification(fixture);
    ::fast_io::io::println("{\"schema\":\"gc-publication-handoff-native-v1\",\"single_cas\":",
        test::single_cas_enabled ? "true" : "false", ",\"workers\":", ::fast_io::mnp::dec(workers),
        ",\"split_stores\":", fixture.split_stores ? "true" : "false", ",\"shared_stripes\":",
        ::fast_io::mnp::dec(fixture.shared_stripes),
        ",\"units_per_worker\":", ::fast_io::mnp::dec(count), ",\"actual_allocations\":",
        ::fast_io::mnp::dec(static_cast<::std::size_t>(workers) * count + 2uz),
        ",\"handoff_ns\":", ::fast_io::mnp::dec(elapsed), ",\"checksum\":",
        ::fast_io::mnp::dec(fixture.consumer_checksum), ",\"exact_reclamation\":true,\"vm_qualified\":false}");
}
