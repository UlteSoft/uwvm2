// Private candidate member fragment. Compact call_indirect views must borrow
// final NEW source records, caller type namespaces and NEW engine addresses.
// No current runtime cache, TLS projection cache or global refresh hook changes.
#pragma once
decltype(g_runtime.modules) indirect_dispatch_modules_{};
bool indirect_bindings_prepared_{};

template<typename Record,typename Selector>
[[nodiscard]] bool locate_private_dispatch_record(Record const* pointer,Selector select,
    ::std::size_t& dense,::std::size_t& local) const noexcept
{
    dense=local=SIZE_MAX;
    if(pointer==nullptr) { return false; }
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const& bound=modules_[owner];
        // Compare an address token with aligned FINAL vector membership before
        // reading the candidate pointee, including every imported alias hop.
        if(checkpoint_world_data::in::details::linked_function_record_index(pointer,select(*bound.actual),local))
        { dense=bound.new_actual_id;return true; }
    }
    return false;
}

[[nodiscard]] runtime_table_storage_t const* resolve_private_dispatch_table(
    module_type const& module,::std::size_t index,::std::size_t all_imports) const noexcept
{
    namespace st=checkpoint_world_data::st;
    auto const imported=module.imported_table_vec_storage.size();
    if(index>=imported)
    {
        auto const local=index-imported;
        return local<module.local_defined_table_vec_storage.size()?
            ::std::addressof(module.local_defined_table_vec_storage.index_unchecked(local)):nullptr;
    }
    auto const* next=::std::addressof(module.imported_table_vec_storage.index_unchecked(index));
    using kind=st::imported_table_storage_t::imported_table_link_kind;
    for(::std::size_t step{};step<all_imports;++step)
    {
        ::std::size_t dense{},local{};
        if(!locate_private_dispatch_record(next,[](auto const& m)->auto const& { return m.imported_table_vec_storage; },dense,local))
        { return nullptr; }
        auto const& actual=modules_[actual_dense_module_owners_[dense]].actual->imported_table_vec_storage.index_unchecked(local);
        if(actual.link_kind==kind::imported) { next=actual.target.imported_ptr;continue; }
        if(actual.link_kind!=kind::defined || !locate_private_dispatch_record(actual.target.defined_ptr,
            [](auto const& m)->auto const& { return m.local_defined_table_vec_storage; },dense,local))
        { return nullptr; }
        return ::std::addressof(modules_[actual_dense_module_owners_[dense]].actual->local_defined_table_vec_storage.index_unchecked(local));
    }
    return nullptr; // cycles cannot consume more than the actual import roster
}

[[nodiscard]] bool prepare_world_indirect_bindings(::std::size_t maximum_bindings,
    llvm_jit_checkpoint_prepare_result& out)
{
    namespace st=checkpoint_world_data::st;
    if(phase_!=preparation_status::resources_prepared || !dispatch_bindings_prepared_ ||
       indirect_bindings_prepared_ || !indirect_dispatch_modules_.empty() ||
       !context_ || !prepared_source_ || prepared_source_->initialized_from_actual_state() ||
       engines_.size()!=modules_.size() || actual_dense_module_owners_.size()!=modules_.size())
    { return fail(preparation_status::source_mismatch); }
    ::std::size_t total{},types{},views{},imported_tables{},targets{},function_views{};
    auto const reserve_count=[&](::std::size_t count)->bool
    {
        if(total>maximum_bindings || count>maximum_bindings-total)
        { return fail(preparation_status::quota_exceeded); }
        total+=count;return true;
    };
    // Bound alias expansion PER CALLER, not just distinct underlying tables.
    // Every count and native backing charge precedes cache/view allocation.
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const& bound=modules_[owner];
        if(bound.actual==nullptr || bound.new_actual_id>=modules_.size() ||
           context_->unpublished_file_for_module(bound.actual)!=bound.file)
        { return fail(preparation_status::source_mismatch); }
        auto const& module=*bound.actual;auto const& section=module.type_section_storage;
        auto const count=static_cast<::std::size_t>(section.type_section_count);
        if(count!=section.type_section_count || count>=UINT32_MAX ||
           (count!=0u && (section.type_section_begin==nullptr || section.type_section_end==nullptr)) ||
           ((section.type_section_begin==nullptr)!=(section.type_section_end==nullptr)) ||
           (section.type_section_begin!=nullptr &&
            (section.type_section_end<section.type_section_begin ||
             static_cast<::std::size_t>(section.type_section_end-section.type_section_begin)!=count)))
        { return fail(preparation_status::type_mismatch); }
        auto const imports=module.imported_table_vec_storage.size();
        auto const locals=module.local_defined_table_vec_storage.size();
        if(!reserve_count(count) || !reserve_count(imports) || !reserve_count(locals)) { return false; }
        types+=count;views+=imports+locals;imported_tables+=imports;
        if(module.llvm_jit_call_indirect_table_views.size()!=imports+locals)
        { return fail(preparation_status::source_mismatch); }
        for(auto const& view:module.llvm_jit_call_indirect_table_views)
        { if(view.data_address!=0u || view.size!=0u) { return fail(preparation_status::source_mismatch); } }
    }
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const& module=*modules_[owner].actual;
        for(::std::size_t index{};index<module.llvm_jit_call_indirect_table_views.size();++index)
        {
            auto const* table=resolve_private_dispatch_table(module,index,imported_tables);
            ::std::size_t table_dense{},local{};
            if(table==nullptr || !locate_private_dispatch_record(table,
                [](auto const& m)->auto const& { return m.local_defined_table_vec_storage; },table_dense,local))
            { return fail(preparation_status::invalid_import); }
            auto const* owner_module=modules_[actual_dense_module_owners_[table_dense]].actual;
            if(table->owner_module_rt_ptr!=owner_module || table->table_type_ptr==nullptr)
            { return fail(preparation_status::source_mismatch); }
            if(st::runtime_table_family(*table)!=st::runtime_table_reference_family::function) { continue; }
            if(!reserve_count(table->elems.size())) { return false; }
            targets+=table->elems.size();++function_views;
        }
    }
    using targets_vector=decltype(compiled_module_record::llvm_jit_call_indirect_targets)::value_type;
    if(!charge_native(modules_.size(),sizeof(compiled_module_record)) ||
       !charge_native(types,sizeof(::std::size_t)) || !charge_native(views,sizeof(targets_vector)) ||
       !charge_native(targets,sizeof(runtime_llvm_jit_raw_call_target_t))) { return false; }
    indirect_dispatch_modules_.resize(modules_.size());
    ::std::size_t defined{},imports{},nulls{},incompatible{};
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const& bound=modules_[owner];auto const dense=bound.new_actual_id;
        auto const& module=*bound.actual;auto& record=indirect_dispatch_modules_.index_unchecked(dense);
        record.runtime_module=bound.actual;record.type_canon_index=build_type_canon_index(module);
        if(record.type_canon_index.size()!=module.type_section_storage.type_section_count)
        { return fail(preparation_status::type_mismatch); }
        auto& tables=record.llvm_jit_call_indirect_targets;
        tables.resize(module.llvm_jit_call_indirect_table_views.size());
        for(::std::size_t index{};index<tables.size();++index)
        {
            auto const* table=resolve_private_dispatch_table(module,index,imported_tables);
            if(table==nullptr) { return fail(preparation_status::invalid_import); }
            if(st::runtime_table_family(*table)!=st::runtime_table_reference_family::function) { continue; }
            auto& entries=tables.index_unchecked(index);entries.resize(table->elems.size());
            for(::std::size_t slot{};slot<entries.size();++slot)
            {
                auto const& element=table->elems.index_unchecked(slot);
                auto& target=entries.index_unchecked(slot);
                module_type const* actual_module{};
                st::wasm_binfmt1_final_function_type_t const* actual_type{};
                ::std::size_t function_dense{},local{};
                using kind=st::local_defined_table_elem_storage_type_t;
                switch(element.type)
                {
                    case kind::func_ref_defined:
                    {
                        auto const* function=element.storage.defined_ptr;
                        if(function==nullptr) { ++nulls;continue; }
                        if(!locate_private_dispatch_record(function,
                            [](auto const& m)->auto const& { return m.local_defined_function_vec_storage; },function_dense,local) ||
                           function_dense>=engines_.size() || local>=engines_[function_dense]->entries_.size())
                        { return fail(preparation_status::source_mismatch); }
                        auto const& entry=engines_[function_dense]->entries_[local];
                        if(entry.raw==0u || entry.typed==0u) { return fail(preparation_status::source_mismatch); }
                        target.entry_address=entry.raw;target.typed_entry_address=entry.typed;
                        actual_module=modules_[actual_dense_module_owners_[function_dense]].actual;
                        actual_type=function->function_type_ptr;++defined;break;
                    }
                    case kind::func_ref_imported:
                    {
                        auto const* function=element.storage.imported_ptr;
                        if(function==nullptr) { ++nulls;continue; }
                        if(!locate_private_dispatch_record(function,
                            [](auto const& m)->auto const& { return m.imported_function_vec_storage; },function_dense,local) ||
                           function_dense>=import_dispatch_bindings_.size() ||
                           local>=import_dispatch_bindings_.index_unchecked(function_dense).size())
                        { return fail(preparation_status::invalid_import); }
                        auto const& cached=import_dispatch_bindings_.index_unchecked(function_dense).index_unchecked(local);
                        if(cached.origin_module_id!=function_dense) { return fail(preparation_status::invalid_import); }
                        if(cached.k==cached_import_target::kind::defined)
                        {
                            ::std::size_t leaf_dense{},leaf_local{};
                            if(!locate_private_dispatch_record(cached.u.defined.runtime_func,
                                [](auto const& m)->auto const& { return m.local_defined_function_vec_storage; },leaf_dense,leaf_local) ||
                               cached.frame.module_id!=leaf_dense)
                            { return fail(preparation_status::invalid_import); }
                            actual_module=modules_[actual_dense_module_owners_[leaf_dense]].actual;
                            actual_type=cached.u.defined.runtime_func->function_type_ptr;
                        }
                        else if(cached.k==cached_import_target::kind::local_imported)
                        {
                            actual_module=modules_[actual_dense_module_owners_[function_dense]].actual;
                            if(function->import_type_ptr==nullptr) { return fail(preparation_status::invalid_import); }
                            actual_type=function->import_type_ptr->imports.storage.function;
                        }
                        else { return fail(preparation_status::unavailable_adapter); }
                        target.entry_address=reinterpret_cast<::std::uintptr_t>(llvm_jit_raw_call_cached_import_entry);
                        target.context_address=reinterpret_cast<::std::uintptr_t>(::std::addressof(cached));
                        ++imports;break;
                    }
                    default: return fail(preparation_status::type_mismatch);
                }
                if(actual_module==nullptr || runtime_type_index_for_pointer(*actual_module,actual_type)==SIZE_MAX)
                { return fail(preparation_status::type_mismatch); }
                // The uncached matcher preserves rich heap/subtype identity and
                // does not contaminate the OLD world's TLS projection cache.
                target.encoded_type_id=find_canonical_type_id_for_type_uncached(record,*actual_module,actual_type);
                // Valid references need not match any callable type in this
                // caller; retain the sentinel so a later call traps normally.
                if(target.encoded_type_id==invalid_llvm_jit_encoded_type_id()) { ++incompatible; }
            }
        }
    }
    if(defined+imports+nulls!=targets) { return fail(preparation_status::source_mismatch); }
    // All final allocations and possible failures precede private view stores.
    // Empty GC/extern/exn tables stay empty even with a legacy funcref carrier.
    for(auto const owner:actual_dense_module_owners_)
    {
        auto& module=*modules_[owner].actual;auto const dense=modules_[owner].new_actual_id;
        auto const& tables=indirect_dispatch_modules_.index_unchecked(dense).llvm_jit_call_indirect_targets;
        for(::std::size_t index{};index<tables.size();++index)
        {
            auto const& entries=tables.index_unchecked(index);
            auto& view=module.llvm_jit_call_indirect_table_views.index_unchecked(index);
            view={};
            if(!entries.empty())
            { view={};
            if(!entries.empty())
            { view.data_address=reinterpret_cast<::std::uintptr_t>(entries.data());view.size=entries.size(); } }
        }
    }
    indirect_bindings_prepared_=true;
    out.runtime_indirect_bindings_prepared=true;out.prepared_runtime_type_bindings=types;
    out.prepared_runtime_table_views=views;out.prepared_runtime_function_table_views=function_views;
    out.prepared_runtime_indirect_targets=targets;out.prepared_runtime_indirect_defined_targets=defined;
    out.prepared_runtime_indirect_import_targets=imports;out.prepared_runtime_indirect_null_targets=nulls;
    out.prepared_runtime_indirect_incompatible_targets=incompatible;
    return true; // private DATA; no global publication or execution credential
}
