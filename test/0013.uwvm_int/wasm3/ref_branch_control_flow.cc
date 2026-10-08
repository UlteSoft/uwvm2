// Run authored Core 3 reference-branch WAT fixtures after wasm-tools assembly.
// The fixtures' final defined function is the no-argument/no-result _start driver.
#include "../strict/uwvm_int_translate_strict_common.h"
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace {
using namespace uwvm2test::uwvm_int_strict;
// This standalone optable runner deliberately does not link the runtime library.
// Install its synchronous call adapter; real runtime calls are covered by the CLI matrix.
template<optable::uwvm_interpreter_translate_option_t Option>struct calls {
 inline static runtime_module_t const* module{};
 inline static compiled_module_t const* compiled{};
 static std::byte* UWVM2TEST_WASM_ABI invoke(std::size_t module_id, std::size_t index, std::byte* stack) {
  UWVM2TEST_REQUIRE(module && compiled);
  if(module_id==SIZE_MAX){
   // Defined calls encode an owned call-info address, not a Wasm function index.
   // Resolve it against this compilation's vector without dereferencing an unchecked address.
   auto address=index;index=SIZE_MAX;
   for(std::size_t i=0;i<compiled->local_defined_call_info.size();++i)
    if(address==reinterpret_cast<std::uintptr_t>(&compiled->local_defined_call_info.index_unchecked(i))){index=i;break;}
  }
  UWVM2TEST_REQUIRE(index<compiled->local_funcs.size());
  UWVM2TEST_REQUIRE(module->imported_function_vec_storage.empty());
  auto const& function=module->local_defined_function_vec_storage.index_unchecked(index);
  auto const& type=*function.function_type_ptr;
  auto count=abi_total_bytes(type.parameter.begin,type.parameter.end);byte_vec parameters;parameters.resize(count);
  // [older operands][validated parameter tuple] stack
  // [safe                                    ] one-past caller's live operand tuple.
  auto base=stack-count;if(count)std::memcpy(parameters.data(),base,count);
  auto result=interpreter_runner<Option>::run(compiled->local_funcs.index_unchecked(index),function,parameters,nullptr,nullptr);
  // Caller translation reserved this result tuple and validates its type/extent before this hook.
  if(!result.results.empty())std::memcpy(base,result.results.data(),result.results.size());
  return base+result.results.size();
 }
};
template<optable::uwvm_interpreter_translate_option_t Option>
void run(byte_vec const& wasm){
 auto f=make_wasm1p1_feature_parameter();
 uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(f).disable_function_references=false;
 auto prepared=prepare_runtime_from_wasm(wasm,u8"reference-control-flow",{},f);
 uwvm2::validation::error::code_validation_error_impl err{};optable::compile_option options;
 auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,err,&f);
 UWVM2TEST_REQUIRE(err.err_code==uwvm2::validation::error::code_validation_error_code::ok);
 calls<Option>::module=prepared.mod;calls<Option>::compiled=&compiled;optable::call_func=calls<Option>::invoke;
 auto index=compiled.local_funcs.size()-1;
 auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),
  prepared.mod->local_defined_function_vec_storage.index_unchecked(index),{},nullptr,nullptr);
 UWVM2TEST_REQUIRE(result.results.empty());
 optable::call_func=nullptr;calls<Option>::compiled=nullptr;calls<Option>::module=nullptr;
}
}
int main(int argc,char** argv){
 UWVM2TEST_REQUIRE(argc>1);install_unexpected_traps();
 for(int i=1;i<argc;++i){
  auto file=std::fopen(argv[i],"rb");UWVM2TEST_REQUIRE(file);UWVM2TEST_REQUIRE(std::fseek(file,0,SEEK_END)==0);
  auto size=std::ftell(file);UWVM2TEST_REQUIRE(size>0);std::rewind(file);byte_vec bytes;bytes.resize(static_cast<std::size_t>(size));
  UWVM2TEST_REQUIRE(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size());std::fclose(file);
  run<k_test_byref_opt>(bytes);run<make_tailcall_scalar4_merged_opt<2>()>(bytes);run<make_tailcall_scalar4_merged_opt<1>()>(bytes);
  std::printf("PASS reference control flow (byref + rings 1/2): %s\n",argv[i]);
 }
}
