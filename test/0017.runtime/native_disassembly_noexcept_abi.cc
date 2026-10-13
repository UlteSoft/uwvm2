// Private ABI witness, source-only. This exercises the actual LLVM C SDK and
// linked LLVM decoder; it is not a Wasm parser, VM, JIT EH or debugger proof.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/native_disassembly_abi.h>

namespace abi = ::uwvm2::uwvm::debugger::native_disassembly_abi;
#if defined(__GNUC__) || defined(__clang__)
# define UWVM_ABI_PROBE_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define UWVM_ABI_PROBE_NOINLINE __declspec(noinline)
#else
# define UWVM_ABI_PROBE_NOINLINE
#endif

// Keep observable callers for a future actual -emit-llvm/COFF-symbol audit.
// Both uint64_t parameters must remain i64 on i686; size_t alone becomes i32.
extern "C" UWVM_ABI_PROBE_NOINLINE LLVMDisasmContextRef
uwvm_probe_disasm_create(char const* triple) noexcept
{
    return abi::uwvm_LLVMCreateDisasm(triple, nullptr, 0, nullptr, nullptr);
}
extern "C" UWVM_ABI_PROBE_NOINLINE ::std::size_t
uwvm_probe_disasm_decode(LLVMDisasmContextRef context, ::std::uint8_t* bytes,
    ::std::uint64_t available, ::std::uint64_t pc, char* output,
    ::std::size_t capacity) noexcept
{
    return abi::uwvm_LLVMDisasmInstruction(context, bytes, available, pc, output, capacity);
}
extern "C" UWVM_ABI_PROBE_NOINLINE void
uwvm_probe_disasm_dispose(LLVMDisasmContextRef context) noexcept
{
    abi::uwvm_LLVMDisasmDispose(context);
}
extern "C" UWVM_ABI_PROBE_NOINLINE void uwvm_probe_disasm_initialize() noexcept
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    abi::uwvm_LLVMInitializeX86TargetInfo();
    abi::uwvm_LLVMInitializeX86Target();
    abi::uwvm_LLVMInitializeX86TargetMC();
    abi::uwvm_LLVMInitializeX86Disassembler();
#elif defined(__aarch64__) || defined(_M_ARM64)
    abi::uwvm_LLVMInitializeAArch64TargetInfo();
    abi::uwvm_LLVMInitializeAArch64Target();
    abi::uwvm_LLVMInitializeAArch64TargetMC();
    abi::uwvm_LLVMInitializeAArch64Disassembler();
#else
# error "The source-only decoder witness needs an actual X86 or AArch64 SDK backend"
#endif
}
static_assert(noexcept(uwvm_probe_disasm_create(nullptr)));
static_assert(noexcept(uwvm_probe_disasm_decode(nullptr, nullptr, 0, 0, nullptr, 0)));
static_assert(noexcept(uwvm_probe_disasm_dispose(nullptr)));
static_assert(noexcept(uwvm_probe_disasm_initialize()));
static_assert(sizeof(::std::uint64_t) == 8);
static_assert(::std::is_same_v<decltype(&uwvm_probe_disasm_decode), abi::decode_abi>);

namespace
{
    struct cpp_payload { int value; };
    struct destructor_guard
    {
        int* count;
        ~destructor_guard() noexcept { ++*count; }
    };
    // A genuine C++ EH positive control. This function is deliberately not
    // noexcept and not asm-aliased; its typed throw/destructor remain real.
    UWVM_ABI_PROBE_NOINLINE void genuine_cpp_throw(int* count)
    {
        destructor_guard guard{count};
        throw cpp_payload{37};
    }
    struct context_owner
    {
        LLVMDisasmContextRef context;
        ~context_owner() noexcept { if(context != nullptr) { uwvm_probe_disasm_dispose(context); } }
    };
    bool require(bool value, char const* name)
    {
        if(!value) { ::fast_io::io::perrln("[FAIL] LLVM C noexcept ABI: ", ::fast_io::mnp::os_c_str(name)); }
        return value;
    }
}
int main()
{
    uwvm_probe_disasm_initialize();
#if defined(__x86_64__) || defined(_M_X64)
# if defined(_WIN32)
    char const* const triple{"x86_64-pc-windows-msvc"};
# elif defined(__APPLE__)
    char const* const triple{"x86_64-apple-darwin"};
# else
    char const* const triple{"x86_64-pc-linux-gnu"};
# endif
    ::std::array<::std::uint8_t, 2> bytes{0x90, 0x90};
    constexpr ::std::size_t expected_length{1};
#elif defined(__i386__) || defined(_M_IX86)
# if defined(_WIN32)
    char const* const triple{"i686-pc-windows-msvc"};
# else
    char const* const triple{"i386-pc-linux-gnu"};
# endif
    ::std::array<::std::uint8_t, 2> bytes{0x90, 0x90};
    constexpr ::std::size_t expected_length{1};
#else
# if defined(__APPLE__)
    char const* const triple{"aarch64-apple-darwin"};
# elif defined(_WIN32)
    char const* const triple{"aarch64-pc-windows-msvc"};
# else
    char const* const triple{"aarch64-pc-linux-gnu"};
# endif
    ::std::array<::std::uint8_t, 8> bytes{0x1f, 0x20, 0x03, 0xd5, 0x1f, 0x20, 0x03, 0xd5};
    constexpr ::std::size_t expected_length{4};
#endif
    context_owner owner{uwvm_probe_disasm_create(triple)};
    if(!require(owner.context != nullptr, "native SDK decoder creation")) { return 1; }
    ::std::array<char, 128> output{};
    auto const first{uwvm_probe_disasm_decode(owner.context, bytes.data(), bytes.size(),
        0x100000001ull, output.data(), output.size())};
    if(!require(first == expected_length && ::std::string_view(output.data(), output.size()).find("nop") != ::std::string_view::npos,
        "first owned NOP with 64-bit PC")) { return 1; }
    output.fill(0);
    // [safe] first == expected_length < bytes.size(); advance stays inside
    // this owned byte array, and the remaining decoder extent is exact.
    auto const second{uwvm_probe_disasm_decode(owner.context, bytes.data() + first, bytes.size() - first,
        0x100000001ull + first, output.data(), output.size())};
    if(!require(second == expected_length && ::std::string_view(output.data(), output.size()).find("nop") != ::std::string_view::npos,
        "second owned NOP")) { return 1; }
    output.fill(0);
    auto const empty{uwvm_probe_disasm_decode(owner.context, bytes.data(), 0,
        0x100000001ull, output.data(), output.size())};
    if(!require(empty == 0, "zero-size decode does not consume bytes")) { return 1; }
    int destroyed{};
    bool caught{};
    try { genuine_cpp_throw(&destroyed); }
    catch(cpp_payload const& value) { caught = value.value == 37; }
    if(!require(caught && destroyed == 1, "genuine typed C++ throw and cleanup retained")) { return 1; }
    ::fast_io::io::println("[PASS] LLVM C noexcept ABI decode=2 empty=1 cpp-cleanup=1");
}
#undef UWVM_ABI_PROBE_NOINLINE
