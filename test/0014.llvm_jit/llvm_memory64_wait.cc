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
#include "../0017.runtime/memory64_wait_cases.h"
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace test=memory64_wait_test;
namespace uwvm2::runtime::lib
{
 void llvm_jit_memory_out_of_bounds_trap(std::size_t index,std::uint_least64_t offset,std::uint_least64_t effective,
  std::uint_least32_t carry,std::uint_least64_t length,std::size_t width,std::uintptr_t,std::uintptr_t) noexcept
 {
  auto sum=static_cast<unsigned __int128>(test::expected_offset)+test::expected_address;
  if(test::expected_trap!=2||index||offset!=test::expected_offset||effective!=std::uint64_t(sum)||carry!=(sum>>64)||
     length!=test::expected_length||width!=test::expected_bytes){_Exit(95);}
  test::complete_trap(2);
 }
 void llvm_jit_runtime_trap(llvm_jit_trap_kind kind,std::uintptr_t,std::uintptr_t) noexcept
 {
  using trap=llvm_jit_trap_kind;
  test::complete_trap(kind==trap::unaligned_atomic?1:kind==trap::atomic_wait_non_shared?3:kind==trap::atomic_wait_cancelled?4:9);
 }
}
struct object_cache:llvm::ObjectCache
{
 std::string output;explicit object_cache(std::string out):output(std::move(out)){}
 void notifyObjectCompiled(llvm::Module const*,llvm::MemoryBufferRef buffer) override
 {std::error_code ec;llvm::raw_fd_ostream out(output,ec);WAIT_CHECK(!ec);out<<buffer.getBuffer();}
 std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override{return {};}
};
void optimize(llvm::Module& module)
{
 llvm::PassBuilder p;llvm::LoopAnalysisManager l;llvm::FunctionAnalysisManager f;llvm::CGSCCAnalysisManager c;llvm::ModuleAnalysisManager m;
 p.registerModuleAnalyses(m);p.registerCGSCCAnalyses(c);p.registerFunctionAnalyses(f);p.registerLoopAnalyses(l);
 p.crossRegisterProxies(l,f,c,m);p.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3).run(module,m);
}
template<unsigned Operation>void emit(llvm::Module& module)
{
 llvm::IRBuilder<> b(module.getContext());auto intptr=b.getIntNTy(sizeof(std::uintptr_t)*8);
 auto carrier=b.getIntNTy(sizeof(d::llvm_jit_atomic_address_carrier_t)*8);
 auto type=llvm::FunctionType::get(carrier,{intptr,b.getInt64Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getInt64Ty()},false);
 auto fn=llvm::Function::Create(type,llvm::Function::ExternalLinkage,"wait_"+std::to_string(Operation),module);
 b.SetInsertPoint(llvm::BasicBlock::Create(module.getContext(),"entry",fn));
 auto call=d::emit_llvm_jit_memory64_wait_notify_call<Operation>(b,fn->getArg(0),fn->getArg(1),fn->getArg(2),fn->getArg(3),fn->getArg(4));WAIT_CHECK(call);
 b.CreateRet(b.CreateZExtOrTrunc(call,carrier));
}
int main(int argc,char** argv)
{
 WAIT_CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-wait-abi",context);
 emit<0>(*module);emit<1>(*module);emit<2>(*module);WAIT_CHECK(!llvm::verifyModule(*module,&llvm::errs()));
 auto view=module.get();std::string error;object_cache cache(out+"/memory64-wait.o");
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
  .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());
 if(!engine){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 view->setDataLayout(engine->getDataLayout());if(optimized){optimize(*view);WAIT_CHECK(!llvm::verifyModule(*view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64-wait.ll",ec);WAIT_CHECK(!ec);view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 using bridge=d::llvm_jit_atomic_address_carrier_t(*)(std::uintptr_t,std::uint64_t,std::uint64_t,std::uint64_t,std::int64_t);
 std::array<bridge,3> functions{};for(unsigned i=0;i<3;++i){functions[i]=reinterpret_cast<bridge>(engine->getFunctionAddress("wait_"+std::to_string(i)));WAIT_CHECK(functions[i]);}
 test::cases<d::runtime_native_memory_t>([&]<unsigned Operation>(auto& memory,auto offset,auto address,auto expected,auto timeout)
 {return functions[Operation](reinterpret_cast<std::uintptr_t>(&memory),offset,address,expected,timeout);});
 std::printf("PASS memory64 actual LLVM wait/notify: 3 configurations, %u checks, IR=%s; i64 bridge ABI, high addresses, full diagnostic checks, VM domain, grow, cancellation\n",test::checks,optimized?"O3":"baseline");
}
