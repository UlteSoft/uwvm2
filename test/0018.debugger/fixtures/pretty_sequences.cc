// Real C++20 producer fixture v3. Compile/run only in the authorized SSH Linux
// 64 GiB cgroup. Record exact compiler version, target, flags and source hash.
// Clang/WASI-libc++ and native GCC/libstdc++ have different ABI profiles; a
// successful synthetic schema test never proves this producer is supported.
#include <bitset>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <span>
#include <vector>
#include <unordered_map>
extern "C" __attribute__((used)) char const pretty_sequences_fixture_identity[]="uwvm-dbg-pretty-sequences-v3;c++20;deque,forward_list,list,map,unordered_map,vector_bool,bitset,static_span";
extern "C" __attribute__((used)) char const pretty_sequences_compiler_identity[]=
#if defined(__clang__)
    "clang " __clang_version__;
#else
    "gcc " __VERSION__;
#endif
::std::deque<int> pretty_deque{};
::std::deque<int>::iterator pretty_deque_iterator{};
::std::forward_list<int> pretty_forward{};
::std::list<int> pretty_list{};
::std::map<int,int> pretty_tree{};
::std::unordered_map<int,int> pretty_hash{};
::std::vector<bool> pretty_packed{};
::std::bitset<130u> pretty_bitset{};
int pretty_span_storage[4u]{7,8,9,10};
::std::span<int,4u> pretty_static_span{pretty_span_storage};
static volatile unsigned pretty_sequences_phase{};
extern "C" __attribute__((noinline)) void pretty_sequences_ready(unsigned phase) noexcept
{ pretty_sequences_phase=phase; }
int main()
{
    for(int i{};i!=2300;++i) { pretty_deque.push_back(i); }
    for(unsigned i{};i!=1020u;++i) { pretty_deque.pop_front(); }
    // Keep iterator metadata for producer inspection, while printer ABI proof
    // comes only from this deque type's actual __block_size static constant.
    // 2300 values/pop1020 crosses the normal libc++ int block size1024 as well
    // as GNU's int block size128. Inspect indices3/4 and1027/1028 at the marker.
    pretty_deque_iterator=pretty_deque.begin();
    pretty_forward={11,22,33};pretty_packed.resize(137u);
    pretty_list={11,22,33};pretty_tree={{1,10},{2,20},{3,30}};pretty_hash={{1,10},{2,20},{3,30}};
    for(unsigned i{};i!=137u;++i) { pretty_packed[i]=i%3u==1u; }
    pretty_bitset.set(0u);pretty_bitset.set(64u);pretty_bitset.set(129u);
    // Break here or at the marker; global owning objects have genuine producer
    // DIEs and can be queried from the marker frame without inferred selectors.
    pretty_sequences_ready(1u);
    return pretty_deque.size()==1280u && pretty_deque.front()==1020 && pretty_deque.back()==2299 && pretty_deque[3u]==1023 && pretty_deque[4u]==1024 &&
        pretty_deque[1027u]==2047 && pretty_deque[1028u]==2048 && *pretty_deque_iterator==1020 &&
        pretty_list.front()==11 && pretty_tree.at(2)==20 && pretty_hash.at(3)==30 &&
        pretty_forward.front()==11 && pretty_packed[1u] && !pretty_packed[0u] && pretty_bitset.test(129u) &&
        pretty_static_span[3u]==10 ? 0 : 1;
}
