// Actual LLVM MC on owned instruction DATA; no live stack or resume authority.
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace semantics = ::uwvm2::uwvm::debugger::native_owned_instruction_semantics;
static void check(bool value, char const* message)
{
    if(!value)
    { ::fast_io::io::perrln("native_return_pop_i386: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
struct description
{
    bool available{true}, little_endian{true}; unsigned description_version{1u}, pointer_bits{32u};
    ::std::size_t maximum_instruction_bytes{15u}, minimum_instruction_alignment{1u};
    ::std::array<char,64u> triple{}, cpu{}, features{};
    ::std::size_t triple_size{}, cpu_size{}, features_size{};
    explicit description(char const* text)
    { triple_size=::fast_io::cstr_len(text); check(triple_size<triple.size(),"bounded description"); ::std::memcpy(triple.data(),text,triple_size+1u); }
};
int main()
{
    description target{"i686-unknown-linux-gnu"};
    semantics::decoder mc{target}; check(bool(mc),"genuine i386 MC target");
    constexpr ::std::array<unsigned char,1u> ret{0xc3u};
    constexpr ::std::array<unsigned char,3u> pop{0xc2u,0x00u,0x01u}, high{0xc2u,0xffu,0xffu};
    constexpr ::std::array<unsigned char,2u> short_pop{0xc2u,0x00u}, ret16{0x66u,0xc3u};
    constexpr ::std::array<unsigned char,1u> far_ret{0xcbu}, ordinary{0x90u}, interrupt_ret{0xcfu};
    check(mc.decode(0x1000u,ret).near_return_pop_bytes()==0u,"actual near RET32");
    check(mc.decode(0x1000u,pop).near_return_pop_bytes()==256u,"actual TailCC RETI32 argument pop");
    check(mc.decode(0x1000u,high).near_return_pop_bytes()==UINT16_MAX,"encoded imm16 is unsigned");
    check(mc.decode(0x1000u,short_pop).near_return_pop_bytes()==SIZE_MAX,"truncated immediate denied");
    check(mc.decode(0x1000u,ret16).near_return_pop_bytes()==SIZE_MAX,"operand-size return denied");
    check(mc.decode(0x1000u,far_ret).near_return_pop_bytes()==SIZE_MAX,"far return denied");
    check(mc.decode(0x1000u,interrupt_ret).near_return_pop_bytes()==SIZE_MAX,"interrupt return denied");
    check(mc.decode(0x1000u,ordinary).near_return_pop_bytes()==SIZE_MAX,"ordinary instruction supplies no return evidence");
    description x64{"x86_64-unknown-linux-gnu"}; x64.pointer_bits=64u;
    semantics::decoder other{x64}; check(bool(other),"genuine x64 MC target");
    check(other.decode(0x1000u,ret).near_return_pop_bytes()==0u &&
          other.decode(0x1000u,pop).near_return_pop_bytes()==256u,"existing x64 near-return semantics");
    ::fast_io::io::println("PASS real i386/x64 MC near-return pop; truncated, 16-bit, far and interrupt returns denied; owned DATA only");
}
