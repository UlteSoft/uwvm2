// Actual Core 3 ref.null <typeidx> validation/execution; include multibyte and padded signed-33 encodings.
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
 {true,false,0x70,{0xd0,0}}, {true,false,0x70,{0xd0,11}}, {true,false,0x70,{0xd0,63}}, {true,false,0x70,{0xd0,0xc0,0}},
 {true,false,0x70,{0xd0,0x80,1}}, {true,false,0x70,{0xd0,0x80,0x80,0x80,0x80,0}}, {true,false,0x7f,{0x41,7,0xd0,0x80,1,0xd1,0x6a}},
 {true,false,0x70,{0,0xd0,0x80,1}}, {true,true,0x70,{0xd0,0x70}}, {true,true,0x6f,{0xd0,0x6f}},
 {false,false,0x6f,{0xd0,0}}, {false,false,0x7f,{0xd0,0}}, {false,false,0x70,{0xd0,0x82,1}},
 {false,false,0x70,{0xd0,0xff,0xff,0xff,0xff,0x0f}}, {false,false,0x70,{0,0xd0,0xff,0xff,0xff,0xff,0x0f}},
 {false,false,0x70,{0xd0,0xf0,0x7f}}, {false,false,0x70,{0xd0,0xef,0x7f}}, {false,false,0x70,{0xd0,0x7f}},
 {false,false,0x70,{0xd0,0x80,0x80,0x80,0x80,0x10}}, {false,false,0x70,{0xd0,0x80,0x80,0x80,0x80,0x80,0}}
};
 for(auto const& c:cases)for(bool enabled:{false,true}){++case_id;module_builder b;b.types.resize(129);func_body body;bytes(body.code,c.body);bytes(body.code,{11});b.add_func({{},{static_cast<std::uint8_t>(c.result)}},std::move(body));auto wasm=b.build();auto f=features(enabled);auto prepared=prepare_runtime_from_wasm(wasm,u8"ref-null-heap-validation",{},f);
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
 std::printf("PASS %u ref.null heap pure/integrated validation cases\n",case_id);
}
byte_vec make_module(){module_builder b;b.types.resize(129);
 for(auto heap:{std::initializer_list<unsigned>{0}, {11}, {0xc0,0}, {0x80,1}, {0x80,0x80,0x80,0x80,0}}){func_body body;bytes(body.code,{0xd0});bytes(body.code,heap);bytes(body.code,{11});b.add_func({{},{0x70}},std::move(body));}
 func_body mixed;bytes(mixed.code,{0x20,0,0xd0,0x80,1,0xd1,0x6a,11});b.add_func({{0x7f},{0x7f}},std::move(mixed));return b.build();}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
void execute()
#else
template<optable::uwvm_interpreter_translate_option_t Option>void execute()
#endif
{
 auto wasm=make_module();auto f=features();auto prepared=prepare_runtime_from_wasm(wasm,u8"ref-null-heap",{},f);error_t err{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option o;o.validator_feature_parameter=&f;o.verify_llvm_jit_ir=true;o.emit_call_stack_frames=!memory64_fixture_unwind;o.emit_unwind_call_stack_frames=memory64_fixture_unwind;
 auto compiled=jit::compile_all_from_uwvm(*prepared.mod,o,err,0);UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"ref-null-heap.ll");
 auto run=[&](unsigned i,byte_vec const& in,std::size_t n){byte_vec out;out.resize(n);uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,i,out.data(),n,in.data(),in.size());return out;};
#else
 optable::compile_option o;auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,o,err,&f);UWVM2TEST_REQUIRE(err.err_code==error_code::ok);
 auto run=[&](unsigned i,byte_vec const& in,std::size_t n){auto r=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(i),prepared.mod->local_defined_function_vec_storage.index_unchecked(i),in,nullptr,nullptr);UWVM2TEST_REQUIRE(r.results.size()==n);return r.results;};
#endif
 using ref=uwvm2::object::global::wasm_global_ref_t;
 for(unsigned i=0;i<5;++i){auto out=run(i,{},sizeof(ref));ref value;std::memcpy(&value,out.data(),sizeof(value));UWVM2TEST_REQUIRE(value.kind==uwvm2::object::global::wasm_ref_kind::wasm_null);}
 for(std::uint32_t i=0;i<256;++i){byte_vec in;in.resize(4);std::memcpy(in.data(),&i,4);auto out=run(5,in,4);std::uint32_t value;std::memcpy(&value,out.data(),4);UWVM2TEST_REQUIRE(value==i+1);}
 std::puts("PASS 261 ref.null heap executions and live register-ring checks");
}
}
int main(int argc,char** argv){validation_cases();
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);execute();
#else
 (void)argc;(void)argv;install_unexpected_traps();execute<k_test_byref_opt>();execute<make_tailcall_scalar4_merged_opt<1>()>();execute<make_tailcall_scalar4_merged_opt<2>()>();
#endif
}
