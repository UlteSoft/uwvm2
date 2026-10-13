// Native compact collector component only. One canonical store and complete
// explicit roots; no VM, mutator peer, host callback or foreign-module actor.
// Compile the SAME source with each pinned product's actual macro values.
#if !defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) || UWVM_EXPERIMENTAL_COMPACT_NUMERIC != 1
# error This component requires the explicitly enabled compact numeric path.
#endif
#if !defined(__cpp_exceptions)
# error Compact allocation must be active, not the no-exceptions legacy fallback.
#endif
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using tick = ::std::chrono::steady_clock;
using reference = gc::gc_reference;
using status = gc::gc_object_status;

namespace
{
    constexpr ::std::size_t root_count{1024uz};
    constexpr ::std::size_t max_duplicate_roots{1024uz};
    constexpr ::std::uint32_t seed{123456789u};
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
    constexpr unsigned directory_enabled{1u};
#else
    constexpr unsigned directory_enabled{0u};
#endif
    void require(bool passed, char const* label) noexcept
    {
        if(!passed)
        {
            ::fast_io::io::perrln("[FAIL] gc_compact_collection_ring ", ::fast_io::mnp::os_c_str(label));
            ::fast_io::fast_terminate();
        }
    }
    ::std::uint64_t elapsed(tick::time_point begin, tick::time_point end) noexcept
    { return static_cast<::std::uint64_t>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count()); }

    ::std::shared_ptr<gc::gc_object_store> make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        type::recursive_type_section section{};
        section.type_count = 1u;
        type::recursive_group group{};
        group.first_type_index = 0u;
        type::sub_type structure{};
        structure.kind = type::composite_kind::struct_;
        type::field_type field{};
        field.storage.value.kind = type::value_kind::i32;
        field.storage.packed = type::packed_kind::none;
        field.mutable_ = false; // Real compact eligibility; the legacy ring is mutable.
        structure.fields.push_back(field);
        group.types.push_back(::std::move(structure));
        section.groups.push_back(::std::move(group));
        auto store{::std::make_shared<gc::gc_object_store>(section, leases)};
        require(store->valid(), "canonical immutable i32 layout");
        return store;
    }

    struct read_result
    {
        bool passed{true};
        ::std::uint64_t checksum{};
        ::std::size_t authenticated{};
    };
    read_result read_roots(::std::shared_ptr<gc::gc_object_store const> const& owner,
        ::std::array<reference, root_count> const& roots,
        ::std::array<::std::uint32_t, root_count> const& wanted) noexcept
    {
        read_result result{};
        for(::std::size_t index{}; index != root_count; ++index)
        {
            // [0,1024) selects complete issued carriers and tracked payloads.
            // Authenticating every root proves actual compact allocation;
            // equal struct.get results alone could also come from legacy.
            gc::compact_numeric_reader reader{};
            ::std::uint32_t observed{};
            auto const acquired{gc::compact_numeric_reader::try_local(owner, roots[index], reader)};
            bool const valid{acquired == gc::compact_numeric_status::ok && reader.valid() &&
                reader.type_index() == 0u && reader.value_kind() == gc::compact_numeric_kind::i32 &&
                reader.raw_bits32(observed) == gc::compact_numeric_status::ok && observed == wanted[index]};
            result.passed = result.passed && valid;
            result.authenticated += static_cast<::std::size_t>(valid);
            result.checksum += observed;
            // No native read borrow may cross a poll/exclusive collection.
            // ^^ reset drops the real reader/admission and both native owners.
            reader.reset();
        }
        return result;
    }

    void run(::std::size_t allocations, ::std::size_t interval, ::std::size_t duplicates)
    {
        auto const process_begin{tick::now()};
        auto leases{::std::make_shared<gc::gc_lease_owner>()};
        ::std::array cohort{make_store(leases)};
        ::std::shared_ptr<gc::gc_object_store const> owner{cohort[0uz]};
        ::std::array<reference, root_count> roots{};
        ::std::array<::std::uint32_t, root_count> wanted{};
        ::std::array<reference, root_count + max_duplicate_roots> snapshot{};
        ::std::uint32_t state{seed};
        ::std::size_t allocated{}, collections{}, reclaimed_total{}, actual_compact_reads{};
        ::std::uint64_t allocation_ns{}, collection_ns{}, readback_ns{}, readback_checksum{}, last_root_checksum{};
        while(allocated != allocations)
        {
            // This saved token is deliberately a negative identity after the
            // next overwrite; it is not a live native root or owning reader.
            auto const retired_key{roots[0uz]};
            status allocation_status{status::ok};
            auto const allocation_begin{tick::now()};
            for(::std::size_t index{}; index != interval; ++index)
            {
                state = state * 1664525u + 1013904223u;
                auto const slot{(allocated + index) & (root_count - 1uz)};
                auto const field{gc::gc_object_value::i32(state)};
                reference fresh{};
                // [field,field+1) one complete initialized numeric input;
                // addressof only borrows that native cell through struct_new.
                allocation_status = cohort[0uz]->struct_new(0u, ::std::addressof(field), 1uz, fresh);
                if(allocation_status != status::ok) { break; }
                // [0,1024) replaces one complete owning explicit-root slot.
                // The opaque carrier is never cast into a native pointer.
                roots[slot] = fresh;
                wanted[slot] = state;
            }
            allocation_ns += elapsed(allocation_begin, tick::now());
            require(allocation_status == status::ok, "compact allocation status");
            allocated += interval; // Bounded by exact allocations/interval divisibility.

            // Root snapshot construction is outside ALL four timing regions.
            for(::std::size_t index{}; index != root_count; ++index) { snapshot[index] = roots[index]; }
            for(::std::size_t index{}; index != duplicates; ++index)
            {
                // [1024,1024+duplicates<=2048) complete duplicate carriers;
                // multiplication is bounded by 1023*31 and selection by 1023.
                snapshot[root_count + index] = roots[(index * 31uz) & (root_count - 1uz)];
            }

            // First-chunk proof occurs BEFORE the first collector, preventing
            // a legacy-only component from acquiring a compact label.
            if(collections == 0uz)
            {
                auto const proof{read_roots(owner, roots, wanted)};
                require(proof.passed && proof.authenticated == root_count, "actual compact admission before collection");
            }
            ::std::size_t reclaimed{};
            status collected{status::invalid_store};
            {
                // Every actual store is pinned in cohort; all native readers
                // have retired. The REAL admission lease precedes timing.
                auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
                require(static_cast<bool>(exclusive), "actual exclusive collection admission");
                auto const collect_begin{tick::now()};
                // [cohort.data(),cohort.data()+1) one canonical owning store;
                // [snapshot.data(),snapshot.data()+1024+duplicates) initialized
                // actual roots; the collector never retains either array.
                collected = gc::gc_object_store::collect_exclusive_aggregate_domain(
                    cohort.data(), cohort.size(), snapshot.data(), root_count + duplicates, reclaimed);
                collection_ns += elapsed(collect_begin, tick::now());
            } // ^^ real exclusive lease retires before any compact readback.
            require(collected == status::ok, "collector status");
            require(reclaimed == allocated - reclaimed_total - root_count, "independent exact reclaim count");
            reclaimed_total += reclaimed;
            ++collections;

            auto const read_begin{tick::now()};
            auto const observed{read_roots(owner, roots, wanted)};
            readback_ns += elapsed(read_begin, tick::now());
            require(observed.passed && observed.authenticated == root_count, "all actual compact roots survive with expected payloads");
            actual_compact_reads += observed.authenticated;
            readback_checksum += observed.checksum; // <=512M*UINT32_MAX, below UINT64_MAX.
            last_root_checksum = observed.checksum;
            if(retired_key.kind != global::wasm_ref_kind::wasm_null)
            {
                gc::compact_numeric_reader stale{};
                auto const rejected{gc::compact_numeric_reader::try_local(owner, retired_key, stale)};
                require(rejected == gc::compact_numeric_status::invalid_reference && !stale.valid(), "overwritten compact identity stays retired");
                stale.reset(); // ^^ no stale reader/admission reaches the next pause.
            }
        }

        // Dropping the complete explicit root set is outside collection timing.
        for(auto& root : roots) { root = {}; } // [0,1024) each complete carrier becomes null.
        for(auto& root : snapshot) { root = {}; } // [0,2048) no duplicate remains a live root.
        ::std::size_t final_reclaimed{};
        status final_status{status::invalid_store};
        {
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            require(static_cast<bool>(exclusive), "final actual exclusive admission");
            auto const collect_begin{tick::now()};
            // [cohort.data(),cohort.data()+1) stays alive; null/zero root span
            // contains no readable element and is accepted only as empty.
            final_status = gc::gc_object_store::collect_exclusive_aggregate_domain(
                cohort.data(), cohort.size(), nullptr, 0uz, final_reclaimed);
            collection_ns += elapsed(collect_begin, tick::now());
        } // ^^ release the real exclusive lease before store teardown.
        require(final_status == status::ok && final_reclaimed == root_count, "final drop reclaims every surviving compact object");
        reclaimed_total += final_reclaimed;
        ++collections;
        require(reclaimed_total == allocations && collections == allocations / interval + 1uz,
            "total independent allocation/collection accounting");
        auto const teardown_begin{tick::now()};
        // ^^ reset releases all canonical pins AFTER readers and exclusive
        // leases retired; this last store owner may run its real destructor.
        owner.reset();
        cohort[0uz].reset();
        leases.reset();
        auto const teardown_ns{elapsed(teardown_begin, tick::now())};
        auto const process_ns{elapsed(process_begin, tick::now())};
        // Success emits one final record only; no logging/formatting occurs in
        // allocation, collector, authenticated readback or teardown regions.
        ::fast_io::io::println("GC_COMPACT_RING {\"allocations\":", allocations,
            ",\"root_count\":", root_count, ",\"duplicate_roots\":", duplicates,
            ",\"collect_every\":", interval, ",\"collections\":", collections,
            ",\"compact_collection_directory_enabled\":", directory_enabled,
            ",\"reclaimed\":", reclaimed_total, ",\"final_reclaimed\":", final_reclaimed,
            ",\"remaining\":0,\"actual_compact_reads\":", actual_compact_reads,
            ",\"seed\":", seed, ",\"last_lcg\":", state,
            ",\"last_root_checksum\":", last_root_checksum, ",\"readback_checksum\":", readback_checksum,
            ",\"allocation_ns\":", allocation_ns, ",\"collection_ns\":", collection_ns,
            ",\"readback_ns\":", readback_ns, ",\"teardown_ns\":", teardown_ns,
            ",\"component_process_ns\":", process_ns,
            ",\"collector_excludes_admission\":true,\"actual_compact_admission_proved\":true,"
            "\"native_component_only\":true,\"automatic_gc\":false,\"vm_qualified\":false}");
    }

    bool parse_count(char const* text, ::std::size_t& result) noexcept
    {
        // Native argc/argv owns a complete NUL-terminated input string.
        ::std::string_view const bytes{text};
        // [bytes.data(),bytes.data()+bytes.size()) readable argument bytes;
        // ^^ end is exactly one-past, used only as the scanner's bound.
        auto const* const end{bytes.data() + bytes.size()};
        auto const parsed{::fast_io::parse_by_scan(bytes.data(), end, ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }
}

int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    ::std::size_t allocations{}, interval{}, duplicates{};
    // [argv,argv+argc) native arguments; indexes 1,2,3 are bounded by argc=4.
    if(!parse_count(argv[1], allocations) || !parse_count(argv[2], interval) || !parse_count(argv[3], duplicates) ||
       allocations < 16384uz || allocations > 512000000uz || interval < root_count || interval > 1000000uz ||
       allocations % interval != 0uz || duplicates > max_duplicate_roots) { return 2; }
    run(allocations, interval, duplicates);
}
