#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
using namespace uwvm2::uwvm::debugger;
static ::std::size_t checks{};
static void require(bool good,char const* message)
{
    ++checks;
    if(!good) { ::fast_io::io::perrln("Go array failure: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
int main()
{
    for(::std::uint8_t width : {4u,8u})
    {
        using K=source_dwarf::type_kind;
        ::std::vector<source_dwarf::type_record> types(8u);
        for(auto& t : types) { t.language=0x0cu;t.tinygo_producer=true;t.size_known=true; }
        types[0].kind=K::scalar;types[0].encoding=5u;types[0].byte_count=4u;types[0].byte_size=4u;
        types[1].kind=K::array;types[1].name=::fast_io::concat_std("[4]int32");types[1].byte_size=16u;types[1].referenced_type=0u;
        source_dwarf::dimension_record bound{};bound.lower_bound_known=true;bound.count_known=true;bound.count=4u;types[1].dimensions={bound};
        types[2].kind=K::pointer;types[2].byte_size=types[2].byte_count=width;types[2].referenced_type=1u;
        types[3].kind=K::structure;types[3].byte_size=16u+width;
        source_dwarf::member_record member{};member.name=::fast_io::concat_std("Array");member.type=1u;member.offset_known=true;
        auto pointer=member;pointer.name=::fast_io::concat_std("Pointer");pointer.type=2u;pointer.byte_offset=16u;types[3].members={member,pointer};
        types[4]=types[2];types[4].referenced_type=3u;
        types[5]=types[1];types[5].name=::fast_io::concat_std("[2][4]int32");types[5].referenced_type=1u;types[5].byte_size=32u;types[5].dimensions[0].count=2u;
        types[6].kind=K::structure;types[6].name=::fast_io::concat_std("struct{}");
        types[7]=types[1];types[7].name=::fast_io::concat_std("[6]struct{}");types[7].referenced_type=6u;types[7].byte_size=0u;types[7].dimensions[0].count=6u;
        ::std::size_t reads{};
        auto reader=[&](auto,auto,auto&){++reads;return false;};
        auto query=[&](::std::size_t root,char const* expression,bool metadata=false)
        {
            source_dwarf::source_expression parsed{};require(source_dwarf::parse_source_expression(expression,parsed)==source_dwarf::object_selector_error::none,"array builtin grammar");
            ::std::vector<source_dwarf::object_node> out{};
            // No array or pointer value has been supplied. A fixed-length
            // builtin uses only independently authenticated owned metadata.
            auto status=metadata ? source_language_expression::type(types,root,parsed.steps,width,out) :
                source_language_expression::value(types,root,parsed.steps,{}, {},width,reader,out);
            require(reads==0u,"constant array expression evaluated its operand");return ::std::pair{status,out};
        };
        auto expect=[&](::std::size_t root,char const* text,::std::uint64_t expected)
        {
            auto [status,out]=query(root,text);require(status==source_dwarf::inline_query_error::none && out.size()==1u &&
                out[0].value_available && out[0].bits==expected && out[0].scalar_bytes==width &&
                out[0].scalar_kind==source_dwarf::numeric_kind::signed_integer,"array length/capacity result");
            auto [typed,nodes]=query(root,text,true);require(typed==source_dwarf::inline_query_error::none && nodes.size()==1u &&
                !nodes[0].value_available && nodes[0].byte_size==width,"array builtin type-only result");
        };
        expect(1u,"len(a)",4u);expect(1u,"cap(a)",4u);expect(2u,"len(p)",4u);expect(2u,"cap(p)",4u);
        expect(2u,"len(*p)",4u);expect(2u,"cap(*p)",4u);expect(4u,"len(box.Array)",4u);expect(4u,"cap(box.Pointer)",4u);
        expect(5u,"len(matrix)",2u);expect(5u,"cap(matrix)",2u);expect(5u,"len(matrix[0])",4u);expect(5u,"cap(matrix[1])",4u);
        require(query(5u,"len(matrix[2])").first==source_dwarf::inline_query_error::unavailable,"out-of-range constant index accepted");
        expect(7u,"len(zeroSized)",6u);expect(7u,"cap(zeroSized)",6u);
        types[1].dimensions[0].count=0u;types[1].byte_size=0u;
        expect(1u,"len(a)",0u);expect(1u,"cap(a)",0u);expect(2u,"len(p)",0u);expect(2u,"cap(*p)",0u);
        types[1].dimensions[0].count=4u;types[1].byte_size=16u;
        auto original=types[1];
        for(unsigned fault{};fault!=12u;++fault)
        {
            types[1]=original;
            switch(fault)
            {
                case 0:types[1].dimensions[0].count_known=false;break;
                case 1:types[1].dimensions[0].lower_bound_known=false;break;
                case 2:types[1].dimensions[0].lower_bound=1;break;
                case 3:types[1].byte_size=15u;break;
                case 4:types[1].size_known=false;break;
                case 5:types[1].contiguous_array=false;break;
                case 6:types[1].row_major_array=false;break;
                case 7:types[1].referenced_type=99u;break;
                case 8:types[1].dimensions.clear();break;
                case 9:types[1].dimensions.push_back(bound);break;
                case 10:types[1].language=0x1cu;types[1].tinygo_producer=false;break;
                case 11:types[1].tinygo_producer=false;break;
            }
            require(query(1u,"len(a)").first!=source_dwarf::inline_query_error::none,"invalid array metadata accepted");
            require(query(2u,"cap(p)",true).first!=source_dwarf::inline_query_error::none,"invalid pointer-array metadata type accepted");
        }
        types[1]=original;types[1].language=0x16u;expect(1u,"len(a)",4u);types[1]=original;
        for(unsigned fault{};fault!=5u;++fault)
        {
            auto saved=types[2];
            if(fault==0u) { types[2].reference_type=true; }
            if(fault==1u) { types[2].rvalue_reference_type=true; }
            if(fault==2u) { types[2].address_class_known=true;types[2].address_class=1u; }
            if(fault==3u) { types[2].byte_count=width==4u ? 8u : 4u; }
            if(fault==4u) { types[2].language=0x1cu;types[2].tinygo_producer=false; }
            require(query(2u,"len(p)").first!=source_dwarf::inline_query_error::none,"invalid pointer array carrier accepted");types[2]=saved;
        }
        auto maximum=width==4u ? 0x7fffffffull : 0x7fffffffffffffffull;
        types[7].dimensions[0].count=maximum;expect(7u,"len(zeroSized)",maximum);
        types[7].dimensions[0].count=maximum+1u;require(query(7u,"cap(zeroSized)").first==source_dwarf::inline_query_error::unavailable,"Go int length overflow accepted");
        types[1]=original;types[1].dimensions[0].count=maximum;types[0].byte_size=~::std::uint64_t{};
        require(query(2u,"len(p)").first==source_dwarf::inline_query_error::unavailable,"array byte extent multiplication overflow accepted");types[0].byte_size=4u;types[1]=original;
        for(auto text : {"len(a)+cap(a)","len(a)-1","sizeof(len(a))"})
        {
            source_scalar_expression::program code{};source_scalar_expression::integer result{};
            require(source_scalar_expression::parse(text,code)==source_scalar_expression::error::none,"scalar array parse");
            auto resolve=[&](auto const& expression,bool size,auto& out)
            {
                ::std::vector<source_dwarf::object_node> nodes{};
                auto status=size ? source_language_expression::type(types,1u,expression.steps,width,nodes) :
                    source_language_expression::value(types,1u,expression.steps,{}, {},width,reader,nodes);
                if(status!=source_dwarf::inline_query_error::none || nodes.size()!=1u) { return false; }
                out=source_scalar_expression::integer{size ? nodes[0].byte_size : nodes[0].bits,unsigned(width)*8u,size};return size || nodes[0].value_available;
            };
            require(source_scalar_expression::evaluate(code,resolve,result,unsigned(width)*8u)==source_scalar_expression::error::none &&
                result.bits==(::std::string_view{text}.starts_with("sizeof") ? width : ::std::string_view{text}=="len(a)-1" ? 3u : 8u),"scalar array value");
            require(reads==0u,"scalar array builtin requested memory");
        }
    }
    ::fast_io::io::println("debug_source_go_arrays: PASS ",checks," assertions");
}
