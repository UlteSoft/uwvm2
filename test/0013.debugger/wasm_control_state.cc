// Detached view/console-format DATA tests only. They mint no paused owner,
// capture, GC lease, publication, native memory or checkpoint restore authority.
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <uwvm2/uwvm/debugger/command.h>
namespace ws=::uwvm2::uwvm::debugger::wasm_state;
namespace dbg=::uwvm2::uwvm::debugger;
static void require(bool good,::fast_io::string_view reason)
{ if(!good) { ::fast_io::io::perrln("control-state DATA: ",reason); ::fast_io::fast_terminate(); } }
static ws::view view(ws::selection selected,::std::uint64_t total,::std::uint64_t first=0u,::std::uint64_t count=64u)
{
    ws::view out{}; out.result=ws::status::available; out.requested.selected=selected; out.requested.participant=1u;
    out.requested.first=first; out.requested.count=count; out.first=first; out.total_values=total; out.runtime_epoch=7u;
    return out;
}
static bool contains(::std::string const& text,::std::string_view item)
{ return ::std::string_view{text}.find(item)!=::std::string_view::npos; }
int main()
{
    auto control{view(ws::selection::controls,3u)};
    control.controls={{0u,0u,50u,0u,0u,0u,0u,1u,ws::control_kind::function},
        {1u,2u,49u,0u,0u,0u,1u,1u,ws::control_kind::block},
        {2u,7u,40u,1u,0u,1u,1u,1u,ws::control_kind::if_else}};
    require(ws::valid(control)&&contains(ws::format(control),"control 2 kind=if-else entry=7 end=40 height=1 saved-first=0 saved-count=1 params=1 results=1"),"exact current lexical control rows");
    auto bad{control}; bad.controls[2u].index=0u; require(!ws::valid(bad),"unrebased original control ordinal");
    bad=control; bad.controls[2u].saved_parameter_count=2u; require(!ws::valid(bad),"saved count cannot exceed exact declaration tuple");
    bad=control; bad.controls[1u].saved_parameter_count=1u; require(!ws::valid(bad),"non-if control has no hidden saved-if suffix");
    bad=control; bad.controls[2u].end_offset=6u; require(!ws::valid(bad),"unclosed or reversed source extent rejected");
    bad=control; bad.rows.push_back({}); require(!ws::valid(bad),"lexical metadata never fabricates an operand VALUE");
    bad=control; bad.requested.member_count=1u;bad.requested.count=1u;require(!ws::valid(bad.requested),"lexical metadata cannot be rooted object input");
    auto handlers{view(ws::selection::handlers,4u)};
    handlers.handlers={{0u,8u,1u,49u,2u,false,false},{1u,9u,1u,49u,1u,false,true},
        {2u,0u,1u,49u,0u,true,false},{3u,0u,1u,49u,0u,true,true}};
    require(ws::valid(handlers)&&contains(ws::format(handlers),"handler 3 catch=catch-all-ref tag-index=0 target-control=1 target-offset=49 params=0"),"four clause forms remain source-order lexical metadata");
    bad=handlers;bad.handlers[2u].tag_index=8u;require(!ws::valid(bad),"catch-all has no fabricated tag identity");
    bad=handlers;bad.handlers[2u].parameter_count=1u;require(!ws::valid(bad),"catch-all has no fabricated declared payload");
    auto params{view(ws::selection::control_parameters,1024u,128u,2u)};params.requested.index=2u;params.rows_truncated=true;
    params.declarations={{128u,{ws::value_kind::reference,4294967295ll,false,true,7u}},{129u,{ws::value_kind::v128,0,false,true}}};
    require(ws::valid(params)&&contains(ws::format(params),"control 2 parameter 128 (ref type-index=4294967295 module=7)")&&contains(ws::format(params),"Wasm layout page next=130"),"large declared tuples paginate original indices and exact Core3 types");
    bad=params;bad.declarations[0u].type.heap=4294967296ll;require(!ws::valid(bad),"defined heap declaration width");
    bad=params;bad.declarations[0u].type.known=false;require(!ws::valid(bad),"opaque carrier cannot supply a declaration type");
    bad=params;bad.declarations[0u].type={ws::value_kind::i32,1,false,true};require(!ws::valid(bad),"numeric declarations have no reference metadata");
    bad=params;bad.requested.first=1025u;bad.first=1025u;require(!ws::valid(bad),"first bound before total-first subtraction");
    auto end{view(ws::selection::control_results,1024u,1024u)};end.requested.index=2u;require(ws::valid(end),"empty exact final tuple page");
    auto saved{view(ws::selection::saved_parameters,1u)}; ws::value number{};number.available=true;number.type.known=true;saved.rows={{0u,number}};
    require(ws::valid(saved)&&contains(ws::format(saved),"saved-parameter 0 i32 = 0"),"saved parameters are separately typed actual values");
    auto cmd{dbg::parse_console_command("control-params 1 2 3 128 64")};
    require(cmd.kind==dbg::console_command_kind::wasm_state&&cmd.wasm_state_request.selected==ws::selection::control_parameters&&
        cmd.wasm_state_request.frame==2u&&cmd.wasm_state_request.index==3u&&cmd.wasm_state_request.first==128u,"console original frame/control/tuple indices");
    require(dbg::parse_console_command("handler-params 1 2").kind==dbg::console_command_kind::invalid,"explicit frame+clause required");
    require(dbg::parse_console_command("controls 1 0 0 65").kind==dbg::console_command_kind::invalid,"console metadata page64 cap");
    cmd=dbg::parse_console_command("members saved 1 0 2 0 0 128 64 7");
    require(cmd.kind==dbg::console_command_kind::wasm_state&&cmd.wasm_state_request.selected==ws::selection::saved_parameters&&cmd.wasm_state_request.path[0u]==7u,"saved reference expansion stays original-root/path based");
    auto longest{view(ws::selection::controls,64u)};auto const top{(::std::numeric_limits<::std::uint64_t>::max)()};
    for(::std::uint64_t i{};i!=64u;++i){longest.controls.push_back({i,top,top,top,0u,top,top,top,ws::control_kind::if_then});}
    auto text{ws::format(longest)};require(ws::valid(longest)&&text.size()<ws::maximum_reply_bytes-256u&&contains(text,"control 63 kind=if-then"),"all64 maximally long metadata rows fit complete bounded format");
    require(!longest.snapshot_or_restore_authority(),"view metadata does not carry restore authority");
    ::fast_io::io::println("control-state DATA model PASS; runtime capture=false");
}
