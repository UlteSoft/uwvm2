// Included only in the real runtime TU, after actual anonymous record/registry
// definitions. A cold internal implementation; not a module/API export.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
namespace details
{
    [[nodiscard]] ::std::unique_ptr<native_owner_publication_bridge::captured>
    native_owner_publication_bridge::capture_actual_full(compiled_module_record& supplied,
        ::llvm::Module& actual_ir,::llvm::LLVMContext const& actual_context) noexcept
    {
        try
        {
        if(get_runtime_state_publication_depth()!=1u || !g_runtime.debug_pause_control ||
           ::std::addressof(actual_ir.getContext())!=::std::addressof(actual_context)) { return {}; }
        auto selected{g_runtime.modules.end()};
        for(auto it{g_runtime.modules.begin()};it!=g_runtime.modules.end();++it)
        {
            // [actual runtime registry begin ... it != end][same registry end]
            // [safe] actual element address compared before selecting/dereferencing
            // a caller-supplied record pointer; no numeric ID can create a record.
            if(::std::addressof(*it)==::std::addressof(supplied)) { selected=it;break; }
        }
        if(selected==g_runtime.modules.end()) { return {}; }
        auto& rec{*selected};auto* publication{rec.llvm_jit_full_publication.get()};
        auto const epoch{current_runtime_generation()};auto const* module{rec.runtime_module};
        if(publication==nullptr || module==nullptr || epoch==0u || rec.llvm_jit_debug_source_fused_epoch!=epoch ||
           !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(publication->source)) { return {}; }
        auto const id{find_runtime_module_id_from_storage_ptr(module)};
        if(id==SIZE_MAX || publication->source->bound_initialized_file(id,module)==nullptr) { return {}; }
        auto const count{module->local_defined_function_vec_storage.size()};
        auto const imported{module->imported_function_vec_storage.size()};
        if(count==0u || count>native_owner_object_graph::detail::max_rows || imported>SIZE_MAX-count ||
           rec.llvm_jit_compiled.local_funcs.size()!=count || rec.llvm_jit_debug_full_entry_generations.size()!=count ||
           rec.llvm_jit_debug_full_typed_entry_targets.size()!=count || publication->engine || publication->context ||
           actual_ir.getModuleFlag("uwvm.native.code-owner.table.version")!=nullptr) { return {}; }
        auto payload{::std::unique_ptr<captured>{new captured{}}};
        payload->source_=publication->source;payload->record_=::std::addressof(rec);payload->publication_=publication;
        payload->module_=module;payload->module_id_=id;payload->epoch_=epoch;
        // First mark every genuine surviving defined Function as a non-guest
        // helper. Backend records ALL MachineFunctions and exact true endpoints;
        // raw/helper overlap/unknown/zero aliases remain actual claim blockers.
        for(auto& function:actual_ir)
        {
            if(function.isDeclaration()) { continue; }
            if(function.getParent()!=::std::addressof(actual_ir) || function.hasFnAttribute("uwvm.native.code-owner.role")) { return {}; }
            function.addFnAttr("uwvm.native.code-owner.role","0");
        }
        for(::std::size_t local{};local<count;++local)
        {
            // [actual complete compiler/local/generation arrays0..count] end
            // [safe] local<count and equal complete extents established above
            // BEFORE borrowing immutable same-walk compiler plan or generation.
            auto const index{imported+local};auto const& compiled{rec.llvm_jit_compiled.local_funcs.index_unchecked(local)};
            auto const generation{rec.llvm_jit_debug_full_entry_generations[local]};
            if(generation==0u || compiled.module_id!=id || compiled.function_index!=index) { return {}; }
            auto const& plan{compiled.checkpoint_plan};
            if(plan && (plan->get().module!=id || plan->get().function!=index || plan->get().function_generation!=generation)) { return {}; }
            auto const name{get_runtime_llvm_jit_wasm_function_name(*module,index)};
            auto* typed{actual_ir.getFunction(::llvm::StringRef{reinterpret_cast<char const*>(name.data()), name.size()})};
            auto const raw_name{get_runtime_llvm_jit_wasm_raw_function_name(*module,index)};
            auto* raw{actual_ir.getFunction(::llvm::StringRef{reinterpret_cast<char const*>(raw_name.data()), raw_name.size()})};
            if(typed==nullptr || raw==nullptr || typed->isDeclaration() || raw->isDeclaration() ||
               typed->getParent()!=::std::addressof(actual_ir) || raw->getParent()!=::std::addressof(actual_ir) || typed==raw) { return {}; }
            typed->addFnAttr("uwvm.native.code-owner.role","1");raw->addFnAttr("uwvm.native.code-owner.role","3");
            auto const ir_name{typed->getName()};
            if(ir_name.empty() || ir_name.size()>native_owner_table_format::max_name_bytes || ir_name.contains('\0')) { return {}; }
            auto original{::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{ir_name.data(),ir_name.size()})};
            if(payload->expected_.size()==native_owner_object_graph::detail::max_rows) { return {}; }
            payload->expected_.push_back({::std::move(original),id,index,generation,1u,plan});
            if(!publication->checkpoint_profile) { continue; }
            if(!plan || !plan->get().profile || plan->get().profile.get()!=publication->checkpoint_profile.get() ||
               plan->get().profile.owner_before(publication->checkpoint_profile) ||
               publication->checkpoint_profile.owner_before(plan->get().profile)) { return {}; }
            if(plan->get().resume_abi_revision==0u) { continue; }
            if(plan->get().resume_abi_revision!=2u || plan->get().resume_sites.empty()) { return {}; }
            auto resume_name{::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{ir_name.data(),ir_name.size()},".checkpoint.resume.v2")};
            auto* resume{actual_ir.getFunction(resume_name)};auto* adapter{actual_ir.getFunction(::fast_io::concat_std(resume_name,".raw"))};
            if(resume==nullptr || adapter==nullptr || resume==typed || resume==raw || resume==adapter ||
               resume->isDeclaration() || adapter->isDeclaration() || resume->getParent()!=::std::addressof(actual_ir) ||
               adapter->getParent()!=::std::addressof(actual_ir) || resume->getFunctionType()!=typed->getFunctionType() ||
               resume->getCallingConv()!=typed->getCallingConv() || resume->getLinkage()!=::llvm::GlobalValue::InternalLinkage) { return {}; }
            resume->addFnAttr("uwvm.native.code-owner.role","2");adapter->addFnAttr("uwvm.native.code-owner.role","3");
            if(payload->expected_.size()==native_owner_object_graph::detail::max_rows) { return {}; }
            payload->expected_.push_back({::std::move(resume_name),id,index,generation,2u,plan});
        }
        if(payload->expected_.empty() || payload->expected_.size()>native_owner_object_graph::detail::max_rows) { return {}; }
        // Exact constant i32 version, only actual cold debug-full materializer.
        // This adds no guest instruction. Ordinary module/default modes unchanged.
        rec.actual_native_generation_positions.assign(count,SIZE_MAX);
        actual_ir.addModuleFlag(::llvm::Module::Error,"uwvm.native.code-owner.table.version",
            ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(actual_ir.getContext()),2u));
        return payload;
        }
        catch(...) { return {}; }
    }
    [[nodiscard]] ::std::unique_ptr<native_owner_publication_bridge::pending>
    native_owner_publication_bridge::observe_actual_full(::llvm::ExecutionEngine& engine,captured const& payload) noexcept
    {
        try
        {
        if(get_runtime_state_publication_depth()!=1u || payload.record_==nullptr || payload.publication_==nullptr ||
           payload.epoch_!=current_runtime_generation()) { return {}; }
        auto selected{g_runtime.modules.end()};
        for(auto it{g_runtime.modules.begin()};it!=g_runtime.modules.end();++it)
        { if(::std::addressof(*it)==payload.record_) { selected=it;break; } }
        if(selected==g_runtime.modules.end()) { return {}; }
        auto const& rec{*selected};auto const* publication{rec.llvm_jit_full_publication.get()};
        if(publication!=payload.publication_ || rec.runtime_module!=payload.module_ ||
           publication->source.get()!=payload.source_.get() || publication->source.owner_before(payload.source_) ||
           payload.source_.owner_before(publication->source) || publication->engine || publication->context) { return {}; }
        // Direct new is intentional: constructor is private to this actual TU
        // bridge. std::make_unique is not a friend and must not expose a factory.
        auto observer{::std::unique_ptr<pending>{new pending{engine,publication,payload.source_,payload.module_,
            payload.module_id_,payload.epoch_,payload.expected_}}};
        if(!observer->valid_ || !observer->attached_ || observer->engine_!=::std::addressof(engine)) { return {}; }
        return observer;
        }
        catch(...) { return {}; }
    }
    [[nodiscard]] bool native_owner_publication_bridge::seal_actual_full(compiled_module_record& supplied,
        captured const& payload,pending& observer) noexcept
    {
        if(get_runtime_state_publication_depth()!=1u || payload.epoch_!=current_runtime_generation() ||
           ::std::addressof(supplied)!=payload.record_) { return false; }
        auto selected{g_runtime.modules.end()};
        for(auto it{g_runtime.modules.begin()};it!=g_runtime.modules.end();++it)
        { if(::std::addressof(*it)==payload.record_) { selected=it;break; } }
        if(selected==g_runtime.modules.end()) { return false; }
        auto const& rec{*selected};auto const* publication{rec.llvm_jit_full_publication.get()};
        if(publication==nullptr || publication!=payload.publication_ || rec.runtime_module!=payload.module_ ||
           !publication->engine || publication->engine->hasError() || !publication->context ||
           publication->source.get()!=payload.source_.get() ||
           publication->source.owner_before(payload.source_) || payload.source_.owner_before(publication->source) ||
           publication->debug_source_runtime_epoch!=payload.epoch_ || rec.llvm_jit_debug_source_fused_epoch!=payload.epoch_ ||
           observer.code_publication_!=publication || observer.engine_!=publication->engine.get() ||
           observer.module_id_!=payload.module_id_ || observer.module_!=payload.module_ || observer.runtime_epoch_!=payload.epoch_) { return false; }
        auto const imported{payload.module_->imported_function_vec_storage.size()};
        auto const count{payload.module_->local_defined_function_vec_storage.size()};
        if(rec.llvm_jit_compiled.local_funcs.size()!=count || rec.llvm_jit_debug_full_entry_generations.size()!=count ||
           rec.llvm_jit_local_entry_addresses.size()!=count || rec.llvm_jit_local_raw_entry_addresses.size()!=count ||
           rec.llvm_jit_debug_full_typed_entry_targets.size()!=count) { return false; }
        for(::std::size_t i{};i<observer.expected_.size();++i)
        {
            auto const& expected{observer.expected_[i]};
            if(expected.module!=payload.module_id_ || expected.function<imported || expected.function-imported>=count) { return false; }
            auto const local{expected.function-imported};auto const& compiled{rec.llvm_jit_compiled.local_funcs.index_unchecked(local)};
            if(expected.generation!=rec.llvm_jit_debug_full_entry_generations[local] || compiled.module_id!=expected.module ||
               compiled.function_index!=expected.function || compiled.checkpoint_plan.get()!=expected.plan.get() ||
               compiled.checkpoint_plan.owner_before(expected.plan) || expected.plan.owner_before(compiled.checkpoint_plan)) { return false; }
        }
        // Verify every role1 actual callable entry against the actual engine-
        // resolved typed entry array. PPCv1 descriptor stays distinct from TEXT.
        for(auto const& body:observer.bodies_)
        {
            if(body.role!=1u) { continue; }
            if(body.expected_index>=observer.expected_.size()) { return false; }
            auto const& expected{observer.expected_[body.expected_index]};
            if(expected.function<imported || expected.function-imported>=count ||
               rec.llvm_jit_local_entry_addresses.index_unchecked(expected.function-imported)!=body.callable_entry) { return false; }
        }
        return observer.seal_actual_adopted_owner(*publication->engine,publication,payload.epoch_);
    }
    [[nodiscard]] ::std::unique_ptr<native_owner_publication_bridge::captured>
    native_owner_publication_bridge::capture_actual_replacement(compiled_module_record& supplied,
        llvm_jit_debug_replace_transaction& transaction,::llvm::Module& actual_ir,
        ::llvm::LLVMContext const& actual_context,
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t const& actual_local) noexcept
    {
        try
        {
            if(get_runtime_state_publication_depth()!=1u || !g_runtime.debug_pause_control ||
               ::std::addressof(actual_ir.getContext())!=::std::addressof(actual_context)) { return {}; }
            auto selected{g_runtime.modules.end()};
            for(auto it{g_runtime.modules.begin()};it!=g_runtime.modules.end();++it)
            { if(::std::addressof(*it)==::std::addressof(supplied)) { selected=it;break; } }
            if(selected==g_runtime.modules.end()) { return {}; }
            auto& rec{*selected};auto const* publication{rec.llvm_jit_full_publication.get()};
            auto const* module{rec.runtime_module};auto const epoch{current_runtime_generation()};
            if(publication==nullptr || module==nullptr || !publication->engine || !publication->context ||
               !publication->actual_native_endpoints || !publication->actual_native_endpoints->sealed_ ||
               !rec.llvm_jit_ready || transaction.committed || transaction.expected_runtime_epoch!=epoch ||
               transaction.expected_generation==UINT64_MAX || transaction.engine || transaction.context ||
               !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(transaction.source) ||
               transaction.source.get()!=publication->source.get() || transaction.source.owner_before(publication->source) ||
               publication->source.owner_before(transaction.source)) { return {}; }
            auto const id{find_runtime_module_id_from_storage_ptr(module)};
            if(id==SIZE_MAX || transaction.module_id!=id || transaction.source->actual_validated_file(id,epoch,module)==nullptr) { return {}; }
            auto const count{module->local_defined_function_vec_storage.size()},imported{module->imported_function_vec_storage.size()};
            if(transaction.local_index>=count || imported>SIZE_MAX-transaction.local_index ||
               transaction.function_index!=imported+transaction.local_index ||
               rec.llvm_jit_debug_full_entry_generations.size()!=count || rec.actual_native_generation_positions.size()!=count ||
               rec.llvm_jit_debug_full_entry_generations[transaction.local_index]!=transaction.expected_generation ||
               actual_local.module_id!=id || actual_local.function_index!=transaction.function_index ||
               actual_local.runtime_module_ptr!=module || actual_local.wasm_code_ptr!=::std::addressof(transaction.code) ||
               actual_local.function_type_ptr!=module->local_defined_function_vec_storage.index_unchecked(transaction.local_index).function_type_ptr ||
               actual_ir.getModuleFlag("uwvm.native.code-owner.table.version")!=nullptr) { return {}; }
            auto const body_begin{reinterpret_cast<::std::uintptr_t>(transaction.section_bytes.data())};
            auto const body_size{transaction.section_bytes.size()};
            auto const expression_begin{reinterpret_cast<::std::uintptr_t>(actual_local.code_begin)};
            auto const expression_end{reinterpret_cast<::std::uintptr_t>(actual_local.code_end)};
            // [actual transaction-owned parsed section][exact new expression]
            // [safe] complete owned extent checked BEFORE deriving its scalar
            // end; no original parser expression authenticates replacement code.
            if(body_begin==0u || body_size==0u || body_size>65542u || body_size>UINTPTR_MAX-body_begin ||
               expression_begin<body_begin || expression_end<=expression_begin || expression_end>body_begin+body_size ||
               expression_end-expression_begin!=transaction.debug_expression_size ||
               actual_local.code_begin!=reinterpret_cast<::std::byte const*>(transaction.code.body.expr_begin) ||
               actual_local.code_end!=reinterpret_cast<::std::byte const*>(transaction.code.body.code_end)) { return {}; }
            auto payload{::std::unique_ptr<captured>{new captured{}}};
            payload->record_=::std::addressof(rec);payload->publication_=publication;payload->replacement_=::std::addressof(transaction);
            payload->source_=transaction.source;payload->module_=module;payload->module_id_=id;payload->epoch_=epoch;
            payload->expression_begin_=expression_begin;payload->expression_end_=expression_end;
            auto const generation{transaction.expected_generation+1u};auto const plan{transaction.checkpoint_plan};
            if(plan && (plan->get().module!=id || plan->get().function!=transaction.function_index ||
               plan->get().function_generation!=generation)) { return {}; }
            for(auto& function:actual_ir)
            {
                if(function.isDeclaration()) { continue; }
                if(function.getParent()!=::std::addressof(actual_ir) || function.hasFnAttribute("uwvm.native.code-owner.role")) { return {}; }
                function.addFnAttr("uwvm.native.code-owner.role","0");
            }
            auto const name{get_runtime_llvm_jit_wasm_function_name(*module,transaction.function_index)};
            auto const raw_name{get_runtime_llvm_jit_wasm_raw_function_name(*module,transaction.function_index)};
            auto* typed{actual_ir.getFunction(::llvm::StringRef{reinterpret_cast<char const*>(name.data()), name.size()})};
            auto* raw{actual_ir.getFunction(::llvm::StringRef{reinterpret_cast<char const*>(raw_name.data()), raw_name.size()})};
            if(typed==nullptr || raw==nullptr || typed==raw || typed->isDeclaration() || raw->isDeclaration() ||
               typed->getParent()!=::std::addressof(actual_ir) || raw->getParent()!=::std::addressof(actual_ir)) { return {}; }
            auto const ir_name{typed->getName()};
            if(ir_name.empty() || ir_name.size()>native_owner_table_format::max_name_bytes || ir_name.contains('\0')) { return {}; }
            typed->addFnAttr("uwvm.native.code-owner.role","1");raw->addFnAttr("uwvm.native.code-owner.role","3");
            payload->expected_.push_back({::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{ir_name.data(),ir_name.size()}),
                id,transaction.function_index,generation,1u,plan});
            if(publication->checkpoint_profile)
            {
                if(!plan || !plan->get().profile || plan->get().profile.get()!=publication->checkpoint_profile.get() ||
                   plan->get().profile.owner_before(publication->checkpoint_profile) ||
                   publication->checkpoint_profile.owner_before(plan->get().profile)) { return {}; }
                if(plan->get().resume_abi_revision!=0u)
                {
                    if(plan->get().resume_abi_revision!=2u || plan->get().resume_sites.empty()) { return {}; }
                    auto resume_name{::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{ir_name.data(),ir_name.size()},".checkpoint.resume.v2")};
                    auto* resume{actual_ir.getFunction(resume_name)};auto* adapter{actual_ir.getFunction(::fast_io::concat_std(resume_name,".raw"))};
                    if(resume==nullptr || adapter==nullptr || resume==typed || resume==raw || resume==adapter ||
                       resume->isDeclaration() || adapter->isDeclaration() || resume->getParent()!=::std::addressof(actual_ir) ||
                       adapter->getParent()!=::std::addressof(actual_ir) || resume->getFunctionType()!=typed->getFunctionType() ||
                       resume->getCallingConv()!=typed->getCallingConv() || resume->getLinkage()!=::llvm::GlobalValue::InternalLinkage) { return {}; }
                    resume->addFnAttr("uwvm.native.code-owner.role","2");adapter->addFnAttr("uwvm.native.code-owner.role","3");
                    payload->expected_.push_back({::std::move(resume_name),id,transaction.function_index,generation,2u,plan});
                }
            }
            actual_ir.addModuleFlag(::llvm::Module::Error,"uwvm.native.code-owner.table.version",
                ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(actual_ir.getContext()),2u));
            return payload;
        }
        catch(...) { return {}; }
    }
    [[nodiscard]] ::std::unique_ptr<native_owner_publication_bridge::pending>
    native_owner_publication_bridge::observe_actual_replacement(llvm_jit_debug_replace_transaction& transaction,
        captured const& payload) noexcept
    {
        try
        {
            if(get_runtime_state_publication_depth()!=1u || payload.replacement_!=::std::addressof(transaction) ||
               transaction.committed || payload.epoch_!=current_runtime_generation() || !transaction.engine || !transaction.context ||
               transaction.source.get()!=payload.source_.get() || transaction.source.owner_before(payload.source_) ||
               payload.source_.owner_before(transaction.source) || transaction.expected_generation==UINT64_MAX) { return {}; }
            auto observer{::std::unique_ptr<pending>{new pending{*transaction.engine,::std::addressof(transaction),payload.source_,
                payload.module_,payload.module_id_,payload.epoch_,payload.expected_}}};
            if(!observer->valid_ || !observer->attached_) { return {}; }
            return observer;
        }
        catch(...) { return {}; }
    }
    [[nodiscard]] bool native_owner_publication_bridge::seal_actual_prepared_replacement(
        llvm_jit_debug_replace_transaction& transaction,captured const& payload,pending& observer) noexcept
    {
        if(get_runtime_state_publication_depth()!=1u || payload.replacement_!=::std::addressof(transaction) ||
           transaction.committed || !transaction.metadata_ready || !transaction.engine || !transaction.context ||
           transaction.expected_runtime_epoch!=payload.epoch_ || payload.epoch_!=current_runtime_generation() ||
           transaction.source.get()!=payload.source_.get() || transaction.source.owner_before(payload.source_) ||
           payload.source_.owner_before(transaction.source) || transaction.expected_generation==UINT64_MAX ||
           observer.engine_!=transaction.engine.get() || observer.code_publication_!=::std::addressof(transaction) ||
           reinterpret_cast<::std::uintptr_t>(transaction.code.body.expr_begin)!=payload.expression_begin_ ||
           reinterpret_cast<::std::uintptr_t>(transaction.code.body.code_end)!=payload.expression_end_) { return false; }
        for(auto const& expected:observer.expected_)
        {
            if(expected.module!=transaction.module_id || expected.function!=transaction.function_index ||
               expected.generation!=transaction.expected_generation+1u || expected.plan.get()!=transaction.checkpoint_plan.get() ||
               expected.plan.owner_before(transaction.checkpoint_plan) || transaction.checkpoint_plan.owner_before(expected.plan)) { return false; }
        }
        for(auto const& body:observer.bodies_)
        { if(body.role==1u && body.callable_entry!=transaction.typed_entry) { return false; } }
        return observer.seal_actual_adopted_owner(*transaction.engine,::std::addressof(transaction),payload.epoch_);
    }
    [[nodiscard]] bool native_owner_publication_bridge::qualifies_actual_replacement_for_commit(
        compiled_module_record const& rec,llvm_jit_debug_replace_transaction const& transaction) noexcept
    {
        if(get_runtime_state_publication_depth()!=1u || transaction.committed || !transaction.metadata_ready ||
           !transaction.engine || !transaction.context || !transaction.actual_native_endpoints ||
           transaction.expected_runtime_epoch!=current_runtime_generation() || transaction.expected_generation==UINT64_MAX ||
           transaction.module_id>=g_runtime.modules.size() ||
           ::std::addressof(g_runtime.modules.index_unchecked(transaction.module_id))!=::std::addressof(rec) ||
           !rec.llvm_jit_full_publication || rec.runtime_module==nullptr) { return false; }
        auto const& publication{*rec.llvm_jit_full_publication};auto const& image{*transaction.actual_native_endpoints};
        auto const count{rec.runtime_module->local_defined_function_vec_storage.size()};
        if(!image.valid_ || !image.sealed_ || image.attached_ || image.engine_!=transaction.engine.get() ||
           image.code_publication_!=::std::addressof(transaction) || image.module_!=rec.runtime_module ||
           image.module_id_!=transaction.module_id || image.runtime_epoch_!=transaction.expected_runtime_epoch ||
           image.source_.get()!=publication.source.get() || image.source_.owner_before(publication.source) ||
           publication.source.owner_before(image.source_) || transaction.source.get()!=image.source_.get() ||
           transaction.source.owner_before(image.source_) || image.source_.owner_before(transaction.source) ||
           rec.actual_native_generation_positions.size()!=count || transaction.local_index>=count ||
           rec.llvm_jit_debug_full_entry_generations.size()!=count ||
           rec.llvm_jit_debug_full_entry_generations[transaction.local_index]!=transaction.expected_generation ||
           image.source_->actual_validated_file(image.module_id_,image.runtime_epoch_,image.module_)==nullptr) { return false; }
        for(auto const& expected:image.expected_)
        {
            if(expected.module!=transaction.module_id || expected.function!=transaction.function_index ||
               expected.generation!=transaction.expected_generation+1u || expected.plan.get()!=transaction.checkpoint_plan.get() ||
               expected.plan.owner_before(transaction.checkpoint_plan) || transaction.checkpoint_plan.owner_before(expected.plan)) { return false; }
        }
        return true;
    }
    [[nodiscard]] bool native_owner_publication_bridge::resolve_actual_full(compiled_module_record const& supplied,
        ::std::size_t function,::std::uint_least64_t generation,::std::uintptr_t pc,
        ::std::uintptr_t& begin,::std::uintptr_t& end) noexcept
    {
        begin=end=0u;
        if(get_runtime_state_publication_depth()!=1u || generation==0u || pc==0u) { return false; }
        auto selected{g_runtime.modules.end()};
        for(auto it{g_runtime.modules.begin()};it!=g_runtime.modules.end();++it)
        { if(::std::addressof(*it)==::std::addressof(supplied)) { selected=it;break; } }
        if(selected==g_runtime.modules.end()) { return false; }
        auto const& rec{*selected};auto const* publication{rec.llvm_jit_full_publication.get()};
        if(publication==nullptr || !publication->engine || publication->engine->hasError() ||
           !publication->context || !rec.llvm_jit_ready ||
           !publication->actual_native_endpoints || !g_runtime.debug_pause_control ||
           !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(publication->source)) { return false; }
        auto const epoch{current_runtime_generation()};auto const* selected_image{publication->actual_native_endpoints.get()};
        auto const* selected_engine{publication->engine.get()};void const* selected_owner{publication};
        if(rec.runtime_module==nullptr) { return false; }
        auto const imported{rec.runtime_module->imported_function_vec_storage.size()};
        auto const count{rec.runtime_module->local_defined_function_vec_storage.size()};
        if(function<imported || function-imported>=count) { return false; }
        auto const local{function-imported};
        if(rec.actual_native_generation_positions.size()!=count || rec.llvm_jit_debug_full_entry_generations.size()!=count ||
           rec.llvm_jit_local_entry_addresses.size()!=count || rec.llvm_jit_local_raw_entry_addresses.size()!=count ||
           local>=rec.llvm_jit_compiled.local_funcs.size() || rec.llvm_jit_debug_full_entry_generations[local]!=generation) { return false; }
        if(generation!=1u)
        {
            auto const position{rec.actual_native_generation_positions[local]};
            if(position>=rec.llvm_jit_debug_full_retained_generations.size()) { return false; }
            // [actual owned retained generations0..N][checked real slot index]
            // [safe] select runtime-owned unique_ptr BEFORE its pointee. No
            // caller opaque token or saved address is cast/dereferenced here.
            auto const& actual{rec.llvm_jit_debug_full_retained_generations[position]};
            if(!actual || !actual->committed || !actual->metadata_ready || !actual->engine || !actual->context ||
               !actual->actual_native_endpoints || actual->expected_generation==UINT64_MAX ||
               actual->expected_generation+1u!=generation || actual->function_index!=function || actual->local_index!=local ||
               actual->expected_runtime_epoch!=epoch || actual->source.get()!=publication->source.get() ||
               actual->source.owner_before(publication->source) || publication->source.owner_before(actual->source) ||
               actual->typed_entry!=rec.llvm_jit_local_entry_addresses.index_unchecked(local) ||
               actual->raw_entry!=rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local)) { return false; }
            selected_image=actual->actual_native_endpoints.get();selected_engine=actual->engine.get();selected_owner=actual.get();
        }
        else if(rec.actual_native_generation_positions[local]!=SIZE_MAX) { return false; }
        if(selected_engine==nullptr || selected_engine->hasError()) { return false; }
        auto const& image{*selected_image};
        if(!image.valid_ || !image.sealed_ || image.attached_ || image.engine_!=selected_engine ||
           image.code_publication_!=selected_owner || image.module_!=rec.runtime_module || image.runtime_epoch_!=epoch ||
           publication->debug_source_runtime_epoch!=epoch || rec.llvm_jit_debug_source_fused_epoch!=epoch ||
           image.source_.get()!=publication->source.get() || image.source_.owner_before(publication->source) ||
           publication->source.owner_before(image.source_) ||
           image.source_->actual_validated_file(image.module_id_,epoch,image.module_)==nullptr) { return false; }
        auto const& compiled{rec.llvm_jit_compiled.local_funcs.index_unchecked(local)};
        auto found{::std::upper_bound(image.bodies_.begin(),image.bodies_.end(),pc,
            [](auto key,auto const& value){ return key<value.code_begin; })};
        if(found==image.bodies_.begin()) { return false; }
        // [same sealed sorted actual body array begin ... found<=end]
        // [safe] begin excluded before moving one element back. No live code
        // pointer or generated instruction is read by this scalar DATA join.
        --found;
        if(found->code_begin>pc || pc>=found->code_end || (found->role!=1u && found->role!=2u) ||
           found->expected_index>=image.expected_.size()) { return false; }
        auto const& expected{image.expected_[found->expected_index]};
        if(expected.role!=found->role || expected.module!=image.module_id_ || expected.function!=function ||
           expected.generation!=generation || expected.plan.get()!=compiled.checkpoint_plan.get() ||
           expected.plan.owner_before(compiled.checkpoint_plan) || compiled.checkpoint_plan.owner_before(expected.plan) ||
           compiled.module_id!=expected.module || compiled.function_index!=expected.function) { return false; }
        // Exact pre-target-trailer TEXT extent. Caller still proves actual
        // capture/frame/engine/gen/epoch/participant and every MC successor.
        // An unknown current replacement or restored epoch has no old fallback.
        begin=found->code_begin;end=found->code_end;return true;
    }
}
#endif
