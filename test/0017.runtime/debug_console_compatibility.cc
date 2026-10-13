// HOST editing and command grammar regression. Runtime/PTY admission is tested separately.
#include <uwvm2/uwvm/debugger/console_line_editor.h>
#include <uwvm2/uwvm/debugger/console_aliases.h>
#include <uwvm2/uwvm/debugger/console_execution.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <fast_io.h>
#include <initializer_list>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace ed = dbg::console_editing;
void type(ed::editor& e, ::fast_io::string_view text) { for(unsigned char ch : text) { (void)e.feed(ch); } }
bool equal(ed::editor const& e, ::fast_io::string_view text) { return e.line().view() == text; }
int main()
{
    ed::editor e;
    e.begin(); type(e,"breakwasm"); if(e.feed(9) != ed::action::changed || !equal(e,"breakwasm ")) { return 1; }
    (void)e.feed(31); if(!equal(e,"breakwasm")) { return 2; }
    e.begin(); type(e,"statsu"); (void)e.feed(20); if(!equal(e,"status")) { return 3; }
    e.begin(); type(e,"a\xc3\xa9"); (void)e.feed(20); if(!equal(e,"\xc3\xa9" "a")) { return 4; }
    e.begin(); type(e,"one two"); type(e,"\x1b[1;5D"); type(e,"X"); if(!equal(e,"one Xtwo")) { return 5; }
    type(e,"\x1b[1;5C"); type(e,"!"); if(!equal(e,"one Xtwo!")) { return 6; }
    e.begin(); type(e,"abcdef"); type(e,"\x1b" "3"); (void)e.feed(2); type(e,"X"); if(!equal(e,"abcXdef")) { return 7; }
    e.begin(); type(e,"abcdef"); (void)e.feed(1); type(e,"\x1b-2"); (void)e.feed(2); type(e,"X"); if(!equal(e,"abXcdef")) { return 8; }
    e.begin(); type(e,"\x1b" "3x"); if(!equal(e,"xxx")) { return 9; }
    e.begin(); type(e,"sta"); (void)e.feed(22); if(e.feed('\n') == ed::action::complete || !equal(e,"sta ")) { return 10; }
    type(e,"tus"); if(e.feed('\n') != ed::action::complete) { return 11; }
    e.begin(); type(e,"\x1b[200~status\nquit\tforce\x1b[201~");
    if(!equal(e,"status quit force") || e.feed('\n') != ed::action::complete) { return 12; }
    if(dbg::console_aliases::parse(e.line().view(),42,7).command.kind != dbg::console_command_kind::invalid) { return 13; }
    e.begin(); type(e,"replace 0 1 1 /tmp/body\x1b[200~\nquit"); if(e.feed(-1) != ed::action::oversized) { return 14; }
    e.begin(); type(e,"replace 0 1 1 /tmp/body\x1b[1;99D"); if(e.feed('\n') != ed::action::oversized) { return 15; }
    e.begin(); type(e,"\x1b[200~"); for(unsigned i{}; i != ed::maximum_bytes + 1u; ++i) { (void)e.feed('x'); }
    type(e,"\x1b[201~"); if(e.feed('\n') != ed::action::oversized) { return 16; }
    e.remember("status",true);e.remember("help",false);e.begin();type(e,"draft");type(e,"\x1b<");
    if(!equal(e,"status")) { return 17; } type(e,"\x1b>");if(!equal(e,"draft")) { return 18; }
    type(e,"\x1b<");if(e.feed(15) != ed::action::complete) { return 19; }
    e.remember(e.line().view(),true);e.begin();if(!equal(e,"help")) { return 20; }
    struct spelling { ::fast_io::string_view in{}, out{}; };
    constexpr ::fast_io::array<spelling, 19u> spellings{{
        {"r","run"},{"ru","run"},{"cont","continue"},{"hel","help"},{"i reg","info registers"},{"info break","info breakpoints"},
        {"d 2","delete 2"},{"f 1","frame 1"},{"br l","info breakpoints"},{"bw 0 1 0","breakwasm 0 1 0"},
        {"process c","continue"},{"process i","pause"},{"process st","status"},
        {"thread l","info threads"},{"thread step-inst-over","nexti"},{"fr v x","print x"},
        {"frame select 2","frame 2"},{"register r rax","info registers rax"},{"bs 0 path with spaces.cc:42","breaksource 0 path with spaces.cc:42"}}};
    for(auto const& spelling : spellings)
    {
        auto const n{dbg::console_aliases::normalize(spelling.in)};
        if(!n.valid || n.view() != spelling.out) { ::fast_io::io::println("normalization failed: ",spelling.in); return 21; }
        if(dbg::console_aliases::parse(spelling.in,42,7).command.kind == dbg::console_command_kind::invalid) { return 22; }
    }
    for(auto text : {::fast_io::string_view{"sta"},::fast_io::string_view{"info w"},::fast_io::string_view{"thread step-i"}})
    { auto const n{dbg::console_aliases::normalize(text)}; if(n.valid || !n.ambiguous) { return 23; } }
    for(auto text : {::fast_io::string_view{"step"},::fast_io::string_view{"thread step-in"},::fast_io::string_view{"thread step-over"},
        ::fast_io::string_view{"thread step-out"},::fast_io::string_view{"thread step-inst"},::fast_io::string_view{"backtrace"},::fast_io::string_view{"fr v"}})
    {
        if(!dbg::console_aliases::uses_current_thread(text)) { return 24; }
        if(!dbg::console_aliases::parse(text,0,0).current_thread_required) { return 25; }
        auto const p{dbg::console_aliases::parse(text,42,7)};
        if(p.current_thread_required || p.command.kind == dbg::console_command_kind::invalid) { return 26; }
    }
    for(auto text : {::fast_io::string_view{"cont&"},::fast_io::string_view{"process cont &"}})
    { auto const p{dbg::console_execution::classify_resume(text)};if(!p.recognized || !p.background) { return 27; } }
    for(auto text : {::fast_io::string_view{"process cont"},::fast_io::string_view{"cont"}})
    { auto const p{dbg::console_execution::classify_resume(text)};if(!p.recognized || p.background) { return 28; } }
    for(auto text : {::fast_io::string_view{"thread select 1"},::fast_io::string_view{"br s -n main"},::fast_io::string_view{"bw main"},
        ::fast_io::string_view{"x/4x 0x1234"},::fast_io::string_view{"process continue;quit"}})
    { if(dbg::console_aliases::parse(text,42,7).command.kind != dbg::console_command_kind::invalid) { return 29; } }
    auto const explicit_wasm{dbg::console_aliases::parse("s 42",0,0)};
    if(explicit_wasm.current_thread_required || explicit_wasm.command.kind != dbg::console_command_kind::protocol) { return 30; }
    e.begin(); type(e,"ru");
    if(e.feed(9) != ed::action::changed || !equal(e,"run")) { return 31; }
    // A sole literal completion uses the editor's changed result even when
    // already complete; the no-argument command must not gain a trailing space.
    if(e.feed(9) != ed::action::changed || !equal(e,"run")) { return 32; }
    e.remember("run",false); e.begin();
    if(e.feed('\n') != ed::action::complete || !equal(e,"")) { return 33; }
    // Execution and argument completion share unique-prefix rules, while the
    // editable command token and explicit authority selectors stay untouched.
    for(auto text : {::fast_io::string_view{"pri coun"}, ::fast_io::string_view{"pt coun"}, ::fast_io::string_view{"fr v coun"}})
    {
        e.begin(); type(e,text);
        if(e.feed(9) != ed::action::completion) { return 34; }
        auto const query{dbg::console_completion::context(e.line().view(),e.cursor())};
        if(query.type != dbg::console_completion::kind::symbol || query.prefix != "coun") { return 35; }
        dbg::console_completion::candidates words{}; (void)words.add("counter",query.prefix);
        if(words.apply(e,query) != ed::action::changed || !e.line().view().ends_with("counter")) { return 36; }
        if(!e.line().view().starts_with(text.subview(0u,text.size()-4u))) { return 37; }
    }
    e.begin(); type(e,"pri 42 7 counZZ"); type(e,"\x1b[D\x1b[D");
    if(e.feed(9) != ed::action::completion) { return 38; }
    auto const middle{dbg::console_completion::context(e.line().view(),e.cursor())};
    if(middle.type != dbg::console_completion::kind::symbol || middle.selectors != "42 7 " || middle.prefix != "coun") { return 39; }
    dbg::console_completion::candidates middle_words{}; (void)middle_words.add("counter",middle.prefix);
    if(middle_words.apply(e,middle) != ed::action::changed || !equal(e,"pri 42 7 counter")) { return 40; }
    for(auto text : {::fast_io::string_view{"pr coun"}, ::fast_io::string_view{"sta coun"}, ::fast_io::string_view{"made-up coun"}})
    { if(dbg::console_completion::context(text,text.size()).type != dbg::console_completion::kind::none) { return 41; } }
    auto const member_query{dbg::console_completion::context("fr v record.co",14u)};
    if(member_query.type != dbg::console_completion::kind::symbol || member_query.root != "record" || member_query.prefix != "co") { return 42; }
    auto const path_query{dbg::console_completion::context("rep 0 1 1 /tmp/body",19u)};
    if(path_query.type != dbg::console_completion::kind::path || path_query.prefix != "/tmp/body") { return 43; }
    auto const ambiguity{dbg::console_aliases::ambiguity_error("thread step-i")};
    if(ambiguity.find("  thread step-in\n  thread step-inst\n  thread step-inst-over\n") == ::fast_io::containers::npos ||
       ambiguity.find("No command was executed") == ::fast_io::containers::npos) { return 44; }
    constexpr ::fast_io::array<::fast_io::string_view,10u> many{{"test-a","test-b","test-c","test-d","test-e","test-f","test-g","test-h","test-i","test-j"}};
    bool ambiguous{}; dbg::console_aliases::ambiguity_candidates bounded{};
    if(!dbg::console_aliases::resolve("test-",many,ambiguous,&bounded).empty() || !ambiguous ||
       bounded.size != 8u || !bounded.truncated) { return 45; }
    if(dbg::console_aliases::normalize("bw 0 1 0").view() != "breakwasm 0 1 0" ||
       dbg::console_aliases::parse("bw 0 1 0",0,0).command.kind != dbg::console_command_kind::protocol ||
       dbg::console_aliases::normalize("bs 0 sample.cc:42").view() != "breaksource 0 sample.cc:42") { return 46; }
    if(dbg::console_aliases::parse("bs 0 sample.cc:42 if counter > 1",0,0).command.kind != dbg::console_command_kind::source_breakpoint ||
       dbg::console_aliases::parse("bs 0 sample.cc:42 ignore 2",0,0).command.breakpoint_ignore != 2u) { return 47; }
    ::fast_io::io::println("debug_console_compatibility: PASS");
}
