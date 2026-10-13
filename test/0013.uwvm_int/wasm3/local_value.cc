// Actual explicit Core 3 local declarations: default nulls, set/tee, reference payloads, branch and select lifetimes.
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
using namespace uwvm2test::uwvm_int_strict;namespace v3=uwvm2::validation::standard::wasm3;unsigned case_id{};
using error_t=uwvm2::validation::error::code_validation_error_impl;using error_code=uwvm2::validation::error::code_validation_error_code;
void bytes(byte_vec& b,std::initializer_list<unsigned> a){for(auto v:a)append_u8(b,v);}
auto features(bool enabled=true){auto f=make_wasm1p1_feature_parameter();uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_function_references=!enabled;return f;}
auto const& code_section(){auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;
 return []<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,uwvm2::utils::container::tuple<Fs...>)->auto const&
 {return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);}
 (parsed.sections,uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);}
void validation_cases(){struct sample{bool valid;bool legacy;unsigned result;std::initializer_list<unsigned> body;};sample cases[]{
 {true,false,0x70,{2,0x63,0x70,0xd0,0x70,11}}, {true,false,0x6f,{3,0x63,0x6f,0xd0,0x6f,11}},
 {true,false,0x70,{0x41,1,4,0x63,0x70,0xd0,0x70,5,0xd0,0x70,11}},
 {true,false,0x70,{0xd0,0x70,0xd0,0x70,0x41,1,0x1c,1,0x63,0x70}},
 {true,false,0x6f,{0xd0,0x6f,0xd0,0x6f,0x41,0,0x1c,0x81,0,0x63,0x6f}},
 {true,false,0x70,{0,2,0x63,0x70,0,11}}, {true,false,0x70,{0,0x1c,1,0x63,0x70}},
 {true,true,0x70,{2,0x70,0xd0,0x70,11}}, {true,true,0x6f,{0xd0,0x6f,0xd0,0x6f,0x41,0,0x1c,1,0x6f}},
 {false,false,0x70,{2,0x63,0x70,0xd0,0x6f,11}}, {false,false,0x6f,{2,0x63,0x70,0xd0,0x70,11}},
 {false,false,0x70,{0xd0,0x70,0xd0,0x6f,0x41,1,0x1c,1,0x63,0x70}},
 {false,false,0x70,{0xd0,0x70,0xd0,0x70,0x42,1,0x1c,1,0x63,0x70}},
 {false,false,0x70,{0,0x1c,0}}, {false,false,0x70,{0,0x1c,2,0x63,0x70,0x63,0x70}},
 {false,false,0x70,{0,0x1c,1,0x63}}, {false,false,0x70,{0,0x1c,1,0x63,0xf0,0x7f}},
 {false,false,0x70,{2,0x63,0xf0,0x7f,0xd0,0x70,11}}, {false,false,0x70,{2,0x63}},
 {false,false,0x70,{2,0x63,0x80,0x80,0x80,0x80,0x10,0,11}},
 {false,false,0x70,{0,0x1c,1,0x64,0x70}}, // Non-null declarations deliberately remain unintegrated.
 {false,false,0x70,{0,0x1c,1,0x63,0}}, // A concrete heap must never be erased into a nullable abstract carrier.
 {false,false,0x70,{0,2,0x64,0x70,0,11}}, {false,false,0x70,{0,2,0x63,0,0,11}}
};
 for(auto const& c:cases)for(bool enabled:{false,true}){++case_id;module_builder b;func_body body;bytes(body.code,c.body);bytes(body.code,{11});b.add_func({{},{static_cast<std::uint8_t>(c.result)}},std::move(body));auto wasm=b.build();auto f=features(enabled);auto prepared=prepare_runtime_from_wasm(wasm,u8"value-immediate-validation",{},f);
  auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;auto const& code=code_section().codes.index_unchecked(0).body;bool valid=c.valid&&(enabled||c.legacy);error_t err{};
  try{v3::validate_code(v3::wasm3_code_version{},parsed,0,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),err,f);}catch(fast_io::error const&){}UWVM2TEST_REQUIRE((err.err_code==error_code::ok)==valid);
  error_t integrated{};try{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option o;o.validator_feature_parameter=&f;auto result=jit::compile_all_from_uwvm(*prepared.mod,o,integrated,0);
#else
   optable::compile_option o;auto result=compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(*prepared.mod,o,integrated,&f);
#endif
  }catch(fast_io::error const&){}UWVM2TEST_REQUIRE((integrated.err_code==error_code::ok)==valid);
 }
 std::printf("PASS %u explicit value-immediate pure/integrated validation cases\n",case_id);
}
byte_vec expand_locals(byte_vec const& input){
 byte_vec out;out.insert(out.end(),input.begin(),input.begin()+8);std::size_t position=8;unsigned expanded{};
 auto read=[&](){std::uint32_t result{};unsigned shift{};for(;;){UWVM2TEST_REQUIRE(position<input.size());auto byte=std::to_integer<unsigned>(input[position++]);result|=(byte&127)<<shift;if(byte<128)return result;shift+=7;UWVM2TEST_REQUIRE(shift<35);}};
 while(position<input.size()){
  auto id=input[position++];auto size=read();auto end=position+size;UWVM2TEST_REQUIRE(end<=input.size());byte_vec payload;
  if(id!=std::byte{10}){payload.insert(payload.end(),input.begin()+position,input.begin()+end);position=end;}
  else{
   auto count=read();append_u32_leb(payload,count);
   for(unsigned i=0;i<count;++i){auto body_size=read();auto body_end=position+body_size;auto groups=read();byte_vec body;append_u32_leb(body,groups);
    for(unsigned j=0;j<groups;++j){auto n=read();append_u32_leb(body,n);auto type=input[position++];if(type==std::byte{0x70}||type==std::byte{0x6f}){body.push_back(std::byte{0x63});++expanded;}body.push_back(type);}
    UWVM2TEST_REQUIRE(position<=body_end&&body_end<=end);body.insert(body.end(),input.begin()+position,input.begin()+body_end);position=body_end;append_u32_leb(payload,static_cast<std::uint32_t>(body.size()));payload.insert(payload.end(),body.begin(),body.end());
   }
   UWVM2TEST_REQUIRE(position==end);
  }
  out.push_back(id);append_u32_leb(out,static_cast<std::uint32_t>(payload.size()));out.insert(out.end(),payload.begin(),payload.end());
 }
 UWVM2TEST_REQUIRE(expanded==10);return out;
}
byte_vec make_module(){module_builder b;
 for(std::uint8_t rt:{0x70,0x6f}){
  func_body block;block.locals.push_back({1,rt});bytes(block.code,{0x20,0,0x21,1,2,0x63,rt,0x20,1,11,11});b.add_func({{rt},{rt}},std::move(block));
  func_body loop;loop.locals.push_back({1,rt});bytes(loop.code,{0x20,0,0x22,1,0x1a,3,0x63,rt,0x20,1,11,11});b.add_func({{rt},{rt}},std::move(loop));
  func_body conditional;conditional.locals.push_back({1,rt});conditional.locals.push_back({1,rt});bytes(conditional.code,{0x20,0,0x21,3,0x20,1,0x21,4,0x20,2,4,0x63,rt,0x20,3,5,0x20,4,11,11});b.add_func({{rt,rt,0x7f},{rt}},std::move(conditional));
  func_body select;select.locals.push_back({1,rt});bytes(select.code,{0x20,3,0xd1,0x45,4,0x40,0,11,0x20,0,0x21,3,0x20,2,0x41,17,0x6a,0x20,3,0x20,1,0x20,2,0x1c,1,0x63,rt,11});b.add_func({{rt,rt,0x7f},{0x7f,rt}},std::move(select));
 }
 return expand_locals(b.build());}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
void execute()
#else
template<optable::uwvm_interpreter_translate_option_t Option>void execute()
#endif
{
 auto wasm=make_module();auto f=features();auto prepared=prepare_runtime_from_wasm(wasm,u8"local-values",{},f);error_t err{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option o;o.validator_feature_parameter=&f;o.verify_llvm_jit_ir=true;o.emit_call_stack_frames=!memory64_fixture_unwind;o.emit_unwind_call_stack_frames=memory64_fixture_unwind;
 auto compiled=jit::compile_all_from_uwvm(*prepared.mod,o,err,0);UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"local-values.ll");
 auto run=[&](unsigned i,byte_vec const& in,std::size_t n){byte_vec out;out.resize(n);uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,i,out.data(),n,in.data(),in.size());return out;};
#else
 optable::compile_option o;auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,o,err,&f);UWVM2TEST_REQUIRE(err.err_code==error_code::ok);
 auto run=[&](unsigned i,byte_vec const& in,std::size_t n){auto r=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(i),prepared.mod->local_defined_function_vec_storage.index_unchecked(i),in,nullptr,nullptr);UWVM2TEST_REQUIRE(r.results.size()==n);return r.results;};
#endif
 using ref=uwvm2::object::global::wasm_global_ref_t;using kind=uwvm2::object::global::wasm_ref_kind;unsigned executed{};
 for(bool external:{false,true})for(unsigned pattern=0;pattern<256;++pattern){
  ref left,right;std::memset(&left,pattern,sizeof(left));std::memset(&right,255-pattern,sizeof(right));
  left.kind=pattern&1?kind::wasm_null:(external?kind::wasm_extern:kind::wasm_func_defined);
  right.kind=pattern&1?(external?kind::wasm_extern:kind::wasm_func_defined):kind::wasm_null;
  byte_vec input;input.resize(sizeof(ref));std::memcpy(input.data(),&left,sizeof(ref));auto fn=external?4u:0u;
  for(auto i:{0u,1u}){auto out=run(fn+i,input,sizeof(ref));UWVM2TEST_REQUIRE(std::memcmp(out.data(),&left,sizeof(ref))==0);++executed;}
  input.resize(2*sizeof(ref)+4);std::memcpy(input.data()+sizeof(ref),&right,sizeof(ref));
  for(std::uint32_t cond:{0u,1u,0x80000000u,0xffffffffu}){
   std::memcpy(input.data()+2*sizeof(ref),&cond,4);auto expected=cond?left:right;auto out=run(fn+2,input,sizeof(ref));UWVM2TEST_REQUIRE(std::memcmp(out.data(),&expected,sizeof(ref))==0);
   auto mixed=run(fn+3,input,4+sizeof(ref));std::uint32_t n;std::memcpy(&n,mixed.data(),4);UWVM2TEST_REQUIRE(n==cond+17);UWVM2TEST_REQUIRE(std::memcmp(mixed.data()+4,&expected,sizeof(ref))==0);executed+=2;
  }
 }
 std::printf("PASS %u explicit local default/set/tee, reference-payload and live register-ring executions\n",executed);
}
}
int main(int argc,char** argv){validation_cases();
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);execute();
#else
 (void)argc;(void)argv;install_unexpected_traps();execute<k_test_byref_opt>();execute<make_tailcall_scalar4_merged_opt<1>()>();execute<make_tailcall_scalar4_merged_opt<2>()>();
#endif
}
