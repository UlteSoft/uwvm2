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
 {true,0x70,{0xd0,0x70,0xd4}}, {true,0x6f,{0xd0,0x6f,0xd4}},
 {true,0x70,{0,0xd4}}, {true,0x6f,{0,0xd4}}, {false,0x7f,{0,0xd4}},
 {false,0x7e,{0,0xd4}}, {false,0x7b,{0,0xd4}}, {false,0x70,{0xd4}},
 {false,0x70,{0x41,0,0xd4}}, {false,0x70,{0,0x41,0,0xd4}},
 {true,0x7f,{0,0xd4,0xd1}}, {false,0x7f,{0,0xd4,0x45}},
 {true,0x6f,{0,0xd4,0xd4}}, {true,0x6f,{0,0xd4,0x22,0}},
 {false,0x70,{0,0xd4,0x22,0}}, // local.tee fixes externref; it must not remain reference bottom.
 {true,0x6f,{0,0xd4,0x21,0,0x20,0}},
 {true,0x6f,{0,0xd4,0x41,0,0x1c,1,0x6f}},
 {false,0x7f,{0,0xd4,0x41,0,0x1c,1,0x7f}},
 {false,0x6f,{0,0xd4,0x41,0,0x1b}}, // untyped select does not accept reference values.
 {true,0x6f,{0x02,0x6f,0,0xd4,0x0c,0,0x0b}},
 {true,0x70,{0x02,0x70,0x02,0x6f,0,0xd4,0x41,0,0x0e,1,0,1,0x0b,0x1a,0,0x0b}},
 {false,0x7f,{0x02,0x7f,0x02,0x6f,0,0xd4,0x41,0,0x0e,1,0,1,0x0b,0x1a,0,0x0b}},
 {true,0x6f,{0x02,0x40,0,0xd4,0x21,0,0x0b,0x20,0}},
 {true,0x6f,{0,0xd4,0x0f}}, {false,0x7f,{0,0xd4,0x0f}}
 };
 for(auto const& c:cases)for(bool enabled:{true,false}){
  ++case_id;module_builder b;func_body body;body.locals.push_back({1,0x6f});bytes(body.code,c.code);bytes(body.code,{11});b.add_func({{},{c.result}},std::move(body));
  auto wasm=b.build();
  if(enabled)if(auto directory=std::getenv("UWVM_REF_NON_NULL_FIXTURES")){
   auto path=std::string(directory)+"/"+std::to_string(case_id)+(c.valid?"-valid.wasm":"-invalid.wasm");
   auto file=std::fopen(path.c_str(),"wb");UWVM2TEST_REQUIRE(file!=nullptr);UWVM2TEST_REQUIRE(std::fwrite(wasm.data(),1,wasm.size(),file)==wasm.size());std::fclose(file);
  }
  auto f=features(enabled);auto prepared=prepare_runtime_from_wasm(wasm,u8"ref-non-null-validation",{},f);
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
 std::printf("PASS %u ref.as_non_null pure/integrated validation cases\n",case_id);return 0;
}
byte_vec make_module(){module_builder b;
 for(auto rt:{0x70u,0x6fu}){
  func_body body;bytes(body.code,{0x20,0,0xd4,0xd4,11});b.add_func({{static_cast<std::uint8_t>(rt)},{static_cast<std::uint8_t>(rt)}},std::move(body));
  func_body test;bytes(test.code,{0x20,0,0xd4,0xd1,11});b.add_func({{static_cast<std::uint8_t>(rt)},{0x7f}},std::move(test));
  func_body mixed;bytes(mixed.code,{0x20,1,0x41,7,0x6a,0x20,0,0xd4,0x1a,0x41,9,0x6a,11});
  b.add_func({{static_cast<std::uint8_t>(rt),0x7f},{0x7f}},std::move(mixed));
 }
 return b.build();}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
int execution_cases()
#else
template<optable::uwvm_interpreter_translate_option_t Option>int execution_cases()
#endif
{
 auto wasm=make_module();auto f=features();auto prepared=prepare_runtime_from_wasm(wasm,u8"ref-non-null",{},f);error_t err{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option options;options.validator_feature_parameter=&f;
 options.verify_llvm_jit_ir=true;options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
 auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,err,0);UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);
 UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"ref-as-non-null.ll");
 auto run=[&](unsigned index,byte_vec const& input,std::size_t n){byte_vec output;output.resize(n);uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,output.data(),n,input.data(),input.size());return output;};
#else
 optable::compile_option options;auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,err,&f);
 auto run=[&](unsigned index,byte_vec const& input,std::size_t n){auto r=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),prepared.mod->local_defined_function_vec_storage.index_unchecked(index),input,nullptr,nullptr);UWVM2TEST_REQUIRE(r.results.size()==n);return r.results;};
 optable::trap_null_reference_func=[]() noexcept{std::_Exit(47);};
#endif
 using ref=uwvm2::object::global::wasm_global_ref_t;using kind=uwvm2::object::global::wasm_ref_kind;
 unsigned executed{};
 for(bool external:{false,true})for(unsigned pattern=0;pattern<256;++pattern){
  ref value;std::memset(&value,pattern,sizeof(value));value.kind=external?kind::wasm_extern:kind::wasm_func_defined;
  byte_vec input;input.resize(sizeof(value));std::memcpy(input.data(),&value,sizeof(value));auto fn=external?3u:0u;
  auto output=run(fn,input,sizeof(value));UWVM2TEST_REQUIRE(std::memcmp(input.data(),output.data(),sizeof(value))==0);
  auto null=run(fn+1,input,4);std::uint32_t n{};std::memcpy(&n,null.data(),4);UWVM2TEST_REQUIRE(n==0);
  input.resize(sizeof(value)+4);std::uint32_t x=pattern;std::memcpy(input.data()+sizeof(value),&x,4);auto mixed=run(fn+2,input,4);std::memcpy(&n,mixed.data(),4);UWVM2TEST_REQUIRE(n==pattern+16);executed+=3;
 }
 for(bool external:{false,true}){
  ref value;std::memset(&value,0xa5,sizeof(value));value.kind=kind::wasm_null;byte_vec input;input.resize(sizeof(value));std::memcpy(input.data(),&value,sizeof(value));
  int pipes[2];UWVM2TEST_REQUIRE(pipe(pipes)==0);auto pid=fork();UWVM2TEST_REQUIRE(pid>=0);
  if(pid==0){close(pipes[0]);UWVM2TEST_REQUIRE(dup2(pipes[1],2)==2);close(pipes[1]);uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());run(external?3:0,input,sizeof(value));std::_Exit(0);}
  close(pipes[1]);std::string diagnostic;char buffer[4096];ssize_t n;while((n=read(pipes[0],buffer,sizeof(buffer)))>0)diagnostic.append(buffer,n);close(pipes[0]);int status;UWVM2TEST_REQUIRE(waitpid(pid,&status,0)==pid);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
  UWVM2TEST_REQUIRE(WIFSIGNALED(status));UWVM2TEST_REQUIRE(diagnostic.find("null reference")!=std::string::npos);UWVM2TEST_REQUIRE(diagnostic.find("Call stack:")!=std::string::npos&&diagnostic.find("ref-non-null")!=std::string::npos);
#else
  UWVM2TEST_REQUIRE(WIFEXITED(status)&&WEXITSTATUS(status)==47);
#endif
  ++executed;
 }
 std::printf("PASS %u ref.as_non_null executions, in-place identity, register-ring values and null traps\n",executed);return 0;
}
}
int main(int argc,char** argv){validation_cases();
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);return execution_cases();
#else
 (void)argc;(void)argv;install_unexpected_traps();execution_cases<k_test_byref_opt>();execution_cases<make_tailcall_scalar4_merged_opt<2>()>();return execution_cases<make_tailcall_scalar4_merged_opt<1>()>();
#endif
}
