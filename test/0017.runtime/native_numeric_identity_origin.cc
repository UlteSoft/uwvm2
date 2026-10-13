// Compiler-origin DATA/IR component. No native stop or execution authority.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/IR/Verifier.h>
#include <fast_io.h>
namespace origin = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("numeric identity origin failure line=",__LINE__); return 1; } } while(false)
int main()
{
    ::llvm::LLVMContext context{};
    ::llvm::Module module{"numeric-identity-origin",context};
    auto* const type{::llvm::Type::getInt32Ty(context)};
    auto* const signature{::llvm::FunctionType::get(type,{type},false)};
    auto* const function{::llvm::Function::Create(signature,::llvm::Function::ExternalLinkage,"origin",module)};
    auto* const scope{origin::attach(*function,"uwvm-m0-f0-g1.wasm-native-v1")}; CHECK(scope);
    auto* const block{::llvm::BasicBlock::Create(context,"entry",function)};
    ::llvm::IRBuilder<> ir{block}; CHECK(origin::location(ir,scope,0u,3u));
    auto* const value{ir.CreateAdd(function->getArg(0u),ir.getInt32(9u))};
    CHECK(origin::numeric(ir,value,0x7fu,0u));
    auto* const actual{origin::materialize_numeric_register(ir,value)}; CHECK(actual && actual->getType()==type);
    if(actual!=value)
    {
        auto* const identity{::llvm::dyn_cast<::llvm::CallInst>(actual)}; CHECK(identity && identity->doesNotThrow());
        auto* const assembly{::llvm::dyn_cast<::llvm::InlineAsm>(identity->getCalledOperand())};
        CHECK(assembly && assembly->getAsmString().empty() && assembly->hasSideEffects());
        CHECK(identity->getMetadata("uwvm.native.numeric.code")!=nullptr);
    }
    // A different inline-assembly call never inherits this origin, even when
    // its result is traversed by the numeric-definition collector.
    auto* const assembly{::llvm::InlineAsm::get(signature,"nop","=r,0",true)};
    auto* const foreign{ir.CreateCall(signature,assembly,{function->getArg(0u)})}; foreign->setDoesNotThrow();
    CHECK(origin::numeric(ir,foreign,0x7fu,1u));
    CHECK(foreign->getMetadata("uwvm.native.numeric.code")==nullptr);
    auto* const pointer{ir.CreateAlloca(type)};
    CHECK(origin::materialize_numeric_register(ir,pointer)==pointer);
    origin::unknown(ir,scope); auto* const returned{ir.CreateRet(actual)};
    origin::restrict_public_code(*function);
    CHECK(::llvm::cast<::llvm::Instruction>(actual)->getDebugLoc().getCol()==1u);
    CHECK(foreign->getDebugLoc().getCol()==0u && pointer->getDebugLoc().getCol()==0u);
    CHECK(returned->getDebugLoc().getLine()==0u && returned->getDebugLoc().getCol()==0u);
    CHECK(!::llvm::verifyModule(module));
    ::fast_io::io::println("PASS exact compiler numeric identity origin; foreign assembly, pointer and return remain unqualified");
}
