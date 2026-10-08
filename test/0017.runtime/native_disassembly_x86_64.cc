#include <uwvm2/uwvm/debugger/native_disassembly.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#define CHECK(condition) do { if(!(condition)) { ::std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); ::std::abort(); } } while(false)

int main()
{
#if defined(__linux__) && defined(__x86_64__) && defined(UWVM_USE_LLVM_JIT)
    namespace dis = ::uwvm2::uwvm::debugger::native_disassembly;
    auto const page{static_cast<::std::size_t>(::sysconf(_SC_PAGESIZE))};
    CHECK(page >= 4096u);
    void* const mapping{::mmap(nullptr, page * 2u, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
    CHECK(mapping != MAP_FAILED);
    auto* const end{static_cast<::std::uint8_t*>(mapping) + page};
    // addq $1, %rax; nop. The last byte borders an inaccessible page, so
    // even a speculative 15-byte decode from nop would expose over-reading.
    ::std::uint8_t const code[]{0x48u, 0x83u, 0xc0u, 0x01u, 0x90u};
    ::std::memcpy(end - sizeof(code), code, sizeof(code));
    CHECK(::mprotect(end, page, PROT_NONE) == 0);
    CHECK(::mprotect(mapping, page, PROT_READ | PROT_EXEC) == 0);
    auto const begin{reinterpret_cast<::std::uintptr_t>(end - sizeof(code))};
    auto const limit{reinterpret_cast<::std::uintptr_t>(end)};
    auto const add{dis::decode_owned(begin, begin, limit)};
    CHECK(add && add.size == 4u && add.pc == begin);
    CHECK(::std::strstr(add.text.data(), "add") != nullptr);
    for(::std::size_t index{}; index != 4u; ++index) { CHECK(add.bytes[index] == code[index]); }
    auto const nop{dis::decode_owned(begin + 4u, begin, limit)};
    CHECK(nop && nop.size == 1u && nop.bytes[0] == 0x90u);
    CHECK(::std::strstr(nop.text.data(), "nop") != nullptr);
    CHECK(!dis::decode_owned(begin - 1u, begin, limit));
    CHECK(!dis::decode_owned(limit, begin, limit));
    CHECK(!dis::decode_owned(begin, begin, begin + 2u));
    CHECK(::munmap(mapping, page * 2u) == 0);
    ::std::puts("PASS bounded LLVM X86 disassembly at JIT owner edge");
#endif
}
