// Execute Core 3 nonempty catch tables through real interpreter translation with ring/delay/combine enabled.
#include "../strict/uwvm_int_translate_strict_common.h"
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace {
using namespace uwvm2test::uwvm_int_strict; unsigned checks{};
void bytes(byte_vec& b,std::initializer_list<unsigned> v){for(auto x:v)append_u8(b,x);}
byte_vec add_tags(byte_vec original){
 // All three tags share the (i32)->() type but have distinct identities.
 byte_vec output; output.insert(output.end(),original.begin(),original.begin()+8);std::size_t cursor=8;bool inserted=false;
 while(cursor<original.size()) {auto begin=cursor;auto id=std::to_integer<unsigned>(original[cursor++]);unsigned size=0,shift=0,x;do{x=std::to_integer<unsigned>(original[cursor++]);size|=(x&127)<<shift;shift+=7;}while(x&128);
  if(!inserted&&id>=6){bytes(output,{13,7,3,0,0,0,0,0,0});inserted=true;}cursor+=size;output.insert(output.end(),original.begin()+begin,original.begin()+cursor);
 }UWVM2TEST_REQUIRE(inserted);return output;
}
template<optable::uwvm_interpreter_translate_option_t Option>void check(){
 module_builder b;b.types.push_back({{0x7f},{}});
 // Nested wrong-tag handler must be skipped; throw matches OUTER tag 0 and preserves payload.
 {func_body body;body.locals.push_back({1,0x7f});bytes(body.code,{0x02,0x7f,0x1f,0x40,1,0,0,0,0x02,0x7f,0x1f,0x40,1,0,1,0,0x20,0,0x41,7,0x6a,0x22,1,0x20,1,0x6a,0x08,0,0x0b,0x00,0x0b,0x1a,0x0b,0x00,0x0b,0x41,3,0x6c,0x0b});b.add_func({{0x7f},{0x7f}},std::move(body));}
 // catch_all discards the payload and all temporaries, retaining the outer operand prefix.
 {func_body body;bytes(body.code,{0x20,0,0x02,0x40,0x1f,0x40,1,2,0,0x42,9,0x20,0,0x08,2,0x0b,0x00,0x0b,0x41,7,0x6a,0x41,6,0x6c,0x0b});b.add_func({{0x7f},{0x7f}},std::move(body));}
 auto wasm=add_tags(b.build());auto f=make_wasm1p1_feature_parameter();uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_exceptions=false;
 auto prepared=prepare_runtime_from_wasm(wasm,u8"local-throw-catch",{},f);uwvm2::validation::error::code_validation_error_impl err{};optable::compile_option options;
 auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,err,&f);UWVM2TEST_REQUIRE(err.err_code==uwvm2::validation::error::code_validation_error_code::ok);
 for(std::uint32_t input:{0u,1u,7u,0x7fffffffu,0x80000000u,0xfffffff9u,0xffffffffu})for(unsigned fn=0;fn<2;++fn){byte_vec params;params.resize(4);std::memcpy(params.data(),&input,4);auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(fn),prepared.mod->local_defined_function_vec_storage.index_unchecked(fn),params,nullptr,nullptr);std::uint32_t actual{};UWVM2TEST_REQUIRE(result.results.size()==4);std::memcpy(&actual,result.results.data(),4);UWVM2TEST_REQUIRE(actual==(input+7u)*6u);++checks;}
 std::printf("PASS local throw/catch ring configuration, tail dispatch=%u\n",unsigned(Option.is_tail_call));
}
}
int main(){install_unexpected_traps();check<k_test_byref_opt>();check<make_tailcall_scalar4_merged_opt<1>()>();check<make_tailcall_scalar4_merged_opt<2>()>();std::printf("PASS %u local throw/catch register-ring executions\n",checks);}
