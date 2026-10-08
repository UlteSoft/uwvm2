// Real MC component on OWNED bytes; not proof of runtime executable ownership.
#include <uwvm2/uwvm/debugger/native_disassembly_window.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace nd = ::uwvm2::uwvm::debugger::native_disassembly;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native MC window failure line=", __LINE__); ::fast_io::fast_terminate(); } } while(false)
struct counted_mc
{
    nd::decoder actual{};
    ::std::size_t calls{};
    [[nodiscard]] explicit operator bool() const noexcept { return bool(actual); }
    nd::instruction decode(::std::uintptr_t pc,::std::span<::std::uint8_t const> bytes) noexcept
    { ++calls; return actual.decode(pc,bytes); }
};
int main()
{
#if defined(UWVM_USE_LLVM_JIT) && ((defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64)) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1))
    ::std::array<::std::uint8_t,5u> bytes{0x48u,0x83u,0xc0u,0x01u,0x90u};
    auto const backward{nd::decode_window(bytes,0x1000u,0x1004u,0,-1,3u)};
    CHECK(backward.available && backward.instructions[0].pc==0x1000u && backward.instructions[0].size==4u);
    CHECK(::std::strstr(backward.instructions[0].text.data(),"add")!=nullptr);
    CHECK(backward.instructions[1].pc==0x1004u && backward.instructions[1].size==1u);
    CHECK(!backward.instructions[2] && backward.instructions[2].pc==0u);
    CHECK(!nd::decode_window(bytes,0x1000u,0x1004u,-1,0,1u).available);
    CHECK(!nd::decode_window(bytes,0x1000u,0x1001u,0,0,1u).available);
#elif defined(UWVM_USE_LLVM_JIT) && defined(__APPLE__) && defined(__aarch64__) && TARGET_OS_OSX
    ::std::array<::std::uint8_t,8u> bytes{0x1fu,0x20u,0x03u,0xd5u,0xc0u,0x03u,0x5fu,0xd6u};
    auto const backward{nd::decode_window(bytes,0x1000u,0x1004u,0,-1,3u)};
    CHECK(backward.available && backward.instructions[0].pc==0x1000u && backward.instructions[0].size==4u);
    CHECK(::std::strstr(backward.instructions[0].text.data(),"nop")!=nullptr);
    CHECK(backward.instructions[1].pc==0x1004u && backward.instructions[1].size==4u);
    CHECK(!backward.instructions[2] && backward.instructions[2].pc==0u);
    CHECK(!nd::decode_window(bytes,0x1000u,0x1004u,-1,0,1u).available);
    CHECK(!nd::decode_window(bytes,0x1000u,0x1001u,0,0,1u).available);
#else
    ::fast_io::io::println("native MC window: UNAVAILABLE qualified native decoder required"); return 77;
#endif
#if defined(UWVM_USE_LLVM_JIT) && ((defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64)) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1) || \
    (defined(__APPLE__) && defined(__aarch64__) && TARGET_OS_OSX))
    counted_mc forward{};
    auto const current{nd::decode_window_with(forward,bytes,0x1000u,0x1004u,0,0,3u)};
    CHECK(current.available && current.instructions[0].pc==0x1004u &&
        !current.instructions[1] && !current.instructions[2] && forward.calls==2u);
    auto const outside{nd::decode_window(bytes,0x1000u,0x1004u,-5,0,2u)};
    CHECK(outside.available && !outside.instructions[0] && !outside.instructions[1]);
    ::fast_io::io::println("native MC window: PASS copied real forward boundaries, backward selection, byte-offset rejection/fillers");
#endif
}
