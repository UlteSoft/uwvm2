#include <uwvm2/uwvm/debugger/native_registers.h>
#include <fast_io.h>
#include <limits>

namespace regs = ::uwvm2::uwvm::debugger::native_registers;
static_assert(regs::index_of(regs::architecture::x86_64, u8"$pc") == 16u);
static_assert(regs::index_of(regs::architecture::x86_64, u8"$rsp") == 7u);
static_assert(regs::index_of(regs::architecture::x86_64, u8"fp") == 6u);
static_assert(regs::index_of(regs::architecture::x86_64, u8"$r15") == 15u);
static_assert(regs::index_of(regs::architecture::aarch64, u8"x29") == 29u);
static_assert(regs::index_of(regs::architecture::aarch64, u8"$x30") == 30u);
static_assert(regs::index_of(regs::architecture::aarch64, u8"$sp") == 31u);
static_assert(regs::index_of(regs::architecture::aarch64, u8"pc") == 32u);
static_assert(regs::index_of(regs::architecture::unavailable, u8"rax") == regs::unavailable_index);
static_assert(regs::index_of(regs::architecture::x86_64, u8"$x0") == regs::unavailable_index);
static_assert(regs::index_of(regs::architecture::aarch64, u8"$") == regs::unavailable_index);
static_assert(regs::index_of(regs::architecture::x86_64, u8"$$rax") == regs::unavailable_index);
static_assert(regs::name(regs::architecture::x86_64, 18u) == nullptr);
static_assert(regs::name(regs::architecture::aarch64, regs::max_registers) == nullptr);
static_assert(regs::name(regs::architecture::aarch64, (::std::numeric_limits<::std::size_t>::max)()) == nullptr);
static_assert(regs::count(static_cast<regs::architecture>(255u)) == 0u);

int main()
{
    // A retained response must own all values even after the backing trap
    // session changes. These architectural aliases use different slots.
    regs::snapshot x86{};
    x86.machine = regs::architecture::x86_64;
    x86.values[6u] = 0x6000u;
    x86.values[7u] = 0x7000u;
    x86.values[16u] = 0x16000u;
    auto const retained{x86};
    x86.values.fill(0u);
    if(retained.size() != 18u || retained.fp() != 0x6000u ||
       retained.sp() != 0x7000u || retained.pc() != 0x16000u) { return 1; }
    regs::snapshot arm{};
    arm.machine = regs::architecture::aarch64;
    arm.values[29u] = 0x29000u;
    arm.values[31u] = 0x31000u;
    arm.values[32u] = 0x32000u;
    if(arm.size() != 34u || arm.fp() != 0x29000u ||
       arm.sp() != 0x31000u || arm.pc() != 0x32000u) { return 2; }
    ::fast_io::io::println("PASS native register names, architectural aliases, bounds, owned response lifetime");
}
