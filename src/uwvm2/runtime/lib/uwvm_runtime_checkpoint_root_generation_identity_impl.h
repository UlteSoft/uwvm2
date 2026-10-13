// TU-private implementation after real record/registry and native-owner bridges.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
namespace
{
[[nodiscard]] bool checkpoint_root_generation_publication_bridge::matches_record(
    identity const& root,compiled_module_record const& record,::std::size_t dense,bool published_slots) noexcept
{
    auto const* publication=record.llvm_jit_full_publication.get();
    // All pointers came from compiler-owned records; compare captured owners
    // before traversing their metadata. This is not a supplied native handle.
    if(!publication || publication!=root.publication_ || !publication->engine || !publication->context ||
       publication->engine.get()!=root.engine_ || publication->context.get()!=root.context_ ||
       publication->engine->hasError() || !same_owner(publication->source,root.source_) ||
       !same_owner(publication->checkpoint_profile,root.profile_) || publication->plan ||
       record.runtime_module!=root.module_ || !record.runtime_module || root.module_id_!=dense ||
       !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(root.source_)) { return false; }
    auto const count=record.runtime_module->local_defined_function_vec_storage.size();
    auto const imported=record.runtime_module->imported_function_vec_storage.size();
    if(imported>SIZE_MAX-count || root.rows_.size()!=count ||
       record.llvm_jit_compiled.local_funcs.size()!=count ||
       record.llvm_jit_debug_full_entry_generations.size()!=count ||
       record.llvm_jit_debug_full_typed_entry_targets.size()!=count ||
       record.llvm_jit_local_entry_addresses.size()!=count ||
       record.llvm_jit_local_raw_entry_addresses.size()!=count ||
       publication->checkpoint_resume_entries.size()!=count) { return false; }
    // The published lookup checks only its one selected row. The preparation /
    // sealing caller checks every row before adoption.
    static_cast<void>(published_slots);return true;
}
[[nodiscard]] bool checkpoint_root_generation_publication_bridge::matches_row(
    identity const& root,compiled_module_record const& record,::std::size_t local,bool published_slots) noexcept
{
    if(local>=root.rows_.size()) { return false; }
    auto const& row=root.rows_[local];
    auto const& metadata=record.llvm_jit_compiled.local_funcs.index_unchecked(local);
    auto const& publication=*record.llvm_jit_full_publication;
    auto const imported=record.runtime_module->imported_function_vec_storage.size();
    auto const begin=reinterpret_cast<::std::uintptr_t>(metadata.code_begin);
    auto const end=reinterpret_cast<::std::uintptr_t>(metadata.code_end);
    if(!row.plan || !same_owner(row.plan,metadata.checkpoint_plan) || !same_owner(row.plan->get().profile,root.profile_) ||
       row.generation==0u || row.plan->get().function_generation!=row.generation ||
       row.plan->get().module!=root.module_id_ || row.plan->get().function!=imported+local ||
       metadata.module_id!=root.module_id_ || metadata.function_index!=imported+local ||
       metadata.runtime_module_ptr!=root.module_ || metadata.wasm_code_ptr!=row.wasm_code ||
       begin==0u || end<=begin || begin!=row.expression_begin || end!=row.expression_end ||
       row.plan->get().expression_bytes!=end-begin ||
       record.llvm_jit_debug_full_entry_generations[local]!=row.generation ||
       row.typed==0u || row.raw==0u ||
       record.llvm_jit_local_entry_addresses.index_unchecked(local)!=row.typed ||
       record.llvm_jit_local_raw_entry_addresses.index_unchecked(local)!=row.raw) { return false; }
    auto const& resume=publication.checkpoint_resume_entries[local];
    if(row.plan->get().resume_abi_revision==0u)
    { if(row.resume!=0u || resume.address!=0u || resume.plan) { return false; } }
    else if(row.plan->get().resume_abi_revision!=2u || row.plan->get().resume_sites.empty() ||
            row.resume==0u || resume.address!=row.resume || !same_owner(resume.plan,row.plan)) { return false; }
    if(published_slots)
    {
        // [actual fixed target array, local<N] exact owner checked above.
        // Concurrent entry readers use the same release/acquire slot protocol.
        auto& slot=const_cast<::std::uintptr_t&>(record.llvm_jit_debug_full_typed_entry_targets.index_unchecked(local));
        if(::std::atomic_ref<::std::uintptr_t>{slot}.load(::std::memory_order_acquire)!=row.typed) { return false; }
    }
    return true;
}
[[nodiscard]] ::std::unique_ptr<checkpoint_root_generation_publication_bridge::identity>
checkpoint_root_generation_publication_bridge::prepare(compiled_module_record const& record,::std::size_t dense) noexcept
{
    try
    {
        auto const* publication=record.llvm_jit_full_publication.get();
        if(!publication || !publication->engine || !publication->context || !publication->checkpoint_profile ||
           !record.runtime_module || dense==SIZE_MAX || publication->checkpoint_root_identity ||
           !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(publication->source)) { return {}; }
        auto root=::std::unique_ptr<identity>{new identity{}};
        root->source_=publication->source;root->profile_=publication->checkpoint_profile;
        root->module_=record.runtime_module;root->engine_=publication->engine.get();root->context_=publication->context.get();
        root->publication_=publication;root->module_id_=dense;
        auto const count=record.runtime_module->local_defined_function_vec_storage.size();
        if(count>SIZE_MAX/sizeof(identity::row) || record.llvm_jit_compiled.local_funcs.size()!=count ||
           record.llvm_jit_debug_full_entry_generations.size()!=count ||
           record.llvm_jit_local_entry_addresses.size()!=count || record.llvm_jit_local_raw_entry_addresses.size()!=count ||
           publication->checkpoint_resume_entries.size()!=count) { return {}; }
        root->rows_.reserve(count);
        for(::std::size_t local{};local<count;++local)
        {
            auto const& metadata=record.llvm_jit_compiled.local_funcs.index_unchecked(local);
            root->rows_.push_back({metadata.checkpoint_plan,metadata.wasm_code_ptr,
                reinterpret_cast<::std::uintptr_t>(metadata.code_begin),reinterpret_cast<::std::uintptr_t>(metadata.code_end),
                record.llvm_jit_local_entry_addresses.index_unchecked(local),
                record.llvm_jit_local_raw_entry_addresses.index_unchecked(local),
                publication->checkpoint_resume_entries[local].address,record.llvm_jit_debug_full_entry_generations[local]});
        }
        if(!matches_record(*root,record,dense,false)) { return {}; }
        for(::std::size_t local{};local<count;++local)
        { if(!matches_row(*root,record,local,false)) { return {}; } }
        return root;
    }
    catch(...) { return {}; }
}
[[nodiscard]] bool checkpoint_root_generation_publication_bridge::seal_actual_full(compiled_module_record& supplied) noexcept
{
    // Match actual registry membership BEFORE dereferencing the supplied record.
    if(get_runtime_state_publication_depth()!=1u) { return false; }
    auto selected=g_runtime.modules.end();
    for(auto it=g_runtime.modules.begin();it!=g_runtime.modules.end();++it)
    { if(::std::addressof(*it)==::std::addressof(supplied)) { selected=it;break; } }
    if(selected==g_runtime.modules.end() || !selected->llvm_jit_full_publication ||
       !selected->llvm_jit_full_publication->checkpoint_root_identity) { return false; }
    auto& record=*selected;auto& root=*record.llvm_jit_full_publication->checkpoint_root_identity;
    auto const epoch=current_runtime_generation();
    auto const serial=::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire);
    if(root.sealed_epoch_!=0u || root.initializer_serial_!=0u || epoch==0u || serial==0u ||
       root.module_id_>=g_runtime.modules.size() ||
       ::std::addressof(g_runtime.modules.index_unchecked(root.module_id_))!=::std::addressof(record) ||
       !matches_record(root,record,root.module_id_,true) ||
       !::uwvm2::uwvm::runtime::full::selected_full_source_matches_owner(root.source_) ||
       !record.runtime_module->gc_collection_phase.initialized_for(serial) ||
       root.source_->actual_validated_file(root.module_id_,epoch,record.runtime_module)==nullptr ||
       record.llvm_jit_full_publication->debug_source_runtime_epoch!=epoch ||
       record.llvm_jit_debug_source_fused_epoch!=epoch) { return false; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    if(!root.rows_.empty() && !details::native_owner_publication_bridge::has_actual_sealed_full_root(record)) { return false; }
#endif
    for(::std::size_t local{};local<root.rows_.size();++local)
    { if(!matches_row(root,record,local,true)) { return false; } }
    root.initializer_serial_=serial;root.sealed_epoch_=epoch;return true;
}
[[nodiscard]] bool checkpoint_root_generation_publication_bridge::matches_private_prepared(
    compiled_module_record const& record,::std::size_t dense) noexcept
{
    auto const* publication=record.llvm_jit_full_publication.get();
    if(!publication || !publication->checkpoint_root_identity || record.llvm_jit_ready ||
       publication->debug_source_runtime_epoch!=0u || record.llvm_jit_debug_source_fused_epoch!=0u) { return false; }
    auto const& root=*publication->checkpoint_root_identity;
    if(root.sealed_epoch_!=0u || root.initializer_serial_!=0u || root.source_->initialized_from_actual_state() ||
       !matches_record(root,record,dense,true)) { return false; }
    for(::std::size_t local{};local<root.rows_.size();++local)
    { if(!matches_row(root,record,local,true)) { return false; } }
    return true;
}
[[nodiscard]] bool checkpoint_root_generation_publication_bridge::matches_current_root(
    compiled_module_record const& supplied,::std::size_t local,::std::uint64_t generation) noexcept
{
    // Internal callers already selected a real runtime record under their
    // execution/metadata scope. This lookup never accepts a public opaque pointer.
    auto const* publication=supplied.llvm_jit_full_publication.get();
    if(!publication || !publication->checkpoint_root_identity || !supplied.llvm_jit_ready || generation==0u) { return false; }
    auto const& root=*publication->checkpoint_root_identity;
    if(root.module_id_>=g_runtime.modules.size() ||
       ::std::addressof(g_runtime.modules.index_unchecked(root.module_id_))!=::std::addressof(supplied)) { return false; }
    auto const epoch=current_runtime_generation();
    if(epoch==0u || root.sealed_epoch_!=epoch || root.initializer_serial_==0u ||
       root.initializer_serial_!=::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) ||
       !matches_record(root,supplied,root.module_id_,true) || local>=root.rows_.size() ||
       root.rows_[local].generation!=generation ||
       !::uwvm2::uwvm::runtime::full::selected_full_source_matches_owner(root.source_) ||
       publication->debug_source_runtime_epoch!=epoch || supplied.llvm_jit_debug_source_fused_epoch!=epoch ||
       !supplied.runtime_module->gc_collection_phase.initialized_for(root.initializer_serial_) ||
       root.source_->actual_validated_file(root.module_id_,epoch,supplied.runtime_module)==nullptr) { return false; }
    return matches_row(root,supplied,local,true);
}
}
#endif
