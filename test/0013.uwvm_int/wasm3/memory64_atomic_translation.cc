// Binary memory64 declarations flow through parser, initializer, validation,
// compilation and execution. No declaration metadata is mutated by this fixture.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <uwvm2/runtime/wasm_threads/impl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
#include "memory64_translation_ir.h"
namespace
{
using namespace uwvm2test::uwvm_int_strict;
namespace w3=uwvm2::validation::standard::wasm3;
using kind=w3::atomic_instruction_kind;
constexpr unsigned opcode_at(unsigned index){return index<4?index:index+12;}
void add_function(module_builder& b,unsigned opcode,bool wide,bool invalid,bool huge=false)
{
 auto d=w3::describe_atomic_instruction(opcode);func_type t{{},{k_val_i64}};func_body f{};
 auto const result=d.result_i64?k_val_i64:k_val_i32;
 // Five older operands force spill/refill in the merged and disjoint tiny rings.
 for(unsigned n=1;n<=5;++n){append_u8(f.code,d.result_i64?0x42:0x41);append_u8(f.code,n);}
 for(unsigned n=0;n<d.operand_count;++n)
 {
  auto type=n==0?((wide^invalid)?k_val_i64:k_val_i32):
   (d.value_i64||((opcode==1||opcode==2)&&n==2))?k_val_i64:k_val_i32;
  t.params.push_back(type);append_u8(f.code,0x20);append_u8(f.code,n);
 }
 append_u8(f.code,0xfe);append_u32_leb(f.code,opcode);
 if(opcode==3)append_u8(f.code,0);
 else
 {
  append_u8(f.code,64+d.natural_alignment);append_u8(f.code,wide?1:0);
  if(huge){for(unsigned n=0;n<9;++n)append_u8(f.code,255);append_u8(f.code,1);}
  else append_u8(f.code,8);
 }
 if(!d.has_result){append_u8(f.code,0x41);append_u8(f.code,0);}
 for(unsigned n=0;n<5;++n)append_u8(f.code,result==k_val_i64?0x7c:0x6a);
 if(result==k_val_i32)append_u8(f.code,0xad);
 append_u8(f.code,11);b.add_func(std::move(t),std::move(f));
}
byte_vec build(int invalid=-1)
{
 module_builder b{};b.has_memory=b.memory_has_max=b.memory_shared=true;b.memory_min=b.memory_max=1;
 b.extra_memories.push_back({1,1,true,true,true});
 if(invalid>=0)add_function(b,opcode_at(invalid),true,true);
 else
 {
  for(bool wide:{false,true})for(unsigned index=0;index<67;++index)add_function(b,opcode_at(index),wide,false);
  add_function(b,0x12,true,false,true);
 }
 return b.build();
}
void append_bits(byte_vec& data,std::uint64_t value,unsigned bytes)
{auto at=data.size();data.resize(at+bytes);std::memcpy(data.data()+at,&value,bytes);}
byte_vec arguments(unsigned opcode,bool wide,std::uint64_t address,std::uint64_t input,std::uint64_t expected)
{
 auto d=w3::describe_atomic_instruction(opcode);byte_vec args{};if(opcode==3)return args;
 append_bits(args,address,wide?8:4);
 if(d.operand_count>=2)append_bits(args,d.kind==kind::compare_exchange?expected:input,d.value_i64?8:4);
 if(d.operand_count==3)append_bits(args,opcode<=2?0:input,opcode<=2||d.value_i64?8:4);
 return args;
}
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
void UWVM2TEST_WASM_ABI bounds_trap(uwvm2::object::memory::error::memory_error_t const&) noexcept{::_exit(42);}
void bounds_signal(uwvm2::object::memory::error::mmap_memory_error_t const&) noexcept{::_exit(42);}
#endif
template<class Run>int trap(Run&& run,unsigned index,std::uint64_t address)
{
 int output[2]{};UWVM2TEST_REQUIRE(::pipe(output)==0);auto child=::fork();UWVM2TEST_REQUIRE(child>=0);
 if(child==0)
 {
  ::close(output[0]);(void)::dup2(output[1],STDERR_FILENO);::close(output[1]);::alarm(15);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
  uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
#else
  optable::trap_memory_out_of_bounds_func=bounds_trap;
#if !defined(UWVM_FORCE_DISABLE_MMAP)
  uwvm2::object::memory::signal::set_mmap_memory_out_of_bounds_handler(bounds_signal);
#endif
#endif
  (void)run(index,arguments(0x12,true,address,0,0));::_exit(99);
 }
 ::close(output[1]);std::string diagnostic;char data[1024];
 for(;;){auto n=::read(output[0],data,sizeof(data));if(n<=0)break;diagnostic.append(data,n);}
 ::close(output[0]);int status{};UWVM2TEST_REQUIRE(::waitpid(child,&status,0)==child);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 if(!WIFSIGNALED(status)||diagnostic.find("memory access out of bounds")==std::string::npos||diagnostic.find("func_idx=")==std::string::npos)
 {std::fprintf(stderr,"memory64 atomic trap failed status=%d %s\n",status,diagnostic.c_str());return 1;}
#else
 UWVM2TEST_REQUIRE(WIFEXITED(status)&&WEXITSTATUS(status)==42);
#endif
 return 0;
}
template<class Run>int execute(prepared_runtime& prepared,Run&& run)
{
 unsigned checks{};
 for(bool wide:{false,true})for(unsigned index=0;index<67;++index)for(unsigned scenario=0;scenario<4;++scenario)
 {
  unsigned op=opcode_at(index);auto d=w3::describe_atomic_instruction(op);unsigned bytes=1u<<d.natural_alignment;
  auto memory=prepared.mod->local_defined_memory_vec_storage.index_unchecked(wide?1:0).memory.memory_begin;
  std::uint64_t patterns[]{0,~0ull,0x8000000080000000ull,0x123456789abcdef0ull};
  std::uint64_t pattern=patterns[scenario],mask=bytes==8?~0ull:(1ull<<(bytes*8))-1,old=pattern&mask;
  auto input=scenario==0?0ull:scenario==1?1ull:0xfedcba9876543210ull;
  auto compared=old^(scenario&1);auto next=old,result=old;
  std::memset(memory+511,0xa5,10);for(unsigned n=0;n<8;++n)memory[512+n]=std::byte(pattern>>(8*n));
  switch(d.kind)
  {
   case kind::fence:case kind::notify:result=0;break;
   case kind::wait32:case kind::wait64:result=(input&(d.value_i64?~0ull:0xffffffffull))==old?2:1;break;
   case kind::store:next=input&mask;result=0;break;
   case kind::add:next=(old+input)&mask;break;
   case kind::sub:next=(old-input)&mask;break;
   case kind::and_:next=old&input;break;
   case kind::or_:next=(old|input)&mask;break;
   case kind::xor_:next=(old^input)&mask;break;
   case kind::exchange:next=input&mask;break;
   case kind::compare_exchange:next=(compared&mask)==old?input&mask:old;break;
   default:break;
  }
  result+=15;if(!d.result_i64)result&=0xffffffffull;
  auto actual=run((wide?67:0)+index,arguments(op,wide,504,input,compared));
  if(actual!=result){std::fprintf(stderr,"atomic op=%x address%u scenario=%u actual=%llx expected=%llx\n",op,wide?64:32,scenario,(unsigned long long)actual,(unsigned long long)result);return 1;}
  for(unsigned n=0;n<8;++n)UWVM2TEST_REQUIRE(memory[512+n]==std::byte((n<bytes?next:pattern)>>(8*n)));
  UWVM2TEST_REQUIRE(memory[511]==std::byte{0xa5}&&memory[520]==std::byte{0xa5});++checks;
 }
 UWVM2TEST_REQUIRE(trap(run,67+6,0x100000000ull)==0); // high dynamic address must not wrap to memory32
 UWVM2TEST_REQUIRE(trap(run,134,1)==0); // u64 max static offset + 1 requires a u65 bounds failure
 std::printf("PASS integrated memory64 atomics: %u executions / 67 opcodes; mixed declarations, ring pressure, u65 traps\n",checks);return 0;
}
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
template<optable::uwvm_interpreter_translate_option_t Option>
#endif
int check()
{
 for(int invalid=-1;invalid<67;++invalid)
 {
  if(invalid==3)continue;auto wasm=build(invalid);auto features=make_wasm1p1_feature_parameter();
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64=false;
  auto& f=uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features);f.disable_threads=f.disable_multi_memory=false;
  auto prepared=prepare_runtime_from_wasm(wasm,u8"memory64-atomic",{},features);
  uwvm2::validation::error::code_validation_error_impl error{};
  try
  {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
   jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
   options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
   auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);UWVM2TEST_REQUIRE(invalid==-1);
   UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted&&!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));
   save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"atomic.ll");
   UWVM2TEST_REQUIRE(execute(prepared,[&](unsigned index,byte_vec const& args)
   {std::uint64_t result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,&result,8,args.data(),args.size());return result;})==0);
#else
   optable::compile_option options{};auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features);
   UWVM2TEST_REQUIRE(invalid==-1);
   UWVM2TEST_REQUIRE(execute(prepared,[&](unsigned index,byte_vec const& args)
   {uwvm2::runtime::wasm_threads::wait_domain domain;
    uwvm2::runtime::wasm_threads::execution_scope scope{domain,{}};
    auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),prepared.mod->local_defined_function_vec_storage.index_unchecked(index),args,nullptr,nullptr);
    if(result.results.size()!=8)std::abort();std::uint64_t bits{};std::memcpy(&bits,result.results.data(),8);return bits;})==0);
#endif
  }
  catch(fast_io::error const&)
  {if(invalid<0)std::fprintf(stderr,"unexpected atomic validation error=%u\n",unsigned(error.err_code));UWVM2TEST_REQUIRE(invalid>=0);}
  UWVM2TEST_REQUIRE((error.err_code==uwvm2::validation::error::code_validation_error_code::ok)==(invalid==-1));
 }
 return 0;
}
}
int main([[maybe_unused]] int argc,[[maybe_unused]] char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);return check();
#else
 install_unexpected_traps();UWVM2TEST_REQUIRE(check<optable::uwvm_interpreter_translate_option_t{.is_tail_call=false}>()==0);
 constexpr auto merged=make_tailcall_scalar4_merged_opt<2>();UWVM2TEST_REQUIRE(check<merged>()==0);
 constexpr auto split=make_tailcall_hardfloat_abi_opt<2,2>();UWVM2TEST_REQUIRE(check<split>()==0);
 constexpr auto tiny=make_tailcall_hardfloat_abi_opt<1,1>();return check<tiny>();
#endif
}
