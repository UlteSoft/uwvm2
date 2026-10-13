// Current formatter/grammar protocol; no controller execution or fake authority.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <string_view>
namespace debugger = uwvm2::uwvm::debugger;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_dwarf_values_console: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static auto parse(::std::string_view text)
{ return debugger::parse_console_command(::fast_io::string_view{text.data(), text.size()}); }
int main()
{
    auto const source{parse("locals source 42")}; auto const raw{parse("locals 42")};
    check(source.kind == debugger::console_command_kind::source_locals &&
          source.operation == raw.operation && source.payload_size == 8u && raw.payload_size == 8u, "same existing locals permission/wire operation");
    for(::std::size_t i{}; i != 8u; ++i) { check(source.payload[i] == raw.payload[i], "same authorized thread payload"); }
    for(auto const text : {"locals source", "locals source 0", "locals source -1", "locals source 42 extra",
                          "locals source 18446744073709551616", "locals source 42;continue", "locals unknown 42"})
    { check(parse(text).kind == debugger::console_command_kind::invalid, "bounded numeric source grammar"); }
    debugger::controller_reply reply{};
    check(debugger::details::format_reply(reply, source) == "source locals unavailable: no current cooperative local snapshot\n", "honest no snapshot message");
    reply.source_locals_reason = debugger::source_inline_unavailable_reason::native_stop;
    check(debugger::details::format_reply(reply, source).find("native trap") != ::std::string::npos, "native trap values unavailable");
    reply.source_locals_reason = debugger::source_inline_unavailable_reason::stale_generation_or_stop;
    check(debugger::details::format_reply(reply, source).find("generation is stale") != ::std::string::npos, "stale capture message");
    reply.source_locals_available = true;
    check(debugger::details::format_reply(reply, source) == "no active source variables\n", "empty scope is not a fabricated value");
    debugger::source_dwarf::numeric_variable value{}; value.name = "v\n"; value.type_name = "i8\x1b"; value.parameter = true;
    value.kind = debugger::source_dwarf::numeric_kind::signed_integer; value.byte_count = 1u; value.bits = 254u;
    reply.source_locals.push_back(value);
    auto const text{debugger::details::format_reply(reply, source)};
    check(text.starts_with("source parameter ") && text.find("= i8=-2") != ::std::string::npos &&
          text.find('\x1b') == ::std::string::npos && text.find("v\\x") != ::std::string::npos, "scalar fast_io output and escaped metadata");
    reply.source_locals[0].kind = debugger::source_dwarf::numeric_kind::unavailable;
    reply.source_locals[0].reason = debugger::source_dwarf::numeric_unavailable_reason::unsupported_plan;
    check(debugger::details::format_reply(reply, source).find("unavailable (location needs unsupported evaluation or memory access)") != ::std::string::npos,
          "unsupported plans explicit, no inferred value");
    // Existing raw protocol consumed by the frozen DAP bridge is unchanged.
    check(debugger::details::format_reply(reply, raw) == "locals unavailable for this stop\n", "source replies cannot masquerade as raw local values");
    ::fast_io::io::println("debug_source_dwarf_values_console: PASS grammar and finite-value output protocol");
}
