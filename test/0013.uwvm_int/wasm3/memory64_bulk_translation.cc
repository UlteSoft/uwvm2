// Binary memory64 declarations flow through parser, initializer, validation,
// compilation and execution. No declaration metadata is mutated by this fixture.
#include "../strict/uwvm_int_translate_strict_common.h"
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
#include "memory64_translation_ir.h"
namespace
{
using namespace uwvm2test::uwvm_int_strict;
byte_vec build(bool destination64,bool source64,unsigned invalid)
{
 module_builder b{};b.has_memory=b.memory_has_max=true;b.memory_address64=destination64;b.memory_min=1;b.memory_max=2;
 b.extra_memories.push_back({1,2,true,false,source64});
 passive_data_segment data{};for(unsigned n=0;n<32;++n)append_u8(data.bytes,n*7);b.passive_datas.push_back(std::move(data));
 auto const d=destination64?k_val_i64:k_val_i32,s=source64?k_val_i64:k_val_i32;
 std::array<std::array<std::uint8_t,3>,4> types{{{d,s,destination64&&source64?k_val_i64:k_val_i32},
   {d,k_val_i32,d},{d,k_val_i32,k_val_i32},{d,d,d}}};
 for(unsigned op=0;op<4;++op)
 {
  func_type type{{},{k_val_i64}};func_body body{};
  for(unsigned n=0;n<3;++n)
  {
   auto t=types[op][n];if(invalid==op*3+n+1)t=t==k_val_i64?k_val_i32:k_val_i64;
   type.params.push_back(t);append_u8(body.code,0x20);append_u8(body.code,n);
  }
  append_u8(body.code,0xfc);append_u8(body.code,op==1?11:op==2?8:10);append_u8(body.code,0);
  if(op!=1)append_u8(body.code,op==0?1:0);
  for(unsigned byte:{0x42u,0u,11u})append_u8(body.code,byte);
  b.add_func(std::move(type),std::move(body));
 }
 func_body drop{};for(unsigned byte:{0xfcu,9u,0u,0x42u,0u,11u})append_u8(drop.code,byte);
 b.add_func({{},{k_val_i64}},std::move(drop));return b.build();
}
byte_vec arguments(std::array<std::uint64_t,3> values,std::array<bool,3> wide)
{
 byte_vec result{};
 for(unsigned i=0;i<3;++i)
 {auto width=wide[i]?8u:4u;auto at=result.size();result.resize(at+width);std::memcpy(result.data()+at,&values[i],width);}
 return result;
}
template<class Run>int execute(prepared_runtime& prepared,bool d64,bool s64,Run&& run)
{
 auto dst=prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory.memory_begin;
 auto src=prepared.mod->local_defined_memory_vec_storage.index_unchecked(1).memory.memory_begin;
 std::array<std::byte,16384> expected{};
 for(unsigned n=0;n<16384;++n){src[n]=std::byte((n*47+3)&255);dst[n]=expected[n]=std::byte((n*61+5)&255);}
 unsigned checks{};
 for(auto [d,s,len]:{std::array<std::uint64_t,3>{17,39,8191},{0,65536,0},{65536,65536,0},{1023,3,127}})
 {
  UWVM2TEST_REQUIRE(run(0,arguments({d,s,len},{d64,s64,d64&&s64}))==0);
  if(len)std::memmove(expected.data()+d,src+s,len);
  UWVM2TEST_REQUIRE(std::memcmp(expected.data(),dst,expected.size())==0);++checks;
 }
 for(auto [d,s,len]:{std::array<std::uint64_t,3>{12,4,8191},{4,12,8191},{7,7,300},{65536,65536,0}})
 {
  UWVM2TEST_REQUIRE(run(3,arguments({d,s,len},{d64,d64,d64}))==0);
  if(len)std::memmove(expected.data()+d,expected.data()+s,len);
  UWVM2TEST_REQUIRE(std::memcmp(expected.data(),dst,expected.size())==0);++checks;
 }
 for(auto [d,value,len]:{std::array<std::uint64_t,3>{23,0xabcd,4097},{0,0xffffffff,8192},{65536,0x7a,0}})
 {
  UWVM2TEST_REQUIRE(run(1,arguments({d,value,len},{d64,false,d64}))==0);
  if(len)std::memset(expected.data()+d,value&255,len);
  UWVM2TEST_REQUIRE(std::memcmp(expected.data(),dst,expected.size())==0);++checks;
 }
 for(auto [d,s,len]:{std::array<std::uint64_t,3>{97,3,23},{0,32,0},{65536,32,0}})
 {
  UWVM2TEST_REQUIRE(run(2,arguments({d,s,len},{d64,false,false}))==0);
  for(unsigned n=0;n<len;++n)expected[d+n]=std::byte((s+n)*7);
  UWVM2TEST_REQUIRE(std::memcmp(expected.data(),dst,expected.size())==0);++checks;
 }
 byte_vec empty{};UWVM2TEST_REQUIRE(run(4,empty)==0);
 UWVM2TEST_REQUIRE(run(2,arguments({65536,0,0},{d64,false,false}))==0);
 std::printf("PASS integrated bulk d%u/s%u: %u copy/fill/init checks, drop and empty endpoints\n",d64?64:32,s64?64:32,checks);
 return 0;
}
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
template<optable::uwvm_interpreter_translate_option_t Option>
#endif
int check()
{
 for(bool d64:{false,true})for(bool s64:{false,true})for(unsigned invalid=0;invalid<=12;++invalid)
 {
  auto wasm=build(d64,s64,invalid);auto features=make_wasm1p1_feature_parameter();
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64=false;
  auto& feature=uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features);feature.disable_multi_memory=false;
  auto prepared=prepare_runtime_from_wasm(wasm,u8"memory64-bulk",{},features);
  uwvm2::validation::error::code_validation_error_impl error{};
  try
  {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
   jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
   options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
   auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);UWVM2TEST_REQUIRE(invalid==0);
   UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted&&!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));
   UWVM2TEST_REQUIRE(execute(prepared,d64,s64,[&](unsigned index,byte_vec const& args)
   {std::uint64_t result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,&result,8,args.data(),args.size());return result;})==0);
#else
   optable::compile_option options{};auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features);
   UWVM2TEST_REQUIRE(invalid==0);
   UWVM2TEST_REQUIRE(execute(prepared,d64,s64,[&](unsigned index,byte_vec const& args)
   {auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),prepared.mod->local_defined_function_vec_storage.index_unchecked(index),args,nullptr,nullptr);
    if(result.results.size()!=8)std::abort();std::uint64_t bits{};std::memcpy(&bits,result.results.data(),8);return bits;})==0);
#endif
  }
  catch(fast_io::error const&)
  {if(!invalid)std::fprintf(stderr,"unexpected bulk validation error=%u\n",unsigned(error.err_code));UWVM2TEST_REQUIRE(invalid!=0);}
  UWVM2TEST_REQUIRE((error.err_code==uwvm2::validation::error::code_validation_error_code::ok)==(invalid==0));
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
 constexpr auto ring=make_tailcall_scalar4_merged_opt<2>();return check<ring>();
#endif
}
