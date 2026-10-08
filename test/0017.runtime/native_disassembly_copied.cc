// LLVM MC component only, not proof of runtime executable ownership.
#include <uwvm2/uwvm/debugger/native_disassembly.h>
#include <fast_io.h>
#include <array>
#include <cstring>
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("copied disassembly failure line=", __LINE__); ::fast_io::fast_terminate(); } } while(false)
int main()
{
#if defined(UWVM_USE_LLVM_JIT) && ((defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1))
    ::std::array<::std::uint8_t, 5u> bytes{0x48u,0x83u,0xc0u,0x01u,0x90u};
    auto const first{::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0x1234u, bytes)};
    CHECK(first && first.pc == 0x1234u && first.size == 4u && ::std::strstr(first.text.data(), "add") != nullptr);
    // [owned bytes[0..5)] end: the checked four-byte first decode leaves one byte.
    // [safe             ] suffix pointer advances only within this fixed array.
    auto const last{::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0x1238u, {bytes.data()+4u,1u})};
    CHECK(last && last.size == 1u && ::std::strstr(last.text.data(), "nop") != nullptr);
    CHECK(!::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0u,bytes));
    CHECK(!::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0x1234u,{bytes.data(),2u}));
    CHECK(!::uwvm2::uwvm::debugger::native_disassembly::decode_copied(UINTPTR_MAX,{bytes.data(),1u}));
    ::fast_io::io::println("copied LLVM MC disassembly: PASS owned bytes, inert display PC, bounds/truncation");
#elif defined(UWVM_USE_LLVM_JIT) && defined(__APPLE__) && defined(__aarch64__) && TARGET_OS_OSX
    ::std::array<::std::uint8_t,4u> bytes{0x1fu,0x20u,0x03u,0xd5u};
    auto const nop{::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0x1234u,bytes)};
    CHECK(nop && nop.size == 4u && ::std::strstr(nop.text.data(), "nop") != nullptr);
    CHECK(!::uwvm2::uwvm::debugger::native_disassembly::decode_copied(0x1235u,bytes));
    ::fast_io::io::println("copied ARM64 LLVM MC disassembly: PASS owned bytes and alignment");
#else
    ::fast_io::io::println("copied LLVM MC disassembly: UNAVAILABLE qualified host backend required");
    return 77;
#endif
}
