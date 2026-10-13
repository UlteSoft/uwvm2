#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
using namespace uwvm2::uwvm::debugger;
static void check(bool v,char const* text)
{ if(!v) { ::fast_io::io::perrln(::fast_io::err(),::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); } }
int main()
{
    for(::std::uint8_t width : {4u,8u}) for(bool rvalue : {false,true})
    {
        ::std::vector<source_dwarf::type_record> t(3u);
        t[0].kind=source_dwarf::type_kind::scalar;t[0].size_known=true;t[0].byte_size=4u;t[0].byte_count=4u;t[0].encoding=5u;
        t[1].kind=source_dwarf::type_kind::structure;t[1].size_known=true;t[1].byte_size=4u;
        source_dwarf::member_record m{};m.name="value";m.type=0u;m.offset_known=true;t[1].members.push_back(m);
        auto& ref{t[2]};ref.kind=source_dwarf::type_kind::pointer;ref.language=0x04u;ref.size_known=true;
        ref.byte_size=width;ref.byte_count=width;ref.reference_type=true;ref.rvalue_reference_type=rvalue;ref.referenced_type=1u;
        ::std::vector<::std::byte> carrier(width),unknown(width),full(width,::std::byte{0xffu});carrier[0]=::std::byte{32u};
        source_dwarf::source_expression e{};check(source_dwarf::parse_source_expression("reference.value",e)==source_dwarf::object_selector_error::none,"parse");
        ::std::vector<source_dwarf::object_node> out{};::std::size_t reads{};
        auto const read{[&](::std::uint64_t offset,::std::size_t bytes,::std::vector<::std::byte>& copied)
        { ++reads;check(offset==32u && bytes==4u,"bounded fixture policy offset and extent");copied={::std::byte{7u},::std::byte{},::std::byte{},::std::byte{}};return true; }};
        check(source_language_expression::type(t,2u,e.steps,width,out)==source_dwarf::inline_query_error::none && out.size()==1u && out[0].type==0u && reads==0u,"reference member type needs no read");
        check(source_language_expression::value(t,2u,e.steps,carrier,full,width,read,out)==source_dwarf::inline_query_error::none && reads==1u && out.size()==1u && out[0].bits==7u,"lvalue/rvalue reference member finite DATA");
        reads=0u;source_dwarf::copied_guest_pointer_plan plan{};
        check(source_dwarf::plan_copied_guest_pointer(t,2u,{},carrier,plan)==source_dwarf::inline_query_error::unavailable,"references remain invalid for explicit pointer plan");
        check(source_language_expression::value(t,2u,e.steps,carrier,unknown,width,read,out)==source_dwarf::inline_query_error::unavailable && out.empty() && reads==0u,"unknown reference carrier bits reject");
        check(source_language_expression::value(t,2u,e.steps,carrier,{},width,read,out,{},unknown)==source_dwarf::inline_query_error::unavailable && out.empty() && reads==0u,"floating-only carrier rejects");
        full[0]=::std::byte{0xfeu};
        check(source_language_expression::value(t,2u,e.steps,carrier,full,width,read,out)==source_dwarf::inline_query_error::unavailable && out.empty() && reads==0u,"partial carrier rejects");
        check(source_language_expression::value(t,2u,e.steps,unknown,{},width,read,out)==source_dwarf::inline_query_error::unavailable && out.empty() && reads==0u,"null rejects");
        check(source_language_expression::value(t,2u,e.steps,carrier,{},width,read,out,{0u})==source_dwarf::inline_query_error::limit_exceeded && out.empty() && reads==0u,"read budget before callback");
        ref.address_class_known=true;ref.address_class=1u;
        check(source_language_expression::type(t,2u,e.steps,width,out)==source_dwarf::inline_query_error::unavailable && out.empty(),"address class rejects metadata transition");
        check(source_language_expression::value(t,2u,e.steps,carrier,{},width,read,out)==source_dwarf::inline_query_error::unavailable && out.empty() && reads==0u,"address class rejects value");
        ref.address_class=0u;ref.language=0x16u;
        check(source_language_expression::value(t,2u,e.steps,carrier,{},width,read,out)==source_dwarf::inline_query_error::unavailable && out.empty() && reads==0u,"Go reference metadata cannot use C++ reference profile");
        ref.language=0x04u;ref.tinygo_producer=true;
        check(source_dwarf::plan_copied_guest_cpp_reference(t,2u,{},carrier,plan)==source_dwarf::inline_query_error::unavailable,"TinyGo cannot use reference profile");
        ref.tinygo_producer=false;ref.reference_type=false;ref.rvalue_reference_type=false;
        check(source_dwarf::plan_copied_guest_cpp_reference(t,2u,{},carrier,plan)==source_dwarf::inline_query_error::unavailable,"ordinary pointer cannot forge reference profile");
    }
    ::fast_io::io::println(::fast_io::out(),"PASS finite C++ reference metadata/owned-byte profile only; runtime authority separately qualified");
}
