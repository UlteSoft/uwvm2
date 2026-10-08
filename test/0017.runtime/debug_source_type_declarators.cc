// Cold declarator metadata only: no stopped frame, pointer read or call target.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_dwarf_selectors.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool v,char const* m)
{ if(!v) { ::fast_io::io::perrln(::fast_io::mnp::os_c_str(m));::fast_io::fast_terminate(); } }
static type_record named(char const* name,type_kind kind=type_kind::scalar)
{
    type_record t{};t.name=::fast_io::concat_std(::fast_io::mnp::os_c_str(name));
    t.kind=kind;t.language=0x21u;t.size_known=true;t.byte_size=4u;t.byte_count=kind==type_kind::scalar?4u:0u;return t;
}
static type_record anonymous(type_kind kind,::std::size_t target)
{ type_record t{};t.kind=kind;t.language=0x21u;t.referenced_type=target;return t; }
int main()
{
    ::std::vector<type_record> t{named("int"),named("Widget",type_kind::class_type)};
    auto array{anonymous(type_kind::array,0u)};array.dimensions={{0,2u,true,true},{0,3u,true,true}};
    t.push_back(array); // 2: int [2][3]
    t.push_back(anonymous(type_kind::pointer,2u)); // 3: int (*)[2][3]
    auto ptrs{anonymous(type_kind::array,4u)};ptrs.referenced_type=5u;ptrs.dimensions={{0,2u,true,true}};
    t.push_back(ptrs);t.push_back(anonymous(type_kind::pointer,0u)); // 4: int *[2]
    auto fn{anonymous(type_kind::subroutine,0u)};fn.signature_complete=true;fn.parameter_types={3u,5u};fn.variadic=true;
    t.push_back(fn);t.push_back(anonymous(type_kind::pointer,6u)); // 7: function pointer
    auto data{anonymous(type_kind::member_pointer,0u)};data.containing_type=1u;t.push_back(data); // 8
    auto method{fn};method.parameter_types={0u};method.variadic=false;method.method_qualifiers=1u;method.method_lvalue_reference=true;
    t.push_back(method);auto member{anonymous(type_kind::member_pointer,9u)};member.containing_type=1u;t.push_back(member); // 10
    auto qualified{array};qualified.display_qualifiers=1u;t.push_back(qualified); // 11
    t.push_back(anonymous(type_kind::pointer,no_record)); // 12: void *
    for(auto const& [index,text] : ::std::array<::std::pair<::std::size_t,::std::string_view>,8u>{{
        {2u,"int [2][3]"},{3u,"int (*)[2][3]"},{4u,"int *[2]"},
        {7u,"int (*)(int (*)[2][3], int *, ...)"},{8u,"int Widget::*"},
        {10u,"int (Widget::*)(int) const &"},{11u,"const int [2][3]"},{12u,"void *"}}})
    {
        ::fast_io::string out{};
        check(type_declarators::format(t,index,out)==inline_query_error::none &&
              ::std::string_view{out.data(),out.size()}==text,"declarator precedence/qualifier position");
    }
    ::std::vector<object_node> nodes{};
    check(query_type_layout(t,10u,nodes)==inline_query_error::none && nodes.size()==1u &&
          nodes[0].type_name=="int (Widget::*)(int) const &" && !nodes[0].value_available,
          "member function declaration must not produce a callable or memory authority");
    check(query_selected_object_type(t,8u,{},nodes)==inline_query_error::none && nodes.size()==1u &&
          nodes[0].type_name=="int Widget::*" && nodes[0].reason==object_unavailable_reason::unknown_size && !nodes[0].value_available,
          "root member-pointer declaration remains available without an invented ABI extent");
    check(query_selected_object_value(t,8u,{}, {},nodes)==inline_query_error::unavailable && nodes.empty(),
          "unknown member-pointer carrier cannot acquire object-value access");
    t[6u].signature_complete=false;
    ::fast_io::string out{};check(type_declarators::format(t,7u,out)==inline_query_error::none && out.empty(),"incomplete signature remains opaque");
    t[6u].signature_complete=true;t[6u].parameter_types={7u};
    check(type_declarators::format(t,7u,out)==inline_query_error::none && out.empty(),"recursive anonymous signature is bounded");
    t[6u].parameter_types={t.size()};
    check(query_type_layout(t,7u,nodes)==inline_query_error::malformed && nodes.empty(),"invalid parameter reference rejects without partial publication");
    t[6u].parameter_types={0u};
    check(type_declarators::format(t,7u,out,{1u,32u,1024u})==inline_query_error::limit_exceeded && out.empty(),"declarator depth limit");
    check(type_declarators::format(t,7u,out,{32u,1u,1024u})==inline_query_error::limit_exceeded && out.empty(),"declarator edge limit");
    check(type_declarators::format(t,7u,out,{32u,32u,3u})==inline_query_error::limit_exceeded && out.empty(),"declarator string limit");
    // The actual Clang producer uses an artificial first parameter for this
    // in a subroutine type. Only matching owned final class DIEs permit hiding it.
    auto owner{named("Widget",type_kind::class_type)};owner.declaration_identity={1u,42u};
    auto const_owner{owner};const_owner.display_qualifiers=1u;
    ::std::vector<type_record> artificial{named("int"),owner,const_owner,anonymous(type_kind::pointer,2u)};
    auto signature{anonymous(type_kind::subroutine,0u)};signature.signature_complete=true;
    signature.first_parameter_artificial=true;signature.parameter_types={3u,0u};signature.method_lvalue_reference=true;
    artificial.push_back(signature);
    auto member_type{anonymous(type_kind::member_pointer,4u)};member_type.containing_type=1u;artificial.push_back(member_type);
    check(type_declarators::format(artificial,5u,out)==inline_query_error::none &&
          ::std::string_view{out.data(),out.size()}=="int (Widget::*)(int) const &",
          "Clang artificial-this preserves member qualifiers without a callable value");
    artificial.push_back(anonymous(type_kind::pointer,4u));
    check(type_declarators::format(artificial,6u,out)==inline_query_error::none && out.empty(),
          "ordinary function hidden ABI argument cannot be inferred as this");
    artificial[2u].declaration_identity={1u,43u};
    check(type_declarators::format(artificial,5u,out)==inline_query_error::none && out.empty(),
          "same class spelling with another owned DIE cannot remove a parameter");
    artificial[2u].declaration_identity={1u,42u};
    artificial[4u].calling_convention_known=true;artificial[4u].calling_convention=0xffu;
    check(type_declarators::format(artificial,5u,out)==inline_query_error::none && out.empty(),
          "unknown calling convention stays opaque");
    ::fast_io::io::println("PASS C++ array/function/member-pointer declarators and bounded opaque metadata");
}
