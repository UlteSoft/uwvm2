// Include ONLY inside runtime_checkpoint_gc_state_borrow's private section.
// All callers already retain its genuine lexical execution/cohort/host/N/publication
// guards. These nested work records are cold construction DATA, never credentials.
#pragma once
using census_state = ::uwvm2::uwvm::debugger::checkpoint::state;
using census_object = ::uwvm2::uwvm::debugger::checkpoint::object;
using census_object_id = ::uwvm2::uwvm::debugger::checkpoint::object_id;
using census_object_kind = ::uwvm2::uwvm::debugger::checkpoint::object_kind;
using census_value = ::uwvm2::uwvm::debugger::checkpoint::value;
using census_error = ::uwvm2::uwvm::debugger::checkpoint::error;
struct census_pending_reference
{
    reference ref{}; // Canonically checked native comparison key; never serialized.
    ::std::size_t owner{};
    census_object_id wire{};
    ::uwvm2::runtime::exception::value_ref exception{};
};
struct census_function_plan_pin
{
    ::std::size_t owner{}, local{};
    ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
};
struct complete_census_work
{
    // Existing canonical pin/resolver machinery only. actual.output is unused:
    // no debugger VIEW, pagination, display value or copied bool is authority.
    work actual{};
    census_state snapshot{};
    ::uwvm2::uwvm::debugger::checkpoint::limits cap{};
    ::std::vector<census_object_id> module_ids{}, instance_ids{};
    ::std::vector<::std::vector<census_object_id>> function_ids{}, tag_ids{},
        table_ids{}, memory_ids{}, global_ids{}, data_ids{}, element_ids{};
    ::std::vector<census_pending_reference> pending{};
    // Strong actual compiler plan owners are retained as cold attachment DATA.
    // They never replace the real execution/cohort/publication lifetime guards.
    ::std::vector<census_function_plan_pin> function_plan_pins{};
    ::std::uint64_t links{}, values{}, payload{}, wire_bytes{128u + 48u};
    census_error status{census_error::none};
    struct reference_index_entry
    {
        reference_kind kind{reference_kind::wasm_null};
        ::std::uintptr_t key{};
        census_object_id wire{}; // zero means unused bucket, never a valid object.
        ::std::size_t exception_pending{SIZE_MAX}; // actual strong owner in pending, not a token.
    };
    ::std::vector<reference_index_entry> reference_index{};
    ::std::size_t indexed_references{};
    [[nodiscard]] bool fail(census_error error) noexcept
    { if(status == census_error::none) { status = error; } return false; }
    [[nodiscard]] bool charge_wire(::std::uint64_t count, ::std::uint64_t width) noexcept
    {
        if(status != census_error::none || width == 0u || wire_bytes > cap.max_file_bytes ||
            count > (cap.max_file_bytes - wire_bytes) / width) { return fail(census_error::limit_exceeded); }
        wire_bytes += count * width; // Division preflight BEFORE multiplication/addition.
        return true;
    }
    [[nodiscard]] bool charge_count(::std::size_t count, ::std::uint64_t limit,
        ::std::uint64_t& total, ::std::uint64_t width) noexcept
    {
        if(total > limit || count > limit - total || !charge_wire(count, width))
        { return fail(census_error::limit_exceeded); }
        total += count; return true;
    }
    [[nodiscard]] bool charge_values(::std::size_t count) noexcept
    { return charge_count(count, cap.max_values, values, 48u); }
    [[nodiscard]] bool charge_links(::std::size_t count) noexcept
    { return charge_count(count, cap.max_links, links, 8u); }
    [[nodiscard]] bool charge_payload(::std::size_t count) noexcept
    { return charge_count(count, cap.max_payload_bytes, payload, 1u); }
    [[nodiscard]] census_object_id allocate(census_object_kind kind)
    {
        if(snapshot.objects.size() >= cap.max_objects ||
            snapshot.objects.size() >= snapshot.objects.max_size() ||
            snapshot.objects.size() >= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_object) ||
            !::uwvm2::uwvm::debugger::checkpoint::known(kind) || !charge_wire(1u, 96u))
        { (void)fail(census_error::limit_exceeded); return 0u; }
        census_object item{}; item.kind = kind; snapshot.objects.push_back(::std::move(item));
        return static_cast<census_object_id>(snapshot.objects.size());
    }
    [[nodiscard]] census_object* object(census_object_id id) noexcept
    {
        if(id == 0u || id > snapshot.objects.size()) { (void)fail(census_error::invalid_reference); return nullptr; }
        // [owned dense objects ... id-1<size] end
        // [safe] ID bound BEFORE index; caller MUST reacquire after allocate().
        return ::std::addressof(snapshot.objects[static_cast<::std::size_t>(id - 1u)]);
    }
    [[nodiscard]] bool append_link(census_object_id id, census_object_id target)
    {
        auto* item{object(id)};
        if(item == nullptr || target == 0u || target > snapshot.objects.size())
        { return fail(census_error::invalid_reference); }
        if(item->links.size() >= item->links.max_size() || item->links.size() >=
            static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_object_id))
        { return fail(census_error::limit_exceeded); }
        if(!charge_links(1u)) { return false; }
        item->links.push_back(target); return true; // No snapshot.objects allocation.
    }
    [[nodiscard]] bool append_value(census_object_id id, census_value const& value)
    {
        auto* item{object(id)}; if(item == nullptr) { return false; }
        if(item->values.size() >= item->values.max_size() || item->values.size() >=
            static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_value))
        { return fail(census_error::limit_exceeded); }
        if(!charge_values(1u)) { return false; }
        item->values.push_back(value); return true; // No snapshot.objects allocation.
    }
    [[nodiscard]] static bool pointer_key(reference ref, ::std::uintptr_t& key) noexcept
    {
        switch(ref.kind)
        {
            case reference_kind::wasm_func_imported: case reference_kind::wasm_func_defined:
            case reference_kind::wasm_struct: case reference_kind::wasm_array:
            case reference_kind::wasm_extern:
                key = reinterpret_cast<::std::uintptr_t>(ref.storage.ptr); return key != 0u;
            // Never read the pointer union member for numeric/i31/parser/null.
            default: key = 0u; return false;
        }
    }
    [[nodiscard]] static ::std::uint64_t reference_hash(reference_kind kind, ::std::uintptr_t key) noexcept
    {
        // Cold open-address bucket selection only; hash is not authentication.
        ::std::uint64_t bits{static_cast<::std::uint64_t>(key) ^ (static_cast<::std::uint64_t>(kind) * 0x9e3779b97f4a7c15u)};
        bits ^= bits >> 30u; bits *= 0xbf58476d1ce4e5b9u;
        bits ^= bits >> 27u; bits *= 0x94d049bb133111ebu; return bits ^ (bits >> 31u);
    }
    [[nodiscard]] census_object_id find_reference(reference ref) const noexcept
    {
        ::std::uintptr_t key{};
        if(reference_index.empty() || !pointer_key(ref, key)) { return 0u; }
        auto const mask{reference_index.size() - 1u};
        auto bucket{static_cast<::std::size_t>(reference_hash(ref.kind, key)) & mask};
        for(::std::size_t probes{}; probes != reference_index.size(); ++probes)
        {
            // [power-of-two owned buckets: bucket<=mask<size] end
            // [safe] mask keeps every wrap/index inside this actual allocation.
            auto const& entry{reference_index[bucket]};
            if(entry.wire == 0u) { return 0u; }
            if(entry.kind == ref.kind && entry.key == key) { return entry.wire; }
            bucket = (bucket + 1u) & mask; // size<=PTRDIFF/entry, no SIZE_MAX wrap.
        }
        return 0u;
    }
    [[nodiscard]] census_object_id find_exception(
        ::uwvm2::runtime::exception::value_ref const& exception) noexcept
    {
        if(!exception || reference_index.empty()) { return 0u; }
        auto const key{reinterpret_cast<::std::uintptr_t>(exception.get())};
        auto const mask{reference_index.size() - 1u};
        auto bucket{static_cast<::std::size_t>(reference_hash(reference_kind::wasm_exn, key)) & mask};
        for(::std::size_t probes{}; probes != reference_index.size(); ++probes)
        {
            // [owned power-of-two buckets: bucket<=mask<size] end
            // [safe] mask bounds each probe; only actual registry-issued strong
            // values reach this cold identity lookup after issuer/tag validation.
            auto const& entry{reference_index[bucket]};
            if(entry.wire == 0u) { return 0u; }
            if(entry.kind == reference_kind::wasm_exn && entry.key == key)
            {
                if(entry.exception_pending >= pending.size())
                { (void)fail(census_error::invalid_reference); return 0u; }
                // [owned pending ... exception_pending<size] end
                // [safe] ordinal checked BEFORE index. Address equality alone
                // cannot merge aliases owning an unrelated shared control block.
                auto const& canonical{pending[entry.exception_pending].exception};
                if(!canonical || canonical.get() != exception.get() ||
                    canonical.owner_before(exception) || exception.owner_before(canonical))
                { (void)fail(census_error::invalid_reference); return 0u; }
                return entry.wire;
            }
            bucket = (bucket + 1u) & mask;
        }
        return 0u;
    }
    [[nodiscard]] bool remember_exception(::std::size_t ordinal)
    {
        if(status != census_error::none || ordinal >= pending.size())
        { return fail(census_error::invalid_reference); }
        auto const& actual{pending[ordinal]}; // bound proved before actual owner access.
        if(!actual.exception || actual.wire == 0u || actual.wire > snapshot.objects.size())
        { return fail(census_error::invalid_reference); }
        auto const prior{find_exception(actual.exception)};
        if(status != census_error::none) { return false; }
        if(prior != 0u) { return prior == actual.wire || fail(census_error::invalid_reference); }
        if(reference_index.empty() || indexed_references >= reference_index.size() / 2u)
        { if(!grow_reference_index()) { return false; } }
        auto const key{reinterpret_cast<::std::uintptr_t>(actual.exception.get())};
        auto const mask{reference_index.size() - 1u};
        auto bucket{static_cast<::std::size_t>(reference_hash(reference_kind::wasm_exn, key)) & mask};
        for(::std::size_t probes{}; probes != reference_index.size(); ++probes)
        {
            auto& entry{reference_index[bucket]};
            if(entry.wire == 0u)
            {
                entry = {reference_kind::wasm_exn, key, actual.wire, ordinal};
                ++indexed_references; return true;
            }
            bucket = (bucket + 1u) & mask;
        }
        return fail(census_error::limit_exceeded);
    }
    [[nodiscard]] bool grow_reference_index()
    {
        constexpr auto host_limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
        auto const old{reference_index.size()};
        if(cap.max_objects > (::std::numeric_limits<::std::uint64_t>::max)() / 2u ||
            old > host_limit / sizeof(reference_index_entry) / 2u ||
            old > reference_index.max_size() / 2u) { return fail(census_error::limit_exceeded); }
        auto const next{old == 0u ? 2u : old * 2u}; // All factors proved BEFORE product.
        if(next > cap.max_objects * 2u || next > host_limit / sizeof(reference_index_entry) ||
            next > reference_index.max_size()) { return fail(census_error::limit_exceeded); }
        ::std::vector<reference_index_entry> replacement(next);
        auto const mask{next - 1u};
        for(auto const& entry : reference_index)
        {
            if(entry.wire == 0u) { continue; }
            auto bucket{static_cast<::std::size_t>(reference_hash(entry.kind, entry.key)) & mask};
            while(replacement[bucket].wire != 0u) { bucket = (bucket + 1u) & mask; }
            replacement[bucket] = entry; // Half occupancy guarantees an empty owned bucket.
        }
        reference_index.swap(replacement); return true;
    }
    [[nodiscard]] bool remember_reference(reference ref, census_object_id wire)
    {
        ::std::uintptr_t key{};
        if(status != census_error::none || !pointer_key(ref, key) || wire == 0u || wire > snapshot.objects.size())
        { return fail(census_error::invalid_reference); }
        auto const prior{find_reference(ref)};
        if(prior != 0u) { return prior == wire || fail(census_error::invalid_reference); }
        if(reference_index.empty() || indexed_references >= reference_index.size() / 2u)
        { if(!grow_reference_index()) { return false; } }
        auto const mask{reference_index.size() - 1u};
        auto bucket{static_cast<::std::size_t>(reference_hash(ref.kind, key)) & mask};
        for(::std::size_t probes{}; probes != reference_index.size(); ++probes)
        {
            auto& entry{reference_index[bucket]};
            if(entry.wire == 0u)
            { entry = {ref.kind, key, wire}; ++indexed_references; return true; }
            bucket = (bucket + 1u) & mask;
        }
        return fail(census_error::limit_exceeded);
    }
};
