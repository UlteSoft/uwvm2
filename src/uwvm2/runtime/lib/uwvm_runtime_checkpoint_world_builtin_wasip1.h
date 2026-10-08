// Private cold-world WASIp1 binding. Included inside the real transaction.
// Authenticate the original factory binding, never a serialized host address.
[[nodiscard]] bool bind_builtin_wasip1_function(source_owner const& original,
    ::std::uint_least64_t epoch, ::std::size_t owner, ::std::size_t imported,
    object_id id)
{
    namespace cp=checkpoint_world_data::cp;namespace st=checkpoint_world_data::st;
    if(owner>=modules_.size() || !source_type::has_canonical_owner(original)) { return false; }
    auto const& module{modules_[owner]};
    auto const old{original->registry_.find(module.file->module_name)};
    if(old==original->registry_.end() ||
       original->actual_validated_file(module.old_actual_id,epoch,::std::addressof(old->second))==nullptr ||
       imported>=old->second.imported_function_vec_storage.size() || imported>=module.actual->imported_function_vec_storage.size())
    { return false; }
    ::uwvm2::uwvm::runtime::full::builtin_wasip1_function_data binding{};
    if(!original->actual_builtin_wasip1_function(module.old_actual_id,epoch,::std::addressof(old->second),imported,binding))
    { return false; }
    auto const& prior{old->second.imported_function_vec_storage.index_unchecked(imported)};
    auto const& next{module.actual->imported_function_vec_storage.index_unchecked(imported)};
    if(next.link_kind!=st::imported_function_link_kind::local_imported ||
       next.target.local_imported.module_ptr!=prior.target.local_imported.module_ptr ||
       next.target.local_imported.index!=prior.target.local_imported.index)
    { return false; } // Compare native adapter identity; do not read its pointee.
    auto const* function{lookup(id)};auto* relocated{native(id)};
    if(function==nullptr || relocated==nullptr || function->kind!=cp::object_kind::function ||
       function->flags!=1u || function->links.size()!=2u || function->links[0u]!=module.instance ||
       function->words[0u]!=imported || function->words[1u]!=1u || !function->bytes.empty()) { return false; }
    auto const& types{module.actual->type_section_storage};
    if(next.import_type_ptr==nullptr || types.type_section_begin==nullptr || function->words[2u]>=types.type_section_count ||
       types.type_section_count>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(*types.type_section_begin) ||
       function->words[2u]>(UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(types.type_section_begin))/sizeof(*types.type_section_begin) ||
       next.import_type_ptr->imports.storage.function!=types.type_section_begin+static_cast<::std::size_t>(function->words[2u]))
    { return false; }
    auto const matches{[](auto const& actual,auto const& expected,unsigned count) noexcept
    {
        if(count>expected.size()) { return false; }
        if(actual.begin==nullptr || actual.end==nullptr)
        { return count==0u && actual.begin==nullptr && actual.end==nullptr; }
        auto const first{reinterpret_cast<::std::uintptr_t>(actual.begin)},last{reinterpret_cast<::std::uintptr_t>(actual.end)};
        if(last<first || last-first!=count*sizeof(*actual.begin)) { return false; }
        for(::std::size_t n{};n<count;++n) { if(static_cast<unsigned>(actual.begin[n])!=expected[n]) { return false; } }
        return true;
    }};
    auto const& signature{types.type_section_begin[function->words[2u]]};
    if(!matches(signature.parameter,binding.parameters,binding.parameter_count) ||
       !matches(signature.result,binding.results,binding.result_count)) { return false; }
    auto const* adapter{lookup(function->links[1u])};
    if(adapter==nullptr || !cp::state_details::valid_builtin_binding(*adapter) ||
       adapter->words!=::std::array<::std::uint64_t,8u>{cp::builtin_wasip1_binding_kind,1u,binding.function_index+1u,0u,0u,0u,0u,0u})
    { return false; }
    // Recreate the complete 108-byte canonical DATA descriptor using FastIO.
    // An equal ordinal/signature alone does not authenticate the interface.
    ::std::array<unsigned char,cp::builtin_wasip1_binding_bytes> bytes{};
    ::fast_io::basic_obuffer_view<unsigned char> output{bytes.data(),bytes.data()+bytes.size()};
    constexpr unsigned char magic[]{'U','W','V','M','W','A','S','I','P','1','B','1'};
    ::fast_io::operations::write_all(output,magic,magic+sizeof(magic));
    ::fast_io::print(output,::fast_io::mnp::le_put<64u>(binding.function_index),::fast_io::mnp::le_put<64u>(::std::uint64_t{1u}));
    auto const* digest{reinterpret_cast<unsigned char const*>(binding.interface_sha256.data())};
    ::fast_io::operations::write_all(output,digest,digest+binding.interface_sha256.size());
    ::fast_io::print(output,::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(binding.parameter_count)),
        ::fast_io::mnp::le_put<64u>(static_cast<::std::uint64_t>(binding.result_count)));
    ::fast_io::operations::write_all(output,binding.parameters.data(),binding.parameters.data()+binding.parameters.size());
    ::fast_io::operations::write_all(output,binding.results.data(),binding.results.data()+binding.results.size());
    if(output.curr_ptr!=bytes.data()+bytes.size() || adapter->bytes.size()!=bytes.size() ||
       ::fast_io::freestanding::my_memcmp(adapter->bytes.data(),bytes.data(),bytes.size())!=0) { return false; }
    if(relocated->owner!=SIZE_MAX)
    { return relocated->owner==owner && relocated->local==imported && relocated->imported_function && relocated->reference_ready; }
    relocated->owner=owner;relocated->local=imported;relocated->imported_function=true;
    relocated->value={};relocated->value.kind=::uwvm2::object::global::wasm_ref_kind::wasm_func_imported;
    relocated->value.storage.ptr=const_cast<st::imported_function_storage_t*>(::std::addressof(next));
    relocated->reference_ready=true;
    return true; // Unpublished native member, no host execution/effect permission.
}
