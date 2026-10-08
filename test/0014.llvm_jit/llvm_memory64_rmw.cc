// Real memory64 native accesses through the production LLVM address/store emitters.
// This fixture does not yet exercise the Wasm parser or whole-VM memory64 frontend.
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
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
#if defined(UWVM_TEST_MEMORY64_ATOMIC)
constexpr bool atomic_test=true;
#else
constexpr bool atomic_test=false;
#endif
constexpr std::uint64_t data_address=520;
volatile std::sig_atomic_t expect_trap{},hardware_trap{},expected_unaligned{};
std::uint64_t expected_static{},expected_dynamic{};
std::size_t expected_width{};
unsigned expected_address_bits{};
std::byte const volatile* suffix{};
void untouched()
{for(unsigned i=0;i<65536;++i){if(suffix[i]!=std::byte{0x5a}){_Exit(93);}}}
void hardware_handler(int)
{if(!expect_trap||!hardware_trap){_Exit(94);}untouched();_Exit(92);}
namespace uwvm2::runtime::lib
{
 void llvm_jit_runtime_trap(llvm_jit_trap_kind kind,std::uintptr_t,std::uintptr_t) noexcept
 {
  if(!expect_trap||!expected_unaligned||kind!=llvm_jit_trap_kind::unaligned_atomic){_Exit(97);}
  untouched();_Exit(92);
 }
 void llvm_jit_memory_out_of_bounds_trap(std::size_t memory_index,std::uint_least64_t offset,
  std::uint_least64_t effective,std::uint_least32_t carry,std::uint_least64_t length,std::size_t width,std::uintptr_t,std::uintptr_t) noexcept
 {
  auto sum=static_cast<unsigned __int128>(expected_static)+expected_dynamic;
  if(!expect_trap||hardware_trap||expected_unaligned||memory_index!=7||offset!=expected_static||effective!=std::uint64_t(sum)||
     carry!=(expected_address_bits==32 ? sum>0xffffffffull : sum>>64)||length!=65536||width!=expected_width){_Exit(95);}
  untouched();_Exit(92);
 }
}
struct object_cache:llvm::ObjectCache
{
 std::string output;
 explicit object_cache(std::string out):output(std::move(out)){}
 void notifyObjectCompiled(llvm::Module const*,llvm::MemoryBufferRef buffer) override
 {std::error_code ec;llvm::raw_fd_ostream out(output,ec);CHECK(!ec);out<<buffer.getBuffer();}
 std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override{return {};}
};
void optimize(llvm::Module& module)
{
 llvm::PassBuilder p;llvm::LoopAnalysisManager l;llvm::FunctionAnalysisManager f;
 llvm::CGSCCAnalysisManager c;llvm::ModuleAnalysisManager m;
 p.registerModuleAnalyses(m);p.registerCGSCCAnalyses(c);p.registerFunctionAnalyses(f);p.registerLoopAnalyses(l);
 p.crossRegisterProxies(l,f,c,m);p.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3).run(module,m);
}
using operation=uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_operation;
struct test_case{std::string name;operation op;unsigned width,value_width,address_bits;bool guard;std::uint64_t offset;};
int main(int argc,char** argv)
{
 CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-real-rmw",context);module->setDataLayout("e-p:64:64");
 std::vector<test_case> cases;
 for(unsigned address_bits:{64u,32u})for(bool guard:{false,true})for(unsigned op=0;op<7;++op)for(unsigned kind=0;kind<7;++kind)
 for(std::uint64_t offset:{0ull,8ull,0x100000000ull,0xfffffffffffffff8ull})
 {
  if(address_bits==32&&offset>0xffffffffull){continue;}
  llvm::IRBuilder<> b(context);auto pointer_type=llvm::PointerType::getUnqual(context);
  constexpr unsigned atomic_bytes[]{4,8,1,2,1,2,4};unsigned width=atomic_bytes[kind],value_width=kind==0||kind==2||kind==3?4:8;
  auto name="access_"+std::to_string(cases.size());
  auto fn=llvm::Function::Create(llvm::FunctionType::get(b.getInt64Ty(),
    {pointer_type,b.getIntNTy(address_bits),b.getInt64Ty(),b.getInt64Ty(),b.getInt64Ty()},false),llvm::Function::ExternalLinkage,name,*module);
  b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",fn));
  CHECK(d::emit_llvm_jit_atomic_alignment(b,fn->getArg(1),offset,width));
  auto pointer=d::emit_llvm_jit_memory_address(b,fn->getArg(0),fn->getArg(1),offset,width,
    guard?d::llvm_jit_memory_protection::partial_guard:d::llvm_jit_memory_protection::software,1ull<<40,
    [&]()->llvm::Value*{return fn->getArg(2);},7);CHECK(pointer);
  auto type=b.getIntNTy(value_width*8);
  auto result=d::emit_llvm_jit_atomic_rmw(b,pointer,static_cast<operation>(op),width,
    b.CreateZExtOrTrunc(fn->getArg(3),type),b.CreateZExtOrTrunc(fn->getArg(4),type));CHECK(result);
  b.CreateRet(b.CreateZExtOrTrunc(result,b.getInt64Ty()));
  cases.push_back({name,static_cast<operation>(op),width,value_width,address_bits,guard,offset});
 }
 CHECK(!llvm::verifyModule(*module,&llvm::errs()));module->setDataLayout("");auto module_view=module.get();
 std::string error;object_cache cache(out+"/memory64.o");
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
   .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());
 if(!engine){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 module_view->setDataLayout(engine->getDataLayout());
 if(optimized){optimize(*module_view);CHECK(!llvm::verifyModule(*module_view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64.ll",ec);CHECK(!ec);module_view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 d::runtime_native_memory_t memory{};memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
 memory.init_by_page_count(1);std::size_t checks{};
 for(auto const& test:cases)
 {
  auto entry=engine->getFunctionAddress(test.name);CHECK(entry);
  auto fn=[&](std::uint64_t address,std::uint64_t input,std::uint64_t expected)->std::uint64_t
  {
   if(test.address_bits==64){return reinterpret_cast<std::uint64_t(*)(std::byte*,std::uint64_t,std::size_t,std::uint64_t,std::uint64_t)>(entry)(memory.memory_begin,address,65536,input,expected);}
   CHECK(address<=0xffffffffull);
   return reinterpret_cast<std::uint64_t(*)(std::byte*,std::uint32_t,std::size_t,std::uint64_t,std::uint64_t)>(entry)(memory.memory_begin,static_cast<std::uint32_t>(address),65536,input,expected);
  };
  auto const mask=~0ull>>(64-test.width*8);auto const initial=0x8585858585858585ull&mask;
  if(test.offset<=data_address)for(auto input:{0ull,~0ull,0x0123456789abcdefull})for(bool match:{true,false})
  {
   std::memset(memory.memory_begin+data_address-1,0x85,10);auto expected=initial^(match?~mask:1ull);
   auto old=fn(data_address-test.offset,input,expected);CHECK(old==initial);auto value=initial;
   switch(test.op)
   {
    case operation::add:value+=input;break;case operation::sub:value-=input;break;
    case operation::and_:value&=input;break;case operation::or_:value|=input;break;case operation::xor_:value^=input;break;
    case operation::exchange:value=input;break;case operation::compare_exchange:if(match){value=input;}break;
   }
   for(unsigned i=0;i<test.width;++i){CHECK(memory.memory_begin[data_address+i]==std::byte(value>>(i*8)));}
   CHECK(memory.memory_begin[data_address-1]==std::byte{0x85});CHECK(memory.memory_begin[data_address+test.width]==std::byte{0x85});++checks;
  }
  for(std::uint64_t address:std::array<std::uint64_t,6>{~0ull,1ull<<32,1ull<<48,0xffffffffull,data_address+1,65536ull-test.width+1})
  {
   if(test.address_bits==32&&address>0xffffffffull){continue;}
   auto sum=static_cast<unsigned __int128>(address)+test.offset;
   if(sum+test.width<=65536&&(std::uint64_t(sum)&(test.width-1))==0){continue;}
   std::memset(memory.memory_begin,0x5a,65536);auto pid=fork();CHECK(pid>=0);
   if(!pid)
   {
    expected_static=test.offset;expected_dynamic=address;expected_width=test.width;expected_address_bits=test.address_bits;expect_trap=1;
    expected_unaligned=std::uint64_t(sum)&(test.width-1);
    hardware_trap=!expected_unaligned&&test.guard&&sum<(1ull<<40)&&(test.address_bits==64||sum<=0xffffffffull);suffix=memory.memory_begin;
    std::signal(SIGSEGV,hardware_handler);std::signal(SIGBUS,hardware_handler);
    (void)fn(address,0xaaaaaaaaaaaaaaaaull,0x5a5a5a5a5a5a5a5aull);_Exit(96);
   }
   int status{};CHECK(waitpid(pid,&status,0)==pid);
   if(!WIFEXITED(status)||WEXITSTATUS(status)!=92){std::fprintf(stderr,"FAIL %s address=%llx status=%x\n",test.name.c_str(),(unsigned long long)address,status);return 3;}
   ++checks;
  }
  if(test.op==operation::add&&test.width==8&&test.offset==0)
  {
   std::memset(memory.memory_begin+data_address,0,8);
   std::vector<std::thread> workers;
   for(unsigned i=0;i<4;++i){workers.emplace_back([&]{for(unsigned n=0;n<1000;++n){(void)fn(data_address,1,0);}});}
   for(auto& worker:workers){worker.join();}
   CHECK(fn(data_address,0,0)==4000);++checks;
  }
 }
 std::printf("PASS memory64 actual LLVM RMW: %zu configurations, %zu checks; IR=%s; all 49 operations, i32/i64 addresses, CAS wrap/zero extension, concurrent RMW, u65/alignment traps\n",cases.size(),checks,optimized?"O3":"baseline");
}
