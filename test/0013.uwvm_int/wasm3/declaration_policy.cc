// Independent compiler policies for actual Core 3 signatures/local declarations,
// including exn/noexn without tags and zero-count encoded local runs.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
#if __has_include(<uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#define UWVM_TEST_DECLARATION_LAZY
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include "memory64_translation_ir.h"
#endif
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do {if(!(x)){::fast_io::io::perrln("FAIL line ",__LINE__," case ",case_id,": ",#x);::fast_io::fast_terminate();}}while(false)
namespace {
using namespace uwvm2test::uwvm_int_strict;namespace v3=uwvm2::validation::standard::wasm3;unsigned case_id{};
using error_t=uwvm2::validation::error::code_validation_error_impl;using error_code=uwvm2::validation::error::code_validation_error_code;
void bytes(byte_vec& b,std::initializer_list<unsigned> a){for(auto v:a)append_u8(b,v);}
auto features(bool enabled=true){auto f=make_wasm1p1_feature_parameter();uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_function_references=!enabled;uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_exceptions=false;return f;}
auto const& code_section(){auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;
 return []<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,uwvm2::utils::container::tuple<Fs...>)->auto const&
 {return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);}
 (parsed.sections,uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);}
byte_vec make_module(std::initializer_list<unsigned> signature,std::initializer_list<unsigned> code,unsigned declaration=0){
 byte_vec out;bytes(out,{0,0x61,0x73,0x6d,1,0,0,0});auto section=[&](unsigned id,byte_vec const& payload){append_u8(out,id);append_u32_leb(out,static_cast<std::uint32_t>(payload.size()));out.insert(out.end(),payload.begin(),payload.end());};
 byte_vec type;bytes(type,{1,0x60});bytes(type,signature);section(1,type);auto external=[&]{byte_vec p;bytes(p,{1});bool imported=declaration&4u;bool table=declaration&2u;unsigned heap=(declaration&1u)?0x6f:0x70;if(imported)bytes(p,{1,'m',1,'x',table?1u:3u});bytes(p,{0x63,heap,0});if(table)bytes(p,{1});else if(!imported)bytes(p,{0xd0,heap,11});return p;};
 if(declaration&0x400u){byte_vec imp;bytes(imp,{1,1,'m',1,'x',4,0,0});section(2,imp);}if((declaration&12u)==12u)section(2,external());byte_vec function;bytes(function,{1,0});section(3,function);if((declaration&8u)&&!(declaration&4u))section((declaration&2u)?4:6,external());if(declaration&0x300u){byte_vec tag;bytes(tag,(declaration&0x100u)?std::initializer_list<unsigned>{1,0,0}:std::initializer_list<unsigned>{0});section(13,tag);}if(declaration&0x30u){byte_vec elem;unsigned heap=(declaration&1u)?0x6f:0x70;bytes(elem,{1,(declaration&0x20u)?7u:5u,0x63,heap,(declaration&0x40u)?1u:0u});if(declaration&0x40u)bytes(elem,{0xd0,heap,11});section(9,elem);}
 byte_vec body;bytes(body,code);byte_vec codes;bytes(codes,{1});append_u32_leb(codes,static_cast<std::uint32_t>(body.size()));codes.insert(codes.end(),body.begin(),body.end());section(10,codes);return out;
}
byte_vec make_provider(unsigned declaration){
 if(declaration&0x400u){byte_vec b;bytes(b,{0,0x61,0x73,0x6d,1,0,0,0,1,4,1,0x60,0,0,13,3,1,0,0,7,5,1,1,'x',4,0});return b;}
 byte_vec out;bytes(out,{0,0x61,0x73,0x6d,1,0,0,0});bool table=declaration&2u;unsigned heap=(declaration&1u)?0x6f:0x70;
 byte_vec payload;bytes(payload,{1,heap,0});if(table)bytes(payload,{1});else bytes(payload,{0xd0,heap,11});
 append_u8(out,table?4:6);append_u32_leb(out,static_cast<std::uint32_t>(payload.size()));out.insert(out.end(),payload.begin(),payload.end());bytes(out,{7,5,1,1,'x',table?1u:3u,0});return out;
}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include "jit_validation_rebuild.h"
#endif
void check(){struct sample{unsigned blocked;std::initializer_list<unsigned> signature;std::initializer_list<unsigned> body;unsigned declaration{};};sample cases[]{
 // 34 == independent reference-types (2) and exceptions (32) gates.
 // Exception heaps do not require function references; no tag/body opcode
 // can accidentally make these declaration-only counterexamples pass.
 {34,{1,0x69,0},{0,11}}, {34,{0,1,0x69},{0,0,11}},
 {34,{0,0},{1,0,0x69,11}}, {34,{0,0},{1,2,0x69,11}},
 {34,{1,0x63,0x69,0},{0,11}}, {34,{1,0x63,0x74,0},{0,11}},
 {34,{0,0},{1,0,0x63,0x74,11}}, {34,{0,0},{1,0,0x64,0x69,11}},
 {3,{0,0},{1,0,0x63,0x70,11}}, {3,{0,0},{1,2,0x63,0x6f,11}},
 {2,{0,0},{1,0,0x70,11}}, {2,{0,0},{1,2,0x6f,11}},
 {4,{0,0},{1,0,0x7b,11}}, {0,{0,0},{1,2,0x7f,11}},
 {3,{1,0x63,0x70,0},{0,11}}, {3,{0,1,0x63,0x6f},{0,0xd0,0x6f,11}},
 {2,{1,0x70,0},{0,11}}, {4,{1,0x7b,0},{0,11}},
 {8,{0,2,0x7f,0x7f},{0,0x41,1,0x41,2,11}}, {0,{0,0},{0,11}},
 {3,{0,0},{0,11},8}, {3,{0,0},{0,11},9}, {3,{0,0},{0,11},10}, {3,{0,0},{0,11},11},
 {3,{0,0},{0,11},12}, {3,{0,0},{0,11},13}, {3,{0,0},{0,11},14}, {3,{0,0},{0,11},15},
 {3,{0,0},{0,11},16}, {3,{0,0},{0,11},17}, {3,{0,0},{0,11},32}, {3,{0,0},{0,11},33},
 {3,{0,0},{0,11},80}, {3,{0,0},{0,11},81}, {3,{0,0},{0,11},96}, {3,{0,0},{0,11},97}, {32,{0,0},{0,11},256}, {32,{0,0},{0,11},512}, {32,{0,0},{0,11},1028}
};
 for(auto const& c:cases)for(unsigned restriction=0;restriction<7;++restriction){++case_id;auto wasm=make_module(c.signature,c.body,c.declaration);auto permissive=features();auto provider=make_provider(c.declaration);auto prepared=(c.declaration&4u)?prepare_runtime_from_wasm(wasm,u8"declaration-policy",{{&provider,u8"m",&permissive}},permissive):prepare_runtime_from_wasm(wasm,u8"declaration-policy",{},permissive);auto restricted=permissive;auto& flags=uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(restricted);
  if(restriction==1)flags.disable_function_references=true;if(restriction==2)flags.disable_reference_types=true;if(restriction==3)flags.disable_simd=true;if(restriction==4)flags.disable_multi_value=true;if(restriction==5)flags.controllable_allow_multi_result_vector=true;if(restriction==6)flags.disable_exceptions=true;
  bool valid=restriction==0||!(c.blocked&(1u<<((restriction==5?4:restriction)-1)));
  auto require_exn_policy_diagnostic=[&](error_t const& got){
   if(c.blocked==34u&&!valid){
    UWVM2TEST_REQUIRE(got.err_code==error_code::wasm1p1_feature_required);
    using feature=uwvm2::parser::wasm::base::wasm1p1_feature_kind;
    using subject=uwvm2::parser::wasm::base::wasm1p1_error_subject;
    auto const& detail=got.err_selectable.wasm1p1_feature_required;
    UWVM2TEST_REQUIRE(detail.feature==(restriction==2?feature::reference_types:feature::exceptions));
    UWVM2TEST_REQUIRE(detail.value==0x69u);
    UWVM2TEST_REQUIRE(detail.subject==(c.signature.size()==2?subject::local_type:subject::function_type));
   }
  };
  auto const& parsed=uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage;auto const& code=code_section().codes.index_unchecked(0).body;error_t err{};
  try{v3::validate_code(v3::wasm3_code_version{},parsed,0,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),err,restricted);}catch(fast_io::error const&){}
  UWVM2TEST_REQUIRE((err.err_code==error_code::ok)==valid);require_exn_policy_diagnostic(err);if(!valid)UWVM2TEST_REQUIRE(err.err_code==error_code::wasm1p1_feature_required);
  if(restriction==0){error_t default_policy{};try{v3::validate_code(v3::wasm3_code_version{},parsed,0,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),default_policy);}catch(fast_io::error const&){}UWVM2TEST_REQUIRE(default_policy.err_code==error_code::ok);}
  error_t rebuilt{};try{uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::validate_runtime_module_with_standard_validator(*prepared.mod,rebuilt,&restricted);}catch(fast_io::error const&){}
  UWVM2TEST_REQUIRE((rebuilt.err_code==error_code::ok)==valid);require_exn_policy_diagnostic(rebuilt);if(!valid)UWVM2TEST_REQUIRE(rebuilt.err_code==error_code::wasm1p1_feature_required);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
  check_jit_declaration_rebuild(*prepared.mod,restricted,valid);
#endif
  error_t facade{};try{v3::validate_code_with_runtime_policy(parsed,0,reinterpret_cast<std::byte const*>(code.expr_begin),reinterpret_cast<std::byte const*>(code.code_end),facade,restricted);}catch(fast_io::error const&){}
  UWVM2TEST_REQUIRE((facade.err_code==error_code::ok)==valid);require_exn_policy_diagnostic(facade);if(!valid)UWVM2TEST_REQUIRE(facade.err_code==error_code::wasm1p1_feature_required);
#if defined(UWVM_TEST_DECLARATION_LAZY)
  namespace lazy=uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator;
  for(bool function_only:{false,true}){lazy::lazy_module_storage_t storage;storage.functions.resize(1);lazy::lazy_split_config config{};if(function_only)config.eu_policy=lazy::lazy_execution_unit_split_policy_t::function_only;error_t split{};
   try{lazy::details::build_lazy_function_execution_units(*prepared.mod,storage,0,config,restricted,split);}catch(fast_io::error const&){}
   UWVM2TEST_REQUIRE((split.err_code==error_code::ok)==valid);require_exn_policy_diagnostic(split);if(!valid){UWVM2TEST_REQUIRE(split.err_code==error_code::wasm1p1_feature_required);UWVM2TEST_REQUIRE(storage.execution_units.empty());}
  }
#endif
  error_t integrated{};try{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;jit::compile_option o;o.validator_feature_parameter=&restricted;auto result=jit::compile_all_from_uwvm(*prepared.mod,o,integrated,0);
#else
   optable::compile_option o;auto result=compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(*prepared.mod,o,integrated,&restricted);
#endif
  }catch(fast_io::error const&){}
  UWVM2TEST_REQUIRE((integrated.err_code==error_code::ok)==valid);require_exn_policy_diagnostic(integrated);if(!valid)UWVM2TEST_REQUIRE(integrated.err_code==error_code::wasm1p1_feature_required);
 }
 ::fast_io::io::println("PASS ",case_id," independent declaration-policy cases through pure/facade/integrated validation (and ordinary lazy barriers)");
}
}
int main(){
 using local=uwvm2::parser::wasm::standard::wasm1::features::final_local_entry_t<uwvm2::parser::wasm::standard::wasm1::features::wasm1,uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1>;
 struct original_local_layout{decltype(local{}.count) count;decltype(local{}.type) type;};
 static_assert(sizeof(local)==sizeof(original_local_layout));
 ::fast_io::io::println("local declaration record: ",sizeof(local)," bytes (unchanged)");check();
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 check_jit_tag_rebuild();
#endif
}
