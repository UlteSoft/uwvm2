// Actual generated memory64 SIMD accesses and native fallback bridge calls.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Passes/PassBuilder.h>
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include "../0017.runtime/memory64_simd_reference.h"
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace reference=memory64_simd_reference;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
volatile std::sig_atomic_t expected_trap{},hardware_trap{};
std::uint64_t expected_address{},expected_offset{};std::size_t expected_width{};
std::byte const volatile* suffix{};
void intact(){for(unsigned n=0;n<32;++n)if(suffix[n]!=std::byte{0x5a})_Exit(91);}
void hardware(int){if(!expected_trap||!hardware_trap)_Exit(93);intact();_Exit(92);}
namespace uwvm2::runtime::lib
{
 void llvm_jit_runtime_trap(llvm_jit_trap_kind,std::uintptr_t,std::uintptr_t) noexcept {_Exit(94);}
 void llvm_jit_memory_out_of_bounds_trap(std::size_t index,std::uint_least64_t offset,std::uint_least64_t effective,
  std::uint_least32_t carry,std::uint_least64_t length,std::size_t width,std::uintptr_t,std::uintptr_t) noexcept
 {
  auto low=expected_address+expected_offset;
  if(!expected_trap||hardware_trap||index!=7||offset!=expected_offset||effective!=low||carry!=(low<expected_address)||length!=65536||width!=expected_width)_Exit(95);
  intact();_Exit(92);
 }
}
struct object_cache:llvm::ObjectCache
{
 std::string output;explicit object_cache(std::string s):output(std::move(s)){}
 void notifyObjectCompiled(llvm::Module const*,llvm::MemoryBufferRef buffer) override
 {std::error_code ec;llvm::raw_fd_ostream out(output,ec);CHECK(!ec);out<<buffer.getBuffer();}
 std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override{return {};}
};
void optimize(llvm::Module& module)
{
 llvm::PassBuilder p;llvm::LoopAnalysisManager l;llvm::FunctionAnalysisManager f;llvm::CGSCCAnalysisManager c;llvm::ModuleAnalysisManager m;
 p.registerModuleAnalyses(m);p.registerCGSCCAnalyses(c);p.registerFunctionAnalyses(f);p.registerLoopAnalyses(l);
 p.crossRegisterProxies(l,f,c,m);p.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3).run(module,m);
}
struct test_case{std::string name;unsigned opcode,lane,kind;std::uint64_t offset;};
template<unsigned Code>void emit(llvm::Module& module,std::vector<test_case>& cases)
{
 constexpr auto op=static_cast<d::llvm_jit_simd_code>(Code);constexpr auto spec=reference::describe(Code);
 for(unsigned lane=0;lane<(spec.lane?16/spec.width:1);++lane)for(unsigned kind=0;kind<3;++kind)for(std::uint64_t offset:{4ull,0xfffffffffffffffcull})
 {
#if !defined(UWVM_SUPPORT_MMAP)
  if(kind!=2)continue; // Moving allocations require the production pinned bridge.
#endif
  llvm::IRBuilder<> b(module.getContext());auto pointer=llvm::PointerType::getUnqual(module.getContext());auto intptr=b.getIntNTy(sizeof(std::uintptr_t)*8);
  auto fn=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{intptr,pointer,b.getInt64Ty(),intptr,pointer,pointer},false),
   llvm::Function::ExternalLinkage,"access_"+std::to_string(cases.size()),module);
  b.SetInsertPoint(llvm::BasicBlock::Create(module.getContext(),"entry",fn));
  if(kind==2)
  {
   auto call=d::emit_llvm_jit_memory64_simd_call<op>(b,fn->getArg(0),b.getInt64(offset),fn->getArg(2),
    b.CreatePtrToInt(fn->getArg(4),intptr),lane,b.CreatePtrToInt(fn->getArg(5),intptr));CHECK(call);
  }
  else
  {
   auto effective=d::emit_llvm_jit_memory_address(b,fn->getArg(1),fn->getArg(2),offset,spec.width,
    kind==1?d::llvm_jit_memory_protection::partial_guard:d::llvm_jit_memory_protection::software,1ull<<40,
    [&]()->llvm::Value*{return fn->getArg(3);},7);CHECK(effective);
   llvm::Value* value{};
   if constexpr(spec.consumes)
   {auto load=b.CreateLoad(llvm::FixedVectorType::get(b.getInt8Ty(),16),fn->getArg(4));load->setAlignment(llvm::Align{1});value=load;}
   if constexpr(spec.store)
   {
    if(kind==1){d::emit_llvm_jit_guarded_store_preflight(b,effective,b.CreateAdd(fn->getArg(2),b.getInt64(offset)),spec.width,16);}
    CHECK(d::simd_ir::emit_store<op>(b,effective,value,lane));
   }
   else
   {auto result=d::simd_ir::emit_load<op>(b,effective,value,lane);CHECK(result);b.CreateStore(result,fn->getArg(5))->setAlignment(llvm::Align{1});}
  }
  b.CreateRetVoid();cases.push_back({fn->getName().str(),Code,lane,kind,offset});
 }
}
int main(int argc,char** argv)
{
 CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-simd",context);module->setDataLayout("e-p:64:64");std::vector<test_case> cases;
 []<unsigned... Code>(llvm::Module& module,std::vector<test_case>& cases,std::integer_sequence<unsigned,Code...>){(emit<Code>(module,cases),...);}
 (*module,cases,std::integer_sequence<unsigned,0,1,2,3,4,5,6,7,8,9,10,11,84,85,86,87,88,89,90,91,92,93>{});
 CHECK(!llvm::verifyModule(*module,&llvm::errs()));module->setDataLayout("");auto view=module.get();
 auto file=std::fopen((out+"/cases.json").c_str(),"w");CHECK(file);std::fprintf(file,"[\n");
 for(std::size_t i=0;i<cases.size();++i)
 {auto const& t=cases[i];std::fprintf(file,"{\"name\":\"%s\",\"opcode\":%u,\"lane\":%u,\"kind\":%u,\"width\":%u,\"store\":%s}%s\n",
  t.name.c_str(),t.opcode,t.lane,t.kind,reference::describe(t.opcode).width,reference::describe(t.opcode).store?"true":"false",i+1==cases.size()?"":",");}
 std::fprintf(file,"]\n");CHECK(std::fclose(file)==0);
 object_cache cache(out+"/memory64-simd.o");std::string error;
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error).setEngineKind(llvm::EngineKind::JIT)
  .setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());CHECK(engine);
 view->setDataLayout(engine->getDataLayout());if(optimized){optimize(*view);CHECK(!llvm::verifyModule(*view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64-simd.ll",ec);CHECK(!ec);view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 d::runtime_native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#endif
 memory.init_by_page_count(1);memory.diagnostic_owner_memory_index=7;unsigned checks{};
 using function=void(*)(std::uintptr_t,std::byte*,std::uint64_t,std::size_t,std::byte*,std::byte*);
 for(auto const& t:cases)
 {
  auto fn=reinterpret_cast<function>(engine->getFunctionAddress(t.name));CHECK(fn);auto spec=reference::describe(t.opcode);
  std::array<std::byte,16> wire{},old{},result{};
  for(unsigned n=0;n<16;++n){wire[n]=std::byte(0x83+13*n);old[n]=std::byte(0x31+17*n);}
  if(t.offset==4)
  {
   std::memset(memory.memory_begin+512,0xa5,18);if(!spec.store)std::memcpy(memory.memory_begin+513,wire.data(),16);
   auto expect=reference::expected(t.opcode,t.lane,wire,old);
   fn(reinterpret_cast<std::uintptr_t>(&memory),memory.memory_begin,509,65536,old.data(),result.data());
   if(spec.store)
   {CHECK(!std::memcmp(memory.memory_begin+513,expect.data(),spec.width));CHECK(memory.memory_begin[513+spec.width]==std::byte{0xa5});}
   else {CHECK(result==expect);}
   CHECK(memory.memory_begin[512]==std::byte{0xa5});++checks;
  }
  std::vector<std::uint64_t> addresses{~0ull,1ull<<32,1ull<<48,16};
  if(t.offset==4)addresses.push_back(65536-spec.width+1-t.offset);
  for(auto address:addresses)
  {
   auto sum=address+t.offset;if(sum>=address&&sum<=65536&&spec.width<=65536-sum)continue;
   std::memset(memory.memory_begin+65536-32,0x5a,32);auto pid=fork();CHECK(pid>=0);
   if(!pid)
   {
    expected_trap=1;expected_address=address;expected_offset=t.offset;expected_width=spec.width;suffix=memory.memory_begin+65536-32;
    auto low=address+t.offset;hardware_trap=t.kind==1&&low>=address&&low<(1ull<<40);
    std::signal(SIGSEGV,hardware);std::signal(SIGBUS,hardware);
    fn(reinterpret_cast<std::uintptr_t>(&memory),memory.memory_begin,address,65536,old.data(),result.data());_Exit(96);
   }
   int status{};CHECK(waitpid(pid,&status,0)==pid);
   if(!WIFEXITED(status)||WEXITSTATUS(status)!=92){std::fprintf(stderr,"FAIL %s opcode=%u lane=%u kind=%u offset=%llx addr=%llx status=%x\n",
    t.name.c_str(),t.opcode,t.lane,t.kind,(unsigned long long)t.offset,(unsigned long long)address,status);return 3;}
   ++checks;
  }
 }
 std::printf("PASS memory64 actual LLVM SIMD: %zu configurations, %u checks, IR=%s; all 22 opcodes/all lanes, u65/native bounds, exact trap diagnostics and failed-store immutability\n",cases.size(),checks,optimized?"O3":"baseline");
}
