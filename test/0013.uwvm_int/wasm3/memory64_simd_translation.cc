// Binary memory64 declarations flow through parser, initializer, validation,
// compilation and execution. No declaration metadata is mutated by this fixture.
#include "../strict/uwvm_int_translate_strict_common.h"
#include "../../0017.runtime/memory64_simd_reference.h"
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
#include "memory64_translation_ir.h"
namespace
{
using namespace uwvm2test::uwvm_int_strict;
namespace reference=memory64_simd_reference;
constexpr std::uint8_t memory64_v128_code=0x7b;
struct entry{unsigned opcode,lane;};
std::vector<entry> cases()
{
 std::vector<entry> out{};for(unsigned op=0;op<=93;++op)
 {if(op>11&&op<84)continue;auto s=reference::describe(op);for(unsigned lane=0;lane<(s.lane?16/s.width:1);++lane)out.push_back({op,lane});}
 return out;
}
byte_vec build(bool valid,std::vector<entry> const& cases)
{
 module_builder b{};b.has_memory=b.memory_has_max=true;b.memory_address64=true;b.memory_min=1;b.memory_max=2;
 for(auto c:cases)
 {
  auto s=reference::describe(c.opcode);func_body body{};func_type type{{valid?k_val_i64:k_val_i32},{memory64_v128_code}};
  // Keep three independent vectors live across the memory access to fill both
  // the one-slot and two-slot FP/vector rings before producing another vector.
  for(unsigned bias:{1u,29u,63u})
  {append_u8(body.code,0xfd);append_u8(body.code,12);for(unsigned n=0;n<16;++n)append_u8(body.code,n+bias);}
  append_u8(body.code,0x20);append_u8(body.code,0);
  if(s.consumes){type.params.push_back(memory64_v128_code);append_u8(body.code,0x20);append_u8(body.code,1);}
  append_u8(body.code,0xfd);append_u32_leb(body.code,c.opcode);append_u8(body.code,std::countr_zero(s.width));append_u8(body.code,4);
  if(s.lane)append_u8(body.code,c.lane);
  if(s.store){append_u8(body.code,0xfd);append_u8(body.code,12);for(unsigned n=0;n<16;++n)append_u8(body.code,0);}
  for(unsigned n=0;n<3;++n){append_u8(body.code,0xfd);append_u8(body.code,81);}
  append_u8(body.code,11);b.add_func(std::move(type),std::move(body));
 }
 // Ten-byte offset must survive SIMD replay, too. It is compiled but not executed.
 func_body wide{};for(unsigned x:{0x20u,0u,0xfdu,0u,4u})append_u8(wide.code,x);
 for(unsigned n=0;n<9;++n)append_u8(wide.code,255);append_u8(wide.code,1);append_u8(wide.code,11);
 b.add_func({{k_val_i64},{memory64_v128_code}},std::move(wide));return b.build();
}
template<class Run>int execute(prepared_runtime& prepared,std::vector<entry> const& cases,Run&& run)
{
 auto memory=prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory.memory_begin;
 unsigned checks{};
 for(unsigned i=0;i<cases.size();++i)for(unsigned seed:{0u,0x83u,0xfdu})
 {
  auto c=cases[i];auto s=reference::describe(c.opcode);std::array<std::byte,16> wire{},old{};
  for(unsigned n=0;n<16;++n){wire[n]=std::byte((seed+n*13)&255);old[n]=std::byte((seed+n*17+7)&255);}
  std::memset(memory+512,0xa5,18);if(!s.store)std::memcpy(memory+513,wire.data(),16);
  byte_vec args{};args.resize(s.consumes?24:8);std::uint64_t address=509;std::memcpy(args.data(),&address,8);
  if(s.consumes)std::memcpy(args.data()+8,old.data(),16);
  auto result=run(i,args);auto expected=reference::expected(c.opcode,c.lane,wire,old);
  if(s.store)
  {
   std::array<std::byte,16> mask{};for(unsigned n=0;n<16;++n)mask[n]=std::byte((n+1)^(n+29)^(n+63));
   if(result!=mask){std::fprintf(stderr,"SIMD preserved values mismatch opcode=%u lane=%u seed=%u\n",c.opcode,c.lane,seed);return 1;}
   UWVM2TEST_REQUIRE(std::memcmp(memory+513,expected.data(),s.width)==0);
   for(unsigned n=s.width;n<17;++n)UWVM2TEST_REQUIRE(memory[513+n]==std::byte{0xa5});
  }
  else
  {
   for(unsigned n=0;n<16;++n)expected[n]^=std::byte((n+1)^(n+29)^(n+63));
   if(result!=expected){std::fprintf(stderr,"SIMD result mismatch opcode=%u lane=%u seed=%u\n",c.opcode,c.lane,seed);return 1;}
  }
  UWVM2TEST_REQUIRE(memory[512]==std::byte{0xa5});++checks;
 }
 std::printf("PASS integrated SIMD: %u executions / %zu opcode-lane combinations / 22 opcodes\n",checks,cases.size());return 0;
}
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
template<optable::uwvm_interpreter_translate_option_t Option>
#endif
int check()
{
 auto all=cases();
 for(bool valid:{true,false})
 {
  auto wasm=build(valid,all);auto features=make_wasm1p1_feature_parameter();
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64=false;
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).cli_mode=
   uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;
  auto prepared=prepare_runtime_from_wasm(wasm,u8"memory64-simd",{},features);
  uwvm2::validation::error::code_validation_error_impl error{};
  try
  {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
   jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
   options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
   auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);UWVM2TEST_REQUIRE(valid);
   UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted&&!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));
   save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"simd.ll");
   UWVM2TEST_REQUIRE(execute(prepared,all,[&](unsigned index,byte_vec const& args)
   {std::array<std::byte,16> result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,result.data(),16,args.data(),args.size());return result;})==0);
#else
   optable::compile_option options{};auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features);
   UWVM2TEST_REQUIRE(valid);
   UWVM2TEST_REQUIRE(execute(prepared,all,[&](unsigned index,byte_vec const& args)
   {auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),prepared.mod->local_defined_function_vec_storage.index_unchecked(index),args,nullptr,nullptr);
    if(result.results.size()!=16)std::abort();std::array<std::byte,16> bits{};std::memcpy(bits.data(),result.results.data(),16);return bits;})==0);
#endif
  }
  catch(fast_io::error const&)
  {if(valid)std::fprintf(stderr,"unexpected SIMD validation error=%u\n",unsigned(error.err_code));UWVM2TEST_REQUIRE(!valid);}
  UWVM2TEST_REQUIRE((error.err_code==uwvm2::validation::error::code_validation_error_code::ok)==valid);
 }
 return 0;
}
}
int main([[maybe_unused]] int argc,[[maybe_unused]] char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);
 return check();
#else
 install_unexpected_traps();UWVM2TEST_REQUIRE(check<optable::uwvm_interpreter_translate_option_t{.is_tail_call=false}>()==0);
 constexpr auto scalar=make_tailcall_scalar4_merged_opt<2>();UWVM2TEST_REQUIRE(check<scalar>()==0);
 // Whole-translator ABI: vector values share the FP ring, disjoint from integer addresses.
 constexpr auto separate=make_tailcall_hardfloat_abi_opt<2,2,true>();UWVM2TEST_REQUIRE(check<separate>()==0);
 constexpr auto tiny=make_tailcall_hardfloat_abi_opt<1,1,true>();return check<tiny>();
#endif
}
