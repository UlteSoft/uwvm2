// Native SDK/asm-link ABI regression. This does not qualify generated JIT CFI,
// guest exceptions, or a section manager's dynamic FDE registration.
#include <fast_io.h>
#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#if !defined(_WIN32) && (defined(__GNUC__) || defined(__clang__)) && __has_include(<unwind.h>) && \
    UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED && defined(__EXCEPTIONS)
# define UWVM_TEST_NATIVE_UNWIND_ABI_AVAILABLE 1
# include <uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h>
#else
# define UWVM_TEST_NATIVE_UNWIND_ABI_AVAILABLE 0
#endif
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED")

#if UWVM_TEST_NATIVE_UNWIND_ABI_AVAILABLE
namespace abi = uwvm2::runtime::compiler::llvm_jit::native_unwind_abi;
namespace
{
    template<typename T> struct noexcept_pointer;
    template<typename Result, typename... Args>
    struct noexcept_pointer<Result (*)(Args...)>
    { using type = Result (*)(Args...) noexcept; };
    template<typename Result, typename... Args>
    struct noexcept_pointer<Result (*)(Args...) noexcept>
    { using type = Result (*)(Args...) noexcept; };
    template<typename T> using noexcept_pointer_t = typename noexcept_pointer<T>::type;

    // Check the complete selected SDK function type, not only a sizeof match.
    // In particular, GNU _Unwind_Word and Darwin uintptr_t need not be spelled
    // alike even when they occupy the same number of bytes.
    static_assert(std::is_same_v<decltype(&abi::get_ip_info_noexcept), noexcept_pointer_t<decltype(&::_Unwind_GetIPInfo)>>);
    static_assert(std::is_same_v<decltype(&abi::get_cfa_noexcept), noexcept_pointer_t<decltype(&::_Unwind_GetCFA)>>);
    static_assert(std::is_same_v<decltype(&abi::get_region_start_noexcept), noexcept_pointer_t<decltype(&::_Unwind_GetRegionStart)>>);
    static_assert(std::is_same_v<decltype(&abi::backtrace_noexcept), noexcept_pointer_t<decltype(&::_Unwind_Backtrace)>>);
    static_assert(noexcept(abi::get_ip_info_noexcept(nullptr, nullptr)));
    static_assert(noexcept(abi::get_cfa_noexcept(nullptr)));
    static_assert(noexcept(abi::get_region_start_noexcept(nullptr)));
    static_assert(noexcept(abi::backtrace_noexcept(nullptr, nullptr)));
# if defined(__APPLE__)
    static_assert(std::is_same_v<decltype(&abi::register_frame_noexcept), noexcept_pointer_t<decltype(&::__register_frame)>>);
    static_assert(std::is_same_v<decltype(&abi::deregister_frame_noexcept), noexcept_pointer_t<decltype(&::__deregister_frame)>>);
# endif
    // These SDK declarations must retain genuine propagation semantics.
    static_assert(!noexcept(::_Unwind_RaiseException(nullptr)));
    static_assert(!noexcept(::_Unwind_Resume(nullptr)));

    struct frame
    {
        std::uintptr_t ip{};
        std::uintptr_t cfa{};
        std::uintptr_t region{};
        int ip_before{};
    };
    struct trace
    {
        std::array<frame, 64> frames{};
        std::array<std::uintptr_t, 3> expected_return_ips{};
        std::size_t size{};
        volatile unsigned after_call{};
        _Unwind_Reason_Code result{_URC_NO_REASON};
    };
    unsigned checks{};
# define CHECK(condition) do { ++checks; if(!(condition)) { ::fast_io::io::perrln("FAIL native unwind ABI ", __LINE__, ": ", #condition); return 1; } } while(false)

    [[nodiscard]] std::uintptr_t instruction_address(void* value) noexcept
    {
        auto address = reinterpret_cast<std::uintptr_t>(value);
# if defined(__arm__) || defined(__thumb__)
        // Thumb's mode bit is not part of an instruction address. Only actual
        // compiler-selected ARM DWARF builds reach this test, never ARM EHABI.
        address &= ~std::uintptr_t{1};
# endif
        return address;
    }
}

// Keep simple unmangled wrappers for the runner's LLVM IR call-attribute audit.
extern "C" [[gnu::noinline]] abi::ip_info_result uwvm_native_abi_ip(_Unwind_Context* context, int* before) noexcept
{ return abi::get_ip_info_noexcept(context, before); }
extern "C" [[gnu::noinline]] abi::cfa_result uwvm_native_abi_cfa(_Unwind_Context* context) noexcept
{ return abi::get_cfa_noexcept(context); }
extern "C" [[gnu::noinline]] abi::region_start_result uwvm_native_abi_region(_Unwind_Context* context) noexcept
{ return abi::get_region_start_noexcept(context); }
extern "C" [[gnu::noinline]] _Unwind_Reason_Code uwvm_native_abi_backtrace(_Unwind_Trace_Fn callback, void* context) noexcept
{ return abi::backtrace_noexcept(callback, context); }

namespace
{
    _Unwind_Reason_Code collect(_Unwind_Context* context, void* opaque) noexcept
    {
        auto& output = *static_cast<trace*>(opaque);
        if(output.size == output.frames.size()) { return _URC_NORMAL_STOP; }
        auto& current = output.frames[output.size++];
        current.ip = instruction_address(reinterpret_cast<void*>(uwvm_native_abi_ip(context, &current.ip_before)));
        current.cfa = static_cast<std::uintptr_t>(uwvm_native_abi_cfa(context));
        current.region = static_cast<std::uintptr_t>(uwvm_native_abi_region(context));
        return _URC_NO_REASON;
    }
    static_assert(std::is_convertible_v<decltype(&collect), _Unwind_Trace_Fn>);

    [[gnu::noinline]] void capture(trace& output) noexcept
    {
        // This exact, compiler-produced return PC belongs to level_three.
        // No function layout, symbol lookup, frame-pointer chain, or fake
        // _Unwind_Context is used to infer the three caller frames.
        output.expected_return_ips[2] = instruction_address(__builtin_extract_return_addr(__builtin_return_address(0)));
        output.result = uwvm_native_abi_backtrace(collect, &output);
        output.after_call = output.after_call + 3u;
    }
    [[gnu::noinline]] void level_three(trace& output) noexcept
    {
        output.expected_return_ips[1] = instruction_address(__builtin_extract_return_addr(__builtin_return_address(0)));
        capture(output);
        output.after_call = output.after_call + 7u;
    }
    [[gnu::noinline]] void level_two(trace& output) noexcept
    {
        output.expected_return_ips[0] = instruction_address(__builtin_extract_return_addr(__builtin_return_address(0)));
        level_three(output);
        output.after_call = output.after_call + 11u;
    }
    [[gnu::noinline]] void level_one(trace& output) noexcept
    {
        level_two(output);
        // An observable operation after every call prevents sibling-call
        // elimination even when this fixture is compiled with -O3.
        output.after_call = output.after_call + 13u;
    }

    struct native_payload_error final
    {
        std::uint64_t bits;
        unsigned* identity;
    };
    struct cleanup_guard final
    {
        unsigned& count;
        ~cleanup_guard() noexcept { ++count; }
    };
    constexpr std::uint64_t payload_bits{0xfedcba9876543210ULL};
}

extern "C" [[gnu::noinline]] void uwvm_native_abi_throw(unsigned* identity)
{ throw native_payload_error{payload_bits, identity}; }
extern "C" [[gnu::noinline]] void uwvm_native_abi_rethrow(unsigned* identity, unsigned& cleanup, unsigned& caught)
{
    try
    {
        cleanup_guard guard{cleanup};
        uwvm_native_abi_throw(identity);
    }
    catch(native_payload_error const& error)
    {
        if(error.bits == payload_bits && error.identity == identity && cleanup == 1u) { ++caught; }
        // This must lower to genuine, potentially unwinding __cxa_rethrow.
        throw;
    }
}
static_assert(!noexcept(uwvm_native_abi_throw(nullptr)));
static_assert(!noexcept(uwvm_native_abi_rethrow(nullptr, std::declval<unsigned&>(), std::declval<unsigned&>())));

int main()
{
    for(unsigned attempt{}; attempt != 4u; ++attempt)
    {
        trace output;
        level_one(output);
        CHECK(output.after_call == 34u);
        CHECK(output.size >= 3u && output.size < output.frames.size());
        CHECK(output.result == _URC_END_OF_STACK || output.result == _URC_NO_REASON);
        std::array<std::size_t, 3> positions{output.size, output.size, output.size};
        for(std::size_t slot{}; slot != positions.size(); ++slot)
        {
            CHECK(output.expected_return_ips[slot] != 0u);
            unsigned matches{};
            for(std::size_t index{}; index != output.size; ++index)
            {
                if(output.frames[index].ip != output.expected_return_ips[slot]) { continue; }
                positions[slot] = index;
                ++matches;
            }
            CHECK(matches == 1u);
            auto const& current = output.frames[positions[slot]];
            CHECK(current.cfa != 0u && current.region != 0u);
            CHECK(current.ip_before == 0);
        }
        CHECK(positions[2] < positions[1] && positions[1] < positions[0]);
        CHECK(output.frames[positions[0]].cfa != output.frames[positions[1]].cfa);
        CHECK(output.frames[positions[0]].cfa != output.frames[positions[2]].cfa);
        CHECK(output.frames[positions[1]].cfa != output.frames[positions[2]].cfa);
    }
    unsigned identity{42u}, cleanup{}, caught{}, outer{};
    try { uwvm_native_abi_rethrow(&identity, cleanup, caught); }
    catch(native_payload_error const& error)
    {
        CHECK(error.bits == payload_bits && error.identity == &identity);
        ++outer;
    }
    catch(...) { CHECK(false); }
    CHECK(cleanup == 1u && caught == 1u && outer == 1u);
    ::fast_io::io::println("PASS native unwind ABI: ", checks,
                          " checks; SDK noexcept types, four exact three-frame native chains, typed catch/rethrow and cleanup; no JIT qualification");
}
#else
int main()
{
    ::fast_io::io::println("SKIP native unwind ABI: GNU/Clang, an eligible non-Windows table-driven native unwind SDK and C++ exceptions required; no JIT qualification");
    return 77;
}
#endif
#undef CHECK
#undef UWVM_TEST_NATIVE_UNWIND_ABI_AVAILABLE
