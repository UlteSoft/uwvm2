#include <uwvm2/uwvm/debugger/console_completion.h>
#include <uwvm2/uwvm/debugger/console_tui.h>
#include <fast_io.h>
namespace edit=uwvm2::uwvm::debugger::console_editing;
namespace completion=uwvm2::uwvm::debugger::console_completion;
namespace tui=uwvm2::uwvm::debugger::console_tui;
using view=fast_io::string_view;
unsigned failures{};
void check(bool good,view label) { if(!good) { fast_io::io::println("FAIL: ",label); ++failures; } }
void input(edit::editor& e,view text) { for(unsigned char c:text) { static_cast<void>(e.feed(c)); } }
void input(edit::editor& e,fast_io::string const& text) { input(e,text.subview(0u)); }
int main(int argc,char** argv)
{
    if(argc != 2) { return 2; } view directory{fast_io::mnp::os_c_str(argv[1])};
    edit::editor editor{}; editor.begin(); input(editor,"print fir + suffix");
    for(unsigned i{};i<9u;++i) { static_cast<void>(editor.feed(2)); }
    auto request{completion::context(editor.line().view(),editor.cursor())};
    completion::candidates values{};
    check(values.add("first",request.prefix),"candidate accepts first");
    check(values.apply(editor,request)==edit::action::changed && editor.line().view()=="print first + suffix","mid-line symbol preserves expression suffix");
    static_cast<void>(editor.feed(31)); check(editor.line().view()=="print fir + suffix","completion is one undo group");
    editor.begin(); input(editor,"print object.fi"); request=completion::context(editor.line().view(),editor.cursor());
    check(request.root=="object" && request.prefix=="fi","member context");
    editor.begin();input(editor,"print 9 7 1 object->fi");request=completion::context(editor.line().view(),editor.cursor());
    check(request.root=="object" && request.selectors=="9 7 1 " && request.prefix=="fi","selectors preserved for pointer members");
    values={};static_cast<void>(values.add("alpha", "a"));static_cast<void>(values.add("alpine", "a"));
    editor.begin();input(editor,"print a");request=completion::context(editor.line().view(),editor.cursor());
    check(values.apply(editor,request)==edit::action::changed && editor.line().view()=="print alp","ambiguous common prefix");
    request=completion::context(editor.line().view(),editor.cursor());check(values.apply(editor,request)==edit::action::candidates,"second Tab lists ambiguity");
    values.truncated=true;editor.begin();input(editor,"print a");request=completion::context(editor.line().view(),editor.cursor());
    check(values.apply(editor,request)==edit::action::candidates && editor.line().view()=="print a","truncated list cannot claim uniqueness");
    values={};static_cast<void>(values.add("bad\x1b[31m", ""));check(values.size==0u,"symbol terminal control rejected");
    static_cast<void>(values.add("bad\xc2\x9b" "31m", ""));check(values.size==0u,"encoded C1 candidate rejected");
    editor.begin(); input(editor,fast_io::concat_fast_io("replace 0 1 1 ",directory,"/up"));
    request=completion::context(editor.line().view(),editor.cursor()); completion::paths(request,values);
    check(!values.unavailable && values.size==2u,"real directory path completion");
    check(values.apply(editor,request)==edit::action::changed && editor.line().view().ends_with("/update-"),"directory common prefix");
    values={};editor.begin();input(editor,fast_io::concat_fast_io("tui source ",directory,"/sub"));request=completion::context(editor.line().view(),editor.cursor());completion::paths(request,values);
    check(values.size==1u && values.words[0].ends_with("/subdirectory/"),"directory slash completion");
    values={};editor.begin();input(editor,fast_io::concat_fast_io("tui source ",directory,"/space"));request=completion::context(editor.line().view(),editor.cursor());completion::paths(request,values);
    check(values.size==1u && values.words[0].ends_with("space name.cc"),"spaces in path remain literal");
    values={};editor.begin();input(editor,fast_io::concat_fast_io("tui source ",directory,"/bad"));request=completion::context(editor.line().view(),editor.cursor());completion::paths(request,values);check(values.size==0u,"control-bearing filename rejected");
    values={};editor.begin();input(editor,fast_io::concat_fast_io("replace 0 1 1 ",directory,"/missing/x"));request=completion::context(editor.line().view(),editor.cursor());completion::paths(request,values);check(values.unavailable,"missing directory is recoverable");
    editor.begin();input(editor,"status");static_cast<void>(editor.feed(24));check(editor.feed('a')==edit::action::tui_toggle && editor.line().view()=="status","CtrlX A only UI action");
    input(editor,"\x1b[5");check(editor.feed('~')==edit::action::page_up,"PageUp recognized");
    editor.begin();input(editor,"memory 0 0 ");check(editor.feed(9)==edit::action::unchanged,"unsupported arguments do not request a provider");
    editor.begin();input(editor,"replace 0 1 1 /tmp/f");check(editor.feed(9)==edit::action::completion,"path Tab requests a provider without executing input");
    editor.begin();input(editor,"break-source 0 /tmp/part:12");for(unsigned i{};i<3u;++i){static_cast<void>(editor.feed(2));}
    request=completion::context(editor.line().view(),editor.cursor());check(request.type==completion::kind::path && request.prefix=="/tmp/part" && request.end==24u,"source path completion preserves line selector");
    fast_io::string output{};
    {
        tui::screen screen{&output,+[](void* p,view text) noexcept { static_cast<fast_io::string*>(p)->append(text); },true};
        check(screen.command("layout split") && screen.enabled(),"layout enables real alternate-screen TUI");
        screen.set_status("stop=1");screen.set_panel(tui::layout::src,"one\n=> 2 source\n");screen.set_panel(tui::layout::assembly,"wasm JIT instruction\n");
        screen.output("reply\n");screen.render(editor,{80u,24u});
        check(output.find("[source *]")!=fast_io::containers::npos && output.find("[asm *]")!=fast_io::containers::npos,"source and asm panes rendered");
        check(output.find("reply")!=fast_io::containers::npos && output.find("(uwvm-debug)")!=fast_io::containers::npos,"command log and editor pane rendered");
        editor.begin();input(editor,"\xe6\xba\x90");screen.render(editor,{80u,24u});
        check(output.ends_with("\x1b[24;16H\x1b[?25h"),"CJK cursor uses terminal cells");
        screen.render(editor,{20u,4u});check(output.find("TUI: enlarge")!=fast_io::containers::npos,"small terminal handled");
    }
    check(output.find("\x1b[?1049h")!=fast_io::containers::npos && output.ends_with("\x1b[r\x1b[?25h\x1b[?1049l"),"alternate screen and cursor restored by RAII");
    output.clear();{tui::screen screen{&output,+[](void* p,view text) noexcept {static_cast<fast_io::string*>(p)->append(text);},false};static_cast<void>(screen.command("tui enable"));check(!screen.enabled(),"pipe cannot enable TUI");}
    check(tui::clean("\xc2\x9b" "31m").find_character('\x1b')==fast_io::containers::npos && tui::clean("\xc2\x9b" "31m").starts_with("\\x"),"Unicode C1 escaped");
    check(tui::clean("\xe6\xba\x90")=="\xe6\xba\x90" && tui::cells("\xe6\xba\x90")==2u,"UTF8 path preserved with correct cursor width");
    check(tui::clean("\x1b[2Jtest").find_character('\x1b')==fast_io::containers::npos,"source ANSI escaped");
    auto const text{tui::source_text(fast_io::concat_fast_io(directory,"/source.cc"),2u)};check(text.find("=> 2 second")!=fast_io::containers::npos,"source text actual line highlighted");
    auto const fifo{tui::source_text(fast_io::concat_fast_io(directory,"/fifo"),1u)};check(fifo.find("unavailable")!=fast_io::containers::npos,"FIFO cannot block source display");
    fast_io::io::println("debug_console_tui_completion: ",failures==0u ? view{"PASS"} : view{"FAIL"});return failures!=0u;
}
