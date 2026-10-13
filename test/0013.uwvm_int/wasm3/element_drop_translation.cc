// Exercise all element payload forms through actual table.init/elem.drop entries.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
#include "memory64_translation_ir.h"
namespace
{
using namespace uwvm2test::uwvm_int_strict;
namespace storage=uwvm2::uwvm::runtime::storage;
byte_vec build(unsigned form)
{
 module_builder b{};b.has_table=b.table_has_max=true;b.table_min=b.table_max=2;b.table_elem_type=form==2?k_ref_externref:k_ref_funcref;
 func_body dummy{};for(unsigned v:{0x41u,19u,11u})append_u8(dummy.code,v);b.add_func({{},{k_val_i32}},std::move(dummy));
 if(form==0)b.passive_elements.push_back({{0,0}});
 else
 {
  byte_vec first{},second{};if(form==1){append_u8(first,0xd2);append_u8(first,0);}
  else{append_u8(first,0xd0);append_u8(first,k_ref_externref);}append_u8(first,11);
  append_u8(second,0xd0);append_u8(second,b.table_elem_type);append_u8(second,11);
  b.passive_element_exprs.push_back({b.table_elem_type,{first,second}});
 }
 func_body init{};for(unsigned n=0;n<3;++n){append_u8(init.code,0x20);append_u8(init.code,n);}
 for(unsigned v:{0xfcu,12u,0u,0u,0x41u,0u,0x25u,0u,0xd1u,11u})append_u8(init.code,v);
 b.add_func({{k_val_i32,k_val_i32,k_val_i32},{k_val_i32}},std::move(init));
 func_body drop{};for(unsigned v:{0xfcu,13u,0u,0xfcu,13u,0u,0x41u,11u,11u})append_u8(drop.code,v);
 b.add_func({{},{k_val_i32}},std::move(drop));return b.build();
}
byte_vec arguments(std::array<std::uint32_t,3> v)
{byte_vec result(12);std::memcpy(result.data(),v.data(),12);return result;}
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
void UWVM2TEST_WASM_ABI table_trap() noexcept {::_exit(42);}
#endif
template<class Run>int expect_trap(Run&& run,std::array<std::uint32_t,3> operands)
{
 int output[2]{};UWVM2TEST_REQUIRE(::pipe(output)==0);auto child=::fork();UWVM2TEST_REQUIRE(child>=0);
 if(child==0)
 {
  ::close(output[0]);(void)::dup2(output[1],STDERR_FILENO);::close(output[1]);::alarm(15);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
  uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
#else
  optable::trap_table_out_of_bounds_func=table_trap;
#endif
  (void)run(1,arguments(operands));::_exit(99);
 }
 ::close(output[1]);std::string diagnostic;char data[1024];
 for(;;){auto n=::read(output[0],data,sizeof(data));if(n<=0)break;diagnostic.append(data,n);}
 ::close(output[0]);int status{};UWVM2TEST_REQUIRE(::waitpid(child,&status,0)==child);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 if(!WIFSIGNALED(status)||diagnostic.find("table access out of bounds")==std::string::npos||diagnostic.find("func_idx=")==std::string::npos)
 {std::fprintf(stderr,"element trap failed status=%d %s\n",status,diagnostic.c_str());return 1;}
#else
 UWVM2TEST_REQUIRE(WIFEXITED(status)&&WEXITSTATUS(status)==42);
#endif
 return 0;
}
template<class Run>int execute(prepared_runtime& prepared,unsigned form,Run&& run)
{
 auto const& element=prepared.mod->local_defined_element_vec_storage.index_unchecked(0).element;
 auto retained=storage::load_wasm_element_segment_payload(element);
 UWVM2TEST_REQUIRE(!storage::wasm_element_segment_is_dropped(element));
 auto const expected=form==2?1u:0u;
 UWVM2TEST_REQUIRE(run(1,arguments({0,0,2}))==expected);
 byte_vec empty{};UWVM2TEST_REQUIRE(run(2,empty)==11);
 auto now=storage::load_wasm_element_segment_payload(element);
 UWVM2TEST_REQUIRE(storage::wasm_element_segment_is_dropped(element));
 UWVM2TEST_REQUIRE(!now.funcidx_begin&&!now.funcidx_end&&!now.funcref_begin&&!now.funcref_end&&!now.externref_begin&&!now.externref_end);
 UWVM2TEST_REQUIRE(element.funcidx_begin==retained.funcidx_begin&&element.funcref_begin==retained.funcref_begin&&element.externref_begin==retained.externref_begin);
 UWVM2TEST_REQUIRE(run(1,arguments({2,0,0}))==expected); // dropped source still admits the empty zero-offset range
 UWVM2TEST_REQUIRE(expect_trap(run,{0,0,1})==0);UWVM2TEST_REQUIRE(expect_trap(run,{0,1,0})==0);
 std::printf("PASS integrated elem.drop form=%u: initialized references retained, idempotent drop, empty endpoint, two source bounds traps\n",form);return 0;
}
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
template<optable::uwvm_interpreter_translate_option_t Option>
#endif
int check()
{
 for(unsigned form=0;form<3;++form)
 {
  auto wasm=build(form);auto features=make_wasm1p1_feature_parameter();auto prepared=prepare_runtime_from_wasm(wasm,u8"element-drop",{},features);
  uwvm2::validation::error::code_validation_error_impl error{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
  namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
  jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
  options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
  auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);
  UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted&&!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));
  UWVM2TEST_REQUIRE(execute(prepared,form,[&](unsigned index,byte_vec const& args)
  {std::uint32_t result{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,&result,4,args.data(),args.size());return result;})==0);
#else
  optable::compile_option options{};auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features);
  UWVM2TEST_REQUIRE(execute(prepared,form,[&](unsigned index,byte_vec const& args)
  {auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),prepared.mod->local_defined_function_vec_storage.index_unchecked(index),args,nullptr,nullptr);
   return load_i32(result.results);})==0);
#endif
  UWVM2TEST_REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok);
 }
 return 0;
}
}
int main([[maybe_unused]] int argc,[[maybe_unused]] char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);return check();
#else
 install_unexpected_traps();UWVM2TEST_REQUIRE(check<optable::uwvm_interpreter_translate_option_t{.is_tail_call=false}>()==0);
 constexpr auto merged=make_tailcall_scalar4_merged_opt<2>();return check<merged>();
#endif
}
