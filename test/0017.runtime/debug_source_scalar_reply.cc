// Real console formatter component. Owned test replies are finite metadata only;
// this does not simulate a private stop ticket or qualify live source values.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <string>
#include <limits>
// Inspect the platform macro inside its matching scoped push/pop pair.
#include <uwvm2/utils/macro/push_macros.h>
#if !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# error This formatter component requires the configured hosted-thread build.
#endif
#include <uwvm2/utils/macro/pop_macros.h>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace dwarf = dbg::source_dwarf;
static void check(bool value, ::std::string_view reason)
{ if(!value) { ::fast_io::io::perrln("source_scalar_reply: ", reason); ::fast_io::fast_terminate(); } }
int main()
{
    dbg::controller_reply reply{}; reply.source_stop_identifier = 9u; reply.source_locals_available = true;
    for(::std::size_t i{}; i != 1024u; ++i)
    {
        dwarf::numeric_variable value{}; value.identity = {0u, i + 1u};
        value.name = ::fast_io::concat_std("var", ::fast_io::mnp::dec(i)); value.type_name = ::fast_io::concat_std("uint64_t");
        value.kind = dwarf::numeric_kind::unavailable; value.reason = dwarf::numeric_unavailable_reason::unsupported_plan;
        reply.source_locals.push_back(::std::move(value));
    }
    auto const command{dbg::parse_console_command("locals source 1")};
    auto rendered{dbg::details::format_reply(reply, command)};
    check(rendered.size() <= 63u * 1024u && rendered.starts_with("source-stop 9\n") && rendered.ends_with("truncated=true\n"),
        "legal 1024-variable reply retains complete bounded rows and an honest trailer");
    auto const trailer{rendered.find("source-variables shown=")};
    check(trailer != ::std::string::npos && trailer != 0u && rendered[trailer - 1u] == '\n' &&
        rendered.find(" total=1024 truncated=true\n", trailer) != ::std::string::npos, "truncation never cuts a value row");
    auto const first_value{reply.source_locals.front()};
    reply.source_locals.clear(); reply.source_locals.push_back(first_value);
    reply.source_locals[0u].name = ::fast_io::concat_std(::std::string_view{"name\x1b\xc2\x9b\xe2\x80\xae", 10u});
    reply.source_locals[0u].type_name = ::fast_io::concat_std(::std::string_view{"type\n\xc2\x9b", 7u});
    rendered = dbg::details::format_reply(reply, command);
    check(rendered.find('\x1b') == ::std::string::npos && rendered.find('\xc2') == ::std::string::npos &&
        rendered.find('\xe2') == ::std::string::npos && rendered.find("name\\x1b\\xc2\\x9b\\xe2\\x80\\xae") != ::std::string::npos &&
        rendered.find("type\\x0a\\xc2\\x9b") != ::std::string::npos, "same metadata CPO escapes C0/C1/invalid UTF8 and bidi in name/type");
    reply.source_locals[0u].name = ::std::string(16385u, 'x');
    check(dbg::details::format_reply(reply, command) == "error: source variable labels exceed bounded capacity\n",
        "independent string budget rejects BEFORE allocating an escaped large row");
    reply.source_locals[0u] = first_value;
    reply.source_locals[0u].kind = dwarf::numeric_kind::signed_integer; reply.source_locals[0u].byte_count = 4u;
    reply.source_locals[0u].bits = 23u;
    reply.source_constant = {{0u, 33u}, {0u, 11u}, {0u, 55u}, 1u, 12u, true};
    rendered = dbg::details::format_reply(reply, dbg::parse_console_command("print var0"));
    check(rendered.find("source-origin stop=9 thread=1 code-offset=12 variable-unit=0 variable-offset=33 scope-unit=0 scope-offset=11 type-unit=0 type-offset=55 kind=DW_AT_const_value\n") != ::std::string::npos,
        "scalar print carries explicit exact metadata provenance after its actual value row");
    check(dbg::details::format_reply(reply, command).find("source-origin") == ::std::string::npos,
        "locals list does not silently acquire per-expression provenance");
    auto const maximum{(::std::numeric_limits<::std::uint64_t>::max)()};
    reply.source_stop_identifier = maximum; reply.source_constant = {{maximum, maximum}, {maximum, maximum}, {maximum, maximum}, maximum, maximum, true};
    rendered = dbg::details::format_reply(reply, dbg::parse_console_command("print var0"));
    check(rendered.size() <= 63u * 1024u && rendered.find("kind=DW_AT_const_value\n") != ::std::string::npos,
        "maximum-width receipt plus header/trailer remains independently budgeted");
    ::fast_io::io::println("PASS bounded real source scalar formatter component; authenticated product value remains separate");
}
