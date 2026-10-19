// Standard-array expression planning uses only the already copied object.
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace expression=::uwvm2::uwvm::debugger::source_language_expression;
static void check(bool good,char const* message)
{ if(!good) { ::std::fprintf(stderr,"array expression FAIL: %s\n",message); ::std::exit(1); } }
int main()
{
    for(unsigned width : {4u,8u}) for(bool libcxx : {false,true})
    {
        ::std::vector<dwarf::type_record> types(3u);
        auto& integer{types[0u]}; integer.name="int"; integer.kind=dwarf::type_kind::scalar;
        integer.language=0x21u; integer.encoding=5u; integer.byte_count=4u; integer.byte_size=4u; integer.size_known=true;
        auto& storage{types[1u]}; storage.kind=dwarf::type_kind::array; storage.referenced_type=0u;
        storage.byte_size=12u; storage.size_known=true;
        dwarf::dimension_record dimension{}; dimension.lower_bound_known=true; dimension.count_known=true; dimension.count=3u;
        storage.dimensions.push_back(dimension);
        auto& array{types[2u]}; array.name=libcxx ? "std::__1::array<int, 3>" : "std::array<int, 3>";
        array.language=0x21u; array.kind=dwarf::type_kind::structure; array.byte_size=12u; array.size_known=true; array.template_types={0u};
        array.template_values.push_back({1u,0u,3u,false,true});
        dwarf::member_record member{}; member.name=libcxx ? "__elems_" : "_M_elems";
        member.type=1u; member.offset_known=true; array.members.push_back(member);
        ::std::vector<::std::byte> bytes(12u),known(12u,::std::byte{255u});
        bytes[0u]=::std::byte{17u}; bytes[4u]=::std::byte{23u}; bytes[8u]=::std::byte{31u};
        ::std::size_t reads{};
        auto read{[&](::std::uint64_t,::std::size_t,::std::vector<::std::byte>&) { ++reads; return false; }};
        dwarf::source_expression selector{};
        check(dwarf::parse_source_expression("fixed[1]",selector)==dwarf::object_selector_error::none,"parse");
        ::std::vector<dwarf::object_node> value{};
        check(expression::type(types,2u,selector.steps,width,value)==dwarf::inline_query_error::none &&
              value.size()==1u && value[0u].type==0u && reads==0u,"ptype follows exact storage array metadata");
        check(expression::value(types,2u,selector.steps,bytes,known,width,read,value)==dwarf::inline_query_error::none &&
              value.size()==1u && value[0u].value_available && value[0u].bits==23u && reads==0u,"selected scalar uses local copied bytes");
        known[4u]=::std::byte{};
        check(expression::value(types,2u,selector.steps,bytes,known,width,read,value)==dwarf::inline_query_error::none &&
              !value[0u].value_available && reads==0u,"unknown selected bytes remain unknown");
        for(auto index : {-1,3})
        {
            selector.steps[0u].index=index;
            check(expression::value(types,2u,selector.steps,bytes,known,width,read,value)!=dwarf::inline_query_error::none &&
                  reads==0u,"invalid index never reads memory");
        }
    }
    ::std::puts("{\"passed\":true,\"scope\":\"std::array copied-object expression indexing\"}");
}
