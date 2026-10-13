// Actual generated mixed-width calls into the production bulk bridge ABIs.
// Wasm frontend dispatch and whole-VM unwind are separately qualified.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Passes/PassBuilder.h>
#include <string>
#include "../0017.runtime/memory64_init_cases.h"
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace test=memory64_init_test;
namespace uwvm2::runtime::lib
{
 void llvm_jit_memory_out_of_bounds_trap(std::size_t index,std::uint_least64_t offset,std::uint_least64_t effective,
  std::uint_least32_t carry,std::uint_least64_t length,std::size_t width,std::uintptr_t,std::uintptr_t) noexcept
 {
  if(!test::expected_trap||index||offset||effective!=test::expected_address||carry||length!=test::expected_bound||width!=test::expected_length){_Exit(95);}
  test::complete_trap();
 }
 void llvm_jit_runtime_trap(llvm_jit_trap_kind,std::uintptr_t,std::uintptr_t) noexcept {_Exit(96);}
}
struct object_cache:llvm::ObjectCache
{
 std::string output;explicit object_cache(std::string out):output(std::move(out)){}
 void notifyObjectCompiled(llvm::Module const*,llvm::MemoryBufferRef buffer) override
 {std::error_code ec;llvm::raw_fd_ostream out(output,ec);INIT_CHECK(!ec);out<<buffer.getBuffer();}
 std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override{return {};}
};
void optimize(llvm::Module& module)
{
 llvm::PassBuilder p;llvm::LoopAnalysisManager l;llvm::FunctionAnalysisManager f;llvm::CGSCCAnalysisManager c;llvm::ModuleAnalysisManager m;
 p.registerModuleAnalyses(m);p.registerCGSCCAnalyses(c);p.registerFunctionAnalyses(f);p.registerLoopAnalyses(l);
 p.crossRegisterProxies(l,f,c,m);p.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3).run(module,m);
}
void emit(llvm::Module& module)
{
 llvm::IRBuilder<> b(module.getContext());auto intptr=b.getIntNTy(sizeof(std::uintptr_t)*8);
 auto carrier=b.getIntNTy(sizeof(d::llvm_jit_atomic_address_carrier_t)*8);
 auto type=llvm::FunctionType::get(b.getVoidTy(),{intptr,intptr,b.getInt64Ty(),carrier,carrier},false);
 auto fn=llvm::Function::Create(type,llvm::Function::ExternalLinkage,"memory_init",module);
 b.SetInsertPoint(llvm::BasicBlock::Create(module.getContext(),"entry",fn));
 auto bridge=d::get_llvm_runtime_bridge_function_symbol_value<d::llvm_jit_memory64_init_bridge>(b,type);INIT_CHECK(bridge);
 auto call=b.CreateCall(type,bridge,{fn->getArg(0),fn->getArg(1),fn->getArg(2),fn->getArg(3),fn->getArg(4)});
 d::apply_llvm_jit_host_calling_conv(call);b.CreateRetVoid();
}
int main(int argc,char** argv)
{
 INIT_CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-init-abi",context);
 emit(*module);INIT_CHECK(!llvm::verifyModule(*module,&llvm::errs()));
 auto view=module.get();std::string error;object_cache cache(out+"/memory64-init.o");
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
  .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());
 if(!engine){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 view->setDataLayout(engine->getDataLayout());if(optimized){optimize(*view);INIT_CHECK(!llvm::verifyModule(*view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64-init.ll",ec);INIT_CHECK(!ec);view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 using carrier=d::llvm_jit_atomic_address_carrier_t;
 using bridge=void(*)(std::uintptr_t,std::uintptr_t,std::uint64_t,carrier,carrier);
 auto fn=reinterpret_cast<bridge>(engine->getFunctionAddress("memory_init"));INIT_CHECK(fn);
 test::cases<d::runtime_native_memory_t>([&](auto& memory,auto& data,std::uint64_t dst,std::uint32_t src,std::uint32_t len)
 {fn(reinterpret_cast<std::uintptr_t>(&memory),reinterpret_cast<std::uintptr_t>(&data),dst,src,len);});
 std::printf("PASS memory64 actual LLVM init: 1 configuration, %u checks, IR=%s; i64/i32/i32 ABI, dropped/empty data, high sparse address, exact diagnostics, no partial writes\n",test::checks,optimized?"O3":"baseline");
}
