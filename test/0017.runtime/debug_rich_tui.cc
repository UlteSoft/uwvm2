#include <uwvm2/uwvm/debugger/console_tui.h>
#include <cstdio>
#include <memory>
using namespace uwvm2::uwvm::debugger;
static void require(bool value,char const* why) { if(!value) { std::fprintf(stderr,"FAIL %s\n",why);std::abort(); } }
int main()
{
    console_layout::tree tree{};
    require(tree.parse("v70(h50(src,memory),io)"),"nested tree");
    auto original{tree.serialize()};
    require(!tree.parse("h50(src,src)") && tree.serialize()==original,"duplicate rejected transactionally");
    require(!tree.parse("h0(src,asm)") && tree.serialize()==original,"zero ratio rejected");
    require(!tree.parse("h50(src,asm)trailing"),"trailing syntax rejected");
    require(tree.move(6u,0u,"below"),"move collapses and splits");
    unsigned count{};tree.visit(tree.root,{0u,1u,3u,120u,30u},[&](console_layout::rectangle r)
    { require(r.width>=20u && r.height>=3u && r.left+r.width<=121u && r.top+r.height<=33u,"pane bounds");++count; });
    require(count==3u,"all panes after move");
    std::byte data[]{std::byte{65},std::byte{66},std::byte{0},std::byte{255},std::byte{0x80},std::byte{0},std::byte{0},std::byte{0}};
    auto dump{console_memory::dump(data,32u,100u)};
    require(dump.find("0x0000000000000020")!=fast_io::containers::npos && dump.find("41 42 00 ff")!=fast_io::containers::npos && dump.find("|AB......|")!=fast_io::containers::npos,"address hex ascii");
    auto types{console_memory::interpret({data+3u,1u},"all")};
    require(types.find("i8=-1")!=fast_io::containers::npos && types.find("u8=255")!=fast_io::containers::npos && types.find("needs 4 bytes")!=fast_io::containers::npos,"signed and insufficient width");
    std::byte surrogate[]{std::byte{0x3d},std::byte{0xd8},std::byte{0x00},std::byte{0xde}};
    require(console_memory::interpret(surrogate,"utf16").find("😀")!=fast_io::containers::npos,"UTF16 surrogate pair");
    std::byte invalid[]{std::byte{0xc0},std::byte{0x80}};
    require(console_memory::interpret(invalid,"utf8").find("invalid UTF-8")!=fast_io::containers::npos,"overlong UTF8 rejected");
    fast_io::string output{};auto editor{std::make_unique<console_editing::editor>()};editor->begin();
    console_tui::screen screen{&output,+[](void* p,fast_io::string_view s) noexcept { static_cast<fast_io::string*>(p)->append(s); },true};
    require(screen.command("layout tree h50(src,v50(memory,io))"),"nested command");
    screen.set_panel(console_tui::layout::src,"=> 5 int a = 0;\n   6 中文 hello\n");
    screen.memory_snapshot(data,32u);screen.render(*editor,{120u,45u});
    require(output.find("AB......")!=fast_io::containers::npos,"memory ASCII in renderer");
    output.clear();screen.render(*editor,{200u,55u});
    require(output.find("\x1b[2J")!=fast_io::containers::npos,"resize clears old extent");
    output.clear();screen.render(*editor,{200u,55u});
    require(output.empty(),"identical frame emits no bytes");
    require(screen.command("memory format u8") && screen.command("memory select 35 1"),"typed memory selection");
    screen.render(*editor,{200u,55u});
    require(output.find("255")!=fast_io::containers::npos,"selection interprets exact requested byte");
    screen.observe_stop(7u);output.clear();require(screen.command("memory select 35 1"),"stale select parsed");
    screen.render(*editor,{200u,55u});
    require(output.find("requires bytes in the current")!=fast_io::containers::npos,"stop invalidates cached bytes");
    for(auto dims:{console_tui::dimensions{29u,12u},console_tui::dimensions{80u,24u},console_tui::dimensions{200u,55u}}) { screen.render(*editor,dims); }
    require(editor->feed(-5)==console_editing::action::idle,"idle tick preserves input state");
    output.clear();static_cast<void>(editor->feed('x'));screen.render(*editor,{200u,55u});
    require(output.size()<256u && output.find("\x1b[2J")==fast_io::containers::npos && output.find("[src")==fast_io::containers::npos,"typing only repaints changed input cells");
    require(screen.command("layout horizontal src memory"),"flat columns");
    screen.memory_snapshot(data,32u);output.clear();screen.render(*editor,{52u,24u});
    require(output.find("0x0000000000000020: 41  |A|")!=fast_io::containers::npos,"minimum memory width retains address and ASCII");
    require(screen.command("focus memory"),"focus narrow memory");
    output.clear();screen.render(*editor,{40u,24u});
    require(output.find("0x0000000000000020: 41  |A|")!=fast_io::containers::npos,"narrow terminal keeps focused memory readable");
    require(screen.command("layout src"),"source relocation pane");
    screen.set_panel(console_tui::layout::src,"=> 5 int a = 0;\n   6 a = 1;\nN> 7 a = 2;\n");
    screen.render(*editor,{120u,45u});
    screen.mouse({0u,3u,4u,false},{120u,45u});
    screen.mouse({32u,3u,5u,false},{120u,45u});
    screen.mouse({0u,3u,5u,true},{120u,45u});
    require(screen.take_query()=="jump src 6","only exact marker drag emits typed source relocation");
    screen.mouse({0u,3u,6u,false},{120u,45u});screen.mouse({0u,3u,5u,true},{120u,45u});
    require(screen.take_query().empty(),"N preview cannot relocate");
    screen.mouse({0u,3u,4u,false},{120u,45u});screen.observe_stop(8u);
    screen.mouse({0u,3u,5u,true},{120u,45u});require(screen.take_query().empty(),"new stop cancels marker gesture");
    require(screen.command("layout asm"),"native relocation pane");
    screen.set_panel(console_tui::layout::assembly,"JIT module=0 function=1\n=> 0x1000  mov w0, #1\n   0x1004  add w0, w0, #1\n");
    screen.render(*editor,{120u,45u});screen.mouse({0u,3u,5u,false},{120u,45u});screen.mouse({0u,3u,6u,true},{120u,45u});
    require(screen.take_query()=="jump asm *0x1004","native drag produces absolute owned target");
    screen.prepare_source("source.c",1u,22u);screen.source_bounds(3u);
    require(screen.command("scroll src down 4096") && screen.source_first_line()==3u,"source clamps to last line");
    static_cast<void>(screen.take_content_change());static_cast<void>(screen.command("scroll src down 4096"));
    require(screen.source_first_line()==3u && !screen.take_content_change(),"source bottom wheel is inert");
    static_cast<void>(screen.command("scroll src up 4096"));require(screen.source_first_line()==1u,"source clamps first line");
    screen.assembly_position(0);screen.assembly_bound(true,-7);screen.assembly_bound(false,80);
    static_cast<void>(screen.command("scroll asm down 4096"));require(screen.assembly_first()==80,"ASM clamps last instruction");
    static_cast<void>(screen.command("scroll asm up 4096"));require(screen.assembly_first()==-7,"ASM clamps first instruction");
    console_layout::tree equal{};unsigned panes[]{0u,4u,1u};equal.chain(panes,true);
    equal.visit(equal.root,{0u,1u,3u,144u,30u},[&](auto b) { require(b.width==48u,"three exact equal columns"); });
    require(equal.resize(0u,51u,{0u,1u,3u,144u,30u}),"exact cell resize");
    auto saved{equal.serialize()};console_layout::tree restored{};require(restored.parse(saved.subview(0u)),"fraction layout config roundtrip");
    restored.visit(restored.root,{0u,1u,3u,144u,30u},[&](auto b) { if(b.pane==0u) { require(b.width==51u,"exact width preserved by config"); } });
    for(unsigned n{};n<50u;++n) { screen.output("old history row\n"); }
    screen.output("LAST_VISIBLE_REPLY\n");screen.guest_output("old stdout\nLAST_VISIBLE_IO\n");
    output.clear();screen.render(*editor,{120u,45u});
    require(output.find("LAST_VISIBLE_REPLY")!=fast_io::containers::npos,"newest reply occupies last command body row");
    require(output.find("LAST_VISIBLE_IO")!=fast_io::containers::npos,"newest IO row remains visible");
    screen.disable();std::puts("PASS nested layout, memory encodings, stale selection and resize");
}
