#include <uwvm2/uwvm/debugger/native_disassembly_window.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <fast_io_dsal/array.h>
#include <array>
namespace nd = uwvm2::uwvm::debugger::native_disassembly;
struct image
{
    std::array<std::uint8_t, 8> bytes{3, 0, 0, 1, 2, 0, 1, 1};
    std::array<std::uint8_t, 8> guest_code{0, 0, 0, 1, 1, 1, 1, 1};
    std::array<std::uint8_t, 0> instruction_code{};
    std::size_t size{8}; std::uintptr_t owner_begin{4096}, owner_end{4104};
    bool complete_storage() const { return size == bytes.size() && size == guest_code.size(); }
};
struct decoder
{
    explicit operator bool() const { return true; }
    nd::instruction decode(std::uintptr_t pc, std::span<std::uint8_t const> bytes) const
    {
        nd::instruction i{};
        if(bytes.empty() || bytes[0] == 0 || bytes[0] > bytes.size()) { return i; }
        i.pc=pc; i.size=bytes[0]; return i;
    }
};
void check(bool ok, fast_io::string_view what)
{ if(!ok) { fast_io::io::perrln("FAIL native breakpoint: ",what); fast_io::fast_terminate(); } }
int main()
{
    image data{}; decoder d{};
    check(bool(nd::guest_breakpoint_with(d,data,3)),"qualified boundary");
    check(bool(nd::guest_breakpoint_with(d,data,4)),"qualified multibyte instruction");
    check(!nd::guest_breakpoint_with(d,data,0),"JIT adapter prefix is not guest code");
    check(!nd::guest_breakpoint_with(d,data,1),"middle of instruction");
    check(!nd::guest_breakpoint_with(d,data,5),"middle of guest instruction");
    for(auto offset : fast_io::array<std::uint64_t,4>{8u,65537u,4096u,UINT64_MAX})
    { check(!nd::guest_breakpoint_with(d,data,offset),"out of owner, absolute address or overflow"); }
    data.guest_code[5]=0;
    check(!nd::guest_breakpoint_with(d,data,4),"every byte needs guest provenance");
    data.size=7;
    check(!nd::guest_breakpoint_with(d,data,3),"truncated mask/image");
    data.size=8; data.owner_end=4103;
    check(!nd::guest_breakpoint_with(d,data,3),"mismatched exact owner extent");
    data.owner_end=4104;data.bytes[0]=0;
    check(!nd::guest_breakpoint_with(d,data,3),"unknown earlier boundary cannot resynchronize");
    fast_io::io::println("debug_native_breakpoint_target: PASS");
}
