// Real production input adapter + renderer, without launching a guest VM.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <memory>
namespace dbg = uwvm2::uwvm::debugger;
namespace editing = dbg::console_editing;
namespace tui = dbg::console_tui;
using view = ::fast_io::string_view;
static void check(bool good, view label)
{ if(!good) { ::fast_io::io::perrln("FAIL cursor: ", label); ::fast_io::fast_terminate(); } }
static void input(editing::editor& editor, view text)
{ for(unsigned char byte : text) { static_cast<void>(editor.feed(byte)); } }
static void unit()
{
    auto owner{::std::make_unique<editing::editor>()}; auto& editor{*owner};
    editor.begin(); input(editor, "status"); input(editor, "\x1b[D\x1b[D");
    check(editor.cursor() == 4u, "CSI Left moves the byte cursor");
    auto row{tui::command_input(editor, 80u)};
    check(row.text == "(uwvm-debug) status" && row.column == 18u, "visible caret before suffix");
    auto rendered{tui::command_line(editor, 80u)};
    check(rendered.ends_with("\r\x1b[17C") && rendered.find("\x1b[s") == ::fast_io::containers::npos &&
          rendered.find("\x1b[u") == ::fast_io::containers::npos, "explicit portable cursor positioning");
    input(editor, "X\x1bOC\x1b[3~");
    check(editor.line().view() == "statXu" && editor.cursor() == 6u, "SS3 Right and Delete edit the actual caret");
    editor.begin(); input(editor, "\xe6\xba\x90\xc3\xa9z\x1bOD\x1b[D");
    row = tui::command_input(editor, 80u);
    check(editor.cursor() == 3u && row.column == 16u, "UTF8 byte boundary uses CJK display cells");
    input(editor, "\xe4\xb8\xad");
    check(editor.line().view() == "\xe6\xba\x90\xe4\xb8\xad\xc3\xa9z", "insert Unicode inside the line");
    editor.begin(); input(editor, "abcdefghijklmnopqrstuvwxyz");
    row = tui::command_input(editor, 24u);
    check(row.text == "(uwvm-debug) qrstuvwxyz" && row.column == 24u, "long input scrolls on one row");
    input(editor, "\x1b[H"); row = tui::command_input(editor, 24u);
    check(row.text == "(uwvm-debug) abcdefghij" && row.column == 14u, "Home reveals the start");
    editor.remember("\xe6\xba\x90\xe4\xb8\xadz", true); editor.begin();
    static_cast<void>(editor.feed(18)); input(editor, "\xe6\xba\x90");
    row = tui::command_input(editor, 80u);
    check(row.column == tui::cells(row.text.subview(0u)) + 1u, "search prompt width uses cells, not UTF8 bytes");
    editor.begin();
    for(unsigned i{}; i < 200u; ++i) { input(editor, "a\xe6\xba\x90\xc3\xa9"); }
    for(::std::size_t columns{1u}; columns <= 240u; ++columns)
    {
        row = tui::command_input(editor, columns);
        check(tui::cells(row.text.subview(0u)) < columns && row.column <= columns && row.column != 0u,
              "bounded viewport even on a one-column terminal");
    }
    editor.begin(); input(editor, "\xc2\x9b" "31m");
    row = tui::command_input(editor, 80u);
    check(row.text.find("\xc2\x9b") == ::fast_io::containers::npos, "encoded C1 cannot control the terminal");
    ::fast_io::io::println("PASS cursor unit");
}
int main(int argc, char** argv)
{
    if(argc != 2) { return 2; }
    view const mode{::fast_io::mnp::os_c_str(argv[1])};
    if(mode == "unit") { unit(); return 0; }
    if(mode != "line" && mode != "tui") { return 2; }
    dbg::console_keyboard::native_session keyboard{};
    if(!keyboard.ready() || !keyboard.interactive_display()) { return 3; }
    auto write{+[](void*, view text) noexcept { ::fast_io::print(::fast_io::out(), text); }};
    dbg::console_io io{::std::addressof(keyboard),
        +[](void* p) noexcept { return static_cast<dbg::console_keyboard::native_session*>(p)->read_byte(); }, write, true};
    tui::screen screen{nullptr, write, true};
    if(mode == "tui") { screen.enable(); }
    dbg::console_read_hooks hooks{::std::addressof(screen), nullptr,
        +[](void* p, editing::editor const& editor, editing::action)
        { auto& current{*static_cast<tui::screen*>(p)}; if(!current.enabled()) { return false; }
          current.render(editor); return true; }};
    auto owner{::std::make_unique<editing::editor>()};
    ::fast_io::io::println("EDITOR READY");
    for(unsigned commands{}; commands < 16u; ++commands)
    {
        auto const line{dbg::read_console_line(io, *owner, hooks)};
        if(line.status == dbg::line_status::end) { break; }
        if(line.status != dbg::line_status::complete) { return 4; }
        auto const text{view{line.bytes.data(), line.size}};
        screen.output(::fast_io::concat_fast_io("RESULT ", text, "\n"));
        if(screen.enabled()) { ::fast_io::io::println("\r\nRESULT ", text); }
        owner->remember(text, false);
        if(text == "quit") { break; }
    }
    screen.disable();
    ::fast_io::io::println("CURSOR RESTORED");
}
