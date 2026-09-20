// Cover generated scalar/vector arithmetic and conversion paths, not only the
// native strict_float helper. Emitted objects must also be linked and executed:
// correct-looking IR alone does not rule out target ABI or legalization errors.
// Standalone LLVM test: emit scalar AND vector instructions, apply the production lowering,
// verify idempotence/zero-overhead bypass, and run the resulting integer-ABI entries in MCJIT.
// Pass --emit to write portable IR for llc + a cross-target oracle consumer.
#include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdio>
#include <memory>
#include <string>

namespace jit = uwvm2::runtime::compiler::shared::strict_float_jit;
unsigned operations(llvm::Module& m)
{
    unsigned count{};
    for(auto& f:m) for(auto& b:f) for(auto& i:b)
        if(i.getType()->isFPOrFPVectorTy() && !llvm::isa<llvm::BitCastInst>(i) && !llvm::isa<llvm::InsertElementInst>(i) &&
           !llvm::isa<llvm::ExtractElementInst>(i)) { ++count; }
    return count;
}
int main(int argc,char**)
{
    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
    llvm::LLVMContext context;
    auto module{std::make_unique<llvm::Module>("strict-float",context)};
    llvm::IRBuilder<> b{context};
    for(unsigned width: {32u,64u}) for(unsigned vector: {0u,1u}) for(unsigned op{};op!=(width==32?8u:7u);++op)
    {
        auto ptr{b.getPtrTy()};
        auto type{llvm::FunctionType::get(b.getVoidTy(),{ptr,ptr,ptr},false)};
        auto name{"strict_"+std::to_string(width)+"_"+std::to_string(vector)+"_"+std::to_string(op)};
        auto f{llvm::Function::Create(type,llvm::Function::ExternalLinkage,name,*module)};
        b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",f));
        auto scalar{width==32?b.getFloatTy():b.getDoubleTy()};
        auto input{op==7?b.getDoubleTy():scalar};
        auto count{vector==0?1u:4u};
        llvm::Type* operand_type{op==5||op==6 ? static_cast<llvm::Type*>(b.getInt64Ty()):input};
        auto load=[&](unsigned arg)
        {
            llvm::Value* value{nullptr};
            if(vector) value=llvm::PoisonValue::get(llvm::FixedVectorType::get(operand_type,count));
            for(unsigned lane{};lane!=count;++lane)
            {
                llvm::Value* raw{b.CreateLoad(b.getInt64Ty(),b.CreateGEP(b.getInt64Ty(),f->getArg(arg),b.getInt32(lane)))};
                if(operand_type->isFloatingPointTy())
                {
                    if(input->isFloatTy()) raw=b.CreateTrunc(raw,b.getInt32Ty());
                    raw=b.CreateBitCast(raw,input);
                }
                value=vector ? b.CreateInsertElement(value,raw,b.getInt32(lane)):raw;
            }
            return value;
        };
        auto left{load(0)},right{load(1)};
        llvm::Type* result_type{vector ? static_cast<llvm::Type*>(llvm::FixedVectorType::get(scalar,count)):scalar};
        llvm::Value* result{};
        switch(op)
        {
            case 0: result=b.CreateFAdd(left,right); break;
            case 1: result=b.CreateFSub(left,right); break;
            case 2: result=b.CreateFMul(left,right); break;
            case 3: result=b.CreateFDiv(left,right); break;
            case 4: result=b.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt,left); break;
            case 5: result=b.CreateSIToFP(left,result_type); break;
            case 6: result=b.CreateUIToFP(left,result_type); break;
            case 7: result=b.CreateFPTrunc(left,result_type); break;
        }
        for(unsigned lane{};lane!=count;++lane)
        {
            auto value{vector ? b.CreateExtractElement(result,b.getInt32(lane)):result};
            auto bits{b.CreateBitCast(value,width==32?b.getInt32Ty():b.getInt64Ty())};
            b.CreateStore(b.CreateZExtOrTrunc(bits,b.getInt64Ty()),b.CreateGEP(b.getInt64Ty(),f->getArg(2),b.getInt32(lane)));
        }
        b.CreateRetVoid();
    }
    auto const before{operations(*module)};
    jit::lower(*module,false);
    if(before!=30 || operations(*module)!=before || module->getFunction(jit::symbol_name)!=nullptr) return 1;
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
    for(auto& function:*module) function.addFnAttr("target-features","-sse2,+sse2");
    jit::lower(*module,true);
    if(llvm::verifyModule(*module) || operations(*module)!=22) return 7;
    for(auto& function:*module) function.addFnAttr("target-features","+sse2,-sse2");
#endif
    jit::lower(*module,true);
    if(llvm::verifyModule(*module) || operations(*module)!=0) return 2;
    jit::lower(*module,true);
    if(llvm::verifyModule(*module) || operations(*module)!=0) return 3;
    for(auto& function:*module) function.removeFnAttr("target-features");
    if(argc>1) { module->print(llvm::outs(),nullptr); return 0; }
    std::string error;
    auto engine{std::unique_ptr<llvm::ExecutionEngine>(llvm::EngineBuilder(std::move(module)).setErrorStr(&error).create())};
    if(!engine) { std::fprintf(stderr,"%s\n",error.c_str()); return 4; }
    engine->finalizeObject();
    using call_t=void(*)(std::uint64_t const*,std::uint64_t const*,std::uint64_t*);
    std::uint64_t left[4]{0x3ff0000000000000,0x3ff0000000000000,0x3ff0000000000000,0x3ff0000000000000};
    std::uint64_t right[4]{0x3ca0000000000001,0x3ca0000000000001,0x3ca0000000000001,0x3ca0000000000001};
    std::uint64_t out[4]{};
    for(unsigned vector:{0u,1u})
    {
        auto address{engine->getFunctionAddress("strict_64_"+std::to_string(vector)+"_0")};
        if(!address) return 5;
        reinterpret_cast<call_t>(address)(left,right,out);
        for(unsigned lane{};lane!=(vector?4u:1u);++lane) if(out[lane]!=0x3ff0000000000001) return 6;
    }
}
