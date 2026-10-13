// Directly include the public helper: verifies its header owns every macro/dependency it needs.
#include <uwvm2/validation/standard/wasm3/memory_validation.h>
#include <array>
#include <cstdio>
#include <cstdlib>
namespace mm = uwvm2::validation::standard::wasm3;
static void require(bool value) { if(!value) { std::abort(); } }
int main()
{
    std::array<unsigned char, 5> const encoded{0x42,0x81,0x01,0x80,0x01}; // align=2, memory=129, offset=128
    auto const begin{reinterpret_cast<std::byte const*>(encoded.data())};
    for(std::size_t length{}; length != encoded.size(); ++length)
    {
        auto cursor{begin};auto arg{mm::scan_memory_argument(cursor,begin+length,true)};
        require(arg.error != mm::memory_immediate_error::ok && cursor==begin);
    }
    auto cursor{begin};auto arg{mm::scan_memory_argument(cursor,begin+encoded.size(),true)};
    require(arg.error==mm::memory_immediate_error::ok && arg.memory_index==129 && arg.alignment==2 && arg.offset==128 && cursor==begin+encoded.size());
    cursor=begin;arg=mm::scan_memory_argument(cursor,begin+encoded.size(),false);
    require(arg.error==mm::memory_immediate_error::feature_disabled && cursor==begin);
    // Every possible flag byte, including truncation of a continued u32 and the explicit-index bit.
    for(unsigned flags{};flags!=256;++flags)
    {
        std::byte bytes[]{std::byte(flags),std::byte{},std::byte{},std::byte{}};
        auto p{bytes+0};std::byte const* q{p};auto result{mm::scan_memory_argument(q,bytes+sizeof(bytes),true)};
        if(flags<128) { require(result.error==mm::memory_immediate_error::ok && result.alignment==(flags&63)); }
        else if(result.error==mm::memory_immediate_error::ok) { require(result.alignment<128); }
        q=p;auto old{mm::scan_memory_argument(q,bytes+sizeof(bytes),false)};
        if(flags>=64 && flags<128) { require(old.error==mm::memory_immediate_error::feature_disabled && q==p); }
    }
    // Full u32 index range and noncanonical-but-valid encodings; overflow and truncation never commit.
    for(unsigned length{1};length!=6;++length)
    {
        std::byte bytes[6]{};
        for(unsigned i{};i+1<length;++i) { bytes[i]=std::byte{0x80}; }
        std::byte const* p{bytes};std::uint_least32_t index{9};
        require(mm::scan_memory_index(p,bytes+length,true,index)==mm::memory_immediate_error::ok && index==0 && p==bytes+length);
        p=bytes;index=9;
        require(mm::scan_memory_index(p,bytes+length-1,true,index)==mm::memory_immediate_error::index && p==bytes);
    }
    std::byte bad[]{std::byte{0xff},std::byte{0xff},std::byte{0xff},std::byte{0xff},std::byte{0x1f}};
    std::byte const* p{bad};std::uint_least32_t index{};
    require(mm::scan_memory_index(p,bad+5,true,index)==mm::memory_immediate_error::index && p==bad);
    std::puts("PASS Core 3 memory immediates: every flag byte, every truncation, padded indices and u32 overflow");
}
