// Core 3 binary table64 declarations, typed instructions and i64 element offsets.
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/cmdline/callback/wasm_feature.h>
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <sys/mman.h>
// Assertion failures in bool/void helpers must terminate the fixture as well.
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace
{
using namespace uwvm2test::uwvm_int_strict;
namespace feature=uwvm2::uwvm::wasm::feature;
namespace parser=uwvm2::parser::wasm;
namespace w3=uwvm2::validation::standard::wasm3;
using error_code=parser::base::wasm_parse_error_code;
unsigned parser_checks{},code_checks{};
byte_vec bytes(std::initializer_list<unsigned> values){byte_vec v;for(auto x:values)append_u8(v,x);return v;}
void section(byte_vec& v,unsigned id,byte_vec const& payload)
{append_u8(v,id);append_u32_leb(v,payload.size());append_bytes(v,payload);}
byte_vec type(unsigned flags,std::uint64_t min=0,std::uint64_t max=4)
{auto v=bytes({0x70,flags});append_u64_leb(v,min);if(flags&1)append_u64_leb(v,max);return v;}
auto policy(bool wide)
{
 auto p=make_wasm1p1_feature_parameter();auto& f=feature::wasm_binfmt_ver1_wasm1p1_parameter(p);
 f.disable_table64=!wide;f.disable_tail_call=false;return p;
}
byte_vec module(byte_vec const& table,bool imported,byte_vec const* second=nullptr,bool active=false,unsigned offset_opcode=0x42,std::int64_t offset=1,bool empty=false)
{
 auto out=bytes({0,97,115,109,1,0,0,0});section(out,1,bytes({1,96,0,0}));
 if(imported){auto p=bytes({1,1,109,1,120,1});append_bytes(p,table);section(out,2,p);}
 section(out,3,bytes({1,0}));
 auto tables=bytes({static_cast<unsigned>((!imported)+bool(second))});if(!imported)append_bytes(tables,table);if(second)append_bytes(tables,*second);
 if(!imported||second)section(out,4,tables);
 // Export table zero for import-linking checks, and declare a passive function segment.
 section(out,7,bytes({1,1,120,1,0}));
 if(active){auto elem=bytes({1,0,offset_opcode});append_i64_leb(elem,offset);append_u8(elem,11);append_u8(elem,empty?0:1);if(!empty)append_u8(elem,0);section(out,9,elem);}
 else section(out,9,bytes({1,1,0,1,0}));
 section(out,10,bytes({1,2,0,11}));return out;
}
bool parse(byte_vec const& raw,bool wide,error_code expected=error_code::ok)
{
 auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));UWVM2TEST_REQUIRE(raw.size()<page);
 auto map=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));UWVM2TEST_REQUIRE(map!=MAP_FAILED);
 UWVM2TEST_REQUIRE(mprotect(map+page,page,PROT_NONE)==0);auto end=map+page;auto begin=end-raw.size();std::memcpy(begin,raw.data(),raw.size());
 parser::base::error_impl err{};bool valid=true;
 try{auto parsed=feature::binfmt_ver1_handler(begin,end,err,policy(wide));}
 catch(fast_io::error const&){valid=false;UWVM2TEST_REQUIRE(err.err_curr>=begin&&err.err_curr<=end);if(expected!=error_code::ok)UWVM2TEST_REQUIRE(err.err_code==expected);}
 UWVM2TEST_REQUIRE(munmap(map,page*2)==0);++parser_checks;return valid;
}
void validate(byte_vec const& raw,byte_vec const& body,bool expected,bool enabled=true)
{
 auto parameters=policy(true);parser::base::error_impl parse_error{};
 auto parsed=feature::binfmt_ver1_handler(raw.data(),raw.data()+raw.size(),parse_error,parameters);
 feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters).disable_table64=!enabled;
 auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));auto map=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
 UWVM2TEST_REQUIRE(map!=MAP_FAILED&&body.size()<page);UWVM2TEST_REQUIRE(mprotect(map+page,page,PROT_NONE)==0);
 auto end=map+page;auto begin=end-body.size();if(!body.empty())std::memcpy(begin,body.data(),body.size());
 uwvm2::validation::error::code_validation_error_impl error{};bool valid=true;
 try{w3::validate_code(w3::wasm3_code_version{},parsed,0,begin,end,error,parameters);}
 catch(fast_io::error const&){valid=false;UWVM2TEST_REQUIRE(error.err_curr>=begin&&error.err_curr<=end);}
 UWVM2TEST_REQUIRE(valid==expected);UWVM2TEST_REQUIRE(munmap(map,page*2)==0);++code_checks;
}
}
int main()
{
 for(bool imported:{false,true})for(bool enabled:{false,true})for(unsigned flag=0;flag<256;++flag)
  UWVM2TEST_REQUIRE(parse(module(type(flag),imported),enabled)==(flag<8&&!(flag&2)&&(!(flag&4)||enabled)));
 for(bool imported:{false,true})
 {
  for(unsigned flag:{4u,5u})
  {
   auto limits=type(flag,~std::uint64_t{},~std::uint64_t{});
   UWVM2TEST_REQUIRE(parse(module(limits,imported),true));
   for(std::size_t n=0;n<limits.size();++n){auto prefix=limits;prefix.resize(n);UWVM2TEST_REQUIRE(!parse(module(prefix,imported),true));}
   UWVM2TEST_REQUIRE(!parse(module(limits,imported),false,error_code::wasm1p1_feature_required));
  }
  UWVM2TEST_REQUIRE(!parse(module(type(0,1ull<<32),imported),true,error_code::wasm3_table_limit_out_of_range));
  UWVM2TEST_REQUIRE(!parse(module(type(5,1ull<<40,(1ull<<40)-1),imported),true,error_code::wasm3_limit_type_max_lt_min));
  auto wide=policy(true);auto raw=module(type(5,2,4),imported,nullptr,true);auto provider=module(type(5,2,4),false);
  auto prepared=imported?prepare_runtime_from_wasm(raw,u8"table64",{{&provider,u8"m",&wide}},wide):prepare_runtime_from_wasm(raw,u8"table64",{},wide);
  auto const& table=imported?*prepared.mod->imported_table_vec_storage.index_unchecked(0).target.defined_ptr:prepared.mod->local_defined_table_vec_storage.index_unchecked(0);
  UWVM2TEST_REQUIRE(table.table_type_ptr->address64&&table.elems.size()==2);
  UWVM2TEST_REQUIRE(table.elems.index_unchecked(1).storage.defined_ptr!=nullptr);
  UWVM2TEST_REQUIRE(!parse(module(type(5,2,4),imported,nullptr,true,0x41),true));
  auto high=module(type(5,2,4),imported,nullptr,true,0x42,0x100000001ll,true);
  UWVM2TEST_REQUIRE(run_in_child_expect_trap_message("element segment initialization would write past table bounds",[&]{
   uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
   auto failed=imported?prepare_runtime_from_wasm(high,u8"high-table64",{{&provider,u8"m",&wide}},wide):prepare_runtime_from_wasm(high,u8"high-table64",{},wide);
  })==0);
  UWVM2TEST_REQUIRE(run_in_child_expect_trap_message("--wasm-feature-enable-table64",[&]{
   uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());parser::base::error_impl error{};
   auto parsed=feature::binfmt_ver1_handler(raw.data(),raw.data()+raw.size(),error,wide);
   uwvm2::uwvm::runtime::initializer::details::enforce_wasm1p1_initializer_feature_parameters(parsed,policy(false));
  })==0);
 }
 for(unsigned mask=0;mask<4;++mask)for(bool imported:{false,true})
 {
  auto second=type(mask&2?5:1,2,4);auto raw=module(type(mask&1?5:1,2,4),imported,&second);
  for(unsigned index=0;index<2;++index)for(bool wrong:{false,true})
  {
   unsigned address=((mask>>index)&1)^wrong?0x42:0x41;
   auto test=[&](byte_vec b){append_u8(b,11);validate(raw,b,!wrong);};
   test(bytes({address,0,0x25,index,0x1a}));
   test(bytes({address,0,0xd0,0x70,0x26,index}));
   test(bytes({0xd0,0x70,address,0,0xfc,15,index,0x1a}));
   test(bytes({address,0,0xd0,0x70,address,0,0xfc,17,index}));
   test(bytes({address,0,0x41,0,0x41,0,0xfc,12,0,index}));
   test(bytes({address,0,0x11,0,index}));
   test(bytes({address,0,0x13,0,index}));
   if(!wrong)validate(raw,bytes({0xfc,16,index,address==0x41?0x45u:0x50u,0x1a,11}),true); // i64.eqz / i32.eqz
  }
  for(unsigned dst=0;dst<2;++dst)for(unsigned src=0;src<2;++src)for(bool wrong:{false,true})
  {
   unsigned da=mask&(1<<dst)?0x42:0x41,sa=mask&(1<<src)?0x42:0x41;
   unsigned length=((da==0x42&&sa==0x42)^wrong)?0x42:0x41;
   validate(raw,bytes({da,0,sa,0,length,0,0xfc,14,dst,src,11}),!wrong);
  }
  validate(raw,bytes({11}),mask==0,false);
  for(auto op:{bytes({0x25}),bytes({0xfc,12}),bytes({0xfc,12,0}),bytes({0xfc,14}),bytes({0xfc,14,0}),bytes({0x11,0}),bytes({0x13,0})})validate(raw,op,false);
 }
 std::printf("PASS table64 binary parser cases=%u typed validation cases=%u\n",parser_checks,code_checks);
}
