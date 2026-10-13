// Finite HOST editing only. No fake VM stop/frame/native capability.
#include <uwvm2/uwvm/debugger/console_line_editor.h>
#include <fast_io.h>
#include <string_view>
namespace editing = uwvm2::uwvm::debugger::console_editing;
static void check(bool ok, char const* name)
{ if(!ok) { ::fast_io::io::perrln("editor shortcuts: FAIL ", ::fast_io::mnp::os_c_str(name)); ::fast_io::fast_terminate(); } }
static void input(editing::editor& e, ::std::string_view s)
{ for(auto c : s) { static_cast<void>(e.feed(static_cast<unsigned char>(c))); } }
static bool equal(editing::editor const& e, ::std::string_view s)
{ auto v{e.line().view()}; return ::std::string_view{v.data(),v.size()} == s; }
int main()
{
    editing::editor e; e.begin(); input(e,"stat");
    check(e.feed(9) == editing::action::changed && equal(e,"status"),"unique Tab command");
    e.begin(); input(e,"locals s"); static_cast<void>(e.feed(9));
    check(equal(e,"locals source "),"multiword argument command Tab");
    e.begin(); input(e,"info "); unsigned found{};
    check(e.feed(9) == editing::action::candidates,"ambiguous Tab displays static matches");
    e.display_candidates([&](::fast_io::string_view s){check(s.starts_with("info "),"candidate prefix");++found;});
    check(found >= 8u && equal(e,"info "),"ambiguous Tab cannot pick or execute command");
    e.begin(); input(e,"memory 0 0 "); check(e.feed(9) == editing::action::unchanged,"arguments never probe paths or guest state");
    e.begin(); input(e,"status"); static_cast<void>(e.feed(1));
    check(e.feed(9) == editing::action::unchanged && equal(e,"status"),"mid-line completion is conservative");
    check(e.feed(12) == editing::action::redraw && equal(e,"status") && e.cursor()==0u,"CtrlL preserves text/cursor");
    e.remember("status",true);e.remember("print value",true);e.remember("status",true);
    e.begin();input(e,"draft"); static_cast<void>(e.feed(18)); input(e,"stat");
    check(e.searching() && e.search_matched() && equal(e,"status"),"incremental reverse search actual history");
    static_cast<void>(e.feed(18)); check(e.search_matched() && equal(e,"status"),"CtrlR earlier matching command");
    check(e.feed('x') == editing::action::changed && !e.search_matched(),"failed query is explicit");
    check(e.feed('\n') == editing::action::unchanged && e.searching(),"failed query Enter cannot execute stale match");
    check(e.feed(-1) == editing::action::oversized,"failed query EOF cannot execute stale match");
    check(e.feed(7)==editing::action::changed && equal(e,"draft") && !e.searching(),"CtrlG restores actual draft");
    e.begin();static_cast<void>(e.feed(18));input(e,"print");
    check(e.feed('\n')==editing::action::complete && equal(e,"print value"),"matched search Enter explicitly accepts real host history");
    e.begin();static_cast<void>(e.feed(18));input(e,"nomatch");
    check(e.feed(-3)==editing::action::interrupted && e.line().size==0u && !e.searching(),"qualified cancel discards search and whole command");
    e.begin();input(e,"draft");static_cast<void>(e.feed(18));input(e,"abc");static_cast<void>(e.feed(127));
    check(e.search_query()=="ab","search query Backspace bounded");
    e.remember("print a\xc3\xa9",true);e.begin();static_cast<void>(e.feed(18));input(e,"a\xc3\xa9");
    static_cast<void>(e.feed(127));check(e.search_query()=="a","query Backspace removes full UTF8 codepoint");
    e.begin();static_cast<void>(e.feed(18));
    for(unsigned i{};i!=editing::maximum_bytes+8u;++i){static_cast<void>(e.feed('x'));}
    check(e.search_query().size()==editing::maximum_bytes,"search query capacity never overflows");
    static_cast<void>(e.feed(7));check(e.line().size==0u,"oversized search does not execute suffix");
    ::fast_io::io::println("editor shortcuts: PASS static Tab, CtrlL and bounded reverse history source component");
}
