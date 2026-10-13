// Focused IR contract test. The two no-op host definitions provide addresses
// only: this executable prints IR and never executes generated guest code.
#define UWVM_USE_LLVM_JIT
#define UWVM_DISABLE_INT
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdio>
namespace uwvm2::runtime::lib
{
    void llvm_jit_push_call_stack_frame(std::size_t, std::size_t) noexcept {}
    void llvm_jit_pop_call_stack_frame() noexcept {}
}
namespace details = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
int main()
{
    llvm::LLVMContext context;
    llvm::Module module{"logical-trace-nounwind-contract",context};
    auto const type{llvm::FunctionType::get(llvm::Type::getVoidTy(context),false)};
    auto const trace_leaf{llvm::Function::Create(type,llvm::GlobalValue::ExternalLinkage,"trace_leaf",module)};
    trace_leaf->addFnAttr(llvm::Attribute::NoInline);
    llvm::IRBuilder<> builder{llvm::BasicBlock::Create(context,"entry",trace_leaf)};
    REQUIRE(details::emit_runtime_local_func_llvm_jit_call_stack_push(builder,0,0));
    REQUIRE(details::emit_runtime_local_func_llvm_jit_call_stack_pop(builder));
    builder.CreateRetVoid();
    auto const guest{llvm::Function::Create(type,llvm::GlobalValue::ExternalLinkage,"actual_guest_call",module)};
    auto const personality{llvm::Function::Create(llvm::FunctionType::get(builder.getInt32Ty(),true),
        llvm::GlobalValue::ExternalLinkage,"__gxx_personality_v0",module)};
    for(bool const guest_call:{false,true})
    {
        auto const function{llvm::Function::Create(type,llvm::GlobalValue::ExternalLinkage,
            guest_call?"guest_caller":"normal_caller",module)};
        function->addFnAttr(llvm::Attribute::NoInline);
        function->setPersonalityFn(personality);
        auto const entry{llvm::BasicBlock::Create(context,"entry",function)};
        auto const normal{llvm::BasicBlock::Create(context,"normal",function)};
        auto const cleanup{llvm::BasicBlock::Create(context,"cleanup",function)};
        builder.SetInsertPoint(entry);
        REQUIRE(details::emit_runtime_local_func_llvm_jit_call_stack_push(builder,0,guest_call?2:1));
        auto const call{builder.CreateInvoke(type,guest_call?guest:trace_leaf,normal,cleanup,{})};
        REQUIRE(!call->doesNotThrow());
        builder.SetInsertPoint(normal);
        REQUIRE(details::emit_runtime_local_func_llvm_jit_call_stack_pop(builder));
        builder.CreateRetVoid();
        builder.SetInsertPoint(cleanup);
        auto const landing{builder.CreateLandingPad(llvm::StructType::get(builder.getPtrTy(),builder.getInt32Ty()),0)};
        landing->setCleanup(true);
        REQUIRE(details::emit_runtime_local_func_llvm_jit_call_stack_pop(builder));
        builder.CreateResume(landing);
    }
    std::size_t helper_calls{};
    for(auto const& function:module) for(auto const& block:function) for(auto const& item:block)
    {
        auto const call{llvm::dyn_cast<llvm::CallInst>(&item)};
        if(call==nullptr) { continue; }
        // The only CallInsts emitted here are the actual production trace helpers.
        REQUIRE(call->doesNotThrow());
        if(auto const callee{call->getCalledFunction()}; callee!=nullptr) { REQUIRE(callee->doesNotThrow()); }
        ++helper_calls;
    }
    REQUIRE(helper_calls==8&&!guest->doesNotThrow());
    REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
    module.print(llvm::outs(),nullptr);
}
