// Private candidate member fragment. Transfer genuine native allocations into
// FINAL full-runtime owners without publishing source/epoch/admission or a LIVE
// native seal. Typed targets and bitmap allocations must keep the same address.
#pragma once
bool full_code_handoff_complete_{};
::std::size_t handed_off_effective_bodies_{};

[[nodiscard]] bool preflight_private_handed_off_full_code() const noexcept
{
    if(!full_code_handoff_complete_ || !full_publication_records_prepared_ ||
       !preflight_private_source_initializer_bindings() || engines_.size()!=modules_.size() ||
       indirect_dispatch_modules_.size()!=engines_.size()) { return false; }
    ::std::size_t functions{},bodies{};
    for(::std::size_t dense{};dense<engines_.size();++dense)
    {
        auto const owner=actual_dense_module_owners_[dense];
        if(owner>=modules_.size() || !engines_[dense]) { return false; }
        auto const& bound=modules_[owner];auto const& staged=*engines_[dense];
        auto const& record=indirect_dispatch_modules_.index_unchecked(dense);
        auto const* publication=record.llvm_jit_full_publication.get();
        auto const count=bound.actual->local_defined_function_vec_storage.size();
        auto const imported=bound.actual->imported_function_vec_storage.size();
        if(!staged.owner_.matches_unpublished_file() || staged.owner_.module()!=bound.actual ||
           staged.owner_.file()!=bound.file || staged.module_id_!=dense || bound.new_actual_id!=dense ||
           record.runtime_module!=bound.actual || record.module_name!=bound.file->module_name ||
           !publication || !publication->engine || !publication->context || publication->engine->hasError() ||
           publication->engine.get()!=staged.handed_off_engine_ || publication->context.get()!=staged.handed_off_context_ ||
           staged.engine_ || staged.context_ || !staged.locals_.empty() || !staged.typed_targets_.empty() ||
           !staged.ranges_ || !staged.ranges_->private_handoff_matches(*publication->engine) ||
           publication->source.get()!=prepared_source_.get() || publication->source.owner_before(prepared_source_) ||
           prepared_source_.owner_before(publication->source) ||
           publication->checkpoint_profile.get()!=actual_prepared_profile_.get() ||
           publication->checkpoint_profile.owner_before(actual_prepared_profile_) ||
           actual_prepared_profile_.owner_before(publication->checkpoint_profile) ||
           publication->runtime_epoch!=0u || publication->debug_source_runtime_epoch!=0u || publication->plan ||
           publication->debug_source_binding || publication->checkpoint_compilation_identity || record.llvm_jit_ready ||
           record.llvm_jit_debug_source_fused_epoch!=0u || record.llvm_jit_precise_gc_roots_emitted ||
           publication->debug_cfi_manager!=staged.debug_cfi_manager_ || !publication->debug_cfi_manager ||
           publication->debug_cfi_manager->has_finalization_failure() ||
           publication->debug_shutdown_abi_revision.load(::std::memory_order_acquire)!=staged.debug_shutdown_abi_revision_ ||
           record.llvm_jit_debug_full_typed_entry_targets.data()!=staged.handed_off_targets_ ||
           record.llvm_jit_debug_full_typed_entry_targets.size()!=count || record.llvm_jit_compiled.local_funcs.size()!=count ||
           record.llvm_jit_local_entry_addresses.size()!=count || record.llvm_jit_local_raw_entry_addresses.size()!=count ||
           record.llvm_jit_debug_full_entry_generations.size()!=count || record.llvm_jit_debug_full_safe_points.size()!=count ||
           record.llvm_jit_checkpoint_generation_owners.size()!=count || publication->checkpoint_resume_entries.size()!=count ||
           staged.entries_.size()!=count || staged.generations_.size()!=count || staged.effective_bodies_.size()!=count ||
           imported>SIZE_MAX-count || count>SIZE_MAX-functions) { return false; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        if(staged.native_endpoints_ || !staged.native_endpoint_capture_qualified_ ||
           record.actual_native_generation_positions.size()!=count ||
           !private_native_endpoint_owner_matches(staged,record,publication)) { return false; }
#endif
        ::std::size_t body_index{};
        for(::std::size_t local{};local<count;++local)
        {
            auto const& metadata=record.llvm_jit_compiled.local_funcs.index_unchecked(local);
            auto const& entry=staged.entries_[local];auto const generation=staged.generations_[local];
            auto const& view=record.llvm_jit_debug_full_safe_points[local];
            auto const& resume=publication->checkpoint_resume_entries[local];
            auto const begin=reinterpret_cast<::std::uintptr_t>(metadata.code_begin);
            auto const end=reinterpret_cast<::std::uintptr_t>(metadata.code_end);
            auto const& declaration=bound.actual->local_defined_function_vec_storage.index_unchecked(local);
            if(metadata.module_id!=dense || metadata.function_index!=imported+local || metadata.runtime_module_ptr!=bound.actual ||
               metadata.function_type_ptr!=declaration.function_type_ptr || metadata.compiler_registry ||
               begin==0u || end<=begin || generation==0u || !metadata.checkpoint_plan ||
               metadata.checkpoint_plan->get().module!=dense || metadata.checkpoint_plan->get().function!=imported+local ||
               metadata.checkpoint_plan->get().function_generation!=generation ||
               metadata.checkpoint_plan->get().expression_bytes!=end-begin ||
               metadata.checkpoint_plan->get().profile.get()!=actual_prepared_profile_.get() ||
               metadata.checkpoint_plan->get().profile.owner_before(actual_prepared_profile_) ||
               actual_prepared_profile_.owner_before(metadata.checkpoint_plan->get().profile) ||
               metadata.debug_safe_point_bits.size()!=(end-begin)/8u+((end-begin)%8u!=0u) ||
               view.bits!=metadata.debug_safe_point_bits.data() || view.byte_count!=metadata.debug_safe_point_bits.size() ||
               view.expression_size!=end-begin || view.function_generation!=generation ||
               entry.typed==0u || entry.raw==0u || entry.resume_raw==0u ||
               record.llvm_jit_local_entry_addresses.index_unchecked(local)!=entry.typed ||
               record.llvm_jit_local_raw_entry_addresses.index_unchecked(local)!=entry.raw ||
               record.llvm_jit_debug_full_typed_entry_targets.index_unchecked(local)!=entry.typed ||
               record.llvm_jit_debug_full_entry_generations[local]!=generation || resume.address!=entry.resume_raw ||
               resume.plan.get()!=metadata.checkpoint_plan.get() || resume.plan.owner_before(metadata.checkpoint_plan) ||
               metadata.checkpoint_plan.owner_before(resume.plan) || staged.effective_bodies_[local] ||
               record.llvm_jit_checkpoint_generation_owners[local]!=nullptr) { return false; }
            if(generation==1u)
            { if(metadata.wasm_code_ptr!=declaration.wasm_code_ptr) { return false; } }
            else
            {
                if(body_index>=record.llvm_jit_debug_full_retained_generations.size()) { return false; }
                auto const& body=record.llvm_jit_debug_full_retained_generations[body_index++];
                // A retained parser body is genuine source DATA, never a fake
                // committed hot-replacement transaction or generation permission.
                if(!body || body->committed || body->engine || body->context || body->local_index!=local ||
                   body->source.get()!=prepared_source_.get() || body->source.owner_before(prepared_source_) ||
                   prepared_source_.owner_before(body->source) || metadata.wasm_code_ptr!=::std::addressof(body->code) ||
                   metadata.code_begin!=reinterpret_cast<::std::byte const*>(body->code.body.expr_begin) ||
                   metadata.code_end!=reinterpret_cast<::std::byte const*>(body->code.body.code_end) ||
                   !engine_type::extent_contains(body->section_bytes.data(),body->section_bytes.size(),
                       reinterpret_cast<::std::byte const*>(body->code.body.code_begin),metadata.code_begin,metadata.code_end))
                { return false; }
            }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
            if(record.actual_native_generation_positions[local]!=SIZE_MAX) { return false; }
#endif
        }
        if(body_index!=record.llvm_jit_debug_full_retained_generations.size() || body_index>SIZE_MAX-bodies) { return false; }
        functions+=count;bodies+=body_index;
    }
    return functions==prepared_publication_functions_ && bodies==handed_off_effective_bodies_;
}

#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
[[nodiscard]] bool private_native_endpoint_owner_matches(engine_type const& staged,
    compiled_module_record const& record,full_code_publication const* publication) const noexcept
{
    auto const* observer=publication->actual_native_endpoints.get();
    if(staged.entries_.empty()) { return observer==nullptr; }
    if(!observer || !observer->valid_ || !observer->staged_only_ || !observer->staged_frozen_ || observer->sealed_ ||
       observer->attached_ || observer->runtime_epoch_!=0u || observer->engine_!=publication->engine.get() ||
       observer->code_publication_!=publication || observer->module_!=record.runtime_module ||
       observer->module_id_!=staged.module_id_ || observer->source_.get()!=prepared_source_.get() ||
       observer->source_.owner_before(prepared_source_) || prepared_source_.owner_before(observer->source_) ||
       observer->staged_profile_.get()!=actual_prepared_profile_.get() ||
       observer->staged_profile_.owner_before(actual_prepared_profile_) ||
       actual_prepared_profile_.owner_before(observer->staged_profile_) || observer->staged_budget_owner_ || observer->staged_charge_ ||
       observer->expected_.size()!=staged.native_endpoint_expected_ || observer->claims_.empty()) { return false; }
    return true; // Frozen expected bodies/complete claims remain staged, never LIVE.
}
#endif

[[nodiscard]] bool handoff_private_full_code_owners(llvm_jit_checkpoint_prepare_result& out) noexcept
{
    if(full_code_handoff_complete_ || !preflight_private_full_publication_records())
    { return fail(preparation_status::source_mismatch); }
    static_assert(::std::is_nothrow_move_assignable_v<engine_type::local_storage>);
    static_assert(::std::is_nothrow_move_assignable_v<decltype(engine_type::typed_targets_)>);
    // ALL owners/observers qualify before the first private ownership move.
    for(auto const& staged:engines_)
    {
        if(!staged->ranges_ || !staged->ranges_->can_handoff_private_owner(*staged->engine_) ||
           !staged->debug_cfi_manager_ || staged->debug_cfi_manager_->has_finalization_failure())
        { return fail(preparation_status::source_mismatch); }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        auto const* observer=staged->native_endpoints_.get();
        if(!staged->locals_.empty() && (!observer || !observer->valid_ || !observer->staged_only_ ||
           !observer->staged_frozen_ || observer->sealed_ || observer->attached_ || observer->runtime_epoch_!=0u ||
           observer->engine_!=staged->engine_.get() || observer->code_publication_!=staged.get()))
        { return fail(preparation_status::source_mismatch); }
#endif
    }
    for(::std::size_t dense{};dense<engines_.size();++dense)
    {
        auto& staged=*engines_[dense];auto& record=indirect_dispatch_modules_.index_unchecked(dense);
        auto& publication=*record.llvm_jit_full_publication;
        staged.handed_off_engine_=staged.engine_.get();staged.handed_off_context_=staged.context_.get();
        staged.handed_off_targets_=staged.typed_targets_.data();
        // Detach while the actual engine still lives. Keep pending diagnostics
        // uncommitted for the later genuine joint issuer; no global ranges move.
        staged.ranges_->handoff_private_owner(*staged.engine_);
        publication.engine=::std::move(staged.engine_);publication.context=::std::move(staged.context_);
        publication.debug_cfi_manager=staged.debug_cfi_manager_;
        publication.debug_shutdown_abi_revision.store(staged.debug_shutdown_abi_revision_,::std::memory_order_release);
        record.llvm_jit_debug_full_typed_entry_targets=::std::move(staged.typed_targets_);
        for(::std::size_t local{};local<staged.locals_.size();++local)
        {
            record.llvm_jit_compiled.local_funcs.index_unchecked(local)=::std::move(staged.locals_[local]);
            if(staged.effective_bodies_[local])
            {
                // Capacity for ALL functions was reserved by R39. This move
                // creates no null entries and cannot reallocate the owner array.
                record.llvm_jit_debug_full_retained_generations.emplace_back(::std::move(staged.effective_bodies_[local]));
                ++handed_off_effective_bodies_;
            }
        }
        staged.locals_.clear();
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
        publication.actual_native_endpoints=::std::move(staged.native_endpoints_);
        if(publication.actual_native_endpoints) { publication.actual_native_endpoints->code_publication_=::std::addressof(publication); }
#endif
    }
    full_code_handoff_complete_=true;
    if(!preflight_private_handed_off_full_code()) { return fail(preparation_status::source_mismatch); }
    out.prepared_full_code_owner_modules=engines_.size();out.prepared_full_code_owner_functions=prepared_publication_functions_;
    out.prepared_full_code_effective_bodies=handed_off_effective_bodies_;
    out.runtime_full_code_owners_transferred_privately=true;out.runtime_full_code_targets_allocation_preserved=true;
    return true;
}
