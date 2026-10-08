// Actual Core 3 ref.as_non_null parser/validator/int/JIT coverage, including reference-only bottom.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include "memory64_translation_ir.h"
#endif
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %u case %u: %s\n",__LINE__,case_id,#x);std::abort();}}while(false)
namespace {
using namespace uwvm2test::uwvm_int_strict;
namespace v3=uwvm2::validation::standard::wasm3;
unsigned case_id{};
using error_t=uwvm2::validation::error::code_validation_error_impl;
using error_code=uwvm2::validation::error::code_validation_error_code;
void bytes(byte_vec& out,std::initializer_list<unsigned> values){for(auto v:values)append_u8(out,v);}
auto features(bool enabled=true){auto f=make_wasm1p1_feature_parameter();uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_function_references=!enabled;return f;}
auto const& code_section(){auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;
 return []<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,uwvm2::utils::container::tuple<Fs...>)->auto const&
 {return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);}
 (parsed.sections,uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);}
int validation_cases(){
 struct sample {bool valid;std::uint8_t result;std::initializer_list<unsigned> code;};
 sample cases[]{
 {true,0x7f,{0x41,11,0xd0,0x70,0xd5,0,0x1a}},
 {true,0x7f,{0x41,11,0xd0,0x6f,0xd5,0,0x1a}},
 {true,0x70,{0xd0,0x70,0xd6,0,0xd0,0x70}},
 {true,0x6f,{0xd0,0x6f,0xd6,0,0xd0,0x6f}},
 {true,0x70,{0,0xd5,0,0x1a}}, {true,0x6f,{0,0xd5,0,0x1a}},
 {true,0x70,{0,0xd6,0}}, {true,0x6f,{0,0xd6,0}},
 {false,0x7f,{0,0xd6,0}}, {false,0x70,{0,0x41,0,0xd5,0}},
 {false,0x70,{0,0x41,0,0xd6,0}}, {false,0x70,{0xd0,0x6f,0xd6,0,0xd0,0x70}},
 {false,0x7f,{0xd0,0x70,0xd5,0,0x1a}},
 {false,0x7f,{0x42,0,0xd0,0x70,0xd5,0,0x1a}},
 {false,0x70,{0xd0,0x70,0xd6,1,0xd0,0x70}},
 {false,0x70,{0xd0,0x70,0xd5,1,0x1a,0xd0,0x70}},
 {false,0x7f,{0,0xd5,0,0x6a}}, // refined reference cannot become an i32 operand.
 {false,0x7f,{0x02,0x40,0,0xd6,0,0x0b,0x41,0}}, // empty label remains invalid in unreachable code.
 {true,0x7f,{0x02,0x40,0xd0,0x70,0xd5,0,0x1a,0x0b,0x41,7}},
 {false,0x70,{0xd0,0x70,0xd6,0xff,0xff,0xff,0xff,0x10}},
 {false,0x70,{0xd0,0x70,0xd5,0xff,0xff,0xff,0xff,0x10}}

 };
 for(auto const& c:cases)for(bool enabled:{true,false}){
  ++case_id;module_builder b;func_body body;body.locals.push_back({1,0x6f});bytes(body.code,c.code);bytes(body.code,{11});b.add_func({{},{c.result}},std::move(body));
  auto wasm=b.build();
  if(enabled)if(auto directory=std::getenv("UWVM_REF_BRANCH_FIXTURES")){
   auto path=std::string(directory)+"/"+std::to_string(case_id)+(c.valid?"-valid.wasm":"-invalid.wasm");
   auto file=std::fopen(path.c_str(),"wb");UWVM2TEST_REQUIRE(file!=nullptr);UWVM2TEST_REQUIRE(std::fwrite(wasm.data(),1,wasm.size(),file)==wasm.size());std::fclose(file);
  }
  auto f=features(enabled);auto prepared=prepare_runtime_from_wasm(wasm,u8"ref-branches-validation",{},f);
  auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;auto const& code=code_section().codes.index_unchecked(0).body;
  error_t err{};try{v3::validate_code(v3::wasm3_code_version{},parsed,0,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),err,f);}catch(fast_io::error const&){}
  UWVM2TEST_REQUIRE((err.err_code==error_code::ok)==(c.valid&&enabled));
  if(!enabled){UWVM2TEST_REQUIRE(err.err_code==error_code::wasm1p1_feature_required);UWVM2TEST_REQUIRE(err.err_selectable.wasm1p1_feature_required.feature==uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references);}
  error_t integrated{};
  try{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option options;options.validator_feature_parameter=&f;
   auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,integrated,0);
#else
   optable::compile_option options;auto compiled=compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(*prepared.mod,options,integrated,&f);
#endif
  }catch(fast_io::error const&){}
  UWVM2TEST_REQUIRE((integrated.err_code==error_code::ok)==(c.valid&&enabled));
 }
 std::printf("PASS %u reference branch pure/integrated validation cases\n",case_id);return 0;
}
byte_vec make_module(){module_builder b;
 for(auto rt:{0x70u,0x6fu}){
  for(bool repair:{false,true}){
   func_body null;bytes(null.code,{0x02,0x7f});if(repair)bytes(null.code,{0x42,7});
   bytes(null.code,{0x41,11,0x20,0,0xd5,0,0x1a,0x1a});if(repair)bytes(null.code,{0x1a});
   bytes(null.code,{0x41,22,11,11});b.add_func({{static_cast<std::uint8_t>(rt)},{0x7f}},std::move(null));
   func_body non;bytes(non.code,{0x02,rt});if(repair)bytes(non.code,{0x42,7});
   bytes(non.code,{0x20,0,0xd6,0});if(repair)bytes(non.code,{0x1a});
   bytes(non.code,{0xd0,rt,11,11});b.add_func({{static_cast<std::uint8_t>(rt)},{static_cast<std::uint8_t>(rt)}},std::move(non));
  }
 }
 return b.build();}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
int execution_cases()
#else
template<optable::uwvm_interpreter_translate_option_t Option>int execution_cases()
#endif
{
 auto wasm=make_module();auto f=features();auto prepared=prepare_runtime_from_wasm(wasm,u8"ref-branches",{},f);error_t err{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option options;options.validator_feature_parameter=&f;
 options.verify_llvm_jit_ir=true;options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
 auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,err,0);UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);
 UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"ref-branches.ll");
 auto run=[&](unsigned index,byte_vec const& input,std::size_t n){byte_vec output;output.resize(n);uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,output.data(),n,input.data(),input.size());return output;};
#else
 optable::compile_option options;auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,err,&f);
 auto run=[&](unsigned index,byte_vec const& input,std::size_t n){auto r=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),prepared.mod->local_defined_function_vec_storage.index_unchecked(index),input,nullptr,nullptr);UWVM2TEST_REQUIRE(r.results.size()==n);return r.results;};
 optable::trap_null_reference_func=[]() noexcept{std::_Exit(47);};
#endif
 using ref=uwvm2::object::global::wasm_global_ref_t;using kind=uwvm2::object::global::wasm_ref_kind;
 unsigned executed{};
 for(bool external:{false,true})for(bool null:{false,true})for(unsigned pattern=0;pattern<256;++pattern){
  ref value;std::memset(&value,pattern,sizeof(value));value.kind=null?kind::wasm_null:(external?kind::wasm_extern:kind::wasm_func_defined);
  byte_vec input;input.resize(sizeof(value));std::memcpy(input.data(),&value,sizeof(value));
  for(bool repair:{false,true}){
   auto fn=(external?4u:0u)+(repair?2u:0u);auto output=run(fn,input,4);std::uint32_t n{};std::memcpy(&n,output.data(),4);UWVM2TEST_REQUIRE(n==(null?11u:22u));
   auto identity=run(fn+1,input,sizeof(value));ref result;std::memcpy(&result,identity.data(),sizeof(result));
   if(null)UWVM2TEST_REQUIRE(result.kind==kind::wasm_null);
   else UWVM2TEST_REQUIRE(std::memcmp(input.data(),identity.data(),sizeof(value))==0);
   executed+=2;
  }
 }
 std::printf("PASS %u reference branch executions, both edges, payload identity and taken-edge stack repair\n",executed);return 0;
}
}
int main(int argc,char** argv){validation_cases();
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);return execution_cases();
#else
 (void)argc;(void)argv;install_unexpected_traps();execution_cases<k_test_byref_opt>();
#ifndef UWVM2TEST_UNCACHED_ONLY
 execution_cases<make_tailcall_scalar4_merged_opt<2>()>();return execution_cases<make_tailcall_scalar4_merged_opt<1>()>();
#else
 return 0;
#endif
#endif
}
