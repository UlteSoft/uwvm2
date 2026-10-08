// Actual production formatter bytes, supplied owned DATA; no live VM authority.
#include <uwvm2/uwvm/debugger/console.h>
using namespace uwvm2::uwvm::debugger;
static void packet(::fast_io::string_view name, controller_reply const& reply, console_command const& command)
{
    auto const text{details::format_reply(reply,command)};
    ::fast_io::io::println("packet ",name," ",text.size());
    ::fast_io::io::print(::std::string_view{text});
}
int main()
{
    auto const command{parse_console_command("print-frame 1 41 2 value + 2")};
    controller_reply reply{};reply.source_stop_identifier=41u;reply.source_object_value_available=true;
    source_dwarf::object_node node{};node.name=::fast_io::concat_std("$expression");node.type_name=::fast_io::concat_std("int");
    node.kind=source_dwarf::type_kind::scalar;node.scalar_kind=source_dwarf::numeric_kind::signed_integer;
    node.scalar_bytes=4u;node.byte_size=4u;node.bits=9u;node.value_available=true;
    reply.source_object_type.push_back(node);packet("scalar",reply,command);
    reply.source_object_type[0u].kind=source_dwarf::type_kind::structure;reply.source_object_type[0u].value_available=false;reply.source_object_type[0u].byte_size=8u;
    reply.source_object_type[0u].name=::fast_io::concat_std("packet");reply.source_object_type[0u].type_name=::fast_io::concat_std("TupleProbe");
    node.parent=0u;node.depth=1u;node.name=::fast_io::concat_std("member\x1b\"");node.bits=42u;node.byte_offset=4u;
    reply.source_object_type.push_back(node);packet("object",reply,command);
    reply.source_object_type.resize(1u);auto& pointer{reply.source_object_type[0u]};
    pointer.kind=source_dwarf::type_kind::pointer;pointer.bits=0x100u;pointer.value_available=true;
    pointer.name=::fast_io::concat_std("p");pointer.type_name=::fast_io::concat_std("int*");pointer.byte_size=4u;
    packet("pointer-display",reply,command);
    reply.source_object_value_available=false;reply.source_object_type.clear();reply.source_locals_available=true;
    source_dwarf::numeric_variable value{};value.name=::fast_io::concat_std("constant");value.type_name=::fast_io::concat_std("int");
    value.kind=source_dwarf::numeric_kind::signed_integer;value.byte_count=4u;value.bits=42u;reply.source_locals.push_back(value);
    reply.source_constant.available=true;reply.source_constant.participant=1u;reply.source_constant.code_offset=4u;
    reply.source_constant.variable={0u,2u};reply.source_constant.scope={0u,3u};reply.source_constant.type={0u,1u};
    packet("constant",reply,command);
    reply.source_constant.available=false;reply.source_locals.clear();reply.source_locals_available=false;
    packet("unavailable-display",reply,command);
}
