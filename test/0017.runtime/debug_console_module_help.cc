#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
namespace dbg = uwvm2::uwvm::debugger;
namespace mh = dbg::console_module_help;
using view = ::fast_io::string_view;
static void check(bool ok, view label)
{ if(!ok) { ::fast_io::io::perrln("FAIL module help: ", label); ::fast_io::fast_terminate(); } }
int main()
{
    for(auto line : {view{"bw"}, view{"breakwasm"}, view{"bs"}, view{"breaksource"}, view{"bs 0"}, view{"bw 0"}, view{"bs 123"}})
    { auto const r{mh::parse(line)}; check(r.handled && r.valid, "bare source commands guide module selection"); }
    for(auto line : {view{"bs name.wasm"}, view{"bw -1"}, view{"bw +1"}, view{"bw 1x"}, view{"bw 184467440737095516160"}})
    { auto const r{mh::parse(line)}; check(r.handled && !r.valid, "module names and malformed numeric IDs rejected"); }
    for(auto line : {view{"bs 0 foo.cc:12"}, view{"breakwasm 0 1 0"}, view{"bs 0 foo.cc:12 if counter > 0"}, view{"info w"}})
    { check(!mh::parse(line).handled, "discovery never intercepts real source/Wasm commands or ambiguity"); }
    for(auto line : {view{"bs 0 foo.cc:12"}, view{"bs 0 C:\\src\\foo.cc:12"}, view{"bs 0 path with spaces.cc:12"}, view{"bs 0 src/变量.cc:12"}})
    { auto const c{dbg::console_aliases::parse(line,0,0).command}; check(c.kind == dbg::console_command_kind::source_breakpoint && c.source_module == 0u && c.source_line == 12u, "bs is a source breakpoint alias"); }
    check(dbg::parse_console_command("bs 0 foo.cc:12").kind == dbg::console_command_kind::source_breakpoint, "direct parser agrees with bs source semantics");
    check(dbg::console_aliases::parse("bs 0 1 0",0,0).command.kind == dbg::console_command_kind::invalid, "bs cannot silently become a Wasm byte breakpoint");
    check(dbg::console_aliases::parse("bw 0 1 0",0,0).command.kind == dbg::console_command_kind::protocol, "bw selects a Wasm breakpoint");
    check(dbg::console_aliases::parse("i mo 2",0,0).command.module_help_page == 2u, "module page accepts command prefixes only");
    for(auto line : {view{"info modules 0"}, view{"info modules -1"}, view{"info modules +1"}, view{"info modules 2x"}, view{"info modules 184467440737095516160"}, view{"info modules 1 extra"}})
    { check(dbg::console_aliases::parse(line,0,0).command.kind == dbg::console_command_kind::invalid, "strict bounded page grammar"); }
    for(auto spelling : {view{"bs"}, view{"breaksource"}})
    {
        auto line{::fast_io::concat_fast_io(spelling, " 0 foo.cc:12 ignore 2 if counter > 0")};
        auto const parsed{dbg::console_aliases::parse(line.subview(0u),0,0).command};
        check(parsed.kind == dbg::console_command_kind::source_breakpoint && parsed.breakpoint_initial_condition &&
            parsed.breakpoint_initial_ignore && parsed.breakpoint_ignore == 2u, "source names share condition/ignore policy");
        line=::fast_io::concat_fast_io(spelling, " 0 /tmp/ma");
        check(dbg::console_completion::context(line.subview(0u),line.size()).type == dbg::console_completion::kind::path,
            "source names share path completion");
        check(dbg::console_help::format(spelling).find("bs MODULE_ID FILE:LINE") != ::fast_io::containers::npos,
            "source names reach canonical source help");
    }
    check(dbg::parse_console_command("bw 0 1 0").kind == dbg::console_command_kind::protocol, "direct b numeric parser");
    check(dbg::console_aliases::normalize("bw 0 main").view() == "breakwasm 0 main",
        "bw retains published function breakpoint lookup");
    check(dbg::console_help::format("bw").find("Set a Wasm breakpoint") != ::fast_io::containers::npos,
        "help bw documents Wasm breakpoints");
    check(dbg::console_completion::context("bw 0 ma",7u).type == dbg::console_completion::kind::function,
        "bw offers function completion");
    check(!dbg::console_aliases::normalize("break-source 0 foo.cc:12").valid &&
        dbg::parse_console_command("break-source 0 foo.cc:12").kind == dbg::console_command_kind::invalid,
        "removed unpublished spelling is rejected by both grammars");
    for(auto text : {view{"b 0 1 0"},view{"break 0 1 0"},view{"breakpoint set 0 1 0"},view{"break-name 0 main"}})
    { check(!dbg::console_aliases::normalize(text).valid,"obsolete setters have no compatibility entry"); }
    for(auto text : {view{"bi 12"},view{"breakasm 12"}})
    { auto const cmd{dbg::console_aliases::parse(text,0,0).command};
      check(cmd.kind == dbg::console_command_kind::assembly_breakpoint && cmd.native_breakpoint_offset == 12u,
            "native breakpoint is a distinct authenticated host policy"); }
    for(auto text : {view{"bi -1"},view{"bi +1"},view{"bi 0x1000"},view{"bi 65537"},view{"bi 0 1 0"},view{"bi 12 if true"}})
    { check(dbg::console_aliases::parse(text,0,0).command.kind == dbg::console_command_kind::invalid,"native grammar grants no address or source condition"); }
    dbg::module_help_page page{}; page.valid=true; page.total=9; page.pages=3; page.page=1; page.size=4;
    for(::std::size_t i{}; i<page.size; ++i)
    { auto& r{page.rows[i]}; r.identifier=i; r.name=::fast_io::concat_fast_io(::fast_io::mnp::dec(8u-i)); r.file=::fast_io::concat_fast_io("/tmp/",::fast_io::mnp::dec(i),".wasm"); r.source=dbg::module_source_status::available; }
    auto text{mh::format(page)};
    check(text.find("0 = 0.wasm") != ::fast_io::containers::npos && text.find("name=\"8\"") != ::fast_io::containers::npos, "numeric names remain separate from actual IDs");
    check(text.find("Page 1/3") != ::fast_io::containers::npos && text.find("Next: info modules 2") != ::fast_io::containers::npos && text.find("4 =") == ::fast_io::containers::npos, "one page only, copyable next page");
    page.page=3; page.size=1; page.rows[0].identifier=8;
    text=mh::format(page); check(text.find("Page 3/3") != ::fast_io::containers::npos && text.find("Next:") == ::fast_io::containers::npos, "final page has no fictitious next page");
    page.rows[0].name=::fast_io::concat_fast_io("x\x1b[2J\nname");
    text=mh::format(page,false,24u); check(text.find("\x1b") == ::fast_io::containers::npos, "control bytes in labels cannot reach terminal");
    for(auto status : {dbg::module_source_status::missing, dbg::module_source_status::no_points})
    { page.rows[0].source=status; text=mh::format(page,false,80u,true); check(text.find("without -g") != ::fast_io::containers::npos && text.find("stripped") != ::fast_io::containers::npos, "zero source locations explain possible missing symbols"); }
    page.rows[0].source=dbg::module_source_status::unchecked;
    text=mh::format(page,false,80u,true); check(text.find("not fully checked") != ::fast_io::containers::npos && text.find("without -g") == ::fast_io::containers::npos, "bounded incomplete scan does not claim missing symbols");
    page.rows[0].source=dbg::module_source_status::replaced;
    text=mh::format(page,false,80u,true); check(text.find("retired by replacement") != ::fast_io::containers::npos, "replacement diagnosed separately");
    page.valid=false; check(mh::format(page).starts_with("error:"), "out-of-range pages rejected");
    auto cmd{dbg::console_aliases::parse("bs 0 wrong.cc:12",0,0).command};
    auto reply{::std::make_unique<dbg::controller_reply>()}; reply->status=uwvm2::utils::control::error::source_location_unmapped;
    text=dbg::console_messages::error(*reply,cmd); check(text.find("matches this FILE:LINE") != ::fast_io::containers::npos && text.find("lack -g") == ::fast_io::containers::npos, "wrong source path does not pretend module lacks symbols");
    reply->source_breakpoint_no_points=true;
    text=dbg::console_messages::error(*reply,cmd); check(text.find("Module 0") != ::fast_io::containers::npos && text.find("lack -g") != ::fast_io::containers::npos, "known zero eligible points gives module-specific diagnosis");
    check(dbg::console_messages::syntax_error("bs 123.wasm foo.cc:12").find("not a filename") != ::fast_io::containers::npos, "bad selectors explain numeric IDs");
    ::fast_io::io::println("PASS source breakpoint aliases, numeric module identity, paging, safe labels and symbol diagnostics");
}
