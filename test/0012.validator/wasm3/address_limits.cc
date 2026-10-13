#include <uwvm2/validation/standard/wasm3/address_limits.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string_view>
#include <sys/mman.h>
#include <unistd.h>
namespace
{
    namespace w3=uwvm2::validation::standard::wasm3;
    using kind=w3::address_limits_kind;
    using addr=w3::storage_address_type;
    using error=w3::address_limits_error;
    using bytes=std::vector<std::byte>;
    unsigned checks{};
    void require(bool value) { if(!value) { std::fprintf(stderr,"failed check %u\n",checks); std::abort(); } ++checks; }
    void leb(bytes& out, std::uint64_t value, unsigned padding=0)
    {
        do
        {
            auto byte=static_cast<unsigned>(value & 127u); value >>= 7u;
            if(value || padding) { byte |= 128u; }
            out.push_back(static_cast<std::byte>(byte));
        } while(value);
        while(padding) { out.push_back(static_cast<std::byte>(--padding ? 128u : 0u)); }
    }
    auto scan(bytes const& encoded, kind k, bool enabled=true, bool threads=true, bool wide=true,
              error expected=error::ok)
    {
        // Place the input immediately before an inaccessible page. Every truncated
        // prefix, including an empty one, must be handled without overreading it.
        auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto allocation=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
        require(allocation!=MAP_FAILED);
        require(mprotect(allocation+page,page,PROT_NONE)==0);
        auto start=allocation+page-encoded.size();
        if(!encoded.empty()) { std::memcpy(start,encoded.data(),encoded.size()); }
        auto cursor=static_cast<std::byte const*>(start);
        auto end=static_cast<std::byte const*>(allocation+page);
        auto result=w3::scan_address_limits(cursor,end,k,enabled,threads,wide);
        require(result.error==expected);
        require(cursor==(expected==error::ok ? end : start));
        require(result.error_offset<=encoded.size());
        require(munmap(allocation,page*2)==0);
        return result;
    }
}
int main(int argc, char** argv)
{
    if(argc==3)
    {
        auto k=std::string_view(argv[1])=="memory"?kind::memory:kind::table;
        std::string_view hex{argv[2]};bytes input{};
        if(hex.size()%2u) { return 2; }
        auto digit=[](char c)->int { if(c>='0'&&c<='9') { return c-'0'; } if(c>='a'&&c<='f') { return c-'a'+10; } return -1; };
        for(std::size_t i{};i<hex.size();i+=2u)
        {
            auto hi=digit(hex[i]),lo=digit(hex[i+1u]);if(hi<0||lo<0) { return 2; }
            input.push_back(static_cast<std::byte>(hi*16+lo));
        }
        std::byte empty{};auto begin=input.empty()?&empty:input.data();
        auto cursor=static_cast<std::byte const*>(begin);
        auto result=w3::scan_address_limits(cursor,begin+input.size(),k,true,true);
        bool valid=result.error==error::ok && cursor==begin+input.size();
        std::printf("%s\n",valid?"valid":"invalid");return valid?0:1;
    }
    if(argc!=1) { return 2; }
    for(auto k:{kind::memory,kind::table}) for(auto at:{addr::i32,addr::i64})
    {
        auto maximum=w3::address_limit_maximum(k,at);
        for(bool bounded:{false,true}) for(auto minimum:{std::uint64_t{},std::uint64_t{1},std::uint64_t{maximum}})
        {
            bytes encoding{static_cast<std::byte>((at==addr::i64 ? 4u:0u)|(bounded?1u:0u))};
            leb(encoding,minimum); if(bounded) { leb(encoding,maximum); }
            auto r=scan(encoding,k);
            require(r.limits.address_type==at && r.limits.min==minimum && r.limits.present_max==bounded);
            if(bounded) { require(r.limits.max==maximum); }
            if(at==addr::i64) { scan(encoding,k,false,true,true,error::address64_disabled); }
            for(std::size_t n{};n<encoding.size();++n)
            {
                bytes prefix(encoding.begin(),encoding.begin()+n);
                auto expected=n==0 ? error::missing_flags : error::minimum_encoding;
                bytes min_bytes{}; leb(min_bytes,minimum);
                if(n>min_bytes.size()) { expected=error::maximum_encoding; }
                scan(prefix,k,true,true,true,expected);
            }
        }
        if(maximum!=0xffff'ffff'ffff'ffffull)
        {
            for(bool max_field:{false,true})
            {
                bytes e{static_cast<std::byte>((at==addr::i64?4u:0u)|(max_field?1u:0u))};
                leb(e,max_field?0u:maximum+1u); if(max_field) { leb(e,maximum+1u); }
                scan(e,k,true,true,true,error::limit_out_of_range);
            }
        }
        bytes descending{static_cast<std::byte>((at==addr::i64?4u:0u)|1u)};
        leb(descending,2);leb(descending,1);scan(descending,k,true,true,true,error::maximum_below_minimum);
    }
    for(unsigned flag{};flag<256u;++flag)
    {
        bytes e{static_cast<std::byte>(flag)};leb(e,0);if(flag&1u) { leb(e,1); }
        for(auto k:{kind::memory,kind::table})
        {
            auto expected=flag>=8u || (k==kind::table && (flag&2u)) ? error::invalid_flags :
                ((flag&2u) && !(flag&1u)) ? error::shared_without_maximum : error::ok;
            auto r=scan(e,k,true,true,true,expected);
            if(expected==error::ok && flag&2u)
            {
                require(r.limits.shared && r.limits.present_max);
                scan(e,k,true,false,true,error::threads_disabled);
            }
        }
    }
    for(unsigned padding=1;padding!=10;++padding)
    {
        bytes e{std::byte{0}};leb(e,0,padding);
        require(scan(e,kind::memory).limits.min==0);
        scan(e,kind::memory,false,false,false,padding>=5?error::minimum_encoding:error::ok);
    }
    // 65th value bit and an unterminated/overlong 11-byte u64 must never be truncated.
    for(bool maximum:{false,true}) for(bool carry:{false,true})
    {
        bytes e{maximum?std::byte{5}:std::byte{4}};if(maximum) { leb(e,0); }
        for(unsigned i{};i!=9;++i) { e.push_back(std::byte{0x80}); }
        e.push_back(carry?std::byte{2}:std::byte{0x80}); if(!carry) { e.push_back(std::byte{}); }
        scan(e,kind::memory,true,true,true,maximum?error::maximum_encoding:error::minimum_encoding);
    }
    for(auto a:{addr::i32,addr::i64}) for(auto b:{addr::i32,addr::i64})
    {
        require(w3::copy_length_address_type(a,b)==((a==addr::i64&&b==addr::i64)?addr::i64:addr::i32));
        w3::address_limits expected{1,5,true,a,false};
        w3::address_limits actual{2,4,true,b,false};
        require(w3::address_limits_match(expected,actual)==(a==b));
        actual.shared=true;require(!w3::address_limits_match(expected,actual));actual.shared=false;
        actual.present_max=false;require(!w3::address_limits_match(expected,actual));actual.present_max=true;
        actual.max=6;require(!w3::address_limits_match(expected,actual));actual.max=4;
        actual.min=0;require(!w3::address_limits_match(expected,actual));
    }
    // Core 3 memargs retain all 64 offset bits, with and without an indexed
    // memory. The existing memory32 entry point must reject large offsets and
    // retain its bytecode field width; it may not silently truncate them.
    for(bool indexed:{false,true}) for(auto offset:{0ull,0xffff'ffffull,0x1'0000'0000ull,0xffff'ffff'ffff'ffffull})
    {
        bytes encoded{};leb(encoded,indexed?67u:3u);if(indexed) { leb(encoded,129); }leb(encoded,offset);
        auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto allocation=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
        require(allocation!=MAP_FAILED);require(mprotect(allocation+page,page,PROT_NONE)==0);
        for(std::size_t size{};size<=encoded.size();++size)
        {
            auto begin=allocation+page-size;auto end=allocation+page;
            if(size) { std::memcpy(begin,encoded.data(),size); }
            auto cursor=static_cast<std::byte const*>(begin);
            auto r=w3::scan_memory_argument64(cursor,end,indexed);
            require((r.error==w3::memory_immediate_error::ok)==(size==encoded.size()));
            require(cursor==(size==encoded.size()?end:begin));
            if(size==encoded.size())
            {
                require(r.offset==offset && r.alignment==3u && r.memory_index==(indexed?129u:0u));
                cursor=begin;
                auto narrow=w3::scan_memory_argument(cursor,end,indexed);
                require((narrow.error==w3::memory_immediate_error::ok)==(offset<=0xffff'ffffull));
                require(cursor==(offset<=0xffff'ffffull?end:begin));
                if(offset<=0xffff'ffffull) { require(narrow.offset==offset); }
                if(indexed)
                {
                    cursor=begin;auto disabled=w3::scan_memory_argument64(cursor,end,false);
                    require(disabled.error==w3::memory_immediate_error::feature_disabled && cursor==begin);
                }
            }
        }
        require(munmap(allocation,page*2)==0);
    }
    for(unsigned final:{2u,127u,128u,255u})
    {
        bytes encoded{std::byte{0}};
        for(unsigned i{};i!=9;++i) { encoded.push_back(std::byte{128}); }
        encoded.push_back(static_cast<std::byte>(final));
        auto cursor=static_cast<std::byte const*>(encoded.data());
        auto r=w3::scan_memory_argument64(cursor,encoded.data()+encoded.size(),false);
        require(r.error==w3::memory_immediate_error::offset && cursor==encoded.data());
    }
    std::printf("PASS Core 3 address limits: %u checks; 32/64-bit limits, shared flags, feature gates, import matching, mixed copies, wide memargs, protected truncation\n",checks);
}
