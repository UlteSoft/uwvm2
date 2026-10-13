// Private staged-engine members. Backend role0/helpers and role3/raw adapters
// remain complete overlap claims; only real Wasm typed/resume declarations are
// expected endpoint DATA. Nothing here grants ASM access to the VM or host.
#pragma once
using native_budget_charge=bool (*)(void*,::std::size_t,::std::size_t) noexcept;
void* native_budget_owner_{};
native_budget_charge native_charge_{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
using native_endpoint_observer=details::pending_actual_native_endpoints;
::std::unique_ptr<native_endpoint_observer> native_endpoints_{};
::std::size_t native_endpoint_expected_{},native_endpoint_helpers_{},native_endpoint_claims_{};
bool native_endpoint_capture_qualified_{};

[[nodiscard]] bool prepare_native_endpoint_declarations(::llvm::Module& ir,::llvm::LLVMContext const& actual_context,
    ::std::vector<native_endpoint_observer::expected_function>& expected)
{
    auto const count=locals_.size();auto const imported=owner_.module()->imported_function_vec_storage.size();
    if(!owner_.matches_unpublished_file() || !profile_ || !native_budget_owner_ || !native_charge_ ||
       ir.getModuleFlag("uwvm.native.code-owner.table.version")!=nullptr ||
       ::std::addressof(ir.getContext())!=::std::addressof(actual_context) || generations_.size()!=count ||
       count>details::native_owner_object_graph::detail::max_rows/2u || imported>SIZE_MAX-count)
    { return false; }
    if(count==0u) { return true; }
    if(!native_charge_(native_budget_owner_,1u,sizeof(native_endpoint_observer)) ||
       !native_charge_(native_budget_owner_,count*2u,
           sizeof(native_endpoint_observer::expected_function)+sizeof(unsigned))) { return false; }
    expected.reserve(count*2u);
    for(auto& function:ir)
    {
        if(function.isDeclaration()) { continue; }
        if(function.getParent()!=::std::addressof(ir) || function.hasFnAttribute("uwvm.native.code-owner.role")) { return false; }
        function.addFnAttr("uwvm.native.code-owner.role","0");
    }
    for(::std::size_t local{};local<count;++local)
    {
        auto const index=imported+local;auto const& plan=locals_[local].checkpoint_plan;
        if(!plan || plan->get().module!=module_id_ || plan->get().function!=index ||
           plan->get().function_generation!=generations_[local] || plan->get().resume_abi_revision!=2u ||
           plan->get().resume_sites.empty() || plan->get().profile.get()!=profile_.get() ||
           plan->get().profile.owner_before(profile_) || profile_.owner_before(plan->get().profile)) { return false; }
        auto const typed_name=get_runtime_llvm_jit_wasm_function_name(*owner_.module(),index);
        auto const raw_name=get_runtime_llvm_jit_wasm_raw_function_name(*owner_.module(),index);
        auto const ref=[](auto const& name) { return ::llvm::StringRef{reinterpret_cast<char const*>(name.data()),name.size()}; };
        auto* typed=ir.getFunction(ref(typed_name));auto* raw=ir.getFunction(ref(raw_name));
        if(!typed || !raw || typed==raw || typed->isDeclaration() || raw->isDeclaration() ||
           typed->getParent()!=::std::addressof(ir) || raw->getParent()!=::std::addressof(ir)) { return false; }
        auto const actual_name=typed->getName();
        constexpr ::std::size_t suffix=sizeof(".checkpoint.resume.v2")-1u;
        if(actual_name.empty() || actual_name.contains('\0') ||
           actual_name.size()>details::native_owner_table_format::max_name_bytes-suffix ||
           !native_charge_(native_budget_owner_,actual_name.size()+suffix+4u,8u)) { return false; }
        auto original=::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{actual_name.data(),actual_name.size()});
        auto resume_name=::fast_io::concat_std(original,".checkpoint.resume.v2");
        auto* resume=ir.getFunction(resume_name);auto* adapter=ir.getFunction(::fast_io::concat_std(resume_name,".raw"));
        if(!resume || !adapter || resume==typed || resume==raw || resume==adapter || resume->isDeclaration() || adapter->isDeclaration() ||
           resume->getParent()!=::std::addressof(ir) || adapter->getParent()!=::std::addressof(ir) ||
           resume->getFunctionType()!=typed->getFunctionType() || resume->getCallingConv()!=typed->getCallingConv() ||
           resume->getLinkage()!=::llvm::GlobalValue::InternalLinkage) { return false; }
        typed->addFnAttr("uwvm.native.code-owner.role","1");raw->addFnAttr("uwvm.native.code-owner.role","3");
        resume->addFnAttr("uwvm.native.code-owner.role","2");adapter->addFnAttr("uwvm.native.code-owner.role","3");
        expected.push_back({::std::move(original),module_id_,index,generations_[local],1u,plan});
        expected.push_back({::std::move(resume_name),module_id_,index,generations_[local],2u,plan});
    }
    ir.addModuleFlag(::llvm::Module::Error,"uwvm.native.code-owner.table.version",
        ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(ir.getContext()),2u));
    return expected.size()==count*2u;
}

[[nodiscard]] bool freeze_native_endpoint_capture() noexcept
{
    if(!owner_.matches_unpublished_file() || !engine_ || engine_->hasError() ||
       !debug_cfi_manager_ || debug_cfi_manager_->has_finalization_failure()) { return false; }
    if(locals_.empty()) { native_endpoint_capture_qualified_=true;return !native_endpoints_; }
    if(!native_endpoints_) { return false; }
    auto& observed=*native_endpoints_;
    if(!observed.valid_ || !observed.staged_only_ || observed.staged_frozen_ || observed.sealed_ ||
       !observed.attached_ || observed.engine_!=engine_.get() || observed.code_publication_!=this ||
       observed.runtime_epoch_!=0u || observed.module_id_!=module_id_ || observed.module_!=owner_.module() ||
       observed.source_.get()!=owner_.source().get() || observed.source_.owner_before(owner_.source()) ||
       owner_.source().owner_before(observed.source_) || observed.staged_profile_.get()!=profile_.get() ||
       observed.staged_profile_.owner_before(profile_) || profile_.owner_before(observed.staged_profile_) ||
       observed.objects_.empty() || observed.expected_.size()!=locals_.size()*2u ||
       observed.seen_.size()!=observed.expected_.size() || observed.bodies_.size()<observed.expected_.size()) { return false; }
    for(auto const seen:observed.seen_) { if(seen!=1u) { return false; } }
    ::std::sort(observed.bodies_.begin(),observed.bodies_.end(),[](auto const& a,auto const& b){ return a.code_begin<b.code_begin; });
    auto const imported=owner_.module()->imported_function_vec_storage.size();
    for(::std::size_t index{};index<observed.bodies_.size();++index)
    {
        auto const& body=observed.bodies_[index];
        if(body.code_begin==0u || body.code_end<=body.code_begin || body.role>3u ||
           (index!=0u && observed.bodies_[index-1u].code_end>body.code_begin)) { return false; }
        if(body.role!=1u && body.role!=2u) { continue; } // helpers retained, never granted Wasm context
        if(body.expected_index>=observed.expected_.size()) { return false; }
        auto const& expected=observed.expected_[body.expected_index];
        if(expected.function<imported || expected.function-imported>=entries_.size()) { return false; }
        auto const local=expected.function-imported;auto const& plan=locals_[local].checkpoint_plan;
        if(expected.module!=module_id_ || expected.role!=body.role || expected.generation!=generations_[local] ||
           expected.plan.get()!=plan.get() || expected.plan.owner_before(plan) || plan.owner_before(expected.plan)) { return false; }
        auto const callable=body.role==1u?entries_[local].typed:entries_[local].resume_typed;
        if(callable==0u || body.callable_entry!=callable ||
           details::native_function_code_address(callable)!=body.code_begin) { return false; }
    }
    native_endpoint_expected_=observed.expected_.size();native_endpoint_helpers_=observed.bodies_.size()-native_endpoint_expected_;
    native_endpoint_claims_=observed.total_symbols_;
    observed.detach();observed.staged_frozen_=true;observed.staged_budget_owner_=nullptr;observed.staged_charge_=nullptr;
    native_endpoint_capture_qualified_=true;
    return true; // LIVE sealed_ remains false, epoch remains zero; no native lookup/admission
}
#endif
