#include <uwvm2/uwvm/debugger/native_registers.h>
#include <fast_io.h>
namespace regs = uwvm2::uwvm::debugger::native_registers;
static void check(bool value) { if(!value) { ::fast_io::io::perrln("native register projection failed"); ::fast_io::fast_terminate(); } }
int main()
{
    regs::snapshot raw{}; raw.machine = regs::architecture::x86_64;
    for(auto& value: raw.values) { value = 0x13579bdf2468ace0u; }
    raw.values[0] = 0xdeadbeef12345678u; raw.values[16] = 0x1234u;
    raw.floating.available = true;
    for(auto& value: raw.floating.values) { value.width = 16u; value.bytes.fill(0xa5u); }
    raw.floating.values[0].bytes[0] = 0x12u;
    regs::numeric_location locations[]{{0u,32u},{17u,32u},{6u,64u},{7u,64u},{16u,64u},{33u,128u}};
    auto shown{regs::project(raw, locations, 6u)};
    check(shown.values[0] == 0x12345678u && shown.known_bits[0] == 32u);
    check(shown.pc() == 0x1234u && shown.known_bits[16] == 64u);
    for(unsigned i{1u}; i != 16u; ++i) { check(shown.values[i] == 0u && shown.known_bits[i] == 0u); }
    check(shown.values[17] == 0u && shown.known_bits[17] == 0u);
    check(shown.floating.values[0].width == 4u && shown.floating.values[0].bytes[0] == 0x12u);
    for(unsigned i{4u}; i != 16u; ++i) { check(shown.floating.values[0].bytes[i] == 0u); }
    for(unsigned i{1u}; i != regs::max_fp_registers; ++i) { check(shown.floating.values[i].width == 0u); }
    check(raw.values[0] == 0xdeadbeef12345678u && raw.floating.values[0].bytes[8] == 0xa5u);
    auto none{regs::project(raw,nullptr,0u)}; check(none.known_bits[0] == 0u && none.pc() == raw.pc());
    raw.machine = regs::architecture::aarch64;
    regs::numeric_location arm[]{{0u,64u},{18u,64u},{29u,64u},{30u,64u},{31u,64u}};
    shown = regs::project(raw,arm,5u); check(shown.known_bits[0] == 64u);
    for(unsigned i{18u}; i != 32u; ++i) { check(shown.known_bits[i] == 0u && shown.values[i] == 0u); }
}
