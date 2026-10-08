// Preserve VM-owned imported/defined identity while constructing ref.func directly in native SSA.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include "memory64_translation_ir.h"
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
using namespace uwvm2test::uwvm_int_strict;
void bytes(byte_vec& b,std::initializer_list<unsigned> a){for(auto v:a)append_u8(b,v);}
int main(int argc,char** argv){configure_memory64_jit_fixture_policy(argc,argv);
 module_builder provider;func_body p;bytes(p.code,{0x41,42,11});provider.add_func({{},{0x7f}},std::move(p));provider.add_export_func(0,"f");auto provider_bytes=provider.build();
 module_builder b;b.types.push_back({{},{0x7f}});b.add_import_func("provider","f",0);b.add_import_func("provider","f",0);
 func_body target;bytes(target.code,{0x41,7,11});b.add_func({{},{0x7f}},std::move(target));
 for(unsigned idx=0;idx<3;++idx){func_body get;bytes(get.code,{0xd2,idx,11});b.add_func({{},{0x70}},std::move(get));}
 b.passive_elements.push_back({{0,1,2}});auto binary=b.build();auto f=make_wasm1p1_feature_parameter();
 uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_function_references=false;
 auto prepared=prepare_runtime_from_wasm(binary,u8"ref-func-identity",{{&provider_bytes,u8"provider",&f}},f);
 namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
 uwvm2::validation::error::code_validation_error_impl error{};jit::compile_option options;options.validator_feature_parameter=&f;
 options.verify_llvm_jit_ir=true;options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
 auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);
 UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,"ref-func.ll");
 using ref=uwvm2::object::global::wasm_global_ref_t;using kind=uwvm2::object::global::wasm_ref_kind;unsigned count{};
 for(unsigned repeat=0;repeat<1024;++repeat)for(unsigned idx=0;idx<3;++idx){
  ref result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,idx+3,&result,sizeof(result),nullptr,0);
  auto expected=idx<2?static_cast<void const*>(&prepared.mod->imported_function_vec_storage.index_unchecked(idx)):static_cast<void const*>(&prepared.mod->local_defined_function_vec_storage.index_unchecked(0));
  UWVM2TEST_REQUIRE(result.storage.ptr==expected);UWVM2TEST_REQUIRE(result.kind==(idx<2?kind::wasm_func_imported:kind::wasm_func_defined));++count;
 }
 std::printf("PASS %u native ref.func identities, including distinct aliases of one imported target\n",count);
}
