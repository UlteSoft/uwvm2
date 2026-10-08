// Keeper-only fresh source-bound GC API diagnostic. A complete, single native
// cohort owns every root and actual reader lease; this does not qualify Wasm
// automatic roots, threads, JIT code or same-Wasm industry performance.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/uwvm/runtime/storage/gc_trace_metadata.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace native_gc_costs_dense_20261003
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace admission = ::uwvm2::runtime::gc;
    using reference = gc::gc_reference;
    using value = gc::gc_object_value;
    using status = gc::gc_object_status;
    using clock = ::std::chrono::steady_clock;
    constexpr ::std::size_t roots_count{1024uz}, batch_count{4096uz};
    using roots_array = ::std::array<reference, roots_count>;
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
    constexpr bool precise_metadata_enabled{true};
#else
    constexpr bool precise_metadata_enabled{false};
#endif
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
    constexpr bool inline_metadata_enabled{true};
#else
    constexpr bool inline_metadata_enabled{false};
#endif

    void require(bool condition, unsigned line) noexcept
    {
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_native_api_costs_dense line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define NATIVE_GC_COST_CHECK(...) ::native_gc_costs_dense_20261003::require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    [[nodiscard]] bool equal(reference a, reference b) noexcept
    { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; }
    [[nodiscard]] ::std::uint64_t nanoseconds(clock::time_point begin, clock::time_point end) noexcept
    { return static_cast<::std::uint64_t>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count()); }
    [[nodiscard]] bool number(::std::string_view argument, ::std::uint32_t& result) noexcept
    {
        // [argument.data(),argument.data()+argument.size()) is the complete
        // native OS argument. Prove this extent before advancing the end.
        // [safe                                             ]
        auto const* end{argument.data() + argument.size()};
        auto const parsed{::fast_io::parse_by_scan(argument.data(), end, ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }
    [[nodiscard]] type::recursive_type_section declarations()
    {
        type::recursive_type_section section{};
        section.type_count = 6u;
        type::recursive_group group{};
        for(::std::size_t shape{}; shape != 6uz; ++shape)
        {
            auto const width{shape == 0uz ? 1uz : shape == 1uz ? 2uz : (shape == 2uz || shape == 4uz) ? 64uz : 65uz};
            type::sub_type node{};
            node.kind = type::composite_kind::struct_;
            for(::std::size_t field{}; field != width; ++field)
            {
                type::field_type current{};
                current.mutable_ = true;
                current.storage.value.kind = type::value_kind::i32;
                if(shape >= 4uz || (shape != 0uz && field == width - 1uz))
                {
                    current.storage.value.kind = type::value_kind::reference;
                    current.storage.value.heap.code = static_cast<::std::int_least64_t>(type::abstract_heap_type::eq);
                    current.storage.value.nullable = true;
                }
                node.fields.push_back(current);
            }
            group.types.push_back(::std::move(node));
        }
        section.groups.push_back(::std::move(group));
        return section;
    }
    struct owned_context
    {
        // The actual module-like lease owner remains alive until AFTER store
        // and every borrowed reference are retired. No copied weak owner is
        // mistaken for a canonical pin or participant admission.
        ::std::shared_ptr<gc::gc_lease_owner> leases{::std::make_shared<gc::gc_lease_owner>()};
        admission::managed_entry_admission::shared_lease shared{admission::runtime_gc_entry_admission.enter()};
        ::std::array<::std::shared_ptr<gc::gc_object_store>, 1uz> cohort{};
        roots_array roots{};
        owned_context()
        {
            auto schema{declarations()};
            cohort[0uz] = ::std::make_shared<gc::gc_object_store>(schema, leases);
            NATIVE_GC_COST_CHECK(cohort[0uz]->valid() && static_cast<bool>(shared));
        }
        [[nodiscard]] gc::gc_object_store& store() const noexcept { return *cohort[0uz]; }
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] ::std::size_t collect(::std::size_t expected, bool keep_roots) noexcept
        {
            // All native actors and the complete explicit root array are
            // fixture-owned. Acquire a genuine exclusive lease protecting this
            // exact outer reader; a bool, active count or OS TID is not proof.
            auto stopped{admission::runtime_gc_entry_admission.try_exclusive(1uz)};
            NATIVE_GC_COST_CHECK(stopped && admission::runtime_gc_entry_admission.protects_shared(stopped, shared));
            ::std::size_t reclaimed{};
            NATIVE_GC_COST_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                cohort.data(), cohort.size(), keep_roots ? roots.data() : nullptr,
                keep_roots ? roots.size() : 0uz, reclaimed) == status::ok && reclaimed == expected);
            return reclaimed;
        }
    };
    [[nodiscard]] ::std::uint32_t scalar(gc::gc_object_store& store, reference root, bool dense = false) noexcept
    {
        value result{};
        NATIVE_GC_COST_CHECK(store.struct_get(root, 0uz, false, result) == status::ok);
        if(dense)
        {
            auto const reference{result.as<gc::gc_reference>()};
            NATIVE_GC_COST_CHECK(reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31);
            // Read the active i31 union member. It carries no host pointer.
            return reference.storage.wasm_i31.get_u();
        }
        return result.as<::std::uint32_t>();
    }
    [[nodiscard]] ::std::uint32_t root_checksum(owned_context& context, bool dense = false) noexcept
    {
        ::std::uint32_t checksum{};
        for(auto root : context.roots) { checksum += scalar(context.store(), root, dense); }
        return checksum;
    }
    struct measurement
    {
        ::std::uint64_t timed_api_ns{}, timed_collection_ns{}, maximum_collection_ns{};
        ::std::size_t allocations{}, timed_collections{}, timed_reclaimed{}, qualification_reclaimed{};
        ::std::uint32_t step_checksum{}, root_checksum{};
        ::std::size_t field_count{}, api_reads_per_unit{}, api_writes_per_unit{};
    };
    void finish(owned_context& context, measurement& result, reference first_retired,
        ::std::size_t pending_garbage) noexcept
    {
        // Untimed real reclamation/stale-token controls are separate from the
        // measured API/collector interval. Whole-process wall time includes them.
        result.qualification_reclaimed += context.collect(pending_garbage, true);
        value discarded{};
        if(pending_garbage != 0uz)
        { NATIVE_GC_COST_CHECK(context.store().struct_get(first_retired, 0uz, false, discarded) == status::invalid_reference); }
        // Keep only a privileged stale-token audit sample, never a semantic
        // root or a native dereferenceable address. Clear the real root array
        // before asking the closed collector to retire the final live objects.
        auto const last{context.roots[0uz]};
        context.roots.fill(reference{});
        result.qualification_reclaimed += context.collect(roots_count, false);
        NATIVE_GC_COST_CHECK(context.store().struct_get(last, 0uz, false, discarded) == status::invalid_reference);
    }
    [[nodiscard]] measurement allocation(owned_context& context, ::std::size_t units, bool references) noexcept
    {
        measurement result{};
        result.field_count = references ? 2uz : 1uz;
        ::std::array<value, 2uz> fields{value::i32(0u), value::reference(reference{})};
        ::std::uint32_t state{123456789u};
        reference earliest{};
        auto const begin{clock::now()};
        for(::std::size_t index{}; index != units; ++index)
        {
            state = state * 1664525u + 1013904223u;
            fields[0uz] = value::i32(state);
            reference object{};
            NATIVE_GC_COST_CHECK(context.store().struct_new(references ? 1u : 0u,
                fields.data(), result.field_count, object) == status::ok);
            if(index == 0uz) { earliest = object; }
            // [roots,roots+1024) native initialized slots; mask is below 1024.
            context.roots[index & (roots_count - 1uz)] = object;
            result.step_checksum += state;
        }
        result.timed_api_ns = nanoseconds(begin, clock::now());
        result.allocations = units;
        result.root_checksum = root_checksum(context);
        finish(context, result, earliest, units - roots_count);
        return result;
    }
    [[nodiscard]] measurement mutation(owned_context& context, ::std::size_t units, bool references) noexcept
    {
        measurement result{};
        result.field_count = references ? 2uz : 1uz;
        result.api_reads_per_unit = references ? 2uz : 1uz;
        result.api_writes_per_unit = 1uz;
        ::std::array<value, 2uz> fields{value::i32(0u), value::reference(reference{})};
        roots_array wanted{};
        for(::std::size_t index{}; index != roots_count; ++index)
        {
            fields[0uz] = value::i32(references ? static_cast<::std::uint32_t>(index) : 0u);
            NATIVE_GC_COST_CHECK(context.store().struct_new(references ? 1u : 0u,
                fields.data(), result.field_count, context.roots[index]) == status::ok);
        }
        ::std::uint32_t state{123456789u};
        auto const begin{clock::now()};
        for(::std::size_t index{}; index != units; ++index)
        {
            // [0,roots_count) both arrays own complete initialized slots.
            // The mask bounds slot before selecting an authentic root.
            auto const slot{index & (roots_count - 1uz)};
            auto const current{context.roots[slot]};
            value observed{};
            if(references)
            {
                state = state * 1664525u + 1013904223u;
                // The unsigned shift is 10 < 32. The mask bounds this varying
                // target below the complete 1024-slot native root array.
                auto const target_index{static_cast<::std::size_t>(state >> 10u) & (roots_count - 1uz)};
                auto const target{context.roots[target_index]};
                NATIVE_GC_COST_CHECK(context.store().struct_get(current, 1uz, false, observed) == status::ok &&
                    equal(observed.as<reference>(), wanted[slot]));
                NATIVE_GC_COST_CHECK(context.store().struct_set(current, 1uz, value::reference(target)) == status::ok);
                NATIVE_GC_COST_CHECK(context.store().struct_get(current, 1uz, false, observed) == status::ok &&
                    equal(observed.as<reference>(), target));
                wanted[slot] = target;
                result.step_checksum += static_cast<::std::uint32_t>(target_index);
            }
            else
            {
                state = state * 1664525u + 1013904223u;
                NATIVE_GC_COST_CHECK(context.store().struct_get(current, 0uz, false, observed) == status::ok);
                result.step_checksum += observed.as<::std::uint32_t>() + state;
                NATIVE_GC_COST_CHECK(context.store().struct_set(current, 0uz, value::i32(state)) == status::ok);
            }
        }
        result.timed_api_ns = nanoseconds(begin, clock::now());
        result.allocations = roots_count;
        if(references)
        {
            for(::std::size_t index{}; index != roots_count; ++index)
            {
                value observed{};
                NATIVE_GC_COST_CHECK(context.store().struct_get(context.roots[index], 1uz, false, observed) == status::ok &&
                    equal(observed.as<reference>(), wanted[index]));
                result.root_checksum += scalar(context.store(), observed.as<reference>());
            }
        }
        else { result.root_checksum = root_checksum(context); }
        finish(context, result, {}, 0uz);
        return result;
    }
    [[nodiscard]] measurement tracing(owned_context& context, ::std::size_t rounds, ::std::size_t width, bool dense) noexcept
    {
        measurement result{};
        result.field_count = width;
        ::std::array<value, 65uz> fields{};
        // [fields,fields+65) initialized owned carriers. Width is exactly 64/65
        // from the native phase selector before width-1 or a payload is accessed.
        NATIVE_GC_COST_CHECK(width == 64uz || width == 65uz);
        if(dense)
        {
            // Every active [fields,fields+width) carrier is a nullable eqref.
            // Width <= 65 is proven above before the index is advanced.
            for(::std::size_t field{}; field != width; ++field)
            { fields[field] = value::reference(reference{}); }
        }
        else { fields[width - 1uz] = value::reference(reference{}); }
        ::std::uint32_t state{123456789u};
        for(::std::size_t round{}; round != rounds; ++round)
        {
            for(::std::size_t index{}; index != batch_count; ++index)
            {
                state = state * 1664525u + 1013904223u;
                auto const payload{dense ? state & 0x7fffffffu : state};
                fields[0uz] = dense ? value::reference(::uwvm2::object::global::make_wasm_i31_reference(
                    static_cast<::std::int32_t>(payload))) : value::i32(payload);
                reference fresh{};
                auto const shape{static_cast<::std::uint_least32_t>((dense ? 4u : 2u) + (width == 65uz ? 1u : 0u))};
                NATIVE_GC_COST_CHECK(context.store().struct_new(shape, fields.data(), width, fresh) == status::ok);
                if(dense)
                {
                    // [1,width) excludes the genuine i31 scalar; each remaining
                    // initialized canonical reference field holds a self edge.
                    for(::std::size_t field{1uz}; field != width; ++field)
                    { NATIVE_GC_COST_CHECK(context.store().struct_set(fresh, field, value::reference(fresh)) == status::ok); }
                }
                else
                { NATIVE_GC_COST_CHECK(context.store().struct_set(fresh, width - 1uz, value::reference(fresh)) == status::ok); }
                // [roots,roots+1024) mask bounds the only true root publication.
                context.roots[index & (roots_count - 1uz)] = fresh;
                result.step_checksum += payload;
            }
            result.allocations += batch_count;
            auto const expected{round == 0uz ? batch_count - roots_count : batch_count};
            auto const begin{clock::now()};
            result.timed_reclaimed += context.collect(expected, true);
            auto const duration{nanoseconds(begin, clock::now())};
            result.timed_collection_ns += duration;
            if(duration > result.maximum_collection_ns) { result.maximum_collection_ns = duration; }
            ++result.timed_collections;
            // Untimed reachability and exact self-cycle readback after every
            // committed sweep. Cached guest bit patterns never select pointers.
            for(auto root : context.roots)
            {
                auto const first_edge{dense ? 1uz : width - 1uz};
                // [first_edge,width) is nonempty and canonical for this shape.
                // Verify every true heap edge after each committed collection.
                for(::std::size_t field{first_edge}; field != width; ++field)
                {
                    value edge{};
                    NATIVE_GC_COST_CHECK(context.store().struct_get(root, field, false, edge) == status::ok &&
                        equal(edge.as<reference>(), root));
                }
                static_cast<void>(scalar(context.store(), root, dense));
            }
        }
        result.root_checksum = root_checksum(context, dense);
        finish(context, result, {}, 0uz);
        return result;
    }
}
int main(int argc, char** argv)
{
    namespace bench = native_gc_costs_dense_20261003;
    NATIVE_GC_COST_CHECK(argc == 5);
    ::std::string_view phase{argv[1]};
    ::std::uint32_t units{}, step{}, roots{};
    NATIVE_GC_COST_CHECK(bench::number(argv[2], units) && bench::number(argv[3], step) && bench::number(argv[4], roots));
    bool const dense{phase == "trace-dense64" || phase == "trace-dense65"};
    bool const trace{phase == "trace64" || phase == "trace65" || dense};
    bool const allocate{phase == "allocate-numeric" || phase == "allocate-reference"};
    bool const mutate{phase == "mutate-numeric" || phase == "mutate-reference"};
    NATIVE_GC_COST_CHECK(trace || allocate || mutate);
    NATIVE_GC_COST_CHECK(trace ? units >= 1u && units <= 256u :
        units >= 4096u && units <= (allocate ? 1'000'000u : 16'000'000u));
    auto const construction_begin{bench::clock::now()};
    bench::owned_context context{};
    auto const construction_ns{bench::nanoseconds(construction_begin, bench::clock::now())};
    auto result{trace ? bench::tracing(context, units, phase == "trace64" || phase == "trace-dense64" ? 64uz : 65uz, dense) :
        allocate ? bench::allocation(context, units, phase == "allocate-reference") :
        bench::mutation(context, units, phase == "mutate-reference")};
    NATIVE_GC_COST_CHECK(result.step_checksum == step && result.root_checksum == roots);
    NATIVE_GC_COST_CHECK(result.allocations == result.timed_reclaimed + result.qualification_reclaimed);
    ::fast_io::io::println("{\"schema\":\"uwvm-gc-native-api-costs-dense-v2\",\"phase\":\"", phase,
        "\",\"units\":", ::fast_io::mnp::dec(units), ",\"root_count\":1024,\"field_count\":", ::fast_io::mnp::dec(result.field_count),
        ",\"dense_reference_layout\":", ::fast_io::mnp::os_c_str(dense ? "true" : "false"),
        ",\"typed_reference_fields_per_trace_object\":", ::fast_io::mnp::dec(trace ? (dense ? result.field_count : 1uz) : 0uz),
        ",\"heap_self_edges_per_trace_object\":", ::fast_io::mnp::dec(trace ? (dense ? result.field_count - 1uz : 1uz) : 0uz),
        ",\"step_checksum_u32\":", ::fast_io::mnp::dec(result.step_checksum), ",\"root_checksum_u32\":", ::fast_io::mnp::dec(result.root_checksum),
        ",\"construction_ns\":", ::fast_io::mnp::dec(construction_ns), ",\"timed_api_ns\":", ::fast_io::mnp::dec(result.timed_api_ns),
        ",\"timed_collection_ns\":", ::fast_io::mnp::dec(result.timed_collection_ns), ",\"maximum_collection_ns\":", ::fast_io::mnp::dec(result.maximum_collection_ns),
        ",\"allocations\":", ::fast_io::mnp::dec(result.allocations), ",\"timed_collections\":", ::fast_io::mnp::dec(result.timed_collections),
        ",\"timed_reclaimed\":", ::fast_io::mnp::dec(result.timed_reclaimed), ",\"qualification_reclaimed\":", ::fast_io::mnp::dec(result.qualification_reclaimed),
        ",\"api_reads_per_mutation_unit\":", ::fast_io::mnp::dec(result.api_reads_per_unit), ",\"api_writes_per_mutation_unit\":", ::fast_io::mnp::dec(result.api_writes_per_unit),
        ",\"metadata_object_bytes\":", ::fast_io::mnp::dec(sizeof(bench::gc::gc_trace_metadata)),
        ",\"metadata_plan_present_in_layout\":", ::fast_io::mnp::os_c_str(bench::precise_metadata_enabled ? "true" : "false"),
        ",\"inline_metadata_enabled\":", ::fast_io::mnp::os_c_str(bench::inline_metadata_enabled ? "true" : "false"),
        ",\"actual_exclusive_reader_protected\":true,\"component_only\":true,\"vm_qualified\":false,\"same_wasm_comparison\":false}");
}
