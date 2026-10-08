// Include ONLY inside runtime_checkpoint_gc_state_borrow's private section.
// Called under the genuine execution/ONE cohort/closed host/N/publication
// guards. Native comparison keys stay in cold scratch; none reach the wire.
#pragma once
[[nodiscard]] static bool copy_complete_declared_type(complete_census_work& state,
    ::std::size_t owner, core_type type,
    ::uwvm2::uwvm::debugger::checkpoint::value_type& out) noexcept
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    out = {};
    if(owner >= state.actual.modules.size() || owner >= state.module_ids.size())
    { return state.fail(census_error::invalid_reference); }
    switch(type.kind)
    {
        case core_kind::i32: out.kind = cp::value_kind::i32; return true;
        case core_kind::i64: out.kind = cp::value_kind::i64; return true;
        case core_kind::f32: out.kind = cp::value_kind::f32; return true;
        case core_kind::f64: out.kind = cp::value_kind::f64; return true;
        case core_kind::v128: out.kind = cp::value_kind::v128; return true;
        case core_kind::reference: break;
        default: return state.fail(census_error::incompatible_type);
    }
    out.kind = cp::value_kind::reference; out.nullable = type.nullable;
    if(type.heap.is_defined())
    {
        auto const& pin{state.actual.modules[owner]};
        if(type.heap.code > 0xffffffffll ||
            type.heap.code >= pin.module->type_section_storage.type_section_count || state.module_ids[owner] == 0u)
        { return state.fail(census_error::incompatible_type); }
        out.heap = cp::heap_kind::defined; out.type_module = state.module_ids[owner];
        out.type_index = static_cast<::std::uint32_t>(type.heap.code); return true;
    }
    switch(static_cast<heap_kind>(type.heap.code))
    {
        case heap_kind::func: out.heap = cp::heap_kind::func; break;
        case heap_kind::extern_: out.heap = cp::heap_kind::external; break;
        case heap_kind::any: out.heap = cp::heap_kind::any; break;
        case heap_kind::eq: out.heap = cp::heap_kind::eq; break;
        case heap_kind::i31: out.heap = cp::heap_kind::i31; break;
        case heap_kind::struct_: out.heap = cp::heap_kind::structure; break;
        case heap_kind::array: out.heap = cp::heap_kind::array; break;
        case heap_kind::exn: out.heap = cp::heap_kind::exception; break;
        case heap_kind::nofunc: out.heap = cp::heap_kind::nofunc; break;
        case heap_kind::noextern: out.heap = cp::heap_kind::noextern; break;
        case heap_kind::none: out.heap = cp::heap_kind::bottom; break;
        case heap_kind::noexn: out.heap = cp::heap_kind::noexception; break;
        default: return state.fail(census_error::incompatible_type);
    }
    return true;
}
[[nodiscard]] static bool census_enqueue_reference(complete_census_work& state,
    ::std::size_t declared_owner, core_type type, reference ref, census_value& out)
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    if(declared_owner >= state.actual.modules.size() || !abstract_matches(ref, type))
    { return state.fail(census_error::incompatible_type); }
    if(ref.kind == reference_kind::wasm_null) { return true; }
    if(ref.kind == reference_kind::wasm_i31)
    { out.reference = cp::reference_kind::i31; out.low_bits = ref.storage.wasm_i31.get_u(); return true; }
    if(ref.storage.ptr == nullptr) { return state.fail(census_error::invalid_reference); }
    if(ref.kind == reference_kind::wasm_func_imported || ref.kind == reference_kind::wasm_func_defined)
    {
        ::std::size_t owner{}, index{};
        // Canonical actual record comparison BEFORE reference_type_matches can
        // inspect the record. A cold hash hit never replaces this proof.
        // The exact linked callback also supports legacy function-only type
        // arrays with no GC layout; it proves rich/legacy compatibility itself.
        bool member{};
        if(ref.kind == reference_kind::wasm_func_imported)
        {
            member = census_record_owner<0u, true>(state,
                static_cast<::uwvm2::uwvm::runtime::storage::imported_function_storage_t const*>(ref.storage.ptr), owner, index);
        }
        else
        {
            member = census_record_owner<0u, false>(state,
                static_cast<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const*>(ref.storage.ptr), owner, index);
            if(member)
            {
                auto const imports{state.actual.modules[owner].module->imported_function_vec_storage.size()};
                if(index > SIZE_MAX - imports) { return state.fail(census_error::invalid_reference); }
                index += imports; // Checked full module function-index formation.
            }
        }
        if(!member || owner >= state.function_ids.size() ||
            index >= state.function_ids[owner].size() ||
            (type.heap.is_defined() &&
                (state.actual.modules[declared_owner].store->function_type_match_ == nullptr ||
                 !state.actual.modules[declared_owner].store->function_type_match_(ref,
                    state.actual.modules[declared_owner].store.get(), static_cast<::std::uint_least32_t>(type.heap.code)))))
        { return state.fail(census_error::incompatible_type); }
        auto const wire{state.function_ids[owner][index]};
        if(wire == 0u || state.find_reference(ref) != wire || state.object(wire) == nullptr ||
            state.object(wire)->kind != census_object_kind::function)
        { return state.fail(census_error::invalid_reference); }
        out.reference = cp::reference_kind::function; out.target = wire; return true;
    }
    ::std::size_t owner{declared_owner}, length{};
    ::std::uint_least32_t type_index{};
    census_object_kind kind{};
    census_object_id tag{};
    ::uwvm2::runtime::exception::value_ref exception{};
    if(ref.kind == reference_kind::wasm_struct || ref.kind == reference_kind::wasm_array)
    {
        if(!aggregate_owner(state.actual, ref, owner, type_index, length) || owner >= state.module_ids.size())
        { return state.fail(census_error::invalid_reference); }
        auto const* layout{state.actual.modules[owner].store->checked_type(type_index,
            ref.kind == reference_kind::wasm_struct ? composite_kind::struct_ : composite_kind::array)};
        if(layout == nullptr || (ref.kind == reference_kind::wasm_struct && length != layout->field_count) ||
            (ref.kind == reference_kind::wasm_array && layout->field_count != 1u) ||
            (type.heap.is_defined() && !store_type::canonical_subtype(state.actual.modules[owner].store.get(), type_index,
                state.actual.modules[declared_owner].store.get(), static_cast<::std::uint_least32_t>(type.heap.code))))
        { return state.fail(census_error::incompatible_type); }
        kind = ref.kind == reference_kind::wasm_struct ? census_object_kind::structure : census_object_kind::array;
        out.reference = ref.kind == reference_kind::wasm_struct ? cp::reference_kind::structure : cp::reference_kind::array;
    }
    else if(ref.kind == reference_kind::wasm_exn)
    {
        if(!store_type::exn_token_shape(ref.storage.ptr)) { return state.fail(census_error::invalid_reference); }
        ::std::shared_ptr<store_type const> issuer{};
        {
            store_type::exn_guard guard{};
            auto const* registered{store_type::find_exn_locked(ref.storage.ptr)};
            if(registered == nullptr) { return state.fail(census_error::invalid_reference); }
            issuer = registered->owner.lock();
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
            exception = registered->value.native_untracked_owner();
#else
            exception = registered->value;
#endif
        }
        bool actual_issuer{};
        for(auto const& pin : state.actual.modules)
        { if(same_owner(issuer, pin.store)) { actual_issuer = true; break; } }
        if(!exception || !actual_issuer) { return state.fail(census_error::invalid_reference); }
        bool found{};
        for(::std::size_t id{}; id != state.actual.modules.size() && !found; ++id)
        {
            auto const& module{*state.actual.modules[id].module};
            auto const imported{module.imported_tag_vec_storage.size()};
            if(id >= state.tag_ids.size() || imported > state.tag_ids[id].size())
            { return state.fail(census_error::invalid_reference); }
            for(::std::size_t local{}; local != module.local_defined_tag_vec_storage.size(); ++local)
            {
                auto const& actual{module.local_defined_tag_vec_storage.index_unchecked(local)};
                if(actual.exception_identity.get() != exception->tag_identity()) { continue; }
                if(!actual.exception_identity || !declaration_origin(state.actual.modules[id]) ||
                    actual.type_index >= module.type_section_storage.type_section_count ||
                    local >= state.tag_ids[id].size() - imported)
                { return state.fail(census_error::incompatible_type); }
                auto const& types{module.type_section_storage};
                if(types.type_section_begin == nullptr || types.type_section_count >
                    static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*types.type_section_begin))
                { return state.fail(census_error::incompatible_type); }
                // [actual canonical type array ... tag_index<count] end
                // [safe] count/tag-index checked BEFORE forming +actual.type_index.
                if(actual.function_type_ptr != types.type_section_begin + actual.type_index)
                { return state.fail(census_error::incompatible_type); }
                owner = id; type_index = actual.type_index;
                tag = state.tag_ids[id][imported + local]; found = true; break;
            }
        }
        if(!found || tag == 0u) { return state.fail(census_error::invalid_reference); }
        length = exception->fields().size(); kind = census_object_kind::exception;
        out.reference = cp::reference_kind::exception;
    }
    else if(ref.kind == reference_kind::wasm_extern)
    {
        if(!store_type::bridge_token_shape(ref.storage.ptr))
        {
            // An opaque host carrier needs a separately authenticated adapter
            // and environment resource. It is never dereferenced, guessed from
            // an address or serialized as a native token by this Wasm producer.
            return state.fail(census_error::non_replayable_import);
        }
        store_type const* bridge_owner{};
        {
            store_type::bridge_guard guard{};
            auto const* actual{store_type::find_bridge_token_locked(ref.storage.ptr)};
            if(actual == nullptr) { return state.fail(census_error::invalid_reference); }
            bridge_owner = actual->owner;
        }
        bool found{};
        for(::std::size_t id{}; id != state.actual.modules.size(); ++id)
        { if(state.actual.modules[id].store.get() == bridge_owner) { owner = id; found = true; break; } }
        if(!found || static_cast<heap_kind>(type.heap.code) != heap_kind::extern_)
        { return state.fail(census_error::incompatible_type); }
        kind = census_object_kind::external; length = 1u; out.reference = cp::reference_kind::external;
    }
    else { return state.fail(census_error::incompatible_type); }
    // Every use was authenticated before this cold deduplication. Allocate the
    // shell BEFORE traversing fields so aliases and cyclic structs/arrays work.
    // Each catch_ref may issue a fresh token for the SAME immutable exception
    // instance. Authenticate that actual token above, then deduplicate the real
    // strong record/control block; token equality is not exception identity.
    auto const prior{kind == census_object_kind::exception ? state.find_exception(exception) : state.find_reference(ref)};
    if(state.status != census_error::none) { return false; }
    if(prior != 0u)
    {
        auto const* object{state.object(prior)};
        if(object == nullptr || object->kind != kind ||
            (kind == census_object_kind::exception && (object->links.empty() || object->links[0u] != tag)))
        { return state.fail(census_error::invalid_reference); }
        out.target = prior; return true;
    }
    if(state.pending.size() >= state.cap.max_objects || state.pending.size() >= state.pending.max_size() ||
        length > state.cap.max_values || length > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_value))
    { return state.fail(census_error::limit_exceeded); }
    auto const wire{state.allocate(kind)};
    if(wire == 0u || (kind != census_object_kind::exception && !state.remember_reference(ref, wire))) { return false; }
    if(kind == census_object_kind::structure || kind == census_object_kind::array)
    {
        if(!state.append_link(wire, state.module_ids[owner])) { return false; }
        state.object(wire)->words[0u] = type_index;
    }
    else if(kind == census_object_kind::exception)
    { if(!state.append_link(wire, tag)) { return false; } }
    auto const pending_ordinal{state.pending.size()};
    state.pending.push_back({ref, owner, wire, ::std::move(exception)});
    if(kind == census_object_kind::exception && !state.remember_exception(pending_ordinal)) { return false; }
    out.target = wire; return true;
}
[[nodiscard]] static bool copy_complete_native(complete_census_work& state,
    ::std::size_t owner, core_type type, ::std::byte const* actual, bool initialized, census_value& out)
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    out = {};
    if(!copy_complete_declared_type(state, owner, type, out.type)) { return false; }
    out.initialized = initialized;
    if(!initialized)
    {
        // Only a typed nondefaultable local can be uninitialized. In particular
        // this path must not read indeterminate native carrier bytes.
        return (!type.nullable && type.kind == core_kind::reference) || state.fail(census_error::incompatible_type);
    }
    if(actual == nullptr) { return state.fail(census_error::invalid_reference); }
    switch(type.kind)
    {
        case core_kind::i32: case core_kind::f32:
        {
            ::std::uint32_t bits{};
            // [actual fixed native16 carrier ... initialized bytes0..4] end
            // [safe] complete carrier owned by the lexical caller; memcpy
            // avoids alignment, aliasing and any floating point evaluation.
            ::std::memcpy(::std::addressof(bits), actual, sizeof(bits)); out.low_bits = bits; return true;
        }
        case core_kind::i64: case core_kind::f64:
            ::std::memcpy(::std::addressof(out.low_bits), actual, sizeof(out.low_bits)); return true;
        case core_kind::v128:
        {
            auto const* bytes{reinterpret_cast<unsigned char const*>(actual)};
            // [actual fixed native16 lane-byte carrier ... 8 ... 16] end
            // [safe] both width8 windows proved within16 BEFORE +8/+16.
            auto const low{::fast_io::parse_by_scan(bytes, bytes + 8u, ::fast_io::mnp::le_get<64u>(out.low_bits))};
            auto const high{::fast_io::parse_by_scan(bytes + 8u, bytes + 16u, ::fast_io::mnp::le_get<64u>(out.high_bits))};
            return (low.code == ::fast_io::parse_code::ok && low.iter == bytes + 8u &&
                high.code == ::fast_io::parse_code::ok && high.iter == bytes + 16u) || state.fail(census_error::invalid_shape);
        }
        case core_kind::reference:
        {
            reference ref{}; static_assert(sizeof(ref) <= 16u);
            ::std::memcpy(::std::addressof(ref), actual, sizeof(ref));
            return census_enqueue_reference(state, owner, type, ref, out);
        }
        default: return state.fail(census_error::incompatible_type);
    }
}
[[nodiscard]] static bool census_append_exception_trace(complete_census_work& state,
    census_object_id exception_wire, ::uwvm2::runtime::exception::diagnostic_trace_ref const& trace)
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    if(!trace) { return true; }
    auto const count{trace->frame_count()};
    if(count > state.cap.max_frames || state.values > state.cap.max_values ||
        count > state.cap.max_values - state.values || state.links > state.cap.max_links ||
        count > state.cap.max_links - state.links)
    { return state.fail(census_error::limit_exceeded); }
    auto const wire{state.allocate(census_object_kind::exception_trace)};
    if(wire == 0u || !state.append_link(exception_wire, wire)) { return false; }
    // Trace payload revision1 retains the actual origin INSTANCE/function
    // index and both original owned names, even for imported function aliases.
    // Original diagnostic generation is independently UNKNOWN (0); the linked
    // function node records current callable state, never old trace provenance.
    // Actual trace producers have no instruction PC: UINT64_MAX is explicitly
    // UNKNOWN, never an executable address or a manufactured offset zero.
    state.object(wire)->flags = trace->truncated() ? 1u : 0u;
    state.object(wire)->words[0u] = 1u;
    for(::std::size_t ordinal{}; ordinal != count; ++ordinal)
    {
        ::uwvm2::runtime::exception::diagnostic_frame_index identity{};
        ::uwvm2::runtime::exception::diagnostic_frame_names names{};
        if(!trace->frame_at(ordinal, identity, names) || identity.module_id >= state.actual.modules.size() ||
            identity.module_id >= state.function_ids.size() || identity.module_id >= state.instance_ids.size() ||
            state.actual.modules[identity.module_id].id != identity.module_id ||
            identity.function_index >= state.function_ids[identity.module_id].size())
        { return state.fail(census_error::invalid_reference); }
        auto const instance{state.instance_ids[identity.module_id]};
        auto const function{state.function_ids[identity.module_id][identity.function_index]};
        if(instance == 0u || function == 0u) { return state.fail(census_error::invalid_reference); }
        if(!state.append_link(wire, function)) { return false; }
        census_value position{}; position.type.kind = cp::value_kind::i64;
        position.low_bits = (::std::numeric_limits<::std::uint64_t>::max)();
        if(!state.append_value(wire, position)) { return false; }
        constexpr auto size_max{(::std::numeric_limits<::std::size_t>::max)()};
        if(names.module_name.size() > size_max - 40u || names.function_name.size() > size_max - 40u - names.module_name.size())
        { return state.fail(census_error::limit_exceeded); }
        auto const added{40u + names.module_name.size() + names.function_name.size()};
        auto const old{state.object(wire)->bytes.size()};
        if(added > size_max - old || old + added > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
            !state.charge_payload(added)) { return state.fail(census_error::limit_exceeded); }
        auto& payload{state.object(wire)->bytes};
        if(old + added > payload.max_size()) { return state.fail(census_error::limit_exceeded); }
        payload.resize(old + added);
        // [actual owned bytes ... old][40-byte header][module][function] end
        // [safe] old+added<=allocation/PTRDIFF and added>=40 BEFORE +old/+40.
        auto* first{reinterpret_cast<unsigned char*>(payload.data()) + old};
        ::fast_io::basic_obuffer_view<unsigned char> output{first, first + 40u};
        ::fast_io::print(output, ::fast_io::mnp::le_put<64u>(instance),
            ::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(identity.function_index)),
            ::fast_io::mnp::le_put<64u>(::std::uint64_t{}),
            ::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(names.module_name.size())),
            ::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(names.function_name.size())));
        if(!names.module_name.empty())
        { ::fast_io::freestanding::my_memcpy(first + 40u, names.module_name.data(), names.module_name.size()); }
        if(!names.function_name.empty())
        {
            // [40-byte header][module length proved][function] end
            // [safe] module/function extents proved by checked added BEFORE offset.
            ::fast_io::freestanding::my_memcpy(first + 40u + names.module_name.size(), names.function_name.data(), names.function_name.size());
        }
    }
    return true;
}
[[nodiscard]] static bool drain_complete_reference_graph(complete_census_work& state)
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    for(::std::size_t index{}; index != state.pending.size(); ++index)
    {
        // Copy the strong owner/key before recursive enqueue can reallocate the
        // pending and object vectors. No object& or iterator survives enqueue.
        auto const pending{state.pending[index]};
        if(pending.owner >= state.actual.modules.size()) { return state.fail(census_error::invalid_reference); }
        auto const& store{*state.actual.modules[pending.owner].store};
        if(pending.ref.kind == reference_kind::wasm_struct || pending.ref.kind == reference_kind::wasm_array)
        {
            auto const* object{state.object(pending.wire)};
            if(object == nullptr || object->words[0u] > 0xffffffffu) { return state.fail(census_error::invalid_shape); }
            auto const type_index{static_cast<::std::uint_least32_t>(object->words[0u])};
            auto const* layout{store.checked_type(type_index,
                pending.ref.kind == reference_kind::wasm_struct ? composite_kind::struct_ : composite_kind::array)};
            ::std::size_t owner{}, length{}; ::std::uint_least32_t current_type{};
            if(layout == nullptr || !aggregate_owner(state.actual, pending.ref, owner, current_type, length) ||
                owner != pending.owner || current_type != type_index ||
                (pending.ref.kind == reference_kind::wasm_struct ? length != layout->field_count : layout->field_count != 1u) ||
                (layout->field_count != 0u && layout->fields == nullptr) ||
                layout->field_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                    sizeof(::uwvm2::parser::wasm::standard::wasm3::type::field_type) ||
                state.values > state.cap.max_values || length > state.cap.max_values - state.values)
            { return state.fail(census_error::incompatible_type); }
            for(::std::size_t member{}; member != length; ++member)
            {
                auto const field{pending.ref.kind == reference_kind::wasm_struct ? member : 0u};
                if(field >= layout->field_count) { return state.fail(census_error::incompatible_type); }
                // [actual immutable fields ... field<count] end
                // [safe] full extent/index proof BEFORE field pointer indexing.
                auto const storage{layout->fields[field].storage}; object_value native{}; census_value value{};
                auto const result{pending.ref.kind == reference_kind::wasm_struct ?
                    store.struct_get(pending.ref, member, false, native) : store.array_get(pending.ref, member, false, native)};
                if(result != ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) { return state.fail(census_error::invalid_shape); }
                if(!copy_complete_native(state, pending.owner, storage.value, native.bits.data(), true, value)) { return false; }
                using packed = ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind;
                if(storage.packed != packed::none && storage.value.kind != core_kind::i32)
                { return state.fail(census_error::incompatible_type); }
                if(storage.packed == packed::i8) { value.type.kind = cp::value_kind::i8; value.low_bits &= 0xffu; }
                else if(storage.packed == packed::i16) { value.type.kind = cp::value_kind::i16; value.low_bits &= 0xffffu; }
                if(!state.append_value(pending.wire, value)) { return false; }
            }
        }
        else if(pending.ref.kind == reference_kind::wasm_extern)
        {
            reference inner{};
            {
                store_type::bridge_guard guard{};
                auto const* actual{store_type::find_bridge_token_locked(pending.ref.storage.ptr)};
                if(actual == nullptr || actual->owner != ::std::addressof(store)) { return state.fail(census_error::invalid_reference); }
                inner = actual->inner;
            }
            // Wrapper edges are nonnull internal any, never another wrapper,
            // exception/function or an unchecked opaque host pointer.
            if(inner.kind != reference_kind::wasm_i31 && inner.kind != reference_kind::wasm_struct && inner.kind != reference_kind::wasm_array)
            { return state.fail(census_error::non_replayable_import); }
            core_type type{}; type.kind = core_kind::reference; type.heap.code = static_cast<::std::int_least64_t>(heap_kind::any);
            type.nullable = false; object_value native{}; census_value value{};
            ::std::memcpy(native.bits.data(), ::std::addressof(inner), sizeof(inner));
            if(!copy_complete_native(state, pending.owner, type, native.bits.data(), true, value) ||
                !state.append_value(pending.wire, value)) { return false; }
        }
        else if(pending.ref.kind == reference_kind::wasm_exn)
        {
            if(!pending.exception) { return state.fail(census_error::invalid_reference); }
            auto const* object{state.object(pending.wire)};
            if(object == nullptr || object->links.size() != 1u) { return state.fail(census_error::invalid_shape); }
            auto const* tag{state.object(object->links[0u])};
            if(tag == nullptr || tag->kind != census_object_kind::tag || tag->words[0u] > 0xffffffffu)
            { return state.fail(census_error::invalid_reference); }
            auto const type_index{static_cast<::std::size_t>(tag->words[0u])};
            auto const& types{state.actual.modules[pending.owner].module->type_section_storage};
            auto const* signatures{types.owned_signature_begin};
            if(signatures == nullptr || types.owned_signature_end == nullptr || type_index >= types.type_section_count ||
                types.type_section_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*signatures))
            { return state.fail(census_error::incompatible_type); }
            auto const first{reinterpret_cast<::std::uintptr_t>(signatures)}, last{reinterpret_cast<::std::uintptr_t>(types.owned_signature_end)};
            if(last < first || types.type_section_count > ((::std::numeric_limits<::std::uintptr_t>::max)() - first) / sizeof(*signatures) ||
                last - first != types.type_section_count * sizeof(*signatures)) { return state.fail(census_error::incompatible_type); }
            // [actual full owned signatures ... type_index<count] end
            // [safe] byte extent/exact index proved BEFORE signature access.
            auto const& signature{signatures[type_index]};
            auto const fields{pending.exception->fields()};
            if(signature.type_index != type_index || !signature.results.empty() || signature.parameters.size() != fields.size() ||
                state.values > state.cap.max_values || fields.size() > state.cap.max_values - state.values)
            { return state.fail(census_error::incompatible_type); }
            for(::std::size_t member{}; member != fields.size(); ++member)
            {
                auto const declared{signature.parameters.index_unchecked(member)};
                auto const& field{fields[member]}; auto const bits{field.bits()};
                using payload = ::uwvm2::runtime::exception::payload_kind;
                auto const kind{field.kind()}; auto const width{::uwvm2::runtime::exception::payload_width(kind)};
                if(width == 0u || width > 16u || bits.size() != width ||
                    (declared.kind == core_kind::reference && kind != payload::wasm_reference) ||
                    (declared.kind == core_kind::i32 && kind != payload::i32) ||
                    (declared.kind == core_kind::i64 && kind != payload::i64) ||
                    (declared.kind == core_kind::f32 && kind != payload::f32) ||
                    (declared.kind == core_kind::f64 && kind != payload::f64) ||
                    (declared.kind == core_kind::v128 && kind != payload::v128))
                { return state.fail(census_error::incompatible_type); }
                object_value native{}; census_value value{};
                // [immutable owned payload width4/8/16 <= native16] end
                // [safe] exact source/destination width proved BEFORE copy.
                ::std::memcpy(native.bits.data(), bits.data(), width);
                if(!copy_complete_native(state, pending.owner, declared, native.bits.data(), true, value) ||
                    !state.append_value(pending.wire, value)) { return false; }
            }
            // Keep complete original diagnostic names/order and explicitly
            // unknown positions. Source-derived current names are no substitute
            // for the immutable trace carried by the actual exception value.
            if(!census_append_exception_trace(state, pending.wire, pending.exception->diagnostic())) { return false; }
        }
        else { return state.fail(census_error::incompatible_type); }
    }
    return state.status == census_error::none;
}
