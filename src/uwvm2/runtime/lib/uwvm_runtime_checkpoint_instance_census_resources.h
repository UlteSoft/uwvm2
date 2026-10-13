// Include ONLY after instance_census.h and the complete typed-reference methods,
// inside runtime_checkpoint_gc_state_borrow's private class section. No manager
// admission or portable-file DATA is converted into runtime ownership here.
#pragma once
using census_file_type = ::uwvm2::uwvm::wasm::type::wasm_file_t;
// Function/table/memory/global/tag index spaces; imports alias actual providers.
template<unsigned Resource, bool Imported>
[[nodiscard]] static auto const& census_records(module_type const& module) noexcept
{
    static_assert(Resource <= 4u);
    if constexpr(Resource == 0u && Imported) { return module.imported_function_vec_storage; }
    else if constexpr(Resource == 0u) { return module.local_defined_function_vec_storage; }
    else if constexpr(Resource == 1u && Imported) { return module.imported_table_vec_storage; }
    else if constexpr(Resource == 1u) { return module.local_defined_table_vec_storage; }
    else if constexpr(Resource == 2u && Imported) { return module.imported_memory_vec_storage; }
    else if constexpr(Resource == 2u) { return module.local_defined_memory_vec_storage; }
    else if constexpr(Resource == 3u && Imported) { return module.imported_global_vec_storage; }
    else if constexpr(Resource == 3u) { return module.local_defined_global_vec_storage; }
    else if constexpr(Imported) { return module.imported_tag_vec_storage; }
    else { return module.local_defined_tag_vec_storage; }
}
template<unsigned Resource>
[[nodiscard]] static auto& census_index_maps(complete_census_work& state) noexcept
{
    static_assert(Resource <= 4u);
    if constexpr(Resource == 0u) { return state.function_ids; }
    else if constexpr(Resource == 1u) { return state.table_ids; }
    else if constexpr(Resource == 2u) { return state.memory_ids; }
    else if constexpr(Resource == 3u) { return state.global_ids; }
    else { return state.tag_ids; }
}
template<unsigned Resource, bool Imported, typename Record>
[[nodiscard]] static bool census_record_owner(complete_census_work const& state,
    Record const* token, ::std::size_t& owner, ::std::size_t& index) noexcept
{
    if(token == nullptr) { return false; }
    auto const supplied{reinterpret_cast<::std::uintptr_t>(token)};
    for(::std::size_t id{}; id != state.actual.modules.size(); ++id)
    {
        auto const& entries{census_records<Resource, Imported>(*state.actual.modules[id].module)};
        auto const count{entries.size()};
        constexpr auto limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
        if(count == 0u || count > limit / sizeof(Record) || entries.data() == nullptr) { continue; }
        auto const base{reinterpret_cast<::std::uintptr_t>(entries.data())};
        auto const extent{count * sizeof(Record)}; // count<=PTRDIFF/sizeof BEFORE product.
        if(base > UINTPTR_MAX - extent || supplied < base || supplied - base >= extent ||
            (supplied - base) % sizeof(Record) != 0u) { continue; }
        owner = id; index = static_cast<::std::size_t>((supplied - base) / sizeof(Record));
        // [actual canonical owned records ... index<count] end
        // [safe] native token is only COMPARED; all reads use owned entries[index].
        return true;
    }
    return false;
}
template<unsigned Resource>
[[nodiscard]] static bool census_resolve_import(complete_census_work& state,
    ::std::size_t& owner, ::std::size_t& local, ::std::size_t imported_index,
    ::std::uint_least64_t epoch, ::std::size_t& terminal_import) noexcept
{
    terminal_import = SIZE_MAX;
    if(owner >= state.actual.modules.size()) { return state.fail(census_error::invalid_reference); }
    auto const& module{*state.actual.modules[owner].module};
    auto const& imported{census_records<Resource, true>(module)};
    if(imported_index >= imported.size()) { return state.fail(census_error::invalid_reference); }
    auto const* next{::std::addressof(imported.index_unchecked(imported_index))};
    for(::std::size_t hops{}; hops != state.actual.records; ++hops)
    {
        ::std::size_t record{};
        if(!census_record_owner<Resource, true>(state, next, owner, record))
        { return state.fail(census_error::invalid_reference); }
        auto const& actual{census_records<Resource, true>(*state.actual.modules[owner].module).index_unchecked(record)};
        using imported_type = ::std::remove_cvref_t<decltype(actual)>;
        if constexpr(Resource == 4u)
        {
            if(actual.defined_target != nullptr)
            {
                if(actual.imported_target != nullptr || actual.resolved_tag != actual.defined_target ||
                    !census_record_owner<4u, false>(state, actual.defined_target, owner, local))
                { return state.fail(census_error::invalid_reference); }
                return true;
            }
            if(actual.imported_target == nullptr || actual.resolved_tag == nullptr)
            { return state.fail(census_error::unavailable_capability); }
            ::std::size_t next_owner{}, next_index{};
            if(!census_record_owner<4u, true>(state, actual.imported_target, next_owner, next_index) ||
                census_records<4u, true>(*state.actual.modules[next_owner].module).index_unchecked(next_index).resolved_tag != actual.resolved_tag)
            { return state.fail(census_error::invalid_reference); }
            next = actual.imported_target; // COMPARE next membership again before any pointee read.
        }
        else
        {
            bool defined{}, chained{};
            if constexpr(Resource == 0u)
            {
                using kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                defined = actual.link_kind == kind::defined; chained = actual.link_kind == kind::imported;
            }
            else if constexpr(Resource == 1u)
            {
                using kind = typename imported_type::imported_table_link_kind;
                defined = actual.link_kind == kind::defined; chained = actual.link_kind == kind::imported;
            }
            else if constexpr(Resource == 2u)
            {
                using kind = typename imported_type::imported_memory_link_kind;
                defined = actual.link_kind == kind::defined; chained = actual.link_kind == kind::imported;
            }
            else
            {
                using kind = typename imported_type::imported_global_link_kind;
                defined = actual.link_kind == kind::defined; chained = actual.link_kind == kind::imported;
            }
            if(defined)
            {
                if(!census_record_owner<Resource, false>(state, actual.target.defined_ptr, owner, local))
                { return state.fail(census_error::invalid_reference); }
                return true;
            }
            if constexpr(Resource == 0u)
            {
                using kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                if(actual.link_kind == kind::local_imported)
                {
                    // This member was just located in the actual complete
                    // cohort/source registry. The loader witness checks its
                    // original canonical wrapper, final native-vector member,
                    // current source/validation epoch and known original ABI.
                    // Never call an arbitrary native vtable or guess by name.
                    auto const& pin{state.actual.modules[owner]};
                    ::uwvm2::uwvm::runtime::full::builtin_wasip1_function_data binding{};
                    if(!pin.source->actual_builtin_wasip1_function(static_cast<::std::size_t>(pin.id),epoch,
                        pin.module,record,binding)) { return state.fail(census_error::unavailable_capability); }
                    terminal_import = record; local = 0u; return true;
                }
            }
            // Unknown native/DL providers still require their own genuine
            // producer. Binding-only DATA supplies NO effects/restore adapter.
            if(!chained) { return state.fail(census_error::unavailable_capability); }
            next = actual.target.imported_ptr; // No dereference until next owned-membership proof.
        }
    }
    return state.fail(census_error::invalid_reference); // Complete bounded chain/cycle check.
}
template<unsigned Resource>
[[nodiscard]] static bool census_allocate_resource_map(complete_census_work& state)
{
    auto& maps{census_index_maps<Resource>(state)}; maps.resize(state.actual.modules.size());
    constexpr census_object_kind kinds[]{census_object_kind::function, census_object_kind::table,
        census_object_kind::memory, census_object_kind::global, census_object_kind::tag};
    for(::std::size_t owner{}; owner != state.actual.modules.size(); ++owner)
    {
        auto const& module{*state.actual.modules[owner].module};
        auto const imports{census_records<Resource, true>(module).size()};
        auto const locals{census_records<Resource, false>(module).size()};
        if(imports > UINT32_MAX || locals > UINT32_MAX - imports || imports > SIZE_MAX - locals ||
            imports + locals > record_limit || imports + locals > maps[owner].max_size() ||
            imports + locals > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_object_id))
        { return state.fail(census_error::limit_exceeded); }
        maps[owner].resize(imports + locals);
        for(::std::size_t local{}; local != locals; ++local)
        {
            auto const wire{state.allocate(kinds[Resource])}; if(wire == 0u) { return false; }
            maps[owner][imports + local] = wire; // sum<=complete owned map count, proved above.
        }
    }
    return true;
}
template<unsigned Resource>
[[nodiscard]] static bool census_bind_import_map(complete_census_work& state, ::std::uint_least64_t epoch)
{
    auto& maps{census_index_maps<Resource>(state)};
    for(::std::size_t declaring{}; declaring != state.actual.modules.size(); ++declaring)
    {
        auto const imports{census_records<Resource, true>(*state.actual.modules[declaring].module).size()};
        for(::std::size_t index{}; index != imports; ++index)
        {
            auto owner{declaring}; ::std::size_t local{}, terminal_import{SIZE_MAX};
            if(!census_resolve_import<Resource>(state, owner, local, index, epoch, terminal_import)) { return false; }
            if constexpr(Resource == 0u)
            {
                if(terminal_import != SIZE_MAX)
                {
                    // Exact terminal receiving instance/ordinal, not the known
                    // builtin index/signature, is this function's identity.
                    // Different terminal imports never share function objects;
                    // real Wasm alias chains reuse ONLY that actual terminal.
                    if(owner >= maps.size() || terminal_import >= maps[owner].size())
                    { return state.fail(census_error::invalid_reference); }
                    auto wire{maps[owner][terminal_import]};
                    if(wire == 0u)
                    {
                        wire = census_allocate_builtin_function(state,owner,terminal_import,epoch);
                        if(wire == 0u) { return false; }
                        maps[owner][terminal_import] = wire; // checked complete actual terminal map.
                    }
                    maps[declaring][index] = wire; continue;
                }
            }
            auto const provider_imports{census_records<Resource, true>(*state.actual.modules[owner].module).size()};
            if(local >= census_records<Resource, false>(*state.actual.modules[owner].module).size() ||
                provider_imports > SIZE_MAX - local || provider_imports + local >= maps[owner].size())
            { return state.fail(census_error::invalid_reference); }
            auto const wire{maps[owner][provider_imports + local]};
            if(wire == 0u) { return state.fail(census_error::invalid_reference); }
            maps[declaring][index] = wire; // Actual aliases reuse ONE provider object, never signature-only copies.
        }
    }
    return true;
}
[[nodiscard]] static bool census_file_slice(census_file_type const& file, void const* first,
    void const* last, ::std::size_t& offset, ::std::size_t& length) noexcept
{
    offset = length = 0u;
    if(!file.has_owned_source_image() || file.source_cbegin() == nullptr || file.source_size() < 8u ||
        file.source_size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())) { return false; }
    if(first == nullptr || last == nullptr) { return first == last; } // Exact parsed empty pair only; no read.
    auto const base{reinterpret_cast<::std::uintptr_t>(file.source_cbegin())};
    auto const begin{reinterpret_cast<::std::uintptr_t>(first)}, end{reinterpret_cast<::std::uintptr_t>(last)};
    if(file.source_size() > UINTPTR_MAX - base || begin < base || end < begin || end - base > file.source_size()) { return false; }
    offset = static_cast<::std::size_t>(begin - base); length = static_cast<::std::size_t>(end - begin);
    return offset <= file.source_size() && length <= file.source_size() - offset;
}
[[nodiscard]] static bool census_type_index(module_pin const& pin,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* token, ::std::uint32_t& index) noexcept
{
    auto const& types{pin.module->type_section_storage}; auto const count{types.type_section_count};
    using type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t;
    if(token == nullptr || types.type_section_begin == nullptr || count == 0u || count > UINT32_MAX ||
        count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(type)) { return false; }
    auto const base{reinterpret_cast<::std::uintptr_t>(types.type_section_begin)}, supplied{reinterpret_cast<::std::uintptr_t>(token)};
    auto const bytes{count * sizeof(type)};
    if(base > UINTPTR_MAX - bytes || supplied < base || supplied - base >= bytes || (supplied - base) % sizeof(type) != 0u) { return false; }
    index = static_cast<::std::uint32_t>((supplied - base) / sizeof(type)); return true;
}
template<typename Vector, typename Record>
[[nodiscard]] static Record const* census_owned_declaration(Vector const& owned, Record const* token) noexcept
{
    auto const count{owned.size()};
    if(token == nullptr || count == 0u || owned.data() == nullptr ||
        count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(Record)) { return nullptr; }
    auto const base{reinterpret_cast<::std::uintptr_t>(owned.data())}, supplied{reinterpret_cast<::std::uintptr_t>(token)};
    auto const extent{count * sizeof(Record)}; // Product bounded BEFORE addition/comparison.
    if(base > UINTPTR_MAX - extent || supplied < base || supplied - base >= extent ||
        (supplied - base) % sizeof(Record) != 0u) { return nullptr; }
    auto const index{static_cast<::std::size_t>((supplied-base)/sizeof(Record))};
    // [actual canonical parser/rewritten-owned vector ... index<count] end
    // [safe] unrelated token only compared, never dereferenced; ordinal bounded.
    return ::std::addressof(owned.index_unchecked(index));
}
template<::uwvm2::parser::wasm::concepts::wasm_feature... Features>
[[nodiscard]] static bool census_declaration_preflight(complete_census_work& state, ::std::size_t owner,
    census_file_type const& file,
    ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Features...> const& parsed)
{
    namespace f = ::uwvm2::parser::wasm::standard::wasm1::features;
    namespace op = ::uwvm2::parser::wasm::concepts::operation;
    auto const& pin{state.actual.modules[owner]}; auto const& module{*pin.module};
    if(!declared_module(pin, parsed)) { return state.fail(census_error::invalid_shape); }
    auto const& imports_section{op::get_first_type_in_tuple<f::import_section_storage_t<Features...>>(parsed.sections)};
    auto const& imported_functions{imports_section.importdesc.index_unchecked(0u)};
    if(imported_functions.size() != module.imported_function_vec_storage.size())
    { return state.fail(census_error::invalid_shape); }
    for(::std::size_t i{}; i != imported_functions.size(); ++i)
    {
        auto const* original{census_owned_declaration(imports_section.imports,imported_functions.index_unchecked(i))};
        auto const& actual{module.imported_function_vec_storage.index_unchecked(i)};
        auto const* active{actual.import_type_ptr == original ? original :
            census_owned_declaration(module.rewritten_import_vec_storage,actual.import_type_ptr)};
        using external = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
        // Original source declaration or real initializer-owned rewrite ONLY.
        // A configured name rewrite cannot change this exact original type.
        // Both declaration memberships precede each pointee/union read.
        ::std::uint32_t type{};
        if(original == nullptr || active == nullptr || original->imports.type != external::func || active->imports.type != external::func ||
            active->imports.storage.function != original->imports.storage.function ||
            !census_type_index(pin,original->imports.storage.function,type)) { return state.fail(census_error::invalid_shape); }
    }
    auto const& functions{op::get_first_type_in_tuple<f::function_section_storage_t>(parsed.sections)};
    auto const& codes{op::get_first_type_in_tuple<f::code_section_storage_t<Features...>>(parsed.sections)};
    auto const& memories{op::get_first_type_in_tuple<f::memory_section_storage_t<Features...>>(parsed.sections).memories};
    auto const& data{op::get_first_type_in_tuple<f::data_section_storage_t<Features...>>(parsed.sections).datas};
    auto const& elements{op::get_first_type_in_tuple<f::element_section_storage_t<Features...>>(parsed.sections).elems};
    auto const& tags{op::get_first_type_in_tuple<::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Features...>>(parsed.sections)};
    if(functions.funcs.size() != module.local_defined_function_vec_storage.size() || codes.codes.size() != functions.funcs.size() ||
        memories.size() != module.local_defined_memory_vec_storage.size() || data.size() != module.local_defined_data_vec_storage.size() ||
        elements.size() != module.local_defined_element_vec_storage.size() || tags.type_indices.size() != module.local_defined_tag_vec_storage.size())
    { return state.fail(census_error::invalid_shape); }
    for(::std::size_t i{}; i != functions.funcs.size(); ++i)
    {
        auto const& actual{module.local_defined_function_vec_storage.index_unchecked(i)}; ::std::uint32_t type{};
        if(actual.wasm_code_ptr != ::std::addressof(codes.codes.index_unchecked(i)) || !census_type_index(pin, actual.function_type_ptr, type) ||
            type != functions.funcs.index_unchecked(i)) { return state.fail(census_error::invalid_shape); }
        ::std::size_t offset{}, length{};
        if(!census_file_slice(file, actual.wasm_code_ptr->body.code_begin, actual.wasm_code_ptr->body.code_end, offset, length) || length == 0u)
        { return state.fail(census_error::invalid_shape); }
    }
    for(::std::size_t i{}; i != memories.size(); ++i)
    { if(module.local_defined_memory_vec_storage.index_unchecked(i).memory_type_ptr != ::std::addressof(memories.index_unchecked(i)))
      { return state.fail(census_error::invalid_shape); } }
    for(::std::size_t i{}; i != tags.type_indices.size(); ++i)
    {
        auto const& actual{module.local_defined_tag_vec_storage.index_unchecked(i)}; ::std::uint32_t type{};
        if(!actual.exception_identity || actual.type_index != tags.type_indices.index_unchecked(i) ||
            !census_type_index(pin, actual.function_type_ptr, type) || type != actual.type_index)
        { return state.fail(census_error::invalid_shape); }
    }
    for(::std::size_t i{}; i != data.size(); ++i)
    {
        auto const& declaration{data.index_unchecked(i)}; auto const& actual{module.local_defined_data_vec_storage.index_unchecked(i)};
        if(actual.data_type_ptr != ::std::addressof(declaration) ||
            actual.data.byte_begin != reinterpret_cast<::std::byte const*>(declaration.storage.segment.byte.begin) ||
            actual.data.byte_end != reinterpret_cast<::std::byte const*>(declaration.storage.segment.byte.end))
        { return state.fail(census_error::invalid_shape); }
        ::std::size_t offset{}, length{};
        if(!census_file_slice(file, actual.data.byte_begin, actual.data.byte_end, offset, length))
        { return state.fail(census_error::invalid_shape); }
    }
    for(::std::size_t i{}; i != elements.size(); ++i)
    { if(module.local_defined_element_vec_storage.index_unchecked(i).element_type_ptr != ::std::addressof(elements.index_unchecked(i)))
      { return state.fail(census_error::invalid_shape); } }
    return true;
}
[[nodiscard]] static census_object_id census_allocate_builtin_function(complete_census_work& state,
    ::std::size_t owner, ::std::size_t imported_index, ::std::uint_least64_t epoch)
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    if(owner >= state.actual.modules.size() || owner >= state.instance_ids.size())
    { (void)state.fail(census_error::invalid_reference); return 0u; }
    auto const& pin{state.actual.modules[owner]}; auto const& imports{pin.module->imported_function_vec_storage};
    if(imported_index >= imports.size()) { (void)state.fail(census_error::invalid_reference); return 0u; }
    auto const& actual{imports.index_unchecked(imported_index)};
    ::uwvm2::uwvm::runtime::full::builtin_wasip1_function_data binding{};
    if(!pin.source->actual_builtin_wasip1_function(static_cast<::std::size_t>(pin.id),epoch,pin.module,imported_index,binding) ||
        binding.function_index >= 128u || binding.parameter_count > 16u || binding.result_count > 16u ||
        actual.import_type_ptr == nullptr)
    { (void)state.fail(census_error::unavailable_capability); return 0u; }
    // Complete original/owned-rewrite import declaration preflight ran BEFORE
    // any resource map allocation. The active declaration is that genuine
    // typed member; membership below authenticates the canonical type pointer
    // BEFORE reading its payload. No native provider pointer is dereferenced.
    ::std::uint32_t type{};
    if(!census_type_index(pin,actual.import_type_ptr->imports.storage.function,type))
    { (void)state.fail(census_error::invalid_shape); return 0u; }
    auto const& signature{pin.module->type_section_storage.type_section_begin[type]};
    auto const matches{[](auto const& native, auto const& expected, unsigned count) noexcept
    {
        if(native.begin == nullptr || native.end == nullptr)
        { return native.begin == nullptr && native.end == nullptr && count == 0u; }
        using item = ::std::remove_cvref_t<decltype(*native.begin)>;
        auto const first{reinterpret_cast<::std::uintptr_t>(native.begin)}, last{reinterpret_cast<::std::uintptr_t>(native.end)};
        if(last < first || count > expected.size() || last-first != count*sizeof(item)) { return false; }
        for(::std::size_t i{}; i != count; ++i)
        {
            // [genuine canonical signature's owned numeric carriers0..count]
            // [safe] exact count<=16 checked BEFORE index; no external buffer.
            if(static_cast<unsigned>(native.begin[i]) != expected[i]) { return false; }
        }
        return true;
    }};
    if(!matches(signature.parameter,binding.parameters,binding.parameter_count) ||
        !matches(signature.result,binding.results,binding.result_count))
    { (void)state.fail(census_error::incompatible_type); return 0u; }
    ::std::array<unsigned char,cp::builtin_wasip1_binding_bytes> bytes{};
    // [fixed private108-byte owner] end
    // [safe] exact owner extent BEFORE one-past; output never outlives owner.
    ::fast_io::basic_obuffer_view<unsigned char> output{bytes.data(),bytes.data()+bytes.size()};
    constexpr unsigned char magic[]{'U','W','V','M','W','A','S','I','P','1','B','1'};
    // [owned12-byte magic] end
    // [safe] exact array extent BEFORE one-past; the bounded byte stream keeps
    // its unsigned-character type, and no text transcoding changes this format.
    ::fast_io::operations::write_all(output,magic,magic+sizeof(magic));
    ::fast_io::io::print(output,::fast_io::mnp::le_put<64u>(binding.function_index),
        ::fast_io::mnp::le_put<64u>(::std::uint64_t{1u}));
    // Header is12+8+8=28bytes; actual knowninterface hash is fixed32bytes.
    // [owned108 ... committed28][32 digest bytes] end
    // [safe] exact fixed extents BEFORE deriving endpoints/copying/advancing.
    ::fast_io::operations::write_all(output,reinterpret_cast<unsigned char const*>(binding.interface_sha256.data()),
        reinterpret_cast<unsigned char const*>(binding.interface_sha256.data())+binding.interface_sha256.size());
    ::fast_io::io::print(output,::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(binding.parameter_count)),
        ::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(binding.result_count)));
    // [owned typed16-byte parameters/results, each endpoint owner-sized] end
    // [safe] fixed array sizes bound BEFORE pointer additions; FastIO bounded
    // output advances within its original108-byte private owner only.
    ::fast_io::operations::write_all(output,binding.parameters.data(),binding.parameters.data()+binding.parameters.size());
    ::fast_io::operations::write_all(output,binding.results.data(),binding.results.data()+binding.results.size());
    if(output.curr_ptr != bytes.data()+bytes.size()) { (void)state.fail(census_error::invalid_shape); return 0u; }
    if(!state.charge_payload(bytes.size())) { return 0u; }
    auto const resource{state.allocate(census_object_kind::host_resource)}, function{state.allocate(census_object_kind::function)};
    if(resource == 0u || function == 0u) { return 0u; }
    // Reacquire after BOTH allocations: no object reference survives growth.
    auto* adapter{state.object(resource)}; adapter->flags=3u;
    adapter->words={cp::builtin_wasip1_binding_kind,1u,binding.function_index+1u,0u,0u,0u,0u,0u};
    adapter->bytes.resize(bytes.size());
    ::fast_io::freestanding::my_memcpy(adapter->bytes.data(),bytes.data(),bytes.size());
    if(!cp::state_details::valid_builtin_binding(*adapter)) { (void)state.fail(census_error::invalid_shape); return 0u; }
    if(!state.append_link(function,state.instance_ids[owner]) || !state.append_link(function,resource)) { return 0u; }
    state.object(function)->flags=1u; state.object(function)->words={imported_index,1u,type,0u,0u,0u,0u,0u};
    // Pure copied binding DATA: original receiver preserved, no host lifetime,
    // external effects/replay/restore callback or entry address is emitted.
    return function;
}
[[nodiscard]] bool census_copy_functions_and_tags(complete_census_work& state) const
{
    for(::std::size_t owner{}; owner != state.actual.modules.size(); ++owner)
    {
        auto const& pin{state.actual.modules[owner]}; auto const& module{*pin.module};
        if(pin.id >= g_runtime.modules.size()) { return state.fail(census_error::stale_generation); }
        auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(pin.id))};
        if(record.runtime_module != pin.module || record.llvm_jit_debug_full_entry_generations.size() != module.local_defined_function_vec_storage.size())
        { return state.fail(census_error::stale_generation); }
        auto const imports{module.imported_function_vec_storage.size()};
        for(::std::size_t local{}; local != module.local_defined_function_vec_storage.size(); ++local)
        {
            auto const& actual{module.local_defined_function_vec_storage.index_unchecked(local)}; ::std::uint32_t type{};
            auto const generation{record.llvm_jit_debug_full_entry_generations[local]};
            auto const plan{checkpoint_current_generation_plan(record, local, generation)};
            if(!plan || !same_owner(plan->get().profile, profile_) || !census_type_index(pin, actual.function_type_ptr, type))
            { return state.fail(census_error::stale_generation); }
            if(state.function_plan_pins.size() >= state.function_plan_pins.max_size() ||
                state.function_plan_pins.size() >= record_limit || state.function_plan_pins.size() >=
                static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_function_plan_pin))
            { return state.fail(census_error::limit_exceeded); }
            state.function_plan_pins.push_back({owner, local, plan}); // Actual sealed lifetime owner, no entry address.
            auto const wire{state.function_ids[owner][imports + local]};
            if(!state.append_link(wire, state.instance_ids[owner])) { return false; }
            state.object(wire)->words = {imports + local, generation, type, 0u, 0u, 0u, 0u, 0u};
            if(generation != 1u)
            {
                // The O(1) resolver above authenticated the actual committed
                // record-owned transaction, engine/source/epoch/plan/current targets.
                if(local >= record.llvm_jit_checkpoint_generation_owners.size())
                { return state.fail(census_error::stale_generation); }
                auto const* transaction{record.llvm_jit_checkpoint_generation_owners[local]};
                if(transaction == nullptr || transaction->section_bytes.empty() ||
                    transaction->section_bytes.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
                { return state.fail(census_error::stale_generation); }
                auto const base{reinterpret_cast<::std::uintptr_t>(transaction->section_bytes.data())};
                auto const first{reinterpret_cast<::std::uintptr_t>(transaction->code.body.code_begin)};
                auto const last{reinterpret_cast<::std::uintptr_t>(transaction->code.body.code_end)};
                if(transaction->section_bytes.size() > UINTPTR_MAX - base || first < base || last <= first ||
                    last - base > transaction->section_bytes.size()) { return state.fail(census_error::invalid_shape); }
                auto const offset{static_cast<::std::size_t>(first - base)}, length{static_cast<::std::size_t>(last - first)};
                if(!state.charge_payload(length)) { return false; }
                auto& bytes{state.object(wire)->bytes}; bytes.resize(length);
                // [actual committed section owner ... offset][exact local-decls+body] end
                // [safe] offset+length<=owned extent and PTRDIFF proved BEFORE +offset.
                ::fast_io::freestanding::my_memcpy(bytes.data(), transaction->section_bytes.data() + offset, length);
            }
            reference ref{}; ref.kind = reference_kind::wasm_func_defined;
            ref.storage.ptr = const_cast<void*>(static_cast<void const*>(::std::addressof(actual)));
            if(!state.remember_reference(ref, wire)) { return false; }
        }
        for(::std::size_t index{}; index != imports; ++index)
        {
            reference ref{}; ref.kind = reference_kind::wasm_func_imported;
            ref.storage.ptr = const_cast<void*>(static_cast<void const*>(::std::addressof(module.imported_function_vec_storage.index_unchecked(index))));
            if(!state.remember_reference(ref, state.function_ids[owner][index])) { return false; }
        }
        auto const tag_imports{module.imported_tag_vec_storage.size()};
        for(::std::size_t local{}; local != module.local_defined_tag_vec_storage.size(); ++local)
        {
            auto const& actual{module.local_defined_tag_vec_storage.index_unchecked(local)};
            auto const wire{state.tag_ids[owner][tag_imports + local]};
            if(!actual.exception_identity || actual.type_index > UINT32_MAX || !state.append_link(wire, state.module_ids[owner]))
            { return state.fail(census_error::invalid_shape); }
            state.object(wire)->words[0u] = actual.type_index;
        }
    }
    return true;
}
struct census_allocator_quiescence
{
    ::std::atomic_flag* flag{};
    explicit census_allocator_quiescence(::std::atomic_flag* actual) noexcept
    { if(actual != nullptr && !actual->test_and_set(::std::memory_order_acquire)) { flag = actual; } }
    census_allocator_quiescence(census_allocator_quiescence const&) = delete;
    census_allocator_quiescence& operator=(census_allocator_quiescence const&) = delete;
    ~census_allocator_quiescence()
    {
        if(flag != nullptr)
        {
            flag->clear(::std::memory_order_release);
#if __cpp_lib_atomic_wait >= 201907L
            flag->notify_all();
#endif
        }
    }
};
[[nodiscard]] static bool census_copy_memory_extent(complete_census_work& state, census_object_id wire,
    ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t const& actual,
    ::std::byte const* base, ::std::size_t length)
{
    auto const& declaration{*actual.memory_type_ptr};
    if(actual.memory.custom_page_size_log2 != 16u || length % 65536u != 0u ||
        length > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
        (length != 0u && base == nullptr) || length > UINTPTR_MAX - reinterpret_cast<::std::uintptr_t>(base))
    { return state.fail(census_error::invalid_shape); }
    auto const pages{length / 65536u};
    // The actual initializer's configured limits replace the declaration,
    // including accepted warned widening. Preserve that exact effective policy.
    auto const saved_bounds{::uwvm2::uwvm::runtime::storage::checkpoint_effective_memory_bounds(
        declaration.address64, actual.effective_limits)};
    auto const minimum{saved_bounds.minimum}, effective_max{saved_bounds.maximum};
    if(pages < minimum || pages > effective_max || (declaration.shared && !declaration.limits.present_max))
    { return state.fail(census_error::invalid_shape); }
    auto* memory{state.object(wire)}; if(memory == nullptr) { return false; }
    memory->flags = declaration.shared ? 1u : 0u;
    memory->words = {declaration.address64 ? 64u : 32u, pages, minimum, effective_max, 0u, 0u, 0u, 0u};
    // Scan the ENTIRE committed allocation, retaining every nonzero page. This
    // is a complete sparse snapshot, not a memory display window or quota truncation.
    for(::std::size_t offset{}; offset != length; offset += 65536u)
    {
        bool nonzero{};
        for(::std::size_t i{}; i != 65536u; ++i)
        {
            // [actual committed immutable-at-stop base: offset+i<length] end
            // [safe] length%65536==0, offset<length and base extent proved above.
            if(base[offset + i] != ::std::byte{}) { nonzero = true; break; }
        }
        if(!nonzero) { continue; } // Omitted pages are explicitly all-zero DATA.
        if(!state.charge_payload(65536u)) { return false; }
        auto const chunk{state.allocate(census_object_kind::memory_chunk)};
        if(chunk == 0u || !state.append_link(chunk, wire)) { return false; }
        auto* item{state.object(chunk)}; item->words[0u] = offset; item->bytes.resize(65536u);
        // [same actual committed owner ... offset][exact64KiB] ... end
        // [safe] complete page proved before deriving base+offset; no guard hot path.
        ::fast_io::freestanding::my_memcpy(item->bytes.data(), base + offset, 65536u);
    }
    return true;
}
template<typename Memory>
[[nodiscard]] static bool census_copy_memory(complete_census_work& state, census_object_id wire,
    ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t const& actual, Memory const& memory)
{
    if constexpr(Memory::can_mmap)
    {
        if(memory.memory_length_p == nullptr) { return state.fail(census_error::invalid_shape); }
        // Stable mmap base; actual hostclose+allpark exclude writes/grow. No
        // software access guard is added to generated memory instructions.
        return census_copy_memory_extent(state, wire, actual, memory.memory_begin,
            memory.memory_length_p->load(::std::memory_order_acquire));
    }
    else if constexpr(Memory::support_multi_thread)
    {
        if(memory.growing_flag_p == nullptr || memory.active_ops_p == nullptr)
        { return state.fail(census_error::invalid_shape); }
        census_allocator_quiescence guard{memory.growing_flag_p};
        // Never wait while the ONE stopped-cohort mutex is held: a retained
        // operation might need that cohort to resume. Return a cold retry instead.
        if(guard.flag == nullptr || memory.active_ops_p->load(::std::memory_order_acquire) != 0u)
        { return state.fail(census_error::unavailable_capability); }
        return census_copy_memory_extent(state, wire, actual, memory.memory_begin, memory.memory_length);
    }
    else
    { return census_copy_memory_extent(state, wire, actual, memory.memory_begin, memory.memory_length); }
}
[[nodiscard]] static bool census_reference_declaration(auto const& declaration, core_type& type) noexcept
{
    type = {}; type.kind = core_kind::reference; type.nullable = true;
    if(declaration.has_core_type) { type = declaration.core_type; return type.kind == core_kind::reference; }
    switch(static_cast<unsigned>(declaration.reftype))
    {
        case 0x70u: type.heap.code = static_cast<::std::int_least64_t>(heap_kind::func); return true;
        case 0x6fu: type.heap.code = static_cast<::std::int_least64_t>(heap_kind::extern_); return true;
        case 0x69u: type.heap.code = static_cast<::std::int_least64_t>(heap_kind::exn); return true;
        default: return false;
    }
}
[[nodiscard]] bool census_copy_global(complete_census_work& state, ::std::size_t owner,
    ::std::size_t local, census_object_id wire) const
{
    auto const& module{*state.actual.modules[owner].module}; auto const& actual{module.local_defined_global_vec_storage.index_unchecked(local)};
    auto const type{::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(*actual.global_type_ptr)};
    object_value native{}; using kind = ::uwvm2::object::global::global_type;
    switch(type.kind)
    {
        case core_kind::i32:
            if(actual.global.kind != kind::wasm_i32) { return state.fail(census_error::incompatible_type); }
            ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(actual.global.storage.i32), 4u); break;
        case core_kind::i64:
            if(actual.global.kind != kind::wasm_i64) { return state.fail(census_error::incompatible_type); }
            ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(actual.global.storage.i64), 8u); break;
        case core_kind::f32:
            if(actual.global.kind != kind::wasm_f32) { return state.fail(census_error::incompatible_type); }
            ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(actual.global.storage.f32), 4u); break;
        case core_kind::f64:
            if(actual.global.kind != kind::wasm_f64) { return state.fail(census_error::incompatible_type); }
            ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(actual.global.storage.f64), 8u); break;
        case core_kind::v128:
            if(actual.global.kind != kind::wasm_v128) { return state.fail(census_error::incompatible_type); }
            ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(actual.global.storage.v128), 16u); break;
        case core_kind::reference:
            if(actual.global.kind != kind::wasm_ref || actual.global.ref_lease_store != module.gc_store.get())
            { return state.fail(census_error::incompatible_type); }
            static_assert(sizeof(reference) <= 16u);
            ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(actual.global.storage.ref), sizeof(reference)); break;
        default: return state.fail(census_error::incompatible_type);
    }
    census_value value{};
    if(!copy_complete_native(state, owner, type, native.bits.data(), true, value) || !state.append_value(wire, value)) { return false; }
    state.object(wire)->flags = actual.global.is_mutable ? 1u : 0u; return true;
}
[[nodiscard]] bool census_copy_table(complete_census_work& state, ::std::size_t owner,
    ::std::size_t local, census_object_id wire) const
{
    auto const& actual{state.actual.modules[owner].module->local_defined_table_vec_storage.index_unchecked(local)};
    auto const& declaration{*actual.table_type_ptr}; auto const count{actual.elems.size()}; core_type type{};
    if(!census_reference_declaration(declaration, type) || count < declaration.limits.min ||
        (declaration.limits.present_max && count > declaration.limits.max)) { return state.fail(census_error::invalid_shape); }
    auto* table{state.object(wire)}; if(table == nullptr) { return false; }
    table->words = {declaration.address64 ? 64u : 32u, count, declaration.limits.min, declaration.limits.max, 0u, 0u, 0u, 0u};
    census_value default_value{};
    if(count == 0u)
    {
        // Empty nonnullable tables have a TYPE, no runtime default VALUE/root.
        // Only schema4's exact zero-table descriptor permits this zero carrier.
        if(!copy_complete_declared_type(state, owner, type, default_value.type)) { return false; }
        default_value.initialized = false; return state.append_value(wire, default_value);
    }
    auto copy_slot = [&](::std::size_t index, census_value& value)
    {
        auto const& slot{actual.elems.index_unchecked(index)};
        if(static_cast<unsigned>(slot.type) > static_cast<unsigned>(::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::gc_array_ref))
        { return state.fail(census_error::invalid_reference); }
        auto const ref{::uwvm2::uwvm::runtime::storage::runtime_table_slot_to_gc_reference(slot)}; object_value native{};
        ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(ref), sizeof(reference));
        return copy_complete_native(state, owner, type, native.bits.data(), true, value);
    };
    if(!copy_slot(0u, default_value) || !state.append_value(wire, default_value)) { return false; }
    ::std::vector<census_value> run{}; ::std::size_t first{};
    auto flush = [&]()
    {
        if(run.empty()) { return true; }
        auto const chunk{state.allocate(census_object_kind::table_chunk)};
        if(chunk == 0u || !state.append_link(chunk, wire)) { return false; }
        auto* item{state.object(chunk)}; item->words[0u] = first; item->values = ::std::move(run);
        run = {}; return true; // Values were charged BEFORE each pending run push.
    };
    for(::std::size_t index{1u}; index != count; ++index)
    {
        census_value value{}; if(!copy_slot(index, value)) { return false; }
        if(value == default_value) { if(!flush()) { return false; } continue; }
        if(run.empty()) { first = index; }
        if(!state.charge_values(1u)) { return false; }
        run.push_back(value);
        if(run.size() == 4096u && !flush()) { return false; }
    }
    return flush();
}
[[nodiscard]] static bool census_copy_data(complete_census_work& state, ::std::size_t owner,
    ::std::size_t local, census_object_id wire, census_file_type const& file)
{
    auto const& actual{state.actual.modules[owner].module->local_defined_data_vec_storage.index_unchecked(local).data};
    ::std::size_t offset{}, length{};
    if(!census_file_slice(file, actual.byte_begin, actual.byte_end, offset, length))
    { return state.fail(census_error::invalid_shape); }
    bool const dropped{::uwvm2::uwvm::runtime::storage::wasm_data_segment_is_dropped(actual)};
    if(!dropped && actual.kind != ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::passive)
    { return state.fail(census_error::invalid_shape); } // Instantiation already completed all active segments.
    if(dropped) { state.object(wire)->flags = 1u; return true; }
    if(!state.charge_payload(length)) { return false; }
    auto& bytes{state.object(wire)->bytes}; bytes.resize(length);
    if(length != 0u)
    {
        // [actual exclusively owned original source ... offset][exact payload] end
        // [safe] offset+length<=source_size, PTRDIFF and ownership proved BEFORE +.
        auto const* base{reinterpret_cast<::std::byte const*>(file.source_cbegin())};
        ::fast_io::freestanding::my_memcpy(bytes.data(), base + offset, length);
    }
    return true;
}
[[nodiscard]] bool census_copy_element(complete_census_work& state, ::std::size_t owner,
    ::std::size_t local, census_object_id wire) const
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    auto const& module{*state.actual.modules[owner].module};
    auto const& actual{module.local_defined_element_vec_storage.index_unchecked(local)};
    // element_type_ptr was compared to the actual immutable parser record in
    // preflight before reading any declaration or native payload pointer.
    auto const& declaration{actual.element_type_ptr->storage.segment}; core_type type{};
    if(!census_reference_declaration(declaration, type)) { return state.fail(census_error::incompatible_type); }
    bool const dropped{storage::wasm_element_segment_is_dropped(actual.element)};
    if(dropped) { state.object(wire)->flags = 1u; return true; }
    if(actual.element.kind != storage::wasm_element_segment_kind::passive || declaration.active || declaration.declarative)
    { return state.fail(census_error::invalid_shape); }
    auto const payload{storage::load_wasm_element_segment_payload(actual.element)};
    auto const forms{unsigned(payload.funcidx_begin != nullptr) + unsigned(payload.funcref_begin != nullptr) +
        unsigned(payload.gc_ref_begin != nullptr) + unsigned(payload.externref_begin != nullptr)};
    if(forms > 1u || ((payload.funcidx_begin == nullptr) != (payload.funcidx_end == nullptr)) ||
        ((payload.funcref_begin == nullptr) != (payload.funcref_end == nullptr)) ||
        ((payload.gc_ref_begin == nullptr) != (payload.gc_ref_end == nullptr)) ||
        ((payload.externref_begin == nullptr) != (payload.externref_end == nullptr)))
    { return state.fail(census_error::invalid_shape); }
    ::std::size_t first{}, count{};
    enum class form { empty, index, function, gc, opaque } representation{form::empty};
    if(payload.funcidx_begin != nullptr)
    {
        representation = form::index;
        if(!storage::gc_static_root_details::payload_slice(declaration.vec_funcidx,
            payload.funcidx_begin, payload.funcidx_end, first, count) || first != 0u || count != declaration.vec_funcidx.size())
        { return state.fail(census_error::invalid_shape); }
    }
    else if(payload.funcref_begin != nullptr)
    {
        representation = form::function;
        if(!storage::gc_static_root_details::payload_slice(module.element_expr_funcref_vec_storage,
            payload.funcref_begin, payload.funcref_end, first, count) || count != declaration.vec_expr.size())
        { return state.fail(census_error::invalid_shape); }
    }
    else if(payload.gc_ref_begin != nullptr)
    {
        representation = form::gc;
        if(!storage::gc_static_root_details::payload_slice(module.element_expr_gc_ref_vec_storage,
            payload.gc_ref_begin, payload.gc_ref_end, first, count) || count != declaration.vec_expr.size())
        { return state.fail(census_error::invalid_shape); }
    }
    else if(payload.externref_begin != nullptr)
    {
        representation = form::opaque;
        if(!storage::gc_static_root_details::payload_slice(module.element_expr_externref_vec_storage,
            payload.externref_begin, payload.externref_end, first, count) || count != declaration.vec_expr.size())
        { return state.fail(census_error::invalid_shape); }
    }
    else if(!declaration.vec_expr.empty() || !declaration.vec_funcidx.empty())
    { return state.fail(census_error::invalid_shape); }
    if(state.values > state.cap.max_values || count > state.cap.max_values - state.values)
    { return state.fail(census_error::limit_exceeded); }
    for(::std::size_t index{}; index != count; ++index)
    {
        reference ref{};
        switch(representation)
        {
            case form::index:
            {
                auto const function{declaration.vec_funcidx.index_unchecked(index)};
                auto const imports{module.imported_function_vec_storage.size()};
                if(function >= state.function_ids[owner].size()) { return state.fail(census_error::invalid_reference); }
                if(function < imports)
                { ref.kind = reference_kind::wasm_func_imported;
                  ref.storage.ptr = const_cast<void*>(static_cast<void const*>(::std::addressof(module.imported_function_vec_storage.index_unchecked(function)))); }
                else
                { ref.kind = reference_kind::wasm_func_defined;
                  ref.storage.ptr = const_cast<void*>(static_cast<void const*>(::std::addressof(module.local_defined_function_vec_storage.index_unchecked(function - imports)))); }
                break;
            }
            case form::function:
            {
                auto const& slot{module.element_expr_funcref_vec_storage.index_unchecked(first + index)};
                if(static_cast<unsigned>(slot.type) > static_cast<unsigned>(storage::local_defined_table_elem_storage_type_t::gc_array_ref))
                { return state.fail(census_error::invalid_reference); }
                ref = storage::runtime_table_slot_to_gc_reference(slot); break;
            }
            case form::gc: ref = module.element_expr_gc_ref_vec_storage.index_unchecked(first + index); break;
            case form::opaque:
                ref.storage.ptr = module.element_expr_externref_vec_storage.index_unchecked(first + index);
                // Opaque native carrier is passed only as a comparison key to
                // the private actual bridge/exn registry, never dereferenced here.
                ref.kind = ref.storage.ptr == nullptr ? reference_kind::wasm_null :
                    (type.heap.code == static_cast<::std::int_least64_t>(heap_kind::exn) ? reference_kind::wasm_exn : reference_kind::wasm_extern); break;
            default: return state.fail(census_error::invalid_shape);
        }
        // [canonical actual owned payload: first+index within proved slice] end
        // [safe] all access is reconstructed through the owner vector; supplied
        // begin/end tokens never serve as dereference or pointer-arithmetic bases.
        object_value native{}; ::fast_io::freestanding::my_memcpy(native.bits.data(), ::std::addressof(ref), sizeof(reference));
        census_value value{};
        if(!copy_complete_native(state, owner, type, native.bits.data(), true, value) || !state.append_value(wire, value)) { return false; }
    }
    return true;
}
[[nodiscard]] bool copy_complete_instance_resources(complete_census_work& state) const
{
    if(!current_scope() || !state.snapshot.objects.empty() || !state.snapshot.root_instances.empty() ||
        !state.snapshot.retained_roots.empty() || !state.actual.modules.empty() || state.actual.records != 0u ||
        state.links != 0u || state.values != 0u || state.payload != 0u || state.wire_bytes != 176u ||
        !state.module_ids.empty() || !state.instance_ids.empty() || !state.pending.empty() ||
        !state.function_plan_pins.empty() || !state.reference_index.empty() || state.indexed_references != 0u ||
        !state.function_ids.empty() || !state.table_ids.empty() || !state.memory_ids.empty() || !state.global_ids.empty() ||
        !state.tag_ids.empty() || !state.data_ids.empty() || !state.element_ids.empty() || state.status != census_error::none)
    { return state.fail(census_error::unavailable_capability); }
    if(!complete_modules(state.actual, profile_, epoch_))
    { return state.fail(census_error::unavailable_capability); }
    auto const count{state.actual.modules.size()};
    ::std::vector<census_file_type const*> files{}; files.reserve(count);
    // Check ALL actual source/type/resource origins BEFORE copying any bytes.
    for(::std::size_t owner{}; owner != count; ++owner)
    {
        auto const& pin{state.actual.modules[owner]};
        auto const* file{pin.source->actual_validated_file(static_cast<::std::size_t>(pin.id), epoch_, pin.module)};
        if(file == nullptr || file->binfmt_ver != 1u || !file->has_owned_source_image() || file->source_size() < 8u ||
            !census_declaration_preflight(state, owner, *file, file->wasm_module_storage.wasm_binfmt_ver1_storage))
        { return state.fail(census_error::unavailable_capability); }
        auto const& module{*pin.module};
        if(!charge(module.imported_memory_vec_storage.size(), state.actual.records) ||
            !charge(module.local_defined_memory_vec_storage.size(), state.actual.records) ||
            !charge(module.local_defined_data_vec_storage.size(), state.actual.records) ||
            !charge(module.local_defined_element_vec_storage.size(), state.actual.records))
        { return state.fail(census_error::limit_exceeded); }
        files.push_back(file); // Genuine source owner remains in actual.modules, never an external token.
    }
    if(!current_scope()) { return state.fail(census_error::stale_generation); }
    state.module_ids.resize(count); state.instance_ids.resize(count);
    state.data_ids.resize(count); state.element_ids.resize(count);
    for(::std::size_t owner{}; owner != count; ++owner)
    {
        auto const module_wire{state.allocate(census_object_kind::module)};
        auto const instance_wire{state.allocate(census_object_kind::instance)};
        if(module_wire == 0u || instance_wire == 0u) { return false; }
        state.module_ids[owner] = module_wire; state.instance_ids[owner] = instance_wire;
        auto const& file{*files[owner]}; auto const bytes{file.source_size()};
        auto const policy{::uwvm2::runtime::checkpoint::module_feature_policy::from_actual_parameter(
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(file.wasm_parameter.binfmt1_para))};
        if(!policy.valid()) { return state.fail(census_error::invalid_shape); }
        // Exact immutable WF policy, after actual source/epoch/member proof.
        // Module flags1 records this policy; legacy flags0 is detached DATA only.
        state.object(module_wire)->flags = 1u;
        state.object(module_wire)->words = {policy.revision, policy.cli_mode, policy.disabled, policy.controlled, 0u, 0u, 0u, 0u};
        state.snapshot.required_features |= policy.compatibility_mask();
        if(bytes > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
            bytes > UINTPTR_MAX - reinterpret_cast<::std::uintptr_t>(file.source_cbegin()) || !state.charge_payload(bytes))
        { return state.fail(census_error::limit_exceeded); }
        auto& original{state.object(module_wire)->bytes}; original.resize(bytes);
        // [actual exclusively owned immutable ORIGINAL module, exact bytes] end
        // [safe] actual validated-member owner, quota/PTRDIFF extent BEFORE copy.
        ::fast_io::freestanding::my_memcpy(original.data(), file.source_cbegin(), bytes);
        if(!state.charge_links(1u)) { return false; }
        state.snapshot.root_instances.push_back(instance_wire);
        auto const& module{*state.actual.modules[owner].module};
        for(auto kind : {census_object_kind::data, census_object_kind::element})
        {
            auto& map{kind == census_object_kind::data ? state.data_ids[owner] : state.element_ids[owner]};
            auto const records{kind == census_object_kind::data ? module.local_defined_data_vec_storage.size() : module.local_defined_element_vec_storage.size()};
            if(records > UINT32_MAX || records > map.max_size() || records >
                static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(census_object_id)) { return state.fail(census_error::limit_exceeded); }
            map.resize(records);
            for(::std::size_t index{}; index != records; ++index)
            { map[index] = state.allocate(kind); if(map[index] == 0u) { return false; } }
        }
    }
    if(!census_allocate_resource_map<0u>(state) || !census_allocate_resource_map<1u>(state) ||
        !census_allocate_resource_map<2u>(state) || !census_allocate_resource_map<3u>(state) ||
        !census_allocate_resource_map<4u>(state) || !census_bind_import_map<0u>(state,epoch_) ||
        !census_bind_import_map<1u>(state,epoch_) || !census_bind_import_map<2u>(state,epoch_) ||
        !census_bind_import_map<3u>(state,epoch_) || !census_bind_import_map<4u>(state,epoch_)) { return false; }
    for(::std::size_t owner{}; owner != count; ++owner)
    {
        auto const instance{state.instance_ids[owner]};
        if(!state.append_link(instance, state.module_ids[owner])) { return false; }
        ::std::array<::std::vector<census_object_id> const*, 7u> maps{::std::addressof(state.function_ids[owner]),
            ::std::addressof(state.table_ids[owner]), ::std::addressof(state.memory_ids[owner]), ::std::addressof(state.global_ids[owner]),
            ::std::addressof(state.tag_ids[owner]), ::std::addressof(state.data_ids[owner]), ::std::addressof(state.element_ids[owner])};
        for(::std::size_t space{}; space != maps.size(); ++space)
        {
            state.object(instance)->words[space] = maps[space]->size();
            for(auto wire : *maps[space]) { if(!state.append_link(instance, wire)) { return false; } }
        }
    }
    if(!census_copy_functions_and_tags(state)) { return false; }
    for(::std::size_t owner{}; owner != count; ++owner)
    {
        auto const& module{*state.actual.modules[owner].module};
        for(::std::size_t local{}; local != module.local_defined_memory_vec_storage.size(); ++local)
        {
            auto const& actual{module.local_defined_memory_vec_storage.index_unchecked(local)};
            if(!census_copy_memory(state, state.memory_ids[owner][module.imported_memory_vec_storage.size() + local], actual, actual.memory)) { return false; }
        }
        for(::std::size_t local{}; local != module.local_defined_table_vec_storage.size(); ++local)
        { if(!census_copy_table(state, owner, local, state.table_ids[owner][module.imported_table_vec_storage.size() + local])) { return false; } }
        for(::std::size_t local{}; local != module.local_defined_global_vec_storage.size(); ++local)
        { if(!census_copy_global(state, owner, local, state.global_ids[owner][module.imported_global_vec_storage.size() + local])) { return false; } }
        for(::std::size_t local{}; local != module.local_defined_data_vec_storage.size(); ++local)
        { if(!census_copy_data(state, owner, local, state.data_ids[owner][local], *files[owner])) { return false; } }
        for(::std::size_t local{}; local != module.local_defined_element_vec_storage.size(); ++local)
        { if(!census_copy_element(state, owner, local, state.element_ids[owner][local])) { return false; } }
    }
    if(!current_scope()) { return state.fail(census_error::stale_generation); }
    // Manager must still drain_complete_reference_graph, copy actual typed
    // activations/controls/handlers/waits/events/trace, validate the final graph,
    // retain its actual attachments, and issue within the SAME lexical guards.
    // No method here saves/publishes a file or grants executable restore.
    return state.status == census_error::none;
}
