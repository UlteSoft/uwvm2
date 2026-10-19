// PRIVATE threaded status EH. Included after tuple layout / handler recording.
// Reuses actual fused-validator lexical handlers and PHIs. No native invoke,
// C++ throw/longjmp/personality, ordinary pending experiment or global registry.
template<auto Native>
[[nodiscard]] inline ::llvm::CallInst* threaded_eh_call(
    runtime_local_func_llvm_jit_emit_state_t& state,::llvm::FunctionType* type,
    ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
{
    auto& b=*state.ir_builder;
    auto const target=get_llvm_runtime_bridge_function_symbol_value<Native>(b,type);
    if(target==nullptr)return nullptr;
    auto const call=apply_llvm_jit_host_calling_conv(b.CreateCall(type,target,arguments));
    call->setDoesNotThrow();return call;
}

[[nodiscard]] inline bool threaded_eh_catch_values(runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_exception_handler_t const& handler,::llvm::Value* token,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t>& values) noexcept
{
    using abi=::uwvm2::runtime::lib::threaded_exception_bridge_api;
    auto& b=*state.ir_builder;auto word=b.getIntNTy(sizeof(::std::uintptr_t)*CHAR_BIT);
    auto count=get_runtime_block_result_count(handler.params);
    if(handler.with_reference&&(count==0u||get_runtime_wasm_value_type_encoding(handler.params.begin[count-1u])!=0x69u))return false;
    auto const payload_count=count-static_cast<::std::size_t>(handler.with_reference);
    if(handler.catch_all&&payload_count!=0u)return false;
    ::std::size_t bytes{};bool reference{};
    if(!llvm_jit_exception_tuple_layout(handler.params,payload_count,bytes,reference))return false;
    auto address=static_cast<::llvm::Value*>(::llvm::ConstantInt::get(word,0u));
    ::llvm::AllocaInst* buffer{};
    if(bytes!=0u)
    {
        buffer=create_llvm_jit_entry_block_alloca(b,b.getInt8Ty(),::llvm::ConstantInt::get(word,bytes),"threaded.catch.tuple");
        if(buffer==nullptr)return false;
        address=b.CreatePtrToInt(buffer,word);
    }
    if(!handler.catch_all)
    {
        auto type=::llvm::FunctionType::get(b.getVoidTy(),{word,word,word,word},false);
        if(threaded_eh_call<&abi::copy>(state,type,{token,::llvm::ConstantInt::get(word,handler.tag_index),
            address,::llvm::ConstantInt::get(word,bytes)})==nullptr)return false;
    }
    values.reserve(count);::std::size_t offset{};
    for(::std::size_t i{};i<payload_count;++i)
    {
        auto const type=handler.params.begin[i];auto llvm_type=get_llvm_type_from_wasm_value_type(b.getContext(),type);
        auto const width=get_runtime_wasm_value_type_abi_size(type);
        if(llvm_type==nullptr||width>bytes-offset)return false;
        auto field=b.CreateInBoundsGEP(b.getInt8Ty(),buffer,::llvm::ConstantInt::get(word,offset));
        auto carrier=get_llvm_jit_scalar_bits_type(llvm_type);auto loaded=b.CreateLoad(carrier,field);
        loaded->setAlignment(::llvm::Align{1u});
        auto value=carrier==llvm_type?static_cast<::llvm::Value*>(loaded):b.CreateBitCast(loaded,llvm_type);
        values.push_back({.type=type,.value=value});offset+=width;
    }
    if(offset!=bytes)return false;
    if(handler.with_reference)
    {
        auto type=get_llvm_type_from_wasm_value_type(b.getContext(),static_cast<runtime_operand_stack_value_type>(0x69u));
        if(type==nullptr||!type->isIntegerTy(sizeof(::uwvm2::uwvm::runtime::storage::gc_reference)*CHAR_BIT))return false;
        auto slot=create_llvm_jit_entry_block_alloca(b,type,nullptr,"threaded.catch.reference");
        if(slot==nullptr)return false;
        b.CreateStore(::llvm::Constant::getNullValue(type),slot);
        auto bridge_type=::llvm::FunctionType::get(b.getVoidTy(),{word,word},false);
        if(threaded_eh_call<&abi::make_reference>(state,bridge_type,{token,b.CreatePtrToInt(slot,word)})==nullptr)return false;
        values.push_back({.type=static_cast<runtime_operand_stack_value_type>(0x69u),.value=b.CreateLoad(type,slot)});
    }
    return values.size()==count;
}

[[nodiscard]] inline bool emit_threaded_exception_route(runtime_local_func_llvm_jit_emit_state_t& state,
    bool lexical=true) noexcept
{
    using abi=::uwvm2::runtime::lib::threaded_exception_bridge_api;
    if(!state.valid||state.ir_builder==nullptr||state.local_func_storage_ptr==nullptr)return false;
    auto& b=*state.ir_builder;auto word=b.getIntNTy(sizeof(::std::uintptr_t)*CHAR_BIT);
    auto pending_type=::llvm::FunctionType::get(word,{},false);
    auto token=threaded_eh_call<&abi::pending>(state,pending_type,{});
    if(token==nullptr)return false;
    auto f=b.GetInsertBlock()->getParent();
    auto normal=::llvm::BasicBlock::Create(b.getContext(),"threaded.eh.empty",f);
    auto exceptional=::llvm::BasicBlock::Create(b.getContext(),"threaded.eh.pending",f);
    b.CreateCondBr(b.CreateICmpEQ(token,::llvm::ConstantInt::get(word,0u)),normal,exceptional);
    b.SetInsertPoint(exceptional);
    bool terminal{};
    if(lexical)for(auto depth=state.control_stack.size();depth!=0u&&!terminal;--depth)
    {
        for(auto const& handler:state.control_stack.index_unchecked(depth-1u).exception_handlers)
        {
            ::llvm::BasicBlock* next{};
            if(!handler.catch_all)
            {
                auto type=::llvm::FunctionType::get(word,{word,word},false);
                auto matched=threaded_eh_call<&abi::matches>(state,type,{token,::llvm::ConstantInt::get(word,handler.tag_index)});
                if(matched==nullptr)return false;
                auto selected=::llvm::BasicBlock::Create(b.getContext(),"threaded.eh.selected",f);
                next=::llvm::BasicBlock::Create(b.getContext(),"threaded.eh.next",f);
                b.CreateCondBr(b.CreateICmpNE(matched,::llvm::ConstantInt::get(word,0u)),selected,next);b.SetInsertPoint(selected);
            }
            ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> values{};
            if(!threaded_eh_catch_values(state,handler,token,values))return false;
            auto consume_type=::llvm::FunctionType::get(b.getVoidTy(),{word},false);
            if(threaded_eh_call<&abi::consume>(state,consume_type,{token})==nullptr)return false;
            llvm_jit_branch_target_t target{.params=handler.params,.block=handler.block,.phis=handler.phis,
                .control_stack_index=handler.control_stack_index};
            if(!try_add_runtime_local_func_llvm_jit_branch_target_incoming(state,target,values,b.GetInsertBlock()))return false;
            b.CreateBr(target.block);
            if(handler.catch_all){terminal=true;break;}
            b.SetInsertPoint(next);
        }
    }
    if(!terminal)
    {
        // Caller detects the SAME immutable pending token before consuming
        // this carrier. Group stop is independent and wins at real stop probes.
        if(f->getReturnType()->isVoidTy())b.CreateRetVoid();
        else b.CreateRet(::llvm::Constant::getNullValue(f->getReturnType()));
    }
    b.SetInsertPoint(normal);return true;
}

[[nodiscard]] inline bool emit_threaded_throw_tuple(runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::size_t tag,runtime_block_result_type params) noexcept
{
    using abi=::uwvm2::runtime::lib::threaded_exception_bridge_api;
    if(!state.valid||state.ir_builder==nullptr||state.control_stack.empty())return false;
    if(!state.control_stack.back().is_reachable)return true;
    auto count=get_runtime_block_result_count(params);::std::size_t bytes{};bool refs{};
    if(state.operand_stack.size()<count||!llvm_jit_exception_tuple_layout(params,count,bytes,refs))return false;
    auto& b=*state.ir_builder;auto word=b.getIntNTy(sizeof(::std::uintptr_t)*CHAR_BIT);
    ::llvm::AllocaInst* tuple{};::llvm::Value* address=::llvm::ConstantInt::get(word,0u);
    if(bytes!=0u)
    {
        tuple=create_llvm_jit_entry_block_alloca(b,b.getInt8Ty(),::llvm::ConstantInt::get(word,bytes),"threaded.throw.tuple");
        if(tuple==nullptr)return false;address=b.CreatePtrToInt(tuple,word);
    }
    auto first=state.operand_stack.size()-count;::std::size_t offset{};
    for(::std::size_t i{};i<count;++i)
    {
        auto const& value=state.operand_stack.index_unchecked(first+i);auto type=get_llvm_type_from_wasm_value_type(b.getContext(),params.begin[i]);
        auto width=get_runtime_wasm_value_type_abi_size(params.begin[i]);
        if(value.type!=params.begin[i]||value.value==nullptr||type==nullptr||value.value->getType()!=type||width>bytes-offset)return false;
        auto field=b.CreateInBoundsGEP(b.getInt8Ty(),tuple,::llvm::ConstantInt::get(word,offset));
        auto carrier=get_llvm_jit_scalar_bits_type(type);auto bits=carrier==type?value.value:b.CreateBitCast(value.value,carrier);
        b.CreateStore(bits,field)->setAlignment(::llvm::Align{1u});offset+=width;
    }
    if(offset!=bytes)return false;
    auto type=::llvm::FunctionType::get(b.getVoidTy(),{word,word,word,word},false);
    if(threaded_eh_call<&abi::publish>(state,type,{::llvm::ConstantInt::get(word,state.local_func_storage_ptr->module_id),
        ::llvm::ConstantInt::get(word,tag),address,::llvm::ConstantInt::get(word,bytes)})==nullptr)return false;
    if(!emit_threaded_exception_route(state))return false;
    // A valid publication is pending. The synthetic empty edge exists only
    // after a native invariant failure/stop, never as Guest continuation.
    if(b.GetInsertBlock()->getParent()->getReturnType()->isVoidTy())b.CreateRetVoid();
    else b.CreateRet(::llvm::Constant::getNullValue(b.GetInsertBlock()->getParent()->getReturnType()));
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);return true;
}

[[nodiscard]] inline bool emit_threaded_throw_reference(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    using abi=::uwvm2::runtime::lib::threaded_exception_bridge_api;
    if(!state.valid||state.ir_builder==nullptr||state.control_stack.empty())return false;
    if(!state.control_stack.back().is_reachable)return true;
    if(state.operand_stack.empty())return false;
    auto value=state.operand_stack.back();auto& b=*state.ir_builder;
    if(get_runtime_wasm_value_type_encoding(value.type)!=0x69u||value.value==nullptr)return false;
    auto type=get_llvm_type_from_wasm_value_type(b.getContext(),value.type);
    if(type==nullptr||value.value->getType()!=type||!type->isIntegerTy(sizeof(::uwvm2::uwvm::runtime::storage::gc_reference)*CHAR_BIT))return false;
    auto slot=create_llvm_jit_entry_block_alloca(b,type,nullptr,"threaded.throw.reference");
    if(slot==nullptr)return false;b.CreateStore(value.value,slot);
    auto word=b.getIntNTy(sizeof(::std::uintptr_t)*CHAR_BIT);
    auto bridge_type=::llvm::FunctionType::get(b.getVoidTy(),{word,word},false);
    if(threaded_eh_call<&abi::throw_reference>(state,bridge_type,{
        ::llvm::ConstantInt::get(word,state.local_func_storage_ptr->module_id),b.CreatePtrToInt(slot,word)})==nullptr)return false;
    if(!emit_threaded_exception_route(state))return false;
    if(b.GetInsertBlock()->getParent()->getReturnType()->isVoidTy())b.CreateRetVoid();
    else b.CreateRet(::llvm::Constant::getNullValue(b.GetInsertBlock()->getParent()->getReturnType()));
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);return true;
}
