// Keeper-only SDK-free storage-leaf chain for the ORIGINAL immutable-i32
// gc-allocation-ring. It owns real module/table storage and uses real codecs,
// retention, cast/get predicates and static root visitation. It does not call
// the LLVM IR emitter/table bridges, fabricate their RT symbols, or qualify
// generated JIT code/debug gates/automatic guest frame maps.
#include <uwvm2/uwvm/runtime/storage/gc_static_roots.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace gc_immutable_table_chain_20261003
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace admission = ::uwvm2::runtime::gc;
    using reference = gc::gc_reference;
    using value = gc::gc_object_value;
    using status = gc::gc_object_status;
    constexpr ::std::size_t slots{1024uz}, batch{4096uz};

    void require(bool condition, unsigned line) noexcept
    {
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_immutable_table_chain line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define GC_IMMUTABLE_CHAIN_CHECK(...) ::gc_immutable_table_chain_20261003::require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    [[nodiscard]] bool number(::std::string_view input, ::std::uint32_t& result) noexcept
    {
        if(input.empty() || !::fast_io::char_category::is_c_digit(input.front())) { return false; }
        // [input.data(),input.data()+input.size()) is the complete native argv.
        // [safe                                    ] input bounds its one-past.
        // ^^ construct only this string's end before invoking fast_io scanning.
        auto const* end{input.data() + input.size()};
        auto const parsed{::fast_io::parse_by_scan(input.data(), end, ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }
    [[nodiscard]] type::recursive_type_section declarations()
    {
        type::recursive_type_section result{};
        result.type_count = 1u;
        type::recursive_group group{};
        type::sub_type node{};
        node.kind = type::composite_kind::struct_;
        type::field_type field{};
        field.storage.value.kind = type::value_kind::i32;
        // Exactly `(type $s (struct (field i32)))`: the field is immutable.
        GC_IMMUTABLE_CHAIN_CHECK(!field.mutable_);
        node.fields.push_back(field);
        group.types.push_back(::std::move(node));
        result.groups.push_back(::std::move(group));
        return result;
    }
    [[nodiscard]] constexpr ::std::uint32_t expected_scalar(::std::size_t count) noexcept
    {
        // Affine exponentiation is an untimed scalar oracle, not an IO parser
        // or a replacement table. All arithmetic is explicitly modulo 2^32.
        ::std::uint32_t multiplier{1u}, increment{}, power_multiplier{1664525u}, power_increment{1013904223u};
        while(count != 0uz)
        {
            if((count & 1uz) != 0uz)
            {
                multiplier *= power_multiplier;
                increment = increment * power_multiplier + power_increment;
            }
            power_increment = (power_multiplier + 1u) * power_increment;
            power_multiplier *= power_multiplier;
            count >>= 1u;
        }
        return multiplier * 123456789u + increment;
    }
    struct context
    {
        admission::managed_entry_admission::shared_lease reader{admission::runtime_gc_entry_admission.enter()};
        type::recursive_type_section schema{declarations()};
        gc::wasm_binfmt1_final_table_type_t table_type{};
        ::std::unique_ptr<gc::wasm_module_storage_t> module{::std::make_unique<gc::wasm_module_storage_t>()};
        ::std::array<::std::shared_ptr<gc::gc_object_store>, 1uz> canonical{};
        context()
        {
            GC_IMMUTABLE_CHAIN_CHECK(reader && module);
            module->gc_lease_roots = ::std::make_shared<gc::gc_lease_owner>();
            module->gc_store = ::std::make_shared<gc::gc_object_store>(schema, module->gc_lease_roots);
            GC_IMMUTABLE_CHAIN_CHECK(module->gc_store->valid());
            canonical[0uz] = module->gc_store;
            // Native-owned immutable metadata outlives this actual module.
            // [schema and live module] Neither address originates in Wasm.
            // [safe                  ] schema is declared before module above.
            // ^^ retain a borrowed parser-like declaration for native lifetime.
            module->type_section_storage.core3_recursive_types_ptr = ::std::addressof(schema);
            module->type_section_storage.type_section_count = 1uz;
            module->type_section_storage.requires_gc = true;
            table_type.limits.min = slots;
            table_type.limits.present_max = false;
            table_type.has_core_type = true;
            table_type.core_type.kind = type::value_kind::reference;
            table_type.core_type.heap.code = static_cast<::std::int_least64_t>(type::abstract_heap_type::any);
            table_type.core_type.nullable = true;
            table_type.core_type.source_prefix = 0x6eu;
            table_type.reftype = static_cast<decltype(table_type.reftype)>(0x6eu);
            module->local_defined_table_vec_storage.resize(1uz);
            auto& actual{module->local_defined_table_vec_storage.index_unchecked(0uz)};
            // [table_type][strongly owned module][actual complete table record]
            // [safe                                                        ]
            // ^^ bind native owners BEFORE exposing initialized table contents.
            actual.table_type_ptr = ::std::addressof(table_type);
            actual.owner_module_rt_ptr = module.get();
            actual.elems.resize(slots);
            GC_IMMUTABLE_CHAIN_CHECK(gc::runtime_table_family(actual) == gc::runtime_table_reference_family::gc);
            // This native-created instance deliberately stays unowned by the
            // product initializer protocol; no forged initializer serial grants
            // automatic-GC/JIT authority to this component diagnostic.
            GC_IMMUTABLE_CHAIN_CHECK(!module->gc_collection_phase.native_storage_owned);
        }
        [[nodiscard]] gc::local_defined_table_storage_t& table() const noexcept
        {
            GC_IMMUTABLE_CHAIN_CHECK(module->local_defined_table_vec_storage.size() == 1uz);
            return module->local_defined_table_vec_storage.index_unchecked(0uz);
        }
        inline void put(::std::size_t index, reference input) noexcept
        {
            auto& actual{table()};
            GC_IMMUTABLE_CHAIN_CHECK(actual.owner_module_rt_ptr == module.get() &&
                actual.table_type_ptr == ::std::addressof(table_type) && index < actual.elems.size());
            GC_IMMUTABLE_CHAIN_CHECK(gc::runtime_table_family(actual) == gc::runtime_table_reference_family::gc);
            // The SAME actual slot codec and table-owner retention leaves used
            // by ordinary LLVM GC table.set. No fake local/no-thread proof.
            auto const slot{gc::runtime_table_slot_from_gc_reference(input)};
            GC_IMMUTABLE_CHAIN_CHECK(gc::retain_runtime_table_reference(::std::addressof(actual), input) == status::ok);
            // [actual.elems,actual.elems+1024) initialized native typed slots.
            // [safe                        ] index < size was checked above.
            // ^^ publish the full discriminant/payload only after retention.
            actual.elems.index_unchecked(index) = slot;
        }
        [[nodiscard]] inline reference get(::std::size_t index) const noexcept
        {
            auto& actual{table()};
            GC_IMMUTABLE_CHAIN_CHECK(actual.owner_module_rt_ptr == module.get() && index < actual.elems.size());
            GC_IMMUTABLE_CHAIN_CHECK(gc::runtime_table_family(actual) == gc::runtime_table_reference_family::gc);
            // [actual.elems,actual.elems+1024) initialized complete typed slots.
            // [safe                        ] index was bounded before this read.
            // ^^ actual codec preserves aggregate kind and opaque token bits.
            auto const result{gc::runtime_table_slot_to_gc_reference(actual.elems.index_unchecked(index))};
            GC_IMMUTABLE_CHAIN_CHECK(gc::uwvm2_gc_retain_reference(module->gc_store.get(), ::std::addressof(result)) == status::ok);
            return result;
        }
        [[nodiscard]] ::std::size_t collect(::std::size_t expected) noexcept
        {
            // No other actor exists in this fixture. Actual exclusive admission
            // must protect this EXACT retained outer reader before root census.
            auto stopped{admission::runtime_gc_entry_admission.try_exclusive(1uz)};
            GC_IMMUTABLE_CHAIN_CHECK(stopped && admission::runtime_gc_entry_admission.protects_shared(stopped, reader));
            ::std::array<gc::wasm_module_storage_t const*, 1uz> actual_modules{module.get()};
            auto const modules{::std::span<gc::wasm_module_storage_t const* const>{actual_modules.data(), actual_modules.size()}};
            auto const counted{gc::visit_quiescent_cohort_static_roots(modules, [](reference) noexcept { return true; })};
            GC_IMMUTABLE_CHAIN_CHECK(counted.status == gc::gc_static_root_status::ok && counted.visited == slots);
            // This is a snapshot COPIED FROM the REAL table visitor, not a
            // stand-in table. Its complete census is stable under this pause.
            ::std::array<reference, slots> snapshot{};
            ::std::size_t used{};
            auto const copied{gc::visit_quiescent_cohort_static_roots(modules, [&](reference root) noexcept
            {
                if(used >= snapshot.size()) { return false; }
                // [snapshot,snapshot+1024) complete native reference slots.
                // [safe                  ] used was checked before assignment.
                // ^^ copy the actual visitor's discriminated root then advance.
                snapshot[used] = root;
                ++used;
                return true;
            })};
            GC_IMMUTABLE_CHAIN_CHECK(copied.status == gc::gc_static_root_status::ok && copied.visited == counted.visited && used == slots);
            ::std::size_t reclaimed{};
            GC_IMMUTABLE_CHAIN_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                canonical.data(), canonical.size(), snapshot.data(), used, reclaimed) == status::ok && reclaimed == expected);
            return reclaimed;
        }
    };
    struct measurement
    {
        ::std::uint64_t chain_ns{}, collection_ns{}, maximum_collection_ns{};
        ::std::uint32_t state{}, root_checksum{};
        ::std::size_t timed_collections{}, timed_reclaimed{}, qualification_reclaimed{};
    };
#if defined(__GNUC__) || defined(__clang__)
    [[gnu::noinline]]
#elif defined(_MSC_VER)
    __declspec(noinline)
#endif
    [[nodiscard]] measurement measure(context& fixture, ::std::size_t count, bool timed_collect) noexcept
    {
        using clock = ::std::chrono::steady_clock;
        measurement result{};
        ::std::uint32_t state{123456789u};
        reference earliest{}, latest{};
        ::std::size_t last_collection{};
        auto const begin{clock::now()};
        for(::std::size_t remaining{count}; remaining != 0uz; --remaining)
        {
            state = state * 1664525u + 1013904223u;
            auto const field{value::i32(state)};
            reference fresh{};
            GC_IMMUTABLE_CHAIN_CHECK(gc::uwvm2_gc_struct_new(fixture.module->gc_store.get(),
                0u, ::std::addressof(field), 1uz, ::std::addressof(fresh)) == status::ok);
            if(remaining == count) { earliest = fresh; }
            latest = fresh;
            // Match the original WAT EXACTLY: remaining & 1023 is evaluated
            // BEFORE the loop's decrement. This is not ascending-step indexing.
            auto const index{remaining & (slots - 1uz)};
            fixture.put(index, fresh);
            auto const loaded{fixture.get(index)};
            GC_IMMUTABLE_CHAIN_CHECK(gc::uwvm2_gc_reference_type_matches(fixture.module->gc_store.get(),
                ::std::addressof(loaded), 0ll, false));
            value output{};
            GC_IMMUTABLE_CHAIN_CHECK(gc::uwvm2_gc_struct_get(fixture.module->gc_store.get(),
                loaded, 0uz, false, ::std::addressof(output)) == status::ok);
            state = output.as<::std::uint32_t>();
            auto const allocated{count - remaining + 1uz};
            if(timed_collect && allocated % batch == 0uz)
            {
                auto const collect_begin{clock::now()};
                auto const expected{last_collection == 0uz ? allocated - slots : allocated - last_collection};
                result.timed_reclaimed += fixture.collect(expected);
                auto const collect_end{clock::now()};
                auto const elapsed{static_cast<::std::uint64_t>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(collect_end - collect_begin).count())};
                result.collection_ns += elapsed;
                if(elapsed > result.maximum_collection_ns) { result.maximum_collection_ns = elapsed; }
                ++result.timed_collections;
                last_collection = allocated;
            }
        }
        auto const end{clock::now()};
        auto const total{static_cast<::std::uint64_t>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count())};
        GC_IMMUTABLE_CHAIN_CHECK(result.collection_ns <= total);
        result.chain_ns = total - result.collection_ns;
        result.state = state;
        for(::std::size_t index{}; index != slots; ++index)
        {
            auto const root{fixture.get(index)};
            value output{};
            GC_IMMUTABLE_CHAIN_CHECK(gc::uwvm2_gc_struct_get(fixture.module->gc_store.get(), root,
                0uz, false, ::std::addressof(output)) == status::ok);
            GC_IMMUTABLE_CHAIN_CHECK(output.as<::std::uint32_t>() ==
                expected_scalar(count - ((index + slots - 1uz) % slots)));
            result.root_checksum += output.as<::std::uint32_t>();
        }
        auto const pending{last_collection == 0uz ? count - slots : count - last_collection};
        result.qualification_reclaimed += fixture.collect(pending);
        value discarded{};
        GC_IMMUTABLE_CHAIN_CHECK(fixture.module->gc_store->struct_get(earliest, 0uz, false, discarded) ==
            (count == slots ? status::ok : status::invalid_reference));
        for(auto& slot : fixture.table().elems)
        {
            // [actual complete table slot] only this single fixture-owned
            // [safe                      ] native actor can clear its root.
            // ^^ write the real null slot; privileged audit tokens are not roots.
            slot = gc::runtime_table_slot_from_gc_reference(reference{});
        }
        result.qualification_reclaimed += fixture.collect(slots);
        GC_IMMUTABLE_CHAIN_CHECK(fixture.module->gc_store->struct_get(latest, 0uz, false, discarded) == status::invalid_reference &&
            !gc::uwvm2_gc_reference_type_matches(fixture.module->gc_store.get(), ::std::addressof(earliest), 0ll, false));
        GC_IMMUTABLE_CHAIN_CHECK(result.timed_reclaimed + result.qualification_reclaimed == count);
        return result;
    }
}

int main(int argc, char** argv)
{
    namespace bench = gc_immutable_table_chain_20261003;
    GC_IMMUTABLE_CHAIN_CHECK(argc == 5);
    auto const name{::std::string_view{argv[1]}};
    GC_IMMUTABLE_CHAIN_CHECK(name == "collect4096" || name == "deferred-collect");
    ::std::uint32_t count{}, expected{}, root_checksum{};
    GC_IMMUTABLE_CHAIN_CHECK(bench::number(argv[2], count) && bench::number(argv[3], expected) && bench::number(argv[4], root_checksum));
    GC_IMMUTABLE_CHAIN_CHECK(count >= 1024u && count <= 16'000'000u);
    bench::context fixture{};
    auto const result{bench::measure(fixture, count, name == "collect4096")};
    GC_IMMUTABLE_CHAIN_CHECK(result.state == expected && result.root_checksum == root_checksum);
#if defined(UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION) && UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION == 1
    constexpr bool single_cas{true};
#else
    constexpr bool single_cas{false};
#endif
    ::fast_io::io::println("{\"family\":\"gc-immutable-table-chain\",\"collection_mode\":\"", name,
        "\",\"count\":", ::fast_io::mnp::dec(count), ",\"single_cas\":", single_cas ? "true" : "false",
        ",\"state_u32\":", ::fast_io::mnp::dec(result.state), ",\"root_checksum_u32\":", ::fast_io::mnp::dec(result.root_checksum),
        ",\"chain_ns\":", ::fast_io::mnp::dec(result.chain_ns), ",\"collection_ns\":", ::fast_io::mnp::dec(result.collection_ns),
        ",\"maximum_collection_ns\":", ::fast_io::mnp::dec(result.maximum_collection_ns),
        ",\"timed_collections\":", ::fast_io::mnp::dec(result.timed_collections), ",\"timed_reclaimed\":", ::fast_io::mnp::dec(result.timed_reclaimed),
        ",\"qualification_reclaimed\":", ::fast_io::mnp::dec(result.qualification_reclaimed),
        ",\"actual_table_slots\":1024,\"actual_static_root_visitor\":true,\"component_storage_chain_only\":true,\"vm_qualified\":false}");
}
