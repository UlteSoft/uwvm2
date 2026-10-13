// Focused Core 3 data-parser/initializer test. Explicit semantic address metadata
// is supplied until the integrated memory64 compilers enable binary declarations.
#ifndef UWVM
#define UWVM 2
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
namespace feature=uwvm2::uwvm::wasm::feature;
namespace parser=uwvm2::parser::wasm;
namespace f=parser::standard::wasm1::features;
namespace f11=parser::standard::wasm1p1::features;
namespace init=uwvm2::uwvm::runtime::initializer::details;
namespace storage=uwvm2::uwvm::runtime::storage;
namespace strict=uwvm2test::uwvm_int_strict;
using bytes=strict::byte_vec;
using parsed_module=feature::wasm_binfmt_ver1_module_storage_t;
using policy=feature::wasm_binfmt_ver1_feature_parameter_storage_t;
using data_t=f11::wasm1p1_data_t<f::wasm1,f11::wasm1p1>;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
std::size_t checks{};
using fixture_memory=decltype(storage::local_defined_memory_storage_t{}.memory);
#if defined(UWVM_TEST_EXPECT_MMAP)
static_assert(fixture_memory::can_mmap, "mmap qualification must not silently select an allocator");
#endif
#if defined(UWVM_FORCE_DISABLE_MMAP)
static_assert(!fixture_memory::can_mmap, "allocator qualification must not select mmap");
#endif
bytes to_bytes(std::initializer_list<unsigned> values){bytes r{};for(auto b:values)r.push_back(std::byte(b));return r;}
void section(bytes& output,unsigned id,std::initializer_list<unsigned> values)
{CHECK(values.size()<128);output.push_back(std::byte(id));output.push_back(std::byte(values.size()));for(auto b:values)output.push_back(std::byte(b));}
bytes context_bytes()
{
 auto result=to_bytes({0,97,115,109,1,0,0,0});
 section(result,2,{2,1,'m',1,'m',2,1,1,2, 1,'m',1,'g',3,0x7e,0});
 section(result,5,{1,1,1,2});
 section(result,6,{3,0x7e,0,0x42,5,11, 0x7e,1,0x42,9,11, 0x7f,0,0x41,7,11});
 section(result,11,{1,1,1,0xab});return result;
}
policy parameters(bool extended=true)
{
 policy result{};auto& options=feature::wasm_binfmt_ver1_wasm1p1_parameter(result);
 options.disable_memory64=false;options.disable_multi_memory=false;options.disable_extended_const=!extended;return result;
}
auto& memories(parsed_module& m){return parser::concepts::operation::get_first_type_in_tuple<f::memory_section_storage_t<f::wasm1,f11::wasm1p1>>(m.sections);}
auto& imports(parsed_module& m){return parser::concepts::operation::get_first_type_in_tuple<f::import_section_storage_t<f::wasm1,f11::wasm1p1>>(m.sections);}
auto& datas(parsed_module& m){return parser::concepts::operation::get_first_type_in_tuple<f::data_section_storage_t<f::wasm1,f11::wasm1p1>>(m.sections);}
void set_widths(parsed_module& m,unsigned mask)
{
 auto& a=imports(m).imports.index_unchecked(0).imports.storage.memory;
 auto& b=memories(m).memories.index_unchecked(0);
 a.address64=(mask&1)!=0;b.address64=(mask&2)!=0;
 a.limits.max=a.address64?(1ull<<48):2;b.limits.max=b.address64?(1ull<<48):2;
}
bool parse_segment(parsed_module& m,policy const& options,std::byte const* begin,std::byte const* end,data_t& data)
{
 parser::base::error_impl error{};
 try
 {
  if(begin==end)return false;
  data.type=static_cast<decltype(data.type)>(std::to_integer<unsigned char>(*begin));
  auto result=f::define_handler_data_type(parser::concepts::feature_reserve_type_t<f::data_section_storage_t<f::wasm1,f11::wasm1p1>>{},
   data.storage,data.type,m,begin+1,end,error,options);
  return result==end;
 }
 catch(fast_io::error const&){CHECK(error.err_curr>=begin&&error.err_curr<=end);return false;}
}
parsed_module parse_context(bytes const& raw,policy const& options)
{parser::base::error_impl err{};return feature::binfmt_ver1_handler(raw.data(),raw.data()+raw.size(),err,options);}
void link_aliases(storage::wasm_module_storage_t& rt)
{
 auto& memory=rt.imported_memory_vec_storage.index_unchecked(0);
 memory.link_kind=storage::imported_memory_storage_t::imported_memory_link_kind::defined;
 memory.target.defined_ptr=std::addressof(rt.local_defined_memory_vec_storage.index_unchecked(0));
 auto& global=rt.imported_global_vec_storage.index_unchecked(0);
 global.link_kind=storage::imported_global_storage_t::imported_global_link_kind::defined;
 global.target.defined_ptr=std::addressof(rt.local_defined_global_vec_storage.index_unchecked(0));
 for(auto& g:rt.local_defined_global_vec_storage)g.owner_module_rt_ptr=std::addressof(rt);
}
void initialize_case(bytes const& segment,std::uint64_t offset,bool apply=true,std::size_t initial_pages=1)
{
 auto raw=context_bytes();auto options=parameters();auto m=parse_context(raw,options);set_widths(m,3);
 memories(m).memories.index_unchecked(0).limits.min=initial_pages;
 imports(m).imports.index_unchecked(0).imports.storage.memory.limits.min=initial_pages;
 data_t data{};CHECK(parse_segment(m,options,segment.data(),segment.data()+segment.size(),data));
 datas(m).datas.index_unchecked(0)=std::move(data);
 storage::wasm_module_storage_t rt{};init::initialize_from_binfmt_ver1_module_storage(m,options,rt);link_aliases(rt);
 auto& d=rt.local_defined_data_vec_storage.index_unchecked(0).data;
 auto const& expr=datas(m).datas.index_unchecked(0).storage.segment.expr;
 init::try_eval_wasm3_memory_const_expr_offset_after_linking(expr,rt,d.offset,init::runtime_memory_is_address64(rt,d.memory_idx));
 CHECK(d.offset==offset);CHECK(init::runtime_memory_is_address64(rt,0));CHECK(init::runtime_memory_is_address64(rt,1));
 auto& memory=rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
 [&]<class Memory>(Memory const& mem){if constexpr(Memory::can_mmap){CHECK(mem.status==decltype(mem.status)::wasm64);CHECK(!mem.is_full_page_protection());}}(memory);
 if(apply)
 {
  auto payload_size=init::safe_ptr_range_size(d.byte_begin,d.byte_end);
  init::apply_wasm1_active_element_and_data_segments_for_module(u8"memory64-initializer",rt);
  CHECK(storage::wasm_data_segment_is_dropped(d));
  if(payload_size){CHECK(memory.memory_begin[offset]==std::byte{0xab});CHECK(memory.memory_begin[offset-1]==std::byte{});CHECK(memory.memory_begin[offset+payload_size]==std::byte{});}
 }
 ++checks;
}
int main(int argc,char**)
{
 if(argc>1)
 {
  auto raw=context_bytes();auto options=parameters();auto m=parse_context(raw,options);
  auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
  auto mapping=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
  CHECK(mapping!=MAP_FAILED);CHECK(mprotect(mapping+page,page,PROT_NONE)==0);
  unsigned mask{},extended{};std::string hex{};
  while(std::cin>>mask>>extended>>hex)
  {
   set_widths(m,mask);options=parameters(extended!=0);CHECK(hex.size()%2==0&&hex.size()/2<page);
   auto begin=mapping+page-hex.size()/2;
   auto nibble=[](char c)->unsigned{CHECK((c>='0'&&c<='9')||(c>='a'&&c<='f'));return c<='9'?c-'0':c-'a'+10;};
   for(std::size_t i=0;i<hex.size()/2;++i)begin[i]=std::byte((nibble(hex[2*i])<<4)|nibble(hex[2*i+1]));
   data_t data{};std::cout<<parse_segment(m,options,begin,mapping+page,data)<<'\n';
  }
  CHECK(std::cin.eof());CHECK(munmap(mapping,page*2)==0);return 0;
 }
 // Both active forms, explicit imported/local memory and imported/local immutable globals.
 initialize_case(to_bytes({0,0x42,17,11,1,0xab}),17);
 initialize_case(to_bytes({2,1,0x42,17,11,1,0xab}),17);
 initialize_case(to_bytes({2,0,0x23,0,11,1,0xab}),5);
 initialize_case(to_bytes({2,1,0x23,1,0x42,7,0x7e,11,1,0xab}),35);
 initialize_case(to_bytes({2,1,0x42,0x7f,0x42,18,0x7c,11,1,0xab}),17);
 // 2^32 and all-ones must survive both preliminary and linked evaluation unchanged.
 initialize_case(to_bytes({2,1,0x42,0x80,0x80,0x80,0x80,0x10,11,0}),1ull<<32,false);
 initialize_case(to_bytes({2,1,0x42,0x7f,11,0}),~0ull,false);
 initialize_case(to_bytes({2,1,0x42,0x80,0x80,4,11,0}),65536); // Empty exact endpoint.
 initialize_case(to_bytes({2,1,0x42,0,11,0}),0);
 initialize_case(to_bytes({2,1,0x42,0,11,0}),0,true,0); // Empty zero-page memory.
 using native_memory=decltype(storage::local_defined_memory_storage_t{}.memory);
 if constexpr(native_memory::can_mmap && sizeof(std::size_t)>=8)
 {
  // Sparse commitment touches only the data byte and its sentinels above 4 GiB.
  initialize_case(to_bytes({2,1,0x42,0x81,0x80,0x80,0x80,0x10,11,1,0xab}),(1ull<<32)+1,true,65537);
 }
 for(auto segment:{to_bytes({2,1,0x42,0x80,0x80,4,11,1,0xab}),
                   to_bytes({2,1,0x42,0x81,0x80,4,11,0}),
                   to_bytes({2,1,0x42,0x80,0x80,0x80,0x80,0x10,11,0}),
                   to_bytes({2,1,0x42,0x7f,11,0})})
 {
  CHECK(strict::run_in_child_expect_trap_message("data segment initialization would write past memory bounds",[&]
  {
   uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
   auto raw=context_bytes();auto options=parameters();auto m=parse_context(raw,options);set_widths(m,3);
   data_t data{};CHECK(parse_segment(m,options,segment.data(),segment.data()+segment.size(),data));datas(m).datas.index_unchecked(0)=std::move(data);
   storage::wasm_module_storage_t rt{};init::initialize_from_binfmt_ver1_module_storage(m,options,rt);link_aliases(rt);
   init::apply_wasm1_active_element_and_data_segments_for_module(u8"out-of-bounds",rt);
  })==0);++checks;
 }
 // Independent initializer limit validation must not confuse wide declarations with native capacity.
 for(unsigned mask:{0u,3u})
 {
  CHECK(strict::run_in_child_expect_trap_message("memory limit exceeds",[&]
  {
   uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
   auto raw=context_bytes();auto options=parameters();auto m=parse_context(raw,options);set_widths(m,mask);
   memories(m).memories.index_unchecked(0).limits.max=(1ull<<(mask?48:16))+1;
   storage::wasm_module_storage_t rt{};init::initialize_from_binfmt_ver1_module_storage(m,options,rt);
  })==0);++checks;
 }
 std::printf("PASS memory64 initializer (%s): %zu allocation, address64 alias, full-width offset, extended/global expression and bounds checks\n",fixture_memory::can_mmap?"mmap":"allocator",checks);
}
