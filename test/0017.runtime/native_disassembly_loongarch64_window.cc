// Real LoongArch64 MC on owned DATA. No kernel stop, JIT owner or resume authority.
#include <uwvm2/uwvm/debugger/native_disassembly_window.h>
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <uwvm2/uwvm/debugger/native_wasm_step_boundary.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace nd = ::uwvm2::uwvm::debugger::native_disassembly;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("LoongArch64 MC window failure line=",__LINE__); ::fast_io::fast_terminate(); } } while(false)
struct description
{
    bool available{true}, little_endian{true}; unsigned description_version{1u}, pointer_bits{64u};
    ::std::size_t maximum_instruction_bytes{4u}, minimum_instruction_alignment{4u};
    ::std::array<char,64u> triple{}, cpu{}, features{};
    ::std::size_t triple_size{}, cpu_size{}, features_size{};
    explicit description(char const* value)
    { triple_size=::fast_io::cstr_len(value); CHECK(triple_size<triple.size()); ::std::memcpy(triple.data(),value,triple_size+1u); }
};
struct counted
{
    nd::decoder actual;
    ::std::size_t calls{};
    explicit counted(description const& target):actual{target} {}
    [[nodiscard]] explicit operator bool() const noexcept { return bool(actual); }
    [[nodiscard]] ::std::size_t fixed_instruction_bytes() const noexcept { return actual.fixed_instruction_bytes(); }
    nd::instruction decode(::std::uintptr_t pc,::std::span<::std::uint8_t const> bytes) noexcept
    { ++calls; return actual.decode(pc,bytes); }
};
int main()
{
    description loong64{"loongarch64-unknown-linux-gnu"};
    loong64.minimum_instruction_alignment=1u; // Actual ELF assembler estimate, not LoongArch64 instruction width.
    counted mc{loong64};
    CHECK(mc && mc.fixed_instruction_bytes()==4u);
    constexpr ::std::array<::std::uint8_t,4u> nop{0x00,0x00,0x40,0x03};
    CHECK(!mc.decode(0x1001u,nop)); mc.calls=0u;
    ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder semantics{loong64};
    CHECK(semantics && semantics.decode(0x1000u,nop).safe_for_single_instruction());
    CHECK(!semantics.decode(0x1001u,nop) && !semantics.decode(0x1002u,nop));
    // Returning Wasm calls acquire their genuine patchable entry slot through
    // DBAR. Only LLVM's bounded ordering hints may advance to the next owned
    // instruction; they grant no public ABI/memory/operand observation.
    for(auto const hint: ::std::array<::std::uint32_t,5u>{0u,0x10u,0x12u,0x14u,0x700u})
    {
        auto const word{0x38720000u|hint};
        ::std::array<::std::uint8_t,8u> code{};
        for(unsigned b{};b!=4u;++b) { code[b]=static_cast<::std::uint8_t>(word>>(8u*b));code[4u+b]=nop[b]; }
        auto const decoded{semantics.decode(0x1000u,code)};
        CHECK(decoded && decoded.semantics().size==4u && decoded.safe_for_single_instruction());
        CHECK(!decoded.safe_for_public_display() && !decoded.safe_for_call_continuation() && !decoded.destination());
        auto const step{::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(mc,semantics,code,0x1000u,0x1008u,0x1000u,false)};
        CHECK(step && step.first_successor==0x1004u && step.second_successor==0u);
        CHECK(!::uwvm2::uwvm::debugger::native_wasm_step_boundary::prepare(mc,semantics,
            {code.data(),4u},0x1000u,0x1004u,0x1000u,false));
    }
    for(auto const word: ::std::array<::std::uint32_t,6u>{
        0x38720001u,0x3872001fu,0x38727fffu,0x38728000u,0x002b0000u,0x002a0000u})
    {
        ::std::array<::std::uint8_t,4u> code{};
        for(unsigned b{};b!=4u;++b) { code[b]=static_cast<::std::uint8_t>(word>>(8u*b)); }
        CHECK(!semantics.decode(0x1000u,code).safe_for_single_instruction());
    }
    // A large initialization prefix must not turn a two-slot current query
    // into work proportional to the whole native function.
    mc.calls=0u;
    ::std::array<::std::uint8_t,80008u> owned{};
    for(::std::size_t i{};i<owned.size();i+=4u) { ::std::memcpy(owned.data()+i,nop.data(),4u); }
    owned[0u]=owned[1u]=owned[2u]=owned[3u]=0xffu; // Unknown earlier slot, never an X86 boundary guess.
    auto const page{nd::decode_window_with(mc,owned,0x1000u,0x1000u+80000u,0,0,2u)};
    CHECK(page.available && page.count==2u && mc.calls==2u);
    CHECK(page.instructions[0u].pc==0x1000u+80000u && page.instructions[0u].size==4u &&
        ::std::strstr(page.instructions[0u].text.data(),"nop")!=nullptr && page.instructions[1u].size==4u);
    mc.calls=0u;
    CHECK(!nd::decode_window_with(mc,owned,0x1000u,0x1001u+80000u,0,0,1u).available && mc.calls==0u);
    CHECK(!nd::decode_window_with(mc,owned,0x1001u,0x1001u+80000u,0,0,1u).available && mc.calls==0u);
    CHECK(!nd::decode_window_with(mc,{owned.data(),owned.size()-1u},0x1000u,0x1000u+80000u,0,0,1u).available);
    auto const end{nd::decode_window_with(mc,owned,0x1000u,0x1000u+80004u,0,0,2u)};
    CHECK(end.available && end.instructions[0u] && !end.instructions[1u] && end.instructions[1u].pc==0u);
    owned[80000u]=owned[80001u]=owned[80002u]=owned[80003u]=0xffu;
    auto const unknown{nd::decode_window_with(mc,owned,0x1000u,0x1000u+80000u,0,0,2u)};
    CHECK(!unknown.available && !unknown.instructions[0u] && !unknown.instructions[1u]);
    ::std::memcpy(owned.data()+80000u,nop.data(),4u);
    owned[80004u]=owned[80005u]=owned[80006u]=owned[80007u]=0xffu;
    auto const suffix{nd::decode_window_with(mc,owned,0x1000u,0x1000u+80000u,0,0,3u)};
    CHECK(suffix.available && suffix.instructions[0u] && !suffix.instructions[1u] && !suffix.instructions[2u]);
    // An explicit TEXT/DATA mask retains its complete forward proof; this
    // optimization never restarts at an unknown earlier TEXT slot or DATA.
    ::std::array<::std::uint8_t,12u> mask{};mask.fill(1u);
    CHECK(!nd::decode_window_with(mc,{owned.data(),12u},0x1000u,0x1004u,0,0,1u,mask).available);
    // A caller's 4/4 bounds cannot claim a fixed-width X86 ISA.
    description forged{"x86_64-unknown-linux-gnu"}; counted variable{forged};
    CHECK(variable.fixed_instruction_bytes()==0u);
    ::fast_io::io::println("PASS LoongArch64 real MC bounded current slots, alignment, unknown/fillers/mapping and forged-ISA refusal; owned DATA only");
}
