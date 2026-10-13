// Finite owned type metadata only. No guest frame, stop token, source lease or
// memory-read/execution authority is created by these regression fixtures.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{
    if(!value) { ::fast_io::io::perrln(::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static type_record object(::std::string_view name)
{
    type_record t{}; t.name=::fast_io::concat_std(name); t.kind=type_kind::structure;
    t.byte_size=4u;t.size_known=true;t.language=0x21u;return t;
}
static type_record pointer(::std::size_t target)
{
    auto t{object({})};t.kind=type_kind::pointer;t.byte_count=4u;t.referenced_type=target;return t;
}
int main()
{
    ::std::vector<type_record> t{object("Node")};
    auto const_node{t[0u]};const_node.display_qualifiers=1u;t.push_back(const_node);
    t.push_back(pointer(1u)); // const Node *
    auto const_pointer{t[2u]};const_pointer.display_qualifiers=1u;t.push_back(const_pointer);
    t.push_back(pointer(3u)); // const Node * const *
    auto reference{pointer(1u)};reference.reference_type=true;t.push_back(reference);
    reference.rvalue_reference_type=true;t.push_back(reference);
    auto alias{pointer(1u)};alias.name=::fast_io::concat_std("Alias");alias.named_type_alias=true;alias.display_qualifiers=1u;t.push_back(alias);
    t.push_back(pointer(7u));
    auto volatile_node{const_node};volatile_node.display_qualifiers=3u;t.push_back(volatile_node);t.push_back(pointer(9u));
    ::std::vector<object_node> out{};
    for(auto const& [index,name] : ::std::array<::std::pair<::std::size_t,::std::string_view>,8u>{{
        {2u,"const Node *"},{3u,"const Node * const"},{4u,"const Node * const *"},{5u,"const Node &"},
        {6u,"const Node &&"},{7u,"const Alias"},{8u,"const Alias *"},{10u,"const volatile Node *"}}})
    {
        check(query_type_layout(t,index,out)==inline_query_error::none && out.size()==1u && out[0u].type_name==name,
            "C++ pointer/reference/typedef cv position differs from source spelling");
        check(!out[0u].value_available,"type name lookup must remain cold metadata without memory authority");
    }
    auto unknown{pointer(1u)};unknown.language=0u;t.push_back(unknown);
    check(query_type_layout(t,11u,out)==inline_query_error::none && out[0u].type_name.empty(),"unknown language must not receive inferred C pointer syntax");
    auto tinygo{pointer(1u)};tinygo.language=0x0cu;tinygo.tinygo_producer=true;t.push_back(tinygo);
    check(query_type_layout(t,12u,out)==inline_query_error::none && out[0u].type_name.empty(),"TinyGo C99 CU must retain language-specific presentation");
    auto cycle{pointer(13u)};t.push_back(cycle);
    check(query_type_layout(t,13u,out)==inline_query_error::none && out[0u].type_name.empty(),"anonymous pointer cycle remains opaque and bounded");
    auto atomic{pointer(1u)};atomic.display_qualifiers=8u;t.push_back(atomic);
    check(query_type_layout(t,14u,out)==inline_query_error::none && out[0u].type_name.empty(),"unsupported atomic declarator cannot be labelled as an ordinary pointer");
    object_query_limits cap{};cap.max_depth=1u;
    check(query_type_layout(t,4u,out,cap)==inline_query_error::limit_exceeded && out.empty(),"name traversal obeys depth limit and clears partial output");
    cap={};cap.max_result_string_bytes=5u;
    check(query_type_layout(t,2u,out,cap)==inline_query_error::limit_exceeded && out.empty(),"constructed type name obeys result string budget before appending");
    t[14u].display_qualifiers=16u;
    check(query_type_layout(t,14u,out)==inline_query_error::malformed && out.empty(),"unknown qualifier bits must not be ignored");
    ::fast_io::io::println("PASS bounded C-family type names, qualifier positions and metadata-only limits");
}
