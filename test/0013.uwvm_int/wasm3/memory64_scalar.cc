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
 namespace init=uwvm2::uwvm::runtime::initializer::details;
 namespace rs=uwvm2::uwvm::runtime::storage;
 struct spec{unsigned width,bits;bool store,sign;std::uint8_t type;};
 constexpr spec describe(unsigned op)
 {
  constexpr spec all[]{
   {4,32,false,false,k_val_i32},{8,64,false,false,k_val_i64},{4,32,false,false,k_val_f32},{8,64,false,false,k_val_f64},
   {1,32,false,true,k_val_i32},{1,32,false,false,k_val_i32},{2,32,false,true,k_val_i32},{2,32,false,false,k_val_i32},
   {1,64,false,true,k_val_i64},{1,64,false,false,k_val_i64},{2,64,false,true,k_val_i64},{2,64,false,false,k_val_i64},
   {4,64,false,true,k_val_i64},{4,64,false,false,k_val_i64},
   {4,32,true,false,k_val_i32},{8,64,true,false,k_val_i64},{4,32,true,false,k_val_f32},{8,64,true,false,k_val_f64},
   {1,32,true,false,k_val_i32},{2,32,true,false,k_val_i32},{1,64,true,false,k_val_i64},{2,64,true,false,k_val_i64},{4,64,true,false,k_val_i64}};
  return all[op-0x28];
 }
 byte_vec build(unsigned invalid)
 {
  module_builder builder{};builder.has_memory=builder.memory_has_max=true;builder.memory_address64=true;builder.memory_min=1;builder.memory_max=2;
  for(unsigned opcode=0x28;opcode<=0x3e;++opcode)
  {
   auto s=describe(opcode);func_body body{};func_type type{{invalid==1?k_val_i32:k_val_i64},{k_val_i64}};
   append_u8(body.code,0x20);append_u8(body.code,0);
   if(s.store){type.params.push_back(s.type);append_u8(body.code,0x20);append_u8(body.code,1);}
   append_u8(body.code,opcode);append_u8(body.code,std::countr_zero(s.width));append_u8(body.code,4);
   if(s.store){append_u8(body.code,0x42);append_u8(body.code,0);}
   else
   {
    if(s.type==k_val_f32)append_u8(body.code,0xbc);
    if(s.type==k_val_f64)append_u8(body.code,0xbd);
    if(s.bits==32)append_u8(body.code,0xad);
   }
   append_u8(body.code,0x0b);builder.add_func(std::move(type),std::move(body));
  }
  // A genuine ten-byte static offset must survive compiler replay and bytecode emission.
  func_body wide{};for(unsigned b:{0x20u,0u,0x29u,3u})append_u8(wide.code,b);
  for(unsigned n=0;n<9;++n)append_u8(wide.code,255);append_u8(wide.code,1);append_u8(wide.code,11);
  builder.add_func({{k_val_i64},{k_val_i64}},std::move(wide));
  func_body size{};for(unsigned b:{0x3fu,0u,11u})append_u8(size.code,b);
  builder.add_func({{},{invalid==3?k_val_i32:k_val_i64}},std::move(size));
  func_body grow{};for(unsigned b:{0x20u,0u,0x40u,0u,11u})append_u8(grow.code,b);
  builder.add_func({{invalid==2?k_val_i32:k_val_i64},{k_val_i64}},std::move(grow));
  for(unsigned op=0x28;op<=0x2b;++op)
  {
   auto spec=describe(op);func_body body{};
   for(unsigned n=1;n<=5;++n)
   {
    append_u8(body.code,0x41+op-0x28);
    if(op<0x2a)append_u8(body.code,n);
    else if(op==0x2a){float f=n;auto bits=std::bit_cast<std::uint32_t>(f);for(unsigned i=0;i<4;++i)append_u8(body.code,bits>>(8*i));}
    else {double f=n;auto bits=std::bit_cast<std::uint64_t>(f);for(unsigned i=0;i<8;++i)append_u8(body.code,bits>>(8*i));}
   }
   for(unsigned b:{0x20u,0u,op,unsigned(std::countr_zero(spec.width)),4u})append_u8(body.code,b);
   for(unsigned n=0;n<5;++n)append_u8(body.code,op==0x28?0x6a:op==0x29?0x7c:op==0x2a?0x92:0xa0);
   if(op==0x2a)append_u8(body.code,0xbc);if(op==0x2b)append_u8(body.code,0xbd);
   if(spec.bits==32)append_u8(body.code,0xad);
   append_u8(body.code,11);builder.add_func({{k_val_i64},{k_val_i64}},std::move(body));
  }
  return builder.build();
 }
 template<class Run>int check_values(prepared_runtime& prepared,Run&& run)
 {
  auto& memory=prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory;
  unsigned checks{};
  for(unsigned op=0x28;op<=0x3e;++op)for(std::uint64_t pattern:{0ull,~0ull,0x8000000080000000ull,0x7ff000017f800001ull,0xfff00001ff800001ull,0x0123456789abcdefull})
  {
   auto s=describe(op);std::memset(memory.memory_begin+512,0xa5,18);
   if(!s.store)for(unsigned n=0;n<8;++n)memory.memory_begin[513+n]=std::byte((pattern>>(8*n))&255);
   byte_vec arguments{};arguments.resize(8+(s.store?s.bits/8:0));std::uint64_t address=509;
   std::memcpy(arguments.data(),&address,8);if(s.store)std::memcpy(arguments.data()+8,&pattern,s.bits/8);
   auto result=run(op-0x28,arguments);std::uint64_t expected{};
   if(!s.store)
   {
    for(unsigned n=0;n<s.bits/8;++n)
    {auto b=n<s.width?(pattern>>(8*n))&255:s.sign&&((pattern>>(8*(s.width-1)))&128)?255ull:0ull;expected|=b<<(8*n);}
   }
   if(result!=expected){std::fprintf(stderr,"scalar op=%x actual=%llx expected=%llx\n",op,(unsigned long long)result,(unsigned long long)expected);return 1;}
   if(s.store)
   {for(unsigned n=0;n<s.width;++n)UWVM2TEST_REQUIRE(memory.memory_begin[513+n]==std::byte((pattern>>(8*n))&255));UWVM2TEST_REQUIRE(memory.memory_begin[513+s.width]==std::byte{0xa5});}
   UWVM2TEST_REQUIRE(memory.memory_begin[512]==std::byte{0xa5});++checks;
  }
  for(unsigned op=0x28;op<=0x2b;++op)
  {
   std::uint64_t input=40,expected=55;
   if(op==0x2a){input=std::bit_cast<std::uint32_t>(40.0f);expected=std::bit_cast<std::uint32_t>(55.0f);}
   if(op==0x2b){input=std::bit_cast<std::uint64_t>(40.0);expected=std::bit_cast<std::uint64_t>(55.0);}
   std::memcpy(memory.memory_begin+513,&input,8);byte_vec argument{};argument.resize(8);std::uint64_t address=509;
   std::memcpy(argument.data(),&address,8);UWVM2TEST_REQUIRE(run(26+op-0x28,argument)==expected);
  }
  byte_vec empty{};UWVM2TEST_REQUIRE(run(24,empty)==1);
  for(auto [delta,expected,size]:{std::array<std::uint64_t,3>{0,1,1},{1,1,2},{1,~0ull,2},{0x100000001ull,~0ull,2},{~0ull,~0ull,2},{0,2,2}})
  {
   byte_vec argument{};argument.resize(8);std::memcpy(argument.data(),&delta,8);
   UWVM2TEST_REQUIRE(run(25,argument)==expected);UWVM2TEST_REQUIRE(run(24,empty)==size);
  }
  std::printf("PASS integrated memory64 scalar: %u executions / 23 opcodes; full-width immediate, integer extension, floating bit patterns, i64 size/grow\n",checks);
  return 0;
 }
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 template<optable::uwvm_interpreter_translate_option_t Option>int check()
 {
  for(unsigned invalid:{0,1,2,3})
  {
   bool const valid=invalid==0;auto wasm=build(invalid);auto features=make_wasm1p1_feature_parameter();
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64=false;
   uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).cli_mode=
    uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;
   auto prepared=prepare_runtime_from_wasm(wasm,u8"memory64-scalar",{},features);
   uwvm2::validation::error::code_validation_error_impl error{};optable::compile_option options{};
   try
   {
    auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features);UWVM2TEST_REQUIRE(valid);
    UWVM2TEST_REQUIRE(check_values(prepared,[&](unsigned index,byte_vec const& arguments)
    {
     auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),
      prepared.mod->local_defined_function_vec_storage.index_unchecked(index),arguments,nullptr,nullptr);
     if(result.results.size()!=8)std::abort();std::uint64_t bits{};std::memcpy(&bits,result.results.data(),8);return bits;
    })==0);
   }
   catch(fast_io::error const&)
   {
    if(valid)std::fprintf(stderr,"unexpected validation error=%u byte=%td\n",unsigned(error.err_code),error.err_curr-reinterpret_cast<std::byte const*>(wasm.data()));
    UWVM2TEST_REQUIRE(!valid);
   }
   UWVM2TEST_REQUIRE((error.err_code==uwvm2::validation::error::code_validation_error_code::ok)==valid);
  }
  return 0;
 }
#endif
}
int main([[maybe_unused]] int argc,[[maybe_unused]] char** argv)
{
 using namespace uwvm2test::uwvm_int_strict;
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);
 namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
 for(unsigned invalid:{0,1,2,3})
 {
  bool const valid=invalid==0;auto wasm=build(invalid);auto features=make_wasm1p1_feature_parameter();
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64=false;
   uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).cli_mode=
    uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;
   auto prepared=prepare_runtime_from_wasm(wasm,u8"memory64-scalar",{},features);
  uwvm2::validation::error::code_validation_error_impl error{};jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
   options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
  try
  {
   auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);UWVM2TEST_REQUIRE(valid);
   UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));
   save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"scalar.ll");
   UWVM2TEST_REQUIRE(check_values(prepared,[&](unsigned index,byte_vec const& arguments)
   {std::uint64_t result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,&result,8,arguments.data(),arguments.size());return result;})==0);
  }
  catch(fast_io::error const&)
   {
    if(valid)std::fprintf(stderr,"unexpected validation error=%u byte=%td\n",unsigned(error.err_code),error.err_curr-reinterpret_cast<std::byte const*>(wasm.data()));
    UWVM2TEST_REQUIRE(!valid);
   }
  UWVM2TEST_REQUIRE((error.err_code==uwvm2::validation::error::code_validation_error_code::ok)==valid);
 }
#else
 install_unexpected_traps();
 UWVM2TEST_REQUIRE(check<optable::uwvm_interpreter_translate_option_t{.is_tail_call=false}>()==0);
 constexpr auto ring=make_tailcall_scalar4_merged_opt<2>();UWVM2TEST_REQUIRE(check<ring>()==0);
 constexpr auto split=make_tailcall_hardfloat_abi_opt<2,2>();UWVM2TEST_REQUIRE(check<split>()==0);
 constexpr auto tiny=make_tailcall_hardfloat_abi_opt<1,1>();UWVM2TEST_REQUIRE(check<tiny>()==0);
#endif
}
