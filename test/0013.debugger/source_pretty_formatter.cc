// Detached formatter DATA only. No paused guest, capture or read authority.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <string>
#include <string_view>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace dwarf = dbg::source_dwarf;
static void require(bool okay, ::fast_io::string_view why)
{ if(!okay) { ::fast_io::io::perrln("source pretty formatter: ", why); ::fast_io::fast_terminate(); } }
static bool contains(::std::string const& text, ::std::string_view value)
{ return ::std::string_view{text}.find(value) != ::std::string_view::npos; }
int main()
{
    dwarf::object_node node{};
    node.name = "[0]"; node.type_name = "int"; node.depth = 1u; node.byte_offset = 4096u; node.byte_size = 4u;
    node.kind = dwarf::type_kind::scalar; node.scalar_kind = dwarf::numeric_kind::signed_integer;
    node.bits = 7u; node.scalar_bytes = 4u; node.value_available = true; node.detached_copy = true;
    node.pretty_available = true; node.pretty_summary = "seven, text=display \"quote\" \\ ";
    node.pretty_summary.push_back('\x1b'); node.pretty_summary.push_back('\n');
    auto text{::fast_io::concat_std(dwarf::object_details_of(node))};
    require(text.starts_with("  [0]: type=int, offset=4096, bytes=4, copied-child, pretty=\""), "flags precede structural suffix in canonical order");
    require(contains(text,"\\\"quote\\\" \\\\ \\x1b\\x0a\", value=7"), "summary preserves delimiters and escapes quotes/control bytes");
    require(text.find('\x1b') == text.npos && text.find('\n') == text.npos, "formatter emits no raw terminal controls from summary");
    node.pretty_summary.assign(4097u, 'x');
    text = ::fast_io::concat_std(dwarf::object_details_of(node));
    require(contains(text,::std::string(4096u,'x')+"... (truncated)\", value=7") && !contains(text,::std::string(4097u,'x')),
        "summary shares the bounded metadata escaping channel");
    node.pretty_available = false; node.detached_copy = false;
    text = ::fast_io::concat_std(dwarf::object_details_of(node));
    require(text == "  [0]: type=int, offset=4096, bytes=4, value=7", "raw formatter grammar remains exact");
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    dbg::controller_reply reply{}; reply.source_stop_identifier = 41u; reply.source_object_value_available = true;
    dwarf::object_node root{}; root.kind = dwarf::type_kind::structure; root.name = "object"; root.type_name = "vector<int>";
    root.byte_size = 12u; root.pretty_available = true; root.pretty_summary = "size=8 capacity=8 [7, ...]";
    root.pretty_summary.push_back('\x1b'); root.pretty_truncated = true; root.pretty_omitted = 7u;
    reply.source_object_type.push_back(root);
    dwarf::object_node implementation{}; implementation.parent=0u; implementation.depth=1u; implementation.name="_M_start";
    implementation.kind=dwarf::type_kind::pointer; implementation.type_name="int*"; implementation.value_available=true;
    implementation.scalar_kind=dwarf::numeric_kind::unsigned_integer; implementation.scalar_bytes=4u; implementation.byte_size=4u; implementation.bits=512u;
    reply.source_object_type.push_back(implementation);
    node.depth = 1u; node.parent = 0u; node.detached_copy = true;
    reply.source_object_type.push_back(node);
    auto command{dbg::parse_console_command("print object")};
    auto raw{dbg::details::format_reply(reply, command)};
    auto human{dbg::console_messages::source_values(reply, command, ::fast_io::string_view{raw.data(),raw.size()})};
    require(human.find("object = size=8 capacity=8 [7, ...]\\x1b [7 pretty-printer elements omitted]") != ::fast_io::containers::npos,
        "concise human print uses escaped summary and explicit omission display");
    require(human.find("[0] = 7") != ::fast_io::containers::npos, "human summary retains copied child values");
    require(human.find("_M_start") == ::fast_io::containers::npos && raw.find("_M_start") != raw.npos,
        "human pretty display hides library fields while machine protocol retains the complete raw copy");
    require(human.find("\x1b") == ::fast_io::containers::npos, "human summary never forwards terminal escape");
    reply.source_object_type[0u].pretty_available=false;
    raw=dbg::details::format_reply(reply,command);
    human=dbg::console_messages::source_values(reply,command,::fast_io::string_view{raw.data(),raw.size()});
    require(human.find("_M_start = guest:0x200") != ::fast_io::containers::npos,"raw value display restores implementation fields");
#endif
    ::fast_io::io::println("source pretty formatter checks passed");
}
