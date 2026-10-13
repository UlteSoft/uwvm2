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
#include "../0017.runtime/memory64_bulk_cases.h"
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace test=memory64_bulk_test;
namespace uwvm2::runtime::lib
{
 void llvm_jit_memory_out_of_bounds_trap(std::size_t index,std::uint_least64_t offset,std::uint_least64_t effective,
  std::uint_least32_t carry,std::uint_least64_t length,std::size_t width,std::uintptr_t,std::uintptr_t) noexcept
 {
  if(!test::trap_expected||index||offset||carry||effective!=test::expected_offset||length!=65536||width!=test::expected_length){_Exit(95);}
  test::complete_trap();
 }
 void llvm_jit_runtime_trap(llvm_jit_trap_kind,std::uintptr_t,std::uintptr_t) noexcept {_Exit(96);}
}
struct object_cache:llvm::ObjectCache
{
 std::string output;explicit object_cache(std::string out):output(std::move(out)){}
 void notifyObjectCompiled(llvm::Module const*,llvm::MemoryBufferRef buffer) override
 {std::error_code ec;llvm::raw_fd_ostream out(output,ec);BULK_CHECK(!ec);out<<buffer.getBuffer();}
 std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override{return {};}
};
void optimize(llvm::Module& module)
{
 llvm::PassBuilder p;llvm::LoopAnalysisManager l;llvm::FunctionAnalysisManager f;llvm::CGSCCAnalysisManager c;llvm::ModuleAnalysisManager m;
 p.registerModuleAnalyses(m);p.registerCGSCCAnalyses(c);p.registerFunctionAnalyses(f);p.registerLoopAnalyses(l);
 p.crossRegisterProxies(l,f,c,m);p.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3).run(module,m);
}
template<bool Copy,bool D64,bool S64>std::string name()
{return std::string("bulk_")+(Copy?"copy_":"fill_")+(D64?"64_":"32_")+(S64?"64":"32");}
template<bool Copy,bool D64,bool S64>void emit(llvm::Module& module)
{
 auto& context=module.getContext();llvm::IRBuilder<> b(context);auto intptr=b.getIntNTy(sizeof(std::uintptr_t)*8);
 auto type=llvm::FunctionType::get(b.getVoidTy(),{intptr,intptr,b.getInt64Ty(),b.getInt64Ty(),b.getInt64Ty()},false);
 auto fn=llvm::Function::Create(type,llvm::Function::ExternalLinkage,name<Copy,D64,S64>(),module);
 b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",fn));
 auto dst_type=D64?b.getInt64Ty():b.getInt32Ty();auto src_type=Copy&&S64?b.getInt64Ty():b.getInt32Ty();
 auto len_type=(Copy?(D64&&S64):D64)?b.getInt64Ty():b.getInt32Ty();
 auto dst=b.CreateZExtOrTrunc(fn->getArg(2),dst_type),src=b.CreateZExtOrTrunc(fn->getArg(3),src_type),len=b.CreateZExtOrTrunc(fn->getArg(4),len_type);
 llvm::CallInst* call{};
 if constexpr(Copy)
 {
  auto signature=llvm::FunctionType::get(b.getVoidTy(),{intptr,intptr,dst_type,src_type,len_type},false);
  auto bridge=d::get_llvm_runtime_bridge_function_symbol_value<d::llvm_jit_memory64_copy_bridge<D64,S64>>(b,signature);BULK_CHECK(bridge);
  call=b.CreateCall(signature,bridge,{fn->getArg(0),fn->getArg(1),dst,src,len});
 }
 else
 {
  auto signature=llvm::FunctionType::get(b.getVoidTy(),{intptr,dst_type,src_type,len_type},false);
  auto bridge=d::get_llvm_runtime_bridge_function_symbol_value<d::llvm_jit_memory64_fill_bridge<D64>>(b,signature);BULK_CHECK(bridge);
  call=b.CreateCall(signature,bridge,{fn->getArg(0),dst,src,len});
 }
 d::apply_llvm_jit_host_calling_conv(call);b.CreateRetVoid();
}
template<bool Copy,bool D64,bool S64>void run(llvm::ExecutionEngine& engine)
{
 using memory=d::runtime_native_memory_t;
 auto fn=reinterpret_cast<void(*)(std::uintptr_t,std::uintptr_t,std::uint64_t,std::uint64_t,std::uint64_t)>(engine.getFunctionAddress(name<Copy,D64,S64>()));
 BULK_CHECK(fn);
 auto invoke=[&](memory& dst,memory& src,std::uint64_t destination,std::uint64_t source,std::uint64_t length)
 {fn(reinterpret_cast<std::uintptr_t>(&dst),reinterpret_cast<std::uintptr_t>(&src),destination,source,length);};
 memory destination{},source{};test::initialize(destination);test::initialize(source);
 test::cases<Copy,D64,S64>(destination,source,invoke);test::high_addresses<Copy,D64,S64,memory>(invoke);
 if constexpr(Copy&&D64&&S64){test::concurrent_grow<memory>(invoke);}
}
int main(int argc,char** argv)
{
 BULK_CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-bulk-abi",context);
 emit<true,false,false>(*module);emit<true,false,true>(*module);emit<true,true,false>(*module);emit<true,true,true>(*module);
 emit<false,false,false>(*module);emit<false,true,false>(*module);BULK_CHECK(!llvm::verifyModule(*module,&llvm::errs()));
 auto view=module.get();std::string error;object_cache cache(out+"/memory64-bulk.o");
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
  .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());
 if(!engine){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 view->setDataLayout(engine->getDataLayout());if(optimized){optimize(*view);BULK_CHECK(!llvm::verifyModule(*view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64-bulk.ll",ec);BULK_CHECK(!ec);view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 run<true,false,false>(*engine);run<true,false,true>(*engine);run<true,true,false>(*engine);run<true,true,true>(*engine);
 run<false,false,false>(*engine);run<false,true,false>(*engine);
 std::printf("PASS memory64 actual LLVM bulk: 6 configurations, %u checks, IR=%s; mixed typed ABI, sparse high addresses, overlap, full diagnostic checks, no partial mutation\n",
  test::checks,optimized?"O3":"baseline");
}
