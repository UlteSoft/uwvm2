#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/wasm_events.h>
#include <array>
#include <limits>
#include <span>
namespace dbg = uwvm2::uwvm::debugger;
namespace ev = dbg::wasm_events;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { fast_io::io::perrln("FAIL wasm events line=",fast_io::mnp::dec(__LINE__),": ",fast_io::mnp::os_c_str(#x));fast_io::fast_terminate(); } } while(false)
template<std::size_t N>
static ev::opcode opcode(std::array<unsigned char,N> const& input, std::uint64_t offset=0u)
{
    // Unit-only known byte array; the product independently authenticates each
    // current emitted instruction boundary through its private source binding.
    return ev::decode({reinterpret_cast<std::byte const*>(input.data()),N},offset);
}
int main()
{
    // Independently enumerated Core 3.0 binary opcodes, including new GC,
    // exception references, tail calls, table/memory and relaxed SIMD prefixes.
    for(unsigned char op : {0x08u,0x0au})
    { auto value=opcode(std::array{op}); CHECK(value.available && ev::matches(ev::category::throw_instruction,value)); }
    auto try_table=opcode(std::array<unsigned char,2>{0x1fu,0x40u});
    CHECK(ev::matches(ev::category::exception_instruction,try_table) && !ev::matches(ev::category::throw_instruction,try_table));
    for(unsigned char op : {0x12u,0x13u,0x14u,0x15u})
    { CHECK(ev::matches(ev::category::call_instruction,opcode(std::array{op}))); }
    auto gc_cast=opcode(std::array<unsigned char,2>{0xfbu,24u});
    CHECK(gc_cast.extended==24u && ev::matches(ev::category::gc_instruction,gc_cast) && ev::matches(ev::category::control_instruction,gc_cast));
    auto gc_struct=opcode(std::array<unsigned char,2>{0xfbu,0u});
    CHECK(ev::matches(ev::category::gc_instruction,gc_struct) && ev::matches(ev::category::reference_instruction,gc_struct));
    auto table=opcode(std::array<unsigned char,2>{0xfcu,15u});
    CHECK(ev::matches(ev::category::table_instruction,table) && !ev::matches(ev::category::memory_instruction,table));
    auto memory=opcode(std::array<unsigned char,2>{0xfcu,10u});
    CHECK(ev::matches(ev::category::memory_instruction,memory));
    auto atomic=opcode(std::array<unsigned char,2>{0xfeu,0u});
    CHECK(ev::matches(ev::category::atomic_instruction,atomic) && ev::matches(ev::category::memory_instruction,atomic));
    auto fence=opcode(std::array<unsigned char,2>{0xfeu,3u});
    CHECK(ev::matches(ev::category::atomic_instruction,fence) && !ev::matches(ev::category::memory_instruction,fence));
    auto simd_load=opcode(std::array<unsigned char,2>{0xfdu,92u});
    CHECK(ev::matches(ev::category::simd_instruction,simd_load) && ev::matches(ev::category::memory_instruction,simd_load));
    auto relaxed=opcode(std::array<unsigned char,3>{0xfdu,0x80u,2u});
    CHECK(relaxed.extended==256u && ev::matches(ev::category::simd_instruction,relaxed) && !ev::matches(ev::category::memory_instruction,relaxed));
    CHECK(!ev::decode({},0u).available);
    CHECK(ev::matches(ev::category::all,{})); // all records an actual safe point even when its bounded opcode copy is unavailable.
    CHECK(!ev::matches(ev::category::gc_instruction,{}));
    CHECK(!opcode(std::array<unsigned char,1>{0xfbu}).available);
    CHECK(!opcode(std::array<unsigned char,2>{0xfbu,0x80u}).available);
    CHECK(!opcode(std::array<unsigned char,6>{0xfbu,0xffu,0xffu,0xffu,0xffu,0x10u}).available);
    CHECK(!opcode(std::array<unsigned char,2>{0x08u,0u},2u).available);
    CHECK(!opcode(std::array<unsigned char,2>{0x08u,0u},UINT64_MAX).available);
    auto parsed=dbg::parse_console_command("catch wasm throw 0 all");
    CHECK(parsed.kind==dbg::console_command_kind::wasm_event && parsed.wasm_request.all_functions && parsed.wasm_request.module==0u);
    CHECK(dbg::parse_console_command("catch wasm thrown 0 all").kind==dbg::console_command_kind::invalid);
    CHECK(dbg::parse_console_command("trace wasm on gc").wasm_request.event==ev::category::gc_instruction);
    CHECK(dbg::parse_console_command("trace wasm on gc extra").kind==dbg::console_command_kind::invalid);
    CHECK(dbg::parse_console_command("trace wasm read").wasm_request.maximum_records==128u);
    auto page=dbg::parse_console_command("trace wasm read 18446744073709551615 7");
    CHECK(page.kind==dbg::console_command_kind::wasm_event && page.wasm_request.after_sequence==UINT64_MAX && page.wasm_request.maximum_records==7u);
    for(auto const text : {"trace wasm read 1 0","trace wasm read 1 129","trace wasm read -1","trace wasm read 18446744073709551616",
        "trace wasm read 0 128 extra","trace wasm on gc extra"})
    { CHECK(dbg::parse_console_command(fast_io::string_view{fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::invalid); }

    CHECK(dbg::parse_console_command("disable wasm-event 18446744073709551616").kind==dbg::console_command_kind::invalid);
    auto script=dbg::parse_wasm_script(dbg::parse_console_command("wasm-script trace wasm on gc; catch wasm throw 0 all; info wasm-events"));
    CHECK(script.valid && script.size==3u);
    CHECK(dbg::parse_wasm_script(dbg::parse_console_command("wasm-script replace 0 0 1 body; info wasm-events")).valid);
    auto mutation_script=dbg::parse_wasm_script(dbg::parse_console_command("wasm-script set wasm memory 0 0 1 0 bytes 7f; operands 1"));
    CHECK(mutation_script.valid && mutation_script.size==2u && mutation_script.commands[0].kind==dbg::console_command_kind::wasm_mutation);
    for(auto const text : {"wasm-script trace wasm on; invalid", "wasm-script status; continue", "wasm-script status; replace 0 0 0 x",
        "wasm-script status; wasm-script status", "wasm-script status;", "wasm-script ;status", "wasm-script status;wait",
        "wasm-script step wasm 1 over", "wasm-script set wasm memory 0 0 1 0 bytes zz; status"})
    { CHECK(!dbg::parse_wasm_script(dbg::parse_console_command(fast_io::string_view{fast_io::mnp::os_c_str(text)})).valid); }
    CHECK(dbg::parse_console_command("info registers").kind==dbg::console_command_kind::assembly_registers);
    CHECK(dbg::parse_console_command("info registers $pc").register_name_size==3u);
    CHECK(dbg::parse_console_command("info registers 1 2 pc").payload_size==8u);
    CHECK(dbg::parse_console_command("info registers 0 2").kind==dbg::console_command_kind::invalid);
    CHECK(dbg::parse_console_command("ptype object").kind==dbg::console_command_kind::source_type);
    CHECK(dbg::parse_console_command("ptype 1 2 object").payload_size==8u);
    CHECK(dbg::parse_console_command("print object").kind==dbg::console_command_kind::source_value);
    CHECK(dbg::parse_console_command("print object.items[7].member").kind==dbg::console_command_kind::source_value);
    CHECK(dbg::parse_console_command("ptype 1 2 object.member").kind==dbg::console_command_kind::source_type);
    CHECK(dbg::parse_console_command("p 1 2 object").payload_size==8u);
    CHECK(dbg::parse_console_command("print 0 2 object").kind==dbg::console_command_kind::invalid);
    CHECK(dbg::parse_wasm_script(dbg::parse_console_command("wasm-script ptype object; print object")).valid);
    ev::trace_buffer ring{};
    for(std::uint64_t i{}; i!=520u; ++i)
    { ev::record entry{}; entry.participant=1u; entry.offset=i; CHECK(ring.append(entry)); }
    CHECK(ring.size()==512u && ring.overwritten()==8u && ring.at(0u).sequence==9u && ring.at(511u).sequence==520u);
    ring.clear();CHECK(ring.size()==0u && ring.overwritten()==0u);
    CHECK(ring.append({}) && ring.at(0u).sequence==521u); // clearing never resurrects an earlier event label.
    fast_io::io::println("PASS bounded Wasm event grammar/classification: ",fast_io::mnp::dec(checks)," checks (source-binding/runtime acceptance tested separately)");
}
