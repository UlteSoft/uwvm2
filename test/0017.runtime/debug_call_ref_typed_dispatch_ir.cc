// Execute the actual IR emitter's native identity gate. These component DATA
// arrays are never production activation, module lifetime or code admission.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_call_ref_typed_dispatch.h>
}
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <atomic>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void require(bool condition, std::source_location line=std::source_location::current())
{ if(!condition) { std::fprintf(stderr,"typed reference IR failed line %u\n",line.line());std::abort(); } }
static void publish_after_observation(std::uintptr_t slot, std::uintptr_t entry)
{
    std::atomic_ref<std::uintptr_t>{*reinterpret_cast<std::uintptr_t*>(slot)}.store(entry,std::memory_order_release);
}
int main()
{
#if defined(__linux__) && defined(__x86_64__)
    namespace compiler=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace emit=compiler::details;
    namespace storage=uwvm2::uwvm::runtime::storage;
    namespace cp=uwvm2::runtime::checkpoint;
    require(!llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter());
    llvm::EngineBuilder select;select.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    std::unique_ptr<llvm::TargetMachine> target{select.selectTarget()};require(bool(target));
    llvm::LLVMContext context;
    auto ir=std::make_unique<llvm::Module>("actual-typed-reference-gate",context);
    ir->setTargetTriple(target->getTargetTriple());ir->setDataLayout(target->createDataLayout());
    storage::wasm_module_storage_t owner;
    std::array<storage::wasm_binfmt1_final_function_type_t,2> signatures{};
    using value_type=std::remove_cvref_t<decltype(*signatures[0].parameter.begin)>;
    std::array<value_type,1> parameter{value_type::i32};
    signatures[1].parameter={parameter.data(),parameter.data()+parameter.size()};
    owner.type_section_storage.type_section_begin=signatures.data();
    owner.type_section_storage.type_section_end=signatures.data()+signatures.size();
    owner.type_section_storage.type_section_count=signatures.size();
    owner.local_defined_function_vec_storage.push_back({.function_type_ptr=&signatures[0]});
    owner.local_defined_function_vec_storage.push_back({.function_type_ptr=&signatures[1]});
    owner.local_defined_function_vec_storage.push_back({});
    owner.local_defined_function_vec_storage.push_back({.function_type_ptr=&signatures[0]});
    owner.local_defined_function_vec_storage.push_back({.function_type_ptr=signatures.data()+signatures.size()});
    std::array<std::uintptr_t,5> published{0x1234000u,0x5678000u,0x9abc000u,0u,0xdef0000u};
    cp::function_plan plan;plan.profile=cp::compilation_profile::create_for_trusted_observer();
    compiler::local_func_storage_t local;local.runtime_module_ptr=&owner;
    emit::runtime_local_func_llvm_jit_emit_state_t state;
    state.llvm_context_holder=&context;state.llvm_module=ir.get();state.local_func_storage_ptr=&local;
    state.checkpoint_plan=&plan;state.debug_activation_enabled=true;
    state.debug_full_patchable_typed_target_base_address=reinterpret_cast<std::uintptr_t>(published.data());
    state.debug_full_patchable_typed_target_count=published.size();
    auto function=llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt64Ty(context),
        {llvm::Type::getInt64Ty(context),llvm::Type::getInt32Ty(context)},false),llvm::Function::ExternalLinkage,"probe",*ir);
    state.llvm_function=function;
    state.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(llvm::BasicBlock::Create(context,"entry",function));
    auto& builder=*state.ir_builder;
    auto value=builder.CreateOr(builder.CreateZExt(function->getArg(0),builder.getInt128Ty()),
        builder.CreateShl(builder.CreateZExt(function->getArg(1),builder.getInt128Ty()),64u));
    auto abi=emit::get_llvm_function_type_from_wasm_function_type(context,signatures[0]);
    auto entry=emit::emit_runtime_llvm_jit_local_reference_dispatch(state,value,0u,1u,abi);require(entry!=nullptr);
    builder.CreateRet(entry);
    require(!llvm::verifyModule(*ir,&llvm::errs()));
    unsigned calls{},atomic_loads{};
    for(auto& block:*function) for(auto& instruction:block)
    { calls+=llvm::isa<llvm::CallBase>(instruction);if(auto load=llvm::dyn_cast<llvm::LoadInst>(&instruction)) atomic_loads+=load->isAtomic(); }
    require(calls==0u && atomic_loads==1u);
    state.debug_activation_enabled=false;require(!emit::runtime_llvm_jit_local_reference_dispatch_selected(state));
    state.debug_activation_enabled=true;state.debug_full_patchable_typed_target_count=published.size()+1u;
    require(emit::emit_runtime_llvm_jit_local_reference_dispatch(state,value,0u,1u,abi)==nullptr);
    state.debug_full_patchable_typed_target_count=published.size();
    require(emit::emit_runtime_llvm_jit_local_reference_dispatch(state,value,UINT32_MAX,1u,abi)==nullptr);
    require(emit::emit_runtime_llvm_jit_local_reference_dispatch(state,value,0u,0u,abi)==nullptr);
    // Execute the emitted code across a real native publication callback.
    // The first entry value is intentionally NOT a cached call target.
    auto make_witness_probe=[&](char const* name,bool bypass)
    {
        auto fn=llvm::Function::Create(llvm::FunctionType::get(builder.getInt64Ty(),
            {builder.getInt64Ty(),builder.getInt32Ty(),builder.getInt64Ty(),builder.getInt1Ty()},false),
            llvm::Function::ExternalLinkage,name,*ir);
        state.llvm_function=fn;
        builder.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",fn));
        auto carrier=builder.CreateOr(builder.CreateZExt(fn->getArg(0),builder.getInt128Ty()),
            builder.CreateShl(builder.CreateZExt(fn->getArg(1),builder.getInt128Ty()),64u));
        auto checked=llvm::BasicBlock::Create(context,"checked",fn);
        auto unproved=llvm::BasicBlock::Create(context,"unproved",fn);
        if(bypass) builder.CreateCondBr(fn->getArg(3),checked,unproved);
        else builder.CreateBr(checked);
        builder.SetInsertPoint(checked);
        emit::llvm_jit_local_reference_dispatch_witness witness{};
        require(emit::emit_runtime_llvm_jit_local_reference_dispatch(state,carrier,0u,1u,abi,&witness)!=nullptr);
        auto joined=llvm::BasicBlock::Create(context,"joined",fn);
        if(bypass) builder.CreateBr(joined);
        else
        {
            // A genuine diamond must not unnecessarily reject reuse.
            auto left=llvm::BasicBlock::Create(context,"left",fn);
            auto right=llvm::BasicBlock::Create(context,"right",fn);
            builder.CreateCondBr(fn->getArg(3),left,right);
            builder.SetInsertPoint(left);builder.CreateBr(joined);
            builder.SetInsertPoint(right);builder.CreateBr(joined);
        }
        builder.SetInsertPoint(unproved);
        if(bypass) builder.CreateBr(joined);
        else builder.CreateUnreachable();
        builder.SetInsertPoint(joined);
        auto reload=[&](auto value,auto id,auto width,auto type,auto const& proof)
        { return emit::emit_runtime_llvm_jit_local_reference_dispatch_after_observation(state,value,id,width,type,proof); };
        emit::llvm_jit_local_reference_dispatch_witness empty{};
        require(reload(carrier,0u,1u,abi,empty)==nullptr);
        require(reload(carrier,1u,1u,abi,witness)==nullptr);
        require(reload(carrier,0u,2u,abi,witness)==nullptr);
        auto frozen=builder.CreateFreeze(carrier);
        require(frozen!=carrier && reload(frozen,0u,1u,abi,witness)==nullptr);
        auto wrong_abi=llvm::FunctionType::get(builder.getVoidTy(),{builder.getInt32Ty()},false);
        require(reload(carrier,0u,1u,wrong_abi,witness)==nullptr);
        state.debug_full_patchable_typed_target_count++;
        require(reload(carrier,0u,1u,abi,witness)==nullptr);
        state.debug_full_patchable_typed_target_count--;
        state.debug_full_patchable_typed_target_base_address+=sizeof(std::uintptr_t);
        require(reload(carrier,0u,1u,abi,witness)==nullptr);
        state.debug_full_patchable_typed_target_base_address-=sizeof(std::uintptr_t);
        state.debug_activation_enabled=false;
        require(reload(carrier,0u,1u,abi,witness)==nullptr);
        state.debug_activation_enabled=true;
        auto saved_plan=state.checkpoint_plan;
        cp::function_plan another_plan;another_plan.profile=cp::compilation_profile::create_for_trusted_observer();
        state.checkpoint_plan=&another_plan;
        require(reload(carrier,0u,1u,abi,witness)==nullptr);
        state.checkpoint_plan=saved_plan;
        state.llvm_function=function;
        require(reload(carrier,0u,1u,abi,witness)==nullptr);
        state.llvm_function=fn;
        auto callback_type=llvm::FunctionType::get(builder.getVoidTy(),{builder.getInt64Ty(),builder.getInt64Ty()},false);
        auto callback=builder.CreateIntToPtr(builder.getInt64(reinterpret_cast<std::uintptr_t>(&publish_after_observation)),builder.getPtrTy());
        builder.CreateCall(callback_type,callback,
            {builder.getInt64(reinterpret_cast<std::uintptr_t>(published.data())),fn->getArg(2)});
        auto target_entry=reload(carrier,0u,1u,abi,witness);
        require((target_entry==nullptr)==bypass);
        if(bypass) target_entry=emit::emit_runtime_llvm_jit_local_reference_dispatch(state,carrier,0u,1u,abi);
        require(target_entry!=nullptr);builder.CreateRet(target_entry);
        unsigned remainders{},divides{},acquires{},volatile_loads{},callbacks{};
        for(auto& block:*fn) for(auto& instruction:block)
        {
            remainders+=instruction.getOpcode()==llvm::Instruction::URem;
            divides+=instruction.getOpcode()==llvm::Instruction::UDiv;
            callbacks+=llvm::isa<llvm::CallBase>(instruction);
            if(auto load=llvm::dyn_cast<llvm::LoadInst>(&instruction))
            {
                acquires+=load->getOrdering()==llvm::AtomicOrdering::Acquire;
                volatile_loads+=load->isAtomic() && load->isVolatile();
            }
        }
        require(remainders==(bypass?2u:1u) && divides==(bypass?2u:1u));
        require(acquires==2u && volatile_loads==2u && callbacks==1u);
        return fn;
    };
    make_witness_probe("witness_diamond",false);
    make_witness_probe("witness_bypass",true);
    require(!llvm::verifyModule(*ir,&llvm::errs()));
    state.ir_builder.reset();state.llvm_module=nullptr;state.llvm_function=nullptr;
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder(std::move(ir)).setEngineKind(llvm::EngineKind::JIT)
        .setOptLevel(llvm::CodeGenOptLevel::Less).create(target.release())};require(bool(engine));
    engine->finalizeObject();require(!engine->hasError());
    auto address=engine->getFunctionAddress("probe");require(address!=0u);
    auto probe=reinterpret_cast<std::uintptr_t(*)(std::uintptr_t,unsigned)>(static_cast<std::uintptr_t>(address));
    auto base=reinterpret_cast<std::uintptr_t>(owner.local_defined_function_vec_storage.data());
    auto stride=sizeof(storage::local_defined_function_storage_t);
    auto kind=static_cast<unsigned>(uwvm2::object::global::wasm_ref_kind::wasm_func_defined);
    require(probe(base,kind)==published[0]);
    for(std::size_t i=1;i<published.size();++i) require(probe(base+i*stride,kind)==0u);
    for(auto invalid:std::array<std::uintptr_t,7>{0u,1u,base-1u,base+1u,base+stride-1u,base+published.size()*stride,UINTPTR_MAX})
        require(probe(invalid,kind)==0u);
    for(auto invalid:std::array<unsigned,5>{0u,1u,2u,4u,UINT_MAX}) require(probe(base,invalid)==0u);
    std::atomic_ref<std::uintptr_t>{published[0]}.store(0xeeee000u,std::memory_order_release);
    require(probe(base,kind)==0xeeee000u);
    std::atomic_ref<std::uintptr_t>{published[0]}.store(0u,std::memory_order_release);require(probe(base,kind)==0u);
    using witnessed_function=std::uintptr_t(*)(std::uintptr_t,unsigned,std::uintptr_t,bool);
    for(auto name:{"witness_diamond","witness_bypass"})
    {
        auto fn=reinterpret_cast<witnessed_function>(static_cast<std::uintptr_t>(engine->getFunctionAddress(name)));
        require(fn!=nullptr);
        for(bool choice:{false,true})
        {
            std::atomic_ref<std::uintptr_t>{published[0]}.store(0x1111000u,std::memory_order_release);
            require(fn(base,kind,0x2222000u,choice)==0x2222000u);
            // Admission remains valid when a previously zero entry is published.
            std::atomic_ref<std::uintptr_t>{published[0]}.store(0u,std::memory_order_release);
            require(fn(base,kind,0x3333000u,choice)==0x3333000u);
            require(fn(base,kind,0u,choice)==0u);
            for(auto invalid:std::array<std::uintptr_t,7>{0u,1u,base-1u,base+1u,base+stride-1u,base+published.size()*stride,UINTPTR_MAX})
                require(fn(invalid,kind,0x4444000u,choice)==0u);
            for(auto invalid:std::array<unsigned,5>{0u,1u,2u,4u,UINT_MAX})
                require(fn(base,invalid,0x4444000u,choice)==0u);
            for(std::size_t i=1;i<published.size();++i)
                require(fn(base+i*stride,kind,0x4444000u,choice)==0u);
        }
    }
    std::puts("PASS observation witness: native publication callback; fresh/nonzero-to-zero/zero-to-nonzero entry; one identity/type gate across diamond; bypass/full-dispatch fallback; invalid carrier/type; SSA/ABI/plan/function/table/count mismatch rejected");
    std::puts("PASS actual typed-ref IR: verifier, no resolver calls, acquire publication, fresh/zero entry, invalid range/stride/kind/type, zero/overflow interval, disabled profile/count mismatch");
#else
    std::puts("SKIP: actual typed-ref dispatch qualified only on Linux x86_64");
#endif
}
