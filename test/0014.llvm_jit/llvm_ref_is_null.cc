// Emit the production reference tag extraction for native 32/64-bit reference layouts and target endianness.
// The generated-object test is separate from the complete table64 JIT runtime fixture.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
using kind=uwvm2::object::global::wasm_ref_kind;
struct ref32 {std::uint32_t payload;kind kind;};
struct ref64 {std::uint64_t payload;kind kind;};
static_assert(sizeof(ref32)==8&&offsetof(ref32,kind)==4);
static_assert(sizeof(ref64)==16&&offsetof(ref64,kind)==8);
template<typename Reference>void emit(llvm::Module& module,bool little)
{
 llvm::IRBuilder<> b(module.getContext());auto ptr=llvm::PointerType::getUnqual(module.getContext());
 auto fn=llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(),{ptr},false),llvm::Function::ExternalLinkage,"ref_is_null",module);
 b.SetInsertPoint(llvm::BasicBlock::Create(module.getContext(),"entry",fn));
 auto bits=b.CreateLoad(b.getIntNTy(sizeof(Reference)*8),fn->getArg(0));bits->setAlignment(llvm::Align{1});
 auto result=d::emit_llvm_jit_ref_is_null<Reference>(b,bits,little);CHECK(result);b.CreateRet(result);
 CHECK(!d::emit_llvm_jit_ref_is_null<Reference>(b,nullptr,little));
 CHECK(!d::emit_llvm_jit_ref_is_null<Reference>(b,b.getInt32(0),little));
}
int main(int argc,char** argv)
{
 CHECK(argc==5);unsigned width=std::stoul(argv[2]);CHECK(width==32||width==64);bool little=std::string(argv[3])=="little";
 CHECK(little||std::string(argv[3])=="big");llvm::LLVMContext context;llvm::Module module("direct-reference-null",context);
 module.setTargetTriple(llvm::Triple(argv[1]));module.setDataLayout(std::string(little?"e":"E")+"-p:"+argv[2]+":"+argv[2]);
 if(width==32)emit<ref32>(module,little);else emit<ref64>(module,little);
 CHECK(!llvm::verifyModule(module,&llvm::errs()));std::error_code ec;llvm::raw_fd_ostream out(argv[4],ec);CHECK(!ec);module.print(out,nullptr);
}
