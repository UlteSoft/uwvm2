#include <uwvm2/uwvm/debugger/console.h>
#include <cstdio>
#include <cstdlib>
using namespace uwvm2::uwvm::debugger;
static void check(bool good,char const* why) { if(!good) { std::fprintf(stderr,"FAIL %s\n",why);std::abort(); } }
int main()
{
    native_registers::display_snapshot value{};value.machine=native_registers::architecture::aarch64;value.word_bits=64u;
    value.known_bits[0]=32u;value.values[0]=0xdeadbeef01234567u;
    auto x=console_inspection::register_preview(value,"x0");
    check(x.available && x.value=="0x????????01234567" && x.type=="u64","unknown high bits never exposed");
    x=console_inspection::register_preview(value,"w0");check(x.available && x.value=="0x01234567" && x.type=="u32","W register projection");
    check(!console_inspection::register_preview(value,"x30").available,"uncaptured LR unavailable");
    value.known_bits[30]=64u;value.values[30]=123u;
    check(console_inspection::register_preview(value,"x30").value==console_inspection::register_preview(value,"lr").value,"LR alias");
    value.floating.available=true;auto& f=value.floating.values[0];f.width=4u;f.bytes.fill(0xde);f.bytes[0]=0;f.bytes[1]=0;f.bytes[2]=0xc0;f.bytes[3]=0x3f;
    x=console_inspection::register_preview(value,"s0");check(x.available && x.type=="f32" && x.value=="1.5","captured scalar float");
    x=console_inspection::register_preview(value,"v0");check(x.available && x.value=="0x????????????????????????3fc00000","uncaptured SIMD lanes stay unknown");
    value.floating.available=false;check(!console_inspection::register_preview(value,"s0").available,"unavailable FP bank stays unavailable");
    value.machine=native_registers::architecture::x86_64;x=console_inspection::register_preview(value,"eax");
    check(x.available && x.value=="0x01234567","x86 scalar register alias");
    controller_reply reply{};reply.registers_stop_identifier=7u;reply.disassembly_code.participant=1u;reply.disassembly_code.pc=0x1234u;
    value.machine=native_registers::architecture::aarch64;value.known_bits[30]=0u;
    auto const pc{native_registers::pc_index(value.machine)};value.known_bits[pc]=64u;value.values[pc]=0x1234u;reply.registers=value;
    auto line{[](fast_io::string_view name,fast_io::string_view type,fast_io::string_view text,bool)
    { return fast_io::concat_fast_io(name," ",type," = ",text,"\n"); }};
    auto panel{[&](std::uint64_t thread=1u,std::uint64_t stop=7u,std::uint64_t address=0x1234u)
    { return console_inspection::register_panel(reply,thread,stop,address,line); }};
    auto text{panel()};
    check(text.subview(0u).starts_with("pc u64 = 0x0000000000001234\n"),"PC visible on first pane row");
    check(text.find("x0 u64 = 0x????????01234567")!=fast_io::containers::npos,"pane preserves unknown high bits");
    check(text.find("x1 ")==fast_io::containers::npos && text.find("unavailable")==fast_io::containers::npos,"omit unavailable register rows");
    check(panel(2u)=="Registers unavailable at this native stop\n" && panel(1u,8u)==panel(2u) && panel(1u,7u,0x1238u)==panel(2u),"reject mismatched participant stop and PC");
    check(panel(1u,7u,0u)=="Wasm stop; use si or ni for native registers\n","Wasm stop discards previous values");
    reply.registers.known_bits[0]=0u;text=panel();
    check(text.find("No other mapped values")!=fast_io::containers::npos,"explain PC-only snapshot once");
    reply.registers.known_bits[pc]=0u;check(panel()=="Registers unavailable at this native stop\n","reject unqualified PC");
    std::puts("PASS compact register aliases, scalar floats and unknown-bit preservation");
}
