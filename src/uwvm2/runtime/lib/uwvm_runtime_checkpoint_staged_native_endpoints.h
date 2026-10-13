// Included in runtime::lib after the genuine initializer-minted staged owner.
// Prepared endpoint DATA never becomes a live native-owner seal or ASM grant.
#pragma once
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
namespace details
{
    inline pending_actual_native_endpoints::pending_actual_native_endpoints(
        ::llvm::ExecutionEngine& engine,void const* code_owner,
        ::uwvm2::uwvm::runtime::initializer::staged_compiler_module_owner const& owner,
        ::uwvm2::runtime::checkpoint::compilation_profile::owner profile,::std::size_t module,
        ::std::vector<expected_function> expected,void* budget,staged_native_charge charge)
        : engine_{::std::addressof(engine)},code_publication_{code_owner},source_{owner.source()},
          module_{owner.module()},module_id_{module},expected_{::std::move(expected)},staged_only_{true},
          staged_profile_{::std::move(profile)},staged_budget_owner_{budget},staged_charge_{charge}
    {
        if(code_owner==nullptr || !owner.matches_unpublished_file() || !staged_profile_ ||
           staged_profile_->purpose()!=::uwvm2::runtime::checkpoint::compilation_purpose::resumable ||
           staged_profile_->cache_identity()[1u]!=17u || module==SIZE_MAX || budget==nullptr || charge==nullptr ||
           expected_.empty() || expected_.size()>native_owner_object_graph::detail::max_rows)
        { return; }
        ::std::sort(expected_.begin(),expected_.end(),[](auto const& a,auto const& b){ return a.original_ir_name<b.original_ir_name; });
        for(::std::size_t index{};index<expected_.size();++index)
        {
            auto const& item=expected_[index];auto const& plan=item.plan;
            if(item.original_ir_name.empty() || item.original_ir_name.size()>native_owner_table_format::max_name_bytes ||
               item.module!=module || item.generation==0u || (item.role!=1u && item.role!=2u) ||
               (index!=0u && expected_[index-1u].original_ir_name==item.original_ir_name) ||
               !plan || plan->get().module!=module || plan->get().function!=item.function ||
               plan->get().function_generation!=item.generation || plan->get().resume_abi_revision!=2u ||
               plan->get().profile.get()!=staged_profile_.get() || plan->get().profile.owner_before(staged_profile_) ||
               staged_profile_.owner_before(plan->get().profile) || (item.role==2u && plan->get().resume_sites.empty()))
            { return; }
        }
        seen_.resize(expected_.size());valid_=true;
        engine.RegisterJITEventListener(this);attached_=true;
        // runtime_epoch_ and the LIVE sealed_ remain zero/false throughout.
    }
}
#endif
