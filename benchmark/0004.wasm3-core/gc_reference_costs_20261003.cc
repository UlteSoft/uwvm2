// Keeper-only component diagnostic. It holds actual entry admission and two
// canonical store pins. These explicit native roots do not qualify Wasm frame
// maps, VM automatic collection, or an industry throughput comparison.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

namespace gc_reference_costs_20261003
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace admission = ::uwvm2::runtime::gc;
    using reference = gc::gc_reference;
    using value = gc::gc_object_value;
    using status = gc::gc_object_status;
    constexpr ::std::size_t root_count{1024uz};
    using roots_array = ::std::array<reference, root_count>;
    enum class phase { match_defined, get32, set32, set_reference };
    enum class relation { local, foreign_equal, foreign_subtype };

    void require(bool condition, unsigned line) noexcept
    {
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_reference_costs line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define GC_REFERENCE_COST_CHECK(...) ::gc_reference_costs_20261003::require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    [[nodiscard]] bool equal(reference a, reference b) noexcept
    { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; }
    [[nodiscard]] bool number(::std::string_view input, ::std::uint32_t& result) noexcept
    {
        if(input.empty() || !::fast_io::char_category::is_c_digit(input.front())) { return false; }
        // [input.data(),input.data()+input.size()) is the complete native argv
        // [safe                                    ] string_view bounds its end.
        // ^^ form the one-past pointer before invoking the fast_io parser.
        auto const* end{input.data() + input.size()};
        auto const parsed{::fast_io::parse_by_scan(input.data(), end, ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }
    [[nodiscard]] type::field_type scalar_field(bool mutable_) noexcept
    {
        type::field_type result{};
        result.mutable_ = mutable_;
        result.storage.value.kind = type::value_kind::i32;
        return result;
    }
    void append(type::recursive_type_section& section, type::sub_type node)
    {
        type::recursive_group group{};
        group.first_type_index = section.type_count;
        group.types.push_back(::std::move(node));
        section.groups.push_back(::std::move(group));
        ++section.type_count;
    }
    [[nodiscard]] type::recursive_type_section declarations(bool prefix)
    {
        type::recursive_type_section result{};
        if(prefix)
        {
            type::sub_type padding{};
            padding.kind = type::composite_kind::function;
            append(result, ::std::move(padding));
        }
        auto const base_index{static_cast<::std::uint_least32_t>(result.type_count)};
        type::sub_type base{};
        base.kind = type::composite_kind::struct_;
        base.final_ = false;
        base.fields.push_back(scalar_field(true));
        append(result, ::std::move(base));
        type::sub_type derived{};
        derived.kind = type::composite_kind::struct_;
        derived.supertypes.push_back(base_index);
        derived.fields.push_back(scalar_field(true));
        type::field_type tail{};
        tail.storage.value.kind = type::value_kind::i64;
        derived.fields.push_back(tail);
        append(result, ::std::move(derived));
        type::sub_type holder{};
        holder.kind = type::composite_kind::struct_;
        type::field_type edge{};
        edge.mutable_ = true;
        edge.storage.value.kind = type::value_kind::reference;
        edge.storage.value.heap.code = base_index;
        edge.storage.value.nullable = true;
        holder.fields.push_back(edge);
        append(result, ::std::move(holder));
        return result;
    }
    [[nodiscard]] constexpr ::std::uint32_t initial(::std::size_t index) noexcept
    { return static_cast<::std::uint32_t>(index) * 0x10101u + 0x13579bdu; }

    struct context
    {
        admission::managed_entry_admission::shared_lease reader{admission::runtime_gc_entry_admission.enter()};
        ::std::array<::std::shared_ptr<gc::gc_lease_owner>, 2uz> leases{
            ::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
        ::std::array<::std::shared_ptr<gc::gc_object_store>, 2uz> stores{};
        roots_array sources{}, holders{};
        ::std::array<::std::size_t, root_count> targets{};
        relation mode{};
        ::std::size_t recipient{};
        ::std::uint_least32_t expected_index{}, holder_index{};
        type::core_value_type expected{};
        explicit context(relation selected) : mode(selected)
        {
            auto source_schema{declarations(false)};
            auto recipient_schema{declarations(true)};
            stores[0uz] = ::std::make_shared<gc::gc_object_store>(source_schema, leases[0uz]);
            stores[1uz] = ::std::make_shared<gc::gc_object_store>(recipient_schema, leases[1uz]);
            GC_REFERENCE_COST_CHECK(reader && stores[0uz]->valid() && stores[1uz]->valid());
            // Canonical equality must hold despite DISTINCT local type indices.
            // A proper subtype has a different ID and an actual declared parent.
            GC_REFERENCE_COST_CHECK(stores[0uz]->canonical_type_id(0u) == stores[1uz]->canonical_type_id(1u));
            GC_REFERENCE_COST_CHECK(stores[0uz]->canonical_type_id(1u) == stores[1uz]->canonical_type_id(2u));
            GC_REFERENCE_COST_CHECK(stores[0uz]->canonical_type_id(0u) != stores[0uz]->canonical_type_id(1u));
            recipient = mode == relation::local ? 0uz : 1uz;
            expected_index = static_cast<::std::uint_least32_t>(recipient);
            holder_index = expected_index + 2u;
            expected.kind = type::value_kind::reference;
            expected.heap.code = expected_index;
            expected.nullable = true;
            auto const actual_type{mode == relation::foreign_subtype ? 1u : 0u};
            for(::std::size_t index{}; index != root_count; ++index)
            {
                ::std::array<value, 2uz> fields{value::i32(initial(index)), value::i64(0x8899aabbccddeeffull)};
                // [sources, sources+1024) native complete reference slots.
                // [safe                   ] index < root_count before indexing.
                // ^^ write a fresh opaque token; never reinterpret it as a header.
                GC_REFERENCE_COST_CHECK(stores[0uz]->struct_new(actual_type, fields.data(),
                    actual_type == 1u ? 2uz : 1uz, sources[index]) == status::ok);
                auto const input{value::reference(sources[index])};
                // [holders,holders+1024) owns initialized output slots, index
                // [safe                ] is bounded by this construction loop.
                // ^^ ordinary struct_new performs real typed field retention.
                GC_REFERENCE_COST_CHECK(stores[recipient]->struct_new(holder_index,
                    ::std::addressof(input), 1uz, holders[index]) == status::ok);
                targets[index] = index;
            }
            GC_REFERENCE_COST_CHECK(store().reference_type_matches(sources[0uz], expected));
            auto wrong{expected};
            wrong.heap.code = holder_index;
            GC_REFERENCE_COST_CHECK(!store().reference_type_matches(sources[0uz], wrong));
        }
        [[nodiscard]] gc::gc_object_store& store() const noexcept { return *stores[recipient]; }
        [[nodiscard]] ::std::size_t collect(reference const* roots, ::std::size_t count,
            ::std::size_t wanted) noexcept
        {
            // This exact fixture owns every native actor. An actual exclusive
            // lease protects the retained outer reader; a bare stopped bool or
            // the cohort enrollment lock would not authorize heap destruction.
            auto stopped{admission::runtime_gc_entry_admission.try_exclusive(1uz)};
            GC_REFERENCE_COST_CHECK(stopped && admission::runtime_gc_entry_admission.protects_shared(stopped, reader));
            ::std::size_t reclaimed{};
            GC_REFERENCE_COST_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                stores.data(), stores.size(), roots, count, reclaimed) == status::ok && reclaimed == wanted);
            return reclaimed;
        }
    };
    struct measurement
    {
        ::std::uint64_t api_ns{};
        ::std::uint32_t checksum{}, scalar_checksum{};
        ::std::size_t kept_sources{}, first_reclaimed{}, final_reclaimed{};
    };
#if defined(__GNUC__) || defined(__clang__)
    [[gnu::noinline]]
#elif defined(_MSC_VER)
    __declspec(noinline)
#endif
    [[nodiscard]] measurement measure(context& fixture, phase selected, ::std::size_t units) noexcept
    {
        measurement result{};
        ::std::uint32_t state{123456789u};
        auto const begin{::std::chrono::steady_clock::now()};
        for(::std::size_t step{}; step != units; ++step)
        {
            state = state * 1664525u + 1013904223u;
            auto const slot{static_cast<::std::size_t>(state >> 10u) & (root_count - 1uz)};
            // [sources,sources+1024) owns all complete tokens. The mask bounds
            // [safe                ] slot before any native array access.
            // ^^ copying this token grants no native pointer authority.
            auto const current{fixture.sources[slot]};
            if(selected == phase::match_defined)
            {
                auto const matched{fixture.store().reference_type_matches(current, fixture.expected)};
                GC_REFERENCE_COST_CHECK(matched);
                result.checksum += static_cast<::std::uint32_t>(slot) + static_cast<::std::uint32_t>(matched);
            }
            else if(selected == phase::get32)
            {
                auto const packed{fixture.store().struct_get32<false>(current, 0uz)};
                GC_REFERENCE_COST_CHECK((packed >> 32u) == 0u);
                result.checksum += static_cast<::std::uint32_t>(packed);
            }
            else if(selected == phase::set32)
            {
                GC_REFERENCE_COST_CHECK(fixture.store().struct_set(current, 0uz, value::i32(state)) == status::ok);
                auto const packed{fixture.store().struct_get32<false>(current, 0uz)};
                GC_REFERENCE_COST_CHECK((packed >> 32u) == 0u && static_cast<::std::uint32_t>(packed) == state);
                result.checksum += static_cast<::std::uint32_t>(packed);
            }
            else
            {
                auto const destination{step & (root_count - 1uz)};
                // [holders,holders+1024) owns all outputs; destination is
                // [safe                ] masked below this complete extent.
                // ^^ checked struct_set owns all dynamic type/lease/lock checks.
                GC_REFERENCE_COST_CHECK(fixture.store().struct_set(fixture.holders[destination],
                    0uz, value::reference(current)) == status::ok);
                value observed{};
                GC_REFERENCE_COST_CHECK(fixture.store().struct_get(fixture.holders[destination], 0uz, false, observed) == status::ok &&
                    equal(observed.as<reference>(), current));
                fixture.targets[destination] = slot;
                result.checksum += static_cast<::std::uint32_t>(slot);
            }
        }
        auto const end{::std::chrono::steady_clock::now()};
        result.api_ns = static_cast<::std::uint64_t>(::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count());
        ::std::array<bool, root_count> retained{};
        for(::std::size_t index{}; index != root_count; ++index)
        {
            GC_REFERENCE_COST_CHECK(fixture.targets[index] < root_count);
            retained[fixture.targets[index]] = true;
            value observed{};
            GC_REFERENCE_COST_CHECK(fixture.store().struct_get(fixture.holders[index], 0uz, false, observed) == status::ok &&
                equal(observed.as<reference>(), fixture.sources[fixture.targets[index]]));
        }
        for(::std::size_t index{}; index != root_count; ++index)
        {
            auto const packed{fixture.store().struct_get32<false>(fixture.sources[index], 0uz)};
            GC_REFERENCE_COST_CHECK((packed >> 32u) == 0u);
            result.scalar_checksum += static_cast<::std::uint32_t>(packed);
            result.kept_sources += retained[index];
        }
        // Privileged audit tokens are intentionally absent from semantic
        // roots. Strong native store pins keep arenas, not every object, live.
        result.first_reclaimed = fixture.collect(fixture.holders.data(), fixture.holders.size(), root_count - result.kept_sources);
        for(::std::size_t index{}; index != root_count; ++index)
        {
            auto const packed{fixture.store().struct_get32<false>(fixture.sources[index], 0uz)};
            GC_REFERENCE_COST_CHECK((packed >> 32u) == static_cast<::std::uint64_t>(
                retained[index] ? status::ok : status::invalid_reference));
        }
        result.final_reclaimed = fixture.collect(nullptr, 0uz, root_count + result.kept_sources);
        value retired{};
        GC_REFERENCE_COST_CHECK(fixture.store().struct_get(fixture.holders[0uz], 0uz, false, retired) == status::invalid_reference);
        GC_REFERENCE_COST_CHECK(!fixture.store().reference_type_matches(fixture.sources[0uz], fixture.expected));
        GC_REFERENCE_COST_CHECK(result.first_reclaimed + result.final_reclaimed == root_count * 2uz);
        return result;
    }
}

int main(int argc, char** argv)
{
    namespace bench = gc_reference_costs_20261003;
    GC_REFERENCE_COST_CHECK(argc == 6);
    auto const name{::std::string_view{argv[1]}};
    auto const relation_name{::std::string_view{argv[2]}};
    GC_REFERENCE_COST_CHECK(name == "match-defined" || name == "get32" || name == "set32" || name == "set-reference");
    GC_REFERENCE_COST_CHECK(relation_name == "local" || relation_name == "foreign-equal" || relation_name == "foreign-subtype");
    auto const selected{name == "match-defined" ? bench::phase::match_defined : name == "get32" ? bench::phase::get32 :
        name == "set32" ? bench::phase::set32 : bench::phase::set_reference};
    auto const mode{relation_name == "local" ? bench::relation::local : relation_name == "foreign-equal" ?
        bench::relation::foreign_equal : bench::relation::foreign_subtype};
    ::std::uint32_t units{}, checksum{}, scalar{};
    GC_REFERENCE_COST_CHECK(bench::number(argv[3], units) && bench::number(argv[4], checksum) && bench::number(argv[5], scalar));
    GC_REFERENCE_COST_CHECK(units >= 1024u && units <= 16'000'000u);
    bench::context fixture{mode};
    auto const result{bench::measure(fixture, selected, units)};
    GC_REFERENCE_COST_CHECK(result.checksum == checksum && result.scalar_checksum == scalar);
#if defined(UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION) && UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION == 1
    constexpr bool single_cas{true};
#else
    constexpr bool single_cas{false};
#endif
    ::fast_io::io::println("{\"family\":\"gc-reference-costs\",\"phase\":\"", name,
        "\",\"relation\":\"", relation_name, "\",\"units\":", ::fast_io::mnp::dec(units),
        ",\"single_cas\":", single_cas ? "true" : "false", ",\"api_ns\":", ::fast_io::mnp::dec(result.api_ns),
        ",\"checksum_u32\":", ::fast_io::mnp::dec(result.checksum), ",\"scalar_checksum_u32\":", ::fast_io::mnp::dec(result.scalar_checksum),
        ",\"allocations\":2048,\"kept_sources\":", ::fast_io::mnp::dec(result.kept_sources),
        ",\"first_reclaimed\":", ::fast_io::mnp::dec(result.first_reclaimed), ",\"final_reclaimed\":", ::fast_io::mnp::dec(result.final_reclaimed),
        ",\"real_entry_and_exclusive_leases\":true,\"vm_qualified\":false}");
}
