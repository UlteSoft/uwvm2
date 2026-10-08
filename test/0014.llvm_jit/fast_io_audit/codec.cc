// Fixed wire images, malformed input and unaligned reads; also built against the
// pre-change source tree so stdout fingerprints compare the actual old/new codecs.
#include <uwvm2/runtime/llvm_jit_cache/compress.h>
#include <uwvm2/runtime/compiler/shared/strict_float.h>
#include "immediate.h" // runner extracts the unchanged production function body
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
namespace cache = uwvm2::runtime::llvm_jit_cache;
using bytes = uwvm2::utils::container::vector<std::byte>;
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
static std::uint64_t fingerprint=14695981039346656037ull;
static void hash(std::byte const* data,std::size_t size) {
    for(std::size_t i=0;i<size;++i) fingerprint=(fingerprint^std::to_integer<unsigned char>(data[i]))*1099511628211ull;
}
static bool equal(bytes const& a,bytes const& b) {
    return a.size()==b.size() && (a.empty() || std::memcmp(a.data(),b.data(),a.size())==0);
}
static constexpr bool constexpr_immediate() {
    std::byte const data[]{std::byte{0x01},std::byte{0x23},std::byte{0xc0},std::byte{0x7f},std::byte{0x80}};
    auto cursor=data;std::uint32_t value{};
    return parse_wasm_little_endian_immediate(cursor,data+4,value) && value==0x7fc02301u && cursor==data+4 &&
           !parse_wasm_little_endian_immediate(cursor,data+5,value) && cursor==data+4 && value==0x7fc02301u;
}
static_assert(constexpr_immediate());
static_assert([] {
    std::byte data[9]{};
    uwvm2::utils::hash::details::xxh_writeLE64(data+1,0x0123456789abcdefull);
    return data[1]==std::byte{0xef} && data[8]==std::byte{1} &&
           uwvm2::utils::hash::details::xxh_readLE16(data+1)==0xcdefu &&
           uwvm2::utils::hash::details::xxh_readLE32(data+1)==0x89abcdefu &&
           uwvm2::utils::hash::details::xxh_readLE64(data+1)==0x0123456789abcdefull;
}());
static cache::cache_fixed_header header() {
    return {{std::byte{'U'},std::byte{'W'},std::byte{'V'},std::byte{'M'},std::byte{1},std::byte{2},std::byte{3},std::byte{4}},
            0x01020304u,64u,0xffffffffu,0x80000000u,0x0123456789abcdefull,0xffffffffffffffffull,
            0x8000000000000000ull,0x1122334455667788ull,0};
}
static void correctness() {
    auto original=header();auto encoded=cache::serialize_fixed_header(original);
    unsigned char const expected[]{'U','W','V','M',1,2,3,4,4,3,2,1,64,0,0,0,255,255,255,255,0,0,0,128,
        239,205,171,137,103,69,35,1,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,128,
        136,119,102,85,68,51,34,17,0,0,0,0,0,0,0,0};
    CHECK(encoded.size()==sizeof(expected));CHECK(std::memcmp(encoded.data(),expected,sizeof(expected))==0);
    hash(encoded.data(),encoded.size());
    for(unsigned align=0;align!=16;++align) {
        std::byte buffer[96]{};std::memcpy(buffer+align,encoded.data(),encoded.size());
        cache::cache_fixed_header decoded{};
        CHECK(cache::parse_fixed_header(buffer+align,buffer+align+encoded.size(),decoded));
        CHECK(equal(cache::serialize_fixed_header(decoded),encoded));
        for(unsigned length=0;length!=64;++length) {
            decoded=original;decoded.version=42;
            CHECK(!cache::parse_fixed_header(buffer+align,buffer+align+length,decoded));
            CHECK(decoded.version==42);
        }
        for(unsigned width:{4u,8u}) for(unsigned length=0;length!=width;++length) {
            auto cursor=buffer+align;std::byte const* input=cursor;
            std::uint32_t u32=42;std::uint64_t u64=42;
            CHECK(!(width==4 ? cache::details::read_u32_le(input,input+length,u32) : cache::details::read_u64_le(input,input+length,u64)));
            CHECK(input==cursor && u32==42 && u64==42);
        }
        // Include signed zero, infinity, quiet and signaling NaNs as raw integers.
        for(auto bits:{0ull,0x8000000000000000ull,0x7ff0000000000000ull,0x7ff0000000000001ull,0x7ff8123456789abcull,~0ull}) {
            bytes wire;cache::details::append_u64_le(wire,bits);std::memcpy(buffer+align,wire.data(),8);
            std::byte const* cursor=buffer+align;std::uint64_t actual{};
            CHECK(parse_wasm_little_endian_immediate(cursor,buffer+align+8,actual) && actual==bits && cursor==buffer+align+8);
        }
    }
    // All 65536 token images exercise both length and distance portions.
    for(unsigned token=0;token!=65536;++token) {
        bytes wire;cache::details::append_lzss_token(wire,(token&4095u)+1u,(token>>12u)+3u);
        CHECK(wire.size()==2 && std::to_integer<unsigned>(wire[0])==(token&255u) && std::to_integer<unsigned>(wire[1])==(token>>8u));
        hash(wire.data(),wire.size());
    }
    std::uint64_t state=0x9e3779b97f4a7c15ull;
    for(unsigned pattern=0;pattern!=4;++pattern) for(std::size_t size:{0u,1u,2u,3u,4u,15u,16u,255u,256u,4096u,8192u}) {
        std::vector<std::byte> input(size+1);
        for(std::size_t i=0;i<size;++i) { state^=state<<13;state^=state>>7;state^=state<<17;
            input[i]=std::byte(pattern==0 ? 0 : pattern==1 ? i%17 : pattern==2 ? i%256 : state&255); }
        for(auto seed:{0ull,1ull,0x0123456789abcdefull,~0ull}) {
            auto result=uwvm2::utils::hash::xxh3_64bits(input.data(),size,seed);
            bytes wire;cache::details::append_u64_le(wire,result);hash(wire.data(),wire.size());++checks;
        }
        for(bool native:{false,true}) {
            auto compressed=native ? cache::compress_native_lz(input.data(),size) : cache::compress_lzss(input.data(),size);
            hash(compressed.data(),compressed.size());
            std::byte empty{};auto first=compressed.empty()?&empty:compressed.data();bytes decoded;
            auto decode=[&](std::size_t length,std::size_t expected) {
                return native ? cache::decompress_native_lz(first,first+length,expected,decoded) : cache::decompress_lzss(first,first+length,expected,decoded); };
            CHECK(decode(compressed.size(),size));CHECK(decoded.size()==size && (!size || std::memcmp(decoded.data(),input.data(),size)==0));
            if(!compressed.empty()) CHECK(!decode(compressed.size()-1,size));
        }
    }
    // Deterministic malformed corpus: compare acceptance and partial output to the
    // pre-change executable, in addition to ASan/UBSan checking every execution.
    for(unsigned i=0;i!=4096;++i) {
        std::byte input[33];for(auto& byte:input) {state^=state<<13;state^=state>>7;state^=state<<17;byte=std::byte(state&255);}
        auto length=i%33;auto expected=i%129;
        for(bool native:{false,true}) {bytes output;bool ok=native ? cache::decompress_native_lz(input,input+length,expected,output) : cache::decompress_lzss(input,input+length,expected,output);
            auto flag=std::byte(ok);hash(&flag,1);hash(output.data(),output.size());++checks;}
    }
    std::printf("PASS checks=%u fingerprint=%016llx endian=%s pointer=%zu\n",checks,(unsigned long long)fingerprint,
        std::endian::native==std::endian::little?"little":"big",sizeof(void*)*8);
}
static volatile std::uint64_t sink;
template<class T> static void escape(T const& value) { asm volatile("" : : "g"(std::addressof(value)) : "memory"); }
template<class F> static void bench(char const* name,std::size_t iterations,F&& f) {
    for(unsigned round=0;round!=9;++round) {
        auto begin=std::chrono::steady_clock::now();std::uint64_t sum{};
        timespec cpu_begin{},cpu_end{};CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu_begin)==0);
        for(std::size_t i=0;i<iterations;++i) {sum+=f(i);asm volatile("" : "+r"(sum) : : "memory");}
        CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu_end)==0);
        auto elapsed=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-begin).count();sink=sum;
        auto cpu_elapsed=(cpu_end.tv_sec-cpu_begin.tv_sec)*1e9+(cpu_end.tv_nsec-cpu_begin.tv_nsec);
        std::printf("BENCH %s %.3f %llu\n",name,cpu_elapsed/iterations,(unsigned long long)sum);
        std::printf("WALL %s %.3f\n",name,elapsed/iterations);
    }
}
int main(int argc,char**) {
    correctness();if(argc==1)return 0;
    auto h=header();auto wire=cache::serialize_fixed_header(h);
    bench("header_encode",300000,[&](std::size_t i){h.payload_size=i;auto out=cache::serialize_fixed_header(h);escape(out);return std::to_integer<unsigned>(out[32]);});
    bench("header_decode",1000000,[&](std::size_t i){wire[32]=std::byte(i);cache::cache_fixed_header out{};CHECK(cache::parse_fixed_header(wire.data(),wire.data()+wire.size(),out));escape(out);return out.payload_size;});
    std::vector<std::byte> data(16384);for(std::size_t i=0;i<data.size();++i)data[i]=std::byte((i*31+i/97)%256);
    bench("native_lz_encode_16KiB",1000,[&](std::size_t i){data[0]=std::byte(i);auto out=cache::compress_native_lz(data.data(),data.size());escape(out);return out.size();});
    auto packed=cache::compress_native_lz(data.data(),data.size());
    bench("native_lz_decode_16KiB",1500,[&](std::size_t){bytes out;CHECK(cache::decompress_native_lz(packed.data(),packed.data()+packed.size(),data.size(),out));escape(out);return out.size();});
    bench("append_bytes_16KiB",10000,[&](std::size_t){bytes out;out.reserve(data.size());cache::details::append_bytes(out,data.data(),data.data()+data.size());escape(out);return std::to_integer<unsigned>(out.back_unchecked());});
}
