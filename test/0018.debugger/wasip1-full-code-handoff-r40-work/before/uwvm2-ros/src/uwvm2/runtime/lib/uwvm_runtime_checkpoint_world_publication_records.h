// Private candidate member fragment. Allocate the FINAL full-runtime record
// shape before retirement. Engines, typed target arrays, loaded provenance and
// LIVE native seals stay with their authentic staged owners until joint commit.
#pragma once
bool full_publication_records_prepared_{};
::std::size_t prepared_publication_functions_{};

template<typename Local>
[[nodiscard]] static bool empty_optional_publication_metadata(Local const& local) noexcept
{
    // ROS full-only metadata deliberately has no tiered/OSR container.
    if constexpr(requires { local.tiered_loop_reentries.empty(); })
    { return local.tiered_loop_reentries.empty(); }
    else { return true; }
}

[[nodiscard]] bool preflight_private_full_publication_records() const noexcept
{
    if(!full_publication_records_prepared_ || !indirect_bindings_prepared_ ||
       !preflight_private_source_initializer_bindings() ||
       engines_.size()!=modules_.size() || indirect_dispatch_modules_.size()!=engines_.size()) { return false; }
    ::std::size_t functions{};
    for(::std::size_t dense{};dense<engines_.size();++dense)
    {
        auto const owner=actual_dense_module_owners_[dense];
        if(owner>=modules_.size() || !engines_[dense]) { return false; }
        auto const& bound=modules_[owner];auto const& engine=*engines_[dense];
        auto const& record=indirect_dispatch_modules_.index_unchecked(dense);
        auto const* publication=record.llvm_jit_full_publication.get();
        auto const count=bound.actual->local_defined_function_vec_storage.size();
        if(record.runtime_module!=bound.actual || record.module_name!=bound.file->module_name ||
           bound.new_actual_id!=dense || engine.module_id_!=dense ||
           engine.owner_.module()!=bound.actual || engine.owner_.file()!=bound.file ||
           !engine.owner_.matches_unpublished_file() || !engine.engine_ || !engine.context_ ||
           engine.locals_.size()!=count || engine.entries_.size()!=count || engine.generations_.size()!=count ||
           engine.effective_bodies_.size()!=count || engine.typed_targets_.size()!=count ||
           publication==nullptr || publication->source.get()!=prepared_source_.get() ||
           publication->source.owner_before(prepared_source_) || prepared_source_.owner_before(publication->source) ||
           publication->checkpoint_profile.get()!=actual_prepared_profile_.get() ||
           publication->checkpoint_profile.owner_before(actual_prepared_profile_) ||
           actual_prepared_profile_.owner_before(publication->checkpoint_profile) ||
           publication->engine || publication->context || publication->plan ||
           publication->runtime_epoch!=0u || publication->debug_source_runtime_epoch!=0u ||
           publication->debug_source_binding || publication->checkpoint_compilation_identity ||
           publication->debug_shutdown_abi_revision.load(::std::memory_order_acquire)!=0u ||
           publication->debug_cfi_manager!=nullptr || record.llvm_jit_ready || record.llvm_jit_precise_gc_roots_emitted ||
           record.llvm_jit_debug_source_fused_epoch!=0u || !record.llvm_jit_debug_full_typed_entry_targets.empty() ||
           record.llvm_jit_compiled.local_funcs.size()!=count || record.llvm_jit_local_entry_addresses.size()!=count ||
           record.llvm_jit_local_raw_entry_addresses.size()!=count || record.llvm_jit_debug_full_entry_generations.size()!=count ||
           record.llvm_jit_debug_full_safe_points.size()!=count || !record.llvm_jit_debug_full_retained_generations.empty() ||
           record.llvm_jit_debug_full_retained_generations.capacity()<count ||
           record.llvm_jit_checkpoint_generation_owners.size()!=count || publication->checkpoint_resume_entries.size()!=count)
        { return false; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        if(publication->actual_native_endpoints || record.actual_native_generation_positions.size()!=count ||
           !engine.native_endpoint_capture_qualified_ || count>SIZE_MAX/2u || engine.native_endpoint_expected_!=count*2u)
        { return false; }
#endif
        for(::std::size_t local{};local<count;++local)
        {
            auto const& input=engine.locals_[local];auto const& slot=record.llvm_jit_compiled.local_funcs.index_unchecked(local);
            auto const& entry=engine.entries_[local];auto const& resume=publication->checkpoint_resume_entries[local];
            auto const& view=record.llvm_jit_debug_full_safe_points[local];
            auto const begin=reinterpret_cast<::std::uintptr_t>(input.code_begin);
            auto const end=reinterpret_cast<::std::uintptr_t>(input.code_end);
            auto const generation=engine.generations_[local];
            // Actual engine/source roster and complete equal extents above
            // precede every bounded slot. Packet/source pointers are DATA only.
            if(begin==0u || end<=begin || generation==0u || !input.checkpoint_plan ||
               input.compiler_registry!=nullptr || input.checkpoint_plan->get().module!=dense ||
               input.checkpoint_plan->get().function_generation!=generation ||
               input.checkpoint_plan->get().expression_bytes!=end-begin ||
               input.debug_safe_point_bits.size()!=(end-begin)/8u+((end-begin)%8u!=0u) ||
               view.bits!=input.debug_safe_point_bits.data() || view.byte_count!=input.debug_safe_point_bits.size() ||
               view.expression_size!=end-begin || view.function_generation!=generation ||
               entry.typed==0u || entry.raw==0u || entry.resume_raw==0u ||
               engine.typed_targets_.index_unchecked(local)!=entry.typed ||
               record.llvm_jit_local_entry_addresses.index_unchecked(local)!=entry.typed ||
               record.llvm_jit_local_raw_entry_addresses.index_unchecked(local)!=entry.raw ||
               record.llvm_jit_debug_full_entry_generations[local]!=generation ||
               resume.address!=entry.resume_raw || resume.plan.get()!=input.checkpoint_plan.get() ||
               resume.plan.owner_before(input.checkpoint_plan) || input.checkpoint_plan.owner_before(resume.plan) ||
               record.llvm_jit_checkpoint_generation_owners[local]!=nullptr ||
               slot.function_type_ptr!=nullptr || slot.wasm_code_ptr!=nullptr || slot.code_begin!=nullptr || slot.code_end!=nullptr ||
               slot.runtime_module_ptr!=nullptr || slot.compiler_registry!=nullptr || slot.checkpoint_plan ||
               !slot.debug_safe_point_bits.empty() || !empty_optional_publication_metadata(slot) ||
               (generation==1u ? static_cast<bool>(engine.effective_bodies_[local]) : !engine.effective_bodies_[local]))
            { return false; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
            if(record.actual_native_generation_positions[local]!=SIZE_MAX) { return false; }
#endif
        }
        if(count>SIZE_MAX-functions) { return false; } functions+=count;
    }
    return functions==prepared_publication_functions_;
}

[[nodiscard]] bool prepare_private_full_publication_records(::std::size_t maximum_functions,
    llvm_jit_checkpoint_prepare_result& out)
{
    if(full_publication_records_prepared_ || !indirect_bindings_prepared_ ||
       !preflight_private_source_initializer_bindings() || indirect_dispatch_modules_.size()!=engines_.size())
    { return fail(preparation_status::source_mismatch); }
    ::std::size_t total{};
    for(auto const& engine:engines_)
    {
        if(!engine || !engine->owner_.matches_unpublished_file() || total>maximum_functions ||
           engine->locals_.size()>maximum_functions-total) { return fail(preparation_status::quota_exceeded); }
        total+=engine->locals_.size(); // bounded BEFORE summing and allocating.
    }
    using local_type=engine_type::local_storage;
    constexpr ::std::size_t slot_bytes=sizeof(local_type)+2u*sizeof(::std::uintptr_t)+sizeof(::std::uint_least64_t)+
        sizeof(llvm_jit_debug_safe_point_view)+sizeof(::std::unique_ptr<llvm_jit_debug_replace_transaction>)+
        sizeof(llvm_jit_debug_replace_transaction const*)+sizeof(full_code_publication::checkpoint_resume_entry)
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        +sizeof(::std::size_t)
#endif
        ;
    if(!charge_native(engines_.size(),sizeof(full_code_publication)) || !charge_native(total,slot_bytes)) { return false; }
    for(::std::size_t dense{};dense<engines_.size();++dense)
    {
        auto const owner=actual_dense_module_owners_[dense];
        if(owner>=modules_.size() || !engines_[dense]) { return fail(preparation_status::source_mismatch); }
        auto const& engine=*engines_[dense];auto const& bound=modules_[owner];
        auto& record=indirect_dispatch_modules_.index_unchecked(dense);auto const count=engine.locals_.size();
        if(record.runtime_module!=bound.actual || record.llvm_jit_full_publication || engine.entries_.size()!=count ||
           engine.generations_.size()!=count || engine.typed_targets_.size()!=count || !engine.engine_ || !engine.context_)
        { return fail(preparation_status::source_mismatch); }
        record.module_name=bound.file->module_name;
        record.llvm_jit_full_publication.reset(new full_code_publication{prepared_source_});
        auto& publication=*record.llvm_jit_full_publication;
        publication.checkpoint_profile=actual_prepared_profile_;
        publication.checkpoint_resume_entries.resize(count);
        record.llvm_jit_compiled.local_funcs.resize(count);
        record.llvm_jit_local_entry_addresses.resize(count);
        record.llvm_jit_local_raw_entry_addresses.resize(count);
        record.llvm_jit_debug_full_entry_generations.resize(count);
        record.llvm_jit_debug_full_safe_points.resize(count);
        // Existing terminal teardown iterates only genuine nonnull retained
        // bodies. Reserve capacity; do not insert artificial null generations.
        record.llvm_jit_debug_full_retained_generations.reserve(count);
        record.llvm_jit_checkpoint_generation_owners.resize(count,nullptr);
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        record.actual_native_generation_positions.resize(count,SIZE_MAX);
#endif
        for(::std::size_t local{};local<count;++local)
        {
            auto const& metadata=engine.locals_[local];auto const& entry=engine.entries_[local];
            auto const begin=reinterpret_cast<::std::uintptr_t>(metadata.code_begin);
            auto const end=reinterpret_cast<::std::uintptr_t>(metadata.code_end);
            if(begin==0u || end<=begin) { return fail(preparation_status::source_mismatch); }
            record.llvm_jit_local_entry_addresses.index_unchecked(local)=entry.typed;
            record.llvm_jit_local_raw_entry_addresses.index_unchecked(local)=entry.raw;
            record.llvm_jit_debug_full_entry_generations[local]=engine.generations_[local];
            publication.checkpoint_resume_entries[local]={metadata.checkpoint_plan,entry.resume_raw};
            record.llvm_jit_debug_full_safe_points[local]={metadata.debug_safe_point_bits.data(),
                metadata.debug_safe_point_bits.size(),end-begin,engine.generations_[local]};
            // FINAL empty metadata slots and retained-body slots are allocated
            // now. Joint commit can move each genuine source/bitmap/body owner
            // without allocation, preserving already embedded target addresses.
            // No engine, target buffer, CFI/provenance or native seal moves here.
        }
    }
    full_publication_records_prepared_=true;prepared_publication_functions_=total;
    if(!preflight_private_full_publication_records()) { return fail(preparation_status::source_mismatch); }
    out.prepared_full_publication_modules=indirect_dispatch_modules_.size();
    out.prepared_full_publication_functions=total;out.runtime_full_publication_records_prepared=true;
    return true;
}
