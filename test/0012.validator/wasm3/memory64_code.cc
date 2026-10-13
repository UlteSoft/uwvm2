// Core 3 instruction validation against explicit memory type contexts. The
// binary memory64 parser/runtime routes remain separate acceptance requirements.
#include <uwvm2/uwvm/wasm/feature/feature.h>
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
namespace feature=uwvm2::uwvm::wasm::feature;
namespace parser=uwvm2::parser::wasm;
namespace w3=uwvm2::validation::standard::wasm3;
using wasm1=parser::standard::wasm1::features::wasm1;
using wasm1p1=parser::standard::wasm1p1::features::wasm1p1;
using code=uwvm2::validation::error::code_validation_error_code;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
int main()
{
 // Import memory 0 and define memory 1, with identical 32-bit initial parser
 // declarations. After parsing, set their Core 3 address type in the context.
 // This fixture deliberately does not claim binary memory64 parser support.
 constexpr unsigned char module_bytes[]{0,97,115,109,1,0,0,0,
  1,4,1,96,0,0, 2,9,1,1,109,1,109,2,3,1,2, 3,2,1,0,
  5,4,1,3,1,2, 12,1,1, 10,4,1,2,0,11, 11,3,1,1,0};
 feature::wasm_binfmt_ver1_feature_parameter_storage_t parameters{};
 auto& policy=feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters);
 policy.disable_memory64=false;policy.disable_threads=false;policy.disable_multi_memory=false;
 parser::base::error_impl parse_error{};
 auto first=reinterpret_cast<std::byte const*>(module_bytes);
 auto module=feature::binfmt_ver1_handler(first,first+sizeof(module_bytes),parse_error,parameters);
 auto& imports=parser::concepts::operation::get_first_type_in_tuple<parser::standard::wasm1::features::import_section_storage_t<wasm1,wasm1p1>>(module.sections);
 auto& memories=parser::concepts::operation::get_first_type_in_tuple<parser::standard::wasm1::features::memory_section_storage_t<wasm1,wasm1p1>>(module.sections);
 auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
 auto mapping=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
 CHECK(mapping!=MAP_FAILED);CHECK(mprotect(mapping+page,page,PROT_NONE)==0);
 unsigned wide_mask{},disabled{};std::string hex{};
 while(std::cin>>wide_mask>>disabled>>hex)
 {
  CHECK(hex.size()%2==0 && hex.size()/2<page);
  imports.imports.index_unchecked(0).imports.storage.memory.address64=(wide_mask&1)!=0;
  memories.memories.index_unchecked(0).address64=(wide_mask&2)!=0;
  policy.disable_threads=(disabled&1)!=0;policy.disable_multi_memory=(disabled&2)!=0;
  policy.disable_simd=(disabled&4)!=0;policy.disable_bulk_memory=(disabled&8)!=0;
  auto count=hex.size()/2;auto begin=mapping+page-count;
  auto nibble=[](char c)->unsigned {CHECK((c>='0'&&c<='9')||(c>='a'&&c<='f'));return c<='9'?c-'0':c-'a'+10;};
  for(std::size_t i=0;i<count;++i){begin[i]=std::byte((nibble(hex[2*i])<<4)|nibble(hex[2*i+1]));}
  uwvm2::validation::error::code_validation_error_impl error{};bool valid=true;
  try {w3::validate_code(w3::wasm3_code_version{},module,0,begin,mapping+page,error,parameters);}
  catch(fast_io::error const&){valid=false;CHECK(error.err_code!=code::ok);CHECK(error.err_curr>=begin&&error.err_curr<=mapping+page);}
  CHECK(valid==(error.err_code==code::ok));
  std::cout<<valid<<' '<<static_cast<unsigned>(error.err_code)<<' '<<(valid?0:error.err_curr-begin)<<'\n';
 }
 CHECK(std::cin.eof());CHECK(munmap(mapping,page*2)==0);
}
