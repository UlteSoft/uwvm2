// Production renderer/read-line fixture and lexical regression checks.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <memory>
namespace dbg=uwvm2::uwvm::debugger;
namespace edit=dbg::console_editing;
namespace tui=dbg::console_tui;
namespace lex=dbg::console_source_highlight;
using view=fast_io::string_view;
static void check(bool good,view why)
{ if(!good) { fast_io::io::perrln("FAIL tui color: ",why); fast_io::fast_terminate(); } }
static void unit(view executable)
{
    using s=lex::style;
    auto expect=[](view text,lex::language lang,lex::state& state,view token,s expected)
    {
        bool found{}; ::std::size_t last{};
        lex::scan(text,lang,state,[&](::std::size_t begin,::std::size_t end,s color)
        { check(begin==last && end>begin && end<=text.size(),"scanner advances without gaps"); last=end;
          if(text.subview(begin,end-begin)==token && color==expected) { found=true; } });
        check(last==text.size() && found,"expected lexical token and style");
    };
    for(auto lang : {lex::language::cpp,lex::language::rust,lex::language::go,lex::language::zig,lex::language::script})
    { lex::state state{}; expect("return 42; // note",lang,state,"return",s::keyword);
      expect("42",lang,state,"42",s::number); expect("\"value\"",lang,state,"\"value\"",s::string); }
    lex::state state{};expect("/* outer",lex::language::rust,state,"/* outer",s::comment);
    expect("/* nested */ still",lex::language::rust,state,"/* nested */ still",s::comment);
    expect("end */ let",lex::language::rust,state,"let",s::keyword);check(state.block_depth==0u,"nested Rust comments close");
    state={};expect("(; outer",lex::language::wat,state,"(; outer",s::comment);
    expect("(; nested ;) ;)",lex::language::wat,state,"(; nested ;) ;)",s::comment);
    expect("i32.const 7 ;; comment",lex::language::wat,state,"i32.const",s::keyword);
    state={};expect("R\"tag(first",lex::language::cpp,state,"R\"tag(first",s::string);
    expect("/* literal )tag\" return",lex::language::cpp,state,"return",s::keyword);check(state.quote==0,"C++ raw delimiter closes");
    state={};expect("r##\"first",lex::language::rust,state,"r##\"first",s::string);
    expect("// literal\"## fn",lex::language::rust,state,"fn",s::keyword);
    state={};expect("`first",lex::language::go,state,"`first",s::string);expect("/* literal */` return",lex::language::go,state,"return",s::keyword);
    for(view prefix : {view{"u8R"},view{"uR"},view{"UR"},view{"LR"}})
    { state={}; auto const raw{fast_io::concat_fast_io(prefix,"\"x(body")};
      expect(raw.subview(0u),lex::language::cpp,state,raw.subview(0u),s::string);
      expect("tail)x\" return",lex::language::cpp,state,"return",s::keyword); }
    state={};fast_io::string raw{fast_io::concat_fast_io("r")}, close{fast_io::concat_fast_io("\"")};
    for(unsigned n{};n<255u;++n) {raw.push_back('#');close.push_back('#');}
    raw.append("\"body");close.append(" fn");
    expect(raw.subview(0u),lex::language::rust,state,raw.subview(0u),s::string);
    expect(close.subview(0u),lex::language::rust,state,"fn",s::keyword);
    state={};expect("`escaped\\` still` return",lex::language::script,state,"return",s::keyword);
    state={};expect("\\\\literal",lex::language::zig,state,"\\\\literal",s::string);
    state={};expect("`template",lex::language::script,state,"`template",s::string);expect("tail` return",lex::language::script,state,"return",s::keyword);
    state={};expect("'life: loop",lex::language::rust,state,"loop",s::keyword);check(state.quote==0,"Rust lifetime is not a string");
    state={};expect("return 2",lex::language::plain,state,"return 2",s::plain);
    check(lex::detect("a.cc")==lex::language::cpp && lex::detect("a.rs")==lex::language::rust &&
          lex::detect("a.go")==lex::language::go && lex::detect("a.zig")==lex::language::zig &&
          lex::detect("a.ts")==lex::language::script && lex::detect("a.wat")==lex::language::wat &&
          lex::detect("a.unknown")==lex::language::plain,"extension policy");
    auto const path{fast_io::concat_fast_io(executable,".source.cc")};
    { fast_io::native_file file{path,fast_io::open_mode::out | fast_io::open_mode::trunc};
      fast_io::print(file,"/* first\nreturn 7;\nreturn 8;\nstill comment\n*/ return 9;\n"); }
    lex::line_styles source_styles{};
    auto const source{tui::source_text(path.subview(0u),5u,&source_styles)};
    check(!source_styles.empty() && source_styles.front().before.block_depth==1u && source.find("=> 5 */ return 9")!=fast_io::containers::npos,
          "FastIO source window retains comment state from earlier file lines");
    auto editor=std::make_unique<edit::editor>();editor->begin();
    fast_io::string output{};
    auto const previous=uwvm2::uwvm::utils::ansies::put_color;
    uwvm2::uwvm::utils::ansies::put_color=true;
    {
        tui::screen screen{&output,+[](void* p,view text) noexcept {static_cast<fast_io::string*>(p)->append(text);},true};
        screen.enable();screen.set_panel(tui::layout::src,"=> 2 return 42; // note\n");
        lex::line_styles lines{};lines.push_back({0u,5u,lex::language::cpp,{}});screen.set_source_styles(std::move(lines),0u);
        screen.output("\x1b[95mKEPT_REPLY\x1b[0m\n");screen.render(*editor,{80u,24u});
        check(output.find("95mreturn")!=fast_io::containers::npos && output.find("96m42")!=fast_io::containers::npos,"source tokens use UWVM palette");
        check(output.find("93m=> 2 ")!=fast_io::containers::npos,"exact source marker and line number are yellow");
        output.clear();screen.clear_panels();
        screen.set_panel(tui::layout::src,"N> 123 return 42; // note\n");
        lines={};lines.push_back({0u,7u,lex::language::cpp,{}});screen.set_source_styles(std::move(lines),0u);
        screen.render(*editor,{80u,24u});
        check(output.find("93mN> 123 ")!=fast_io::containers::npos,"Wasm step preview marker and every line-number digit are yellow");
        for(view prefix : {view{"=> 456 "},view{"N> 789 "}})
        {
            output.clear();screen.clear_panels();screen.set_panel(tui::layout::src,fast_io::concat_fast_io(prefix,"unclassified source\n"));
            screen.key(edit::action::redraw);
            screen.render(*editor,{80u,24u});
            check(output.find(fast_io::concat_fast_io("93m",prefix).subview(0u))!=fast_io::containers::npos,"source stop colors do not depend on lexical metadata");
        }
        output.clear();screen.show_candidates("TRANSIENT_ONLY\nsecond");screen.render(*editor,{80u,24u});
        check(output.find("TRANSIENT_ONLY")!=fast_io::containers::npos,"candidate visible");
        screen.key(edit::action::changed);output.clear();screen.render(*editor,{80u,24u});
        check(output.find("TRANSIENT_ONLY")==fast_io::containers::npos && output.find("KEPT_REPLY")!=fast_io::containers::npos &&
              output.find("\\x1b")==fast_io::containers::npos,"candidate never enters log and diagnostic SGR stays renderable");
        uwvm2::uwvm::utils::ansies::put_color=false;output.clear();screen.render(*editor,{80u,24u});
        check(output.find("\x1b[95m")==fast_io::containers::npos && output.find("\x1b[0m")==fast_io::containers::npos,"NO_COLOR policy suppresses SGR");
        screen.clear_panels();screen.set_panel(tui::layout::src,"BAD\x1b[2JEND\n");output.clear();screen.render(*editor,{80u,24u});
        check(output.find("BAD\\x1b[2JEND")!=fast_io::containers::npos,"source controls remain escaped");
        screen.disable();output.clear();screen.show_candidates("POPUP_ONLY");screen.render(*editor,{80u,24u});
        check(output.find("\x1b[?1049h")==fast_io::containers::npos && output.find("POPUP_ONLY")!=fast_io::containers::npos,"line candidates append to primary scrollback");
        screen.key(edit::action::unchanged);check(!screen.has_candidates() && output.find("\x1b[?1049l")==fast_io::containers::npos,"line completion never switches terminal screens");
    }
    uwvm2::uwvm::utils::ansies::put_color=previous;
    fast_io::io::println("PASS tui color unit");
}
int main(int argc,char** argv)
{
    if(argc!=2) { return 2; } view mode{fast_io::mnp::os_c_str(argv[1])};
    if(mode=="unit") { unit(view{fast_io::mnp::os_c_str(argv[0])});return 0; } if(mode!="line" && mode!="tui") { return 2; }
    dbg::console_keyboard::native_session keyboard{};if(!keyboard.ready() || !keyboard.interactive_display()) { return 3; }
    auto write{+[](void*,view text) noexcept { fast_io::print(fast_io::out(),text); }};
    dbg::console_io io{&keyboard,+[](void* p) noexcept {return static_cast<dbg::console_keyboard::native_session*>(p)->read_byte();},write,true};
    tui::screen screen{nullptr,write,true};
    screen.set_panel(tui::layout::src,"=> 1 return 42; // colored source\n");
    lex::line_styles lines{};lines.push_back({0u,5u,lex::language::cpp,{}});screen.set_source_styles(std::move(lines),0u);
    if(mode=="tui") { screen.enable(); }
    dbg::console_read_hooks hooks{&screen,nullptr,
        +[](void* p,edit::editor const& editor,edit::action action)
        { auto& s=*static_cast<tui::screen*>(p);auto const before=s.enabled();s.key(action);
          if(s.enabled() || s.has_candidates()) {s.render(editor);return true;}
          if(before) {fast_io::print(fast_io::out(),tui::command_line(editor,tui::terminal_dimensions().columns));return true;}return false;},
        +[](void* p,edit::editor const& editor,bool)
        {fast_io::string text{};editor.display_candidates([&](view word){if(!text.empty()){text.push_back('\n');}text.append(word);});
         static_cast<tui::screen*>(p)->show_candidates(text.subview(0u));}};
    auto editor=std::make_unique<edit::editor>();fast_io::io::println("EDITOR READY");
    for(unsigned n{};n<16u;++n)
    {
        auto line=dbg::read_console_line(io,*editor,hooks);if(line.status==dbg::line_status::end) {break;}
        if(line.status==dbg::line_status::interrupted) {screen.output("^C\n");continue;}
        if(line.status!=dbg::line_status::complete) {return 4;}
        view text{line.bytes.data(),line.size};screen.record(fast_io::concat_fast_io("RESULT ",text,"\n"));
        if(text=="quit") {break;} if(!screen.command(text) && !screen.enabled()) {fast_io::io::println("RESULT ",text);}
    }
    screen.disable();fast_io::io::println("COLOR RESTORED");
}
