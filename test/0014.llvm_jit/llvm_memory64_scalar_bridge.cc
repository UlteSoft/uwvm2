// Actual generated memory64 scalar fallback calls with integer bit carriers.
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
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
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
struct scalar_spec{unsigned width,bits;bool store,sign,floating;};
constexpr scalar_spec describe(unsigned op)
{
 constexpr scalar_spec all[]{
  {4,32,false,false,false},{8,64,false,false,false},{4,32,false,false,true},{8,64,false,false,true},
  {1,32,false,true,false},{1,32,false,false,false},{2,32,false,true,false},{2,32,false,false,false},
  {1,64,false,true,false},{1,64,false,false,false},{2,64,false,true,false},{2,64,false,false,false},
  {4,64,false,true,false},{4,64,false,false,false},
  {4,32,true,false,false},{8,64,true,false,false},{4,32,true,false,true},{8,64,true,false,true},
  {1,32,true,false,false},{2,32,true,false,false},{1,64,true,false,false},{2,64,true,false,false},{4,64,true,false,false}};
 return all[op-0x28];
}
std::array<std::byte,16> expected(unsigned opcode,std::array<std::byte,16> const& wire,std::array<std::byte,16> const& value)
{
 auto s=describe(opcode);if(s.store)return value;
 std::array<std::byte,16> result{};
 for(unsigned n=0;n<s.bits/8;++n)
 {
  // Independent byte-level sign extension avoids using the production arithmetic formula.
  result[n]=n<s.width?wire[n]:s.sign&&(std::to_integer<unsigned>(wire[s.width-1])&128)?std::byte{255}:std::byte{};
 }
 return result;
}
struct test_case{std::string name;unsigned opcode,lane,kind;std::uint64_t offset;};
template<unsigned Code>void emit(llvm::Module& module,std::vector<test_case>& cases)
{
 constexpr auto spec=describe(Code);
 for(std::uint64_t offset:{4ull,0xfffffffffffffffcull})
 {
  llvm::IRBuilder<> b(module.getContext());auto pointer=llvm::PointerType::getUnqual(module.getContext());auto intptr=b.getIntNTy(sizeof(std::uintptr_t)*8);
  auto fn=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{intptr,pointer,b.getInt64Ty(),intptr,pointer,pointer},false),
   llvm::Function::ExternalLinkage,"access_"+std::to_string(cases.size()),module);
  b.SetInsertPoint(llvm::BasicBlock::Create(module.getContext(),"entry",fn));
  llvm::Type* value_type=spec.floating?(spec.bits==32?b.getFloatTy():b.getDoubleTy()):b.getIntNTy(spec.bits);
  llvm::Value* value{};
  if constexpr(spec.store)
  {
   auto load=b.CreateLoad(b.getIntNTy(spec.bits),fn->getArg(4));load->setAlignment(llvm::Align{1});
   value=b.CreateBitCast(load,value_type);
  }
  auto result=d::emit_llvm_jit_memory64_scalar_call<spec.bits,spec.width,spec.sign,spec.store>(
   b,fn->getArg(0),b.getInt64(offset),fn->getArg(2),value_type,value);CHECK(result);
  if constexpr(!spec.store)
  {
   auto bits=b.CreateZExtOrTrunc(b.CreateBitCast(result,b.getIntNTy(spec.bits)),b.getInt64Ty());
   b.CreateStore(bits,fn->getArg(5))->setAlignment(llvm::Align{1});
  }
  b.CreateRetVoid();cases.push_back({fn->getName().str(),Code,0,2,offset});
 }
}
int main(int argc,char** argv)
{
 CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::InitializeNativeTargetAsmParser();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-scalar-bridge",context);module->setDataLayout("e-p:64:64");std::vector<test_case> cases;
 []<unsigned... Code>(llvm::Module& module,std::vector<test_case>& cases,std::integer_sequence<unsigned,Code...>){(emit<Code>(module,cases),...);}
 (*module,cases,std::integer_sequence<unsigned,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62>{});
 CHECK(!llvm::verifyModule(*module,&llvm::errs()));module->setDataLayout("");auto view=module.get();
 auto file=std::fopen((out+"/cases.json").c_str(),"w");CHECK(file);std::fprintf(file,"[\n");
 for(std::size_t i=0;i<cases.size();++i)
 {auto const& t=cases[i];std::fprintf(file,"{\"name\":\"%s\",\"opcode\":%u,\"lane\":%u,\"kind\":%u,\"width\":%u,\"store\":%s}%s\n",
  t.name.c_str(),t.opcode,t.lane,t.kind,describe(t.opcode).width,describe(t.opcode).store?"true":"false",i+1==cases.size()?"":",");}
 std::fprintf(file,"]\n");CHECK(std::fclose(file)==0);
 object_cache cache(out+"/memory64-scalar-bridge.o");std::string error;
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error).setEngineKind(llvm::EngineKind::JIT)
  .setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());CHECK(engine);
 view->setDataLayout(engine->getDataLayout());if(optimized){optimize(*view);CHECK(!llvm::verifyModule(*view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64-scalar-bridge.ll",ec);CHECK(!ec);view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 d::runtime_native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#endif
 memory.init_by_page_count(1);memory.diagnostic_owner_memory_index=7;unsigned checks{};
 using function=void(*)(std::uintptr_t,std::byte*,std::uint64_t,std::size_t,std::byte*,std::byte*);
 for(auto const& t:cases)
 {
  auto fn=reinterpret_cast<function>(engine->getFunctionAddress(t.name));CHECK(fn);auto spec=describe(t.opcode);
  std::array<std::byte,16> wire{},old{},result{};
  for(unsigned n=0;n<16;++n){wire[n]=std::byte(0x83+13*n);old[n]=std::byte(0x31+17*n);}
  if(t.offset==4) for(std::uint64_t pattern:{0ull,~0ull,0x0123456789abcdefull,0x8000000080000000ull,0x7ff000017f800001ull,0xfff00001ff800001ull})
  {
   for(unsigned n=0;n<16;++n){wire[n]=std::byte((pattern>>(8*(n%8)))&255);old[n]=wire[n];}
   std::memset(memory.memory_begin+512,0xa5,18);if(!spec.store)std::memcpy(memory.memory_begin+513,wire.data(),16);
   auto expect=expected(t.opcode,wire,old);
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
 std::printf("PASS memory64 actual LLVM scalar bridges: %zu configurations, %u checks, IR=%s; all 23 scalar opcodes, integer extensions/float payloads, u65/native bounds, exact trap diagnostics and failed-store immutability\n",cases.size(),checks,optimized?"O3":"baseline");
}
