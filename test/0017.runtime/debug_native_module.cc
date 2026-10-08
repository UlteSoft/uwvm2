// Import-only consumer: no debugger or LLVM header can hide a missing export.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#if defined(__APPLE__)
# include <TargetConditionals.h>
#endif
import fast_io;
import uwvm2.uwvm.debugger;

namespace dbg = ::uwvm2::uwvm::debugger;
static void check(bool value, char const* reason) noexcept
{
    if(!value)
    {
        ::fast_io::io::perrln("debug_native_module: ", ::fast_io::mnp::os_c_str(reason));
        ::fast_io::fast_terminate();
    }
}
static_assert(sizeof(::std::array<::std::uint64_t, dbg::native_registers::max_registers>) == 272u);
static_assert(!::std::is_constructible_v<dbg::native_owned_instruction_semantics::decoded_instruction,
                                       dbg::native_instruction_semantics::classification>);

int main()
{
    // Pure copied-model data. No values below came from a live thread/context;
    // these checks establish exported layout and aliases, never native capture.
    dbg::native_registers::snapshot model{};
    check(model.size() == 0u && model.pc() == 0u && model.sp() == 0u, "imported empty snapshot stays unavailable");
    model.machine = dbg::native_registers::architecture::x86_64;
    model.values[16u] = 0x1000u; model.values[7u] = 0x2000u;
    check(model.size() == 18u && model.pc() == 0x1000u && model.sp() == 0x2000u &&
          dbg::native_registers::index_of(model.machine, u8"$pc") == 16u &&
          dbg::native_registers::name(model.machine, dbg::native_registers::max_registers) == nullptr,
          "imported register model count/alias/bounds");
    auto const copied_model{model}; model.values[16u] = 0u;
    check(copied_model.pc() == 0x1000u, "copied model owns immutable display values");
    dbg::native_step::session inactive{};
    check(inactive.state.load() == dbg::native_step::phase::idle, "imported platform session ABI starts idle");
    auto const unavailable{dbg::native_next_policy::choose({}, 15u)};
    check(!unavailable && unavailable.unavailable_reason == dbg::native_next_policy::reason::incomplete_decode,
          "imported default decoded DATA cannot select an instruction");

    dbg::native_owned_instruction_semantics::decoder decoder{};
#if defined(UWVM_USE_LLVM_JIT) && \
    ((defined(__linux__) && defined(__x86_64__)) || \
     (defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
      defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1) || \
     (defined(__APPLE__) && defined(TARGET_OS_OSX) && TARGET_OS_OSX && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1))))
    check(static_cast<bool>(decoder), "actual LLVM-backed imported decoder constructed");
# if defined(__aarch64__)
    constexpr ::std::array<::std::uint8_t, 4u> ordinary{0x20u, 0u, 0x80u, 0x52u};
    constexpr ::std::array<::std::uint8_t, 4u> called{0u, 0u, 0u, 0x94u};
# else
    constexpr ::std::array<::std::uint8_t, 2u> ordinary{0x31u, 0xc0u};
    constexpr ::std::array<::std::uint8_t, 5u> called{0xe8u, 0u, 0u, 0u, 0u};
# endif
    // [owned fixture bytes ... actual array size] end
    // [safe                                   ] only these bounded arrays are
    //  ^^ decoded. Integer 0x1000 is display metadata, never a native pointer.
    auto const decoded{decoder.decode(0x1000u, ordinary)};
    auto const selection{dbg::native_next_policy::choose(decoded, ordinary.size())};
    check(selection && selection.instruction_size == ordinary.size() && decoded.safe_for_single_instruction(),
          "imported actual LLVM MC result selects ordinary DATA");
    auto const call{decoder.decode(0x1000u, called)};
    auto const refused{dbg::native_next_policy::choose(call, called.size())};
    check(call && !refused && refused.unavailable_reason == dbg::native_next_policy::reason::call_continuation_unavailable,
          "imported actual call DATA does not invent a continuation");
    auto const display{dbg::native_disassembly::decode_copied(0x1000u, ordinary)};
    check(display && display.pc == 0x1000u && display.size == ordinary.size(), "imported display decoder consumes owned copy");
    auto const window{dbg::native_disassembly::decode_window(ordinary, 0x1000u, 0x1000u, 0, 0, 1u)};
    check(window.available && window.count == 1u && window.instructions[0u] &&
          window.instructions[0u].size == ordinary.size(), "imported forward window uses known owned boundary");
    check(!decoder.decode(0u, ordinary) && !decoder.decode(UINTPTR_MAX, ordinary), "imported invalid display ranges fail closed");
    ::fast_io::io::println("PASS focused imported native API and real LLVM MC DATA; no kernel capture/runtime/native-next execution qualification");
#else
    check(!decoder && !decoder.decode(0x1000u, ::std::array<::std::uint8_t, 1u>{0x90u}),
          "imported disabled decoder keeps existing platform/build gates");
    ::fast_io::io::println("PASS focused imported native model API; LLVM decoder unavailable, no kernel/runtime qualification");
#endif
}
