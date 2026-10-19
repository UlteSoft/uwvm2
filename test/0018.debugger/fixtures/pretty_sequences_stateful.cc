// Real C++20 producer fixture v1. Compile/run only inside the authorized SSH
// Linux 64 GiB cgroup. Record compiler, target, flags and this source hash.
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
extern "C" __attribute__((used)) char const pretty_stateful_fixture_identity[]="uwvm-dbg-pretty-stateful-v2;c++20;list,map,unordered_map,vector_bool,vector,string";
extern "C" __attribute__((used)) char const pretty_stateful_compiler_identity[]=
#if defined(__clang__)
    "clang " __clang_version__;
#else
    "gcc " __VERSION__;
#endif
template<class T> struct pretty_stateful_allocator
{
    using value_type=T;
    unsigned state{17u};
    pretty_stateful_allocator()=default;
    template<class U> pretty_stateful_allocator(pretty_stateful_allocator<U> const& other) noexcept:state(other.state) {}
    T* allocate(::std::size_t count) { return ::std::allocator<T>{}.allocate(count); }
    void deallocate(T* pointer,::std::size_t count) { ::std::allocator<T>{}.deallocate(pointer,count); }
    template<class U> bool operator==(pretty_stateful_allocator<U> const& other) const noexcept { return state==other.state; }
};
struct pretty_stateful_compare
{
    int direction{1};
    bool operator()(int left,int right) const noexcept { return direction>0 ? left<right : right<left; }
};
struct pretty_stateful_hash
{
    ::std::size_t salt{29u};
    ::std::size_t operator()(int value) const noexcept { return ::std::hash<int>{}(value)^salt; }
};
using pretty_key_value=::std::pair<int const,int>;
::std::list<int,pretty_stateful_allocator<int>> pretty_stateful_list{};
::std::map<int,int,pretty_stateful_compare,pretty_stateful_allocator<pretty_key_value>> pretty_stateful_tree{};
::std::unordered_map<int,int,pretty_stateful_hash,::std::equal_to<int>,pretty_stateful_allocator<pretty_key_value>> pretty_stateful_hash_table{};
::std::vector<bool,pretty_stateful_allocator<bool>> pretty_stateful_packed{};
::std::vector<int,pretty_stateful_allocator<int>> pretty_stateful_vector{};
using pretty_stateful_string=::std::basic_string<char,::std::char_traits<char>,pretty_stateful_allocator<char>>;
pretty_stateful_string pretty_stateful_short_text{},pretty_stateful_long_text{};
static volatile unsigned pretty_stateful_phase{};
extern "C" __attribute__((noinline)) void pretty_stateful_ready(unsigned phase) noexcept { pretty_stateful_phase=phase; }
int main()
{
    pretty_stateful_list={11,22,33};pretty_stateful_tree={{1,10},{2,20},{3,30}};
    pretty_stateful_hash_table={{1,10},{2,20},{3,30}};pretty_stateful_packed.resize(137u);
    pretty_stateful_vector={41,42,43};pretty_stateful_short_text="stateful";pretty_stateful_long_text.assign(48u,'x');
    for(unsigned i{};i!=137u;++i) { pretty_stateful_packed[i]=i%3u==1u; }
    // Real LLVM18 compressed pairs have two nonempty carriers here. The
    // printer must bind ordinal0 through template metadata, preserving generic
    // ambiguous __value_ lookup rejection. Map elements must expose the pair.
    pretty_stateful_ready(1u);
    return pretty_stateful_list.front()==11 && pretty_stateful_tree.at(2)==20 &&
        pretty_stateful_hash_table.at(3)==30 && pretty_stateful_packed[1u] && !pretty_stateful_packed[0u] &&
        pretty_stateful_vector[1u]==42 && pretty_stateful_short_text=="stateful" &&
        pretty_stateful_long_text.size()==48u && pretty_stateful_long_text.front()=='x' && pretty_stateful_long_text.back()=='x' ? 0 : 1;
}
