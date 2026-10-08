// Compare generated threaded code and execute the same bodies under block and Core 3 try_table.
#include "../strict/uwvm_int_translate_strict_common.h"
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace {
using namespace uwvm2test::uwvm_int_strict;unsigned checks{};
void bytes(byte_vec& b,std::initializer_list<unsigned> v){for(auto x:v)append_u8(b,x);}
template<optable::uwvm_interpreter_translate_option_t Option>void check(){
 module_builder b;
 for(bool exception:{false,true}){
  func_body body;body.locals.push_back({1,0x7f});bytes(body.code,{exception?0x1fu:0x02u,0x7f});if(exception)bytes(body.code,{0});
  // Delayed locals and combine candidates cross the structural entry and exit, with a live result.
  bytes(body.code,{0x20,0,0x41,7,0x6a,0x22,1,0x20,1,0x6a,0x0b,0x41,3,0x6c,0x0b});b.add_func({{0x7f},{0x7f}},std::move(body));
 }
 auto wasm=b.build();auto f=make_wasm1p1_feature_parameter();uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_exceptions=false;
 auto prepared=prepare_runtime_from_wasm(wasm,u8"try-table-zero-handlers",{},f);uwvm2::validation::error::code_validation_error_impl err{};optable::compile_option options;
 auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,err,&f);UWVM2TEST_REQUIRE(err.err_code==uwvm2::validation::error::code_validation_error_code::ok);
 auto const& first=compiled.local_funcs.index_unchecked(0).op.operands;auto const& second=compiled.local_funcs.index_unchecked(1).op.operands;
 UWVM2TEST_REQUIRE(first.size()==second.size());UWVM2TEST_REQUIRE(first.empty()||std::memcmp(first.data(),second.data(),first.size())==0);
 for(std::uint32_t input:{0u,1u,7u,0x7fffffffu,0x80000000u,0xfffffff9u,0xffffffffu})for(unsigned fn=0;fn<2;++fn){byte_vec params;params.resize(4);std::memcpy(params.data(),&input,4);auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(fn),prepared.mod->local_defined_function_vec_storage.index_unchecked(fn),params,nullptr,nullptr);std::uint32_t actual{};UWVM2TEST_REQUIRE(result.results.size()==4);std::memcpy(&actual,result.results.data(),4);UWVM2TEST_REQUIRE(actual==(input+7u)*6u);++checks;}
 std::printf("PASS try_table/block threaded bytecode identical: %zu bytes, tail dispatch=%u\n",first.size(),unsigned(Option.is_tail_call));
}
}
int main(){install_unexpected_traps();check<k_test_byref_opt>();check<make_tailcall_scalar4_merged_opt<1>()>();check<make_tailcall_scalar4_merged_opt<2>()>();std::printf("PASS %u zero-handler try_table register-ring executions\n",checks);}
