// Synchronous compiler-owned status CFG. The common constant trap targets
// carry no status SSA out of a loop: one large PHI would make LCSSA quadratic.
// Debug locations retain the original per-opcode classifier and direct trap.
class llvm_jit_gc_status_dispatch_plan
{
    ::llvm::IRBuilder<>* builder_{};
    ::llvm::FunctionType* trap_type_{};
    ::llvm::Value* trap_callee_{};
    ::llvm::BasicBlock* ok_{};
    ::llvm::BasicBlock* error_{};
    ::llvm::BasicBlock* errors_[4]{};
    bool owned_errors_[4]{};
    ::llvm::IRBuilder<>::InsertPoint original_{};
    bool committed_{};
    bool shared_{};

    void emit_trap_body(::llvm::Value* kind, ::llvm::Value* callee) noexcept
    {
        auto& builder{*builder_};
        auto const intptr{::llvm::cast<::llvm::IntegerType>(trap_type_->getParamType(1u))};
        ::llvm::Value* arguments[]{kind, emit_llvm_jit_current_frame_address(builder, intptr),
            emit_llvm_jit_current_stack_pointer(builder, intptr)};
        auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(trap_type_, callee, arguments))};
        call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
        // Direct generated-frame call; explicit Win64 FP/SP and memory clobber
        // stay at the real cold trap. No C++ wrapper or tail call is introduced.
        emit_llvm_jit_memory_clobber(builder);
        builder.CreateUnreachable();
    }
    void emit_debug_error_body(::llvm::Value& status) noexcept
    {
        namespace storage = ::uwvm2::uwvm::runtime::storage;
        using trap_kind = ::uwvm2::runtime::lib::llvm_jit_trap_kind;
        auto& builder{*builder_};
        auto const type{::llvm::cast<::llvm::IntegerType>(status.getType())};
        builder.SetInsertPoint(error_);
        auto const failed{[&](storage::gc_object_status value)
        { return builder.CreateICmpEQ(::std::addressof(status),
            ::llvm::ConstantInt::get(type, static_cast<::std::uint_least64_t>(value))); }};
        auto const constant{[&](trap_kind value)
        { return ::llvm::ConstantInt::get(trap_type_->getParamType(0u), static_cast<::std::uint_least64_t>(value)); }};
        auto const exhausted{builder.CreateOr(failed(storage::gc_object_status::out_of_memory),
            failed(storage::gc_object_status::size_overflow))};
        auto const generic{builder.CreateSelect(exhausted,
            constant(trap_kind::gc_allocation_failure), constant(trap_kind::runtime_invariant_failure))};
        auto const bounds{builder.CreateSelect(failed(storage::gc_object_status::out_of_bounds),
            constant(trap_kind::array_out_of_bounds), generic)};
        auto const selected{builder.CreateSelect(failed(storage::gc_object_status::null_reference),
            constant(trap_kind::null_reference), bounds, get_llvm_string_ref(u8"gc.status.error.kind"))};
        emit_trap_body(selected, trap_callee_);
    }
public:
    llvm_jit_gc_status_dispatch_plan() noexcept = default;
    llvm_jit_gc_status_dispatch_plan(llvm_jit_gc_status_dispatch_plan const&) = delete;
    llvm_jit_gc_status_dispatch_plan& operator=(llvm_jit_gc_status_dispatch_plan const&) = delete;
    ~llvm_jit_gc_status_dispatch_plan()
    {
        if(builder_ == nullptr || committed_) { return; }
        // Preparation precedes the native call. Only freshly owned unused blocks
        // retire on failure; a previously committed shared target stays live.
        auto const current{builder_->GetInsertBlock()};
        bool restore{current == ok_ || current == error_};
        for(auto const block: errors_) { restore = restore || current == block; }
        if(restore) { builder_->restoreIP(original_); }
        for(::std::size_t i{}; i != 4uz; ++i)
        { if(owned_errors_[i]) { errors_[i]->eraseFromParent(); } }
        if(error_ != nullptr) { error_->eraseFromParent(); }
        ok_->eraseFromParent();
    }
    [[nodiscard]] bool prepare(::llvm::Module& module, ::llvm::IRBuilder<>& builder) noexcept
    {
        auto const block{builder.GetInsertBlock()};
        auto const function{block == nullptr ? nullptr : block->getParent()};
        if(builder_ != nullptr || block == nullptr || (!block->empty() && block->back().isTerminator()) ||
           function == nullptr || function->getParent() != ::std::addressof(module)) { return false; }
        auto const type{get_llvm_runtime_trap_bridge_function_type(builder.getContext())};
        if(type == nullptr) { return false; }
        builder_ = ::std::addressof(builder);
        trap_type_ = type;
        original_ = builder.saveIP();
        ok_ = ::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.status.ok"), function);
        shared_ = !builder.getCurrentDebugLocation();
        if(!shared_)
        {
            error_ = ::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.status.error"), function);
            builder.SetInsertPoint(error_);
            trap_callee_ = get_llvm_runtime_bridge_function_symbol_value<
                ::uwvm2::runtime::lib::llvm_jit_runtime_trap>(builder, type);
            builder.restoreIP(original_);
            return trap_callee_ != nullptr;
        }
        using trap_kind = ::uwvm2::runtime::lib::llvm_jit_trap_kind;
        ::llvm::StringRef const names[]{get_llvm_string_ref(u8"gc.status.error.invariant.shared"),
            get_llvm_string_ref(u8"gc.status.error.alloc.shared"),
            get_llvm_string_ref(u8"gc.status.error.bounds.shared"),
            get_llvm_string_ref(u8"gc.status.error.null.shared")};
        trap_kind const kinds[]{trap_kind::runtime_invariant_failure, trap_kind::gc_allocation_failure,
            trap_kind::array_out_of_bounds, trap_kind::null_reference};
        for(::std::size_t i{}; i != 4uz; ++i)
        {
            if(function->getValueSymbolTable() != nullptr)
            { errors_[i] = ::llvm::dyn_cast_or_null<::llvm::BasicBlock>(function->getValueSymbolTable()->lookup(names[i])); }
            if(errors_[i] != nullptr)
            {
                if(errors_[i]->getParent() != function || errors_[i]->empty() ||
                   !::llvm::isa<::llvm::UnreachableInst>(errors_[i]->back())) { return false; }
                continue;
            }
            errors_[i] = ::llvm::BasicBlock::Create(builder.getContext(), names[i], function);
            owned_errors_[i] = true;
            builder.SetInsertPoint(errors_[i]);
            auto const callee{get_llvm_runtime_bridge_function_symbol_value<
                ::uwvm2::runtime::lib::llvm_jit_runtime_trap>(builder, type)};
            if(callee == nullptr) { builder.restoreIP(original_); return false; }
            emit_trap_body(::llvm::ConstantInt::get(type->getParamType(0u),
                static_cast<::std::uint_least64_t>(kinds[i])), callee);
            builder.restoreIP(original_);
        }
        return true;
    }
    // Caller has already released any temporary input buffer. No fallback may
    // repeat the native GC operation after this exact integer status is read.
    void commit(::llvm::Value& status) noexcept
    {
        namespace storage = ::uwvm2::uwvm::runtime::storage;
        auto& builder{*builder_};
        auto& context{builder.getContext()};
        auto const type{::llvm::cast<::llvm::IntegerType>(status.getType())};
        if(shared_)
        {
            auto const branch{builder.CreateSwitch(::std::addressof(status), errors_[0], 5u)};
            auto const constant{[&](storage::gc_object_status value)
            { return ::llvm::ConstantInt::get(type, static_cast<::std::uint_least64_t>(value)); }};
            branch->addCase(::llvm::ConstantInt::get(type, 0u), ok_);
            branch->addCase(constant(storage::gc_object_status::out_of_memory), errors_[1]);
            branch->addCase(constant(storage::gc_object_status::size_overflow), errors_[1]);
            branch->addCase(constant(storage::gc_object_status::out_of_bounds), errors_[2]);
            branch->addCase(constant(storage::gc_object_status::null_reference), errors_[3]);
            // Static branch preferences: default, success, OOM, size, bounds, null.
            branch->setMetadata(::llvm::LLVMContext::MD_prof, ::llvm::MDNode::get(context, {
                ::llvm::MDString::get(context, "branch_weights"),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(1u)),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(2000u)),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(1u)),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(1u)),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(1u)),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(1u))}));
        }
        else
        {
            auto const succeeded{builder.CreateICmpEQ(::std::addressof(status),
                ::llvm::ConstantInt::get(type, 0u), get_llvm_string_ref(u8"gc.status.succeeded"))};
            auto const branch{builder.CreateCondBr(succeeded, ok_, error_)};
            branch->setMetadata(::llvm::LLVMContext::MD_prof, ::llvm::MDNode::get(context, {
                ::llvm::MDString::get(context, "branch_weights"),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(2000u)),
                ::llvm::ConstantAsMetadata::get(builder.getInt32(1u))}));
            emit_debug_error_body(status);
        }
        committed_ = true;
        builder.SetInsertPoint(ok_);
    }
};
