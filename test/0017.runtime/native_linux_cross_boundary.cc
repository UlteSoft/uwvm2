// Actual Linux target execution of the unsupported native backend's boundary.
// Passing this test proves refusal and DATA projection, never JIT stepping.
#include <uwvm2/uwvm/debugger/native_step.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
#include <bit>
#include <cstdint>
#include <limits>

#if !defined(__linux__) || defined(__x86_64__)
# error This negative backend test requires a non-x86_64 Linux target.
#endif

namespace step = ::uwvm2::uwvm::debugger::native_step;
namespace dbg = ::uwvm2::uwvm::debugger;
namespace regs = dbg::native_registers;
#define CHECK(value) do { if(!(value)) { ::fast_io::io::perrln("FAIL cross native boundary line=", __LINE__); return 1; } } while(false)

int main()
{
    #if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
    CHECK(step::platform_available() && step::install());
#else
    CHECK(!step::platform_available() && !step::install());
#endif
    CHECK(step::uninstall());
    unsigned callbacks{};
    step::session session{};
    for(auto state : {step::phase::idle, step::phase::ready, step::phase::at_guest_pc,
                      step::phase::trapped, step::phase::released})
    {
        session.state.store(state);
        CHECK(!step::request(session, 1u, 1u, (::std::numeric_limits<::std::uintptr_t>::max)(), 1u));
        CHECK(!step::arm_at_return(1u) && !step::continue_one(session));
        CHECK(!step::continue_from_trap(session) && !step::release(session) && !step::clear(session));
        CHECK(session.state.load() == state);
        CHECK(!step::with_owned_trap(::std::addressof(session), [&](auto...) noexcept { ++callbacks; }));
        CHECK(!step::with_owned_registers(::std::addressof(session), [&](auto...) noexcept { ++callbacks; }));
        // These identities must be refused without dereferencing address 1.
        auto const forged{reinterpret_cast<void const*>(::std::uintptr_t{1u})};
        CHECK(!step::with_owned_trap(forged, [&](auto...) noexcept { ++callbacks; }));
        CHECK(!step::with_owned_registers(forged, [&](auto...) noexcept { ++callbacks; }));
    }
    CHECK(callbacks == 0u && session.register_snapshot.size() == 0u);
    CHECK(session.register_snapshot.pc() == 0u && session.register_snapshot.sp() == 0u);
    CHECK(session.register_snapshot.fp() == 0u && !session.register_snapshot.floating.available);

    for(auto text : {"disassemble 1 2 1 0x1000", "disassemble 1 2 1 -1",
                     "ni 0x1000", "ni 1 0x1000", "memory 0 0 0x1000 8",
                     "memory $sp 0 0 8", "memory 0 0 $sp 8"})
    { CHECK(dbg::parse_console_command(::fast_io::mnp::os_c_str(text)).kind == dbg::console_command_kind::invalid); }
    CHECK(dbg::parse_console_command("memory 0 0 0 8").operation == ::uwvm2::utils::control::operation::read_memory);

    // Models are intentionally independent of the executing ISA. No real trap
    // or Wasm provenance is invented by constructing this DATA input.
    regs::snapshot raw{}; raw.machine = regs::architecture::aarch64;
    raw.values.fill(0xdeadbeef12345678ull);
    regs::numeric_location locations[]{{0u,32u},{18u,64u},{29u,64u},{30u,64u},{31u,64u},{33u,64u}};
    auto const shown{regs::project(raw, locations, 6u)};
    CHECK(shown.values[0u] == 0x12345678u && shown.known_bits[0u] == 32u);
    for(unsigned i{1u}; i != 32u; ++i) { CHECK(shown.values[i] == 0u && shown.known_bits[i] == 0u); }
    CHECK(shown.values[33u] == 0u && shown.known_bits[33u] == 0u && !shown.floating.available);
    auto const oversized{regs::project(raw, locations, 65u)};
    CHECK(oversized.known_bits[0u] == 0u && oversized.sp() == 0u && oversized.fp() == 0u);
    raw.machine = regs::architecture::unavailable;
    auto const absent{regs::project(raw, locations, 6u)};
    CHECK(absent.size() == 0u && absent.pc() == 0u && absent.sp() == 0u && absent.fp() == 0u);
    for(auto value : absent.values) { CHECK(value == 0u); }
    for(auto value : absent.known_bits) { CHECK(value == 0u); }
    ::fast_io::io::println("PASS integer-only Linux native requests refused: no trap/register callbacks, no forged address access; bounded grammar and register DATA projection; pointer_bits=", sizeof(void*) * 8u,
                         " endian=", ::fast_io::mnp::os_c_str(::std::endian::native == ::std::endian::little ? "little" : "big"),
                         " native_capability=closed native_stack=unavailable");
}
