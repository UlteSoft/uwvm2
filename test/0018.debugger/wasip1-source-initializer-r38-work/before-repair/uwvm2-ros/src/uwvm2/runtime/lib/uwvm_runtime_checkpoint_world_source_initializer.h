// Private world member fragment. Fixed source/initializer binding DATA is
// prepared before retirement and rechecked after real OS/TLS join. It never
// issues an initializer serial, source seal, epoch or execution permission.
#pragma once
struct source_initializer_binding
{
    module_type const* module{};
    file_type const* file{};
    ::std::size_t owner{SIZE_MAX}, preload{SIZE_MAX};
};
::std::vector<source_initializer_binding> source_initializer_bindings_{};
decltype(source_type::builtin_wasip1_identity_) source_initializer_builtin_identity_{};
decltype(source_type::builtin_wasip1_member_) source_initializer_builtin_member_{};
decltype(source_type::builtin_wasip1_vector_begin_) source_initializer_builtin_vector_{};
::std::size_t source_initializer_builtin_extent_{},source_initializer_main_{SIZE_MAX};
::std::uint_least64_t source_initializer_observed_epoch_{},source_initializer_observed_serial_{};

[[nodiscard]] bool source_initializer_original_current(source_owner const& original) const noexcept
{
    auto const selected=::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
    if(!source_type::has_canonical_owner(original) || !selected || original.get()!=selected.get() ||
       original.owner_before(selected) || selected.owner_before(original) ||
       !original->initialized_from_actual_state() || source_initializer_observed_epoch_==0u ||
       source_initializer_observed_epoch_!=current_runtime_generation() ||
       source_initializer_observed_serial_==0u || original->initializer_serial_!=source_initializer_observed_serial_ ||
       ::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)!=source_initializer_observed_serial_ ||
       original->registry_.size()!=modules_.size() ||
       original->builtin_wasip1_identity_.get()!=source_initializer_builtin_identity_.get() ||
       original->builtin_wasip1_identity_.owner_before(source_initializer_builtin_identity_) ||
       source_initializer_builtin_identity_.owner_before(original->builtin_wasip1_identity_) ||
       original->builtin_wasip1_member_!=source_initializer_builtin_member_ ||
       original->builtin_wasip1_vector_begin_!=source_initializer_builtin_vector_ ||
       original->builtin_wasip1_vector_extent_!=source_initializer_builtin_extent_) { return false; }
    for(auto const& [name,module]:original->registry_)
    {
        static_cast<void>(name);
        auto const dense=original->bound_initialized_module_id(::std::addressof(module));
        // Canonical old source and its genuine finalized dense binding precede
        // every provider read. The builtin method resolves the real selected
        // loader/vector membership, never a saved host pointer or provider name.
        if(dense==SIZE_MAX || original->actual_validated_file(dense,source_initializer_observed_epoch_,::std::addressof(module))==nullptr ||
           !original->actual_no_unadapted_native_memory_provider(dense,source_initializer_observed_epoch_,::std::addressof(module)))
        { return false; }
    }
    return true;
}

[[nodiscard]] bool preflight_private_source_initializer_bindings() const noexcept
{
    if(phase_!=preparation_status::resources_prepared || !source_type::has_canonical_owner(prepared_source_) ||
       !context_ || !context_->all_actual_resource_records_filled() || !staged_gc_.publication_preflight() ||
       prepared_source_->initialized_ || prepared_source_->initializer_serial_!=0u || prepared_source_->main_module_!=nullptr ||
       prepared_source_->main_module_id_!=SIZE_MAX || prepared_source_->full_validation_complete_ || prepared_source_->validation_epoch_!=0u ||
       prepared_source_->preload_files_.size()!=prepared_source_->preload_bindings_.size() ||
       prepared_source_->registry_.size()!=modules_.size() || source_initializer_bindings_.size()!=modules_.size() ||
       source_initializer_bindings_.size()!=actual_dense_module_owners_.size() || source_initializer_main_>=source_initializer_bindings_.size())
    { return false; }
    auto const original=::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
    if(!source_initializer_original_current(original)) { return false; }
    ::std::size_t mains{},preloads{};
    for(::std::size_t dense{};dense<source_initializer_bindings_.size();++dense)
    {
        auto const& row=source_initializer_bindings_[dense];auto const owner=actual_dense_module_owners_[dense];
        if(owner>=modules_.size() || row.owner!=owner || row.module!=modules_[owner].actual || row.file!=modules_[owner].file ||
           modules_[owner].new_actual_id!=dense || context_->unpublished_file_for_module(row.module)!=row.file) { return false; }
        // All pointers have matched genuine nonmoving candidate registry/parser
        // members before their phase/name/declaration fields are read.
        auto const& phase=row.module->gc_collection_phase;
        if(phase.initializer_serial!=0u || phase.native_storage_owned || phase.initialized || phase.active_segments_ready) { return false; }
        auto const declaration=prepared_source_->checkpoint_declarations_.find(row.file->module_name);
        if(declaration==prepared_source_->checkpoint_declarations_.end() || declaration->second.module_storage_ptr.wf!=row.file) { return false; }
        using kind=::uwvm2::uwvm::wasm::type::module_type_t;
        if(row.preload==SIZE_MAX)
        {
            if(dense!=source_initializer_main_ || row.file!=::std::addressof(prepared_source_->file_) ||
               declaration->second.type!=kind::exec_wasm || ++mains!=1u) { return false; }
        }
        else
        {
            if(row.preload>=prepared_source_->preload_files_.size() ||
               row.file!=::std::addressof(prepared_source_->preload_files_.index_unchecked(row.preload)) ||
               declaration->second.type!=kind::preloaded_wasm) { return false; }
            auto const& unpublished=prepared_source_->preload_bindings_[row.preload];
            if(unpublished.module!=nullptr || unpublished.id!=SIZE_MAX || unpublished.validation_epoch!=0u) { return false; }
            ++preloads;
        }
    }
    return mains==1u && preloads==prepared_source_->preload_files_.size();
}

[[nodiscard]] bool prepare_private_source_initializer_bindings(source_owner const& original,
    ::std::uint_least64_t observed_epoch,::std::size_t maximum_modules,llvm_jit_checkpoint_prepare_result& out)
{
    if(!source_initializer_bindings_.empty() || phase_!=preparation_status::resources_prepared ||
       !source_type::has_canonical_owner(original) || modules_.empty() || modules_.size()!=actual_dense_module_owners_.size())
    { return fail(preparation_status::source_mismatch); }
    if(modules_.size()>maximum_modules || modules_.size()>source_initializer_bindings_.max_size() ||
       !charge_native(modules_.size(),sizeof(source_initializer_binding))) { return fail(preparation_status::quota_exceeded); }
    // Actual selected loader provenance only; the strong record retains no
    // implementation and confers no host admission. No old source/store/engine
    // is retained by this candidate. A later publisher must check it again.
    source_initializer_observed_epoch_=observed_epoch;source_initializer_observed_serial_=original->initializer_serial_;
    source_initializer_builtin_identity_=original->builtin_wasip1_identity_;
    source_initializer_builtin_member_=original->builtin_wasip1_member_;
    source_initializer_builtin_vector_=original->builtin_wasip1_vector_begin_;
    source_initializer_builtin_extent_=original->builtin_wasip1_vector_extent_;
    if(!source_initializer_original_current(original)) { return fail(preparation_status::source_mismatch); }
    source_initializer_bindings_.resize(modules_.size());
    for(::std::size_t dense{};dense<source_initializer_bindings_.size();++dense)
    {
        auto const owner=actual_dense_module_owners_[dense];
        if(owner>=modules_.size() || modules_[owner].new_actual_id!=dense ||
           context_->unpublished_file_for_module(modules_[owner].actual)!=modules_[owner].file)
        { return fail(preparation_status::source_mismatch); }
        auto& row=source_initializer_bindings_[dense];row={modules_[owner].actual,modules_[owner].file,owner,SIZE_MAX};
        if(row.file==::std::addressof(prepared_source_->file_))
        {
            if(source_initializer_main_!=SIZE_MAX) { return fail(preparation_status::source_mismatch); }
            source_initializer_main_=dense;
        }
        else
        {
            for(::std::size_t n{};n<prepared_source_->preload_files_.size();++n)
            { if(row.file==::std::addressof(prepared_source_->preload_files_.index_unchecked(n))) { row.preload=n;break; } }
            if(row.preload==SIZE_MAX) { return fail(preparation_status::source_mismatch); }
        }
    }
    if(!preflight_private_source_initializer_bindings()) { return fail(preparation_status::source_mismatch); }
    out.prepared_source_initializer_modules=source_initializer_bindings_.size();
    out.prepared_source_initializer_preloads=prepared_source_->preload_files_.size();
    out.runtime_source_initializer_bindings_prepared=true;
    return true;
}
