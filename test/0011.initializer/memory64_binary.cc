// Core 3 binary memory types, policy isolation and initializer gates.
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/cmdline/callback/wasm_feature.h>
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <sys/mman.h>
#include <string_view>
namespace
{
using namespace uwvm2test::uwvm_int_strict;
namespace feature=uwvm2::uwvm::wasm::feature;
namespace parser=uwvm2::parser::wasm;
namespace init=uwvm2::uwvm::runtime::initializer::details;
using error_code=parser::base::wasm_parse_error_code;
unsigned checks{};
byte_vec type(unsigned flags,std::uint64_t min=0,std::uint64_t max=1)
{byte_vec out{};append_u8(out,flags);append_u64_leb(out,min);if(flags&1)append_u64_leb(out,max);return out;}
byte_vec module(byte_vec const& memory,bool imported)
{
 byte_vec out{};for(unsigned x:{0,97,115,109,1,0,0,0})append_u8(out,x);
 byte_vec section{};append_u8(section,1);if(imported)for(unsigned x:{1,109,1,120,2})append_u8(section,x);
 append_bytes(section,memory);append_u8(out,imported?2:5);append_u32_leb(out,section.size());append_bytes(out,section);return out;
}
auto policy(bool memory64,bool threads)
{
 auto p=make_wasm1p1_feature_parameter();auto& f=feature::wasm_binfmt_ver1_wasm1p1_parameter(p);
 f.disable_memory64=!memory64;f.disable_threads=!threads;f.cli_mode=parser::standard::wasm1p1::features::wasm_feature_cli_mode::scoped;return p;
}
bool parse(byte_vec const& bytes,bool memory64,bool threads,error_code expected=error_code::ok,
           [[maybe_unused]] std::u8string_view diagnostic={})
{
 auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));UWVM2TEST_REQUIRE(bytes.size()<page);
 auto mapping=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
 UWVM2TEST_REQUIRE(mapping!=MAP_FAILED&&mprotect(mapping+page,page,PROT_NONE)==0);
 auto begin=mapping+page-bytes.size(),end=mapping+page;std::memcpy(begin,bytes.data(),bytes.size());
 parser::base::error_impl error{};bool accepted=true;
 try{auto parsed=feature::binfmt_ver1_handler(begin,end,error,policy(memory64,threads));}
 catch(fast_io::error const&)
 {
  accepted=false;UWVM2TEST_REQUIRE(error.err_curr>=begin&&error.err_curr<=end);
  if(expected!=error_code::ok)UWVM2TEST_REQUIRE(error.err_code==expected);
  // Keep sanitizer coverage on bounded parsing and complete numeric diagnostics.
  // The CLI fixture checks rendered messages separately, avoiding instantiating
  // every parser/validator formatter under sanitizer instrumentation here.
  if(expected==error_code::wasm3_memory_limit_out_of_range)
   UWVM2TEST_REQUIRE(error.err_selectable.u64arr[0]==(diagnostic==u8"4294967296"?1ull<<32:(1ull<<48)+1));
  if(expected==error_code::wasm3_limit_type_max_lt_min)
   UWVM2TEST_REQUIRE(error.err_selectable.u64arr[0]==(1ull<<40)-1&&error.err_selectable.u64arr[1]==(1ull<<40));
 }
 UWVM2TEST_REQUIRE(munmap(mapping,page*2)==0);++checks;return accepted;
}
}
int main()
{
 for(bool imported:{false,true})for(bool wide:{false,true})for(bool threads:{false,true})for(unsigned flags=0;flags<256;++flags)
 {
  bool valid=flags<8&&(!(flags&2)||(flags&1))&&(!(flags&4)||wide)&&(!(flags&2)||threads);
  UWVM2TEST_REQUIRE(parse(module(type(flags),imported),wide,threads)==valid);
 }
 for(bool imported:{false,true})
 {
  for(unsigned flags:{4u,5u,7u})
  {
   UWVM2TEST_REQUIRE(parse(module(type(flags,1ull<<48,1ull<<48),imported),true,true));
   UWVM2TEST_REQUIRE(!parse(module(type(flags,0),imported),false,true,error_code::wasm1p1_feature_required,u8"--wasm-feature-enable-memory64"));
   auto full=type(flags,1ull<<48,1ull<<48);
   for(std::size_t n=0;n<full.size();++n){auto truncated=full;truncated.resize(n);UWVM2TEST_REQUIRE(!parse(module(truncated,imported),true,true));}
   UWVM2TEST_REQUIRE(!parse(module(type(flags,(1ull<<48)+1,(1ull<<48)+1),imported),true,true,error_code::wasm3_memory_limit_out_of_range,u8"281474976710657"));
  }
  UWVM2TEST_REQUIRE(!parse(module(type(5,1ull<<40,(1ull<<40)-1),imported),true,true,error_code::wasm3_limit_type_max_lt_min,u8"1099511627775"));
  UWVM2TEST_REQUIRE(!parse(module(type(7),imported),true,false,error_code::wasm1p1_feature_required,u8"--wasm-feature-enable-threads"));
  UWVM2TEST_REQUIRE(!parse(module(type(0,1ull<<32),imported),true,false,error_code::wasm3_memory_limit_out_of_range,u8"4294967296"));
  for(unsigned flags:{0u,4u})
  {
   byte_vec padded{};append_u8(padded,flags);for(unsigned n=0;n<9;++n)append_u8(padded,128);append_u8(padded,0);
   UWVM2TEST_REQUIRE(parse(module(padded,imported),true,false));
   UWVM2TEST_REQUIRE(!parse(module(padded,imported),false,false));
   padded.back()=std::byte{2};UWVM2TEST_REQUIRE(!parse(module(padded,imported),true,false,error_code::limit_type_invalid_min));
  }
  auto raw=module(type(5,1,2),imported);auto enabled=policy(true,false);
  UWVM2TEST_REQUIRE(run_in_child_expect_trap_message("--wasm-feature-enable-memory64",[&]
  {
   uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());parser::base::error_impl error{};
   auto parsed=feature::binfmt_ver1_handler(raw.data(),raw.data()+raw.size(),error,enabled);
   init::enforce_wasm1p1_initializer_feature_parameters(parsed,policy(false,false));
  })==0);
 }
 // Real local/imported memory64 active segments use i64 offsets, including shared memory.
 for(bool imported:{false,true})for(bool shared:{false,true})
 {
  auto features=policy(true,shared);auto raw=module(type(shared?7:5,1,2),imported);
  for(unsigned x:{11,7,1,0,0x42,16,11,1,0xab})append_u8(raw,x);
  auto provider=module(type(shared?7:5,1,2),false);for(unsigned x:{7,5,1,1,120,2,0})append_u8(provider,x);
  auto prepared=imported?prepare_runtime_from_wasm(raw,u8"memory64-data",{{&provider,u8"m",&features}},features):
   prepare_runtime_from_wasm(raw,u8"memory64-data",{},features);
  auto const& owner=imported?*prepared.mod->imported_memory_vec_storage.index_unchecked(0).target.defined_ptr:
   prepared.mod->local_defined_memory_vec_storage.index_unchecked(0);
  UWVM2TEST_REQUIRE(owner.memory_type_ptr->address64&&owner.memory_type_ptr->shared==shared);
  UWVM2TEST_REQUIRE(owner.memory.memory_begin[16]==std::byte{0xab});
 }
 // Enabling memory64 must not accidentally accept independently disabled table64 syntax.
 byte_vec table{};for(unsigned x:{0,97,115,109,1,0,0,0,4,4,1,0x70,4,0})append_u8(table,x);
 UWVM2TEST_REQUIRE(!parse(table,true,false));
 // A reused parsed declaration must also obey an explicitly stricter code validator.
 module_builder b{};b.has_memory=b.memory_address64=true;b.memory_min=1;
 func_body body{};append_u8(body.code,11);b.add_func({{},{}},std::move(body));auto raw=b.build();
 parser::base::error_impl parse_error{};auto parsed=feature::binfmt_ver1_handler(raw.data(),raw.data()+raw.size(),parse_error,policy(true,false));
 std::byte code[]{std::byte{11}};uwvm2::validation::error::code_validation_error_impl error{};
 bool rejected=false;try{uwvm2::validation::standard::wasm3::validate_code_with_runtime_policy(parsed,0,code,code+1,error,policy(false,false));}
 catch(fast_io::error const&){rejected=true;UWVM2TEST_REQUIRE(error.err_selectable.wasm1p1_feature_required.feature==parser::base::wasm1p1_feature_kind::memory64);}
 UWVM2TEST_REQUIRE(rejected);
 rejected=false;error={};try{uwvm2::validation::standard::wasm3::validate_code_with_runtime_policy(parsed,0,code,code+1,error,make_wasm1p1_feature_parameter());}
 catch(fast_io::error const&){rejected=true;}
 UWVM2TEST_REQUIRE(rejected&&error.err_selectable.wasm1p1_feature_required.feature==parser::base::wasm1p1_feature_kind::memory64);
 // Independent CLI switches must neither enable threads nor table64.
 namespace cli=uwvm2::uwvm::cmdline::params::details;auto& f=cli::wasm_feature_details::wasm1p1_parameter();f={};
 uwvm2::utils::cmdline::parameter_parsing_results argument{};argument.str=u8"--wasm-feature-enable-memory64";
 UWVM2TEST_REQUIRE(cli::wasm_feature_enable_memory64_callback(&argument,&argument,&argument+1)==uwvm2::utils::cmdline::parameter_return_type::def);
 UWVM2TEST_REQUIRE(!f.disable_memory64&&f.disable_table64&&f.disable_threads&&f.disable_multi_memory&&f.disable_tail_call);
 auto saved=f;cli::wasm_feature_details::apply_wasm2_feature_set(f);UWVM2TEST_REQUIRE(f.disable_memory64);f=saved;
 argument.str=u8"--wasm-feature-disable-memory64";
 UWVM2TEST_REQUIRE(cli::wasm_feature_disable_memory64_callback(&argument,&argument,&argument+1)==uwvm2::utils::cmdline::parameter_return_type::return_m1_imme);
 UWVM2TEST_REQUIRE(!f.disable_memory64);f={};
 UWVM2TEST_REQUIRE(cli::wasm_feature_disable_memory64_callback(&argument,&argument,&argument+1)==uwvm2::utils::cmdline::parameter_return_type::def);
 UWVM2TEST_REQUIRE(f.disable_memory64&&f.explicit_disable_memory64);
 std::printf("PASS binary memory64: %u guarded parser cases, u64 diagnostics, two initializer policy gates, validator gate, isolated CLI ownership\n",checks);
}
