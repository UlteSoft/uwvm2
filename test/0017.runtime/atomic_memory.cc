#include <uwvm2/runtime/compiler/shared/wasm_threads.h>
#include <array>
#include <algorithm>
#include <thread>
#include <latch>
#include <vector>
#include <cstdio>
#include <cstdlib>
#define CHECK(condition) do { if(!(condition)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#condition); std::abort(); } } while(false)
namespace atom = uwvm2::runtime::compiler::shared::wasm_threads;
using op = atom::atomic_rmw_operation;
template<std::size_t Bytes>
void check()
{
    alignas(8) std::array<std::byte,16> memory{};
    constexpr std::uint64_t mask{~std::uint64_t{} >> (64-Bytes*8)};
    auto read=[&] {std::uint64_t value{};for(unsigned i{};i!=Bytes;++i) {value |= std::uint64_t(std::to_integer<unsigned>(memory[i])) << (i*8);}return value;};
    auto write=[&](std::uint64_t value) {memory.fill(std::byte{0xa5});for(unsigned i{};i!=Bytes;++i) {memory[i]=std::byte((value>>(i*8))&255);}};
    auto operation=[&]<op Operation>(std::uint64_t initial,std::uint64_t operand,std::uint64_t expected)
    {
        write(initial); auto old=initial&mask;auto value=operand&mask;std::uint64_t next{};
        if constexpr(Operation==op::add) {next=(old+value)&mask;}
        else if constexpr(Operation==op::sub) {next=(old-value)&mask;}
        else if constexpr(Operation==op::and_) {next=old&value;}
        else if constexpr(Operation==op::or_) {next=old|value;}
        else if constexpr(Operation==op::xor_) {next=old^value;}
        else if constexpr(Operation==op::exchange) {next=value;}
        else {next=old==(expected&mask) ? value : old;}
        CHECK((atom::atomic_rmw_le<Operation,Bytes>(memory.data(),operand,expected)==old));
        CHECK(read()==next);
        for(unsigned i=Bytes;i!=memory.size();++i) {CHECK(memory[i]==std::byte{0xa5});}
    };
    std::uint64_t random{0x81726354a5b6c7d8};
    for(unsigned i{};i!=128;++i)
    {
        auto initial=random;random=random*6364136223846793005ull+1;auto operand=random;
        write(initial);CHECK(atom::atomic_load_le<Bytes>(memory.data())==(initial&mask));
        atom::atomic_store_le<Bytes>(memory.data(),operand);CHECK(read()==(operand&mask));
        operation.template operator()<op::add>(initial,operand,0);
        operation.template operator()<op::sub>(initial,operand,0);
        operation.template operator()<op::and_>(initial,operand,0);
        operation.template operator()<op::or_>(initial,operand,0);
        operation.template operator()<op::xor_>(initial,operand,0);
        operation.template operator()<op::exchange>(initial,operand,0);
        operation.template operator()<op::compare_exchange>(initial,operand,initial^~mask); // high expected bits ignored
        operation.template operator()<op::compare_exchange>(initial,operand,initial^1); // mismatch returns old without write
    }
    write(0);
    constexpr unsigned workers=4, calls=512;
    std::array<std::array<std::uint64_t,calls>,workers> observed{};
    std::latch start{workers};std::vector<std::thread> threads{};
    for(unsigned worker{};worker!=workers;++worker)
    {threads.emplace_back([&,worker]{start.count_down();start.wait();for(auto& value:observed[worker]) {value=atom::atomic_rmw_le<op::add,Bytes>(memory.data(),1);}});}
    for(auto& thread:threads) {thread.join();}
    CHECK(read()==((workers*calls)&mask));
    std::vector<std::uint64_t> actual{},expected{};
    for(auto const& group:observed) {actual.insert(actual.end(),group.begin(),group.end());}
    for(unsigned i{};i!=workers*calls;++i) {expected.push_back(i&mask);}
    std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());CHECK(actual==expected);
}
int main()
{
    check<1>();check<2>();check<4>();check<8>();
    std::puts("PASS atomic raw memory: all widths and RMW families, endian bytes, wrapped CAS, unchanged neighbors, concurrent fetch-add histories");
}
