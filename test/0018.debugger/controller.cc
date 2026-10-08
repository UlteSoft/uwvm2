#include "../0003.utils/control/posix_test_abi.h"
#include "../0003.utils/control/buffer_helpers.h"
using namespace control_test;
#include <uwvm2/uwvm/debugger/impl.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <unistd.h>
#include <llvm-c/Disassembler.h>
#include "controller_runtime_unavailable.h"
namespace uwvm2::runtime::lib
{
    static unsigned replacement_calls{};
    llvm_jit_debug_replace_prepare_result llvm_jit_debug_prepare_function_replacement_host_api(
        std::uint_least64_t module, std::uint_least64_t function, std::uint_least64_t generation,
        std::byte const* body, std::size_t size) noexcept
    {
        ++replacement_calls;
        if(module != 7u || function != 9u || generation != 1u || body == nullptr ||
           !((size == 2u && body[0] == std::byte{} && body[1] == std::byte{0x0b}) ||
             (size == 65536u && body[0] == std::byte{} && body[32768] == std::byte{0x55} && body[65535] == std::byte{0x0b})))
        { std::abort(); }
        return {llvm_jit_debug_replace_status::unsupported_mode, 0u, nullptr};
    }
    llvm_jit_debug_replace_result llvm_jit_debug_commit_function_replacement_host_api(
        llvm_jit_debug_replace_transaction*) noexcept { std::abort(); }
    void llvm_jit_debug_discard_function_replacement_host_api(
        llvm_jit_debug_replace_transaction*) noexcept { std::abort(); }
    llvm_jit_debug_safe_point_view llvm_jit_debug_safe_points_host_api(
        std::uint_least64_t, std::uint_least64_t) noexcept
    {
        // This isolated controller test admits exactly offsets 0..255. Actual
        // LLVM safe-point bitmaps are exercised by the product debugger tests.
        static constexpr std::array<std::uint_least8_t, 32> bits = [] {
            std::array<std::uint_least8_t, 32> result{};
            result.fill(0xffu);
            return result;
        }();
        return {bits.data(), bits.size(), 256u, 1u};
    }
    llvm_jit_debug_activation_capture_owner llvm_jit_debug_capture_activation_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&) noexcept
    { return {}; } // This synthetic fixture cannot mint runtime event credentials.
    bool llvm_jit_debug_copy_native_code_host_api(llvm_jit_debug_activation_capture_owner const&, void const*,
        llvm_jit_debug_native_code_bytes& out) noexcept
    { out = {}; return false; } // Synthetic unit has no real runtime/code-owner credential.
    bool llvm_jit_debug_source_position_host_api(llvm_jit_debug_source_binding_owner const&,
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&,
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::uint_least64_t, llvm_jit_debug_source_position& out) noexcept
    { out = {}; return false; } // No actual runtime source/pause credential in this synthetic fixture.
    bool llvm_jit_debug_query_source_activation_host_api(llvm_jit_debug_activation_capture_owner const&,
        llvm_jit_debug_source_binding_owner const&, llvm_jit_debug_source_activation_snapshot& out) noexcept
    { out = {}; return false; } // Actual source/activation producers are tested separately with the runtime.
    bool llvm_jit_debug_query_activation_host_api(llvm_jit_debug_activation_capture_owner const&,
        llvm_jit_debug_activation_snapshot& out) noexcept
    { out = {}; return false; } // Detached controller fixtures cannot mint real Wasm frame authority.
    bool llvm_jit_debug_copy_source_object_host_api(llvm_jit_debug_activation_capture_owner const&,
        llvm_jit_debug_source_binding_owner const&, std::uint_least64_t, std::size_t, std::uint_least8_t,
        llvm_jit_debug_source_object_copy& out) noexcept
    { out = {}; return false; } // No privately minted source/activation read capability in this synthetic unit.
    bool llvm_jit_debug_native_position_host_api(llvm_jit_debug_activation_capture_owner const&, void const*,
        llvm_jit_debug_native_position& out) noexcept
    { out = {}; return false; } // No authentic loaded native provenance in a synthetic controller unit.
    bool llvm_jit_debug_copy_native_function_host_api(llvm_jit_debug_activation_capture_owner const&, void const*,
        llvm_jit_debug_native_function_image& out) noexcept
    { out = {}; return false; } // A synthetic controller cannot mint an actual stopped native function owner.
    bool llvm_jit_enable_debug_native_step_host_api() noexcept { return false; }
    llvm_jit_debug_native_step_site llvm_jit_capture_debug_native_step_site_host_api() noexcept
    { return {}; }
}
// The controller unit test never decodes native instructions. Abort if that
// path is unexpectedly reached; real LLVM disassembly is tested by DAP/native
// stepping against the source-bound VM. This fixture links only actual LLVM
// Support/Demangle dependencies, never the runtime or LLVM target/disassembler.
extern "C"
{
    void LLVMInitializeX86TargetInfo() { std::abort(); }
    void LLVMInitializeX86Target() { std::abort(); }
    void LLVMInitializeX86TargetMC() { std::abort(); }
    void LLVMInitializeX86Disassembler() { std::abort(); }
    LLVMDisasmContextRef LLVMCreateDisasm(char const*, void*, int,
        LLVMOpInfoCallback, LLVMSymbolLookupCallback) { std::abort(); }
    size_t LLVMDisasmInstruction(LLVMDisasmContextRef, uint8_t*, uint64_t,
        uint64_t, char*, size_t) { std::abort(); }
    void LLVMDisasmDispose(LLVMDisasmContextRef) { std::abort(); }
}
namespace dbg = uwvm2::uwvm::debugger;
namespace ctl = uwvm2::utils::control;
namespace th = uwvm2::utils::thread;
using namespace std::chrono_literals;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { fast_io::io::perrln("FAIL line=",__LINE__,": ",fast_io::mnp::os_c_str(#x)); fast_io::fast_terminate(); } } while(false)
static std::atomic<unsigned> captures{};
static thread_local unsigned worker_index{};
static uwvm2::runtime::exception::diagnostic_trace_ref capture() noexcept
{
    ++captures;
    // Test-only provider verifies on-thread capture and immutable ownership. It
    // does not claim an actual runtime/native backtrace; the CLI test covers it.
    return uwvm2::runtime::exception::diagnostic_trace::make({{7u,worker_index,u8"module\x1b[2J",u8"test"}});
}
static bool read_memory(std::uint_least64_t module,std::uint_least32_t memory,
                        std::uint_least64_t offset,void* destination,std::size_t size) noexcept
{
    constexpr unsigned char bytes[]{0x11,0x22,0x33,0x44};
    if(module!=7 || memory!=3 || offset>sizeof(bytes) || size>sizeof(bytes)-offset ||
       (size!=0 && destination==nullptr)) return false;
    if(size!=0) std::memcpy(destination,bytes+offset,size);
    return true;
}
static ctl::launch_config config()
{
    ctl::launch_config c;
    c.debug_enabled=true; c.compiler=ctl::backend::llvm; c.mode=ctl::compile_mode::full;
    c.instance[0]=1u; c.vm_process=1; c.origin=ctl::launch_origin::console;
    return c;
}
static dbg::controller_reply command(dbg::controller& c,std::string_view text,std::chrono::milliseconds timeout=2s)
{ return c.execute(dbg::parse_console_command(fast_io::string_view{text.data(),text.size()}),timeout); }
static dbg::controller_reply wait_stopped(dbg::controller& c)
{
    auto const deadline=std::chrono::steady_clock::now()+3s;
    for(;;)
    {
        auto r=c.inspect();
        if(r.execution==dbg::execution_status::stopped) return r;
        if(std::chrono::steady_clock::now()>=deadline) { fast_io::io::perrln("stop timeout");fast_io::fast_terminate(); }
        std::this_thread::yield();
    }
}
struct memory_io
{
    std::string input,output;std::size_t cursor{};
    dbg::console_io adapter()
    {
        return {this,[](void* p) noexcept { auto& io=*static_cast<memory_io*>(p);return io.cursor==io.input.size()?-1:static_cast<unsigned char>(io.input[io.cursor++]); },
            [](void* p,fast_io::string_view text) noexcept { static_cast<memory_io*>(p)->output.append(text.data(),text.size()); }};
    }
};
static_assert([] {
    std::uint64_t value{};
    return dbg::details::parse_decimal("00018446744073709551615",value) && value==UINT64_MAX &&
           !dbg::details::parse_decimal("18446744073709551616",value) && value==UINT64_MAX &&
           !dbg::details::parse_decimal("1x",value) && !dbg::details::parse_decimal("-1",value) &&
           !dbg::details::parse_decimal("+1",value) && dbg::details::parse_decimal("000",value) && value==0;
}());
static void numeric_console_tests()
{
    for(auto text:{"", "+0", "-0", " 1", "1 ", "1\t", "1.0", "1e2", "0x10", "0b10", "1_000", "1,000", "12x",
                   "18446744073709551616", "999999999999999999999999999999", "00018446744073709551616"})
    {
        std::uint64_t value{42};
        CHECK(!dbg::details::parse_decimal(fast_io::mnp::os_c_str(text),value));
        CHECK(value==42);
    }
    std::uint64_t value{42};
    CHECK(!dbg::details::parse_decimal({},value) && value==42);
    CHECK(!dbg::details::parse_decimal(fast_io::string_view{"1\0",2},value) && value==42);
    // Retain the old grammar's arbitrary redundant leading zeroes, up to the line bound.
    std::string const padded="step "+std::string(dbg::max_command_bytes-6,'0')+"1";
    auto const padded_command=dbg::parse_console_command(fast_io::string_view{padded.data(),padded.size()});
    CHECK(padded_command.kind==dbg::console_command_kind::protocol);
    CHECK(get_le<std::uint64_t>(padded_command.payload,0)==1);
    for(auto text:{"step 18446744073709551615", "bt 00018446744073709551615", "delete 18446744073709551615"})
    {
        auto const parsed=dbg::parse_console_command(fast_io::mnp::os_c_str(text));
        CHECK(parsed.kind==dbg::console_command_kind::protocol);
        CHECK(get_le<std::uint64_t>(parsed.payload,0)==UINT64_MAX);
    }
    auto const point=dbg::parse_console_command("break 0 0009 18446744073709551615");
    CHECK(point.kind==dbg::console_command_kind::protocol);
    CHECK(get_le<std::uint64_t>(point.payload,0)==0 && get_le<std::uint64_t>(point.payload,8)==9 &&
          get_le<std::uint64_t>(point.payload,16)==UINT64_MAX);
    CHECK(dbg::parse_console_command("memory 18446744073709551615 4294967295 18446744073709551615 0").kind==dbg::console_command_kind::protocol);
    for(auto text:{"step 1.0", "step 1e2", "step 0x10", "step -0", "step 000", "break 0 +1 2", "memory 0 0 0 1x"})
    { CHECK(dbg::parse_console_command(fast_io::mnp::os_c_str(text)).kind==dbg::console_command_kind::invalid); }

    dbg::controller_reply reply;
    reply.breakpoint_identifier=UINT64_MAX;
    CHECK(dbg::details::format_reply(reply,dbg::parse_console_command("break 0 0 0"))==
          "breakpoint 18446744073709551615 registered at executable Wasm expression byte offset\n");
    reply.breakpoint_identifier=0;
    reply.breakpoints.push_back({UINT64_MAX,0,9,UINT64_MAX});
    CHECK(dbg::details::format_reply(reply,dbg::parse_console_command("info breakpoints"))==
          "breakpoint 18446744073709551615 module=0 function=9 byte-offset=18446744073709551615 enabled=yes hits=0 ignore=0\n");
    reply.execution=dbg::execution_status::exited;
    for(auto code:{(std::numeric_limits<int>::min)(),-1,0,(std::numeric_limits<int>::max)()})
    {
        reply.guest_exit_code=code;
        char expected[64];fast_io::basic_obuffer_view<char> output{expected,expected+sizeof(expected)};
        // [64 writable bytes] contains the literal, signed int, newline and NUL;
        // ^^ output advances only inside this owning test array.
        fast_io::io::print(output,"guest exited: ",code,fast_io::mnp::chvw('\n'),fast_io::mnp::chvw('\0'));
        CHECK(dbg::details::format_reply(reply,dbg::parse_console_command("status"))==expected);
    }
    reply={};
    reply.source_stop_identifier=UINT64_MAX;
    auto const source_command=dbg::parse_console_command("locals source 1");
    CHECK(dbg::details::format_reply(reply,source_command)==
        "source-stop 18446744073709551615\nsource locals unavailable: no current cooperative local snapshot\n");
    reply.source_locals_available=true;
    CHECK(dbg::details::format_reply(reply,source_command)==
        "source-stop 18446744073709551615\nno active source variables\n");
    reply={};
    reply.status=static_cast<ctl::error>(65535);
    CHECK(dbg::details::format_reply(reply,{})=="error: control request rejected (code 65535)\n");
    reply={};
    reply.threads.push_back({UINT64_MAX,{0,9,UINT64_MAX,UINT64_MAX},
        uwvm2::runtime::exception::diagnostic_trace::make({{UINT64_MAX,UINT64_MAX,u8"module",u8"function"}})});
    CHECK(dbg::details::format_reply(reply,dbg::parse_console_command("bt 1"))==
          "running\nthread 18446744073709551615 module=0 function=9 byte-offset=18446744073709551615 generation=18446744073709551615\n"
          "  inline metadata unavailable: no embedded metadata bound to this source\n"
          "  #0 module=18446744073709551615 function=18446744073709551615 [module] function\n"
          "  caller source lines and byte offsets unavailable\n");
    reply={};
    auto const memory=dbg::parse_console_command("memory 0 0 0 256");
    CHECK(dbg::details::format_reply(reply,memory)=="memory: <empty>\n");
    for(unsigned byte{};byte!=256;++byte) reply.memory.push_back(static_cast<std::byte>(byte));
    CHECK(dbg::details::format_reply(reply,memory)==
          "memory: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f"
          " 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f"
          " 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f"
          " 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f"
          " 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f"
          " 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f"
          " 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f"
          " 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f"
          " 80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f"
          " 90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f"
          " a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af"
          " b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf"
          " c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf"
          " d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df"
          " e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef"
          " f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff\n");
    std::u8string name;
    for(unsigned byte{};byte!=32;++byte) name+=static_cast<char8_t>(byte);
    name+=u8"\x7f 标识";
    std::string escaped;
    dbg::details::add_name(escaped,name);
    CHECK(escaped=="\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\x09\\x0a\\x0b\\x0c\\x0d\\x0e\\x0f"
                   "\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1a\\x1b\\x1c\\x1d\\x1e\\x1f\\x7f 标识");
}
// Formatting owns copied data only. These rows test escaping/bounds, never
// manufacture a runtime source value or native step authorization.
static void source_value_format_tests()
{
    dbg::controller_reply reply{}; reply.source_stop_identifier=9u; reply.source_object_value_available=true;
    dbg::source_dwarf::object_node node{}; node.name="member\x1b"; node.type_name="int32_t";
    node.kind=dbg::source_dwarf::type_kind::scalar; node.scalar_kind=dbg::source_dwarf::numeric_kind::signed_integer;
    node.scalar_bytes=4u; node.byte_size=4u; node.bits=42u; node.value_available=true;
    reply.source_object_type.push_back(node);
    auto cmd=dbg::parse_console_command("print object"); auto text=dbg::details::format_reply(reply,cmd);
    CHECK(text.find("source-value stop=9")!=std::string::npos && text.find("value=42")!=std::string::npos &&
          text.find("member\\x1b")!=std::string::npos && text.find('\x1b')==std::string::npos);
    reply.source_object_type[0u].kind=dbg::source_dwarf::type_kind::pointer;
    reply.source_object_type[0u].bits=0x100u;
    CHECK(dbg::details::format_reply(reply,cmd).find("value=guest:0x100")!=std::string::npos);
    reply.source_object_type[0u].parent=0u;
    CHECK(dbg::details::format_reply(reply,cmd)=="error: invalid source object tree\n");
    reply={}; reply.execution=dbg::execution_status::stopped; reply.native_step_from=1u;
    reply.native_instruction.size=reply.native_instruction.bytes.size()+1u;
    cmd={dbg::console_command_kind::assembly_step};
    CHECK(dbg::details::format_reply(reply,cmd)=="error: invalid native instruction extent\n");
    reply.native_instruction.size=1u; reply.native_instruction.text.fill('a');
    CHECK(dbg::details::format_reply(reply,cmd)=="error: invalid native instruction text\n");
    reply={}; reply.execution=dbg::execution_status::stopped; cmd=dbg::parse_console_command("status");
    dbg::stopped_thread stopped{}; stopped.identifier=1u; stopped.location={7u,9u,3u,2u}; stopped.native_pc=102u;
    uwvm2::runtime::lib::llvm_jit_debug_native_position mapped{};
    mapped.status=uwvm2::runtime::lib::llvm_jit_debug_native_position_status::exact;
    mapped.pc=102u; mapped.owner_begin=100u; mapped.owner_end=200u; mapped.row_begin=100u; mapped.row_end=105u;
    mapped.participant=1u; mapped.module=7u; mapped.function=9u; mapped.function_generation=1u; mapped.runtime_epoch=2u; mapped.wasm_offset=11u;
    stopped.native_wasm_position=mapped; reply.threads.push_back(stopped);
    text=dbg::details::format_reply(reply,cmd);
    CHECK(text.find("native-wasm provenance=exact module=7 function=9 byte-offset=11")!=std::string::npos &&
          text.find("cooperative-location=last-observed")!=std::string::npos);
    CHECK(text.find("Note: Last Wasm safepoint snapshot; may differ from current native state.")!=std::string::npos);
    reply.threads[0u].trace=uwvm2::runtime::exception::diagnostic_trace::make({{7u,9u,u8"module",u8"function"}});
    auto const historical{dbg::details::format_reply(reply,dbg::parse_console_command("bt 1"))};
    auto const notice_offset{historical.find("Note: Last Wasm safepoint snapshot;")};
    auto const frame_offset{historical.find("  #0 module=7 function=9")};
    CHECK(notice_offset!=std::string::npos && frame_offset!=std::string::npos && notice_offset<frame_offset);
    reply.threads[0u].native_wasm_position->status=uwvm2::runtime::lib::llvm_jit_debug_native_position_status::unknown;
    text=dbg::details::format_reply(reply,cmd);
    CHECK(text.find("native-wasm provenance=unknown")!=std::string::npos && text.find("byte-offset=11")==std::string::npos);
    reply.threads[0u].native_wasm_position->status=uwvm2::runtime::lib::llvm_jit_debug_native_position_status::exact;
    reply.threads[0u].native_wasm_position->row_end=101u;
    CHECK(dbg::details::format_reply(reply,cmd)=="error: invalid native provenance row\n");
}
static void wasm_policy_tests()
{
    auto c=dbg::controller::create(config(),1,capture); CHECK(c && c->arm_initial_pause());
    auto bad=command(*c,"wasm-script trace wasm on gc;continue");
    CHECK(bad.status==ctl::error::malformed);
    CHECK(!command(*c,"trace wasm read").wasm_trace_enabled); // no valid-prefix side effect.
    auto script=command(*c,"wasm-script trace wasm on gc;info wasm-events");
    CHECK(script.status==ctl::error::none && script.script_replies.size()==2u && script.script_commands.size()==2u);
    CHECK(script.script_replies[0u].wasm_trace_enabled && script.script_replies[1u].wasm_trace_enabled);
    auto failed_mutation=command(*c,"wasm-script set wasm memory 0 0 1 0 bytes 7f;trace wasm off");
    CHECK(failed_mutation.script_replies.size()==1u && !failed_mutation.script_replies[0u].wasm_mutation_value.applied);
    CHECK(command(*c,"trace wasm read").wasm_trace_enabled); // failure stops before the next child.
    CHECK(command(*c,"catch wasm throw 0 all").status==ctl::error::invalid_breakpoint_target);
    CHECK(command(*c,"info registers").status==ctl::error::invalid_state); // no fabricated native trap.
    CHECK(command(*c,"print object").status==ctl::error::invalid_state);
    CHECK(command(*c,"ptype object").status==ctl::error::invalid_state); // prepared is not a source activation.
    CHECK(command(*c,"trace wasm on all").status==ctl::error::none);
    auto observer=c->observer();
    for(std::uint64_t i{};i!=550u;++i)
    {
        // Synthetic host observer seam tests bounded policy only. It cannot
        // mint actual runtime/source credentials, so opcode MUST be unavailable.
        observer.on_safe_point(observer.context.get(),1u,{0u,2u,i,1u});
    }
    auto trace=command(*c,"trace wasm read");
    CHECK(trace.status==ctl::error::none && trace.wasm_trace.size()==128u && trace.wasm_trace_overwritten==38u);
    CHECK(trace.wasm_trace.front().sequence==39u && trace.wasm_trace.back().sequence==166u);
    CHECK(trace.wasm_trace_page_available && trace.wasm_trace_oldest_sequence==39u && trace.wasm_trace_newest_sequence==550u &&
          trace.wasm_trace_next_sequence==166u && trace.wasm_trace_remaining==384u && !trace.wasm_trace_cursor_gap);
    CHECK(!trace.wasm_trace.front().instruction.available && trace.wasm_trace.front().function_generation==0u);
    auto gap=command(*c,"trace wasm read 1 7");
    CHECK(gap.status==ctl::error::none && gap.wasm_trace.size()==7u && gap.wasm_trace.front().sequence==39u &&
          gap.wasm_trace_next_sequence==45u && gap.wasm_trace_remaining==505u && gap.wasm_trace_cursor_gap);
    std::uint64_t after=trace.wasm_trace_next_sequence; std::size_t seen=trace.wasm_trace.size();
    while(after!=550u)
    {
        auto page=command(*c,fast_io::concat_std("trace wasm read ",fast_io::mnp::dec(after)));
        CHECK(page.status==ctl::error::none && !page.wasm_trace.empty() && page.wasm_trace.front().sequence==after+1u &&
              page.wasm_trace.size()<=128u && !page.wasm_trace_cursor_gap);
        seen+=page.wasm_trace.size(); after=page.wasm_trace_next_sequence;
    }
    CHECK(seen==512u);
    auto future=command(*c,"trace wasm read 18446744073709551615 1");
    CHECK(future.status==ctl::error::none && future.wasm_trace.empty() && future.wasm_trace_remaining==0u &&
          future.wasm_trace_next_sequence==UINT64_MAX);
    CHECK(command(*c,"trace wasm clear").status==ctl::error::none && command(*c,"trace wasm read").wasm_trace.empty());
    observer.on_safe_point(observer.context.get(),1u,{0u,2u,99u,1u});
    CHECK(command(*c,"trace wasm read").wasm_trace[0u].sequence==551u);
    auto text=dbg::details::format_reply(trace,dbg::parse_console_command("trace wasm read"));
    CHECK(text.find("opcode=unavailable")!=std::string::npos && text.find("overwritten=38")!=std::string::npos);
}
static void bounded_reply_tests()
{
    auto cmd=dbg::parse_console_command("trace wasm read"); dbg::controller_reply page{};
    page.wasm_trace_page_available=true; page.wasm_trace_enabled=true;
    page.wasm_trace_oldest_sequence=page.wasm_trace_newest_sequence=page.wasm_trace_next_sequence=page.wasm_trace_remaining=UINT64_MAX;
    for(std::size_t i{};i!=128u;++i)
    {
        dbg::wasm_events::record row{};
        row.sequence=row.participant=row.module=row.function=row.offset=row.runtime_epoch=row.function_generation=UINT64_MAX;
        row.instruction.primary=255u; row.instruction.extended=UINT32_MAX; row.instruction.available=row.instruction.prefixed=true;
        page.wasm_trace.push_back(row);
    }
    auto text=dbg::details::format_reply(page,cmd);
    CHECK(text.size()<=65536u && text.ends_with("wasm-events end\n") && text.find("next-after=18446744073709551615")!=std::string::npos);
    dbg::controller_reply script{};
    for(std::size_t i{};i!=16u;++i) { script.script_commands.push_back(cmd); script.script_replies.push_back(page); }
    text=dbg::details::format_reply(script,dbg::parse_console_command("wasm-script trace wasm read"));
    CHECK(text.size()<=65536u && text.ends_with("wasm-script end\n") && text.find("output-truncated")!=std::string::npos &&
          text.find("executed=16 last-command-status=0")!=std::string::npos);
    dbg::controller_reply object{}; object.source_object_type_available=true; object.source_stop_identifier=1u;
    dbg::source_dwarf::object_node node{}; node.name=fast_io::concat_std(std::string(4096u,'x')); node.type_name="large";
    node.byte_size=4u; node.parent=dbg::source_dwarf::no_record;
    for(std::size_t i{};i!=1024u;++i) { object.source_object_type.push_back(node); }
    text=dbg::details::format_reply(object,dbg::parse_console_command("ptype object"));
    CHECK(text.size()<=65536u && text.ends_with("source-type end\n") && text.find("source-object output-truncated")!=std::string::npos);
    object.source_object_type.back().parent=1024u; // even an omitted malformed node is rejected.
    CHECK(dbg::details::format_reply(object,dbg::parse_console_command("ptype object"))=="error: invalid source object tree\n");
    dbg::controller_reply locals{};locals.locals_available=true;locals.total_local_count=1u;
    dbg::local_value unavailable{};unavailable.type=0x70u;unavailable.bytes.fill(std::byte{0xffu});
    locals.locals.push_back(unavailable);
    text=dbg::details::format_reply(locals,dbg::parse_console_command("locals 1"));
    CHECK(text.find("value unavailable (local not proven initialized at this stop)")!=std::string::npos &&
          text.find("funcref=")==std::string::npos && text.find("non-null")==std::string::npos);

}
int main()
{
    bounded_reply_tests();
    source_value_format_tests();
    wasm_policy_tests();
    numeric_console_tests();
    auto invalid=config();invalid.debug_enabled=false;CHECK(!dbg::controller::create(invalid,4,capture));
    invalid=config();invalid.mode=ctl::compile_mode::lazy;CHECK(!dbg::controller::create(invalid,4,capture));
    invalid=config();invalid.origin=ctl::launch_origin::launcher_channel;CHECK(!dbg::controller::create(invalid,4,capture));
    CHECK(!dbg::controller::create(config(),0,capture));
    for(auto text:{"break 1 2 3","step 1","bt 1","delete 1","info threads","info breakpoints","pause","continue","status","wait","memory 0 0 32 256"})
    {CHECK(dbg::parse_console_command(fast_io::mnp::os_c_str(text)).kind==dbg::console_command_kind::protocol);}
    for(auto text:{"break 1 2 -1","break 1 2 18446744073709551616","step 0","bt -1","delete 0","continue extra","break 1 2 3 extra","\x1b[2J","step +1","step 1;quit",
                   "memory 0 4294967296 32 1","memory 0 0 32 257","memory 0 0 18446744073709551615 2","memory 0 0 32 -1"})
    {CHECK(dbg::parse_console_command(fast_io::mnp::os_c_str(text)).kind==dbg::console_command_kind::invalid);}
    auto const memory_command{dbg::parse_console_command("memory 7 3 65534 2")};
    CHECK(memory_command.operation==ctl::operation::read_memory && memory_command.payload_size==32);
    CHECK(get_le<std::uint64_t>(memory_command.payload,0)==7);
    CHECK(get_le<std::uint32_t>(memory_command.payload,8)==3);
    CHECK(get_le<std::uint32_t>(memory_command.payload,12)==0);
    CHECK(get_le<std::uint64_t>(memory_command.payload,16)==65534);
    CHECK(get_le<std::uint32_t>(memory_command.payload,24)==2);
    CHECK(get_le<std::uint32_t>(memory_command.payload,28)==0);
    std::string const oversized_command(513,'a');
    CHECK(dbg::parse_console_command(fast_io::string_view{oversized_command.data(),oversized_command.size()}).kind==dbg::console_command_kind::invalid);
    CHECK(dbg::parse_console_command(fast_io::string_view("help\0quit",9)).kind==dbg::console_command_kind::invalid);
    CHECK(dbg::parse_console_command("replace 1").kind==dbg::console_command_kind::invalid);
    auto const replace=dbg::parse_console_command("replace 7 9 1 /tmp/body.wasm");
    CHECK(replace.kind==dbg::console_command_kind::replacement_file && replace.replacement_module==7u &&
          replace.replacement_function==9u && replace.replacement_generation==1u &&
          std::string_view(replace.replacement_path.data(),replace.replacement_path_size)=="/tmp/body.wasm");
    for(auto text:{"replace 7 9 0 /tmp/body", "replace 7 9 1", "replace 7 -9 1 /tmp/body"})
    { CHECK(dbg::parse_console_command(fast_io::mnp::os_c_str(text)).kind==dbg::console_command_kind::invalid); }
    memory_io bounds{std::string(dbg::max_input_command_bytes+1u,'x')+"continue\nhelp\n", {}, {}};
    CHECK(dbg::read_console_line(bounds.adapter()).status==dbg::line_status::oversized);
    auto line=dbg::read_console_line(bounds.adapter());CHECK(fast_io::string_view(line.bytes.data(),line.size)=="help");
    CHECK(dbg::read_console_line(bounds.adapter()).status==dbg::line_status::end);

    for(unsigned round{};round!=16;++round)
    {
        auto c=dbg::controller::create(config(),2,capture,read_memory);CHECK(c && c->status()==ctl::error::none);
        CHECK(c->arm_initial_pause());CHECK(c->inspect().threads.empty());
        auto malformed_source_step{dbg::parse_console_command("step source 1")};
        CHECK(malformed_source_step.kind == dbg::console_command_kind::source_step);
        malformed_source_step.source_policy = static_cast<dbg::source_step_policy>(255u);
        CHECK(c->execute(malformed_source_step).status == ctl::error::malformed);
        auto malformed_wasm_step{dbg::parse_console_command("step wasm 1 over")};
        CHECK(malformed_wasm_step.kind == dbg::console_command_kind::wasm_step);
        malformed_wasm_step.wasm_policy = static_cast<dbg::wasm_step_policy>(255u);
        CHECK(c->execute(malformed_wasm_step).status == ctl::error::malformed);
        auto const initial_stop=c->inspect().stop_identifier;
        CHECK(initial_stop!=0 && c->inspect().stop_identifier==initial_stop);
        CHECK(dbg::details::format_reply(c->inspect(),dbg::parse_console_command("status")).find("stop-id ")!=std::string::npos);
        auto memory=command(*c,"memory 7 3 1 2");
        CHECK(memory.status==ctl::error::none && memory.memory.size()==2 &&
              memory.memory[0]==std::byte{0x22} && memory.memory[1]==std::byte{0x33});
        CHECK(command(*c,"memory 7 3 4 1").status==ctl::error::invalid_memory_range);
        CHECK(command(*c,"memory 7 3 0 0").status==ctl::error::none);
        CHECK(command(*c,"memory 7 3 5 0").status==ctl::error::invalid_memory_range);
        CHECK(command(*c,"wait",5ms).execution==dbg::execution_status::stopped);
        auto add=command(*c,"break 7 9 2");CHECK(add.status==ctl::error::none && add.breakpoint_identifier==1);
        CHECK(command(*c,"break 7 9 2").breakpoint_identifier==1);
        CHECK(command(*c,"info breakpoints").breakpoints.size()==1);
        std::atomic<unsigned> admitted{},ready{};std::atomic<bool> finish{},unavailable_requested{};
        auto observer=c->observer();
        std::vector<std::thread> workers;
        for(unsigned id{};id!=2;++id)
        {
            workers.emplace_back([&,id]
            {
                worker_index=id;
                auto lease=c->domain()->enter();if(!lease) std::abort();
                ++admitted;++ready;
                while(ready.load()!=2) std::this_thread::yield();
                for(std::uint64_t offset{};!finish.load();offset=(offset+1)%16)
                {
                    th::cooperative_pause_location loc{7,9,offset,1};
                    observer.on_safe_point(observer.context.get(),lease.identifier(),loc);
                    lease.poll(loc, [&]() noexcept {
                        std::array<std::byte,16> local{}; local[0]=std::byte{73};
                        bool const unavailable=unavailable_requested.load();
                        std::array<std::uint_least8_t,1> types{static_cast<std::uint_least8_t>(unavailable ? 0x70u : 0x7fu)};
                        std::array<std::uint_least8_t,1> availability{static_cast<std::uint_least8_t>(unavailable ? 0u : 1u)};
                        uwvm2::runtime::lib::llvm_jit_debug_local_view view{};
                        // Component-only callback witness: flag0 must NEVER
                        // touch this deliberately unreadable carrier pointer.
                        // It does not mint a genuine runtime/source capability.
                        view.values=unavailable ? reinterpret_cast<std::byte const*>(std::uintptr_t{1u}) : local.data();
                        view.types=types.data();view.captured_count=view.total_count=1u;
                        view.availability=availability.data();
                        observer.on_before_park(observer.context.get(),lease.identifier(),loc,view);
                    });
                }
            });
        }
        CHECK(admitted.load()==0); // Admission remains closed at prepared pause.
        CHECK(command(*c,"continue").status==ctl::error::none);
        auto stopped=wait_stopped(*c);CHECK(stopped.threads.size()==2);CHECK(stopped.reason==dbg::stop_reason::breakpoint);
        CHECK(stopped.stop_identifier>initial_stop);
        auto chosen=stopped.threads.front().identifier;
        auto loc=stopped.threads.front().location;
        auto stepped=command(*c,"step "+std::to_string(chosen));
        CHECK(stepped.status==ctl::error::none && !stepped.timed_out && stepped.execution==dbg::execution_status::stopped);
        bool found{};for(auto const& t:stepped.threads) if(t.identifier==chosen){found=true;CHECK(t.location.offset==(loc.offset+1)%16);}
        CHECK(found);CHECK(stepped.reason==dbg::stop_reason::step);
        CHECK(stepped.stop_identifier>stopped.stop_identifier && c->inspect().stop_identifier==stepped.stop_identifier);
        auto trace=command(*c,"bt "+std::to_string(chosen));CHECK(trace.threads.size()==1 && trace.threads[0].trace);
        CHECK(trace.threads[0].trace->frames().size()==1);
        CHECK(trace.stop_identifier==stepped.stop_identifier);
        auto locals=command(*c,"locals "+std::to_string(chosen));
        CHECK(locals.status==ctl::error::none && locals.locals_available && locals.total_local_count==1 &&
              locals.locals.size()==1 && locals.locals[0].type==0x7fu && locals.locals[0].available && locals.locals[0].bytes[0]==std::byte{73});
        auto const locals_command="locals "+std::to_string(chosen);
        CHECK(dbg::details::format_reply(locals,dbg::parse_console_command(
                  fast_io::string_view{locals_command.data(),locals_command.size()})).find("local 0 i32=73\n")!=std::string::npos);
        unavailable_requested=true;
        auto unknown_step=command(*c,fast_io::concat_std("step ",fast_io::mnp::dec(chosen)));
        CHECK(unknown_step.status==ctl::error::none && !unknown_step.timed_out && unknown_step.execution==dbg::execution_status::stopped);
        auto unknown_locals=command(*c,fast_io::concat_std("locals ",fast_io::mnp::dec(chosen)));
        CHECK(unknown_locals.status==ctl::error::none && unknown_locals.locals_available && unknown_locals.locals.size()==1u &&
              unknown_locals.locals[0u].type==0x70u && !unknown_locals.locals[0u].available);
        for(auto byte:unknown_locals.locals[0u].bytes) { CHECK(byte==std::byte{}); }
        auto unknown_text=dbg::details::format_reply(unknown_locals,dbg::parse_console_command("locals 1"));
        CHECK(unknown_text.find("value unavailable")!=std::string::npos && unknown_text.find("funcref=")==std::string::npos);
        CHECK(command(*c,"locals 999999").status==ctl::error::invalid_state);
        auto const backtrace_command="bt "+std::to_string(chosen);
        auto formatted=dbg::details::format_reply(trace,dbg::parse_console_command(fast_io::string_view{backtrace_command.data(),backtrace_command.size()}));
        CHECK(formatted.find('\x1b')==std::string::npos && formatted.find("\\x1b")!=std::string::npos);
        CHECK(command(*c,"step 999999").status==ctl::error::invalid_state);
        CHECK(command(*c,"delete 1").status==ctl::error::none);
        CHECK(command(*c,"info breakpoints").breakpoints.empty());
        CHECK(command(*c,"continue").status==ctl::error::none);
        auto paused=command(*c,"pause");CHECK(!paused.timed_out && paused.execution==dbg::execution_status::stopped);
        CHECK(paused.threads.size()==2);
        CHECK(paused.stop_identifier>stepped.stop_identifier);
        finish=true;CHECK(command(*c,"continue").status==ctl::error::none);
        for(auto& worker:workers)worker.join();
        c->notify_guest_exit(17);CHECK(c->inspect().execution==dbg::execution_status::exited);
        CHECK(c->inspect().guest_exit_code==17);
    }
    {
        auto c=dbg::controller::create(config(),1,capture);
        std::atomic<bool> active{},release{};
        std::thread blocked([&]{auto participant=c->domain()->enter();active=true;while(!release)std::this_thread::yield();});
        while(!active)std::this_thread::yield();
        auto observation=command(*c,"wait",5ms);
        CHECK(observation.timed_out && observation.execution==dbg::execution_status::running);
        CHECK(!c->domain()->pause_requested());
        auto timed=command(*c,"pause",5ms);
        CHECK(timed.timed_out && timed.execution==dbg::execution_status::running);
        CHECK(!c->domain()->pause_requested());
        CHECK(command(*c,"status").status==ctl::error::none);
        release=true;blocked.join();
    }
    {
        auto c=dbg::controller::create(config(),1,capture);CHECK(c->arm_initial_pause());
        for(unsigned i{};i!=256;++i)CHECK(command(*c,"break 1 2 "+std::to_string(i)).status==ctl::error::none);
        CHECK(command(*c,"break 1 3 0").status==ctl::error::exhausted);
        CHECK(command(*c,"delete 1").status==ctl::error::none);
        CHECK(command(*c,"break 1 2 1").breakpoint_identifier==2); // Duplicate must not consume the earlier hole.
        CHECK(command(*c,"info breakpoints").breakpoints.size()==255);
        memory_io io{"help\nlocals\nserver\nwat\nstatus\nquit\n", {}, {}};
        CHECK(dbg::run_console(*c,io.adapter())==dbg::console_exit::terminate_process);
        CHECK(io.output.find("(uwvm-debug) ")!=std::string::npos);
        CHECK(io.output.find("prepared; no Wasm instruction executed")!=std::string::npos);
        CHECK(io.output.find("error: command is unsupported")!=std::string::npos);
        CHECK(io.output.find("error: invalid command")!=std::string::npos);
        CHECK(dbg::run_console(*c,dbg::console_io{})==dbg::console_exit::input_failure);
    }
    {
        char path[]{"/dev/shm/uwvm-debug-replace-XXXXXX"};
        int const fd{control_test::posix_abi::mkstemp_noexcept(path)}; CHECK(fd>=0);
        unsigned char const body[]{0u,0x0bu};
        CHECK(control_test::posix_abi::write_noexcept(fd,body,sizeof(body))==static_cast<ssize_t>(sizeof(body)));
        CHECK(control_test::posix_abi::close_noexcept(fd)==0);
        auto allowed=config(); allowed.replacement_enabled=true;
        auto c=dbg::controller::create(allowed,1,capture); CHECK(c && c->arm_initial_pause());
        auto const request=std::string{"replace 7 9 1 "}+path;
        auto const rejected=command(*c,request);
        CHECK(rejected.status==ctl::error::unsupported_command &&
              uwvm2::runtime::lib::replacement_calls==1u && rejected.replacement_generation==0u);
        std::vector<unsigned char> maximum_body(65536u,0x55u);
        maximum_body.front()=0u; maximum_body.back()=0x0bu;
        auto file=control_test::posix_abi::open_noexcept(path,O_WRONLY|O_TRUNC|O_CLOEXEC); CHECK(file>=0);
        CHECK(control_test::posix_abi::write_noexcept(file,maximum_body.data(),maximum_body.size())==static_cast<ssize_t>(maximum_body.size()));
        CHECK(control_test::posix_abi::close_noexcept(file)==0);
        CHECK(command(*c,request).status==ctl::error::unsupported_command &&
              uwvm2::runtime::lib::replacement_calls==2u);
        maximum_body.push_back(0u);
        file=control_test::posix_abi::open_noexcept(path,O_WRONLY|O_TRUNC|O_CLOEXEC); CHECK(file>=0);
        CHECK(control_test::posix_abi::write_noexcept(file,maximum_body.data(),maximum_body.size())==static_cast<ssize_t>(maximum_body.size()));
        CHECK(control_test::posix_abi::close_noexcept(file)==0);
        CHECK(command(*c,request).status==ctl::error::invalid_replacement_source &&
              uwvm2::runtime::lib::replacement_calls==2u);
        auto const link=std::string{path}+".link";
        CHECK(control_test::posix_abi::symlink_noexcept(path,link.c_str())==0);
        CHECK(command(*c,std::string{"replace 7 9 1 "}+link).status==ctl::error::invalid_replacement_source);
        CHECK(control_test::posix_abi::unlink_noexcept(link.c_str())==0 && control_test::posix_abi::unlink_noexcept(path)==0);
        CHECK(command(*c,request).status==ctl::error::invalid_replacement_source);
    }
    CHECK(captures.load()>0);
    fast_io::io::println("PASS debugger controller: ",checks," checks, 16 two-participant rounds; captures=",captures.load());
}
