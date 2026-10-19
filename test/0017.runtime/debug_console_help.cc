// Production help/parser/editor/terminal adapter, without launching a guest VM.
#include <uwvm2/uwvm/debugger/console.h>
#if defined(__linux__)
# include <uwvm2/uwvm/debugger/linux_control_fd.h>
#endif
#include <fast_io.h>
#include <memory>
namespace dbg = uwvm2::uwvm::debugger;
namespace help = dbg::console_help;
namespace ed = dbg::console_editing;
using view = ::fast_io::string_view;
static void check(bool good, view label)
{ if(!good) { ::fast_io::io::perrln("FAIL help: ", label); ::fast_io::fast_terminate(); } }
static void type(ed::editor& editor, view text)
{ for(unsigned char byte : text) { static_cast<void>(editor.feed(byte)); } }
static ::fast_io::string plain(view text)
{
    ::fast_io::string result{};
    for(::std::size_t i{}; i < text.size(); ++i)
    {
        if(text[i] == '\x1b')
        { check(i + 1u < text.size() && text[++i] == '[', "only SGR color sequences");
          while(++i < text.size() && text[i] != 'm') { check((text[i] >= '0' && text[i] <= '9') || text[i] == ';', "SGR parameters"); }
          check(i < text.size(), "complete color reset at page boundaries"); }
        else { result.push_back(text[i]); }
    }
    return result;
}
static void unit()
{
    check(dbg::console_aliases::uses_current_thread("fi"),"native finish obtains current stopped thread");
    for(view text : {view{"jump src 12"},view{"jump wasm 0"},view{"jump asm *0x1234"},view{"  jump   asm   16  "}})
    { check(dbg::console_aliases::parse(text,1u,2u).command.kind==dbg::console_command_kind::execution_relocate,"three-level jump grammar"); }
    for(view text : {view{"jump src 0"},view{"jump wasm -1"},view{"jump wasm *0x20"},view{"jump asm 18446744073709551616"},view{"jump asm 0xzz"}})
    { check(dbg::parse_console_command(text).kind==dbg::console_command_kind::invalid,"invalid relocation is rejected"); }
    auto const root{help::format()};
    check(root.find("help asm") != ::fast_io::containers::npos && root.find("help wasm") != ::fast_io::containers::npos,
          "root guides the four debug levels");
    check(root.find("Usage:") == ::fast_io::containers::npos && root.find("replace MODULE") == ::fast_io::containers::npos,
          "root does not dump every command");
    ::std::size_t lines{}; for(char c : root) { if(c == '\n') { ++lines; } }
    check(lines <= 22u, "root fits ordinary terminal height");
    check(help::format("r") == help::format("execution run"), "aliases lead to one command page");
    for(view name:{view{"layout register"},view{"layout registers"},view{"layout regs"}})
    { check(help::format(name)==help::format("layout"),"register layout help aliases"); }
    check(help::format("pri") == help::format("print"), "help expands the same unique command prefix");
    check(help::format("thr") == help::format("thread"), "incomplete command prefix has discoverable help");
    check(help::format("reg") == help::format("register"), "register prefix help lists its subcommand");
    check(help::format("thread").find("thread list") != ::fast_io::containers::npos, "thread menu displays commands to type");
    check(help::format("set").find("set wasip1") != ::fast_io::containers::npos, "set menu includes both guest levels");
    for(auto input : {view{"thr"}, view{"proc"}, view{"reg"}, view{"br"}, view{"inf"}, view{"info wasi"}, view{"set wasip1"}, view{"path"}})
    { check(help::menu_request(input) != nullptr, "bare prefixes resolve to static menus"); }
    for(auto input : {view{"r"}, view{"c"}, view{"s"}, view{"n"}, view{"f"}, view{"p"}, view{"i r"}, view{"thread list"}, view{"reg r"}, view{"br list"}, view{"set wasm memory 0 0 1 0 bytes 01"}, view{"info w"}, view{"pa"}})
    { check(help::menu_request(input) == nullptr, "menus do not intercept executable, ambiguous or argument-bearing commands"); }
    for(auto const& menu : help::prefix_menus)
    { check(help::find(menu.topic) != nullptr && help::menu_request(menu.command) != nullptr, "every menu is reachable and has documentation"); }
    for(auto const& choice : help::menu_choices)
    { check(help::find(choice.parent) != nullptr && help::find(choice.topic) != nullptr, "every menu choice has a valid help target");
      auto documented{help::format(choice.command)}; check(!documented.starts_with("Unknown") && !documented.starts_with("Ambiguous"), "every offered command has help"); }
    check(help::format("i lo") == help::format("info locals"), "scoped command prefix help");
    check(help::format("thread step-ov") == help::format("next"), "LLDB command prefix help");
    check(help::format("wasm set memo") == help::format("wasm set memory"), "hierarchical category prefix help");
    check(help::format("language v pri full 2", false, 80u, 4u) == help::format("print full 2", false, 80u, 4u), "prefix help retains full/page suffixes");
    check(help::format("st") == help::format("start") && help::format("stw") == help::format("startw") &&
          help::format("sti") == help::format("starti"), "explicit launch aliases have exact help");
    auto ambiguous{help::format("sta")};
    check(ambiguous.starts_with("Ambiguous") && ambiguous.find("help status") != ::fast_io::containers::npos &&
          ambiguous.find("help startw") != ::fast_io::containers::npos, "ambiguous help lists executable command names");
    ambiguous = help::format("info w");
    check(ambiguous.find("help info wasm-events") != ::fast_io::containers::npos && ambiguous.find("help info wasip1") != ::fast_io::containers::npos, "scoped help ambiguity");
    ambiguous = help::format("wasm set m");
    check(ambiguous.find("help wasm set memory") != ::fast_io::containers::npos && ambiguous.find("help wasm set member") != ::fast_io::containers::npos, "nested category ambiguity");
    check(help::format("pri").find("counter =") == ::fast_io::containers::npos, "help never evaluates expressions");
    for(auto const& sample : {::std::pair<view,view>{"prinnt", "print"}, {"thr lsit", "thread list"},
         {"i waspi1 args", "info wasip1 args"}, {"info wasip1 agrs", "info wasip1 args"},
         {"waspi1 inspect args", "wasip1 inspect args"}, {"wasm set memroy", "wasm set memory"},
         {"set wasip1 restroe", "set wasip1 restore"}, {"wasip2", "wasip1"}})
    {
        auto text{help::format(sample.first)};
        bool const offered{text.starts_with("Unknown") && text.find("Did you mean:") != ::fast_io::containers::npos &&
            text.find(::fast_io::concat_fast_io("  help ", sample.second, "\n").subview(0u)) != ::fast_io::containers::npos &&
            text.find("Usage:") == ::fast_io::containers::npos};
        if(!offered) { ::fast_io::io::perrln("selector: ", sample.first, "\n", text); }
        check(offered, "help offers a copyable static correction without silently choosing its page");
        auto suggestions{help::suggest_topic(sample.first)};
        check(suggestions.size != 0u && suggestions.size <= 3u, "at most three help suggestions");
        for(::std::size_t i{}; i < suggestions.size; ++i)
        { check(help::resolve_topic(suggestions.names[i]).selected != nullptr, "every suggestion resolves to real documentation"); }
    }
    check(help::format("set wasip1 restroe full 2").find("help set wasip1 restore full 2") != ::fast_io::containers::npos,
          "corrections preserve the full-detail and page request");
    for(auto selector : {view{"set wasm memory 0 0 1 0 bytes reaad"}, view{"print counter"},
         view{"waspi1 inspect agrs"}, view{"wasm set memroy 0"}, view{"wasm set memroy 1x"},
         view{"wasm set memroy 184467440737095516160"}, view{"set wasip1 restroe\x1b[2J"}})
    {
        auto text{help::format(selector)};
        check(text.find("Did you mean:") == ::fast_io::containers::npos && text.find("\x1b") == ::fast_io::containers::npos,
              "help never corrects arguments, multiple typos, invalid pages or terminal control bytes");
    }
    check(help::single_typo("waspi1", "wasip1") && help::single_typo("wasip2", "wasip1") &&
          !help::single_typo("waspi2", "wasip1") && !help::single_typo("wasip1", "wasip1"),
          "digit-containing names share the bounded single-edit rule");
    check(help::search("NATIVE register") == help::search("native register"), "case insensitive literal AND search");
    auto search{help::search("native register")};
    check(search.find("help asm registers") != ::fast_io::containers::npos, "discover registers by purpose");
    auto const literal{help::search(".*")};
    check(literal.find("1 matching commands") != ::fast_io::containers::npos &&
          literal.find("help language values print") != ::fast_io::containers::npos,
          "literal dot-star finds Zig p.* documentation, not every command");
    check(help::search("^.*$").starts_with("No matching"), "search syntax is literal, not regexp");
    check(help::search("nomatchunlikely").starts_with("No matching"), "no-result search offers guidance");
    for(auto bad : {view{}, view{"register 0"}, view{"register 1x"}, view{"register 184467440737095516160"}, view{"register\x1b[2J"}})
    { auto text{help::search(bad)}; check(text.starts_with("error:") && text.find("\x1b") == ::fast_io::containers::npos, "bounded search rejects invalid input without echo"); }
    check(help::search("register 999").find("out of range") != ::fast_io::containers::npos, "search rejects nonexistent page");
    ::fast_io::string long_search{}; for(::std::size_t i{}; i < 129u; ++i) { long_search.push_back('a'); }
    check(help::search(long_search.subview(0u)).starts_with("error:"), "search input byte cap");
    ::fast_io::string all_search{}; ::std::size_t search_pages{};
    for(::std::size_t page{1u}; page <= 128u; ++page)
    { auto text{help::search(::fast_io::concat_fast_io("memory ", ::fast_io::mnp::dec(page)).subview(0u), false, 80u, 4u)};
      all_search.append(text); ++search_pages; if(text.find("Next: apropos") == ::fast_io::containers::npos) { break; } }
    check(search_pages > 1u && search_pages < 128u && all_search.find("help wasm set memory") != ::fast_io::containers::npos, "all search results are reachable by bounded pages");
    check(help::format("locals") == help::format("wasm values locals"), "raw locals is Wasm");
    check(help::format("info locals") == help::format("language values locals"), "info locals is language");
    check(help::format("bt").find("source debug information is optional") != ::fast_io::containers::npos, "Wasm backtrace does not require DWARF");
    check(help::format("missing\x1b[2J").find("Unknown") == 0u, "untrusted selector cannot control the terminal");
    for(auto selector : {view{"asm 0"}, view{"asm 1x"}, view{"asm 999999999999999999999999999"}})
    { check(help::format(selector).find("Unknown") == 0u, "reject malformed/overflow pages"); }
    check(help::format("asm 999").find("out of range") != ::fast_io::containers::npos, "reject nonexistent page");
    for(view action : {view{"save"}, view{"load"}})
    {
        auto const command{::fast_io::concat_fast_io("layout config ", action)};
        auto const page{help::format(command.subview(0u), false, 144u, 80u)};
        check(page.find(::fast_io::concat_fast_io(command, " <filePath>").subview(0u)) != ::fast_io::containers::npos &&
              page.find("no default") != ::fast_io::containers::npos, "layout config help requires the matching explicit path");
    }
    auto members{help::format("members")};
    check(members.find("THREAD MODULE FRAME") != ::fast_io::containers::npos, "short usage keeps mandatory continuation arguments");
    auto replacement_help{help::format("replace")};
    check(replacement_help.find("BODY_FILE") != ::fast_io::containers::npos && replacement_help.find("local declarations") != ::fast_io::containers::npos, "short help explains the body file argument completely");
    auto const parsed{dbg::console_aliases::parse("h  wasm   set\tglobal  2", 1u, 7u).command};
    check(parsed.kind == dbg::console_command_kind::help && view{parsed.help_topic.data(), parsed.help_topic_size} == "wasm set global 2",
          "owned normalized help selector");
    auto const too_long{::fast_io::concat_fast_io("help ", "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz")};
    check(dbg::parse_console_command(too_long.subview(0u)).kind == dbg::console_command_kind::invalid, "bounded selector");
    ::std::size_t commands{}, examples{}, invalid_examples{};
    dbg::console_tui::screen panel{nullptr, +[](void*, view) noexcept {}, false};
    for(auto const& item : help::entries)
    {
        check(!item.key.empty() && !item.title.empty() && !item.summary.empty(), "every page has a clear purpose");
        for(auto const& other : help::entries) { if(&other != &item) { check(other.key != item.key, "unique topic keys"); } }
        if(!item.parent.empty()) { auto parent{help::find(item.parent)}; check(parent != nullptr && parent->group(), "parent is a category"); }
        if(item.group()) { continue; }
        ++commands; check(!item.arguments.empty() && !item.examples.empty(), "every command has arguments and examples");
        auto brief{help::format(item.key)};
        check(brief.find("Example:") != ::fast_io::containers::npos && brief.find("Full details:") != ::fast_io::containers::npos, "short help shows how to use the command before advanced details");
        ::fast_io::string complete{};
        for(::std::size_t page{1u}; page <= 128u; ++page)
        {
            auto selector{::fast_io::concat_fast_io(item.key, " full ", ::fast_io::mnp::dec(page))};
            auto output{help::format(selector.subview(0u), false, 80u, 4u)};
            check(output.find("Unknown") != 0u, "all catalogue selectors are reachable");
            complete.append(output);
            if(output.find("Next: help") == ::fast_io::containers::npos) { break; }
            check(page != 128u, "bounded pagination");
        }
        check(complete.find("Usage:") != ::fast_io::containers::npos && complete.find("Arguments:") != ::fast_io::containers::npos &&
              complete.find("Examples:") != ::fast_io::containers::npos, "all leaf sections survive pagination");
        if(!item.command_examples) { continue; }
        auto remaining{item.examples};
        while(!remaining.empty())
        {
            auto split{remaining.find("\n")}; auto example{remaining.subview(0u, split == ::fast_io::containers::npos ? remaining.size() : split)};
            auto resume{dbg::console_execution::classify_resume(example)};
            auto parsed_example{dbg::console_aliases::parse(example, 1u, 7u)};
            auto normalized_example{dbg::console_aliases::normalize(example)};
            // Function-name breakpoints resolve through the console's separate
            // stopped metadata lookup; TUI commands through its panel adapter.
            auto display{dbg::console_displays::parse(example)};
            bool accepted{resume.recognized || parsed_example.command.kind != dbg::console_command_kind::invalid ||
                (normalized_example.valid && normalized_example.view() == "breakwasm 0 main") ||
                (display.operation != dbg::console_displays::action::none && display.operation != dbg::console_displays::action::invalid) || example=="list src" || example=="list wasm" || example=="list asm" || panel.command(example)};
            if(!accepted) { ::fast_io::io::perrln("Example page=", item.key, " text=", example); }
            if(!accepted) { ++invalid_examples; } ++examples;
            if(split == ::fast_io::containers::npos) { break; } remaining = remaining.subview(split + 1u);
        }
    }
    check(invalid_examples == 0u, "documented examples use accepted command syntax");
    for(auto const& alias : help::aliases)
    { check(alias.target.empty() || help::find(alias.target) != nullptr, "help aliases have real targets"); }
    auto owner{::std::make_unique<ed::editor>()}; auto& editor{*owner};
    editor.begin(); type(editor, "help"); check(editor.feed(9u) == ed::action::changed && editor.line().view() == "help ", "help Tab opens categories");
    type(editor, "asm"); check(editor.feed(9u) == ed::action::changed && editor.line().view() == "help asm ", "group Tab opens commands");
    type(editor, "reg"); check(editor.feed(9u) == ed::action::changed && editor.line().view() == "help asm registers", "nested command completion");
    editor.begin(); type(editor, "help source"); check(editor.feed(9u) == ed::action::changed && editor.line().view() == "help language ", "group aliases retain child navigation");
    editor.begin(); type(editor, "apr"); check(editor.feed(9u) == ed::action::changed && editor.line().view() == "apropos ", "command discovery has ordinary Tab completion");
    editor.begin(); type(editor, "h wasm "); check(editor.feed(9u) == ed::action::candidates, "nested ambiguous completions");
    ::std::size_t candidates{}; editor.display_candidates([&](view text) { check(text.starts_with("h wasm "), "only immediate help children"); ++candidates; });
    check(candidates >= 7u && candidates < 20u, "bounded relevant candidate list");
    bool const initial{::uwvm2::uwvm::utils::ansies::put_color};
    ::uwvm2::uwvm::utils::ansies::put_color = true;
    auto const colored{help::format({}, true)};
    check(colored.find("\x1b[92m") != ::fast_io::containers::npos && colored.find("\x1b[96m") != ::fast_io::containers::npos &&
          colored.find("\x1b[0m") != ::fast_io::containers::npos && plain(colored.subview(0u)) == root, "project palette preserves readable layout");
    ::uwvm2::uwvm::utils::ansies::put_color = false;
    check(help::format({}, true) == root, "global no-color policy honored");
    ::uwvm2::uwvm::utils::ansies::put_color = initial;
    for(::std::size_t width{1u}; width <= 100u; ++width)
    {
        auto const text{help::format("wasm set global", false, width, 4u)};
        ::std::size_t length{}; for(char c : text)
        { if(c == '\n') { check(length <= (width > 80u ? 80u : width), "help wraps to terminal width"); length = 0u; } else { ++length; } }
        auto const menu_text{help::format("thread", false, width, 4u)}; length = 0u;
        for(char c : menu_text)
        { if(c == '\n') { check(length <= (width > 80u ? 80u : width), "prefix menus wrap to terminal width"); length = 0u; } else { ++length; } }
        for(auto selector : {view{"advance"}, view{"native register"}, view{"memory"}, view{"memory 999"}, view{}, view{"register\x1b[2J"}})
        { auto result{help::search(selector, false, width, 4u)}; length = 0u; for(char c : result)
          { if(c == '\n') { check(length <= (width > 80u ? 80u : width), "search and errors wrap to terminal width"); length = 0u; } else { ++length; } } }
    }
    ::fast_io::io::println("PASS help unit pages=", ::fast_io::mnp::dec(commands), " examples=", ::fast_io::mnp::dec(examples));
}
int main(int argc, char** argv)
{
    if(argc != 2) { return 2; } view mode{::fast_io::mnp::os_c_str(argv[1])};
    if(mode == "unit") { unit(); return 0; }
    if(mode == "render") { ::fast_io::print(::fast_io::out(), help::format({}, true)); return 0; }
    if(mode != "line" && mode != "tui") { return 2; }
    dbg::console_keyboard::native_session keyboard{};
    if(!keyboard.ready() || !keyboard.interactive_display()) { return 3; }
    auto write{+[](void*, view text) noexcept { ::fast_io::print(::fast_io::out(), text); }};
    dbg::console_io io{::std::addressof(keyboard), +[](void* p) noexcept { return static_cast<dbg::console_keyboard::native_session*>(p)->read_byte(); }, write, true};
    dbg::console_tui::screen screen{nullptr, write, true}; if(mode == "tui") { screen.enable(); }
    dbg::console_read_hooks hooks{::std::addressof(screen), nullptr, +[](void* p, ed::editor const& editor, ed::action)
    { auto& current{*static_cast<dbg::console_tui::screen*>(p)}; if(!current.enabled()) { return false; } current.render(editor); return true; }};
    auto owner{::std::make_unique<ed::editor>()}; ::fast_io::io::println("EDITOR READY");
    for(unsigned count{}; count != 24u; ++count)
    {
        auto line{dbg::read_console_line(io, *owner, hooks)};
        if(line.status == dbg::line_status::end) { break; } if(line.status != dbg::line_status::complete) { return 4; }
        view text{line.bytes.data(), line.size}; if(text == "quit") { break; }
        auto command{dbg::console_aliases::parse(text, 1u, 7u).command};
        if(command.kind != dbg::console_command_kind::help) { return 5; }
        auto size{dbg::console_tui::terminal_dimensions()};
        auto output{help::format(view{command.help_topic.data(), command.help_topic_size}, !screen.enabled(), size.columns,
            size.rows < 23u ? (size.rows > 5u ? size.rows - 5u : 4u) : 18u)};
        screen.output(output); owner->remember(text, false);
        ::fast_io::io::println("\r\nHELP DONE");
    }
    screen.disable(); ::fast_io::io::println("CURSOR RESTORED");
}
