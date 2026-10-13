#define UWVM_RUNTIME_LLVM_JIT 1
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/IR/Verifier.h>
#include <chrono>
#include <fast_io.h>
namespace metadata=::uwvm2::runtime::compiler::llvm_jit::native_provenance;
int main()
{
    for(unsigned count:{1000u,2000u,4000u})
    {
        ::llvm::LLVMContext context{};::llvm::Module module{"constant-sibling-finalization",context};
        auto* vector{::llvm::FixedVectorType::get(::llvm::Type::getInt8Ty(context),16u)};
        auto* function{::llvm::Function::Create(::llvm::FunctionType::get(vector,{::llvm::Type::getInt1Ty(context)},false),
            ::llvm::Function::ExternalLinkage,"siblings",module)};
        auto* scope{metadata::attach(*function,"uwvm-m0-f1-g1.wasm-native-v1")};
        auto* entry{::llvm::BasicBlock::Create(context,"entry",function)};::llvm::IRBuilder<> ir{entry};
        ::llvm::SmallVector<metadata::numeric_identity,0u> identities{};
        auto* literal{::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u),ir.getInt8(0xa5u))};
        ::llvm::SmallVector<::llvm::ReturnInst*,0u> returns{};
        for(unsigned i{};i!=count;++i)
        {
            auto* body{::llvm::BasicBlock::Create(context,"body",function)};
            auto* next{::llvm::BasicBlock::Create(context,"next",function)};
            ir.CreateCondBr(function->getArg(0u),body,next);ir.SetInsertPoint(body);
            if(!metadata::location(ir,scope,0u,1u))return 1;
            ::llvm::Instruction* witness{};auto* value{metadata::materialize_numeric_register(ir,literal,&witness)};
            if(value==nullptr || value==literal)return 1;
            auto* same{::llvm::dyn_cast<::llvm::Instruction>(ir.CreateBitCast(value,literal->getType()))};if(!same)return 1;
            identities.push_back({literal,same});returns.push_back(ir.CreateRet(literal));ir.SetInsertPoint(next);
        }
        ir.CreateRet(literal);if(::llvm::verifyModule(module))return 1;
        auto const before{::std::chrono::steady_clock::now()};metadata::restrict_public_code(*function,identities);
        auto const elapsed{::std::chrono::duration_cast<::std::chrono::microseconds>(::std::chrono::steady_clock::now()-before).count()};
        if(::llvm::verifyModule(module))return 1;
        for(unsigned i{};i!=count;++i)
        {if(returns[i]->getReturnValue()!=static_cast<::llvm::Value*>(identities[i].definition))return 1;}
        ::fast_io::io::println("actual sibling v128 finalization identities=",count," microseconds=",elapsed);
    }
}
