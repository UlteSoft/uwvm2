// Production scalar pass -> verified IR -> actual MCJIT execution. These are
// component values/addresses; they grant no VM activation or debugger authority.
#include <uwvm2/runtime/compiler/llvm_jit/debug_numeric_local_ssa.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/InitializePasses.h>
#include <llvm/PassRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace opt = uwvm2::runtime::compiler::llvm_jit::details;
static std::array<std::uint64_t,2> observed;
static unsigned captures;
static void require(bool b) { if(!b) { std::fputs("debug scalar cleanup assertion failed\n",stderr);std::abort(); } }
extern "C" void fixture_capture(std::uint64_t* words) noexcept
{ require(captures<observed.size() && words[1]==0u);observed[captures++]=words[0];words[0]+=7u; }
extern "C" void fixture_may_throw() {}
struct shape
{
    unsigned blocks{},safe_calls{},nothrow_safe{},required{},invokes{},atomic_loads{},volatile_stores{},instructions{};
};
static shape inspect(llvm::Function const& f)
{
    shape s;
    for(auto const& b:f)
    {
        ++s.blocks;
        for(auto const& i:b)
        {
            ++s.instructions;
            s.safe_calls+=i.getMetadata("uwvm.debug.safe_point")!=nullptr;
            if(i.getMetadata("uwvm.debug.safe_point")!=nullptr) { auto call=llvm::dyn_cast<llvm::CallBase>(&i);s.nothrow_safe+=call!=nullptr && call->doesNotThrow();s.required+=i.getMetadata("uwvm.debug.shutdown.required")!=nullptr; }
            s.invokes+=llvm::isa<llvm::InvokeInst>(i);
            if(auto load=llvm::dyn_cast<llvm::LoadInst>(&i))s.atomic_loads+=load->isAtomic();
            if(auto store=llvm::dyn_cast<llvm::StoreInst>(&i))s.volatile_stores+=store->isVolatile();
        }
    }
    return s;
}
int main()
{
#if defined(__linux__) && defined(__x86_64__)
    auto& registry=*llvm::PassRegistry::getPassRegistry();
    llvm::initializeCore(registry);llvm::initializeTransformUtils(registry);
    llvm::initializeAnalysis(registry);llvm::initializeScalarOpts(registry);llvm::initializeInstCombine(registry);
    require(!llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter());
    llvm::EngineBuilder choose;choose.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    std::unique_ptr<llvm::TargetMachine> target{choose.selectTarget()};require(bool(target));
    llvm::LLVMContext context;auto module=std::make_unique<llvm::Module>("debug-scalar-cleanup",context);
#if LLVM_VERSION_MAJOR >= 21
    module->setTargetTriple(target->getTargetTriple());
#else
    module->setTargetTriple(target->getTargetTriple().str());
#endif
    module->setDataLayout(target->createDataLayout());
    llvm::IRBuilder<> b{context};auto i64=b.getInt64Ty();auto ptr=b.getPtrTy();
    auto type=llvm::FunctionType::get(i64,{i64,ptr,b.getInt1Ty()},false);
    auto f=llvm::Function::Create(type,llvm::Function::ExternalLinkage,"probe",*module);
    f->addFnAttr("uwvm.native.debug.pcrel");
    auto entry=llvm::BasicBlock::Create(context,"entry",f);b.SetInsertPoint(entry);
    auto local=b.CreateAlloca(i64);local->setMetadata("uwvm.debug.numeric.local",llvm::MDNode::get(context,{}));
    b.CreateStore(f->getArg(0),local);
    auto first=b.CreateAdd(b.CreateLoad(i64,local),b.getInt64(3));
    auto second=b.CreateAdd(b.CreateLoad(i64,local),b.getInt64(3));
    b.CreateStore(b.CreateZExt(first,b.getInt128Ty()),f->getArg(1))->setAlignment(llvm::Align{1});
    auto flag=b.CreateInBoundsGEP(i64,f->getArg(1),b.getInt64(2));
    b.CreateStore(b.getInt64(1),flag)->setVolatile(true);
    auto a=b.CreateLoad(i64,flag);a->setAtomic(llvm::AtomicOrdering::Acquire);a->setAlignment(llvm::Align{8});
    auto capture_type=llvm::FunctionType::get(b.getVoidTy(),{ptr},false);
    auto capture_address=llvm::ConstantExpr::getIntToPtr(b.getInt64(reinterpret_cast<std::uintptr_t>(&fixture_capture)),ptr);
    llvm::FunctionCallee capture{capture_type,capture_address};
    auto site1=b.CreateCall(capture,{f->getArg(1)});site1->setMetadata("uwvm.debug.safe_point",llvm::MDNode::get(context,{}));site1->setDoesNotThrow();site1->setMetadata("uwvm.debug.shutdown.required",llvm::MDNode::get(context,{llvm::ConstantAsMetadata::get(b.getInt32(1))}));
    auto initialized=llvm::BasicBlock::Create(context,"reference.initialized",f);
    auto absent=llvm::BasicBlock::Create(context,"reference.absent",f);
    auto ready=llvm::BasicBlock::Create(context,"ready",f);
    b.CreateCondBr(f->getArg(2),initialized,absent);b.SetInsertPoint(initialized);
    auto reference=b.CreateLoad(i64,b.CreateInBoundsGEP(i64,f->getArg(1),b.getInt64(3)));
    b.CreateBr(ready);b.SetInsertPoint(absent);b.CreateBr(ready);b.SetInsertPoint(ready);
    auto selected=b.CreatePHI(i64,2);selected->addIncoming(reference,initialized);selected->addIncoming(b.getInt64(0),absent);
    b.CreateStore(selected,b.CreateInBoundsGEP(i64,f->getArg(1),b.getInt64(4)));
    b.CreateStore(b.getInt64(1),flag)->setVolatile(true);
    auto a2=b.CreateLoad(i64,flag);a2->setAtomic(llvm::AtomicOrdering::Acquire);a2->setAlignment(llvm::Align{8});
    auto site2=b.CreateCall(capture,{f->getArg(1)});site2->setMetadata("uwvm.debug.safe_point",llvm::MDNode::get(context,{}));site2->setDoesNotThrow();site2->setMetadata("uwvm.debug.shutdown.required",llvm::MDNode::get(context,{llvm::ConstantAsMetadata::get(b.getInt32(1))}));
    // The opaque callback changes the same escaped payload. A stale forwarded
    // load would produce a wrong result even if ordinary arithmetic is correct.
    b.CreateRet(b.CreateAdd(b.CreateAdd(second,b.CreateLoad(i64,f->getArg(1))),b.CreateAdd(a,a2)));
    auto before=inspect(*f);require(!llvm::verifyModule(*module,&llvm::errs()));
    require(opt::promote_runtime_llvm_jit_debug_numeric_locals(*f)==1u);
    auto after=inspect(*f);
    require(after.instructions<before.instructions && after.blocks==before.blocks &&
            after.safe_calls==2 && after.nothrow_safe==2 && after.required==2 && after.atomic_loads==2 && after.volatile_stores==2);
    require(reference->getParent()==initialized);

    // Actual exception Invoke, cleanup landingpad and metadata survive the
    // production passes. optnone continuation bodies remain excluded.
    auto cleanup=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{},false),llvm::Function::ExternalLinkage,"cleanup",*module);
    cleanup->addFnAttr("uwvm.native.debug.pcrel");
    auto personality=module->getOrInsertFunction("__gxx_personality_v0",llvm::FunctionType::get(b.getInt32Ty(),{},true));
    cleanup->setPersonalityFn(llvm::cast<llvm::Constant>(personality.getCallee()));
    auto ce=llvm::BasicBlock::Create(context,"entry",cleanup);
    auto done=llvm::BasicBlock::Create(context,"done",cleanup);
    auto unwind=llvm::BasicBlock::Create(context,"unwind",cleanup);b.SetInsertPoint(ce);
    llvm::FunctionCallee opaque{llvm::FunctionType::get(b.getVoidTy(),{},false),
        llvm::ConstantExpr::getIntToPtr(b.getInt64(reinterpret_cast<std::uintptr_t>(&fixture_may_throw)),ptr)};
    auto invoke=b.CreateInvoke(opaque,done,unwind);invoke->setMetadata("uwvm.debug.shutdown",llvm::MDNode::get(context,{}));
    b.SetInsertPoint(done);b.CreateRetVoid();b.SetInsertPoint(unwind);
    auto landing=b.CreateLandingPad(llvm::StructType::get(ptr,b.getInt32Ty()),0);landing->setCleanup(true);b.CreateResume(landing);
    auto cb=inspect(*cleanup);static_cast<void>(opt::simplify_runtime_llvm_jit_debug_scalar_ir(*cleanup));
    require(inspect(*cleanup).blocks==cb.blocks && inspect(*cleanup).invokes==1 && invoke->getMetadata("uwvm.debug.shutdown")!=nullptr);
    // The actual failed VM shape: a constant-dead safepoint followed by an
    // Invoke. InstCombine can strip nonterminators and leave a poison Invoke;
    // the production cleanup must erase the whole unreachable path together.
    auto dead=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{ptr},false),llvm::Function::ExternalLinkage,"dead_cleanup",*module);
    dead->addFnAttr("uwvm.native.debug.pcrel");dead->setPersonalityFn(llvm::cast<llvm::Constant>(personality.getCallee()));
    auto de=llvm::BasicBlock::Create(context,"entry",dead);
    auto never=llvm::BasicBlock::Create(context,"never",dead);
    auto live=llvm::BasicBlock::Create(context,"live",dead);
    auto dr=llvm::BasicBlock::Create(context,"dead.return",dead);
    auto du=llvm::BasicBlock::Create(context,"dead.unwind",dead);b.SetInsertPoint(de);
    b.CreateCondBr(b.getFalse(),never,live);b.SetInsertPoint(never);
    auto ds=b.CreateCall(capture,{dead->getArg(0)});ds->setDoesNotThrow();
    ds->setMetadata("uwvm.debug.safe_point",llvm::MDNode::get(context,{}));
    ds->setMetadata("uwvm.debug.shutdown.required",llvm::MDNode::get(context,{llvm::ConstantAsMetadata::get(b.getInt32(1))}));
    auto di=b.CreateInvoke(opaque,dr,du);di->setMetadata("uwvm.debug.shutdown",llvm::MDNode::get(context,{}));
    b.SetInsertPoint(dr);b.CreateBr(live);b.SetInsertPoint(du);
    auto dl=b.CreateLandingPad(llvm::StructType::get(ptr,b.getInt32Ty()),0);dl->setCleanup(true);b.CreateResume(dl);
    b.SetInsertPoint(live);b.CreateRetVoid();require(!llvm::verifyModule(*module,&llvm::errs()));
    require(inspect(*dead).safe_calls==1 && inspect(*dead).invokes==1);
    require(opt::simplify_runtime_llvm_jit_debug_scalar_ir(*dead));
    require(inspect(*dead).safe_calls==0 && inspect(*dead).invokes==0 && dead->size()==2);
    cleanup->addFnAttr(llvm::Attribute::NoInline);cleanup->addFnAttr(llvm::Attribute::OptimizeNone);
    require(!opt::simplify_runtime_llvm_jit_debug_scalar_ir(*cleanup));
    f->removeFnAttr("uwvm.native.debug.pcrel");require(!opt::simplify_runtime_llvm_jit_debug_scalar_ir(*f));f->addFnAttr("uwvm.native.debug.pcrel");
    require(!llvm::verifyModule(*module,&llvm::errs()));
    // Actual owned bridge addresses, just as the production emitter uses.
    // A name lookup or mocked identity grants no VM admission.
    std::string error;llvm::EngineBuilder build(std::move(module));build.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less).setErrorStr(&error);
    std::unique_ptr<llvm::ExecutionEngine> engine{build.create()};require(bool(engine));engine->finalizeObject();
    auto probe=reinterpret_cast<std::uint64_t(*)(std::uint64_t,std::uint64_t*,bool)>(engine->getFunctionAddress("probe"));require(probe);
    auto clean=reinterpret_cast<void(*)(std::uint64_t*)>(engine->getFunctionAddress("dead_cleanup"));require(clean);captures=0;clean(nullptr);require(captures==0);
    for(std::uint64_t x:{0u,14u,999u})for(bool active:{false,true})
    {
        std::array<std::uint64_t,5> words{0,0,0,88,0};captures=0;
        require(probe(x,words.data(),active)==2*x+22 && captures==2 &&
                observed[0]==x+3 && observed[1]==x+10 && words[4]==(active?88u:0u));
    }
    std::printf("scalar instructions %u -> %u; live CFG, callbacks, atomic/volatile accesses, guarded reference load, EH, constant-dead cancellation cleanup, optnone and actual native results: passed\n",before.instructions,after.instructions);
#endif
}
