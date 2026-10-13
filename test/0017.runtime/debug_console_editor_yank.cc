// Finite HOST owned-input editing only. No mocked VM/stop/native authority.
#if defined(UWVM_MODULE)
#include <cstddef>
import fast_io;
import uwvm2.uwvm.debugger;
#else
#include <uwvm2/uwvm/debugger/console_line_editor.h>
#include <fast_io.h>
#endif
namespace editing = uwvm2::uwvm::debugger::console_editing;
static void check(bool ok, char const* name)
{
    if(!ok)
    {
        ::fast_io::println(::fast_io::err(), "debug_console_editor_yank: FAIL ", ::fast_io::mnp::os_c_str(name));
        ::fast_io::fast_terminate();
    }
}
static void input(editing::editor& e, ::fast_io::string_view text)
{ for(auto const c : text) { static_cast<void>(e.feed(static_cast<unsigned char>(c))); } }
static bool equal(editing::editor const& e, ::fast_io::string_view text)
{ return e.line().view() == text; }
static void pop(editing::editor& e)
{ static_cast<void>(e.feed(27)); static_cast<void>(e.feed('y')); }
int main()
{
    editing::editor e; e.begin();
    check(e.feed(25) == editing::action::unchanged && equal(e,""),"CtrlY empty ring cannot edit or execute");
    input(e,"abc def"); static_cast<void>(e.feed(1));
    for(unsigned i{}; i != 4u; ++i) { static_cast<void>(e.feed(6)); }
    check(e.feed(11) == editing::action::changed && equal(e,"abc "),"CtrlK stores exact suffix");
    check(e.feed(25) == editing::action::changed && equal(e,"abc def") && e.cursor()==7u,"CtrlY restores suffix at original point");
    e.begin();input(e,"one two three");static_cast<void>(e.feed(23));static_cast<void>(e.feed(23));
    check(equal(e,"one ") && e.kill_ring_size()==2u,"consecutive backward kills coalesce in natural order");
    static_cast<void>(e.feed(25));check(equal(e,"one two three"),"CtrlY restores both consecutive words and their delimiter");
    e.begin();input(e,"abc def");static_cast<void>(e.feed(1));
    for(unsigned i{};i!=4u;++i){static_cast<void>(e.feed(6));}
    static_cast<void>(e.feed(11));static_cast<void>(e.feed(21));static_cast<void>(e.feed(25));
    check(equal(e,"abc def"),"mixed forward/backward consecutive kills restore exact original byte order");
    e.begin();input(e,"a\xc3\xa9 z");static_cast<void>(e.feed(23));static_cast<void>(e.feed(23));static_cast<void>(e.feed(25));
    check(equal(e,"a\xc3\xa9 z"),"CtrlW and yank preserve complete UTF8 codepoints");
    e.begin();input(e,"first");static_cast<void>(e.feed(21));
    e.begin();input(e,"second");static_cast<void>(e.feed(21));
    e.begin();static_cast<void>(e.feed(25));check(equal(e,"second"),"kill ring survives completed input begin without auto execution");
    pop(e);check(equal(e,"first"),"AltY replaces only actual prior yanked range with previous real item");
    static_cast<void>(e.feed(2));pop(e);check(equal(e,"first") && e.cursor()==4u,"intervening movement disables yank-pop");
    e.begin();input(e,"ZZ");static_cast<void>(e.feed(21));
    e.begin();for(unsigned i{};i!=editing::maximum_bytes-1u;++i){static_cast<void>(e.feed('x'));}
    check(e.feed(25)==editing::action::unchanged && e.line().size==editing::maximum_bytes-1u,"whole yank rejects capacity overflow without a truncated command");
    check(e.feed('\n')==editing::action::complete && e.line().size==editing::maximum_bytes-1u,"rejected yank leaves deliberate original command intact");
    e.begin();for(unsigned i{};i!=editing::maximum_bytes-2u;++i){static_cast<void>(e.feed('x'));}
    check(e.feed(25)==editing::action::changed && e.line().size==editing::maximum_bytes,"whole yank exact-capacity safe");
    check(e.feed('\n')==editing::action::complete,"exact capacity remains an explicitly accepted complete command");
    editing::editor range;range.begin();input(range,"a");static_cast<void>(range.feed(21));
    range.begin();for(unsigned i{};i!=editing::maximum_bytes;++i){static_cast<void>(range.feed('L'));}
    static_cast<void>(range.feed(21));range.begin();static_cast<void>(range.feed(25));pop(range);
    check(equal(range,"a"),"yank-pop can replace large prior owned item with a smaller one");
    static_cast<void>(range.feed('z'));pop(range);check(equal(range,"az"),"insertion invalidates prior yank replacement authority");
    range.begin();static_cast<void>(range.feed(25));pop(range);static_cast<void>(range.feed(25));
    check(equal(range,"a"),"failed next whole yank cannot truncate oversized replacement");
    editing::editor full;full.begin();
    for(unsigned i{};i!=12u;++i)
    {
        full.begin();auto const text{::fast_io::concat("kill",::fast_io::mnp::dec(i))};input(full,::fast_io::string_view{text.data(),text.size()});
        static_cast<void>(full.feed(21));
    }
    check(full.kill_ring_size()==editing::maximum_kills,"kill ring fixed to eight owned items");
    full.begin();static_cast<void>(full.feed(25));check(equal(full,"kill11"),"newest ring item retained");
    for(unsigned i{};i!=7u;++i){pop(full);}
    check(equal(full,"kill4"),"oldest retained ring item exactly eight entries behind");
    pop(full);check(equal(full,"kill11"),"AltY wraps within owned bounded ring");
    static_cast<void>(full.feed(-3));check(full.line().size==0u && full.feed('\n')==editing::action::complete && equal(full,""),"CtrlC cannot implicitly execute killed/history content");
    full.begin();input(full,"status");input(full,"\x1b[1~");check(full.cursor()==0u,"VT Home 1 tilde");
    input(full,"\x1b[4~");check(full.cursor()==6u,"VT End 4 tilde");
    input(full,"\x1b[7~");check(full.cursor()==0u,"VT Home 7 tilde");
    input(full,"\x1b[8~");check(full.cursor()==6u,"VT End 8 tilde");
    input(full,"\x1b[1~\x1b[3~");check(equal(full,"tatus"),"VT Delete uses existing point/delete behavior");
    full.begin();input(full,"status\x1b[9");check(full.feed('\n')==editing::action::oversized,"unknown escape still discards complete line");
    ::fast_io::println(::fast_io::out(),"debug_console_editor_yank: PASS bounded HOST kill/yank/AltY UTF8 capacity adjacency and VT HomeEnd");
}
