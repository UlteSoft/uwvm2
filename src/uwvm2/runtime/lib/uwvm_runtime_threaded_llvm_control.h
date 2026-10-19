// ThreadedLlvmChild private member fragment. No ordinary compiler option/IR.
        template<typename Fn>[[nodiscard]] static ::std::uintptr_t address(Fn fn) noexcept
        {return reinterpret_cast<::std::uintptr_t>(fn);}
        [[nodiscard]] bool add_external(::llvm::Function& f,::std::uintptr_t pointer)
        {
            if(pointer==0u||!f.isDeclaration()||f.isIntrinsic()||f.getName().size()>256u||externals_.size()>=256u)return false;
            for(auto const& old:externals_)if(old.name==f.getName())return old.address==pointer;
            externals_.push_back({f.getName().str(),pointer});return true;
        }
        [[nodiscard]] bool bind_emitted_bridges()
        {
            namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            for(auto const& bridge:emitted_bridges_)
            {
                if(bridge.declaration==nullptr)return false;
                ::std::uintptr_t replacement{};
                if(bridge.original==address(&llvm_jit_runtime_trap))replacement=address(&trap);
                else if(bridge.original==address(&llvm_jit_memory_out_of_bounds_trap))replacement=address(&memory_trap);
                else if(bridge.original==address(&details::llvm_jit_call_raw_from_generated_wasm_abi_bridge))replacement=address(&import_call);
                else if(bridge.original==address(&emit::llvm_jit_atomic_wait_notify_bridge<0u>))replacement=address(&wait_notify<0u>);
                else if(bridge.original==address(&emit::llvm_jit_atomic_wait_notify_bridge<1u>))replacement=address(&wait_notify<1u>);
                else if(bridge.original==address(&emit::llvm_jit_atomic_wait_notify_bridge<2u>))replacement=address(&wait_notify<2u>);
                else if(bridge.original==address(&emit::llvm_jit_memory_copy_bridge))replacement=address(&bulk_copy);
                else if(bridge.original==address(&emit::llvm_jit_memory_fill_bridge))replacement=address(&bulk_fill);
                else if(bridge.original==address(&emit::llvm_jit_memory_grow_bridge))replacement=address(&memory_grow);
                else if(bridge.original==address(&emit::llvm_jit_memory_size_bridge))replacement=address(&memory_size);
                else if(bridge.original==address(&emit::llvm_jit_memory_init_bridge))replacement=address(&memory_init);
                else if(bridge.original==address(&emit::llvm_jit_data_drop_bridge))replacement=address(&data_drop);
                else if(bridge.original==address(&threaded_exception_bridge_api::pending))replacement=address(&threaded_exception_bridge_api::pending);
                else if(bridge.original==address(&threaded_exception_bridge_api::publish))replacement=address(&threaded_exception_bridge_api::publish);
                else if(bridge.original==address(&threaded_exception_bridge_api::matches))replacement=address(&threaded_exception_bridge_api::matches);
                else if(bridge.original==address(&threaded_exception_bridge_api::copy))replacement=address(&threaded_exception_bridge_api::copy);
                else if(bridge.original==address(&threaded_exception_bridge_api::make_reference))replacement=address(&threaded_exception_bridge_api::make_reference);
                else if(bridge.original==address(&threaded_exception_bridge_api::consume))replacement=address(&threaded_exception_bridge_api::consume);
                else if(bridge.original==address(&threaded_exception_bridge_api::throw_reference))replacement=address(&threaded_exception_bridge_api::throw_reference);
                // Fail CLOSED, including runtime-global table/ref adapters,
                // native EH/GC, provider callbacks and memory64. No grow-silently.
                // No original address is accepted just because it was emitted.
                if(replacement==0u||!add_external(*bridge.declaration,replacement))return false;
            }
            return true;
        }
        [[nodiscard]] static ::std::size_t native_function_index(::llvm::Function const& f,
            ::std::vector<entries> const& entries,::std::size_t imports) noexcept
        {
            for(::std::size_t i{};i<entries.size();++i)
                if(f.getName()==entries[i].typed_name||f.getName()==entries[i].raw_name)return imports+i;
            return SIZE_MAX;
        }
        static void return_carrier(::llvm::IRBuilder<>& b,::llvm::Function& f)
        {
            // This carrier is unreachable by successful guest continuation:
            // each caller tests group stop before consuming its call result.
            if(f.getReturnType()->isVoidTy())b.CreateRetVoid();
            else b.CreateRet(::llvm::Constant::getNullValue(f.getReturnType()));
        }
        [[nodiscard]] bool add_control(::llvm::Module& module)
        {
            namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            for(auto const& global:module.globals())if(global.isThreadLocal())return false;
            auto& c=module.getContext();auto* word=::llvm::Type::getIntNTy(c,sizeof(::std::uintptr_t)*CHAR_BIT);
            auto* poll_type=::llvm::FunctionType::get(word,{word},false);
            auto* leave_type=::llvm::FunctionType::get(::llvm::Type::getVoidTy(c),{},false);
            auto declare=[&](char const* name,::llvm::FunctionType* type,::std::uintptr_t target)->::llvm::Function*
            {
                if(module.getNamedValue(name)!=nullptr)return nullptr;
                auto* f=::llvm::Function::Create(type,::llvm::Function::ExternalLinkage,name,module);
                f->addFnAttr(::llvm::Attribute::NoUnwind);f->addFnAttr(::llvm::Attribute::NoInline);
                f->addFnAttr(::llvm::Attribute::Convergent);f->addFnAttr(::llvm::Attribute::NoDuplicate);
                emit::apply_llvm_jit_host_calling_conv(*f);
                return add_external(*f,target)?f:nullptr;
            };
            auto* poll_fn=declare("uwvm.threaded.poll.v1",poll_type,address(&poll));
            auto* enter_fn=declare("uwvm.threaded.enter.v1",poll_type,address(&enter));
            auto* leave_fn=declare("uwvm.threaded.leave.v1",leave_type,address(&leave));
            if(poll_fn==nullptr||enter_fn==nullptr||leave_fn==nullptr)return false;
            for(auto& f:module)
            {
                if(f.isDeclaration())continue;
                auto const function=native_function_index(f,entries_,imports_);if(function==SIZE_MAX)return false;
                if(f.hasPersonalityFn()||f.isVarArg())return false;
                ::std::vector<::llvm::CallInst*> calls;
                ::std::vector<::llvm::BasicBlock*> blocks;
                ::std::vector<::llvm::CallInst*> traps;
                for(auto& block:f)
                {
                    blocks.push_back(::std::addressof(block));
                    for(auto& i:block)
                    {
                        if(::llvm::isa<::llvm::InvokeInst,::llvm::ResumeInst,::llvm::LandingPadInst,::llvm::CatchSwitchInst,
                            ::llvm::CatchReturnInst,::llvm::CleanupReturnInst,::llvm::IndirectBrInst,::llvm::CallBrInst>(i))return false;
                        auto* call=::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(i));if(call==nullptr)continue;
                        auto* target=call->getCalledFunction();
                        if(call->isInlineAsm())
                        {
                            auto* a=::llvm::cast<::llvm::InlineAsm>(call->getCalledOperand());
                            if(!a->getAsmString().empty()||a->getConstraintString()!="~{memory}")return false;
                            continue;
                        }
                        if(target!=nullptr&&target->isIntrinsic())
                        {
                            if(target->getIntrinsicID()==::llvm::Intrinsic::trap||target->getIntrinsicID()==::llvm::Intrinsic::debugtrap)return false;
                            // LLVM intrinsics lower to finite scalar/vector/memory
                            // work. External libcalls are separately checked in the
                            // actual generated object before code loading.
                        }
                        bool is_trap{};
                        for(auto const& bridge:emitted_bridges_)if(bridge.declaration==target&&
                            (bridge.original==address(&llvm_jit_runtime_trap)||bridge.original==address(&llvm_jit_memory_out_of_bounds_trap)))is_trap=true;
                        if(is_trap)traps.push_back(call);else calls.push_back(call);
                    }
                }
                // Replace the original fatal-only unreachable suffix. We retain
                // its exact trap kind and call arguments; it now records group
                // outcome and returns through actual native frames.
                for(auto* call:traps)
                {
                    if(!call->getType()->isVoidTy())return false;
                    auto* block=call->getParent();auto it=call->getIterator();++it;
                    while(it!=block->end())
                    {
                        auto& dead=*it++;
                        if(!dead.use_empty())return false;
                        dead.eraseFromParent();
                    }
                    call->removeFnAttr(::llvm::Attribute::NoReturn);
                    ::llvm::IRBuilder<> b(block);return_carrier(b,f);
                }
                auto probe=[&](::llvm::Instruction* before,::llvm::Function* probe_fn)
                {
                    auto* block=before->getParent();
                    auto* continued=block->splitBasicBlock(before->getIterator(),"threaded.cont");
                    block->getTerminator()->eraseFromParent();
                    auto* cancelled=::llvm::BasicBlock::Create(c,"threaded.stop",::std::addressof(f),continued);
                    ::llvm::IRBuilder<> b(block);
                    auto* observed=emit::apply_llvm_jit_host_calling_conv(b.CreateCall(poll_type,probe_fn,
                        {::llvm::ConstantInt::get(word,function)}));
                    observed->setTailCallKind(::llvm::CallInst::TCK_NoTail);
                    b.CreateCondBr(b.CreateICmpNE(observed,::llvm::ConstantInt::get(word,0u)),cancelled,continued);
                    b.SetInsertPoint(cancelled);return_carrier(b,f);
                };
                for(auto* block:blocks)
                {
                    auto first=block->getFirstInsertionPt();if(first==block->end())return false;
                    probe(::std::addressof(*first),block==::std::addressof(f.getEntryBlock())?enter_fn:poll_fn);
                }
                for(auto* call:calls)
                {
                    if(call->isMustTailCall())continue;
                    auto* next=call->getNextNode();if(next==nullptr)return false;
                    // Tail hint becomes ordinary call because the stop check must
                    // precede SSA use / memory effects. musttail stays untouched.
                    call->setTailCallKind(::llvm::CallInst::TCK_NoTail);probe(next,poll_fn);
                }
                for(auto& block:f)if(auto* ret=::llvm::dyn_cast<::llvm::ReturnInst>(block.getTerminator()))
                {
                    auto* before=static_cast<::llvm::Instruction*>(ret);
                    auto* previous=ret->getPrevNode();auto* tail=::llvm::dyn_cast_or_null<::llvm::CallInst>(previous);
                    if(tail!=nullptr&&tail->isMustTailCall())before=tail;
                    ::llvm::IRBuilder<> b(before);emit::apply_llvm_jit_host_calling_conv(b.CreateCall(leave_type,leave_fn,{}));
                }
            }
            return !::llvm::verifyModule(module);
        }
        [[nodiscard]] static bool prove_stop_cycles(::llvm::Module const& module)
        {
            // After the actual optimizer: remove blocks with real poll/enter
            // calls. Kahn's algorithm must consume the remaining entire CFG,
            // including unreachable / irreducible cycles, without recursion.
            for(auto const& f:module)
            {
                if(f.isDeclaration())continue;
                ::std::vector<::llvm::BasicBlock const*> nodes;
                for(auto const& block:f)
                {
                    bool has_probe{};
                    for(auto const& i:block)if(auto const* call=::llvm::dyn_cast<::llvm::CallInst>(&i))
                        if(auto const* target=call->getCalledFunction())
                            has_probe|=target->getName()=="uwvm.threaded.poll.v1"||target->getName()=="uwvm.threaded.enter.v1";
                    if(!has_probe)nodes.push_back(&block);
                }
                ::llvm::DenseMap<::llvm::BasicBlock const*,::std::size_t> degree;
                for(auto* block:nodes)degree.try_emplace(block,0u);
                for(auto* block:nodes)for(auto* successor : ::llvm::successors(block))
                    if(auto it=degree.find(successor);it!=degree.end())++it->second;
                ::std::vector<::llvm::BasicBlock const*> ready;
                for(auto const& value:degree)if(value.second==0u)ready.push_back(value.first);
                ::std::size_t consumed{};
                while(!ready.empty())
                {
                    auto* block=ready.back();ready.pop_back();++consumed;
                    for(auto* successor : ::llvm::successors(block))if(auto it=degree.find(successor);it!=degree.end())
                        if(--it->second==0u)ready.push_back(successor);
                }
                if(consumed!=nodes.size())return false;
            }
            return true;
        }
        template<bool Fill>
        static void bulk(::std::uintptr_t address,::std::int32_t dst_bits,::std::int32_t source_or_value,::std::int32_t length_bits) noexcept
        {
            auto* memory=actual_memory(address);if(memory==nullptr||stopped())return;
            namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            auto const dst=static_cast<::std::uint32_t>(dst_bits),src=static_cast<::std::uint32_t>(source_or_value);
            auto const len=static_cast<::std::uint32_t>(length_bits);
            // One stable native mmap snapshot; no provider callback and no guest
            // pointer survives the scope. All bounds precede the first write.
            emit::llvm_jit_with_bulk_memory_snapshot(*memory,[&](::std::byte* begin,::std::size_t size) noexcept
            {
                if(dst>size||len>size-dst||(!Fill&&(src>size||len>size-src))||(len!=0u&&begin==nullptr))
                {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::memory_out_of_bounds));return false;}
                ::std::size_t remaining=len;
                while(remaining!=0u&&!stopped())
                {
                    auto const count=(::std::min)(remaining,::std::size_t{4096u});
                    // Backward chunking preserves memmove overlap semantics.
                    auto const offset=(!Fill&&dst>src&&dst-src<len)?remaining-count:len-remaining;
                    for(::std::size_t j{};j<count;++j)
                    {
                        auto const within=(!Fill&&dst>src&&dst-src<len)?count-1u-j:j;
                        auto const at=offset+within;
                        unsigned char value{};
                        if constexpr(Fill)value=static_cast<unsigned char>(src);
                        else value=__atomic_load_n(reinterpret_cast<unsigned char const*>(begin+src+at),__ATOMIC_RELAXED);
                        __atomic_store_n(reinterpret_cast<unsigned char*>(begin+dst+at),value,__ATOMIC_RELAXED);
                    }
                    remaining-=count;
                }
                return true;
            });
        }
        static void bulk_copy(::std::uintptr_t a,::std::int32_t d,::std::int32_t s,::std::int32_t n) noexcept{bulk<false>(a,d,s,n);}
        static void bulk_fill(::std::uintptr_t a,::std::int32_t d,::std::int32_t v,::std::int32_t n) noexcept{bulk<true>(a,d,v,n);}

        static void data_drop(::std::uintptr_t module_address,::std::uint32_t index) noexcept
        {
            auto* e=active_;if(e==nullptr||stopped())return;
            if(module_address!=reinterpret_cast<::std::uintptr_t>(e->child->module_)||
               index>=e->child->module_->local_defined_data_vec_storage.size())
            {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return;}
            // The World interface is const metadata; this exact runtime instance
            // owns mutable drop state. No parent instance or parser is selected.
            auto& module=*const_cast<module_type*>(e->child->module_);
            ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload(
                module.local_defined_data_vec_storage.index_unchecked(index).data);
        }
        static void memory_init(::std::uintptr_t memory_address,::std::uintptr_t module_address,::std::uint32_t index,
            ::std::int32_t destination_bits,::std::int32_t source_bits,::std::int32_t length_bits) noexcept
        {
            auto* e=active_;auto* memory=actual_memory(memory_address);if(e==nullptr||memory==nullptr||stopped())return;
            if(module_address!=reinterpret_cast<::std::uintptr_t>(e->child->module_)||
               index>=e->child->module_->local_defined_data_vec_storage.size())
            {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return;}
            auto const payload=::uwvm2::uwvm::runtime::storage::load_wasm_data_segment_payload(
                e->child->module_->local_defined_data_vec_storage.index_unchecked(index).data);
            auto const* file=e->child->world_->file(0u);
            auto const base=reinterpret_cast<::std::uintptr_t>(file->source_cbegin());
            auto const begin=reinterpret_cast<::std::uintptr_t>(payload.byte_begin),end=reinterpret_cast<::std::uintptr_t>(payload.byte_end);
            if((begin==0u)!=(end==0u)||(begin!=0u&&(begin<base||end<begin||end-base>file->source_size())))
            {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return;}
            auto const bytes=begin==0u?0u:end-begin;
            auto const destination=static_cast<::std::uint32_t>(destination_bits),source=static_cast<::std::uint32_t>(source_bits);
            auto const length=static_cast<::std::uint32_t>(length_bits);
            if(source>bytes||length>bytes-source)
            {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::memory_out_of_bounds));return;}
            namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            emit::llvm_jit_with_bulk_memory_snapshot(*memory,[&](::std::byte* target,::std::size_t size) noexcept
            {
                if(destination>size||length>size-destination||(length!=0u&&target==nullptr))
                {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::memory_out_of_bounds));return false;}
                ::std::size_t at{};
                while(at<length&&!stopped())
                {
                    auto const chunk=(::std::min)(::std::size_t{4096u},length-at);
                    for(::std::size_t j{};j<chunk;++j)
                        __atomic_store_n(reinterpret_cast<unsigned char*>(target+destination+at+j),
                            ::std::to_integer<unsigned char>(payload.byte_begin[source+at+j]),__ATOMIC_RELAXED);
                    at+=chunk;
                }
                return true;
            });
        }
