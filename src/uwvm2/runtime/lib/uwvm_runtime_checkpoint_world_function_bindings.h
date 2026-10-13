// Private member fragment of the complete candidate world. These final caches
// borrow ONLY the new source's authenticated records and its actual staged
// engines. Preparing them grants no world publication or execution permission.
#pragma once
decltype(g_runtime.defined_func_cache) defined_dispatch_bindings_{};
decltype(g_import_call_cache) import_dispatch_bindings_{};
decltype(g_runtime.defined_func_ptr_ranges) defined_dispatch_pointer_ranges_{};
::std::size_t prepared_defined_bindings_{}, prepared_import_bindings_{};
bool dispatch_bindings_prepared_{};

[[nodiscard]] bool prepare_world_dispatch_bindings(source_owner const& original,
    ::std::size_t maximum_bindings, llvm_jit_checkpoint_prepare_result& out)
{
    namespace in=checkpoint_world_data::in;
    using imported_kind=checkpoint_world_data::st::imported_function_link_kind;
    if(phase_!=preparation_status::resources_prepared || !source_type::has_canonical_owner(original) ||
       !original->initialized_from_actual_state() || !actual_prepared_profile_ ||
       engines_.size()!=modules_.size() || actual_dense_module_owners_.size()!=modules_.size() ||
       dispatch_bindings_prepared_ || !defined_dispatch_bindings_.empty() || !import_dispatch_bindings_.empty() ||
       !defined_dispatch_pointer_ranges_.empty())
    { return fail(preparation_status::source_mismatch); }
    ::std::size_t defined{},imports{},ranges{};
    for(auto const owner:actual_dense_module_owners_)
    {
        if(owner>=modules_.size() || modules_[owner].actual==nullptr ||
           context_->unpublished_file_for_module(modules_[owner].actual)!=modules_[owner].file)
        { return fail(preparation_status::source_mismatch); }
        auto const& module=*modules_[owner].actual;
        auto const locals=module.local_defined_function_vec_storage.size();
        auto const external=module.imported_function_vec_storage.size();
        if(defined>maximum_bindings || imports>maximum_bindings-defined ||
           locals>maximum_bindings-defined-imports)
        { return fail(preparation_status::quota_exceeded); }
        defined+=locals;
        if(external>maximum_bindings-defined-imports) { return fail(preparation_status::quota_exceeded); }
        imports+=external; // every sum bounded BEFORE increment
        if(locals!=0u) { ++ranges; } // at most the authenticated module count
    }
    using defined_vector=decltype(defined_dispatch_bindings_)::value_type;
    using import_vector=decltype(import_dispatch_bindings_)::value_type;
    if(!charge_native(modules_.size(),sizeof(defined_vector)+sizeof(import_vector)) ||
       !charge_native(defined,sizeof(compiled_defined_func_info)) ||
       !charge_native(imports,sizeof(cached_import_target)) ||
       !charge_native(ranges,sizeof(defined_func_ptr_range))) { return false; }
    defined_dispatch_bindings_.resize(modules_.size());
    import_dispatch_bindings_.resize(modules_.size());
    defined_dispatch_pointer_ranges_.reserve(ranges);
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const& bound=modules_[owner];auto const dense=bound.new_actual_id;
        if(dense>=engines_.size() || !engines_[dense] || dense>=defined_dispatch_bindings_.size() ||
           dense>=import_dispatch_bindings_.size()) { return fail(preparation_status::source_mismatch); }
        auto const& module=*bound.actual;auto const& engine=*engines_[dense];
        auto const locals=module.local_defined_function_vec_storage.size();
        auto const imported=module.imported_function_vec_storage.size();
        if(engine.module_id_!=dense || engine.owner_.module()!=bound.actual || engine.owner_.file()!=bound.file ||
           engine.locals_.size()!=locals || engine.entries_.size()!=locals || engine.typed_targets_.size()!=locals ||
           engine.generations_.size()!=locals || !engine.engine_ || !engine.context_)
        { return fail(preparation_status::source_mismatch); }
        auto& cache=defined_dispatch_bindings_.index_unchecked(dense);cache.resize(locals);
        import_dispatch_bindings_.index_unchecked(dense).resize(imported);
        if(locals!=0u)
        {
            // Final new-source storage is nonmoving. The pointer-range cache is
            // required by table/funcref dispatch as well as the direct cache.
            auto const* first=module.local_defined_function_vec_storage.data();
            constexpr auto width=sizeof(runtime_local_func_storage_t);
            if(first==nullptr || locals>(::std::numeric_limits<::std::uintptr_t>::max)()/width)
            { return fail(preparation_status::source_mismatch); }
            auto const begin=reinterpret_cast<::std::uintptr_t>(first);
            auto const bytes=static_cast<::std::uintptr_t>(locals*width);
            if(begin>(::std::numeric_limits<::std::uintptr_t>::max)()-bytes)
            { return fail(preparation_status::source_mismatch); }
            defined_dispatch_pointer_ranges_.push_back(defined_func_ptr_range{begin,begin+bytes,dense});
        }
        for(::std::size_t local{};local<locals;++local)
        {
            auto const* function=::std::addressof(module.local_defined_function_vec_storage.index_unchecked(local));
            auto const& entry=engine.entries_[local];auto const& plan=engine.locals_[local].checkpoint_plan;
            // The real fresh engine, final source member, sealed profile and
            // function generation all agree. No old entry/cache is reused.
            if(function->function_type_ptr==nullptr || imported>SIZE_MAX-local ||
               entry.typed==0u || entry.raw==0u || engine.typed_targets_.index_unchecked(local)!=entry.typed ||
               !plan || plan->get().module!=dense || plan->get().function!=imported+local ||
               plan->get().function_generation!=engine.generations_[local] ||
               plan->get().profile.get()!=actual_prepared_profile_.get() ||
               plan->get().profile.owner_before(actual_prepared_profile_) ||
               actual_prepared_profile_.owner_before(plan->get().profile))
            { return fail(preparation_status::source_mismatch); }
            auto const sig=func_sig_from_defined(function);
            auto const parameters=total_abi_bytes(sig.params);auto const results=total_abi_bytes(sig.results);
            if((parameters==0u && sig.params.size!=0u) || (results==0u && sig.results.size!=0u))
            { return fail(preparation_status::type_mismatch); }
            auto& next=cache.index_unchecked(local);
            next.module_id=dense;next.function_index=imported+local;next.runtime_func=function;
            next.param_bytes=parameters;next.result_bytes=results;
            next.reference_params=signature_has_reference_carrier(sig.params);
            next.reference_results=signature_has_reference_carrier(sig.results);
        }
    }
    ::std::sort(defined_dispatch_pointer_ranges_.begin(),defined_dispatch_pointer_ranges_.end(),
        [](defined_func_ptr_range const& left,defined_func_ptr_range const& right) noexcept { return left.begin<right.begin; });
    for(::std::size_t n{1u};n<defined_dispatch_pointer_ranges_.size();++n)
    {
        if(defined_dispatch_pointer_ranges_.index_unchecked(n-1u).end>
           defined_dispatch_pointer_ranges_.index_unchecked(n).begin)
        { return fail(preparation_status::source_mismatch); }
    }
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const& bound=modules_[owner];auto const dense=bound.new_actual_id;
        auto const& module=*bound.actual;auto& cache=import_dispatch_bindings_.index_unchecked(dense);
        for(::std::size_t index{};index<module.imported_function_vec_storage.size();++index)
        {
            auto const* function=::std::addressof(module.imported_function_vec_storage.index_unchecked(index));
            // Contextual resolver authenticates every hop against the NEW final
            // vectors before a union read. Cycles and foreign addresses refuse.
            auto const leaf=in::details::resolve_linked_imported_function_leaf_in_context(context_->world_,function);
            if(leaf.imported==nullptr) { return fail(preparation_status::invalid_import); }
            cached_import_target next{};next.frame.module_id=dense;next.frame.function_index=index;
            next.origin_module_id=dense;
            if(leaf.defined!=nullptr)
            {
                ::std::size_t target_owner{SIZE_MAX};
                for(::std::size_t n{};n<modules_.size();++n)
                { if(modules_[n].actual==leaf.defined_location.module) { target_owner=n;break; } }
                if(target_owner>=modules_.size()) { return fail(preparation_status::invalid_import); }
                auto const target=modules_[target_owner].new_actual_id;
                auto const local=leaf.defined_location.index;
                if(target>=defined_dispatch_bindings_.size() ||
                   local>=defined_dispatch_bindings_.index_unchecked(target).size())
                { return fail(preparation_status::invalid_import); }
                auto const& resolved=defined_dispatch_bindings_.index_unchecked(target).index_unchecked(local);
                if(resolved.runtime_func!=leaf.defined) { return fail(preparation_status::invalid_import); }
                next.k=cached_import_target::kind::defined;next.frame.module_id=target;
                next.frame.function_index=resolved.function_index;next.u.defined.runtime_func=resolved.runtime_func;
                next.sig=func_sig_from_defined(resolved.runtime_func);
                next.param_bytes=resolved.param_bytes;next.result_bytes=resolved.result_bytes;
            }
            else
            {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
                if(leaf.imported->link_kind!=imported_kind::local_imported ||
                   original->builtin_wasip1_member_==nullptr ||
                   leaf.imported->target.local_imported.module_ptr!=original->builtin_wasip1_member_ ||
                   dense>=wasip1_dispatch_contexts_.size())
                { return fail(preparation_status::unavailable_adapter); }
                auto const& environment=wasip1_dispatch_contexts_.index_unchecked(dense);
                if(!environment.import_visible || environment.env==nullptr)
                { return fail(preparation_status::invalid_import); }
                // Only the authenticated immutable builtin can supply native
                // signature metadata. No arbitrary provider callback is invoked.
                next.k=cached_import_target::kind::local_imported;
                next.u.local_imported=leaf.imported->target.local_imported;
                // The ordinary runtime helper terminates on invalid provider
                // metadata. Candidate preparation must decline without changing
                // the paused original world, including an invalid builtin index.
                if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_function_signature(
                    next.u.local_imported.module_ptr,next.u.local_imported.index,next.local_imported_signature.owned))
                { return fail(preparation_status::invalid_import); }
                // The primary cache retains a per-import FP policy. The ROS
                // full-only cache has no such member; preserve its native ABI.
                []<typename target_type>(target_type& target)
                {
                    if constexpr(requires { target.local_imported_wasm_fp_control_policy; })
                    {
                        target.local_imported_wasm_fp_control_policy=query_local_imported_function_wasm_fp_control_policy(
                            target.u.local_imported.module_ptr,target.u.local_imported.index);
                    }
                }(next);
                auto const sig=next.signature();
                next.param_bytes=total_abi_bytes(sig.params);next.result_bytes=total_abi_bytes(sig.results);
                if((next.param_bytes==0u && sig.params.size!=0u) || (next.result_bytes==0u && sig.results.size!=0u))
                { return fail(preparation_status::type_mismatch); }
#else
                return fail(preparation_status::unavailable_adapter);
#endif
            }
            next.reference_params=signature_has_reference_carrier(next.signature().params);
            next.reference_results=signature_has_reference_carrier(next.signature().results);
            cache.index_unchecked(index)=::std::move(next);
        }
    }
    prepared_defined_bindings_=defined;prepared_import_bindings_=imports;dispatch_bindings_prepared_=true;
    out.prepared_runtime_defined_bindings=defined;out.prepared_runtime_import_bindings=imports;
    out.prepared_runtime_defined_pointer_ranges=defined_dispatch_pointer_ranges_.size();
    out.runtime_dispatch_bindings_prepared=true;
    return true; // private DATA only: global cache/source/epoch untouched
}
