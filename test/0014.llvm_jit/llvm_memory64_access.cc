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
#include <sys/wait.h>
#include <unistd.h>
namespace d=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
#if defined(UWVM_TEST_MEMORY64_ATOMIC)
constexpr bool atomic_test=true;
#else
constexpr bool atomic_test=false;
#endif
constexpr std::uint64_t data_address=atomic_test?520:513;
volatile std::sig_atomic_t expect_trap{},hardware_trap{},expected_unaligned{};
std::uint64_t expected_static{},expected_dynamic{};
std::size_t expected_width{};
unsigned expected_address_bits{};
std::byte const volatile* suffix{};
void untouched()
{for(unsigned i=0;i<32;++i){if(suffix[i]!=std::byte{0x5a}){_Exit(93);}}}
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
struct test_case{std::string name;unsigned width,value_width,address_bits;bool store,guard;std::uint64_t offset;};
int main(int argc,char** argv)
{
 CHECK(argc==2||argc==3);bool optimized=argc==3;std::string out=argv[1];
 llvm::InitializeNativeTarget();llvm::InitializeNativeTargetAsmPrinter();llvm::LLVMContext context;
 auto module=std::make_unique<llvm::Module>("memory64-real-access",context);module->setDataLayout("e-p:64:64");
 std::vector<test_case> cases;
 for(unsigned address_bits:{64u,32u})
 for(bool guard:{false,true})for(bool store:{false,true})for(unsigned kind=0;kind<7;++kind)
 for(std::uint64_t offset:{0ull,4ull,0x100000000ull,0xfffffffffffffffcull})
 {
  if(address_bits==32&&(!atomic_test||offset>0xffffffffull)){continue;}
  llvm::IRBuilder<> b(context);auto pointer_type=llvm::PointerType::getUnqual(context);
  constexpr unsigned atomic_bytes[]{4,8,1,2,1,2,4};
  unsigned width=atomic_test?atomic_bytes[kind]:(kind<4?1u<<kind:kind==4?4:kind==5?8:16);
  unsigned value_width=atomic_test?(kind==0||kind==2||kind==3?4:8):width;
  llvm::Type* type=atomic_test?static_cast<llvm::Type*>(b.getIntNTy(value_width*8)):kind<4?static_cast<llvm::Type*>(b.getIntNTy(width*8)):
    kind==4?b.getFloatTy():kind==5?b.getDoubleTy():static_cast<llvm::Type*>(llvm::FixedVectorType::get(b.getInt8Ty(),16));
  auto name="access_"+std::to_string(cases.size());
  auto fn=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{pointer_type,b.getIntNTy(address_bits),b.getInt64Ty(),pointer_type},false),
    llvm::Function::ExternalLinkage,name,*module);
  b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",fn));
  if constexpr(atomic_test){CHECK(d::emit_llvm_jit_atomic_alignment(b,fn->getArg(1),offset,width));}
  auto pointer=d::emit_llvm_jit_memory_address(b,fn->getArg(0),fn->getArg(1),offset,width,
    guard?d::llvm_jit_memory_protection::partial_guard:d::llvm_jit_memory_protection::software,1ull<<40,
    [&]()->llvm::Value*{return fn->getArg(2);},7);
  CHECK(pointer);
  if(store)
  {
   if(guard&&!atomic_test){d::emit_llvm_jit_guarded_store_preflight(b,pointer,b.CreateAdd(fn->getArg(1),b.getInt64(offset)),width,16);}
   auto value=b.CreateLoad(type,fn->getArg(3));value->setAlignment(llvm::Align{1});
   if constexpr(atomic_test){CHECK(d::emit_llvm_jit_atomic_store(b,pointer,width,value));}
   else {CHECK(d::finalize_llvm_jit_direct_memory_store(b.CreateStore(value,pointer),llvm::Align{1}));}
  }
  else
  {
   llvm::Value* value{};
   if constexpr(atomic_test){value=d::emit_llvm_jit_atomic_load(b,pointer,width,static_cast<llvm::IntegerType*>(type));CHECK(value);}
   else {auto load=b.CreateLoad(type,pointer);load->setAlignment(llvm::Align{1});load->setVolatile(true);value=load;}
   auto output=b.CreateStore(value,fn->getArg(3));output->setAlignment(llvm::Align{1});
  }
  b.CreateRetVoid();cases.push_back({name,width,value_width,address_bits,store,guard,offset});
 }
 // Exercise the actual i64 host bridge ABI via generated code, including
 // invalid high deltas. No guest address participates in the native-object ABI.
 for(bool grow:{false,true})
 {
  llvm::IRBuilder<> b(context);auto intptr=b.getIntNTy(sizeof(std::uintptr_t)*8);
  auto type=llvm::FunctionType::get(b.getInt64Ty(),grow?std::vector<llvm::Type*>{intptr,b.getInt64Ty()}:std::vector<llvm::Type*>{intptr},false);
  auto fn=llvm::Function::Create(type,llvm::Function::ExternalLinkage,grow?"memory64_grow":"memory64_size",*module);
  b.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",fn));
  llvm::CallInst* call{};
  if(grow)
  {
   auto bridge_type=llvm::FunctionType::get(b.getInt64Ty(),{intptr,intptr,b.getInt64Ty()},false);
   auto bridge=d::get_llvm_runtime_bridge_function_symbol_value<d::llvm_jit_memory64_grow_bridge>(b,bridge_type);CHECK(bridge);
   call=b.CreateCall(bridge_type,bridge,{fn->getArg(0),llvm::ConstantInt::get(intptr,4*65536),fn->getArg(1)});
  }
  else
  {
   auto bridge=d::get_llvm_runtime_bridge_function_symbol_value<d::llvm_jit_memory64_size_bridge>(b,type);CHECK(bridge);
   call=b.CreateCall(type,bridge,{fn->getArg(0)});
  }
  d::apply_llvm_jit_host_calling_conv(call);b.CreateRet(call);
 }
 {
  auto file=std::fopen((out+"/cases.json").c_str(),"w");CHECK(file);std::fprintf(file,"[\n");
  for(std::size_t i=0;i<cases.size();++i)
  {
   auto const& c=cases[i];std::fprintf(file,"{\"name\":\"%s\",\"width\":%u,\"address_bits\":%u,\"store\":%s}%s\n",
    c.name.c_str(),c.width,c.address_bits,c.store?"true":"false",i+1==cases.size()?"":",");
  }
  std::fprintf(file,"]\n");CHECK(std::fclose(file)==0);
 }
 CHECK(!llvm::verifyModule(*module,&llvm::errs()));
 // Emission above fixes address/pointer widths. Let MCJIT supply the complete
 // native target layout before optimization and final machine-code emission.
 module->setDataLayout("");auto module_view=module.get();
 std::string error;object_cache cache(out+"/memory64.o");
 std::unique_ptr<llvm::ExecutionEngine> engine(llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
   .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).create());
 if(!engine){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 module_view->setDataLayout(engine->getDataLayout());
 if(optimized){optimize(*module_view);CHECK(!llvm::verifyModule(*module_view,&llvm::errs()));}
 {std::error_code ec;llvm::raw_fd_ostream ir(out+"/memory64.ll",ec);CHECK(!ec);module_view->print(ir,nullptr);}
 engine->setObjectCache(&cache);engine->finalizeObject();
 d::runtime_native_memory_t memory{};
#if defined(UWVM_SUPPORT_MMAP)
 memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
#else
#error "Guard-path qualification requires a real mmap reservation"
#endif
 memory.init_by_page_count(1);std::size_t checks{};
 for(auto const& test:cases)
 {
  auto entry=engine->getFunctionAddress(test.name);CHECK(entry);
  auto fn=[&](std::byte* base,std::uint64_t address,std::size_t length,std::byte* payload)
  {
   if(test.address_bits==64){reinterpret_cast<void(*)(std::byte*,std::uint64_t,std::size_t,std::byte*)>(entry)(base,address,length,payload);}
   else {CHECK(address<=0xffffffffull);reinterpret_cast<void(*)(std::byte*,std::uint32_t,std::size_t,std::byte*)>(entry)(base,static_cast<std::uint32_t>(address),length,payload);}
  };
  for(auto pattern:{0x0123456789abcdefull,0x7ff000007f800001ull,0xfff00000ff800007ull,0x8000000080000000ull})
  {
   if(test.offset>data_address){continue;}
   std::array<std::byte,16> payload{},result{};
   for(unsigned i=0;i<16;++i){payload[i]=std::byte((i<8?pattern:~pattern)>>(8*(i%8)));}
   std::memset(memory.memory_begin+data_address-1,0xa5,18);
   if(test.store){fn(memory.memory_begin,data_address-test.offset,65536,payload.data());CHECK(!std::memcmp(memory.memory_begin+data_address,payload.data(),test.width));}
   else {std::memcpy(memory.memory_begin+data_address,payload.data(),test.width);fn(memory.memory_begin,data_address-test.offset,65536,result.data());CHECK(!std::memcmp(result.data(),payload.data(),test.width));}
   if constexpr(atomic_test){if(!test.store){for(unsigned i=test.width;i<test.value_width;++i){CHECK(result[i]==std::byte{});}}}
   CHECK(memory.memory_begin[data_address-1]==std::byte{0xa5});CHECK(memory.memory_begin[data_address+test.width]==std::byte{0xa5});++checks;
  }
  std::vector<std::uint64_t> addresses{~0ull,1ull<<32,1ull<<48,16,0xffffffffull,data_address+1};
  if(test.offset<=65536-test.width+1){addresses.push_back(65536-test.width+1-test.offset);}
  for(auto address:addresses)
  {
   if(test.address_bits==32&&address>0xffffffffull){continue;}
   auto sum=static_cast<unsigned __int128>(address)+test.offset;
   if(sum+test.width<=65536&&(!atomic_test||(std::uint64_t(sum)&(test.width-1))==0)){continue;}
   std::memset(memory.memory_begin+65536-32,0x5a,32);auto pid=fork();CHECK(pid>=0);
   if(!pid)
   {
    expected_static=test.offset;expected_dynamic=address;expected_width=test.width;expected_address_bits=test.address_bits;expect_trap=1;
    expected_unaligned=atomic_test&&(std::uint64_t(sum)&(test.width-1));
    hardware_trap=!expected_unaligned&&test.guard&&sum<(1ull<<40)&&(test.address_bits==64||sum<=0xffffffffull);suffix=memory.memory_begin+65536-32;
    std::signal(SIGSEGV,hardware_handler);std::signal(SIGBUS,hardware_handler);
    std::array<std::byte,16> buffer{};fn(memory.memory_begin,address,65536,buffer.data());_Exit(96);
   }
   int status{};CHECK(waitpid(pid,&status,0)==pid);
   if(!WIFEXITED(status)||WEXITSTATUS(status)!=92)
   {std::fprintf(stderr,"FAIL %s address=%llx status=%x\n",test.name.c_str(),(unsigned long long)address,status);return 3;}
   ++checks;
  }
 }
 auto grow=reinterpret_cast<std::uint64_t(*)(std::uintptr_t,std::uint64_t)>(engine->getFunctionAddress("memory64_grow"));
 auto size=reinterpret_cast<std::uint64_t(*)(std::uintptr_t)>(engine->getFunctionAddress("memory64_size"));CHECK(grow&&size);
 for(bool strict:{false,true})
 {
  uwvm2::object::memory::flags::grow_strict=strict;
  d::runtime_native_memory_t growing{};growing.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
  growing.init_by_page_count(1);auto address=reinterpret_cast<std::uintptr_t>(&growing);
  CHECK(size(address)==1);CHECK(grow(address,0)==1);CHECK(grow(address,1)==1);
  CHECK(grow(address,0x100000000ull)==~0ull);CHECK(grow(address,2)==2);CHECK(grow(address,1)==~0ull);
  CHECK(grow(address,~0ull)==~0ull);CHECK(grow(address,0)==4);CHECK(size(address)==4);checks+=9;
 }
 CHECK(grow(0,1)==~0ull);CHECK(size(0)==~0ull);checks+=2;
 std::printf("PASS memory64 actual LLVM %saccesses: %zu configurations, %zu checks; IR=%s; %s, u65 traps, unchanged failed stores, i64 grow/size bridge ABI\n",
  atomic_test?"atomic ":"",cases.size(),checks,optimized?"O3":"baseline",atomic_test?"i32/i64 addresses, integer widths, exact alignment":"FP/NaN/vector bytes");
}
