// Real binary table64 and mixed table32/table64 instructions, with checked results.
#include "../strict/uwvm_int_translate_strict_common.h"
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
#include "memory64_translation_ir.h"
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace
{
using namespace uwvm2test::uwvm_int_strict;
byte_vec raw(std::initializer_list<unsigned> values){byte_vec v;for(auto x:values)append_u8(v,x);return v;}
byte_vec args(std::initializer_list<std::uint64_t> values,std::initializer_list<bool> wide)
{
 byte_vec v;auto w=wide.begin();
 for(auto x:values)
 {
  bool is_wide=*w++;auto n=is_wide?8u:4u;auto old=v.size();v.resize(old+n);
  if(is_wide)std::memcpy(v.data()+old,&x,8);
  else {auto narrow=static_cast<std::uint32_t>(x);std::memcpy(v.data()+old,&narrow,4);}
 }
 return v;
}
byte_vec build(bool d64,bool s64,bool ext,unsigned invalid)
{
 module_builder b{};b.has_table=b.table_has_max=true;b.table_min=3;b.table_max=6;b.table_address64=d64;b.table_elem_type=ext?0x6f:0x70;
 b.extra_tables.push_back({b.table_elem_type,3,6,true,s64});
 auto add=[&](std::initializer_list<std::uint8_t> params,byte_vec body,bool narrow=false)
 {if(narrow)append_u8(body,0xad);append_u8(body,11);func_body f{};f.code=std::move(body);b.add_func({params,{k_val_i64}},std::move(f));};
 auto dt=static_cast<std::uint8_t>(d64?0x7e:0x7f),st=static_cast<std::uint8_t>(s64?0x7e:0x7f),lt=static_cast<std::uint8_t>(d64&&s64?0x7e:0x7f);
 add({},raw({0xfc,16,0}),!d64); // 0: size
 add({dt},raw({0xd0,b.table_elem_type,0x20,0,0xfc,15,0}),!d64); // 1: grow
 add({static_cast<std::uint8_t>(invalid==1?(d64?0x7f:0x7e):dt)},raw({0x20,0,0x25,0,0xd1}),true); // 2: get/null
 auto reference=[&]{return ext?raw({0xd0,0x6f}):raw({0xd2,0});};
 auto set=raw({0x20,0});append_bytes(set,reference());append_bytes(set,raw({0x26,0,0x42,0}));add({dt},std::move(set)); // 3
 auto fill=raw({0x20,0});append_bytes(fill,reference());append_bytes(fill,raw({0x20,1,0xfc,17,0,0x42,0}));add({dt,dt},std::move(fill)); // 4
 add({dt,st,static_cast<std::uint8_t>(invalid==2?(lt==0x7e?0x7f:0x7e):lt)},raw({0x20,0,0x20,1,0x20,2,0xfc,14,0,1,0x42,0})); // 5
 add({dt,k_val_i32,static_cast<std::uint8_t>(invalid==3?k_val_i64:k_val_i32)},raw({0x20,0,0x20,1,0x20,2,0xfc,12,0,0,0x42,0})); // 6
 add({},raw({0xfc,13,0,0x42,0})); // 7: drop
 add({dt,dt,dt},raw({0x20,0,0x20,1,0x20,2,0xfc,14,0,0,0x42,0})); // 8: overlap
 if(ext){passive_element_expr_segment e{};e.ref_type=0x6f;e.init_exprs={raw({0xd0,0x6f,11}),raw({0xd0,0x6f,11})};b.passive_element_exprs.push_back(std::move(e));}
 else b.passive_elements.push_back({{0,0}});
 return b.build();
}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
int check()
#else
template<optable::uwvm_interpreter_translate_option_t Option>int check()
#endif
{
 unsigned checked{};
 for(bool d64:{false,true})for(bool s64:{false,true})for(bool ext:{false,true})for(unsigned invalid=0;invalid<4;++invalid)
 {
  auto wasm=build(d64,s64,ext,invalid);auto features=make_wasm1p1_feature_parameter();
  uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_table64=false;
  auto prepared=prepare_runtime_from_wasm(wasm,u8"table64-ops",{},features);uwvm2::validation::error::code_validation_error_impl error{};
  try
  {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
   jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
   options.emit_call_stack_frames=!memory64_fixture_unwind;options.emit_unwind_call_stack_frames=memory64_fixture_unwind;
   auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);UWVM2TEST_REQUIRE(invalid==0);
   UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);
   UWVM2TEST_REQUIRE(!llvm::verifyModule(*compiled.llvm_jit_module.llvm_module,&llvm::errs()));
   auto name="table-"+std::to_string(d64)+std::to_string(s64)+std::to_string(ext)+".ll";
   save_memory64_translation_ir(*compiled.llvm_jit_module.llvm_module,name.c_str());
   auto run=[&](unsigned index,byte_vec const& arguments)
   {std::uint64_t value{};uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,index,&value,8,arguments.data(),arguments.size());++checked;return value;};
#else
   optable::compile_option options{};auto compiled=compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,error,&features);
   UWVM2TEST_REQUIRE(invalid==0);
   auto run=[&](unsigned index,byte_vec const& arguments){auto result=interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(index),
     prepared.mod->local_defined_function_vec_storage.index_unchecked(index),arguments,nullptr,nullptr);UWVM2TEST_REQUIRE(result.results.size()==8);
     std::uint64_t value{};std::memcpy(&value,result.results.data(),8);++checked;return value;};
#endif
   auto& destination=prepared.mod->local_defined_table_vec_storage.index_unchecked(0);
   // This fixture exclusively owns the mutable runtime instance; seed a host
   // reference slot without changing any parsed table declaration or address type.
   auto& source=const_cast<uwvm2::uwvm::runtime::storage::local_defined_table_storage_t&>(prepared.mod->local_defined_table_vec_storage.index_unchecked(1));
   UWVM2TEST_REQUIRE(run(0,{})==3);UWVM2TEST_REQUIRE(run(2,args({0},{d64}))==1);
   UWVM2TEST_REQUIRE(run(3,args({1},{d64}))==0);UWVM2TEST_REQUIRE(run(2,args({1},{d64}))==(ext?1u:0u));
   UWVM2TEST_REQUIRE(run(4,args({0,3},{d64,d64}))==0);
   auto marker=destination.elems.index_unchecked(1);
   if(ext){static int host_object{};marker.storage.extern_ptr=&host_object;}
   source.elems.index_unchecked(1)=marker;
   UWVM2TEST_REQUIRE(run(5,args({2,1,1},{d64,s64,d64&&s64}))==0);
   if(ext)UWVM2TEST_REQUIRE(destination.elems.index_unchecked(2).storage.extern_ptr==marker.storage.extern_ptr);
   else UWVM2TEST_REQUIRE(destination.elems.index_unchecked(2).storage.defined_ptr==marker.storage.defined_ptr);
   UWVM2TEST_REQUIRE(run(8,args({0,1,2},{d64,d64,d64}))==0);
   if(ext)UWVM2TEST_REQUIRE(destination.elems.index_unchecked(1).storage.extern_ptr==marker.storage.extern_ptr);
   else UWVM2TEST_REQUIRE(destination.elems.index_unchecked(1).storage.defined_ptr==marker.storage.defined_ptr);
   UWVM2TEST_REQUIRE(run(6,args({0,0,2},{d64,false,false}))==0);
   UWVM2TEST_REQUIRE(run(7,{})==0);UWVM2TEST_REQUIRE(run(6,args({3,0,0},{d64,false,false}))==0);
   UWVM2TEST_REQUIRE(run(5,args({3,3,0},{d64,s64,d64&&s64}))==0);
   UWVM2TEST_REQUIRE(run(1,args({0},{d64}))==3);UWVM2TEST_REQUIRE(run(1,args({1},{d64}))==3);UWVM2TEST_REQUIRE(run(0,{})==4);
   for(auto delta:{3ull,0x100000001ull,~0ull})
   {
    if(!d64&&delta==0x100000001ull)continue;
    auto expected=d64?~0ull:0xffffffffull;UWVM2TEST_REQUIRE(run(1,args({delta},{d64}))==expected);UWVM2TEST_REQUIRE(run(0,{})==4);
   }
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
   auto trap=[&](unsigned fn,byte_vec arguments)
   {
    int descriptors[2];UWVM2TEST_REQUIRE(pipe(descriptors)==0);auto pid=fork();UWVM2TEST_REQUIRE(pid>=0);
    if(pid==0){close(descriptors[0]);UWVM2TEST_REQUIRE(dup2(descriptors[1],2)==2);close(descriptors[1]);uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());run(fn,arguments);std::_Exit(0);}
    close(descriptors[1]);std::string diagnostic;char buffer[4096];ssize_t count;
    while((count=read(descriptors[0],buffer,sizeof(buffer)))>0)diagnostic.append(buffer,count);
    close(descriptors[0]);int status{};UWVM2TEST_REQUIRE(waitpid(pid,&status,0)==pid);
    UWVM2TEST_REQUIRE(WIFSIGNALED(status));
    UWVM2TEST_REQUIRE(diagnostic.find("table access out of bounds")!=std::string::npos);
    UWVM2TEST_REQUIRE(diagnostic.find("Call stack:")!=std::string::npos&&diagnostic.find("table64-ops")!=std::string::npos);++checked;
   };
#else
   optable::trap_table_out_of_bounds_func=[]() noexcept{std::_Exit(47);};
   auto trap=[&](unsigned fn,byte_vec arguments){auto pid=fork();UWVM2TEST_REQUIRE(pid>=0);if(pid==0){run(fn,arguments);std::_Exit(0);}int status{};UWVM2TEST_REQUIRE(waitpid(pid,&status,0)==pid);UWVM2TEST_REQUIRE(WIFEXITED(status)&&WEXITSTATUS(status)==47);++checked;};
#endif
   trap(2,args({4},{d64}));trap(3,args({4},{d64}));trap(4,args({3,2},{d64,d64}));trap(6,args({0,0,1},{d64,false,false}));
   if(d64){trap(2,args({0x100000001ull},{true}));trap(4,args({0x100000000ull,0},{true,true}));trap(8,args({0,0,~0ull},{true,true,true}));}
   if(s64)trap(5,args({0,0x100000000ull,0},{d64,true,d64}));
  }
  catch(fast_io::error const&){UWVM2TEST_REQUIRE(invalid!=0);}
  UWVM2TEST_REQUIRE((error.err_code==uwvm2::validation::error::code_validation_error_code::ok)==(invalid==0));
 }
 std::printf("PASS integrated table64: %u executions, both reference types, all four address-width combinations\n",checked);return 0;
}
}
int main([[maybe_unused]] int argc,[[maybe_unused]] char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
 configure_memory64_jit_fixture_policy(argc,argv);return check();
#else
 install_unexpected_traps();check<optable::uwvm_interpreter_translate_option_t{.is_tail_call=false}>();
 check<make_tailcall_scalar4_merged_opt<2>()>();
 return check<make_tailcall_scalar4_merged_opt<1>()>();
#endif
}
