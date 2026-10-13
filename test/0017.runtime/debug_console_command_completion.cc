// Command words are HOST input. This test never creates or executes a guest.
#include <uwvm2/uwvm/debugger/console_line_editor.h>
#include <fast_io.h>
#include <memory>
namespace editing = uwvm2::uwvm::debugger::console_editing;
using view = fast_io::string_view;
static void check(bool good, view label)
{ if(!good) { fast_io::io::perrln("FAIL command completion: ", label); fast_io::fast_terminate(); } }
static void type(editing::editor& editor, view text)
{ for(unsigned char c : text) { static_cast<void>(editor.feed(c)); } }
int main()
{
    auto owner{std::make_unique<editing::editor>()}; auto& editor{*owner};
    struct sample { view input{}, expected{}; };
    constexpr sample samples[]{
        {"thr li", "thr list"}, {"proc st", "proc status"}, {"reg re", "reg read "},
        {"br li", "br list"}, {"br ena", "br enable "}, {"fr var", "fr variable "},
        {"i lo", "i locals"}, {"i all-r", "i all-registers"}, {"inf wasi", "inf wasip1 "},
        {"i wasi ar", "i wasi args "}, {"set wasm memo", "set wasm memory "},
        {"set wasip1 check", "set wasip1 checkpoint "}, {"set wasip1 fd-d", "set wasip1 fd-dup "},
        {"set wasip1 arg", "set wasip1 arg "}, {"unset wasip1 fd", "unset wasip1 fd "},
        {"trace wasi", "trace wasip1 "}, {"trace wasm of", "trace wasm off"},
        {"trace wasip1 re", "trace wasip1 read "}, {"path cr", "path create "},
        {"layout as", "layout asm"}, {"breakwasm", "breakwasm "},
        {"help  asm   reg", "help  asm   registers"}, {"  h   wasm    set    memo", "  h   wasm    set    memory"},
        {"help   source", "help   language "}, {"help  thread  step-ov", "help  thread  step-over"}};
    for(auto const& sample : samples)
    {
        editor.begin(); type(editor, sample.input);
        check(editor.feed(9) == editing::action::changed && editor.line().view() == sample.expected, sample.input);
        check(editor.feed(31) == editing::action::changed && editor.line().view() == sample.input,
              "completion is one undo step");
    }
    for(auto word : {view{"st"}, view{"stw"}, view{"sti"}, view{"start"}, view{"startw"}, view{"starti"}})
    { editor.begin(); type(editor, word); auto const result{editor.feed(9)};
      check((result == editing::action::unchanged || result == editing::action::changed) && editor.line().view() == word && editor.cursor() == word.size(),
          "exact launch commands and aliases do not expand into unrelated commands");
      check(editor.feed(31) == editing::action::changed && editor.line().view().empty(), "exact command redraw adds no undo record"); }
    editor.begin(); type(editor, "star");
    check(editor.feed(9) == editing::action::changed && editor.line().view() == "start", "launch common prefix");
    check(editor.feed(9) == editing::action::unchanged, "exact start wins over longer launch commands");
    editor.begin(); type(editor, "thread step-in");
    check(editor.feed(9) == editing::action::unchanged && editor.line().view() == "thread step-in",
          "exact source step does not expand into native step");
    editor.begin(); type(editor, "set ");
    check(editor.feed(9) == editing::action::candidates && editor.line().view() == "set ", "source and Wasm settings share no prefix");
    check(editor.feed(9) == editing::action::candidates, "next Tab lists the two choices");
    std::size_t count{}; bool wasm{}, wasip1{}, source{};
    editor.display_candidates([&](view name) { ++count; wasm = wasm || name == "set wasm"; wasip1 = wasip1 || name == "set wasip1"; source=source || name=="set source"; });
    check(count == 3u && wasm && wasip1 && source, "set candidates show only the next level");
    editor.begin(); type(editor, "i wasi "); count = 0u;
    check(editor.feed(9) == editing::action::candidates, "scoped abbreviated inspection candidates");
    editor.display_candidates([&](view name) { ++count; check(name.starts_with("i wasi ") && name.subview(7u).find(" ") == fast_io::containers::npos,
         "candidate uses the typed alias and one child"); });
    check(count == 5u, "all five WASIp1 inspection choices");
    editor.begin(); count = 0u;
    check(editor.feed(9) == editing::action::candidates, "empty Tab lists root commands");
    editor.display_candidates([&](view name) { ++count; check(name.find(" ") == fast_io::containers::npos, "root list does not dump descendant commands"); });
    check(count > 20u && count < 128u, "bounded root vocabulary");
    constexpr view original{"set wasm memoZZ 0 0 1 0 bytes 0102"};
    editor.begin(); type(editor, original); static_cast<void>(editor.feed(1));
    for(std::size_t i{}; i != original.find("ZZ"); ++i) { static_cast<void>(editor.feed(6)); }
    check(editor.feed(9) == editing::action::changed && editor.line().view() == "set wasm memory 0 0 1 0 bytes 0102" && editor.cursor() == 15u,
          "mid-word completion preserves all following arguments and the caret");
    check(editor.feed(31) == editing::action::changed && editor.line().view() == original && editor.cursor() == original.find("ZZ"),
          "undo restores the original suffix and caret");
    editor.begin(); type(editor, "thr listZZ"); static_cast<void>(editor.feed(2)); static_cast<void>(editor.feed(2));
    check(editor.feed(9) == editing::action::changed && editor.line().view() == "thr list", "exact word before caret removes only the token suffix");
    for(auto text : {view{"memory 0 0 "}, view{"set wasip1 env 0 4d4f4445 v"}, view{"wasm-script status; thread "}, view{"st list "}})
    { editor.begin(); type(editor, text); auto actual{editor.feed(9)};if(actual!=editing::action::unchanged) { std::fprintf(stderr,"completion for %.*s returned %u\n",int(text.size()),text.data(),unsigned(actual)); } check(actual == editing::action::unchanged && editor.line().view() == text,
          "numeric/hex/script arguments and ambiguous commands are not rewritten"); }
    for(auto text : {view{"pri coun"}, view{"fr v record.co"}, view{"rep 0 1 1 /tmp/up"}})
    { editor.begin(); type(editor, text); check(editor.feed(9) == editing::action::completion && editor.line().view() == text,
          "source/path arguments still delegate without changing bytes"); }
    check(editing::command_matches("thr \x1b[2J", 8u).size == 0u, "terminal controls never become candidates");
    for(auto separator : {view{" "}, view{"\t"}, view{"\n"}, view{"\r"}, view{"\v"}, view{"\f"}})
    {
        auto const input{fast_io::concat_fast_io(separator, "help", separator, separator, "asm", separator, separator, "reg")};
        auto const matches{editing::help_matches(input.subview(0u), input.size())};
        check(matches.recognized && matches.size == 1u && matches.words[0u].text == "registers" &&
              matches.begin == input.size() - 3u && matches.end == input.size(), "every FastIO c_space separator has the same help match and raw token bounds");
        auto const words{uwvm2::uwvm::debugger::console_aliases::split(input.subview(0u))};
        check(words.word == "help", "command roots skip every FastIO c_space character");
        auto const selector{fast_io::concat_fast_io(separator, "asm", separator, separator, "reg")};
        std::size_t offered{};
        uwvm2::uwvm::debugger::console_help::visit_completions(selector.subview(0u), [&](view name)
        { check(name == "asm registers", "catalogue candidates contain only static words and printable spaces"); ++offered; });
        check(offered == 1u, "catalogue and editor use the same c_space matching");
    }
    editor.begin(); type(editor, "help   wasm   set   ");
    check(editor.feed(9) == editing::action::candidates, "trailing repeated separators list the next help level");
    count = 0u; editor.display_candidates([&](view name)
    { ++count; check(name.starts_with("help wasm set ") && name.subview(14u).find(" ") == fast_io::containers::npos,
                   "help choices list only the immediate level in copyable spelling"); });
    check(count == 4u && editor.line().view() == "help   wasm   set   ", "candidate listing preserves repeated input separators");
    constexpr view raw_help{"  help  asm   regZZ   full 2"};
    constexpr view completed_help{"  help  asm   registers   full 2"};
    editor.begin(); type(editor, raw_help); static_cast<void>(editor.feed(1));
    for(std::size_t i{}; i < raw_help.find("ZZ"); ++i) { static_cast<void>(editor.feed(6)); }
    check(editor.feed(9) == editing::action::changed && editor.line().view() == completed_help &&
          editor.cursor() == completed_help.find("   full"), "help completion replaces only the caret token and retains full/page suffixes");
    check(editor.feed(31) == editing::action::changed && editor.line().view() == raw_help && editor.cursor() == raw_help.find("ZZ"),
          "one undo restores repeated help separators, the token suffix and the caret");
    check(!editing::help_matches("helper  asm reg", 15u).recognized && editing::help_matches("help  asm \x1b[2J", 14u).size == 0u,
          "non-help roots and terminal controls do not gain help candidates");
    for(auto const& choice : uwvm2::uwvm::debugger::console_help::menu_choices)
    {
        bool found{}; for(auto const& word : editing::completion_words) { found = found || word.text == choice.command; }
        // Intermediate choices are synthesized from their descendant words.
        if(!found) { for(auto const& word : editing::completion_words)
            { found = found || (word.text.starts_with(choice.command) && word.text.size() > choice.command.size() && word.text[choice.command.size()] == ' '); } }
        check(found, "every displayed submenu has a Tab route");
    }
    fast_io::io::println("PASS static command completion, hierarchy, aliases, arguments, caret and undo");
}
