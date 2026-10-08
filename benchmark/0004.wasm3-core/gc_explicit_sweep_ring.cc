// Source/header-bound native component proof and performance diagnostic.
// Roots are explicit native arrays, all admitted stores are canonically pinned,
// and no other reader/mutator/module admission exists in this process. This is
// not VM root enumeration, automatic collection, or a WebAssembly benchmark.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <fast_io_device.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <string_view>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using tick = ::std::chrono::steady_clock;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;

#if defined(__linux__) && defined(UWVM2TEST_GC_SWEEP_FAULT_INJECT)
namespace
{
    ::std::atomic<bool> fail_next_array_new{};
    ::std::atomic<unsigned> failed_array_new{};
}
// Only sanitizer correctness builds wrap the array allocator. The flag is set
// immediately before one collection and the next array new is its work queue.
// O3 performance binaries contain no allocator wrapper or fault-check branch.
extern "C" void* sweep_real_array_new(::std::size_t, ::std::nothrow_t const&) noexcept
    asm("__real__ZnamRKSt9nothrow_t");
extern "C" void* sweep_wrap_array_new(::std::size_t, ::std::nothrow_t const&) noexcept
    asm("__wrap__ZnamRKSt9nothrow_t");
extern "C" void* sweep_wrap_array_new(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    if(fail_next_array_new.exchange(false, ::std::memory_order_relaxed))
    {
        failed_array_new.fetch_add(1u, ::std::memory_order_relaxed);
        return nullptr;
    }
    return sweep_real_array_new(bytes, tag);
}
#endif

namespace
{
    constexpr ::std::size_t ring_size{1024uz};
    constexpr ::std::size_t field_iterations{1000000uz};
#if defined(UWVM2TEST_GC_EXPLICIT_SWEEP)
    constexpr bool has_collector{true};
#else
    constexpr bool has_collector{false};
#endif
    unsigned checks{};

    void require(bool result, char const* label)
    {
        ++checks;
        if(!result)
        {
            ::fast_io::io::perrln("FAIL native explicit sweep: ", ::fast_io::mnp::os_c_str(label));
            ::fast_io::fast_terminate();
        }
    }

    ::std::uint64_t elapsed(tick::time_point begin, tick::time_point end) noexcept
    { return static_cast<::std::uint64_t>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count()); }

    type::recursive_type_section declarations()
    {
        type::recursive_type_section section{};
        section.type_count = 3u;
        type::recursive_group group{};
        type::field_type numeric{};
        numeric.storage.value.kind = type::value_kind::i32;
        numeric.mutable_ = true;
        type::sub_type scalar{};
        scalar.kind = type::composite_kind::struct_;
        scalar.fields.push_back(numeric);
        group.types.push_back(::std::move(scalar)); // 0: one mutable i32, like the Wasm allocation ring
        type::field_type child{};
        child.storage.value.kind = type::value_kind::reference;
        child.storage.value.heap = {static_cast<::std::int_least64_t>(type::abstract_heap_type::eq)};
        child.storage.value.nullable = true;
        child.mutable_ = true;
        type::sub_type node{};
        node.kind = type::composite_kind::struct_;
        node.fields.push_back(child);
        group.types.push_back(::std::move(node)); // 1: aggregate graph edge
        type::sub_type array{};
        array.kind = type::composite_kind::array;
        array.fields.push_back(child);
        group.types.push_back(::std::move(array)); // 2: zero-length array proof
        section.groups.push_back(::std::move(group));
        return section;
    }

    reference scalar(gc::gc_object_store& store, ::std::uint32_t bits)
    {
        value const fields[]{value::i32(bits)};
        reference result{};
        require(store.struct_new(0u, fields, 1uz, result) == status::ok, "one-field allocation succeeds");
        require(result.kind == global::wasm_ref_kind::wasm_struct && result.storage.ptr != nullptr, "published scalar has opaque non-null identity");
        return result;
    }

    void scalar_is(gc::gc_object_store& store, reference object, ::std::uint32_t expected)
    {
        value observed{};
        require(store.struct_get(object, 0uz, false, observed) == status::ok, "retained scalar remains resolvable");
        require(observed.as<::std::uint32_t>() == expected, "retained scalar payload unchanged");
    }

    void points_to(gc::gc_object_store& store, reference object, reference expected)
    {
        value observed{};
        require(store.struct_get(object, 0uz, false, observed) == status::ok, "retained graph node remains resolvable");
        auto const child{observed.as<reference>()};
        require(child.kind == expected.kind && child.storage.ptr == expected.storage.ptr, "typed graph edge unchanged");
    }

    template<::std::size_t Size>
    ::std::size_t collect(::std::array<::std::shared_ptr<gc::gc_object_store>, Size> const& cohort,
        reference const* roots, ::std::size_t root_count, status expected, ::std::size_t expected_reclaimed)
    {
#if defined(UWVM2TEST_GC_EXPLICIT_SWEEP)
        ::std::size_t reclaimed{SIZE_MAX};
        require(gc::gc_object_store::collect_exclusive_aggregate_domain(cohort.data(), cohort.size(), roots,
            root_count, reclaimed) == expected, "explicit collector returns expected status");
        require(reclaimed == expected_reclaimed, "exact reclaimed count follows explicit reachability");
        return reclaimed;
#else
        static_cast<void>(cohort); static_cast<void>(roots); static_cast<void>(root_count);
        static_cast<void>(expected); static_cast<void>(expected_reclaimed);
        require(false, "monotonic baseline must not pretend to collect");
        return 0uz;
#endif
    }

    void proof()
    {
        auto const types{declarations()};
#if !defined(UWVM2TEST_GC_EXPLICIT_SWEEP)
        auto store{::std::make_shared<gc::gc_object_store>(types)};
        require(store->valid(), "baseline layouts valid");
        auto const kept{scalar(*store, 0x1234'5678u)};
        for(::std::size_t index{}; index != 4096uz; ++index) { static_cast<void>(scalar(*store, static_cast<::std::uint32_t>(index))); }
        scalar_is(*store, kept, 0x1234'5678u);
        ::fast_io::io::println("GC_SWEEP_PROOF {\"variant\":\"monotonic\",\"checks\":", checks,
            ",\"collections\":0,\"reclaimed\":0,\"component_semantics\":true,\"collector_qualified\":false,\"vm_qualified\":false}");
#else
        type::recursive_type_section empty{};
        auto leases_a{::std::make_shared<gc::gc_lease_owner>()};
        auto leases_b{::std::make_shared<gc::gc_lease_owner>()};
        auto leases_c{::std::make_shared<gc::gc_lease_owner>()};
        ::std::array cohort{::std::make_shared<gc::gc_object_store>(types, leases_a),
            ::std::make_shared<gc::gc_object_store>(types, leases_b),
            ::std::make_shared<gc::gc_object_store>(empty, leases_c)};
        for(auto const& store : cohort) { require(store->valid(), "canonical proof cohort valid"); }
        ::std::weak_ptr<gc::gc_object_store> weak_b{cohort[1uz]};
        auto const live_b{scalar(*cohort[1uz], 0xbeef'1234u)};
        reference live_a{}, dead_a{}, dead_b{}, empty_array{};
        require(cohort[0uz]->struct_new_default(1u, live_a) == status::ok, "live parent allocation");
        require(cohort[0uz]->struct_new_default(1u, dead_a) == status::ok, "unreachable cycle A allocation");
        require(cohort[1uz]->struct_new_default(1u, dead_b) == status::ok, "unreachable cycle B allocation");
        require(cohort[0uz]->struct_set(live_a, 0uz, value::reference(live_b)) == status::ok, "live cross-store edge installed");
        require(cohort[0uz]->struct_set(dead_a, 0uz, value::reference(dead_b)) == status::ok, "unreachable cross-store cycle A edge");
        require(cohort[1uz]->struct_set(dead_b, 0uz, value::reference(dead_a)) == status::ok, "unreachable cross-store cycle B edge");
        auto const garbage{scalar(*cohort[0uz], 7u)};
        require(cohort[0uz]->array_new_default(2u, 0uz, empty_array) == status::ok, "zero-length array allocated");
        // Repeated precise roots queue a header only once. live_b is a genuine
        // foreign-store root as well as the live parent's transitive child.
        ::std::array<reference, 5uz> roots{live_a, live_b, live_a, {}, global::make_wasm_i31_reference(-17)};
        {
            ::std::array partial{cohort[0uz], cohort[1uz]};
            collect(partial, roots.data(), roots.size(), status::invalid_reference, 0uz);
        }
        reference forged{};
        forged.kind = global::wasm_ref_kind::wasm_struct;
        forged.storage.ptr = reinterpret_cast<void*>(::std::uintptr_t{1u});
        collect(cohort, &forged, 1uz, status::invalid_reference, 0uz);
        ::std::array late_invalid_roots{live_a, live_b, live_a, forged};
        // A legitimate root may already have been marked when a later token
        // fails. Neither reachable nor unreachable objects may be reclaimed.
        collect(cohort, late_invalid_roots.data(), late_invalid_roots.size(), status::invalid_reference, 0uz);
        points_to(*cohort[0uz], live_a, live_b);
        scalar_is(*cohort[1uz], live_b, 0xbeef'1234u);
        points_to(*cohort[0uz], dead_a, dead_b);
        points_to(*cohort[1uz], dead_b, dead_a);
        scalar_is(*cohort[0uz], garbage, 7u);
        bool oom_injected{};
# if defined(__linux__) && defined(UWVM2TEST_GC_SWEEP_FAULT_INJECT)
        // This fixture contains only legacy headers, even with compact support:
        // the next array allocation is the existing collector work queue.
        // Invalid roots retain precedence over an injected work-array OOM.
        fail_next_array_new.store(true, ::std::memory_order_relaxed);
        collect(cohort, late_invalid_roots.data(), late_invalid_roots.size(), status::invalid_reference, 0uz);
        // A collector may reject the root before attempting the allocation.
        // Clear any unconsumed injection; status/retained objects define the
        // contract, not the implementation's chosen allocation ordering.
        fail_next_array_new.store(false, ::std::memory_order_relaxed);
        auto const earlier_failed_allocations{failed_array_new.load(::std::memory_order_relaxed)};
        fail_next_array_new.store(true, ::std::memory_order_relaxed);
        collect(cohort, roots.data(), roots.size(), status::out_of_memory, 0uz);
        require(!fail_next_array_new.load(::std::memory_order_relaxed) && failed_array_new.load(::std::memory_order_relaxed) == earlier_failed_allocations + 1u,
            "valid roots report the separately failed work allocation");
        points_to(*cohort[0uz], dead_a, dead_b);
        points_to(*cohort[1uz], dead_b, dead_a);
        scalar_is(*cohort[0uz], garbage, 7u);
        scalar_is(*cohort[1uz], live_b, 0xbeef'1234u);
        oom_injected = true;
# endif
        ::std::size_t reclaimed{};
        reclaimed += collect(cohort, roots.data(), roots.size(), status::ok, 4uz);
        points_to(*cohort[0uz], live_a, live_b);
        scalar_is(*cohort[1uz], live_b, 0xbeef'1234u);
        value observed{};
        for(auto const retired : {dead_a, dead_b, garbage})
        {
            require(cohort[0uz]->struct_get(retired, 0uz, false, observed) == status::invalid_reference,
                "swept token misses local and foreign indices without dereference");
        }
        ::std::size_t retired_length{};
        require(cohort[0uz]->array_length(empty_array, retired_length) == status::invalid_reference, "zero-length array token retired");
        auto const later{scalar(*cohort[0uz], 123u)};
        require(later.storage.ptr != garbage.storage.ptr && later.storage.ptr != dead_a.storage.ptr && later.storage.ptr != dead_b.storage.ptr,
            "new identity does not reuse swept opaque tokens");
        ::std::array late_stale_roots{live_a, dead_b};
        collect(cohort, late_stale_roots.data(), late_stale_roots.size(), status::invalid_reference, 0uz);
        points_to(*cohort[0uz], live_a, live_b);
        scalar_is(*cohort[0uz], later, 123u);
        require(cohort[0uz]->struct_set(live_a, 0uz, value::reference({})) == status::ok, "live parent edge overwritten with null");
        reclaimed += collect(cohort, &live_a, 1uz, status::ok, 2uz);
        points_to(*cohort[0uz], live_a, {});
        require(cohort[0uz]->struct_get(live_b, 0uz, false, observed) == status::invalid_reference, "unreachable former child retired");
        // Module lease roots are explicitly retired here. The collector must
        // also have pruned the surviving parent's obsolete object lease.
        leases_a.reset(); leases_b.reset(); cohort[1uz].reset();
        require(weak_b.expired(), "obsolete aggregate field lease does not keep store B alive");
        ::std::array remaining{cohort[0uz], cohort[2uz]};
        reclaimed += collect(remaining, nullptr, 0uz, status::ok, 1uz);
        require(cohort[0uz]->struct_get(live_a, 0uz, false, observed) == status::invalid_reference, "last explicit root release retires parent");
        auto const final{scalar(*cohort[0uz], 777u)};
        require(final.storage.ptr != live_a.storage.ptr && final.storage.ptr != later.storage.ptr, "identity remains nonrecycling across collections");
        reclaimed += collect(remaining, nullptr, 0uz, status::ok, 1uz);
        require(reclaimed == 8uz, "all eight proof objects reclaimed exactly once");
        ::fast_io::io::println("GC_SWEEP_PROOF {\"variant\":\"explicit-component\",\"checks\":", checks,
            ",\"collections\":4,\"reclaimed\":", reclaimed,
            ",\"work_oom_injected\":", ::fast_io::mnp::os_c_str(oom_injected ? "true" : "false"),
            ",\"missing_empty_store_rejected\":true,\"stale_rejected\":true,\"foreign_cycle_reclaimed\":true,",
            "\"overwrite_lease_released\":true,\"component_semantics\":true,\"vm_qualified\":false}");
#endif
    }

    bool parse_count(::std::string_view text, ::std::size_t& result) noexcept
    {
        // [text.data(), text.data()+text.size()) is one complete OS argument.
        // [safe                                ] decimal parsing stays within that range.
        auto const end{text.data() + text.size()};
        auto const parsed{::fast_io::parse_by_scan(text.data(), end, ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }

    ::std::uint64_t rss_bytes(::std::size_t page_bytes)
    {
#if defined(__linux__)
        ::fast_io::ibuf_file input{"/proc/self/statm", ::fast_io::open_mode::in};
        ::std::size_t total_pages{}, resident_pages{};
        require(::fast_io::io::scan<true>(input, ::fast_io::mnp::dec_get(total_pages), ::fast_io::mnp::dec_get(resident_pages)),
            "FastIO reads actual process resident pages");
        require(resident_pages <= UINT64_MAX / page_bytes, "resident page multiplication bounded");
        return static_cast<::std::uint64_t>(resident_pages) * page_bytes;
#else
        static_cast<void>(page_bytes);
        return 0u; // Only Linux runs supply RSS/performance qualification.
#endif
    }

    ::std::uint64_t verify_roots(gc::gc_object_store& store,
        ::std::array<reference, ring_size> const& roots, ::std::array<::std::uint32_t, ring_size> const& wanted,
        ::std::size_t initialized)
    {
        ::std::uint64_t checksum{};
        for(::std::size_t index{}; index != initialized; ++index)
        {
            // [0,initialized) owns exactly the initialized explicit root slots.
            value observed{};
            require(store.struct_get(roots[index], 0uz, false, observed) == status::ok, "every explicit root survives");
            auto const bits{observed.as<::std::uint32_t>()};
            require(bits == wanted[index], "every explicit root retains its independently tracked payload");
            checksum += bits;
        }
        return checksum;
    }

    void ring(::std::size_t allocations, ::std::size_t collect_every, ::std::size_t page_bytes)
    {
        auto const types{declarations()};
        ::std::array cohort{::std::make_shared<gc::gc_object_store>(types)};
        require(cohort[0uz]->valid(), "ring layout initialized");
        ::std::array<reference, ring_size> roots{};
        ::std::array<::std::uint32_t, ring_size> wanted{};
        ::std::uint32_t state{123456789u};
        ::std::size_t allocated{}, reclaimed_total{}, collections{};
        ::std::uint64_t allocation_ns{}, collection_ns{}, root_validation_ns{}, maximum_pause_ns{};
        auto constexpr checkpoint_every{1000000uz};
        auto const initial_rss{rss_bytes(page_bytes)};
        auto const start{tick::now()};
        while(allocated != allocations)
        {
            auto const remaining{allocations - allocated};
            auto const until_checkpoint{checkpoint_every - allocated % checkpoint_every};
            auto const count{(::std::min)({remaining, collect_every, until_checkpoint})};
            reference retired_sample{};
            ::std::uint32_t retired_payload{};
            auto const allocate_begin{tick::now()};
            for(::std::size_t index{}; index != count; ++index)
            {
                state = state * 1664525u + 1013904223u;
                auto const slot{(allocated + index) & (ring_size - 1uz)};
                // [0,1024) slot selects a complete explicit root/value slot.
                if(retired_sample.kind == global::wasm_ref_kind::wasm_null && roots[slot].kind != global::wasm_ref_kind::wasm_null)
                { retired_sample = roots[slot]; retired_payload = wanted[slot]; }
                roots[slot] = scalar(*cohort[0uz], state);
                wanted[slot] = state;
            }
            allocation_ns += elapsed(allocate_begin, tick::now());
            allocated += count;
            auto const initialized{(::std::min)(allocated, ring_size)};
#if defined(UWVM2TEST_GC_EXPLICIT_SWEEP)
            ::std::size_t reclaimed{};
            auto const pause_begin{tick::now()};
            auto const result{gc::gc_object_store::collect_exclusive_aggregate_domain(
                cohort.data(), cohort.size(), roots.data(), initialized, reclaimed)};
            auto const pause_ns{elapsed(pause_begin, tick::now())};
            require(result == status::ok, "ring explicit collection succeeds");
            require(reclaimed == allocated - reclaimed_total - initialized, "ring exact reclaim count independent of collector internals");
            reclaimed_total += reclaimed; ++collections; collection_ns += pause_ns;
            maximum_pause_ns = (::std::max)(maximum_pause_ns, pause_ns);
            ::fast_io::io::println("GC_SWEEP_COLLECTION {\"allocated\":", allocated, ",\"collections\":", collections,
                ",\"reclaimed\":", reclaimed, ",\"reclaimed_total\":", reclaimed_total,
                ",\"live_expected\":", initialized, ",\"pause_ns\":", pause_ns, "}");
#endif
            auto const verify_begin{tick::now()};
            auto const checksum{verify_roots(*cohort[0uz], roots, wanted, initialized)};
            if(retired_sample.kind != global::wasm_ref_kind::wasm_null)
            {
                value observed{};
                auto const found{cohort[0uz]->struct_get(retired_sample, 0uz, false, observed)};
                if constexpr(has_collector) { require(found == status::invalid_reference, "ring overwritten token retired by actual collection"); }
                else { require(found == status::ok && observed.as<::std::uint32_t>() == retired_payload, "monotonic baseline retains overwritten object"); }
            }
            root_validation_ns += elapsed(verify_begin, tick::now());
            if(allocated % checkpoint_every == 0uz || allocated == allocations)
            {
                ::fast_io::io::println("GC_SWEEP_CHECKPOINT {\"allocated\":", allocated, ",\"collections\":", collections,
                    ",\"reclaimed_total\":", reclaimed_total, ",\"live_expected\":", has_collector ? initialized : allocated,
                    ",\"root_count\":", initialized, ",\"checksum\":", checksum, ",\"rss_bytes\":", rss_bytes(page_bytes),
                    ",\"allocation_ns\":", allocation_ns, ",\"collection_ns\":", collection_ns,
                    ",\"root_validation_ns\":", root_validation_ns, "}");
            }
        }
        auto const ring_wall_ns{elapsed(start, tick::now())};
        ::std::uint64_t field_checksum{};
        auto const field_begin{tick::now()};
        for(::std::size_t index{}; index != field_iterations; ++index)
        {
            state = state * 1664525u + 1013904223u;
            auto const slot{(state >> 20u) & (ring_size - 1uz)};
            // [0,1024) state-derived slot loads a real varying opaque reference.
            require(cohort[0uz]->struct_set(roots[slot], 0uz, value::i32(state)) == status::ok, "dynamic retained field store");
            value observed{};
            require(cohort[0uz]->struct_get(roots[slot], 0uz, false, observed) == status::ok && observed.as<::std::uint32_t>() == state,
                "dynamic retained field read observes the last write");
            field_checksum += observed.as<::std::uint32_t>();
            wanted[slot] = state;
        }
        auto const local_fields_ns{elapsed(field_begin, tick::now())};
        auto const root_checksum{verify_roots(*cohort[0uz], roots, wanted, ring_size)};
        auto const retained_rss{rss_bytes(page_bytes)};
        ::std::size_t final_reclaimed{};
#if defined(UWVM2TEST_GC_EXPLICIT_SWEEP)
        // The only remaining roots are explicitly dropped. No VM safepoint or
        // hidden-root discovery is inferred from this component call.
        roots = {};
        auto const drop_begin{tick::now()};
        auto const drop_result{gc::gc_object_store::collect_exclusive_aggregate_domain(
            cohort.data(), cohort.size(), nullptr, 0uz, final_reclaimed)};
        auto const drop_pause_ns{elapsed(drop_begin, tick::now())};
        require(drop_result == status::ok && final_reclaimed == ring_size,
            "last explicit root release reclaims the retained ring");
        reclaimed_total += final_reclaimed; ++collections; collection_ns += drop_pause_ns;
        maximum_pause_ns = (::std::max)(maximum_pause_ns, drop_pause_ns);
        require(reclaimed_total == allocations, "every issued ring object reclaimed exactly once");
        ::fast_io::io::println("GC_SWEEP_COLLECTION {\"allocated\":", allocated, ",\"collections\":", collections,
            ",\"reclaimed\":", final_reclaimed, ",\"reclaimed_total\":", reclaimed_total,
            ",\"live_expected\":0,\"final_drop\":true,\"pause_ns\":", drop_pause_ns, "}");
#endif
        auto const teardown_begin{tick::now()};
        cohort[0uz].reset();
        auto const teardown_ns{elapsed(teardown_begin, tick::now())};
        ::fast_io::io::println("GC_SWEEP_SUMMARY {\"variant\":\"", ::fast_io::mnp::os_c_str(has_collector ? "explicit-component" : "monotonic"),
            "\",\"allocations\":", allocations, ",\"ring_roots\":", ring_size, ",\"collect_every\":", collect_every,
            ",\"collections\":", collections, ",\"reclaimed\":", reclaimed_total, ",\"final_reclaimed\":", final_reclaimed,
            ",\"allocation_ns\":", allocation_ns, ",\"collection_ns\":", collection_ns,
            ",\"root_validation_ns\":", root_validation_ns, ",\"ring_wall_ns\":", ring_wall_ns,
            ",\"maximum_pause_ns\":", maximum_pause_ns, ",\"field_iterations\":", field_iterations,
            ",\"local_fields_ns\":", local_fields_ns, ",\"field_checksum\":", field_checksum,
            ",\"root_checksum\":", root_checksum, ",\"initial_rss_bytes\":", initial_rss,
            ",\"retained_rss_bytes\":", retained_rss, ",\"teardown_ns\":", teardown_ns,
            ",\"checks\":", checks, ",\"explicit_native_roots\":true,\"vm_qualified\":false,\"automatic_gc\":false}");
    }
}

int main(int argc, char** argv)
{
    if(argc == 2 && ::std::string_view{argv[1]} == "proof") { proof(); return 0; }
    if(argc != 5 || ::std::string_view{argv[1]} != "ring") { return 64; }
    ::std::size_t allocations{}, interval{}, page_bytes{};
    if(!parse_count(argv[2], allocations) || !parse_count(argv[3], interval) || !parse_count(argv[4], page_bytes) ||
        allocations < ring_size || allocations > 10000000uz || interval < ring_size || interval > 1000000uz ||
        page_bytes == 0uz || page_bytes > 1048576uz || (page_bytes & (page_bytes - 1uz)) != 0uz) { return 64; }
    ring(allocations, interval, page_bytes);
}
