#include <uwvm2/uwvm/debugger/console_tui.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <uwvm2/uwvm/debugger/wasm_listing.h>
#include <memory>
#include <cstdio>
using namespace uwvm2::uwvm::debugger;
static void require(bool value, char const* label)
{ if(!value) { std::fprintf(stderr,"FAIL %s\n",label); std::abort(); } }
int main()
{
    fast_io::string output{};
    auto editor=std::make_unique<console_editing::editor>(); editor->begin();
    console_tui::screen screen{&output,+[](void* p,fast_io::string_view text) noexcept { static_cast<fast_io::string*>(p)->append(text); },true};
    require(!screen.custom_layout(),"initial automatic layout");
    require(screen.command("layout config save   ") && !screen.custom_layout(),"missing save path retains automatic layout");
    require(screen.command("layout config load   ") && !screen.custom_layout(),"missing load path retains automatic layout");
    uwvm2::uwvm::utils::ansies::put_color=true;
    for(auto name:{fast_io::string_view{"register"},fast_io::string_view{"registers"},fast_io::string_view{"regs"}})
    {
        require(screen.command(fast_io::concat_fast_io("layout ",name).subview(0u)) && screen.wants(console_tui::layout::registers),"register layout aliases");
        require(screen.command(fast_io::concat_fast_io("focus ",name).subview(0u)),"register focus aliases");
        screen.set_value_panel(console_tui::layout::registers,"\x1b[96mpc\x1b[0m = \x1b[93m0x1234\x1b[0m\n");
        screen.disable();screen.enable();
        output.clear();screen.render(*editor,{80u,24u});
        require(output.find("regs *]")!=fast_io::containers::npos && output.find("96mpc")!=fast_io::containers::npos,"register pane keeps formatted colour");
        require(screen.command(fast_io::concat_fast_io("scroll ",name," down 4096").subview(0u)),"register scroll aliases");
        screen.set_value_panel(console_tui::layout::registers,"Wasm stop; use si or ni for native registers\n");
        output.clear();screen.render(*editor,{80u,24u});
        require(output.find("Wasm stop;")!=fast_io::containers::npos,"shrinking register pane clamps scroll");
        console_layout::tree tree{};
        require(tree.parse(fast_io::concat_fast_io("h50(asm,",name,")").subview(0u)) && tree.serialize()=="h50(asm,regs)","register tree aliases serialize canonically");
    }
    screen.output("\x1b[95mPRESERVED_COLOR\x1b[0m\n");
    require(screen.command("  layout   wasm  locals stack memory"),"layout grammar");
    require(screen.wants(console_tui::layout::wasm) && screen.wants(console_tui::layout::operands) && screen.wants(console_tui::layout::memory),"composable panes");
    screen.set_panel(console_tui::layout::wasm,"=> 4 local.get 1\nN> 6 i32.add\n");
    screen.set_panel(console_tui::layout::memory,"memory 00 01\n");
    output.clear();screen.render(*editor,{100u,40u});
    require(output.find("95mPRESERVED_COLOR")!=fast_io::containers::npos,"history color retained");
    require(screen.command("focus stack"),"focus stack");output.clear();screen.key(console_editing::action::redraw);screen.render(*editor,{100u,40u});
    require(output.find("operands *]")!=fast_io::containers::npos,"independent operand focus");
    require(screen.command("layout horizontal src asm"),"horizontal layout");
    require(screen.command("memory 0 0 32 16") && screen.memory_query()=="memory 0 0 32 16","memory selection");
    uwvm2::uwvm::utils::ansies::put_color=false;output.clear();screen.render(*editor,{80u,24u});
    require(output.find("\x1b[95m")==fast_io::containers::npos,"NO_COLOR history");
    screen.disable();output.clear();screen.show_candidates("one\ntwo");
    require(output.find("one")!=fast_io::containers::npos && output.find("\x1b[?1049")==fast_io::containers::npos,"shell completion below input");
    auto request=console_completion::context("  bs   0 suffix",8u);
    require(request.type==console_completion::kind::module && request.begin==7u && request.end==8u,"module id completion preserves repeated spaces and suffix");
    for(auto action:{fast_io::string_view{"save"},fast_io::string_view{"load"}})
    { auto input{fast_io::concat_fast_io("layout config ",action," ")};auto completion{console_completion::context(input.subview(0u),input.size())};
      require(completion.type==console_completion::kind::path && completion.prefix.empty(),"required config path completion");
      output.clear();require(screen.command(input.subview(0u)),"missing config path consumed");
      require(output.find("<filePath>")!=fast_io::containers::npos,"missing config path diagnosed"); }
    auto path=fast_io::string_view{"set source /tmp/example"};request=console_completion::context(path,path.size());
    require(request.type==console_completion::kind::path && request.prefix=="/tmp/example","canonical source path completion");
    editor->begin();for(char c:path) { static_cast<void>(editor->feed(c)); }
    require(editor->feed(9)==console_editing::action::completion,"source path reaches dynamic provider");
    request=console_completion::context("layout wasm sta",15u);
    require(request.type==console_completion::kind::pane && request.prefix=="sta","composed layout completion");
    for(auto name:{fast_io::string_view{"sw"},fast_io::string_view{"nw"},fast_io::string_view{"fw"},fast_io::string_view{"step wasm"},fast_io::string_view{"next wasm"}})
    { auto parsed=console_aliases::parse(name,1u,2u);require(parsed.command.kind!=console_command_kind::invalid,"level aliases"); }
    require(console_stdio::queue_input("hello"),"queue stdin");
    char data[6]{};fast_io::io_scatter_t scatter{data,6u};std::size_t read{};
    require(console_stdio::read(0,&scatter,1u,read) && read==6u && fast_io::string_view{data,6u}=="hello\n","isolated stdin");
    screen.enable();require(console_stdio::write(1,&scatter,1u),"stdout captured");require(screen.drain_guest(),"guest IO drained");
    output.clear();screen.render(*editor,{80u,24u});require(output.find("[stdout] hello")!=fast_io::containers::npos,"guest IO pane");
    editor->begin();for(char ch : fast_io::string_view{"  status"}) { static_cast<void>(editor->feed(ch)); }
    console_editing::action event{};
    for(char ch : fast_io::string_view{"\x1b[<65;7;12M"}) { event=editor->feed(ch); }
    require(event==console_editing::action::mouse && editor->mouse().button==65u && editor->mouse().column==7u && editor->mouse().row==12u && editor->line().view()=="  status","SGR wheel preserves draft");
    for(char ch : fast_io::string_view{"\x1b[<0;120;40m"}) { event=editor->feed(ch); }
    require(event==console_editing::action::mouse && editor->mouse().released,"SGR release");
    for(char ch : fast_io::string_view{"\x1b[<1;999999;4M"}) { event=editor->feed(ch); }
    require(event!=console_editing::action::mouse && editor->line().view()=="  status","malformed mouse cannot become command text");
    screen.disable();require(!console_stdio::write(1,&scatter,1u),"plain output uses original stream");
    std::puts("PASS composable TUI, color, completion, level aliases, isolated IO");
}
