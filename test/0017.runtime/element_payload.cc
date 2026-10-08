#define UWVM_DISABLE_INT
#define UWVM_DISABLE_JIT
#ifndef UWVM
#define UWVM 2
#endif
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/storage/impl.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>
namespace s=uwvm2::uwvm::runtime::storage;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL element payload %u: %s\n",__LINE__,#x);std::abort();}}while(false)
static_assert(std::is_trivially_copyable_v<s::wasm_element_storage_t>);
constexpr bool constant_drop()
{
 s::wasm_element_storage_t value{};if(s::wasm_element_segment_is_dropped(value))return false;
 s::drop_wasm_element_segment_payload(value);auto p=s::load_wasm_element_segment_payload(value);
 return s::wasm_element_segment_is_dropped(value)&&!p.funcidx_begin&&!p.funcidx_end&&!p.funcref_begin&&!p.funcref_end&&!p.externref_begin&&!p.externref_end;
}
static_assert(constant_drop());
int main()
{
 std::array<s::wasm_element_storage_t::func_idx_t,16> indices{};
 std::array<s::local_defined_table_elem_storage_t,16> refs{};std::array<void*,16> external{};
 for(unsigned i=0;i<16;++i){indices[i]=i*7+3;external[i]=&indices[i];}
 unsigned snapshots{};
 for(unsigned round=0;round<32;++round)
 {
  std::array<s::wasm_element_storage_t,513> values{};
  for(unsigned i=0;i<values.size();++i)
  {
   auto& v=values[i];v.kind=s::wasm_element_segment_kind::passive;
   if(i%3==0){v.funcidx_begin=indices.data();v.funcidx_end=indices.data()+16;}
   else if(i%3==1){v.funcref_begin=refs.data();v.funcref_end=refs.data()+16;}
   else{v.externref_begin=external.data();v.externref_end=external.data()+16;}
  }
  auto retained=s::load_wasm_element_segment_payload(values[0]);
  std::latch start{5};std::array<std::thread,4> readers{};std::array<unsigned,4> counts{};
  for(unsigned worker=0;worker<4;++worker)readers[worker]=std::thread([&,worker]
  {
   start.count_down();start.wait();
   for(unsigned pass=0;pass<8;++pass)for(unsigned i=0;i<513;++i)
   {
    auto index=(i*17+worker*31)%513;auto p=s::load_wasm_element_segment_payload(values[index]);
    CHECK((p.funcidx_begin==nullptr)==(p.funcidx_end==nullptr));
    CHECK((p.funcref_begin==nullptr)==(p.funcref_end==nullptr));
    CHECK((p.externref_begin==nullptr)==(p.externref_end==nullptr));
    CHECK(unsigned(p.funcidx_begin!=nullptr)+unsigned(p.funcref_begin!=nullptr)+unsigned(p.externref_begin!=nullptr)<=1);
    if(p.funcidx_begin){CHECK(index%3==0&&p.funcidx_begin==indices.data()&&p.funcidx_end==indices.data()+16);for(unsigned n=0;n<16;++n)CHECK(p.funcidx_begin[n]==n*7+3);}
    if(p.funcref_begin){CHECK(index%3==1&&p.funcref_begin==refs.data()&&p.funcref_end==refs.data()+16);}
    if(p.externref_begin){CHECK(index%3==2&&p.externref_begin==external.data()&&p.externref_end==external.data()+16);for(unsigned n=0;n<16;++n)CHECK(p.externref_begin[n]==&indices[n]);}
    ++counts[worker];if(i%16==0)std::this_thread::yield();
   }
  });
  start.count_down();start.wait();
  for(auto& v:values){s::drop_wasm_element_segment_payload(v);s::drop_wasm_element_segment_payload(v);std::this_thread::yield();}
  for(auto& reader:readers)reader.join();
  for(unsigned i=0;i<513;++i)
  {
   auto const& v=values[i];auto p=s::load_wasm_element_segment_payload(v);
   CHECK(s::wasm_element_segment_is_dropped(v)&&!p.funcidx_begin&&!p.funcidx_end&&!p.funcref_begin&&!p.funcref_end&&!p.externref_begin&&!p.externref_end);
   if(i%3==0)CHECK(v.funcidx_begin==indices.data()&&v.funcidx_end==indices.data()+16);
   else if(i%3==1)CHECK(v.funcref_begin==refs.data()&&v.funcref_end==refs.data()+16);
   else CHECK(v.externref_begin==external.data()&&v.externref_end==external.data()+16);
  }
  CHECK(retained.funcidx_begin==indices.data()&&retained.funcidx_end==indices.data()+16);
  for(unsigned i=0;i<16;++i)CHECK(retained.funcidx_begin[i]==i*7+3);
  for(auto n:counts)snapshots+=n;
 }
 std::printf("PASS elem.drop snapshots: %u concurrent reads; three payload forms, atomic empty/live view, retained lifetime, idempotence\n",snapshots);
}
