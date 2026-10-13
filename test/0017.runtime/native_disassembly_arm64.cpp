// Host-owned JIT-byte decoding must stop exactly at the authenticated function
// boundary, even when the next virtual-memory page is inaccessible.
#if !defined(__APPLE__) || !defined(__aarch64__)
# error This test only runs on macOS ARM64.
#endif
#include <TargetConditionals.h>
#if !TARGET_OS_OSX
# error This test only runs on macOS ARM64.
#endif
#ifndef UWVM_USE_LLVM_JIT
# define UWVM_USE_LLVM_JIT
#endif
#include <uwvm2/uwvm/debugger/native_disassembly.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace disassembly = ::uwvm2::uwvm::debugger::native_disassembly;

int main()
{
    auto const page_length{::sysconf(_SC_PAGESIZE)};
    if(page_length < 4096) { return 1; }
    auto const page_size{static_cast<::std::size_t>(page_length)};
    auto* const pages{static_cast<::std::uint8_t*>(::mmap(nullptr, page_size * 2u,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0))};
    if(pages == MAP_FAILED) { return 2; }
    if(::mprotect(pages + page_size, page_size, PROT_NONE) != 0) { return 3; }
    // AArch64 NOP (0xd503201f), the last complete instruction before the
    // inaccessible page. The decoder may copy only these four owner bytes.
    ::std::uint8_t constexpr nop[]{0x1f, 0x20, 0x03, 0xd5};
    auto* const instruction{pages + page_size - sizeof(nop)};
    ::std::memcpy(instruction, nop, sizeof(nop));
    if(::mprotect(pages, page_size, PROT_READ | PROT_EXEC) != 0) { return 4; }
    auto const pc{reinterpret_cast<::std::uintptr_t>(instruction)};
    auto const decoded{disassembly::decode_owned(pc, pc, pc + 4u)};
    if(!decoded || decoded.size != 4u || decoded.pc != pc ||
       ::std::memcmp(decoded.bytes.data(), nop, sizeof(nop)) != 0 ||
       ::std::strstr(decoded.text.data(), "nop") == nullptr) { return 5; }
    if(disassembly::decode_owned(pc, pc, pc + 3u) ||
       disassembly::decode_owned(pc + 1u, pc, pc + 4u) ||
       disassembly::decode_owned(pc + 4u, pc, pc + 4u)) { return 6; }
    if(::munmap(pages, page_size * 2u) != 0) { return 7; }
    ::std::puts("PASS ARM64 owner-bounded LLVM MC disassembly at guard page");
}
