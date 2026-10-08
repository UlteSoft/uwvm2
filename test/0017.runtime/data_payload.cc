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
namespace storage=uwvm2::uwvm::runtime::storage;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL data payload %u: %s\n",__LINE__,#x);std::abort();}}while(false)
static_assert(std::is_trivially_copyable_v<storage::wasm_data_storage_t>);
constexpr bool constant_drop()
{
 storage::wasm_data_storage_t value{};
 if(storage::wasm_data_segment_is_dropped(value)){return false;}
 storage::drop_wasm_data_segment_payload(value);
 auto payload=storage::load_wasm_data_segment_payload(value);
 return storage::wasm_data_segment_is_dropped(value)&&payload.byte_begin==nullptr&&payload.byte_end==nullptr;
}
static_assert(constant_drop());
int main()
{
 std::array<std::byte,16> bytes{};for(unsigned i=0;i<16;++i){bytes[i]=std::byte(i*7+3);}
 unsigned snapshots{};
 for(unsigned round=0;round<32;++round)
 {
  std::array<storage::wasm_data_storage_t,512> data{};
  for(auto& value:data){value.byte_begin=bytes.data();value.byte_end=bytes.data()+bytes.size();value.kind=storage::wasm_data_segment_kind::passive;}
  auto retained=storage::load_wasm_data_segment_payload(data[0]);
  std::latch start{5};std::array<std::thread,4> readers{};std::array<unsigned,4> counts{};
  for(unsigned worker=0;worker<4;++worker)
  {
   readers[worker]=std::thread([&,worker]
   {
    start.count_down();start.wait();
    for(unsigned pass=0;pass<8;++pass)for(unsigned i=0;i<512;++i)
    {
     auto payload=storage::load_wasm_data_segment_payload(data[(i*17+worker*31)%512]);
     CHECK((payload.byte_begin==nullptr)==(payload.byte_end==nullptr));
     if(payload.byte_begin!=nullptr)
     {
      CHECK(payload.byte_begin==bytes.data()&&payload.byte_end==bytes.data()+16);
      for(unsigned b=0;b<16;++b){CHECK(payload.byte_begin[b]==std::byte(b*7+3));}
     }
     ++counts[worker];if(i%16==0){std::this_thread::yield();}
    }
   });
  }
  start.count_down();start.wait();
  for(auto& value:data){storage::drop_wasm_data_segment_payload(value);storage::drop_wasm_data_segment_payload(value);std::this_thread::yield();}
  for(auto& reader:readers){reader.join();}
  for(auto const& value:data)
  {
   auto payload=storage::load_wasm_data_segment_payload(value);
   CHECK(payload.byte_begin==nullptr&&payload.byte_end==nullptr&&storage::wasm_data_segment_is_dropped(value));
   CHECK(value.byte_begin==bytes.data()&&value.byte_end==bytes.data()+16);
  }
  CHECK(retained.byte_begin==bytes.data()&&retained.byte_end==bytes.data()+16);
  for(unsigned b=0;b<16;++b){CHECK(retained.byte_begin[b]==bytes[b]);}
  for(auto count:counts){snapshots+=count;}
 }
 std::printf("PASS data.drop snapshots: %u concurrent reads; consistent empty/live payload, retained lifetime, idempotent drop, constexpr and trivial relocation\n",snapshots);
}
