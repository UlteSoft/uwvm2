// Real LLVM MC policy component, not a runtime/native-trap qualification.
// No instruction from these owned buffers is executed and no integer PC is
// converted into a pointer. The keeper compiles/runs this in its test cgroup.
#define UWVM_USE_LLVM_JIT 1
#if defined(_WIN32)
# define UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT 1
#endif
#if defined(__APPLE__) && defined(__x86_64__)
# define UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT 1
#endif
#include <uwvm2/uwvm/debugger/native_next_policy.h>
#include <array>
#include <cstdint>
#include <span>
#include <fast_io.h>

namespace mc = ::uwvm2::uwvm::debugger::native_owned_instruction_semantics;
namespace policy = ::uwvm2::uwvm::debugger::native_next_policy;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native-next policy component failed line=", __LINE__); return 1; } } while(false)

int main()
{
    mc::decoder decoder{};
    if(!decoder)
    {
        ::fast_io::io::println("UNAVAILABLE native-next MC policy component: unchanged native decoder platform gate");
        return 77;
    }
    auto decode{[&](auto const& bytes)
    {
        // [owned exact instruction fixture ... bytes.size()] buffer_end
        // [safe                                          ] the real decoder
        //  ^^ receives this bounded owner; 0x1000 is only a decode origin.
        return decoder.decode(0x1000u, ::std::span<::std::uint8_t const>{bytes});
    }};
#if defined(__x86_64__) || defined(_M_X64)
    constexpr ::std::array<::std::uint8_t, 2u> normal{0x31u, 0xc0u}; // xor eax,eax
    constexpr ::std::array<::std::uint8_t, 5u> call{0xe8u, 0u, 0u, 0u, 0u};
    constexpr ::std::array<::std::uint8_t, 2u> indirect_call{0xffu, 0xd0u};
    constexpr ::std::array<::std::uint8_t, 1u> returned{0xc3u};
    constexpr ::std::array<::std::uint8_t, 2u> branch{0x75u, 0x02u};
    constexpr ::std::array<::std::uint8_t, 2u> indirect_branch{0xffu, 0xe0u};
    constexpr ::std::array<::std::uint8_t, 2u> trapped{0x0fu, 0x0bu};
    // Repeated string execution and flags/interrupt state require stronger
    // physical-step semantics. A bare MC ordinary-flow flag is insufficient.
    CHECK(!policy::choose(decode(::std::array<::std::uint8_t, 2u>{0xf3u, 0xa4u}), 2u));
    CHECK(!policy::choose(decode(::std::array<::std::uint8_t, 1u>{0x9du}), 1u));
    CHECK(!policy::choose(decode(::std::array<::std::uint8_t, 1u>{0xfbu}), 1u));
    CHECK(!policy::choose(decode(::std::array<::std::uint8_t, 2u>{0x8eu, 0xd0u}), 2u));
    CHECK(!policy::choose(decode(::std::array<::std::uint8_t, 2u>{0x0fu, 0x05u}), 2u));
#elif defined(__aarch64__)
    constexpr ::std::array<::std::uint8_t, 4u> normal{0x20u, 0u, 0x80u, 0x52u}; // mov w0,#1
    constexpr ::std::array<::std::uint8_t, 4u> call{0u, 0u, 0u, 0x94u};
    constexpr ::std::array<::std::uint8_t, 4u> indirect_call{0u, 0u, 0x3fu, 0xd6u};
    constexpr ::std::array<::std::uint8_t, 4u> returned{0xc0u, 0x03u, 0x5fu, 0xd6u};
    constexpr ::std::array<::std::uint8_t, 4u> branch{0x40u, 0u, 0u, 0x54u};
    constexpr ::std::array<::std::uint8_t, 4u> indirect_branch{0u, 0u, 0x1fu, 0xd6u};
    constexpr ::std::array<::std::uint8_t, 4u> trapped{0u, 0u, 0x20u, 0xd4u};
    // Architecturally constrained writeback is an actual LLVM SoftFail, not
    // a successful ordinary instruction with an invented fallthrough.
    CHECK(!policy::choose(decode(::std::array<::std::uint8_t, 4u>{0u, 0x44u, 0x40u, 0xb8u}), 4u));
#else
    ::fast_io::io::println("UNAVAILABLE native-next MC policy component: architecture gate");
    return 77;
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    auto const ordinary{decode(normal)};
    auto const selected{policy::choose(ordinary, normal.size())};
    CHECK(ordinary && selected && selected.operation == policy::action::single_instruction &&
          selected.unavailable_reason == policy::reason::none && selected.instruction_size == normal.size());
    CHECK(!policy::choose(ordinary, normal.size() - 1u));
    CHECK(!policy::choose(mc::decoded_instruction{}, normal.size()));
    CHECK(!policy::choose(decoder.decode(0x1000u, {}), 0u));
    CHECK(policy::choose(decode(call), call.size()).unavailable_reason == policy::reason::call_continuation_unavailable);
    CHECK(policy::choose(decode(indirect_call), indirect_call.size()).unavailable_reason == policy::reason::call_continuation_unavailable);
    CHECK(policy::choose(decode(returned), returned.size()).unavailable_reason == policy::reason::caller_unwind_unavailable);
    CHECK(policy::choose(decode(branch), branch.size()).unavailable_reason == policy::reason::branch_continuation_unavailable);
    CHECK(policy::choose(decode(indirect_branch), indirect_branch.size()).unavailable_reason == policy::reason::branch_continuation_unavailable);
    CHECK(policy::choose(decode(trapped), trapped.size()).unavailable_reason == policy::reason::trap_instruction);
    ::fast_io::io::println("PASS real LLVM native-next policy component: bounded ordinary selection, actual call/return/branch/trap rejection; no instruction execution or runtime authorization");
    return 0;
#endif
}
