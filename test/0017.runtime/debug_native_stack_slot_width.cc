// Private reader boundary fixture on its actual registered thread stack.
// This does not issue a Wasm capture, debugger memory or finish capability.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_debug_native_stack.h>
#include <array>
#include <cstring>
#include <fast_io.h>
namespace stack = ::uwvm2::runtime::lib::details::debug_native_stack;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native stack slot failure line=", __LINE__); return 1; } } while(false)
int check_current_stack(bool require_high = false)
{
    constexpr auto width{sizeof(::std::uintptr_t)};
    static_assert(width==4u || width==8u);
    alignas(8) ::std::array<::std::uint32_t,4u> source{0x12345678u,0x87654321u,0xaabbccddu,0xdeadc0deu};
    auto const address{reinterpret_cast<::std::uintptr_t>(source.data())};
    CHECK(!require_high || address>INT32_MAX);
    auto const tid{static_cast<::std::uint_least64_t>(::fast_io::system_call<__NR_gettid,long>())};
    ::std::array<::std::byte,16u> output{};
    stack::owner retired{};
    {
        stack::scope actual{}; auto const owner{actual.pin()}; CHECK(owner);
        CHECK(owner->contains_frame(tid,address,address+width));
        output.fill(::std::byte{0xcc});
        auto read=[&](auto thread,auto sp,auto cfa,auto slot,auto buffer) noexcept
        {
            if constexpr(width==4u) { return owner->copy_word32(thread,sp,cfa,slot,buffer); }
            else { return owner->copy_word(thread,sp,cfa,slot,buffer); }
        };
        CHECK(read(tid,address,address+width,address,output.data()));
        CHECK(::std::memcmp(output.data(),source.data(),width)==0);
        for(auto n{width};n!=output.size();++n) { CHECK(output[n]==::std::byte{0xcc}); }
        output.fill(::std::byte{0xcc});
        CHECK(!read(tid,address,address+width-1u,address,output.data()));
        CHECK(!read(tid,address,address+width,address+1u,output.data()));
        CHECK(!read(tid,address,address+width,address-1u,output.data()));
        CHECK(!read(tid+1u,address,address+width,address,output.data()));
        CHECK(!read(tid,1u,1u+width,1u,output.data()));
        CHECK(!read(tid,address,address+width,address,static_cast<::std::byte*>(nullptr)));
        if constexpr(width==4u) { CHECK(!owner->copy_word(tid,address,address+16u,address,output.data())); }
        else { CHECK(!owner->copy_word32(tid,address,address+16u,address,output.data())); }
        for(auto value:output) { CHECK(value==::std::byte{0xcc}); }
        retired=owner;
    }
    CHECK(!retired->contains_frame(tid,address,address+width));
    CHECK(!retired->copy_word(tid,address,address+16u,address,output.data()));
    CHECK(!retired->copy_word32(tid,address,address+16u,address,output.data()));
    ::fast_io::io::println("native private stack slot: PASS pointer-width=",width,
        " adjacent-bytes-written=0 wrong-width-refused=true stale-owner-refused=true live-Wasm-caller-qualified=false");
    return 0;
}
int main()
{
    CHECK(check_current_stack()==0);
#if (defined(__i386__) || (defined(__arm__) && defined(__ARM_EABI__) && defined(__ARMEL__))) && __SIZEOF_POINTER__ == 4
    // Exercise the target split/aligned pread64 offset ABI above INT32_MAX on a genuine
    // registered pthread stack. A heap mapping cannot stand in for that stack.
    constexpr ::std::size_t bytes{1024u*1024u};
    auto const memory{::uwvm2::runtime::lib::posix_abi::mmap_noexcept(
        reinterpret_cast<void*>(::std::uintptr_t{0xc0000000u}),bytes,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)};
    CHECK(memory!=MAP_FAILED && reinterpret_cast<::std::uintptr_t>(memory)>INT32_MAX);
    ::pthread_attr_t attributes{}; ::pthread_t thread{}; int status{2};
    CHECK(::fast_io::noexcept_call(::pthread_attr_init,&attributes)==0);
    CHECK(::fast_io::noexcept_call(::pthread_attr_setstack,&attributes,memory,bytes)==0);
    CHECK(::fast_io::noexcept_call(::pthread_create,&thread,&attributes,
        +[](void* value) noexcept -> void* { *static_cast<int*>(value)=check_current_stack(true); return nullptr; },&status)==0);
    CHECK(::fast_io::noexcept_call(::pthread_attr_destroy,&attributes)==0);
    CHECK(::fast_io::noexcept_call(::pthread_join,thread,static_cast<void**>(nullptr))==0);
    CHECK(::uwvm2::runtime::lib::posix_abi::munmap_noexcept(memory,bytes)==0 && status==0);
    ::fast_io::io::println("32-bit actual high pthread stack: PASS address-above-INT32_MAX=true");
#endif
}
